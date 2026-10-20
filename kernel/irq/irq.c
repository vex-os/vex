#include <arch/irq.h>
#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/errno.h>
#include <vex/irq.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/spinlock.h>

#define IRQ_TABLE_SIZE (ARCH_IRQ_MAX - ARCH_IRQ_BASE + 1)

typedef struct irq_entry_s irq_entry_t;

struct irq_entry_s {
    irq_handler_t handler;
    void* ctxdata;
    unsigned in_flight;
};

static spinlock_t irq_table_lock = SPINLOCK_INIT;
static irq_entry_t irq_table[IRQ_TABLE_SIZE] = { 0 };

static void claim_nolock(unsigned number, irq_handler_t handler, void* restrict ctxdata)
{
    irq_table[number].ctxdata = ctxdata;

    atomic_store(&irq_table[number].handler, handler, ATOMIC_RELEASE);
}

unsigned irq_handler_add(unsigned desired_number, irq_handler_t handler, void* restrict ctxdata)
{
    bool intr_state = spinlock_acquire_intr(&irq_table_lock);

    unsigned number = desired_number;

    if(number >= IRQ_TABLE_SIZE || irq_table[number].handler) {
        for(number = 0; number < IRQ_TABLE_SIZE; number += 1) {
            if(irq_table[number].handler == nullptr) {
                break;
            }
        }
    }

    if(number < IRQ_TABLE_SIZE) {
        claim_nolock(number, handler, ctxdata);
    }
    else {
        number = UINT_MAX;
    }

    spinlock_release_intr(&irq_table_lock, intr_state);

    return number;
}

void irq_handler_remove(unsigned number)
{
    bool intr_state = spinlock_acquire_intr(&irq_table_lock);

    atomic_store(&irq_table[number].handler, nullptr, ATOMIC_SEQ_CST);

    while(atomic_load(&irq_table[number].in_flight, ATOMIC_SEQ_CST)) {
        hart_spin();
    }

    irq_table[number].ctxdata = nullptr;

    spinlock_release_intr(&irq_table_lock, intr_state);
}

void irq_dispatch(unsigned number, const hart_context_t* restrict ctx)
{
    if(number >= IRQ_TABLE_SIZE) {
        pr_verbose("irq: spurious IRQ 0x%02X out of range", number);
        return;
    }

    irq_entry_t* entry = &irq_table[number];

    atomic_fetch_add(&entry->in_flight, 1, ATOMIC_SEQ_CST);

    irq_handler_t handler = atomic_load(&entry->handler, ATOMIC_SEQ_CST);

    if(handler) {
        handler(number, ctx, entry->ctxdata);
    }

    atomic_fetch_sub(&entry->in_flight, 1, ATOMIC_RELEASE);

    if(handler == nullptr) {
        pr_verbose("irq: unhandled IRQ 0x%02X", number);
    }
}
