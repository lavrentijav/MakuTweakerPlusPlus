"""Pack loc/, assets/, previewimg/ into a single binary blob for onefile builds."""
from __future__ import annotations

import struct
import sys
from pathlib import Path

MAGIC = b"MAKU\x01"


def collect(root: Path) -> list[tuple[str, Path]]:
    out: list[tuple[str, Path]] = []
    for folder in ("loc", "assets", "previewimg"):
        base = root / folder
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if path.is_file():
                rel = path.relative_to(root).as_posix()
                out.append((rel, path))
    return out


def pack(root: Path, dest: Path) -> None:
    entries = collect(root)
    if not entries:
        raise SystemExit(f"no payload files under {root}")

    parts = [MAGIC, struct.pack("<I", len(entries))]
    for rel, path in entries:
        data = path.read_bytes()
        rel_b = rel.encode("utf-8")
        parts.append(struct.pack("<I", len(rel_b)))
        parts.append(rel_b)
        parts.append(struct.pack("<I", len(data)))
        parts.append(data)

    dest.write_bytes(b"".join(parts))
    print(f"packed {len(entries)} files -> {dest} ({dest.stat().st_size} bytes)")


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: pack_embedded.py <repo_root> <output.bin>")
    pack(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())


if __name__ == "__main__":
    main()
