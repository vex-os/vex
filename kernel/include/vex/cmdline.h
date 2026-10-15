#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

struct cmdline {
    char* buffer;
    size_t length;
};

extern struct cmdline sys_cmdline;

// Parses a raw command line string into a structured cmdline object;
// the buffer is NOT copied and instead cmdline assumes ownership of it
// and writes directly into it, replacing spaces with null terminators
int parse_cmdline(struct cmdline* restrict cmdl, char* restrict buffer, size_t length);

bool cmdline_contains(const struct cmdline* restrict cmdl, const char* restrict option) __nodiscard;
const char* cmdline_argument(const struct cmdline* restrict cmdl, const char* restrict option) __nodiscard;
