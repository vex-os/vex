/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_UAPI_KAN_KPRINTF_H__
#define _INCLUDE_UAPI_KAN_KPRINTF_H__

#define KP_MSG_SIZE     0x200
#define KP_RING_SIZE    0x1000
#define KP_MASK_ALL     0xFFFFFFFF
#define KP_MASK_NONE    0x00000000

#define KP_TRACE        50
#define KP_DEBUG        25
#define KP_NOTICE       10
#define KP_INFORM        0
#define KP_WARNING     -10
#define KP_ERROR       -25
#define KP_EMERG       -50

#define KP_UNMASKABLE   0x00000000
#define KP_MACHINE      0x00000001
#define KP_KPRINTF      0x00000002
#define KP_SYSCON       0x00000004
#define KP_RESOURCE     0x00000008
#define KP_INTERRUPT    0x00000010
#define KP_INITCALL     0x00000020

#endif /* _INCLUDE_UAPI_KAN_KPRINTF_H__ */
