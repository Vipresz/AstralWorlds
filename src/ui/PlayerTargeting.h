#pragma once

#include "core/Types.h"
#include "core/network/NetTypes.h"
#include "render/ICameraSource.h"

#include <glm/glm.hpp>
#include <string>

namespace Engine {
namespace ECS {
class Registry;
}
namespace Platform {
namespace Networking {
namespace Systems {
class NetworkSystem;
}
} // namespace Networking
} // namespace Platform
} // namespace Engine

namespace Game {
namespace UI {

struct PlayerPickResult {
    Engine::EntityID entity = Engine::INVALID_ENTITY;
    Engine::Net::UserId userId = Engine::Net::kInvalidUserId;
    std::string displayName;
};

// Screen-space pick of the nearest remote player under the cursor.
bool TryPickRemotePlayerAtScreen(Engine::ECS::Registry& registry,
                                 Engine::Platform::Networking::Systems::NetworkSystem& network,
                                 const Engine::Platform::Render::ICameraSource& camera,
                                 const glm::mat4& projection,
                                 int screenWidth,
                                 int screenHeight,
                                 float mouseX,
                                 float mouseY,
                                 PlayerPickResult& out,
                                 float maxScreenDistancePixels = 48.0f);

// Horizontal (XZ) distance between two entities with transforms.
bool TryGetHorizontalDistanceBetweenEntities(Engine::ECS::Registry& registry,
                                             Engine::EntityID firstEntity,
                                             Engine::EntityID secondEntity,
                                             float& outDistance);

} // namespace UI
} // namespace Game

