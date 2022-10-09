/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_STRING_H__
#define __INCLUDE_STRING_H__
#include <kaneru/cdefs.h>
#include <stddef.h>

/* appends a string to an another, fixed-length string */
char *kstrncat(char *restrict s1, const char *restrict s2, size_t n) __weak;

/* writes a string to an another, fixed-length string */
char *kstrncpy(char *restrict s1, const char *restrict s2, size_t n) __weak;

/* locates a byte entry within a fixed-length buffer */
const void *memchr(const void *restrict s, int c, size_t n) __weak;

/* compares two fixed-length buffers */
int memcmp(const void *restrict s1, const void *restrict s2, size_t n) __weak;

/* writes a fixed-length buffer to an another buffer */
void *memcpy(void *restrict s1, const void *restrict s2, size_t n) __weak;

/* fills up a fixed-length buffer with a value */
void *memset(void *restrict s, int c, size_t n) __weak;

/* appends a string to an another string */
char *strcat(char *restrict s1, const char *restrict s2) __weak;

/* locates a character entry inside an input string */
const char *strchr(const char *restrict s, int c) __weak;

/* compares two strings */
int strcmp(const char *restrict s1, const char *restrict s2) __weak;

/* writes a string to an another string */
char *strcpy(char *restrict s1, const char *restrict s2) __weak;

/* determines the length of a string */
size_t strlen(const char *restrict s) __weak;

/* appends a fixed-length string to an another string */
char *strncat(char *restrict s1, const char *restrict s2, size_t n) __weak;

/* compares two fixed-length strings */
int strncmp(const char *restrict s1, const char *restrict s2, size_t n) __weak;

/* writes a fixed-length string to an another string */
char *strncpy(char *restrict s1, const char *restrict s2, size_t n) __weak;

/* determines the length of a fixed-length string */
size_t strnlen(const char *restrict s, size_t n) __weak;

/* locates a character entry inside an input string */
const char *strrchr(const char *restrict s, int c) __weak;

/* locates a substring entry inside an input string, in reverse */
const char *strstr(const char *restrict s1, const char *restrict s2) __weak;

#endif /* __INCLUDE_STRING_H__ */
