#ifndef C35706EE_41EE_4119_8553_0E2ADC7CC54A
#define C35706EE_41EE_4119_8553_0E2ADC7CC54A
#pragma once

#include <usr/vex.arch/mctx.h>
#include <vex.arch/regfile.h>
#include <vex/compiler.h>
#include <vex/types.h>

#define k_mcontext __k_mcontext

static __always_inline void regfile_to_mcontext(const struct regfile* restrict regs, struct __k_mcontext* restrict mc)
{
    mc->mc_rax = regs->x86_rax;
    mc->mc_rbx = regs->x86_rbx;
    mc->mc_rcx = regs->x86_rcx;
    mc->mc_rdx = regs->x86_rdx;
    mc->mc_rsi = regs->x86_rsi;
    mc->mc_rdi = regs->x86_rdi;
    mc->mc_rbp = regs->x86_rbp;
    mc->mc_r8 = regs->x86_r8;
    mc->mc_r9 = regs->x86_r9;
    mc->mc_r10 = regs->x86_r10;
    mc->mc_r11 = regs->x86_r11;
    mc->mc_r12 = regs->x86_r12;
    mc->mc_r13 = regs->x86_r13;
    mc->mc_r14 = regs->x86_r14;
    mc->mc_r15 = regs->x86_r15;
    mc->mc_rip = regs->x86_rip;
    mc->mc_rflags = regs->x86_rflags;
    mc->mc_rsp = regs->x86_rsp;
}

static __always_inline void mcontext_to_regfile(const struct __k_mcontext* restrict mc, struct regfile* restrict regs)
{
    regs->x86_rax = mc->mc_rax;
    regs->x86_rbx = mc->mc_rbx;
    regs->x86_rcx = mc->mc_rcx;
    regs->x86_rdx = mc->mc_rdx;
    regs->x86_rsi = mc->mc_rsi;
    regs->x86_rdi = mc->mc_rdi;
    regs->x86_rbp = mc->mc_rbp;
    regs->x86_r8 = mc->mc_r8;
    regs->x86_r9 = mc->mc_r9;
    regs->x86_r10 = mc->mc_r10;
    regs->x86_r11 = mc->mc_r11;
    regs->x86_r12 = mc->mc_r12;
    regs->x86_r13 = mc->mc_r13;
    regs->x86_r14 = mc->mc_r14;
    regs->x86_r15 = mc->mc_r15;
    regs->x86_rip = mc->mc_rip;
    regs->x86_rflags = mc->mc_rflags;
    regs->x86_rsp = mc->mc_rsp;
}

#endif /* C35706EE_41EE_4119_8553_0E2ADC7CC54A */
