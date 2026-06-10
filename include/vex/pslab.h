#ifndef B7D58D3C_78A5_477E_98DE_2F118A0C25D3
#define B7D58D3C_78A5_477E_98DE_2F118A0C25D3
#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#ifdef TRACE_PSLAB
#include <vex/kprintf.h>
#endif

uintptr_t pslab_alloc_phys_notrace(void) __nodiscard;
void* pslab_alloc_virt_notrace(void) __nodiscard;
void pslab_free_phys_notrace(uintptr_t physaddr);
void pslab_free_virt_notrace(void* restrict virtaddr);

#ifdef TRACE_PSLAB
#define pslab_alloc_phys() (pr_verbose("%s:%lu: pslab_alloc_phys", __FILE__, (unsigned long)(__LINE__)), pslab_alloc_phys_notrace())
#define pslab_alloc_virt() (pr_verbose("%s:%lu: pslab_alloc_virt", __FILE__, (unsigned long)(__LINE__)), pslab_alloc_virt_notrace())
#define pslab_free_phys(addr)                                                                                   \
    (pr_verbose("%s:%lu: pslab_free_phys %p", __FILE__, (unsigned long)(__LINE__), (void*)((uintptr_t)(addr))), \
        pslab_free_phys_notrace(addr))
#define pslab_free_virt(addr) \
    (pr_verbose("%s:%lu: pslab_free_virt %p", __FILE__, (unsigned long)(__LINE__), addr), pslab_free_virt_notrace(addr))
#else
#define pslab_alloc_phys()    pslab_alloc_phys_notrace()
#define pslab_alloc_virt()    pslab_alloc_virt_notrace()
#define pslab_free_phys(addr) pslab_free_phys_notrace(addr)
#define pslab_free_virt(addr) pslab_free_virt_notrace(addr)
#endif

#endif /* B7D58D3C_78A5_477E_98DE_2F118A0C25D3 */
