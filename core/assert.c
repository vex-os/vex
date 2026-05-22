#include <core/assert.h>
#include <core/panic.h>

void __noreturn assertion_fail(const char* restrict file, unsigned long line, const char* restrict func, const char* restrict expr)
{
    panic("assertion failed: [%s] at %s:%lu in function %s", expr, file, line, func);
}
