#ifndef F65619C6_8095_4DD9_89C5_319FEE71DB72
#define F65619C6_8095_4DD9_89C5_319FEE71DB72
#pragma once

#include <sys.arch/regs.h>
#include <sys/cdefs.h>
#include <sys/types.h>

struct vmm_pagemap;

enum {
    SCHED_TASK_READY,
    SCHED_TASK_RUNNING,
    SCHED_TASK_BLOCKED,
    SCHED_TASK_ZOMBIE, // bruuuuuuuuh
};

struct sched_task;
struct sched_process {
    pid_t procnumber;
    struct sched_process* parent;
    struct sched_task* tasks;
    struct vmm_pagemap* vm;
};

struct sched_task {
    uint32_t tasknumber;
    uint32_t state;
    uintptr_t kstack;
    struct sched_process* owner;
    struct sched_task* ls_next;
    struct sched_task* rq_next;
    struct sched_task* sq_next;
    uint64_t sleep_deadline_nsec;
};

static_assert(8 == offsetof(struct sched_task, kstack));

void sched_switch(struct sched_task* restrict prev, struct sched_task* restrict next);
void sched_tick(void);

struct sched_task* create_task(struct sched_process* restrict owner, void (*entry)(void));

void sched_enqueue(struct sched_task* restrict task);
void sched_yield(void);
void sched_sleep_nsec(uint64_t nsec);

static __always_inline void sched_sleep_usec(uint64_t usec)
{
    sched_sleep_nsec(usec * 1000);
}

static __always_inline void sched_sleep_msec(uint64_t msec)
{
    sched_sleep_nsec(msec * 1000000);
}

#endif /* F65619C6_8095_4DD9_89C5_319FEE71DB72 */
