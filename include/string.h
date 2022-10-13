/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_STRING_H__
#define __INCLUDE_STRING_H__
#include <stddef.h>
#include <sys/cdefs.h>

char *kstrncat(char *restrict s1, const char *restrict s2, size_t n) __weak;
char *kstrncpy(char *restrict s1, const char *restrict s2, size_t n) __weak;

const void *memchr(const void *restrict s, int c, size_t n) __weak;
int memcmp(const void *restrict s1, const void *restrict s2, size_t n) __weak;
void *memcpy(void *restrict s1, const void *restrict s2, size_t n) __weak;
void *memset(void *restrict s, int c, size_t n) __weak;

char *strcat(char *restrict s1, const char *restrict s2) __weak;
const char *strchr(const char *restrict s, int c) __weak;
int strcmp(const char *restrict s1, const char *restrict s2) __weak;
char *strcpy(char *restrict s1, const char *restrict s2) __weak;
size_t strlen(const char *restrict s) __weak;
char *strncat(char *restrict s1, const char *restrict s2, size_t n) __weak;
int strncmp(const char *restrict s1, const char *restrict s2, size_t n) __weak;
char *strncpy(char *restrict s1, const char *restrict s2, size_t n) __weak;
size_t strnlen(const char *restrict s, size_t n) __weak;
const char *strrchr(const char *restrict s, int c) __weak;
const char *strstr(const char *restrict s1, const char *restrict s2) __weak;

#endif /* __INCLUDE_STRING_H__ */
