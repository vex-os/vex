/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/errno.h>
#include <string.h>
#include <sys/console.h>
#include <sys/system.h>

static console_t *console_drivers = NULL;
static console_t *primary_console = NULL;

int console_register(console_t *restrict console)
{
    size_t i;
    console_t *it;

    for(it = console_drivers; it; it = it->next) {
        if(it != console && strcmp(it->name, console->name))
            continue;
        return EBUSY;
    }

    if(console->write) {
        for(i = 0; i < kprintf_history_size; i++) {
            console->write(console, kprintf_history[i], strnlen(kprintf_history[i], KPRINTF_MESSAGE_SZ));
            console->write(console, "\r\n", 2);
        }
    }

    console->next = console_drivers;
    console_drivers = console;

    return 0;
}

void console_unregister(console_t *restrict console)
{
    console_t *it;
    console_t *prev;

    if(console_drivers == console) {
        console_drivers = console->next;
        return;
    }

    prev = NULL;
    for(it = console_drivers; it; it = it->next) {
        if(it == console) {
            if(prev)
                prev->next = it->next;
            return;
        }

        prev = it;
    }
}

void console_primary(console_t *restrict console)
{
    primary_console = console;
}

void console_write(const void *restrict s, size_t n)
{
    console_t *it;
    for(it = console_drivers; it; it = it->next) {
        if(it->write) {
            it->write(it, s, n);
        }
    }
}

void console_fcprintf_func(int c, void *restrict arg)
{
    char chr = c;
    console_t *it;
    for(it = console_drivers; it; it = it->next) {
        if(it->write) {
            it->write(it, &chr, 1);
        }
    }
}
