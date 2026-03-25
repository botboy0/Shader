#include "file_dialog.h"

#include <cstdio>

namespace magnetar {

// File filters used for both open and save dialogs
static const SDL_DialogFileFilter s_filters[] = {
    { "GLSL Shaders", "glsl;frag;vert" },
    { "All Files",    "*" }
};
static constexpr int s_numFilters = sizeof(s_filters) / sizeof(s_filters[0]);

// ─── Static callbacks (may fire from a different thread) ─────────────────────

void FileDialog::openCallback(void* userdata, const char* const* filelist, int /*filter*/)
{
    auto* self = static_cast<FileDialog*>(userdata);

    // filelist == NULL → error; *filelist == NULL → user cancelled
    if (!filelist || !filelist[0]) {
        fprintf(stderr, "[FileDialog] Open cancelled or no file selected\n");
        return;
    }

    std::string path(filelist[0]);
    fprintf(stderr, "[FileDialog] Open: %s\n", path.c_str());

    std::lock_guard<std::mutex> lock(self->m_mutex);
    self->m_result = Result{ false, std::move(path) };
}

void FileDialog::saveCallback(void* userdata, const char* const* filelist, int /*filter*/)
{
    auto* self = static_cast<FileDialog*>(userdata);

    if (!filelist || !filelist[0]) {
        fprintf(stderr, "[FileDialog] Save cancelled or no file selected\n");
        return;
    }

    std::string path(filelist[0]);
    fprintf(stderr, "[FileDialog] Save: %s\n", path.c_str());

    std::lock_guard<std::mutex> lock(self->m_mutex);
    self->m_result = Result{ true, std::move(path) };
}

// ─── Public API ──────────────────────────────────────────────────────────────

void FileDialog::openFile(SDL_Window* window, const std::string& lastDir)
{
    const char* defaultLoc = lastDir.empty() ? nullptr : lastDir.c_str();
    SDL_ShowOpenFileDialog(openCallback, this, window, s_filters, s_numFilters, defaultLoc, false);
    fprintf(stderr, "[FileDialog] Open dialog launched\n");
}

void FileDialog::saveFile(SDL_Window* window, const std::string& defaultName, const std::string& lastDir)
{
    const char* defaultLoc = lastDir.empty() ? nullptr : lastDir.c_str();
    (void)defaultName; // SDL3 save dialog uses default_location, not a separate filename param
    SDL_ShowSaveFileDialog(saveCallback, this, window, s_filters, s_numFilters, defaultLoc);
    fprintf(stderr, "[FileDialog] Save dialog launched\n");
}

bool FileDialog::hasResult() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_result.has_value();
}

FileDialog::Result FileDialog::pollResult()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    Result r = m_result.value_or(Result{});
    m_result.reset();
    return r;
}

} // namespace magnetar
