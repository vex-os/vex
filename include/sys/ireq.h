#ifndef C1213E0C_C387_4AFF_BEC8_12A1F442D94B
#define C1213E0C_C387_4AFF_BEC8_12A1F442D94B
#pragma once

#include <sys/cdefs.h>

struct interrupt_frame;

typedef void (*ireq_handler_type)(unsigned number, const struct interrupt_frame* restrict frame, void* restrict ctxdata);

unsigned add_ireq_handler(unsigned desired_number, ireq_handler_type handler, void* restrict ctxdata);
void remove_ireq_handler(unsigned number);

void dispatch_ireq(unsigned number, const struct interrupt_frame* restrict frame);

#endif /* C1213E0C_C387_4AFF_BEC8_12A1F442D94B */
