/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_X86_64_PMIO_H__
#define __INCLUDE_X86_64_PMIO_H__
#include <stdint.h>

int x86_pmio_read8(uintptr_t off, uint8_t *restrict val);
int x86_pmio_read16(uintptr_t off, uint16_t *restrict val);
int x86_pmio_read32(uintptr_t off, uint32_t *restrict val);
int x86_pmio_write8(uintptr_t off, uint8_t val);
int x86_pmio_write16(uintptr_t off, uint16_t val);
int x86_pmio_write32(uintptr_t off, uint32_t val);
void x86_pmio_throttle(void);

#endif /* __INCLUDE_X86_64_PMIO_H__ */
