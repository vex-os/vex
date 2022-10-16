/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/errno.h>
#include <kaneru/kprintf.h>
#include <kaneru/syscon.h>
#include <sprintf.h>
#include <string.h>

struct message {
    char message[KP_MSG_LENGTH];
    size_t length;
};

static unsigned long mask = KP_NOTHING;
static struct message ring[KP_MSG_COUNT] = { 0 };
static unsigned long first = 0;
static unsigned long last = 0;

static void push_message(const struct message *msg)
{
    struct message *target = &ring[last];
    memcpy(target, msg, sizeof(struct message));

    console_write(target->message, target->length);

    first = last++ / KP_MSG_COUNT;
    last %= KP_MSG_COUNT;
}

unsigned long kp_getmask(void)
{
    return mask;
}

unsigned long kp_setmask(unsigned long new_mask)
{
    unsigned long old_mask = mask;
    mask = new_mask;
    return old_mask;
}

void kp_flush_buffer(struct sys_console *console)
{
    unsigned long i;
    const struct message *msg;

    for(i = 0; i < KP_MSG_COUNT; i++) {
        if(console->write_fn) {
            msg = &ring[(i + last) % KP_MSG_COUNT];
            console->write_fn(console, msg->message, msg->length);
        }
    }
}

void kputs(unsigned long origin, const char *restrict s)
{
    static const char crlf[] = "\r\n";
    struct message msg = { 0 };

    if(!(origin & mask)) {
        strncpy_k(msg.message, s, sizeof(msg.message) - sizeof(crlf));
        strncat_k(msg.message, crlf, sizeof(msg.message));
        msg.length = strlen(msg.message);
        push_message(&msg);
    }
}

void kvprintf(unsigned long origin, const char *restrict fmt, va_list ap)
{
    char buffer[KP_MSG_LENGTH] = { 0 };
    if(!(origin & mask)) {
        vsnprintf(buffer, sizeof(buffer), fmt, ap);
        kputs(origin, buffer);
    }
}

void kprintf(unsigned long origin, const char *restrict fmt, ...)
{
    va_list ap;
    if(!(origin & mask)) {
        va_start(ap, fmt);
        kvprintf(origin, fmt, ap);
        va_end(ap);
    }
}
