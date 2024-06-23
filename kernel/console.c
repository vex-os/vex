/* SPDX-License-Identifier: Zlib */
#include <vex/console.h>
#include <vex/errno.h>
#include <vex/printf.h>
#include <string.h>

static struct console *cons = NULL;

int register_console(struct console *restrict con)
{
    size_t ksz;
    const char *kptr;
    struct console *it;

    for(it = cons; it; it = it->cs_next) {
        if(it != con)
            continue;
        return EBUSY;
    }

    kprintf(KP_INFORM, "console: registering %s", con->cs_identity);

    if(con->cs_unblank) {
        /* Unblank callback makes the driver to
         * perform sort of a soft-init. With this in
         * mind, the code assumes the driver has already
         * hard-inited before calling register_console on itself. */
        con->cs_unblank(con);
    }

    /* NOTE: register_console prioritizes CS_PRINTBUFFER
     * and dumps kmsg buffer contents disregarding CS_BLANKED. */
    if(con->cs_write && (con->cs_flags & CS_PRINTBUFFER)) {
        kptr = &kmsg[kmsg_writepos];
        ksz = strnlen(kptr, KMSG_SIZE - kmsg_writepos);
        con->cs_write(con, kptr, ksz);

        kptr = &kmsg[0];
        ksz = strnlen(kptr, kmsg_writepos);
        con->cs_write(con, kptr, ksz);
    }

    con->cs_next = cons;
    cons = con;

    return 0;
}

int unregister_console(struct console *restrict con)
{
    struct console *it;
    struct console *prev = NULL;

    if(cons == con) {
        cons = cons->cs_next;
        kprintf(KP_INFORM, "console: unregistering %s", con->cs_identity);
        return 0;
    }

    for(it = cons; it; it = it->cs_next) {
        if(it == con) {
            if(prev)
                prev->cs_next = it->cs_next;
            kprintf(KP_INFORM, "console: unregistering %s", con->cs_identity);
            return 0;
        }

        prev = it;
    }

    return EINVAL;
}

void console_write_all(const void *restrict buf, size_t sz)
{
    struct console *it;
    for(it = cons; it; it = it->cs_next) {
        if(!it->cs_write || (it->cs_flags & CS_BLANKED))
            continue;
        it->cs_write(it, buf, sz);
    }
}

void console_unblank_all(void)
{
    struct console *it;
    for(it = cons; it; it = it->cs_next) {
        if(it->cs_unblank)
            it->cs_unblank(it);
        it->cs_flags &= ~CS_BLANKED;
    }
}
