#pragma once

#include <map>
#include <string>
#include <vector>
#include <TextEditor.h>
#include "autocomplete.h"

namespace magnetar {

/// Wraps ImGuiColorTextEdit with a custom GLSL language definition and
/// Magnetar-themed syntax palette.  The caller manages ImGui window context
/// and PushFont/PopFont around render().
class CodeEditor {
public:
    CodeEditor();
    ~CodeEditor() = default;

    CodeEditor(const CodeEditor&) = delete;
    CodeEditor& operator=(const CodeEditor&) = delete;

    /// Render the text editor widget.  Must be called inside an ImGui window
    /// context with the desired code font already pushed.
    void render();

    /// Get the full editor text content.
    std::string getText() const;

    /// Replace editor text content.
    void setText(const std::string& text);

    /// Returns true on the frame the user modified text.
    bool isTextChanged() const;

    /// Set error markers (red line highlights with tooltip messages).
    /// Line numbers are 1-based.
    void setErrorMarkers(const std::map<int, std::string>& markers);

    /// Clear all error markers.
    void clearErrorMarkers();

    /// Get cursor line (0-based).
    int getCursorLine() const;

    /// Get cursor column (0-based).
    int getCursorCol() const;

    /// Set cursor position (0-based line and column).
    void setCursorPosition(int line, int col);

private:
    void setupGlslLanguage();
    void setupMagnetarPalette();

    // Autocomplete popup management
    void updateAutocomplete();
    void renderAutocompletePopup();
    void acceptCompletion();
    void dismissAutocomplete();

    TextEditor m_editor;

    // Autocomplete state
    bool m_acVisible = false;
    int  m_acSelected = 0;
    int  m_acPrevLine = -1;
    std::string m_acPrefix;
    std::vector<const CompletionItem*> m_acFiltered;
};

} // namespace magnetar
