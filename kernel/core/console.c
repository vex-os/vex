#include <vex/assert.h>
#include <vex/console.h>
#include <vex/errno.h>
#include <vex/kprintf.h>
#include <vex/spinlock.h>
#include <vex/string.h>

static spinlock_t console_lock = SPINLOCK_INIT;
static console_t* console_head = nullptr;

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

    if(cons->ops->unblank) {
        cons->ops->unblank(cons);
    }

    if(cons->ops->write && (cons->consflags & CONSF_RECALL)) {
        const char* kptr;
        size_t ksz;

        kptr = &kp_buffer[kp_writepos];
        ksz = strnlen(kptr, KP_BUFSIZE - kp_writepos);
        cons->ops->write(cons, kptr, ksz);

        kptr = &kp_buffer[0];
        ksz = strnlen(kptr, kp_writepos);
        cons->ops->write(cons, kptr, ksz);
    }

    irq_state = spinlock_acquire_intr(&console_lock);

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
    bool irq_state = spinlock_acquire_intr(&console_lock);

    for(console_t* it = console_head; it; it = it->priv.next) {
        if(it->ops->write == nullptr)
            continue;
        if(it->consflags & CONSF_BLANKED)
            continue;
        it->ops->write(it, data, size);
    }

    spinlock_release_intr(&console_lock, irq_state);
}

void console_unblank(void)
{
    bool irq_state = spinlock_acquire_intr(&console_lock);

    for(console_t* it = console_head; it; it = it->priv.next) {
        if(it->ops->unblank)
            it->ops->unblank(it);
        it->consflags &= ~CONSF_BLANKED;
    }

    spinlock_release_intr(&console_lock, irq_state);
}
