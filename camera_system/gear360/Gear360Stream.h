#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>
#include <cstdint>

namespace Engine {
namespace Editors {
namespace CameraSystem {

// Video frame data structure
struct VideoFrame {
    std::vector<uint8_t> data;      // Raw video frame data (JPEG, H.264, etc.)
    int width = 0;
    int height = 0;
    int format = 0;                  // Format code (0=JPEG, 1=H.264, etc.)
    double timestamp = 0.0;
    bool is360 = true;               // Is this a 360° spherical frame?
};

// Gear 360 configuration
struct Gear360Config {
    // Bluetooth connection settings
    std::string deviceName = "Gear 360";  // Device name to search for
    std::string deviceAddress;             // Bluetooth MAC address (if known)
    bool autoDiscover = true;              // Auto-discover device
    
    // Video streaming settings
    int videoWidth = 3840;                 // 4K width
    int videoHeight = 1920;                // 4K height (spherical)
    int fps = 30;                          // Frames per second
    bool useWiFi = false;                  // Use WiFi instead of Bluetooth (recommended)
    std::string wifiSSID;                  // WiFi SSID if using WiFi
    std::string wifiPassword;              // WiFi password if using WiFi
    
    // HTTP authentication (if required by camera)
    std::string httpUsername;              // HTTP Basic Auth username
    std::string httpPassword;              // HTTP Basic Auth password
    std::string cameraIP = "192.168.1.1";  // Camera IP address (try 192.168.43.1 if this doesn't work)
    std::string streamPath = "/stream";    // Stream endpoint path
    
    // Camera ID to assign to frames
    int32_t cameraId = 1;
    
    // Note: The Gear 360 (2017) primarily uses WiFi for video streaming.
    // Bluetooth is mainly used for control/configuration. This implementation
    // provides Bluetooth connectivity, but video streaming may require WiFi.
};

// Callback types
using VideoFrameCallback = std::function<void(const VideoFrame& frame)>;
using ConnectionCallback = std::function<void(bool connected)>;

// Gear 360 streaming class
// Connects to Samsung Gear 360 (2017) via Bluetooth and streams video data
class Gear360Stream {
public:
    Gear360Stream();
    ~Gear360Stream();
    
    // Configuration
    void SetConfig(const Gear360Config& config);
    const Gear360Config& GetConfig() const { return m_config; }
    
    // Connection
    bool Connect();
    void Disconnect();
    bool IsConnected() const { return m_connected; }
    
    // Device discovery
    bool DiscoverDevices(std::vector<std::string>& deviceNames);
    
    // Streaming control
    void StartStreaming();
    void StopStreaming();
    bool IsStreaming() const { return m_streaming; }
    
    // Camera control (via Bluetooth)
    bool StartRecording();
    bool StopRecording();
    bool TakePhoto();
    bool SetVideoMode(bool is360);  // true for 360°, false for regular
    
    // Callback registration (called from streaming thread)
    void SetVideoFrameCallback(VideoFrameCallback callback) { m_videoFrameCallback = callback; }
    void SetConnectionCallback(ConnectionCallback callback) { m_connectionCallback = callback; }
    
    // Polling interface (thread-safe queue)
    // Returns true if a new frame is available
    bool PollVideoFrame(VideoFrame& outFrame);
    
    // Get latest frame (blocking if queue empty, with timeout)
    bool WaitForVideoFrame(VideoFrame& outFrame, int timeoutMs = 1000);
    
    // Statistics
    int GetFrameCount() const { return m_frameCount; }
    int GetDroppedFrames() const { return m_droppedFrames; }
    double GetLastTimestamp() const { return m_lastTimestamp; }
    double GetFPS() const { return m_currentFPS; }
    
    // Device info
    std::string GetDeviceInfo() const;
    std::string GetDeviceName() const { return m_deviceName; }
    std::string GetDeviceAddress() const { return m_deviceAddress; }
    
    // Public method for Objective-C bridge to call
    void ProcessVideoData(const uint8_t* data, size_t size);
    
private:
    void StreamingThread();
    
    // Bluetooth connection helpers (platform-specific)
    bool InitializeBluetooth();
    void CleanupBluetooth();
    bool ConnectToDevice();
    bool SendCommand(const std::vector<uint8_t>& command);
    bool ReceiveData(std::vector<uint8_t>& buffer, size_t maxSize);
    
    // WiFi connection helpers (if using WiFi)
    bool InitializeWiFi();
    bool ConnectToWiFi();
    bool ReceiveVideoOverWiFi();
    
private:
    Gear360Config m_config;
    
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_streaming{false};
    std::atomic<bool> m_shouldStop{false};
    
    std::unique_ptr<std::thread> m_streamThread;
    
    // Callbacks
    VideoFrameCallback m_videoFrameCallback;
    ConnectionCallback m_connectionCallback;
    
    // Thread-safe queue for polling
    std::mutex m_frameMutex;
    std::queue<VideoFrame> m_frameQueue;
    static constexpr size_t MAX_QUEUE_SIZE = 30;  // ~1 second at 30fps
    
    // Statistics
    std::atomic<int> m_frameCount{0};
    std::atomic<int> m_droppedFrames{0};
    std::atomic<double> m_lastTimestamp{0.0};
    std::atomic<double> m_currentFPS{0.0};
    
    // Device information
    std::string m_deviceName;
    std::string m_deviceAddress;
    
    // Platform-specific handles (will be void* or specific types)
    void* m_bluetoothHandle = nullptr;
    void* m_wifiHandle = nullptr;
    
    // FPS calculation
    std::chrono::steady_clock::time_point m_lastFPSUpdate;
    int m_fpsFrameCount = 0;
};

// Convenience function to create a configured stream
std::unique_ptr<Gear360Stream> CreateGear360Stream(const std::string& deviceName = "Gear 360");

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine
