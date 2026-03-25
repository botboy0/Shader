#include "multipass_parser.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <sstream>

namespace magnetar {

// ─── Helpers ────────────────────────────────────────────────────────────────

namespace {

/// Trim leading and trailing whitespace.
std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return {};
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/// Convert string to lowercase.
std::string toLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

/// Map a lowercase buffer/pass name to an index: buffera=0..bufferd=3, image=4. Returns -1 on failure.
int passNameToIndex(const std::string& name) {
    if (name == "buffera") return 0;
    if (name == "bufferb") return 1;
    if (name == "bufferc") return 2;
    if (name == "bufferd") return 3;
    if (name == "image")   return 4;
    return -1;
}

/// Map a lowercase channel source name to a ChannelSource.
/// "self" is handled by the caller with the current section index.
ChannelSource sourceNameToEnum(const std::string& name) {
    if (name == "buffera") return ChannelSource::BufferA;
    if (name == "bufferb") return ChannelSource::BufferB;
    if (name == "bufferc") return ChannelSource::BufferC;
    if (name == "bufferd") return ChannelSource::BufferD;
    return ChannelSource::None;
}

/// Map a pass index (0-3) to its ChannelSource for "self" resolution.
ChannelSource passIndexToSource(int idx) {
    switch (idx) {
        case 0: return ChannelSource::BufferA;
        case 1: return ChannelSource::BufferB;
        case 2: return ChannelSource::BufferC;
        case 3: return ChannelSource::BufferD;
        default: return ChannelSource::None; // Image has no self-feedback
    }
}

/// Check if a trimmed line is a section marker like "//--- BufferA ---".
/// Returns the pass index (0-4) or -1 if not a marker.
int parseSectionMarker(const std::string& trimmedLine) {
    // Must start with "//---" and end with "---"
    if (trimmedLine.size() < 10) return -1; // minimum: "//--- X ---"
    if (trimmedLine.substr(0, 5) != "//---") return -1;

    auto lastDash = trimmedLine.rfind("---");
    if (lastDash == std::string::npos || lastDash < 5) return -1;

    // Extract the name between the dashes
    std::string inner = trimmedLine.substr(5, lastDash - 5);
    std::string name = toLower(trim(inner));
    return passNameToIndex(name);
}

/// Parse a channel directive like "//!channel0 BufferA" or "//!channel2 self".
/// Returns true if parsed successfully, populating channelIdx and sourceName.
bool parseChannelDirective(const std::string& trimmedLine, int& channelIdx, std::string& sourceName) {
    // Must start with "//!channel"
    const std::string prefix = "//!channel";
    if (trimmedLine.size() < prefix.size() + 2) return false; // need at least digit + space + name
    if (trimmedLine.substr(0, prefix.size()) != "//!channel") return false;

    // Next char must be a digit 0-3
    char digit = trimmedLine[prefix.size()];
    if (digit < '0' || digit > '3') return false;
    channelIdx = digit - '0';

    // Rest is the source name (after whitespace)
    std::string rest = trimmedLine.substr(prefix.size() + 1);
    sourceName = toLower(trim(rest));
    return !sourceName.empty();
}

} // anonymous namespace

// ─── Parser ─────────────────────────────────────────────────────────────────

MultipassSource MultipassParser::parse(const std::string& source)
{
    MultipassSource result;

    // Pointers to the 5 pass data slots for index-based access.
    PassData* passes[5] = {
        &result.buffers[0], &result.buffers[1],
        &result.buffers[2], &result.buffers[3],
        &result.image
    };

    int currentPass = -1; // -1 = no section yet (pre-marker content)
    bool anyMarkerFound = false;
    std::string preMarkerContent; // content before any marker

    std::istringstream stream(source);
    std::string line;
    int lineNum = 0; // 1-based line number in the editor text

    while (std::getline(stream, line)) {
        lineNum++;
        std::string trimmedLine = trim(line);

        // Check for section marker
        int markerIdx = parseSectionMarker(trimmedLine);
        if (markerIdx >= 0) {
            anyMarkerFound = true;
            currentPass = markerIdx;
            // The next line is where the pass source starts
            passes[currentPass]->active = true;
            passes[currentPass]->startLine = lineNum + 1; // source begins after the marker line
            continue;
        }

        // Check for channel directive (only meaningful within a section)
        if (currentPass >= 0) {
            int channelIdx = 0;
            std::string srcName;
            if (parseChannelDirective(trimmedLine, channelIdx, srcName)) {
                if (srcName == "self") {
                    passes[currentPass]->channels[channelIdx].source = passIndexToSource(currentPass);
                } else {
                    passes[currentPass]->channels[channelIdx].source = sourceNameToEnum(srcName);
                }
                continue; // directives are not included in the pass source
            }
        }

        // Accumulate source lines
        if (currentPass >= 0) {
            passes[currentPass]->source += line + "\n";
        } else {
            preMarkerContent += line + "\n";
        }
    }

    if (anyMarkerFound) {
        result.isMultipass = true;

        // Pre-marker content (before any //--- marker) is prepended to Image pass
        // if there is any. This handles shared helper functions declared before sections.
        if (!preMarkerContent.empty() && result.image.active) {
            result.image.source = preMarkerContent + result.image.source;
            // Adjust startLine since pre-marker content pushed the effective start earlier
            result.image.startLine = 1;
        }

        // Count active passes for logging
        int activeBuffers[4] = {};
        for (int i = 0; i < 4; ++i) {
            activeBuffers[i] = result.buffers[i].active ? 1 : 0;
        }
        int totalActive = activeBuffers[0] + activeBuffers[1] + activeBuffers[2] + activeBuffers[3]
                        + (result.image.active ? 1 : 0);

        fprintf(stderr, "[MultipassParser] Parsed %d passes (buffers: A=%d B=%d C=%d D=%d, image=%d)\n",
                totalActive, activeBuffers[0], activeBuffers[1], activeBuffers[2], activeBuffers[3],
                result.image.active ? 1 : 0);
    } else {
        // No markers found → single-pass backward-compatible mode
        result.isMultipass = false;
        result.image.source    = source;
        result.image.active    = true;
        result.image.startLine = 1;

        fprintf(stderr, "[MultipassParser] No markers found — single-pass mode\n");
    }

    return result;
}

} // namespace magnetar
