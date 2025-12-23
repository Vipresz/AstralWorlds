#include "CameraSystemEditor.h"
#include "GlobalConstants.h"
#include "config/ConfigManager.h"
#include "../../engine/platform/render/Window.h"
#include "../../engine/platform/render/Shader.h"
#include "../../engine/platform/render/Model.h"
#include "../../engine/platform/Logger.h"
#include "objects/ObjectRenderer.h"
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>
#include <chrono>
#include <unordered_set>
#include <set>
#include <tuple>
#include <fstream>
#include <vector>
#include <cstddef>
#include <algorithm>
#include <random>
#include <functional>

// Nuklear implementation is now in nuklear/NuklearUI.cpp
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
// Note: NK_IMPLEMENTATION is defined in nuklear/NuklearUI.cpp
#include "../../../game_libs/Nuklear/nuklear.h"
#include "../../../game_libs/Nuklear/demo/glfw_opengl3/nuklear_glfw_gl3.h"

using namespace Engine::Platform;
using namespace Engine::Platform::Render;
using EngineLogging::Logger;
using namespace Engine::Editors::CameraSystem::Constants;

namespace Engine {
namespace Editors {
namespace CameraSystem {

// SceneObject destructor - defined here so Model type is complete
SceneObject::~SceneObject() {
    if (model) {
        delete model;
        model = nullptr;
    }
    if (animationPlayer) {
        delete animationPlayer;
        animationPlayer = nullptr;
    }
}

CameraSystemEditor::CameraSystemEditor()
    : m_window(nullptr)
    , m_objectRenderer(nullptr)
    , m_captureEnabled(false)
    , m_tofDataEnabled(false)
    , m_tofColorCodeByCamera(false)  // Color coding disabled by default (points are white)
    , m_captureOutputDir("C:\\Users\\inque\\OneDrive\\Desktop\\Work\\games\\thegame\\build\\bin\\Release\\pics")
    , m_vipresEnabled(false)
    , m_vipresComputeColor(false)
    , m_vipresCameraEnabled()
    , m_vipresSamplingDensity(SAMPLING_DENSITY_DEFAULT)
    , m_vipresDepthLimit(DEPTH_LIMIT_DEFAULT)
    , m_vipresGriddingEnabled(false)
    , m_vipresRemeshEnabled(false)
    , m_vipresAutoRecomputeMesh(false)
    , m_vipresRemeshAlgorithm(0)  // Default to GriddedQuads
    , m_vipresRemeshMaxDistance(REMESH_MAX_DISTANCE_DEFAULT)
    , m_vipresRemeshMaxNeighbors(REMESH_MAX_NEIGHBORS_DEFAULT)
    , m_vipresRemeshProjectionPlane(0)  // Default to XY plane
    , m_vipresRemeshDelaunayThreshold(REMESH_DELAUNAY_THRESHOLD_DEFAULT)
    , m_vipresRemeshPoissonRadius(REMESH_POISSON_RADIUS_DEFAULT)
    , m_vipresRemeshPoissonDepth(REMESH_POISSON_DEPTH_DEFAULT)
    , m_vipresRemeshPoissonDensity(REMESH_POISSON_DENSITY_DEFAULT)
    , m_vipresRemeshPoissonSmoothNormals(true)
    , m_vipresRemeshFCTMSliceThickness(REMESH_FCTM_SLICE_THICKNESS_DEFAULT)
    , m_vipresRemeshGeodesicKNeighbors(REMESH_GEODESIC_K_NEIGHBORS_DEFAULT)
    , m_vipresRemeshGeodesicNumSeeds(REMESH_GEODESIC_NUM_SEEDS_DEFAULT)
    , m_vipresRemeshGeodesicInteriorBias(REMESH_GEODESIC_INTERIOR_BIAS_DEFAULT)
    , m_vipresFreeCameraObjectEnabled(true)
    , m_vipresNoisyTofEnabled(false)
    , m_vipresNoiseAmount(NOISE_AMOUNT_DEFAULT)
    , m_vipresGridSize(GRID_SIZE_DEFAULT)
    , m_vipresGridMode(TOFGridMode::GridValue)
    , m_tofLowPassCutoff(LOW_PASS_CUTOFF_DEFAULT)
    , m_tofSamplingRate(TOF_SAMPLING_RATE_DEFAULT)
    , m_tofLowPassEnabled(false) // Low-pass filter disabled by default
    , m_tofTsdfEnabled(false)   // TSDF disabled by default
    , m_tofTsdfStoreColor(false)  // TSDF color storage disabled by default
    , m_tofTsdfTruncationDistance(TSDF_TRUNCATION_DISTANCE_DEFAULT)
    , m_tofTsdfWeightDecay(TSDF_WEIGHT_DECAY_DEFAULT)
    , m_smartOccupancyEnabled(false)
    , m_smartOccupancyNeighborConsistency(false)
    , m_smartOccupancyNeighborRadius(2.0f)  // Default: 2 grid cells radius
    , m_smartOccupancyResidualThreshold(1.5f)  // Default: 1.5 units (r0) - for ±1 unit noise, outliers are >1.5 units
    , m_smartOccupancyPersistence(false)
    , m_smartOccupancyPersistenceGamma(2.0f)  // Default: γ=2 (less aggressive than 4)
    , m_smartOccupancyVariance(false)
    , m_smartOccupancyVarianceThreshold(1.0f)  // Default: σ0=1.0 (matches ±1 unit noise stddev)
    , m_smartOccupancyEviction(false)
    , m_smartOccupancyEvictionDecay(2.0f)  // Decay value used in (decay^count)/100
    , m_smartOccupancyEvictionIncrement(1.5f)  // Multiply weight by this when occupied
    , m_smartOccupancyEvictionMinStartValue(0.1f)  // Minimum start value
    , m_smartOccupancyEviction2(false)
    , m_smartOccupancyEviction2Radius(2.0f)  // Default: check neighbors within 2 grid units
    , m_smartOccupancyEviction2MinNeighbors(3)  // Default: require at least 3 neighbors
    , m_weightThreshold(0.5f)  // Default: filter points below 0.5 weight
    , m_currentFPS(0.0f)
    , m_fpsUpdateTime(0.0f)
    , m_fpsFrameCount(0)
    , m_filteredTofWriteBuffer(0)
    , m_filteredTofReadBuffer(1)
    , m_pointCloudWriteBuffer(0)
    , m_pointCloudReadBuffer(1)
    , m_pointCloudVAO(0)
    , m_pointCloudVBO(0)
    , m_pointCloudBuffersInitialized(false)
    , m_remeshVAO(0)
    , m_remeshVBO(0)
    , m_remeshEBO(0)
    , m_remeshBuffersInitialized(false)
    , m_remeshNeedsUpdate(false)
    , m_captureInterval(1)
    , m_lastCapturedFrame(-1)
    , m_nextCameraToCapture(0)
    , m_glfwWindow(nullptr)
    , m_meshShader(nullptr)
    , m_pendingCameraSwitch(SIZE_MAX)
    , m_hasSafeParameters(false)
    , m_framesSinceStartup(0)
    , m_justRestoredParameters(false)
    , m_freeCameraPos(10.0f, 10.0f, 5.0f)
    , m_freeCameraYaw(0.0f)
    , m_freeCameraPitch(0.0f)
    , m_movementSpeed(CAMERA_MOVEMENT_SPEED)
    , m_mouseRightDown(false)
    , m_lastMouseX(0.0)
    , m_lastMouseY(0.0)
    // Nuklear UI is now handled by m_nuklearUI member
    , m_globalAnimationPlaying(false)
    , m_globalAnimationTime(0.0f)
    , m_configLoaded(false)
    , m_sceneTime(0.0f)
    , m_totalDuration(0.0f)
    , m_numCameras(0)
    , m_totalFrames(0)
    , m_currentFrame(0)
    , m_frameProcessingCounter(0)
    , m_isPlaying(false)
    , m_loopEnabled(false)
{
    // Initialize low-pass filter coefficients with default values (for new algorithms module)
    Algorithms::ComputeLowPassFilterCoefficients(m_tofLowPassCutoff, m_tofSamplingRate, m_lowPassFilterCoefficients);
        
    // Initialize static camera frame arrays
    for (int i = 0; i < MAX_CAMERAS; i++) {
        m_cameraFrameCounts[i] = 0;
        for (int j = 0; j < MAX_FRAMES; j++) {
            m_cameraFrames[i][j].tick = -1;  // -1 means unused
            m_cameraFrames[i][j].position = glm::vec3(0.0f);
            m_cameraFrames[i][j].orientation = glm::vec3(0.0f, 0.0f, 1.0f);  // Default forward
        }
    }
}

CameraSystemEditor::~CameraSystemEditor() {
    ShutdownNuklear();
    Cleanup();
    
    // Clean up object renderer
    if (m_objectRenderer) {
        delete m_objectRenderer;
        m_objectRenderer = nullptr;
    }
}

bool CameraSystemEditor::Initialize(int windowWidth, int windowHeight) {
    LOG_INFO("CameraSystem", "Initializing camera system editor...");
    
    try {
        m_window = new Window(windowWidth, windowHeight);
        int initResult = m_window->Initialize();
        if (initResult != 0) {
            LOG_ERROR_FMT("CameraSystem", "Failed to initialize window (error code: %d)", initResult);
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("CameraSystem", "Exception during window creation: %s", e.what());
        return false;
    }
    
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) {
        LOG_ERROR("CameraSystem", "Failed to get GLFW window context!");
        return false;
    }
    
    m_glfwWindow = window;
    
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    glfwSetWindowTitle(window, "Camera System Editor");
    glfwSetWindowShouldClose(window, GLFW_FALSE);
    glfwShowWindow(window);
    
    if (!CreateShaders()) {
        LOG_ERROR("CameraSystem", "Failed to create shaders");
        return false;
    }
    
    // Initialize world (grid shader and buffers)
    if (!m_world.Initialize()) {
        LOG_ERROR("CameraSystem", "Failed to initialize world");
        return false;
    }
    
    float aspect = (float)windowWidth / (float)windowHeight;
    m_projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, Constants::FAR_PLANE);
    glViewport(0, 0, windowWidth, windowHeight);
    UpdateCamera();
       
    if (!InitializeNuklear()) {
        LOG_ERROR("CameraSystem", "Failed to initialize Nuklear GUI");
        return false;
    }
    
    // Initialize UI system
    m_ui.Initialize(&m_nuklearUI, m_glfwWindow);
    
    // Initialize object renderer
    m_objectRenderer = new ObjectRenderer();
    
    // Initialize default cameras
    InitializeDefaultCameras();
    
    if (window) {
        glfwSetWindowUserPointer(window, this);
        
        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* win, int width, int height) {
            CameraSystemEditor* editor = static_cast<CameraSystemEditor*>(glfwGetWindowUserPointer(win));
            if (!editor || !editor->m_window) return;
            
            glViewport(0, 0, width, height);
            float aspect = (float)width / (float)height;
            editor->m_projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100000.0f);
        });
        
        // Scroll callback disabled - Nuklear will handle scroll manually if needed
        // We don't install this to avoid conflicts with camera controls
    }
    
    LOG_INFO("CameraSystem", "Camera system editor initialized successfully");
    return true;
}

bool CameraSystemEditor::CreateShaders() {
    LOG_INFO("CameraSystem", "Compiling shaders...");
    
    // Use the same shader files as the main application (which works correctly with lighting)
    // The shader files are in engine/platform/render/shaders/
    // Try the path relative to project root first, then try relative to build directory
    m_meshShader = new Shader();
    
    // Try path from project root (if running from thegame/ directory)
    m_meshShader->CreateFromFiles("engine/platform/render/shaders/shader.vert", "engine/platform/render/shaders/shader.frag");
    
    // If that failed, try path from build/bin/Release (go up 3 levels to project root)
    if (!m_meshShader->IsValid()) {
        m_meshShader->CreateFromFiles("../../engine/platform/render/shaders/shader.vert", "../../engine/platform/render/shaders/shader.frag");
    }
    
    // If that also failed, try the same path as main.cpp uses (shaders/ relative to executable)
    // This assumes shaders are copied to a shaders/ directory next to the executable
    if (!m_meshShader->IsValid()) {
        m_meshShader->CreateFromFiles("shaders/shader.vert", "shaders/shader.frag");
    }
    if (!m_meshShader->IsValid()) {
        LOG_ERROR("CameraSystem", "Mesh shader failed to compile");
        return false;
    }
    m_meshShader->Validate();
    LOG_INFO("CameraSystem", "Mesh shader loaded successfully (using engine shader files)");
    
    // Grid shader is now handled by World class
    bool valid = m_meshShader->IsValid();
    if (valid) {
        LOG_INFO("CameraSystem", "Shaders compiled successfully");
    } else {
        LOG_ERROR("CameraSystem", "Shader compilation failed");
    }
    
    return valid;
}

bool CameraSystemEditor::InitializeNuklear() {
    return m_nuklearUI.Initialize(m_glfwWindow);
}

void CameraSystemEditor::ShutdownNuklear() {
    m_nuklearUI.Shutdown();
}

// Reset frame arrays (editor-specific state management)
void CameraSystemEditor::ResetFrameArrays() {
    // Reset object frame arrays
    for (int i = 0; i < MAX_OBJECTS; i++) {
        m_objectFrameCounts[i] = 0;
        m_objectsUseFrames[i] = false;
        for (int j = 0; j < MAX_FRAMES; j++) {
            m_objectFrames[i][j].frame = -1;
            m_objectFrames[i][j].position = glm::vec3(0.0f);
        }
    }
    
    // Reset static camera frame arrays
    m_numCameras = 0;
    for (int i = 0; i < MAX_CAMERAS; i++) {
        m_cameraFrameCounts[i] = 0;
        for (int j = 0; j < MAX_FRAMES; j++) {
            m_cameraFrames[i][j].tick = -1;
            m_cameraFrames[i][j].position = glm::vec3(0.0f);
            m_cameraFrames[i][j].orientation = glm::vec3(0.0f, 0.0f, 1.0f);
        }
    }
}

bool CameraSystemEditor::LoadConfig(const std::string& configFile) {
    SystemConfig config;
    try {
        if (!ConfigLoader::LoadFromFile(configFile, config)) {
            LOG_ERROR_FMT("CameraSystem", "Failed to parse configuration file: %s", configFile.c_str());
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("CameraSystem", "Exception parsing config file '%s': %s", configFile.c_str(), e.what());
        return false;
    } catch (...) {
        LOG_ERROR_FMT("CameraSystem", "Unknown exception parsing config file: %s", configFile.c_str());
        return false;
    }
    
    // Load capture settings
    auto captureSettings = ConfigManager::LoadCaptureSettings(config, m_configLoaded, m_captureEnabled);
    if (!m_configLoaded) {
        m_captureEnabled = captureSettings.enabled;
    }
    if (!captureSettings.outputDir.empty()) {
        m_captureOutputDir = captureSettings.outputDir;
    }
    m_captureInterval = captureSettings.interval;
    
    // Clear existing objects and reset frame arrays
    m_objects.clear();
    m_objectPaths.clear();
    m_objectStartTimes.clear();
    ResetFrameArrays();
    
    // Load cameras (animated or legacy)
    if (!config.animatedCameras.empty() && config.totalFrames > 0) {
        ConfigManager::LoadAnimatedCameras<CameraFrame>(
            config,
            m_cameras,
            m_cameraFrames,
            m_cameraFrameCounts,
            m_numCameras,
            m_totalFrames,
            m_currentFrame,
            m_isPlaying,
            m_freeCameraPos,
            m_freeCameraYaw,
            m_freeCameraPitch,
            m_cameraSystem
        );
    } else {
        ConfigManager::LoadLegacyCamera(
            config,
            m_cameras,
            m_totalFrames,
            m_currentFrame,
            m_freeCameraPos,
            m_freeCameraYaw,
            m_freeCameraPitch,
            m_cameraSystem
        );
    }
    
    m_totalDuration = config.totalDuration;
    m_sceneTime = 0.0f;
    
    // Load objects from config
    ConfigManager::LoadObjects<ObjectFrame>(
        config,
        configFile,
        m_objectManager,
        m_objects,
        m_objectPaths,
        m_objectStartTimes,
        m_objectFrames,
        m_objectFrameCounts,
        m_objectsUseFrames,
        m_totalFrames,
        m_numCameras
    );
    
    m_configLoaded = true;
    m_globalAnimationPlaying = false;
    
    // Update animated cameras if loaded (set initial positions from first keyframe)
    if (m_totalFrames > 0 && m_numCameras > 0) {
        m_currentFrame = 0;
    }
    
    // Load lights from config
    m_lights = config.lights;
    
    // Update object positions to frame 0 immediately after loading
    UpdatePathFollowing(0.0f);
    
    // Initialize cameras from config
    InitializeCamerasFromConfig();
    
    // Update camera to apply config settings
    try {
        UpdateCamera();
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("CameraSystem", "Exception in UpdateCamera during load: %s", e.what());
    } catch (...) {
        LOG_ERROR("CameraSystem", "Unknown exception in UpdateCamera during load");
    }
    
    return true;
}

void CameraSystemEditor::InitializeCamerasFromConfig() {
    // Initialize camera system from loaded cameras
    size_t activeIdx = m_cameraSystem.GetActiveCameraIndex();
    m_cameraSystem.InitializeFromConfig(m_cameras, activeIdx, m_freeCameraPos, m_freeCameraYaw, m_freeCameraPitch);
    m_cameraSystem.SetActiveCameraIndex(activeIdx);
}

bool CameraSystemEditor::AddObject(const std::string& meshFilePath, const std::string& animationFilePath, const std::string& name) {
    auto obj = std::make_unique<SceneObject>();
    
    // Set name
    if (!name.empty()) {
        obj->name = name;
    } else {
        // Extract name from file path
        size_t lastSlash = meshFilePath.find_last_of("/\\");
        size_t lastDot = meshFilePath.find_last_of('.');
        if (lastSlash != std::string::npos && lastDot != std::string::npos && lastDot > lastSlash) {
            obj->name = meshFilePath.substr(lastSlash + 1, lastDot - lastSlash - 1);
        } else {
            obj->name = "Object_" + std::to_string(m_objects.size());
        }
    }
    
    obj->meshFilePath = meshFilePath;
    obj->animationFilePath = animationFilePath;
    
    // Load model
    obj->model = new Model();
    try {
        if (!std::filesystem::exists(meshFilePath)) {
            LOG_ERROR_FMT("CameraSystem", "Model file does not exist: %s", meshFilePath.c_str());
            delete obj->model;
            obj->model = nullptr;
            return false;
        }
        obj->model->LoadModel(meshFilePath);
        if (obj->model->GetMeshCount() == 0) {
            LOG_ERROR_FMT("CameraSystem", "Model loaded but has no meshes: %s", meshFilePath.c_str());
            delete obj->model;
            obj->model = nullptr;
            return false;
        }
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("CameraSystem", "Exception loading model '%s': %s", meshFilePath.c_str(), e.what());
        delete obj->model;
        obj->model = nullptr;
        return false;
    } catch (...) {
        LOG_ERROR_FMT("CameraSystem", "Failed to load model: %s", meshFilePath.c_str());
        delete obj->model;
        obj->model = nullptr;
        return false;
    }
    
    // Animation disabled - only rendering static OBJ files
    obj->isAnimating = false;
    if (obj->animationPlayer) {
        delete obj->animationPlayer;
        obj->animationPlayer = nullptr;
    }
    
    m_objects.push_back(std::move(obj));
    return true;
}

void CameraSystemEditor::RemoveObject(size_t index) {
    if (index >= m_objects.size()) return;
    
    m_objects.erase(m_objects.begin() + index);
}

SceneObject* CameraSystemEditor::GetObject(size_t index) {
    if (index >= m_objects.size()) return nullptr;
    return m_objects[index].get();
}

const SceneObject* CameraSystemEditor::GetObject(size_t index) const {
    if (index >= m_objects.size()) return nullptr;
    return m_objects[index].get();
}

void CameraSystemEditor::InitializeDefaultCameras() {
    m_cameraSystem.InitializeDefaultCameras(m_cameras);
    
    // Initialize VIPRES camera enabled list
    m_vipresCameraEnabled.clear();
    m_vipresCameraEnabled.resize(m_cameras.size(), true);
    
    // Initialize free camera position from first camera
    if (!m_cameras.empty()) {
        size_t activeIdx = m_cameraSystem.GetActiveCameraIndex();
        m_cameraSystem.InitializeFromConfig(m_cameras, activeIdx, m_freeCameraPos, m_freeCameraYaw, m_freeCameraPitch);
        m_cameraSystem.SetActiveCameraIndex(activeIdx);
    }
}

void CameraSystemEditor::AddCamera(const std::string& name, const glm::vec3& position, const glm::vec3& target, bool canMove) {
    m_cameraSystem.AddCamera(name, position, target, canMove, m_cameras);
    // Add corresponding entry to VIPRES camera enabled list (default to enabled for anchor cameras)
    m_vipresCameraEnabled.push_back(true); // Enable by default for all cameras
}

void CameraSystemEditor::SwitchCamera(size_t cameraIndex) {
    size_t activeIdx = m_cameraSystem.GetActiveCameraIndex();
    m_cameraSystem.SwitchCamera(cameraIndex, m_cameras, activeIdx, m_freeCameraPos, m_view);
    m_cameraSystem.SetActiveCameraIndex(activeIdx);
}

void CameraSystemEditor::UpdateCamera() {
    size_t activeIndex = m_cameraSystem.GetActiveCameraIndex();
    m_cameraSystem.UpdateCamera(m_cameras, activeIndex, m_freeCameraPos, m_freeCameraYaw, m_freeCameraPitch, m_view);
}

void CameraSystemEditor::UpdateFreeCamera(float deltaTime) {
    size_t activeIndex = m_cameraSystem.GetActiveCameraIndex();
    m_cameraSystem.UpdateFreeCamera(deltaTime, m_cameras, activeIndex, m_freeCameraPos, m_freeCameraYaw, m_freeCameraPitch, m_movementSpeed, m_glfwWindow);
}

void CameraSystemEditor::HandleMouseInput() {
    size_t activeIndex = m_cameraSystem.GetActiveCameraIndex();
    m_cameraSystem.HandleMouseInput(m_cameras, activeIndex, m_mouseRightDown, m_lastMouseX, m_lastMouseY, 
                                   m_freeCameraPos, m_freeCameraYaw, m_freeCameraPitch, m_glfwWindow);
    UpdateCamera();  // Update view after mouse input
}

void CameraSystemEditor::HandleKeyboardInput() {
    if (!m_glfwWindow) return;
    
    // ESC to exit
    if (glfwGetKey(m_glfwWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(m_glfwWindow, GLFW_TRUE);
    }
    
    // Number keys 1-5 to switch cameras (defer to avoid issues)
    if (glfwGetKey(m_glfwWindow, GLFW_KEY_1) == GLFW_PRESS) {
        if (m_cameras.size() > 0 && 0 < m_cameras.size()) {
            m_pendingCameraSwitch = 0;
        }
    }
    if (glfwGetKey(m_glfwWindow, GLFW_KEY_2) == GLFW_PRESS) {
        if (m_cameras.size() > 1 && 1 < m_cameras.size()) {
            m_pendingCameraSwitch = 1;
        }
    }
    if (glfwGetKey(m_glfwWindow, GLFW_KEY_3) == GLFW_PRESS) {
        if (m_cameras.size() > 2 && 2 < m_cameras.size()) {
            m_pendingCameraSwitch = 2;
        }
    }
    if (glfwGetKey(m_glfwWindow, GLFW_KEY_4) == GLFW_PRESS) {
        if (m_cameras.size() > 3 && 3 < m_cameras.size()) {
            m_pendingCameraSwitch = 3;
        }
    }
    if (glfwGetKey(m_glfwWindow, GLFW_KEY_5) == GLFW_PRESS) {
        if (m_cameras.size() > 4 && 4 < m_cameras.size()) {
            m_pendingCameraSwitch = 4;
        }
    }
    
    // Camera system handles keyboard input
    m_cameraSystem.HandleKeyboardInput(m_cameras, m_pendingCameraSwitch);
}


float CameraSystemEditor::GetObjectLocalTime(size_t objectIndex) const {
    if (objectIndex >= m_objectStartTimes.size()) {
        return 0.0f;
    }
    
    float startTime = m_objectStartTimes[objectIndex];
    float localTime = m_sceneTime - startTime;
    
    // Find path duration
    if (objectIndex < m_objectPaths.size() && !m_objectPaths[objectIndex].empty()) {
        float pathDuration = m_objectPaths[objectIndex].back().time;
        if (localTime > pathDuration) {
            localTime = pathDuration; // Stop at end of path
        }
    }
    
    return std::max(0.0f, localTime);
}

void CameraSystemEditor::UpdateAnimatedCameras() {
    if (m_totalFrames == 0 || m_numCameras == 0) {
        return;
    }
    
    if (m_currentFrame < 0 || m_currentFrame >= m_totalFrames) {
        return;
    }
    
    // Safety check: ensure we have cameras
    if (m_cameras.empty()) {
        return;
    }
    
    // Update each camera based on current frame using static array
    // IMPORTANT: Only update cameras that are NOT free cameras (canMove = false)
    int maxCameras = std::min(m_numCameras, static_cast<int>(m_cameras.size()));
    for (int i = 0; i < maxCameras; i++) {
        // Bounds check
        if (i < 0 || i >= MAX_CAMERAS || i >= static_cast<int>(m_cameras.size())) {
            continue;
        }
        
        // SKIP free cameras - they should not be modified by config/animation
        if (m_cameras[i].canMove) {
            continue;
        }
        
        if (m_cameraFrameCounts[i] == 0) {
            continue;
        }
        
        // SIMPLE: Since we filled all frames, just use m_currentFrame as the index directly
        const CameraFrame* currentFrame = nullptr;
        
        if (m_currentFrame >= 0 && m_currentFrame < m_cameraFrameCounts[i] && m_currentFrame < MAX_FRAMES) {
            currentFrame = &m_cameraFrames[i][m_currentFrame];
            // Verify the frame data is valid
            if (currentFrame->tick != m_currentFrame) {
                // Frame data mismatch - this shouldn't happen
                LOG_WARNING_FMT("CameraSystem", "Camera %d frame %d: tick mismatch (tick=%d)", 
                               i, m_currentFrame, currentFrame->tick);
            }
        } else if (m_cameraFrameCounts[i] > 0) {
            // Fallback to first frame if out of bounds
            currentFrame = &m_cameraFrames[i][0];
            LOG_WARNING_FMT("CameraSystem", "Camera %d: frame %d out of bounds (max=%d), using frame 0", 
                           i, m_currentFrame, m_cameraFrameCounts[i]);
        } else {
            LOG_WARNING_FMT("CameraSystem", "Camera %d: no frame data available", i);
        }
        
        if (currentFrame) {
            // Update camera position and target (only for non-free cameras)
            glm::vec3 oldPos = m_cameras[i].position;
            m_cameras[i].position = currentFrame->position;
            m_cameras[i].target = currentFrame->position + currentFrame->orientation;
            
            if (i == static_cast<int>(m_cameraSystem.GetActiveCameraIndex()) && !m_cameras[i].canMove) {
                static int logCounter = 0;
                if (++logCounter % 30 == 0) {  // Log every 30 frames
                    LOG_INFO_FMT("CameraSystem", "Active camera %d (frame %d): pos=(%.2f,%.2f,%.2f) target=(%.2f,%.2f,%.2f)", 
                                i, m_currentFrame,
                                m_cameras[i].position.x, m_cameras[i].position.y, m_cameras[i].position.z,
                                m_cameras[i].target.x, m_cameras[i].target.y, m_cameras[i].target.z);
                }
            }
        }
    }
}

glm::vec3 CameraSystemEditor::InterpolatePath(const std::vector<PathPoint>& path, float currentTime) {
    if (path.empty()) {
        return glm::vec3(0.0f);
    }
    
    if (path.size() == 1) {
        return path[0].position;
    }
    
    // Find the two points to interpolate between
    for (size_t i = 0; i < path.size() - 1; i++) {
        if (currentTime >= path[i].time && currentTime <= path[i + 1].time) {
            float t0 = path[i].time;
            float t1 = path[i + 1].time;
            float t = (currentTime - t0) / (t1 - t0);
            
            // Linear interpolation
            return path[i].position + t * (path[i + 1].position - path[i].position);
        }
    }
    
    // If past the last point, return the last position
    if (currentTime >= path.back().time) {
        return path.back().position;
    }
    
    // If before the first point, return the first position
    return path[0].position;
}

void CameraSystemEditor::UpdatePathFollowing(float deltaTime) {
    (void)deltaTime; // Parameter not used - path following uses scene time or frames instead
    if (!m_configLoaded) {
        return;
    }
    
    for (size_t i = 0; i < m_objects.size(); i++) {
        if (!m_objects[i]) {
            continue;
        }
        
        // Use frame-based positions if available
        if (i < static_cast<size_t>(MAX_OBJECTS) && m_objectsUseFrames[i] && m_totalFrames > 0) {
            glm::vec3 targetPosition = glm::vec3(0.0f);
            
            // Try to get position from current frame
            if (m_currentFrame >= 0 && m_currentFrame < m_objectFrameCounts[i] && m_currentFrame < MAX_FRAMES) {
                // Directly use m_currentFrame as index - we filled all frames
                glm::vec3 framePosition = m_objectFrames[i][m_currentFrame].position;
                
                // Validate position: check for NaN, infinity, and extreme values only
                // Don't compare with frame 0 - objects are allowed to move!
                bool isValid = true;
                if (std::isnan(framePosition.x) || std::isinf(framePosition.x) ||
                    std::isnan(framePosition.y) || std::isinf(framePosition.y) ||
                    std::isnan(framePosition.z) || std::isinf(framePosition.z)) {
                    isValid = false;
                } else if (std::abs(framePosition.x) > 500.0f || 
                           std::abs(framePosition.y) > 500.0f || 
                           std::abs(framePosition.z) > 500.0f) {
                    isValid = false;
                }
                
                if (isValid) {
                    targetPosition = framePosition;
                }
            }
            
            // Fallback to frame 0 if current frame position is invalid
            if (targetPosition == glm::vec3(0.0f) && m_objectFrameCounts[i] > 0 && m_objectFrames[i][0].frame == 0) {
                glm::vec3 frame0Pos = m_objectFrames[i][0].position;
                if (!std::isnan(frame0Pos.x) && !std::isinf(frame0Pos.x) &&
                    !std::isnan(frame0Pos.y) && !std::isinf(frame0Pos.y) &&
                    !std::isnan(frame0Pos.z) && !std::isinf(frame0Pos.z)) {
                    targetPosition = frame0Pos;
                }
            }
            
            // Only update if position actually changed (avoid unnecessary updates)
            if (m_objects[i]->position != targetPosition) {
                m_objects[i]->position = targetPosition;
            }
        } else if (i < m_objectPaths.size() && !m_objectPaths[i].empty()) {
            // Legacy: use time-based path
            float localTime = GetObjectLocalTime(i);
            glm::vec3 newPosition = InterpolatePath(m_objectPaths[i], localTime);
            
            // Validate path position
            if (std::isnan(newPosition.x) || std::isinf(newPosition.x) ||
                std::isnan(newPosition.y) || std::isinf(newPosition.y) ||
                std::isnan(newPosition.z) || std::isinf(newPosition.z)) {
                newPosition = glm::vec3(0.0f);
            }
            m_objects[i]->position = newPosition;
        }
        
        // Final safety check: ensure position is always valid
        if (std::isnan(m_objects[i]->position.x) || std::isinf(m_objects[i]->position.x) ||
            std::isnan(m_objects[i]->position.y) || std::isinf(m_objects[i]->position.y) ||
            std::isnan(m_objects[i]->position.z) || std::isinf(m_objects[i]->position.z)) {
            m_objects[i]->position = glm::vec3(0.0f);
        }
    }
}

void CameraSystemEditor::UpdateAnimations(float /*deltaTime*/) {
    // Animation disabled - only rendering static OBJ files
}

glm::mat4 CameraSystemEditor::CalculateCameraView(const Camera& cam) const {
    return m_cameraSystem.CalculateCameraView(cam);
}

void CameraSystemEditor::CaptureScreenshots() {
    // Increment frame processing counter to refresh noise at every frame
    m_frameProcessingCounter++;
    
    if (m_cameras.empty()) {
        static int warnCount = 0;
        if (warnCount++ < 3) {
            LOG_WARNING("CameraSystem", "No cameras to capture");
        }
        return;
    }
    
    // Check if we should capture this frame based on capture interval
    // When capture_interval == 1: Capture every frame for every camera
    // When capture_interval > 1: Only capture frames that are multiples of the interval
    if (m_captureInterval > 1) {
        if ((m_currentFrame % m_captureInterval) != 0) {
            // Skip this frame - not a capture interval frame
            return;
        }
    }
    // When capture_interval == 1, we capture every frame (no skipping)
    
    // Reset capture tracking if we've moved to a new frame
    // IMPORTANT: Only start capturing a new frame if the previous frame was fully captured
    if (m_currentFrame != m_lastCapturedFrame) {
        // Check if we finished capturing the previous frame
        bool prevFrameComplete = true;
        if (m_lastCapturedFrame >= 0 && !m_capturedCamerasForFrame.empty()) {
            for (size_t i = 0; i < m_cameras.size(); i++) {
                if (i >= m_capturedCamerasForFrame.size() || !m_capturedCamerasForFrame[i]) {
                    prevFrameComplete = false;
                    LOG_WARNING_FMT("CameraSystem", "Frame %d advanced before all cameras were captured! Camera %zu not captured. Waiting to complete frame %d before starting frame %d.", 
                                  m_lastCapturedFrame, i, m_lastCapturedFrame, m_currentFrame);
                    break;
                }
            }
        }
        
        // Only start capturing the new frame if the previous frame was complete
        // This ensures we capture every frame when capture_interval == 1
        if (prevFrameComplete || m_lastCapturedFrame < 0) {
            if (prevFrameComplete && m_lastCapturedFrame >= 0) {
                LOG_INFO_FMT("CameraSystem", "Completed capture for frame %d (all %zu cameras)", 
                           m_lastCapturedFrame, m_cameras.size());
            }
            
            m_capturedCamerasForFrame.clear();
            m_capturedCamerasForFrame.resize(m_cameras.size(), false);
            m_nextCameraToCapture = 0;
            int oldFrame = m_lastCapturedFrame;
            m_lastCapturedFrame = m_currentFrame;
            LOG_INFO_FMT("CameraSystem", "Starting capture for frame %d (was %d, output dir: %s, %zu cameras)", 
                        m_currentFrame, oldFrame, m_captureOutputDir.c_str(), m_cameras.size());
        } else {
            // Previous frame not complete - don't start capturing new frame yet
            // This will cause the system to continue capturing the previous frame
            return;
        }
    }
    
    // Check if all cameras have been captured for this frame
    bool allCaptured = true;
    for (size_t i = 0; i < m_cameras.size(); i++) {
        if (!m_capturedCamerasForFrame[i]) {
            allCaptured = false;
            break;
        }
    }
    
    if (allCaptured) {
        // All cameras already captured for this frame - wait for next frame
        // This is normal - we capture one camera per render frame to avoid blocking
        return;
    }
    
    int width, height;
    glfwGetFramebufferSize(m_glfwWindow, &width, &height);
    
    std::filesystem::path baseDir(m_captureOutputDir);
    try {
        if (!std::filesystem::exists(baseDir)) {
            std::filesystem::create_directories(baseDir);
            LOG_INFO_FMT("CameraSystem", "Created capture directory: %s", baseDir.string().c_str());
        }
        if (!std::filesystem::is_directory(baseDir)) {
            LOG_ERROR_FMT("CameraSystem", "Capture output path exists but is not a directory: %s", baseDir.string().c_str());
            return;
        }
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("CameraSystem", "Failed to create capture directory '%s': %s", baseDir.string().c_str(), e.what());
        return;
    }
    
    // Save current view matrix and active camera index (don't change what user sees)
    glm::mat4 originalView = m_view;
    size_t originalActiveCamera = m_cameraSystem.GetActiveCameraIndex();
    
    // Capture one camera per frame to avoid blocking
    // Start from m_nextCameraToCapture and find the next uncaptured camera
    size_t camerasToCapture = 0;
    size_t startIndex = m_nextCameraToCapture;
    bool foundUncaptured = false;
    
    for (size_t i = 0; i < m_cameras.size(); i++) {
        size_t idx = (startIndex + i) % m_cameras.size();
        
        if (m_capturedCamerasForFrame[idx]) {
            continue; // Already captured
        }
        
        foundUncaptured = true;
        
        const Camera& cam = m_cameras[idx];
        
        std::filesystem::path cameraDir = baseDir / cam.name;
        try {
            if (!std::filesystem::exists(cameraDir)) {
                std::filesystem::create_directories(cameraDir);
                LOG_INFO_FMT("CameraSystem", "Created camera directory: %s", cameraDir.string().c_str());
            }
        } catch (const std::exception& e) {
            LOG_ERROR_FMT("CameraSystem", "Failed to create camera directory '%s': %s", cameraDir.string().c_str(), e.what());
            continue; // Skip this camera if directory creation fails
        }
        
        // Temporarily set view matrix and active camera for this camera
        m_view = CalculateCameraView(cam);
        m_cameraSystem.SetActiveCameraIndex(idx);
        
        // Render scene with this camera's view to back buffer
        RenderScene();
        
        // Capture the screenshot
        std::filesystem::path filePath = cameraDir / (std::to_string(m_currentFrame) + ".bmp");
        std::string filename = filePath.string();
        
        LOG_INFO_FMT("CameraSystem", "Attempting to capture frame %d from camera '%s' to %s", 
                    m_currentFrame, cam.name.c_str(), filename.c_str());
        
        bool screenshotSaved = SaveScreenshot(filename, width, height);
        
        // Save depth map if TOF data is enabled
        if (m_tofDataEnabled && screenshotSaved) {
            std::filesystem::path depthCameraDir = baseDir / (cam.name + "_depth");
            try {
                if (!std::filesystem::exists(depthCameraDir)) {
                    std::filesystem::create_directories(depthCameraDir);
                    LOG_INFO_FMT("CameraSystem", "Created depth camera directory: %s", depthCameraDir.string().c_str());
                }
            } catch (const std::exception& e) {
                LOG_ERROR_FMT("CameraSystem", "Failed to create depth camera directory '%s': %s", depthCameraDir.string().c_str(), e.what());
            }
            
            std::filesystem::path depthFilePath = depthCameraDir / (std::to_string(m_currentFrame) + ".bmp");
            std::string depthFilename = depthFilePath.string();
            
            if (!SaveDepthMap(depthFilename, width, height, cam)) {
                LOG_WARNING_FMT("CameraSystem", "Failed to save depth map for frame %d from camera '%s'", 
                               m_currentFrame, cam.name.c_str());
            }
        }
        
        if (screenshotSaved) {
            // Mark this camera as captured only if save succeeded
            m_capturedCamerasForFrame[idx] = true;
            m_nextCameraToCapture = (idx + 1) % m_cameras.size();
            camerasToCapture++;
            
            LOG_INFO_FMT("CameraSystem", "Successfully captured frame %d from camera '%s' (index %zu) to %s", 
                        m_currentFrame, cam.name.c_str(), idx, filename.c_str());
        } else {
            LOG_ERROR_FMT("CameraSystem", "Failed to save screenshot for frame %d from camera '%s' (index %zu) to %s", 
                         m_currentFrame, cam.name.c_str(), idx, filename.c_str());
            // Don't mark as captured if save failed, so we can retry
        }
        
        // Only capture one camera per render frame to avoid blocking
        // This ensures we don't take longer than a frame duration
        // The next render frame will continue with the next camera
        break;
    }
    
    if (!foundUncaptured && !allCaptured) {
        // This shouldn't happen, but log it if it does
        LOG_WARNING_FMT("CameraSystem", "Frame %d: No uncaptured camera found but allCaptured is false", m_currentFrame);
    }
    
    // Restore original view and active camera
    // Note: We don't re-render here - the main render loop will handle rendering
    // This prevents double-rendering and UI button click issues
    m_view = originalView;
    m_cameraSystem.SetActiveCameraIndex(originalActiveCamera);
}

bool CameraSystemEditor::SaveScreenshot(const std::string& filepath, int width, int height) {
    if (width <= 0 || height <= 0) {
        LOG_ERROR_FMT("CameraSystem", "Invalid dimensions for screenshot: %dx%d", width, height);
        return false;
    }
    
    std::vector<unsigned char> pixels(width * height * 3);
    
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR_FMT("CameraSystem", "OpenGL error reading pixels: 0x%x", error);
        return false;
    }
    
    // No flip needed - we'll write in reverse order directly
    
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_FMT("CameraSystem", "Failed to open file for writing: %s", filepath.c_str());
        return false;
    }
    
    if (!file.good()) {
        LOG_ERROR_FMT("CameraSystem", "File stream is not good after opening: %s", filepath.c_str());
        return false;
    }
    
    unsigned char header[54] = {0};
    header[0] = 'B';
    header[1] = 'M';
    int fileSize = 54 + width * height * 3;
    header[2] = fileSize & 0xFF;
    header[3] = (fileSize >> 8) & 0xFF;
    header[4] = (fileSize >> 16) & 0xFF;
    header[5] = (fileSize >> 24) & 0xFF;
    header[10] = 54;
    header[14] = 40;
    header[18] = width & 0xFF;
    header[19] = (width >> 8) & 0xFF;
    header[22] = height & 0xFF;
    header[23] = (height >> 8) & 0xFF;
    header[26] = 1;
    header[28] = 24;
    
    file.write(reinterpret_cast<const char*>(header), 54);
    if (!file.good()) {
        LOG_ERROR_FMT("CameraSystem", "Error writing BMP header to file: %s", filepath.c_str());
        file.close();
        return false;
    }
    
    // BMP format expects rows from top to bottom (top row first)
    // Write rows in forward order (try this if reverse order was upside down)
    for (int y = 0; y < height; y++) {
        file.write(reinterpret_cast<const char*>(&pixels[y * width * 3]), width * 3);
        if (!file.good()) {
            LOG_ERROR_FMT("CameraSystem", "Error writing pixel data to file: %s", filepath.c_str());
            file.close();
            return false;
        }
    }
    
    file.flush();
    file.close();
    
    // Small delay to ensure file system has time to write (removed - causes blocking)
    
    // Verify file was created and has content
    std::filesystem::path filePathObj(filepath);
    if (!std::filesystem::exists(filePathObj)) {
        LOG_ERROR_FMT("CameraSystem", "File was not created: %s", filepath.c_str());
        return false;
    }
    
    std::uintmax_t actualFileSize = std::filesystem::file_size(filePathObj);
    if (actualFileSize < 54) { // BMP header is 54 bytes minimum
        LOG_ERROR_FMT("CameraSystem", "File created but too small (%llu bytes): %s", static_cast<unsigned long long>(actualFileSize), filepath.c_str());
        return false;
    }
    
    return true;
}

bool CameraSystemEditor::SaveDepthMap(const std::string& filepath, int width, int height, const Camera& cam) {
    if (width <= 0 || height <= 0) {
        LOG_ERROR_FMT("CameraSystem", "Invalid dimensions for depth map: %dx%d", width, height);
        return false;
    }
    
    // Log if noisy TOF is enabled
    if (m_vipresNoisyTofEnabled && m_vipresNoiseAmount > 0.0f) {
        static bool loggedNoise = false;
        if (!loggedNoise) {
            LOG_INFO_FMT("CameraSystem", "Applying noise to TOF depth data: ±%.3f units", m_vipresNoiseAmount);
            loggedNoise = true;
        }
    }
    
    int fullWidth, fullHeight;
    glfwGetFramebufferSize(m_glfwWindow, &fullWidth, &fullHeight);
    
    if (width != fullWidth || height != fullHeight) {
        LOG_WARNING_FMT("CameraSystem", "Depth map dimensions (%dx%d) don't match framebuffer (%dx%d), using framebuffer size", 
                       width, height, fullWidth, fullHeight);
        width = fullWidth;
        height = fullHeight;
    }
    
    std::vector<float> depthBuffer(width * height);
    glReadPixels(0, 0, width, height, GL_DEPTH_COMPONENT, GL_FLOAT, depthBuffer.data());
    
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        LOG_ERROR_FMT("CameraSystem", "OpenGL error reading depth buffer: 0x%x", error);
        return false;
    }
    
    const float nearPlane = 0.1f;
    const float farPlane = Constants::FAR_PLANE;
    const float farPlaneThreshold = 0.9999f;  // Adjusted to allow depth values close to far plane (was 0.999)
    
    std::vector<unsigned char> depthPixels(width * height * 3);
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float normalizedDepth = depthBuffer[y * width + x];
            
            if (normalizedDepth >= farPlaneThreshold) {
                depthPixels[y * width * 3 + x * 3 + 0] = 255;
                depthPixels[y * width * 3 + x * 3 + 1] = 255;
                depthPixels[y * width * 3 + x * 3 + 2] = 255;
                continue;
            }
            
            // Convert normalized depth to linear depth in eye space
            float z_ndc = 2.0f * normalizedDepth - 1.0f;
            float z_eye = (2.0f * nearPlane * farPlane) / 
                         (farPlane + nearPlane - z_ndc * (farPlane - nearPlane));
            float linearDepth = std::abs(z_eye);
            
            if (m_vipresNoisyTofEnabled && m_vipresNoiseAmount > 0.0f) {
                unsigned int seed = static_cast<unsigned int>((x * 73856093) ^ (y * 19349663) ^ 
                                                               (static_cast<unsigned int>(m_currentFrame) * 83492791) ^
                                                               (static_cast<unsigned int>(linearDepth * 100.0f) * 19349669) ^
                                                               (m_frameProcessingCounter * 19349673));
                std::mt19937 rng(seed);
                std::uniform_real_distribution<float> dist(-m_vipresNoiseAmount, m_vipresNoiseAmount);
                float noise = dist(rng);
                linearDepth += noise;
                if (linearDepth < 0.0f) {
                    linearDepth = 0.0f;
                }
            }
            
            float x_norm = 2.0f * (static_cast<float>(x) / (width - 1.0f)) - 1.0f;
            float y_norm = 2.0f * (static_cast<float>(height - 1 - y) / (height - 1.0f)) - 1.0f;
            
            const float fov_degrees = 45.0f;
            float fov_rad = glm::radians(fov_degrees);
            float tan_half_fov = std::tan(fov_rad / 2.0f);
            float min_dim = static_cast<float>(std::min(width, height));
            float max_dim = static_cast<float>(std::max(width, height));
            float aspect_ratio = max_dim / min_dim;
            float tan_half_fov_large = tan_half_fov * aspect_ratio;
            
            float x_3d, y_3d, z_3d;
            if (width <= height) {
                x_3d = tan_half_fov * x_norm * linearDepth;
                y_3d = -tan_half_fov_large * y_norm * linearDepth;
            } else {
                x_3d = tan_half_fov_large * x_norm * linearDepth;
                y_3d = -tan_half_fov * y_norm * linearDepth;
            }
            z_3d = -linearDepth;
            
            glm::mat4 camView = CalculateCameraView(cam);
            glm::vec3 eyeSpacePos(x_3d, y_3d, z_3d);
            glm::mat4 invView = glm::inverse(camView);
            glm::vec4 worldPos4 = invView * glm::vec4(eyeSpacePos, 1.0f);
            if (std::abs(worldPos4.w) > 0.0001f) {
                worldPos4 /= worldPos4.w;
            }
            
            float distance = linearDepth;
            if (std::isnan(distance) || std::isinf(distance) || distance < 0.0f) {
                distance = 10000.0f;
            }
            const float maxDistance = 10000.0f;
            const int maxEncodedValue = 16777215;
            const float scaleFactor = static_cast<float>(maxEncodedValue) / maxDistance;
            
            float clampedDepth = glm::clamp(distance, 0.0f, maxDistance);
            int encodedValue = glm::clamp(static_cast<int>(std::round(clampedDepth * scaleFactor)), 0, maxEncodedValue);
            
            depthPixels[y * width * 3 + x * 3 + 0] = static_cast<unsigned char>((encodedValue >> 16) & 0xFF);
            depthPixels[y * width * 3 + x * 3 + 1] = static_cast<unsigned char>((encodedValue >> 8) & 0xFF);
            depthPixels[y * width * 3 + x * 3 + 2] = static_cast<unsigned char>(encodedValue & 0xFF);
        }
    }
    
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_FMT("CameraSystem", "Failed to open file for writing: %s", filepath.c_str());
        return false;
    }
    
    unsigned char header[54] = {0};
    header[0] = 'B';
    header[1] = 'M';
    int fileSize = 54 + width * height * 3;
    header[2] = fileSize & 0xFF;
    header[3] = (fileSize >> 8) & 0xFF;
    header[4] = (fileSize >> 16) & 0xFF;
    header[5] = (fileSize >> 24) & 0xFF;
    header[10] = 54;
    header[14] = 40;
    header[18] = width & 0xFF;
    header[19] = (width >> 8) & 0xFF;
    header[22] = height & 0xFF;
    header[23] = (height >> 8) & 0xFF;
    header[26] = 1;
    header[28] = 24;
    
    file.write(reinterpret_cast<const char*>(header), 54);
    if (!file.good()) {
        LOG_ERROR_FMT("CameraSystem", "Error writing BMP header to file: %s", filepath.c_str());
        file.close();
        return false;
    }
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            file.write(reinterpret_cast<const char*>(&depthPixels[y * width * 3 + x * 3]), 3);
        }
        if (!file.good()) {
            LOG_ERROR_FMT("CameraSystem", "Error writing depth pixel data to file: %s", filepath.c_str());
            file.close();
            return false;
        }
    }
    
    file.flush();
    file.close();
    
    return true;
}

glm::vec3 CameraSystemEditor::UnprojectPixel(const glm::vec2& screenPos, float linearDepth, const Camera& cam) const {
    int fullWidth, fullHeight;
    glfwGetFramebufferSize(m_glfwWindow, &fullWidth, &fullHeight);
    return UnprojectPixelWithMatrices(screenPos, linearDepth, CalculateCameraView(cam), m_projection, fullWidth, fullHeight);
}

glm::vec3 CameraSystemEditor::UnprojectPixelWithMatrices(const glm::vec2& screenPos, float linearDepth,
                                                          const glm::mat4& view, const glm::mat4& projection,
                                                          int width, int height) const {
    float x_ndc = (2.0f * screenPos.x) / width - 1.0f;
    float y_ndc = 1.0f - (2.0f * screenPos.y) / height;
    
    glm::vec4 nearClipPos(x_ndc, y_ndc, -1.0f, 1.0f);
    glm::mat4 invProjection = glm::inverse(projection);
    glm::vec4 nearEyePos = invProjection * nearClipPos;
    nearEyePos /= nearEyePos.w;
    
    glm::vec3 rayDir = glm::normalize(glm::vec3(nearEyePos));
    glm::vec3 eyeSpacePos = rayDir * linearDepth;
    
    glm::mat4 invView = glm::inverse(view);
    glm::vec4 worldPos4 = invView * glm::vec4(eyeSpacePos, 1.0f);
    if (std::abs(worldPos4.w) > 0.0001f) {
        worldPos4 /= worldPos4.w;
    }
    
    return glm::vec3(worldPos4);
}

void CameraSystemEditor::UpdatePointCloud() {
    LOG_INFO("CameraSystem", "UpdatePointCloud called");
    
    // Increment frame processing counter to refresh noise at every frame
    m_frameProcessingCounter++;
    
    // Clear storage (this removes all points)
    m_pointCloudGrid.clear();
    m_gridAccumulators.clear();
    // Clear filtered TOF write buffer (we'll write new data to it)
    m_filteredTofData[m_filteredTofWriteBuffer].clear();
    // Clear point cloud write buffer (we'll process into it)
    m_pointCloud[m_pointCloudWriteBuffer].clear();
    
    // Depth data calculation is controlled by TOF, not VIPRES
    if (!m_tofDataEnabled || m_cameras.empty()) {
        return;
    }
    
    int fullWidth, fullHeight;
    glfwGetFramebufferSize(m_glfwWindow, &fullWidth, &fullHeight);
    
    const float nearPlane = 0.1f;
    const float farPlane = Constants::FAR_PLANE;
    const float farPlaneThreshold = 0.9999f;
    
    if (m_vipresCameraEnabled.size() != m_cameras.size()) {
        m_vipresCameraEnabled.resize(m_cameras.size(), true);
    }
    
    m_filteredTofData[m_filteredTofWriteBuffer].clear();
    
    for (size_t camIdx = 0; camIdx < m_cameras.size(); camIdx++) {
        const Camera& cam = m_cameras[camIdx];
        if (cam.canMove || !m_vipresCameraEnabled[camIdx]) {
            continue;
        }
        
        glm::mat4 camView = CalculateCameraView(cam);
            
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);  // Enable depth writes
            glDisable(GL_CULL_FACE);
            glViewport(0, 0, fullWidth, fullHeight);
        
        if (m_world.IsGridVisible()) {
            RenderGrid();
        }
        
        if (m_meshShader && m_meshShader->IsValid()) {
            m_meshShader->UseShader();
            GLint uniformView = m_meshShader->GetViewLocation();
            GLint uniformProjection = m_meshShader->GetProjectionLocation();
            if (uniformView != -1) glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(camView));
            if (uniformProjection != -1) glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(m_projection));
            
            for (size_t i = 0; i < m_objects.size(); i++) {
                if (m_objects[i] && m_objects[i]->visible) {
                    RenderObject(m_objects[i].get());
                }
            }
        }
        
        // Ensure rendering is complete before reading
        glFinish();
        
        // Read depth buffer from back buffer
        std::vector<float> depthBuffer(fullWidth * fullHeight);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, fullWidth, fullHeight, GL_DEPTH_COMPONENT, GL_FLOAT, depthBuffer.data());
        
        std::vector<unsigned char> colorBuffer;
        if (m_vipresComputeColor) {
            colorBuffer.resize(fullWidth * fullHeight * 4);
            glReadPixels(0, 0, fullWidth, fullHeight, GL_RGBA, GL_UNSIGNED_BYTE, colorBuffer.data());
        }
        
        // Precompute values outside pixel loop for performance
        const float twoNearFar = 2.0f * nearPlane * farPlane;
        const float farPlusNear = farPlane + nearPlane;
        const float invWidthMinusOne = 1.0f / (fullWidth - 1.0f);
        const float invHeightMinusOne = 1.0f / (fullHeight - 1.0f);
        const float fov_rad = glm::radians(45.0f);
        const float tan_half_fov = std::tan(fov_rad / 2.0f);
        const float min_dim = static_cast<float>(std::min(fullWidth, fullHeight));
        const float max_dim = static_cast<float>(std::max(fullWidth, fullHeight));
        const float aspect_ratio = max_dim / min_dim;
        const float tan_half_fov_large = tan_half_fov * aspect_ratio;
        const bool widthLeHeight = (fullWidth <= fullHeight);
        const glm::mat4 invView = glm::inverse(camView);
        const glm::vec3 defaultColor = m_tofColorCodeByCamera ? 
            glm::vec3(std::sin(camIdx * 0.618f) * 0.5f + 0.5f,
                     std::sin(camIdx * 0.618f + 2.094f) * 0.5f + 0.5f,
                     std::sin(camIdx * 0.618f + 4.189f) * 0.5f + 0.5f) : glm::vec3(1.0f);
        const float inv255 = 0.003921568627451f;  // 1.0f / 255.0f
        std::mt19937 rng;
        std::uniform_real_distribution<float> noiseDist(-m_vipresNoiseAmount, m_vipresNoiseAmount);
        const bool useNoise = m_vipresNoisyTofEnabled && m_vipresNoiseAmount > 0.0f;
        
        for (int y = 0; y < fullHeight; y += m_vipresSamplingDensity) {
            const int yIndex = y * fullWidth;
            const float yNormBase = 2.0f * (static_cast<float>(fullHeight - 1 - y) * invHeightMinusOne) - 1.0f;
            
            for (int x = 0; x < fullWidth; x += m_vipresSamplingDensity) {
                const float normalizedDepth = depthBuffer[yIndex + x];
                if (normalizedDepth >= farPlaneThreshold) {
                    continue;
                }
                
                float z_ndc = 2.0f * normalizedDepth - 1.0f;
                float z_eye = twoNearFar / (farPlusNear - z_ndc * (farPlane - nearPlane));
                float linearDepth = std::abs(z_eye);
                
                if (useNoise) {
                    rng.seed(static_cast<unsigned int>((x * 73856093) ^ (y * 19349663) ^ 
                                                        (static_cast<unsigned int>(m_currentFrame) * 83492791) ^
                                                        (static_cast<unsigned int>(linearDepth * 100.0f) * 19349669) ^
                                                        (m_frameProcessingCounter * 19349673)));
                    linearDepth += noiseDist(rng);
                    if (linearDepth < 0.0f) linearDepth = 0.0f;
                }
                
                if (linearDepth > m_vipresDepthLimit || std::isnan(linearDepth) || std::isinf(linearDepth)) {
                    continue;
                }
                
                float x_norm = 2.0f * (static_cast<float>(x) * invWidthMinusOne) - 1.0f;
                float y_norm = -yNormBase;
                
                float x_3d, y_3d;
                if (widthLeHeight) {
                    x_3d = tan_half_fov * x_norm * linearDepth;
                    y_3d = tan_half_fov_large * y_norm * linearDepth;
                } else {
                    x_3d = tan_half_fov_large * x_norm * linearDepth;
                    y_3d = tan_half_fov * y_norm * linearDepth;
                }
                
                glm::vec4 worldPos4 = invView * glm::vec4(x_3d, y_3d, -linearDepth, 1.0f);
                if (std::abs(worldPos4.w) > 0.0001f) {
                    worldPos4 /= worldPos4.w;
                }
                
                glm::vec3 color = defaultColor;
                if (m_vipresComputeColor && !colorBuffer.empty()) {
                    const int pixelIndex = (yIndex + x) * 4;
                    if (pixelIndex + 2 < static_cast<int>(colorBuffer.size())) {
                        color = glm::vec3(
                            colorBuffer[pixelIndex] * inv255,
                            colorBuffer[pixelIndex + 1] * inv255,
                            colorBuffer[pixelIndex + 2] * inv255
                        );
                    }
                }
                
                PointCloudPoint point;
                point.position = glm::vec3(worldPos4);
                point.color = color;
                m_filteredTofData[m_filteredTofWriteBuffer].push_back(point);
            }
        }
        
    }
    
    UpdateCamera();
    
    // Swap filtered TOF data buffers using std::swap: write buffer becomes read buffer for processing
    std::swap(m_filteredTofReadBuffer, m_filteredTofWriteBuffer);
    
    // Process filtered TOF data from read buffer into final point cloud
    // Processing order: Get buffer data -> (noise already applied during sampling) -> TSDF (if enabled) -> Gridding -> Low-pass
    // Note: TSDF and Low-pass are mutually exclusive
    std::vector<PointCloudPoint> processedData = m_filteredTofData[m_filteredTofReadBuffer];
    
    // Step 1: Apply TSDF if enabled (before gridding, mutually exclusive with low-pass)
    if (m_tofTsdfEnabled) {
        // Ensure low-pass is disabled (mutually exclusive)
        if (m_tofLowPassEnabled) {
            m_tofLowPassEnabled = false;
            Algorithms::ClearLowPassFilterState(m_lowPassFilterState);
            m_tofProcessing.ClearLowPassFilterState();
        }
        
        // Find camera positions for TSDF integration
        // Use first anchor camera as approximation for all points
        glm::vec3 cameraPos(0.0f, 0.0f, 300.0f);
        for (const auto& cam : m_cameras) {
            if (!cam.canMove) {
                cameraPos = cam.position;
                break;
            }
        }
        
        const float invGridSize = 1.0f / m_vipresGridSize;
        for (const auto& point : processedData) {
            GridKey gridKey;
            gridKey.x = static_cast<int>(std::round(point.position.x * invGridSize));
            gridKey.y = static_cast<int>(std::round(point.position.y * invGridSize));
            gridKey.z = static_cast<int>(std::round(point.position.z * invGridSize));
            IntegrateTSDF(gridKey, point.position, cameraPos, point.color);
        }
        
        // Cleanup old/unobserved TSDF voxels before extraction to prevent unbounded growth
        m_tofProcessing.CleanupTSDF(0.05f, m_tofTsdfWeightDecay);
        
        // Extract TSDF point cloud - this becomes the processed data
        processedData.clear();
        ExtractTSDFPointCloud(processedData);
    }
    
    // Step 2: Apply gridding if enabled (using TOFProcessing)
    std::vector<GridKey> pointGridKeys;  // Track grid keys for smart occupancy
    
    if (m_vipresGriddingEnabled) {
        // ApplyGridding clears and populates the grid, so no need to clear here
        m_tofProcessing.ApplyGridding(processedData, m_pointCloudGrid, m_vipresGridMode, m_vipresGridSize);
        
        // Step 3: Apply smart occupancy weights if enabled (ONLY on gridded data)
        // Smart occupancy algorithms require gridding to be enabled
        if (m_smartOccupancyEnabled && m_vipresGriddingEnabled) {
            ApplySmartOccupancyWeights(m_pointCloudGrid);
        } else {
            // Set default weights to 1.0 if smart occupancy is disabled or gridding is disabled
            for (auto& pair : m_pointCloudGrid) {
                pair.second.weight = 1.0f;
            }
        }
        
        // Step 4: Apply weight-based low-pass filter if enabled (AFTER occupancy filtering)
        if (m_tofLowPassEnabled && m_vipresGriddingEnabled && !m_tofTsdfEnabled) {
            // This ensures filter responds immediately to slider changes
            Algorithms::ComputeLowPassFilterCoefficients(m_tofLowPassCutoff, m_tofSamplingRate, m_lowPassFilterCoefficients);
            
            // Apply weight-based low-pass filter to gridded points
            Algorithms::ApplyWeightBasedLowPassFilter(m_pointCloudGrid, m_lowPassFilterCoefficients, m_lowPassFilterState);
        }
        
        // Convert gridded data to point cloud (after weights and low-pass are applied)
        processedData.clear();
        pointGridKeys.clear();
        processedData.reserve(m_pointCloudGrid.size());
        pointGridKeys.reserve(m_pointCloudGrid.size());
        
        for (const auto& pair : m_pointCloudGrid) {
            const GridKey& gridKey = pair.first;
            PointCloudPoint point = pair.second;
            // Weight is already set by smart occupancy or defaulted to 1.0
            
            pointGridKeys.push_back(gridKey);
            processedData.push_back(point);
        }
    } else {
        // If not gridding, initialize weights to 1.0
        for (auto& point : processedData) {
            point.weight = 1.0f;
        }
    }
    
    // Final step: Copy processed data to point cloud buffer
    m_pointCloud[m_pointCloudWriteBuffer] = processedData;
    
    // Swap point cloud buffers using std::swap: write buffer becomes read buffer for rendering
    // This ensures rendering always uses a complete, consistent frame
    std::swap(m_pointCloudReadBuffer, m_pointCloudWriteBuffer);
    
    if (!m_pointCloudBuffersInitialized) {
        glGenVertexArrays(1, &m_pointCloudVAO);
        glGenBuffers(1, &m_pointCloudVBO);
        m_pointCloudBuffersInitialized = true;
    }
    
    // VBO update with filtering happens in RenderPointCloud() to ensure weight-based filtering is applied
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

// Compute second-order Butterworth low-pass filter coefficients
void CameraSystemEditor::ComputeLowPassFilterCoefficients(float cutoffHz, float sampleRate) {
    // Update new algorithms module coefficients
    Algorithms::ComputeLowPassFilterCoefficients(cutoffHz, sampleRate, m_lowPassFilterCoefficients);
    
    // Also update old TOFProcessing coefficients for backward compatibility
    m_tofProcessing.ComputeLowPassFilterCoefficients(cutoffHz, sampleRate);
}

// Second-order low-pass filter implementation (Butterworth design) for 3D positions
// Applied to gridded points using GridKey
glm::vec3 CameraSystemEditor::ApplyLowPassFilterPosition(const GridKey& key, const glm::vec3& inputPos) {
    return m_tofProcessing.ApplyLowPassFilterPosition(key, inputPos);
}

// TSDF Integration: Add a point measurement to the TSDF grid
void CameraSystemEditor::IntegrateTSDF(const GridKey& key, const glm::vec3& pointPos, const glm::vec3& cameraPos, const glm::vec3& color) {
    m_tofProcessing.IntegrateTSDF(key, pointPos, cameraPos, m_vipresGridSize, m_tofTsdfTruncationDistance, m_tofTsdfWeightDecay, color, m_tofTsdfStoreColor);
}

// Extract point cloud from TSDF by finding zero-crossings (surface locations)
void CameraSystemEditor::ExtractTSDFPointCloud(std::vector<PointCloudPoint>& output) {
    // Use stored colors if TSDF color storage is enabled and Compute Color is enabled
    bool useStoredColors = m_tofTsdfStoreColor && m_vipresComputeColor;
    m_tofProcessing.ExtractTSDFPointCloud(output, m_vipresGridSize, useStoredColors);
}

// Clear TSDF data
void CameraSystemEditor::ClearTSDF() {
    m_tofProcessing.ClearTSDF();
}

// Cleanup TSDF by removing low-weight voxels
void CameraSystemEditor::CleanupTSDF(float minWeightThreshold, float weightDecay) {
    m_tofProcessing.CleanupTSDF(minWeightThreshold, weightDecay);
}

// Second-order low-pass filter implementation for non-gridded points using position hash
glm::vec3 CameraSystemEditor::ApplyLowPassFilterPosition(size_t positionHash, const glm::vec3& inputPos) {
    return m_tofProcessing.ApplyLowPassFilterPosition(positionHash, inputPos);
}

// Smart Occupancy: Apply weight algorithms directly to grid map
// NOTE: All occupancy algorithms ONLY operate on gridded data - gridding must be enabled
void CameraSystemEditor::ApplySmartOccupancyWeights(std::map<GridKey, PointCloudPoint>& pointCloudGrid) {
    // Require gridding to be enabled - occupancy algorithms only work on gridded data
    if (!m_vipresGriddingEnabled) {
        return;  // Do not run occupancy algorithms if gridding is disabled
    }
    
    if (pointCloudGrid.empty()) {
        return;  // No data to process
    }
    
    // Build parameters from member variables
    GridBased::OccupancyParams params;
    params.neighborConsistencyEnabled = m_smartOccupancyNeighborConsistency;
    params.neighborRadius = m_smartOccupancyNeighborRadius;
    params.residualThreshold = m_smartOccupancyResidualThreshold;
    params.persistenceEnabled = m_smartOccupancyPersistence;
    params.persistenceGamma = m_smartOccupancyPersistenceGamma;
    params.varianceEnabled = m_smartOccupancyVariance;
    params.varianceThreshold = m_smartOccupancyVarianceThreshold;
    params.evictionEnabled = m_smartOccupancyEviction;
    params.evictionUseStrategy2 = true;  // Always use Strategy 2
    params.evictionDecay = m_smartOccupancyEvictionDecay;
    params.evictionIncrement = m_smartOccupancyEvictionIncrement;
    params.evictionMinStartValue = m_smartOccupancyEvictionMinStartValue;
    params.evictionRemovalThreshold = 5;  // Hardcoded removal threshold
    params.eviction2Enabled = m_smartOccupancyEviction2;
    params.eviction2Radius = m_smartOccupancyEviction2Radius;
    params.eviction2MinNeighbors = m_smartOccupancyEviction2MinNeighbors;
    
    // Call the occupancy algorithms module
    GridBased::ApplyOccupancyAlgorithms(pointCloudGrid, params, m_occupancyState);
}

std::vector<float> CameraSystemEditor::GetPointCloudWeights() const {
    std::vector<float> weights;
    
    // Prefer gridded data if available (more accurate since occupancy algorithms operate on grid)
    if (!m_pointCloudGrid.empty()) {
        weights.reserve(m_pointCloudGrid.size());
        for (const auto& pair : m_pointCloudGrid) {
            weights.push_back(pair.second.weight);
        }
    } else if (!m_pointCloud[m_pointCloudReadBuffer].empty()) {
        // Fallback to point cloud buffer
        weights.reserve(m_pointCloud[m_pointCloudReadBuffer].size());
        for (const auto& point : m_pointCloud[m_pointCloudReadBuffer]) {
            weights.push_back(point.weight);
        }
    }
    
    return weights;
}

void CameraSystemEditor::RenderPointCloud() {
    // Point cloud rendering is controlled by TOF (which generates the depth data)
    // Use the read buffer (stable buffer) for rendering - matches old implementation
    static int renderLogCount = 0;
    if (renderLogCount++ < 10) {
        LOG_INFO_FMT("CameraSystem", "RenderPointCloud called: TOF=%d, empty=%d, buffersInit=%d, pointCount=%zu", 
                    m_tofDataEnabled, m_pointCloud[m_pointCloudReadBuffer].empty(), 
                    m_pointCloudBuffersInitialized, m_pointCloud[m_pointCloudReadBuffer].size());
    }
    
    if (!m_tofDataEnabled || m_pointCloud[m_pointCloudReadBuffer].empty() || !m_pointCloudBuffersInitialized) {
        return;
    }
    
    Platform::Render::Shader* gridShader = m_world.GetGridShader();
    if (!gridShader || !gridShader->IsValid()) {
        return;
    }
    
    gridShader->UseShader();
    
    GLuint shaderID = gridShader->GetShaderID();
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint colorLoc = glGetUniformLocation(shaderID, "color");
    
    if (projLoc == -1 || viewLoc == -1 || modelLoc == -1 || colorLoc == -1) {
        return;
    }
    
    glm::mat4 model = glm::mat4(1.0f);
    if (projLoc != -1) glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projection));
    if (viewLoc != -1) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_view));
    if (modelLoc != -1) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    
    glEnable(GL_DEPTH_TEST);
    glPointSize(2.0f);  // Match old implementation point size
    
    // Filter points by weight threshold before rendering
    std::vector<PointCloudPoint> filteredPoints;
    filteredPoints.reserve(m_pointCloud[m_pointCloudReadBuffer].size());
    for (const auto& point : m_pointCloud[m_pointCloudReadBuffer]) {
        if (point.weight >= m_weightThreshold) {
            filteredPoints.push_back(point);
        }
    }
    
    size_t filteredCount = filteredPoints.size();
    if (filteredCount == 0) {
        glBindVertexArray(0);
        glPointSize(1.0f);
        return;
    }
    
    // Update VBO with filtered points
    glBindVertexArray(m_pointCloudVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_pointCloudVBO);
    glBufferData(GL_ARRAY_BUFFER, filteredCount * sizeof(Engine::Editors::CameraSystem::PointCloudPoint), 
                filteredPoints.data(), GL_DYNAMIC_DRAW);
    
    // Re-setup vertex attributes after buffer update
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Engine::Editors::CameraSystem::PointCloudPoint), 
                         reinterpret_cast<void*>(offsetof(Engine::Editors::CameraSystem::PointCloudPoint, position)));
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Engine::Editors::CameraSystem::PointCloudPoint), 
                         reinterpret_cast<void*>(offsetof(Engine::Editors::CameraSystem::PointCloudPoint, color)));
    
    if (colorLoc != -1) {
        glUniform3f(colorLoc, 1.0f, 1.0f, 1.0f);
    }
    
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(filteredCount));
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glPointSize(1.0f);
}

void CameraSystemEditor::GenerateRemesh() {
    m_remeshVertices.clear();
    m_remeshIndices.clear();
    m_remeshNeedsUpdate = false;
    
    // Use the read buffer (stable buffer) for remeshing - ensures consistent data
    if (m_pointCloud[m_pointCloudReadBuffer].empty()) {
        return;
    }
    
    // Use the new Remeshing class for improved mesh generation
    Engine::Editors::CameraSystem::Remeshing remeshing;
    
    Engine::Editors::CameraSystem::Algorithm algorithm = Engine::Editors::CameraSystem::Algorithm::GriddedQuads;
    if (m_vipresRemeshAlgorithm == 1) {
        algorithm = Engine::Editors::CameraSystem::Algorithm::NearestNeighbor;
    } else if (m_vipresRemeshAlgorithm == 2) {
        algorithm = Engine::Editors::CameraSystem::Algorithm::Delaunay2D;
    } else if (m_vipresRemeshAlgorithm == 3) {
        algorithm = Engine::Editors::CameraSystem::Algorithm::PoissonLike;
    } else if (m_vipresRemeshAlgorithm == 4) {
        algorithm = Engine::Editors::CameraSystem::Algorithm::FCTM;
    } else if (m_vipresRemeshAlgorithm == 5) {
        algorithm = Engine::Editors::CameraSystem::Algorithm::GeodesicTree;
    } else if (m_vipresRemeshAlgorithm == 6) {
        algorithm = Engine::Editors::CameraSystem::Algorithm::GTree;
    }
    
    // Set up algorithm parameters
    Engine::Editors::CameraSystem::AlgorithmParameters params;
    params.maxDistance = m_vipresRemeshMaxDistance;
    params.maxNeighbors = m_vipresRemeshMaxNeighbors;
    params.projectionPlane = m_vipresRemeshProjectionPlane;
    params.delaunayThreshold = m_vipresRemeshDelaunayThreshold;
    params.poissonRadius = m_vipresRemeshPoissonRadius;
    params.poissonDepth = m_vipresRemeshPoissonDepth;
    params.poissonDensity = m_vipresRemeshPoissonDensity;
    params.poissonSmoothNormals = m_vipresRemeshPoissonSmoothNormals;
    params.fctmSliceThickness = m_vipresRemeshFCTMSliceThickness;
    params.geodesicKNeighbors = m_vipresRemeshGeodesicKNeighbors;
    params.geodesicNumSeeds = m_vipresRemeshGeodesicNumSeeds;
    params.geodesicInteriorBias = m_vipresRemeshGeodesicInteriorBias;
    params.useWeights = m_smartOccupancyEnabled && m_vipresGriddingEnabled;  // Use weights if smart occupancy is enabled
    
    // Convert CameraSystemEditor types to Remeshing types
    // Use type aliases to avoid name collision with CameraSystemEditor's nested types
    using RemeshPointCloudPoint = ::Engine::Editors::CameraSystem::PointCloudPoint;
    using RemeshGridKey = ::Engine::Editors::CameraSystem::GridKey;
    
    std::vector<RemeshPointCloudPoint> remeshPointCloud;
    remeshPointCloud.reserve(m_pointCloud[m_pointCloudReadBuffer].size());
    for (const auto& point : m_pointCloud[m_pointCloudReadBuffer]) {
        RemeshPointCloudPoint converted;
        converted.position = point.position;
        converted.color = point.color;
        converted.weight = point.weight;
        remeshPointCloud.push_back(converted);
    }
    
    std::map<RemeshGridKey, RemeshPointCloudPoint> remeshPointCloudGrid;
    for (const auto& pair : m_pointCloudGrid) {
        RemeshGridKey key;
        key.x = pair.first.x;
        key.y = pair.first.y;
        key.z = pair.first.z;
        
        RemeshPointCloudPoint point;
        point.position = pair.second.position;
        point.color = pair.second.color;
        point.weight = pair.second.weight;
        remeshPointCloudGrid[key] = point;
    }
    
    Engine::Editors::CameraSystem::RemeshResult result = remeshing.GenerateMesh(
        remeshPointCloud,
        remeshPointCloudGrid,
        m_vipresGriddingEnabled,
        m_vipresGridSize,
        algorithm,
        params
    );
    
    // Display initial cluster count for FCTM algorithm
    if (algorithm == Engine::Editors::CameraSystem::Algorithm::FCTM && result.initialClusterCount > 0) {
        LOG_INFO_FMT("CameraSystem", "FCTM: Computed %zu initial clusters from point cloud", result.initialClusterCount);
    }
    
    // Convert Remeshing::MeshVertex to CameraSystemEditor::MeshVertex
    m_remeshVertices.clear();
    m_remeshVertices.reserve(result.vertices.size());
    for (const auto& vertex : result.vertices) {
        MeshVertex converted;
        converted.position = vertex.position;
        converted.normal = vertex.normal;
        converted.color = vertex.color;
        m_remeshVertices.push_back(converted);
    }
    m_remeshIndices = result.indices;
    
    if (!m_remeshBuffersInitialized) {
        glGenVertexArrays(1, &m_remeshVAO);
        glGenBuffers(1, &m_remeshVBO);
        glGenBuffers(1, &m_remeshEBO);
        m_remeshBuffersInitialized = true;
    }
    
    if (m_remeshVertices.empty()) {
        return;
    }
    
    glBindVertexArray(m_remeshVAO);
    
    // Update vertex buffer
    glBindBuffer(GL_ARRAY_BUFFER, m_remeshVBO);
    glBufferData(GL_ARRAY_BUFFER, m_remeshVertices.size() * sizeof(MeshVertex), 
                m_remeshVertices.data(), GL_DYNAMIC_DRAW);
    
    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), 
                         reinterpret_cast<void*>(offsetof(MeshVertex, position)));
    
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), 
                         reinterpret_cast<void*>(offsetof(MeshVertex, normal)));
    
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), 
                         reinterpret_cast<void*>(offsetof(MeshVertex, color)));
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_remeshEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_remeshIndices.size() * sizeof(unsigned int), 
                m_remeshIndices.data(), GL_DYNAMIC_DRAW);
    
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void CameraSystemEditor::RenderRemesh() {
    if (!m_vipresRemeshEnabled || m_remeshVertices.empty() || !m_remeshBuffersInitialized) {
        return;
    }
    
    if (!m_meshShader || !m_meshShader->IsValid()) {
        return;
    }
    
    m_meshShader->UseShader();
    
    GLuint shaderID = m_meshShader->GetShaderID();
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    
    if (projLoc == -1 || viewLoc == -1 || modelLoc == -1) {
        return;
    }
    
    glm::mat4 model = glm::mat4(1.0f);
    if (projLoc != -1) glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(m_projection));
    if (viewLoc != -1) glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_view));
    if (modelLoc != -1) glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
    
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(m_remeshVAO);
    
    // Check rendering mode based on algorithm
    if (m_vipresRemeshAlgorithm == 4 && m_remeshIndices.empty()) {
        // FCTM: Render as larger red points
        glDisable(GL_CULL_FACE);
        glPointSize(8.0f);  // Larger point size for FCTM cluster centers
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_remeshVertices.size()));
        glPointSize(1.0f);
    } else if (m_vipresRemeshAlgorithm == 5 || m_vipresRemeshAlgorithm == 6) {
        // GeodesicTree/GTree: Render as large spheres (points) and connecting lines
        glDisable(GL_CULL_FACE);
        
        // First render lines connecting consecutive points
        if (!m_remeshIndices.empty()) {
            glLineWidth(2.0f);
            glDrawElements(GL_LINES, static_cast<GLsizei>(m_remeshIndices.size()), GL_UNSIGNED_INT, 0);
            glLineWidth(1.0f);
        }
        
        // Then render points as large spheres
        glPointSize(12.0f);  // Large sphere size
        glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(m_remeshVertices.size()));
        glPointSize(1.0f);
    } else {
        // Other algorithms: Render as triangles
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(m_remeshIndices.size()), GL_UNSIGNED_INT, 0);
        glDisable(GL_CULL_FACE);
    }
    
    glBindVertexArray(0);
}

void CameraSystemEditor::RenderGrid() {
    m_world.RenderGrid(m_projection, m_view);
}

void CameraSystemEditor::RenderObject(SceneObject* obj) {
    if (m_objectRenderer) {
        m_objectRenderer->RenderObject(obj, m_meshShader);
    }
}

void CameraSystemEditor::RenderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    
    if (m_world.IsGridVisible()) {
        RenderGrid();
    }
    
    // Render point cloud if TOF is enabled (unless remesh is enabled, in which case render remesh instead)
    if (m_tofDataEnabled) {
        if (m_vipresEnabled && m_vipresRemeshEnabled) {
            // If remesh is enabled, render the remeshed mesh instead of point cloud
            RenderRemesh();
        } else {
            // Otherwise, render the point cloud
            // Debug: log point cloud status
            static int debugCount = 0;
            if (debugCount++ < 10) {
                LOG_INFO_FMT("CameraSystem", "Rendering TOF point cloud: enabled=%d, bufferInit=%d, pointCount=%zu", 
                           m_tofDataEnabled, m_pointCloudBuffersInitialized, m_pointCloud[m_pointCloudReadBuffer].size());
            }
            RenderPointCloud();
        }
    }
    
    if (!m_meshShader || !m_meshShader->IsValid()) {
        return;
    }
    
    m_meshShader->UseShader();
    if (!m_meshShader->IsValid()) {
        return;
    }
    
    GLint uniformView = m_meshShader->GetViewLocation();
    GLint uniformProjection = m_meshShader->GetProjectionLocation();
    GLint uniformEyePosition = m_meshShader->GetEyePositionLocation();
    
    if (uniformView == -1 || uniformProjection == -1) {
        return;
    }
    
    glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(m_view));
    glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(m_projection));
    
    glm::vec3 eyePos = m_cameras[m_cameraSystem.GetActiveCameraIndex()].position;
    if (uniformEyePosition != -1) {
        glUniform3f(uniformEyePosition, eyePos.x, eyePos.y, eyePos.z);
    }
    
    GLuint shaderProgramID = m_meshShader->GetShaderID();
    
    int directionalLightCount = 0;
    int pointLightCount = 0;
    int spotLightCount = 0;
    
    for (const auto& light : m_lights) {
        if (light.type == LightType::Directional) directionalLightCount++;
        else if (light.type == LightType::Point) pointLightCount++;
        else if (light.type == LightType::Spot) spotLightCount++;
    }
    
    GLint uniformPointLightCount = glGetUniformLocation(shaderProgramID, "pointLightCount");
    GLint uniformSpotLightCount = glGetUniformLocation(shaderProgramID, "spotLightCount");
    if (uniformPointLightCount != -1) glUniform1i(uniformPointLightCount, std::min(pointLightCount, 3));
    if (uniformSpotLightCount != -1) glUniform1i(uniformSpotLightCount, std::min(spotLightCount, 3));
    
    LightConfig defaultDirLight;
    defaultDirLight.type = LightType::Directional;
    defaultDirLight.colour = glm::vec3(1.0f, 1.0f, 1.0f);
    defaultDirLight.ambientIntensity = 0.3f;
    defaultDirLight.diffuseIntensity = 0.7f;
    defaultDirLight.directionOrPosition = glm::vec3(0.0f, 0.0f, -1.0f);
    
    const LightConfig* dirLight = &defaultDirLight;
    for (const auto& light : m_lights) {
        if (light.type == LightType::Directional) {
            dirLight = &light;
            break;
        }
    }
    
    GLint uniformDirectionalLightColour = glGetUniformLocation(shaderProgramID, "directionalLight.base.colour");
    GLint uniformDirectionalLightAmbient = glGetUniformLocation(shaderProgramID, "directionalLight.base.ambientIntensity");
    GLint uniformDirectionalLightDiffuse = glGetUniformLocation(shaderProgramID, "directionalLight.base.diffuseIntensity");
    GLint uniformDirectionalLightDirection = glGetUniformLocation(shaderProgramID, "directionalLight.direction");
    
    if (uniformDirectionalLightColour != -1) glUniform3f(uniformDirectionalLightColour, dirLight->colour.x, dirLight->colour.y, dirLight->colour.z);
    if (uniformDirectionalLightAmbient != -1) glUniform1f(uniformDirectionalLightAmbient, dirLight->ambientIntensity);
    if (uniformDirectionalLightDiffuse != -1) glUniform1f(uniformDirectionalLightDiffuse, dirLight->diffuseIntensity);
    if (uniformDirectionalLightDirection != -1) glUniform3f(uniformDirectionalLightDirection, dirLight->directionOrPosition.x, dirLight->directionOrPosition.y, dirLight->directionOrPosition.z);
    
    int pointLightIndex = 0;
    for (const auto& light : m_lights) {
        if (light.type == LightType::Point && pointLightIndex < 3) {
            std::string baseName = "pointLights[" + std::to_string(pointLightIndex) + "]";
            GLint uniformColour = glGetUniformLocation(shaderProgramID, (baseName + ".base.colour").c_str());
            GLint uniformAmbient = glGetUniformLocation(shaderProgramID, (baseName + ".base.ambientIntensity").c_str());
            GLint uniformDiffuse = glGetUniformLocation(shaderProgramID, (baseName + ".base.diffuseIntensity").c_str());
            GLint uniformPos = glGetUniformLocation(shaderProgramID, (baseName + ".position").c_str());
            GLint uniformConstant = glGetUniformLocation(shaderProgramID, (baseName + ".constant").c_str());
            GLint uniformLinear = glGetUniformLocation(shaderProgramID, (baseName + ".linear").c_str());
            GLint uniformExponent = glGetUniformLocation(shaderProgramID, (baseName + ".exponent").c_str());
                    
            if (uniformColour != -1) glUniform3f(uniformColour, light.colour.x, light.colour.y, light.colour.z);
            if (uniformAmbient != -1) glUniform1f(uniformAmbient, light.ambientIntensity);
            if (uniformDiffuse != -1) glUniform1f(uniformDiffuse, light.diffuseIntensity);
            if (uniformPos != -1) glUniform3f(uniformPos, light.directionOrPosition.x, light.directionOrPosition.y, light.directionOrPosition.z);
            if (uniformConstant != -1) glUniform1f(uniformConstant, light.constant);
            if (uniformLinear != -1) glUniform1f(uniformLinear, light.linear);
            if (uniformExponent != -1) glUniform1f(uniformExponent, light.exponent);
            
            pointLightIndex++;
        }
    }
    
    int spotLightIndex = 0;
    for (const auto& light : m_lights) {
        if (light.type == LightType::Spot && spotLightIndex < 3) {
            std::string baseName = "spotLights[" + std::to_string(spotLightIndex) + "]";
            GLint uniformColour = glGetUniformLocation(shaderProgramID, (baseName + ".base.base.colour").c_str());
            GLint uniformAmbient = glGetUniformLocation(shaderProgramID, (baseName + ".base.base.ambientIntensity").c_str());
            GLint uniformDiffuse = glGetUniformLocation(shaderProgramID, (baseName + ".base.base.diffuseIntensity").c_str());
            GLint uniformPos = glGetUniformLocation(shaderProgramID, (baseName + ".base.position").c_str());
            GLint uniformConstant = glGetUniformLocation(shaderProgramID, (baseName + ".base.constant").c_str());
            GLint uniformLinear = glGetUniformLocation(shaderProgramID, (baseName + ".base.linear").c_str());
            GLint uniformExponent = glGetUniformLocation(shaderProgramID, (baseName + ".base.exponent").c_str());
            GLint uniformDir = glGetUniformLocation(shaderProgramID, (baseName + ".direction").c_str());
            GLint uniformEdge = glGetUniformLocation(shaderProgramID, (baseName + ".edge").c_str());
            
            if (uniformColour != -1) glUniform3f(uniformColour, light.colour.x, light.colour.y, light.colour.z);
            if (uniformAmbient != -1) glUniform1f(uniformAmbient, light.ambientIntensity);
            if (uniformDiffuse != -1) glUniform1f(uniformDiffuse, light.diffuseIntensity);
            if (uniformPos != -1) glUniform3f(uniformPos, light.directionOrPosition.x, light.directionOrPosition.y, light.directionOrPosition.z);
            if (uniformConstant != -1) glUniform1f(uniformConstant, light.constant);
            if (uniformLinear != -1) glUniform1f(uniformLinear, light.linear);
            if (uniformExponent != -1) glUniform1f(uniformExponent, light.exponent);
            if (uniformDir != -1) glUniform3f(uniformDir, 0.0f, 0.0f, -1.0f);
            if (uniformEdge != -1) glUniform1f(uniformEdge, light.edge);
            
            spotLightIndex++;
        }
    }
    
    m_meshShader->SetTexture(1);
    glm::mat4 lightTransform = glm::mat4(1.0f);
    m_meshShader->SetDirectionalLightTransform(&lightTransform);
    
    static GLuint defaultShadowTexture = 0;
    if (defaultShadowTexture == 0) {
        glGenTextures(1, &defaultShadowTexture);
        glBindTexture(GL_TEXTURE_2D, defaultShadowTexture);
        unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, defaultShadowTexture);
    GLint uniformShadowMap = glGetUniformLocation(shaderProgramID, "directionalShadowMap");
    if (uniformShadowMap != -1) {
        glUniform1i(uniformShadowMap, 0);
    }
    
    static GLuint defaultWhiteTexture = 0;
    if (defaultWhiteTexture == 0) {
        glGenTextures(1, &defaultWhiteTexture);
        glBindTexture(GL_TEXTURE_2D, defaultWhiteTexture);
        unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glBindTexture(GL_TEXTURE_2D, 0);
    }
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, defaultWhiteTexture);
    
    // Render objects - check if we should render them based on camera type
    bool shouldRenderObjects = true;
    size_t activeIdx = m_cameraSystem.GetActiveCameraIndex();
    if (activeIdx < m_cameras.size()) {
        const Camera& activeCam = m_cameras[activeIdx];
        // If free camera is active, check the toggle
        if (activeCam.canMove) {
            shouldRenderObjects = m_vipresFreeCameraObjectEnabled;
        }
        // For other cameras (anchor/view), always render objects
    }
    
    if (shouldRenderObjects) {
        for (auto& obj : m_objects) {
            RenderObject(obj.get());
        }
    }
}

void CameraSystemEditor::RenderUI() {
    // Delegate to UI class
    m_ui.Render(*this);
}

// Run() helper function implementations
void CameraSystemEditor::UpdateFPSAndSafeParameters(float deltaTime) {
    m_fpsUpdateTime += deltaTime;
    m_fpsFrameCount++;
    m_framesSinceStartup++;
    
    if (m_fpsUpdateTime >= 0.5f) {
        m_currentFPS = static_cast<float>(m_fpsFrameCount) / m_fpsUpdateTime;
        m_fpsUpdateTime = 0.0f;
        m_fpsFrameCount = 0;
        
        if (m_framesSinceStartup >= 60 && !m_hasSafeParameters && m_currentFPS >= 1.0f) {
            SaveSafeParameters();
            LOG_INFO("CameraSystem", "Initial safe parameters saved");
        }
        
        if (m_justRestoredParameters && m_currentFPS >= 1.0f) {
            m_justRestoredParameters = false;
        }
        
        CheckAndRestoreIfNeeded();
    }
}

bool CameraSystemEditor::CanAdvanceFrameWithCapture() const {
    if (!m_captureEnabled || !m_configLoaded) {
        return true;
    }
    
    bool allCapturedForCurrentFrame = true;
    
    if (m_currentFrame == m_lastCapturedFrame && !m_capturedCamerasForFrame.empty()) {
        for (size_t i = 0; i < m_cameras.size(); i++) {
            if (i >= m_capturedCamerasForFrame.size() || !m_capturedCamerasForFrame[i]) {
                allCapturedForCurrentFrame = false;
                break;
            }
        }
    } else if (m_currentFrame != m_lastCapturedFrame && m_lastCapturedFrame >= 0) {
        allCapturedForCurrentFrame = false;
    }
    
    return allCapturedForCurrentFrame;
}

void CameraSystemEditor::AdvanceFrame(float deltaTime, float maxFrameTime) {
    static float frameAccumulator = 0.0f;
    
    if (!m_isPlaying || !m_configLoaded || m_totalFrames <= 0) {
        if (!m_isPlaying) {
            frameAccumulator = 0.0f;
        }
        return;
    }
    
    if (m_captureEnabled) {
        if (CanAdvanceFrameWithCapture()) {
            m_currentFrame++;
            if (m_currentFrame >= m_totalFrames) {
                if (m_loopEnabled) {
                    m_currentFrame = 0;
                } else {
                    m_currentFrame = m_totalFrames - 1;
                    m_isPlaying = false;
                }
            }
            frameAccumulator = 0.0f;
        }
    } else {
        frameAccumulator += deltaTime;
        const float frameInterval = maxFrameTime;
        while (frameAccumulator >= frameInterval) {
            frameAccumulator -= frameInterval;
            m_currentFrame++;
            if (m_currentFrame >= m_totalFrames) {
                if (m_loopEnabled) {
                    m_currentFrame = 0;
                } else {
                    m_currentFrame = m_totalFrames - 1;
                    m_isPlaying = false;
                }
            }
        }
    }
}

void CameraSystemEditor::ProcessPendingCameraSwitch() {
    if (m_pendingCameraSwitch != SIZE_MAX) {
        size_t switchTo = m_pendingCameraSwitch;
        m_pendingCameraSwitch = SIZE_MAX;
        if (switchTo < m_cameras.size()) {
            SwitchCamera(switchTo);
        }
    }
}

void CameraSystemEditor::HandleInputSafely() {
    try {
        HandleMouseInput();
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("CameraSystem", "Exception in HandleMouseInput: %s", e.what());
    } catch (...) {
        LOG_ERROR("CameraSystem", "Unknown exception in HandleMouseInput");
    }
    
    try {
        HandleKeyboardInput();
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("CameraSystem", "Exception in HandleKeyboardInput: %s", e.what());
    } catch (...) {
        LOG_ERROR("CameraSystem", "Unknown exception in HandleKeyboardInput");
    }
}

void CameraSystemEditor::UpdateSceneSafely(float updateInterval) {
    if (m_configLoaded && m_totalFrames > 0) {
        UpdateAnimatedCameras();
        UpdateCamera();
    }
    
    ProcessPendingCameraSwitch();
    HandleInputSafely();
    
    try {
        UpdateFreeCamera(updateInterval);
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("CameraSystem", "Exception in UpdateFreeCamera: %s", e.what());
    } catch (...) {
        LOG_ERROR("CameraSystem", "Unknown exception in UpdateFreeCamera");
    }
    
    if (m_configLoaded) {
        if (m_isPlaying) {
            m_sceneTime += updateInterval;
            if (m_sceneTime > m_totalDuration) {
                m_sceneTime = 0.0f;
            }
        }
        UpdatePathFollowing(updateInterval);
    }
    
    UpdateAnimations(updateInterval);
    UpdateCamera();
}

void CameraSystemEditor::RenderFrameSafely() {
    if (m_nuklearUI.IsInitialized()) {
        try {
            m_nuklearUI.BeginFrame();
        } catch (const std::exception& e) {
            LOG_ERROR_FMT("CameraSystem", "Exception in Nuklear BeginFrame: %s", e.what());
        } catch (...) {
            LOG_ERROR("CameraSystem", "Unknown exception in Nuklear BeginFrame");
        }
    }
    
    if (m_tofDataEnabled) {
        try {
            UpdatePointCloud();
        } catch (const std::exception& e) {
            LOG_ERROR_FMT("CameraSystem", "Exception in UpdatePointCloud: %s", e.what());
        } catch (...) {
            LOG_ERROR("CameraSystem", "Unknown exception in UpdatePointCloud");
        }
    }
    
    if (m_vipresEnabled && m_vipresRemeshEnabled && m_vipresAutoRecomputeMesh) {
        try {
            auto startTime = std::chrono::high_resolution_clock::now();
            GenerateRemesh();
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            float meshTimeSeconds = duration.count() / 1000000.0f;
            
            const float MAX_MESH_TIME = 1.0f;
            if (meshTimeSeconds > MAX_MESH_TIME) {
                m_vipresAutoRecomputeMesh = false;
                LOG_WARNING_FMT("CameraSystem", "Mesh generation took %.3f seconds (exceeds limit of %.1f second), auto-recompute disabled", 
                               meshTimeSeconds, MAX_MESH_TIME);
            }
        } catch (const std::exception& e) {
            LOG_ERROR_FMT("CameraSystem", "Exception in GenerateRemesh: %s", e.what());
            m_vipresAutoRecomputeMesh = false;
        } catch (...) {
            LOG_ERROR("CameraSystem", "Unknown exception in GenerateRemesh");
            m_vipresAutoRecomputeMesh = false;
        }
    }
    
    try {
        RenderScene();
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("CameraSystem", "Exception in RenderScene: %s", e.what());
    } catch (...) {
        LOG_ERROR("CameraSystem", "Unknown exception in RenderScene");
    }
    
    if (m_nuklearUI.IsInitialized()) {
        RenderUI();
    }
    
    if (m_captureEnabled && m_configLoaded) {
        CaptureScreenshots();
    } else if (m_captureEnabled) {
        static int warnCount = 0;
        if (warnCount++ < 3) {
            LOG_WARNING_FMT("CameraSystem", "Capture enabled but config not loaded (frame %d)", m_currentFrame);
        }
    }
}

void CameraSystemEditor::SleepForFrameTiming(float maxFrameTime, const std::chrono::high_resolution_clock::time_point& lastFrameTime) {
    if (!m_captureEnabled) {
        auto frameEndTime = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration<float>(frameEndTime - lastFrameTime).count();
        float sleepTime = maxFrameTime - elapsed;
        if (sleepTime > 0.0f && sleepTime < maxFrameTime * 2.0f) {
            std::this_thread::sleep_for(std::chrono::milliseconds(
                static_cast<int>(sleepTime * 1000.0f)));
        }
    }
}

void CameraSystemEditor::Run() {
    if (!m_window) {
        LOG_ERROR("CameraSystem", "Window is null, cannot run editor");
        return;
    }
    
    const float maxFrameTime = 1.0f / 60.0f; // Maximum 60 FPS
    auto lastFrameTime = std::chrono::high_resolution_clock::now();
    float accumulator = 0.0f;
    
    while (!m_window->getShouldClose()) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(currentTime - lastFrameTime).count();
        lastFrameTime = currentTime;
        
        float cappedDeltaTime = std::min(deltaTime, 0.1f);
        
        // Update FPS and safe parameters
        UpdateFPSAndSafeParameters(deltaTime);
        
        // Advance frame (capture-aware)
        AdvanceFrame(deltaTime, maxFrameTime);
        
        accumulator += deltaTime;
        
        // Process window events
        try {
            m_window->pollEvents();
        } catch (const std::exception& e) {
            LOG_ERROR_FMT("CameraSystem", "Exception in pollEvents: %s", e.what());
            break;
        } catch (...) {
            LOG_ERROR("CameraSystem", "Unknown exception in pollEvents");
            break;
        }
        
        // Fixed timestep update loop
        const float updateInterval = maxFrameTime;
        if (accumulator > updateInterval * 10.0f) {
            accumulator = updateInterval; // Prevent unbounded growth
        }
        
        if (accumulator >= updateInterval) {
            int maxUpdates = 5;
            int updateCount = 0;
            while (accumulator >= updateInterval && updateCount < maxUpdates) {
                accumulator -= updateInterval;
                updateCount++;
                UpdateSceneSafely(updateInterval);
            }
        } else {
            // Handle input even when not updating (for responsiveness)
            ProcessPendingCameraSwitch();
            HandleInputSafely();
            
            try {
                UpdateFreeCamera(cappedDeltaTime);
            } catch (const std::exception& e) {
                LOG_ERROR_FMT("CameraSystem", "Exception in UpdateFreeCamera: %s", e.what());
            } catch (...) {
                LOG_ERROR("CameraSystem", "Unknown exception in UpdateFreeCamera");
            }
        }
        
        // Render frame
        RenderFrameSafely();
        
        // Swap buffers
        try {
            m_window->swapBuffers();
        } catch (const std::exception& e) {
            LOG_ERROR_FMT("CameraSystem", "Exception in swapBuffers: %s", e.what());
            break;
        } catch (...) {
            LOG_ERROR("CameraSystem", "Unknown exception in swapBuffers");
            break;
        }
        
        if (m_window->getShouldClose()) {
            break;
        }
        
        // Sleep for frame timing
        SleepForFrameTiming(maxFrameTime, lastFrameTime);
    }
}

void CameraSystemEditor::LoadAlgorithmParams(int algorithmIndex) {
    if (algorithmIndex < 0 || algorithmIndex >= 6) {
        return;
    }
    
    const AlgorithmParams& params = m_remeshParams[algorithmIndex];
    m_vipresRemeshMaxDistance = params.maxDistance;
    m_vipresRemeshMaxNeighbors = params.maxNeighbors;
    m_vipresRemeshProjectionPlane = params.projectionPlane;
    m_vipresRemeshDelaunayThreshold = params.delaunayThreshold;
    m_vipresRemeshPoissonRadius = params.poissonRadius;
    m_vipresRemeshPoissonDepth = params.poissonDepth;
    m_vipresRemeshPoissonDensity = params.poissonDensity;
    m_vipresRemeshPoissonSmoothNormals = params.poissonSmoothNormals;
    m_vipresRemeshFCTMSliceThickness = params.fctmSliceThickness;
    m_vipresRemeshGeodesicKNeighbors = params.geodesicKNeighbors;
    m_vipresRemeshGeodesicNumSeeds = params.geodesicNumSeeds;
    m_vipresRemeshGeodesicInteriorBias = params.geodesicInteriorBias;
}

void CameraSystemEditor::SaveAlgorithmParams(int algorithmIndex) {
    if (algorithmIndex < 0 || algorithmIndex >= 6) {
        return;
    }
    
    AlgorithmParams& params = m_remeshParams[algorithmIndex];
    params.maxDistance = m_vipresRemeshMaxDistance;
    params.maxNeighbors = m_vipresRemeshMaxNeighbors;
    params.projectionPlane = m_vipresRemeshProjectionPlane;
    params.delaunayThreshold = m_vipresRemeshDelaunayThreshold;
    params.poissonRadius = m_vipresRemeshPoissonRadius;
    params.poissonDepth = m_vipresRemeshPoissonDepth;
    params.poissonDensity = m_vipresRemeshPoissonDensity;
    params.poissonSmoothNormals = m_vipresRemeshPoissonSmoothNormals;
    params.fctmSliceThickness = m_vipresRemeshFCTMSliceThickness;
    params.geodesicKNeighbors = m_vipresRemeshGeodesicKNeighbors;
    params.geodesicNumSeeds = m_vipresRemeshGeodesicNumSeeds;
    params.geodesicInteriorBias = m_vipresRemeshGeodesicInteriorBias;
}

void CameraSystemEditor::ApplyRainbowColorsToObjects() {
    m_objectManager.ApplyRainbowColorsToObjects(m_objects);
}

void CameraSystemEditor::SaveSafeParameters() {
    // Don't save immediately after restoring (wait for next FPS update cycle)
    if (m_justRestoredParameters) {
        return;
    }
    
    // Only save if FPS is above 1fps
    if (m_currentFPS < 1.0f) {
        return;
    }
    
    m_safeParameters.loopEnabled = m_loopEnabled;
    m_safeParameters.captureEnabled = m_captureEnabled;
    m_safeParameters.tofDataEnabled = m_tofDataEnabled;
    m_safeParameters.tofColorCodeByCamera = m_tofColorCodeByCamera;
    m_safeParameters.vipresEnabled = m_vipresEnabled;
    m_safeParameters.vipresComputeColor = m_vipresComputeColor;
    m_safeParameters.vipresSamplingDensity = m_vipresSamplingDensity;
    m_safeParameters.vipresDepthLimit = m_vipresDepthLimit;
    m_safeParameters.vipresGriddingEnabled = m_vipresGriddingEnabled;
    m_safeParameters.vipresRemeshEnabled = m_vipresRemeshEnabled;
    m_safeParameters.vipresAutoRecomputeMesh = m_vipresAutoRecomputeMesh;
    m_safeParameters.vipresRemeshAlgorithm = m_vipresRemeshAlgorithm;
    m_safeParameters.vipresRemeshMaxDistance = m_vipresRemeshMaxDistance;
    m_safeParameters.vipresRemeshMaxNeighbors = m_vipresRemeshMaxNeighbors;
    m_safeParameters.vipresRemeshProjectionPlane = m_vipresRemeshProjectionPlane;
    m_safeParameters.vipresRemeshDelaunayThreshold = m_vipresRemeshDelaunayThreshold;
    m_safeParameters.vipresRemeshPoissonRadius = m_vipresRemeshPoissonRadius;
    m_safeParameters.vipresRemeshPoissonDepth = m_vipresRemeshPoissonDepth;
    m_safeParameters.vipresRemeshPoissonDensity = m_vipresRemeshPoissonDensity;
    m_safeParameters.vipresRemeshPoissonSmoothNormals = m_vipresRemeshPoissonSmoothNormals;
    m_safeParameters.vipresRemeshFCTMSliceThickness = m_vipresRemeshFCTMSliceThickness;
    m_safeParameters.vipresRemeshGeodesicKNeighbors = m_vipresRemeshGeodesicKNeighbors;
    m_safeParameters.vipresRemeshGeodesicNumSeeds = m_vipresRemeshGeodesicNumSeeds;
    m_safeParameters.vipresRemeshGeodesicInteriorBias = m_vipresRemeshGeodesicInteriorBias;
    m_safeParameters.vipresFreeCameraObjectEnabled = m_vipresFreeCameraObjectEnabled;
    m_safeParameters.vipresNoisyTofEnabled = m_vipresNoisyTofEnabled;
    m_safeParameters.vipresNoiseAmount = m_vipresNoiseAmount;
    m_safeParameters.vipresGridSize = m_vipresGridSize;
    m_safeParameters.vipresGridMode = m_vipresGridMode;
    m_safeParameters.vipresCameraEnabled = m_vipresCameraEnabled;
    m_safeParameters.gridVisible = m_world.IsGridVisible();
    m_safeParameters.tofLowPassCutoff = m_tofLowPassCutoff;
    m_safeParameters.tofSamplingRate = m_tofSamplingRate;
    m_safeParameters.tofLowPassEnabled = m_tofLowPassEnabled;
    m_safeParameters.tofTsdfEnabled = m_tofTsdfEnabled;
    m_safeParameters.tofTsdfStoreColor = m_tofTsdfStoreColor;
    m_safeParameters.tofTsdfTruncationDistance = m_tofTsdfTruncationDistance;
    m_safeParameters.tofTsdfWeightDecay = m_tofTsdfWeightDecay;
    
    // Save all algorithm parameters
    for (int i = 0; i < 7; i++) {
        m_safeParameters.remeshParams[i] = m_remeshParams[i];
    }
    
    m_hasSafeParameters = true;
}

void CameraSystemEditor::RestoreSafeParameters() {
    if (!m_hasSafeParameters) {
        return;
    }
    
    LOG_WARNING("CameraSystem", "FPS dropped below 1fps! Restoring safe parameters...");
    
    m_loopEnabled = m_safeParameters.loopEnabled;
    m_captureEnabled = m_safeParameters.captureEnabled;
    m_tofDataEnabled = m_safeParameters.tofDataEnabled;
    m_tofColorCodeByCamera = m_safeParameters.tofColorCodeByCamera;
    m_vipresEnabled = m_safeParameters.vipresEnabled;
    m_vipresComputeColor = m_safeParameters.vipresComputeColor;
    m_vipresSamplingDensity = m_safeParameters.vipresSamplingDensity;
    m_vipresDepthLimit = m_safeParameters.vipresDepthLimit;
    m_vipresGriddingEnabled = m_safeParameters.vipresGriddingEnabled;
    m_vipresRemeshEnabled = m_safeParameters.vipresRemeshEnabled;
    m_vipresAutoRecomputeMesh = m_safeParameters.vipresAutoRecomputeMesh;
    m_vipresRemeshAlgorithm = m_safeParameters.vipresRemeshAlgorithm;
    m_vipresRemeshMaxDistance = m_safeParameters.vipresRemeshMaxDistance;
    m_vipresRemeshMaxNeighbors = m_safeParameters.vipresRemeshMaxNeighbors;
    m_vipresRemeshProjectionPlane = m_safeParameters.vipresRemeshProjectionPlane;
    m_vipresRemeshDelaunayThreshold = m_safeParameters.vipresRemeshDelaunayThreshold;
    m_vipresRemeshPoissonRadius = m_safeParameters.vipresRemeshPoissonRadius;
    m_vipresRemeshPoissonDepth = m_safeParameters.vipresRemeshPoissonDepth;
    m_vipresRemeshPoissonDensity = m_safeParameters.vipresRemeshPoissonDensity;
    m_vipresRemeshPoissonSmoothNormals = m_safeParameters.vipresRemeshPoissonSmoothNormals;
    m_vipresRemeshFCTMSliceThickness = m_safeParameters.vipresRemeshFCTMSliceThickness;
    m_vipresRemeshGeodesicKNeighbors = m_safeParameters.vipresRemeshGeodesicKNeighbors;
    m_vipresRemeshGeodesicNumSeeds = m_safeParameters.vipresRemeshGeodesicNumSeeds;
    m_vipresRemeshGeodesicInteriorBias = m_safeParameters.vipresRemeshGeodesicInteriorBias;
    m_vipresFreeCameraObjectEnabled = m_safeParameters.vipresFreeCameraObjectEnabled;
    m_vipresNoisyTofEnabled = m_safeParameters.vipresNoisyTofEnabled;
    m_vipresNoiseAmount = m_safeParameters.vipresNoiseAmount;
    m_vipresGridSize = m_safeParameters.vipresGridSize;
    m_vipresGridMode = m_safeParameters.vipresGridMode;
    m_vipresCameraEnabled = m_safeParameters.vipresCameraEnabled;
    m_world.SetGridVisible(m_safeParameters.gridVisible);
    m_tofLowPassCutoff = m_safeParameters.tofLowPassCutoff;
    m_tofSamplingRate = m_safeParameters.tofSamplingRate;
    m_tofLowPassEnabled = m_safeParameters.tofLowPassEnabled;
    m_tofTsdfEnabled = m_safeParameters.tofTsdfEnabled;
    m_tofTsdfStoreColor = m_safeParameters.tofTsdfStoreColor;
    m_tofTsdfTruncationDistance = m_safeParameters.tofTsdfTruncationDistance;
    m_tofTsdfWeightDecay = m_safeParameters.tofTsdfWeightDecay;
    // Recompute filter coefficients after restoring parameters
    Algorithms::ComputeLowPassFilterCoefficients(m_tofLowPassCutoff, m_tofSamplingRate, m_lowPassFilterCoefficients);
    m_tofProcessing.ComputeLowPassFilterCoefficients(m_tofLowPassCutoff, m_tofSamplingRate);
    // Clear filter state and TSDF when restoring
    Algorithms::ClearLowPassFilterState(m_lowPassFilterState);
    m_tofProcessing.ClearLowPassFilterState();
    ClearTSDF();
    
    for (int i = 0; i < 7; i++) {
        m_remeshParams[i] = m_safeParameters.remeshParams[i];
    }
    
    // Reload current algorithm's parameters
    LoadAlgorithmParams(m_vipresRemeshAlgorithm);
    
    // Update point cloud if TOF is enabled
    if (m_tofDataEnabled) {
        UpdatePointCloud();
    }
    
    // Regenerate remesh if VIPRES remesh is enabled
    if (m_vipresEnabled && m_vipresRemeshEnabled) {
        m_remeshNeedsUpdate = true;
        GenerateRemesh();
    }
    
    // Set flag to prevent immediate save after restore
    m_justRestoredParameters = true;
    
    LOG_INFO("CameraSystem", "Safe parameters restored");
}

void CameraSystemEditor::CheckAndRestoreIfNeeded() {
    // Check if FPS dropped below 1fps
    if (m_currentFPS < 1.0f && m_hasSafeParameters) {
        RestoreSafeParameters();
    }
}

void CameraSystemEditor::Cleanup() {
    if (m_meshShader) {
        m_meshShader->ClearShader();
        delete m_meshShader;
        m_meshShader = nullptr;
    }
    
    // Grid shader and buffers are now handled by World class
    
    if (m_remeshBuffersInitialized) {
        if (m_remeshVAO) {
            glDeleteVertexArrays(1, &m_remeshVAO);
            m_remeshVAO = 0;
        }
        if (m_remeshVBO) {
            glDeleteBuffers(1, &m_remeshVBO);
            m_remeshVBO = 0;
        }
        if (m_remeshEBO) {
            glDeleteBuffers(1, &m_remeshEBO);
            m_remeshEBO = 0;
        }
        m_remeshBuffersInitialized = false;
    }
    
    m_objects.clear();
    
    if (m_window) {
        delete m_window;
        m_window = nullptr;
    }
    
    // Buffers are now handled by World class
}


} // namespace CameraSystem
} // namespace Editors
} // namespace Engine


