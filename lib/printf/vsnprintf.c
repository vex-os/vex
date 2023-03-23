/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <printf.h>

typedef struct buffer_s {
    char *buf_ptr;
    size_t buf_length;
    size_t buf_pos;
} buffer_t;

static void func(int c, void *restrict arg)
{
    buffer_t *buffer = arg;
    if(buffer->buf_ptr && buffer->buf_pos < buffer->buf_length)
        buffer->buf_ptr[buffer->buf_pos] = c;
    buffer->buf_pos++;
}

int vsnprintf(char *restrict s, size_t n, const char *restrict fmt, va_list ap)
{
    int r;
    buffer_t buffer = { 0 };
    buffer.buf_ptr = s;
    buffer.buf_length = n;
    buffer.buf_pos = 0;
    r = fcvprintf(&func, &buffer, fmt, ap);
    buffer.buf_ptr[buffer.buf_length - 1] = 0;
    return r;
}
