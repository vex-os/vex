// SPDX-License-Identifier: Zlib
#ifndef INCLUDE_ARCH_INTREQ_H
#define INCLUDE_ARCH_INTREQ_H
#include <arch/intr.h>

typedef void (*intreq_handler_t)(struct interrupt_frame *restrict frame);
int set_intreq_handler(unsigned int vector, intreq_handler_t handler);
int unset_intreq_handler(unsigned int vector);

void init_intreq(void);

#endif // INCLUDE_ARCH_INTREQ_H
