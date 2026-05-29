// SPDX-License-Identifier: BSD-3-Clause

#include <amd64/msr.h>
#include <arch/hart.h>
#include <core/atomic.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <init/initfn.h>
#include <lib/string.h>
#include <mm/malloc.h>

#define MSR_GS_BASE 0xC0000101

static unsigned int hart_count = 0;

struct hart_local* hart_alloc(void)
{
    struct hart_local* hart;

    hart = kmalloc(sizeof(struct hart_local));

    if(hart == nullptr) {
        return nullptr;
    }

    memset(hart, 0, sizeof(*hart));

    hart->self = hart;
    hart->index = atomic_fetch_add(&hart_count, 1, ATOMIC_RELAXED);

    msr_write(MSR_GS_BASE, (uint64_t)(hart));

    return hart;
}

static int AMD64_init_hart(void)
{
    struct hart_local* hart;

    if(msr_read(MSR_GS_BASE) != 0) {
        return 0;
    }

    hart = hart_alloc();

    if(hart == nullptr) {
        return ENOMEM;
    }

    kprintf(KP_INFORM, "hart: index=%u", hart->index);

    return 0;
}

DEFINE_INITFN_MP(AMD64_init_hart, INITFN_STAGE_MP_ARCH, INITFN_ORDER_FIRST);
