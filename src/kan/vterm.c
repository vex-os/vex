/* SPDX-License-Identifier: BSD-2-Clause */
/* libvterm: Copyright (c) 2021, Kirill GPRB. */
/* kan/vterm: Copyright (c), 2023, KanOS Contributors */
#include <ctype.h>
#include <kan/errno.h>
#include <kan/kmalloc.h>
#include <kan/vterm.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/* und: This simplifies the code by
 * getting rid of one-liner if statements */
#define VT_CALL(fn, ...) ({if(fn) (fn)(__VA_ARGS__);})

/* Go through dirty cells and draw them */
static void vt_redraw(vt_t *restrict vt)
{
    size_t i, j;
    vt_cell_t *cell;
    for(i = 0; i < vt->cwidth; i++) {
        for(j = 0; j < vt->cheight; j++) {
            cell = &vt->cells[j * vt->cwidth + i];
            if(cell->dirty) {
                cell->dirty = false;
                VT_CALL(vt->draw_cell, vt, cell, i, j);
            }
        }
    }

    /* Draw the cell supposed to be under the old cursor */
    if(vt->flags & VT_UNBLANK) {
        cell = &vt->cells[vt->pcursor.y * vt->cwidth + vt->pcursor.x];
        VT_CALL(vt->draw_cell, vt, cell, vt->pcursor.x, vt->pcursor.y);
    }

    /* Draw the new cursor */
    VT_CALL(vt->draw_cursor, vt, &vt->cursor);

    /* This essentially lets the backend know that the
     * vterm_puts call has finished and the backend is
     * free to do its own drawing or, perhaps copying
     * contents of a backbuffer to the framebuffer. */
    VT_CALL(vt->draw_end, vt);

    /* Save the cursor position */
    vt->pcursor = vt->cursor;
}

/* Move the screen <lines> lines up. */
static void vt_scroll_up(vt_t *restrict vt, size_t lines)
{
    size_t i;
    size_t j;
    size_t screen;
    size_t subscreen;
    vt_cell_t *cell;

    if(lines > vt->cheight)
        lines = vt->cheight;
    screen = vt->cwidth * vt->cheight;
    subscreen = screen - vt->cwidth;

    /* Repeat the scrolling <lines> times. */
    /* FIXME: there MUST be a faster way to do this. */
    for(i = 0; i < lines; i++) {
        /* Clone a part of the screen up one line */
        for(j = 0; j < subscreen; j++) {
            cell = &vt->cells[j];
            cell[0] = cell[vt->cwidth];
            cell->dirty = true;
        }

        /* Erase the newly founded line */
        cell = &vt->cells[subscreen];
        for(j = 0; j < vt->cwidth; j++) {
            cell[j].attr = vt->attr;
            cell[j].attr.mode = VT_CONCEAL;
            cell[j].dirty = true;
            cell[j].wc = VT_NUL;
        }
    }
}

/* Clear a range of cells from X0:Y0 to X1:Y1 */
static void vt_clear(vt_t *restrict vt, size_t x0, size_t y0, size_t x1, size_t y1)
{
    size_t i;
    size_t beg = y0 * vt->cwidth + x0;
    size_t end = y1 * vt->cwidth + x1;
    vt_cell_t *cell;

    for(i = beg; i <= end; i++) {
        cell = &vt->cells[i];
        cell->attr = vt->attr;
        cell->attr.mode = VT_CONCEAL;
        cell->dirty = true;
        cell->wc = VT_NUL;
    }
}

/* Move cursor to a new line <lines> times */
static void vt_newline(vt_t *restrict vt, size_t lines)
{
    vt->cursor.y += lines;

    if(vt->cursor.y >= vt->cheight) {
        if(vt->flags & VT_SCROLLING) {
            vt_scroll_up(vt, vt->cursor.y - vt->cheight + 1);
            vt->cursor.y = vt->cheight - 1;
        }
        else {
            vt->cursor.x = 0;
            vt->cursor.y = 0;
            vt_clear(vt, 0, 0, vt->cwidth - 1, vt->cheight - 1);
        }
    }
}

/* Print a unicode character */
static void vt_putwchar(vt_t *restrict vt, wchar_t wc)
{
    vt_cell_t *cell = &vt->cells[vt->cursor.y * vt->cwidth + vt->cursor.x++];
    cell->attr = vt->attr;
    cell->dirty = true;
    cell->wc = wc;

    if(vt->cursor.x >= vt->cwidth) {
        vt->cursor.x = 0;
        vt_newline(vt, 1);
    }
}

/* ANSI CUU/CUD/CUF/CUB (cursor up/down/forward/back) */
static void vt_ansi_cux(vt_t *restrict vt, int horz, int vert)
{
    size_t attrib = vt->parser.args[0];
    if(!attrib)
        attrib = 1;

    if(horz) {
        if(horz < 0) {
            if(vt->cursor.x < attrib)
                vt->cursor.x = 0;
            else
                vt->cursor.x -= attrib;
            return;
        }

        vt->cursor.x += attrib;
        if(vt->cursor.x >= vt->cwidth)
            vt->cursor.x = vt->cwidth - 1;
        return;
    }

    if(vert) {
        if(vert < 0) {
            if(vt->cursor.y < attrib)
                vt->cursor.y = 0;
            else
                vt->cursor.y -= attrib;
            return;
        }

        vt->cursor.y += attrib;
        if(vt->cursor.y >= vt->cheight)
            vt->cursor.y = vt->cheight - 1;
        return;
    }
}

/* ANSI CNL (cursor next line) */
static void vt_ansi_cnl(vt_t *restrict vt)
{
    size_t attrib = vt->parser.args[0];
    if(!attrib)
        attrib = 1;
    vt->cursor.x = 0;
    vt_newline(vt, attrib);
}

/* ANSI CPL (cursor previous line) */
static void vt_ansi_cpl(vt_t *restrict vt)
{
    size_t attrib = vt->parser.args[0];
    if(!attrib)
        attrib = 1;
    if(attrib > vt->cursor.y)
        attrib = vt->cursor.y;
    vt->cursor.x = 0;
    vt->cursor.y -= attrib;
}

/* ANSI CHA (cursor horizontal absolute) */
static void vt_ansi_cha(vt_t *restrict vt)
{
    size_t attrib = vt->parser.args[0];
    if(!attrib)
        attrib = 1;
    if(attrib > vt->cwidth)
        attrib = vt->cwidth;
    vt->cursor.x = attrib - 1;
}

/* ANSI CUP (cursor position) */
static void vt_ansi_cup(vt_t *restrict vt)
{
    size_t attrib_x = vt->parser.args[1];
    size_t attrib_y = vt->parser.args[0];
    if(!attrib_x)
        attrib_x = 1;
    if(attrib_x > vt->cwidth)
        attrib_x = vt->cwidth;
    if(!attrib_y)
        attrib_y = 1;
    if(attrib_y > vt->cheight)
        attrib_y = vt->cheight;
    vt->cursor.x = attrib_x - 1;
    vt->cursor.y = attrib_y - 1;
}

/* ANSI ED (erase in display) */
static void vt_ansi_ed(vt_t *restrict vt)
{
    switch(vt->parser.args[0]) {
        case 0:
            vt_clear(vt, vt->cursor.x, vt->cursor.y, vt->cwidth - 1, vt->cheight - 1);
            break;
        case 1:
            vt_clear(vt, 0, 0, vt->cursor.x, vt->cursor.y);
            break;
        case 2:
            vt_clear(vt, 0, 0, vt->cwidth - 1, vt->cheight - 1);
            VT_CALL(vt->draw_clear, vt, vt_get_bg(vt, &vt->attr));
            break;
        case 3:
            /* UNDONE: clear scrollback */
            /* UNDONE: the scrollback itself */
            vt_clear(vt, 0, 0, vt->cwidth - 1, vt->cheight - 1);
            VT_CALL(vt->draw_clear, vt, vt_get_bg(vt, &vt->attr));
            break;
    }
}

/* ANSI EL (erase in line) */
static void vt_ansi_el(vt_t *restrict vt)
{
    switch(vt->parser.args[0]) {
        case 0:
            vt_clear(vt, vt->cursor.x, vt->cursor.y, vt->cwidth - 1, vt->cursor.y);
            break;
        case 1:
            vt_clear(vt, 0, vt->cursor.y, vt->cursor.x, vt->cursor.y);
            break;
        case 2:
            vt_clear(vt, 0, vt->cursor.y, vt->cwidth - 1, vt->cursor.y);
            break;
    }
}

/* ANSI SU (scroll up) */
static void vt_ansi_su(vt_t *restrict vt)
{
    size_t attrib = vt->parser.args[0];
    if(!attrib)
        attrib = 1;
    vt_scroll_up(vt, attrib);
}

/* ANSI SGR (select graphic rendition) */
/* UNDONE: 8-bit (256-color) color modifiers */
static void vt_ansi_sgr(vt_t *restrict vt)
{
    size_t i;

    for(i = 0; i < vt->parser.argc; i++) {
        switch(vt->parser.args[i]) {
            case 0:
                vt->attr.mode = VT_BG_24BIT | VT_FG_24BIT;
                vt->attr.bg = VT_DBG_COLOR;
                vt->attr.fg = VT_DFG_COLOR;
                continue;
            case 1:
                vt->attr.mode |= VT_BOLD;
                continue;
            case 2:
                vt->attr.mode &= ~VT_BOLD;
                vt->attr.mode |= VT_FAINT;
                continue;
            case 3:
                vt->attr.mode |= VT_ITALIC;
                continue;
            case 4:
                vt->attr.mode |= VT_UNDERLINE;
                continue;
            case 5:
                vt->attr.mode &= ~VT_FAST_BLINK;
                vt->attr.mode |= VT_SLOW_BLINK;
                continue;
            case 6:
                vt->attr.mode &= ~VT_SLOW_BLINK;
                vt->attr.mode |= VT_FAST_BLINK;
                continue;
            case 7:
                vt->attr.mode |= VT_REVERSE;
                continue;
            case 8:
                vt->attr.mode |= VT_CONCEAL;
                continue;
            case 9:
                vt->attr.mode |= VT_CROSSED_OUT;
                continue;
            case 21:
                vt->attr.mode &= ~VT_BOLD;
                continue;
            case 22:
                vt->attr.mode &= ~VT_BOLD;
                vt->attr.mode &= ~VT_FAINT;
                continue;
            case 23:
                vt->attr.mode &= ~VT_ITALIC;
                continue;
            case 24:
                vt->attr.mode &= ~VT_UNDERLINE;
                continue;
            case 25:
                vt->attr.mode &= ~VT_SLOW_BLINK;
                vt->attr.mode &= ~VT_FAST_BLINK;
                continue;
            case 27:
                vt->attr.mode &= ~VT_REVERSE;
                continue;
            case 28:
                vt->attr.mode &= ~VT_CONCEAL;
                continue;
            case 29:
                vt->attr.mode &= ~VT_CROSSED_OUT;
                continue;
            case 39:
                vt->attr.fg = VT_DFG_COLOR;
                vt->attr.mode &= ~VT_FG_BRIGHT;
                vt->attr.mode |= VT_FG_24BIT;
                continue;
            case 49:
                vt->attr.bg = VT_DBG_COLOR;
                vt->attr.mode &= ~VT_BG_BRIGHT;
                vt->attr.mode |= VT_BG_24BIT;
                continue;
        }

        /* ANSI (3-bit) foreground */
        if(vt->parser.args[i] >= 30 && vt->parser.args[i] <= 37) {
            vt->attr.fg = (vt->parser.args[i] - 30) & 0x07;
            vt->attr.mode &= ~VT_FG_BRIGHT;
            vt->attr.mode &= ~VT_FG_24BIT;
            continue;
        }

        /* ANSI (3-bit) background */
        if(vt->parser.args[i] >= 40 && vt->parser.args[i] <= 47) {
            vt->attr.bg = (vt->parser.args[i] - 40) & 0x07;
            vt->attr.mode &= ~VT_BG_BRIGHT;
            vt->attr.mode &= ~VT_BG_24BIT;
            continue;
        }

        /* ANSI (3-bit) bright foreground */
        if(vt->parser.args[i] >= 90 && vt->parser.args[i] <= 97) {
            vt->attr.fg = (vt->parser.args[i] - 90) & 0x07;
            vt->attr.mode |= VT_FG_BRIGHT;
            vt->attr.mode &= ~VT_FG_24BIT;
            continue;
        }

        /* ANSI (3-bit) bright background */
        if(vt->parser.args[i] >= 100 && vt->parser.args[i] <= 107) {
            vt->attr.bg = (vt->parser.args[i] - 100) & 0x07;
            vt->attr.mode |= VT_BG_BRIGHT;
            vt->attr.mode &= ~VT_BG_24BIT;
            continue;
        }

        if(i + 4 < vt->parser.argc) {
            /* 24-bit foreground */
            if(vt->parser.args[i + 0] == 38 && vt->parser.args[i + 1] == 2) {
                vt->attr.fg = 0;
                vt->attr.fg |= (vt->parser.args[i + 2] & 0xFF) << 16;
                vt->attr.fg |= (vt->parser.args[i + 3] & 0xFF) << 8;
                vt->attr.fg |= (vt->parser.args[i + 4] & 0xFF);
                vt->attr.mode &= ~VT_FG_BRIGHT;
                vt->attr.mode |= VT_FG_24BIT;
                i += 5;
                continue;
            }

            /* 24-bit background */
            if(vt->parser.args[i + 0] == 48 && vt->parser.args[i + 1] == 2) {
                vt->attr.bg = 0;
                vt->attr.bg |= (vt->parser.args[i + 2] & 0xFF) << 16;
                vt->attr.bg |= (vt->parser.args[i + 3] & 0xFF) << 8;
                vt->attr.bg |= (vt->parser.args[i + 4] & 0xFF);
                vt->attr.mode &= ~VT_BG_BRIGHT;
                vt->attr.mode |= VT_BG_24BIT;
                i += 5;
                continue;
            }
        }
    }
}

/* Handle some non-standard private sequences */
/* Returns TRUE if a private sequence was captured */
static bool vt_ansi_private(vt_t *restrict vt, char c)
{

    return false;
}

/* Parse a complete (VT_STATE_END) sequence */
static void vt_parse(vt_t *restrict vt, char c)
{
    /* UNDONE: move this away from here!!! */
    if(c == ';') {
        vt->parser.state = VT_STATE_ATTR;
        return;
    }

    switch(c) {
        case 'A':
            vt_ansi_cux(vt, 0, -1);
            break;
        case 'B':
            vt_ansi_cux(vt, 0, +1);
            break;
        case 'C':
            vt_ansi_cux(vt, +1, 0);
            break;
        case 'D':
            vt_ansi_cux(vt, -1, 0);
            break;
        case 'E':
            vt_ansi_cnl(vt);
            break;
        case 'F':
            vt_ansi_cpl(vt);
            break;
        case 'G':
            vt_ansi_cha(vt);
            break;
        case 'H':
        case 'f':
            vt_ansi_cup(vt);
            break;
        case 'J':
            vt_ansi_ed(vt);
            break;
        case 'K':
            vt_ansi_el(vt);
            break;
        case 'S':
            vt_ansi_su(vt);
            break;
        case 'm':
            vt_ansi_sgr(vt);
            break;
        default:
            if(!vt_ansi_private(vt, c))
                VT_CALL(vt->term_pseq, vt, c);
            break;
    }

    vt->parser.state = VT_STATE_ESC;
    vt->parser.prefix = 0;
}

int vt_init(vt_t *restrict vt, size_t cwidth, size_t cheight)
{
    size_t i;

    vt->cwidth = cwidth;
    vt->cheight = cheight;
    vt->clength = cwidth * cheight;
    vt->cells = kcalloc(vt->clength, sizeof(vt_cell_t));

    if(vt->cells) {
        vt->attr.mode = VT_BG_24BIT | VT_FG_24BIT;
        vt->attr.bg = VT_DBG_COLOR;
        vt->attr.fg = VT_DFG_COLOR;

        vt->cursor.x = 0;
        vt->cursor.y = 0;
        vt->pcursor.x = 0;
        vt->pcursor.y = 0;

        vt->palette.faint[VT_BLACK] = VT_D0_BLACK;
        vt->palette.faint[VT_RED] = VT_D0_RED;
        vt->palette.faint[VT_GREEN] = VT_D0_GREEN;
        vt->palette.faint[VT_YELLOW] = VT_D0_YELLOW;
        vt->palette.faint[VT_BLUE] = VT_D0_BLUE;
        vt->palette.faint[VT_MAGENTA] = VT_D0_MAGENTA;
        vt->palette.faint[VT_CYAN] = VT_D0_CYAN;
        vt->palette.faint[VT_WHITE] = VT_D0_WHITE;

        vt->palette.normal[VT_BLACK] = VT_D1_BLACK;
        vt->palette.normal[VT_RED] = VT_D1_RED;
        vt->palette.normal[VT_GREEN] = VT_D1_GREEN;
        vt->palette.normal[VT_YELLOW] = VT_D1_YELLOW;
        vt->palette.normal[VT_BLUE] = VT_D1_BLUE;
        vt->palette.normal[VT_MAGENTA] = VT_D1_MAGENTA;
        vt->palette.normal[VT_CYAN] = VT_D1_CYAN;
        vt->palette.normal[VT_WHITE] = VT_D1_WHITE;

        vt->palette.bright[VT_BLACK] = VT_D2_BLACK;
        vt->palette.bright[VT_RED] = VT_D2_RED;
        vt->palette.bright[VT_GREEN] = VT_D2_GREEN;
        vt->palette.bright[VT_YELLOW] = VT_D2_YELLOW;
        vt->palette.bright[VT_BLUE] = VT_D2_BLUE;
        vt->palette.bright[VT_MAGENTA] = VT_D2_MAGENTA;
        vt->palette.bright[VT_CYAN] = VT_D2_CYAN;
        vt->palette.bright[VT_WHITE] = VT_D2_WHITE;

        vt->parser.state = VT_STATE_ESC;
        vt->parser.argc = 0;

        vt->cursor_color = VT_D1_WHITE;

        /* Non-concealed NUL characters result in some
        * wonky shit before the first scroll occurs. */
        for(i = 0; i < vt->clength; i++) {
            vt->cells[i].attr = vt->attr;
            vt->cells[i].attr.mode = VT_CONCEAL;
            vt->cells[i].dirty = true;
            vt->cells[i].wc = VT_NUL;
        }

        /* Make sure we are all nice and clean */
        VT_CALL(vt->draw_clear, vt, vt_get_bg(vt, &vt->attr));

        vt_redraw(vt);

        return EOK;
    }

    return ENOMEM;
}

void vt_deinit(vt_t *restrict vt)
{
    kfree(vt->cells);
    memset(vt, 0, sizeof(vt_t));
}

void vt_puts(vt_t *restrict vt, const char *restrict s)
{
    size_t i;
    size_t nmb;
    wchar_t wc;

    for(i = 0; s[i]; i++) {
        if(vt->parser.state == VT_STATE_ESC) {
            if(s[i] == VT_ESC) {
                vt->parser.state = VT_STATE_CSI;
                vt->parser.argc = 0;
                memset(vt->parser.args, 0, sizeof(vt->parser.args));
                continue;
            }
        }

        if(vt->parser.state == VT_STATE_CSI) {
            if(s[i] == VT_CSI) {
                vt->parser.state = VT_STATE_ATTR;
                continue;
            }

            if(!isspace(s[i])) {
                vt->parser.state = VT_STATE_ESC;
                vt->parser.prefix = 0;
            }
        }

        if(vt->parser.state == VT_STATE_ATTR) {
            if(strchr("<=>?", s[i])) {
                vt->parser.prefix = s[i];
                continue;
            }

            if(isdigit(s[i])) {
                vt->parser.args[vt->parser.argc] *= 10;
                vt->parser.args[vt->parser.argc] += s[i] - '0';
                continue;
            }

            if(vt->parser.argc < VT_MAX_ARGS)
                vt->parser.argc++;
            vt->parser.args[vt->parser.argc] = 0;

            if(s[i] != ';') {
                vt->parser.state = VT_STATE_ESC;
                vt_parse(vt, s[i]);
            }

            continue;
        }

        switch(s[i]) {
            case VT_BEL:
                VT_CALL(vt->term_bell, vt);
                continue;
            case VT_BS:
            case VT_DEL:
                if(vt->cursor.x > 0)
                    vt->cursor.x--;
                continue;
            case VT_HT:
                vt->cursor.x += 4 - (vt->cursor.x % 4);
                if(vt->cursor.x >= vt->cwidth)
                    vt->cursor.x = vt->cwidth - 1;
                continue;
            case VT_LF:
                vt_newline(vt, 1);
                continue;
            case VT_VT:
                vt_newline(vt, 1);
                continue;
            case VT_FF:
                vt->cursor.x = 0;
                vt->cursor.y = 0;
                vt_clear(vt, 0, 0, vt->cwidth, vt->cheight - 1);
                VT_CALL(vt->draw_clear, vt, vt_get_bg(vt, &vt->attr));
                continue;
            case VT_CR:
                vt->cursor.x = 0;
                continue;
        }

        nmb = mbtowc(&wc, &s[i], 4);
        if(nmb != SIZE_MAX) {
            vt_putwchar(vt, wc);
            i += nmb - 1;
            continue;
        }
    }

    vt_redraw(vt);
}
