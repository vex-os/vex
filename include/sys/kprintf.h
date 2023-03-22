#ifndef __INCLUDE_SYS_KPRINTF_H__
#define __INCLUDE_SYS_KPRINTF_H__
#include <stdarg.h>
#include <stddef.h>
#include <sys/cdefs.h>

typedef struct console_s {
    struct console_s *cn_next;
    void (*cn_putchar)(struct console_s *restrict console, int c);
    void (*cn_unblank)(struct console_s *restrict console);
    char cn_name[64];
    void *cn_data;
} console_t;

int register_console(console_t *restrict console);
void unregister_console(console_t *restrict console);
void console_unblank(void);

int putchar(int c);
int puts(const char *restrict s);
int printf(const char *restrict fmt, ...) __printflike(1, 2);
int vprintf(const char *restrict fmt, va_list ap);

// These add a newline at the end like puts() does
int kprintf(const char *restrict fmt, ...) __printflike(1, 2);
int kvprintf(const char *restrict fmt, va_list ap);

#endif /* __INCLUDE_SYS_KPRINTF_H__ */
