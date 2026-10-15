#pragma once

#include <vex/compiler.h>

struct hart_context;

extern void sched_enter(struct hart_context* restrict ctx) __noreturn;
