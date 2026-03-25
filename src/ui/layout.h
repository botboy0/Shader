#pragma once

#include <glad.h>
#include <imgui.h>
#include <string>
#include <vector>

namespace magnetar {

class CodeEditor;  // forward declaration

/// View mode for the editor/preview layout.
enum class ViewMode { Split = 0, EditorOnly = 1, PreviewOnly = 2 };

/// Mouse state from the preview pane, readable by UniformBridge each frame.
struct PreviewMouseState {
    float x = 0.0f;          ///< Shader pixel-space X (0 = left)
    float y = 0.0f;          ///< Shader pixel-space Y (0 = bottom, Y-up)
    bool  inPreview = false;  ///< True if cursor is over the preview image
    bool  buttonDown = false; ///< True while left button is held
    bool  clicked = false;    ///< True for exactly one frame on press
};

/// Result of tab bar interaction each frame.
struct TabBarResult {
    int  clickedTab = -1;    ///< Index of tab user clicked to switch to (-1 = none)
    bool requestClose = false; ///< True if user clicked a tab close button
    int  closedTab = -1;     ///< Index of tab user requested to close (-1 = none)
    bool requestNewTab = false; ///< True if user clicked the "+" button
};

/// Manages the ImGui dockspace layout: Editor (left) and Preview (right) panes,
/// plus a status overlay for FPS/vsync/cap.  Uses DockBuilder on the first frame
/// to establish the initial split, then lets the user rearrange freely.
class Layout {
public:
    Layout() = default;
    ~Layout() = default;

    Layout(const Layout&) = delete;
    Layout& operator=(const Layout&) = delete;

    /// Set up the dockspace over the full viewport.
    /// On the first call, splits into Editor (left, ~40%) and Preview (right, ~60%).
    void beginFrame();

    /// Render the Editor pane with the code editor widget.
    /// If editor is null, shows placeholder text (fallback).
    void renderEditorPane(CodeEditor* editor = nullptr, ImFont* codeFont = nullptr);

    /// Render the Preview pane showing the shader FBO texture via ImGui::Image().
    void renderPreviewPane(GLuint textureId, int texWidth, int texHeight);

    /// Render a status overlay showing FPS, vsync state, and framerate cap.
    /// settingsClicked is set to true if the user clicks the settings button.
    void renderStatusBar(float fps, bool vsync, int cap, bool* settingsClicked = nullptr);

    /// Render the tab bar with given tab names and dirty flags.
    /// Returns interaction result (which tab was clicked, close requests, new tab).
    TabBarResult renderTabBar(const std::vector<std::string>& tabNames,
                              const std::vector<bool>& dirtyFlags,
                              int activeTab);

    /// Set the view mode. Triggers dock layout rebuild on next frame.
    void setViewMode(ViewMode mode);

    /// Get the current view mode.
    ViewMode getViewMode() const { return m_viewMode; }

    /// Cycle through Split → EditorOnly → PreviewOnly → Split.
    void cycleViewMode();

    /// Last-known Preview pane content size (for resizing the renderer FBO).
    int previewWidth()  const { return m_previewWidth; }
    int previewHeight() const { return m_previewHeight; }

    /// Current mouse state in the preview pane (shader pixel-space coords).
    const PreviewMouseState& getMouseState() const { return m_mouseState; }

private:
    void applyDockLayout();

    bool       m_layoutApplied = false;
    ImGuiID    m_dockspaceId   = 0;

    int        m_previewWidth  = 0;
    int        m_previewHeight = 0;

    ViewMode   m_viewMode = ViewMode::Split;
    ViewMode   m_appliedViewMode = ViewMode::Split; // last mode applied to dock layout
    bool       m_viewModeChanged = false;

    PreviewMouseState m_mouseState;
};

} // namespace magnetar
