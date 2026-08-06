#!/usr/bin/env python3
"""Reject a PC-98 TSR whose live state is placed past its resident marker."""

from pathlib import Path
import re
import sys


REQUIRED_SYMBOLS = (
    "pc98_old_input_vector",
    "pc98_old_serial_irq_vector",
    "pc98_old_timer_vector",
    "pc98_old_idle_vector",
    "pc98_hotkey_latched",
    "pc98_serial_irq_capture",
    "pc98_serial_irq_buffer",
    "pc98_worker_stack_top",
)

REQUIRED_RESIDENT_SYMBOLS = (
    "pc98_uninstall_prepare_entry",
    "pc98_prepare_uninstall",
    "pc98_tsr_signature",
)


def symbol_address(text: str, symbol: str) -> int:
    match = re.search(
        rf"^\s*(0x[0-9a-fA-F]+)\s+{re.escape(symbol)}\s*$",
        text,
        re.MULTILINE,
    )
    if not match:
        raise ValueError(f"missing linker symbol: {symbol}")
    return int(match.group(1), 16)


def validate_map(text: str) -> list[str]:
    resident_start = symbol_address(text, "pc98_tsr_bss_start")
    resident_end = symbol_address(text, "pc98_tsr_resident_end")
    outside = []
    for symbol in REQUIRED_SYMBOLS:
        address = symbol_address(text, symbol)
        if address < resident_start or address >= resident_end:
            outside.append(f"{symbol}=0x{address:04X}")
    for symbol in REQUIRED_RESIDENT_SYMBOLS:
        address = symbol_address(text, symbol)
        if address >= resident_end:
            outside.append(f"{symbol}=0x{address:04X}")
    return outside


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_pc98_tsr_map.py MAP", file=sys.stderr)
        return 2
    text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
    resident_start = symbol_address(text, "pc98_tsr_bss_start")
    resident_end = symbol_address(text, "pc98_tsr_resident_end")
    outside = validate_map(text)
    if outside:
        print(
            "TSR state lies outside resident memory: " + ", ".join(outside),
            file=sys.stderr,
        )
        return 1
    print(
        "PC-98 TSR resident map verified: "
        f"bss=0x{resident_start:04X}..0x{resident_end:04X}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
