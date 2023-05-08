/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <kan/errno.h>
#include <stddef.h>
#include <string.h>
#include <sys/console.h>
#include <sys/systm.h>

#define CONSOLE_RING_SIZE 4096
static char console_ring[CONSOLE_RING_SIZE] = { 0 };
static size_t console_ring_pos = 0;
static console_t *consoles = NULL;

int register_console(console_t *restrict console)
{
    size_t i;
    console_t *it;

    for(it = consoles; it; it = it->next) {
        if(it != console && strcmp(it->ident, console->ident))
            continue;
        return EBUSY;
    }

    kprintf("console: registering %s", console->ident);

    if(console->unblank) {
        console->unblank(console);
    }

    if(console->putchar) {
        for(i = console_ring_pos; i < CONSOLE_RING_SIZE; i++) {
            if(!console_ring[i])
                continue;
            console->putchar(console, console_ring[i]);
        }

        for(i = 0; i < console_ring_pos; i++) {
            if(!console_ring[i])
                continue;
            console->putchar(console, console_ring[i]);
        }
    }

    console->next = consoles;
    consoles = console;

    return 0;
}

void unregister_console(console_t *restrict console)
{
    console_t *it;
    console_t *prev;

    if(consoles == console) {
        consoles = console->next;
        kprintf("console: unregistering %s", console->ident);
        return;
    }

    prev = NULL;
    for(it = consoles; it; it = it->next) {
        if(it == console) {
            if(prev)
                prev->next = it->next;
            kprintf("console: unregistering %s", console->ident);
            return;
        }

        prev = it;
    }
}

void console_putchar(int c)
{
    console_t *it;

    for(it = consoles; it; it = it->next) {
        if(it->putchar) {
            it->putchar(it, c);
        }
    }

    console_ring[console_ring_pos++] = c;
    console_ring_pos %= CONSOLE_RING_SIZE;
}

void console_unblank(void)
{
    console_t *it;

    for(it = consoles; it; it = it->next) {
        if(it->unblank) {
            it->unblank(it);
        }
    }
}
