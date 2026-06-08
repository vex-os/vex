#ifndef F65619C6_8095_4DD9_89C5_319FEE71DB72
#define F65619C6_8095_4DD9_89C5_319FEE71DB72
#pragma once

#include <sys.arch/regs.h>
#include <sys/cdefs.h>
#include <sys/types.h>

struct pagemap;

enum {
    TASK_WAIT,
    TASK_EXEC,
    TASK_DEAD,
};

struct sched_task;
struct sched_process {
    struct sched_process* parent;
    struct sched_task* tasks;
    struct pagemap* vm;
    pid_t pidnumber;
};

struct sched_task {
    uintptr_t kstack;

    struct sched_process* owner;
    struct sched_task* list_next;
    struct sched_task* runq_next;

    uint32_t state;
    uint32_t id;

    uint64_t deadline;

    void* entry_ctxdata;
    int16_t entry_retval;
};

static_assert(0 == offsetof(struct sched_task, kstack));

typedef int16_t (*task_entry_type)(void* restrict ctxdata);

void sched_do_switch(struct sched_task* restrict prev, struct sched_task* restrict next);
void sched_do_preempt(void);

struct sched_task* create_task(struct sched_process* restrict owner, task_entry_type entry, void* restrict ctxdata);
void destroy_task(struct sched_task* restrict task);
void enqueue_task(struct sched_task* restrict task);

void sched_yield(void);
void sched_sleep_nsec(uint64_t nsec);
void sched_sleep_usec(uint64_t usec);
void sched_sleep_msec(uint64_t msec);

#endif /* F65619C6_8095_4DD9_89C5_319FEE71DB72 */
