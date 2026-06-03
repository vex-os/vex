#ifndef FBA4EE33_16D0_491E_B145_335A2BCB4620
#define FBA4EE33_16D0_491E_B145_335A2BCB4620
#pragma once

#include <core/types.h>

struct mmio_region {
    volatile uint8_t* virt8;
    volatile uint16_t* virt16;
    volatile uint32_t* virt32;
    volatile uint64_t* virt64;
    size_t length;
};

#endif /* FBA4EE33_16D0_491E_B145_335A2BCB4620 */
