#include <vex/device.h>
#include <vex/errno.h>
#include <vex/fwnode.h>

#include "uart16550.h"

static const driver_match_t uart16550_fw_matches[] = {
    DRIVER_MATCH_FW("PNP0500"),
    DRIVER_MATCH_FW("PNP0501"),
    DRIVER_MATCH_END(),
};

static void uart16550_fw_irq(unsigned number, const hart_context_t* restrict ctx, void* restrict ctxdata)
{
    uart16550_interrupt(ctxdata);
}

static int uart16550_fw_probe(device_t* restrict device, const driver_match_t* restrict match, unsigned* restrict bid)
{
    *bid = DRIVER_BID_DEFAULT;

    return 0;
}

static int uart16550_fw_attach(device_t* restrict device, const driver_match_t* restrict match)
{
    io_region_t io;
    int retval = device_map_io(device, 0, &io);

    if(retval < 0) {
        return retval;
    }

    char id[UART16550_ID_MAX];

    if(fwnode_get_id(&device->fwnode, id, sizeof(id)) < 0) {
        id[0] = 0;
    }

    uart16550_t* port = uart16550_create(&io, 0, UART16550_CLOCK, id[0] ? id : nullptr);

    if(port == nullptr) {
        return -ENOMEM;
    }

    device->ctxdata = port;

    retval = device_request_irq(device, 0, &uart16550_fw_irq, port);

    if(retval < 0) {
        return retval;
    }

    return uart16550_add(port);
}

static void uart16550_fw_detach(device_t* restrict device)
{
    uart16550_remove(device->ctxdata);
}

static void uart16550_fw_release(device_t* restrict device)
{
    if(device->ctxdata) {
        uart16550_free(device->ctxdata);
    }
}

static const driver_ops_t uart16550_fw_ops = {
    .probe = &uart16550_fw_probe,
    .attach = &uart16550_fw_attach,
    .detach = &uart16550_fw_detach,
    .release = &uart16550_fw_release,
};

define_driver(uart16550_fw, &uart16550_fw_ops, uart16550_fw_matches);
