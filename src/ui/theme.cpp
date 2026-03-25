#include "theme.h"

#include <cstdio>
#include <imgui.h>

#include "fonts/oxproto_regular.h"
#include "fonts/space_mono_regular.h"

namespace magnetar {

void Theme::loadFonts(float uiSize, float codeSize)
{
    ImGuiIO& io = ImGui::GetIO();

    // Font config: data lives in static arrays, ImGui must NOT free them.
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;

    // Space Mono Regular — default UI font (first font added = default)
    m_uiFont = io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(space_mono_regular_data),
        static_cast<int>(space_mono_regular_size),
        uiSize, &cfg);

    if (!m_uiFont) {
        fprintf(stderr, "[Theme] Warning: Failed to load Space Mono, using default font\n");
    }

    // 0xProto Regular — code editor font
    m_codeFont = io.Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(oxproto_regular_data),
        static_cast<int>(oxproto_regular_size),
        codeSize, &cfg);

    if (!m_codeFont) {
        fprintf(stderr, "[Theme] Warning: Failed to load 0xProto, using default font\n");
    }

    // Build the atlas texture
    if (!io.Fonts->Build()) {
        fprintf(stderr, "[Theme] Warning: Font atlas build failed, falling back to default\n");
        io.Fonts->Clear();
        io.Fonts->AddFontDefault();
        io.Fonts->Build();
        m_uiFont   = nullptr;
        m_codeFont = nullptr;
        return;
    }

    fprintf(stderr, "[Theme] Fonts loaded: SpaceMono (UI), 0xProto (code)\n");
}

void Theme::apply()
{
    ImGuiStyle& style = ImGui::GetStyle();

    // ── Magnetar palette ────────────────────────────────────────────────
    // Void black  : #0a0a0f
    // Frame bg    : #111118
    // Violet      : #8b5cf6
    // Cyan        : #06b6d4
    // Magenta     : #d946ef
    // Dust        : #6b7280
    // Off-white   : #f9fafb
    // Border      : #2a2a35

    ImVec4* c = style.Colors;

    // Backgrounds
    c[ImGuiCol_WindowBg]             = ImVec4(0.039f, 0.039f, 0.059f, 1.00f); // #0a0a0f
    c[ImGuiCol_ChildBg]              = ImVec4(0.039f, 0.039f, 0.059f, 1.00f);
    c[ImGuiCol_PopupBg]              = ImVec4(0.055f, 0.055f, 0.075f, 0.96f);
    c[ImGuiCol_FrameBg]              = ImVec4(0.067f, 0.067f, 0.094f, 1.00f); // #111118
    c[ImGuiCol_FrameBgHovered]       = ImVec4(0.090f, 0.090f, 0.120f, 1.00f);
    c[ImGuiCol_FrameBgActive]        = ImVec4(0.110f, 0.110f, 0.150f, 1.00f);

    // Title bar
    c[ImGuiCol_TitleBg]              = ImVec4(0.055f, 0.040f, 0.120f, 1.00f); // dark muted violet
    c[ImGuiCol_TitleBgActive]        = ImVec4(0.545f, 0.361f, 0.965f, 1.00f); // #8b5cf6 violet
    c[ImGuiCol_TitleBgCollapsed]     = ImVec4(0.039f, 0.039f, 0.059f, 0.75f);

    // Tabs
    c[ImGuiCol_Tab]                  = ImVec4(0.130f, 0.090f, 0.280f, 1.00f);
    c[ImGuiCol_TabHovered]           = ImVec4(0.545f, 0.361f, 0.965f, 0.80f); // violet hover
    c[ImGuiCol_TabSelected]          = ImVec4(0.545f, 0.361f, 0.965f, 1.00f); // violet active
    c[ImGuiCol_TabSelectedOverline]  = ImVec4(0.024f, 0.714f, 0.831f, 1.00f); // cyan accent
    c[ImGuiCol_TabDimmed]            = ImVec4(0.080f, 0.060f, 0.160f, 1.00f);
    c[ImGuiCol_TabDimmedSelected]    = ImVec4(0.200f, 0.140f, 0.400f, 1.00f);

    // Buttons
    c[ImGuiCol_Button]               = ImVec4(0.545f, 0.361f, 0.965f, 0.65f); // violet
    c[ImGuiCol_ButtonHovered]        = ImVec4(0.620f, 0.450f, 1.000f, 0.80f); // lighter violet
    c[ImGuiCol_ButtonActive]         = ImVec4(0.851f, 0.275f, 0.937f, 1.00f); // #d946ef magenta

    // Check marks, slider grabs — cyan
    c[ImGuiCol_CheckMark]            = ImVec4(0.024f, 0.714f, 0.831f, 1.00f); // #06b6d4
    c[ImGuiCol_SliderGrab]           = ImVec4(0.024f, 0.714f, 0.831f, 0.80f);
    c[ImGuiCol_SliderGrabActive]     = ImVec4(0.024f, 0.714f, 0.831f, 1.00f);

    // Resize grip
    c[ImGuiCol_ResizeGrip]           = ImVec4(0.545f, 0.361f, 0.965f, 0.40f); // violet
    c[ImGuiCol_ResizeGripHovered]    = ImVec4(0.545f, 0.361f, 0.965f, 0.65f);
    c[ImGuiCol_ResizeGripActive]     = ImVec4(0.024f, 0.714f, 0.831f, 1.00f); // cyan

    // Scrollbar
    c[ImGuiCol_ScrollbarBg]          = ImVec4(0.039f, 0.039f, 0.059f, 0.60f);
    c[ImGuiCol_ScrollbarGrab]        = ImVec4(0.420f, 0.447f, 0.498f, 0.50f); // #6b7280 dust
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.420f, 0.447f, 0.498f, 0.75f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.420f, 0.447f, 0.498f, 1.00f);

    // Text
    c[ImGuiCol_Text]                 = ImVec4(0.976f, 0.980f, 0.984f, 1.00f); // #f9fafb off-white
    c[ImGuiCol_TextDisabled]         = ImVec4(0.420f, 0.447f, 0.498f, 1.00f); // dust gray

    // Borders
    c[ImGuiCol_Border]               = ImVec4(0.165f, 0.165f, 0.208f, 1.00f); // #2a2a35
    c[ImGuiCol_BorderShadow]         = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);

    // Separators
    c[ImGuiCol_Separator]            = ImVec4(0.420f, 0.447f, 0.498f, 0.50f); // dust
    c[ImGuiCol_SeparatorHovered]     = ImVec4(0.545f, 0.361f, 0.965f, 0.70f);
    c[ImGuiCol_SeparatorActive]      = ImVec4(0.545f, 0.361f, 0.965f, 1.00f);

    // Headers (collapsible headers, menu items)
    c[ImGuiCol_Header]               = ImVec4(0.545f, 0.361f, 0.965f, 0.30f); // violet shade
    c[ImGuiCol_HeaderHovered]        = ImVec4(0.545f, 0.361f, 0.965f, 0.50f);
    c[ImGuiCol_HeaderActive]         = ImVec4(0.545f, 0.361f, 0.965f, 0.70f);

    // Docking
    c[ImGuiCol_DockingPreview]       = ImVec4(0.545f, 0.361f, 0.965f, 0.50f);
    c[ImGuiCol_DockingEmptyBg]       = ImVec4(0.039f, 0.039f, 0.059f, 1.00f);

    // Nav
    c[ImGuiCol_NavHighlight]         = ImVec4(0.024f, 0.714f, 0.831f, 1.00f); // cyan
    c[ImGuiCol_NavWindowingHighlight]= ImVec4(0.545f, 0.361f, 0.965f, 0.70f);
    c[ImGuiCol_NavWindowingDimBg]    = ImVec4(0.039f, 0.039f, 0.059f, 0.60f);

    // Modal dimming
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.039f, 0.039f, 0.059f, 0.70f);

    // Menu bar
    c[ImGuiCol_MenuBarBg]            = ImVec4(0.055f, 0.055f, 0.075f, 1.00f);

    // Table
    c[ImGuiCol_TableHeaderBg]        = ImVec4(0.080f, 0.060f, 0.160f, 1.00f);
    c[ImGuiCol_TableBorderStrong]    = ImVec4(0.165f, 0.165f, 0.208f, 1.00f);
    c[ImGuiCol_TableBorderLight]     = ImVec4(0.165f, 0.165f, 0.208f, 0.50f);
    c[ImGuiCol_TableRowBg]           = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
    c[ImGuiCol_TableRowBgAlt]        = ImVec4(1.000f, 1.000f, 1.000f, 0.02f);

    // Text selection
    c[ImGuiCol_TextSelectedBg]       = ImVec4(0.545f, 0.361f, 0.965f, 0.35f);

    // ── Style vars ──────────────────────────────────────────────────────
    style.WindowRounding    = 4.0f;
    style.FrameRounding     = 3.0f;
    style.GrabRounding      = 2.0f;
    style.TabRounding       = 3.0f;
    style.ScrollbarRounding = 4.0f;
    style.FrameBorderSize   = 0.0f;
    style.WindowBorderSize  = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FramePadding      = ImVec2(6.0f, 4.0f);
    style.WindowPadding     = ImVec2(8.0f, 8.0f);
    style.ItemSpacing       = ImVec2(8.0f, 4.0f);

    fprintf(stderr, "[Theme] Magnetar theme applied\n");
}

} // namespace magnetar
