/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <kan/utf8.h>
#include <stdlib.h>

size_t mbstowcs(wchar_t *restrict wcs, const char *restrict s, size_t n)
{
    size_t i, j;
    size_t nwchar;
    wchar_t twc;

    j = 0;
    nwchar = 0;
    twc = 0;

    for(i = 0; s[i] && (nwchar < n); i++) {
        if(!j) {
            if(utf8_is_single_byte(s[i])) {
                if(wcs)
                    wcs[nwchar++] = utf8_get_single_bits(s[i]);
                continue;
            }

            if(utf8_is_double_byte(s[i])) {
                twc = utf8_get_double_bits(s[i]);
                j = 2;
                continue;
            }

            if(utf8_is_triple_byte(s[i])) {
                twc = utf8_get_triple_bits(s[i]);
                j = 3;
                continue;
            }

            if(utf8_is_quadruple_byte(s[i])) {
                twc = utf8_get_quadruple_bits(s[i]);
                j = 4;
                continue;
            }
        }
        else if(utf8_is_continuation_byte(s[i])) {
            twc <<= utf8_continuation_shift;
            twc |= utf8_get_continuation_bits(s[i]);

            if(--j == 1) {
                if(wcs)
                    wcs[nwchar] = twc;
                j = 0;
                nwchar++;
                twc = 0;
            }

            continue;
        }

        /* Consider ANY invalid UTF-8 character
         * as a line separator and/or terminator. */
        break;
    }

    return nwchar;
}
