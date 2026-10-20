#pragma once

#include <arch/hart.h>
#include <vex/atomic.h>
#include <vex/compiler.h>
#include <vex/spinlock.h>

#define SEQLOCK_INIT { .sequence = 0, .writer = SPINLOCK_INIT }

typedef struct seqlock_s seqlock_t;

struct seqlock_s {
    unsigned sequence; // Odd while a writer is inside
    spinlock_t writer;
};

static __always_inline __nodiscard unsigned seqlock_read_begin(const seqlock_t* restrict lock)
{
    for(;;) {
        unsigned sequence = atomic_load(&lock->sequence, ATOMIC_ACQUIRE);

        if(0 == (sequence & 1)) {
            return sequence;
        }

        hart_spin();
    }
}

static __always_inline __nodiscard bool seqlock_read_retry(const seqlock_t* restrict lock, unsigned sequence)
{
    atomic_thread_fence(ATOMIC_ACQUIRE);

    return sequence != atomic_load(&lock->sequence, ATOMIC_RELAXED);
}

static __always_inline __nodiscard bool seqlock_write_begin(seqlock_t* restrict lock)
{
    bool intr_state = spinlock_acquire_intr(&lock->writer);

    atomic_store(&lock->sequence, lock->sequence + 1, ATOMIC_RELAXED);
    atomic_thread_fence(ATOMIC_RELEASE);

    return intr_state;
}

static __always_inline void seqlock_write_end(seqlock_t* restrict lock, bool intr_state)
{
    atomic_store(&lock->sequence, lock->sequence + 1, ATOMIC_RELEASE);

    spinlock_release_intr(&lock->writer, intr_state);
}
