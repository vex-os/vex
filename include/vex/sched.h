#ifndef D251CACE_9BF6_4463_A9B0_D2450E2BA6CC
#define D251CACE_9BF6_4463_A9B0_D2450E2BA6CC
#pragma once

#include <vex.arch/sched.h>

struct thread;

typedef void (*thread_entry_kern)(void* restrict arg);
struct thread* create_thread_kern(thread_entry_kern entry, void* restrict arg);
struct thread* create_thread_kern_enq(thread_entry_kern entry, void* restrict arg);

void sched_enqueue(struct thread* restrict thrd);
void sched_dequeue(struct thread* restrict thrd);
void sched_terminate(void) __noreturn;
void sched_yield(void);

#endif /* D251CACE_9BF6_4463_A9B0_D2450E2BA6CC */
