#include <vex/capability.h>
#include <vex/errno.h>
#include <vex/fcntl.h>
#include <vex/futex.h>
#include <vex/hwtree.h>
#include <vex/kplog.h>
#include <vex/mman.h>
#include <vex/serial.h>
#include <vex/signal.h>
#include <vex/stat.h>
#include <vex/syscall.h>
#include <vex/termios.h>
#include <vex/time.h>
#include <vex/utsname.h>

static long syscall1(long num, long a1)
{
    long ret;
    asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

static long syscall2(long num, long a1, long a2)
{
    long ret;
    asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

static long syscall3(long num, long a1, long a2, long a3)
{
    long ret;
    asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
    return ret;
}

static long syscall4(long num, long a1, long a2, long a3, long a4)
{
    long ret;
    register long r10 asm("r10") = a4;
    asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10) : "rcx", "r11", "memory");
    return ret;
}

static long syscall5(long num, long a1, long a2, long a3, long a4, long a5)
{
    long ret;
    register long r10 asm("r10") = a4;
    register long r8 asm("r8") = a5;
    asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8) : "rcx", "r11", "memory");
    return ret;
}

static long syscall6(long num, long a1, long a2, long a3, long a4, long a5, long a6)
{
    long ret;
    register long r10 asm("r10") = a4;
    register long r8 asm("r8") = a5;
    register long r9 asm("r9") = a6;
    asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return ret;
}

static unsigned long ustrlen(const char* s)
{
    unsigned long n = 0;
    while(*s++)
        n++;
    return n;
}

static void printnumber(long number)
{
    int i, sign;
    char buffer[128];

    for(i = 0; i < sizeof(buffer); buffer[i++] = 0) {
        /* empty */
    }

    if(number == 0) {
        syscall2(__k_SYS_TEST, (long)"0", 1);
        return;
    }

    if(number < 0) {
        number = -number;
        sign = 1;
    }
    else {
        sign = 0;
    }

    i = 0;

    while(number > 0) {
        buffer[i++] = '0' + (number % 10);
        number /= 10;
    }

    if(sign) {
        buffer[i++] = '-';
    }

    // reverse the string
    for(int j = 0; j < i / 2; j++) {
        char tmp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = tmp;
    }

    syscall2(__k_SYS_TEST, (long)buffer, i);
}

static void readback(const char* path)
{
    long fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)path, __k_O_RDONLY, 0);

    if(fd < 0) {
        printnumber(fd);
        return;
    }

    char buf[16];
    long n = syscall3(__k_SYS_READ, fd, (long)buf, sizeof(buf) - 1);

    if(n > 0) {
        syscall2(__k_SYS_TEST, (long)buf, n);
    }

    syscall1(__k_SYS_CLOSE, fd);
}

#define THREAD_STACK_SIZE 4096

static _Alignas(16) char thread_stack[THREAD_STACK_SIZE];
static long tls_block[2];

static volatile int worker_done;
static volatile long worker_tid;
static volatile long worker_arg;
static volatile long worker_tcb_word;

static volatile unsigned int join_word;

// The entry sees the stack as if it was called, the return address slot left empty
static long spawn(void (*entry)(long), long arg)
{
    long stack_top = (long)(thread_stack + THREAD_STACK_SIZE - 8);
    return syscall5(__k_SYS_THREAD_CREATE, (long)entry, stack_top, (long)tls_block, arg, 0);
}

// One joinable thread at a time, they all share thread_stack
static long spawn_joinable(void (*entry)(long), long arg)
{
    long stack_top = (long)(thread_stack + THREAD_STACK_SIZE - 8);
    join_word = 1;
    return syscall5(__k_SYS_THREAD_CREATE, (long)entry, stack_top, (long)tls_block, arg, (long)&join_word);
}

static void join(void)
{
    while(join_word) {
        syscall4(__k_SYS_FUTEX_WAIT, (long)&join_word, 1, 0, 0);
    }
}

static volatile long waited_child;
static volatile int waited_status;
static volatile long waited_result;

static void child_waiter(long arg)
{
    int wstatus = -1;
    waited_result = syscall2(__k_SYS_WAITPID, waited_child, (long)&wstatus);
    waited_status = wstatus;
    syscall1(__k_SYS_THREAD_EXIT, 0);
}

#define PINGPONG_ROUNDS 1000

static volatile unsigned int turn;
static volatile long pongs;
static volatile unsigned int never_changes;
static volatile long interrupted_result;

static void __attribute__((noreturn)) ponger(long arg)
{
    for(long i = 0; i < arg; ++i) {
        while(turn == 0) {
            syscall4(__k_SYS_FUTEX_WAIT, (long)&turn, 0, 0, 0);
        }

        pongs += 1;
        turn = 0;
        syscall3(__k_SYS_FUTEX_WAKE, (long)&turn, 1, 0);
    }

    syscall1(__k_SYS_THREAD_EXIT, 0);
    __builtin_unreachable();
}

// Takes SIGUSR1 even when spawned by a thread that blocks it
static void __attribute__((noreturn)) sleeper(long arg)
{
    __k_sigset_t unblocked = 1UL << (__k_SIGUSR1 - 1);
    syscall3(__k_SYS_SIGPROCMASK, __k_SIG_UNBLOCK, (long)&unblocked, 0);

    interrupted_result = syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 0, 0, 0);
    syscall1(__k_SYS_THREAD_EXIT, 0);
    __builtin_unreachable();
}

static _Alignas(16) char spinner_stack[THREAD_STACK_SIZE];
static volatile int spinner_running;
static char pid_text[24];

static void __attribute__((noreturn)) spinner(long arg)
{
    spinner_running = 1;

    for(;;) {
        // Busy on its own hart, never entering the kernel
    }
}

static void format_number(char* buffer, long number)
{
    char digits[24];
    int count = 0;

    do {
        digits[count++] = '0' + (number % 10);
        number /= 10;
    } while(number > 0);

    for(int i = 0; i < count; ++i) {
        buffer[i] = digits[count - i - 1];
    }

    buffer[count] = 0;
}

static long parse_number(const char* text)
{
    long number = 0;

    while(*text >= '0' && *text <= '9') {
        number = number * 10 + (*text++ - '0');
    }

    return number;
}

static int same_string(const char* a, const char* b)
{
    while(*a && *a == *b) {
        a++;
        b++;
    }

    return *a == *b;
}

// Execs from a thread that is not the main one, while one sibling sleeps in a futex
// and another spins in userspace; the exec has to take both away first
static void __attribute__((noreturn)) execer(long arg)
{
    format_number(pid_text, syscall1(__k_SYS_GETPID, 0));

    const char* argv[] = { "/utest.elf", "exec-child", pid_text, 0 };
    const char* envp[] = { 0 };

    syscall3(__k_SYS_EXEC, (long)"/utest.elf", (long)argv, (long)envp);
    syscall1(__k_SYS_EXIT, 99); // The exec failed
    __builtin_unreachable();
}

static int exec_child(char** argv)
{
    long pid = syscall1(__k_SYS_GETPID, 0);

    printnumber(pid == parse_number(argv[2]));       // should print 1, the process is the same
    printnumber(syscall1(__k_SYS_GETTID, 0) == pid); // should print 1, the surviving thread took the pid as its tid

    return 42;
}

static volatile long handled_tid;

#define PAGE_SIZE             4096
#define SHOOTDOWN_WRITERS     3
#define SHOOTDOWN_ROUNDS      10
#define SHOOTDOWN_WARMUP      100000
#define SHOOTDOWN_REUSE_PAGES 64
#define SHOOTDOWN_JOIN_TRIES  20

static _Alignas(16) char writer_stacks[SHOOTDOWN_WRITERS][THREAD_STACK_SIZE];
static long writer_tcb[SHOOTDOWN_WRITERS]; // A writer finds its index at %fs:0
static volatile unsigned int writer_join[SHOOTDOWN_WRITERS];
static volatile long writer_writes[SHOOTDOWN_WRITERS];
static volatile long writer_final[SHOOTDOWN_WRITERS];
static volatile long* volatile target_page;

// Counts every write that went through, after the write itself
static void __attribute__((noreturn)) writer(long arg)
{
    volatile long* page = target_page;

    for(;;) {
        page[arg] = writer_writes[arg];
        writer_writes[arg] += 1;
    }
}

static void on_fault(__k_int32_t sig)
{
    long index;
    asm volatile("movq %%fs:0, %0" : "=r"(index));

    writer_final[index] = writer_writes[index];
    syscall1(__k_SYS_THREAD_EXIT, 0);
}

static long shootdown_worst;
static long shootdown_corrupted;
static long shootdown_stuck;

// Once munmap or mprotect returns, a writer gets at most the one write it had
// in flight; any more went through a translation the shootdown should have killed
static void shootdown_round(int protect)
{
    // Pages on both sides keep the hole a single page, too small
    // for the next mapping to land on the address the writers use
    long guarded = syscall6(__k_SYS_MMAP, 0, 3 * PAGE_SIZE, __k_PROT_READ | __k_PROT_WRITE, __k_MAP_PRIVATE | __k_MAP_ANONYMOUS, -1, 0);
    long page = guarded + PAGE_SIZE;
    target_page = (volatile long*)page;

    for(long i = 0; i < SHOOTDOWN_WRITERS; ++i) {
        writer_writes[i] = 0;
        writer_final[i] = -1;
        writer_join[i] = 1;
        writer_tcb[i] = i;
        syscall5(__k_SYS_THREAD_CREATE, (long)&writer, (long)(writer_stacks[i] + THREAD_STACK_SIZE - 8), (long)&writer_tcb[i], i,
            (long)&writer_join[i]);
    }

    for(long i = 0; i < SHOOTDOWN_WRITERS; ++i) {
        while(writer_writes[i] < SHOOTDOWN_WARMUP) {
            // All writers busy on their own harts
        }
    }

    if(protect) {
        syscall3(__k_SYS_MPROTECT, page, PAGE_SIZE, __k_PROT_READ);
    }
    else {
        syscall2(__k_SYS_MUNMAP, page, PAGE_SIZE);
    }

    long snapshot[SHOOTDOWN_WRITERS];

    for(long i = 0; i < SHOOTDOWN_WRITERS; ++i) {
        snapshot[i] = writer_writes[i];
    }

    // The freed frame is likely among these; a writer that still reached it would leave its counter here
    long length = SHOOTDOWN_REUSE_PAGES * PAGE_SIZE;
    long* reuse = (long*)syscall6(__k_SYS_MMAP, 0, length, __k_PROT_READ | __k_PROT_WRITE, __k_MAP_PRIVATE | __k_MAP_ANONYMOUS, -1, 0);

    for(long i = 0; i < length / 8; ++i) {
        reuse[i] = -1;
    }

    struct __k_timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 100000000;

    for(long i = 0; i < SHOOTDOWN_WRITERS; ++i) {
        for(int tries = 0; writer_join[i] && tries < SHOOTDOWN_JOIN_TRIES; ++tries) {
            syscall4(__k_SYS_FUTEX_WAIT, (long)&writer_join[i], 1, (long)&timeout, 0);
        }

        if(writer_join[i]) {
            shootdown_stuck += 1;
            continue;
        }

        long extra = writer_final[i] - snapshot[i];

        if(extra > shootdown_worst) {
            shootdown_worst = extra;
        }
    }

    for(long i = 0; i < length / 8; ++i) {
        if(reuse[i] != -1) {
            shootdown_corrupted += 1;
        }
    }

    syscall2(__k_SYS_MUNMAP, (long)reuse, length);
    syscall2(__k_SYS_MUNMAP, guarded, 3 * PAGE_SIZE);
}

static void on_signal(__k_int32_t sig)
{
    handled_tid = syscall1(__k_SYS_GETTID, 0);
}

static void __attribute__((naked)) signal_restorer(void)
{
    asm volatile("movq %0, %%rax\n\tsyscall" ::"i"(__k_SYS_SIGRETURN));
}

static void __attribute__((noreturn)) worker(long arg)
{
    long tcb_word;
    asm volatile("movq %%fs:0, %0" : "=r"(tcb_word));

    worker_tid = syscall1(__k_SYS_GETTID, 0);
    worker_arg = arg;
    worker_tcb_word = tcb_word;
    worker_done = 1;

    syscall1(__k_SYS_THREAD_EXIT, 0);
    __builtin_unreachable();
}

static void __attribute__((noreturn)) finisher(long arg)
{
    // Long enough for the main thread to be gone
    for(int i = 0; i < 100; ++i) {
        syscall1(__k_SYS_YIELD, 0);
    }

    syscall1(__k_SYS_EXIT, arg);
    __builtin_unreachable();
}

#define LINE_RECEIVE_ERROR __k_IOCTL_WRITE(sizeof(__k_int32_t), 't', 0x01) /* The far end's own, in serial_test.c */
#define LINE_ERROR_BREAK   1
#define LINE_ERROR_PARITY  2
#define LINE_ERROR_OVERRUN 4
#define LINE_FILL_SIZE     3900 /* Leaves the 4 KiB input ring less room than IXOFF's margin */

static long tty_fd;
static long line_fd;
static long serial_fd;
static char line_fill[LINE_FILL_SIZE];

static int same_bytes(const char* left, const char* right, long length)
{
    long i;

    for(i = 0; i < length; ++i) {
        if(left[i] != right[i]) {
            return 0;
        }
    }

    return 1;
}

static void line_send(const char* text)
{
    syscall3(__k_SYS_WRITE, line_fd, (long)text, ustrlen(text));
}

static void line_error(int error, int byte)
{
    __k_int32_t value = (error << 8) | byte;
    syscall3(__k_SYS_IOCTL, line_fd, LINE_RECEIVE_ERROR, (long)&value);
}

/* Prints 1 when the next read gives exactly what is expected, and what came otherwise */
static void expect_bytes(long fd, const char* expected, long expected_length)
{
    char buffer[128];
    long length = syscall3(__k_SYS_READ, fd, (long)buffer, sizeof(buffer));

    if(length == expected_length && same_bytes(buffer, expected, length)) {
        printnumber(1);
    }
    else if(length > 0) {
        syscall2(__k_SYS_TEST, (long)buffer, length);
    }
    else {
        printnumber(length);
    }
}

static void expect_read(long fd, const char* expected)
{
    expect_bytes(fd, expected, ustrlen(expected));
}

/* What the terminal sends may come in pieces, as tty_worker echoes before the kernel prints a status line */
static void expect_line(const char* expected)
{
    char buffer[128];
    long expected_length = ustrlen(expected);
    long length = 0;
    long piece = 1;

    while(length < expected_length && piece > 0) {
        piece = syscall3(__k_SYS_READ, line_fd, (long)(buffer + length), sizeof(buffer) - length);

        if(piece > 0) {
            length += piece;
        }
    }

    if(length == expected_length && same_bytes(buffer, expected, length)) {
        printnumber(1);
    }
    else {
        syscall2(__k_SYS_TEST, (long)buffer, length);
    }
}

static void line_drain(void)
{
    char buffer[128];

    while(syscall3(__k_SYS_READ, line_fd, (long)buffer, sizeof(buffer)) > 0) {
        /* empty */
    }
}

static void tty_raw(int vmin, int vtime)
{
    struct __k_termios termios;

    syscall3(__k_SYS_IOCTL, tty_fd, __k_TCGETS, (long)&termios);
    termios.c_lflag &= ~(__k_ICANON | __k_ECHO);
    termios.c_cc[__k_VMIN] = vmin;
    termios.c_cc[__k_VTIME] = vtime;
    syscall3(__k_SYS_IOCTL, tty_fd, __k_TCSETS, (long)&termios);
}

static void tty_set_iflag(__k_uint32_t iflag)
{
    struct __k_termios termios;

    syscall3(__k_SYS_IOCTL, tty_fd, __k_TCGETS, (long)&termios);
    termios.c_iflag = iflag;
    syscall3(__k_SYS_IOCTL, tty_fd, __k_TCSETS, (long)&termios);
}

/* Before TIOCSCTTY, so that BRKINT's SIGINT has no foreground group to reach */
static void test_tty_line(void)
{
    long total = 0;
    long length = 1;
    long i;

    tty_raw(1, 0);

    /* PARMRK marks a parity error and a break, and doubles a \377 that came intact */
    tty_set_iflag(__k_INPCK | __k_PARMRK);
    line_error(LINE_ERROR_PARITY, 'x');
    expect_bytes(tty_fd, "\377\0x", 3); /* should print 1 */
    line_send("\377");
    expect_bytes(tty_fd, "\377\377", 2); /* should print 1 */
    line_error(LINE_ERROR_BREAK, 0);
    expect_bytes(tty_fd, "\377\0\0", 3); /* should print 1 */

    /* Without PARMRK the error reads as a NUL, IGNPAR drops it, and without INPCK the byte stays */
    tty_set_iflag(__k_INPCK);
    line_error(LINE_ERROR_PARITY, 'x');
    expect_bytes(tty_fd, "\0", 1); /* should print 1 */
    tty_set_iflag(__k_INPCK | __k_IGNPAR);
    line_error(LINE_ERROR_PARITY, 'x');
    line_send("y");
    expect_read(tty_fd, "y"); /* should print 1 */
    tty_set_iflag(0);
    line_error(LINE_ERROR_PARITY, 'z');
    expect_read(tty_fd, "z"); /* should print 1 */
    line_error(LINE_ERROR_OVERRUN, 'o');
    expect_read(tty_fd, "o"); /* should print 1 */

    /* BRKINT throws away what came before the break */
    tty_set_iflag(__k_BRKINT);
    line_send("abc");
    line_error(LINE_ERROR_BREAK, 0);
    line_drain();
    line_send("q");
    expect_read(tty_fd, "q"); /* should print 1 */

    /* IXOFF sends VSTOP as the input fills, and VSTART once a read emptied it */
    tty_set_iflag(__k_IXOFF);

    for(i = 0; i < LINE_FILL_SIZE; ++i) {
        line_fill[i] = 'a';
    }

    syscall3(__k_SYS_WRITE, line_fd, (long)line_fill, LINE_FILL_SIZE);
    expect_line("\x13"); /* should print 1 */

    while(total < LINE_FILL_SIZE && length > 0) {
        length = syscall3(__k_SYS_READ, tty_fd, (long)line_fill, LINE_FILL_SIZE);

        if(length > 0) {
            total += length;
        }
    }

    printnumber(total);  /* should print 3900 */
    expect_line("\x11"); /* should print 1 */
}

/* Answers the AT command an exchange sends, as a modem would */
static void __attribute__((noreturn)) modem_answerer(long arg)
{
    char buffer[16];
    long length = 0;
    long rounds = 0;
    long piece;

    while(length < 3 && rounds < 10) {
        piece = syscall3(__k_SYS_READ, line_fd, (long)(buffer + length), sizeof(buffer) - length);

        if(piece > 0) {
            length += piece;
        }

        rounds += 1;
    }

    if(length == 3 && same_bytes(buffer, "AT\r", 3)) {
        line_send("OK\r\n");
    }

    syscall1(__k_SYS_THREAD_EXIT, 0);
    __builtin_unreachable();
}

/* Leaves serial0 bound to a terminal, and serial_fd its controlling file */
static void test_serial(void)
{
    struct __k_serial_info info;
    struct __k_serial_line line;
    struct __k_serial_exchange exchange;
    struct __k_serial_binding binding = {};
    char answer[16];
    long viewer;

    viewer = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/serial0", __k_O_RDONLY, 0);
    printnumber(viewer >= 0); /* should print 1 */
    syscall3(__k_SYS_IOCTL, viewer, __k_SERIAL_GET_INFO, (long)&info);
    syscall2(__k_SYS_TEST, (long)info.driver, ustrlen(info.driver)); /* should print serial_test */
    syscall3(__k_SYS_IOCTL, viewer, __k_SERIAL_GET_LINE, (long)&line);
    printnumber(line.speed); /* should print 38400 */

    /* Only a file that keeps everyone else out changes the port */
    line.speed = 115200;
    printnumber(syscall3(__k_SYS_IOCTL, viewer, __k_SERIAL_SET_LINE, (long)&line));                     /* should print -263 (EPERM) */
    printnumber(syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/serial0", __k_O_RDWR | __k_O_EXCL, 0)); /* should print -210 (EBUSY) */
    syscall1(__k_SYS_CLOSE, viewer);

    serial_fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/serial0", __k_O_RDWR | __k_O_EXCL, 0);
    printnumber(serial_fd >= 0);                                                                   /* should print 1 */
    printnumber(syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/serial0", __k_O_RDONLY, 0));    /* should print -210 (EBUSY) */
    printnumber(syscall3(__k_SYS_IOCTL, serial_fd, __k_SERIAL_SET_LINE, (long)&line));             /* should print 0 */

    /* An unbound port talks to what is on the line, here a modem that answers OK */
    exchange.send = (long)"AT\r";
    exchange.send_size = 3;
    exchange.receive = (long)answer;
    exchange.receive_size = sizeof(answer);
    exchange.timeout_msec = 2000;
    exchange.terminator = '\n';
    spawn_joinable(&modem_answerer, 0);
    printnumber(syscall3(__k_SYS_IOCTL, serial_fd, __k_SERIAL_EXCHANGE, (long)&exchange)); /* should print 0 */
    join();
    printnumber(exchange.receive_size == 4 && same_bytes(answer, "OK\r\n", 4)); /* should print 1 */

    binding.consumer[0] = 't';
    binding.consumer[1] = 't';
    binding.consumer[2] = 'y';
    binding.index = __k_SERIAL_ANY_INDEX;
    printnumber(syscall3(__k_SYS_IOCTL, serial_fd, __k_SERIAL_BIND, (long)&binding));              /* should print 0 */
    syscall2(__k_SYS_TEST, (long)binding.object, ustrlen(binding.object));                        /* should print tty0 */
    printnumber(syscall3(__k_SYS_IOCTL, serial_fd, __k_SERIAL_BIND, (long)&binding));              /* should print -210 (EBUSY) */
    printnumber(syscall3(__k_SYS_IOCTL, serial_fd, __k_SERIAL_EXCHANGE, (long)&exchange));         /* should print -210 (EBUSY) */
}

static void test_tty(void)
{
    struct __k_termios saved;
    char buffer[64];
    __k_int32_t value = 0;
    __k_pid_t pgid;
    struct __k_winsize winsize = { 24, 80, 0, 0 };
    struct __k_winsize winsize_back;
    long child;
    int wstatus;

    syscall3(__k_SYS_MKDIRAT, __k_AT_FDCWD, (long)"/dev", 0755);
    printnumber(syscall6(__k_SYS_MOUNT, 0, (long)"/dev", (long)"devfs", 0, 0, 0)); /* should print 0 */

    line_fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/serialtest", __k_O_RDWR, 0);
    test_serial();
    tty_fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/tty0", __k_O_RDWR, 0);
    printnumber(tty_fd >= 0 && line_fd >= 0); /* should print 1 */

    syscall3(__k_SYS_IOCTL, tty_fd, __k_TCGETS, (long)&saved);
    printnumber(saved.c_cc[__k_VMIN] == 1 && (saved.c_lflag & __k_ICANON)); /* should print 1 */

    /* DEL erases with ECHOE, and ONLCR echoes the newline as \r\n */
    line_send("ab\x7f"
              "c\n");
    expect_line("ab\b \bc\r\n"); /* should print 1 */
    expect_read(tty_fd, "ac\n"); /* should print 1 */

    syscall3(__k_SYS_WRITE, tty_fd, (long)"x\ny", 3);
    expect_line("x\r\ny"); /* should print 1 */

    /* ^U kills the line and ^W the last word */
    line_send("abc\x15"
              "d\n");
    line_drain();
    expect_read(tty_fd, "d\n"); /* should print 1 */
    line_send("foo bar\x17"
              "x\n");
    line_drain();
    expect_read(tty_fd, "foo x\n"); /* should print 1 */

    /* ^D on a line hands it out, and on an empty one reads as end of file */
    line_send("ab\x04");
    line_drain();
    expect_read(tty_fd, "ab"); /* should print 1 */
    line_send("\x04");
    line_drain();
    printnumber(syscall3(__k_SYS_READ, tty_fd, (long)buffer, sizeof(buffer))); /* should print 0 */

    /* A line is readable once it ends */
    line_send("hi\n");
    line_drain();
    syscall3(__k_SYS_IOCTL, tty_fd, __k_FIONREAD, (long)&value);
    printnumber(value);          /* should print 3 */
    expect_read(tty_fd, "hi\n"); /* should print 1 */

    /* VMIN 0 and VTIME 2 give up after 0.2 s, VMIN 3 waits for three bytes */
    tty_raw(0, 2);
    printnumber(syscall3(__k_SYS_READ, tty_fd, (long)buffer, sizeof(buffer))); /* should print 0 */
    tty_raw(3, 0);
    line_send("xyz");
    expect_read(tty_fd, "xyz"); /* should print 1 */

    syscall3(__k_SYS_FCNTL, tty_fd, __k_F_SETFL, __k_O_NONBLOCK);
    printnumber(syscall3(__k_SYS_READ, tty_fd, (long)buffer, sizeof(buffer))); /* should print -206 (EAGAIN) */
    syscall3(__k_SYS_FCNTL, tty_fd, __k_F_SETFL, 0);

    test_tty_line();
    syscall3(__k_SYS_IOCTL, tty_fd, __k_TCSETS, (long)&saved);

    syscall3(__k_SYS_IOCTL, tty_fd, __k_TIOCSWINSZ, (long)&winsize);
    syscall3(__k_SYS_IOCTL, tty_fd, __k_TIOCGWINSZ, (long)&winsize_back);
    printnumber(winsize_back.ws_row == 24 && winsize_back.ws_col == 80); /* should print 1 */

    /* Init leads session 1, so the terminal can become its controlling one */
    printnumber(syscall3(__k_SYS_IOCTL, tty_fd, __k_TIOCGPGRP, (long)&pgid)); /* should print -259 (ENOTTY) */
    value = 0;
    printnumber(syscall3(__k_SYS_IOCTL, tty_fd, __k_TIOCSCTTY, (long)&value)); /* should print 0 */
    syscall3(__k_SYS_IOCTL, tty_fd, __k_TIOCGPGRP, (long)&pgid);
    printnumber(pgid); /* should print 1 */
    syscall3(__k_SYS_IOCTL, tty_fd, __k_TIOCGSID, (long)&pgid);
    printnumber(pgid); /* should print 1 */

    /* ^C goes to the foreground group, a job in a group of its own */
    child = syscall1(__k_SYS_FORK, 0);

    if(child == 0) {
        for(;;) {
            syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 0, 0, 0);
        }
    }

    syscall2(__k_SYS_SETPGID, child, child);
    pgid = child;
    printnumber(syscall3(__k_SYS_IOCTL, tty_fd, __k_TIOCSPGRP, (long)&pgid)); /* should print 0 */
    line_send("\x03");
    wstatus = -1;
    syscall2(__k_SYS_WAITPID, child, (long)&wstatus);
    printnumber(wstatus); /* should print 2 (SIGINT) */
    expect_line("^C");    /* should print 1 */

    pgid = 1;
    printnumber(syscall3(__k_SYS_IOCTL, tty_fd, __k_TIOCSPGRP, (long)&pgid)); /* should print 0 */

    /* ^T has the kernel print the foreground group, init ignores the SIGINFO */
    line_send("\x14");
    expect_line("^Tprocess group 1: 1 (running)\r\n"); /* should print 1 */

    printnumber(syscall3(__k_SYS_IOCTL, tty_fd, __k_TIOCNOTTY, 0));           /* should print 0 */
    printnumber(syscall3(__k_SYS_IOCTL, tty_fd, __k_TIOCGPGRP, (long)&pgid)); /* should print -259 (ENOTTY) */

    /* The terminal goes away with the binding, but not while it is open */
    printnumber(syscall3(__k_SYS_IOCTL, serial_fd, __k_SERIAL_UNBIND, 0)); /* should print -210 (EBUSY) */
    syscall1(__k_SYS_CLOSE, tty_fd);
    printnumber(syscall3(__k_SYS_IOCTL, serial_fd, __k_SERIAL_UNBIND, 0));                   /* should print 0 */
    printnumber(syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/tty0", __k_O_RDWR, 0)); /* should print -243 (ENOENT) */
    syscall1(__k_SYS_CLOSE, serial_fd);
    syscall1(__k_SYS_CLOSE, line_fd);
    syscall1(__k_SYS_CLOSE, line_fd);
}

#define KPLOG_POKE "kplog poke"

/* Sleeps so the reader blocks first, then logs a line through sys_test */
static void __attribute__((noreturn)) kplog_poker(long arg)
{
    struct __k_timespec delay;
    delay.tv_sec = 0;
    delay.tv_nsec = 100000000;
    syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 0, (long)&delay, 0);
    syscall2(__k_SYS_TEST, (long)KPLOG_POKE, ustrlen(KPLOG_POKE));
    syscall1(__k_SYS_THREAD_EXIT, 0);
    __builtin_unreachable();
}

/* Records come in whole, aligned to __k_FEED_ALIGN; a read may give several */
static unsigned long long kplog_buffer[4096 / sizeof(unsigned long long)];
static long kplog_offset;
static long kplog_length;

/* Drops what kplog_next has read ahead, as after a seek */
static void kplog_forget(void)
{
    kplog_offset = 0;
    kplog_length = 0;
}

/* The next record from the file, read when the buffer runs out; null with the read's result in *result */
static const struct __k_kplog_record* kplog_next(long fd, long* result)
{
    const struct __k_kplog_record* record;

    if(kplog_offset == kplog_length) {
        kplog_forget();
        *result = syscall3(__k_SYS_READ, fd, (long)kplog_buffer, sizeof(kplog_buffer));

        if(*result <= 0) {
            return 0;
        }

        kplog_length = *result;
    }

    record = (const struct __k_kplog_record*)((const char*)kplog_buffer + kplog_offset);
    kplog_offset += record->feed.size;
    *result = record->feed.size;

    return record;
}

/* Whether the record's text ends with what sys_test logs for data */
static int kplog_has_data(const struct __k_kplog_record* record, const char* data)
{
    long length = ustrlen(data);
    long size = record->text_size;

    if(record->feed.type != __k_FEED_TYPE_KPLOG || size < length + 1) {
        return 0;
    }

    return same_bytes(record->text + size - length - 1, data, length) && record->text[size - 1] == '"';
}

static void test_kplog(void)
{
    const struct __k_kplog_record* record;
    struct __k_kplog_info info;
    char banner[__k_KPLOG_TEXT_MAX + 1];
    char small[8];
    long fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/kplog", __k_O_RDONLY, 0);
    long length, nonblock, seek_bad, seek_end, set_level;
    __k_uint64_t seq;
    __k_int32_t level = -1;
    long reads = 0;
    long i;
    int found = 0;

    printnumber(fd >= 0); /* should print 1 */

    record = kplog_next(fd, &length);
    printnumber(record && record->feed.size % __k_FEED_ALIGN == 0 && record->feed.type == __k_FEED_TYPE_KPLOG); /* should print 1 */
    printnumber((long)record->feed.seq);                                                                        /* should print 0 */

    for(i = 0; i < (long)record->text_size; ++i) {
        banner[i] = record->text[i];
    }

    banner[i] = 0;
    syscall2(__k_SYS_TEST, (long)banner, i); /* should print the banner */

    kplog_forget();
    printnumber(syscall3(__k_SYS_READ, fd, (long)small, sizeof(small))); /* should print -226 (EINVAL) */

    syscall3(__k_SYS_IOCTL, fd, __k_KPLOG_GET_INFO, (long)&info);
    printnumber(info.seq_first <= info.seq_next && info.seq_next > 1); /* should print 1 */

    seq = info.seq_next - 1;
    printnumber(syscall3(__k_SYS_IOCTL, fd, __k_KPLOG_SEEK_SEQ, (long)&seq)); /* should print 0 */
    kplog_forget();
    record = kplog_next(fd, &length);
    printnumber(record && record->feed.seq == seq); /* should print 1 */

    printnumber(syscall3(__k_SYS_IOCTL, fd, __k_KPLOG_GET_CONSOLE_LEVEL, (long)&level)); /* should print 0 */
    printnumber(level >= __k_KP_EMERG && level <= __k_KP_VERBOSE);                       /* should print 1 */

    /* Nothing logs between the seeks and the reads, so the results wait until after them */
    set_level = syscall3(__k_SYS_IOCTL, fd, __k_KPLOG_SET_CONSOLE_LEVEL, (long)&level);
    seq = info.seq_next + 1000;
    seek_bad = syscall3(__k_SYS_IOCTL, fd, __k_KPLOG_SEEK_SEQ, (long)&seq);
    seek_end = syscall3(__k_SYS_IOCTL, fd, __k_KPLOG_SEEK_END, 0);
    kplog_forget();
    syscall3(__k_SYS_FCNTL, fd, __k_F_SETFL, __k_O_NONBLOCK);
    kplog_next(fd, &nonblock);
    syscall3(__k_SYS_FCNTL, fd, __k_F_SETFL, 0);
    spawn_joinable(&kplog_poker, 0);

    /* The kernel may log something else first */
    while(!found && reads < 16) {
        record = kplog_next(fd, &length);
        found = record && kplog_has_data(record, KPLOG_POKE);
        reads += 1;
    }

    join();

    printnumber(set_level); /* should print -263 (EPERM) */
    printnumber(seek_bad);  /* should print -226 (EINVAL) */
    printnumber(seek_end);  /* should print 0 */
    printnumber(nonblock);  /* should print -206 (EAGAIN) */
    printnumber(found);     /* should print 1 */

    syscall1(__k_SYS_CLOSE, fd);
}

/* Whether boot logged from memory management and from the device tree, each with its subsystem */
static void test_kplog_subsystems(void)
{
    const struct __k_kplog_record* record;
    long fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/kplog", __k_O_RDONLY | __k_O_NONBLOCK, 0);
    long result;
    int seen_mm = 0;
    int seen_device = 0;

    kplog_forget();

    while((record = kplog_next(fd, &result))) {
        seen_mm |= record->subsystem == __k_KP_MM;
        seen_device |= record->subsystem == __k_KP_DEVICE;
    }

    printnumber(seen_mm && seen_device); /* should print 1 */

    syscall1(__k_SYS_CLOSE, fd);
}

/* Reads records without blocking until a userspace one has exactly the level and the text; 0 when none does */
static int kplog_find(long fd, long level, const char* text)
{
    const struct __k_kplog_record* record;
    long length = ustrlen(text);
    long result;

    while((record = kplog_next(fd, &result))) {
        if(record->subsystem == __k_KP_USER && record->level == level && (long)record->text_size == length
            && same_bytes(record->text, text, length)) {
            return 1;
        }
    }

    return 0;
}

static void test_kplog_write(void)
{
    char too_long[600];
    int wstatus = -1;
    long child;
    long reader = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/kplog", __k_O_RDONLY | __k_O_NONBLOCK, 0);
    long writer = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/kplog", __k_O_WRONLY, 0);
    long i;

    for(i = 0; i < (long)sizeof(too_long); ++i) {
        too_long[i] = 'x';
    }

    printnumber(reader >= 0 && writer >= 0); /* should print 1 */
    syscall3(__k_SYS_IOCTL, reader, __k_KPLOG_SEEK_END, 0);
    kplog_forget();

    printnumber(syscall3(__k_SYS_WRITE, writer, (long)"user line\r\n", 11));        /* should print 11 */
    printnumber(syscall3(__k_SYS_WRITE, writer, (long)"<4>warned\n", 10));          /* should print 10 */
    printnumber(syscall3(__k_SYS_WRITE, writer, (long)"<14>syslog user", 15));      /* should print 15 */
    printnumber(syscall3(__k_SYS_WRITE, writer, (long)"tab\there\\", 9));           /* should print 9 */
    printnumber(syscall3(__k_SYS_WRITE, writer, (long)"<2>critical", 11));          /* should print -226 (EINVAL) */
    printnumber(syscall3(__k_SYS_WRITE, writer, (long)"two\nlines", 9));            /* should print -226 (EINVAL) */
    printnumber(syscall3(__k_SYS_WRITE, writer, (long)"nul\0byte", 8));             /* should print -226 (EINVAL) */
    printnumber(syscall3(__k_SYS_WRITE, writer, (long)too_long, sizeof(too_long))); /* should print -226 (EINVAL) */
    printnumber(syscall3(__k_SYS_WRITE, reader, (long)"read only", 9));             /* should print -208 (EBADF) */

    /* A <N> prefix sets only the level, and the text comes as it was written */
    printnumber(kplog_find(reader, __k_KP_NOTICE, "user line"));   /* should print 1 */
    printnumber(kplog_find(reader, __k_KP_WARNING, "warned"));     /* should print 1 */
    printnumber(kplog_find(reader, __k_KP_INFORM, "syslog user")); /* should print 1 */
    printnumber(kplog_find(reader, __k_KP_NOTICE, "tab\there\\")); /* should print 1 */

    /* A child without CAP_SYSLOG still reads, and its open for writing exits with the error */
    child = syscall1(__k_SYS_FORK, 0);

    if(child == 0) {
        struct __k_caps caps;
        long denied, allowed;

        syscall1(__k_SYS_CAPGET, (long)&caps);
        caps.effective &= ~__k_CAP_TO_MASK(__k_CAP_SYSLOG);
        syscall1(__k_SYS_CAPSET, (long)&caps);

        denied = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/kplog", __k_O_WRONLY, 0);
        allowed = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/kplog", __k_O_RDONLY, 0);
        syscall1(__k_SYS_EXIT, allowed >= 0 ? -denied : 0);
    }

    syscall2(__k_SYS_WAITPID, child, (long)&wstatus);
    printnumber(wstatus >> 8); /* should print 263 & 255 = 7 (EPERM) */

    syscall1(__k_SYS_CLOSE, writer);
    syscall1(__k_SYS_CLOSE, reader);
}

/* Records come in whole, aligned to __k_FEED_ALIGN; a read may give several */
static unsigned long long hwtree_buffer[4096 / sizeof(unsigned long long)];
static long hwtree_offset;
static long hwtree_length;

/* Drops what hwtree_next has read ahead, as after a resnapshot */
static void hwtree_forget(void)
{
    hwtree_offset = 0;
    hwtree_length = 0;
}

/* The next record from the file, read when the buffer runs out; null with the read's result in *result */
static const struct __k_hwtree_record* hwtree_next(long fd, long* result)
{
    const struct __k_hwtree_record* record;

    if(hwtree_offset == hwtree_length) {
        hwtree_forget();
        *result = syscall3(__k_SYS_READ, fd, (long)hwtree_buffer, sizeof(hwtree_buffer));

        if(*result <= 0) {
            return 0;
        }

        hwtree_length = *result;
    }

    record = (const struct __k_hwtree_record*)((const char*)hwtree_buffer + hwtree_offset);
    hwtree_offset += record->feed.size;
    *result = record->feed.size;

    return record;
}

/* The value of the record's field, or null */
static const char* hwtree_field(const struct __k_hwtree_record* record, const char* key)
{
    long length = ustrlen(key);
    long offset = 0;

    while(offset < (long)record->fields_size) {
        const char* field = record->fields + offset;

        if(same_bytes(field, key, length) && field[length] == '=') {
            return field + length + 1;
        }

        offset += ustrlen(field) + 1;
    }

    return 0;
}

static void test_hwtree(void)
{
    const struct __k_hwtree_record* record;
    const char* kind;
    char small[8];
    long reader = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/hwtree", __k_O_RDONLY | __k_O_NONBLOCK, 0);
    long writer = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/hwtree", __k_O_WRONLY, 0);
    long result;
    __k_uint64_t id = 0;
    __k_uint64_t missing = ~(__k_uint64_t)0;
    int began, ended = 0, parents_first = 1, seen_block = 0;
    __k_uint64_t seen[256];
    long seen_count = 0;
    long i;

    printnumber(reader >= 0 && writer >= 0); /* should print 1 */

    hwtree_forget();
    record = hwtree_next(reader, &result);
    began = record && record->feed.type == __k_FEED_TYPE_HWTREE_SNAPSHOT_BEGIN;

    while(!ended && (record = hwtree_next(reader, &result))) {
        if(record->feed.type == __k_FEED_TYPE_HWTREE_ADD) {
            int known = record->parent == 0;

            for(i = 0; i < seen_count; ++i) {
                known |= seen[i] == record->parent;
            }

            parents_first &= known;

            if(seen_count < 256) {
                seen[seen_count] = record->id;
                seen_count += 1;
            }

            if(id == 0) {
                id = record->id;
            }
        }

        kind = hwtree_field(record, "kind");
        seen_block |= record->feed.type == __k_FEED_TYPE_HWTREE_INTERFACE && kind && same_bytes(kind, "block", 6);
        ended = record->feed.type == __k_FEED_TYPE_HWTREE_SNAPSHOT_END;
    }

    printnumber(began && ended);                  /* should print 1 */
    printnumber(seen_count > 0 && parents_first); /* should print 1 */
    printnumber(seen_block);                      /* should print 1 */

    hwtree_next(reader, &result);
    printnumber(result); /* should print -206 (EAGAIN) */

    printnumber(syscall3(__k_SYS_IOCTL, reader, __k_HWTREE_RESNAPSHOT, 0));  /* should print 0 */
    printnumber(syscall3(__k_SYS_READ, reader, (long)small, sizeof(small))); /* should print -226 (EINVAL) */
    hwtree_forget();
    record = hwtree_next(reader, &result);
    printnumber(record && record->feed.type == __k_FEED_TYPE_HWTREE_SNAPSHOT_BEGIN); /* should print 1 */

    printnumber(syscall3(__k_SYS_IOCTL, reader, __k_HWTREE_RESCAN, (long)&id));      /* should print -263 (EPERM) */
    printnumber(syscall3(__k_SYS_IOCTL, writer, __k_HWTREE_RESCAN, (long)&missing)); /* should print -243 (ENOENT) */
    printnumber(syscall3(__k_SYS_IOCTL, writer, __k_HWTREE_RESCAN, (long)&id));      /* should print 0 */
    printnumber(syscall3(__k_SYS_READ, writer, (long)small, sizeof(small)));         /* should print -208 (EBADF) */

    syscall1(__k_SYS_CLOSE, writer);
    syscall1(__k_SYS_CLOSE, reader);
}

/* A console on the first 16550 port, for trying the driver by hand: Enter within
 * CONSOLE_WAIT_DECISECONDS of the prompt opens it, and without one utest goes on */
#define CONSOLE_WAIT_DECISECONDS 50
#define CONSOLE_PORTS_MAX        16
#define CONSOLE_EOF              0x04 /* Ctrl-D, which leaves raw mode */

static long console_fd;

static void console_write(const char* text)
{
    syscall3(__k_SYS_WRITE, console_fd, (long)text, ustrlen(text));
}

static void console_number(long number)
{
    char buffer[24];

    if(number < 0) {
        console_write("-");
        number = -number;
    }

    format_number(buffer, number);
    console_write(buffer);
    console_write("\n");
}

static void console_mode(int canonical, int vmin, int vtime)
{
    struct __k_termios termios;

    syscall3(__k_SYS_IOCTL, console_fd, __k_TCGETS, (long)&termios);

    if(canonical) {
        termios.c_lflag |= __k_ICANON | __k_ECHO;
    }
    else {
        termios.c_lflag &= ~(__k_ICANON | __k_ECHO);
    }

    termios.c_cc[__k_VMIN] = vmin;
    termios.c_cc[__k_VTIME] = vtime;
    syscall3(__k_SYS_IOCTL, console_fd, __k_TCSETS, (long)&termios);
}

static int has_prefix(const char* text, const char* prefix)
{
    while(*prefix && *text == *prefix) {
        text++;
        prefix++;
    }

    return *prefix == 0;
}

static void append_string(char* buffer, const char* text)
{
    buffer += ustrlen(buffer);

    while(*text) {
        *buffer++ = *text++;
    }

    *buffer = 0;
}

static void console_raw(void)
{
    static const char hex[] = "0123456789abcdef";
    char byte;
    char text[4];

    console_write("raw: bytes in hex, Ctrl-D to leave\n");
    console_mode(0, 1, 0);

    while(syscall3(__k_SYS_READ, console_fd, (long)&byte, 1) == 1 && byte != CONSOLE_EOF) {
        text[0] = hex[(byte >> 4) & 0x0F];
        text[1] = hex[byte & 0x0F];
        text[2] = ' ';
        text[3] = 0;
        console_write(text);
    }

    console_mode(1, 1, 0);
    console_write("\n");
}

static void console_speed(const char* text)
{
    struct __k_termios termios;

    syscall3(__k_SYS_IOCTL, console_fd, __k_TCGETS, (long)&termios);
    termios.c_ispeed = parse_number(text);
    termios.c_ospeed = termios.c_ispeed;
    console_number(syscall3(__k_SYS_IOCTL, console_fd, __k_TCSETSW, (long)&termios));
}

/* Returns 0 for the line that closes the console */
static int console_command(const char* line)
{
    __k_int32_t msec = 0;

    if(same_string(line, "quit")) {
        return 0;
    }

    if(has_prefix(line, "echo ")) {
        console_write(line + 5);
        console_write("\n");
    }
    else if(same_string(line, "raw")) {
        console_raw();
    }
    else if(has_prefix(line, "speed ")) {
        console_speed(line + 6);
    }
    else if(same_string(line, "break")) {
        console_number(syscall3(__k_SYS_IOCTL, console_fd, __k_TCSENDBREAK, (long)&msec));
    }
    else if(line[0]) {
        console_write("commands: echo <text>, raw, speed <bps>, break, quit\n");
    }

    return 1;
}

/* Binds a terminal to the first 16550 port, and opens it */
static long console_open(void)
{
    struct __k_serial_info info;
    struct __k_serial_binding binding;
    char path[48] = "/dev/serial";
    long port;

    for(int i = 0; i < CONSOLE_PORTS_MAX; ++i) {
        format_number(path + 11, i);
        port = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)path, __k_O_RDWR | __k_O_EXCL, 0);

        if(port < 0) {
            continue;
        }

        syscall3(__k_SYS_IOCTL, port, __k_SERIAL_GET_INFO, (long)&info);

        if(!same_string(info.driver, "uart16550")) {
            syscall1(__k_SYS_CLOSE, port);
            continue;
        }

        binding = (struct __k_serial_binding) {};
        binding.consumer[0] = 't';
        binding.consumer[1] = 't';
        binding.consumer[2] = 'y';
        binding.index = __k_SERIAL_ANY_INDEX;

        long retval = syscall3(__k_SYS_IOCTL, port, __k_SERIAL_BIND, (long)&binding);
        syscall1(__k_SYS_CLOSE, port);

        if(retval < 0) {
            return retval;
        }

        path[5] = 0;
        append_string(path, binding.object);

        return syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)path, __k_O_RDWR, 0);
    }

    return -__k_ENOENT;
}

/* Reads a line without its newline, cutting what doesn't fit; returns -1 at the end of input */
static long console_read_line(char* line, long size)
{
    char rest[64];
    char last = 0;
    long length = syscall3(__k_SYS_READ, console_fd, (long)line, size - 1);

    if(length <= 0) {
        return -1;
    }

    if(line[length - 1] == '\n') {
        line[length - 1] = 0;
        return length - 1;
    }

    line[length] = 0;

    /* A canonical read stops at the newline, so the rest of a long line waits for the next reads */
    while(last != '\n') {
        long piece = syscall3(__k_SYS_READ, console_fd, (long)rest, sizeof(rest));

        if(piece <= 0) {
            break;
        }

        last = rest[piece - 1];
    }

    return length;
}

static void serial_console(void)
{
    char line[128];
    long length;

    console_fd = console_open();

    if(console_fd < 0) {
        return;
    }

    console_write("utest: press Enter for a console\n");
    console_mode(0, 0, CONSOLE_WAIT_DECISECONDS);

    if(syscall3(__k_SYS_READ, console_fd, (long)line, sizeof(line)) <= 0) {
        console_write("utest: no console\n");
        syscall1(__k_SYS_CLOSE, console_fd);
        return;
    }

    console_mode(1, 1, 0);

    do {
        console_write("utest> ");
        length = console_read_line(line, sizeof(line));

        if(length < 0) {
            break;
        }
    } while(console_command(line));

    console_write("utest: bye\n");
    syscall1(__k_SYS_CLOSE, console_fd);
}

int main(int argc, char** argv)
{
    if(argc > 2 && same_string(argv[1], "exec-child")) {
        return exec_child(argv);
    }

    struct __k_utsname name;
    syscall1(__k_SYS_UNAME, (long)&name);

    syscall2(__k_SYS_TEST, (long)"Hello, world!", 13);

    long number = 1234567890;
    printnumber(number);

    number = syscall3(__k_SYS_MKDIRAT, __k_AT_FDCWD, (long)"/test/subdir", 0755); // should fail because /test doesn't exist yet

    printnumber(number); // should print a negative error code (likely -ENOENT which is -243)

    // create /test directory
    syscall3(__k_SYS_MKDIRAT, __k_AT_FDCWD, (long)"/test", 0755);

    // open /test/filename.txt for writing, create if not exists
    long fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/test/filename.txt", __k_O_WRONLY | __k_O_CREAT, 0644);

    if(fd >= 0) {
        static const char* newline = "\n\n";
        static const char* msg = "sus amogus\n";

        syscall3(__k_SYS_WRITE, fd, (long)name.sysname, ustrlen(name.sysname));
        syscall3(__k_SYS_WRITE, fd, (long)newline, 2);

        syscall3(__k_SYS_WRITE, fd, (long)name.nodename, ustrlen(name.nodename));
        syscall3(__k_SYS_WRITE, fd, (long)newline, 2);

        syscall3(__k_SYS_WRITE, fd, (long)name.release, ustrlen(name.release));
        syscall3(__k_SYS_WRITE, fd, (long)newline, 2);

        syscall3(__k_SYS_WRITE, fd, (long)name.version, ustrlen(name.version));
        syscall3(__k_SYS_WRITE, fd, (long)newline, 2);

        syscall3(__k_SYS_WRITE, fd, (long)name.machine, ustrlen(name.machine));
        syscall3(__k_SYS_WRITE, fd, (long)newline, 2);

        syscall3(__k_SYS_WRITE, fd, (long)msg, ustrlen(msg));

        syscall1(__k_SYS_CLOSE, fd);
    }

    // read it back through a path relative to the working directory
    syscall1(__k_SYS_CHDIR, (long)"/test");
    fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"filename.txt", __k_O_RDONLY, 0);

    if(fd >= 0) {
        char buf[16];
        long n = syscall3(__k_SYS_READ, fd, (long)buf, sizeof(buf) - 1);

        if(n > 0) {
            syscall2(__k_SYS_TEST, (long)buf, n);
        }

        // F_SETFL changes O_APPEND and O_NONBLOCK only, the access mode stays
        printnumber(syscall3(__k_SYS_FCNTL, fd, __k_F_GETFL, 0) == __k_O_RDONLY);                    // should print 1
        printnumber(syscall3(__k_SYS_FCNTL, fd, __k_F_SETFL, __k_O_NONBLOCK | __k_O_WRONLY));        // should print 0
        printnumber(syscall3(__k_SYS_FCNTL, fd, __k_F_GETFL, 0) == (__k_O_RDONLY | __k_O_NONBLOCK)); // should print 1
        printnumber(syscall3(__k_SYS_FCNTL, fd, 99, 0));                                             // should print -226 (EINVAL)

        syscall1(__k_SYS_CLOSE, fd);
    }

    // unlinkat refuses a directory without AT_REMOVEDIR
    syscall3(__k_SYS_MKDIRAT, __k_AT_FDCWD, (long)"subdir", 0755);
    printnumber(syscall3(__k_SYS_UNLINKAT, __k_AT_FDCWD, (long)"subdir", 0));                // should print -229 (EISDIR)
    printnumber(syscall3(__k_SYS_UNLINKAT, __k_AT_FDCWD, (long)"subdir", __k_AT_REMOVEDIR)); // should print 0

    // renameat moves a file into another directory
    syscall3(__k_SYS_MKDIRAT, __k_AT_FDCWD, (long)"moved", 0755);
    printnumber(syscall4(__k_SYS_RENAMEAT, __k_AT_FDCWD, (long)"filename.txt", __k_AT_FDCWD, (long)"moved/renamed.txt")); // should print 0
    printnumber(syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"filename.txt", __k_O_RDONLY, 0)); // should print -243 (ENOENT)
    readback("moved/renamed.txt");                                                              // should print the uname dump

    // renameat replaces an existing file, and refuses to move a directory into itself
    fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"other.txt", __k_O_WRONLY | __k_O_CREAT, 0644);
    syscall3(__k_SYS_WRITE, fd, (long)"replaced\n", 9);
    syscall1(__k_SYS_CLOSE, fd);
    printnumber(syscall4(__k_SYS_RENAMEAT, __k_AT_FDCWD, (long)"other.txt", __k_AT_FDCWD, (long)"moved/renamed.txt")); // should print 0
    readback("moved/renamed.txt");                                                                           // should print "replaced"
    printnumber(syscall4(__k_SYS_RENAMEAT, __k_AT_FDCWD, (long)"moved", __k_AT_FDCWD, (long)"moved/inner")); // should print -226 (EINVAL)

    // A moved directory's ".." follows it to its new parent
    printnumber(syscall4(__k_SYS_RENAMEAT, __k_AT_FDCWD, (long)"moved", __k_AT_FDCWD, (long)"/moved"));            // should print 0
    printnumber(syscall4(__k_SYS_RENAMEAT, __k_AT_FDCWD, (long)"/moved/../moved", __k_AT_FDCWD, (long)"/moved2")); // should print 0
    readback("/moved2/renamed.txt"); // should print "replaced"

    // Long names spill the directory past its first cluster
    static const char* long_names[] = {
        "/moved2/a-rather-long-file-name-1.txt",
        "/moved2/a-rather-long-file-name-2.txt",
        "/moved2/a-rather-long-file-name-3.txt",
        "/moved2/a-rather-long-file-name-4.txt",
        "/moved2/a-rather-long-file-name-5.txt",
        "/moved2/a-rather-long-file-name-6.txt",
    };

    for(int i = 0; i < 6; ++i) {
        fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)long_names[i], __k_O_WRONLY | __k_O_CREAT, 0644);
        syscall3(__k_SYS_WRITE, fd, (long)long_names[i], ustrlen(long_names[i]));
        syscall1(__k_SYS_CLOSE, fd);
    }

    readback(long_names[5]); // should print "/moved2/a-rath"
    printnumber(syscall4(__k_SYS_RENAMEAT, __k_AT_FDCWD, (long)long_names[5], __k_AT_FDCWD, (long)"/moved2/short.txt")); // should print 0
    readback("/moved2/short.txt");                                                       // should print "/moved2/a-rath"
    printnumber(syscall3(__k_SYS_UNLINKAT, __k_AT_FDCWD, (long)"/moved2/short.txt", 0)); // should print 0
    printnumber(syscall3(__k_SYS_UNLINKAT, __k_AT_FDCWD, (long)long_names[4], 0));       // should print 0

    struct __k_timespec now;
    syscall2(__k_SYS_GETTIME, __k_CLOCK_REALTIME, (long)&now);
    printnumber(now.tv_sec); // should print the date at boot, seconds since the epoch
    syscall2(__k_SYS_GETTIME, __k_CLOCK_MONOTONIC, (long)&now);
    printnumber(now.tv_sec); // should print a few seconds of uptime

    struct __k_timespec date;
    date.tv_sec = 1000000000;
    date.tv_nsec = 0;
    printnumber(syscall2(__k_SYS_SETTIME, __k_CLOCK_MONOTONIC, (long)&date)); // should print -226 (EINVAL)
    printnumber(syscall2(__k_SYS_SETTIME, __k_CLOCK_REALTIME, (long)&date));  // should print 0
    syscall2(__k_SYS_GETTIME, __k_CLOCK_REALTIME, (long)&now);
    printnumber(now.tv_sec); // should print 1000000000

    struct __k_stat st;
    fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/moved2/stamped.txt", __k_O_WRONLY | __k_O_CREAT, 0644);
    syscall3(__k_SYS_WRITE, fd, (long)"stamped\n", 8);
    syscall1(__k_SYS_CLOSE, fd);
    syscall4(__k_SYS_FSTATAT, __k_AT_FDCWD, (long)"/moved2/stamped.txt", (long)&st, 0);
    printnumber(st.st_mtim.tv_sec);  // should print 1000000000, the clock was just set
    printnumber(st.st_mtim.tv_nsec); // should print the nanoseconds on ext2, 0 on FAT

    // Past 2038 the seconds no longer fit ext2's signed 32-bit field
    struct __k_timespec times[2] = {
        { .tv_sec = 4000000000, .tv_nsec = 123 },
        { .tv_sec = 4000000000, .tv_nsec = 456 },
    };
    syscall4(__k_SYS_UTIMENSAT, __k_AT_FDCWD, (long)"/moved2/stamped.txt", (long)times, 0);
    syscall4(__k_SYS_FSTATAT, __k_AT_FDCWD, (long)"/moved2/stamped.txt", (long)&st, 0);
    printnumber(st.st_mtim.tv_sec);  // should print 4000000000
    printnumber(st.st_mtim.tv_nsec); // should print 456 on ext2, 0 on FAT

    // Relatime: the first read moves the atime past the change, later ones leave it
    struct __k_stat before, first, second;
    syscall4(__k_SYS_UTIMENSAT, __k_AT_FDCWD, (long)"/moved2/stamped.txt", 0, 0);
    syscall4(__k_SYS_FSTATAT, __k_AT_FDCWD, (long)"/moved2/stamped.txt", (long)&before, 0);
    readback("/moved2/stamped.txt"); // should print "stamped"
    syscall4(__k_SYS_FSTATAT, __k_AT_FDCWD, (long)"/moved2/stamped.txt", (long)&first, 0);
    readback("/moved2/stamped.txt"); // should print "stamped"
    syscall4(__k_SYS_FSTATAT, __k_AT_FDCWD, (long)"/moved2/stamped.txt", (long)&second, 0);
    printnumber(first.st_atim.tv_nsec != before.st_atim.tv_nsec); // should print 1 on ext2, 0 on FAT
    printnumber(second.st_atim.tv_nsec - first.st_atim.tv_nsec);  // should print 0
    printnumber(second.st_ctim.tv_nsec - before.st_ctim.tv_nsec); // should print 0

    // A second thread gets its own tid, its argument and its TCB
    long pid = syscall1(__k_SYS_GETPID, 0);
    printnumber(syscall1(__k_SYS_GETTID, 0) == pid); // should print 1

    tls_block[0] = 0xabc;
    long tid = spawn(&worker, 42);
    printnumber(tid > pid); // should print 1

    while(!worker_done) {
        syscall1(__k_SYS_YIELD, 0);
    }

    printnumber(worker_tid == tid); // should print 1
    printnumber(worker_arg);        // should print 42
    printnumber(worker_tcb_word);   // should print 2748 (0xabc)

    printnumber(syscall5(__k_SYS_THREAD_CREATE, -4096L, (long)(thread_stack + THREAD_STACK_SIZE), 0, 0, 0)); // should print -226 (EINVAL)

    // futex: argument checks and timeouts
    struct __k_timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 10000000;
    printnumber(syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 1, 0, 0));                // should print -206 (EAGAIN)
    printnumber(syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 0, (long)&timeout, 0));   // should print -273 (ETIMEDOUT)
    printnumber(syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes + 1, 0, 0, 0));            // should print -226 (EINVAL)
    printnumber(syscall4(__k_SYS_FUTEX_WAIT, -4096L, 0, 0, 0));                              // should print -220 (EFAULT)
    printnumber(syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 0, 0, __k_FUTEX_SHARED)); // should print -253 (ENOSYS)
    printnumber(syscall3(__k_SYS_FUTEX_WAKE, (long)&never_changes, 1, 0));                   // should print 0
    timeout.tv_nsec = 0;
    printnumber(syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 0, (long)&timeout, 0)); // should print -273 (ETIMEDOUT)

    // futex: two threads hand a turn back and forth, then the second is joined through clear_tid
    spawn_joinable(&ponger, PINGPONG_ROUNDS);

    for(long i = 0; i < PINGPONG_ROUNDS; ++i) {
        turn = 1;
        syscall3(__k_SYS_FUTEX_WAKE, (long)&turn, 1, 0);

        while(turn == 1) {
            syscall4(__k_SYS_FUTEX_WAIT, (long)&turn, 1, 0, 0);
        }
    }

    join();
    printnumber(pongs);     // should print 1000
    printnumber(join_word); // should print 0

    // futex: a signal interrupts a wait, the main thread keeps it blocked so the sleeper takes it
    struct __k_sigaction action;
    action.sa_handler = &on_signal;
    action.sa_flags = __k_SA_RESTORER;
    action.sa_restorer = &signal_restorer;
    action.sa_mask = 0;
    syscall3(__k_SYS_SIGACTION, __k_SIGUSR1, (long)&action, 0);

    spawn_joinable(&sleeper, 0);

    __k_sigset_t blocked = 1UL << (__k_SIGUSR1 - 1);
    syscall3(__k_SYS_SIGPROCMASK, __k_SIG_BLOCK, (long)&blocked, 0);

    // Sent until it lands, a signal handled before the wait began would leave the sleeper asleep
    while(join_word) {
        syscall2(__k_SYS_KILL, pid, __k_SIGUSR1);
        syscall1(__k_SYS_YIELD, 0);
    }

    printnumber(interrupted_result); // should print -225 (EINTR)
    printnumber(handled_tid != pid); // should print 1, the main thread blocks the signal

    // tgkill: the signal goes to the named thread
    long sleeper_tid = spawn_joinable(&sleeper, 0);

    while(join_word) {
        syscall3(__k_SYS_TGKILL, pid, sleeper_tid, __k_SIGUSR1);
        syscall1(__k_SYS_YIELD, 0);
    }

    printnumber(handled_tid == sleeper_tid);                    // should print 1
    printnumber(syscall3(__k_SYS_TGKILL, pid, sleeper_tid, 0)); // should print -270 (ESRCH), joined already
    printnumber(syscall3(__k_SYS_TGKILL, pid, pid, 0));         // should print 0

    // One the main thread blocks stays pending on it, no other thread takes it
    syscall3(__k_SYS_TGKILL, pid, pid, __k_SIGUSR1);
    __k_sigset_t pending = 0;
    syscall1(__k_SYS_SIGPENDING, (long)&pending);
    printnumber(pending == blocked); // should print 1

    // TLB shootdown: writers on other harts lose the page as soon as munmap or mprotect returns
    action.sa_handler = &on_fault;
    syscall3(__k_SYS_SIGACTION, __k_SIGSEGV, (long)&action, 0);

    for(int round = 0; round < SHOOTDOWN_ROUNDS; ++round) {
        shootdown_round(0);
        shootdown_round(1);
    }

    printnumber(shootdown_worst);     // should print 1 or 0, the write in flight at most
    printnumber(shootdown_corrupted); // should print 0
    printnumber(shootdown_stuck);     // should print 0, every writer faulted

    // exec from a secondary thread takes the other threads away and keeps the pid
    long child = syscall1(__k_SYS_FORK, 0);

    if(child == 0) {
        syscall5(__k_SYS_THREAD_CREATE, (long)&spinner, (long)(spinner_stack + THREAD_STACK_SIZE - 8), 0, 0, 0);

        while(!spinner_running) {
            syscall1(__k_SYS_YIELD, 0);
        }

        spawn(&execer, 0);

        for(;;) {
            syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 0, 0, 0);
        }
    }

    int wstatus = -1;
    syscall2(__k_SYS_WAITPID, child, (long)&wstatus);
    printnumber(wstatus); // should print 10752 (exit code 42)

    // The process outlives its main thread, and the last thread out sets the exit code
    child = syscall1(__k_SYS_FORK, 0);

    if(child == 0) {
        spawn(&finisher, 7);
        syscall1(__k_SYS_THREAD_EXIT, 0);
    }

    wstatus = -1;
    printnumber(syscall2(__k_SYS_WAITPID, child, (long)&wstatus) == child); // should print 1
    printnumber(wstatus);                                                   // should print 1792 (exit code 7)

    child = syscall1(__k_SYS_FORK, 0);

    if(child == 0) {
        syscall1(__k_SYS_THREAD_EXIT, 0);
    }

    wstatus = -1;
    syscall2(__k_SYS_WAITPID, child, (long)&wstatus);
    printnumber(wstatus); // should print 0, a lone thread leaving is an exit with 0

    // Two threads wait for different children; the first child to die must not use up the other's wakeup
    long slow_child = syscall1(__k_SYS_FORK, 0);

    if(slow_child == 0) {
        for(int i = 0; i < 1000; ++i) {
            syscall1(__k_SYS_YIELD, 0);
        }

        syscall1(__k_SYS_EXIT, 5);
    }

    long fast_child = syscall1(__k_SYS_FORK, 0);

    if(fast_child == 0) {
        syscall1(__k_SYS_EXIT, 3);
    }

    waited_child = fast_child;
    spawn_joinable(&child_waiter, 0);

    wstatus = -1;
    printnumber(syscall2(__k_SYS_WAITPID, slow_child, (long)&wstatus) == slow_child); // should print 1
    printnumber(wstatus);                                                             // should print 1280 (exit code 5)

    join();
    printnumber(waited_result == fast_child); // should print 1
    printnumber(waited_status);               // should print 768 (exit code 3)

    printnumber(syscall2(__k_SYS_WAITPID, slow_child, (long)&wstatus)); // should print -212 (ECHILD), collected already
    printnumber(syscall2(__k_SYS_WAITPID, 1, (long)&wstatus));          // should print -212 (ECHILD), not a child

    // Init leads session 1 and group 1, so it can neither start a session nor leave its group
    printnumber(syscall1(__k_SYS_GETPGID, 0));     // should print 1
    printnumber(syscall1(__k_SYS_GETSID, 0));      // should print 1
    printnumber(syscall1(__k_SYS_SETSID, 0));      // should print -263 (EPERM)
    printnumber(syscall2(__k_SYS_SETPGID, 0, 0));  // should print -263 (EPERM)
    printnumber(syscall1(__k_SYS_GETPGID, 99999)); // should print -270 (ESRCH)
    printnumber(syscall2(__k_SYS_KILL, 0, 0));     // should print 0, its own group

    // Both the shell and the job set the job's group, whichever runs first
    long leader = syscall1(__k_SYS_FORK, 0);

    if(leader == 0) {
        syscall2(__k_SYS_SETPGID, 0, 0);

        for(;;) {
            syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 0, 0, 0);
        }
    }

    long member = syscall1(__k_SYS_FORK, 0);

    if(member == 0) {
        for(;;) {
            syscall4(__k_SYS_FUTEX_WAIT, (long)&never_changes, 0, 0, 0);
        }
    }

    printnumber(syscall2(__k_SYS_SETPGID, leader, leader));    // should print 0
    printnumber(syscall2(__k_SYS_SETPGID, member, leader));    // should print 0
    printnumber(syscall1(__k_SYS_GETPGID, member) == leader);  // should print 1
    printnumber(syscall2(__k_SYS_SETPGID, member, 99999));     // should print -263 (EPERM), no such group
    printnumber(syscall2(__k_SYS_SETPGID, 99999, leader));     // should print -270 (ESRCH)
    printnumber(syscall2(__k_SYS_KILL, -leader, __k_SIGKILL)); // should print 0

    printnumber(syscall2(__k_SYS_WAITPID, -leader, (long)&wstatus) > 0); // should print 1
    printnumber(wstatus);                                                // should print 9 (SIGKILL)
    printnumber(syscall2(__k_SYS_WAITPID, -leader, (long)&wstatus) > 0); // should print 1
    printnumber(wstatus);                                                // should print 9 (SIGKILL)
    printnumber(syscall2(__k_SYS_WAITPID, -leader, (long)&wstatus));     // should print -212 (ECHILD), group gone
    printnumber(syscall2(__k_SYS_KILL, -leader, 0));                     // should print -270 (ESRCH)

    // A session leader stays in its own group, and its session is out of the parent's reach
    child = syscall1(__k_SYS_FORK, 0);

    if(child == 0) {
        long pid = syscall1(__k_SYS_GETPID, 0);
        int code = 0;

        if(syscall1(__k_SYS_SETSID, 0) == pid && syscall1(__k_SYS_GETSID, 0) == pid && syscall1(__k_SYS_GETPGID, 0) == pid) {
            code += 1;
        }

        if(syscall2(__k_SYS_SETPGID, 0, 1) == -__k_EPERM) {
            code += 2;
        }

        if(syscall1(__k_SYS_SETSID, 0) == -__k_EPERM) {
            code += 4;
        }

        syscall1(__k_SYS_EXIT, code);
    }

    wstatus = -1;
    syscall2(__k_SYS_WAITPID, child, (long)&wstatus);
    printnumber(wstatus); // should print 1792 (exit code 7)

    // A child left in the caller's group is what waitpid(0) waits for
    child = syscall1(__k_SYS_FORK, 0);

    if(child == 0) {
        syscall1(__k_SYS_EXIT, 4);
    }

    wstatus = -1;
    printnumber(syscall2(__k_SYS_WAITPID, 0, (long)&wstatus) == child); // should print 1
    printnumber(wstatus);                                               // should print 1024 (exit code 4)

    test_tty();
    test_kplog();
    test_kplog_subsystems();
    test_kplog_write();
    test_hwtree();
    serial_console();

    return 1337;
}
