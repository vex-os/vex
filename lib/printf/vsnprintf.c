/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <printf.h>

typedef struct buffer_s {
    char *ptr;
    size_t length;
    size_t pos;
} buffer_t;

static void func(int c, void *restrict arg)
{
    buffer_t *buffer = arg;
    if(buffer->ptr && buffer->pos < buffer->length)
        buffer->ptr[buffer->pos] = c;
    buffer->pos++;
}

int vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list ap)
{
    buffer_t buffer = { 0 };
    buffer.ptr = s;
    buffer.length = n;
    buffer.pos = 0;
    return fcvprintf(&func, NULL, fmt, ap);
}
