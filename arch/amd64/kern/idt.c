#include <string.h>
#include <sys.amd64/gdt.h>
#include <sys.amd64/idt.h>
#include <sys.arch/hart.h>
#include <sys.arch/intr.h>
#include <sys.arch/regs.h>
#include <sys/initcall.h>
#include <sys/kprintf.h>
#include <sys/malloc.h>
#include <sys/panic.h>
#include <sys/process.h>
#include <sys/sched.h>
#include <sys/vmm.h>

#define IDT_TRAP    (0x0F << 0)
#define IDT_INTR    (0x0E << 0)
#define IDT_RING_0  (0x00 << 5)
#define IDT_RING_3  (0x03 << 5)
#define IDT_PRESENT (0x01 << 7)

struct idt_entry {
    uint16_t offset_0;
    uint16_t selector;
    uint8_t ist_off;
    uint8_t flags;
    uint16_t offset_1;
    uint32_t offset_2;
    uint32_t reserved;
} __packed;

struct amd64_idt {
    struct idt_entry entries[IDT_SIZE];
    struct {
        uint16_t size;
        uintptr_t offset;
    } __packed idtr;
};

extern void amd64_halt_loop(void);
extern void amd64_isr_00(void);
extern void amd64_isr_01(void);
extern void amd64_isr_02(void);
extern void amd64_isr_03(void);
extern void amd64_isr_04(void);
extern void amd64_isr_05(void);
extern void amd64_isr_06(void);
extern void amd64_isr_07(void);
extern void amd64_isr_08(void);
extern void amd64_isr_09(void);
extern void amd64_isr_0A(void);
extern void amd64_isr_0B(void);
extern void amd64_isr_0C(void);
extern void amd64_isr_0D(void);
extern void amd64_isr_0E(void);
extern void amd64_isr_0F(void);
extern void amd64_isr_10(void);
extern void amd64_isr_11(void);
extern void amd64_isr_12(void);
extern void amd64_isr_13(void);
extern void amd64_isr_14(void);
extern void amd64_isr_15(void);
extern void amd64_isr_16(void);
extern void amd64_isr_17(void);
extern void amd64_isr_18(void);
extern void amd64_isr_19(void);
extern void amd64_isr_1A(void);
extern void amd64_isr_1B(void);
extern void amd64_isr_1C(void);
extern void amd64_isr_1D(void);
extern void amd64_isr_1E(void);
extern void amd64_isr_1F(void);

static void page_fault_handler(struct hart_regs* restrict regs)
{
    struct vmm_fault_info info;
    memset(&info, 0, sizeof(info));

    info.present = (bool)(regs->amd64.err & 0x1);

    if(regs->amd64.err & 0x2) {
        info.vprot = VPROT_WRITE; // tried to write
    }
    else if(regs->amd64.err & 0x10) {
        info.vprot = VPROT_EXEC;
    }
    else {
        info.vprot = VPROT_READ;
    }

    if(regs->amd64.err & 0x4) {
        info.vprot |= VPROT_USER;
    }

    asm volatile("movq %%cr2, %0" : "=r"(info.fault));

    struct hart_local* hart = this_hart();
    struct pagemap* old_vm = &sys_pagemap;
    struct pagemap* new_vm = vmm_handle_fault(old_vm, &info);

    if(new_vm == nullptr) {
        if(info.vprot & VPROT_USER) {
            if(hart->u_curr) {
                pr_warning("process %d: page fault at %p (vprot=%02X) — killing uthread", hart->u_curr->owner->id, (void*)(info.fault),
                    info.vprot);
                uthread_destroy(hart->u_curr);
                hart->u_curr = nullptr;
            }

            // redirect iretq back to kernel ring-0 hlt loop
            // RSP must point to a valid kernel stack — use TSS RSP0 (top of uthread kstack)
            regs->amd64.rip = (uintptr_t)&amd64_halt_loop;
            regs->amd64.cs = gdt_selector(GDT_KERN_CODE_64, 0, 0);
            regs->amd64.ss = gdt_selector(GDT_KERN_DATA_64, 0, 0);
            regs->amd64.rsp = hart->amd64.tss->rsp[0];
            regs->amd64.rflags = (1 << 9); // IF=1

            return;
        }

        panic("unhandled page fault at %p (vprot=%02X)", (void*)(info.fault), info.vprot);
    }
}

void __used amd64_isr_handler(struct hart_regs* restrict regs, uint64_t intvec)
{
    disable_interrupts();
    vmm_switch(&sys_pagemap);

    if(intvec == 0x0E) {
        page_fault_handler(regs);
        return;
    }

    panic("idt: unhandled exception 0x%02zX", (size_t)(intvec));
}

static void set_entry(struct idt_entry* restrict entry, bool trap, uint16_t selector, const void* restrict pfn)
{
    memset(entry, 0, sizeof(*entry));

    entry->offset_0 = ((uintptr_t)pfn & 0x000000000000FFFF);
    entry->offset_1 = ((uintptr_t)pfn & 0x00000000FFFF0000) >> 16;
    entry->offset_2 = ((uintptr_t)pfn & 0xFFFFFFFF00000000) >> 32;
    entry->selector = selector;
    entry->flags = IDT_PRESENT;

    if(trap) {
        entry->flags |= IDT_TRAP;
    }
    else {
        entry->flags |= IDT_INTR;
    }
}

void set_idt_entry_kern(unsigned vector, bool trap, const void* restrict pfn)
{
    if(vector < IDT_SIZE) {
        struct hart_local* hart = this_hart();
        struct amd64_idt* idt = hart->amd64.idt;
        set_entry(&idt->entries[vector], trap, gdt_selector(GDT_KERN_CODE_64, 0, 0), pfn);
        idt->entries[vector].flags |= IDT_RING_0;
    }
}

void set_idt_entry_user(unsigned vector, bool trap, const void* restrict pfn)
{
    if(vector < IDT_SIZE) {
        struct hart_local* hart = this_hart();
        struct amd64_idt* idt = hart->amd64.idt;
        set_entry(&idt->entries[vector], trap, gdt_selector(GDT_KERN_CODE_64, 0, 0), pfn);
        idt->entries[vector].flags |= IDT_RING_3;
    }
}

void unset_idt_entry(unsigned vector)
{
    if(vector < IDT_SIZE) {
        struct hart_local* hart = this_hart();
        struct amd64_idt* idt = hart->amd64.idt;
        memset(&idt->entries[vector], 0, sizeof(struct idt_entry));
    }
}

define_initcall(amd64_idt, INITCALL_STAGE_SMP_ARCH, INITCALL_ORDER_EARLY)
{
    struct amd64_idt* idt = kmalloc(sizeof(struct amd64_idt));

    if(idt == nullptr) {
        panic("out of memory");
    }

    struct hart_local* hart = this_hart();
    hart->amd64.idt = idt;

    set_idt_entry_kern(0x00, true, &amd64_isr_00);
    set_idt_entry_kern(0x01, true, &amd64_isr_01);
    set_idt_entry_kern(0x02, true, &amd64_isr_02);
    set_idt_entry_kern(0x03, true, &amd64_isr_03);
    set_idt_entry_kern(0x04, true, &amd64_isr_04);
    set_idt_entry_kern(0x05, true, &amd64_isr_05);
    set_idt_entry_kern(0x06, true, &amd64_isr_06);
    set_idt_entry_kern(0x07, true, &amd64_isr_07);
    set_idt_entry_kern(0x08, true, &amd64_isr_08);
    set_idt_entry_kern(0x09, true, &amd64_isr_09);
    set_idt_entry_kern(0x0A, true, &amd64_isr_0A);
    set_idt_entry_kern(0x0B, true, &amd64_isr_0B);
    set_idt_entry_kern(0x0C, true, &amd64_isr_0C);
    set_idt_entry_kern(0x0D, true, &amd64_isr_0D);
    set_idt_entry_kern(0x0E, true, &amd64_isr_0E);
    set_idt_entry_kern(0x0F, true, &amd64_isr_0F);
    set_idt_entry_kern(0x10, true, &amd64_isr_10);
    set_idt_entry_kern(0x11, true, &amd64_isr_11);
    set_idt_entry_kern(0x12, true, &amd64_isr_12);
    set_idt_entry_kern(0x13, true, &amd64_isr_13);
    set_idt_entry_kern(0x14, true, &amd64_isr_14);
    set_idt_entry_kern(0x15, true, &amd64_isr_15);
    set_idt_entry_kern(0x16, true, &amd64_isr_16);
    set_idt_entry_kern(0x17, true, &amd64_isr_17);
    set_idt_entry_kern(0x18, true, &amd64_isr_18);
    set_idt_entry_kern(0x19, true, &amd64_isr_19);
    set_idt_entry_kern(0x1A, true, &amd64_isr_1A);
    set_idt_entry_kern(0x1B, true, &amd64_isr_1B);
    set_idt_entry_kern(0x1C, true, &amd64_isr_1C);
    set_idt_entry_kern(0x1D, true, &amd64_isr_1D);
    set_idt_entry_kern(0x1E, true, &amd64_isr_1E);
    set_idt_entry_kern(0x1F, true, &amd64_isr_1F);

    idt->idtr.size = (uint16_t)(sizeof(idt->entries) - 1);
    idt->idtr.offset = (uintptr_t)(&idt->entries[0]);

    asm volatile("lidtq %0" ::"m"(idt->idtr));
}
