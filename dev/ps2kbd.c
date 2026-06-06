#include <arch/interrupts.h>
#include <arch/ireq.h>
#include <bus/isa.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <init/initcall.h>
#include <lib/string.h>

static void i8042_wait_write(struct pmio_region* restrict region)
{
    unsigned long i;

    for(i = 0; i < 65536; ++i) {
        if(0 == (pmio_read8(region, 0) & 0x02)) {
            return;
        }
    }
}

static void i8042_wait_read(struct pmio_region* restrict region)
{
    unsigned long i;

    for(i = 0; i < 65536; ++i) {
        if(pmio_read8(region, 0) & 0x01) {
            return;
        }
    }
}

static void ps2kbd_ireq(unsigned number, struct interrupt_frame* restrict frame, void* restrict ctxdata)
{
    struct isa_device* device = ctxdata;

    struct isa_resource* data_res = isa_get_resource(device, ISA_RESOURCE_PMIO, 0);
    struct isa_resource* cmd_res = isa_get_resource(device, ISA_RESOURCE_PMIO, 1);

    if(data_res == nullptr || cmd_res == nullptr) {
        return;
    }

    if(0 == (pmio_read8(&cmd_res->pmio, 0) & 0x01)) {
        return;
    }

    uint8_t scancode = pmio_read8(&data_res->pmio, 0);

    if(scancode & 0x80) {
        pr_verbose("ps2kbd: key up   scancode=0x%02X", (unsigned)(scancode & 0x7F));
    }
    else {
        pr_verbose("ps2kbd: key down scancode=0x%02X", (unsigned)scancode);
    }
}

static int ps2kbd_probe(struct isa_device* restrict device)
{
    if(strcmp(device->identifier, "PNP0303")) {
        return -ENODEV;
    }

    struct isa_resource* data_res = isa_get_resource(device, ISA_RESOURCE_PMIO, 0);
    struct isa_resource* cmd_res = isa_get_resource(device, ISA_RESOURCE_PMIO, 1);

    if(data_res == nullptr || cmd_res == nullptr) {
        return -ENODEV;
    }

    i8042_wait_write(&cmd_res->pmio);
    pmio_write8(&cmd_res->pmio, 0, 0x20);

    i8042_wait_read(&cmd_res->pmio);
    uint8_t command_byte = pmio_read8(&data_res->pmio, 0);
    command_byte |= 0x01;
    command_byte &= ~0x10U;

    i8042_wait_write(&cmd_res->pmio);
    pmio_write8(&cmd_res->pmio, 0, 0x60);

    i8042_wait_write(&cmd_res->pmio);
    pmio_write8(&data_res->pmio, 0, command_byte);

    while(pmio_read8(&cmd_res->pmio, 0) & 0x01) {
        pmio_read8(&data_res->pmio, 0);
    }

    i8042_wait_write(&cmd_res->pmio);
    pmio_write8(&data_res->pmio, 0, 0xF4);
    i8042_wait_read(&cmd_res->pmio);
    pmio_read8(&data_res->pmio, 0);

    int retval = isa_request_irq(device, 0, &ps2kbd_ireq, device);

    if(retval < 0) {
        return retval;
    }

    pr_verbose("ps2kbd: probed %s, command_byte=0x%02x", device->identifier, (unsigned)command_byte);

    return 0;
}

static void ps2kbd_remove(struct isa_device* restrict device)
{
    isa_release_irq(device, 0);
}

static const struct isa_driver_ops ps2kbd_ops = {
    .probe = &ps2kbd_probe,
    .remove = &ps2kbd_remove,
};

static struct isa_driver ps2kbd_driver = {
    .ops = &ps2kbd_ops,
    .identifier = "ps2kbd",
};

define_initcall(ps2kbd, INITCALL_STAGE_BSP_DEV, INITCALL_ORDER_EARLY)
{
    isa_driver_register(&ps2kbd_driver);
}
