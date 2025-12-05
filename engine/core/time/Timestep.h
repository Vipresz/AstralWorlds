#pragma once

#include "core/Types.h"
#include <chrono>
#include <cstdint>

namespace Engine {
namespace Time {

// High-resolution timer for frame timing
class Timer {
public:
    Timer() {
        Reset();
    }
    
    void Reset() {
        m_startTime = std::chrono::high_resolution_clock::now();
    }
    
    float ElapsedSeconds() const {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - m_startTime);
        return duration.count() / 1000000.0f;
    }
    
    uint64_t ElapsedMicroseconds() const {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - m_startTime);
        return duration.count();
    }
    
private:
    std::chrono::high_resolution_clock::time_point m_startTime;
};

// Fixed timestep accumulator for deterministic simulation
class FixedTimestep {
public:
    FixedTimestep(float fixedDeltaTime = FIXED_DELTA_TIME)
        : m_fixedDeltaTime(fixedDeltaTime)
        , m_accumulator(0.0f)
        , m_tickCount(0)
    {}
    
    // Update with variable frame time, returns number of ticks to process
    uint32_t Update(float frameTime) {
        m_accumulator += frameTime;
        
        uint32_t ticks = 0;
        while (m_accumulator >= m_fixedDeltaTime && ticks < MAX_TICKS_PER_FRAME) {
            m_accumulator -= m_fixedDeltaTime;
            m_tickCount++;
            ticks++;
        }
        
        return ticks;
    }
    
    float GetFixedDeltaTime() const { return m_fixedDeltaTime; }
    float GetAccumulator() const { return m_accumulator; }
    float GetInterpolationAlpha() const { return m_accumulator / m_fixedDeltaTime; }
    uint64_t GetTickCount() const { return m_tickCount; }
    
    // Reset tick count (useful for network sync)
    void ResetTickCount() { m_tickCount = 0; }
    
private:
    float m_fixedDeltaTime;
    float m_accumulator;
    uint64_t m_tickCount; // Total ticks since start (for deterministic sync)
};

} // namespace Time
} // namespace Engine

