#pragma once

#include <string>
#include <vector>

namespace magnetar {

/// Category tag for autocomplete items — drives display styling.
enum class CompletionCategory {
    Function,
    Type,
    Keyword,
    Uniform,
    Qualifier,
    Variable
};

/// A single autocomplete entry: name, display signature, and category.
struct CompletionItem {
    std::string name;
    std::string signature;   // e.g. "float sin(float x)" — shown in popup
    CompletionCategory category;
};

/// Static GLSL autocomplete dictionary with 170+ items.
/// Thread-safe after first call to getItems() (lazy-init singleton).
class GlslAutocomplete {
public:
    /// Returns the full static completion item list (built once).
    static const std::vector<CompletionItem>& getItems();

    /// Case-insensitive prefix filter.  Returns pointers into the static list.
    /// Results are sorted alphabetically by name.
    static std::vector<const CompletionItem*> filter(const std::string& prefix);
};

} // namespace magnetar
