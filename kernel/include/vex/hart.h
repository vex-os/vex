#pragma once

#include <arch/hart.h>

struct thread;

struct hart_local {
    unsigned short index;
    unsigned short flags;

    struct thread* curr_thread;
    struct thread* idle_thread;
    struct thread* dead_thread;

    bool tlb_online;
    uint64_t tlb_flushed;

    struct arch_hart arch;
};

extern struct hart_local* harts;
extern unsigned long num_harts;

struct hart_local* this_hart(void) __nodiscard;
struct hart_local* boot_hart(void) __nodiscard;
