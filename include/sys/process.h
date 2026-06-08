#ifndef E9B2BC00_D1B5_467A_9CB9_047C40FE32F8
#define E9B2BC00_D1B5_467A_9CB9_047C40FE32F8
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

struct thread;
struct pagemap;

struct process {
    struct process* parent;
    struct thread* threads;
    struct pagemap* vm;
    uint32_t thread_count;
    intptr_t exit_code;
    pid_t pid;
};

struct process* process_create(void); // TODO: parenting
void process_destroy(struct process* restrict proc);

struct thread* process_spawn_thread(struct process* restrict proc, uintptr_t user_entry, uintptr_t user_stack_top);

#endif /* E9B2BC00_D1B5_467A_9CB9_047C40FE32F8 */
