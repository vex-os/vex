#pragma once

#include <vex/compiler.h>
#include <vex/kprintf.h>
#include <vex/types.h>

#define CONSF_NONE     0x0000U
#define CONSF_RECALL   0x0001U // Replay the kprintf history when added and when taking the device back
#define CONSF_LOGLEVEL 0x0002U // Filter by the console's own loglevel instead of the global one
#define CONSF_RELEASED 0x1000U // The device belongs to its driver and won't receive write calls

enum {
    CONSOLE_UNSPECIFIED, // There's no console= option, so drivers do as they would without one
    CONSOLE_LISTED,
    CONSOLE_UNLISTED,
};

typedef struct console_s console_t;
typedef struct console_ops_s console_ops_t;
typedef struct console_private_s console_private_t;

/// A console borrows its device until the first open of the device. Only a panic takes the device back
/// release and takeover can be nullptr if the device needs nothing
struct console_ops_s {
    void (*init)(console_t* restrict cons);

    /// Writes one line of output, ended with CR LF
    /// @pre Calls are serialized by the console core
    /// @note Takes no lock, as a panic in the middle of a write writes again
    void (*write)(console_t* restrict cons, const void* restrict data, size_t size);

    /// Finishes the output in flight and leaves the device alone from then on
    void (*release)(console_t* restrict cons);

    /// Brings the device back to the state the console writes in, whatever its driver left it in
    /// @pre Called by panic with interrupts disabled, so it can't take locks or allocate
    /// @retval false The device is unusable, and the console stays released
    bool (*takeover)(console_t* restrict cons);
};

struct console_private_s {
    console_t* next;
    kp_cursor_t cursor; // The next record to write
};

struct console_s {
    console_private_t priv;
    const console_ops_t* ops;
    const char* name;
    unsigned consflags;
    unsigned loglevel; // With CONSF_LOGLEVEL
    void* ctxdata;
};

int console_add(console_t* restrict cons);
int console_remove(console_t* restrict cons);

/// Writes to every console the records it hasn't written yet
/// @note Returns at once if another writer holds the consoles, on this hart or on another, as that writer writes the records
void console_flush(void);

/// Looks the device up in the console= options, one per console, such as console=serial1,115200n8 console=fbd0
/// @param options Gets what follows the first comma, or an empty string without one
/// @return One of CONSOLE_*
unsigned console_lookup(const char* restrict name, char* restrict options, size_t size);

/// Hands the console's device over to its driver, on the device's first open
void console_release(console_t* restrict cons);

/// Takes back every released device, replaying the kprintf history to consoles with CONSF_RECALL
/// @pre Called by panic, with interrupts disabled and the other harts stopped
void console_takeover_all(void);
