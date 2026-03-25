#include "compiler.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

namespace magnetar {

// ─── Fixed vertex shader (same as renderer.cpp kDefaultVertexShader) ─────────

static const char* kVertexShader = R"glsl(
#version 450 core
layout(location = 0) in vec2 aPosition;

out vec2 fragCoord;

void main()
{
    fragCoord   = aPosition * 0.5 + 0.5; // map [-1,1] → [0,1]
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)glsl";

// ─── Error line parsing ──────────────────────────────────────────────────────

/// Try to extract a line number from a driver error line.
/// Handles NVIDIA format "0(N)" and Mesa/AMD format "0:N".
/// Returns -1 if no line number found.
static int parseLineNumber(const std::string& line)
{
    // NVIDIA: "0(123) : error ..."
    {
        auto pos = line.find("0(");
        if (pos != std::string::npos) {
            pos += 2; // skip "0("
            int num = 0;
            bool found = false;
            while (pos < line.size() && line[pos] >= '0' && line[pos] <= '9') {
                num = num * 10 + (line[pos] - '0');
                found = true;
                pos++;
            }
            if (found && pos < line.size() && line[pos] == ')') {
                return num;
            }
        }
    }

    // Mesa/AMD: "0:123: error ..." or "0:123(45): error ..."
    {
        auto pos = line.find("0:");
        if (pos != std::string::npos) {
            pos += 2; // skip "0:"
            int num = 0;
            bool found = false;
            while (pos < line.size() && line[pos] >= '0' && line[pos] <= '9') {
                num = num * 10 + (line[pos] - '0');
                found = true;
                pos++;
            }
            if (found) {
                return num;
            }
        }
    }

    return -1;
}

/// Parse a shader info log into structured errors.
/// totalOffset = number of lines prepended before user code (1 for #version + lineOffset).
static std::vector<CompileError> parseInfoLog(const char* log, int totalOffset)
{
    std::vector<CompileError> errors;
    if (!log || log[0] == '\0') return errors;

    std::istringstream stream(log);
    std::string line;
    while (std::getline(stream, line)) {
        // Skip empty lines
        if (line.empty()) continue;
        // Skip lines that are purely whitespace
        bool allSpace = true;
        for (char c : line) {
            if (c != ' ' && c != '\t' && c != '\r') { allSpace = false; break; }
        }
        if (allSpace) continue;

        int rawLine = parseLineNumber(line);
        if (rawLine > 0) {
            int userLine = rawLine - totalOffset;
            if (userLine < 1) userLine = 1;
            errors.push_back({userLine, line});
        } else {
            // No line number found — attach to line 1
            errors.push_back({1, line});
        }
    }
    return errors;
}

// ─── ShaderCompiler implementation ───────────────────────────────────────────

CompileResult ShaderCompiler::compile(const std::string& fragmentSource, int lineOffset)
{
    // Number of lines prepended: 1 for "#version 450 core\n" + lineOffset
    int totalOffset = 1 + lineOffset;

    // Build the full fragment source with version directive
    std::string fullFrag = "#version 450 core\n" + fragmentSource;

    GLint success = 0;
    char infoLog[2048];

    // --- Vertex shader ---
    GLuint vert = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vert, 1, &kVertexShader, nullptr);
    glCompileShader(vert);
    glGetShaderiv(vert, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vert, sizeof(infoLog), nullptr, infoLog);
        fprintf(stderr, "[Compiler] Compile failed: vertex shader error\n");
        fprintf(stderr, "[Compiler] Error line 1: %s\n", infoLog);
        glDeleteShader(vert);
        return {false, 0, {{1, std::string(infoLog)}}};
    }

    // --- Fragment shader ---
    GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fragSrc = fullFrag.c_str();
    glShaderSource(frag, 1, &fragSrc, nullptr);
    glCompileShader(frag);
    glGetShaderiv(frag, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(frag, sizeof(infoLog), nullptr, infoLog);
        auto errors = parseInfoLog(infoLog, totalOffset);
        fprintf(stderr, "[Compiler] Compile failed: %d error(s)\n", (int)errors.size());
        for (auto& e : errors) {
            fprintf(stderr, "[Compiler] Error line %d: %s\n", e.line, e.message.c_str());
        }
        glDeleteShader(vert);
        glDeleteShader(frag);
        return {false, 0, std::move(errors)};
    }

    // --- Link program ---
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(program, sizeof(infoLog), nullptr, infoLog);
        auto errors = parseInfoLog(infoLog, totalOffset);
        if (errors.empty()) {
            // Link errors don't always have line numbers
            errors.push_back({1, std::string(infoLog)});
        }
        fprintf(stderr, "[Compiler] Compile failed: %d error(s) (link stage)\n", (int)errors.size());
        for (auto& e : errors) {
            fprintf(stderr, "[Compiler] Error line %d: %s\n", e.line, e.message.c_str());
        }
        glDetachShader(program, vert);
        glDetachShader(program, frag);
        glDeleteShader(vert);
        glDeleteShader(frag);
        glDeleteProgram(program);
        return {false, 0, std::move(errors)};
    }

    // Clean up shaders after successful link
    glDetachShader(program, vert);
    glDetachShader(program, frag);
    glDeleteShader(vert);
    glDeleteShader(frag);

    fprintf(stderr, "[Compiler] Compile success (program=%u)\n", program);
    return {true, program, {}};
}

} // namespace magnetar
