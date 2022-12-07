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

#define KP_UNDEFINED    0xFFFFFFFF
#define KP_MACHINE      0x00000001
#define KP_INITCALL     0x00000002
#define KP_INTERRUPT    0x00000004
#define KP_RESOURCE     0x00000008

typedef void(*kp_callback_t)(const void *restrict, size_t);

unsigned long kp_get_mask(void);
unsigned long kp_set_mask(unsigned long mask);
void kp_set_callback(kp_callback_t callback);

void kputs_impl(unsigned long source, const char *restrict file, unsigned long line, const char *restrict s);
void kvprintf_impl(unsigned long source, const char *restrict file, unsigned long line, const char *restrict fmt, va_list ap);
void kprintf_impl(unsigned long source, const char *restrict file, unsigned long line, const char *restrict fmt, ...) __format(printf, 4, 5);

#define kputs(source, s) ({kputs_impl((source),__FILE__,__LINE__,(s));})
#define kvprintf(source, fmt, ap) ({kvprintf_impl((source),__FILE__,__LINE__,(fmt),(ap));})
#define kprintf(source, fmt, ...) ({kprintf_impl((source),__FILE__,__LINE__,(fmt),##__VA_ARGS__);})

#endif /* __INCLUDE_KANERU_KPRINTF_H__ */
