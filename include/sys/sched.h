#ifndef F65619C6_8095_4DD9_89C5_319FEE71DB72
#define F65619C6_8095_4DD9_89C5_319FEE71DB72
#pragma once

#include <sys.arch/regs.h>
#include <sys/cdefs.h>
#include <sys/types.h>

enum {
    THREAD_WAIT,
    THREAD_EXEC,
    THREAD_DEAD,
};

struct kthread {
    uintptr_t stack;
    struct kthread* rq;
    unsigned short id;
    unsigned short state;
    uint64_t sleep_deadline;
    void* entry_ctxdata;
    int16_t entry_ret;
};

struct uprocess;
struct uthread {
    uintptr_t kstack;
    uintptr_t ustack;
    struct uthread* rq;
    struct uthread* next;
    struct uprocess* owner;
    unsigned long id;
    unsigned short state;
    uint64_t sleep_deadline;
};

// Context switching for kernel tasks (kthread) is most
// likely implemented in assembly; because of this, we
// don't exactly know _where_ the stack pointer field
// is located relative to the passed structure address
// so the field must be assumed to be at the beginning
static_assert((0 * sizeof(uintptr_t)) == offsetof(struct kthread, stack));
static_assert((0 * sizeof(uintptr_t)) == offsetof(struct uthread, kstack));
static_assert((1 * sizeof(uintptr_t)) == offsetof(struct uthread, ustack));

typedef int16_t (*kthread_entry_type)(void* restrict ctxdata);

extern void task_switch_kern(struct kthread* restrict prev, struct kthread* restrict next);
extern void task_switch_user(struct uthread* restrict prev, struct uthread* restrict next);
extern void task_enter_user(struct uthread* restrict next);

struct kthread* kthread_create(kthread_entry_type entry, void* restrict ctxdata);
void kthread_destroy(struct kthread* restrict thread);
void kthread_enqueue(struct kthread* restrict thread);

struct uthread* uthread_create(struct uprocess* restrict owner);
void uthread_set_entry(struct uthread* restrict thread, uintptr_t address, uintptr_t ustack);
void uthread_destroy(struct uthread* restrict thread);
void uthread_enqueue(struct uthread* restrict thread);

void sched_hart_idle(void);
void sched_preempt(void);

void sched_yield(void);
void sched_sleep_nsec(uint64_t nsec);
void sched_sleep_usec(uint64_t usec);
void sched_sleep_msec(uint64_t msec);

#endif /* F65619C6_8095_4DD9_89C5_319FEE71DB72 */
