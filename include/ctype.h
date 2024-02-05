/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#ifndef INCLUDE_CTYPE_H
#define INCLUDE_CTYPE_H
#include <sys/cdefs.h>

#define CTYPE_CT (1 << 0)
#define CTYPE_GR (1 << 1)
#define CTYPE_SP (1 << 2)
#define CTYPE_BL (1 << 3)
#define CTYPE_UC (1 << 4)
#define CTYPE_LC (1 << 5)
#define CTYPE_DD (1 << 6)
#define CTYPE_XD (1 << 7)
#define CTYPE_PT (1 << 8)

struct ctype_ent {
    int lower;
    int upper;
    int flags;
};

#define CTYPE_SIZE 256
extern const struct ctype_ent ctype[CTYPE_SIZE];

static __always_inline inline int isalnum(int c)
{
    if(c < 0)
        return 0;
    return ctype[c % CTYPE_SIZE].flags & (CTYPE_UC | CTYPE_LC | CTYPE_DD);
}

static __always_inline inline int isalpha(int c)
{
    if(c < 0)
        return 0;
    return ctype[c % CTYPE_SIZE].flags & (CTYPE_UC | CTYPE_LC);
}

static __always_inline inline int isblank(int c)
{
    if(c < 0)
        return 0;
    return ctype[c % CTYPE_SIZE].flags & (CTYPE_BL);
}

static __always_inline inline int iscntrl(int c)
{
    if(c < 0)
        return 0;
    return ctype[c % CTYPE_SIZE].flags & (CTYPE_CT);
}

static __always_inline inline int isdigit(int c)
{
    if(c < 0)
        return 0;
    return ctype[c % CTYPE_SIZE].flags & (CTYPE_DD);
}

static __always_inline inline int isgraph(int c)
{
    if(c < 0)
        return 0;
    return ctype[c % CTYPE_SIZE].flags & (CTYPE_GR);
}

static __always_inline inline int islower(int c)
{
    if(c < 0)
        return 0;
    return ctype[c % CTYPE_SIZE].flags & (CTYPE_LC);
}

static __always_inline inline int isprint(int c)
{
    if(c < 0)
        return 0;
    if(c == ' ')
        return 1;
    return ctype[c % CTYPE_SIZE].flags & (CTYPE_CT);
}

static __always_inline inline int ispunct(int c)
{
    if(c < 0)
        return 0;
    return ctype[c % CTYPE_SIZE].flags & (CTYPE_PT);
}

static __always_inline inline int isspace(int c)
{
    if(c < 0)
        return 0;
    return ctype[c % CTYPE_SIZE].flags & (CTYPE_SP);
}

static __always_inline inline int isupper(int c)
{
    if(c < 0)
        return 0;
    return ctype[c % CTYPE_SIZE].flags & (CTYPE_UC);
}

static __always_inline inline int isxdigit(int c)
{
    if(c < 0)
        return 0;
    return ctype[c % CTYPE_SIZE].flags & (CTYPE_XD);
}

static __always_inline inline int tolower(int c)
{
    if(c < 0)
        return c;
    return ctype[c % CTYPE_SIZE].lower;
}

static __always_inline inline int toupper(int c)
{
    if(c < 0)
        return c;
    return ctype[c % CTYPE_SIZE].upper;
}

#endif /* INCLUDE_CTYPE_H */
