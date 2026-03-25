#include "layout.h"

#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>  // DockBuilder APIs
#include "editor/code_editor.h"

namespace magnetar {

// ─── Dock layout ─────────────────────────────────────────────────────────────

void Layout::applyDockLayout()
{
    ImGui::DockBuilderRemoveNodeChildNodes(m_dockspaceId);

    switch (m_viewMode) {
    case ViewMode::EditorOnly:
        ImGui::DockBuilderDockWindow("Editor",  m_dockspaceId);
        break;
    case ViewMode::PreviewOnly:
        ImGui::DockBuilderDockWindow("Preview", m_dockspaceId);
        break;
    case ViewMode::Split:
    default: {
        ImGuiID leftId  = 0;
        ImGuiID rightId = 0;
        ImGui::DockBuilderSplitNode(m_dockspaceId, ImGuiDir_Left, 0.4f, &leftId, &rightId);
        ImGui::DockBuilderDockWindow("Editor",  leftId);
        ImGui::DockBuilderDockWindow("Preview", rightId);
        break;
    }
    }

    ImGui::DockBuilderFinish(m_dockspaceId);
    m_appliedViewMode = m_viewMode;

    const char* modeStr = m_viewMode == ViewMode::Split      ? "Split"
                        : m_viewMode == ViewMode::EditorOnly ? "EditorOnly"
                        :                                      "PreviewOnly";
    fprintf(stderr, "[Layout] DockSpace layout applied: %s\n", modeStr);
}

void Layout::beginFrame()
{
    // Create a dockspace that covers the entire viewport
    m_dockspaceId = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(),
                                                  ImGuiDockNodeFlags_PassthruCentralNode);

    // Apply initial layout once, or rebuild when view mode changes
    if (!m_layoutApplied || m_viewModeChanged) {
        m_layoutApplied = true;
        m_viewModeChanged = false;
        applyDockLayout();
    }
}

// ─── View mode ───────────────────────────────────────────────────────────────

void Layout::setViewMode(ViewMode mode)
{
    if (mode == m_viewMode) return;
    m_viewMode = mode;
    m_viewModeChanged = true;
    fprintf(stderr, "[Layout] ViewMode set to %d\n", static_cast<int>(mode));
}

void Layout::cycleViewMode()
{
    int next = (static_cast<int>(m_viewMode) + 1) % 3;
    setViewMode(static_cast<ViewMode>(next));
}

// ─── Tab bar ─────────────────────────────────────────────────────────────────

TabBarResult Layout::renderTabBar(const std::vector<std::string>& tabNames,
                                   const std::vector<bool>& dirtyFlags,
                                   int activeTab)
{
    TabBarResult result;

    ImGuiTabBarFlags barFlags = ImGuiTabBarFlags_Reorderable
                              | ImGuiTabBarFlags_AutoSelectNewTabs
                              | ImGuiTabBarFlags_FittingPolicyScroll
                              | ImGuiTabBarFlags_TabListPopupButton;

    if (ImGui::BeginTabBar("##ShaderTabs", barFlags)) {
        // "+" button for new tab
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) {
            result.requestNewTab = true;
        }

        for (int i = 0; i < static_cast<int>(tabNames.size()); i++) {
            ImGuiTabItemFlags itemFlags = ImGuiTabItemFlags_None;
            if (i < static_cast<int>(dirtyFlags.size()) && dirtyFlags[i]) {
                itemFlags |= ImGuiTabItemFlags_UnsavedDocument;
            }

            bool open = true;
            // Use a stable ID: tab index + name
            std::string label = tabNames[i] + "###tab" + std::to_string(i);
            if (ImGui::BeginTabItem(label.c_str(), &open, itemFlags)) {
                // This tab is the currently visible/selected one in ImGui
                if (i != activeTab) {
                    result.clickedTab = i;
                }
                ImGui::EndTabItem();
            }

            if (!open) {
                result.requestClose = true;
                result.closedTab = i;
            }
        }

        ImGui::EndTabBar();
    }

    return result;
}

// ─── Editor pane ─────────────────────────────────────────────────────────────

void Layout::renderEditorPane(CodeEditor* editor, ImFont* codeFont)
{
    // In PreviewOnly mode, don't render the editor window at all
    if (m_viewMode == ViewMode::PreviewOnly) return;

    ImGui::Begin("Editor");
    if (editor) {
        if (codeFont) ImGui::PushFont(codeFont);
        editor->render();
        if (codeFont) ImGui::PopFont();
    } else {
        ImGui::TextWrapped("No active editor tab");
    }
    ImGui::End();
}

// ─── Preview pane ────────────────────────────────────────────────────────────

void Layout::renderPreviewPane(GLuint textureId, int texWidth, int texHeight)
{
    // In EditorOnly mode, don't render the preview window at all
    if (m_viewMode == ViewMode::EditorOnly) {
        // Keep reasonable fallback dimensions so FBO doesn't go to zero
        if (m_previewWidth  < 1) m_previewWidth  = 1;
        if (m_previewHeight < 1) m_previewHeight = 1;
        return;
    }

    ImGui::Begin("Preview");

    ImVec2 avail = ImGui::GetContentRegionAvail();
    int pw = static_cast<int>(avail.x);
    int ph = static_cast<int>(avail.y);

    // Clamp to reasonable minimums to avoid zero-size FBO
    if (pw < 1) pw = 1;
    if (ph < 1) ph = 1;

    m_previewWidth  = pw;
    m_previewHeight = ph;

    if (textureId != 0 && texWidth > 0 && texHeight > 0) {
        // OpenGL textures have bottom-left origin; flip UVs vertically for ImGui
        ImGui::Image(
            static_cast<ImTextureID>(static_cast<uintptr_t>(textureId)),
            avail,
            ImVec2(0.0f, 1.0f),  // uv0: top-left maps to bottom-left of texture
            ImVec2(1.0f, 0.0f)   // uv1: bottom-right maps to top-right of texture
        );

        // --- Mouse tracking for iMouse uniform ---
        // Reset per-frame click state before checking
        m_mouseState.clicked = false;

        if (ImGui::IsItemHovered()) {
            ImVec2 mousePos = ImGui::GetMousePos();
            ImVec2 rectMin  = ImGui::GetItemRectMin();
            ImVec2 rectMax  = ImGui::GetItemRectMax();
            float paneW = rectMax.x - rectMin.x;
            float paneH = rectMax.y - rectMin.y;

            // Convert to shader pixel coords (0,0 = bottom-left, Y increases upward)
            float mx = mousePos.x - rectMin.x;
            float my = paneH - (mousePos.y - rectMin.y);  // flip Y

            // Scale to FBO resolution if pane size differs from FBO size
            if (paneW > 0.0f && paneH > 0.0f) {
                mx *= static_cast<float>(texWidth)  / paneW;
                my *= static_cast<float>(texHeight) / paneH;
            }

            m_mouseState.x = mx;
            m_mouseState.y = my;
            m_mouseState.inPreview = true;
            m_mouseState.buttonDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);

            if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                m_mouseState.clicked = true;
            }
        } else {
            m_mouseState.inPreview = false;
            m_mouseState.buttonDown = false;
            // x/y intentionally retain last values — iMouse.xy holds last known position
        }
    }

    ImGui::End();
}

// ─── Status bar ──────────────────────────────────────────────────────────────

void Layout::renderStatusBar(float fps, bool vsync, int cap, bool* settingsClicked)
{
    const float PAD = 10.0f;

    // Position overlay in the top-right corner of the viewport
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 workPos  = viewport->WorkPos;
    ImVec2 workSize = viewport->WorkSize;

    ImGui::SetNextWindowPos(
        ImVec2(workPos.x + workSize.x - PAD, workPos.y + PAD),
        ImGuiCond_Always,
        ImVec2(1.0f, 0.0f)  // anchor: top-right
    );
    ImGui::SetNextWindowBgAlpha(0.5f);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                           | ImGuiWindowFlags_NoDocking
                           | ImGuiWindowFlags_AlwaysAutoResize
                           | ImGuiWindowFlags_NoSavedSettings
                           | ImGuiWindowFlags_NoFocusOnAppearing
                           | ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("##StatusOverlay", nullptr, flags)) {
        ImGui::Text("%.1f FPS", fps);
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        ImGui::Text("vsync: %s", vsync ? "on" : "off");
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        if (cap > 0)
            ImGui::Text("cap: %d", cap);
        else
            ImGui::Text("cap: off");

        // View mode indicator
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        const char* modeStr = m_viewMode == ViewMode::Split      ? "Split"
                            : m_viewMode == ViewMode::EditorOnly ? "Editor"
                            :                                      "Preview";
        ImGui::Text("F6:%s", modeStr);

        // Settings gear button
        if (settingsClicked) {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            if (ImGui::SmallButton("Settings")) {
                *settingsClicked = true;
            }
        }
    }
    ImGui::End();
}

} // namespace magnetar
