#pragma once

enum cpu_feature {
    CPU_FEATURE_FXSR,    // FPU FXSAVE/FXRSTOR
    CPU_FEATURE_PAT,     // Page Attribute Table
    CPU_FEATURE_NX,      // No-Execute bit in pmap entries
    CPU_FEATURE_PAGE_1G, // Very-large-sized pages
    CPU_FEATURE_XSAVE,   // FPU XSAVE/XRSTOR
    CPU_FEATURE_AVX,     // 256-bit vector math
    CPU_FEATURE_AVX512,  // 512-bit vector math
    CPU_FEATURE_SMEP,    // Supervisor Execution Prevention
    CPU_FEATURE_SMAP,    // Supervisor Access Prevention
    NUM_CPU_FEATURES,
};

struct cpucaps;

void arch_cpucaps_probe(struct cpucaps* restrict caps);
