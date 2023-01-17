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
    char *sptr;
    console_t *bcon;

    for(bcon = console_drivers; bcon; bcon = bcon->next) {
        if(bcon != con && strcmp(bcon->name, con->name))
            continue;
        return EBUSY;
    }

#if 0
    /* Certain console drivers may actually set
     * the write callback after registering, so
     * we might not want to bail out here */
    if(!con->write)
        return EINVAL;
#endif

    pr_inform("console: registering %s", con->name);

    if(con->write) {
        sptr = kp_ring + kp_ring_pos;
        con->write(con, sptr, strnlen(sptr, KP_RING_SZ - kp_ring_pos));
        con->write(con, kp_ring, strnlen(kp_ring, kp_ring_pos));
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

void console_write(const void *restrict str, size_t size)
{
    console_t *bcon;
    for(bcon = console_drivers; bcon; bcon = bcon->next) {
        if(!bcon->write)
            continue;
        bcon->write(bcon, str, size);
    }
}
