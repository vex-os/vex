/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c), 2022, Kaneru Contributors */
#include <ctype.h>
#include "kcp.h"

int isalnum(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_UPRC | KCP_LWRC | KCP_DECD);
}

int isalpha(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_UPRC | KCP_LWRC);
}

int isascii(int c)
{
    return (c & 0x7F) == c;
}

int isblank(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_BLNK);
}

int iscntrl(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_CTRL);
}

int isdigit(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_DECD);
}

int isgraph(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_GRPH);
}

int islower(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_LWRC);
}

int isprint(int c)
{
    if(c == ' ')
        return 1;
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_CTRL);
}

int ispunct(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_PNCT);
}

int isspace(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_SPCE);
}

int isupper(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_UPRC);
}

int isxdigit(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_HEXD);
}

int toascii(int c)
{
    return (c & 0x7F);
}

int tolower(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].lower;
}

int toupper(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].upper;
}
