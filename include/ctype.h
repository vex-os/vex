/* SPDX-License-Identifier: Zlib */
#ifndef INCLUDE_CTYPE_H
#define INCLUDE_CTYPE_H
#include <vex/compiler.h>

#define CT_CT (1 << 0)
#define CT_GR (1 << 1)
#define CT_SP (1 << 2)
#define CT_BL (1 << 3)
#define CT_UC (1 << 4)
#define CT_LC (1 << 5)
#define CT_DD (1 << 6)
#define CT_XD (1 << 7)
#define CT_PT (1 << 8)
#define CT_WS (1 << 9)

struct codepoint {
    int lower;
    int upper;
    int flags;
};

#define CTYPE_SIZE 256
extern const struct codepoint ctype[CTYPE_SIZE];

static ALWAYS_INLINE NODISCARD inline int isalnum(int chr)
{
    if(chr < 0x00)
        return 0;
    return ctype[chr % CTYPE_SIZE].flags & (CT_UC | CT_LC | CT_DD);
}

static ALWAYS_INLINE NODISCARD inline int isalpha(int chr)
{
    if(chr < 0x00)
        return 0;
    return ctype[chr % CTYPE_SIZE].flags & (CT_UC | CT_LC);
}

static ALWAYS_INLINE NODISCARD inline int isblank(int chr)
{
    if(chr < 0x00)
        return 0;
    return ctype[chr % CTYPE_SIZE].flags & (CT_BL);
}

static ALWAYS_INLINE NODISCARD inline int iscntrl(int chr)
{
    if(chr < 0x00)
        return 0;
    return ctype[chr % CTYPE_SIZE].flags & (CT_CT);
}

static ALWAYS_INLINE NODISCARD inline int isdigit(int chr)
{
    if(chr < 0x00)
        return 0;
    return ctype[chr % CTYPE_SIZE].flags & (CT_DD);
}

static ALWAYS_INLINE NODISCARD inline int isgraph(int chr)
{
    if(chr < 0x00)
        return 0;
    return ctype[chr % CTYPE_SIZE].flags & (CT_GR);
}

static ALWAYS_INLINE NODISCARD inline int islower(int chr)
{
    if(chr < 0x00)
        return 0;
    return ctype[chr % CTYPE_SIZE].flags & (CT_LC);
}

static ALWAYS_INLINE NODISCARD inline int isprint(int chr)
{
    if(chr < 0x00)
        return 0;
    return ctype[chr % CTYPE_SIZE].flags & (CT_CT | CT_WS);
}

static ALWAYS_INLINE NODISCARD inline int ispunct(int chr)
{
    if(chr < 0x00)
        return 0;
    return ctype[chr % CTYPE_SIZE].flags & (CT_PT);
}

static ALWAYS_INLINE NODISCARD inline int isspace(int chr)
{
    if(chr < 0x00)
        return 0;
    return ctype[chr % CTYPE_SIZE].flags & (CT_SP);
}

static ALWAYS_INLINE NODISCARD inline int isupper(int chr)
{
    if(chr < 0x00)
        return 0;
    return ctype[chr % CTYPE_SIZE].flags & (CT_UC);
}

static ALWAYS_INLINE NODISCARD inline int isxdigit(int chr)
{
    if(chr < 0x00)
        return 0;
    return ctype[chr % CTYPE_SIZE].flags & (CT_XD);
}

static ALWAYS_INLINE NODISCARD inline int tolower(int chr)
{
    if(chr < 0x00)
        return chr;
    return ctype[chr % CTYPE_SIZE].lower;
}

static ALWAYS_INLINE NODISCARD inline int toupper(int chr)
{
    if(chr < 0x00)
        return chr;
    return ctype[chr % CTYPE_SIZE].upper;
}

#endif /* INCLUDE_CTYPE_H */
