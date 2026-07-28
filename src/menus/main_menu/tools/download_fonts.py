#!/usr/bin/env python3
"""Download bundled menu / HUD fonts from Google Fonts (GitHub)."""

from __future__ import annotations

import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FONTS = {
    "Inter-Regular.ttf": "https://github.com/google/fonts/raw/main/ofl/inter/static/Inter_24pt-Regular.ttf",
    "Inter-Bold.ttf": "https://github.com/google/fonts/raw/main/ofl/inter/static/Inter_24pt-Bold.ttf",
    "JetBrainsMono-Regular.ttf": "https://github.com/google/fonts/raw/main/ofl/jetbrainsmono/static/JetBrainsMono-Regular.ttf",
}


def main() -> None:
    out_dir = ROOT / "fonts"
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, url in FONTS.items():
        dest = out_dir / name
        print(f"Downloading {name}...")
        urllib.request.urlretrieve(url, dest)
        print(f"  -> {dest} ({dest.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
