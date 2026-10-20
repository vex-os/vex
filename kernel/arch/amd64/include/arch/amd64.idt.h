#pragma once

#include <vex/compiler.h>

#define IDT_SIZE 256

#define IDT_IST_DOUBLE_FAULT  1
#define IDT_IST_NMI           2
#define IDT_IST_MACHINE_CHECK 3

void idt_set_kernel_gate(unsigned vector, bool trap, const void* restrict pfn);
void idt_set_ist_gate(unsigned vector, unsigned ist, const void* restrict pfn);
void idt_set_user_gate(unsigned vector, bool trap, const void* restrict pfn);
void idt_clear_gate(unsigned vector);
