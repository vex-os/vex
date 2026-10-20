#pragma once

#include <vex/compiler.h>

typedef struct console_s console_t;
typedef struct __k_vexfb_info vexfb_info_t;

/// @param virt The caller's mapping of the memory info describes
/// @return The console, or nullptr when the format isn't supported
/// @note The console core replays the kprintf history onto the new screen
console_t* fbcons_attach(const vexfb_info_t* restrict info, void* restrict virt);

/// @post Nothing draws to the old memory, so the caller may unmap it
void fbcons_detach(void);
