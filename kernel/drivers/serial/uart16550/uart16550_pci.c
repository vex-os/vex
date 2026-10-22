#include <vex/atomic.h>
#include <vex/device.h>
#include <vex/driver.h>
#include <vex/errno.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/pci.h>
#include <vex/string.h>

#include "uart16550_internal.h"

#define PROG_IF_16550 0x02
#define PORT_STRIDE   8 // Between the ports of a card that puts them all in its first BAR
#define PORTS_MAX     4

typedef struct card_s card_t;

struct card_s {
    uart16550_t* ports[PORTS_MAX];
    size_t num_ports; // Of those added, which the interrupt handler reads as it grows
};

static const driver_match_t uart16550_pci_matches[] = {
    DRIVER_MATCH_PCI_ID(0x1B36, 0x0002), // QEMU pci-serial
    DRIVER_MATCH_PCI_ID(0x1B36, 0x0003), // QEMU pci-serial-2x
    DRIVER_MATCH_PCI_ID(0x1B36, 0x0004), // QEMU pci-serial-4x
    DRIVER_MATCH_PCI_CLASS(0x07, 0x00),
    DRIVER_MATCH_END(),
};

static size_t ports_on(pci_function_t* restrict function, const driver_match_t* restrict match)
{
    if(match->kind == DRIVER_MATCH_PCI_CLASS) {
        if(PROG_IF_16550 == pci_read_config8(function, PCI_CFG_PROG_IF)) {
            return 1;
        }
        else {
            return 0; // A serial controller of another interface lays its registers out its own way
        }
    }

    if(match->device_id == 0x0003) {
        return 2;
    }

    if(match->device_id == 0x0004) {
        return 4;
    }

    return 1;
}

// The ports share the card's interrupt, and each returns at once when it has nothing pending
static void uart16550_pci_irq(unsigned number, const hart_context_t* restrict ctx, void* restrict ctxdata)
{
    card_t* card = ctxdata;
    size_t num_ports = atomic_load(&card->num_ports, ATOMIC_ACQUIRE);

    for(size_t i = 0; i < num_ports; ++i) {
        uart16550_interrupt(card->ports[i]);
    }
}

static void free_ports(uart16550_t* const* restrict ports, size_t count)
{
    for(size_t i = 0; i < count; ++i) {
        uart16550_free(ports[i]);
    }
}

static int create_ports(device_t* restrict device, const io_region_t* restrict bar, uart16550_t** restrict ports, size_t count)
{
    for(size_t i = 0; i < count; ++i) {
        io_region_t io = *bar;
        io.base += i * PORT_STRIDE;
        io.length = PORT_STRIDE;

        ports[i] = uart16550_create(device, &io, 0, UART16550_CLOCK, nullptr);

        if(ports[i] == nullptr) {
            free_ports(ports, i);
            return -ENOMEM;
        }
    }

    return 0;
}

// A port that doesn't answer is left out, and the others still work
static void add_ports(card_t* restrict card, uart16550_t* const* restrict ports, size_t count)
{
    for(size_t i = 0; i < count; ++i) {
        if(uart16550_add(ports[i]) < 0) {
            uart16550_free(ports[i]);
            continue;
        }

        card->ports[card->num_ports] = ports[i];
        atomic_store(&card->num_ports, card->num_ports + 1, ATOMIC_RELEASE);
    }
}

static int uart16550_pci_probe(device_t* restrict device, const driver_match_t* restrict match)
{
    pci_function_t* function = pci_function_of(device);

    if(ports_on(function, match) == 0) {
        return -ENODEV;
    }

    if(match->kind == DRIVER_MATCH_PCI_ID) {
        return DRIVER_SCORE_SPECIFIC;
    }

    return DRIVER_SCORE_DEFAULT;
}

static int uart16550_pci_attach(device_t* restrict device, const driver_match_t* restrict match)
{
    pci_function_t* function = pci_function_of(device);
    size_t count = ports_on(function, match);
    card_t* card = kmalloc(sizeof(card_t));

    if(card == nullptr) {
        return -ENOMEM;
    }

    memset(card, 0, sizeof(card_t));
    device->ctxdata = card;

    io_region_t bar;
    int retval = device_map_io(device, 0, &bar);

    if(retval < 0) {
        return retval;
    }

    uart16550_t* ports[PORTS_MAX];
    retval = create_ports(device, &bar, ports, count);

    if(retval < 0) {
        return retval;
    }

    retval = device_request_irq(device, 0, &uart16550_pci_irq, card);

    if(retval < 0) {
        free_ports(ports, count);
        return retval;
    }

    add_ports(card, ports, count);

    if(card->num_ports == 0) {
        return -ENODEV;
    }

    return 0;
}

static void uart16550_pci_detach(device_t* restrict device)
{
    card_t* card = device->ctxdata;

    for(size_t i = 0; i < card->num_ports; ++i) {
        uart16550_remove(card->ports[i]);
    }
}

static void uart16550_pci_release(device_t* restrict device)
{
    card_t* card = device->ctxdata;

    if(card) {
        free_ports(card->ports, card->num_ports);
        kfree(card);
    }
}

static const driver_ops_t uart16550_pci_ops = {
    .probe = &uart16550_pci_probe,
    .attach = &uart16550_pci_attach,
    .detach = &uart16550_pci_detach,
    .release = &uart16550_pci_release,
};

define_driver(uart16550_pci, &uart16550_pci_ops, uart16550_pci_matches);
