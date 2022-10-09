/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c), 2022, Kaneru Contributors */
#ifndef __INCLUDE_X86_64_PMIO_H__
#define __INCLUDE_X86_64_PMIO_H__
#include <stdint.h>

#define PMIO_READ8_NAME "inb"
#define PMIO_READ16_NAME "inw"
#define PMIO_READ32_NAME "inl"
#define PMIO_WRITE8_NAME "outb"
#define PMIO_WRITE16_NAME "outw"
#define PMIO_WRITE32_NAME "outl"

typedef uint16_t pmio_addr_t;

typedef uint8_t pmio_uint8_t;
typedef uint16_t pmio_uint16_t;
typedef uint32_t pmio_uint32_t;

static inline void pmio_throttle(void)
{
    /* FIXME: this may not work on certain
     * platforms as this port is taken by some
     * device that shits bricks when startled. */
    asm volatile("outb %0, $0x80"::"a"((pmio_uint8_t)(0)));
}

#define pmio_define_read(bits) \
    static inline pmio_uint##bits##_t pmio_read##bits(pmio_addr_t addr) \
    {pmio_uint##bits##_t value;asm volatile(PMIO_READ##bits##_NAME" %1,%0":"=a"(value):"Nd"(addr));return value;} \
    static inline pmio_uint##bits##_t pmio_read##bits##_throttle(pmio_addr_t addr) \
    {pmio_throttle();return pmio_read##bits(addr);}
pmio_define_read(8)
pmio_define_read(16)
pmio_define_read(32)
#undef pmio_define_read

#define pmio_define_write(bits) \
    static inline void pmio_write##bits(pmio_addr_t addr, pmio_uint##bits##_t value) \
    {asm volatile(PMIO_WRITE##bits##_NAME" %0,%1"::"a"(value),"Nd"(addr));} \
    static inline void pmio_write##bits##_throttle(pmio_addr_t addr, pmio_uint##bits##_t value) \
    {pmio_write##bits(addr, value);pmio_throttle();}
pmio_define_write(8)
pmio_define_write(16)
pmio_define_write(32)
#undef pmio_define_write

#endif /* __INCLUDE_X86_64_PMIO_H__ */
