#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

enum {
    TRIGGER_LEVEL,
    TRIGGER_EDGE,
};

enum {
    ACTIVE_LOW,
    ACTIVE_HIGH,
};

typedef struct interrupt_controller_s interrupt_controller_t;
typedef struct interrupt_controller_ops_s interrupt_controller_ops_t;
typedef struct interrupt_controller_private_s interrupt_controller_private_t;

struct interrupt_controller_ops_s {
    void (*mask)(interrupt_controller_t* restrict controller, uint32_t gsi);
    void (*unmask)(interrupt_controller_t* restrict controller, uint32_t gsi);
    void (*route)(interrupt_controller_t* restrict controller, uint32_t gsi, uint8_t vector, uint32_t hart, unsigned trigger,
        unsigned polarity);
};

struct interrupt_controller_private_s {
    interrupt_controller_t* next;
};

struct interrupt_controller_s {
    interrupt_controller_private_t priv;
    const interrupt_controller_ops_t* ops;
    const char* name;
    uint32_t gsi_base;
    uint32_t gsi_count;
    void* ctxdata;
};

int interrupt_controller_add(interrupt_controller_t* restrict controller);
interrupt_controller_t* interrupt_controller_find(uint32_t gsi) __nodiscard;

void interrupt_controller_mask(interrupt_controller_t* restrict controller, uint32_t gsi);
void interrupt_controller_unmask(interrupt_controller_t* restrict controller, uint32_t gsi);
void interrupt_controller_route(interrupt_controller_t* restrict controller, uint32_t gsi, uint8_t vector, uint32_t hart, unsigned trigger,
    unsigned polarity);
