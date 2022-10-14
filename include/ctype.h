/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_CTYPE_H__
#define __INCLUDE_CTYPE_H__
#include <sys/cdefs.h>

#define __CTYPE_NULL 0x000
#define __CTYPE_CTRL 0x001
#define __CTYPE_GRPH 0x002
#define __CTYPE_SPCE 0x004
#define __CTYPE_BLNK 0x008
#define __CTYPE_UPRC 0x010
#define __CTYPE_LWRC 0x020
#define __CTYPE_DECD 0x040
#define __CTYPE_HEXD 0x080
#define __CTYPE_PNCT 0x100

struct __libc_ctype {
    int lower, upper;
    unsigned int flags;
};

int isalnum(int c) __weak;
int isalpha(int c) __weak;
int isblank(int c) __weak;
int iscntrl(int c) __weak;
int isdigit(int c) __weak;
int isgraph(int c) __weak;
int islower(int c) __weak;
int isprint(int c) __weak;
int ispunct(int c) __weak;
int isspace(int c) __weak;
int isupper(int c) __weak;
int isxdigit(int c) __weak;

int tolower(int c) __weak;
int toupper(int c) __weak;

#endif /* __INCLUDE_CTYPE_H__ */
