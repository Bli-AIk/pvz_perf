#pragma once
#include <string>
#include <string_view>
namespace Sexy {
struct WebGLPlatform {
    static constexpr bool MixedSprites = false;
    static bool SupportsFrameSync();
    static void InitializeColorState();
    static void UploadPixels(int width, int height, const void* pixels, int pitch, unsigned int format);
    static void PrepareShaderSource(std::string& storage, std::string_view& source);
    static void BindFragmentOutputs(unsigned int program, bool mixed);
    static std::string_view MixedSpriteSource();
    static void ReadTextureFloats(int width, int height, unsigned int format, float* output);
};
}
