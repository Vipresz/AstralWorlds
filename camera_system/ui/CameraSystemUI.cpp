#include "CameraSystemUI.h"
#include "../CameraSystemEditor.h"
#include "../GlobalConstants.h"
#include "UIHelpers.h"
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
    const float statusBarHeight = 30.0f;
    if (nk_begin(ctx, "Status Bar", nk_rect(0.0f, 0.0f, static_cast<float>(width), statusBarHeight),
                 NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_BACKGROUND)) {
        
        // Create status text with FPS on the right
        std::string statusText = "Camera System Editor";
        if (editor.m_configLoaded) {
            statusText += " | Config: Loaded";
            if (editor.m_isPlaying) {
                statusText += " | Playing";
            } else {
                statusText += " | Paused";
            }
            statusText += " | Frame: " + std::to_string(editor.m_currentFrame) + "/" + std::to_string(editor.m_totalFrames - 1);
            if (editor.m_captureEnabled) {
                statusText += " | Capture: ON";
            } else {
                statusText += " | Capture: OFF";
            }
        } else {
            statusText += " | No Config Loaded";
        }
        
        // Add FPS on the right side (display as Hz for accuracy)
        char fpsText[32];
        std::snprintf(fpsText, sizeof(fpsText), "%.1f Hz", editor.m_currentFPS);
        
        // Use a simpler layout: two columns, left for status, right for FPS
        nk_layout_row_begin(ctx, NK_STATIC, statusBarHeight, 2);
        
        // Left column: status text (takes remaining space)
        float fpsColumnWidth = 100.0f; // Fixed width for FPS column
        nk_layout_row_push(ctx, static_cast<float>(width) - fpsColumnWidth);
        nk_label(ctx, statusText.c_str(), NK_TEXT_LEFT);
        
        // Right column: FPS (fixed width, right-aligned)
        nk_layout_row_push(ctx, fpsColumnWidth);
        nk_label(ctx, fpsText, NK_TEXT_RIGHT);
        nk_layout_row_end(ctx);
    }
    nk_end(ctx);
}

void CameraSystemUI::RenderCameraControls(CameraSystemEditor& editor) {
    struct nk_context* ctx = m_nuklearUI->GetContext();
    if (!ctx) return;
    
    const float statusBarHeight = 30.0f;
    
    // Camera control panel - resizable with scrollbar (offset by status bar height)
    // Match old version exactly: NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE
    if (nk_begin(ctx, "Camera Controls", nk_rect(10.0f, statusBarHeight + 10.0f, 350.0f, 400.0f),
                    NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE)) {
        
        // Display active camera name
        nk_layout_row_dynamic(ctx, 25, 1);
        std::string activeCameraName = "None";
        size_t activeIdx = editor.m_cameraSystem.GetActiveCameraIndex();
        if (activeIdx < editor.m_cameras.size()) {
            activeCameraName = editor.m_cameras[activeIdx].name;
        }
        nk_label(ctx, ("Active: " + activeCameraName).c_str(), NK_TEXT_LEFT);
        
        // Play/Pause button and Start Capture button side by side
        if (editor.m_configLoaded && editor.m_totalFrames > 0) {
            nk_layout_row_dynamic(ctx, 3, 1);
            nk_spacing(ctx, 1);
            
            nk_layout_row_dynamic(ctx, 30, 2);
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
                LOG_INFO_FMT("CameraSystem", "Capture toggled: %s (was %s)", 
                            editor.m_captureEnabled ? "ENABLED" : "DISABLED",
                            editor.m_captureEnabled ? "DISABLED" : "ENABLED");
                editor.SaveSafeParameters();
            }
            
            // Display current frame info (always show, even if not playing)
            nk_layout_row_dynamic(ctx, 20, 1);
            char frameInfo[128];
            std::snprintf(frameInfo, sizeof(frameInfo), "Frame: %d/%d %s", 
                            editor.m_currentFrame, editor.m_totalFrames, 
                            editor.m_isPlaying ? "(Playing)" : "(Paused)");
            nk_label(ctx, frameInfo, NK_TEXT_LEFT);
            
            // Loop toggle (checkbox) right under the play button
            nk_layout_row_dynamic(ctx, 25, 1);
            bool loopEnabled = editor.m_loopEnabled;
            nk_bool loopCheckboxState = loopEnabled ? nk_false : nk_true; // Inverted logic
            if (nk_checkbox_label(ctx, "Loop", &loopCheckboxState)) {
                editor.m_loopEnabled = (loopCheckboxState == nk_false);
                LOG_INFO_FMT("CameraSystem", "Loop toggled: %s", editor.m_loopEnabled ? "ENABLED" : "DISABLED");
                editor.SaveSafeParameters();
            }
        } else {
            // If no animation, still show capture button
            nk_layout_row_dynamic(ctx, 3, 1);
            nk_spacing(ctx, 1);
            
            nk_layout_row_dynamic(ctx, 30, 1);
            const char* captureText = editor.m_captureEnabled ? "Stop Capture" : "Start Capture";
            if (nk_button_label(ctx, captureText)) {
                editor.m_captureEnabled = !editor.m_captureEnabled;
                LOG_INFO_FMT("CameraSystem", "Capture toggled: %s (was %s)", 
                            editor.m_captureEnabled ? "ENABLED" : "DISABLED",
                            editor.m_captureEnabled ? "DISABLED" : "ENABLED");
                editor.SaveSafeParameters();
            }
        }
        
        nk_layout_row_dynamic(ctx, 5, 1);
        nk_spacing(ctx, 1);
        
        // TOF data toggle
        nk_layout_row_dynamic(ctx, 30, 1);
        const char* tofText = editor.m_tofDataEnabled ? "Disable TOF Data" : "Enable TOF Data";
        if (nk_button_label(ctx, tofText)) {
            editor.m_tofDataEnabled = !editor.m_tofDataEnabled;
            LOG_INFO_FMT("CameraSystem", "TOF data capture toggled: %s", 
                        editor.m_tofDataEnabled ? "ENABLED" : "DISABLED");
            if (editor.m_tofDataEnabled) {
                // When enabling TOF, calculate depth data
                LOG_INFO("CameraSystem", "Calling UpdatePointCloud after TOF enable");
                editor.UpdatePointCloud();
                LOG_INFO_FMT("CameraSystem", "UpdatePointCloud returned. Point cloud size: %zu", 
                           editor.m_pointCloud[editor.m_pointCloudReadBuffer].size());
            }
            editor.SaveSafeParameters();
        }
        
        // VIPRES toggle (right under Enable TOF Data)
        nk_layout_row_dynamic(ctx, 30, 1);
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
        
        nk_layout_row_dynamic(ctx, 5, 1);
        nk_spacing(ctx, 1);
        
        // Show Object and Show Grid toggles side by side
        nk_layout_row_dynamic(ctx, 25, 2);
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
            LOG_INFO_FMT("CameraSystem", "Grid visibility toggled: %s", (gridCheckboxState == nk_true) ? "VISIBLE" : "HIDDEN");
            editor.SaveSafeParameters();
        }
        
        nk_layout_row_dynamic(ctx, 10, 1);
        nk_spacing(ctx, 1);
        
        // Camera list with position and direction vectors
        if (!editor.m_cameras.empty()) {
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "All Cameras:", NK_TEXT_LEFT);
            
            // First, display free cameras (canMove=true)
            for (size_t i = 0; i < editor.m_cameras.size(); i++) {
                const Camera& cam = editor.m_cameras[i];
                if (!cam.canMove) {
                    continue; // Skip non-free cameras for now
                }
                
                // Camera name (clickable to switch)
                nk_layout_row_dynamic(ctx, 25, 1);
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
                
                // Spacing between cameras
                nk_layout_row_dynamic(ctx, 5, 1);
                nk_spacing(ctx, 1);
            }
            
            // Then, display non-free cameras (canMove=false)
            for (size_t i = 0; i < editor.m_cameras.size(); i++) {
                const Camera& cam = editor.m_cameras[i];
                if (cam.canMove) {
                    continue; // Skip free cameras (already displayed)
                }
                
                // Camera name (clickable to switch)
                nk_layout_row_dynamic(ctx, 25, 1);
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
                
                // Spacing between cameras
                nk_layout_row_dynamic(ctx, 5, 1);
                nk_spacing(ctx, 1);
            }
        } else {
            nk_layout_row_dynamic(ctx, 20, 1);
            nk_label(ctx, "No cameras available", NK_TEXT_CENTERED);
        }
    }
    nk_end(ctx);
}

void CameraSystemUI::RenderTOFControls(CameraSystemEditor& editor) {
    struct nk_context* ctx = m_nuklearUI->GetContext();
    if (!ctx) return;
    
    const float statusBarHeight = 30.0f;
    
    if (!editor.m_tofDataEnabled) return;
    
    float tofWindowHeight = 400.0f;
    int anchorCameraCount = 0;
    for (const auto& cam : editor.m_cameras) {
        if (!cam.canMove) {
            anchorCameraCount++;
        }
    }
    if (anchorCameraCount > 0) {
        tofWindowHeight = std::min(800.0f, 400.0f + anchorCameraCount * 25.0f);
    }
    
    bool tofControlsVisible = nk_begin(ctx, "TOF Controls", nk_rect(680.0f, statusBarHeight + 10.0f, 300.0f, tofWindowHeight),
                    NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE);
    static int tofDebugCount = 0;
    if (tofDebugCount++ < 5 && tofControlsVisible) {
        LOG_INFO_FMT("CameraSystem", "TOF Controls window nk_begin returned: true");
    }
    if (tofControlsVisible) {
        
        // Point count display at the top (after problem points are removed)
        nk_layout_row_dynamic(ctx, 25, 1);
        char pointCountText[128];
        std::snprintf(pointCountText, sizeof(pointCountText), "Points: %zu", editor.m_pointCloud[editor.m_pointCloudReadBuffer].size());
        nk_label(ctx, pointCountText, NK_TEXT_LEFT);
        
        nk_layout_row_dynamic(ctx, 3, 1);
        nk_spacing(ctx, 1);
        
        // Color code TOF toggle
        nk_layout_row_dynamic(ctx, 25, 1);
        bool colorCodeEnabled = editor.m_tofColorCodeByCamera;
        nk_bool colorCodeCheckboxState = colorCodeEnabled ? nk_true : nk_false;
        if (nk_checkbox_label(ctx, "Color Code TOF", &colorCodeCheckboxState)) {
            editor.m_tofColorCodeByCamera = (colorCodeCheckboxState == nk_true);
            LOG_INFO_FMT("CameraSystem", "TOF color coding toggled: %s", 
                        editor.m_tofColorCodeByCamera ? "ENABLED" : "DISABLED");
            if (editor.m_tofDataEnabled) {
                // Update point cloud to reflect color change
                editor.UpdatePointCloud();
            }
            editor.SaveSafeParameters();
        }
        
        nk_layout_row_dynamic(ctx, 3, 1);
        nk_spacing(ctx, 1);
        
        // Sampling Density with value on same line, slider below
        nk_layout_row_template_begin(ctx, 25);
        nk_layout_row_template_push_static(ctx, 150);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);
        char densityLabelText[128];
        std::snprintf(densityLabelText, sizeof(densityLabelText), "Sampling Density: %d", editor.m_vipresSamplingDensity);
        nk_label(ctx, densityLabelText, NK_TEXT_LEFT);
        
        nk_layout_row_dynamic(ctx, 25, 1);
        int density = editor.m_vipresSamplingDensity;
        nk_slider_int(ctx, SAMPLING_DENSITY_MIN, &density, SAMPLING_DENSITY_MAX, 1);
        if (density != editor.m_vipresSamplingDensity) {
            editor.m_vipresSamplingDensity = density;
            editor.UpdatePointCloud();
            editor.SaveSafeParameters();
        }
        
        nk_layout_row_dynamic(ctx, 3, 1);
        nk_spacing(ctx, 1);
        
        // Depth Limit with value on same line, slider below (log scale: 0.001 to 100000)
        nk_layout_row_template_begin(ctx, 25);
        nk_layout_row_template_push_static(ctx, 150);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);
        char limitLabelText[128];
        std::snprintf(limitLabelText, sizeof(limitLabelText), "Depth Limit: %.3f", editor.m_vipresDepthLimit);
        nk_label(ctx, limitLabelText, NK_TEXT_LEFT);
        
        nk_layout_row_dynamic(ctx, 25, 1);
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
        
        nk_layout_row_dynamic(ctx, 3, 1);
        nk_spacing(ctx, 1);
        
        // TOF Sampling Rate (log scale, 1-100 Hz) with value on same line, slider below
        nk_layout_row_template_begin(ctx, 25);
        nk_layout_row_template_push_static(ctx, 200);
        nk_layout_row_template_push_dynamic(ctx);
        nk_layout_row_template_end(ctx);
        char samplingLabelText[128];
        std::snprintf(samplingLabelText, sizeof(samplingLabelText), "TOF Sampling Rate (Hz): %.1f", editor.m_tofSamplingRate);
        nk_label(ctx, samplingLabelText, NK_TEXT_LEFT);
        
        nk_layout_row_dynamic(ctx, 25, 1);
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
            editor.m_tofProcessing.ComputeLowPassFilterCoefficients(editor.m_tofLowPassCutoff, editor.m_tofSamplingRate);
            // Reuse existing filter buffers - don't clear state, just update coefficients
            editor.UpdatePointCloud();
            editor.SaveSafeParameters();
        }
        
        nk_layout_row_dynamic(ctx, 3, 1);
        nk_spacing(ctx, 1);
        
        // Gridding toggle
        nk_layout_row_dynamic(ctx, 25, 1);
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
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Grid Mode:", NK_TEXT_LEFT);
            
            nk_layout_row_dynamic(ctx, 25, 1);
            int modeInt = static_cast<int>(editor.m_vipresGridMode);
            const char* modes[] = { "Average", "First Come", "Grid Value" };
            int selected = nk_combo(ctx, modes, 3, modeInt, 25, nk_vec2(200, 200));
            if (selected != modeInt) {
                editor.m_vipresGridMode = static_cast<CameraSystemEditor::GridMode>(selected);
                editor.UpdatePointCloud();
                editor.SaveSafeParameters();
            }
            
            // Grid Size with value on same line, slider below
            nk_layout_row_template_begin(ctx, 25);
            nk_layout_row_template_push_static(ctx, 150);
            nk_layout_row_template_push_dynamic(ctx);
            nk_layout_row_template_end(ctx);
            char gridSizeLabelText[128];
            std::snprintf(gridSizeLabelText, sizeof(gridSizeLabelText), "Grid Size: %.3f", editor.m_vipresGridSize);
            nk_label(ctx, gridSizeLabelText, NK_TEXT_LEFT);
            
            nk_layout_row_dynamic(ctx, 25, 1);
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
            
            nk_layout_row_dynamic(ctx, 25, 1);
            bool lowPassEnabled = editor.m_tofLowPassEnabled && editor.m_vipresGriddingEnabled && !editor.m_tofTsdfEnabled;
            nk_bool lowPassCheckboxState = lowPassEnabled ? nk_false : nk_true; // Inverted logic
            if (!editor.m_vipresGriddingEnabled) {
                nk_widget_disable_begin(ctx);
            }
            if (nk_checkbox_label(ctx, "Enable Low-Pass Filter", &lowPassCheckboxState)) {
                if (editor.m_vipresGriddingEnabled) {
                    editor.m_tofLowPassEnabled = (lowPassCheckboxState == nk_false);
                    if (editor.m_tofLowPassEnabled) {
                        editor.m_tofTsdfEnabled = false;  // Mutually exclusive
                        editor.ClearTSDF();  // Clear TSDF when switching to low-pass
                    }
                    editor.UpdatePointCloud();
                    editor.SaveSafeParameters();
                }
            }
            if (!editor.m_vipresGriddingEnabled) {
                nk_widget_disable_end(ctx);
            }
            
            nk_layout_row_dynamic(ctx, 25, 1);
            bool tsdfEnabled = editor.m_tofTsdfEnabled && editor.m_vipresGriddingEnabled && !editor.m_tofLowPassEnabled;
            nk_bool tsdfCheckboxState = tsdfEnabled ? nk_false : nk_true; // Inverted logic
            if (!editor.m_vipresGriddingEnabled) {
                nk_widget_disable_begin(ctx);
            }
            if (nk_checkbox_label(ctx, "Enable TSDF", &tsdfCheckboxState)) {
                if (editor.m_vipresGriddingEnabled) {
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
            }
            if (!editor.m_vipresGriddingEnabled) {
                nk_widget_disable_end(ctx);
            }
            
            // TSDF Store Color toggle (only show when TSDF is enabled)
            if (editor.m_tofTsdfEnabled && editor.m_vipresGriddingEnabled) {
                nk_layout_row_dynamic(ctx, 25, 1);
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
                nk_layout_row_template_begin(ctx, 25);
                nk_layout_row_template_push_static(ctx, 200);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char truncLabelText[128];
                std::snprintf(truncLabelText, sizeof(truncLabelText), "TSDF Truncation: %.2f", editor.m_tofTsdfTruncationDistance);
                nk_label(ctx, truncLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, 25, 1);
                float truncation = editor.m_tofTsdfTruncationDistance;
                nk_slider_float(ctx, TSDF_TRUNCATION_DISTANCE_MIN, &truncation, TSDF_TRUNCATION_DISTANCE_MAX, 0.1f);
                if (std::abs(truncation - editor.m_tofTsdfTruncationDistance) > 0.01f) {
                    editor.m_tofTsdfTruncationDistance = truncation;
                    editor.UpdatePointCloud();
                    editor.SaveSafeParameters();
                }
                
                // TSDF Weight Decay with value on same line, slider below
                nk_layout_row_template_begin(ctx, 25);
                nk_layout_row_template_push_static(ctx, 200);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char decayLabelText[128];
                std::snprintf(decayLabelText, sizeof(decayLabelText), "TSDF Weight Decay: %.3f", editor.m_tofTsdfWeightDecay);
                nk_label(ctx, decayLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, 25, 1);
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
                nk_layout_row_template_begin(ctx, 25);
                nk_layout_row_template_push_static(ctx, 200);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char cutoffLabelText[128];
                std::snprintf(cutoffLabelText, sizeof(cutoffLabelText), "Low-Pass Cutoff (Hz): %.1f", editor.m_tofLowPassCutoff);
                nk_label(ctx, cutoffLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, 25, 1);
                float cutoff = editor.m_tofLowPassCutoff;
                nk_slider_float(ctx, LOW_PASS_CUTOFF_MIN, &cutoff, LOW_PASS_CUTOFF_MAX, 0.1f);
            if (std::abs(cutoff - editor.m_tofLowPassCutoff) > 0.01f) {
                editor.m_tofLowPassCutoff = cutoff;
                // Recompute filter coefficients when cutoff changes
                editor.m_tofProcessing.ComputeLowPassFilterCoefficients(editor.m_tofLowPassCutoff, editor.m_tofSamplingRate);
                // Reuse existing filter buffers - don't clear state, just update coefficients
                editor.UpdatePointCloud();
                editor.SaveSafeParameters();
            }
            }
        }
        
        nk_layout_row_dynamic(ctx, 3, 1);
        nk_spacing(ctx, 1);
        
        // Noisy TOF toggle with noise value on same line
        nk_layout_row_template_begin(ctx, 25);
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
            nk_layout_row_dynamic(ctx, 25, 1);
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
                
                LOG_INFO_FMT("CameraSystem", "Camera '%s' TOF toggled: checkbox=%s, enabled=%s", 
                            cam.name.c_str(), 
                            (cameraCheckboxState == nk_true) ? "CHECKED" : "UNCHECKED",
                            shouldBeEnabled ? "ENABLED" : "DISABLED");
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
    
    const float statusBarHeight = 30.0f;
    
    if (!editor.m_vipresEnabled) return;
    
    // Calculate window height based on remesh settings (min 200, max 600)
    float vipresWindowHeight = 200.0f;
    if (editor.m_vipresRemeshEnabled) {
        vipresWindowHeight = 500.0f; // More space for remesh controls
    }
    
    if (nk_begin(ctx, "VIPRES Controls", nk_rect(370.0f, statusBarHeight + 10.0f, 300.0f, vipresWindowHeight),
                    NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE)) {
        
        // Compute Color toggle
        nk_layout_row_dynamic(ctx, 25, 1);
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
        
        nk_layout_row_dynamic(ctx, 3, 1);
        nk_spacing(ctx, 1);
        
        // Remesh toggle
        nk_layout_row_dynamic(ctx, 25, 1);
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
            nk_layout_row_dynamic(ctx, 25, 1);
            bool autoRecompute = editor.m_vipresAutoRecomputeMesh;
            nk_bool autoRecomputeCheckboxState = autoRecompute ? nk_false : nk_true; // Inverted logic
            if (nk_checkbox_label(ctx, "Auto-recompute Mesh (Every Frame)", &autoRecomputeCheckboxState)) {
                editor.m_vipresAutoRecomputeMesh = (autoRecomputeCheckboxState == nk_false);
                editor.SaveSafeParameters();
            }
            
            // Remeshing algorithm selection
            nk_layout_row_dynamic(ctx, 25, 1);
            nk_label(ctx, "Remeshing Algorithm:", NK_TEXT_LEFT);
            
            nk_layout_row_dynamic(ctx, 25, 1);
            const char* algorithms[] = {"Gridded Quads", "Nearest Neighbor", "Delaunay 2D", "Poisson-like"};
            int currentSelection = editor.m_vipresRemeshAlgorithm;
            int newSelection = nk_combo(ctx, algorithms, 4, currentSelection, 25, nk_vec2(200, 200));
            // nk_combo returns the selected index, update if it changed
            bool algorithmChanged = false;
            if (newSelection != currentSelection && newSelection >= 0 && newSelection < 4) {
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
                nk_layout_row_template_begin(ctx, 25);
                nk_layout_row_template_push_static(ctx, 150);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char maxDistLabelText[128];
                std::snprintf(maxDistLabelText, sizeof(maxDistLabelText), "Max Distance: %.2f", editor.m_vipresRemeshMaxDistance);
                nk_label(ctx, maxDistLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, 25, 1);
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
                nk_layout_row_template_begin(ctx, 25);
                nk_layout_row_template_push_static(ctx, 150);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char maxNeighborsLabelText[128];
                std::snprintf(maxNeighborsLabelText, sizeof(maxNeighborsLabelText), "Max Neighbors: %d", editor.m_vipresRemeshMaxNeighbors);
                nk_label(ctx, maxNeighborsLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, 25, 1);
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
                nk_layout_row_dynamic(ctx, 25, 1);
                nk_label(ctx, "Projection Plane:", NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, 25, 1);
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
                
                nk_layout_row_template_begin(ctx, 25);
                nk_layout_row_template_push_static(ctx, 150);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char delaunayThresholdLabelText[128];
                std::snprintf(delaunayThresholdLabelText, sizeof(delaunayThresholdLabelText), "Delaunay Threshold: %.2f", editor.m_vipresRemeshDelaunayThreshold);
                nk_label(ctx, delaunayThresholdLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, 25, 1);
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
                nk_layout_row_template_begin(ctx, 25);
                nk_layout_row_template_push_static(ctx, 150);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char poissonRadiusLabelText[128];
                std::snprintf(poissonRadiusLabelText, sizeof(poissonRadiusLabelText), "Poisson Radius: %.2f", editor.m_vipresRemeshPoissonRadius);
                nk_label(ctx, poissonRadiusLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, 25, 1);
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
                
                nk_layout_row_template_begin(ctx, 25);
                nk_layout_row_template_push_static(ctx, 150);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char poissonDepthLabelText[128];
                std::snprintf(poissonDepthLabelText, sizeof(poissonDepthLabelText), "Poisson Depth: %d", editor.m_vipresRemeshPoissonDepth);
                nk_label(ctx, poissonDepthLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, 25, 1);
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
                
                nk_layout_row_template_begin(ctx, 25);
                nk_layout_row_template_push_static(ctx, 150);
                nk_layout_row_template_push_dynamic(ctx);
                nk_layout_row_template_end(ctx);
                char poissonDensityLabelText[128];
                std::snprintf(poissonDensityLabelText, sizeof(poissonDensityLabelText), "Poisson Density: %.2f", editor.m_vipresRemeshPoissonDensity);
                nk_label(ctx, poissonDensityLabelText, NK_TEXT_LEFT);
                
                nk_layout_row_dynamic(ctx, 25, 1);
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
                
                nk_layout_row_dynamic(ctx, 25, 1);
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
            
            // Apply button to regenerate mesh with current parameters
            nk_layout_row_dynamic(ctx, 25, 1);
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
    
    const float statusBarHeight = 30.0f;
    
    // Objects panel
    if (nk_begin(ctx, "Objects", nk_rect(370.0f, statusBarHeight + 10.0f, 300.0f, 200.0f),
                 NK_WINDOW_BORDER | NK_WINDOW_TITLE | NK_WINDOW_SCALABLE | NK_WINDOW_MOVABLE)) {

    nk_layout_row_dynamic(ctx, 25, 1);
   nk_label(ctx, "Loaded Objects:", NK_TEXT_LEFT);
           
           nk_layout_row_dynamic(ctx, 25, 1);
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
           nk_layout_row_template_begin(ctx, 25);
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
               nk_layout_row_template_begin(ctx, 25);
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
               
               nk_layout_row_dynamic(ctx, 25, 1);
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

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

