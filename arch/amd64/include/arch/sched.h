#ifndef FFDED8C1_DEB6_4CC0_8403_D765FAABCD58
#define FFDED8C1_DEB6_4CC0_8403_D765FAABCD58
#pragma once

#include <vex/compiler.h>

struct hart_context;

extern void sched_enter(struct hart_context* restrict ctx) __noreturn;

#endif /* FFDED8C1_DEB6_4CC0_8403_D765FAABCD58 */
