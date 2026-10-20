#pragma once

#include <vex/compiler.h>
#include <vex/irq.h>
#include <vex/mmio.h>
#include <vex/pmio.h>
#include <vex/types.h>

enum {
    ISA_RESOURCE_MMIO,
    ISA_RESOURCE_PMIO,
    ISA_RESOURCE_DMA,
    ISA_RESOURCE_IRQ,
};

typedef struct isa_resource_dma_s isa_resource_dma_t;
typedef struct isa_resource_irq_s isa_resource_irq_t;
typedef struct isa_resource_s isa_resource_t;
typedef struct isa_device_s isa_device_t;
typedef struct isa_driver_s isa_driver_t;
typedef struct isa_device_private_s isa_device_private_t;
typedef struct isa_driver_ops_s isa_driver_ops_t;
typedef struct isa_driver_private_s isa_driver_private_t;

struct isa_resource_dma_s {
    uint8_t channel;
};

struct isa_resource_irq_s {
    uint8_t line;
    uint32_t gsi;
    unsigned trigger;
    unsigned polarity;
    unsigned irq_number;
};

struct isa_resource_s {
    unsigned type;
    union {
        mmio_region_t mmio;
        pmio_region_t pmio;
        isa_resource_dma_t dma;
        isa_resource_irq_t irq;
    };
};

struct isa_device_private_s {
    isa_device_t* next;
    isa_driver_t* driver;
    isa_resource_t* resources;
    size_t num_resources;
    void* acpi_node;
};

struct isa_device_s {
    isa_device_private_t priv;
    const char* name;
    void* ctxdata;
};

struct isa_driver_ops_s {
    int (*probe)(isa_device_t* restrict device);
    void (*remove)(isa_device_t* restrict device);
};

struct isa_driver_private_s {
    isa_driver_t* next;
};

struct isa_driver_s {
    isa_driver_private_t priv;
    const isa_driver_ops_t* ops;
    const char* name;
};

int isa_device_add(isa_device_t* restrict device);
int isa_device_remove(isa_device_t* restrict device);

int isa_driver_add(isa_driver_t* restrict driver);
int isa_driver_remove(isa_driver_t* restrict driver);

isa_resource_t* isa_get_resource(isa_device_t* restrict device, unsigned type, unsigned index) __nodiscard;

int isa_request_irq(isa_device_t* restrict device, unsigned index, irq_handler_t handler, void* ctxdata);
void isa_release_irq(isa_device_t* restrict device, unsigned index);

// Without a MADT override the GSI is the line itself, edge-triggered and active-high
void isa_irq_route(uint8_t line, uint32_t* out_gsi, unsigned* out_trigger, unsigned* out_polarity);
