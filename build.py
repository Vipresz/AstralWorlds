#!/usr/bin/env python3
"""
Build script for Game Engine project.
Allows configuring and building specific targets through command-line flags.
"""

import argparse
import subprocess
import sys
import os
from pathlib import Path

# Project configuration
PROJECT_ROOT = Path(__file__).parent.absolute()
BUILD_DIR = PROJECT_ROOT / "build"
CMAKE_SOURCE_DIR = PROJECT_ROOT

# Available build targets
BUILD_TARGETS = {
    "game": "BUILD_GAME",
    "skeleton": "BUILD_SKELETON_EDITOR",
    "camera": "BUILD_CAMERA_SYSTEM",
}

# Build configurations
BUILD_TYPES = ["Debug", "Release", "RelWithDebInfo", "MinSizeRel"]


def print_status(message):
    """Print a status message."""
    print(f"[BUILD] {message}")


def print_error(message):
    """Print an error message."""
    print(f"[ERROR] {message}", file=sys.stderr)


def run_command(cmd, cwd=None, check=True):
    """Run a shell command and return the result."""
    print_status(f"Running: {' '.join(cmd)}")
    try:
        result = subprocess.run(cmd, cwd=cwd, check=check, capture_output=False)
        return result.returncode == 0
    except subprocess.CalledProcessError as e:
        print_error(f"Command failed with exit code {e.returncode}")
        return False
    except FileNotFoundError:
        print_error(f"Command not found: {cmd[0]}")
        print_error("Make sure CMake is installed and in your PATH")
        return False


def configure_cmake(build_type, targets_to_build, build_dir):
    """Configure CMake with the specified options."""
    if not build_dir.exists():
        build_dir.mkdir(parents=True, exist_ok=True)
    
    # Build CMake configuration arguments
    cmake_args = [
        "cmake",
        "-S", str(CMAKE_SOURCE_DIR),
        "-B", str(build_dir),
        f"-DCMAKE_BUILD_TYPE={build_type}",
    ]
    
    # Add build target options
    for target_name, cmake_option in BUILD_TARGETS.items():
        should_build = target_name in targets_to_build
        cmake_args.append(f"-D{cmake_option}=ON" if should_build else f"-D{cmake_option}=OFF")
    
    print_status(f"Configuring CMake build (type: {build_type})...")
    print_status(f"Building targets: {', '.join(targets_to_build) if targets_to_build else 'all'}")
    
    return run_command(cmake_args, cwd=build_dir.parent)


def build_targets(build_dir, config=None, target_names=None):
    """Build the specified targets."""
    cmake_args = ["cmake", "--build", str(build_dir)]
    
    if config:
        cmake_args.extend(["--config", config])
    
    if target_names:
        # Map our target names to actual CMake target names
        target_map = {
            "game": "Game",
            "skeleton": "visual_skeleton_editor",
            "camera": "camera_system_editor",
        }
        actual_targets = [target_map.get(t, t) for t in target_names]
        cmake_args.extend(["--target"] + actual_targets)
    
    print_status("Building targets...")
    return run_command(cmake_args)


def list_targets():
    """List all available build targets."""
    print("Available build targets:")
    for target_name, description in [
        ("game", "Main game executable"),
        ("skeleton", "Skeleton editor"),
        ("camera", "Camera system editor"),
    ]:
        print(f"  {target_name:10} - {description}")


def main():
    parser = argparse.ArgumentParser(
        description="Build script for Game Engine project",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Build all targets (default)
  python build.py

  # Build only the game executable
  python build.py --targets game

  # Build skeleton and camera editors
  python build.py --targets skeleton camera

  # Build in Debug mode
  python build.py --build-type Debug

  # Configure only (don't build)
  python build.py --configure-only

  # Build specific target after configuration
  python build.py --build-only --targets game
        """
    )
    
    parser.add_argument(
        "--targets",
        nargs="+",
        choices=list(BUILD_TARGETS.keys()),
        default=list(BUILD_TARGETS.keys()),
        help="Targets to build (default: all)"
    )
    
    parser.add_argument(
        "--build-type",
        choices=BUILD_TYPES,
        default="Release",
        help="CMake build type (default: Release)"
    )
    
    parser.add_argument(
        "--build-dir",
        type=Path,
        default=BUILD_DIR,
        help=f"Build directory (default: {BUILD_DIR.relative_to(PROJECT_ROOT)})"
    )
    
    parser.add_argument(
        "--configure-only",
        action="store_true",
        help="Only configure CMake, don't build"
    )
    
    parser.add_argument(
        "--build-only",
        action="store_true",
        help="Only build, skip CMake configuration (assumes already configured)"
    )
    
    parser.add_argument(
        "--list-targets",
        action="store_true",
        help="List all available build targets and exit"
    )
    
    parser.add_argument(
        "--clean",
        action="store_true",
        help="Clean build directory before building"
    )
    
    args = parser.parse_args()
    
    # Handle list targets
    if args.list_targets:
        list_targets()
        return 0
    
    # Handle clean
    if args.clean and args.build_dir.exists():
        print_status("Cleaning build directory...")
        import shutil
        try:
            shutil.rmtree(args.build_dir)
            print_status("Build directory cleaned")
        except Exception as e:
            print_error(f"Failed to clean build directory: {e}")
            return 1
    
    # Determine build config for multi-config generators (Visual Studio)
    build_config = None
    if sys.platform == "win32":
        build_config = args.build_type
    
    # Configure CMake
    if not args.build_only:
        if not configure_cmake(args.build_type, args.targets, args.build_dir):
            return 1
    
    # Build targets
    if not args.configure_only:
        if not build_targets(args.build_dir, build_config, args.targets):
            return 1
        print_status("Build completed successfully!")
    else:
        print_status("Configuration completed successfully!")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())

