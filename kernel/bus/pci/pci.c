#include <arch/irq.h>
#include <arch/msi.h>
#include <vex/assert.h>
#include <vex/device.h>
#include <vex/device_bus.h>
#include <vex/driver.h>
#include <vex/errno.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/mmio.h>
#include <vex/mutex.h>
#include <vex/page.h>
#include <vex/pci.h>
#include <vex/pci_config.h>
#include <vex/spinlock.h>
#include <vex/sprintf.h>
#include <vex/vprot.h>

#define PCI_ROOT_WINDOW_MAX 16

#define PCI_ANY_FUNCTION 0xFFFF // In a firmware node's address

typedef struct pci_root_s pci_root_t;
typedef struct companion_search_s companion_search_t;

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

// The firmware gives a function's address on its bus as device << 16 | function, as ACPI's _ADR does
struct companion_search_s {
    uint64_t address;
    fwnode_t* node;
    bool found;
};

static const char* const pin_names[] = {
    "INTA",
    "INTB",
    "INTC",
    "INTD",
};

static void scan_bus(pci_bus_t* restrict bus);

static void visit_companion(const fwnode_t* child, void* arg)
{
    companion_search_t* search = arg;
    uint64_t address;

    if(search->found) {
        return;
    }

    if(fwnode_get_address(child, &address) < 0) {
        return;
    }

    if(address == search->address || address == ((search->address & ~UINT64_C(0xFFFF)) | PCI_ANY_FUNCTION)) {
        *search->node = *child;
        search->found = true;
    }
}

static bool find_companion(const pci_bus_t* restrict bus, const pci_address_t* restrict address, fwnode_t* restrict companion)
{
    const fwnode_t* bus_node = &bus->device->fwnode;

    if(bus_node->ops == nullptr) {
        return false;
    }

    companion_search_t search = {};
    search.address = (uint64_t)(address->device) << 16 | address->function;
    search.node = companion;

    fwnode_for_each_child(bus_node, &visit_companion, &search);

    return search.found;
}

static void log_device(pci_function_t* restrict function)
{
    pr_verbose("pci: found %04X:%04X at %s", function->vendor_id, function->device_id, function->priv.device->name);
}

static void collect_window(const fwnode_resource_t* resource, void* arg)
{
    pci_root_t* root = arg;
    bool passes_registers = resource->type == FWNODE_RESOURCE_PORT || resource->type == FWNODE_RESOURCE_MEMORY;

    if(!passes_registers || 0 == (resource->range.flags & FWNODE_RANGE_WINDOW) || root->num_windows == PCI_ROOT_WINDOW_MAX) {
        return;
    }

    root->windows[root->num_windows] = *resource;
    root->num_windows += 1;

    unsigned long long first = resource->range.start;
    unsigned long long last = resource->range.start + resource->range.length - 1;
    unsigned long long translation = resource->range.translation;

    pr_verbose("pci: segment %04X window %s 0x%llX-0x%llX, translation 0x%llX", root->segment,
        resource->type == FWNODE_RESOURCE_PORT ? "port" : "memory", first, last, translation);
}

static const fwnode_range_t* find_window(const pci_root_t* restrict root, unsigned type, uint64_t start, uint64_t length)
{
    for(size_t i = 0; i < root->num_windows; ++i) {
        const fwnode_resource_t* window = &root->windows[i];

        if(window->type == type && start >= window->range.start && start + length <= window->range.start + window->range.length) {
            return &window->range;
        }
    }

    return nullptr;
}

static uintptr_t bar_address(pci_function_t* restrict function, unsigned bar_index)
{
    uintptr_t offset = PCI_CFG_BAR0 + bar_index * 4;
    uint32_t raw = pci_read_config32(function, offset);

    if(raw & PCI_BAR_IO) {
        return raw & PCI_BAR_ADDR_MASK_IO;
    }

    if((raw & PCI_BAR_MEM_TYPE_MASK) == PCI_BAR_MEM_64BIT) {
        uint64_t raw_higher = pci_read_config32(function, offset + 4);

        return (raw & PCI_BAR_ADDR_MASK_MEM) | (raw_higher << 32);
    }

    return raw & PCI_BAR_ADDR_MASK_MEM;
}

static uint32_t probe_bar_mask(pci_function_t* restrict function, uintptr_t offset)
{
    uint32_t raw = pci_read_config32(function, offset);
    pci_write_config32(function, offset, UINT32_C(0xFFFFFFFF));
    uint32_t mask = pci_read_config32(function, offset);
    pci_write_config32(function, offset, raw);
    return mask;
}

static uint64_t bar_length(pci_function_t* restrict function, unsigned bar_index)
{
    uintptr_t offset = PCI_CFG_BAR0 + bar_index * 4;
    uint32_t raw = pci_read_config32(function, offset);

    // A half-written BAR would decode at
    // a bogus address while it is being probed
    // so we disable the BAR while probing its length
    uint16_t command = pci_read_config16(function, PCI_CFG_COMMAND);

    pci_write_config16(function, PCI_CFG_COMMAND, command & ~(PCI_COMMAND_IO | PCI_COMMAND_MEM));

    uint64_t mask = probe_bar_mask(function, offset);

    if(0 == (raw & PCI_BAR_IO) && (raw & PCI_BAR_MEM_TYPE_MASK) == PCI_BAR_MEM_64BIT && bar_index < 5) {
        mask |= (uint64_t)(probe_bar_mask(function, offset + 4)) << 32;
    }
    else {
        mask |= UINT64_C(0xFFFFFFFF00000000);
    }

    pci_write_config16(function, PCI_CFG_COMMAND, command);

    if(raw & PCI_BAR_IO) {
        // I/O BARs may hardwire the upper 16 bits to zero
        mask = (mask & PCI_BAR_ADDR_MASK_IO) | UINT64_C(0xFFFFFFFFFFFF0000);
    }
    else {
        mask &= PCI_BAR_ADDR_MASK_MEM;
    }

    if(mask == UINT64_C(0xFFFFFFFF00000000) || mask == UINT64_C(0xFFFFFFFFFFFF0000)) {
        return 0;
    }

    return ~mask + 1;
}

// Ports and memory reach the CPU through the windows of the host
// bridge, which can show the CPU ports as memory or the other way round
static int host_translate_range(device_t* restrict bus, unsigned* restrict type, uint64_t* restrict address, uint64_t length)
{
    const pci_root_t* root = bus->ctxdata;
    const fwnode_range_t* window = find_window(root, *type, *address, length);

    if(window == nullptr) {
        return -ERANGE;
    }

    *address += window->translation;

    if(window->flags & FWNODE_RANGE_OTHER_TYPE) {
        if(*type == FWNODE_RESOURCE_PORT) {
            *type = FWNODE_RESOURCE_MEMORY;
        }
        else {
            *type = FWNODE_RESOURCE_PORT;
        }
    }

    return 0;
}

// PCI-to-PCI Bridge Architecture 1.2, 9.1: a bridge the firmware gives no routes for
// passes a pin on as a pin of its own slot, rotated by the number of the device behind it
static int route_intx(const pci_function_t* restrict function, unsigned pin, fwnode_irq_t* restrict irq)
{
    const pci_function_t* current = function;

    while(true) {
        const pci_bus_t* bus = current->priv.bus;
        int retval = -ENOENT;

        const fwnode_t* bus_node = &bus->device->fwnode;

        if(bus_node->ops) {
            retval = fwnode_route_pci_interrupt(bus_node, current->address.device, pin, irq);
        }

        if(retval != -ENOENT || bus->bridge == nullptr) {
            return retval;
        }

        pin = (pin + current->address.device) % 4;
        current = bus->bridge;
    }
}

static int pci_get_range(device_t* restrict bus, device_t* restrict child, unsigned index, fwnode_resource_t* restrict range)
{
    pci_function_t* function = child->bus_data;

    if(index >= 6) {
        return -ENODEV;
    }

    uint64_t length = bar_length(function, index);

    if(length == 0) {
        return -ENODEV;
    }

    uint32_t raw = pci_read_config32(function, PCI_CFG_BAR0 + index * 4);

    *range = (fwnode_resource_t) {};
    range->type = (raw & PCI_BAR_IO) ? FWNODE_RESOURCE_PORT : FWNODE_RESOURCE_MEMORY;
    range->range.start = bar_address(function, index);
    range->range.length = length;

    return 0;
}

static int pci_get_irq(device_t* restrict bus, device_t* restrict child, unsigned index, fwnode_irq_t* restrict irq)
{
    pci_function_t* function = child->bus_data;
    unsigned pin = pci_read_config8(function, PCI_CFG_INT_PIN);

    if(index || pin == 0 || pin > sizeof(pin_names) / sizeof(pin_names[0])) {
        return -ENODEV;
    }

    if(route_intx(function, pin - 1, irq) < 0) {
        return -ENODEV;
    }

    irq->flags |= FWNODE_IRQ_SHARED; // PCI Local Bus 3.0, 2.2.6: INTx lines are wired-OR between functions

    pr_verbose("pci: %s %s is GSI %u", child->name, pin_names[pin - 1], irq->number);

    return 0;
}

// A child with a function address was merged with its function in the
// scan, or stands for an empty slot that a rescan finds the function for
static int pci_place_child(device_t* restrict bus, const fwnode_t* restrict child)
{
    uint64_t address;

    if(fwnode_get_address(child, &address) < 0) {
        return -ENOENT;
    }

    return 0;
}

static bool pci_match(device_t* restrict bus, device_t* restrict child, const driver_match_t* restrict match)
{
    const pci_function_t* function = child->bus_data;

    if(match->kind == DRIVER_MATCH_PCI_ID) {
        return match->vendor_id == function->vendor_id && match->device_id == function->device_id;
    }

    if(match->kind == DRIVER_MATCH_PCI_CLASS) {
        return match->class_id == function->class_id && match->subclass_id == function->subclass_id;
    }

    return false;
}

// The driver has let go, so the function stops decoding and mastering until the next one turns that on
static void release_msi(pci_function_t* restrict function)
{
    if(function->priv.irq_number == UINT_MAX) {
        return;
    }

    uint8_t cap = pci_get_cap(function, PCI_CAP_MSI);

    if(cap) {
        uint16_t control = pci_read_config16(function, cap + PCI_MSI_CONTROL);
        pci_write_config16(function, cap + PCI_MSI_CONTROL, control & ~(uint16_t)(PCI_MSI_CONTROL_ENABLE));
    }

    irq_handler_remove(function->priv.irq_number);

    function->priv.irq_number = UINT_MAX;
}

// PCI Local Bus 3.0, 6.2.2: Interrupt Disable masks the function's INTx pin, and MSI masks it as well
static void pci_enable_irq(device_t* restrict bus, device_t* restrict child, unsigned index)
{
    pci_function_t* function = child->bus_data;
    uint16_t command = pci_read_config16(function, PCI_CFG_COMMAND);

    pci_write_config16(function, PCI_CFG_COMMAND, command & ~(uint16_t)(PCI_CMD_INT_DISABLE));
}

static void pci_disable_irq(device_t* restrict bus, device_t* restrict child, unsigned index)
{
    pci_function_t* function = child->bus_data;
    uint16_t command = pci_read_config16(function, PCI_CFG_COMMAND);

    pci_write_config16(function, PCI_CFG_COMMAND, command | PCI_CMD_INT_DISABLE);
}

static void pci_release_child(device_t* restrict bus, device_t* restrict child)
{
    pci_function_t* function = child->bus_data;

    release_msi(function);
    pci_disable_busmaster(function);
}

static void pci_remove_child(device_t* restrict bus, device_t* restrict child)
{
    pci_function_t* function = child->bus_data;
    pci_bus_t* parent = function->priv.bus;

    kassert(function->priv.secondary == nullptr || function->priv.secondary->functions == nullptr);

    pci_function_t** prev = &parent->functions;

    while(*prev != function) {
        prev = &(*prev)->priv.next;
    }

    *prev = function->priv.next;

    kfree(function->priv.secondary);
    kfree(function);

    child->bus_data = nullptr;
}

static const bus_ops_t host_bus_ops = {
    .place_child = &pci_place_child,
    .match = &pci_match,
    .enable_irq = &pci_enable_irq,
    .disable_irq = &pci_disable_irq,
    .release_child = &pci_release_child,
    .remove_child = &pci_remove_child,
    .get_range = &pci_get_range,
    .get_irq = &pci_get_irq,
    .translate_range = &host_translate_range,
};

// A bridge passes ranges on unchanged, within the windows it is set up with
static const bus_ops_t bridge_bus_ops = {
    .place_child = &pci_place_child,
    .match = &pci_match,
    .enable_irq = &pci_enable_irq,
    .disable_irq = &pci_disable_irq,
    .release_child = &pci_release_child,
    .remove_child = &pci_remove_child,
    .get_range = &pci_get_range,
    .get_irq = &pci_get_irq,
};

static pci_bus_t* add_bus(pci_root_t* restrict root, pci_function_t* restrict bridge, pci_config_space_t* restrict config_space,
    device_t* restrict device, unsigned number)
{
    pci_bus_t* bus = kmalloc(sizeof(pci_bus_t));

    if(bus == nullptr) {
        return nullptr;
    }

    bus->root = root;
    bus->bridge = bridge;
    bus->device = device;
    bus->config_space = config_space;
    bus->number = number;
    bus->functions = nullptr;

    scan_bus(bus);

    return bus;
}

// UNDONE: a bridge the firmware left without bus numbers needs them assigned
static void add_bridge(pci_function_t* restrict bridge)
{
    const pci_bus_t* parent = bridge->priv.bus;
    const pci_address_t* address = &bridge->address;
    unsigned secondary = pci_read_config8(bridge, PCI_CFG_SECONDARY_BUS);
    unsigned subordinate = pci_read_config8(bridge, PCI_CFG_SUBORDINATE_BUS);

    if(secondary <= parent->number || subordinate < secondary) {
        pr_warning("pci: bridge at %04X:%02X:%02X.%u has no bus numbers", address->segment, address->bus, address->device,
            address->function);
        return;
    }

    pci_config_space_t* config_space = pci_config_space_find(address->segment, secondary);

    if(config_space == nullptr) {
        pr_warning("pci: no configuration access to bus %04X:%02X", address->segment, secondary);
        return;
    }

    bridge->priv.device->bus_ops = &bridge_bus_ops;

    bridge->priv.secondary = add_bus(parent->root, bridge, config_space, bridge->priv.device, secondary);

    if(bridge->priv.secondary == nullptr) {
        pr_warning("pci: out of memory");
    }
}

static void add_function(pci_bus_t* restrict bus, const pci_address_t* restrict address)
{
    char name[DEVICE_NAME_MAX];
    snprintf(name, sizeof(name), "pci:%04x:%02x:%02x.%x", address->segment, address->bus, address->device, address->function);

    fwnode_t companion;
    bool has_companion = find_companion(bus, address, &companion);

    pci_function_t* function = kmalloc(sizeof(pci_function_t));
    device_t* device = nullptr;

    if(function == nullptr || device_add(bus->device, has_companion ? &companion : nullptr, name, &device) < 0) {
        pr_warning("pci: out of memory");
        kfree(function);
        return;
    }

    device->bus_data = function;

    function->priv.bus = bus;
    function->priv.config_space = bus->config_space;
    function->priv.device = device;
    function->priv.irq_number = UINT_MAX;
    function->priv.secondary = nullptr;
    function->address = *address;
    function->vendor_id = pci_read_config16(function, PCI_CFG_VENDOR_ID);
    function->device_id = pci_read_config16(function, PCI_CFG_DEVICE_ID);
    function->class_id = pci_read_config8(function, PCI_CFG_CLASS);
    function->subclass_id = pci_read_config8(function, PCI_CFG_SUBCLASS);

    log_device(function);

    pci_function_t** tail = &bus->functions;

    while(*tail) {
        tail = &(*tail)->priv.next;
    }

    function->priv.next = nullptr;
    *tail = function;

    if((pci_read_config8(function, PCI_CFG_HEADER_TYPE) & PCI_HDR_TYPE_MASK) == PCI_HDR_TYPE_BRIDGE) {
        add_bridge(function);
    }
}

static void scan_slot(pci_bus_t* restrict bus, unsigned slot)
{
    pci_address_t address = {};
    address.segment = bus->root->segment;
    address.bus = bus->number;
    address.device = slot;

    if(pci_config_space_read16(bus->config_space, &address, PCI_CFG_VENDOR_ID) == 0xFFFF) {
        return;
    }

    unsigned header = pci_config_space_read8(bus->config_space, &address, PCI_CFG_HEADER_TYPE);
    unsigned num_functions = (header & PCI_HDR_MULTIFUNC) ? PCI_FUNCTION_MAX : 1;

    for(unsigned function = 0; function < num_functions; function += 1) {
        address.function = function;

        if(pci_config_space_read16(bus->config_space, &address, PCI_CFG_VENDOR_ID) != 0xFFFF) {
            add_function(bus, &address);
        }
    }
}

static void scan_bus(pci_bus_t* restrict bus)
{
    for(unsigned slot = 0; slot < PCI_DEVICE_MAX; slot += 1) {
        scan_slot(bus, slot);
    }
}

static int request_msi(pci_function_t* restrict function, uint8_t cap, irq_handler_t handler, void* restrict ctxdata)
{
    unsigned irq_number = irq_handler_add(UINT_MAX, handler, ctxdata);

    if(irq_number == UINT_MAX) {
        return -ENOMEM;
    }

    function->priv.irq_number = irq_number;

    uint64_t address;
    uint32_t data;
    arch_msi_compose(irq_number + ARCH_IRQ_BASE, &address, &data);

    uint16_t control = pci_read_config16(function, cap + PCI_MSI_CONTROL);
    control &= ~(uint16_t)(PCI_MSI_CONTROL_ENABLE | PCI_MSI_CONTROL_MME);

    pci_write_config16(function, cap + PCI_MSI_CONTROL, control);
    pci_write_config32(function, cap + PCI_MSI_ADDRESS_LO, (uint32_t)(address));

    uintptr_t mask_offset = cap + PCI_MSI_MASK_32;

    if(control & PCI_MSI_CONTROL_64BIT) {
        pci_write_config32(function, cap + PCI_MSI_ADDRESS_HI, (uint32_t)(address >> 32));
        pci_write_config16(function, cap + PCI_MSI_DATA_64, (uint16_t)(data));
        mask_offset = cap + PCI_MSI_MASK_64;
    }
    else {
        pci_write_config16(function, cap + PCI_MSI_DATA_32, (uint16_t)(data));
    }

    if(control & PCI_MSI_CONTROL_PVM) {
        pci_write_config32(function, mask_offset, pci_read_config32(function, mask_offset) & ~UINT32_C(1));
    }

    uint16_t cmd = pci_read_config16(function, PCI_CFG_COMMAND);
    pci_write_config16(function, PCI_CFG_COMMAND, cmd | PCI_CMD_INT_DISABLE);

    pci_write_config16(function, cap + PCI_MSI_CONTROL, control | PCI_MSI_CONTROL_ENABLE);

    return 0;
}

int pci_root_attach(device_t* restrict host, unsigned segment, unsigned bus)
{
    pci_config_space_t* config_space = pci_config_space_find(segment, bus);

    if(config_space == nullptr) {
        return -ENODEV;
    }

    pci_root_t* root = kmalloc(sizeof(pci_root_t));

    if(root == nullptr) {
        return -ENOMEM;
    }

    root->segment = segment;
    root->num_windows = 0;

    fwnode_for_each_resource(&host->fwnode, &collect_window, root);

    host->ctxdata = root;
    host->bus_ops = &host_bus_ops;
    root->bus = add_bus(root, nullptr, config_space, host, bus);

    if(root->bus == nullptr) {
        pci_root_detach(host);
        return -ENOMEM;
    }

    return 0;
}

void pci_root_detach(device_t* restrict host)
{
    pci_root_t* root = host->ctxdata;

    kassert(root->bus == nullptr || root->bus->functions == nullptr);

    kfree(root->bus);
    kfree(root);

    host->ctxdata = nullptr;
    host->bus_ops = nullptr;
}

pci_function_t* pci_function_of(device_t* restrict device)
{
    device_t* bus = device->parent;

    if(bus == nullptr || (bus->bus_ops != &host_bus_ops && bus->bus_ops != &bridge_bus_ops)) {
        return nullptr;
    }

    return device->bus_data;
}

uint8_t pci_read_config8(pci_function_t* restrict function, uintptr_t offset)
{
    return pci_config_space_read8(function->priv.config_space, &function->address, offset);
}

uint16_t pci_read_config16(pci_function_t* restrict function, uintptr_t offset)
{
    return pci_config_space_read16(function->priv.config_space, &function->address, offset);
}

uint32_t pci_read_config32(pci_function_t* restrict function, uintptr_t offset)
{
    return pci_config_space_read32(function->priv.config_space, &function->address, offset);
}

void pci_write_config8(pci_function_t* restrict function, uintptr_t offset, uint8_t value)
{
    pci_config_space_write8(function->priv.config_space, &function->address, offset, value);
}

void pci_write_config16(pci_function_t* restrict function, uintptr_t offset, uint16_t value)
{
    pci_config_space_write16(function->priv.config_space, &function->address, offset, value);
}

void pci_write_config32(pci_function_t* restrict function, uintptr_t offset, uint32_t value)
{
    pci_config_space_write32(function->priv.config_space, &function->address, offset, value);
}

uint8_t pci_get_cap(pci_function_t* restrict function, uint8_t cap_id)
{
    uint16_t status = pci_read_config16(function, PCI_CFG_STATUS);

    if(0 == (status & PCI_STS_CAP_LIST)) {
        return 0;
    }

    uint8_t offset = pci_read_config8(function, PCI_CFG_CAP_PTR) & 0xFC;

    while(offset) {
        if(pci_read_config8(function, offset) == cap_id)
            return offset;
        offset = pci_read_config8(function, offset + 1) & 0xFC;
    }

    return 0;
}

void pci_enable_busmaster(pci_function_t* restrict function)
{
    uint16_t cmd = pci_read_config16(function, PCI_CFG_COMMAND);

    pci_write_config16(function, PCI_CFG_COMMAND, cmd | PCI_CMD_BUS_MASTER);
}

void pci_disable_busmaster(pci_function_t* restrict function)
{
    uint16_t cmd = pci_read_config16(function, PCI_CFG_COMMAND);

    pci_write_config16(function, PCI_CFG_COMMAND, cmd & ~(uint16_t)(PCI_CMD_BUS_MASTER));
}

int pci_request_msi(pci_function_t* restrict function, irq_handler_t handler, void* restrict ctxdata)
{
    if(function->priv.irq_number != UINT_MAX) {
        return -EBUSY;
    }

    uint8_t cap = pci_get_cap(function, PCI_CAP_MSI);

    if(cap == 0) {
        return -ENODEV;
    }

    return request_msi(function, cap, handler, ctxdata);
}
