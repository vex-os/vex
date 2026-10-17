#pragma once

#include <vex/types.h>

void arch_msi_compose(unsigned vector, uint64_t* restrict address, uint32_t* restrict data);
