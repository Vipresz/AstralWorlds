#include "Logger.h"
#include "core/network/NetTypes.h"
#include "network/NetServer.h"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>

using Engine::Net::kDefaultPort;
using Engine::Platform::Networking::NetServer;
using EngineLogging::Logger;

namespace {

std::atomic<bool> g_running{true};

void OnSignal(int) {
    g_running = false;
}

void PrintUsage(const char* program) {
    std::cout << "AstralWorlds multiplayer relay server\n\n"
              << "Usage:\n"
              << "  " << program << " [port]\n"
              << "  " << program << " --port <port>\n\n"
              << "Default port: " << kDefaultPort << "\n";
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
        } else {
            return static_cast<uint16_t>(std::atoi(argv[1]));
        }
    }
    return kDefaultPort;
}

} // namespace

int main(int argc, char* argv[]) {
    Logger& logger = Logger::GetInstance();
    logger.Initialize("server.log");
    logger.SetMinLevel(EngineLogging::LogLevel::LevelInfo);

    const uint16_t port = ParsePort(argc, argv);

    std::signal(SIGINT, OnSignal);
#ifdef SIGTERM
    std::signal(SIGTERM, OnSignal);
#endif

    NetServer server;
    if (!server.Start("0.0.0.0", port)) {
        LOG_ERROR_FMT("Server", "Failed to start on port %u", port);
        return 1;
    }

    LOG_INFO_FMT("Server", "Relay server running on 0.0.0.0:%u - Ctrl+C to stop", port);
    std::cout << "AstralWorlds server listening on port " << port << std::endl;
    std::cout << "Keep this window open while clients connect." << std::endl;
    std::cout << "Clients: game.exe --name YourName" << std::endl;
    std::cout.flush();

    while (g_running && server.IsRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }

    LOG_INFO("Server", "Shutting down...");
    server.Stop();
    logger.Shutdown();
    return 0;
}
