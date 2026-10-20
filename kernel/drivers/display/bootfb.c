#include <limine.h>
#include <vex/fb_device.h>
#include <vex/fbcons.h>
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

typedef struct bootfb_s bootfb_t;

struct bootfb_s {
    fb_device_t device;
    bootfb_t* next;
};

static_assert(sizeof(bootfb_t) <= SLAB_MAX_SIZE);

static const fb_device_ops_t bootfb_ops = {}; // Scanned out directly, without a palette

static bootfb_t* bootfb_head = nullptr;
static console_t* bootfb_console = nullptr;

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

static bootfb_t* bootfb_create(const struct limine_framebuffer* restrict fb)
{
    bootfb_t* bootfb = slab_alloc(sizeof(bootfb_t));

    if(bootfb == nullptr) {
        pr_warning("bootfb: out of memory");
        return nullptr;
    }

    memset(bootfb, 0, sizeof(bootfb_t));

    fb_device_t* device = &bootfb->device;
    device->ops = &bootfb_ops;

    if(!describe(fb, &device->info)) {
        slab_free(bootfb);
        return nullptr;
    }

    // Limine hands out an HHDM address
    // but Vex only maps usable blocks there
    device->phys = virt_to_phys(fb->address);
    device->cache = VPROT_CACHE_WC;
    device->virt = mmio_map(device->phys, device->info.size, VPROT_RW | device->cache);

    if(device->virt == nullptr) {
        pr_warning("bootfb: failed to map a framebuffer");
        slab_free(bootfb);
        return nullptr;
    }

    return bootfb;
}

static void init_bootfb(void)
{
    if(fb_request.response == nullptr) {
        return;
    }

    bootfb_t** tail = &bootfb_head;

    for(size_t i = 0; i < fb_request.response->framebuffer_count; ++i) {
        bootfb_t* bootfb = bootfb_create(fb_request.response->framebuffers[i]);

        if(bootfb == nullptr) {
            continue;
        }

        *tail = bootfb;
        tail = &bootfb->next;
    }

#ifdef CONFIG_FBCONS
    if(bootfb_head) {
        bootfb_console = fbcons_attach(&bootfb_head->device.info, bootfb_head->device.virt);
    }
#endif
}

initcall_bsp_heap(init_bootfb, INIT_LAST);

static void init_bootfb_device(void)
{
    for(bootfb_t* bootfb = bootfb_head; bootfb; bootfb = bootfb->next) {
        fb_device_t* device = &bootfb->device;
        int retval = fb_device_add(device, bootfb == bootfb_head ? bootfb_console : nullptr);

        if(retval < 0) {
            pr_warning("bootfb: %s", strerror(retval));
            continue;
        }

        pr_inform("bootfb: %s: %ux%u, %u bpp", device->chr.name, device->info.width, device->info.height, device->info.bpp);
    }
}

initcall_bsp_dev(init_bootfb_device, INIT_MIDDLE);
