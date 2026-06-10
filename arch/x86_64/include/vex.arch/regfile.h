#ifndef CB843259_FB1F_406D_AEC7_DD452B5E5899
#define CB843259_FB1F_406D_AEC7_DD452B5E5899
#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

struct regfile {
    uint64_t x86_rax;
    uint64_t x86_rbx;
    uint64_t x86_rcx;
    uint64_t x86_rdx;
    uint64_t x86_rsi;
    uint64_t x86_rdi;
    uint64_t x86_rbp;
    uint64_t x86_r8;
    uint64_t x86_r9;
    uint64_t x86_r10;
    uint64_t x86_r11;
    uint64_t x86_r12;
    uint64_t x86_r13;
    uint64_t x86_r14;
    uint64_t x86_r15;
    uint64_t x86_err;
    uint64_t x86_rip;
    uint64_t x86_cs;
    uint64_t x86_rflags;
    uint64_t x86_rsp;
    uint64_t x86_ss;
} __packed;

// Static assertions as per arch/x86_64/kernel/syscall.S
static_assert(0x00 == offsetof(struct regfile, x86_rax));
static_assert(0x80 == offsetof(struct regfile, x86_rip));
static_assert(0x90 == offsetof(struct regfile, x86_rflags));
static_assert(0x98 == offsetof(struct regfile, x86_rsp));

#define syscall_number x86_rax
#define syscall_arg_0  x86_rdi
#define syscall_arg_1  x86_rsi
#define syscall_arg_2  x86_rdx
#define syscall_arg_3  x86_r10
#define syscall_arg_4  x86_r8
#define syscall_arg_5  x86_r9
#define syscall_retval x86_rax

#define regfile_pc    x86_rip
#define regfile_sp    x86_rsp
#define regfile_arg_0 x86_rdi
#define regfile_arg_1 x86_rsi
#define regfile_arg_2 x86_rdx

#endif /* CB843259_FB1F_406D_AEC7_DD452B5E5899 */
