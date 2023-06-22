/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <kernel/page.h>
#include <kernel/pmm.h>
#include <kernel/slab.h>
#include <kernel/systm.h>
#include <stdbool.h>
#include <string.h>

struct slab {
    size_t sl_blocksize;
    void **sl_head;
};

static size_t numslab = 0;
static struct slab *slabs = NULL;

static struct slab *find_matching_slab(size_t n)
{
    size_t i;

    for(i = 0; i < numslab; ++i) {
        if(slabs[i].sl_blocksize <= n)
            return &slabs[i];
        continue;
    }

    return NULL;
}

static bool try_expand_slab(struct slab *restrict sl)
{
    size_t i;
    size_t hsize;
    size_t obj_count;
    size_t obj_gap;

    kassert(sl->sl_blocksize / sizeof(void *) >= 1);
    kassert(sl->sl_blocksize % sizeof(void *) == 0);

    if((sl->sl_head = pmm_alloc_highmem(1)) != NULL) {
        hsize = __align_ceil(sizeof(struct slab *), sl->sl_blocksize);

        sl->sl_head[0] = sl;
        sl->sl_head = (void **)((uintptr_t)sl->sl_head + hsize);

        /* Linked list will have obj_count new entries,
         * each of them pointing to the next one obj_gap bytes forward */
        obj_count = __align_floor((PAGE_SIZE - hsize), sl->sl_blocksize) / sl->sl_blocksize;
        obj_gap = sl->sl_blocksize / sizeof(void *);

        for(i = 1; i < obj_count; i++)
            sl->sl_head[obj_gap * (i - 1)] = &sl->sl_head[obj_gap * i];
        sl->sl_head[obj_gap * (obj_count - 1)] = NULL;

        return true;
    }

    return false;
}

static void setup_slab(struct slab *restrict sl, size_t blocksize)
{
    sl->sl_blocksize = __align_ceil(blocksize, sizeof(void *));
    sl->sl_head = NULL;

    if(!try_expand_slab(sl)) {
        panic("slab: insufficient memory");
        unreachable();
    }
}

void *kmalloc(size_t n)
{
    struct slab *sl;
    void *head_ptr;

    if((sl = find_matching_slab(n)) != NULL) {
        if(!sl->sl_head && !try_expand_slab(sl))
            return NULL;
        head_ptr = sl->sl_head;
        sl->sl_head = sl->sl_head[0];
        return head_ptr;
    }

    return NULL;
}

void *kcalloc(size_t count, size_t n)
{
    void *ptr = kmalloc(count * n);
    if(!ptr)
        return NULL;
    return memset(ptr, 0, count * n);
}

void *krealloc(void *restrict ptr, size_t n)
{
    struct slab *sl;
    void **head_ptr;
    void *aligned_ptr;
    void *new_ptr;

    if(ptr) {
        if((new_ptr = kmalloc(n)) != NULL) {
            aligned_ptr = page_align_pointer(ptr);

            if(ptr != aligned_ptr) {
                sl = ((struct slab **)aligned_ptr)[0];

                memcpy(new_ptr, ptr, sl->sl_blocksize);

                head_ptr = ptr;
                head_ptr[0] = sl->sl_head;
                sl->sl_head = head_ptr;

                return new_ptr;
            }
        }

        kfree(ptr);

        return NULL;
    }

    return kmalloc(n);
}

void kfree(void *restrict ptr)
{
    struct slab *sl;
    void **head_ptr;
    void *aligned_ptr;

    if(ptr) {
        aligned_ptr = page_align_pointer(ptr);

        if(ptr != aligned_ptr) {
            sl = ((struct slab **)aligned_ptr)[0];

            head_ptr = ptr;
            head_ptr[0] = sl->sl_head;
            sl->sl_head = head_ptr;
        }
    }
}

static void init_slab(void)
{
    size_t numpage;

    numslab = 8;
    numpage = get_page_count(sizeof(struct slab) * numslab);

    if((slabs = pmm_alloc_highmem(numpage)) == NULL) {
        panic("slab: insufficient memory");
        unreachable();
    }

    memset(slabs, 0, numpage * PAGE_SIZE);

    setup_slab(&slabs[0], PAGE_SIZE / 512);
    setup_slab(&slabs[1], PAGE_SIZE / 256);
    setup_slab(&slabs[2], PAGE_SIZE / 128);
    setup_slab(&slabs[3], PAGE_SIZE / 64);
    setup_slab(&slabs[4], PAGE_SIZE / 32);
    setup_slab(&slabs[5], PAGE_SIZE / 16);
    setup_slab(&slabs[6], PAGE_SIZE / 8);
    setup_slab(&slabs[7], PAGE_SIZE / 4);
}
core_initcall(slab, init_slab);
initcall_depend(slab, pmm);