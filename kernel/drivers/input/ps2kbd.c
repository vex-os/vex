#include <vex/device.h>
#include <vex/driver.h>
#include <vex/errno.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>

typedef struct ps2kbd_s ps2kbd_t;

struct ps2kbd_s {
    io_region_t data;
    io_region_t command;
};

static const driver_match_t ps2kbd_matches[] = {
    DRIVER_MATCH_FIRMWARE("PNP0303"),
    DRIVER_MATCH_END(),
};

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
    ps2kbd_t* ps2kbd = ctxdata;

    if(0 == (io_read8(&ps2kbd->command, 0) & 0x01)) {
        return;
    }

    uint8_t scancode = io_read8(&ps2kbd->data, 0);

    if(scancode & 0x80) {
        pr_verbose("ps2kbd: key up   scancode=0x%02X", (unsigned)(scancode & 0x7F));
    }
    else {
        pr_verbose("ps2kbd: key down scancode=0x%02X", (unsigned)scancode);
    }
}

static void enable_keyboard(ps2kbd_t* restrict ps2kbd)
{
    i8042_wait_write(&ps2kbd->command);
    io_write8(&ps2kbd->command, 0, 0x20);

    i8042_wait_read(&ps2kbd->command);
    uint8_t command_byte = io_read8(&ps2kbd->data, 0);
    command_byte |= 0x01;
    command_byte &= ~0x10U;

    i8042_wait_write(&ps2kbd->command);
    io_write8(&ps2kbd->command, 0, 0x60);

    i8042_wait_write(&ps2kbd->command);
    io_write8(&ps2kbd->data, 0, command_byte);

    while(io_read8(&ps2kbd->command, 0) & 0x01) {
        io_read8(&ps2kbd->data, 0);
    }

    i8042_wait_write(&ps2kbd->command);
    io_write8(&ps2kbd->data, 0, 0xF4);
    i8042_wait_read(&ps2kbd->command);
    io_read8(&ps2kbd->data, 0);
}

// The controller stops raising the keyboard's
// interrupt while the keyboard itself stays on
static void disable_keyboard(ps2kbd_t* restrict ps2kbd)
{
    i8042_wait_write(&ps2kbd->command);
    io_write8(&ps2kbd->command, 0, 0x20);

    i8042_wait_read(&ps2kbd->command);
    uint8_t command_byte = io_read8(&ps2kbd->data, 0) & ~0x01U;

    i8042_wait_write(&ps2kbd->command);
    io_write8(&ps2kbd->command, 0, 0x60);

    i8042_wait_write(&ps2kbd->command);
    io_write8(&ps2kbd->data, 0, command_byte);
}

static int ps2kbd_probe(device_t* restrict device, const driver_match_t* restrict match)
{
    return DRIVER_SCORE_DEFAULT;
}

static int ps2kbd_attach(device_t* restrict device, const driver_match_t* restrict match)
{
    ps2kbd_t* ps2kbd = kmalloc(sizeof(ps2kbd_t));

    if(ps2kbd == nullptr) {
        return -ENOMEM;
    }

    device->ctxdata = ps2kbd;

    int retval = device_map_io(device, 0, &ps2kbd->data);

    if(retval < 0) {
        return retval;
    }

    retval = device_map_io(device, 1, &ps2kbd->command);

    if(retval < 0) {
        return retval;
    }

    enable_keyboard(ps2kbd);

    return device_request_irq(device, 0, &ps2kbd_irq, ps2kbd);
}

static void ps2kbd_detach(device_t* restrict device)
{
    ps2kbd_t* ps2kbd = device->ctxdata;

    if(!device_is_gone(device)) {
        disable_keyboard(ps2kbd);
    }
}

static void ps2kbd_release(device_t* restrict device)
{
    kfree(device->ctxdata);
}

static const driver_ops_t ps2kbd_ops = {
    .probe = &ps2kbd_probe,
    .attach = &ps2kbd_attach,
    .detach = &ps2kbd_detach,
    .release = &ps2kbd_release,
};

define_driver(ps2kbd, &ps2kbd_ops, ps2kbd_matches);
