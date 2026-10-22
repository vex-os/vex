#pragma once

#include <vex/fwnode.h>
#include <vex/pci.h>

typedef struct device_s device_t;

#define PCI_ROOT_WINDOW_MAX 16

typedef struct pci_root_s pci_root_t;

struct pci_root_s {
    pci_bus_t* bus;
    unsigned segment;
    fwnode_resource_t windows[PCI_ROOT_WINDOW_MAX]; // Ports and memory the host bridge passes on
    size_t num_windows;
};

struct pci_bus_s {
    pci_root_t* root;
    pci_function_t* bridge; // nullptr on a root bus
    device_t* device;       // The host bridge, or the bridge
    pci_config_space_t* config_space;
    unsigned number;
    pci_function_t* functions; // In the order of their addresses
};

/// Sizes the function's BARs and keeps what they say
/// @pre No driver has the function, as a BAR doesn't decode while it is sized
void pci_read_bars(pci_function_t* restrict function);

/// Gives the BARs that the firmware left without an address, or outside the windows of the function's bus,
/// room in those windows, and turns on decoding of the spaces that then all have room
/// @pre pci_read_bars has read the BARs
/// @note A BAR that doesn't fit stays unassigned, with the others of its space, which then doesn't decode
void pci_assign_bars(pci_function_t* restrict function);
