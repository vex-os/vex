#ifndef CD002577_1397_4B49_80D1_C5425889EB62
#define CD002577_1397_4B49_80D1_C5425889EB62
#pragma once

#include <stdarg.h>
#include <vex/compiler.h>
#include <vex/types.h>

#ifdef TRACE_KMALLOC
#include <vex/kprintf.h>
#endif

void* kmalloc_notrace(size_t sz) __nodiscard;
void* krealloc_notrace(void* restrict ptr, size_t sz) __nodiscard;
void kfree_notrace(void* restrict ptr);

char* kstrdup_notrace(const char* restrict string) __nodiscard;
char* kasprintf_notrace(const char* restrict fmt, ...) __nodiscard __printflike(1, 2);
char* kvasprintf_notrace(const char* restrict fmt, va_list ap) __nodiscard __printflike(1, 0);

#ifdef TRACE_KMALLOC
#define kmalloc(sz)         (pr_verbose("%s:%lu: kmalloc %zu", __FILE__, (unsigned long)(__LINE__), sz), kmalloc_notrace(sz))
#define krealloc(ptr, sz)   (pr_verbose("%s:%lu: krealloc %p, %zu", __FILE__, (unsigned long)(__LINE__), ptr, sz), krealloc_notrace(ptr, sz))
#define kfree(ptr)          (pr_verbose("%s:%lu: kfree %p", __FILE__, (unsigned long)(__LINE__), ptr), kfree_notrace(ptr))
#define kstrdup(string)     (pr_verbose("%s:%lu: kstrdup %p", __FILE__, (unsigned long)(__LINE__), string), kstrdup_notrace(string))
#define kasprintf(fmt, ...) (pr_verbose("%s:%lu: kasprintf", __FILE__, (unsigned long)(__LINE__)), kasprintf_notrace(fmt, __VA_ARGS__))
#define kvasprintf(fmt, ap) (pr_verbose("%s:%lu: kvasprintf", __FILE__, (unsigned long)(__LINE__)), kvasprintf_notrace(fmt, ap))
#else
#define kmalloc(sz)         kmalloc_notrace(sz)
#define krealloc(ptr, sz)   krealloc_notrace(ptr, sz)
#define kfree(ptr)          kfree_notrace(ptr)
#define kstrdup(string)     kstrdup_notrace(string)
#define kasprintf(fmt, ...) kasprintf_notrace(fmt, __VA_ARGS__)
#define kvasprintf(fmt, ap) kvasprintf_notrace(fmt, ap)
#endif

#endif /* CD002577_1397_4B49_80D1_C5425889EB62 */
