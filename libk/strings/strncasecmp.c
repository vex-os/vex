// SPDX-License-Identifier: BSD-2-Clause
#include <ctype.h>
#include <strings.h>

int strncasecmp(const char* restrict sa, const char* restrict sb, size_t sz)
{
    while(*sa && tolower(*sa) == tolower(*sb) && sz--) {
        sa++;
        sb++;
    }

    return *sa - *sb;
}
