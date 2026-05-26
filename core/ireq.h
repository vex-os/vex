// SPDX-License-Identifier: BSD-3-Clause
#ifndef C1213E0C_C387_4AFF_BEC8_12A1F442D94B
#define C1213E0C_C387_4AFF_BEC8_12A1F442D94B
#pragma once

#include <core/cdefs.h>
#include <stdint.h>

struct interrupt_frame;

typedef void (*ireq_handler_type)(unsigned int vector, struct interrupt_frame* restrict frame, void* restrict ctx);

extern int register_ireq_handler(unsigned int vector, ireq_handler_type handler, void* restrict ctx);
extern int unregister_ireq_handler(unsigned int vector);
extern void dispatch_ireq(unsigned int vector, struct interrupt_frame* restrict frame);

extern void init_ireq(void);

#endif /* C1213E0C_C387_4AFF_BEC8_12A1F442D94B */
