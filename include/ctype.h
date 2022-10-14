/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_CTYPE_H__
#define __INCLUDE_CTYPE_H__
#include <sys/cdefs.h>

#define _CT_NULL 0x000
#define _CT_CTRL 0x001
#define _CT_GRPH 0x002
#define _CT_SPCE 0x004
#define _CT_BLNK 0x008
#define _CT_UPRC 0x010
#define _CT_LWRC 0x020
#define _CT_DECD 0x040
#define _CT_HEXD 0x080
#define _CT_PNCT 0x100

struct ctype {
    int lower, upper;
    unsigned int flags;
};

#define CTYPE_SIZE 0x101 /* 256 plus EOF */
extern const struct ctype _ctype[CTYPE_SIZE];

static inline int isalnum(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CT_UPRC | _CT_LWRC | _CT_DECD);
}

static inline int isalpha(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CT_UPRC | _CT_LWRC);
}

static inline int isblank(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CT_BLNK);
}

static inline int iscntrl(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CT_CTRL);
}

static inline int isdigit(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CT_DECD);
}

static inline int isgraph(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CT_GRPH);
}

static inline int islower(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CT_LWRC);
}

static inline int isprint(int c)
{
    if(c == ' ')
        return 1;
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CT_CTRL);
}

static inline int ispunct(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CT_PNCT);
}

static inline int isspace(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CT_SPCE);
}

static inline int isupper(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CT_UPRC);
}

static inline int isxdigit(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CT_HEXD);
}

static inline int tolower(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].lower;
}

static inline int toupper(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].upper;
}

#endif /* __INCLUDE_CTYPE_H__ */
