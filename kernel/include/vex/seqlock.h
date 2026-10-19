#pragma once

#include <arch/hart.h>
#include <vex/atomic.h>
#include <vex/compiler.h>
#include <vex/spinlock.h>

#define SEQLOCK_INIT { .sequence = 0, .writer = SPINLOCK_INIT }

struct seqlock {
    unsigned sequence; // Odd while a writer is inside
    spinlock_type writer;
};

static __always_inline __nodiscard unsigned seqlock_read_begin(const struct seqlock* restrict lock)
{
    for(;;) {
        unsigned sequence = atomic_load(&lock->sequence, ATOMIC_ACQUIRE);

        if(0 == (sequence & 1)) {
            return sequence;
        }

        hart_spin();
    }
}

static __always_inline __nodiscard bool seqlock_read_retry(const struct seqlock* restrict lock, unsigned sequence)
{
    atomic_thread_fence(ATOMIC_ACQUIRE);

    return sequence != atomic_load(&lock->sequence, ATOMIC_RELAXED);
}

static __always_inline __nodiscard bool seqlock_write_begin(struct seqlock* restrict lock)
{
    bool intr_state = spinlock_acquire_intr(&lock->writer);

    atomic_store(&lock->sequence, lock->sequence + 1, ATOMIC_RELAXED);
    atomic_thread_fence(ATOMIC_RELEASE);

    return intr_state;
}

static __always_inline void seqlock_write_end(struct seqlock* restrict lock, bool intr_state)
{
    atomic_store(&lock->sequence, lock->sequence + 1, ATOMIC_RELEASE);

    spinlock_release_intr(&lock->writer, intr_state);
}
