#pragma once

#include <string>
#include <vector>

namespace magnetar {

/// Abstract interface for pluggable shader format support (R021).
/// Each format knows how to wrap user source code and report its
/// uniform set and line-offset for error mapping.
class IShaderFormat {
public:
    virtual ~IShaderFormat() = default;

    /// Wrap user-authored shader code into a complete fragment shader body.
    /// The compiler will prepend "#version 450 core\n" separately.
    virtual std::string wrapSource(const std::string& userCode) const = 0;

    /// Number of lines prepended by wrapSource() before the user code starts.
    /// Used by ShaderCompiler to adjust error line numbers back to user space.
    virtual int getLineOffset() const = 0;

    /// Names of all uniforms that this format declares and expects the host to set.
    virtual std::vector<std::string> getUniformNames() const = 0;

    /// Default shader source for a new editor buffer in this format.
    virtual std::string getDefaultSource() const = 0;
};

} // namespace magnetar
