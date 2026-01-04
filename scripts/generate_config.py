#!/usr/bin/env python3

import configparser
import sys
import os

from datetime import datetime, timezone
from pathlib import Path

def gather_configs(root_config):
    configs = []
    processed = set()

    def process_config(config):
        if config in processed:
            return
        processed.add(config)

        parser = configparser.ConfigParser()
        parser.optionxform = str # preserve case
        parser.read(config)

        for section in parser.sections():
            if section.startswith("base:"):
                base_file = section.split(":", 1)[1].strip()
                base_path = (Path(config).parent / (base_file + ".conf")).resolve()
                process_config(str(base_path))

        configs.append((config, parser))

    process_config(root_config)

    return configs

def aggregate_configs(configs, arch):
    merged = configparser.ConfigParser()
    merged.optionxform = str # preserve case

    for config, parser in configs:
        for section in parser.sections():
            if not merged.has_section(section):
                merged.add_section(section)

            for key, value in parser.items(section):
                merged.set(section, key, value)

    sections_to_remove = []
    sections_to_rename = {}

    for section in merged.sections():
        if '@' in section:
            base_section, section_arch = section.rsplit('@', 1)

            if section_arch == arch:
                sections_to_rename[section] = base_section
            else:
                sections_to_remove.append(section)

    for section in sections_to_remove:
        merged.remove_section(section)
    for old_section, new_section in sections_to_rename.items():
        if not merged.has_section(new_section):
            merged.add_section(new_section)
        for key, value in merged.items(old_section):
            merged.set(new_section, key, value)
        merged.remove_section(old_section)

    return merged

def sanitize_value(value):
    if value.lower() in ("true", "false"):
        return "1" if value.lower() == "true" else "0"
    if value.lower() in ("yes", "no"):
        return "1" if value.lower() == "yes" else "0"
    return value

def generate_cppflags(parser):
    cppflags = []

    for section in parser.sections():
        if section == "config":
            for key, value in parser.items(section):
                key_upper = key.upper()
                value_cpp = sanitize_value(value)
                cppflags.append(f"-D CONF_{key_upper}={value_cpp}")
            continue
    
        if section.startswith("device:"):
            device_name = section.split(":", 1)[1].strip().upper()
            device_name_upper = device_name.upper()
            for key, value in parser.items(section):
                key_upper = key.upper()
                value_cpp = sanitize_value(value)
                cppflags.append(f"-D CONF_DEV_{device_name_upper}_{key_upper}={value_cpp}")
            continue
    
        if section.startswith("filesys:"):
            fs_name = section.split(":", 1)[1].strip().upper()
            fs_name_upper = fs_name.upper()
            for key, value in parser.items(section):
                key_upper = key.upper()
                value_cpp = sanitize_value(value)
                cppflags.append(f"-D CONF_FS_{fs_name_upper}_{key_upper}={value_cpp}")
            continue
    
    for i in range(len(cppflags)):
        cppflags[i] = f"override CPPFLAGS += {cppflags[i]}"
    return "\n".join(cppflags)

def generate_make_macros(parser):
    makeconsts = []

    for section in parser.sections():
        if section == "config":
            for key, value in parser.items(section):
                key_upper = key.upper()
                value_mk = sanitize_value(value)
                makeconsts.append(f"CONF_{key_upper} = {value_mk}")
            continue
    
        if section.startswith("device:"):
            device_name = section.split(":", 1)[1].strip().upper()
            device_name_upper = device_name.upper()
            if parser.getboolean(section, "enable", fallback=False):
                makeconsts.append(f"DEV_{device_name_upper} = 1")
            else:
                makeconsts.append(f"DEV_{device_name_upper} = 0")
            continue
    
        if section.startswith("filesys:"):
            fs_name = section.split(":", 1)[1].strip().upper()
            fs_name_upper = fs_name.upper()
            if parser.getboolean(section, "enable", fallback=False):
                makeconsts.append(f"FS_{fs_name_upper} = 1")
            else:
                makeconsts.append(f"FS_{fs_name_upper} = 0")
            continue
    
    return "\n".join(makeconsts)

def print_filesystems(parser):
    ansi_green = "\033[1;32;92m"
    ansi_yellow = "\033[1;33;93m"
    ansi_reset = "\033[0m"

    for section in parser.sections():
        if section.startswith("filesys:"):
            fs_name = section.split(":", 1)[1].strip()
            enabled = parser.getboolean(section, "enable", fallback=False)
            if enabled:
                fs_text = f"{ansi_green}ENABLED{ansi_reset}"
            else:
                fs_text = f"{ansi_yellow}DISABLED{ansi_reset}"
            print(f"filesys: {fs_name}: {fs_text}")

def generate(input, output, arch):
    configs = gather_configs(input)
    parser = aggregate_configs(configs, arch)

    cppflags = generate_cppflags(parser)
    makeconsts = generate_make_macros(parser)

    with open(output, "w") as f:
        now_utc = datetime.now(timezone.utc)
        timestamp = now_utc.strftime("%Y-%m-%d %H:%M:%S %z")
        f.write("# generated at {}\n".format(timestamp))
        f.write("# warning: changes will be lost\n")
        f.write("TIMESTAMP := {}\n".format(timestamp))
        f.write(cppflags)
        f.write("\n")
        f.write(makeconsts)
        f.write("\n")

if __name__ == "__main__":
    if len(sys.argv) < 4:
        print(f"usage: {sys.argv[0]} <input_config> <output_file> <arch>")
        sys.exit(1)

    input_config = sys.argv[1]
    output_file = sys.argv[2]
    arch = sys.argv[3]

    generate(input_config, output_file, arch)
