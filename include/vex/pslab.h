#ifndef B7D58D3C_78A5_477E_98DE_2F118A0C25D3
#define B7D58D3C_78A5_477E_98DE_2F118A0C25D3
#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

uintptr_t pslab_alloc(void) __nodiscard;
void* pslab_alloc_virt(void) __nodiscard;
void pslab_free(uintptr_t physaddr);
void pslab_free_virt(void* restrict virtaddr);

#endif /* B7D58D3C_78A5_477E_98DE_2F118A0C25D3 */
