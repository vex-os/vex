#pragma once

#include <vex/compiler.h>

#define IDT_SIZE 256

void set_idt_entry_kern(unsigned vector, bool trap, const void* restrict pfn);
void set_idt_entry_user(unsigned vector, bool trap, const void* restrict pfn);
void unset_idt_entry(unsigned vector);
