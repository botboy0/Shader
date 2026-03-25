#pragma once

#include <SDL3/SDL.h>

namespace magnetar {

/// Actions that keyboard shortcuts can trigger.
enum class ShortcutAction {
    None,
    NewTab,
    OpenFile,
    Save,
    SaveAs,
    CloseTab,
    Quit,
    ForceRecompile,
    CycleViewMode
};

/// Processes SDL key events and maps them to shortcut actions.
/// Ctrl+modifier shortcuts always fire (even when editor has keyboard focus).
/// Raw keys (F5, F6) only fire when ImGui doesn't want keyboard input.
class ShortcutManager {
public:
    ShortcutManager();
    ~ShortcutManager() = default;

    /// Process an SDL event. Returns the recognized action, or None.
    ShortcutAction processEvent(const SDL_Event& event);

    /// Convert an action to a human-readable name (for logging).
    static const char* actionName(ShortcutAction action);
};

} // namespace magnetar
