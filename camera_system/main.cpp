#include "CameraSystemEditor.h"
#include "../../engine/platform/Logger.h"
#include "logs/ConsoleRedirect.h"
#include <iostream>
#include <string>
#include <exception>
#include <csignal>
#include <atomic>

using namespace Engine::Editors::CameraSystem;
using EngineLogging::Logger;

// Global flag for signal handling (non-static so it can be accessed from CameraSystemEditor.cpp)
std::atomic<bool> g_shouldExit{false};
static CameraSystemEditor* g_editorInstance = nullptr;

// Signal handler for clean shutdown
// Note: SIGKILL (kill -9) cannot be caught and will always terminate immediately
// Signal handlers must be minimal - only set flags, no complex operations
static void SignalHandler(int sig) {
    // Set the flag - the main loop will check it
    g_shouldExit = true;
    
    // For SIGINT (Ctrl+C) and SIGTERM, also try to close the window immediately
    // This is safe because we're just setting a boolean flag on an object
    if ((sig == SIGINT || sig == SIGTERM) && g_editorInstance) {
        // Directly request exit - this is async-safe (just sets a flag)
        g_editorInstance->RequestExit();
    }
    
    // For other signals, restore default and re-raise
    if (sig != SIGTERM && sig != SIGINT) {
        std::signal(sig, SIG_DFL);
        std::raise(sig);
    }
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string configFile;
    std::string objFile;
    std::string ousterPacketFile;
    std::string ousterPcapFile;
    std::string ousterMetadataFile;
    bool hasAnyArgs = false;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configFile = argv[++i];
            hasAnyArgs = true;
        } else if (arg == "--obj" && i + 1 < argc) {
            objFile = argv[++i];
            hasAnyArgs = true;
        } else if (arg == "--ouster-packet" && i + 1 < argc) {
            ousterPacketFile = argv[++i];
            hasAnyArgs = true;
        } else if (arg == "--ouster-pcap" && i + 1 < argc) {
            ousterPcapFile = argv[++i];
            hasAnyArgs = true;
        } else if (arg == "--ouster-metadata" && i + 1 < argc) {
            ousterMetadataFile = argv[++i];
            hasAnyArgs = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Camera System Editor" << std::endl;
            std::cout << "Usage: camera_system_editor [options]" << std::endl;
            std::cout << "  --config <file>           Load configuration from JSON file" << std::endl;
            std::cout << "  --obj <file>              Load an OBJ/FBX model file directly" << std::endl;
            std::cout << "  --ouster-packet <file>    Load a saved Ouster packet file for debugging" << std::endl;
            std::cout << "  --ouster-pcap <file>      Load a PCAP file (requires --ouster-metadata)" << std::endl;
            std::cout << "  --ouster-metadata <file>  Metadata JSON file for PCAP (required with --ouster-pcap)" << std::endl;
            std::cout << "  --help, -h                Show this help message" << std::endl;
            return 0;
        }
    }
    
    // Set up signal handlers for clean shutdown
    signal(SIGINT, SignalHandler);   // Ctrl+C
    signal(SIGTERM, SignalHandler);  // kill command
    // Note: SIGKILL (kill -9) cannot be caught - it will always terminate immediately
    
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
        g_editorInstance = &editor;  // Store for signal handler
        
        LOG_INFO("CameraSystem", "Initializing editor...");
        if (!editor.Initialize(1280, 720)) {
            LOG_ERROR("CameraSystem", "Failed to initialize editor");
            logger.Shutdown();
            return 1;
        }
        LOG_INFO("CameraSystem", "Editor initialized successfully");
        
        // If no command line arguments provided, disable both Ouster and TOF by default
        if (!hasAnyArgs) {
            LOG_INFO("CameraSystem", "No command line arguments - disabling all sensors by default");
            editor.DisableAllSensors();
        }
        
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
        
        // Load Ouster packet file if provided (for debugging)
        if (!ousterPacketFile.empty()) {
            std::cout << "Loading Ouster packet from: " << ousterPacketFile << std::endl;
            LOG_INFO_FMT("CameraSystem", "Loading Ouster packet from: %s", ousterPacketFile.c_str());
            if (!editor.LoadOusterPacket(ousterPacketFile)) {
                LOG_ERROR_FMT("CameraSystem", "Failed to load Ouster packet: %s", ousterPacketFile.c_str());
                std::cerr << "Error: Failed to load Ouster packet file: " << ousterPacketFile << std::endl;
                logger.Shutdown();
                return 1;
            }
            LOG_INFO("CameraSystem", "Ouster packet loaded successfully");
        }
        
        // Set PCAP file paths if provided (for UI access)
        if (!ousterPcapFile.empty() && !ousterMetadataFile.empty()) {
            editor.SetOusterPcapFiles(ousterPcapFile, ousterMetadataFile);
            
            // Load metadata file for use with live streaming (faster initialization, offline support)
            LOG_INFO_FMT("CameraSystem", "Loading metadata from file for live streaming: %s", ousterMetadataFile.c_str());
            if (editor.LoadOusterMetadata(ousterMetadataFile)) {
                LOG_INFO("CameraSystem", "Metadata loaded successfully - will be used for parsing live stream data");
            } else {
                LOG_WARNING_FMT("CameraSystem", "Failed to load metadata from file: %s (will fetch from sensor)", ousterMetadataFile.c_str());
            }
            
            // Optionally auto-load PCAP on startup if provided
            // Uncomment the following lines to auto-load PCAP files on startup:
            /*
            std::cout << "Loading Ouster PCAP from: " << ousterPcapFile << std::endl;
            std::cout << "Using metadata: " << ousterMetadataFile << std::endl;
            LOG_INFO_FMT("CameraSystem", "Loading Ouster PCAP from: %s with metadata: %s", 
                        ousterPcapFile.c_str(), ousterMetadataFile.c_str());
            if (!editor.LoadOusterPcap(ousterPcapFile, ousterMetadataFile)) {
                LOG_ERROR_FMT("CameraSystem", "Failed to load Ouster PCAP: %s", ousterPcapFile.c_str());
                std::cerr << "Error: Failed to load Ouster PCAP file: " << ousterPcapFile << std::endl;
                logger.Shutdown();
                return 1;
            }
            LOG_INFO("CameraSystem", "Ouster PCAP loaded successfully");
            */
        } else if (!ousterPcapFile.empty() && ousterMetadataFile.empty()) {
            std::cerr << "Error: --ouster-pcap requires --ouster-metadata to be specified" << std::endl;
            LOG_ERROR("CameraSystem", "--ouster-pcap requires --ouster-metadata");
            logger.Shutdown();
            return 1;
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
        // The Run() method will check g_shouldExit and window shouldClose flag
        editor.Run();
        
        // Check if we should exit due to signal
        if (g_shouldExit) {
            LOG_INFO("CameraSystem", "Exit requested by signal handler");
        }
        
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

