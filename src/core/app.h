#pragma once

#include <functional>
#include <string>

#include <SDL3/SDL.h>
#include "frame_control.h"

namespace magnetar {

/// Owns the SDL3 window, OpenGL 4.5 context, and main event loop.
/// Designed for callback-based update so ImGui hooks can be inserted in S02.
class App {
public:
    using UpdateCallback = std::function<void()>;
    using ResizeCallback = std::function<void(int width, int height)>;
    using EventCallback  = std::function<bool(const SDL_Event&)>;

    App(const std::string& title, int width, int height);
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    /// Initialize SDL, create window + GL context, load GL functions.
    /// Returns true on success, false on failure (errors logged to stderr).
    bool init();

    /// Enter the main loop. Calls onUpdate each frame, swaps buffers.
    /// Returns when the window is closed or an error occurs.
    void run();

    /// Tear down GL context, window, and SDL.
    void shutdown();

    /// Set the per-frame update callback (called between clear and swap).
    void setUpdateCallback(UpdateCallback cb) { m_updateCallback = std::move(cb); }

    /// Set the resize callback (called when window dimensions change).
    void setResizeCallback(ResizeCallback cb) { m_resizeCallback = std::move(cb); }

    /// Set the event callback (called for every SDL event before internal handling).
    /// Used by ImGui to forward events to its backend.
    void setEventCallback(EventCallback cb) { m_eventCallback = std::move(cb); }

    int width() const { return m_width; }
    int height() const { return m_height; }
    SDL_Window* window() const { return m_window; }

    /// Request a clean quit (e.g. from Ctrl+Q via ShortcutManager).
    void requestQuit() { m_running = false; }

    /// Check if the app is still running.
    bool isRunning() const { return m_running; }

    /// Get elapsed time in seconds since init() (high-precision).
    float getElapsedTime() const;

    /// Access the frame control for FPS, vsync, framerate cap.
    FrameControl& frameControl() { return m_frameControl; }
    const FrameControl& frameControl() const { return m_frameControl; }

private:
    void processEvents();
    void updateWindowTitle();

    std::string    m_title;
    int            m_width;
    int            m_height;
    bool           m_running = false;

    SDL_Window*    m_window  = nullptr;
    SDL_GLContext  m_glCtx   = nullptr;

    Uint64         m_startCounter = 0;
    Uint64         m_perfFrequency = 0;

    FrameControl   m_frameControl;
    Uint64         m_lastTitleUpdate = 0; // last time window title was updated

    UpdateCallback m_updateCallback;
    ResizeCallback m_resizeCallback;
    EventCallback  m_eventCallback;
};

} // namespace magnetar
