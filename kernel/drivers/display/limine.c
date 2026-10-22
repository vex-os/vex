#include <limine.h>
#include <vex/fb_interface.h>
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

static_assert(sizeof(fb_interface_t) <= SLAB_MAX_SIZE);

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

static void liminefb_release(const fb_interface_t* restrict interface)
{
    mmio_unmap(interface->virt);
    slab_free((void*)(interface));
}

static const fb_interface_ops_t liminefb_ops = {
    .release = &liminefb_release,
};

static fb_interface_t* liminefb_create(const struct limine_framebuffer* restrict fb)
{
    fb_interface_t* interface = slab_alloc(sizeof(fb_interface_t));

    if(interface == nullptr) {
        pr_warning("liminefb: out of memory");
        return nullptr;
    }

    memset(interface, 0, sizeof(fb_interface_t));

    interface->ops = &liminefb_ops;

    if(!describe(fb, &interface->info)) {
        slab_free(interface);
        return nullptr;
    }

    // Limine hands out an HHDM address
    // but Vex only maps usable blocks there
    interface->phys = virt_to_phys(fb->address);
    interface->cache = VPROT_CACHE_WC;
    interface->virt = mmio_map(interface->phys, interface->info.size, VPROT_RW | interface->cache);

    if(interface->virt == nullptr) {
        pr_warning("liminefb: failed to map a framebuffer");
        slab_free(interface);
        return nullptr;
    }

    return interface;
}

static void init_liminefb(void)
{
    if(fb_request.response == nullptr) {
        return;
    }

    for(size_t i = 0; i < fb_request.response->framebuffer_count; ++i) {
        fb_interface_t* interface = liminefb_create(fb_request.response->framebuffers[i]);

        if(interface) {
            fb_boot_add(interface);
        }
    }
}

initcall_bsp_heap(init_liminefb, INIT_LAST);
