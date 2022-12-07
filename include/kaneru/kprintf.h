/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_KPRINTF_H__
#define __INCLUDE_KANERU_KPRINTF_H__
#include <kaneru/cdefs.h>
#include <kaneru/initcall.h>
#include <stdarg.h>
#include <stddef.h>

#define KP_EVERYTHING   0xFFFFFFFF
#define KP_NOTHING      0x00000000

#define KP_UNMASKABLE   0x00000000 /* always printed */
#define KP_MACHINE      0x00000001
#define KP_INITCALL     0x00000002
#define KP_INTERRUPT    0x00000004
#define KP_RESOURCE     0x00000008

typedef void(*kp_callback_t)(const void *restrict, size_t);

unsigned long kp_get_mask(void);
unsigned long kp_set_mask(unsigned long mask);
void kp_set_callback(kp_callback_t callback);

void kputs(unsigned long source, const char *restrict s);
void kvprintf(unsigned long source, const char *restrict fmt, va_list ap);
void kprintf(unsigned long source, const char *restrict fmt, ...) __format(printf, 2, 3);

#endif /* __INCLUDE_KANERU_KPRINTF_H__ */
