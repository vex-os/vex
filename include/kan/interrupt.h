/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_KAN_INTERRUPT_H__
#define _INCLUDE_KAN_INTERRUPT_H__
#include <kan/compiler.h>

#if defined(__X86_64__)
#include <x86/interrupt.h>
#define MAX_INTERRUPTS X86_MAX_INTERRUPTS
typedef x86_interrupt_frame_t interrupt_frame_t;
#else
#define MAX_INTERRUPTS 1
typedef void interrupt_frame_t;
#endif

#define MAX_INTERRUPT_HANDLERS 4
#define INVALID_INTERRUPT ((long)(-1))

#define INTERRUPT_PASS 0
#define INTERRUPT_CONSUME 1
typedef int(*interrupt_handler_t)(interrupt_frame_t *restrict frame, void *restrict arg);

long alloc_interrupt(long hint);
int add_interrupt_handler(long vector, interrupt_handler_t handler, void *restrict arg);
int trigger_interrupt(long vector, interrupt_frame_t *restrict frame);

#endif /* _INCLUDE_KAN_INTERRUPT_H__ */
