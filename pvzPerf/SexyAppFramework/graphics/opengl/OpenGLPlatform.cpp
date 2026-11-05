#include "OpenGLPlatform.h"
#include "OpenGLApi.h"
#include "OpenGLShaders.h"
namespace Sexy {
using namespace GL3;
bool OpenGLPlatform::SupportsFrameSync() { return glFenceSync != nullptr; }
void OpenGLPlatform::InitializeColorState() {
    glDisable(GL_FRAMEBUFFER_SRGB);
    glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
    glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);
}
void OpenGLPlatform::UploadPixels(int width, int height, const void* pixels, int pitch, unsigned int format) {
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, pitch / (format == GL_RED ? 1 : 4));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, format, GL_UNSIGNED_BYTE, pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}
void OpenGLPlatform::PrepareShaderSource(std::string&, std::string_view&) {}
void OpenGLPlatform::BindFragmentOutputs(unsigned int program, bool mixed) {
    if (mixed) {
        glBindFragDataLocationIndexed(program, 0, 0, "outColor");
        glBindFragDataLocationIndexed(program, 0, 1, "blendControl");
    } else glBindFragDataLocation(program, 0, "outColor");
}
std::string_view OpenGLPlatform::MixedSpriteSource() { return MixedSpriteFragmentShader(); }
void OpenGLPlatform::ReadTextureFloats(int width, int height, unsigned int, float* output) {
    glReadPixels(0, 0, width, height, GL_RGBA, GL_FLOAT, output);
}
}
