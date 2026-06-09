#ifndef CB843259_FB1F_406D_AEC7_DD452B5E5899
#define CB843259_FB1F_406D_AEC7_DD452B5E5899
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

struct regfile {
    uint64_t amd64_rax;
    uint64_t amd64_rbx;
    uint64_t amd64_rcx;
    uint64_t amd64_rdx;
    uint64_t amd64_rsi;
    uint64_t amd64_rdi;
    uint64_t amd64_rbp;
    uint64_t amd64_r8;
    uint64_t amd64_r9;
    uint64_t amd64_r10;
    uint64_t amd64_r11;
    uint64_t amd64_r12;
    uint64_t amd64_r13;
    uint64_t amd64_r14;
    uint64_t amd64_r15;
    uint64_t amd64_err;
    uint64_t amd64_rip;
    uint64_t amd64_cs;
    uint64_t amd64_rflags;
    uint64_t amd64_rsp;
    uint64_t amd64_ss;
} __packed;

// Static assertions as per arch/amd64/kern/syscall.S
static_assert(0x00 == offsetof(struct regfile, amd64_rax));
static_assert(0x80 == offsetof(struct regfile, amd64_rip));
static_assert(0x90 == offsetof(struct regfile, amd64_rflags));
static_assert(0x98 == offsetof(struct regfile, amd64_rsp));

#define syscall_number amd64_rax
#define syscall_arg_0  amd64_rdi
#define syscall_arg_1  amd64_rsi
#define syscall_arg_2  amd64_rdx
#define syscall_arg_3  amd64_r10
#define syscall_arg_4  amd64_r8
#define syscall_arg_5  amd64_r9
#define syscall_retval amd64_rax

#endif /* CB843259_FB1F_406D_AEC7_DD452B5E5899 */
