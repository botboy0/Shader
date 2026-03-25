#include "multipass_pipeline.h"
#include "core/renderer.h"

#include <cstdio>
#include <cstring>

namespace magnetar {

// ─── Lifecycle ──────────────────────────────────────────────────────────────

MultipassPipeline::~MultipassPipeline()
{
    shutdown();
}

bool MultipassPipeline::init(int width, int height)
{
    if (m_initialized) {
        fprintf(stderr, "[MultipassPipeline] Already initialized, call shutdown() first\n");
        return false;
    }

    m_width  = width;
    m_height = height;

    // Create ping-pong FBO pairs for all 4 buffers (8 FBOs total).
    bool allComplete = true;
    for (int b = 0; b < kNumBuffers; ++b) {
        for (int s = 0; s < 2; ++s) {
            createBufferFbo(m_buffers[b].fbo[s], m_buffers[b].texture[s], width, height, b, s);
            if (m_buffers[b].fbo[s] == 0) {
                allComplete = false;
            }
        }
        m_buffers[b].readIndex  = 0;
        m_buffers[b].writeIndex = 1;
    }

    if (!allComplete) {
        fprintf(stderr, "[MultipassPipeline] Error: Some FBOs failed completeness check\n");
        destroyAllFbos();
        return false;
    }

    m_initialized = true;
    m_firstRender = true;
    fprintf(stderr, "[MultipassPipeline] Created 4 buffer slots (8 FBOs) at %dx%d\n", width, height);
    return true;
}

void MultipassPipeline::resize(int width, int height)
{
    if (width <= 0 || height <= 0) return;
    if (width == m_width && height == m_height) return;

    // Destroy and recreate at the new size.
    bool wasInitialized = m_initialized;
    destroyAllFbos();
    m_initialized = false;

    if (wasInitialized) {
        init(width, height);
        fprintf(stderr, "[MultipassPipeline] Resized to %dx%d\n", width, height);
    }
}

void MultipassPipeline::shutdown()
{
    if (!m_initialized) return;

    destroyAllFbos();

    // Clear pass configs (do NOT delete programs — TabManager owns them).
    for (auto& pass : m_passes) {
        pass.program = 0;
        pass.active  = false;
        for (auto& ch : pass.channels) {
            ch.source = ChannelSource::None;
        }
    }

    m_width  = 0;
    m_height = 0;
    m_initialized = false;
    m_firstRender = true;
    fprintf(stderr, "[MultipassPipeline] Shutdown\n");
}

// ─── Pass configuration ─────────────────────────────────────────────────────

void MultipassPipeline::setPass(int passIndex, GLuint program, const ChannelBinding channels[4])
{
    if (passIndex < 0 || passIndex >= kNumPasses) {
        fprintf(stderr, "[MultipassPipeline] Error: Invalid pass index %d\n", passIndex);
        return;
    }

    auto& pass   = m_passes[passIndex];
    pass.program = program;
    pass.active  = (program != 0);
    for (int i = 0; i < 4; ++i) {
        pass.channels[i] = channels[i];
    }

    // Set sampler uniforms iChannel0..3 → texture units 0..3 (DSA, no bind needed).
    if (program != 0) {
        const char* samplerNames[4] = {"iChannel0", "iChannel1", "iChannel2", "iChannel3"};
        for (int i = 0; i < 4; ++i) {
            GLint loc = glGetUniformLocation(program, samplerNames[i]);
            if (loc >= 0) {
                glProgramUniform1i(program, loc, i);
            }
        }

        const char* passNames[5] = {"BufferA", "BufferB", "BufferC", "BufferD", "Image"};
        fprintf(stderr, "[MultipassPipeline] Pass %s configured (program=%u)\n",
                passNames[passIndex], program);
    }
}

void MultipassPipeline::clearPass(int passIndex)
{
    if (passIndex < 0 || passIndex >= kNumPasses) return;

    auto& pass   = m_passes[passIndex];
    pass.program = 0;
    pass.active  = false;
    for (auto& ch : pass.channels) {
        ch.source = ChannelSource::None;
    }
}

bool MultipassPipeline::isMultipass() const
{
    for (int i = 0; i < kNumBuffers; ++i) {
        if (m_passes[i].active) return true;
    }
    return false;
}

// ─── Rendering ──────────────────────────────────────────────────────────────

void MultipassPipeline::renderAllPasses(GLuint quadVao, ShaderRenderer& renderer)
{
    if (!m_initialized) return;

    int activeCount = 0;
    for (const auto& p : m_passes) {
        if (p.active) ++activeCount;
    }

    if (m_firstRender) {
        fprintf(stderr, "[MultipassPipeline] Rendering %d active passes\n", activeCount);
        m_firstRender = false;
    }

    // --- Buffer passes (A=0, B=1, C=2, D=3) ---
    for (int b = 0; b < kNumBuffers; ++b) {
        const auto& pass = m_passes[b];
        if (!pass.active) continue;

        auto& slot = m_buffers[b];

        // Bind write FBO
        glBindFramebuffer(GL_FRAMEBUFFER, slot.fbo[slot.writeIndex]);
        glViewport(0, 0, m_width, m_height);

        // Bind channel textures (DSA)
        for (int ch = 0; ch < 4; ++ch) {
            GLuint tex = resolveChannelTexture(b, ch);
            glBindTextureUnit(ch, tex);
        }

        // Set iChannelResolution uniform for each channel
        GLint locChanRes = glGetUniformLocation(pass.program, "iChannelResolution");
        if (locChanRes >= 0) {
            float chanRes[12] = {}; // vec3[4] = 12 floats
            for (int ch = 0; ch < 4; ++ch) {
                GLuint tex = resolveChannelTexture(b, ch);
                if (tex != 0) {
                    chanRes[ch * 3 + 0] = static_cast<float>(m_width);
                    chanRes[ch * 3 + 1] = static_cast<float>(m_height);
                    chanRes[ch * 3 + 2] = 1.0f;
                }
            }
            glProgramUniform3fv(pass.program, locChanRes, 4, chanRes);
        }

        // Render
        glUseProgram(pass.program);
        glBindVertexArray(quadVao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Swap ping-pong indices
        slot.readIndex  = slot.writeIndex;
        slot.writeIndex = 1 - slot.writeIndex;
    }

    // --- Image pass (index 4) ---
    if (m_passes[kImagePass].active) {
        const auto& pass = m_passes[kImagePass];

        // Render to the ShaderRenderer's existing FBO
        glBindFramebuffer(GL_FRAMEBUFFER, renderer.getFboId());
        glViewport(0, 0, renderer.width(), renderer.height());

        // Bind channel textures
        for (int ch = 0; ch < 4; ++ch) {
            GLuint tex = resolveChannelTexture(kImagePass, ch);
            glBindTextureUnit(ch, tex);
        }

        // Set iChannelResolution for Image pass
        GLint locChanRes = glGetUniformLocation(pass.program, "iChannelResolution");
        if (locChanRes >= 0) {
            float chanRes[12] = {};
            for (int ch = 0; ch < 4; ++ch) {
                GLuint tex = resolveChannelTexture(kImagePass, ch);
                if (tex != 0) {
                    chanRes[ch * 3 + 0] = static_cast<float>(m_width);
                    chanRes[ch * 3 + 1] = static_cast<float>(m_height);
                    chanRes[ch * 3 + 2] = 1.0f;
                }
            }
            glProgramUniform3fv(pass.program, locChanRes, 4, chanRes);
        }

        // Render
        glUseProgram(pass.program);
        glBindVertexArray(quadVao);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    // Unbind
    glBindVertexArray(0);
    glUseProgram(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Unbind texture units
    for (int ch = 0; ch < 4; ++ch) {
        glBindTextureUnit(ch, 0);
    }
}

GLuint MultipassPipeline::getBufferReadTexture(int bufferIndex) const
{
    if (bufferIndex < 0 || bufferIndex >= kNumBuffers) return 0;
    const auto& slot = m_buffers[bufferIndex];
    return slot.texture[slot.readIndex];
}

// ─── Private helpers ────────────────────────────────────────────────────────

void MultipassPipeline::createBufferFbo(GLuint& fbo, GLuint& texture,
                                         int width, int height,
                                         int bufferIdx, int slotIdx)
{
    // Create color texture via DSA (matches ShaderRenderer::createFbo pattern)
    glCreateTextures(GL_TEXTURE_2D, 1, &texture);
    glTextureStorage2D(texture, 1, GL_RGBA8, width, height);
    glTextureParameteri(texture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Create FBO and attach via DSA
    glCreateFramebuffers(1, &fbo);
    glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, texture, 0);

    // Completeness check
    GLenum status = glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "[MultipassPipeline] Error: FBO incomplete for buffer %d slot %d (status=0x%X)\n",
                bufferIdx, slotIdx, status);
        glDeleteTextures(1, &texture);
        glDeleteFramebuffers(1, &fbo);
        texture = 0;
        fbo     = 0;
        return;
    }

    // Clear the texture to black (prevents reading garbage on first frame)
    float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    glClearNamedFramebufferfv(fbo, GL_COLOR, 0, clearColor);
}

void MultipassPipeline::destroyAllFbos()
{
    for (auto& slot : m_buffers) {
        for (int s = 0; s < 2; ++s) {
            if (slot.texture[s]) { glDeleteTextures(1, &slot.texture[s]); slot.texture[s] = 0; }
            if (slot.fbo[s])     { glDeleteFramebuffers(1, &slot.fbo[s]); slot.fbo[s] = 0; }
        }
        slot.readIndex  = 0;
        slot.writeIndex = 1;
    }
}

GLuint MultipassPipeline::resolveChannelTexture(int currentPass, int channelIdx) const
{
    const auto& binding = m_passes[currentPass].channels[channelIdx];

    switch (binding.source) {
        case ChannelSource::None:
            return 0;
        case ChannelSource::BufferA:
        case ChannelSource::BufferB:
        case ChannelSource::BufferC:
        case ChannelSource::BufferD: {
            int bufIdx = static_cast<int>(binding.source) - static_cast<int>(ChannelSource::BufferA);
            const auto& slot = m_buffers[bufIdx];
            // Always read from the read index — for self-referencing (feedback),
            // this is the previous frame's output before the swap.
            return slot.texture[slot.readIndex];
        }
    }

    return 0;
}

} // namespace magnetar
