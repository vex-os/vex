#ifndef BAF7322B_9BD5_4ECE_B20C_153EF98E138C
#define BAF7322B_9BD5_4ECE_B20C_153EF98E138C
#pragma once

#include <core/cdefs.h>
#include <core/ireq.h>
#include <stddef.h>
#include <stdint.h>

enum {
    ISA_RESOURCE_MMIO,
    ISA_RESOURCE_PMIO,
    ISA_RESOURCE_DMA,
    ISA_RESOURCE_IRQ,
};

enum {
    ISA_DRIVER_GENERIC,
    ISA_DRIVER_PROBING, // probe-only driver
};

struct isa_resource_mmio {
    volatile uint8_t* virt8;
    volatile uint16_t* virt16;
    volatile uint32_t* virt32;
    size_t length;
};

struct isa_resource_pmio {
    volatile uint8_t* virt8;
    volatile uint16_t* virt16;
    uintptr_t pmio;
    size_t length;
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
        struct isa_resource_mmio mmio;
        struct isa_resource_pmio pmio;
        struct isa_resource_dma dma;
        struct isa_resource_irq irq;
    };
};

struct isa_device_private {
    struct isa_device* next;
    struct isa_driver* driver;
    struct isa_resource* resources;
    size_t num_resources;
    void* pmio_ctxdata;
    void* acpi_node;
};

struct isa_device {
    struct isa_device_private priv;
    char identifier[64];
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
    char identifier[64];
    unsigned type;
};

extern uint8_t arch_isa_read8(struct isa_resource_pmio* restrict pmio, void* ctxdata, uintptr_t port);
extern uint16_t arch_isa_read16(struct isa_resource_pmio* restrict pmio, void* ctxdata, uintptr_t port);
extern void arch_isa_write8(struct isa_resource_pmio* restrict pmio, void* ctxdata, uintptr_t port, uint8_t value);
extern void arch_isa_write16(struct isa_resource_pmio* restrict pmio, void* ctxdata, uintptr_t port, uint16_t value);

extern int register_isa_device(struct isa_device* restrict device);
extern int unregister_isa_device(struct isa_device* restrict device);

extern int register_isa_driver(struct isa_driver* restrict driver);
extern int unregister_isa_driver(struct isa_driver* restrict driver);

extern struct isa_resource* isa_get_resource(struct isa_device* restrict device, unsigned type, unsigned index) __nodiscard;

extern int isa_request_irq(struct isa_device* restrict device, unsigned index, ireq_handler_type handler, void* ctxdata);
extern void isa_release_irq(struct isa_device* restrict device, unsigned index);

/// Resolves an ISA IRQ number to a GSI and trigger/polarity characteristics;
/// Applies any ACPI MADT interrupt source overrides; defaults to identity
/// mapping GSI numbers to IRQ lines edge-triggered, active-high if no override exists
extern void isa_irq_route(uint8_t line, uint32_t* out_gsi, unsigned* out_trigger, unsigned* out_polarity);

extern uint8_t isa_read8(struct isa_device* restrict device, struct isa_resource_pmio* restrict pmio, uintptr_t port);
extern uint16_t isa_read16(struct isa_device* restrict device, struct isa_resource_pmio* restrict pmio, uintptr_t port);
extern void isa_write8(struct isa_device* restrict device, struct isa_resource_pmio* restrict pmio, uintptr_t port, uint8_t value);
extern void isa_write16(struct isa_device* restrict device, struct isa_resource_pmio* restrict pmio, uintptr_t port, uint16_t value);

#endif /* BAF7322B_9BD5_4ECE_B20C_153EF98E138C */
