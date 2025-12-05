#pragma once

#include "core/ecs/Component.h"
#include "core/Types.h"
#include <vector>
#include <unordered_map>
#include <memory>
#include <cassert>

namespace Engine {
namespace ECS {

// Base component pool interface
class IComponentPool {
public:
    virtual ~IComponentPool() = default;
    virtual void Destroy(EntityID entity) = 0;
    virtual bool Has(EntityID entity) const = 0;
    virtual size_t Size() const = 0;
};

// Type-specific component pool (Structure of Arrays approach)
template<typename T>
class ComponentPool : public IComponentPool {
public:
    static_assert(std::is_pod_v<T> || std::is_trivially_copyable_v<T>,
                  "Components should be POD or trivially copyable");

    T& Add(EntityID entity) {
        assert(entity != INVALID_ENTITY);
        assert(m_entityToIndex.find(entity) == m_entityToIndex.end());
        
        size_t index = m_components.size();
        m_components.push_back(T{});
        m_indexToEntity.push_back(entity);
        m_entityToIndex[entity] = index;
        
        return m_components[index];
    }
    
    void Remove(EntityID entity) {
        assert(Has(entity));
        
        size_t indexOfRemoved = m_entityToIndex[entity];
        size_t indexOfLast = m_components.size() - 1;
        EntityID entityOfLast = m_indexToEntity[indexOfLast];
        
        // Swap with last element for O(1) removal
        m_components[indexOfRemoved] = std::move(m_components[indexOfLast]);
        m_indexToEntity[indexOfRemoved] = entityOfLast;
        m_entityToIndex[entityOfLast] = indexOfRemoved;
        
        // Remove last element
        m_components.pop_back();
        m_indexToEntity.pop_back();
        m_entityToIndex.erase(entity);
    }
    
    T* Get(EntityID entity) {
        auto it = m_entityToIndex.find(entity);
        if (it != m_entityToIndex.end()) {
            return &m_components[it->second];
        }
        return nullptr;
    }
    
    const T* Get(EntityID entity) const {
        auto it = m_entityToIndex.find(entity);
        if (it != m_entityToIndex.end()) {
            return &m_components[it->second];
        }
        return nullptr;
    }
    
    bool Has(EntityID entity) const override {
        return m_entityToIndex.find(entity) != m_entityToIndex.end();
    }
    
    void Destroy(EntityID entity) override {
        Remove(entity);
    }
    
    size_t Size() const override {
        return m_components.size();
    }
    
    // Iteration support
    std::vector<T>& GetComponents() { return m_components; }
    const std::vector<T>& GetComponents() const { return m_components; }
    
private:
    std::vector<T> m_components;                                    // Component data (SoA)
    std::vector<EntityID> m_indexToEntity;                          // Map index -> entity
    std::unordered_map<EntityID, size_t> m_entityToIndex;          // Map entity -> index
};

} // namespace ECS
} // namespace Engine

