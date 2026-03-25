#pragma once

#include <string>
#include <vector>

namespace magnetar {

class TabManager;

/// Data for a single tab, used for session serialization.
struct TabData {
    std::string filePath;   // empty = untitled
    std::string content;    // always saved — captures unsaved edits
    int cursorLine = 0;
    int cursorCol  = 0;
};

/// Full session state for persistence.
struct SessionData {
    int activeTabIndex = 0;
    std::vector<TabData> tabs;
};

/// Serializes/deserializes the editing session (all open tabs, content,
/// cursor positions, active tab) to a JSON file in the config directory.
class SessionManager {
public:
    /// Construct with the directory where session.json will live.
    explicit SessionManager(const std::string& sessionDir);

    /// Serialize all tab state from TabManager to session.json.
    /// Uses atomic write (temp file + rename). Returns true on success.
    bool save(const TabManager& tabManager);

    /// Deserialize session.json into SessionData.
    /// Returns false if file is missing, corrupt, or empty — caller should
    /// create a default tab in that case.
    bool load(SessionData& data);

    /// Full path to session.json.
    const std::string& path() const { return m_path; }

private:
    std::string m_path;
};

} // namespace magnetar
