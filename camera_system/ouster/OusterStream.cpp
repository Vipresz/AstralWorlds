#include "OusterStream.h"
#include <cmath>
#include <chrono>
#include <iostream>

#ifdef __APPLE__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <cerrno>
#include <cstring>
#endif

namespace Engine {
namespace Editors {
namespace CameraSystem {

// Ouster packet structures for legacy profile (v2.3.x firmware)
// Reference: Ouster Software User Manual
#pragma pack(push, 1)

struct LidarColumnHeader {
    uint64_t timestamp;
    uint16_t measurementId;
    uint16_t frameId;
    uint32_t encoderCount;
};

// Each pixel in a column (for OS0-128)
struct LidarPixel {
    uint32_t range;       // Range in mm
    uint16_t signal;      // Signal photons
    uint16_t reflectivity;
    uint16_t nearIr;
};

#pragma pack(pop)

OusterStream::OusterStream() {
    m_currentFrame.reserve(m_beamCount * m_columnsPerFrame);
}

OusterStream::~OusterStream() {
    StopStreaming();
    Disconnect();
}

void OusterStream::SetConfig(const OusterConfig& config) {
    m_config = config;
}

bool OusterStream::Connect() {
    // For now, just mark as connected - actual connection happens in streaming thread
    // In a full implementation, we'd query sensor metadata here
    m_connected = true;
    return true;
}

void OusterStream::Disconnect() {
    StopStreaming();
    m_connected = false;
}

void OusterStream::StartStreaming() {
    if (m_streaming) return;
    
    // Prevent rapid restart attempts - wait 5 seconds between attempts
    if (m_startAttempted) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_lastStartAttempt).count();
        if (elapsed < 5) {
            return;  // Still in cooldown period
        }
    }
    
    // Record this attempt
    m_startAttempted = true;
    m_lastStartAttempt = std::chrono::steady_clock::now();
    m_bindFailed = false;
    
    m_shouldStop = false;
    m_streaming = true;
    m_streamThread = std::make_unique<std::thread>(&OusterStream::StreamingThread, this);
}

void OusterStream::StopStreaming() {
    if (!m_streaming) return;
    
    m_shouldStop = true;
    if (m_streamThread && m_streamThread->joinable()) {
        m_streamThread->join();
    }
    m_streamThread.reset();
    m_streaming = false;
}

void OusterStream::StreamingThread() {
#ifdef __APPLE__
    // Create UDP socket for lidar data
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        std::cerr << "OusterStream: Failed to create socket" << std::endl;
        m_streaming = false;
        return;
    }
    
    // Set socket to non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    // Increase receive buffer size for high-bandwidth lidar data
    int bufSize = 4 * 1024 * 1024;  // 4MB
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));
    
    // Allow address and port reuse (helps after crash)
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
    
    // Bind to lidar port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_config.lidarPort);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "OusterStream: Failed to bind to port " << m_config.lidarPort 
                  << " (errno: " << errno << " - " << strerror(errno) << ")" << std::endl;
        std::cerr << "OusterStream: Make sure no other application is using this port." << std::endl;
        std::cerr << "OusterStream: You can check with: sudo lsof -i :" << m_config.lidarPort << std::endl;
        std::cerr << "OusterStream: Will retry in 5 seconds..." << std::endl;
        close(sock);
        m_streaming = false;
        m_bindFailed = true;
        return;
    }
    
    std::cout << "OusterStream: Successfully bound to UDP port " << m_config.lidarPort << std::endl;
    std::cout << "OusterStream: Waiting for lidar data from " << m_config.hostname << "..." << std::endl;
    std::cout << "OusterStream: Min range: " << m_config.minRange << "m, Max range: " << m_config.maxRange << "m" << std::endl;
    std::cout.flush();
    
    int totalPackets = 0;
    int totalPoints = 0;
    
    // Buffer for receiving packets
    std::vector<uint8_t> buffer(65536);
    
    // Track current frame
    uint16_t currentFrameId = 0;
    bool frameStarted = false;
    
    // Beam angles for OS0-128 (approximate - should be loaded from sensor metadata)
    std::vector<float> beamAltitudeAngles(128);
    std::vector<float> beamAzimuthAngles(128);
    for (int i = 0; i < 128; i++) {
        // OS0-128 has roughly +/- 45 degree vertical FOV
        beamAltitudeAngles[i] = (45.0f - 90.0f * i / 127.0f) * M_PI / 180.0f;
        beamAzimuthAngles[i] = 0.0f;  // Azimuth offset per beam (usually small)
    }
    
    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLIN;
    
    while (!m_shouldStop) {
        // Poll with 100ms timeout
        int ret = poll(&pfd, 1, 100);
        if (ret <= 0) continue;
        
        ssize_t received = recv(sock, buffer.data(), buffer.size(), 0);
        if (received <= 0) continue;
        
        totalPackets++;
        if (totalPackets == 1) {
            std::cout << "OusterStream: First packet received! Size: " << received << " bytes" << std::endl;
        }
        
        // Parse lidar packet
        // Ouster packets contain multiple columns of data
        // Each column has a header followed by pixel data for all beams
        
        const size_t columnHeaderSize = sizeof(LidarColumnHeader);
        const size_t pixelSize = sizeof(LidarPixel);
        const size_t columnSize = columnHeaderSize + m_beamCount * pixelSize;
        
        size_t offset = 0;
        while (offset + columnSize <= static_cast<size_t>(received)) {
            const LidarColumnHeader* header = reinterpret_cast<const LidarColumnHeader*>(buffer.data() + offset);
            const LidarPixel* pixels = reinterpret_cast<const LidarPixel*>(buffer.data() + offset + columnHeaderSize);
            
            // Check for new frame
            if (!frameStarted || header->frameId != currentFrameId) {
                if (frameStarted && !m_currentFrame.empty()) {
                    // Complete previous frame
                    double timestamp = static_cast<double>(header->timestamp) / 1e9;
                    m_lastTimestamp = timestamp;
                    m_frameCount++;
                    
                    if (m_frameCount <= 5 || m_frameCount % 100 == 0) {
                        std::cout << "OusterStream: Frame " << m_frameCount 
                                  << " complete with " << m_currentFrame.size() << " points" << std::endl;
                    }
                    
                    // Call callback
                    if (m_pointCloudCallback) {
                        m_pointCloudCallback(m_currentFrame, timestamp);
                    }
                    
                    // Add to queue
                    {
                        std::lock_guard<std::mutex> lock(m_pointCloudMutex);
                        if (m_pointCloudQueue.size() >= MAX_QUEUE_SIZE) {
                            m_pointCloudQueue.pop();
                            m_droppedFrames++;
                        }
                        m_pointCloudQueue.push({m_currentFrame, timestamp});
                    }
                }
                
                // Start new frame
                m_currentFrame.clear();
                currentFrameId = header->frameId;
                frameStarted = true;
            }
            
            // Calculate azimuth angle for this column
            float azimuth = 2.0f * M_PI * header->encoderCount / 90112.0f;  // Encoder ticks per revolution
            
            // Process each pixel in the column
            for (int beam = 0; beam < m_beamCount; beam += m_config.downsampleFactor) {
                const LidarPixel& pixel = pixels[beam];
                
                // Range in meters
                float range = pixel.range * 0.001f;  // mm to m
                
                // Skip invalid or out-of-range points
                if (range < m_config.minRange || range > m_config.maxRange) {
                    continue;
                }
                
                // Calculate 3D position
                float altitude = beamAltitudeAngles[beam];
                float az = azimuth + beamAzimuthAngles[beam];
                
                float x = range * cosf(altitude) * cosf(az);
                float y = range * cosf(altitude) * sinf(az);
                float z = range * sinf(altitude);
                
                // Normalize intensity (signal typically 0-65535)
                float intensity = static_cast<float>(pixel.signal) / 65535.0f;
                
                // Convert to PointCloudPoint
                PointCloudPoint point = ConvertPoint(x, y, z, intensity, range);
                m_currentFrame.push_back(point);
            }
            
            offset += columnSize;
        }
    }
    
    close(sock);
#else
    std::cerr << "OusterStream: Only macOS is currently supported" << std::endl;
#endif
    
    m_streaming = false;
}

PointCloudPoint OusterStream::ConvertPoint(float x, float y, float z, float intensity, float range) {
    PointCloudPoint point;
    
    // Apply sensor-to-world transform
    glm::vec4 pos(x, y, z, 1.0f);
    pos = m_config.sensorToWorld * pos;
    point.position = glm::vec3(pos);
    
    // Map to color
    point.color = MapToColor(intensity, range);
    
    // Set metadata
    point.weight = 1.0f;
    point.segmentId = -1;  // Unassigned - let segmentation algorithm handle this
    point.cameraId = m_config.cameraId;
    
    return point;
}

glm::vec3 OusterStream::MapToColor(float intensity, float range) {
    if (m_config.useIntensityAsColor) {
        // Grayscale based on intensity
        float v = std::clamp(intensity, 0.0f, 1.0f);
        return glm::vec3(v, v, v);
    } else if (m_config.useRangeAsColor) {
        // Color gradient based on range (blue = near, red = far)
        float t = std::clamp((range - m_config.minRange) / (m_config.maxRange - m_config.minRange), 0.0f, 1.0f);
        return glm::vec3(t, 0.2f, 1.0f - t);
    } else {
        // Default gray
        return glm::vec3(0.7f, 0.7f, 0.7f);
    }
}

bool OusterStream::PollPointCloud(std::vector<PointCloudPoint>& outPoints, double& outTimestamp) {
    std::lock_guard<std::mutex> lock(m_pointCloudMutex);
    if (m_pointCloudQueue.empty()) {
        return false;
    }
    
    auto& front = m_pointCloudQueue.front();
    outPoints = std::move(front.first);
    outTimestamp = front.second;
    m_pointCloudQueue.pop();
    return true;
}

bool OusterStream::PollIMU(OusterIMUData& outIMU) {
    std::lock_guard<std::mutex> lock(m_imuMutex);
    if (m_imuQueue.empty()) {
        return false;
    }
    
    outIMU = m_imuQueue.front();
    m_imuQueue.pop();
    return true;
}

bool OusterStream::WaitForPointCloud(std::vector<PointCloudPoint>& outPoints, double& outTimestamp, int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (PollPointCloud(outPoints, outTimestamp)) {
            return true;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeoutMs) {
            return false;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

std::string OusterStream::GetSensorInfo() const {
    return "Ouster OS0-128 at " + m_config.hostname + 
           " (Beams: " + std::to_string(m_beamCount) + 
           ", Columns: " + std::to_string(m_columnsPerFrame) + ")";
}

std::unique_ptr<OusterStream> CreateOusterStream(const std::string& hostname) {
    auto stream = std::make_unique<OusterStream>();
    OusterConfig config;
    config.hostname = hostname;
    stream->SetConfig(config);
    return stream;
}

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine
