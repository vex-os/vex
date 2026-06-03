#ifndef C1213E0C_C387_4AFF_BEC8_12A1F442D94B
#define C1213E0C_C387_4AFF_BEC8_12A1F442D94B
#pragma once

#include <core/cdefs.h>

struct interrupt_frame;

typedef void (*ireq_handler_type)(unsigned number, struct interrupt_frame* restrict frame, void* restrict ctxdata);

extern unsigned ireq_handler_register(unsigned desired_number, ireq_handler_type handler, void* restrict ctxdata);
extern void ireq_handler_unregister(unsigned number);
extern void dispatch_ireq(unsigned number, struct interrupt_frame* restrict frame);

#endif /* C1213E0C_C387_4AFF_BEC8_12A1F442D94B */
