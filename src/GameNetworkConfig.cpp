#include "GameNetworkConfig.h"

#include <cstdlib>
#include <string>

namespace Game {

namespace {

std::string ArgValue(int argc, char* argv[], int index) {
    if (index + 1 >= argc) {
        return {};
    }
    return argv[index + 1];
}

} // namespace

GameNetworkConfig GameNetworkConfig::ParseCommandLine(int argc, char* argv[]) {
    GameNetworkConfig config;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--host" || arg == "-h") {
            const std::string value = ArgValue(argc, argv, i);
            if (!value.empty()) {
                config.serverHost = value;
                ++i;
            }
        } else if (arg == "--port" || arg == "-p") {
            const std::string value = ArgValue(argc, argv, i);
            if (!value.empty()) {
                config.serverPort = static_cast<uint16_t>(std::atoi(value.c_str()));
                ++i;
            }
        } else if (arg == "--name" || arg == "-n") {
            const std::string value = ArgValue(argc, argv, i);
            if (!value.empty()) {
                config.displayName = value;
                ++i;
            }
        } else if (arg == "--offline" || arg == "--no-network") {
            config.enabled = false;
        }
    }

    return config;
}

} // namespace Game
