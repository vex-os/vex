/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_CTYPE_H__
#define __INCLUDE_CTYPE_H__
#include <kaneru/cdefs.h>

/* is it an alphanumeric character? */
int isalnum(int c) __weak;

/* is it an alphabetic character? */
int isalpha(int c) __weak;

/* is the character within ASCII range? */
int isascii(int c) __weak;

/* is it an invisible character? */
int isblank(int c) __weak;

/* is it a control character? */
int iscntrl(int c) __weak;

/* is it a decimal character? */
int isdigit(int c) __weak;

/* is it a graphical character? */
int isgraph(int c) __weak;

/* is it a lowercase character? */
int islower(int c) __weak;

/* is it a printable character? */
int isprint(int c) __weak;

/* is it a punctuation character? */
int ispunct(int c) __weak;

/* is it a whitespace character? */
int isspace(int c) __weak;

/* is it an uppercase character? */
int isupper(int c) __weak;

/* is it hexadecimal character? */
int isxdigit(int c) __weak;

/* force the character to ASCII range */
int toascii(int c) __weak;

/* convert the character to lowercase */
int tolower(int c) __weak;

/* convert the character to uppercase */
int toupper(int c) __weak;

#endif /* __INCLUDE_CTYPE_H__ */
