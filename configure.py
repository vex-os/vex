#!/usr/bin/env python3

from datetime import datetime
import os
import sys

ROOT_DIR = os.path.dirname(os.path.abspath(__file__))
CONF_DIR = os.path.join(ROOT_DIR, 'config')

# Current state of parsed config
cur_arch = None
cur_envt = None
cur_sysname = None
cur_release = None
cur_options = {}
cur_drivers = set()
cur_fs = set()

# Current state of parsed file listings
cur_filelist = set()

def raise_error(path, lineno, message):
    relative = os.path.relpath(path, ROOT_DIR)
    if lineno > 0:
        print(f"error: {relative}:{lineno}: {message}", file=sys.stderr)
    else:
        print(f"error: {relative}: {message}", file=sys.stderr)
    sys.exit(1)

def raise_warning(path, lineno, message):
    relative = os.path.relpath(path, ROOT_DIR)
    if lineno > 0:
        print(f"warning: {relative}:{lineno}: {message}", file=sys.stderr)
    else:
        print(f"warning: {relative}: {message}", file=sys.stderr)

def parse_config(path, _seen=None):
    global cur_arch
    global cur_envt
    global cur_sysname
    global cur_release
    global cur_options
    global cur_drivers
    global cur_fs

    if _seen is None:
        _seen = set()

    path = os.path.abspath(path)

    if path in _seen:
        raise_warning(path, 0, "already included, skipping")
        return

    _seen.add(path)

    if not os.path.isfile(path):
        raise_error(path, 0, "file not found")

    with open(path, 'r') as f:
        for lineno, line in enumerate(f, start=1):
            line = line.split('#', 1)[0].strip()
            line = line.split(';', 1)[0].strip() 

            if not line:
                continue
                
            parts = line.split()
            kword = parts[0]
            args = parts[1:]

            if kword == "include":
                if not args:
                    raise_error(path, lineno, f"missing arguments for {kword} keyword")
                include_name = args[0]
                include_path = os.path.join(CONF_DIR, f"config.{include_name}.conf")
                parse_config(include_path, _seen)
            elif kword == "arch" or kword == "machine":
                if not args:
                    raise_error(path, lineno, f"missing arguments for {kword} keyword")
                cur_arch = args[0]
            elif kword == "envt" or kword == "toolchain":
                if not args:
                    raise_error(path, lineno, f"missing arguments for {kword} keyword")
                cur_envt = args[0]
            elif kword == "sysname":
                if not args:
                    raise_error(path, lineno, f"missing arguments for {kword} keyword")
                cur_sysname = args[0]
            elif kword == "release":
                if not args:
                    raise_error(path, lineno, f"missing arguments for {kword} keyword")
                cur_release = args[0]
            elif kword == "option":
                if not args:
                    raise_error(path, lineno, f"missing arguments for {kword} keyword")
                name, _, value = args[0].partition('=')
                if not name:
                    raise_error(path, lineno, f"invalid option format, expected name=value")
                cur_options[name] = value if value else True
            elif kword == "nooption":
                if not args:
                    raise_error(path, lineno, f"missing arguments for {kword} keyword")
                cur_options.pop(args[0], None)
            elif kword == "driver":
                if not args:
                    raise_error(path, lineno, f"missing arguments for {kword} keyword")
                cur_drivers.add(args[0])
            elif kword == "nodriver":
                if not args:
                    raise_error(path, lineno, f"missing arguments for {kword} keyword")
                cur_drivers.discard(args[0])
            elif kword == "filesys":
                if not args:
                    raise_error(path, lineno, f"missing arguments for {kword} keyword")
                cur_fs.add(args[0])
            elif kword == "nofilesys":
                if not args:
                    raise_error(path, lineno, f"missing arguments for {kword} keyword")
                cur_fs.discard(args[0])
            else:
                raise_error(path, lineno, f"unknown keyword: {kword}")

def parse_filelist(path):
    global cur_filelist

    if not os.path.isfile(path):
        raise_warning(path, 0, "file not found, skipping")
        return

    with open(path, 'r') as f:
        for lineno, line in enumerate(f, start=1):
            line = line.split('#', 1)[0].strip()
            if not line:
                continue

            parts = line.split()
            source = parts[0]

            if len(parts) == 1:
                cur_filelist.add(source)
                continue
            
            if parts[1] != "optional":
                raise_warning(path, lineno, f"unknown filelist option: {parts[1]}, ignoring")
                continue

            conditions = parts[2:]

            if not conditions:
                raise_warning(path, lineno, f"optional file missing conditions")
                cur_filelist.add(source)
                continue

            if all(c in cur_options or c in cur_drivers or c in cur_fs or c == cur_arch for c in conditions):
                cur_filelist.add(source)

def main():
    config_name = sys.argv[1] if len(sys.argv) > 1 else "default"
    config_path = os.path.join(CONF_DIR, f"config.{config_name}.conf")

    parse_config(config_path)

    parse_filelist(os.path.join(CONF_DIR, f"files.arch.{cur_arch}.lst"))
    parse_filelist(os.path.join(CONF_DIR, f"files.generic.lst"))
    parse_filelist(os.path.join(CONF_DIR, f"files.vendor.lst"))

    config_path = os.path.join(ROOT_DIR, ".configuration.mk")
    config_date = datetime.now().strftime("%Y-%m-%d")

    assert cur_arch is not None, "ARCH not set in configuration"
    assert cur_envt is not None, "ENVT not set in configuration"

    with open(config_path, 'w') as f:
        f.write(f"# Generated at {config_date}\n")
        f.write(f"# WARNING: changes will be lost\n")
        f.write(f"ARCH := {cur_arch}\n")
        f.write(f"ENVT := {cur_envt}\n")
        if cur_sysname:
            f.write(f"SYSNAME := {cur_sysname}\n")
        if cur_release:
            f.write(f"RELEASE := {cur_release}\n")
        for name, value in cur_options.items():
            if value is True:
                f.write(f"override CFLAGS += -D{name}\n")
            else:
                f.write(f"override CFLAGS += -D{name}={value}\n")
        f.write(f"SOURCES :=\n")
        for source in sorted(cur_filelist):
            f.write(f"SOURCES += {source}\n")

if __name__ == "__main__":
    main()
