#include "tab_manager.h"

#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <map>

#include "editor/code_editor.h"
#include "shader/uniform_bridge.h"
#include "editor/compiler.h"
#include "shader/shadertoy_format.h"
#include "shader/multipass_parser.h"
#include "shader/multipass_pipeline.h"

namespace magnetar {

// TabState special members — defined here where CodeEditor and UniformBridge are complete types
TabState::TabState() = default;
TabState::~TabState() = default;
TabState::TabState(TabState&&) noexcept = default;
TabState& TabState::operator=(TabState&&) noexcept = default;

TabManager::~TabManager()
{
    shutdown();
}

void TabManager::init(ShaderCompiler* compiler, ShadertoyFormat* format)
{
    m_compiler = compiler;
    m_format   = format;
    fprintf(stderr, "[TabManager] Initialized\n");
}

int TabManager::createTab(const std::string& initialSource, const std::string& filePath)
{
    auto tab = std::make_unique<TabState>();
    tab->editor = std::make_unique<CodeEditor>();
    tab->bridge = std::make_unique<UniformBridge>();

    // Set file info
    tab->filePath = filePath;
    if (filePath.empty()) {
        tab->fileName = generateUntitledName();
        tab->untitledIndex = m_nextUntitledIndex - 1; // already incremented
    } else {
        tab->fileName = std::filesystem::path(filePath).filename().string();
    }

    // Set editor content
    tab->editor->setText(initialSource);

    // Compile the initial source
    if (m_compiler && m_format) {
        std::string wrapped = m_format->wrapSource(initialSource);
        auto result = m_compiler->compile(wrapped, m_format->getLineOffset());
        if (result.success) {
            tab->program = result.program;
        } else {
            fprintf(stderr, "[TabManager] Warning: Initial compile failed for tab '%s'\n",
                    tab->fileName.c_str());
            tab->program = 0;
        }
    }

    int index = static_cast<int>(m_tabs.size());
    std::string name = tab->fileName; // copy before move
    m_tabs.push_back(std::move(tab));
    m_activeTab = index;

    fprintf(stderr, "[TabManager] Tab created: %s (index=%d)\n", name.c_str(), index);
    return index;
}

void TabManager::closeTab(int index)
{
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;

    std::string name = m_tabs[index]->fileName;

    // Delete the tab's GL program (only if not owned by pipeline)
    if (m_tabs[index]->program != 0) {
        bool ownedByPipeline = false;
        if (m_tabs[index]->pipeline) {
            for (int i = 0; i < 5; ++i) {
                if (m_tabs[index]->pipeline->getPassConfig(i).program == m_tabs[index]->program) {
                    ownedByPipeline = true;
                    break;
                }
            }
        }
        if (!ownedByPipeline) {
            glDeleteProgram(m_tabs[index]->program);
        }
        m_tabs[index]->program = 0;
    }

    // Delete all pipeline-owned programs and shutdown pipeline
    if (m_tabs[index]->pipeline) {
        for (int i = 0; i < 5; ++i) {
            GLuint prog = m_tabs[index]->pipeline->getPassConfig(i).program;
            if (prog != 0) {
                glDeleteProgram(prog);
            }
        }
        m_tabs[index]->pipeline->shutdown();
        m_tabs[index]->pipeline.reset();
    }

    m_tabs.erase(m_tabs.begin() + index);

    // If no tabs remain, create a blank tab
    if (m_tabs.empty()) {
        fprintf(stderr, "[TabManager] Tab closed: %s — creating blank tab\n", name.c_str());
        std::string defaultSrc = m_format ? m_format->getDefaultSource() : "";
        createTab(defaultSrc);
        return;
    }

    // Adjust active tab index
    if (m_activeTab >= static_cast<int>(m_tabs.size())) {
        m_activeTab = static_cast<int>(m_tabs.size()) - 1;
    } else if (m_activeTab > index) {
        m_activeTab--;
    }
    // If we closed the active tab, the new activeTab index points to the nearest tab.
    // Reset the new active tab's bridge for clean state.
    if (m_activeTab >= 0 && m_activeTab < static_cast<int>(m_tabs.size())) {
        m_tabs[m_activeTab]->bridge->reset();
    }

    fprintf(stderr, "[TabManager] Tab closed: %s (active=%d)\n", name.c_str(), m_activeTab);
}

void TabManager::switchTab(int index)
{
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return;
    if (index == m_activeTab) return;

    m_activeTab = index;

    // Reset the target tab's UniformBridge (bridge is stateful, needs reset on switch)
    m_tabs[m_activeTab]->bridge->reset();

    fprintf(stderr, "[TabManager] Switched to tab: %s (index=%d)\n",
            m_tabs[m_activeTab]->fileName.c_str(), index);
}

GLuint TabManager::update(float dt)
{
    TabState* tab = getActive();
    if (!tab) return 0;

    // Check for text changes and reset debounce timer
    if (tab->editor->isTextChanged()) {
        tab->needsCompile = true;
        tab->compileTimer = 0.3f; // TODO: use settings.compileDelay
        tab->dirty = true;
    }

    if (!tab->needsCompile) return 0;

    tab->compileTimer -= dt;
    if (tab->compileTimer > 0.0f) return 0;

    // Time to compile
    tab->needsCompile = false;

    if (!m_compiler || !m_format) return 0;

    std::string source = tab->editor->getText();

    // Parse to detect multipass markers
    auto parsed = MultipassParser::parse(source);

    if (parsed.isMultipass) {
        return compileMultipass(tab, parsed);
    } else {
        // Single-pass: clear pipeline if one was active
        if (tab->pipeline) {
            tab->pipeline->shutdown();
            tab->pipeline.reset();
            fprintf(stderr, "[TabManager] Cleared multipass pipeline — single-pass mode\n");
        }

        // Existing single-pass compile path
        std::string wrapped = m_format->wrapSource(source);
        auto result = m_compiler->compile(wrapped, m_format->getLineOffset());

        if (result.success) {
            if (tab->program != 0) {
                glDeleteProgram(tab->program);
            }
            tab->program = result.program;
            tab->editor->clearErrorMarkers();
            fprintf(stderr, "[TabManager] Compile success for tab: %s\n", tab->fileName.c_str());
            return tab->program;
        } else {
            std::map<int, std::string> markers;
            for (auto& e : result.errors) {
                auto it = markers.find(e.line);
                if (it != markers.end()) {
                    it->second += "\n" + e.message;
                } else {
                    markers[e.line] = e.message;
                }
            }
            tab->editor->setErrorMarkers(markers);
            fprintf(stderr, "[TabManager] Compile failed for tab: %s (%d errors)\n",
                    tab->fileName.c_str(), static_cast<int>(result.errors.size()));
            return 0;
        }
    }
}

GLuint TabManager::compileMultipass(TabState* tab, const MultipassSource& parsed)
{
    // Lazily create or reinitialize the pipeline
    if (!tab->pipeline) {
        tab->pipeline = std::make_unique<MultipassPipeline>();
        // Use a default size — will be resized by main.cpp when preview pane size is known
        if (!tab->pipeline->init(256, 256)) {
            fprintf(stderr, "[TabManager] Error: Failed to create multipass pipeline\n");
            tab->pipeline.reset();
            return 0;
        }
    }

    // PassData pointers for indexed access: 0-3 = buffers, 4 = image
    const PassData* passes[5] = {
        &parsed.buffers[0], &parsed.buffers[1],
        &parsed.buffers[2], &parsed.buffers[3],
        &parsed.image
    };
    const char* passNames[5] = {"BufferA", "BufferB", "BufferC", "BufferD", "Image"};

    std::map<int, std::string> allMarkers;
    GLuint imageProgram = 0;

    for (int i = 0; i < 5; ++i) {
        const PassData* pass = passes[i];
        if (!pass->active) {
            tab->pipeline->clearPass(i);
            continue;
        }

        std::string wrapped = m_format->wrapSource(pass->source);
        auto result = m_compiler->compile(wrapped, m_format->getLineOffset());

        if (result.success) {
            // Delete old per-pass program if pipeline already had one
            const auto& oldConfig = tab->pipeline->getPassConfig(i);
            if (oldConfig.program != 0) {
                glDeleteProgram(oldConfig.program);
            }
            tab->pipeline->setPass(i, result.program, pass->channels);

            if (i == MultipassPipeline::kImagePass) {
                imageProgram = result.program;
            }

            fprintf(stderr, "[TabManager] %s compile success (program=%u)\n",
                    passNames[i], result.program);
        } else {
            tab->pipeline->clearPass(i);
            fprintf(stderr, "[MultipassPipeline] %s compile failed\n", passNames[i]);

            // Remap error lines: rawErrorLine is already header-adjusted.
            // Add the pass's startLine to map back to editor coordinates.
            for (auto& e : result.errors) {
                int editorLine = e.line + pass->startLine - 1;
                auto it = allMarkers.find(editorLine);
                if (it != allMarkers.end()) {
                    it->second += "\n[" + std::string(passNames[i]) + "] " + e.message;
                } else {
                    allMarkers[editorLine] = "[" + std::string(passNames[i]) + "] " + e.message;
                }
            }
        }
    }

    if (allMarkers.empty()) {
        tab->editor->clearErrorMarkers();
    } else {
        tab->editor->setErrorMarkers(allMarkers);
    }

    // The Image pass program is what main.cpp sets on the renderer
    // (for single-pass fallback path and for getTextureId() display)
    if (imageProgram != 0) {
        if (tab->program != 0 && tab->program != imageProgram) {
            // Don't delete old program if it belongs to a pipeline pass —
            // the pipeline owns it. Only delete if it's a stale single-pass program.
            bool ownedByPipeline = false;
            for (int i = 0; i < MultipassPipeline::kNumPasses; ++i) {
                if (tab->pipeline->getPassConfig(i).program == tab->program) {
                    ownedByPipeline = true;
                    break;
                }
            }
            if (!ownedByPipeline) {
                glDeleteProgram(tab->program);
            }
        }
        tab->program = imageProgram;
    }

    return imageProgram;
}

std::vector<std::string> TabManager::getTabNames() const
{
    std::vector<std::string> names;
    names.reserve(m_tabs.size());
    for (auto& tab : m_tabs) {
        names.push_back(tab->fileName);
    }
    return names;
}

std::vector<bool> TabManager::getDirtyFlags() const
{
    std::vector<bool> flags;
    flags.reserve(m_tabs.size());
    for (auto& tab : m_tabs) {
        flags.push_back(tab->dirty);
    }
    return flags;
}

TabState* TabManager::getActive()
{
    if (m_activeTab < 0 || m_activeTab >= static_cast<int>(m_tabs.size())) return nullptr;
    return m_tabs[m_activeTab].get();
}

const TabState* TabManager::getActive() const
{
    if (m_activeTab < 0 || m_activeTab >= static_cast<int>(m_tabs.size())) return nullptr;
    return m_tabs[m_activeTab].get();
}

TabState* TabManager::getTab(int index)
{
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return nullptr;
    return m_tabs[index].get();
}

const TabState* TabManager::getTab(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_tabs.size())) return nullptr;
    return m_tabs[index].get();
}

int TabManager::tabCount() const
{
    return static_cast<int>(m_tabs.size());
}

int TabManager::activeIndex() const
{
    return m_activeTab;
}

void TabManager::shutdown()
{
    int programsDeleted = 0;
    for (auto& tab : m_tabs) {
        // Delete pipeline-owned programs first
        if (tab->pipeline) {
            for (int i = 0; i < 5; ++i) {
                GLuint prog = tab->pipeline->getPassConfig(i).program;
                if (prog != 0) {
                    glDeleteProgram(prog);
                    programsDeleted++;
                }
            }
            tab->pipeline->shutdown();
            tab->pipeline.reset();
        }

        // Delete the tab's main program if it's not already pipeline-owned
        if (tab->program != 0) {
            // May have been already deleted above if it was a pipeline pass
            // Only delete if it's a standalone program
            glDeleteProgram(tab->program);
            tab->program = 0;
            programsDeleted++;
        }
    }
    m_tabs.clear();
    m_activeTab = 0;

    fprintf(stderr, "[TabManager] Shutdown: %d programs deleted\n", programsDeleted);
}

std::string TabManager::generateUntitledName()
{
    std::string name = "Untitled " + std::to_string(m_nextUntitledIndex);
    m_nextUntitledIndex++;
    return name;
}

} // namespace magnetar
