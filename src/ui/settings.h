#pragma once

#include <string>

namespace magnetar {

/// Application settings with JSON persistence.
/// Fields are loaded from / saved to a JSON file in SDL_GetPrefPath.
struct Settings {
    bool vsync = true;
    int framerateCap = 60;
    float compileDelay = 0.3f;
    int viewMode = 0; // 0=Split, 1=EditorOnly, 2=PreviewOnly
    std::string lastOpenDir;
    std::string lastSaveDir;

    /// Load settings from a JSON file. On failure, keeps defaults and logs a warning.
    void load(const std::string& path);

    /// Save settings to a JSON file (atomic write via temp file + rename).
    void save(const std::string& path);

    /// Return the default config directory via SDL_GetPrefPath("Magnetar", "ShaderEditor").
    /// Returns empty string if SDL_GetPrefPath fails.
    static std::string getDefaultConfigDir();

    /// Return the full path to the default settings file (configDir + "settings.json").
    static std::string getDefaultPath();

    /// Render an ImGui modal settings dialog. Call every frame.
    /// Pass settingsPath so the dialog can auto-save on close.
    /// Returns true if settings were changed (caller should apply).
    bool renderDialog(const std::string& settingsPath);

    /// Request the settings dialog to open on the next frame.
    void openDialog();

private:
    bool m_dialogOpen = false;
    bool m_shouldOpen = false;
};

} // namespace magnetar
