#include "WorldSceneBootstrap.h"

#include "AppPaths.h"
#include "Logger.h"
#include "core/scene/SceneSpawner.h"

using EngineLogging::Logger;

namespace Game {

bool BootstrapWorldScene(Engine::ECS::Registry& registry) {
    const std::string scenePath = ResolveDataPath("scenes/world.scene.json");
    if (!Engine::Scene::SceneSpawner::LoadAndInstantiateScene(registry, scenePath)) {
        LOG_WARNING_FMT("WorldScene", "Failed to load world scene at %s", scenePath.c_str());
        return false;
    }

    LOG_INFO_FMT("WorldScene", "Loaded world scene from %s", scenePath.c_str());
    return true;
}

} // namespace Game
