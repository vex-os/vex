#ifndef A3F1C2D4_8B7E_4F9A_B3C1_5D2E6F8A9B0C
#define A3F1C2D4_8B7E_4F9A_B3C1_5D2E6F8A9B0C
#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

struct amd64_tss {
    uint32_t reserved_0;
    uint64_t rsp[3];
    uint64_t reserved_1;
    uint64_t ist[7];
    uint64_t reserved_2;
    uint16_t reserved_3;
    uint16_t iomap_base;
} __packed;

#endif /* A3F1C2D4_8B7E_4F9A_B3C1_5D2E6F8A9B0C */
