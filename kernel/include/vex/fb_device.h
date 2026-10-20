#pragma once

#include <vex/chr_device.h>
#include <vex/compiler.h>
#include <vex/fb.h>
#include <vex/types.h>

typedef struct console_s console_t;

typedef struct fb_device_s fb_device_t;
typedef struct fb_device_ops_s fb_device_ops_t;

struct fb_device_ops_s {
    /// @pre The rectangles are clipped to the screen
    /// @note nullptr when the memory is scanned out directly, which also leaves VEXFB_FLAG_FLUSH clear
    int (*flush)(const fb_device_t* restrict device, const vexfb_rect_t* restrict rects, size_t count);

    /// @pre first + count is within info.palette_size
    /// @note nullptr when info.palette_size is zero
    int (*get_palette)(const fb_device_t* restrict device, uint32_t first, uint32_t count, uint32_t* restrict colors);

    /// @pre first + count is within info.palette_size
    /// @note nullptr when info.palette_size is zero
    int (*set_palette)(const fb_device_t* restrict device, uint32_t first, uint32_t count, const uint32_t* restrict colors);

    void (*release)(const fb_device_t* restrict device);
};

struct fb_device_s {
    chr_device_t chr; // Its name, ops and ctxdata belong to the fb core
    const fb_device_ops_t* ops;
    vexfb_info_t info;
    uintptr_t phys; // Of the info.size bytes userspace maps
    unsigned cache; // A VPROT_CACHE value for them
    void* virt;     // The kernel's mapping of them, for read and write
    void* ctxdata;
};

/// @param console Writes to the device's memory until its first open; may be nullptr
int fb_device_add(fb_device_t* restrict device, console_t* restrict console);
int fb_device_remove(fb_device_t* restrict device);
