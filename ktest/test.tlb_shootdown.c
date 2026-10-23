#include <vex/errno.h>
#include <vex/mman.h>
#include <vex/signal.h>
#include <vex/syscall.h>
#include <vex/time.h>

#include "ktest.h"

#define PAGE_SIZE             4096
#define SHOOTDOWN_WRITERS     3
#define SHOOTDOWN_ROUNDS      10
#define SHOOTDOWN_WARMUP      100000
#define SHOOTDOWN_REUSE_PAGES 64
#define SHOOTDOWN_JOIN_TRIES  20

alignas(16) static char writer_stacks[SHOOTDOWN_WRITERS][KTEST_THREAD_STACK_SIZE];
static long writer_tcb[SHOOTDOWN_WRITERS]; // A writer finds its index at %fs:0
static volatile unsigned int writer_join[SHOOTDOWN_WRITERS];
static volatile long writer_writes[SHOOTDOWN_WRITERS];
static volatile long writer_final[SHOOTDOWN_WRITERS];
static volatile long* volatile target_page;

// Counts every write that went through, after the write itself
[[noreturn]] static void writer(long arg)
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
    ktest_syscall1(__k_SYS_THREAD_EXIT, 0);
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
    long guarded = ktest_syscall6(__k_SYS_MMAP, 0, 3 * PAGE_SIZE, __k_PROT_READ | __k_PROT_WRITE, __k_MAP_PRIVATE | __k_MAP_ANONYMOUS, -1,
        0);
    long page = guarded + PAGE_SIZE;
    target_page = (volatile long*)page;

    for(long i = 0; i < SHOOTDOWN_WRITERS; ++i) {
        writer_writes[i] = 0;
        writer_final[i] = -1;
        writer_join[i] = 1;
        writer_tcb[i] = i;
        ktest_syscall5(__k_SYS_THREAD_CREATE, (long)&writer, (long)(writer_stacks[i] + KTEST_THREAD_STACK_SIZE - 8), (long)&writer_tcb[i],
            i, (long)&writer_join[i]);
    }

    for(long i = 0; i < SHOOTDOWN_WRITERS; ++i) {
        while(writer_writes[i] < SHOOTDOWN_WARMUP) {
            // All writers busy on their own harts
        }
    }

    if(protect) {
        ktest_syscall3(__k_SYS_MPROTECT, page, PAGE_SIZE, __k_PROT_READ);
    }
    else {
        ktest_syscall2(__k_SYS_MUNMAP, page, PAGE_SIZE);
    }

    long snapshot[SHOOTDOWN_WRITERS];

    for(long i = 0; i < SHOOTDOWN_WRITERS; ++i) {
        snapshot[i] = writer_writes[i];
    }

    // The freed frame is likely among these; a writer that still reached it would leave its counter here
    long length = SHOOTDOWN_REUSE_PAGES * PAGE_SIZE;
    long* reuse = (long*)ktest_syscall6(__k_SYS_MMAP, 0, length, __k_PROT_READ | __k_PROT_WRITE, __k_MAP_PRIVATE | __k_MAP_ANONYMOUS, -1,
        0);

    for(long i = 0; i < length / 8; ++i) {
        reuse[i] = -1;
    }

    struct __k_timespec timeout;
    timeout.tv_sec = 0;
    timeout.tv_nsec = 100000000;

    for(long i = 0; i < SHOOTDOWN_WRITERS; ++i) {
        for(int tries = 0; writer_join[i] && tries < SHOOTDOWN_JOIN_TRIES; ++tries) {
            ktest_syscall4(__k_SYS_FUTEX_WAIT, (long)&writer_join[i], 1, (long)&timeout, 0);
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

    ktest_syscall2(__k_SYS_MUNMAP, (long)reuse, length);
    ktest_syscall2(__k_SYS_MUNMAP, guarded, 3 * PAGE_SIZE);
}

int main(void)
{
    struct __k_sigaction action;
    action.sa_handler = &on_fault;
    action.sa_flags = __k_SA_RESTORER;
    action.sa_restorer = &ktest_signal_restorer;
    action.sa_mask = 0;

    ktest_syscall3(__k_SYS_SIGACTION, __k_SIGSEGV, (long)&action, 0);

    for(int round = 0; round < SHOOTDOWN_ROUNDS; ++round) {
        shootdown_round(0);
        shootdown_round(1);
    }

    ktest_expect(shootdown_worst <= 1, 1); // the write in flight at most
    ktest_expect(shootdown_corrupted, 0);
    ktest_expect(shootdown_stuck, 0); // every writer faulted

    return ktest_result();
}
