#pragma once

#include <vex/compiler.h>

typedef struct chr_device_s chr_device_t;

enum {
    CHR_EVENT_DEVICE_ADDED,
    CHR_EVENT_DEVICE_REMOVED,
};

typedef void (*chr_event_handler_t)(unsigned event, const chr_device_t* restrict device, void* restrict ctxdata);

void chr_event_lock(void);
void chr_event_unlock(void);

int chr_event_subscribe(chr_event_handler_t handler, void* restrict ctxdata);
int chr_event_unsubscribe(chr_event_handler_t handler, void* restrict ctxdata);
void chr_event_notify(unsigned event, const chr_device_t* restrict device);
