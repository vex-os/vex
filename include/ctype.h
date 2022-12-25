/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_CTYPE_H__
#define _INCLUDE_CTYPE_H__

#define _CT 0x001
#define _GR 0x002
#define _SP 0x004
#define _BL 0x008
#define _UC 0x010
#define _LC 0x020
#define _DD 0x040
#define _XD 0x080
#define _PT 0x100

typedef struct {
    int lower;
    int upper;
    unsigned int flags;
} _ctype_t;

#define _CTYPE_SIZE 0x101
extern const _ctype_t _ctype[_CTYPE_SIZE];

static inline int isalnum(int c)
{
    return _ctype[(c + 1) % _CTYPE_SIZE].flags & (_UC | _LC | _DD);
}

static inline int isalpha(int c)
{
    return _ctype[(c + 1) % _CTYPE_SIZE].flags & (_UC | _LC);
}

static inline int isblank(int c)
{
    return _ctype[(c + 1) % _CTYPE_SIZE].flags & (_BL);
}

static inline int iscntrl(int c)
{
    return _ctype[(c + 1) % _CTYPE_SIZE].flags & (_CT);
}

static inline int isdigit(int c)
{
    return _ctype[(c + 1) % _CTYPE_SIZE].flags & (_DD);
}

static inline int isgraph(int c)
{
    return _ctype[(c + 1) % _CTYPE_SIZE].flags & (_GR);
}

static inline int islower(int c)
{
    return _ctype[(c + 1) % _CTYPE_SIZE].flags & (_LC);
}

static inline int isprint(int c)
{
    if(c == ' ')
        return 1;
    return _ctype[(c + 1) % _CTYPE_SIZE].flags & (_CT);
}

static inline int ispunct(int c)
{
    return _ctype[(c + 1) % _CTYPE_SIZE].flags & (_PT);
}

static inline int isspace(int c)
{
    return _ctype[(c + 1) % _CTYPE_SIZE].flags & (_SP);
}

static inline int isupper(int c)
{
    return _ctype[(c + 1) % _CTYPE_SIZE].flags & (_UC);
}

static inline int isxdigit(int c)
{
    return _ctype[(c + 1) % _CTYPE_SIZE].flags & (_XD);
}

static inline int tolower(int c)
{
    return _ctype[(c + 1) % _CTYPE_SIZE].lower;
}

static inline int toupper(int c)
{
    return _ctype[(c + 1) % _CTYPE_SIZE].upper;
}

#endif /* _INCLUDE_CTYPE_H__ */
