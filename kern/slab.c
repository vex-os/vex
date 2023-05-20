/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#include <stdbool.h>
#include <string.h>
#include <sys/slab.h>
#include <sys/panic.h>
#include <sys/pmm.h>

typedef struct slab_s {
    size_t blocksize;
    void **head;
} slab_t;

static size_t numslab = 0;
static slab_t *slabs = NULL;

static slab_t *find_matching_slab(size_t n)
{
    size_t i;

    for(i = 0; i < numslab; ++i) {
        if(slabs[i].blocksize <= n)
            return &slabs[i];
        continue;
    }

    return NULL;
}

static bool try_expand_slab(slab_t *restrict slab)
{
    size_t i;
    size_t hsize;
    size_t obj_count;
    size_t obj_gap;

    kassert(slab->blocksize / sizeof(void *) >= 1);
    kassert(slab->blocksize % sizeof(void *) == 0);

    if((slab->head = pmm_alloc_hhdm(1, PMM_ZONE_NORMAL)) != NULL) {
        hsize = __align_ceil(sizeof(slab_t *), slab->blocksize);

        slab->head[0] = slab;
        slab->head = (void **)((uintptr_t)slab->head + hsize);

        /* Linked list will have obj_count new entries,
         * each of them pointing to the next one obj_gap bytes forward */
        obj_count = __align_floor((PAGE_SIZE - hsize), slab->blocksize) / slab->blocksize;
        obj_gap = slab->blocksize / sizeof(void *);

        for(i = 1; i < obj_count; i++)
            slab->head[obj_gap * (i - 1)] = &slab->head[obj_gap * i];
        slab->head[obj_gap * (obj_count - 1)] = NULL;

        return true;
    }

    return false;
}

static void setup_slab(slab_t *restrict slab, size_t blocksize)
{
    slab->blocksize = __align_ceil(blocksize, sizeof(void *));
    slab->head = NULL;

    if(!try_expand_slab(slab)) {
        panic("slab: insufficient memory");
        UNREACHABLE();
    }
}

void *slab_alloc(size_t n)
{
    slab_t *slab;
    void *head_ptr;

    if((slab = find_matching_slab(n)) != NULL) {
        if(!slab->head && !try_expand_slab(slab))
            return NULL;
        head_ptr = slab->head;
        slab->head = slab->head[0];
        return head_ptr;
    }

    return NULL;
}

void *slab_calloc(size_t count, size_t n)
{
    void *ptr = slab_alloc(count * n);
    if(!ptr)
        return NULL;
    return memset(ptr, 0, count * n);
}

void *slab_realloc(void *restrict ptr, size_t n)
{
    slab_t *slab;
    void **head_ptr;
    void *aligned_ptr;
    void *new_ptr;

    if(ptr) {
        if((new_ptr = slab_alloc(n)) != NULL) {
            aligned_ptr = page_align_pointer(ptr);

            if(ptr != aligned_ptr) {
                slab = ((slab_t **)aligned_ptr)[0];

                memcpy(new_ptr, ptr, slab->blocksize);

                head_ptr = ptr;
                head_ptr[0] = slab->head;
                slab->head = head_ptr;

                return new_ptr;
            }
        }

        slab_free(ptr);

        return NULL;
    }

    return slab_alloc(n);
}

void slab_free(void *restrict ptr)
{
    slab_t *slab;
    void **head_ptr;
    void *aligned_ptr;

    if(ptr) {
        aligned_ptr = page_align_pointer(ptr);

        if(ptr != aligned_ptr) {
            slab = ((slab_t **)aligned_ptr)[0];

            head_ptr = ptr;
            head_ptr[0] = slab->head;
            slab->head = head_ptr;
        }
    }
}

static void init_slab(void)
{
    size_t numpage;

    numslab = 8;
    numpage = get_page_count(sizeof(slab_t) * numslab);

    if((slabs = pmm_alloc_hhdm(numpage, PMM_ZONE_NORMAL)) == NULL) {
        panic("slab: insufficient memory");
        UNREACHABLE();
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
