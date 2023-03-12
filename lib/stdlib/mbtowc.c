/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
#include <stdlib.h>

size_t mbtowc(wchar_t *restrict wc, const char *restrict s, size_t n)
{
    size_t i, j;
    size_t nchar;
    wchar_t twc;

    if(!s || !s[0]) {
        // If I were to implement something like
        // mbstate_t, at this point it should be
        // reset according to cppreference...
        return 0;
    }

    j = 0;
    nchar = 0;
    twc = 0;

    for(i = 0; s[i] && (i < n); i++) {
        if(!nchar) {
            if((s[i] & 0x80) == 0x00) {
                if(wc)
                    wc[0] = (s[i] & 0x7F);
                return 1;
            }

            if((s[i] & 0xE0) == 0xC0) {
                twc = (s[i] & 0x1F);
                nchar = 2;
                j = nchar;
                continue;
            }

            if((s[i] & 0xF0) == 0xE0) {
                twc = (s[i] & 0x0F);
                nchar = 3;
                j = nchar;
                continue;
            }

            if((s[i] & 0xF8) == 0xF0) {
                twc = (s[i] & 0x07);
                nchar = 4;
                j = nchar;
                continue;
            }
        }
        else if((s[i] & 0xC0) == 0x80) {
            twc <<= 6;
            twc |= (s[i] & 0x3F);
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
