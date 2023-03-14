#ifndef __INCLUDE_SYS_MALLOC_H__
#define __INCLUDE_SYS_MALLOC_H__
#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <sys/initcall.h>

void *malloc(size_t n);
void *calloc(size_t count, size_t n);
void *realloc(void *restrict ptr, size_t n);
void free(void *restrict ptr);

initcall_extern(malloc);

#endif /* __INCLUDE_SYS_MALLOC_H__ */
