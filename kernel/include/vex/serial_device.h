#pragma once

#include <vex/chr_device.h>
#include <vex/compiler.h>
#include <vex/console.h>
#include <vex/mutex.h>
#include <vex/serial.h>
#include <vex/spinlock.h>
#include <vex/types.h>

typedef struct dev_unit_s dev_unit_t;

#define SERIAL_ERROR_NONE    0U
#define SERIAL_ERROR_BREAK   1U
#define SERIAL_ERROR_PARITY  2U
#define SERIAL_ERROR_FRAMING 3U
#define SERIAL_ERROR_OVERRUN 4U // The hardware lost bytes before this one, which came intact

#define SERIAL_DEFAULT_SPEED 38400

typedef struct serial_device_s serial_device_t;
typedef struct serial_device_ops_s serial_device_ops_t;
typedef struct serial_device_private_s serial_device_private_t;
typedef struct serial_consumer_s serial_consumer_t;

struct serial_device_ops_s {
    /// Enables the interrupts and starts taking input
    /// @pre The serial core serializes it with the other calls but start_tx, and the port is shut down
    /// @note nullptr when there is nothing to start
    int (*startup)(serial_device_t* restrict device);

    /// @pre The serial core serializes it with the other calls but start_tx, and the port is started up
    void (*shutdown)(serial_device_t* restrict device);

    /// There is output: take it with serial_transmit_take, now or from an interrupt
    /// @pre The port's dispatch lock is not held; may come again before the driver took anything
    void (*start_tx)(serial_device_t* restrict device);

    /// @pre The serial core serializes it with the other calls but start_tx, and checked
    /// the line: a nonzero speed, 5 to 8 data bits, 1 or 2 stop bits, a known parity
    /// @retval -EINVAL The hardware can't run the line so
    /// @note Comes whether the port is started up or not
    int (*set_line)(serial_device_t* restrict device, const serial_line_t* restrict line);

    /// @pre The serial core serializes it with the other calls but start_tx
    /// @param lines SERIAL_MODEM_DTR and SERIAL_MODEM_RTS to raise; the one missing is lowered
    /// @note nullptr when modem_lines has neither
    void (*set_modem)(serial_device_t* restrict device, uint32_t lines);

    /// @pre The serial core serializes it with the other calls but start_tx
    /// @return The SERIAL_MODEM_* lines that are raised
    /// @note nullptr when modem_lines has no input
    uint32_t (*get_modem)(serial_device_t* restrict device);

    /// @pre The serial core serializes it with the other calls but start_tx
    /// @note nullptr when the port can't send a break
    void (*set_break)(serial_device_t* restrict device, bool on);

    void (*release)(const serial_device_t* restrict device);

    /// Writes bytes for the kernel console, waiting on the transmitter instead of an interrupt
    /// @pre Interrupts may be disabled, and any other call may have stopped midway, as in a panic
    /// @note Takes no lock; nullptr when the port can't be a console
    void (*poll_write)(serial_device_t* restrict device, const void* restrict data, size_t size);

    /// Waits until what poll_write wrote is on the line
    /// @pre As for poll_write
    void (*poll_drain)(serial_device_t* restrict device);

    /// Turns the port's interrupts off and sets the line, so poll_write works whatever the driver left the port in
    /// @pre As for poll_write
    void (*poll_reset)(serial_device_t* restrict device, const serial_line_t* restrict line);
};

struct serial_device_private_s {
    dev_unit_t* unit;
    mutex_t lock;                      // Serializes the calls into the driver but start_tx, and the binding
    spinlock_t dispatch_lock;          // Taken with interrupts disabled: consumer, which the bytes go to and come from
    const serial_consumer_t* consumer; // Bound, or an exchange's; changes under both locks, or the dispatch lock alone while unbinding
    serial_binding_t binding;          // consumer[0] is NUL while nothing is bound
    bool unbinding;                    // A consumer's unbind runs, without the lock
    bool started;
    uint32_t modem; // The SERIAL_MODEM_DTR and SERIAL_MODEM_RTS raised
    uint32_t lost;  // Bytes that came with nothing to take them
    bool removed;
    console_t console;          // Set up when console= names the port
    serial_line_t console_line; // What a panic puts the line back to
};

struct serial_device_s {
    chr_device_t chr; // Its ops and ctxdata belong to the serial core
    serial_device_private_t priv;
    const serial_device_ops_t* ops;
    const char* driver;
    const char* firmware_id; // What the firmware calls the device; nullptr when it doesn't
    uint32_t max_speed;      // In bits per second
    uint32_t modem_lines;    // The SERIAL_MODEM_* lines the port has
    serial_line_t line;      // As the hardware is set before serial_device_add, a zero speed for 38400 8N1; the serial core's after
    void* consumer_data;     // The bound consumer's own
    void* ctxdata;
};

struct serial_consumer_s {
    serial_consumer_t* next;
    const char* name;

    /// Makes what the consumer turns the port into, such as a terminal
    /// @pre The port's lock is held, and nothing is bound to the port
    /// @param index The index to give what it makes, SERIAL_ANY_INDEX for the first free; comes back as the one given
    /// @param object Zeroed; gets the name of what it made
    /// @retval -EEXIST The index is taken
    /// @retval -EINVAL The consumer doesn't take the options
    /// @note May set consumer_data, and gets the bytes from the port as soon as it returns
    int (*bind)(serial_device_t* restrict device, const char* restrict options, int32_t* restrict index, char* restrict object,
        size_t size);

    /// Takes away what bind made
    /// @pre The port's lock is not held, as the last calls of what is taken away into the port may need it
    /// @param force The port is going away, so it is taken away even when in use
    /// @retval -EBUSY It is in use, and force is false
    /// @note Calls serial_device_detach before freeing what receive and transmit_take touch
    int (*unbind)(serial_device_t* restrict device, bool force);

    /// @pre The port's dispatch lock is held, so this may run in an interrupt handler
    void (*receive)(serial_device_t* restrict device, const uint8_t* restrict bytes, size_t count);

    /// @pre The port's dispatch lock is held, so this may run in an interrupt handler
    /// @param error One of SERIAL_ERROR_*, but SERIAL_ERROR_NONE
    void (*receive_error)(serial_device_t* restrict device, uint8_t byte, unsigned error);

    /// @pre The port's dispatch lock is held, so this may run in an interrupt handler
    /// @return How many bytes it put into the buffer
    size_t (*transmit_take)(serial_device_t* restrict device, void* restrict buffer, size_t size);
};

/// Makes the port's control node, serialN, and binds what the serial.bind= command line option names for it
int serial_device_add(serial_device_t* restrict device);

/// Takes away what is bound to the port, even when in use, and the control node
/// @post No call into the serial core touches the driver any more; release still comes
int serial_device_remove(serial_device_t* restrict device);

/// @pre At initcall time, before the ports that the serial.bind= option binds it to are added
void serial_consumer_register(serial_consumer_t* restrict consumer);

/// Hands bytes from the line to what the port is bound to; with nothing bound they are lost and counted
/// @note Any context, an interrupt handler among them
void serial_receive(serial_device_t* restrict device, const void* restrict bytes, size_t count);

/// Hands over a byte that came with a line error
/// @param byte What the hardware read; a break's is ignored
/// @param error One of SERIAL_ERROR_*, but SERIAL_ERROR_NONE
/// @note Any context, an interrupt handler among them
void serial_receive_error(serial_device_t* restrict device, uint8_t byte, unsigned error);

/// Takes bytes to send on the line
/// @return How many bytes it took; zero when there is nothing to send
/// @note Any context, an interrupt handler among them
size_t serial_transmit_take(serial_device_t* restrict device, void* restrict buffer, size_t size);

/// Stops the bytes going to and coming from the consumer bound to the port
/// @pre The bound consumer's unbind calls it
/// @post No receive or transmit_take of the consumer runs or comes
void serial_device_detach(serial_device_t* restrict device);

/// Starts the port up for the bound consumer, such as on the first open of its terminal
/// @retval -ENODEV The port is removed
int serial_device_startup(serial_device_t* restrict device);

void serial_device_shutdown(serial_device_t* restrict device);

/// @retval -EINVAL The line is invalid, or the hardware can't run it
/// @retval -ENODEV The port is removed
int serial_device_set_line(serial_device_t* restrict device, const serial_line_t* restrict line);

/// @param raise SERIAL_MODEM_DTR and SERIAL_MODEM_RTS to raise
/// @param lower SERIAL_MODEM_DTR and SERIAL_MODEM_RTS to lower, unless raise has them
void serial_device_set_modem(serial_device_t* restrict device, uint32_t raise, uint32_t lower);

/// Sends a break and sleeps for as long as it lasts
/// @param msec Zero for a quarter of a second
/// @retval -EINVAL The port can't send a break
/// @retval -ENODEV The port is removed
int serial_device_send_break(serial_device_t* restrict device, uint32_t msec);

/// Has the driver take the output
/// @pre The port's dispatch lock is not held
void serial_device_start_tx(serial_device_t* restrict device);
