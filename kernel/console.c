/* SPDX-License-Identifier: GPL-2.0-only */
#include <string.h>
#include <sys/console.h>
#include <sys/errno.h>
#include <sys/printf.h>

static struct console *consoles = NULL;

int register_console(struct console *restrict cons)
{
    size_t ksz;
    const char *kptr;
    struct console *it;

    for(it = consoles; it; it = it->cs_next) {
        if(it != cons)
            continue;
        return EBUSY;
    }

    kprintf(KP_INFORM, "console: registering %s", cons->cs_identity);

    if(cons->cs_unblank) {
        /* Unblank callback makes the driver to
         * perform sort of a soft-init. With this in
         * mind, the code assumes the driver has already
         * hard-inited before calling register_console on itself. */
        cons->cs_unblank(cons);
    }

    /* NOTE: register_console prioritizes CS_PRINTBUFFER
     * and dumps kmsg buffer contents disregarding CS_BLANKED. */
    if(cons->cs_write && (cons->cs_flags & CS_PRINTBUFFER)) {
        kptr = &kmsg[kmsg_writepos];
        ksz = strnlen(kptr, KMSG_SIZE - kmsg_writepos);
        cons->cs_write(cons, kptr, ksz);

        kptr = &kmsg[0];
        ksz = strnlen(kptr, kmsg_writepos);
        cons->cs_write(cons, kptr, ksz);
    }

    cons->cs_next = consoles;
    consoles = cons;

    return 0;
}

int unregister_console(struct console *restrict cons)
{
    struct console *it;
    struct console *prev = NULL;

    if(consoles == cons) {
        consoles = consoles->cs_next;
        kprintf(KP_INFORM, "console: unregistering %s", cons->cs_identity);
        return 0;
    }

    for(it = consoles; it; it = it->cs_next) {
        if(it == cons) {
            if(prev)
                prev->cs_next = it->cs_next;
            kprintf(KP_INFORM, "console: unregistering %s", cons->cs_identity);
            return 0;
        }

        prev = it;
    }

    return EINVAL;
}

void console_write_all(const void *restrict buf, size_t sz)
{
    struct console *it;
    for(it = consoles; it; it = it->cs_next) {
        if(!it->cs_write || (it->cs_flags & CS_BLANKED))
            continue;
        it->cs_write(it, buf, sz);
    }
}

void console_unblank_all(void)
{
    struct console *it;
    for(it = consoles; it; it = it->cs_next) {
        if(it->cs_unblank)
            it->cs_unblank(it);
        it->cs_flags &= ~CS_BLANKED;
    }
}
