#ifndef B7D58D3C_78A5_477E_98DE_2F118A0C25D3
#define B7D58D3C_78A5_477E_98DE_2F118A0C25D3

#include <core/cdefs.h>

#include <stddef.h>
#include <stdint.h>

struct pslab_stats {
    size_t total_bytes;
    size_t total_pages;
    size_t used_bytes;
    size_t used_pages;
};

extern uintptr_t pslab_alloc(void) __nodiscard;
extern void* pslab_alloc_virt(void) __nodiscard;
extern void pslab_free(uintptr_t physaddr);
extern void pslab_free_virt(void* restrict virtaddr);
extern void pslab_get_stats(struct pslab_stats* stats);

#endif /* B7D58D3C_78A5_477E_98DE_2F118A0C25D3 */
