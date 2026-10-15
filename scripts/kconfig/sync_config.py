#!/usr/bin/env python3
"""
Configure-time helper for Meson: makes sure KCONFIG_CONFIG exists and is
up to date with the Kconfig tree (seeding it from a defconfig if it's not
there yet), then prints every parsed Kconfig file, one per line, so that
the build system can reconfigure when any of them changes.
"""

import argparse
import os
import sys

import kconfiglib


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kconfig", required=True, help="Top-level Kconfig file")
    parser.add_argument("--defconfig", required=True, help="Configuration to use when KCONFIG_CONFIG doesn't exist")
    args = parser.parse_args()

    config = kconfiglib.standard_config_filename()
    kconf = kconfiglib.Kconfig(args.kconfig, warn_to_stderr=False, suppress_traceback=True)

    if os.path.exists(config):
        kconf.load_config(config)
    else:
        kconf.load_config(args.defconfig)

    for warning in kconf.warnings:
        print(warning, file=sys.stderr)

    # Doesn't touch the file if nothing changed, so
    # it won't cause Meson to reconfigure in a loop
    print(kconf.write_config(config), file=sys.stderr)

    for filename in kconf.kconfig_filenames:
        print(os.path.abspath(os.path.join(kconf.srctree or "", filename)))


if __name__ == "__main__":
    main()
