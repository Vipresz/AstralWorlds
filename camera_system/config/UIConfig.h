#pragma once

#include <string>
#include <map>
#include <variant>
#include <vector>

namespace Engine {
namespace Editors {
namespace CameraSystem {

/**
 * UIConfig - Runtime configuration for UI defaults and limits
 * 
 * Loads values from ui_defaults.toml and provides access to all UI settings.
 * This replaces the compile-time constexpr values in GlobalConstants.h and UIDefaults.h.
 */
class UIConfig {
public:
    // Singleton access
    static UIConfig& Instance();
    
    // Load configuration from TOML file
    bool LoadFromFile(const std::string& filepath);
    
    // Reload the configuration file
    bool Reload();
    
    // Check if config was loaded successfully
    bool IsLoaded() const { return m_loaded; }
    
    // Get the path of the loaded config file
    const std::string& GetConfigPath() const { return m_configPath; }
    
    // ========================================================================
    // Camera Settings
    // ========================================================================
    float GetCameraMovementSpeed() const { return m_cameraMovementSpeed; }
    float GetCameraYaw() const { return m_cameraYaw; }
    float GetCameraPitch() const { return m_cameraPitch; }
    
    // ========================================================================
    // Grid Settings
    // ========================================================================
    float GetWorldGridSpacing() const { return m_worldGridSpacing; }
    int GetGridCellCount() const { return m_gridCellCount; }
    float GetWorldGridSize() const { return m_gridCellCount * m_worldGridSpacing; }
    bool GetGridVisible() const { return m_gridVisible; }
    
    // ========================================================================
    // Camera Controls - Toggles
    // ========================================================================
    bool GetLoopEnabled() const { return m_loopEnabled; }
    bool GetCaptureEnabled() const { return m_captureEnabled; }
    bool GetTofDataEnabled() const { return m_tofDataEnabled; }
    bool GetVipresEnabled() const { return m_vipresEnabled; }
    bool GetVipresFreeObjectEnabled() const { return m_vipresFreeObjectEnabled; }
    
    // ========================================================================
    // TOF Controls - Toggles
    // ========================================================================
    bool GetExtractColorEnabled() const { return m_extractColorEnabled; }
    int GetTofRenderColorMode() const { return m_tofRenderColorMode; }
    bool GetGriddingEnabled() const { return m_griddingEnabled; }
    bool GetLowPassEnabled() const { return m_lowPassEnabled; }
    bool GetTsdfEnabled() const { return m_tsdfEnabled; }
    bool GetTsdfStoreColor() const { return m_tsdfStoreColor; }
    bool GetNoisyTofEnabled() const { return m_noisyTofEnabled; }
    
    // TOF Controls - Sliders
    int GetSamplingDensity() const { return m_samplingDensity; }
    float GetDepthLimit() const { return m_depthLimit; }
    float GetTofSamplingRate() const { return m_tofSamplingRate; }
    float GetGridSize() const { return m_gridSize; }
    int GetGridMode() const { return m_gridMode; }
    float GetTsdfTruncationDistance() const { return m_tsdfTruncationDistance; }
    float GetTsdfWeightDecay() const { return m_tsdfWeightDecay; }
    float GetLowPassCutoff() const { return m_lowPassCutoff; }
    float GetNoiseAmount() const { return m_noiseAmount; }
    
    // TOF Slider Limits
    int GetSamplingDensityMin() const { return m_samplingDensityMin; }
    int GetSamplingDensityMax() const { return m_samplingDensityMax; }
    float GetDepthLimitMin() const { return m_depthLimitMin; }
    float GetDepthLimitMax() const { return m_depthLimitMax; }
    float GetTofSamplingRateMin() const { return m_tofSamplingRateMin; }
    float GetTofSamplingRateMax() const { return m_tofSamplingRateMax; }
    float GetGridSizeMin() const { return m_gridSizeMin; }
    float GetGridSizeMax() const { return m_gridSizeMax; }
    float GetTsdfTruncationDistanceMin() const { return m_tsdfTruncationDistanceMin; }
    float GetTsdfTruncationDistanceMax() const { return m_tsdfTruncationDistanceMax; }
    float GetTsdfWeightDecayMin() const { return m_tsdfWeightDecayMin; }
    float GetTsdfWeightDecayMax() const { return m_tsdfWeightDecayMax; }
    float GetLowPassCutoffMin() const { return m_lowPassCutoffMin; }
    float GetLowPassCutoffMax() const { return m_lowPassCutoffMax; }
    float GetNoiseAmountMin() const { return m_noiseAmountMin; }
    float GetNoiseAmountMax() const { return m_noiseAmountMax; }
    
    // ========================================================================
    // Smart Occupancy Settings
    // ========================================================================
    bool GetOccupancyEnabled() const { return m_occupancyEnabled; }
    bool GetOccupancyNeighborConsistency() const { return m_occupancyNeighborConsistency; }
    bool GetOccupancyPersistence() const { return m_occupancyPersistence; }
    bool GetOccupancyVariance() const { return m_occupancyVariance; }
    bool GetOccupancyEviction() const { return m_occupancyEviction; }
    bool GetOccupancyEviction2() const { return m_occupancyEviction2; }
    
    float GetOccupancyNeighborRadius() const { return m_occupancyNeighborRadius; }
    float GetOccupancyResidualThreshold() const { return m_occupancyResidualThreshold; }
    float GetOccupancyPersistenceGamma() const { return m_occupancyPersistenceGamma; }
    float GetOccupancyVarianceThreshold() const { return m_occupancyVarianceThreshold; }
    float GetOccupancyEvictionDecay() const { return m_occupancyEvictionDecay; }
    float GetOccupancyEvictionIncrement() const { return m_occupancyEvictionIncrement; }
    float GetOccupancyEvictionMinStartValue() const { return m_occupancyEvictionMinStartValue; }
    int GetOccupancyEviction2AppearThreshold() const { return m_occupancyEviction2AppearThreshold; }
    int GetOccupancyEviction2RemovalThreshold() const { return m_occupancyEviction2RemovalThreshold; }
    float GetWeightThreshold() const { return m_weightThreshold; }
    
    // Occupancy Limits
    float GetOccupancyNeighborRadiusMin() const { return m_occupancyNeighborRadiusMin; }
    float GetOccupancyNeighborRadiusMax() const { return m_occupancyNeighborRadiusMax; }
    float GetOccupancyNeighborRadiusStep() const { return m_occupancyNeighborRadiusStep; }
    float GetOccupancyResidualThresholdMin() const { return m_occupancyResidualThresholdMin; }
    float GetOccupancyResidualThresholdMax() const { return m_occupancyResidualThresholdMax; }
    float GetOccupancyResidualThresholdStep() const { return m_occupancyResidualThresholdStep; }
    float GetOccupancyPersistenceGammaMin() const { return m_occupancyPersistenceGammaMin; }
    float GetOccupancyPersistenceGammaMax() const { return m_occupancyPersistenceGammaMax; }
    float GetOccupancyPersistenceGammaStep() const { return m_occupancyPersistenceGammaStep; }
    float GetOccupancyVarianceThresholdMin() const { return m_occupancyVarianceThresholdMin; }
    float GetOccupancyVarianceThresholdMax() const { return m_occupancyVarianceThresholdMax; }
    float GetOccupancyVarianceThresholdStep() const { return m_occupancyVarianceThresholdStep; }
    float GetOccupancyEvictionDecayMin() const { return m_occupancyEvictionDecayMin; }
    float GetOccupancyEvictionDecayMax() const { return m_occupancyEvictionDecayMax; }
    float GetOccupancyEvictionDecayStep() const { return m_occupancyEvictionDecayStep; }
    float GetOccupancyEvictionIncrementMin() const { return m_occupancyEvictionIncrementMin; }
    float GetOccupancyEvictionIncrementMax() const { return m_occupancyEvictionIncrementMax; }
    float GetOccupancyEvictionIncrementStep() const { return m_occupancyEvictionIncrementStep; }
    float GetOccupancyEvictionMinStartMin() const { return m_occupancyEvictionMinStartMin; }
    float GetOccupancyEvictionMinStartMax() const { return m_occupancyEvictionMinStartMax; }
    float GetOccupancyEvictionMinStartStep() const { return m_occupancyEvictionMinStartStep; }
    int GetOccupancyEviction2AppearThresholdMin() const { return m_occupancyEviction2AppearThresholdMin; }
    int GetOccupancyEviction2AppearThresholdMax() const { return m_occupancyEviction2AppearThresholdMax; }
    int GetOccupancyEviction2RemovalThresholdMin() const { return m_occupancyEviction2RemovalThresholdMin; }
    int GetOccupancyEviction2RemovalThresholdMax() const { return m_occupancyEviction2RemovalThresholdMax; }
    float GetWeightThresholdMin() const { return m_weightThresholdMin; }
    float GetWeightThresholdMax() const { return m_weightThresholdMax; }
    float GetWeightThresholdStep() const { return m_weightThresholdStep; }
    
    // ========================================================================
    // VIPRES Settings
    // ========================================================================
    bool GetVipresComputeColor() const { return m_vipresComputeColor; }
    bool GetSegmentationIdEnabled() const { return m_segmentationIdEnabled; }
    bool GetSurfaceFlowEnabled() const { return m_surfaceFlowEnabled; }
    bool GetRemeshEnabled() const { return m_remeshEnabled; }
    bool GetAutoRecomputeMesh() const { return m_autoRecomputeMesh; }
    bool GetRemeshPoissonSmoothNormals() const { return m_remeshPoissonSmoothNormals; }
    
    float GetSurfaceFlowRadius() const { return m_surfaceFlowRadius; }
    float GetSurfaceFlowThreshold() const { return m_surfaceFlowThreshold; }
    int GetRemeshAlgorithm() const { return m_remeshAlgorithm; }
    float GetRemeshMaxDistance() const { return m_remeshMaxDistance; }
    int GetRemeshMaxNeighbors() const { return m_remeshMaxNeighbors; }
    int GetRemeshProjectionPlane() const { return m_remeshProjectionPlane; }
    float GetRemeshDelaunayThreshold() const { return m_remeshDelaunayThreshold; }
    float GetRemeshPoissonRadius() const { return m_remeshPoissonRadius; }
    int GetRemeshPoissonDepth() const { return m_remeshPoissonDepth; }
    float GetRemeshPoissonDensity() const { return m_remeshPoissonDensity; }
    float GetRemeshFctmSliceThickness() const { return m_remeshFctmSliceThickness; }
    int GetRemeshGeodesicKNeighbors() const { return m_remeshGeodesicKNeighbors; }
    float GetRemeshGeodesicInteriorBias() const { return m_remeshGeodesicInteriorBias; }
    float GetSegmentMatchRadius() const { return m_segmentMatchRadius; }
    
    // VIPRES Limits
    float GetRemeshMaxDistanceMin() const { return m_remeshMaxDistanceMin; }
    float GetRemeshMaxDistanceMax() const { return m_remeshMaxDistanceMax; }
    int GetRemeshMaxNeighborsMin() const { return m_remeshMaxNeighborsMin; }
    int GetRemeshMaxNeighborsMax() const { return m_remeshMaxNeighborsMax; }
    float GetRemeshDelaunayThresholdMin() const { return m_remeshDelaunayThresholdMin; }
    float GetRemeshDelaunayThresholdMax() const { return m_remeshDelaunayThresholdMax; }
    float GetRemeshPoissonRadiusMin() const { return m_remeshPoissonRadiusMin; }
    float GetRemeshPoissonRadiusMax() const { return m_remeshPoissonRadiusMax; }
    int GetRemeshPoissonDepthMin() const { return m_remeshPoissonDepthMin; }
    int GetRemeshPoissonDepthMax() const { return m_remeshPoissonDepthMax; }
    float GetRemeshPoissonDensityMin() const { return m_remeshPoissonDensityMin; }
    float GetRemeshPoissonDensityMax() const { return m_remeshPoissonDensityMax; }
    float GetRemeshFctmSliceThicknessMin() const { return m_remeshFctmSliceThicknessMin; }
    float GetRemeshFctmSliceThicknessMax() const { return m_remeshFctmSliceThicknessMax; }
    int GetRemeshGeodesicKNeighborsMin() const { return m_remeshGeodesicKNeighborsMin; }
    int GetRemeshGeodesicKNeighborsMax() const { return m_remeshGeodesicKNeighborsMax; }
    float GetRemeshGeodesicInteriorBiasMin() const { return m_remeshGeodesicInteriorBiasMin; }
    float GetRemeshGeodesicInteriorBiasMax() const { return m_remeshGeodesicInteriorBiasMax; }
    
    // DensitySDF Settings
    float GetDensityVoxelSize() const { return m_densityVoxelSize; }
    float GetDensitySmoothRadius() const { return m_densitySmoothRadius; }
    float GetDensityIsoValue() const { return m_densityIsoValue; }
    
    // ========================================================================
    // Object Settings
    // ========================================================================
    float GetObjectSizeDefault() const { return m_objectSizeDefault; }
    float GetObjectSizeMin() const { return m_objectSizeMin; }
    float GetObjectSizeMax() const { return m_objectSizeMax; }
    
    // ========================================================================
    // UI Layout Constants
    // ========================================================================
    float GetStatusBarHeight() const { return m_statusBarHeight; }
    float GetWindowOffsetY() const { return m_windowOffsetY; }
    float GetFpsColumnWidth() const { return m_fpsColumnWidth; }
    float GetRowHeightSmall() const { return m_rowHeightSmall; }
    float GetRowHeightMedium() const { return m_rowHeightMedium; }
    float GetRowHeightStandard() const { return m_rowHeightStandard; }
    float GetRowHeightButton() const { return m_rowHeightButton; }
    float GetSpacingSmall() const { return m_spacingSmall; }
    float GetSpacingMedium() const { return m_spacingMedium; }
    float GetSpacingLarge() const { return m_spacingLarge; }
    
    // Camera Controls Window
    float GetCameraControlsX() const { return m_cameraControlsX; }
    float GetCameraControlsWidth() const { return m_cameraControlsWidth; }
    float GetCameraControlsHeight() const { return m_cameraControlsHeight; }
    
    // TOF Controls Window
    float GetTofControlsX() const { return m_tofControlsX; }
    float GetTofControlsWidth() const { return m_tofControlsWidth; }
    float GetTofControlsHeightBase() const { return m_tofControlsHeightBase; }
    float GetTofControlsHeightMax() const { return m_tofControlsHeightMax; }
    float GetTofControlsHeightPerCamera() const { return m_tofControlsHeightPerCamera; }
    
    // VIPRES Controls Window
    float GetVipresControlsX() const { return m_vipresControlsX; }
    float GetVipresControlsWidth() const { return m_vipresControlsWidth; }
    float GetVipresControlsHeightMin() const { return m_vipresControlsHeightMin; }
    float GetVipresControlsHeightRemesh() const { return m_vipresControlsHeightRemesh; }
    
    // Objects Panel Window
    float GetObjectsPanelX() const { return m_objectsPanelX; }
    float GetObjectsPanelWidth() const { return m_objectsPanelWidth; }
    float GetObjectsPanelHeight() const { return m_objectsPanelHeight; }
    
    // Statistics Panel Window
    float GetStatisticsPanelX() const { return m_statisticsPanelX; }
    float GetStatisticsPanelWidth() const { return m_statisticsPanelWidth; }
    float GetStatisticsPanelHeight() const { return m_statisticsPanelHeight; }
    float GetHistogramHeight() const { return m_histogramHeight; }

private:
    UIConfig();
    ~UIConfig() = default;
    UIConfig(const UIConfig&) = delete;
    UIConfig& operator=(const UIConfig&) = delete;
    
    // Parse a TOML file
    bool ParseToml(const std::string& content);
    
    // Helper to get value from parsed data
    std::string GetValue(const std::string& section, const std::string& key) const;
    float GetFloat(const std::string& section, const std::string& key, float defaultVal) const;
    int GetInt(const std::string& section, const std::string& key, int defaultVal) const;
    bool GetBool(const std::string& section, const std::string& key, bool defaultVal) const;
    
    // Set default values
    void SetDefaults();
    
    // State
    bool m_loaded = false;
    std::string m_configPath;
    
    // Parsed TOML data: section.key -> value
    std::map<std::string, std::string> m_values;
    
    // ========================================================================
    // Member variables for all config values
    // ========================================================================
    
    // Camera
    float m_cameraMovementSpeed;
    float m_cameraYaw;
    float m_cameraPitch;
    
    // Grid
    float m_worldGridSpacing;
    int m_gridCellCount;
    bool m_gridVisible;
    
    // Camera Controls - Toggles
    bool m_loopEnabled;
    bool m_captureEnabled;
    bool m_tofDataEnabled;
    bool m_vipresEnabled;
    bool m_vipresFreeObjectEnabled;
    
    // TOF Controls - Toggles
    bool m_extractColorEnabled;
    int m_tofRenderColorMode;
    bool m_griddingEnabled;
    bool m_lowPassEnabled;
    bool m_tsdfEnabled;
    bool m_tsdfStoreColor;
    bool m_noisyTofEnabled;
    
    // TOF Controls - Sliders
    int m_samplingDensity;
    float m_depthLimit;
    float m_tofSamplingRate;
    float m_gridSize;
    int m_gridMode;
    float m_tsdfTruncationDistance;
    float m_tsdfWeightDecay;
    float m_lowPassCutoff;
    float m_noiseAmount;
    
    // TOF Limits
    int m_samplingDensityMin, m_samplingDensityMax;
    float m_depthLimitMin, m_depthLimitMax;
    float m_tofSamplingRateMin, m_tofSamplingRateMax;
    float m_gridSizeMin, m_gridSizeMax;
    float m_tsdfTruncationDistanceMin, m_tsdfTruncationDistanceMax;
    float m_tsdfWeightDecayMin, m_tsdfWeightDecayMax;
    float m_lowPassCutoffMin, m_lowPassCutoffMax;
    float m_noiseAmountMin, m_noiseAmountMax;
    
    // Occupancy
    bool m_occupancyEnabled;
    bool m_occupancyNeighborConsistency;
    bool m_occupancyPersistence;
    bool m_occupancyVariance;
    bool m_occupancyEviction;
    bool m_occupancyEviction2;
    
    float m_occupancyNeighborRadius;
    float m_occupancyResidualThreshold;
    float m_occupancyPersistenceGamma;
    float m_occupancyVarianceThreshold;
    float m_occupancyEvictionDecay;
    float m_occupancyEvictionIncrement;
    float m_occupancyEvictionMinStartValue;
    int m_occupancyEviction2AppearThreshold;
    int m_occupancyEviction2RemovalThreshold;
    float m_weightThreshold;
    
    // Occupancy Limits
    float m_occupancyNeighborRadiusMin, m_occupancyNeighborRadiusMax, m_occupancyNeighborRadiusStep;
    float m_occupancyResidualThresholdMin, m_occupancyResidualThresholdMax, m_occupancyResidualThresholdStep;
    float m_occupancyPersistenceGammaMin, m_occupancyPersistenceGammaMax, m_occupancyPersistenceGammaStep;
    float m_occupancyVarianceThresholdMin, m_occupancyVarianceThresholdMax, m_occupancyVarianceThresholdStep;
    float m_occupancyEvictionDecayMin, m_occupancyEvictionDecayMax, m_occupancyEvictionDecayStep;
    float m_occupancyEvictionIncrementMin, m_occupancyEvictionIncrementMax, m_occupancyEvictionIncrementStep;
    float m_occupancyEvictionMinStartMin, m_occupancyEvictionMinStartMax, m_occupancyEvictionMinStartStep;
    int m_occupancyEviction2AppearThresholdMin, m_occupancyEviction2AppearThresholdMax;
    int m_occupancyEviction2RemovalThresholdMin, m_occupancyEviction2RemovalThresholdMax;
    float m_weightThresholdMin, m_weightThresholdMax, m_weightThresholdStep;
    
    // VIPRES
    bool m_vipresComputeColor;
    bool m_segmentationIdEnabled;
    bool m_surfaceFlowEnabled;
    bool m_remeshEnabled;
    bool m_autoRecomputeMesh;
    bool m_remeshPoissonSmoothNormals;
    
    float m_surfaceFlowRadius;
    float m_surfaceFlowThreshold;
    int m_remeshAlgorithm;
    float m_remeshMaxDistance;
    int m_remeshMaxNeighbors;
    int m_remeshProjectionPlane;
    float m_remeshDelaunayThreshold;
    float m_remeshPoissonRadius;
    int m_remeshPoissonDepth;
    float m_remeshPoissonDensity;
    float m_remeshFctmSliceThickness;
    int m_remeshGeodesicKNeighbors;
    float m_remeshGeodesicInteriorBias;
    float m_segmentMatchRadius;
    
    // VIPRES Limits
    float m_remeshMaxDistanceMin, m_remeshMaxDistanceMax;
    int m_remeshMaxNeighborsMin, m_remeshMaxNeighborsMax;
    float m_remeshDelaunayThresholdMin, m_remeshDelaunayThresholdMax;
    float m_remeshPoissonRadiusMin, m_remeshPoissonRadiusMax;
    int m_remeshPoissonDepthMin, m_remeshPoissonDepthMax;
    float m_remeshPoissonDensityMin, m_remeshPoissonDensityMax;
    float m_remeshFctmSliceThicknessMin, m_remeshFctmSliceThicknessMax;
    int m_remeshGeodesicKNeighborsMin, m_remeshGeodesicKNeighborsMax;
    float m_remeshGeodesicInteriorBiasMin, m_remeshGeodesicInteriorBiasMax;
    
    // DensitySDF
    float m_densityVoxelSize;
    float m_densitySmoothRadius;
    float m_densityIsoValue;
    
    // Objects
    float m_objectSizeDefault, m_objectSizeMin, m_objectSizeMax;
    
    // UI Layout
    float m_statusBarHeight;
    float m_windowOffsetY;
    float m_fpsColumnWidth;
    float m_rowHeightSmall, m_rowHeightMedium, m_rowHeightStandard, m_rowHeightButton;
    float m_spacingSmall, m_spacingMedium, m_spacingLarge;
    
    float m_cameraControlsX, m_cameraControlsWidth, m_cameraControlsHeight;
    float m_tofControlsX, m_tofControlsWidth, m_tofControlsHeightBase, m_tofControlsHeightMax, m_tofControlsHeightPerCamera;
    float m_vipresControlsX, m_vipresControlsWidth, m_vipresControlsHeightMin, m_vipresControlsHeightRemesh;
    float m_objectsPanelX, m_objectsPanelWidth, m_objectsPanelHeight;
    float m_statisticsPanelX, m_statisticsPanelWidth, m_statisticsPanelHeight, m_histogramHeight;
};

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine
