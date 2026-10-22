#pragma once

#include <vex/compiler.h>
#include <vex/io.h>
#include <vex/serial_interface.h>
#include <vex/spinlock.h>
#include <vex/types.h>

typedef struct device_s device_t;

#define UART16550_CLOCK  1843200 // Of the PC's COM ports, in Hz; the baud generator divides it by 16
#define UART16550_ID_MAX 16

typedef struct uart16550_s uart16550_t;

struct uart16550_s {
    serial_interface_t serial;
    io_region_t io;
    unsigned reg_shift; // Registers are 1 << reg_shift bytes apart
    uint32_t clock;     // In Hz
    size_t fifo_size;   // Of the transmitter, 1 without a FIFO
    spinlock_t lock;    // Over the registers, taken with interrupts disabled
    uint8_t ier;
    uint8_t lcr;
    uint8_t mcr;
    bool tx_requested; // start_tx came since the interrupt handler last found nothing to send
    bool added;
    char firmware_id[UART16550_ID_MAX];
};

/// Allocates a port on registers the bus glue mapped, with its interrupts off
/// @param device The node of the port, which its serial device names as its hardware
/// @param name What the firmware calls the device, or nullptr; the port keeps a copy
/// @return nullptr when out of memory
uart16550_t* uart16550_create(device_t* restrict device, const io_region_t* restrict io, unsigned reg_shift, uint32_t clock,
    const char* restrict name);

/// Checks that a 16550 answers, takes the line the firmware left it on and makes the serial port
/// @pre The bus glue's interrupt handler is attached, as a consumer may start the port up at once
/// @retval -ENODEV Nothing answers on the registers
int uart16550_add(uart16550_t* restrict port);

/// Takes the serial port away and turns its interrupts off
/// @post Nothing touches the registers any more but the interrupt handler, so the bus glue
/// may detach it, unmap the registers and call uart16550_free
void uart16550_remove(uart16550_t* restrict port);

/// Lets go of the port: frees one uart16550_add didn't take, or one uart16550_remove took away
/// once the serial core is done with it too
void uart16550_free(uart16550_t* restrict port);

/// Handles what the port interrupts for; the bus glue calls it from its interrupt handler
/// @note Returns at once when the port has nothing pending, as on a shared line
void uart16550_interrupt(uart16550_t* restrict port);
