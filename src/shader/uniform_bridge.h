#pragma once

#include <glad.h>

namespace magnetar {

class ShaderRenderer;  // forward declaration

/// Mouse input state from the UI layer, consumed by UniformBridge each frame.
struct MouseInput {
    float x = 0.0f;       ///< Preview-relative X (shader pixel space, 0 = left)
    float y = 0.0f;       ///< Preview-relative Y (shader pixel space, 0 = bottom)
    bool  inPreview = false;  ///< True if cursor is over the preview pane
    bool  buttonDown = false; ///< True if left mouse button is currently held
    bool  clicked = false;    ///< True for exactly one frame when button is pressed
};

/// Manages per-frame Shadertoy uniform state (time, resolution, mouse, date, frame count)
/// and uploads them to a ShaderRenderer via its string-based uniform setters.
///
/// The iMouse state machine follows simplified Shadertoy semantics:
///   .xy — cursor position while dragging, else last drag position (0,0 if never clicked)
///   .z  — positive (click x) while button down, negative (-click x) after release
///   .w  — positive (click y) for one frame on click, then negative (-click y)
class UniformBridge {
public:
    UniformBridge() = default;
    ~UniformBridge() = default;

    /// Advance time, delta time, frame count, and iMouse state machine.
    /// Call once per frame before apply().
    void update(float dt, float resX, float resY, const MouseInput& mouse);

    /// Upload all 6 Shadertoy uniforms to the renderer.
    /// Call after update(), before renderer.render().
    void apply(ShaderRenderer& renderer);

    /// Upload all Shadertoy uniforms directly to an arbitrary GL program (DSA).
    /// Used by the multipass pipeline to set uniforms on each pass's program.
    void applyToProgram(GLuint program);

    /// Reset all state (time, frame count, mouse). Use on shader change or tab switch.
    void reset();

private:
    float m_time      = 0.0f;
    float m_timeDelta = 0.0f;
    int   m_frame     = 0;

    float m_resX = 0.0f;
    float m_resY = 0.0f;

    // iMouse state machine
    float m_mouseX    = 0.0f;  ///< Last known cursor X (drag position)
    float m_mouseY    = 0.0f;  ///< Last known cursor Y (drag position)
    float m_clickX    = 0.0f;  ///< X at last click (for z/w sign encoding)
    float m_clickY    = 0.0f;  ///< Y at last click (for z/w sign encoding)
    bool  m_mouseDown = false;
    bool  m_wasClicked = false; ///< One-frame pulse: true on click frame, consumed in apply()

    bool  m_firstApply = true;  ///< Log on first apply() call only
};

} // namespace magnetar
