#include "Logger.h"
#include "AppPaths.h"
#include "GameNetworkConfig.h"
#include "character/CharacterLoader.h"
#include "character/CharacterDatabase.h"
#include "character/CharacterEquipmentSystem.h"
#include "character/ItemCatalog.h"
#include "character/EquipmentRules.h"
#include "character/NameplateComponent.h"
#include "character/CharacterNameplateRenderer.h"
#include "core/ecs/Registry.h"
#include "core/System.h"
#include "render/Window.h"
#include "render/Shader.h"
#include "GameCameraSetup.h"
#include "GameCameraInput.h"
#include "render/Model.h"
#include "render/components/TransformComponent.h"
#include "render/components/MeshComponent.h"
#include "render/components/MaterialComponent.h"
#include "render/components/AnimationComponent.h"
#include "render/systems/RenderSystem.h"
#include "render/systems/AnimationSystem.h"
#include "render/systems/CameraSystem.h"
#include "input/InputSystem.h"
#include "physics/SimplePhysicsWorld.h"
#include "physics/systems/PhysicsSystem.h"
#include "audio/AudioSystem.h"
#include "core/network/NetTypes.h"
#include "network/systems/NetworkSystem.h"
#ifdef PLATFORM_HAS_NUKLEAR
#include "ui/UISystem.h"
#include "ui/MenuStyle.h"
#include "MainMenu.h"
#include "Menu.h"
#include "ui/Minimap.h"
#include <GLFW/glfw3.h>
#endif
#include <memory>
#include <exception>
#include <cstdio>
#include <string>
#include <algorithm>
#include <vector>
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
using namespace Engine::Physics;
using namespace Engine::Physics::Systems;
using namespace Engine::Audio;
using namespace Engine::Platform::Networking::Systems;
using namespace Game::Character;
#ifdef PLATFORM_HAS_NUKLEAR
using namespace Engine::Platform::UI;
using namespace Game::UI;
#endif

int main(int argc, char* argv[]) {
    if (!Game::SetWorkingDirectoryToExecutable()) {
        fprintf(stderr, "Warning: could not set working directory to executable folder.\n");
        fprintf(stderr, "Run from build/bin/<Config>/ or ensure shaders/ is on the search path.\n");
    }

    const Game::GameNetworkConfig networkConfig = Game::GameNetworkConfig::ParseCommandLine(argc, argv);

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
        Game::GameCameraRig cameraRig;
        float aspectRatio = (float)mainWindow.getBufferWidth() / (float)mainWindow.getBufferHeight();
        // Far plane 2000: supports large worlds (100x100+ terrain); near 0.1 avoids z-fighting
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 2000.0f);
        
        LOG_INFO("Main", "Creating ECS registry...");
        Registry registry;

        ItemCatalog itemCatalog;
        EquipmentRules equipmentRules;
        ServerLoadoutDatabase characterDatabase;
        itemCatalog.Load();
        equipmentRules.Load();
        CharacterLoader characterLoader(itemCatalog, equipmentRules, characterDatabase);
        CharacterInstance characterInstance;
        
        LOG_INFO("Main", "Creating character entity...");
        Entity character = registry.CreateEntity();
        auto& transform = registry.AddComponent<TransformComponent>(character.id);
        transform.SetPosition(0.0f, 1.0f, 0.0f);
        transform.SetScale(1.0f);
        
        cameraRig.ConfigureFollowTarget(character.id, registry, 0.1f, 100.0f, 0.5f, 0.5f);
        
        LOG_INFO("Main", "Creating terrain from heightmap...");
        Entity ground = registry.CreateEntity();
        std::unique_ptr<Model> groundModel = std::make_unique<Model>();
        const std::string heightmapPath = "terrain/barren_scar/00.png";
        constexpr float kTerrainSizeX = 100.0f;
        constexpr float kTerrainSizeZ = 100.0f;
        if (groundModel->CreateFromHeightmap(heightmapPath, kTerrainSizeX, kTerrainSizeZ, 10.0f)) {
            LOG_INFO("Main", "Heightmap terrain loaded from barren_scar");
        } else {
            LOG_WARNING("Main", "Heightmap not found, falling back to flat plane");
            groundModel->CreatePlane(100.0f, 100.0f);
        }
        
        auto& groundTransform = registry.AddComponent<TransformComponent>(ground.id);
        groundTransform.SetPosition(0.0f, 0.0f, 0.0f);
        
        auto& groundMesh = registry.AddComponent<MeshComponent>(ground.id);
        groundMesh.model = groundModel.get();
        
        auto& groundMaterial = registry.AddComponent<MaterialComponent>(ground.id);
        groundMaterial.specularIntensity = 0.3f;
        groundMaterial.shininess = 16.0f;
        
        const float spawnX = 5.0f;
        const float spawnZ = 5.0f;
        const float terrainHeight = groundModel->GetHeightAt(spawnX, spawnZ);

        CharacterLoginContext offlineLogin;
        offlineLogin.displayName = networkConfig.displayName;
        offlineLogin.isOnlineLogin = false;
        if (!characterLoader.LoginAndBuild(registry, characterInstance, character.id, offlineLogin)) {
            LOG_ERROR("Main", "Failed to build character from loadout");
            return 1;
        }
        characterLoader.PlaceOnTerrain(registry, character.id, characterInstance, spawnX, spawnZ, terrainHeight);

        const float nameplateHeight = characterInstance.GetHeadHeightOffset() + 0.15f;
        AttachNameplate(registry, character.id, networkConfig.displayName.c_str(), nameplateHeight);

        cameraRig.SetHeightOffset(characterInstance.GetHeadHeightOffset());
        cameraRig.followCamera.Update(0.0f);

        auto& animComp = registry.AddComponent<AnimationComponent>(character.id);
        animComp.groundLevel = terrainHeight;
        
        LOG_INFO("Main", "Initializing systems...");
        SystemManager systemManager;

        auto characterEquipmentSystem = std::make_shared<CharacterEquipmentSystem>();
        systemManager.RegisterSystem(characterEquipmentSystem);
        LOG_INFO("Main", "  CharacterEquipmentSystem registered");

        std::shared_ptr<NetworkSystem> networkSystem;
        bool networkConnected = false;
        bool networkWantsConnect = networkConfig.enabled;
        float networkRetryCooldown = 0.0f;
        constexpr float kNetworkRetrySeconds = 3.0f;
        auto reloadCharacterFromLogin = [&](const CharacterLoginContext& loginContext) {
            if (!characterLoader.LoginAndBuild(registry, characterInstance, character.id, loginContext)) {
                LOG_WARNING("Main", "Failed to refresh character loadout after login");
                return;
            }
            characterLoader.PlaceOnTerrain(
                registry, character.id, characterInstance, spawnX, spawnZ, terrainHeight);
            cameraRig.SetHeightOffset(characterInstance.GetHeadHeightOffset());
            AttachNameplate(
                registry,
                character.id,
                loginContext.displayName.c_str(),
                characterInstance.GetHeadHeightOffset() + 0.15f);
            if (networkSystem) {
                networkSystem->SetRemotePlayerModel(
                    characterInstance.GetBodyModel(), characterInstance.scale);
            }
        };

        if (networkConfig.enabled) {
            networkSystem = std::make_shared<NetworkSystem>();
            networkSystem->SetLocalPlayerEntity(character.id);
            networkSystem->SetRemotePlayerModel(
                characterInstance.GetBodyModel(), characterInstance.scale);
            networkSystem->SetTerrainModel(groundModel.get());
            networkSystem->SetRemotePlayerSetupHandler(
                [&characterInstance](Registry& reg,
                                     EntityID entityId,
                                     Engine::Net::UserId /*userId*/,
                                     const std::string& displayName) {
                    const float remoteNameplateHeight =
                        characterInstance.GetHeadHeightOffset() + 0.15f;
                    AttachNameplate(reg, entityId, displayName.c_str(), remoteNameplateHeight);
                });
            systemManager.RegisterSystem(networkSystem);
            LOG_INFO_FMT("Main",
                         "  NetworkSystem registered (will connect to %s:%u as \"%s\")",
                         networkConfig.serverHost.c_str(),
                         networkConfig.serverPort,
                         networkConfig.displayName.c_str());
        } else {
            LOG_INFO("Main", "  Networking disabled (--offline)");
        }
        
        auto inputSystem = std::make_shared<InputSystem>(&mainWindow);
        inputSystem->SetMovementSpeed(1.4f); // Walking speed (realistic)
        inputSystem->SetCharacterEntity(character.id);
        inputSystem->SetTerrainModel(groundModel.get());
        systemManager.RegisterSystem(inputSystem);
        LOG_INFO("Main", "  InputSystem registered");

        auto cameraSystem = std::make_shared<CameraSystem>();
        systemManager.RegisterSystem(cameraSystem);
        LOG_INFO("Main", "  CameraSystem registered");

        auto physicsWorld = std::make_unique<SimplePhysicsWorld>();
        physicsWorld->SetTerrainSampler([groundModelPtr = groundModel.get()](float x, float z) {
            return groundModelPtr ? groundModelPtr->GetHeightAt(x, z) : 0.0f;
        });
        auto physicsSystem = std::make_shared<PhysicsSystem>(physicsWorld.get());
        systemManager.RegisterSystem(physicsSystem);
        LOG_INFO("Main", "  PhysicsSystem registered");

        auto audioSystem = std::make_shared<AudioSystem>();
        audioSystem->SetCameraSource(cameraRig.source);
        systemManager.RegisterSystem(audioSystem);
        LOG_INFO("Main", "  AudioSystem registered (null backend)");
        
        auto animationSystem = std::make_shared<AnimationSystem>();
        animationSystem->SetEnabled(false);
        systemManager.RegisterSystem(animationSystem);
        LOG_INFO("Main", "  AnimationSystem registered (disabled for OBJ player model)");
        
        auto renderSystem = std::make_shared<RenderSystem>(&shader, cameraRig.source, projection);
        systemManager.RegisterSystem(renderSystem);
        LOG_INFO("Main", "  RenderSystem registered");

        Game::WireGameCamera(cameraRig, inputSystem, cameraSystem, renderSystem, true);

        auto gameCameraInput = std::make_shared<Game::GameCameraInput>(
            &mainWindow, cameraRig.source, character.id);
        systemManager.RegisterSystem(gameCameraInput);
        LOG_INFO("Main", "  GameCameraInput registered (LMB orbit, RMB face camera)");

        auto tryConnectNetwork = [&]() {
            if (!networkSystem || networkConnected || !networkWantsConnect || networkRetryCooldown > 0.0f) {
                return;
            }
            if (networkSystem->ConnectAndLogin(
                    networkConfig.serverHost,
                    networkConfig.serverPort,
                    networkConfig.displayName)) {
                networkConnected = true;
                networkWantsConnect = false;
                LOG_INFO_FMT("Main", "Connected to server as user %u", networkSystem->GetLocalUserId());

                CharacterLoginContext onlineLogin;
                onlineLogin.userId = networkSystem->GetLocalUserId();
                onlineLogin.displayName = networkConfig.displayName;
                onlineLogin.isOnlineLogin = true;
                reloadCharacterFromLogin(onlineLogin);
            } else {
                networkRetryCooldown = kNetworkRetrySeconds;
                LOG_WARNING_FMT("Main",
                                "Failed to connect to %s:%u - start game_server.exe first (retry in %.0fs)",
                                networkConfig.serverHost.c_str(),
                                networkConfig.serverPort,
                                kNetworkRetrySeconds);
            }
        };

#ifdef PLATFORM_HAS_NUKLEAR
        std::shared_ptr<UISystem> uiSystem;
        MenuStyle menuStyle = MenuStyle::Astral();
        MainMenuConfig menuConfig;
        std::unique_ptr<MainMenu> mainMenu = std::make_unique<MainMenu>();
        std::unique_ptr<CharacterMenu> characterMenu = std::make_unique<CharacterMenu>();
        Engine::Platform::UI::Minimap minimap;
        const bool menuBlocksStart = true;
#else
        const bool menuBlocksStart = false;
#endif

#ifdef PLATFORM_HAS_NUKLEAR
        uiSystem = std::make_shared<UISystem>();
        MainMenu::LoadConfig(menuConfig);
        if (menuConfig.minScreenWidth > 0 && menuConfig.minScreenHeight > 0) {
            glfwSetWindowSizeLimits(
                mainWindow.GetGLFWWindow(),
                menuConfig.minScreenWidth,
                menuConfig.minScreenHeight,
                GLFW_DONT_CARE,
                GLFW_DONT_CARE);
        }
        menuStyle.metrics.hudFontSize = menuConfig.textSize;
        if (uiSystem->Initialize(&mainWindow, menuStyle)) {
            if (!mainMenu->Initialize(&menuConfig)) {
                LOG_WARNING("Main", "Main menu button textures failed to load — using text-only PLAY button");
            }
            CharacterMenuConfig characterMenuConfig;
            CharacterMenu::LoadConfig(characterMenuConfig);
            if (!characterMenu->Initialize(&characterMenuConfig)) {
                LOG_WARNING("Main", "Character menu bottom bar failed to load");
            }
            characterMenu->SetPaperdollCharacter(&registry, character.id, &shader, &characterInstance);
            if (!minimap.Initialize(heightmapPath.c_str(),
                                    groundModel.get(),
                                    kTerrainSizeX,
                                    kTerrainSizeZ,
                                    characterMenuConfig.minimap.borderTexturePath.c_str())) {
                LOG_WARNING("Main", "Minimap failed to load terrain image");
            } else {
                minimap.SetReferenceScreen(characterMenuConfig.minScreenWidth,
                                           characterMenuConfig.minScreenHeight);
                minimap.SetSizePixels(characterMenuConfig.minimap.sizePixels);
                minimap.SetMarginPixels(characterMenuConfig.minimap.marginPixels);
                minimap.SetBorderInsetFraction(characterMenuConfig.minimap.borderInsetFraction);
                minimap.SetTrackedEntity(character.id);
                LOG_INFO("Main", "  Minimap initialized (top-right HUD)");
            }
            uiSystem->SetUIBuilder([menu = mainMenu.get(),
                                    characterMenu = characterMenu.get(),
                                    minimap = &minimap,
                                    network = networkSystem.get(),
                                    uiSys = uiSystem.get(),
                                    &registry,
                                    &cameraRig,
                                    &projection](struct nk_context* ctx,
                                                  const MenuStyle& style,
                                                  int screenW,
                                                  int screenH) {
                if (menu->IsVisible()) {
                    menu->Build(ctx, style, *uiSys, screenW, screenH);
                } else {
                    if (menu->IsGameStarted() && !characterMenu->IsVisible()) {
                        characterMenu->Show();
                    }
                    if (characterMenu->IsVisible()) {
                        characterMenu->Build(ctx, style, *uiSys, screenW, screenH);
                    }
                    if (menu->IsGameStarted()) {
                        static thread_local std::vector<EntityID> minimapRemotePlayers;
                        minimapRemotePlayers.clear();
                        if (network) {
                            network->CollectRemotePlayerEntities(minimapRemotePlayers);
                        }
                        minimap->Draw(ctx,
                                      registry,
                                      screenW,
                                      screenH,
                                      minimapRemotePlayers.data(),
                                      minimapRemotePlayers.size());
                    }
                    if (cameraRig.source) {
                        CharacterNameplateRenderer::Draw(ctx,
                                                         registry,
                                                         uiSys->GetFontLibrary(),
                                                         *cameraRig.source,
                                                         projection,
                                                         screenW,
                                                         screenH,
                                                         true);
                    }
                }
                const bool blockInput =
                    menu->BlocksGameInput() || characterMenu->BlocksGameInput();
                uiSys->SetBlockingGameInput(blockInput);
            });
            inputSystem->SetInputBlockQuery([menu = mainMenu.get(), characterMenu = characterMenu.get()]() {
                return menu->BlocksGameInput() || characterMenu->BlocksGameInput();
            });
            gameCameraInput->SetInputBlockQuery([menu = mainMenu.get(), characterMenu = characterMenu.get()]() {
                return menu->BlocksGameInput() || characterMenu->BlocksGameInput();
            });
            systemManager.RegisterSystem(uiSystem);
            LOG_INFO("Main", "  UISystem registered (main menu)");
        } else {
            LOG_WARNING("Main", "UISystem failed to initialize — running without menus");
            mainMenu.reset();
        }
#endif
        
        systemManager.Initialize(registry);
        LOG_INFO("Main", "All systems initialized");

        if (!menuBlocksStart) {
            tryConnectNetwork();
        }
        
        LOG_INFO("Main", "=== Entering game loop ===");
        float lastTime = (float)glfwGetTime();
        uint64_t tickNumber = 0;
        int lastFramebufferWidth = 0;
        int lastFramebufferHeight = 0;
        
        while (!mainWindow.getShouldClose()) {
            float currentTime = (float)glfwGetTime();
            float deltaTime = currentTime - lastTime;
            lastTime = currentTime;
            
            mainWindow.pollEvents();
            if (mainWindow.getShouldClose()) break;

            const int framebufferWidth = mainWindow.getBufferWidth();
            const int framebufferHeight = mainWindow.getBufferHeight();
            if (framebufferWidth > 0 && framebufferHeight > 0 &&
                (framebufferWidth != lastFramebufferWidth ||
                 framebufferHeight != lastFramebufferHeight)) {
                lastFramebufferWidth = framebufferWidth;
                lastFramebufferHeight = framebufferHeight;

                const float resizedAspect =
                    static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight);
                projection = glm::perspective(glm::radians(45.0f), resizedAspect, 0.1f, 2000.0f);
                renderSystem->SetProjection(projection);
            }

            if (networkRetryCooldown > 0.0f) {
                networkRetryCooldown = std::max(0.0f, networkRetryCooldown - deltaTime);
            }

#ifdef PLATFORM_HAS_NUKLEAR
            if (mainMenu && mainMenu->IsGameStarted() && networkWantsConnect && !networkConnected) {
                tryConnectNetwork();
            }
#endif
            
            try {
                systemManager.Update(deltaTime, registry, tickNumber++);
            } catch (const std::exception& e) {
                LOG_ERROR_FMT("Main", "System update error: %s", e.what());
                break;
            }
            
            if (mainWindow.getShouldClose()) break;
            
            mainWindow.swapBuffers();
        }
        
        LOG_INFO("Main", "Shutting down...");
        characterInstance.Clear(registry);
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
