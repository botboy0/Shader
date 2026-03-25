#include "renderer.h"

#include <cstdio>
#include <cstring>

namespace magnetar {

// ─── Default shaders ─────────────────────────────────────────────────────────

static const char* kDefaultVertexShader = R"glsl(
#version 450 core
layout(location = 0) in vec2 aPosition;

out vec2 fragCoord;

void main()
{
    fragCoord   = aPosition * 0.5 + 0.5; // map [-1,1] → [0,1]
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)glsl";

static const char* kDefaultFragmentShader = R"glsl(
#version 450 core
in vec2 fragCoord;
out vec4 outColor;

uniform float iTime;
uniform vec3  iResolution;

void main()
{
    vec2 uv = gl_FragCoord.xy / iResolution.xy;

    // Animated color cycling — smooth RGB phase offsets driven by time
    vec3 col = 0.5 + 0.5 * cos(iTime + uv.xyx + vec3(0.0, 2.0, 4.0));

    // Add a subtle moving wave pattern for visual interest
    float wave = 0.5 + 0.5 * sin(uv.x * 6.2831 + iTime * 1.5)
                     * sin(uv.y * 6.2831 + iTime * 0.7);
    col = mix(col, col * (0.8 + 0.4 * wave), 0.3);

    outColor = vec4(col, 1.0);
}
)glsl";

// ─── Fullscreen quad geometry (triangle strip: 4 vertices) ───────────────────

static const float kQuadVertices[] = {
    -1.0f, -1.0f,
     1.0f, -1.0f,
    -1.0f,  1.0f,
     1.0f,  1.0f,
};

// ─── ShaderRenderer implementation ───────────────────────────────────────────

ShaderRenderer::ShaderRenderer() = default;

ShaderRenderer::~ShaderRenderer()
{
    shutdown();
}

bool ShaderRenderer::init(int width, int height)
{
    m_width  = width;
    m_height = height;

    // Create fullscreen quad
    createQuad();

    // Create FBO
    createFbo(width, height);
    if (m_fbo == 0) {
        return false;
    }

    // Compile default shader
    m_program = compileShader(kDefaultVertexShader, kDefaultFragmentShader);
    if (m_program == 0) {
        fprintf(stderr, "[Renderer] Error: Default shader compilation failed\n");
        return false;
    }

    // Cache well-known uniform locations
    cacheUniformLocations();

    fprintf(stderr, "[Renderer] Initialized (%dx%d)\n", width, height);
    return true;
}

GLuint ShaderRenderer::compileShader(const char* vertSrc, const char* fragSrc)
{
    GLint success = 0;
    char infoLog[1024];

    // --- Vertex shader ---
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &vertSrc, nullptr);
    glCompileShader(vert);
    glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vert, sizeof(infoLog), nullptr, infoLog);
        fprintf(stderr, "[Renderer] Error: Vertex shader compile failed:\n%s\n", infoLog);
        glDeleteShader(vert);
        return 0;
    }

    // --- Fragment shader ---
    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(frag, 1, &fragSrc, nullptr);
    glCompileShader(frag);
    glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(frag, sizeof(infoLog), nullptr, infoLog);
        fprintf(stderr, "[Renderer] Error: Fragment shader compile failed:\n%s\n", infoLog);
        glDeleteShader(vert);
        glDeleteShader(frag);
        return 0;
    }

    // --- Link program ---
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        fprintf(stderr, "[Renderer] Error: Shader link failed:\n%s\n", infoLog);
        glDeleteShader(vert);
        glDeleteShader(frag);
        glDeleteProgram(program);
        return 0;
    }

    // Shaders can be detached and deleted after linking
    glDetachShader(program, vert);
    glDetachShader(program, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);

    fprintf(stderr, "[Renderer] Shader compiled successfully (program=%u)\n", program);
    return program;
}

GLuint ShaderRenderer::render()
{
    // Bind FBO and render the shader to it
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glViewport(0, 0, m_width, m_height);

    glUseProgram(m_program);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glUseProgram(0);

    // Unbind FBO — leave default framebuffer active
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return m_colorTex;
}

void ShaderRenderer::resize(int width, int height)
{
    if (width <= 0 || height <= 0) return;
    if (width == m_width && height == m_height) return;

    m_width  = width;
    m_height = height;

    // Recreate FBO at new size
    destroyFbo();
    createFbo(width, height);

    fprintf(stderr, "[Renderer] Resized FBO to %dx%d\n", width, height);
}

void ShaderRenderer::setProgram(GLuint newProgram)
{
    // NOTE: The renderer does NOT own the program — TabManager does.
    // We just point to the new program and re-cache uniform locations.
    m_program = newProgram;
    cacheUniformLocations();
    fprintf(stderr, "[Renderer] Program swapped (program=%u)\n", newProgram);
}

void ShaderRenderer::shutdown()
{
    if (!m_program && !m_vao && !m_vbo && !m_fbo && !m_colorTex) return; // already shut down

    // NOTE: Do NOT delete m_program here — TabManager owns the GL programs.
    // We just clear our reference.
    m_program = 0;

    destroyFbo();
    if (m_vbo)     { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_vao)     { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    fprintf(stderr, "[Renderer] Shutdown complete\n");
}

// ─── Private helpers ─────────────────────────────────────────────────────────

void ShaderRenderer::createFbo(int width, int height)
{
    // Create color texture via DSA
    glCreateTextures(GL_TEXTURE_2D, 1, &m_colorTex);
    glTextureStorage2D(m_colorTex, 1, GL_RGBA8, width, height);
    glTextureParameteri(m_colorTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(m_colorTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(m_colorTex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(m_colorTex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create FBO and attach texture via DSA
    glCreateFramebuffers(1, &m_fbo);
    glNamedFramebufferTexture(m_fbo, GL_COLOR_ATTACHMENT0, m_colorTex, 0);

    // Check completeness
    GLenum status = glCheckNamedFramebufferStatus(m_fbo, GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "[Renderer] Error: FBO incomplete (status=0x%X)\n", status);
        destroyFbo();
        return;
    }

    fprintf(stderr, "[Renderer] FBO created (%dx%d, tex=%u, fbo=%u)\n",
            width, height, m_colorTex, m_fbo);
}

void ShaderRenderer::destroyFbo()
{
    if (m_colorTex) { glDeleteTextures(1, &m_colorTex); m_colorTex = 0; }
    if (m_fbo)      { glDeleteFramebuffers(1, &m_fbo);  m_fbo = 0; }
}

void ShaderRenderer::createQuad()
{
    // Create VAO + VBO via DSA
    glCreateVertexArrays(1, &m_vao);
    glCreateBuffers(1, &m_vbo);

    // Upload vertex data (immutable storage)
    glNamedBufferStorage(m_vbo, sizeof(kQuadVertices), kQuadVertices, 0);

    // Bind VBO to VAO binding point 0
    glVertexArrayVertexBuffer(m_vao, 0, m_vbo, 0, 2 * sizeof(float));

    // Position attribute: location=0, 2 floats
    glEnableVertexArrayAttrib(m_vao, 0);
    glVertexArrayAttribFormat(m_vao, 0, 2, GL_FLOAT, GL_FALSE, 0);
    glVertexArrayAttribBinding(m_vao, 0, 0);

    fprintf(stderr, "[Renderer] Fullscreen quad created (vao=%u, vbo=%u)\n", m_vao, m_vbo);
}

void ShaderRenderer::cacheUniformLocations()
{
    if (m_program == 0) return;

    m_locITime       = glGetUniformLocation(m_program, "iTime");
    m_locIResolution = glGetUniformLocation(m_program, "iResolution");
    m_locITimeDelta  = glGetUniformLocation(m_program, "iTimeDelta");
    m_locIMouse      = glGetUniformLocation(m_program, "iMouse");
    m_locIDate       = glGetUniformLocation(m_program, "iDate");
    m_locIFrame      = glGetUniformLocation(m_program, "iFrame");

    fprintf(stderr, "[Renderer] Uniform locations: iTime=%d, iResolution=%d, iTimeDelta=%d, iMouse=%d, iDate=%d, iFrame=%d\n",
            m_locITime, m_locIResolution, m_locITimeDelta, m_locIMouse, m_locIDate, m_locIFrame);
}

void ShaderRenderer::setFloat(const char* name, float value)
{
    if (m_program == 0) return;
    GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) {
        glProgramUniform1f(m_program, loc, value);
    }
}

void ShaderRenderer::setVec3(const char* name, float x, float y, float z)
{
    if (m_program == 0) return;
    GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) {
        glProgramUniform3f(m_program, loc, x, y, z);
    }
}

void ShaderRenderer::setVec4(const char* name, float x, float y, float z, float w)
{
    if (m_program == 0) return;
    GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) {
        glProgramUniform4f(m_program, loc, x, y, z, w);
    }
}

void ShaderRenderer::setInt(const char* name, int value)
{
    if (m_program == 0) return;
    GLint loc = glGetUniformLocation(m_program, name);
    if (loc >= 0) {
        glProgramUniform1i(m_program, loc, value);
    }
}

} // namespace magnetar
