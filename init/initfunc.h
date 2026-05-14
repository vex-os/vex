#ifndef D6DE57CA_EA06_4C53_BB52_6B8E263AC6DF
#define D6DE57CA_EA06_4C53_BB52_6B8E263AC6DF

#include <core/cdefs.h>

struct initfunc_metadata {
    int (*function)(void);
    unsigned int level;
    char identity[64];
};

#define __initfunc(function_ptr, level_no)                                                                            \
    static const __used __section(".initfunc." #level_no) struct initfunc_metadata __initfunc_metadata_##function = { \
        .function = function_ptr,                                                                                     \
        .level = level_no,                                                                                            \
        .identity = #function_ptr,                                                                                    \
    }

#define initfunc_dev_arch(function) __initfunc(function, 0)
#define initfunc_dev(function)      __initfunc(function, 1)
#define initfunc_fs(function)       __initfunc(function, 2)
#define initfunc_late(function)     __initfunc(function, 3)

extern const struct initfunc_metadata initfunc_dev_arch_start[];
extern const struct initfunc_metadata initfunc_dev_arch_end[];

extern const struct initfunc_metadata initfunc_dev_start[];
extern const struct initfunc_metadata initfunc_dev_end[];

extern const struct initfunc_metadata initfunc_fs_start[];
extern const struct initfunc_metadata initfunc_fs_end[];

extern const struct initfunc_metadata initfunc_late_start[];
extern const struct initfunc_metadata initfunc_late_end[];

extern void do_initfuncs(const struct initfunc_metadata* start, const struct initfunc_metadata* end);

#endif /* D6DE57CA_EA06_4C53_BB52_6B8E263AC6DF */
