#include <arch/ireq.h>
#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/errno.h>
#include <vex/ireq.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/spinlock.h>

#define IREQ_TABLE_SIZE (ARCH_IREQ_MAX - ARCH_IREQ_BASE + 1)

struct ireq_entry {
    ireq_handler_type handler;
    void* ctxdata;
    unsigned in_flight;
};

static spinlock_type ireq_table_lock = SPINLOCK_INIT;
static struct ireq_entry ireq_table[IREQ_TABLE_SIZE] = { 0 };

static void claim_nolock(unsigned number, ireq_handler_type handler, void* restrict ctxdata)
{
    ireq_table[number].ctxdata = ctxdata;

    atomic_store(&ireq_table[number].handler, handler, ATOMIC_RELEASE);
}

unsigned add_ireq_handler(unsigned desired_number, ireq_handler_type handler, void* restrict ctxdata)
{
    bool intr_state = spinlock_acquire_intr(&ireq_table_lock);

    unsigned number = desired_number;

    if(number >= IREQ_TABLE_SIZE || ireq_table[number].handler) {
        for(number = 0; number < IREQ_TABLE_SIZE; number += 1) {
            if(ireq_table[number].handler == nullptr) {
                break;
            }
        }
    }

    if(number < IREQ_TABLE_SIZE) {
        claim_nolock(number, handler, ctxdata);
    }
    else {
        number = UINT_MAX;
    }

    spinlock_release_intr(&ireq_table_lock, intr_state);

    return number;
}

void remove_ireq_handler(unsigned number)
{
    bool intr_state = spinlock_acquire_intr(&ireq_table_lock);

    atomic_store(&ireq_table[number].handler, nullptr, ATOMIC_SEQ_CST);

    while(atomic_load(&ireq_table[number].in_flight, ATOMIC_SEQ_CST)) {
        hart_spin();
    }

    ireq_table[number].ctxdata = nullptr;

    spinlock_release_intr(&ireq_table_lock, intr_state);
}

void dispatch_ireq(unsigned number, const struct hart_context* restrict ctx)
{
    if(number >= IREQ_TABLE_SIZE) {
        pr_verbose("ireq: spurious IRQ 0x%02X out of range", number);
        return;
    }

    struct ireq_entry* entry = &ireq_table[number];

    atomic_fetch_add(&entry->in_flight, 1, ATOMIC_SEQ_CST);

    ireq_handler_type handler = atomic_load(&entry->handler, ATOMIC_SEQ_CST);

    if(handler) {
        handler(number, ctx, entry->ctxdata);
    }

    atomic_fetch_sub(&entry->in_flight, 1, ATOMIC_RELEASE);

    if(handler == nullptr) {
        pr_verbose("ireq: unhandled IRQ 0x%02X", number);
    }
}
