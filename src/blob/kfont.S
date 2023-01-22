/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
.section .rodata

#define FONT_PATH "fonts/Cozette-6x13.psf"
//#define FONT_PATH "fonts/ProggyCleanSZ-7x13.psf"
//#define FONT_PATH "fonts/Tamsyn10x20b.psf"
//#define FONT_PATH "fonts/spleen-12x24.psf"

.global __kfont
.type __kfont, @object
__kfont:
    .incbin FONT_PATH

.global __kfont_end
.type __kfont_end, @object
__kfont_end:

.global __kfont_size
.type __kfont_size, @object
__kfont_size:
    .quad __kfont_end - __kfont

.global __kfont_path
.type __kfont_path, @object
__kfont_path:
    .asciz FONT_PATH
