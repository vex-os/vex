#include <vex/fb.h>
#include <vex/fcntl.h>
#include <vex/mman.h>
#include <vex/syscall.h>
#include <vex/time.h>

#include "amogus.h"

#define AMOGUS_SCALE 4
#define BACKGROUND   0x202040
#define STRIPE_COLOR 0xFFD800
#define STRIPE_TOP   40
#define STRIPE_LINES 8

static long syscall1(long num, long a1)
{
    long ret;
    asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1) : "rcx", "r11", "memory");
    return ret;
}

static long syscall2(long num, long a1, long a2)
{
    long ret;
    asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
    return ret;
}

static long syscall3(long num, long a1, long a2, long a3)
{
    long ret;
    asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
    return ret;
}

static long syscall4(long num, long a1, long a2, long a3, long a4)
{
    long ret;
    register long r10 asm("r10") = a4;
    asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10) : "rcx", "r11", "memory");
    return ret;
}

static long syscall6(long num, long a1, long a2, long a3, long a4, long a5, long a6)
{
    long ret;
    register long r10 asm("r10") = a4;
    register long r8 asm("r8") = a5;
    register long r9 asm("r9") = a6;
    asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r10), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
    return ret;
}

static void printnumber(long number)
{
    char buffer[32];
    char digits[24];
    int length = 0;
    int count = 0;

    if(number < 0) {
        buffer[length++] = '-';
        number = -number;
    }

    do {
        digits[count++] = '0' + (number % 10);
        number /= 10;
    } while(number > 0);

    while(count > 0) {
        buffer[length++] = digits[--count];
    }

    buffer[length] = 0; // SYS_TEST reads up to the terminator

    syscall2(__k_SYS_TEST, (long)buffer, length);
}

static void wait_seconds(long seconds)
{
    struct __k_timespec start, now;

    syscall2(__k_SYS_GETTIME, __k_CLOCK_MONOTONIC, (long)&start);

    do {
        syscall1(__k_SYS_YIELD, 0);
        syscall2(__k_SYS_GETTIME, __k_CLOCK_MONOTONIC, (long)&now);
    } while(now.tv_sec - start.tv_sec < seconds);
}

static struct __k_vexfb_info info;

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
    printnumber(syscall6(__k_SYS_MOUNT, 0, (long)"/dev", (long)"devfs", 0, 0, 0)); // should print 0

    long fd = syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/fbd0", __k_O_RDWR, 0);
    printnumber(fd); // should print 0, and fbcons stops drawing

    printnumber(syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_GET_INFO, (long)&info)); // should print 0
    printnumber(info.width);
    printnumber(info.height);
    printnumber(info.bpp);
    printnumber(info.palette_size); // should print 0, liminefb is direct color
    printnumber(info.flags);        // should print 0, liminefb is scanned out directly

    unsigned char* fb = (unsigned char*)syscall6(__k_SYS_MMAP, 0, info.size, __k_PROT_READ | __k_PROT_WRITE, __k_MAP_SHARED, fd, 0);
    printnumber((long)fb > 0); // should print 1

    // should print -260 (ENXIO), the mapping runs past the framebuffer
    printnumber(syscall6(__k_SYS_MMAP, 0, info.size + (1 << 20), __k_PROT_READ, __k_MAP_SHARED, fd, 0));

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

    struct __k_vexfb_rect whole = { 0, 0, 100000, 100000 };
    struct __k_vexfb_flush flush = { (unsigned long)&whole, 1, 0 };
    printnumber(syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_FLUSH, (long)&flush)); // should print 0, clipped to the screen

    flush.count = __k_VEXFB_FLUSH_MAX_RECTANGLES + 1;
    printnumber(syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_FLUSH, (long)&flush)); // should print -226 (EINVAL)

    unsigned int colors[4];
    struct __k_vexfb_palette palette = { (unsigned long)colors, 0, 4 };
    printnumber(syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_GET_PALETTE, (long)&palette)); // should print -226 (EINVAL), no palette
    printnumber(syscall3(__k_SYS_IOCTL, fd, 0x1234, 0));                             // should print -259 (ENOTTY)

    // A yellow stripe through write() rather than the mapping
    static unsigned char line[16384];
    long written = 0;

    for(unsigned int x = 0; x < info.width; ++x) {
        store_pixel(line + x * (info.bpp / 8), STRIPE_COLOR);
    }

    for(unsigned int y = STRIPE_TOP; y < STRIPE_TOP + STRIPE_LINES; ++y) {
        syscall3(__k_SYS_SEEK, fd, (long)y * info.pitch, 0);
        written += syscall3(__k_SYS_WRITE, fd, (long)line, info.pitch);
    }

    printnumber(written == (long)STRIPE_LINES * info.pitch); // should print 1

    static unsigned char readback[16384];
    syscall3(__k_SYS_SEEK, fd, (long)STRIPE_TOP * info.pitch, 0);
    printnumber(syscall3(__k_SYS_READ, fd, (long)readback, info.pitch) == info.pitch); // should print 1

    int same = 1;

    for(unsigned int i = 0; i < info.width * (info.bpp / 8); ++i) {
        if(readback[i] != line[i] || fb[STRIPE_TOP * info.pitch + i] != line[i]) {
            same = 0;
        }
    }

    printnumber(same); // should print 1, read() and the mapping both see the stripe

    syscall3(__k_SYS_SEEK, fd, (long)info.size, 0);
    printnumber(syscall3(__k_SYS_READ, fd, (long)readback, 16));  // should print 0, the end of the framebuffer
    printnumber(syscall3(__k_SYS_WRITE, fd, (long)readback, 16)); // should print -250 (ENOSPC)

    // should print -210 (EBUSY), the device is already open
    printnumber(syscall4(__k_SYS_OPENAT, __k_AT_FDCWD, (long)"/dev/fbd0", __k_O_RDWR | __k_O_EXCL, 0));

    wait_seconds(3);

    struct __k_vexfb_modes modes = { 0, 0, 0 };
    printnumber(syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_GET_MODES, (long)&modes)); // should print 0
    printnumber(modes.count > 0);                                                // should print 1

    static struct __k_vexfb_mode list[32];
    modes.modes = (unsigned long)list;
    modes.count = 32;
    printnumber(syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_GET_MODES, (long)&modes)); // should print 0
    printnumber(list[0].width);
    printnumber(list[0].height);
    printnumber(list[0].flags); // should print 1 (VEXFB_MODE_PREFERRED) with QEMU's EDID

    struct __k_vexfb_mode mode = { 1001, 768, 32, 0 };
    printnumber(syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_SET_MODE, (long)&mode)); // should print -226 (EINVAL), not a step of 8

    mode.width = 1024;
    printnumber(syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_SET_MODE, (long)&mode)); // should print 0
    printnumber(syscall3(__k_SYS_IOCTL, fd, __k_VEXFB_GET_INFO, (long)&info)); // should print 0
    printnumber(info.width);                                                    // should print 1024
    printnumber(info.height);                                                   // should print 768
    printnumber(info.pitch);                                                    // should print 4096

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

    printnumber(1); // should print 1, the old mapping still works within the new size

    // Long enough to look at the screen; the panic after the exit brings
    // back the boot mode and takes the screen back for fbcons
    wait_seconds(10);

    return 0;
}
