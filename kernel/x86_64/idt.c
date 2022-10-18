/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kaneru/initcall.h>
#include <kaneru/interrupt.h>
#include <kaneru/kprintf.h>
#include <stdint.h>
#include <string.h>
#include <x86_64/gdt.h>
#include <x86_64/idt.h>

#define IDT_TRAP    (15 << 0)
#define IDT_INTR    (14 << 0)
#define IDT_RING_0  ( 0 << 5)
#define IDT_RING_3  ( 3 << 5)
#define IDT_PRESENT ( 1 << 7)

/* UNDONE: move it somewhere*/
struct intr_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector, error;
    uint64_t rip, cs, rflags;
    uint64_t rsp, ss;
} __packed;

struct idt_entry {
    uint16_t offset_0;
    uint16_t selector;
    uint8_t ist_off;
    uint8_t flags;
    uint16_t offset_1;
    uint32_t offset_2;
    uint32_t reserved;
} __packed;

struct idt_pointer {
    uint16_t limit;
    uintptr_t base;
} __packed;

static struct idt_entry idt[X86_IDT_SIZE] = { 0 };
static struct idt_pointer idt_ptr = { 0 };
static int interrupt_map[X86_IDT_SIZE] = { 0 };

/* see sys/gen.x86_64.interrupts.sh */
extern const uintptr_t __interrupt_stubs[X86_IDT_SIZE];

/* called from sys/x86_64/idt_trampoline.S */
void __used __interrupt_handler(struct intr_frame *frame)
{
    //interrupt_handler_t fn;
    const struct interrupt *intr;

    frame->vector %= X86_IDT_SIZE;
    if(interrupt_map[frame->vector] < 0)
        return;

    intr = &__interrupts[interrupt_map[frame->vector]];
    if(!intr->is_occupied)
        return;

    if(intr->handlers[0])
        intr->handlers[0]();
}

static void set_entry(int vector, uintptr_t offset, bool user)
{
    struct idt_entry entry = { 0 };
    entry.offset_0 = offset & 0xFFFF;
    entry.offset_1 = (offset >> 16) & 0xFFFF;
    entry.offset_2 = (offset >> 32) & 0xFFFFFFFF;
    entry.selector = GDT_SEL(GDT_KERN_CODE_64, 0, 0);
    entry.flags = IDT_PRESENT;
    if(vector == 0 || vector == 1 || (vector >= 3 && vector <= 31))
        entry.flags |= IDT_TRAP;
    else
        entry.flags |= IDT_INTR;
    entry.flags |= user ? IDT_RING_3 : IDT_RING_0;
    memcpy(idt + (uint8_t)(vector % X86_IDT_SIZE), &entry, sizeof(entry));
}

void x86_enable_interrupts(void)
{
    asm volatile("cli");
}

void x86_disable_interrupts(void)
{
    asm volatile("sti");
}

bool x86_map_interrupt(int intvec, int idtvec, bool user)
{
    if(intvec < 0 || intvec >= MAX_INTERRUPTS)
        return false;
    if(idtvec < 0 || idtvec >= X86_IDT_SIZE)
        return false;
    if(interrupt_map[idtvec] >= 0)
        return false;

    set_entry(idtvec, __interrupt_stubs[idtvec], user);

    interrupt_map[idtvec] = intvec;

    return true;
}

static int init_idt(void)
{
    int i;

    idt_ptr.limit = (uint16_t)(sizeof(idt) - 1);
    idt_ptr.base = (uintptr_t)(&idt[0]);

    for(i = 0; i < X86_IDT_SIZE; i++)
        set_entry(i, __interrupt_stubs[i], false);

    asm volatile("lidtq %0"::"m"(idt_ptr));

    kprintf(KP_MACHINE, "idt: limit=%hu, base=%p", idt_ptr.limit, (void *)idt_ptr.base);

    return 0;
}
initcall_tier_0(x86_idt, init_idt);
initcall_depend(x86_idt, x86_gdt);
