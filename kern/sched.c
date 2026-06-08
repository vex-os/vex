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
#include <sys/process.h>
#include <sys/pslab.h>
#include <sys/sched.h>
#include <sys/slab.h>

#define KTASK_STACK_SIZE PAGE_SIZE

static_assert(sizeof(struct kthread) <= (PAGE_SIZE / 4));

static unsigned short next_kthread_id = 0;
static unsigned long next_uthread_id = 0;

static void push_tail(struct hart_local* restrict hart, struct kthread* restrict task)
{
    task->rq = nullptr;

    if(hart->runq == nullptr) {
        hart->runq = task;
        return;
    }

    struct kthread* tail = hart->runq;

    while(tail->rq) {
        tail = tail->rq;
    }

    tail->rq = task;
}

void __used kthread_exit_handler(int16_t retval)
{
    struct hart_local* hart = this_hart();
    struct kthread* task = hart->task;

    task->entry_ret = retval;
    task->state = THREAD_DEAD;
    hart->dead = task;

    hart->task = hart->idle;
    hart->idle->state = THREAD_EXEC;

    task_switch_kern(task, hart->idle);

    unreachable();
}

void sched_preempt(void)
{
    struct hart_local* hart = this_hart();

    if(hart->dead) {
        struct kthread* dead = hart->dead;
        hart->dead = nullptr;
        kthread_destroy(dead);
    }

    uint64_t now = clocksrc_nsec();

    struct kthread* prev = hart->task;
    struct kthread* next = nullptr;

    if(prev != hart->idle && prev && prev->state == THREAD_EXEC) {
        prev->state = THREAD_WAIT;
        push_tail(hart, prev);
    }

    struct kthread** slot = &hart->runq;

    while(*slot) {
        struct kthread* task = *slot;

        if(task->sleep_deadline == 0 || now >= task->sleep_deadline) {
            *slot = task->rq;
            task->rq = nullptr;
            task->sleep_deadline = 0;
            next = task;
            break;
        }

        slot = &task->rq;
    }

    if(next == nullptr) {
        next = hart->idle;
    }

    if(next == prev) {
        return;
    }

    next->state = THREAD_EXEC;
    hart->task = next;

    task_switch_kern(prev, next);
}

struct kthread* kthread_create(kthread_entry_type entry, void* restrict ctxdata)
{
    struct kthread* task = slab_alloc(sizeof(struct kthread));

    if(task == nullptr) {
        return nullptr;
    }

    uintptr_t stack = pslab_alloc();

    if(stack == 0) {
        slab_free(task);
        return nullptr;
    }

    task->id = atomic_fetch_add(&next_kthread_id, 1, ATOMIC_RELAXED);
    task->state = THREAD_WAIT;
    task->rq = nullptr;
    task->sleep_deadline = 0;
    task->entry_ctxdata = ctxdata;
    task->entry_ret = 0;

    uintptr_t* stack_ptr = phys_to_virt(stack + KTASK_STACK_SIZE);
    stack_ptr = setup_kthread_kstack(stack_ptr, (void*)entry, ctxdata);
    task->stack = (uintptr_t)(stack_ptr);

    return task;
}

void kthread_destroy(struct kthread* restrict task)
{
    uintptr_t stack_base = page_align(task->stack);
    pslab_free_virt((void*)(stack_base));
    slab_free(task);
}

void kthread_enqueue(struct kthread* restrict task)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    task->state = THREAD_WAIT;
    push_tail(hart, task);

    restore_interrupts(intr_state);
}

struct uthread* uthread_create(struct uprocess* restrict owner)
{
    struct uthread* thread = slab_alloc(sizeof(struct uthread));

    if(thread == nullptr) {
        return nullptr;
    }

    uintptr_t kstack = pslab_alloc();

    if(kstack == 0) {
        slab_free(thread);
        return nullptr;
    }

    thread->id = atomic_fetch_add(&next_uthread_id, 1, ATOMIC_RELAXED);
    thread->state = THREAD_WAIT;
    thread->rq = nullptr;
    thread->next = nullptr;
    thread->owner = owner;
    thread->sleep_deadline = 0;
    thread->kstack = (uintptr_t)(phys_to_virt(kstack + PAGE_SIZE));
    thread->ustack = 0;

    // link into owner's thread list
    thread->next = owner->threads;
    owner->threads = thread;

    atomic_fetch_add(&owner->refcount, 1, ATOMIC_RELAXED);

    return thread;
}

void uthread_set_entry(struct uthread* restrict thread, uintptr_t address, uintptr_t ustack)
{
    thread->ustack = ustack;

    uintptr_t* stack_ptr = (uintptr_t*)thread->kstack;
    stack_ptr = setup_uthread_kstack(stack_ptr, address, ustack);
    thread->kstack = (uintptr_t)stack_ptr;
}

void uthread_destroy(struct uthread* restrict thread)
{
    struct uthread** slot = &thread->owner->threads;

    while(*slot && *slot != thread) {
        slot = &(*slot)->next;
    }

    if(*slot) {
        *slot = thread->next;
    }

    uintptr_t kstack_base = page_align(thread->kstack);
    pslab_free_virt((void*)(kstack_base));
    slab_free(thread);

    uprocess_unref(thread->owner);
}

void uthread_enqueue(struct uthread* restrict thread)
{
    // TODO: implement when uthread scheduling is wired into sched_preempt
}

void sched_yield(void)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    struct kthread* task = hart->task;
    task->state = THREAD_WAIT;
    push_tail(hart, task);

    hart->task = hart->idle;
    hart->idle->state = THREAD_EXEC;

    task_switch_kern(task, hart->idle);

    restore_interrupts(intr_state);
}

void sched_sleep_nsec(uint64_t nsec)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    struct kthread* task = hart->task;
    task->sleep_deadline = nsec + clocksrc_nsec();
    task->state = THREAD_WAIT;
    push_tail(hart, task);

    hart->task = hart->idle;
    hart->idle->state = THREAD_EXEC;

    task_switch_kern(task, hart->idle);

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

define_initcall(sched_hart_idle, INITCALL_STAGE_SMP_TIMER, INITCALL_ORDER_LAST)
{
    struct kthread* idle = slab_alloc(sizeof(struct kthread));

    if(idle == nullptr) {
        panic("sched: out of memory");
    }

    idle->id = atomic_fetch_add(&next_kthread_id, 1, ATOMIC_RELAXED);
    idle->state = THREAD_EXEC;
    idle->stack = 0;
    idle->rq = nullptr;
    idle->sleep_deadline = 0;

    struct hart_local* hart = this_hart();
    hart->idle = idle;
    hart->task = idle;
}
