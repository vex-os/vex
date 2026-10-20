#include <vex/atomic.h>
#include <vex/clocksrc.h>
#include <vex/errno.h>
#include <vex/futex.h>
#include <vex/hhdm.h>
#include <vex/sched.h>
#include <vex/spinlock.h>
#include <vex/thread.h>
#include <vex/vms.h>

#define FUTEX_BUCKET_BITS UINT64_C(8)
#define FUTEX_BUCKETS     UINT64_C(256)
#define FUTEX_HASH_FACTOR UINT64_C(0x9E3779B97F4A7C15) // 2^64 divided by the golden ratio

static_assert(FUTEX_BUCKETS == (UINT64_C(1) << FUTEX_BUCKET_BITS));

typedef struct futex_bucket_s futex_bucket_t;
typedef struct futex_waiter_s futex_waiter_t;

struct futex_bucket_s {
    spinlock_t lock;
    futex_waiter_t* head; // Woken in the order they started waiting
    futex_waiter_t* tail;
};

// Lives on the waiting thread's stack: a waker only touches it under the
// bucket's lock, which the waiter takes again before it returns
struct futex_waiter_s {
    futex_waiter_t* next;
    futex_waiter_t* prev;
    const vmspace_t* vmspace;
    uintptr_t address;
    thread_t* thread;
    bool queued;
};

static futex_bucket_t buckets[FUTEX_BUCKETS];

static futex_bucket_t* bucket_of(const vmspace_t* restrict vmspace, uintptr_t address)
{
    uint64_t key = ((uintptr_t)(vmspace) ^ address) * FUTEX_HASH_FACTOR;
    return &buckets[key >> (UINT64_C(64) - FUTEX_BUCKET_BITS)];
}

static void link_locked(futex_bucket_t* restrict bucket, futex_waiter_t* restrict waiter)
{
    waiter->next = nullptr;
    waiter->prev = bucket->tail;
    waiter->queued = true;

    if(bucket->tail) {
        bucket->tail->next = waiter;
    }
    else {
        bucket->head = waiter;
    }

    bucket->tail = waiter;
}

static void unlink_locked(futex_bucket_t* restrict bucket, futex_waiter_t* restrict waiter)
{
    if(waiter->prev) {
        waiter->prev->next = waiter->next;
    }
    else {
        bucket->head = waiter->next;
    }

    if(waiter->next) {
        waiter->next->prev = waiter->prev;
    }
    else {
        bucket->tail = waiter->prev;
    }

    waiter->queued = false;
}

int futex_wait(vmspace_t* restrict vmspace, uintptr_t address, uint32_t expected, uint64_t deadline)
{
    if(address % sizeof(uint32_t)) {
        return -EINVAL;
    }

    uintptr_t phys = vms_pin(vmspace, address);

    if(phys == 0) {
        return -EFAULT;
    }

    thread_t* thread = this_thread();
    futex_bucket_t* bucket = bucket_of(vmspace, address);
    const uint32_t* word = phys_to_virt(phys);

    futex_waiter_t waiter;
    waiter.vmspace = vmspace;
    waiter.address = address;
    waiter.thread = thread;

    bool intr_state = spinlock_acquire_intr(&bucket->lock);

    // A waker changes the word before it takes the lock, so the
    // change is either seen here or the waker finds this waiter
    int retval = 0;

    if(atomic_load(word, ATOMIC_SEQ_CST) != expected) {
        retval = -EAGAIN;
    }
    else if(deadline && deadline <= clock_source_now_ns()) {
        retval = -ETIMEDOUT;
    }

    if(retval < 0) {
        spinlock_release_intr(&bucket->lock, intr_state);
        vms_unpin(phys);
        return retval;
    }

    link_locked(bucket, &waiter);
    sched_dequeue(thread, true);

    if(deadline) {
        sched_wake_at(thread, deadline);
    }

    spinlock_release_intr(&bucket->lock, intr_state);
    vms_unpin(phys);

    sched_yield();

    if(deadline) {
        sched_wake_cancel(thread);
    }

    intr_state = spinlock_acquire_intr(&bucket->lock);

    bool woken = !waiter.queued;

    if(!woken) {
        unlink_locked(bucket, &waiter);
    }

    spinlock_release_intr(&bucket->lock, intr_state);

    if(woken) {
        return 0;
    }

    if(atomic_load(&thread->pending_work, ATOMIC_ACQUIRE)) {
        return -EINTR;
    }

    if(deadline && deadline <= clock_source_now_ns()) {
        return -ETIMEDOUT;
    }

    return 0; // Spurious, the caller looks at the word again anyway
}

size_t futex_wake(const vmspace_t* restrict vmspace, uintptr_t address, size_t count)
{
    futex_bucket_t* bucket = bucket_of(vmspace, address);
    size_t woken = 0;

    bool intr_state = spinlock_acquire_intr(&bucket->lock);

    futex_waiter_t* waiter = bucket->head;

    while(waiter && woken < count) {
        futex_waiter_t* next = waiter->next;

        if(waiter->vmspace == vmspace && waiter->address == address) {
            unlink_locked(bucket, waiter);
            sched_enqueue(waiter->thread);
            woken += 1;
        }

        waiter = next;
    }

    spinlock_release_intr(&bucket->lock, intr_state);

    return woken;
}
