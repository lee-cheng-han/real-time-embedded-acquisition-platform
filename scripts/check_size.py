#!/usr/bin/env python3
"""Enforce DRATS linker budgets and emit a machine-readable size report."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import subprocess

FLASH_BUDGET = 768 * 1024
RAM_BUDGET = 128 * 1024
FLASH_SECTIONS = {".isr_vector", ".text", ".ARM.extab", ".ARM.exidx",
                  ".preinit_array", ".init_array", ".fini_array", ".data"}
RAM_SECTIONS = {".data", ".bss", ".noinit", ".heap_stack"}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    output = subprocess.run(
        ["arm-none-eabi-size", "-A", str(args.elf)],
        check=True, text=True, stdout=subprocess.PIPE
    ).stdout
    sections: dict[str, int] = {}
    for line in output.splitlines():
        fields = line.split()
        if len(fields) >= 2 and fields[0].startswith("."):
            try:
                sections[fields[0]] = int(fields[1])
            except ValueError:
                pass
    flash = sum(size for name, size in sections.items() if name in FLASH_SECTIONS)
    ram = sum(size for name, size in sections.items() if name in RAM_SECTIONS)
    report = {
        "elf": str(args.elf),
        "flash": {"used_bytes": flash, "budget_bytes": FLASH_BUDGET,
                  "percent": round(100 * flash / FLASH_BUDGET, 3)},
        "ram": {"used_bytes": ram, "budget_bytes": RAM_BUDGET,
                "percent": round(100 * ram / RAM_BUDGET, 3)},
        "pass": flash <= FLASH_BUDGET and ram <= RAM_BUDGET,
        "sections": sections,
    }
    encoded = json.dumps(report, indent=2, sort_keys=True)
    print(encoded)
    if args.output:
        args.output.write_text(encoded + "\n", encoding="utf-8")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
