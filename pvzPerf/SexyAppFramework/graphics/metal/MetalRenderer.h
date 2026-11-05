#pragma once
#include "SexyAppFramework/graphics/GpuTypes.h"
#include "SexyAppFramework/graphics/GpuPool.h"
#include "SexyAppFramework/graphics/ImageReadback.h"
#include "SexyAppFramework/graphics/vulkan/VulkanHdr.h"
#include "MetalScaleTypes.h"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace Sexy {
class MetalContext;
struct MetalTexture;
using GpuHdrParams = VulkanHdrParams;

class MetalRenderer final {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    MetalRenderer(MetalContext& context, int sceneWidth, int sceneHeight,
        const std::filesystem::path& shaderDirectory, const GpuHdrParams* hdr = nullptr);
    ~MetalRenderer();
    MetalRenderer(const MetalRenderer&) = delete;
    MetalRenderer& operator=(const MetalRenderer&) = delete;
    void PrepareLoadingPipelines();
    MetalTexture* CreateTexture(int width, int height, const void* bgra, int rowPitch, bool opaque = false);
    MetalTexture* CreateRawTexture(int width, int height, const void* bgra, int rowPitch);
    MetalTexture* ReplaceTexture(MetalTexture* previous, int width, int height,
        const void* bgra, int rowPitch, bool opaque = false);
    MetalTexture* CreateTextureStatic(int width, int height, const void* bgra,
        int rowPitch, bool opaque, uint32_t staticSlot);
    void ReserveStaticTextureSlots(uint32_t count);
    void BindStaticTexture(MetalTexture* texture, uint32_t staticSlot);
    MetalTexture* CreateCoverageMask(int width, int height,
        std::span<const uint8_t> coverage, int rowPitch);
    MetalTexture* CreateRenderImage(int width, int height);
    void SetRenderImage(MetalTexture* image);
    MetalTexture* CopyTextureRegion(MetalTexture* source, int x, int y, int width, int height);
    MetalTexture* ProcessImage(MetalTexture* source, const GpuImageProcessing& processing);
    MetalTexture* ComposeAlphaRaw(MetalTexture* base, MetalTexture* alpha,
        uint32_t color, bool colorOnly);
    MetalTexture* AdoptComposedTexture(MetalTexture* composed);
    MetalTexture* ComposeAlpha(MetalTexture* base, MetalTexture* alpha,
        uint32_t color, bool colorOnly);
    void RenderFontAtlas(MetalTexture* target, MetalTexture* source,
        std::span<const GpuFontGlyph> glyphs, uint32_t samplingFlags, bool forceWhite);
    void DestroyTexture(MetalTexture* texture);
    MetalTexture* const& whiteTexture;
    static uint32_t SlotOf(const MetalTexture* texture);
    MetalTexture* CreateCausticsTexture(int width, int height, MetalTexture* source);
    void UseCausticsTexture(MetalTexture* output, MetalTexture* source);
    void SetCausticsTimeNs(unsigned long long timeNs);
    MetalTexture* const& causticsTexture;
    bool HasCausticsCompute() const;
    bool BeginScene();
    const bool& sceneOpen;
    void SubmitTriangles(std::span<const GpuVertex> vertices, MetalTexture* texture,
        bool linear, bool wrap, GpuBlend blend, const GpuClip* clip = nullptr);
    void SubmitQuads(std::span<const GpuVertex> vertices, MetalTexture* texture,
        bool linear, bool wrap, GpuBlend blend, const GpuClip* clip = nullptr);
    void SubmitCoverageTriangles(std::span<const GpuVertex> vertices,
        MetalTexture* mask, GpuBlend blend, const GpuClip* clip = nullptr);
    void SubmitPool(MetalTexture* texture, const GpuPoolParams& params);
    void EndSceneAndPresent();
    bool PresentRetainedScene();
    bool ResizePresent();
    void ResizeScene(int width, int height);
    void SetHdrParams(const GpuHdrParams& params);
    GpuHdrParams HdrParams() const;
    void PrepareScale(bool fullscreen, std::span<const MetalScaleStep> steps,
        uint32_t targetWidth, uint32_t targetHeight);
    void WarmScale(bool fullscreen);
    void SetScaleMode(bool enabled, bool fullscreen);
    MetalScaleImage ScaleOutput(bool fullscreen) const;
    struct ScaleCounts { size_t programs, lookups, writableImages; };
    ScaleCounts GetScaleCounts() const;
    GpuClip PresentRect(int framebufferWidth, int framebufferHeight) const;
    std::vector<uint32_t> ReadScenePixels();
    ImagePixels ReadTexturePixels(MetalTexture* texture);
    std::vector<uint32_t> ReadScalePixels(bool fullscreen);
    const int& sceneWidth;
    const int& sceneHeight;
    uint32_t MaxImageDimension2D() const;
    struct ResourceCounts { size_t textures, retiredTextures, descriptorPools; };
    ResourceCounts GetResourceCounts() const;
};
}
