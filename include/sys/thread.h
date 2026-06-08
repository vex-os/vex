#ifndef C16998F8_37EC_4A2E_A0B5_2F7057FF9AEF
#define C16998F8_37EC_4A2E_A0B5_2F7057FF9AEF
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

#define THREAD_WAIT 0x0001 ///< The thread is waiting in the run queue
#define THREAD_EXEC 0x0002 ///< The thread is currently running on a hart
#define THREAD_DEAD 0x0003 ///< The thread has finished and is waiting to be reaped

struct process;
struct thread {
    uintptr_t kernel_stack;
    uintptr_t kernel_stack_top;

    struct process* owner;
    struct thread* runq_next;
    struct thread* proc_next;

    uint32_t index;
    uint16_t state;

    uint64_t deadline;

    intptr_t retval;
};

typedef intptr_t (*thread_entry_type)(void* restrict ctxdata);

extern void thread_switch(struct thread* restrict prev, struct thread* restrict next);

struct thread* thread_create_base(struct process* restrict owner);
struct thread* thread_create_kern(struct process* restrict owner, thread_entry_type entry, void* restrict ctxdata);
struct thread* thread_create_user(struct process* restrict owner, uintptr_t user_entry, uintptr_t user_stack_top);
void thread_destroy(struct thread* restrict thrd);
void thread_enqueue(struct thread* restrict thrd);

void thread_yield(void);
void thread_nsleep(uint64_t nsec);
void thread_usleep(uint64_t usec);
void thread_msleep(uint64_t msec);

#endif /* C16998F8_37EC_4A2E_A0B5_2F7057FF9AEF */
