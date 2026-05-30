#ifndef B793DFAA_5257_48C9_A9BD_66862A0F1555
#define B793DFAA_5257_48C9_A9BD_66862A0F1555
#pragma once

#include <core/cdefs.h>
#include <stdint.h>

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
    void* privdata;
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

extern int register_clocksrc_internal(struct clocksrc* restrict src, uint64_t scale, uint64_t frequency);
extern int calibrate_clocksrc_internal(struct clocksrc* restrict src, uint64_t scale, uint64_t frequency);
extern int unregister_clocksrc(struct clocksrc* restrict src);
extern struct clocksrc* active_clocksrc(void) __nodiscard;
extern uint64_t clocksrc_read_nsec(void) __nodiscard;

static __always_inline inline int register_clocksrc_hz(struct clocksrc* restrict src, uint64_t hz)
{
    return register_clocksrc_internal(src, UINT64_C(1), hz);
}

static __always_inline inline int register_clocksrc_khz(struct clocksrc* restrict src, uint64_t khz)
{
    return register_clocksrc_internal(src, UINT64_C(1000), khz);
}

static __always_inline inline int register_clocksrc_mhz(struct clocksrc* restrict src, uint64_t mhz)
{
    return register_clocksrc_internal(src, UINT64_C(1000000), mhz);
}

static __always_inline inline int calibrate_clocksrc_hz(struct clocksrc* restrict src, uint64_t hz)
{
    return calibrate_clocksrc_internal(src, UINT64_C(1), hz);
}

static __always_inline inline int calibrate_clocksrc_khz(struct clocksrc* restrict src, uint64_t khz)
{
    return calibrate_clocksrc_internal(src, UINT64_C(1000), khz);
}

static __always_inline inline int calibrate_clocksrc_mhz(struct clocksrc* restrict src, uint64_t mhz)
{
    return calibrate_clocksrc_internal(src, UINT64_C(1000000), mhz);
}

#endif /* B793DFAA_5257_48C9_A9BD_66862A0F1555 */
