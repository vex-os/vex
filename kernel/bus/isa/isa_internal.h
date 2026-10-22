#pragma once

#include <vex/device_bus.h>

// What a PCI-ISA bridge's driver puts on its node, so that the firmware
// children with an id become devices on the bus, named by their first range
extern const bus_ops_t isa_bus_ops;
