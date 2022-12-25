#ifndef _INCLUDE_X86_IDT_H__
#define _INCLUDE_X86_IDT_H__
#include <kan/initcall.h>
#include <stdbool.h>
#include <uapi/x86/idt.h>

/* maps a portable interrupt vector to x86 interrupt vector */
int x86_map_interrupt(long vector, unsigned short x86_vector, bool user);

initcall_extern(x86_idt);

#endif /* _INCLUDE_X86_IDT_H__ */
