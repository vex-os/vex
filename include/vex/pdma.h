#ifndef B2E72E76_5D2F_43BF_A8B3_80677A7ECE4C
#define B2E72E76_5D2F_43BF_A8B3_80677A7ECE4C
#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#ifdef TRACE_PDMA
#include <vex/kprintf.h>
#endif

#ifndef PDMA_APPROX_END
#define PDMA_APPROX_END 0xFFFFFF
#endif

uintptr_t pdma_alloc_phys_notrace(size_t numpages) __nodiscard;
void* pdma_alloc_virt_notrace(size_t numpages) __nodiscard;
void pdma_free_phys_notrace(uintptr_t physaddr, size_t numpages);
void pdma_free_virt_notrace(void* restrict virtaddr, size_t numpages);

#ifdef TRACE_PDMA
#define pdma_alloc_phys(npages) \
    (pr_verbose("%s:%lu: pdma_alloc_phys %zu", __FILE__, (unsigned long)(__LINE__), (size_t)(npages)), pdma_alloc_phys_notrace(npages))
#define pdma_alloc_virt(npages) \
    (pr_verbose("%s:%lu: pdma_alloc_virt %zu", __FILE__, (unsigned long)(__LINE__), (size_t)(npages)), pdma_alloc_virt_notrace(npages))
#define pdma_free_phys(addr, npages)                                                                                                \
    (pr_verbose("%s:%lu: pdma_free_phys %p, %zu", __FILE__, (unsigned long)(__LINE__), (void*)(uintptr_t)(addr), (size_t)(npages)), \
        pdma_free_phys_notrace(addr, (size_t)(npages)))
#define pdma_free_virt(addr, npages)                                                                            \
    (pr_verbose("%s:%lu: pdma_free_virt %p, %zu", __FILE__, (unsigned long)(__LINE__), addr, (size_t)(npages)), \
        pdma_free_virt_notrace(addr, (size_t)(npages)))
#else
#define pdma_alloc_phys(npages)      pdma_alloc_phys_notrace(npages)
#define pdma_alloc_virt(npages)      pdma_alloc_virt_notrace(npages)
#define pdma_free_phys(addr, npages) pdma_free_phys_notrace(addr, npages)
#define pdma_free_virt(addr, npages) pdma_free_virt_notrace(addr, npages)
#endif

#endif /* B2E72E76_5D2F_43BF_A8B3_80677A7ECE4C */
