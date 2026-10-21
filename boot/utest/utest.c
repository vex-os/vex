#include <vex/errno.h>
#include <vex/fcntl.h>
#include <vex/futex.h>
#include <vex/mman.h>
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

static long tty_fd;
static long line_fd;

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

/* Prints 1 when the next read gives exactly what is expected, and what came otherwise */
static void expect_read(long fd, const char* expected)
{
    char buffer[128];
    long length = syscall3(__k_SYS_READ, fd, (long)buffer, sizeof(buffer));

    if(length == (long)ustrlen(expected) && same_bytes(buffer, expected, length)) {
        printnumber(1);
    }
    else if(length > 0) {
        syscall2(__k_SYS_TEST, (long)buffer, length);
    }
    else {
        printnumber(length);
    }
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

    tty_fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/tty0", __k_O_RDWR, 0);
    line_fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/pty0", __k_O_RDWR, 0);
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

    syscall1(__k_SYS_CLOSE, tty_fd);
    syscall1(__k_SYS_CLOSE, line_fd);
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

    return 1337;
}
