#pragma once

#include "core/network/NetTypes.h"

#include <cstdint>
#include <string>

namespace Game {

struct GameNetworkConfig {
    std::string serverHost = "127.0.0.1";
    uint16_t serverPort = Engine::Net::kDefaultPort;
    std::string displayName = "Player";
    std::string username;
    std::string password;
    std::string authToken;
    bool useTls = false;
    bool tlsInsecure = false;
    std::string tlsCaPath;
    bool enabled = true;
    bool smokeTest = false;
    bool e2eTest = false;
    int e2eRunSeconds = 8;
    int e2eMovementSamples = 5;
    std::string e2eReportPath = "e2e_report.json";

    static GameNetworkConfig ParseCommandLine(int argc, char* argv[]);
};

} // namespace Game
