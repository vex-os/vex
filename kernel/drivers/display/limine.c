#include <limine.h>
#include <vex/fb_device.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/mmio.h>
#include <vex/slab.h>
#include <vex/string.h>
#include <vex/vprot.h>

static volatile __used __section(".limine_requests") struct limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
};

static_assert(sizeof(fb_device_t) <= SLAB_MAX_SIZE);

static bool describe(const struct limine_framebuffer* restrict fb, vexfb_info_t* restrict info)
{
    if(fb->memory_model != LIMINE_FRAMEBUFFER_RGB || fb->bpp == 0 || fb->bpp % 8) {
        return false;
    }

    if(fb->width > UINT32_MAX || fb->height > UINT32_MAX || fb->pitch > UINT32_MAX) {
        return false;
    }

    memset(info, 0, sizeof(vexfb_info_t));

    info->width = fb->width;
    info->height = fb->height;
    info->pitch = fb->pitch;
    info->bpp = fb->bpp;
    info->red.bit_offset = fb->red_mask_shift;
    info->red.bit_length = fb->red_mask_size;
    info->green.bit_offset = fb->green_mask_shift;
    info->green.bit_length = fb->green_mask_size;
    info->blue.bit_offset = fb->blue_mask_shift;
    info->blue.bit_length = fb->blue_mask_size;
    info->size = fb->pitch * fb->height;

    return true;
}

static void liminefb_release(const fb_device_t* restrict device)
{
    mmio_unmap(device->virt);
    slab_free((void*)(device));
}

static const fb_device_ops_t liminefb_ops = {
    .release = &liminefb_release,
};

static fb_device_t* liminefb_create(const struct limine_framebuffer* restrict fb)
{
    fb_device_t* device = slab_alloc(sizeof(fb_device_t));

    if(device == nullptr) {
        pr_warning("liminefb: out of memory");
        return nullptr;
    }

    memset(device, 0, sizeof(fb_device_t));

    device->ops = &liminefb_ops;

    if(!describe(fb, &device->info)) {
        slab_free(device);
        return nullptr;
    }

    // Limine hands out an HHDM address
    // but Vex only maps usable blocks there
    device->phys = virt_to_phys(fb->address);
    device->cache = VPROT_CACHE_WC;
    device->virt = mmio_map(device->phys, device->info.size, VPROT_RW | device->cache);

    if(device->virt == nullptr) {
        pr_warning("liminefb: failed to map a framebuffer");
        slab_free(device);
        return nullptr;
    }

    return device;
}

static void init_liminefb(void)
{
    if(fb_request.response == nullptr) {
        return;
    }

    for(size_t i = 0; i < fb_request.response->framebuffer_count; ++i) {
        fb_device_t* device = liminefb_create(fb_request.response->framebuffers[i]);

        if(device) {
            fb_boot_add(device);
        }
    }
}

initcall_bsp_heap(init_liminefb, INIT_LAST);
