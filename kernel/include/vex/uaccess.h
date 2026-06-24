#ifndef C9E11902_F4B4_482D_A0EC_04C6B65A46A2
#define C9E11902_F4B4_482D_A0EC_04C6B65A46A2
#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

ssize_t user_copyin(void* restrict dst, uintptr_t src, size_t sz);
ssize_t user_copyin_str(char* restrict dst, uintptr_t src, size_t max_sz);
ssize_t user_copyout(uintptr_t dst, const void* restrict src, size_t sz);
ssize_t user_copyout_str(uintptr_t dst, const char* restrict src, size_t max_sz);

#endif /* C9E11902_F4B4_482D_A0EC_04C6B65A46A2 */
