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

#define STACK_SIZE PAGE_SIZE

// We allocate tasks and processes using a faster slab allocator
// compared to a slower general-purpose kmalloc, but we have to fit
// into slab's limitation of only allowing up to quarter of a page for a size
static_assert(sizeof(struct sched_task) <= (PAGE_SIZE / 4));

static uint32_t next_taskid = 0;

static void push_tail(struct hart_local* restrict hart, struct sched_task* restrict task)
{
    task->runq_next = nullptr;

    if(hart->runq == nullptr) {
        hart->runq = task;
        return;
    }

    struct sched_task* tail = hart->runq;

    while(tail->runq_next) {
        tail = tail->runq_next;
    }

    tail->runq_next = task;
}

void __used sched_task_exit(int16_t retval)
{
    struct hart_local* hart = this_hart();
    struct sched_task* task = hart->task;

    task->entry_retval = retval;
    task->state = TASK_DEAD;
    hart->dead = task;

    hart->task = hart->idle;
    hart->idle->state = TASK_EXEC;

    sched_do_switch(task, hart->idle);

    unreachable();
}

void sched_do_preempt(void)
{
    struct hart_local* hart = this_hart();

    if(hart->dead) {
        struct sched_task* dead = hart->dead;

        hart->dead = nullptr;

        if(dead->owner == nullptr || dead->owner->parent == nullptr) {
            destroy_task(dead);
        }
    }

    uint64_t now = clocksrc_nsec();

    struct sched_task* prev = hart->task;
    struct sched_task* next = nullptr;

    if(prev != hart->idle && prev && prev->state == TASK_EXEC) {
        prev->state = TASK_WAIT;
        push_tail(hart, prev);
    }

    struct sched_task** slot = &hart->runq;

    while(*slot) {
        struct sched_task* task = *slot;

        if(task->deadline == 0 || now >= task->deadline) {
            *slot = task->runq_next;
            task->runq_next = nullptr;
            task->deadline = 0;
            next = task;
            break;
        }

        slot = &task->runq_next;
    }

    if(next == nullptr) {
        next = hart->idle;
    }

    if(next == prev) {
        return;
    }

    next->state = TASK_EXEC;
    hart->task = next;

    sched_do_switch(prev, next);
}

struct sched_task* create_task(struct sched_process* restrict owner, task_entry_type entry, void* restrict ctxdata)
{
    struct sched_task* task = slab_alloc(sizeof(struct sched_task));

    if(task == nullptr) {
        return nullptr;
    }

    uintptr_t stack = pslab_alloc();

    if(stack == 0) {
        slab_free(task);
        return nullptr;
    }

    task->id = atomic_fetch_add(&next_taskid, 1, ATOMIC_RELAXED);
    task->state = TASK_WAIT;
    task->owner = owner;
    task->list_next = nullptr;
    task->runq_next = nullptr;
    task->deadline = 0;
    task->entry_ctxdata = ctxdata;
    task->entry_retval = 0;

    uintptr_t* stack_ptr = phys_to_virt(stack + STACK_SIZE);
    stack_ptr = setup_task_stack(stack_ptr, (void*)entry, ctxdata);
    task->kstack = (uintptr_t)(stack_ptr);

    return task;
}

void destroy_task(struct sched_task* restrict task)
{
    uintptr_t stack_base = page_align(task->kstack);
    pslab_free_virt((void*)(stack_base));
    slab_free(task);
}

void enqueue_task(struct sched_task* restrict task)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    task->state = TASK_WAIT;

    push_tail(hart, task);

    restore_interrupts(intr_state);
}

void sched_yield(void)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    struct sched_task* task = hart->task;
    task->state = TASK_WAIT;
    push_tail(hart, task);

    hart->task = hart->idle;
    hart->idle->state = TASK_EXEC;

    sched_do_switch(task, hart->idle);

    restore_interrupts(intr_state);
}

void sched_sleep_nsec(uint64_t nsec)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    struct sched_task* task = hart->task;
    task->deadline = nsec + clocksrc_nsec();
    task->state = TASK_WAIT;
    push_tail(hart, task);

    hart->task = hart->idle;
    hart->idle->state = TASK_EXEC;

    sched_do_switch(task, hart->idle);

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

define_initcall(sched_idle, INITCALL_STAGE_SMP_TIMER, INITCALL_ORDER_LAST)
{
    struct sched_task* idle = slab_alloc(sizeof(struct sched_task));

    if(idle == nullptr) {
        panic("sched: out of memory");
    }

    idle->id = atomic_fetch_add(&next_taskid, 1, ATOMIC_RELAXED);
    idle->state = TASK_EXEC;
    idle->owner = nullptr;
    idle->kstack = 0;
    idle->list_next = nullptr;
    idle->runq_next = nullptr;
    idle->deadline = 0;

    struct hart_local* hart = this_hart();
    hart->idle = idle;
    hart->task = idle;
}
