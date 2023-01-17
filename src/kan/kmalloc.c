/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/bootinfo.h>
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
    size_t blocksize;
    void **head;
} slab_t;

static slab_t *slabs = NULL;

static slab_t *find_matching_slab(size_t size)
{
    size_t i;
    for(i = 0; slabs[i].blocksize; i++) {
        if(slabs[i].blocksize < size)
            continue;
        return &slabs[i];
    }

    return NULL;
}

static bool expand_slab(slab_t *restrict slab)
{
    size_t i;
    size_t offset;
    size_t numblocks;
    size_t blockgap;

    /* Paranoically ensure bsize is valid */
    kassert(slab->blocksize / sizeof(void *) >= 1);
    kassert(slab->blocksize % sizeof(void *) == 0);

    slab->head = pmalloc_hhdm(1);

    if(slab->head) {
        /* Ensure the header takes a whole number of blocks */
        offset = __align_ceil(sizeof(slab_t *), slab->blocksize);

        slab->head[0] = slab;
        slab->head = (void **)((uintptr_t)slab->head + offset);

        numblocks = (PAGE_SIZE - offset) / slab->blocksize;
        blockgap = slab->blocksize / sizeof(void *);

        for(i = 1; i < numblocks; i++)
            slab->head[(i - 1) * blockgap] = &slab->head[i * blockgap];
        slab->head[(numblocks - 1) * blockgap] = NULL;

        return true;
    }

    /* Out of memory. Too Bad! */
    return false;
}

static void init_slab(slab_t *restrict slab, size_t blocksize)
{
    slab->blocksize = blocksize;
    slab->head = NULL;
    panic_if(!expand_slab(slab), "kmalloc: not enough memory to initialize");
}

void *kmalloc(size_t size)
{
    slab_t *slab = find_matching_slab(size);
    void *headptr;
    size_t psize;

    if(slab) {
        if(!slab->head && !expand_slab(slab))
            return NULL;
        headptr = slab->head;
        slab->head = slab->head[0];
        return headptr;
    }
    else {
        psize = get_page_count(size);
        headptr = pmalloc_hhdm(psize + 1);
        if(headptr) {
            ((size_t *)headptr)[0] = psize;
            return (void *)((uintptr_t)headptr + PAGE_SIZE);
        }
    }

    /* Out of memory. Too Bad! */
    return NULL;
}
EXPORT_SYMBOL(kmalloc);

void *kcalloc(size_t count, size_t size)
{
    void *ptr = kmalloc(count * size);
    if(ptr)
        return memset(ptr, 0, count * size);
    return NULL;
}
EXPORT_SYMBOL(kcalloc);

void *krealloc(void *restrict ptr, size_t size)
{
    void *nptr = kmalloc(size);
    if(nptr) {
        if(ptr) {
            memcpy(nptr, ptr, size);
            kfree(ptr);
        }

        return nptr;
    }

    return NULL;
}
EXPORT_SYMBOL(krealloc);

char *kstrdup(const char *restrict str)
{
    size_t size = strlen(str) + 1;
    char *new_str = kmalloc(size);
    if(new_str)
        return kstrncpy(new_str, str, size);
    return NULL;
}
EXPORT_SYMBOL(kstrdup);

void kfree(void *restrict ptr)
{
    slab_t *slab;
    void **headptr;
    void *aptr = (void *)page_align_ptr(ptr);
    size_t psize;

    if(ptr) {
        if(aptr != ptr) {
            slab = ((slab_t **)aptr)[0];
            headptr = ptr;
            headptr[0] = slab->head;
            slab->head = headptr;
        }
        else {
            aptr = (void *)((uintptr_t)aptr - PAGE_SIZE);
            psize = ((size_t *)aptr)[0];
            pmfree_hhdm(aptr, psize + 1);
        }
    }
}
EXPORT_SYMBOL(kfree);

static int init_kmalloc(void)
{
    size_t numslabs = 8;

    slabs = pmalloc_hhdm(get_page_count(sizeof(slab_t) * (numslabs + 1)));
    panic_if(!slabs, "kmalloc: not enough memory to initialize");

    slabs[numslabs].blocksize = 0;
    slabs[numslabs].head = NULL;

    init_slab(&slabs[0], PAGE_SIZE / 512);
    init_slab(&slabs[1], PAGE_SIZE / 256);
    init_slab(&slabs[2], PAGE_SIZE / 128);
    init_slab(&slabs[3], PAGE_SIZE / 64);
    init_slab(&slabs[4], PAGE_SIZE / 32);
    init_slab(&slabs[5], PAGE_SIZE / 16);
    init_slab(&slabs[6], PAGE_SIZE / 8);
    init_slab(&slabs[7], PAGE_SIZE / 4);

    return 0;
}
initcall_tier_0(kmalloc, init_kmalloc);
initcall_depend(kmalloc, bootinfo);
initcall_depend(kmalloc, pmem);
