// SPDX-License-Identifier: BSD-3-Clause

#include <core/assert.h>
#include <core/errno.h>
#include <core/linker.h>
#include <core/panic.h>
#include <init/initfn.h>
#include <mm/malloc.h>
#include <mm/pslab.h>

void* kmalloc(size_t sz)
{
    panic("kmalloc is not implemented");
    unreachable();
}

void* kcalloc(size_t count, size_t sz)
{
    panic("kcalloc is not implemented");
    unreachable();
}

void* krealloc(void* restrict ptr, size_t sz)
{
    panic("krealloc is not implemented");
    unreachable();
}

void kfree(void* restrict ptr)
{
    panic("kfree is not implemented");
    unreachable();
}

static int init_malloc(void)
{
    return ENODEV;
}

DEFINE_INITFN(init_malloc, INITFN_CAT_HEAP, INITFN_ORDER_FIRST);
