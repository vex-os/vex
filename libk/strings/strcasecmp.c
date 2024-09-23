/* SPDX-License-Identifier: BSD-2-Clause */
#include <ctype.h>
#include <strings.h>

int strcasecmp(const char *restrict sa, const char *restrict sb)
{
    while(*sa && tolower(*sa) == tolower(*sb)) {
        sa++;
        sb++;
    }

    return *sa - *sb;
}
