#pragma once

#include "core/Types.h"
#include <vector>
#include <bitset>
#include <cstdint>

namespace Engine {
namespace ECS {

// Maximum number of component types (adjust as needed)
constexpr size_t MAX_COMPONENT_TYPES = 64;

// Entity signature - which components does it have?
using ComponentSignature = std::bitset<MAX_COMPONENT_TYPES>;

// Entity is just an ID + signature
struct Entity {
    EntityID id = INVALID_ENTITY;
    ComponentSignature signature;
    
    bool IsValid() const { return id != INVALID_ENTITY; }
    bool HasComponent(ComponentTypeID typeID) const {
        return signature.test(typeID);
    }
};

} // namespace ECS
} // namespace Engine

