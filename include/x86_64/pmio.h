/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_X86_64_PMIO_H__
#define __INCLUDE_X86_64_PMIO_H__
#include <kan/errno.h>
#include <stdint.h>
#include <sys/cdefs.h>

#define PMIO_SUPPORTED 1
#define PMIO_MAX_OFFSET 0xFFFF
int pmio_read8(uintptr_t offset, uint8_t *restrict value);
int pmio_read16(uintptr_t offset, uint16_t *restrict value);
int pmio_read32(uintptr_t offset, uint32_t *restrict value);
int pmio_write8(uintptr_t offset, uint8_t value);
int pmio_write16(uintptr_t offset, uint16_t value);
int pmio_write32(uintptr_t offset, uint32_t value);
void pmio_wait(void);

#endif /* __INCLUDE_X86_64_PMIO_H__ */
