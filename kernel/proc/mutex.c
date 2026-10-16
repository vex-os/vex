#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/hart.h>
#include <vex/mutex.h>
#include <vex/slab.h>
#include <vex/thread.h>

static_assert(sizeof(struct mutex) <= SLAB_MAX_SIZE);

void mutex_init(struct mutex* restrict mtx)
{
    mtx->lock = SPINLOCK_INIT;
    mtx->owner = nullptr;
    mtx->num_waiters = 0;

    event_init(&mtx->released);
}

void mutex_deinit(struct mutex* restrict mtx)
{
    kassert(mtx->owner == nullptr);
    kassert(mtx->num_waiters == 0);

    event_deinit(&mtx->released);
}

bool mutex_try_acquire(struct mutex* restrict mtx)
{
    struct thread* self = this_thread();

    bool intr_state = spinlock_acquire_intr(&mtx->lock);

    kassert(mtx->owner != self);

    bool acquired = mtx->owner == nullptr;

    if(acquired) {
        mtx->owner = self;
    }

    spinlock_release_intr(&mtx->lock, intr_state);

    return acquired;
}

void mutex_acquire(struct mutex* restrict mtx)
{
    struct thread* self = this_thread();

    bool intr_state = spinlock_acquire_intr(&mtx->lock);

    kassert(mtx->owner != self);

    while(mtx->owner) {
        // Nothing can switch away from a caller
        // with interrupts disabled, such as kmain
        // during boot, so it waits for the owner on another hart
        if(!intr_state) {
            spinlock_release_intr(&mtx->lock, intr_state);
            hart_spin();
            intr_state = spinlock_acquire_intr(&mtx->lock);
            continue;
        }

        mtx->num_waiters += 1;
        spinlock_release_intr(&mtx->lock, intr_state);

        struct event* released = &mtx->released;
        event_await_blocking(&released, 1);

        intr_state = spinlock_acquire_intr(&mtx->lock);
        mtx->num_waiters -= 1;
    }

    mtx->owner = self;

    spinlock_release_intr(&mtx->lock, intr_state);
}

void mutex_release(struct mutex* restrict mtx)
{
    bool intr_state = spinlock_acquire_intr(&mtx->lock);

    kassert(mtx->owner == this_thread());

    mtx->owner = nullptr;
    bool contended = mtx->num_waiters > 0;

    spinlock_release_intr(&mtx->lock, intr_state);

    // Pending wakeups only build up while
    // someone waits, and those waiters consume
    // them; an uncontended mutex never leaves any behind
    if(contended) {
        event_trigger(&mtx->released, true);
    }
}

bool mutex_held(const struct mutex* restrict mtx)
{
    return atomic_load(&mtx->owner, ATOMIC_RELAXED) == this_thread();
}
