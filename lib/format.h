#ifndef AF47BF7C_48CD_4CD6_A12B_4EC3DE881CA5
#define AF47BF7C_48CD_4CD6_A12B_4EC3DE881CA5

#include <core/cdefs.h>

#include <stdarg.h>
#include <stddef.h>

typedef void (*format_func_t)(int character, void* restrict user);

extern int format(format_func_t func, void* restrict user, const char* restrict fmt, ...) __printflike(3, 4);
extern int vformat(format_func_t func, void* restrict user, const char* restrict fmt, va_list args) __printflike(3, 0);

#endif /* AF47BF7C_48CD_4CD6_A12B_4EC3DE881CA5 */
