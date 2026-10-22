#include <vex/atomic.h>
#include <vex/capability.h>
#include <vex/char_device.h>
#include <vex/cred.h>
#include <vex/dev_name.h>
#include <vex/errno.h>
#include <vex/event.h>
#include <vex/fcntl.h>
#include <vex/file.h>
#include <vex/initcall.h>
#include <vex/kplog.h>
#include <vex/kprintf.h>
#include <vex/mutex.h>
#include <vex/slab.h>
#include <vex/string.h>
#include <vex/thread.h>
#include <vex/uaccess.h>

static_assert(sizeof(kplog_record_t) + KP_LINE_MAX <= FILE_IO_CHUNK, "A read system call can't take the longest record");

typedef struct kplog_file_s kplog_file_t;

struct kplog_file_s {
    mutex_t lock; // Threads that share the file share its cursor
    kp_cursor_t cursor;
    unsigned level; // Of the records the file writes, which takes no lock
};

static_assert(sizeof(kplog_file_t) <= SLAB_MAX_SIZE);

static char_device_t kplog_device;

static int kplog_open_file(const char_device_t* restrict device, unsigned flags, void** restrict file_ctxdata)
{
    thread_t* thread = this_thread();

    if((flags & O_WRONLY) && !cred_capable(thread->credentials, CAP_SYSLOG)) {
        return -EPERM;
    }

    kplog_file_t* file = slab_alloc(sizeof(kplog_file_t));

    if(file == nullptr) {
        return -ENOMEM;
    }

    mutex_init(&file->lock);
    kprintf_seek_first(&file->cursor);
    atomic_store(&file->level, KP_NOTICE, ATOMIC_RELAXED);

    *file_ctxdata = file;

    return 0;
}

static void kplog_close_file(const char_device_t* restrict device, void* restrict file_ctxdata)
{
    kplog_file_t* file = file_ctxdata;

    mutex_deinit(&file->lock);
    slab_free(file);
}

// The cursor moves past a record only once the record is handed out whole
static ssize_t read_locked(kplog_file_t* restrict file, void* restrict buffer, size_t size)
{
    kp_cursor_t cursor = file->cursor;
    kplog_record_t record;
    char text[KP_LINE_MAX];

    if(!kprintf_read(&cursor, &record, text)) {
        return -EAGAIN;
    }

    if(record.seq != file->cursor.seq) {
        kprintf_seek_first(&file->cursor);
        return -EPIPE;
    }

    size_t total = sizeof(record) + record.size;

    if(size < total) {
        return -EINVAL;
    }

    char* bytes = buffer;
    memcpy(bytes, &record, sizeof(record));
    memcpy(&bytes[sizeof(record)], text, record.size);

    file->cursor = cursor;

    return (ssize_t)(total);
}

static ssize_t kplog_read(const char_device_t* restrict device, void* restrict file_ctxdata, off_t offset, void* restrict buffer,
    size_t size, unsigned flags)
{
    kplog_file_t* file = file_ctxdata;
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
static off_t kplog_seek(const char_device_t* restrict device, void* restrict file_ctxdata, off_t position, off_t offset, int origin)
{
    kplog_file_t* file = file_ctxdata;

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

// A record is one line, so a longer write fails whole
// instead of the system call passing the rest as another
static ssize_t kplog_write(const char_device_t* restrict device, void* restrict file_ctxdata, off_t offset, const void* restrict buffer,
    size_t size, unsigned flags)
{
    kplog_file_t* file = file_ctxdata;
    const char* text = buffer;
    size_t text_size = size_without_line_end(text, size);

    if(text_size > KP_LINE_MAX || memchr(text, '\n', text_size) || memchr(text, '\r', text_size) || memchr(text, '\0', text_size)) {
        return -EINVAL;
    }

    kputs(atomic_load(&file->level, ATOMIC_RELAXED), KPLOG_USER, text, text_size);

    return (ssize_t)(size);
}

static int get_level(kplog_file_t* restrict file, uintptr_t argument)
{
    int32_t level = (int32_t)(atomic_load(&file->level, ATOMIC_RELAXED));

    if(user_copyout(argument, &level, sizeof(level)) < 0) {
        return -EFAULT;
    }

    return 0;
}

// Userspace can't log as severely as the kernel
// failing, which takes the levels above KP_ERROR
static int set_level(kplog_file_t* restrict file, uintptr_t argument)
{
    int32_t level;

    if(user_copyin(&level, argument, sizeof(level)) < 0) {
        return -EFAULT;
    }

    if(level < (int32_t)(KP_ERROR) || level > (int32_t)(KP_VERBOSE)) {
        return -EINVAL;
    }

    atomic_store(&file->level, (unsigned)(level), ATOMIC_RELAXED);

    return 0;
}

static int kplog_ioctl(const char_device_t* restrict device, void* restrict file_ctxdata, uint32_t request, uintptr_t argument)
{
    switch(request) {
        case KPLOG_GETLEVEL:
            return get_level(file_ctxdata, argument);

        case KPLOG_SETLEVEL:
            return set_level(file_ctxdata, argument);

        default:
            return -ENOTTY;
    }
}

static const char_device_ops_t kplog_ops = {
    .open_file = &kplog_open_file,
    .close_file = &kplog_close_file,
    .read = &kplog_read,
    .write = &kplog_write,
    .seek = &kplog_seek,
    .ioctl = &kplog_ioctl,
};

static void init_kplog(void)
{
    kplog_device.ops = &kplog_ops;

    int retval = dev_prefix_claim(&dev_prefix_kplog);

    if(retval == 0) {
        retval = char_device_add_unitless(&kplog_device, &dev_prefix_kplog);
    }

    if(retval < 0) {
        pr_warning("kplog: %s", strerror(retval));
    }
}

initcall_bsp_dev(init_kplog, INIT_MIDDLE);
