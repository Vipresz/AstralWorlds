// Basic example showing how to use the engine core systems
// This demonstrates ECS, fixed timestep, and systems working together

#include "core/ecs/Registry.h"
#include "core/System.h"
#include "core/time/GameLoop.h"
#include "core/math/DeterministicMath.h"
#include <iostream>
#include <memory>

// Use explicit namespace qualification to avoid conflicts with std namespace
using Engine::ECS::Registry;
using Engine::ECS::Entity;
using Engine::ECS::ISystem;
using Engine::ECS::SystemManager;

// Example components
struct Position {
    float x, y, z;
    Position() : x(0), y(0), z(0) {}
    Position(float x, float y, float z) : x(x), y(y), z(z) {}
};

struct Velocity {
    float x, y, z;
    Velocity() : x(0), y(0), z(0) {}
    Velocity(float x, float y, float z) : x(x), y(y), z(z) {}
};

struct Health {
    float current;
    float max;
    Health() : current(100), max(100) {}
    Health(float max) : current(max), max(max) {}
};

// Example system - moves entities with Position and Velocity
class MovementSystem : public ISystem {
public:
    MovementSystem() {
        SetPriority(100); // Lower priority = runs earlier
    }
    
    void Update(float deltaTime, Registry& registry, uint64_t tickNumber) override {
        // Get all entities with Position and Velocity components
        auto entities = registry.View<Position, Velocity>();
        
        for (EntityID entity : entities) {
            Position* pos = registry.GetComponent<Position>(entity);
            Velocity* vel = registry.GetComponent<Velocity>(entity);
            
            if (pos && vel) {
                // Update position based on velocity
                pos->x += vel->x * deltaTime;
                pos->y += vel->y * deltaTime;
                pos->z += vel->z * deltaTime;
            }
        }
    }
};

// Example system - demonstrates deterministic behavior
class HealthSystem : public ISystem {
public:
    HealthSystem() {
        SetPriority(200); // Runs after movement
    }
    
    void Update(float deltaTime, Registry& registry, uint64_t tickNumber) override {
        // This system would handle health regeneration, damage, etc.
        // For now, just demonstrate it runs
        auto entities = registry.View<Health>();
        
        // Example: regenerate health over time
        for (EntityID entity : entities) {
            Health* health = registry.GetComponent<Health>(entity);
            if (health && health->current < health->max) {
                health->current += 1.0f * deltaTime; // 1 HP per second
                if (health->current > health->max) {
                    health->current = health->max;
                }
            }
        }
    }
};

int main() {
    ::std::cout << "Engine - Basic Example\n";
    ::std::cout << "==================================\n\n";
    
    // Create registry
    Registry registry;
    
    // Create system manager
    SystemManager systemManager;
    
    // Register systems
    systemManager.RegisterSystem(::std::make_shared<MovementSystem>());
    systemManager.RegisterSystem(::std::make_shared<HealthSystem>());
    
    // Initialize systems
    systemManager.Initialize(registry);
    
    // Create some entities
    Entity player = registry.CreateEntity();
    auto& playerPos = registry.AddComponent<Position>(player.id);
    auto& playerVel = registry.AddComponent<Velocity>(player.id);
    auto& playerHealth = registry.AddComponent<Health>(player.id);
    
    playerPos = Position(0.0f, 0.0f, 0.0f);
    playerVel = Velocity(5.0f, 0.0f, 0.0f); // Moving at 5 units/sec in X
    playerHealth = Health(100.0f);
    
    // Create an NPC
    Entity npc = registry.CreateEntity();
    auto& npcPos = registry.AddComponent<Position>(npc.id);
    auto& npcVel = registry.AddComponent<Velocity>(npc.id);
    auto& npcHealth = registry.AddComponent<Health>(npc.id);
    
    npcPos = Position(10.0f, 0.0f, 0.0f);
    npcVel = Velocity(-2.0f, 0.0f, 0.0f);
    npcHealth = Health(50.0f);
    
    ::std::cout << "Initial state:\n";
    ::std::cout << "Player position: (" << playerPos.x << ", " << playerPos.y << ", " << playerPos.z << ")\n";
    ::std::cout << "NPC position: (" << npcPos.x << ", " << npcPos.y << ", " << npcPos.z << ")\n";
    ::std::cout << "NPC health: " << npcHealth.current << "/" << npcHealth.max << "\n\n";
    
    // Simulate a few ticks manually (for demonstration)
    constexpr float TICK_TIME = 1.0f / 60.0f;
    uint64_t tickNumber = 0;
    
    for (int i = 0; i < 5; ++i) {
        systemManager.Update(TICK_TIME, registry, tickNumber);
        tickNumber++;
        
        ::std::cout << "After tick " << tickNumber << ":\n";
        ::std::cout << "Player position: (" << playerPos.x << ", " << playerPos.y << ", " << playerPos.z << ")\n";
        ::std::cout << "NPC position: (" << npcPos.x << ", " << npcPos.y << ", " << npcPos.z << ")\n";
        ::std::cout << "NPC health: " << npcHealth.current << "/" << npcHealth.max << "\n\n";
    }
    
    // Cleanup
    systemManager.Shutdown(registry);
    
    ::std::cout << "Example completed successfully!\n";
    return 0;
}

