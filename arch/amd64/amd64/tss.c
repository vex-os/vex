#include <amd64/gdt.h>
#include <arch/hart.h>
#include <core/assert.h>
#include <core/kprintf.h>
#include <init/initcall.h>
#include <lib/string.h>
#include <mm/malloc.h>

struct amd64_tss {
    uint32_t reserved_0;
    uint64_t rsp[3];
    uint64_t reserved_1;
    uint64_t ist[AMD64_IST_COUNT];
    uint64_t reserved_2;
    uint16_t reserved_3;
    uint16_t iomap_base;
} __packed;

define_initcall(amd64_tss, INITCALL_STAGE_SMP_ARCH, INITCALL_ORDER_MIDDLE)
{
    struct amd64_tss* tss = kmalloc(sizeof(struct amd64_tss));
    strict_kassert_text(tss, "out of memory");
    memset(tss, 0, sizeof(*tss));

    tss->iomap_base = sizeof(*tss);

    struct hart_local* hart = this_hart();

    for(size_t i = 0; i < AMD64_IST_COUNT; ++i) {
        if(hart->amd64.ist_stacks[i]) {
            tss->ist[i] = hart->amd64.ist_stacks[i];
        }
    }

    hart->amd64.tss = tss;

    gdt_install_tss(hart->amd64.gdt, (uintptr_t)tss, sizeof(struct amd64_tss));

    asm volatile("ltr %0" ::"r"(gdt_selector(GDT_TSS, 0, 0)));
}
