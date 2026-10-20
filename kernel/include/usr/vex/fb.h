#ifndef USR_VEX_FB_H
#define USR_VEX_FB_H

#include <vex/ioctl.h>
#include <vex/types.h>

#define __k_VEXFB_FLAG_FLUSH ((__k_uint32_t)(0x0001)) /* Memory isn't scanned out directly: changes show after VEXFB_FLUSH */

#define __k_VEXFB_FLUSH_MAX_RECTANGLES 256

struct __k_vexfb_channel {
    __k_uint32_t bit_offset;
    __k_uint32_t bit_length;
};

struct __k_vexfb_info {
    __k_uint32_t width;
    __k_uint32_t height;
    __k_uint32_t pitch;
    __k_uint32_t bpp;
    struct __k_vexfb_channel red; /* All four are zero length when palette_size isn't zero */
    struct __k_vexfb_channel green;
    struct __k_vexfb_channel blue;
    struct __k_vexfb_channel alpha;
    __k_uint64_t size;
    __k_uint32_t palette_size; /* Entries; zero for direct color */
    __k_uint32_t flags;
};

struct __k_vexfb_rect {
    __k_uint32_t x;
    __k_uint32_t y;
    __k_uint32_t width;
    __k_uint32_t height;
};

struct __k_vexfb_flush {
    __k_uint64_t rects; /* __k_vexfb_rect[count] */
    __k_uint32_t count;
    __k_uint32_t pad;
};

struct __k_vexfb_palette {
    __k_uint64_t colors; /* __k_uint32_t[count], 0x00RRGGBB */
    __k_uint32_t first;
    __k_uint32_t count;
};

#define __k_VEXFB_GET_INFO    __k_IOCTL_READ(sizeof(struct __k_vexfb_info), 'F', 0x01)
#define __k_VEXFB_FLUSH       __k_IOCTL_WRITE(sizeof(struct __k_vexfb_flush), 'F', 0x02)
#define __k_VEXFB_GET_PALETTE __k_IOCTL_WRITE(sizeof(struct __k_vexfb_palette), 'F', 0x03) /* The kernel fills colors */
#define __k_VEXFB_SET_PALETTE __k_IOCTL_WRITE(sizeof(struct __k_vexfb_palette), 'F', 0x04)

#endif /* USR_VEX_FB_H */
