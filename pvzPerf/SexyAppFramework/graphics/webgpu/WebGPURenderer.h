#pragma once
#include "WebGPUContext.h"
#include "WebGPUScaleTypes.h"
#include "graphics/GpuPool.h"
#include "graphics/GpuTypes.h"
#include "graphics/ImageReadback.h"
#include "graphics/vulkan/VulkanHdr.h"
#include <cstddef>
#include <filesystem>
#include <span>
#include <vector>

namespace Sexy
{
struct WebGPUTexture;
class WebGPURenderer final
{
    struct Impl;
    Impl *impl_;

  public:
    WebGPURenderer(WebGPUContext &context, int width, int height, const std::filesystem::path &shaderDirectory,
                   const VulkanHdrParams *hdr = nullptr);
    ~WebGPURenderer();
    WebGPURenderer(const WebGPURenderer &) = delete;
    WebGPURenderer &operator=(const WebGPURenderer &) = delete;
    void PrepareLoadingPipelines();
    WebGPUTexture *CreateTexture(int width, int height, const void *bgra, int pitch, bool opaque = false);
    WebGPUTexture *CreateTextureStatic(int width, int height, const void *bgra, int pitch, bool opaque,
                                       unsigned int slot);
    WebGPUTexture *CreateRawTexture(int width, int height, const void *bgra, int pitch);
    WebGPUTexture *ReplaceTexture(WebGPUTexture *previous, int width, int height, const void *bgra, int pitch,
                                  bool opaque = false);
    void ReserveStaticTextureSlots(unsigned int count);
    void BindStaticTexture(WebGPUTexture *texture, unsigned int slot);
    WebGPUTexture *CreateCoverageMask(int width, int height, std::span<const unsigned char> coverage, int pitch);
    WebGPUTexture *CreateRenderImage(int width, int height);
    void SetRenderImage(WebGPUTexture *texture);
    WebGPUTexture *CopyTextureRegion(WebGPUTexture *source, int x, int y, int width, int height);
    WebGPUTexture *ProcessImage(WebGPUTexture *source, const GpuImageProcessing &processing);
    WebGPUTexture *ComposeAlphaRaw(WebGPUTexture *base, WebGPUTexture *alpha, unsigned int color, bool colorOnly);
    WebGPUTexture *AdoptComposedTexture(WebGPUTexture *texture);
    WebGPUTexture *ComposeAlpha(WebGPUTexture *base, WebGPUTexture *alpha, unsigned int color, bool colorOnly);
    void RenderFontAtlas(WebGPUTexture *target, WebGPUTexture *source, std::span<const GpuFontGlyph> glyphs,
                         unsigned int samplingFlags, bool forceWhite);
    void DestroyTexture(WebGPUTexture *texture);
    WebGPUTexture *const &whiteTexture;
    static unsigned int SlotOf(const WebGPUTexture *texture);
    WebGPUTexture *CreateCausticsTexture(int width, int height, WebGPUTexture *source);
    void UseCausticsTexture(WebGPUTexture *output, WebGPUTexture *source);
    void SetCausticsTimeNs(unsigned long long timeNs);
    WebGPUTexture *const &causticsTexture;
    bool HasCausticsCompute() const;
    bool BeginScene();
    const bool &sceneOpen;
    void SubmitTriangles(std::span<const GpuVertex> vertices, WebGPUTexture *texture, bool linear, bool wrap,
                         GpuBlend blend, const GpuClip *clip = nullptr);
    void SubmitQuads(std::span<const GpuVertex> vertices, WebGPUTexture *texture, bool linear, bool wrap,
                     GpuBlend blend, const GpuClip *clip = nullptr);
    void SubmitPool(WebGPUTexture *texture, const GpuPoolParams &params);
    void SubmitCoverageTriangles(std::span<const GpuVertex> vertices, WebGPUTexture *texture, GpuBlend blend,
                                 const GpuClip *clip = nullptr);
    void EndSceneAndPresent();
    bool PresentRetainedScene();
    bool ResizePresent();
    void ResizeScene(int width, int height);
    void SetHdrParams(const VulkanHdrParams &params);
    VulkanHdrParams HdrParams() const;
    void PrepareScale(bool fullscreen, std::span<const WebGPUScaleStep> steps, unsigned int width, unsigned int height);
    void WarmScale(bool fullscreen);
    void SetScaleMode(bool enabled, bool fullscreen);
    WebGPUScaleImage ScaleOutput(bool fullscreen) const;
    struct ScaleCounts
    {
        size_t programs, lookups, writableImages;
    };
    ScaleCounts GetScaleCounts() const;
    GpuClip PresentRect(int width, int height) const;
    std::vector<unsigned int> ReadScenePixels();
    ImagePixels ReadTexturePixels(WebGPUTexture *texture);
    std::vector<float> ReadTextureFloats(WebGPUTexture *texture);
    std::vector<unsigned int> ReadScalePixels(bool fullscreen);
    const int &sceneWidth;
    const int &sceneHeight;
    unsigned int MaxImageDimension2D() const;
    struct ResourceCounts
    {
        size_t textures, retiredTextures, descriptorPools, vertexBuffers;
        size_t queuedBatches{};
    };
    ResourceCounts GetResourceCounts() const;
};
} // namespace Sexy
