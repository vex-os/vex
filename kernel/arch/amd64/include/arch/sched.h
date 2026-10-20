#pragma once

#include <vex/compiler.h>
#include <vex/spinlock.h>

typedef struct hart_context_s hart_context_t;

/// Resumes the context on the current hart and never returns. Called by the
/// scheduler with interrupts disabled, still on the previous thread's stack
/// \param ctx A thread's saved context, at thread_context, bound for either ring
/// \param prev_hart_lock The previous thread's hart_lock or nullptr; released only
/// once off that thread's stack, as another hart may resume the thread the moment it is free
extern void sched_enter(hart_context_t* restrict ctx, spinlock_t* restrict prev_hart_lock) __noreturn;
