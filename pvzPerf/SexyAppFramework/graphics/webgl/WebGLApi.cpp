#include "WebGLApi.h"
#include <stdexcept>
namespace Sexy::GL3 {
void CheckError(const char* operation) {
    const auto error = glGetError();
    if (error != GL_NO_ERROR) throw std::runtime_error(std::string(operation) + ": GL error " + std::to_string(error));
}
std::string WebShaderSource(std::string_view source) {
    std::string text(source);
    const auto line = text.find('\n');
    text.replace(0, line, "#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp sampler2D;\nprecision highp usampler2D;");
    return text;
}
}
