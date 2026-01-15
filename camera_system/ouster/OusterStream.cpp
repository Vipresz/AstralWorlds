#include "OusterStream.h"
#include "../../engine/platform/Logger.h"
#include <cmath>
#include <chrono>
#include <iostream>
#include <cstdlib>
#include <thread>
#include <cstdio>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <cstring>
#include <exception>
#include <typeinfo>
#include <stdexcept>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

// Ouster C++ SDK includes (if available)
#ifdef OUSTER_SDK_AVAILABLE
#include "ouster/pcap_scan_source.h"
#include "ouster/lidar_scan.h"
#include "ouster/xyzlut.h"
#include "ouster/cartesian.h"  // For cartesian() function
#include "ouster/field.h"
#include "ouster/types.h"  // Contains SensorInfo definition and RANGE_UNIT

using namespace ouster::sdk;
using namespace ouster::sdk::core;
using namespace ouster::sdk::pcap;

// Include sensor headers if BUILD_SENSOR is defined (set by CMakeLists.txt)
#ifdef BUILD_SENSOR
#include "ouster/sensor_scan_source.h"  // For live streaming
#include "ouster/sensor_packet_source.h"  // For live packet streaming
using namespace ouster::sdk::sensor;
#define OUSTER_SENSOR_AVAILABLE 1
#else
#define OUSTER_SENSOR_AVAILABLE 0
#endif
#endif  // OUSTER_SDK_AVAILABLE

using EngineLogging::Logger;

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

OusterStream::OusterStream() {
    m_currentFrame.reserve(m_beamCount * m_columnsPerFrame);
    m_udpProfileLidar = "LEGACY";
    m_columnsPerPacket = 16;
    m_packetHeaderSize = 0;
    m_measurementBlockHeaderSize = 4;
    m_pixelStride = 12;
    m_encoderTicksPerRev = 90112;
    m_rangeUnit = 0.001f;
    
    std::memset(m_beamToLidarTransform, 0, 16 * sizeof(float));
    m_beamToLidarTransform[0 * 4 + 0] = 1.0f;
    m_beamToLidarTransform[1 * 4 + 1] = 1.0f;
    m_beamToLidarTransform[2 * 4 + 2] = 1.0f;
    m_beamToLidarTransform[3 * 4 + 3] = 1.0f;
    
    std::memset(m_lidarToSensorTransform, 0, 16 * sizeof(float));
    m_lidarToSensorTransform[0 * 4 + 0] = 1.0f;
    m_lidarToSensorTransform[1 * 4 + 1] = 1.0f;
    m_lidarToSensorTransform[2 * 4 + 2] = 1.0f;
    m_lidarToSensorTransform[3 * 4 + 3] = 1.0f;
}

OusterStream::~OusterStream() {
    StopStreaming();
    Disconnect();
}

void OusterStream::SetConfig(const OusterConfig& config) {
    m_config = config;
}

bool OusterStream::Connect() {
#ifdef __APPLE__
    LOG_INFO_FMT("OusterStream", "Connect() called - configuring sensor at %s", m_config.hostname.c_str());
    
    char curlCmd[512];
    
    LOG_INFO("OusterStream", "Setting UDP destination to auto...");
    snprintf(curlCmd, sizeof(curlCmd), 
             "curl -s -X POST http://%s/api/v1/sensor/cmd/set_udp_dest_auto --max-time 5 > /dev/null 2>&1",
             m_config.hostname.c_str());
    int ret = system(curlCmd);
    if (ret == 0) {
        LOG_INFO("OusterStream", "Configured sensor UDP destination to auto");
    } else {
        LOG_WARNING_FMT("OusterStream", "Failed to set UDP destination (curl returned %d) - sensor may already be configured", ret);
    }
    
    LOG_INFO_FMT("OusterStream", "Setting lidar port to %d...", m_config.lidarPort);
    snprintf(curlCmd, sizeof(curlCmd),
             "ouster-cli source %s config udp_port_lidar %d > /dev/null 2>&1",
             m_config.hostname.c_str(), m_config.lidarPort);
    ret = system(curlCmd);
    if (ret == 0) {
        LOG_INFO_FMT("OusterStream", "Set sensor lidar port to %d", m_config.lidarPort);
    } else {
        LOG_INFO("OusterStream", "Could not set lidar port via ouster-cli (may already be configured)");
    }
    
    LOG_INFO("OusterStream", "Skipping sensor reinitialization (assuming sensor is already configured)");
    LOG_INFO("OusterStream", "If no data is received, run setup_ouster.py first to configure the sensor");
    
    m_connected = true;
    LOG_INFO("OusterStream", "Connect() completed successfully");
    return true;
#else
    m_connected = true;
    return true;
#endif
}

void OusterStream::Disconnect() {
    StopStreaming();
    m_connected = false;
}

void OusterStream::StartStreaming() {
    if (m_streaming) {
        LOG_INFO("OusterStream", "Already streaming, skipping StartStreaming");
        return;
    }
    
    LOG_INFO("OusterStream", "StartStreaming() called - creating streaming thread");
    
    m_shouldStop = false;
    m_streaming = true;
    m_liveLastColumnTimestampNs.store(0);
    m_streamThread = std::make_unique<std::thread>(&OusterStream::StreamingThread, this);
    
    LOG_INFO("OusterStream", "Streaming thread created, waiting for it to initialize...");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}

void OusterStream::StopStreaming() {
    if (!m_streaming) return;
    
    m_shouldStop = true;
    if (m_streamThread && m_streamThread->joinable()) {
        m_streamThread->join();
    }
    m_streamThread.reset();
    
    if (!m_currentFrame.empty()) {
        uint64_t ts = m_liveLastColumnTimestampNs.load();
        if (ts == 0) {
            ts = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        }
        FlushCurrentFrame(ts, 0, 0, false);
    }
    
    m_streaming = false;
}

void OusterStream::ClearFileLoadedState() {
    m_packetLoadedFromFile = false;
    m_loadedPacketPoints.clear();
    m_loadedPacketTimestamp = 0.0;
    LOG_INFO("OusterStream", "Cleared file-loaded state");
}

void OusterStream::StreamingThread() {
#ifdef __APPLE__
    LOG_INFO("OusterStream", "StreamingThread started");
    
    // Set up a terminate handler to catch any uncaught exceptions that cause std::terminate
    // This will help us see what's causing the crash
    std::set_terminate([]() {
        LOG_ERROR("OusterStream", "std::terminate() called - uncaught exception in SDK");
        LOG_ERROR("OusterStream", "This usually means an exception was thrown but not caught");
        // Try to get current exception info if available
        try {
            auto exc = std::current_exception();
            if (exc) {
                std::rethrow_exception(exc);
            }
        } catch (const std::exception& e) {
            LOG_ERROR_FMT("OusterStream", "Uncaught exception: %s", e.what());
        } catch (...) {
            LOG_ERROR("OusterStream", "Uncaught exception of unknown type");
        }
        // Call abort to get a stack trace
        std::abort();
    });
    
#ifdef OUSTER_SDK_AVAILABLE
#if OUSTER_SENSOR_AVAILABLE
    // Try to use SDK for live streaming if available
    try {
        LOG_INFO("OusterStream", "Attempting to initialize Ouster SDK live streaming...");
        LOG_INFO_FMT("OusterStream", "Connecting to sensor at %s, lidar_port=%d", 
                     m_config.hostname.c_str(), m_config.lidarPort);
        
        // Create SensorScanSource for live streaming
        // Use the simpler constructor that takes hostname and options function
        // With firmware 2.5.3+, the SDK can properly configure the sensor
        // Note: The SDK will handle connection and port binding internally
        LOG_INFO("OusterStream", "Creating SensorScanSource (SDK will handle connection and ports)...");
        try {
            m_sdkScanSource = std::make_shared<SensorScanSource>(
                m_config.hostname,
                [this](SensorScanSourceOptions& opts) {
                    opts.lidar_port = m_config.lidarPort;
                    opts.imu_port = 7503;  // Default IMU port
                    opts.timeout = 10.0f;  // Timeout in seconds (float)
                    // With firmware 2.5.3+, allow SDK to configure sensor if needed
                    // Set to false to let SDK configure sensor, true to skip if already configured
                    opts.do_not_reinitialize = false;  // Let SDK configure sensor (firmware 2.5.3+)
                    // Set queue size to buffer a few scans (helps with processing latency)
                    opts.queue_size = 2;
                    // Optional: configure timeout for sensor configuration
                    opts.config_timeout = 60.0f;  // 60 seconds for sensor configuration
                    LOG_INFO_FMT("OusterStream", "SensorScanSourceOptions configured: lidar_port=%d, imu_port=7503", 
                            m_config.lidarPort);
                }
            );
            LOG_INFO("OusterStream", "SensorScanSource created successfully");
        } catch (const std::exception& e) {
            LOG_ERROR_FMT("OusterStream", "Failed to create SensorScanSource: %s", e.what());
            LOG_ERROR_FMT("OusterStream", "Exception type: %s", typeid(e).name());
            // Try to get more details
            try {
                std::rethrow_if_nested(e);
            } catch (const std::exception& nested) {
                LOG_ERROR_FMT("OusterStream", "Nested exception: %s", nested.what());
            } catch (...) {
                LOG_ERROR("OusterStream", "Nested exception of unknown type");
            }
            throw;  // Re-throw to be caught by outer catch
        } catch (...) {
            LOG_ERROR("OusterStream", "Failed to create SensorScanSource: unknown exception (not std::exception)");
            throw std::runtime_error("Unknown exception creating SensorScanSource");
        }
        
        // Check if the scan source is live
        try {
            if (!m_sdkScanSource->is_live()) {
                throw std::runtime_error("SensorScanSource is not live - sensor may not be configured or accessible");
            }
            LOG_INFO("OusterStream", "SensorScanSource is live");
        } catch (const std::exception& e) {
            LOG_ERROR_FMT("OusterStream", "Error checking is_live(): %s", e.what());
            throw;
        } catch (...) {
            LOG_ERROR("OusterStream", "Unknown error checking is_live()");
            throw std::runtime_error("Unknown error checking SensorScanSource live status");
        }
        
        // Get sensor info from the scan source
        try {
            const auto& sensorInfos = m_sdkScanSource->sensor_info();
            if (sensorInfos.empty() || !sensorInfos[0]) {
                throw std::runtime_error("No sensor info available from SensorScanSource");
            }
            m_sensorInfo = sensorInfos[0];
            LOG_INFO_FMT("OusterStream", "Retrieved sensor info: SN=%llu", 
                        static_cast<unsigned long long>(m_sensorInfo->sn));
        } catch (const std::exception& e) {
            LOG_ERROR_FMT("OusterStream", "Error getting sensor_info(): %s", e.what());
            throw;
        } catch (...) {
            LOG_ERROR("OusterStream", "Unknown error getting sensor_info()");
            throw std::runtime_error("Unknown error getting sensor info");
        }
        
        // Create XYZ lookup table (use_extrinsics = false for sensor frame coordinates)
        try {
            auto xyzlut = std::make_shared<XYZLut>(*m_sensorInfo, false);
            m_xyzlut_ptr = std::static_pointer_cast<void>(xyzlut);
            LOG_INFO("OusterStream", "XYZ lookup table created successfully");
        } catch (const std::exception& e) {
            LOG_ERROR_FMT("OusterStream", "Error creating XYZ lookup table: %s", e.what());
            throw;
        } catch (...) {
            LOG_ERROR("OusterStream", "Unknown error creating XYZ lookup table");
            throw std::runtime_error("Unknown error creating XYZ lookup table");
        }
        
        m_sdkStreamingInitialized = true;
        LOG_INFO("OusterStream", "Ouster SDK live streaming initialized successfully");
        LOG_INFO_FMT("OusterStream", "Sensor SN: %llu", static_cast<unsigned long long>(m_sensorInfo->sn));
        LOG_INFO_FMT("OusterStream", "Min range: %.2fm, Max range: %.2fm", m_config.minRange, m_config.maxRange);
        
        // Stream using SDK - iterate through scans
        // Use begin(0) to get iterator for first sensor (sensor_index = 0)
        int scanCount = 0;
        auto scanIt = m_sdkScanSource->begin(0);  // Get iterator for sensor index 0
        auto scanEnd = m_sdkScanSource->end();
        
        LOG_INFO("OusterStream", "Starting SDK scan iteration...");
        
        while (!m_shouldStop) {
            // The iterator will block/wait for new scans when incremented
            // If we've reached the end, it means the source is done (shouldn't happen for live streaming)
            if (scanIt == scanEnd) {
                LOG_WARNING("OusterStream", "SDK iterator reached end - this shouldn't happen for live streaming");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                // Try to get a new iterator
                scanIt = m_sdkScanSource->begin(0);
                scanEnd = m_sdkScanSource->end();
                continue;
            }
            
            // Dereference iterator to get LidarScanSet (may block waiting for next scan)
            LidarScanSet scanSet = *scanIt;
            ++scanIt;  // Move to next scan (this may block waiting for data)
            
            // Iterate through scans in the set (usually just one for single sensor)
            for (const auto& scanPtr : scanSet) {
                if (!scanPtr) {
                    continue;
                }
                
                const LidarScan& scan = *scanPtr;
                
                scanCount++;
                if (scanCount == 1 || scanCount % 10 == 0) {
                    LOG_INFO_FMT("OusterStream", "Received scan %d via SDK", scanCount);
                }
                
                // Convert scan to point cloud using SDK
                auto ranges = scan.field<uint32_t>(ChanField::RANGE);
                
                // Convert to XYZ using the lookup table
                auto xyzlut = std::static_pointer_cast<XYZLut>(m_xyzlut_ptr);
                auto xyz = (*xyzlut)(ranges);
                
                // Get range unit from sensor info (default RANGE_UNIT = 0.001 m/count)
                // Note: range_unit is typically in the metadata JSON, not in DataFormat
                // For now, use the constant RANGE_UNIT
                double rangeUnit = RANGE_UNIT;
                
                std::vector<PointCloudPoint> points;
                size_t w = scan.w;
                size_t h = scan.h;
                points.reserve(w * h);
                
                // Try to get signal/reflectivity fields (may not be available)
                // Check if fields exist before accessing using has_field
                // ChanField::SIGNAL is already a const char* string
                bool hasSignal = scan.has_field(ChanField::SIGNAL);
                bool hasReflectivity = scan.has_field(ChanField::REFLECTIVITY);
                
                for (size_t col = 0; col < w; col++) {
                    for (size_t row = 0; row < h; row++) {
                        uint32_t rangeCounts = ranges(row, col);
                        
                        // Skip zero range (invalid measurements)
                        if (rangeCounts == 0) {
                            continue;
                        }
                        
                        // Convert range counts to meters
                        double rangeM = static_cast<double>(rangeCounts) * rangeUnit;
                        
                        // Apply range filtering
                        if (rangeM < m_config.minRange || rangeM > m_config.maxRange) {
                            continue;
                        }
                        
                        // Get XYZ coordinates (xyz is h*w x 3 array, row-major)
                        size_t idx = row * w + col;
                        double x = xyz(idx, 0);
                        double y = xyz(idx, 1);
                        double z = xyz(idx, 2);
                        
                        // Get intensity/reflectivity
                        float intensity = 0.0f;
                        if (hasSignal) {
                            auto signalField = scan.field<uint16_t>(ChanField::SIGNAL);
                            intensity = static_cast<float>(signalField(row, col));
                        } else if (hasReflectivity) {
                            auto reflectivityField = scan.field<uint16_t>(ChanField::REFLECTIVITY);
                            intensity = static_cast<float>(reflectivityField(row, col));
                        }
                        
                        // Apply downsample
                        if (m_config.downsampleFactor > 1 && (row * w + col) % m_config.downsampleFactor != 0) {
                            continue;
                        }
                        
                        PointCloudPoint point;
                        // Convert to cm (position is stored in cm)
                        point.position = glm::vec3(
                            static_cast<float>(x * 100.0),
                            static_cast<float>(y * 100.0),
                            static_cast<float>(z * 100.0)
                        );
                        point.color = MapToColor(intensity, static_cast<float>(rangeM));
                        point.cameraId = m_config.cameraId;
                        
                        points.push_back(point);
                    }
                }
                
                // Get timestamp from scan
                // scan.timestamp() returns an array, we need to get a representative timestamp
                // Use the first timestamp value or current time
                double timestamp = 0.0;
                auto timestampField = scan.timestamp();
                if (timestampField.size() > 0) {
                    // Use the first timestamp value (in nanoseconds typically)
                    timestamp = static_cast<double>(timestampField(0)) * 1e-9;  // Convert ns to seconds
                } else {
                    // Fallback to current time
                    timestamp = std::chrono::duration<double>(
                        std::chrono::system_clock::now().time_since_epoch()).count();
                }
                
                // Queue point cloud
                {
                    std::lock_guard<std::mutex> lock(m_pointCloudMutex);
                    if (m_pointCloudQueue.size() >= MAX_QUEUE_SIZE) {
                        m_pointCloudQueue.pop();
                        m_droppedFrames++;
                    }
                    m_pointCloudQueue.push({std::move(points), timestamp});
                }
                
                m_frameCount++;
                m_lastTimestamp = timestamp;
            }  // End of scan set iteration
        }  // End of while loop
        
        LOG_INFO("OusterStream", "SDK streaming thread exiting");
        m_sdkScanSource.reset();
        m_sensorInfo.reset();
        m_xyzlut_ptr.reset();
        m_sdkStreamingInitialized = false;
        
        // SDK streaming completed normally, exit the function
        return;
        
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("OusterStream", "SDK live streaming failed: %s", e.what());
        LOG_ERROR_FMT("OusterStream", "Exception type: %s", typeid(e).name());
        // Try to get more details about the exception
        try {
            std::rethrow_if_nested(e);
        } catch (const std::exception& nested) {
            LOG_ERROR_FMT("OusterStream", "Nested exception: %s", nested.what());
        } catch (...) {
            LOG_ERROR("OusterStream", "Nested exception of unknown type");
        }
        LOG_WARNING("OusterStream", "Falling back to manual UDP socket streaming");
        m_sdkStreamingInitialized = false;
        // Clean up SDK resources if they were partially created
        if (m_sdkScanSource) {
            m_sdkScanSource.reset();
        }
        if (m_sensorInfo) {
            m_sensorInfo.reset();
        }
        if (m_xyzlut_ptr) {
            m_xyzlut_ptr.reset();
        }
        // Fall through to manual streaming
    } catch (...) {
        LOG_ERROR("OusterStream", "SDK live streaming failed: unknown exception (not std::exception)");
        LOG_WARNING("OusterStream", "Falling back to manual UDP socket streaming");
        m_sdkStreamingInitialized = false;
        // Clean up SDK resources if they were partially created
        if (m_sdkScanSource) {
            m_sdkScanSource.reset();
        }
        if (m_sensorInfo) {
            m_sensorInfo.reset();
        }
        if (m_xyzlut_ptr) {
            m_xyzlut_ptr.reset();
        }
        // Fall through to manual streaming
    }
    
    // If we reach here, SDK streaming failed or wasn't available
    // Fall through to manual streaming
    
#else  // OUSTER_SENSOR_AVAILABLE
    // Sensor library not available, use manual streaming
    LOG_INFO("OusterStream", "Ouster sensor library not available, using manual UDP socket streaming");
#endif  // OUSTER_SENSOR_AVAILABLE
#endif  // OUSTER_SDK_AVAILABLE
    
    // Manual UDP socket streaming (fallback or when SDK not available)
    {
    LOG_INFO("OusterStream", "Using manual UDP socket streaming");
    
    // Create UDP socket for lidar data
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        LOG_ERROR_FMT("OusterStream", "Failed to create socket (errno: %d)", errno);
        m_streaming = false;
        return;
    }
    LOG_INFO("OusterStream", "Socket created successfully");
    
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    int bufSize = 4 * 1024 * 1024;
    setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufSize, sizeof(bufSize));
    
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &reuse, sizeof(reuse));
    
    // Bind to lidar port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(m_config.lidarPort);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    LOG_INFO_FMT("OusterStream", "Attempting to bind to port %d...", m_config.lidarPort);
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOG_ERROR_FMT("OusterStream", "Failed to bind to port %d (errno: %d - %s)", 
                      m_config.lidarPort, errno, strerror(errno));
        close(sock);
        m_streaming = false;
        return;
    }
    
    LOG_INFO_FMT("OusterStream", "Successfully bound to UDP port %d", m_config.lidarPort);
    LOG_INFO_FMT("OusterStream", "Waiting for lidar data from %s...", m_config.hostname.c_str());
    LOG_INFO_FMT("OusterStream", "Min range: %.2fm, Max range: %.2fm", m_config.minRange, m_config.maxRange);
    
    int totalPackets = 0;
    
    // Buffer for receiving packets
    std::vector<uint8_t> buffer(65536);
    
    struct pollfd pfd;
    pfd.fd = sock;
    pfd.events = POLLIN;
    
    int pollIterations = 0;
    int recvFailures = 0;
    
    while (!m_shouldStop) {
        int ret = poll(&pfd, 1, 100);
        pollIterations++;
        
        if (pollIterations <= 20 || pollIterations % 50 == 0) {
            LOG_INFO_FMT("OusterStream", "Poll iteration %d: ret=%d, revents=0x%x, shouldStop=%d", 
                         pollIterations, ret, pfd.revents, m_shouldStop.load() ? 1 : 0);
        }
        
        if (ret < 0) {
            if (errno != EINTR) {
                LOG_ERROR_FMT("OusterStream", "poll() error: %s", strerror(errno));
            }
            continue;
        }
        
        if (ret == 0) {
            continue;
        }
        
        if (!(pfd.revents & POLLIN)) {
            if (pfd.revents & POLLERR) {
                LOG_WARNING("OusterStream", "Socket error detected in poll");
            }
            continue;
        }
        
        ssize_t received = recv(sock, buffer.data(), buffer.size(), 0);
        if (received <= 0) {
            recvFailures++;
            if (recvFailures <= 10) {
                LOG_WARNING_FMT("OusterStream", "recv failed: %zd (errno: %d - %s)", 
                               received, errno, strerror(errno));
            }
            continue;
        }
        
        totalPackets++;
        if (totalPackets == 1) {
            LOG_INFO_FMT("OusterStream", "First packet received! Size: %zd bytes", received);
            std::string hexPreview;
            size_t previewLen = std::min(static_cast<size_t>(received), size_t(64));
            for (size_t i = 0; i < previewLen; i++) {
                char hex[4];
                snprintf(hex, sizeof(hex), "%02x ", buffer[i]);
                hexPreview += hex;
            }
            LOG_INFO_FMT("OusterStream", "First packet hex dump (first 64 bytes): %s", hexPreview.c_str());
        }
        if (totalPackets <= 5 || totalPackets % 100 == 0) {
            LOG_INFO_FMT("OusterStream", "Packet %d received, size: %zd bytes", totalPackets, received);
        }
        
        ProcessLidarPacket(buffer.data(), received);
    }
    
    if (!m_currentFrame.empty()) {
        uint64_t ts = m_liveLastColumnTimestampNs.load();
        if (ts == 0) {
            ts = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        }
        FlushCurrentFrame(ts, 0, 0, false);
    }
    
    close(sock);
    }  // End of manual streaming block
    
#else
    std::cerr << "OusterStream: Only macOS is currently supported" << std::endl;
#endif
    
    m_streaming = false;
}

PointCloudPoint OusterStream::CalculatePoint3D(uint16_t measurementId, int beam, float range, uint16_t signal,
                                                const std::vector<float>& beamAltitudeAngles,
                                                const std::vector<float>& beamAzimuthAngles) {
    int columnIndex = static_cast<int>(measurementId);
    int shift = (!m_pixelShiftByRow.empty() && beam < static_cast<int>(m_pixelShiftByRow.size())) 
                ? m_pixelShiftByRow[beam] : 0;
    int correctedColumn = (columnIndex + shift + m_columnsPerFrame) % m_columnsPerFrame;
    
    float theta_encoder = 2.0f * M_PI * (1.0f - static_cast<float>(correctedColumn) / static_cast<float>(m_columnsPerFrame));
    float theta_azimuth = -2.0f * M_PI * (beamAzimuthAngles[beam] / 360.0f);
    float phi = 2.0f * M_PI * (beamAltitudeAngles[beam] / 360.0f);
    
    float n = m_lidarOriginToBeamOriginMm;
    float r_mm = range * 1000.0f;
    float x_lidar = (r_mm - n) * cosf(theta_encoder + theta_azimuth) * cosf(phi) + m_beamToLidarTransform[0 * 4 + 3] * cosf(theta_encoder);
    float y_lidar = (r_mm - n) * sinf(theta_encoder + theta_azimuth) * cosf(phi) + m_beamToLidarTransform[0 * 4 + 3] * sinf(theta_encoder);
    float z_lidar = (r_mm - n) * sinf(phi) + m_beamToLidarTransform[2 * 4 + 3];
    
    float x_sensor_mm = m_lidarToSensorTransform[0 * 4 + 0] * x_lidar + m_lidarToSensorTransform[0 * 4 + 1] * y_lidar + m_lidarToSensorTransform[0 * 4 + 2] * z_lidar + m_lidarToSensorTransform[0 * 4 + 3];
    float y_sensor_mm = m_lidarToSensorTransform[1 * 4 + 0] * x_lidar + m_lidarToSensorTransform[1 * 4 + 1] * y_lidar + m_lidarToSensorTransform[1 * 4 + 2] * z_lidar + m_lidarToSensorTransform[1 * 4 + 3];
    float z_sensor_mm = m_lidarToSensorTransform[2 * 4 + 0] * x_lidar + m_lidarToSensorTransform[2 * 4 + 1] * y_lidar + m_lidarToSensorTransform[2 * 4 + 2] * z_lidar + m_lidarToSensorTransform[2 * 4 + 3];
    
    float x = x_sensor_mm * 0.1f;
    float y = y_sensor_mm * 0.1f;
    float z = z_sensor_mm * 0.1f;
    
    float intensity = std::max(static_cast<float>(signal) / 65535.0f, 0.1f);
    
    return ConvertPoint(x, y, z, intensity, range);
}

void OusterStream::FlushCurrentFrame(uint64_t lastTimestamp, uint32_t minEncoder, uint32_t maxEncoder, bool encoderInitialized) {
    if (m_currentFrame.empty()) return;
    
    double timestamp = static_cast<double>(lastTimestamp) / 1e9;
    m_lastTimestamp = timestamp;
    m_frameCount++;
    
    if (encoderInitialized) {
        uint32_t encoderSpan = (maxEncoder - minEncoder) % m_encoderTicksPerRev;
        LOG_INFO_FMT("OusterStream", "Frame %d complete: %zu points, encoder span: %u/%u (%.1f%%)", 
                     m_frameCount.load(), m_currentFrame.size(), 
                     encoderSpan, m_encoderTicksPerRev, (encoderSpan * 100.0f / static_cast<float>(m_encoderTicksPerRev)));
    } else {
        LOG_INFO_FMT("OusterStream", "Frame %d complete with %zu points", 
                     m_frameCount.load(), m_currentFrame.size());
    }
    
    if (m_pointCloudCallback) {
        m_pointCloudCallback(m_currentFrame, timestamp);
    }
    
    {
        std::lock_guard<std::mutex> lock(m_pointCloudMutex);
        if (m_pointCloudQueue.size() >= MAX_QUEUE_SIZE) {
            m_pointCloudQueue.pop();
            m_droppedFrames++;
        }
        m_pointCloudQueue.push({m_currentFrame, timestamp});
    }
    
    m_currentFrame.clear();
}

PointCloudPoint OusterStream::ConvertPoint(float x, float y, float z, float intensity, float range) {
    PointCloudPoint point;
    
    glm::vec4 pos(x, y, z, 1.0f);
    pos = m_config.sensorToWorld * pos;
    point.position = glm::vec3(pos);
    point.color = MapToColor(intensity, range);
    point.weight = 1.0f;
    point.segmentId = -1;
    point.cameraId = m_config.cameraId;
    
    return point;
}

glm::vec3 OusterStream::MapToColor(float intensity, float range) {
    if (m_config.useIntensityAsColor) {
        float v = std::clamp(intensity, 0.0f, 1.0f);
        return glm::vec3(v, v, v);
    } else if (m_config.useRangeAsColor) {
        float t = std::clamp((range - m_config.minRange) / (m_config.maxRange - m_config.minRange), 0.0f, 1.0f);
        return glm::vec3(t, 0.2f, 1.0f - t);
    } else {
        return glm::vec3(0.7f, 0.7f, 0.7f);
    }
}

bool OusterStream::PollPointCloud(std::vector<PointCloudPoint>& outPoints, double& outTimestamp) {
    if (m_packetLoadedFromFile && !m_loadedPacketPoints.empty()) {
        outPoints = m_loadedPacketPoints;
        outTimestamp = m_loadedPacketTimestamp;
        return true;
    }
    
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


bool OusterStream::LoadPacketFromFile(const std::string& filename) {
#ifdef __APPLE__
    LOG_INFO_FMT("OusterStream", "Loading packet from file: %s", filename.c_str());
    
    std::vector<std::string> pathsToTry;
    pathsToTry.push_back(filename);
    
    try {
        pathsToTry.push_back((std::filesystem::current_path() / filename).string());
    } catch (...) {}
    
    char exePath[1024];
    uint32_t size = sizeof(exePath);
    if (_NSGetExecutablePath(exePath, &size) == 0) {
        try {
            std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
            pathsToTry.push_back((exeDir / "../" / filename).string());
            pathsToTry.push_back((exeDir / "../../" / filename).string());
            pathsToTry.push_back((exeDir / filename).string());
        } catch (...) {}
    }
    
    pathsToTry.push_back("../" + filename);
    pathsToTry.push_back("../../" + filename);
    pathsToTry.push_back("../../../" + filename);
    
    try {
        std::filesystem::path currentPath = std::filesystem::current_path();
        if (currentPath.filename() == "bin" || currentPath.filename() == "Release" || currentPath.filename() == "Debug") {
            std::filesystem::path thegamePath = currentPath.parent_path().parent_path();
            pathsToTry.push_back((thegamePath / filename).string());
        }
        if (currentPath.filename() == "thegame") {
            pathsToTry.push_back((currentPath / filename).string());
        }
    } catch (...) {}
    
    FILE* file = nullptr;
    std::string actualPath;
    
    // Try each path
    for (const auto& path : pathsToTry) {
        try {
            if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path)) {
                file = fopen(path.c_str(), "rb");
                if (file) {
                    actualPath = path;
                    LOG_INFO_FMT("OusterStream", "Found packet file at: %s", path.c_str());
                    break;
                }
            }
        } catch (...) {
            // Try next path
            continue;
        }
    }
    
    if (!file) {
        LOG_ERROR_FMT("OusterStream", "Failed to open packet file: %s", filename.c_str());
        LOG_INFO("OusterStream", "Tried the following paths:");
        for (const auto& path : pathsToTry) {
            try {
                bool exists = std::filesystem::exists(path);
                LOG_INFO_FMT("OusterStream", "  - %s %s", path.c_str(), exists ? "(exists)" : "(not found)");
            } catch (...) {
                LOG_INFO_FMT("OusterStream", "  - %s (error checking)", path.c_str());
            }
        }
        return false;
    }
    
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (fileSize <= 0 || fileSize > 2 * 1024 * 1024) {
        LOG_ERROR_FMT("OusterStream", "Invalid packet file size: %ld bytes (expected up to 2MB for full frame)", fileSize);
        fclose(file);
        return false;
    }
    
    std::vector<uint8_t> buffer(fileSize);
    size_t read = fread(buffer.data(), 1, fileSize, file);
    fclose(file);
    
    if (read != static_cast<size_t>(fileSize)) {
        LOG_ERROR_FMT("OusterStream", "Failed to read packet file: read %zu bytes, expected %ld", read, fileSize);
        return false;
    }
    
    LOG_INFO_FMT("OusterStream", "Loaded packet file: %zu bytes", read);
    
    m_loadedPacketPoints.clear();
    m_packetLoadedFromFile = true;
    
    ProcessLidarPacket(buffer.data(), read);
    
    {
        std::lock_guard<std::mutex> lock(m_pointCloudMutex);
        while (!m_pointCloudQueue.empty()) {
            auto& frame = m_pointCloudQueue.front();
            m_loadedPacketPoints.insert(m_loadedPacketPoints.end(), 
                                       frame.first.begin(), frame.first.end());
            m_loadedPacketTimestamp = frame.second;
            m_pointCloudQueue.pop();
        }
    }
    
    LOG_INFO_FMT("OusterStream", "Aggregated %zu points from loaded packet into persistent frame", 
                 m_loadedPacketPoints.size());
    
    if (!m_loadedPacketPoints.empty()) {
        float minX = m_loadedPacketPoints[0].position.x, maxX = m_loadedPacketPoints[0].position.x;
        float minY = m_loadedPacketPoints[0].position.y, maxY = m_loadedPacketPoints[0].position.y;
        float minZ = m_loadedPacketPoints[0].position.z, maxZ = m_loadedPacketPoints[0].position.z;
        float minRange = 0.0f, maxRange = 0.0f;
        bool firstRange = true;
        
        for (const auto& point : m_loadedPacketPoints) {
            minX = std::min(minX, point.position.x);
            maxX = std::max(maxX, point.position.x);
            minY = std::min(minY, point.position.y);
            maxY = std::max(maxY, point.position.y);
            minZ = std::min(minZ, point.position.z);
            maxZ = std::max(maxZ, point.position.z);
            
            float range = std::sqrt(point.position.x * point.position.x + 
                                   point.position.y * point.position.y + 
                                   point.position.z * point.position.z) / 100.0f;
            if (firstRange) {
                minRange = maxRange = range;
                firstRange = false;
            } else {
                minRange = std::min(minRange, range);
                maxRange = std::max(maxRange, range);
            }
        }
        
        LOG_INFO("OusterStream", "Point cloud statistics:");
        LOG_INFO_FMT("OusterStream", "  X: [%.3f, %.3f] cm (%.3f to %.3f m)", minX, maxX, minX/100.0f, maxX/100.0f);
        LOG_INFO_FMT("OusterStream", "  Y: [%.3f, %.3f] cm (%.3f to %.3f m)", minY, maxY, minY/100.0f, maxY/100.0f);
        LOG_INFO_FMT("OusterStream", "  Z: [%.3f, %.3f] cm (%.3f to %.3f m)", minZ, maxZ, minZ/100.0f, maxZ/100.0f);
        LOG_INFO_FMT("OusterStream", "  Range: [%.3f, %.3f] m", minRange, maxRange);
        LOG_INFO_FMT("OusterStream", "  Total points: %zu", m_loadedPacketPoints.size());
        
        size_t sampleSize = std::min(size_t(5), m_loadedPacketPoints.size());
        LOG_INFO_FMT("OusterStream", "  Sample points (first %zu):", sampleSize);
        for (size_t i = 0; i < sampleSize; i++) {
            const auto& p = m_loadedPacketPoints[i];
            float range = std::sqrt(p.position.x * p.position.x + 
                                   p.position.y * p.position.y + 
                                   p.position.z * p.position.z) / 100.0f;
            LOG_INFO_FMT("OusterStream", "    [%zu] x=%.3f, y=%.3f, z=%.3f cm, range=%.3f m, color=(%.3f,%.3f,%.3f)", 
                         i, p.position.x, p.position.y, p.position.z, range, 
                         p.color.r, p.color.g, p.color.b);
        }
    }
    
    m_connected = true;
    m_streaming = false;
    LOG_INFO("OusterStream", "Packet loaded and processed, Ouster stream is now active.");
    
    return true;
#else
    LOG_ERROR("OusterStream", "LoadPacketFromFile: Only macOS is currently supported");
    return false;
#endif
}

bool OusterStream::LoadPcapFromFile(const std::string& pcapFile, const std::string& metadataFile) {
#ifdef __APPLE__
    LOG_INFO_FMT("OusterStream", "Loading PCAP from file: %s with metadata: %s", pcapFile.c_str(), metadataFile.c_str());
    
#ifdef OUSTER_SDK_AVAILABLE
    // Use Ouster C++ SDK for robust PCAP parsing
    try {
        // Resolve file paths
        std::string actualPcapPath = pcapFile;
        std::string actualMetadataPath = metadataFile;
        
        // Try to find files if paths are relative
        auto findFile = [](const std::string& filename) -> std::string {
            if (std::filesystem::exists(filename)) return filename;
            
            std::vector<std::string> pathsToTry = {
                filename,
                (std::filesystem::current_path() / filename).string(),
                "../" + filename,
                "../../" + filename
            };
            
            char exePath[1024];
            uint32_t size = sizeof(exePath);
            if (_NSGetExecutablePath(exePath, &size) == 0) {
                try {
                    std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
                    pathsToTry.push_back((exeDir / filename).string());
                    pathsToTry.push_back((exeDir / "../" / filename).string());
                    pathsToTry.push_back((exeDir / "../../" / filename).string());
                } catch (...) {}
            }
            
            for (const auto& path : pathsToTry) {
                if (std::filesystem::exists(path) && std::filesystem::is_regular_file(path)) {
                    return path;
                }
            }
            return filename; // Return original if not found
        };
        
        actualPcapPath = findFile(pcapFile);
        actualMetadataPath = findFile(metadataFile);
        
        if (!std::filesystem::exists(actualPcapPath)) {
            LOG_ERROR_FMT("OusterStream", "PCAP file not found: %s", pcapFile.c_str());
            return false;
        }
        
        if (!std::filesystem::exists(actualMetadataPath)) {
            LOG_ERROR_FMT("OusterStream", "Metadata file not found: %s", metadataFile.c_str());
            return false;
        }
        
        LOG_INFO_FMT("OusterStream", "Using PCAP: %s, Metadata: %s", actualPcapPath.c_str(), actualMetadataPath.c_str());
        
        // Configure PcapScanSource options
        PcapScanSourceOptions opts;
        opts.meta = {actualMetadataPath};
        opts.index = true; // Index the PCAP for better performance
        opts.field_names = nonstd::make_optional(std::vector<std::string>{"RANGE", "SIGNAL"}); // Only decode fields we need
        
        // Create PcapScanSource
        PcapScanSource scanSource(actualPcapPath, [&opts](PcapScanSourceOptions& o) { o = opts; });
        
        // Get sensor info
        const auto& sensorInfos = scanSource.sensor_info();
        if (sensorInfos.empty()) {
            LOG_ERROR("OusterStream", "No sensor info found in PCAP");
            return false;
        }
        
        if (!sensorInfos[0]) {
            LOG_ERROR("OusterStream", "Sensor info is null");
            return false;
        }
        const SensorInfo& sensorInfo = *sensorInfos[0];
        LOG_INFO_FMT("OusterStream", "Sensor: %s (SN: %llu)", sensorInfo.prod_line.c_str(), static_cast<unsigned long long>(sensorInfo.sn));
        
        // Create XYZ lookup table
        XYZLut lut(sensorInfo, false); // false = don't use extrinsics
        
        // Clear previous data
        m_loadedPacketPoints.clear();
        m_packetLoadedFromFile = true;
        m_connected = true;
        m_streaming = false;
        
        // Iterate through scans
        size_t scanCount = 0;
        size_t totalPoints = 0;
        
        auto scanIt = scanSource.begin();
        auto scanEnd = scanSource.end();
        
        for (; scanIt != scanEnd; ++scanIt) {
            LidarScanSet scanSet = *scanIt;
            
            // Iterate through scans in the set (usually just one for single sensor)
            for (const auto& scanPtr : scanSet) {
                if (!scanPtr) {
                    continue;
                }
                
                const LidarScan& scan = *scanPtr;
                scanCount++;
                
                // Get range and signal fields
                auto ranges = scan.field<uint32_t>(ChanField::RANGE);
                auto signals = scan.field<uint16_t>(ChanField::SIGNAL);
                
                // Convert to XYZ point cloud
                auto xyz = lut(ranges);
                
                // Use constant range unit (0.001 meters per count)
                constexpr double rangeUnit = RANGE_UNIT;
                
                // Process points
                size_t w = scan.w;
                size_t h = scan.h;
                
                for (size_t col = 0; col < w; col++) {
                    for (size_t row = 0; row < h; row++) {
                        uint32_t rangeCounts = ranges(row, col);
                        uint16_t signal = signals(row, col);
                        
                        // Convert range counts to meters
                        double rangeM = static_cast<double>(rangeCounts) * rangeUnit;
                        
                        // Apply range filtering
                        if (rangeM <= 0.0 || rangeM < m_config.minRange || rangeM > m_config.maxRange) {
                            continue;
                        }
                        
                        // Get XYZ coordinates (xyz is h*w x 3 array, row-major)
                        size_t idx = row * w + col;
                        double x = xyz(idx, 0);
                        double y = xyz(idx, 1);
                        double z = xyz(idx, 2);
                        
                        // Convert to PointCloudPoint (position in cm, color from intensity/range)
                        PointCloudPoint point;
                        point.position = glm::vec3(
                            static_cast<float>(x * 100.0), // Convert to cm
                            static_cast<float>(y * 100.0),
                            static_cast<float>(z * 100.0)
                        );
                        point.cameraId = m_config.cameraId;
                        
                        // Apply color mapping (using signal as intensity, rangeM as range)
                        float intensity = static_cast<float>(signal);
                        float range = static_cast<float>(rangeM);
                        glm::vec3 color = MapToColor(intensity, range);
                        point.color = color;
                        
                        m_loadedPacketPoints.push_back(point);
                        totalPoints++;
                    }
                }
                
                if (scanCount % 10 == 0 || scanCount == 1) {
                    LOG_INFO_FMT("OusterStream", "Processed scan %zu: %zu points (total: %zu)", 
                                scanCount, totalPoints, m_loadedPacketPoints.size());
                }
            }
        }
        
        LOG_INFO_FMT("OusterStream", "PCAP loaded: %zu scans, %zu total points extracted", 
                    scanCount, totalPoints);
        
        if (m_loadedPacketPoints.empty()) {
            LOG_WARNING("OusterStream", "No points extracted from PCAP file");
            return false;
        }
        
        // Set timestamp from last scan
        m_loadedPacketTimestamp = static_cast<double>(scanCount) * 0.1; // Approximate
        
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR_FMT("OusterStream", "Exception loading PCAP with SDK: %s", e.what());
        LOG_WARNING("OusterStream", "Falling back to manual PCAP parsing");
        return LoadPcapFromFileManual(this, pcapFile, metadataFile);
    }
#else
    // Fallback to manual parsing if SDK not available
    LOG_WARNING("OusterStream", "Ouster SDK not available, using manual PCAP parsing");
    return LoadPcapFromFileManual(this, pcapFile, metadataFile);
#endif
#else
    LOG_ERROR("OusterStream", "LoadPcapFromFile: Only macOS is currently supported");
    return false;
#endif
}

void OusterStream::ProcessLidarPacket(const uint8_t* data, size_t size) {
    // Use manual parsing for live streaming (SDK doesn't provide live packet source in this version)
    ProcessLidarPacketManual(this, data, size);
}

bool OusterStream::ParseMetadataFile(const std::string& metadataFile) {
    FILE* file = fopen(metadataFile.c_str(), "r");
    if (!file) {
        LOG_ERROR_FMT("OusterStream", "Failed to open metadata file: %s", metadataFile.c_str());
        return false;
    }
    
    // Read entire file
    fseek(file, 0, SEEK_END);
    long fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (fileSize <= 0 || fileSize > 1024 * 1024) { // Max 1MB
        fclose(file);
        return false;
    }
    
    std::vector<char> buffer(fileSize + 1);
    size_t read = fread(buffer.data(), 1, fileSize, file);
    fclose(file);
    
    if (read != static_cast<size_t>(fileSize)) {
        return false;
    }
    buffer[fileSize] = '\0';
    
    std::string jsonStr(buffer.data());
    
    size_t altPos = jsonStr.find("\"beam_altitude_angles\"");
    if (altPos != std::string::npos) {
        size_t arrayStart = jsonStr.find('[', altPos);
        if (arrayStart != std::string::npos) {
            size_t arrayEnd = jsonStr.find(']', arrayStart);
            if (arrayEnd != std::string::npos) {
                std::string arrayStr = jsonStr.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
                m_beamAltitudeAngles.clear();
                
                size_t pos = 0;
                while (pos < arrayStr.length()) {
                    while (pos < arrayStr.length() && (arrayStr[pos] == ' ' || arrayStr[pos] == '\n' || arrayStr[pos] == ',' || arrayStr[pos] == '\t')) {
                        pos++;
                    }
                    if (pos >= arrayStr.length()) break;
                    
                    size_t end = pos;
                    while (end < arrayStr.length() && arrayStr[end] != ',' && arrayStr[end] != ' ' && arrayStr[end] != '\n' && arrayStr[end] != '\t') {
                        end++;
                    }
                    
                    if (end > pos) {
                        std::string numStr = arrayStr.substr(pos, end - pos);
                        try {
                            float angle = std::stof(numStr);
                            m_beamAltitudeAngles.push_back(angle);
                        } catch (...) {
                        }
                    }
                    pos = end;
                }
                
                LOG_INFO_FMT("OusterStream", "Parsed %zu beam altitude angles from metadata", m_beamAltitudeAngles.size());
            }
        }
    }
    
    // Parse beam_azimuth_angles
    size_t azPos = jsonStr.find("\"beam_azimuth_angles\"");
    if (azPos != std::string::npos) {
        size_t arrayStart = jsonStr.find('[', azPos);
        if (arrayStart != std::string::npos) {
            size_t arrayEnd = jsonStr.find(']', arrayStart);
            if (arrayEnd != std::string::npos) {
                std::string arrayStr = jsonStr.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
                m_beamAzimuthAngles.clear();
                
                size_t pos = 0;
                while (pos < arrayStr.length()) {
                    while (pos < arrayStr.length() && (arrayStr[pos] == ' ' || arrayStr[pos] == '\n' || arrayStr[pos] == ',' || arrayStr[pos] == '\t')) {
                        pos++;
                    }
                    if (pos >= arrayStr.length()) break;
                    
                    size_t end = pos;
                    while (end < arrayStr.length() && arrayStr[end] != ',' && arrayStr[end] != ' ' && arrayStr[end] != '\n' && arrayStr[end] != '\t') {
                        end++;
                    }
                    
                    if (end > pos) {
                        std::string numStr = arrayStr.substr(pos, end - pos);
                        try {
                            float angle = std::stof(numStr);
                            m_beamAzimuthAngles.push_back(angle);
                        } catch (...) {
                        }
                    }
                    pos = end;
                }
                
                LOG_INFO_FMT("OusterStream", "Parsed %zu beam azimuth angles from metadata", m_beamAzimuthAngles.size());
            }
        }
    }
    
    auto FindNestedField = [&jsonStr](const std::string& parent, const std::string& field) -> size_t {
        size_t parentPos = jsonStr.find("\"" + parent + "\"");
        if (parentPos == std::string::npos) return std::string::npos;
        
        size_t braceStart = jsonStr.find('{', parentPos);
        if (braceStart == std::string::npos) return std::string::npos;
        
        int braceDepth = 1;
        size_t pos = braceStart + 1;
        while (pos < jsonStr.length() && braceDepth > 0) {
            if (jsonStr[pos] == '{') braceDepth++;
            else if (jsonStr[pos] == '}') braceDepth--;
            pos++;
        }
        size_t braceEnd = pos - 1;
        
        size_t fieldPos = jsonStr.find("\"" + field + "\"", braceStart);
        if (fieldPos != std::string::npos && fieldPos < braceEnd) {
            return fieldPos;
        }
        return std::string::npos;
    };
    
    auto ExtractStringValue = [&jsonStr](size_t fieldPos) -> std::string {
        size_t colonPos = jsonStr.find(':', fieldPos);
        if (colonPos == std::string::npos) return "";
        
        size_t quoteStart = jsonStr.find('"', colonPos);
        if (quoteStart == std::string::npos) return "";
        
        size_t quoteEnd = jsonStr.find('"', quoteStart + 1);
        if (quoteEnd == std::string::npos) return "";
        
        return jsonStr.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
    };
    
    auto ExtractNumberValue = [&jsonStr](size_t fieldPos) -> float {
        size_t colonPos = jsonStr.find(':', fieldPos);
        if (colonPos == std::string::npos) return 0.0f;
        
        size_t numStart = jsonStr.find_first_of("0123456789.-", colonPos);
        if (numStart == std::string::npos) return 0.0f;
        
        size_t numEnd = numStart;
        while (numEnd < jsonStr.length() && 
               (std::isdigit(jsonStr[numEnd]) || jsonStr[numEnd] == '.' || jsonStr[numEnd] == 'e' || 
                jsonStr[numEnd] == 'E' || jsonStr[numEnd] == '+' || jsonStr[numEnd] == '-')) {
            numEnd++;
        }
        
        try {
            return std::stof(jsonStr.substr(numStart, numEnd - numStart));
        } catch (...) {
            return 0.0f;
        }
    };
    
    size_t profilePos = FindNestedField("data_format", "udp_profile_lidar");
    if (profilePos == std::string::npos) {
        profilePos = FindNestedField("lidar_data_format", "udp_profile_lidar");
    }
    if (profilePos == std::string::npos) {
        profilePos = jsonStr.find("\"udp_profile_lidar\"");
    }
    
    if (profilePos != std::string::npos) {
        m_udpProfileLidar = ExtractStringValue(profilePos);
        LOG_INFO_FMT("OusterStream", "Parsed udp_profile_lidar: '%s'", m_udpProfileLidar.c_str());
        
        if (m_udpProfileLidar == "LEGACY") {
            m_pixelStride = 12;
            m_packetHeaderSize = 0;
            m_measurementBlockHeaderSize = 4;
        } else if (m_udpProfileLidar == "RNG19_RFL8_SIG16_NIR16") {
            m_pixelStride = 8;
            m_packetHeaderSize = 0;
            m_measurementBlockHeaderSize = 4;
        } else if (m_udpProfileLidar == "RNG15_RFL8_NIR8") {
            m_pixelStride = 4;
            m_packetHeaderSize = 0;
            m_measurementBlockHeaderSize = 4;
        } else if (m_udpProfileLidar == "RNG19_RFL8_SIG16_NIR16_DUAL") {
            m_pixelStride = 16;
            m_packetHeaderSize = 0;
            m_measurementBlockHeaderSize = 4;
        } else {
            LOG_WARNING_FMT("OusterStream", "Unknown UDP profile: '%s', using LEGACY defaults", m_udpProfileLidar.c_str());
            m_pixelStride = 12;
            m_packetHeaderSize = 0;
            m_measurementBlockHeaderSize = 4;
        }
        LOG_INFO_FMT("OusterStream", "Profile '%s': pixelStride=%d, blockHeaderSize=%d", 
                    m_udpProfileLidar.c_str(), m_pixelStride, m_measurementBlockHeaderSize);
    } else {
        LOG_WARNING("OusterStream", "udp_profile_lidar not found, using LEGACY defaults");
        m_udpProfileLidar = "LEGACY";
        m_pixelStride = 12;
        m_packetHeaderSize = 0;
        m_measurementBlockHeaderSize = 4;
    }
    
    size_t rangeUnitPos = FindNestedField("data_format", "range_unit");
    if (rangeUnitPos == std::string::npos) {
        rangeUnitPos = FindNestedField("lidar_data_format", "range_unit");
    }
    if (rangeUnitPos == std::string::npos) {
        rangeUnitPos = jsonStr.find("\"range_unit\"");
    }
    
    if (rangeUnitPos != std::string::npos) {
        m_rangeUnit = ExtractNumberValue(rangeUnitPos);
        if (m_rangeUnit > 0.0f) {
            LOG_INFO_FMT("OusterStream", "Parsed range_unit: %.6f m/count", m_rangeUnit);
        } else {
            LOG_WARNING("OusterStream", "Failed to parse range_unit, using default 0.001");
            m_rangeUnit = 0.001f;
        }
    } else {
        LOG_INFO("OusterStream", "range_unit not found in metadata, using default 0.001 m/count");
        m_rangeUnit = 0.001f;
    }
    
    auto FindNestedNumber = [&](const std::string& parent, const std::string& field) -> int {
        size_t fieldPos = FindNestedField(parent, field);
        if (fieldPos == std::string::npos) return -1;
        return static_cast<int>(ExtractNumberValue(fieldPos));
    };
    
    int colsPerPacket = FindNestedNumber("data_format", "columns_per_packet");
    if (colsPerPacket <= 0) {
        colsPerPacket = FindNestedNumber("lidar_data_format", "columns_per_packet");
    }
    if (colsPerPacket <= 0) {
        size_t colsPerPacketPos = jsonStr.find("\"columns_per_packet\"");
        if (colsPerPacketPos != std::string::npos) {
            colsPerPacket = static_cast<int>(ExtractNumberValue(colsPerPacketPos));
        }
    }
    if (colsPerPacket > 0) {
        m_columnsPerPacket = colsPerPacket;
        LOG_INFO_FMT("OusterStream", "Parsed columns_per_packet: %d", m_columnsPerPacket);
    }
    
    int colsPerFrame = FindNestedNumber("data_format", "columns_per_frame");
    if (colsPerFrame <= 0) {
        colsPerFrame = FindNestedNumber("lidar_data_format", "columns_per_frame");
    }
    if (colsPerFrame <= 0) {
        size_t colsPos = jsonStr.find("\"columns_per_frame\"");
        if (colsPos != std::string::npos) {
            colsPerFrame = static_cast<int>(ExtractNumberValue(colsPos));
        }
    }
    if (colsPerFrame > 0) {
        m_columnsPerFrame = colsPerFrame;
        LOG_INFO_FMT("OusterStream", "Parsed columns_per_frame: %d", m_columnsPerFrame);
        
        if (m_columnsPerFrame == 1024) {
            m_encoderTicksPerRev = 90112;
        } else {
            m_encoderTicksPerRev = m_columnsPerFrame * 88;
            LOG_INFO_FMT("OusterStream", "Computed encoder_ticks_per_rev: %d (from %d columns * 88)", 
                        m_encoderTicksPerRev, m_columnsPerFrame);
        }
    }
    
    int pixelsPerColumn = FindNestedNumber("data_format", "pixels_per_column");
    if (pixelsPerColumn <= 0) {
        pixelsPerColumn = FindNestedNumber("lidar_data_format", "pixels_per_column");
    }
    if (pixelsPerColumn <= 0) {
        size_t pixelsPos = jsonStr.find("\"pixels_per_column\"");
        if (pixelsPos != std::string::npos) {
            pixelsPerColumn = static_cast<int>(ExtractNumberValue(pixelsPos));
        }
    }
    if (pixelsPerColumn > 0) {
        m_beamCount = pixelsPerColumn;
        LOG_INFO_FMT("OusterStream", "Parsed pixels_per_column: %d (beam count set to %d)", pixelsPerColumn, m_beamCount);
    }
    
    auto FindNestedArray = [&FindNestedField](const std::string& parent, const std::string& field) -> size_t {
        size_t fieldPos = FindNestedField(parent, field);
        if (fieldPos == std::string::npos) return std::string::npos;
        return fieldPos;
    };
    
    size_t shiftPos = FindNestedArray("data_format", "pixel_shift_by_row");
    if (shiftPos == std::string::npos) {
        shiftPos = FindNestedArray("lidar_data_format", "pixel_shift_by_row");
    }
    if (shiftPos == std::string::npos) {
        shiftPos = jsonStr.find("\"pixel_shift_by_row\"");
    }
    
    if (shiftPos != std::string::npos) {
        size_t arrayStart = jsonStr.find('[', shiftPos);
        if (arrayStart != std::string::npos) {
            size_t arrayEnd = jsonStr.find(']', arrayStart);
            if (arrayEnd != std::string::npos) {
                std::string arrayStr = jsonStr.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
                m_pixelShiftByRow.clear();
                
                size_t pos = 0;
                while (pos < arrayStr.length()) {
                    while (pos < arrayStr.length() && (arrayStr[pos] == ' ' || arrayStr[pos] == '\n' || arrayStr[pos] == ',' || arrayStr[pos] == '\t')) {
                        pos++;
                    }
                    if (pos >= arrayStr.length()) break;
                    
                    bool negative = false;
                    if (arrayStr[pos] == '-') {
                        negative = true;
                        pos++;
                    }
                    
                    size_t end = pos;
                    while (end < arrayStr.length() && std::isdigit(arrayStr[end])) {
                        end++;
                    }
                    
                    if (end > pos) {
                        std::string numStr = arrayStr.substr(pos, end - pos);
                        try {
                            int shift = std::stoi(numStr);
                            if (negative) shift = -shift;
                            m_pixelShiftByRow.push_back(shift);
                        } catch (...) {
                        }
                    }
                    pos = end;
                }
                
                LOG_INFO_FMT("OusterStream", "Parsed pixel_shift_by_row: %zu values", m_pixelShiftByRow.size());
                if (m_pixelShiftByRow.size() > 0 && m_pixelShiftByRow.size() <= 10) {
                    std::string shiftPreview = "First few: ";
                    for (size_t i = 0; i < std::min(m_pixelShiftByRow.size(), size_t(5)); i++) {
                        shiftPreview += std::to_string(m_pixelShiftByRow[i]) + " ";
                    }
                    LOG_INFO_FMT("OusterStream", "  %s", shiftPreview.c_str());
                }
            }
        }
    } else {
        LOG_WARNING("OusterStream", "pixel_shift_by_row not found in metadata - azimuth calculation may be incorrect");
    }
    
    if (m_beamAltitudeAngles.size() != static_cast<size_t>(m_beamCount) || 
        m_beamAzimuthAngles.size() != static_cast<size_t>(m_beamCount)) {
        LOG_WARNING_FMT("OusterStream", "Beam angle count mismatch: altitude=%zu, azimuth=%zu, beamCount=%d. Using defaults.", 
                       m_beamAltitudeAngles.size(), m_beamAzimuthAngles.size(), m_beamCount);
        m_beamAltitudeAngles.clear();
        m_beamAzimuthAngles.clear();
        return false;
    }
    
    if (!m_pixelShiftByRow.empty() && m_pixelShiftByRow.size() != static_cast<size_t>(m_beamCount)) {
        LOG_WARNING_FMT("OusterStream", "pixel_shift_by_row count mismatch: %zu vs beamCount=%d", 
                       m_pixelShiftByRow.size(), m_beamCount);
        m_pixelShiftByRow.clear();
    }
    
    size_t originPos = jsonStr.find("\"lidar_origin_to_beam_origin_mm\"");
    if (originPos != std::string::npos) {
        size_t numStart = jsonStr.find_first_of("0123456789.", originPos);
        if (numStart != std::string::npos) {
            size_t numEnd = numStart;
            while (numEnd < jsonStr.length() && (std::isdigit(jsonStr[numEnd]) || jsonStr[numEnd] == '.' || jsonStr[numEnd] == 'e' || jsonStr[numEnd] == 'E' || jsonStr[numEnd] == '+' || jsonStr[numEnd] == '-')) {
                numEnd++;
            }
            try {
                m_lidarOriginToBeamOriginMm = std::stof(jsonStr.substr(numStart, numEnd - numStart));
                LOG_INFO_FMT("OusterStream", "Parsed lidar_origin_to_beam_origin_mm: %.3f", m_lidarOriginToBeamOriginMm);
            } catch (...) {
                LOG_WARNING("OusterStream", "Failed to parse lidar_origin_to_beam_origin_mm");
            }
        }
    }
    
    size_t beamTransformPos = jsonStr.find("\"beam_to_lidar_transform\"");
    if (beamTransformPos != std::string::npos) {
        size_t arrayStart = jsonStr.find('[', beamTransformPos);
        if (arrayStart != std::string::npos) {
            size_t arrayEnd = jsonStr.find(']', arrayStart);
            if (arrayEnd != std::string::npos) {
                std::string arrayStr = jsonStr.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
                std::vector<float> values;
                
                size_t pos = 0;
                while (pos < arrayStr.length() && values.size() < 16) {
                    while (pos < arrayStr.length() && (arrayStr[pos] == ' ' || arrayStr[pos] == '\n' || arrayStr[pos] == ',' || arrayStr[pos] == '\t')) {
                        pos++;
                    }
                    if (pos >= arrayStr.length()) break;
                    
                    size_t end = pos;
                    while (end < arrayStr.length() && (std::isdigit(arrayStr[end]) || arrayStr[end] == '.' || arrayStr[end] == '-' || arrayStr[end] == '+' || arrayStr[end] == 'e' || arrayStr[end] == 'E')) {
                        end++;
                    }
                    
                    if (end > pos) {
                        try {
                            values.push_back(std::stof(arrayStr.substr(pos, end - pos)));
                        } catch (...) {}
                    }
                    pos = end;
                }
                
                if (values.size() == 16) {
                    std::memcpy(m_beamToLidarTransform, values.data(), 16 * sizeof(float));
                    LOG_INFO("OusterStream", "Parsed beam_to_lidar_transform");
                }
            }
        }
    }
    
    size_t lidarTransformPos = jsonStr.find("\"lidar_to_sensor_transform\"");
    if (lidarTransformPos != std::string::npos) {
        size_t arrayStart = jsonStr.find('[', lidarTransformPos);
        if (arrayStart != std::string::npos) {
            size_t arrayEnd = jsonStr.find(']', arrayStart);
            if (arrayEnd != std::string::npos) {
                std::string arrayStr = jsonStr.substr(arrayStart + 1, arrayEnd - arrayStart - 1);
                std::vector<float> values;
                
                size_t pos = 0;
                while (pos < arrayStr.length() && values.size() < 16) {
                    while (pos < arrayStr.length() && (arrayStr[pos] == ' ' || arrayStr[pos] == '\n' || arrayStr[pos] == ',' || arrayStr[pos] == '\t')) {
                        pos++;
                    }
                    if (pos >= arrayStr.length()) break;
                    
                    size_t end = pos;
                    while (end < arrayStr.length() && (std::isdigit(arrayStr[end]) || arrayStr[end] == '.' || arrayStr[end] == '-' || arrayStr[end] == '+' || arrayStr[end] == 'e' || arrayStr[end] == 'E')) {
                        end++;
                    }
                    
                    if (end > pos) {
                        try {
                            values.push_back(std::stof(arrayStr.substr(pos, end - pos)));
                        } catch (...) {}
                    }
                    pos = end;
                }
                
                if (values.size() == 16) {
                    std::memcpy(m_lidarToSensorTransform, values.data(), 16 * sizeof(float));
                    LOG_INFO("OusterStream", "Parsed lidar_to_sensor_transform");
                    LOG_INFO_FMT("OusterStream", "  Transform: [%.1f, %.1f, %.1f, %.1f; %.1f, %.1f, %.1f, %.1f; %.1f, %.1f, %.1f, %.1f; %.1f, %.1f, %.1f, %.1f]",
                                m_lidarToSensorTransform[0], m_lidarToSensorTransform[1], m_lidarToSensorTransform[2], m_lidarToSensorTransform[3],
                                m_lidarToSensorTransform[4], m_lidarToSensorTransform[5], m_lidarToSensorTransform[6], m_lidarToSensorTransform[7],
                                m_lidarToSensorTransform[8], m_lidarToSensorTransform[9], m_lidarToSensorTransform[10], m_lidarToSensorTransform[11],
                                m_lidarToSensorTransform[12], m_lidarToSensorTransform[13], m_lidarToSensorTransform[14], m_lidarToSensorTransform[15]);
                }
            }
        }
    }
    
    return !m_beamAltitudeAngles.empty() && !m_beamAzimuthAngles.empty();
}

void OusterStream::ProcessLidarPacketFromFile(const uint8_t* data, size_t size) {
    // Delegate to manual parsing implementation
    ProcessLidarPacketFromFileManual(this, data, size);
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
