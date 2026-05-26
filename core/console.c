// SPDX-License-Identifier: BSD-3-Clause

#include <core/assert.h>
#include <core/console.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <lib/string.h>

static struct console_driver* console_head = nullptr;

int register_console_driver(struct console_driver* restrict driver)
{
    struct console_driver* it;

    kassert(driver && "invalid console driver pointer");
    kassert(driver->ops && "invalid console driver ops pointer");

    for(it = console_head; it; it = it->next) {
        if(it == driver)
            return EEXIST;
        continue;
    }

    kprintf(KP_INFORM, "console: adding %s", driver->identity);

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

    driver->next = console_head;
    console_head = driver;

    return 0;
}

int unregister_console_driver(struct console_driver* restrict driver)
{
    struct console_driver* it;
    struct console_driver* prev = nullptr;

    kassert(driver && "invalid console driver pointer");

    if(console_head == driver) {
        console_head = console_head->next;
        kprintf(KP_INFORM, "console: removing %s", driver->identity);
        return 0;
    }

    for(it = console_head; it; it = it->next) {
        if(it == driver) {
            if(prev) {
                prev->next = it->next;
            }

            kprintf(KP_INFORM, "console: removing %s", driver->identity);

            return 0;
        }

        prev = it;
    }

    return EINVAL;
}

void console_write(const void* restrict data, size_t size)
{
    struct console_driver* it;

    kassert(data && "invalid console write data pointer");

    for(it = console_head; it; it = it->next) {
        if(it->ops->write == nullptr)
            continue;
        if(it->consflags & CONSF_BLANKED)
            continue;
        it->ops->write(it, data, size);
    }
}

void console_unblank_all(void)
{
    struct console_driver* it;

    for(it = console_head; it; it = it->next) {
        if(it->ops->unblank)
            it->ops->unblank(it);
        it->consflags &= ~CONSF_BLANKED;
    }
}
