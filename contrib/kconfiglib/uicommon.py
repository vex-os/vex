# Copyright (c) 2011-2019 Ulf Magnusson
# SPDX-License-Identifier: ISC

"""
Presentation helpers shared by the terminal and Tk configuration interfaces.

menuconfig.py and guiconfig.py show the same information about a symbol; they
differ only in how they paint it. Everything that turns a MenuNode or an
expression into text, with no terminal and no Tk anywhere in it, lives here so
that the two tools cannot answer the same question differently.

These are pure functions of the objects handed to them. That is what makes
them testable on their own, and it is the reason to keep them out of the two
tools: a helper that reads a module global from menuconfig.py could not be
called from guiconfig.py, and vice versa.
"""

from kconfiglib import (
    AND,
    HEX,
    INT,
    MENU,
    OR,
    STRING,
    TRI_TO_STR,
    Choice,
    Symbol,
    expr_str,
    expr_value,
    split_expr,
    standard_sc_expr_str,
)


def is_num(name):
    # Heuristic to see if a symbol name looks like a number, for nicer output
    # when printing expressions. Things like 16 are actually symbol names, only
    # they get their name as their value when the symbol is undefined.

    try:
        int(name)
    except ValueError:
        if not name.startswith(("0x", "0X")):
            return False

        try:
            int(name, 16)
        except ValueError:
            return False

    return True


def is_y_mode_choice_sym(item):
    # The choice mode is an upper bound on the visibility of choice symbols, so
    # we can check the choice symbols' own visibility to see if the choice is
    # in y mode.
    #
    # 'is not None' so that a non-choice symbol yields False rather than None
    return isinstance(item, Symbol) and item.choice is not None and item.visibility == 2


def parent_menu(node):
    # Returns the menu node of the menu that contains 'node'. In addition to
    # proper 'menu's, this might also be a 'menuconfig' symbol or a 'choice'.
    # "Menu" here means a menu in the interface.

    menu = node.parent
    while not menu.is_menuconfig:
        menu = menu.parent
    return menu


def visible(node):
    # Returns True if the node should appear in the menu (outside show-all
    # mode)

    return (
        node.prompt
        and expr_value(node.prompt[1])
        and not (node.item == MENU and not expr_value(node.visibility))
    )


def changeable(node):
    # Returns True if the value if 'node' can be changed

    sc = node.item

    if not isinstance(sc, (Symbol, Choice)):
        return False

    # This will hit for invisible symbols, which appear in show-all mode and
    # when an invisible symbol has visible children (which can happen e.g. for
    # symbols with optional prompts)
    if not (node.prompt and expr_value(node.prompt[1])):
        return False

    return (
        sc.orig_type in (STRING, INT, HEX)
        or len(sc.assignable) > 1
        or is_y_mode_choice_sym(sc)
    )


def range_info(sym):
    # Returns a string with information about the valid range for the symbol
    # 'sym', or None if 'sym' doesn't have a range

    if sym.orig_type in (INT, HEX):
        for low, high, cond, _ in sym.ranges:
            if expr_value(cond):
                return f"Range: {low.str_value}-{high.str_value}"

    return None


def include_path_info(node):
    if not node.include_path:
        # In the top-level Kconfig file
        return ""

    return "Included via {}\n".format(
        " -> ".join(f"{filename}:{linenr}" for filename, linenr in node.include_path)
    )


def choice_syms_info(choice):
    # Returns a string listing the choice symbols in 'choice'. Adds
    # "(selected)" next to the selected one.

    s = "Choice symbols:\n"

    for sym in choice.syms:
        s += "  - " + sym.name
        if sym is choice.selection:
            s += " (selected)"
        s += "\n"

    return s + "\n"


def name_and_val_str(sc):
    # Custom symbol/choice printer that shows symbol values after symbols

    # Show the values of non-constant (non-quoted) symbols that don't look like
    # numbers. Things like 123 are actually symbol references, and only work as
    # expected due to undefined symbols getting their name as their value.
    # Showing the symbol value for those isn't helpful though.
    if isinstance(sc, Symbol) and not sc.is_constant and not is_num(sc.name):
        if not sc.nodes:
            # Undefined symbol reference
            return f"{sc.name}(undefined/n)"

        return f"{sc.name}(={sc.str_value})"

    # For other items, use the standard format
    return standard_sc_expr_str(sc)


def expr_str_with_values(expr):
    # Custom expression printer that shows symbol values
    return expr_str(expr, name_and_val_str)


def split_expr_info(expr, indent):
    # Returns a string with 'expr' split into its top-level && or || operands,
    # with one operand per line, together with the operand's value. This is
    # usually enough to get something readable for long expressions. A fancier
    # recursive thingy would be possible too.
    #
    # indent:
    #   Number of leading spaces to add before the split expression.

    if len(split_expr(expr, AND)) > 1:
        split_op = AND
        op_str = "&&"
    else:
        split_op = OR
        op_str = "||"

    s = ""
    for i, term in enumerate(split_expr(expr, split_op)):
        s += "{}{} {}".format(
            indent * " ", "  " if i == 0 else op_str, expr_str_with_values(term)
        )

        # Don't bother showing the value hint if the expression is just a
        # single symbol. _expr_str() already shows its value.
        if isinstance(term, tuple):
            s += f"  (={TRI_TO_STR[expr_value(term)]})"

        s += "\n"

    return s
