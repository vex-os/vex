#pragma once

#include <vex/compiler.h>

#define IDT_SIZE 256

#define IDT_IST_DOUBLE_FAULT  1
#define IDT_IST_NMI           2
#define IDT_IST_MACHINE_CHECK 3
#define IDT_IST_PAGE_FAULT    4 // per-thread; amd64_thread::pfault_stack

void set_idt_entry_kern(unsigned vector, bool trap, const void* restrict pfn);
void set_idt_entry_ist(unsigned vector, unsigned ist, const void* restrict pfn);
void set_idt_entry_user(unsigned vector, bool trap, const void* restrict pfn);
void unset_idt_entry(unsigned vector);
