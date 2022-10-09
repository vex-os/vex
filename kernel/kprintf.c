/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/kprintf.h>
#include <kaneru/syscon.h>
#include <sprintf.h>
#include <string.h>

short kprintf_level = 20;
static char kprintf_buffer[KPRINTF_BUFSIZE] = { 0 };
static size_t kprintf_writepos = 0;

size_t kprintf(short level, const char *fmt, ...)
{
    size_t r;
    va_list va;

    if(level < kprintf_level) {
        va_start(va, fmt);
        r = kvprintf(level, fmt, va);
        va_end(va);
        return r;
    }

    return 0;
}

size_t kvprintf(short level, const char *fmt, va_list va)
{
    size_t r, i, n;
    char buf[1024] = { 0 };

    if(level < kprintf_level) {
        r = vsnprintf(buf, sizeof(buf) - 3, fmt, va);

        /* Appending CRLF is a redundancy measure
         * for some picky terminals that when
         * encountering a LF just move the cursor
         * down without returning it (CR) */
        kstrncat(buf, "\r\n", sizeof(buf));
        syscon_write(buf, n = strlen(buf));

        for(i = 0; i < n; i++) {
            kprintf_buffer[kprintf_writepos++] = buf[i];
            kprintf_writepos %= KPRINTF_BUFSIZE;
        }

        return r;
    }

    return 0;
}

void kprintf_flush_syscon(struct syscon *console)
{
    if(console && console->write) {
        console->write(console, kprintf_buffer + kprintf_writepos, KPRINTF_BUFSIZE - kprintf_writepos);
        console->write(console, kprintf_buffer, kprintf_writepos);
    }
}
