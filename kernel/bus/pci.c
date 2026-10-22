#include <arch/irq.h>
#include <arch/msi.h>
#include <vex/assert.h>
#include <vex/device.h>
#include <vex/errno.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/limits.h>
#include <vex/mmio.h>
#include <vex/mutex.h>
#include <vex/page.h>
#include <vex/pci.h>
#include <vex/spinlock.h>
#include <vex/sprintf.h>
#include <vex/vprot.h>

#define PCI_ROOT_WINDOW_MAX 16

#define PCI_ANY_FUNCTION 0xFFFF // In a firmware node's address

typedef struct pci_root_s pci_root_t;
typedef struct companion_search_s companion_search_t;

struct pci_root_s {
    unsigned segment;
    fwnode_resource_t windows[PCI_ROOT_WINDOW_MAX]; // Ports and memory the host bridge passes on
    size_t num_windows;
};

struct pci_bus_s {
    pci_bus_t* next; // Of all buses, in the order they were found
    pci_root_t* root;
    pci_device_t* bridge; // nullptr on a root bus
    device_t* device;     // The host bridge, or the bridge
    pci_config_t* config;
    unsigned number;
    pci_device_t* devices; // In the order of their addresses
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

static spinlock_t config_lock = SPINLOCK_INIT;
static pci_config_t* config_head = nullptr;
static mutex_t pci_mutex = MUTEX_INIT;
static pci_bus_t* bus_head = nullptr;
static pci_bus_t** bus_tail = &bus_head;
static pci_driver_t* driver_head = nullptr;

static void scan_bus(pci_bus_t* restrict bus);

static bool configs_overlap(const pci_config_t* restrict first, const pci_config_t* restrict second)
{
    return first->segment == second->segment && first->bus_start <= second->bus_end && second->bus_start <= first->bus_end;
}

static void device_bind(pci_device_t* restrict device, pci_driver_t* restrict driver)
{
    if(device->priv.driver) {
        return;
    }

    for(const pci_driver_id_t* id = driver->ids; id->vendor_id < 0xFFFF || id->device_id < 0xFFFF; id += 1) {
        bool match;

        if(id->vendor_id == 0) {
            match = id->class_id == device->class_id && id->subclass_id == device->subclass_id;
        }
        else {
            match = id->vendor_id == device->vendor_id && id->device_id == device->device_id;
        }

        if(match) {
            if(0 == driver->ops->probe(device, id))
                device->priv.driver = driver;
            return;
        }
    }
}

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

static void log_device(pci_device_t* restrict device)
{
    pr_verbose("pci: found %04X:%04X at %s", device->vendor_id, device->device_id, device->priv.device->name);
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

static uintptr_t bar_address(pci_device_t* restrict device, unsigned bar_index)
{
    uintptr_t offset = PCI_CFG_BAR0 + bar_index * 4;
    uint32_t raw = pci_cfg_read32(device, offset);

    if(raw & PCI_BAR_IO) {
        return raw & PCI_BAR_ADDR_MASK_IO;
    }

    if((raw & PCI_BAR_MEM_TYPE_MASK) == PCI_BAR_MEM_64BIT) {
        uint64_t raw_higher = pci_cfg_read32(device, offset + 4);

        return (raw & PCI_BAR_ADDR_MASK_MEM) | (raw_higher << 32);
    }

    return raw & PCI_BAR_ADDR_MASK_MEM;
}

static uint32_t probe_bar_mask(pci_device_t* restrict device, uintptr_t offset)
{
    uint32_t raw = pci_cfg_read32(device, offset);
    pci_cfg_write32(device, offset, UINT32_C(0xFFFFFFFF));
    uint32_t mask = pci_cfg_read32(device, offset);
    pci_cfg_write32(device, offset, raw);
    return mask;
}

static uint64_t bar_length(pci_device_t* restrict device, unsigned bar_index)
{
    uintptr_t offset = PCI_CFG_BAR0 + bar_index * 4;
    uint32_t raw = pci_cfg_read32(device, offset);

    // A half-written BAR would decode at
    // a bogus address while it is being probed
    // so we disable the BAR while probing its length
    uint16_t command = pci_cfg_read16(device, PCI_CFG_COMMAND);

    pci_cfg_write16(device, PCI_CFG_COMMAND, command & ~(PCI_COMMAND_IO | PCI_COMMAND_MEM));

    uint64_t mask = probe_bar_mask(device, offset);

    if(0 == (raw & PCI_BAR_IO) && (raw & PCI_BAR_MEM_TYPE_MASK) == PCI_BAR_MEM_64BIT && bar_index < 5) {
        mask |= (uint64_t)(probe_bar_mask(device, offset + 4)) << 32;
    }
    else {
        mask |= UINT64_C(0xFFFFFFFF00000000);
    }

    pci_cfg_write16(device, PCI_CFG_COMMAND, command);

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

static int map_range(device_t* restrict bus, unsigned type, uint64_t start, uint64_t length, unsigned vprot, io_region_t* restrict region,
    uintptr_t* restrict phys)
{
    uint64_t cpu_address = start;
    int retval = device_translate_range(bus, &type, &cpu_address, length);

    if(retval < 0) {
        return retval;
    }

    if(type == FWNODE_RESOURCE_PORT) {
        region->space = IO_SPACE_PORT;
        region->base = cpu_address;
        region->length = length;
        *phys = 0;
        return 0;
    }

    void* virt = mmio_map(cpu_address, length, vprot);

    if(virt == nullptr) {
        return -ENOMEM;
    }

    region->space = IO_SPACE_MEMORY;
    region->base = (uintptr_t)(virt);
    region->length = length;
    *phys = cpu_address;

    return 0;
}

static void warn_outside_windows(const pci_device_t* restrict device, unsigned bar_index, uint64_t address)
{
    const pci_address_t* location = &device->address;
    unsigned long long value = address;

    pr_warning("pci: %04X:%02X:%02X.%u BAR%u at 0x%llX is outside the host bridge's windows", location->segment, location->bus,
        location->device, location->function, bar_index, value);
}

// Two mappings of the same memory with
// different types are undefined on x86 (PAT)
// so a BAR has one mapping and every caller gets it
static pci_bar_t* map_bar(pci_device_t* restrict device, unsigned bar_index, unsigned vprot, bool memory_only)
{
    kassert(bar_index < 6);

    pci_bar_t* bar = &device->priv.bars[bar_index];
    uint32_t raw = pci_cfg_read32(device, PCI_CFG_BAR0 + bar_index * 4);

    if(memory_only && (raw & PCI_BAR_IO)) {
        return nullptr;
    }

    if(device->priv.bars_mapped & (1U << bar_index)) {
        kassert(bar->io.space == IO_SPACE_PORT || bar->vprot == vprot);

        return bar;
    }

    uint64_t length = bar_length(device, bar_index);

    if(length == 0) {
        return nullptr;
    }

    unsigned type = (raw & PCI_BAR_IO) ? FWNODE_RESOURCE_PORT : FWNODE_RESOURCE_MEMORY;
    uint64_t address = bar_address(device, bar_index);
    int retval = map_range(device->priv.bus->device, type, address, length, vprot, &bar->io, &bar->phys);

    if(retval == -ERANGE) {
        warn_outside_windows(device, bar_index, address);
    }

    if(retval < 0) {
        return nullptr;
    }

    bar->vprot = bar->io.space == IO_SPACE_MEMORY ? vprot : VPROT_NONE;

    device->priv.bars_mapped |= 1U << bar_index;

    return bar;
}

// PCI-to-PCI Bridge Architecture 1.2, 9.1: a bridge the firmware gives no routes for
// passes a pin on as a pin of its own slot, rotated by the number of the device behind it
static int route_intx(const pci_device_t* restrict device, unsigned pin, fwnode_irq_t* restrict irq)
{
    const pci_device_t* current = device;

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
    pci_device_t* function = child->bus_ctxdata;

    if(index >= 6) {
        return -ENODEV;
    }

    uint64_t length = bar_length(function, index);

    if(length == 0) {
        return -ENODEV;
    }

    uint32_t raw = pci_cfg_read32(function, PCI_CFG_BAR0 + index * 4);

    *range = (fwnode_resource_t) {};
    range->type = (raw & PCI_BAR_IO) ? FWNODE_RESOURCE_PORT : FWNODE_RESOURCE_MEMORY;
    range->range.start = bar_address(function, index);
    range->range.length = length;

    return 0;
}

static int pci_get_irq(device_t* restrict bus, device_t* restrict child, unsigned index, fwnode_irq_t* restrict irq)
{
    pci_device_t* function = child->bus_ctxdata;
    unsigned pin = pci_cfg_read8(function, PCI_CFG_INT_PIN);

    if(index != 0 || pin == 0 || pin > sizeof(pin_names) / sizeof(pin_names[0])) {
        return -ENODEV;
    }

    if(route_intx(function, pin - 1, irq) < 0) {
        return -ENODEV;
    }

    irq->flags |= FWNODE_IRQ_SHARED; // PCI Local Bus 3.0, 2.2.6: INTx lines are wired-OR between functions

    pr_verbose("pci: %s %s is GSI %u", child->name, pin_names[pin - 1], irq->number);

    return 0;
}

static const device_bus_ops_t host_bus_ops = {
    .get_range = &pci_get_range,
    .get_irq = &pci_get_irq,
    .translate_range = &host_translate_range,
};

// A bridge passes ranges on unchanged, within the windows it is set up with
static const device_bus_ops_t bridge_bus_ops = {
    .get_range = &pci_get_range,
    .get_irq = &pci_get_irq,
};

static int add_bus(pci_root_t* restrict root, pci_device_t* restrict bridge, pci_config_t* restrict config, device_t* restrict device,
    unsigned number)
{
    pci_bus_t* bus = kmalloc(sizeof(pci_bus_t));

    if(bus == nullptr) {
        return -ENOMEM;
    }

    bus->root = root;
    bus->bridge = bridge;
    bus->device = device;
    bus->config = config;
    bus->number = number;
    bus->devices = nullptr;
    bus->next = nullptr;

    *bus_tail = bus;
    bus_tail = &bus->next;

    scan_bus(bus);

    return 0;
}

// UNDONE: a bridge the firmware left without bus numbers needs them assigned
static void add_bridge(pci_device_t* restrict bridge)
{
    const pci_bus_t* parent = bridge->priv.bus;
    const pci_address_t* address = &bridge->address;
    unsigned secondary = pci_cfg_read8(bridge, PCI_CFG_SECONDARY_BUS);
    unsigned subordinate = pci_cfg_read8(bridge, PCI_CFG_SUBORDINATE_BUS);

    if(secondary <= parent->number || subordinate < secondary) {
        pr_warning("pci: bridge at %04X:%02X:%02X.%u has no bus numbers", address->segment, address->bus, address->device,
            address->function);
        return;
    }

    pci_config_t* config = pci_config_find(address->segment, secondary);

    if(config == nullptr) {
        pr_warning("pci: no configuration access to bus %04X:%02X", address->segment, secondary);
        return;
    }

    bridge->priv.device->bus_ops = &bridge_bus_ops;

    if(add_bus(parent->root, bridge, config, bridge->priv.device, secondary) < 0) {
        pr_warning("pci: out of memory");
    }
}

static void add_function(pci_bus_t* restrict bus, const pci_address_t* restrict address)
{
    char name[DEVICE_NAME_MAX];
    snprintf(name, sizeof(name), "pci:%04x:%02x:%02x.%x", address->segment, address->bus, address->device, address->function);

    fwnode_t companion;
    bool has_companion = find_companion(bus, address, &companion);

    pci_device_t* function = kmalloc(sizeof(pci_device_t));
    device_t* device = nullptr;

    if(function == nullptr || device_add(bus->device, has_companion ? &companion : nullptr, name, &device) < 0) {
        pr_warning("pci: out of memory");
        kfree(function);
        return;
    }

    device->bus_ctxdata = function;

    function->priv.bus = bus;
    function->priv.config = bus->config;
    function->priv.driver = nullptr;
    function->priv.device = device;
    function->priv.irq_number = UINT_MAX;
    function->priv.bars_mapped = 0;
    function->address = *address;
    function->vendor_id = pci_cfg_read16(function, PCI_CFG_VENDOR_ID);
    function->device_id = pci_cfg_read16(function, PCI_CFG_DEVICE_ID);
    function->class_id = pci_cfg_read8(function, PCI_CFG_CLASS);
    function->subclass_id = pci_cfg_read8(function, PCI_CFG_SUBCLASS);
    function->ctxdata = nullptr;

    log_device(function);

    pci_device_t** tail = &bus->devices;

    while(*tail) {
        tail = &(*tail)->priv.next;
    }

    function->priv.next = nullptr;
    *tail = function;

    if((pci_cfg_read8(function, PCI_CFG_HEADER_TYPE) & PCI_HDR_TYPE_MASK) == PCI_HDR_TYPE_BRIDGE) {
        add_bridge(function);
    }

    for(pci_driver_t* it = driver_head; it; it = it->priv.next) {
        device_bind(function, it);
    }
}

static void scan_slot(pci_bus_t* restrict bus, unsigned slot)
{
    pci_address_t address = {};
    address.segment = bus->root->segment;
    address.bus = bus->number;
    address.device = slot;

    if(pci_config_read16(bus->config, &address, PCI_CFG_VENDOR_ID) == 0xFFFF) {
        return;
    }

    unsigned header = pci_config_read8(bus->config, &address, PCI_CFG_HEADER_TYPE);
    unsigned num_functions = (header & PCI_HDR_MULTIFUNC) ? PCI_FUNCTION_MAX : 1;

    for(unsigned function = 0; function < num_functions; function += 1) {
        address.function = function;

        if(pci_config_read16(bus->config, &address, PCI_CFG_VENDOR_ID) != 0xFFFF) {
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

static int request_msi(pci_device_t* restrict device, uint8_t cap, irq_handler_t handler, void* restrict ctxdata)
{
    unsigned irq_number = irq_handler_add(UINT_MAX, handler, ctxdata);

    if(irq_number == UINT_MAX) {
        return -ENOMEM;
    }

    device->priv.irq_number = irq_number;

    uint64_t address;
    uint32_t data;
    arch_msi_compose(irq_number + ARCH_IRQ_BASE, &address, &data);

    uint16_t control = pci_cfg_read16(device, cap + PCI_MSI_CONTROL);
    control &= ~(uint16_t)(PCI_MSI_CONTROL_ENABLE | PCI_MSI_CONTROL_MME);

    pci_cfg_write16(device, cap + PCI_MSI_CONTROL, control);
    pci_cfg_write32(device, cap + PCI_MSI_ADDRESS_LO, (uint32_t)(address));

    uintptr_t mask_offset = cap + PCI_MSI_MASK_32;

    if(control & PCI_MSI_CONTROL_64BIT) {
        pci_cfg_write32(device, cap + PCI_MSI_ADDRESS_HI, (uint32_t)(address >> 32));
        pci_cfg_write16(device, cap + PCI_MSI_DATA_64, (uint16_t)(data));
        mask_offset = cap + PCI_MSI_MASK_64;
    }
    else {
        pci_cfg_write16(device, cap + PCI_MSI_DATA_32, (uint16_t)(data));
    }

    if(control & PCI_MSI_CONTROL_PVM) {
        pci_cfg_write32(device, mask_offset, pci_cfg_read32(device, mask_offset) & ~UINT32_C(1));
    }

    uint16_t cmd = pci_cfg_read16(device, PCI_CFG_COMMAND);
    pci_cfg_write16(device, PCI_CFG_COMMAND, cmd | PCI_CMD_INT_DISABLE);

    pci_cfg_write16(device, cap + PCI_MSI_CONTROL, control | PCI_MSI_CONTROL_ENABLE);

    return 0;
}

static int request_intx(pci_device_t* restrict device, irq_handler_t handler, void* restrict ctxdata)
{
    int retval = device_request_irq(device->priv.device, 0, handler, ctxdata);

    if(retval < 0) {
        return retval;
    }

    uint16_t cmd = pci_cfg_read16(device, PCI_CFG_COMMAND);
    pci_cfg_write16(device, PCI_CFG_COMMAND, cmd & ~(uint16_t)(PCI_CMD_INT_DISABLE));

    return 0;
}

int pci_config_add(pci_config_t* restrict config)
{
    spinlock_acquire(&config_lock);

    for(pci_config_t* it = config_head; it; it = it->priv.next) {
        if(it == config || configs_overlap(it, config)) {
            spinlock_release(&config_lock);
            return -EEXIST;
        }
    }

    config->priv.next = config_head;
    config_head = config;

    spinlock_release(&config_lock);

    return 0;
}

pci_config_t* pci_config_find(unsigned segment, unsigned bus)
{
    spinlock_acquire(&config_lock);

    for(pci_config_t* it = config_head; it; it = it->priv.next) {
        if(it->segment == segment && bus >= it->bus_start && bus <= it->bus_end) {
            spinlock_release(&config_lock);
            return it;
        }
    }

    spinlock_release(&config_lock);

    return nullptr;
}

int pci_root_add(const fwnode_t* restrict node, unsigned segment, unsigned bus)
{
    pci_config_t* config = pci_config_find(segment, bus);

    if(config == nullptr) {
        return -ENODEV;
    }

    char name[DEVICE_NAME_MAX];

    if(fwnode_get_name(node, name, sizeof(name)) < 0) {
        snprintf(name, sizeof(name), "pci:%04x:%02x", segment, bus);
    }

    int retval = fwnode_claim(node);

    if(retval < 0) {
        return retval;
    }

    pci_root_t* root = kmalloc(sizeof(pci_root_t));
    device_t* tree_root = device_get_root();
    device_t* host = nullptr;

    if(root == nullptr || tree_root == nullptr || device_add(tree_root, node, name, &host) < 0) {
        if(tree_root) {
            device_decref(tree_root);
        }

        kfree(root);
        return -ENOMEM;
    }

    device_decref(tree_root);

    root->segment = segment;
    root->num_windows = 0;

    fwnode_for_each_resource(node, &collect_window, root);

    host->ctxdata = root;
    host->bus_ops = &host_bus_ops;

    mutex_acquire(&pci_mutex);
    retval = add_bus(root, nullptr, config, host, bus);
    mutex_release(&pci_mutex);

    return retval;
}

int pci_driver_add(pci_driver_t* restrict driver)
{
    mutex_acquire(&pci_mutex);

    for(pci_driver_t* it = driver_head; it; it = it->priv.next) {
        if(it == driver) {
            mutex_release(&pci_mutex);
            return -EEXIST;
        }
    }

    driver->priv.next = driver_head;
    driver_head = driver;

    for(pci_bus_t* bus = bus_head; bus; bus = bus->next) {
        for(pci_device_t* it = bus->devices; it; it = it->priv.next) {
            device_bind(it, driver);
        }
    }

    mutex_release(&pci_mutex);

    return 0;
}

int pci_driver_remove(pci_driver_t* restrict driver)
{
    mutex_acquire(&pci_mutex);

    pci_driver_t** prev = &driver_head;

    while(*prev && *prev != driver) {
        prev = &(*prev)->priv.next;
    }

    if(*prev == nullptr) {
        mutex_release(&pci_mutex);
        return -ENODEV;
    }

    *prev = driver->priv.next;

    for(pci_bus_t* bus = bus_head; bus; bus = bus->next) {
        for(pci_device_t* it = bus->devices; it; it = it->priv.next) {
            if(it->priv.driver == driver) {
                if(driver->ops->remove) {
                    driver->ops->remove(it);
                }

                it->priv.driver = nullptr;
            }
        }
    }

    mutex_release(&pci_mutex);

    return 0;
}

int pci_get_companion(const pci_device_t* restrict device, fwnode_t* restrict node)
{
    const fwnode_t* companion = &device->priv.device->fwnode;

    if(companion->ops == nullptr) {
        return -ENOENT;
    }

    *node = *companion;

    return 0;
}

uint8_t pci_cfg_read8(pci_device_t* restrict device, uintptr_t offset)
{
    return pci_config_read8(device->priv.config, &device->address, offset);
}

uint16_t pci_cfg_read16(pci_device_t* restrict device, uintptr_t offset)
{
    return pci_config_read16(device->priv.config, &device->address, offset);
}

uint32_t pci_cfg_read32(pci_device_t* restrict device, uintptr_t offset)
{
    return pci_config_read32(device->priv.config, &device->address, offset);
}

void pci_cfg_write8(pci_device_t* restrict device, uintptr_t offset, uint8_t value)
{
    pci_config_write8(device->priv.config, &device->address, offset, value);
}

void pci_cfg_write16(pci_device_t* restrict device, uintptr_t offset, uint16_t value)
{
    pci_config_write16(device->priv.config, &device->address, offset, value);
}

void pci_cfg_write32(pci_device_t* restrict device, uintptr_t offset, uint32_t value)
{
    pci_config_write32(device->priv.config, &device->address, offset, value);
}

pci_bar_t* pci_bar_map(pci_device_t* restrict device, unsigned bar_index)
{
    return map_bar(device, bar_index, VPROT_MMIO, false);
}

pci_bar_t* pci_bar_map_vprot(pci_device_t* restrict device, unsigned bar_index, unsigned vprot)
{
    return map_bar(device, bar_index, vprot, true);
}

int pci_map_range(pci_device_t* restrict device, unsigned type, uint64_t start, uint64_t length, io_region_t* restrict region)
{
    return device_map_range(device->priv.bus->device, type, start, length, region);
}

void pci_unmap_range(io_region_t* restrict region)
{
    device_unmap_io(region);
}

void pci_bar_unmap(pci_device_t* restrict device, unsigned bar_index)
{
    kassert(bar_index < 6);

    if(device->priv.bars_mapped & (1U << bar_index)) {
        pci_bar_t* bar = &device->priv.bars[bar_index];

        if(bar->io.space == IO_SPACE_MEMORY) {
            mmio_unmap((volatile void*)(bar->io.base));
        }

        device->priv.bars_mapped &= ~(1U << bar_index);
    }
}

uint8_t pci_get_cap(pci_device_t* restrict device, uint8_t cap_id)
{
    uint16_t status = pci_cfg_read16(device, PCI_CFG_STATUS);

    if(0 == (status & PCI_STS_CAP_LIST)) {
        return 0;
    }

    uint8_t offset = pci_cfg_read8(device, PCI_CFG_CAP_PTR) & 0xFC;

    while(offset) {
        if(pci_cfg_read8(device, offset) == cap_id)
            return offset;
        offset = pci_cfg_read8(device, offset + 1) & 0xFC;
    }

    return 0;
}

void pci_enable_busmaster(pci_device_t* restrict device)
{
    uint16_t cmd = pci_cfg_read16(device, PCI_CFG_COMMAND);

    pci_cfg_write16(device, PCI_CFG_COMMAND, cmd | PCI_CMD_BUS_MASTER);
}

void pci_disable_busmaster(pci_device_t* restrict device)
{
    uint16_t cmd = pci_cfg_read16(device, PCI_CFG_COMMAND);

    pci_cfg_write16(device, PCI_CFG_COMMAND, cmd & ~(uint16_t)(PCI_CMD_BUS_MASTER));
}

int pci_request_irq(pci_device_t* restrict device, irq_handler_t handler, void* restrict ctxdata)
{
    uint8_t cap = pci_get_cap(device, PCI_CAP_MSI);

    if(cap) {
        return request_msi(device, cap, handler, ctxdata);
    }

    return request_intx(device, handler, ctxdata);
}

void pci_release_irq(pci_device_t* restrict device)
{
    uint16_t cmd = pci_cfg_read16(device, PCI_CFG_COMMAND);
    pci_cfg_write16(device, PCI_CFG_COMMAND, cmd | PCI_CMD_INT_DISABLE);

    device_release_irq(device->priv.device, 0);

    if(device->priv.irq_number == UINT_MAX) {
        return;
    }

    uint8_t cap = pci_get_cap(device, PCI_CAP_MSI);

    if(cap) {
        uint16_t control = pci_cfg_read16(device, cap + PCI_MSI_CONTROL);
        pci_cfg_write16(device, cap + PCI_MSI_CONTROL, control & ~(uint16_t)(PCI_MSI_CONTROL_ENABLE));
    }

    irq_handler_remove(device->priv.irq_number);

    device->priv.irq_number = UINT_MAX;
}
