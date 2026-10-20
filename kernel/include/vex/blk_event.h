#pragma once

#include <vex/compiler.h>

typedef struct blk_device_s blk_device_t;

enum {
    BLK_EVENT_DEVICE_ADDED,
    BLK_EVENT_DEVICE_REMOVED,
    BLK_EVENT_MEDIA_CONNECTED,
    BLK_EVENT_MEDIA_DISCONNECTED,
    BLK_EVENT_PARTITION_ADDED,
    BLK_EVENT_PARTITION_REMOVED,
};

typedef void (*blk_event_handler_t)(unsigned event, const blk_device_t* restrict device, void* restrict ctxdata);

void blk_event_lock(void);
void blk_event_unlock(void);

int blk_event_subscribe(blk_event_handler_t handler, void* restrict ctxdata);
int blk_event_unsubscribe(blk_event_handler_t handler, void* restrict ctxdata);
void blk_event_notify(unsigned event, const blk_device_t* restrict device);
