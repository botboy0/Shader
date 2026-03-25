#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>

#include <glad.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>

#include "core/app.h"
#include "core/renderer.h"
#include "editor/code_editor.h"
#include "editor/compiler.h"
#include "shader/shadertoy_format.h"
#include "shader/uniform_bridge.h"
#include "shader/multipass_pipeline.h"
#include "ui/imgui_manager.h"
#include "ui/layout.h"
#include "ui/settings.h"
#include "ui/tab_manager.h"
#include "ui/file_dialog.h"
#include "ui/template_picker.h"
#include "ui/shortcuts.h"
#include "ui/session.h"

int main(int /*argc*/, char* /*argv*/[])
{
    magnetar::App app("Magnetar Shader Editor", 1280, 720);

    if (!app.init()) {
        return 1;
    }

    magnetar::ShaderRenderer renderer;
    if (!renderer.init(app.width(), app.height())) {
        fprintf(stderr, "[Main] Error: Renderer initialization failed\n");
        app.shutdown();
        return 1;
    }

    // --- ImGui setup ---
    magnetar::ImGuiManager imguiMgr;
    if (!imguiMgr.init(app.window(), SDL_GL_GetCurrentContext())) {
        fprintf(stderr, "[Main] Error: ImGui initialization failed\n");
        renderer.shutdown();
        app.shutdown();
        return 1;
    }

    // --- Settings ---
    magnetar::Settings settings;
    std::string settingsPath = magnetar::Settings::getDefaultPath();
    if (!settingsPath.empty()) {
        settings.load(settingsPath);
    }

    // Apply loaded settings to FrameControl
    if (settings.vsync != app.frameControl().isVsyncEnabled()) {
        app.frameControl().toggleVsync();
    }
    app.frameControl().setFramerateCap(settings.framerateCap);
    fprintf(stderr, "[Main] Settings applied: vsync=%s, cap=%d\n",
            settings.vsync ? "on" : "off", settings.framerateCap);

    // --- Layout ---
    magnetar::Layout layout;
    layout.setViewMode(static_cast<magnetar::ViewMode>(settings.viewMode));

    // --- Shader compiler + format ---
    magnetar::ShaderCompiler compiler;
    magnetar::ShadertoyFormat format;

    // --- File dialog, template picker, shortcuts ---
    magnetar::FileDialog fileDialog;
    magnetar::TemplatePicker templatePicker;
    magnetar::ShortcutManager shortcuts;

    // --- TabManager ---
    magnetar::TabManager tabManager;
    tabManager.init(&compiler, &format);

    // --- Session restore ---
    std::string configDir = magnetar::Settings::getDefaultConfigDir();
    magnetar::SessionManager sessionManager(configDir);
    magnetar::SessionData sessionData;
    bool sessionRestored = false;

    if (sessionManager.load(sessionData) && !sessionData.tabs.empty()) {
        for (auto& td : sessionData.tabs) {
            tabManager.createTab(td.content, td.filePath);
        }
        // Restore cursor positions after all tabs are created
        for (int i = 0; i < static_cast<int>(sessionData.tabs.size()); i++) {
            magnetar::TabState* tab = tabManager.getTab(i);
            if (tab && tab->editor) {
                tab->editor->setCursorPosition(sessionData.tabs[i].cursorLine,
                                                sessionData.tabs[i].cursorCol);
            }
        }
        // Switch to saved active tab
        if (sessionData.activeTabIndex >= 0 && sessionData.activeTabIndex < tabManager.tabCount()) {
            tabManager.switchTab(sessionData.activeTabIndex);
        }
        sessionRestored = true;
        fprintf(stderr, "[Main] Session restored: %d tabs\n",
                static_cast<int>(sessionData.tabs.size()));
    }

    // Create one default tab if no session was restored
    if (!sessionRestored) {
        tabManager.createTab(format.getDefaultSource());
    }

    // Set the renderer to the default tab's program, clean up renderer's init program
    GLuint initProgram = renderer.getProgram();
    magnetar::TabState* initialTab = tabManager.getActive();
    if (initialTab && initialTab->program != 0) {
        renderer.setProgram(initialTab->program);
    }
    // Delete the orphaned program from renderer.init() — TabManager owns programs now
    if (initProgram != 0 && (!initialTab || initProgram != initialTab->program)) {
        glDeleteProgram(initProgram);
    }

    float lastFrameTime = app.getElapsedTime();
    bool settingsClicked = false;

    // --- Shortcut action queue (set in event callback, processed in update) ---
    magnetar::ShortcutAction pendingAction = magnetar::ShortcutAction::None;

    // --- Dirty-close confirmation state ---
    bool showDirtyCloseModal = false;
    int  dirtyCloseTabIndex = -1;

    // Forward all SDL events to ImGui backend, then to ShortcutManager
    app.setEventCallback([&shortcuts, &pendingAction](const SDL_Event& e) {
        ImGui_ImplSDL3_ProcessEvent(&e);

        // Feed to ShortcutManager — it handles Ctrl+keys and F5/F6
        auto action = shortcuts.processEvent(e);
        if (action != magnetar::ShortcutAction::None) {
            pendingAction = action;
        }

        return true;
    });

    // Resize callback: no longer ties renderer to window size.
    app.setResizeCallback([](int /*w*/, int /*h*/) {
        // Window resize handled implicitly — ImGui panes reflow,
        // and the next frame will pick up the new Preview pane size.
    });

    // Helper: update renderer to show the active tab's program
    auto syncRendererToActiveTab = [&renderer, &tabManager]() {
        magnetar::TabState* tab = tabManager.getActive();
        if (tab && tab->program != 0) {
            renderer.setProgram(tab->program);
        }
    };

    // Helper: save active tab content to a file path
    auto saveTabToFile = [&tabManager, &settings](const std::string& path) {
        magnetar::TabState* tab = tabManager.getActive();
        if (!tab) return;

        std::ofstream out(path);
        if (!out.is_open()) {
            fprintf(stderr, "[Main] Error: Could not write to %s\n", path.c_str());
            return;
        }
        out << tab->editor->getText();
        out.close();

        tab->filePath = path;
        tab->fileName = std::filesystem::path(path).filename().string();
        tab->dirty = false;

        // Remember save directory
        settings.lastSaveDir = std::filesystem::path(path).parent_path().string();

        fprintf(stderr, "[Main] Saved tab to %s\n", path.c_str());
    };

    // Helper: load file content into a new tab
    auto loadFileIntoNewTab = [&tabManager, &settings, &syncRendererToActiveTab](const std::string& path) {
        std::ifstream in(path);
        if (!in.is_open()) {
            fprintf(stderr, "[Main] Error: Could not read %s\n", path.c_str());
            return;
        }
        std::string content((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
        in.close();

        tabManager.createTab(content, path);
        syncRendererToActiveTab();

        // Remember open directory
        settings.lastOpenDir = std::filesystem::path(path).parent_path().string();

        fprintf(stderr, "[Main] Opened file: %s\n", path.c_str());
    };

    // Wire update: render shader → FBO, then ImGui dockspace layout
    app.setUpdateCallback([&app, &renderer, &imguiMgr, &layout, &tabManager,
                           &lastFrameTime, &format, &settingsClicked, &settings,
                           &settingsPath, &fileDialog, &templatePicker,
                           &pendingAction, &showDirtyCloseModal, &dirtyCloseTabIndex,
                           &syncRendererToActiveTab, &saveTabToFile, &loadFileIntoNewTab]() {
        // Compute delta time
        float now = app.getElapsedTime();
        float dt = now - lastFrameTime;
        lastFrameTime = now;
        if (dt <= 0.0f) dt = 1.0f / 60.0f; // safety clamp

        // --- Poll file dialog results ---
        if (fileDialog.hasResult()) {
            auto result = fileDialog.pollResult();
            if (result.isSave) {
                saveTabToFile(result.path);
            } else {
                loadFileIntoNewTab(result.path);
            }
        }

        // --- Process shortcut actions ---
        magnetar::ShortcutAction action = pendingAction;
        pendingAction = magnetar::ShortcutAction::None;

        switch (action) {
        case magnetar::ShortcutAction::NewTab:
            templatePicker.open();
            break;
        case magnetar::ShortcutAction::OpenFile:
            fileDialog.openFile(app.window(), settings.lastOpenDir);
            break;
        case magnetar::ShortcutAction::Save: {
            magnetar::TabState* tab = tabManager.getActive();
            if (tab) {
                if (!tab->filePath.empty()) {
                    saveTabToFile(tab->filePath);
                } else {
                    // No file path yet — trigger Save As
                    fileDialog.saveFile(app.window(), tab->fileName, settings.lastSaveDir);
                }
            }
            break;
        }
        case magnetar::ShortcutAction::SaveAs: {
            magnetar::TabState* tab = tabManager.getActive();
            if (tab) {
                fileDialog.saveFile(app.window(), tab->fileName, settings.lastSaveDir);
            }
            break;
        }
        case magnetar::ShortcutAction::CloseTab: {
            magnetar::TabState* tab = tabManager.getActive();
            if (tab && tab->dirty) {
                showDirtyCloseModal = true;
                dirtyCloseTabIndex = tabManager.activeIndex();
            } else {
                tabManager.closeTab(tabManager.activeIndex());
                syncRendererToActiveTab();
            }
            break;
        }
        case magnetar::ShortcutAction::Quit:
            // Save session + settings handled at exit
            app.requestQuit();
            break;
        case magnetar::ShortcutAction::ForceRecompile: {
            magnetar::TabState* tab = tabManager.getActive();
            if (tab) {
                tab->needsCompile = true;
                tab->compileTimer = 0.0f; // compile immediately
                fprintf(stderr, "[Main] Force recompile requested\n");
            }
            break;
        }
        case magnetar::ShortcutAction::CycleViewMode:
            layout.cycleViewMode();
            break;
        case magnetar::ShortcutAction::None:
            break;
        }

        // --- TabManager compile debounce ---
        GLuint newProgram = tabManager.update(dt);
        if (newProgram != 0) {
            renderer.setProgram(newProgram);
        }

        // --- Get active tab for rendering ---
        magnetar::TabState* activeTab = tabManager.getActive();

        // UniformBridge handles all Shadertoy uniforms
        int pw = layout.previewWidth();
        int ph = layout.previewHeight();
        auto& mouseState = layout.getMouseState();
        magnetar::MouseInput mouseInput;
        mouseInput.x = mouseState.x;
        mouseInput.y = mouseState.y;
        mouseInput.inPreview = mouseState.inPreview;
        mouseInput.buttonDown = mouseState.buttonDown;
        mouseInput.clicked = mouseState.clicked;

        if (activeTab) {
            activeTab->bridge->update(dt,
                static_cast<float>(pw > 0 ? pw : app.width()),
                static_cast<float>(ph > 0 ? ph : app.height()),
                mouseInput);
            activeTab->bridge->apply(renderer);
        }

        // Render shader to FBO — multipass or single-pass
        if (activeTab && activeTab->pipeline && activeTab->pipeline->isMultipass()) {
            // Resize pipeline FBOs to match preview pane
            int rpw = pw > 0 ? pw : app.width();
            int rph = ph > 0 ? ph : app.height();
            activeTab->pipeline->resize(rpw, rph);

            // Apply uniforms to each active pass's program
            for (int i = 0; i < magnetar::MultipassPipeline::kNumPasses; ++i) {
                const auto& cfg = activeTab->pipeline->getPassConfig(i);
                if (cfg.active && cfg.program != 0) {
                    activeTab->bridge->applyToProgram(cfg.program);
                }
            }

            // Render all passes (buffers → Image → renderer's FBO)
            activeTab->pipeline->renderAllPasses(renderer.getQuadVao(), renderer);
        } else {
            // Single-pass: existing path
            renderer.render();
        }

        // Clear default framebuffer to Magnetar void black before ImGui draws
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, app.width(), app.height());
        glClearColor(0.04f, 0.04f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // ImGui frame: dockspace + panes
        imguiMgr.beginFrame();

        layout.beginFrame();

        // --- Tab bar (inside Editor pane) ---
        if (layout.getViewMode() != magnetar::ViewMode::PreviewOnly) {
            ImGui::Begin("Editor");

            auto tabNames = tabManager.getTabNames();
            auto dirtyFlags = tabManager.getDirtyFlags();
            auto tabResult = layout.renderTabBar(tabNames, dirtyFlags, tabManager.activeIndex());

            // Process tab bar interactions
            if (tabResult.clickedTab >= 0) {
                tabManager.switchTab(tabResult.clickedTab);
                syncRendererToActiveTab();
            }
            if (tabResult.requestClose) {
                int closedIdx = tabResult.closedTab;
                magnetar::TabState* closingTab = tabManager.getTab(closedIdx);
                if (closingTab && closingTab->dirty) {
                    // Dirty tab — show confirmation modal
                    // Switch to the tab being closed so the user sees which one
                    if (closedIdx != tabManager.activeIndex()) {
                        tabManager.switchTab(closedIdx);
                        syncRendererToActiveTab();
                    }
                    showDirtyCloseModal = true;
                    dirtyCloseTabIndex = closedIdx;
                } else {
                    bool closedWasActive = (closedIdx == tabManager.activeIndex());
                    tabManager.closeTab(closedIdx);
                    if (closedWasActive) {
                        syncRendererToActiveTab();
                    }
                }
            }
            if (tabResult.requestNewTab) {
                templatePicker.open();
            }

            // Render editor content for active tab
            magnetar::TabState* tab = tabManager.getActive();
            if (tab) {
                ImFont* codeFont = imguiMgr.theme().getCodeFont();
                if (codeFont) ImGui::PushFont(codeFont);
                tab->editor->render();
                if (codeFont) ImGui::PopFont();
            }

            ImGui::End();
        }

        layout.renderPreviewPane(renderer.getTextureId(), renderer.width(), renderer.height());
        layout.renderStatusBar(
            app.frameControl().getFps(),
            app.frameControl().isVsyncEnabled(),
            app.frameControl().getFramerateCap(),
            &settingsClicked
        );

        // --- Settings dialog (triggered by status bar button) ---
        if (settingsClicked) {
            settings.openDialog();
            settingsClicked = false;
        }
        if (settings.renderDialog(settingsPath)) {
            // Settings changed — apply immediately
            if (settings.vsync != app.frameControl().isVsyncEnabled()) {
                app.frameControl().toggleVsync();
            }
            app.frameControl().setFramerateCap(settings.framerateCap);
        }

        // --- Template picker modal ---
        templatePicker.render();
        if (templatePicker.hasSelection()) {
            std::string source = templatePicker.takeSelection();
            tabManager.createTab(source);
            syncRendererToActiveTab();
        }

        // --- Dirty-close confirmation modal ---
        if (showDirtyCloseModal) {
            ImGui::OpenPopup("Save Changes?");
            showDirtyCloseModal = false;
        }
        {
            ImVec2 center = ImGui::GetMainViewport()->GetCenter();
            ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            if (ImGui::BeginPopupModal("Save Changes?", nullptr,
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize)) {
                magnetar::TabState* dirtyTab = tabManager.getTab(dirtyCloseTabIndex);
                const char* name = dirtyTab ? dirtyTab->fileName.c_str() : "this tab";
                ImGui::Text("Do you want to save changes to \"%s\"?", name);
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Save", ImVec2(100, 0))) {
                    if (dirtyTab) {
                        if (!dirtyTab->filePath.empty()) {
                            saveTabToFile(dirtyTab->filePath);
                        } else {
                            fileDialog.saveFile(app.window(), dirtyTab->fileName, settings.lastSaveDir);
                        }
                    }
                    tabManager.closeTab(dirtyCloseTabIndex);
                    syncRendererToActiveTab();
                    dirtyCloseTabIndex = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Don't Save", ImVec2(100, 0))) {
                    tabManager.closeTab(dirtyCloseTabIndex);
                    syncRendererToActiveTab();
                    dirtyCloseTabIndex = -1;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(100, 0))) {
                    dirtyCloseTabIndex = -1;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }
        }

        imguiMgr.endFrame();

        // Resize renderer FBO to match Preview pane (next frame renders at correct size)
        pw = layout.previewWidth();
        ph = layout.previewHeight();
        if (pw > 0 && ph > 0) {
            renderer.resize(pw, ph);
        }
    });

    app.run();

    // --- Save session and settings before exit ---
    sessionManager.save(tabManager);

    settings.vsync = app.frameControl().isVsyncEnabled();
    settings.framerateCap = app.frameControl().getFramerateCap();
    settings.viewMode = static_cast<int>(layout.getViewMode());
    if (!settingsPath.empty()) {
        settings.save(settingsPath);
    }

    tabManager.shutdown();
    imguiMgr.shutdown();
    renderer.shutdown();
    app.shutdown();

    return 0;
}
