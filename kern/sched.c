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
#include <sys/vmm.h>

#define KTASK_STACK_SIZE PAGE_SIZE

static_assert(sizeof(struct kthread) <= (PAGE_SIZE / 4));

static unsigned short next_kthread_id = 0;
static unsigned long next_uthread_id = 0;

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

static void u_push_tail(struct hart_local* restrict hart, struct uthread* restrict thread)
{
    thread->rq = nullptr;

    if(hart->ut_runq == nullptr) {
        hart->ut_runq = thread;
        return;
    }

    struct uthread* tail = hart->ut_runq;

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

    task_switch_kern(thread, hart->kt_idle);

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

    if(hart->ut_dead) {
        struct uthread* dead = hart->ut_dead;
        hart->ut_dead = nullptr;
        uthread_destroy(dead);
    }

    uint64_t now = clocksrc_nsec();

    struct uthread** u_slot = &hart->ut_runq;

    while(*u_slot) {
        struct uthread* u_task = *u_slot;

        if(u_task->sleep_deadline == 0 || now >= u_task->sleep_deadline) {
            *u_slot = u_task->rq;

            u_task->rq = nullptr;
            u_task->sleep_deadline = 0;
            u_task->state = THREAD_EXEC;

            struct uthread* u_prev = hart->ut_curr;
            struct kthread* k_prev = hart->kt_curr;

            hart->ut_curr = u_task;

            if(k_prev != hart->kt_idle && k_prev->state == THREAD_EXEC) {
                k_prev->state = THREAD_WAIT;
                k_push_tail(hart, k_prev);
            }

            hart->kt_curr = hart->kt_idle;

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

    if(hart->ut_curr && hart->ut_curr->state == THREAD_EXEC) {
        hart->ut_curr->state = THREAD_WAIT;
        u_push_tail(hart, hart->ut_curr);
        hart->ut_curr = nullptr;
    }

    struct kthread* k_prev = hart->kt_curr;
    struct kthread* k_next = nullptr;

    if(k_prev != hart->kt_idle && k_prev && k_prev->state == THREAD_EXEC) {
        k_prev->state = THREAD_WAIT;
        k_push_tail(hart, k_prev);
    }

    struct kthread** k_slot = &hart->kt_runq;

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
        k_next = hart->kt_idle;
    }

    if(k_next == k_prev) {
        return;
    }

    k_next->state = THREAD_EXEC;
    hart->kt_curr = k_next;

    task_switch_kern(k_prev, k_next);
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

    uintptr_t* stack_ptr = phys_to_virt(stack + KTASK_STACK_SIZE);
    stack_ptr = setup_kthread_kstack(stack_ptr, (void*)entry, ctxdata);
    thread->stack = (uintptr_t)(stack_ptr);

    return thread;
}

void kthread_destroy(struct kthread* restrict thread)
{
    uintptr_t stack_base = page_align(thread->stack);
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

    struct kthread* thread = hart->kt_curr;
    thread->state = THREAD_WAIT;
    k_push_tail(hart, thread);

    hart->kt_curr = hart->kt_idle;
    hart->kt_idle->state = THREAD_EXEC;

    task_switch_kern(thread, hart->kt_idle);

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

    task_switch_kern(thread, hart->kt_idle);

    restore_interrupts(intr_state);
}

void __noreturn uthread_exit(struct uthread* restrict thread)
{
    struct hart_local* hart = this_hart();

    disable_interrupts();

    hart->ut_curr = nullptr;
    hart->ut_dead = thread;
    hart->kt_curr = hart->kt_idle;
    hart->kt_idle->state = THREAD_EXEC;

    vmm_switch(&sys_pagemap);

    uintptr_t amd64_exit_ksp = hart->amd64_exit_ksp;
    asm volatile("movq %0, %%rsp\n\t"
                 "sti\n\t"
                 "1: call sched_preempt\n\t"
                 "jmp 1b" ::"r"(amd64_exit_ksp)
        : "memory");

    unreachable();
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
    idle->stack = 0;
    idle->rq = nullptr;
    idle->sleep_deadline = 0;

    struct hart_local* hart = this_hart();
    hart->kt_idle = idle;
    hart->kt_curr = idle;
}

initcall_smp_timer(init_sched_idle, INIT_LAST);
