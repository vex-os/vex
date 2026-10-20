#pragma once

#include <vex/compiler.h>
#include <vex/types.h>
#include <vex/vprot.h>

typedef struct mmio_region_s mmio_region_t;

struct mmio_region_s {
    volatile uint8_t* virt8;
    volatile uint16_t* virt16;
    volatile uint32_t* virt32;
    volatile uint64_t* virt64;
    size_t length;
};

void* mmio_map(uintptr_t phys, size_t size, unsigned vprot) __nodiscard;
void mmio_unmap(volatile void* virt);
bool mmio_owns(const volatile void* virt) __nodiscard;
