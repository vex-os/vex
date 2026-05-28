// SPDX-License-Identifier: BSD-3-Clause
#ifndef BAF7322B_9BD5_4ECE_B20C_153EF98E138C
#define BAF7322B_9BD5_4ECE_B20C_153EF98E138C
#pragma once

#include <core/cdefs.h>
#include <core/ireq.h>
#include <stddef.h>
#include <stdint.h>

#define ISA_RES_MMIO 0
#define ISA_RES_PMIO 1
#define ISA_RES_DMA  2
#define ISA_RES_IRQ  3

struct isa_resource_mmio {
    uintptr_t base_phys;
    void* base_virt;
    size_t length;
};

struct isa_resource_pmio {
    uintptr_t base;
    size_t length;
};

struct isa_resource_dma {
    uint8_t channel;
};

struct isa_resource_irq {
    uint8_t irq;
    uint32_t gsi;
    unsigned trigger;     // TRIGGER_LEVEL / TRIGGER_EDGE
    unsigned polarity;    // ACTIVE_LOW / ACTIVE_HIGH
    unsigned ireq_number; // Filled by isa_request_irq
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

/// Abstracted mapped IO operations; on amd64, these plug
/// straight into pmio (pmread8, pmread16, pmwrite8, etc) and
/// on architectures that may suddenly need a SoundBlaster16
/// but don't have an IO bus these should be memory-mapped
struct isa_xmio_ops {
    uint8_t (*read8)(void* restrict privdata, uintptr_t port);
    uint16_t (*read16)(void* restrict privdata, uintptr_t port);
    void (*write8)(void* restrict privdata, uintptr_t port, uint8_t value);
    void (*write16)(void* restrict privdata, uintptr_t port, uint16_t value);
};

struct isa_device {
    struct isa_device* next;
    char identifier[64];
    struct isa_resource* resources;
    size_t num_resources;
    const struct isa_xmio_ops* xmio;
    void* xmio_privdata;
    void* acpi_node;
    void* privdata;
};

struct isa_driver_ops {
    int (*probe)(struct isa_device* restrict device);
    void (*remove)(struct isa_device* restrict device);
};

struct isa_driver {
    struct isa_driver* next;
    const struct isa_driver_ops* ops;
    char identifier[64]; // empty if the driver is probe-only
};

/// Adds an ISA device to the ISA device tee for later probing
/// through device drivers in INITFN_STAGE_DEV; this is public
/// for cases where the device is quite old and is non-orthodox
/// to the point it's not even present in the ACPI device tree
extern int register_isa_device(struct isa_device* restrict device);

extern int register_isa_driver(struct isa_driver* restrict driver);
extern struct isa_resource* isa_get_resource(struct isa_device* restrict device, unsigned type, unsigned index) __nodiscard;
extern int isa_request_irq(struct isa_device* restrict device, unsigned index, ireq_handler_type handler, void* privdata);
extern void isa_release_irq(struct isa_device* restrict device, unsigned index);

/// Resolves an ISA IRQ number to a GSI and trigger/polarity characteristics.
/// Applies any ACPI MADT interrupt source overrides; defaults to identity
/// mapping (gsi == irq), edge-triggered, active-high if no override exists
extern void isa_irq_route(uint8_t irq, uint32_t* out_gsi, unsigned* out_trigger, unsigned* out_polarity);

static __always_inline inline uint8_t isa_read8(struct isa_device* restrict device, uintptr_t port)
{
    return device->xmio->read8(device->xmio_privdata, port);
}

static __always_inline inline uint16_t isa_read16(struct isa_device* restrict device, uintptr_t port)
{
    return device->xmio->read16(device->xmio_privdata, port);
}

static __always_inline inline void isa_write8(struct isa_device* restrict device, uintptr_t port, uint8_t value)
{
    device->xmio->write8(device->xmio_privdata, port, value);
}

static __always_inline inline void isa_write16(struct isa_device* restrict device, uintptr_t port, uint16_t value)
{
    device->xmio->write16(device->xmio_privdata, port, value);
}

#endif /* BAF7322B_9BD5_4ECE_B20C_153EF98E138C */
