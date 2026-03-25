#include "settings.h"

#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>
#include <SDL3/SDL.h>
#include <imgui.h>

namespace magnetar {

// ─── JSON serialization ──────────────────────────────────────────────────────

void Settings::load(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        fprintf(stderr, "[Settings] Warning: Could not open %s — using defaults\n", path.c_str());
        return;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(file);

        if (j.contains("vsync") && j["vsync"].is_boolean())
            vsync = j["vsync"].get<bool>();
        if (j.contains("framerateCap") && j["framerateCap"].is_number_integer())
            framerateCap = j["framerateCap"].get<int>();
        if (j.contains("compileDelay") && j["compileDelay"].is_number())
            compileDelay = j["compileDelay"].get<float>();
        if (j.contains("viewMode") && j["viewMode"].is_number_integer())
            viewMode = j["viewMode"].get<int>();
        if (j.contains("lastOpenDir") && j["lastOpenDir"].is_string())
            lastOpenDir = j["lastOpenDir"].get<std::string>();
        if (j.contains("lastSaveDir") && j["lastSaveDir"].is_string())
            lastSaveDir = j["lastSaveDir"].get<std::string>();

        fprintf(stderr, "[Settings] Loaded from %s\n", path.c_str());
    }
    catch (const nlohmann::json::exception& e) {
        fprintf(stderr, "[Settings] Warning: JSON parse error in %s: %s — using defaults\n",
                path.c_str(), e.what());
    }
}

void Settings::save(const std::string& path)
{
    nlohmann::json j;
    j["vsync"]        = vsync;
    j["framerateCap"] = framerateCap;
    j["compileDelay"] = compileDelay;
    j["viewMode"]     = viewMode;
    j["lastOpenDir"]  = lastOpenDir;
    j["lastSaveDir"]  = lastSaveDir;

    // Atomic write: write to temp file, then rename
    std::string tmpPath = path + ".tmp";
    {
        std::ofstream file(tmpPath);
        if (!file.is_open()) {
            fprintf(stderr, "[Settings] Error: Could not write to %s\n", tmpPath.c_str());
            return;
        }
        file << j.dump(2) << "\n";
    }

    // Rename temp → final (atomic on most filesystems)
    if (std::rename(tmpPath.c_str(), path.c_str()) != 0) {
        fprintf(stderr, "[Settings] Error: Could not rename %s → %s\n",
                tmpPath.c_str(), path.c_str());
        // Try direct write as fallback
        std::ofstream file(path);
        if (file.is_open()) {
            file << j.dump(2) << "\n";
            fprintf(stderr, "[Settings] Saved to %s (direct write fallback)\n", path.c_str());
        }
        return;
    }

    fprintf(stderr, "[Settings] Saved to %s\n", path.c_str());
}

std::string Settings::getDefaultConfigDir()
{
    char* prefPath = SDL_GetPrefPath("Magnetar", "ShaderEditor");
    if (!prefPath) {
        fprintf(stderr, "[Settings] Warning: SDL_GetPrefPath failed: %s\n", SDL_GetError());
        return "";
    }
    std::string dir(prefPath);
    SDL_free(prefPath);
    return dir;
}

std::string Settings::getDefaultPath()
{
    std::string dir = getDefaultConfigDir();
    if (dir.empty()) return "";
    return dir + "settings.json";
}

void Settings::openDialog()
{
    m_shouldOpen = true;
}

bool Settings::renderDialog(const std::string& settingsPath)
{
    if (m_shouldOpen) {
        ImGui::OpenPopup("Settings");
        m_shouldOpen = false;
        m_dialogOpen = true;
    }

    bool changed = false;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 250), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Settings", &m_dialogOpen, ImGuiWindowFlags_NoResize)) {
        ImGui::Text("Application Settings");
        ImGui::Separator();
        ImGui::Spacing();

        // Vsync toggle
        if (ImGui::Checkbox("VSync", &vsync)) {
            changed = true;
        }

        // Framerate cap slider (0=uncapped, 30-240)
        ImGui::Spacing();
        if (framerateCap == 0) {
            ImGui::Text("Framerate Cap: Uncapped");
        } else {
            ImGui::Text("Framerate Cap: %d FPS", framerateCap);
        }
        int capSlider = framerateCap;
        if (ImGui::SliderInt("##FramerateCap", &capSlider, 0, 240, capSlider == 0 ? "Uncapped" : "%d")) {
            if (capSlider > 0 && capSlider < 30) capSlider = 30; // clamp minimum
            framerateCap = capSlider;
            changed = true;
        }

        // Compile delay slider
        ImGui::Spacing();
        ImGui::Text("Compile Delay: %.2f s", compileDelay);
        if (ImGui::SliderFloat("##CompileDelay", &compileDelay, 0.1f, 2.0f, "%.2f s")) {
            changed = true;
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (ImGui::Button("Close", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            m_dialogOpen = false;
            // Auto-save on close
            if (!settingsPath.empty()) {
                save(settingsPath);
            }
        }

        ImGui::EndPopup();
    } else if (!m_dialogOpen && changed) {
        // Dialog was closed via X button — auto-save
        if (!settingsPath.empty()) {
            save(settingsPath);
        }
    }

    return changed;
}

} // namespace magnetar
