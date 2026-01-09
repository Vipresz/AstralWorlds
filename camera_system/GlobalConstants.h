#pragma once

/**
 * Global Constants for Camera System
 * 
 * This file contains global constants used throughout the camera system,
 * including camera speeds, grid configuration, and other shared values.
 */

namespace Engine {
namespace Editors {
namespace CameraSystem {
namespace Constants {

// ============================================================================
// Camera Movement Constants
// ============================================================================

/// Default camera movement speed (units per second)
constexpr float CAMERA_MOVEMENT_SPEED = 150.0f;

// ============================================================================
// Grid Constants
// ============================================================================
// Note: TOF/VIPRES grid size (m_vipresGridSize) is configurable via UI slider,
// not defined here as a constant.

/// Grid spacing for world grid rendering (units between grid lines)
constexpr float WORLD_GRID_SPACING = 50.0f;

/// Number of grid cells/squares (20x20 = 400 squares total)
constexpr int GRID_CELL_COUNT = 20;

/// Total grid size = GRID_CELL_COUNT * WORLD_GRID_SPACING
constexpr float WORLD_GRID_SIZE = GRID_CELL_COUNT * WORLD_GRID_SPACING;

// ============================================================================
// TOF/VIPRES Slider Defaults and Limits
// ============================================================================

/// Sampling Density (pixels between samples)
constexpr int SAMPLING_DENSITY_DEFAULT = 5;
constexpr int SAMPLING_DENSITY_MIN = 1;
constexpr int SAMPLING_DENSITY_MAX = 25;

/// Depth Limit (units)
constexpr float DEPTH_LIMIT_DEFAULT = 100000.0f;
constexpr float DEPTH_LIMIT_MIN = 0.001f;
constexpr float DEPTH_LIMIT_MAX = 100000.0f;

/// Far Plane (units) - used for projection matrix and depth buffer calculations
constexpr float FAR_PLANE = 100000.0f;

/// Grid Size (units)
constexpr float GRID_SIZE_DEFAULT = 0.1f;
constexpr float GRID_SIZE_MIN = 0.1f;
constexpr float GRID_SIZE_MAX = 10.0f;

/// TOF Sampling Rate (Hz)
constexpr float TOF_SAMPLING_RATE_DEFAULT = 20.0f;
constexpr float TOF_SAMPLING_RATE_MIN = 1.0f;
constexpr float TOF_SAMPLING_RATE_MAX = 60.0f;

/// TSDF Truncation Distance (voxel units)
constexpr float TSDF_TRUNCATION_DISTANCE_DEFAULT = 3.0f;
constexpr float TSDF_TRUNCATION_DISTANCE_MIN = 0.5f;
constexpr float TSDF_TRUNCATION_DISTANCE_MAX = 10.0f;

/// TSDF Weight Decay
constexpr float TSDF_WEIGHT_DECAY_DEFAULT = 0.95f;
constexpr float TSDF_WEIGHT_DECAY_MIN = 0.0f;
constexpr float TSDF_WEIGHT_DECAY_MAX = 1.0f;

/// Low-pass Filter Cutoff (Hz)
constexpr float LOW_PASS_CUTOFF_DEFAULT = 10.0f;
constexpr float LOW_PASS_CUTOFF_MIN = 0.0f;
constexpr float LOW_PASS_CUTOFF_MAX = 30.0f;

/// Noise Amount (units)
constexpr float NOISE_AMOUNT_DEFAULT = 1.0f;
constexpr float NOISE_AMOUNT_MIN = 0.1f;
constexpr float NOISE_AMOUNT_MAX = 10.0f;

// ============================================================================
// Remeshing Slider Defaults and Limits
// ============================================================================

/// Remesh Max Distance (units)
constexpr float REMESH_MAX_DISTANCE_DEFAULT = 1.0f;
constexpr float REMESH_MAX_DISTANCE_MIN = 1.0f;
constexpr float REMESH_MAX_DISTANCE_MAX = 100.0f;

/// Remesh Max Neighbors
constexpr int REMESH_MAX_NEIGHBORS_DEFAULT = 20;
constexpr int REMESH_MAX_NEIGHBORS_MIN = 5;
constexpr int REMESH_MAX_NEIGHBORS_MAX = 50;

/// Remesh Delaunay Threshold
constexpr float REMESH_DELAUNAY_THRESHOLD_DEFAULT = 1.0f;
constexpr float REMESH_DELAUNAY_THRESHOLD_MIN = 1.0f;
constexpr float REMESH_DELAUNAY_THRESHOLD_MAX = 100.0f;

/// Remesh Poisson Radius
constexpr float REMESH_POISSON_RADIUS_DEFAULT = 1.0f;
constexpr float REMESH_POISSON_RADIUS_MIN = 1.0f;
constexpr float REMESH_POISSON_RADIUS_MAX = 100.0f;

/// Remesh Poisson Depth
constexpr int REMESH_POISSON_DEPTH_DEFAULT = 8;
constexpr int REMESH_POISSON_DEPTH_MIN = 4;
constexpr int REMESH_POISSON_DEPTH_MAX = 12;

/// Remesh Poisson Density
constexpr float REMESH_POISSON_DENSITY_DEFAULT = 1.0f;
constexpr float REMESH_POISSON_DENSITY_MIN = 0.1f;
constexpr float REMESH_POISSON_DENSITY_MAX = 5.0f;

/// Remesh FCTM Slice Thickness (steps are auto-calculated from this and bounding box size)
constexpr float REMESH_FCTM_SLICE_THICKNESS_DEFAULT = 0.1f;
constexpr float REMESH_FCTM_SLICE_THICKNESS_MIN = 0.01f;
constexpr float REMESH_FCTM_SLICE_THICKNESS_MAX = 10.0f;

/// Remesh FCTM Max Clusters (now used as bandwidth for mean-shift)
constexpr int REMESH_FCTM_MAX_CLUSTERS_DEFAULT = 0;  // 0 = auto-determine
constexpr int REMESH_FCTM_MAX_CLUSTERS_MIN = 0;
constexpr int REMESH_FCTM_MAX_CLUSTERS_MAX = 100;

/// Remesh GeodesicTree K Neighbors (lowered for performance)
constexpr int REMESH_GEODESIC_K_NEIGHBORS_DEFAULT = 6;
constexpr int REMESH_GEODESIC_K_NEIGHBORS_MIN = 5;
constexpr int REMESH_GEODESIC_K_NEIGHBORS_MAX = 15;  // Reduced max for performance

/// Remesh GeodesicTree Number of Seeds (0 = auto-determine)
constexpr int REMESH_GEODESIC_NUM_SEEDS_DEFAULT = 0;
constexpr int REMESH_GEODESIC_NUM_SEEDS_MIN = 0;
constexpr int REMESH_GEODESIC_NUM_SEEDS_MAX = 100;

/// Remesh GeodesicTree Interior Bias (0.0-1.0)
constexpr float REMESH_GEODESIC_INTERIOR_BIAS_DEFAULT = 0.5f;
constexpr float REMESH_GEODESIC_INTERIOR_BIAS_MIN = 0.0f;
constexpr float REMESH_GEODESIC_INTERIOR_BIAS_MAX = 1.0f;

/// Remesh Density Voxel Size (units)
constexpr float REMESH_DENSITY_VOXEL_SIZE_DEFAULT = 2.0f;

/// Remesh Density Smooth Radius (units)
constexpr float REMESH_DENSITY_SMOOTH_RADIUS_DEFAULT = 2.0f;

/// Remesh Density Iso Value (0.0-1.0)
constexpr float REMESH_DENSITY_ISO_VALUE_DEFAULT = 0.5f;

// ============================================================================
// Object Slider Defaults and Limits
// ============================================================================

/// Object Size (units) - default size for objects
constexpr float OBJECT_SIZE_DEFAULT = 200.0f;
constexpr float OBJECT_SIZE_MIN = 1.0f;
constexpr float OBJECT_SIZE_MAX = 500.0f;

// ============================================================================
// UI Layout Constants
// ============================================================================

/// Status bar height (pixels)
constexpr float UI_STATUS_BAR_HEIGHT = 30.0f;

/// Window spacing from status bar (pixels)
constexpr float UI_WINDOW_OFFSET_Y = 10.0f;

/// Camera Controls window
constexpr float UI_CAMERA_CONTROLS_X = 10.0f;
constexpr float UI_CAMERA_CONTROLS_WIDTH = 350.0f;
constexpr float UI_CAMERA_CONTROLS_HEIGHT = 400.0f;

/// TOF Controls window
constexpr float UI_TOF_CONTROLS_X = 680.0f;
constexpr float UI_TOF_CONTROLS_WIDTH = 300.0f;
constexpr float UI_TOF_CONTROLS_HEIGHT_BASE = 400.0f;
constexpr float UI_TOF_CONTROLS_HEIGHT_MAX = 800.0f;
constexpr float UI_TOF_CONTROLS_HEIGHT_PER_CAMERA = 25.0f;

/// VIPRES Controls window
constexpr float UI_VIPRES_CONTROLS_X = 370.0f;
constexpr float UI_VIPRES_CONTROLS_WIDTH = 300.0f;
constexpr float UI_VIPRES_CONTROLS_HEIGHT_MIN = 200.0f;
constexpr float UI_VIPRES_CONTROLS_HEIGHT_REMESH = 500.0f;

/// Objects panel window
constexpr float UI_OBJECTS_PANEL_X = 370.0f;
constexpr float UI_OBJECTS_PANEL_WIDTH = 300.0f;
constexpr float UI_OBJECTS_PANEL_HEIGHT = 200.0f;

/// Statistics panel window
constexpr float UI_STATISTICS_PANEL_X = 990.0f;  // Positioned to the right of TOF Controls
constexpr float UI_STATISTICS_PANEL_WIDTH = 400.0f;
constexpr float UI_STATISTICS_PANEL_HEIGHT = 350.0f;
constexpr float UI_HISTOGRAM_HEIGHT = 200.0f;

/// FPS column width in status bar
constexpr float UI_FPS_COLUMN_WIDTH = 100.0f;

/// Standard row heights
constexpr float UI_ROW_HEIGHT_SMALL = 18.0f;
constexpr float UI_ROW_HEIGHT_MEDIUM = 20.0f;
constexpr float UI_ROW_HEIGHT_STANDARD = 25.0f;
constexpr float UI_ROW_HEIGHT_BUTTON = 30.0f;

/// Spacing values
constexpr float UI_SPACING_SMALL = 3.0f;
constexpr float UI_SPACING_MEDIUM = 5.0f;
constexpr float UI_SPACING_LARGE = 10.0f;

// ============================================================================
// Smart Occupancy Algorithm Slider Ranges
// ============================================================================

/// Neighbor Consistency - Neighbor Radius
constexpr float OCCUPANCY_NEIGHBOR_RADIUS_MIN = 1.0f;
constexpr float OCCUPANCY_NEIGHBOR_RADIUS_MAX = 5.0f;
constexpr float OCCUPANCY_NEIGHBOR_RADIUS_STEP = 0.1f;

/// Neighbor Consistency - Residual Threshold (r0)
constexpr float OCCUPANCY_RESIDUAL_THRESHOLD_MIN = 0.1f;
constexpr float OCCUPANCY_RESIDUAL_THRESHOLD_MAX = 5.0f;
constexpr float OCCUPANCY_RESIDUAL_THRESHOLD_STEP = 0.1f;

/// Persistence - Gamma (γ)
constexpr float OCCUPANCY_PERSISTENCE_GAMMA_MIN = 1.0f;
constexpr float OCCUPANCY_PERSISTENCE_GAMMA_MAX = 6.0f;
constexpr float OCCUPANCY_PERSISTENCE_GAMMA_STEP = 0.1f;

/// Variance-based - Variance Threshold (σ0)
constexpr float OCCUPANCY_VARIANCE_THRESHOLD_MIN = 0.1f;
constexpr float OCCUPANCY_VARIANCE_THRESHOLD_MAX = 5.0f;
constexpr float OCCUPANCY_VARIANCE_THRESHOLD_STEP = 0.1f;

/// Eviction-based - Decay (a) and Increment (b)
constexpr float OCCUPANCY_EVICTION_DECAY_MIN = 0.0f;
constexpr float OCCUPANCY_EVICTION_DECAY_MAX = 3.0f;
constexpr float OCCUPANCY_EVICTION_DECAY_STEP = 0.01f;

constexpr float OCCUPANCY_EVICTION_INCREMENT_MIN = 0.0f;
constexpr float OCCUPANCY_EVICTION_INCREMENT_MAX = 3.0f;
constexpr float OCCUPANCY_EVICTION_INCREMENT_STEP = 0.01f;

/// Eviction-based - Min Start Value
constexpr float OCCUPANCY_EVICTION_MIN_START_MIN = 0.0f;
constexpr float OCCUPANCY_EVICTION_MIN_START_MAX = 1.0f;
constexpr float OCCUPANCY_EVICTION_MIN_START_STEP = 0.01f;

/// Eviction-based-2 - Appearance Threshold (N consecutive frames to confirm a point)
constexpr int OCCUPANCY_EVICTION2_APPEARANCE_THRESHOLD_MIN = 1;
constexpr int OCCUPANCY_EVICTION2_APPEARANCE_THRESHOLD_MAX = 30;

/// Eviction-based-2 - Removal Threshold (M consecutive frames unseen to remove a point)
constexpr int OCCUPANCY_EVICTION2_REMOVAL_THRESHOLD_MIN = 1;
constexpr int OCCUPANCY_EVICTION2_REMOVAL_THRESHOLD_MAX = 60;

/// Weight Threshold
constexpr float OCCUPANCY_WEIGHT_THRESHOLD_MIN = 0.0f;
constexpr float OCCUPANCY_WEIGHT_THRESHOLD_MAX = 1.0f;
constexpr float OCCUPANCY_WEIGHT_THRESHOLD_STEP = 0.01f;

} // namespace Constants
} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

