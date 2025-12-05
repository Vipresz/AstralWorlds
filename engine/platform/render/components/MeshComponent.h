#pragma once

namespace Engine {
namespace Platform {
namespace Render {
    class Model;
}
}
}

namespace Engine {
namespace Platform {
namespace Render {
namespace Components {

// Mesh component - stores model pointer
// Note: Model pointer is stored externally and managed separately
// This component just holds a pointer to avoid ownership issues with ECS
// Must be POD/trivially copyable for ECS storage
struct MeshComponent {
    Model* model = nullptr;  // Forward declaration allows pointer
    
    MeshComponent() = default;
    
    bool IsLoaded() const { return model != nullptr; }
};

} // namespace Components
} // namespace Render
} // namespace Platform
} // namespace Engine

