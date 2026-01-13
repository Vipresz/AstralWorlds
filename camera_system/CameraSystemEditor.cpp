#include "CameraSystemEditor.h"
#include "GlobalConstants.h"
#include "UIDefaults.h"
#include "config/ConfigManager.h"
#include "../../engine/platform/render/Window.h"
#include "../../engine/platform/render/Shader.h"
#include "../../engine/platform/render/Model.h"
#include "../../engine/platform/Logger.h"
#include "objects/ObjectRenderer.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <random>
#include <set>
#include <thread>
#include <tuple>
#include <unordered_set>
#include <vector>

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
using namespace Engine::Editors::CameraSystem::Defaults;

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
    // === Camera Controls - Toggles (from UIDefaults.h) ===
    , m_captureEnabled(CAPTURE_ENABLED)
    , m_tofDataEnabled(TOF_DATA_ENABLED)
    , m_extractColorEnabled(EXTRACT_COLOR_ENABLED)
    , m_tofRenderColorMode(static_cast<TOFRenderColorMode>(TOF_RENDER_COLOR_MODE))
    , m_captureOutputDir("./pics")  // Relative path for cross-platform compatibility
    , m_showObjectsPanel(false)  // Objects panel hidden by default
    , m_vipresEnabled(VIPRES_ENABLED)
    , m_vipresComputeColor(VIPRES_COMPUTE_COLOR)
    , m_vipresCameraEnabled()
    // === TOF Controls - Sliders (from UIDefaults.h, clamped in body) ===
    , m_vipresSamplingDensity(VIPRES_SAMPLING_DENSITY)
    , m_vipresDepthLimit(VIPRES_DEPTH_LIMIT)
    , m_vipresRemeshEnabled(VIPRES_REMESH_ENABLED)
    , m_vipresAutoRecomputeMesh(VIPRES_AUTO_RECOMPUTE_MESH)
    , m_vipresRemeshAlgorithm(VIPRES_REMESH_ALGORITHM)
    , m_vipresRemeshMaxDistance(VIPRES_REMESH_MAX_DISTANCE)
    , m_vipresRemeshMaxNeighbors(VIPRES_REMESH_MAX_NEIGHBORS)
    , m_vipresRemeshProjectionPlane(VIPRES_REMESH_PROJECTION_PLANE)
    , m_vipresRemeshDelaunayThreshold(VIPRES_REMESH_DELAUNAY_THRESHOLD)
    , m_vipresRemeshPoissonRadius(VIPRES_REMESH_POISSON_RADIUS)
    , m_vipresRemeshPoissonDepth(VIPRES_REMESH_POISSON_DEPTH)
    , m_vipresRemeshPoissonDensity(VIPRES_REMESH_POISSON_DENSITY)
    , m_vipresRemeshPoissonSmoothNormals(VIPRES_REMESH_POISSON_SMOOTH_NORMALS)
    , m_vipresRemeshFCTMSliceThickness(VIPRES_REMESH_FCTM_SLICE_THICKNESS)
    , m_vipresRemeshGeodesicKNeighbors(VIPRES_REMESH_GEODESIC_K_NEIGHBORS)
    , m_vipresRemeshGeodesicNumSeeds(REMESH_GEODESIC_NUM_SEEDS_DEFAULT)  // From GlobalConstants
    , m_vipresRemeshGeodesicInteriorBias(VIPRES_REMESH_GEODESIC_INTERIOR_BIAS)
    , m_vipresRemeshDensityVoxelSize(REMESH_DENSITY_VOXEL_SIZE_DEFAULT)
    , m_vipresRemeshDensitySmoothRadius(REMESH_DENSITY_SMOOTH_RADIUS_DEFAULT)
    , m_vipresRemeshDensityIsoValue(REMESH_DENSITY_ISO_VALUE_DEFAULT)
    , m_vipresRemeshSurfaceOnly(true)
    , m_vipresRemeshSurfaceCameraConsistent(true)
    , m_vipresRemeshSurfaceNormalThreshold(0.0f)
    , m_vipresFreeCameraObjectEnabled(VIPRES_FREE_CAMERA_OBJECT_ENABLED)
    , m_vipresNoisyTofEnabled(VIPRES_NOISY_TOF_ENABLED)
    , m_vipresNoiseAmount(VIPRES_NOISE_AMOUNT)
    , m_vipresGridSize(VIPRES_GRID_SIZE)
    , m_vipresGridMode(TOFGridMode::Disabled)  // Gridding disabled by default
    , m_tofLowPassCutoff(TOF_LOW_PASS_CUTOFF)
    , m_tofSamplingRate(TOF_SAMPLING_RATE)
    , m_tofLowPassEnabled(TOF_LOW_PASS_ENABLED)
    , m_tofTsdfEnabled(TOF_TSDF_ENABLED)
    , m_tofTsdfStoreColor(TOF_TSDF_STORE_COLOR)
    , m_tofTsdfTruncationDistance(TOF_TSDF_TRUNCATION_DISTANCE)
    , m_tofTsdfWeightDecay(TOF_TSDF_WEIGHT_DECAY)
    // === Ouster LiDAR (disabled by default) ===
    , m_ousterEnabled(false)
    , m_ousterHostname("172.17.1.56")
    , m_lastOusterTimestamp(0.0)
    // === Smart Occupancy - Toggles and Sliders (from UIDefaults.h) ===
    , m_smartOccupancyEnabled(SMART_OCCUPANCY_ENABLED)
    , m_smartOccupancyNeighborConsistency(SMART_OCCUPANCY_NEIGHBOR_CONSISTENCY)
    , m_smartOccupancyNeighborRadius(SMART_OCCUPANCY_NEIGHBOR_RADIUS)
    , m_smartOccupancyResidualThreshold(SMART_OCCUPANCY_RESIDUAL_THRESHOLD)
    , m_smartOccupancyPersistence(SMART_OCCUPANCY_PERSISTENCE)
    , m_smartOccupancyPersistenceGamma(SMART_OCCUPANCY_PERSISTENCE_GAMMA)
    , m_smartOccupancyVariance(SMART_OCCUPANCY_VARIANCE)
    , m_smartOccupancyVarianceThreshold(SMART_OCCUPANCY_VARIANCE_THRESHOLD)
    , m_smartOccupancyEviction(SMART_OCCUPANCY_EVICTION)
    , m_smartOccupancyEvictionDecay(SMART_OCCUPANCY_EVICTION_DECAY)
    , m_smartOccupancyEvictionIncrement(SMART_OCCUPANCY_EVICTION_INCREMENT)
    , m_smartOccupancyEvictionMinStartValue(SMART_OCCUPANCY_EVICTION_MIN_START_VALUE)
    , m_smartOccupancyEviction2(SMART_OCCUPANCY_EVICTION2)
    , m_smartOccupancyEviction2AppearanceThreshold(SMART_OCCUPANCY_EVICTION2_APPEARANCE_THRESHOLD)
    , m_smartOccupancyEviction2RemovalThreshold(SMART_OCCUPANCY_EVICTION2_REMOVAL_THRESHOLD)
    , m_weightThreshold(WEIGHT_THRESHOLD)
    // === Segmentation (from UIDefaults.h) ===
    , m_segmentationIdEnabled(SEGMENTATION_ID_ENABLED)
    , m_segmentMatchRadius(SEGMENT_MATCH_RADIUS)
    // === Surface Flow (from UIDefaults.h) ===
    , m_surfaceFlowEnabled(SURFACE_FLOW_ENABLED)
    , m_surfaceFlowRadius(SURFACE_FLOW_RADIUS)
    , m_surfaceFlowThreshold(SURFACE_FLOW_THRESHOLD)
    // === Internal state ===
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
    , m_albedoShader(nullptr)
    , m_pendingCameraSwitch(SIZE_MAX)
    , m_hasSafeParameters(false)
    , m_framesSinceStartup(0)
    , m_justRestoredParameters(false)
    // === Camera state (from UIDefaults.h) ===
    , m_freeCameraPos(10.0f, 10.0f, 5.0f)
    , m_freeCameraYaw(FREE_CAMERA_YAW)
    , m_freeCameraPitch(FREE_CAMERA_PITCH)
    , m_movementSpeed(Defaults::CAMERA_MOVEMENT_SPEED)
    , m_mouseRightDown(false)
    , m_lastMouseX(0.0)
    , m_lastMouseY(0.0)
    // === Animation state ===
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
    , m_loopEnabled(LOOP_ENABLED)
{
    // Clamp slider values to their valid ranges (in case UIDefaults exceed limits)
    m_vipresSamplingDensity = std::clamp(m_vipresSamplingDensity, SAMPLING_DENSITY_MIN, SAMPLING_DENSITY_MAX);
    m_vipresDepthLimit = std::clamp(m_vipresDepthLimit, DEPTH_LIMIT_MIN, DEPTH_LIMIT_MAX);
    m_vipresGridSize = std::clamp(m_vipresGridSize, GRID_SIZE_MIN, GRID_SIZE_MAX);
    m_tofSamplingRate = std::clamp(m_tofSamplingRate, TOF_SAMPLING_RATE_MIN, TOF_SAMPLING_RATE_MAX);
    m_tofLowPassCutoff = std::clamp(m_tofLowPassCutoff, LOW_PASS_CUTOFF_MIN, LOW_PASS_CUTOFF_MAX);
    m_tofTsdfTruncationDistance = std::clamp(m_tofTsdfTruncationDistance, TSDF_TRUNCATION_DISTANCE_MIN, TSDF_TRUNCATION_DISTANCE_MAX);
    m_tofTsdfWeightDecay = std::clamp(m_tofTsdfWeightDecay, TSDF_WEIGHT_DECAY_MIN, TSDF_WEIGHT_DECAY_MAX);
    m_vipresNoiseAmount = std::clamp(m_vipresNoiseAmount, NOISE_AMOUNT_MIN, NOISE_AMOUNT_MAX);
    m_vipresRemeshMaxDistance = std::clamp(m_vipresRemeshMaxDistance, REMESH_MAX_DISTANCE_MIN, REMESH_MAX_DISTANCE_MAX);
    m_vipresRemeshMaxNeighbors = std::clamp(m_vipresRemeshMaxNeighbors, REMESH_MAX_NEIGHBORS_MIN, REMESH_MAX_NEIGHBORS_MAX);
    m_vipresRemeshDelaunayThreshold = std::clamp(m_vipresRemeshDelaunayThreshold, REMESH_DELAUNAY_THRESHOLD_MIN, REMESH_DELAUNAY_THRESHOLD_MAX);
    m_vipresRemeshPoissonRadius = std::clamp(m_vipresRemeshPoissonRadius, REMESH_POISSON_RADIUS_MIN, REMESH_POISSON_RADIUS_MAX);
    m_vipresRemeshPoissonDepth = std::clamp(m_vipresRemeshPoissonDepth, REMESH_POISSON_DEPTH_MIN, REMESH_POISSON_DEPTH_MAX);
    m_vipresRemeshPoissonDensity = std::clamp(m_vipresRemeshPoissonDensity, REMESH_POISSON_DENSITY_MIN, REMESH_POISSON_DENSITY_MAX);
    m_vipresRemeshFCTMSliceThickness = std::clamp(m_vipresRemeshFCTMSliceThickness, REMESH_FCTM_SLICE_THICKNESS_MIN, REMESH_FCTM_SLICE_THICKNESS_MAX);
    m_vipresRemeshGeodesicKNeighbors = std::clamp(m_vipresRemeshGeodesicKNeighbors, REMESH_GEODESIC_K_NEIGHBORS_MIN, REMESH_GEODESIC_K_NEIGHBORS_MAX);
    m_vipresRemeshGeodesicInteriorBias = std::clamp(m_vipresRemeshGeodesicInteriorBias, REMESH_GEODESIC_INTERIOR_BIAS_MIN, REMESH_GEODESIC_INTERIOR_BIAS_MAX);
    m_smartOccupancyNeighborRadius = std::clamp(m_smartOccupancyNeighborRadius, OCCUPANCY_NEIGHBOR_RADIUS_MIN, OCCUPANCY_NEIGHBOR_RADIUS_MAX);
    m_smartOccupancyResidualThreshold = std::clamp(m_smartOccupancyResidualThreshold, OCCUPANCY_RESIDUAL_THRESHOLD_MIN, OCCUPANCY_RESIDUAL_THRESHOLD_MAX);
    m_smartOccupancyPersistenceGamma = std::clamp(m_smartOccupancyPersistenceGamma, OCCUPANCY_PERSISTENCE_GAMMA_MIN, OCCUPANCY_PERSISTENCE_GAMMA_MAX);
    m_smartOccupancyVarianceThreshold = std::clamp(m_smartOccupancyVarianceThreshold, OCCUPANCY_VARIANCE_THRESHOLD_MIN, OCCUPANCY_VARIANCE_THRESHOLD_MAX);
    m_smartOccupancyEvictionDecay = std::clamp(m_smartOccupancyEvictionDecay, OCCUPANCY_EVICTION_DECAY_MIN, OCCUPANCY_EVICTION_DECAY_MAX);
    m_smartOccupancyEvictionIncrement = std::clamp(m_smartOccupancyEvictionIncrement, OCCUPANCY_EVICTION_INCREMENT_MIN, OCCUPANCY_EVICTION_INCREMENT_MAX);
    m_smartOccupancyEvictionMinStartValue = std::clamp(m_smartOccupancyEvictionMinStartValue, OCCUPANCY_EVICTION_MIN_START_MIN, OCCUPANCY_EVICTION_MIN_START_MAX);
    m_smartOccupancyEviction2AppearanceThreshold = std::clamp(m_smartOccupancyEviction2AppearanceThreshold, OCCUPANCY_EVICTION2_APPEARANCE_THRESHOLD_MIN, OCCUPANCY_EVICTION2_APPEARANCE_THRESHOLD_MAX);
    m_smartOccupancyEviction2RemovalThreshold = std::clamp(m_smartOccupancyEviction2RemovalThreshold, OCCUPANCY_EVICTION2_REMOVAL_THRESHOLD_MIN, OCCUPANCY_EVICTION2_REMOVAL_THRESHOLD_MAX);
    m_weightThreshold = std::clamp(m_weightThreshold, OCCUPANCY_WEIGHT_THRESHOLD_MIN, OCCUPANCY_WEIGHT_THRESHOLD_MAX);
    
    // Initialize World grid visibility from UIDefaults
    m_world.SetGridVisible(GRID_VISIBLE);
    
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
    
    // Load UI defaults from TOML config file
    // Try multiple paths: relative to working directory, then relative to executable
    UIConfig& config = UIConfig::Instance();
    bool configLoaded = config.LoadFromFile("camera_system/ui_defaults.toml");
    if (!configLoaded) {
        configLoaded = config.LoadFromFile("../camera_system/ui_defaults.toml");
    }
    if (!configLoaded) {
        configLoaded = config.LoadFromFile("../../camera_system/ui_defaults.toml");
    }
    if (!configLoaded) {
        LOG_WARNING("CameraSystem", "Could not load ui_defaults.toml - using hardcoded defaults");
    }
    
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
        
        // Uninstall the Window class's key callback - it expects the user pointer to be
        // a Window*, but we've overwritten it with CameraSystemEditor*. We use glfwGetKey()
        // for polling instead of callbacks, so the key callback is not needed.
        glfwSetKeyCallback(window, nullptr);
        
        glfwSetFramebufferSizeCallback(window, [](GLFWwindow* win, int width, int height) {
            CameraSystemEditor* editor = static_cast<CameraSystemEditor*>(glfwGetWindowUserPointer(win));
            if (!editor || !editor->m_window) return;
            
            glViewport(0, 0, width, height);
            float aspect = (float)width / (float)height;
            editor->m_projection = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100000.0f);
        });
    }
    
    // Initialize Ouster LiDAR stream (disabled by default)
    m_ousterStream = CreateOusterStream(m_ousterHostname);
    OusterConfig ousterConfig;
    ousterConfig.hostname = m_ousterHostname;
    ousterConfig.minRange = 0.3f;
    ousterConfig.maxRange = 50.0f;
    ousterConfig.downsampleFactor = 1;
    ousterConfig.cameraId = 99;  // Use camera ID 99 for Ouster data
    ousterConfig.useIntensityAsColor = true;
    m_ousterStream->SetConfig(ousterConfig);
    LOG_INFO("CameraSystem", "Ouster LiDAR stream initialized (disabled by default)");
    
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
    
    // Load albedo shader for capturing unlit texture/vertex colors
    m_albedoShader = new Shader();
    m_albedoShader->CreateFromFiles("engine/platform/render/shaders/albedo.vert", "engine/platform/render/shaders/albedo.frag");
    if (!m_albedoShader->IsValid()) {
        m_albedoShader->CreateFromFiles("../../engine/platform/render/shaders/albedo.vert", "../../engine/platform/render/shaders/albedo.frag");
    }
    if (!m_albedoShader->IsValid()) {
        m_albedoShader->CreateFromFiles("shaders/albedo.vert", "shaders/albedo.frag");
    }
    if (!m_albedoShader->IsValid()) {
        LOG_WARNING("CameraSystem", "Albedo shader failed to compile - compute color will use lit colors");
        // Not a fatal error - we can fall back to the mesh shader
    } else {
        m_albedoShader->Validate();
        LOG_INFO("CameraSystem", "Albedo shader loaded successfully");
    }
    
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

void CameraSystemEditor::ResetToConfigDefaults() {
    UIConfig& cfg = UIConfig::Instance();
    
    // Reload the config file to pick up any changes
    cfg.Reload();
    
    LOG_INFO("CameraSystem", "Resetting UI to config defaults...");
    
    // Camera settings
    m_movementSpeed = cfg.GetCameraMovementSpeed();
    m_freeCameraYaw = cfg.GetCameraYaw();
    m_freeCameraPitch = cfg.GetCameraPitch();
    
    // Grid settings
    m_world.SetGridVisible(cfg.GetGridVisible());
    
    // Camera Controls - Toggles
    m_loopEnabled = cfg.GetLoopEnabled();
    // Don't reset capture enabled - that's a runtime action toggle
    m_tofDataEnabled = cfg.GetTofDataEnabled();
    m_vipresEnabled = cfg.GetVipresEnabled();
    m_vipresFreeCameraObjectEnabled = cfg.GetVipresFreeObjectEnabled();
    
    // TOF Controls - Toggles
    m_extractColorEnabled = cfg.GetExtractColorEnabled();
    m_tofRenderColorMode = static_cast<TOFRenderColorMode>(cfg.GetTofRenderColorMode());
    // Note: gridding is now controlled by m_vipresGridMode (Disabled = gridding off)
    // If config had gridding disabled, set mode to Disabled
    if (!cfg.GetGriddingEnabled()) {
        m_vipresGridMode = TOFGridMode::Disabled;
    }
    m_tofLowPassEnabled = cfg.GetLowPassEnabled();
    m_tofTsdfEnabled = cfg.GetTsdfEnabled();
    m_tofTsdfStoreColor = cfg.GetTsdfStoreColor();
    m_vipresNoisyTofEnabled = cfg.GetNoisyTofEnabled();
    
    // TOF Controls - Sliders
    m_vipresSamplingDensity = cfg.GetSamplingDensity();
    m_vipresDepthLimit = cfg.GetDepthLimit();
    m_tofSamplingRate = cfg.GetTofSamplingRate();
    m_vipresGridSize = cfg.GetGridSize();
    // Grid mode now has Disabled at index 0, so old config values need +1 if gridding was enabled
    int configGridMode = cfg.GetGridMode();
    if (cfg.GetGriddingEnabled() && configGridMode >= 0 && configGridMode <= 2) {
        // Old config: 0=Average, 1=FirstCome, 2=GridValue -> New: 1=Average, 2=FirstCome, 3=GridValue
        m_vipresGridMode = static_cast<TOFGridMode>(configGridMode + 1);
    } else if (!cfg.GetGriddingEnabled()) {
        m_vipresGridMode = TOFGridMode::Disabled;
    } else {
        m_vipresGridMode = static_cast<TOFGridMode>(configGridMode);
    }
    m_tofTsdfTruncationDistance = cfg.GetTsdfTruncationDistance();
    m_tofTsdfWeightDecay = cfg.GetTsdfWeightDecay();
    m_tofLowPassCutoff = cfg.GetLowPassCutoff();
    m_vipresNoiseAmount = cfg.GetNoiseAmount();
    
    // Smart Occupancy settings
    m_smartOccupancyEnabled = cfg.GetOccupancyEnabled();
    m_smartOccupancyNeighborConsistency = cfg.GetOccupancyNeighborConsistency();
    m_smartOccupancyPersistence = cfg.GetOccupancyPersistence();
    m_smartOccupancyVariance = cfg.GetOccupancyVariance();
    m_smartOccupancyEviction = cfg.GetOccupancyEviction();
    m_smartOccupancyEviction2 = cfg.GetOccupancyEviction2();
    
    m_smartOccupancyNeighborRadius = cfg.GetOccupancyNeighborRadius();
    m_smartOccupancyResidualThreshold = cfg.GetOccupancyResidualThreshold();
    m_smartOccupancyPersistenceGamma = cfg.GetOccupancyPersistenceGamma();
    m_smartOccupancyVarianceThreshold = cfg.GetOccupancyVarianceThreshold();
    m_smartOccupancyEvictionDecay = cfg.GetOccupancyEvictionDecay();
    m_smartOccupancyEvictionIncrement = cfg.GetOccupancyEvictionIncrement();
    m_smartOccupancyEvictionMinStartValue = cfg.GetOccupancyEvictionMinStartValue();
    m_smartOccupancyEviction2AppearanceThreshold = cfg.GetOccupancyEviction2AppearThreshold();
    m_smartOccupancyEviction2RemovalThreshold = cfg.GetOccupancyEviction2RemovalThreshold();
    m_weightThreshold = cfg.GetWeightThreshold();
    
    // VIPRES settings
    m_vipresComputeColor = cfg.GetVipresComputeColor();
    m_segmentationIdEnabled = cfg.GetSegmentationIdEnabled();
    m_surfaceFlowEnabled = cfg.GetSurfaceFlowEnabled();
    m_vipresRemeshEnabled = cfg.GetRemeshEnabled();
    m_vipresAutoRecomputeMesh = cfg.GetAutoRecomputeMesh();
    m_vipresRemeshPoissonSmoothNormals = cfg.GetRemeshPoissonSmoothNormals();
    
    m_surfaceFlowRadius = cfg.GetSurfaceFlowRadius();
    m_surfaceFlowThreshold = cfg.GetSurfaceFlowThreshold();
    m_vipresRemeshAlgorithm = cfg.GetRemeshAlgorithm();
    m_vipresRemeshMaxDistance = cfg.GetRemeshMaxDistance();
    m_vipresRemeshMaxNeighbors = cfg.GetRemeshMaxNeighbors();
    m_vipresRemeshProjectionPlane = cfg.GetRemeshProjectionPlane();
    m_vipresRemeshDelaunayThreshold = cfg.GetRemeshDelaunayThreshold();
    m_vipresRemeshPoissonRadius = cfg.GetRemeshPoissonRadius();
    m_vipresRemeshPoissonDepth = cfg.GetRemeshPoissonDepth();
    m_vipresRemeshPoissonDensity = cfg.GetRemeshPoissonDensity();
    m_vipresRemeshFCTMSliceThickness = cfg.GetRemeshFctmSliceThickness();
    m_vipresRemeshGeodesicKNeighbors = cfg.GetRemeshGeodesicKNeighbors();
    m_vipresRemeshGeodesicInteriorBias = cfg.GetRemeshGeodesicInteriorBias();
    m_segmentMatchRadius = cfg.GetSegmentMatchRadius();
    
    // DensitySDF settings
    m_vipresRemeshDensityVoxelSize = cfg.GetDensityVoxelSize();
    m_vipresRemeshDensitySmoothRadius = cfg.GetDensitySmoothRadius();
    m_vipresRemeshDensityIsoValue = cfg.GetDensityIsoValue();
    
    LOG_INFO("CameraSystem", "UI reset to config defaults complete");
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
    
    // Apply rainbow colors to loaded objects (sets vertex colors for Color by Object mode)
    if (!m_objects.empty()) {
        ApplyRainbowColorsToObjects();
        LOG_INFO_FMT("CameraSystem", "Applied rainbow colors to %zu objects", m_objects.size());
    }
    
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
    
    // ========================================================================
    // TOF PIPELINE START
    // ========================================================================
    // The TOF pipeline is responsible for:
    //   1. Rendering depth from each anchor camera (or receiving from Ouster LiDAR)
    //   2. Extracting 3D point positions from depth buffer
    //   3. Extracting color from rendered image (if Compute Color is enabled)
    //   4. Applying noise (if enabled)
    //   5. Assigning segment IDs based on nearest object
    //   6. TSDF integration (if enabled)
    //   7. Gridding (if enabled)
    //   8. Low-pass filtering (if enabled)
    //   9. Smart occupancy weighting (if enabled)
    // ========================================================================
    
    // Increment frame processing counter to refresh noise at every frame
    m_frameProcessingCounter++;
    
    // Clear storage (this removes all points)
    m_pointCloudGrid.clear();
    m_gridAccumulators.clear();
    // Clear filtered TOF write buffer (we'll write new data to it)
    m_filteredTofData[m_filteredTofWriteBuffer].clear();
    // Clear point cloud write buffer (we'll process into it)
    m_pointCloud[m_pointCloudWriteBuffer].clear();
    
    // ========================================================================
    // OUSTER LIDAR MODE - Use real sensor data instead of simulated TOF
    // ========================================================================
    if (m_ousterEnabled) {
        if (!m_ousterStream) {
            LOG_WARNING("CameraSystem", "Ouster enabled but stream not initialized");
            return;
        }
        
        // Make sure streaming is started
        if (!m_ousterStream->IsStreaming()) {
            LOG_INFO("CameraSystem", "Starting Ouster stream...");
            if (!m_ousterStream->IsConnected()) {
                m_ousterStream->Connect();
            }
            m_ousterStream->StartStreaming();
        }
        
        std::vector<PointCloudPoint> ousterPoints;
        double timestamp;
        
        // Poll for new Ouster data
        if (m_ousterStream->PollPointCloud(ousterPoints, timestamp)) {
            m_lastOusterTimestamp = timestamp;
            
            LOG_INFO_FMT("CameraSystem", "Received %zu points from Ouster (timestamp: %.3f)", 
                        ousterPoints.size(), timestamp);
            
            // Copy Ouster points to filtered TOF buffer (bypasses simulated depth capture)
            m_filteredTofData[m_filteredTofWriteBuffer] = std::move(ousterPoints);
            
            // Apply gridding if enabled
            if (m_vipresGridMode != TOFGridMode::Disabled) {
                m_tofProcessing.ApplyGridding(m_filteredTofData[m_filteredTofWriteBuffer], 
                                              m_pointCloudGrid, 
                                              m_vipresGridMode, 
                                              m_vipresGridSize);
                
                // Extract gridded points to write buffer
                for (const auto& pair : m_pointCloudGrid) {
                    m_pointCloud[m_pointCloudWriteBuffer].push_back(pair.second);
                }
            } else {
                // No gridding - use raw Ouster points
                m_pointCloud[m_pointCloudWriteBuffer] = m_filteredTofData[m_filteredTofWriteBuffer];
            }
            
            // Swap buffers
            m_filteredTofReadBuffer = m_filteredTofWriteBuffer;
            m_filteredTofWriteBuffer = 1 - m_filteredTofWriteBuffer;
            m_pointCloudReadBuffer = m_pointCloudWriteBuffer;
            m_pointCloudWriteBuffer = 1 - m_pointCloudWriteBuffer;
        } else {
            // No new data this frame - keep previous data (don't clear)
            LOG_DEBUG("CameraSystem", "No new Ouster data this frame");
        }
        
        // Always return when Ouster is enabled - never fall through to simulated TOF
        return;
    }
    
    // ========================================================================
    // SIMULATED TOF MODE - Render depth from virtual cameras
    // ========================================================================
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
        
        // Set clear color to black for color extraction (so we can distinguish objects from background)
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);  // Enable depth writes
        glDisable(GL_CULL_FACE);
        glViewport(0, 0, fullWidth, fullHeight);
        
        if (m_world.IsGridVisible()) {
            RenderGrid();
        }
        
        // Do we need to capture unlit colors from the scene?
        // Yes if any of:
        //   - Extract Color from TOF toggle is enabled
        //   - Compute Color for VIPRES is enabled
        //   - Color by Object render mode is selected
        bool needColors = m_extractColorEnabled || 
                          m_vipresComputeColor || 
                          (m_tofRenderColorMode == TOFRenderColorMode::ColorByObject);
        
        // Choose shader based on whether we need unlit colors
        // Use albedo shader for pure texture colors (no lighting)
        // Fall back to mesh shader if albedo shader not available
        Shader* renderShader = m_meshShader;
        bool useAlbedoShader = needColors && m_albedoShader && m_albedoShader->IsValid();
        if (useAlbedoShader) {
            renderShader = m_albedoShader;
        }
        
        if (renderShader && renderShader->IsValid()) {
            renderShader->UseShader();
            GLuint shaderID = renderShader->GetShaderID();
            
            GLint uniformView = glGetUniformLocation(shaderID, "view");
            GLint uniformProjection = glGetUniformLocation(shaderID, "projection");
            if (uniformView != -1) glUniformMatrix4fv(uniformView, 1, GL_FALSE, glm::value_ptr(camView));
            if (uniformProjection != -1) glUniformMatrix4fv(uniformProjection, 1, GL_FALSE, glm::value_ptr(m_projection));
            
            for (size_t i = 0; i < m_objects.size(); i++) {
                if (m_objects[i] && m_objects[i]->visible) {
                    // For albedo shader, we need to set useTexture and model uniforms
                    if (useAlbedoShader) {
                        GLint uniformModel = glGetUniformLocation(shaderID, "model");
                        GLint uniformUseTexture = glGetUniformLocation(shaderID, "useTexture");
                        GLint uniformUseBones = glGetUniformLocation(shaderID, "useBones");
                        
                        // Build model matrix (same as ObjectRenderer)
                        glm::mat4 modelMatrix = glm::mat4(1.0f);
                        modelMatrix = glm::translate(modelMatrix, m_objects[i]->position);
                        modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
                        modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
                        modelMatrix = glm::rotate(modelMatrix, glm::radians(m_objects[i]->rotation.z), glm::vec3(0.0f, 1.0f, 0.0f));
                        modelMatrix = glm::rotate(modelMatrix, glm::radians(m_objects[i]->rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
                        modelMatrix = glm::rotate(modelMatrix, glm::radians(m_objects[i]->rotation.y), glm::vec3(0.0f, 0.0f, 1.0f));
                        modelMatrix = glm::scale(modelMatrix, m_objects[i]->scale);
                        
                        if (uniformModel != -1) glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(modelMatrix));
                        if (uniformUseBones != -1) glUniform1i(uniformUseBones, 0);
                        
                        // Set texture sampler to use texture unit 1 (where Texture::UseTexture binds)
                        GLint uniformTexture = glGetUniformLocation(shaderID, "theTexture");
                        if (uniformTexture != -1) glUniform1i(uniformTexture, 1);
                        
                        // Render the model
                        // For "Color by Object" mode, always use vertex colors (rainbow)
                        // to capture the height-based gradient colors we applied
                        // For other modes, use textures if available
                        if (m_objects[i]->model) {
                            bool useTextureForCapture = false;
                            
                            // Only use textures if:
                            // 1. The model actually has textures, AND
                            // 2. We're NOT in "Color by Object" mode (which uses rainbow vertex colors)
                            if (m_objects[i]->model->HasTextures() && 
                                m_tofRenderColorMode != TOFRenderColorMode::ColorByObject) {
                                useTextureForCapture = true;
                            }
                            
                            if (uniformUseTexture != -1) glUniform1i(uniformUseTexture, useTextureForCapture ? 1 : 0);
                            
                            m_objects[i]->model->RenderModel();
                        }
                    } else {
                        RenderObject(m_objects[i].get());
                    }
                }
            }
        }
        
        // Ensure rendering is complete before reading
        glFinish();
        
        // Read depth buffer from back buffer
        std::vector<float> depthBuffer(fullWidth * fullHeight);
        glReadBuffer(GL_BACK);
        glReadPixels(0, 0, fullWidth, fullHeight, GL_DEPTH_COMPONENT, GL_FLOAT, depthBuffer.data());
        
        // Read color buffer if we need colors (already determined above for shader selection)
        std::vector<unsigned char> colorBuffer;
        if (needColors) {
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
        // Always use white as fallback color - the actual object color will be read from colorBuffer if available
        const glm::vec3 fallbackColor = glm::vec3(1.0f);
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
                
                // Extract color from the rendered image at this TOF coordinate
                // This is the object's actual color from the image data
                // Used for:
                //   - VIPRES remeshing (when Compute Color is enabled)
                //   - Point cloud display (when Color by Object mode is selected)
                glm::vec3 color = fallbackColor;
                if (needColors && !colorBuffer.empty()) {
                    const int pixelIndex = (yIndex + x) * 4;  // RGBA format
                    if (pixelIndex + 2 < static_cast<int>(colorBuffer.size())) {
                        // Extract RGB channels from the image at this pixel coordinate
                        color = glm::vec3(
                            colorBuffer[pixelIndex] * inv255,      // R
                            colorBuffer[pixelIndex + 1] * inv255,  // G
                            colorBuffer[pixelIndex + 2] * inv255   // B
                        );
                        
                    }
                }
                
                PointCloudPoint point;
                point.position = glm::vec3(worldPos4);
                point.color = color;
                point.cameraId = static_cast<int32_t>(camIdx);
                
                // Assign initial segment ID based on nearest scene object
                if (m_segmentationIdEnabled && !m_objects.empty()) {
                    float minDist = std::numeric_limits<float>::max();
                    int32_t nearestObjIdx = -1;
                    for (size_t objIdx = 0; objIdx < m_objects.size(); objIdx++) {
                        if (!m_objects[objIdx] || !m_objects[objIdx]->visible) continue;
                        float dist = glm::length(point.position - m_objects[objIdx]->position);
                        if (dist < minDist) {
                            minDist = dist;
                            nearestObjIdx = static_cast<int32_t>(objIdx);
                        }
                    }
                    point.segmentId = nearestObjIdx;
                }
                
                m_filteredTofData[m_filteredTofWriteBuffer].push_back(point);
            }
        }
        
        // Unbind shader after rendering for this camera
        glUseProgram(0);
    }
    
    UpdateCamera();
    
    // Swap filtered TOF data: write buffer becomes read buffer for processing
    std::swap(m_filteredTofReadBuffer, m_filteredTofWriteBuffer);
    
    // ------------------------------------------------------------------------
    // TOF POST-PROCESSING PIPELINE
    // ------------------------------------------------------------------------
    // Process filtered TOF data from read buffer into final point cloud

    std::vector<PointCloudPoint> processedData = m_filteredTofData[m_filteredTofReadBuffer];
    
    // TOF Step 1: Apply TSDF if enabled (before gridding, mutually exclusive with low-pass)
    if (m_tofTsdfEnabled) {
        // Ensure low-pass is disabled (mutually exclusive)
        if (m_tofLowPassEnabled) {
            m_tofLowPassEnabled = false;
            Algorithms::ClearLowPassFilterState(m_lowPassFilterState);
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
    
    // TOF Step 2: Apply gridding if enabled (using TOFProcessing)
    // NOTE: Some variables have "vipres" prefix for historical reasons but are part of TOF pipeline
    std::vector<GridKey> pointGridKeys;  // Track grid keys for smart occupancy
    
    bool griddingEnabled = (m_vipresGridMode != TOFGridMode::Disabled);
    if (griddingEnabled) {
        // ApplyGridding clears and populates the grid, so no need to clear here
        m_tofProcessing.ApplyGridding(processedData, m_pointCloudGrid, m_vipresGridMode, m_vipresGridSize);
        
        // TOF Step 3: Apply low-pass filter if enabled (BEFORE smart occupancy)
        // Low-pass smooths the raw data first
        if (m_tofLowPassEnabled && !m_tofTsdfEnabled) {
            // This ensures filter responds immediately to slider changes
            Algorithms::ComputeLowPassFilterCoefficients(m_tofLowPassCutoff, m_tofSamplingRate, m_lowPassFilterCoefficients);
            
            // Apply weight-based low-pass filter to gridded points
            Algorithms::ApplyWeightBasedLowPassFilter(m_pointCloudGrid, m_lowPassFilterCoefficients, m_lowPassFilterState);
        }
        
        // TOF Step 4: Apply smart occupancy weights if enabled (AFTER low-pass filter)
        // Smart occupancy analyzes the smoothed data for consistency/persistence
        if (m_smartOccupancyEnabled) {
            ApplySmartOccupancyWeights(m_pointCloudGrid);
        } else {
            // Set default weights to 1.0 if smart occupancy is disabled
            for (auto& pair : m_pointCloudGrid) {
                pair.second.weight = 1.0f;
            }
        }
        
        // TOF Step 5: Apply segmentation IDs if enabled (AFTER all filtering)
        // This assigns IDs to points based on spatial overlap - points from different cameras
        // that fall into the same grid cell are considered part of the same object
        if (m_segmentationIdEnabled) {
            AssignSegmentationIds(m_pointCloudGrid);
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
        
        // Apply segmentation IDs for non-gridded data
        // Segment IDs are already assigned during point extraction based on nearest object
        // Here we just need to collect active segment IDs
        if (m_segmentationIdEnabled) {
            m_activeSegmentIds.clear();
            for (const auto& point : processedData) {
                if (point.segmentId >= 0) {
                    m_activeSegmentIds.insert(point.segmentId);
                }
            }
        }
    }
    
    // ========================================================================
    // TOF Step 6: Surface Flow Filtering (temporal smoothing)
    // ========================================================================
    // Uses 3-frame history to detect local surface motion and remove outlier points
    // that don't match the local flow pattern. This stabilizes the point cloud.
    // ========================================================================
    if (m_surfaceFlowEnabled && griddingEnabled) {
        // Update the 3-frame history with current frame data
        m_tofProcessing.UpdateSurfaceFlowHistory(m_pointCloudGrid);
        
        // Apply surface flow filtering if we have enough history
        if (m_tofProcessing.GetSurfaceFlowFrameCount() >= 2) {
            m_tofProcessing.ApplySurfaceFlowFilter(
                m_pointCloudGrid,
                m_surfaceFlowRadius,
                m_surfaceFlowThreshold,
                m_vipresGridSize
            );
            
            // Rebuild processedData from filtered grid
            processedData.clear();
            pointGridKeys.clear();
            processedData.reserve(m_pointCloudGrid.size());
            pointGridKeys.reserve(m_pointCloudGrid.size());
            
            for (const auto& pair : m_pointCloudGrid) {
                pointGridKeys.push_back(pair.first);
                processedData.push_back(pair.second);
            }
        }
    }
    
    // ========================================================================
    // TOF PIPELINE END
    // ========================================================================
    // The processed point cloud (with colors, weights, segment IDs) is now ready.
    // Filter out excluded segment IDs before passing to rendering and VIPRES.
    // ========================================================================
    
    // Final step: Filter by segment ID inclusion and copy to point cloud buffer
    m_pointCloud[m_pointCloudWriteBuffer].clear();
    m_pointCloud[m_pointCloudWriteBuffer].reserve(processedData.size());
    
    for (const auto& point : processedData) {
        // Check if this segment ID should be included
        if (m_segmentationIdEnabled) {
            auto it = m_segmentIdIncluded.find(point.segmentId);
            // If segment ID is explicitly disabled, skip this point
            if (it != m_segmentIdIncluded.end() && !it->second) {
                continue;
            }
        }
        m_pointCloud[m_pointCloudWriteBuffer].push_back(point);
    }
    
    // Also filter the grid map for VIPRES
    if (m_segmentationIdEnabled) {
        for (auto it = m_pointCloudGrid.begin(); it != m_pointCloudGrid.end();) {
            auto segIt = m_segmentIdIncluded.find(it->second.segmentId);
            if (segIt != m_segmentIdIncluded.end() && !segIt->second) {
                it = m_pointCloudGrid.erase(it);
            } else {
                ++it;
            }
        }
    }
    
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
    Algorithms::ComputeLowPassFilterCoefficients(cutoffHz, sampleRate, m_lowPassFilterCoefficients);
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


// Smart Occupancy: Apply weight algorithms directly to grid map
// NOTE: All occupancy algorithms ONLY operate on gridded data - gridding must be enabled
void CameraSystemEditor::ApplySmartOccupancyWeights(std::map<GridKey, PointCloudPoint>& pointCloudGrid) {
    // Require gridding to be enabled - occupancy algorithms only work on gridded data
    if (m_vipresGridMode == TOFGridMode::Disabled) {
        LOG_WARNING("CameraSystem", "ApplySmartOccupancyWeights: Gridding not enabled, skipping");
        return;  // Do not run occupancy algorithms if gridding is disabled
    }
    
    if (pointCloudGrid.empty()) {
        LOG_WARNING("CameraSystem", "ApplySmartOccupancyWeights: pointCloudGrid is empty, no data to process");
        return;  // No data to process
    }
    
    LOG_INFO_FMT("CameraSystem", "ApplySmartOccupancyWeights: Processing %zu grid points", pointCloudGrid.size());
    
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
    params.eviction2AppearanceThreshold = m_smartOccupancyEviction2AppearanceThreshold;
    params.eviction2RemovalThreshold = m_smartOccupancyEviction2RemovalThreshold;
    
    // Call the occupancy algorithms module
    GridBased::ApplyOccupancyAlgorithms(pointCloudGrid, params, m_occupancyState);
}

// Segmentation ID assignment: Uses object-based IDs assigned during TOF capture
// Points are assigned to the nearest scene object during capture
// For gridded data, uses majority voting to determine the grid cell's segment ID
// Label positions are calculated as (min_x, min_y, max_z) for each segment
void CameraSystemEditor::AssignSegmentationIds(std::map<GridKey, PointCloudPoint>& pointCloudGrid) {
    if (pointCloudGrid.empty()) {
        m_activeSegmentIds.clear();
        m_segmentLabelPos.clear();
        return;
    }
    
    // Track bounds for each segment: min_x, min_y, max_z
    std::map<int32_t, float> minX, minY, maxZ;
    
    m_activeSegmentIds.clear();
    
    for (const auto& pair : pointCloudGrid) {
        int32_t segId = pair.second.segmentId;
        const glm::vec3& pos = pair.second.position;
        
        // Skip unassigned points
        if (segId < 0) continue;
        
        m_activeSegmentIds.insert(segId);
        
        // Initialize or update bounds
        if (minX.count(segId) == 0) {
            minX[segId] = pos.x;
            minY[segId] = pos.y;
            maxZ[segId] = pos.z;
        } else {
            if (pos.x < minX[segId]) minX[segId] = pos.x;
            if (pos.y < minY[segId]) minY[segId] = pos.y;
            if (pos.z > maxZ[segId]) maxZ[segId] = pos.z;
        }
    }
    
    // Build label positions: (min_x, min_y, max_z)
    m_segmentLabelPos.clear();
    for (int32_t segId : m_activeSegmentIds) {
        m_segmentLabelPos[segId] = glm::vec3(minX[segId], minY[segId], maxZ[segId]);
    }
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
    
    // Filter points by weight threshold and apply render color mode before rendering
    std::vector<PointCloudPoint> filteredPoints;
    filteredPoints.reserve(m_pointCloud[m_pointCloudReadBuffer].size());
    for (const auto& point : m_pointCloud[m_pointCloudReadBuffer]) {
        if (point.weight >= m_weightThreshold) {
            PointCloudPoint renderPoint = point;
            
            // Apply render color mode (stored color is preserved in original point)
            switch (m_tofRenderColorMode) {
                case TOFRenderColorMode::BaseColor:
                    // White/base color for all points
                    renderPoint.color = glm::vec3(1.0f);
                    break;
                case TOFRenderColorMode::ColorByCamera: {
                    // Color code by camera source using predefined distinct colors
                    // These are vibrant, easily distinguishable colors
                    static const glm::vec3 cameraColors[] = {
                        glm::vec3(1.0f, 0.2f, 0.2f),   // Red
                        glm::vec3(0.2f, 1.0f, 0.2f),   // Green
                        glm::vec3(0.2f, 0.4f, 1.0f),   // Blue
                        glm::vec3(1.0f, 1.0f, 0.2f),   // Yellow
                        glm::vec3(1.0f, 0.2f, 1.0f),   // Magenta
                        glm::vec3(0.2f, 1.0f, 1.0f),   // Cyan
                        glm::vec3(1.0f, 0.6f, 0.2f),   // Orange
                        glm::vec3(0.6f, 0.2f, 1.0f),   // Purple
                        glm::vec3(0.2f, 1.0f, 0.6f),   // Teal
                        glm::vec3(1.0f, 0.4f, 0.6f),   // Pink
                    };
                    static const int numColors = sizeof(cameraColors) / sizeof(cameraColors[0]);
                    int colorIdx = point.cameraId % numColors;
                    if (colorIdx < 0) colorIdx = 0;
                    renderPoint.color = cameraColors[colorIdx];
                    break;
                }
                case TOFRenderColorMode::ColorByObject:
                    // Use original object color from image data
                    // Colors are captured when any of these are enabled:
                    //   - Extract Color from TOF
                    //   - Compute Color (VIPRES)
                    //   - Color by Object mode (automatically enables color capture)
                    // So we always keep point.color as-is in this mode
                    break;
            }
            
            filteredPoints.push_back(renderPoint);
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
    // ========================================================================
    // VIPRES PIPELINE
    // ========================================================================
    // The VIPRES pipeline is responsible for:
    //   1. Generating a mesh from the TOF point cloud data
    //   2. Applying colors from the TOF point cloud to the mesh vertices
    // 
    // Input: TOF point cloud (from m_pointCloud[m_pointCloudReadBuffer])
    //        - Contains positions, colors (from image), weights, segment IDs
    // Output: Mesh vertices and indices (m_remeshVertices, m_remeshIndices)
    // ========================================================================
    
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
    } else if (m_vipresRemeshAlgorithm == 7) {
        algorithm = Engine::Editors::CameraSystem::Algorithm::DensitySDF;
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
    params.densityVoxelSize = m_vipresRemeshDensityVoxelSize;
    params.densitySmoothRadius = m_vipresRemeshDensitySmoothRadius;
    params.densityIsoValue = m_vipresRemeshDensityIsoValue;
    params.surfaceOnly = m_vipresRemeshSurfaceOnly;
    params.surfaceCameraConsistent = m_vipresRemeshSurfaceCameraConsistent;
    params.surfaceNormalThreshold = m_vipresRemeshSurfaceNormalThreshold;
    params.useWeights = m_smartOccupancyEnabled && (m_vipresGridMode != TOFGridMode::Disabled);  // Use weights if smart occupancy is enabled
    
    // Convert CameraSystemEditor types to Remeshing types
    // Note: Segment ID filtering is already done at the end of the TOF pipeline
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
        (m_vipresGridMode != TOFGridMode::Disabled),
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
    
    // Position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), 
                         reinterpret_cast<void*>(offsetof(MeshVertex, position)));
    
    // Normal attribute (location 2 - matches shader.vert)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), 
                         reinterpret_cast<void*>(offsetof(MeshVertex, normal)));
    
    // Vertex color attribute (location 5 - matches shader.vert's vertexColor)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(MeshVertex), 
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
    
    // Tell shader to use vertex colors instead of textures
    GLint useTexLoc = glGetUniformLocation(shaderID, "useTexture");
    if (useTexLoc != -1) glUniform1i(useTexLoc, 0);
    
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
    
    int pointLightCount = 0;
    int spotLightCount = 0;
    
    for (const auto& light : m_lights) {
        if (light.type == LightType::Point) pointLightCount++;
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
    
    // DEBUG: Render red spheres at segment ID label positions
    if (m_segmentationIdEnabled && !m_activeSegmentIds.empty()) {
        RenderDebugLabelMarkers();
    }
}

void CameraSystemEditor::RenderUI() {
    // Delegate to UI class
    m_ui.Render(*this);
    
    // Render segment ID labels in 3D space if enabled
    if (m_segmentationIdEnabled && !m_activeSegmentIds.empty()) {
        RenderSegmentIdLabels();
    }
}

// Render red markers at the world positions where segment ID labels appear (for visual debugging)
void CameraSystemEditor::RenderDebugLabelMarkers() {
    if (m_objects.empty() || m_activeSegmentIds.empty()) return;
    if (!m_meshShader || !m_meshShader->IsValid()) return;
    
    std::vector<glm::vec3> debugPoints;
    
    for (int32_t segId : m_activeSegmentIds) {
        if (segId < 0 || static_cast<size_t>(segId) >= m_objects.size()) continue;
        
        const auto& obj = m_objects[segId];
        if (!obj || !obj->visible) continue;
        
        // Build the same model matrix as ObjectRenderer uses
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, obj->position);
        modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(obj->rotation.z), glm::vec3(0.0f, 1.0f, 0.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(obj->rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(obj->rotation.y), glm::vec3(0.0f, 0.0f, 1.0f));
        modelMatrix = glm::scale(modelMatrix, obj->scale);
        
        // Get top of model in model space (Y is up in model space)
        glm::vec3 modelTopPos(0.0f, 0.0f, 0.0f);
        if (obj->model && obj->model->GetMeshCount() > 0) {
            float minX, maxX, minY, maxY, minZ, maxZ;
            obj->model->GetBounds(minX, maxX, minY, maxY, minZ, maxZ);
            modelTopPos.x = (minX + maxX) * 0.5f;
            modelTopPos.y = maxY;
            modelTopPos.z = (minZ + maxZ) * 0.5f;
        }
        
        // Transform to world space
        glm::vec4 worldPos4 = modelMatrix * glm::vec4(modelTopPos, 1.0f);
        glm::vec3 worldPos = glm::vec3(worldPos4);
        worldPos.z += 2.0f;  // Offset above
        
        debugPoints.push_back(worldPos);
    }
    
    if (debugPoints.empty()) return;
    
    // Render debug points using a simple VAO/VBO approach
    static GLuint debugVAO = 0, debugVBO = 0;
    if (debugVAO == 0) {
        glGenVertexArrays(1, &debugVAO);
        glGenBuffers(1, &debugVBO);
    }
    
    // Prepare data: position + color (red)
    struct DebugVertex {
        glm::vec3 pos;
        glm::vec3 color;
    };
    std::vector<DebugVertex> vertices;
    for (const auto& p : debugPoints) {
        vertices.push_back({p, glm::vec3(1.0f, 0.0f, 0.0f)});  // Red
    }
    
    glBindVertexArray(debugVAO);
    glBindBuffer(GL_ARRAY_BUFFER, debugVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(DebugVertex), vertices.data(), GL_DYNAMIC_DRAW);
    
    // Position attribute (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (void*)0);
    // Color attribute (location 5 for vCol in shader)
    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, sizeof(DebugVertex), (void*)offsetof(DebugVertex, color));
    
    // Use mesh shader with identity model matrix
    m_meshShader->UseShader();
    glm::mat4 identity = glm::mat4(1.0f);
    GLint uniformModel = m_meshShader->GetModelLocation();
    if (uniformModel != -1) {
        glUniformMatrix4fv(uniformModel, 1, GL_FALSE, glm::value_ptr(identity));
    }
    
    glDisable(GL_DEPTH_TEST);
    glPointSize(15.0f);
    glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(vertices.size()));
    glPointSize(1.0f);
    glEnable(GL_DEPTH_TEST);
    
    glBindVertexArray(0);
}

// Render segment ID labels directly above each scene object
// Uses the same model matrix transformation as ObjectRenderer
void CameraSystemEditor::RenderSegmentIdLabels() {
    if (!m_glfwWindow || m_activeSegmentIds.empty() || m_objects.empty()) return;
    
    // Get window size - use glfwGetWindowSize for logical coordinates (Nuklear uses logical, not framebuffer pixels)
    // On Retina displays, framebuffer is 2x the window size
    int windowWidth, windowHeight;
    glfwGetWindowSize(m_glfwWindow, &windowWidth, &windowHeight);
    if (windowWidth <= 0 || windowHeight <= 0) return;
    
    // Also get framebuffer size to calculate scale factor
    // Note: framebuffer size may differ from window size on Retina displays
    // but Nuklear uses window coordinates, so we use windowWidth/windowHeight
    
    // Get Nuklear context
    struct nk_context* ctx = m_nuklearUI.GetContext();
    if (!ctx) return;
    
    for (int32_t segId : m_activeSegmentIds) {
        if (segId < 0 || static_cast<size_t>(segId) >= m_objects.size()) continue;
        
        const auto& obj = m_objects[segId];
        if (!obj || !obj->visible) continue;
        
        // Build model matrix (must match RenderDebugLabelMarkers and ObjectRenderer)
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, obj->position);
        modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(-90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(obj->rotation.z), glm::vec3(0.0f, 1.0f, 0.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(obj->rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
        modelMatrix = glm::rotate(modelMatrix, glm::radians(obj->rotation.y), glm::vec3(0.0f, 0.0f, 1.0f));
        modelMatrix = glm::scale(modelMatrix, obj->scale);
        
        // Get top of model in model space (Y is up in model space)
        glm::vec3 modelTopPos(0.0f, 0.0f, 0.0f);
        if (obj->model && obj->model->GetMeshCount() > 0) {
            float minX, maxX, minY, maxY, minZ, maxZ;
            obj->model->GetBounds(minX, maxX, minY, maxY, minZ, maxZ);
            modelTopPos.x = (minX + maxX) * 0.5f;
            modelTopPos.y = maxY;
            modelTopPos.z = (minZ + maxZ) * 0.5f;
        }
        
        // Transform model-space top position to world space
        glm::vec4 worldPos4 = modelMatrix * glm::vec4(modelTopPos, 1.0f);
        glm::vec3 worldPos = glm::vec3(worldPos4);
        worldPos.z += 2.0f;  // Offset above the object
        
        // Project to clip space
        glm::mat4 mvp = m_projection * m_view;
        glm::vec4 clipPos = mvp * glm::vec4(worldPos, 1.0f);
        
        // Skip if behind camera
        if (clipPos.w <= 0.01f) continue;
        
        // Perspective divide to NDC
        glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
        
        // Skip if outside view frustum
        if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f) {
            continue;
        }
        
        // Convert NDC to screen coordinates
        // NDC: x [-1,1] left-right, y [-1,1] bottom-top
        // Screen: x [0,width] left-right, y [0,height] top-bottom
        float screenX = (ndc.x + 1.0f) * 0.5f * static_cast<float>(windowWidth);
        float screenY = (1.0f - ndc.y) * 0.5f * static_cast<float>(windowHeight);  // Flip Y for screen coords
        
        // Clamp to screen bounds
        screenX = std::max(25.0f, std::min(screenX, static_cast<float>(windowWidth) - 50.0f));
        screenY = std::max(25.0f, std::min(screenY, static_cast<float>(windowHeight) - 50.0f));
        
        // Create label text
        char labelText[32];
        std::snprintf(labelText, sizeof(labelText), "[%d]", segId);
        
        // Create unique window name for each segment
        char windowName[64];
        std::snprintf(windowName, sizeof(windowName), "##SegID%d", segId);
        
        // Render as a small Nuklear window
        struct nk_rect labelRect = nk_rect(screenX - 20, screenY - 12, 45, 26);
        nk_style_push_color(ctx, &ctx->style.window.background, nk_rgba(0, 0, 0, 200));
        if (nk_begin(ctx, windowName, labelRect, 
                     NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_NO_INPUT | NK_WINDOW_BORDER)) {
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label_colored(ctx, labelText, NK_TEXT_CENTERED, nk_rgb(255, 255, 0));
        }
        nk_end(ctx);
        nk_style_pop_color(ctx);
    }
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
    m_safeParameters.extractColorEnabled = m_extractColorEnabled;
    m_safeParameters.tofRenderColorMode = m_tofRenderColorMode;
    m_safeParameters.vipresEnabled = m_vipresEnabled;
    m_safeParameters.vipresComputeColor = m_vipresComputeColor;
    m_safeParameters.vipresSamplingDensity = m_vipresSamplingDensity;
    m_safeParameters.vipresDepthLimit = m_vipresDepthLimit;
    m_safeParameters.vipresGriddingEnabled = (m_vipresGridMode != TOFGridMode::Disabled);
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
    m_extractColorEnabled = m_safeParameters.extractColorEnabled;
    m_tofRenderColorMode = m_safeParameters.tofRenderColorMode;
    m_vipresEnabled = m_safeParameters.vipresEnabled;
    m_vipresComputeColor = m_safeParameters.vipresComputeColor;
    m_vipresSamplingDensity = m_safeParameters.vipresSamplingDensity;
    m_vipresDepthLimit = m_safeParameters.vipresDepthLimit;
    // Note: vipresGriddingEnabled is now derived from m_vipresGridMode
    // If saved state was disabled and current mode is not Disabled, we keep current mode
    // If saved state was disabled and current mode is not Disabled, set to Disabled
    if (!m_safeParameters.vipresGriddingEnabled && m_vipresGridMode != TOFGridMode::Disabled) {
        m_vipresGridMode = TOFGridMode::Disabled;
    }
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
    // Clear filter state and TSDF when restoring
    Algorithms::ClearLowPassFilterState(m_lowPassFilterState);
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
    
    if (m_albedoShader) {
        m_albedoShader->ClearShader();
        delete m_albedoShader;
        m_albedoShader = nullptr;
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


