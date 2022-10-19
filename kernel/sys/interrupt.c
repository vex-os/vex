/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/interrupt.h>
#include <string.h>

#define MAX_HANDLERS 32
struct interrupt {
    bool occupied;
    size_t num_handlers;
    interrupt_handler_t handlers[MAX_HANDLERS];
};

static struct interrupt interrupts[MAX_INTERRUPTS];

intvec_t alloc_interrupt(intvec_t hint)
{
    intvec_t intvec;

    if(hint < 0 || hint >= MAX_INTERRUPTS)
        hint = 0;

    for(intvec = hint; intvec < MAX_INTERRUPTS; intvec++) {
        if(!interrupts[intvec].occupied) {
            memset(&interrupts[intvec], 0, sizeof(interrupts[intvec]));
            interrupts[intvec].occupied = true;
            interrupts[intvec].num_handlers = 0;
            return intvec;
        }
    }

    for(intvec = 0; intvec < hint; intvec++) {
        if(!interrupts[intvec].occupied) {
            memset(&interrupts[intvec], 0, sizeof(interrupts[intvec]));
            interrupts[intvec].occupied = true;
            interrupts[intvec].num_handlers = 0;
            return intvec;
        }
    }

    return INTVEC_NULL;
}

bool bind_interrupt_handler(intvec_t intvec, interrupt_handler_t handler)
{
    struct interrupt *intr;

    if(intvec < 0 || intvec >= MAX_INTERRUPTS)
        return false;

    if(!handler)
        return false;

    intr = &interrupts[intvec];

    if(!intr->occupied)
        return false;
    
    if(intr->num_handlers >= MAX_HANDLERS)
        return false;

    intr->handlers[intr->num_handlers++] = handler;

    return true;
}

bool trigger_interrupt(intvec_t intvec, void *restrict frame)
{
    size_t i;
    struct interrupt *intr;

    if(intvec < 0 || intvec >= MAX_INTERRUPTS)
        return false;

    intr = &interrupts[intvec];

    if(!intr->occupied)
        return false;

    for(i = 0; i < intr->num_handlers; i++)
        intr->handlers[i](frame);

    return true;
}

static int init_interrupt(void)
{
    memset(interrupts, 0, sizeof(interrupts));
    return 0;
}
initcall_tier_0(interrupt, init_interrupt);
