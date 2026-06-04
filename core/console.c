#include <core/assert.h>
#include <core/console.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <core/spinlock.h>
#include <lib/string.h>

static spinlock_type console_lock = SPINLOCK_INIT;
static struct console_driver* console_head = nullptr;

int console_driver_register(struct console_driver* restrict driver)
{
    struct console_driver* it;

    spinlock_acquire(&console_lock);

    for(it = console_head; it; it = it->priv.next) {
        if(it == driver) {
            spinlock_release(&console_lock);
            return -EEXIST;
        }
    }

    spinlock_release(&console_lock);

    pr_debug("console: adding %s", driver->identifier);

    if(driver->ops->init) {
        driver->ops->init(driver);
    }

    if(driver->ops->unblank) {
        driver->ops->unblank(driver);
    }

    if(driver->ops->write && (driver->consflags & CONSF_RECALL)) {
        const char* kptr;
        size_t ksz;

        kptr = &kp_buffer[kp_writepos];
        ksz = strnlen(kptr, KP_BUFSIZE - kp_writepos);
        driver->ops->write(driver, kptr, ksz);

        kptr = &kp_buffer[0];
        ksz = strnlen(kptr, kp_writepos);
        driver->ops->write(driver, kptr, ksz);
    }

    spinlock_acquire(&console_lock);

    driver->priv.next = console_head;
    console_head = driver;

    spinlock_release(&console_lock);

    return 0;
}

int console_driver_unregister(struct console_driver* restrict driver)
{
    struct console_driver* it;

    spinlock_acquire(&console_lock);

    if(console_head == driver) {
        console_head = console_head->priv.next;

        spinlock_release(&console_lock);

        pr_debug("console: removing %s", driver->identifier);

        return 0;
    }

    struct console_driver* prev = nullptr;

    for(it = console_head; it; it = it->priv.next) {
        if(it == driver) {
            if(prev) {
                prev->priv.next = it->priv.next;
            }

            spinlock_release(&console_lock);

            pr_debug("console: removing %s", driver->identifier);

            return 0;
        }

        prev = it;
    }

    spinlock_release(&console_lock);

    return -EINVAL;
}

void console_write(const void* restrict data, size_t size)
{
    struct console_driver* it;

    spinlock_acquire(&console_lock);

    for(it = console_head; it; it = it->priv.next) {
        if(it->ops->write == nullptr)
            continue;
        if(it->consflags & CONSF_BLANKED)
            continue;
        it->ops->write(it, data, size);
    }

    spinlock_release(&console_lock);
}

void console_unblank_all(void)
{
    struct console_driver* it;

    spinlock_acquire(&console_lock);

    for(it = console_head; it; it = it->priv.next) {
        if(it->ops->unblank)
            it->ops->unblank(it);
        it->consflags &= ~CONSF_BLANKED;
    }

    spinlock_release(&console_lock);
}
