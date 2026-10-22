#include <vex/capability.h>
#include <vex/char_interface.h>
#include <vex/cred.h>
#include <vex/ctype.h>
#include <vex/dev_name.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/fcntl.h>
#include <vex/file.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/kmsg.h>
#include <vex/kprintf.h>
#include <vex/mutex.h>
#include <vex/sprintf.h>
#include <vex/string.h>
#include <vex/thread.h>

#define KMSG_HEAD_MAX 64 // "<prefix>,<seq>,<usec>,-;" with 64-bit numbers

#define KMSG_LINE_MAX (KMSG_HEAD_MAX + KP_LINE_MAX * 4 + 1) // Each text byte may become \xNN, then the newline

typedef struct kmsg_file_s kmsg_file_t;

struct kmsg_file_s {
    mutex_t lock; // Threads that share the file share its cursor
    kp_cursor_t cursor;
    bool lost;                // The ring dropped records under the cursor, which the next read tells with -EPIPE
    char line[KMSG_LINE_MAX]; // The record that a short read took only part of
    size_t line_length;
    size_t line_offset;
};

static char_interface_t kmsg_device;

static int kmsg_open_file(const char_interface_t* restrict interface, unsigned flags, void** restrict file_ctxdata)
{
    thread_t* thread = this_thread();

    if((flags & O_WRONLY) && !cred_capable(thread->credentials, CAP_SYSLOG)) {
        return -EPERM;
    }

    kmsg_file_t* file = kmalloc(sizeof(kmsg_file_t));

    if(file == nullptr) {
        return -ENOMEM;
    }

    mutex_init(&file->lock);
    kprintf_seek_first(&file->cursor);
    file->lost = false;
    file->line_length = 0;
    file->line_offset = 0;

    *file_ctxdata = file;

    return 0;
}

static void kmsg_close_file(const char_interface_t* restrict interface, void* restrict file_ctxdata)
{
    kmsg_file_t* file = file_ctxdata;

    mutex_deinit(&file->lock);
    kfree(file);
}

static void format_line(kmsg_file_t* restrict file, const kp_record_t* restrict record, const char* restrict text)
{
    unsigned facility;

    if(record->flags & KP_USER) {
        facility = KMSG_FACILITY_USER;
    }
    else {
        facility = KMSG_FACILITY_KERNEL;
    }

    unsigned prefix = facility << 3 | record->level;
    unsigned long long seq = record->seq;
    unsigned long long usec = record->time_ns / 1000;
    ssize_t length = snprintf(file->line, KMSG_HEAD_MAX, "%u,%llu,%llu,-;", prefix, seq, usec);

    for(size_t i = 0; i < record->size; ++i) {
        unsigned char chr = text[i];

        if(chr == '\\' || !isprint(chr)) {
            length += snprintf(&file->line[length], 5, "\\x%02x", chr);
        }
        else {
            file->line[length] = chr;
            length += 1;
        }
    }

    file->line[length] = '\n';
    file->line_length = length + 1;
    file->line_offset = 0;
}

// The cursor moves past a record only once the record is in the line
static bool fetch_record(kmsg_file_t* restrict file)
{
    kp_cursor_t cursor = file->cursor;
    kp_record_t record;
    char text[KP_LINE_MAX];

    if(!kprintf_read(&cursor, &record, text)) {
        return false;
    }

    if(record.seq != file->cursor.seq) {
        kprintf_seek_first(&file->cursor);
        file->lost = true;
        return false;
    }

    format_line(file, &record, text);
    file->cursor = cursor;

    return true;
}

static ssize_t read_locked(kmsg_file_t* restrict file, char* restrict buffer, size_t size)
{
    size_t copied = 0;

    while(copied < size) {
        if(file->line_offset == file->line_length && (file->lost || !fetch_record(file))) {
            break;
        }

        size_t left = file->line_length - file->line_offset;
        size_t count = left < size - copied ? left : size - copied;

        memcpy(&buffer[copied], &file->line[file->line_offset], count);
        file->line_offset += count;
        copied += count;
    }

    if(copied) {
        return (ssize_t)(copied);
    }

    if(file->lost) {
        file->lost = false;
        return -EPIPE;
    }

    return -EAGAIN;
}

static ssize_t kmsg_read(const char_interface_t* restrict interface, void* restrict file_ctxdata, off_t offset, void* restrict buffer,
    size_t size, unsigned flags)
{
    kmsg_file_t* file = file_ctxdata;
    event_t* appended = &kprintf_appended;
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

// The offset isn't a position: only the oldest record and the next one to come can be sought
static off_t kmsg_seek(const char_interface_t* restrict interface, void* restrict file_ctxdata, off_t position, off_t offset, int origin)
{
    kmsg_file_t* file = file_ctxdata;

    if(offset || (origin != SEEK_SET && origin != SEEK_END)) {
        return -EINVAL;
    }

    mutex_acquire(&file->lock);

    if(origin == SEEK_SET) {
        kprintf_seek_first(&file->cursor);
    }
    else {
        kprintf_seek_end(&file->cursor);
    }

    file->lost = false;
    file->line_length = 0;
    file->line_offset = 0;

    mutex_release(&file->lock);

    return 0;
}

// Drops the line end of a text from any
// convention: LF, CR, or CRLF of the DOS world
static size_t size_without_line_end(const char* restrict text, size_t size)
{
    if(size && text[size - 1U] == '\n') {
        size -= 1U;
    }

    if(size && text[size - 1U] == '\r') {
        size -= 1U;
    }

    return size;
}

// Takes a leading <N> as syslog writes it, facility and all; text that only looks like one stays text
static size_t parse_prefix(const char* restrict text, size_t size, unsigned* restrict level)
{
    unsigned value = 0;
    size_t i = 1;

    if(size < 3 || text[0] != '<') {
        return 0;
    }

    while(i < size && i <= 4 && isdigit(text[i])) {
        value = value * 10 + (text[i] - '0');
        i += 1;
    }

    if(i == 1 || i == size || text[i] != '>') {
        return 0;
    }

    *level = value & 7;

    return i + 1;
}

// A record is one line, so a longer write fails whole instead of the system call passing the
// rest as another. Userspace can't log as severely as the kernel failing, which takes the levels above KP_ERROR
static ssize_t kmsg_write(const char_interface_t* restrict interface, void* restrict file_ctxdata, off_t offset,
    const void* restrict buffer, size_t size, unsigned flags)
{
    const char* text = buffer;
    size_t text_size = size_without_line_end(text, size);
    unsigned level = KP_NOTICE;
    size_t prefix = parse_prefix(text, text_size, &level);

    text += prefix;
    text_size -= prefix;

    if(level < KP_ERROR || text_size > KP_LINE_MAX) {
        return -EINVAL;
    }

    if(memchr(text, '\n', text_size) || memchr(text, '\r', text_size) || memchr(text, '\0', text_size)) {
        return -EINVAL;
    }

    kputs(level, KP_USER, text, text_size);

    return (ssize_t)(size);
}

static const char_interface_ops_t kmsg_ops = {
    .open_file = &kmsg_open_file,
    .close_file = &kmsg_close_file,
    .read = &kmsg_read,
    .write = &kmsg_write,
    .seek = &kmsg_seek,
};

static void init_kmsg(void)
{
    kmsg_device.ops = &kmsg_ops;
    kmsg_device.device = nullptr;

    int retval = dev_prefix_claim(&dev_prefix_kmsg);

    if(retval == 0) {
        retval = char_interface_add_unitless(&kmsg_device, &dev_prefix_kmsg);
    }

    if(retval < 0) {
        pr_warning("kmsg: %s", strerror(retval));
    }
}

initcall_bsp_dev(init_kmsg, INIT_MIDDLE);
