#!/usr/bin/env python3
"""Sync menu manifests and character stats from src/ to build output.

The game loads runtime data next to game.exe, e.g.
  build/bin/Debug/menus/manifest.json
  build/bin/Debug/character/character_stats.json

Use this after editing files under src/menus/ or src/character/ to avoid a full rebuild.

Examples:
  python tools/sync_menu_manifests.py
  python tools/sync_menu_manifests.py --with-assets
  python tools/sync_menu_manifests.py --with-character-data --config Debug
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
SRC_CHARACTER = REPO_ROOT / "src" / "character"
CHARACTER_STATS_NAME = "character_stats.json"
BUILD_BIN = REPO_ROOT / "build" / "bin"

# Shared HUD asset folders under menus/ (character bar, minimap, paperdoll, etc.).
SHARED_ASSET_DIRS = ("char_screen", "map", "paperdoll", "bag", "hud", "close")


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


def file_sha256_prefix(path: Path, length: int = 12) -> str:
    import hashlib

    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    return digest[:length]


def copy_file(src: Path, dest: Path, dry_run: bool) -> None:
    if dry_run:
        print(f"  copy {src.relative_to(REPO_ROOT)} -> {dest.relative_to(REPO_ROOT)}")
        return
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dest)
    src_hash = file_sha256_prefix(src)
    dest_hash = file_sha256_prefix(dest)
    if src_hash != dest_hash:
        print(
            f"  ERROR: hash mismatch after copy {src.name} ({src_hash} != {dest_hash})",
            file=sys.stderr,
        )
    else:
        print(f"  ok {src.relative_to(REPO_ROOT)} -> {dest.relative_to(REPO_ROOT)} [{src_hash}]")


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


def sync_character_stats(src_character: Path, dest_character: Path, dry_run: bool) -> bool:
    src = src_character / CHARACTER_STATS_NAME
    if not src.is_file():
        print(f"Character stats not found: {src}", file=sys.stderr)
        return False
    copy_file(src, dest_character / CHARACTER_STATS_NAME, dry_run)
    return True


def sync_character_data(src_character: Path, dest_character: Path, dry_run: bool) -> bool:
    if not src_character.is_dir():
        print(f"Character source folder not found: {src_character}", file=sys.stderr)
        return False
    copy_tree(src_character, dest_character, dry_run)
    return True


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Copy menu manifests and character stats from src/ to build/bin/<Config>/."
        )
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
        "--with-character-data",
        action="store_true",
        help="Sync the entire src/character tree (items, loadouts, stats, etc.)",
    )
    parser.add_argument(
        "--skip-character-stats",
        action="store_true",
        help="Do not sync character/character_stats.json",
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

    has_menus = SRC_MENUS.is_dir()
    has_character = SRC_CHARACTER.is_dir()
    if not has_menus and not has_character:
        print(
            f"Neither {SRC_MENUS} nor {SRC_CHARACTER} found.",
            file=sys.stderr,
        )
        return 1

    build_dirs = resolve_build_dirs(args.config)
    if not build_dirs:
        print(
            f"No build output found under {BUILD_BIN}. "
            "Build the game once or pass --config Debug|Release.",
            file=sys.stderr,
        )
        return 1

    if has_menus and len(find_manifests(SRC_MENUS)) == 0 and not has_character:
        print(f"No manifest.json files under {SRC_MENUS}", file=sys.stderr)
        return 1

    for build_dir in build_dirs:
        print(f"Syncing to {build_dir.relative_to(REPO_ROOT)}/")

        if has_menus:
            dest_menus = build_dir / "menus"
            if args.all_menus:
                sync_all_menus(SRC_MENUS, dest_menus, args.dry_run)
                print("  entire src/menus tree")
            else:
                copied = sync_manifests(SRC_MENUS, dest_menus, args.dry_run)
                print(f"  {copied} menu manifest(s)")

                if args.with_assets:
                    asset_dirs = sync_shared_assets(SRC_MENUS, dest_menus, args.dry_run)
                    print(f"  {asset_dirs} shared menu asset folder(s)")

        if has_character:
            dest_character = build_dir / "character"
            if args.with_character_data:
                if sync_character_data(SRC_CHARACTER, dest_character, args.dry_run):
                    print("  entire src/character tree")
            elif not args.skip_character_stats:
                if sync_character_stats(SRC_CHARACTER, dest_character, args.dry_run):
                    print(f"  {CHARACTER_STATS_NAME}")

    if args.dry_run:
        print("Dry run — no files written.")
    else:
        print(
            "Done. Sync copies src/menus -> build/bin/<Config>/menus "
            "(it does not edit src/). Save the file in the editor first, then sync."
        )
        print(
            "Character HUD layout hot-reloads after sync; other menus still need a game restart."
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
