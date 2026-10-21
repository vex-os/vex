#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/isa.h>

#include "uart16550.h"

static const char* const uart16550_isa_ids[] = {
    "PNP0500",
    "PNP0501",
    nullptr,
};

static void uart16550_isa_irq(unsigned number, const hart_context_t* restrict ctx, void* restrict ctxdata)
{
    uart16550_interrupt(ctxdata);
}

static int uart16550_isa_probe(isa_device_t* restrict device)
{
    io_region_t io;
    int retval = isa_map_io(device, 0, &io);

    if(retval < 0) {
        return retval;
    }

    uart16550_t* port = uart16550_create(&io, 0, UART16550_CLOCK, device->name);

    if(port == nullptr) {
        isa_unmap_io(&io);
        return -ENOMEM;
    }

    retval = isa_request_irq(device, 0, &uart16550_isa_irq, port);

    if(retval < 0) {
        uart16550_free(port);
        isa_unmap_io(&io);
        return retval;
    }

    retval = uart16550_add(port);

    if(retval < 0) {
        isa_release_irq(device, 0);
        uart16550_free(port);
        isa_unmap_io(&io);
        return retval;
    }

    device->ctxdata = port;

    return 0;
}

static void uart16550_isa_remove(isa_device_t* restrict device)
{
    uart16550_t* port = device->ctxdata;
    io_region_t io = port->io;

    uart16550_remove(port);
    isa_release_irq(device, 0);
    isa_unmap_io(&io);
    uart16550_free(port);

    device->ctxdata = nullptr;
}

static const isa_driver_ops_t uart16550_isa_ops = {
    .probe = &uart16550_isa_probe,
    .remove = &uart16550_isa_remove,
};

static isa_driver_t uart16550_isa_driver = {
    .ops = &uart16550_isa_ops,
    .ids = uart16550_isa_ids,
};

static void init_uart16550_isa(void)
{
    isa_driver_add(&uart16550_isa_driver);
}

initcall_bsp_dev(init_uart16550_isa, INIT_LATE);
