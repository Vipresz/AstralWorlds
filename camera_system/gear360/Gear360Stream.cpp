#include "Gear360Stream.h"
#include "../../engine/platform/Logger.h"
#include <cmath>
#include <chrono>
#include <iostream>
#include <cstring>
#include <sstream>
#include <iomanip>

using EngineLogging::Logger;

#ifdef __APPLE__
#include <CoreBluetooth/CoreBluetooth.h>
#include <Foundation/Foundation.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/select.h>
#include <cerrno>
#include <cstring>
#endif

// Forward declaration of C++ class
namespace Engine {
namespace Editors {
namespace CameraSystem {
    class Gear360Stream;
}
}
}

#ifdef __APPLE__
// Objective-C++ bridge class for Core Bluetooth
// Must be at global scope (not inside C++ namespace)
@interface Gear360BluetoothDelegate : NSObject <CBCentralManagerDelegate, CBPeripheralDelegate>
{
    @public
    Engine::Editors::CameraSystem::Gear360Stream* stream;
    CBCentralManager* centralManager;
    CBPeripheral* connectedPeripheral;
    CBCharacteristic* videoCharacteristic;
    CBCharacteristic* controlCharacteristic;
    bool isConnected;
}
- (instancetype)initWithStream:(Engine::Editors::CameraSystem::Gear360Stream*)s;
- (void)startScanning;
- (void)stopScanning;
- (void)connectToPeripheral:(CBPeripheral*)peripheral;
- (void)disconnect;
- (bool)sendCommand:(const std::vector<uint8_t>&)command;
@end
#endif

#ifdef __APPLE__
@implementation Gear360BluetoothDelegate

- (instancetype)initWithStream:(Engine::Editors::CameraSystem::Gear360Stream*)s {
    self = [super init];
    if (self) {
        stream = s;
        centralManager = [[CBCentralManager alloc] initWithDelegate:self queue:nil];
        connectedPeripheral = nil;
        videoCharacteristic = nil;
        controlCharacteristic = nil;
        isConnected = false;
    }
    return self;
}

- (void)centralManagerDidUpdateState:(CBCentralManager *)central {
    if (central.state == CBManagerStatePoweredOn) {
        LOG_INFO("Gear360Stream", "Bluetooth is powered on, starting scan...");
        [self startScanning];
    } else {
        LOG_WARNING_FMT("Gear360Stream", "Bluetooth state: %ld", (long)central.state);
    }
}

- (void)centralManager:(CBCentralManager *)central 
 didDiscoverPeripheral:(CBPeripheral *)peripheral 
     advertisementData:(NSDictionary<NSString *,id> *)advertisementData 
                  RSSI:(NSNumber *)RSSI {
    NSString* name = peripheral.name;
    if (name && ([name containsString:@"Gear 360"] || [name containsString:@"GEAR 360"])) {
        LOG_INFO_FMT("Gear360Stream", "Found Gear 360 device: %s", [name UTF8String]);
        [centralManager stopScan];
        [self connectToPeripheral:peripheral];
    }
}

- (void)centralManager:(CBCentralManager *)central 
  didConnectPeripheral:(CBPeripheral *)peripheral {
    LOG_INFO("Gear360Stream", "Connected to Gear 360");
    connectedPeripheral = peripheral;
    connectedPeripheral.delegate = self;
    [connectedPeripheral discoverServices:nil];
    isConnected = true;
}

- (void)centralManager:(CBCentralManager *)central 
didDisconnectPeripheral:(CBPeripheral *)peripheral 
                 error:(NSError *)error {
    LOG_WARNING("Gear360Stream", "Disconnected from Gear 360");
    connectedPeripheral = nil;
    videoCharacteristic = nil;
    controlCharacteristic = nil;
    isConnected = false;
}

- (void)peripheral:(CBPeripheral *)peripheral 
didDiscoverServices:(NSError *)error {
    if (error) {
        LOG_ERROR_FMT("Gear360Stream", "Service discovery error: %s", [error.localizedDescription UTF8String]);
        return;
    }
    
    for (CBService* service in peripheral.services) {
        LOG_INFO_FMT("Gear360Stream", "Found service: %s", [service.UUID.UUIDString UTF8String]);
        [peripheral discoverCharacteristics:nil forService:service];
    }
}

- (void)peripheral:(CBPeripheral *)peripheral 
didDiscoverCharacteristicsForService:(CBService *)service 
             error:(NSError *)error {
    if (error) {
        LOG_ERROR_FMT("Gear360Stream", "Characteristic discovery error: %s", [error.localizedDescription UTF8String]);
        return;
    }
    
    for (CBCharacteristic* characteristic in service.characteristics) {
        LOG_INFO_FMT("Gear360Stream", "Found characteristic: %s", [characteristic.UUID.UUIDString UTF8String]);
        
        // Look for video/control characteristics
        // Note: Actual UUIDs need to be determined from Gear 360 documentation or reverse engineering
        if ([characteristic.UUID.UUIDString containsString:@"video"] || 
            [characteristic.UUID.UUIDString containsString:@"stream"]) {
            videoCharacteristic = characteristic;
            [peripheral setNotifyValue:YES forCharacteristic:characteristic];
        }
        if ([characteristic.UUID.UUIDString containsString:@"control"] || 
            [characteristic.UUID.UUIDString containsString:@"command"]) {
            controlCharacteristic = characteristic;
        }
    }
}

- (void)peripheral:(CBPeripheral *)peripheral 
didUpdateValueForCharacteristic:(CBCharacteristic *)characteristic 
             error:(NSError *)error {
    if (error) {
        LOG_ERROR_FMT("Gear360Stream", "Characteristic update error: %s", [error.localizedDescription UTF8String]);
        return;
    }
    
    if (characteristic == videoCharacteristic && characteristic.value) {
        NSData* data = characteristic.value;
        const uint8_t* bytes = (const uint8_t*)data.bytes;
        size_t length = data.length;
        
        // Process video data
        if (stream) {
            stream->ProcessVideoData(bytes, length);
        }
    }
}

- (void)startScanning {
    NSArray* services = nil;  // Scan for all services
    [centralManager scanForPeripheralsWithServices:services options:nil];
}

- (void)stopScanning {
    [centralManager stopScan];
}

- (void)connectToPeripheral:(CBPeripheral*)peripheral {
    [centralManager connectPeripheral:peripheral options:nil];
}

- (void)disconnect {
    if (connectedPeripheral) {
        [centralManager cancelPeripheralConnection:connectedPeripheral];
    }
    isConnected = false;
}

- (bool)sendCommand:(const std::vector<uint8_t>&)command {
    if (!connectedPeripheral || !controlCharacteristic) {
        return false;
    }
    
    NSData* data = [NSData dataWithBytes:command.data() length:command.size()];
    [connectedPeripheral writeValue:data 
                  forCharacteristic:controlCharacteristic 
                               type:CBCharacteristicWriteWithResponse];
    return true;
}

@end
#endif

namespace Engine {
namespace Editors {
namespace CameraSystem {

Gear360Stream::Gear360Stream() {
    m_lastFPSUpdate = std::chrono::steady_clock::now();
}

Gear360Stream::~Gear360Stream() {
    StopStreaming();
    Disconnect();
    CleanupBluetooth();
}

void Gear360Stream::SetConfig(const Gear360Config& config) {
    m_config = config;
}

bool Gear360Stream::InitializeBluetooth() {
#ifdef __APPLE__
    if (m_bluetoothHandle) {
        return true;  // Already initialized
    }
    
    Gear360BluetoothDelegate* delegate = [[Gear360BluetoothDelegate alloc] initWithStream:this];
    m_bluetoothHandle = (void*)CFBridgingRetain(delegate);
    
    LOG_INFO("Gear360Stream", "Bluetooth initialized");
    return true;
#else
    LOG_WARNING("Gear360Stream", "Bluetooth not supported on this platform");
    return false;
#endif
}

void Gear360Stream::CleanupBluetooth() {
#ifdef __APPLE__
    if (m_bluetoothHandle) {
        Gear360BluetoothDelegate* delegate = (Gear360BluetoothDelegate*)CFBridgingRelease(m_bluetoothHandle);
        [delegate disconnect];
        delegate = nil;
        m_bluetoothHandle = nullptr;
    }
#endif
}

bool Gear360Stream::Connect() {
    if (m_connected) {
        return true;
    }
    
    LOG_INFO("Gear360Stream", "Connecting to Gear 360...");
    
    if (m_config.useWiFi) {
        if (!InitializeWiFi()) {
            LOG_ERROR("Gear360Stream", "Failed to initialize WiFi");
            return false;
        }
        if (!ConnectToWiFi()) {
            LOG_ERROR("Gear360Stream", "Failed to connect to WiFi");
            return false;
        }
    } else {
        if (!InitializeBluetooth()) {
            LOG_ERROR("Gear360Stream", "Failed to initialize Bluetooth");
            return false;
        }
        if (!ConnectToDevice()) {
            LOG_ERROR("Gear360Stream", "Failed to connect to device");
            return false;
        }
    }
    
    m_connected = true;
    LOG_INFO("Gear360Stream", "Connected to Gear 360");
    
    if (m_connectionCallback) {
        m_connectionCallback(true);
    }
    
    return true;
}

bool Gear360Stream::ConnectToDevice() {
#ifdef __APPLE__
    if (!m_bluetoothHandle) {
        return false;
    }
    
    Gear360BluetoothDelegate* delegate = (__bridge Gear360BluetoothDelegate*)m_bluetoothHandle;
    
    // Wait for connection (with timeout)
    auto start = std::chrono::steady_clock::now();
    while (!delegate->isConnected) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > 10) {
            LOG_ERROR("Gear360Stream", "Connection timeout");
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return true;
#else
    return false;
#endif
}

void Gear360Stream::Disconnect() {
    if (!m_connected) {
        return;
    }
    
    StopStreaming();
    
#ifdef __APPLE__
    if (m_bluetoothHandle) {
        Gear360BluetoothDelegate* delegate = (Gear360BluetoothDelegate*)m_bluetoothHandle;
        [delegate disconnect];
    }
#endif
    
    m_connected = false;
    LOG_INFO("Gear360Stream", "Disconnected from Gear 360");
    
    if (m_connectionCallback) {
        m_connectionCallback(false);
    }
}

bool Gear360Stream::DiscoverDevices(std::vector<std::string>& deviceNames) {
    // Implementation would scan for Bluetooth devices
    // For now, return empty list
    deviceNames.clear();
    return true;
}

void Gear360Stream::StartStreaming() {
    if (m_streaming) {
        return;
    }
    
    if (!m_connected) {
        LOG_WARNING("Gear360Stream", "Cannot start streaming: not connected");
        return;
    }
    
    LOG_INFO("Gear360Stream", "Starting video stream...");
    
    m_shouldStop = false;
    m_streaming = true;
    m_streamThread = std::make_unique<std::thread>(&Gear360Stream::StreamingThread, this);
}

void Gear360Stream::StopStreaming() {
    if (!m_streaming) {
        return;
    }
    
    LOG_INFO("Gear360Stream", "Stopping video stream...");
    
    m_shouldStop = true;
    if (m_streamThread && m_streamThread->joinable()) {
        m_streamThread->join();
    }
    m_streamThread.reset();
    m_streaming = false;
}

void Gear360Stream::StreamingThread() {
    LOG_INFO("Gear360Stream", "Streaming thread started");
    
    if (m_config.useWiFi) {
        // WiFi streaming implementation
        while (!m_shouldStop && m_connected) {
            if (ReceiveVideoOverWiFi()) {
                // Video data received and processed
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        }
    } else {
        // Bluetooth streaming - data comes via callbacks
        // This thread mainly handles statistics and queue management
        while (!m_shouldStop && m_connected) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // Update FPS
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - m_lastFPSUpdate).count();
            
            if (elapsed >= 1000) {
                double fps = m_fpsFrameCount * 1000.0 / elapsed;
                m_currentFPS = fps;
                m_fpsFrameCount = 0;
                m_lastFPSUpdate = now;
            }
        }
    }
    
    LOG_INFO("Gear360Stream", "Streaming thread stopped");
    m_streaming = false;
}

void Gear360Stream::ProcessVideoData(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        return;
    }
    
    // Create video frame
    VideoFrame frame;
    frame.data.resize(size);
    std::memcpy(frame.data.data(), data, size);
    frame.width = m_config.videoWidth;
    frame.height = m_config.videoHeight;
    auto now = std::chrono::steady_clock::now();
    frame.timestamp = std::chrono::duration<double>(
        now.time_since_epoch()).count();
    frame.is360 = true;
    frame.format = 0;  // Assume JPEG for now
    
    m_lastTimestamp = frame.timestamp;
    m_frameCount++;
    m_fpsFrameCount++;
    
    // Call callback
    if (m_videoFrameCallback) {
        m_videoFrameCallback(frame);
    }
    
    // Add to queue
    {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        if (m_frameQueue.size() >= MAX_QUEUE_SIZE) {
            m_frameQueue.pop();
            m_droppedFrames++;
        }
        m_frameQueue.push(frame);
    }
}

bool Gear360Stream::StartRecording() {
    if (!m_connected) {
        return false;
    }
    
    // Send start recording command via Bluetooth
    // Note: Actual command format needs to be determined from Gear 360 protocol
    std::vector<uint8_t> command = {0x01, 0x01};  // Placeholder
    
    return SendCommand(command);
}

bool Gear360Stream::StopRecording() {
    if (!m_connected) {
        return false;
    }
    
    std::vector<uint8_t> command = {0x01, 0x00};  // Placeholder
    
    return SendCommand(command);
}

bool Gear360Stream::TakePhoto() {
    if (!m_connected) {
        return false;
    }
    
    std::vector<uint8_t> command = {0x02, 0x01};  // Placeholder
    
    return SendCommand(command);
}

bool Gear360Stream::SetVideoMode(bool is360) {
    if (!m_connected) {
        return false;
    }
    
    std::vector<uint8_t> command = {0x03, static_cast<uint8_t>(is360 ? 0x01 : 0x00)};  // Placeholder
    
    return SendCommand(command);
}

bool Gear360Stream::SendCommand(const std::vector<uint8_t>& command) {
#ifdef __APPLE__
    if (!m_bluetoothHandle) {
        return false;
    }
    
    Gear360BluetoothDelegate* delegate = (__bridge Gear360BluetoothDelegate*)m_bluetoothHandle;
    return [delegate sendCommand:command];
#else
    return false;
#endif
}

bool Gear360Stream::PollVideoFrame(VideoFrame& outFrame) {
    std::lock_guard<std::mutex> lock(m_frameMutex);
    if (m_frameQueue.empty()) {
        return false;
    }
    
    outFrame = m_frameQueue.front();
    m_frameQueue.pop();
    return true;
}

bool Gear360Stream::WaitForVideoFrame(VideoFrame& outFrame, int timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (true) {
        if (PollVideoFrame(outFrame)) {
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

std::string Gear360Stream::GetDeviceInfo() const {
    return "Samsung Gear 360 (2017) - " + m_deviceName + 
           " (" + m_deviceAddress + ")";
}

bool Gear360Stream::InitializeWiFi() {
    if (m_wifiHandle) {
        return true;  // Already initialized
    }
    
    LOG_INFO("Gear360Stream", "WiFi initialized (will connect to camera network)");
    return true;
}

bool Gear360Stream::ConnectToWiFi() {
    // Note: The user needs to manually connect to the Gear 360's WiFi network first
    // This function just verifies we can reach the camera's IP
    // Typical Gear 360 IP: 192.168.1.1 or 192.168.43.1
    
    LOG_INFO("Gear360Stream", "WiFi connection - ensure you're connected to Gear 360's WiFi network");
    return true;
}

// Helper function to base64 encode (for HTTP Basic Auth)
static std::string Base64Encode(const std::string& input) {
    const char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    int val = 0, valb = -6;
    for (unsigned char c : input) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    while (encoded.size() % 4) encoded.push_back('=');
    return encoded;
}

bool Gear360Stream::ReceiveVideoOverWiFi() {
#ifdef __APPLE__
    // Create socket if needed
    static int sock = -1;
    static bool connectionAttempted = false;
    static bool authenticated = false;
    
    if (sock < 0 && !connectionAttempted) {
        connectionAttempted = true;
        const std::string& cameraIP = m_config.cameraIP;
        const std::string& streamPath = m_config.streamPath;
        
        LOG_INFO_FMT("Gear360Stream", "Attempting to connect to camera at %s:80%s", cameraIP.c_str(), streamPath.c_str());
        
        // Create TCP socket for HTTP streaming
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0) {
            LOG_ERROR_FMT("Gear360Stream", "Failed to create socket (errno: %d)", errno);
            connectionAttempted = false;
            return false;
        }
        
        // Set socket to blocking for initial connection
        int flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags & ~O_NONBLOCK);
        
        // Connect to camera
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(80);  // HTTP port
        if (inet_pton(AF_INET, cameraIP.c_str(), &addr.sin_addr) <= 0) {
            LOG_ERROR("Gear360Stream", "Invalid camera IP address");
            close(sock);
            sock = -1;
            connectionAttempted = false;
            return false;
        }
        
        // Try to connect
        int result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        if (result < 0) {
            LOG_ERROR_FMT("Gear360Stream", "Failed to connect to %s:80 (errno: %d - %s). Check if camera IP is correct.", 
                         cameraIP.c_str(), errno, strerror(errno));
            close(sock);
            sock = -1;
            connectionAttempted = false;
            return false;
        }
        
        LOG_INFO_FMT("Gear360Stream", "Connected to camera at %s", cameraIP.c_str());
        
        // Set socket back to non-blocking for data reception
        flags = fcntl(sock, F_GETFL, 0);
        fcntl(sock, F_SETFL, flags | O_NONBLOCK);
        
        // Build HTTP request with authentication if provided
        std::string authHeader;
        if (!m_config.httpUsername.empty() && !m_config.httpPassword.empty()) {
            std::string credentials = m_config.httpUsername + ":" + m_config.httpPassword;
            std::string encoded = Base64Encode(credentials);
            authHeader = "Authorization: Basic " + encoded + "\r\n";
            LOG_INFO("Gear360Stream", "Using HTTP Basic Authentication");
        }
        
        // Send HTTP request for MJPEG stream
        std::string request = 
            "GET " + streamPath + " HTTP/1.1\r\n"
            "Host: " + cameraIP + "\r\n"
            "Connection: keep-alive\r\n"
            "Accept: multipart/x-mixed-replace;boundary=--myboundary\r\n"
            "User-Agent: Gear360Stream/1.0\r\n"
            + authHeader +
            "\r\n";
        
        ssize_t sent = send(sock, request.c_str(), request.length(), 0);
        if (sent < 0) {
            LOG_ERROR_FMT("Gear360Stream", "Failed to send HTTP request (errno: %d)", errno);
            close(sock);
            sock = -1;
            connectionAttempted = false;
            return false;
        }
        
        LOG_INFO_FMT("Gear360Stream", "Sent HTTP request (%zd bytes), waiting for response...", sent);
    }
    
    if (sock < 0) {
        return false;
    }
    
    // Receive data
    std::vector<uint8_t> buffer(65536);
    ssize_t received = recv(sock, buffer.data(), buffer.size(), 0);
    
    if (received > 0) {
        static bool firstData = true;
        if (firstData) {
            firstData = false;
            // Log first few bytes to see what we're getting
            std::string preview;
            size_t previewLen = std::min(static_cast<size_t>(received), size_t(200));
            for (size_t i = 0; i < previewLen; i++) {
                if (buffer[i] >= 32 && buffer[i] < 127) {
                    preview += static_cast<char>(buffer[i]);
                } else {
                    preview += ".";
                }
            }
            LOG_INFO_FMT("Gear360Stream", "Received %zd bytes. First bytes: %s", received, preview.c_str());
            
            // Check for 401 Unauthorized
            std::string response(reinterpret_cast<const char*>(buffer.data()), 
                               std::min(static_cast<size_t>(received), size_t(200)));
            if (response.find("401") != std::string::npos || response.find("Unauthorized") != std::string::npos) {
                LOG_ERROR("Gear360Stream", "Received 401 Unauthorized. Camera requires authentication.");
                LOG_ERROR("Gear360Stream", "Please set httpUsername and httpPassword in Gear360Config");
                LOG_ERROR("Gear360Stream", "Note: Response shows 'NETGEAR R7000' - verify you're connected to the Gear 360, not a router");
                // Close connection
                close(sock);
                sock = -1;
                connectionAttempted = false;
                authenticated = false;
                // Don't retry immediately - wait a bit
                std::this_thread::sleep_for(std::chrono::seconds(5));
                return false;
            }
            
            // Check for successful response
            if (response.find("200 OK") != std::string::npos) {
                LOG_INFO("Gear360Stream", "HTTP 200 OK - stream starting");
            }
        }
        
        // Parse MJPEG stream
        // MJPEG format: --boundary\r\nContent-Type: image/jpeg\r\nContent-Length: <size>\r\n\r\n<JPEG data>
        static std::vector<uint8_t> frameBuffer;
        static bool inFrame = false;
        static size_t contentLength = 0;
        static int frameCount = 0;
        
        size_t offset = 0;
        while (offset < static_cast<size_t>(received)) {
            if (!inFrame) {
                // Look for boundary - try different common formats
                const char* data = reinterpret_cast<const char*>(buffer.data() + offset);
                const char* found = nullptr;
                
                // Try different boundary formats
                const char* boundaries[] = {
                    "--myboundary",
                    "Content-Type: image/jpeg",
                    "\xff\xd8",  // JPEG start marker
                    "HTTP/1.1 200"
                };
                
                for (int i = 0; i < 4; i++) {
                    found = strstr(data, boundaries[i]);
                    if (found) break;
                }
                
                if (found) {
                    // Found something, look for Content-Length header
                    const char* contentLenStr = "Content-Length: ";
                    const char* lenStart = strstr(found, contentLenStr);
                    if (lenStart) {
                        lenStart += strlen(contentLenStr);
                        contentLength = atoi(lenStart);
                        
                        // Find end of headers (double CRLF)
                        const char* headerEndPtr = strstr(lenStart, "\r\n\r\n");
                        if (headerEndPtr) {
                            size_t headerEnd = (headerEndPtr - data) + 4;  // +4 for \r\n\r\n
                            inFrame = true;
                            frameBuffer.clear();
                            frameBuffer.reserve(contentLength);
                            offset = headerEnd;
                            LOG_INFO_FMT("Gear360Stream", "Found frame header, Content-Length: %zu", contentLength);
                        }
                    } else {
                        // No Content-Length, might be JPEG directly
                        // Look for JPEG start marker
                        const uint8_t* jpegStart = reinterpret_cast<const uint8_t*>(found);
                        if (jpegStart[0] == 0xFF && jpegStart[1] == 0xD8) {
                            // Found JPEG start, read until JPEG end
                            inFrame = true;
                            frameBuffer.clear();
                            offset = found - data;
                            contentLength = 0;  // Will read until JPEG end marker
                            LOG_INFO("Gear360Stream", "Found JPEG start marker");
                        }
                    }
                }
            }
            
            if (inFrame) {
                if (contentLength > 0) {
                    // Collect frame data with known length
                    size_t remaining = received - offset;
                    size_t needed = contentLength - frameBuffer.size();
                    size_t toCopy = std::min(remaining, needed);
                    
                    frameBuffer.insert(frameBuffer.end(), 
                                     buffer.data() + offset, 
                                     buffer.data() + offset + toCopy);
                    offset += toCopy;
                    
                    if (frameBuffer.size() >= contentLength) {
                        // Complete frame received
                        frameCount++;
                        if (frameCount <= 5 || frameCount % 30 == 0) {
                            LOG_INFO_FMT("Gear360Stream", "Received frame %d, size: %zu bytes", frameCount, frameBuffer.size());
                        }
                        ProcessVideoData(frameBuffer.data(), frameBuffer.size());
                        inFrame = false;
                        frameBuffer.clear();
                    }
                } else {
                    // Reading JPEG until end marker
                    size_t remaining = received - offset;
                    frameBuffer.insert(frameBuffer.end(), 
                                     buffer.data() + offset, 
                                     buffer.data() + offset + remaining);
                    offset = received;
                    
                    // Check for JPEG end marker (0xFF 0xD9)
                    if (frameBuffer.size() >= 2) {
                        for (size_t i = frameBuffer.size() - 2; i < frameBuffer.size(); i++) {
                            if (i + 1 < frameBuffer.size() && 
                                frameBuffer[i] == 0xFF && frameBuffer[i+1] == 0xD9) {
                                // Found JPEG end
                                frameCount++;
                                if (frameCount <= 5 || frameCount % 30 == 0) {
                                    LOG_INFO_FMT("Gear360Stream", "Received JPEG frame %d, size: %zu bytes", frameCount, frameBuffer.size());
                                }
                                ProcessVideoData(frameBuffer.data(), frameBuffer.size());
                                inFrame = false;
                                frameBuffer.clear();
                                break;
                            }
                        }
                    }
                }
            } else {
                offset++;
                if (offset >= static_cast<size_t>(received)) break;
            }
        }
        
        return true;
    } else if (received == 0) {
        // Connection closed
        LOG_WARNING("Gear360Stream", "Connection closed by camera");
        close(sock);
        sock = -1;
        return false;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
        // Error
        LOG_ERROR_FMT("Gear360Stream", "Receive error: %s", strerror(errno));
        close(sock);
        sock = -1;
        return false;
    }
    
    return false;
#else
    LOG_WARNING("Gear360Stream", "WiFi streaming not implemented on this platform");
    return false;
#endif
}

std::unique_ptr<Gear360Stream> CreateGear360Stream(const std::string& deviceName) {
    auto stream = std::make_unique<Gear360Stream>();
    Gear360Config config;
    config.deviceName = deviceName;
    stream->SetConfig(config);
    return stream;
}

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine
