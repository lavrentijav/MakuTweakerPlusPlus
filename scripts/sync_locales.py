#!/usr/bin/env python3
"""Add missing keys from en.json into locale files without overwriting translations."""
import json
import re
from copy import deepcopy
from pathlib import Path

LOCALE_FILE = re.compile(r"^[a-z]{2}\.json$")


def merge_missing(template: dict, target: dict) -> dict:
    result = deepcopy(target)
    for key, val in template.items():
        if key not in result:
            result[key] = deepcopy(val)
        elif isinstance(val, dict) and isinstance(result.get(key), dict):
            result[key] = merge_missing(val, result[key])
    return result


def main() -> None:
    root = Path(__file__).resolve().parent.parent / "loc"
    with open(root / "en.json", encoding="utf-8") as f:
        en = json.load(f)["categories"]

    for fp in sorted(root.glob("*.json")):
        if fp.name == "en.json" or not LOCALE_FILE.match(fp.name):
            continue
        with open(fp, encoding="utf-8") as f:
            data = json.load(f)
        cats = data.setdefault("categories", {})
        merged = merge_missing(en, cats)
        data["categories"] = merged
        with open(fp, "w", encoding="utf-8", newline="\n") as f:
            json.dump(data, f, ensure_ascii=False, indent=4)
            f.write("\n")
        print(f"Synced {fp.name}")


if __name__ == "__main__":
    main()
