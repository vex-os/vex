#pragma once

#include <vex/chr_device.h>
#include <vex/compiler.h>
#include <vex/event.h>
#include <vex/mutex.h>
#include <vex/page.h>
#include <vex/spinlock.h>
#include <vex/termios.h>
#include <vex/types.h>

typedef struct dev_unit_s dev_unit_t;
typedef struct process_group_s process_group_t;
typedef struct session_s session_t;

#define TTY_RING_SIZE PAGE_SIZE

typedef struct tty_ring_s tty_ring_t;
typedef struct tty_ldisc_state_s tty_ldisc_state_t;
typedef struct tty_device_s tty_device_t;
typedef struct tty_device_ops_s tty_device_ops_t;
typedef struct tty_device_private_s tty_device_private_t;

// The positions count up and wrap around
// as uint32_t, which TTY_RING_SIZE divides
struct tty_ring_s {
    uint8_t* data; // A page
    uint32_t head; // The next byte to take
    uint32_t tail; // Where the next byte goes
};

struct tty_ldisc_state_s {
    uint8_t delimiters[TTY_RING_SIZE / 8]; // A set bit ends a line at that input position; a NUL there is VEOF
    uint32_t line_end;                     // Input position past the last line read may hand out; the line being edited follows
    uint32_t line_column;                  // The output column the line being edited started at
    uint32_t column;                       // Of the output, for tabs, ONOCR and erasing
    uint32_t signals;                      // Bit n set: signal n is to go to the foreground group
    bool literal_next;                     // VLNEXT came, so the next byte is plain data
    bool escaped;                          // XCASE: a backslash came, so the next letter is a capital
    bool output_stopped;                   // VSTOP came while IXON was set
    bool hardcopy_erasing;                 // ECHOPRT: erased bytes are being echoed between \ and /
};

struct tty_device_ops_s {
    /// Sets the hardware up and starts taking input
    /// @pre The first open, and the device's console is released
    int (*open)(tty_device_t* restrict device);

    /// @pre The last close
    void (*close)(tty_device_t* restrict device);

    /// There is output: take it with tty_transmit_take, now or from an interrupt
    /// @pre No tty lock is held; may come again before the driver took anything
    void (*start_tx)(tty_device_t* restrict device);

    /// Applies c_cflag and the speeds to the hardware
    /// @pre The tty core serializes it with writes; nullptr when there is nothing to set
    int (*set_termios)(tty_device_t* restrict device, const termios_t* restrict termios);

    /// Requests the tty core doesn't know, TCSENDBREAK among them
    int (*ioctl)(tty_device_t* restrict device, uint32_t request, uintptr_t argument);

    void (*release)(const tty_device_t* restrict device);
};

struct tty_device_private_s {
    spinlock_t lock;    // Taken with interrupts disabled: the rings, ldisc and termios
    mutex_t write_lock; // Keeps writes from mixing, and termios from changing under one
    termios_t termios;
    winsize_t winsize;
    tty_ring_t flip; // From the driver, for tty_worker
    tty_ring_t input;
    tty_ring_t output;
    uint32_t lost; // Bytes the flip ring had no room for
    tty_ldisc_state_t ldisc;
    event_t readable;
    event_t writable;
    event_t drained;
    session_t* session;          // Under process_group_lock, a reference
    process_group_t* foreground; // Under process_group_lock, a reference
    tty_device_t* worker_next;   // Under the worker queue's lock
    bool queued;                 // Under the worker queue's lock
    unsigned in_flight;          // Calls into the tty core that may still touch the device
    bool removed;
};

struct tty_device_s {
    chr_device_t chr; // Its ops and ctxdata belong to the tty core
    tty_device_private_t priv;
    const tty_device_ops_t* ops;
    void* ctxdata;
};

/// @param index The n of the name; DEV_NAME_NONE for a device that is its unit's only one
/// @retval -ENOMEM No memory for the rings
int tty_device_add(tty_device_t* restrict device, dev_unit_t* restrict unit, unsigned index);

/// Hangs the terminal up: readers get end of file, writers -EIO, and its session loses it
/// @post No call into the tty core touches the driver any more; release still comes
int tty_device_remove(tty_device_t* restrict device);

/// Hands bytes from the line to the tty core; what doesn't fit is lost and counted
/// @note Any context, an interrupt handler among them
void tty_receive(tty_device_t* restrict device, const void* restrict bytes, size_t count);

/// Takes bytes to send on the line
/// @return How many bytes it took; zero when the output is empty or stopped
/// @note Any context, an interrupt handler among them
size_t tty_transmit_take(tty_device_t* restrict device, void* restrict buffer, size_t size);
