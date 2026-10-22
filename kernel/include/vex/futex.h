#pragma once

#include <usr/vex/futex.h>
#include <vex/compiler.h>
#include <vex/types.h>

typedef struct vmspace_s vmspace_t;

#define FUTEX_SHARED __k_FUTEX_SHARED

/// Sleeps while the 32-bit word at address holds expected, until futex_wake
/// on the same address, the deadline, or pending work such as a signal
/// @param deadline Absolute time on the clock_source_now_ns scale, zero for none
/// @retval 0 Woken, possibly spuriously; the caller looks at the word again
/// @retval -EAGAIN The word didn't hold expected
/// @retval -ETIMEDOUT The deadline passed
/// @retval -EINTR Work is pending
/// @retval -EFAULT The address isn't mapped
/// @retval -EINVAL The address isn't 4-byte aligned
int futex_wait(vmspace_t* restrict vmspace, uintptr_t address, uint32_t expected, uint64_t deadline);

/// Wakes up to count waiters on the address, the longest waiting first
/// @return The number of threads woken
size_t futex_wake(const vmspace_t* restrict vmspace, uintptr_t address, size_t count);
