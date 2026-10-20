#pragma once

#include <vex/compiler.h>

typedef struct hart_context_s hart_context_t;

typedef void (*irq_handler_t)(unsigned number, const hart_context_t* restrict ctx, void* restrict ctxdata);

unsigned irq_handler_add(unsigned desired_number, irq_handler_t handler, void* restrict ctxdata);
void irq_handler_remove(unsigned number);

void irq_dispatch(unsigned number, const hart_context_t* restrict ctx);
