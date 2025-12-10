#pragma once

#include "SkeletonEditor.h"
#include "SkeletonDefinition.h"
#include "Animation.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <string>
#include <map>
#include <filesystem>

// Forward declarations for Nuklear
struct nk_glfw;
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
    class Mesh;
}
}
}

namespace Engine {
namespace Editors {
namespace Skeleton {

// Vertex weight data for a single vertex
struct VertexWeight {
    int boneID;
    float weight;
    
    VertexWeight(int id = -1, float w = 0.0f) : boneID(id), weight(w) {}
};

// Weight data for the entire mesh
class MeshWeights {
public:
    MeshWeights();
    
    // Set weight for a vertex
    void SetWeight(size_t vertexIndex, int boneID, float weight);
    
    // Get weights for a vertex (returns up to 4 bones)
    void GetWeights(size_t vertexIndex, std::vector<VertexWeight>& weights) const;
    
    // Get all weights for a vertex
    const std::vector<VertexWeight>& GetVertexWeights(size_t vertexIndex) const;
    
    // Clear all weights
    void Clear();
    
    // Get vertex count
    size_t GetVertexCount() const { return m_vertexWeights.size(); }
    
    // Resize to match vertex count
    void Resize(size_t vertexCount);
    
    // Normalize weights for a vertex (ensure sum = 1.0)
    void NormalizeVertexWeights(size_t vertexIndex);
    
    // Get total weight for a vertex
    float GetTotalWeight(size_t vertexIndex) const;

private:
    std::vector<std::vector<VertexWeight>> m_vertexWeights; // vertex index -> list of bone weights
};

// Visual bone representation
struct VisualBone {
    glm::vec3 startPos;  // Bone start (parent joint)
    glm::vec3 endPos;    // Bone end (this joint)
    glm::vec3 color;     // Display color
    bool selected;       // Is this bone selected?
    int boneID;         // ID in skeleton definition
    std::string name;   // Bone name
};

// Mouse picking result
struct PickResult {
    bool hit;
    glm::vec3 worldPos;      // World space hit position
    glm::vec3 normal;        // Surface normal
    size_t vertexIndex;      // Nearest vertex index
    float distance;          // Distance from camera
};

// Visual skeleton editor with OpenGL rendering
class VisualSkeletonEditor {
public:
    VisualSkeletonEditor();
    ~VisualSkeletonEditor();
    
    // Initialize editor (creates window, loads shaders, etc.)
    bool Initialize(int windowWidth = 1280, int windowHeight = 720);
    
    // Load mesh from file (OBJ, FBX, etc.)
    bool LoadMesh(const std::string& filePath);
    
    // Main editor loop
    void Run();
    
    // Save rig data (skeleton + weights) to file
    bool SaveRig(const std::string& filePath);
    
    // Load rig data from file
    bool LoadRig(const std::string& filePath);
    
    // Set output path for saving skeleton
    void SetOutputPath(const std::string& filePath) { m_outputPath = filePath; }
    const std::string& GetOutputPath() const { return m_outputPath; }
    
    enum class EditorMode {
        PlaceBones,
        SelectBones,
        PaintWeights,
        ViewOnly
    };
    
    void SetMode(EditorMode mode) { m_mode = mode; }
    EditorMode GetMode() const { return m_mode; }
    
    void LoadAnimationsFromDirectory(const std::string& directoryPath);
    bool LoadAnimationFile(const std::string& filePath, const std::string& animationName = "");
    void SetAnimationsLoaded(bool loaded) { m_animationsLoaded = loaded; }
    bool AreAnimationsLoaded() const { return m_animationsLoaded; }
    AnimationPlayer& GetAnimationPlayer() { return m_animationPlayer; }
    const AnimationPlayer& GetAnimationPlayer() const { return m_animationPlayer; }

private:
    // Window and rendering
    Platform::Window* m_window;
    Platform::Render::Shader* m_meshShader;
    Platform::Render::Shader* m_boneShader;
    Platform::Render::Model* m_mesh;
    
    // OpenGL buffers for rendering bones and grid
    GLuint m_boneVAO;
    GLuint m_boneVBO;
    GLuint m_gridVAO;
    GLuint m_gridVBO;
    bool m_buffersInitialized;
    
    // Camera
    glm::mat4 m_projection;
    glm::mat4 m_view;
    glm::vec3 m_cameraPos;
    glm::vec3 m_cameraTarget;
    float m_cameraYaw;
    float m_cameraPitch;
    float m_cameraDistance;
    
    // Editor state
    SkeletonEditor m_skeletonEditor;
    MeshWeights m_weights;
    EditorMode m_mode;
    
    // Animation system
    AnimationPlayer m_animationPlayer;
    std::string m_animationDirectory; // Directory containing animation files
    bool m_animationsLoaded;           // Whether animations have been loaded
    double m_lastFrameTime;           // Time of last frame (for delta time calculation)
    bool m_animateMesh;                // Whether to move mesh with skeleton
    bool m_animationPaused;            // Whether animation is manually paused (for time slider control)
    float m_manualAnimationTime;       // Manual time control for animation slider
    std::map<std::string, glm::vec3> m_bindPosePositions; // Store original bone positions (bind pose)
    std::map<std::string, glm::quat> m_bindPoseRotations; // Store original bone rotations (bind pose)
    glm::vec3 m_originalMeshOffset;   // Original mesh position offset
    
    // Visual bones
    std::vector<VisualBone> m_visualBones;
    int m_selectedBoneID;
    std::string m_parentBoneName; // For placing child bones
    
    // Bone ordering and tracking
    std::vector<std::string> m_expectedBoneOrder; // Expected bone names in order
    std::vector<bool> m_bonesPlaced; // Boolean list: true if bone at that index has been placed
    int m_nextBoneIndex; // Index of next bone to place (calculated from m_bonesPlaced)
    std::map<std::string, glm::vec3> m_removedBonePositions; // Store last position of removed bones for restoration
    
    // Mouse state
    bool m_mouseLeftDown;
    bool m_mouseRightDown;
    bool m_mouseMiddleDown;
    double m_lastMouseX;
    double m_lastMouseY;
    double m_scrollY; // Track scroll for zoom
    
    // Weight painting
    float m_paintRadius;
    float m_paintIntensity;
    int m_activePaintBoneID;
    
    // Output file path
    std::string m_outputPath;
    std::string m_meshFilePath;
    std::string m_skeletonFilePath;
    bool m_autoSave; // Auto-save skeleton when bones are added/removed
    bool m_showResetDialog; // Show reset confirmation dialog
    
    // Nuklear GUI
    struct nk_glfw* m_nuklear;
    struct nk_context* m_nkContext;
    bool m_nuklearInitialized;
    
    // File input buffers for Nuklear
    char m_meshFileInput[512];
    char m_skeletonFileInput[512];
    
    // Mesh data for raycasting (stored when mesh is loaded)
    struct Triangle {
        glm::vec3 v0, v1, v2;
        glm::vec3 normal;
    };
    std::vector<Triangle> m_meshTriangles;
    std::vector<Triangle> m_originalMeshTriangles; // Store original bind pose triangles
    std::vector<std::vector<GLfloat>> m_originalVertexData; // Store original vertex data per mesh
    std::vector<unsigned int> m_meshVertexCounts; // Vertex count per mesh
    bool m_meshDataLoaded;
    bool m_meshVisible;
    bool m_contoursVisible;
    float m_contourInterval; // Distance between contour slices
    float m_boneMarkerSize; // Size of bone placement marker sphere (in pixels)
    bool m_showBoneConnections; // Whether to show red lines connecting parent bones to children
    
    // Rendering
    void RenderMesh();
    void RenderGrid();  // Render a reference grid when no mesh is loaded
    void RenderBones();
    void RenderBone(const VisualBone& bone);
    void RenderBoneHandle(const glm::vec3& position, const glm::vec3& color, bool selected);
    void RenderContours(); // Render mesh contours by slicing
    
    // Camera control
    void UpdateCamera();
    void HandleMouseInput();
    void HandleKeyboardInput();
    
    // Mouse picking
    PickResult PickMesh(double mouseX, double mouseY);
    glm::vec3 ScreenToWorld(double mouseX, double mouseY, float depth);
    glm::vec3 Unproject(const glm::vec3& screenPos);
    
    // Bone operations
    void PlaceBoneAtPosition(const glm::vec3& worldPos);
    void RemoveBoneAtPosition(const glm::vec3& worldPos);
    void SelectBoneAtPosition(const glm::vec3& worldPos);
    void RemoveBoneAndChildren(const std::string& boneName); // Recursively remove bone and all descendants
    glm::vec3 CalculateBoneWorldPosition(const std::string& boneName) const; // Calculate world position from local transforms
    void ConvertWorldPositionsToLocal(); // Convert stored world positions to local positions after loading
    void StoreBindPose(); // Store current bone positions/rotations as bind pose
    void ResetToBindPose(); // Reset skeleton to bind pose (original positions)
    void ApplyAnimationAtTime(float time); // Apply animation at specific time (for slider control)
    void ApplyMeshAnimation(); // Apply bone transforms to mesh vertices (skinning)
    void ResetMeshToOriginal(); // Reset mesh to original position
    void StoreOriginalVertexData(const std::string& filePath); // Store original vertex data from mesh file
    void GenerateWeightsFromMesh(); // Automatically generate vertex weights based on bone proximity
    void UpdateMeshVertexBuffers(); // Update OpenGL vertex buffers with animated positions
    void UpdateVisualBones();
    void InitializeBoneOrder();
    std::string GetNextBoneName() const;
    std::string ExtractBaseBoneName(const std::string& displayName) const;
    void UpdateNextBoneIndexFromLoadedSkeleton();
    void UpdateNextBoneIndex(); // Update m_nextBoneIndex from m_bonesPlaced
    
    // Weight painting
    void PaintWeightsAtPosition(const glm::vec3& worldPos);
    void GetVerticesInRadius(const glm::vec3& center, float radius, std::vector<size_t>& vertexIndices);
    
    // UI rendering
    void RenderUI();
    void RenderBoneNumber(const glm::vec3& position, int number);
    void RenderBonePlacementSphere(const glm::vec3& position);
    float CalculateSphereRadius(const glm::vec3& position) const;
    void RenderNextBoneIndicator();
    void RenderRightPanel();
    void RenderResetDialog();
    void ResetAllBones();
    
    // Animation functions
    void UpdateAnimation(float deltaTime);
    void ApplyAnimationToSkeleton();
    bool InitializeNuklear();
    void ShutdownNuklear();
    
    // Helper: Load mesh data for raycasting (extract triangles)
    void LoadMeshDataForRaycasting(const std::string& filePath);
    
    // Helper: Ray-triangle intersection
    bool RayTriangleIntersect(const glm::vec3& rayOrigin, const glm::vec3& rayDir,
                              const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                              float& t, glm::vec3& hitPoint);
    
    // Helper: Calculate bind poses
    void CalculateBindPoses();
    
    // Helper: Create bone visualization
    void CreateBoneVisualization();
    
    // Shader creation
    bool CreateShaders();
    
    // Initialize OpenGL buffers for rendering
    bool InitializeBuffers();
    
    // Cleanup
    void Cleanup();
};

} // namespace Skeleton
} // namespace Editors
} // namespace Engine

