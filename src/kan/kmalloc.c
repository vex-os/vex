/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, Kirill GPRB */
#include <kan/boot.h>
#include <kan/debug.h>
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

/* Zero-bsize-terminated */
static slab_t *slab_list = NULL;

static slab_t *find_slab(size_t n)
{
    size_t i;
    for(i = 0; slab_list[i].bsize; i++) {
        if(slab_list[i].bsize < n)
            continue;
        return &slab_list[i];
    }

    return NULL;
}

static bool expand_slab(slab_t *restrict slab)
{
    size_t i;
    size_t offset;
    size_t max, gap;
    uintptr_t head_ptr = pmalloc(1);

    panic_if(!boot_hhdm.response, "kmalloc: no boot_hhdm");

    if(head_ptr) {
        slab->head = (void **)(head_ptr + boot_hhdm.response->offset);

        offset = __align_ceil(sizeof(slab_t *), slab->bsize);
        max = ((PAGE_SIZE - offset) / slab->bsize) - 1;
        gap = (slab->bsize / sizeof(void *));

        slab->head[0] = slab;
        slab->head = (void **)((uintptr_t)slab->head + offset);

        for(i = 0; i < max; i++)
            slab->head[i * gap] = &slab->head[(i + 1) * gap];
        slab->head[max * gap] = NULL;

        return true;
    }

    return false;
}

static void init_slab(slab_t *restrict slab, size_t bsize)
{
    slab->bsize = bsize;
    slab->head = NULL;
    panic_if(!expand_slab(slab), "kmalloc: not enough memory");
}

void *kmalloc(size_t n)
{
    void *ptr;
    slab_t *slab = find_slab(n);

    if(slab) {
        if(!slab->head && !expand_slab(slab))
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
    size_t len = strlen(s) + 1;
    char *new_str = kmalloc(len);
    if(new_str)
        return strlcpy(new_str, s, len);
    return new_str;
}
EXPORT_SYMBOL(kstrdup);

void kfree(void *restrict ptr)
{
    slab_t *slab;
    void **new_head;
    uintptr_t aligned_ptr = page_align_address((uintptr_t)ptr);

    if(aligned_ptr != (uintptr_t)ptr) {
        slab = ((slab_t **)aligned_ptr)[0];
        new_head = ptr;
        new_head[0] = slab->head;
        slab->head = new_head;
        return;
    }
}
EXPORT_SYMBOL(kfree);

#define SLAB_COUNT 10
static int init_kmalloc(void)
{
    /* Allocate space for SLAB_COUNT slabs plus a terminator */
    uintptr_t list_ptr = pmalloc(get_page_count((SLAB_COUNT + 1) * sizeof(slab_t)));

    panic_if(!boot_hhdm.response, "kmalloc: no boot_hhdm");
    panic_if(!list_ptr, "kmalloc: not enough memory");

    slab_list = (slab_t *)(list_ptr + boot_hhdm.response->offset);

    init_slab(&slab_list[0], 8);
    init_slab(&slab_list[1], 16);
    init_slab(&slab_list[2], 24);
    init_slab(&slab_list[3], 32);
    init_slab(&slab_list[4], 48);
    init_slab(&slab_list[5], 64);
    init_slab(&slab_list[6], 128);
    init_slab(&slab_list[7], 256);
    init_slab(&slab_list[8], 512);
    init_slab(&slab_list[9], 1024);

    return 0;
}
initcall_tier_0(kmalloc, init_kmalloc);
initcall_depend(kmalloc, pmem);
