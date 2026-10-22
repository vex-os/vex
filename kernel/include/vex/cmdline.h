#pragma once

#include <vex/compiler.h>
#include <vex/types.h>

typedef struct cmdline_s cmdline_t;

struct cmdline_s {
    char* buffer;
    size_t length;
};

extern cmdline_t sys_cmdline;

// Takes ownership of buffer and splits it in place
int parse_cmdline(cmdline_t* restrict cmdl, char* restrict buffer, size_t length);

bool cmdline_contains(const cmdline_t* restrict cmdl, const char* restrict option) __nodiscard;
const char* cmdline_argument(const cmdline_t* restrict cmdl, const char* restrict option) __nodiscard;

/// Finds the next occurrence of an option that the command line may repeat, such as console=
/// @param prev The value that the previous call returned, or nullptr for the first occurrence
/// @return The value, an empty string for an option without one, or nullptr after the last occurrence
const char* cmdline_next_argument(const cmdline_t* restrict cmdl, const char* restrict option, const char* restrict prev) __nodiscard;
