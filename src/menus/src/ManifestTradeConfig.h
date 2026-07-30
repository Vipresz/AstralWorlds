#pragma once

namespace Game {
namespace UI {

struct ManifestTradeConfig {
    float maxDistance = 8.0f;
};

constexpr const char* kManifestTradeConfigPath = "menus/manifest.json";

bool LoadManifestTradeConfig(ManifestTradeConfig& out,
                             const char* manifestPath = kManifestTradeConfigPath);

} // namespace UI
} // namespace Game
