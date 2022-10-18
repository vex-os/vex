/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/interrupt.h>

struct interrupt __interrupts[MAX_INTERRUPTS] = { 0 };

bool alloc_interrupt(int *intvec, int wish_intvec)
{
    int i;
    struct interrupt *intr;

    /* force wish_intvec in valid range */
    if(wish_intvec < 0 || wish_intvec >= MAX_INTERRUPTS)
        wish_intvec = 0;

    for(i = wish_intvec; i < MAX_INTERRUPTS; i++) {
        intr = &__interrupts[i];
        if(!intr->is_occupied) {
            intr->is_occupied = true;
            intvec[0] = i;
            return true;
        }
    }

    for(i = 0; i < wish_intvec; i++) {
        intr = &__interrupts[i];
        if(!intr->is_occupied) {
            intr->is_occupied = true;
            intvec[0] = i;
            return true;
        }
    }

    return false;
}

bool add_interrupt_handler(int intvec, interrupt_handler_t fn)
{
    int i;
    struct interrupt *intr;

    if(intvec < 0 || intvec >= MAX_INTERRUPTS || !fn)
        return false;
    intr = &__interrupts[intvec];

    for(i = 0; i < MAX_INTERRUPT_HANDLERS; i++) {
        if(!intr->handlers[i]) {
            intr->handlers[i] = fn;
            return true;
        }
    }

    return false;
}

#include <string.h>
static int init_interrupt(void)
{
    memset(__interrupts, 0, sizeof(__interrupts));
    return 0;
}
initcall_tier_0(interrupt, init_interrupt);
