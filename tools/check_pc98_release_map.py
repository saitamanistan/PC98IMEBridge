#!/usr/bin/env python3
"""Reject a release PC-98 map that contains the optional debug serial code."""

from pathlib import Path
import sys


FORBIDDEN_MARKERS = ("debug_serial_pc98.c", "pc98_debug_")


def find_forbidden_markers(text: str) -> list[str]:
    return [marker for marker in FORBIDDEN_MARKERS if marker in text]


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: check_pc98_release_map.py MAP", file=sys.stderr)
        return 2
    text = Path(sys.argv[1]).read_text(encoding="utf-8", errors="replace")
    found = find_forbidden_markers(text)
    if found:
        print(
            "release map contains PC-9861K debug serial code: "
            + ", ".join(found),
            file=sys.stderr,
        )
        return 1
    print("PC-98 release map verified: COM1 transport only")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
