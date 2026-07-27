#!/usr/bin/env python3
"""Sync menu manifests (and optional assets) from src/ to build output.

The game loads manifests at runtime from next to game.exe, e.g.
  build/bin/Debug/menus/manifest.json

Use this after editing files under src/menus/ to avoid a full rebuild.

Examples:
  python tools/sync_menu_manifests.py
  python tools/sync_menu_manifests.py --with-assets
  python tools/sync_menu_manifests.py --all-menus --config Debug
  python tools/sync_menu_manifests.py -n
"""

from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SRC_MENUS = REPO_ROOT / "src" / "menus"
BUILD_BIN = REPO_ROOT / "build" / "bin"

# Shared HUD asset folders under menus/ (character bar, minimap, paperdoll, etc.).
SHARED_ASSET_DIRS = ("char_screen", "map", "paperdoll", "bag")


def find_manifests(src_menus: Path) -> list[Path]:
    if not src_menus.is_dir():
        return []
    return sorted(src_menus.rglob("manifest.json"))


def resolve_build_dirs(config: str) -> list[Path]:
    if config == "all":
        candidates = [BUILD_BIN / "Debug", BUILD_BIN / "Release"]
    else:
        candidates = [BUILD_BIN / config]
    return [path for path in candidates if path.is_dir()]


def copy_file(src: Path, dest: Path, dry_run: bool) -> None:
    if dry_run:
        print(f"  copy {src.relative_to(REPO_ROOT)} -> {dest.relative_to(REPO_ROOT)}")
        return
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dest)


def copy_tree(src: Path, dest: Path, dry_run: bool) -> None:
    if dry_run:
        print(f"  tree {src.relative_to(REPO_ROOT)}/ -> {dest.relative_to(REPO_ROOT)}/")
        return
    if dest.exists():
        shutil.copytree(src, dest, dirs_exist_ok=True)
    else:
        shutil.copytree(src, dest)


def sync_manifests(src_menus: Path, dest_menus: Path, dry_run: bool) -> int:
    manifests = find_manifests(src_menus)
    if not manifests:
        print(f"No manifests found under {src_menus}", file=sys.stderr)
        return 0

    for manifest in manifests:
        rel = manifest.relative_to(src_menus)
        copy_file(manifest, dest_menus / rel, dry_run)
    return len(manifests)


def sync_shared_assets(src_menus: Path, dest_menus: Path, dry_run: bool) -> int:
    copied = 0
    for name in SHARED_ASSET_DIRS:
        src = src_menus / name
        if not src.is_dir():
            continue
        copy_tree(src, dest_menus / name, dry_run)
        copied += 1
    return copied


def sync_all_menus(src_menus: Path, dest_menus: Path, dry_run: bool) -> None:
    copy_tree(src_menus, dest_menus, dry_run)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Copy menu manifests from src/menus to build/bin/<Config>/menus."
    )
    parser.add_argument(
        "--config",
        choices=("Debug", "Release", "all"),
        default="all",
        help="Build configuration folder under build/bin/ (default: all existing)",
    )
    parser.add_argument(
        "--with-assets",
        action="store_true",
        help="Also sync shared asset folders referenced at menus/ root (e.g. char_screen/)",
    )
    parser.add_argument(
        "--all-menus",
        action="store_true",
        help="Sync the entire src/menus tree (same as the Game post-build copy step)",
    )
    parser.add_argument(
        "-n",
        "--dry-run",
        action="store_true",
        help="Print what would be copied without writing files",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if not SRC_MENUS.is_dir():
        print(f"Source menus folder not found: {SRC_MENUS}", file=sys.stderr)
        return 1

    build_dirs = resolve_build_dirs(args.config)
    if not build_dirs:
        print(
            f"No build output found under {BUILD_BIN}. "
            "Build the game once or pass --config Debug|Release.",
            file=sys.stderr,
        )
        return 1

    manifest_count = len(find_manifests(SRC_MENUS))
    if manifest_count == 0:
        print(f"No manifest.json files under {SRC_MENUS}", file=sys.stderr)
        return 1

    for build_dir in build_dirs:
        dest_menus = build_dir / "menus"
        print(f"Syncing to {dest_menus.relative_to(REPO_ROOT)}/")

        if args.all_menus:
            sync_all_menus(SRC_MENUS, dest_menus, args.dry_run)
            print("  entire src/menus tree")
        else:
            copied = sync_manifests(SRC_MENUS, dest_menus, args.dry_run)
            print(f"  {copied} manifest(s)")

            if args.with_assets:
                asset_dirs = sync_shared_assets(SRC_MENUS, dest_menus, args.dry_run)
                print(f"  {asset_dirs} shared asset folder(s)")

    if args.dry_run:
        print("Dry run — no files written.")
    else:
        print("Done. Restart game.exe to pick up changes.")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
