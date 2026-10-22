#pragma once

#include <vex/compiler.h>

typedef struct block_device_s block_device_t;

enum {
    BLOCK_EVENT_DEVICE_ADDED,
    BLOCK_EVENT_DEVICE_REMOVED,
    BLOCK_EVENT_MEDIA_CONNECTED,
    BLOCK_EVENT_MEDIA_DISCONNECTED,
    BLOCK_EVENT_PARTITION_ADDED,
    BLOCK_EVENT_PARTITION_REMOVED,
};

typedef void (*block_event_handler_t)(unsigned event, const block_device_t* restrict device, void* restrict ctxdata);

void block_event_lock(void);
void block_event_unlock(void);

int block_event_subscribe(block_event_handler_t handler, void* restrict ctxdata);
int block_event_unsubscribe(block_event_handler_t handler, void* restrict ctxdata);
void block_event_notify(unsigned event, const block_device_t* restrict device);
