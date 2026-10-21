#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/isa.h>
#include <vex/kmalloc.h>
#include <vex/kprintf.h>

typedef struct ps2kbd_s ps2kbd_t;

struct ps2kbd_s {
    io_region_t data;
    io_region_t command;
};

static const char* const ps2kbd_ids[] = {
    "PNP0303",
    nullptr,
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

static int ps2kbd_probe(isa_device_t* restrict device)
{
    ps2kbd_t* ps2kbd = kmalloc(sizeof(ps2kbd_t));

    if(ps2kbd == nullptr) {
        return -ENOMEM;
    }

    int retval = isa_map_io(device, 0, &ps2kbd->data);

    if(retval < 0) {
        kfree(ps2kbd);
        return retval;
    }

    retval = isa_map_io(device, 1, &ps2kbd->command);

    if(retval < 0) {
        isa_unmap_io(&ps2kbd->data);
        kfree(ps2kbd);
        return retval;
    }

    enable_keyboard(ps2kbd);

    retval = isa_request_irq(device, 0, &ps2kbd_irq, ps2kbd);

    if(retval < 0) {
        isa_unmap_io(&ps2kbd->command);
        isa_unmap_io(&ps2kbd->data);
        kfree(ps2kbd);
        return retval;
    }

    device->ctxdata = ps2kbd;

    return 0;
}

static void ps2kbd_remove(isa_device_t* restrict device)
{
    ps2kbd_t* ps2kbd = device->ctxdata;

    isa_release_irq(device, 0);
    isa_unmap_io(&ps2kbd->command);
    isa_unmap_io(&ps2kbd->data);
    kfree(ps2kbd);

    device->ctxdata = nullptr;
}

static const isa_driver_ops_t ps2kbd_ops = {
    .probe = &ps2kbd_probe,
    .remove = &ps2kbd_remove,
};

static isa_driver_t ps2kbd_driver = {
    .ops = &ps2kbd_ops,
    .ids = ps2kbd_ids,
};

static void init_ps2kbd(void)
{
    isa_driver_add(&ps2kbd_driver);
}

initcall_bsp_dev(init_ps2kbd, INIT_EARLY);
