#pragma once

#include "core/ecs/Registry.h"

namespace Game {

// Loads scenes/world.scene.json relative to the executable and spawns ECS entities.
bool BootstrapWorldScene(Engine::ECS::Registry& registry);

} // namespace Game
