#ifndef A6A7BDE1_6F01_427C_80B6_14BB87B2C2AD
#define A6A7BDE1_6F01_427C_80B6_14BB87B2C2AD
#pragma once

#include <sys.arch/hart.h>
#include <sys/atomic.h>
#include <sys/types.h>

typedef uintptr_t spinlock_type;
#define SPINLOCK_INIT ((spinlock_type)(0))
#define SPINLOCK_FREE ((spinlock_type)(0))
#define SPINLOCK_LOCK ((spinlock_type)(1))

static __always_inline __used void spinlock_acquire(spinlock_type* restrict lock)
{
    spinlock_type expected;

    for(;;) {
        expected = SPINLOCK_FREE;

        if(atomic_cmpxchg(lock, &expected, SPINLOCK_LOCK, ATOMIC_ACQUIRE, ATOMIC_RELAXED)) {
            return;
        }

        while(SPINLOCK_LOCK == atomic_load(lock, ATOMIC_RELAXED)) {
            yield_hart();
        }
    }
}

static __always_inline __nodiscard __used bool spinlock_try_acquire(spinlock_type* restrict lock)
{
    spinlock_type expected = SPINLOCK_FREE;
    return atomic_cmpxchg(lock, &expected, SPINLOCK_LOCK, ATOMIC_ACQUIRE, ATOMIC_RELAXED);
}

static __always_inline __used void spinlock_release(spinlock_type* restrict lock)
{
    atomic_store(lock, SPINLOCK_FREE, ATOMIC_RELEASE);
}

#endif /* A6A7BDE1_6F01_427C_80B6_14BB87B2C2AD */
