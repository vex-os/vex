// SPDX-License-Identifier: BSD-3-Clause

#include <amd64/msr.h>
#include <arch/hart.h>
#include <core/assert.h>
#include <core/atomic.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <init/initcall.h>
#include <lib/string.h>
#include <mm/malloc.h>

#define MSR_GS_BASE 0xC0000101

static unsigned int hart_count = 0;

define_initcall(amd64_hart, INITCALL_STAGE_SMP_HART, INITCALL_ORDER_FIRST)
{
    struct hart_local* hart;

    hart = kmalloc(sizeof(struct hart_local));
    strict_kassert_text(hart, "out of memory");

    memset(hart, 0, sizeof(struct hart_local));

    hart->self = hart;
    hart->index = atomic_fetch_add(&hart_count, 1, ATOMIC_RELAXED);

    msr_write(MSR_GS_BASE, (uint64_t)(hart));
}
