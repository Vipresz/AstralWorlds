#include "ObjectManager.h"
#include "../GlobalConstants.h"
#include "../CameraSystemEditor.h"
#include "../../engine/platform/render/Model.h"
#include "../../engine/platform/Logger.h"
#include <filesystem>
#include <algorithm>
#include <limits>

using namespace Engine::Platform::Render;
using EngineLogging::Logger;
using namespace Engine::Editors::CameraSystem::Constants;

namespace Engine {
namespace Editors {
namespace CameraSystem {

ObjectManager::ObjectManager() {
}

ObjectManager::~ObjectManager() {
}

std::unique_ptr<SceneObject> ObjectManager::LoadObject(
    const ObjectConfig& objConfig,
    const std::filesystem::path& configDir,
    float defaultSize) {
    
    // Create new object
    auto obj = std::make_unique<SceneObject>();
    obj->name = objConfig.name;
    obj->meshFilePath = objConfig.objFile;
    obj->animationFilePath = objConfig.animationFbx;
    
    // Set position, scale, rotation, visibility from config
    if (!objConfig.keyframes.empty()) {
        obj->position = objConfig.keyframes[0].position;
    } else if (!objConfig.path.empty()) {
        obj->position = objConfig.path[0].position;
    } else {
        obj->position = glm::vec3(0.0f);
    }
    
    obj->scale = objConfig.scale;
    obj->rotation = objConfig.rotation;
    obj->visible = objConfig.visible;
    
    // Load model - try multiple path resolutions
    obj->model = new Model();
    std::string actualObjPath;
    bool modelLoaded = false;
    
    if (objConfig.objFile.empty()) {
        LOG_WARNING_FMT("CameraSystem", "Object '%s' has no obj_file specified - object will be added without model", objConfig.name.c_str());
        // Don't skip - add object anyway so it can be positioned
        delete obj->model;
        obj->model = nullptr;
    } else {
        // Find model path
        actualObjPath = FindModelPath(objConfig.objFile, configDir);
        
        if (!actualObjPath.empty()) {
            try {
                obj->model->LoadModel(actualObjPath);
                if (obj->model->GetMeshCount() > 0) {
                    modelLoaded = true;
                    obj->meshFilePath = actualObjPath;
                    LOG_INFO_FMT("CameraSystem", "Loaded model: %s (%zu meshes)", 
                                actualObjPath.c_str(), obj->model->GetMeshCount());
                } else {
                    // Model loaded but has no meshes
                    LOG_WARNING_FMT("CameraSystem", "Model '%s' loaded but has no meshes - object will be added without model", actualObjPath.c_str());
                    delete obj->model;
                    obj->model = nullptr;
                }
            } catch (const std::exception& e) {
                LOG_WARNING_FMT("CameraSystem", "Exception loading model '%s': %s", actualObjPath.c_str(), e.what());
                delete obj->model;
                obj->model = nullptr;
            } catch (...) {
                LOG_WARNING_FMT("CameraSystem", "Unknown exception loading model '%s'", actualObjPath.c_str());
                delete obj->model;
                obj->model = nullptr;
            }
        } else {
            LOG_WARNING_FMT("CameraSystem", "Failed to find model file '%s' for object '%s'", 
                           objConfig.objFile.c_str(), objConfig.name.c_str());
            delete obj->model;
            obj->model = nullptr;
        }
    }
    
    // Animation disabled - only rendering static OBJ files
    obj->isAnimating = false;
    if (obj->animationPlayer) {
        delete obj->animationPlayer;
        obj->animationPlayer = nullptr;
    }
    
    // Set default scale to make bounding box the specified size
    if (obj->model && obj->model->GetMeshCount() > 0) {
        SetObjectScale(obj.get(), defaultSize);
        
        LOG_INFO_FMT("CameraSystem", "Object '%s' loaded with model (%zu meshes) at position (%.2f, %.2f, %.2f), scale (%.2f, %.2f, %.2f)", 
                    objConfig.name.c_str(), obj->model->GetMeshCount(),
                    obj->position.x, obj->position.y, obj->position.z,
                    obj->scale.x, obj->scale.y, obj->scale.z);
    } else {
        LOG_WARNING_FMT("CameraSystem", "Object '%s' loaded WITHOUT model (model file not found or invalid) at position (%.2f, %.2f, %.2f)", 
                       objConfig.name.c_str(),
                       obj->position.x, obj->position.y, obj->position.z);
    }
    
    return obj;
}

void ObjectManager::ApplyRainbowColorsToObjects(std::vector<std::unique_ptr<SceneObject>>& objects) {
    if (objects.empty()) {
        LOG_WARNING("CameraSystem", "No objects loaded to apply rainbow colors");
        return;
    }
    
    // Calculate min and max Z across all objects
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    
    for (const auto& obj : objects) {
        if (!obj || !obj->model) continue;
        
        float objMinX, objMaxX, objMinY, objMaxY, objMinZ, objMaxZ;
        obj->model->GetBounds(objMinX, objMaxX, objMinY, objMaxY, objMinZ, objMaxZ);
        
        // Account for object position
        float worldMinZ = objMinZ + obj->position.z;
        float worldMaxZ = objMaxZ + obj->position.z;
        
        minZ = std::min(minZ, worldMinZ);
        maxZ = std::max(maxZ, worldMaxZ);
    }
    
    if (std::abs(maxZ - minZ) < 0.0001f) {
        LOG_WARNING("CameraSystem", "All objects have the same Z position, cannot create gradient");
        return;
    }
    
    LOG_INFO_FMT("CameraSystem", "Applying rainbow colors: Z range [%.2f, %.2f]", minZ, maxZ);
    
    // Apply rainbow colors to each object
    for (const auto& obj : objects) {
        if (!obj || !obj->model) continue;
        
        // Get object's Z bounds in world space
        float objMinX, objMaxX, objMinY, objMaxY, objMinZ, objMaxZ;
        obj->model->GetBounds(objMinX, objMaxX, objMinY, objMaxY, objMinZ, objMaxZ);
        
        // Adjust for object position to get world space Z
        float worldMinZ = objMinZ + obj->position.z;
        float worldMaxZ = objMaxZ + obj->position.z;
        
        // Use the object's Z range for the gradient
        // This ensures each object gets a gradient based on its own Z extent
        // but normalized to the global min/max
        obj->model->ApplyRainbowColors(minZ, maxZ);
    }
    
    LOG_INFO("CameraSystem", "Rainbow colors applied to all objects");
}

std::string ObjectManager::FindModelPath(
    const std::string& objFile,
    const std::filesystem::path& configDir) {
    
    // Try multiple path resolutions
    std::vector<std::string> pathsToTry;
    
    // 1. Path as specified in config
    pathsToTry.push_back(objFile);
    
    // 2. Relative to config file directory
    pathsToTry.push_back((configDir / objFile).string());
    
    // 3. Relative to current working directory (thegame directory)
    pathsToTry.push_back((std::filesystem::current_path() / objFile).string());
    
    // 4. Relative to thegame directory (if config is in thegame/)
    std::filesystem::path thegamePath = configDir.parent_path();
    pathsToTry.push_back((thegamePath / objFile).string());
    
    // 5. Relative to build/bin/Release (common location)
    pathsToTry.push_back("../../" + objFile);
    pathsToTry.push_back("../../../" + objFile);
    
    // 6. From thegame root directory
    std::filesystem::path rootPath = std::filesystem::current_path();
    if (rootPath.filename().string() == "thegame") {
        pathsToTry.push_back((rootPath / objFile).string());
    } else {
        // Try going up to find thegame directory
        std::filesystem::path parent = rootPath.parent_path();
        if (parent.filename().string() == "thegame") {
            pathsToTry.push_back((parent / objFile).string());
        }
    }
    
    // 7. From build/bin/Release/graphics (if running from build directory)
    std::filesystem::path buildPath = std::filesystem::current_path();
    if (buildPath.filename().string() == "Release" || buildPath.filename().string() == "Debug") {
        pathsToTry.push_back((buildPath / "graphics" / objFile).string());
    }
    
    // 8. Try with graphics prefix removed (in case we're already in graphics directory)
    std::string objFileNoPrefix = objFile;
    if (objFileNoPrefix.find("graphics/") == 0) {
        objFileNoPrefix = objFileNoPrefix.substr(8); // Remove "graphics/" prefix
        pathsToTry.push_back(objFileNoPrefix);
        pathsToTry.push_back((std::filesystem::current_path() / objFileNoPrefix).string());
    }
    
    LOG_INFO_FMT("CameraSystem", "Trying %zu paths to find model file '%s'", pathsToTry.size(), objFile.c_str());
    
    // Try each path
    for (const auto& tryPath : pathsToTry) {
        try {
            if (std::filesystem::exists(tryPath)) {
                // Try to load the model to verify it's valid
                Model testModel;
                testModel.LoadModel(tryPath);
                if (testModel.GetMeshCount() > 0) {
                    return tryPath;
                }
            }
        } catch (const std::exception& e) {
            // Try next path
            continue;
        } catch (...) {
            // Try next path
            continue;
        }
    }
    
    // Log all tried paths for debugging
    LOG_WARNING_FMT("CameraSystem", "Failed to find model file '%s'. Tried paths:", objFile.c_str());
    for (const auto& path : pathsToTry) {
        LOG_WARNING_FMT("CameraSystem", "  - %s %s", path.c_str(), 
                       std::filesystem::exists(path) ? "(exists)" : "(not found)");
    }
    
    return std::string();
}

void ObjectManager::SetObjectScale(SceneObject* obj, float targetSize) {
    if (!obj || !obj->model || obj->model->GetMeshCount() == 0) {
        return;
    }
    
    // Get bounding box
    float minX, maxX, minY, maxY, minZ, maxZ;
    obj->model->GetBounds(minX, maxX, minY, maxY, minZ, maxZ);
    float baseSizeX = maxX - minX;
    float baseSizeY = maxY - minY;
    float baseSizeZ = maxZ - minZ;
    float baseSize = std::max(baseSizeX, std::max(baseSizeY, baseSizeZ));
    
    if (baseSize > 0.0001f) {
        // Calculate scale factor to make the largest dimension targetSize units
        float scaleFactor = targetSize / baseSize;
        obj->scale = glm::vec3(scaleFactor, scaleFactor, scaleFactor);
    }
}

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

