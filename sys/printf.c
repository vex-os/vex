/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <pfmt.h>
#include <stddef.h>
#include <sys/errno.h>
#include <sys/printf.h>

#define HISTORY_SIZE 4096
static struct console *consoles = NULL;
static char history[HISTORY_SIZE] = { 0 };
static size_t history_write = 0;

static void func(int c, void *restrict arg)
{
    struct console *it;

    for(it = consoles; it; it = it->con_next) {
        if(!it->con_putchar)
            continue;
        it->con_putchar(it, c);
    }

    history[history_write++] = c;
    history_write %= HISTORY_SIZE;
}

int add_console(struct console *restrict con)
{
    size_t i;
    struct console *it;

    for(it = consoles; it; it = it->con_next) {
        if(it != con)
            continue;
        return EBUSY;
    }

    kprintf("console: adding %s", con->con_identity);

    if(con->con_unblank) {
        /* Typically you'd expect a newly registered
         * console to be ready to output something
         * right away instead of waiting for a kernel panic */
        con->con_unblank(con);
    }

    if(con->con_putchar) {
        for(i = history_write; i < HISTORY_SIZE; ++i) {
            if(!history[i])
                continue;
            con->con_putchar(con, history[i]);
        }

        for(i = 0; i < history_write; ++i) {
            if(!history[i])
                continue;
            con->con_putchar(con, history[i]);
        }
    }

    con->con_next = consoles;
    consoles = con;

    return 0;
}

int remove_console(struct console *restrict con)
{
    struct console *it;
    struct console *prev = NULL;

    if(consoles == con) {
        consoles = consoles->con_next;
        kprintf("console: removing %s", con->con_identity);
        return 0;
    }

    for(it = consoles; it; it = it->con_next) {
        if(it == con) {
            if(prev)
                prev->con_next = it->con_next;
            kprintf("console: removing %s", con->con_identity);
            return 0;
        }
    }

    return EINVAL;
}

void unblank_consoles(void)
{
    struct console *it;

    for(it = consoles; it; it = it->con_next) {
        if(!it->con_unblank)
            continue;
        it->con_unblank(it);
    }
}

void kputchar(int c)
{
    struct console *it;

    for(it = consoles; it; it = it->con_next) {
        if(!it->con_putchar)
            continue;
        it->con_putchar(it, c);
    }

    history[history_write++] = c;
    history_write %= HISTORY_SIZE;
}

void kputs(const char *restrict s)
{
    size_t i;
    for(i = 0; s[i]; kputchar(s[i++]));
    kputchar('\r');
    kputchar('\n');
}

void kprintf(const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vpfmt(&func, NULL, fmt, ap);
    kputchar('\r');
    kputchar('\n');
    va_end(ap);
}

void kvprintf(const char *restrict fmt, va_list ap)
{
    vpfmt(&func, NULL, fmt, ap);
    kputchar('\r');
    kputchar('\n');
}
