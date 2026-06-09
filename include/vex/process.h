#ifndef E9B2BC00_D1B5_467A_9CB9_047C40FE32F8
#define E9B2BC00_D1B5_467A_9CB9_047C40FE32F8
#pragma once

#include <vex/compiler.h>
#include <vex/signal.h>
#include <vex/types.h>

struct thread;
struct pagemap;

struct process {
    struct process* parent;
    struct pagemap* vm;

    struct thread* threads;
    uint32_t thread_count;
    uint32_t live_count;

    intptr_t exit_code;

    pid_t pid;

    sigset_t sig_pending;
    struct k_sigaction sig_actions[NSIG];
};

struct process* process_create(void); // TODO: parenting
void process_destroy(struct process* restrict proc);

struct thread* process_spawn_thread(struct process* restrict proc, uintptr_t user_entry, uintptr_t user_stack_top);

size_t memcpy_sys_to_proc(struct process* restrict proc, uintptr_t v_dst, const void* restrict s_src, size_t sz);
size_t memcpy_proc_to_sys(void* restrict s_dst, const struct process* restrict proc, uintptr_t v_src, size_t sz);

size_t memcpy_sys_to_user(uintptr_t v_dst, const void* restrict s_src, size_t sz);
size_t memcpy_user_to_sys(void* restrict s_dst, uintptr_t v_src, size_t sz);

#endif /* E9B2BC00_D1B5_467A_9CB9_047C40FE32F8 */
