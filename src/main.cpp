#include "Logger.h"
#include "core/ecs/Registry.h"
#include "core/System.h"
#include "render/Window.h"
#include "render/Shader.h"
#include "render/FollowCamera.h"
#include "render/Model.h"
#include "render/Animation.h"
#include "render/components/TransformComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/MaterialComponent.h"
#include "render/components/AnimationComponent.h"
#include "render/systems/RenderSystem.h"
#include "render/systems/AnimationSystem.h"
#include "render/Skeleton.h"
#include "input/InputSystem.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <memory>
#include <exception>
#include <fstream>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

using namespace Engine;
using namespace Engine::ECS;
using namespace Engine::Platform;
using namespace Engine::Platform::Render;
using namespace Engine::Platform::Render::Components;
using namespace Engine::Platform::Render::Systems;
using namespace Engine::Platform::Input::Systems;

int main(int argc, char* argv[]) {
    // Debug: Write to file immediately to verify we're running
    {
        FILE* f = fopen("main_startup.log", "w");
        if (f) {
            fprintf(f, "main() started\n");
            fprintf(f, "Working directory should be wherever we launched from\n");
            fclose(f);
        }
    }
    
    EngineLogging::Logger& logger = EngineLogging::Logger::GetInstance();
    logger.Initialize("debug.log");
    logger.SetMinLevel(EngineLogging::LogLevel::LevelInfo);
    
    LOG_INFO("Main", "=== Game Engine Initialization ===");
    
    try {
        LOG_INFO("Main", "Initializing window...");
        Window mainWindow(1366, 768);
        if (mainWindow.Initialize() != 0) {
            LOG_ERROR("Main", "Failed to initialize window");
            return 1;
        }
        LOG_INFO_FMT("Main", "Window initialized: %dx%d", mainWindow.getBufferWidth(), mainWindow.getBufferHeight());
        
        LOG_INFO("Main", "Loading shaders...");
        Shader shader;
        shader.CreateFromFiles("shaders/shader.vert", "shaders/shader.frag");
        if (!shader.IsValid()) {
            LOG_ERROR("Main", "Shader failed to compile");
            return 1;
        }
        shader.Validate();
        LOG_INFO("Main", "Shaders loaded successfully");
        
        LOG_INFO("Main", "Setting up camera...");
        FollowCamera followCamera;
        float aspectRatio = (float)mainWindow.getBufferWidth() / (float)mainWindow.getBufferHeight();
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
        
        LOG_INFO("Main", "Creating ECS registry...");
        Registry registry;
        
        LOG_INFO("Main", "Creating character entity...");
        Entity character = registry.CreateEntity();
        auto& transform = registry.AddComponent<TransformComponent>(character.id);
        // Initial position - will be adjusted after model loads
        transform.SetPosition(0.0f, 1.0f, 0.0f);
        
        // Character scale will be calculated after model loads based on its actual size
        float characterScale = 1.0f;
        transform.SetScale(characterScale);
        
        followCamera.SetTarget(character.id, &registry);
        followCamera.SetMinDistance(0.1f);
        followCamera.SetMaxDistance(20.0f);
        followCamera.SetRotationSpeed(0.5f);
        followCamera.SetZoomSpeed(0.5f);
        
        LOG_INFO("Main", "Loading character model...");
        std::unique_ptr<Model> characterModel = std::make_unique<Model>();
        std::string animationBasePath = "graphics/Models/character/animation/regular/";
        std::string baseModelPath = "graphics/Models/character/animation/magic/FinalBaseMesh.fbx";
        
        // Try to use FinalBaseMesh.fbx as base model (contains mesh + skeleton, no animation)
        std::string modelPath = baseModelPath;
        std::ifstream test(modelPath);
        bool usingFbxModel = test.good();
        test.close();
        
        // Fallback to Walking.fbx if FinalBaseMesh.fbx doesn't exist
        if (!usingFbxModel) {
            modelPath = animationBasePath + "Walking.fbx";
            test.open(modelPath);
            usingFbxModel = test.good();
            test.close();
            
            if (!usingFbxModel) {
                modelPath = "graphics/Models/character/graphics/male_model.obj";
                LOG_WARNING("Main", "FBX models not found, using OBJ (no skeletal animation)");
            } else {
                LOG_INFO("Main", "Using Walking.fbx as base model (contains animation)");
            }
        } else {
            LOG_INFO("Main", "Using FinalBaseMesh.fbx as base model (recommended)");
        }
        
        try {
            characterModel->LoadModel(modelPath);
            LOG_INFO_FMT("Main", "Character model loaded: %s (%zu meshes)", modelPath.c_str(), characterModel->GetMeshCount());
            
            // Calculate proper scale based on model height
            // Normal human character should be around 1.7-1.8 meters tall
            float modelHeight = characterModel->GetHeight();
            float modelBottomY = characterModel->GetBottomY();
            float targetHeight = 1.8f; // Target height in meters
            
            if (modelHeight > 0.01f && modelHeight > 5.0f) {
                // Model is too large, scale it down
                characterScale = targetHeight / modelHeight;
                transform.SetScale(characterScale);
                LOG_INFO_FMT("Main", "Model is large (%.2f units), scaling down to %.2f meters (scale=%.4f)", 
                    modelHeight, targetHeight, characterScale);
            } else if (modelHeight < 0.5f) {
                // Model is too small, scale it up
                characterScale = targetHeight / modelHeight;
                transform.SetScale(characterScale);
                LOG_INFO_FMT("Main", "Model is small (%.2f units), scaling up to %.2f meters (scale=%.4f)", 
                    modelHeight, targetHeight, characterScale);
            }
            
            // Adjust character Y position so bottom of model sits on ground (Y=0)
            // The model's bottom is at modelBottomY in model space
            // After scaling, we need to position it so: position.y + (modelBottomY * scale) = 0
            // Therefore: position.y = -modelBottomY * scale
            float adjustedY = -modelBottomY * characterScale;
            if (adjustedY < -10.0f || adjustedY > 10.0f || !std::isfinite(adjustedY)) {
                adjustedY = 0.0f; // Fallback to ground level
                LOG_WARNING_FMT("Main", "Calculated Y position (%.2f) out of range, using 0.0", adjustedY);
            }
            
            // Position character on ground
            transform.SetPosition(transform.position.x, adjustedY, transform.position.z);
            transform.SetScale(characterScale);
            
            // Calculate head height offset for camera
            float headHeightOffset = 1.7f;
            if (characterModel->GetHeight() > 0.01f) {
                float modelTopY = characterModel->GetTopY();
                headHeightOffset = (modelTopY - modelBottomY) * characterScale;
                if (headHeightOffset < 0.1f || headHeightOffset > 50.0f || !std::isfinite(headHeightOffset)) {
                    headHeightOffset = 1.7f;
                }
            }
            followCamera.SetHeightOffset(headHeightOffset);
            followCamera.Update(0.0f);
            
            LOG_INFO_FMT("Main", "Character setup: position(%.2f, %.2f, %.2f), scale=%.4f, modelHeight=%.2f", 
                transform.position.x, transform.position.y, transform.position.z, 
                characterScale, modelHeight);
            glm::vec3 camPos = followCamera.GetCameraPosition();
            glm::vec3 camTarget = followCamera.GetCameraTarget();
            LOG_INFO_FMT("Main", "Camera setup: position(%.2f, %.2f, %.2f), target(%.2f, %.2f, %.2f), distance=%.2f", 
                camPos.x, camPos.y, camPos.z, camTarget.x, camTarget.y, camTarget.z, followCamera.GetDistance());
        } catch (const std::exception& e) {
            LOG_ERROR_FMT("Main", "Failed to load character model: %s", e.what());
        }
        
        auto& mesh = registry.AddComponent<MeshComponent>(character.id);
        if (characterModel) {
            mesh.model = characterModel.get();
        }
        
        auto& material = registry.AddComponent<MaterialComponent>(character.id);
        material.specularIntensity = 0.5f;
        material.shininess = 32.0f;
        
        auto& animComp = registry.AddComponent<AnimationComponent>(character.id);
        animComp.groundLevel = transform.position.y;
        animComp.currentState = AnimationComponent::AnimationState::Idle;
        
        LOG_INFO("Main", "Loading animations...");
        std::unique_ptr<Animation> walkingAnimation = std::make_unique<Animation>();
        std::unique_ptr<Animation> runningAnimation = std::make_unique<Animation>();
        std::unique_ptr<Animation> jumpAnimation = std::make_unique<Animation>();
        
        if (walkingAnimation->LoadAnimation(animationBasePath + "Walking.fbx")) {
            animComp.walkingAnimation = walkingAnimation.get();
            LOG_INFO("Main", "  Walking animation loaded");
        }
        if (runningAnimation->LoadAnimation(animationBasePath + "Running.fbx")) {
            animComp.runningAnimation = runningAnimation.get();
            LOG_INFO("Main", "  Running animation loaded");
        }
        if (jumpAnimation->LoadAnimation(animationBasePath + "Jump.fbx")) {
            animComp.jumpAnimation = jumpAnimation.get();
            LOG_INFO("Main", "  Jump animation loaded");
        }
        
        LOG_INFO("Main", "Loading skeleton...");
        std::unique_ptr<Skeleton> characterSkeleton = std::make_unique<Skeleton>();
        // Use the same file as the model for skeleton (ensures bone names match)
        std::string skeletonPath = modelPath;
        
        Assimp::Importer importer;
        const aiScene* skeletonScene = importer.ReadFile(
            skeletonPath.c_str(),
            aiProcess_Triangulate | aiProcess_JoinIdenticalVertices
        );
        
        if (skeletonScene && !(skeletonScene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) && skeletonScene->mRootNode) {
            if (characterSkeleton->LoadSkeleton(skeletonScene)) {
                animComp.skeleton = characterSkeleton.get();
                characterSkeleton->SetMatrixFormulaMode(0);
                LOG_INFO_FMT("Main", "Skeleton loaded from %s: %zu bones", skeletonPath.c_str(), characterSkeleton->GetBoneCount());
            } else {
                LOG_WARNING("Main", "Failed to load skeleton from model file");
            }
        } else {
            LOG_WARNING("Main", "Failed to load scene for skeleton extraction");
        }
        
        LOG_INFO("Main", "Creating ground plane...");
        Entity ground = registry.CreateEntity();
        std::unique_ptr<Model> groundModel = std::make_unique<Model>();
        groundModel->CreatePlane(20.0f, 20.0f);
        
        auto& groundTransform = registry.AddComponent<TransformComponent>(ground.id);
        groundTransform.SetPosition(0.0f, 0.0f, 0.0f);
        
        auto& groundMesh = registry.AddComponent<MeshComponent>(ground.id);
        groundMesh.model = groundModel.get();
        
        auto& groundMaterial = registry.AddComponent<MaterialComponent>(ground.id);
        groundMaterial.specularIntensity = 0.3f;
        groundMaterial.shininess = 16.0f;
        
        LOG_INFO("Main", "Initializing systems...");
        SystemManager systemManager;
        
        auto inputSystem = std::make_shared<InputSystem>(&mainWindow);
        inputSystem->SetFollowCamera(&followCamera);
        inputSystem->SetMovementSpeed(1.4f); // Walking speed (realistic)
        inputSystem->SetCharacterEntity(character.id);
        systemManager.RegisterSystem(inputSystem);
        LOG_INFO("Main", "  InputSystem registered");
        
        auto animationSystem = std::make_shared<AnimationSystem>();
        systemManager.RegisterSystem(animationSystem);
        LOG_INFO("Main", "  AnimationSystem registered");
        
        auto renderSystem = std::make_shared<RenderSystem>(&shader, &followCamera, projection);
        systemManager.RegisterSystem(renderSystem);
        LOG_INFO("Main", "  RenderSystem registered");
        
        systemManager.Initialize(registry);
        LOG_INFO("Main", "All systems initialized");
        
        LOG_INFO("Main", "=== Entering game loop ===");
        float lastTime = (float)glfwGetTime();
        
        while (!mainWindow.getShouldClose()) {
            float currentTime = (float)glfwGetTime();
            float deltaTime = currentTime - lastTime;
            lastTime = currentTime;
            
            mainWindow.pollEvents();
            if (mainWindow.getShouldClose()) break;
            
            try {
                systemManager.Update(deltaTime, registry, 0);
            } catch (const std::exception& e) {
                LOG_ERROR_FMT("Main", "System update error: %s", e.what());
                break;
            }
            
            if (mainWindow.getShouldClose()) break;
            
            mainWindow.swapBuffers();
        }
        
        LOG_INFO("Main", "Shutting down...");
        if (characterModel) {
            characterModel->ClearModel();
        }
        shader.ClearShader();
        
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("Main", "Fatal error: %s", e.what());
        logger.Shutdown();
        return 1;
    } catch (...) {
        LOG_ERROR("Main", "Fatal error: Unknown exception");
        logger.Shutdown();
        return 1;
    }
    
    logger.Shutdown();
    return 0;
}
