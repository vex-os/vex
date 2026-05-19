#ifndef C1213E0C_C387_4AFF_BEC8_12A1F442D94B
#define C1213E0C_C387_4AFF_BEC8_12A1F442D94B

#include <core/cdefs.h>
#include <stdint.h>

struct interrupt_frame;

typedef void (*ireq_handler_t)(unsigned int vector, struct interrupt_frame* restrict frame);

extern int register_ireq_handler(unsigned int vector, ireq_handler_t handler);
extern int unregister_ireq_handler(unsigned int vector);
extern void dispatch_ireq(unsigned int vector, struct interrupt_frame* restrict frame);

extern void init_ireq(void);

#endif /* C1213E0C_C387_4AFF_BEC8_12A1F442D94B */
