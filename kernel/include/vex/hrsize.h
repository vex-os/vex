#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

int hrsize_bin(char* restrict buffer, size_t sz, size_t value); // IEC 1024-base prefixes
int hrsize_dec(char* restrict buffer, size_t sz, size_t value); // SI 1000-base prefixes
