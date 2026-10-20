#pragma once

#include <usr/vex/fb.h>

#define VEXFB_FLAG_FLUSH           __k_VEXFB_FLAG_FLUSH
#define VEXFB_FLUSH_MAX_RECTANGLES __k_VEXFB_FLUSH_MAX_RECTANGLES

#define VEXFB_GET_INFO    __k_VEXFB_GET_INFO
#define VEXFB_FLUSH       __k_VEXFB_FLUSH
#define VEXFB_GET_PALETTE __k_VEXFB_GET_PALETTE
#define VEXFB_SET_PALETTE __k_VEXFB_SET_PALETTE

typedef struct __k_vexfb_channel vexfb_channel_t;
typedef struct __k_vexfb_info vexfb_info_t;
typedef struct __k_vexfb_rect vexfb_rect_t;
typedef struct __k_vexfb_flush vexfb_flush_t;
typedef struct __k_vexfb_palette vexfb_palette_t;
