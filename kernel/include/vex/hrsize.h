#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

int format_size_iec(char* restrict buffer, size_t size, size_t value); // IEC 1024-base prefixes
int format_size_si(char* restrict buffer, size_t size, size_t value);  // SI 1000-base prefixes
