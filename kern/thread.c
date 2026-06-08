#include <sys.arch/hart.h>
#include <sys.arch/intr.h>
#include <sys.arch/thread.h>
#include <sys/atomic.h>
#include <sys/clocksrc.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <sys/page.h>
#include <sys/panic.h>
#include <sys/pslab.h>
#include <sys/slab.h>
#include <sys/thread.h>

#define THREAD_KSTACK_SIZE PAGE_SIZE

// Threads are allocated using a fast O(1) allocator, the only
// caveat of using it is that the size of an allocated object must
// be equal or less of quarter of a physical page size
static_assert(sizeof(struct thread) <= (PAGE_SIZE / 4));

static uint32_t next_kthread_index = 0; // for process-less kernel threads

static void push_tail(struct hart_local* restrict hart, struct thread* restrict thrd)
{
    thrd->runq_next = nullptr;

    if(hart->runq_head == nullptr) {
        hart->runq_head = thrd;
        return;
    }

    struct thread* tail = hart->runq_head;

    while(tail->runq_next) {
        tail = tail->runq_next;
    }

    tail->runq_next = thrd;
}

void __used thread_exit_handler(intptr_t retval)
{
    struct hart_local* hart = this_hart();
    struct thread* thread = hart->curr_thread;

    thread->retval = retval;
    thread->state = THREAD_DEAD;
    hart->dead_thread = thread;

    hart->curr_thread = hart->idle_thread;
    hart->idle_thread->state = THREAD_EXEC;

    thread_switch(thread, hart->idle_thread);

    unreachable();
}

struct thread* thread_create(struct process* restrict owner, thread_entry_type entry, void* restrict ctxdata)
{
    struct thread* thread = slab_alloc(sizeof(struct thread));

    if(thread == nullptr) {
        return nullptr;
    }

    uintptr_t kstack = pslab_alloc();

    if(kstack == 0) {
        slab_free(thread);
        return nullptr;
    }

    // FIXME: when we get to processes, it's really a good idea
    // to use the process's own thread index for this
    thread->index = atomic_fetch_add(&next_kthread_index, 1, ATOMIC_RELAXED);

    thread->state = THREAD_WAIT;
    thread->owner = owner;
    thread->proc_next = nullptr;
    thread->runq_next = nullptr;
    thread->deadline = 0;
    thread->retval = 0;

    uintptr_t* kernel_stack = phys_to_virt(kstack + THREAD_KSTACK_SIZE);
    kernel_stack = setup_thread_kstack(kernel_stack, entry, ctxdata);
    thread->kernel_stack = (uintptr_t)(kernel_stack);

    return thread;
}

void thread_destroy(struct thread* restrict thrd)
{
    uintptr_t stack_base = page_align(thrd->kernel_stack);
    pslab_free_virt((void*)(stack_base));
    slab_free(thrd);
}

void thread_enqueue(struct thread* restrict thrd)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    thrd->state = THREAD_WAIT;
    push_tail(hart, thrd);

    restore_interrupts(intr_state);
}
