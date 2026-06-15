#ifndef DAD7E4CF_1C26_44A8_B4BF_6ABBD792D805
#define DAD7E4CF_1C26_44A8_B4BF_6ABBD792D805
#pragma once

#include <vex/event.h>
#include <vex/limits.h>
#include <vex/spinlock.h>

struct thread;
struct process {
    pid_t pid;
    pid_t ppid;
    int exit_code;

    spinlock_type lock;

    struct pagemap* vm;

    // Stack for each thread is allocated below the previous one
    // with an additional guard page; this points to the top of the last allocated stack
    uintptr_t thread_stack_top;

    struct thread* threads;

    struct process* children;
    struct process* cnext;
    struct process* cprev;

    struct process* pnext;

    struct event evnt;

    char name[NAME_SIZE];
};

struct process* process_create(struct process* restrict parent, struct pagemap* restrict vm, const char* restrict name) __nodiscard;
struct process* process_locate(pid_t pid) __nodiscard;
void process_exit(struct process* proc, int exit_code);
void process_destroy(struct process* proc);

#endif /* DAD7E4CF_1C26_44A8_B4BF_6ABBD792D805 */
