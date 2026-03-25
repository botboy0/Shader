#pragma once

struct ImFont;

namespace magnetar {

/// Manages the Magnetar visual identity: CI color palette, style vars, and
/// embedded font loading (Space Mono for UI, 0xProto for code).
class Theme {
public:
    Theme() = default;
    ~Theme() = default;

    /// Load Space Mono (UI, default) and 0xProto (code) into the ImGui font atlas.
    /// Must be called after ImGui::CreateContext() and before the first NewFrame().
    /// On failure, falls back to ImGui's built-in font with a warning log.
    void loadFonts(float uiSize = 16.0f, float codeSize = 15.0f);

    /// Apply the full Magnetar palette and style vars to ImGuiStyle.
    /// Logs "[Theme] Magnetar theme applied" to stderr.
    void apply();

    /// Code font pointer (0xProto) — use with ImGui::PushFont/PopFont.
    /// Returns nullptr if fonts weren't loaded (falls back to default).
    ImFont* getCodeFont() const { return m_codeFont; }

    /// UI font pointer (Space Mono) — set as the default font.
    ImFont* getUiFont() const { return m_uiFont; }

private:
    ImFont* m_uiFont   = nullptr;
    ImFont* m_codeFont = nullptr;
};

} // namespace magnetar
