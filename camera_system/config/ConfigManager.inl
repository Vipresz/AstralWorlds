// Template implementations for ConfigManager
// This file is included by ConfigManager.h

#include "../CameraSystemEditor.h"  // For SceneObject full definition
#include "../GlobalConstants.h"
#include "../../engine/platform/Logger.h"
#include <algorithm>
#include <filesystem>

using EngineLogging::Logger;
using namespace Engine::Editors::CameraSystem::Constants;

namespace Engine {
namespace Editors {
namespace CameraSystem {

template<typename CameraFrameType>
void ConfigManager::FillCameraKeyframes(
    int cameraIndex,
    const std::vector<CameraKeyframe>& sortedKeyframes,
    int totalFrames,
    CameraFrameType frames[MAX_CAMERAS][MAX_FRAMES],
    int frameCounts[MAX_CAMERAS]
) {
    if (sortedKeyframes.empty() || cameraIndex >= MAX_CAMERAS) {
        return;
    }
    
    const auto& firstKeyframe = sortedKeyframes[0];
    int firstFrame = std::min(firstKeyframe.frame, MAX_FRAMES - 1);
    
    for (int fillFrame = 0; fillFrame < firstFrame && fillFrame < MAX_FRAMES; fillFrame++) {
        frames[cameraIndex][fillFrame].tick = fillFrame;
        frames[cameraIndex][fillFrame].position = firstKeyframe.position;
        frames[cameraIndex][fillFrame].orientation = firstKeyframe.direction;
    }
    
    if (firstFrame < MAX_FRAMES) {
        frames[cameraIndex][firstFrame].tick = firstFrame;
        frames[cameraIndex][firstFrame].position = firstKeyframe.position;
        frames[cameraIndex][firstFrame].orientation = firstKeyframe.direction;
    }
    
    int lastFilledFrame = firstFrame;
    
    for (size_t keyframeIndex = 1; keyframeIndex < sortedKeyframes.size(); keyframeIndex++) {
        const auto& kf = sortedKeyframes[keyframeIndex];
        int targetFrame = kf.frame;
        
        if (targetFrame >= MAX_FRAMES) {
            continue;
        }
        
        const auto& previousKf = sortedKeyframes[keyframeIndex - 1];
        for (int fillFrame = lastFilledFrame + 1; fillFrame < targetFrame && fillFrame < MAX_FRAMES; fillFrame++) {
            frames[cameraIndex][fillFrame].tick = fillFrame;
            frames[cameraIndex][fillFrame].position = previousKf.position;
            frames[cameraIndex][fillFrame].orientation = previousKf.direction;
        }
        
        if (targetFrame < MAX_FRAMES) {
            frames[cameraIndex][targetFrame].tick = targetFrame;
            frames[cameraIndex][targetFrame].position = kf.position;
            frames[cameraIndex][targetFrame].orientation = kf.direction;
            lastFilledFrame = targetFrame;
        }
    }
    
    const auto& lastKf = sortedKeyframes.back();
    for (int fillFrame = lastFilledFrame + 1; fillFrame < totalFrames && fillFrame < MAX_FRAMES; fillFrame++) {
        frames[cameraIndex][fillFrame].tick = fillFrame;
        frames[cameraIndex][fillFrame].position = lastKf.position;
        frames[cameraIndex][fillFrame].orientation = lastKf.direction;
    }
    
    frameCounts[cameraIndex] = totalFrames;
}

template<typename CameraFrameType>
void ConfigManager::LoadAnimatedCameras(
    const SystemConfig& config,
    std::vector<Camera>& cameras,
    CameraFrameType frames[MAX_CAMERAS][MAX_FRAMES],
    int frameCounts[MAX_CAMERAS],
    int& numCameras,
    int& totalFrames,
    int& currentFrame,
    bool& isPlaying,
    glm::vec3& freeCameraPos,
    float& freeCameraYaw,
    float& freeCameraPitch,
    CameraSystem& cameraSystem
) {
    totalFrames = std::min(config.totalFrames, MAX_FRAMES);
    currentFrame = 0;
    isPlaying = false;
    cameras.clear();
    
    int cameraIndex = 0;
    for (const auto& animCam : config.animatedCameras) {
        if (cameraIndex >= MAX_CAMERAS) {
            LOG_WARNING_FMT("CameraSystem", "Too many cameras, max is %d. Skipping camera '%s'", 
                           MAX_CAMERAS, animCam.name.c_str());
            break;
        }
        
        if (animCam.keyframes.empty()) {
            continue;
        }
        
        std::vector<CameraKeyframe> sortedKeyframes = animCam.keyframes;
        std::sort(sortedKeyframes.begin(), sortedKeyframes.end(), 
                 [](const CameraKeyframe& a, const CameraKeyframe& b) {
                     return a.frame < b.frame;
                 });
        
        FillCameraKeyframes<CameraFrameType>(cameraIndex, sortedKeyframes, totalFrames, frames, frameCounts);
        
        if (frameCounts[cameraIndex] > 0) {
            const auto& firstCamKeyframe = sortedKeyframes[0];
            glm::vec3 target = firstCamKeyframe.position + firstCamKeyframe.direction;
            cameraSystem.AddCamera(animCam.name, firstCamKeyframe.position, target, animCam.canMove, cameras);
            
            if (!animCam.canMove && cameraIndex == 0) {
                cameraSystem.AddCamera("Free Camera", firstCamKeyframe.position, target, true, cameras);
                freeCameraPos = firstCamKeyframe.position;
                glm::vec3 direction = glm::normalize(target - firstCamKeyframe.position);
                CameraSystem::CalculateYawPitchFromDirection(direction, freeCameraYaw, freeCameraPitch);
            }
        }
        
        cameraIndex++;
    }
    
    numCameras = cameraIndex;
    
    if (cameras.empty()) {
        glm::vec3 freeCamPos(10.0f, 10.0f, 5.0f);
        glm::vec3 target(0.0f, 0.0f, 0.0f);
        glm::vec3 direction = glm::normalize(target - freeCamPos);
        CameraSystem::CalculateYawPitchFromDirection(direction, freeCameraYaw, freeCameraPitch);
        cameraSystem.AddCamera("Free Camera", freeCamPos, target, true, cameras);
        if (!cameras.empty()) {
            cameraSystem.SetActiveCameraIndex(0);
            freeCameraPos = freeCamPos;
        }
    } else {
        if (cameraSystem.GetActiveCameraIndex() >= cameras.size()) {
            cameraSystem.SetActiveCameraIndex(0);
        }
        
        cameraSystem.EnsureFreeCameraExists(cameras, freeCameraPos, freeCameraYaw, freeCameraPitch);
        
        for (size_t i = 0; i < cameras.size(); i++) {
            if (cameras[i].canMove) {
                cameraSystem.SetActiveCameraIndex(i);
                break;
            }
        }
        
        size_t activeIdx = cameraSystem.GetActiveCameraIndex();
        if (cameras.size() > 0 && cameras[activeIdx].canMove) {
            freeCameraPos = cameras[activeIdx].position;
            size_t anchorIdx = cameraSystem.FindAnchorCameraIndex(cameras);
            if (anchorIdx < cameras.size()) {
                const Camera& anchorCam = cameras[anchorIdx];
                glm::vec3 direction = glm::normalize(anchorCam.target - anchorCam.position);
                CameraSystem::CalculateYawPitchFromDirection(direction, freeCameraYaw, freeCameraPitch);
            }
        }
    }
}

template<typename ObjectFrameType>
void ConfigManager::FillObjectKeyframes(
    size_t objectIndex,
    const std::vector<ObjectKeyframe>& sortedKeyframes,
    int totalFrames,
    ObjectFrameType frames[MAX_OBJECTS][MAX_FRAMES],
    int frameCounts[MAX_OBJECTS],
    bool useFrames[MAX_OBJECTS],
    std::vector<std::unique_ptr<SceneObject>>& objects
) {
    if (sortedKeyframes.empty() || objectIndex >= static_cast<size_t>(MAX_OBJECTS)) {
        return;
    }
    
    glm::vec3 lastKeyframePosition = sortedKeyframes[0].position;
    for (const auto& kf : sortedKeyframes) {
        if (kf.frame < totalFrames && kf.frame < MAX_FRAMES) {
            lastKeyframePosition = kf.position;
        }
    }
    
    for (int fillFrame = 0; fillFrame < totalFrames && fillFrame < MAX_FRAMES; fillFrame++) {
        frames[objectIndex][fillFrame].frame = fillFrame;
        frames[objectIndex][fillFrame].position = lastKeyframePosition;
    }
    
    int lastKeyframeFrame = -1;
    glm::vec3 lastKeyframePos = lastKeyframePosition;
    
    for (const auto& kf : sortedKeyframes) {
        int targetFrame = kf.frame;
        if (targetFrame < 0 || targetFrame >= totalFrames || targetFrame >= MAX_FRAMES) {
            continue;
        }
        
        if (lastKeyframeFrame >= 0) {
            for (int fillFrame = lastKeyframeFrame + 1; fillFrame < targetFrame && fillFrame < totalFrames && fillFrame < MAX_FRAMES; fillFrame++) {
                frames[objectIndex][fillFrame].frame = fillFrame;
                frames[objectIndex][fillFrame].position = lastKeyframePos;
            }
        }
        
        frames[objectIndex][targetFrame].frame = targetFrame;
        frames[objectIndex][targetFrame].position = kf.position;
        
        lastKeyframeFrame = targetFrame;
        lastKeyframePos = kf.position;
    }
    
    if (lastKeyframeFrame >= 0) {
        for (int fillFrame = lastKeyframeFrame + 1; fillFrame < totalFrames && fillFrame < MAX_FRAMES; fillFrame++) {
            frames[objectIndex][fillFrame].frame = fillFrame;
            frames[objectIndex][fillFrame].position = lastKeyframePos;
        }
    }
    
    frameCounts[objectIndex] = totalFrames;
    useFrames[objectIndex] = true;
    
    if (objectIndex < objects.size() && objects[objectIndex] && !sortedKeyframes.empty()) {
        objects[objectIndex]->position = sortedKeyframes[0].position;
    }
}

template<typename ObjectFrameType>
void ConfigManager::LoadObjects(
    const SystemConfig& config,
    const std::string& configFile,
    ObjectManager& objectManager,
    std::vector<std::unique_ptr<SceneObject>>& objects,
    std::vector<std::vector<PathPoint>>& objectPaths,
    std::vector<float>& objectStartTimes,
    ObjectFrameType objectFrames[MAX_OBJECTS][MAX_FRAMES],
    int objectFrameCounts[MAX_OBJECTS],
    bool objectsUseFrames[MAX_OBJECTS],
    int totalFrames,
    int& numCameras
) {
    LOG_INFO_FMT("CameraSystem", "Loading %zu objects from config", config.objects.size());
    size_t objectsLoaded = 0;
    size_t objectsWithModels = 0;
    
    std::filesystem::path configPath(configFile);
    std::filesystem::path configDir = configPath.parent_path();
    
    for (const auto& objConfig : config.objects) {
        LOG_INFO_FMT("CameraSystem", "Loading object '%s' with obj_file='%s' (keyframes: %zu)", 
                    objConfig.name.c_str(), objConfig.objFile.c_str(), objConfig.keyframes.size());
        
        auto obj = objectManager.LoadObject(objConfig, configDir, OBJECT_SIZE_DEFAULT);
        if (!obj) {
            LOG_ERROR_FMT("CameraSystem", "Failed to load object '%s'", objConfig.name.c_str());
            continue;
        }
        
        size_t objectIndex = objects.size();
        SceneObject* objPtr = obj.get();
        objects.push_back(std::move(obj));
        
        objectsLoaded++;
        if (objPtr->model && objPtr->model->GetMeshCount() > 0) {
            objectsWithModels++;
        }
        
        if (!objConfig.keyframes.empty() && totalFrames > 0 && objectIndex < static_cast<size_t>(MAX_OBJECTS)) {
            std::vector<ObjectKeyframe> sortedKeyframes = objConfig.keyframes;
            std::sort(sortedKeyframes.begin(), sortedKeyframes.end(), 
                     [](const ObjectKeyframe& a, const ObjectKeyframe& b) {
                         return a.frame < b.frame;
                     });
            
            FillObjectKeyframes<ObjectFrameType>(objectIndex, sortedKeyframes, totalFrames, objectFrames, objectFrameCounts, objectsUseFrames, objects);
            
            LOG_INFO_FMT("CameraSystem", "Object '%s': Filled %d frames (keyframes: %zu, first at frame %d, last at frame %d)", 
                       objConfig.name.c_str(), totalFrames, sortedKeyframes.size(), 
                       sortedKeyframes.front().frame, sortedKeyframes.back().frame);
        } else {
            objectPaths.push_back(objConfig.path);
            objectStartTimes.push_back(objConfig.startTime);
            if (objectIndex < static_cast<size_t>(MAX_OBJECTS)) {
                objectsUseFrames[objectIndex] = false;
            }
        }
    }
    
    LOG_INFO_FMT("CameraSystem", "Config loaded: %zu objects loaded (%zu with models), %d cameras, %zu lights, %d total frames", 
                objectsLoaded, objectsWithModels, numCameras, config.lights.size(), totalFrames);
    
    for (size_t i = 0; i < objects.size(); i++) {
        if (objects[i]) {
            bool hasModel = (objects[i]->model != nullptr && objects[i]->model->GetMeshCount() > 0);
            LOG_INFO_FMT("CameraSystem", "  Object[%zu]: '%s' %s at (%.2f, %.2f, %.2f)", 
                        i, objects[i]->name.c_str(),
                        hasModel ? "HAS MODEL" : "NO MODEL",
                        objects[i]->position.x, objects[i]->position.y, objects[i]->position.z);
        }
    }
}

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

