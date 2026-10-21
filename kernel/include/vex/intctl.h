#pragma once

#include <vex/compiler.h>
#include <vex/irq.h>
#include <vex/types.h>

enum {
    TRIGGER_LEVEL,
    TRIGGER_EDGE,
};

enum {
    ACTIVE_LOW,
    ACTIVE_HIGH,
};

#define INTERRUPT_SHARED 0x01U // The handler may share the GSI with others that agree to

typedef struct interrupt_controller_s interrupt_controller_t;
typedef struct interrupt_controller_ops_s interrupt_controller_ops_t;
typedef struct interrupt_controller_private_s interrupt_controller_private_t;
typedef struct interrupt_action_s interrupt_action_t;

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

/// Routes a GSI to a handler; every handler on a shared GSI is called for each interrupt
/// @param flags INTERRUPT_*
/// @retval -ENODEV No interrupt controller has the GSI
/// @retval -EBUSY The GSI is taken without sharing, or with another trigger or polarity
int interrupt_attach(uint32_t gsi, unsigned trigger, unsigned polarity, unsigned flags, irq_handler_t handler, void* restrict ctxdata,
    interrupt_action_t** restrict action);

/// @post The handler isn't running anywhere and won't be called again
void interrupt_detach(interrupt_action_t* restrict action);
