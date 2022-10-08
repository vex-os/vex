/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c), 2022, Kaneru Contributors */
#include <string.h>

char *kstrncat(char *restrict s1, const char *restrict s2, size_t n)
{
    size_t nc;
    char *save = s1;
    while(*s1 && n--)
        s1++;
    nc = strnlen(s2, --n);
    s1[nc] = 0;
    memcpy(s1, s2, nc);
    return save;
}

char *kstrncpy(char *restrict s1, const char *restrict s2, size_t n)
{
    char *save = s1;
    while(*s2 && n--)
        *s1++ = *s2++;
    if(n)
        *s1 = 0;
    return save;
}

const void *memchr(const void *restrict s, int c, size_t n)
{
    const unsigned char *sp = s;
    while(n--) {
        if(*sp == (unsigned char)c)
            return sp;
        sp++;
    }

    return NULL;
}

int memcmp(const void *restrict s1, const void *restrict s2, size_t n)
{
    const unsigned char *ap = s1;
    const unsigned char *bp = s2;
    while(n--) {
        if(*ap != *bp)
            return *ap - *bp;
        ap++;
        bp++;
    }

    return 0;
}

void *memcpy(void *restrict s1, const void *restrict s2, size_t n)
{
    unsigned char *dp = s1;
    const unsigned char *sp = s2;
    while(n--)
        *dp++ = *sp++;
    return s1;
}

void *memset(void *restrict s, int c, size_t n)
{
    unsigned char *sp = s;
    while(n--)
        *sp++ = (unsigned char)c;
    return s;
}

char *strcat(char *restrict s1, const char *restrict s2)
{
    char *save = s1;
    if(*s1)
        while(*++s1);
    while((*s1++ = *s2++));
    return save;
}

const char *strchr(const char *restrict s, int c)
{
    do {
        if(*s != (char)c)
            continue;
        return s;
    } while(*s++);
    return NULL;
}

int strcmp(const char *restrict s1, const char *restrict s2)
{
    while(*s1 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

char *strcpy(char *restrict s1, const char *restrict s2)
{
    char *save = s1;
    while((*s1++ = *s2++));
    return save;
}

size_t strlen(const char *restrict s)
{
    size_t i;
    for(i = 0; *s++; i++);
    return i;
}

char *strncat(char *restrict s1, const char *restrict s2, size_t n)
{
    char *save = s1;
    while(*s1)
        s1++;
    while(n && (*s1++ = *s2++))
        n--;
    if(n == 0)
        *s1 = 0;
    return save;
}

int strncmp(const char *restrict s1, const char *restrict s2, size_t n)
{
    while(*s1 && *s1 == *s2 && n--) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

char *strncpy(char *restrict s1, const char *restrict s2, size_t n)
{
    char *save = s1;
    while(n && (*s1++ = *s2++))
        n--;
    while(n-- > 1)
        *s1++ = 0;
    return save;
}

size_t strnlen(const char *restrict s, size_t n)
{
    size_t i;
    for(i = 0; *s++ && i < n; i++);
    return i;
}

const char *strrchr(const char *restrict s, int c)
{
    const char *cp = NULL;
    do {
        if(*s == c)
            cp = s;
    } while(*s++);
    return cp;
}

const char *strstr(const char *restrict s1, const char *restrict s2)
{
    const char *ap = s1;
    const char *bp;

    while(*s1) {
        bp = s2;
        while (*bp && (*ap == *bp)) {
            ++ap;
            ++bp;
        }
        if (!*bp)
            return s1;
        ++s1;
        ap = s1;
    }

    return NULL;
}
