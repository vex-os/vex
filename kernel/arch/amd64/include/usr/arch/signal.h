#ifndef USR_ARCH_SIGNAL_H
#define USR_ARCH_SIGNAL_H

#include <vex/types.h>

#define __k_MINSIGSTKSZ 8192
#define __k_SIGSTKSZ    16384

struct __k_mcontext {
    __k_uint64_t mc_rax;
    __k_uint64_t mc_rbx;
    __k_uint64_t mc_rcx;
    __k_uint64_t mc_rdx;
    __k_uint64_t mc_rsi;
    __k_uint64_t mc_rdi;
    __k_uint64_t mc_rbp;
    __k_uint64_t mc_rsp;
    __k_uint64_t mc_r8;
    __k_uint64_t mc_r9;
    __k_uint64_t mc_r10;
    __k_uint64_t mc_r11;
    __k_uint64_t mc_r12;
    __k_uint64_t mc_r13;
    __k_uint64_t mc_r14;
    __k_uint64_t mc_r15;
    __k_uint64_t mc_rip;
    __k_uint64_t mc_rflags;
    void* mc_fpu_state; /* XSAVE or FXSAVE image, 64-byte aligned */
    __k_uint32_t mc_fpu_size;
    __k_uint32_t mc_reserved0;
    __k_uint64_t mc_reserved[4];
};

#endif /* USR_ARCH_SIGNAL_H */
