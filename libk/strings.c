#include <vex/ctype.h>
#include <vex/string.h>
#include <vex/strings.h>

char* kstrncat(char* restrict dst, const char* restrict src, size_t sz)
{
    size_t nc;
    char* save = dst;

    while(*dst && sz--) {
        dst++;
    }

    nc = strnlen(src, --sz);

    dst[nc] = 0;

    memcpy(dst, src, nc);

    return save;
}

char* kstrncpy(char* restrict dst, const char* restrict src, size_t sz)
{
    char* save = dst;

    while(*src && sz--) {
        *dst++ = *src++;
    }

    if(sz) {
        *dst = 0;
    }

    return save;
}

int strcasecmp(const char* restrict sa, const char* restrict sb)
{
    while(*sa && tolower(*sa) == tolower(*sb)) {
        sa++;
        sb++;
    }

    return *sa - *sb;
}

int strncasecmp(const char* restrict sa, const char* restrict sb, size_t sz)
{
    while(*sa && tolower(*sa) == tolower(*sb) && sz--) {
        sa++;
        sb++;
    }

    return *sa - *sb;
}
