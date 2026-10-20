#include <vex/assert.h>
#include <vex/atomic.h>
#include <vex/console.h>
#include <vex/errno.h>
#include <vex/kprintf.h>
#include <vex/panic.h>
#include <vex/spinlock.h>
#include <vex/string.h>

static spinlock_t console_lock = SPINLOCK_INIT;
static console_t* console_head = nullptr;

static void recall_history(console_t* restrict cons)
{
    const char* kptr;
    size_t ksz;

    kptr = &kp_buffer[kp_writepos];
    ksz = strnlen(kptr, KP_BUFSIZE - kp_writepos);
    cons->ops->write(cons, kptr, ksz);

    kptr = &kp_buffer[0];
    ksz = strnlen(kptr, kp_writepos);
    cons->ops->write(cons, kptr, ksz);
}

int console_add(console_t* restrict cons)
{
    bool irq_state = spinlock_acquire_intr(&console_lock);

    for(console_t* it = console_head; it; it = it->priv.next) {
        if(it == cons) {
            spinlock_release_intr(&console_lock, irq_state);
            return -EEXIST;
        }
    }

    spinlock_release_intr(&console_lock, irq_state);

    pr_verbose("console: adding %s", cons->name);

    if(cons->ops->init) {
        cons->ops->init(cons);
    }

    irq_state = spinlock_acquire_intr(&console_lock);

    if(cons->ops->write && (cons->consflags & CONSF_RECALL)) {
        recall_history(cons);
    }

    cons->priv.next = console_head;
    console_head = cons;

    spinlock_release_intr(&console_lock, irq_state);

    return 0;
}

int console_remove(console_t* restrict cons)
{
    bool irq_state = spinlock_acquire_intr(&console_lock);

    if(console_head == cons) {
        console_head = console_head->priv.next;

        spinlock_release_intr(&console_lock, irq_state);

        pr_verbose("console: removing %s", cons->name);

        return 0;
    }

    console_t* prev = nullptr;

    for(console_t* it = console_head; it; it = it->priv.next) {
        if(it == cons) {
            if(prev) {
                prev->priv.next = it->priv.next;
            }

            spinlock_release_intr(&console_lock, irq_state);

            pr_verbose("console: removing %s", cons->name);

            return 0;
        }

        prev = it;
    }

    spinlock_release_intr(&console_lock, irq_state);

    return -EINVAL;
}

void console_write(const void* restrict data, size_t size)
{
    bool irq_state = disable_interrupts();

    // A panic has stopped the other harts, so a holder of the lock is either stopped or this hart
    bool locked = !atomic_load_n(&panic_in_progress, ATOMIC_ACQUIRE);

    if(locked) {
        raw_spinlock_acquire(&console_lock);
    }

    for(console_t* it = console_head; it; it = it->priv.next) {
        if(it->ops->write == nullptr)
            continue;
        if(it->consflags & CONSF_RELEASED)
            continue;
        it->ops->write(it, data, size);
    }

    if(locked) {
        raw_spinlock_release(&console_lock);
    }

    restore_interrupts(irq_state);
}

void console_release(console_t* restrict cons)
{
    bool irq_state = spinlock_acquire_intr(&console_lock);

    if(0 == (cons->consflags & CONSF_RELEASED)) {
        if(cons->ops->release) {
            cons->ops->release(cons);
        }

        cons->consflags |= CONSF_RELEASED;
    }

    spinlock_release_intr(&console_lock, irq_state);
}

void console_takeover_all(void)
{
    for(console_t* it = console_head; it; it = it->priv.next) {
        if(0 == (it->consflags & CONSF_RELEASED))
            continue;
        if(it->ops->takeover && !it->ops->takeover(it))
            continue;

        it->consflags &= ~CONSF_RELEASED;

        if(it->ops->write && (it->consflags & CONSF_RECALL)) {
            recall_history(it);
        }
    }
}
