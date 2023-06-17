/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <kern/console.h>
#include <kern/systm.h>

#define CONSOLE_HISTORY_SIZE 4096
static struct console *consoles = NULL;
static char console_history[CONSOLE_HISTORY_SIZE] = { 0 };
static size_t console_writepos = 0;

int register_console(struct console *restrict con)
{
    size_t i;
    struct console *it;

    for(it = consoles; it; it = it->con_next) {
        if(it != con)
            continue;
        return EBUSY;
    }

    kprintf("console: registering %s", con->con_ident);

    if(con->con_unblank) {
        /* Typically you'd expect a newly registered
         * console to be ready to output something
         * right away instead of waiting for a kernel panic */
        con->con_unblank(con);
    }

    if(con->con_putchar) {
        for(i = console_writepos; i < CONSOLE_HISTORY_SIZE; ++i) {
            if(!console_history[i])
                continue;
            con->con_putchar(con, console_history[i]);
        }

        for(i = 0; i < console_writepos; ++i) {
            if(!console_history[i])
                continue;
            con->con_putchar(con, console_history[i]);
        }
    }

    con->con_next = consoles;
    consoles = con;

    return 0;
}

int unregister_console(struct console *restrict con)
{
    struct console *it;
    struct console *prev = NULL;

    if(consoles == con) {
        consoles = consoles->con_next;
        kprintf("console: unregistering %s", con->con_ident);
        return 0;
    }

    for(it = consoles; it; it = it->con_next) {
        if(it == con) {
            if(prev)
                prev->con_next = it->con_next;
            kprintf("console: unregistering %s", con->con_ident);
            return 0;
        }
    }

    return EINVAL;
}

void console_putchar(int c)
{
    struct console *it;
    for(it = consoles; it; it = it->con_next) {
        if(!it->con_putchar)
            continue;
        it->con_putchar(it, c);
    }
}

void console_unblank(void)
{
    struct console *it;
    for(it = consoles; it; it = it->con_next) {
        if(!it->con_unblank)
            continue;
        it->con_unblank(it);
    }
}
