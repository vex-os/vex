/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include "ctype_ascii.h"

int isalnum(int c)
{
    return __ctype_ascii[(c + 1) % CTYPE_SIZE].flags & (__CTYPE_UPRC | __CTYPE_LWRC | __CTYPE_DECD);
}

int isalpha(int c)
{
    return __ctype_ascii[(c + 1) % CTYPE_SIZE].flags & (__CTYPE_UPRC | __CTYPE_LWRC);
}

int isblank(int c)
{
    return __ctype_ascii[(c + 1) % CTYPE_SIZE].flags & (__CTYPE_BLNK);
}

int iscntrl(int c)
{
    return __ctype_ascii[(c + 1) % CTYPE_SIZE].flags & (__CTYPE_CTRL);
}

int isdigit(int c)
{
    return __ctype_ascii[(c + 1) % CTYPE_SIZE].flags & (__CTYPE_DECD);
}

int isgraph(int c)
{
    return __ctype_ascii[(c + 1) % CTYPE_SIZE].flags & (__CTYPE_GRPH);
}

int islower(int c)
{
    return __ctype_ascii[(c + 1) % CTYPE_SIZE].flags & (__CTYPE_LWRC);
}

int isprint(int c)
{
    if(c == ' ')
        return 1;
    return __ctype_ascii[(c + 1) % CTYPE_SIZE].flags & (__CTYPE_CTRL);
}

int ispunct(int c)
{
    return __ctype_ascii[(c + 1) % CTYPE_SIZE].flags & (__CTYPE_PNCT);
}

int isspace(int c)
{
    return __ctype_ascii[(c + 1) % CTYPE_SIZE].flags & (__CTYPE_SPCE);
}

int isupper(int c)
{
    return __ctype_ascii[(c + 1) % CTYPE_SIZE].flags & (__CTYPE_UPRC);
}

int isxdigit(int c)
{
    return __ctype_ascii[(c + 1) % CTYPE_SIZE].flags & (__CTYPE_HEXD);
}

int tolower(int c)
{
    return __ctype_ascii[(c + 1) % CTYPE_SIZE].lower;
}

int toupper(int c)
{
    return __ctype_ascii[(c + 1) % CTYPE_SIZE].upper;
}
