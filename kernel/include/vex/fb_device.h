#pragma once

#include <vex/chr_device.h>
#include <vex/compiler.h>
#include <vex/fb.h>
#include <vex/types.h>

typedef struct dev_unit_s dev_unit_t;

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

    /// @note Also called for a boot framebuffer a native driver claimed before it was registered
    void (*release)(const fb_device_t* restrict device);
};

struct fb_device_s {
    chr_device_t chr; // Its name, ops and ctxdata belong to the fb core
    const fb_device_ops_t* ops;
    vexfb_info_t info;
    uintptr_t phys;   // Of the info.size bytes userspace maps
    unsigned cache;   // A VPROT_CACHE value for them
    void* virt;       // The kernel's mapping of them, for read and write
    dev_unit_t* unit; // Taken by fb_device_add and freed by fb_device_remove
    fb_device_t* boot_next;
    void* ctxdata;
};

/// Queues a framebuffer set up before the kernel, to be added at dev INIT_LAST
/// unless a native driver claims its memory first; the first one queued gets fbcons
/// @pre The heap stage, before any boot framebuffer is added
void fb_boot_add(fb_device_t* restrict device);

/// Takes the boot framebuffers within the memory out of the queue and releases them
/// @pre A native driver's probe, before its fb_device_add for the same memory
/// @retval -EBUSY The boot framebuffers are already added
/// @note fbcons, if it drew to one of them, moves to the next device added
int fb_boot_claim(uintptr_t phys, size_t size);

int fb_device_add(fb_device_t* restrict device);
int fb_device_remove(fb_device_t* restrict device);
