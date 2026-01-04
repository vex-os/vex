#ifndef _ARCH_IREQ_H
#define _ARCH_IREQ_H 1

#include <arch/intr.h>

typedef void (*ireq_handler_t)(struct interrupt_frame* restrict frame);

extern int add_ireq_handler(unsigned int vector, ireq_handler_t handler);
extern int remove_ireq_handler(unsigned int vector, ireq_handler_t handler);

extern void init_ireq(void);

#endif
