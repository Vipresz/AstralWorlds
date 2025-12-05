#pragma once

#include "core/time/Timestep.h"
#include <functional>
#include <atomic>
#include <cstdint>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Engine {
namespace Time {

// Main game loop with fixed timestep for deterministic simulation
class GameLoop {
public:
    using TickCallback = std::function<void(float deltaTime, uint64_t tickNumber)>;
    using RenderCallback = std::function<void(float interpolationAlpha)>;
    
    GameLoop()
        : m_timestep(FIXED_DELTA_TIME)
        , m_running(false)
        , m_targetFPS(0) // 0 = uncapped
    {}
    
    void SetTickCallback(TickCallback callback) {
        m_tickCallback = callback;
    }
    
    void SetRenderCallback(RenderCallback callback) {
        m_renderCallback = callback;
    }
    
    void SetTargetFPS(uint32_t fps) {
        m_targetFPS = fps;
    }
    
    void Start() {
        m_running = true;
        m_timer.Reset();
        m_lastFrameTime = m_timer.ElapsedSeconds();
        
        while (m_running) {
            float currentTime = m_timer.ElapsedSeconds();
            float frameTime = currentTime - m_lastFrameTime;
            m_lastFrameTime = currentTime;
            
            // Clamp frame time to prevent spiral of death
            if (frameTime > 0.25f) {
                frameTime = 0.25f;
            }
            
            // Process fixed timestep ticks
            uint32_t ticks = m_timestep.Update(frameTime);
            for (uint32_t i = 0; i < ticks; ++i) {
                if (m_tickCallback) {
                    m_tickCallback(m_timestep.GetFixedDeltaTime(), m_timestep.GetTickCount());
                }
            }
            
            // Render with interpolation
            if (m_renderCallback) {
                m_renderCallback(m_timestep.GetInterpolationAlpha());
            }
            
            // Frame rate limiting
            if (m_targetFPS > 0) {
                float targetFrameTime = 1.0f / m_targetFPS;
                float elapsed = m_timer.ElapsedSeconds() - currentTime;
                if (elapsed < targetFrameTime) {
                    // Sleep for remaining time
                    float sleepTime = targetFrameTime - elapsed;
                    Sleep(static_cast<uint32_t>(sleepTime * 1000.0f));
                }
            }
        }
    }
    
    void Stop() {
        m_running = false;
    }
    
    bool IsRunning() const {
        return m_running;
    }
    
    uint64_t GetTickCount() const {
        return m_timestep.GetTickCount();
    }
    
private:
    FixedTimestep m_timestep;
    Timer m_timer;
    TickCallback m_tickCallback;
    RenderCallback m_renderCallback;
    std::atomic<bool> m_running;
    uint32_t m_targetFPS;
    float m_lastFrameTime;
    
    void Sleep(uint32_t milliseconds) {
        // Platform-specific sleep
        #ifdef _WIN32
            ::Sleep(milliseconds);
        #else
            std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
        #endif
    }
};

} // namespace Time
} // namespace Engine

