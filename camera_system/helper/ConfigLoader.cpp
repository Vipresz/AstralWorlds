#include "ConfigLoader.h"
#include "../../engine/platform/Logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>

namespace Engine {
namespace Editors {
namespace CameraSystem {

// Simple JSON parser for our specific configuration structure
// This is a basic implementation - for production, consider using a proper JSON library

static void SkipWhitespace(const std::string& str, size_t& pos) {
    while (pos < str.length() && std::isspace(str[pos])) {
        pos++;
    }
}

static std::string ExtractString(const std::string& str, size_t& pos) {
    SkipWhitespace(str, pos);
    if (pos >= str.length() || str[pos] != '"') {
        return "";
    }
    pos++; // Skip opening quote
    
    std::string result;
    while (pos < str.length() && str[pos] != '"') {
        if (str[pos] == '\\' && pos + 1 < str.length()) {
            pos++;
            switch (str[pos]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                default: result += str[pos]; break;
            }
        } else {
            result += str[pos];
        }
        pos++;
    }
    
    if (pos < str.length() && str[pos] == '"') {
        pos++; // Skip closing quote
    }
    
    return result;
}

static float ExtractNumber(const std::string& str, size_t& pos) {
    try {
        if (pos >= str.length()) {
            return 0.0f;
        }
        SkipWhitespace(str, pos);
        if (pos >= str.length()) {
            return 0.0f;
        }
        std::string numStr;
        bool hasDecimal = false;
        bool hasExponent = false;
        
        // Handle sign
        if (pos < str.length() && (str[pos] == '-' || str[pos] == '+')) {
            numStr += str[pos++];
        }
        
        // Parse integer part
        while (pos < str.length() && std::isdigit(str[pos])) {
            numStr += str[pos++];
        }
        
        // Parse decimal point and fractional part
        if (pos < str.length() && str[pos] == '.') {
            numStr += str[pos++];
            hasDecimal = true;
            while (pos < str.length() && std::isdigit(str[pos])) {
                numStr += str[pos++];
            }
        }
        
        // Parse exponent (e or E, optional +/-, digits)
        if (pos < str.length() && (str[pos] == 'e' || str[pos] == 'E')) {
            numStr += str[pos++];
            hasExponent = true;
            
            // Optional sign for exponent
            if (pos < str.length() && (str[pos] == '+' || str[pos] == '-')) {
                numStr += str[pos++];
            }
            
            // Exponent digits
            while (pos < str.length() && std::isdigit(str[pos])) {
                numStr += str[pos++];
            }
        }
        
        if (numStr.empty()) {
            return 0.0f;
        }
        
        try {
            return std::stof(numStr);
        } catch (const std::exception&) {
            return 0.0f;
        } catch (...) {
            return 0.0f;
        }
    } catch (...) {
        return 0.0f;
    }
}

static bool ExtractBoolean(const std::string& str, size_t& pos) {
    SkipWhitespace(str, pos);
    if (str.substr(pos, 4) == "true") {
        pos += 4;
        return true;
    } else if (str.substr(pos, 5) == "false") {
        pos += 5;
        return false;
    }
    return false;
}

static bool ExpectToken(const std::string& str, size_t& pos, char token) {
    SkipWhitespace(str, pos);
    if (pos < str.length() && str[pos] == token) {
        pos++;
        return true;
    }
    return false;
}

static std::vector<float> ExtractFloatArray(const std::string& str, size_t& pos) {
    std::vector<float> result;
    try {
        if (pos >= str.length()) {
            return result;
        }
        if (!ExpectToken(str, pos, '[')) return result;
        
        SkipWhitespace(str, pos);
        int elementCount = 0;
        while (pos < str.length() && str[pos] != ']') {
            elementCount++;
            // Increased limit to handle large arrays (e.g., 300 frames)
            if (elementCount > 1000) { // Safety limit - should be enough for any reasonable config
                LOG_WARNING_FMT("ConfigLoader", "Float array has more than 1000 elements, stopping at element %d", elementCount);
                break;
            }
            try {
                float value = ExtractNumber(str, pos);
                result.push_back(value);
            } catch (...) {
                // Skip this element
                break;
            }
            
            SkipWhitespace(str, pos);
            if (pos < str.length() && str[pos] == ',') {
                pos++;
                SkipWhitespace(str, pos);
            }
        }
        
        ExpectToken(str, pos, ']');
    } catch (...) {
        // Return whatever we've parsed so far
    }
    return result;
}

static glm::vec3 ExtractVec3(const std::string& str, size_t& pos) {
    try {
        if (pos >= str.length()) {
            return glm::vec3(0.0f);
        }
        auto arr = ExtractFloatArray(str, pos);
        if (arr.size() >= 3) {
            // Check for NaN/Inf before creating vec3
            if (std::isnan(arr[0]) || std::isinf(arr[0]) ||
                std::isnan(arr[1]) || std::isinf(arr[1]) ||
                std::isnan(arr[2]) || std::isinf(arr[2])) {
                return glm::vec3(0.0f);
            }
            return glm::vec3(arr[0], arr[1], arr[2]);
        }
        return glm::vec3(0.0f);
    } catch (const std::exception&) {
        // Log to stderr since we can't use logger here
        return glm::vec3(0.0f);
    } catch (...) {
        return glm::vec3(0.0f);
    }
}

static bool ParsePathPoint(const std::string& str, size_t& pos, PathPoint& point) {
    if (!ExpectToken(str, pos, '{')) return false;
    
    while (pos < str.length() && str[pos] != '}') {
        SkipWhitespace(str, pos);
        std::string key = ExtractString(str, pos);
        
        if (key == "position") {
            ExpectToken(str, pos, ':');
            point.position = ExtractVec3(str, pos);
        } else if (key == "time") {
            ExpectToken(str, pos, ':');
            point.time = ExtractNumber(str, pos);
        }
        
        SkipWhitespace(str, pos);
        if (pos < str.length() && str[pos] == ',') {
            pos++;
        }
    }
    
    return ExpectToken(str, pos, '}');
}

static bool ParseObjectKeyframe(const std::string& str, size_t& pos, ObjectKeyframe& keyframe) {
    if (!ExpectToken(str, pos, '{')) return false;
    
    while (pos < str.length() && str[pos] != '}') {
        SkipWhitespace(str, pos);
        std::string key = ExtractString(str, pos);
        ExpectToken(str, pos, ':');
        
        if (key == "position") {
            keyframe.position = ExtractVec3(str, pos);
        } else if (key == "frame") {
            keyframe.frame = static_cast<int>(ExtractNumber(str, pos));
        }
        
        SkipWhitespace(str, pos);
        if (pos < str.length() && str[pos] == ',') {
            pos++;
        }
    }
    
    return ExpectToken(str, pos, '}');
}

static bool ParseObjectConfig(const std::string& str, size_t& pos, ObjectConfig& obj) {
    if (!ExpectToken(str, pos, '{')) return false;
    
    while (pos < str.length() && str[pos] != '}') {
        SkipWhitespace(str, pos);
        std::string key = ExtractString(str, pos);
        ExpectToken(str, pos, ':');
        
        if (key == "name") {
            obj.name = ExtractString(str, pos);
        } else if (key == "obj_file") {
            obj.objFile = ExtractString(str, pos);
        } else if (key == "animation_fbx") {
            SkipWhitespace(str, pos);
            if (pos < str.length() && str[pos] != 'n') { // not null
                obj.animationFbx = ExtractString(str, pos);
            } else if (str.substr(pos, 4) == "null") {
                pos += 4;
                obj.animationFbx = "";
            }
        } else if (key == "skel_file") {
            SkipWhitespace(str, pos);
            if (pos < str.length() && str[pos] != 'n') { // not null
                obj.skelFile = ExtractString(str, pos);
            } else if (str.substr(pos, 4) == "null") {
                pos += 4;
                obj.skelFile = "";
            }
        } else if (key == "weights_file") {
            SkipWhitespace(str, pos);
            if (pos < str.length() && str[pos] != 'n') { // not null
                obj.weightsFile = ExtractString(str, pos);
            } else if (str.substr(pos, 4) == "null") {
                pos += 4;
                obj.weightsFile = "";
            }
        } else if (key == "start_time") {
            obj.startTime = ExtractNumber(str, pos);
        } else if (key == "visible") {
            obj.visible = ExtractBoolean(str, pos);
        } else if (key == "scale") {
            obj.scale = ExtractVec3(str, pos);
        } else if (key == "rotation") {
            obj.rotation = ExtractVec3(str, pos);
        } else if (key == "path") {
            // Legacy: time-based path
            if (ExpectToken(str, pos, '[')) {
                while (pos < str.length() && str[pos] != ']') {
                    PathPoint point;
                    if (ParsePathPoint(str, pos, point)) {
                        obj.path.push_back(point);
                    }
                    SkipWhitespace(str, pos);
                    if (pos < str.length() && str[pos] == ',') {
                        pos++;
                    }
                }
                ExpectToken(str, pos, ']');
            }
        } else if (key == "keyframes") {
            // New: frame-based keyframes
            if (ExpectToken(str, pos, '[')) {
                while (pos < str.length() && str[pos] != ']') {
                    ObjectKeyframe keyframe;
                    if (ParseObjectKeyframe(str, pos, keyframe)) {
                        obj.keyframes.push_back(keyframe);
                    }
                    SkipWhitespace(str, pos);
                    if (pos < str.length() && str[pos] == ',') {
                        pos++;
                    }
                }
                ExpectToken(str, pos, ']');
            }
        }
        
        SkipWhitespace(str, pos);
        if (pos < str.length() && str[pos] == ',') {
            pos++;
        }
    }
    
    return ExpectToken(str, pos, '}');
}

static bool ParseCameraConfig(const std::string& str, size_t& pos, CameraConfig& camera) {
    if (!ExpectToken(str, pos, '{')) return false;
    
    while (pos < str.length() && str[pos] != '}') {
        SkipWhitespace(str, pos);
        std::string key = ExtractString(str, pos);
        ExpectToken(str, pos, ':');
        
        if (key == "position") {
            camera.position = ExtractVec3(str, pos);
        } else if (key == "target") {
            camera.target = ExtractVec3(str, pos);
        } else if (key == "distance") {
            camera.distance = ExtractNumber(str, pos);
        } else if (key == "yaw") {
            camera.yaw = ExtractNumber(str, pos);
        } else if (key == "pitch") {
            camera.pitch = ExtractNumber(str, pos);
        }
        
        SkipWhitespace(str, pos);
        if (pos < str.length() && str[pos] == ',') {
            pos++;
        }
    }
    
    return ExpectToken(str, pos, '}');
}

static bool ParseCameraKeyframe(const std::string& str, size_t& pos, CameraKeyframe& keyframe) {
    // Use same static file handle
    static std::ofstream* debugLogPtr = nullptr;
    static bool logInitialized = false;
    
    if (!logInitialized) {
        debugLogPtr = new std::ofstream("C:\\Users\\inque\\OneDrive\\Desktop\\Work\\games\\thegame\\build\\bin\\Release\\configloader_debug.log", std::ios::app);
        logInitialized = true;
    }
    
    if (debugLogPtr && debugLogPtr->is_open()) {
        *debugLogPtr << "[ParseCameraKeyframe] Starting at position " << pos << std::endl;
        debugLogPtr->flush();
    }
    
    try {
        if (debugLogPtr && debugLogPtr->is_open()) {
            *debugLogPtr << "[ParseCameraKeyframe] About to expect opening brace at position " << pos << std::endl;
            debugLogPtr->flush();
        }
        
        if (!ExpectToken(str, pos, '{')) {
            if (debugLogPtr && debugLogPtr->is_open()) {
                *debugLogPtr << "[ParseCameraKeyframe] Failed to find opening brace at position " << pos << ", char='" << (pos < str.length() ? str[pos] : '?') << "'" << std::endl;
                debugLogPtr->flush();
            }
            return false;
        }
        
        if (debugLogPtr && debugLogPtr->is_open()) {
            *debugLogPtr << "[ParseCameraKeyframe] Opening brace found, now at position " << pos << std::endl;
            debugLogPtr->flush();
        }
        
        int keyCount = 0;
        size_t lastPos = pos;
        int noProgressCount = 0;
        
        while (pos < str.length() && str[pos] != '}') {
            keyCount++;
            if (keyCount > 10) {
                if (debugLogPtr && debugLogPtr->is_open()) {
                    *debugLogPtr << "[ParseCameraKeyframe] Too many keys, breaking" << std::endl;
                    debugLogPtr->flush();
                }
                break;
            }
            
            // Check for infinite loop
            if (pos == lastPos) {
                noProgressCount++;
                if (noProgressCount > 5) {
                    if (debugLogPtr && debugLogPtr->is_open()) {
                        *debugLogPtr << "[ParseCameraKeyframe] Infinite loop detected, breaking" << std::endl;
                        debugLogPtr->flush();
                    }
                    break;
                }
            } else {
                noProgressCount = 0;
                lastPos = pos;
            }
            
            SkipWhitespace(str, pos);
            if (pos >= str.length()) {
                if (debugLogPtr && debugLogPtr->is_open()) {
                    *debugLogPtr << "[ParseCameraKeyframe] Reached end of string while skipping whitespace" << std::endl;
                    debugLogPtr->flush();
                }
                break;
            }
            
            if (debugLogPtr && debugLogPtr->is_open()) {
                *debugLogPtr << "[ParseCameraKeyframe] Extracting key " << keyCount << " at position " << pos << std::endl;
                debugLogPtr->flush();
            }
            
            std::string key = ExtractString(str, pos);
            
            if (debugLogPtr && debugLogPtr->is_open()) {
                *debugLogPtr << "[ParseCameraKeyframe] Extracted key: '" << key << "'" << std::endl;
                debugLogPtr->flush();
            }
            
            try {
                if (!ExpectToken(str, pos, ':')) {
                    if (debugLogPtr && debugLogPtr->is_open()) {
                        *debugLogPtr << "[ParseCameraKeyframe] Failed to find colon after key '" << key << "', skipping" << std::endl;
                        debugLogPtr->flush();
                    }
                    // Skip to next key or end of object
                    while (pos < str.length() && str[pos] != ',' && str[pos] != '}') {
                        pos++;
                    }
                    continue;
                }
            } catch (...) {
                if (debugLogPtr && debugLogPtr->is_open()) {
                    *debugLogPtr << "[ParseCameraKeyframe] Exception in ExpectToken for colon" << std::endl;
                    debugLogPtr->flush();
                }
                // Skip to next key
                while (pos < str.length() && str[pos] != ',' && str[pos] != '}') {
                    pos++;
                }
                continue;
            }
            
            try {
                    if (key == "position") {
                        if (debugLogPtr && debugLogPtr->is_open()) {
                            *debugLogPtr << "[ParseCameraKeyframe] About to extract position at position " << pos << std::endl;
                            debugLogPtr->flush();
                        }
                        try {
                            keyframe.position = ExtractVec3(str, pos);
                            if (debugLogPtr && debugLogPtr->is_open()) {
                                *debugLogPtr << "[ParseCameraKeyframe] Position: (" << keyframe.position.x << ", " << keyframe.position.y << ", " << keyframe.position.z << ")" << std::endl;
                                debugLogPtr->flush();
                            }
                        } catch (const std::exception& e) {
                            if (debugLogPtr && debugLogPtr->is_open()) {
                                *debugLogPtr << "[ParseCameraKeyframe] Exception extracting position: " << e.what() << std::endl;
                                debugLogPtr->flush();
                            }
                            keyframe.position = glm::vec3(0.0f);
                        } catch (...) {
                            if (debugLogPtr && debugLogPtr->is_open()) {
                                *debugLogPtr << "[ParseCameraKeyframe] Unknown exception extracting position" << std::endl;
                                debugLogPtr->flush();
                            }
                            keyframe.position = glm::vec3(0.0f);
                        }
                    } else if (key == "direction") {
                        if (debugLogPtr && debugLogPtr->is_open()) {
                            *debugLogPtr << "[ParseCameraKeyframe] About to extract direction at position " << pos << std::endl;
                            debugLogPtr->flush();
                        }
                        try {
                            keyframe.direction = ExtractVec3(str, pos);
                            if (debugLogPtr && debugLogPtr->is_open()) {
                                *debugLogPtr << "[ParseCameraKeyframe] Direction (before normalize): (" << keyframe.direction.x << ", " << keyframe.direction.y << ", " << keyframe.direction.z << ")" << std::endl;
                                debugLogPtr->flush();
                            }
                        } catch (const std::exception& e) {
                            if (debugLogPtr && debugLogPtr->is_open()) {
                                *debugLogPtr << "[ParseCameraKeyframe] Exception extracting direction: " << e.what() << std::endl;
                                debugLogPtr->flush();
                            }
                            keyframe.direction = glm::vec3(0.0f, 0.0f, 1.0f);
                        } catch (...) {
                            if (debugLogPtr && debugLogPtr->is_open()) {
                                *debugLogPtr << "[ParseCameraKeyframe] Unknown exception extracting direction" << std::endl;
                                debugLogPtr->flush();
                            }
                            keyframe.direction = glm::vec3(0.0f, 0.0f, 1.0f);
                        }
                    
                    // Normalize direction vector - with safety check
                    try {
                        // Check for NaN or Inf values
                        bool isValid = true;
                        if (std::isnan(keyframe.direction.x) || std::isinf(keyframe.direction.x) ||
                            std::isnan(keyframe.direction.y) || std::isinf(keyframe.direction.y) ||
                            std::isnan(keyframe.direction.z) || std::isinf(keyframe.direction.z)) {
                            if (debugLogPtr && debugLogPtr->is_open()) {
                                *debugLogPtr << "[ParseCameraKeyframe] Direction has NaN/Inf values, using default" << std::endl;
                                debugLogPtr->flush();
                            }
                            keyframe.direction = glm::vec3(0.0f, 0.0f, 1.0f);
                            isValid = false;
                        }
                        
                        if (isValid) {
                            float len = glm::length(keyframe.direction);
                            if (debugLogPtr && debugLogPtr->is_open()) {
                                *debugLogPtr << "[ParseCameraKeyframe] Direction length: " << len << std::endl;
                                debugLogPtr->flush();
                            }
                            
                            if (std::isnan(len) || std::isinf(len)) {
                                if (debugLogPtr && debugLogPtr->is_open()) {
                                    *debugLogPtr << "[ParseCameraKeyframe] Length is NaN/Inf, using default" << std::endl;
                                    debugLogPtr->flush();
                                }
                                keyframe.direction = glm::vec3(0.0f, 0.0f, 1.0f);
                            } else if (len > 0.0001f) {
                                keyframe.direction = keyframe.direction / len;
                                if (debugLogPtr && debugLogPtr->is_open()) {
                                    *debugLogPtr << "[ParseCameraKeyframe] Direction normalized" << std::endl;
                                    debugLogPtr->flush();
                                }
                            } else {
                                if (debugLogPtr && debugLogPtr->is_open()) {
                                    *debugLogPtr << "[ParseCameraKeyframe] Direction too short, using default" << std::endl;
                                    debugLogPtr->flush();
                                }
                                keyframe.direction = glm::vec3(0.0f, 0.0f, 1.0f);
                            }
                        }
                    } catch (const std::exception& e) {
                        if (debugLogPtr && debugLogPtr->is_open()) {
                            *debugLogPtr << "[ParseCameraKeyframe] Exception normalizing direction: " << e.what() << std::endl;
                            debugLogPtr->flush();
                        }
                        keyframe.direction = glm::vec3(0.0f, 0.0f, 1.0f);
                    } catch (...) {
                        if (debugLogPtr && debugLogPtr->is_open()) {
                            *debugLogPtr << "[ParseCameraKeyframe] Unknown exception normalizing direction" << std::endl;
                            debugLogPtr->flush();
                        }
                        keyframe.direction = glm::vec3(0.0f, 0.0f, 1.0f);
                    }
                } else if (key == "frame") {
                    keyframe.frame = static_cast<int>(ExtractNumber(str, pos));
                    if (debugLogPtr && debugLogPtr->is_open()) {
                        *debugLogPtr << "[ParseCameraKeyframe] Frame: " << keyframe.frame << std::endl;
                        debugLogPtr->flush();
                    }
                } else if (key == "time_tick") {
                    // Backward compatibility: convert time_tick to frame (multiply by 30 for 1 second intervals)
                    int timeTick = static_cast<int>(ExtractNumber(str, pos));
                    keyframe.frame = timeTick * 30;  // Convert to frames (30 frames per second)
                    if (debugLogPtr && debugLogPtr->is_open()) {
                        *debugLogPtr << "[ParseCameraKeyframe] Frame (from time_tick): " << keyframe.frame << std::endl;
                        debugLogPtr->flush();
                    }
                }
            } catch (const std::exception& e) {
                if (debugLogPtr && debugLogPtr->is_open()) {
                    *debugLogPtr << "[ParseCameraKeyframe] Exception parsing key '" << key << "': " << e.what() << std::endl;
                    debugLogPtr->flush();
                }
            } catch (...) {
                if (debugLogPtr && debugLogPtr->is_open()) {
                    *debugLogPtr << "[ParseCameraKeyframe] Unknown exception parsing key '" << key << "'" << std::endl;
                    debugLogPtr->flush();
                }
            }
            
            SkipWhitespace(str, pos);
            if (pos < str.length() && str[pos] == ',') {
                pos++;
            }
        }
        
        bool result = ExpectToken(str, pos, '}');
        if (debugLogPtr && debugLogPtr->is_open()) {
            *debugLogPtr << "[ParseCameraKeyframe] Completed, result: " << (result ? "true" : "false") << std::endl;
            debugLogPtr->flush();
        }
        return result;
    } catch (const std::exception& e) {
        if (debugLogPtr && debugLogPtr->is_open()) {
            *debugLogPtr << "[ParseCameraKeyframe] Exception: " << e.what() << std::endl;
            debugLogPtr->flush();
        }
        return false;
    } catch (...) {
        if (debugLogPtr && debugLogPtr->is_open()) {
            *debugLogPtr << "[ParseCameraKeyframe] Unknown exception" << std::endl;
            debugLogPtr->flush();
        }
        return false;
    }
}

static bool ParseAnimatedCameraConfig(const std::string& str, size_t& pos, AnimatedCameraConfig& camera) {
    // Use a static file handle to avoid multiple opens
    static std::ofstream* debugLogPtr = nullptr;
    static bool logInitialized = false;
    
    if (!logInitialized) {
        debugLogPtr = new std::ofstream("C:\\Users\\inque\\OneDrive\\Desktop\\Work\\games\\thegame\\build\\bin\\Release\\configloader_debug.log", std::ios::app);
        logInitialized = true;
    }
    
    if (!debugLogPtr || !debugLogPtr->is_open()) {
        // If log file can't be opened, continue without logging
        // Fall through to actual parsing
    } else {
        *debugLogPtr << "[ParseAnimatedCameraConfig] Starting at position " << pos << std::endl;
        debugLogPtr->flush();
    }
    
    try {
        if (!ExpectToken(str, pos, '{')) {
            if (debugLogPtr && debugLogPtr->is_open()) {
                *debugLogPtr << "[ParseAnimatedCameraConfig] Failed to find opening brace" << std::endl;
                debugLogPtr->flush();
            }
            return false;
        }
        if (debugLogPtr && debugLogPtr->is_open()) {
            *debugLogPtr << "[ParseAnimatedCameraConfig] Opening brace found" << std::endl;
            debugLogPtr->flush();
        }
        
        int keyCount = 0;
        while (pos < str.length() && str[pos] != '}') {
            keyCount++;
            if (keyCount > 10) {
                if (debugLogPtr && debugLogPtr->is_open()) {
                    *debugLogPtr << "[ParseAnimatedCameraConfig] Too many keys, breaking" << std::endl;
                    debugLogPtr->flush();
                }
                break;
            }
            
            SkipWhitespace(str, pos);
            if (pos >= str.length()) break;
            
            if (debugLogPtr && debugLogPtr->is_open()) {
                *debugLogPtr << "[ParseAnimatedCameraConfig] Extracting key " << keyCount << " at position " << pos << std::endl;
                debugLogPtr->flush();
            }
            
            std::string key = ExtractString(str, pos);
            if (debugLogPtr && debugLogPtr->is_open()) {
                *debugLogPtr << "[ParseAnimatedCameraConfig] Extracted key: '" << key << "'" << std::endl;
                debugLogPtr->flush();
            }
            
            ExpectToken(str, pos, ':');
            
            try {
                if (key == "name") {
                    camera.name = ExtractString(str, pos);
                    if (debugLogPtr && debugLogPtr->is_open()) {
                        *debugLogPtr << "[ParseAnimatedCameraConfig] Name: '" << camera.name << "'" << std::endl;
                        debugLogPtr->flush();
                    }
                } else if (key == "can_move") {
                    camera.canMove = ExtractBoolean(str, pos);
                    if (debugLogPtr && debugLogPtr->is_open()) {
                        *debugLogPtr << "[ParseAnimatedCameraConfig] Can move: " << (camera.canMove ? "true" : "false") << std::endl;
                        debugLogPtr->flush();
                    }
                } else if (key == "keyframes") {
                    if (debugLogPtr && debugLogPtr->is_open()) {
                        *debugLogPtr << "[ParseAnimatedCameraConfig] Parsing keyframes array..." << std::endl;
                        debugLogPtr->flush();
                    }
                    if (ExpectToken(str, pos, '[')) {
                        int kfCount = 0;
                        size_t lastPos = pos;  // Track position to detect infinite loops
                        int noProgressCount = 0;
                        
                        while (pos < str.length() && str[pos] != ']') {
                            kfCount++;
                            if (kfCount > 500) {
                                if (debugLogPtr && debugLogPtr->is_open()) {
                                    *debugLogPtr << "[ParseAnimatedCameraConfig] Too many keyframes, breaking" << std::endl;
                                    debugLogPtr->flush();
                                }
                                break;
                            }
                            
                            // Check for infinite loop - if position hasn't changed, break
                            if (pos == lastPos) {
                                noProgressCount++;
                                if (noProgressCount > 5) {
                                    if (debugLogPtr && debugLogPtr->is_open()) {
                                        *debugLogPtr << "[ParseAnimatedCameraConfig] Infinite loop detected at position " << pos << ", breaking" << std::endl;
                                        debugLogPtr->flush();
                                    }
                                    break;
                                }
                            } else {
                                noProgressCount = 0;
                                lastPos = pos;
                            }
                            
                            if (debugLogPtr && debugLogPtr->is_open()) {
                                *debugLogPtr << "[ParseAnimatedCameraConfig] Parsing keyframe " << kfCount << " at position " << pos << std::endl;
                                debugLogPtr->flush();
                            }
                            
                            CameraKeyframe keyframe;
                            size_t posBeforeParse = pos;
                            if (ParseCameraKeyframe(str, pos, keyframe)) {
                                // CRITICAL: Try-catch around push_back in case of memory issues
                                try {
                                    camera.keyframes.push_back(keyframe);
                                    if (debugLogPtr && debugLogPtr->is_open()) {
                                        *debugLogPtr << "[ParseAnimatedCameraConfig] Keyframe " << kfCount << " parsed successfully (frame=" << keyframe.frame << "), pos moved from " << posBeforeParse << " to " << pos << std::endl;
                                        debugLogPtr->flush();
                                    }
                                } catch (const std::exception& e) {
                                    if (debugLogPtr && debugLogPtr->is_open()) {
                                        *debugLogPtr << "[ParseAnimatedCameraConfig] Exception pushing keyframe: " << e.what() << std::endl;
                                        debugLogPtr->flush();
                                    }
                                    break;
                                } catch (...) {
                                    if (debugLogPtr && debugLogPtr->is_open()) {
                                        *debugLogPtr << "[ParseAnimatedCameraConfig] Unknown exception pushing keyframe" << std::endl;
                                        debugLogPtr->flush();
                                    }
                                    break;
                                }
                            } else {
                                if (debugLogPtr && debugLogPtr->is_open()) {
                                    *debugLogPtr << "[ParseAnimatedCameraConfig] Failed to parse keyframe " << kfCount << " at position " << posBeforeParse << std::endl;
                                    debugLogPtr->flush();
                                }
                                // If parsing failed, try to skip to next comma or closing bracket
                                SkipWhitespace(str, pos);
                                if (pos < str.length() && str[pos] != ',' && str[pos] != ']') {
                                    // Skip until we find a comma or closing bracket
                                    while (pos < str.length() && str[pos] != ',' && str[pos] != ']') {
                                        pos++;
                                    }
                                }
                            }
                            
                            SkipWhitespace(str, pos);
                            if (pos < str.length() && str[pos] == ',') {
                                pos++;
                            }
                            if (pos >= str.length()) {
                                if (debugLogPtr && debugLogPtr->is_open()) {
                                    *debugLogPtr << "[ParseAnimatedCameraConfig] Reached end of string" << std::endl;
                                    debugLogPtr->flush();
                                }
                                break;
                            }
                        }
                        ExpectToken(str, pos, ']');
                        if (debugLogPtr && debugLogPtr->is_open()) {
                            *debugLogPtr << "[ParseAnimatedCameraConfig] Parsed " << camera.keyframes.size() << " keyframes" << std::endl;
                            debugLogPtr->flush();
                        }
                    }
                }
            } catch (const std::exception& e) {
                if (debugLogPtr && debugLogPtr->is_open()) {
                    *debugLogPtr << "[ParseAnimatedCameraConfig] Exception parsing key '" << key << "': " << e.what() << std::endl;
                    debugLogPtr->flush();
                }
                // Skip to next key
                while (pos < str.length() && str[pos] != ',' && str[pos] != '}') {
                    pos++;
                }
            } catch (...) {
                if (debugLogPtr && debugLogPtr->is_open()) {
                    *debugLogPtr << "[ParseAnimatedCameraConfig] Unknown exception parsing key '" << key << "'" << std::endl;
                    debugLogPtr->flush();
                }
                // Skip to next key
                while (pos < str.length() && str[pos] != ',' && str[pos] != '}') {
                    pos++;
                }
            }
            
            SkipWhitespace(str, pos);
            if (pos < str.length() && str[pos] == ',') {
                pos++;
            }
        }
        
        bool result = ExpectToken(str, pos, '}');
        if (debugLogPtr && debugLogPtr->is_open()) {
            *debugLogPtr << "[ParseAnimatedCameraConfig] Completed, result: " << (result ? "true" : "false") << std::endl;
            debugLogPtr->flush();
        }
        return result;
    } catch (const std::exception& e) {
        if (debugLogPtr && debugLogPtr->is_open()) {
            *debugLogPtr << "[ParseAnimatedCameraConfig] Exception: " << e.what() << std::endl;
            debugLogPtr->flush();
        }
        return false;
    } catch (...) {
        if (debugLogPtr && debugLogPtr->is_open()) {
            *debugLogPtr << "[ParseAnimatedCameraConfig] Unknown exception" << std::endl;
            debugLogPtr->flush();
        }
        return false;
    }
}

// Parse new dictionary format: cameras dictionary
static bool ParseCamerasDictionary(const std::string& str, size_t& pos, SystemConfig& config) {
    if (!ExpectToken(str, pos, '{')) return false;
    
    while (pos < str.length() && str[pos] != '}') {
        SkipWhitespace(str, pos);
        if (pos >= str.length() || str[pos] == '}') break;
        
        // Extract camera name (key)
        std::string cameraName = ExtractString(str, pos);
        if (cameraName.empty()) break;
        
        ExpectToken(str, pos, ':');
        if (!ExpectToken(str, pos, '{')) {
            // Skip this camera
            while (pos < str.length() && str[pos] != ',' && str[pos] != '}') {
                pos++;
            }
            SkipWhitespace(str, pos);
            if (pos < str.length() && str[pos] == ',') pos++;
            continue;
        }
        
        AnimatedCameraConfig animCam;
        animCam.name = cameraName;
        animCam.canMove = false;
        
        // Store frames array temporarily in case it's parsed before positions
        std::vector<float> tempFrameArray;
        std::vector<CameraKeyframe> tempKeyframes;
        
        // Parse camera data: positions, frames, can_move
        while (pos < str.length() && str[pos] != '}') {
            SkipWhitespace(str, pos);
            std::string key = ExtractString(str, pos);
            if (key.empty()) break;
            
            ExpectToken(str, pos, ':');
            
            if (key == "positions") {
                // Parse array of [x, y, z, hx, hy, hz] arrays
                if (ExpectToken(str, pos, '[')) {
                    while (pos < str.length() && str[pos] != ']') {
                        SkipWhitespace(str, pos);
                        if (pos < str.length() && str[pos] == ']') break;
                        
                        // Extract [x, y, z, hx, hy, hz]
                        auto posArr = ExtractFloatArray(str, pos);
                        if (posArr.size() >= 3) {
                            glm::vec3 position(posArr[0], posArr[1], posArr[2]);
                            glm::vec3 direction(0.0f, 0.0f, -1.0f);
                            
                            // Extract heading direction if provided (indices 3, 4, 5)
                            if (posArr.size() >= 6) {
                                direction = glm::vec3(posArr[3], posArr[4], posArr[5]);
                                float len = glm::length(direction);
                                if (len > 0.0001f) {
                                    direction = direction / len;
                                } else {
                                    direction = glm::vec3(0.0f, 0.0f, -1.0f);
                                }
                            } else {
                                // Default: look at origin
                                direction = -position;
                                float len = glm::length(direction);
                                if (len > 0.0001f) {
                                    direction = direction / len;
                                } else {
                                    direction = glm::vec3(0.0f, 0.0f, -1.0f);
                                }
                            }
                            
                            // Frame will be set when we parse the frames array or match later
                            CameraKeyframe kf;
                            kf.position = position;
                            kf.direction = direction;
                            kf.frame = -1; // Will be set later
                            tempKeyframes.push_back(kf);
                        }
                        
                        SkipWhitespace(str, pos);
                        if (pos < str.length() && str[pos] == ',') pos++;
                    }
                    ExpectToken(str, pos, ']');
                }
            } else if (key == "frames") {
                // Parse array of frame numbers - store temporarily
                tempFrameArray = ExtractFloatArray(str, pos);
            } else if (key == "can_move") {
                animCam.canMove = ExtractBoolean(str, pos);
            } else {
                // Skip unknown key
                SkipWhitespace(str, pos);
                if (pos < str.length() && str[pos] == '[') {
                    // Skip array
                    int depth = 1;
                    pos++;
                    while (pos < str.length() && depth > 0) {
                        if (str[pos] == '[') depth++;
                        else if (str[pos] == ']') depth--;
                        pos++;
                    }
                } else if (pos < str.length() && str[pos] == '{') {
                    // Skip object
                    int depth = 1;
                    pos++;
                    while (pos < str.length() && depth > 0) {
                        if (str[pos] == '{') depth++;
                        else if (str[pos] == '}') depth--;
                        pos++;
                    }
                } else {
                    // Skip value
                    while (pos < str.length() && str[pos] != ',' && str[pos] != '}') {
                        pos++;
                    }
                }
            }
            
            SkipWhitespace(str, pos);
            if (pos < str.length() && str[pos] == ',') pos++;
        }
        
        ExpectToken(str, pos, '}');
        
        // Now match frames to keyframes (handles any order of keys in JSON)
        animCam.keyframes = tempKeyframes;
        if (!tempFrameArray.empty() && !animCam.keyframes.empty()) {
            // Match frames to positions - ensure arrays match in size
            size_t minSize = std::min(tempFrameArray.size(), animCam.keyframes.size());
            if (tempFrameArray.size() != animCam.keyframes.size()) {
                LOG_WARNING_FMT("ConfigLoader", "Camera '%s': Frames array (%zu) and positions array (%zu) have different sizes, matching %zu elements", 
                               cameraName.c_str(), tempFrameArray.size(), animCam.keyframes.size(), minSize);
            }
            for (size_t i = 0; i < minSize; i++) {
                animCam.keyframes[i].frame = static_cast<int>(tempFrameArray[i]);
            }
        } else if (animCam.keyframes.empty()) {
            LOG_ERROR_FMT("ConfigLoader", "Camera '%s': No positions found", cameraName.c_str());
        } else if (tempFrameArray.empty()) {
            LOG_ERROR_FMT("ConfigLoader", "Camera '%s': No frames found", cameraName.c_str());
        }
        
        // Remove keyframes with invalid frames and sort
        size_t beforeRemove = animCam.keyframes.size();
        animCam.keyframes.erase(
            std::remove_if(animCam.keyframes.begin(), animCam.keyframes.end(),
                [](const CameraKeyframe& kf) { return kf.frame < 0; }),
            animCam.keyframes.end());
        size_t afterRemove = animCam.keyframes.size();
        
        if (beforeRemove != afterRemove) {
            LOG_WARNING_FMT("ConfigLoader", "Camera '%s': Removed %zu keyframes with invalid frame numbers", 
                           cameraName.c_str(), beforeRemove - afterRemove);
        }
        
        std::sort(animCam.keyframes.begin(), animCam.keyframes.end(),
            [](const CameraKeyframe& a, const CameraKeyframe& b) { return a.frame < b.frame; });
        
        if (!animCam.keyframes.empty()) {
            LOG_INFO_FMT("ConfigLoader", "Camera '%s': Added with %zu keyframes (frames: %d to %d)", 
                        cameraName.c_str(), animCam.keyframes.size(), 
                        animCam.keyframes.front().frame, animCam.keyframes.back().frame);
            // Log first and last keyframe positions for verification
            LOG_INFO_FMT("ConfigLoader", "Camera '%s': First keyframe pos=(%.2f,%.2f,%.2f) dir=(%.2f,%.2f,%.2f)", 
                        cameraName.c_str(),
                        animCam.keyframes.front().position.x, animCam.keyframes.front().position.y, animCam.keyframes.front().position.z,
                        animCam.keyframes.front().direction.x, animCam.keyframes.front().direction.y, animCam.keyframes.front().direction.z);
            LOG_INFO_FMT("ConfigLoader", "Camera '%s': Last keyframe pos=(%.2f,%.2f,%.2f) dir=(%.2f,%.2f,%.2f)", 
                        cameraName.c_str(),
                        animCam.keyframes.back().position.x, animCam.keyframes.back().position.y, animCam.keyframes.back().position.z,
                        animCam.keyframes.back().direction.x, animCam.keyframes.back().direction.y, animCam.keyframes.back().direction.z);
            config.animatedCameras.push_back(animCam);
        } else {
            LOG_ERROR_FMT("ConfigLoader", "Camera '%s': No valid keyframes after parsing", cameraName.c_str());
        }
        
        SkipWhitespace(str, pos);
        if (pos < str.length() && str[pos] == ',') pos++;
    }
    
    return ExpectToken(str, pos, '}');
}

// Parse new dictionary format: objects dictionary
static bool ParseObjectsDictionary(const std::string& str, size_t& pos, SystemConfig& config, 
                                    const std::map<std::string, std::map<std::string, std::string>>& objectFiles) {
    if (!ExpectToken(str, pos, '{')) return false;
    
    while (pos < str.length() && str[pos] != '}') {
        SkipWhitespace(str, pos);
        if (pos >= str.length() || str[pos] == '}') break;
        
        // Extract object name (key)
        std::string objectName = ExtractString(str, pos);
        if (objectName.empty()) break;
        
        LOG_INFO_FMT("ConfigLoader", "Parsing object: '%s'", objectName.c_str());
        
        ExpectToken(str, pos, ':');
        if (!ExpectToken(str, pos, '{')) {
            // Skip this object
            LOG_WARNING_FMT("ConfigLoader", "Object '%s': Missing opening brace, skipping", objectName.c_str());
            while (pos < str.length() && str[pos] != ',' && str[pos] != '}') {
                pos++;
            }
            SkipWhitespace(str, pos);
            if (pos < str.length() && str[pos] == ',') pos++;
            continue;
        }
        
        ObjectConfig obj;
        obj.name = objectName;
        obj.visible = true;
        obj.scale = glm::vec3(1.0f);
        obj.rotation = glm::vec3(0.0f);
        obj.startTime = 0.0f;
        
        // Get file paths from object_files dictionary
        LOG_INFO_FMT("ConfigLoader", "Object '%s': Looking up in object_files (total entries: %zu)", objectName.c_str(), objectFiles.size());
        auto it = objectFiles.find(objectName);
        if (it != objectFiles.end()) {
            const auto& files = it->second;
            obj.objFile = files.count("obj_file") ? files.at("obj_file") : "";
            obj.animationFbx = files.count("fbx_file") ? files.at("fbx_file") : "";
            obj.skelFile = files.count("skel_file") ? files.at("skel_file") : "";
            obj.weightsFile = files.count("weights_file") ? files.at("weights_file") : "";
            LOG_INFO_FMT("ConfigLoader", "Object '%s': Set obj_file='%s' from object_files (found %zu file entries)", 
                        objectName.c_str(), obj.objFile.c_str(), files.size());
        } else {
            // Log all available object file names for debugging
            LOG_WARNING_FMT("ConfigLoader", "Object '%s': No entry in object_files dictionary. Available entries:", objectName.c_str());
            for (const auto& entry : objectFiles) {
                LOG_WARNING_FMT("ConfigLoader", "  - '%s'", entry.first.c_str());
            }
        }
        
        // Parse object data: positions, frames
        while (pos < str.length() && str[pos] != '}') {
            SkipWhitespace(str, pos);
            std::string key = ExtractString(str, pos);
            if (key.empty()) break;
            
            ExpectToken(str, pos, ':');
            
            if (key == "positions") {
                // Parse array of [x, y, z, hx, hy, hz] arrays
                if (ExpectToken(str, pos, '[')) {
                    while (pos < str.length() && str[pos] != ']') {
                        SkipWhitespace(str, pos);
                        if (pos < str.length() && str[pos] == ']') break;
                        
                        // Extract [x, y, z, hx, hy, hz]
                        auto posArr = ExtractFloatArray(str, pos);
                        if (posArr.size() >= 3) {
                            glm::vec3 position(posArr[0], posArr[1], posArr[2]);
                            
                            // Frame will be set when we parse the frames array
                            ObjectKeyframe kf;
                            kf.position = position;
                            kf.frame = -1; // Will be set later
                            obj.keyframes.push_back(kf);
                        }
                        
                        SkipWhitespace(str, pos);
                        if (pos < str.length() && str[pos] == ',') pos++;
                    }
                    ExpectToken(str, pos, ']');
                }
            } else if (key == "frames") {
                // Parse array of frame numbers
                auto frameArr = ExtractFloatArray(str, pos);
                // Match frames to positions
                for (size_t i = 0; i < frameArr.size() && i < obj.keyframes.size(); i++) {
                    obj.keyframes[i].frame = static_cast<int>(frameArr[i]);
                }
            } else {
                // Skip unknown key
                SkipWhitespace(str, pos);
                if (pos < str.length() && str[pos] == '[') {
                    // Skip array
                    int depth = 1;
                    pos++;
                    while (pos < str.length() && depth > 0) {
                        if (str[pos] == '[') depth++;
                        else if (str[pos] == ']') depth--;
                        pos++;
                    }
                } else if (pos < str.length() && str[pos] == '{') {
                    // Skip object
                    int depth = 1;
                    pos++;
                    while (pos < str.length() && depth > 0) {
                        if (str[pos] == '{') depth++;
                        else if (str[pos] == '}') depth--;
                        pos++;
                    }
                } else {
                    // Skip value
                    while (pos < str.length() && str[pos] != ',' && str[pos] != '}') {
                        pos++;
                    }
                }
            }
            
            SkipWhitespace(str, pos);
            if (pos < str.length() && str[pos] == ',') pos++;
        }
        
        ExpectToken(str, pos, '}');
        
        // Remove keyframes with invalid frames and sort
        obj.keyframes.erase(
            std::remove_if(obj.keyframes.begin(), obj.keyframes.end(),
                [](const ObjectKeyframe& kf) { return kf.frame < 0; }),
            obj.keyframes.end());
        std::sort(obj.keyframes.begin(), obj.keyframes.end(),
            [](const ObjectKeyframe& a, const ObjectKeyframe& b) { return a.frame < b.frame; });
        
        if (!obj.keyframes.empty()) {
            // Add object if it has keyframes, even if obj_file is missing
            // The model loading code will handle missing files gracefully
            if (!obj.objFile.empty()) {
                LOG_INFO_FMT("ConfigLoader", "Object '%s': Added with %zu keyframes, obj_file='%s'", 
                            objectName.c_str(), obj.keyframes.size(), obj.objFile.c_str());
            } else {
                LOG_WARNING_FMT("ConfigLoader", "Object '%s': Added with %zu keyframes but no obj_file (will be added without model)", 
                               objectName.c_str(), obj.keyframes.size());
            }
            config.objects.push_back(obj);
        } else {
            LOG_WARNING_FMT("ConfigLoader", "Object '%s': No keyframes, skipping", objectName.c_str());
        }
        
        SkipWhitespace(str, pos);
        if (pos < str.length() && str[pos] == ',') pos++;
    }
    
    return ExpectToken(str, pos, '}');
}

// Parse lights array
static bool ParseLightsArray(const std::string& str, size_t& pos, SystemConfig& config) {
    if (!ExpectToken(str, pos, '[')) return false;
    
    while (pos < str.length() && str[pos] != ']') {
        SkipWhitespace(str, pos);
        if (pos < str.length() && str[pos] == ']') break;
        
        if (!ExpectToken(str, pos, '{')) {
            // Skip to next entry or closing bracket
            while (pos < str.length() && str[pos] != ',' && str[pos] != ']') {
                pos++;
            }
            if (pos < str.length() && str[pos] == ',') pos++;
            continue;
        }
        
        LightConfig light;
        
        while (pos < str.length() && str[pos] != '}') {
            SkipWhitespace(str, pos);
            std::string key = ExtractString(str, pos);
            if (key.empty()) break;
            
            ExpectToken(str, pos, ':');
            
            if (key == "name") {
                light.name = ExtractString(str, pos);
            } else if (key == "type") {
                std::string typeStr = ExtractString(str, pos);
                if (typeStr == "directional") {
                    light.type = LightType::Directional;
                } else if (typeStr == "point") {
                    light.type = LightType::Point;
                } else if (typeStr == "spot") {
                    light.type = LightType::Spot;
                }
            } else if (key == "colour" || key == "color") {
                auto colorArr = ExtractFloatArray(str, pos);
                if (colorArr.size() >= 3) {
                    light.colour = glm::vec3(colorArr[0], colorArr[1], colorArr[2]);
                }
            } else if (key == "ambient_intensity") {
                light.ambientIntensity = ExtractNumber(str, pos);
            } else if (key == "diffuse_intensity") {
                light.diffuseIntensity = ExtractNumber(str, pos);
            } else if (key == "direction_or_position") {
                auto dirPosArr = ExtractFloatArray(str, pos);
                if (dirPosArr.size() >= 3) {
                    light.directionOrPosition = glm::vec3(dirPosArr[0], dirPosArr[1], dirPosArr[2]);
                }
            } else if (key == "constant") {
                light.constant = ExtractNumber(str, pos);
            } else if (key == "linear") {
                light.linear = ExtractNumber(str, pos);
            } else if (key == "exponent") {
                light.exponent = ExtractNumber(str, pos);
            } else if (key == "edge") {
                light.edge = ExtractNumber(str, pos);
            } else {
                // Skip unknown key
                SkipWhitespace(str, pos);
                if (pos < str.length() && str[pos] == '[') {
                    // Skip array
                    int depth = 1;
                    pos++;
                    while (pos < str.length() && depth > 0) {
                        if (str[pos] == '[') depth++;
                        else if (str[pos] == ']') depth--;
                        pos++;
                    }
                } else if (pos < str.length() && str[pos] == '{') {
                    // Skip object
                    int depth = 1;
                    pos++;
                    while (pos < str.length() && depth > 0) {
                        if (str[pos] == '{') depth++;
                        else if (str[pos] == '}') depth--;
                        pos++;
                    }
                } else {
                    // Skip value
                    while (pos < str.length() && str[pos] != ',' && str[pos] != '}') {
                        pos++;
                    }
                }
            }
            
            SkipWhitespace(str, pos);
            if (pos < str.length() && str[pos] == ',') pos++;
        }
        
        ExpectToken(str, pos, '}');
        
        // Normalize direction for directional lights
        if (light.type == LightType::Directional) {
            float len = glm::length(light.directionOrPosition);
            if (len > 0.0001f) {
                light.directionOrPosition = glm::normalize(light.directionOrPosition);
            } else {
                light.directionOrPosition = glm::vec3(0.0f, 0.0f, -1.0f);
            }
        }
        
        config.lights.push_back(light);
        LOG_INFO_FMT("ConfigLoader", "Light '%s' parsed: type=%d, color=(%.2f,%.2f,%.2f)", 
                    light.name.c_str(), static_cast<int>(light.type),
                    light.colour.x, light.colour.y, light.colour.z);
        
        SkipWhitespace(str, pos);
        if (pos < str.length() && str[pos] == ',') pos++;
    }
    
    return ExpectToken(str, pos, ']');
}

// Parse object_files dictionary
static bool ParseObjectFilesDictionary(const std::string& str, size_t& pos, 
                                       std::map<std::string, std::map<std::string, std::string>>& objectFiles) {
    if (!ExpectToken(str, pos, '{')) return false;
    
    int entryCount = 0;
    while (pos < str.length() && str[pos] != '}') {
        SkipWhitespace(str, pos);
        if (pos >= str.length() || str[pos] == '}') break;
        
        // Safety check to prevent infinite loop
        entryCount++;
        if (entryCount > 100) {
            LOG_ERROR("ConfigLoader", "Too many object_files entries, breaking");
            break;
        }
        
        // Extract object name (key)
        std::string objectName = ExtractString(str, pos);
        if (objectName.empty()) break;
        
        LOG_INFO_FMT("ConfigLoader", "Parsing object_files entry: '%s'", objectName.c_str());
        
        ExpectToken(str, pos, ':');
        if (!ExpectToken(str, pos, '{')) {
            LOG_WARNING_FMT("ConfigLoader", "Object file entry '%s': Missing opening brace, skipping", objectName.c_str());
            // Skip this entry
            while (pos < str.length() && str[pos] != ',' && str[pos] != '}') {
                pos++;
            }
            SkipWhitespace(str, pos);
            if (pos < str.length() && str[pos] == ',') pos++;
            continue;
        }
        
        std::map<std::string, std::string> files;
        
        // Parse file paths
        while (pos < str.length() && str[pos] != '}') {
            SkipWhitespace(str, pos);
            std::string key = ExtractString(str, pos);
            if (key.empty()) break;
            
            ExpectToken(str, pos, ':');
            
            if (key == "obj_file" || key == "fbx_file" || key == "skel_file" || key == "weights_file") {
                SkipWhitespace(str, pos);
                std::string value;
                if (pos < str.length() && str[pos] == '"') {
                    // It's a string value
                    value = ExtractString(str, pos);
                } else if (pos < str.length() && str.substr(pos, 4) == "null") {
                    // It's null, skip it
                    pos += 4;
                    value = "";
                } else {
                    // Try to extract as string anyway
                    value = ExtractString(str, pos);
                }
                if (!value.empty()) {
                    files[key] = value;
                    LOG_INFO_FMT("ConfigLoader", "  Set %s='%s'", key.c_str(), value.c_str());
                }
            } else {
                // Skip unknown key
                SkipWhitespace(str, pos);
                if (pos < str.length() && str[pos] == '"') {
                    ExtractString(str, pos); // Skip string value
                } else if (pos < str.length() && str[pos] == '[') {
                    // Skip array
                    int depth = 1;
                    pos++;
                    while (pos < str.length() && depth > 0) {
                        if (str[pos] == '[') depth++;
                        else if (str[pos] == ']') depth--;
                        pos++;
                    }
                } else if (pos < str.length() && str[pos] == '{') {
                    // Skip object
                    int depth = 1;
                    pos++;
                    while (pos < str.length() && depth > 0) {
                        if (str[pos] == '{') depth++;
                        else if (str[pos] == '}') depth--;
                        pos++;
                    }
                } else {
                    // Skip value
                    while (pos < str.length() && str[pos] != ',' && str[pos] != '}') {
                        pos++;
                    }
                }
            }
            
            SkipWhitespace(str, pos);
            if (pos < str.length() && str[pos] == ',') pos++;
        }
        
        ExpectToken(str, pos, '}');
        
        objectFiles[objectName] = files;
        LOG_INFO_FMT("ConfigLoader", "Object file entry '%s': Added with %zu file entries (obj_file='%s')", 
                    objectName.c_str(), files.size(), 
                    files.count("obj_file") ? files.at("obj_file").c_str() : "(empty)");
        if (false) { // Keep old log format for reference
            LOG_INFO_FMT("ConfigLoader", "Object files for '%s': obj_file='%s', fbx_file='%s', skel_file='%s', weights_file='%s'", 
                        objectName.c_str(),
                        files.count("obj_file") ? files.at("obj_file").c_str() : "(none)",
                        files.count("fbx_file") ? files.at("fbx_file").c_str() : "(none)",
                        files.count("skel_file") ? files.at("skel_file").c_str() : "(none)",
                        files.count("weights_file") ? files.at("weights_file").c_str() : "(none)");
        } else {
            LOG_WARNING_FMT("ConfigLoader", "Object files for '%s': No file paths found", objectName.c_str());
        }
        
        SkipWhitespace(str, pos);
        if (pos < str.length() && str[pos] == ',') pos++;
    }
    
    return ExpectToken(str, pos, '}');
}

bool ConfigLoader::ParseJsonFile(const std::string& filepath, SystemConfig& config) {
    // Initialize capture settings to defaults
    config.captureEnabled = false;
    config.captureOutputDir = "";
    config.captureInterval = 1;  // Default: capture every frame
    std::ofstream debugLog("C:\\Users\\inque\\OneDrive\\Desktop\\Work\\games\\thegame\\build\\bin\\Release\\configloader_debug.log", std::ios::app);
    debugLog << "[ParseJsonFile] Called with path: " << filepath << std::endl;
    debugLog.flush();
    
    try {
        debugLog << "[ParseJsonFile] Opening file..." << std::endl;
        debugLog.flush();
        
        std::ifstream file(filepath);
        if (!file.is_open()) {
            debugLog << "[ParseJsonFile] Failed to open file" << std::endl;
            debugLog.flush();
            debugLog.close();
            return false;
        }
        debugLog << "[ParseJsonFile] File opened successfully" << std::endl;
        debugLog.flush();
        
        debugLog << "[ParseJsonFile] Reading file contents..." << std::endl;
        debugLog.flush();
        
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json = buffer.str();
        file.close();
        
        debugLog << "[ParseJsonFile] File read complete, size: " << json.size() << " bytes" << std::endl;
        debugLog.flush();
        
        if (json.empty()) {
            debugLog << "[ParseJsonFile] File is empty" << std::endl;
            debugLog.flush();
            debugLog.close();
            return false;
        }
        
        debugLog << "[ParseJsonFile] Starting JSON parsing..." << std::endl;
        debugLog.flush();
        
        size_t pos = 0;
        LOG_INFO("ConfigLoader", "Looking for opening brace...");
        if (!ExpectToken(json, pos, '{')) {
            LOG_ERROR("ConfigLoader", "Expected opening brace '{' not found");
            return false;
        }
        LOG_INFO("ConfigLoader", "Opening brace found, starting to parse keys...");
        
        // Store object_files for use when parsing objects
        std::map<std::string, std::map<std::string, std::string>> objectFiles;
        
        // FIRST PASS: Collect object_files before parsing objects
        // This ensures object_files are available when parsing objects
        LOG_INFO("ConfigLoader", "First pass: Collecting object_files...");
        size_t firstPassPos = 1; // Start after opening brace
        int firstPassKeyCount = 0;
        while (firstPassPos < json.length() && json[firstPassPos] != '}') {
            firstPassKeyCount++;
            if (firstPassKeyCount > 50) {
                LOG_WARNING("ConfigLoader", "First pass: Too many keys, breaking to prevent infinite loop");
                break;
            }
            
            SkipWhitespace(json, firstPassPos);
            if (firstPassPos >= json.length() || json[firstPassPos] == '}') break;
            
            std::string key = ExtractString(json, firstPassPos);
            if (key.empty()) break;
            
            LOG_INFO_FMT("ConfigLoader", "First pass: Found key '%s'", key.c_str());
            
            ExpectToken(json, firstPassPos, ':');
            SkipWhitespace(json, firstPassPos);
            
            if (key == "object_files") {
                LOG_INFO("ConfigLoader", "Found object_files in first pass, parsing...");
                if (ParseObjectFilesDictionary(json, firstPassPos, objectFiles)) {
                    LOG_INFO_FMT("ConfigLoader", "First pass: Parsed %zu object file entries", objectFiles.size());
                    // Log all parsed entries
                    for (const auto& entry : objectFiles) {
                        LOG_INFO_FMT("ConfigLoader", "  - '%s': obj_file='%s'", 
                                    entry.first.c_str(),
                                    entry.second.count("obj_file") ? entry.second.at("obj_file").c_str() : "(empty)");
                    }
                } else {
                    LOG_ERROR("ConfigLoader", "First pass: Failed to parse object_files");
                }
                break; // Found it, no need to continue first pass
            } else {
                // Skip this key-value pair
                SkipWhitespace(json, firstPassPos);
                if (firstPassPos < json.length() && json[firstPassPos] == '{') {
                    // Skip object
                    int depth = 1;
                    firstPassPos++;
                    while (firstPassPos < json.length() && depth > 0) {
                        if (json[firstPassPos] == '{') depth++;
                        else if (json[firstPassPos] == '}') depth--;
                        firstPassPos++;
                    }
                } else if (firstPassPos < json.length() && json[firstPassPos] == '[') {
                    // Skip array
                    int depth = 1;
                    firstPassPos++;
                    while (firstPassPos < json.length() && depth > 0) {
                        if (json[firstPassPos] == '[') depth++;
                        else if (json[firstPassPos] == ']') depth--;
                        firstPassPos++;
                    }
                } else if (firstPassPos < json.length() && json[firstPassPos] == '"') {
                    // Skip string value
                    firstPassPos++; // Skip opening quote
                    bool escaped = false;
                    while (firstPassPos < json.length()) {
                        if (escaped) {
                            escaped = false;
                            firstPassPos++;
                        } else if (json[firstPassPos] == '\\') {
                            escaped = true;
                            firstPassPos++;
                        } else if (json[firstPassPos] == '"') {
                            firstPassPos++; // Skip closing quote
                            break;
                        } else {
                            firstPassPos++;
                        }
                    }
                } else {
                    // Skip value (number, boolean, null)
                    while (firstPassPos < json.length() && 
                           json[firstPassPos] != ',' && 
                           json[firstPassPos] != '}' &&
                           json[firstPassPos] != '{' &&
                           json[firstPassPos] != '[') {
                        firstPassPos++;
                    }
                }
            }
            
            SkipWhitespace(json, firstPassPos);
            if (firstPassPos < json.length() && json[firstPassPos] == ',') {
                firstPassPos++;
            }
        }
        
        LOG_INFO("ConfigLoader", "First pass complete, starting main parse...");
        
        int keyCount = 0;
        while (pos < json.length() && json[pos] != '}') {
            keyCount++;
            if (keyCount > 100) {
                LOG_ERROR("ConfigLoader", "Too many keys, possible infinite loop");
                break;
            }
            // Safety check to prevent infinite loop
            if (pos >= json.length()) {
                break;
            }
            
            SkipWhitespace(json, pos);
            
            // Safety check after whitespace skip
            if (pos >= json.length()) {
                LOG_INFO("ConfigLoader", "Reached end of JSON while skipping whitespace");
                break;
            }
            
            LOG_INFO_FMT("ConfigLoader", "Extracting key %d at position %zu...", keyCount, pos);
            std::string key = ExtractString(json, pos);
            LOG_INFO_FMT("ConfigLoader", "Extracted key: '%s'", key.c_str());
            
            if (key.empty() && pos >= json.length()) {
                LOG_INFO("ConfigLoader", "Empty key or end of file");
                break;  // End of file or invalid key
            }
            
            LOG_INFO("ConfigLoader", "Expecting colon...");
            ExpectToken(json, pos, ':');
            
            // Safety check before accessing json[pos]
            if (pos >= json.length()) {
                break;
            }
            
            try {
                LOG_INFO_FMT("ConfigLoader", "Processing key: '%s'", key.c_str());
                if (key == "version") {
                    config.version = ExtractString(json, pos);
                    LOG_INFO_FMT("ConfigLoader", "Version: %s", config.version.c_str());
                } else if (key == "total_duration") {
                    config.totalDuration = ExtractNumber(json, pos);
                    LOG_INFO_FMT("ConfigLoader", "Total duration: %f", config.totalDuration);
                } else if (key == "camera") {
                    LOG_INFO("ConfigLoader", "Parsing camera config...");
                    ParseCameraConfig(json, pos, config.camera);
                    LOG_INFO("ConfigLoader", "Camera config parsed");
                } else if (key == "total_frames") {
                    config.totalFrames = static_cast<int>(ExtractNumber(json, pos));
                    LOG_INFO_FMT("ConfigLoader", "Total frames: %d", config.totalFrames);
                } else if (key == "num_time_ticks") {
                    // Backward compatibility: convert time ticks to frames
                    int numTimeTicks = static_cast<int>(ExtractNumber(json, pos));
                    config.totalFrames = numTimeTicks * 30;  // 30 frames per tick (1 second per tick)
                    LOG_INFO_FMT("ConfigLoader", "Total frames (from ticks): %d", config.totalFrames);
                } else if (key == "animated_cameras") {
                    LOG_INFO("ConfigLoader", "Parsing animated_cameras array...");
                    if (ExpectToken(json, pos, '[')) {
                        int camCount = 0;
                        size_t lastPos = pos;
                        int noProgressCount = 0;
                        
                        while (pos < json.length() && json[pos] != ']') {
                            camCount++;
                            
                            // Check for infinite loop
                            if (pos == lastPos) {
                                noProgressCount++;
                                if (noProgressCount > 5) {
                                    LOG_ERROR("ConfigLoader", "Infinite loop detected while parsing animated cameras, breaking");
                                    break;
                                }
                            } else {
                                noProgressCount = 0;
                                lastPos = pos;
                            }
                            
                            LOG_INFO_FMT("ConfigLoader", "Parsing animated camera %d at position %zu...", camCount, pos);
                            
                            AnimatedCameraConfig animCam;
                            size_t posBeforeParse = pos;
                            
                            try {
                                if (ParseAnimatedCameraConfig(json, pos, animCam)) {
                                    // Only add if we have keyframes
                                    if (!animCam.keyframes.empty()) {
                                        config.animatedCameras.push_back(animCam);
                                        LOG_INFO_FMT("ConfigLoader", "Animated camera %d parsed: '%s' with %zu keyframes", 
                                                    camCount, animCam.name.c_str(), animCam.keyframes.size());
                                    } else {
                                        LOG_WARNING_FMT("ConfigLoader", "Animated camera %d has no keyframes, skipping", camCount);
                                    }
                                } else {
                                    LOG_ERROR_FMT("ConfigLoader", "Failed to parse animated camera %d at position %zu", camCount, posBeforeParse);
                                    // Skip to next camera or closing bracket
                                    SkipWhitespace(json, pos);
                                    if (pos < json.length() && json[pos] != ',' && json[pos] != ']') {
                                        while (pos < json.length() && json[pos] != ',' && json[pos] != ']') {
                                            pos++;
                                        }
                                    }
                                }
                            } catch (const std::exception& e) {
                                LOG_ERROR_FMT("ConfigLoader", "Exception parsing animated camera %d: %s", camCount, e.what());
                                // Skip to next camera
                                while (pos < json.length() && json[pos] != ',' && json[pos] != ']') {
                                    pos++;
                                }
                            } catch (...) {
                                LOG_ERROR_FMT("ConfigLoader", "Unknown exception parsing animated camera %d", camCount);
                                // Skip to next camera
                                while (pos < json.length() && json[pos] != ',' && json[pos] != ']') {
                                    pos++;
                                }
                            }
                            
                            SkipWhitespace(json, pos);
                            if (pos < json.length() && json[pos] == ',') {
                                pos++;
                            }
                            if (pos >= json.length()) {
                                LOG_ERROR("ConfigLoader", "Reached end of JSON while parsing animated cameras");
                                break;
                            }
                            if (camCount > 20) {
                                LOG_ERROR("ConfigLoader", "Too many animated cameras, breaking");
                                break;
                            }
                        }
                        ExpectToken(json, pos, ']');
                        LOG_INFO_FMT("ConfigLoader", "Parsed %zu animated cameras", config.animatedCameras.size());
                    }
                } else if (key == "cameras") {
                    // New dictionary format for cameras
                    LOG_INFO("ConfigLoader", "Parsing cameras dictionary (new format)...");
                    ParseCamerasDictionary(json, pos, config);
                } else if (key == "object_files") {
                    // New dictionary format for object files
                    LOG_INFO("ConfigLoader", "Parsing object_files dictionary (new format)...");
                    size_t savedPos = pos;
                    if (ParseObjectFilesDictionary(json, pos, objectFiles)) {
                        LOG_INFO_FMT("ConfigLoader", "Parsed %zu object file entries", objectFiles.size());
                    } else {
                        pos = savedPos; // Restore position if parsing failed
                    }
                } else if (key == "lights") {
                    // Parse lights array
                    LOG_INFO("ConfigLoader", "Parsing lights array...");
                    size_t savedPos = pos;
                    if (ParseLightsArray(json, pos, config)) {
                        LOG_INFO_FMT("ConfigLoader", "Parsed %zu lights", config.lights.size());
                    } else {
                        pos = savedPos; // Restore position if parsing failed
                        LOG_WARNING("ConfigLoader", "Failed to parse lights array");
                    }
                } else if (key == "capture_enabled" || key == "captureEnabled") {
                    std::string val = ExtractString(json, pos);
                    if (val.empty()) {
                        config.captureEnabled = ExtractNumber(json, pos) != 0.0;
                    } else {
                        config.captureEnabled = (val == "true" || val == "1");
                    }
                    LOG_INFO_FMT("ConfigLoader", "Capture enabled: %s", config.captureEnabled ? "true" : "false");
                } else if (key == "capture_output_dir" || key == "captureOutputDir") {
                    config.captureOutputDir = ExtractString(json, pos);
                    LOG_INFO_FMT("ConfigLoader", "Capture output dir: %s", config.captureOutputDir.c_str());
                } else if (key == "capture_interval" || key == "captureInterval") {
                    config.captureInterval = static_cast<int>(ExtractNumber(json, pos));
                    if (config.captureInterval < 1) {
                        config.captureInterval = 1;  // Minimum is 1 (every frame)
                    }
                    LOG_INFO_FMT("ConfigLoader", "Capture interval: %d frames", config.captureInterval);
                } else if (key == "objects") {
                    // Check if it's new dictionary format or old array format
                    SkipWhitespace(json, pos);
                    if (pos < json.length() && json[pos] == '{') {
                        // New dictionary format
                        LOG_INFO("ConfigLoader", "Parsing objects dictionary (new format)...");
                        ParseObjectsDictionary(json, pos, config, objectFiles);
                    } else {
                        // Old array format
                        LOG_INFO("ConfigLoader", "Parsing objects array (old format)...");
                        if (ExpectToken(json, pos, '[')) {
                        int objCount = 0;
                        while (pos < json.length() && json[pos] != ']') {
                            objCount++;
                            LOG_INFO_FMT("ConfigLoader", "Parsing object %d...", objCount);
                            ObjectConfig obj;
                            if (ParseObjectConfig(json, pos, obj)) {
                                config.objects.push_back(obj);
                                LOG_INFO_FMT("ConfigLoader", "Object %d parsed: '%s'", objCount, obj.name.c_str());
                            } else {
                                LOG_ERROR_FMT("ConfigLoader", "Failed to parse object %d", objCount);
                            }
                            SkipWhitespace(json, pos);
                            if (pos < json.length() && json[pos] == ',') {
                                pos++;
                            }
                            // Safety check to prevent infinite loop
                            if (pos >= json.length()) {
                                LOG_ERROR("ConfigLoader", "Reached end of JSON while parsing objects");
                                break;
                            }
                            if (objCount > 100) {
                                LOG_ERROR("ConfigLoader", "Too many objects, breaking");
                                break;
                            }
                        }
                        ExpectToken(json, pos, ']');
                        LOG_INFO_FMT("ConfigLoader", "Parsed %zu objects", config.objects.size());
                        }
                    }
                } else {
                    LOG_INFO_FMT("ConfigLoader", "Unknown key: '%s', skipping...", key.c_str());
                }
            } catch (const std::exception& e) {
                LOG_ERROR_FMT("ConfigLoader", "Exception parsing key '%s': %s", key.c_str(), e.what());
                // Error parsing this key, skip it and continue
                // Find next comma or closing brace
                while (pos < json.length() && json[pos] != ',' && json[pos] != '}') {
                    pos++;
                }
            } catch (...) {
                LOG_ERROR_FMT("ConfigLoader", "Unknown exception parsing key '%s'", key.c_str());
                // Unknown error, skip this key
                while (pos < json.length() && json[pos] != ',' && json[pos] != '}') {
                    pos++;
                }
            }
            
            SkipWhitespace(json, pos);
            if (pos < json.length() && json[pos] == ',') {
                pos++;
            }
        }
        
        debugLog << "[ParseJsonFile] JSON parsing completed successfully" << std::endl;
        debugLog.flush();
        debugLog.close();
        return true;
    } catch (const std::exception& e) {
        debugLog << "[ParseJsonFile] Exception during parsing: " << e.what() << std::endl;
        debugLog.flush();
        debugLog.close();
        return false;
    } catch (...) {
        debugLog << "[ParseJsonFile] Unknown exception during parsing" << std::endl;
        debugLog.flush();
        debugLog.close();
        return false;
    }
}

bool ConfigLoader::LoadFromFile(const std::string& filepath, SystemConfig& config) {
    // Write directly to file for debugging (in case logger crashes)
    std::ofstream debugLog("C:\\Users\\inque\\OneDrive\\Desktop\\Work\\games\\thegame\\build\\bin\\Release\\configloader_debug.log", std::ios::app);
    debugLog << "[ConfigLoader] LoadFromFile called with path: " << filepath << std::endl;
    debugLog.flush();
    
    try {
        debugLog << "[ConfigLoader] About to call ParseJsonFile..." << std::endl;
        debugLog.flush();
        
        bool result = ParseJsonFile(filepath, config);
        
        debugLog << "[ConfigLoader] ParseJsonFile returned: " << (result ? 1 : 0) << std::endl;
        debugLog.flush();
        debugLog.close();
        
        return result;
    } catch (const std::exception& e) {
        debugLog << "[ConfigLoader] Exception in LoadFromFile: " << e.what() << std::endl;
        debugLog.flush();
        debugLog.close();
        return false;
    } catch (...) {
        debugLog << "[ConfigLoader] Unknown exception in LoadFromFile" << std::endl;
        debugLog.flush();
        debugLog.close();
        return false;
    }
}

bool ConfigLoader::SaveToFile(const std::string& /*filepath*/, const SystemConfig& /*config*/) {
    // Not implemented - not needed for this use case
    return false;
}

} // namespace CameraSystem
} // namespace Editors
} // namespace Engine

