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

#define KSTACK_SIZE PAGE_SIZE

// We allocate tasks and processes using a faster slab allocator
// compared to a slower general-purpose kmalloc, but we have to fit
// into slab's limitation of only allowing up to quarter of a page for a size
static_assert(sizeof(struct sched_task) <= (PAGE_SIZE / 4));

static uint32_t next_tasknumber = 0;

static void wake_tasks(struct hart_local* restrict hart)
{
    uint64_t now = clocksrc_nsec();

    struct sched_task** queue = &hart->sq;

    while(*queue) {
        struct sched_task* task = *queue;

        if(now >= task->sleep_deadline_nsec) {
            *queue = task->sq_next;

            task->sq_next = nullptr;
            task->state = SCHED_TASK_READY;

            task->rq_next = hart->rq;
            hart->rq = task;
        }
        else {
            queue = &task->sq_next;
        }
    }
}

static void __noreturn idle_task(void)
{
    for(;;) {
        stop_hart();
    }
}

struct sched_task* create_task(struct sched_process* restrict owner, void (*entry)(void))
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

    task->tasknumber = atomic_fetch_add(&next_tasknumber, 1, ATOMIC_RELAXED);
    task->state = SCHED_TASK_READY;
    task->owner = owner;
    task->ls_next = nullptr;
    task->rq_next = nullptr;
    task->sq_next = nullptr;

    uintptr_t* stack_ptr = phys_to_virt(stack + KSTACK_SIZE);
    stack_ptr = setup_task_stack(stack_ptr, entry);
    task->kstack = (uintptr_t)(stack_ptr);

    return task;
}

void sched_enqueue(struct sched_task* restrict task)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    task->rq_next = hart->rq;
    hart->rq = task;

    task->state = SCHED_TASK_READY;

    restore_interrupts(intr_state);
}

void sched_tick(void)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    wake_tasks(hart);

    struct sched_task* prev = hart->task;
    struct sched_task* next = nullptr;

    if(prev && prev->state == SCHED_TASK_RUNNING) {
        prev->state = SCHED_TASK_READY;

        if(hart->rq == nullptr) {
            hart->rq = prev;
            prev->rq_next = nullptr;
            next = prev;
        }
        else {
            struct sched_task* tail = hart->rq;
            unsigned rq_len = 0;

            while(tail->rq_next) {
                tail = tail->rq_next;
                if(++rq_len > 64) {
                    pr_emerg("sched[%u]: rq cycle! prev=%u rq_head=%u\n", hart->index, prev ? prev->tasknumber : 0xFFFFFFFF,
                        hart->rq ? hart->rq->tasknumber : 0xFFFFFFFF);
                    struct sched_task* dbg = hart->rq;
                    for(unsigned i = 0; i < 16 && dbg; i++) {
                        pr_emerg("  rq[%u] = task %u (state=%u)\n", i, dbg->tasknumber, dbg->state);
                        dbg = dbg->rq_next;
                    }
                    panic("sched: rq cycle detected");
                }
            }

            tail->rq_next = prev;
            prev->rq_next = nullptr;

            next = hart->rq;

            hart->rq = hart->rq->rq_next;
            next->rq_next = nullptr;
        }
    }
    else if(hart->rq) {
        next = hart->rq;
        hart->rq = next->rq_next;
        next->rq_next = nullptr;
    }

    if(next == nullptr || next == prev) {
        restore_interrupts(intr_state);
        return;
    }

    next->state = SCHED_TASK_RUNNING;
    hart->task = next;

    sched_switch(prev, next);

    restore_interrupts(intr_state);
}

void sched_yield(void)
{
    sched_tick();
}

void sched_sleep_nsec(uint64_t nsec)
{
    struct hart_local* hart = this_hart();

    bool intr_state = disable_interrupts();

    struct sched_task* task = hart->task;
    task->sleep_deadline_nsec = nsec + clocksrc_nsec();
    task->state = SCHED_TASK_BLOCKED;

    task->sq_next = hart->sq;
    hart->sq = task;

    sched_tick();

    // If sched_tick returned early (no other task to run), wait for
    // the timer to fire and wake us rather than returning immediately.
    // Returning early would cause the task to re-enter sleep and
    // corrupt the sleep queue with a self-referential sq_next.
    while(task->state == SCHED_TASK_BLOCKED) {
        asm volatile("sti; hlt; cli" ::: "memory");
    }

    task->state = SCHED_TASK_RUNNING;

    restore_interrupts(intr_state);
}

define_initcall(sched_idle, INITCALL_STAGE_SMP_TIMER, INITCALL_ORDER_LAST)
{
    struct sched_task* idle = create_task(nullptr, &idle_task);

    if(idle == nullptr) {
        panic("sched: out of memory");
    }

    idle->state = SCHED_TASK_RUNNING;

    struct hart_local* hart = this_hart();
    hart->task = idle;
}
