#pragma once

#include "shader_format.h"

namespace magnetar {

/// Shadertoy-compatible format: wraps user mainImage() code with
/// uniform declarations and a void main() entry point bridge.
class ShadertoyFormat : public IShaderFormat {
public:
    ShadertoyFormat();

    std::string wrapSource(const std::string& userCode) const override;
    int getLineOffset() const override;
    std::vector<std::string> getUniformNames() const override;
    std::string getDefaultSource() const override;

private:
    std::string m_header;
    std::string m_footer;
    int m_headerLineCount;
};

} // namespace magnetar
