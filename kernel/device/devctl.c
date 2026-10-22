#include <vex/atomic.h>
#include <vex/block_event.h>
#include <vex/block_interface.h>
#include <vex/capability.h>
#include <vex/char_event.h>
#include <vex/char_interface.h>
#include <vex/cred.h>
#include <vex/ctype.h>
#include <vex/dev_name.h>
#include <vex/devctl.h>
#include <vex/driver.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/fcntl.h>
#include <vex/file.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/mutex.h>
#include <vex/page.h>
#include <vex/slab.h>
#include <vex/sprintf.h>
#include <vex/stdlib.h>
#include <vex/string.h>
#include <vex/thread.h>

#include "device_internal.h"

#define BUILDER_START_SIZE PAGE_SIZE // Grows by doubling, so a small tree takes one or two steps
#define RING_LINES         128       // Events kept for readers that fall behind; past that, a reader gets a new snapshot

typedef struct ring_line_s ring_line_t;
typedef struct builder_s builder_t;
typedef struct snapshot_s snapshot_t;
typedef struct devctl_file_s devctl_file_t;

struct ring_line_s {
    char text[DEVCTL_LINE_MAX];
    size_t length; // With the newline
};

struct builder_s {
    char* text;
    size_t length;
    size_t capacity;
    bool failed; // Out of memory
};

// The tree as it was at seq, shared by the readers that open the file before the next event
struct snapshot_s {
    unsigned refcount;
    uint64_t seq; // Of the first event after the snapshot
    char* text;
    size_t length;
};

struct devctl_file_s {
    mutex_t lock;         // Threads that share the file share its position
    snapshot_t* snapshot; // Not read to the end yet; nullptr after that
    size_t snapshot_offset;
    uint64_t seq;                  // Of the next event to read
    char pending[DEVCTL_LINE_MAX]; // The rest of an event that a short read took only part of
    size_t pending_length;
    size_t pending_offset;
};

static_assert(DEVCTL_LINE_MAX <= FILE_IO_CHUNK, "A write system call can't take the longest command");
static_assert(sizeof(devctl_file_t) <= SLAB_MAX_SIZE);

static const char* const state_names[] = {
    [DEVICE_PRESENT] = "present",
    [DEVICE_PROBING] = "probing",
    [DEVICE_WAITING] = "waiting",
    [DEVICE_BOUND] = "bound",
    [DEVICE_DETACHING] = "detaching",
    [DEVICE_GONE] = "gone",
};

static mutex_t ring_lock = MUTEX_INIT;
static ring_line_t ring[RING_LINES];
static uint64_t ring_next_seq = 0;         // Under ring_lock
static event_t ring_appended = {};         // All zero is an initialized event, so the nodes added before the dev stage can post
static mutex_t snapshot_lock = MUTEX_INIT; // Over cached_snapshot, held while a new snapshot is built
static snapshot_t* cached_snapshot = nullptr;
static char_interface_t devctl_device;

static bool is_shell_safe(char chr)
{
    return isalnum(chr) || (chr && strchr("_.:/,+-", chr));
}

static void begin_line(devctl_line_t* restrict line, const char* restrict event)
{
    line->length = 0;
    line->text[0] = 0;

    size_t length = strlen(event);

    if(length < DEVCTL_LINE_MAX - 1) {
        memcpy(line->text, event, length + 1);
        line->length = length;
    }
}

static void post(const devctl_line_t* restrict line)
{
    mutex_acquire(&ring_lock);

    ring_line_t* slot = &ring[ring_next_seq % RING_LINES];
    memcpy(slot->text, line->text, line->length);
    slot->text[line->length] = '\n';
    slot->length = line->length + 1;
    ring_next_seq += 1;

    mutex_release(&ring_lock);

    event_trigger(&ring_appended, true);
}

static void describe_node(device_t* restrict device, devctl_line_t* restrict line)
{
    char text[DEVICE_PATH_MAX];
    device_t* parent = device->parent;

    begin_line(line, "add");
    devctl_put(line, "id", "%llu", (unsigned long long)(device->id));
    devctl_put(line, "parent", "%llu", (unsigned long long)(parent ? parent->id : 0));
    devctl_put(line, "name", "%s", device->name);

    device_get_path(device, text, sizeof(text));
    devctl_put(line, "path", "%s", text);
    devctl_put(line, "state", "%s", state_names[device->priv.state]);

    if(device->driver) {
        devctl_put(line, "driver", "%s", device->driver->name);
    }

    if(device->fwnode.ops && 0 == fwnode_get_id(&device->fwnode, text, sizeof(text))) {
        devctl_put(line, "fwid", "%s", text);
    }

    device_t* bus = device_bus_of(device);

    if(bus && bus->bus_ops->describe_child) {
        bus->bus_ops->describe_child(bus, device, line);
    }
}

static void describe_interface(const device_t* restrict device, const char* restrict kind, const char* restrict name,
    devctl_line_t* restrict line)
{
    begin_line(line, "interface");
    devctl_put(line, "id", "%llu", (unsigned long long)(device->id));
    devctl_put(line, "kind", "%s", kind);
    devctl_put(line, "name", "%s", name);
}

static void append_line(builder_t* restrict builder, const devctl_line_t* restrict line)
{
    size_t needed = builder->length + line->length + 1;

    if(builder->failed) {
        return;
    }

    if(needed > builder->capacity) {
        size_t capacity = builder->capacity ? builder->capacity * 2 : BUILDER_START_SIZE;

        while(capacity < needed) {
            capacity *= 2;
        }

        char* text = krealloc(builder->text, capacity);

        if(text == nullptr) {
            builder->failed = true;
            return;
        }

        builder->text = text;
        builder->capacity = capacity;
    }

    memcpy(&builder->text[builder->length], line->text, line->length);
    builder->text[builder->length + line->length] = '\n';
    builder->length = needed;
}

static void append_event(builder_t* restrict builder, const char* restrict event)
{
    devctl_line_t line;
    begin_line(&line, event);
    append_line(builder, &line);
}

// Parents come before their children, so that a reader knows the parent of each node it gets
static void append_children(builder_t* restrict builder, device_t* parent)
{
    for(device_t* child = device_next_child(parent, nullptr); child;) {
        if(child->priv.state != DEVICE_GONE) {
            devctl_line_t line;
            describe_node(child, &line);
            append_line(builder, &line);
            append_children(builder, child);
        }

        device_t* next = device_next_child(parent, child);
        device_decref(child);
        child = next;
    }
}

static void append_char_interface(const char_interface_t* restrict interface, void* restrict arg)
{
    if(interface->device) {
        devctl_line_t line;
        describe_interface(interface->device, "char", interface->name, &line);
        append_line(arg, &line);
    }
}

static void append_block_interface(const block_interface_t* restrict interface, void* restrict arg)
{
    if(interface->device) {
        devctl_line_t line;
        describe_interface(interface->device, "block", interface->name, &line);
        append_line(arg, &line);
    }
}

static void snapshot_decref(snapshot_t* restrict snapshot)
{
    if(1 == atomic_fetch_sub(&snapshot->refcount, 1, ATOMIC_ACQ_REL)) {
        kfree(snapshot->text);
        kfree(snapshot);
    }
}

// The number of the next event is taken before the walk. Thus a change during the walk is in the
// snapshot, in the events after it, or in both, and never lost. This is why the events are idempotent
static snapshot_t* build_snapshot(uint64_t seq)
{
    builder_t builder = {};

    append_event(&builder, "snapshot_begin");

    device_t* root = device_get_root();

    if(root) {
        append_children(&builder, root);
        device_decref(root);
    }

    char_event_lock();
    char_interface_for_each(&append_char_interface, &builder);
    char_event_unlock();

    block_event_lock();
    block_interface_for_each(&append_block_interface, &builder);
    block_event_unlock();

    append_event(&builder, "snapshot_end");

    snapshot_t* snapshot = nullptr;

    if(root && !builder.failed) {
        snapshot = kmalloc(sizeof(snapshot_t));
    }

    if(snapshot == nullptr) {
        kfree(builder.text);
        return nullptr;
    }

    snapshot->refcount = 1;
    snapshot->seq = seq;
    snapshot->text = builder.text;
    snapshot->length = builder.length;

    return snapshot;
}

// The cached snapshot is current while no event came after it. The cache keeps
// a stale one until the next open, as posting an event can't take snapshot_lock
static snapshot_t* get_snapshot(void)
{
    mutex_acquire(&snapshot_lock);

    mutex_acquire(&ring_lock);
    uint64_t seq = ring_next_seq;
    mutex_release(&ring_lock);

    snapshot_t* snapshot = cached_snapshot;

    if(snapshot == nullptr || snapshot->seq != seq) {
        snapshot = build_snapshot(seq);

        if(snapshot == nullptr) {
            mutex_release(&snapshot_lock);
            return nullptr;
        }

        if(cached_snapshot) {
            snapshot_decref(cached_snapshot);
        }

        cached_snapshot = snapshot;
    }

    atomic_fetch_add(&snapshot->refcount, 1, ATOMIC_RELAXED);

    mutex_release(&snapshot_lock);

    return snapshot;
}

static void drop_snapshot(devctl_file_t* restrict file)
{
    if(file->snapshot) {
        snapshot_decref(file->snapshot);
        file->snapshot = nullptr;
    }
}

static int take_snapshot(devctl_file_t* restrict file)
{
    snapshot_t* snapshot = get_snapshot();

    if(snapshot == nullptr) {
        return -ENOMEM;
    }

    drop_snapshot(file);

    file->snapshot = snapshot;
    file->snapshot_offset = 0;
    file->seq = snapshot->seq;
    file->pending_length = 0;
    file->pending_offset = 0;

    return 0;
}

static size_t take_bytes(char* restrict buffer, size_t size, const char* restrict text, size_t length, size_t* restrict offset)
{
    size_t count = length - *offset < size ? length - *offset : size;

    memcpy(buffer, &text[*offset], count);
    *offset += count;

    return count;
}

// A reader that fell behind the ring gets a new snapshot, which starts with snapshot_begin
static int fetch_event(devctl_file_t* restrict file, bool* restrict fetched)
{
    mutex_acquire(&ring_lock);

    uint64_t next_seq = ring_next_seq;
    *fetched = file->seq != next_seq && next_seq - file->seq <= RING_LINES;

    if(*fetched) {
        const ring_line_t* slot = &ring[file->seq % RING_LINES];
        memcpy(file->pending, slot->text, slot->length);
        file->pending_length = slot->length;
        file->pending_offset = 0;
        file->seq += 1;
    }

    mutex_release(&ring_lock);

    if(file->seq != next_seq && !*fetched) {
        int retval = take_snapshot(file);
        *fetched = retval == 0;
        return retval;
    }

    return 0;
}

static ssize_t read_locked(devctl_file_t* restrict file, char* restrict buffer, size_t size)
{
    size_t copied = 0;

    while(copied < size) {
        snapshot_t* snapshot = file->snapshot;

        if(snapshot) {
            copied += take_bytes(&buffer[copied], size - copied, snapshot->text, snapshot->length, &file->snapshot_offset);

            if(file->snapshot_offset == snapshot->length) {
                drop_snapshot(file);
            }

            continue;
        }

        if(file->pending_offset < file->pending_length) {
            copied += take_bytes(&buffer[copied], size - copied, file->pending, file->pending_length, &file->pending_offset);
            continue;
        }

        bool fetched;
        int retval = fetch_event(file, &fetched);

        if(retval < 0 && copied == 0) {
            return retval;
        }

        if(!fetched) {
            break;
        }
    }

    return copied ? (ssize_t)(copied) : -EAGAIN;
}

static device_t* find_by_id(device_t* parent, uint64_t id)
{
    for(device_t* child = device_next_child(parent, nullptr); child;) {
        if(child->id == id) {
            return child;
        }

        device_t* found = find_by_id(child, id);

        if(found) {
            device_decref(child);
            return found;
        }

        device_t* next = device_next_child(parent, child);
        device_decref(child);
        child = next;
    }

    return nullptr;
}

static device_t* find_by_path(device_t* parent, const char* restrict path)
{
    for(device_t* child = device_next_child(parent, nullptr); child;) {
        char text[DEVICE_PATH_MAX];
        device_get_path(child, text, sizeof(text));

        if(0 == strcmp(text, path)) {
            return child;
        }

        device_t* found = find_by_path(child, path);

        if(found) {
            device_decref(child);
            return found;
        }

        device_t* next = device_next_child(parent, child);
        device_decref(child);
        child = next;
    }

    return nullptr;
}

static unsigned hex_value(char chr)
{
    return isdigit(chr) ? chr - '0' : tolower(chr) - 'a' + 10;
}

static bool unescape(char* restrict text)
{
    char* out = text;

    for(const char* in = text; *in; in += 1) {
        if(*in != '%') {
            *out = *in;
            out += 1;
            continue;
        }

        if(!isxdigit(in[1]) || !isxdigit(in[2])) {
            return false;
        }

        *out = hex_value(in[1]) * 16 + hex_value(in[2]);
        out += 1;
        in += 2;
    }

    *out = 0;

    return true;
}

static device_t* find_target(char* restrict target)
{
    device_t* root = device_get_root();

    if(root == nullptr) {
        return nullptr;
    }

    device_t* device = nullptr;

    if(0 == strncmp(target, "id=", 3)) {
        const char* end;
        unsigned long long id = strtoull(&target[3], &end, 10);

        if(end != &target[3] && *end == 0) {
            device = find_by_id(root, id);
        }
    }
    else if(0 == strncmp(target, "path=", 5) && unescape(&target[5])) {
        device = find_by_path(root, &target[5]);
    }

    device_decref(root);

    return device;
}

static int run_command(char* restrict command)
{
    size_t split = strcspn(command, " ");

    if(command[split] == 0) {
        return -EINVAL;
    }

    command[split] = 0;
    char* target = &command[split + 1];

    if(strchr(target, ' ')) {
        return -EINVAL;
    }

    void (*request)(device_t* restrict device) = nullptr;

    if(0 == strcmp(command, "rescan")) {
        request = &device_rescan;
    }
    else if(0 == strcmp(command, "unbind")) {
        request = &device_unbind;
    }
    else if(0 == strcmp(command, "eject")) {
        request = &device_eject;
    }
    else {
        return -EINVAL;
    }

    device_t* device = find_target(target);

    if(device == nullptr) {
        return -ENOENT;
    }

    request(device);
    device_decref(device);

    return 0;
}

static int devctl_open_file(const char_interface_t* restrict interface, unsigned flags, void** restrict file_ctxdata)
{
    thread_t* thread = this_thread();

    if((flags & O_WRONLY) && !cred_capable(thread->credentials, CAP_DEVICE)) {
        return -EPERM;
    }

    devctl_file_t* file = slab_alloc(sizeof(devctl_file_t));

    if(file == nullptr) {
        return -ENOMEM;
    }

    mutex_init(&file->lock);
    file->snapshot = nullptr;
    file->seq = 0;
    file->pending_length = 0;
    file->pending_offset = 0;

    if((flags & O_RDONLY) && take_snapshot(file) < 0) {
        mutex_deinit(&file->lock);
        slab_free(file);
        return -ENOMEM;
    }

    *file_ctxdata = file;

    return 0;
}

static void devctl_close_file(const char_interface_t* restrict interface, void* restrict file_ctxdata)
{
    devctl_file_t* file = file_ctxdata;

    drop_snapshot(file);
    mutex_deinit(&file->lock);
    slab_free(file);
}

static ssize_t devctl_read(const char_interface_t* restrict interface, void* restrict file_ctxdata, off_t offset, void* restrict buffer,
    size_t size, unsigned flags)
{
    devctl_file_t* file = file_ctxdata;
    event_t* appended = &ring_appended;
    ssize_t retval;

    mutex_acquire(&file->lock);

    for(;;) {
        retval = read_locked(file, buffer, size);

        if(retval != -EAGAIN || (flags & O_NONBLOCK)) {
            break;
        }

        retval = event_await_interruptible(&appended, 1);

        if(retval < 0) {
            break;
        }
    }

    mutex_release(&file->lock);

    return retval;
}

// A write is one command, with or without its newline
static ssize_t devctl_write(const char_interface_t* restrict interface, void* restrict file_ctxdata, off_t offset,
    const void* restrict buffer, size_t size, unsigned flags)
{
    char command[DEVCTL_LINE_MAX];
    size_t length = size;

    if(length && ((const char*)(buffer))[length - 1] == '\n') {
        length -= 1;
    }

    if(length >= sizeof(command) || memchr(buffer, 0, length) || memchr(buffer, '\n', length)) {
        return -EINVAL;
    }

    memcpy(command, buffer, length);
    command[length] = 0;

    int retval = run_command(command);

    if(retval < 0) {
        return retval;
    }

    return (ssize_t)(size);
}

// The offset isn't a position: seeking to the start gives a new snapshot
static off_t devctl_seek(const char_interface_t* restrict interface, void* restrict file_ctxdata, off_t position, off_t offset, int origin)
{
    devctl_file_t* file = file_ctxdata;

    if(offset || origin != SEEK_SET) {
        return -EINVAL;
    }

    mutex_acquire(&file->lock);
    int retval = take_snapshot(file);
    mutex_release(&file->lock);

    return retval;
}

static const char_interface_ops_t devctl_ops = {
    .open_file = &devctl_open_file,
    .close_file = &devctl_close_file,
    .read = &devctl_read,
    .write = &devctl_write,
    .seek = &devctl_seek,
};

void devctl_put(devctl_line_t* restrict line, const char* restrict key, const char* restrict fmt, ...)
{
    char value[DEVCTL_LINE_MAX];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(value, sizeof(value), fmt, ap);
    va_end(ap);

    char field[DEVCTL_LINE_MAX];
    size_t length = (size_t)(snprintf(field, sizeof(field), " %s=", key));

    for(const char* chr = value; *chr && length < sizeof(field) - 3; chr += 1) {
        if(is_shell_safe(*chr)) {
            field[length] = *chr;
            length += 1;
        }
        else {
            length += (size_t)(snprintf(&field[length], sizeof(field) - length, "%%%02X", (unsigned)((unsigned char)(*chr))));
        }
    }

    if(line->length + length >= DEVCTL_LINE_MAX - 1) {
        return;
    }

    memcpy(&line->text[line->length], field, length);
    line->length += length;
    line->text[line->length] = 0;
}

void devctl_interface_added(const device_t* restrict device, const char* restrict kind, const char* restrict name)
{
    devctl_line_t line;
    describe_interface(device, kind, name, &line);
    post(&line);
}

void devctl_interface_removed(const device_t* restrict device, const char* restrict name)
{
    devctl_line_t line;
    begin_line(&line, "interface_remove");
    devctl_put(&line, "id", "%llu", (unsigned long long)(device->id));
    devctl_put(&line, "name", "%s", name);
    post(&line);
}

void devctl_node_added(device_t* restrict device)
{
    devctl_line_t line;
    describe_node(device, &line);
    post(&line);
}

void devctl_node_removed(const device_t* restrict device)
{
    devctl_line_t line;
    begin_line(&line, "remove");
    devctl_put(&line, "id", "%llu", (unsigned long long)(device->id));
    post(&line);
}

void devctl_node_bound(const device_t* restrict device)
{
    devctl_line_t line;
    begin_line(&line, "bind");
    devctl_put(&line, "id", "%llu", (unsigned long long)(device->id));
    devctl_put(&line, "driver", "%s", device->driver->name);
    post(&line);
}

void devctl_node_unbound(const device_t* restrict device, const driver_t* restrict driver)
{
    devctl_line_t line;
    begin_line(&line, "unbind");
    devctl_put(&line, "id", "%llu", (unsigned long long)(device->id));
    devctl_put(&line, "driver", "%s", driver->name);
    post(&line);
}

void devctl_node_waiting(const device_t* restrict device, const driver_t* restrict driver)
{
    devctl_line_t line;
    begin_line(&line, "waiting");
    devctl_put(&line, "id", "%llu", (unsigned long long)(device->id));
    devctl_put(&line, "driver", "%s", driver->name);

    if(device->priv.waits_for) {
        devctl_put(&line, "for", "%llu", (unsigned long long)(device->priv.waits_for->id));
    }
    else {
        char name[DEVICE_NAME_MAX];

        if(0 == fwnode_get_name(&device->priv.waits_for_fwnode, name, sizeof(name))) {
            devctl_put(&line, "for_name", "%s", name);
        }
    }

    post(&line);
}

void devctl_attach_failed(const device_t* restrict device, const driver_t* restrict driver, int error)
{
    devctl_line_t line;
    begin_line(&line, "attach_failed");
    devctl_put(&line, "id", "%llu", (unsigned long long)(device->id));
    devctl_put(&line, "driver", "%s", driver->name);
    devctl_put(&line, "error", "%d", -error);
    post(&line);
}

static void init_devctl(void)
{
    devctl_device.ops = &devctl_ops;
    devctl_device.device = nullptr;

    int retval = dev_prefix_claim(&dev_prefix_devctl);

    if(retval == 0) {
        retval = char_interface_add_unitless(&devctl_device, &dev_prefix_devctl);
    }

    if(retval < 0) {
        pr_warning("devctl: %s", strerror(retval));
    }
}

initcall_bsp_dev(init_devctl, INIT_MIDDLE);
