#include <limine.h>
#include <vex/fb_device.h>
#include <vex/fbcons.h>
#include <vex/hhdm.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/mmio.h>
#include <vex/string.h>
#include <vex/vprot.h>

static volatile __used __section(".limine_requests") struct limine_framebuffer_request fb_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST_ID,
    .revision = 0,
};

static const fb_device_ops_t bootfb_ops = {}; // Scanned out directly, without a palette

static fb_device_t bootfb = {
    .ops = &bootfb_ops,
};

static console_t* bootfb_console = nullptr;
static bool bootfb_found = false;

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

static void init_bootfb(void)
{
    if(fb_request.response == nullptr) {
        return;
    }

    for(size_t i = 0; i < fb_request.response->framebuffer_count; ++i) {
        struct limine_framebuffer* fb = fb_request.response->framebuffers[i];

        if(!describe(fb, &bootfb.info)) {
            continue;
        }

        // Limine hands out an HHDM address
        // but Vex only maps usable blocks there
        bootfb.phys = virt_to_phys(fb->address);
        bootfb.cache = VPROT_CACHE_WC;
        bootfb.virt = mmio_map(bootfb.phys, bootfb.info.size, VPROT_RW | bootfb.cache);

        if(bootfb.virt == nullptr) {
            pr_warning("bootfb: failed to map the framebuffer");
            return;
        }

        bootfb_found = true;

#ifdef CONFIG_FBCONS
        bootfb_console = fbcons_attach(&bootfb.info, bootfb.virt);
#endif

        return;
    }
}

initcall_bsp_heap(init_bootfb, INIT_LAST);

static void init_bootfb_device(void)
{
    if(!bootfb_found) {
        return;
    }

    int retval = fb_device_add(&bootfb, bootfb_console);

    if(retval < 0) {
        pr_warning("bootfb: %s", strerror(retval));
        return;
    }

    pr_inform("bootfb: %s: %ux%u, %u bpp", bootfb.chr.name, bootfb.info.width, bootfb.info.height, bootfb.info.bpp);
}

initcall_bsp_dev(init_bootfb_device, INIT_MIDDLE);
