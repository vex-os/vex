/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <printf.h>
#include <string.h>
#include <sys/console.h>
#include <sys/system.h>

typedef struct message_buffer_s {
    kprintf_message_t message;
    size_t position;
} message_buffer_t;

kprintf_message_t kprintf_history[KPRINTF_HISTORY_SZ] = { 0 };
size_t kprintf_history_size = 0;

static void kprintf_fcprintf_func(int c, void *restrict arg)
{
    message_buffer_t *buffer = arg;

    // Echo to system consoles
    console_fcprintf_func(c, arg);

    // Append to the buffer
    if(buffer->position < KPRINTF_MESSAGE_SZ)
        buffer->message[buffer->position] = c;
    buffer->position++;
}

void kputs(const char *restrict s)
{
    size_t i;

    // Echo to system consoles
    console_write(s, strlen(s));

    // Append a message's part to the history
    kstrncpy(kprintf_history[kprintf_history_size++], s, KPRINTF_MESSAGE_SZ);

    // Shift the history back one chunk if we reached the end
    if(kprintf_history_size >= KPRINTF_HISTORY_SZ) {
        for(i = 1; i < KPRINTF_HISTORY_SZ; i++)
            memcpy(&kprintf_history[i - 1], &kprintf_history[i], sizeof(kprintf_message_t));
        kprintf_history_size = (KPRINTF_HISTORY_SZ - 1);
    }

    // Add the line termination
    console_write("\r\n", 2);
}

void kprintf(const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvprintf(fmt, ap);
    va_end(ap);
}

void kvprintf(const char *restrict fmt, va_list ap)
{
    size_t i;
    message_buffer_t buffer = { 0 };
    buffer.position = 0;

    // Format the message + echo to system consoles
    fcvprintf(&kprintf_fcprintf_func, &buffer, fmt, ap);

    // Append a message's part to history
    memcpy(&kprintf_history[kprintf_history_size++], &buffer.message, sizeof(kprintf_message_t));

    // Shift the history back one chunk if we reached the end
    if(kprintf_history_size >= KPRINTF_HISTORY_SZ) {
        for(i = 1; i < KPRINTF_HISTORY_SZ; i++)
            memcpy(&kprintf_history[i - 1], &kprintf_history[i], sizeof(kprintf_message_t));
        kprintf_history_size = (KPRINTF_HISTORY_SZ - 1);
    }

    // Add the line termination
    console_write("\r\n", 2);
}
