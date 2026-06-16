#include <vex/assert.h>
#include <vex/console.h>
#include <vex/errno.h>
#include <vex/kprintf.h>
#include <vex/spinlock.h>
#include <vex/string.h>

static spinlock_type console_lock = SPINLOCK_INIT;
static struct console* console_head = nullptr;

int add_console(struct console* restrict cons)
{
    bool ireq_state = spinlock_acquire_intr(&console_lock);

    for(struct console* it = console_head; it; it = it->priv.next) {
        if(it == cons) {
            spinlock_release_intr(&console_lock, ireq_state);
            return -EEXIST;
        }
    }

    spinlock_release_intr(&console_lock, ireq_state);

    pr_verbose("console: adding %s", cons->identifier);

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

    ireq_state = spinlock_acquire_intr(&console_lock);

    cons->priv.next = console_head;
    console_head = cons;

    spinlock_release_intr(&console_lock, ireq_state);

    return 0;
}

int remove_console(struct console* restrict cons)
{
    bool ireq_state = spinlock_acquire_intr(&console_lock);

    if(console_head == cons) {
        console_head = console_head->priv.next;

        spinlock_release_intr(&console_lock, ireq_state);

        pr_verbose("console: removing %s", cons->identifier);

        return 0;
    }

    struct console* prev = nullptr;

    for(struct console* it = console_head; it; it = it->priv.next) {
        if(it == cons) {
            if(prev) {
                prev->priv.next = it->priv.next;
            }

            spinlock_release_intr(&console_lock, ireq_state);

            pr_verbose("console: removing %s", cons->identifier);

            return 0;
        }

        prev = it;
    }

    spinlock_release_intr(&console_lock, ireq_state);

    return -EINVAL;
}

void console_write(const void* restrict data, size_t size)
{
    bool ireq_state = spinlock_acquire_intr(&console_lock);

    for(struct console* it = console_head; it; it = it->priv.next) {
        if(it->ops->write == nullptr)
            continue;
        if(it->consflags & CONSF_BLANKED)
            continue;
        it->ops->write(it, data, size);
    }

    spinlock_release_intr(&console_lock, ireq_state);
}

void console_unblank(void)
{
    bool ireq_state = spinlock_acquire_intr(&console_lock);

    for(struct console* it = console_head; it; it = it->priv.next) {
        if(it->ops->unblank)
            it->ops->unblank(it);
        it->consflags &= ~CONSF_BLANKED;
    }

    spinlock_release_intr(&console_lock, ireq_state);
}
