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
    struct hart_regs ctx;
    struct sched_process* owner;
    struct sched_task* ls_next;
    struct sched_task* rq_next;
};

static_assert(8 == offsetof(struct sched_task, kstack));
static_assert(8 + sizeof(uintptr_t) == offsetof(struct sched_task, ctx));

struct sched_task* create_task(struct sched_process* restrict owner, void (*entry)(void));

void sched_enqueue(struct sched_task* restrict task);
void sched_tick(void);
void sched_yield(void);

void sched_switch(struct sched_task* restrict prev, struct sched_task* restrict next);

#endif /* F65619C6_8095_4DD9_89C5_319FEE71DB72 */
