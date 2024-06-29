// SPDX-License-Identifier: Zlib
#include <string.h>

int strncmp(const char *restrict sa, const char *restrict sb, size_t sz)
{
    while(*sa && *sa == *sb && sz--) {
        sa++;
        sb++;
    }

    return *sa - *sb;
}
