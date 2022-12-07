/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/errno.h>
#include <kaneru/interrupt.h>
#include <kaneru/kprintf.h>
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

    if(hint < 0 || hint >= MAX_INTERRUPTS) {
        /* no desired interrupt */
        hint = 0;
    }

    /* try to allocate the desired interrupt vector */
    for(intvec = hint; intvec < MAX_INTERRUPTS; intvec++) {
        if(!interrupts[intvec].occupied) {
            memset(&interrupts[intvec], 0, sizeof(interrupts[intvec]));
            interrupts[intvec].occupied = true;
            interrupts[intvec].num_handlers = 0;
            return intvec;
        }
    }

    /* try to allocate a free interrupt vector */
    for(intvec = 0; intvec < hint; intvec++) {
        if(!interrupts[intvec].occupied) {
            memset(&interrupts[intvec], 0, sizeof(interrupts[intvec]));
            interrupts[intvec].occupied = true;
            interrupts[intvec].num_handlers = 0;
            return intvec;
        }
    }

    /* that's a bad thing really but not a death sentence */
    kprintf(KP_INTERRUPT, "out of spare interrupt vectors!");

    return INTVEC_NULL;
}

int bind_interrupt_handler(intvec_t intvec, interrupt_handler_t handler)
{
    struct interrupt *intr;

    if(intvec < 0 || intvec >= MAX_INTERRUPTS) {
        kprintf(KP_INTERRUPT, "vector <%ld> is out of range", intvec);
        return -ERANGE;
    }

    intr = &interrupts[intvec];
    if(!intr->occupied){ 
        kprintf(KP_INTERRUPT, "vector <%ld> is not allocated", intvec);
        return -EINVAL;
    }

    if(intr->num_handlers >= MAX_HANDLERS) {
        kprintf(KP_INTERRUPT, "vector <%ld> is out of handlers", intvec);
        return -ENOMEM;
    }

    intr->handlers[intr->num_handlers++] = handler;

    return 0;
}

void trigger_interrupt(intvec_t intvec, void *restrict frame)
{
    size_t i;
    struct interrupt *intr;

    if(intvec < 0 || intvec >= MAX_INTERRUPTS) {
        kprintf(KP_INTERRUPT, "vector <%ld> is out of range", intvec);
        return;
    }

    intr = &interrupts[intvec];
    if(!intr->occupied){ 
        kprintf(KP_INTERRUPT, "vector <%ld> is not allocated", intvec);
        return;
    }

    /* trigger all the registered handlers */
    for(i = 0; i < intr->num_handlers; intr->handlers[i++](frame));
}

static int init_interrupt(void)
{
    memset(interrupts, 0, sizeof(interrupts));
    return 0;
}
initcall_tier_0(interrupt, init_interrupt);
