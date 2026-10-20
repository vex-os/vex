#pragma once

#include <vex/compiler.h>
#include <vex/irq.h>
#include <vex/limits.h>
#include <vex/mmio.h>
#include <vex/pmio.h>
#include <vex/types.h>

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

#define PCI_COMMAND_IO  0x0001
#define PCI_COMMAND_MEM 0x0002

#define PCI_BAR_IO            0x0001
#define PCI_BAR_MEM_TYPE_MASK 0x0006
#define PCI_BAR_MEM_32BIT     0x0000
#define PCI_BAR_MEM_64BIT     0x0004
#define PCI_BAR_PREFETCH      0x0008
#define PCI_BAR_ADDR_MASK_IO  (~((uintptr_t)(0x03)))
#define PCI_BAR_ADDR_MASK_MEM (~((uintptr_t)(0x0F)))

#define PCI_CAP_PM   0x01 // Power Management
#define PCI_CAP_AGP  0x02 // AGP
#define PCI_CAP_MSI  0x05 // Message Signalled Interrupts
#define PCI_CAP_PCIX 0x07 // PCI-X
#define PCI_CAP_MSIX 0x11 // MSI-X
#define PCI_CAP_PCIE 0x10 // PCI Express

#define PCI_STS_CAP_LIST 0x0010

#define PCI_MSI_CONTROL    0x02 // Message Control
#define PCI_MSI_ADDRESS_LO 0x04 // Message Address
#define PCI_MSI_ADDRESS_HI 0x08 // Message Upper Address, 64-bit capable functions only
#define PCI_MSI_DATA_32    0x08 // Message Data
#define PCI_MSI_DATA_64    0x0C
#define PCI_MSI_MASK_32    0x0C // Mask Bits, per-vector masking capable functions only
#define PCI_MSI_MASK_64    0x10

#define PCI_MSI_CONTROL_ENABLE 0x0001 // MSI Enable
#define PCI_MSI_CONTROL_MME    0x0070 // Multiple Message Enable
#define PCI_MSI_CONTROL_64BIT  0x0080 // 64 Bit Address Capable
#define PCI_MSI_CONTROL_PVM    0x0100 // Per-Vector Masking Capable

enum {
    PCI_BAR_TYPE_MMIO,
    PCI_BAR_TYPE_PMIO,
};

typedef struct pci_bar_s pci_bar_t;
typedef struct pci_bus_s pci_bus_t;
typedef struct pci_bus_ops_s pci_bus_ops_t;
typedef struct pci_device_s pci_device_t;
typedef struct pci_bus_private_s pci_bus_private_t;
typedef struct pci_driver_s pci_driver_t;
typedef struct pci_device_private_s pci_device_private_t;
typedef struct pci_driver_id_s pci_driver_id_t;
typedef struct pci_driver_ops_s pci_driver_ops_t;
typedef struct pci_driver_private_s pci_driver_private_t;

struct pci_bar_s {
    unsigned type;
    union {
        mmio_region_t mmio;
        pmio_region_t pmio;
    };
};

struct pci_bus_ops_s {
    uint8_t (*read8)(pci_bus_t* restrict bus, unsigned device_index, unsigned function, uintptr_t offset);
    uint16_t (*read16)(pci_bus_t* restrict bus, unsigned device_index, unsigned function, uintptr_t offset);
    uint32_t (*read32)(pci_bus_t* restrict bus, unsigned device_index, unsigned function, uintptr_t offset);
    void (*write8)(pci_bus_t* restrict bus, unsigned device_index, unsigned function, uintptr_t offset, uint8_t value);
    void (*write16)(pci_bus_t* restrict bus, unsigned device_index, unsigned function, uintptr_t offset, uint16_t value);
    void (*write32)(pci_bus_t* restrict bus, unsigned device_index, unsigned function, uintptr_t offset, uint32_t value);
};

struct pci_bus_private_s {
    pci_bus_t* next;
    pci_device_t* devices;
};

struct pci_bus_s {
    pci_bus_private_t priv;
    const pci_bus_ops_t* ops;
    const char* name;
    unsigned segment;
    unsigned index;
    void* ctxdata;
};

struct pci_device_private_s {
    pci_device_t* next;
    pci_driver_t* driver;
    unsigned irq_number;
    pci_bar_t bars[6];
    unsigned bars_mapped; // bitmask, bit N set = bars[N] initialized
};

struct pci_device_s {
    pci_device_private_t priv;
    pci_bus_t* bus;
    unsigned index;
    unsigned function;
    unsigned vendor_id;
    unsigned device_id;
    unsigned class_id;
    unsigned subclass_id;
    void* ctxdata;
};

struct pci_driver_id_s {
    unsigned vendor_id; // 0 = match by class/subclass instead of vendor+device
    unsigned device_id;
    unsigned class_id;
    unsigned subclass_id;
};

struct pci_driver_ops_s {
    int (*probe)(pci_device_t* restrict device, const pci_driver_id_t* restrict id);
    void (*remove)(pci_device_t* restrict device);
};

struct pci_driver_private_s {
    pci_driver_t* next;
};

struct pci_driver_s {
    pci_driver_private_t priv;
    const pci_driver_ops_t* ops;
    const pci_driver_id_t* ids;
};

#define PCI_DEVICE_CLASS(class_val, subclass_val) \
    ((pci_driver_id_t) {                          \
        .vendor_id = 0,                           \
        .device_id = 0,                           \
        .class_id = (class_val),                  \
        .subclass_id = (subclass_val),            \
    })

#define PCI_DEVICE_SPECIFIC(vendor_val, device_val) \
    ((pci_driver_id_t) {                            \
        .vendor_id = (vendor_val),                  \
        .device_id = (device_val),                  \
        .class_id = 0,                              \
        .subclass_id = 0,                           \
    })

#define PCI_DEVICE_END()         \
    ((pci_driver_id_t) {         \
        .vendor_id = UINT_MAX,   \
        .device_id = UINT_MAX,   \
        .class_id = UINT_MAX,    \
        .subclass_id = UINT_MAX, \
    })

int pci_bus_add(pci_bus_t* restrict bus);
int pci_bus_remove(pci_bus_t* restrict bus);

int pci_driver_add(pci_driver_t* restrict driver);
int pci_driver_remove(pci_driver_t* restrict driver);

pci_device_t* pci_find(unsigned segment, unsigned bus, unsigned device, unsigned function) __nodiscard;

uint8_t pci_cfg_read8(pci_device_t* restrict device, uintptr_t offset);
uint16_t pci_cfg_read16(pci_device_t* restrict device, uintptr_t offset);
uint32_t pci_cfg_read32(pci_device_t* restrict device, uintptr_t offset);

void pci_cfg_write8(pci_device_t* restrict device, uintptr_t offset, uint8_t value);
void pci_cfg_write16(pci_device_t* restrict device, uintptr_t offset, uint16_t value);
void pci_cfg_write32(pci_device_t* restrict device, uintptr_t offset, uint32_t value);

pci_bar_t* pci_bar_map(pci_device_t* restrict device, unsigned bar_index) __nodiscard;
void pci_bar_unmap(pci_device_t* restrict device, unsigned bar_index);

uint8_t pci_bar_read8(pci_bar_t* restrict bar, uintptr_t offset);
uint16_t pci_bar_read16(pci_bar_t* restrict bar, uintptr_t offset);
uint32_t pci_bar_read32(pci_bar_t* restrict bar, uintptr_t offset);
uint64_t pci_bar_read64(pci_bar_t* restrict bar, uintptr_t offset);

void pci_bar_read8s(pci_bar_t* restrict bar, uintptr_t offset, uint8_t* restrict data, size_t count);
void pci_bar_read16s(pci_bar_t* restrict bar, uintptr_t offset, uint16_t* restrict data, size_t count);
void pci_bar_read32s(pci_bar_t* restrict bar, uintptr_t offset, uint32_t* restrict data, size_t count);
void pci_bar_read64s(pci_bar_t* restrict bar, uintptr_t offset, uint64_t* restrict data, size_t count);

void pci_bar_write8(pci_bar_t* restrict bar, uintptr_t offset, uint8_t value);
void pci_bar_write16(pci_bar_t* restrict bar, uintptr_t offset, uint16_t value);
void pci_bar_write32(pci_bar_t* restrict bar, uintptr_t offset, uint32_t value);
void pci_bar_write64(pci_bar_t* restrict bar, uintptr_t offset, uint64_t value);

void pci_bar_write8s(pci_bar_t* restrict bar, uintptr_t offset, const uint8_t* restrict data, size_t count);
void pci_bar_write16s(pci_bar_t* restrict bar, uintptr_t offset, const uint16_t* restrict data, size_t count);
void pci_bar_write32s(pci_bar_t* restrict bar, uintptr_t offset, const uint32_t* restrict data, size_t count);
void pci_bar_write64s(pci_bar_t* restrict bar, uintptr_t offset, const uint64_t* restrict data, size_t count);

uint8_t pci_get_cap(pci_device_t* restrict device, uint8_t cap_id) __nodiscard;

void pci_enable_busmaster(pci_device_t* restrict device);
void pci_disable_busmaster(pci_device_t* restrict device);

int pci_request_irq(pci_device_t* restrict device, irq_handler_t handler, void* restrict ctxdata);
void pci_release_irq(pci_device_t* restrict device);
