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
        } else if (arg == "--user" || arg == "-u") {
            const std::string value = ArgValue(argc, argv, i);
            if (!value.empty()) {
                config.username = value;
                ++i;
            }
        } else if (arg == "--password") {
            const std::string value = ArgValue(argc, argv, i);
            if (!value.empty()) {
                config.password = value;
                ++i;
            }
        } else if (arg == "--auth-token") {
            const std::string value = ArgValue(argc, argv, i);
            if (!value.empty()) {
                config.authToken = value;
                ++i;
            }
        } else if (arg == "--tls") {
            config.useTls = true;
        } else if (arg == "--tls-insecure") {
            config.useTls = true;
            config.tlsInsecure = true;
        } else if (arg == "--tls-ca") {
            const std::string value = ArgValue(argc, argv, i);
            if (!value.empty()) {
                config.tlsCaPath = value;
                config.useTls = true;
                ++i;
            }
        } else if (arg == "--tls-server-name") {
            const std::string value = ArgValue(argc, argv, i);
            if (!value.empty()) {
                config.tlsServerName = value;
                config.useTls = true;
                ++i;
            }
        } else if (arg == "--offline" || arg == "--no-network") {
            config.enabled = false;
        } else if (arg == "--smoke-test") {
            config.smokeTest = true;
            config.enabled = false;
        } else if (arg == "--e2e-test") {
            config.e2eTest = true;
            config.enabled = true;
        } else if (arg == "--e2e-seconds") {
            const std::string value = ArgValue(argc, argv, i);
            if (!value.empty()) {
                config.e2eRunSeconds = std::atoi(value.c_str());
                ++i;
            }
        } else if (arg == "--e2e-movement-samples") {
            const std::string value = ArgValue(argc, argv, i);
            if (!value.empty()) {
                config.e2eMovementSamples = std::atoi(value.c_str());
                ++i;
            }
        } else if (arg == "--e2e-report") {
            const std::string value = ArgValue(argc, argv, i);
            if (!value.empty()) {
                config.e2eReportPath = value;
                ++i;
            }
        }
    }

    return config;
}

std::string GameNetworkConfig::ResolveTlsServerName() const {
    if (!tlsServerName.empty()) {
        return tlsServerName;
    }
    if (serverHost == "127.0.0.1" || serverHost == "::1") {
        return "localhost";
    }
    return serverHost;
}

} // namespace Game
