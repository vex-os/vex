#include <vex/assert.h>
#include <vex/consdrv.h>
#include <vex/errno.h>
#include <vex/kprintf.h>
#include <vex/string.h>

static struct consdrv* consdrv_s = NULL;

extern void write_e9(const char* fmt, ...);

int consdrv_add(struct consdrv* driver)
{
    size_t ksz;
    const char* kptr;
    struct consdrv* it;

    for(it = consdrv_s; it; it = it->next) {
        if(it == driver)
            return EEXIST;
        continue;
    }

    kprintf(KP_INFORM, "consdrv: adding %s", driver->identity);

    kassert(driver->ops);

    if(driver->ops->init) {
        // Initialize the driver
        driver->ops->init(driver);
    }

    if(driver->ops->unblank) {
        // Unblank callback makes the driver to
        // perform sort of a soft-init. With this in
        // mind, the code assumes the driver has already
        // hard-inited before calling add_consdrv on itself.
        driver->ops->unblank(driver);
    }

    // NOTE: add_consdrv prioritizes CONSF_RECALL
    // and dumps kprintf_buffer contents disregarding CONSF_BLANKED
    if(driver->ops->write && (driver->flags & CONSF_RECALL)) {
        kptr = &kprintf_buffer[kprintf_writepos];
        ksz = strnlen(kptr, CONF_KPRINTF_BUFSIZE - kprintf_writepos);
        driver->ops->write(driver, kptr, ksz);

        kptr = &kprintf_buffer[0];
        ksz = strnlen(kptr, kprintf_writepos);
        driver->ops->write(driver, kptr, ksz);
    }

    driver->next = consdrv_s;
    consdrv_s = driver;

    return 0;
}

int consdrv_remove(struct consdrv* driver)
{
    struct consdrv* it;
    struct consdrv* prev = NULL;

    if(consdrv_s == driver) {
        consdrv_s = consdrv_s->next;

        kprintf(KP_INFORM, "consdrv: removing %s", driver->identity);

        return 0;
    }

    for(it = consdrv_s; it; it = it->next) {
        if(it == driver) {
            if(prev) {
                prev->next = it->next;
            }

            kprintf(KP_INFORM, "consdrv: removing %s", driver->identity);

            return 0;
        }

        prev = it;
    }

    return EINVAL;
}

void consdrv_write(const void* restrict data, size_t sz)
{
    struct consdrv* it;

    for(it = consdrv_s; it; it = it->next) {
        if(it->ops->write == NULL)
            continue;
        if(it->flags & CONSF_BLANKED)
            continue;
        it->ops->write(it, data, sz);
    }
}

void consdrv_unblank(void)
{
    struct consdrv* it;

    for(it = consdrv_s; it; it = it->next) {
        if(it->ops->unblank)
            it->ops->unblank(it);
        it->flags &= ~CONSF_BLANKED;
    }
}
