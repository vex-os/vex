#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

int format_size_iec(char* restrict buffer, size_t size, size_t value);
int format_size_si(char* restrict buffer, size_t size, size_t value);
