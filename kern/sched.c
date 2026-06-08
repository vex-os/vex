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

static void k_push_tail(struct hart_local* restrict hart, struct kthread* restrict task)
{
    task->rq = nullptr;

    if(hart->k_runq == nullptr) {
        hart->k_runq = task;
        return;
    }

    struct kthread* tail = hart->k_runq;

    while(tail->rq) {
        tail = tail->rq;
    }

    tail->rq = task;
}

static void u_push_tail(struct hart_local* restrict hart, struct uthread* restrict thread)
{
    thread->rq = nullptr;

    if(hart->u_runq == nullptr) {
        hart->u_runq = thread;
        return;
    }

    struct uthread* tail = hart->u_runq;

    while(tail->rq) {
        tail = tail->rq;
    }

    tail->rq = thread;
}

void __used kthread_exit_handler(int16_t retval)
{
    struct hart_local* hart = this_hart();
    struct kthread* task = hart->k_curr;

    task->entry_ret = retval;
    task->state = THREAD_DEAD;
    hart->k_dead = task;

    hart->k_curr = hart->k_idle;
    hart->k_idle->state = THREAD_EXEC;

    task_switch_kern(task, hart->k_idle);

    unreachable();
}

void sched_preempt(void)
{
    struct hart_local* hart = this_hart();

    if(hart->k_dead) {
        struct kthread* dead = hart->k_dead;
        hart->k_dead = nullptr;
        kthread_destroy(dead);
    }

    uint64_t now = clocksrc_nsec();

    struct uthread** u_slot = &hart->u_runq;

    while(*u_slot) {
        struct uthread* u_task = *u_slot;

        if(u_task->sleep_deadline == 0 || now >= u_task->sleep_deadline) {
            *u_slot = u_task->rq;

            u_task->rq = nullptr;
            u_task->sleep_deadline = 0;
            u_task->state = THREAD_EXEC;

            struct uthread* u_prev = hart->u_curr;
            struct kthread* k_prev = hart->k_curr;

            hart->u_curr = u_task;

            if(k_prev != hart->k_idle && k_prev->state == THREAD_EXEC) {
                k_prev->state = THREAD_WAIT;
                k_push_tail(hart, k_prev);
            }

            hart->k_curr = hart->k_idle;

            if(u_prev) {
                u_prev->state = THREAD_WAIT;
                task_switch_user(u_prev, u_task);
            }
            else {
                task_enter_user(u_task);
            }

            return;
        }

        u_slot = &(*u_slot)->rq;
    }

    if(hart->u_curr && hart->u_curr->state == THREAD_EXEC) {
        hart->u_curr->state = THREAD_WAIT;
        u_push_tail(hart, hart->u_curr);
        hart->u_curr = nullptr;
    }

    struct kthread* k_prev = hart->k_curr;
    struct kthread* k_next = nullptr;

    if(k_prev != hart->k_idle && k_prev && k_prev->state == THREAD_EXEC) {
        k_prev->state = THREAD_WAIT;
        k_push_tail(hart, k_prev);
    }

    struct kthread** k_slot = &hart->k_runq;

    while(*k_slot) {
        struct kthread* k_task = *k_slot;

        if(k_task->sleep_deadline == 0 || now >= k_task->sleep_deadline) {
            *k_slot = k_task->rq;
            k_task->rq = nullptr;
            k_task->sleep_deadline = 0;
            k_next = k_task;
            break;
        }

        k_slot = &k_task->rq;
    }

    if(k_next == nullptr) {
        k_next = hart->k_idle;
    }

    if(k_next == k_prev) {
        return;
    }

    k_next->state = THREAD_EXEC;
    hart->k_curr = k_next;

    task_switch_kern(k_prev, k_next);
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
    k_push_tail(hart, task);

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

    struct uprocess* owner = thread->owner;

    pr_inform("uthread %lu of process %d exited", thread->id, owner->id);

    uintptr_t kstack_base = page_align(thread->kstack);
    pslab_free_virt((void*)(kstack_base));
    slab_free(thread);

    uprocess_unref(owner);
}

void uthread_enqueue(struct uthread* restrict thread)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    thread->state = THREAD_WAIT;
    u_push_tail(hart, thread);

    restore_interrupts(intr_state);
}

void sched_yield(void)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    struct kthread* task = hart->k_curr;
    task->state = THREAD_WAIT;
    k_push_tail(hart, task);

    hart->k_curr = hart->k_idle;
    hart->k_idle->state = THREAD_EXEC;

    task_switch_kern(task, hart->k_idle);

    restore_interrupts(intr_state);
}

void sched_sleep_nsec(uint64_t nsec)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    struct kthread* task = hart->k_curr;
    task->sleep_deadline = nsec + clocksrc_nsec();
    task->state = THREAD_WAIT;
    k_push_tail(hart, task);

    hart->k_curr = hart->k_idle;
    hart->k_idle->state = THREAD_EXEC;

    task_switch_kern(task, hart->k_idle);

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

define_initcall(sched_hart_idle, INITCALL_STAGE_SMP_TIMER, INIT_LAST)
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
    hart->k_idle = idle;
    hart->k_curr = idle;
}
