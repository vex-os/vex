/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#include "ctype_private.h"

int isalnum(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CTYPE_UPRC | _CTYPE_LWRC | _CTYPE_DECD);
}

int isalpha(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CTYPE_UPRC | _CTYPE_LWRC);
}

int isascii(int c)
{
    return (c & 0x7F) == c;
}

int isblank(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CTYPE_BLNK);
}

int iscntrl(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CTYPE_CTRL);
}

int isdigit(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CTYPE_DECD);
}

int isgraph(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CTYPE_GRPH);
}

int islower(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CTYPE_LWRC);
}

int isprint(int c)
{
    if(c == ' ')
        return 1;
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CTYPE_CTRL);
}

int ispunct(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CTYPE_PNCT);
}

int isspace(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CTYPE_SPCE);
}

int isupper(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CTYPE_UPRC);
}

int isxdigit(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].flags & (_CTYPE_HEXD);
}

int toascii(int c)
{
    return (c & 0x7F);
}

int tolower(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].lower;
}

int toupper(int c)
{
    return _ctype[(c + 1) % CTYPE_SIZE].upper;
}
