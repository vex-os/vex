#include <limine.h>
#include <vex/clockevt.h>
#include <vex/delay.h>
#include <vex/event.h>
#include <vex/hart.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/ireq.h>
#include <vex/kbase.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/process.h>
#include <vex/pslab.h>
#include <vex/sched.h>
#include <vex/string.h>
#include <vex/version.h>
#include <vex/vmm.h>
#include <vex/vprot.h>

static volatile __used __section(".limine_requests") uint64_t limine_base_revision[] = LIMINE_BASE_REVISION(6);
static volatile __used __section(".limine_requests_start") uint64_t limine_requests_start_marker[] = LIMINE_REQUESTS_START_MARKER;
static volatile __used __section(".limine_requests_end") uint64_t limine_requests_end_marker[] = LIMINE_REQUESTS_END_MARKER;

static void sched_test_thread(void* restrict arg)
{
    const char* name = arg;

    for(unsigned long i = 0;; ++i) {
        if(i > 10) {
            sched_terminate();
            unreachable();
        }

        pr_inform("[%hu] sched_test: %s tick %lu", this_hart()->index, name, i);
        sched_yield();
    }
}

// Unlike sched_test_thread above (which always calls sched_yield() well
// within its timeslice, so sched_ireq_handler is only ever reached via a
// self-IPI), tick_producer never yields voluntarily: delay_msec() just
// busy-spins. The only way it ever gives up the hart is the LAPIC oneshot
// timer armed for its timeslice actually expiring, which is the one
// preemption path nothing else here exercises. tick_consumer proves the
// other half - blocking on and being woken by an event.
static struct event tick_event;

static void tick_producer(void* restrict arg)
{
    (void)(arg);

    for(unsigned long i = 0; i < 6; ++i) {
        delay_msec(1000);
        event_trigger(&tick_event, true);
    }

    sched_terminate();
    unreachable();
}

static void tick_consumer(void* restrict arg)
{
    (void)(arg);

    for(unsigned long i = 0; i < 5; ++i) {
        struct event* events[] = { &tick_event };
        event_await_blocking(events, 1);

        pr_inform("[%hu] tick_consumer: tick %lu", this_hart()->index, i);
    }

    sched_terminate();
    unreachable();
}

// Stand-in for an ELF loader: the kernel's own (page-isolated, see
// arch/amd64/kernel/utest.S) compiled code, exposed read+exec to one test
// process at a low user address. Proves the syscall entry/return
// trampoline and a System V AMD64 process-entry stack end to end, with no
// real userland required yet.
//
// All of this - allocating and mapping the user stack, and laying out
// argc/argv/envp/auxv on it - is exactly the kind of thing that belongs to
// whatever loads a given executable format, not to the scheduler. It's a
// loader stand-in, so it lives here rather than in kernel/sched.c, which
// only ever sees the resulting entry/stack_top addresses.
extern char amd64_utest_blob[];

#define UTEST_BLOB_UVA   UINT64_C(0x0000000000010000)
#define UTEST_STACK_UVA  UINT64_C(0x0000000000020000)
#define UTEST_STACK_SIZE PAGE_SIZE

#ifndef UTEST_ARGV_MAX
#define UTEST_ARGV_MAX 32
#endif

// Reserves (shrinks the stack by) n bytes, moving the kernel-visible and
// user-visible cursors together so they always describe the same offset.
static bool utest_stack_reserve(uint8_t** restrict kva, uintptr_t* restrict uva, uintptr_t low_bound, size_t n)
{
    if(*uva - low_bound < n) {
        return false;
    }

    *kva -= n;
    *uva -= n;

    return true;
}

// Builds the System V AMD64 process-entry stack layout (argc, argv[],
// envp[], auxv[], strings) at the top `avail` bytes below top_kva/top_uva
// (a kernel-visible alias of the same physical memory, and the
// corresponding user virtual address). Returns the resulting user-space
// stack pointer (pointing at argc) to use as the thread's entry %rsp, or 0
// if it didn't fit.
static uintptr_t build_utest_argv_stack(void* restrict top_kva, uintptr_t top_uva, size_t avail, int argc, const char* const* restrict argv,
    const char* const* restrict envp)
{
    if(argc < 0 || argc > UTEST_ARGV_MAX) {
        return 0;
    }

    int envc = 0;

    while(envp != nullptr && envp[envc] != nullptr) {
        if(envc >= UTEST_ARGV_MAX) {
            return 0;
        }

        envc += 1;
    }

    uint8_t* kva = top_kva;
    uintptr_t uva = top_uva;
    uintptr_t low_bound = top_uva - avail;

    uintptr_t argv_uva[UTEST_ARGV_MAX];
    uintptr_t envp_uva[UTEST_ARGV_MAX];

    for(int i = argc - 1; i >= 0; --i) {
        size_t len = strlen(argv[i]) + 1;

        if(!utest_stack_reserve(&kva, &uva, low_bound, len)) {
            return 0;
        }

        memcpy(kva, argv[i], len);
        argv_uva[i] = uva;
    }

    for(int i = envc - 1; i >= 0; --i) {
        size_t len = strlen(envp[i]) + 1;

        if(!utest_stack_reserve(&kva, &uva, low_bound, len)) {
            return 0;
        }

        memcpy(kva, envp[i], len);
        envp_uva[i] = uva;
    }

    // Align to 8 bytes before the pointer arrays / auxv / argc below, all
    // of which are stored as 8-byte words.
    if(!utest_stack_reserve(&kva, &uva, low_bound, uva % sizeof(uintptr_t))) {
        return 0;
    }

    // No ELF interpreter yet, so the auxiliary vector is just AT_NULL.
    if(!utest_stack_reserve(&kva, &uva, low_bound, 2 * sizeof(uintptr_t))) {
        return 0;
    }

    memset(kva, 0, 2 * sizeof(uintptr_t));

    // The ABI requires the final entry %rsp (pointing at argc) to be
    // 16-byte aligned. Everything from here down is a fixed, known number
    // of 8-byte words, so insert one more pad word now if needed rather
    // than trying to fix up the alignment after the fact.
    size_t fixed_size = sizeof(uintptr_t) * ((size_t)(envc) + 1 + (size_t)(argc) + 1 + 1);

    if((uva - fixed_size) % (2 * sizeof(uintptr_t)) != 0) {
        if(!utest_stack_reserve(&kva, &uva, low_bound, sizeof(uintptr_t))) {
            return 0;
        }
    }

    if(!utest_stack_reserve(&kva, &uva, low_bound, sizeof(uintptr_t))) {
        return 0;
    }

    *(uintptr_t*)(kva) = 0; // envp[] terminator

    for(int i = envc - 1; i >= 0; --i) {
        if(!utest_stack_reserve(&kva, &uva, low_bound, sizeof(uintptr_t))) {
            return 0;
        }

        *(uintptr_t*)(kva) = envp_uva[i];
    }

    if(!utest_stack_reserve(&kva, &uva, low_bound, sizeof(uintptr_t))) {
        return 0;
    }

    *(uintptr_t*)(kva) = 0; // argv[] terminator

    for(int i = argc - 1; i >= 0; --i) {
        if(!utest_stack_reserve(&kva, &uva, low_bound, sizeof(uintptr_t))) {
            return 0;
        }

        *(uintptr_t*)(kva) = argv_uva[i];
    }

    if(!utest_stack_reserve(&kva, &uva, low_bound, sizeof(uintptr_t))) {
        return 0;
    }

    *(uintptr_t*)(kva) = (uintptr_t)(argc);

    return uva;
}

static void spawn_utest_process(void)
{
    struct pagemap* vm = vmm_create();

    if(vm == nullptr) {
        pr_alert("utest: out of memory creating address space");
        return;
    }

    struct process* proc = process_create(nullptr, vm, "utest");

    if(proc == nullptr) {
        pr_alert("utest: out of memory creating process");
        vmm_release(vm);
        return;
    }

    uintptr_t blob_phys = kbase_phys + ((uintptr_t)(amd64_utest_blob)-kbase_virt);

    if(vmm_map(vm, UTEST_BLOB_UVA, blob_phys, VPROT_URX) < 0) {
        pr_alert("utest: failed to map test blob");
        process_exit(proc, -1); // releases proc->vm
        process_destroy(proc);
        return;
    }

    uintptr_t stack_phys = pslab_alloc_phys();

    if(stack_phys == 0 || vmm_map(vm, UTEST_STACK_UVA, stack_phys, VPROT_USER | VPROT_READ | VPROT_WRITE) < 0) {
        pr_alert("utest: failed to allocate/map test stack");

        if(stack_phys != 0) {
            pslab_free_phys(stack_phys);
        }

        process_exit(proc, -1);
        process_destroy(proc);
        return;
    }

    void* stack_top_kva = phys_to_virt(stack_phys) + UTEST_STACK_SIZE;
    uintptr_t stack_top_uva = UTEST_STACK_UVA + UTEST_STACK_SIZE;

    const char* argv[] = { "hello" };
    uintptr_t entry_rsp = build_utest_argv_stack(stack_top_kva, stack_top_uva, UTEST_STACK_SIZE, 1, argv, nullptr);

    if(entry_rsp == 0) {
        pr_alert("utest: argv stack didn't fit");
        process_exit(proc, -1);
        process_destroy(proc);
        return;
    }

    struct thread* thrd = create_thread_user_enq(proc, UTEST_BLOB_UVA, entry_rsp, nullptr);

    if(thrd == nullptr) {
        pr_alert("utest: failed to create test thread");
    }
}

void __noreturn kmain(void)
{
    pr_emerg("%s %s %s", k_sysname, k_release, k_version);

    invoke_initcalls(initcalls_bsp_start, initcalls_bsp_end);

    create_thread_kern_enq(sched_test_thread, (void*)"42");
    create_thread_kern_enq(sched_test_thread, (void*)"67");
    create_thread_kern_enq(sched_test_thread, (void*)"69");
    create_thread_kern_enq(sched_test_thread, (void*)"1337");
    create_thread_kern_enq(sched_test_thread, (void*)"420");

    create_thread_kern_enq(tick_producer, nullptr);
    create_thread_kern_enq(tick_consumer, nullptr);

    spawn_utest_process();

    pr_emerg("sched test threads spawned, enabling interrupts");

    enable_interrupts();

    for(;;) {
        hart_spin();
    }

    unreachable();
}
