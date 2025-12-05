#pragma once

#include "core/Types.h"
#include <memory>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cstdlib>

namespace Engine {
namespace ECS {

class Registry;

// Base system interface
class ISystem {
public:
    virtual ~ISystem() = default;
    virtual void Update(float deltaTime, Registry& registry, uint64_t tickNumber) = 0;
    virtual void Initialize(Registry& registry) {}
    virtual void Shutdown(Registry& registry) {}
    
    // Optional: Systems can specify execution order
    int32_t GetPriority() const { return m_priority; }
    void SetPriority(int32_t priority) { m_priority = priority; }
    
    bool IsEnabled() const { return m_enabled; }
    void SetEnabled(bool enabled) { m_enabled = enabled; }
    
protected:
    int32_t m_priority = 0;
    bool m_enabled = true;
};

// System manager - handles system registration and updates
class SystemManager {
public:
    void RegisterSystem(std::shared_ptr<ISystem> system) {
        m_systems.push_back(system);
        // Sort by priority
        std::sort(m_systems.begin(), m_systems.end(),
            [](const auto& a, const auto& b) {
                return a->GetPriority() < b->GetPriority();
            });
    }
    
    void Initialize(Registry& registry) {
        for (auto& system : m_systems) {
            system->Initialize(registry);
        }
    }
    
    void Update(float deltaTime, Registry& registry, uint64_t tickNumber) {
        for (auto& system : m_systems) {
            if (system->IsEnabled()) {
                system->Update(deltaTime, registry, tickNumber);
            }
        }
    }
    
    void Shutdown(Registry& registry) {
        for (auto& system : m_systems) {
            system->Shutdown(registry);
        }
    }
    
private:
    std::vector<std::shared_ptr<ISystem>> m_systems;
};

} // namespace ECS
} // namespace Engine

