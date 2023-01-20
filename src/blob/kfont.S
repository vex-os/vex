/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c), 2023, KanOS Contributors */
.section .rodata

.global __kfont
.type __kfont, @object
__kfont:
    .incbin "fonts/proggyclean-sz-13.psf"

.global __kfont_end
.type __kfont_end, @object
__kfont_end:

.global __kfont_size
.type __kfont_size, @object
__kfont_size:
    .quad __kfont_end - __kfont
