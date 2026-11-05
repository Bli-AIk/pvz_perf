#pragma once
#include "pvz/BuildConfig.h"
#include <string>

namespace Sexy::GpuBackendDetail {
// Keep backend-specific shapes behind the concrete shared rendering interface.
template<BuildConfig::GraphicsApi Selected, class Renderer, class Context, class Hdr>
inline Renderer* CreateRenderer(Context& context, int width, int height,
    const std::string& shaderDirectory, const Hdr* hdr)
{
    if constexpr (Selected == BuildConfig::GraphicsApi::Vulkan) {
        return new Renderer(context, width, height,
            shaderDirectory + "game_vs.spv", shaderDirectory + "game_ps.spv", hdr);
    } else {
        static_assert(Selected == BuildConfig::GraphicsApi::D3d12 ||
            Selected == BuildConfig::GraphicsApi::Metal ||
            Selected == BuildConfig::GraphicsApi::OpenGL ||
            Selected == BuildConfig::GraphicsApi::WebGL ||
            Selected == BuildConfig::GraphicsApi::D3d9);
        return new Renderer(context, width, height, shaderDirectory, hdr);
    }
}

template<BuildConfig::GraphicsApi Selected, class Renderer>
inline auto* CausticsTexture(const Renderer& renderer)
{
    if constexpr (Selected == BuildConfig::GraphicsApi::Vulkan) {
        return renderer.causticsTexture;
    } else {
        static_assert(Selected == BuildConfig::GraphicsApi::D3d12 ||
            Selected == BuildConfig::GraphicsApi::Metal ||
            Selected == BuildConfig::GraphicsApi::OpenGL ||
            Selected == BuildConfig::GraphicsApi::WebGL ||
            Selected == BuildConfig::GraphicsApi::D3d9);
        return renderer.causticsTexture;
    }
}

template<BuildConfig::GraphicsApi Selected, class Image>
inline auto ScaleHandle(const Image& image)
{
    if constexpr (Selected == BuildConfig::GraphicsApi::D3d12)
        return image.resource;
    else if constexpr (Selected == BuildConfig::GraphicsApi::Metal ||
        Selected == BuildConfig::GraphicsApi::OpenGL ||
            Selected == BuildConfig::GraphicsApi::WebGL ||
        Selected == BuildConfig::GraphicsApi::D3d9)
        return image.texture;
    else {
        static_assert(Selected == BuildConfig::GraphicsApi::Vulkan);
        return image.image;
    }
}
}
