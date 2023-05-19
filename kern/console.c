/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <kan/console.h>
#include <kan/errno.h>
#include <kan/system.h>

#define CONS_HISTORY_SIZE 4096

static console_t *console_drivers = NULL;
static char history[CONS_HISTORY_SIZE] = { 0 };
static size_t history_pos = 0;

int register_console(console_t *restrict cons)
{
    console_t *it;

    for(it = console_drivers; it; it = it->next) {
        if(it != cons)
            continue;
        return EBUSY;
    }

    kprintf("console: registering %s", cons->ident);

    if((cons->flags & CONSF_UNBLANK) && cons->unblank) {
        /* Typically you would expect a newly registered
         * console_t instance to start printing stuff right
         * away instead of patiently waiting for a kernel panic */
        cons->unblank(cons);
    }

    if((cons->flags & CONSF_PRINTHIST) && cons->write) {
        /* We maintain a history ringbuffer in order for
         * the newly registered log_sink instance to have some
         * information printed there in case shit blows up right after */
        cons->write(cons, &history[history_pos], (CONS_HISTORY_SIZE - history_pos - 1));
        cons->write(cons, &history[0], history_pos);
    }

    cons->next = console_drivers;
    console_drivers = cons;

    return 0;
}

int unregister_console(console_t *restrict cons)
{
    console_t *it;
    console_t *prev;

    if(console_drivers == cons) {
        console_drivers = cons->next;
        kprintf("console: unregistering %s", cons->ident);
        return 0;
    }

    prev = NULL;
    for(it = console_drivers; it; it = it->next) {
        if(it == cons) {
            if(prev)
                prev->next = it->next;
            kprintf("console: unregistering %s", cons->ident);
            return 0;
        }

        prev = it;
    }

    return EINVAL;
}

void console_putchar(int c)
{
    console_t *it;
    char value = c;

    for(it = console_drivers; it; it = it->next) {
        if(!it->write)
            continue;
        it->write(it, &value, 1);
    }

    history[history_pos++] = c;
    history_pos %= CONS_HISTORY_SIZE;
}

void console_unblank(void)
{
    console_t *it;

    for(it = console_drivers; it; it = it->next) {
        if(!it->unblank)
            continue;
        it->unblank(it);
    }
}
