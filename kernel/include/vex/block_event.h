#pragma once

#include <vex/compiler.h>

typedef struct block_interface_s block_interface_t;

enum {
    BLOCK_EVENT_INTERFACE_ADDED,
    BLOCK_EVENT_INTERFACE_REMOVED,
    BLOCK_EVENT_MEDIA_CONNECTED,
    BLOCK_EVENT_MEDIA_DISCONNECTED,
    BLOCK_EVENT_PARTITION_ADDED,
    BLOCK_EVENT_PARTITION_REMOVED,
};

typedef void (*block_event_handler_t)(unsigned event, const block_interface_t* restrict interface, void* restrict ctxdata);

void block_event_lock(void);
void block_event_unlock(void);

int block_event_subscribe(block_event_handler_t handler, void* restrict ctxdata);
int block_event_unsubscribe(block_event_handler_t handler, void* restrict ctxdata);
void block_event_notify(unsigned event, const block_interface_t* restrict interface);
