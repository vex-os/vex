#pragma once

#include <vex/compiler.h>

typedef struct char_interface_s char_interface_t;

enum {
    CHAR_EVENT_INTERFACE_ADDED,
    CHAR_EVENT_INTERFACE_REMOVED,
    CHAR_EVENT_MEMORY_CHANGED,
};

typedef void (*char_event_handler_t)(unsigned event, const char_interface_t* restrict interface, void* restrict ctxdata);

void char_event_lock(void);
void char_event_unlock(void);

int char_event_subscribe(char_event_handler_t handler, void* restrict ctxdata);
int char_event_unsubscribe(char_event_handler_t handler, void* restrict ctxdata);
void char_event_notify(unsigned event, const char_interface_t* restrict interface);
