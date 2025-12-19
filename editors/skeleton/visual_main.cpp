#include "VisualSkeletonEditor.h"
#include "../../engine/platform/Logger.h"
#include "ConsoleRedirect.h"
#include <iostream>
#include <string>
#include <exception>
#include <filesystem>

using namespace Engine::Editors::Skeleton;
using EngineLogging::Logger;

int main(int argc, char* argv[]) {
    // Initialize logger first
    EngineLogging::Logger& logger = EngineLogging::Logger::GetInstance();
    logger.Initialize("visual_editor_debug.log");
    logger.SetMinLevel(EngineLogging::LogLevel::LevelInfo);
    
    // Redirect stdout/stderr to log file (also keeps console output)
    ConsoleRedirect consoleRedirect("visual_editor_debug.log");
    if (!consoleRedirect.IsValid()) {
        std::cerr << "Warning: Failed to redirect console output to log file" << std::endl;
        LOG_WARNING("VisualEditor", "Console redirect failed - some output may not be logged");
    } else {
        LOG_INFO("VisualEditor", "Console output redirection enabled");
    }
    
    std::cout << "=== Visual Skeleton Editor ===" << std::endl;
    std::cout << "All output (stdout/stderr) is being logged to: visual_editor_debug.log" << std::endl;
    LOG_INFO("VisualEditor", "=== Visual Skeleton Editor ===");
    
    // Parse command line arguments
    std::string inputMeshPath;
    std::string outputSkeletonPath;
    std::string animationPath;
    
    // Parse flags
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--mesh" && i + 1 < argc) {
            inputMeshPath = argv[++i];
        } else if (arg == "--skel" && i + 1 < argc) {
            outputSkeletonPath = argv[++i];
        } else if (arg == "--anim" && i + 1 < argc) {
            animationPath = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: visual_skeleton_editor.exe [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --mesh <path>    Path to mesh file (OBJ/FBX)" << std::endl;
            std::cout << "  --skel <path>    Path to skeleton file (.skel)" << std::endl;
            std::cout << "                   If not specified, will be created from mesh path" << std::endl;
            std::cout << "  --anim <path>    Path to animation directory or FBX file" << std::endl;
            std::cout << "                   Recursively loads all .fbx files from directory" << std::endl;
            std::cout << "  --help, -h        Show this help message" << std::endl;
            std::cout << std::endl;
            std::cout << "Controls:" << std::endl;
            std::cout << "  Right Mouse - Rotate camera" << std::endl;
            std::cout << "  Left Mouse - Place bones" << std::endl;
            std::cout << "  Middle Mouse - Remove bones" << std::endl;
            std::cout << "  Mouse Wheel - Zoom" << std::endl;
            std::cout << "  M - Toggle mesh visibility" << std::endl;
            std::cout << "  V - Toggle contours visibility" << std::endl;
            std::cout << "  S - Save skeleton" << std::endl;
            std::cout << "  ESC - Exit editor" << std::endl;
            return 0;
        }
    }
    
    // DO NOT auto-generate skeleton path from mesh path
    // Skeleton should only be loaded when explicitly passed via --skel or selected from dropdown
    // If skeleton path is empty, we'll use a default name for saving, but won't try to load it
    
    LOG_INFO("VisualEditor", "Input mesh: " + inputMeshPath);
    LOG_INFO("VisualEditor", "Output skeleton: " + outputSkeletonPath);
    LOG_INFO("VisualEditor", "Controls:");
    LOG_INFO("VisualEditor", "  1 - Place Bones mode");
    LOG_INFO("VisualEditor", "  2 - Select Bones mode");
    LOG_INFO("VisualEditor", "  3 - Paint Weights mode");
    LOG_INFO("VisualEditor", "  Right Mouse - Rotate camera");
    LOG_INFO("VisualEditor", "  Left Mouse - Place/Select/Paint");
    LOG_INFO("VisualEditor", "  Mouse Wheel - Zoom");
    LOG_INFO("VisualEditor", "  Delete - Remove selected bone");
    LOG_INFO("VisualEditor", "  S - Save skeleton");
    LOG_INFO("VisualEditor", "  ESC - Exit");
    
    try {
        VisualSkeletonEditor editor;
        
        // Set output path
        editor.SetOutputPath(outputSkeletonPath);
        
        LOG_INFO("VisualEditor", "Initializing editor...");
        if (!editor.Initialize(1280, 720)) {
            LOG_ERROR("VisualEditor", "Failed to initialize editor");
            logger.Shutdown();
            return 1;
        }
        LOG_INFO("VisualEditor", "Editor initialized successfully");
        
        // Load mesh if provided
        if (!inputMeshPath.empty()) {
            LOG_INFO_FMT("VisualEditor", "Loading mesh: %s", inputMeshPath.c_str());
            std::cout << "Loading mesh: " << inputMeshPath << std::endl;
            if (!editor.LoadMesh(inputMeshPath)) {
                LOG_ERROR_FMT("VisualEditor", "Failed to load mesh: %s", inputMeshPath.c_str());
                std::cerr << "Failed to load mesh: " << inputMeshPath << std::endl;
                std::cerr << "The editor will still open, but no mesh will be displayed." << std::endl;
            } else {
                LOG_INFO("VisualEditor", "Mesh loaded successfully");
                std::cout << "✓ Mesh loaded successfully" << std::endl;
            }
        } else {
            LOG_INFO("VisualEditor", "No mesh file provided, editor will open with grid only");
        }
        
        // Only load skeleton if explicitly passed via --skel argument
        // DO NOT auto-load skeleton files even if mesh is provided
        // Skeleton can be loaded later via dropdown menu in the editor
        if (!outputSkeletonPath.empty()) {
            // Convert to absolute path
            std::filesystem::path skelPath(outputSkeletonPath);
            try {
                if (skelPath.is_relative()) {
                    skelPath = std::filesystem::absolute(skelPath);
                }
                skelPath = skelPath.lexically_normal();
                outputSkeletonPath = skelPath.string();
            } catch (...) {
                // Use original path if filesystem operations fail
            }
            
            // Try to load skeleton (don't create if it doesn't exist)
            if (editor.LoadRig(outputSkeletonPath)) {
                LOG_INFO_FMT("VisualEditor", "Loaded skeleton from: %s", outputSkeletonPath.c_str());
                std::cout << "✓ Loaded skeleton file" << std::endl;
            
            // Set output path for future saves
            editor.SetOutputPath(outputSkeletonPath);
            } else {
                LOG_WARNING_FMT("VisualEditor", "Skeleton file not found: %s", outputSkeletonPath.c_str());
                std::cerr << "Warning: Skeleton file not found. Use the dropdown menu in the editor to select a skeleton file." << std::endl;
            }
        } else {
            // No skeleton path provided - use default for saving, but don't try to load
            editor.SetOutputPath("skeleton.skel");
        }
        
        // Load animations if path provided
        if (!animationPath.empty()) {
            LOG_INFO_FMT("VisualEditor", "Loading animations from: %s", animationPath.c_str());
            std::cout << "Loading animations from: " << animationPath << std::endl;
            
            // Convert to absolute path
            std::filesystem::path animPath(animationPath);
            try {
                if (animPath.is_relative()) {
                    animPath = std::filesystem::absolute(animPath);
                }
                animPath = animPath.lexically_normal();
                animationPath = animPath.string();
            } catch (...) {
                // Use original path if filesystem operations fail
            }
            
            // Check if it's a file or directory
            if (std::filesystem::is_regular_file(animationPath)) {
                // Single file - load it directly
                if (editor.LoadAnimationFile(animationPath)) {
                    std::cout << "✓ Loaded animation file" << std::endl;
                } else {
                    LOG_WARNING_FMT("VisualEditor", "Failed to load animation: %s", animationPath.c_str());
                    std::cerr << "Warning: Failed to load animation file" << std::endl;
                }
            } else if (std::filesystem::is_directory(animationPath)) {
                // Directory - load recursively
                editor.LoadAnimationsFromDirectory(animationPath);
                std::cout << "✓ Loaded animations from directory" << std::endl;
            } else {
                LOG_WARNING_FMT("VisualEditor", "Animation path is neither file nor directory: %s", animationPath.c_str());
                std::cerr << "Warning: Animation path is invalid" << std::endl;
            }
        }
        
        LOG_INFO("VisualEditor", "Starting editor main loop...");
        // Run editor
        editor.Run();
        
        LOG_INFO("VisualEditor", "Editor closed");
        
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("VisualEditor", "Fatal error: %s", e.what());
        logger.Shutdown();
        return 1;
    } catch (...) {
        LOG_ERROR("VisualEditor", "Fatal error: Unknown exception");
        logger.Shutdown();
        return 1;
    }
    
    logger.Shutdown();
    return 0;
}

