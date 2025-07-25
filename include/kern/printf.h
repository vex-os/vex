// SPDX-License-Identifier: BSD-2-Clause
#ifndef INCLUDE_KERN_PRINTF_H
#define INCLUDE_KERN_PRINTF_H
#include <kern/compiler.h>
#include <stdarg.h>
#include <stddef.h>

#if !defined(KMSG_SIZE)
#define KMSG_SIZE 4096
#endif

#define KP_EMERG    0x0000U
#define KP_ALERT    0x0001U
#define KP_CRITICAL 0x0002U
#define KP_ERROR    0x0003U
#define KP_WARNING  0x0004U
#define KP_NOTICE   0x0005U
#define KP_INFORM   0x0006U
#define KP_DEBUG    0x0007U

extern size_t kmsg_writepos;
extern char kmsg[KMSG_SIZE];
extern unsigned int loglevel;

void kputs(unsigned int sv, const char* restrict str);
void kprintf(unsigned int sv, const char* restrict fmt, ...) __printflike(2, 3);
void kvprintf(unsigned int sv, const char* restrict fmt, va_list ap) __printflike(2, 0);

void init_printf(void);

#endif // INCLUDE_KERN_PRINTF_H
