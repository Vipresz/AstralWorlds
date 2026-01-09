#pragma once

/**
 * UI Defaults for Camera System Editor
 * 
 * This file contains default values for all toggles, sliders, and buttons
 * in the Camera System Editor UI. These values are loaded at launch.
 * 
 * Note: If any default value exceeds the slider limits defined in GlobalConstants.h,
 * it will be clamped to the valid range during initialization.
 */

namespace Engine {
namespace Editors {
namespace CameraSystem {
namespace Defaults {

// ============================================================================
// Camera Controls - Toggles
// ============================================================================

/// Loop animation when reaching the end
constexpr bool LOOP_ENABLED = false;

/// Capture toggle state (screenshot/depth capture)
constexpr bool CAPTURE_ENABLED = false;

/// TOF/depth data capture toggle
constexpr bool TOF_DATA_ENABLED = true;

/// VIPRES (Visual Point Cloud Rendering) mode toggle
constexpr bool VIPRES_ENABLED = true;

/// Enable object rendering in free camera view
constexpr bool VIPRES_FREE_CAMERA_OBJECT_ENABLED = false;

/// World grid visibility
constexpr bool GRID_VISIBLE = false;

// ============================================================================
// TOF Controls - Toggles
// ============================================================================

/// Extract color from images toggle - when enabled, extracts colors from images for TOF data
constexpr bool EXTRACT_COLOR_ENABLED = false;

/// TOF Render Color Mode (0=BaseColor/white, 1=ColorByCamera, 2=ColorByObject)
/// BaseColor is the default - shows all TOF points in white
constexpr int TOF_RENDER_COLOR_MODE = 0;  // BaseColor

/// Enable grid-based point storage
constexpr bool VIPRES_GRIDDING_ENABLED = true;

/// Enable Low-Pass Filter for TOF depth values
constexpr bool TOF_LOW_PASS_ENABLED = false;

/// Enable TSDF (Truncated Signed Distance Function)
constexpr bool TOF_TSDF_ENABLED = false;

/// TSDF Store Color toggle
constexpr bool TOF_TSDF_STORE_COLOR = false;

/// Enable Smart Occupancy (4th step in TOF pipeline)
constexpr bool SMART_OCCUPANCY_ENABLED = false;

/// Smart Occupancy - Neighbor Consistency algorithm enabled
constexpr bool SMART_OCCUPANCY_NEIGHBOR_CONSISTENCY = false;

/// Smart Occupancy - Persistence algorithm enabled
constexpr bool SMART_OCCUPANCY_PERSISTENCE = false;

/// Smart Occupancy - Variance-based algorithm enabled
constexpr bool SMART_OCCUPANCY_VARIANCE = false;

/// Smart Occupancy - Eviction-based algorithm enabled
constexpr bool SMART_OCCUPANCY_EVICTION = false;

/// Smart Occupancy - Eviction-based-2 algorithm enabled
constexpr bool SMART_OCCUPANCY_EVICTION2 = false;

/// Enable Noisy TOF data capture
constexpr bool VIPRES_NOISY_TOF_ENABLED = false;

// ============================================================================
// TOF Controls - Sliders
// ============================================================================

/// Sampling density (pixels between samples, 1 = every pixel)
constexpr int VIPRES_SAMPLING_DENSITY = 20;

/// Maximum depth limit (units)
constexpr float VIPRES_DEPTH_LIMIT = 100000.0f;

/// TOF sampling rate (Hz)
constexpr float TOF_SAMPLING_RATE = 20.0f;

/// Grid size for point cloud (units)
constexpr float VIPRES_GRID_SIZE = 1.5f;

/// Grid mode (0=Average, 1=FirstCome, 2=GridValue)
constexpr int VIPRES_GRID_MODE = 0;

/// TSDF Truncation Distance (voxel units)
constexpr float TOF_TSDF_TRUNCATION_DISTANCE = 3.0f;

/// TSDF Weight Decay factor (0-1)
constexpr float TOF_TSDF_WEIGHT_DECAY = 0.95f;

/// Low-pass filter cutoff frequency (Hz)
constexpr float TOF_LOW_PASS_CUTOFF = 10.0f;

/// Smart Occupancy - Neighbor Radius (grid units)
constexpr float SMART_OCCUPANCY_NEIGHBOR_RADIUS = 2.0f;

/// Smart Occupancy - Residual Threshold (r0)
constexpr float SMART_OCCUPANCY_RESIDUAL_THRESHOLD = 1.0f;

/// Smart Occupancy - Persistence Gamma (γ)
constexpr float SMART_OCCUPANCY_PERSISTENCE_GAMMA = 3.0f;

/// Smart Occupancy - Variance Threshold (σ0)
constexpr float SMART_OCCUPANCY_VARIANCE_THRESHOLD = 1.0f;

/// Smart Occupancy - Eviction Decay (a)
constexpr float SMART_OCCUPANCY_EVICTION_DECAY = 0.5f;

/// Smart Occupancy - Eviction Increment (b)
constexpr float SMART_OCCUPANCY_EVICTION_INCREMENT = 0.5f;

/// Smart Occupancy - Eviction Min Start Value
constexpr float SMART_OCCUPANCY_EVICTION_MIN_START_VALUE = 0.1f;

/// Smart Occupancy - Eviction2 Appearance Threshold (N consecutive frames to confirm a point)
constexpr int SMART_OCCUPANCY_EVICTION2_APPEARANCE_THRESHOLD = 3;

/// Smart Occupancy - Eviction2 Removal Threshold (M consecutive frames unseen to remove a point)
constexpr int SMART_OCCUPANCY_EVICTION2_REMOVAL_THRESHOLD = 5;

/// Weight Threshold for rendering (0.0-1.0)
constexpr float WEIGHT_THRESHOLD = 0.0f;

/// Noise amount for TOF measurements (+/- range)
constexpr float VIPRES_NOISE_AMOUNT = 1.0f;

// ============================================================================
// VIPRES Controls - Toggles
// ============================================================================

/// Enable color collection from framebuffer
constexpr bool VIPRES_COMPUTE_COLOR = true;

/// Enable segmentation ID assignment
constexpr bool SEGMENTATION_ID_ENABLED = false;

/// Enable surface flow filtering (temporal smoothing using 3-frame history)
constexpr bool SURFACE_FLOW_ENABLED = false;

/// Surface flow neighborhood radius (units) - radius for local flow estimation
constexpr float SURFACE_FLOW_RADIUS = 50.0f;

/// Surface flow threshold - velocity deviation threshold for outlier rejection
constexpr float SURFACE_FLOW_THRESHOLD = 10.0f;

/// Enable mesh generation from point cloud
constexpr bool VIPRES_REMESH_ENABLED = false;

/// Auto-recompute mesh every frame
constexpr bool VIPRES_AUTO_RECOMPUTE_MESH = false;

/// Smooth normals for Poisson-like reconstruction
constexpr bool VIPRES_REMESH_POISSON_SMOOTH_NORMALS = true;

// ============================================================================
// VIPRES Controls - Sliders
// ============================================================================

/// Remeshing algorithm index (0=GriddedQuads, 1=NearestNeighbor, 2=Delaunay2D, 3=PoissonLike, 4=FCTM, 5=GeodesicTree, 6=GTree)
constexpr int VIPRES_REMESH_ALGORITHM = 0;

/// Max distance for neighbor search (units)
constexpr float VIPRES_REMESH_MAX_DISTANCE = 1.0f;

/// Max neighbors to consider
constexpr int VIPRES_REMESH_MAX_NEIGHBORS = 20;

/// Projection plane for Delaunay2D (0=XY, 1=XZ, 2=YZ, 3=BestFit)
constexpr int VIPRES_REMESH_PROJECTION_PLANE = 0;

/// Threshold for Delaunay2D
constexpr float VIPRES_REMESH_DELAUNAY_THRESHOLD = 1.0f;

/// Radius for Poisson-like reconstruction
constexpr float VIPRES_REMESH_POISSON_RADIUS = 1.0f;

/// Depth for Poisson-like reconstruction
constexpr int VIPRES_REMESH_POISSON_DEPTH = 8;

/// Density for Poisson-like reconstruction
constexpr float VIPRES_REMESH_POISSON_DENSITY = 1.0f;

/// Slice thickness for FCTM
constexpr float VIPRES_REMESH_FCTM_SLICE_THICKNESS = 0.1f;

/// K neighbors for GeodesicTree
constexpr int VIPRES_REMESH_GEODESIC_K_NEIGHBORS = 6;

/// Interior bias for GeodesicTree (0.0-1.0)
constexpr float VIPRES_REMESH_GEODESIC_INTERIOR_BIAS = 0.5f;

// ============================================================================
// Segmentation Settings
// ============================================================================

/// Max distance to match segments across frames
constexpr float SEGMENT_MATCH_RADIUS = 5.0f;

// ============================================================================
// Camera Settings
// ============================================================================

/// Default camera movement speed (units per second)
constexpr float CAMERA_MOVEMENT_SPEED = 150.0f;

/// Initial free camera yaw (degrees)
constexpr float FREE_CAMERA_YAW = -90.0f;

/// Initial free camera pitch (degrees)
constexpr float FREE_CAMERA_PITCH = 0.0f;

} // namespace Defaults
} // namespace CameraSystem
} // namespace Editors
} // namespace Engine


