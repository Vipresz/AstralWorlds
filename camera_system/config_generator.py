#!/usr/bin/env python3
"""
Camera System Configuration Generator
Dictionary-based format: positions/orientations and frames for cameras and objects.
"""

import json
import os
import sys
import subprocess
import argparse
import numpy as np
from pathlib import Path
from typing import List, Dict, Any, Optional


# CONFIGURATION - EDIT THIS SECTION

TOTAL_FRAMES = 60
CAPTURE_INTERVAL = 1
def generate_circular_camera(radius, height):
    """Generate camera positions for a circular path around the origin.
    
    Args:
        radius: Radius of the circular path
        height: Z height of the camera
    """
    num_keyframes = TOTAL_FRAMES//CAPTURE_INTERVAL
    
    positions = []
    frames = []
    
    for i in range(num_keyframes + 1):
        if num_keyframes == 0:
            frame = 0
        else:
            frame = int(i * (TOTAL_FRAMES - 1) / num_keyframes)
        
        frame = min(frame, TOTAL_FRAMES - 1)
        angle = (frame / (TOTAL_FRAMES - 1)) * 2 * np.pi
        
        x = radius * np.cos(angle)
        y = radius * np.sin(angle)
        z = height
        
        # Point toward center of x-y plane (0, 0) at z=5 (about 5 units above ground)
        target_z = 10.0
        heading_x = -x  # Toward origin x
        heading_y = -y  # Toward origin y
        heading_z = target_z - z  # Point downward toward z=5
        
        # Normalize the entire heading vector
        heading_length = np.sqrt(heading_x**2 + heading_y**2 + heading_z**2)
        if heading_length > 0.0001:
            heading_x /= heading_length
            heading_y /= heading_length
            heading_z /= heading_length
        else:
            heading_x, heading_y, heading_z = 0.0, 0.0, -1.0  # Default downward if at origin
        
        positions.append([float(x), float(y), float(z), float(heading_x), float(heading_y), float(heading_z)])
        frames.append(frame)
    
    if len(positions) != len(frames):
        print(f"ERROR: Position count ({len(positions)}) doesn't match frame count ({len(frames)})")
    
    return positions, frames

def generate_camera_positions(square_size=50.0, height=25.0, target_z=10.0):
    """Generate camera positions at corners and middle of sides of a square.
    
    Args:
        square_size: Size of the square (default 50.0)
        height: Z height of cameras (default 25.0)
        target_z: Z coordinate to point cameras at (default 10.0)
    
    Returns:
        List of (x, y, z, target_z) tuples
    """
    half_size = square_size / 2.0
    cameras = []
    
    # Corners of the square
    cameras.append((half_size, half_size, height, target_z))   # (25, 25, 25)
    cameras.append((-half_size, half_size, height, target_z))  # (-25, 25, 25)
    cameras.append((-half_size, -half_size, height, target_z)) # (-25, -25, 25)
    cameras.append((half_size, -half_size, height, target_z))  # (25, -25, 25)
    
    # Middle of each side
    cameras.append((0, half_size, height, target_z))   # (0, 25, 25) - North
    cameras.append((-half_size, 0, height, target_z)) # (-25, 0, 25) - West
    cameras.append((0, -half_size, height, target_z)) # (0, -25, 25) - South
    cameras.append((half_size, 0, height, target_z))  # (25, 0, 25) - East
    
    return cameras

# Generate camera positions for 1000x1000 square at height 300
CAMERA_POSITIONS = generate_camera_positions(square_size=1000.0, height=300.0, target_z=10.0)

def generate_static_objects():
    """Generate static objects configuration."""
    return {
        "character1": {
            "positions": [
                [0, 0.0, 0.0, 1.0, 0.0, 0.0],
            ],
            "frames": [0],  # Only frame 0 - object stays at origin for all frames
        },
    }

def generate_moving_objects(total_frames=60):
    """Generate moving objects with sinusoidal paths.
    
    Args:
        total_frames: Total number of frames (default 60)
    
    Returns:
        Dictionary of object configurations
    """
    # Starting positions
    start_positions = [
        (200.0, 200.0, 0.0),   # Object 1
        (-200.0, 200.0, 0.0),  # Object 2
        (-200.0, -200.0, 0.0), # Object 3
        (200.0, -200.0, 0.0),  # Object 4
    ]
    
    # Different speeds for each object (frequencies for sinusoidal motion)
    speeds = [0.5, 0.75, 1.0, 1.25]  # Different frequencies
    
    # Different amplitudes for each object
    amplitudes = [50.0, 70.0, 60.0, 80.0]
    
    objects = {}
    
    for i, (start_x, start_y, start_z) in enumerate(start_positions):
        obj_name = f"character{i+1}"
        positions = []
        frames = []
        
        speed = speeds[i]
        amplitude = amplitudes[i]
        
        for frame in range(total_frames):
            # Normalized time from 0 to 2*pi
            t = (frame / (total_frames - 1)) * 2 * np.pi * speed
            
            # Sinusoidal motion in X and Y directions
            # Each object moves in a different pattern
            offset_x = amplitude * np.sin(t)
            offset_y = amplitude * np.cos(t * 0.7)  # Different phase for Y
            
            x = start_x + offset_x
            y = start_y + offset_y
            z = start_z
            
            # Default orientation (facing positive X)
            positions.append([float(x), float(y), float(z), 1.0, 0.0, 0.0])
            frames.append(frame)
        
        objects[obj_name] = {
            "positions": positions,
            "frames": frames
        }
    
    return objects

OBJECT_FILES = {
    "character1": {
        "obj_file": "graphics/Models/character/graphics/male_model.obj",
        "fbx_file": None,
        "skel_file": None,
        "weights_file": None,
    },
}

LIGHTS = {
    "Point Light at 10,10,10": {
        "type": "point",
        "colour": [1.0, 1.0, 1.0],
        "ambient_intensity": 0.2,
        "diffuse_intensity": 1.0,
        "direction_or_position": [10.0, 10.0, 10.0],
        "constant": 1.0,
        "linear": 0.09,
        "exponent": 0.032,
        "edge": 0.0,
    },
    "Directional Light": {
        "type": "directional",
        "colour": [0.8, 0.8, 0.9],
        "ambient_intensity": 0.4,
        "diffuse_intensity": 0.6,
        "direction_or_position": [0.0, 0.0, -1.0],
        "constant": 1.0,
        "linear": 0.09,
        "exponent": 0.032,
        "edge": 0.0,
    },
}


# CODE - DO NOT EDIT BELOW THIS LINE

def normalize_vector(vec: List[float]) -> List[float]:
    """Normalize a 3D vector."""
    if len(vec) < 3:
        return [0.0, 0.0, -1.0]
    length = (vec[0]**2 + vec[1]**2 + vec[2]**2) ** 0.5
    if length > 0.0001:
        return [vec[0]/length, vec[1]/length, vec[2]/length]
    return [0.0, 0.0, -1.0]


def auto_detect_skel_weights(obj_file: str) -> tuple:
    """Auto-detect skel and weights files from obj file path."""
    obj_path = Path(obj_file)
    obj_dir = obj_path.parent
    obj_stem = obj_path.stem
    
    skel_file = None
    weights_file = None
    
    if obj_dir.exists():
        skel_path = obj_dir / f"{obj_stem}.skel"
        weights_path = obj_dir / f"{obj_stem}.weights"
        
        if skel_path.exists():
            skel_file = str(skel_path)
        if weights_path.exists():
            weights_file = str(weights_path)
    
    return skel_file, weights_file


def create_cameras_dict(camera_positions):
    """Create cameras dictionary from camera positions.
    
    Args:
        camera_positions: List of (x, y, z, target_z) tuples
    
    Returns:
        Dictionary of camera configurations
    """
    cameras_dict = {}
    
    for i, camera_data in enumerate(camera_positions):
        if len(camera_data) == 4:
            x, y, z, target_z = camera_data
        else:
            x, y, z = camera_data[:3]
            target_z = 0.0
        
        # Calculate heading direction from camera position (x, y, z) to target (0, 0, target_z)
        heading_x = 0.0 - x  # Target X is 0
        heading_y = 0.0 - y  # Target Y is 0
        heading_z = target_z - z  # Target Z is target_z
        
        # Normalize heading
        heading_length = np.sqrt(heading_x**2 + heading_y**2 + heading_z**2)
        if heading_length > 0.0001:
            heading_x /= heading_length
            heading_y /= heading_length
            heading_z /= heading_length
        else:
            heading_x, heading_y, heading_z = 0.0, 0.0, -1.0
        
        camera_name = f"Anchor_Camera_{i+1}"
        cameras_dict[camera_name] = {
            "positions": [[float(x), float(y), float(z), float(heading_x), float(heading_y), float(heading_z)]],
            "frames": [0],
            "can_move": False
        }
    
    return cameras_dict

def create_object_files_dict(objects_dict, base_object_files=None):
    """Create object files dictionary from objects and base object files.
    
    Args:
        objects_dict: Dictionary of object configurations
        base_object_files: Base object files dictionary (default: OBJECT_FILES from global scope)
    
    Returns:
        Dictionary of object file configurations
    """
    if base_object_files is None:
        base_object_files = OBJECT_FILES
    
    object_files_dict = {}
    
    # For each object in objects_dict, try to find matching file in base_object_files
    for obj_name in objects_dict.keys():
        # Try to find a matching base object file
        # First try exact match, then try "character1" as fallback
        file_data = None
        if obj_name in base_object_files:
            file_data = base_object_files[obj_name]
        elif "character1" in base_object_files:
            file_data = base_object_files["character1"]
        
        if file_data:
            obj_file = file_data.get("obj_file", "")
            if obj_file:
                skel_file = file_data.get("skel_file")
                weights_file = file_data.get("weights_file")
                if skel_file is None or weights_file is None:
                    auto_skel, auto_weights = auto_detect_skel_weights(obj_file)
                    if skel_file is None:
                        skel_file = auto_skel
                    if weights_file is None:
                        weights_file = auto_weights
                
                object_files_dict[obj_name] = {
                    "obj_file": obj_file,
                    "fbx_file": file_data.get("fbx_file"),
                    "skel_file": skel_file,
                    "weights_file": weights_file
                }
    
    return object_files_dict

def create_config(cameras_dict, objects_dict, object_files_dict=None, capture_interval=CAPTURE_INTERVAL):
    """Create configuration from cameras and objects dictionaries.
    
    Args:
        cameras_dict: Dictionary of camera configurations
        objects_dict: Dictionary of object configurations
        object_files_dict: Dictionary of object file configurations (if None, will be generated)
        capture_interval: Frame interval for capturing images
            - When capture_interval == 1: Capture every frame (0 to total_frames-1) for every camera
            - When capture_interval > 1: Capture every Nth frame (0, N, 2N, ...) for every camera
    """
    
    for name, data in cameras_dict.items():
        positions = data["positions"]
        frames = data["frames"]
        if len(positions) != len(frames):
            print(f"Warning: Camera '{name}' has {len(positions)} positions but {len(frames)} frames.")
        for i, pos in enumerate(positions):
            if len(pos) < 6:
                print(f"Warning: Camera '{name}' position {i} has less than 6 values. Padding with zeros.")
                positions[i] = pos + [0.0] * (6 - len(pos))
    
    for name, data in objects_dict.items():
        positions = data["positions"]
        frames = data["frames"]
        if len(positions) != len(frames):
            print(f"Warning: Object '{name}' has {len(positions)} positions but {len(frames)} frames.")
        for i, pos in enumerate(positions):
            if len(pos) < 6:
                print(f"Warning: Object '{name}' position {i} has less than 6 values. Padding with zeros.")
                positions[i] = pos + [0.0] * (6 - len(pos))
    
    # Generate object_files_dict if not provided
    if object_files_dict is None:
        object_files_dict = create_object_files_dict(objects_dict)
    
    lights_list = []
    for name, light_data in LIGHTS.items():
        light_type = light_data.get("type", "directional")
        direction_or_pos = light_data.get("direction_or_position", [0.0, 0.0, -1.0])
        
        if light_type == "directional":
            direction_or_pos = normalize_vector(direction_or_pos)
        
        light_config = {
            "name": name,
            "type": light_type,
            "colour": light_data.get("colour", [1.0, 1.0, 1.0]),
            "ambient_intensity": light_data.get("ambient_intensity", 0.3),
            "diffuse_intensity": light_data.get("diffuse_intensity", 0.7),
            "direction_or_position": direction_or_pos,
            "constant": light_data.get("constant", 1.0),
            "linear": light_data.get("linear", 0.09),
            "exponent": light_data.get("exponent", 0.032),
            "edge": light_data.get("edge", 0.0),
        }
        lights_list.append(light_config)
    
    # Determine total_frames from objects (use max frame from all objects)
    total_frames = TOTAL_FRAMES
    for name, data in objects_dict.items():
        frames = data["frames"]
        if frames:
            max_frame = max(frames)
            total_frames = max(total_frames, max_frame + 1)
    
    config = {
        "version": "2.0",
        "camera": {
            "position": [0.0, 2.0, 5.0],
            "target": [0.0, 0.0, 0.0],
            "distance": 50.0,
            "yaw": 0.0,
            "pitch": 0.3
        },
        "total_duration": total_frames / 30.0,
        "total_frames": total_frames,
        "cameras": cameras_dict,
        "objects": objects_dict,
        "object_files": object_files_dict,
        "lights": lights_list,
        "capture_enabled": False,
        "capture_output_dir": "C:\\Users\\inque\\OneDrive\\Desktop\\Work\\games\\thegame\\build\\bin\\Release\\pics",
        "capture_interval": capture_interval
    }
    
    return config


def main():
    """Main function."""
    parser = argparse.ArgumentParser(
        description='Generate camera system configuration files'
    )
    parser.add_argument(
        '--play',
        action='store_true',
        help='Launch the camera system editor with the generated config file'
    )
    parser.add_argument(
        '--editor-path',
        default='build/bin/Release/camera_system_editor.exe',
        help='Path to camera_system_editor.exe'
    )
    parser.add_argument(
        '--capture-interval',
        type=int,
        default=5,
        help='Frame interval for capturing images (default: 5, captures every 5th frame)'
    )
    
    args = parser.parse_args()
    
    # Generate camera positions (same for both configs)
    camera_positions = generate_camera_positions(square_size=1000.0, height=300.0, target_z=10.0)
    cameras_dict = create_cameras_dict(camera_positions)
    
    # Generate static config
    static_objects = generate_static_objects()
    static_object_files = create_object_files_dict(static_objects)
    static_config = create_config(cameras_dict, static_objects, static_object_files, capture_interval=args.capture_interval)
    
    static_output_file = 'camera_config_static.json'
    with open(static_output_file, 'w') as f:
        json.dump(static_config, f, indent=2)
    
    print(f"Static configuration saved to: {static_output_file}")
    print(f"  Total frames: {static_config['total_frames']}")
    print(f"  Cameras: {len(static_config['cameras'])}")
    print(f"  Objects: {len(static_config['objects'])}")
    print(f"  Lights: {len(static_config.get('lights', []))}")
    
    # Generate moving config
    moving_objects = generate_moving_objects(total_frames=TOTAL_FRAMES)
    moving_object_files = create_object_files_dict(moving_objects)
    moving_config = create_config(cameras_dict, moving_objects, moving_object_files, capture_interval=args.capture_interval)
    
    moving_output_file = 'camera_config_moving.json'
    with open(moving_output_file, 'w') as f:
        json.dump(moving_config, f, indent=2)
    
    print(f"\nMoving configuration saved to: {moving_output_file}")
    print(f"  Total frames: {moving_config['total_frames']}")
    print(f"  Cameras: {len(moving_config['cameras'])}")
    print(f"  Objects: {len(moving_config['objects'])}")
    print(f"  Lights: {len(moving_config.get('lights', []))}")
    
    if args.play:
        editor_path = args.editor_path
        if not os.path.exists(editor_path):
            print(f"\nError: Editor not found at {editor_path}")
            print(f"Please specify the correct path with --editor-path")
            return 1
        
        print(f"\nLaunching camera system editor with static config...")
        try:
            subprocess.Popen([editor_path, '--config', static_output_file])
            print(f"Editor launched successfully!")
        except Exception as e:
            print(f"\nError launching editor: {e}")
            return 1
    else:
        print(f"\nTo use these configurations, run:")
        print(f"  camera_system_editor.exe --config {static_output_file}")
        print(f"  camera_system_editor.exe --config {moving_output_file}")
        print(f"\nOr use --play to launch automatically:")
        print(f"  python config_generator.py --play")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
