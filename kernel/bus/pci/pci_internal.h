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

/// Gives room in the windows of the function's bus to each BAR that the firmware left without an address,
/// outside those windows, or over another BAR or a bridge window of the bus. Then turns on decoding of the spaces that all have room
/// @pre pci_read_bars has read the BARs of the function and of all the other new functions of its bus
/// @note A BAR that doesn't fit stays unassigned, with the others of its space, which then doesn't decode
void pci_assign_bars(pci_function_t* restrict function);
