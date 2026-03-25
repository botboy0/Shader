#pragma once

#include <SDL3/SDL.h>
#include "theme.h"

namespace magnetar {

/// Manages the Dear ImGui lifecycle: context creation, backend initialization,
/// per-frame begin/end, and shutdown.  Wraps SDL3 + OpenGL3 backends.
/// Owns the Theme (Magnetar palette + fonts) and exposes it for downstream use.
class ImGuiManager {
public:
    ImGuiManager() = default;
    ~ImGuiManager();

    ImGuiManager(const ImGuiManager&) = delete;
    ImGuiManager& operator=(const ImGuiManager&) = delete;

    /// Create ImGui context, enable docking, initialize SDL3+OpenGL3 backends,
    /// load fonts, and apply the Magnetar theme.
    /// Returns true on success.  Logs "[ImGui] ..." to stderr.
    bool init(SDL_Window* window, SDL_GLContext glCtx);

    /// Shut down backends and destroy the ImGui context.
    void shutdown();

    /// Start a new ImGui frame (call once per frame, after event processing).
    void beginFrame();

    /// Finalize and render ImGui draw data (call once per frame, after all UI code).
    void endFrame();

    bool isInitialized() const { return m_initialized; }

    /// Access the Magnetar theme (palette + fonts).
    /// Valid after init() returns true.
    Theme& theme() { return m_theme; }
    const Theme& theme() const { return m_theme; }

private:
    bool  m_initialized = false;
    Theme m_theme;
};

} // namespace magnetar
