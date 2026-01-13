#pragma once

#include "../remesh/Remeshing.h"  // For PointCloudPoint
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <queue>

namespace Engine {
namespace Editors {
namespace CameraSystem {

// Ouster sensor configuration
struct OusterConfig {
    std::string hostname = "172.17.1.56";  // Your sensor's IP
    int lidarPort = 7504;  // Changed from 7502 to avoid zombie socket
    int imuPort = 7503;
    
    // Transform to apply to all points (sensor -> world frame)
    glm::mat4 sensorToWorld = glm::mat4(1.0f);
    
    // Point cloud filtering
    float minRange = 0.3f;      // Minimum range in meters
    float maxRange = 50.0f;     // Maximum range in meters
    
    // Color mapping
    bool useIntensityAsColor = true;  // Map signal intensity to grayscale
    bool useRangeAsColor = false;     // Map range to color gradient
    
    // Downsampling (skip every N points, 1 = no skip)
    int downsampleFactor = 1;
    
    // Camera ID to assign to all points from this sensor
    int32_t cameraId = 0;
};

// IMU data from Ouster
struct OusterIMUData {
    double timestamp;
    glm::vec3 acceleration;  // m/s^2
    glm::vec3 angularVelocity;  // rad/s
};

// Callback types
using PointCloudCallback = std::function<void(const std::vector<PointCloudPoint>& points, double timestamp)>;
using IMUCallback = std::function<void(const OusterIMUData& imu)>;

// Ouster streaming class
// Connects to sensor and provides point cloud data in PointCloudPoint format
class OusterStream {
public:
    OusterStream();
    ~OusterStream();
    
    // Configuration
    void SetConfig(const OusterConfig& config);
    const OusterConfig& GetConfig() const { return m_config; }
    
    // Connection
    bool Connect();
    void Disconnect();
    bool IsConnected() const { return m_connected; }
    
    // Streaming control
    void StartStreaming();
    void StopStreaming();
    bool IsStreaming() const { return m_streaming; }
    
    // Callback registration (called from streaming thread)
    void SetPointCloudCallback(PointCloudCallback callback) { m_pointCloudCallback = callback; }
    void SetIMUCallback(IMUCallback callback) { m_imuCallback = callback; }
    
    // Polling interface (thread-safe queue)
    // Returns true if a new frame is available
    bool PollPointCloud(std::vector<PointCloudPoint>& outPoints, double& outTimestamp);
    bool PollIMU(OusterIMUData& outIMU);
    
    // Get latest frame (blocking if queue empty, with timeout)
    bool WaitForPointCloud(std::vector<PointCloudPoint>& outPoints, double& outTimestamp, int timeoutMs = 1000);
    
    // Statistics
    int GetFrameCount() const { return m_frameCount; }
    int GetDroppedFrames() const { return m_droppedFrames; }
    double GetLastTimestamp() const { return m_lastTimestamp; }
    
    // Sensor info
    std::string GetSensorInfo() const;
    int GetBeamCount() const { return m_beamCount; }
    int GetColumnsPerFrame() const { return m_columnsPerFrame; }
    
private:
    void StreamingThread();
    void ProcessLidarPacket(const uint8_t* data, size_t size);
    
    // Convert Ouster point to PointCloudPoint
    PointCloudPoint ConvertPoint(float x, float y, float z, float intensity, float range);
    
    // Apply color mapping based on config
    glm::vec3 MapToColor(float intensity, float range);
    
private:
    OusterConfig m_config;
    
    std::atomic<bool> m_connected{false};
    std::atomic<bool> m_streaming{false};
    std::atomic<bool> m_shouldStop{false};
    std::atomic<bool> m_startAttempted{false};  // True if we've tried to start (prevents rapid restart)
    std::atomic<bool> m_bindFailed{false};  // True if last bind attempt failed
    std::chrono::steady_clock::time_point m_lastStartAttempt;  // For retry cooldown
    
    std::unique_ptr<std::thread> m_streamThread;
    
    // Callbacks
    PointCloudCallback m_pointCloudCallback;
    IMUCallback m_imuCallback;
    
    // Thread-safe queues for polling
    std::mutex m_pointCloudMutex;
    std::queue<std::pair<std::vector<PointCloudPoint>, double>> m_pointCloudQueue;
    static constexpr size_t MAX_QUEUE_SIZE = 10;
    
    std::mutex m_imuMutex;
    std::queue<OusterIMUData> m_imuQueue;
    
    // Statistics
    std::atomic<int> m_frameCount{0};
    std::atomic<int> m_droppedFrames{0};
    std::atomic<double> m_lastTimestamp{0.0};
    
    // Sensor metadata
    int m_beamCount = 128;  // OS0-128
    int m_columnsPerFrame = 1024;
    
    // Internal buffer
    std::vector<PointCloudPoint> m_currentFrame;
};

// Convenience function to create a configured stream
std::unique_ptr<OusterStream> CreateOusterStream(const std::string& hostname = "172.17.1.56");

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine
