#pragma once

#include <string>
#include <vector>

namespace magnetar {

/// A built-in shader template with Shadertoy-compatible mainImage() entry point.
struct ShaderTemplate {
    const char* name;
    const char* description;
    const char* source;
};

/// Modal popup that lets the user pick from built-in shader templates.
/// Call open() to request the popup. Each frame, call render() which
/// draws the modal when open. After selection, hasSelection() returns
/// true and takeSelection() returns the source code.
class TemplatePicker {
public:
    TemplatePicker();
    ~TemplatePicker() = default;

    /// Request the template picker modal to open on the next frame.
    void open();

    /// Render the modal popup. Must be called every frame.
    void render();

    /// True if user picked a template since last take.
    bool hasSelection() const { return m_hasSelection; }

    /// Return the selected template source and clear the selection state.
    std::string takeSelection();

    /// Access the built-in templates.
    const std::vector<ShaderTemplate>& templates() const { return m_templates; }

private:
    std::vector<ShaderTemplate> m_templates;
    bool m_shouldOpen = false;
    bool m_hasSelection = false;
    std::string m_selectedSource;
};

} // namespace magnetar
