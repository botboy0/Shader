#include "shortcuts.h"

#include <cstdio>
#include <imgui.h>

namespace magnetar {

ShortcutManager::ShortcutManager()
{
    fprintf(stderr, "[Shortcuts] Initialized\n");
}

ShortcutAction ShortcutManager::processEvent(const SDL_Event& event)
{
    if (event.type != SDL_EVENT_KEY_DOWN) return ShortcutAction::None;

    // Ignore key repeats for shortcuts
    if (event.key.repeat) return ShortcutAction::None;

    SDL_Keycode key = event.key.key;
    SDL_Keymod mod  = event.key.mod;

    bool ctrl  = (mod & SDL_KMOD_CTRL) != 0;
    bool shift = (mod & SDL_KMOD_SHIFT) != 0;

    // Ctrl+modifier shortcuts always process regardless of ImGui keyboard focus
    if (ctrl) {
        ShortcutAction action = ShortcutAction::None;

        if (key == SDLK_N && !shift) {
            action = ShortcutAction::NewTab;
        } else if (key == SDLK_O && !shift) {
            action = ShortcutAction::OpenFile;
        } else if (key == SDLK_S && shift) {
            action = ShortcutAction::SaveAs;
        } else if (key == SDLK_S && !shift) {
            action = ShortcutAction::Save;
        } else if (key == SDLK_W && !shift) {
            action = ShortcutAction::CloseTab;
        } else if (key == SDLK_Q && !shift) {
            action = ShortcutAction::Quit;
        } else if (key == SDLK_R && !shift) {
            action = ShortcutAction::ForceRecompile;
        }

        if (action != ShortcutAction::None) {
            fprintf(stderr, "[Shortcuts] Processed: %s\n", actionName(action));
            return action;
        }
    }

    // Non-ctrl keys: only process when ImGui doesn't want keyboard
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        ShortcutAction action = ShortcutAction::None;

        if (key == SDLK_F5) {
            action = ShortcutAction::ForceRecompile;
        } else if (key == SDLK_F6) {
            action = ShortcutAction::CycleViewMode;
        }

        if (action != ShortcutAction::None) {
            fprintf(stderr, "[Shortcuts] Processed: %s\n", actionName(action));
            return action;
        }
    }

    return ShortcutAction::None;
}

const char* ShortcutManager::actionName(ShortcutAction action)
{
    switch (action) {
    case ShortcutAction::None:           return "None";
    case ShortcutAction::NewTab:         return "NewTab";
    case ShortcutAction::OpenFile:       return "OpenFile";
    case ShortcutAction::Save:           return "Save";
    case ShortcutAction::SaveAs:         return "SaveAs";
    case ShortcutAction::CloseTab:       return "CloseTab";
    case ShortcutAction::Quit:           return "Quit";
    case ShortcutAction::ForceRecompile: return "ForceRecompile";
    case ShortcutAction::CycleViewMode:  return "CycleViewMode";
    }
    return "Unknown";
}

} // namespace magnetar
