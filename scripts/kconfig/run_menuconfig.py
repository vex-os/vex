#!/usr/bin/env python3
"""
Runs menuconfig with a custom style unless MENUCONFIG_STYLE is set
"""

import os
import runpy

# Same layout as lxdialog's blue theme, but with muted 256-color
# palette entries instead of the saturated ANSI blue and white:
#  60 #5f5f87 slate (screen)    252 #d0d0d0 light gray (dialog)
#  24 #005f87 deep teal (bar)   235 #262626 near-black (text)
SOFT_STYLE = " ".join([
    "linux",
    "screen=fg:152,bg:60,bold",
    "path=fg:255,bg:60,bold",
    "separator=fg:254,bg:60",
    "help=fg:254,bg:60",
    "frame=fg:255,bg:60,bold",
    "dialog-frame=frame",
    "shadow=fg:237,bg:237",
    "list=fg:235,bg:252",
    "body=list",
    "dialog=list",
    "text=list",
    "show-help=list",
    "menubox=fg:243,bg:252",
    "menubox-border=fg:255,bg:252",
    "border=menubox-border",
    "title=fg:24,bg:252,bold",
    "edit=fg:235,bg:255",
    "jump-edit=edit",
    "selection=fg:255,bg:24,bold",
    "inv-list=fg:131,bg:252",
    "inv-selection=fg:217,bg:24,bold",
    "button-active=selection",
    "button-label-active=selection",
    "button-key-active=fg:222,bg:24,bold",
    "button-inactive=list",
    "button-label-inactive=fg:235,bg:252,bold",
    "button-key-inactive=fg:131,bg:252",
])

os.environ.setdefault("MENUCONFIG_STYLE", SOFT_STYLE)

runpy.run_module("menuconfig", run_name="__main__", alter_sys=True)
