#pragma once

#include <vex/compiler.h>
#include <vex/tty_device.h>
#include <vex/types.h>

#define TTY_EXPANSION_MAX 16 // What one byte of output may become: a tab as spaces, or \r\n with fill bytes

// The calls between the tty core and its line discipline; the tty core calls
// each with its device's lock held, and starts the transmitter when one
// left IXOFF's VSTOP or VSTART in priv.xchar

/// Empties the input and drops the line being edited
void tty_ldisc_flush_input(tty_device_t* restrict device);

/// Runs what the flip ring holds through the line discipline: line errors, input editing, echo,
/// and the signal characters, which it leaves in ldisc.signals for tty_worker to send
void tty_ldisc_receive(tty_device_t* restrict device);

/// Puts a byte written to the terminal through OPOST into the output ring
/// @return Whether there was room; when there was none, nothing went in
bool tty_ldisc_output(tty_device_t* restrict device, uint8_t byte) __nodiscard;

/// @return How many bytes a read could take now, a line at most in canonical mode
size_t tty_ldisc_readable(const tty_device_t* restrict device) __nodiscard;

/// Takes what a read may have: in canonical mode up to the end of a line
/// @param end_of_file Set when a VEOF on an empty line was taken, which the read reports as end of file
/// @return How many bytes went into the buffer
size_t tty_ldisc_read(tty_device_t* restrict device, uint8_t* restrict buffer, size_t size, bool* restrict end_of_file);

/// Follows a termios change: leaving canonical mode makes the line being edited readable
void tty_ldisc_set_termios(tty_device_t* restrict device, const termios_t* restrict old_termios);

static __always_inline uint32_t tty_ring_used(const tty_ring_t* restrict ring)
{
    return ring->tail - ring->head;
}

static __always_inline uint32_t tty_ring_room(const tty_ring_t* restrict ring)
{
    return TTY_RING_SIZE - (ring->tail - ring->head);
}

static __always_inline uint8_t tty_ring_at(const tty_ring_t* restrict ring, uint32_t position)
{
    return ring->data[position % TTY_RING_SIZE];
}

static __always_inline void tty_ring_put(tty_ring_t* restrict ring, uint8_t byte)
{
    ring->data[ring->tail % TTY_RING_SIZE] = byte;
    ring->tail += 1;
}

static __always_inline uint8_t tty_ring_take(tty_ring_t* restrict ring)
{
    uint8_t byte = ring->data[ring->head % TTY_RING_SIZE];
    ring->head += 1;
    return byte;
}
