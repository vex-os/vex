// SPDX-License-Identifier: BSD-3-Clause

#include <arch/interrupts.h>
#include <arch/ireq.h>
#include <bus/isa.h>
#include <core/errno.h>
#include <core/kprintf.h>
#include <init/initfn.h>
#include <lib/string.h>

static void ps2kbd_irq_handler(unsigned number, struct interrupt_frame* restrict frame, void* restrict ctx)
{
    struct isa_device* device = ctx;
    struct isa_resource* data_res;
    struct isa_resource* cmd_res;
    uint8_t scancode;

    data_res = isa_get_resource(device, ISA_RESOURCE_PMIO, 0);
    cmd_res = isa_get_resource(device, ISA_RESOURCE_PMIO, 1);

    if(data_res == nullptr || cmd_res == nullptr)
        return;

    // Don't read if the output buffer is empty (spurious interrupt)
    if(!(isa_read8(device, &cmd_res->pmio, 0) & 0x01))
        return;

    scancode = isa_read8(device, &data_res->pmio, 0);

    if(scancode & 0x80) {
        kprintf(KP_DEBUG, "ps2kbd: key up   scancode=0x%02x", (unsigned)(scancode & 0x7F));
    }
    else {
        kprintf(KP_DEBUG, "ps2kbd: key down scancode=0x%02x", (unsigned)scancode);
    }
}

static void i8042_wait_write(struct isa_device* restrict device, struct isa_resource_pmio* restrict cmd)
{
    unsigned i;
    for(i = 0; i < 65536; ++i) {
        if(!(isa_read8(device, cmd, 0) & 0x02))
            return;
    }
}

static void i8042_wait_read(struct isa_device* restrict device, struct isa_resource_pmio* restrict cmd)
{
    unsigned i;
    for(i = 0; i < 65536; ++i) {
        if(isa_read8(device, cmd, 0) & 0x01)
            return;
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

    if(data_res == nullptr || cmd_res == nullptr)
        return ENODEV;

    // Read current command byte and enable keyboard IRQ (bit 0)
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

    // Flush the output buffer: if the UEFI/firmware left stale data in
    // the i8042 output buffer, IRQ1 is already asserted HIGH. Registering
    // an edge-triggered IOAPIC entry at that point means no new rising
    // edge will ever fire — all subsequent keypresses are silently lost.
    while(isa_read8(device, &cmd_res->pmio, 0) & 0x01)
        isa_read8(device, &data_res->pmio, 0);

    // Enable keyboard scanning: UEFI or the bootloader may leave the
    // keyboard in "scanning disabled" state (0xF5). Send 0xF4 to re-enable.
    i8042_wait_write(device, &cmd_res->pmio);
    isa_write8(device, &data_res->pmio, 0, 0xF4);
    i8042_wait_read(device, &cmd_res->pmio);
    isa_read8(device, &data_res->pmio, 0); // discard ACK (0xFA)

    retval = isa_request_irq(device, 0, &ps2kbd_irq_handler, device);

    if(retval)
        return retval;

    kprintf(KP_INFORM, "ps2kbd: probed %s, command_byte=0x%02x", device->identifier, (unsigned)command_byte);

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

static int init_ps2kbd(void)
{
    return register_isa_driver(&ps2kbd_driver);
}

DEFINE_INITFN(init_ps2kbd, INITFN_STAGE_DEV, INITFN_ORDER_EARLY);
