#pragma once

#include <vex/compiler.h>
#include <vex/fwnode.h>
#include <vex/intctl.h>
#include <vex/io.h>
#include <vex/irq.h>
#include <vex/limits.h>
#include <vex/types.h>

typedef struct device_s device_t;
typedef struct pci_bus_s pci_bus_t;

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

#define PCI_CFG_PRIMARY_BUS     0x18 // Of a PCI-PCI bridge
#define PCI_CFG_SECONDARY_BUS   0x19
#define PCI_CFG_SUBORDINATE_BUS 0x1A

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
#define PCI_CAP_AGP  0x02
#define PCI_CAP_MSI  0x05 // Message Signalled Interrupts
#define PCI_CAP_PCIX 0x07
#define PCI_CAP_MSIX 0x11
#define PCI_CAP_PCIE 0x10

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

#define PCI_BUS_MAX      256
#define PCI_DEVICE_MAX   32
#define PCI_FUNCTION_MAX 8

typedef struct pci_address_s pci_address_t;
typedef struct pci_bar_s pci_bar_t;
typedef struct pci_config_ops_s pci_config_ops_t;
typedef struct pci_config_private_s pci_config_private_t;
typedef struct pci_config_s pci_config_t;
typedef struct pci_device_private_s pci_device_private_t;
typedef struct pci_device_s pci_device_t;

struct pci_address_s {
    unsigned segment;
    unsigned bus;
    unsigned device;
    unsigned function;
};

struct pci_bar_s {
    io_region_t io;
    uintptr_t phys; // Of a BAR the CPU reaches through memory
    unsigned vprot; // Of that mapping
};

struct pci_config_ops_s {
    uint8_t (*read8)(pci_config_t* restrict config, const pci_address_t* restrict address, uintptr_t offset);
    uint16_t (*read16)(pci_config_t* restrict config, const pci_address_t* restrict address, uintptr_t offset);
    uint32_t (*read32)(pci_config_t* restrict config, const pci_address_t* restrict address, uintptr_t offset);
    void (*write8)(pci_config_t* restrict config, const pci_address_t* restrict address, uintptr_t offset, uint8_t value);
    void (*write16)(pci_config_t* restrict config, const pci_address_t* restrict address, uintptr_t offset, uint16_t value);
    void (*write32)(pci_config_t* restrict config, const pci_address_t* restrict address, uintptr_t offset, uint32_t value);
};

struct pci_config_private_s {
    pci_config_t* next;
};

// A way to reach the configuration space of the buses bus_start through bus_end of a segment
struct pci_config_s {
    pci_config_private_t priv;
    const pci_config_ops_t* ops;
    const char* name;
    unsigned segment;
    unsigned bus_start;
    unsigned bus_end;
    void* ctxdata;
};

struct pci_device_private_s {
    pci_device_t* next; // On its bus
    pci_bus_t* bus;
    pci_config_t* config;
    device_t* device;    // Its fwnode is the companion
    unsigned irq_number; // Of MSI, UINT_MAX without
    pci_bar_t bars[6];
    unsigned bars_mapped; // bitmask, bit N set = bars[N] initialized
    pci_bus_t* secondary; // The bus behind a bridge, nullptr on other functions
};

struct pci_device_s {
    pci_device_private_t priv;
    pci_address_t address;
    unsigned vendor_id;
    unsigned device_id;
    unsigned class_id;
    unsigned subclass_id;
};

/// @retval -EEXIST Another way already reaches some of the buses
int pci_config_add(pci_config_t* restrict config);

/// @return The way to reach the bus, which stays for good, or nullptr when there is none
pci_config_t* pci_config_find(unsigned segment, unsigned bus) __nodiscard;

/// Makes a node of the tree a host bridge and scans the hierarchy below it,
/// with the windows and interrupt routes the node's firmware node gives it
/// @retval -ENODEV No way reaches the configuration space of the root bus
/// @retval -ENOMEM Out of memory
int pci_root_attach(device_t* restrict host, unsigned segment, unsigned bus);

/// Lets go of what pci_root_attach made of the host bridge
/// @pre The host bridge has no children left
void pci_root_detach(device_t* restrict host);

/// Gets the PCI function that a node of the tree is
/// @return nullptr when the node isn't on a PCI bus
pci_device_t* pci_device_of(device_t* restrict device) __nodiscard;

uint8_t pci_cfg_read8(pci_device_t* restrict device, uintptr_t offset);
uint16_t pci_cfg_read16(pci_device_t* restrict device, uintptr_t offset);
uint32_t pci_cfg_read32(pci_device_t* restrict device, uintptr_t offset);

void pci_cfg_write8(pci_device_t* restrict device, uintptr_t offset, uint8_t value);
void pci_cfg_write16(pci_device_t* restrict device, uintptr_t offset, uint16_t value);
void pci_cfg_write32(pci_device_t* restrict device, uintptr_t offset, uint32_t value);

pci_bar_t* pci_bar_map(pci_device_t* restrict device, unsigned bar_index) __nodiscard;

/// pci_bar_map with a memory type other than uncached, such as VPROT_CACHE_WC for video memory
/// @pre The BAR isn't mapped yet, or is mapped with the same vprot
/// @return nullptr also for an I/O BAR
pci_bar_t* pci_bar_map_vprot(pci_device_t* restrict device, unsigned bar_index, unsigned vprot) __nodiscard;
void pci_bar_unmap(pci_device_t* restrict device, unsigned bar_index);

/// Maps a range of the bus address space that the device decodes for others, as a bridge to another bus does
/// @param type FWNODE_RESOURCE_PORT or FWNODE_RESOURCE_MEMORY, as the range is on the bus
/// @retval -ERANGE The host bridge doesn't pass the range on
/// @retval -ENOMEM No address space to map memory into
int pci_map_range(pci_device_t* restrict device, unsigned type, uint64_t start, uint64_t length, io_region_t* restrict region);

void pci_unmap_range(io_region_t* restrict region);

uint8_t pci_get_cap(pci_device_t* restrict device, uint8_t cap_id) __nodiscard;

void pci_enable_busmaster(pci_device_t* restrict device);
void pci_disable_busmaster(pci_device_t* restrict device);

/// Takes MSI where the device has it, or its INTx pin as the firmware routes it, shared with other devices
/// @retval -ENODEV The device has neither, or the firmware routes no interrupt for the pin
/// @retval -EBUSY The pin's GSI is taken by a handler that doesn't share it
int pci_request_irq(pci_device_t* restrict device, irq_handler_t handler, void* restrict ctxdata);
void pci_release_irq(pci_device_t* restrict device);

static __always_inline uint8_t pci_config_read8(pci_config_t* restrict config, const pci_address_t* restrict address, uintptr_t offset)
{
    return config->ops->read8(config, address, offset);
}

static __always_inline uint16_t pci_config_read16(pci_config_t* restrict config, const pci_address_t* restrict address, uintptr_t offset)
{
    return config->ops->read16(config, address, offset);
}

static __always_inline uint32_t pci_config_read32(pci_config_t* restrict config, const pci_address_t* restrict address, uintptr_t offset)
{
    return config->ops->read32(config, address, offset);
}

static __always_inline void pci_config_write8(pci_config_t* restrict config, const pci_address_t* restrict address, uintptr_t offset,
    uint8_t value)
{
    config->ops->write8(config, address, offset, value);
}

static __always_inline void pci_config_write16(pci_config_t* restrict config, const pci_address_t* restrict address, uintptr_t offset,
    uint16_t value)
{
    config->ops->write16(config, address, offset, value);
}

static __always_inline void pci_config_write32(pci_config_t* restrict config, const pci_address_t* restrict address, uintptr_t offset,
    uint32_t value)
{
    config->ops->write32(config, address, offset, value);
}
