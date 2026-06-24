#ifndef BAF7322B_9BD5_4ECE_B20C_153EF98E138C
#define BAF7322B_9BD5_4ECE_B20C_153EF98E138C
#pragma once

#include <vex/compiler.h>
#include <vex/ireq.h>
#include <vex/mmio.h>
#include <vex/pmio.h>
#include <vex/types.h>

enum {
    ISA_RESOURCE_MMIO,
    ISA_RESOURCE_PMIO,
    ISA_RESOURCE_DMA,
    ISA_RESOURCE_IRQ,
};

struct isa_resource_dma {
    uint8_t channel;
};

struct isa_resource_irq {
    uint8_t line;
    uint32_t gsi;
    unsigned trigger;
    unsigned polarity;
    unsigned ireq_number;
};

struct isa_resource {
    unsigned type;
    union {
        struct mmio_region mmio;
        struct pmio_region pmio;
        struct isa_resource_dma dma;
        struct isa_resource_irq irq;
    };
};

struct isa_device_private {
    struct isa_device* next;
    struct isa_driver* driver;
    struct isa_resource* resources;
    size_t num_resources;
    void* acpi_node;
};

struct isa_device {
    struct isa_device_private priv;
    const char* identifier;
    void* ctxdata;
};

struct isa_driver_ops {
    int (*probe)(struct isa_device* restrict device);
    void (*remove)(struct isa_device* restrict device);
};

struct isa_driver_private {
    struct isa_driver* next;
};

struct isa_driver {
    struct isa_driver_private priv;
    const struct isa_driver_ops* ops;
    const char* identifier;
};

int add_isa_device(struct isa_device* restrict device);
int remove_isa_device(struct isa_device* restrict device);

int add_isa_driver(struct isa_driver* restrict driver);
int remove_isa_driver(struct isa_driver* restrict driver);

struct isa_resource* isa_get_resource(struct isa_device* restrict device, unsigned type, unsigned index) __nodiscard;

int isa_request_irq(struct isa_device* restrict device, unsigned index, ireq_handler_type handler, void* ctxdata);
void isa_release_irq(struct isa_device* restrict device, unsigned index);

// Resolves an ISA IRQ number to a GSI and trigger/polarity characteristics.
// Applies any ACPI MADT interrupt source overrides; defaults to identity
// mapping GSI numbers to IRQ lines edge-triggered, active-high if no override exists.
void isa_irq_route(uint8_t line, uint32_t* out_gsi, unsigned* out_trigger, unsigned* out_polarity);

#endif /* BAF7322B_9BD5_4ECE_B20C_153EF98E138C */
