#ifndef BECC41D1_6D4B_4AC8_AAE9_441C37E140AB
#define BECC41D1_6D4B_4AC8_AAE9_441C37E140AB

#include <core/cdefs.h>

#include <stdarg.h>

extern void fpanic(const char* restrict file, unsigned long line, const char* restrict fmt, ...) __noreturn __printflike(3, 4);
extern void fvpanic(const char* restrict file, unsigned long line, const char* restrict fmt, va_list ap) __noreturn __printflike(3, 0);
#define panic(...)  fpanic(__FILE__, __LINE__, __VA_ARGS__)
#define vpanic(...) fvpanic(__FILE__, __LINE__, __VA_ARGS__)

#endif /* BECC41D1_6D4B_4AC8_AAE9_441C37E140AB */
