#include <arch/amd64.cpuid.h>
#include <arch/amd64.fpu.h>
#include <vex/compiler.h>
#include <vex/cpucaps.h>
#include <vex/initcall.h>
#include <vex/kstack.h>
#include <vex/page.h>
#include <vex/panic.h>
#include <vex/string.h>

#define CR0_MP UINT64_C(0x0000000000000002)
#define CR0_EM UINT64_C(0x0000000000000004)
#define CR0_TS UINT64_C(0x0000000000000008)
#define CR0_NE UINT64_C(0x0000000000000020)

#define CR4_OSFXSR     UINT64_C(0x0000000000000200)
#define CR4_OSXMMEXCPT UINT64_C(0x0000000000000400)
#define CR4_OSXSAVE    UINT64_C(0x0000000000040000)

#define XFEATURE_X87    UINT64_C(0x0000000000000001)
#define XFEATURE_SSE    UINT64_C(0x0000000000000002)
#define XFEATURE_AVX    UINT64_C(0x0000000000000004)
#define XFEATURE_AVX512 UINT64_C(0x00000000000000E0)

#define FXSAVE_SIZE              512
#define FXSAVE_FCW_OFFSET        0
#define FXSAVE_MXCSR_OFFSET      24
#define FXSAVE_MXCSR_MASK_OFFSET 28

#define XSAVE_HEADER_OFFSET 512
#define XSAVE_HEADER_SIZE   64

#define FPU_INIT_FCW   UINT16_C(0x037F)
#define FPU_INIT_MXCSR UINT32_C(0x1F80)

#define FPU_DEFAULT_MXCSR_MASK UINT32_C(0x0000FFBF) // What FXSAVE reports when the processor leaves the mask out

size_t fpu_state_size = 0;

static uint64_t xfeatures = 0;
static uint32_t mxcsr_mask = FPU_DEFAULT_MXCSR_MASK;

static uint8_t init_state[PAGE_SIZE] __aligned(FPU_STATE_ALIGN);

static __always_inline void xsetbv(uint32_t index, uint64_t value)
{
    asm volatile("xsetbv" ::"c"(index), "a"((uint32_t)(value)), "d"((uint32_t)(value >> 32)) : "memory");
}

static void detect_xfeatures(void)
{
    if(cpu_has(CPU_FEATURE_XSAVE)) {
        xfeatures = XFEATURE_X87 | XFEATURE_SSE;

        if(cpu_has(CPU_FEATURE_AVX)) {
            xfeatures |= XFEATURE_AVX;
        }

        if(cpu_has(CPU_FEATURE_AVX512)) {
            xfeatures |= XFEATURE_AVX512;
        }
    }
}

static void program_fpu(void)
{
    uint64_t cr0;
    asm volatile("movq %%cr0, %0" : "=r"(cr0));
    cr0 &= ~(CR0_EM | CR0_TS);
    cr0 |= CR0_MP | CR0_NE;
    asm volatile("movq %0, %%cr0" ::"r"(cr0) : "memory");

    uint64_t cr4;
    asm volatile("movq %%cr4, %0" : "=r"(cr4));
    cr4 |= CR4_OSFXSR | CR4_OSXMMEXCPT;

    if(xfeatures) {
        cr4 |= CR4_OSXSAVE;
    }

    asm volatile("movq %0, %%cr4" ::"r"(cr4) : "memory");

    if(xfeatures) {
        xsetbv(0, xfeatures);
    }

    asm volatile("fninit");
}

void fpu_save(void* restrict state)
{
    if(xfeatures) {
        asm volatile("xsave64 (%0)" ::"r"(state), "a"(UINT32_MAX), "d"(UINT32_MAX) : "memory");
    }
    else {
        asm volatile("fxsave64 (%0)" ::"r"(state) : "memory");
    }
}

void fpu_load(const void* restrict state)
{
    if(xfeatures) {
        asm volatile("xrstor64 (%0)" ::"r"(state), "a"(UINT32_MAX), "d"(UINT32_MAX) : "memory");
    }
    else {
        asm volatile("fxrstor64 (%0)" ::"r"(state) : "memory");
    }
}

void fpu_init_state(void* restrict state)
{
    memcpy(state, init_state, fpu_state_size);
}

void fpu_reset(void)
{
    fpu_load(init_state);
}

bool fpu_state_valid(const void* restrict state)
{
    const uint8_t* bytes = state;

    uint32_t mxcsr;
    memcpy(&mxcsr, &bytes[FXSAVE_MXCSR_OFFSET], sizeof(mxcsr));

    if(mxcsr & ~mxcsr_mask) {
        return false;
    }

    if(xfeatures == 0) {
        return true;
    }

    uint64_t header[XSAVE_HEADER_SIZE / sizeof(uint64_t)];
    memcpy(header, &bytes[XSAVE_HEADER_OFFSET], sizeof(header));

    // XSTATE_BV within the enabled features, XCOMP_BV zero for the
    // standard format, and the reserved rest of the header zero too
    if(header[0] & ~xfeatures) {
        return false;
    }

    for(size_t i = 1; i < XSAVE_HEADER_SIZE / sizeof(uint64_t); ++i) {
        if(header[i]) {
            return false;
        }
    }

    return true;
}

static void arch_init_fpu_bsp(void)
{
    detect_xfeatures();
    program_fpu();

    fpu_state_size = FXSAVE_SIZE;

    if(xfeatures) {
        uint32_t eax, ebx, ecx, edx;
        cpuid(0x0D, 0, &eax, &ebx, &ecx, &edx);
        fpu_state_size = ebx;
    }

    if(fpu_state_size > sizeof(init_state) || fpu_state_size > KSTACK_SIZE / 2U) {
        panic("fpu: %zu bytes of state do not fit", fpu_state_size);
    }

    static uint8_t probe[FXSAVE_SIZE] __aligned(FPU_STATE_ALIGN);
    asm volatile("fxsave64 (%0)" ::"r"(probe) : "memory");
    memcpy(&mxcsr_mask, &probe[FXSAVE_MXCSR_MASK_OFFSET], sizeof(mxcsr_mask));

    if(mxcsr_mask == 0) {
        mxcsr_mask = FPU_DEFAULT_MXCSR_MASK;
    }

    uint16_t fcw = FPU_INIT_FCW;
    uint32_t mxcsr = FPU_INIT_MXCSR;
    memcpy(&init_state[FXSAVE_FCW_OFFSET], &fcw, sizeof(fcw));
    memcpy(&init_state[FXSAVE_MXCSR_OFFSET], &mxcsr, sizeof(mxcsr));
}

static void arch_init_fpu(void)
{
    program_fpu();
}

initcall_bsp_arch(arch_init_fpu_bsp, INIT_MIDDLE);
initcall_smp_arch(arch_init_fpu, INIT_MIDDLE);
