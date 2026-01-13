#!/usr/bin/env python3
"""
Build script for Game Engine project.
Allows configuring and building specific targets through command-line flags.
Supports both Windows (game_libs) and macOS (Homebrew) dependencies.
"""

import argparse
import subprocess
import sys
import os
import shutil
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

# macOS Homebrew dependencies
MACOS_DEPENDENCIES = {
    "glm": "GLM (OpenGL Mathematics)",
    "glew": "GLEW (OpenGL Extension Wrangler)",
    "glfw": "GLFW (windowing library)",
    "assimp": "Assimp (model loading)",
}


def print_status(message):
    """Print a status message."""
    print(f"[BUILD] {message}")


def print_warning(message):
    """Print a warning message."""
    print(f"[WARN] {message}", file=sys.stderr)


def print_error(message):
    """Print an error message."""
    print(f"[ERROR] {message}", file=sys.stderr)


def run_command(cmd, cwd=None, check=True, capture=False):
    """Run a shell command and return the result."""
    print_status(f"Running: {' '.join(cmd)}")
    try:
        result = subprocess.run(cmd, cwd=cwd, check=check, 
                                capture_output=capture, text=capture)
        if capture:
            return result.stdout.strip() if result.returncode == 0 else None
        return result.returncode == 0
    except subprocess.CalledProcessError as e:
        print_error(f"Command failed with exit code {e.returncode}")
        return False if not capture else None
    except FileNotFoundError:
        print_error(f"Command not found: {cmd[0]}")
        print_error("Make sure CMake is installed and in your PATH")
        return False if not capture else None


def get_machine_arch():
    """Get the machine architecture."""
    import platform
    machine = platform.machine().lower()
    if machine in ('arm64', 'aarch64'):
        return 'arm64'
    return 'x86_64'


def get_homebrew_prefix():
    """Get Homebrew prefix on macOS, matching the build architecture."""
    if sys.platform != "darwin":
        return None
    
    arch = get_machine_arch()
    
    # Apple Silicon Macs should use /opt/homebrew for arm64 builds
    # Intel Macs use /usr/local
    if arch == 'arm64':
        if Path("/opt/homebrew").exists():
            return Path("/opt/homebrew")
        else:
            print_warning("Apple Silicon detected but /opt/homebrew not found!")
            print_warning("You may have Intel Homebrew installed.")
            print_warning("Install arm64 Homebrew with:")
            print_warning("  /bin/bash -c \"$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\"")
            # Fall back to /usr/local but warn
            if Path("/usr/local/Homebrew").exists():
                print_warning("Using Intel Homebrew at /usr/local (may cause architecture issues)")
                return Path("/usr/local")
            return None
    else:
        # Intel Mac
        if Path("/usr/local/Homebrew").exists():
            return Path("/usr/local")
        elif Path("/opt/homebrew").exists():
            return Path("/opt/homebrew")
        return None


def check_macos_dependencies():
    """Check if required macOS dependencies are installed via Homebrew."""
    if sys.platform != "darwin":
        return True
    
    print_status("Checking macOS dependencies...")
    arch = get_machine_arch()
    brew_prefix = get_homebrew_prefix()
    
    if not brew_prefix:
        print_warning("Homebrew not found. Install from https://brew.sh")
        return False
    
    # Check for architecture mismatch
    if arch == 'arm64' and str(brew_prefix) == '/usr/local':
        print_warning("")
        print_warning("=" * 60)
        print_warning("ARCHITECTURE MISMATCH DETECTED!")
        print_warning("=" * 60)
        print_warning("You have an Apple Silicon Mac but only Intel Homebrew found.")
        print_warning("Libraries from /usr/local are x86_64, but we're building arm64.")
        print_warning("")
        print_warning("To fix this:")
        print_warning("  1. Install native arm64 Homebrew (in a native Terminal):")
        print_warning('     /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"')
        print_warning("  2. Add to your shell profile:")
        print_warning('     eval "$(/opt/homebrew/bin/brew shellenv)"')
        print_warning("  3. Restart your terminal and run:")
        print_warning("     brew install glm glew glfw assimp")
        print_warning("=" * 60)
        return False
    
    missing = []
    for pkg, desc in MACOS_DEPENDENCIES.items():
        pkg_path = brew_prefix / "opt" / pkg
        if not pkg_path.exists():
            missing.append((pkg, desc))
    
    if missing:
        print_warning("Missing dependencies at " + str(brew_prefix) + ":")
        for pkg, desc in missing:
            print(f"  - {pkg}: {desc}")
        print()
        
        # Check if they exist in the wrong Homebrew location
        wrong_prefix = Path("/usr/local") if str(brew_prefix) == "/opt/homebrew" else Path("/opt/homebrew")
        wrong_location = []
        for pkg, desc in missing:
            if (wrong_prefix / "opt" / pkg).exists():
                wrong_location.append(pkg)
        
        if wrong_location and arch == 'arm64':
            print_warning("")
            print_warning("=" * 60)
            print_warning("DEPENDENCIES IN WRONG HOMEBREW!")
            print_warning("=" * 60)
            print_warning(f"Found packages in {wrong_prefix} (Intel/x86_64):")
            print_warning(f"  {', '.join(wrong_location)}")
            print_warning("")
            print_warning("These won't work for arm64 builds!")
            print_warning("Install arm64 versions with:")
            print_warning(f"  /opt/homebrew/bin/brew install {' '.join(pkg for pkg, _ in missing)}")
            print_warning("=" * 60)
        else:
            print_status("Install missing dependencies with:")
            print(f"  brew install {' '.join(pkg for pkg, _ in missing)}")
        return False
    
    print_status(f"All macOS dependencies found at {brew_prefix}!")
    return True


def install_macos_dependencies():
    """Install macOS dependencies via Homebrew."""
    if sys.platform != "darwin":
        print_error("This command is only available on macOS")
        return False
    
    arch = get_machine_arch()
    brew_prefix = get_homebrew_prefix()
    
    if not brew_prefix:
        print_error("Homebrew not found. Install from https://brew.sh")
        return False
    
    # Warn about architecture mismatch
    if arch == 'arm64' and str(brew_prefix) == '/usr/local':
        print_warning("")
        print_warning("=" * 60)
        print_warning("ARCHITECTURE MISMATCH DETECTED!")
        print_warning("=" * 60)
        print_warning("You have an Apple Silicon Mac but are using Intel Homebrew.")
        print_warning("This will cause linker errors (x86_64 vs arm64).")
        print_warning("")
        print_warning("To fix this, install native arm64 Homebrew:")
        print_warning("  1. Open a native Terminal (not Rosetta)")
        print_warning("  2. Run: /bin/bash -c \"$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\"")
        print_warning("  3. Add to PATH: eval \"$(/opt/homebrew/bin/brew shellenv)\"")
        print_warning("  4. Then run: brew install glm glew glfw assimp")
        print_warning("=" * 60)
        print_warning("")
        return False
    
    # Check which packages need to be installed
    to_install = []
    for pkg in MACOS_DEPENDENCIES:
        pkg_path = brew_prefix / "opt" / pkg
        if not pkg_path.exists():
            to_install.append(pkg)
    
    if not to_install:
        print_status("All dependencies already installed!")
        return True
    
    print_status(f"Installing: {', '.join(to_install)}")
    cmd = ["brew", "install"] + to_install
    try:
        subprocess.run(cmd, check=True)
        print_status("Dependencies installed successfully!")
        return True
    except subprocess.CalledProcessError:
        print_error("Failed to install dependencies")
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
    
    # macOS-specific: Add Homebrew paths
    if sys.platform == "darwin":
        brew_prefix = get_homebrew_prefix()
        if brew_prefix:
            cmake_args.append(f"-DHOMEBREW_PREFIX={brew_prefix}")
            # Set CMAKE_PREFIX_PATH to help find_package locate Homebrew packages
            cmake_args.append(f"-DCMAKE_PREFIX_PATH={brew_prefix}")
    
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
  # Install dependencies (macOS only)
  python build.py --setup

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

  # Clean and rebuild
  python build.py --clean
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
    
    parser.add_argument(
        "--setup",
        action="store_true",
        help="Install dependencies (macOS: via Homebrew)"
    )
    
    parser.add_argument(
        "--skip-dep-check",
        action="store_true",
        help="Skip dependency check before building"
    )
    
    args = parser.parse_args()
    
    # Handle list targets
    if args.list_targets:
        list_targets()
        return 0
    
    # Handle setup (install dependencies)
    if args.setup:
        if sys.platform == "darwin":
            if not install_macos_dependencies():
                return 1
            print_status("Setup complete!")
            return 0
        else:
            print_status("On Windows, please ensure game_libs directory is set up.")
            return 0
    
    # Check dependencies (macOS only)
    if sys.platform == "darwin" and not args.skip_dep_check:
        if not check_macos_dependencies():
            print_error("Missing dependencies. Run 'python build.py --setup' to install them.")
            return 1
    
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

