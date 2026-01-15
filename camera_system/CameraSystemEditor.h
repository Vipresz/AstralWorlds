#pragma once

#include "animation/Animation.h"
#include "helper/ConfigLoader.h"
#include "remesh/Remeshing.h"  // For GridKey and PointCloudPoint - must be included early
#include "tof/TOFProcessing.h"
#include "occupancy/OccupancyAlgorithms.h"
#include "algorithms/LowPassFilter.h"
#include "world/World.h"
#include "camera/CameraSystem.h"
#include "nuklear/NuklearUI.h"
#include "ui/CameraSystemUI.h"
#include "objects/ObjectManager.h"
#include "config/UIConfig.h"
#include "ouster/OusterStream.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <memory>
#include <filesystem>
#include <chrono>

// Forward declarations for Nuklear (now handled by NuklearUI)
struct nk_context;

// Forward declarations
namespace Engine {
namespace Platform {
    class Window;
}
namespace Platform {
namespace Render {
    class Model;
    class Shader;
}
}
}

namespace Engine {
namespace Editors {
namespace CameraSystem {

// TOF Render Color Mode - controls how TOF points are colored during rendering
// (the original image color is always stored with each point for data saving)
enum class TOFRenderColorMode {
    BaseColor = 0,      // White/base color for all points (default)
    ColorByCamera = 1,  // Color code by camera source
    ColorByObject = 2   // Use original object color from image data
};

// Forward declarations
class ObjectRenderer;

struct SceneObject {
    std::string name;
    std::string meshFilePath;
    std::string animationFilePath;
    Platform::Render::Model* model;
    AnimationPlayer* animationPlayer;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;
    bool isAnimating;
    bool visible;
    
    SceneObject() 
        : model(nullptr)
        , animationPlayer(nullptr)
        , position(0.0f, 0.0f, 0.0f)
        , rotation(0.0f, 0.0f, 0.0f)
        , scale(1.0f, 1.0f, 1.0f)
        , isAnimating(false)
        , visible(true)
    {    }
    
    ~SceneObject();
};

// Forward declaration
class CameraSystemUI;

class CameraSystemEditor {
    friend class CameraSystemUI;  // Allow UI class to access private members
    
public:
    CameraSystemEditor();
    ~CameraSystemEditor();
    
    // Initialize editor (creates window, loads shaders, etc.)
    bool Initialize(int windowWidth = 1280, int windowHeight = 720);
    
    // Main editor loop
    void Run();
    
    // Load configuration from JSON file
    bool LoadConfig(const std::string& configFile);
    
    // Reset all UI slider values to the values from ui_defaults.toml
    void ResetToConfigDefaults();
    
    // Add a new object to the scene
    bool AddObject(const std::string& meshFilePath, const std::string& animationFilePath = "", 
                   const std::string& name = "");
    
    // Load a saved Ouster packet file for debugging (enables Ouster mode and loads packet)
    bool LoadOusterPacket(const std::string& packetFile);
    
    // Load a PCAP file with metadata JSON (enables Ouster mode and loads PCAP)
    bool LoadOusterPcap(const std::string& pcapFile, const std::string& metadataFile);
    
    // Set PCAP file paths (called from main.cpp when command-line args are provided)
    void SetOusterPcapFiles(const std::string& pcapFile, const std::string& metadataFile);
    
    // Disable both Ouster and TOF data (for clean startup when no command line args)
    void DisableAllSensors();
    
    // Remove an object from the scene
    void RemoveObject(size_t index);
    
    // Get object count
    size_t GetObjectCount() const { return m_objects.size(); }
    
    // Get object by index
    SceneObject* GetObject(size_t index);
    const SceneObject* GetObject(size_t index) const;
    
    // Get point cloud weight data for statistics (returns weights from gridded data if available, otherwise from point cloud buffer)
    std::vector<float> GetPointCloudWeights() const;

private:
    // Window and rendering
    Platform::Window* m_window;
    GLFWwindow* m_glfwWindow;  // Cached GLFW window pointer for safe access
    Platform::Render::Shader* m_meshShader;
    Platform::Render::Shader* m_albedoShader;  // Simple shader for capturing unlit texture/vertex colors
    
    // World (grid and environment)
    World m_world;
    
    // Camera system
    CameraSystem m_cameraSystem;
    glm::mat4 m_projection;
    glm::mat4 m_view;
    std::vector<Camera> m_cameras;
    size_t m_pendingCameraSwitch;  // Deferred camera switch index (SIZE_MAX = no switch pending)
    
    // Free camera movement (for default/free camera only)
    glm::vec3 m_freeCameraPos;
    float m_freeCameraYaw;
    float m_freeCameraPitch;
    float m_movementSpeed;
    
    // Scene objects
    std::vector<std::unique_ptr<SceneObject>> m_objects;
    std::vector<LightConfig> m_lights;  // Lights from config
    
    // Mouse state (for free camera control)
    bool m_mouseRightDown;
    double m_lastMouseX;
    double m_lastMouseY;
    
    // Nuklear GUI (wrapped in NuklearUI class)
    NuklearUI m_nuklearUI;
    
    // UI rendering (moved to separate class)
    CameraSystemUI m_ui;
    
    
    // Animation state
    bool m_globalAnimationPlaying;
    float m_globalAnimationTime;
    
    // Configuration and path following
    bool m_configLoaded;
    float m_sceneTime;                                // Current scene time (0 to totalDuration)
    float m_totalDuration;                            // Total scene duration from config
    std::vector<std::vector<PathPoint>> m_objectPaths; // Legacy: time-based path data for each object
    std::vector<float> m_objectStartTimes;            // Start time for each object
    
    // Static camera animation data structure
    // 100 cameras, each with 300 frames (10 seconds at 30fps)
    // Each frame stores: tick (int), position (vec3), orientation (vec3) = 7 values
    static constexpr int MAX_CAMERAS = 100;
    static constexpr int MAX_FRAMES = 300;  // 10 seconds at 30fps
    
    // Frame-based object positions (similar to camera frames)
    static constexpr int MAX_OBJECTS = 20;
    struct ObjectFrame {
        int frame;
        glm::vec3 position;
    };
    ObjectFrame m_objectFrames[MAX_OBJECTS][MAX_FRAMES];  // [objectIndex][frameIndex]
    int m_objectFrameCounts[MAX_OBJECTS];                // Number of valid frames for each object
    bool m_objectsUseFrames[MAX_OBJECTS];                 // Whether object uses frame-based positions
    
    struct CameraFrame {
        int tick;                    // Game tick/frame at which this applies
        glm::vec3 position;          // Camera position
        glm::vec3 orientation;       // Camera orientation (direction vector)
    };
    
    CameraFrame m_cameraFrames[MAX_CAMERAS][MAX_FRAMES];  // Static array: [cameraIndex][frameIndex]
    int m_cameraFrameCounts[MAX_CAMERAS];                  // Number of valid frames for each camera
    int m_numCameras;                                       // Number of cameras actually loaded
    int m_totalFrames;                                      // Total number of frames (for looping)
    int m_currentFrame;                                     // Current frame number (0 to totalFrames-1)
    unsigned int m_frameProcessingCounter;                  // Counter that increments each time we process a frame (for noise refresh)
    bool m_isPlaying;                                       // Play/pause state for camera animation
    bool m_loopEnabled;                                     // Loop animation when reaching the end
    
    // Capture state
    bool m_captureEnabled;                                   // Capture toggle state
    bool m_tofDataEnabled;                                   // TOF/depth data capture toggle state
    bool m_extractColorEnabled;                              // Extract colors from images for TOF data
    TOFRenderColorMode m_tofRenderColorMode;                  // How to color TOF points during rendering
    std::string m_captureOutputDir;                         // Output directory for captures
    
    // UI panel visibility
    bool m_showObjectsPanel;                                 // Show/hide Objects panel
    
    // VIPRES (Visual Point Cloud Rendering) state
    bool m_vipresEnabled;                                   // VIPRES mode toggle
    bool m_vipresComputeColor;                              // Enable color collection from framebuffer
    int m_vipresSamplingDensity;                            // Sampling density (1-100, where 1=every pixel, 100=every 100th pixel)
    float m_vipresDepthLimit;                               // Maximum depth limit (0.001-100000 units, log scale)
    // Note: Gridding is now controlled by m_vipresGridMode (Disabled = gridding off)
    bool m_vipresRemeshEnabled;                             // Enable mesh generation from point cloud
    bool m_vipresAutoRecomputeMesh;                         // Auto-recompute mesh every frame
    int m_vipresRemeshAlgorithm;                           // Remeshing algorithm index (0=GriddedQuads, 1=NearestNeighbor, 2=Delaunay2D, 3=PoissonLike, 4=FCTM, 5=GeodesicTree)
    
    // Per-algorithm parameter storage
    struct AlgorithmParams {
        float maxDistance = 1.0f;
        int maxNeighbors = 20;
        int projectionPlane = 0;  // For Delaunay2D
        float delaunayThreshold = 0.1f;  // For Delaunay2D
        float poissonRadius = 0.1f;  // For Poisson-like
        int poissonDepth = 8;  // For Poisson-like
        float poissonDensity = 1.0f;  // For Poisson-like
        bool poissonSmoothNormals = true;  // For Poisson-like
        int fctmSteps = 10;  // For FCTM
        float fctmSliceThickness = 0.1f;  // For FCTM
        int geodesicKNeighbors = 10;  // For GeodesicTree: number of neighbors for kNN graph
        int geodesicNumSeeds = 0;  // For GeodesicTree: number of seed points (0 = auto-determine)
        float geodesicInteriorBias = 0.5f;  // For GeodesicTree: bias weight for staying inside point cloud (0.0-1.0)
    };
    AlgorithmParams m_remeshParams[7];  // One set of parameters for each algorithm (0-6)
    
    // Legacy members kept for backward compatibility - now just reference current algorithm's params
    float m_vipresRemeshMaxDistance;                       // Max distance for neighbor search (references current algorithm)
    int m_vipresRemeshMaxNeighbors;                         // Max neighbors to consider (references current algorithm)
    int m_vipresRemeshProjectionPlane;                     // Projection plane for Delaunay2D (references current algorithm)
    float m_vipresRemeshDelaunayThreshold;                 // Threshold for Delaunay2D (references current algorithm)
    float m_vipresRemeshPoissonRadius;                     // Radius for Poisson-like reconstruction (references current algorithm)
    int m_vipresRemeshPoissonDepth;                         // Depth for Poisson-like reconstruction (references current algorithm)
    float m_vipresRemeshPoissonDensity;                    // Density for Poisson-like reconstruction (references current algorithm)
    bool m_vipresRemeshPoissonSmoothNormals;               // Smooth normals for Poisson-like (references current algorithm)
    float m_vipresRemeshFCTMSliceThickness;                // Slice thickness for FCTM (steps auto-calculated from this)
    int m_vipresRemeshGeodesicKNeighbors;                  // K neighbors for GeodesicTree (references current algorithm)
    int m_vipresRemeshGeodesicNumSeeds;                    // Number of seed points for GeodesicTree (references current algorithm)
    float m_vipresRemeshGeodesicInteriorBias;              // Interior bias for GeodesicTree (references current algorithm)
    float m_vipresRemeshDensityVoxelSize;                  // Voxel size for DensitySDF algorithm
    float m_vipresRemeshDensitySmoothRadius;               // Gaussian smooth radius for DensitySDF algorithm
    float m_vipresRemeshDensityIsoValue;                   // Iso-value threshold for DensitySDF surface extraction
    bool m_vipresRemeshSurfaceOnly;                        // Enable surface-only connections (no through-object edges)
    bool m_vipresRemeshSurfaceCameraConsistent;            // Only connect points from the same camera
    float m_vipresRemeshSurfaceNormalThreshold;            // Min normal dot product for connection (0.0-1.0)
    bool m_vipresFreeCameraObjectEnabled;                    // Enable object rendering in free camera view
    bool m_vipresNoisyTofEnabled;                           // Enable noisy TOF data capture
    float m_vipresNoiseAmount;                              // Noise amount for TOF measurements (+- range)
    float m_vipresGridSize;                                 // Grid spacing for point cloud (0.01-1.0, log scale)
    float m_tofLowPassCutoff;                               // Low-pass filter cutoff frequency for TOF depth values (Hz)
    float m_tofSamplingRate;                                // TOF sampling rate (Hz) - independent from FPS (1-100 Hz, log scale)
    bool m_tofLowPassEnabled;                              // Low-pass filter enabled toggle
    bool m_tofTsdfEnabled;                                 // TSDF enabled toggle (mutually exclusive with low-pass)
    bool m_tofTsdfStoreColor;                             // TSDF store color toggle
    float m_tofTsdfTruncationDistance;                     // TSDF truncation distance (voxel units)
    float m_tofTsdfWeightDecay;                            // TSDF weight decay factor (0-1)
    
    // Smart Occupancy (4th step in TOF pipeline)
    bool m_smartOccupancyEnabled;                         // Smart occupancy enabled toggle
    bool m_smartOccupancyNeighborConsistency;              // Neighbor consistency weight algorithm enabled
    float m_smartOccupancyNeighborRadius;                 // Neighbor radius for consistency check (in grid units)
    float m_smartOccupancyResidualThreshold;              // Residual threshold (r0) for neighbor consistency
    bool m_smartOccupancyPersistence;                     // Persistence weight algorithm enabled
    float m_smartOccupancyPersistenceGamma;                // Persistence sharpness (γ)
    bool m_smartOccupancyVariance;                         // Variance-based weight algorithm enabled
    float m_smartOccupancyVarianceThreshold;               // Allowed jitter (σ0)
    bool m_smartOccupancyEviction;                         // Eviction-based weight algorithm enabled
    float m_smartOccupancyEvictionDecay;                   // Decay value used in (decay^count)/100 formula
    float m_smartOccupancyEvictionIncrement;               // Multiply weight by this when occupied
    float m_smartOccupancyEvictionMinStartValue;           // Minimum start value added to increment/decrement deltas
    bool m_smartOccupancyEviction2;                        // Eviction-based-2 weight algorithm enabled (consecutive frame persistence)
    int m_smartOccupancyEviction2AppearanceThreshold;      // N: consecutive frames point must be seen to be confirmed
    int m_smartOccupancyEviction2RemovalThreshold;         // M: consecutive frames unseen before point is removed
    float m_weightThreshold;                              // Minimum weight threshold for rendering (0.0-1.0)
    
    // Segmentation ID tracking (applies IDs to TOF data based on spatial overlap)
    bool m_segmentationIdEnabled;                          // Enable segmentation ID assignment
    std::set<int32_t> m_activeSegmentIds;                  // Currently active/detected segment IDs
    std::map<int32_t, bool> m_segmentIdIncluded;           // Whether each segment ID is included in final output (for VIPRES)
    std::map<int32_t, glm::vec3> m_segmentLabelPos;        // Label position for each segment (min_x, min_y, max_z)
    float m_segmentMatchRadius;                            // Max distance to match segments across frames
    
    // Surface flow filtering (temporal smoothing using 3-frame history)
    bool m_surfaceFlowEnabled;                             // Enable surface flow filtering
    float m_surfaceFlowRadius;                             // Neighborhood radius for local flow estimation
    float m_surfaceFlowThreshold;                          // Velocity deviation threshold for outlier rejection
    
    // Occupancy algorithm state (managed by GridBased namespace)
    GridBased::OccupancyState m_occupancyState;
    
    // Low-pass filter state (managed by Algorithms namespace)
    Algorithms::LowPassFilterStateContainer m_lowPassFilterState;
    Algorithms::LowPassFilterCoefficients m_lowPassFilterCoefficients;
    
    // TOF Processing - handles TSDF, low-pass filtering, and gridding
    TOFProcessing m_tofProcessing;
    
    // Ouster LiDAR stream
    std::unique_ptr<OusterStream> m_ousterStream;
    bool m_ousterEnabled;                    // Use Ouster LiDAR instead of simulated TOF
    bool m_ousterPacketLoaded;               // True if packet was loaded from file (don't start streaming)
    std::string m_ousterHostname;            // Ouster sensor IP address
    double m_lastOusterTimestamp;            // Timestamp of last received Ouster frame
    std::string m_ousterPcapFile;            // PCAP file path (from command-line or UI)
    std::string m_ousterMetadataFile;        // Metadata file path (from command-line or UI)
    
    // Object Manager - handles object loading and coloring
    ObjectManager m_objectManager;
    
    // Object Renderer - handles object rendering
    ObjectRenderer* m_objectRenderer;
    
    // Use TOFGridMode from TOFProcessing (no duplicate enum)
    TOFGridMode m_vipresGridMode;                           // How to handle multiple points in same grid cell
    std::vector<bool> m_vipresCameraEnabled;                // Track which cameras are enabled for VIPRES point cloud
    
    // Safe parameter storage for performance recovery
    struct SafeParameters {
        bool loopEnabled;
        bool captureEnabled;
        bool tofDataEnabled;
        bool extractColorEnabled;
        TOFRenderColorMode tofRenderColorMode;
        bool vipresEnabled;
        bool vipresComputeColor;
        int vipresSamplingDensity;
        float vipresDepthLimit;
        bool vipresGriddingEnabled;
        bool vipresRemeshEnabled;
        bool vipresAutoRecomputeMesh;
        int vipresRemeshAlgorithm;
        float vipresRemeshMaxDistance;
        int vipresRemeshMaxNeighbors;
        int vipresRemeshProjectionPlane;
        float vipresRemeshDelaunayThreshold;
        float vipresRemeshPoissonRadius;
        int vipresRemeshPoissonDepth;
        float vipresRemeshPoissonDensity;
        bool vipresRemeshPoissonSmoothNormals;
        float vipresRemeshFCTMSliceThickness;
        int vipresRemeshGeodesicKNeighbors;
        int vipresRemeshGeodesicNumSeeds;
        float vipresRemeshGeodesicInteriorBias;
        bool vipresFreeCameraObjectEnabled;
        bool vipresNoisyTofEnabled;
        float vipresNoiseAmount;
        float vipresGridSize;
        TOFGridMode vipresGridMode;
        std::vector<bool> vipresCameraEnabled;
        bool gridVisible;
        AlgorithmParams remeshParams[7];
        float tofLowPassCutoff;
        float tofSamplingRate;
        bool tofLowPassEnabled;
        bool tofTsdfEnabled;
        bool tofTsdfStoreColor;
        float tofTsdfTruncationDistance;
        float tofTsdfWeightDecay;
    };
    SafeParameters m_safeParameters;                        // Stored safe parameters (FPS > 1fps)
    bool m_hasSafeParameters;                                // Whether we have valid safe parameters stored
    int m_framesSinceStartup;                                // Counter to track frames since startup (for initial save)
    bool m_justRestoredParameters;                           // Flag to prevent saving immediately after restore
    // PointCloudPoint and GridKey are defined in remesh/Remeshing.h (included above)
    
    struct GridAccumulator {
        glm::vec3 positionSum;
        glm::vec3 colorSum;
        int count;
    };
    std::map<Engine::Editors::CameraSystem::GridKey, Engine::Editors::CameraSystem::PointCloudPoint> m_pointCloudGrid;
    std::map<Engine::Editors::CameraSystem::GridKey, GridAccumulator> m_gridAccumulators;
    
    // Double-buffering for filtered TOF data (depth threshold filtered)
    // Buffer 0: Currently being written to by TOF depth sampling (filtered by depth threshold)
    // Buffer 1: Stable buffer used for processing (gridding, TSDF, etc.)
    std::vector<Engine::Editors::CameraSystem::PointCloudPoint> m_filteredTofData[2];
    int m_filteredTofWriteBuffer;  // Which buffer is currently being written to (0 or 1)
    int m_filteredTofReadBuffer;   // Which buffer is currently being read from for processing (0 or 1)
    
    // Double-buffering for final processed point cloud data
    // Buffer 0: Currently being written to by processing (gridding, TSDF, etc.)
    // Buffer 1: Stable buffer used by rendering and VIPRES for remeshing
    std::vector<Engine::Editors::CameraSystem::PointCloudPoint> m_pointCloud[2];
    int m_pointCloudWriteBuffer;  // Which buffer is currently being written to (0 or 1)
    int m_pointCloudReadBuffer;   // Which buffer is currently being read from by VIPRES (0 or 1)
    
    GLuint m_pointCloudVAO;
    GLuint m_pointCloudVBO;
    bool m_pointCloudBuffersInitialized;
    
    struct MeshVertex {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec3 color;
    };
    std::vector<MeshVertex> m_remeshVertices;
    std::vector<unsigned int> m_remeshIndices;
    GLuint m_remeshVAO;
    GLuint m_remeshVBO;
    GLuint m_remeshEBO;
    bool m_remeshBuffersInitialized;
    bool m_remeshNeedsUpdate;
    int m_captureInterval;
    
    float m_currentFPS;
    float m_fpsUpdateTime;
    int m_fpsFrameCount;
    int m_lastCapturedFrame;
    std::vector<bool> m_capturedCamerasForFrame;
    size_t m_nextCameraToCapture;
    
    static constexpr float MAX_FPS = 60.0f;
    static constexpr float MIN_FRAME_TIME = 1.0f / MAX_FPS;
    
    // Rendering
    void RenderScene();
    void RenderGrid();  // Delegated to World
    void RenderObject(SceneObject* obj);  // Delegated to ObjectManager
    
    // Depth/TOF capture
    bool SaveDepthMap(const std::string& filepath, int width, int height, const Camera& cam);
    glm::vec3 UnprojectPixel(const glm::vec2& screenPos, float depth, const Camera& cam) const;
    glm::vec3 UnprojectPixelWithMatrices(const glm::vec2& screenPos, float linearDepth, 
                                         const glm::mat4& view, const glm::mat4& projection,
                                         int width, int height) const;
    
    // VIPRES point cloud
    void UpdatePointCloud();
    
    // Smart Occupancy: Apply weight algorithms to point cloud
    void ApplySmartOccupancyWeights(std::map<GridKey, PointCloudPoint>& pointCloudGrid);
    void RenderPointCloud();
    void GenerateRemesh();                                  // Generate mesh from point cloud
    void RenderRemesh();                                    // Render the remeshed mesh
    
    // Segmentation ID assignment: Apply IDs to TOF data based on spatial overlap
    void AssignSegmentationIds(std::map<GridKey, PointCloudPoint>& pointCloudGrid);
    void RenderSegmentIdLabels();                           // Render ID numbers above detected segments
    void RenderDebugLabelMarkers();                         // DEBUG: Render red markers at label positions
    
    // Low-pass filter for TOF point positions (applied after conversion to world coordinates)
    // Uses algorithms/LowPassFilter.h - filter is applied via Algorithms::ApplyWeightBasedLowPassFilter
    void ComputeLowPassFilterCoefficients(float cutoffHz, float sampleRate);
    
    // TSDF (Truncated Signed Distance Function) for point cloud smoothing
    // Delegated to TOFProcessing class
    void IntegrateTSDF(const Engine::Editors::CameraSystem::GridKey& key, const glm::vec3& pointPos, const glm::vec3& cameraPos, const glm::vec3& color = glm::vec3(0.0f));
    void ExtractTSDFPointCloud(std::vector<Engine::Editors::CameraSystem::PointCloudPoint>& output);
    void ClearTSDF();
    void CleanupTSDF(float minWeightThreshold = 0.05f, float weightDecay = 0.95f);
    
    // Camera control (delegated to CameraSystem)
    void UpdateCamera();
    void UpdateFreeCamera(float deltaTime);
    void HandleMouseInput();
    void HandleKeyboardInput();
    void SwitchCamera(size_t cameraIndex);
    void AddCamera(const std::string& name, const glm::vec3& position, const glm::vec3& target, bool canMove = false);
    void InitializeDefaultCameras();
    void InitializeCamerasFromConfig();  // Initialize cameras based on config
    
    // UI rendering (minimal - just camera switching)
    void RenderUI();
    
    // Shader creation
    bool CreateShaders();
    
    // Initialize OpenGL buffers (delegated to World)
    bool InitializeBuffers();
    
    // Nuklear initialization (delegated to NuklearUI)
    bool InitializeNuklear();
    void ShutdownNuklear();
    
    // Cleanup
    void Cleanup();
    
    // Animation helpers
    void UpdateAnimations(float deltaTime);
    
    // Path following
    void UpdatePathFollowing(float deltaTime);
    glm::vec3 InterpolatePath(const std::vector<PathPoint>& path, float currentTime);
    
    // Algorithm parameter management
    void LoadAlgorithmParams(int algorithmIndex);
    void SaveAlgorithmParams(int algorithmIndex);
    float GetObjectLocalTime(size_t objectIndex) const;
    
    // Animated camera updates
    void UpdateAnimatedCameras();
    
    // Safe parameter management
    void SaveSafeParameters();                              // Save current parameters if FPS > 1fps
    void RestoreSafeParameters();                           // Restore safe parameters when FPS < 1fps
    void CheckAndRestoreIfNeeded();                         // Check FPS and restore if needed
    
    // Screenshot capture
    glm::mat4 CalculateCameraView(const Camera& cam) const;
    void CaptureScreenshots();
    bool SaveScreenshot(const std::string& filepath, int width, int height);
    
    // Apply rainbow colors to loaded meshes
    void ApplyRainbowColorsToObjects();
    
    // LoadConfig helper functions (editor-specific state management)
    void ResetFrameArrays();
    
    // Run() helper functions
    void UpdateFPSAndSafeParameters(float deltaTime);
    void AdvanceFrame(float deltaTime, float maxFrameTime);
    bool CanAdvanceFrameWithCapture() const;
    void ProcessPendingCameraSwitch();
    void HandleInputSafely();
    void UpdateSceneSafely(float updateInterval);
    void RenderFrameSafely();
    void SleepForFrameTiming(float maxFrameTime, const std::chrono::high_resolution_clock::time_point& lastFrameTime);
};

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

