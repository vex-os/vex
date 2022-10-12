/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#include <sprintf.h>
#include <string.h>
#include <sys/console.h>
#include <sys/klog.h>

#define KL_MSGCOUNT 16
#define KL_MSGSIZE 2048

struct klog_msg {
    char message[KL_MSGSIZE];
    size_t length;
};

static unsigned long kl_mask = KL_MASK_ALL;
static struct klog_msg kl_buf[KL_MSGCOUNT] = { 0 };
static unsigned int kl_first = 0;
static unsigned int kl_last = 0;

static const char *get_basename(const char *s)
{
    const char *ss = strrchr(s, '/');
    return ss ? (ss + 1) : s;
}

static void klog_push(const struct klog_msg *msg)
{
    struct klog_msg *target = &kl_buf[kl_last];
    memcpy(target, msg, sizeof(struct klog_msg));
    console_write(target->message, target->length);

    /* update positions */
    kl_first = kl_last++ / KL_MSGCOUNT;
    kl_last %= KL_MSGCOUNT;
}

unsigned long klog_getmask(void)
{
    return kl_mask;
}

unsigned long klog_setmask(unsigned long mask)
{
    unsigned long save = kl_mask;
    kl_mask = mask;
    return save;
}

void klog_print_all(struct console *con)
{
    unsigned int i;
    const struct klog_msg *msg;
    for(i = 0; i < KL_MSGCOUNT; i++) {
        if(con->write_fn) {
            msg = &kl_buf[(i + kl_last) % KL_MSGCOUNT];
            con->write_fn(con, msg->message, msg->length);
        }
    }
}

void klogv_impl(unsigned long origin, const char *file, unsigned long line, const char *fmt, va_list va)
{
    char tmpbuf[KL_MSGSIZE] = { 0 };
    struct klog_msg msg = { 0 };

    if(kl_mask & KL_MASK(origin)) {
        vsnprintf(tmpbuf, sizeof(tmpbuf), fmt, va);
        snprintf(msg.message, sizeof(msg.message), "%.64s:%lu: %s\r\n", get_basename(file), line, tmpbuf);
        msg.length = strlen(msg.message);
        klog_push(&msg);
    }
}

void klog_impl(unsigned long origin, const char *file, unsigned long line, const char *fmt, ...)
{
    va_list va;

    if(kl_mask & KL_MASK(origin)) {
        va_start(va, fmt);
        klogv_impl(origin, file, line, fmt, va);
        va_end(va);
    }
}
