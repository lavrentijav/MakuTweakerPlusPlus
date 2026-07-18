#!/usr/bin/env python3
"""Increment BuildNumber.txt (compilation counter, last segment of app version)."""
from __future__ import annotations

import argparse
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(description="Bump MakuTweaker build counter")
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="Repository root",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print next value without writing",
    )
    args = parser.parse_args()

    path = args.root / "BuildNumber.txt"
    current = 0
    if path.is_file():
        raw = path.read_text(encoding="utf-8").strip()
        if raw:
            try:
                current = int(raw)
            except ValueError:
                print(f"Invalid BuildNumber.txt: {raw!r}", file=sys.stderr)
                return 1

    nxt = current + 1
    if args.dry_run:
        print(nxt)
        return 0

    path.write_text(f"{nxt}\n", encoding="utf-8")
    print(f"BuildNumber={nxt}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
