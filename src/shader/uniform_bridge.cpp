#include "uniform_bridge.h"
#include "../core/renderer.h"

#include <cstdio>
#include <ctime>

namespace magnetar {

void UniformBridge::update(float dt, float resX, float resY, const MouseInput& mouse)
{
    // Advance time
    m_time += dt;
    m_timeDelta = dt;
    m_frame++;

    // Store resolution for apply()
    m_resX = resX;
    m_resY = resY;

    // iMouse state machine
    if (mouse.clicked) {
        m_clickX = mouse.x;
        m_clickY = mouse.y;
        m_wasClicked = true;
    }

    if (mouse.buttonDown && mouse.inPreview) {
        m_mouseX = mouse.x;
        m_mouseY = mouse.y;
        m_mouseDown = true;
    }

    if (!mouse.buttonDown) {
        m_mouseDown = false;
    }
}

void UniformBridge::apply(ShaderRenderer& renderer)
{
    // iTime, iTimeDelta, iFrame
    renderer.setFloat("iTime", m_time);
    renderer.setFloat("iTimeDelta", m_timeDelta);
    renderer.setInt("iFrame", m_frame);

    // iResolution — vec3(width, height, 1.0)
    renderer.setVec3("iResolution", m_resX, m_resY, 1.0f);

    // iMouse — vec4(xy, z, w) with sign-encoded button state
    //   z: positive (click x) while pressed, negative (-click x) after release; 0 if never clicked
    //   w: positive (click y) for one frame on click, then negative (-click y); 0 if never clicked
    float z = m_mouseDown ? m_clickX : -m_clickX;
    float w = m_wasClicked ? m_clickY : -m_clickY;
    renderer.setVec4("iMouse", m_mouseX, m_mouseY, z, w);

    // iDate — vec4(year, month-1, day, secondsSinceMidnight)
    {
        std::time_t now = std::time(nullptr);
        std::tm* t = std::localtime(&now);
        if (t) {
            float year  = static_cast<float>(t->tm_year + 1900);
            float month = static_cast<float>(t->tm_mon);  // 0-based, matches Shadertoy
            float day   = static_cast<float>(t->tm_mday);
            float secs  = static_cast<float>(t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec);
            renderer.setVec4("iDate", year, month, day, secs);
        }
    }

    // Consume one-frame click pulse
    m_wasClicked = false;

    // Log once on first frame
    if (m_firstApply) {
        m_firstApply = false;
        fprintf(stderr, "[UniformBridge] Applied uniforms (iTime=%.2f, iFrame=%d)\n",
                m_time, m_frame);
    }
}

void UniformBridge::applyToProgram(GLuint program)
{
    if (program == 0) return;

    // iTime
    GLint loc = glGetUniformLocation(program, "iTime");
    if (loc >= 0) glProgramUniform1f(program, loc, m_time);

    // iTimeDelta
    loc = glGetUniformLocation(program, "iTimeDelta");
    if (loc >= 0) glProgramUniform1f(program, loc, m_timeDelta);

    // iFrame
    loc = glGetUniformLocation(program, "iFrame");
    if (loc >= 0) glProgramUniform1i(program, loc, m_frame);

    // iResolution — vec3(width, height, 1.0)
    loc = glGetUniformLocation(program, "iResolution");
    if (loc >= 0) glProgramUniform3f(program, loc, m_resX, m_resY, 1.0f);

    // iMouse — vec4(xy, z, w) with sign-encoded button state
    float z = m_mouseDown ? m_clickX : -m_clickX;
    float w = m_wasClicked ? m_clickY : -m_clickY;
    loc = glGetUniformLocation(program, "iMouse");
    if (loc >= 0) glProgramUniform4f(program, loc, m_mouseX, m_mouseY, z, w);

    // iDate
    {
        std::time_t now = std::time(nullptr);
        std::tm* t = std::localtime(&now);
        if (t) {
            float year  = static_cast<float>(t->tm_year + 1900);
            float month = static_cast<float>(t->tm_mon);
            float day   = static_cast<float>(t->tm_mday);
            float secs  = static_cast<float>(t->tm_hour * 3600 + t->tm_min * 60 + t->tm_sec);
            loc = glGetUniformLocation(program, "iDate");
            if (loc >= 0) glProgramUniform4f(program, loc, year, month, day, secs);
        }
    }
}

void UniformBridge::reset()
{
    m_time      = 0.0f;
    m_timeDelta = 0.0f;
    m_frame     = 0;
    m_resX      = 0.0f;
    m_resY      = 0.0f;
    m_mouseX    = 0.0f;
    m_mouseY    = 0.0f;
    m_clickX    = 0.0f;
    m_clickY    = 0.0f;
    m_mouseDown = false;
    m_wasClicked = false;
    m_firstApply = true;  // re-enable log after reset
    fprintf(stderr, "[UniformBridge] State reset\n");
}

} // namespace magnetar
