#pragma once

#include <vex/compiler.h>
#include <vex/spinlock.h>

struct hart_context;

extern void sched_enter(struct hart_context* restrict ctx, spinlock_type* restrict prev_hart_lock) __noreturn;
