#ifndef FF32ABB5_460B_4345_9EE3_A2925175426A
#define FF32ABB5_460B_4345_9EE3_A2925175426A
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

enum {
    TRIGGER_LEVEL,
    TRIGGER_EDGE,
};

enum {
    ACTIVE_LOW,
    ACTIVE_HIGH,
};

struct intctl;
struct intctl_ops {
    void (*mask)(struct intctl* restrict controller, uint32_t gsi);
    void (*unmask)(struct intctl* restrict controller, uint32_t gsi);
    void (*route)(struct intctl* restrict controller, uint32_t gsi, uint8_t vector, uint32_t hart, unsigned trigger, unsigned polarity);
};

struct intctl_private {
    struct intctl* next;
};

struct intctl {
    struct intctl_private priv;
    const struct intctl_ops* ops;
    const char* identifier;
    uint32_t gsi_base;
    uint32_t gsi_count;
    void* ctxdata;
};

int add_intctl(struct intctl* restrict controller);
struct intctl* find_intctl(uint32_t gsi) __nodiscard;

void intctl_mask(struct intctl* restrict controller, uint32_t gsi);
void intctl_unmask(struct intctl* restrict controller, uint32_t gsi);
void intctl_route(struct intctl* restrict controller, uint32_t gsi, uint8_t vector, uint32_t hart, unsigned trigger, unsigned polarity);

#endif /* FF32ABB5_460B_4345_9EE3_A2925175426A */
