#ifndef A2CB20C7_9ACE_4616_8BDB_6B13E10A34D2
#define A2CB20C7_9ACE_4616_8BDB_6B13E10A34D2

#include <core/cdefs.h>

#include <stdint.h>

#define X86_IDT_SIZE 256

extern void x86_set_idt_entry_kern(unsigned int vector, int is_trap, const void* restrict pfn);
extern void x86_set_idt_entry_user(unsigned int vector, int is_trap, const void* restrict pfn);
extern void x86_unset_idt_entry(unsigned int vector);

extern void x86_init_idt(void);

#endif /* A2CB20C7_9ACE_4616_8BDB_6B13E10A34D2 */
