#ifndef FF32ABB5_460B_4345_9EE3_A2925175426A
#define FF32ABB5_460B_4345_9EE3_A2925175426A
#pragma once

#include <core/cdefs.h>
#include <stdbool.h>
#include <stdint.h>

#define TRIGGER_LEVEL 0U
#define TRIGGER_EDGE  1U

#define ACTIVE_LOW  0U
#define ACTIVE_HIGH 1U

struct intctl;
struct intctl_ops {
    void (*mask)(struct intctl* restrict controller, uint32_t gsi);
    void (*unmask)(struct intctl* restrict controller, uint32_t gsi);
    void (*route)(struct intctl* restrict controller, uint32_t gsi, uint8_t vector, uint32_t hart, unsigned trigger, unsigned polarity);
};

struct intctl {
    struct intctl* next;
    const struct intctl_ops* ops;
    char identifier[64];
    uint32_t gsi_base;
    uint32_t gsi_count;
    void* privdata;
};

extern int register_intctl(struct intctl* restrict controller);
extern struct intctl* find_intctl(uint32_t gsi) __nodiscard;

extern void intctl_mask(struct intctl* restrict controller, uint32_t gsi);
extern void intctl_unmask(struct intctl* restrict controller, uint32_t gsi);
extern void intctl_route(struct intctl* restrict controller, uint32_t gsi, uint8_t vector, uint32_t hart, unsigned trigger,
    unsigned polarity);

#endif /* FF32ABB5_460B_4345_9EE3_A2925175426A */
