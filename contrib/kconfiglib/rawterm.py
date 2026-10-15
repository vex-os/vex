#!/usr/bin/env python3

# Copyright (c) 2026 Kconfiglib contributors
# SPDX-License-Identifier: ISC

"""
rawterm -- pure-Python terminal I/O for menuconfig

NOT a curses emulation layer. A clean API designed from menuconfig's
functional requirements: rectangular screen regions, styled text output,
staged updates with frame diffing, and keyboard input including function
keys.

Zero external dependencies. Uses only Python stdlib: termios, select,
signal, shutil, os, sys, codecs, unicodedata on Unix; ctypes on Windows.

Platform support:
  - Unix (Linux, macOS): termios cbreak mode, poll(2)-based input
  - Windows 10 build 1511+: VT100 output via SetConsoleMode,
    three input paths (VT100, ReadConsoleInputW, MSYS2/mintty PTY)

Minimum: Python 3.8+, any VT100-capable terminal.
"""

import atexit
import codecs
import os
import shutil
import signal
import sys
import unicodedata

_IS_WINDOWS = os.name == "nt"

if not _IS_WINDOWS:
    import select
    import termios


# ---------------------------------------------------------------------------
# Colors
# ---------------------------------------------------------------------------


class Color:
    """Terminal color: named constant, 256-color index, or 24-bit RGB."""

    __slots__ = ("_kind", "_value")

    # kind: "default", "named", "index", "rgb"
    def __init__(self, kind, value):
        self._kind = kind
        self._value = value

    # Predefined named colors (indices 0-7 in standard palette)
    DEFAULT = None  # will be assigned below

    BLACK = None
    RED = None
    GREEN = None
    YELLOW = None
    BLUE = None
    MAGENTA = None
    CYAN = None
    WHITE = None

    # Bright variants (indices 8-15)
    BRIGHT_BLACK = None
    BRIGHT_RED = None
    BRIGHT_GREEN = None
    BRIGHT_YELLOW = None
    BRIGHT_BLUE = None
    BRIGHT_MAGENTA = None
    BRIGHT_CYAN = None
    BRIGHT_WHITE = None

    @staticmethod
    def rgb(r, g, b):
        """Create a 24-bit RGB color."""
        return Color("rgb", (r, g, b))

    @staticmethod
    def index(n):
        """Create a color from xterm 256-color palette index."""
        return Color("index", n)

    def _sgr_fg(self):
        """Return SGR escape for foreground."""
        if self._kind == "default":
            return "39"
        if self._kind == "named":
            idx = self._value
            if idx < 8:
                return str(30 + idx)
            return str(90 + idx - 8)
        if self._kind == "index":
            return f"38;5;{self._value}"
        r, g, b = self._value
        return f"38;2;{r};{g};{b}"

    def _sgr_bg(self):
        """Return SGR escape for background."""
        if self._kind == "default":
            return "49"
        if self._kind == "named":
            idx = self._value
            if idx < 8:
                return str(40 + idx)
            return str(100 + idx - 8)
        if self._kind == "index":
            return f"48;5;{self._value}"
        r, g, b = self._value
        return f"48;2;{r};{g};{b}"

    def __eq__(self, other):
        if not isinstance(other, Color):
            return NotImplemented
        return self._kind == other._kind and self._value == other._value

    def __hash__(self):
        return hash((self._kind, self._value))

    # Reverse map filled after named constants are created
    _NAMED_REPRS = {}

    def __repr__(self):
        if self._kind == "default":
            return "Color.DEFAULT"
        if self._kind == "named":
            return Color._NAMED_REPRS.get(self._value, f"Color('named', {self._value})")
        if self._kind == "index":
            return f"Color.index({self._value})"
        return "Color.rgb({},{},{})".format(*self._value)


# Initialize named color constants
Color.DEFAULT = Color("default", None)
Color.BLACK = Color("named", 0)
Color.RED = Color("named", 1)
Color.GREEN = Color("named", 2)
Color.YELLOW = Color("named", 3)
Color.BLUE = Color("named", 4)
Color.MAGENTA = Color("named", 5)
Color.CYAN = Color("named", 6)
Color.WHITE = Color("named", 7)
Color.BRIGHT_BLACK = Color("named", 8)
Color.BRIGHT_RED = Color("named", 9)
Color.BRIGHT_GREEN = Color("named", 10)
Color.BRIGHT_YELLOW = Color("named", 11)
Color.BRIGHT_BLUE = Color("named", 12)
Color.BRIGHT_MAGENTA = Color("named", 13)
Color.BRIGHT_CYAN = Color("named", 14)
Color.BRIGHT_WHITE = Color("named", 15)

# Populate reverse map for __repr__
for _attr in dir(Color):
    _obj = getattr(Color, _attr)
    if isinstance(_obj, Color) and _obj._kind == "named":
        Color._NAMED_REPRS[_obj._value] = "Color." + _attr
del _attr, _obj

# Map curses-style color names to Color constants (used by menuconfig style
# parser)
NAMED_COLORS = {
    "black": Color.BLACK,
    "red": Color.RED,
    "green": Color.GREEN,
    "yellow": Color.YELLOW,
    "blue": Color.BLUE,
    "magenta": Color.MAGENTA,
    "cyan": Color.CYAN,
    "white": Color.WHITE,
    "purple": Color.MAGENTA,
    "brightblack": Color.BRIGHT_BLACK,
    "brightred": Color.BRIGHT_RED,
    "brightgreen": Color.BRIGHT_GREEN,
    "brightyellow": Color.BRIGHT_YELLOW,
    "brightblue": Color.BRIGHT_BLUE,
    "brightmagenta": Color.BRIGHT_MAGENTA,
    "brightcyan": Color.BRIGHT_CYAN,
    "brightwhite": Color.BRIGHT_WHITE,
    "brightpurple": Color.BRIGHT_MAGENTA,
}


# ---------------------------------------------------------------------------
# Style
# ---------------------------------------------------------------------------


class Style:
    """Immutable style combining foreground, background, and attributes."""

    __slots__ = ("fg", "bg", "bold", "standout", "underline", "_sgr_cache")

    def __init__(self, fg=None, bg=None, bold=False, standout=False, underline=False):
        self.fg = fg if fg is not None else Color.DEFAULT
        self.bg = bg if bg is not None else Color.DEFAULT
        self.bold = bold
        self.standout = standout
        self.underline = underline
        self._sgr_cache = None

    def __or__(self, other):
        """Combine two styles. 'other' overrides non-default fields."""
        if not isinstance(other, Style):
            return NotImplemented
        return Style(
            fg=other.fg if other.fg != Color.DEFAULT else self.fg,
            bg=other.bg if other.bg != Color.DEFAULT else self.bg,
            bold=self.bold or other.bold,
            standout=self.standout or other.standout,
            underline=self.underline or other.underline,
        )

    def sgr(self):
        """Return the SGR escape sequence string for this style."""
        if self._sgr_cache is not None:
            return self._sgr_cache

        parts = ["0"]  # reset first
        parts.append(self.fg._sgr_fg())
        parts.append(self.bg._sgr_bg())
        if self.bold:
            parts.append("1")
        if self.underline:
            parts.append("4")
        if self.standout:
            parts.append("7")

        self._sgr_cache = "\x1b[{}m".format(";".join(parts))
        return self._sgr_cache

    def __eq__(self, other):
        if not isinstance(other, Style):
            return NotImplemented
        return (
            self.fg == other.fg
            and self.bg == other.bg
            and self.bold == other.bold
            and self.standout == other.standout
            and self.underline == other.underline
        )

    def __hash__(self):
        return hash((self.fg, self.bg, self.bold, self.standout, self.underline))

    def __repr__(self):
        parts = []
        if self.fg != Color.DEFAULT:
            parts.append(f"fg={self.fg}")
        if self.bg != Color.DEFAULT:
            parts.append(f"bg={self.bg}")
        if self.bold:
            parts.append("bold")
        if self.standout:
            parts.append("standout")
        if self.underline:
            parts.append("underline")
        return "Style({})".format(", ".join(parts))


# Default style (terminal defaults, no attributes)
STYLE_DEFAULT = Style()


# ---------------------------------------------------------------------------
# Input constants
# ---------------------------------------------------------------------------


class Key:
    """Named constants for special keys."""

    UP = "key_up"
    DOWN = "key_down"
    LEFT = "key_left"
    RIGHT = "key_right"
    PAGE_UP = "key_page_up"
    PAGE_DOWN = "key_page_down"
    HOME = "key_home"
    END = "key_end"
    BACKSPACE = "key_backspace"
    DELETE = "key_delete"
    RESIZE = "key_resize"


# ---------------------------------------------------------------------------
# Box-drawing characters (Unicode, no ACS_ needed)
# ---------------------------------------------------------------------------


class Box:
    HLINE = "\u2500"  # ─
    VLINE = "\u2502"  # │
    ULCORNER = "\u250c"  # ┌
    URCORNER = "\u2510"  # ┐
    LLCORNER = "\u2514"  # └
    LRCORNER = "\u2518"  # ┘
    LTEE = "\u251c"  # ├
    RTEE = "\u2524"  # ┤
    DARROW = "\u2193"  # ↓
    UARROW = "\u2191"  # ↑
    RARROW = "\u2192"  # →


# ---------------------------------------------------------------------------
# Character width
# ---------------------------------------------------------------------------


def _char_width(ch):
    """Return the display width of a character in terminal cells.

    - ASCII printable (0x20-0x7E): 1 cell (fast path)
    - East Asian Wide/Fullwidth: 2 cells
    - Combining marks, control chars: 0 cells
    - Everything else: 1 cell
    """
    o = ord(ch)

    # Fast path for ASCII
    if 0x20 <= o <= 0x7E:
        return 1

    # Control characters
    if o < 0x20 or o == 0x7F:
        return 0

    # Check east asian width
    eaw = unicodedata.east_asian_width(ch)
    if eaw in ("W", "F"):
        return 2

    # Combining marks
    cat = unicodedata.category(ch)
    if cat.startswith("M"):
        return 0

    return 1


def _str_width(s):
    """Return the display width of a string in terminal cells."""
    return sum(_char_width(ch) for ch in s)


# ---------------------------------------------------------------------------
# Escape sequence trie for input parsing
# ---------------------------------------------------------------------------

# Map escape sequences to Key constants. Multiple entries per key to
# handle terminal variants (xterm, rxvt, tmux, application mode).
_ESCAPE_SEQUENCES = {
    # Arrow keys
    "\x1b[A": Key.UP,
    "\x1bOA": Key.UP,  # application mode
    "\x1b[B": Key.DOWN,
    "\x1bOB": Key.DOWN,
    "\x1b[C": Key.RIGHT,
    "\x1bOC": Key.RIGHT,
    "\x1b[D": Key.LEFT,
    "\x1bOD": Key.LEFT,
    # Page Up / Page Down
    "\x1b[5~": Key.PAGE_UP,
    "\x1b[6~": Key.PAGE_DOWN,
    # Home
    "\x1b[H": Key.HOME,  # xterm
    "\x1bOH": Key.HOME,  # application mode
    "\x1b[1~": Key.HOME,  # tmux/linux
    "\x1b[7~": Key.HOME,  # rxvt
    # End
    "\x1b[F": Key.END,  # xterm
    "\x1bOF": Key.END,  # application mode
    "\x1b[4~": Key.END,  # tmux/linux
    "\x1b[8~": Key.END,  # rxvt
    # Delete
    "\x1b[3~": Key.DELETE,
}


def _build_trie(sequences):
    """Build a trie (nested dict) from escape sequence table."""
    root = {}
    for seq, key in sequences.items():
        node = root
        for ch in seq[:-1]:
            if ch not in node:
                node[ch] = {}
            node = node[ch]
        node[seq[-1]] = key
    return root


_ESCAPE_TRIE = _build_trie(_ESCAPE_SEQUENCES)


# ---------------------------------------------------------------------------
# Region -- rectangular cell buffer
# ---------------------------------------------------------------------------


class Region:
    """Rectangular cell buffer with position, size, and dirty tracking.

    Created via terminal.region(), not standalone constructor.
    """

    def __init__(self, terminal, height, width, y, x):
        self._terminal = terminal
        self._height = height
        self._width = width
        self._y = y
        self._x = x
        self._dirty = True
        self._fill_style = STYLE_DEFAULT
        # Cell buffer: list of rows, each row is list of (char, style) tuples
        self._cells = self._make_cells(height, width)

    def _make_cells(self, height, width):
        default = (" ", self._fill_style)
        return [[default] * width for _ in range(height)]

    @property
    def height(self):
        return self._height

    @property
    def width(self):
        return self._width

    @property
    def y(self):
        return self._y

    @property
    def x(self):
        return self._x

    def close(self):
        """Unregister from compositor."""
        if self._terminal:
            try:
                self._terminal._regions.remove(self)
            except ValueError:
                pass
            if self._terminal._cursor_region is self:
                self._terminal._cursor_region = None
            self._terminal = None

    def resize(self, height, width):
        """Resize the region, clearing its contents."""
        self._height = height
        self._width = width
        self._cells = self._make_cells(height, width)
        self._dirty = True

    def move(self, y, x):
        """Move the region to a new position."""
        self._y = y
        self._x = x
        self._dirty = True

    def clear(self):
        """Clear the region to spaces using the stored fill style."""
        self.fill(self._fill_style)

    def fill(self, style):
        """Set background style for entire region and remember it for clear()."""
        self._fill_style = style
        cell = (" ", style)
        for row in self._cells:
            row[:] = [cell] * len(row)
        self._dirty = True

    def write(self, y, x, text, style=None, max_len=None):
        """Write text at (y, x) with optional style. Clips to region bounds.

        Returns number of cells written (useful for cursor positioning
        after CJK characters).
        """
        if style is None:
            style = STYLE_DEFAULT
        if y < 0 or y >= self._height or x >= self._width:
            return 0

        text = text.expandtabs()
        col = x
        cells_written = 0

        for ch in text:
            if max_len is not None and cells_written >= max_len:
                break

            w = _char_width(ch)
            if w == 0:
                continue

            if col < 0:
                col += w
                continue

            if col + w > self._width:
                # Wide char would overflow -- stop
                break

            self._cells[y][col] = (ch, style)
            cells_written += w

            # For wide (CJK) chars, fill the second cell with a placeholder
            if w == 2 and col + 1 < self._width:
                self._cells[y][col + 1] = ("", style)

            col += w

        self._dirty = True
        return cells_written

    def write_char(self, y, x, char, style=None):
        """Write a single character at (y, x)."""
        if style is None:
            style = STYLE_DEFAULT
        if y < 0 or y >= self._height or x < 0 or x >= self._width:
            return

        w = _char_width(char)
        if x + w > self._width:
            return

        self._cells[y][x] = (char, style)
        if w == 2 and x + 1 < self._width:
            self._cells[y][x + 1] = ("", style)

        self._dirty = True

    def getyx(self):
        """Compatibility: return (y, x) position. Always (0, 0) for regions."""
        return (0, 0)


# ---------------------------------------------------------------------------
# Terminal
# ---------------------------------------------------------------------------


class Terminal:
    """Manages terminal state, screen compositing, and input."""

    def __init__(self):
        if not _IS_WINDOWS:
            if not os.isatty(sys.stdin.fileno()):
                raise RuntimeError("stdin is not a terminal")
            if not os.isatty(sys.stdout.fileno()):
                raise RuntimeError("stdout is not a terminal")

        self._regions = []
        self._cursor_region = None
        self._cursor_y = 0
        self._cursor_x = 0
        self._cursor_visible = False
        self._cursor_very_visible = False
        self._suspended = False
        self._resize_pending = False
        self._prev_frame = None  # Previous frame for diffing

        # Escape sequence parser state (shared by Unix and Windows VT paths)
        self._esc_buf = []
        self._esc_node = None
        self._pending_key = None

        # Query initial terminal size
        sz = shutil.get_terminal_size()
        self._width = sz.columns
        self._height = sz.lines

        # Platform-specific init
        if _IS_WINDOWS:
            self._init_windows()
        else:
            self._init_unix()

        # Enter alternate screen
        self._write_raw("\x1b[?1049h")
        # Hide cursor by default
        self._write_raw("\x1b[?25l")
        self._flush()

    @staticmethod
    def _set_cbreak():
        """Apply cbreak terminal settings: no echo, no canonical mode."""
        fd = sys.stdin.fileno()
        new = termios.tcgetattr(fd)
        # LFLAG: clear ICANON, ECHO, IEXTEN; keep ISIG for Ctrl-C
        new[3] &= ~(termios.ICANON | termios.ECHO | termios.IEXTEN)
        # IFLAG: clear IXON, IXOFF, ICRNL, INLCR, IGNCR
        new[1] &= ~(
            termios.IXON | termios.IXOFF | termios.ICRNL | termios.INLCR | termios.IGNCR
        )
        # Set VMIN=1 (Solaris: VMIN shares slot with VEOF)
        new[6][termios.VMIN] = 1
        new[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, new)

    def _init_unix(self):
        """Set up Unix terminal: cbreak mode, SIGWINCH."""
        self._old_termios = termios.tcgetattr(sys.stdin.fileno())
        self._set_cbreak()

        # UTF-8 incremental decoder for input
        self._decoder = codecs.getincrementaldecoder("utf-8")("replace")

        # SIGWINCH handler
        self._old_sigwinch = signal.getsignal(signal.SIGWINCH)
        signal.signal(signal.SIGWINCH, self._sigwinch_handler)

        # Poller for input readability checks
        self._poller = select.poll()
        self._poller.register(sys.stdin.fileno(), select.POLLIN)

    def _init_windows(self):
        """Set up Windows terminal: VT100 output, console input."""
        import ctypes
        from ctypes import wintypes

        kernel32 = ctypes.windll.kernel32

        # Get handles
        STD_INPUT_HANDLE = -10
        STD_OUTPUT_HANDLE = -11
        self._stdin_handle = kernel32.GetStdHandle(STD_INPUT_HANDLE)
        self._stdout_handle = kernel32.GetStdHandle(STD_OUTPUT_HANDLE)

        # Save original console modes
        self._old_out_mode = wintypes.DWORD()
        kernel32.GetConsoleMode(self._stdout_handle, ctypes.byref(self._old_out_mode))
        self._old_in_mode = wintypes.DWORD()
        kernel32.GetConsoleMode(self._stdin_handle, ctypes.byref(self._old_in_mode))

        # Enable VT100 output.  DISABLE_NEWLINE_AUTO_RETURN (0x0008)
        # prevents the console from inserting a CR before every LF,
        # which causes cursor-positioning drift in TUI apps.  Microsoft
        # recommends setting both flags together for VT100 applications.
        #
        # Graceful fallback: if the combination is rejected (pre-1607
        # builds), retry with VT100 alone.
        ENABLE_VIRTUAL_TERMINAL_PROCESSING = 0x0004
        DISABLE_NEWLINE_AUTO_RETURN = 0x0008
        new_out = (
            self._old_out_mode.value
            | ENABLE_VIRTUAL_TERMINAL_PROCESSING
            | DISABLE_NEWLINE_AUTO_RETURN
        )
        if not kernel32.SetConsoleMode(self._stdout_handle, new_out):
            # Pre-1607: DISABLE_NEWLINE_AUTO_RETURN unsupported, VT100 only
            new_out = self._old_out_mode.value | ENABLE_VIRTUAL_TERMINAL_PROCESSING
            kernel32.SetConsoleMode(self._stdout_handle, new_out)

        # Try VT100 input
        ENABLE_VIRTUAL_TERMINAL_INPUT = 0x0200
        self._win_vt_input = False
        new_in = (self._old_in_mode.value | ENABLE_VIRTUAL_TERMINAL_INPUT) & ~(
            0x0004 | 0x0002 | 0x0001
        )  # clear ECHO, LINE, PROCESSED
        if kernel32.SetConsoleMode(self._stdin_handle, new_in):
            self._win_vt_input = True
            self._decoder = codecs.getincrementaldecoder("utf-8")("replace")
        else:
            # Fall back to ReadConsoleInputW
            new_in = self._old_in_mode.value & ~(0x0004 | 0x0002 | 0x0001)
            new_in |= 0x0008  # ENABLE_WINDOW_INPUT
            kernel32.SetConsoleMode(self._stdin_handle, new_in)

        self._kernel32 = kernel32

    def close(self):
        """Restore terminal state."""
        # Show cursor
        self._write_raw("\x1b[?25h")
        # Leave alternate screen
        self._write_raw("\x1b[?1049l")
        # Reset attributes
        self._write_raw("\x1b[0m")
        self._flush()

        if _IS_WINDOWS:
            self._kernel32.SetConsoleMode(self._stdout_handle, self._old_out_mode)
            self._kernel32.SetConsoleMode(self._stdin_handle, self._old_in_mode)
        else:
            # Restore termios
            fd = sys.stdin.fileno()
            termios.tcsetattr(fd, termios.TCSANOW, self._old_termios)
            # Restore SIGWINCH handler
            signal.signal(signal.SIGWINCH, self._old_sigwinch)

    @property
    def width(self):
        return self._width

    @property
    def height(self):
        return self._height

    def region(self, height, width, y=0, x=0):
        """Create and register a new Region."""
        r = Region(self, height, width, y, x)
        self._regions.append(r)
        return r

    def hide_cursor(self):
        self._cursor_visible = False
        self._cursor_very_visible = False

    def show_cursor(self, very_visible=False):
        self._cursor_visible = True
        self._cursor_very_visible = very_visible

    def set_cursor(self, region, y, x):
        """Position cursor in a region (for edit fields)."""
        self._cursor_region = region
        self._cursor_y = y
        self._cursor_x = x

    def suspend(self):
        """Temporarily exit for stderr output."""
        self._suspended = True
        # Leave alternate screen, show cursor, restore terminal
        self._write_raw("\x1b[?25h")
        self._write_raw("\x1b[?1049l")
        self._write_raw("\x1b[0m")
        self._flush()

        if not _IS_WINDOWS:
            fd = sys.stdin.fileno()
            termios.tcsetattr(fd, termios.TCSANOW, self._old_termios)

    def resume(self):
        """Re-enter terminal mode after suspend."""
        if not _IS_WINDOWS:
            self._set_cbreak()

        # Re-enter alternate screen
        self._write_raw("\x1b[?1049h")
        if not self._cursor_visible:
            self._write_raw("\x1b[?25l")
        self._flush()

        # Requery terminal size
        sz = shutil.get_terminal_size()
        old_w, old_h = self._width, self._height
        self._width = sz.columns
        self._height = sz.lines

        # Force full repaint
        self._prev_frame = None
        for r in self._regions:
            r._dirty = True

        self._suspended = False

        # Queue resize if dimensions changed during suspend
        if old_w != self._width or old_h != self._height:
            self._resize_pending = True

    def _sigwinch_handler(self, signum, frame):
        """SIGWINCH: set flag, don't resize mid-render."""
        self._resize_pending = True

    def _check_resize(self):
        """Check and handle pending resize."""
        if self._resize_pending:
            self._resize_pending = False
            sz = shutil.get_terminal_size()
            self._width = sz.columns
            self._height = sz.lines
            self._prev_frame = None  # force full repaint
            # Clear screen so full repaint starts from clean slate.
            # Terminal emulators may garble the alternate screen on resize.
            self._write_raw("\x1b[2J")
            self._flush()
            return True
        return False

    # --- Output ---

    def _write_raw(self, s):
        """Append raw string to output. Caller must call _flush()."""
        # We write to stdout.buffer for binary safety
        try:
            sys.stdout.buffer.write(s.encode("utf-8"))
        except OSError:
            pass

    def _flush(self):
        """Flush stdout.

        Terminal fds are always blocking, so the non-blocking dance is
        purely defensive.  Each step (get_blocking, set_blocking, flush,
        restore) is isolated so that a failure in the probe never
        prevents the flush from running.

        os.get_blocking raises AttributeError when unavailable (Windows
        Python <3.12) and OSError on Windows console handles even when
        present, so we catch both and default to was_blocking=True
        (skip the set_blocking detour).
        """
        was_blocking = True
        fd = None
        try:
            fd = sys.stdout.fileno()
        except OSError:
            pass

        if fd is not None:
            try:
                was_blocking = os.get_blocking(fd)
            except (AttributeError, OSError):
                was_blocking = True

            if not was_blocking:
                try:
                    os.set_blocking(fd, True)
                except OSError:
                    pass

        try:
            sys.stdout.buffer.flush()
        except OSError:
            pass
        finally:
            if fd is not None and not was_blocking:
                try:
                    os.set_blocking(fd, False)
                except OSError:
                    pass

    def update(self):
        """Composite all regions and flush to terminal.

        Painter's algorithm: render regions back-to-front by registration
        order. Frame diffing: only emit ANSI for changed cells.
        """
        if self._suspended:
            return

        h = self._height
        w = self._width

        # Build current frame: 2D array of (char, style)
        default_cell = (" ", STYLE_DEFAULT)
        frame = [[default_cell] * w for _ in range(h)]

        # Painter's algorithm: paint regions in order (later = on top)
        for region in self._regions:
            ry, rx = region._y, region._x
            # Clamp visible row/col ranges to screen bounds
            row_start = max(0, -ry)
            row_end = min(region._height, h - ry)
            col_start = max(0, -rx)
            col_end = min(region._width, w - rx)
            for row in range(row_start, row_end):
                frame_row = frame[ry + row]
                region_row = region._cells[row]
                for col in range(col_start, col_end):
                    cell = region_row[col]
                    if cell[0] != "":  # skip wide-char placeholders
                        frame_row[rx + col] = cell

        # Diff against previous frame and emit changes
        buf = []
        prev = self._prev_frame

        last_style = None
        last_row = -1
        last_col = -1

        for row in range(h):
            for col in range(w):
                cell = frame[row][col]
                if prev and prev[row][col] == cell:
                    continue

                ch, style = cell

                # Move cursor if not contiguous
                if row != last_row or col != last_col:
                    buf.append(f"\x1b[{row + 1};{col + 1}H")

                # Set style if changed
                if style != last_style:
                    buf.append(style.sgr())
                    last_style = style

                buf.append(ch or " ")
                cw = _char_width(ch) if ch else 1
                last_row = row
                last_col = col + cw

        # Handle cursor visibility and position
        if self._cursor_visible and self._cursor_region:
            r = self._cursor_region
            abs_y = r._y + self._cursor_y
            abs_x = r._x + self._cursor_x
            buf.append(f"\x1b[{abs_y + 1};{abs_x + 1}H")
            if self._cursor_very_visible:
                buf.append("\x1b[?12;25h")  # blink + show
            else:
                buf.append("\x1b[?12l\x1b[?25h")  # no blink + show
        elif not self._cursor_visible:
            buf.append("\x1b[?25l")

        if buf:
            self._write_raw("".join(buf))
            self._flush()

        self._prev_frame = frame

    # --- Input ---

    def read_key(self):
        """Block and return next key event: Key constant or str character.

        Esc returned as "\\x1b" (after timeout to disambiguate from
        escape sequences). Regular characters returned as str.
        """
        if self._suspended:
            raise RuntimeError("terminal is suspended")

        # Return any key buffered from a previous escape dead-end
        if self._pending_key is not None:
            pending = self._pending_key
            self._pending_key = None
            return pending

        # Check for pending resize first
        if self._check_resize():
            return Key.RESIZE

        if _IS_WINDOWS:
            return self._read_key_windows()
        return self._read_key_unix()

    def _read_key_unix(self):
        """Read key on Unix using os.read + escape parser."""
        fd = sys.stdin.fileno()

        while True:
            try:
                data = os.read(fd, 1024)
            except OSError:
                # EINTR from SIGWINCH
                if self._check_resize():
                    return Key.RESIZE
                continue

            if not data:
                continue

            chars = self._decoder.decode(data)

            for ch in chars:
                result = self._feed_escape(ch)
                if result is not None:
                    return result

            # If we have a partial escape sequence, wait briefly for more
            if self._esc_buf:
                if self._poller.poll(25):
                    # More data available, keep reading
                    continue
                # Timeout -- flush escape buffer
                result = self._flush_escape()
                if result is not None:
                    return result

            # Check resize
            if self._check_resize():
                return Key.RESIZE

    def _feed_escape(self, ch):
        """Feed a character to the escape sequence parser.

        Returns a Key constant or str if a complete key was parsed,
        or None if more input is needed.
        """
        if self._esc_node is not None:
            # We're in the middle of an escape sequence
            if ch not in self._esc_node:
                # Dead end -- flush first (before _feed_char can
                # overwrite _esc_buf when ch is ESC), then buffer ch
                result = self._flush_escape()
                self._pending_key = self._feed_char(ch)
                return result

            val = self._esc_node[ch]
            if isinstance(val, dict):
                # More characters expected
                self._esc_buf.append(ch)
                self._esc_node = val
                return None

            # Complete match
            self._esc_buf = []
            self._esc_node = None
            return val

        return self._feed_char(ch)

    def _feed_char(self, ch):
        """Process a non-escape-sequence character."""
        if ch == "\x1b":
            # Start of potential escape sequence
            self._esc_buf = [ch]
            self._esc_node = _ESCAPE_TRIE["\x1b"]
            return None

        if ch == "\x7f":
            return Key.BACKSPACE

        # Normalize CR to LF so callers can check "\n" for Enter.
        # On Unix, ICRNL is cleared for raw input; on Windows,
        # msvcrt.getwch() and ReadConsoleInputW both return "\r".
        if ch == "\r":
            return "\n"

        return ch

    def _flush_escape(self):
        """Flush partial escape sequence buffer, returning first char."""
        if not self._esc_buf:
            return None

        # The first char is always ESC
        self._esc_buf = []
        self._esc_node = None
        return "\x1b"

    def _read_key_windows(self):
        """Read key on Windows."""
        if self._win_vt_input:
            return self._read_key_windows_vt()
        return self._read_key_windows_console()

    def _read_key_windows_vt(self):
        """Windows VT100 input mode -- reuse Unix escape parser."""
        import msvcrt
        import time

        while True:
            if msvcrt.kbhit():
                ch = msvcrt.getwch()
                result = self._feed_escape(ch)
                if result is not None:
                    return result
            else:
                if self._esc_buf:
                    result = self._flush_escape()
                    if result is not None:
                        return result
                if self._check_resize():
                    return Key.RESIZE
                # Small sleep to avoid busy-wait
                time.sleep(0.01)

    def _read_key_windows_console(self):
        """Windows console input via ReadConsoleInputW."""
        import ctypes
        from ctypes import wintypes

        kernel32 = self._kernel32

        # VK code to Key constant mapping
        VK_MAP = {
            33: Key.PAGE_UP,
            34: Key.PAGE_DOWN,
            35: Key.END,
            36: Key.HOME,
            37: Key.LEFT,
            38: Key.UP,
            39: Key.RIGHT,
            40: Key.DOWN,
            46: Key.DELETE,
        }

        class KEY_EVENT_RECORD(ctypes.Structure):
            _fields_ = [
                ("bKeyDown", wintypes.BOOL),
                ("wRepeatCount", wintypes.WORD),
                ("wVirtualKeyCode", wintypes.WORD),
                ("wVirtualScanCode", wintypes.WORD),
                ("uChar", wintypes.WCHAR),
                ("dwControlKeyState", wintypes.DWORD),
            ]

        class INPUT_RECORD(ctypes.Structure):
            class _Event(ctypes.Union):
                _fields_ = [("KeyEvent", KEY_EVENT_RECORD)]

            _fields_ = [
                ("EventType", wintypes.WORD),
                ("Event", _Event),
            ]

        KEY_EVENT = 0x0001
        WINDOW_BUFFER_SIZE_EVENT = 0x0004

        ir = INPUT_RECORD()
        n_read = wintypes.DWORD()

        while True:
            kernel32.ReadConsoleInputW(
                self._stdin_handle, ctypes.byref(ir), 1, ctypes.byref(n_read)
            )

            if ir.EventType == KEY_EVENT:
                ke = ir.Event.KeyEvent
                if not ke.bKeyDown:
                    continue

                vk = ke.wVirtualKeyCode
                ch = ke.uChar

                if vk in VK_MAP:
                    return VK_MAP[vk]

                if vk == 8:  # VK_BACK
                    return Key.BACKSPACE

                if vk == 27:  # VK_ESCAPE
                    return "\x1b"

                if ch and ch != "\0":
                    return self._feed_char(ch)

            elif ir.EventType == WINDOW_BUFFER_SIZE_EVENT:
                self._resize_pending = True
                if self._check_resize():
                    return Key.RESIZE


# ---------------------------------------------------------------------------
# Safe entry point
# ---------------------------------------------------------------------------


def run(fn):
    """Safe wrapper: init terminal, call fn(terminal), restore on exit.

    Catches KeyboardInterrupt (Ctrl-C via SIGINT) and always restores
    terminal state.
    """
    term = None
    try:
        term = Terminal()
        # Register atexit as safety net
        atexit.register(lambda: term.close() if term else None)
        return fn(term)
    except KeyboardInterrupt:
        pass
    finally:
        if term:
            term.close()
