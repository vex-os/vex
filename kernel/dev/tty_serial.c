#include <vex/chr_device.h>
#include <vex/dev_name.h>
#include <vex/errno.h>
#include <vex/initcall.h>
#include <vex/kmalloc.h>
#include <vex/serial_device.h>
#include <vex/string.h>
#include <vex/strings.h>
#include <vex/tty_device.h>
#include <vex/uaccess.h>

typedef struct tty_serial_s tty_serial_t;

struct tty_serial_s {
    tty_device_t tty;
    serial_device_t* port; // A reference, through its control node
    dev_unit_t* unit;
    bool hangup_on_close; // HUPCL, kept from set_termios as close has no termios
    bool hung_up;         // A zero speed lowered DTR
};

static_assert(SERIAL_ERROR_BREAK == TTY_ERROR_BREAK && SERIAL_ERROR_PARITY == TTY_ERROR_PARITY);
static_assert(SERIAL_ERROR_FRAMING == TTY_ERROR_FRAMING && SERIAL_ERROR_OVERRUN == TTY_ERROR_OVERRUN);

static tty_serial_t* tty_serial_of(const tty_device_t* restrict device)
{
    return device->ctxdata;
}

static int tty_serial_open(tty_device_t* restrict device)
{
    tty_serial_t* terminal = tty_serial_of(device);
    int retval = serial_device_startup(terminal->port);

    if(retval < 0) {
        return retval;
    }

    serial_device_set_modem(terminal->port, SERIAL_MODEM_DTR | SERIAL_MODEM_RTS, 0);
    terminal->hung_up = false;

    return 0;
}

static void tty_serial_close(tty_device_t* restrict device)
{
    tty_serial_t* terminal = tty_serial_of(device);

    if(terminal->hangup_on_close) {
        serial_device_set_modem(terminal->port, 0, SERIAL_MODEM_DTR | SERIAL_MODEM_RTS);
    }

    serial_device_shutdown(terminal->port);
}

static void tty_serial_start_tx(tty_device_t* restrict device)
{
    tty_serial_t* terminal = tty_serial_of(device);
    serial_device_start_tx(terminal->port);
}

static uint8_t data_bits_of(uint32_t cflag)
{
    switch(cflag & CSIZE) {
        case CS5:
            return 5;
        case CS6:
            return 6;
        case CS7:
            return 7;
        default:
            return 8;
    }
}

static uint8_t parity_of(uint32_t cflag)
{
    if(0 == (cflag & PARENB)) {
        return SERIAL_PARITY_NONE;
    }

    if(cflag & PARODD) {
        return SERIAL_PARITY_ODD;
    }

    return SERIAL_PARITY_EVEN;
}

static int tty_serial_set_termios(tty_device_t* restrict device, const termios_t* restrict termios)
{
    tty_serial_t* terminal = tty_serial_of(device);
    uint32_t cflag = termios->c_cflag;

    if(termios->c_ispeed && termios->c_ispeed != termios->c_ospeed) {
        return -EINVAL;
    }

    terminal->hangup_on_close = cflag & HUPCL;

    if(termios->c_ospeed == 0) {
        serial_device_set_modem(terminal->port, 0, SERIAL_MODEM_DTR);
        terminal->hung_up = true;
        return 0;
    }

    serial_line_t line = {};
    line.speed = termios->c_ospeed;
    line.data_bits = data_bits_of(cflag);
    line.stop_bits = (cflag & CSTOPB) ? 2 : 1;
    line.parity = parity_of(cflag);

    if(cflag & CRTSCTS) {
        line.flow = SERIAL_FLOW_RTSCTS;
    }

    int retval = serial_device_set_line(terminal->port, &line);

    if(retval == 0 && terminal->hung_up) {
        serial_device_set_modem(terminal->port, SERIAL_MODEM_DTR | SERIAL_MODEM_RTS, 0);
        terminal->hung_up = false;
    }

    return retval;
}

// A nonzero duration is in milliseconds, which POSIX leaves to the implementation
static int send_break(tty_serial_t* restrict terminal, uintptr_t argument)
{
    int32_t duration;

    if(user_copyin(&duration, argument, sizeof(duration)) < 0) {
        return -EFAULT;
    }

    if(duration < 0) {
        return -EINVAL;
    }

    return serial_device_send_break(terminal->port, duration);
}

static int tty_serial_ioctl(tty_device_t* restrict device, uint32_t request, uintptr_t argument)
{
    if(request == TCSENDBREAK) {
        return send_break(tty_serial_of(device), argument);
    }

    return -ENOTTY;
}

static void tty_serial_release(const tty_device_t* restrict device)
{
    tty_serial_t* terminal = tty_serial_of(device);

    chr_device_decref(&terminal->port->chr);
    kfree(terminal);
}

static const tty_device_ops_t tty_serial_ops = {
    .open = &tty_serial_open,
    .close = &tty_serial_close,
    .start_tx = &tty_serial_start_tx,
    .set_termios = &tty_serial_set_termios,
    .ioctl = &tty_serial_ioctl,
    .release = &tty_serial_release,
};

static int take_unit(int32_t index, dev_unit_t** restrict unit)
{
    if(index == SERIAL_ANY_INDEX) {
        return dev_unit_alloc(&dev_prefix_tty, unit);
    }

    return dev_unit_claim(&dev_prefix_tty, index, unit);
}

static int tty_serial_bind(serial_device_t* restrict device, const char* restrict options, int32_t* restrict index, char* restrict object,
    size_t size)
{
    if(options[0]) {
        return -EINVAL;
    }

    tty_serial_t* terminal = kmalloc(sizeof(tty_serial_t));

    if(terminal == nullptr) {
        return -ENOMEM;
    }

    memset(terminal, 0, sizeof(*terminal));

    int retval = take_unit(*index, &terminal->unit);

    if(retval < 0) {
        kfree(terminal);
        return retval;
    }

    terminal->port = device;
    terminal->tty.ops = &tty_serial_ops;
    terminal->tty.initial_speed = device->line.speed;
    terminal->tty.ctxdata = terminal;

    chr_device_incref(&device->chr);
    device->consumer_data = terminal;

    retval = tty_device_add(&terminal->tty, terminal->unit, DEV_NAME_NONE);

    if(retval < 0) {
        device->consumer_data = nullptr;
        chr_device_decref(&device->chr);
        dev_unit_free(terminal->unit);
        kfree(terminal);
        return retval;
    }

    *index = terminal->unit->number;
    kstrncpy(object, terminal->tty.chr.name, size - 1);

    return 0;
}

static int tty_serial_unbind(serial_device_t* restrict device, bool force)
{
    tty_serial_t* terminal = device->consumer_data;
    dev_unit_t* unit = terminal->unit;

    if(!force) {
        int retval = chr_device_refuse_opens(&terminal->tty.chr);

        if(retval < 0) {
            return retval;
        }
    }

    // Before the removal, which may free the terminal
    serial_device_detach(device);

    // A hangup when forced; the terminal is freed once the last file of it closes
    (void)tty_device_remove(&terminal->tty);
    dev_unit_free(unit);

    return 0;
}

static void tty_serial_receive(serial_device_t* restrict device, const uint8_t* restrict bytes, size_t count)
{
    tty_serial_t* terminal = device->consumer_data;
    tty_receive(&terminal->tty, bytes, count);
}

static void tty_serial_receive_error(serial_device_t* restrict device, uint8_t byte, unsigned error)
{
    tty_serial_t* terminal = device->consumer_data;
    tty_receive_error(&terminal->tty, byte, error);
}

static size_t tty_serial_transmit_take(serial_device_t* restrict device, void* restrict buffer, size_t size)
{
    tty_serial_t* terminal = device->consumer_data;
    return tty_transmit_take(&terminal->tty, buffer, size);
}

static serial_consumer_t tty_serial_consumer = {
    .name = "tty",
    .bind = &tty_serial_bind,
    .unbind = &tty_serial_unbind,
    .receive = &tty_serial_receive,
    .receive_error = &tty_serial_receive_error,
    .transmit_take = &tty_serial_transmit_take,
};

static void init_tty_serial(void)
{
    serial_consumer_register(&tty_serial_consumer);
}

initcall_bsp_dev(init_tty_serial, INIT_FIRST);
