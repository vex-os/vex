/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_KPRINTF_H__
#define __INCLUDE_KANERU_KPRINTF_H__
#include <kaneru/cdefs.h>
#include <stdarg.h>
#include <stddef.h>

#define KP_EVERYTHING   0xFFFFFFFF /* all the mask bits set */
#define KP_NOTHING      0x00000000 /* all the mask bits clear */

#define KP_UNDEFINED    0xFFFFFFFF /* undefined origin */
#define KP_MACHINE      0x00000001 /* machine-specific */
#define KP_INITIAL      0x00000002 /* initialization */
#define KP_CONSOLE      0x00000004 /* system console */
#define KP_INTERRUPT    0x00000008 /* interrupt handling */

#define KP_MSG_COUNT    16
#define KP_MSG_LENGTH   1024

unsigned long kp_getmask(void);
unsigned long kp_setmask(unsigned long new_mask);

struct sys_console;
void kp_flush_buffer(struct sys_console *console);

void kputs(unsigned long origin, const char *restrict s);
void kvprintf(unsigned long origin, const char *restrict fmt, va_list ap);
void kprintf(unsigned long origin, const char *restrict fmt, ...) __format(printf, 2, 3);

#endif /* __INCLUDE_KANERU_KPRINTF_H__ */
