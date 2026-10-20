#pragma once

#include <vex/compiler.h>

typedef struct hart_context_s hart_context_t;

/// Runs in interrupt context with interrupts disabled
typedef void (*irq_handler_t)(unsigned number, const hart_context_t* restrict ctx, void* restrict ctxdata);

/// @param desired_number The number wanted; when it is taken or out of range, as UINT_MAX is, any free one
/// @return The number the handler got, or UINT_MAX when none is free
unsigned irq_handler_add(unsigned desired_number, irq_handler_t handler, void* restrict ctxdata);
void irq_handler_remove(unsigned number);

void irq_dispatch(unsigned number, const hart_context_t* restrict ctx);
