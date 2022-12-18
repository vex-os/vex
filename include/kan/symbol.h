/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef _INCLUDE_KAN_SYMBOL_H__
#define _INCLUDE_KAN_SYMBOL_H__
#include <kan/compiler.h>
#include <uapi/kan/symbol.h>
#include <stdbool.h>
#include <stddef.h>

/* These symbols are exposed manually
 * for certain functions and objects that
 * are going to be useful within a kernel module.
 * FIXME: these are only declared!!!!!
 * UNDONE: kernel modules and whatnot. */
extern symbol_t __ksymtab_begin;
extern symbol_t __ksymtab_end;

/* Unlike section .ksymtab symbols, these
 * are automatically pulled from an intermediate
 * kernel binary (via llvm-nm) and used within
 * backtrace subroutine to walk the stack. */
extern const symbol_t __trace_ksyms[];

int find_symbol(const char *restrict name, symbol_t *restrict sym);
bool trace_address(uintptr_t address, symbol_t *restrict sym, ptrdiff_t *restrict off);

#endif /* _INCLUDE_KAN_SYMBOL_H__ */
