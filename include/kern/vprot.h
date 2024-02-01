/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2024, VX/sys Contributors */
#ifndef INCLUDE_KERN_VPROT_H
#define INCLUDE_KERN_VPROT_H

enum vprot {
    VPROT_NONE  = 0x0000,
    VPROT_READ  = 0x0001,
    VPROT_WRITE = 0x0002,
    VPROT_EXEC  = 0x0003,
    VPROT_USER  = 0x0004,
};

#endif /* INCLUDE_KERN_VPROT_H */
