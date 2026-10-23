#include <vex/errno.h>
#include <vex/fb.h>
#include <vex/fcntl.h>
#include <vex/mman.h>
#include <vex/syscall.h>
#include <vex/time.h>

#include "ktest.h"
#include "test.fbtest.amogus.h"

#define AMOGUS_SCALE 4
#define BACKGROUND   0x202040
#define STRIPE_COLOR 0xFFD800
#define STRIPE_TOP   40
#define STRIPE_LINES 8

static void wait_seconds(long seconds)
{
    struct __k_timespec start, now;

    ktest_syscall2(__k_SYS_GETTIME, __k_CLOCK_MONOTONIC, (long)&start);

    do {
        ktest_syscall1(__k_SYS_YIELD, 0);
        ktest_syscall2(__k_SYS_GETTIME, __k_CLOCK_MONOTONIC, (long)&now);
    } while(now.tv_sec - start.tv_sec < seconds);
}

static struct __k_vexfb_info info;

static long read_all(long fd, unsigned char* buffer, long size)
{
    long total = 0;

    while(total < size) {
        long count = ktest_read(fd, buffer + total, size - total);

        if(count <= 0) {
            return count;
        }

        total += count;
    }

    return total;
}

static unsigned int scale_channel(unsigned int value, unsigned int bit_length)
{
    if(bit_length >= 8) {
        return value << (bit_length - 8);
    }

    return value >> (8 - bit_length);
}

// From 0x00RRGGBB to the framebuffer's own pixel format
static unsigned int pack_color(unsigned int rgb)
{
    unsigned int red = scale_channel((rgb >> 16) & 0xFF, info.red.bit_length) << info.red.bit_offset;
    unsigned int green = scale_channel((rgb >> 8) & 0xFF, info.green.bit_length) << info.green.bit_offset;
    unsigned int blue = scale_channel(rgb & 0xFF, info.blue.bit_length) << info.blue.bit_offset;
    return red | green | blue;
}

static void store_pixel(unsigned char* pixel, unsigned int rgb)
{
    unsigned int value = pack_color(rgb);

    for(unsigned int i = 0; i < info.bpp / 8; ++i) {
        pixel[i] = (value >> (8 * i)) & 0xFF;
    }
}

static void put_pixel(unsigned char* fb, unsigned int x, unsigned int y, unsigned int rgb)
{
    store_pixel(fb + (unsigned long)y * info.pitch + (unsigned long)x * (info.bpp / 8), rgb);
}

int main(void)
{
    long fd = ktest_open("/dev/fbd0", __k_O_RDWR, 0);
    ktest_expect(fd >= 0, 1); // and fbcons stops drawing

    ktest_expect(ktest_syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_GET_INFO, (long)&info), 0);
    ktest_printf("fbtest: %u x %u, %u bpp\n", info.width, info.height, info.bpp);
    ktest_expect(info.palette_size, 0); // liminefb is direct color
    ktest_expect(info.flags, 0);        // liminefb is scanned out directly

    unsigned char* fb = (unsigned char*)ktest_syscall6(__k_SYS_MMAP, 0, info.size, __k_PROT_READ | __k_PROT_WRITE, __k_MAP_SHARED, fd, 0);
    ktest_expect((long)fb > 0, 1);

    // The mapping runs past the framebuffer
    ktest_expect(ktest_syscall6(__k_SYS_MMAP, 0, info.size + (1 << 20), __k_PROT_READ, __k_MAP_SHARED, fd, 0), -__k_ENXIO);

    for(unsigned int y = 0; y < info.height; ++y) {
        for(unsigned int x = 0; x < info.width; ++x) {
            put_pixel(fb, x, y, BACKGROUND);
        }
    }

    unsigned int left = (info.width - AMOGUS_WIDTH * AMOGUS_SCALE) / 2;
    unsigned int top = (info.height - AMOGUS_HEIGHT * AMOGUS_SCALE) / 2;

    for(unsigned int y = 0; y < AMOGUS_HEIGHT * AMOGUS_SCALE; ++y) {
        for(unsigned int x = 0; x < AMOGUS_WIDTH * AMOGUS_SCALE; ++x) {
            put_pixel(fb, left + x, top + y, amogus[(y / AMOGUS_SCALE) * AMOGUS_WIDTH + x / AMOGUS_SCALE]);
        }
    }

    struct __k_vexfb_rect whole = {};
    struct __k_vexfb_flush flush = {};

    whole.width = 100000;
    whole.height = 100000;
    flush.rects = (unsigned long)&whole;
    flush.count = 1;
    ktest_expect(ktest_syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_FLUSH, (long)&flush), 0); // clipped to the screen

    flush.count = __k_VEXFB_FLUSH_MAX_RECTANGLES + 1;
    ktest_expect(ktest_syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_FLUSH, (long)&flush), -__k_EINVAL);

    unsigned int colors[4];
    struct __k_vexfb_palette palette = {};

    palette.colors = (unsigned long)colors;
    palette.count = 4;
    ktest_expect(ktest_syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_GET_PALETTE, (long)&palette), -__k_EINVAL); // no palette
    ktest_expect(ktest_syscall3(__k_SYS_IOCTL, fd, 0x1234, 0), -__k_ENOTTY);

    // A yellow stripe through write() rather than the mapping
    static unsigned char line[16384];
    long written = 0;

    for(unsigned int x = 0; x < info.width; ++x) {
        store_pixel(line + x * (info.bpp / 8), STRIPE_COLOR);
    }

    for(unsigned int y = STRIPE_TOP; y < STRIPE_TOP + STRIPE_LINES; ++y) {
        ktest_syscall3(__k_SYS_SEEK, fd, (long)y * info.pitch, 0);
        written += ktest_syscall3(__k_SYS_WRITE, fd, (long)line, info.pitch);
    }

    ktest_expect(written == (long)STRIPE_LINES * info.pitch, 1);

    static unsigned char readback[16384];
    ktest_syscall3(__k_SYS_SEEK, fd, (long)STRIPE_TOP * info.pitch, 0);
    ktest_expect(read_all(fd, readback, info.pitch), info.pitch); // a read gives at most one chunk of the framebuffer

    int same = 1;

    for(unsigned int i = 0; i < info.width * (info.bpp / 8); ++i) {
        if(readback[i] != line[i] || fb[STRIPE_TOP * info.pitch + i] != line[i]) {
            same = 0;
        }
    }

    ktest_expect(same, 1); // read() and the mapping both see the stripe

    ktest_syscall3(__k_SYS_SEEK, fd, (long)info.size, 0);
    ktest_expect(ktest_syscall3(__k_SYS_READ, fd, (long)readback, 16), 0); // the end of the framebuffer
    ktest_expect(ktest_syscall3(__k_SYS_WRITE, fd, (long)readback, 16), -__k_ENOSPC);

    // The device is already open
    ktest_expect(ktest_open("/dev/fbd0", __k_O_RDWR | __k_O_EXCL, 0), -__k_EBUSY);

    wait_seconds(3);

    struct __k_vexfb_modes modes = {};
    ktest_expect(ktest_syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_GET_MODES, (long)&modes), 0);
    ktest_expect(modes.count > 0, 1);

    static struct __k_vexfb_mode list[32];
    modes.modes = (unsigned long)list;
    modes.count = 32;
    ktest_expect(ktest_syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_GET_MODES, (long)&modes), 0);
    ktest_printf("fbtest: preferred mode %u x %u\n", list[0].width, list[0].height);
    ktest_expect(list[0].flags, 1);

    struct __k_vexfb_mode mode = {};

    mode.width = 1001;
    mode.height = 768;
    mode.bpp = 32;
    ktest_expect(ktest_syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_SET_MODE, (long)&mode), -__k_EINVAL); // not a step of 8

    mode.width = 1024;
    ktest_expect(ktest_syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_SET_MODE, (long)&mode), 0);
    ktest_expect(ktest_syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_GET_INFO, (long)&info), 0);
    ktest_expect(info.width, 1024);
    ktest_expect(info.height, 768);
    ktest_expect(info.pitch, 4096);

    // Through the mapping from before the mode change, which faults in again
    for(unsigned int y = 0; y < info.height; ++y) {
        for(unsigned int x = 0; x < info.width; ++x) {
            put_pixel(fb, x, y, STRIPE_COLOR);
        }
    }

    left = (info.width - AMOGUS_WIDTH * AMOGUS_SCALE) / 2;
    top = (info.height - AMOGUS_HEIGHT * AMOGUS_SCALE) / 2;

    for(unsigned int y = 0; y < AMOGUS_HEIGHT * AMOGUS_SCALE; ++y) {
        for(unsigned int x = 0; x < AMOGUS_WIDTH * AMOGUS_SCALE; ++x) {
            put_pixel(fb, left + x, top + y, amogus[(y / AMOGUS_SCALE) * AMOGUS_WIDTH + x / AMOGUS_SCALE]);
        }
    }

    // Long enough to look at the screen
    wait_seconds(10);

    return ktest_result();
}
