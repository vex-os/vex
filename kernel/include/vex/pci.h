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
typedef struct pci_config_space_s pci_config_space_t;

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
typedef struct pci_function_private_s pci_function_private_t;
typedef struct pci_function_s pci_function_t;

struct pci_address_s {
    unsigned segment;
    unsigned bus;
    unsigned device;
    unsigned function;
};

struct pci_function_private_s {
    pci_function_t* next; // On its bus
    pci_bus_t* bus;
    pci_config_space_t* config_space;
    device_t* device;     // Its fwnode is the companion
    unsigned irq_number;  // Of MSI, UINT_MAX without
    pci_bus_t* secondary; // The bus behind a bridge, nullptr on other functions
};

struct pci_function_s {
    pci_function_private_t priv;
    pci_address_t address;
    unsigned vendor_id;
    unsigned device_id;
    unsigned class_id;
    unsigned subclass_id;
};

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
pci_function_t* pci_function_of(device_t* restrict device) __nodiscard;

uint8_t pci_read_config8(pci_function_t* restrict function, uintptr_t offset);
uint16_t pci_read_config16(pci_function_t* restrict function, uintptr_t offset);
uint32_t pci_read_config32(pci_function_t* restrict function, uintptr_t offset);

void pci_write_config8(pci_function_t* restrict function, uintptr_t offset, uint8_t value);
void pci_write_config16(pci_function_t* restrict function, uintptr_t offset, uint16_t value);
void pci_write_config32(pci_function_t* restrict function, uintptr_t offset, uint32_t value);

uint8_t pci_get_cap(pci_function_t* restrict function, uint8_t cap_id) __nodiscard;

void pci_enable_busmaster(pci_function_t* restrict function);
void pci_disable_busmaster(pci_function_t* restrict function);

/// Routes the function's MSI to a handler; the bus takes it back when the driver lets go of the function
/// @note Without MSI, the driver takes the INTx pin through device_request_irq instead
/// @retval -ENODEV The function has no MSI
/// @retval -EBUSY MSI is already requested
/// @retval -ENOMEM No interrupt vector is free
int pci_request_msi(pci_function_t* restrict function, irq_handler_t handler, void* restrict ctxdata);
