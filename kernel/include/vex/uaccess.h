#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

// The vms_copy functions on the calling process's address space; a kernel thread gets -EFAULT

ssize_t user_copyin(void* restrict dst, uintptr_t src, size_t size);
ssize_t user_copyin_str(char* restrict dst, uintptr_t src, size_t max_sz);
ssize_t user_copyout(uintptr_t dst, const void* restrict src, size_t size);
ssize_t user_copyout_str(uintptr_t dst, const char* restrict src, size_t max_sz);
