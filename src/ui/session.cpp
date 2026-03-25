#include "session.h"

#include <cstdio>
#include <fstream>
#include <nlohmann/json.hpp>

#include "tab_manager.h"
#include "editor/code_editor.h"

namespace magnetar {

SessionManager::SessionManager(const std::string& sessionDir)
{
    if (sessionDir.empty()) {
        m_path.clear();
        fprintf(stderr, "[Session] Warning: Empty session directory — session persistence disabled\n");
    } else {
        m_path = sessionDir + "session.json";
    }
    fprintf(stderr, "[Session] Initialized (path=%s)\n", m_path.empty() ? "<none>" : m_path.c_str());
}

bool SessionManager::save(const TabManager& tabManager)
{
    if (m_path.empty()) {
        fprintf(stderr, "[Session] Save skipped — no session path configured\n");
        return false;
    }

    int count = tabManager.tabCount();
    if (count == 0) {
        fprintf(stderr, "[Session] Save skipped — no tabs open\n");
        return false;
    }

    nlohmann::json j;
    j["activeTabIndex"] = tabManager.activeIndex();

    nlohmann::json tabsArray = nlohmann::json::array();
    for (int i = 0; i < count; i++) {
        const TabState* tab = tabManager.getTab(i);
        if (!tab) continue;

        nlohmann::json tabJson;
        tabJson["filePath"]   = tab->filePath;
        tabJson["content"]    = tab->editor->getText();
        tabJson["cursorLine"] = tab->editor->getCursorLine();
        tabJson["cursorCol"]  = tab->editor->getCursorCol();
        tabsArray.push_back(tabJson);
    }
    j["tabs"] = tabsArray;

    // Atomic write: temp file + rename
    std::string tmpPath = m_path + ".tmp";
    {
        std::ofstream file(tmpPath);
        if (!file.is_open()) {
            fprintf(stderr, "[Session] Error: Could not write to %s\n", tmpPath.c_str());
            return false;
        }
        file << j.dump(2) << "\n";
    }

    if (std::rename(tmpPath.c_str(), m_path.c_str()) != 0) {
        fprintf(stderr, "[Session] Error: Could not rename %s -> %s\n",
                tmpPath.c_str(), m_path.c_str());
        // Fallback: direct write
        std::ofstream file(m_path);
        if (file.is_open()) {
            file << j.dump(2) << "\n";
            fprintf(stderr, "[Session] Saved %d tabs to %s (direct write fallback)\n",
                    count, m_path.c_str());
            return true;
        }
        return false;
    }

    fprintf(stderr, "[Session] Saved %d tabs to %s\n", count, m_path.c_str());
    return true;
}

bool SessionManager::load(SessionData& data)
{
    if (m_path.empty()) {
        fprintf(stderr, "[Session] Load skipped — no session path configured\n");
        return false;
    }

    std::ifstream file(m_path);
    if (!file.is_open()) {
        fprintf(stderr, "[Session] Warning: Could not open %s — starting fresh\n", m_path.c_str());
        return false;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(file);

        if (j.contains("activeTabIndex") && j["activeTabIndex"].is_number_integer()) {
            data.activeTabIndex = j["activeTabIndex"].get<int>();
        }

        if (j.contains("tabs") && j["tabs"].is_array()) {
            for (auto& tabJson : j["tabs"]) {
                TabData td;
                if (tabJson.contains("filePath") && tabJson["filePath"].is_string())
                    td.filePath = tabJson["filePath"].get<std::string>();
                if (tabJson.contains("content") && tabJson["content"].is_string())
                    td.content = tabJson["content"].get<std::string>();
                if (tabJson.contains("cursorLine") && tabJson["cursorLine"].is_number_integer())
                    td.cursorLine = tabJson["cursorLine"].get<int>();
                if (tabJson.contains("cursorCol") && tabJson["cursorCol"].is_number_integer())
                    td.cursorCol = tabJson["cursorCol"].get<int>();
                data.tabs.push_back(td);
            }
        }

        if (data.tabs.empty()) {
            fprintf(stderr, "[Session] Warning: Session file has no tabs — starting fresh\n");
            return false;
        }

        fprintf(stderr, "[Session] Loaded %d tabs from %s (active=%d)\n",
                static_cast<int>(data.tabs.size()), m_path.c_str(), data.activeTabIndex);
        return true;
    }
    catch (const nlohmann::json::exception& e) {
        fprintf(stderr, "[Session] Warning: JSON parse error in %s: %s — starting fresh\n",
                m_path.c_str(), e.what());
        return false;
    }
}

} // namespace magnetar
