#pragma once

#include "multipass_pipeline.h"  // ChannelSource, ChannelBinding

#include <array>
#include <string>

namespace magnetar {

/// Parsed data for a single rendering pass (buffer or Image).
struct PassData {
    std::string    source;          ///< The pass's GLSL code (without markers/directives).
    ChannelBinding channels[4] = {};///< Channel bindings from //!channelN directives.
    bool           active = false;  ///< True if this pass has source code.
    int            startLine = 0;   ///< 1-based line in editor text where this pass's source begins.
};

/// Result of parsing a multipass source string.
struct MultipassSource {
    PassData buffers[4] = {};  ///< BufferA (0), BufferB (1), BufferC (2), BufferD (3).
    PassData image;            ///< Image pass.
    bool     isMultipass = false; ///< True if any //--- markers were found.
};

/// Splits a single editor source string into per-pass code blocks.
///
/// Section markers:  `//--- BufferA ---`, `//--- Image ---`, etc. (case-insensitive).
/// Channel directives within a section: `//!channel0 BufferA`, `//!channel0 self`.
///
/// If no markers are found, the entire source becomes the Image pass (backward compatible).
/// Render order is always A → B → C → D → Image regardless of section order in source.
class MultipassParser {
public:
    /// Parse source text into per-pass blocks with channel bindings.
    static MultipassSource parse(const std::string& source);
};

} // namespace magnetar
