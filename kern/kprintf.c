/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/kprintf.h>
#include <sprintf.h>
#include <string.h>

#define KP_BUFSIZE 16384

unsigned short kp_level = 20;
static kp_callback_t kp_callback = NULL;
static void *kp_callback_arg = NULL;
static char kp_buffer[KP_BUFSIZE] = { 0 };
static size_t kp_writepos = 0;

void kprintf_set_callback(kp_callback_t callback, void *arg)
{
    kp_callback = callback;
    kp_callback_arg = arg;

    if(kp_callback) {
        kp_callback(kp_buffer + kp_writepos, KP_BUFSIZE - kp_writepos, kp_callback_arg);
        kp_callback(kp_buffer, kp_writepos, kp_callback_arg);
    }
}

size_t kprintf(unsigned short level, const char *fmt, ...)
{
    size_t r;
    va_list va;

    if(level < kp_level) {
        va_start(va, fmt);
        r = kvprintf(level, fmt, va);
        va_end(va);
        return r;
    }

    return 0;
}

size_t kvprintf(unsigned short level, const char *fmt, va_list va)
{
    size_t r, n, i;
    char buffer[1024] = { 0 };
    const char crlf[3] = "\r\n";

    if(level < kp_level) {
        r = vsnprintf(buffer, sizeof(buffer) - sizeof(crlf), fmt, va);

        /* CRLF is a redundancy measure for
         * picky ANSI-compatible terminals that
         * just move the cursor down without actually
         * returning it to the leftmost position. */
        kstrncat(buffer, crlf, sizeof(buffer));

        n = strlen(buffer);

        if(kp_callback)
            kp_callback(buffer, n, kp_callback_arg);

        for(i = 0; i < n; i++) {
            kp_buffer[kp_writepos++] = buffer[i];
            kp_writepos %= KP_BUFSIZE;
        }

        return r;
    }

    return 0;
}
