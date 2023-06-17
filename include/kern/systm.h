/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_KERN_SYSTM_H__
#define __INCLUDE_KERN_SYSTM_H__
#include <kan/cdefs.h>
#include <kan/errno.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

extern const char sysname[];
extern const char release[];
extern const char version[];
extern const char machine[];

extern const char start_text[];
extern const char start_rodata[];
extern const char start_data[];
extern const char start_bss[];

extern const char end_text[];
extern const char end_rodata[];
extern const char end_data[];
extern const char end_bss[];

extern uintptr_t highmem;
extern uintptr_t kbase_phys;
extern uintptr_t kbase_virt;

void kputs(const char *restrict s);
void kprintf(const char *restrict fmt, ...) __printflike(1, 2);
void kvprintf(const char *restrict fmt, va_list ap) __printflike(1, 0);

void fpanic(const char *restrict file, long line, const char *restrict fmt, ...) __printflike(3, 4) __noreturn;
void fvpanic(const char *restrict file, long line, const char *restrict fmt, va_list ap) __printflike(3, 0) __noreturn;
#define panic(fmt, ...) fpanic(__FILE__, __LINE__, (fmt), ##__VA_ARGS__)
#define vpanic(fmt, ap) fvpanic(__FILE__, __LINE__, (fmt), (ap))

#define kassert(x) ({if(predict_false(!(x))){panic("assertion failed - %s", #x); unreachable();}})
#define kassertf(x, fmt, ...) ({if(predict_false(!(x))){panic((fmt), ##__VA_ARGS__); unreachable();}})

#endif /* __INCLUDE_KERN_SYSTM_H__ */
