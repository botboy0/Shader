#pragma once

#include <glad.h>

namespace magnetar {

/// Owns shader compilation, FBO management, and fullscreen quad rendering.
/// Renders fragment shaders to an offscreen texture that can be blitted
/// to the default framebuffer or passed to ImGui::Image() via getTextureId().
class ShaderRenderer {
public:
    ShaderRenderer();
    ~ShaderRenderer();

    ShaderRenderer(const ShaderRenderer&) = delete;
    ShaderRenderer& operator=(const ShaderRenderer&) = delete;

    /// Initialize FBO, quad geometry, and compile the default shader.
    /// Returns true on success.
    bool init(int width, int height);

    /// Compile a shader program from vertex + fragment source.
    /// Returns program ID, or 0 on failure (errors logged to stderr).
    GLuint compileShader(const char* vertSrc, const char* fragSrc);

    /// Render the current shader to the FBO.
    /// Returns the FBO color attachment texture ID.
    GLuint render();

    /// Recreate the FBO texture at a new size.
    void resize(int width, int height);

    /// Get the FBO color attachment texture ID (for ImGui::Image in S02).
    GLuint getTextureId() const { return m_colorTex; }

    /// Get the FBO ID (for glBlitNamedFramebuffer).
    GLuint getFboId() const { return m_fbo; }

    /// Get the fullscreen quad VAO (for multipass rendering).
    GLuint getQuadVao() const { return m_vao; }

    int width() const { return m_width; }
    int height() const { return m_height; }

    /// Set a float uniform on the current shader program (DSA — no bind needed).
    void setFloat(const char* name, float value);

    /// Set a vec3 uniform on the current shader program (DSA — no bind needed).
    void setVec3(const char* name, float x, float y, float z);

    /// Set a vec4 uniform on the current shader program (DSA — no bind needed).
    void setVec4(const char* name, float x, float y, float z, float w);

    /// Set an int uniform on the current shader program (DSA — no bind needed).
    void setInt(const char* name, int value);

    /// Replace the active shader program and re-cache uniform locations.
    /// Does NOT delete the old program — program ownership belongs to TabManager
    /// (each TabState owns its compiled GL program). Caller must have compiled+linked newProgram.
    void setProgram(GLuint newProgram);

    /// Get the current shader program ID.
    GLuint getProgram() const { return m_program; }

    // --- Cached uniform location getters (for UniformBridge) ---
    GLint locITime()       const { return m_locITime; }
    GLint locIResolution() const { return m_locIResolution; }
    GLint locITimeDelta()  const { return m_locITimeDelta; }
    GLint locIMouse()      const { return m_locIMouse; }
    GLint locIDate()       const { return m_locIDate; }
    GLint locIFrame()      const { return m_locIFrame; }

    /// Clean up all GL resources.
    void shutdown();

private:
    void createFbo(int width, int height);
    void destroyFbo();
    void createQuad();
    void cacheUniformLocations();

    int    m_width   = 0;
    int    m_height  = 0;

    GLuint m_fbo      = 0;
    GLuint m_colorTex = 0;
    GLuint m_vao      = 0;
    GLuint m_vbo      = 0;
    GLuint m_program  = 0;

    // Cached uniform locations (-1 = not found / optimized out)
    GLint  m_locITime       = -1;
    GLint  m_locIResolution = -1;
    GLint  m_locITimeDelta  = -1;
    GLint  m_locIMouse      = -1;
    GLint  m_locIDate       = -1;
    GLint  m_locIFrame      = -1;
};

} // namespace magnetar
