#pragma once

#include <vex/compiler.h>

typedef struct console_s console_t;
typedef struct __k_vexfb_info vexfb_info_t;

/// @param virt The caller's mapping of the memory info describes
/// @return The console, for fb_device_add, or nullptr when the format isn't supported
console_t* fbcons_attach(const vexfb_info_t* restrict info, void* restrict virt);
