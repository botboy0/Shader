#pragma once

#include <SDL3/SDL.h>

namespace magnetar {

/// Manages frame timing: FPS measurement (rolling average), vsync toggle, and framerate cap.
/// Display method is separate — in S01 we write to window title; in S02 this feeds an ImGui overlay.
class FrameControl {
public:
    FrameControl();
    ~FrameControl() = default;

    FrameControl(const FrameControl&) = delete;
    FrameControl& operator=(const FrameControl&) = delete;

    /// Call at the top of each frame (before update/render).
    void beginFrame();

    /// Call at the bottom of each frame (after swap).
    /// Applies framerate cap (SDL_Delay) when vsync is off and cap > 0.
    void endFrame();

    /// Returns rolling average FPS over the last 60 frames.
    float getFps() const;

    /// Toggle vsync on/off. Logs state change to stderr with [FrameControl] prefix.
    void toggleVsync();

    /// Returns true if vsync is currently enabled.
    bool isVsyncEnabled() const { return m_vsync; }

    /// Set target max FPS (0 = uncapped). Ignored when vsync is on.
    void setFramerateCap(int fps);

    /// Get current framerate cap (0 = uncapped).
    int getFramerateCap() const { return m_framerateCap; }

    /// Adjust framerate cap by delta. Clamps to 30–240 range, or snaps to 0 (uncapped)
    /// if going below 30.
    void adjustFramerateCap(int delta);

private:
    static constexpr int kHistorySize = 60;

    bool   m_vsync        = true;
    int    m_framerateCap  = 60;

    Uint64 m_perfFrequency = 0;
    Uint64 m_frameStart    = 0;
    Uint64 m_lastFrameStart = 0;  // for measuring wall-clock frame interval

    // Circular buffer of frame durations (in seconds)
    double m_frameTimes[kHistorySize] = {};
    int    m_frameIndex   = 0;
    int    m_frameCount   = 0; // how many frames recorded so far (up to kHistorySize)
};

} // namespace magnetar
