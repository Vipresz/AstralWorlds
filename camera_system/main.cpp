#include "CameraSystemEditor.h"
#include "../../engine/platform/Logger.h"
#include "logs/ConsoleRedirect.h"
#include <iostream>
#include <string>
#include <exception>

using namespace Engine::Editors::CameraSystem;
using EngineLogging::Logger;

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string configFile;
    std::string objFile;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configFile = argv[++i];
        } else if (arg == "--obj" && i + 1 < argc) {
            objFile = argv[++i];
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Camera System Editor" << std::endl;
            std::cout << "Usage: camera_system_editor [options]" << std::endl;
            std::cout << "  --config <file>  Load configuration from JSON file" << std::endl;
            std::cout << "  --obj <file>     Load an OBJ/FBX model file directly" << std::endl;
            std::cout << "  --help, -h       Show this help message" << std::endl;
            return 0;
        }
    }
    
    // Initialize logger with relative path (works on any platform)
    EngineLogging::Logger& logger = EngineLogging::Logger::GetInstance();
    const std::string logFilePath = "camera_system_editor_debug.log";
    logger.Initialize(logFilePath);
    logger.SetMinLevel(EngineLogging::LogLevel::LevelInfo);
    
    // Redirect stdout/stderr to log file (also keeps console output)
    ConsoleRedirect consoleRedirect(logFilePath);
    if (!consoleRedirect.IsValid()) {
        std::cerr << "Warning: Failed to redirect console output to log file" << std::endl;
        LOG_WARNING("CameraSystem", "Console redirect failed - some output may not be logged");
    } else {
        LOG_INFO("CameraSystem", "Console output redirection enabled");
    }
    
    std::cout << "=== Camera System Editor ===" << std::endl;
    std::cout << "All output (stdout/stderr) is being logged to: " << logFilePath << std::endl;
    LOG_INFO("CameraSystem", "=== Camera System Editor ===");
    LOG_INFO_FMT("CameraSystem", "Log file: %s", logFilePath.c_str());
    
    if (!configFile.empty()) {
        std::cout << "Configuration file: " << configFile << std::endl;
        LOG_INFO_FMT("CameraSystem", "Loading configuration from: %s", configFile.c_str());
    } else {
        std::cout << "No configuration file specified. Use --config <file.json> to load a configuration." << std::endl;
        std::cout << "Run with --help for usage information." << std::endl;
        LOG_WARNING("CameraSystem", "No configuration file specified");
    }
    
    try {
        CameraSystemEditor editor;
        
        LOG_INFO("CameraSystem", "Initializing editor...");
        if (!editor.Initialize(1280, 720)) {
            LOG_ERROR("CameraSystem", "Failed to initialize editor");
            logger.Shutdown();
            return 1;
        }
        LOG_INFO("CameraSystem", "Editor initialized successfully");
        
        // Load configuration if provided
        if (!configFile.empty()) {
            if (!editor.LoadConfig(configFile)) {
                LOG_ERROR_FMT("CameraSystem", "Failed to load configuration from: %s", configFile.c_str());
                std::cerr << "Error: Failed to load configuration file: " << configFile << std::endl;
                logger.Shutdown();
                return 1;
            }
            LOG_INFO("CameraSystem", "Configuration loaded successfully");
        }
        
        // Load OBJ file if provided
        if (!objFile.empty()) {
            std::cout << "Loading model: " << objFile << std::endl;
            LOG_INFO_FMT("CameraSystem", "Loading model from command line: %s", objFile.c_str());
            if (!editor.AddObject(objFile)) {
                LOG_ERROR_FMT("CameraSystem", "Failed to load model: %s", objFile.c_str());
                std::cerr << "Error: Failed to load model file: " << objFile << std::endl;
                logger.Shutdown();
                return 1;
            }
            LOG_INFO("CameraSystem", "Model loaded successfully");
        }
        
        std::cout << "Controls:" << std::endl;
        std::cout << "  ESC - Exit editor" << std::endl;
        std::cout << "  1-5 - Switch cameras" << std::endl;
        std::cout << "  WASD - Move free camera (when active)" << std::endl;
        std::cout << "  Space/Shift - Move free camera up/down" << std::endl;
        std::cout << "  Right Mouse + Drag - Rotate free camera" << std::endl;
        std::cout << std::endl;
        
        LOG_INFO("CameraSystem", "Starting editor main loop...");
        // Run editor (will run at 30fps and follow paths if config loaded)
        editor.Run();
        
        LOG_INFO("CameraSystem", "Editor closed");
        
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("CameraSystem", "Fatal error: %s", e.what());
        logger.Shutdown();
        return 1;
    } catch (...) {
        LOG_ERROR("CameraSystem", "Fatal error: Unknown exception");
        logger.Shutdown();
        return 1;
    }
    
    logger.Shutdown();
    return 0;
}

