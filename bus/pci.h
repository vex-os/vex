#ifndef DE230BAC_AA17_4780_B3C2_71F7DA67E522
#define DE230BAC_AA17_4780_B3C2_71F7DA67E522
#pragma once

#include <core/cdefs.h>
#include <core/mmio.h>
#include <core/pmio.h>
#include <core/types.h>
#include <ireq/dispatch.h>
#include <limits.h>

#define PCI_CFG_VENDOR_ID       0x00
#define PCI_CFG_DEVICE_ID       0x02
#define PCI_CFG_COMMAND         0x04
#define PCI_CFG_STATUS          0x06
#define PCI_CFG_REVISION        0x08
#define PCI_CFG_PROG_IF         0x09
#define PCI_CFG_SUBCLASS        0x0A
#define PCI_CFG_CLASS           0x0B
#define PCI_CFG_CACHE_LINE_SIZE 0x0C
#define PCI_CFG_LATENCY_TIMER   0x0D
#define PCI_CFG_HEADER_TYPE     0x0E
#define PCI_CFG_BIST            0x0F
#define PCI_CFG_BAR0            0x10
#define PCI_CFG_BAR1            0x14
#define PCI_CFG_BAR2            0x18
#define PCI_CFG_BAR3            0x1C
#define PCI_CFG_BAR4            0x20
#define PCI_CFG_BAR5            0x24
#define PCI_CFG_CARDBUS_CIS     0x28
#define PCI_CFG_SUBSYS_VENDOR   0x2C
#define PCI_CFG_SUBSYS_ID       0x2E
#define PCI_CFG_ROM_BASE        0x30
#define PCI_CFG_CAP_PTR         0x34
#define PCI_CFG_INT_LINE        0x3C
#define PCI_CFG_INT_PIN         0x3D
#define PCI_CFG_MIN_GNT         0x3E
#define PCI_CFG_MAX_LAT         0x3F

#define PCI_HDR_TYPE_MASK    0x7F
#define PCI_HDR_TYPE_NORMAL  0x00
#define PCI_HDR_TYPE_BRIDGE  0x01
#define PCI_HDR_TYPE_CARDBUS 0x02
#define PCI_HDR_MULTIFUNC    0x80

#define PCI_CMD_IO_SPACE    0x0001
#define PCI_CMD_MEM_SPACE   0x0002
#define PCI_CMD_BUS_MASTER  0x0004
#define PCI_CMD_PARITY_ERR  0x0040
#define PCI_CMD_SERR        0x0100
#define PCI_CMD_INT_DISABLE 0x0400

#define PCI_BAR_IO            0x0001
#define PCI_BAR_MEM_TYPE_MASK 0x0006
#define PCI_BAR_MEM_32BIT     0x0000
#define PCI_BAR_MEM_64BIT     0x0004
#define PCI_BAR_PREFETCH      0x0008
#define PCI_BAR_ADDR_MASK_IO  (~((uintptr_t)(0x03)))
#define PCI_BAR_ADDR_MASK_MEM (~((uintptr_t)(0x0F)))

#define PCI_CAP_PM   0x01 ///< Power Management
#define PCI_CAP_AGP  0x02 ///< AGP
#define PCI_CAP_MSI  0x05 ///< Message Signalled Interrupts
#define PCI_CAP_PCIX 0x07 ///< PCI-X
#define PCI_CAP_MSIX 0x11 ///< MSI-X
#define PCI_CAP_PCIE 0x10 ///< PCI Express

#define PCI_STS_CAP_LIST 0x0010

enum {
    PCI_BAR_TYPE_MMIO,
    PCI_BAR_TYPE_PMIO,
};

struct pci_bar {
    unsigned type;
    union {
        struct mmio_region mmio;
        struct pmio_region pmio;
    };
};

struct pci_bus;
struct pci_bus_ops {
    uint8_t (*read8)(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset);
    uint16_t (*read16)(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset);
    uint32_t (*read32)(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset);
    void (*write8)(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset, uint8_t value);
    void (*write16)(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset, uint16_t value);
    void (*write32)(struct pci_bus* restrict bus, unsigned device_index, unsigned function, uintptr_t offset, uint32_t value);
};

struct pci_device;
struct pci_bus_private {
    struct pci_bus* next;
    struct pci_device* devices;
};

struct pci_bus {
    struct pci_bus_private priv;
    const struct pci_bus_ops* ops;
    char identifier[64];
    unsigned segment;
    unsigned index;
    void* ctxdata;
};

struct pci_device_private {
    struct pci_device* next;
    struct pci_driver* driver;
    unsigned ireq_number;
    struct pci_bar bars[6];
    unsigned bars_mapped; // bitmask, bit N set = bars[N] initialized
};

struct pci_driver;
struct pci_device {
    struct pci_device_private priv;
    struct pci_bus* bus;
    unsigned index;
    unsigned function;
    unsigned vendor_id;
    unsigned device_id;
    unsigned class_id;
    unsigned subclass_id;
    void* ctxdata;
};

struct pci_driver_id {
    unsigned vendor_id; ///< 0 = match by class/subclass instead of vendor+device
    unsigned device_id;
    unsigned class_id;
    unsigned subclass_id;
};

struct pci_driver_ops {
    int (*probe)(struct pci_device* restrict device, const struct pci_driver_id* restrict id);
    void (*remove)(struct pci_device* restrict device);
};

struct pci_driver_private {
    struct pci_driver* next;
};

struct pci_driver {
    struct pci_driver_private priv;
    const struct pci_driver_ops* ops;
    const struct pci_driver_id* ids;
};

#define PCI_DEVICE_CLASS(class_val, subclass_val) \
    ((struct pci_driver_id) {                     \
        .vendor_id = 0,                           \
        .device_id = 0,                           \
        .class_id = (class_val),                  \
        .subclass_id = (subclass_val),            \
    })

#define PCI_DEVICE_SPECIFIC(vendor_val, device_val) \
    ((struct pci_driver_id) {                       \
        .vendor_id = (vendor_val),                  \
        .device_id = (device_val),                  \
        .class_id = 0,                              \
        .subclass_id = 0,                           \
    })

#define PCI_DEVICE_END()         \
    ((struct pci_driver_id) {    \
        .vendor_id = UINT_MAX,   \
        .device_id = UINT_MAX,   \
        .class_id = UINT_MAX,    \
        .subclass_id = UINT_MAX, \
    })

extern int pci_bus_register(struct pci_bus* restrict bus);
extern int pci_bus_unregister(struct pci_bus* restrict bus);

extern int pci_driver_register(struct pci_driver* restrict driver);
extern int pci_driver_unregister(struct pci_driver* restrict driver);

extern struct pci_device* pci_find(unsigned segment, unsigned bus, unsigned device, unsigned function) __nodiscard;

extern uint8_t pci_cfg_read8(struct pci_device* restrict device, uintptr_t offset);
extern uint16_t pci_cfg_read16(struct pci_device* restrict device, uintptr_t offset);
extern uint32_t pci_cfg_read32(struct pci_device* restrict device, uintptr_t offset);

extern void pci_cfg_write8(struct pci_device* restrict device, uintptr_t offset, uint8_t value);
extern void pci_cfg_write16(struct pci_device* restrict device, uintptr_t offset, uint16_t value);
extern void pci_cfg_write32(struct pci_device* restrict device, uintptr_t offset, uint32_t value);

extern struct pci_bar* pci_bar_map(struct pci_device* restrict device, unsigned bar_index) __nodiscard;
extern void pci_bar_unmap(struct pci_device* restrict device, unsigned bar_index);

extern uint8_t pci_bar_read8(struct pci_bar* restrict bar, uintptr_t offset);
extern uint16_t pci_bar_read16(struct pci_bar* restrict bar, uintptr_t offset);
extern uint32_t pci_bar_read32(struct pci_bar* restrict bar, uintptr_t offset);
extern uint64_t pci_bar_read64(struct pci_bar* restrict bar, uintptr_t offset);

extern void pci_bar_read8_string(struct pci_bar* restrict bar, uintptr_t offset, uint8_t* restrict data, size_t count);
extern void pci_bar_read16_string(struct pci_bar* restrict bar, uintptr_t offset, uint16_t* restrict data, size_t count);
extern void pci_bar_read32_string(struct pci_bar* restrict bar, uintptr_t offset, uint32_t* restrict data, size_t count);
extern void pci_bar_read64_string(struct pci_bar* restrict bar, uintptr_t offset, uint64_t* restrict data, size_t count);

extern void pci_bar_write8(struct pci_bar* restrict bar, uintptr_t offset, uint8_t value);
extern void pci_bar_write16(struct pci_bar* restrict bar, uintptr_t offset, uint16_t value);
extern void pci_bar_write32(struct pci_bar* restrict bar, uintptr_t offset, uint32_t value);
extern void pci_bar_write64(struct pci_bar* restrict bar, uintptr_t offset, uint64_t value);

extern void pci_bar_write8_string(struct pci_bar* restrict bar, uintptr_t offset, const uint8_t* restrict data, size_t count);
extern void pci_bar_write16_string(struct pci_bar* restrict bar, uintptr_t offset, const uint16_t* restrict data, size_t count);
extern void pci_bar_write32_string(struct pci_bar* restrict bar, uintptr_t offset, const uint32_t* restrict data, size_t count);
extern void pci_bar_write64_string(struct pci_bar* restrict bar, uintptr_t offset, const uint64_t* restrict data, size_t count);

/// Returns the config space offset of the capability with the given ID,
/// or 0 if not found. Requires PCI_STS_CAP_LIST set in status register;
extern uint8_t pci_get_cap(struct pci_device* restrict device, uint8_t cap_id) __nodiscard;

extern void pci_enable_busmaster(struct pci_device* restrict device);
extern void pci_disable_busmaster(struct pci_device* restrict device);

extern int pci_request_irq(struct pci_device* restrict device, ireq_handler_type handler, void* restrict ctxdata);
extern void pci_release_irq(struct pci_device* restrict device);

#endif /* DE230BAC_AA17_4780_B3C2_71F7DA67E522 */
