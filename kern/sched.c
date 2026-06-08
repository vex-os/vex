#include <sys.arch/hart.h>
#include <sys.arch/intr.h>
#include <sys.arch/sched.h>
#include <sys/atomic.h>
#include <sys/clocksrc.h>
#include <sys/hhdm.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <sys/malloc.h>
#include <sys/page.h>
#include <sys/panic.h>
#include <sys/pslab.h>
#include <sys/sched.h>
#include <sys/slab.h>
#include <sys/vmm.h>

#define KTASK_STACK_SIZE PAGE_SIZE

// Threads are allocated using a fast O(1) allocator, the only
// caveat of using it is that the size of an allocated object must
// be equal or less of quarter of a physical page size
static_assert(sizeof(struct kthread) <= (PAGE_SIZE / 4));

static unsigned short next_kthread_id = 0;

static void k_push_tail(struct hart_local* restrict hart, struct kthread* restrict thread)
{
    thread->rq = nullptr;

    if(hart->kt_runq == nullptr) {
        hart->kt_runq = thread;
        return;
    }

    struct kthread* tail = hart->kt_runq;

    while(tail->rq) {
        tail = tail->rq;
    }

    tail->rq = thread;
}

void __used kthread_exit_handler(int16_t retval)
{
    struct hart_local* hart = this_hart();
    struct kthread* thread = hart->kt_curr;

    thread->entry_ret = retval;
    thread->state = THREAD_DEAD;
    hart->kt_dead = thread;

    hart->kt_curr = hart->kt_idle;
    hart->kt_idle->state = THREAD_EXEC;

    kthread_switch(thread, hart->kt_idle);

    unreachable();
}

void sched_hart_idle(void)
{
    pause_hart();
}

void sched_preempt(void)
{
    struct hart_local* hart = this_hart();

    if(hart->kt_dead) {
        struct kthread* dead = hart->kt_dead;
        hart->kt_dead = nullptr;
        kthread_destroy(dead);
    }

    uint64_t now = clocksrc_nsec();

    struct kthread* prev = hart->kt_curr;
    struct kthread* next = nullptr;

    if(prev != hart->kt_idle && prev && prev->state == THREAD_EXEC) {
        prev->state = THREAD_WAIT;
        k_push_tail(hart, prev);
    }

    struct kthread** slot = &hart->kt_runq;

    while(*slot) {
        struct kthread* thread = *slot;

        if(thread->sleep_deadline == 0 || now >= thread->sleep_deadline) {
            *slot = thread->rq;
            thread->rq = nullptr;
            thread->sleep_deadline = 0;
            next = thread;
            break;
        }

        slot = &thread->rq;
    }

    if(next == nullptr) {
        next = hart->kt_idle;
    }

    if(next == prev) {
        return;
    }

    next->state = THREAD_EXEC;
    hart->kt_curr = next;

    kthread_switch(prev, next);
}

struct kthread* kthread_create(kthread_entry_type entry, void* restrict ctxdata)
{
    struct kthread* thread = slab_alloc(sizeof(struct kthread));

    if(thread == nullptr) {
        return nullptr;
    }

    uintptr_t stack = pslab_alloc();

    if(stack == 0) {
        slab_free(thread);
        return nullptr;
    }

    thread->id = atomic_fetch_add(&next_kthread_id, 1, ATOMIC_RELAXED);
    thread->state = THREAD_WAIT;
    thread->rq = nullptr;
    thread->sleep_deadline = 0;
    thread->entry_ctxdata = ctxdata;
    thread->entry_ret = 0;

    uintptr_t* kstack_ptr = phys_to_virt(stack + KTASK_STACK_SIZE);
    kstack_ptr = setup_kthread_kstack(kstack_ptr, entry, ctxdata);
    thread->kstack = (uintptr_t)(kstack_ptr);

    return thread;
}

void kthread_destroy(struct kthread* restrict thread)
{
    uintptr_t stack_base = page_align(thread->kstack);
    pslab_free_virt((void*)(stack_base));
    slab_free(thread);
}

void kthread_enqueue(struct kthread* restrict thread)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    thread->state = THREAD_WAIT;
    k_push_tail(hart, thread);

    restore_interrupts(intr_state);
}

void sched_yield(void)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    struct kthread* thread = hart->kt_curr;
    thread->state = THREAD_WAIT;
    k_push_tail(hart, thread);

    hart->kt_curr = hart->kt_idle;
    hart->kt_idle->state = THREAD_EXEC;

    kthread_switch(thread, hart->kt_idle);

    restore_interrupts(intr_state);
}

void sched_sleep_nsec(uint64_t nsec)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    struct kthread* thread = hart->kt_curr;
    thread->sleep_deadline = nsec + clocksrc_nsec();
    thread->state = THREAD_WAIT;
    k_push_tail(hart, thread);

    hart->kt_curr = hart->kt_idle;
    hart->kt_idle->state = THREAD_EXEC;

    kthread_switch(thread, hart->kt_idle);

    restore_interrupts(intr_state);
}

void sched_sleep_usec(uint64_t usec)
{
    sched_sleep_nsec(usec * 1000);
}

void sched_sleep_msec(uint64_t msec)
{
    sched_sleep_nsec(msec * 1000000);
}

static void init_sched_idle(void)
{
    struct kthread* idle = slab_alloc(sizeof(struct kthread));

    if(idle == nullptr) {
        panic("sched: out of memory");
    }

    idle->id = atomic_fetch_add(&next_kthread_id, 1, ATOMIC_RELAXED);
    idle->state = THREAD_EXEC;
    idle->kstack = 0;
    idle->rq = nullptr;
    idle->sleep_deadline = 0;

    struct hart_local* hart = this_hart();
    hart->kt_idle = idle;
    hart->kt_curr = idle;
}

initcall_smp_timer(init_sched_idle, INIT_LAST);
