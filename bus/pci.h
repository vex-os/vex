#ifndef DE230BAC_AA17_4780_B3C2_71F7DA67E522
#define DE230BAC_AA17_4780_B3C2_71F7DA67E522
#pragma once

#include <core/cdefs.h>
#include <stddef.h>
#include <stdint.h>

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
};

struct pci_driver;
struct pci_device {
    struct pci_device_private priv;
    struct pci_bus* bus;
    unsigned index;
    unsigned function;
    unsigned vendor_id;
    unsigned device_id;
    void* ctxdata;
};

struct pci_driver_id {
    unsigned vendor_id;
    unsigned device_id;
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

#define PCI_DEVICE(vendor, device) ((struct pci_driver_id) { .vendor_id = (vendor), .device_id = (device) })
#define PCI_DEVICE_END()           ((struct pci_driver_id) { .vendor_id = 0xFFFF, .device_id = 0xFFFF })

extern int register_pci_bus(struct pci_bus* restrict bus);
extern int unregister_pci_bus(struct pci_bus* restrict bus);

extern int register_pci_driver(struct pci_driver* restrict driver);
extern int unregister_pci_driver(struct pci_driver* restrict driver);

extern struct pci_device* pci_find(unsigned vendor_id, unsigned device_id, unsigned index) __nodiscard;
extern struct pci_device* pci_find_raw(unsigned segment, unsigned bus, unsigned device, unsigned function) __nodiscard;

extern uint8_t pci_read8(struct pci_device* restrict device, uintptr_t offset);
extern uint16_t pci_read16(struct pci_device* restrict device, uintptr_t offset);
extern uint32_t pci_read32(struct pci_device* restrict device, uintptr_t offset);

extern void pci_write8(struct pci_device* restrict device, uintptr_t offset, uint8_t value);
extern void pci_write16(struct pci_device* restrict device, uintptr_t offset, uint16_t value);
extern void pci_write32(struct pci_device* restrict device, uintptr_t offset, uint32_t value);

#endif /* DE230BAC_AA17_4780_B3C2_71F7DA67E522 */
