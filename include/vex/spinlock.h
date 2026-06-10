#ifndef A6A7BDE1_6F01_427C_80B6_14BB87B2C2AD
#define A6A7BDE1_6F01_427C_80B6_14BB87B2C2AD
#pragma once

#include <vex.arch/hart.h>
#include <vex.arch/intr.h>
#include <vex/atomic.h>
#include <vex/types.h>

#ifdef DEADLOCK_CATCHER
#include <vex/panic.h>
#endif

typedef uintptr_t spinlock_type;
#define SPINLOCK_INIT ((spinlock_type)(0))
#define SPINLOCK_FREE ((spinlock_type)(0))
#define SPINLOCK_LOCK ((spinlock_type)(1))

extern volatile bool panic_in_progress;

static __always_inline void spinlock_acquire(spinlock_type* restrict lock)
{
    if(atomic_load(&panic_in_progress, ATOMIC_ACQUIRE)) {
        return;
    }

    spinlock_type expected;

#ifdef DEADLOCK_CATCHER
    uintptr_t catcher = 0;
#endif

    for(;;) {
        expected = SPINLOCK_FREE;

        if(atomic_cmpxchg(lock, &expected, SPINLOCK_LOCK, ATOMIC_ACQUIRE, ATOMIC_RELAXED)) {
            return;
        }

        while(SPINLOCK_LOCK == atomic_load(lock, ATOMIC_RELAXED)) {
#ifdef DEADLOCK_CATCHER
            if(++catcher > 1000000) {
                panic("spinlock: possible deadlock: lock=%p", (void*)(lock));
                unreachable();
            }
#endif

            pause_hart();
        }
    }
}

static __always_inline __nodiscard bool spinlock_try_acquire(spinlock_type* restrict lock)
{
    spinlock_type expected = SPINLOCK_FREE;
    return atomic_cmpxchg(lock, &expected, SPINLOCK_LOCK, ATOMIC_ACQUIRE, ATOMIC_RELAXED);
}

static __always_inline void spinlock_release(spinlock_type* restrict lock)
{
    atomic_store(lock, SPINLOCK_FREE, ATOMIC_RELEASE);
}

static __always_inline __nodiscard bool spinlock_acquire_intr(spinlock_type* restrict lock)
{
    bool intr_state = disable_interrupts();
    spinlock_acquire(lock);
    return intr_state;
}

static __always_inline void spinlock_release_intr(spinlock_type* restrict lock, bool intr_state)
{
    spinlock_release(lock);
    restore_interrupts(intr_state);
}

#endif /* A6A7BDE1_6F01_427C_80B6_14BB87B2C2AD */
