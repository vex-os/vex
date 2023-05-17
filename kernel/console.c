/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <kan/console.h>
#include <kan/errno.h>
#include <kan/system.h>

#define CONS_HISTORY_SIZE 4096

static console_t *console_drivers = NULL;
static char history[CONS_HISTORY_SIZE] = { 0 };
static size_t history_pos = 0;

int register_console(console_t *restrict console)
{
    console_t *it;

    for(it = console_drivers; it; it = it->next) {
        if(it != console)
            continue;
        return EBUSY;
    }

    kprintf("console: registering %s", console->ident);

    if((console->flags & CONSF_UNBLANK) && console->unblank) {
        /* Typically you would expect a newly registered
         * console_t instance to start printing stuff right
         * away instead of patiently waiting for a kernel panic */
        console->unblank(console);
    }

    if((console->flags & CONSF_PRINTHIST) && console->write) {
        /* We maintain a history ringbuffer in order for
         * the newly registered log_sink instance to have some
         * information printed there in case shit blows up right after */
        console->write(console, &history[history_pos], (CONS_HISTORY_SIZE - history_pos - 1));
        console->write(console, &history[0], history_pos);
    }

    console->next = console_drivers;
    console_drivers = console;

    return 0;
}

int unregister_console(console_t *restrict console)
{
    console_t *it;
    console_t *prev;

    if(console_drivers == console) {
        console_drivers = console->next;
        kprintf("console: unregistering %s", console->ident);
        return 0;
    }

    prev = NULL;
    for(it = console_drivers; it; it = it->next) {
        if(it == console) {
            if(prev)
                prev->next = it->next;
            kprintf("console: unregistering %s", console->ident);
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
