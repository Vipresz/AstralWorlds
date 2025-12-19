# Camera System Editor

3D editor for camera and object animation with frame-based keyframes.

## Quick Start

```powershell
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Run: `build/bin/Release/camera_system_editor.exe --config camera_config.json`

## Configuration

Edit `config_generator.py` to define cameras, objects, and lights, then generate the JSON:

```bash
python config_generator.py camera_config.json
```

### Configuration Format

The JSON uses dictionaries for cameras and objects with keyframe data:

```json
{
  "total_frames": 300,
  "cameras": {
    "Camera Name": {
      "positions": [[x, y, z, heading_x, heading_y, heading_z], ...],
      "frames": [0, 10, 20, ...],
      "can_move": false
    }
  },
  "objects": {
    "Object Name": {
      "positions": [[x, y, z, heading_x, heading_y, heading_z], ...],
      "frames": [0, 10, 20, ...]
    }
  },
  "object_files": {
    "Object Name": {
      "obj_file": "path/to/model.obj"
    }
  },
  "lights": [
    {
      "name": "Light Name",
      "type": "directional|point|spot",
      "colour": [r, g, b],
      "ambient_intensity": 0.2,
      "diffuse_intensity": 1.0,
      "direction_or_position": [x, y, z]
    }
  ]
}
```

- **Cameras**: Dictionary mapping names to position/orientation keyframes at specific frames
- **Objects**: Dictionary mapping names to position/orientation keyframes at specific frames
- **Object Files**: Dictionary mapping object names to file paths (`.skel` and `.weights` auto-detected)
- **Lights**: Array of light configurations (directional, point, or spot)

Intermediate frames are filled with the most recent keyframe data.

## Controls

- **ESC** - Exit
- **1-5** - Switch cameras
- **WASD** - Move free camera
- **Space/Shift** - Move free camera up/down
- **Right Mouse + Drag** - Rotate free camera

## Coordinate System

- Ground plane: **x-y axis** (z is up)
- Camera yaw: Rotation around z-axis
- Camera pitch: Rotation around y-axis (unclamped, full 360°)
