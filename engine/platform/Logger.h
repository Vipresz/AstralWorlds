#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <cstdio>
#include <ctime>

// Don't include windows.h here to avoid macro conflicts (ERROR, WARNING, DEBUG)
// Use C standard library functions instead

// Protect against Windows macros that might be defined by other headers
#ifdef _WIN32
    #ifdef ERROR
        #undef ERROR
    #endif
    #ifdef WARNING
        #undef WARNING
    #endif
    #ifdef DEBUG
        #undef DEBUG
    #endif
#endif

// Logger in its own namespace to avoid conflicts with Engine namespace
namespace EngineLogging {

enum class LogLevel {
    LevelDebug = 0,
    LevelInfo = 1,
    LevelWarning = 2,
    LevelError = 3
};

class Logger {
public:
    static Logger& GetInstance() {
        static Logger instance;
        return instance;
    }
    
    // Initialize logger with log file path
    bool Initialize(const std::string& logFilePath = "debug.log") {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            
            if (m_logFile.is_open()) {
                m_logFile.close();
            }
            
            m_logFile.open(logFilePath, std::ios::out | std::ios::trunc);
            if (!m_logFile.is_open()) {
                std::cerr << "Failed to open log file: " << logFilePath << std::endl;
                return false;
            }
            
            m_logFilePath = logFilePath;
            
            // Write a simple test message directly to file first
            try {
                m_logFile << "=== LOGGER INITIALIZED ===" << std::endl;
                m_logFile.flush();
            } catch (...) {
                // Ignore - will try again with Log()
            }
        } // Release lock before calling Log()
        
        // Now try using Log() method (outside the lock to avoid deadlock)
        Log(LogLevel::LevelInfo, "Logger", "Logger initialized. Log file: " + logFilePath);
        return true;
    }
    
    void SetMinLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_minLevel = level;
    }
    
    void Log(LogLevel level, const std::string& category, const std::string& message) {
        try {
            if (level < m_minLevel) {
                return;
            }
            
            std::lock_guard<std::mutex> lock(m_mutex);
            
            // Format timestamp (with error handling)
            std::string timestamp = "TIMESTAMP_ERROR";
            try {
                auto now = std::chrono::system_clock::now();
                auto time = std::chrono::system_clock::to_time_t(now);
                auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()) % 1000;
                
                // Format timestamp (cross-platform, thread-safe by copying)
                struct tm timeinfo;
                #ifdef _WIN32
                    // Windows: localtime_s is in time.h (C11 standard), not windows.h
                    if (localtime_s(&timeinfo, &time) == 0) {
                        std::stringstream ss;
                        ss << std::setfill('0') << std::setw(4) << (timeinfo.tm_year + 1900) << "-"
                           << std::setw(2) << (timeinfo.tm_mon + 1) << "-"
                           << std::setw(2) << timeinfo.tm_mday << " "
                           << std::setw(2) << timeinfo.tm_hour << ":"
                           << std::setw(2) << timeinfo.tm_min << ":"
                           << std::setw(2) << timeinfo.tm_sec
                           << '.' << std::setfill('0') << std::setw(3) << ms.count();
                        timestamp = ss.str();
                    }
                #else
                    // Unix: use localtime_r (thread-safe)
                    if (localtime_r(&time, &timeinfo) != nullptr) {
                        std::stringstream ss;
                        ss << std::setfill('0') << std::setw(4) << (timeinfo.tm_year + 1900) << "-"
                           << std::setw(2) << (timeinfo.tm_mon + 1) << "-"
                           << std::setw(2) << timeinfo.tm_mday << " "
                           << std::setw(2) << timeinfo.tm_hour << ":"
                           << std::setw(2) << timeinfo.tm_min << ":"
                           << std::setw(2) << timeinfo.tm_sec
                           << '.' << std::setfill('0') << std::setw(3) << ms.count();
                        timestamp = ss.str();
                    }
                #endif
            } catch (...) {
                // Timestamp failed, use error marker
                timestamp = "TIMESTAMP_ERROR";
            }
            
            // Format log message
            std::string levelStr = LevelToString(level);
            std::string logMessage = "[" + timestamp + "] [" + levelStr + "] [" + category + "] " + message;
            
            // Write to file (with error handling)
            if (m_logFile.is_open()) {
                try {
                    m_logFile << logMessage << std::endl;
                    m_logFile.flush(); // Ensure it's written immediately
                } catch (...) {
                    // File write failed, continue to console output
                }
            }
            
            // Also write to console (always flush immediately for debugging)
            try {
                std::ostream& out = (level >= LogLevel::LevelError) ? std::cerr : std::cout;
                out << logMessage << std::endl;
                out.flush(); // Always flush immediately for real-time debugging
            } catch (...) {
                // Console output failed - nothing we can do
            }
        } catch (...) {
            // If logging itself fails, write a minimal error to stderr
            try {
                std::cerr << "[LOGGER ERROR] Failed to log message: " << message << std::endl;
                std::cerr.flush();
            } catch (...) {
                // Even error logging failed - catastrophic failure
            }
        }
    }
    
    void Shutdown() {
        // Log before acquiring the lock to avoid deadlock
        Log(LogLevel::LevelInfo, "Logger", "Logger shutting down");
        
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_logFile.is_open()) {
            m_logFile.close();
        }
    }
    
    ~Logger() {
        Shutdown();
    }

private:
    Logger() : m_minLevel(LogLevel::LevelDebug) {}
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::string LevelToString(LogLevel level) {
        switch (level) {
            case LogLevel::LevelDebug: return "DEBUG";
            case LogLevel::LevelInfo: return "INFO ";
            case LogLevel::LevelWarning: return "WARN ";
            case LogLevel::LevelError: return "ERROR";
            default: return "UNKNOWN";
        }
    }
    
    std::ofstream m_logFile;
    std::string m_logFilePath;
    LogLevel m_minLevel;
    std::mutex m_mutex;
};

} // namespace EngineLogging

// Convenience macros - use EngineLogging namespace to avoid conflicts
#define LOG_DEBUG(category, message) \
    EngineLogging::Logger::GetInstance().Log(EngineLogging::LogLevel::LevelDebug, category, message)

#define LOG_INFO(category, message) \
    EngineLogging::Logger::GetInstance().Log(EngineLogging::LogLevel::LevelInfo, category, message)

#define LOG_WARNING(category, message) \
    EngineLogging::Logger::GetInstance().Log(EngineLogging::LogLevel::LevelWarning, category, message)

#define LOG_ERROR(category, message) \
    EngineLogging::Logger::GetInstance().Log(EngineLogging::LogLevel::LevelError, category, message)

// Format macros using printf-style formatting (supports variadic arguments)
#ifdef _WIN32
#define LOG_DEBUG_FMT(category, format, ...) \
    do { \
        char _log_buffer[1024]; \
        _snprintf_s(_log_buffer, sizeof(_log_buffer), _TRUNCATE, format, __VA_ARGS__); \
        EngineLogging::Logger::GetInstance().Log(EngineLogging::LogLevel::LevelDebug, category, _log_buffer); \
    } while(0)

#define LOG_INFO_FMT(category, format, ...) \
    do { \
        char _log_buffer[1024]; \
        _snprintf_s(_log_buffer, sizeof(_log_buffer), _TRUNCATE, format, __VA_ARGS__); \
        EngineLogging::Logger::GetInstance().Log(EngineLogging::LogLevel::LevelInfo, category, _log_buffer); \
    } while(0)

#define LOG_WARNING_FMT(category, format, ...) \
    do { \
        char _log_buffer[1024]; \
        _snprintf_s(_log_buffer, sizeof(_log_buffer), _TRUNCATE, format, __VA_ARGS__); \
        EngineLogging::Logger::GetInstance().Log(EngineLogging::LogLevel::LevelWarning, category, _log_buffer); \
    } while(0)

#define LOG_ERROR_FMT(category, format, ...) \
    do { \
        char _log_buffer[1024]; \
        _snprintf_s(_log_buffer, sizeof(_log_buffer), _TRUNCATE, format, __VA_ARGS__); \
        EngineLogging::Logger::GetInstance().Log(EngineLogging::LogLevel::LevelError, category, _log_buffer); \
    } while(0)
#else
#define LOG_DEBUG_FMT(category, format, ...) \
    do { \
        char _log_buffer[1024]; \
        snprintf(_log_buffer, sizeof(_log_buffer), format, __VA_ARGS__); \
        EngineLogging::Logger::GetInstance().Log(EngineLogging::LogLevel::LevelDebug, category, _log_buffer); \
    } while(0)

#define LOG_INFO_FMT(category, format, ...) \
    do { \
        char _log_buffer[1024]; \
        snprintf(_log_buffer, sizeof(_log_buffer), format, __VA_ARGS__); \
        EngineLogging::Logger::GetInstance().Log(EngineLogging::LogLevel::LevelInfo, category, _log_buffer); \
    } while(0)

#define LOG_WARNING_FMT(category, format, ...) \
    do { \
        char _log_buffer[1024]; \
        snprintf(_log_buffer, sizeof(_log_buffer), format, __VA_ARGS__); \
        EngineLogging::Logger::GetInstance().Log(EngineLogging::LogLevel::LevelWarning, category, _log_buffer); \
    } while(0)

#define LOG_ERROR_FMT(category, format, ...) \
    do { \
        char _log_buffer[1024]; \
        snprintf(_log_buffer, sizeof(_log_buffer), format, __VA_ARGS__); \
        EngineLogging::Logger::GetInstance().Log(EngineLogging::LogLevel::LevelError, category, _log_buffer); \
    } while(0)
#endif
