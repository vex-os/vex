/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/errno.h>
#include <kan/interrupt.h>
#include <kan/kprintf.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

typedef struct interrupt_s {
    bool is_allocated;
    size_t num_handlers;
    interrupt_handler_t handlers[MAX_INTERRUPT_HANDLERS];
} interrupt_t;

static interrupt_t interrupts[MAX_INTERRUPTS] = { 0 };

long alloc_interrupt(long hint)
{
    long vector;

    if(hint < 0 || hint > MAX_INTERRUPTS) {
        /* no desired vector */
        hint = 0;
    }

    /* try to allocate after the hint */
    for(vector = hint; vector < MAX_INTERRUPTS; vector++) {
        if(!interrupts[vector].is_allocated) {
            memset(&interrupts[vector], 0, sizeof(interrupt_t));
            interrupts[vector].is_allocated = true;
            interrupts[vector].num_handlers = 0;
            return vector;
        }
    }

    /* try to allocate before the hint */
    for(vector = 0; vector < hint; vector++) {
        if(!interrupts[vector].is_allocated) {
            memset(&interrupts[vector], 0, sizeof(interrupt_t));
            interrupts[vector].is_allocated = true;
            interrupts[vector].num_handlers = 0;
            return vector;
        }
    }

    pr_error(KP_INTERRUPT, "interrupt: out of vectors!");

    return NULL_INTERRUPT;
}

int add_interrupt_handler(long vector, interrupt_handler_t handler)
{
    interrupt_t *interrupt;

    if(vector < 0 || vector >= MAX_INTERRUPTS) {
        pr_warning(KP_INTERRUPT, "interrupt: vector <%ld> out of range", vector);
        return -ERANGE;
    }

    interrupt = &interrupts[vector];
    if(!interrupt->is_allocated) {
        pr_warning(KP_INTERRUPT, "interrupt: vector <%ld> is not allocated", vector);
        return -EINVAL;
    }

    if(interrupt->num_handlers >= MAX_INTERRUPT_HANDLERS) {
        pr_warning(KP_INTERRUPT, "interrupt: vector <%ld> is out of handlers", vector);
        return -ENOMEM;
    }

    interrupt->handlers[interrupt->num_handlers++] = handler;
    return 0;
}

void trigger_interrupt(long vector, interrupt_frame_t *restrict frame)
{
    size_t i;
    interrupt_t *interrupt;

    if(vector < 0 || vector >= MAX_INTERRUPTS) {
        pr_warning(KP_INTERRUPT, "interrupt: vector <%ld> out of range", vector);
        return;
    }

    interrupt = &interrupts[vector];
    if(!interrupt->is_allocated) {
        pr_warning(KP_INTERRUPT, "interrupt: vector <%ld> is not allocated", vector);
        return;
    }

    for(i = 0; i < interrupt->num_handlers; interrupt->handlers[i++](frame));
}
