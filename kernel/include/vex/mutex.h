#pragma once

#include <vex/compiler.h>
#include <vex/event.h>
#include <vex/spinlock.h>

typedef struct thread_s thread_t;

#define MUTEX_INIT { .lock = SPINLOCK_INIT }

typedef struct mutex_s mutex_t;

struct mutex_s {
    spinlock_t lock;
    uint32_t num_waiters;
    thread_t* owner;
    event_t released;
};

void mutex_init(mutex_t* restrict mtx);
void mutex_deinit(mutex_t* restrict mtx);

bool mutex_try_acquire(mutex_t* restrict mtx) __nodiscard;
void mutex_acquire(mutex_t* restrict mtx);
void mutex_release(mutex_t* restrict mtx);

bool mutex_held(const mutex_t* restrict mtx) __nodiscard;
