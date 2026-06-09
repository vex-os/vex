#ifndef F65619C6_8095_4DD9_89C5_319FEE71DB72
#define F65619C6_8095_4DD9_89C5_319FEE71DB72
#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

struct thread;

void sched_idle(void);
void sched_preempt(void);

void sched_yield(void);
void sched_nsleep(uint64_t nsec);
void sched_usleep(uint64_t usec);
void sched_msleep(uint64_t msec);

void sched_enqueue(struct thread* restrict thrd);

#endif /* F65619C6_8095_4DD9_89C5_319FEE71DB72 */
