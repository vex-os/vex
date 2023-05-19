/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <printf.h>
#include <string.h>
#include <strings.h>
#include <sys/errno.h>
#include <sys/klog.h>

typedef struct klog_buffer_s {
    klog_message_t message;
    size_t write_pos;
} klog_buffer_t;

unsigned short loglevel = LOG_MAX;
klog_message_t klog_history[KLOG_HISTORY_SIZE] = { 0 };
size_t klog_history_pos = 0;
klog_sink_t *klog_sinks = NULL;

static void klog_fcprintf(int c, void *restrict arg)
{
    char cval = c;
    klog_sink_t *it;
    klog_buffer_t *buffer = arg;

    if(buffer->write_pos < KLOG_MESSAGE_SIZE)
        buffer->message[buffer->write_pos] = c;
    buffer->write_pos++;

    for(it = klog_sinks; it; it = it->next) {
        if(!it->write)
            continue;
        it->write(it, &cval, 1);
    }
}

int register_klog_sink(klog_sink_t *restrict sink)
{
    size_t i;
    klog_sink_t *it;

    for(it = klog_sinks; it; it = it->next) {
        if(it != sink)
            continue;
        return EBUSY;
    }

    if(sink->unblank) {
        /* Typically you would expect a newly registered
         * klog_sink_t instance to start printing stuff right
         * away instead of patiently waiting for a kernel panic */
        sink->unblank(sink);
    }

    if(sink->write) {
        /* We maintain a history ringbuffer in order for
         * the newly registered klog_sink_t instance to have some
         * information printed there in case shit blows up right after */
        for(i = 0; i < klog_history_pos; ++i) {
            sink->write(sink, klog_history[i], strnlen(klog_history[i], KLOG_MESSAGE_SIZE));
            sink->write(sink, "\r\n", 2);
        }
    }

    sink->next = klog_sinks;
    klog_sinks = sink;

    return 0;
}

int unregister_klog_sink(klog_sink_t *restrict sink)
{
    klog_sink_t *it;
    klog_sink_t *prev;

    if(klog_sinks == sink) {
        klog_sinks = klog_sinks->next;
        return 0;
    }

    prev = NULL;
    for(it = klog_sinks; it; it = it->next) {
        if(it == sink) {
            if(prev)
                prev->next = it->next;
            return 0;
        }
    }

    return EINVAL;
}

void klog(unsigned short severity, const char *restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    kvlog(severity, fmt, ap);
    va_end(ap);
}

void kvlog(unsigned short severity, const char *restrict fmt, va_list ap)
{
    size_t i;
    klog_buffer_t buf;
    klog_sink_t *it;

    if(severity <= loglevel) {
        memset(buf.message, 0, sizeof(buf.message));
        buf.write_pos = 0;

        fcvprintf(&klog_fcprintf, &buf, fmt, ap);

        for(it = klog_sinks; it; it = it->next) {
            if(!it->write)
                continue;
            it->write(it, "\r\n", 2);
        }

        memcpy(klog_history[klog_history_pos++], buf.message, KLOG_MESSAGE_SIZE);

        if(klog_history_pos >= KLOG_HISTORY_SIZE) {
            for(i = 1; i < KLOG_HISTORY_SIZE; ++i)
                memcpy(klog_history[i - 1], klog_history[i], KLOG_MESSAGE_SIZE);
            memset(klog_history[KLOG_HISTORY_SIZE - 1], 0, KLOG_MESSAGE_SIZE);
            klog_history_pos = (KLOG_HISTORY_SIZE - 1);
        }
    }
}
