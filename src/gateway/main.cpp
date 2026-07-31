#include "Logger.h"

#include "core/network/ShardRegistry.h"
#include "network/NetGateway.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

using Engine::Platform::Networking::NetGateway;
using EngineLogging::Logger;

namespace {

std::atomic<bool> g_running{true};

void OnSignal(int) {
    g_running = false;
}

std::string ParseArgValue(int argc, char* argv[], const std::string& flag) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (flag == argv[i]) {
            return argv[i + 1];
        }
    }
    return {};
}

uint16_t ParsePort(int argc, char* argv[]) {
    const std::string portArg = ParseArgValue(argc, argv, "--port");
    if (!portArg.empty()) {
        return static_cast<uint16_t>(std::atoi(portArg.c_str()));
    }
    if (argc >= 2 && argv[1][0] != '-') {
        return static_cast<uint16_t>(std::atoi(argv[1]));
    }
    return 7780;
}

} // namespace

int main(int argc, char* argv[]) {
    Logger& logger = Logger::GetInstance();
    logger.Initialize("gateway.log");
    logger.SetMinLevel(EngineLogging::LogLevel::LevelInfo);

    std::signal(SIGINT, OnSignal);
#ifdef SIGTERM
    std::signal(SIGTERM, OnSignal);
#endif

    const uint16_t port = ParsePort(argc, argv);
    const std::string authToken = ParseArgValue(argc, argv, "--auth-token");
    const std::string shardListPath = ParseArgValue(argc, argv, "--shards");

    NetGateway gateway;
    if (!authToken.empty()) {
        gateway.SetRequiredAuthToken(authToken);
    }

    if (!shardListPath.empty()) {
        // shard entries: id,host,tcp,udp,maxPlayers per line
        std::ifstream input(shardListPath);
        std::string line;
        while (std::getline(input, line)) {
            if (line.empty() || line[0] == '#') {
                continue;
            }
            Engine::Net::ShardEndpoint endpoint;
            char comma = 0;
            std::istringstream stream(line);
            std::string host;
            uint32_t tcp = 0;
            uint32_t udp = 0;
            if (!(stream >> endpoint.shardId)) {
                continue;
            }
            if (!std::getline(stream, line, ',') || line.size() <= 1) {
                continue;
            }
            host = line.substr(1);
            if (!(stream >> tcp >> comma >> udp >> comma >> endpoint.maxPlayers)) {
                endpoint.maxPlayers = 256;
            }
            endpoint.host = host;
            endpoint.tcpPort = static_cast<uint16_t>(tcp);
            endpoint.udpPort = static_cast<uint16_t>(udp);
            gateway.GetRegistry().RegisterShard(endpoint);
        }
    }

    if (!gateway.Start("0.0.0.0", port)) {
        LOG_ERROR_FMT("Gateway", "Failed to start on port %u", port);
        return 1;
    }

    std::cout << "AstralWorlds gateway listening on port " << port << std::endl;
    while (g_running && gateway.IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    gateway.Stop();
    logger.Shutdown();
    return 0;
}
