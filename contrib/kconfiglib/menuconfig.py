#!/usr/bin/env python3

# Copyright (c) 2018-2020 Nordic Semiconductor ASA and Ulf Magnusson
# Copyright (c) 2024-2026 Kconfiglib contributors
# SPDX-License-Identifier: ISC

"""
Overview
========

A terminal-based menuconfig implementation using rawterm (pure-Python terminal
I/O). The interface mimics the mconf ('make menuconfig' in the Linux kernel) as
closely as possible, using the same key bindings.

Key bindings match mconf:

  Arrow keys      : Navigate menu items
  PgUp/PgDn       : Page up/down (also Ctrl-D/Ctrl-U)
  Home/End         : Jump to beginning/end of list
  Space            : Toggle value or enter menu
  Enter            : Activate focused button
  Tab/Left/Right   : Cycle between buttons
  Y/S              : Set symbol to YES
  N                : Set symbol to NO
  M                : Set symbol to MODULE
  H/?              : Show help for selected item
  /                : Jump-to symbol search
  ESC/Backspace    : Leave menu (quit dialog at top level)
  E/X              : Leave menu (same as ESC in submenus)
  Q                : Quit

The bottom of the dialog shows five buttons: Select, Exit, Help, Save, Load.
[Tab]/[Left]/[Right] cycle between buttons. [Enter] activates the focused
button. [Space] toggles values if possible, and enters menus otherwise.

VI-style navigation keys (j/k/l/g/G) are not bound, leaving them free for
future hotkey search (matching mconf).  H is rebound to help (matching mconf).

A jump-to feature for jumping directly to any symbol (including invisible
symbols), choice, menu or comment (as in a Kconfig 'comment "Foo"') is
available via the '/' key.

A few different modes are available:

  F: Toggle show-help mode, which shows the help text of the currently selected
  item in the window at the bottom of the menu display. This is handy when
  browsing through options.

  C: Toggle show-name mode, which shows the symbol name before each symbol menu
  entry

  A/Z: Toggle show-all mode, which shows all items, including currently
  invisible items and items that lack a prompt. Invisible items are drawn in a
  different style to make them stand out.


Running
=======

menuconfig.py can be run either as a standalone executable or by calling the
menuconfig() function with an existing Kconfig instance. The second option is a
bit inflexible in that it will still load and save .config, etc.

When run in standalone mode, the top-level Kconfig file to load can be passed
as a command-line argument. With no argument, it defaults to "Kconfig".

The KCONFIG_CONFIG environment variable specifies the .config file to load (if
it exists) and save. If KCONFIG_CONFIG is unset, ".config" is used.

When overwriting a configuration file, the old version is saved to
<filename>.old (e.g. .config.old).

$srctree is supported through Kconfiglib.


Color schemes
=============

The default color scheme matches the Linux kernel's menuconfig (mconf/lxdialog).
It is possible to customize the color scheme by setting the MENUCONFIG_STYLE
environment variable.

This is the current list of built-in styles:
    - linux         style matching the Linux kernel's menuconfig (mconf/lxdialog) [default]
    - monochrome    colorless theme (uses only bold and standout) attributes,
                    this style is used if the terminal doesn't support colors

It is possible to customize the current style by changing colors of UI
elements on the screen. This is the list of elements that can be stylized:

    - screen        Full-screen background behind the dialog
    - path          Top row in the main display, with the menu path
    - separator     Separator lines between windows. Also used for the top line
                    in the symbol information display.
    - list          List of items, e.g. the main display
    - selection     Style for the selected item
    - inv-list      Like list, but for invisible items. Used in show-all mode.
    - inv-selection Like selection, but for invisible items. Used in show-all
                    mode.
    - help          Help text windows at the bottom of various fullscreen
                    dialogs
    - show-help     Window showing the help text in show-help mode
    - frame         Frame around pop-up dialog boxes
    - body          Body of the main dialog
    - edit          Edit box in pop-up dialogs
    - jump-edit     Edit box in jump-to dialog
    - text          Symbol information text
    - title         Dialog title text
    - border        Border of the main dialog box
    - menubox       Inner menu box border (left/top edges)
    - menubox-border Inner menu box border (right/bottom edges and fill)

The color definition is a comma separated list of attributes:

    - fg:COLOR      Set the foreground/background colors. COLOR can be one of
      * or *        the basic 16 colors (black, red, green, yellow, blue,
    - bg:COLOR      magenta, cyan, white and brighter versions, for example,
                    brightred). On terminals that support more than 8 colors,
                    you can also directly put in a color number, e.g. fg:123
                    (hexadecimal and octal constants are accepted as well).
                    Colors outside the range -1..255 are ignored (with a
                    warning). The COLOR can be also specified using a RGB
                    value in the HTML notation, for example #RRGGBB. The
                    color is rendered using the closest available
                    representation for the terminal.

                    If the background or foreground color of an element is not
                    specified, it defaults to the terminal default color.

                    Note: On some terminals a bright version of the color
                    implies bold.
    - bold          Use bold text
    - underline     Use underline text
    - standout      Standout text attribute (reverse color)

More often than not, some UI elements share the same color definition. In such
cases the right value may specify an UI element from which the color definition
will be copied. For example, "separator=help" will apply the current color
definition for "help" to "separator".

A keyword without the '=' is assumed to be a style template. The template name
is looked up in the built-in styles list and the style definition is expanded
in-place. With this, built-in styles can be used as basis for new styles.

For example, take the linux theme and give it a red selection bar:

MENUCONFIG_STYLE="linux selection=fg:white,bg:red"

If there's an error in the style definition or if a missing style is assigned
to, the assignment will be ignored, along with a warning being printed on
stderr.

The 'linux' theme is always implicitly parsed first, so the following two
settings have the same effect:

    MENUCONFIG_STYLE="selection=fg:white,bg:red"
    MENUCONFIG_STYLE="linux selection=fg:white,bg:red"

If the terminal doesn't support colors, the 'monochrome' theme is used, and
MENUCONFIG_STYLE is ignored.


Other features
==============

  - Seamless terminal resizing

  - No curses dependency -- uses rawterm, a pure-Python terminal I/O module.
    Works on Unix and Windows 10+ without any pip install.

  - Unicode text entry

  - Improved information screen compared to mconf:

      * Expressions are split up by their top-level &&/|| operands to improve
        readability

      * Undefined symbols in expressions are pointed out

      * Menus and comments have information displays

      * Kconfig definitions are printed

      * The include path is shown, listing the locations of the 'source'
        statements that included the Kconfig file of the symbol (or other
        item)
"""

import os
import sys

_IS_WINDOWS = os.name == "nt"  # Are we running on Windows?

import errno
import locale
import re
import textwrap

if __package__:
    from . import rawterm, uicommon
    from .rawterm import Key, Box, Style, Color, NAMED_COLORS
else:
    import rawterm
    import uicommon
    from rawterm import Key, Box, Style, Color, NAMED_COLORS

from kconfiglib import (
    Symbol,
    Choice,
    MENU,
    COMMENT,
    MenuNode,
    BOOL,
    TRISTATE,
    STRING,
    INT,
    HEX,
    AND,
    OR,
    expr_value,
    split_expr,
    standard_sc_expr_str,
    TRI_TO_STR,
    TYPE_TO_STR,
    standard_kconfig,
    standard_config_filename,
    _needs_save as _kconf_needs_save,
    _extract_controlling_symbols,
)

#
# Configuration variables
#

# If True, try to change LC_CTYPE to a UTF-8 locale if it is set to the C
# locale (which implies ASCII). This fixes Unicode I/O issues on systems with
# bad defaults.
#
# Related PEP: https://www.python.org/dev/peps/pep-0538/
_CHANGE_C_LC_CTYPE_TO_UTF8 = True

# How many steps an implicit submenu will be indented. Implicit submenus are
# created when an item depends on the symbol before it. Note that symbols
# defined with 'menuconfig' create a separate menu instead of indenting.
_SUBMENU_INDENT = 4

# Number of steps for Page Up/Down to jump
_PG_JUMP = 6

# Height of the help window in show-help mode
_SHOW_HELP_HEIGHT = 8

# How far the cursor needs to be from the edge of the window before it starts
# to scroll. Used for the main menu display, the information display, the
# search display, and for text boxes.
_SCROLL_OFFSET = 5

# Minimum width of dialogs that ask for text input
_INPUT_DIALOG_MIN_WIDTH = 30

# Number of arrows pointing up/down to draw when a window is scrolled
_N_SCROLL_ARROWS = 14

# Instruction text shown inside the mconf-style dialog, above the menu box.
# Matches the menu_instructions[] string in mconf/mconf.c.
_MENU_INSTRUCTIONS = (
    "Arrow keys navigate the menu.  "
    "<Enter> selects submenus ---> (or empty submenus ----).  "
    "Highlighted letters are hotkeys.  "
    "Pressing <Y> includes, <N> excludes, <M> modularizes features.  "
    "Press <Esc><Esc> to exit, <?> for Help, </> for Search.  "
    "Legend: [*] built-in  [ ] excluded  <M> module  < > module capable"
)

# Button labels for the main menu (matching mconf/lxdialog/menubox.c
# print_buttons()).  Spaces in labels provide visual padding.
_MENU_BUTTONS = ["Select", " Exit ", " Help ", " Save ", " Load "]

# Lines of help text shown at the bottom of the information dialog
_INFO_HELP_LINES = """
[ESC/q] Return to menu      [/] Jump to symbol
"""[1:-1].split("\n")

# Lines of help text shown at the bottom of the search dialog
_JUMP_TO_HELP_LINES = """
Type text to narrow the search. Regexes are supported (via Python's 're'
module). The up/down cursor keys step in the list. [Enter] jumps to the
selected symbol. [ESC] aborts the search. Type multiple space-separated
strings/regexes to find entries that match all of them. Type Ctrl-F to
view the help of the selected item without leaving the dialog.
"""[1:-1].split("\n")

#
# Styling
#

_STYLES = {
    # Style matching the Linux kernel's menuconfig (mconf/lxdialog).
    # Matches lxdialog's set_bluetitle_theme() (the default theme).
    "linux": """
    screen=fg:cyan,bg:blue,bold
    path=fg:white,bg:blue,bold
    separator=fg:white,bg:blue
    list=fg:black,bg:white
    selection=fg:white,bg:blue,bold
    inv-list=fg:red,bg:white
    inv-selection=fg:red,bg:blue,bold
    help=fg:white,bg:blue
    show-help=fg:black,bg:white
    frame=fg:white,bg:blue,bold
    body=fg:black,bg:white
    edit=fg:black,bg:white
    jump-edit=fg:black,bg:white
    text=fg:black,bg:white
    title=fg:blue,bg:white,bold
    border=fg:white,bg:white,bold
    menubox=fg:black,bg:white
    menubox-border=fg:white,bg:white,bold
    shadow=fg:black,bg:black,bold
    button-active=fg:white,bg:blue,bold
    button-inactive=fg:black,bg:white
    button-key-active=fg:yellow,bg:blue,bold
    button-key-inactive=fg:red,bg:white
    button-label-active=fg:white,bg:blue,bold
    button-label-inactive=fg:black,bg:white,bold
    dialog=fg:black,bg:white
    dialog-frame=fg:white,bg:blue,bold
    """,
    # This style is forced on terminals that do not support colors
    "monochrome": """
    screen=bold
    path=bold
    separator=bold,standout
    list=
    selection=bold,standout
    inv-list=bold
    inv-selection=bold,standout
    help=bold
    show-help=
    frame=bold,standout
    body=
    edit=standout
    jump-edit=
    text=
    title=bold
    border=bold
    menubox=
    menubox-border=bold
    """,
}


def _parse_color(color_def):
    """Parse a color definition string, returning a rawterm.Color."""
    # HTML format, #RRGGBB
    if re.match("^#[A-Fa-f0-9]{6}$", color_def):
        return Color.rgb(
            int(color_def[1:3], 16),
            int(color_def[3:5], 16),
            int(color_def[5:7], 16),
        )

    if color_def in NAMED_COLORS:
        return NAMED_COLORS[color_def]

    try:
        num = int(color_def, 0)
        if 0 <= num <= 255:
            return Color.index(num)
        _warn(f"Ignoring color {color_def} outside range 0..255")
        return Color.DEFAULT
    except ValueError:
        _warn("Ignoring color", color_def, "that's neither predefined nor a number")
        return Color.DEFAULT


def _style_from_def(style_def):
    """Parse a style definition string, returning a rawterm.Style."""
    fg = Color.DEFAULT
    bg = Color.DEFAULT
    bold = False
    standout = False
    underline = False

    if style_def:
        for field in style_def.split(","):
            if field.startswith("fg:"):
                fg = _parse_color(field.split(":", 1)[1])
            elif field.startswith("bg:"):
                bg = _parse_color(field.split(":", 1)[1])
            elif field == "bold":
                bold = not _IS_WINDOWS
            elif field == "standout":
                standout = True
            elif field == "underline":
                underline = True
            else:
                _warn("Ignoring unknown style attribute", field)

    return Style(fg=fg, bg=bg, bold=bold, standout=standout, underline=underline)


def _parse_style(style_str, parsing_default):
    # Parses a string with '<element>=<style>' assignments. Anything not
    # containing '=' is assumed to be a reference to a built-in style, which is
    # treated as if all the assignments from the style were inserted at that
    # point in the string.
    #
    # The parsing_default flag is set to True when we're implicitly parsing the
    # 'default'/'monochrome' style, to prevent warnings.

    for sline in style_str.split():
        if "=" in sline:
            key, data = sline.split("=", 1)

            if key not in _style and not parsing_default:
                _warn("Ignoring non-existent style", key)

            # If data is a reference to another key, copy its style
            if data in _style:
                _style[key] = _style[data]
            else:
                _style[key] = _style_from_def(data)

        elif sline in _STYLES:
            _parse_style(_STYLES[sline], parsing_default)

        else:
            _warn("Ignoring non-existent style template", sline)


# Dictionary mapping element names to rawterm.Style objects
_style = {}


def _init_styles():
    # Use the 'linux' theme as the base, and add any user-defined style
    # settings from the environment
    _parse_style("linux", True)
    if "MENUCONFIG_STYLE" in os.environ:
        _parse_style(os.environ["MENUCONFIG_STYLE"], False)


#
# Main application
#


def _main():
    menuconfig(standard_kconfig(__doc__))


def menuconfig(kconf, headless=False):
    """
    Launches the configuration interface, returning after the user exits.

    kconf:
      Kconfig instance to be configured

    headless:
      If True, run in headless mode without launching the terminal interface.
      This is useful for testing, CI/CD pipelines, and automated configuration
      processing. In headless mode, the function only loads the configuration
      and returns immediately without user interaction.
    """
    global _s

    # Start from clean state. A second run in the same process would
    # otherwise inherit the first run's scroll position, dialog state and
    # show-all mode.
    _s = _State()
    _s.kconf = kconf

    # Filename to save configuration to
    _s.conf_filename = standard_config_filename()

    # Load existing configuration and set _s.conf_changed True if it is outdated
    _s.conf_changed = _load_config()

    # Filename to save minimal configuration to
    _s.minconf_filename = "defconfig"

    # Any visible items in the top menu?
    _s.show_all = False
    if not _shown_nodes(kconf.top_node):
        # Nothing visible. Start in show-all mode and try again.
        _s.show_all = True
        if not _shown_nodes(kconf.top_node):
            # Give up. The implementation relies on always having a selected
            # node.
            print(
                "Empty configuration -- nothing to configure.\n"
                "Check that environment variables are set properly."
            )
            return

    # Disable warnings. They get mangled in terminal mode, and we deal with
    # errors ourselves.
    kconf.warn = False

    try:
        # Make the locale settings specified in the environment active
        locale.setlocale(locale.LC_ALL, "")
    except locale.Error:
        # fall back to the default locale
        locale.setlocale(locale.LC_ALL, "C")

    # Try to fix Unicode issues on systems with bad defaults
    if _CHANGE_C_LC_CTYPE_TO_UTF8:
        _change_c_lc_ctype_to_utf8()

    # In headless mode, just load the configuration and return
    if headless:
        return

    # Enter terminal mode via rawterm. _menuconfig() returns a string to print
    # on exit.
    result = rawterm.run(_menuconfig)
    if result is not None:
        print(result)


def _load_config():
    # Loads any existing .config file. See the Kconfig.load_config() docstring.
    #
    # Returns True if .config exists and is outdated, or if .config doesn't exist
    # at all. We prompt for saving the configuration if there are actual changes
    # or if no .config file exists (so user can save the default configuration).

    print(_s.kconf.load_config())
    if not os.path.exists(_s.conf_filename):
        # No .config exists - treat as changed so user can save defaults
        return True

    return _needs_save()


def _needs_save():
    return _kconf_needs_save(_s.kconf)


class _State:
    """Everything the interface mutates while it is running.

    One object rather than twenty module globals, so that the drawing and
    navigation helpers can be driven without a terminal: build a _State, point
    the module's _s at it, and call the function under test. That is how the
    row-rendering, search and navigation tests drive this module without a
    terminal.

    Fields:

      term:
        rawterm.Terminal instance

      kconf:
        The Kconfig instance being configured

      cur_menu:
        Menu node of the menu (or menuconfig symbol, or choice) currently being
        shown

      shown:
        List of items in cur_menu that are shown (ignoring scrolling). In
        show-all mode, this list contains all items in cur_menu. Otherwise, it
        contains just the visible items.

      sel_node_i:
        Index in shown of the currently selected node

      menu_scroll:
        Index in shown of the top row of the main display

      parent_screen_rows:
        List/stack of the row numbers that the selections in the parent menus
        appeared on. This is used to prevent the scrolling from jumping around
        when going in and out of menus.

      show_help/show_name/show_all:
        If True, the corresponding mode is on. See the module docstring.

      conf_filename:
        File to save the configuration to

      minconf_filename:
        File to save minimal configurations to

      conf_changed:
        True if the configuration has been changed. If False, we don't bother
        showing the save-and-quit dialog.

        We reset this to False whenever the configuration is saved explicitly
        from the save dialog.

      screen_win/menu_win/dialog_win/help_win:
        rawterm regions making up the display

      dlg_bottom_shadow/dlg_right_shadow:
        Regions painting the drop shadow of the active dialog, or None

      active_button:
        Index of the selected button in the dialog currently being shown
    """

    def __init__(self):
        self.term = None
        self.kconf = None

        self.cur_menu = None
        self.shown = []
        self.sel_node_i = 0
        self.menu_scroll = 0
        self.parent_screen_rows = []

        # Search caches, rebuilt on demand by _sorted_sc_nodes() and
        # _sorted_menu_comment_nodes()
        self.cached_sc_nodes = []
        self.cached_menu_comment_nodes = []

        self.show_help = False
        self.show_name = False
        self.show_all = False

        self.conf_filename = None
        self.minconf_filename = None
        self.conf_changed = False

        self.screen_win = None
        self.menu_win = None
        self.dialog_win = None
        self.help_win = None
        self.dlg_bottom_shadow = None
        self.dlg_right_shadow = None

        self.active_button = 0


# Rebuilt by menuconfig(), so that a second run in the same process does not
# inherit the first run's scroll position or dialog state.
_s = _State()


def _menuconfig(term):
    # Logic for the main display, with the list of symbols, etc.

    _s.term = term

    _init()

    while True:
        _draw_main()
        _s.term.update()

        c = _s.term.read_key()

        if c == Key.RESIZE:
            _resize_main()

        #
        # Menu item navigation (Up/Down/PgUp/PgDn/Home/End)
        #

        elif c == Key.DOWN:
            _select_next_menu_entry()

        elif c == Key.UP:
            _select_prev_menu_entry()

        elif c in (Key.PAGE_DOWN, "\x04"):  # Page Down/Ctrl-D
            for _ in range(_PG_JUMP):
                _select_next_menu_entry()

        elif c in (Key.PAGE_UP, "\x15"):  # Page Up/Ctrl-U
            for _ in range(_PG_JUMP):
                _select_prev_menu_entry()

        elif c == Key.END:
            _select_last_menu_entry()

        elif c == Key.HOME:
            _select_first_menu_entry()

        #
        # Button navigation (Tab/Left/Right cycle buttons, matching mconf)
        #

        elif c in ("\t", Key.RIGHT):
            _s.active_button = (_s.active_button + 1) % len(_MENU_BUTTONS)

        elif c == Key.LEFT:
            _s.active_button = (_s.active_button - 1) % len(_MENU_BUTTONS)

        #
        # Enter activates the currently focused button (matching mconf)
        #

        elif c == "\n":
            if _s.active_button == 0:  # Select
                sel_node = _s.shown[_s.sel_node_i]
                if not _enter_menu(sel_node):
                    _change_node(sel_node)

            elif _s.active_button == 1:  # Exit
                if _s.cur_menu is _s.kconf.top_node:
                    res = _quit_dialog()
                    if res:
                        return res
                else:
                    _leave_menu()

            elif _s.active_button == 2:  # Help
                _info_dialog(_s.shown[_s.sel_node_i], False)
                _resize_main()

            elif _s.active_button == 3:  # Save
                filename = _save_dialog(
                    _s.kconf.write_config, _s.conf_filename, "configuration"
                )
                if filename:
                    _s.conf_filename = filename
                    _s.conf_changed = False

            elif _s.active_button == 4:  # Load
                _load_dialog()

        #
        # Direct item operations (matching mconf key codes)
        #

        elif c == " ":
            # Toggle the node if possible
            sel_node = _s.shown[_s.sel_node_i]
            if not _change_node(sel_node):
                _enter_menu(sel_node)

        elif c in ("n", "N"):
            _set_sel_node_tri_val(0)

        elif c in ("m", "M"):
            _set_sel_node_tri_val(1)

        elif c in ("y", "Y", "s", "S"):
            # Y and S both set YES (matching C mconf)
            _set_sel_node_tri_val(2)

        elif c in (Key.BACKSPACE, "\x1b", "e", "x", "E", "X"):
            # Leave menu (ESC/Backspace/E/X). At top level, ESC/E/X show
            # quit dialog; Backspace always just leaves.
            if c != Key.BACKSPACE and _s.cur_menu is _s.kconf.top_node:
                res = _quit_dialog()
                if res:
                    return res
            else:
                _leave_menu()

        elif c == "/":
            _jump_to_dialog()
            _resize_main()

        elif c in ("?", "h", "H"):
            _info_dialog(_s.shown[_s.sel_node_i], False)
            _resize_main()

        elif c in ("f", "F"):
            _s.show_help = not _s.show_help
            _resize_main()

        elif c in ("c", "C"):
            _s.show_name = not _s.show_name

        elif c in ("a", "A", "z", "Z"):
            _toggle_show_all()

        elif c in ("q", "Q"):
            res = _quit_dialog()
            if res:
                return res


def _quit_dialog():
    config_exists = os.path.exists(_s.conf_filename)

    if not _s.conf_changed and config_exists:
        return f"No changes to save (for '{_s.conf_filename}')"

    # Match C mconf's handle_exit() -- dialog_yesno with 2 buttons
    dialog_text = (
        "Do you wish to save your new configuration?\n"
        "(Press <ESC> to continue kernel configuration.)"
    )

    result = _button_dialog(
        None,  # No title in yesno dialog
        dialog_text,
        [" Yes ", "  No  "],
        default_button=0,
    )

    if result is None:  # ESC -- back to menu
        return None

    if result == 0:  # Yes
        # Returns a message to print
        msg = _try_save(_s.kconf.write_config, _s.conf_filename, "configuration")
        if msg:
            return msg
        # If save failed, try again
        return None

    elif result == 1:  # No
        if not config_exists:
            return "Configuration was not saved"
        return f"Configuration ({_s.conf_filename}) was not saved"


def _init():
    # Initializes the main display with the list of symbols, etc. Also does
    # misc. global initialization that needs to happen after initializing
    # the terminal.
    #
    # The layout matches mconf/lxdialog's dialog_menu():
    #   _s.screen_win - full screen background (cyan/blue)
    #   _s.dialog_win - centered dialog body (white, with border/title/
    #                   instructions/inner menu box/separator/buttons)
    #   _s.menu_win   - inner menu item area (positioned inside the dialog)
    #   _s.help_win   - help text (show-help mode only)
    #
    # Shadow regions for the dialog are created in _resize_main().

    _init_styles()

    # Hide the cursor
    _s.term.hide_cursor()

    # Initialize regions -- creation order determines compositing order
    # (painter's algorithm: later regions paint on top of earlier ones)

    # Full-screen background (lowest layer)
    _s.screen_win = _styled_region("screen")

    # The main dialog body (above screen background)
    _s.dialog_win = _styled_region("body")

    # Inner menu item area (above dialog body)
    _s.menu_win = _styled_region("list")

    # Help text window for show-help mode (above dialog, initially hidden)
    _s.help_win = _styled_region("show-help")

    # Shadow regions -- created in _resize_main()
    _s.dlg_bottom_shadow = None
    _s.dlg_right_shadow = None

    # Currently focused button (0=Select, 1=Exit, 2=Help, 3=Save, 4=Load)
    _s.active_button = 0

    # The rows we'd like the nodes in the parent menus to appear on. This
    # prevents the scroll from jumping around when going in and out of menus.
    _s.parent_screen_rows = []

    # Initial state

    _s.cur_menu = _s.kconf.top_node
    _s.shown = _shown_nodes(_s.cur_menu)
    _s.sel_node_i = _s.menu_scroll = 0

    _s.show_help = _s.show_name = False

    # Give regions their initial size
    _resize_main()


def _resize_main():
    # Resizes the main display to match mconf/lxdialog's dialog_menu() layout.
    #
    # Layout (matching menubox.c):
    #   Screen background (full terminal, cyan/blue):
    #     Row 0: backtitle (mainmenu_text)
    #     Row 1: subtitle path + hline
    #   Shadow (right 2 cols + bottom 1 row of dialog)
    #   Dialog (centered, white background):
    #     Row 0:            top border with title
    #     Rows 1..box_y-1:  instruction text
    #     Row box_y:        inner menu box top border
    #     Rows box_y+1...:  menu items (menu_height rows)
    #     Row box_y+mh+1:   inner menu box bottom border
    #     Row dlg_h-3:      separator (LTEE + HLINE + RTEE)
    #     Row dlg_h-2:      buttons
    #     Row dlg_h-1:      bottom border
    #   Menu items region (_s.menu_win) overlaid inside the inner box

    screen_height = _s.term.height
    screen_width = _s.term.width

    # Dialog dimensions -- matching mconf/lxdialog/menubox.c dialog_menu()
    dlg_height = screen_height - 4
    dlg_width = screen_width - 5

    # Clamp to minimum usable sizes
    if dlg_height < 10:
        dlg_height = max(screen_height - 2, 6)
    if dlg_width < 40:
        dlg_width = max(screen_width - 2, 20)

    # Menu item area dimensions within the dialog
    # mconf: menu_height = height - 10;  menu_width = width - 6;
    menu_height = max(dlg_height - 10, 1)
    menu_width = max(dlg_width - 6, 10)

    # In show-help mode, steal rows from the menu area for help text
    help_in_dialog = 0
    if _s.show_help:
        help_in_dialog = min(_SHOW_HELP_HEIGHT, max(menu_height - 2, 0))
        menu_height = max(menu_height - help_in_dialog, 1)

    # Menu box position within dialog
    # mconf: box_y = height - menu_height - 5;
    #        box_x = (width - menu_width) / 2 - 1;
    box_y = dlg_height - menu_height - 5 - help_in_dialog
    box_x = (dlg_width - menu_width) // 2 - 1
    box_y = max(box_y, 1)

    # Center dialog on screen
    dlg_y = (screen_height - dlg_height) // 2
    dlg_x = (screen_width - dlg_width) // 2

    # --- Resize and position regions ---

    # Screen background
    _s.screen_win.resize(screen_height, screen_width)
    _s.screen_win.move(0, 0)
    _s.screen_win.fill(_style["screen"])

    # Dialog body
    _s.dialog_win.resize(dlg_height, dlg_width)
    _s.dialog_win.move(dlg_y, dlg_x)
    _s.dialog_win.fill(_style["body"])

    # Menu items (positioned inside the inner menu box of the dialog)
    _s.menu_win.resize(menu_height, menu_width)
    _s.menu_win.move(dlg_y + box_y + 1, dlg_x + box_x + 1)
    _s.menu_win.fill(_style["list"])

    # Help window -- positioned below inner menu box in show-help mode,
    # or moved off-screen when not needed
    if _s.show_help and help_in_dialog > 0:
        help_y = dlg_y + box_y + menu_height + 2
        _s.help_win.resize(help_in_dialog, menu_width)
        _s.help_win.move(help_y, dlg_x + box_x + 1)
        _s.help_win.fill(_style["show-help"])
    else:
        _s.help_win.resize(1, 1)
        _s.help_win.move(screen_height, 0)  # off-screen

    # Shadow regions for the dialog
    _close_shadow_windows(_s.dlg_bottom_shadow, _s.dlg_right_shadow)
    _s.dlg_bottom_shadow, _s.dlg_right_shadow = _create_shadow_for_win(_s.dialog_win)

    # Adjust the scroll so that the selected node is still within the window
    if _s.sel_node_i - _s.menu_scroll >= menu_height:
        _s.menu_scroll = _s.sel_node_i - menu_height + 1


def _height(win):
    return win.height


def _width(win):
    return win.width


def _enter_menu(menu):
    # Makes 'menu' the currently displayed menu. In addition to actual 'menu's,
    # "menu" here includes choices and symbols defined with the 'menuconfig'
    # keyword.
    #
    # Returns False if 'menu' can't be entered.

    if not menu.is_menuconfig:
        return False  # Not a menu

    shown_sub = _shown_nodes(menu)
    # Never enter empty menus. We depend on having a current node.
    if not shown_sub:
        return False

    # Remember where the current node appears on the screen, so we can try
    # to get it to appear in the same place when we leave the menu
    _s.parent_screen_rows.append(_s.sel_node_i - _s.menu_scroll)

    # Jump into menu
    _s.cur_menu = menu
    _s.shown = shown_sub
    _s.sel_node_i = _s.menu_scroll = 0

    if isinstance(menu.item, Choice):
        _select_selected_choice_sym()

    return True


def _select_selected_choice_sym():
    # Puts the cursor on the currently selected (y-valued) choice symbol, if
    # any. Does nothing if if the choice has no selection (is not visible/in y
    # mode).

    choice = _s.cur_menu.item
    if choice.selection:
        # Search through all menu nodes to handle choice symbols being defined
        # in multiple locations
        for node in choice.selection.nodes:
            if node in _s.shown:
                _s.sel_node_i = _s.shown.index(node)
                _center_vertically()
                return


def _jump_to(node):
    # Jumps directly to the menu node 'node'

    # Clear remembered menu locations. We might not even have been in the
    # parent menus before.
    _s.parent_screen_rows = []

    old_show_all = _s.show_all
    jump_into = (isinstance(node.item, Choice) or node.item == MENU) and node.list

    # If we're jumping to a non-empty choice or menu, jump to the first entry
    # in it instead of jumping to its menu node
    if jump_into:
        _s.cur_menu = node
        node = node.list
    else:
        _s.cur_menu = uicommon.parent_menu(node)

    _s.shown = _shown_nodes(_s.cur_menu)
    if node not in _s.shown:
        # The node wouldn't be shown. Turn on show-all to show it.
        _s.show_all = True
        _s.shown = _shown_nodes(_s.cur_menu)

    _s.sel_node_i = _s.shown.index(node)

    if jump_into and not old_show_all and _s.show_all:
        # If we're jumping into a choice or menu and were forced to turn on
        # show-all because the first entry wasn't visible, try turning it off.
        # That will land us at the first visible node if there are visible
        # nodes, and is a no-op otherwise.
        _toggle_show_all()

    _center_vertically()

    # If we're jumping to a non-empty choice, jump to the selected symbol, if
    # any
    if jump_into and isinstance(_s.cur_menu.item, Choice):
        _select_selected_choice_sym()


def _leave_menu():
    # Jumps to the parent menu of the current menu. Does nothing if we're in
    # the top menu.

    if _s.cur_menu is _s.kconf.top_node:
        return

    # Jump to parent menu
    parent = uicommon.parent_menu(_s.cur_menu)
    _s.shown = _shown_nodes(parent)

    try:
        _s.sel_node_i = _s.shown.index(_s.cur_menu)
    except ValueError:
        # The parent actually does not contain the current menu (e.g., symbol
        # search). So we jump to the first node instead.
        _s.sel_node_i = 0

    _s.cur_menu = parent

    # Try to make the menu entry appear on the same row on the screen as it did
    # before we entered the menu.

    if _s.parent_screen_rows:
        # The terminal might have shrunk since we were last in the parent menu
        screen_row = min(_s.parent_screen_rows.pop(), _height(_s.menu_win) - 1)
        _s.menu_scroll = max(_s.sel_node_i - screen_row, 0)
    else:
        # No saved parent menu locations, meaning we jumped directly to some
        # node earlier
        _center_vertically()


def _select_next_menu_entry():
    # Selects the menu entry after the current one, adjusting the scroll if
    # necessary. Does nothing if we're already at the last menu entry.

    if _s.sel_node_i < len(_s.shown) - 1:
        # Jump to the next node
        _s.sel_node_i += 1

        # If the new node is sufficiently close to the edge of the menu window
        # (as determined by _SCROLL_OFFSET), increase the scroll by one. This
        # gives nice and non-jumpy behavior even when
        # _SCROLL_OFFSET >= _height(_s.menu_win).
        last_visible = _s.menu_scroll + _height(_s.menu_win) - _SCROLL_OFFSET
        if _s.sel_node_i >= last_visible and _s.menu_scroll < _max_scroll(
            _s.shown, _s.menu_win
        ):
            _s.menu_scroll += 1


def _select_prev_menu_entry():
    # Selects the menu entry before the current one, adjusting the scroll if
    # necessary. Does nothing if we're already at the first menu entry.

    if _s.sel_node_i > 0:
        # Jump to the previous node
        _s.sel_node_i -= 1

        # See _select_next_menu_entry()
        if _s.sel_node_i < _s.menu_scroll + _SCROLL_OFFSET:
            _s.menu_scroll = max(_s.menu_scroll - 1, 0)


def _select_last_menu_entry():
    # Selects the last menu entry in the current menu

    _s.sel_node_i = len(_s.shown) - 1
    _s.menu_scroll = _max_scroll(_s.shown, _s.menu_win)


def _select_first_menu_entry():
    # Selects the first menu entry in the current menu

    _s.sel_node_i = _s.menu_scroll = 0


def _toggle_show_all():
    # Toggles show-all mode on/off. If turning it off would give no visible
    # items in the current menu, it is left on.

    # Row on the screen the cursor is on. Preferably we want the same row to
    # stay highlighted.
    old_row = _s.sel_node_i - _s.menu_scroll

    _s.show_all = not _s.show_all
    # List of new nodes to be shown after toggling _s.show_all
    new_shown = _shown_nodes(_s.cur_menu)

    # Find a good node to select. The selected node might disappear if show-all
    # mode is turned off.

    # Select the previously selected node itself if it is still visible. If
    # there are visible nodes before it, select the closest one.
    for node in _s.shown[_s.sel_node_i :: -1]:
        if node in new_shown:
            _s.sel_node_i = new_shown.index(node)
            break
    else:
        # No visible nodes before the previously selected node. Select the
        # closest visible node after it instead.
        for node in _s.shown[_s.sel_node_i + 1 :]:
            if node in new_shown:
                _s.sel_node_i = new_shown.index(node)
                break
        else:
            # No visible nodes at all, meaning show-all was turned off inside
            # an invisible menu. Don't allow that, as the implementation relies
            # on always having a selected node.
            _s.show_all = True
            return

    _s.shown = new_shown

    # Try to make the cursor stay on the same row in the menu window. This
    # might be impossible if too many nodes have disappeared above the node.
    _s.menu_scroll = max(_s.sel_node_i - old_row, 0)


def _center_vertically():
    # Centers the selected node vertically, if possible

    _s.menu_scroll = min(
        max(_s.sel_node_i - _height(_s.menu_win) // 2, 0),
        _max_scroll(_s.shown, _s.menu_win),
    )


def _draw_main():
    # Draws the mconf-style "main" display: screen background with path,
    # centered dialog with title/instructions/inner menu box/buttons, and
    # shadow.

    screen_width = _s.term.width
    dlg_h = _height(_s.dialog_win)
    dlg_w = _width(_s.dialog_win)

    menu_height = _height(_s.menu_win)
    menu_width = _width(_s.menu_win)

    # --- Compute inner box position within the dialog ---
    # These must match _resize_main() calculations.
    help_in_dialog = 0
    if _s.show_help:
        help_in_dialog = min(_SHOW_HELP_HEIGHT, max(dlg_h - 10 - 2, 0))
        # Recalculate menu_height for positioning only (actual size is from
        # the _s.menu_win region).
    box_y = dlg_h - menu_height - 5 - help_in_dialog
    box_x = (dlg_w - menu_width) // 2 - 1
    box_y = max(box_y, 1)

    # item_x: indent of menu items within the inner box (matching mconf)
    if menu_width >= 80:
        item_x = (menu_width - 70) // 2
    else:
        item_x = 4

    # ---------------------------------------------------------------
    # 1. Screen background
    # ---------------------------------------------------------------
    _s.screen_win.clear()

    screen_style = _style["screen"]

    # Backtitle at row 0 (like mconf's dialog_clear() + backtitle)
    _s.screen_win.write(0, 1, _s.kconf.mainmenu_text, screen_style)

    # Subtitle path at row 1 (like mconf's subtitle trail)
    subtitle_parts = []
    menu = _s.cur_menu
    while menu is not _s.kconf.top_node:
        subtitle_parts.append(
            menu.prompt[0] if menu.prompt else standard_sc_expr_str(menu.item)
        )
        menu = menu.parent
    subtitle_parts.reverse()

    if subtitle_parts:
        path_str = ""
        for part in subtitle_parts:
            path_str += Box.RARROW + " " + part + " "
        _s.screen_win.write(1, 1, path_str[: screen_width - 2], screen_style)
        hline_start = min(1 + len(path_str), screen_width - 1)
    else:
        hline_start = 1

    # Fill rest of row 1 with horizontal line
    for j in range(hline_start, screen_width - 1):
        _s.screen_win.write_char(1, j, Box.HLINE, screen_style)

    # Mode indicators on screen background (show-name/show-all/show-help)
    enabled_modes = []
    if _s.show_help:
        enabled_modes.append("show-help")
    if _s.show_name:
        enabled_modes.append("show-name")
    if _s.show_all:
        enabled_modes.append("show-all")
    if enabled_modes:
        mode_str = "[" + "+".join(enabled_modes) + "]"
        _s.screen_win.write(
            0,
            max(screen_width - len(mode_str) - 1, 0),
            mode_str,
            screen_style,
        )

    # ---------------------------------------------------------------
    # 2. Dialog body
    # ---------------------------------------------------------------
    _s.dialog_win.clear()

    body_style = _style["body"]
    border_style = _style.get("border", _style["frame"])

    # Outer dialog box: body for interior, frame for border
    # Matches mconf: draw_box(dialog, 0, 0, h, w, dlg.dialog.atr, dlg.border.atr)
    _draw_box(_s.dialog_win, 0, 0, dlg_h, dlg_w, body_style, border_style)

    # Separator between menu area and buttons
    _draw_separator(_s.dialog_win, dlg_h - 3, dlg_w)

    # Title centered in top border (like mconf's print_title())
    title = _s.cur_menu.prompt[0] if _s.cur_menu.prompt else _s.kconf.mainmenu_text
    _draw_title(_s.dialog_win, title, dlg_w)

    # Instruction text (autowrapped, like mconf's print_autowrap())
    # mconf: print_autowrap(dialog, prompt, width - 2, 1, 3)
    # Centers text if the entire string fits in one line, otherwise
    # wraps at column 3 with width (width - 2).
    inst_x = 3
    inst_width = dlg_w - 2 * inst_x
    if inst_width > 0:
        if len(_MENU_INSTRUCTIONS) <= inst_width:
            # Entire text fits -- center it (like mconf's
            # print_text_centered)
            cx = (dlg_w - len(_MENU_INSTRUCTIONS)) // 2
            _s.dialog_win.write(1, cx, _MENU_INSTRUCTIONS, body_style)
        else:
            inst_lines = textwrap.wrap(_MENU_INSTRUCTIONS, inst_width)
            for idx, line in enumerate(inst_lines):
                row = 1 + idx
                if row >= box_y:
                    break
                _s.dialog_win.write(row, inst_x, line, body_style)

    # Inner menu box: box_style=menubox-border, border_style=menubox
    # (matching mconf's draw_box for the menu area)
    inner_box_style = _style.get("menubox-border", _style["frame"])
    inner_border_style = _style.get("menubox", _style["list"])
    _draw_box(
        _s.dialog_win,
        box_y,
        box_x,
        menu_height + 2,
        menu_width + 2,
        inner_box_style,
        inner_border_style,
    )

    # Scroll arrows (like mconf's print_arrows())
    _draw_scroll_arrows(
        _s.dialog_win,
        len(_s.shown),
        _s.menu_scroll,
        box_y,
        box_x + item_x + 1,
        menu_height,
        _style["list"],
        border_style,
    )

    # Buttons (like mconf's print_buttons())
    _draw_main_buttons(_s.dialog_win, dlg_h, dlg_w)

    # ---------------------------------------------------------------
    # 3. Menu items (drawn into _s.menu_win, positioned inside inner box)
    # ---------------------------------------------------------------
    _s.menu_win.clear()

    text_width = menu_width - item_x
    for i in range(_s.menu_scroll, min(_s.menu_scroll + menu_height, len(_s.shown))):
        node = _s.shown[i]

        if uicommon.visible(node) or not _s.show_all:
            style = _style["selection" if i == _s.sel_node_i else "list"]
        else:
            style = _style["inv-selection" if i == _s.sel_node_i else "inv-list"]

        # Clear entire row with list style, then draw text
        _s.menu_win.write(i - _s.menu_scroll, 0, " " * menu_width, _style["list"])

        node_text = _node_str(node)
        node_text = node_text[:text_width].ljust(text_width)
        _s.menu_win.write(i - _s.menu_scroll, item_x, node_text, style)

    # ---------------------------------------------------------------
    # 4. Help text (show-help mode only)
    # ---------------------------------------------------------------
    if _s.show_help and _s.help_win.height > 1:
        _s.help_win.clear()
        node = _s.shown[_s.sel_node_i]
        sh_style = _style["show-help"]
        if isinstance(node.item, (Symbol, Choice)) and node.help:
            help_lines = textwrap.wrap(node.help, _width(_s.help_win))
            for i in range(min(_height(_s.help_win), len(help_lines))):
                _s.help_win.write(i, 0, help_lines[i], sh_style)
        else:
            _s.help_win.write(0, 0, "(no help)", sh_style)

    # ---------------------------------------------------------------
    # 5. Shadow
    # ---------------------------------------------------------------
    _refresh_shadow_windows(_s.dlg_bottom_shadow, _s.dlg_right_shadow)


def _draw_scroll_arrows(
    win, item_count, scroll, box_y, x, menu_height, menubox_style, border_style
):
    # Draw scroll indicator arrows matching mconf/lxdialog's print_arrows().
    #
    # When scrolled up: up-arrow "^(-)" at the top of the inner box
    # When more below: down-arrow "v(+)" at the bottom of the inner box
    # Otherwise: horizontal lines (part of the box border)

    # Up arrow position: box_y row, at x
    if scroll > 0:
        win.write_char(box_y, x, Box.UARROW, border_style)
        win.write(box_y, x + 1, "(-)", border_style)
    else:
        for j in range(4):
            if x + j < _width(win) - 1:
                win.write_char(box_y, x + j, Box.HLINE, menubox_style)

    # Down arrow position: box_y + menu_height + 1 row, at x
    down_y = box_y + menu_height + 1
    if menu_height < item_count and scroll + menu_height < item_count:
        win.write_char(down_y, x, Box.DARROW, border_style)
        win.write(down_y, x + 1, "(+)", border_style)
    else:
        for j in range(4):
            if x + j < _width(win) - 1:
                win.write_char(down_y, x + j, Box.HLINE, border_style)


def _draw_main_buttons(win, dlg_h, dlg_w):
    # Draw the 5 main menu buttons matching mconf/lxdialog's print_buttons().
    #
    # In mconf, buttons are spaced 12 apart starting at (width/2 - 28):
    #   print_button(win, "Select", y, x,      selected == 0);
    #   print_button(win, " Exit ", y, x + 12, selected == 1);
    #   print_button(win, " Help ", y, x + 24, selected == 2);
    #   print_button(win, " Save ", y, x + 36, selected == 3);
    #   print_button(win, " Load ", y, x + 48, selected == 4);

    button_y = dlg_h - 2
    start_x = max(dlg_w // 2 - 28, 1)

    for i, label in enumerate(_MENU_BUTTONS):
        bx = start_x + i * 12
        if bx + len(label) + 2 > dlg_w:
            break
        _print_button(win, label, button_y, bx, i == _s.active_button)


def _shown_nodes(menu):
    # Returns the list of menu nodes from 'menu' (see _parent_menu()) that
    # would be shown when entering it

    def rec(node):
        res = []

        while node:
            if uicommon.visible(node) or _s.show_all:
                res.append(node)
                if node.list and not node.is_menuconfig:
                    # Nodes from implicit menu created from dependencies. Will
                    # be shown indented. Note that is_menuconfig is True for
                    # menus and choices as well as 'menuconfig' symbols.
                    res += rec(node.list)

            elif node.list and isinstance(node.item, Symbol):
                # Show invisible symbols if they have visible children. This
                # can happen for an m/y-valued symbol with an optional prompt
                # ('prompt "foo" is COND') that is currently disabled. Note
                # that it applies to both 'config' and 'menuconfig' symbols.
                shown_children = rec(node.list)
                if shown_children:
                    res.append(node)
                    if not node.is_menuconfig:
                        res += shown_children

            node = node.next

        return res

    if isinstance(menu.item, Choice):
        # For named choices defined in multiple locations, entering the choice
        # at a particular menu node would normally only show the choice symbols
        # defined there (because that's what the MenuNode tree looks like).
        #
        # That might look confusing, and makes extending choices by defining
        # them in multiple locations less useful. Instead, gather all the child
        # menu nodes for all the choices whenever a choice is entered. That
        # makes all choice symbols visible at all locations.
        #
        # Choices can contain non-symbol items (people do all sorts of weird
        # stuff with them), hence the generality here. We really need to
        # preserve the menu tree at each choice location.
        #
        # Note: Named choices are pretty broken in the C tools, and this is
        # super obscure, so you probably won't find much that relies on this.
        # This whole 'if' could be deleted if you don't care about defining
        # choices in multiple locations to add symbols (which will still work,
        # just with things being displayed in a way that might be unexpected).

        # Do some additional work to avoid listing choice symbols twice if all
        # or part of the choice is copied in multiple locations (e.g. by
        # including some Kconfig file multiple times). We give the prompts at
        # the current location precedence.
        seen_syms = {
            node.item for node in rec(menu.list) if isinstance(node.item, Symbol)
        }
        res = []
        for choice_node in menu.item.nodes:
            for node in rec(choice_node.list):
                # 'choice_node is menu' checks if we're dealing with the
                # current location
                if node.item not in seen_syms or choice_node is menu:
                    res.append(node)
                    if isinstance(node.item, Symbol):
                        seen_syms.add(node.item)
        return res

    return rec(menu.list)


def _change_node(node):
    # Changes the value of the menu node 'node' if it is a symbol. Bools and
    # tristates are toggled, while other symbol types pop up a text entry
    # dialog.
    #
    # Returns False if the value of 'node' can't be changed.

    if not uicommon.changeable(node):
        return False

    # sc = symbol/choice
    sc = node.item

    if sc.orig_type in (INT, HEX, STRING):
        s = sc.str_value

        while True:
            s = _input_dialog(
                f"{node.prompt[0]} ({TYPE_TO_STR[sc.orig_type]})",
                s,
                uicommon.range_info(sc),
            )

            if s is None:
                break

            if sc.orig_type in (INT, HEX):
                s = s.strip()

                # 'make menuconfig' does this too. Hex values not starting with
                # '0x' are accepted when loading .config files though.
                if sc.orig_type == HEX and not s.startswith(("0x", "0X")):
                    s = "0x" + s

            if _check_valid(sc, s):
                _set_val(sc, s)
                break

    elif len(sc.assignable) == 1:
        # Handles choice symbols for choices in y mode, which are a special
        # case: .assignable can be (2,) while .tri_value is 0.
        _set_val(sc, sc.assignable[0])

    else:
        # Set the symbol to the value after the current value in
        # sc.assignable, with wrapping
        val_index = sc.assignable.index(sc.tri_value)
        _set_val(sc, sc.assignable[(val_index + 1) % len(sc.assignable)])

    if uicommon.is_y_mode_choice_sym(sc) and not node.list:
        # Immediately jump to the parent menu after making a choice selection,
        # like 'make menuconfig' does, except if the menu node has children
        # (which can happen if a symbol 'depends on' a choice symbol that
        # immediately precedes it).
        _leave_menu()

    return True


def _set_sel_node_tri_val(tri_val):
    # Sets the value of the currently selected menu entry to 'tri_val', if that
    # value can be assigned

    sc = _s.shown[_s.sel_node_i].item
    if isinstance(sc, (Symbol, Choice)) and tri_val in sc.assignable:
        _set_val(sc, tri_val)


def _set_val(sc, val):
    # Wrapper around Symbol/Choice.set_value() for updating the menu state and
    # _s.conf_changed

    # Use the string representation of tristate values. This makes the format
    # consistent for all symbol types.
    if val in TRI_TO_STR:
        val = TRI_TO_STR[val]

    if val != sc.str_value:
        sc.set_value(val)
        _s.conf_changed = True

        # Changing the value of the symbol might have changed what items in the
        # current menu are visible. Recalculate the state.
        _update_menu()


def _update_menu():
    # Updates the current menu after the value of a symbol or choice has been
    # changed. Changing a value might change which items in the menu are
    # visible.
    #
    # If possible, preserves the location of the cursor on the screen when
    # items are added/removed above the selected item.

    # Row on the screen the cursor was on
    old_row = _s.sel_node_i - _s.menu_scroll

    sel_node = _s.shown[_s.sel_node_i]

    # New visible nodes
    _s.shown = _shown_nodes(_s.cur_menu)

    # New index of selected node
    _s.sel_node_i = _s.shown.index(sel_node)

    # Try to make the cursor stay on the same row in the menu window. This
    # might be impossible if too many nodes have disappeared above the node.
    _s.menu_scroll = max(_s.sel_node_i - old_row, 0)


def _input_dialog(title, initial_text, info_text=None):
    # Pops up a dialog that prompts the user for a string
    #
    # title:
    #   Title to display at the top of the dialog window's border
    #
    # initial_text:
    #   Initial text to prefill the input field with
    #
    # info_text:
    #   String to show next to the input field. If None, just the input field
    #   is shown.

    win = None
    bottom_shadow = right_shadow = None

    try:
        win = _styled_region("body")

        info_lines = info_text.split("\n") if info_text else []

        # Give the input dialog its initial size
        _resize_input_dialog(win, title, info_lines)

        _s.term.show_cursor(very_visible=True)

        # Input field text
        s = initial_text

        # Cursor position
        i = len(initial_text)

        def edit_width():
            return _width(win) - 4

        # Horizontal scroll offset
        hscroll = max(i - edit_width() + 1, 0)

        bottom_shadow, right_shadow = _create_shadow_for_win(win)
        while True:
            # Draw the "main" display with the menu, etc., so that resizing
            # still works properly. This is like a stack of windows, only
            # hardcoded for now.
            _draw_main()

            _draw_input_dialog(win, title, info_lines, s, i, hscroll)

            _refresh_shadow_windows(bottom_shadow, right_shadow)

            _s.term.update()

            c = _s.term.read_key()

            if c == Key.RESIZE:
                _resize_main()
                _resize_input_dialog(win, title, info_lines)
                _close_shadow_windows(bottom_shadow, right_shadow)
                bottom_shadow, right_shadow = _create_shadow_for_win(win)

            elif c == "\n":
                _s.term.hide_cursor()
                return s

            elif c == "\x1b":  # ESC
                _s.term.hide_cursor()
                return None

            elif c == "\0":  # NUL, ignore
                pass

            else:
                s, i, hscroll = _edit_text(c, s, i, hscroll, edit_width())
    finally:
        _close_shadow_windows(bottom_shadow, right_shadow)
        if win:
            win.close()


def _resize_input_dialog(win, title, info_lines):
    # Resizes the input dialog to a size appropriate for the terminal size

    screen_height, screen_width = _s.term.height, _s.term.width

    win_height = 5
    if info_lines:
        win_height += len(info_lines) + 1
    win_height = min(win_height, screen_height)

    win_width = max(
        _INPUT_DIALOG_MIN_WIDTH, len(title) + 4, *(len(line) + 4 for line in info_lines)
    )
    win_width = min(win_width, screen_width)

    win.resize(win_height, win_width)
    win.move((screen_height - win_height) // 2, (screen_width - win_width) // 2)


def _draw_input_dialog(win, title, info_lines, s, i, hscroll):
    edit_width = _width(win) - 4

    win.clear()

    # Draw the frame first (like _draw_key_dialog), then content on top.
    # _draw_frame fills the interior with body_style via _draw_box, so
    # content must be drawn after to remain visible.
    _draw_frame(win, title)

    # Note: Perhaps having a separate window for the input field would be nicer
    visible_s = s[hscroll : hscroll + edit_width]
    win.write(2, 2, visible_s + " " * (edit_width - len(visible_s)), _style["edit"])

    max_info_rows = max(_height(win) - 5, 0)
    for linenr, line in enumerate(info_lines):
        if linenr >= max_info_rows:
            break
        # Truncate then pad to interior width so body_style covers frame bg
        win.write(4 + linenr, 2, line[:edit_width].ljust(edit_width), _style["body"])

    _s.term.set_cursor(win, 2, 2 + i - hscroll)


def _load_dialog():
    # Dialog for loading a new configuration

    if _s.conf_changed:
        c = _key_dialog(
            "Load",
            "You have unsaved changes. Load new\n"
            "configuration anyway?\n"
            "\n"
            "         (O)K  (C)ancel",
            "oc",
        )

        if c is None or c == "c":
            return

    filename = _s.conf_filename
    while True:
        filename = _input_dialog("File to load", filename, _load_save_info())
        if filename is None:
            return

        filename = os.path.expanduser(filename)

        if _try_load(filename):
            _s.conf_filename = filename
            _s.conf_changed = _needs_save()

            # Turn on show-all mode if the selected node is not visible after
            # loading the new configuration. _s.shown still holds the old state.
            if _s.shown[_s.sel_node_i] not in _shown_nodes(_s.cur_menu):
                _s.show_all = True

            _update_menu()

            # The message dialog indirectly updates the menu display, so _msg()
            # must be called after the new state has been initialized
            _msg("Success", "Loaded " + filename)
            return


def _try_load(filename):
    # Tries to load a configuration file. Pops up an error and returns False on
    # failure.
    #
    # filename:
    #   Configuration file to load

    try:
        _s.kconf.load_config(filename)
        return True
    except OSError as e:
        _error(
            f"Error loading '{filename}'\n\n{e.strerror} (errno: {errno.errorcode[e.errno]})"
        )
        return False


def _save_dialog(save_fn, default_filename, description):
    # Dialog for saving the current configuration
    #
    # save_fn:
    #   Function to call with 'filename' to save the file
    #
    # default_filename:
    #   Prefilled filename in the input field
    #
    # description:
    #   String describing the thing being saved
    #
    # Return value:
    #   The path to the saved file, or None if no file was saved

    filename = default_filename
    while True:
        filename = _input_dialog(
            f"Filename to save {description} to", filename, _load_save_info()
        )
        if filename is None:
            return None

        filename = os.path.expanduser(filename)

        msg = _try_save(save_fn, filename, description)
        if msg:
            _msg("Success", msg)
            return filename


def _try_save(save_fn, filename, description):
    # Tries to save a configuration file. Returns a message to print on
    # success.
    #
    # save_fn:
    #   Function to call with 'filename' to save the file
    #
    # description:
    #   String describing the thing being saved
    #
    # Return value:
    #   A message to print on success, and None on failure

    try:
        # save_fn() returns a message to print
        return save_fn(filename)
    except OSError as e:
        _error(
            f"Error saving {description} to '{e.filename}'\n\n{e.strerror} (errno: {errno.errorcode[e.errno]})"
        )
        return None


def _key_dialog(title, text, keys):
    # Pops up a dialog that can be closed by pressing a key
    #
    # title:
    #   Title to display at the top of the dialog window's border
    #
    # text:
    #   Text to show in the dialog
    #
    # keys:
    #   List of keys that will close the dialog. Other keys (besides ESC) are
    #   ignored. The caller is responsible for providing a hint about which
    #   keys can be pressed in 'text'.
    #
    # Return value:
    #   The key that was pressed to close the dialog. Uppercase characters are
    #   converted to lowercase. ESC will always close the dialog, and returns
    #   None.

    win = None
    bottom_shadow = right_shadow = None

    try:
        win = _styled_region("body")

        _resize_key_dialog(win, text)

        bottom_shadow, right_shadow = _create_shadow_for_win(win)
        while True:
            _draw_main()

            _draw_key_dialog(win, title, text)

            _refresh_shadow_windows(bottom_shadow, right_shadow)

            _s.term.update()

            c = _s.term.read_key()

            if c == Key.RESIZE:
                _resize_main()
                _resize_key_dialog(win, text)
                _close_shadow_windows(bottom_shadow, right_shadow)
                bottom_shadow, right_shadow = _create_shadow_for_win(win)

            elif c == "\x1b":  # \x1B = ESC
                return None

            elif isinstance(c, str):
                c = c.lower()
                if c in keys:
                    return c
    finally:
        _close_shadow_windows(bottom_shadow, right_shadow)
        if win:
            win.close()


def _resize_key_dialog(win, text):
    # Resizes the key dialog to a size appropriate for the terminal size

    screen_height, screen_width = _s.term.height, _s.term.width

    lines = text.split("\n")

    win_height = min(len(lines) + 4, screen_height)
    win_width = min(max(len(line) for line in lines) + 4, screen_width)

    win.resize(win_height, win_width)
    win.move((screen_height - win_height) // 2, (screen_width - win_width) // 2)


def _draw_key_dialog(win, title, text):
    win.clear()

    # Draw the frame first
    _draw_frame(win, title)

    # Draw text content inside the frame, clamped to the interior so that
    # border characters are never overwritten on small terminals.
    text_width = max(_width(win) - 4, 0)
    max_rows = max(_height(win) - 3, 0)
    for i, line in enumerate(text.split("\n")):
        if i >= max_rows:
            break
        win.write(2 + i, 2, line[:text_width], _style["body"])


def _button_dialog(title, text, buttons, default_button=0):
    # Dialog with button selection support, matching lxdialog's yesno/msgbox
    #
    # title: Dialog title (shown at top of border if provided)
    # text: Dialog text content
    # buttons: List of button labels (e.g., [" Yes ", "  No  "])
    # default_button: Index of initially selected button
    #
    # Returns: Index of selected button, or None if ESC pressed

    win = None
    bottom_shadow = right_shadow = None

    try:
        win = _styled_region("dialog")

        selected_button = default_button

        # Calculate window size based on content
        lines = text.split("\n")
        # Height: border(1) + text lines + blank + separator(1) + buttons +
        # border(1) = 1 + len(lines) + 1 + 1 + 1 + 1 = len(lines) + 5
        win_height = min(len(lines) + 5, _s.term.height - 4)
        # Calculate width from longest line and button row
        # Button row width includes buttons + spacing between them
        # 2 buttons: spacing 6, 3+ buttons: spacing 4
        spacing = 6 if len(buttons) == 2 else 4
        button_row_width = sum(len(b) + 2 for b in buttons) + spacing * (
            len(buttons) - 1
        )
        win_width = min(
            max(max(len(line) for line in lines) + 4, button_row_width + 4),
            _s.term.width - 4,
        )

        win.resize(win_height, win_width)
        win.move((_s.term.height - win_height) // 2, (_s.term.width - win_width) // 2)

        bottom_shadow, right_shadow = _create_shadow_for_win(win)

        body_style = _style["body"]

        while True:
            # Draw main display behind dialog
            _draw_main()

            win.clear()

            _draw_box(win, 0, 0, win_height, win_width, body_style, body_style)

            _draw_title(win, title, win_width)

            _draw_separator(win, win_height - 3, win_width)

            # Draw text content, clamped to the interior above the separator
            text_width = win_width - 4
            max_text_rows = win_height - 4
            for i, line in enumerate(lines):
                if i >= max_text_rows:
                    break
                win.write(1 + i, 2, line[:text_width], body_style)

            # Draw buttons at row (height - 2), using the same spacing
            # computed for initial sizing above.  Clamp x to protect the
            # left border on narrow terminals.
            button_y = win_height - 2
            current_x = max((win_width - button_row_width) // 2, 1)
            for i, button_label in enumerate(buttons):
                _print_button(
                    win,
                    button_label,
                    button_y,
                    current_x,
                    i == selected_button,
                )
                current_x += len(button_label) + 2 + spacing

            _refresh_shadow_windows(bottom_shadow, right_shadow)

            _s.term.update()

            # Handle input
            c = _s.term.read_key()

            if c == Key.RESIZE:
                _resize_main()
                # Recompute dimensions for new terminal size
                win_height = min(len(lines) + 5, _s.term.height - 4)
                win_width = min(
                    max(max(len(line) for line in lines) + 4, button_row_width + 4),
                    _s.term.width - 4,
                )
                win.resize(win_height, win_width)
                win.move(
                    (_s.term.height - win_height) // 2,
                    (_s.term.width - win_width) // 2,
                )
                _close_shadow_windows(bottom_shadow, right_shadow)
                bottom_shadow, right_shadow = _create_shadow_for_win(win)

            elif c == "\x1b":  # ESC
                return None

            elif c in ("\t", Key.RIGHT):  # Tab/Right
                selected_button = (selected_button + 1) % len(buttons)

            elif c == Key.LEFT:
                selected_button = (selected_button - 1) % len(buttons)

            elif c in (" ", "\n"):  # Space/Enter
                return selected_button

            elif isinstance(c, str):
                # Check for hotkey match
                c_lower = c.lower()
                for i, button_label in enumerate(buttons):
                    if button_label.strip().lower().startswith(c_lower):
                        return i
    finally:
        _close_shadow_windows(bottom_shadow, right_shadow)
        if win:
            win.close()


def _print_button(win, label, y, x, selected):
    # Print a button matching lxdialog's print_button().
    # Format: <Label> with the first letter highlighted as a hotkey.
    # Uses button-active or button-inactive style depending on 'selected'.

    # Count leading spaces
    leading_spaces = len(label) - len(label.lstrip(" "))
    label_stripped = label.lstrip(" ")

    # Resolve style suffix once: "active" or "inactive"
    sfx = "active" if selected else "inactive"
    btn_style = _style["button-" + sfx]
    key_style = _style["button-key-" + sfx]
    lbl_style = _style["button-label-" + sfx]

    # Draw bracket "<"
    win.write(y, x, "<", btn_style)

    # Draw leading spaces with label style
    if leading_spaces:
        win.write(y, x + 1, " " * leading_spaces, lbl_style)

    # Draw first character (hotkey) with key style, then rest with label style
    if label_stripped:
        win.write_char(y, x + 1 + leading_spaces, label_stripped[0], key_style)
        win.write(y, x + 1 + leading_spaces + 1, label_stripped[1:], lbl_style)

    # Draw bracket ">"
    win.write(y, x + 1 + len(label), ">", btn_style)


def _draw_box(win, y, x, height, width, box_style, border_style):
    # Draw a rectangular box with line drawing characters, matching lxdialog's
    # draw_box().  Fills the interior with box_style, matching the C version
    # which writes (box | ' ') for every interior cell.
    #
    # box_style:    interior fill, right edge, bottom edge, URCORNER, LRCORNER
    # border_style: left edge, top edge, ULCORNER, LLCORNER
    #
    # Popup dialogs pass the same style for both to ensure visible borders
    # (the C bold-white-on-white convention relies on ncurses ACS rendering
    # that raw terminal output cannot replicate).

    last_row = y + height - 1
    last_col = x + width - 1

    # Top row
    win.write_char(y, x, Box.ULCORNER, border_style)
    for j in range(x + 1, last_col):
        win.write_char(y, j, Box.HLINE, border_style)
    win.write_char(y, last_col, Box.URCORNER, box_style)

    # Interior rows
    fill = " " * (width - 2)
    for i in range(y + 1, last_row):
        win.write_char(i, x, Box.VLINE, border_style)
        win.write(i, x + 1, fill, box_style)
        win.write_char(i, last_col, Box.VLINE, box_style)

    # Bottom row
    win.write_char(last_row, x, Box.LLCORNER, border_style)
    for j in range(x + 1, last_col):
        win.write_char(last_row, j, Box.HLINE, box_style)
    win.write_char(last_row, last_col, Box.LRCORNER, box_style)


def _draw_title(win, title, width):
    # Draw centered title on the top border, matching C's print_title().
    # Uses width-4 (not width-2) so the padding spaces never overwrite the
    # corner characters at columns 0 and width-1.
    if title:
        title_style = _style.get("title", _style["body"])
        max_tlen = width - 4
        if max_tlen <= 0:
            return
        tlen = min(max_tlen, len(title))
        tx = (width - tlen) // 2
        win.write_char(0, tx - 1, " ", title_style)
        win.write(0, tx, title[:tlen], title_style)
        win.write_char(0, tx + tlen, " ", title_style)


def _draw_separator(win, y, width):
    # Draw separator line.  Uses body_style for all characters so the line
    # is visible on white backgrounds (the C bold-white-on-white convention
    # relies on ncurses ACS rendering that raw terminal output cannot match).
    body_s = _style["body"]
    win.write_char(y, 0, Box.LTEE, body_s)
    for j in range(1, width - 1):
        win.write_char(y, j, Box.HLINE, body_s)
    win.write_char(y, width - 1, Box.RTEE, body_s)


def _draw_scrollbar(
    win, scroll, max_scroll, x, y_start, height, track_style, thumb_style=None
):
    # Draws a vertical scrollbar with up/down arrows and a thumb indicator.
    #
    # win:          Window to draw in
    # scroll:       Current scroll position
    # max_scroll:   Maximum scroll position
    # x:            Column to draw scrollbar in
    # y_start:      Row for the first scrollbar element
    # height:       Total height of scrollbar area (including arrows)
    # track_style:  Style for arrows and track
    # thumb_style:  Style for the thumb indicator (defaults to track_style)

    if thumb_style is None:
        thumb_style = track_style

    # Calculate thumb position between the two arrows
    if height > 2:
        thumb_pos = 1 + int((float(scroll) / max_scroll) * (height - 3))
    else:
        thumb_pos = 1

    for i in range(height):
        y = y_start + i
        if i == 0:
            win.write_char(y, x, Box.UARROW, track_style)
        elif i == height - 1:
            win.write_char(y, x, Box.DARROW, track_style)
        elif i == thumb_pos:
            win.write_char(y, x, " ", thumb_style)
        else:
            win.write_char(y, x, Box.VLINE, track_style)


def _create_shadow_for_win(win, right_y_offset=1):
    # Convenience wrapper: creates shadow regions from a rawterm Region's
    # position and size
    return _create_shadow_windows(win.y, win.x, win.height, win.width, right_y_offset)


def _create_shadow_windows(y, x, height, width, right_y_offset=1):
    # Create shadow regions for bottom and right edges
    # Returns tuple of (bottom_shadow, right_shadow)
    #
    # Based on lxdialog's draw_shadow():
    # - Bottom: at y + height, from x + 2, width chars
    # - Right: from y + right_y_offset to y + height (inclusive), at x + width, 2 chars wide

    try:
        shadow_style = _style.get("shadow", Style())

        # Bottom shadow region (1 line high, width wide, offset by 2 on x)
        bottom_shadow = None
        if y + height < _s.term.height and x + 2 + width <= _s.term.width:
            try:
                bottom_shadow = _s.term.region(1, width, y + height, x + 2)
                bottom_shadow.fill(shadow_style)
            except Exception:
                pass

        # Right shadow region
        right_shadow = None
        if x + width + 2 <= _s.term.width and y + height <= _s.term.height:
            try:
                shadow_height = height - right_y_offset + 1
                if shadow_height > 0:
                    right_shadow = _s.term.region(
                        shadow_height, 2, y + right_y_offset, x + width
                    )
                    right_shadow.fill(shadow_style)
            except Exception:
                pass

        return bottom_shadow, right_shadow
    except Exception:
        return None, None


def _close_shadow_windows(bottom_shadow, right_shadow):
    # Unregister shadow regions from the compositor so they stop rendering.
    for shadow in (bottom_shadow, right_shadow):
        if shadow:
            try:
                shadow.close()
            except Exception:
                pass


def _refresh_shadow_windows(bottom_shadow, right_shadow):
    # Shadow regions are composited automatically by _s.term.update().
    # Just clear and refill them to ensure correct content.
    for shadow in (bottom_shadow, right_shadow):
        if shadow:
            try:
                shadow.clear()
            except Exception:
                pass


def _draw_frame(win, title):
    # Draw a frame around the inner edges of 'win', with 'title' at the top.

    win_height = win.height
    win_width = win.width
    body_style = _style["body"]

    _draw_box(win, 0, 0, win_height, win_width, body_style, body_style)

    _draw_title(win, title, win_width)


def _jump_to_matches(search_string):
    # Returns a (matches, bad_re) pair for 'search_string': the menu nodes
    # matching every whitespace-separated regex in it, and an error message if
    # one of those regexes did not compile (None otherwise).
    #
    # Lifted out of _jump_to_dialog() so that it can be tested without a
    # terminal, and so that it stays comparable to
    # guiconfig._update_jump_to_matches(), which does the same job.

    try:
        # We could use re.IGNORECASE here instead of lower(), but this is
        # noticeably less jerky while inputting regexes like '.*debug$' (though
        # the '.*' is redundant there). Those probably have bad interactions
        # with re.search(), which matches anywhere in the string.
        #
        # It's not horrible either way. Just a bit smoother.
        prefix = _s.kconf.config_prefix.lower()
        prefix_len = len(prefix)

        regex_searches = [
            re.compile(token[prefix_len:] if token.startswith(prefix) else token).search
            for token in search_string.lower().split()
        ]
    except re.error as e:
        # Only re.compile() raises this; a compiled pattern's search() does not,
        # so nothing below needs to be inside the try
        return [], "Bad regular expression: " + e.msg

    matches = []
    add_match = matches.append

    # Search symbols and choices

    for node in _sorted_sc_nodes():
        # Symbol/choice
        sc = node.item

        for search in regex_searches:
            # Both the name and the prompt might be missing, since we're
            # searching both symbols and choices

            # Does the regex match either the symbol name or the prompt (if
            # any)?
            if not (
                sc.name
                and search(sc.name.lower())
                or node.prompt
                and search(node.prompt[0].lower())
            ):

                # Give up on the first regex that doesn't match, to speed
                # things up a bit when multiple regexes are entered
                break

        else:
            add_match(node)

    # Search menus and comments

    for node in _sorted_menu_comment_nodes():
        for search in regex_searches:
            if not search(node.prompt[0].lower()):
                break
        else:
            add_match(node)

    return matches, None


def _jump_to_dialog():
    # Implements the jump-to dialog, where symbols can be looked up via
    # incremental search and jumped to.
    #
    # Returns True if the user jumped to a symbol, and False if the dialog was
    # canceled.

    edit_box = matches_win = bot_sep_win = help_win = None
    bottom_shadow = right_shadow = None

    try:
        s = ""  # Search text
        prev_s = None  # Previous search text
        s_i = 0  # Search text cursor position
        hscroll = 0  # Horizontal scroll offset

        sel_node_i = 0  # Index of selected row
        scroll = 0  # Index in 'matches' of the top row of the list

        # Edit box at the top
        edit_box = _styled_region("jump-edit")

        # List of matches
        matches_win = _styled_region("list")

        # Bottom separator, with arrows pointing down
        bot_sep_win = _styled_region("separator")

        # Help window with instructions at the bottom
        help_win = _styled_region("help")

        # Give windows their initial size
        _resize_jump_to_dialog(
            edit_box, matches_win, bot_sep_win, help_win, sel_node_i, scroll
        )

        def _jump_to_shadows():
            # Shadow windows cover the dialog area (everything except help win)
            _close_shadow_windows(bottom_shadow, right_shadow)
            sh, sw = _s.term.height, _s.term.width
            dh = sh - len(_JUMP_TO_HELP_LINES) - 1
            return _create_shadow_windows(0, 0, dh, sw, right_y_offset=1)

        bottom_shadow, right_shadow = _jump_to_shadows()

        _s.term.show_cursor(very_visible=True)

        # Functional variants of _select_{next,prev}_menu_entry() that return
        # new (sel_node_i, scroll) tuples instead of mutating globals, to
        # avoid 'nonlocal'.

        def select_next_match():
            if sel_node_i == len(matches) - 1:
                return sel_node_i, scroll

            if sel_node_i + 1 >= scroll + _height(
                matches_win
            ) - _SCROLL_OFFSET and scroll < _max_scroll(matches, matches_win):

                return sel_node_i + 1, scroll + 1

            return sel_node_i + 1, scroll

        def select_prev_match():
            if sel_node_i == 0:
                return sel_node_i, scroll

            if sel_node_i - 1 < scroll + _SCROLL_OFFSET:
                return sel_node_i - 1, max(scroll - 1, 0)

            return sel_node_i - 1, scroll

        while True:
            if s != prev_s:
                # The search text changed. Find new matching nodes.

                prev_s = s
                matches, bad_re = _jump_to_matches(s)

                # Reset scroll and jump to the top of the list of matches
                sel_node_i = scroll = 0

            _draw_jump_to_dialog(
                edit_box,
                matches_win,
                bot_sep_win,
                help_win,
                s,
                s_i,
                hscroll,
                bad_re,
                matches,
                sel_node_i,
                scroll,
            )

            # Refresh shadow windows after all other windows
            _refresh_shadow_windows(bottom_shadow, right_shadow)

            _s.term.update()

            c = _s.term.read_key()

            if c == "\n":
                if matches:
                    _jump_to(matches[sel_node_i])
                    _s.term.hide_cursor()
                    return True

            elif c == "\x1b":  # ESC
                _s.term.hide_cursor()
                return False

            elif c == Key.RESIZE:
                # We adjust the scroll so that the selected node stays visible
                # in the list when the terminal is resized, hence the 'scroll'
                # assignment
                scroll = _resize_jump_to_dialog(
                    edit_box, matches_win, bot_sep_win, help_win, sel_node_i, scroll
                )

                bottom_shadow, right_shadow = _jump_to_shadows()

            elif c == "\x06":  # Ctrl-F
                if matches:
                    _s.term.hide_cursor()
                    _info_dialog(matches[sel_node_i], True)
                    _s.term.show_cursor(very_visible=True)

                    scroll = _resize_jump_to_dialog(
                        edit_box, matches_win, bot_sep_win, help_win, sel_node_i, scroll
                    )

                    bottom_shadow, right_shadow = _jump_to_shadows()

            elif c == Key.DOWN:
                sel_node_i, scroll = select_next_match()

            elif c == Key.UP:
                sel_node_i, scroll = select_prev_match()

            elif c in (Key.PAGE_DOWN, "\x04"):  # Page Down/Ctrl-D
                # Keep it simple. This way we get sane behavior for small
                # windows, etc., for free.
                for _ in range(_PG_JUMP):
                    sel_node_i, scroll = select_next_match()

            # Page Up (no Ctrl-U, as it's already used by the edit box)
            elif c == Key.PAGE_UP:
                for _ in range(_PG_JUMP):
                    sel_node_i, scroll = select_prev_match()

            elif c == Key.END:
                sel_node_i = len(matches) - 1
                scroll = _max_scroll(matches, matches_win)

            elif c == Key.HOME:
                sel_node_i = scroll = 0

            elif c == "\0":  # NUL, ignore
                pass

            else:
                s, s_i, hscroll = _edit_text(c, s, s_i, hscroll, _width(edit_box) - 2)
    finally:
        _close_shadow_windows(bottom_shadow, right_shadow)
        for r in (edit_box, matches_win, bot_sep_win, help_win):
            if r:
                r.close()


def _sorted_sc_nodes():
    # Returns a sorted list of symbol and choice nodes to search. The symbol
    # nodes appear first, sorted by name, and then the choice nodes, sorted by
    # prompt and (secondarily) name.

    if not _s.cached_sc_nodes:
        # Add symbol nodes
        for sym in sorted(_s.kconf.unique_defined_syms, key=lambda sym: sym.name):
            _s.cached_sc_nodes.extend(sym.nodes)

        # Add choice nodes

        choices = sorted(_s.kconf.unique_choices, key=lambda choice: choice.name or "")

        _s.cached_sc_nodes.extend(
            sorted(
                [node for choice in choices for node in choice.nodes],
                key=lambda node: node.prompt[0] if node.prompt else "",
            )
        )

    return _s.cached_sc_nodes


def _sorted_menu_comment_nodes():
    # Returns a list of menu and comment nodes to search, sorted by prompt,
    # with the menus first

    if not _s.cached_menu_comment_nodes:

        def prompt_text(mc):
            return mc.prompt[0]

        _s.cached_menu_comment_nodes.extend(sorted(_s.kconf.menus, key=prompt_text))
        _s.cached_menu_comment_nodes.extend(sorted(_s.kconf.comments, key=prompt_text))

    return _s.cached_menu_comment_nodes


def _resize_jump_to_dialog(
    edit_box, matches_win, bot_sep_win, help_win, sel_node_i, scroll
):
    # Resizes the jump-to dialog to fill the terminal.
    #
    # Returns the new scroll index. We adjust the scroll if needed so that the
    # selected node stays visible.

    screen_height, screen_width = _s.term.height, _s.term.width

    bot_sep_win.resize(1, screen_width)

    help_win_height = len(_JUMP_TO_HELP_LINES)
    matches_win_height = screen_height - help_win_height - 4

    if matches_win_height >= 1:
        edit_box.resize(3, screen_width)
        matches_win.resize(matches_win_height, screen_width)
        help_win.resize(help_win_height, screen_width)

        matches_win.move(3, 0)
        bot_sep_win.move(3 + matches_win_height, 0)
        help_win.move(3 + matches_win_height + 1, 0)
    else:
        # Degenerate case. Give up on nice rendering and just prevent errors.

        matches_win_height = 1

        edit_box.resize(screen_height, screen_width)
        matches_win.resize(1, screen_width)
        help_win.resize(1, screen_width)

        for win in matches_win, bot_sep_win, help_win:
            win.move(0, 0)

    # Adjust the scroll so that the selected row is still within the window, if
    # needed
    if sel_node_i - scroll >= matches_win_height:
        return sel_node_i - matches_win_height + 1
    return scroll


def _draw_jump_to_dialog(
    edit_box,
    matches_win,
    bot_sep_win,
    help_win,
    s,
    s_i,
    hscroll,
    bad_re,
    matches,
    sel_node_i,
    scroll,
):

    edit_width = _width(edit_box) - 2

    #
    # Update list of matches
    #

    matches_win.clear()

    # Draw box border around matches window
    matches_win_height = matches_win.height
    matches_win_width = matches_win.width
    _draw_box(
        matches_win,
        0,
        0,
        matches_win_height,
        matches_win_width,
        _style["list"],
        _style["list"],
    )

    # Calculate max scroll for scrollbar (account for borders)
    # Actual visible height is matches_win_height - 2 (top and bottom borders)
    max_scroll = max(0, len(matches) - (matches_win_height - 2)) if matches else 0

    # Determine text display width (leave space for borders and scrollbar if needed)
    if max_scroll > 0:
        text_display_width = matches_win_width - 4  # borders(2) + scrollbar space(2)
    else:
        text_display_width = matches_win_width - 2  # borders(2)

    if matches:
        # Draw items inside the box (offset by 1 row and 1 column for borders)
        for i in range(scroll, min(scroll + _height(matches_win) - 2, len(matches))):

            node = matches[i]

            if isinstance(node.item, (Symbol, Choice)):
                node_str = uicommon.name_and_val_str(node.item)
                if node.prompt:
                    node_str += f' "{node.prompt[0]}"'
            elif node.item == MENU:
                node_str = f'menu "{node.prompt[0]}"'
            else:  # node.item == COMMENT
                node_str = f'comment "{node.prompt[0]}"'

            # Truncate and pad text to fill full row width
            node_str = node_str[:text_display_width].ljust(text_display_width)

            matches_win.write(
                1 + i - scroll,  # Offset by 1 for top border
                1,  # Offset by 1 for left border
                node_str,
                _style["selection" if i == sel_node_i else "list"],
            )

        # Draw scrollbar if content is scrollable
        if max_scroll > 0:
            _draw_scrollbar(
                matches_win,
                scroll,
                max_scroll,
                matches_win_width - 2,
                1,
                matches_win_height - 2,
                _style["list"],
                _style.get("selection", _style["list"]),
            )

    else:
        matches_win.write(1, 1, bad_re or "No matches", _style["list"])

    #
    # Update bottom separator line
    #

    bot_sep_win.clear()

    #
    # Update help window at bottom
    #

    help_win.clear()

    for i, line in enumerate(_JUMP_TO_HELP_LINES):
        help_win.write(i, 0, line, _style["help"])

    #
    # Update edit box. We do this last since it makes it handy to position the
    # cursor.
    #

    edit_box.clear()

    _draw_frame(edit_box, "Jump to symbol/choice/menu/comment")

    visible_s = s[hscroll : hscroll + edit_width]
    edit_box.write(1, 1, visible_s, _style["jump-edit"])

    _s.term.set_cursor(edit_box, 1, 1 + s_i - hscroll)


def _info_dialog(node, from_jump_to_dialog):
    # Shows a dialog window with information about 'node', matching mconf style.
    #
    # If 'from_jump_to_dialog' is True, the information dialog was opened from
    # within the jump-to-dialog. In this case, we make '/' from within the
    # information dialog just return, to avoid a confusing recursive invocation
    # of the jump-to-dialog.

    win = None
    bottom_shadow = right_shadow = None

    try:
        win = _styled_region("body")

        # Get lines of help text in mconf format
        lines = _info_str_mconf(node).split("\n")

        # Index of first row in 'lines' to show
        scroll = 0

        # Give window its initial size
        _resize_info_dialog(win, node, lines)

        bottom_shadow, right_shadow = _create_shadow_for_win(win)
        while True:
            _draw_main()

            _draw_info_dialog(win, node, lines, scroll)

            _refresh_shadow_windows(bottom_shadow, right_shadow)

            _s.term.update()

            c = _s.term.read_key()

            if c == Key.RESIZE:
                _resize_main()
                _resize_info_dialog(win, node, lines)
                _close_shadow_windows(bottom_shadow, right_shadow)
                bottom_shadow, right_shadow = _create_shadow_for_win(win)

            elif c in (Key.DOWN, "j", "J"):
                if scroll < _max_scroll_info(lines, win):
                    scroll += 1

            elif c in (Key.PAGE_DOWN, "\x04"):  # Page Down/Ctrl-D
                scroll = min(scroll + _PG_JUMP, _max_scroll_info(lines, win))

            elif c in (Key.PAGE_UP, "\x15"):  # Page Up/Ctrl-U
                scroll = max(scroll - _PG_JUMP, 0)

            elif c in (Key.END, "G"):
                scroll = _max_scroll_info(lines, win)

            elif c in (Key.HOME, "g"):
                scroll = 0

            elif c in (Key.UP, "k", "K"):
                if scroll > 0:
                    scroll -= 1

            elif c == "/":
                # Support starting a search from within the information dialog

                if from_jump_to_dialog:
                    return  # Avoid recursion

                if _jump_to_dialog():
                    return  # Jumped to a symbol. Cancel the info dialog.

                # Stay in the information dialog if the jump-to dialog was
                # canceled. Resize it in case the terminal was resized while
                # the fullscreen jump-to dialog was open.
                _resize_main()
                _resize_info_dialog(win, node, lines)
                _close_shadow_windows(bottom_shadow, right_shadow)
                bottom_shadow, right_shadow = _create_shadow_for_win(win)

            elif c in (
                Key.LEFT,
                Key.BACKSPACE,
                "\x1b",  # ESC
                "\n",  # Enter
                " ",  # Space
                "q",
                "Q",
                "h",
                "H",
            ):

                return
    finally:
        _close_shadow_windows(bottom_shadow, right_shadow)
        if win:
            win.close()


def _info_dialog_title(node):
    # Returns the title string for an info dialog based on the node type

    if isinstance(node.item, (Symbol, Choice)) and node.prompt:
        return node.prompt[0]
    if isinstance(node.item, Symbol) and node.item.name:
        return node.item.name
    if isinstance(node.item, Choice):
        return "Choice"
    return ""


def _resize_info_dialog(win, node, lines):
    # Resizes the info dialog to match mconf's dialog_textbox() -- nearly
    # full-screen: height = screen_height - 4, width = screen_width - 5,
    # centered on the terminal.

    screen_height, screen_width = _s.term.height, _s.term.width

    # Match dialog_textbox() sizing
    dlg_height = screen_height - 4
    dlg_width = screen_width - 5

    # Clamp to minimums (TEXTBOX_HEIGHT_MIN=8, TEXTBOX_WIDTH_MIN=8)
    if dlg_height < 8:
        dlg_height = min(8, screen_height)
    if dlg_width < 8:
        dlg_width = min(8, screen_width)

    win.resize(dlg_height, dlg_width)
    win.move((screen_height - dlg_height) // 2, (screen_width - dlg_width) // 2)


def _draw_info_dialog(win, node, lines, scroll):
    # Draws the info dialog matching mconf's dialog_textbox() layout:
    #   Row 0:           top border with title
    #   Rows 1..h-4:     text content (boxh = height - 4)
    #   Row h-3:         separator (LTEE + HLINE + RTEE) with scroll %
    #   Row h-2:         Exit button
    #   Row h-1:         bottom border

    win_height = win.height
    win_width = win.width

    title = _info_dialog_title(node)

    body_style = _style["body"]

    win.clear()

    _draw_box(win, 0, 0, win_height, win_width, body_style, body_style)

    _draw_title(win, title, win_width)

    _draw_separator(win, win_height - 3, win_width)

    # Text area: rows 1 .. height-4
    text_height = win_height - 4
    text_width = win_width - 3  # borders + 1 space margin
    max_scroll = _max_scroll_info(lines, win)

    for i in range(text_height):
        line_idx = scroll + i
        if line_idx < len(lines):
            win.write(1 + i, 2, lines[line_idx][:text_width], body_style)

    # Scroll percentage on the separator line (matching mconf position)
    percentage = int((float(scroll) / max_scroll) * 100) if max_scroll > 0 else 100
    percent_str = f"({percentage:3d}%)"
    if win_width > len(percent_str) + 2:
        win.write(
            win_height - 3, win_width - len(percent_str) - 2, percent_str, body_style
        )

    # Exit button at height - 2, centered (matching dialog_textbox).
    # Clamp x to protect the left border on narrow terminals.
    _print_button(win, " Exit ", win_height - 2, max(win_width // 2 - 4, 1), True)


def _max_scroll_info(lines, win):
    # Calculate max scroll for info dialog
    # Text area height = win_height - 4 (top border, separator, button, bottom border)
    win_height = _height(win)
    text_area_height = win_height - 4
    return max(0, len(lines) - text_area_height)


def _info_str_mconf(node):
    # Returns information about the menu node 'node' in mconf format,
    # matching menu_get_ext_help() + get_symbol_str() from mconf.c.

    sc = node.item if isinstance(node.item, (Symbol, Choice)) else None

    s = ""

    # MENU/COMMENT nodes may not have .help set (only initialized by
    # _parse_props when a 'help' keyword exists in the Kconfig source).
    node_help = getattr(node, "help", None)

    # CONFIG_NAME header (only when node has help and symbol/choice has name)
    if node_help and sc and sc.name:
        s += f"CONFIG_{sc.name}:\n\n"

    # Help text or default
    if node_help:
        s += node_help.rstrip() + "\n"
    else:
        s += "There is no help available for this option.\n"

    # get_symbol_str() equivalent -- only for symbols/choices
    if sc:
        # Symbol: NAME [=VALUE] and Type (only if named)
        if sc.name:
            s += f"Symbol: {sc.name} [={sc.str_value}]\n"
            s += f"Type  : {TYPE_TO_STR[sc.orig_type].lower()}\n"
            if isinstance(sc, Symbol) and sc.orig_type in (INT, HEX):
                for low, high, cond in sc.orig_ranges:
                    if expr_value(cond):
                        s += f"Range : [{low.str_value}..{high.str_value}]\n"
                        break

        # Definitions with prompts first
        for n in sc.nodes:
            if n.prompt:
                s += f"Defined at {n.filename}:{n.linenr}\n"
                s += f"  Prompt: {n.prompt[0]}\n"
                if n.dep is not _s.kconf.y:
                    s += f"  Depends on: {uicommon.expr_str_with_values(n.dep)}\n"
                # Location hierarchy (matching get_prompt_str in mconf)
                submenu = []
                m = n
                while m is not _s.kconf.top_node and len(submenu) < 8:
                    submenu.append(m)
                    m = m.parent
                s += "  Location:\n"
                for j in range(len(submenu)):
                    pm = submenu[len(submenu) - 1 - j]
                    indent = 2 * j + 4
                    prompt_text = (
                        pm.prompt[0] if pm.prompt else standard_sc_expr_str(pm.item)
                    )
                    s += "{}-> {}".format(" " * indent, prompt_text)
                    if isinstance(pm.item, (Symbol, Choice)):
                        name = pm.item.name if pm.item.name else "<choice>"
                        s += f" ({name} [={pm.item.str_value}])"
                    s += "\n"

        # Definitions without prompts
        for n in sc.nodes:
            if not n.prompt:
                s += f"Defined at {n.filename}:{n.linenr}\n"
                if n.dep is not _s.kconf.y:
                    s += f"  Depends on: {uicommon.expr_str_with_values(n.dep)}\n"

        # Selects (symbols only)
        if isinstance(sc, Symbol) and sc.selects:
            sel_strs = [
                uicommon.expr_str_with_values(sel_sym)
                for sel_sym, cond in sc.orig_selects
            ]
            s += "Selects: {}\n".format(" && ".join(sel_strs))

        # Selected by
        if isinstance(sc, Symbol) and sc.rev_dep is not _s.kconf.n:
            for val, label in (
                (2, "Selected by [y]:"),
                (1, "Selected by [m]:"),
                (0, "Selected by [n]:"),
            ):
                sels = [
                    si for si in split_expr(sc.rev_dep, OR) if expr_value(si) == val
                ]
                if sels:
                    s += label + "\n"
                    for si in sels:
                        parts = split_expr(si, AND)
                        if parts and isinstance(parts[0], Symbol):
                            s += f"  - {parts[0].name}\n"

        # Implies (symbols only)
        if isinstance(sc, Symbol) and sc.implies:
            imp_strs = [
                uicommon.expr_str_with_values(imp_sym)
                for imp_sym, cond in sc.orig_implies
            ]
            s += "Implies: {}\n".format(" && ".join(imp_strs))

        # Implied by
        if isinstance(sc, Symbol) and sc.weak_rev_dep is not _s.kconf.n:
            for val, label in (
                (2, "Implied by [y]:"),
                (1, "Implied by [m]:"),
                (0, "Implied by [n]:"),
            ):
                imps = [
                    si
                    for si in split_expr(sc.weak_rev_dep, OR)
                    if expr_value(si) == val
                ]
                if imps:
                    s += label + "\n"
                    for si in imps:
                        parts = split_expr(si, AND)
                        if parts and isinstance(parts[0], Symbol):
                            s += f"  - {parts[0].name}\n"

        s += "\n\n"

    return s


def _info_str(node):
    # Returns information about the menu node 'node' as a string.
    #
    # The helper functions are responsible for adding newlines. This allows
    # them to return "" if they don't want to add any output.

    if isinstance(node.item, Symbol):
        sym = node.item

        return (
            _name_info(sym)
            + _prompt_info(sym)
            + f"Type: {TYPE_TO_STR[sym.type]}\n"
            + _value_info(sym)
            + _help_info(sym)
            + _direct_dep_info(sym)
            + _defaults_info(sym)
            + _select_imply_info(sym)
            + _kconfig_def_info(sym)
        )

    if isinstance(node.item, Choice):
        choice = node.item

        return (
            _name_info(choice)
            + _prompt_info(choice)
            + f"Type: {TYPE_TO_STR[choice.type]}\n"
            + f"Mode: {choice.str_value}\n"
            + _help_info(choice)
            + uicommon.choice_syms_info(choice)
            + _direct_dep_info(choice)
            + _defaults_info(choice)
            + _kconfig_def_info(choice)
        )

    return _kconfig_def_info(node)  # node.item in (MENU, COMMENT)


def _name_info(sc):
    # Returns a string with the name of the symbol/choice. Names are optional
    # for choices.

    return f"Name: {sc.name}\n" if sc.name else ""


def _prompt_info(sc):
    # Returns a string listing the prompts of 'sc' (Symbol or Choice)

    s = ""

    for node in sc.nodes:
        if node.prompt:
            s += f"Prompt: {node.prompt[0]}\n"

    return s


def _value_info(sym):
    # Returns a string showing 'sym's value

    # Only put quotes around the value for string symbols
    s = "Value: {}\n".format(
        f'"{sym.str_value}"' if sym.orig_type == STRING else sym.str_value
    )

    # Add origin information to explain where the value comes from
    origin = sym.origin
    if origin:
        kind, sources = origin
        if kind == "select":
            if sources:
                s += "  (selected by: {})\n".format(", ".join(sources))
        elif kind == "imply":
            if sources:
                s += "  (implied by: {})\n".format(", ".join(sources))
        elif kind == "default":
            s += "  (from default)\n"
        elif kind == "assign":
            s += "  (user assigned)\n"

    return s


def _help_info(sc):
    # Returns a string with the help text(s) of 'sc' (Symbol or Choice).
    # Symbols and choices defined in multiple locations can have multiple help
    # texts.

    s = "\n"

    for node in sc.nodes:
        if node.help is not None:
            s += f"Help:\n\n{_indent(node.help, 2)}\n\n"

    return s


def _direct_dep_info(sc):
    # Returns a string describing the direct dependencies of 'sc' (Symbol or
    # Choice). The direct dependencies are the OR of the dependencies from each
    # definition location. The dependencies at each definition location come
    # from 'depends on' and dependencies inherited from parent items.

    return (
        ""
        if sc.direct_dep is _s.kconf.y
        else f"Direct dependencies (={TRI_TO_STR[expr_value(sc.direct_dep)]}):\n{uicommon.split_expr_info(sc.direct_dep, 2)}\n"
    )


def _defaults_info(sc):
    # Returns a string describing the defaults of 'sc' (Symbol or Choice)

    if not sc.defaults:
        return ""

    s = "Default"
    if len(sc.defaults) > 1:
        s += "s"
    s += ":\n"

    for val, cond in sc.orig_defaults:
        s += "  - "
        if isinstance(sc, Symbol):
            s += uicommon.expr_str_with_values(val)

            # Skip the tristate value hint if the expression is just a single
            # symbol. _expr_str() already shows its value as a string.
            #
            # This also avoids showing the tristate value for string/int/hex
            # defaults, which wouldn't make any sense.
            if isinstance(val, tuple):
                s += f"  (={TRI_TO_STR[expr_value(val)]})"
        else:
            # Don't print the value next to the symbol name for choice
            # defaults, as it looks a bit confusing
            s += val.name
        s += "\n"

        if cond is not _s.kconf.y:
            s += f"    Condition (={TRI_TO_STR[expr_value(cond)]}):\n{uicommon.split_expr_info(cond, 4)}"

    return s + "\n"


def _select_imply_info(sym):
    # Returns a string with information about which symbols 'select' or 'imply'
    # 'sym'. The selecting/implying symbols are grouped according to which
    # value they select/imply 'sym' to (n/m/y).

    def sis(expr, val, title):
        # sis = selects/implies
        sis = [si for si in split_expr(expr, OR) if expr_value(si) == val]
        if not sis:
            return ""

        res = title
        for si in sis:
            res += f"  - {split_expr(si, AND)[0].name}\n"
        return res + "\n"

    s = ""

    if sym.rev_dep is not _s.kconf.n:
        s += sis(sym.rev_dep, 2, "Symbols currently y-selecting this symbol:\n")
        s += sis(sym.rev_dep, 1, "Symbols currently m-selecting this symbol:\n")
        s += sis(
            sym.rev_dep, 0, "Symbols currently n-selecting this symbol (no effect):\n"
        )

    if sym.weak_rev_dep is not _s.kconf.n:
        s += sis(sym.weak_rev_dep, 2, "Symbols currently y-implying this symbol:\n")
        s += sis(sym.weak_rev_dep, 1, "Symbols currently m-implying this symbol:\n")
        s += sis(
            sym.weak_rev_dep,
            0,
            "Symbols currently n-implying this symbol (no effect):\n",
        )

    return s


def _kconfig_def_info(item):
    # Returns a string with the definition of 'item' in Kconfig syntax,
    # together with the definition location(s) and their include and menu paths

    nodes = [item] if isinstance(item, MenuNode) else item.nodes

    s = "Kconfig definition{}, with parent deps. propagated to 'depends on'\n".format(
        "s" if len(nodes) > 1 else ""
    )
    s += (len(s) - 1) * "="

    for node in nodes:
        s += (
            "\n\n"
            f"At {node.filename}:{node.linenr}\n"
            f"{uicommon.include_path_info(node)}"
            f"Menu path: {_menu_path_info(node)}\n\n"
            f"{_indent(node.custom_str(uicommon.name_and_val_str), 2)}"
        )

    return s


def _menu_path_info(node):
    # Returns a string describing the menu path leading up to 'node'

    path = ""

    while node.parent is not _s.kconf.top_node:
        node = node.parent

        # Promptless choices might appear among the parents. Use
        # standard_sc_expr_str() for them, so that they show up as
        # '<choice (name if any)>'.
        path = (
            " -> "
            + (node.prompt[0] if node.prompt else standard_sc_expr_str(node.item))
            + path
        )

    return "(Top)" + path


def _indent(s, n):
    # Returns 's' with each line indented 'n' spaces.

    return "\n".join(n * " " + line for line in s.split("\n"))


def _styled_region(style):
    # Returns a new rawterm Region with style 'style' and space as the fill
    # character. The initial dimensions are (1, 1), so the region needs to be
    # sized and positioned separately.

    win = _s.term.region(1, 1)
    win.fill(_style[style])
    return win


def _max_scroll(lst, win):
    # Assuming 'lst' is a list of items to be displayed in 'win',
    # returns the maximum number of steps 'win' can be scrolled down.
    # We stop scrolling when the bottom item is visible.

    return max(0, len(lst) - _height(win))


def _edit_text(c, s, i, hscroll, width):
    # Implements text editing commands for edit boxes. Takes a character (which
    # could also be e.g. Key.LEFT) and the edit box state, and returns the new
    # state after the character has been processed.
    #
    # c:
    #   Character from user
    #
    # s:
    #   Current contents of string
    #
    # i:
    #   Current cursor index in string
    #
    # hscroll:
    #   Index in s of the leftmost character in the edit box, for horizontal
    #   scrolling
    #
    # width:
    #   Width in characters of the edit box
    #
    # Return value:
    #   An (s, i, hscroll) tuple for the new state

    if c == Key.LEFT:
        if i > 0:
            i -= 1

    elif c == Key.RIGHT:
        if i < len(s):
            i += 1

    elif c in (Key.HOME, "\x01"):  # Ctrl-A
        i = 0

    elif c in (Key.END, "\x05"):  # Ctrl-E
        i = len(s)

    elif c == Key.BACKSPACE:
        if i > 0:
            s = s[: i - 1] + s[i:]
            i -= 1

    elif c == Key.DELETE:
        s = s[:i] + s[i + 1 :]

    elif c == "\x17":  # Ctrl-W
        # The \W removes characters like ',' one at a time
        new_i = re.search(r"(?:\w*|\W)\s*$", s[:i]).start()
        s = s[:new_i] + s[i:]
        i = new_i

    elif c == "\x0b":  # Ctrl-K
        s = s[:i]

    elif c == "\x15":  # Ctrl-U
        s = s[i:]
        i = 0

    elif isinstance(c, str):
        # Insert character
        s = s[:i] + c + s[i:]
        i += 1

    # Adjust the horizontal scroll so that the cursor never touches the left or
    # right edges of the edit box, except when it's at the beginning or the end
    # of the string
    if i < hscroll + _SCROLL_OFFSET:
        hscroll = max(i - _SCROLL_OFFSET, 0)
    elif i >= hscroll + width - _SCROLL_OFFSET:
        max_scroll = max(len(s) - width + 1, 0)
        hscroll = min(i - width + _SCROLL_OFFSET + 1, max_scroll)

    return s, i, hscroll


def _load_save_info():
    # Returns an information string for load/save dialog boxes

    return "(Relative to {})\n\nRefer to your home directory with ~".format(
        os.path.join(os.getcwd(), "")
    )


def _msg(title, text):
    # Pops up a message dialog that can be dismissed with Space/Enter/ESC

    _key_dialog(title, text, " \n")


def _error(text):
    # Pops up an error dialog that can be dismissed with Space/Enter/ESC

    _msg("Error", text)


def _get_force_info(sym):
    # Returns a string indicating what's forcing a symbol's value, or None
    # if the value is not being forced by select/imply.
    #
    # Example return values:
    #   " [selected by FOO]"
    #   " [implied by BAR, BAZ]"

    if sym.orig_type not in (BOOL, TRISTATE):
        return None

    origin = sym.origin
    if not origin:
        return None

    kind, sources = origin
    if kind not in ("select", "imply") or not sources:
        return None

    # Format the force info string
    prefix = "selected by" if kind == "select" else "implied by"
    sym_names = _extract_controlling_symbols(sources)

    if not sym_names:
        return None

    # Show up to 2 symbols to keep line length reasonable
    if len(sym_names) <= 2:
        return " [{} {}]".format(prefix, ", ".join(sym_names))

    return " [{} {}, +{}]".format(prefix, ", ".join(sym_names[:2]), len(sym_names) - 2)


def _node_str(node):
    # Returns the complete menu entry text for a menu node.
    #
    # Example return value: "[*] Support for X"

    # Calculate the indent to print the item with by checking how many levels
    # above it the closest 'menuconfig' item is (this includes menus and
    # choices as well as menuconfig symbols)
    indent = 0
    parent = node.parent
    while not parent.is_menuconfig:
        indent += _SUBMENU_INDENT
        parent = parent.parent

    # This approach gives nice alignment for empty string symbols ("()  Foo")
    s = "{:{}}".format(_value_str(node), 3 + indent)

    if _should_show_name(node):
        if isinstance(node.item, Symbol):
            s += f" <{node.item.name}>"
        else:
            # For choices, use standard_sc_expr_str(). That way they show up as
            # '<choice (name if any)>'.
            s += " " + standard_sc_expr_str(node.item)

    if node.prompt:
        if node.item == COMMENT:
            s += f" *** {node.prompt[0]} ***"
        else:
            s += " " + node.prompt[0]

        if isinstance(node.item, Symbol):
            sym = node.item

            # Print "(NEW)" next to symbols without a user value (from e.g. a
            # .config), but skip it for choice symbols in choices in y mode,
            # and for symbols of UNKNOWN type (which generate a warning though)
            if (
                sym.user_value is None
                and sym.orig_type
                and not (sym.choice and sym.choice.tri_value == 2)
            ):

                s += " (NEW)"

            # Show what's controlling this symbol if it's selected/implied
            force_info = _get_force_info(sym)
            if force_info:
                s += force_info

    if isinstance(node.item, Choice) and node.item.tri_value == 2:
        # Print the prompt of the selected symbol after the choice for
        # choices in y mode
        sym = node.item.selection
        if sym:
            for sym_node in sym.nodes:
                # Use the prompt used at this choice location, in case the
                # choice symbol is defined in multiple locations
                if sym_node.parent is node and sym_node.prompt:
                    s += f" ({sym_node.prompt[0]})"
                    break
            else:
                # If the symbol isn't defined at this choice location, then
                # just use whatever prompt we can find for it
                for sym_node in sym.nodes:
                    if sym_node.prompt:
                        s += f" ({sym_node.prompt[0]})"
                        break

    # Print "--->" next to nodes that have menus that can potentially be
    # entered. Print "----" if the menu is empty. We don't allow those to be
    # entered.
    if node.is_menuconfig:
        s += "  --->" if _shown_nodes(node) else "  ----"

    return s


def _should_show_name(node):
    # Returns True if 'node' is a symbol or choice whose name should shown (if
    # any, as names are optional for choices)

    # The 'not node.prompt' case only hits in show-all mode, for promptless
    # symbols and choices
    return not node.prompt or (_s.show_name and isinstance(node.item, (Symbol, Choice)))


def _value_str(node):
    # Returns the value part ("[*]", "<M>", "(foo)" etc.) of a menu node

    item = node.item

    if item in (MENU, COMMENT):
        return ""

    # Wouldn't normally happen, and generates a warning
    if not item.orig_type:
        return ""

    if item.orig_type in (STRING, INT, HEX):
        return f"({item.str_value})"

    # BOOL or TRISTATE

    if uicommon.is_y_mode_choice_sym(item):
        return "(X)" if item.choice.selection is item else "( )"

    tri_val_str = (" ", "M", "*")[item.tri_value]

    if len(item.assignable) <= 1:
        # Pinned to a single value
        return "" if isinstance(item, Choice) else f"-{tri_val_str}-"

    if item.type == BOOL:
        return f"[{tri_val_str}]"

    # item.type == TRISTATE
    if item.assignable == (1, 2):
        return f"{{{tri_val_str}}}"  # {M}/{*}
    return f"<{tri_val_str}>"


def _check_valid(sym, s):
    # Returns True if the string 's' is a well-formed value for 'sym'.
    # Otherwise, displays an error and returns False.

    if sym.orig_type not in (INT, HEX):
        return True  # Anything goes for non-int/hex symbols

    base = 10 if sym.orig_type == INT else 16
    try:
        int(s, base)
    except ValueError:
        _error(f"'{s}' is a malformed {TYPE_TO_STR[sym.orig_type]} value")
        return False

    for low_sym, high_sym, cond, _ in sym.ranges:
        if expr_value(cond):
            low_s = low_sym.str_value
            high_s = high_sym.str_value

            if not int(low_s, base) <= int(s, base) <= int(high_s, base):
                _error(f"{s} is outside the range {low_s}-{high_s}")
                return False

            break

    return True


def _warn(*args):
    # Temporarily exits terminal mode and prints a warning to stderr.
    # The warning would get lost in terminal mode.
    try:
        _s.term.suspend()
    except Exception:
        pass
    print("menuconfig warning: ", end="", file=sys.stderr)
    print(*args, file=sys.stderr)
    try:
        _s.term.resume()
    except Exception:
        pass


def _change_c_lc_ctype_to_utf8():
    # See _CHANGE_C_LC_CTYPE_TO_UTF8

    if _IS_WINDOWS:
        # Windows rarely has issues here, and the PEP 538 implementation avoids
        # changing the locale on it. None of the UTF-8 locales below were
        # supported from some quick testing either. Play it safe.
        return

    def try_set_locale(loc):
        try:
            locale.setlocale(locale.LC_CTYPE, loc)
            return True
        except locale.Error:
            return False

    # Is LC_CTYPE set to the C locale?
    if locale.setlocale(locale.LC_CTYPE) == "C":
        # This list was taken from the PEP 538 implementation in the CPython
        # code, in Python/pylifecycle.c
        for loc in "C.UTF-8", "C.utf8", "UTF-8":
            if try_set_locale(loc):
                # LC_CTYPE successfully changed
                return


if __name__ == "__main__":
    _main()
