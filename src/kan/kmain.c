/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#include <kan/initcall.h>
#include <kan/kprintf.h>

void __noreturn __used kmain(void)
{
    for(;;);
    unreachable();
}
