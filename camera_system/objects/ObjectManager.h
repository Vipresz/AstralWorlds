#pragma once

#include "../../engine/platform/render/Model.h"
#include "../../engine/platform/Logger.h"
#include "../helper/ConfigLoader.h"
#include <vector>
#include <string>
#include <memory>
#include <filesystem>
#include <glm/glm.hpp>

namespace Engine {
namespace Editors {
namespace CameraSystem {

// Forward declaration
struct SceneObject;

class ObjectManager {
public:
    ObjectManager();
    ~ObjectManager();
    
    // Load an object from OBJ file with size calculation
    // Returns the created SceneObject, or nullptr on failure
    std::unique_ptr<SceneObject> LoadObject(
        const ObjectConfig& objConfig,
        const std::filesystem::path& configDir,
        float defaultSize  // Default size in units (use Constants::OBJECT_SIZE_DEFAULT)
    );
    
    // Apply rainbow colors to a collection of objects
    void ApplyRainbowColorsToObjects(std::vector<std::unique_ptr<SceneObject>>& objects);
    
private:
    // Helper to find model file path
    std::string FindModelPath(
        const std::string& objFile,
        const std::filesystem::path& configDir
    );
    
    // Helper to calculate and set object scale based on bounding box
    void SetObjectScale(SceneObject* obj, float targetSize);
};

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

