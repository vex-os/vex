#ifndef C35706EE_41EE_4119_8553_0E2ADC7CC54A
#define C35706EE_41EE_4119_8553_0E2ADC7CC54A
#pragma once

#include <uapi/vex.arch/mctx.h>
#include <vex.arch/regs.h>
#include <vex/compiler.h>
#include <vex/types.h>

#define k_mcontext __k_mcontext

static __always_inline void regfile_to_mcontext(const struct regfile* restrict regs, struct __k_mcontext* restrict mc)
{
    mc->mc_rax = regs->amd64_rax;
    mc->mc_rbx = regs->amd64_rbx;
    mc->mc_rcx = regs->amd64_rcx;
    mc->mc_rdx = regs->amd64_rdx;
    mc->mc_rsi = regs->amd64_rsi;
    mc->mc_rdi = regs->amd64_rdi;
    mc->mc_rbp = regs->amd64_rbp;
    mc->mc_r8 = regs->amd64_r8;
    mc->mc_r9 = regs->amd64_r9;
    mc->mc_r10 = regs->amd64_r10;
    mc->mc_r11 = regs->amd64_r11;
    mc->mc_r12 = regs->amd64_r12;
    mc->mc_r13 = regs->amd64_r13;
    mc->mc_r14 = regs->amd64_r14;
    mc->mc_r15 = regs->amd64_r15;
    mc->mc_rip = regs->amd64_rip;
    mc->mc_rflags = regs->amd64_rflags;
    mc->mc_rsp = regs->amd64_rsp;
}

static __always_inline void mcontext_to_regfile(const struct __k_mcontext* restrict mc, struct regfile* restrict regs)
{
    regs->amd64_rax = mc->mc_rax;
    regs->amd64_rbx = mc->mc_rbx;
    regs->amd64_rcx = mc->mc_rcx;
    regs->amd64_rdx = mc->mc_rdx;
    regs->amd64_rsi = mc->mc_rsi;
    regs->amd64_rdi = mc->mc_rdi;
    regs->amd64_rbp = mc->mc_rbp;
    regs->amd64_r8 = mc->mc_r8;
    regs->amd64_r9 = mc->mc_r9;
    regs->amd64_r10 = mc->mc_r10;
    regs->amd64_r11 = mc->mc_r11;
    regs->amd64_r12 = mc->mc_r12;
    regs->amd64_r13 = mc->mc_r13;
    regs->amd64_r14 = mc->mc_r14;
    regs->amd64_r15 = mc->mc_r15;
    regs->amd64_rip = mc->mc_rip;
    regs->amd64_rflags = mc->mc_rflags;
    regs->amd64_rsp = mc->mc_rsp;
}

#endif /* C35706EE_41EE_4119_8553_0E2ADC7CC54A */
