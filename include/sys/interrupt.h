/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_SYS_INTERRUPT_H__
#define __INCLUDE_SYS_INTERRUPT_H__
#include <machine/cpu.h>
#include <machine/inttab.h>
#include <stdbool.h>
#include <sys/cdefs.h>

#define MAX_INTERRUPT_HANDLERS 4
#define INVALID_INTERRUPT ((long)(-1))
typedef bool (*interrupt_handler_t)(cpu_ctx_t *restrict ctx, void *restrict arg);

long alloc_interrupt(long desired_vector);
int add_interrupt_handler(long vector, interrupt_handler_t handler, void *restrict arg);
int trigger_interrupt(long vector, cpu_ctx_t *restrict ctx);

#endif /* __INCLUDE_SYS_INTERRUPT_H__ */
