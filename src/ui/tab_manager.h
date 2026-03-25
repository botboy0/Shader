#pragma once

#include <glad.h>

#include <memory>
#include <string>
#include <vector>

namespace magnetar {

class CodeEditor;
class UniformBridge;
class ShaderCompiler;
class ShadertoyFormat;
class MultipassPipeline;

/// Per-tab state: owns the editor, uniform bridge, and compiled GL program.
struct TabState {
    std::unique_ptr<CodeEditor> editor;
    std::unique_ptr<UniformBridge> bridge;
    std::unique_ptr<MultipassPipeline> pipeline;  // non-null when source has multipass markers
    GLuint program = 0;         // compiled GL program (0 = not yet compiled)
    std::string filePath;       // empty = untitled
    std::string fileName;       // display name (e.g. "Untitled 1", "shader.glsl")
    bool dirty = false;
    bool needsCompile = false;
    float compileTimer = 0.0f;
    int untitledIndex = 0;      // for "Untitled N" naming

    TabState();
    ~TabState();
    TabState(TabState&&) noexcept;
    TabState& operator=(TabState&&) noexcept;
};

/// Manages multiple shader editing tabs. Each tab owns its CodeEditor,
/// UniformBridge, and compiled GL program. The TabManager handles creation,
/// closing, switching, and cleanup.
class TabManager {
public:
    TabManager() = default;
    ~TabManager();

    TabManager(const TabManager&) = delete;
    TabManager& operator=(const TabManager&) = delete;

    /// Initialize with pointers to the compiler and format (not owned).
    void init(ShaderCompiler* compiler, ShadertoyFormat* format);

    /// Create a new tab with initial source. If filePath is empty, generates
    /// an "Untitled N" name. Compiles the source and activates the new tab.
    /// Returns the index of the new tab.
    int createTab(const std::string& initialSource, const std::string& filePath = "");

    /// Close a tab by index. Deletes its GL program. If closing the active tab,
    /// switches to the nearest remaining tab. If closing the last tab, creates
    /// a new blank tab.
    void closeTab(int index);

    /// Switch to a tab by index. Resets the target tab's UniformBridge.
    void switchTab(int index);

    /// Get the active tab state, or nullptr if none.
    TabState* getActive();
    const TabState* getActive() const;

    /// Get a tab by index, or nullptr if out of range.
    TabState* getTab(int index);
    const TabState* getTab(int index) const;

    /// Number of open tabs.
    int tabCount() const;

    /// Index of the currently active tab.
    int activeIndex() const;

    /// Tick debounce timers for the active tab and recompile when ready.
    /// Returns the newly compiled program ID if compilation happened and succeeded,
    /// or 0 if no compilation happened or it failed. Caller should call
    /// renderer.setProgram() when the return value is non-zero.
    GLuint update(float dt);

    /// Get all tab names for display (e.g. in tab bar).
    std::vector<std::string> getTabNames() const;

    /// Get all tab dirty flags for display.
    std::vector<bool> getDirtyFlags() const;

    /// Delete all GL programs and clear all tabs.
    void shutdown();

private:
    std::string generateUntitledName();
    GLuint compileMultipass(TabState* tab, const struct MultipassSource& parsed);

    std::vector<std::unique_ptr<TabState>> m_tabs;
    int m_activeTab = 0;
    int m_nextUntitledIndex = 1;

    ShaderCompiler* m_compiler = nullptr;
    ShadertoyFormat* m_format  = nullptr;
};

} // namespace magnetar
