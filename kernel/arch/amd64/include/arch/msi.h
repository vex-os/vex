#pragma once

#include <vex/types.h>

typedef struct hart_local_s hart_local_t;

void arch_msi_compose(const hart_local_t* restrict hart, unsigned vector, uint64_t* restrict address, uint32_t* restrict data);
