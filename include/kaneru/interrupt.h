/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_INTERRUPT_H__
#define __INCLUDE_KANERU_INTERRUPT_H__
#include <kaneru/cdefs.h>
#include <stdbool.h>
#include <stddef.h>

#if defined(__X86_64__)
#include <x86_64/idt.h>
#define MAX_INTERRUPTS X86_IDT_SIZE
#else
#error "Have your heard of the popular hit game among us?"
#endif

typedef void(*interrupt_handler_t)(void);

#define MAX_INTERRUPT_HANDLERS 32
struct interrupt {
    bool is_occupied;
    interrupt_handler_t handlers[MAX_INTERRUPT_HANDLERS];
};

/* used by machine-specific code to invoke handlers */
extern struct interrupt __interrupts[MAX_INTERRUPTS];

bool alloc_interrupt(int *intvec, int wish_intvec);
bool add_interrupt_handler(int intvec, interrupt_handler_t fn);

#endif /* __INCLUDE_KANERU_INTERRUPT_H__ */
