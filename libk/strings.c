#include <ctype.h>
#include <string.h>
#include <strings.h>

char* kstrncat(char* restrict dst, const char* restrict src, size_t sz)
{
    char* save = dst;

    while(*dst && sz--) {
        dst++;
    }

    size_t nc = strnlen(src, --sz);

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

    return tolower((unsigned char)*sa) - tolower((unsigned char)*sb);
}

int strncasecmp(const char* restrict sa, const char* restrict sb, size_t sz)
{
    while(sz-- && *sa && tolower(*sa) == tolower(*sb)) {
        sa++;
        sb++;
    }

    if(sz == SIZE_MAX) {
        return 0;
    }

    return tolower(*sa) - tolower(*sb);
}
