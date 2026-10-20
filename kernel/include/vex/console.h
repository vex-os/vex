#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#define CONSF_NONE     0x0000U
#define CONSF_RECALL   0x0001U // Replay the kprintf history when added and when taking the device back
#define CONSF_RELEASED 0x1000U // The device belongs to its driver and won't receive write calls

typedef struct console_s console_t;
typedef struct console_ops_s console_ops_t;
typedef struct console_private_s console_private_t;

/// A console borrows its device until the device is first opened, and only a panic takes it back.
/// Either of release and takeover may be nullptr when the device needs nothing done
struct console_ops_s {
    void (*init)(console_t* restrict cons);

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
};

struct console_s {
    console_private_t priv;
    const console_ops_t* ops;
    const char* name;
    unsigned consflags;
    void* ctxdata;
};

int console_add(console_t* restrict cons);
int console_remove(console_t* restrict cons);
void console_write(const void* restrict data, size_t size);

/// Hands the console's device over to its driver, on the device's first open
void console_release(console_t* restrict cons);

/// Takes back every released device, replaying the kprintf history to consoles with CONSF_RECALL
/// @pre Called by panic, with interrupts disabled and the other harts stopped
void console_takeover_all(void);
