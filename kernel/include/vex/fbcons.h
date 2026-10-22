#pragma once

#include <vex/compiler.h>

typedef struct console_s console_t;
typedef struct __k_vexfb_info vexfb_info_t;

typedef void (*fbcons_restore_t)(void* restrict ctxdata);

/// @param virt The caller's mapping of the memory info describes
/// @param restore Brings back the mode info describes when a panic takes the screen; may be nullptr
/// @return The console, or nullptr when the format isn't supported
/// @note The console core replays the kprintf history onto the new screen
/// restore runs on the panicking hart with the other harts stopped, so it takes no lock and allocates nothing
console_t* fbcons_attach(const vexfb_info_t* restrict info, void* restrict virt, fbcons_restore_t restore, void* restrict ctxdata);

/// @post Nothing draws to the old memory, so the caller may unmap it
void fbcons_detach(void);
