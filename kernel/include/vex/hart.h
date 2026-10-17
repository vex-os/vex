#pragma once

#include <arch/hart.h>

#ifdef CONFIG_SMP
#define MAX_HARTS ((unsigned)(CONFIG_MAX_HARTS))
#else
#define MAX_HARTS 1U
#endif

#define HART_FLAG_BSP     0x0001
#define HART_FLAG_ACTIVE  0x0002
#define HART_FLAG_PRESENT 0x0004
#define HART_FLAG_ONLINE  0x0008

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

static_assert(MAX_HARTS <= 65536);

extern struct hart_local harts[MAX_HARTS];
extern unsigned long num_harts;

static __always_inline __nodiscard struct hart_local* boot_hart(void)
{
    return &harts[0];
}

struct hart_local* this_hart(void) __nodiscard;

struct hart_local* hart_register(bool is_boot);
void hart_entry(struct hart_local* restrict hart) __noreturn;
