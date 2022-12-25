/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_UAPI_KAN_INTERRUPT_H__
#define _INCLUDE_UAPI_KAN_INTERRUPT_H__
#include <kan/compiler.h>

#if defined(__X86_64__)
#include <x86/idt.h>
#define MAX_INTERRUPTS X86_MAX_INTERRUPTS
typedef struct x86_interrupt_frame interrupt_frame_t;
#else
#define MAX_INTERRUPTS 0
typedef void interrupt_frame_t;
#endif

#define MAX_INTERRUPT_HANDLERS 16
#define NULL_INTERRUPT ((long)(-1))

typedef void(*interrupt_handler_t)(interrupt_frame_t *restrict frame);

#endif /* _INCLUDE_UAPI_KAN_INTERRUPT_H__ */
