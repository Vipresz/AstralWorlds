# Refactoring Analysis: Function Placement

This document analyzes which functions from `CameraSystemEditor::LoadConfig` should be moved to:
1. **Config class** (`camera_system/config/`)
2. **CameraSystem** (`camera_system/camera/`)

## Functions to Move to Config Class (`camera_system/config/ConfigManager.h`)

These functions handle **loading and processing configuration data**:

### 1. `LoadCaptureSettings()`
- **Purpose**: Loads capture-related settings from config
- **Dependencies**: `SystemConfig`, editor member variables (`m_captureEnabled`, `m_captureOutputDir`, `m_captureInterval`)
- **Decision**: Move to `ConfigManager` but needs editor state reference
- **Note**: Could return a struct with capture settings instead

### 2. `LoadAnimatedCameras()`
- **Purpose**: Loads animated camera configurations from config
- **Dependencies**: `SystemConfig`, editor state (cameras, frames, etc.)
- **Decision**: **MOVE** - Core config processing logic
- **Returns**: Should populate camera data structures passed by reference

### 3. `LoadLegacyCamera()`
- **Purpose**: Handles legacy single-camera config format
- **Dependencies**: `SystemConfig`, camera management functions
- **Decision**: **MOVE** - Config loading logic
- **Note**: Will need to call camera management functions

### 4. `LoadObjects()`
- **Purpose**: Loads scene objects from config
- **Dependencies**: `SystemConfig`, `ObjectManager`, editor state
- **Decision**: **MOVE** - Config processing
- **Returns**: Should populate objects and frame data

### 5. `FillCameraKeyframes()`
- **Purpose**: Processes camera keyframes into frame arrays
- **Dependencies**: Editor frame arrays (`m_cameraFrames`, `m_cameraFrameCounts`)
- **Decision**: **MOVE** - Config data processing
- **Note**: Takes frame arrays as parameters

### 6. `FillObjectKeyframes()`
- **Purpose**: Processes object keyframes into frame arrays
- **Dependencies**: Editor frame arrays (`m_objectFrames`, `m_objectFrameCounts`)
- **Decision**: **MOVE** - Config data processing
- **Note**: Takes frame arrays as parameters

## Functions to Move to CameraSystem (`camera_system/camera/CameraSystem.h`)

These functions handle **camera management operations**:

### 1. `FindAnchorCameraIndex()`
- **Purpose**: Finds the first anchor (non-movable) camera
- **Dependencies**: `std::vector<Camera>`
- **Decision**: **MOVE** - Pure camera query function
- **Signature**: `size_t FindAnchorCameraIndex(const std::vector<Camera>& cameras) const;`

### 2. `HasFreeCamera()`
- **Purpose**: Checks if a free (movable) camera exists
- **Dependencies**: `std::vector<Camera>`
- **Decision**: **MOVE** - Pure camera query function
- **Signature**: `bool HasFreeCamera(const std::vector<Camera>& cameras) const;`

### 3. `CalculateYawPitchFromDirection()`
- **Purpose**: Calculates camera yaw/pitch from direction vector
- **Dependencies**: None (pure math function)
- **Decision**: **MOVE** - Camera utility function
- **Signature**: `static void CalculateYawPitchFromDirection(const glm::vec3& direction, float& yaw, float& pitch);`
- **Note**: Could be static utility

### 4. `EnsureFreeCameraExists()`
- **Purpose**: Ensures a free camera exists, creates one if needed
- **Dependencies**: Camera management, `AddCamera`, `FindAnchorCameraIndex`
- **Decision**: **MOVE** - Camera management operation
- **Signature**: `void EnsureFreeCameraExists(std::vector<Camera>& cameras, glm::vec3& freeCameraPos, float& freeCameraYaw, float& freeCameraPitch);`
- **Note**: Needs to call `AddCamera` which is already in CameraSystem

## Functions to Keep in CameraSystemEditor

These functions are **editor-specific state management**:

### 1. `ResetFrameArrays()`
- **Purpose**: Resets internal frame data structures
- **Dependencies**: Editor member variables (`m_cameraFrames`, `m_objectFrames`, etc.)
- **Decision**: **KEEP** - Editor-specific state management
- **Reason**: Directly manipulates editor's internal state

## Proposed Class Structure

### ConfigManager (`camera_system/config/ConfigManager.h`)

```cpp
class ConfigManager {
public:
    struct CaptureSettings {
        bool enabled;
        std::string outputDir;
        int interval;
    };
    
    struct CameraLoadResult {
        std::vector<Camera> cameras;
        int totalFrames;
        int numCameras;
        // Frame data would be passed by reference
    };
    
    struct ObjectLoadResult {
        // Objects would be loaded via ObjectManager
        // Frame data passed by reference
    };
    
    // Load capture settings from config
    static CaptureSettings LoadCaptureSettings(const SystemConfig& config, bool preserveExisting = false);
    
    // Load animated cameras from config
    static void LoadAnimatedCameras(
        const SystemConfig& config,
        std::vector<Camera>& cameras,
        CameraFrame frames[MAX_CAMERAS][MAX_FRAMES],
        int frameCounts[MAX_CAMERAS],
        int& numCameras,
        int& totalFrames
    );
    
    // Load legacy camera config
    static void LoadLegacyCamera(
        const SystemConfig& config,
        std::vector<Camera>& cameras,
        // ... other params
    );
    
    // Load objects from config
    static void LoadObjects(
        const SystemConfig& config,
        const std::string& configFile,
        ObjectManager& objectManager,
        // ... frame arrays and object storage
    );
    
    // Process keyframes into frame arrays
    static void FillCameraKeyframes(
        int cameraIndex,
        const std::vector<CameraKeyframe>& sortedKeyframes,
        int totalFrames,
        CameraFrame frames[MAX_CAMERAS][MAX_FRAMES],
        int frameCounts[MAX_CAMERAS]
    );
    
    static void FillObjectKeyframes(
        size_t objectIndex,
        const std::vector<ObjectKeyframe>& sortedKeyframes,
        int totalFrames,
        ObjectFrame frames[MAX_OBJECTS][MAX_FRAMES],
        int frameCounts[MAX_OBJECTS],
        bool useFrames[MAX_OBJECTS]
    );
};
```

### CameraSystem Extensions (`camera_system/camera/CameraSystem.h`)

```cpp
class CameraSystem {
    // ... existing methods ...
    
    // Camera query functions
    size_t FindAnchorCameraIndex(const std::vector<Camera>& cameras) const;
    bool HasFreeCamera(const std::vector<Camera>& cameras) const;
    
    // Camera utility functions
    static void CalculateYawPitchFromDirection(const glm::vec3& direction, float& yaw, float& pitch);
    
    // Camera management
    void EnsureFreeCameraExists(
        std::vector<Camera>& cameras,
        glm::vec3& freeCameraPos,
        float& freeCameraYaw,
        float& freeCameraPitch
    );
};
```

## Summary

| Function | Destination | Reason |
|----------|-------------|--------|
| `LoadCaptureSettings()` | ConfigManager | Config loading |
| `LoadAnimatedCameras()` | ConfigManager | Config processing |
| `LoadLegacyCamera()` | ConfigManager | Config loading |
| `LoadObjects()` | ConfigManager | Config processing |
| `FillCameraKeyframes()` | ConfigManager | Config data processing |
| `FillObjectKeyframes()` | ConfigManager | Config data processing |
| `FindAnchorCameraIndex()` | CameraSystem | Camera query |
| `HasFreeCamera()` | CameraSystem | Camera query |
| `CalculateYawPitchFromDirection()` | CameraSystem | Camera utility |
| `EnsureFreeCameraExists()` | CameraSystem | Camera management |
| `ResetFrameArrays()` | CameraSystemEditor | Editor state management |

## Benefits

1. **Separation of Concerns**: Config loading separated from camera management
2. **Reusability**: Camera utilities can be used elsewhere
3. **Testability**: Each class can be tested independently
4. **Maintainability**: Clear responsibilities for each class
5. **Reduced Coupling**: Editor doesn't need to know config processing details


