/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, KanOS Contributors */
#ifndef _INCLUDE_UAPI_KAN_KPRINTF_H__
#define _INCLUDE_UAPI_KAN_KPRINTF_H__

#define KP_MSG_SZ   0x200
#define KP_RING_SZ  0x1000

#define KP_TRACE    50
#define KP_DEBUG    25
#define KP_NOTICE   10
#define KP_INFORM    0
#define KP_WARNING -10
#define KP_ERROR   -25
#define KP_EMERG   -50

#endif /* _INCLUDE_UAPI_KAN_KPRINTF_H__ */
