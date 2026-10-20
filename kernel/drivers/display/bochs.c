#include <vex/errno.h>
#include <vex/fb_device.h>
#include <vex/initcall.h>
#include <vex/kprintf.h>
#include <vex/pci.h>
#include <vex/slab.h>
#include <vex/spinlock.h>
#include <vex/string.h>
#include <vex/vprot.h>

#define BAR_VRAM 0
#define BAR_MMIO 2

#define MMIO_VGA   0x400 // Ports 0x3C0-0x3DF
#define MMIO_DISPI 0x500 // 16-bit registers, one per index

#define VGA_DAC_READ_INDEX  (MMIO_VGA + 0x07)
#define VGA_DAC_WRITE_INDEX (MMIO_VGA + 0x08)
#define VGA_DAC_DATA        (MMIO_VGA + 0x09)

#define DISPI_ID         0x00
#define DISPI_XRES       0x01
#define DISPI_YRES       0x02
#define DISPI_BPP        0x03
#define DISPI_ENABLE     0x04
#define DISPI_VIRT_WIDTH 0x06
#define DISPI_X_OFFSET   0x08
#define DISPI_Y_OFFSET   0x09

#define DISPI_ID_MIN 0xB0C0
#define DISPI_ID_MAX 0xB0C5

#define DISPI_ENABLED     0x0001
#define DISPI_8BIT_DAC    0x0020
#define DISPI_LFB_ENABLED 0x0040

#define PALETTE_SIZE 256

typedef struct bochsfb_s bochsfb_t;

struct bochsfb_s {
    fb_device_t device;
    pci_bar_t* mmio;
    spinlock_t dac_lock; // The DAC takes an index, then three data accesses per color
    unsigned dac_bits;
};

static_assert(sizeof(bochsfb_t) <= SLAB_MAX_SIZE);

static uint16_t dispi_read(pci_bar_t* restrict mmio, unsigned index)
{
    return pci_bar_read16(mmio, MMIO_DISPI + index * 2);
}

static void set_channel(vexfb_channel_t* restrict channel, uint32_t bit_offset, uint32_t bit_length)
{
    channel->bit_offset = bit_offset;
    channel->bit_length = bit_length;
}

// Reads the mode the firmware left behind; nothing is changed
static bool describe(pci_bar_t* restrict mmio, vexfb_info_t* restrict info, uint64_t* restrict offset)
{
    uint16_t enable = dispi_read(mmio, DISPI_ENABLE);

    if(0 == (enable & DISPI_ENABLED) || 0 == (enable & DISPI_LFB_ENABLED)) {
        return false;
    }

    memset(info, 0, sizeof(vexfb_info_t));

    uint32_t bpp = dispi_read(mmio, DISPI_BPP);

    switch(bpp) {
        case 8:
            info->palette_size = PALETTE_SIZE;
            break;
        case 15:
            set_channel(&info->red, 10, 5);
            set_channel(&info->green, 5, 5);
            set_channel(&info->blue, 0, 5);
            break;
        case 16:
            set_channel(&info->red, 11, 5);
            set_channel(&info->green, 5, 6);
            set_channel(&info->blue, 0, 5);
            break;
        case 24:
        case 32:
            set_channel(&info->red, 16, 8);
            set_channel(&info->green, 8, 8);
            set_channel(&info->blue, 0, 8);
            break;
        default:
            return false;
    }

    uint32_t bytes_per_pixel = (bpp + 7) / 8;
    uint32_t virt_width = dispi_read(mmio, DISPI_VIRT_WIDTH);

    info->width = dispi_read(mmio, DISPI_XRES);
    info->height = dispi_read(mmio, DISPI_YRES);
    info->bpp = 8 * bytes_per_pixel;
    info->pitch = (virt_width > info->width ? virt_width : info->width) * bytes_per_pixel;
    info->size = (uint64_t)(info->pitch) * info->height;

    *offset = (uint64_t)(dispi_read(mmio, DISPI_Y_OFFSET)) * info->pitch + dispi_read(mmio, DISPI_X_OFFSET) * bytes_per_pixel;

    return info->width && info->height;
}

static uint32_t dac_to_rgb(unsigned dac_bits, uint8_t value)
{
    if(dac_bits == 8) {
        return value;
    }

    return (value << 2) | (value >> 4);
}

static int bochsfb_get_palette(const fb_device_t* restrict device, uint32_t first, uint32_t count, uint32_t* restrict colors)
{
    bochsfb_t* bochsfb = device->ctxdata;
    bool intr_state = spinlock_acquire_intr(&bochsfb->dac_lock);

    pci_bar_write8(bochsfb->mmio, VGA_DAC_READ_INDEX, first);

    for(uint32_t i = 0; i < count; ++i) {
        uint32_t red = dac_to_rgb(bochsfb->dac_bits, pci_bar_read8(bochsfb->mmio, VGA_DAC_DATA));
        uint32_t green = dac_to_rgb(bochsfb->dac_bits, pci_bar_read8(bochsfb->mmio, VGA_DAC_DATA));
        uint32_t blue = dac_to_rgb(bochsfb->dac_bits, pci_bar_read8(bochsfb->mmio, VGA_DAC_DATA));
        colors[i] = (red << 16) | (green << 8) | blue;
    }

    spinlock_release_intr(&bochsfb->dac_lock, intr_state);

    return 0;
}

static int bochsfb_set_palette(const fb_device_t* restrict device, uint32_t first, uint32_t count, const uint32_t* restrict colors)
{
    bochsfb_t* bochsfb = device->ctxdata;
    unsigned shift = 8 - bochsfb->dac_bits;
    bool intr_state = spinlock_acquire_intr(&bochsfb->dac_lock);

    pci_bar_write8(bochsfb->mmio, VGA_DAC_WRITE_INDEX, first);

    for(uint32_t i = 0; i < count; ++i) {
        pci_bar_write8(bochsfb->mmio, VGA_DAC_DATA, ((colors[i] >> 16) & 0xFF) >> shift);
        pci_bar_write8(bochsfb->mmio, VGA_DAC_DATA, ((colors[i] >> 8) & 0xFF) >> shift);
        pci_bar_write8(bochsfb->mmio, VGA_DAC_DATA, (colors[i] & 0xFF) >> shift);
    }

    spinlock_release_intr(&bochsfb->dac_lock, intr_state);

    return 0;
}

static void bochsfb_release(const fb_device_t* restrict device)
{
    slab_free(device->ctxdata);
}

static const fb_device_ops_t bochsfb_ops = {
    .get_palette = &bochsfb_get_palette,
    .set_palette = &bochsfb_set_palette,
    .release = &bochsfb_release,
};

static void unmap_bars(pci_device_t* restrict pci_dev)
{
    pci_bar_unmap(pci_dev, BAR_VRAM);
    pci_bar_unmap(pci_dev, BAR_MMIO);
}

static int bochsfb_probe(pci_device_t* restrict pci_dev, const pci_driver_id_t* restrict id)
{
    pci_bar_t* mmio = pci_bar_map(pci_dev, BAR_MMIO);

    // UNDONE: old QEMU std-vga has no MMIO BAR
    // and only takes DISPI through ports 0x1CE/0x1CF
    if(mmio == nullptr || mmio->type != PCI_BAR_TYPE_MMIO) {
        pr_verbose("bochsfb: no register BAR");
        unmap_bars(pci_dev);
        return -ENODEV;
    }

    uint16_t dispi_id = dispi_read(mmio, DISPI_ID);

    if(dispi_id < DISPI_ID_MIN || dispi_id > DISPI_ID_MAX) {
        pr_verbose("bochsfb: unknown DISPI version 0x%04X", dispi_id);
        unmap_bars(pci_dev);
        return -ENODEV;
    }

    vexfb_info_t info;
    uint64_t offset;

    // UNDONE: set a mode of our own once modes can change
    if(!describe(mmio, &info, &offset)) {
        pr_verbose("bochsfb: no linear framebuffer mode is set");
        unmap_bars(pci_dev);
        return -ENODEV;
    }

    pci_bar_t* vram = pci_bar_map_vprot(pci_dev, BAR_VRAM, VPROT_RW | VPROT_CACHE_WC);

    if(vram == nullptr || offset + info.size > vram->mmio.length) {
        pr_warning("bochsfb: the mode doesn't fit the video memory");
        unmap_bars(pci_dev);
        return -ENODEV;
    }

    bochsfb_t* bochsfb = slab_alloc(sizeof(bochsfb_t));

    if(bochsfb == nullptr) {
        unmap_bars(pci_dev);
        return -ENOMEM;
    }

    memset(bochsfb, 0, sizeof(bochsfb_t));

    bochsfb->mmio = mmio;
    bochsfb->dac_lock = SPINLOCK_INIT;
    bochsfb->dac_bits = (dispi_read(mmio, DISPI_ENABLE) & DISPI_8BIT_DAC) ? 8 : 6;

    fb_device_t* device = &bochsfb->device;
    device->ops = &bochsfb_ops;
    device->info = info;
    device->phys = vram->phys + offset;
    device->cache = VPROT_CACHE_WC;
    device->virt = (uint8_t*)(vram->mmio.virt8) + offset;
    device->ctxdata = bochsfb;

    // NOTE: after a successful claim the
    // screen is ours even if the add below fails
    if(fb_boot_claim(vram->phys, vram->mmio.length) < 0) {
        pr_warning("bochsfb: boot framebuffers are already added, one may share the memory");
    }

    int retval = fb_device_add(device);

    if(retval < 0) {
        slab_free(bochsfb);
        unmap_bars(pci_dev);
        return retval;
    }

    pci_dev->ctxdata = bochsfb;

    pr_inform("bochsfb: %s: DISPI 0x%04X, %zu KiB of video memory", device->chr.name, dispi_id, (size_t)(vram->mmio.length / 1024));

    return 0;
}

// Calls through the fb core after this are refused, so the BARs can go
// now, while pci_dev still exists; the rest goes with the last reference
static void bochsfb_remove(pci_device_t* restrict pci_dev)
{
    bochsfb_t* bochsfb = pci_dev->ctxdata;

    if(bochsfb == nullptr) {
        return;
    }

    pci_dev->ctxdata = nullptr;

    fb_device_remove(&bochsfb->device);
    unmap_bars(pci_dev);
}

static const pci_driver_ops_t bochsfb_driver_ops = {
    .probe = &bochsfb_probe,
    .remove = &bochsfb_remove,
};

static const pci_driver_id_t bochsfb_ids[] = {
    PCI_DEVICE_SPECIFIC(0x1234, 0x1111),
    PCI_DEVICE_END(),
};

static pci_driver_t bochsfb_driver = {
    .ops = &bochsfb_driver_ops,
    .ids = bochsfb_ids,
};

static void init_bochsfb(void)
{
    pci_driver_add(&bochsfb_driver);
}

initcall_bsp_dev(init_bochsfb, INIT_MIDDLE);
