#include <vex/assert.h>
#include <vex/kprintf.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pmm.h>
#include <vex/slab.h>
#include <vex/string.h>

#include <stdbool.h>

#define SLAB_BLOCKSIZE(index) ((size_t)((1U << (index)) * sizeof(void*)))

struct slab {
    size_t blocksize;
    void** head;
};

static size_t slabs_count = 0U;
static struct slab* slabs = NULL;

static struct slab* find_slab(size_t sz)
{
    size_t i;

    // Try to find the exact size match
    for(i = 0U; i < slabs_count; ++i) {
        if(slabs[i].blocksize == sz)
            return &slabs[i];
        continue;
    }

    // Give up and find anything that fits
    for(i = 0U; i < slabs_count; ++i) {
        if(slabs[i].blocksize >= sz)
            return &slabs[i];
        continue;
    }

    return NULL;
}

static bool expand_slab(struct slab* restrict sl)
{
    size_t i;
    size_t header;
    size_t count, gap;

    kassert((sl->blocksize / sizeof(void*)) >= 1U);
    kassert((sl->blocksize % sizeof(void*)) == 0U);

    sl->head = pmm_alloc_hhdm();

    if(sl->head) {
        header = align_ceil(sizeof(struct slab*), sl->blocksize);

        sl->head[0] = sl;
        sl->head = (void**)((uintptr_t)(sl->head) + header);

        // Each page contains a chunk of the linked list
        // prepended with a header that takes at least a single
        // block of sl->blocksize bytes off the available space
        count = align_floor((PAGE_SIZE - header), sl->blocksize) / sl->blocksize;
        gap = sl->blocksize / sizeof(void*);

        for(i = 1U; i < count; ++i)
            sl->head[gap * (i - 1)] = &sl->head[gap * i];
        sl->head[gap * (count - 1)] = NULL;

        return true;
    }

    return false;
}

void* __nodiscard slab_alloc(size_t sz)
{
    void** head_ptr;
    struct slab* sl = find_slab(sz);

    if(sl) {
        if(!sl->head && !expand_slab(sl)) {
            return NULL;
        }

        head_ptr = sl->head;
        sl->head = head_ptr[0];
        return head_ptr;
    }

    return NULL;
}

void* __nodiscard slab_calloc(size_t count, size_t sz)
{
    void* ptr = slab_alloc(count * sz);
    if(ptr == NULL)
        return NULL;
    return memset(ptr, 0, count * sz);
}

void* __nodiscard slab_realloc(void* restrict ptr, size_t sz)
{
    struct slab* sl;
    void** head_ptr;
    void* new_ptr = slab_alloc(sz);

    if(ptr == NULL) {
        // Just return the new thing
        // without any memory copying
        return new_ptr;
    }

    sl = ((struct slab**)(page_align_ptr(ptr)))[0];

    memcpy(new_ptr, ptr, sl->blocksize);

    head_ptr = ptr;
    head_ptr[0] = sl->head;
    sl->head = new_ptr;

    return new_ptr;
}

void slab_free(void* restrict ptr)
{
    struct slab* sl;
    void** head_ptr;

    if(ptr) {
        sl = ((struct slab**)(page_align_ptr(ptr)))[0];

        head_ptr = ptr;
        head_ptr[0] = sl->head;
        sl->head = head_ptr;
    }
}

void init_slab(void)
{
    size_t i;
    size_t npages;

    slabs_count = 0U;

    while(SLAB_BLOCKSIZE(slabs_count) <= (PAGE_SIZE / 4)) {
        slabs_count += 1U;
    }

    npages = page_count(slabs_count * sizeof(struct slab));
    slabs = pmm_alloc_dma_hhdm(npages);

    if(slabs == NULL) {
        panic("slab: out of memory [pmm_alloc_dma %zu pages]", npages);
        unreachable();
    }

    for(i = 0U; i < slabs_count; ++i) {
        slabs[i].blocksize = SLAB_BLOCKSIZE(i);
        slabs[i].head = NULL;

        if(!expand_slab(&slabs[i])) {
            panic("slab: out of memory [slab %zu:%zu]", i, slabs[i].blocksize);
            unreachable();
        }

        kprintf(KP_NOTICE, "slab: slab[%zu] head=%p blocksize=[%zu]", i, slabs[i].head, slabs[i].blocksize);
    }
}
