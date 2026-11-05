#pragma once

#include "VulkanContext.h"
#include "VulkanHdr.h"
#include "VulkanViewport.h"
#include "VulkanScalePipeline.h"
#include "SexyAppFramework/graphics/ImageReadback.h"
#include "SexyAppFramework/graphics/GpuTypes.h"
#include "SexyAppFramework/graphics/GpuPool.h"
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace Sexy {

// Framework pixel coordinates, straight alpha, normalized image coordinates.
// No DirectX types or emulated device/state interface cross this boundary.
using VulkanVertex = GpuVertex;
using VulkanBlend = GpuBlend;
struct VulkanTexture;
using VulkanFontGlyph = GpuFontGlyph;
using VulkanImageOperation = GpuImageOperation;
using VulkanImageProcessing = GpuImageProcessing;

#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
// Uniform per-quad metadata; no runtime animation data or color quantization.
struct VulkanSpriteInstance {
    float positions[8],uvs[8],color[4];
    unsigned int slot,sampler;
};
static_assert(sizeof(VulkanSpriteInstance)==88);
#endif

// Lives inside SexyFramework. VulkanContext owns GPU/WSI resources; this owns the
// logical game render target, textures, samplers, pipelines and vertex streams.
// ResizePresent changes WSI only; ResizeScene explicitly replaces the canvas.
// Neither operation replaces resident game images.
// Read-only metadata references borrow stable storage from this renderer's Impl.
// Main WSI alternates two command/fence sets and their dynamic draw streams.
// Texture descriptors remain immutable while referenced by recorded GPU work.
class VulkanRenderer final {
    struct Impl;
    std::unique_ptr<Impl> impl_;
public:
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    // Isolated desktop experiment only; absent from the production library.
    static VkQueryPool benchmarkQueries;
    static bool benchmarkSimpleGenerated;
    static bool benchmarkSeparateBlend;
    static bool benchmarkManualBakedFilters;
    static bool benchmarkNonIndexedTriangles;
    static bool benchmark32BitQuads;
    static bool benchmarkPackedColor;
    static bool benchmarkPackedColor16;
    static bool benchmarkDiscardScene;
    static bool benchmarkInstancedQuads;
    static bool benchmarkPresentCopy;
    static bool benchmarkSolidFill;
    static bool benchmarkGenerateIndexPrefix;
    static unsigned int benchmarkDrawCalls, benchmarkSceneDraws;
    static unsigned int benchmarkTriangles, benchmarkSceneTriangles;
    void SubmitBenchmarkSprite(const VulkanSpriteInstance&,VulkanTexture*,VulkanBlend,const VulkanClip*);
    VulkanTexture* ConvertBenchmarkTexture(VulkanTexture* source,VkFormat format);
#endif
    VulkanRenderer(VulkanContext& window, int sceneWidth, int sceneHeight,
                   const std::string& vertexShader, const std::string& fragmentShader,
                   const VulkanHdrParams* hdr = nullptr);
    ~VulkanRenderer();
    VulkanRenderer(const VulkanRenderer&) = delete;
    VulkanRenderer& operator=(const VulkanRenderer&) = delete;

    // Render-thread loading action, before image composition or font rendering.
    // Prepares fixed compute pipelines without images or command submission.
    // Idempotent after success; business calls require the prepared pipelines.
    void PrepareLoadingPipelines();

    // Input matches MemoryImage: native little-endian 0xAARRGGBB (BGRA bytes).
    // Source row pitch may exceed width*4. Opaque ignores alpha
    // through draw metadata, without rewriting the raw source image.
    // Copies source pixels before returning; callers may immediately free them.
    // GPU copies are batched and completed before game submission. A destroyed
    // handle's recorded drawing/upload remains alive through the frame fence.
    VulkanTexture* CreateTexture(int width, int height, const void* bgra,
                                 int rowPitch, bool opaque = false);
    // Device-local premultiplied RGBA16F image, initialized to transparent black
    // on the GPU; attachment, blend, filtering, storage and transfer usages checked.
    // Its pointer stays stable when writes snapshot a previously sampled version.
    // Premultiplied storage does not retain hidden RGB at exactly zero alpha.
    VulkanTexture* CreateRenderImage(int width, int height);
    void RenderFontAtlas(VulkanTexture* target,VulkanTexture* source,
        std::span<const VulkanFontGlyph> glyphs,uint32_t samplingFlags,bool forceWhite);
    VulkanTexture* CreateRawTexture(int width,int height,const void* bgra,int pitch);
    VulkanTexture* ComposeAlphaRaw(VulkanTexture* base,VulkanTexture* alpha,
        uint32_t color,bool colorOnly);
    VulkanTexture* AdoptComposedTexture(VulkanTexture* composed);
    VulkanTexture* ComposeAlpha(VulkanTexture* base,VulkanTexture* alpha,
        uint32_t color,bool colorOnly);
    // Device-local immutable-pixel copy; source image edges are zero padded.
    VulkanTexture* CopyTextureRegion(VulkanTexture* source,int x,int y,int width,int height);
    VulkanTexture* ProcessImage(VulkanTexture* source,
        const VulkanImageProcessing& processing);
    void BindStaticTexture(VulkanTexture* texture,uint32_t staticSlot);
    // nullptr selects the retained main scene. Caller owns any nesting stack.
    // Inside a main frame this switches LOAD passes without submitting/presenting.
    // From an image-only batch to nullptr, submits/waits only that batch and
    // closes the scene; the facade must call BeginScene again before scene draws.
    // All renderer calls are serialized on the render thread, not worker jobs.
    void SetRenderImage(VulkanTexture* image);
    const bool& sceneOpen;
    // Common drawing counts eligible picture candidates even on CPU fallback.
    // The next completed-batch decision uses their aggregate frame density.
    // reanim-v4 静态槽位：纹理落在槽位区恒等位 staticBase+staticSlot——
    // 编译记录烘焙的最终下标，运行时零翻译。静态槽永不回收（启动语料
    // 终身常驻）；重复放置同槽（变体重建）原地覆盖描述符。
    VulkanTexture* CreateTextureStatic(int width, int height, const void* bgra,
                                       int rowPitch, bool opaque, uint32_t staticSlot);
    // 预约槽位区顶部 count 个为静态区 [staticBase, kTextureSlots)。
    // 顶部设计免除初始化时序耦合：动态区 [0,staticBase) 行为不变，预约
    // 可晚于任意动态创建（须在首次静态放置前）。
    void ReserveStaticTextureSlots(uint32_t count);
    // Consumes the previous handle and returns its current version. Reuses the
    // image/view/descriptor when compatible and not yet sampled in this frame;
    // otherwise retains the recorded old version until its frame completes.
    // GPU render images cannot be replaced by this host-upload API.
    VulkanTexture* ReplaceTexture(VulkanTexture* previous, int width, int height,
        const void* bgra, int rowPitch, bool opaque = false);
    // Lossless byte coverage, not a recompressed game image. Source is copied
    // before return; the same frame-fence retirement applies as for BGRA images.
    VulkanTexture* CreateCoverageMask(int width, int height,
        std::span<const uint8_t> coverage, int rowPitch);
    // The selected render image must be deselected before destruction.
    void DestroyTexture(VulkanTexture* texture);
    VulkanTexture* const& whiteTexture;
    // Encoded vertex slot, not a raw descriptor-array index (see VulkanVertex).
    static uint32_t SlotOf(const VulkanTexture* texture);

    // GPU-owned animated texture (pool caustics). The image is created from
    // the given BGRA snapshot once, then rewritten every frame by an internal
    // compute pass recorded before the render pass; it is never re-uploaded
    // from host memory. lut is the 65536-byte grayscale table (copied).
    VulkanTexture* CreateCausticsTexture(int width, int height, VulkanTexture* source);
    void UseCausticsTexture(VulkanTexture* output, VulkanTexture* source);
    // Latches integer nanosecond age for the next compute pass. Call between
    // frames (update phase); the dispatch happens inside BeginScene.
    void SetCausticsTimeNs(unsigned long long timeNs);
    VulkanTexture* const& causticsTexture;
    bool HasCausticsCompute() const;

    // A selected render image begins a WSI-independent batch, even minimized.
    // Only main-scene batches return false for minimized/out-of-date WSI.
    bool BeginScene();
    void SubmitTriangles(std::span<const VulkanVertex> vertices, VulkanTexture* texture,
                         bool linear, bool wrap, VulkanBlend blend,
                         const VulkanClip* clip = nullptr);
    // Four vertices per rectangle, with triangles (0,1,2) and (1,2,3).
    // Native uint16 indexed runs also accept ordinary triangles without reordering.
    void SubmitQuads(std::span<const VulkanVertex> vertices,VulkanTexture* texture,
        bool linear,bool wrap,VulkanBlend blend,const VulkanClip* clip = nullptr);
    void SubmitPool(VulkanTexture* texture, const GpuPoolParams& params);
    void SavePipelineCache();
    // Nearest/clamp mask sampling, integer byte coverage -> alpha in the GPU,
    // with the same native blending and clipping as ordinary primitives.
    void SubmitCoverageTriangles(std::span<const VulkanVertex> vertices, VulkanTexture* mask,
        VulkanBlend blend, const VulkanClip* clip = nullptr);
    // Image-only batches submit/wait GPU work without presenting or readback.
    // Main frames present once, even if their last selected target is an image.
    void EndSceneAndPresent();
    // false: no scene yet or WSI temporarily unavailable. Errors throw.
    bool PresentRetainedScene();
    bool ResizePresent(); // false: WSI deferred; canvas/textures remain intact.
    void ResizeScene(int width, int height);
    // HDR monitor metadata only; updates next frame's push constants. Does not
    // rebuild pipelines, textures, swapchains or re-read effect source files.
    void SetHdrParams(const VulkanHdrParams& params);
    VulkanHdrParams HdrParams() const;

    // Both modes remain resident; preparation/warm-up is separate from activation.
    // Effect descriptors must outlive this renderer. Empty steps mean no chain.
    void PrepareScale(bool fullscreen, std::span<const VulkanScaleStep> steps,
                      uint32_t targetWidth, uint32_t targetHeight);
    void WarmScale(bool fullscreen);
    void SetScaleMode(bool enabled, bool fullscreen);
    VulkanScaleImage ScaleOutput(bool fullscreen) const;
    VulkanClip PresentRect(int framebufferWidth, int framebufferHeight) const;
    struct ScaleCounts { size_t programs, lookups, writableImages; };
    ScaleCounts GetScaleCounts() const;

    // Explicit diagnostic/export operation only. Normal rendering NEVER calls
    // this or copies the game scene through host memory on the way to display.
    std::vector<std::uint32_t> ReadScenePixels();
    ImagePixels ReadTexturePixels(VulkanTexture* texture);
    std::vector<std::uint32_t> ReadScalePixels(bool fullscreen); // diagnostic, BGRA8 output only
    const int& sceneWidth;
    const int& sceneHeight;
    uint32_t MaxImageDimension2D() const;
    struct ResourceCounts { size_t textures, retiredTextures, descriptorPools, vertexBuffers; };
    ResourceCounts GetResourceCounts() const;
    struct TextureUploadState {
        size_t pending;
        uint64_t stagingCapacity;
    };
    TextureUploadState GetTextureUploadState() const;
};
}
