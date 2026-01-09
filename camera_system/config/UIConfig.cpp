#include "UIConfig.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>

namespace Engine {
namespace Editors {
namespace CameraSystem {

UIConfig& UIConfig::Instance() {
    static UIConfig instance;
    return instance;
}

UIConfig::UIConfig() {
    SetDefaults();
}

void UIConfig::SetDefaults() {
    // Camera
    m_cameraMovementSpeed = 150.0f;
    m_cameraYaw = -90.0f;
    m_cameraPitch = 0.0f;
    
    // Grid
    m_worldGridSpacing = 50.0f;
    m_gridCellCount = 20;
    m_gridVisible = false;
    
    // Camera Controls - Toggles
    m_loopEnabled = false;
    m_captureEnabled = false;
    m_tofDataEnabled = true;
    m_vipresEnabled = true;
    m_vipresFreeObjectEnabled = false;
    
    // TOF Controls - Toggles
    m_extractColorEnabled = false;
    m_tofRenderColorMode = 0;
    m_griddingEnabled = true;
    m_lowPassEnabled = false;
    m_tsdfEnabled = false;
    m_tsdfStoreColor = false;
    m_noisyTofEnabled = false;
    
    // TOF Controls - Sliders
    m_samplingDensity = 20;
    m_depthLimit = 100000.0f;
    m_tofSamplingRate = 20.0f;
    m_gridSize = 1.5f;
    m_gridMode = 0;
    m_tsdfTruncationDistance = 3.0f;
    m_tsdfWeightDecay = 0.95f;
    m_lowPassCutoff = 10.0f;
    m_noiseAmount = 1.0f;
    
    // TOF Limits
    m_samplingDensityMin = 1; m_samplingDensityMax = 25;
    m_depthLimitMin = 0.001f; m_depthLimitMax = 100000.0f;
    m_tofSamplingRateMin = 1.0f; m_tofSamplingRateMax = 60.0f;
    m_gridSizeMin = 0.1f; m_gridSizeMax = 10.0f;
    m_tsdfTruncationDistanceMin = 0.5f; m_tsdfTruncationDistanceMax = 10.0f;
    m_tsdfWeightDecayMin = 0.0f; m_tsdfWeightDecayMax = 1.0f;
    m_lowPassCutoffMin = 0.0f; m_lowPassCutoffMax = 30.0f;
    m_noiseAmountMin = 0.1f; m_noiseAmountMax = 10.0f;
    
    // Occupancy
    m_occupancyEnabled = false;
    m_occupancyNeighborConsistency = false;
    m_occupancyPersistence = false;
    m_occupancyVariance = false;
    m_occupancyEviction = false;
    m_occupancyEviction2 = false;
    
    m_occupancyNeighborRadius = 2.0f;
    m_occupancyResidualThreshold = 1.0f;
    m_occupancyPersistenceGamma = 3.0f;
    m_occupancyVarianceThreshold = 1.0f;
    m_occupancyEvictionDecay = 0.5f;
    m_occupancyEvictionIncrement = 0.5f;
    m_occupancyEvictionMinStartValue = 0.1f;
    m_occupancyEviction2AppearThreshold = 3;
    m_occupancyEviction2RemovalThreshold = 5;
    m_weightThreshold = 0.0f;
    
    // Occupancy Limits
    m_occupancyNeighborRadiusMin = 1.0f; m_occupancyNeighborRadiusMax = 5.0f; m_occupancyNeighborRadiusStep = 0.1f;
    m_occupancyResidualThresholdMin = 0.1f; m_occupancyResidualThresholdMax = 5.0f; m_occupancyResidualThresholdStep = 0.1f;
    m_occupancyPersistenceGammaMin = 1.0f; m_occupancyPersistenceGammaMax = 6.0f; m_occupancyPersistenceGammaStep = 0.1f;
    m_occupancyVarianceThresholdMin = 0.1f; m_occupancyVarianceThresholdMax = 5.0f; m_occupancyVarianceThresholdStep = 0.1f;
    m_occupancyEvictionDecayMin = 0.0f; m_occupancyEvictionDecayMax = 3.0f; m_occupancyEvictionDecayStep = 0.01f;
    m_occupancyEvictionIncrementMin = 0.0f; m_occupancyEvictionIncrementMax = 3.0f; m_occupancyEvictionIncrementStep = 0.01f;
    m_occupancyEvictionMinStartMin = 0.0f; m_occupancyEvictionMinStartMax = 1.0f; m_occupancyEvictionMinStartStep = 0.01f;
    m_occupancyEviction2AppearThresholdMin = 1; m_occupancyEviction2AppearThresholdMax = 30;
    m_occupancyEviction2RemovalThresholdMin = 1; m_occupancyEviction2RemovalThresholdMax = 60;
    m_weightThresholdMin = 0.0f; m_weightThresholdMax = 1.0f; m_weightThresholdStep = 0.01f;
    
    // VIPRES
    m_vipresComputeColor = true;
    m_segmentationIdEnabled = false;
    m_surfaceFlowEnabled = false;
    m_remeshEnabled = false;
    m_autoRecomputeMesh = false;
    m_remeshPoissonSmoothNormals = true;
    
    m_surfaceFlowRadius = 50.0f;
    m_surfaceFlowThreshold = 10.0f;
    m_remeshAlgorithm = 0;
    m_remeshMaxDistance = 1.0f;
    m_remeshMaxNeighbors = 20;
    m_remeshProjectionPlane = 0;
    m_remeshDelaunayThreshold = 1.0f;
    m_remeshPoissonRadius = 1.0f;
    m_remeshPoissonDepth = 8;
    m_remeshPoissonDensity = 1.0f;
    m_remeshFctmSliceThickness = 0.1f;
    m_remeshGeodesicKNeighbors = 6;
    m_remeshGeodesicInteriorBias = 0.5f;
    m_segmentMatchRadius = 5.0f;
    
    // VIPRES Limits
    m_remeshMaxDistanceMin = 1.0f; m_remeshMaxDistanceMax = 100.0f;
    m_remeshMaxNeighborsMin = 5; m_remeshMaxNeighborsMax = 50;
    m_remeshDelaunayThresholdMin = 1.0f; m_remeshDelaunayThresholdMax = 100.0f;
    m_remeshPoissonRadiusMin = 1.0f; m_remeshPoissonRadiusMax = 100.0f;
    m_remeshPoissonDepthMin = 4; m_remeshPoissonDepthMax = 12;
    m_remeshPoissonDensityMin = 0.1f; m_remeshPoissonDensityMax = 5.0f;
    m_remeshFctmSliceThicknessMin = 0.01f; m_remeshFctmSliceThicknessMax = 10.0f;
    m_remeshGeodesicKNeighborsMin = 5; m_remeshGeodesicKNeighborsMax = 15;
    m_remeshGeodesicInteriorBiasMin = 0.0f; m_remeshGeodesicInteriorBiasMax = 1.0f;
    
    // DensitySDF
    m_densityVoxelSize = 0.5f;
    m_densitySmoothRadius = 0.5f;
    m_densityIsoValue = 0.3f;
    
    // Objects
    m_objectSizeDefault = 200.0f; m_objectSizeMin = 1.0f; m_objectSizeMax = 500.0f;
    
    // UI Layout
    m_statusBarHeight = 30.0f;
    m_windowOffsetY = 10.0f;
    m_fpsColumnWidth = 100.0f;
    m_rowHeightSmall = 18.0f; m_rowHeightMedium = 20.0f; m_rowHeightStandard = 25.0f; m_rowHeightButton = 30.0f;
    m_spacingSmall = 3.0f; m_spacingMedium = 5.0f; m_spacingLarge = 10.0f;
    
    m_cameraControlsX = 10.0f; m_cameraControlsWidth = 350.0f; m_cameraControlsHeight = 400.0f;
    m_tofControlsX = 680.0f; m_tofControlsWidth = 300.0f; m_tofControlsHeightBase = 400.0f; m_tofControlsHeightMax = 800.0f; m_tofControlsHeightPerCamera = 25.0f;
    m_vipresControlsX = 370.0f; m_vipresControlsWidth = 300.0f; m_vipresControlsHeightMin = 200.0f; m_vipresControlsHeightRemesh = 500.0f;
    m_objectsPanelX = 370.0f; m_objectsPanelWidth = 300.0f; m_objectsPanelHeight = 200.0f;
    m_statisticsPanelX = 990.0f; m_statisticsPanelWidth = 400.0f; m_statisticsPanelHeight = 350.0f; m_histogramHeight = 200.0f;
}

bool UIConfig::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "[UIConfig] Failed to open config file: " << filepath << std::endl;
        return false;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    m_configPath = filepath;
    
    if (!ParseToml(buffer.str())) {
        std::cerr << "[UIConfig] Failed to parse config file: " << filepath << std::endl;
        return false;
    }
    
    // Now load all values from parsed data
    
    // Camera
    m_cameraMovementSpeed = GetFloat("camera", "movement_speed", m_cameraMovementSpeed);
    m_cameraYaw = GetFloat("camera", "yaw", m_cameraYaw);
    m_cameraPitch = GetFloat("camera", "pitch", m_cameraPitch);
    
    // Grid
    m_worldGridSpacing = GetFloat("grid", "world_spacing", m_worldGridSpacing);
    m_gridCellCount = GetInt("grid", "cell_count", m_gridCellCount);
    m_gridVisible = GetBool("grid", "visible", m_gridVisible);
    
    // Camera Controls - Toggles
    m_loopEnabled = GetBool("camera_controls", "loop_enabled", m_loopEnabled);
    m_captureEnabled = GetBool("camera_controls", "capture_enabled", m_captureEnabled);
    m_tofDataEnabled = GetBool("camera_controls", "tof_data_enabled", m_tofDataEnabled);
    m_vipresEnabled = GetBool("camera_controls", "vipres_enabled", m_vipresEnabled);
    m_vipresFreeObjectEnabled = GetBool("camera_controls", "vipres_free_camera_object_enabled", m_vipresFreeObjectEnabled);
    
    // TOF Controls - Toggles
    m_extractColorEnabled = GetBool("tof", "extract_color_enabled", m_extractColorEnabled);
    m_tofRenderColorMode = GetInt("tof", "render_color_mode", m_tofRenderColorMode);
    m_griddingEnabled = GetBool("tof", "gridding_enabled", m_griddingEnabled);
    m_lowPassEnabled = GetBool("tof", "low_pass_enabled", m_lowPassEnabled);
    m_tsdfEnabled = GetBool("tof", "tsdf_enabled", m_tsdfEnabled);
    m_tsdfStoreColor = GetBool("tof", "tsdf_store_color", m_tsdfStoreColor);
    m_noisyTofEnabled = GetBool("tof", "noisy_tof_enabled", m_noisyTofEnabled);
    
    // TOF Controls - Sliders
    m_samplingDensity = GetInt("tof", "sampling_density", m_samplingDensity);
    m_depthLimit = GetFloat("tof", "depth_limit", m_depthLimit);
    m_tofSamplingRate = GetFloat("tof", "sampling_rate", m_tofSamplingRate);
    m_gridSize = GetFloat("tof", "grid_size", m_gridSize);
    m_gridMode = GetInt("tof", "grid_mode", m_gridMode);
    m_tsdfTruncationDistance = GetFloat("tof", "tsdf_truncation_distance", m_tsdfTruncationDistance);
    m_tsdfWeightDecay = GetFloat("tof", "tsdf_weight_decay", m_tsdfWeightDecay);
    m_lowPassCutoff = GetFloat("tof", "low_pass_cutoff", m_lowPassCutoff);
    m_noiseAmount = GetFloat("tof", "noise_amount", m_noiseAmount);
    
    // TOF Limits
    m_samplingDensityMin = GetInt("tof.limits", "sampling_density_min", m_samplingDensityMin);
    m_samplingDensityMax = GetInt("tof.limits", "sampling_density_max", m_samplingDensityMax);
    m_depthLimitMin = GetFloat("tof.limits", "depth_limit_min", m_depthLimitMin);
    m_depthLimitMax = GetFloat("tof.limits", "depth_limit_max", m_depthLimitMax);
    m_tofSamplingRateMin = GetFloat("tof.limits", "sampling_rate_min", m_tofSamplingRateMin);
    m_tofSamplingRateMax = GetFloat("tof.limits", "sampling_rate_max", m_tofSamplingRateMax);
    m_gridSizeMin = GetFloat("tof.limits", "grid_size_min", m_gridSizeMin);
    m_gridSizeMax = GetFloat("tof.limits", "grid_size_max", m_gridSizeMax);
    m_tsdfTruncationDistanceMin = GetFloat("tof.limits", "tsdf_truncation_distance_min", m_tsdfTruncationDistanceMin);
    m_tsdfTruncationDistanceMax = GetFloat("tof.limits", "tsdf_truncation_distance_max", m_tsdfTruncationDistanceMax);
    m_tsdfWeightDecayMin = GetFloat("tof.limits", "tsdf_weight_decay_min", m_tsdfWeightDecayMin);
    m_tsdfWeightDecayMax = GetFloat("tof.limits", "tsdf_weight_decay_max", m_tsdfWeightDecayMax);
    m_lowPassCutoffMin = GetFloat("tof.limits", "low_pass_cutoff_min", m_lowPassCutoffMin);
    m_lowPassCutoffMax = GetFloat("tof.limits", "low_pass_cutoff_max", m_lowPassCutoffMax);
    m_noiseAmountMin = GetFloat("tof.limits", "noise_amount_min", m_noiseAmountMin);
    m_noiseAmountMax = GetFloat("tof.limits", "noise_amount_max", m_noiseAmountMax);
    
    // Occupancy
    m_occupancyEnabled = GetBool("occupancy", "enabled", m_occupancyEnabled);
    m_occupancyNeighborConsistency = GetBool("occupancy", "neighbor_consistency", m_occupancyNeighborConsistency);
    m_occupancyPersistence = GetBool("occupancy", "persistence", m_occupancyPersistence);
    m_occupancyVariance = GetBool("occupancy", "variance", m_occupancyVariance);
    m_occupancyEviction = GetBool("occupancy", "eviction", m_occupancyEviction);
    m_occupancyEviction2 = GetBool("occupancy", "eviction2", m_occupancyEviction2);
    
    m_occupancyNeighborRadius = GetFloat("occupancy", "neighbor_radius", m_occupancyNeighborRadius);
    m_occupancyResidualThreshold = GetFloat("occupancy", "residual_threshold", m_occupancyResidualThreshold);
    m_occupancyPersistenceGamma = GetFloat("occupancy", "persistence_gamma", m_occupancyPersistenceGamma);
    m_occupancyVarianceThreshold = GetFloat("occupancy", "variance_threshold", m_occupancyVarianceThreshold);
    m_occupancyEvictionDecay = GetFloat("occupancy", "eviction_decay", m_occupancyEvictionDecay);
    m_occupancyEvictionIncrement = GetFloat("occupancy", "eviction_increment", m_occupancyEvictionIncrement);
    m_occupancyEvictionMinStartValue = GetFloat("occupancy", "eviction_min_start_value", m_occupancyEvictionMinStartValue);
    m_occupancyEviction2AppearThreshold = GetInt("occupancy", "eviction2_appearance_threshold", m_occupancyEviction2AppearThreshold);
    m_occupancyEviction2RemovalThreshold = GetInt("occupancy", "eviction2_removal_threshold", m_occupancyEviction2RemovalThreshold);
    m_weightThreshold = GetFloat("occupancy", "weight_threshold", m_weightThreshold);
    
    // Occupancy Limits
    m_occupancyNeighborRadiusMin = GetFloat("occupancy.limits", "neighbor_radius_min", m_occupancyNeighborRadiusMin);
    m_occupancyNeighborRadiusMax = GetFloat("occupancy.limits", "neighbor_radius_max", m_occupancyNeighborRadiusMax);
    m_occupancyNeighborRadiusStep = GetFloat("occupancy.limits", "neighbor_radius_step", m_occupancyNeighborRadiusStep);
    m_occupancyResidualThresholdMin = GetFloat("occupancy.limits", "residual_threshold_min", m_occupancyResidualThresholdMin);
    m_occupancyResidualThresholdMax = GetFloat("occupancy.limits", "residual_threshold_max", m_occupancyResidualThresholdMax);
    m_occupancyResidualThresholdStep = GetFloat("occupancy.limits", "residual_threshold_step", m_occupancyResidualThresholdStep);
    m_occupancyPersistenceGammaMin = GetFloat("occupancy.limits", "persistence_gamma_min", m_occupancyPersistenceGammaMin);
    m_occupancyPersistenceGammaMax = GetFloat("occupancy.limits", "persistence_gamma_max", m_occupancyPersistenceGammaMax);
    m_occupancyPersistenceGammaStep = GetFloat("occupancy.limits", "persistence_gamma_step", m_occupancyPersistenceGammaStep);
    m_occupancyVarianceThresholdMin = GetFloat("occupancy.limits", "variance_threshold_min", m_occupancyVarianceThresholdMin);
    m_occupancyVarianceThresholdMax = GetFloat("occupancy.limits", "variance_threshold_max", m_occupancyVarianceThresholdMax);
    m_occupancyVarianceThresholdStep = GetFloat("occupancy.limits", "variance_threshold_step", m_occupancyVarianceThresholdStep);
    m_occupancyEvictionDecayMin = GetFloat("occupancy.limits", "eviction_decay_min", m_occupancyEvictionDecayMin);
    m_occupancyEvictionDecayMax = GetFloat("occupancy.limits", "eviction_decay_max", m_occupancyEvictionDecayMax);
    m_occupancyEvictionDecayStep = GetFloat("occupancy.limits", "eviction_decay_step", m_occupancyEvictionDecayStep);
    m_occupancyEvictionIncrementMin = GetFloat("occupancy.limits", "eviction_increment_min", m_occupancyEvictionIncrementMin);
    m_occupancyEvictionIncrementMax = GetFloat("occupancy.limits", "eviction_increment_max", m_occupancyEvictionIncrementMax);
    m_occupancyEvictionIncrementStep = GetFloat("occupancy.limits", "eviction_increment_step", m_occupancyEvictionIncrementStep);
    m_occupancyEvictionMinStartMin = GetFloat("occupancy.limits", "eviction_min_start_min", m_occupancyEvictionMinStartMin);
    m_occupancyEvictionMinStartMax = GetFloat("occupancy.limits", "eviction_min_start_max", m_occupancyEvictionMinStartMax);
    m_occupancyEvictionMinStartStep = GetFloat("occupancy.limits", "eviction_min_start_step", m_occupancyEvictionMinStartStep);
    m_occupancyEviction2AppearThresholdMin = GetInt("occupancy.limits", "eviction2_appearance_threshold_min", m_occupancyEviction2AppearThresholdMin);
    m_occupancyEviction2AppearThresholdMax = GetInt("occupancy.limits", "eviction2_appearance_threshold_max", m_occupancyEviction2AppearThresholdMax);
    m_occupancyEviction2RemovalThresholdMin = GetInt("occupancy.limits", "eviction2_removal_threshold_min", m_occupancyEviction2RemovalThresholdMin);
    m_occupancyEviction2RemovalThresholdMax = GetInt("occupancy.limits", "eviction2_removal_threshold_max", m_occupancyEviction2RemovalThresholdMax);
    m_weightThresholdMin = GetFloat("occupancy.limits", "weight_threshold_min", m_weightThresholdMin);
    m_weightThresholdMax = GetFloat("occupancy.limits", "weight_threshold_max", m_weightThresholdMax);
    m_weightThresholdStep = GetFloat("occupancy.limits", "weight_threshold_step", m_weightThresholdStep);
    
    // VIPRES
    m_vipresComputeColor = GetBool("vipres", "compute_color", m_vipresComputeColor);
    m_segmentationIdEnabled = GetBool("vipres", "segmentation_id_enabled", m_segmentationIdEnabled);
    m_surfaceFlowEnabled = GetBool("vipres", "surface_flow_enabled", m_surfaceFlowEnabled);
    m_remeshEnabled = GetBool("vipres", "remesh_enabled", m_remeshEnabled);
    m_autoRecomputeMesh = GetBool("vipres", "auto_recompute_mesh", m_autoRecomputeMesh);
    m_remeshPoissonSmoothNormals = GetBool("vipres", "remesh_poisson_smooth_normals", m_remeshPoissonSmoothNormals);
    
    m_surfaceFlowRadius = GetFloat("vipres", "surface_flow_radius", m_surfaceFlowRadius);
    m_surfaceFlowThreshold = GetFloat("vipres", "surface_flow_threshold", m_surfaceFlowThreshold);
    m_remeshAlgorithm = GetInt("vipres", "remesh_algorithm", m_remeshAlgorithm);
    m_remeshMaxDistance = GetFloat("vipres", "remesh_max_distance", m_remeshMaxDistance);
    m_remeshMaxNeighbors = GetInt("vipres", "remesh_max_neighbors", m_remeshMaxNeighbors);
    m_remeshProjectionPlane = GetInt("vipres", "remesh_projection_plane", m_remeshProjectionPlane);
    m_remeshDelaunayThreshold = GetFloat("vipres", "remesh_delaunay_threshold", m_remeshDelaunayThreshold);
    m_remeshPoissonRadius = GetFloat("vipres", "remesh_poisson_radius", m_remeshPoissonRadius);
    m_remeshPoissonDepth = GetInt("vipres", "remesh_poisson_depth", m_remeshPoissonDepth);
    m_remeshPoissonDensity = GetFloat("vipres", "remesh_poisson_density", m_remeshPoissonDensity);
    m_remeshFctmSliceThickness = GetFloat("vipres", "remesh_fctm_slice_thickness", m_remeshFctmSliceThickness);
    m_remeshGeodesicKNeighbors = GetInt("vipres", "remesh_geodesic_k_neighbors", m_remeshGeodesicKNeighbors);
    m_remeshGeodesicInteriorBias = GetFloat("vipres", "remesh_geodesic_interior_bias", m_remeshGeodesicInteriorBias);
    m_segmentMatchRadius = GetFloat("vipres", "segment_match_radius", m_segmentMatchRadius);
    
    // VIPRES Limits
    m_remeshMaxDistanceMin = GetFloat("vipres.limits", "max_distance_min", m_remeshMaxDistanceMin);
    m_remeshMaxDistanceMax = GetFloat("vipres.limits", "max_distance_max", m_remeshMaxDistanceMax);
    m_remeshMaxNeighborsMin = GetInt("vipres.limits", "max_neighbors_min", m_remeshMaxNeighborsMin);
    m_remeshMaxNeighborsMax = GetInt("vipres.limits", "max_neighbors_max", m_remeshMaxNeighborsMax);
    m_remeshDelaunayThresholdMin = GetFloat("vipres.limits", "delaunay_threshold_min", m_remeshDelaunayThresholdMin);
    m_remeshDelaunayThresholdMax = GetFloat("vipres.limits", "delaunay_threshold_max", m_remeshDelaunayThresholdMax);
    m_remeshPoissonRadiusMin = GetFloat("vipres.limits", "poisson_radius_min", m_remeshPoissonRadiusMin);
    m_remeshPoissonRadiusMax = GetFloat("vipres.limits", "poisson_radius_max", m_remeshPoissonRadiusMax);
    m_remeshPoissonDepthMin = GetInt("vipres.limits", "poisson_depth_min", m_remeshPoissonDepthMin);
    m_remeshPoissonDepthMax = GetInt("vipres.limits", "poisson_depth_max", m_remeshPoissonDepthMax);
    m_remeshPoissonDensityMin = GetFloat("vipres.limits", "poisson_density_min", m_remeshPoissonDensityMin);
    m_remeshPoissonDensityMax = GetFloat("vipres.limits", "poisson_density_max", m_remeshPoissonDensityMax);
    m_remeshFctmSliceThicknessMin = GetFloat("vipres.limits", "fctm_slice_thickness_min", m_remeshFctmSliceThicknessMin);
    m_remeshFctmSliceThicknessMax = GetFloat("vipres.limits", "fctm_slice_thickness_max", m_remeshFctmSliceThicknessMax);
    m_remeshGeodesicKNeighborsMin = GetInt("vipres.limits", "geodesic_k_neighbors_min", m_remeshGeodesicKNeighborsMin);
    m_remeshGeodesicKNeighborsMax = GetInt("vipres.limits", "geodesic_k_neighbors_max", m_remeshGeodesicKNeighborsMax);
    m_remeshGeodesicInteriorBiasMin = GetFloat("vipres.limits", "geodesic_interior_bias_min", m_remeshGeodesicInteriorBiasMin);
    m_remeshGeodesicInteriorBiasMax = GetFloat("vipres.limits", "geodesic_interior_bias_max", m_remeshGeodesicInteriorBiasMax);
    
    // DensitySDF
    m_densityVoxelSize = GetFloat("vipres.density_sdf", "voxel_size", m_densityVoxelSize);
    m_densitySmoothRadius = GetFloat("vipres.density_sdf", "smooth_radius", m_densitySmoothRadius);
    m_densityIsoValue = GetFloat("vipres.density_sdf", "iso_value", m_densityIsoValue);
    
    // Objects
    m_objectSizeDefault = GetFloat("objects", "size_default", m_objectSizeDefault);
    m_objectSizeMin = GetFloat("objects", "size_min", m_objectSizeMin);
    m_objectSizeMax = GetFloat("objects", "size_max", m_objectSizeMax);
    
    // UI Layout
    m_statusBarHeight = GetFloat("ui_layout", "status_bar_height", m_statusBarHeight);
    m_windowOffsetY = GetFloat("ui_layout", "window_offset_y", m_windowOffsetY);
    m_fpsColumnWidth = GetFloat("ui_layout", "fps_column_width", m_fpsColumnWidth);
    m_rowHeightSmall = GetFloat("ui_layout", "row_height_small", m_rowHeightSmall);
    m_rowHeightMedium = GetFloat("ui_layout", "row_height_medium", m_rowHeightMedium);
    m_rowHeightStandard = GetFloat("ui_layout", "row_height_standard", m_rowHeightStandard);
    m_rowHeightButton = GetFloat("ui_layout", "row_height_button", m_rowHeightButton);
    m_spacingSmall = GetFloat("ui_layout", "spacing_small", m_spacingSmall);
    m_spacingMedium = GetFloat("ui_layout", "spacing_medium", m_spacingMedium);
    m_spacingLarge = GetFloat("ui_layout", "spacing_large", m_spacingLarge);
    
    m_cameraControlsX = GetFloat("ui_layout.camera_controls", "x", m_cameraControlsX);
    m_cameraControlsWidth = GetFloat("ui_layout.camera_controls", "width", m_cameraControlsWidth);
    m_cameraControlsHeight = GetFloat("ui_layout.camera_controls", "height", m_cameraControlsHeight);
    
    m_tofControlsX = GetFloat("ui_layout.tof_controls", "x", m_tofControlsX);
    m_tofControlsWidth = GetFloat("ui_layout.tof_controls", "width", m_tofControlsWidth);
    m_tofControlsHeightBase = GetFloat("ui_layout.tof_controls", "height_base", m_tofControlsHeightBase);
    m_tofControlsHeightMax = GetFloat("ui_layout.tof_controls", "height_max", m_tofControlsHeightMax);
    m_tofControlsHeightPerCamera = GetFloat("ui_layout.tof_controls", "height_per_camera", m_tofControlsHeightPerCamera);
    
    m_vipresControlsX = GetFloat("ui_layout.vipres_controls", "x", m_vipresControlsX);
    m_vipresControlsWidth = GetFloat("ui_layout.vipres_controls", "width", m_vipresControlsWidth);
    m_vipresControlsHeightMin = GetFloat("ui_layout.vipres_controls", "height_min", m_vipresControlsHeightMin);
    m_vipresControlsHeightRemesh = GetFloat("ui_layout.vipres_controls", "height_remesh", m_vipresControlsHeightRemesh);
    
    m_objectsPanelX = GetFloat("ui_layout.objects_panel", "x", m_objectsPanelX);
    m_objectsPanelWidth = GetFloat("ui_layout.objects_panel", "width", m_objectsPanelWidth);
    m_objectsPanelHeight = GetFloat("ui_layout.objects_panel", "height", m_objectsPanelHeight);
    
    m_statisticsPanelX = GetFloat("ui_layout.statistics_panel", "x", m_statisticsPanelX);
    m_statisticsPanelWidth = GetFloat("ui_layout.statistics_panel", "width", m_statisticsPanelWidth);
    m_statisticsPanelHeight = GetFloat("ui_layout.statistics_panel", "height", m_statisticsPanelHeight);
    m_histogramHeight = GetFloat("ui_layout.statistics_panel", "histogram_height", m_histogramHeight);
    
    m_loaded = true;
    std::cout << "[UIConfig] Loaded configuration from: " << filepath << std::endl;
    return true;
}

bool UIConfig::Reload() {
    if (m_configPath.empty()) {
        return false;
    }
    SetDefaults();  // Reset to defaults first
    m_values.clear();
    return LoadFromFile(m_configPath);
}

bool UIConfig::ParseToml(const std::string& content) {
    std::istringstream stream(content);
    std::string line;
    std::string currentSection;
    
    while (std::getline(stream, line)) {
        // Trim whitespace
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        line = line.substr(start, end - start + 1);
        
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#') continue;
        
        // Check for section header [section] or [section.subsection]
        if (line[0] == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.size() - 2);
            continue;
        }
        
        // Parse key = value
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;
        
        std::string key = line.substr(0, eqPos);
        std::string value = line.substr(eqPos + 1);
        
        // Trim key and value
        start = key.find_first_not_of(" \t");
        end = key.find_last_not_of(" \t");
        if (start != std::string::npos) {
            key = key.substr(start, end - start + 1);
        }
        
        start = value.find_first_not_of(" \t");
        end = value.find_last_not_of(" \t");
        if (start != std::string::npos) {
            value = value.substr(start, end - start + 1);
        }
        
        // Remove inline comments
        size_t commentPos = value.find('#');
        if (commentPos != std::string::npos) {
            value = value.substr(0, commentPos);
            end = value.find_last_not_of(" \t");
            if (end != std::string::npos) {
                value = value.substr(0, end + 1);
            }
        }
        
        // Store with full key path
        std::string fullKey = currentSection.empty() ? key : currentSection + "." + key;
        m_values[fullKey] = value;
    }
    
    return true;
}

std::string UIConfig::GetValue(const std::string& section, const std::string& key) const {
    std::string fullKey = section + "." + key;
    auto it = m_values.find(fullKey);
    if (it != m_values.end()) {
        return it->second;
    }
    return "";
}

float UIConfig::GetFloat(const std::string& section, const std::string& key, float defaultVal) const {
    std::string val = GetValue(section, key);
    if (val.empty()) return defaultVal;
    try {
        return std::stof(val);
    } catch (...) {
        return defaultVal;
    }
}

int UIConfig::GetInt(const std::string& section, const std::string& key, int defaultVal) const {
    std::string val = GetValue(section, key);
    if (val.empty()) return defaultVal;
    try {
        return std::stoi(val);
    } catch (...) {
        return defaultVal;
    }
}

bool UIConfig::GetBool(const std::string& section, const std::string& key, bool defaultVal) const {
    std::string val = GetValue(section, key);
    if (val.empty()) return defaultVal;
    // Convert to lowercase for comparison
    std::transform(val.begin(), val.end(), val.begin(), ::tolower);
    if (val == "true" || val == "1" || val == "yes" || val == "on") return true;
    if (val == "false" || val == "0" || val == "no" || val == "off") return false;
    return defaultVal;
}

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine
