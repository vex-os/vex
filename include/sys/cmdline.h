#ifndef B5BF702E_C52D_4754_891B_76D140A38305
#define B5BF702E_C52D_4754_891B_76D140A38305
#pragma once

#include <sys/cdefs.h>
#include <sys/types.h>

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

#endif /* B5BF702E_C52D_4754_891B_76D140A38305 */
