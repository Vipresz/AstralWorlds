# Quick Refactoring Summary

## 📦 Move to `camera_system/config/ConfigManager`

**Purpose**: Handle all configuration loading and data processing

| Function | What It Does |
|---------|--------------|
| `LoadCaptureSettings()` | Loads capture settings from config |
| `LoadAnimatedCameras()` | Processes animated camera configs |
| `LoadLegacyCamera()` | Handles legacy camera config format |
| `LoadObjects()` | Loads scene objects from config |
| `FillCameraKeyframes()` | Converts keyframes → frame arrays (cameras) |
| `FillObjectKeyframes()` | Converts keyframes → frame arrays (objects) |

**Why**: These all process configuration data and convert it into runtime formats.

---

## 🎥 Move to `camera_system/camera/CameraSystem`

**Purpose**: Camera management and utility functions

| Function | What It Does |
|---------|--------------|
| `FindAnchorCameraIndex()` | Finds first anchor (non-movable) camera |
| `HasFreeCamera()` | Checks if free camera exists |
| `CalculateYawPitchFromDirection()` | Math: direction → yaw/pitch angles |
| `EnsureFreeCameraExists()` | Creates free camera if missing |

**Why**: These are camera operations, not config-specific.

---

## 🏠 Keep in `CameraSystemEditor`

| Function | What It Does | Why Keep |
|---------|--------------|----------|
| `ResetFrameArrays()` | Clears internal frame data | Directly manages editor's internal state |

---

## Architecture After Refactoring

```
CameraSystemEditor
    ├── Uses ConfigManager to load configs
    ├── Uses CameraSystem for camera operations
    └── Manages its own internal state (ResetFrameArrays)
    
ConfigManager
    ├── LoadCaptureSettings()
    ├── LoadAnimatedCameras()
    ├── LoadLegacyCamera()
    ├── LoadObjects()
    ├── FillCameraKeyframes()
    └── FillObjectKeyframes()
    
CameraSystem
    ├── FindAnchorCameraIndex()
    ├── HasFreeCamera()
    ├── CalculateYawPitchFromDirection()
    └── EnsureFreeCameraExists()
```

---

## Benefits

✅ **Clear separation**: Config loading ≠ Camera management  
✅ **Reusable**: Camera utilities can be used anywhere  
✅ **Testable**: Each class has focused responsibilities  
✅ **Maintainable**: Easy to find where things belong  


