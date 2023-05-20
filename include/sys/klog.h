/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_KLOG_H__
#define __INCLUDE_SYS_KLOG_H__
#include <stdarg.h>
#include <stddef.h>
#include <sys/cdefs.h>

enum {
    LOG_EMERG   = 0x0000,
    LOG_ALERT   = 0x0001,
    LOG_CRIT    = 0x0002,
    LOG_ERROR   = 0x0003,
    LOG_WARNING = 0x0004,
    LOG_NOTICE  = 0x0005,
    LOG_INFO    = 0x0006,
    LOG_DEBUG   = 0x0007,
    LOG_MAX     = 0xFFFF,
};

#define KLOG_HISTORY_SIZE 64
#define KLOG_MESSAGE_SIZE 256

typedef struct klog_sink_s {
    struct klog_sink_s *next;
    void (*write)(struct klog_sink_s *restrict sink, const void *restrict s, size_t n);
    void (*unblank)(struct klog_sink_s *restrict sink);
    char ident[64];
    void *data;
} klog_sink_t;

typedef char klog_message_t[KLOG_MESSAGE_SIZE];

extern unsigned short loglevel;
extern klog_message_t klog_history[KLOG_HISTORY_SIZE];
extern size_t klog_history_pos;
extern klog_sink_t *klog_sinks;

int register_klog_sink(klog_sink_t *restrict sink);
int unregister_klog_sink(klog_sink_t *restrict sink);
void klog(unsigned short severity, const char *restrict fmt, ...) __printflike(2, 3);
void kvlog(unsigned short severity, const char *restrict fmt, va_list ap) __printflike(2, 0);

#endif /* __INCLUDE_SYS_KLOG_H__ */
