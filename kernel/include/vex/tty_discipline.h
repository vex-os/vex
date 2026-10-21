#pragma once

#include <vex/compiler.h>
#include <vex/tty_device.h>
#include <vex/types.h>

#define TTY_EXPANSION_MAX 16 // What one byte of output may become: a tab as spaces, or \r\n with fill bytes

// The calls between the tty core and its line discipline. The discipline keeps to
// its own state and termios, and reaches the output and signals through the core.
// Both sides call the other's functions with the device's lock held, except for
// tty_discipline_init and tty_discipline_deinit

/// @retval -ENOMEM No memory for the input ring
int tty_discipline_init(tty_device_t* restrict device);

void tty_discipline_deinit(tty_device_t* restrict device);

/// Empties the input and forgets the line being edited, for a first open
void tty_discipline_reset(tty_device_t* restrict device);

/// Empties the input and drops the line being edited
void tty_discipline_flush_input(tty_device_t* restrict device);

/// Runs a byte from the line through the discipline: line errors, input editing, echo, and the signal characters
/// @param error One of TTY_ERROR_*, TTY_ERROR_NONE for a byte that came intact; never TTY_ERROR_OVERRUN
void tty_discipline_receive(tty_device_t* restrict device, uint8_t byte, unsigned error);

/// Puts a byte written to the terminal through OPOST into the output
/// @return Whether there was room; when there was none, nothing went in
bool tty_discipline_output(tty_device_t* restrict device, uint8_t byte);

/// @return How many bytes a read could take now, a line at most in canonical mode
size_t tty_discipline_readable(const tty_device_t* restrict device) __nodiscard;

/// Takes what a read may have: in canonical mode up to the end of a line
/// @param end_of_file Set when a VEOF on an empty line was taken, which the read reports as end of file
/// @return How many bytes went into the buffer
size_t tty_discipline_read(tty_device_t* restrict device, uint8_t* restrict buffer, size_t size, bool* restrict end_of_file);

/// Follows a termios change: leaving canonical mode makes the line being edited readable
void tty_discipline_set_termios(tty_device_t* restrict device, const termios_t* restrict old_termios);

/// Puts bytes into the output, all or none
/// @return Whether there was room for them
bool tty_output_put(tty_device_t* restrict device, const uint8_t* restrict bytes, size_t count) __nodiscard;

/// Sends a byte before the output, even while it is stopped; one still waiting is replaced
/// @note The tty core starts the transmitter for it once the lock is dropped
void tty_output_put_ahead(tty_device_t* restrict device, uint8_t byte);

void tty_output_flush(tty_device_t* restrict device);

/// Stops or restarts taking the output for the line; tty_output_put_ahead isn't held back
void tty_output_set_stopped(tty_device_t* restrict device, bool stopped);

bool tty_output_is_stopped(const tty_device_t* restrict device) __nodiscard;

/// Has the tty core send a signal to the foreground process group once the lock is dropped
void tty_queue_signal(tty_device_t* restrict device, int sig);
