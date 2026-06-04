#ifndef DBBBDE94_B1A0_4205_949D_F9F3536C1BA2
#define DBBBDE94_B1A0_4205_949D_F9F3536C1BA2
#pragma once

#include <core/cdefs.h>

enum {
    CHR_EVENT_DEVICE_ADDED,
    CHR_EVENT_DEVICE_REMOVED,
};

struct chr_device;

typedef void (*chr_event_handler_type)(unsigned event, const struct chr_device* restrict device, void* restrict ctxdata);

int chr_event_register_handler(chr_event_handler_type handler, void* restrict ctxdata);
int chr_event_unregister_handler(chr_event_handler_type handler, void* restrict ctxdata);
void chr_event_notify(unsigned event, const struct chr_device* restrict device);

#endif /* DBBBDE94_B1A0_4205_949D_F9F3536C1BA2 */
