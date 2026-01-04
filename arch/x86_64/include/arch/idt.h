#ifndef _ARCH_IDT_H
#define _ARCH_IDT_H 1

#include <vex/compiler.h>

#include <stdint.h>

#define IDT_SIZE 256

extern void set_idt_entry_kern(unsigned int vector, int is_trap, const void* restrict pfn);
extern void set_idt_entry_user(unsigned int vector, int is_trap, const void* restrict pfn);
extern void unset_idt_entry(unsigned int vector);

extern void init_idt(void);

#endif
