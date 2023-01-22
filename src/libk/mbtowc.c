/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/utf8.h>
#include <stdlib.h>

size_t mbtowc(wchar_t *restrict wc, const char *restrict s, size_t n)
{
    size_t i, j;
    size_t nchar;
    wchar_t twc;

    if(!s || !s[0]) {
        /* If I were to implement something
         * like mbstate_t, at this point it
         * should be reset (cppreference). */
        return 0;
    }

    j = 0;
    nchar = 0;
    twc = 0;

    for(i = 0; s[i] && (i < n); i++) {
        if(!nchar) {
            if(utf8_is_single_byte(s[i])) {
                if(wc)
                    wc[0] = utf8_get_single_bits(s[i]);
                return 1;
            }

            if(utf8_is_double_byte(s[i])) {
                twc = utf8_get_double_bits(s[i]);
                nchar = 2;
                j = nchar;
                continue;
            }

            if(utf8_is_triple_byte(s[i])) {
                twc = utf8_get_triple_bits(s[i]);
                nchar = 3;
                j = nchar;
                continue;
            }

            if(utf8_is_quadruple_byte(s[i])) {
                twc = utf8_get_quadruple_bits(s[i]);
                nchar = 4;
                j = nchar;
                continue;
            }
        }
        else if(utf8_is_continuation_byte(s[i])) {
            twc <<= utf8_continuation_shift;
            twc |= utf8_get_continuation_bits(s[i]);
            if(--j == 1)
                break;
            continue;
        }

        return SIZE_MAX;
    }

    if(j == 1) {
        if(wc)
            wc[0] = twc;
        return nchar;
    }

    return SIZE_MAX;
}
