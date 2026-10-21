#include <arch/hart.h>
#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/capability.h>
#include <vex/cred.h>
#include <vex/errno.h>
#include <vex/fcntl.h>
#include <vex/format.h>
#include <vex/initcall.h>
#include <vex/ioctl.h>
#include <vex/panic.h>
#include <vex/pgrp.h>
#include <vex/process.h>
#include <vex/pslab.h>
#include <vex/sched.h>
#include <vex/signal.h>
#include <vex/string.h>
#include <vex/thread.h>
#include <vex/tty_ldisc.h>
#include <vex/uaccess.h>

#define VTIME_UNIT_NSEC UINT64_C(100000000) // VTIME counts tenths of a second

#define DEFAULT_SPEED 38400

#define STATUS_LINE_SIZE   128
#define STATUS_MEMBERS_MAX 4

#define CONTROL(letter) ((letter) & 0x1F)

typedef struct status_line_s status_line_t;
typedef struct ctty_drop_s ctty_drop_t;

struct status_line_s {
    char text[STATUS_LINE_SIZE];
    size_t length;
};

// References a detached controlling terminal leaves
// behind, dropped once process_group_lock is released
struct ctty_drop_s {
    session_t* session;
    process_group_t* foreground;
    tty_device_t* device;
};

static spinlock_t worker_lock = SPINLOCK_INIT; // Taken with interrupts disabled
static tty_device_t* worker_head = nullptr;
static tty_device_t* worker_tail = nullptr;
static event_t worker_wakeup;

static tty_device_t* tty_of(const chr_device_t* restrict chr)
{
    return chr->ctxdata;
}

static void leave(tty_device_t* restrict device)
{
    atomic_fetch_sub(&device->priv.in_flight, 1, ATOMIC_RELEASE);
}

static bool enter(tty_device_t* restrict device)
{
    atomic_fetch_add(&device->priv.in_flight, 1, ATOMIC_SEQ_CST);

    if(atomic_load(&device->priv.removed, ATOMIC_SEQ_CST)) {
        leave(device);
        return false;
    }

    return true;
}

static bool is_removed(const tty_device_t* restrict device)
{
    return atomic_load(&device->priv.removed, ATOMIC_ACQUIRE);
}

static void set_default_termios(termios_t* restrict termios)
{
    memset(termios, 0, sizeof(*termios));

    termios->c_iflag = ICRNL | IXON | IMAXBEL;
    termios->c_oflag = OPOST | ONLCR;
    termios->c_cflag = CS8 | CREAD | HUPCL;
    termios->c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN;

    termios->c_cc[VINTR] = CONTROL('C');
    termios->c_cc[VQUIT] = CONTROL('\\');
    termios->c_cc[VERASE] = 0x7F;
    termios->c_cc[VKILL] = CONTROL('U');
    termios->c_cc[VEOF] = CONTROL('D');
    termios->c_cc[VSTART] = CONTROL('Q');
    termios->c_cc[VSTOP] = CONTROL('S');
    termios->c_cc[VSUSP] = CONTROL('Z');
    termios->c_cc[VREPRINT] = CONTROL('R');
    termios->c_cc[VWERASE] = CONTROL('W');
    termios->c_cc[VLNEXT] = CONTROL('V');
    termios->c_cc[VDISCARD] = CONTROL('O');
    termios->c_cc[VMIN] = 1;
    termios->c_cc[VTIME] = 0;
    termios->c_cc[VSTATUS] = CONTROL('T');

    termios->c_ispeed = DEFAULT_SPEED;
    termios->c_ospeed = DEFAULT_SPEED;
}

static void reset_rings_locked(tty_device_t* restrict device)
{
    tty_device_private_t* priv = &device->priv;

    priv->flip.head = priv->flip.tail;
    priv->output.head = priv->output.tail;

    memset(&priv->ldisc, 0, sizeof(priv->ldisc));
    priv->ldisc.line_end = priv->input.tail;
    priv->input.head = priv->input.tail;
}

static void notify_input_locked(tty_device_t* restrict device)
{
    if(tty_ldisc_readable(device)) {
        event_trigger(&device->priv.readable, false);
    }
}

// The caller calls start_tx once it dropped the lock when this says so
static bool notify_output_locked(tty_device_t* restrict device)
{
    const tty_ring_t* output = &device->priv.output;

    if(tty_ring_room(output) >= TTY_EXPANSION_MAX) {
        event_trigger(&device->priv.writable, false);
    }

    if(tty_ring_used(output) == 0) {
        event_trigger(&device->priv.drained, false);
        return false;
    }

    return !device->priv.ldisc.output_stopped;
}

static void start_tx(tty_device_t* restrict device, bool transmit)
{
    if(transmit && device->ops->start_tx) {
        device->ops->start_tx(device);
    }
}

static void queue_for_worker(tty_device_t* restrict device)
{
    bool intr_state = spinlock_acquire_intr(&worker_lock);
    bool was_queued = device->priv.queued;

    if(!was_queued) {
        chr_device_incref(&device->chr);

        device->priv.queued = true;
        device->priv.worker_next = nullptr;

        if(worker_tail) {
            worker_tail->priv.worker_next = device;
        }
        else {
            worker_head = device;
        }

        worker_tail = device;
    }

    spinlock_release_intr(&worker_lock, intr_state);

    if(!was_queued) {
        event_trigger(&worker_wakeup, true);
    }
}

// Hands out the queue's reference to the device
static tty_device_t* worker_pop(void)
{
    bool intr_state = spinlock_acquire_intr(&worker_lock);
    tty_device_t* device = worker_head;

    if(device) {
        worker_head = device->priv.worker_next;

        if(worker_head == nullptr) {
            worker_tail = nullptr;
        }

        device->priv.queued = false;
    }

    spinlock_release_intr(&worker_lock, intr_state);

    return device;
}

static void status_append(int character, void* restrict user)
{
    status_line_t* line = user;

    if(line->length + 1 < sizeof(line->text)) {
        line->text[line->length] = character;
        line->length += 1;
    }
}

static const char* process_state(process_t* restrict process)
{
    uint8_t flags = atomic_load_n(&process->flags, ATOMIC_ACQUIRE);

    if(flags & PROCESS_ZOMBIE) {
        return "zombie";
    }

    if(flags & PROCESS_EXITING) {
        return "exiting";
    }

    return "running";
}

// Under process_group_lock, which keeps the foreground group and its members still
static void format_status_locked(const tty_device_t* restrict device, status_line_t* restrict line)
{
    const process_group_t* foreground = device->priv.foreground;

    if(foreground == nullptr) {
        format(&status_append, line, "no foreground process group\n");
        return;
    }

    format(&status_append, line, "process group %d:", (int)(foreground->pgid));

    unsigned count = 0;

    for(process_t* it = foreground->members; it; it = it->group_next) {
        if(count == STATUS_MEMBERS_MAX) {
            format(&status_append, line, " ...");
            break;
        }

        format(&status_append, line, " %d (%s)", (int)(it->pid), process_state(it));
        count += 1;
    }

    format(&status_append, line, "\n");
}

static void report_status(tty_device_t* restrict device)
{
    status_line_t line = { 0 };

    process_group_lock_acquire();
    format_status_locked(device, &line);
    process_group_lock_release();

    bool intr_state = spinlock_acquire_intr(&device->priv.lock);

    for(size_t i = 0; i < line.length; ++i) {
        if(!tty_ldisc_output(device, line.text[i])) {
            break;
        }
    }

    bool transmit = notify_output_locked(device);

    spinlock_release_intr(&device->priv.lock, intr_state);

    start_tx(device, transmit);
}

static void send_signals(tty_device_t* restrict device, uint32_t signals)
{
    process_group_lock_acquire();

    process_group_t* foreground = device->priv.foreground;

    for(int sig = 1; foreground && sig < 32; sig += 1) {
        if(signals & (1U << sig)) {
            process_group_signal_locked(foreground, sig);
        }
    }

    process_group_lock_release();
}

static void process_input(tty_device_t* restrict device)
{
    if(!enter(device)) {
        return;
    }

    bool intr_state = spinlock_acquire_intr(&device->priv.lock);

    tty_ldisc_receive(device);

    uint32_t signals = device->priv.ldisc.signals;
    uint32_t lflag = device->priv.termios.c_lflag;

    device->priv.ldisc.signals = 0;

    notify_input_locked(device);
    bool transmit = notify_output_locked(device);

    spinlock_release_intr(&device->priv.lock, intr_state);

    start_tx(device, transmit);

    if(signals) {
        send_signals(device, signals);
    }

    if((signals & (1U << SIGINFO)) && 0 == (lflag & NOKERNINFO)) {
        report_status(device);
    }

    leave(device);
}

// Runs the line discipline where it may sleep and
// look up process groups, which an interrupt handler can't
static void __noreturn tty_worker(void* restrict arg)
{
    for(;;) {
        event_t* wakeup = &worker_wakeup;
        (void)event_await_blocking(&wakeup, 1);

        for(tty_device_t* device = worker_pop(); device; device = worker_pop()) {
            process_input(device);
            chr_device_decref(&device->chr);
        }
    }
}

static process_t* calling_process(void)
{
    thread_t* thread = this_thread();
    return thread->process;
}

// Under process_group_lock
static void detach_locked(tty_device_t* restrict device, ctty_drop_t* restrict drop)
{
    session_t* session = device->priv.session;

    drop->session = session;
    drop->foreground = device->priv.foreground;
    drop->device = device;

    session->tty = nullptr;
    device->priv.session = nullptr;
    device->priv.foreground = nullptr;
}

static void drop_detached(const ctty_drop_t* restrict drop)
{
    if(drop->session) {
        session_decref(drop->session);
    }

    if(drop->foreground) {
        process_group_decref(drop->foreground);
    }

    if(drop->device) {
        chr_device_decref(&drop->device->chr);
    }
}

// Under process_group_lock
static bool is_controlling_locked(const tty_device_t* restrict device, process_t* restrict process)
{
    process_group_t* group = process->group;
    session_t* session = group->session;

    return session->tty == device;
}

static int make_controlling(tty_device_t* restrict device, uintptr_t argument)
{
    int32_t force;

    if(user_copyin(&force, argument, sizeof(force)) < 0) {
        return -EFAULT;
    }

    thread_t* thread = this_thread();
    process_t* process = thread->process;

    if(process == nullptr) {
        return -EPERM;
    }

    ctty_drop_t drop = { 0 };
    int retval = 0;

    process_group_lock_acquire();

    process_group_t* group = process->group;
    session_t* session = group->session;

    if(session->sid != process->pid) {
        retval = -EPERM;
    }
    else if(session->tty == device) {
        retval = 0;
    }
    else if(session->tty) {
        retval = -EPERM;
    }
    else if(device->priv.session && !(force && cred_capable(thread->credentials, CAP_SYS_ADMIN))) {
        retval = -EPERM;
    }
    else {
        if(device->priv.session) {
            detach_locked(device, &drop);
        }

        chr_device_incref(&device->chr);
        session_incref(session);
        process_group_incref(group);

        session->tty = device;
        device->priv.session = session;
        device->priv.foreground = group;
    }

    process_group_lock_release();

    drop_detached(&drop);

    return retval;
}

static int give_up_controlling(tty_device_t* restrict device)
{
    process_t* process = calling_process();

    if(process == nullptr) {
        return -ENOTTY;
    }

    ctty_drop_t drop = { 0 };
    int retval = 0;

    process_group_lock_acquire();

    process_group_t* group = process->group;
    session_t* session = group->session;

    if(session->tty != device) {
        retval = -ENOTTY;
    }
    else if(session->sid != process->pid) {
        retval = -EPERM; // UNDONE: only the leader lets go of the session's terminal for now
    }
    else {
        detach_locked(device, &drop);
    }

    process_group_lock_release();

    drop_detached(&drop);

    return retval;
}

static int get_foreground(tty_device_t* restrict device, uintptr_t argument)
{
    process_t* process = calling_process();
    pid_t pgid = 0;
    int retval = 0;

    process_group_lock_acquire();

    if(process == nullptr || !is_controlling_locked(device, process)) {
        retval = -ENOTTY;
    }
    else {
        process_group_t* foreground = device->priv.foreground;
        pgid = foreground->pgid;
    }

    process_group_lock_release();

    if(retval == 0 && user_copyout(argument, &pgid, sizeof(pgid)) < 0) {
        retval = -EFAULT;
    }

    return retval;
}

static int set_foreground(tty_device_t* restrict device, uintptr_t argument)
{
    pid_t pgid;

    if(user_copyin(&pgid, argument, sizeof(pgid)) < 0) {
        return -EFAULT;
    }

    if(pgid <= 0) {
        return -EINVAL;
    }

    process_t* process = calling_process();
    process_group_t* old_group = nullptr;
    int retval = 0;

    process_group_lock_acquire();

    process_group_t* group = process_group_find_locked(pgid);

    if(process == nullptr || !is_controlling_locked(device, process)) {
        retval = -ENOTTY;
    }
    else if(group == nullptr || group->session != device->priv.session) {
        retval = -EPERM;
    }
    else {
        process_group_incref(group);
        old_group = device->priv.foreground;
        device->priv.foreground = group;
    }

    process_group_lock_release();

    if(old_group) {
        process_group_decref(old_group);
    }

    return retval;
}

static int get_session(tty_device_t* restrict device, uintptr_t argument)
{
    pid_t sid = 0;
    int retval = 0;

    process_group_lock_acquire();

    session_t* session = device->priv.session;

    if(session == nullptr) {
        retval = -ENOTTY;
    }
    else {
        sid = session->sid;
    }

    process_group_lock_release();

    if(retval == 0 && user_copyout(argument, &sid, sizeof(sid)) < 0) {
        retval = -EFAULT;
    }

    return retval;
}

static int wait_drained(tty_device_t* restrict device)
{
    bool intr_state = spinlock_acquire_intr(&device->priv.lock);

    while(tty_ring_used(&device->priv.output) && !is_removed(device)) {
        event_t* drained = &device->priv.drained;
        ssize_t result = event_await_unlock(&drained, 1, &device->priv.lock, intr_state, 0);

        if(result < 0) {
            return result;
        }

        intr_state = spinlock_acquire_intr(&device->priv.lock);
    }

    spinlock_release_intr(&device->priv.lock, intr_state);

    return 0;
}

static int get_termios(tty_device_t* restrict device, uintptr_t argument)
{
    bool intr_state = spinlock_acquire_intr(&device->priv.lock);
    termios_t termios = device->priv.termios;
    spinlock_release_intr(&device->priv.lock, intr_state);

    if(user_copyout(argument, &termios, sizeof(termios)) < 0) {
        return -EFAULT;
    }

    return 0;
}

static int set_termios(tty_device_t* restrict device, uintptr_t argument, uint32_t request)
{
    termios_t termios;

    if(user_copyin(&termios, argument, sizeof(termios)) < 0) {
        return -EFAULT;
    }

    mutex_acquire(&device->priv.write_lock);

    int retval = 0;

    if(request != TCSETS) {
        retval = wait_drained(device);
    }

    if(retval == 0 && device->ops->set_termios) {
        retval = device->ops->set_termios(device, &termios);
    }

    if(retval == 0) {
        bool intr_state = spinlock_acquire_intr(&device->priv.lock);
        termios_t old_termios = device->priv.termios;

        device->priv.termios = termios;
        tty_ldisc_set_termios(device, &old_termios);

        if(request == TCSETSF) {
            device->priv.flip.head = device->priv.flip.tail;
            tty_ldisc_flush_input(device);
        }

        notify_input_locked(device);

        spinlock_release_intr(&device->priv.lock, intr_state);
    }

    mutex_release(&device->priv.write_lock);

    return retval;
}

static int get_window_size(tty_device_t* restrict device, uintptr_t argument)
{
    bool intr_state = spinlock_acquire_intr(&device->priv.lock);
    winsize_t winsize = device->priv.winsize;
    spinlock_release_intr(&device->priv.lock, intr_state);

    if(user_copyout(argument, &winsize, sizeof(winsize)) < 0) {
        return -EFAULT;
    }

    return 0;
}

static int set_window_size(tty_device_t* restrict device, uintptr_t argument)
{
    winsize_t winsize;

    if(user_copyin(&winsize, argument, sizeof(winsize)) < 0) {
        return -EFAULT;
    }

    bool intr_state = spinlock_acquire_intr(&device->priv.lock);
    bool changed = 0 != memcmp(&device->priv.winsize, &winsize, sizeof(winsize));
    device->priv.winsize = winsize;
    spinlock_release_intr(&device->priv.lock, intr_state);

    if(changed) {
        send_signals(device, 1U << SIGWINCH);
    }

    return 0;
}

static int flush_queues(tty_device_t* restrict device, uintptr_t argument)
{
    int32_t queue;

    if(user_copyin(&queue, argument, sizeof(queue)) < 0) {
        return -EFAULT;
    }

    if(queue != TCIFLUSH && queue != TCOFLUSH && queue != TCIOFLUSH) {
        return -EINVAL;
    }

    bool intr_state = spinlock_acquire_intr(&device->priv.lock);

    if(queue != TCOFLUSH) {
        device->priv.flip.head = device->priv.flip.tail;
        tty_ldisc_flush_input(device);
    }

    if(queue != TCIFLUSH) {
        device->priv.output.head = device->priv.output.tail;
    }

    (void)notify_output_locked(device);

    spinlock_release_intr(&device->priv.lock, intr_state);

    return 0;
}

static int control_flow(tty_device_t* restrict device, uintptr_t argument)
{
    int32_t action;

    if(user_copyin(&action, argument, sizeof(action)) < 0) {
        return -EFAULT;
    }

    bool intr_state = spinlock_acquire_intr(&device->priv.lock);
    const termios_t* termios = &device->priv.termios;
    tty_ring_t* output = &device->priv.output;
    int retval = 0;

    if(action == TCOOFF) {
        device->priv.ldisc.output_stopped = true;
    }
    else if(action == TCOON) {
        device->priv.ldisc.output_stopped = false;
    }
    else if(action == TCIOFF || action == TCION) {
        uint8_t special = termios->c_cc[VSTART];

        if(action == TCIOFF) {
            special = termios->c_cc[VSTOP];
        }

        if(special != _POSIX_VDISABLE && tty_ring_room(output)) {
            tty_ring_put(output, special);
        }
    }
    else {
        retval = -EINVAL;
    }

    bool transmit = notify_output_locked(device);

    spinlock_release_intr(&device->priv.lock, intr_state);

    start_tx(device, transmit);

    return retval;
}

static int copyout_count(uintptr_t argument, size_t count)
{
    int32_t value = count;

    if(user_copyout(argument, &value, sizeof(value)) < 0) {
        return -EFAULT;
    }

    return 0;
}

static int set_exclusive(tty_device_t* restrict device, bool exclusive)
{
    chr_device_set_exclusive(&device->chr, exclusive);
    return 0;
}

static int count_input(tty_device_t* restrict device, uintptr_t argument)
{
    bool intr_state = spinlock_acquire_intr(&device->priv.lock);
    size_t count = tty_ldisc_readable(device);
    spinlock_release_intr(&device->priv.lock, intr_state);

    return copyout_count(argument, count);
}

static int count_output(tty_device_t* restrict device, uintptr_t argument)
{
    bool intr_state = spinlock_acquire_intr(&device->priv.lock);
    size_t count = tty_ring_used(&device->priv.output);
    spinlock_release_intr(&device->priv.lock, intr_state);

    return copyout_count(argument, count);
}

static int handle_ioctl(tty_device_t* restrict device, uint32_t request, uintptr_t argument)
{
    switch(request) {
        case TCGETS:
            return get_termios(device, argument);

        case TCSETS:
        case TCSETSW:
        case TCSETSF:
            return set_termios(device, argument, request);

        case TIOCGWINSZ:
            return get_window_size(device, argument);

        case TIOCSWINSZ:
            return set_window_size(device, argument);

        case TIOCGPGRP:
            return get_foreground(device, argument);

        case TIOCSPGRP:
            return set_foreground(device, argument);

        case TIOCGSID:
            return get_session(device, argument);

        case TIOCSCTTY:
            return make_controlling(device, argument);

        case TIOCNOTTY:
            return give_up_controlling(device);

        case TCFLSH:
            return flush_queues(device, argument);

        case TCXONC:
            return control_flow(device, argument);

        case TCDRAIN:
            return wait_drained(device);

        case TIOCEXCL:
            return set_exclusive(device, true);

        case TIOCNXCL:
            return set_exclusive(device, false);

        case TIOCOUTQ:
            return count_output(device, argument);

        case FIONREAD:
            return count_input(device, argument);

        default:
            break;
    }

    if(device->ops->ioctl) {
        return device->ops->ioctl(device, request, argument);
    }

    // A line without a break to send has nothing to do
    if(request == TCSENDBREAK) {
        return 0;
    }

    return -ENOTTY;
}

// VMIN and VTIME decide when a non-canonical read is done: VMIN bytes, or VTIME tenths of a
// second, of the whole read when VMIN is zero and between bytes otherwise; both zero just polls
static bool read_done(const termios_t* restrict termios, size_t copied, size_t size)
{
    size_t wanted = termios->c_cc[VMIN];

    if(wanted == 0) {
        wanted = 1;
    }

    if(wanted > size) {
        wanted = size;
    }

    if(copied >= wanted) {
        return true;
    }

    return termios->c_cc[VMIN] == 0 && termios->c_cc[VTIME] == 0;
}

static uint64_t read_timeout(const termios_t* restrict termios, size_t copied)
{
    uint64_t tenths = termios->c_cc[VTIME];

    if(0 == (termios->c_lflag & ICANON) && tenths && (termios->c_cc[VMIN] == 0 || copied > 0)) {
        return tenths * VTIME_UNIT_NSEC;
    }

    return 0;
}

static ssize_t read_input(tty_device_t* restrict device, uint8_t* restrict buffer, size_t size, unsigned flags)
{
    size_t copied = 0;
    ssize_t error = 0;

    bool intr_state = spinlock_acquire_intr(&device->priv.lock);

    for(;;) {
        const termios_t* termios = &device->priv.termios;
        bool end_of_file;

        copied += tty_ldisc_read(device, buffer + copied, size - copied, &end_of_file);

        if(termios->c_lflag & ICANON) {
            if(copied || end_of_file) {
                break;
            }
        }
        else if(read_done(termios, copied, size)) {
            break;
        }

        // A hung up terminal reads as end of file
        if(is_removed(device)) {
            break;
        }

        if(flags & O_NONBLOCK) {
            if(copied == 0) {
                error = -EAGAIN;
            }

            break;
        }

        event_t* readable = &device->priv.readable;
        ssize_t result = event_await_unlock(&readable, 1, &device->priv.lock, intr_state, read_timeout(termios, copied));

        intr_state = spinlock_acquire_intr(&device->priv.lock);

        if(result == -ETIMEDOUT) {
            break;
        }

        if(result < 0) {
            if(copied == 0) {
                error = result;
            }

            break;
        }
    }

    spinlock_release_intr(&device->priv.lock, intr_state);

    if(error) {
        return error;
    }

    return copied;
}

static ssize_t write_output(tty_device_t* restrict device, const uint8_t* restrict buffer, size_t size, unsigned flags)
{
    size_t written = 0;
    ssize_t error = 0;

    mutex_acquire(&device->priv.write_lock);

    bool intr_state = spinlock_acquire_intr(&device->priv.lock);

    for(;;) {
        while(written < size && tty_ldisc_output(device, buffer[written])) {
            written += 1;
        }

        bool transmit = notify_output_locked(device);
        bool done = written == size || is_removed(device) || (flags & O_NONBLOCK);

        spinlock_release_intr(&device->priv.lock, intr_state);

        start_tx(device, transmit);

        if(done) {
            break;
        }

        intr_state = spinlock_acquire_intr(&device->priv.lock);

        // The driver may have made room while the lock was dropped
        if(tty_ring_room(&device->priv.output) >= TTY_EXPANSION_MAX || is_removed(device)) {
            continue;
        }

        event_t* writable = &device->priv.writable;
        ssize_t result = event_await_unlock(&writable, 1, &device->priv.lock, intr_state, 0);

        if(result < 0) {
            error = result;
            break;
        }

        intr_state = spinlock_acquire_intr(&device->priv.lock);
    }

    mutex_release(&device->priv.write_lock);

    if(written) {
        return written;
    }

    if(is_removed(device)) {
        return -EIO;
    }

    if(error) {
        return error;
    }

    if(flags & O_NONBLOCK) {
        return -EAGAIN;
    }

    return 0;
}

static int tty_chr_open(const chr_device_t* restrict chr, unsigned flags)
{
    tty_device_t* device = tty_of(chr);

    bool intr_state = spinlock_acquire_intr(&device->priv.lock);
    reset_rings_locked(device);
    spinlock_release_intr(&device->priv.lock, intr_state);

    if(device->ops->open) {
        return device->ops->open(device);
    }

    return 0;
}

static void tty_chr_close(const chr_device_t* restrict chr)
{
    tty_device_t* device = tty_of(chr);

    if(device->ops->close) {
        device->ops->close(device);
    }
}

static ssize_t tty_chr_read(const chr_device_t* restrict chr, void* restrict file_ctxdata, off_t offset, void* restrict buffer, size_t size,
    unsigned flags)
{
    tty_device_t* device = tty_of(chr);

    if(size == 0) {
        return 0;
    }

    if(!enter(device)) {
        return 0;
    }

    ssize_t retval = read_input(device, buffer, size, flags);

    leave(device);

    return retval;
}

static ssize_t tty_chr_write(const chr_device_t* restrict chr, void* restrict file_ctxdata, off_t offset, const void* restrict buffer,
    size_t size, unsigned flags)
{
    tty_device_t* device = tty_of(chr);

    if(!enter(device)) {
        return -EIO;
    }

    ssize_t retval = write_output(device, buffer, size, flags);

    leave(device);

    return retval;
}

static int tty_chr_ioctl(const chr_device_t* restrict chr, void* restrict file_ctxdata, uint32_t request, uintptr_t argument)
{
    tty_device_t* device = tty_of(chr);

    if(!enter(device)) {
        return -EIO;
    }

    int retval = handle_ioctl(device, request, argument);

    leave(device);

    return retval;
}

static void tty_chr_release(const chr_device_t* restrict chr)
{
    tty_device_t* device = tty_of(chr);

    pslab_free_virt(device->priv.flip.data);
    pslab_free_virt(device->priv.input.data);
    pslab_free_virt(device->priv.output.data);

    event_deinit(&device->priv.readable);
    event_deinit(&device->priv.writable);
    event_deinit(&device->priv.drained);
    mutex_deinit(&device->priv.write_lock);

    if(device->ops->release) {
        device->ops->release(device);
    }
}

static const chr_device_ops_t tty_chr_ops = {
    .open = &tty_chr_open,
    .close = &tty_chr_close,
    .read = &tty_chr_read,
    .write = &tty_chr_write,
    .ioctl = &tty_chr_ioctl,
    .release = &tty_chr_release,
};

int tty_device_add(tty_device_t* restrict device, dev_unit_t* restrict unit, unsigned index)
{
    tty_device_private_t* priv = &device->priv;

    memset(priv, 0, sizeof(*priv));

    priv->flip.data = pslab_alloc_virt();
    priv->input.data = pslab_alloc_virt();
    priv->output.data = pslab_alloc_virt();

    if(priv->flip.data == nullptr || priv->input.data == nullptr || priv->output.data == nullptr) {
        pslab_free_virt(priv->flip.data);
        pslab_free_virt(priv->input.data);
        pslab_free_virt(priv->output.data);
        return -ENOMEM;
    }

    priv->lock = SPINLOCK_INIT;
    mutex_init(&priv->write_lock);
    event_init(&priv->readable);
    event_init(&priv->writable);
    event_init(&priv->drained);
    set_default_termios(&priv->termios);

    device->chr.ops = &tty_chr_ops;
    device->chr.ctxdata = device;

    int retval = chr_device_add(&device->chr, unit, index);

    if(retval < 0) {
        pslab_free_virt(priv->flip.data);
        pslab_free_virt(priv->input.data);
        pslab_free_virt(priv->output.data);
        event_deinit(&priv->readable);
        event_deinit(&priv->writable);
        event_deinit(&priv->drained);
        mutex_deinit(&priv->write_lock);
        return retval;
    }

    return 0;
}

int tty_device_remove(tty_device_t* restrict device)
{
    atomic_store(&device->priv.removed, true, ATOMIC_SEQ_CST);

    int retval = chr_device_remove(&device->chr);

    if(retval < 0) {
        return retval;
    }

    // Under the lock, so a reader or writer about
    // to sleep either sees removed or gets woken
    bool intr_state = spinlock_acquire_intr(&device->priv.lock);
    event_trigger(&device->priv.readable, false);
    event_trigger(&device->priv.writable, false);
    event_trigger(&device->priv.drained, false);
    spinlock_release_intr(&device->priv.lock, intr_state);

    while(atomic_load(&device->priv.in_flight, ATOMIC_ACQUIRE)) {
        hart_relax();
    }

    ctty_drop_t drop = { 0 };

    process_group_lock_acquire();

    if(device->priv.session) {
        detach_locked(device, &drop);
    }

    process_group_lock_release();

    drop_detached(&drop);

    return 0;
}

void tty_receive(tty_device_t* restrict device, const void* restrict bytes, size_t count)
{
    const uint8_t* data = bytes;
    tty_ring_t* flip = &device->priv.flip;

    bool intr_state = spinlock_acquire_intr(&device->priv.lock);

    for(size_t i = 0; i < count; ++i) {
        if(tty_ring_room(flip) == 0) {
            device->priv.lost += count - i;
            break;
        }

        tty_ring_put(flip, data[i]);
    }

    spinlock_release_intr(&device->priv.lock, intr_state);

    if(!is_removed(device)) {
        queue_for_worker(device);
    }
}

size_t tty_transmit_take(tty_device_t* restrict device, void* restrict buffer, size_t size)
{
    uint8_t* out = buffer;
    tty_ring_t* output = &device->priv.output;
    size_t taken = 0;

    bool intr_state = spinlock_acquire_intr(&device->priv.lock);

    if(!device->priv.ldisc.output_stopped) {
        while(taken < size && tty_ring_used(output)) {
            out[taken] = tty_ring_take(output);
            taken += 1;
        }
    }

    if(taken) {
        (void)notify_output_locked(device);
    }

    spinlock_release_intr(&device->priv.lock, intr_state);

    return taken;
}

static void init_tty(void)
{
    event_init(&worker_wakeup);

    if(!thread_spawn_kernel(&tty_worker, nullptr)) {
        panic("tty: out of memory");
    }
}

initcall_bsp_dev(init_tty, INIT_FIRST);
