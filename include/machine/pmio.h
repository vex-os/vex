/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef __INCLUDE_MACHINE_PMIO_H__
#define __INCLUDE_MACHINE_PMIO_H__

#if defined(__x86_64__)
#include <x86_64/pmio.h>
#define PMIO_MAX_OFFSET X86_PMIO_MAX_OFFSET
#define pmio_read8(offset, value) x86_pmio_read8((offset), (value))
#define pmio_read16(offset, value) x86_pmio_read16((offset), (value))
#define pmio_read32(offset, value) x86_pmio_read32((offset), (value))
#define pmio_write8(offset, value) x86_pmio_write8((offset), (value))
#define pmio_write16(offset, value) x86_pmio_write16((offset), (value))
#define pmio_write32(offset, value) x86_pmio_write32((offset), (value))
#define pmio_wait() x86_pmio_wait()
#else
#include <kan/errno.h>
#define PMIO_MAX_OFFSET 0
#define pmio_read8(offset, value) ((int)(EIO))
#define pmio_read16(offset, value) ((int)(EIO))
#define pmio_read32(offset, value) ((int)(EIO))
#define pmio_write8(offset, value) ((int)(EIO))
#define pmio_write16(offset, value) ((int)(EIO))
#define pmio_write32(offset, value) ((int)(EIO))
#define pmio_wait() ({})
#endif

#endif/* __INCLUDE_MACHINE_PMIO_H__ */
