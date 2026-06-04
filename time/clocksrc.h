#ifndef B793DFAA_5257_48C9_A9BD_66862A0F1555
#define B793DFAA_5257_48C9_A9BD_66862A0F1555
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

struct clocksrc;
struct clocksrc_ops {
    uint64_t (*read)(struct clocksrc* restrict src);
};

struct clocksrc_private {
    struct clocksrc* next;
    uint64_t last;
    uint64_t base;
    uint64_t mult;
    uint64_t shift;
};

struct clocksrc {
    struct clocksrc_private priv;
    const struct clocksrc_ops* ops;
    char identifier[64];
    unsigned rating;
    uint64_t mask;
    void* ctxdata;
};

#define CLOCKSRC_MASK_1  UINT64_C(0x0000000000000001)
#define CLOCKSRC_MASK_2  UINT64_C(0x0000000000000003)
#define CLOCKSRC_MASK_4  UINT64_C(0x000000000000000F)
#define CLOCKSRC_MASK_8  UINT64_C(0x00000000000000FF)
#define CLOCKSRC_MASK_16 UINT64_C(0x000000000000FFFF)
#define CLOCKSRC_MASK_24 UINT64_C(0x0000000000FFFFFF)
#define CLOCKSRC_MASK_32 UINT64_C(0x00000000FFFFFFFF)
#define CLOCKSRC_MASK_48 UINT64_C(0x0000FFFFFFFFFFFF)
#define CLOCKSRC_MASK_64 UINT64_C(0xFFFFFFFFFFFFFFFF)

int clocksrc_register_internal(struct clocksrc* restrict src, uint64_t scale, uint64_t frequency);
int clocksrc_calibrate_internal(struct clocksrc* restrict src, uint64_t scale, uint64_t frequency);
int clocksrc_unregister(struct clocksrc* restrict src);
struct clocksrc* active_clocksrc(void) __nodiscard;
uint64_t clocksrc_read_nsec(void) __nodiscard;

static __always_inline inline int clocksrc_register_hz(struct clocksrc* restrict src, uint64_t hz)
{
    return clocksrc_register_internal(src, UINT64_C(1), hz);
}

static __always_inline inline int clocksrc_register_khz(struct clocksrc* restrict src, uint64_t khz)
{
    return clocksrc_register_internal(src, UINT64_C(1000), khz);
}

static __always_inline inline int clocksrc_register_mhz(struct clocksrc* restrict src, uint64_t mhz)
{
    return clocksrc_register_internal(src, UINT64_C(1000000), mhz);
}

static __always_inline inline int clocksrc_calibrate_hz(struct clocksrc* restrict src, uint64_t hz)
{
    return clocksrc_calibrate_internal(src, UINT64_C(1), hz);
}

static __always_inline inline int clocksrc_calibrate_khz(struct clocksrc* restrict src, uint64_t khz)
{
    return clocksrc_calibrate_internal(src, UINT64_C(1000), khz);
}

static __always_inline inline int clocksrc_calibrate_mhz(struct clocksrc* restrict src, uint64_t mhz)
{
    return clocksrc_calibrate_internal(src, UINT64_C(1000000), mhz);
}

#endif /* B793DFAA_5257_48C9_A9BD_66862A0F1555 */
