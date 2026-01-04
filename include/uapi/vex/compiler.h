#ifndef _UAPI_VEX_COMPILER_H
#define _UAPI_VEX_COMPILER_H 1

#ifndef __GNUC__
#define __attribute__(...)
#define __restrict
#define __inline
#endif

#ifndef __has_attribute
#define __has_attribute(...) (0)
#endif

#ifndef __has_builtin
#define __has_builtin(...) (0)
#endif

#if __STDC_VERSION__ < 199901L
#define restrict __restrict
#define inline   __inline
#endif

#define __alias(func)      __attribute__((alias(#func)))
#define __align_as(type)   __attribute__((aligned(sizeof(type))))
#define __always_inline    __attribute__((always_inline))
#define __nodiscard        __attribute__((warn_unused_result))
#define __noreturn         __attribute__((noreturn))
#define __packed           __attribute__((packed))
#define __printflike(x, y) __attribute__((format(printf, x, y)))
#define __scanflike(x, y)  __attribute__((format(scanf, x, y)))
#define __section(name)    __attribute__((section(name)))
#define __used             __attribute__((used))

#endif
