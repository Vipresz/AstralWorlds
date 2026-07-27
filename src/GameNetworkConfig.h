#pragma once

#include "core/network/NetTypes.h"

#include <cstdint>
#include <string>

namespace Game {

struct GameNetworkConfig {
    std::string serverHost = "127.0.0.1";
    uint16_t serverPort = Engine::Net::kDefaultPort;
    std::string displayName = "Player";
    bool enabled = true;

    static GameNetworkConfig ParseCommandLine(int argc, char* argv[]);
};

} // namespace Game
