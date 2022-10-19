/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/errno.h>
#include <kaneru/kprintf.h>
#include <sprintf.h>
#include <string.h>

struct kp_message {
    char message[KP_MSG_LENGTH];
    size_t length;
};

static kp_callback_t kp_callbacks[KP_CALLBACKS] = { 0 };
static unsigned long kp_mask = KP_NOTHING;
static struct kp_message kp_ring[KP_MSG_COUNT] = { 0 };
static unsigned long kp_first = 0;
static unsigned long kp_last = 0;

static void push_message(const struct kp_message *msg)
{
    unsigned int i;
    struct kp_message *target = &kp_ring[kp_last];
    memcpy(target, msg, sizeof(struct kp_message));

    for(i = 0; i < KP_CALLBACKS; i++) {
        if(!kp_callbacks[i])
            continue;
        kp_callbacks[i](target->message, target->length);
    }

    kp_first = kp_last++ / KP_MSG_COUNT;
    kp_last %= KP_MSG_COUNT;
}

unsigned long kp_get_mask(void)
{
    return kp_mask;
}

unsigned long kp_set_mask(unsigned long new_mask)
{
    unsigned long old_mask = kp_mask;
    kp_mask = new_mask;
    return old_mask;
}

int kp_bind_callback(unsigned int slot, kp_callback_t callback)
{
    unsigned long i;
    const struct kp_message *msg;

    if(slot < KP_CALLBACKS && callback) {
        kp_callbacks[slot] = callback;
        for(i = 0; i < KP_MSG_COUNT; i++) {
            msg = &kp_ring[(i + kp_last) % KP_MSG_COUNT];
            callback(msg->message, msg->length);
        }

        return 0;
    }

    return -ERANGE;
}

void kputs(unsigned long origin, const char *restrict s)
{
    static const char crlf[] = "\r\n";
    struct kp_message msg = { 0 };

    if(!(origin & kp_mask)) {
        strncpy_k(msg.message, s, sizeof(msg.message) - sizeof(crlf));
        strncat_k(msg.message, crlf, sizeof(msg.message));
        msg.length = strlen(msg.message);
        push_message(&msg);
    }
}

void kvprintf(unsigned long origin, const char *restrict fmt, va_list ap)
{
    char buffer[KP_MSG_LENGTH] = { 0 };
    if(!(origin & kp_mask)) {
        vsnprintf(buffer, sizeof(buffer), fmt, ap);
        kputs(origin, buffer);
    }
}

void kprintf(unsigned long origin, const char *restrict fmt, ...)
{
    va_list ap;
    if(!(origin & kp_mask)) {
        va_start(ap, fmt);
        kvprintf(origin, fmt, ap);
        va_end(ap);
    }
}

static int init_kprintf(void)
{
    memset(kp_callbacks, 0, sizeof(kp_callbacks));
    return 0;
}
initcall_tier_0(kprintf, init_kprintf);
