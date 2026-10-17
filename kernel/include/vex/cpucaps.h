#pragma once

#include <arch/cpucaps.h>
#include <vex/bitmap.h>

enum cpu_feature_scope {
    CPU_SCOPE_SYSTEM,   // CPU feature is usable only if all harts have it
    CPU_SCOPE_REQUIRED, // CPU feature is usable all the time, otherwise the hart is parked
    CPU_SCOPE_LOCAL,    // CPU feature is usable on that specific hart
};

struct cpu_feature_desc {
    const char* name;
    enum cpu_feature_scope scope;
};

#define CPUCAPS_CHUNKS ((NUM_CPU_FEATURES + BITMAP_CHUNK_BITS - 1) / BITMAP_CHUNK_BITS)

extern const struct cpu_feature_desc arch_cpu_features[];

struct cpucaps {
    bitmap_type features[CPUCAPS_CHUNKS];
};

static __always_inline void cpucaps_set(struct cpucaps* restrict caps, enum cpu_feature feature)
{
    bitmap_set(caps->features, feature);
}

static __always_inline __nodiscard bool cpucaps_has(const struct cpucaps* restrict caps, enum cpu_feature feature)
{
    return bitmap_isset(caps->features, feature);
}

bool cpu_has(enum cpu_feature feature) __nodiscard;

void cpucaps_init(const struct cpucaps* restrict boot_caps);
bool cpucaps_merge(const struct cpucaps* restrict caps, unsigned short index) __nodiscard;
void cpucaps_commit(void);
