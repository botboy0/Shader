#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <SDL3/SDL.h>

namespace magnetar {

/// Thread-safe wrapper around SDL3's native file open/save dialogs.
/// Callbacks may fire from a different thread, so the callback writes
/// a result into a mutex-protected optional. The main thread polls
/// each frame with pollResult().
class FileDialog {
public:
    FileDialog() = default;
    ~FileDialog() = default;

    FileDialog(const FileDialog&) = delete;
    FileDialog& operator=(const FileDialog&) = delete;

    struct Result {
        bool isSave = false;
        std::string path;
    };

    /// Open a native "Open File" dialog. Result arrives asynchronously.
    void openFile(SDL_Window* window, const std::string& lastDir);

    /// Open a native "Save File" dialog. Result arrives asynchronously.
    void saveFile(SDL_Window* window, const std::string& defaultName, const std::string& lastDir);

    /// True if a dialog result is pending (user picked or cancelled).
    bool hasResult() const;

    /// Return the pending result and clear it. Only valid when hasResult() is true.
    Result pollResult();

private:
    static void openCallback(void* userdata, const char* const* filelist, int filter);
    static void saveCallback(void* userdata, const char* const* filelist, int filter);

    mutable std::mutex m_mutex;
    std::optional<Result> m_result;
};

} // namespace magnetar
