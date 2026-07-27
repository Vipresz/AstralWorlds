#!/usr/bin/env python3
"""Generate solid-color placeholder PNGs for menu asset slots."""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SLOTS: dict[str, tuple[int, int, int, int, int]] = {
  "backgrounds/panel.png": (256, 256, 18, 20, 28),
  "backgrounds/overlay_dim.png": (64, 64, 0, 0, 0),
  "buttons/normal.png": (256, 64, 40, 48, 68),
  "buttons/hover.png": (256, 64, 55, 75, 120),
  "buttons/active.png": (256, 64, 75, 110, 170),
  "buttons/disabled.png": (256, 64, 55, 58, 68),
  "frames/window_border.png": (128, 128, 60, 65, 80),
  "frames/title_bar.png": (256, 48, 30, 35, 50),
}


def write_rgba_png(path: Path, width: int, height: int, r: int, g: int, b: int, a: int = 255) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    row = bytes([r, g, b, a]) * width
    raw = b"".join(b"\x00" + row for _ in range(height))
    compressed = zlib.compress(raw, 9)

    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", ihdr)
    png += chunk(b"IDAT", compressed)
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


def main() -> None:
    for rel, (w, h, r, g, b) in SLOTS.items():
        out = ROOT / rel
        alpha = 140 if "overlay_dim" in rel else 255
        write_rgba_png(out, w, h, r, g, b, alpha)
        print(f"wrote {out.relative_to(ROOT.parent)}")


if __name__ == "__main__":
    main()
