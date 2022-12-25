/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_KAN_SYMBOL_H__
#define _INCLUDE_KAN_SYMBOL_H__
#include <kan/compiler.h>
#include <uapi/kan/symbol.h>
#include <stdbool.h>
#include <stddef.h>

/* These symbols are automatically exported
 * from an intermediate kernel executable
 * using llvm-nm and are sorted by address */
extern const symbol_t kernel_syms[];

bool trace_address(uintptr_t address, symbol_t *restrict sym, ptrdiff_t *restrict off);

#endif /* _INCLUDE_KAN_SYMBOL_H__ */
