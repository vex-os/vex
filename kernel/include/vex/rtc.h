#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

int arch_rtc_read(time_t* restrict out) __nodiscard;
int arch_rtc_write(time_t time);
