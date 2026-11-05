#include "WebGLPlatform.h"
#include "WebGLApi.h"
#include "graphics/gl/GLShaders.h"
#include <stdexcept>
#include <vector>
namespace Sexy {
bool WebGLPlatform::SupportsFrameSync() { return true; }
void WebGLPlatform::InitializeColorState() {}
void WebGLPlatform::UploadPixels(int width, int height, const void* pixels, int pitch, unsigned int format) {
    std::vector<unsigned char> rgba;
    if (format == GL_BGRA) {
        rgba.resize(size_t(width) * height * 4);
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x) {
                const auto* source = static_cast<const unsigned char*>(pixels) + size_t(y) * pitch + x * 4;
                auto* target = rgba.data() + (size_t(y) * width + x) * 4;
                target[0] = source[2]; target[1] = source[1]; target[2] = source[0]; target[3] = source[3];
            }
        pixels = rgba.data(); pitch = width * 4; format = GL_RGBA;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / (format == GL_RED ? 1 : 4));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}
void WebGLPlatform::PrepareShaderSource(std::string& storage, std::string_view& source) {
    storage = GL3::WebShaderSource(source); source = storage;
}
void WebGLPlatform::BindFragmentOutputs(unsigned int, bool mixed) {
    if (mixed) throw std::logic_error("WebGL2 does not support dual-source blending");
}
std::string_view WebGLPlatform::MixedSpriteSource() { return GL3::SpriteFragmentShader(); }
void WebGLPlatform::ReadTextureFloats(int width, int height, unsigned int format, float* output) {
    if (format == GL_RGBA8 || format == GL_R8) {
        std::vector<unsigned char> bytes(size_t(width) * height * 4);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, bytes.data());
        for (size_t i = 0; i < bytes.size(); ++i) output[i] = bytes[i] / 255.0f;
    } else glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, output);
}
}
