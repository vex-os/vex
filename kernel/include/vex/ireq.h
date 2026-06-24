#ifndef C1213E0C_C387_4AFF_BEC8_12A1F442D94B
#define C1213E0C_C387_4AFF_BEC8_12A1F442D94B
#pragma once

#include <vex/compiler.h>

struct hart_context;

typedef void (*ireq_handler_type)(unsigned number, const struct hart_context* restrict ctx, void* restrict ctxdata);

unsigned add_ireq_handler(unsigned desired_number, ireq_handler_type handler, void* restrict ctxdata);
void remove_ireq_handler(unsigned number);

void dispatch_ireq(unsigned number, const struct hart_context* restrict ctx);

#endif /* C1213E0C_C387_4AFF_BEC8_12A1F442D94B */
