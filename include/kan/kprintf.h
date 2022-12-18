/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef _INCLUDE_KAN_KPRINTF_H__
#define _INCLUDE_KAN_KPRINTF_H__
#include <kan/compiler.h>
#include <uapi/kan/kprintf.h>
#include <stdarg.h>
#include <stddef.h>

extern short kp_loglevel;
extern unsigned long kp_mask;
extern char kp_ring[KP_RING_SIZE];
extern size_t kp_ring_pos;

void kputs(short level, unsigned long source, const char *restrict s);
void kvprintf(short level, unsigned long source, const char *restrict fmt, va_list ap);
void kprintf(short level, unsigned long source, const char *restrict fmt, ...) __printf(3, 4);

#endif /* _INCLUDE_KAN_KPRINTF_H__ */
