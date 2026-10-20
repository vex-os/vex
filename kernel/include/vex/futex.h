#pragma once

#include <usr/vex/futex.h>
#include <vex/compiler.h>
#include <vex/types.h>

typedef struct vmspace_s vmspace_t;

#define FUTEX_SHARED __k_FUTEX_SHARED

int futex_wait(vmspace_t* restrict vmspace, uintptr_t address, uint32_t expected, uint64_t deadline);
size_t futex_wake(const vmspace_t* restrict vmspace, uintptr_t address, size_t count);
