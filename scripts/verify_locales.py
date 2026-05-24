#!/usr/bin/env python3
"""Verify locale files against en.json: key parity and untranslated strings."""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

LOCALE_FILE = re.compile(r"^[a-z]{2}\.json$")

ALLOWED_EXACT = {
    "Windows",
    "UWP",
    "GitHub",
    "NUMA",
    "SQLite",
    "MakuTweaker++",
}

ALLOWED_SUBSTRINGS = (
    "Windows",
    "UWP",
    "GitHub",
    "NUMA",
    "SQLite",
    "MakuTweaker++",
)


def flatten_strings(node: dict, prefix: str = "") -> dict[str, str]:
    out: dict[str, str] = {}
    for key, value in node.items():
        path = f"{prefix}.{key}" if prefix else key
        if isinstance(value, dict):
            out.update(flatten_strings(value, path))
        elif isinstance(value, str):
            out[path] = value
        else:
            raise TypeError(f"Non-string leaf at {path}: {type(value).__name__}")
    return out


def leaf_key(path: str) -> str:
    return path.rsplit(".", 1)[-1]


def is_number(value: str) -> bool:
    try:
        float(value)
        return True
    except ValueError:
        return False


def is_allowed_untranslated(path: str, value: str) -> bool:
    if len(leaf_key(path)) == 1:
        return True
    if is_number(value):
        return True
    if value in ALLOWED_EXACT:
        return True
    if any(term in value for term in ALLOWED_SUBSTRINGS):
        return True
    return False


def compare_structure(en: dict, loc: dict, prefix: str = "") -> tuple[list[str], list[str]]:
    missing: list[str] = []
    extra: list[str] = []
    for key in en:
        path = f"{prefix}.{key}" if prefix else key
        if key not in loc:
            missing.append(path)
        elif isinstance(en[key], dict):
            if not isinstance(loc.get(key), dict):
                missing.append(path)
            else:
                sub_missing, sub_extra = compare_structure(en[key], loc[key], path)
                missing.extend(sub_missing)
                extra.extend(sub_extra)
    for key in loc:
        path = f"{prefix}.{key}" if prefix else key
        if key not in en:
            extra.append(path)
    return missing, extra


def untranslated_strings(en_flat: dict[str, str], loc_flat: dict[str, str]) -> list[str]:
    hits: list[str] = []
    for path, en_val in en_flat.items():
        loc_val = loc_flat.get(path)
        if loc_val is None:
            continue
        if loc_val == en_val and not is_allowed_untranslated(path, en_val):
            hits.append(path)
    return hits


def main() -> int:
    root = Path(__file__).resolve().parent.parent / "loc"
    en_path = root / "en.json"
    with open(en_path, encoding="utf-8") as f:
        en_root = json.load(f)
    en_cats = en_root["categories"]
    en_flat = flatten_strings(en_cats)

    print(f"Reference: {en_path.name} ({len(en_flat)} strings)\n")

    structure_errors = 0
    rows: list[tuple[str, int, int, int]] = []

    for fp in sorted(root.glob("*.json")):
        if fp.name == "en.json" or not LOCALE_FILE.match(fp.name):
            continue

        with open(fp, encoding="utf-8") as f:
            data = json.load(f)
        loc_cats = data.get("categories")
        if not isinstance(loc_cats, dict):
            print(f"{fp.name}: missing or invalid 'categories' object")
            structure_errors += 1
            continue

        missing, extra = compare_structure(en_cats, loc_cats)
        loc_flat = flatten_strings(loc_cats)
        untranslated = untranslated_strings(en_flat, loc_flat)

        if missing or extra:
            structure_errors += 1
            print(f"{fp.name}: STRUCTURE MISMATCH")
            for path in missing[:10]:
                print(f"  missing: {path}")
            if len(missing) > 10:
                print(f"  ... and {len(missing) - 10} more missing")
            for path in extra[:10]:
                print(f"  extra: {path}")
            if len(extra) > 10:
                print(f"  ... and {len(extra) - 10} more extra")
            print()

        rows.append((fp.name, len(missing), len(extra), len(untranslated)))

    print(f"{'File':<12} {'Missing':>8} {'Extra':>8} {'Untranslated':>14}")
    print("-" * 46)
    for name, missing_n, extra_n, untrans_n in rows:
        print(f"{name:<12} {missing_n:>8} {extra_n:>8} {untrans_n:>14}")

    total_untrans = sum(r[3] for r in rows)
    print("-" * 46)
    print(f"{'TOTAL':<12} {'':>8} {'':>8} {total_untrans:>14}")

    if structure_errors:
        print(f"\n{structure_errors} file(s) with structure errors.")
        return 1

    worst = max(rows, key=lambda r: r[3], default=None)
    if worst and worst[3] > 0:
        print(f"\nMost untranslated: {worst[0]} ({worst[3]} strings)")
        with open(root / worst[0], encoding="utf-8") as f:
            loc_flat = flatten_strings(json.load(f)["categories"])
        sample = untranslated_strings(en_flat, loc_flat)[:5]
        if sample:
            print("Sample paths:")
            for path in sample:
                print(f"  {path}: {en_flat[path]!r}")

    return 0 if structure_errors == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
