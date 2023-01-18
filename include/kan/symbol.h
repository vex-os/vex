/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_KAN_SYMBOL_H__
#define _INCLUDE_KAN_SYMBOL_H__
#include <kan/compiler.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct symbol_s {
    uintptr_t address;
    char name[64];
} symbol_t;

#define EXPORT_SYMBOL(symbol) \
    symbol_t __used __section(".rodata.exports") \
        __export_##symbol = { .address = (uintptr_t)(&(symbol)), .name = #symbol }

/* These symbols are exported manually
 * by using a macro, they will be used
 * in kernel modules in the future. */
/* Defined in temp/ldscript.ld */
extern symbol_t __exports_begin;
extern symbol_t __exports_end;

/* These symbols are automatically exported
 * from an intermediate kernel executable
 * using llvm-nm and are sorted by address.
 * UNDONE: multiple symbol tables so kernel
 * modules can export their symbols as well. */
extern const symbol_t __ksymtab[];

const void *get_export(const char *restrict sym);
bool trace_address(uintptr_t address, symbol_t *restrict sym, ptrdiff_t *restrict off);

#endif /* _INCLUDE_KAN_SYMBOL_H__ */
