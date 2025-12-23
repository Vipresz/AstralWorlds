#include "CameraSystemUI.h"
#include "../CameraSystemEditor.h"
#include "../GlobalConstants.h"
#include "UIHelpers.h"
#include "Plotting.h"
using namespace Engine::Editors::CameraSystem::Plotting;
#include "../../engine/platform/Logger.h"
#include "../../engine/platform/render/Model.h"
#include "../../../game_libs/Nuklear/nuklear.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>

using EngineLogging::Logger;
using namespace Engine::Editors::CameraSystem::Constants;

namespace Engine {
namespace Editors {
namespace CameraSystem {

CameraSystemUI::CameraSystemUI()
    : m_nuklearUI(nullptr)
    , m_glfwWindow(nullptr)
    , m_initialized(false)
{
}

CameraSystemUI::~CameraSystemUI() {
}

void CameraSystemUI::Initialize(NuklearUI* nuklearUI, GLFWwindow* glfwWindow) {
    m_nuklearUI = nuklearUI;
    m_glfwWindow = glfwWindow;
    m_initialized = (nuklearUI != nullptr && glfwWindow != nullptr);
}

void CameraSystemUI::GetWindowSize(int& width, int& height) {
    if (m_glfwWindow) {
        glfwGetFramebufferSize(m_glfwWindow, &width, &height);
    } else {
        width = 0;
        height = 0;
    }
}

void CameraSystemUI::Render(CameraSystemEditor& editor) {
    if (!m_initialized || !m_nuklearUI || !m_nuklearUI->IsInitialized()) {
        static int warnCount = 0;
        if (warnCount++ < 3) {
            LOG_WARNING("CameraSystemUI", "Render: UI not initialized");
        }
        return;
    }
    
    struct nk_context* ctx = m_nuklearUI->GetContext();
    if (!ctx) {
        static int warnCount = 0;
        if (warnCount++ < 3) {
            LOG_WARNING("CameraSystemUI", "Render: Nuklear context is null");
        }
        return;
    }
    
    try {
        if (!m_glfwWindow) return;
        
        int width, height;
        GetWindowSize(width, height);
        
        if (width <= 0 || height <= 0) {
            static int warnCount = 0;
            if (warnCount++ < 3) {
                LOG_WARNING_FMT("CameraSystemUI", "Render: Invalid window size: %d x %d", width, height);
            }
            return;
        }
        
        // Render all UI sections
        RenderStatusBar(editor);
        RenderCameraControls(editor);
        RenderTOFControls(editor);
        RenderVIPRESControls(editor);
        RenderObjectsPanel(editor);
        RenderStatisticsPanel(editor);
        
        // Render Nuklear UI to screen (must be called after all UI windows are built)
        // This matches the old version where nk_glfw3_render was called at the end of RenderUI()
        // OLD VERSION USED: nk_glfw3_render(m_nuklear, NK_ANTI_ALIASING_ON, 512 * 1024, 128 * 1024);
        if (m_nuklearUI->IsInitialized()) {
            // Use same hardcoded buffer sizes as old version (512KB vertex, 128KB element)
            m_nuklearUI->Render(512 * 1024, 128 * 1024);
        }
        
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("CameraSystemUI", "Exception in Render: %s", e.what());
    } catch (...) {
        LOG_ERROR("CameraSystemUI", "Unknown exception in Render");
    }
}

void CameraSystemUI::RenderStatusBar(CameraSystemEditor& editor) {
    struct nk_context* ctx = m_nuklearUI->GetContext();
    if (!ctx) return;
    
    int width, height;
    GetWindowSize(width, height);
    
    // Status bar at the top
    if (nk_begin(ctx, "Status Bar", nk_rect(0.0f, 0.0f, static_cast<float>(width), UI_STATUS_BAR_HEIGHT),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
        
        std::string statusText = "Camera System Editor";
        if (editor.m_configLoaded) {
            statusText += " | Config: Loaded";
            statusText += editor.m_isPlaying ? " | Playing" : " | Paused";
            statusText += " | Frame: " + std::to_string(editor.m_currentFrame) + "/" + std::to_string(editor.m_totalFrames - 1);
            statusText += editor.m_captureEnabled ? " | Capture: ON" : " | Capture: OFF";
        } else {
            statusText += " | No Config Loaded";
        }
        
        char fpsText[32];
        std::snprintf(fpsText, sizeof(fpsText), "%.1f Hz", editor.m_currentFPS);
        
        nk_layout_row_begin(ctx, NK_STATIC, UI_STATUS_BAR_HEIGHT, 2);
        nk_layout_row_push(ctx, static_cast<float>(width) - UI_FPS_COLUMN_WIDTH);
        nk_label(ctx, statusText.c_str(), NK_TEXT_LEFT);
        nk_layout_row_push(ctx, UI_FPS_COLUMN_WIDTH);
        nk_label(ctx, fpsText, NK_TEXT_RIGHT);
        nk_layout_row_end(ctx);
    }
    nk_end(ctx);
}

void CameraSystemUI::RenderCameraControls(CameraSystemEditor& editor) {
    struct nk_context* ctx = m_nuklearUI->GetContext();
    if (!ctx) return;
    
    if (nk_begin(ctx, "Camera Controls", 
                 nk_rect(UI_CAMERA_CONTROLS_X, UI_STATUS_BAR_HEIGHT + UI_WINDOW_OFFSET_Y, 
                        UI_CAMERA_CONTROLS_WIDTH, UI_CAMERA_CONTROLS_HEIGHT),
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE)) {
        
        nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
        std::string activeCameraName = "None";
        size_t activeIdx = editor.m_cameraSystem.GetActiveCameraIndex();
        if (activeIdx < editor.m_cameras.size()) {
            activeCameraName = editor.m_cameras[activeIdx].name;
        }
        nk_label(ctx, ("Active: " + activeCameraName).c_str(), NK_TEXT_LEFT);
        
        // Play/Pause button and Start Capture button side by side
        if (editor.m_configLoaded && editor.m_totalFrames > 0) {
            nk_layout_row_dynamic(ctx, UI_SPACING_SMALL, 1);
            nk_spacing(ctx, 1);
            
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_BUTTON, 2);
            const char* playPauseText = editor.m_isPlaying ? "Pause" : "Play";
            if (nk_button_label(ctx, playPauseText)) {
                // If we're at the end and loop is off, reset to frame 0 when starting play
                if (!editor.m_isPlaying && editor.m_currentFrame >= editor.m_totalFrames - 1 && !editor.m_loopEnabled) {
                    editor.m_currentFrame = 0;
                }
                editor.m_isPlaying = !editor.m_isPlaying;
                // Immediately update cameras when toggling play/pause
                if (editor.m_totalFrames > 0 && editor.m_numCameras > 0) {
                    editor.UpdateAnimatedCameras();
                    editor.UpdateCamera();
                }
            }
            
            // Capture toggle button (right next to Play button)
            const char* captureText = editor.m_captureEnabled ? "Stop Capture" : "Start Capture";
            if (nk_button_label(ctx, captureText)) {
                editor.m_captureEnabled = !editor.m_captureEnabled;
                editor.SaveSafeParameters();
            }
            
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_MEDIUM, 1);
            char frameInfo[128];
            std::snprintf(frameInfo, sizeof(frameInfo), "Frame: %d/%d %s", 
                            editor.m_currentFrame, editor.m_totalFrames, 
                            editor.m_isPlaying ? "(Playing)" : "(Paused)");
            nk_label(ctx, frameInfo, NK_TEXT_LEFT);
            
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
            bool loopEnabled = editor.m_loopEnabled;
            nk_bool loopCheckboxState = loopEnabled ? nk_false : nk_true; // Inverted logic
            if (nk_checkbox_label(ctx, "Loop", &loopCheckboxState)) {
                editor.m_loopEnabled = (loopCheckboxState == nk_false);
                editor.SaveSafeParameters();
            }
        } else {
            nk_layout_row_dynamic(ctx, UI_SPACING_SMALL, 1);
            nk_spacing(ctx, 1);
            
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_BUTTON, 1);
            const char* captureText = editor.m_captureEnabled ? "Stop Capture" : "Start Capture";
            if (nk_button_label(ctx, captureText)) {
                editor.m_captureEnabled = !editor.m_captureEnabled;
                editor.SaveSafeParameters();
            }
        }
        
        nk_layout_row_dynamic(ctx, UI_SPACING_MEDIUM, 1);
        nk_spacing(ctx, 1);
        
        nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_BUTTON, 1);
        const char* tofText = editor.m_tofDataEnabled ? "Disable TOF Data" : "Enable TOF Data";
        if (nk_button_label(ctx, tofText)) {
            editor.m_tofDataEnabled = !editor.m_tofDataEnabled;
            if (editor.m_tofDataEnabled) {
                editor.UpdatePointCloud();
            }
            editor.SaveSafeParameters();
        }
        
        nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_BUTTON, 1);
        const char* vipresText = editor.m_vipresEnabled ? "Disable VIPRES" : "Enable VIPRES";
        if (nk_button_label(ctx, vipresText)) {
            editor.m_vipresEnabled = !editor.m_vipresEnabled;
            if (editor.m_vipresEnabled) {
                // VIPRES only handles remesh computation, not depth data calculation
                // If remesh is enabled, generate it from existing point cloud
                if (editor.m_vipresRemeshEnabled) {
                    editor.m_remeshNeedsUpdate = true;
                    editor.GenerateRemesh();
                }
            }
            editor.SaveSafeParameters();
        }
        
        nk_layout_row_dynamic(ctx, UI_SPACING_MEDIUM, 1);
        nk_spacing(ctx, 1);
        
        nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 2);
        bool freeCamObjEnabled = editor.m_vipresFreeCameraObjectEnabled;
        nk_bool freeCamObjCheckboxState = freeCamObjEnabled ? nk_false : nk_true; // Inverted logic
        if (nk_checkbox_label(ctx, "Show Object", &freeCamObjCheckboxState)) {
            editor.m_vipresFreeCameraObjectEnabled = (freeCamObjCheckboxState == nk_false);
            editor.SaveSafeParameters();
        }
        
        bool gridVisible = editor.m_world.IsGridVisible();
        nk_bool gridCheckboxState = gridVisible ? nk_true : nk_false; // Normal logic: checked = visible
        if (nk_checkbox_label(ctx, "Show Grid", &gridCheckboxState)) {
            editor.m_world.SetGridVisible(gridCheckboxState == nk_true);
            editor.SaveSafeParameters();
        }
        
        nk_layout_row_dynamic(ctx, UI_SPACING_LARGE, 1);
        nk_spacing(ctx, 1);
        
        if (!editor.m_cameras.empty()) {
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_MEDIUM, 1);
            nk_label(ctx, "All Cameras:", NK_TEXT_LEFT);
            
            for (size_t i = 0; i < editor.m_cameras.size(); i++) {
                const Camera& cam = editor.m_cameras[i];
                if (cam.canMove) continue;
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                std::string cameraLabel;
                size_t activeIdx2 = editor.m_cameraSystem.GetActiveCameraIndex();
                if (i == activeIdx2) {
                    cameraLabel = "> " + cam.name + " <";
                } else {
                    cameraLabel = cam.name;
                }
                
                // Make camera name clickable
                if (nk_button_label(ctx, cameraLabel.c_str())) {
                    if (i != activeIdx2) {
                        editor.m_pendingCameraSwitch = i;
                    }
                }
                
                // Position vector
                nk_layout_row_dynamic(ctx, 18, 1);
                char posStr[128];
                std::snprintf(posStr, sizeof(posStr), "  Pos: (%.2f, %.2f, %.2f)", 
                        cam.position.x, cam.position.y, cam.position.z);
                nk_label(ctx, posStr, NK_TEXT_LEFT);
                
                // Direction vector (from position to target)
                glm::vec3 direction = cam.target - cam.position;
                float dirLength = glm::length(direction);
                if (dirLength > 0.0001f) {
                    direction = direction / dirLength; // Normalize
                }
                nk_layout_row_dynamic(ctx, 18, 1);
                char dirStr[128];
                std::snprintf(dirStr, sizeof(dirStr), "  Dir: (%.2f, %.2f, %.2f)", 
                        direction.x, direction.y, direction.z);
                nk_label(ctx, dirStr, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_SPACING_MEDIUM, 1);
                nk_spacing(ctx, 1);
            }
            
            for (size_t i = 0; i < editor.m_cameras.size(); i++) {
                const Camera& cam = editor.m_cameras[i];
                if (!cam.canMove) continue;
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                std::string cameraLabel;
                size_t activeIdx2 = editor.m_cameraSystem.GetActiveCameraIndex();
                if (i == activeIdx2) {
                    cameraLabel = "> " + cam.name + " <";
                } else {
                    cameraLabel = cam.name;
                }
                
                // Make camera name clickable
                if (nk_button_label(ctx, cameraLabel.c_str())) {
                    if (i != activeIdx2) {
                        editor.m_pendingCameraSwitch = i;
                    }
                }
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_SMALL, 1);
                char posStr[128];
                std::snprintf(posStr, sizeof(posStr), "  Pos: (%.2f, %.2f, %.2f)", 
                        cam.position.x, cam.position.y, cam.position.z);
                nk_label(ctx, posStr, NK_TEXT_LEFT);
                
                glm::vec3 direction = cam.target - cam.position;
                float dirLength = glm::length(direction);
                if (dirLength > 0.0001f) {
                    direction = direction / dirLength;
                }
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_SMALL, 1);
                char dirStr[128];
                std::snprintf(dirStr, sizeof(dirStr), "  Dir: (%.2f, %.2f, %.2f)", 
                        direction.x, direction.y, direction.z);
                nk_label(ctx, dirStr, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_SPACING_MEDIUM, 1);
                nk_spacing(ctx, 1);
            }
        } else {
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_MEDIUM, 1);
            nk_label(ctx, "No cameras available", NK_TEXT_CENTERED);
        }
    }
    nk_end(ctx);
}

void CameraSystemUI::RenderTOFControls(CameraSystemEditor& editor) {
    struct nk_context* ctx = m_nuklearUI->GetContext();
    if (!ctx) return;
    
    if (!editor.m_tofDataEnabled) return;
    
    int anchorCameraCount = 0;
    for (const auto& cam : editor.m_cameras) {
        if (!cam.canMove) anchorCameraCount++;
    }
    
    float tofWindowHeight = UI_TOF_CONTROLS_HEIGHT_BASE;
    if (anchorCameraCount > 0) {
        tofWindowHeight = std::min(UI_TOF_CONTROLS_HEIGHT_MAX, 
                                   UI_TOF_CONTROLS_HEIGHT_BASE + anchorCameraCount * UI_TOF_CONTROLS_HEIGHT_PER_CAMERA);
    }
    
    bool tofControlsVisible = nk_begin(ctx, "TOF Controls", 
                                        nk_rect(UI_TOF_CONTROLS_X, UI_STATUS_BAR_HEIGHT + UI_WINDOW_OFFSET_Y, 
                                               UI_TOF_CONTROLS_WIDTH, tofWindowHeight),
                                        NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE);
    if (tofControlsVisible) {
        
        // Point count display at the top (after problem points are removed)
        nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
        char pointCountText[128];
        std::snprintf(pointCountText, sizeof(pointCountText), "Points: %zu", editor.m_pointCloud[editor.m_pointCloudReadBuffer].size());
        nk_label(ctx, pointCountText, NK_TEXT_LEFT);
        
        nk_layout_row_dynamic(ctx, UI_SPACING_SMALL, 1);
        nk_spacing(ctx, 1);
        
        // Color code TOF toggle
        nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
        bool colorCodeEnabled = editor.m_tofColorCodeByCamera;
        nk_bool colorCodeCheckboxState = colorCodeEnabled ? nk_true : nk_false;
        if (nk_checkbox_label(ctx, "Color Code TOF", &colorCodeCheckboxState)) {
            editor.m_tofColorCodeByCamera = (colorCodeCheckboxState == nk_true);
            if (editor.m_tofDataEnabled) {
                // Update point cloud to reflect color change
                editor.UpdatePointCloud();
            }
            editor.SaveSafeParameters();
        }
        
        nk_layout_row_dynamic(ctx, UI_SPACING_SMALL, 1);
        nk_spacing(ctx, 1);
        
        // Sampling Density with value on same line, slider below
        nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
        nk_layout_row_template_push_static(ctx, 150);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);
        char densityLabelText[128];
        std::snprintf(densityLabelText, sizeof(densityLabelText), "Sampling Density: %d", editor.m_vipresSamplingDensity);
        nk_label(ctx, densityLabelText, NK_TEXT_LEFT);
        
        nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
        int density = editor.m_vipresSamplingDensity;
        nk_slider_int(ctx, SAMPLING_DENSITY_MIN, &density, SAMPLING_DENSITY_MAX, 1);
        if (density != editor.m_vipresSamplingDensity) {
            editor.m_vipresSamplingDensity = density;
            editor.UpdatePointCloud();
            editor.SaveSafeParameters();
        }
        
        nk_layout_row_dynamic(ctx, UI_SPACING_SMALL, 1);
        nk_spacing(ctx, 1);
        
        // Depth Limit with value on same line, slider below (log scale: 0.001 to 100000)
        nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
        nk_layout_row_template_push_static(ctx, 150);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);
        char limitLabelText[128];
        std::snprintf(limitLabelText, sizeof(limitLabelText), "Depth Limit: %.3f", editor.m_vipresDepthLimit);
        nk_label(ctx, limitLabelText, NK_TEXT_LEFT);
        
        nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
        // Log scale slider: map DEPTH_LIMIT_MIN to DEPTH_LIMIT_MAX
        const float depthLogMin = std::log10(DEPTH_LIMIT_MIN);
        const float depthLogMax = std::log10(DEPTH_LIMIT_MAX);
        float currentDepthLog = std::log10(std::max(DEPTH_LIMIT_MIN, editor.m_vipresDepthLimit));
        float depthNormalized = (currentDepthLog - depthLogMin) / (depthLogMax - depthLogMin);
        depthNormalized = std::max(0.0f, std::min(1.0f, depthNormalized));
        
        nk_slider_float(ctx, 0.0f, &depthNormalized, 1.0f, 0.01f);
        
        float newDepthLog = depthLogMin + depthNormalized * (depthLogMax - depthLogMin);
        float newDepthLimit = std::pow(10.0f, newDepthLog);
        
        if (std::abs(newDepthLimit - editor.m_vipresDepthLimit) > 0.001f) {
            editor.m_vipresDepthLimit = newDepthLimit;
            editor.UpdatePointCloud();
            editor.SaveSafeParameters();
        }
        
        nk_layout_row_dynamic(ctx, UI_SPACING_SMALL, 1);
        nk_spacing(ctx, 1);
        
        // TOF Sampling Rate (log scale, 1-100 Hz) with value on same line, slider below
        nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
        nk_layout_row_template_push_static(ctx, 200);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);
        char samplingLabelText[128];
        std::snprintf(samplingLabelText, sizeof(samplingLabelText), "TOF Sampling Rate (Hz): %.1f", editor.m_tofSamplingRate);
        nk_label(ctx, samplingLabelText, NK_TEXT_LEFT);
        
        nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
        // Log scale slider: map TOF_SAMPLING_RATE_MIN to TOF_SAMPLING_RATE_MAX Hz on log scale
        float samplingMin = TOF_SAMPLING_RATE_MIN;
        float samplingMax = TOF_SAMPLING_RATE_MAX;
        float currentSampling = editor.m_tofSamplingRate;
        int samplingSliderValue = 0;
        
        if (currentSampling > 0.9f) {
            float samplingLogMin = std::log10(samplingMin);
            float samplingLogMax = std::log10(samplingMax);
            float samplingCurrentLog = std::log10(currentSampling);
            float samplingNormalized = (samplingCurrentLog - samplingLogMin) / (samplingLogMax - samplingLogMin);
            samplingSliderValue = static_cast<int>(samplingNormalized * 100.0f); // 0-100 range
            samplingSliderValue = glm::clamp(samplingSliderValue, 0, 100);
        }
        
        nk_slider_int(ctx, 0, &samplingSliderValue, 100, 1);
        
        float newSamplingRate = samplingMin;
        if (samplingSliderValue > 0) {
            float samplingNormalized = samplingSliderValue / 100.0f; // 0-1 range
            float samplingLogMin = std::log10(samplingMin);
            float samplingLogMax = std::log10(samplingMax);
            float samplingNewLog = samplingLogMin + samplingNormalized * (samplingLogMax - samplingLogMin);
            newSamplingRate = std::pow(10.0f, samplingNewLog);
        } else {
            newSamplingRate = samplingMin;
        }
        
        if (std::abs(newSamplingRate - editor.m_tofSamplingRate) > 0.1f) {
            editor.m_tofSamplingRate = newSamplingRate;
            // Recompute filter coefficients when sampling rate changes
            editor.ComputeLowPassFilterCoefficients(editor.m_tofLowPassCutoff, editor.m_tofSamplingRate);
            // Reuse existing filter buffers - don't clear state, just update coefficients
            editor.UpdatePointCloud();
            editor.SaveSafeParameters();
        }
        
        nk_layout_row_dynamic(ctx, UI_SPACING_SMALL, 1);
        nk_spacing(ctx, 1);
        
        // Gridding toggle
        nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
        bool griddingEnabled = editor.m_vipresGriddingEnabled;
        nk_bool checkboxState = griddingEnabled ? nk_false : nk_true; // Inverted logic
        if (nk_checkbox_label(ctx, "Enable Gridding", &checkboxState)) {
            editor.m_vipresGriddingEnabled = (checkboxState == nk_false);
            // Disable low-pass filter and TSDF when gridding is disabled
            if (!editor.m_vipresGriddingEnabled) {
                editor.m_tofLowPassEnabled = false;
                editor.m_tofTsdfEnabled = false;
                editor.ClearTSDF();
            }
            editor.UpdatePointCloud();
            editor.SaveSafeParameters();
        }
        
        // Grid controls (only show when gridding is enabled)
        if (editor.m_vipresGriddingEnabled) {
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
            nk_label(ctx, "Grid Mode:", NK_TEXT_LEFT);
            
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
            int modeInt = static_cast<int>(editor.m_vipresGridMode);
            const char* modes[] = { "Average", "First Come", "Grid Value" };
            int selected = nk_combo(ctx, modes, 3, modeInt, 25, nk_vec2(200, 200));
            if (selected != modeInt) {
                editor.m_vipresGridMode = static_cast<TOFGridMode>(selected);
                editor.UpdatePointCloud();
                editor.SaveSafeParameters();
            }
            
            // Grid Size with value on same line, slider below
            nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
            nk_layout_row_template_push_static(ctx, 150);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_end(ctx);
            char gridSizeLabelText[128];
            std::snprintf(gridSizeLabelText, sizeof(gridSizeLabelText), "Grid Size: %.3f", editor.m_vipresGridSize);
            nk_label(ctx, gridSizeLabelText, NK_TEXT_LEFT);
            
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
            // Log scale slider: map GRID_SIZE_MIN to GRID_SIZE_MAX on log scale
            float gridLogMin = std::log10(GRID_SIZE_MIN);
            float gridLogMax = std::log10(GRID_SIZE_MAX);
            float currentGridLog = std::log10(editor.m_vipresGridSize);
            float gridNormalized = (currentGridLog - gridLogMin) / (gridLogMax - gridLogMin);
            int gridSliderValue = static_cast<int>(gridNormalized * 100.0f);
            gridSliderValue = glm::clamp(gridSliderValue, 0, 100);
            
            nk_slider_int(ctx, 0, &gridSliderValue, 100, 1);
            
            gridNormalized = gridSliderValue / 100.0f;
            float newGridLog = gridLogMin + gridNormalized * (gridLogMax - gridLogMin);
            float newGridSize = std::pow(10.0f, newGridLog);
            
            if (std::abs(newGridSize - editor.m_vipresGridSize) > 0.001f) {
                editor.m_vipresGridSize = newGridSize;
                editor.UpdatePointCloud(); // Rebuild point cloud with new grid size
                editor.SaveSafeParameters();
            }
            
            // Smoothing method selection (Low-Pass or TSDF) - only enabled when gridding is enabled
            nk_layout_row_dynamic(ctx, 3, 1);
            nk_spacing(ctx, 1);
            
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
            bool lowPassEnabled = editor.m_tofLowPassEnabled && editor.m_vipresGriddingEnabled && !editor.m_tofTsdfEnabled;
            nk_bool lowPassCheckboxState = lowPassEnabled ? nk_false : nk_true; // Inverted logic
            if (nk_checkbox_label(ctx, "Enable Low-Pass Filter", &lowPassCheckboxState)) {
                editor.m_tofLowPassEnabled = (lowPassCheckboxState == nk_false);
                if (editor.m_tofLowPassEnabled) {
                    editor.m_tofTsdfEnabled = false;  // Mutually exclusive
                    editor.ClearTSDF();  // Clear TSDF when switching to low-pass
                }
                editor.UpdatePointCloud();
                editor.SaveSafeParameters();
            }
            
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
            bool tsdfEnabled = editor.m_tofTsdfEnabled && editor.m_vipresGriddingEnabled && !editor.m_tofLowPassEnabled;
            nk_bool tsdfCheckboxState = tsdfEnabled ? nk_false : nk_true; // Inverted logic
            if (nk_checkbox_label(ctx, "Enable TSDF", &tsdfCheckboxState)) {
                editor.m_tofTsdfEnabled = (tsdfCheckboxState == nk_false);
                if (editor.m_tofTsdfEnabled) {
                    editor.m_tofLowPassEnabled = false;  // Mutually exclusive
                    // Clear low-pass filter state when switching to TSDF
                    editor.m_tofProcessing.ClearLowPassFilterState();
                } else {
                    editor.ClearTSDF();  // Clear TSDF when disabling
                }
                editor.UpdatePointCloud();
                editor.SaveSafeParameters();
            }
            
            // TSDF Store Color toggle (only show when TSDF is enabled)
            if (editor.m_tofTsdfEnabled && editor.m_vipresGriddingEnabled) {
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                bool tsdfStoreColorEnabled = editor.m_tofTsdfStoreColor;
                nk_bool tsdfStoreColorCheckboxState = tsdfStoreColorEnabled ? nk_false : nk_true; // Inverted logic
                if (nk_checkbox_label(ctx, "TSDF Store Color", &tsdfStoreColorCheckboxState)) {
                    editor.m_tofTsdfStoreColor = (tsdfStoreColorCheckboxState == nk_false);
                    // Clear TSDF to rebuild with new color setting
                    editor.ClearTSDF();
                    editor.UpdatePointCloud();
                    editor.SaveSafeParameters();
                }
            }
            
            // TSDF controls (only show when enabled and gridding is enabled)
            if (editor.m_tofTsdfEnabled && editor.m_vipresGriddingEnabled) {
                // TSDF Truncation Distance with value on same line, slider below
                nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                nk_layout_row_template_push_static(ctx, 200);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char truncLabelText[128];
                std::snprintf(truncLabelText, sizeof(truncLabelText), "TSDF Truncation: %.2f", editor.m_tofTsdfTruncationDistance);
                nk_label(ctx, truncLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                float truncation = editor.m_tofTsdfTruncationDistance;
                nk_slider_float(ctx, TSDF_TRUNCATION_DISTANCE_MIN, &truncation, TSDF_TRUNCATION_DISTANCE_MAX, 0.1f);
                if (std::abs(truncation - editor.m_tofTsdfTruncationDistance) > 0.01f) {
                    editor.m_tofTsdfTruncationDistance = truncation;
                    editor.UpdatePointCloud();
                    editor.SaveSafeParameters();
                }
                
                // TSDF Weight Decay with value on same line, slider below
                nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                nk_layout_row_template_push_static(ctx, 200);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char decayLabelText[128];
                std::snprintf(decayLabelText, sizeof(decayLabelText), "TSDF Weight Decay: %.3f", editor.m_tofTsdfWeightDecay);
                nk_label(ctx, decayLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                float decay = editor.m_tofTsdfWeightDecay;
                nk_slider_float(ctx, TSDF_WEIGHT_DECAY_MIN, &decay, TSDF_WEIGHT_DECAY_MAX, 0.01f);
                if (std::abs(decay - editor.m_tofTsdfWeightDecay) > 0.001f) {
                    editor.m_tofTsdfWeightDecay = decay;
                    editor.UpdatePointCloud();
                    editor.SaveSafeParameters();
                }
            }
            
            // Low-pass filter controls (only show when enabled and gridding is enabled)
            if (editor.m_tofLowPassEnabled && editor.m_vipresGriddingEnabled) {
                // Low-pass filter cutoff frequency with value on same line, slider below
                nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                nk_layout_row_template_push_static(ctx, 200);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char cutoffLabelText[128];
                std::snprintf(cutoffLabelText, sizeof(cutoffLabelText), "Low-Pass Cutoff (Hz): %.1f", editor.m_tofLowPassCutoff);
                nk_label(ctx, cutoffLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                float cutoff = editor.m_tofLowPassCutoff;
                nk_slider_float(ctx, LOW_PASS_CUTOFF_MIN, &cutoff, LOW_PASS_CUTOFF_MAX, 0.1f);
            if (std::abs(cutoff - editor.m_tofLowPassCutoff) > 0.01f) {
                editor.m_tofLowPassCutoff = cutoff;
                // Recompute filter coefficients when cutoff changes
                editor.ComputeLowPassFilterCoefficients(editor.m_tofLowPassCutoff, editor.m_tofSamplingRate);
                // Reuse existing filter buffers - don't clear state, just update coefficients
                editor.UpdatePointCloud();
                editor.SaveSafeParameters();
            }
            }
            
            // Smart Occupancy toggle (4th step in TOF pipeline)
            nk_layout_row_dynamic(ctx, 3, 1);
            nk_spacing(ctx, 1);
            
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
            bool smartOccupancyEnabled = editor.m_smartOccupancyEnabled;
            nk_bool smartOccupancyCheckboxState = smartOccupancyEnabled ? nk_false : nk_true;
            if (nk_checkbox_label(ctx, "Enable Smart Occupancy", &smartOccupancyCheckboxState)) {
                editor.m_smartOccupancyEnabled = (smartOccupancyCheckboxState == nk_false);
                editor.UpdatePointCloud();
                editor.SaveSafeParameters();
            }
            
            if (editor.m_smartOccupancyEnabled) {
                // Weighing category
                nk_layout_row_dynamic(ctx, 3, 1);
                nk_spacing(ctx, 1);
                nk_layout_row_dynamic(ctx, 20, 1);
                nk_label(ctx, "Weighing Algorithms:", NK_TEXT_LEFT);
                
                // Neighbor Consistency algorithm
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                bool neighborConsistencyEnabled = editor.m_smartOccupancyNeighborConsistency;
                nk_bool neighborConsistencyCheckboxState = neighborConsistencyEnabled ? nk_false : nk_true;
                if (nk_checkbox_label(ctx, "Neighbor Consistency", &neighborConsistencyCheckboxState)) {
                    editor.m_smartOccupancyNeighborConsistency = (neighborConsistencyCheckboxState == nk_false);
                    editor.UpdatePointCloud();
                    editor.SaveSafeParameters();
                }
                
                if (editor.m_smartOccupancyNeighborConsistency) {
                    // Neighbor radius slider
                    nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                    nk_layout_row_template_push_static(ctx, 200);
                    nk_layout_row_template_push_dynamic(ctx);
                    nk_layout_row_template_end(ctx);
                    char neighborRadiusLabelText[128];
                    std::snprintf(neighborRadiusLabelText, sizeof(neighborRadiusLabelText), "Neighbor Radius: %.1f", editor.m_smartOccupancyNeighborRadius);
                    nk_label(ctx, neighborRadiusLabelText, NK_TEXT_LEFT);
                    
                    nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                    float neighborRadius = editor.m_smartOccupancyNeighborRadius;
                    nk_slider_float(ctx, OCCUPANCY_NEIGHBOR_RADIUS_MIN, &neighborRadius, 
                                   OCCUPANCY_NEIGHBOR_RADIUS_MAX, OCCUPANCY_NEIGHBOR_RADIUS_STEP);
                    if (std::abs(neighborRadius - editor.m_smartOccupancyNeighborRadius) > 0.01f) {
                        editor.m_smartOccupancyNeighborRadius = neighborRadius;
                        editor.UpdatePointCloud();
                        editor.SaveSafeParameters();
                    }
                    
                    // Residual threshold (r0) slider
                    nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                    nk_layout_row_template_push_static(ctx, 200);
                    nk_layout_row_template_push_dynamic(ctx);
                    nk_layout_row_template_end(ctx);
                    char residualThresholdLabelText[128];
                    std::snprintf(residualThresholdLabelText, sizeof(residualThresholdLabelText), "r0 (Neighbor Deviation): %.2f", editor.m_smartOccupancyResidualThreshold);
                    nk_label(ctx, residualThresholdLabelText, NK_TEXT_LEFT);
                    
                    nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                    float residualThreshold = editor.m_smartOccupancyResidualThreshold;
                    nk_slider_float(ctx, OCCUPANCY_RESIDUAL_THRESHOLD_MIN, &residualThreshold, 
                                   OCCUPANCY_RESIDUAL_THRESHOLD_MAX, OCCUPANCY_RESIDUAL_THRESHOLD_STEP);
                    if (std::abs(residualThreshold - editor.m_smartOccupancyResidualThreshold) > 0.01f) {
                        editor.m_smartOccupancyResidualThreshold = residualThreshold;
                        editor.UpdatePointCloud();
                        editor.SaveSafeParameters();
                    }
                }
                
                // Persistence algorithm
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                bool persistenceEnabled = editor.m_smartOccupancyPersistence;
                nk_bool persistenceCheckboxState = persistenceEnabled ? nk_false : nk_true;
                if (nk_checkbox_label(ctx, "Persistence", &persistenceCheckboxState)) {
                    editor.m_smartOccupancyPersistence = (persistenceCheckboxState == nk_false);
                    editor.UpdatePointCloud();
                    editor.SaveSafeParameters();
                }
                
                if (editor.m_smartOccupancyPersistence) {
                    // Persistence gamma (γ) slider
                    nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                    nk_layout_row_template_push_static(ctx, 200);
                    nk_layout_row_template_push_dynamic(ctx);
                    nk_layout_row_template_end(ctx);
                    char persistenceGammaLabelText[128];
                    std::snprintf(persistenceGammaLabelText, sizeof(persistenceGammaLabelText), "γ (Persistence Sharpness): %.1f", editor.m_smartOccupancyPersistenceGamma);
                    nk_label(ctx, persistenceGammaLabelText, NK_TEXT_LEFT);
                    
                    nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                    float persistenceGamma = editor.m_smartOccupancyPersistenceGamma;
                    nk_slider_float(ctx, OCCUPANCY_PERSISTENCE_GAMMA_MIN, &persistenceGamma, 
                                   OCCUPANCY_PERSISTENCE_GAMMA_MAX, OCCUPANCY_PERSISTENCE_GAMMA_STEP);
                    if (std::abs(persistenceGamma - editor.m_smartOccupancyPersistenceGamma) > 0.01f) {
                        editor.m_smartOccupancyPersistenceGamma = persistenceGamma;
                        editor.UpdatePointCloud();
                        editor.SaveSafeParameters();
                    }
                }
                
                // Variance-based algorithm
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                bool varianceEnabled = editor.m_smartOccupancyVariance;
                nk_bool varianceCheckboxState = varianceEnabled ? nk_false : nk_true;
                if (nk_checkbox_label(ctx, "Variance-based", &varianceCheckboxState)) {
                    editor.m_smartOccupancyVariance = (varianceCheckboxState == nk_false);
                    editor.UpdatePointCloud();
                    editor.SaveSafeParameters();
                }
                
                if (editor.m_smartOccupancyVariance) {
                    // Variance threshold (σ0) slider
                    nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                    nk_layout_row_template_push_static(ctx, 200);
                    nk_layout_row_template_push_dynamic(ctx);
                    nk_layout_row_template_end(ctx);
                    char varianceThresholdLabelText[128];
                    std::snprintf(varianceThresholdLabelText, sizeof(varianceThresholdLabelText), "σ0 (Allowed Jitter): %.2f", editor.m_smartOccupancyVarianceThreshold);
                    nk_label(ctx, varianceThresholdLabelText, NK_TEXT_LEFT);
                    
                    nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                    float varianceThreshold = editor.m_smartOccupancyVarianceThreshold;
                    nk_slider_float(ctx, OCCUPANCY_VARIANCE_THRESHOLD_MIN, &varianceThreshold, 
                                   OCCUPANCY_VARIANCE_THRESHOLD_MAX, OCCUPANCY_VARIANCE_THRESHOLD_STEP);
                    if (std::abs(varianceThreshold - editor.m_smartOccupancyVarianceThreshold) > 0.01f) {
                        editor.m_smartOccupancyVarianceThreshold = varianceThreshold;
                        editor.UpdatePointCloud();
                        editor.SaveSafeParameters();
                    }
                }
                
                // Eviction-based algorithm
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                bool evictionEnabled = editor.m_smartOccupancyEviction;
                nk_bool evictionCheckboxState = evictionEnabled ? nk_false : nk_true;
                if (nk_checkbox_label(ctx, "Eviction-based", &evictionCheckboxState)) {
                    editor.m_smartOccupancyEviction = (evictionCheckboxState == nk_false);
                    editor.UpdatePointCloud();
                    editor.SaveSafeParameters();
                }
                
                if (editor.m_smartOccupancyEviction) {
                    // Decay factor (a) slider
                    nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                    nk_layout_row_template_push_static(ctx, 200);
                    nk_layout_row_template_push_dynamic(ctx);
                    nk_layout_row_template_end(ctx);
                    char evictionDecayLabelText[128];
                    std::snprintf(evictionDecayLabelText, sizeof(evictionDecayLabelText), "a (Decay): %.3f", editor.m_smartOccupancyEvictionDecay);
                    nk_label(ctx, evictionDecayLabelText, NK_TEXT_LEFT);
                    
                    nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                    float evictionDecay = editor.m_smartOccupancyEvictionDecay;
                    nk_slider_float(ctx, OCCUPANCY_EVICTION_DECAY_MIN, &evictionDecay, 
                                   OCCUPANCY_EVICTION_DECAY_MAX, OCCUPANCY_EVICTION_DECAY_STEP);
                    if (std::abs(evictionDecay - editor.m_smartOccupancyEvictionDecay) > 0.001f) {
                        editor.m_smartOccupancyEvictionDecay = evictionDecay;
                        editor.UpdatePointCloud();
                        editor.SaveSafeParameters();
                    }
                    
                    // Increment factor (b) slider
                    nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                    nk_layout_row_template_push_static(ctx, 200);
                    nk_layout_row_template_push_dynamic(ctx);
                    nk_layout_row_template_end(ctx);
                    char evictionIncrementLabelText[128];
                    std::snprintf(evictionIncrementLabelText, sizeof(evictionIncrementLabelText), "b (Increment): %.3f", editor.m_smartOccupancyEvictionIncrement);
                    nk_label(ctx, evictionIncrementLabelText, NK_TEXT_LEFT);
                    
                    nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                    float evictionIncrement = editor.m_smartOccupancyEvictionIncrement;
                    nk_slider_float(ctx, OCCUPANCY_EVICTION_INCREMENT_MIN, &evictionIncrement, 
                                   OCCUPANCY_EVICTION_INCREMENT_MAX, OCCUPANCY_EVICTION_INCREMENT_STEP);
                    if (std::abs(evictionIncrement - editor.m_smartOccupancyEvictionIncrement) > 0.001f) {
                        editor.m_smartOccupancyEvictionIncrement = evictionIncrement;
                        editor.UpdatePointCloud();
                        editor.SaveSafeParameters();
                    }
                    
                    // Min Start Value slider
                    nk_layout_row_dynamic(ctx, 20, 1);
                    char evictionMinStartLabelText[128];
                    std::snprintf(evictionMinStartLabelText, sizeof(evictionMinStartLabelText), "Min Start Value: %.3f", editor.m_smartOccupancyEvictionMinStartValue);
                    nk_label(ctx, evictionMinStartLabelText, NK_TEXT_LEFT);
                    
                    nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                    float evictionMinStartValue = editor.m_smartOccupancyEvictionMinStartValue;
                    nk_slider_float(ctx, OCCUPANCY_EVICTION_MIN_START_MIN, &evictionMinStartValue, 
                                   OCCUPANCY_EVICTION_MIN_START_MAX, OCCUPANCY_EVICTION_MIN_START_STEP);
                    if (std::abs(evictionMinStartValue - editor.m_smartOccupancyEvictionMinStartValue) > 0.001f) {
                        editor.m_smartOccupancyEvictionMinStartValue = evictionMinStartValue;
                        editor.UpdatePointCloud();
                        editor.SaveSafeParameters();
                    }
                }
                
                // Eviction-based-2 algorithm
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                bool eviction2Enabled = editor.m_smartOccupancyEviction2;
                nk_bool eviction2CheckboxState = eviction2Enabled ? nk_false : nk_true;
                if (nk_checkbox_label(ctx, "Eviction-based-2", &eviction2CheckboxState)) {
                    editor.m_smartOccupancyEviction2 = (eviction2CheckboxState == nk_false);
                    editor.UpdatePointCloud();
                    editor.SaveSafeParameters();
                }
                
                if (editor.m_smartOccupancyEviction2) {
                    // Radius (r) slider
                    nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                    nk_layout_row_template_push_static(ctx, 200);
                    nk_layout_row_template_push_dynamic(ctx);
                    nk_layout_row_template_end(ctx);
                    char eviction2RadiusLabelText[128];
                    std::snprintf(eviction2RadiusLabelText, sizeof(eviction2RadiusLabelText), "r (Radius): %.2f", editor.m_smartOccupancyEviction2Radius);
                    nk_label(ctx, eviction2RadiusLabelText, NK_TEXT_LEFT);
                    
                    nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                    float eviction2Radius = editor.m_smartOccupancyEviction2Radius;
                    nk_slider_float(ctx, OCCUPANCY_EVICTION2_RADIUS_MIN, &eviction2Radius, 
                                   OCCUPANCY_EVICTION2_RADIUS_MAX, OCCUPANCY_EVICTION2_RADIUS_STEP);
                    if (std::abs(eviction2Radius - editor.m_smartOccupancyEviction2Radius) > 0.01f) {
                        editor.m_smartOccupancyEviction2Radius = eviction2Radius;
                        // Force recomputation of neighbor offsets
                        editor.m_occupancyState.eviction2CachedRadius = -1.0f;
                        editor.UpdatePointCloud();
                        editor.SaveSafeParameters();
                    }
                    
                    // Min neighbors slider
                    nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                    nk_layout_row_template_push_static(ctx, 200);
                    nk_layout_row_template_push_dynamic(ctx);
                    nk_layout_row_template_end(ctx);
                    char eviction2MinNeighborsLabelText[128];
                    std::snprintf(eviction2MinNeighborsLabelText, sizeof(eviction2MinNeighborsLabelText), "Min Neighbors: %d", editor.m_smartOccupancyEviction2MinNeighbors);
                    nk_label(ctx, eviction2MinNeighborsLabelText, NK_TEXT_LEFT);
                    
                    nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                    int eviction2MinNeighbors = editor.m_smartOccupancyEviction2MinNeighbors;
                    nk_slider_int(ctx, OCCUPANCY_EVICTION2_MIN_NEIGHBORS_MIN, &eviction2MinNeighbors, 
                                  OCCUPANCY_EVICTION2_MIN_NEIGHBORS_MAX, 1);
                    if (eviction2MinNeighbors != editor.m_smartOccupancyEviction2MinNeighbors) {
                        editor.m_smartOccupancyEviction2MinNeighbors = eviction2MinNeighbors;
                        editor.UpdatePointCloud();
                        editor.SaveSafeParameters();
                    }
                }
                
                // Weight threshold slider (for filtering points before rendering)
                nk_layout_row_dynamic(ctx, 3, 1);
                nk_spacing(ctx, 1);
                
                nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                nk_layout_row_template_push_static(ctx, 200);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char weightThresholdLabelText[128];
                std::snprintf(weightThresholdLabelText, sizeof(weightThresholdLabelText), "Weight Threshold: %.3f", editor.m_weightThreshold);
                nk_label(ctx, weightThresholdLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                float weightThreshold = editor.m_weightThreshold;
                nk_slider_float(ctx, OCCUPANCY_WEIGHT_THRESHOLD_MIN, &weightThreshold, 
                               OCCUPANCY_WEIGHT_THRESHOLD_MAX, OCCUPANCY_WEIGHT_THRESHOLD_STEP);
                if (std::abs(weightThreshold - editor.m_weightThreshold) > 0.001f) {
                    editor.m_weightThreshold = weightThreshold;
                    // No need to update point cloud, just re-render
                    editor.SaveSafeParameters();
                }
            }
        }
        
        nk_layout_row_dynamic(ctx, UI_SPACING_SMALL, 1);
        nk_spacing(ctx, 1);
        
        // Noisy TOF toggle with noise value on same line
        nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
        nk_layout_row_template_push_static(ctx, 150);  // Checkbox + label width
        nk_layout_row_template_push_dynamic(ctx);      // Value width
        nk_layout_row_template_end(ctx);
        bool noisyTofEnabled = editor.m_vipresNoisyTofEnabled;
        nk_bool noisyTofCheckboxState = noisyTofEnabled ? nk_false : nk_true; // Inverted logic
        if (nk_checkbox_label(ctx, "Enable Noisy TOF", &noisyTofCheckboxState)) {
            editor.m_vipresNoisyTofEnabled = (noisyTofCheckboxState == nk_false);
            editor.SaveSafeParameters();
        }
        char noisyTofValueText[64];
        std::snprintf(noisyTofValueText, sizeof(noisyTofValueText), "±%.3f", editor.m_vipresNoiseAmount);
        nk_label(ctx, noisyTofValueText, NK_TEXT_LEFT);
        
        // Noise amount slider (only show when noisy TOF is enabled)
        if (editor.m_vipresNoisyTofEnabled) {
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
            // Log scale slider: map 0-100 to NOISE_AMOUNT_MIN to NOISE_AMOUNT_MAX on log scale
            // 0 maps to 0, 1-100 maps to log scale from NOISE_AMOUNT_MIN to NOISE_AMOUNT_MAX
            float noiseMin = NOISE_AMOUNT_MIN;
            float noiseMax = NOISE_AMOUNT_MAX;
            float currentNoise = editor.m_vipresNoiseAmount;
            int noiseSliderValue = 0;
            
            if (currentNoise > 0.0001f) {
                float noiseLogMin = std::log10(noiseMin);
                float noiseLogMax = std::log10(noiseMax);
                float noiseCurrentLog = std::log10(currentNoise);
                float noiseNormalized = (noiseCurrentLog - noiseLogMin) / (noiseLogMax - noiseLogMin);
                noiseSliderValue = static_cast<int>(noiseNormalized * 99.0f) + 1; // 1-100 range
                noiseSliderValue = glm::clamp(noiseSliderValue, 1, 100);
            }
            
            nk_slider_int(ctx, 0, &noiseSliderValue, 100, 1);
            
            float newNoiseAmount = 0.0f;
            if (noiseSliderValue == 0) {
                newNoiseAmount = 0.0f;
            } else {
                float noiseNormalized = (noiseSliderValue - 1) / 99.0f; // 0-1 range
                float noiseLogMin = std::log10(noiseMin);
                float noiseLogMax = std::log10(noiseMax);
                float noiseNewLog = noiseLogMin + noiseNormalized * (noiseLogMax - noiseLogMin);
                newNoiseAmount = std::pow(10.0f, noiseNewLog);
            }
            
            if (std::abs(newNoiseAmount - editor.m_vipresNoiseAmount) > 0.0001f) {
                editor.m_vipresNoiseAmount = newNoiseAmount;
                editor.SaveSafeParameters();
            }
        }
        
        // Camera enable/disable list
        nk_layout_row_dynamic(ctx, 20, 1);
        nk_label(ctx, "Cameras:", NK_TEXT_LEFT);
        
        // Ensure vector size matches camera count
        if (editor.m_vipresCameraEnabled.size() != editor.m_cameras.size()) {
            size_t oldSize = editor.m_vipresCameraEnabled.size();
            editor.m_vipresCameraEnabled.resize(editor.m_cameras.size(), true);
            // Explicitly set any new entries to enabled
            for (size_t i = oldSize; i < editor.m_cameras.size(); i++) {
                editor.m_vipresCameraEnabled[i] = true;
            }
        }
        
        // Show only anchor cameras (canMove=false)
        for (size_t i = 0; i < editor.m_cameras.size(); i++) {
            const Camera& cam = editor.m_cameras[i];
            if (cam.canMove) {
                continue; // Skip free cameras
            }
            
            nk_layout_row_dynamic(ctx, 20, 1);
            // Read current enabled state (default to true if vector is not properly sized)
            // editor.m_vipresCameraEnabled[i] = true means camera IS enabled (depth data IS used)
            bool currentEnabled = (i < editor.m_vipresCameraEnabled.size()) ? editor.m_vipresCameraEnabled[i] : true;
            
            // INVERTED LOGIC: Checkbox checked (ON) = camera DISABLED, unchecked (OFF) = camera ENABLED
            // So if camera is enabled, show checkbox as unchecked (nk_false)
            // If camera is disabled, show checkbox as checked (nk_true)
            nk_bool cameraCheckboxState = currentEnabled ? nk_false : nk_true;
            
            // Display checkbox - checked means DISABLED, unchecked means ENABLED (inverted)
            if (nk_checkbox_label(ctx, cam.name.c_str(), &cameraCheckboxState)) {
                // Checkbox was clicked - update the state with INVERTED logic
                // nk_true (checked/ON) = camera DISABLED (depth data NOT used)
                // nk_false (unchecked/OFF) = camera ENABLED (depth data IS used)
                bool shouldBeEnabled = (cameraCheckboxState == nk_false); // Inverted: unchecked = enabled
                
                // Ensure vector is properly sized
                if (i >= editor.m_vipresCameraEnabled.size()) {
                    editor.m_vipresCameraEnabled.resize(i + 1, true);
                }
                
                // Set the state - true means enabled (use depth data), false means disabled (skip depth data)
                editor.m_vipresCameraEnabled[i] = shouldBeEnabled;
                
                editor.UpdatePointCloud(); // Update immediately when toggled
                editor.SaveSafeParameters();
            }
        }
        
    }
    nk_end(ctx);
}

void CameraSystemUI::RenderVIPRESControls(CameraSystemEditor& editor) {
    struct nk_context* ctx = m_nuklearUI->GetContext();
    if (!ctx) return;
    
    if (!editor.m_vipresEnabled) return;
    
    float vipresWindowHeight = editor.m_vipresRemeshEnabled ? UI_VIPRES_CONTROLS_HEIGHT_REMESH : UI_VIPRES_CONTROLS_HEIGHT_MIN;
    
    if (nk_begin(ctx, "VIPRES Controls", 
                 nk_rect(UI_VIPRES_CONTROLS_X, UI_STATUS_BAR_HEIGHT + UI_WINDOW_OFFSET_Y, 
                        UI_VIPRES_CONTROLS_WIDTH, vipresWindowHeight),
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE)) {
        
        // Compute Color toggle
        nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
        bool computeColorEnabled = editor.m_vipresComputeColor;
        nk_bool computeColorCheckboxState = computeColorEnabled ? nk_false : nk_true; // Inverted logic
        if (nk_checkbox_label(ctx, "Compute Color", &computeColorCheckboxState)) {
            editor.m_vipresComputeColor = (computeColorCheckboxState == nk_false);
            // Regenerate point cloud with new color setting (TOF controls point cloud generation)
            if (editor.m_tofDataEnabled) {
                editor.UpdatePointCloud();
            }
            // Regenerate remesh to map the new colors to the mesh
            if (editor.m_vipresRemeshEnabled) {
                editor.m_remeshNeedsUpdate = true;
                editor.GenerateRemesh();
            }
            editor.SaveSafeParameters();
        }
        
        nk_layout_row_dynamic(ctx, UI_SPACING_SMALL, 1);
        nk_spacing(ctx, 1);
        
        // Remesh toggle
        nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
        bool remeshEnabled = editor.m_vipresRemeshEnabled;
        nk_bool remeshCheckboxState = remeshEnabled ? nk_false : nk_true; // Inverted logic
        if (nk_checkbox_label(ctx, "Enable Remesh", &remeshCheckboxState)) {
            editor.m_vipresRemeshEnabled = (remeshCheckboxState == nk_false);
            if (editor.m_vipresRemeshEnabled) {
                editor.m_remeshNeedsUpdate = true;
                editor.GenerateRemesh();
            }
            editor.SaveSafeParameters();
        }
        
        // Auto-recompute mesh toggle (only visible if remesh is enabled)
        if (editor.m_vipresRemeshEnabled) {
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
            bool autoRecompute = editor.m_vipresAutoRecomputeMesh;
            nk_bool autoRecomputeCheckboxState = autoRecompute ? nk_false : nk_true; // Inverted logic
            if (nk_checkbox_label(ctx, "Auto-recompute Mesh (Every Frame)", &autoRecomputeCheckboxState)) {
                editor.m_vipresAutoRecomputeMesh = (autoRecomputeCheckboxState == nk_false);
                editor.SaveSafeParameters();
            }
            
            // Remeshing algorithm selection
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
            nk_label(ctx, "Remeshing Algorithm:", NK_TEXT_LEFT);
            
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
            const char* algorithms[] = {"Gridded Quads", "Nearest Neighbor", "Delaunay 2D", "Poisson-like", "FCTM", "GeodesicTree", "GTree"};
            int currentSelection = editor.m_vipresRemeshAlgorithm;
            int newSelection = nk_combo(ctx, algorithms, 7, currentSelection, 25, nk_vec2(200, 200));
            bool algorithmChanged = false;
            if (newSelection != currentSelection && newSelection >= 0 && newSelection < 7) {
                // Save current algorithm's parameters before switching
                editor.SaveAlgorithmParams(currentSelection);
                
                // Switch to new algorithm
                editor.m_vipresRemeshAlgorithm = newSelection;
                
                // Load new algorithm's saved parameters
                editor.LoadAlgorithmParams(newSelection);
                
                algorithmChanged = true;
                editor.m_remeshNeedsUpdate = true;
                // Immediately regenerate mesh with new algorithm
                editor.GenerateRemesh();
                editor.SaveSafeParameters();
            }
            
            // Max Distance parameter (only for NearestNeighbor and Delaunay2D)
            // Use newSelection if algorithm just changed, otherwise use current value
            int activeAlgorithm = algorithmChanged ? newSelection : editor.m_vipresRemeshAlgorithm;
            if (activeAlgorithm == 1 || activeAlgorithm == 2) {
                nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                nk_layout_row_template_push_static(ctx, 150);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char maxDistLabelText[128];
                std::snprintf(maxDistLabelText, sizeof(maxDistLabelText), "Max Distance: %.2f", editor.m_vipresRemeshMaxDistance);
                nk_label(ctx, maxDistLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                float maxDist = editor.m_vipresRemeshMaxDistance;
                nk_slider_float(ctx, REMESH_MAX_DISTANCE_MIN, &maxDist, REMESH_MAX_DISTANCE_MAX, 0.01f);
                if (std::abs(maxDist - editor.m_vipresRemeshMaxDistance) > 0.001f) {
                    editor.m_vipresRemeshMaxDistance = maxDist;
                    editor.SaveAlgorithmParams(activeAlgorithm);  // Save when slider changes
                    editor.m_remeshNeedsUpdate = true;
                    if (!editor.m_vipresAutoRecomputeMesh) {
                        editor.GenerateRemesh();
                    }
                    editor.SaveSafeParameters();
                }
            }
            
            // Max Neighbors parameter (for NearestNeighbor, Delaunay2D, and PoissonLike)
            if (activeAlgorithm == 1 || activeAlgorithm == 2 || activeAlgorithm == 3) {
                nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                nk_layout_row_template_push_static(ctx, 150);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char maxNeighborsLabelText[128];
                std::snprintf(maxNeighborsLabelText, sizeof(maxNeighborsLabelText), "Max Neighbors: %d", editor.m_vipresRemeshMaxNeighbors);
                nk_label(ctx, maxNeighborsLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                float maxNeighbors = static_cast<float>(editor.m_vipresRemeshMaxNeighbors);
                nk_slider_float(ctx, static_cast<float>(REMESH_MAX_NEIGHBORS_MIN), &maxNeighbors, static_cast<float>(REMESH_MAX_NEIGHBORS_MAX), 1.0f);
                int newMaxNeighbors = static_cast<int>(maxNeighbors + 0.5f);
                if (newMaxNeighbors != editor.m_vipresRemeshMaxNeighbors) {
                    editor.m_vipresRemeshMaxNeighbors = newMaxNeighbors;
                    editor.SaveAlgorithmParams(activeAlgorithm);  // Save when slider changes
                    editor.m_remeshNeedsUpdate = true;
                    if (!editor.m_vipresAutoRecomputeMesh) {
                        editor.GenerateRemesh();
                    }
                    editor.SaveSafeParameters();
                }
            }
            
            // Delaunay2D specific parameters
            if (activeAlgorithm == 2) {
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                nk_label(ctx, "Projection Plane:", NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                const char* planes[] = {"XY", "XZ", "YZ", "Best Fit"};
                int currentPlaneSelection = editor.m_vipresRemeshProjectionPlane;
                int newPlaneSelection = nk_combo(ctx, planes, 4, currentPlaneSelection, 25, nk_vec2(200, 200));
                if (newPlaneSelection != currentPlaneSelection && newPlaneSelection >= 0 && newPlaneSelection < 4) {
                    editor.m_vipresRemeshProjectionPlane = newPlaneSelection;
                    editor.SaveAlgorithmParams(activeAlgorithm);  // Save when selection changes
                    editor.m_remeshNeedsUpdate = true;
                    if (!editor.m_vipresAutoRecomputeMesh) {
                        editor.GenerateRemesh();
                    }
                    editor.SaveSafeParameters();
                }
                
                nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                nk_layout_row_template_push_static(ctx, 150);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char delaunayThresholdLabelText[128];
                std::snprintf(delaunayThresholdLabelText, sizeof(delaunayThresholdLabelText), "Delaunay Threshold: %.2f", editor.m_vipresRemeshDelaunayThreshold);
                nk_label(ctx, delaunayThresholdLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                float threshold = editor.m_vipresRemeshDelaunayThreshold;
                nk_slider_float(ctx, REMESH_DELAUNAY_THRESHOLD_MIN, &threshold, REMESH_DELAUNAY_THRESHOLD_MAX, 0.01f);
                if (std::abs(threshold - editor.m_vipresRemeshDelaunayThreshold) > 0.001f) {
                    editor.m_vipresRemeshDelaunayThreshold = threshold;
                    editor.SaveAlgorithmParams(activeAlgorithm);  // Save when slider changes
                    editor.m_remeshNeedsUpdate = true;
                    if (!editor.m_vipresAutoRecomputeMesh) {
                        editor.GenerateRemesh();
                    }
                    editor.SaveSafeParameters();
                }
            }
            
            // Poisson-like specific parameters
            if (activeAlgorithm == 3) {
                nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                nk_layout_row_template_push_static(ctx, 150);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char poissonRadiusLabelText[128];
                std::snprintf(poissonRadiusLabelText, sizeof(poissonRadiusLabelText), "Poisson Radius: %.2f", editor.m_vipresRemeshPoissonRadius);
                nk_label(ctx, poissonRadiusLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                float poissonRadius = editor.m_vipresRemeshPoissonRadius;
                nk_slider_float(ctx, REMESH_POISSON_RADIUS_MIN, &poissonRadius, REMESH_POISSON_RADIUS_MAX, 0.01f);
                if (std::abs(poissonRadius - editor.m_vipresRemeshPoissonRadius) > 0.001f) {
                    editor.m_vipresRemeshPoissonRadius = poissonRadius;
                    editor.SaveAlgorithmParams(activeAlgorithm);  // Save when slider changes
                    editor.m_remeshNeedsUpdate = true;
                    if (!editor.m_vipresAutoRecomputeMesh) {
                        editor.GenerateRemesh();
                    }
                    editor.SaveSafeParameters();
                }
                
                nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                nk_layout_row_template_push_static(ctx, 150);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char poissonDepthLabelText[128];
                std::snprintf(poissonDepthLabelText, sizeof(poissonDepthLabelText), "Poisson Depth: %d", editor.m_vipresRemeshPoissonDepth);
                nk_label(ctx, poissonDepthLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                float poissonDepth = static_cast<float>(editor.m_vipresRemeshPoissonDepth);
                nk_slider_float(ctx, static_cast<float>(REMESH_POISSON_DEPTH_MIN), &poissonDepth, static_cast<float>(REMESH_POISSON_DEPTH_MAX), 1.0f);
                int newPoissonDepth = static_cast<int>(poissonDepth + 0.5f);
                if (newPoissonDepth != editor.m_vipresRemeshPoissonDepth) {
                    editor.m_vipresRemeshPoissonDepth = newPoissonDepth;
                    editor.SaveAlgorithmParams(activeAlgorithm);  // Save when slider changes
                    editor.m_remeshNeedsUpdate = true;
                    if (!editor.m_vipresAutoRecomputeMesh) {
                        editor.GenerateRemesh();
                    }
                    editor.SaveSafeParameters();
                }
                
                nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                nk_layout_row_template_push_static(ctx, 150);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char poissonDensityLabelText[128];
                std::snprintf(poissonDensityLabelText, sizeof(poissonDensityLabelText), "Poisson Density: %.2f", editor.m_vipresRemeshPoissonDensity);
                nk_label(ctx, poissonDensityLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                float poissonDensity = editor.m_vipresRemeshPoissonDensity;
                nk_slider_float(ctx, REMESH_POISSON_DENSITY_MIN, &poissonDensity, REMESH_POISSON_DENSITY_MAX, 0.1f);
                if (std::abs(poissonDensity - editor.m_vipresRemeshPoissonDensity) > 0.001f) {
                    editor.m_vipresRemeshPoissonDensity = poissonDensity;
                    editor.SaveAlgorithmParams(activeAlgorithm);  // Save when slider changes
                    editor.m_remeshNeedsUpdate = true;
                    if (!editor.m_vipresAutoRecomputeMesh) {
                        editor.GenerateRemesh();
                    }
                    editor.SaveSafeParameters();
                }
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                bool smoothNormals = editor.m_vipresRemeshPoissonSmoothNormals;
                nk_bool smoothNormalsCheckboxState = smoothNormals ? nk_false : nk_true; // Inverted logic
                if (nk_checkbox_label(ctx, "Smooth Normals", &smoothNormalsCheckboxState)) {
                    editor.m_vipresRemeshPoissonSmoothNormals = (smoothNormalsCheckboxState == nk_false);
                    editor.SaveAlgorithmParams(activeAlgorithm);  // Save when checkbox changes
                    editor.m_remeshNeedsUpdate = true;
                    if (!editor.m_vipresAutoRecomputeMesh) {
                        editor.GenerateRemesh();
                    }
                    editor.SaveSafeParameters();
                }
            }
            
            // FCTM specific parameters
            if (activeAlgorithm == 4) {
                nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                nk_layout_row_template_push_static(ctx, 150);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char fctmSliceLabelText[128];
                std::snprintf(fctmSliceLabelText, sizeof(fctmSliceLabelText), "Slice Thickness: %.3f", editor.m_vipresRemeshFCTMSliceThickness);
                nk_label(ctx, fctmSliceLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                float fctmSliceThickness = editor.m_vipresRemeshFCTMSliceThickness;
                nk_slider_float(ctx, REMESH_FCTM_SLICE_THICKNESS_MIN, &fctmSliceThickness, REMESH_FCTM_SLICE_THICKNESS_MAX, 0.01f);
                if (std::abs(fctmSliceThickness - editor.m_vipresRemeshFCTMSliceThickness) > 0.001f) {
                    editor.m_vipresRemeshFCTMSliceThickness = fctmSliceThickness;
                    editor.SaveAlgorithmParams(activeAlgorithm);
                    editor.m_remeshNeedsUpdate = true;
                    if (!editor.m_vipresAutoRecomputeMesh) {
                        editor.GenerateRemesh();
                    }
                    editor.SaveSafeParameters();
                }
            }
            
            // GeodesicTree specific parameters
            if (activeAlgorithm == 5) {
                nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                nk_layout_row_template_push_static(ctx, 150);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char geodesicKNeighborsLabelText[128];
                std::snprintf(geodesicKNeighborsLabelText, sizeof(geodesicKNeighborsLabelText), "K Neighbors: %d", editor.m_vipresRemeshGeodesicKNeighbors);
                nk_label(ctx, geodesicKNeighborsLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                float geodesicKNeighbors = static_cast<float>(editor.m_vipresRemeshGeodesicKNeighbors);
                nk_slider_float(ctx, static_cast<float>(REMESH_GEODESIC_K_NEIGHBORS_MIN), &geodesicKNeighbors, static_cast<float>(REMESH_GEODESIC_K_NEIGHBORS_MAX), 1.0f);
                int newGeodesicKNeighbors = static_cast<int>(geodesicKNeighbors + 0.5f);
                if (newGeodesicKNeighbors != editor.m_vipresRemeshGeodesicKNeighbors) {
                    editor.m_vipresRemeshGeodesicKNeighbors = newGeodesicKNeighbors;
                    editor.SaveAlgorithmParams(activeAlgorithm);
                    editor.m_remeshNeedsUpdate = true;
                    if (!editor.m_vipresAutoRecomputeMesh) {
                        editor.GenerateRemesh();
                    }
                    editor.SaveSafeParameters();
                }
                
                nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
                nk_layout_row_template_push_static(ctx, 150);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char geodesicInteriorBiasLabelText[128];
                std::snprintf(geodesicInteriorBiasLabelText, sizeof(geodesicInteriorBiasLabelText), "Interior Bias: %.2f", editor.m_vipresRemeshGeodesicInteriorBias);
                nk_label(ctx, geodesicInteriorBiasLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
                float geodesicInteriorBias = editor.m_vipresRemeshGeodesicInteriorBias;
                nk_slider_float(ctx, REMESH_GEODESIC_INTERIOR_BIAS_MIN, &geodesicInteriorBias, REMESH_GEODESIC_INTERIOR_BIAS_MAX, 0.01f);
                if (std::abs(geodesicInteriorBias - editor.m_vipresRemeshGeodesicInteriorBias) > 0.001f) {
                    editor.m_vipresRemeshGeodesicInteriorBias = geodesicInteriorBias;
                    editor.SaveAlgorithmParams(activeAlgorithm);
                    editor.m_remeshNeedsUpdate = true;
                    if (!editor.m_vipresAutoRecomputeMesh) {
                        editor.GenerateRemesh();
                    }
                    editor.SaveSafeParameters();
                }
            }
            
            // Apply button to regenerate mesh with current parameters
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
            if (nk_button_label(ctx, "Reapply Remesh")) {
                editor.m_remeshNeedsUpdate = true;
                editor.GenerateRemesh();
            }
        }
        
    }
    nk_end(ctx);
}

void CameraSystemUI::RenderObjectsPanel(CameraSystemEditor& editor) {
    struct nk_context* ctx = m_nuklearUI->GetContext();
    if (!ctx) return;
    
    if (nk_begin(ctx, "Objects", 
                 nk_rect(UI_OBJECTS_PANEL_X, UI_STATUS_BAR_HEIGHT + UI_WINDOW_OFFSET_Y, 
                        UI_OBJECTS_PANEL_WIDTH, UI_OBJECTS_PANEL_HEIGHT),
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE)) {

    nk_layout_row_dynamic(ctx, 25, 1);
   nk_label(ctx, "Loaded Objects:", NK_TEXT_LEFT);
           
           nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
   char objCountText[64];
   std::snprintf(objCountText, sizeof(objCountText), "Count: %zu", editor.m_objects.size());
   nk_label(ctx, objCountText, NK_TEXT_LEFT);
   
   // Apply rainbow colors automatically (no button needed)
   if (!editor.m_objects.empty()) {
       editor.ApplyRainbowColorsToObjects();
   }
   
   // List objects with enable/disable toggles and size sliders
   if (!editor.m_objects.empty()) {
       nk_layout_row_dynamic(ctx, 20, 1);
       nk_label(ctx, "Objects:", NK_TEXT_LEFT);
       
       for (size_t i = 0; i < editor.m_objects.size() && i < 10; i++) { // Limit to 10 for UI
           if (!editor.m_objects[i]) continue;
           
           // Object name with enable/disable toggle on the left
           nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
           nk_layout_row_template_push_static(ctx, 25);  // Toggle checkbox
           nk_layout_row_template_push_dynamic(ctx);      // Object name
           nk_layout_row_template_end(ctx);
           
           // Enable/disable toggle
           bool visible = editor.m_objects[i]->visible;
           nk_bool checkboxState = visible ? nk_false : nk_true; // Inverted logic
           if (nk_checkbox_label(ctx, "", &checkboxState)) {
               editor.m_objects[i]->visible = (checkboxState == nk_false);
           }
           
           // Object name
           char objNameText[128];
           std::snprintf(objNameText, sizeof(objNameText), "%zu. %s", i + 1, editor.m_objects[i]->name.c_str());
           nk_label(ctx, objNameText, NK_TEXT_LEFT);
           
           // Size slider with bounding box display
           if (editor.m_objects[i]->model && editor.m_objects[i]->model->GetMeshCount() > 0) {
               nk_layout_row_dynamic(ctx, 3, 1);
       nk_spacing(ctx, 1);
       
               // Get base bounding box size (unscaled)
               float minX, maxX, minY, maxY, minZ, maxZ;
               editor.m_objects[i]->model->GetBounds(minX, maxX, minY, maxY, minZ, maxZ);
               float baseSizeX = maxX - minX;
               float baseSizeY = maxY - minY;
               float baseSizeZ = maxZ - minZ;
               float baseSize = std::max(baseSizeX, std::max(baseSizeY, baseSizeZ));
               
               if (baseSize < 0.0001f) baseSize = 1.0f; // Avoid division by zero
               
               // Get current scaled bounding box size
               float currentScaleFactor = std::max(editor.m_objects[i]->scale.x, std::max(editor.m_objects[i]->scale.y, editor.m_objects[i]->scale.z));
               float currentSize = currentScaleFactor * baseSize;
               
               // Size slider (1-100 units)
               nk_layout_row_template_begin(ctx, UI_ROW_HEIGHT_STANDARD);
               nk_layout_row_template_push_static(ctx, 200);
               nk_layout_row_template_push_dynamic(ctx);
               nk_layout_row_template_end(ctx);
               char sizeLabelText[128];
               std::snprintf(sizeLabelText, sizeof(sizeLabelText), "Size: %.1f units (BB: %.1f x %.1f x %.1f)", 
                           currentSize,
                           baseSizeX * currentScaleFactor,
                           baseSizeY * currentScaleFactor,
                           baseSizeZ * currentScaleFactor);
               nk_label(ctx, sizeLabelText, NK_TEXT_LEFT);
               
               nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
               // Convert current size to slider value (OBJECT_SIZE_MIN-OBJECT_SIZE_MAX range)
               int sliderValue = static_cast<int>(glm::clamp(currentSize, OBJECT_SIZE_MIN, OBJECT_SIZE_MAX));
               
               nk_slider_int(ctx, static_cast<int>(OBJECT_SIZE_MIN), &sliderValue, static_cast<int>(OBJECT_SIZE_MAX), 1);
               
               // Convert slider value back to scale factor
               float newTargetSize = static_cast<float>(sliderValue);
               float newScaleFactor = newTargetSize / baseSize;
               
               if (std::abs(newScaleFactor - currentScaleFactor) > 0.01f) {
                   // Apply uniform scale to all axes
                   editor.m_objects[i]->scale = glm::vec3(newScaleFactor, newScaleFactor, newScaleFactor);
               }
           }
           
           nk_layout_row_dynamic(ctx, 3, 1);
           nk_spacing(ctx, 1);
       }
   }
   }
   nk_end(ctx);
}

void CameraSystemUI::RenderStatisticsPanel(CameraSystemEditor& editor) {
    struct nk_context* ctx = m_nuklearUI->GetContext();
    if (!ctx) return;
    
    // Show statistics panel when smart occupancy is enabled
    // Note: Occupancy weights are only computed when both occupancy and gridding are enabled
    if (!editor.m_smartOccupancyEnabled) {
        return;  // Don't show panel if occupancy is disabled
    }
    
    if (nk_begin(ctx, "Statistics", 
                 nk_rect(UI_STATISTICS_PANEL_X, 
                        UI_STATUS_BAR_HEIGHT + UI_WINDOW_OFFSET_Y, 
                        UI_STATISTICS_PANEL_WIDTH, UI_STATISTICS_PANEL_HEIGHT),
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE)) {
        
        // Check if gridding is enabled (required for occupancy weights computation)
        if (!editor.m_vipresGriddingEnabled) {
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
            nk_label(ctx, "Gridding must be enabled for occupancy weights", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_SMALL, 1);
            nk_label(ctx, "Enable gridding to see weight histograms", NK_TEXT_LEFT);
            nk_end(ctx);
            return;
        }
        
        // Collect weight data using public method
        std::vector<float> weights = editor.GetPointCloudWeights();
        
        if (!weights.empty()) {
            // Initialize histogram from weights
            HistogramData histogram = InitializeHistogram(weights, 
                                                          "Point Cloud Weight Distribution",
                                                          "Weight",
                                                          20,
                                                          0.0f,
                                                          1.0f);
            
            // Render histogram
            RenderHistogram(ctx, histogram);
        } else {
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_STANDARD, 1);
            nk_label(ctx, "No point cloud data available", NK_TEXT_LEFT);
            nk_layout_row_dynamic(ctx, UI_ROW_HEIGHT_SMALL, 1);
            nk_label(ctx, "Enable TOF data to see weight histograms", NK_TEXT_LEFT);
        }
    }
    nk_end(ctx);
}

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

