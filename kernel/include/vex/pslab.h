#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

uintptr_t pslab_alloc_phys(void) __nodiscard;
void* pslab_alloc_virt(void) __nodiscard;
void pslab_free_phys(uintptr_t physaddr);
void pslab_free_virt(void* restrict virtaddr);
