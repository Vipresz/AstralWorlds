#pragma once

#include "core/ecs/Entity.h"
#include "core/ecs/ComponentPool.h"
#include "core/Types.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <cassert>

namespace Engine {
namespace ECS {

// ECS Registry - manages entities and components
class Registry {
public:
    // Entity management
    Entity CreateEntity() {
        EntityID id = m_nextEntityID++;
        Entity entity;
        entity.id = id;
        return entity;
    }
    
    void DestroyEntity(EntityID entity) {
        // Remove all components for this entity
        for (auto& [typeID, pool] : m_componentPools) {
            if (pool->Has(entity)) {
                pool->Destroy(entity);
            }
        }
        
        // Remove entity signature
        auto it = m_entities.find(entity);
        if (it != m_entities.end()) {
            m_entities.erase(it);
        }
    }
    
    // Component management
    template<typename T>
    T& AddComponent(EntityID entity) {
        ComponentTypeID typeID = TypeID<T>::Get();
        
        // Create pool if it doesn't exist
        if (m_componentPools.find(typeID) == m_componentPools.end()) {
            m_componentPools[typeID] = std::make_unique<ComponentPool<T>>();
        }
        
        // Get or create entity signature
        Entity& e = GetOrCreateEntity(entity);
        e.signature.set(typeID);
        
        // Add component
        auto* pool = static_cast<ComponentPool<T>*>(m_componentPools[typeID].get());
        return pool->Add(entity);
    }
    
    template<typename T>
    void RemoveComponent(EntityID entity) {
        ComponentTypeID typeID = TypeID<T>::Get();
        
        auto poolIt = m_componentPools.find(typeID);
        if (poolIt != m_componentPools.end()) {
            poolIt->second->Destroy(entity);
            
            // Update entity signature
            auto entityIt = m_entities.find(entity);
            if (entityIt != m_entities.end()) {
                entityIt->second.signature.reset(typeID);
            }
        }
    }
    
    template<typename T>
    T* GetComponent(EntityID entity) {
        ComponentTypeID typeID = TypeID<T>::Get();
        
        auto poolIt = m_componentPools.find(typeID);
        if (poolIt != m_componentPools.end()) {
            auto* pool = static_cast<ComponentPool<T>*>(poolIt->second.get());
            return pool->Get(entity);
        }
        return nullptr;
    }
    
    template<typename T>
    bool HasComponent(EntityID entity) const {
        ComponentTypeID typeID = TypeID<T>::Get();
        
        auto poolIt = m_componentPools.find(typeID);
        if (poolIt != m_componentPools.end()) {
            return poolIt->second->Has(entity);
        }
        return false;
    }
    
    template<typename... Components>
    std::vector<EntityID> View() {
        std::vector<EntityID> result;
        
        // Build signature that matches all required components
        ComponentSignature requiredSignature;
        ((requiredSignature.set(TypeID<Components>::Get())), ...);
        
        // Find all entities matching this signature
        for (const auto& [entityID, entity] : m_entities) {
            if ((entity.signature & requiredSignature) == requiredSignature) {
                result.push_back(entityID);
            }
        }
        
        return result;
    }
    
    size_t GetEntityCount() const {
        return m_entities.size();
    }
    
private:
    EntityID m_nextEntityID = 1;
    std::unordered_map<EntityID, Entity> m_entities;
    std::unordered_map<ComponentTypeID, std::unique_ptr<IComponentPool>> m_componentPools;
    
    Entity& GetOrCreateEntity(EntityID id) {
        auto it = m_entities.find(id);
        if (it == m_entities.end()) {
            Entity entity;
            entity.id = id;
            m_entities[id] = entity;
            return m_entities[id];
        }
        return it->second;
    }
};

} // namespace ECS
} // namespace Engine

