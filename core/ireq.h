// SPDX-License-Identifier: BSD-3-Clause
#ifndef C1213E0C_C387_4AFF_BEC8_12A1F442D94B
#define C1213E0C_C387_4AFF_BEC8_12A1F442D94B
#pragma once

#include <core/cdefs.h>
#include <stdint.h>

struct interrupt_frame;

typedef void (*ireq_handler_type)(unsigned number, struct interrupt_frame* restrict frame, void* restrict privdata);

extern unsigned register_ireq_handler(unsigned desired_number, ireq_handler_type handler, void* restrict privdata) __nodiscard;
extern void unregister_ireq_handler(unsigned number);
extern void dispatch_ireq(unsigned number, struct interrupt_frame* restrict frame);

#endif /* C1213E0C_C387_4AFF_BEC8_12A1F442D94B */
