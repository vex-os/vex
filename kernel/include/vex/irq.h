#pragma once

#include <vex/compiler.h>

typedef struct hart_context_s hart_context_t;
typedef struct hart_local_s hart_local_t;

/// Runs in interrupt context with interrupts disabled
typedef void (*irq_handler_t)(unsigned number, const hart_context_t* restrict ctx, void* restrict ctxdata);

/// Gives a handler a number that is the same on all harts. IPIs and hart timers use these numbers
/// @param desired_number The number wanted; when it is taken or out of range, as UINT_MAX is, any free one
/// @return The number the handler got, or UINT_MAX when none is free
unsigned irq_handler_add(unsigned desired_number, irq_handler_t handler, void* restrict ctxdata);

/// @post The handler isn't running anywhere and won't be called again
void irq_handler_remove(unsigned number);

/// Gives a handler a number on one hart only. Interrupts routed to that hart use these numbers
/// @pre Interrupts are enabled, because the call can allocate the table of the hart
/// @return The number of the handler, or UINT_MAX when no number is free on the hart or no memory is available
unsigned irq_handler_add_hart(hart_local_t* restrict hart, irq_handler_t handler, void* restrict ctxdata);

/// @post The handler isn't running on the hart and won't be called again
void irq_handler_remove_hart(hart_local_t* restrict hart, unsigned number);

void irq_dispatch(unsigned number, const hart_context_t* restrict ctx);
