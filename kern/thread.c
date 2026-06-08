#include <sys.arch/hart.h>
#include <sys.arch/intr.h>
#include <sys.arch/thread.h>
#include <sys/atomic.h>
#include <sys/clocksrc.h>
#include <sys/hhdm.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <sys/page.h>
#include <sys/panic.h>
#include <sys/process.h>
#include <sys/pslab.h>
#include <sys/slab.h>
#include <sys/thread.h>

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
    struct thread* thread = hart->curr_thread;

    thread->retval = retval;

    if(thread->state != THREAD_SEGV) {
        thread->state = THREAD_DEAD;
    }

    thread->runq_next = hart->dead_head;
    hart->dead_head = thread;

    hart->curr_thread = hart->idle_thread;
    hart->idle_thread->state = THREAD_EXEC;

    thread_switch(thread, hart->idle_thread);

    unreachable();
}

struct thread* thread_create_base(struct process* restrict owner)
{
    struct thread* thread = slab_alloc(sizeof(struct thread));

    if(thread == nullptr) {
        return nullptr;
    }

    if(owner == nullptr) {
        thread->index = atomic_fetch_add(&next_kthread_index, 1, ATOMIC_RELAXED);
        thread->proc_next = nullptr;
    }
    else {
        thread->index = atomic_fetch_add(&owner->thread_count, 1, ATOMIC_RELAXED);
        atomic_fetch_add(&owner->live_count, 1, ATOMIC_RELAXED);
        thread->proc_next = owner->threads;
        owner->threads = thread;
    }

    thread->owner = owner;
    thread->runq_next = nullptr;
    thread->deadline = 0;
    thread->retval = 0;

    return thread;
}

struct thread* thread_create_kern(struct process* restrict owner, thread_entry_type entry, void* restrict ctxdata)
{
    struct thread* thread = thread_create_base(owner);

    if(thread == nullptr) {
        return nullptr;
    }

    uintptr_t kstack = pslab_alloc();

    if(kstack == 0) {
        slab_free(thread);
        return nullptr;
    }

    thread->state = THREAD_WAIT;

    uintptr_t* kernel_stack_top = phys_to_virt(kstack + THREAD_KSTACK_SIZE);
    uintptr_t* kernel_stack = setup_thread_kstack(kernel_stack_top, entry, ctxdata);
    thread->kernel_stack_top = (uintptr_t)(kernel_stack_top);
    thread->kernel_stack = (uintptr_t)(kernel_stack);

    return thread;
}

struct thread* thread_create_user(struct process* restrict owner, uintptr_t user_entry, uintptr_t user_stack_top)
{
    struct thread* thread = thread_create_base(owner);

    if(thread == nullptr) {
        return nullptr;
    }

    uintptr_t kstack = pslab_alloc();

    if(kstack == 0) {
        slab_free(thread);
        return nullptr;
    }

    thread->state = THREAD_WAIT;

    uintptr_t* kernel_stack_top = phys_to_virt(kstack + THREAD_KSTACK_SIZE);
    uintptr_t* kernel_stack = setup_thread_ustack(kernel_stack_top, user_entry, user_stack_top);
    thread->kernel_stack_top = (uintptr_t)(kernel_stack_top);
    thread->kernel_stack = (uintptr_t)(kernel_stack);

    return thread;
}

void thread_destroy(struct thread* restrict thrd)
{
    uintptr_t stack_base = page_align(thrd->kernel_stack);
    pslab_free_virt((void*)(stack_base));
    slab_free(thrd);
}
