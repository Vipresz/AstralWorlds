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
constexpr float REMESH_MAX_DISTANCE_MIN = 0.01f;
constexpr float REMESH_MAX_DISTANCE_MAX = 10.0f;

/// Remesh Max Neighbors
constexpr int REMESH_MAX_NEIGHBORS_DEFAULT = 20;
constexpr int REMESH_MAX_NEIGHBORS_MIN = 5;
constexpr int REMESH_MAX_NEIGHBORS_MAX = 50;

/// Remesh Delaunay Threshold
constexpr float REMESH_DELAUNAY_THRESHOLD_DEFAULT = 0.1f;
constexpr float REMESH_DELAUNAY_THRESHOLD_MIN = 0.01f;
constexpr float REMESH_DELAUNAY_THRESHOLD_MAX = 1.0f;

/// Remesh Poisson Radius
constexpr float REMESH_POISSON_RADIUS_DEFAULT = 0.1f;
constexpr float REMESH_POISSON_RADIUS_MIN = 0.01f;
constexpr float REMESH_POISSON_RADIUS_MAX = 1.0f;

/// Remesh Poisson Depth
constexpr int REMESH_POISSON_DEPTH_DEFAULT = 8;
constexpr int REMESH_POISSON_DEPTH_MIN = 4;
constexpr int REMESH_POISSON_DEPTH_MAX = 12;

/// Remesh Poisson Density
constexpr float REMESH_POISSON_DENSITY_DEFAULT = 1.0f;
constexpr float REMESH_POISSON_DENSITY_MIN = 0.1f;
constexpr float REMESH_POISSON_DENSITY_MAX = 5.0f;

// ============================================================================
// Object Slider Defaults and Limits
// ============================================================================

/// Object Size (units) - default size for objects
constexpr float OBJECT_SIZE_DEFAULT = 200.0f;
constexpr float OBJECT_SIZE_MIN = 1.0f;
constexpr float OBJECT_SIZE_MAX = 500.0f;

} // namespace Constants
} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

