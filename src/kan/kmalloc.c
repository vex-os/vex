/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/boot.h>
#include <kan/debug.h>
#include <kan/errno.h>
#include <kan/initcall.h>
#include <kan/kmalloc.h>
#include <kan/pmem.h>
#include <kan/symbol.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

typedef struct slab_s {
    size_t bsize;
    void **head;
} slab_t;

static size_t num_slabs = 0;
static slab_t *slabs = NULL;

static slab_t *find_slab(size_t n)
{
    size_t i;

    for(i = 0; i < num_slabs; i++) {
        if(slabs[i].bsize < n)
            continue;
        return &slabs[i];
    }

    return NULL;
}

static bool extend_slab(slab_t *restrict slab)
{
    size_t i;
    size_t hsize;
    size_t last_block, gap;
    uintptr_t head_addr = pmalloc(1);

    kassert(slab->bsize >= sizeof(void *));
    kassert(slab->bsize % sizeof(void *) == 0);
    kassert(boot_hhdm.response != NULL);

    if(head_addr) {
        slab->head = (void **)(head_addr + boot_hhdm.response->offset);

        /* The page contains a header with the size
         * ceil-aligned to slab->bsize that has a pointer
         * to the slab structure that owns that page. */
        hsize = __align_ceil(sizeof(slab_t *), slab->bsize);

        slab->head[0] = slab;
        slab->head = (void **)((uintptr_t)slab->head + hsize);

        /* Last free block's <next> shall always be NULL. */
        last_block = (PAGE_SIZE - hsize) / slab->bsize - 1;

        /* Because we maintain free blocks in a linked list,
         * bsize is a multiple of a pointer size, meaning
         * there there is a gap of (N >= 1) pointer-sized objects. */
        gap = slab->bsize / sizeof(void *);

        for(i = 0; i < last_block; i++)
            slab->head[i * gap] = &slab->head[(i + 1) * gap];
        slab->head[last_block * gap] = NULL;

        return true;
    }

    return false;
}

static void init_slab(slab_t *restrict slab, size_t bsize)
{
    slab->bsize = bsize;
    slab->head = NULL;
    panic_if(!extend_slab(slab), "kmalloc: not enough memory to initialize");
}

void *kmalloc(size_t n)
{
    void *ptr;
    slab_t *slab = find_slab(n);

    if(slab) {
        if(!slab->head && !extend_slab(slab))
            return NULL;

        ptr = slab->head;
        slab->head = slab->head[0];

        return ptr;
    }

    return NULL;
}
EXPORT_SYMBOL(kmalloc);

void *kcalloc(size_t count, size_t n)
{
    void *ptr = kmalloc(count * n);
    if(ptr)
        return memset(ptr, 0, count * n);
    return ptr;
}
EXPORT_SYMBOL(kcalloc);

void *krealloc(void *restrict ptr, size_t n)
{
    void *new_ptr = kmalloc(n);
    if(new_ptr) {
        if(ptr) {
            memcpy(new_ptr, ptr, n);
            kfree(ptr);
        }

        return new_ptr;
    }

    return NULL;
}
EXPORT_SYMBOL(krealloc);

char *kstrdup(const char *restrict s)
{
    size_t n = strlen(s) + 1;
    char *new_str = kmalloc(n);
    if(new_str)
        return kstrncpy(new_str, s, n);
    return NULL;
}
EXPORT_SYMBOL(kstrdup);

void kfree(void *restrict ptr)
{
    slab_t *slab;
    void **new_head;
    uintptr_t aptr = page_align_address((uintptr_t)ptr);

    /* Slab allocated blocks are never page-aligned */
    if(aptr != (uintptr_t)ptr) {
        slab = ((slab_t **)aptr)[0];

        new_head = ptr;
        new_head[0] = slab->head;
        slab->head = new_head;

        return;
    }
}
EXPORT_SYMBOL(kfree);

static int init_kmalloc(void)
{
    uintptr_t slabs_ptr;

    kassert(boot_hhdm.response != NULL);

    num_slabs = 10;
    slabs_ptr = pmalloc(get_page_count(num_slabs * sizeof(slab_t)));
    panic_if(!slabs_ptr, "kmalloc: not enough memory to initialize");

    slabs = (slab_t *)(slabs_ptr + boot_hhdm.response->offset);
    init_slab(&slabs[0], 8);
    init_slab(&slabs[1], 16);
    init_slab(&slabs[2], 24);
    init_slab(&slabs[3], 32);
    init_slab(&slabs[4], 48);
    init_slab(&slabs[5], 64);
    init_slab(&slabs[6], 128);
    init_slab(&slabs[7], 256);
    init_slab(&slabs[8], 512);
    init_slab(&slabs[9], 1024);

    return EOK;
}
initcall_tier_0(kmalloc, init_kmalloc);
initcall_depend(kmalloc, pmem);
