#pragma once

#include <vex/types.h>

struct mmio_region {
    volatile uint8_t* virt8;
    volatile uint16_t* virt16;
    volatile uint32_t* virt32;
    volatile uint64_t* virt64;
    size_t length;
};
