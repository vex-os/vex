#ifndef D2B097A2_7B4F_425F_B9E1_BE9BBB7F0759
#define D2B097A2_7B4F_425F_B9E1_BE9BBB7F0759
#pragma once

#include <core/cdefs.h>

enum {
    BLK_EVENT_DEVICE_ADDED,
    BLK_EVENT_DEVICE_REMOVED,
    BLK_EVENT_MEDIA_CONNECTED,
    BLK_EVENT_MEDIA_DISCONNECTED,
    BLK_EVENT_PARTITION_ADDED,
    BLK_EVENT_PARTITION_REMOVED,
};

struct blk_device;

typedef void (*blk_event_handler_type)(unsigned event, const struct blk_device* restrict device, void* restrict ctxdata);

extern int blk_event_register_handler(blk_event_handler_type handler, void* restrict ctxdata);
extern int blk_event_unregister_handler(blk_event_handler_type handler, void* restrict ctxdata);
extern void blk_event_notify(unsigned event, const struct blk_device* restrict device);

#endif /* D2B097A2_7B4F_425F_B9E1_BE9BBB7F0759 */
