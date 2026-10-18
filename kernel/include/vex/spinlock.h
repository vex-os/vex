#pragma once

#include <arch/hart.h>
#include <vex/atomic.h>
#include <vex/preempt.h>
#include <vex/types.h>

typedef uintptr_t spinlock_type;
#define SPINLOCK_INIT ((spinlock_type)(0))
#define SPINLOCK_FREE ((spinlock_type)(0))
#define SPINLOCK_LOCK ((spinlock_type)(1))

// https://files.catbox.moe/7j9kqv.gif
static __always_inline void raw_spinlock_acquire(spinlock_type* restrict lock)
{
    spinlock_type expected;

    for(;;) {
        expected = SPINLOCK_FREE;

        if(atomic_cmpxchg(lock, &expected, SPINLOCK_LOCK, ATOMIC_ACQUIRE, ATOMIC_RELAXED)) {
            return;
        }

        while(SPINLOCK_LOCK == atomic_load(lock, ATOMIC_RELAXED)) {
            hart_spin();
        }
    }
}

static __always_inline __nodiscard bool raw_spinlock_try_acquire(spinlock_type* restrict lock)
{
    spinlock_type expected = SPINLOCK_FREE;
    return atomic_cmpxchg(lock, &expected, SPINLOCK_LOCK, ATOMIC_ACQUIRE, ATOMIC_RELAXED);
}

static __always_inline void raw_spinlock_release(spinlock_type* restrict lock)
{
    atomic_store(lock, SPINLOCK_FREE, ATOMIC_RELEASE);
}

// The holder can't be preempted, so a waiter on the same hart never
// spins out the holder's timeslice; raw variants skip this for locks
// that outlive a context switch, such as the scheduler's own
static __always_inline void spinlock_acquire(spinlock_type* restrict lock)
{
    preempt_disable();
    raw_spinlock_acquire(lock);
}

static __always_inline __nodiscard bool spinlock_try_acquire(spinlock_type* restrict lock)
{
    preempt_disable();

    if(raw_spinlock_try_acquire(lock)) {
        return true;
    }

    preempt_enable();

    return false;
}

static __always_inline void spinlock_release(spinlock_type* restrict lock)
{
    raw_spinlock_release(lock);
    preempt_enable();
}

// Disabled interrupts already keep the holder from being preempted
static __always_inline __nodiscard bool spinlock_acquire_intr(spinlock_type* restrict lock)
{
    bool intr_state = disable_interrupts();
    raw_spinlock_acquire(lock);
    return intr_state;
}

static __always_inline void spinlock_release_intr(spinlock_type* restrict lock, bool intr_state)
{
    raw_spinlock_release(lock);
    restore_interrupts(intr_state);
}
