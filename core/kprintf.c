// SPDX-License-Identifier: BSD-3-Clause

#include <core/assert.h>
#include <core/console.h>
#include <core/kprintf.h>
#include <lib/format.h>
#include <lib/string.h>
#include <time/clocksource.h>

#ifndef KP_DEFAULT_LOGLEVEL
#define KP_DEFAULT_LOGLEVEL KP_DEBUG
#endif

#define LF 0x0A
#define CR 0x0D

#define STAGEBUF_SIZE 32

struct stagebuf {
    char data[STAGEBUF_SIZE];
    size_t size;
    int last;
};

size_t kp_writepos = 0;
char kp_buffer[KP_BUFSIZE];
unsigned int loglevel = KP_DEFAULT_LOGLEVEL;

static void kprintf_format_func(int chr, void* restrict arg)
{
    struct stagebuf* stage = arg;
    stage->last = chr;
    stage->data[stage->size++] = chr;

    if(stage->size >= STAGEBUF_SIZE) {
        console_write(stage->data, STAGEBUF_SIZE);
        memset(stage->data, 0, STAGEBUF_SIZE);
        stage->size = 0;
    }

    kp_buffer[kp_writepos++] = chr;
    kp_writepos %= KP_BUFSIZE;
}

void kprintf(unsigned int sv, const char* restrict fmt, ...)
{
    kassert(fmt && "invalid format string pointer");

    va_list ap;
    va_start(ap, fmt);
    kvprintf(sv, fmt, ap);
    va_end(ap);
}

void kvprintf(unsigned int sv, const char* restrict fmt, va_list ap)
{
    struct stagebuf st = { 0 };
    uint64_t timestamp = 0;
    uint64_t timestamp_s = 0;
    uint64_t timestamp_msx = 0;

    kassert(fmt && "invalid format string pointer");

    if(sv <= loglevel) {
        timestamp = clocksource_read_ns();
        timestamp_s = timestamp / UINT64_C(1000000000);
        timestamp_msx = (timestamp / UINT64_C(100000)) % UINT64_C(10000);

        format(&kprintf_format_func, &st, "[%5ju.%04ju] ", (uintmax_t)(timestamp_s), (uintmax_t)(timestamp_msx));
        vformat(&kprintf_format_func, &st, fmt, ap);

        if(st.last != LF) {
            kprintf_format_func(CR, &st);
            kprintf_format_func(LF, &st);
        }
    }

    if(st.size) {
        console_write(st.data, st.size);
    }
}
