#include "render/systems/AnimationSystem.h"
#include "render/components/AnimationComponent.h"
#include "render/Animation.h"
#include "render/Skeleton.h"
#include "Logger.h"
#include <cmath>
#include <algorithm>

namespace Engine {
namespace Platform {
namespace Render {
namespace Systems {

AnimationSystem::AnimationSystem() {
    SetPriority(50); // Run after InputSystem (0) but before RenderSystem (100)
}

void AnimationSystem::Initialize(ECS::Registry& registry) {
    LOG_INFO("AnimationSystem", "AnimationSystem initialized");
}

void AnimationSystem::Update(float deltaTime, ECS::Registry& registry, uint64_t tickNumber) {
    // ANIMATIONS DISABLED
    // UpdateAnimationPlayback(deltaTime, registry);
}

void AnimationSystem::Shutdown(ECS::Registry& registry) {
    // Cleanup if needed
}

void AnimationSystem::UpdateAnimationPlayback(float deltaTime, ECS::Registry& registry) {
    auto entities = registry.View<Components::AnimationComponent>();
    
    for (auto entityID : entities) {
        auto* animComp = registry.GetComponent<Components::AnimationComponent>(entityID);
        if (!animComp) {
            continue;
        }
        
        Animation* targetAnimation = nullptr;
        
        switch (animComp->currentState) {
            case Components::AnimationComponent::AnimationState::Walking:
                targetAnimation = animComp->walkingAnimation;
                break;
            case Components::AnimationComponent::AnimationState::Running:
                targetAnimation = animComp->runningAnimation;
                break;
            case Components::AnimationComponent::AnimationState::Jumping:
                targetAnimation = animComp->jumpAnimation;
                break;
            default:
                targetAnimation = nullptr;
                break;
        }
        
        if (animComp->currentAnimation != targetAnimation) {
            animComp->currentAnimation = targetAnimation;
            animComp->currentAnimationTime = 0.0f;
            
            if (targetAnimation) {
                animComp->isLooping = (animComp->currentState != Components::AnimationComponent::AnimationState::Jumping);
            }
        }
        
        if (animComp->currentAnimation && animComp->currentAnimation->IsLoaded()) {
            float duration = animComp->currentAnimation->GetDuration();
            float ticksPerSecond = animComp->currentAnimation->GetTicksPerSecond();
            
            if (ticksPerSecond > 0.0f) {
                animComp->currentAnimationTime += deltaTime;
                
                if (animComp->isLooping) {
                    float durationSeconds = duration / ticksPerSecond;
                    if (animComp->currentAnimationTime > durationSeconds) {
                        animComp->currentAnimationTime = std::fmod(animComp->currentAnimationTime, durationSeconds);
                    }
                }
                
                if (animComp->skeleton && animComp->skeleton->IsLoaded()) {
                    float timeInTicks = animComp->currentAnimationTime * ticksPerSecond;
                    
                    std::map<std::string, glm::mat4> animationTransforms;
                    animComp->currentAnimation->GetBoneTransforms(timeInTicks, animationTransforms);
                    animComp->skeleton->UpdateBoneTransforms(animationTransforms);
                }
            }
        }
    }
}

} // namespace Systems
} // namespace Render
} // namespace Platform
} // namespace Engine

