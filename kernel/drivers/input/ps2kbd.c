#include <arch/irq.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/isa.h>
#include <vex/kprintf.h>
#include <vex/string.h>

static void i8042_wait_write(io_region_t* restrict region)
{
    unsigned long i;

    for(i = 0; i < 65536; ++i) {
        if(0 == (io_read8(region, 0) & 0x02)) {
            return;
        }
    }
}

static void i8042_wait_read(io_region_t* restrict region)
{
    unsigned long i;

    for(i = 0; i < 65536; ++i) {
        if(io_read8(region, 0) & 0x01) {
            return;
        }
    }
}

static void ps2kbd_irq(unsigned number, const hart_context_t* restrict ctx, void* restrict ctxdata)
{
    isa_device_t* device = ctxdata;

    isa_resource_t* data_res = isa_get_resource(device, ISA_RESOURCE_PMIO, 0);
    isa_resource_t* cmd_res = isa_get_resource(device, ISA_RESOURCE_PMIO, 1);

    if(data_res == nullptr || cmd_res == nullptr) {
        return;
    }

    if(0 == (io_read8(&cmd_res->io, 0) & 0x01)) {
        return;
    }

    uint8_t scancode = io_read8(&data_res->io, 0);

    if(scancode & 0x80) {
        pr_verbose("ps2kbd: key up   scancode=0x%02X", (unsigned)(scancode & 0x7F));
    }
    else {
        pr_verbose("ps2kbd: key down scancode=0x%02X", (unsigned)scancode);
    }
}

static int ps2kbd_probe(isa_device_t* restrict device)
{
    if(strcmp(device->name, "PNP0303")) {
        return -ENODEV;
    }

    isa_resource_t* data_res = isa_get_resource(device, ISA_RESOURCE_PMIO, 0);
    isa_resource_t* cmd_res = isa_get_resource(device, ISA_RESOURCE_PMIO, 1);

    if(data_res == nullptr || cmd_res == nullptr) {
        return -ENODEV;
    }

    i8042_wait_write(&cmd_res->io);
    io_write8(&cmd_res->io, 0, 0x20);

    i8042_wait_read(&cmd_res->io);
    uint8_t command_byte = io_read8(&data_res->io, 0);
    command_byte |= 0x01;
    command_byte &= ~0x10U;

    i8042_wait_write(&cmd_res->io);
    io_write8(&cmd_res->io, 0, 0x60);

    i8042_wait_write(&cmd_res->io);
    io_write8(&data_res->io, 0, command_byte);

    while(io_read8(&cmd_res->io, 0) & 0x01) {
        io_read8(&data_res->io, 0);
    }

    i8042_wait_write(&cmd_res->io);
    io_write8(&data_res->io, 0, 0xF4);
    i8042_wait_read(&cmd_res->io);
    io_read8(&data_res->io, 0);

    int retval = isa_request_irq(device, 0, &ps2kbd_irq, device);

    if(retval < 0) {
        return retval;
    }

    return 0;
}

static void ps2kbd_remove(isa_device_t* restrict device)
{
    isa_release_irq(device, 0);
}

static const isa_driver_ops_t ps2kbd_ops = {
    .probe = &ps2kbd_probe,
    .remove = &ps2kbd_remove,
};

static isa_driver_t ps2kbd_driver = {
    .ops = &ps2kbd_ops,
    .name = "ps2kbd",
};

static void init_ps2kbd(void)
{
    isa_driver_add(&ps2kbd_driver);
}

initcall_bsp_dev(init_ps2kbd, INIT_EARLY);
