#pragma once

#include <vex/compiler.h>
#include <vex/event.h>
#include <vex/spinlock.h>

struct thread;

#define MUTEX_INIT { .lock = SPINLOCK_INIT }

struct mutex {
    spinlock_type lock;
    uint32_t num_waiters;
    struct thread* owner;
    struct event released;
};

void mutex_init(struct mutex* restrict mtx);
void mutex_deinit(struct mutex* restrict mtx);

bool mutex_try_acquire(struct mutex* restrict mtx) __nodiscard;
void mutex_acquire(struct mutex* restrict mtx);
void mutex_release(struct mutex* restrict mtx);

bool mutex_held(const struct mutex* restrict mtx) __nodiscard;
