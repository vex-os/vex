/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
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
            if((s[i] & 0x80) == 0x00) {
                if(wcs)
                    wcs[nwchar++] = (s[i] & 0x7F);
                continue;
            }

            if((s[i] & 0xE0) == 0xC0) {
                twc = (s[i] & 0x1F);
                j = 2;
                continue;
            }

            if((s[i] & 0xF0) == 0xE0) {
                twc = (s[i] & 0x0F);
                j = 3;
                continue;
            }

            if((s[i] & 0xF8) == 0xF0) {
                twc = (s[i] & 0x07);
                j = 4;
                continue;
            }
        }
        else if((s[i] & 0xC0) == 0x80) {
            twc <<= 6;
            twc |= (s[i] & 0x3F);

            if(--j == 1) {
                if(wcs)
                    wcs[nwchar] = twc;
                j = 0;
                nwchar++;
                twc = 0;
            }

            continue;
        }

        // Consider ANY invalid UTF-8 byte
        // as a newline or as a sequence terminator.
        break;
    }

    return nwchar;
}
