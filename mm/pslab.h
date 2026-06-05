#ifndef B7D58D3C_78A5_477E_98DE_2F118A0C25D3
#define B7D58D3C_78A5_477E_98DE_2F118A0C25D3
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

struct pslab_stats {
    size_t total_bytes;
    size_t total_pages;
    size_t used_bytes;
    size_t used_pages;
};

uintptr_t pslab_alloc(void) __nodiscard;
void* pslab_alloc_virt(void) __nodiscard;
void pslab_free(uintptr_t physaddr);
void pslab_free_virt(void* restrict virtaddr);
void pslab_stats(struct pslab_stats* stats);

#endif /* B7D58D3C_78A5_477E_98DE_2F118A0C25D3 */
