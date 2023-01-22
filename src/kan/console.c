/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/console.h>
#include <kan/errno.h>
#include <kan/kprintf.h>
#include <kan/symbol.h>
#include <string.h>

static console_t *console_drivers = NULL;

int register_console(console_t *restrict con)
{
    size_t i;
    kp_msg_t *msg;
    console_t *bcon;

    for(bcon = console_drivers; bcon; bcon = bcon->next) {
        if(bcon != con && strcmp(bcon->name, con->name))
            continue;
        return EBUSY;
    }

#if 0
    /* Certain console drivers may actually set
     * the puts_fn callback after registering, so
     * we might not want to bail out here */
    if(!con->puts_fn)
        return EINVAL;
#endif

    pr_inform("console: registering %s", con->name);

    if(con->puts_fn) {
        for(i = 0; i < kp_msg_count; i++) {
            msg = &kp_history[i];
            con->puts_fn(con, msg[0]);
            con->puts_fn(con, "\r\n");
        }
    }

    con->next = console_drivers;
    console_drivers = con;

    return EOK;
}
EXPORT_SYMBOL(register_console);

void unregister_console(console_t *restrict con)
{
    console_t *a;
    console_t *b;

    if(console_drivers) {
        if(console_drivers == con) {
            pr_inform("console: unregistering %s", con->name);
            console_drivers = con->next;
            return;
        }

        for(a = console_drivers->next, b = console_drivers; a; b = a, a = b->next) {
            if(a == con) {
                pr_inform("console: unregistering %s", con->name);
                b->next = a->next;
                return;
            }
        }
    }
}
EXPORT_SYMBOL(unregister_console);

void console_puts(const char *restrict s)
{
    console_t *bcon;
    for(bcon = console_drivers; bcon; bcon = bcon->next) {
        if(bcon->puts_fn) {
            bcon->puts_fn(bcon, s);
            bcon->puts_fn(bcon, "\r\n");
        }
    }
}
