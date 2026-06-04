#ifndef C1D6CD55_7B10_4B04_B0BB_64211D90DB93
#define C1D6CD55_7B10_4B04_B0BB_64211D90DB93
#pragma once

#include <core/cdefs.h>
#include <core/types.h>

// Standard sprintf implementations do not support human-readable
// size in bytes, nor do most standard syntax highlighters and even
// compilers (remember, we're compiling with -Werror), so the best
// way to have these sizes at hand is to just use a dedicated function
// that would make a human-readable size string at runtime

int hrsize_bin(char* restrict buffer, size_t sz, size_t value); // IEC 1024-base prefixes
int hrsize_dec(char* restrict buffer, size_t sz, size_t value); // SI 1000-base prefixes

#endif /* C1D6CD55_7B10_4B04_B0BB_64211D90DB93 */
