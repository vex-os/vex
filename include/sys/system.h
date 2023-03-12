/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_SYSTEM_H__
#define __INCLUDE_SYS_SYSTEM_H__
#include <stdarg.h>
#include <stddef.h>
#include <sys/cdefs.h>

// Defined in /gen/version.c
extern const char sys_build[];
extern const char sys_machine[];
extern const char sys_revision[];
extern const char sys_version[];

#define KPRINTF_MESSAGE_SZ 256
#define KPRINTF_HISTORY_SZ 64
typedef char kprintf_message_t[KPRINTF_MESSAGE_SZ];
extern kprintf_message_t kprintf_history[KPRINTF_HISTORY_SZ];
extern size_t kprintf_history_size;

void kputs(const char *restrict s);
void kprintf(const char *restrict fmt, ...) __printflike(1, 2);
void kvprintf(const char *restrict fmt, va_list ap);

void panic(const char *restrict fmt, ...) __noreturn __printflike(1, 2);
void vpanic(const char *restrict fmt, va_list ap) __noreturn;
#define kassert(x) ({if_unlikely(!(x)){panic("assertion failed: [%s]", #x); UNREACHABLE();}})
#define kassertf(x, fmt, ...) ({if_unlikely(!(x)){panic((fmt), ##__VA_ARGS__); UNREACHABLE();}})
#define panic_if(x, fmt, ...) ({if_unlikely(x){panic((fmt), ##__VA_ARGS__); UNREACHABLE();}})

#endif /* __INCLUDE_SYS_SYSTEM_H__ */
