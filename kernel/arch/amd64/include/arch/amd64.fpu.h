#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

typedef struct thread_s thread_t;

#define FPU_STATE_ALIGN 64

#define FPU_EXCEPTION_IE UINT32_C(0x00000001) // Invalid operation
#define FPU_EXCEPTION_DE UINT32_C(0x00000002) // Denormal operand
#define FPU_EXCEPTION_ZE UINT32_C(0x00000004) // Divide by zero
#define FPU_EXCEPTION_OE UINT32_C(0x00000008) // Overflow
#define FPU_EXCEPTION_UE UINT32_C(0x00000010) // Underflow
#define FPU_EXCEPTION_PE UINT32_C(0x00000020) // Precision
#define FPU_EXCEPTIONS   UINT32_C(0x0000003F)

extern size_t fpu_state_size;

void fpu_save(void* restrict state);
void fpu_load(const void* restrict state);
void fpu_init_state(void* restrict state);
void fpu_reset(void);
bool fpu_state_valid(const void* restrict state) __nodiscard;
uint32_t fpu_unmasked_exceptions(bool simd) __nodiscard;

void* fpu_state_of(const thread_t* restrict thread) __nodiscard;
