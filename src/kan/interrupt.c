/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#include <kan/errno.h>
#include <kan/interrupt.h>
#include <kan/symbol.h>
#include <stdbool.h>
#include <string.h>

typedef struct interrupt_s {
    bool taken;
    void *arguments[MAX_INTERRUPT_HANDLERS];
    interrupt_handler_t handlers[MAX_INTERRUPT_HANDLERS];
} interrupt_t;

static interrupt_t interrupts[MAX_INTERRUPTS] = { 0 };

long alloc_interrupt(long hint)
{
    long vector;

    if(hint < 0 || hint > MAX_INTERRUPTS) {
        /* Caller has no desired vector! */
        hint = 0;
    }

    for(vector = hint; vector < MAX_INTERRUPTS; vector++) {
        if(interrupts[vector].taken)
            continue;
        goto found;
    }

    for(vector = 0; vector < hint; vector++) {
        if(interrupts[vector].taken)
            continue;
        goto found;
    }

    return INVALID_INTERRUPT;

found:
    memset(&interrupts[vector], 0, sizeof(interrupt_t));
    interrupts[vector].taken = true;
    return vector;
}
EXPORT_SYMBOL(alloc_interrupt);

int add_interrupt_handler(long vector, interrupt_handler_t handler, void *restrict arg)
{
    size_t i;
    interrupt_t *interrupt;

    if(vector >= 0 && vector < MAX_INTERRUPTS) {
        interrupt = &interrupts[vector];

        if(interrupt->taken) {
            for(i = 0; i < MAX_INTERRUPT_HANDLERS; i++) {
                if(!interrupt->handlers[i]) {
                    interrupt->handlers[i] = handler;
                    interrupt->arguments[i] = arg;
                    return EOK;
                }
            }

            return ENOMEM;
        }

        return EINVAL;
    }

    return ERANGE;
}
EXPORT_SYMBOL(add_interrupt_handler);

int trigger_interrupt(long vector, interrupt_frame_t *restrict frame)
{
    size_t i;
    interrupt_t *interrupt;
    interrupt_handler_t handler;
    interrupt_frame_t stored_frame;

    if(vector >= 0 && vector < MAX_INTERRUPTS) {
        interrupt = &interrupts[vector];

        if(interrupt->taken) {
            for(i = 0; i < MAX_INTERRUPT_HANDLERS; i++) {
                handler = interrupt->handlers[i];

                if(handler) {
                    memcpy(&stored_frame, frame, sizeof(interrupt_frame_t));

                    /* Handlers that CONSUME the interrupt have the
                     * authority on modifying the interrupt frame. */
                    if(handler(&stored_frame, interrupt->arguments[i]) == INTERRUPT_CONSUME) {
                        memcpy(frame, &stored_frame, sizeof(interrupt_frame_t));
                        break;
                    }
                }
            }

            return EOK;
        }

        return EINVAL;
    }

    return ERANGE;
}
EXPORT_SYMBOL(trigger_interrupt);
