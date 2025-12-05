#pragma once

#include "core/System.h"
#include "core/ecs/Registry.h"
#include "render/components/AnimationComponent.h"

namespace Engine {
namespace Platform {
namespace Render {
namespace Systems {

// Animation system - updates animation playback and prepares bone transforms
class AnimationSystem : public ECS::ISystem {
public:
    AnimationSystem();
    
    void Initialize(ECS::Registry& registry) override;
    void Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) override;
    void Shutdown(ECS::Registry& registry) override;
    
private:
    void UpdateAnimationPlayback(float deltaTime, ECS::Registry& registry);
};

} // namespace Systems
} // namespace Render
} // namespace Platform
} // namespace Engine

