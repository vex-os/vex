/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_SYS_PMIO_H__
#define __INCLUDE_SYS_PMIO_H__
#if defined(__X86_64__)
#include <x86_64/pmio.h>
#else
#error "Port-mapped IO not supported!"
#endif
#endif /* __INCLUDE_SYS_PMIO_H__ */
