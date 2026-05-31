#include <arch/interrupts.h>
#include <arch/ireq.h>
#include <bus/isa.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <init/initcall.h>
#include <lib/string.h>

static void i8042_wait_write(struct isa_device* restrict device, struct isa_resource_pmio* restrict pmio)
{
    unsigned long i;

    for(i = 0; i < 65536; ++i) {
        if(0 == (isa_read8(device, pmio, 0) & 0x02)) {
            return;
        }
    }
}

static void i8042_wait_read(struct isa_device* restrict device, struct isa_resource_pmio* restrict pmio)
{
    unsigned long i;

    for(i = 0; i < 65536; ++i) {
        if(isa_read8(device, pmio, 0) & 0x01) {
            return;
        }
    }
}

static void ps2kbd_ireq(unsigned number, struct interrupt_frame* restrict frame, void* restrict ctxdata)
{
    struct isa_device* device = ctxdata;
    struct isa_resource* data_res;
    struct isa_resource* cmd_res;
    uint8_t scancode;

    data_res = isa_get_resource(device, ISA_RESOURCE_PMIO, 0);
    cmd_res = isa_get_resource(device, ISA_RESOURCE_PMIO, 1);

    if(data_res == nullptr || cmd_res == nullptr) {
        return;
    }

    if(0 == (isa_read8(device, &cmd_res->pmio, 0) & 0x01)) {
        return; // Spurious interrupt, output buffer is empty
    }

    scancode = isa_read8(device, &data_res->pmio, 0);

    if(scancode & 0x80) {
        pr_inform("ps2kbd: key up   scancode=0x%02X", (unsigned)(scancode & 0x7F));
    }
    else {
        pr_inform("ps2kbd: key down scancode=0x%02X", (unsigned)(scancode));
    }
}

static int ps2kbd_probe(struct isa_device* restrict device)
{
    struct isa_resource* data_res;
    struct isa_resource* cmd_res;
    uint8_t command_byte;
    int retval;

    if(strcmp(device->identifier, "PNP0303")) {
        return ENODEV;
    }

    // PNP0303 has two IO ranges: data (0x60) and command/status (0x64)
    data_res = isa_get_resource(device, ISA_RESOURCE_PMIO, 0);
    cmd_res = isa_get_resource(device, ISA_RESOURCE_PMIO, 1);

    if(data_res == nullptr || cmd_res == nullptr) {
        return ENODEV;
    }

    i8042_wait_write(device, &cmd_res->pmio);
    isa_write8(device, &cmd_res->pmio, 0, 0x20);

    i8042_wait_read(device, &cmd_res->pmio);
    command_byte = isa_read8(device, &data_res->pmio, 0);
    command_byte |= 0x01;   // enable keyboard interrupt
    command_byte &= ~0x10U; // clear "disable keyboard" bit

    i8042_wait_write(device, &cmd_res->pmio);
    isa_write8(device, &cmd_res->pmio, 0, 0x60);

    i8042_wait_write(device, &cmd_res->pmio);
    isa_write8(device, &data_res->pmio, 0, command_byte);

    // Flush the output buffer; if the UEFI/firmware left stale
    // data in the buffer, its IRQ line is already asserted HIGH
    // and registering an edge-triggered IOAPIC entry at that point
    // means no new rising edge will ever fire, all subsequent
    // key presses will be silently lost
    while(isa_read8(device, &cmd_res->pmio, 0) & 0x01) {
        isa_read8(device, &data_res->pmio, 0);
    }

    // Enable keyboard scanning; UEFI or a bootloader may leave
    // the keyboard in a scanning-disabled state;
    i8042_wait_write(device, &cmd_res->pmio);
    isa_write8(device, &data_res->pmio, 0, 0xF4);
    i8042_wait_read(device, &cmd_res->pmio);
    isa_read8(device, &data_res->pmio, 0); // discard ACK

    retval = isa_request_irq(device, 0, &ps2kbd_ireq, device);

    if(retval) {
        return retval;
    }

    pr_debug("ps2kbd: probed %s, command_byte=0x%02x", device->identifier, (unsigned)command_byte);

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
    .type = ISA_DRIVER_GENERIC,
};

define_initcall(ps2kbd, INITCALL_STAGE_BSP_DEV, INITCALL_ORDER_EARLY)
{
    register_isa_driver(&ps2kbd_driver);
}
