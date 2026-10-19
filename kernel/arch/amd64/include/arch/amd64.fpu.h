#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

#define FPU_STATE_ALIGN 64

extern size_t fpu_state_size;

void fpu_save(void* restrict state);
void fpu_load(const void* restrict state);
void fpu_init_state(void* restrict state);
void fpu_reset(void);
bool fpu_state_valid(const void* restrict state) __nodiscard;

struct thread;
void* fpu_state_of(const struct thread* restrict thrd) __nodiscard;
