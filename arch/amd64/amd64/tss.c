// SPDX-License-Identifier: BSD-3-Clause

#include <amd64/gdt.h>
#include <arch/hart.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <init/initfn.h>
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

static int AMD64_init_tss(void)
{
    struct hart_local* hart = this_hart();
    struct amd64_tss* tss;
    size_t i;

    tss = kmalloc(sizeof(struct amd64_tss));
    if(!tss)
        return ENOMEM;

    memset(tss, 0, sizeof(*tss));

    tss->iomap_base = (uint16_t)sizeof(*tss);

    for(i = 0; i < AMD64_IST_COUNT; ++i) {
        if(hart->amd64.ist_stacks[i])
            tss->ist[i] = (uint64_t)hart->amd64.ist_stacks[i];
    }

    hart->amd64.tss = tss;

    amd64_gdt_install_tss(hart->amd64.gdt, tss, sizeof(*tss));

    asm volatile("ltr %0" ::"r"((uint16_t)gdt_selector(GDT_TSS, 0, 0)));

    kprintf(KP_INFORM, "tss: hart=%u base=%p", hart->index, (const void*)tss);

    return 0;
}

DEFINE_INITFN_MP(AMD64_init_tss, INITFN_STAGE_MP_ARCH, INITFN_ORDER_MIDDLE);
