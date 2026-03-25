#include "frame_control.h"

#include <cstdio>
#include <algorithm>

namespace magnetar {

FrameControl::FrameControl()
{
    m_perfFrequency = SDL_GetPerformanceFrequency();
    m_frameStart    = SDL_GetPerformanceCounter();
    m_lastFrameStart = m_frameStart;
    fprintf(stderr, "[FrameControl] Initialized (vsync=ON, cap=%d, freq=%llu)\n",
            m_framerateCap, (unsigned long long)m_perfFrequency);
}

void FrameControl::beginFrame()
{
    Uint64 now = SDL_GetPerformanceCounter();

    // Record wall-clock time since last beginFrame — this captures the full
    // frame interval including any delay or vsync wait from the previous frame.
    if (m_lastFrameStart > 0) {
        double frameInterval = static_cast<double>(now - m_lastFrameStart)
                             / static_cast<double>(m_perfFrequency);
        m_frameTimes[m_frameIndex] = frameInterval;
        m_frameIndex = (m_frameIndex + 1) % kHistorySize;
        if (m_frameCount < kHistorySize) {
            m_frameCount++;
        }
    }
    m_lastFrameStart = now;

    m_frameStart = now;
}

void FrameControl::endFrame()
{
    // Apply framerate cap when vsync is off and cap > 0.
    // SDL_DelayPrecise uses hybrid sleep+spin for sub-millisecond accuracy.
    if (!m_vsync && m_framerateCap > 0) {
        double targetDuration = 1.0 / static_cast<double>(m_framerateCap);
        Uint64 now = SDL_GetPerformanceCounter();
        double elapsed = static_cast<double>(now - m_frameStart)
                       / static_cast<double>(m_perfFrequency);
        double remaining = targetDuration - elapsed;
        if (remaining > 0.0) {
            Uint64 delayNs = static_cast<Uint64>(remaining * 1e9);
            SDL_DelayPrecise(delayNs);
        }
    }
}

float FrameControl::getFps() const
{
    if (m_frameCount == 0) return 0.0f;

    double sum = 0.0;
    for (int i = 0; i < m_frameCount; ++i) {
        sum += m_frameTimes[i];
    }
    double avgFrameTime = sum / static_cast<double>(m_frameCount);
    if (avgFrameTime <= 0.0) return 0.0f;

    return static_cast<float>(1.0 / avgFrameTime);
}

void FrameControl::toggleVsync()
{
    m_vsync = !m_vsync;
    if (m_vsync) {
        SDL_GL_SetSwapInterval(1);
    } else {
        SDL_GL_SetSwapInterval(0);
    }
    fprintf(stderr, "[FrameControl] Vsync toggled: %s\n", m_vsync ? "ON" : "OFF");
}

void FrameControl::setFramerateCap(int fps)
{
    if (fps <= 0) {
        m_framerateCap = 0;
    } else {
        m_framerateCap = std::clamp(fps, 30, 240);
    }
    fprintf(stderr, "[FrameControl] Framerate cap set: %d\n", m_framerateCap);
}

void FrameControl::adjustFramerateCap(int delta)
{
    int newCap = m_framerateCap + delta;
    if (newCap < 30) {
        m_framerateCap = 0; // snap to uncapped
    } else {
        m_framerateCap = std::clamp(newCap, 30, 240);
    }
    fprintf(stderr, "[FrameControl] Framerate cap adjusted: ");
    if (m_framerateCap > 0) {
        fprintf(stderr, "%d\n", m_framerateCap);
    } else {
        fprintf(stderr, "uncapped\n");
    }
}

} // namespace magnetar
