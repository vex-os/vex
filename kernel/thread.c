#include <vex.arch/hart.h>
#include <vex.arch/intr.h>
#include <vex.arch/thread.h>
#include <vex/atomic.h>
#include <vex/clocksrc.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/process.h>
#include <vex/pslab.h>
#include <vex/slab.h>
#include <vex/thread.h>

#define THREAD_KSTACK_SIZE PAGE_SIZE

// Threads are allocated using a fast O(1) allocator, the only
// caveat of using it is that the size of an allocated object must
// be equal or less of quarter of a physical page size
static_assert(sizeof(struct thread) <= (PAGE_SIZE / 4));

static uint32_t next_kthread_index = 1; // for process-less kernel threads

void __noreturn thread_exit_handler(intptr_t retval)
{
    disable_interrupts();

    struct hart_local* hart = this_hart();
    struct thread* thrd = hart->curr_thread;

    thrd->retval = retval;

    if(thrd->state != THREAD_SEGV) {
        thrd->state = THREAD_DEAD;
    }

    thrd->runq_next = hart->dead_head;
    hart->dead_head = thrd;

    hart->curr_thread = hart->idle_thread;
    hart->idle_thread->state = THREAD_EXEC;

    thread_switch_hook(hart, hart->idle_thread);
    thread_switch(thrd, hart->idle_thread);

    unreachable();
}

struct thread* thread_create_base(struct process* restrict owner)
{
    struct thread* thrd = slab_alloc(sizeof(struct thread));

    if(thrd == nullptr) {
        return nullptr;
    }

    if(owner == nullptr) {
        thrd->index = atomic_fetch_add(&next_kthread_index, 1, ATOMIC_RELAXED);
        thrd->proc_next = nullptr;
    }
    else {
        thrd->index = atomic_fetch_add(&owner->thread_count, 1, ATOMIC_RELAXED);
        atomic_fetch_add(&owner->live_count, 1, ATOMIC_RELAXED);
        thrd->proc_next = owner->threads;
        owner->threads = thrd;
    }

    thrd->owner = owner;
    thrd->runq_next = nullptr;
    thrd->deadline = 0;
    thrd->retval = 0;

    return thrd;
}

struct thread* thread_create_kern(struct process* restrict owner, thread_entry_type entry, void* restrict ctxdata)
{
    struct thread* thrd = thread_create_base(owner);

    if(thrd == nullptr) {
        return nullptr;
    }

    uintptr_t kstack = pslab_alloc();

    if(kstack == 0) {
        slab_free(thrd);
        return nullptr;
    }

    thrd->state = THREAD_WAIT;

    uintptr_t* kernel_stack_top = phys_to_virt(kstack + THREAD_KSTACK_SIZE);
    uintptr_t* kernel_stack = setup_thread_kstack(kernel_stack_top, entry, ctxdata);
    thrd->kernel_stack_top = (uintptr_t)(kernel_stack_top);
    thrd->kernel_stack = (uintptr_t)(kernel_stack);

    return thrd;
}

struct thread* thread_create_user(struct process* restrict owner, uintptr_t user_entry, uintptr_t user_stack_top)
{
    struct thread* thrd = thread_create_base(owner);

    if(thrd == nullptr) {
        return nullptr;
    }

    uintptr_t kstack = pslab_alloc();

    if(kstack == 0) {
        slab_free(thrd);
        return nullptr;
    }

    thrd->state = THREAD_WAIT;

    uintptr_t* kernel_stack_top = phys_to_virt(kstack + THREAD_KSTACK_SIZE);
    uintptr_t* kernel_stack = setup_thread_ustack(kernel_stack_top, user_entry, user_stack_top);
    thrd->kernel_stack_top = (uintptr_t)(kernel_stack_top);
    thrd->kernel_stack = (uintptr_t)(kernel_stack);

    return thrd;
}

void thread_destroy(struct thread* restrict thrd)
{
    uintptr_t stack_base = thrd->kernel_stack - THREAD_KSTACK_SIZE;
    pslab_free_virt((void*)(stack_base));
    slab_free(thrd);
}
