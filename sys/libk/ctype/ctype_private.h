/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __LIBK_CTYPE_CTYPE_PRIVATE_H__
#define __LIBK_CTYPE_CTYPE_PRIVATE_H__
#include <ctype.h>

#define _CTYPE_NULL 0x000
#define _CTYPE_CTRL 0x001
#define _CTYPE_GRPH 0x002
#define _CTYPE_SPCE 0x004
#define _CTYPE_BLNK 0x008
#define _CTYPE_UPRC 0x010
#define _CTYPE_LWRC 0x020
#define _CTYPE_DECD 0x040
#define _CTYPE_HEXD 0x080
#define _CTYPE_PNCT 0x100

struct ctype {
    int lower, upper;
    unsigned int flags;
};

#define CTYPE_SIZE 0x101 /* 256 plus EOF */
extern const struct ctype _ctype[CTYPE_SIZE];

#endif /* __LIBK_CTYPE_CTYPE_PRIVATE_H__ */
