#!/usr/bin/env python3
"""
Generates a C header exposing every Kconfig symbol as a CONFIG_
macro, same as Linux's autoconf.h: enabled bools are defined to 1,
disabled ones are left undefined
"""

import argparse
import sys

import kconfiglib


def c_string(value):
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def c_definition(sym):
    name = "CONFIG_" + sym.name
    value = sym.str_value

    if sym.orig_type == kconfiglib.BOOL:
        return f"#define {name} 1" if value == "y" else None
    if sym.orig_type == kconfiglib.INT:
        return f"#define {name} {value or '0'}"
    if sym.orig_type == kconfiglib.HEX:
        return f"#define {name} {value or '0x0'}"
    if sym.orig_type == kconfiglib.STRING:
        return f"#define {name} {c_string(value)}"

    raise ValueError(f"{sym.name}: unsupported symbol type {kconfiglib.TYPE_TO_STR[sym.orig_type]}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kconfig", required=True, help="Top-level Kconfig file")
    parser.add_argument("--config", required=True, help="Configuration file to read")
    parser.add_argument("--output", required=True, help="Header file to write")
    parser.add_argument("--config-hash", help="Unused; it changes the command when the configuration changes")
    args = parser.parse_args()

    kconf = kconfiglib.Kconfig(args.kconfig, warn_to_stderr=False, suppress_traceback=True)
    kconf.load_config(args.config)

    lines = [
        "/* Generated from Kconfig by scripts/kconfig/gen_config_h.py, do not edit */",
        "#pragma once",
        "",
    ]

    for sym in kconf.unique_defined_syms:
        try:
            definition = c_definition(sym)
        except ValueError as ex:
            print(f"error: {ex}", file=sys.stderr)
            sys.exit(1)

        if definition is not None:
            lines.append(definition)

    lines.append("")

    with open(args.output, "w") as f:
        f.write("\n".join(lines))


if __name__ == "__main__":
    main()
