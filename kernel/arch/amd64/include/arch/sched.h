#pragma once

#include <vex/compiler.h>
#include <vex/spinlock.h>

typedef struct hart_context_s hart_context_t;

extern void sched_enter(hart_context_t* restrict ctx, spinlock_t* restrict prev_hart_lock) __noreturn;
