#pragma once

#include "core/math/DeterministicMath.h"
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace Engine {
namespace Systems {
namespace Economy {

using namespace Math;

// Resource types
using ResourceType = uint32_t;

// Resource flow connection (like pipes/conveyors)
struct ResourceConnection {
    EntityID fromEntity;
    EntityID toEntity;
    float flowRate;        // Units per second
    ResourceType resourceType;
    float currentAmount;   // Currently flowing
    
    ResourceConnection() 
        : fromEntity(0)
        , toEntity(0)
        , flowRate(10.0f)
        , resourceType(0)
        , currentAmount(0.0f)
    {}
};

// Resource node (extraction point - ore, trees, etc.)
struct ResourceNodeComponent {
    ResourceType type;
    float remainingAmount;
    float maxAmount;
    float extractionRate;      // Units per second
    bool isDepleted;
    float respawnTime;         // Time until respawn (0 = never respawns)
    float timeUntilRespawn;
    
    ResourceNodeComponent()
        : type(0)
        , remainingAmount(100.0f)
        , maxAmount(100.0f)
        , extractionRate(1.0f)
        , isDepleted(false)
        , respawnTime(0.0f)
        , timeUntilRespawn(0.0f)
    {}
    
    float Extract(float deltaTime) {
        if (isDepleted) return 0.0f;
        
        float extracted = std::min(extractionRate * deltaTime, remainingAmount);
        remainingAmount -= extracted;
        
        if (remainingAmount <= 0.0f) {
            isDepleted = true;
            if (respawnTime > 0.0f) {
                timeUntilRespawn = respawnTime;
            }
        }
        
        return extracted;
    }
    
    void Update(float deltaTime) {
        if (isDepleted && respawnTime > 0.0f) {
            timeUntilRespawn -= deltaTime;
            if (timeUntilRespawn <= 0.0f) {
                remainingAmount = maxAmount;
                isDepleted = false;
                timeUntilRespawn = 0.0f;
            }
        }
    }
};

// Resource storage (containers, warehouses)
struct ResourceStorageComponent {
    std::unordered_map<ResourceType, float> storedResources;
    std::unordered_map<ResourceType, float> capacity;
    
    bool CanStore(ResourceType type, float amount) const {
        auto it = capacity.find(type);
        if (it == capacity.end()) return true; // No limit
        
        float current = GetAmount(type);
        return (current + amount) <= it->second;
    }
    
    float GetAmount(ResourceType type) const {
        auto it = storedResources.find(type);
        return (it != storedResources.end()) ? it->second : 0.0f;
    }
    
    bool Add(ResourceType type, float amount) {
        if (!CanStore(type, amount)) return false;
        storedResources[type] += amount;
        return true;
    }
    
    bool Remove(ResourceType type, float amount) {
        float current = GetAmount(type);
        if (current < amount) return false;
        storedResources[type] -= amount;
        if (storedResources[type] <= 0.0f) {
            storedResources.erase(type);
        }
        return true;
    }
    
    void SetCapacity(ResourceType type, float capacity) {
        this->capacity[type] = capacity;
    }
};

// Resource processor (converts resources)
struct ResourceProcessorComponent {
    struct Recipe {
        std::unordered_map<ResourceType, float> inputs;
        std::unordered_map<ResourceType, float> outputs;
        float processingTime;
        float timeRemaining;
    };
    
    std::vector<Recipe> recipes;
    Recipe* activeRecipe;
    float processingSpeed;     // Multiplier (1.0 = normal)
    
    ResourceProcessorComponent()
        : activeRecipe(nullptr)
        , processingSpeed(1.0f)
    {}
};

// Resource flow system - handles extraction, transport, processing
class ResourceFlowSystem {
public:
    void Update(float deltaTime) {
        // 1. Update resource nodes (extraction, respawning)
        // 2. Process resource connections (flow through pipes/conveyors)
        // 3. Process resource processors (conversion)
        // 4. Update storage
    }
    
    // Extract from resource node
    float ExtractFromNode(ResourceNodeComponent& node, float deltaTime) {
        return node.Extract(deltaTime);
    }
    
    // Flow resources through connection
    void ProcessConnection(ResourceConnection& connection, ResourceStorageComponent& from, ResourceStorageComponent& to, float deltaTime) {
        if (!from.CanStore(connection.resourceType, 0.0f)) {
            float toMove = std::min(connection.flowRate * deltaTime, from.GetAmount(connection.resourceType));
            
            if (to.CanStore(connection.resourceType, toMove)) {
                if (from.Remove(connection.resourceType, toMove)) {
                    to.Add(connection.resourceType, toMove);
                }
            }
        }
    }
};

} // namespace Economy
} // namespace Systems
} // namespace Engine

