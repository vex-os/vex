#include <vex/atomic.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>
#include <vex/pci.h>
#include <vex/string.h>

#include "uart16550.h"

#define PROG_IF_16550 0x02
#define PORT_STRIDE   8 // Between the ports of a card that puts them all in its first BAR
#define PORTS_MAX     4

typedef struct card_s card_t;

struct card_s {
    pci_bar_t* bar;
    uart16550_t* ports[PORTS_MAX];
    size_t num_ports; // Of those added, which the interrupt handler reads as it grows
};

static const pci_driver_id_t uart16550_pci_ids[] = {
    PCI_DEVICE_SPECIFIC(0x1B36, 0x0002), // QEMU pci-serial
    PCI_DEVICE_SPECIFIC(0x1B36, 0x0003), // QEMU pci-serial-2x
    PCI_DEVICE_SPECIFIC(0x1B36, 0x0004), // QEMU pci-serial-4x
    PCI_DEVICE_CLASS(0x07, 0x00),
    PCI_DEVICE_END(),
};

static size_t ports_on(pci_device_t* restrict device, const pci_driver_id_t* restrict id)
{
    if(id->vendor_id == 0x1B36 && id->device_id == 0x0003) {
        return 2;
    }

    if(id->vendor_id == 0x1B36 && id->device_id == 0x0004) {
        return 4;
    }

    // A serial controller of another interface lays its registers out its own way
    if(id->vendor_id == 0 && pci_cfg_read8(device, PCI_CFG_PROG_IF) != PROG_IF_16550) {
        return 0;
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

static int create_ports(const pci_bar_t* restrict bar, uart16550_t** restrict ports, size_t count)
{
    for(size_t i = 0; i < count; ++i) {
        io_region_t io = bar->io;
        io.base += i * PORT_STRIDE;
        io.length = PORT_STRIDE;

        ports[i] = uart16550_create(&io, 0, UART16550_CLOCK, nullptr);

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

static int uart16550_pci_probe(pci_device_t* restrict device, const pci_driver_id_t* restrict id)
{
    size_t count = ports_on(device, id);

    if(count == 0) {
        return -ENODEV;
    }

    card_t* card = kmalloc(sizeof(card_t));

    if(card == nullptr) {
        return -ENOMEM;
    }

    memset(card, 0, sizeof(card_t));
    card->bar = pci_bar_map(device, 0);

    if(card->bar == nullptr) {
        kfree(card);
        return -ENODEV;
    }

    uart16550_t* ports[PORTS_MAX];
    int retval = create_ports(card->bar, ports, count);

    if(retval < 0) {
        pci_bar_unmap(device, 0);
        kfree(card);
        return retval;
    }

    retval = pci_request_irq(device, &uart16550_pci_irq, card);

    if(retval < 0) {
        free_ports(ports, count);
        pci_bar_unmap(device, 0);
        kfree(card);
        return retval;
    }

    add_ports(card, ports, count);

    if(card->num_ports == 0) {
        pci_release_irq(device);
        pci_bar_unmap(device, 0);
        kfree(card);
        return -ENODEV;
    }

    device->ctxdata = card;

    return 0;
}

static void uart16550_pci_remove(pci_device_t* restrict device)
{
    card_t* card = device->ctxdata;

    for(size_t i = 0; i < card->num_ports; ++i) {
        uart16550_remove(card->ports[i]);
    }

    pci_release_irq(device);
    pci_bar_unmap(device, 0);
    free_ports(card->ports, card->num_ports);
    kfree(card);

    device->ctxdata = nullptr;
}

static const pci_driver_ops_t uart16550_pci_ops = {
    .probe = &uart16550_pci_probe,
    .remove = &uart16550_pci_remove,
};

static pci_driver_t uart16550_pci_driver = {
    .ops = &uart16550_pci_ops,
    .ids = uart16550_pci_ids,
};

static void init_uart16550_pci(void)
{
    pci_driver_add(&uart16550_pci_driver);
}

initcall_bsp_dev(init_uart16550_pci, INIT_LATE);
