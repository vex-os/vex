/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_INTERRUPT_H__
#define __INCLUDE_KANERU_INTERRUPT_H__
#include <kaneru/cdefs.h>
#include <kaneru/initcall.h>
#include <kaneru/intvec.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(__X86_64__)
#include <x86_64/idt.h>
#define MAX_INTERRUPTS X86_IDT_SIZE
#else
#error "Have your heard of a popular hit game among us?"
#endif

typedef void(*interrupt_handler_t)(void *restrict frame);

intvec_t alloc_interrupt(intvec_t hint);
void bind_interrupt_handler(intvec_t intvec, interrupt_handler_t handler);
void trigger_interrupt(intvec_t intvec, void *restrict frame);

initcall_extern(interrupt);

#endif /* __INCLUDE_KANERU_INTERRUPT_H__ */
