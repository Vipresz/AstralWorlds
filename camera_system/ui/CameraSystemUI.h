#pragma once

#include "../nuklear/NuklearUI.h"
#include "../world/World.h"
#include <GLFW/glfw3.h>
#include <string>
#include <vector>
#include <cstddef>

// Forward declarations
struct nk_context;
struct nk_glfw;

namespace Engine {
namespace Editors {
namespace CameraSystem {

// Forward declaration
class CameraSystemEditor;

/**
 * UI class for CameraSystemEditor
 * Handles all Nuklear UI rendering
 */
class CameraSystemUI {
public:
    CameraSystemUI();
    ~CameraSystemUI();
    
    /**
     * Initialize the UI system
     * @param nuklearUI Reference to NuklearUI instance
     * @param glfwWindow GLFW window pointer
     */
    void Initialize(NuklearUI* nuklearUI, GLFWwindow* glfwWindow);
    
    /**
     * Render all UI elements
     * @param editor Reference to the editor instance (for accessing state and callbacks)
     */
    void Render(CameraSystemEditor& editor);
    
    /**
     * Check if UI is initialized
     */
    bool IsInitialized() const { return m_initialized; }

private:
    // UI state
    NuklearUI* m_nuklearUI;
    GLFWwindow* m_glfwWindow;
    bool m_initialized;
    
    // Helper methods for rendering different UI sections
    void RenderStatusBar(CameraSystemEditor& editor);       // Top menu bar
    void RenderBottomStatusBar(CameraSystemEditor& editor); // Bottom status bar
    void RenderCameraControls(CameraSystemEditor& editor);
    void RenderTOFControls(CameraSystemEditor& editor);
    void RenderVIPRESControls(CameraSystemEditor& editor);
    void RenderObjectsPanel(CameraSystemEditor& editor);
    void RenderStatisticsPanel(CameraSystemEditor& editor);
    
    // Helper to get window dimensions
    void GetWindowSize(int& width, int& height);
};

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

