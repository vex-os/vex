#ifndef C2E4BB53_C260_4DF2_A71B_28802A280F44
#define C2E4BB53_C260_4DF2_A71B_28802A280F44
#pragma once

#include <arch/hart.h>

struct thread;

struct hart_local {
    unsigned short index;
    unsigned short flags;

    struct thread* curr_thread;
    struct thread* idle_thread;
    unsigned long last_rq_index;

    struct arch_hart arch;
};

extern struct hart_local* harts;
extern unsigned long num_harts;

struct hart_local* this_hart(void) __nodiscard;

#endif /* C2E4BB53_C260_4DF2_A71B_28802A280F44 */
