/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_CTYPE_H__
#define __INCLUDE_CTYPE_H__
#include <kaneru/cdefs.h>

#define KCP_NULL 0x000
#define KCP_CTRL 0x001
#define KCP_GRPH 0x002
#define KCP_SPCE 0x004
#define KCP_BLNK 0x008
#define KCP_UPRC 0x010
#define KCP_LWRC 0x020
#define KCP_DECD 0x040
#define KCP_HEXD 0x080
#define KCP_PNCT 0x100

struct kcp {
    int lower, upper;
    unsigned int flags;
};

#define KCP_SIZE 0x101 /* 256 plus EOF */
extern const struct kcp _kcp[KCP_SIZE];

/* is it an alphanumeric character? */
static inline int isalnum(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_UPRC | KCP_LWRC | KCP_DECD);
}

/* is it an alphabetic character? */
static inline int isalpha(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_UPRC | KCP_LWRC);
}

/* is the character within ASCII range? */
static inline int isascii(int c)
{
    return (c & 0x7F) == c;
}

/* is it an invisible character? */
static inline int isblank(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_BLNK);
}

/* is it a control character? */
static inline int iscntrl(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_CTRL);
}

/* is it a decimal character? */
static inline int isdigit(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_DECD);
}

/* is it a graphical character? */
static inline int isgraph(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_GRPH);
}

/* is it a lowercase character? */
static inline int islower(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_LWRC);
}

/* is it a printable character? */
static inline int isprint(int c)
{
    if(c == ' ')
        return 1;
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_CTRL);
}

/* is it a punctuation character? */
static inline int ispunct(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_PNCT);
}

/* is it a whitespace character? */
static inline int isspace(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_SPCE);
}

/* is it an uppercase character? */
static inline int isupper(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_UPRC);
}

/* is it hexadecimal character? */
static inline int isxdigit(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].flags & (KCP_HEXD);
}

/* force the character to ASCII range */
static inline int toascii(int c)
{
    return (c & 0x7F);
}

/* convert the character to lowercase */
static inline int tolower(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].lower;
}

/* convert the character to uppercase */
static inline int toupper(int c)
{
    return _kcp[(c + 1) % KCP_SIZE].upper;
}

#endif /* __INCLUDE_CTYPE_H__ */
