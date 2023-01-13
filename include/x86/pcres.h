/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, VxEngine Contributors */
#ifndef _INCLUDE_X86_PCRES_H__
#define _INCLUDE_X86_PCRES_H__
#include <kan/initcall.h>
#include <kan/resource.h>

extern resource_t bochs_port_E9;
extern resource_t serial_0;
extern resource_t serial_1;

initcall_extern(pcres);

#endif /* _INCLUDE_X86_PCRES_H__ */
