#include "Logger.h"
#include "AppPaths.h"
#include "GameNetworkConfig.h"
#include "character/CharacterLoader.h"
#include "character/CharacterDatabase.h"
#include "character/CharacterEquipmentSystem.h"
#include "character/CharacterStats.h"
#include "character/ItemCatalog.h"
#include "character/EquipmentRules.h"
#include "character/NameplateComponent.h"
#include "character/CharacterNameplateRenderer.h"
#include "ui/CharacterHud.h"
#include "character/CharacterReplication.h"
#include "character/CharacterReplicationComponent.h"
#include "audio/CharacterWalkAudioSystem.h"
#include "systems/character/RemoteLoadoutApplySystem.h"
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
#include "audio/AudioSystem.h"
#include "core/network/NetTypes.h"
#include "core/network/LoadoutReplication.h"
#include "core/network/NetworkPayloads.h"
#include "network/systems/NetworkSystem.h"
#include "network/systems/ReplicationSystem.h"
#ifdef ENGINE_ENABLE_AUDIO
#include "audio/MiniaudioBackend.h"
#endif
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
#include <cstdlib>
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
using namespace Engine::Audio;
using namespace Engine::Platform::Networking::Systems;
using namespace Game::Character;
using namespace Game::Audio;
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

#ifdef PLATFORM_HAS_NUKLEAR
        std::shared_ptr<UISystem> uiSystem = std::make_shared<UISystem>();
        MenuStyle menuStyle = MenuStyle::Astral();
        MainMenuConfig menuConfig;
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
        std::unique_ptr<MainMenu> mainMenu = std::make_unique<MainMenu>();
        std::unique_ptr<CharacterMenu> characterMenu = std::make_unique<CharacterMenu>();
        Engine::Platform::UI::Minimap minimap;
        Game::UI::CharacterHud characterHud;
        const bool menuBlocksStart = true;
        bool bootUiReady = false;
        if (uiSystem->Initialize(&mainWindow, menuStyle)) {
            mainMenu->Initialize(&menuConfig);
            uiSystem->SetUIBuilder([menu = mainMenu.get(), uiSys = uiSystem.get()](
                                       struct nk_context* ctx,
                                       const MenuStyle& style,
                                       int screenW,
                                       int screenH) {
                menu->Build(ctx, style, *uiSys, screenW, screenH);
                uiSys->SetBlockingGameInput(menu->BlocksGameInput());
            });
            bootUiReady = true;
            LOG_INFO("Main", "Boot loading UI ready");
        } else {
            LOG_WARNING("Main", "UISystem failed to initialize — continuing without loading screen");
            mainMenu.reset();
        }

        auto pumpBootLoading = [&](float progress, const char* status) {
            if (!bootUiReady || !mainMenu || !uiSystem) {
                return;
            }
            mainMenu->SetLoadProgress(progress);
            if (status) {
                mainMenu->SetLoadStatus(status);
            }
            mainWindow.pollEvents();
            if (mainWindow.getShouldClose()) {
                return;
            }
            mainWindow.updateFramebufferSize();
            glViewport(0, 0, mainWindow.getBufferWidth(), mainWindow.getBufferHeight());
            glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            uiSystem->PresentFrame();
            mainWindow.swapBuffers();
        };
        pumpBootLoading(0.02f, "Starting...");
#else
        const bool menuBlocksStart = false;
        auto pumpBootLoading = [](float, const char*) {};
#endif

        LOG_INFO("Main", "Loading shaders...");
        pumpBootLoading(0.08f, "Loading shaders...");
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
        pumpBootLoading(0.15f, "Loading character data...");
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
        pumpBootLoading(0.30f, "Loading terrain...");
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
        pumpBootLoading(0.55f, "Loading world...");
        
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

        // Character mesh/loadout is loaded once on PLAY (enter-world), not during boot.
        transform.SetPosition(spawnX, terrainHeight, spawnZ);
        auto& animComp = registry.AddComponent<AnimationComponent>(character.id);
        animComp.groundLevel = terrainHeight;
        cameraRig.SetHeightOffset(1.7f);
        cameraRig.followCamera.Update(0.0f);

        pumpBootLoading(0.78f, "Initializing systems...");
        if (mainWindow.getShouldClose()) {
            return 0;
        }

        std::shared_ptr<NetworkSystem> networkSystem;
        std::shared_ptr<ReplicationSystem> replicationSystem;
        bool characterLoaded = false;

        auto syncReplicationLoadout = [&](EntityID entityId, const CharacterLoginContext& loginContext) {
            CharacterLoadout loadout;
            if (!characterLoader.FetchLoginLoadout(loginContext, loadout)) {
                return;
            }
            CharacterReplicationComponent* replicationComp =
                registry.GetComponent<CharacterReplicationComponent>(entityId);
            if (!replicationComp) {
                replicationComp = &registry.AddComponent<CharacterReplicationComponent>(entityId);
            }
            replicationComp->loadout = loadout;

            // Publish loadout to authoritative server (server owns replication snapshots).
            if (networkSystem && networkSystem->IsLoggedIn()) {
                Engine::Net::GenericDataPayload payload;
                payload.userId = networkSystem->GetLocalUserId();
                payload.tick = 0;
                payload.channel = Engine::Net::kLoadoutChannel;
                const Engine::Net::NetBuffer loadoutBuffer = ToReplicatedLoadout(loadout).Serialize();
                payload.data = loadoutBuffer.Data();
                networkSystem->GetClient().SendGenericData(payload);
            }
        };

        auto loadLocalCharacter = [&](const CharacterLoginContext& loginContext) -> bool {
            if (characterLoaded) {
                return true;
            }
            if (!characterLoader.LoginAndBuild(registry, characterInstance, character.id, loginContext)) {
                LOG_ERROR("Main", "Failed to build character from loadout");
                return false;
            }
            characterInstance.WireAnimationComponent(registry);
            characterLoader.PlaceOnTerrain(
                registry, character.id, characterInstance, spawnX, spawnZ, terrainHeight);
            cameraRig.SetHeightOffset(characterInstance.GetHeadHeightOffset());
            cameraRig.followCamera.Update(0.0f);
            AttachNameplate(
                registry,
                character.id,
                loginContext.displayName.c_str(),
                characterInstance.GetHeadHeightOffset() + 0.15f);
            if (AnimationComponent* anim = registry.GetComponent<AnimationComponent>(character.id)) {
                anim->groundLevel = terrainHeight;
            }
            if (networkSystem) {
                networkSystem->SetRemotePlayerModel(
                    characterInstance.GetBodyModel(), characterInstance.scale);
            }
            syncReplicationLoadout(character.id, loginContext);
            characterLoaded = true;
            LOG_INFO("Main", "Local character loaded");
            return true;
        };
        
        LOG_INFO("Main", "Initializing systems...");
        SystemManager systemManager;

        auto characterEquipmentSystem = std::make_shared<CharacterEquipmentSystem>();
        systemManager.RegisterSystem(characterEquipmentSystem);
        LOG_INFO("Main", "  CharacterEquipmentSystem registered");

        bool networkConnected = false;
        bool networkWantsConnect = networkConfig.enabled;
        float networkRetryCooldown = 0.0f;
        constexpr float kNetworkRetrySeconds = 3.0f;

        if (networkConfig.enabled) {
            replicationSystem = std::make_shared<ReplicationSystem>();
            RegisterLoadoutReplication(*replicationSystem);
            systemManager.RegisterSystem(replicationSystem);
            LOG_INFO("Main", "  ReplicationSystem registered");

            networkSystem = std::make_shared<NetworkSystem>();
            networkSystem->SetAuthoritativeClient(true);
            networkSystem->SetPeerEntityHandler(
                [replicationSystem](Engine::Net::UserId userId, EntityID entityId) {
                    replicationSystem->RegisterReplicatedEntity(userId, entityId);
                });
            // Authoritative server publishes snapshots — clients only apply them.
            // Local transform comes from prediction + TransformState reconcile; applying
            // server EntitySnapshot transforms would stomp scale and fight prediction.
            auto applyReplicationSnapshot =
                [networkSystem, replicationSystem, &registry](const Engine::Net::EntitySnapshot& snapshot) {
                    if (snapshot.entityId == networkSystem->GetLocalUserId()) {
                        Engine::Net::EntitySnapshot filtered = snapshot;
                        filtered.components.erase(
                            std::remove_if(
                                filtered.components.begin(),
                                filtered.components.end(),
                                [](const Engine::Net::ComponentSnapshot& component) {
                                    return component.componentId ==
                                               Engine::Net::kTransformReplicationComponent ||
                                           component.componentId ==
                                               Engine::Net::kAnimationReplicationComponent;
                                }),
                            filtered.components.end());
                        replicationSystem->ApplyEntitySnapshot(filtered, registry);
                        return;
                    }
                    replicationSystem->ApplyEntitySnapshot(snapshot, registry);
                };
            networkSystem->SetEntitySnapshotHandler(applyReplicationSnapshot);
            networkSystem->SetWorldSnapshotHandler(
                [networkSystem, applyReplicationSnapshot, &registry](
                    const Engine::Net::WorldSnapshotPayload& snapshot) {
                    const Engine::Net::UserId localUserId = networkSystem->GetLocalUserId();
                    for (const Engine::Net::EntitySnapshot& entity : snapshot.entities) {
                        if (entity.entityId != localUserId) {
                            networkSystem->EnsureRemoteEntity(entity.entityId, registry);
                        }
                        applyReplicationSnapshot(entity);
                    }
                });
            networkSystem->SetLocalPlayerEntity(character.id);
            // Body model is set after PLAY loads the character.
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

            auto remoteLoadoutSystem = std::make_shared<RemoteLoadoutApplySystem>(
                characterLoader, networkSystem.get(), character.id);
            systemManager.RegisterSystem(remoteLoadoutSystem);
            LOG_INFO("Main", "  RemoteLoadoutApplySystem registered");

            LOG_INFO_FMT("Main",
                         "  NetworkSystem registered (will connect to %s:%u as \"%s\")",
                         networkConfig.serverHost.c_str(),
                         networkConfig.serverPort,
                         networkConfig.displayName.c_str());
        } else {
            LOG_INFO("Main", "  Networking disabled (--offline)");
        }
        
        auto inputSystem = std::make_shared<InputSystem>(&mainWindow);
        CharacterStats characterStats;
        LoadCharacterStats(characterStats);
        inputSystem->SetMovementSpeeds(characterStats.walkSpeed, characterStats.runSpeed);
        inputSystem->SetCharacterEntity(character.id);
        inputSystem->SetTerrainModel(groundModel.get());
        systemManager.RegisterSystem(inputSystem);
        LOG_INFO("Main", "  InputSystem registered");

        if (networkSystem) {
            networkSystem->SetInputBuffer(&inputSystem->GetInputBuffer());
            if (!networkConfig.username.empty()) {
                networkSystem->SetCredentials(networkConfig.username, networkConfig.password);
            }
            if (!networkConfig.authToken.empty()) {
                networkSystem->SetAuthToken(networkConfig.authToken);
            }
            if (networkConfig.useTls) {
                Engine::Platform::Networking::TlsConfig tls;
                tls.enabled = true;
                tls.caCertificatePath = networkConfig.tlsCaPath;
                tls.insecureSkipVerify = networkConfig.tlsInsecure;
                networkSystem->SetTlsConfig(tls);
            }
        }

        auto cameraSystem = std::make_shared<CameraSystem>();
        systemManager.RegisterSystem(cameraSystem);
        LOG_INFO("Main", "  CameraSystem registered");

        auto audioSystem = std::make_shared<AudioSystem>();
#ifdef ENGINE_ENABLE_AUDIO
        audioSystem->SetBackend(std::make_unique<MiniaudioBackend>());
        LOG_INFO("Main", "  AudioSystem registered (miniaudio backend)");
#else
        LOG_INFO("Main", "  AudioSystem registered (null backend)");
#endif
        audioSystem->SetCameraSource(cameraRig.source);
        systemManager.RegisterSystem(audioSystem);

        auto walkAudioSystem =
            std::make_shared<CharacterWalkAudioSystem>(audioSystem.get(), character.id);
        systemManager.RegisterSystem(walkAudioSystem);
        LOG_INFO("Main", "  CharacterWalkAudioSystem registered");
        
        auto animationSystem = std::make_shared<AnimationSystem>();
        systemManager.RegisterSystem(animationSystem);
        LOG_INFO("Main", "  AnimationSystem registered");
        
        auto renderSystem = std::make_shared<RenderSystem>(&shader, cameraRig.source, projection);
        systemManager.RegisterSystem(renderSystem);
        LOG_INFO("Main", "  RenderSystem registered");

        Game::WireGameCamera(cameraRig, inputSystem, cameraSystem, renderSystem, true);

        auto gameCameraInput = std::make_shared<Game::GameCameraInput>(
            &mainWindow, cameraRig.source, character.id);
        systemManager.RegisterSystem(gameCameraInput);
        LOG_INFO("Main", "  GameCameraInput registered (LMB orbit, RMB face camera)");

#ifdef PLATFORM_HAS_NUKLEAR
        auto pumpEnterLoading = [&](float progress, const char* status) {
            if (!bootUiReady || !mainMenu || !uiSystem || !mainMenu->IsEnterWorldLoading()) {
                return;
            }
            mainMenu->SetEnterLoadProgress(progress);
            if (status) {
                mainMenu->SetEnterLoadStatus(status);
            }
            mainWindow.pollEvents();
            if (mainWindow.getShouldClose()) {
                return;
            }
            mainWindow.updateFramebufferSize();
            glViewport(0, 0, mainWindow.getBufferWidth(), mainWindow.getBufferHeight());
            glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            uiSystem->PresentFrame();
            mainWindow.swapBuffers();
        };
#else
        auto pumpEnterLoading = [](float, const char*) {};
#endif

        auto tryConnectNetwork = [&]() {
            if (!networkSystem || networkConnected || !networkWantsConnect || networkRetryCooldown > 0.0f) {
                return;
            }
            pumpEnterLoading(0.20f, "Connecting...");
            if (networkSystem->ConnectAndLogin(
                    networkConfig.serverHost,
                    networkConfig.serverPort,
                    networkConfig.displayName)) {
                networkConnected = true;
                networkWantsConnect = false;
                LOG_INFO_FMT("Main", "Connected to server as user %u", networkSystem->GetLocalUserId());
                pumpEnterLoading(0.55f, "Loading character...");
                if (replicationSystem) {
                    replicationSystem->RegisterReplicatedEntity(networkSystem->GetLocalUserId(), character.id);
                }

                CharacterLoginContext onlineLogin;
                onlineLogin.userId = networkSystem->GetLocalUserId();
                onlineLogin.displayName = networkConfig.displayName;
                onlineLogin.isOnlineLogin = true;
                if (!loadLocalCharacter(onlineLogin)) {
                    pumpEnterLoading(0.55f, "Character load failed");
                    return;
                }
                pumpEnterLoading(1.0f, "Ready");
#ifdef PLATFORM_HAS_NUKLEAR
                if (mainMenu && mainMenu->IsEnterWorldLoading()) {
                    mainMenu->NotifyEnterWorldReady();
                }
#endif
            } else {
                networkRetryCooldown = kNetworkRetrySeconds;
                LOG_WARNING_FMT("Main",
                                "Failed to connect to %s:%u - start game_server.exe first (retry in %.0fs)",
                                networkConfig.serverHost.c_str(),
                                networkConfig.serverPort,
                                kNetworkRetrySeconds);
                pumpEnterLoading(0.35f, "Waiting for server...");
            }
        };

#ifdef PLATFORM_HAS_NUKLEAR
        pumpBootLoading(0.90f, "Loading menus...");
        if (bootUiReady && uiSystem && mainMenu) {
            CharacterMenuConfig characterMenuConfig;
            CharacterMenu::LoadConfig(characterMenuConfig);
            if (!characterMenu->Initialize(&characterMenuConfig)) {
                LOG_WARNING("Main", "Character menu bottom bar failed to load");
            }
            characterMenu->SetPaperdollCharacter(&registry, character.id, &shader, &characterInstance);
            pumpBootLoading(0.94f, "Loading HUD...");
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
            if (!characterHud.Initialize(characterMenuConfig.characterHud)) {
                LOG_WARNING("Main", "Character HUD failed to load");
            } else {
                LOG_INFO("Main", "  Character HUD initialized (top-left)");
            }
            uiSystem->SetUIBuilder([menu = mainMenu.get(),
                                    characterMenu = characterMenu.get(),
                                    minimap = &minimap,
                                    characterHud = &characterHud,
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
                        characterHud->Draw(ctx, screenW, screenH);
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
            cameraRig.obstruction.SetInputBlockedQuery(
                [menu = mainMenu.get(), characterMenu = characterMenu.get()]() {
                    return menu->BlocksGameInput() || characterMenu->BlocksGameInput();
                });
            systemManager.RegisterSystem(uiSystem);
            LOG_INFO("Main", "  UISystem registered (main menu)");
            pumpBootLoading(1.0f, "Ready");
            mainMenu->SetReady();
            // Draw one ready frame so PLAY appears before the first game-loop tick.
            pumpBootLoading(1.0f, "Ready");
            if (const char* autoPlay = std::getenv("AW_AUTO_PLAY");
                autoPlay && autoPlay[0] == '1') {
                mainMenu->BeginEnterWorld();
            }
        } else if (uiSystem) {
            LOG_WARNING("Main", "UISystem failed to initialize — running without menus");
            mainMenu.reset();
        }
#endif
        
        systemManager.Initialize(registry);
        LOG_INFO("Main", "All systems initialized");

        if (!menuBlocksStart) {
            if (networkWantsConnect) {
                tryConnectNetwork();
            } else {
                CharacterLoginContext offlineLogin;
                offlineLogin.displayName = networkConfig.displayName;
                offlineLogin.isOnlineLogin = false;
                if (!loadLocalCharacter(offlineLogin)) {
                    return 1;
                }
            }
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
            if (mainMenu) {
                // PLAY → enter-world loading screen, then connect / finish offline join.
                if (mainMenu->IsEnterWorldLoading()) {
                    if (networkWantsConnect && !networkConnected) {
                        tryConnectNetwork();
                    } else if (!networkWantsConnect || !networkSystem) {
                        // Offline PLAY: load character once, then splash/fade.
                        if (!characterLoaded) {
                            pumpEnterLoading(0.45f, "Loading character...");
                            CharacterLoginContext offlineLogin;
                            offlineLogin.displayName = networkConfig.displayName;
                            offlineLogin.isOnlineLogin = false;
                            if (loadLocalCharacter(offlineLogin)) {
                                pumpEnterLoading(1.0f, "Ready");
                                mainMenu->NotifyEnterWorldReady();
                            } else {
                                mainMenu->SetEnterLoadStatus("Character load failed");
                            }
                        } else {
                            mainMenu->NotifyEnterWorldReady();
                        }
                    }
                }
                mainMenu->Update(deltaTime);
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
