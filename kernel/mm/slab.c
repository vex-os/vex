#include <vex/assert.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/pdma.h>
#include <vex/pslab.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>

#define SLAB_BLOCKSIZE(index) ((size_t)((1U << (index)) * sizeof(void*)))

typedef struct slab_s slab_t;

struct slab_s {
    size_t blocksize;
    void** head;
};

static size_t slabs_count = 0U;
static slab_t* slabs = nullptr;
static spinlock_t slab_lock = SPINLOCK_INIT;

static slab_t* find_slab(size_t size)
{
    size_t i;

    for(i = 0U; i < slabs_count; ++i) {
        if(slabs[i].blocksize == size)
            return &slabs[i];
        continue;
    }

    for(i = 0U; i < slabs_count; ++i) {
        if(slabs[i].blocksize >= size)
            return &slabs[i];
        continue;
    }

    return nullptr;
}

static bool expand_slab(slab_t* restrict sl)
{
    size_t i;
    size_t header;
    size_t count, gap;

    kassert((sl->blocksize / sizeof(void*)) >= 1U);
    kassert((sl->blocksize % sizeof(void*)) == 0U);

    sl->head = pslab_alloc_virt();

    if(sl->head) {
        header = align_ceil(sizeof(slab_t*), sl->blocksize);

        sl->head[0] = sl;
        sl->head = (void**)((uintptr_t)sl->head + header);

        // Each page contains a chunk of the linked list
        // prepended with a header that takes at least a single
        // block of sl->blocksize bytes off the available space
        count = align_floor((PAGE_SIZE - header), sl->blocksize) / sl->blocksize;
        gap = sl->blocksize / sizeof(void*);

        for(i = 1U; i < count; ++i)
            sl->head[gap * (i - 1)] = &sl->head[gap * i];
        sl->head[gap * (count - 1)] = nullptr;

        return true;
    }

    return false;
}

void* slab_alloc(size_t size)
{
    void** head_ptr;
    slab_t* sl;

    bool intr_state = spinlock_acquire_intr(&slab_lock);

    sl = find_slab(size);

    if(sl) {
        if(!sl->head && !expand_slab(sl)) {
            spinlock_release_intr(&slab_lock, intr_state);
            return nullptr;
        }

        head_ptr = sl->head;
        sl->head = head_ptr[0];

        spinlock_release_intr(&slab_lock, intr_state);

        return memset(head_ptr, 0, size);
    }

    spinlock_release_intr(&slab_lock, intr_state);

    return nullptr;
}

void* slab_realloc(void* restrict ptr, size_t size)
{
    if(ptr == nullptr) {
        return slab_alloc(size);
    }

    void* new_ptr = slab_alloc(size);

    if(new_ptr == nullptr) {
        return nullptr;
    }

    const slab_t* sl = ((slab_t**)(page_align_ptr(ptr)))[0];

    memcpy(new_ptr, ptr, sl->blocksize < size ? sl->blocksize : size);

    slab_free(ptr);

    return new_ptr;
}

void slab_free(void* restrict ptr)
{
    slab_t* sl;
    void** head_ptr;

    bool intr_state = spinlock_acquire_intr(&slab_lock);

    if(ptr) {
        sl = ((slab_t**)(page_align_ptr(ptr)))[0];

        head_ptr = ptr;
        head_ptr[0] = sl->head;
        sl->head = head_ptr;
    }

    spinlock_release_intr(&slab_lock, intr_state);
}

static void init_slab(void)
{
    slabs_count = 0U;

    while(SLAB_BLOCKSIZE(slabs_count) <= SLAB_MAX_SIZE) {
        slabs_count += 1U;
    }

    size_t npages = page_count(slabs_count * sizeof(slab_t));

    slabs = pdma_alloc_virt(npages);
    if(slabs == nullptr) {
        panic("slab: out of memory");
    }

    for(size_t i = 0U; i < slabs_count; ++i) {
        slabs[i].blocksize = SLAB_BLOCKSIZE(i);
        slabs[i].head = nullptr;

        bool result = expand_slab(&slabs[i]);
        if(!result) {
            panic("slab: out of memory");
        }

        pr_verbose("slab: slab[%zu]: head=%p, blocksize=%zu", i, slabs[i].head, slabs[i].blocksize);
    }
}

initcall_bsp_slab(init_slab, INIT_MIDDLE);
