#pragma once

#include <vex/compiler.h>
#include <vex/types.h>
#include <vex/vprot.h>

void* mmio_map(uintptr_t phys, size_t size, unsigned vprot) __nodiscard;
void mmio_unmap(volatile void* virt);
bool mmio_owns(const volatile void* virt) __nodiscard;
