#include "shadertoy_format.h"

#include <algorithm>
#include <cstdio>

namespace magnetar {

ShadertoyFormat::ShadertoyFormat()
{
    // Build the header prepended before user code.
    // Each line ends with \n. The count of \n characters = number of lines prepended.
    m_header =
        "// --- Shadertoy uniforms (prepended by Magnetar) ---\n"
        "uniform vec3  iResolution;\n"
        "uniform float iTime;\n"
        "uniform float iTimeDelta;\n"
        "uniform int   iFrame;\n"
        "uniform vec4  iMouse;\n"
        "uniform vec4  iDate;\n"
        "uniform sampler2D iChannel0;\n"
        "uniform sampler2D iChannel1;\n"
        "uniform sampler2D iChannel2;\n"
        "uniform sampler2D iChannel3;\n"
        "uniform vec3 iChannelResolution[4];\n"
        "out vec4 outColor;\n"
        "\n";

    m_footer =
        "\n"
        "// --- Entry point (appended by Magnetar) ---\n"
        "void main() {\n"
        "    mainImage(outColor, gl_FragCoord.xy);\n"
        "}\n";

    // Count newlines precisely — this is the line offset for error mapping.
    m_headerLineCount = static_cast<int>(
        std::count(m_header.begin(), m_header.end(), '\n'));
}

std::string ShadertoyFormat::wrapSource(const std::string& userCode) const
{
    fprintf(stderr, "[ShadertoyFormat] Wrapping source (%d header lines)\n", m_headerLineCount);
    return m_header + userCode + m_footer;
}

int ShadertoyFormat::getLineOffset() const
{
    return m_headerLineCount;
}

std::vector<std::string> ShadertoyFormat::getUniformNames() const
{
    return {"iResolution", "iTime", "iTimeDelta", "iFrame", "iMouse", "iDate",
            "iChannel0", "iChannel1", "iChannel2", "iChannel3", "iChannelResolution"};
}

std::string ShadertoyFormat::getDefaultSource() const
{
    return
        "void mainImage(out vec4 fragColor, in vec2 fragCoord)\n"
        "{\n"
        "    vec2 uv = fragCoord / iResolution.xy;\n"
        "\n"
        "    // Animated color cycling\n"
        "    vec3 col = 0.5 + 0.5 * cos(iTime + uv.xyx + vec3(0.0, 2.0, 4.0));\n"
        "\n"
        "    // Subtle wave pattern\n"
        "    float wave = 0.5 + 0.5 * sin(uv.x * 6.2831 + iTime * 1.5)\n"
        "                     * sin(uv.y * 6.2831 + iTime * 0.7);\n"
        "    col = mix(col, col * (0.8 + 0.4 * wave), 0.3);\n"
        "\n"
        "    fragColor = vec4(col, 1.0);\n"
        "}\n";
}

} // namespace magnetar
