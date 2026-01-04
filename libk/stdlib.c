#include <vex/ctype.h>
#include <vex/stdlib.h>
#include <vex/string.h>
#include <vex/strings.h>

#include <limits.h>
#include <stdint.h>

#define IMPL_STRTOINT(name, type, max_value, min_value)                            \
    type name(const char* restrict nptr, const char** restrict endptr, int base)   \
    {                                                                              \
        int chr;                                                                   \
        int minus;                                                                 \
        int limdigit;                                                              \
        type limit;                                                                \
        type accum;                                                                \
        const char* cptr;                                                          \
        const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";              \
                                                                                   \
        do {                                                                       \
            chr = *nptr++;                                                         \
        } while(isspace(chr));                                                     \
                                                                                   \
        if(chr == '-') {                                                           \
            minus = 1;                                                             \
            chr = *nptr++;                                                         \
        }                                                                          \
        else {                                                                     \
            if(chr == '+') {                                                       \
                chr = *nptr++;                                                     \
            }                                                                      \
                                                                                   \
            minus = 0;                                                             \
        }                                                                          \
                                                                                   \
        if(chr == '0') {                                                           \
            if((base == 0 || base == 2) && (*nptr == 'B' || *nptr == 'b')) {       \
                base = 2;                                                          \
                chr = nptr[1];                                                     \
                nptr += 2;                                                         \
            }                                                                      \
            else if((base == 0 || base == 16) && (*nptr == 'X' || *nptr == 'x')) { \
                base = 16;                                                         \
                chr = nptr[1];                                                     \
                nptr += 2;                                                         \
            }                                                                      \
            else if(base == 0) {                                                   \
                base = 8;                                                          \
            }                                                                      \
        }                                                                          \
        else if(base == 0) {                                                       \
            base = 10;                                                             \
        }                                                                          \
                                                                                   \
        base = ((base < 2) ? 2 : base);                                            \
        base = ((base > 36) ? 36 : base);                                          \
        accum = 0;                                                                 \
                                                                                   \
        if(minus) {                                                                \
            limdigit = min_value % -base;                                          \
            limit = min_value / -base;                                             \
        }                                                                          \
        else {                                                                     \
            limdigit = max_value % base;                                           \
            limit = max_value / base;                                              \
        }                                                                          \
                                                                                   \
        while((cptr = memchr(digits, tolower(chr), base)) != NULL) {               \
            chr = cptr - digits;                                                   \
                                                                                   \
            if(accum < limit || (accum == limit && chr < limdigit)) {              \
                accum *= base;                                                     \
                accum += chr;                                                      \
                chr = *nptr++;                                                     \
            }                                                                      \
            else {                                                                 \
                do {                                                               \
                    chr = *++nptr;                                                 \
                } while(memchr(digits, tolower(chr), base));                       \
                                                                                   \
                if(endptr) {                                                       \
                    *endptr = nptr;                                                \
                }                                                                  \
                                                                                   \
                return minus ? min_value : max_value;                              \
            }                                                                      \
        }                                                                          \
                                                                                   \
        if(endptr) {                                                               \
            *endptr = nptr - 1;                                                    \
        }                                                                          \
                                                                                   \
        if(minus) {                                                                \
            return -accum;                                                         \
        }                                                                          \
                                                                                   \
        return accum;                                                              \
    }

IMPL_STRTOINT(strtol, long, LONG_MAX, LONG_MIN);
IMPL_STRTOINT(strtoll, long long, LLONG_MAX, LLONG_MIN);
IMPL_STRTOINT(strtoimax, intmax_t, INTMAX_MAX, INTMAX_MIN);

#define IMPL_STRTOUINT(name, type, max_value)                                      \
    type name(const char* restrict nptr, const char** restrict endptr, int base)   \
    {                                                                              \
        int chr;                                                                   \
        int minus;                                                                 \
        int limdigit;                                                              \
        type limit;                                                                \
        type accum;                                                                \
        const char* cptr;                                                          \
        const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";              \
                                                                                   \
        do {                                                                       \
            chr = *nptr++;                                                         \
        } while(isspace(chr));                                                     \
                                                                                   \
        if(chr == '-') {                                                           \
            minus = 1;                                                             \
            chr = *nptr++;                                                         \
        }                                                                          \
        else {                                                                     \
            if(chr == '+') {                                                       \
                chr = *nptr++;                                                     \
            }                                                                      \
                                                                                   \
            minus = 0;                                                             \
        }                                                                          \
                                                                                   \
        if(chr == '0') {                                                           \
            if((base == 0 || base == 2) && (*nptr == 'B' || *nptr == 'b')) {       \
                base = 2;                                                          \
                chr = nptr[1];                                                     \
                nptr += 2;                                                         \
            }                                                                      \
            else if((base == 0 || base == 16) && (*nptr == 'X' || *nptr == 'x')) { \
                base = 16;                                                         \
                chr = nptr[1];                                                     \
                nptr += 2;                                                         \
            }                                                                      \
            else if(base == 0) {                                                   \
                base = 8;                                                          \
            }                                                                      \
        }                                                                          \
        else if(base == 0) {                                                       \
            base = 10;                                                             \
        }                                                                          \
                                                                                   \
        base = ((base < 2) ? 2 : base);                                            \
        base = ((base > 36) ? 36 : base);                                          \
        limdigit = max_value % base;                                               \
        limit = max_value / base;                                                  \
        accum = 0;                                                                 \
                                                                                   \
        while((cptr = memchr(digits, tolower(chr), base)) != NULL) {               \
            chr = cptr - digits;                                                   \
                                                                                   \
            if(accum < limit || (accum == limit && chr < limdigit)) {              \
                accum *= base;                                                     \
                accum += chr;                                                      \
                chr = *nptr++;                                                     \
            }                                                                      \
            else {                                                                 \
                do {                                                               \
                    chr = *++nptr;                                                 \
                } while(memchr(digits, tolower(chr), base));                       \
                                                                                   \
                if(endptr) {                                                       \
                    *endptr = nptr;                                                \
                }                                                                  \
                                                                                   \
                return max_value;                                                  \
            }                                                                      \
        }                                                                          \
                                                                                   \
        if(endptr) {                                                               \
            *endptr = nptr - 1;                                                    \
        }                                                                          \
                                                                                   \
        if(minus) {                                                                \
            return -accum;                                                         \
        }                                                                          \
                                                                                   \
        return accum;                                                              \
    }

IMPL_STRTOUINT(strtoul, unsigned long, ULONG_MAX);
IMPL_STRTOUINT(strtoull, unsigned long long, ULLONG_MAX);
IMPL_STRTOUINT(strtoumax, uintmax_t, UINTMAX_MAX);
IMPL_STRTOUINT(strtousize, size_t, SIZE_MAX);
