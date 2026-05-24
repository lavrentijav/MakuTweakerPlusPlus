#!/usr/bin/env python3
"""Translate loc/en.json into cs, pl, et, lv locale files."""
from __future__ import annotations

import json
import sys
import time
from pathlib import Path

from deep_translator import GoogleTranslator

sys.path.insert(0, str(Path(__file__).resolve().parent))
from translate_loc import flatten, protect, restore, unflatten  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
LOC = ROOT / "loc"
SOURCE = LOC / "en.json"

TARGETS = {
    "cs": "cs",
    "pl": "pl",
    "et": "et",
    "lv": "lv",
}


def translate_value(text: str, translator: GoogleTranslator) -> str:
    if not text.strip():
        return text
    protected, saved = protect(text)
    for attempt in range(5):
        try:
            translated = translator.translate(protected)
            if translated is None:
                raise RuntimeError("empty translation")
            return restore(translated, saved)
        except Exception as exc:  # noqa: BLE001
            if attempt == 4:
                raise
            time.sleep(1.5 * (attempt + 1))
            print(f"  retry {attempt + 1}: {exc}", file=sys.stderr)
    return text


def translate_file(lang_code: str, flat_en: dict[str, str]) -> dict[str, str]:
    out_path = LOC / f"{lang_code}.json"
    cache_path = LOC / f".{lang_code}.cache.json"

    cached: dict[str, str] = {}
    if cache_path.exists():
        cached = json.loads(cache_path.read_text(encoding="utf-8"))

    translator = GoogleTranslator(source="en", target=TARGETS[lang_code])
    result = dict(cached)
    total = len(flat_en)

    for idx, (key, value) in enumerate(flat_en.items(), 1):
        if key in result:
            continue
        print(f"[{lang_code}] {idx}/{total} {key}", file=sys.stderr)
        result[key] = translate_value(value, translator)
        cache_path.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
        time.sleep(0.12)

    nested = unflatten(result)
    out_path.write_text(json.dumps(nested, ensure_ascii=False, indent=4) + "\n", encoding="utf-8")
    if cache_path.exists():
        cache_path.unlink()
    return result


def main() -> int:
    en = json.loads(SOURCE.read_text(encoding="utf-8"))
    flat_en = flatten(en)

    for lang in TARGETS:
        print(f"=== Translating {lang} ===", file=sys.stderr)
        translate_file(lang, flat_en)
        print(f"=== Done {lang} ===", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
