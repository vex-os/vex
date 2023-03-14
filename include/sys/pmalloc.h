#ifndef __INCLUDE_SYS_PMALLOC_H__
#define __INCLUDE_SYS_PMALLOC_H__
#include <machine/page.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>

void add_memory_block(uintptr_t address, size_t length);
size_t get_total_memory(void);
size_t get_used_memory(void);
uintptr_t pmalloc(size_t npages);
void *pmalloc_virt(size_t npages);
void pmfree(uintptr_t address, size_t npages);
void pmfree_virt(void *restrict ptr, size_t npages);

static __always_inline inline uintptr_t page_align_address(uintptr_t address)
{
    return __align_floor(address, PAGE_SIZE);
}

static __always_inline inline void *page_align_pointer(void *restrict s)
{
    return (void *)(__align_floor((uintptr_t)s, PAGE_SIZE));
}

static __always_inline inline const void *page_align_const_pointer(const void *restrict s)
{
    return (const void *)(__align_floor((uintptr_t)s, PAGE_SIZE));
}

static __always_inline inline size_t get_page_count(size_t n)
{
    return __align_ceil(n, PAGE_SIZE) / PAGE_SIZE;
}

#endif /* __INCLUDE_SYS_PMALLOC_H__ */
