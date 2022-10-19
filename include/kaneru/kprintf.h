/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_KPRINTF_H__
#define __INCLUDE_KANERU_KPRINTF_H__
#include <kaneru/cdefs.h>
#include <kaneru/initcall.h>
#include <stdarg.h>
#include <stddef.h>

#define KP_EVERYTHING   0xFFFFFFFF /* all the mask bits set */
#define KP_NOTHING      0x00000000 /* all the mask bits clear */

#define KP_UNDEFINED    0xFFFFFFFF /* undefined origin */
#define KP_EARLY        0x00000001 /* early initialization */
#define KP_KPRINTF      0x00000002 /* kprintf and its callbacks */
#define KP_INTERRUPT    0x00000004 /* interrupt handling */

#define KP_MSG_COUNT    16
#define KP_MSG_LENGTH   1024

#define KP_CALLBACKS    32
#define KP_CALLBACK_0   0x00
#define KP_CALLBACK_1   0x01
#define KP_CALLBACK_2   0x02
#define KP_CALLBACK_3   0x03

typedef void(*kp_callback_t)(const void *restrict, size_t);

unsigned long kp_get_mask(void);
unsigned long kp_set_mask(unsigned long new_mask);
int kp_bind_callback(unsigned int slot, kp_callback_t callback);

void kputs(unsigned long origin, const char *restrict s);
void kvprintf(unsigned long origin, const char *restrict fmt, va_list ap);
void kprintf(unsigned long origin, const char *restrict fmt, ...) __format(printf, 2, 3);

initcall_extern(kprintf);

#endif /* __INCLUDE_KANERU_KPRINTF_H__ */
