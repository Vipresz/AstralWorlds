# Visual Skeleton Editor

Interactive 3D editor for placing bones on character meshes, generating vertex weights, and previewing animations.

## Quick Start

### Building

```powershell
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Executable: `build/bin/Release/visual_skeleton_editor.exe`

### Basic Usage

```powershell
visual_skeleton_editor.exe --mesh "path/to/mesh.obj" --skel "path/to/skeleton.skel"
```

**Command-line flags:**
- `--mesh <path>` - Path to mesh file (OBJ/FBX format)
- `--skel <path>` - Path to skeleton file (optional, auto-created if not specified)
- `--anim <path>` - Path to animation directory or single FBX file (recursively loads all .fbx files)
- `--help` - Show help message

## Controls

### Mouse
- **Left Click** - Place bone at clicked position
- **Middle Click** - Remove bone at clicked position
- **Right Mouse + Drag** - Rotate camera
- **Shift + Right Mouse + Drag** - Pan camera
- **Mouse Wheel** - Zoom in/out

### Keyboard
- **M** - Toggle mesh visibility
- **V** - Toggle contour visibility
- **S** - Save skeleton manually
- **ESC** - Exit editor

## Workflow

### 1. Place Bones

1. Load your mesh using `--mesh` flag
2. The editor shows which bone to place next at the top of the screen
3. Click on the mesh where each joint should be located
4. Bones are placed in hierarchical order automatically

### 2. Generate Weights

1. Place all bones (editor will prompt you)
2. Click **"Generate Weights from Mesh"** button in the right panel
3. Weights are automatically saved to `.weights` file

### 3. Load Animations

1. Use `--anim` flag to load animations:
   ```powershell
   visual_skeleton_editor.exe --mesh "model.obj" --skel "model.skel" --anim "path/to/animations/"
   ```
2. Animations load recursively from the directory
3. Use the animation controls in the right panel to play/pause/step through

### 4. Preview Animation

1. Enable **"Animate Mesh"** toggle in the right panel
2. Select an animation from the dropdown
3. Use the time slider to scrub through the animation
4. Click **"Play"** to animate or **"Pause"** to stop

## UI Controls (Right Panel)

- **Mesh: ON/OFF** - Toggle mesh visibility
- **Contours: ON/OFF** - Toggle contour lines (helpful for seeing mesh structure)
- **Contour Density** - Adjust spacing between contour slices (0.05 to 2.0)
- **Connections: ON/OFF** - Show/hide bone connection lines
- **Generate Weights from Mesh** - Automatically generate vertex weights based on bone proximity
- **Auto-Save: ON/OFF** - Enable/disable automatic saving when bones are added/removed
- **Save Skeleton** - Manually save the skeleton file
- **Reset** - Clear all bones (with confirmation dialog)
- **Animation Controls** - Play, pause, time slider, and animation selection dropdown
- **Animate Mesh** - Enable/disable mesh deformation with skeleton animation
- **Reset to Bind Pose** - Reset skeleton and mesh to original positions

## Bone List

The scrollable bone list shows:
- ✅ Green bullet + text = Bone placed (click to remove)
- ⚪ Yellow bullet + text = Bone not yet placed (click to restore if previously placed)

## File Formats

- **`.skel`** - Skeleton definition file (bone positions, hierarchy, IDs)
- **`.weights`** - Vertex weight file (which bones influence each vertex)
- **`.obj` / `.fbx`** - Mesh files
- **`.fbx`** - Animation files

## Tips

- Start with the root bone (Hips) and work outward
- Use contours to better see the mesh structure
- Enable auto-save to avoid losing work
- Generate weights after placing all bones
- Use "Reset to Bind Pose" to return to original pose
- Animations are paused by default - use the UI controls to play them
