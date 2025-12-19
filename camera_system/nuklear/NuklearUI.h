#pragma once

#include <GL/glew.h>
#include <GLFW/glfw3.h>

// Forward declarations
struct nk_glfw;
struct nk_context;

namespace Engine {
namespace Editors {
namespace CameraSystem {

class NuklearUI {
public:
    NuklearUI();
    ~NuklearUI();
    
    // Initialize Nuklear
    bool Initialize(GLFWwindow* window);
    
    // Shutdown Nuklear
    void Shutdown();
    
    // Get Nuklear context
    struct nk_context* GetContext() { return m_nkContext; }
    const struct nk_context* GetContext() const { return m_nkContext; }
    
    // Check if initialized
    bool IsInitialized() const { return m_nuklearInitialized; }
    
    // Begin/end frame
    void BeginFrame();
    void EndFrame();
    
    // Render with custom buffer sizes (matches old version behavior)
    void Render(int maxVertexBuffer, int maxElementBuffer);
    
    // Get nuklear instance for direct rendering (matches old version)
    struct nk_glfw* GetNuklear() { return m_nuklear; }
    
private:
    struct nk_glfw* m_nuklear;
    struct nk_context* m_nkContext;
    bool m_nuklearInitialized;
    GLFWwindow* m_window;
};

} // namespace CameraSystem
} // namespace Editors
}



