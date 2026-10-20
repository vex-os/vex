#pragma once

#include <arch/cpucaps.h>
#include <vex/bitmap.h>

#define CPUCAPS_CHUNKS ((NUM_CPU_FEATURES + BITMAP_CHUNK_BITS - 1) / BITMAP_CHUNK_BITS)

typedef struct cpu_feature_desc_s cpu_feature_desc_t;
typedef struct cpucaps_s cpucaps_t;

enum cpu_feature_scope_e {
    CPU_SCOPE_SYSTEM,   // CPU feature is usable only if all harts have it
    CPU_SCOPE_REQUIRED, // CPU feature is usable all the time, otherwise the hart is parked
    CPU_SCOPE_LOCAL,    // CPU feature is usable on that specific hart
};

typedef enum cpu_feature_scope_e cpu_feature_scope_t;

struct cpu_feature_desc_s {
    const char* name;
    cpu_feature_scope_t scope;
};

struct cpucaps_s {
    bitmap_t features[CPUCAPS_CHUNKS];
};

extern const cpu_feature_desc_t arch_cpu_features[];

bool cpu_has(cpu_feature_t feature) __nodiscard;

void cpucaps_init(const cpucaps_t* restrict boot_caps);
bool cpucaps_merge(const cpucaps_t* restrict caps, unsigned short index) __nodiscard;
void cpucaps_commit(void);

static __always_inline void cpucaps_set(cpucaps_t* restrict caps, cpu_feature_t feature)
{
    bitmap_set(caps->features, feature);
}

static __always_inline __nodiscard bool cpucaps_has(const cpucaps_t* restrict caps, cpu_feature_t feature)
{
    return bitmap_isset(caps->features, feature);
}
