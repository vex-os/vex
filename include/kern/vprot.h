/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2023, KanOS Contributors */
#ifndef __INCLUDE_KERN_VPROT_H__
#define __INCLUDE_KERN_VPROT_H__

enum {
    VPROT_READ  = 0x0001,
    VPROT_WRITE = 0x0002,
    VPROT_EXEC  = 0x0004,
    VPROT_USER  = 0x0008,
};

#endif /* __INCLUDE_KERN_VPROT_H__ */
