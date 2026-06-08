#ifndef F65619C6_8095_4DD9_89C5_319FEE71DB72
#define F65619C6_8095_4DD9_89C5_319FEE71DB72
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

enum {
    THREAD_WAIT,
    THREAD_EXEC,
    THREAD_DEAD,
};

struct kthread {
    uintptr_t kstack;
    struct kthread* rq;
    unsigned short id;
    unsigned short state;
    uint64_t sleep_deadline;
    void* entry_ctxdata;
    int16_t entry_ret;
};

static_assert((0 * sizeof(uintptr_t)) == offsetof(struct kthread, kstack));

typedef int16_t (*kthread_entry_type)(void* restrict ctxdata);

extern void kthread_switch(struct kthread* restrict prev, struct kthread* restrict next);

struct kthread* kthread_create(kthread_entry_type entry, void* restrict ctxdata);
void kthread_destroy(struct kthread* restrict thread);
void kthread_enqueue(struct kthread* restrict thread);

void sched_hart_idle(void);
void sched_preempt(void);

void sched_yield(void);
void sched_sleep_nsec(uint64_t nsec);
void sched_sleep_usec(uint64_t usec);
void sched_sleep_msec(uint64_t msec);

#endif /* F65619C6_8095_4DD9_89C5_319FEE71DB72 */
