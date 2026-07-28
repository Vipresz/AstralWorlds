// End-to-end game stack test: authoritative server + NetworkSystem + CharacterLoader.
//
// Fast CI (default):
//   GameTests --gtest_filter=GameNetworkE2ETest.*
//
// Multi-client stress (manual / nightly):
//   set GAME_RUN_E2E_STRESS=1
//   set GAME_E2E_STRESS_CLIENTS=16
//   GameTests --gtest_filter=GameNetworkE2ETest.MultiClientLatencyStress

#include <gtest/gtest.h>

#include "GameTestSupport.h"
#include "Logger.h"
#include "TestHelpers.h"
#include "TestPort.h"
#include "character/CharacterBodyAssetCache.h"
#include "character/CharacterDatabase.h"
#include "character/CharacterInstance.h"
#include "character/CharacterLoader.h"
#include "character/CharacterReplication.h"
#include "character/CharacterStats.h"
#include "character/EquipmentRules.h"
#include "character/ItemCatalog.h"
#include "core/ecs/Registry.h"
#include "core/network/JoinAdmission.h"
#include "core/network/MovementCommand.h"
#include "core/network/NetworkPayloads.h"
#include "core/network/ScalePolicyConfig.h"
#include "core/network/ZoneScaleConfig.h"
#include "network/AuthoritativeGameSimulator.h"
#include "network/NetServer.h"
#include "network/systems/NetworkSystem.h"
#include "network/systems/ReplicationSystem.h"
#include "render/components/TransformComponent.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using Engine::ECS::Registry;
using Engine::Net::MovementAckPayload;
using Engine::Platform::Networking::AuthoritativeGameSimulator;
using Engine::Platform::Networking::NetServer;
using Engine::Platform::Networking::Systems::NetworkSystem;
using Engine::Platform::Networking::Systems::ReplicationSystem;
using Game::Character::CharacterBodyAssetCache;
using Game::Character::CharacterInstance;
using Game::Character::CharacterLoader;
using Game::Character::CharacterLoginContext;
using Game::Character::EquipmentRules;
using Game::Character::ItemCatalog;
using Game::Character::ItemDefinition;
using Game::Character::RegisterLoadoutReplication;
using Game::Character::ServerLoadoutDatabase;
using Engine::Platform::Render::Components::TransformComponent;

namespace {

constexpr const char* kHost = "127.0.0.1";
constexpr uint64_t kMovementTickBase = 5'000'000;

bool E2eStressEnabled() {
    const char* flag = std::getenv("GAME_RUN_E2E_STRESS");
    return flag && flag[0] == '1' && flag[1] == '\0';
}

int E2eStressClientCount() {
    const char* value = std::getenv("GAME_E2E_STRESS_CLIENTS");
    if (!value || value[0] == '\0') {
        return 8;
    }
    const int parsed = std::atoi(value);
    return parsed > 0 ? parsed : 8;
}

bool StressProfileEnabled() {
    const char* flag = std::getenv("GAME_E2E_STRESS_PROFILE");
    return flag && flag[0] == '1' && flag[1] == '\0';
}

int StressPlayerCount() {
    const char* value = std::getenv("GAME_E2E_PLAYER_COUNT");
    if (!value || value[0] == '\0') {
        return 64;
    }
    const int parsed = std::atoi(value);
    return parsed > 0 ? parsed : 64;
}

std::string ResolveGameStressScaleConfigPath() {
    if (const char* envPath = std::getenv("GAME_E2E_SCALE_CONFIG")) {
        if (envPath[0] != '\0') {
            return envPath;
        }
    }

#ifdef GAME_SOURCE_DIR
    {
        const std::string bundled =
            std::string(GAME_SOURCE_DIR) + "/RenderingEngine/config/join_load_stress_scale.json";
        std::ifstream file(bundled);
        if (file.good()) {
            return bundled;
        }
    }
#endif

    const char* candidates[] = {
        "config/join_load_stress_scale.json",
        "RenderingEngine/config/join_load_stress_scale.json",
        "../RenderingEngine/config/join_load_stress_scale.json",
    };
    for (const char* candidate : candidates) {
        std::ifstream file(candidate);
        if (file.good()) {
            return candidate;
        }
    }
    return {};
}

int JoinRetryCountForPlayers(int playerCount) {
    if (playerCount >= 512) {
        return 8;
    }
    if (playerCount >= 128) {
        return 5;
    }
    return 3;
}

struct SharedCharacterAssets {
    ItemCatalog catalog;
    EquipmentRules rules;
    ServerLoadoutDatabase database;
};

SharedCharacterAssets& GetSharedCharacterAssets() {
    static SharedCharacterAssets assets;
    static std::once_flag once;
    std::call_once(once, []() {
        const std::string catalogPath = Game::Tests::RequireAsset("tests/fixtures/e2e_items.json");
        if (!assets.catalog.Load(catalogPath.c_str())) {
            assets.catalog.Load(Game::Tests::RequireAsset("character/items.json").c_str());
        }
        assets.rules.Load(Game::Tests::RequireAsset("character/equipment_rules.json").c_str());
    });
    return assets;
}

void PreloadSharedCharacterAssets() {
    SharedCharacterAssets& assets = GetSharedCharacterAssets();
    const ItemDefinition* defaultBody = assets.catalog.DefaultBody();
    if (defaultBody && !defaultBody->modelPath.empty()) {
        CharacterBodyAssetCache::Instance().GetOrLoad(defaultBody->id, defaultBody->modelPath);
    }
}

bool ConnectAndLoginWithRetries(NetworkSystem& networkSystem,
                                uint16_t tcpPort,
                                uint16_t udpPort,
                                const std::string& displayName,
                                int joinRetryCount,
                                int playerIndex = 0,
                                int playerCount = 1) {
    Engine::Net::SleepJoinStagger(playerIndex, playerCount);
    const int attempts = (std::max)(joinRetryCount, 1);
    for (int attempt = 0; attempt < attempts; ++attempt) {
        if (networkSystem.ConnectAndLogin(
                kHost, tcpPort, displayName, Engine::Net::kInvalidUserId, udpPort)) {
            return true;
        }
        networkSystem.Disconnect();
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    return false;
}

void ApplyStressScaleConfig(NetServer& server) {
    const std::string scaleConfigPath = ResolveGameStressScaleConfigPath();
    if (scaleConfigPath.empty()) {
        return;
    }

    Engine::Net::ZoneScaleConfig zoneConfig;
    if (!Engine::Net::ZoneScaleConfig::TryLoadFromFile(scaleConfigPath, zoneConfig)) {
        return;
    }

    server.SetShardConfig(zoneConfig.shard);
    server.SetScalePolicyConfig(zoneConfig.scalePolicy);
    server.ApplyZoneScaleConfig(zoneConfig);
}

class AuthoritativeGameServerFixture {
public:
    explicit AuthoritativeGameServerFixture(bool applyStressScale = false) {
        EngineLogging::Logger::GetInstance().Initialize("game_network_e2e.log");
        if (!Game::Tests::SetWorkingDirectoryToTestRuntime()) {
            throw std::runtime_error("failed to set test runtime working directory");
        }

        m_tcpPort = Engine::Tests::AllocateEphemeralTestPort(17);
        m_udpPort = static_cast<uint16_t>(m_tcpPort + 1);

        m_simulator = std::make_unique<AuthoritativeGameSimulator>(m_server);
        m_simulator->SetSpawnPosition(glm::vec3(5.0f, 0.0f, 5.0f));
        m_simulator->SetTerrainSampler([](float /*x*/, float /*z*/) { return 0.0f; });

        Game::Character::CharacterStats stats;
        Game::Character::LoadCharacterStats(stats);
        m_simulator->SetMovementSpeeds(stats.walkSpeed, stats.runSpeed);

        m_server.SetAuthoritativeMode(true);
        m_server.SetClientLoggedInCallback([this](Engine::Net::UserId userId, const std::string& name) {
            if (m_simulator) {
                m_simulator->OnClientLoggedIn(userId, name);
            }
        });
        m_server.SetClientDisconnectedCallback([this](Engine::Net::UserId userId) {
            if (m_simulator) {
                m_simulator->OnClientDisconnected(userId);
            }
        });
        m_server.SetGenericDataInterceptor([this](Engine::Net::UserId userId,
                                                  Engine::Net::GenericDataPayload& payload) {
            return m_simulator && m_simulator->HandleGenericData(userId, payload);
        });
        m_server.SetMovementCommandInterceptor([this](Engine::Net::UserId userId,
                                                        Engine::Net::MovementCommand& command) {
            return m_simulator && m_simulator->HandleMovementCommand(userId, command);
        });

        if (applyStressScale) {
            ApplyStressScaleConfig(m_server);
        }

        if (!m_server.Start(kHost, m_tcpPort, m_udpPort)) {
            throw std::runtime_error("NetServer failed to start");
        }

        m_tickThread = std::thread([this]() { TickLoop(); });
    }

    ~AuthoritativeGameServerFixture() {
        m_running = false;
        if (m_tickThread.joinable()) {
            m_tickThread.join();
        }
        m_server.Stop();
    }

    void SetStressRateLimits(int playerCount) {
        const float loginBurst = static_cast<float>((std::max)(playerCount * 2, 256));
        m_server.SetLoginRateLimit(10000.0f, loginBurst);
        m_server.SetMovementRateLimit(10000.0f, 500.0f);
    }

    uint16_t TcpPort() const { return m_tcpPort; }
    uint16_t UdpPort() const { return m_udpPort; }

private:
    void TickLoop() {
        auto lastTick = std::chrono::steady_clock::now();
        constexpr float kTickSeconds = 1.0f / 60.0f;
        while (m_running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            const auto now = std::chrono::steady_clock::now();
            const float elapsed =
                std::chrono::duration<float>(now - lastTick).count();
            if (elapsed < kTickSeconds) {
                continue;
            }
            lastTick = now;
            if (m_simulator) {
                m_simulator->Update(elapsed);
            }
        }
    }

    NetServer m_server;
    std::unique_ptr<AuthoritativeGameSimulator> m_simulator;
    uint16_t m_tcpPort = 0;
    uint16_t m_udpPort = 0;
    std::atomic<bool> m_running{true};
    std::thread m_tickThread;
};

struct ClientSessionResult {
    bool connected = false;
    bool characterLoaded = false;
    double connectMs = 0.0;
    double characterLoadMs = 0.0;
    std::vector<double> movementRttMs;
    std::string error;
};

struct StressClientHandle {
    Registry registry;
    Engine::EntityID playerEntity = 0;
    std::shared_ptr<ReplicationSystem> replicationSystem;
    std::shared_ptr<NetworkSystem> networkSystem;
    std::unique_ptr<CharacterInstance> characterInstance;
};

void SetupStressClientNetworkStack(StressClientHandle& handle) {
    handle.playerEntity = handle.registry.CreateEntity().id;
    handle.registry.AddComponent<TransformComponent>(handle.playerEntity);

    handle.replicationSystem = std::make_shared<ReplicationSystem>();
    RegisterLoadoutReplication(*handle.replicationSystem);

    handle.networkSystem = std::make_shared<NetworkSystem>();
    handle.networkSystem->SetAuthoritativeClient(true);
    handle.networkSystem->SetLocalPlayerEntity(handle.playerEntity);
    handle.networkSystem->SetPeerEntityHandler(
        [replicationSystem = handle.replicationSystem](Engine::Net::UserId userId, Engine::EntityID entityId) {
            replicationSystem->RegisterReplicatedEntity(userId, entityId);
        });
    handle.networkSystem->SetEntitySnapshotHandler(
        [networkSystem = handle.networkSystem,
         replicationSystem = handle.replicationSystem,
         registry = &handle.registry](const Engine::Net::EntitySnapshot& snapshot) {
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
                replicationSystem->ApplyEntitySnapshot(filtered, *registry);
                return;
            }
            replicationSystem->ApplyEntitySnapshot(snapshot, *registry);
        });
}

ClientSessionResult RunStressClientJoin(StressClientHandle& handle,
                                        uint16_t tcpPort,
                                        uint16_t udpPort,
                                        const std::string& displayName,
                                        bool loadCharacter,
                                        int joinRetryCount,
                                        int playerIndex,
                                        int playerCount) {
    ClientSessionResult result;
    SetupStressClientNetworkStack(handle);

    std::unique_ptr<CharacterLoader> characterLoader;
    if (loadCharacter) {
        SharedCharacterAssets& sharedAssets = GetSharedCharacterAssets();
        characterLoader =
            std::make_unique<CharacterLoader>(sharedAssets.catalog, sharedAssets.rules, sharedAssets.database);
        handle.characterInstance = std::make_unique<CharacterInstance>();
    }

    const auto connectStart = std::chrono::steady_clock::now();
    if (!ConnectAndLoginWithRetries(
            *handle.networkSystem, tcpPort, udpPort, displayName, joinRetryCount, playerIndex, playerCount)) {
        result.error = "ConnectAndLogin failed";
        return result;
    }
    result.connected = true;
    result.connectMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - connectStart).count();

    handle.replicationSystem->RegisterReplicatedEntity(handle.networkSystem->GetLocalUserId(), handle.playerEntity);

    const auto characterStart = std::chrono::steady_clock::now();
    if (loadCharacter && characterLoader && handle.characterInstance) {
        CharacterLoginContext login;
        login.userId = handle.networkSystem->GetLocalUserId();
        login.displayName = displayName;
        login.isOnlineLogin = true;
        if (!characterLoader->LoginAndBuild(
                handle.registry, *handle.characterInstance, handle.playerEntity, login)) {
            result.error = "character load failed";
            handle.networkSystem->Disconnect();
            return result;
        }
        handle.characterInstance->WireAnimationComponent(handle.registry);
        handle.networkSystem->SetRemotePlayerModel(handle.characterInstance->GetBodyModel(),
                                                   handle.characterInstance->scale);
        result.characterLoaded = true;
        result.characterLoadMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - characterStart).count();
    }

    return result;
}

void RunStressClientMovement(StressClientHandle& handle,
                             ClientSessionResult& result,
                             int movementSamples,
                             uint64_t movementTickBase,
                             std::chrono::seconds movementAckTimeout) {
    if (!handle.networkSystem || !result.connected) {
        return;
    }

    for (int sample = 0; sample < movementSamples; ++sample) {
        const uint64_t commandTick = movementTickBase + static_cast<uint64_t>(sample);

        Engine::Net::MovementCommand command;
        command.tick = commandTick;
        command.deltaTime = 1.0f / 60.0f;
        command.moveX = 0.4f;
        command.moveZ = 0.0f;
        command.run = true;

        std::atomic<bool> ackReceived{false};
        handle.networkSystem->ArmMovementAckWait(commandTick, &ackReceived);

        const auto sendStart = std::chrono::steady_clock::now();
        if (!handle.networkSystem->GetClient().SendMovementCommand(command)) {
            handle.networkSystem->DisarmMovementAckWait();
            result.error = "SendMovementCommand failed";
            break;
        }

        const bool acked = handle.networkSystem->PollUntilMovementAck(
            std::chrono::duration_cast<std::chrono::milliseconds>(movementAckTimeout));
        handle.networkSystem->DisarmMovementAckWait();

        const double rttMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sendStart).count();
        if (!acked) {
            result.error = "movement ack timeout";
            break;
        }
        result.movementRttMs.push_back(rttMs);
    }

    handle.networkSystem->Disconnect();
}

ClientSessionResult RunClientSession(uint16_t tcpPort,
                                     uint16_t udpPort,
                                     const std::string& displayName,
                                     int movementSamples,
                                     bool loadCharacter,
                                     uint64_t movementTickBase = kMovementTickBase,
                                     std::chrono::seconds movementAckTimeout = std::chrono::seconds(5),
                                     int joinRetryCount = 1) {
    StressClientHandle handle;
    ClientSessionResult result =
        RunStressClientJoin(handle, tcpPort, udpPort, displayName, loadCharacter, joinRetryCount, 0, 1);
    if (!result.connected) {
        return result;
    }
    RunStressClientMovement(handle, result, movementSamples, movementTickBase, movementAckTimeout);
    return result;
}

void RecordLatencyProperty(const char* prefix, const Engine::Tests::LatencyStats& stats) {
    testing::Test::RecordProperty(std::string(prefix) + "_count", static_cast<int>(stats.sampleCount));
    testing::Test::RecordProperty(std::string(prefix) + "_p95_ms", stats.p95Ms);
}

} // namespace

class GameNetworkE2ETest : public ::testing::Test {
protected:
    void SetUp() override {
        const std::string bodyPath =
            Game::Tests::RequireAsset("graphics/Models/character/animation/magic/FinalBaseMesh.fbx");
        if (!Game::Tests::FileExists(bodyPath)) {
            GTEST_SKIP() << "Character assets not copied to test runtime directory";
        }
    }
};

TEST_F(GameNetworkE2ETest, ConnectLoadCharacterAndMeasureMovementAck) {
    AuthoritativeGameServerFixture server;
    ClientSessionResult session =
        RunClientSession(server.TcpPort(), server.UdpPort(), "e2e_player", 5, true);

    ASSERT_TRUE(session.connected) << session.error;
    ASSERT_TRUE(session.characterLoaded) << session.error;
    EXPECT_GT(session.connectMs, 0.0);
    EXPECT_GT(session.characterLoadMs, 0.0);
    ASSERT_FALSE(session.movementRttMs.empty()) << session.error;

    const Engine::Tests::LatencyStats movementStats =
        Engine::Tests::ComputeLatencyStats(session.movementRttMs);
    RecordLatencyProperty("movement", movementStats);

    std::cout << "[GameE2E] connect_ms=" << session.connectMs
              << " character_load_ms=" << session.characterLoadMs << " movement "
              << Engine::Tests::FormatLatencyStats(movementStats) << std::endl;

    EXPECT_LT(movementStats.p95Ms, 2000.0) << "movement ack p95 should stay under 2s in local e2e";
}

TEST_F(GameNetworkE2ETest, MultiClientLatencyStress) {
    if (!E2eStressEnabled()) {
        GTEST_SKIP() << "Set GAME_RUN_E2E_STRESS=1 to run multi-client game e2e stress";
    }

    const int clientCount = E2eStressClientCount();
    AuthoritativeGameServerFixture server;

    std::vector<double> connectSamplesMs;
    std::vector<double> characterSamplesMs;
    std::vector<double> movementSamplesMs;
    connectSamplesMs.reserve(static_cast<size_t>(clientCount));
    characterSamplesMs.reserve(static_cast<size_t>(clientCount));
    movementSamplesMs.reserve(static_cast<size_t>(clientCount) * 3);

    for (int clientIndex = 0; clientIndex < clientCount; ++clientIndex) {
        ClientSessionResult session = RunClientSession(
            server.TcpPort(),
            server.UdpPort(),
            "stress_" + std::to_string(clientIndex),
            3,
            true);
        ASSERT_TRUE(session.connected) << "client " << clientIndex << ": " << session.error;
        ASSERT_TRUE(session.characterLoaded) << "client " << clientIndex << ": " << session.error;
        connectSamplesMs.push_back(session.connectMs);
        characterSamplesMs.push_back(session.characterLoadMs);
        movementSamplesMs.insert(movementSamplesMs.end(),
                                 session.movementRttMs.begin(),
                                 session.movementRttMs.end());
    }

    const Engine::Tests::LatencyStats connectStats = Engine::Tests::ComputeLatencyStats(connectSamplesMs);
    const Engine::Tests::LatencyStats characterStats =
        Engine::Tests::ComputeLatencyStats(characterSamplesMs);
    const Engine::Tests::LatencyStats movementStats =
        Engine::Tests::ComputeLatencyStats(movementSamplesMs);

    RecordLatencyProperty("connect", connectStats);
    RecordLatencyProperty("character_load", characterStats);
    RecordLatencyProperty("movement", movementStats);

    std::ostringstream profile;
    profile << std::fixed << std::setprecision(3);
    profile << "[GameE2EProfile] "
            << "{\"clients\":" << clientCount << ","
            << "\"connect_p95_ms\":" << connectStats.p95Ms << ","
            << "\"character_load_p95_ms\":" << characterStats.p95Ms << ","
            << "\"movement_p95_ms\":" << movementStats.p95Ms << "}";

    std::cout << profile.str() << std::endl;
    testing::Test::RecordProperty("game_e2e_profile", profile.str());
}

TEST_F(GameNetworkE2ETest, ConcurrentStressProfile) {
    if (!StressProfileEnabled()) {
        GTEST_SKIP() << "Set GAME_E2E_STRESS_PROFILE=1 to run the game e2e stress profile";
    }

    const int playerCount = StressPlayerCount();
    const bool loadCharacterMeshes = playerCount <= 64;
    const int movementSamples = loadCharacterMeshes ? 1 : 1;
    const int joinRetryCount = JoinRetryCountForPlayers(playerCount);

    if (loadCharacterMeshes) {
        PreloadSharedCharacterAssets();
    }

    AuthoritativeGameServerFixture server(true);
    server.SetStressRateLimits(playerCount);

    std::vector<ClientSessionResult> results(static_cast<size_t>(playerCount));
    std::vector<StressClientHandle> handles(static_cast<size_t>(playerCount));
    std::vector<std::thread> workers;
    workers.reserve(static_cast<size_t>(playerCount));
    std::atomic<bool> joinGate{false};
    std::atomic<bool> movementGate{false};
    std::atomic<int> workersReady{0};
    std::atomic<int> joinsComplete{0};
    std::mutex resultsMutex;

    const auto gateTimeout = std::chrono::seconds((std::max)(60, playerCount / 4 + 60));
    const auto spawnWaitTimeout = std::chrono::seconds((std::max)(5, playerCount / 12 + 5));
    const auto movementAckTimeout =
        std::chrono::seconds((std::max)(10, playerCount / 32 + 10));

    for (int playerIndex = 0; playerIndex < playerCount; ++playerIndex) {
        workers.emplace_back([&, playerIndex]() {
            workersReady.fetch_add(1, std::memory_order_relaxed);

            const auto joinWaitStart = std::chrono::steady_clock::now();
            while (!joinGate.load(std::memory_order_acquire)) {
                if (std::chrono::steady_clock::now() - joinWaitStart > gateTimeout) {
                    ClientSessionResult timeout;
                    timeout.error = "join gate timeout";
                    std::lock_guard<std::mutex> lock(resultsMutex);
                    results[static_cast<size_t>(playerIndex)] = std::move(timeout);
                    return;
                }
                std::this_thread::yield();
            }

            ClientSessionResult joinResult = RunStressClientJoin(
                handles[static_cast<size_t>(playerIndex)],
                server.TcpPort(),
                server.UdpPort(),
                "game_stress_" + std::to_string(playerIndex),
                loadCharacterMeshes,
                joinRetryCount,
                playerIndex,
                playerCount);
            {
                std::lock_guard<std::mutex> lock(resultsMutex);
                results[static_cast<size_t>(playerIndex)] = joinResult;
            }
            joinsComplete.fetch_add(1, std::memory_order_release);

            const auto movementWaitStart = std::chrono::steady_clock::now();
            while (!movementGate.load(std::memory_order_acquire)) {
                if (std::chrono::steady_clock::now() - movementWaitStart > gateTimeout) {
                    std::lock_guard<std::mutex> lock(resultsMutex);
                    results[static_cast<size_t>(playerIndex)].error = "movement gate timeout";
                    if (handles[static_cast<size_t>(playerIndex)].networkSystem) {
                        handles[static_cast<size_t>(playerIndex)].networkSystem->Disconnect();
                    }
                    return;
                }
                if (handles[static_cast<size_t>(playerIndex)].networkSystem) {
                    handles[static_cast<size_t>(playerIndex)].networkSystem->PollNetwork();
                }
                std::this_thread::yield();
            }

            bool shouldRunMovement = false;
            {
                std::lock_guard<std::mutex> lock(resultsMutex);
                shouldRunMovement = results[static_cast<size_t>(playerIndex)].connected;
            }
            if (shouldRunMovement) {
                ClientSessionResult& session = results[static_cast<size_t>(playerIndex)];
                RunStressClientMovement(handles[static_cast<size_t>(playerIndex)],
                                        session,
                                        movementSamples,
                                        kMovementTickBase + static_cast<uint64_t>(playerIndex) * 16u,
                                        movementAckTimeout);
            } else if (handles[static_cast<size_t>(playerIndex)].networkSystem) {
                handles[static_cast<size_t>(playerIndex)].networkSystem->Disconnect();
            }
        });
    }

    ASSERT_TRUE(Engine::Tests::WaitUntil(
        [&]() { return workersReady.load(std::memory_order_acquire) >= playerCount; }, gateTimeout));

    const auto burstStart = std::chrono::steady_clock::now();
    joinGate.store(true, std::memory_order_release);
    ASSERT_TRUE(Engine::Tests::WaitUntil(
        [&]() { return joinsComplete.load(std::memory_order_acquire) >= playerCount; }, gateTimeout));
    const double burstWallMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - burstStart).count();

    std::this_thread::sleep_for(spawnWaitTimeout);
    movementGate.store(true, std::memory_order_release);

    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    std::vector<double> connectMs;
    std::vector<double> characterMs;
    std::vector<double> movementMs;
    size_t successCount = 0;
    size_t characterSuccessCount = 0;
    size_t movementSuccessCount = 0;

    for (size_t i = 0; i < results.size(); ++i) {
        const ClientSessionResult& session = results[i];
        if (!session.connected) {
            ADD_FAILURE() << "Player " << i << " connect failed: " << session.error;
            continue;
        }
        ++successCount;
        connectMs.push_back(session.connectMs);
        if (session.characterLoaded) {
            ++characterSuccessCount;
            characterMs.push_back(session.characterLoadMs);
        } else if (loadCharacterMeshes) {
            ADD_FAILURE() << "Player " << i << " character load failed: " << session.error;
        }
        if (!session.movementRttMs.empty()) {
            ++movementSuccessCount;
            movementMs.insert(movementMs.end(), session.movementRttMs.begin(), session.movementRttMs.end());
        } else {
            ADD_FAILURE() << "Player " << i << " movement failed: " << session.error;
        }
    }

    const Engine::Tests::LatencyStats connectStats = Engine::Tests::ComputeLatencyStats(connectMs);
    const Engine::Tests::LatencyStats characterStats = Engine::Tests::ComputeLatencyStats(characterMs);
    const Engine::Tests::LatencyStats movementStats = Engine::Tests::ComputeLatencyStats(movementMs);

    RecordLatencyProperty("connect", connectStats);
    RecordLatencyProperty("character_load", characterStats);
    RecordLatencyProperty("movement", movementStats);
    testing::Test::RecordProperty("player_count", playerCount);
    testing::Test::RecordProperty("burst_wall_ms", burstWallMs);
    testing::Test::RecordProperty("character_mesh_load", loadCharacterMeshes ? 1 : 0);

    std::ostringstream profile;
    profile << std::fixed << std::setprecision(3);
    profile << "[GameE2EProfile] "
            << "{\"players\":" << playerCount << ","
            << "\"joined\":" << successCount << ","
            << "\"character_ok\":" << characterSuccessCount << ","
            << "\"movement_ok\":" << movementSuccessCount << ","
            << "\"burst_wall_ms\":" << burstWallMs << ","
            << "\"connect_p95_ms\":" << connectStats.p95Ms << ","
            << "\"character_load_p95_ms\":" << characterStats.p95Ms << ","
            << "\"movement_p95_ms\":" << movementStats.p95Ms << ","
            << "\"character_mesh_load\":" << (loadCharacterMeshes ? "true" : "false") << "}";

    std::cout << profile.str() << std::endl;
    testing::Test::RecordProperty("game_e2e_stress_profile", profile.str());

    EXPECT_EQ(successCount, static_cast<size_t>(playerCount));
    EXPECT_EQ(movementSuccessCount, static_cast<size_t>(playerCount));
    if (loadCharacterMeshes) {
        EXPECT_EQ(characterSuccessCount, static_cast<size_t>(playerCount));
    }
}
