#pragma once

#include "core/Types.h"
#include <cstdint>

namespace Engine {
namespace ECS {

// Base component interface - components are POD types
struct IComponent {
    virtual ~IComponent() = default;
};

// Component storage uses SoA (Structure of Arrays) for cache efficiency
// This header just defines the interface - implementation is in ComponentPool

} // namespace ECS
} // namespace Engine

