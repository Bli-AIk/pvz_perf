#pragma once
#include "GLContext.h"
#include "graphics/GpuTypes.h"
#include "graphics/GpuPool.h"
#include "graphics/ImageReadback.h"
#include "graphics/vulkan/VulkanHdr.h"
#include "GLScaleTypes.h"
#include <cstddef>
#include <filesystem>
#include <memory>
#include <span>
#include <vector>

namespace Sexy {
struct GLTexture;
class GLRenderer final {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
    GLRenderer(GLContext& context, int width, int height,
        const std::filesystem::path& shaderDirectory, const VulkanHdrParams* hdr = nullptr);
    ~GLRenderer();
    GLRenderer(const GLRenderer&) = delete;
    GLRenderer& operator=(const GLRenderer&) = delete;
    void PrepareLoadingPipelines();
    GLTexture* CreateTexture(int width, int height, const void* bgra, int pitch, bool opaque = false);
    GLTexture* CreateTextureStatic(int width, int height, const void* bgra,
        int pitch, bool opaque, unsigned int slot);
    GLTexture* CreateRawTexture(int width, int height, const void* bgra, int pitch);
    GLTexture* ReplaceTexture(GLTexture* previous, int width, int height,
        const void* bgra, int pitch, bool opaque = false);
    void ReserveStaticTextureSlots(unsigned int count);
    void BindStaticTexture(GLTexture* texture, unsigned int slot);
    GLTexture* CreateCoverageMask(int width, int height,
        std::span<const unsigned char> coverage, int pitch);
    GLTexture* CreateRenderImage(int width, int height);
    void SetRenderImage(GLTexture* texture);
    GLTexture* CopyTextureRegion(GLTexture* source, int x, int y, int width, int height);
    GLTexture* ProcessImage(GLTexture* source, const GpuImageProcessing& processing);
    GLTexture* ComposeAlphaRaw(GLTexture* base, GLTexture* alpha,
        unsigned int color, bool colorOnly);
    GLTexture* AdoptComposedTexture(GLTexture* texture);
    GLTexture* ComposeAlpha(GLTexture* base, GLTexture* alpha,
        unsigned int color, bool colorOnly);
    void RenderFontAtlas(GLTexture* target, GLTexture* source,
        std::span<const GpuFontGlyph> glyphs, unsigned int samplingFlags, bool forceWhite);
    void DestroyTexture(GLTexture* texture);
    GLTexture* const& whiteTexture;
    static unsigned int SlotOf(const GLTexture* texture);
    GLTexture* CreateCausticsTexture(int width, int height, GLTexture* source);
    void UseCausticsTexture(GLTexture* output, GLTexture* source);
    void SetCausticsTimeNs(unsigned long long timeNs);
    GLTexture* const& causticsTexture;
    bool HasCausticsCompute() const;
    bool BeginScene();
    const bool& sceneOpen;
    void SubmitTriangles(std::span<const GpuVertex> vertices, GLTexture* texture,
        bool linear, bool wrap, GpuBlend blend, const GpuClip* clip = nullptr);
    void SubmitQuads(std::span<const GpuVertex> vertices, GLTexture* texture,
        bool linear, bool wrap, GpuBlend blend, const GpuClip* clip = nullptr);
    void SubmitPool(GLTexture* texture, const GpuPoolParams& params);
    void SubmitCoverageTriangles(std::span<const GpuVertex> vertices, GLTexture* texture,
        GpuBlend blend, const GpuClip* clip = nullptr);
    void EndSceneAndPresent();
    bool PresentRetainedScene();
    bool ResizePresent();
    void ResizeScene(int width, int height);
    void SetHdrParams(const VulkanHdrParams& params);
    VulkanHdrParams HdrParams() const;
    void PrepareScale(bool fullscreen, std::span<const GLScaleStep> steps,
        unsigned int width, unsigned int height);
    void WarmScale(bool fullscreen);
    void SetScaleMode(bool enabled, bool fullscreen);
    GLScaleImage ScaleOutput(bool fullscreen) const;
    struct ScaleCounts { size_t programs, lookups, writableImages; };
    ScaleCounts GetScaleCounts() const;
    GpuClip PresentRect(int width, int height) const;
    std::vector<unsigned int> ReadScenePixels();
    ImagePixels ReadTexturePixels(GLTexture* texture);
    std::vector<float> ReadTextureFloats(GLTexture* texture);
    std::vector<unsigned int> ReadScalePixels(bool fullscreen);
    const int& sceneWidth;
    const int& sceneHeight;
    unsigned int MaxImageDimension2D() const;
    struct ResourceCounts {
        size_t textures,retiredTextures,descriptorPools,vertexBuffers;
        size_t queuedBatches{};
    };
    ResourceCounts GetResourceCounts() const;
};
}
