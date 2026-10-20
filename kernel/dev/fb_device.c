#include <vex/chr_device.h>
#include <vex/dev_name.h>
#include <vex/errno.h>
#include <vex/fb_device.h>
#include <vex/page.h>
#include <vex/string.h>
#include <vex/uaccess.h>

#define RECT_BATCH  16
#define COLOR_BATCH 64

static const fb_device_t* fb_of(const chr_device_t* restrict device)
{
    return device->ctxdata;
}

static uint64_t mapped_size(const fb_device_t* restrict fb)
{
    return page_align_up(fb->info.size);
}

static bool clip_rect(const fb_device_t* restrict fb, vexfb_rect_t* restrict rect)
{
    if(rect->x >= fb->info.width || rect->y >= fb->info.height || rect->width == 0 || rect->height == 0) {
        return false;
    }

    if(rect->width > fb->info.width - rect->x) {
        rect->width = fb->info.width - rect->x;
    }

    if(rect->height > fb->info.height - rect->y) {
        rect->height = fb->info.height - rect->y;
    }

    return true;
}

static int flush_rects(const fb_device_t* restrict fb, uintptr_t user_rects, uint32_t count)
{
    vexfb_rect_t batch[RECT_BATCH];

    while(count > 0) {
        uint32_t chunk = count < RECT_BATCH ? count : RECT_BATCH;

        if(user_copyin(batch, user_rects, chunk * sizeof(vexfb_rect_t)) < 0) {
            return -EFAULT;
        }

        size_t kept = 0;

        for(uint32_t i = 0; i < chunk; ++i) {
            if(clip_rect(fb, &batch[i])) {
                batch[kept] = batch[i];
                kept += 1;
            }
        }

        if(kept > 0) {
            int retval = fb->ops->flush(fb, batch, kept);

            if(retval < 0) {
                return retval;
            }
        }

        user_rects += chunk * sizeof(vexfb_rect_t);
        count -= chunk;
    }

    return 0;
}

static int ioctl_flush(const fb_device_t* restrict fb, uintptr_t argument)
{
    vexfb_flush_t flush;

    if(user_copyin(&flush, argument, sizeof(flush)) < 0) {
        return -EFAULT;
    }

    if(flush.count > VEXFB_FLUSH_MAX_RECTANGLES) {
        return -EINVAL;
    }

    if(fb->ops->flush == nullptr) {
        return 0;
    }

    return flush_rects(fb, flush.rects, flush.count);
}

static int ioctl_palette(const fb_device_t* restrict fb, uintptr_t argument, bool set)
{
    vexfb_palette_t palette;

    if(user_copyin(&palette, argument, sizeof(palette)) < 0) {
        return -EFAULT;
    }

    uint32_t size = fb->info.palette_size;

    if(size == 0 || palette.first > size || palette.count > size - palette.first) {
        return -EINVAL;
    }

    uint32_t colors[COLOR_BATCH];
    uint32_t first = palette.first;
    uint32_t remaining = palette.count;
    uintptr_t user_colors = palette.colors;

    while(remaining > 0) {
        uint32_t chunk = remaining < COLOR_BATCH ? remaining : COLOR_BATCH;
        size_t bytes = chunk * sizeof(uint32_t);
        int retval;

        if(set) {
            if(user_copyin(colors, user_colors, bytes) < 0) {
                return -EFAULT;
            }

            retval = fb->ops->set_palette(fb, first, chunk, colors);
        }
        else {
            retval = fb->ops->get_palette(fb, first, chunk, colors);

            if(retval == 0 && user_copyout(user_colors, colors, bytes) < 0) {
                retval = -EFAULT;
            }
        }

        if(retval < 0) {
            return retval;
        }

        first += chunk;
        user_colors += bytes;
        remaining -= chunk;
    }

    return 0;
}

static ssize_t fb_chr_read(const chr_device_t* restrict device, off_t offset, void* restrict buffer, size_t size)
{
    const fb_device_t* fb = fb_of(device);

    if(offset < 0) {
        return -EINVAL;
    }

    if((uint64_t)(offset) >= fb->info.size) {
        return 0;
    }

    uint64_t left = fb->info.size - offset;

    if(size > left) {
        size = left;
    }

    memcpy(buffer, (const uint8_t*)(fb->virt) + offset, size);

    return size;
}

// Flushes the lines it touched, so a writer through write() needs no VEXFB_FLUSH
static ssize_t fb_chr_write(const chr_device_t* restrict device, off_t offset, const void* restrict buffer, size_t size)
{
    const fb_device_t* fb = fb_of(device);

    if(offset < 0) {
        return -EINVAL;
    }

    if((uint64_t)(offset) >= fb->info.size) {
        return -ENOSPC;
    }

    uint64_t left = fb->info.size - offset;

    if(size > left) {
        size = left;
    }

    if(size == 0) {
        return 0;
    }

    memcpy((uint8_t*)(fb->virt) + offset, buffer, size);

    if(fb->ops->flush) {
        vexfb_rect_t rect;
        rect.x = 0;
        rect.y = offset / fb->info.pitch;
        rect.width = fb->info.width;
        rect.height = (offset + size - 1) / fb->info.pitch - rect.y + 1;

        if(clip_rect(fb, &rect)) {
            int retval = fb->ops->flush(fb, &rect, 1);

            if(retval < 0) {
                return retval;
            }
        }
    }

    return size;
}

static int fb_chr_ioctl(const chr_device_t* restrict device, uint32_t request, uintptr_t argument)
{
    const fb_device_t* fb = fb_of(device);

    switch(request) {
        case VEXFB_GET_INFO:
            return user_copyout(argument, &fb->info, sizeof(fb->info)) < 0 ? -EFAULT : 0;

        case VEXFB_FLUSH:
            return ioctl_flush(fb, argument);

        case VEXFB_GET_PALETTE:
            return ioctl_palette(fb, argument, false);

        case VEXFB_SET_PALETTE:
            return ioctl_palette(fb, argument, true);

        default:
            return -ENOTTY;
    }
}

static int fb_chr_mmap(const chr_device_t* restrict device, off_t offset, size_t length)
{
    uint64_t size = mapped_size(fb_of(device));

    if(offset < 0 || (uint64_t)(offset) > size || length > size - offset) {
        return -ENXIO;
    }

    return 0;
}

static int fb_chr_resolve_page(const chr_device_t* restrict device, off_t offset, uintptr_t* restrict phys, unsigned* restrict cache)
{
    const fb_device_t* fb = fb_of(device);

    if(offset < 0 || (uint64_t)(offset) >= mapped_size(fb)) {
        return -ENXIO;
    }

    *phys = fb->phys + offset;
    *cache = fb->cache;

    return 0;
}

static void fb_chr_release(const chr_device_t* restrict device)
{
    const fb_device_t* fb = fb_of(device);

    if(fb->ops->release) {
        fb->ops->release(fb);
    }
}

static const chr_device_ops_t fb_chr_ops = {
    .read = &fb_chr_read,
    .write = &fb_chr_write,
    .ioctl = &fb_chr_ioctl,
    .mmap = &fb_chr_mmap,
    .resolve_page = &fb_chr_resolve_page,
    .release = &fb_chr_release,
};

int fb_device_add(fb_device_t* restrict device, console_t* restrict console)
{
    device->chr.ops = &fb_chr_ops;
    device->chr.ctxdata = device;
    device->chr.console = console;

    int retval = dev_unit_alloc(&dev_prefix_fbd, &device->unit);

    if(retval < 0) {
        return retval;
    }

    retval = chr_device_add(&device->chr, device->unit, DEV_NAME_NONE);

    if(retval < 0) {
        dev_unit_free(device->unit);
    }

    return retval;
}

int fb_device_remove(fb_device_t* restrict device)
{
    int retval = chr_device_remove(&device->chr);

    if(retval == 0) {
        dev_unit_free(device->unit);
    }

    return retval;
}
