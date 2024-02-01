/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#include <kern/console.h>
#include <kern/printf.h>
#include <sys/vx/errno.h>

static struct console *cons = NULL;

int register_console(struct console *restrict con)
{
    size_t i;
    struct console *it;

    for(it = cons; it; it = it->co_next) {
        if(it != con)
            continue;
        return EBUSY;
    }

    kprintf(KP_INFORM, "console: registering %s", con->co_name);

    if(con->co_unblank) {
        /* Unblank callback makes the driver to
         * perform sort of a soft-init. With this in
         * mind, the code assumes the driver has already
         * hard-inited before calling register_console on itself. */
        con->co_unblank(con);
    }

    /* NOTE: register_console prioritizes CON_PRINTBUFFER
     * and dumps kmsg buffer contents disregarding CON_BLANKED. */
    if(con->co_putchar && (con->co_flags & CON_PRINTBUFFER)) {
        for(i = kmsg_writepos; i < KMSG_SIZE; ++i) {
            if(!kmsg[i])
                continue;
            con->co_putchar(con, kmsg[i]);
        }

        for(i = 0; i < kmsg_writepos; ++i) {
            if(!kmsg[i])
                continue;
            con->co_putchar(con, kmsg[i]);
        }
    }

    con->co_next = cons;
    cons = con;

    return 0;
}

int unregister_console(struct console *restrict con)
{
    struct console *it;
    struct console *prev = NULL;

    if(cons == con) {
        cons = cons->co_next;
        kprintf(KP_INFORM, "console: unregistering %s", con->co_name);
        return 0;
    }

    for(it = cons; it; it = it->co_next) {
        if(it == con) {
            if(prev)
                prev->co_next = it->co_next;
            kprintf(KP_INFORM, "console: unregistering %s", con->co_name);
            return 0;
        }

        prev = it;
    }

    return EINVAL;
}

void console_putchar(int ch)
{
    struct console *it;
    for(it = cons; it; it = it->co_next) {
        if(!it->co_putchar || (it->co_flags & CON_BLANKED))
            continue;
        it->co_putchar(it, ch);
    }
}

void console_unblank(void)
{
    struct console *it;
    for(it = cons; it; it = it->co_next) {
        if(it->co_unblank)
            it->co_unblank(it);
        it->co_flags &= ~CON_BLANKED;
    }
}
