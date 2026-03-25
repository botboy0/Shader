#pragma once

#include <string>
#include <vector>

#include <glad.h>

namespace magnetar {

/// A single compile error with a 1-based line number in the user's source.
struct CompileError {
    int line;            // 1-based line number in user code
    std::string message; // error message from driver
};

/// Result of a shader compilation attempt.
struct CompileResult {
    bool success;
    GLuint program;      // valid only if success==true; caller owns it
    std::vector<CompileError> errors;
};

/// Compiles GLSL fragment shader source into a linked GL program.
/// Prepends "#version 450 core\n" and pairs with a fixed fullscreen-quad vertex shader.
class ShaderCompiler {
public:
    /// Compile fragment source into a linked program.
    /// lineOffset: number of extra lines prepended before user code (for Shadertoy wrapping in S04).
    /// Error line numbers are adjusted by subtracting lineOffset + 1 (for the #version line).
    CompileResult compile(const std::string& fragmentSource, int lineOffset = 0);
};

} // namespace magnetar
