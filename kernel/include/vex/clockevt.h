#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#define CLOCK_EVENT_ONESHOT       0x01U
#define CLOCK_EVENT_PERIODIC      0x02U
#define CLOCK_EVENT_PER_HART      0x04U // One of a kind on every hart, which only that hart uses
#define CLOCK_EVENT_STOPS_IN_IDLE 0x08U // Says nothing in deep idle states, so another has to wake the hart

typedef struct clock_event_s clock_event_t;
typedef struct clock_event_ops_s clock_event_ops_t;
typedef struct clock_event_private_s clock_event_private_t;

struct clock_event_ops_s {
    /// Raises the interrupt once, after at least nsec
    /// @pre On the hart that uses the timer, with interrupts disabled
    /// @param irq The handler to run, numbered by irq_handler_add; a timer whose interrupt
    /// comes through an interrupt controller calls irq_dispatch with it from its own handler
    /// @note nsec is within the timer's min_delta_ns and max_delta_ns
    void (*set_oneshot)(clock_event_t* restrict event, unsigned irq, uint64_t nsec);

    /// Raises the interrupt every nsec until stop
    /// @pre On the hart that uses the timer, with interrupts disabled
    /// @param irq The handler to run, numbered by irq_handler_add; a timer whose interrupt
    /// comes through an interrupt controller calls irq_dispatch with it from its own handler
    /// @note nsec is within the timer's min_delta_ns and max_delta_ns
    void (*set_periodic)(clock_event_t* restrict event, unsigned irq, uint64_t nsec);

    /// @pre On the hart that uses the timer, or on any for a global one, with interrupts disabled
    void (*stop)(clock_event_t* restrict event);
};

struct clock_event_private_s {
    clock_event_t* next;
    unsigned hart_index; // The hart it serves
};

// A timer that raises an interrupt when asked; every hart uses the best one it may,
// its own or, on the boot hart that device interrupts go to, a global one
struct clock_event_s {
    clock_event_private_t priv;
    const clock_event_ops_t* ops; // set_oneshot or set_periodic may be nullptr as features say, never both
    const char* name;
    unsigned rating;
    unsigned features; // CLOCK_EVENT_*
    uint64_t min_delta_ns;
    uint64_t max_delta_ns;
    void* ctxdata;
};

/// @pre A per-hart timer is added on its hart
/// @retval -EEXIST The timer is already added
/// @retval -EINVAL The timer can raise neither a single interrupt nor periodic ones
int clock_event_add(clock_event_t* restrict event);

/// Takes a timer out, with the hart that used it going on with the best one left
/// @retval -ENOENT The timer isn't added
/// @retval -EBUSY A hart would be left without a timer, and the scheduler with it
/// @note A per-hart timer taken out from another hart may raise one more interrupt there
int clock_event_remove(clock_event_t* restrict event);

/// Raises irq on this hart once, after nsec or as close to it as the timer can
/// @pre Interrupts are disabled, and the hart has a timer
/// @note A timer that is only periodic raises it until clock_event_stop
void clock_event_oneshot(unsigned irq, uint64_t nsec);

/// Raises irq on this hart every nsec, or as close to it as the timer can
/// @pre Interrupts are disabled, and the hart has a timer
/// @note A timer without a periodic mode raises it once
void clock_event_periodic(unsigned irq, uint64_t nsec);

/// @pre Interrupts are disabled
void clock_event_stop(void);
