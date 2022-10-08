/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_KANERU_PMIO_H__
#define __INCLUDE_KANERU_PMIO_H__
#if defined __X86_64__ || 1
#include <x86_64/pmio.h>
#else
#error Architecture doesn't support PMIO!
#endif
#endif /* __INCLUDE_KANERU_PMIO_H__ */
