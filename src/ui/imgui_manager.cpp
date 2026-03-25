#include "imgui_manager.h"

#include <cstdio>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>

namespace magnetar {

ImGuiManager::~ImGuiManager()
{
    shutdown();
}

bool ImGuiManager::init(SDL_Window* window, SDL_GLContext glCtx)
{
    if (m_initialized) return true;

    // --- Create ImGui context ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;  // Enable docking (S02 requirement)

    // --- Initialize backends ---
    if (!ImGui_ImplSDL3_InitForOpenGL(window, glCtx)) {
        fprintf(stderr, "[ImGui] Error: ImGui_ImplSDL3_InitForOpenGL failed\n");
        ImGui::DestroyContext();
        return false;
    }

    if (!ImGui_ImplOpenGL3_Init("#version 450")) {
        fprintf(stderr, "[ImGui] Error: ImGui_ImplOpenGL3_Init failed\n");
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        return false;
    }

    // --- Load fonts and apply Magnetar theme (must be before first NewFrame) ---
    m_theme.loadFonts();
    m_theme.apply();

    m_initialized = true;
    fprintf(stderr, "[ImGui] ImGui initialized (docking enabled, GLSL #version 450)\n");
    return true;
}

void ImGuiManager::shutdown()
{
    if (!m_initialized) return;

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    m_initialized = false;
    fprintf(stderr, "[ImGui] Shutdown\n");
}

void ImGuiManager::beginFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::endFrame()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

} // namespace magnetar
