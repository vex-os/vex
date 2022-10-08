/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __LIBK_CTYPE_KCP_H__
#define __LIBK_CTYPE_KCP_H__

#define KCP_NULL 0x000
#define KCP_CTRL 0x001
#define KCP_GRPH 0x002
#define KCP_SPCE 0x004
#define KCP_BLNK 0x008
#define KCP_UPRC 0x010
#define KCP_LWRC 0x020
#define KCP_DECD 0x040
#define KCP_HEXD 0x080
#define KCP_PNCT 0x100

struct kcp {
    int lower, upper;
    unsigned int flags;
};

#define KCP_SIZE 0x101 /* 256 plus EOF */
extern const struct kcp _kcp[KCP_SIZE];

#endif /* __LIBK_CTYPE_KCP_H__ */
