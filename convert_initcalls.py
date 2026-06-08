#!/usr/bin/env python3
"""Convert old define_initcall() syntax to new initcall_*() syntax."""

import re
import sys
from pathlib import Path

STAGE_MAP = {
    "INITCALL_STAGE_BSP_EARLY":     "initcall_bsp_early",
    "INTICALL_STAGE_BSP_ARCH":      "initcall_bsp_arch",   # typo in picstub.c
    "INITCALL_STAGE_BSP_ARCH":      "initcall_bsp_arch",
    "INITCALL_STAGE_BSP_BOOTINFO":  "initcall_bsp_bootinfo",
    "INITCALL_STAGE_BSP_PMM":       "initcall_bsp_pmm",
    "INITCALL_STAGE_BSP_SLAB":      "initcall_bsp_slab",
    "INITCALL_STAGE_BSP_VMM":       "initcall_bsp_vmm",
    "INITCALL_STAGE_BSP_HEAP":      "initcall_bsp_heap",
    "INITCALL_STAGE_BSP_IREQ":      "initcall_bsp_ireq",
    "INITCALL_STAGE_BSP_TIMER":     "initcall_bsp_timer",
    "INITCALL_STAGE_BSP_MULTICORE": "initcall_bsp_multicore",
    "INITCALL_STAGE_BSP_BUS":       "initcall_bsp_bus",
    "INITCALL_STAGE_BSP_VFS":       "initcall_bsp_vfs",
    "INITCALL_STAGE_BSP_DEV":       "initcall_bsp_dev",
    "INITCALL_STAGE_BSP_FS":        "initcall_bsp_fs",
    "INITCALL_STAGE_BSP_CPUDEV":    "initcall_bsp_cpudev",
    "INITCALL_STAGE_BSP_SERVICES":  "initcall_bsp_services",
    "INITCALL_STAGE_BSP_LATE":      "initcall_bsp_late",
    "INITCALL_STAGE_SMP_HART":      "initcall_smp_hart",
    "INITCALL_STAGE_SMP_ARCH":      "initcall_smp_arch",
    "INITCALL_STAGE_SMP_IREQ":      "initcall_smp_ireq",
    "INITCALL_STAGE_SMP_TIMER":     "initcall_smp_timer",
    "INITCALL_STAGE_SMP_LATE":      "initcall_smp_late",
}

HEADER_RE = re.compile(
    r"define_initcall\s*\(\s*(\w+)\s*,\s*(\w+)\s*,\s*(\w+)\s*\)"
)


def extract_block(text, start):
    """Return (block_text, end_index) where block_text includes the braces."""
    assert text[start] == '{', f"Expected '{{' at {start}, got {text[start]!r}"
    depth = 0
    i = start
    while i < len(text):
        ch = text[i]
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                return text[start:i + 1], i + 1
        i += 1
    raise ValueError("Unterminated block starting at index {}".format(start))


def convert_file(path: Path, dry_run: bool = False) -> bool:
    source = path.read_text(encoding="utf-8")
    result = []
    pos = 0
    changed = False

    for m in HEADER_RE.finditer(source):
        func_name = m.group(1)
        stage_raw = m.group(2)
        order = m.group(3)

        macro = STAGE_MAP.get(stage_raw)
        if macro is None:
            print(f"  WARNING: unknown stage '{stage_raw}' in {path}", file=sys.stderr)
            continue

        # Find the '{' that follows the macro header (skip whitespace/newlines)
        brace_pos = m.end()
        while brace_pos < len(source) and source[brace_pos] in " \t\r\n":
            brace_pos += 1

        if brace_pos >= len(source) or source[brace_pos] != '{':
            print(f"  WARNING: no block after define_initcall in {path}:{m.start()}", file=sys.stderr)
            continue

        block, block_end = extract_block(source, brace_pos)

        # Everything before this define_initcall header goes through unchanged
        result.append(source[pos:m.start()])

        # Emit: static void func_name(void) <block>\n\nmacro(func_name, order);
        result.append(f"static void {func_name}(void)\n{block}\n\n{macro}({func_name}, {order});")

        pos = block_end
        changed = True
        print(f"  {func_name}: {stage_raw} -> {macro}")

    if not changed:
        return False

    result.append(source[pos:])
    new_source = "".join(result)

    if dry_run:
        print(f"  [dry-run] would write {path}")
    else:
        path.write_text(new_source, encoding="utf-8")

    return True


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Convert define_initcall to new initcall_*() syntax")
    parser.add_argument("paths", nargs="+", help="C source files or directories to process")
    parser.add_argument("--dry-run", action="store_true", help="Show what would change without writing")
    args = parser.parse_args()

    files = []
    for p in args.paths:
        path = Path(p)
        if path.is_dir():
            files.extend(path.rglob("*.c"))
        else:
            files.append(path)

    total = 0
    for f in files:
        print(f"Processing {f}")
        if convert_file(f, dry_run=args.dry_run):
            total += 1

    print(f"\nDone. {total} file(s) modified.")


if __name__ == "__main__":
    main()
