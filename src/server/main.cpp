#include "Logger.h"

#include "AppPaths.h"
#include "character/CharacterInventoryDatabase.h"
#include "character/CharacterDatabase.h"
#include "character/CharacterStats.h"
#include "character/CharacterTypes.h"
#include "character/GameItemCatalog.h"
#include "core/ecs/components/PlayerItemOwnershipComponent.h"
#include "core/network/NetTypes.h"
#include "core/network/MovementCommand.h"
#include "core/network/ShardConfig.h"
#include "core/network/ScalePolicyConfig.h"
#include "core/network/ZoneScaleConfig.h"

#include "network/AuthoritativeGameSimulator.h"

#include "network/NetServer.h"

#include "network/TlsSession.h"



#include <atomic>

#include <chrono>

#include <cstdlib>

#include <csignal>

#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <glm/glm.hpp>



using Engine::Net::kDefaultPort;

using Engine::Platform::Networking::AuthoritativeGameSimulator;

using Engine::Platform::Networking::NetServer;

using Engine::Platform::Networking::TlsConfig;

using EngineLogging::Logger;



namespace {



std::atomic<bool> g_running{true};



void OnSignal(int) {

    g_running = false;

}



void PrintUsage(const char* program) {

    std::cout << "AstralWorlds multiplayer server\n\n"

              << "Usage:\n"

              << "  " << program << " [port]\n"

              << "  " << program << " --port <port>\n"

              << "  " << program << " --accounts <users.txt>\n"

              << "  " << program << " --relay-only\n"

              << "  " << program << " --heightmap <terrain.png>\n"

              << "  " << program << " --tls-cert <cert.pem> --tls-key <key.pem>\n"
              << "  " << program << " --max-players <count>\n"
              << "  " << program << " --shard-id <id>\n"
              << "  " << program << " --redirect-host <host> --redirect-port <port>\n"
              << "  " << program << " --scale-config <zone.json>\n"
              << "  " << program << " --shard-soft-cap <count>\n"
              << "  " << program << " --layer-density-threshold <count>\n"
              << "  " << program << " --overflow-policy <LayerFirst|ShardFirst|LayerThenShard>\n\n"

              << "Default: authoritative ECS simulation on port " << kDefaultPort << "\n";

}



uint16_t ParsePort(int argc, char* argv[]) {

    if (argc >= 2) {

        const std::string arg = argv[1];

        if (arg == "--help" || arg == "-h") {

            PrintUsage(argv[0]);

            std::exit(arg == "--help" ? 0 : 1);

        }

        if (arg == "--port" || arg == "-p") {

            if (argc >= 3) {

                return static_cast<uint16_t>(std::atoi(argv[2]));

            }

        } else if (arg[0] != '-') {

            return static_cast<uint16_t>(std::atoi(argv[1]));

        }

    }

    return kDefaultPort;

}



std::string ParseArgValue(int argc, char* argv[], const std::string& flag) {

    for (int i = 1; i + 1 < argc; ++i) {

        if (flag == argv[i]) {

            return argv[i + 1];

        }

    }

    return {};

}



bool HasFlag(int argc, char* argv[], const std::string& flag) {

    for (int i = 1; i < argc; ++i) {

        if (flag == argv[i]) {

            return true;

        }

    }

    return false;

}



} // namespace



int main(int argc, char* argv[]) {

    Logger& logger = Logger::GetInstance();

    logger.Initialize("server.log");

    logger.SetMinLevel(EngineLogging::LogLevel::LevelInfo);

    if (!Game::SetWorkingDirectoryToExecutable()) {
        LOG_WARNING("Server", "Could not set working directory to executable folder");
    }



    const uint16_t port = ParsePort(argc, argv);

    const std::string accountsPath = ParseArgValue(argc, argv, "--accounts");

    const std::string tlsCertPath = ParseArgValue(argc, argv, "--tls-cert");

    const std::string tlsKeyPath = ParseArgValue(argc, argv, "--tls-key");

    const bool relayOnly = HasFlag(argc, argv, "--relay-only");



    std::signal(SIGINT, OnSignal);

#ifdef SIGTERM

    std::signal(SIGTERM, OnSignal);

#endif



    NetServer server;

    std::unique_ptr<AuthoritativeGameSimulator> simulator;
    std::unique_ptr<Game::Character::ServerLoadoutDatabase> loadoutDatabase;
    std::unique_ptr<Game::Character::CharacterInventoryDatabase> inventoryDatabase;
    std::unique_ptr<Game::Character::GameItemCatalog> gameItemCatalog;

    if (!relayOnly) {

        simulator = std::make_unique<AuthoritativeGameSimulator>(server);

        gameItemCatalog = std::make_unique<Game::Character::GameItemCatalog>();
        if (!gameItemCatalog->Load("character/game_items.json")) {
            LOG_WARNING("Server", "Game item catalog failed to load — using default stack caps");
        }
        simulator->SetBagCapacity(Engine::ECS::kDefaultBagCapacity);
        simulator->SetItemStackCapResolver(
            [catalog = gameItemCatalog.get()](const std::string& itemId) {
                return catalog->GetStackCap(itemId);
            });

        simulator->SetSpawnPosition(glm::vec3(5.0f, 0.0f, 5.0f));
        Game::Character::CharacterStats characterStats;
        Game::Character::LoadCharacterStats(characterStats);
        simulator->SetMovementSpeeds(characterStats.walkSpeed, characterStats.runSpeed);
        const std::string heightmapPath =
            ParseArgValue(argc, argv, "--heightmap").empty()
                ? std::string("terrain/barren_scar/00.png")
                : ParseArgValue(argc, argv, "--heightmap");
        constexpr float kTerrainSizeX = 100.0f;
        constexpr float kTerrainSizeZ = 100.0f;
        if (!simulator->LoadHeightmap(heightmapPath, kTerrainSizeX, kTerrainSizeZ, 10.0f)) {
            LOG_WARNING_FMT("Server", "Heightmap not loaded from %s — using flat terrain", heightmapPath.c_str());
            simulator->SetTerrainSampler([](float /*x*/, float /*z*/) { return 0.0f; });
        } else {
            LOG_INFO_FMT("Server", "Authoritative terrain loaded from %s", heightmapPath.c_str());
        }

        server.SetAuthoritativeMode(true);

        loadoutDatabase = std::make_unique<Game::Character::ServerLoadoutDatabase>("character/loadouts");
        LOG_INFO_FMT("Server", "Using loadout directory %s", loadoutDatabase->GetLoadoutsDirectory().c_str());
        inventoryDatabase = std::make_unique<Game::Character::CharacterInventoryDatabase>("character/astral_worlds.db");
        if (!inventoryDatabase->Open()) {
            LOG_WARNING("Server", "Inventory database failed to open — persistence disabled");
        }

        server.SetClientLoggedInCallback([&simulator, &loadoutDatabase, &inventoryDatabase](
                                             Engine::Net::UserId userId,
                                             const std::string& displayName) {
            if (!simulator || !loadoutDatabase || !inventoryDatabase) {
                return;
            }

            Game::Character::CharacterLoginContext loginContext;
            loginContext.userId = userId;
            loginContext.displayName = displayName;

            Engine::Net::ReplicatedItemOwnership restoredOwnership;
            const bool hasSavedInventory =
                inventoryDatabase->LoadOwnership(userId, restoredOwnership) &&
                !restoredOwnership.items.empty();
            if (hasSavedInventory) {
                simulator->OnClientLoggedIn(userId, displayName, {}, &restoredOwnership);
                return;
            }

            Game::Character::CharacterLoadout loadout;
            loadoutDatabase->FetchLoadout(loginContext, loadout);

            std::vector<std::pair<std::string, uint32_t>> starterInventory;
            starterInventory.reserve(loadout.inventory.size());
            for (const Game::Character::LoadoutInventoryEntry& entry : loadout.inventory) {
                starterInventory.emplace_back(entry.itemId, entry.quantity);
            }

            if (!starterInventory.empty()) {
                LOG_INFO_FMT(
                    "Server",
                    "Granting %zu starter inventory stack(s) to new account %u",
                    starterInventory.size(),
                    userId);
            } else {
                LOG_WARNING_FMT(
                    "Server",
                    "No starter inventory configured for account %u",
                    userId);
            }

            simulator->OnClientLoggedIn(userId, displayName, starterInventory);
        });

        server.SetClientDisconnectedCallback([&simulator, &inventoryDatabase](
                                                 Engine::Net::UserId userId) {
            if (!simulator || !inventoryDatabase) {
                return;
            }

            const Engine::Net::ReplicatedItemOwnership* ownership = simulator->GetOwnedItems(userId);
            if (ownership) {
                inventoryDatabase->SaveOwnership(userId, *ownership);
            }

            simulator->OnClientDisconnected(userId);
        });

        server.SetGenericDataInterceptor([&simulator](Engine::Net::UserId userId,

                                                        Engine::Net::GenericDataPayload& payload) {

            return simulator && simulator->HandleGenericData(userId, payload);

        });

        server.SetMovementCommandInterceptor([&simulator](Engine::Net::UserId userId,
                                                          Engine::Net::MovementCommand& command) {
            return simulator && simulator->HandleMovementCommand(userId, command);
        });

    }



    if (!accountsPath.empty()) {

        server.GetCredentialStore().SetRequireCredentials(true);

        if (!server.GetCredentialStore().LoadFromFile(accountsPath)) {

            LOG_ERROR_FMT("Server", "Failed to load accounts from %s", accountsPath.c_str());

            return 1;

        }

        LOG_INFO_FMT("Server", "Loaded credential store from %s", accountsPath.c_str());

    }



    if (!tlsCertPath.empty() || !tlsKeyPath.empty()) {

        TlsConfig tls;

        tls.enabled = true;

        tls.certificatePath = tlsCertPath;

        tls.privateKeyPath = tlsKeyPath;

        server.SetTlsConfig(tls);

        LOG_INFO("Server", "TLS enabled for incoming TCP connections");

    }



    Engine::Net::ShardConfig shardConfig;
    Engine::Net::ScalePolicyConfig scalePolicy;
    const std::string scaleConfigPath = ParseArgValue(argc, argv, "--scale-config");
    if (!scaleConfigPath.empty()) {
        Engine::Net::ZoneScaleConfig zoneConfig;
        if (!Engine::Net::ZoneScaleConfig::TryLoadFromFile(scaleConfigPath, zoneConfig)) {
            LOG_ERROR_FMT("Server", "Failed to load scale config: %s", scaleConfigPath.c_str());
            return 1;
        }
        shardConfig = zoneConfig.shard;
        scalePolicy = zoneConfig.scalePolicy;
        LOG_INFO_FMT("Server", "Loaded zone scale config '%s' for zone '%s'",
                     scaleConfigPath.c_str(),
                     zoneConfig.zoneId.c_str());
    }

    const std::string maxPlayersArg = ParseArgValue(argc, argv, "--max-players");
    if (!maxPlayersArg.empty()) {
        shardConfig.maxPlayers = static_cast<uint32_t>(std::atoi(maxPlayersArg.c_str()));
    }
    const std::string shardIdArg = ParseArgValue(argc, argv, "--shard-id");
    if (!shardIdArg.empty()) {
        shardConfig.shardId = static_cast<uint32_t>(std::atoi(shardIdArg.c_str()));
    }
    shardConfig.redirectHost = ParseArgValue(argc, argv, "--redirect-host");
    const std::string redirectPortArg = ParseArgValue(argc, argv, "--redirect-port");
    if (!redirectPortArg.empty()) {
        shardConfig.redirectTcpPort = static_cast<uint16_t>(std::atoi(redirectPortArg.c_str()));
        shardConfig.redirectUdpPort = static_cast<uint16_t>(shardConfig.redirectTcpPort + 1);
    }
    server.SetShardConfig(shardConfig);

    if (shardConfig.maxPlayers > 0) {
        scalePolicy.shardHardCap = shardConfig.maxPlayers;
    }
    const std::string shardSoftCapArg = ParseArgValue(argc, argv, "--shard-soft-cap");
    if (!shardSoftCapArg.empty()) {
        scalePolicy.shardSoftCap = static_cast<uint32_t>(std::atoi(shardSoftCapArg.c_str()));
    }
    const std::string layerDensityArg = ParseArgValue(argc, argv, "--layer-density-threshold");
    if (!layerDensityArg.empty()) {
        scalePolicy.cellLayerDensityThreshold = static_cast<uint32_t>(std::atoi(layerDensityArg.c_str()));
    }
    const std::string overflowPolicyArg = ParseArgValue(argc, argv, "--overflow-policy");
    if (overflowPolicyArg == "LayerFirst") {
        scalePolicy.overflowPolicy = Engine::Net::OverflowPolicy::LayerFirst;
    } else if (overflowPolicyArg == "ShardFirst") {
        scalePolicy.overflowPolicy = Engine::Net::OverflowPolicy::ShardFirst;
    } else if (overflowPolicyArg == "LayerThenShard") {
        scalePolicy.overflowPolicy = Engine::Net::OverflowPolicy::LayerThenShard;
    }
    server.SetScalePolicyConfig(scalePolicy);

    if (!server.Start("0.0.0.0", port)) {

        LOG_ERROR_FMT("Server", "Failed to start on port %u", port);

        return 1;

    }



    const char* modeLabel = relayOnly ? "relay" : "authoritative";

    LOG_INFO_FMT("Server", "%s server running on 0.0.0.0:%u - Ctrl+C to stop", modeLabel, port);

    std::cout << "AstralWorlds " << modeLabel << " server listening on port " << port << std::endl;

    std::cout << "Keep this window open while clients connect." << std::endl;

    std::cout << "Clients: game.exe --name YourName [--user alice --password secret]" << std::endl;

    std::cout.flush();



    constexpr float kTickSeconds = 1.0f / 60.0f;

    auto lastTick = std::chrono::steady_clock::now();

    while (g_running && server.IsRunning()) {

        std::this_thread::sleep_for(std::chrono::milliseconds(5));

        if (!simulator) {

            continue;

        }



        const auto now = std::chrono::steady_clock::now();

        const float elapsed =

            std::chrono::duration<float>(now - lastTick).count();

        if (elapsed < kTickSeconds) {

            continue;

        }

        lastTick = now;

        simulator->Update(elapsed);

    }



    LOG_INFO("Server", "Shutting down...");

    server.Stop();

    logger.Shutdown();

    return 0;

}


