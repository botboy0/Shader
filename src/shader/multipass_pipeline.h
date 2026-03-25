#pragma once

#include <glad.h>
#include <array>
#include <cstdio>

namespace magnetar {

class ShaderRenderer;

// ─── Channel / Pass configuration ────────────────────────────────────────────

/// Source for a channel texture binding.
enum class ChannelSource { None, BufferA, BufferB, BufferC, BufferD };

/// Describes which buffer a single channel reads from.
struct ChannelBinding {
    ChannelSource source = ChannelSource::None;
};

/// Configuration for a single rendering pass (buffer or Image).
struct PassConfig {
    GLuint         program  = 0;
    ChannelBinding channels[4] = {};
    bool           active   = false;
};

// ─── MultipassPipeline ──────────────────────────────────────────────────────

/// Manages up to 4 buffer passes (BufferA-D) with ping-pong FBOs for feedback,
/// plus an Image pass that composites to the ShaderRenderer's existing FBO.
///
/// Pass indices: 0=BufferA, 1=BufferB, 2=BufferC, 3=BufferD, 4=Image.
///
/// Lifecycle: init → (setPass / resize / renderAllPasses) → shutdown.
class MultipassPipeline {
public:
    static constexpr int kNumBuffers    = 4;   // A, B, C, D
    static constexpr int kNumPasses     = 5;   // A, B, C, D, Image
    static constexpr int kImagePass     = 4;   // Index of the Image pass

    MultipassPipeline() = default;
    ~MultipassPipeline();

    MultipassPipeline(const MultipassPipeline&) = delete;
    MultipassPipeline& operator=(const MultipassPipeline&) = delete;

    /// Create ping-pong FBO pairs for all 4 buffers.
    /// Returns true if all FBOs are framebuffer-complete.
    bool init(int width, int height);

    /// Destroy and recreate FBOs at a new size. No-op if dimensions unchanged.
    void resize(int width, int height);

    /// Destroy all GL resources.
    void shutdown();

    /// Configure a pass with its compiled program and channel bindings.
    /// passIndex: 0-3 = BufferA-D, 4 = Image.
    /// Sets sampler uniforms (iChannel0..3) to texture units 0..3.
    void setPass(int passIndex, GLuint program, const ChannelBinding channels[4]);

    /// Deactivate a pass. Does NOT delete the program (TabManager owns it).
    void clearPass(int passIndex);

    /// Returns true if any buffer pass (0-3) is active.
    bool isMultipass() const;

    /// Render all active passes in order: BufferA → BufferB → BufferC → BufferD → Image.
    /// Image pass renders to the ShaderRenderer's existing FBO.
    /// quadVao: the fullscreen triangle-strip VAO.
    /// renderer: provides the Image pass render target (FBO) and resolution.
    void renderAllPasses(GLuint quadVao, ShaderRenderer& renderer);

    /// Get the read texture for a buffer (for external inspection / debug).
    GLuint getBufferReadTexture(int bufferIndex) const;

    /// Get the pass config (read-only).
    const PassConfig& getPassConfig(int passIndex) const { return m_passes[passIndex]; }

private:
    /// A single buffer slot with ping-pong FBO pair.
    struct BufferSlot {
        GLuint fbo[2]     = {0, 0};
        GLuint texture[2] = {0, 0};
        int    readIndex   = 0;
        int    writeIndex  = 1;
    };

    void createBufferFbo(GLuint& fbo, GLuint& texture, int width, int height, int bufferIdx, int slotIdx);
    void destroyAllFbos();
    GLuint resolveChannelTexture(int currentPass, int channelIdx) const;

    int m_width  = 0;
    int m_height = 0;
    bool m_initialized = false;
    bool m_firstRender = true;

    std::array<BufferSlot, kNumBuffers> m_buffers = {};
    std::array<PassConfig, kNumPasses>  m_passes  = {};
};

} // namespace magnetar
