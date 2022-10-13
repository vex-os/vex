/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_CTYPE_H__
#define __INCLUDE_CTYPE_H__
#include <sys/cdefs.h>

int isalnum(int c) __weak;
int isalpha(int c) __weak;
int isascii(int c) __weak;
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
int toascii(int c) __weak;
int tolower(int c) __weak;
int toupper(int c) __weak;

#endif /* __INCLUDE_CTYPE_H__ */
