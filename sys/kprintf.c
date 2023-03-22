/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/errno.h>
#include <printf.h>
#include <string.h>
#include <sys/kprintf.h>

#define KPRINTF_RING_SIZE 4096
static char kprintf_ring[KPRINTF_RING_SIZE] = { 0 };
static size_t kprintf_ring_pos = 0;
static console_t *consoles = NULL;

static void func(int c, void *restrict arg)
{
    console_t *it;

    for(it = consoles; it; it = it->cn_next) {
        if(it->cn_putchar) {
            it->cn_putchar(it, c);
        }
    }

    kprintf_ring[kprintf_ring_pos++] = c;
    kprintf_ring_pos %= KPRINTF_RING_SIZE;
}

int register_console(console_t *restrict console)
{
    size_t i;
    console_t *it;

    // Ensure we don't have this console
    // or a console with the same name already
    for(it = consoles; it; it = it->cn_next) {
        if(it != console && strcmp(it->cn_name, console->cn_name))
            continue;
        return EBUSY;
    }

    // Notify the rest of console drivers
    // about a new challenger approaching
    kprintf("kprintf: register_console %s", console->cn_name);

    // Unblank the console
    if(console->cn_unblank) {
        console->cn_unblank(console);
    }

    // Dump the contents of kprintf_history
    if(console->cn_putchar) {
        for(i = kprintf_ring_pos; i < KPRINTF_RING_SIZE; i++) {
            if(!kprintf_ring[i])
                continue;
            console->cn_putchar(console, kprintf_ring[i]);
        }

        for(i = 0; i < kprintf_ring_pos; i++) {
            if(!kprintf_ring[i])
                continue;
            console->cn_putchar(console, kprintf_ring[i]);
        }
    }

    console->cn_next = consoles;
    consoles = console;

    return 0;
}

void unregister_console(console_t *restrict console)
{
    console_t *it;
    console_t *prev;

    // If we are the last registered,
    // the task can be simplified drastically.
    if(consoles == console) {
        consoles = console->cn_next;
        kprintf("kprintf: unregister_console %s", console->cn_name);
        return;
    }

    prev = NULL;
    for(it = consoles; it; it = it->cn_next) {
        if(it == console) {
            if(prev)
                prev->cn_next = it->cn_next;
            kprintf("kprintf: unregister_console %s", console->cn_name);
            return;
        }

        prev = it;
    }
}

void console_unblank(void)
{
    console_t *it;
    for(it = consoles; it; it = it->cn_next) {
        if(it->cn_unblank) {
            it->cn_unblank(it);
        }
    }
}

int putchar(int c)
{
    console_t *it;

    for(it = consoles; it; it = it->cn_next) {
        if(it->cn_putchar) {
            it->cn_putchar(it, c);
        }
    }

    kprintf_ring[kprintf_ring_pos++] = c;
    kprintf_ring_pos %= KPRINTF_RING_SIZE;

    return c;
}

int puts(const char *restrict s)
{
    while(*s)
        putchar(*s++);
    putchar('\n');
    return 0;
}

int printf(const char *restrict fmt, ...)
{
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}

int vprintf(const char *restrict fmt, va_list ap)
{
    return fcvprintf(&func, NULL, fmt, ap);
}

int kprintf(const char *restrict fmt, ...)
{
    int r;
    va_list ap;
    va_start(ap, fmt);
    r = kvprintf(fmt, ap);
    va_end(ap);
    return r;
}

int kvprintf(const char *restrict fmt, va_list ap)
{
    int r = fcvprintf(&func, NULL, fmt, ap);
    putchar('\n');
    return r;
}
