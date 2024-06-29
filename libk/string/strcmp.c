// SPDX-License-Identifier: Zlib
#include <string.h>

int strcmp(const char *restrict sa, const char *restrict sb)
{
    while(*sa && *sa == *sb) {
        sa++;
        sb++;
    }

    return *sa - *sb;
}
