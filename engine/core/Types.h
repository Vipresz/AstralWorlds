#pragma once

#include <cstdint>
#include <cstddef>

namespace Engine {

// Forward declarations
using EntityID = uint32_t;
using ComponentTypeID = uint32_t;
using SystemTypeID = uint32_t;

// Constants
constexpr EntityID INVALID_ENTITY = UINT32_MAX;
constexpr float FIXED_DELTA_TIME = 1.0f / 60.0f; // 60 ticks/sec
constexpr uint32_t MAX_TICKS_PER_FRAME = 4; // Prevent spiral of death
constexpr uint32_t TICKS_PER_SECOND = 60;

// Type IDs for components/systems
// Use a shared counter so each type gets a unique ID
namespace TypeIDDetail {
    inline ComponentTypeID& GetNextTypeID() {
        static ComponentTypeID nextID = 1;
        return nextID;
    }
}

template<typename T>
struct TypeID {
    static ComponentTypeID Get() {
        static ComponentTypeID id = TypeIDDetail::GetNextTypeID()++;
        return id;
    }
};

} // namespace Engine

