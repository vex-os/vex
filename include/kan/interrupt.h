/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef _INCLUDE_KAN_INTERRUPT_H__
#define _INCLUDE_KAN_INTERRUPT_H__
#include <uapi/kan/interrupt.h>

long alloc_interrupt(long hint);
int add_interrupt_handler(long vector, interrupt_handler_t handler);
void trigger_interrupt(long vector, interrupt_frame_t *restrict frame);

#endif /* _INCLUDE_KAN_INTERRUPT_H__ */
