#include "VisualSkeletonEditor.h"
#include "../../engine/platform/render/Window.h"
#include "../../engine/platform/render/Shader.h"
#include "../../engine/platform/render/Model.h"
#include "../../engine/platform/render/Mesh.h"
#include "../../engine/platform/Logger.h"
#include <filesystem>

// Include Nuklear header (without NK_IMPLEMENTATION - that's in NuklearImpl.cpp)
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#include "../../../game_libs/Nuklear/nuklear.h"
#include "../../../game_libs/Nuklear/demo/glfw_opengl3/nuklear_glfw_gl3.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <functional>
#include <limits>
#include <map>
#include <set>
#include <cstring>
#include <filesystem>

using namespace Engine::Platform;
using namespace Engine::Platform::Render;
using EngineLogging::Logger;

namespace Engine {
namespace Editors {
namespace Skeleton {

MeshWeights::MeshWeights() {
}

void MeshWeights::SetWeight(size_t vertexIndex, int boneID, float weight) {
    if (vertexIndex >= m_vertexWeights.size()) {
        m_vertexWeights.resize(vertexIndex + 1);
    }
    
    auto& weights = m_vertexWeights[vertexIndex];
    
    for (auto& w : weights) {
        if (w.boneID == boneID) {
            w.weight = weight;
            NormalizeVertexWeights(vertexIndex);
            return;
        }
    }
    
    weights.push_back(VertexWeight(boneID, weight));
    
    if (weights.size() > 4) {
        std::sort(weights.begin(), weights.end(), 
            [](const VertexWeight& a, const VertexWeight& b) {
                return a.weight > b.weight;
            });
        weights.resize(4);
    }
    
    NormalizeVertexWeights(vertexIndex);
}

void MeshWeights::GetWeights(size_t vertexIndex, std::vector<VertexWeight>& weights) const {
    weights.clear();
    if (vertexIndex < m_vertexWeights.size()) {
        weights = m_vertexWeights[vertexIndex];
    }
}

const std::vector<VertexWeight>& MeshWeights::GetVertexWeights(size_t vertexIndex) const {
    static const std::vector<VertexWeight> empty;
    if (vertexIndex < m_vertexWeights.size()) {
        return m_vertexWeights[vertexIndex];
    }
    return empty;
}

void MeshWeights::Clear() {
    m_vertexWeights.clear();
}

void MeshWeights::Resize(size_t vertexCount) {
    m_vertexWeights.resize(vertexCount);
}

void MeshWeights::NormalizeVertexWeights(size_t vertexIndex) {
    if (vertexIndex >= m_vertexWeights.size()) {
        return;
    }
    
    auto& weights = m_vertexWeights[vertexIndex];
    if (weights.empty()) {
        return;
    }
    
    float total = 0.0f;
    for (const auto& w : weights) {
        total += w.weight;
    }
    
    if (total > 0.0001f) {
        for (auto& w : weights) {
            w.weight /= total;
        }
    }
}

float MeshWeights::GetTotalWeight(size_t vertexIndex) const {
    if (vertexIndex >= m_vertexWeights.size()) {
        return 0.0f;
    }
    
    float total = 0.0f;
    for (const auto& w : m_vertexWeights[vertexIndex]) {
        total += w.weight;
    }
    return total;
}

VisualSkeletonEditor::VisualSkeletonEditor()
    : m_window(nullptr)
    , m_meshShader(nullptr)
    , m_boneShader(nullptr)
    , m_mesh(nullptr)
    , m_mode(EditorMode::PlaceBones)
    , m_selectedBoneID(-1)
    , m_mouseLeftDown(false)
    , m_mouseRightDown(false)
    , m_lastMouseX(0.0)
    , m_lastMouseY(0.0)
    , m_paintRadius(0.1f)
    , m_paintIntensity(1.0f)
    , m_activePaintBoneID(-1)
    , m_cameraYaw(0.0f)
    , m_cameraPitch(0.3f)
    , m_cameraDistance(5.0f)
    , m_outputPath("skeleton.skel")
    , m_meshDataLoaded(false)
    , m_boneVAO(0)
    , m_boneVBO(0)
    , m_gridVAO(0)
    , m_gridVBO(0)
    , m_buffersInitialized(false)
    , m_nextBoneIndex(0)
    , m_mouseMiddleDown(false)
    , m_meshVisible(true)
    , m_contoursVisible(false)
    , m_contourInterval(1.0f)
    , m_boneMarkerSize(8.0f)
    , m_showBoneConnections(false)
    , m_scrollY(0.0)
    , m_nuklear(nullptr)
    , m_nkContext(nullptr)
    , m_nuklearInitialized(false)
    , m_showResetDialog(false)
    , m_animationsLoaded(false)
    , m_lastFrameTime(0.0)
{
    memset(m_meshFileInput, 0, sizeof(m_meshFileInput));
    memset(m_skeletonFileInput, 0, sizeof(m_skeletonFileInput));
    m_autoSave = false;
    
    m_cameraPos = glm::vec3(0.0f, 2.0f, 5.0f);
    m_cameraTarget = glm::vec3(0.0f, 0.0f, 0.0f);
    InitializeBoneOrder();
}

VisualSkeletonEditor::~VisualSkeletonEditor() {
    ShutdownNuklear();
    Cleanup();
}

bool VisualSkeletonEditor::Initialize(int windowWidth, int windowHeight) {
    LOG_INFO("VisualEditor", "Initializing visual skeleton editor...");
    std::cout << "Initializing visual skeleton editor..." << std::endl;
    
    // Create window
    LOG_INFO_FMT("VisualEditor", "Creating window: %dx%d", windowWidth, windowHeight);
    std::cout << "Creating window: " << windowWidth << "x" << windowHeight << std::endl;
    
    try {
        m_window = new Window(windowWidth, windowHeight);
        int initResult = m_window->Initialize();
        if (initResult != 0) {
            LOG_ERROR_FMT("VisualEditor", "Failed to initialize window (error code: %d)", initResult);
            std::cerr << "ERROR: Failed to initialize window (error code: " << initResult << ")" << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("VisualEditor", "Exception during window creation: %s", e.what());
        std::cerr << "EXCEPTION during window creation: " << e.what() << std::endl;
        return false;
    } catch (...) {
        LOG_ERROR("VisualEditor", "Unknown exception during window creation");
        std::cerr << "UNKNOWN EXCEPTION during window creation" << std::endl;
        return false;
    }
    LOG_INFO_FMT("VisualEditor", "Window initialized: %dx%d", m_window->getBufferWidth(), m_window->getBufferHeight());
    
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) {
        LOG_ERROR("VisualEditor", "Failed to get GLFW window context!");
        return false;
    }
    
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetWindowTitle(window, "Visual Skeleton Editor - Press S to save, ESC to exit");
    glfwSetWindowShouldClose(window, GLFW_FALSE);
    glfwShowWindow(window);
    
    if (!CreateShaders()) {
        LOG_ERROR("VisualEditor", "Failed to create shaders");
        return false;
    }
    
    float aspect = (float)windowWidth / (float)windowHeight;
    m_projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);
    glViewport(0, 0, windowWidth, windowHeight);
    UpdateCamera();
    
    m_mesh = new Model();
    
    if (!InitializeBuffers()) {
        LOG_ERROR("VisualEditor", "Failed to initialize OpenGL buffers");
        return false;
    }
    
    if (!InitializeNuklear()) {
        LOG_ERROR("VisualEditor", "Failed to initialize Nuklear GUI");
        return false;
    }
    
    if (window) {
        glfwSetWindowUserPointer(window, this);
        
        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* win, int width, int height) {
            VisualSkeletonEditor* editor = static_cast<VisualSkeletonEditor*>(glfwGetWindowUserPointer(win));
            if (!editor || !editor->m_window) {
                return;
            }
            
            glViewport(0, 0, width, height);
            float aspect = (float)width / (float)height;
            editor->m_projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);
            LOG_INFO_FMT("VisualEditor", "Window resized: %dx%d", width, height);
        });
        
        glfwSetScrollCallback(window, [](GLFWwindow* win, double xoffset, double yoffset) {
            VisualSkeletonEditor* editor = static_cast<VisualSkeletonEditor*>(glfwGetWindowUserPointer(win));
            if (!editor) {
                return;
            }
            
            double mouseX, mouseY;
            glfwGetCursorPos(win, &mouseX, &mouseY);
            
            const float UI_HEIGHT = 130.0f;
            if (editor->m_nuklearInitialized && editor->m_nuklear && mouseY < UI_HEIGHT) {
                editor->m_nuklear->scroll.x += static_cast<float>(xoffset);
                editor->m_nuklear->scroll.y += static_cast<float>(yoffset);
            } else {
                editor->m_scrollY += yoffset;
            }
        });
    }
    
    LOG_INFO("VisualEditor", "Visual skeleton editor initialized successfully");
    return true;
}

bool VisualSkeletonEditor::InitializeBuffers() {
    LOG_INFO("VisualEditor", "Initializing OpenGL buffers...");
    
    glGenVertexArrays(1, &m_gridVAO);
    glGenBuffers(1, &m_gridVBO);
    
    glBindVertexArray(m_gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);
    
    std::vector<float> gridVertices;
    gridVertices.reserve(200 * 3);
    
    glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(float), gridVertices.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
    
    glGenVertexArrays(1, &m_boneVAO);
    glGenBuffers(1, &m_boneVBO);
    
    glBindVertexArray(m_boneVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_boneVBO);
    
    std::vector<float> boneVertices;
    boneVertices.reserve(100 * 3);
    
    glBufferData(GL_ARRAY_BUFFER, boneVertices.size() * sizeof(float), boneVertices.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glBindVertexArray(0);
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR_FMT("VisualEditor", "OpenGL error during buffer initialization: 0x%x", error);
        return false;
    }
    
    m_buffersInitialized = true;
    LOG_INFO("VisualEditor", "OpenGL buffers initialized successfully");
    return true;
}

bool VisualSkeletonEditor::CreateShaders() {
    LOG_INFO("VisualEditor", "Compiling shaders...");
    const char* meshVertShader = R"(
        #version 330 core
        layout (location = 0) in vec3 position;
        layout (location = 1) in vec3 normal;
        layout (location = 2) in vec2 texCoord;
        
        uniform mat4 projection;
        uniform mat4 view;
        uniform mat4 model;
        uniform vec3 color;
        
        out vec3 FragColor;
        out vec3 FragNormal;
        out vec2 FragTexCoord;
        
        void main() {
            gl_Position = projection * view * model * vec4(position, 1.0);
            FragColor = color;
            FragNormal = mat3(transpose(inverse(model))) * normal;
            FragTexCoord = texCoord;
        }
    )";
    
    const char* meshFragShader = R"(
        #version 330 core
        in vec3 FragColor;
        in vec3 FragNormal;
        in vec2 FragTexCoord;
        
        out vec4 color;
        
        void main() {
            vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
            float diff = max(dot(normalize(FragNormal), lightDir), 0.3);
            color = vec4(FragColor * diff, 1.0);
        }
    )";
    
    m_meshShader = new Shader();
    m_meshShader->CreateFromString(meshVertShader, meshFragShader);
    
    const char* boneVertShader = R"(
        #version 330 core
        layout (location = 0) in vec3 position;
        
        uniform mat4 projection;
        uniform mat4 view;
        uniform mat4 model;
        uniform vec3 color;
        
        out vec3 FragColor;
        
        void main() {
            gl_Position = projection * view * model * vec4(position, 1.0);
            FragColor = color;
        }
    )";
    
    const char* boneFragShader = R"(
        #version 330 core
        in vec3 FragColor;
        out vec4 color;
        
        void main() {
            color = vec4(FragColor, 1.0);
        }
    )";
    
    m_boneShader = new Shader();
    m_boneShader->CreateFromString(boneVertShader, boneFragShader);
    
    bool valid = m_meshShader->IsValid() && m_boneShader->IsValid();
    if (valid) {
        LOG_INFO("VisualEditor", "Shaders compiled successfully");
    } else {
        LOG_ERROR("VisualEditor", "Shader compilation failed");
    }
    
    return valid;
}

bool VisualSkeletonEditor::LoadMesh(const std::string& filePath) {
    if (!m_mesh) {
        LOG_ERROR("VisualEditor", "Mesh object is null, cannot load");
        return false;
    }
    
    LOG_INFO_FMT("VisualEditor", "Loading mesh from: %s", filePath.c_str());
    try {
        m_mesh->LoadModel(filePath);
        LOG_INFO_FMT("VisualEditor", "Mesh loaded successfully (%zu meshes)", m_mesh->GetMeshCount());
        
        float minX, maxX, minY, maxY, minZ, maxZ;
        m_mesh->GetBounds(minX, maxX, minY, maxY, minZ, maxZ);
        
        glm::vec3 meshCenter(
            (minX + maxX) * 0.5f,
            (minY + maxY) * 0.5f,
            (minZ + maxZ) * 0.5f
        );
        
        float meshSizeX = maxX - minX;
        float meshSizeY = maxY - minY;
        float meshSizeZ = maxZ - minZ;
        float meshSize = std::max(meshSizeX, std::max(meshSizeY, meshSizeZ));
        
        m_cameraTarget = meshCenter;
        m_originalMeshOffset = meshCenter; // Store original mesh center
        
        m_cameraDistance = meshSize * 2.0f;
        m_cameraDistance = std::max(1.0f, std::min(200.0f, m_cameraDistance));
        
        UpdateCamera();
        
        LOG_INFO_FMT("VisualEditor", "Mesh bounds: X[%.2f, %.2f] Y[%.2f, %.2f] Z[%.2f, %.2f]", 
                     minX, maxX, minY, maxY, minZ, maxZ);
        LOG_INFO_FMT("VisualEditor", "Mesh center: (%.2f, %.2f, %.2f), size: %.2f", 
                     meshCenter.x, meshCenter.y, meshCenter.z, meshSize);
        LOG_INFO_FMT("VisualEditor", "Camera adjusted: distance=%.2f, target=(%.2f, %.2f, %.2f)", 
                     m_cameraDistance, m_cameraTarget.x, m_cameraTarget.y, m_cameraTarget.z);
        
        m_meshTriangles.clear();
        m_originalMeshTriangles.clear();
        m_originalVertexData.clear();
        m_meshVertexCounts.clear();
        LoadMeshDataForRaycasting(filePath);
        // Store original triangles as bind pose
        m_originalMeshTriangles = m_meshTriangles;
        // Store original vertex data from Model
        StoreOriginalVertexData(filePath);
        m_meshDataLoaded = !m_meshTriangles.empty();
        LOG_INFO_FMT("VisualEditor", "Loaded %zu triangles for raycasting", m_meshTriangles.size());
        
        // Try to load animations from the animation directory (relative to mesh file)
        std::filesystem::path meshPath(filePath);
        std::filesystem::path animDir = meshPath.parent_path().parent_path() / "animation" / "regular";
        if (std::filesystem::exists(animDir)) {
            LoadAnimationsFromDirectory(animDir.string());
        } else {
            // Try alternative path structure
            animDir = meshPath.parent_path() / "animation" / "regular";
            if (std::filesystem::exists(animDir)) {
                LoadAnimationsFromDirectory(animDir.string());
            }
        }
        
        // Store mesh file path as absolute path
        try {
            std::filesystem::path path(filePath);
            if (path.is_relative()) {
                path = std::filesystem::absolute(path);
            }
            // Normalize the path (remove redundant separators, resolve . and ..)
            path = path.lexically_normal();
            m_meshFilePath = path.string();
        } catch (...) {
            // If filesystem operations fail, use original path
            m_meshFilePath = filePath;
        }
        
        // Don't update input field here - let user edit it freely
        // Only update after explicit load action
        
        // Try to auto-load skeleton backup from same location
        std::string backupPath = filePath;
        size_t lastDot = backupPath.find_last_of('.');
        if (lastDot != std::string::npos) {
            backupPath = backupPath.substr(0, lastDot) + ".skel";
        } else {
            backupPath += ".skel";
        }
        
        // Convert backup path to absolute and store
        try {
            std::filesystem::path path(backupPath);
            if (path.is_relative()) {
                path = std::filesystem::absolute(path);
            }
            path = path.lexically_normal();
            backupPath = path.string();
            m_skeletonFilePath = backupPath;
        } catch (...) {
            m_skeletonFilePath = backupPath;
        }
        
        // Try to load existing skeleton
        if (LoadRig(backupPath)) {
            LOG_INFO_FMT("VisualEditor", "Auto-loaded skeleton from: %s", m_skeletonFilePath.c_str());
            StoreBindPose(); // Store bind pose after loading skeleton
            UpdateVisualBones();
        } else {
            LOG_INFO_FMT("VisualEditor", "No existing skeleton found at: %s", backupPath.c_str());
            // Create empty skeleton file if it doesn't exist
            try {
                std::filesystem::path skelPath(backupPath);
                std::filesystem::path parentDir = skelPath.parent_path();
                
                // Create parent directory if it doesn't exist
                if (!parentDir.empty() && !std::filesystem::exists(parentDir)) {
                    std::filesystem::create_directories(parentDir);
                    LOG_INFO_FMT("VisualEditor", "Created directory: %s", parentDir.string().c_str());
                }
                
                // Create empty skeleton file using SkeletonSerializer
                // This ensures the file format is correct
                if (m_skeletonEditor.SaveSkeleton(backupPath)) {
                    LOG_INFO_FMT("VisualEditor", "Created empty skeleton file: %s", backupPath.c_str());
                } else {
                    LOG_WARNING_FMT("VisualEditor", "Could not create skeleton file: %s", backupPath.c_str());
                }
            } catch (const std::exception& e) {
                LOG_ERROR_FMT("VisualEditor", "Exception creating skeleton file: %s", e.what());
            }
            
            // Update input field with full path only if not currently being edited
            // Use safe string copy to ensure null termination
            size_t copyLen = std::min(m_skeletonFilePath.length(), sizeof(m_skeletonFileInput) - 1);
            memcpy(m_skeletonFileInput, m_skeletonFilePath.c_str(), copyLen);
            m_skeletonFileInput[copyLen] = '\0';
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("VisualEditor", "Exception while loading mesh: %s", e.what());
        return false;
    }
}

void VisualSkeletonEditor::Run() {
    if (!m_window) {
        LOG_ERROR("VisualEditor", "Window is null, cannot run editor");
        return;
    }
    
    LOG_INFO("VisualEditor", "Entering main render loop");
    std::cout << "Entering main render loop..." << std::endl;
    std::cout << "Window should be visible now. If not, check that the window is not minimized." << std::endl;
    
    bool shouldClose = m_window->getShouldClose();
    LOG_INFO_FMT("VisualEditor", "Initial window state - shouldClose: %s", shouldClose ? "true" : "false");
    std::cout << "Initial window state - shouldClose: " << (shouldClose ? "true" : "false") << std::endl;
    
    if (shouldClose) {
        LOG_ERROR("VisualEditor", "Window is marked to close immediately! This shouldn't happen.");
        std::cerr << "ERROR: Window is marked to close immediately!" << std::endl;
        std::cerr << "This usually means the window was closed or failed to initialize properly." << std::endl;
        return;
    }
    
    int frameCount = 0;
    
    while (!m_window->getShouldClose()) {
        m_window->pollEvents();
        
        if (frameCount == 0) {
            LOG_INFO("VisualEditor", "First frame - window is open and rendering");
        }
        
        bool* keys = m_window->getsKeys();
        if (keys[GLFW_KEY_ESCAPE]) {
            LOG_INFO("VisualEditor", "ESC key pressed, closing window");
            GLFWwindow* window = glfwGetCurrentContext();
            if (window) {
                glfwSetWindowShouldClose(window, GLFW_TRUE);
            }
            break;
        }
        
        GLenum error = glGetError();
        if (error != GL_NO_ERROR && frameCount < 10) {
            LOG_WARNING_FMT("VisualEditor", "OpenGL error detected: %d (frame %d)", error, frameCount);
        }
        
        HandleMouseInput();
        HandleKeyboardInput();
        
        // Calculate delta time for animation updates
        double currentTime = glfwGetTime();
        float deltaTime = 0.0f;
        if (m_lastFrameTime > 0.0) {
            deltaTime = static_cast<float>(currentTime - m_lastFrameTime);
        }
        m_lastFrameTime = currentTime;
        
        // Update and apply animations
        if (m_animationsLoaded && !m_animationPaused) {
            m_animationPlayer.Update(deltaTime);
            ApplyAnimationToSkeleton();
        } else if (m_animationsLoaded && m_animationPaused) {
            // Apply animation at manual time when paused
            ApplyAnimationAtTime(m_manualAnimationTime);
        }
        
        // Apply mesh animation if enabled
        if (m_animateMesh) {
            ApplyMeshAnimation();
        }
        
        // Get current framebuffer size directly from GLFW to ensure accuracy
        GLFWwindow* glfwWindow = glfwGetCurrentContext();
        int currentWidth, currentHeight;
        if (glfwWindow) {
            glfwGetFramebufferSize(glfwWindow, &currentWidth, &currentHeight);
        } else {
            // Fallback to window class if GLFW context not available
            currentWidth = m_window->getBufferWidth();
            currentHeight = m_window->getBufferHeight();
        }
        
        UpdateCamera();
        
        // Always set viewport to full window size before rendering
        // This ensures the rendered area expands to fill the entire window
        glViewport(0, 0, currentWidth, currentHeight);
        
        // Disable scissor test to ensure full window rendering
        glDisable(GL_SCISSOR_TEST);
        
        glClearColor(0.15f, 0.15f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        
        if (!glfwGetCurrentContext()) {
            LOG_ERROR("VisualEditor", "OpenGL context lost!");
            break;
        }
        
        if (m_meshVisible) {
            RenderMesh();
        }
        if (m_contoursVisible && m_mesh) {
            RenderContours();
        }
        RenderGrid();
        RenderBones();
        
        if (m_nuklearInitialized && m_nkContext) {
            // Nuklear automatically handles window size changes via nk_glfw3_new_frame
            nk_glfw3_new_frame(m_nuklear);
            RenderUI();
            nk_glfw3_render(m_nuklear, NK_ANTI_ALIASING_ON, 512 * 1024, 128 * 1024);
            
            // Restore OpenGL state after Nuklear rendering
            // Nuklear may have changed the active shader program, viewport, and scissor test
            glViewport(0, 0, currentWidth, currentHeight);
            glDisable(GL_SCISSOR_TEST);
            glUseProgram(0);
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            glEnable(GL_DEPTH_TEST);
        } else {
            RenderUI();
        }
        
        m_window->swapBuffers();
        
        frameCount++;
        if (frameCount == 1) {
            LOG_INFO("VisualEditor", "First frame rendered successfully");
        }
        if (frameCount == 10) {
            LOG_INFO("VisualEditor", "10 frames rendered - window should be stable");
        }
        
        // Small delay to prevent CPU spinning (optional, but helps with debugging)
        // glfwWaitEventsTimeout(0.001); // Wait up to 1ms for events
    }
    
    LOG_INFO("VisualEditor", "Exiting main render loop");
    LOG_INFO("VisualEditor", "Window close was requested");
}

void VisualSkeletonEditor::RenderMesh() {
    if (!m_meshShader || !m_meshShader->IsValid()) {
        return;
    }
    
    GLuint shaderID = m_meshShader->GetShaderID();
    if (shaderID == 0) {
        LOG_ERROR("VisualEditor", "Mesh shader ID is 0");
        return;
    }
    
    // Validate shader program exists
    if (!glIsProgram(shaderID)) {
        LOG_ERROR("VisualEditor", "Mesh shader program ID is not a valid program");
        return;
    }
    
    m_meshShader->UseShader();
    
    // Check for errors after UseShader
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR_FMT("VisualEditor", "OpenGL error after UseShader (mesh): 0x%x", error);
        return;
    }
    
    // Get uniform locations and validate them
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint colorLoc = glGetUniformLocation(shaderID, "color");
    
    if (projLoc == -1 || viewLoc == -1 || modelLoc == -1 || colorLoc == -1) {
        LOG_WARNING("VisualEditor", "Invalid uniform locations in mesh shader");
        return;
    }
    
    // Check current program before setting uniforms
    GLint currentProgram;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    if (static_cast<GLuint>(currentProgram) != shaderID) {
        LOG_WARNING_FMT("VisualEditor", "Shader program mismatch: expected %u, got %d", shaderID, currentProgram);
        glUseProgram(shaderID);
    }
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projection));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_view));
    
    error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR_FMT("VisualEditor", "OpenGL error when setting mesh shader uniforms: 0x%x", error);
        return;
    }
    
    // Render mesh if loaded
    if (m_mesh && m_mesh->GetMeshCount() > 0) {
        glm::mat4 model = glm::mat4(1.0f);
        glm::vec3 color(0.8f, 0.8f, 0.9f);
        
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
        glUniform3fv(colorLoc, 1, glm::value_ptr(color));
        
        m_mesh->RenderModel();
    }
    // Grid is now rendered separately in the main loop
}

void VisualSkeletonEditor::RenderGrid() {
    if (!m_boneShader || !m_buffersInitialized || !m_boneShader->IsValid()) {
        return;
    }
    
    // Clear any pending OpenGL errors from previous operations
    while (glGetError() != GL_NO_ERROR) {}
    
    GLuint shaderID = m_boneShader->GetShaderID();
    if (shaderID == 0) {
        LOG_ERROR("VisualEditor", "Bone shader ID is 0");
        return;
    }
    
    // Validate shader program exists
    if (!glIsProgram(shaderID)) {
        LOG_ERROR("VisualEditor", "Bone shader program ID is not a valid program");
        return;
    }
    
    m_boneShader->UseShader();
    
    // Check for errors after UseShader
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR_FMT("VisualEditor", "OpenGL error after UseShader (bone): 0x%x", error);
        return;
    }
    
    // Get uniform locations and validate them
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint colorLoc = glGetUniformLocation(shaderID, "color");
    
    if (projLoc == -1 || viewLoc == -1 || modelLoc == -1 || colorLoc == -1) {
        LOG_WARNING("VisualEditor", "Invalid uniform locations in bone shader");
        return;
    }
    
    // Check current program before setting uniforms
    GLint currentProgram;
    glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);
    if (static_cast<GLuint>(currentProgram) != shaderID) {
        LOG_WARNING_FMT("VisualEditor", "Shader program mismatch: expected %u, got %d", shaderID, currentProgram);
        glUseProgram(shaderID);
    }
    
    glm::mat4 model = glm::mat4(1.0f);
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projection));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_view));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    
    error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR_FMT("VisualEditor", "OpenGL error when setting bone shader uniforms: 0x%x", error);
        return;
    }
    
    // Build grid vertices - use fixed size grid (not dependent on camera distance)
    // Grid should stay the same size in world space regardless of zoom
    // When zooming in, camera moves closer, so grid appears larger (perspective), not smaller
    std::vector<float> gridVertices;
    float gridSize = 50.0f; // Fixed grid size in world units (large enough to be visible at any zoom)
    int gridLines = 21; // -10 to +10
    
    // Grid lines
    for (int i = -10; i <= 10; i++) {
        float pos = (float)i * (gridSize / 10.0f);
        // X-axis lines (along Z)
        gridVertices.push_back(pos);
        gridVertices.push_back(0.0f);
        gridVertices.push_back(-gridSize);
        
        gridVertices.push_back(pos);
        gridVertices.push_back(0.0f);
        gridVertices.push_back(gridSize);
        
        // Z-axis lines (along X)
        gridVertices.push_back(-gridSize);
        gridVertices.push_back(0.0f);
        gridVertices.push_back(pos);
        
        gridVertices.push_back(gridSize);
        gridVertices.push_back(0.0f);
        gridVertices.push_back(pos);
    }
    
    // Axes (scaled to grid size)
    float axisLength = gridSize * 0.5f;
    // X axis (red) - start at origin
    gridVertices.push_back(0.0f);
    gridVertices.push_back(0.0f);
    gridVertices.push_back(0.0f);
    gridVertices.push_back(axisLength);
    gridVertices.push_back(0.0f);
    gridVertices.push_back(0.0f);
    
    // Y axis (green)
    gridVertices.push_back(0.0f);
    gridVertices.push_back(0.0f);
    gridVertices.push_back(0.0f);
    gridVertices.push_back(0.0f);
    gridVertices.push_back(axisLength);
    gridVertices.push_back(0.0f);
    
    // Z axis (blue)
    gridVertices.push_back(0.0f);
    gridVertices.push_back(0.0f);
    gridVertices.push_back(0.0f);
    gridVertices.push_back(0.0f);
    gridVertices.push_back(0.0f);
    gridVertices.push_back(axisLength);
    
    if (gridVertices.empty()) {
        return;
    }
    
    // Render grid lines (gray)
    glm::vec3 gridColor(0.3f, 0.3f, 0.3f);
    glUniform3fv(colorLoc, 1, glm::value_ptr(gridColor));
    
    error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_WARNING_FMT("VisualEditor", "OpenGL error after setting grid color: 0x%x", error);
    }
    
    glBindVertexArray(m_gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_gridVBO);
    
    // Ensure vertex attribute is enabled (VAO should handle this, but be safe after Nuklear)
    glEnableVertexAttribArray(0);
    
    error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_WARNING_FMT("VisualEditor", "OpenGL error after binding VAO/VBO: 0x%x", error);
        glBindVertexArray(0);
        return;
    }
    
    // Grid lines (first 84 vertices = 21 lines * 2 vertices * 2 directions)
    size_t gridLineCount = gridLines * 2 * 2; // 84 vertices
    size_t gridLineVertexCount = gridLineCount * 3; // 252 floats
    
    if (gridLineVertexCount <= gridVertices.size()) {
        glBufferData(GL_ARRAY_BUFFER, gridLineVertexCount * sizeof(float), gridVertices.data(), GL_DYNAMIC_DRAW);
        error = glGetError();
        if (error != GL_NO_ERROR) {
            LOG_WARNING_FMT("VisualEditor", "OpenGL error after glBufferData (grid lines): 0x%x", error);
            glBindVertexArray(0);
            return;
        }
        
        // Note: glLineWidth may not be supported on all systems, but 1.0f should be safe
        glLineWidth(1.0f);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(gridLineCount));
        error = glGetError();
        if (error != GL_NO_ERROR) {
            LOG_WARNING_FMT("VisualEditor", "OpenGL error after glDrawArrays (grid lines): 0x%x", error);
        }
    }
    
    // Render axes with different colors
    // Use 1.0f for line width to avoid GL_INVALID_VALUE errors on systems that don't support > 1.0
    // (Many modern systems only support line width of 1.0)
    glLineWidth(1.0f);
    
    // X axis (red)
    if (gridLineVertexCount + 6 <= gridVertices.size()) {
        glm::vec3 red(1.0f, 0.0f, 0.0f);
        glUniform3fv(colorLoc, 1, glm::value_ptr(red));
        glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(float), &gridVertices[gridLineVertexCount], GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, 2);
    }
    
    // Y axis (green)
    if (gridLineVertexCount + 12 <= gridVertices.size()) {
        glm::vec3 green(0.0f, 1.0f, 0.0f);
        glUniform3fv(colorLoc, 1, glm::value_ptr(green));
        glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(float), &gridVertices[gridLineVertexCount + 6], GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, 2);
    }
    
    // Z axis (blue)
    if (gridLineVertexCount + 18 <= gridVertices.size()) {
        glm::vec3 blue(0.0f, 0.0f, 1.0f);
        glUniform3fv(colorLoc, 1, glm::value_ptr(blue));
        glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(float), &gridVertices[gridLineVertexCount + 12], GL_DYNAMIC_DRAW);
        glDrawArrays(GL_LINES, 0, 2);
    }
    
    glBindVertexArray(0);
    
    // Check for OpenGL errors at the end
    error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_WARNING_FMT("VisualEditor", "OpenGL error in RenderGrid (end): 0x%x", error);
    }
}

void VisualSkeletonEditor::RenderBones() {
    if (!m_boneShader || !m_boneShader->IsValid()) {
        return;
    }
    
    UpdateVisualBones();
    
    GLuint shaderID = m_boneShader->GetShaderID();
    if (shaderID == 0) {
        LOG_ERROR("VisualEditor", "Bone shader ID is 0 in RenderBones");
        return;
    }
    
    // Validate shader program exists
    if (!glIsProgram(shaderID)) {
        LOG_ERROR("VisualEditor", "Bone shader program ID is not a valid program");
        return;
    }
    
    m_boneShader->UseShader();
    
    // Check for errors after UseShader
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR_FMT("VisualEditor", "OpenGL error after UseShader in RenderBones: 0x%x", error);
        return;
    }
    
    // Get uniform locations and validate them
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    
    if (projLoc == -1 || viewLoc == -1) {
        LOG_WARNING("VisualEditor", "Invalid uniform locations in bone shader");
        return;
    }
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projection));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_view));
    
    error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR_FMT("VisualEditor", "OpenGL error when setting uniforms in RenderBones: 0x%x", error);
        return;
    }
    
    for (const auto& bone : m_visualBones) {
        RenderBone(bone);
    }
}

void VisualSkeletonEditor::RenderBone(const VisualBone& bone) {
    if (!m_boneShader || !m_buffersInitialized || !m_boneShader->IsValid()) {
        return;
    }
    
    // Render green sphere at bone placement point (screen-space sized)
    RenderBonePlacementSphere(bone.endPos);
    
    // Render red line connecting parent to child if enabled
    if (m_showBoneConnections) {
        GLuint shaderID = m_boneShader->GetShaderID();
        if (shaderID == 0) return;
        
        GLint colorLoc = glGetUniformLocation(shaderID, "color");
        if (colorLoc == -1) return;
        
        // Set red color for bone connections
        glm::vec3 lineColor(1.0f, 0.0f, 0.0f); // Red
        glUniform3fv(colorLoc, 1, glm::value_ptr(lineColor));
        
        // Create line vertices from parent (startPos) to child (endPos)
        std::vector<float> lineVertices = {
            bone.startPos.x, bone.startPos.y, bone.startPos.z,
            bone.endPos.x, bone.endPos.y, bone.endPos.z
        };
        
        glBindVertexArray(m_boneVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_boneVBO);
        glBufferData(GL_ARRAY_BUFFER, lineVertices.size() * sizeof(float), lineVertices.data(), GL_DYNAMIC_DRAW);
        
        // Ensure vertex attribute is enabled
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        
        // Draw the line
        glLineWidth(1.0f);
        glDrawArrays(GL_LINES, 0, 2);
    }
}

void VisualSkeletonEditor::RenderBoneHandle(const glm::vec3& position, const glm::vec3& color, bool selected) {
    if (!m_boneShader || !m_buffersInitialized || !m_boneShader->IsValid()) {
        return;
    }
    
    float size = selected ? 0.08f : 0.05f;
    
    // Create a small quad for the handle (better than point which might not be visible)
    float halfSize = size * 0.5f;
    std::vector<float> vertices = {
        position.x - halfSize, position.y, position.z,
        position.x + halfSize, position.y, position.z,
        position.x, position.y - halfSize, position.z,
        position.x, position.y + halfSize, position.z,
        position.x, position.y, position.z - halfSize,
        position.x, position.y, position.z + halfSize
    };
    
    GLuint shaderID = m_boneShader->GetShaderID();
    if (shaderID == 0) {
        return;
    }
    
    // Validate shader program exists and is not deleted
    GLint deleteStatus;
    glGetProgramiv(shaderID, GL_DELETE_STATUS, &deleteStatus);
    if (deleteStatus == GL_TRUE) {
        return; // Shader deleted, skip rendering
    }
    
    m_boneShader->UseShader();
    
    // Check for errors after UseShader
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        return; // Skip if error
    }
    
    // Get uniform locations and validate them
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint colorLoc = glGetUniformLocation(shaderID, "color");
    
    if (projLoc == -1 || viewLoc == -1 || modelLoc == -1 || colorLoc == -1) {
        return; // Silently skip if uniforms are invalid
    }
    
    glm::mat4 model = glm::mat4(1.0f);
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projection));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_view));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(colorLoc, 1, glm::value_ptr(color));
    
    error = glGetError();
    if (error != GL_NO_ERROR) {
        return; // Skip if error setting uniforms
    }
    
    // Update VBO with handle vertices
    glBindVertexArray(m_boneVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_boneVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
    
    // Render as points
    glPointSize(size * 100.0f);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertices.size() / 3));
    
    glBindVertexArray(0);
}

void VisualSkeletonEditor::UpdateCamera() {
    // Calculate camera position from spherical coordinates
    // Camera orbits around m_cameraTarget at distance m_cameraDistance
    float x = m_cameraDistance * cosf(m_cameraPitch) * sinf(m_cameraYaw);
    float y = m_cameraDistance * sinf(m_cameraPitch);
    float z = m_cameraDistance * cosf(m_cameraPitch) * cosf(m_cameraYaw);
    
    // Camera position is target + offset (spherical coordinates)
    m_cameraPos = m_cameraTarget + glm::vec3(x, y, z);
    
    m_view = glm::lookAt(m_cameraPos, m_cameraTarget, glm::vec3(0.0f, 1.0f, 0.0f));
}

void VisualSkeletonEditor::HandleMouseInput() {
    // Get current GLFW window
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) return;
    
    // Check if Shift is pressed for panning (use GLFW directly for modifier keys)
    bool shiftPressed = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
    
    // Log Shift state changes (once when pressed/released)
    static bool lastShiftState = false;
    if (shiftPressed && !lastShiftState) {
        LOG_INFO("VisualEditor", "Shift pressed");
        lastShiftState = true;
    } else if (!shiftPressed && lastShiftState) {
        LOG_INFO("VisualEditor", "Shift released");
        lastShiftState = false;
    }
    
    // Get mouse position
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    
    // Convert window coordinates to framebuffer coordinates (important for high-DPI displays)
    int windowWidth, windowHeight;
    int framebufferWidth, framebufferHeight;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    
    if (windowWidth > 0 && windowHeight > 0 && framebufferWidth > 0 && framebufferHeight > 0) {
        // Scale mouse coordinates to framebuffer space
        mouseX = mouseX * (static_cast<double>(framebufferWidth) / static_cast<double>(windowWidth));
        mouseY = mouseY * (static_cast<double>(framebufferHeight) / static_cast<double>(windowHeight));
    }
    
    // Get mouse button states
    int leftButton = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT);
    int rightButton = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT);
    int middleButton = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE);
    
    bool leftPressed = (leftButton == GLFW_PRESS);
    bool rightPressed = (rightButton == GLFW_PRESS);
    bool middlePressed = (middleButton == GLFW_PRESS);
    
    // Check if mouse is over Nuklear UI - if so, don't process bone placement or camera
    bool mouseOverUI = false;
    if (m_nuklearInitialized && m_nkContext && m_window) {
        // Menu bar is at Y: 0-90, Next bone indicator is at Y: 90-130
        // Total UI area: Y: 0-130
        const float UI_HEIGHT = 130.0f;
        
        if (mouseY < UI_HEIGHT) {
            mouseOverUI = true;
        } else {
            // Also check if Nuklear is handling the click (e.g., buttons, sliders)
            // Check if any Nuklear window is being interacted with
            struct nk_window* menuBar = nk_window_find(m_nkContext, "MenuBar");
            struct nk_window* indicator = nk_window_find(m_nkContext, "NextBoneIndicator");
            
            if (menuBar && nk_input_is_mouse_hovering_rect(&m_nkContext->input, menuBar->bounds)) {
                mouseOverUI = true;
            }
            if (indicator && nk_input_is_mouse_hovering_rect(&m_nkContext->input, indicator->bounds)) {
                mouseOverUI = true;
            }
        }
    }
    
    // Handle Shift + Right Mouse for panning (only if not over UI)
    if (shiftPressed && rightPressed && !mouseOverUI) {
        glm::vec3 forward = glm::normalize(m_cameraTarget - m_cameraPos);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 up = glm::normalize(glm::cross(right, forward));
        
        float panSpeed = m_cameraDistance * 0.002f;
        float deltaX = static_cast<float>(mouseX - m_lastMouseX) * panSpeed;
        float deltaY = static_cast<float>(mouseY - m_lastMouseY) * panSpeed;
        
        m_cameraTarget += right * deltaX;
        m_cameraTarget += up * (-deltaY);
        UpdateCamera();
        
        m_mouseRightDown = true;
    }
    // Handle Right Mouse for camera rotation (only if not over UI and Shift not pressed)
    else if (rightPressed && !mouseOverUI && !shiftPressed) {
        if (m_mouseRightDown) {
            float deltaX = static_cast<float>(mouseX - m_lastMouseX) * 0.01f;
            float deltaY = static_cast<float>(mouseY - m_lastMouseY) * 0.01f;
            
            m_cameraYaw += deltaX;
            m_cameraPitch += deltaY;
            m_cameraPitch = std::max(-1.5f, std::min(1.5f, m_cameraPitch));
        }
        m_mouseRightDown = true;
    } else {
        m_mouseRightDown = false;
    }
    
    // Handle left click for bone placement/selection (only if not over UI)
    if (leftPressed && !m_mouseLeftDown && !mouseOverUI) {
        PickResult pick = PickMesh(mouseX, mouseY);
        
        if (pick.hit) {
            switch (m_mode) {
                case EditorMode::PlaceBones:
                    PlaceBoneAtPosition(pick.worldPos);
                    break;
                case EditorMode::SelectBones:
                    SelectBoneAtPosition(pick.worldPos);
                    break;
                case EditorMode::PaintWeights:
                    PaintWeightsAtPosition(pick.worldPos);
                    break;
                default:
                    break;
            }
        }
        m_mouseLeftDown = true;
    } else if (!leftPressed) {
        m_mouseLeftDown = false;
    }
    
    // Handle mouse wheel for zoom
    if (std::abs(m_scrollY) > 0.001) {
        float zoomSpeed = m_cameraDistance * 0.15f;
        m_cameraDistance += static_cast<float>(m_scrollY) * zoomSpeed;
        m_cameraDistance = std::max(0.5f, std::min(200.0f, m_cameraDistance));
        UpdateCamera();
        m_scrollY = 0.0;
        m_mouseMiddleDown = false;
    }
    
    // Handle middle click for bone removal
    if (m_mouseMiddleDown && !middlePressed && !mouseOverUI) {
        PickResult pick = PickMesh(mouseX, mouseY);
        if (pick.hit) {
            RemoveBoneAtPosition(pick.worldPos);
        }
        m_mouseMiddleDown = false;
    } else if (middlePressed && !m_mouseMiddleDown) {
        m_mouseMiddleDown = true;
    } else if (!middlePressed) {
        m_mouseMiddleDown = false;
    }
    
    m_lastMouseX = mouseX;
    m_lastMouseY = mouseY;
}

void VisualSkeletonEditor::HandleKeyboardInput() {
    bool* keys = m_window->getsKeys();
    
    // Get GLFW window for direct key access (modifier keys may not be in Window's key array)
    GLFWwindow* glfwWindow = glfwGetCurrentContext();
    
    // Check if Ctrl is pressed using GLFW directly (modifier keys may not be tracked in Window's key array)
    bool leftCtrl = false;
    bool rightCtrl = false;
    bool ctrlPressed = false;
    if (glfwWindow) {
        leftCtrl = (glfwGetKey(glfwWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS);
        rightCtrl = (glfwGetKey(glfwWindow, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
        ctrlPressed = (leftCtrl || rightCtrl);
    }
    
    
    // Mode switching
    static bool key1Pressed = false;
    static bool key2Pressed = false;
    static bool key3Pressed = false;
    static bool keySPressed = false;
    
    if (keys[GLFW_KEY_1] && !key1Pressed) {
        m_mode = EditorMode::PlaceBones;
        LOG_INFO("VisualEditor", "Mode: Place Bones");
        key1Pressed = true;
    } else if (!keys[GLFW_KEY_1]) {
        key1Pressed = false;
    }
    
    if (keys[GLFW_KEY_2] && !key2Pressed) {
        m_mode = EditorMode::SelectBones;
        LOG_INFO("VisualEditor", "Mode: Select Bones");
        key2Pressed = true;
    } else if (!keys[GLFW_KEY_2]) {
        key2Pressed = false;
    }
    
    if (keys[GLFW_KEY_3] && !key3Pressed) {
        m_mode = EditorMode::PaintWeights;
        LOG_INFO("VisualEditor", "Mode: Paint Weights");
        key3Pressed = true;
    } else if (!keys[GLFW_KEY_3]) {
        key3Pressed = false;
    }
    
    // Save skeleton (Ctrl+S) - use GLFW directly for letter keys when combined with Ctrl
    bool keyS = glfwWindow ? (glfwGetKey(glfwWindow, GLFW_KEY_S) == GLFW_PRESS) : keys[GLFW_KEY_S];
    if (keyS && ctrlPressed && !keySPressed) {
        // Use the same save logic as the button
        if (!m_skeletonFilePath.empty()) {
            if (SaveRig(m_skeletonFilePath)) {
                LOG_INFO_FMT("VisualEditor", "Manually saved skeleton to: %s", m_skeletonFilePath.c_str());
            } else {
                LOG_ERROR_FMT("VisualEditor", "Failed to save skeleton to: %s", m_skeletonFilePath.c_str());
            }
        } else if (!m_meshFilePath.empty()) {
            // Generate skeleton path from mesh path
            std::string savePath = m_meshFilePath;
            size_t lastDot = savePath.find_last_of('.');
            if (lastDot != std::string::npos) {
                savePath = savePath.substr(0, lastDot) + ".skel";
            } else {
                savePath += ".skel";
            }
            if (SaveRig(savePath)) {
                m_skeletonFilePath = savePath;
                LOG_INFO_FMT("VisualEditor", "Manually saved skeleton to: %s", savePath.c_str());
            } else {
                LOG_ERROR_FMT("VisualEditor", "Failed to save skeleton to: %s", savePath.c_str());
            }
        } else {
            LOG_WARNING("VisualEditor", "No skeleton file path available for saving");
        }
        keySPressed = true;
    } else if (!keyS || !ctrlPressed) {
        keySPressed = false;
    }
    
    // Delete selected bone
    if (keys[GLFW_KEY_DELETE] && m_selectedBoneID >= 0) {
        // Get bone name and remove
        const BoneDefinition* bone = m_skeletonEditor.GetSkeleton()->GetBone(m_selectedBoneID);
        if (bone) {
            LOG_INFO_FMT("VisualEditor", "Deleting bone: %s", bone->name.c_str());
            m_skeletonEditor.RemoveBone(bone->name);
            m_selectedBoneID = -1;
        }
    }
    
    // Toggle mesh visibility (Ctrl+M) - use GLFW directly for letter keys when combined with Ctrl
    static bool keyMPressed = false;
    bool keyM = glfwWindow ? (glfwGetKey(glfwWindow, GLFW_KEY_M) == GLFW_PRESS) : keys[GLFW_KEY_M];
    if (keyM && ctrlPressed && !keyMPressed) {
        m_meshVisible = !m_meshVisible;
        LOG_INFO_FMT("VisualEditor", "Mesh visibility: %s", m_meshVisible ? "ON" : "OFF");
        std::cout << "Mesh visibility: " << (m_meshVisible ? "ON" : "OFF") << std::endl;
        keyMPressed = true;
    } else if (!keyM || !ctrlPressed) {
        keyMPressed = false;
    }
    
    // Toggle contours visibility (Ctrl+V) - use GLFW directly for letter keys when combined with Ctrl
    static bool keyVPressed = false;
    bool keyV = glfwWindow ? (glfwGetKey(glfwWindow, GLFW_KEY_V) == GLFW_PRESS) : keys[GLFW_KEY_V];
    if (keyV && ctrlPressed && !keyVPressed) {
        m_contoursVisible = !m_contoursVisible;
        LOG_INFO_FMT("VisualEditor", "Contours visibility: %s", m_contoursVisible ? "ON" : "OFF");
        std::cout << "Contours visibility: " << (m_contoursVisible ? "ON" : "OFF") << std::endl;
        keyVPressed = true;
    } else if (!keyV || !ctrlPressed) {
        keyVPressed = false;
    }
    
    // ESC to exit
    if (keys[GLFW_KEY_ESCAPE]) {
        LOG_INFO("VisualEditor", "ESC pressed, closing editor");
    }
}

PickResult VisualSkeletonEditor::PickMesh(double mouseX, double mouseY) {
    PickResult result;
    result.hit = false;
    
    // Get current framebuffer size directly from GLFW to ensure accuracy (especially after window resize)
    GLFWwindow* glfwWindow = glfwGetCurrentContext();
    int width, height;
    if (glfwWindow) {
        glfwGetFramebufferSize(glfwWindow, &width, &height);
    } else {
        // Fallback to window class if GLFW context not available
        width = m_window->getBufferWidth();
        height = m_window->getBufferHeight();
    }
    
    // Convert screen coordinates to normalized device coordinates
    // Ensure we have valid dimensions to avoid division by zero
    if (width <= 0 || height <= 0) {
        return result; // Invalid dimensions, return no hit
    }
    
    float x = (2.0f * static_cast<float>(mouseX)) / static_cast<float>(width) - 1.0f;
    float y = 1.0f - (2.0f * static_cast<float>(mouseY)) / static_cast<float>(height);
    
    // Create ray in world space
    glm::vec4 rayClip(x, y, -1.0f, 1.0f);
    glm::mat4 invProjection = glm::inverse(m_projection);
    glm::vec4 rayEye = invProjection * rayClip;
    rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
    
    glm::mat4 invView = glm::inverse(m_view);
    glm::vec4 rayWorld = invView * rayEye;
    glm::vec3 rayDir = glm::normalize(glm::vec3(rayWorld));
    glm::vec3 rayOrigin = m_cameraPos;
    
    // If we have mesh triangles, do proper ray-triangle intersection
    if (m_meshDataLoaded && !m_meshTriangles.empty()) {
        float closestT = std::numeric_limits<float>::max();
        glm::vec3 closestHit(0.0f);
        glm::vec3 closestNormal(0.0f, 1.0f, 0.0f);
        size_t closestVertexIndex = 0;
        bool foundHit = false;
        
        // Test ray against all triangles
        for (size_t i = 0; i < m_meshTriangles.size(); i++) {
            const Triangle& tri = m_meshTriangles[i];
            float t;
            glm::vec3 hitPoint;
            
            if (RayTriangleIntersect(rayOrigin, rayDir, tri.v0, tri.v1, tri.v2, t, hitPoint)) {
                if (t > 0.001f && t < closestT) { // Avoid self-intersection
                    closestT = t;
                    closestHit = hitPoint;
                    closestNormal = tri.normal;
                    closestVertexIndex = i * 3; // Approximate vertex index
                    foundHit = true;
                }
            }
        }
        
        if (foundHit) {
            result.hit = true;
            result.worldPos = closestHit;
            result.normal = closestNormal;
            result.distance = closestT;
            result.vertexIndex = closestVertexIndex;
            LOG_INFO_FMT("VisualEditor", "Ray hit mesh at (%.2f, %.2f, %.2f)", 
                        closestHit.x, closestHit.y, closestHit.z);
        }
    } else {
        // Fallback: Simple intersection with a plane at y=0
        if (std::abs(rayDir.y) > 0.001f) {
            float t = -m_cameraPos.y / rayDir.y;
            if (t > 0.0f) {
                result.hit = true;
                result.worldPos = m_cameraPos + rayDir * t;
                result.normal = glm::vec3(0.0f, 1.0f, 0.0f);
                result.distance = t;
            }
        }
    }
    
    return result;
}

void VisualSkeletonEditor::PlaceBoneAtPosition(const glm::vec3& worldPos) {
    std::string boneName;
    std::string parentName = "";
    
    if (m_nextBoneIndex < static_cast<int>(m_expectedBoneOrder.size())) {
        std::string displayName = m_expectedBoneOrder[m_nextBoneIndex];
        boneName = ExtractBaseBoneName(displayName);
        
        std::map<std::string, std::string> boneHierarchy = {
            {"Hips", ""},
            {"Spine", "Hips"},
            {"Spine1", "Spine"},
            {"Spine2", "Spine1"},
            {"Neck", "Spine2"},
            {"Head", "Neck"},
            {"HeadTop_End", "Head"},
            {"LeftShoulder", "Spine2"},
            {"LeftArm", "LeftShoulder"},
            {"LeftForeArm", "LeftArm"},
            {"LeftHand", "LeftForeArm"},
            {"LeftHandThumb1", "LeftHand"},
            {"LeftHandThumb2", "LeftHandThumb1"},
            {"LeftHandThumb3", "LeftHandThumb2"},
            {"LeftHandThumb4", "LeftHandThumb3"},
            {"LeftHandIndex1", "LeftHand"},
            {"LeftHandIndex2", "LeftHandIndex1"},
            {"LeftHandIndex3", "LeftHandIndex2"},
            {"LeftHandIndex4", "LeftHandIndex3"},
            {"LeftHandMiddle1", "LeftHand"},
            {"LeftHandMiddle2", "LeftHandMiddle1"},
            {"LeftHandMiddle3", "LeftHandMiddle2"},
            {"LeftHandMiddle4", "LeftHandMiddle3"},
            {"LeftHandRing1", "LeftHand"},
            {"LeftHandRing2", "LeftHandRing1"},
            {"LeftHandRing3", "LeftHandRing2"},
            {"LeftHandRing4", "LeftHandRing3"},
            {"LeftHandPinky1", "LeftHand"},
            {"LeftHandPinky2", "LeftHandPinky1"},
            {"LeftHandPinky3", "LeftHandPinky2"},
            {"LeftHandPinky4", "LeftHandPinky3"},
            {"RightShoulder", "Spine2"},
            {"RightArm", "RightShoulder"},
            {"RightForeArm", "RightArm"},
            {"RightHand", "RightForeArm"},
            {"RightHandThumb1", "RightHand"},
            {"RightHandThumb2", "RightHandThumb1"},
            {"RightHandThumb3", "RightHandThumb2"},
            {"RightHandThumb4", "RightHandThumb3"},
            {"RightHandIndex1", "RightHand"},
            {"RightHandIndex2", "RightHandIndex1"},
            {"RightHandIndex3", "RightHandIndex2"},
            {"RightHandIndex4", "RightHandIndex3"},
            {"RightHandMiddle1", "RightHand"},
            {"RightHandMiddle2", "RightHandMiddle1"},
            {"RightHandMiddle3", "RightHandMiddle2"},
            {"RightHandMiddle4", "RightHandMiddle3"},
            {"RightHandRing1", "RightHand"},
            {"RightHandRing2", "RightHandRing1"},
            {"RightHandRing3", "RightHandRing2"},
            {"RightHandRing4", "RightHandRing3"},
            {"RightHandPinky1", "RightHand"},
            {"RightHandPinky2", "RightHandPinky1"},
            {"RightHandPinky3", "RightHandPinky2"},
            {"RightHandPinky4", "RightHandPinky3"},
            {"LeftUpLeg", "Hips"},
            {"LeftLeg", "LeftUpLeg"},
            {"LeftFoot", "LeftLeg"},
            {"LeftToeBase", "LeftFoot"},
            {"LeftToe_End", "LeftToeBase"},
            {"RightUpLeg", "Hips"},
            {"RightLeg", "RightUpLeg"},
            {"RightFoot", "RightLeg"},
            {"RightToeBase", "RightFoot"},
            {"RightToe_End", "RightToeBase"}
        };
        
        auto it = boneHierarchy.find(boneName);
        if (it != boneHierarchy.end()) {
            parentName = it->second;
        }
    } else {
        int boneCount = (int)m_skeletonEditor.GetSkeleton()->GetBoneCount();
        boneName = "Bone_" + std::to_string(boneCount);
        parentName = m_parentBoneName;
    }
    
    LOG_INFO_FMT("VisualEditor", "Placing bone '%s' at position (%.2f, %.2f, %.2f)", 
                 boneName.c_str(), worldPos.x, worldPos.y, worldPos.z);
    
    // Check if parent exists - if not, we need to place the parent first
    if (!parentName.empty()) {
        const BoneDefinition* parentBone = m_skeletonEditor.GetSkeleton()->GetBone(parentName);
        if (!parentBone) {
            // Parent doesn't exist - find the parent's index and set that as next bone
            for (int i = 0; i < static_cast<int>(m_expectedBoneOrder.size()); i++) {
                std::string expectedBone = ExtractBaseBoneName(m_expectedBoneOrder[i]);
                if (expectedBone == parentName) {
                    m_nextBoneIndex = i;
                    LOG_INFO_FMT("VisualEditor", "Parent '%s' not found, setting next bone index to %d to place parent first", 
                                 parentName.c_str(), m_nextBoneIndex);
                    return; // Don't place this bone yet, place parent first
                }
            }
            LOG_WARNING_FMT("VisualEditor", "Parent '%s' not found and not in expected bone order, cannot place '%s'", 
                           parentName.c_str(), boneName.c_str());
            return;
        }
    }
    
    // Try to add bone (or update if it already exists)
    bool boneAdded = false;
    const BoneDefinition* existingBone = m_skeletonEditor.GetSkeleton()->GetBone(boneName);
    if (existingBone) {
        // Bone already exists - just update position
        m_skeletonEditor.SetBonePosition(boneName, worldPos.x, worldPos.y, worldPos.z);
        boneAdded = true;
        LOG_INFO_FMT("VisualEditor", "Bone '%s' already exists, updated position", boneName.c_str());
    } else if (m_skeletonEditor.AddBone(boneName, parentName)) {
        // Bone doesn't exist - add it
        m_skeletonEditor.SetBonePosition(boneName, worldPos.x, worldPos.y, worldPos.z);
        boneAdded = true;
        LOG_INFO_FMT("VisualEditor", "Bone '%s' added successfully", boneName.c_str());
    } else {
        LOG_WARNING_FMT("VisualEditor", "Failed to add bone '%s'", boneName.c_str());
        return;
    }
    
    // Update tracking and rendering
    if (boneAdded) {
        // Find the actual index of this bone in the expected order (don't rely on m_nextBoneIndex)
        int actualIndex = -1;
        for (int i = 0; i < static_cast<int>(m_expectedBoneOrder.size()); i++) {
            std::string expectedBone = ExtractBaseBoneName(m_expectedBoneOrder[i]);
            if (expectedBone == boneName) {
                actualIndex = i;
                break;
            }
        }
        
        // Mark this bone as placed using the actual index
        if (actualIndex >= 0 && actualIndex < static_cast<int>(m_bonesPlaced.size())) {
            m_bonesPlaced[actualIndex] = true;
        } else if (actualIndex >= 0) {
            // Index is valid but m_bonesPlaced is too small - resize it
            if (actualIndex >= static_cast<int>(m_bonesPlaced.size())) {
                m_bonesPlaced.resize(actualIndex + 1, false);
                m_bonesPlaced[actualIndex] = true;
            }
        }
        
        // Remove from stored removed positions if it was there (bone has been restored)
        // Use base bone name to match the key format used when storing
        std::string baseBoneNameForErase = ExtractBaseBoneName(boneName);
        m_removedBonePositions.erase(baseBoneNameForErase);
        m_removedBonePositions.erase(boneName); // Also try with full name in case it was stored differently
        
        // Update next bone index and parent
        UpdateNextBoneIndex();
        m_parentBoneName = boneName;
        
        // Always update visual bones to reflect changes
        UpdateVisualBones();
        
        // Auto-save skeleton after each bone placement (if enabled)
        if (m_autoSave) {
            std::string savePath = m_skeletonFilePath.empty() ? "" : m_skeletonFilePath;
            if (savePath.empty() && !m_meshFilePath.empty()) {
                savePath = m_meshFilePath;
                size_t lastDot = savePath.find_last_of('.');
                if (lastDot != std::string::npos) {
                    savePath = savePath.substr(0, lastDot) + ".skel";
                } else {
                    savePath += ".skel";
                }
            }
            if (!savePath.empty()) {
                SaveRig(savePath);
                LOG_INFO_FMT("VisualEditor", "Auto-saved skeleton to: %s", savePath.c_str());
            }
        }
    }
}

float VisualSkeletonEditor::CalculateSphereRadius(const glm::vec3& position) const {
    // Get current framebuffer size for screen-space sizing
    GLFWwindow* glfwWindow = glfwGetCurrentContext();
    int width = 1280, height = 720;
    if (glfwWindow) {
        glfwGetFramebufferSize(glfwWindow, &width, &height);
    }
    
    // Calculate distance from camera to position
    float distance = glm::length(m_cameraPos - position);
    
    // Calculate world-space size to maintain constant pixel size on screen
    // Use the field of view and screen height to calculate proper world size
    float fov = 45.0f; // Match the perspective FOV
    float fovRad = glm::radians(fov);
    float radius = (m_boneMarkerSize / static_cast<float>(height)) * distance * 2.0f * tanf(fovRad * 0.5f);
    
    return radius;
}

void VisualSkeletonEditor::RemoveBoneAtPosition(const glm::vec3& worldPos) {
    // Ensure visual bones are up-to-date with skeleton state before removal
    UpdateVisualBones();
    
    // Find nearest bone to remove (must click inside the sphere)
    float minDist = 1000.0f;
    std::string nearestBoneName = "";
    std::string parentName = "";
    
    for (const auto& visualBone : m_visualBones) {
        // Calculate the sphere radius for this bone (varies with distance from camera)
        float sphereRadius = CalculateSphereRadius(visualBone.endPos);
        
        // Check if click is inside the sphere
        float dist = glm::distance(worldPos, visualBone.endPos);
        if (dist < minDist && dist <= sphereRadius) {
            // Use bone name directly from visual bone (more reliable than ID lookup)
            const BoneDefinition* bone = m_skeletonEditor.GetSkeleton()->GetBone(visualBone.name);
            if (bone) {
                // Verify this bone still exists in skeleton before selecting it
                minDist = dist;
                nearestBoneName = visualBone.name;
                parentName = bone->parentName;
            }
        }
    }
    
    if (nearestBoneName.empty()) {
        return; // No bone found to remove
    }
    
    // Verify bone still exists in skeleton before attempting removal
    const BoneDefinition* verifyBone = m_skeletonEditor.GetSkeleton()->GetBone(nearestBoneName);
    if (!verifyBone) {
        LOG_WARNING_FMT("VisualEditor", "Bone '%s' not found in skeleton, updating visual bones and retrying", nearestBoneName.c_str());
        // Update visual bones to sync with skeleton state
        UpdateVisualBones();
        return; // Bone doesn't exist, nothing to remove
    }
    
    // Find the index of the removed bone in the expected order
    int removedIndex = -1;
    for (int i = 0; i < static_cast<int>(m_expectedBoneOrder.size()); i++) {
        std::string expectedBone = ExtractBaseBoneName(m_expectedBoneOrder[i]);
        if (expectedBone == nearestBoneName) {
            removedIndex = i;
            break;
        }
    }
    
    LOG_INFO_FMT("VisualEditor", "Removing bone: %s (index: %d)", nearestBoneName.c_str(), removedIndex);
    
    // Recursively remove bone and all its children
    RemoveBoneAndChildren(nearestBoneName);
    
    // Check if removal was successful
    const BoneDefinition* checkBone = m_skeletonEditor.GetSkeleton()->GetBone(nearestBoneName);
    if (checkBone) {
        LOG_WARNING_FMT("VisualEditor", "Failed to remove bone: %s (still exists after removal attempt)", nearestBoneName.c_str());
        // Update visual bones to sync with skeleton state
        UpdateVisualBones();
        return; // Don't update UI if removal failed
    }
    
    // Update parent tracking
    if (m_parentBoneName == nearestBoneName) {
        m_parentBoneName = parentName;
    }
    
    // Mark the bone as not placed in the boolean list
    if (removedIndex >= 0 && removedIndex < static_cast<int>(m_bonesPlaced.size())) {
        m_bonesPlaced[removedIndex] = false;
    }
    
    // Always update visual bones to remove deleted bones from rendering
    UpdateVisualBones();
    
    // Update next bone index (will find first unplaced bone)
    UpdateNextBoneIndex();
    
    // Auto-save skeleton after bone removal (if enabled)
    if (m_autoSave) {
        std::string savePath = m_skeletonFilePath.empty() ? "" : m_skeletonFilePath;
        if (savePath.empty() && !m_meshFilePath.empty()) {
            savePath = m_meshFilePath;
            size_t lastDot = savePath.find_last_of('.');
            if (lastDot != std::string::npos) {
                savePath = savePath.substr(0, lastDot) + ".skel";
            } else {
                savePath += ".skel";
            }
        }
        if (!savePath.empty()) {
            SaveRig(savePath);
            LOG_INFO_FMT("VisualEditor", "Auto-saved skeleton to: %s", savePath.c_str());
        }
    }
}

void VisualSkeletonEditor::RemoveBoneAndChildren(const std::string& boneName) {
    const SkeletonDefinition* skeleton = m_skeletonEditor.GetSkeleton();
    
    // Get all children of this bone
    std::vector<std::string> children = skeleton->GetBoneChildren(boneName);
    
    // Recursively remove all children first (depth-first, so we remove leaves first)
    for (const std::string& childName : children) {
        RemoveBoneAndChildren(childName); // Recursive call
    }
    
    // Store the bone's position before removing it (for potential restoration)
    const BoneDefinition* bone = skeleton->GetBone(boneName);
    if (bone) {
        m_removedBonePositions[boneName] = bone->position;
        LOG_INFO_FMT("VisualEditor", "Stored position for removed bone '%s': (%.2f, %.2f, %.2f)", 
                    boneName.c_str(), bone->position.x, bone->position.y, bone->position.z);
    }
    
    // Now remove this bone (all children are gone, so it should succeed)
    bool removed = m_skeletonEditor.RemoveBone(boneName);
    if (removed) {
        // Find the index of the removed bone in the expected order
        int boneIndex = -1;
        for (int i = 0; i < static_cast<int>(m_expectedBoneOrder.size()); i++) {
            std::string expectedBone = ExtractBaseBoneName(m_expectedBoneOrder[i]);
            if (expectedBone == boneName) {
                boneIndex = i;
                break;
            }
        }
        
        // Mark the bone as not placed in the boolean list
        if (boneIndex >= 0 && boneIndex < static_cast<int>(m_bonesPlaced.size())) {
            m_bonesPlaced[boneIndex] = false;
        }
        
        LOG_INFO_FMT("VisualEditor", "Removed bone: %s (index: %d)", boneName.c_str(), boneIndex);
    } else {
        LOG_WARNING_FMT("VisualEditor", "Failed to remove bone: %s", boneName.c_str());
    }
}

void VisualSkeletonEditor::SelectBoneAtPosition(const glm::vec3& worldPos) {
    // Find nearest bone (must click inside the sphere)
    float minDist = 1000.0f;
    int nearestBoneID = -1;
    
    for (const auto& visualBone : m_visualBones) {
        // Calculate the sphere radius for this bone (varies with distance from camera)
        float sphereRadius = CalculateSphereRadius(visualBone.endPos);
        
        // Check if click is inside the sphere
        float dist = glm::distance(worldPos, visualBone.endPos);
        if (dist < minDist && dist <= sphereRadius) {
            minDist = dist;
            nearestBoneID = visualBone.boneID;
        }
    }
    
    m_selectedBoneID = nearestBoneID;
    if (nearestBoneID >= 0) {
        const BoneDefinition* bone = m_skeletonEditor.GetSkeleton()->GetBone(nearestBoneID);
        if (bone) {
            m_parentBoneName = bone->name;
            LOG_INFO_FMT("VisualEditor", "Selected bone: %s (ID: %d)", bone->name.c_str(), nearestBoneID);
        }
    } else {
        LOG_INFO("VisualEditor", "No bone selected (clicked too far from any bone)");
    }
}

glm::vec3 VisualSkeletonEditor::CalculateBoneWorldPosition(const std::string& boneName) const {
    const SkeletonDefinition* skeleton = m_skeletonEditor.GetSkeleton();
    if (!skeleton) return glm::vec3(0.0f);
    
    glm::mat4 worldTransform = glm::mat4(1.0f);
    
    // Traverse up the hierarchy from this bone to root
    std::string currentBone = boneName;
    while (!currentBone.empty()) {
        const BoneDefinition* bone = skeleton->GetBone(currentBone);
        if (!bone) break;
        
        // Get local transform and accumulate
        glm::mat4 localTransform = bone->GetLocalTransform();
        worldTransform = localTransform * worldTransform;
        
        // Move to parent
        currentBone = bone->parentName;
    }
    
    // Extract position from world transform matrix
    return glm::vec3(worldTransform[3]);
}

void VisualSkeletonEditor::UpdateVisualBones() {
    m_visualBones.clear();
    
    const SkeletonDefinition* skeleton = m_skeletonEditor.GetSkeleton();
    auto boneNames = skeleton->GetAllBoneNames();
    
    for (const auto& boneName : boneNames) {
        const BoneDefinition* bone = skeleton->GetBone(boneName);
        if (!bone) continue;
        
        VisualBone visualBone;
        visualBone.boneID = bone->id;
        visualBone.name = boneName;
        visualBone.selected = (bone->id == m_selectedBoneID);
        
        // Calculate world position by accumulating transforms up the hierarchy
        visualBone.endPos = CalculateBoneWorldPosition(boneName);
        
        // Find parent world position
        if (!bone->parentName.empty()) {
            visualBone.startPos = CalculateBoneWorldPosition(bone->parentName);
        } else {
            // Root bone - start position is slightly below end position
            visualBone.startPos = visualBone.endPos - glm::vec3(0.0f, 0.1f, 0.0f);
        }
        
        // Color based on selection - red for bones, yellow for selected
        if (visualBone.selected) {
            visualBone.color = glm::vec3(1.0f, 1.0f, 0.0f); // Yellow when selected
        } else {
            visualBone.color = glm::vec3(1.0f, 0.0f, 0.0f); // Red for bones
        }
        
        m_visualBones.push_back(visualBone);
    }
}

void VisualSkeletonEditor::PaintWeightsAtPosition(const glm::vec3& worldPos) {
    if (m_activePaintBoneID < 0) {
        return;
    }
    
    std::vector<size_t> vertexIndices;
    GetVerticesInRadius(worldPos, m_paintRadius, vertexIndices);
    
    for (size_t vertexIndex : vertexIndices) {
        glm::vec3 vertexPos = glm::vec3(0.0f);
        if (vertexIndex < m_meshTriangles.size() * 3) {
            size_t triIdx = vertexIndex / 3;
            size_t vIdx = vertexIndex % 3;
            if (triIdx < m_meshTriangles.size()) {
                const Triangle& tri = m_meshTriangles[triIdx];
                if (vIdx == 0) vertexPos = tri.v0;
                else if (vIdx == 1) vertexPos = tri.v1;
                else vertexPos = tri.v2;
            }
        }
        
        float distance = glm::length(vertexPos - worldPos);
        if (distance <= m_paintRadius) {
            float weight = m_paintIntensity * (1.0f - distance / m_paintRadius);
            weight = std::max(0.0f, std::min(1.0f, weight));
            m_weights.SetWeight(vertexIndex, m_activePaintBoneID, weight);
        }
    }
}

void VisualSkeletonEditor::GetVerticesInRadius(const glm::vec3& center, float radius, std::vector<size_t>& vertexIndices) {
    vertexIndices.clear();
    if (!m_meshDataLoaded) {
        return;
    }
    
    float radiusSq = radius * radius;
    for (size_t i = 0; i < m_meshTriangles.size(); i++) {
        const Triangle& tri = m_meshTriangles[i];
        if (glm::length2(tri.v0 - center) <= radiusSq) {
            vertexIndices.push_back(i * 3 + 0);
        }
        if (glm::length2(tri.v1 - center) <= radiusSq) {
            vertexIndices.push_back(i * 3 + 1);
        }
        if (glm::length2(tri.v2 - center) <= radiusSq) {
            vertexIndices.push_back(i * 3 + 2);
        }
    }
}

void VisualSkeletonEditor::RenderUI() {
    RenderNextBoneIndicator();
    RenderRightPanel();
    RenderResetDialog();
}

void VisualSkeletonEditor::InitializeBoneOrder() {
    m_expectedBoneOrder = {
        "Hips (Pelvis - Root)", 
        "Spine (Lower Back)", 
        "Spine1 (Mid Back)", 
        "Spine2 (Upper Back/Chest)", 
        "Neck (Base of Neck)", 
        "Head (Center of Head)", 
        "HeadTop_End (Top of Head)",
        "LeftShoulder (Left Shoulder Joint)", 
        "LeftArm (Left Upper Arm)", 
        "LeftForeArm (Left Elbow to Wrist)", 
        "LeftHand (Left Wrist)",
        "LeftHandThumb1 (Left Thumb Base)", 
        "LeftHandThumb2 (Left Thumb Middle)", 
        "LeftHandThumb3 (Left Thumb Tip Joint)", 
        "LeftHandThumb4 (Left Thumb Tip)",
        "LeftHandIndex1 (Left Index Base)", 
        "LeftHandIndex2 (Left Index Middle)", 
        "LeftHandIndex3 (Left Index Tip Joint)", 
        "LeftHandIndex4 (Left Index Tip)",
        "LeftHandMiddle1 (Left Middle Base)", 
        "LeftHandMiddle2 (Left Middle Middle)", 
        "LeftHandMiddle3 (Left Middle Tip Joint)", 
        "LeftHandMiddle4 (Left Middle Tip)",
        "LeftHandRing1 (Left Ring Base)", 
        "LeftHandRing2 (Left Ring Middle)", 
        "LeftHandRing3 (Left Ring Tip Joint)", 
        "LeftHandRing4 (Left Ring Tip)",
        "LeftHandPinky1 (Left Pinky Base)", 
        "LeftHandPinky2 (Left Pinky Middle)", 
        "LeftHandPinky3 (Left Pinky Tip Joint)", 
        "LeftHandPinky4 (Left Pinky Tip)",
        "RightShoulder (Right Shoulder Joint)", 
        "RightArm (Right Upper Arm)", 
        "RightForeArm (Right Elbow to Wrist)", 
        "RightHand (Right Wrist)",
        "RightHandThumb1 (Right Thumb Base)", 
        "RightHandThumb2 (Right Thumb Middle)", 
        "RightHandThumb3 (Right Thumb Tip Joint)", 
        "RightHandThumb4 (Right Thumb Tip)",
        "RightHandIndex1 (Right Index Base)", 
        "RightHandIndex2 (Right Index Middle)", 
        "RightHandIndex3 (Right Index Tip Joint)", 
        "RightHandIndex4 (Right Index Tip)",
        "RightHandMiddle1 (Right Middle Base)", 
        "RightHandMiddle2 (Right Middle Middle)", 
        "RightHandMiddle3 (Right Middle Tip Joint)", 
        "RightHandMiddle4 (Right Middle Tip)",
        "RightHandRing1 (Right Ring Base)", 
        "RightHandRing2 (Right Ring Middle)", 
        "RightHandRing3 (Right Ring Tip Joint)", 
        "RightHandRing4 (Right Ring Tip)",
        "RightHandPinky1 (Right Pinky Base)", 
        "RightHandPinky2 (Right Pinky Middle)", 
        "RightHandPinky3 (Right Pinky Tip Joint)", 
        "RightHandPinky4 (Right Pinky Tip)",
        "LeftUpLeg (Left Hip to Knee)", 
        "LeftLeg (Left Knee to Ankle)", 
        "LeftFoot (Left Ankle)", 
        "LeftToeBase (Left Toe Base)", 
        "LeftToe_End (Left Toe Tip)",
        "RightUpLeg (Right Hip to Knee)", 
        "RightLeg (Right Knee to Ankle)", 
        "RightFoot (Right Ankle)", 
        "RightToeBase (Right Toe Base)", 
        "RightToe_End (Right Toe Tip)"
    };
    m_bonesPlaced.clear();
    m_bonesPlaced.resize(m_expectedBoneOrder.size(), false);
    UpdateNextBoneIndex();
}

void VisualSkeletonEditor::UpdateNextBoneIndex() {
    m_nextBoneIndex = 0;
    for (size_t i = 0; i < m_bonesPlaced.size(); i++) {
        if (!m_bonesPlaced[i]) {
            m_nextBoneIndex = static_cast<int>(i);
            return;
        }
    }
    m_nextBoneIndex = static_cast<int>(m_bonesPlaced.size());
}

std::string VisualSkeletonEditor::GetNextBoneName() const {
    if (m_nextBoneIndex < static_cast<int>(m_expectedBoneOrder.size())) {
        return m_expectedBoneOrder[m_nextBoneIndex];
    }
    return "";
}

void VisualSkeletonEditor::UpdateNextBoneIndexFromLoadedSkeleton() {
    // Get all bone names from the loaded skeleton
    std::vector<std::string> loadedBoneNames = m_skeletonEditor.GetSkeleton()->GetAllBoneNames();
    std::set<std::string> loadedBoneSet(loadedBoneNames.begin(), loadedBoneNames.end());
    
    // Mark bones as placed based on loaded skeleton
    int highestPlacedIndex = -1;
    for (int i = 0; i < static_cast<int>(m_expectedBoneOrder.size()); i++) {
        std::string displayName = m_expectedBoneOrder[i];
        std::string baseName = ExtractBaseBoneName(displayName);
        
        if (loadedBoneSet.find(baseName) != loadedBoneSet.end()) {
            m_bonesPlaced[i] = true;
            highestPlacedIndex = i;
        } else {
            m_bonesPlaced[i] = false;
        }
    }
    
    // Update parent bone name to the last placed bone (if any)
    if (highestPlacedIndex >= 0 && highestPlacedIndex < static_cast<int>(m_expectedBoneOrder.size())) {
        std::string lastBoneDisplayName = m_expectedBoneOrder[highestPlacedIndex];
        m_parentBoneName = ExtractBaseBoneName(lastBoneDisplayName);
    } else {
        m_parentBoneName = "";
    }
    
    // Update next bone index (will find first unplaced bone)
    UpdateNextBoneIndex();
    
    LOG_INFO_FMT("VisualEditor", "Updated next bone index to %d (based on %zu loaded bones)", 
                 m_nextBoneIndex, loadedBoneNames.size());
}

std::string VisualSkeletonEditor::ExtractBaseBoneName(const std::string& displayName) const {
    // Extract base name from display name like "Spine1 (Mid Back)" -> "Spine1"
    size_t parenPos = displayName.find('(');
    if (parenPos != std::string::npos) {
        // Remove trailing space before parenthesis
        std::string base = displayName.substr(0, parenPos);
        // Trim trailing whitespace
        while (!base.empty() && base.back() == ' ') {
            base.pop_back();
        }
        return base;
    }
    return displayName; // No description, return as-is
}

void VisualSkeletonEditor::RenderBoneNumber(const glm::vec3& position, int number) {
    if (!m_boneShader || !m_buffersInitialized) return;
    
    float size = 0.08f + (number % 10) * 0.01f;
    glm::vec3 offset = glm::vec3(0.0f, size * 1.5f, 0.0f);
    glm::vec3 numberPos = position + offset;
    
    m_boneShader->UseShader();
    GLuint shaderID = m_boneShader->GetShaderID();
    if (shaderID == 0) return;
    
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint colorLoc = glGetUniformLocation(shaderID, "color");
    
    if (projLoc == -1 || viewLoc == -1 || modelLoc == -1 || colorLoc == -1) return;
    
    glm::mat4 model = glm::translate(glm::mat4(1.0f), numberPos);
    model = glm::scale(model, glm::vec3(size));
    
    float hue = (number * 0.1f);
    glm::vec3 color = glm::vec3(
        0.5f + 0.5f * sinf(hue),
        0.5f + 0.5f * sinf(hue + 2.094f),
        0.5f + 0.5f * sinf(hue + 4.189f)
    );
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projection));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_view));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(colorLoc, 1, glm::value_ptr(color));
    
    float halfSize = 0.5f;
    std::vector<float> vertices = {
        -halfSize, -halfSize, 0.0f,
        halfSize, -halfSize, 0.0f,
        halfSize, halfSize, 0.0f,
        -halfSize, halfSize, 0.0f
    };
    
    glBindVertexArray(m_boneVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_boneVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
}

void VisualSkeletonEditor::RenderBonePlacementSphere(const glm::vec3& position) {
    if (!m_boneShader || !m_buffersInitialized) return;
    
    // Project position to check if behind camera
    glm::vec4 clipPos = m_projection * m_view * glm::vec4(position, 1.0f);
    if (clipPos.w <= 0.0f) return; // Behind camera
    
    // Calculate sphere radius using helper function
    float radius = CalculateSphereRadius(position);
    
    m_boneShader->UseShader();
    GLuint shaderID = m_boneShader->GetShaderID();
    if (shaderID == 0) return;
    
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint colorLoc = glGetUniformLocation(shaderID, "color");
    
    if (projLoc == -1 || viewLoc == -1 || modelLoc == -1 || colorLoc == -1) return;
    
    // Create a simple sphere using icosahedron approximation (12 vertices, 20 faces)
    // For a simple sphere, we'll use a low-poly sphere with 8 segments
    const int segments = 8;
    std::vector<float> vertices;
    
    // Generate sphere vertices
    for (int i = 0; i <= segments; i++) {
        float theta = static_cast<float>(i) * 3.14159f / static_cast<float>(segments); // 0 to PI
        float sinTheta = sinf(theta);
        float cosTheta = cosf(theta);
        
        for (int j = 0; j <= segments; j++) {
            float phi = static_cast<float>(j) * 2.0f * 3.14159f / static_cast<float>(segments); // 0 to 2*PI
            float sinPhi = sinf(phi);
            float cosPhi = cosf(phi);
            
            // Sphere point
            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;
            
            // Scale by radius and translate to position
            vertices.push_back(position.x + x * radius);
            vertices.push_back(position.y + y * radius);
            vertices.push_back(position.z + z * radius);
        }
    }
    
    // Generate indices for sphere triangles
    std::vector<unsigned int> indices;
    for (int i = 0; i < segments; i++) {
        for (int j = 0; j < segments; j++) {
            int first = i * (segments + 1) + j;
            int second = first + segments + 1;
            
            // First triangle
            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);
            
            // Second triangle
            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }
    
    glm::mat4 model = glm::mat4(1.0f);
    glm::vec3 color = glm::vec3(0.0f, 1.0f, 0.0f); // Green
    
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projection));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_view));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv(colorLoc, 1, glm::value_ptr(color));
    
    glBindVertexArray(m_boneVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_boneVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_DYNAMIC_DRAW);
    
    // Draw sphere using triangles
    // Note: We're using GL_LINES for wireframe or we need an index buffer for filled
    // For simplicity, let's use GL_TRIANGLES with the vertices
    // Actually, we need to use the indices, but since we don't have an EBO set up,
    // let's use a simpler approach - render as triangle strips
    
    // Simple approach: render as points connected, or use a simpler sphere
    // Let's use a simpler icosahedron-based sphere
    // Actually, for now let's just render multiple quads in different orientations to approximate a sphere
    // Or better: render a proper sphere using triangle strips
    
    // For immediate rendering without index buffer, let's create triangle vertices directly
    std::vector<float> triangleVertices;
    for (size_t i = 0; i < indices.size(); i += 3) {
        int idx0 = indices[i] * 3;
        int idx1 = indices[i + 1] * 3;
        int idx2 = indices[i + 2] * 3;
        
        triangleVertices.push_back(vertices[idx0]);
        triangleVertices.push_back(vertices[idx0 + 1]);
        triangleVertices.push_back(vertices[idx0 + 2]);
        triangleVertices.push_back(vertices[idx1]);
        triangleVertices.push_back(vertices[idx1 + 1]);
        triangleVertices.push_back(vertices[idx1 + 2]);
        triangleVertices.push_back(vertices[idx2]);
        triangleVertices.push_back(vertices[idx2 + 1]);
        triangleVertices.push_back(vertices[idx2 + 2]);
    }
    
    glBufferData(GL_ARRAY_BUFFER, triangleVertices.size() * sizeof(float), triangleVertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(triangleVertices.size() / 3));
    glBindVertexArray(0);
}

void VisualSkeletonEditor::RenderRightPanel() {
    if (!m_nuklearInitialized || !m_nkContext || !m_window) return;
    
    // Get current framebuffer size directly from GLFW to ensure accuracy
    GLFWwindow* glfwWindow = glfwGetCurrentContext();
    int width, height;
    if (glfwWindow) {
        glfwGetFramebufferSize(glfwWindow, &width, &height);
    } else {
        // Fallback to window class if GLFW context not available
        width = m_window->getBufferWidth();
        height = m_window->getBufferHeight();
    }
    
    // Right panel: fixed width, full height (starting below next bone indicator)
    float panelWidth = 320.0f;
    float panelX = static_cast<float>(width) - panelWidth;
    float panelY = 30.0f; // Start below the next bone indicator
    float panelHeight = static_cast<float>(height) - panelY;
    
    if (nk_begin(m_nkContext, "Controls", nk_rect(panelX, panelY, panelWidth, panelHeight),
        NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE | NK_WINDOW_SCALABLE)) {
        
        // Stack all controls vertically in the right panel
        nk_layout_row_dynamic(m_nkContext, 30, 1);
        
        // Mesh toggle button
        if (nk_button_label(m_nkContext, m_meshVisible ? "Mesh: ON (Ctrl+M)" : "Mesh: OFF (Ctrl+M)")) {
            m_meshVisible = !m_meshVisible;
            LOG_INFO_FMT("VisualEditor", "Mesh visibility: %s", m_meshVisible ? "ON" : "OFF");
        }
        
        // Contours toggle button
        if (nk_button_label(m_nkContext, m_contoursVisible ? "Contours: ON (Ctrl+V)" : "Contours: OFF (Ctrl+V)")) {
            m_contoursVisible = !m_contoursVisible;
            LOG_INFO_FMT("VisualEditor", "Contours visibility: %s", m_contoursVisible ? "ON" : "OFF");
        }
        
        // Auto-Save toggle button
        if (nk_button_label(m_nkContext, m_autoSave ? "Auto-Save: ON" : "Auto-Save: OFF")) {
            m_autoSave = !m_autoSave;
            LOG_INFO_FMT("VisualEditor", "Auto-save: %s", m_autoSave ? "ON" : "OFF");
        }
        
        // Save Skeleton button
        if (nk_button_label(m_nkContext, "Save Skeleton (Ctrl+S)")) {
            if (!m_skeletonFilePath.empty()) {
                if (SaveRig(m_skeletonFilePath)) {
                    LOG_INFO_FMT("VisualEditor", "Manually saved skeleton to: %s", m_skeletonFilePath.c_str());
                } else {
                    LOG_ERROR_FMT("VisualEditor", "Failed to save skeleton to: %s", m_skeletonFilePath.c_str());
                }
            } else if (!m_meshFilePath.empty()) {
                // Generate skeleton path from mesh path
                std::string savePath = m_meshFilePath;
                size_t lastDot = savePath.find_last_of('.');
                if (lastDot != std::string::npos) {
                    savePath = savePath.substr(0, lastDot) + ".skel";
                } else {
                    savePath += ".skel";
                }
                if (SaveRig(savePath)) {
                    m_skeletonFilePath = savePath;
                    LOG_INFO_FMT("VisualEditor", "Manually saved skeleton to: %s", savePath.c_str());
                } else {
                    LOG_ERROR_FMT("VisualEditor", "Failed to save skeleton to: %s", savePath.c_str());
                }
            } else {
                LOG_WARNING("VisualEditor", "No skeleton file path available for saving");
            }
        }
        
        // Reset All button
        if (nk_button_label(m_nkContext, "Reset All")) {
            m_showResetDialog = true;
        }
        
        // Connections toggle button
        if (nk_button_label(m_nkContext, m_showBoneConnections ? "Connections: ON" : "Connections: OFF")) {
            m_showBoneConnections = !m_showBoneConnections;
            LOG_INFO_FMT("VisualEditor", "Bone connections: %s", m_showBoneConnections ? "ON" : "OFF");
        }
        
        // Weight Generation Section
        nk_layout_row_dynamic(m_nkContext, 15, 1);
        nk_spacing(m_nkContext, 1);
        
        nk_layout_row_dynamic(m_nkContext, 25, 1);
        nk_label(m_nkContext, "Weight Generation:", NK_TEXT_LEFT);
        
        // Check if all bones are placed
        bool allBonesPlaced = (m_nextBoneIndex >= static_cast<int>(m_expectedBoneOrder.size()));
        int bonesPlacedCount = 0;
        int totalBonesCount = static_cast<int>(m_expectedBoneOrder.size());
        for (size_t i = 0; i < m_bonesPlaced.size(); i++) {
            if (m_bonesPlaced[i]) bonesPlacedCount++;
        }
        
        nk_layout_row_dynamic(m_nkContext, 20, 1);
        char boneStatusText[128];
        snprintf(boneStatusText, sizeof(boneStatusText), "Bones placed: %d / %d", bonesPlacedCount, totalBonesCount);
        nk_label(m_nkContext, boneStatusText, NK_TEXT_LEFT);
        
        nk_layout_row_dynamic(m_nkContext, 30, 1);
        if (allBonesPlaced && m_meshDataLoaded) {
            if (nk_button_label(m_nkContext, "Generate Weights from Mesh")) {
                GenerateWeightsFromMesh();
            }
        } else {
            // Disabled button - just show label instead of button
            nk_label_colored(m_nkContext, "Generate Weights from Mesh (Disabled)", NK_TEXT_LEFT, nk_rgb(150, 150, 150));
            
            nk_layout_row_dynamic(m_nkContext, 20, 1);
            if (!allBonesPlaced) {
                nk_label_colored(m_nkContext, "⚠ All bones must be placed first", NK_TEXT_LEFT, nk_rgb(255, 200, 0));
            } else if (!m_meshDataLoaded) {
                nk_label_colored(m_nkContext, "⚠ Mesh must be loaded first", NK_TEXT_LEFT, nk_rgb(255, 200, 0));
            }
        }
        
        // Spacing before sliders
        nk_layout_row_dynamic(m_nkContext, 15, 1);
        nk_spacing(m_nkContext, 1);
        
        // Bone marker size slider
        nk_layout_row_dynamic(m_nkContext, 30, 1);
        nk_label(m_nkContext, "Marker Size:", NK_TEXT_LEFT);
        nk_layout_row_dynamic(m_nkContext, 25, 1);
        nk_slider_float(m_nkContext, 2.0f, &m_boneMarkerSize, 20.0f, 1.0f);
        nk_layout_row_dynamic(m_nkContext, 20, 1);
        char sizeText[32];
        snprintf(sizeText, sizeof(sizeText), "%.0fpx", m_boneMarkerSize);
        nk_label(m_nkContext, sizeText, NK_TEXT_LEFT);
        
        // Contour density slider (only show when contours are visible)
        if (m_contoursVisible) {
            nk_layout_row_dynamic(m_nkContext, 15, 1);
            nk_spacing(m_nkContext, 1);
            nk_layout_row_dynamic(m_nkContext, 30, 1);
            nk_label(m_nkContext, "Contour Density:", NK_TEXT_LEFT);
            nk_layout_row_dynamic(m_nkContext, 25, 1);
            nk_slider_float(m_nkContext, 0.05f, &m_contourInterval, 2.0f, 0.05f);
            nk_layout_row_dynamic(m_nkContext, 20, 1);
            char intervalText[32];
            snprintf(intervalText, sizeof(intervalText), "%.2f", m_contourInterval);
            const char* densityDesc = "";
            if (m_contourInterval < 0.5f) densityDesc = "(Very Dense)";
            else if (m_contourInterval < 1.0f) densityDesc = "(Dense)";
            else if (m_contourInterval < 1.5f) densityDesc = "(Medium)";
            else densityDesc = "(Sparse)";
            std::string densityText = std::string(intervalText) + " " + densityDesc;
            nk_label(m_nkContext, densityText.c_str(), NK_TEXT_LEFT);
        }
        
        // Animation controls
        if (m_animationsLoaded) {
            nk_layout_row_dynamic(m_nkContext, 15, 1);
            nk_spacing(m_nkContext, 1);
            
            nk_layout_row_dynamic(m_nkContext, 25, 1);
            nk_label(m_nkContext, "Animations:", NK_TEXT_LEFT);
            
            // Get list of loaded animations
            std::vector<std::string> animNames = m_animationPlayer.GetLoadedAnimationNames();
            std::string currentAnim = m_animationPlayer.GetCurrentAnimationName();
            
            // Animation selection dropdown
            if (!animNames.empty()) {
                nk_layout_row_dynamic(m_nkContext, 30, 1);
                if (nk_combo_begin_label(m_nkContext, currentAnim.empty() ? "Select Animation" : currentAnim.c_str(), 
                                        nk_vec2(nk_widget_width(m_nkContext), 200))) {
                    nk_layout_row_dynamic(m_nkContext, 25, 1);
                    for (const std::string& animName : animNames) {
                        if (nk_combo_item_label(m_nkContext, animName.c_str(), NK_TEXT_LEFT)) {
                            m_animationPlayer.Play(animName);
                        }
                    }
                    nk_combo_end(m_nkContext);
                }
                
                // Playback controls
                nk_layout_row_dynamic(m_nkContext, 30, 1);
                if (m_animationPaused) {
                    // When paused (manual time control), show Play button
                    if (nk_button_label(m_nkContext, "Play")) {
                        m_animationPaused = false;
                        m_animationPlayer.Resume();
                        if (!m_animationPlayer.IsPlaying()) {
                            // If not playing, start from current manual time
                            if (!currentAnim.empty()) {
                                m_animationPlayer.Play(currentAnim);
                                // Set time to manual time
                                // Note: AnimationPlayer doesn't support setting time directly,
                                // so we'll let it play from the start and it will catch up
                            }
                        }
                    }
                } else if (m_animationPlayer.IsPlaying()) {
                    if (m_animationPlayer.IsPaused()) {
                        if (nk_button_label(m_nkContext, "Resume")) {
                            m_animationPlayer.Resume();
                        }
                    } else {
                        if (nk_button_label(m_nkContext, "Pause")) {
                            m_animationPlayer.Pause();
                        }
                    }
                    if (nk_button_label(m_nkContext, "Stop")) {
                        m_animationPlayer.Stop();
                        m_animationPaused = false;
                        m_manualAnimationTime = 0.0f;
                    }
                } else {
                    if (nk_button_label(m_nkContext, "Play")) {
                        if (!currentAnim.empty()) {
                            m_animationPlayer.Play(currentAnim);
                            m_animationPaused = false;
                        } else if (!animNames.empty()) {
                            m_animationPlayer.Play(animNames[0]);
                            m_animationPaused = false;
                        }
                    }
                }
                
                // Animation time slider
                float duration = m_animationPlayer.GetAnimationDuration();
                if (duration > 0.0f) {
                    nk_layout_row_dynamic(m_nkContext, 20, 1);
                    nk_label(m_nkContext, "Time:", NK_TEXT_LEFT);
                    nk_layout_row_dynamic(m_nkContext, 25, 1);
                    
                    float currentTime = m_animationPaused ? m_manualAnimationTime : m_animationPlayer.GetCurrentTime();
                    float timeValue = currentTime;
                    bool sliderChanged = nk_slider_float(m_nkContext, 0.0f, &timeValue, duration, 0.01f);
                    if (sliderChanged) {
                        // User is dragging the slider - pause animation and set manual time
                        if (!m_animationPaused) {
                            m_animationPlayer.Pause();
                            m_animationPaused = true;
                        }
                        m_manualAnimationTime = timeValue;
                        // Apply animation at manual time
                        ApplyAnimationAtTime(timeValue);
                    }
                    
                    nk_layout_row_dynamic(m_nkContext, 20, 1);
                    char timeText[64];
                    snprintf(timeText, sizeof(timeText), "%.2f / %.2fs", timeValue, duration);
                    nk_label(m_nkContext, timeText, NK_TEXT_LEFT);
                }
                
                // Reset to bind pose button
                nk_layout_row_dynamic(m_nkContext, 30, 1);
                if (nk_button_label(m_nkContext, "Reset to Bind Pose")) {
                    ResetToBindPose();
                }
                
                // Animate mesh toggle
                nk_layout_row_dynamic(m_nkContext, 30, 1);
                if (nk_button_label(m_nkContext, m_animateMesh ? "Mesh Animation: ON" : "Mesh Animation: OFF")) {
                    m_animateMesh = !m_animateMesh;
                    LOG_INFO_FMT("VisualEditor", "Mesh animation: %s", m_animateMesh ? "ON" : "OFF");
                    if (!m_animateMesh) {
                        // Reset mesh to original position when disabling
                        ResetMeshToOriginal();
                    }
                }
                
            }
        }
        
        // Spacing before bone list
        nk_layout_row_dynamic(m_nkContext, 15, 1);
        nk_spacing(m_nkContext, 1);
        
        // Bone list header
        nk_layout_row_dynamic(m_nkContext, 25, 1);
        nk_label(m_nkContext, "Bone Placement Status:", NK_TEXT_LEFT);
        
        // Get remaining space for scrollable list
        struct nk_rect bounds = nk_widget_bounds(m_nkContext);
        float remainingHeight = panelHeight - (bounds.y - panelY) - 30.0f; // Leave space at bottom
        
        if (remainingHeight > 100.0f) { // Only show if there's enough space
            // Use a fixed height row to reserve space for the scrollable list
            nk_layout_row_dynamic(m_nkContext, remainingHeight, 1);
            
            // Begin scrollable group - Nuklear will enable scrolling when content exceeds bounds
            // Use a unique name for the group
            char groupName[64];
            snprintf(groupName, sizeof(groupName), "BoneList_%p", (void*)this);
            
            if (nk_group_begin(m_nkContext, groupName, NK_WINDOW_BORDER)) {
                // Iterate through all expected bones
                for (int i = 0; i < static_cast<int>(m_expectedBoneOrder.size()); i++) {
                    // Use full bone description instead of just base name
                    std::string boneDescription = m_expectedBoneOrder[i];
                    std::string baseBoneName = ExtractBaseBoneName(boneDescription);
                    bool isPlaced = (i < static_cast<int>(m_bonesPlaced.size()) && m_bonesPlaced[i]);
                    bool isNext = (i == m_nextBoneIndex);
                    
                    // Create display text with number
                    std::string displayText = std::to_string(i + 1) + ". " + boneDescription;
                    if (isNext) {
                        displayText += " [NEXT]";
                    }
                    
                    // Set colors based on status
                    struct nk_color bulletColor;
                    struct nk_color textColor;
                    if (isPlaced) {
                        // Green bullet and text for placed bones
                        bulletColor = nk_rgb(0, 255, 0);
                        textColor = nk_rgb(0, 255, 0);
                    } else {
                        // Yellow bullet point and gray text for unplaced bones
                        bulletColor = nk_rgb(255, 255, 0);
                        textColor = nk_rgb(200, 200, 200);
                    }
                    
                    // Layout: bullet point + clickable text
                    nk_layout_row_begin(m_nkContext, NK_STATIC, 20, 2);
                    nk_layout_row_push(m_nkContext, 15); // Space for bullet
                    nk_label_colored(m_nkContext, "•", NK_TEXT_LEFT, bulletColor);
                    nk_layout_row_push(m_nkContext, panelWidth - 35); // Rest for text
                    
                    // Create a button style that looks like text (transparent background)
                    struct nk_style_button textButtonStyle = m_nkContext->style.button;
                    textButtonStyle.normal = nk_style_item_color(nk_rgba(0, 0, 0, 0)); // Transparent
                    textButtonStyle.hover = nk_style_item_color(nk_rgba(50, 50, 50, 100)); // Slight highlight on hover
                    textButtonStyle.active = nk_style_item_color(nk_rgba(100, 100, 100, 150)); // More visible on click
                    textButtonStyle.text_normal = textColor;
                    textButtonStyle.text_hover = textColor;
                    textButtonStyle.text_active = textColor;
                    
                    // Make the text clickable using a button styled to look like text
                    if (nk_button_label_styled(m_nkContext, &textButtonStyle, displayText.c_str())) {
                        if (isPlaced) {
                            // Bone is placed - remove it
                            LOG_INFO_FMT("VisualEditor", "Removing bone '%s' from list", baseBoneName.c_str());
                            RemoveBoneAndChildren(baseBoneName);
                            UpdateVisualBones();
                            UpdateNextBoneIndex();
                            
                            // Auto-save if enabled
                            if (m_autoSave) {
                                std::string savePath = m_skeletonFilePath.empty() ? "" : m_skeletonFilePath;
                                if (savePath.empty() && !m_meshFilePath.empty()) {
                                    savePath = m_meshFilePath;
                                    size_t lastDot = savePath.find_last_of('.');
                                    if (lastDot != std::string::npos) {
                                        savePath = savePath.substr(0, lastDot) + ".skel";
                                    } else {
                                        savePath += ".skel";
                                    }
                                }
                                if (!savePath.empty()) {
                                    SaveRig(savePath);
                                }
                            }
                        } else {
                            // Bone is not placed - check if it has a stored position from previous removal
                            auto posIt = m_removedBonePositions.find(baseBoneName);
                            if (posIt != m_removedBonePositions.end()) {
                                // Restore bone at its previous position
                                glm::vec3 previousPos = posIt->second;
                                LOG_INFO_FMT("VisualEditor", "Found stored position for '%s': (%.2f, %.2f, %.2f), restoring...", 
                                           baseBoneName.c_str(), previousPos.x, previousPos.y, previousPos.z);
                                
                                // Set as next bone first, then place it
                                m_nextBoneIndex = i;
                                
                                // Remove from stored positions BEFORE placing (PlaceBoneAtPosition will try to erase it too)
                                m_removedBonePositions.erase(posIt);
                                
                                // Place the bone at its previous position
                                PlaceBoneAtPosition(previousPos);
                            } else {
                                // No stored position - set it as the next bone to place
                                LOG_INFO_FMT("VisualEditor", "No stored position for '%s', setting as next bone to place", baseBoneName.c_str());
                                m_nextBoneIndex = i;
                            }
                        }
                    }
                    
                    nk_layout_row_end(m_nkContext);
                }
                
                nk_group_end(m_nkContext);
            }
        }
    }
    nk_end(m_nkContext);
}

void VisualSkeletonEditor::RenderResetDialog() {
    if (!m_nuklearInitialized || !m_nkContext || !m_window) return;
    
    if (!m_showResetDialog) return;
    
    // Get current framebuffer size directly from GLFW to ensure accuracy
    GLFWwindow* glfwWindow = glfwGetCurrentContext();
    int width, height;
    if (glfwWindow) {
        glfwGetFramebufferSize(glfwWindow, &width, &height);
    } else {
        // Fallback to window class if GLFW context not available
        width = m_window->getBufferWidth();
        height = m_window->getBufferHeight();
    }
    
    // Center the dialog - compact size with just text and buttons
    float dialogWidth = 480.0f;
    float dialogHeight = 250.0f; // Reduced height since we removed spacing
    float x = (static_cast<float>(width) - dialogWidth) * 0.5f;
    float y = (static_cast<float>(height) - dialogHeight) * 0.5f;
    
    if (nk_begin(m_nkContext, "Reset Confirmation", nk_rect(x, y, dialogWidth, dialogHeight),
        NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_MOVABLE | NK_WINDOW_CLOSABLE)) {
        
        // Main question
        nk_layout_row_dynamic(m_nkContext, 30, 1);
        nk_label(m_nkContext, "Are you sure you want to reset all bones?", NK_TEXT_CENTERED);
        
        // "This will:" label
        nk_layout_row_dynamic(m_nkContext, 30, 1);
        nk_label(m_nkContext, "This will:", NK_TEXT_CENTERED);
        
        // Bullet points
        nk_layout_row_dynamic(m_nkContext, 30, 1);
        nk_label(m_nkContext, "- Clear all placed bones", NK_TEXT_CENTERED);
        nk_layout_row_dynamic(m_nkContext, 30, 1);
        nk_label(m_nkContext, "- Delete the skeleton file", NK_TEXT_CENTERED);
        
        // Warning message
        nk_layout_row_dynamic(m_nkContext, 30, 1);
        nk_label(m_nkContext, "This action cannot be undone!", NK_TEXT_CENTERED);
        
        // Buttons
        nk_layout_row_dynamic(m_nkContext, 40, 2);
        if (nk_button_label(m_nkContext, "Cancel")) {
            m_showResetDialog = false;
        }
        if (nk_button_label(m_nkContext, "Reset")) {
            ResetAllBones();
            m_showResetDialog = false;
        }
    } else {
        // Window was closed (X button clicked)
        m_showResetDialog = false;
    }
    nk_end(m_nkContext);
}

void VisualSkeletonEditor::ResetAllBones() {
    LOG_INFO("VisualEditor", "Resetting all bones and clearing skeleton file");
    
    // Clear all bones from skeleton editor
    m_skeletonEditor.NewSkeleton();
    
    // Clear visual bones
    m_visualBones.clear();
    
    // Reset bone tracking - mark all bones as not placed
    m_bonesPlaced.clear();
    m_bonesPlaced.resize(m_expectedBoneOrder.size(), false);
    m_removedBonePositions.clear(); // Clear stored removed bone positions
    UpdateNextBoneIndex();
    m_selectedBoneID = -1;
    m_parentBoneName.clear();
    
    // Clear weights
    m_weights.Clear();
    
    // Delete the skeleton file if it exists
    if (!m_skeletonFilePath.empty()) {
        try {
            if (std::filesystem::exists(m_skeletonFilePath)) {
                std::filesystem::remove(m_skeletonFilePath);
                LOG_INFO_FMT("VisualEditor", "Deleted skeleton file: %s", m_skeletonFilePath.c_str());
            }
        } catch (const std::exception& e) {
            LOG_ERROR_FMT("VisualEditor", "Failed to delete skeleton file: %s", e.what());
        }
    }
    
    // Also try to delete backup skeleton file (if mesh file path exists)
    if (!m_meshFilePath.empty()) {
        try {
            std::string backupPath = m_meshFilePath;
            size_t lastDot = backupPath.find_last_of('.');
            if (lastDot != std::string::npos) {
                backupPath = backupPath.substr(0, lastDot) + ".skel";
            } else {
                backupPath += ".skel";
            }
            
            if (std::filesystem::exists(backupPath)) {
                std::filesystem::remove(backupPath);
                LOG_INFO_FMT("VisualEditor", "Deleted backup skeleton file: %s", backupPath.c_str());
            }
        } catch (const std::exception& e) {
            LOG_ERROR_FMT("VisualEditor", "Failed to delete backup skeleton file: %s", e.what());
        }
    }
    
    // Clear skeleton file path
    m_skeletonFilePath.clear();
    
    LOG_INFO("VisualEditor", "Reset complete - all bones cleared and skeleton file deleted");
}

void VisualSkeletonEditor::RenderNextBoneIndicator() {
    if (!m_nuklearInitialized || !m_nkContext || !m_window) {
        GLFWwindow* window = glfwGetCurrentContext();
        if (window) {
            std::string nextBone = GetNextBoneName();
            std::string message;
            int totalExpectedBones = static_cast<int>(m_expectedBoneOrder.size());
            int bonesPlaced = m_nextBoneIndex;
            int bonesRemaining = totalExpectedBones - bonesPlaced;
            
            if (!nextBone.empty()) {
                int boneNumber = m_nextBoneIndex + 1;
                message = "Next Bone: #" + std::to_string(boneNumber) + " of " + std::to_string(totalExpectedBones) + " - " + nextBone + " (" + std::to_string(bonesRemaining) + " remaining)";
            } else {
                int totalBones = static_cast<int>(m_skeletonEditor.GetSkeleton()->GetBoneCount());
                message = "All bones placed! Total: " + std::to_string(totalBones) + " of " + std::to_string(totalExpectedBones);
            }
            std::string title = "Visual Skeleton Editor - " + message + " | Mesh: " + (m_meshVisible ? "ON" : "OFF") + " (M) | Contours: " + (m_contoursVisible ? "ON" : "OFF") + " (V) | S to save | ESC to exit";
            glfwSetWindowTitle(window, title.c_str());
        }
        return;
    }
    
    // Get current framebuffer size directly from GLFW to ensure accuracy
    GLFWwindow* glfwWindow = glfwGetCurrentContext();
    int width;
    if (glfwWindow) {
        int height;
        glfwGetFramebufferSize(glfwWindow, &width, &height);
    } else {
        // Fallback to window class if GLFW context not available
        width = m_window->getBufferWidth();
    }
    
    std::string nextBone = GetNextBoneName();
    std::string message;
    
    int totalExpectedBones = static_cast<int>(m_expectedBoneOrder.size());
    int bonesPlaced = m_nextBoneIndex;
    int bonesRemaining = totalExpectedBones - bonesPlaced;
    
    if (!nextBone.empty()) {
        int boneNumber = m_nextBoneIndex + 1;
        message = "Next Bone: #" + std::to_string(boneNumber) + " of " + std::to_string(totalExpectedBones) + " - " + nextBone + " (" + std::to_string(bonesRemaining) + " remaining)";
    } else {
        int totalBones = static_cast<int>(m_skeletonEditor.GetSkeleton()->GetBoneCount());
        message = "All bones placed! Total: " + std::to_string(totalBones) + " of " + std::to_string(totalExpectedBones);
    }
    
    GLFWwindow* window = glfwGetCurrentContext();
    if (window) {
        std::string title = "Visual Skeleton Editor - " + message + " | Mesh: " + (m_meshVisible ? "ON" : "OFF") + " (M) | Contours: " + (m_contoursVisible ? "ON" : "OFF") + " (V) | S to save | ESC to exit";
        glfwSetWindowTitle(window, title.c_str());
    }
    
    // Position indicator at the top of the window (no menu bar anymore)
    // Make it span full width but leave space for right panel
    float panelWidth = 320.0f;
    float indicatorWidth = static_cast<float>(width) - panelWidth;
    if (nk_begin(m_nkContext, "NextBoneIndicator", nk_rect(0, 0, indicatorWidth, 30),
        NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
        nk_layout_row_dynamic(m_nkContext, 30, 1);
        nk_label_colored(m_nkContext, message.c_str(), NK_TEXT_LEFT, nk_rgb(255, 255, 0));
    }
    nk_end(m_nkContext);
}

void VisualSkeletonEditor::RenderContours() {
    if (!m_mesh || !m_boneShader || !m_buffersInitialized || !m_meshDataLoaded) return;
    if (m_meshTriangles.empty()) return;
    if (m_contourInterval <= 0.0f) return;
    
    m_boneShader->UseShader();
    GLuint shaderID = m_boneShader->GetShaderID();
    if (shaderID == 0) return;
    
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint colorLoc = glGetUniformLocation(shaderID, "color");
    
    if (projLoc == -1 || viewLoc == -1 || modelLoc == -1 || colorLoc == -1) return;
    
    glm::mat4 model = glm::mat4(1.0f);
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projection));
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_view));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    
    // Calculate mesh bounding box
    glm::vec3 minBounds = m_meshTriangles[0].v0;
    glm::vec3 maxBounds = m_meshTriangles[0].v0;
    for (const auto& tri : m_meshTriangles) {
        minBounds = glm::min(minBounds, glm::min(tri.v0, glm::min(tri.v1, tri.v2)));
        maxBounds = glm::max(maxBounds, glm::max(tri.v0, glm::max(tri.v1, tri.v2)));
    }
    
    // Function to find intersection between a triangle and a plane
    auto intersectTrianglePlane = [](const Triangle& tri, const glm::vec3& planeNormal, float planeD, std::vector<glm::vec3>& intersections) {
        // Plane equation: dot(normal, point) = planeD
        // For axis-aligned planes: normal is (1,0,0), (0,1,0), or (0,0,1), planeD is the coordinate value
        
        float d0 = glm::dot(planeNormal, tri.v0) - planeD;
        float d1 = glm::dot(planeNormal, tri.v1) - planeD;
        float d2 = glm::dot(planeNormal, tri.v2) - planeD;
        
        // Check if triangle intersects the plane
        bool s0 = d0 >= 0.0f;
        bool s1 = d1 >= 0.0f;
        bool s2 = d2 >= 0.0f;
        
        // All vertices on same side - no intersection
        if (s0 == s1 && s1 == s2) return;
        
        // Find intersection points
        if (s0 != s1) {
            float t = d0 / (d0 - d1);
            intersections.push_back(tri.v0 + t * (tri.v1 - tri.v0));
        }
        if (s1 != s2) {
            float t = d1 / (d1 - d2);
            intersections.push_back(tri.v1 + t * (tri.v2 - tri.v1));
        }
        if (s2 != s0) {
            float t = d2 / (d2 - d0);
            intersections.push_back(tri.v2 + t * (tri.v0 - tri.v2));
        }
    };
    
    // Separate contour segments by axis
    std::vector<glm::vec3> xAxisContourVertices; // Perpendicular to X-axis (slicing along X) - BLUE
    std::vector<glm::vec3> yAxisContourVertices; // Perpendicular to Y-axis (slicing along Y) - RED
    std::vector<glm::vec3> zAxisContourVertices; // Perpendicular to Z-axis (slicing along Z) - GREEN
    
    // Slices perpendicular to X-axis (planes with normal (1,0,0)) - BLUE
    for (float x = minBounds.x; x <= maxBounds.x; x += m_contourInterval) {
        glm::vec3 planeNormal(1.0f, 0.0f, 0.0f);
        std::vector<glm::vec3> slicePoints;
        
        for (const auto& tri : m_meshTriangles) {
            intersectTrianglePlane(tri, planeNormal, x, slicePoints);
        }
        
        for (size_t i = 0; i < slicePoints.size(); i += 2) {
            if (i + 1 < slicePoints.size()) {
                xAxisContourVertices.push_back(slicePoints[i]);
                xAxisContourVertices.push_back(slicePoints[i + 1]);
            }
        }
    }
    
    // Slices perpendicular to Y-axis (planes with normal (0,1,0)) - RED
    for (float y = minBounds.y; y <= maxBounds.y; y += m_contourInterval) {
        glm::vec3 planeNormal(0.0f, 1.0f, 0.0f);
        std::vector<glm::vec3> slicePoints;
        
        for (const auto& tri : m_meshTriangles) {
            intersectTrianglePlane(tri, planeNormal, y, slicePoints);
        }
        
        // Connect points in pairs (each triangle intersection gives 2 points)
        for (size_t i = 0; i < slicePoints.size(); i += 2) {
            if (i + 1 < slicePoints.size()) {
                yAxisContourVertices.push_back(slicePoints[i]);
                yAxisContourVertices.push_back(slicePoints[i + 1]);
            }
        }
    }
    
    // Slices perpendicular to Z-axis (planes with normal (0,0,1)) - GREEN
    for (float z = minBounds.z; z <= maxBounds.z; z += m_contourInterval) {
        glm::vec3 planeNormal(0.0f, 0.0f, 1.0f);
        std::vector<glm::vec3> slicePoints;
        
        for (const auto& tri : m_meshTriangles) {
            intersectTrianglePlane(tri, planeNormal, z, slicePoints);
        }
        
        for (size_t i = 0; i < slicePoints.size(); i += 2) {
            if (i + 1 < slicePoints.size()) {
                zAxisContourVertices.push_back(slicePoints[i]);
                zAxisContourVertices.push_back(slicePoints[i + 1]);
            }
        }
    }
    
    glEnable(GL_DEPTH_TEST);
    glLineWidth(1.0f);
    
    glBindVertexArray(m_boneVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_boneVBO);
    
    // Render X-axis contours (perpendicular to X) in BLUE
    if (!xAxisContourVertices.empty()) {
        std::vector<float> xAxisVertexData;
        xAxisVertexData.reserve(xAxisContourVertices.size() * 3);
        for (const auto& v : xAxisContourVertices) {
            xAxisVertexData.push_back(v.x);
            xAxisVertexData.push_back(v.y);
            xAxisVertexData.push_back(v.z);
        }
        
        glUniform3fv(colorLoc, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 1.0f))); // Blue for X-axis contours
        glBufferData(GL_ARRAY_BUFFER, xAxisVertexData.size() * sizeof(float), xAxisVertexData.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(xAxisContourVertices.size()));
    }
    
    // Render Y-axis contours (perpendicular to Y) in RED
    if (!yAxisContourVertices.empty()) {
        std::vector<float> yAxisVertexData;
        yAxisVertexData.reserve(yAxisContourVertices.size() * 3);
        for (const auto& v : yAxisContourVertices) {
            yAxisVertexData.push_back(v.x);
            yAxisVertexData.push_back(v.y);
            yAxisVertexData.push_back(v.z);
        }
        
        glUniform3fv(colorLoc, 1, glm::value_ptr(glm::vec3(1.0f, 0.0f, 0.0f))); // Red for Y-axis contours
        glBufferData(GL_ARRAY_BUFFER, yAxisVertexData.size() * sizeof(float), yAxisVertexData.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(yAxisContourVertices.size()));
    }
    
    // Render Z-axis contours (perpendicular to Z) in GREEN
    if (!zAxisContourVertices.empty()) {
        std::vector<float> zAxisVertexData;
        zAxisVertexData.reserve(zAxisContourVertices.size() * 3);
        for (const auto& v : zAxisContourVertices) {
            zAxisVertexData.push_back(v.x);
            zAxisVertexData.push_back(v.y);
            zAxisVertexData.push_back(v.z);
        }
        
        glUniform3fv(colorLoc, 1, glm::value_ptr(glm::vec3(0.0f, 1.0f, 0.0f))); // Green for Z-axis contours
        glBufferData(GL_ARRAY_BUFFER, zAxisVertexData.size() * sizeof(float), zAxisVertexData.data(), GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(zAxisContourVertices.size()));
    }
    
    glBindVertexArray(0);
}


void VisualSkeletonEditor::LoadMeshDataForRaycasting(const std::string& filePath) {
    m_meshTriangles.clear();
    
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        filePath,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices
    );
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_ERROR_FMT("VisualEditor", "Failed to load mesh for raycasting: %s", importer.GetErrorString());
        return;
    }
    
    // Recursively process all nodes
    std::function<void(aiNode*)> processNode = [&](aiNode* node) {
        // Process all meshes in this node
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            
            // Extract triangles from this mesh
            for (unsigned int j = 0; j < mesh->mNumFaces; j++) {
                aiFace face = mesh->mFaces[j];
                if (face.mNumIndices == 3) {
                    // Get triangle vertices
                    glm::vec3 v0(
                        mesh->mVertices[face.mIndices[0]].x,
                        mesh->mVertices[face.mIndices[0]].y,
                        mesh->mVertices[face.mIndices[0]].z
                    );
                    glm::vec3 v1(
                        mesh->mVertices[face.mIndices[1]].x,
                        mesh->mVertices[face.mIndices[1]].y,
                        mesh->mVertices[face.mIndices[1]].z
                    );
                    glm::vec3 v2(
                        mesh->mVertices[face.mIndices[2]].x,
                        mesh->mVertices[face.mIndices[2]].y,
                        mesh->mVertices[face.mIndices[2]].z
                    );
                    
                    // Calculate normal
                    glm::vec3 edge1 = v1 - v0;
                    glm::vec3 edge2 = v2 - v0;
                    glm::vec3 normal = glm::normalize(glm::cross(edge1, edge2));
                    
                    Triangle tri;
                    tri.v0 = v0;
                    tri.v1 = v1;
                    tri.v2 = v2;
                    tri.normal = normal;
                    
                    m_meshTriangles.push_back(tri);
                }
            }
        }
        
        // Process child nodes
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i]);
        }
    };
    
    processNode(scene->mRootNode);
    LOG_INFO_FMT("VisualEditor", "Extracted %zu triangles for raycasting", m_meshTriangles.size());
}

bool VisualSkeletonEditor::RayTriangleIntersect(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                                                  const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                                                  float& t, glm::vec3& hitPoint) {
    // Möller-Trumbore intersection algorithm
    const float EPSILON = 0.0000001f;
    
    glm::vec3 edge1 = v1 - v0;
    glm::vec3 edge2 = v2 - v0;
    glm::vec3 h = glm::cross(rayDir, edge2);
    float a = glm::dot(edge1, h);
    
    if (a > -EPSILON && a < EPSILON) {
        return false; // Ray is parallel to triangle
    }
    
    float f = 1.0f / a;
    glm::vec3 s = rayOrigin - v0;
    float u = f * glm::dot(s, h);
    
    if (u < 0.0f || u > 1.0f) {
        return false;
    }
    
    glm::vec3 q = glm::cross(s, edge1);
    float v = f * glm::dot(rayDir, q);
    
    if (v < 0.0f || u + v > 1.0f) {
        return false;
    }
    
    // Compute t to find intersection point
    t = f * glm::dot(edge2, q);
    
    if (t > EPSILON) {
        hitPoint = rayOrigin + rayDir * t;
        return true;
    }
    
    return false; // Line intersection but not ray intersection
}

void VisualSkeletonEditor::CalculateBindPoses() {
    // Calculate inverse bind matrices for all bones
    const SkeletonDefinition* skeleton = m_skeletonEditor.GetSkeleton();
    auto boneNames = skeleton->GetAllBoneNames();
    
    for (const auto& boneName : boneNames) {
        const BoneDefinition* bone = skeleton->GetBone(boneName);
        if (!bone) continue;
        
        // Calculate global transform
        glm::mat4 globalTransform = glm::mat4(1.0f);
        
        // Traverse up hierarchy
        std::string currentBone = boneName;
        while (!currentBone.empty()) {
            const BoneDefinition* b = skeleton->GetBone(currentBone);
            if (!b) break;
            
            glm::mat4 local = b->GetLocalTransform();
            globalTransform = local * globalTransform;
            
            currentBone = b->parentName;
        }
        
        // Calculate inverse bind matrix
        BoneDefinition* mutableBone = const_cast<BoneDefinition*>(bone);
        mutableBone->CalculateOffsetMatrix(globalTransform);
    }
}

void VisualSkeletonEditor::CreateBoneVisualization() {
    UpdateVisualBones();
}

bool VisualSkeletonEditor::SaveRig(const std::string& filePath) {
    LOG_INFO_FMT("VisualEditor", "Saving rig to: %s", filePath.c_str());
    
    CalculateBindPoses();
    
    LOG_INFO_FMT("VisualEditor", "Saving skeleton to: %s", filePath.c_str());
    if (!m_skeletonEditor.SaveSkeleton(filePath)) {
        LOG_ERROR("VisualEditor", "Failed to save skeleton");
        return false;
    }
    LOG_INFO("VisualEditor", "Skeleton saved successfully");
    
    std::string weightsPath = filePath;
    size_t lastDot = weightsPath.find_last_of('.');
    size_t lastSlash = weightsPath.find_last_of("/\\");
    
    if (lastDot != std::string::npos && (lastSlash == std::string::npos || lastDot > lastSlash)) {
        // Has extension, replace it
        weightsPath = weightsPath.substr(0, lastDot) + ".weights";
    } else {
        // No extension, append .weights
        weightsPath += ".weights";
    }
    
    LOG_INFO_FMT("VisualEditor", "Saving weights to: %s", weightsPath.c_str());
    std::ofstream file(weightsPath);
    if (!file.is_open()) {
        LOG_WARNING("VisualEditor", "Failed to open weights file for writing (weights not saved)");
        // Don't fail the entire save if weights can't be saved
    } else {
        file << "# Vertex Weights\n";
        file << "# Format: vertexIndex|boneID1:weight1,boneID2:weight2,...\n\n";
        
        // Get skeleton to validate bone IDs before saving
        const SkeletonDefinition* skeleton = m_skeletonEditor.GetSkeleton();
        std::set<int> validBoneIDs;
        if (skeleton) {
            auto boneNames = skeleton->GetAllBoneNames();
            for (const std::string& boneName : boneNames) {
                const BoneDefinition* bone = skeleton->GetBone(boneName);
                if (bone) {
                    validBoneIDs.insert(bone->id);
                }
            }
        }
        
        size_t weightCount = 0;
        size_t invalidBoneIDCount = 0;
        for (size_t i = 0; i < m_weights.GetVertexCount(); i++) {
            auto weights = m_weights.GetVertexWeights(i);
            if (weights.empty()) continue;
            
            // Filter out invalid bone IDs before saving
            std::vector<VertexWeight> validWeights;
            float totalValidWeight = 0.0f;
            for (const auto& w : weights) {
                if (validBoneIDs.find(w.boneID) != validBoneIDs.end()) {
                    validWeights.push_back(w);
                    totalValidWeight += w.weight;
                } else {
                    invalidBoneIDCount++;
                }
            }
            
            // Skip vertices with no valid weights
            if (validWeights.empty()) continue;
            
            // Normalize weights if they don't sum to 1.0
            if (totalValidWeight > 0.0f && totalValidWeight < 0.99f) {
                float scale = 1.0f / totalValidWeight;
                for (auto& w : validWeights) {
                    w.weight *= scale;
                }
            }
            
            file << i << "|";
            for (size_t j = 0; j < validWeights.size(); j++) {
                if (j > 0) file << ",";
                file << validWeights[j].boneID << ":" << validWeights[j].weight;
            }
            file << "\n";
            weightCount++;
        }
        
        if (invalidBoneIDCount > 0) {
            LOG_WARNING_FMT("VisualEditor", "Filtered out %zu weight entries with invalid bone IDs when saving", invalidBoneIDCount);
        }
        
        file.close();
        LOG_INFO_FMT("VisualEditor", "Weights saved successfully (%zu vertices with weights)", weightCount);
    }
    
    LOG_INFO("VisualEditor", "Rig saved successfully");
    return true;
}

bool VisualSkeletonEditor::LoadRig(const std::string& filePath) {
    LOG_INFO_FMT("VisualEditor", "Loading rig from: %s", filePath.c_str());
    
    std::string absPath = filePath;
    if (!absPath.empty() && absPath.front() == '"' && absPath.back() == '"') {
        absPath = absPath.substr(1, absPath.length() - 2);
    }
    try {
        std::filesystem::path path(absPath);
        if (path.is_relative()) {
            path = std::filesystem::absolute(path);
        }
        path = path.lexically_normal();
        absPath = path.string();
    } catch (...) {
        absPath = filePath;
    }
    
    LOG_INFO_FMT("VisualEditor", "Loading skeleton from: %s", absPath.c_str());
    if (!m_skeletonEditor.LoadSkeleton(absPath)) {
        LOG_ERROR("VisualEditor", "Failed to load skeleton");
        return false;
    }
    
    // Store absolute path
    m_skeletonFilePath = absPath;
    LOG_INFO_FMT("VisualEditor", "Skeleton loaded successfully (%zu bones)", 
                 m_skeletonEditor.GetSkeleton()->GetBoneCount());
    
    // Store bind pose (original bone positions/rotations)
    StoreBindPose();
    
    // Note: SkeletonSerializer now handles world-to-local conversion automatically
    // based on the description field, so we don't need to do it here anymore
    
    // Update input field with full absolute path only if not currently being edited
    // Use safe string copy to ensure null termination
    size_t copyLen = std::min(m_skeletonFilePath.length(), sizeof(m_skeletonFileInput) - 1);
    memcpy(m_skeletonFileInput, m_skeletonFilePath.c_str(), copyLen);
    m_skeletonFileInput[copyLen] = '\0';
    
    // Update next bone index based on which bones have already been placed
    UpdateNextBoneIndexFromLoadedSkeleton();
    
    // Load weights - try multiple possible paths
    std::string weightsPath = filePath + ".weights";
    
    // Also try replacing .skel with .weights
    std::string altWeightsPath = absPath;
    size_t lastDot = altWeightsPath.find_last_of('.');
    if (lastDot != std::string::npos) {
        altWeightsPath = altWeightsPath.substr(0, lastDot) + ".weights";
    } else {
        altWeightsPath += ".weights";
    }
    
    // Try absolute path first, then relative
    std::ifstream file(altWeightsPath);
    if (!file.is_open()) {
        file.open(weightsPath);
    }
    if (!file.is_open()) {
        LOG_WARNING_FMT("VisualEditor", "Weights file not found (tried: %s and %s), continuing without weights", 
                       altWeightsPath.c_str(), weightsPath.c_str());
        return true;
    }
    
    LOG_INFO_FMT("VisualEditor", "Loading weights from: %s", altWeightsPath.c_str());
    
    m_weights.Clear();
    
    const SkeletonDefinition* skeleton = m_skeletonEditor.GetSkeleton();
    
    size_t weightCount = 0;
    size_t totalWeightEntries = 0;
    size_t mismatchedBoneIDs = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        size_t pipePos = line.find('|');
        if (pipePos == std::string::npos) continue;
        
        size_t vertexIndex = std::stoul(line.substr(0, pipePos));
        std::string weightsStr = line.substr(pipePos + 1);
        
        std::istringstream iss(weightsStr);
        std::string weightPair;
        int boneCount = 0;
        while (std::getline(iss, weightPair, ',')) {
            size_t colonPos = weightPair.find(':');
            if (colonPos == std::string::npos) continue;
            
            int boneID = std::stoi(weightPair.substr(0, colonPos));
            float weight = std::stof(weightPair.substr(colonPos + 1));
            
            if (skeleton) {
                const BoneDefinition* bone = skeleton->GetBone(boneID);
                if (!bone) {
                    mismatchedBoneIDs++;
                    static std::set<int> loggedMissingIDs;
                    if (loggedMissingIDs.find(boneID) == loggedMissingIDs.end() && loggedMissingIDs.size() < 10) {
                        LOG_WARNING_FMT("VisualEditor", "Bone ID %d in weights file not found in skeleton, skipping", boneID);
                        loggedMissingIDs.insert(boneID);
                    }
                    continue;
                }
            }
            
            m_weights.SetWeight(vertexIndex, boneID, weight);
            boneCount++;
            totalWeightEntries++;
        }
        if (boneCount > 0) {
            weightCount++;
        }
    }
    
    if (mismatchedBoneIDs > 0) {
        LOG_WARNING_FMT("VisualEditor", "Skipped %zu weight entries due to bone ID mismatches. Consider regenerating weights.", 
                       mismatchedBoneIDs);
    }
    
    file.close();
    LOG_INFO_FMT("VisualEditor", "Weights loaded successfully: %zu vertices with weights, %zu total weight entries", 
                 weightCount, totalWeightEntries);
    
    // If no weights were loaded and mesh is loaded, offer to generate them
    if (weightCount == 0 && m_meshDataLoaded && !m_originalVertexData.empty()) {
        LOG_INFO("VisualEditor", "No weights found. Use 'Generate Weights from Mesh' button to create them automatically.");
    }
    
    LOG_INFO("VisualEditor", "Rig loaded successfully");
    return true;
}

bool VisualSkeletonEditor::InitializeNuklear() {
    if (!m_window) {
        LOG_ERROR("VisualEditor", "Cannot initialize Nuklear: window is null");
        return false;
    }
    
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) {
        LOG_ERROR("VisualEditor", "Cannot initialize Nuklear: GLFW window is null");
        return false;
    }
    
    m_nuklear = new struct nk_glfw;
    memset(m_nuklear, 0, sizeof(struct nk_glfw));
    
    m_nkContext = nk_glfw3_init(m_nuklear, window, NK_GLFW3_DEFAULT);
    if (!m_nkContext) {
        LOG_ERROR("VisualEditor", "Failed to initialize Nuklear context");
        delete m_nuklear;
        m_nuklear = nullptr;
        return false;
    }
    
    struct nk_font_atlas* atlas;
    nk_glfw3_font_stash_begin(m_nuklear, &atlas);
    nk_glfw3_font_stash_end(m_nuklear);
    
    m_nuklearInitialized = true;
    LOG_INFO("VisualEditor", "Nuklear GUI initialized successfully");
    return true;
}

void VisualSkeletonEditor::ShutdownNuklear() {
    if (m_nuklearInitialized && m_nuklear) {
        nk_glfw3_shutdown(m_nuklear);
        delete m_nuklear;
        m_nuklear = nullptr;
        m_nkContext = nullptr;
        m_nuklearInitialized = false;
        LOG_INFO("VisualEditor", "Nuklear GUI shut down");
    }
}

void VisualSkeletonEditor::Cleanup() {
    ShutdownNuklear();
    
    if (m_buffersInitialized) {
        if (m_boneVAO != 0) {
            glDeleteVertexArrays(1, &m_boneVAO);
            m_boneVAO = 0;
        }
        if (m_boneVBO != 0) {
            glDeleteBuffers(1, &m_boneVBO);
            m_boneVBO = 0;
        }
        if (m_gridVAO != 0) {
            glDeleteVertexArrays(1, &m_gridVAO);
            m_gridVAO = 0;
        }
        if (m_gridVBO != 0) {
            glDeleteBuffers(1, &m_gridVBO);
            m_gridVBO = 0;
        }
        m_buffersInitialized = false;
    }
    
    if (m_mesh) {
        m_mesh->ClearModel();
        delete m_mesh;
        m_mesh = nullptr;
    }
    
    if (m_meshShader) {
        m_meshShader->ClearShader();
        delete m_meshShader;
        m_meshShader = nullptr;
    }
    
    if (m_boneShader) {
        m_boneShader->ClearShader();
        delete m_boneShader;
        m_boneShader = nullptr;
    }
    
    if (m_window) {
        delete m_window;
        m_window = nullptr;
    }
}

void VisualSkeletonEditor::LoadAnimationsFromDirectory(const std::string& directoryPath) {
    m_animationDirectory = directoryPath;
    
    if (!std::filesystem::exists(directoryPath)) {
        LOG_WARNING_FMT("VisualEditor", "Animation directory does not exist: %s", directoryPath.c_str());
        m_animationsLoaded = false;
        return;
    }
    
    LOG_INFO_FMT("VisualEditor", "Loading animations from directory (recursive): %s", directoryPath.c_str());
    
    int loadedCount = 0;
    int skippedCount = 0;
    
    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directoryPath)) {
            if (entry.is_regular_file()) {
                std::string filePath = entry.path().string();
                std::string extension = entry.path().extension().string();
                
                if (extension == ".fbx" || extension == ".FBX") {
                    std::filesystem::path relPath = std::filesystem::relative(entry.path(), directoryPath);
                    std::string animName = relPath.stem().string();
                    
                    if (relPath.has_parent_path() && !relPath.parent_path().empty()) {
                        std::string parentName = relPath.parent_path().string();
                        std::replace(parentName.begin(), parentName.end(), '\\', '_');
                        std::replace(parentName.begin(), parentName.end(), '/', '_');
                        animName = parentName + "_" + animName;
                    }
                    
                    LOG_INFO_FMT("VisualEditor", "Loading animation from: %s (name: %s)", filePath.c_str(), animName.c_str());
                    if (m_animationPlayer.LoadAnimation(filePath, animName)) {
                        loadedCount++;
                    } else {
                        skippedCount++;
                        LOG_WARNING_FMT("VisualEditor", "Failed to load animation: %s", filePath.c_str());
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        LOG_ERROR_FMT("VisualEditor", "Error traversing directory: %s", e.what());
        m_animationsLoaded = (loadedCount > 0);
        return;
    }
    
    m_animationsLoaded = (loadedCount > 0);
    LOG_INFO_FMT("VisualEditor", "Loaded %d animations from %s (skipped: %d)", loadedCount, directoryPath.c_str(), skippedCount);
}

bool VisualSkeletonEditor::LoadAnimationFile(const std::string& filePath, const std::string& animationName) {
    std::string animName = animationName;
    if (animName.empty()) {
        animName = std::filesystem::path(filePath).stem().string();
    }
    
    if (m_animationPlayer.LoadAnimation(filePath, animName)) {
        m_animationsLoaded = true;
        LOG_INFO_FMT("VisualEditor", "Loaded animation '%s' from: %s", animName.c_str(), filePath.c_str());
        return true;
    } else {
        LOG_WARNING_FMT("VisualEditor", "Failed to load animation from: %s", filePath.c_str());
        return false;
    }
}

void VisualSkeletonEditor::UpdateAnimation(float deltaTime) {
    m_animationPlayer.Update(deltaTime);
}

void VisualSkeletonEditor::ApplyAnimationToSkeleton() {
    if (!m_animationsLoaded) {
        return;
    }
    
    if (!m_animationPlayer.IsPlaying()) {
        return;
    }
    
    SkeletonDefinition* skeleton = const_cast<SkeletonDefinition*>(m_skeletonEditor.GetSkeleton());
    if (!skeleton) {
        return;
    }
    
    // Get all bone names from skeleton
    auto boneNames = skeleton->GetAllBoneNames();
    
    // Get current animation to check what bones it has
    const std::string& currentAnim = m_animationPlayer.GetCurrentAnimationName();
    if (currentAnim.empty()) {
        return;
    }
    
    // Helper function to normalize bone names for matching
    auto normalizeBoneName = [](const std::string& name) -> std::string {
        std::string normalized = name;
        // Convert to lowercase
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
        // Remove common prefixes/suffixes that FBX might add
        // Remove "mixamorig:" prefix (common in Mixamo exports)
        if (normalized.find("mixamorig:") == 0) {
            normalized = normalized.substr(10);
        }
        return normalized;
    };
    
    // Get animation bone names for matching
    std::vector<std::string> animBoneNames = m_animationPlayer.GetAnimationBoneNames(currentAnim);
    
    // Build a map of normalized animation bone names to actual names
    std::map<std::string, std::string> animBoneMap; // normalized -> actual
    for (const std::string& animBone : animBoneNames) {
        std::string normalized = normalizeBoneName(animBone);
        animBoneMap[normalized] = animBone;
    }
    
    // Log bone matching on first frame (for debugging)
    static bool loggedBoneMatching = false;
    if (!loggedBoneMatching) {
        LOG_INFO_FMT("VisualEditor", "Applying animation '%s' to skeleton with %zu bones", 
                     currentAnim.c_str(), boneNames.size());
        LOG_INFO_FMT("VisualEditor", "Animation has %zu bone channels", animBoneNames.size());
        loggedBoneMatching = true;
    }
    
    int matchedBones = 0;
    int totalBones = 0;
    
    // Apply animation transforms to each bone
    for (const std::string& boneName : boneNames) {
        totalBones++;
        glm::vec3 animPosition;
        glm::quat animRotation;
        glm::vec3 animScale;
        
        // Try exact match first
        bool found = m_animationPlayer.GetBoneTransform(boneName, animPosition, animRotation, animScale);
        std::string matchedAnimBone = boneName;
        
        // If exact match fails, try normalized matching
        if (!found) {
            std::string normalizedSkel = normalizeBoneName(boneName);
            auto it = animBoneMap.find(normalizedSkel);
            if (it != animBoneMap.end()) {
                // Found a match with normalized name
                matchedAnimBone = it->second;
                found = m_animationPlayer.GetBoneTransform(matchedAnimBone, animPosition, animRotation, animScale);
                if (found && !loggedBoneMatching) {
                    LOG_INFO_FMT("VisualEditor", "Matched skeleton bone '%s' to animation bone '%s' (normalized)", 
                                boneName.c_str(), matchedAnimBone.c_str());
                }
            }
        }
        
        // If still not found, try common variations
        if (!found) {
            std::vector<std::string> variations = {
                "mixamorig:" + boneName,
                boneName + "_bone",
                boneName + "bone",
            };
            for (const std::string& variant : variations) {
                if (m_animationPlayer.GetBoneTransform(variant, animPosition, animRotation, animScale)) {
                    found = true;
                    matchedAnimBone = variant;
                    if (!loggedBoneMatching) {
                        LOG_INFO_FMT("VisualEditor", "Matched skeleton bone '%s' to animation bone '%s' (variant)", 
                                    boneName.c_str(), variant.c_str());
                    }
                    break;
                }
            }
        }
        
        if (found) {
            matchedBones++;
            
            // Bone is animated - update its transform
            // Note: Animation transforms are typically in local space (relative to parent)
            // which matches how our skeleton stores bone transforms
            m_skeletonEditor.SetBonePosition(boneName, animPosition.x, animPosition.y, animPosition.z);
            
            // Use SkeletonDefinition directly for quaternion rotation
            skeleton->SetBoneRotation(boneName, animRotation);
            
            // Scale is typically (1,1,1) in most animations, but apply it if needed
            if (animScale != glm::vec3(1.0f, 1.0f, 1.0f)) {
                m_skeletonEditor.SetBoneScale(boneName, animScale.x, animScale.y, animScale.z);
            }
        }
    }
    
    // Log matching results once
    if (!loggedBoneMatching) {
        LOG_INFO_FMT("VisualEditor", "Matched %d/%d skeleton bones with animation", matchedBones, totalBones);
        if (matchedBones == 0) {
            LOG_WARNING("VisualEditor", "No bones matched! Bone names in animation may differ from skeleton.");
            LOG_WARNING("VisualEditor", "Check animation bone names vs skeleton bone names for mismatches.");
            if (animBoneNames.size() > 0 && animBoneNames.size() <= 10) {
                LOG_WARNING("VisualEditor", "Animation bone names:");
                for (const std::string& animBone : animBoneNames) {
                    LOG_WARNING_FMT("VisualEditor", "  - %s", animBone.c_str());
                }
            }
        }
    }
    
    // Update visual bones to reflect animated transforms
    UpdateVisualBones();
}

void VisualSkeletonEditor::ConvertWorldPositionsToLocal() {
    SkeletonDefinition* skeleton = const_cast<SkeletonDefinition*>(m_skeletonEditor.GetSkeleton());
    if (!skeleton) {
        return;
    }
    
    LOG_INFO("VisualEditor", "Converting world positions to local positions...");
    
    // Get all bones and process them in hierarchy order (parents before children)
    // First, collect root bones (no parent)
    std::vector<std::string> rootBones = skeleton->GetRootBones();
    std::map<std::string, glm::vec3> worldPositions; // Store original world positions
    
    // First pass: collect all world positions
    auto allBoneNames = skeleton->GetAllBoneNames();
    for (const std::string& boneName : allBoneNames) {
        const BoneDefinition* bone = skeleton->GetBone(boneName);
        if (bone) {
            worldPositions[boneName] = bone->position; // Store original world position
        }
    }
    
    // Second pass: convert to local positions
    // Process bones in hierarchy order (breadth-first)
    std::vector<std::string> toProcess = rootBones;
    std::set<std::string> processed;
    
    while (!toProcess.empty()) {
        std::string currentBoneName = toProcess.front();
        toProcess.erase(toProcess.begin());
        
        if (processed.find(currentBoneName) != processed.end()) {
            continue; // Already processed
        }
        
        BoneDefinition* bone = skeleton->GetBone(currentBoneName);
        if (!bone) {
            continue;
        }
        
        glm::vec3 worldPos = worldPositions[currentBoneName];
        glm::vec3 localPos;
        
        if (bone->parentName.empty()) {
            // Root bone: world position = local position
            localPos = worldPos;
        } else {
            // Child bone: local position = world position - parent's world position
            auto parentIt = worldPositions.find(bone->parentName);
            if (parentIt != worldPositions.end()) {
                localPos = worldPos - parentIt->second;
            } else {
                // Parent not found or not processed yet - keep world position for now
                LOG_WARNING_FMT("VisualEditor", "Parent '%s' not found for bone '%s', keeping world position", 
                               bone->parentName.c_str(), currentBoneName.c_str());
                localPos = worldPos;
            }
        }
        
        // Update bone's local position
        skeleton->SetBonePosition(currentBoneName, localPos);
        processed.insert(currentBoneName);
        
        // Add children to processing queue
        auto children = skeleton->GetBoneChildren(currentBoneName);
        for (const std::string& childName : children) {
            if (processed.find(childName) == processed.end()) {
                toProcess.push_back(childName);
            }
        }
    }
    
    LOG_INFO_FMT("VisualEditor", "Converted %zu bones from world to local positions", processed.size());
}

void VisualSkeletonEditor::StoreBindPose() {
    const SkeletonDefinition* skeleton = m_skeletonEditor.GetSkeleton();
    if (!skeleton) {
        return;
    }
    
    m_bindPosePositions.clear();
    m_bindPoseRotations.clear();
    
    auto boneNames = skeleton->GetAllBoneNames();
    for (const std::string& boneName : boneNames) {
        const BoneDefinition* bone = skeleton->GetBone(boneName);
        if (bone) {
            m_bindPosePositions[boneName] = bone->position;
            m_bindPoseRotations[boneName] = bone->rotation;
        }
    }
    
    LOG_INFO_FMT("VisualEditor", "Stored bind pose for %d bones", static_cast<int>(m_bindPosePositions.size()));
}

void VisualSkeletonEditor::ResetToBindPose() {
    SkeletonDefinition* skeleton = const_cast<SkeletonDefinition*>(m_skeletonEditor.GetSkeleton());
    if (!skeleton) {
        return;
    }
    
    // Stop animation
    m_animationPlayer.Stop();
    m_animationPaused = false;
    m_manualAnimationTime = 0.0f;
    
    // Restore bind pose positions and rotations
    for (const auto& pair : m_bindPosePositions) {
        const std::string& boneName = pair.first;
        const glm::vec3& bindPos = pair.second;
        
        m_skeletonEditor.SetBonePosition(boneName, bindPos.x, bindPos.y, bindPos.z);
        
        auto rotIt = m_bindPoseRotations.find(boneName);
        if (rotIt != m_bindPoseRotations.end()) {
            skeleton->SetBoneRotation(boneName, rotIt->second);
        }
    }
    
    // Reset mesh to original bind pose vertices
    ResetMeshToOriginal();
    
    UpdateVisualBones();
    LOG_INFO("VisualEditor", "Reset skeleton and mesh to bind pose");
}

void VisualSkeletonEditor::ApplyAnimationAtTime(float time) {
    if (!m_animationsLoaded || time < 0.0f) {
        return;
    }
    
    // Ensure we have an animation selected (use first if none selected)
    std::string currentAnim = m_animationPlayer.GetCurrentAnimationName();
    if (currentAnim.empty()) {
        auto animNames = m_animationPlayer.GetLoadedAnimationNames();
        if (animNames.empty()) {
            return;
        }
        currentAnim = animNames[0];
        // Don't actually play it, just use it for getting transforms
    }
    
    SkeletonDefinition* skeleton = const_cast<SkeletonDefinition*>(m_skeletonEditor.GetSkeleton());
    if (!skeleton) {
        return;
    }
    
    // Get animation bone names for matching
    std::vector<std::string> animBoneNames = m_animationPlayer.GetAnimationBoneNames(currentAnim);
    
    // Helper function to normalize bone names for matching
    auto normalizeBoneName = [](const std::string& name) -> std::string {
        std::string normalized = name;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), ::tolower);
        if (normalized.find("mixamorig:") == 0) {
            normalized = normalized.substr(10);
        }
        return normalized;
    };
    
    // Build a map of normalized animation bone names to actual names
    std::map<std::string, std::string> animBoneMap;
    for (const std::string& animBone : animBoneNames) {
        std::string normalized = normalizeBoneName(animBone);
        animBoneMap[normalized] = animBone;
    }
    
    // Get all bone names from skeleton
    auto boneNames = skeleton->GetAllBoneNames();
    
    // Apply animation transforms at the specified time
    for (const std::string& boneName : boneNames) {
        glm::vec3 animPosition;
        glm::quat animRotation;
        glm::vec3 animScale;
        
        // Try to get transform from animation at specified time
        bool found = m_animationPlayer.GetBoneTransformAtTime(boneName, time, animPosition, animRotation, animScale);
        std::string matchedAnimBone = boneName;
        
        // If exact match fails, try normalized matching
        if (!found) {
            std::string normalizedSkel = normalizeBoneName(boneName);
            auto it = animBoneMap.find(normalizedSkel);
            if (it != animBoneMap.end()) {
                matchedAnimBone = it->second;
                found = m_animationPlayer.GetBoneTransformAtTime(matchedAnimBone, time, animPosition, animRotation, animScale);
            }
        }
        
        // If still not found, try common variations
        if (!found) {
            std::vector<std::string> variations = {
                "mixamorig:" + boneName,
                boneName + "_bone",
                boneName + "bone",
            };
            for (const std::string& variant : variations) {
                if (m_animationPlayer.GetBoneTransformAtTime(variant, time, animPosition, animRotation, animScale)) {
                    found = true;
                    matchedAnimBone = variant;
                    break;
                }
            }
        }
        
        if (found) {
            m_skeletonEditor.SetBonePosition(boneName, animPosition.x, animPosition.y, animPosition.z);
            skeleton->SetBoneRotation(boneName, animRotation);
            if (animScale != glm::vec3(1.0f, 1.0f, 1.0f)) {
                m_skeletonEditor.SetBoneScale(boneName, animScale.x, animScale.y, animScale.z);
            }
        }
    }
    
    UpdateVisualBones();
}

void VisualSkeletonEditor::ApplyMeshAnimation() {
    UpdateMeshVertexBuffers();
}

void VisualSkeletonEditor::ResetMeshToOriginal() {
    if (!m_originalMeshTriangles.empty()) {
        m_meshTriangles = m_originalMeshTriangles;
    }
    if (!m_originalVertexData.empty()) {
        m_mesh->UpdateVertexPositions(m_originalVertexData, m_meshVertexCounts);
    }
    LOG_INFO("VisualEditor", "Reset mesh to original position");
}

void VisualSkeletonEditor::StoreOriginalVertexData(const std::string& filePath) {
    m_originalVertexData.clear();
    m_meshVertexCounts.clear();
    
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(
        filePath,
        aiProcess_Triangulate |
        aiProcess_FlipUVs |
        aiProcess_GenSmoothNormals |
        aiProcess_JoinIdenticalVertices
    );
    
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LOG_WARNING_FMT("VisualEditor", "Failed to load mesh for vertex data storage: %s", importer.GetErrorString());
        return;
    }
    
    std::function<void(aiNode*)> processNode = [&](aiNode* node) {
        for (unsigned int i = 0; i < node->mNumMeshes; i++) {
            aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
            
            std::vector<GLfloat> vertexData;
            vertexData.reserve(mesh->mNumVertices * 8);
            
            for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
                vertexData.push_back(mesh->mVertices[v].x);
                vertexData.push_back(mesh->mVertices[v].y);
                vertexData.push_back(mesh->mVertices[v].z);
                
                if (mesh->mTextureCoords[0]) {
                    vertexData.push_back(mesh->mTextureCoords[0][v].x);
                    vertexData.push_back(mesh->mTextureCoords[0][v].y);
                } else {
                    vertexData.push_back(0.0f);
                    vertexData.push_back(0.0f);
                }
                
                if (mesh->mNormals) {
                    vertexData.push_back(-mesh->mNormals[v].x);
                    vertexData.push_back(-mesh->mNormals[v].y);
                    vertexData.push_back(-mesh->mNormals[v].z);
                } else {
                    vertexData.push_back(0.0f);
                    vertexData.push_back(1.0f);
                    vertexData.push_back(0.0f);
                }
            }
            
            m_originalVertexData.push_back(vertexData);
            m_meshVertexCounts.push_back(mesh->mNumVertices);
        }
        
        for (unsigned int i = 0; i < node->mNumChildren; i++) {
            processNode(node->mChildren[i]);
        }
    };
    
    processNode(scene->mRootNode);
    LOG_INFO_FMT("VisualEditor", "Stored vertex data for %zu meshes", m_originalVertexData.size());
}

void VisualSkeletonEditor::GenerateWeightsFromMesh() {
    // Check if all bones are placed
    bool allBonesPlaced = (m_nextBoneIndex >= static_cast<int>(m_expectedBoneOrder.size()));
    if (!allBonesPlaced) {
        int bonesPlaced = 0;
        for (size_t i = 0; i < m_bonesPlaced.size(); i++) {
            if (m_bonesPlaced[i]) bonesPlaced++;
        }
        LOG_WARNING_FMT("VisualEditor", "Cannot generate weights: not all bones placed (%d / %d)", 
                       bonesPlaced, static_cast<int>(m_expectedBoneOrder.size()));
        return;
    }
    
    if (!m_meshDataLoaded || m_originalVertexData.empty() || m_meshVertexCounts.empty()) {
        LOG_WARNING("VisualEditor", "Cannot generate weights: mesh data not loaded");
        return;
    }
    
    const SkeletonDefinition* skeleton = m_skeletonEditor.GetSkeleton();
    if (!skeleton) {
        LOG_WARNING("VisualEditor", "Cannot generate weights: skeleton not loaded");
        return;
    }
    
    LOG_INFO("VisualEditor", "Generating vertex weights from mesh geometry...");
    
    m_weights.Clear();
    
    std::vector<std::pair<std::string, glm::vec3>> bonePositions;
    auto boneNames = skeleton->GetAllBoneNames();
    for (const std::string& boneName : boneNames) {
        glm::vec3 worldPos = CalculateBoneWorldPosition(boneName);
        bonePositions.push_back({boneName, worldPos});
    }
    
    if (bonePositions.empty()) {
        LOG_WARNING("VisualEditor", "No bones found for weight generation");
        return;
    }
    
    std::map<std::string, int> boneNameToID;
    for (const std::string& boneName : boneNames) {
        const BoneDefinition* bone = skeleton->GetBone(boneName);
        if (bone) {
            boneNameToID[boneName] = bone->id;
        }
    }
    
    size_t totalVertices = 0;
    size_t verticesWithWeights = 0;
    size_t verticesSkipped = 0;
    
    // Process each mesh
    for (size_t meshIdx = 0; meshIdx < m_originalVertexData.size() && meshIdx < m_meshVertexCounts.size(); meshIdx++) {
        const std::vector<GLfloat>& meshVertices = m_originalVertexData[meshIdx];
        unsigned int vertexCount = m_meshVertexCounts[meshIdx];
        
        for (unsigned int v = 0; v < vertexCount; v++) {
            size_t baseIdx = v * 8;
            if (baseIdx + 2 >= meshVertices.size()) continue;
            
            totalVertices++;
            
            glm::vec3 vertexPos(
                meshVertices[baseIdx + 0],
                meshVertices[baseIdx + 1],
                meshVertices[baseIdx + 2]
            );
            
            const int maxBonesPerVertex = 4;
            std::vector<std::pair<float, std::string>> boneDistances;
            
            for (const auto& bonePair : bonePositions) {
                float distance = glm::length(vertexPos - bonePair.second);
                float invDistSq = 1.0f / (distance * distance + 0.0001f);
                boneDistances.push_back({invDistSq, bonePair.first});
            }
            
            std::sort(boneDistances.begin(), boneDistances.end(), 
                     [](const auto& a, const auto& b) { return a.first > b.first; });
            
            int numBones = std::min(maxBonesPerVertex, static_cast<int>(boneDistances.size()));
            if (numBones == 0) {
                verticesSkipped++;
                continue; // Should never happen if we have bones
            }
            
            unsigned int globalVertexIndex = v;
            for (size_t prevMesh = 0; prevMesh < meshIdx; prevMesh++) {
                globalVertexIndex += m_meshVertexCounts[prevMesh];
            }
            
            float totalInvDist = 0.0f;
            for (int i = 0; i < numBones; i++) {
                totalInvDist += boneDistances[i].first;
            }
            
            float totalWeight = 0.0f;
            
            if (totalInvDist > 0.0f) {
                for (int i = 0; i < numBones; i++) {
                    float weight = boneDistances[i].first / totalInvDist;
                    if (weight > 0.0001f) {
                        std::string boneName = boneDistances[i].second;
                        auto idIt = boneNameToID.find(boneName);
                        if (idIt != boneNameToID.end()) {
                            m_weights.SetWeight(globalVertexIndex, idIt->second, weight);
                            totalWeight += weight;
                        }
                    }
                }
            }
            
            if (totalWeight < 0.001f) {
                if (!boneDistances.empty()) {
                    std::string closestBone = boneDistances[0].second;
                    auto idIt = boneNameToID.find(closestBone);
                    if (idIt != boneNameToID.end()) {
                        m_weights.SetWeight(globalVertexIndex, idIt->second, 1.0f);
                        totalWeight = 1.0f;
                    } else if (!boneNameToID.empty()) {
                            int fallbackBoneID = boneNameToID.begin()->second;
                            m_weights.SetWeight(globalVertexIndex, fallbackBoneID, 1.0f);
                            totalWeight = 1.0f;
                            LOG_WARNING_FMT("VisualEditor", "Bone name '%s' not found in ID map for vertex %u, using fallback bone ID %d", 
                                          closestBone.c_str(), globalVertexIndex, fallbackBoneID);
                        }
                    }
                }
            } else if (totalWeight < 0.99f) {
                float remaining = 1.0f - totalWeight;
                std::string strongestBone = boneDistances[0].second;
                auto idIt = boneNameToID.find(strongestBone);
                if (idIt != boneNameToID.end()) {
                    std::vector<VertexWeight> existingWeights;
                    m_weights.GetWeights(globalVertexIndex, existingWeights);
                    float currentWeight = 0.0f;
                    for (const auto& w : existingWeights) {
                        if (w.boneID == idIt->second) {
                            currentWeight = w.weight;
                            break;
                        }
                    }
                    m_weights.SetWeight(globalVertexIndex, idIt->second, currentWeight + remaining);
                    totalWeight = 1.0f;
                }
            }
            
            if (totalWeight > 0.0f) {
                verticesWithWeights++;
            }
        }
    }
    
    LOG_INFO_FMT("VisualEditor", "Generated weights for %zu/%zu vertices (skipped: %zu)", 
                 verticesWithWeights, totalVertices, verticesSkipped);
    
    if (verticesWithWeights < totalVertices) {
        LOG_WARNING_FMT("VisualEditor", "Warning: %zu vertices did not get weights assigned!", 
                       totalVertices - verticesWithWeights);
    } else {
        LOG_INFO("VisualEditor", "Success: All vertices have weights assigned!");
    }
    
    if (!m_skeletonFilePath.empty()) {
        SaveRig(m_skeletonFilePath);
        LOG_INFO_FMT("VisualEditor", "Weights saved to: %s.weights", m_skeletonFilePath.c_str());
    } else if (!m_meshFilePath.empty()) {
        std::string weightsPath = m_meshFilePath;
        size_t lastDot = weightsPath.find_last_of('.');
        if (lastDot != std::string::npos) {
            weightsPath = weightsPath.substr(0, lastDot) + ".weights";
        } else {
            weightsPath += ".weights";
        }
        std::ofstream file(weightsPath);
        if (file.is_open()) {
            file << "# Vertex Weights\n";
            file << "# Format: vertexIndex|boneID1:weight1,boneID2:weight2,...\n\n";
            
            size_t weightCount = 0;
            for (size_t i = 0; i < m_weights.GetVertexCount(); i++) {
                auto weights = m_weights.GetVertexWeights(i);
                if (weights.empty()) continue;
                
                file << i << "|";
                for (size_t j = 0; j < weights.size(); j++) {
                    if (j > 0) file << ",";
                    file << weights[j].boneID << ":" << weights[j].weight;
                }
                file << "\n";
                weightCount++;
            }
            file.close();
            LOG_INFO_FMT("VisualEditor", "Weights saved to: %s", weightsPath.c_str());
        }
    }
}

void VisualSkeletonEditor::UpdateMeshVertexBuffers() {
    if (!m_mesh || m_originalVertexData.empty() || m_meshVertexCounts.empty()) {
        return;
    }
    
    const SkeletonDefinition* skeleton = m_skeletonEditor.GetSkeleton();
    if (!skeleton || !m_animateMesh) {
        m_mesh->UpdateVertexPositions(m_originalVertexData, m_meshVertexCounts);
        return;
    }
    
    std::map<std::string, glm::mat4> boneTransforms;
    auto boneNames = skeleton->GetAllBoneNames();
    
    for (const std::string& boneName : boneNames) {
        const BoneDefinition* bone = skeleton->GetBone(boneName);
        if (!bone) continue;
        
        glm::mat4 currentModelTransform = glm::mat4(1.0f);
        std::string currentBone = boneName;
        while (!currentBone.empty()) {
            const BoneDefinition* b = skeleton->GetBone(currentBone);
            if (!b) break;
            
            glm::mat4 localTransform = b->GetLocalTransform();
            currentModelTransform = localTransform * currentModelTransform;
            
            currentBone = b->parentName;
        }
        
        glm::mat4 inverseBindPose = glm::mat4(1.0f);
        bool useOffsetMatrix = (glm::determinant(bone->offsetMatrix) != 0.0f && 
                               glm::abs(glm::determinant(bone->offsetMatrix)) > 0.0001f);
        
        if (useOffsetMatrix) {
            inverseBindPose = bone->offsetMatrix;
        } else {
            glm::mat4 bindPoseTransform = glm::mat4(1.0f);
            std::string bindBone = boneName;
            while (!bindBone.empty()) {
                const BoneDefinition* b = skeleton->GetBone(bindBone);
                if (!b) break;
                
                glm::vec3 bindPos = b->position;
                glm::quat bindRot = b->rotation;
                glm::vec3 bindScale = b->scale;
                
                auto bindIt = m_bindPosePositions.find(bindBone);
                if (bindIt != m_bindPosePositions.end()) {
                    bindPos = bindIt->second;
                }
                
                auto rotIt = m_bindPoseRotations.find(bindBone);
                if (rotIt != m_bindPoseRotations.end()) {
                    bindRot = rotIt->second;
                }
                
                glm::mat4 T = glm::translate(glm::mat4(1.0f), bindPos);
                glm::mat4 R = glm::mat4_cast(bindRot);
                glm::mat4 S = glm::scale(glm::mat4(1.0f), bindScale);
                glm::mat4 localBind = T * R * S;
                
                bindPoseTransform = localBind * bindPoseTransform;
                bindBone = b->parentName;
            }
            inverseBindPose = glm::inverse(bindPoseTransform);
        }
        
        boneTransforms[boneName] = currentModelTransform * inverseBindPose;
    }
    
    std::vector<std::vector<GLfloat>> animatedVertexData = m_originalVertexData;
    size_t verticesWithWeights = 0;
    size_t totalVerticesProcessed = 0;
    
    for (size_t meshIdx = 0; meshIdx < animatedVertexData.size() && meshIdx < m_meshVertexCounts.size(); meshIdx++) {
        std::vector<GLfloat>& meshVertices = animatedVertexData[meshIdx];
        unsigned int vertexCount = m_meshVertexCounts[meshIdx];
        
        for (unsigned int v = 0; v < vertexCount; v++) {
            size_t baseIdx = v * 8;
            if (baseIdx + 2 >= meshVertices.size()) continue;
            
            totalVerticesProcessed++;
            
            glm::vec3 originalPos(
                meshVertices[baseIdx + 0],
                meshVertices[baseIdx + 1],
                meshVertices[baseIdx + 2]
            );
            
            unsigned int globalVertexIndex = v;
            if (meshIdx > 0) {
                for (size_t i = 0; i < meshIdx; i++) {
                    globalVertexIndex += m_meshVertexCounts[i];
                }
            }
            
            std::vector<VertexWeight> weights;
            m_weights.GetWeights(globalVertexIndex, weights);
            
            glm::vec3 skinnedPos = originalPos;
            if (!weights.empty()) {
                verticesWithWeights++;
                
                glm::vec4 pos4(originalPos, 1.0f);
                glm::vec4 transformedPos(0.0f);
                float totalWeight = 0.0f;
                int missingBoneCount = 0;
                int missingTransformCount = 0;
                
                for (const VertexWeight& weight : weights) {
                    const BoneDefinition* bone = skeleton->GetBone(weight.boneID);
                    if (!bone) {
                        missingBoneCount++;
                        static std::set<int> loggedMissingBones;
                        if (loggedMissingBones.find(weight.boneID) == loggedMissingBones.end()) {
                            LOG_WARNING_FMT("VisualEditor", "Bone ID %d not found in skeleton (vertex %u)", 
                                          weight.boneID, globalVertexIndex);
                            loggedMissingBones.insert(weight.boneID);
                        }
                        continue;
                    }
                    
                    std::string boneName = bone->name;
                    auto transformIt = boneTransforms.find(boneName);
                    if (transformIt != boneTransforms.end()) {
                        glm::vec4 transformed = transformIt->second * pos4;
                        transformedPos += transformed * weight.weight;
                        totalWeight += weight.weight;
                    } else {
                        missingTransformCount++;
                        static std::set<std::string> loggedMissingTransforms;
                        if (loggedMissingTransforms.find(boneName) == loggedMissingTransforms.end()) {
                            LOG_WARNING_FMT("VisualEditor", "Transform not found for bone '%s' (ID %d)", 
                                          boneName.c_str(), weight.boneID);
                            loggedMissingTransforms.insert(boneName);
                        }
                    }
                }
                
                if (totalWeight > 0.0f) {
                    if (totalWeight < 1.0f) {
                        transformedPos += pos4 * (1.0f - totalWeight);
                    }
                    skinnedPos = glm::vec3(transformedPos.x, transformedPos.y, transformedPos.z);
                } else {
                    skinnedPos = originalPos;
                    static int loggedNoTransformCount = 0;
                    if (loggedNoTransformCount < 10 && (missingBoneCount > 0 || missingTransformCount > 0)) {
                        LOG_WARNING_FMT("VisualEditor", "Vertex %u has weights but no valid transforms: %d missing bones, %d missing transforms",
                                       globalVertexIndex, missingBoneCount, missingTransformCount);
                        loggedNoTransformCount++;
                    }
                }
            }
            
            meshVertices[baseIdx + 0] = skinnedPos.x;
            meshVertices[baseIdx + 1] = skinnedPos.y;
            meshVertices[baseIdx + 2] = skinnedPos.z;
        }
    }
    
    m_mesh->UpdateVertexPositions(animatedVertexData, m_meshVertexCounts);
    
    static int updateCount = 0;
    updateCount++;
    if (updateCount % 60 == 0 || updateCount == 1) {
        LOG_INFO_FMT("VisualEditor", "Updated vertex buffers: %zu meshes, %zu vertices processed, %zu with weights", 
                     animatedVertexData.size(), totalVerticesProcessed, verticesWithWeights);
        
        if (verticesWithWeights == 0) {
            LOG_WARNING("VisualEditor", "No vertices have weights! Mesh won't animate. Load weights file or generate weights.");
        } else {
            LOG_INFO_FMT("VisualEditor", "Mesh animation applied: %zu vertices skinned out of %zu total", 
                        verticesWithWeights, totalVerticesProcessed);
        }
    }
}

} // namespace Skeleton
} // namespace Editors
} // namespace Engine

