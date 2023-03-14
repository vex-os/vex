#ifndef __INCLUDE_SYS_INTERRUPT_H__
#define __INCLUDE_SYS_INTERRUPT_H__
#include <machine/cpu.h>
#include <machine/idefs.h>
#include <sys/cdefs.h>

// Defined in <machine/idefs.h>
// #define MAX_INTERRUPTS 256
// #define MAX_INTERRUPT_REQUESTS 256

// How many functions can pretend to consume
// an interrupt signal, if there happens to be one.
#define MAX_INTERRUPT_HANDLERS 4

// Returned by alloc_interrupt when it fails
#define INVALID_INTERRUPT_VECTOR ((long)(-1))

// Returns nonzero to notify the kernel that
// the interrupt signal has been consumed
typedef int (*interrupt_handler_t)(cpu_ctx_t *restrict ctx, void *restrict arg);

long allocate_interrupt(long desired_vector);
int add_interrupt_handler(long vector, interrupt_handler_t handler, void *restrict arg);
int trigger_interrupt(long vector, cpu_ctx_t *restrict ctx);

#endif /* __INCLUDE_SYS_INTERRUPT_H__ */
