#!/usr/bin/env python3
"""Write checksums for installers in one version directory."""
import hashlib
from pathlib import Path
import sys

root = Path(sys.argv[1])
files = sorted(p for p in root.iterdir() if p.is_file() and p.suffix in {".dmg", ".exe", ".zip"})
lines = []
for path in files:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    lines.append(digest.hexdigest() + "  " + path.name + "\n")
(root / "SHA256SUMS.txt").write_text("".join(lines), encoding="utf-8")
print("Updated", root / "SHA256SUMS.txt", "with", len(files), "packages")
