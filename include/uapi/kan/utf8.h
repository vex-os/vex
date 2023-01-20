/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#ifndef _INCLUDE_UAPI_KAN_UTF8_H__
#define _INCLUDE_UAPI_KAN_UTF8_H__

#define utf8_continuation_shift 6

#define utf8_is_single_byte(c) (((c) & 0x80) == 0x00)
#define utf8_is_double_byte(c) (((c) & 0xE0) == 0xC0)
#define utf8_is_triple_byte(c) (((c) & 0xF0) == 0xE0)
#define utf8_is_quadruple_byte(c) (((c) & 0xF8) == 0xF0)
#define utf8_is_continuation_byte(c) (((c) & 0xC0) == 0x80)

#define utf8_get_single_bits(c) ((c) & 0x7F)
#define utf8_get_double_bits(c) ((c) & 0x1F)
#define utf8_get_triple_bits(c) ((c) & 0x0F)
#define utf8_get_quadruple_bits(c) ((c) & 0x07)
#define utf8_get_continuation_bits(c) ((c) & 0x3F)

#endif /* _INCLUDE_UAPI_KAN_UTF8_H__ */
