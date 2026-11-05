#pragma once
#include "GLRenderer.h"
#include "GLContext.h"
#include "GLPlatform.h"
#include "GLApi.h"
#include <array>
#include <memory>
#include <string_view>
#include <string>
#include <unordered_set>
#include <vector>

namespace Sexy {
struct GLTexture {
    unsigned int image = 0, framebuffer = 0;
    int width = 0, height = 0;
    unsigned int format = 0;
    unsigned int slot = ~0U, staticSlot = ~0U, flags = 0;
    unsigned int sampler = 0; // Actual GL texture filter/wrap state; GL 3.0 has no core sampler objects.
    bool render = false, caustic = false;
    GLTexture() = default;
    ~GLTexture();
    GLTexture(const GLTexture&) = delete;
    GLTexture& operator=(const GLTexture&) = delete;
};
struct GLRenderer::Impl {
    enum Uniform : unsigned int {
        uTargetExtent, uImage, uPremultipliedOutput, uCoverageInput,
        uSourceImage, uAlphaImage, uSourceRect, uAlphaRect, uSourceSlot,
        uOperation, uRectangle, uTint, uPackedColor, uCausticU0, uCausticU1, uCausticV1, uColorOnly,
        uSamplingFlags, uForceWhite, uXRanges, uYRanges, uXTaps, uYTaps,
        uXTapExtent, uYTapExtent, uGlyphSrc, uGlyphDst, uUniformCount
    };
    using UniformLocations=std::array<int,uUniformCount>;
    using Vertex = GpuVertex;
    struct Batch {
        GLTexture* texture;
        // Contiguous VBO vertex range; quad batches use the shared static index pattern.
        size_t first, count;
        GpuBlend blend;
        GpuClip clip;
        bool coverage;
        bool sprite;
        bool mixed;
        bool quads;
        unsigned int sampler;
    };
    GLContext& context;
    int width, height, maximum = 0;
    unsigned int staticBase = 4096, vao = 0, stream = 0, quadIndices = 0;
    struct FrameResources {
        unsigned int vao = 0, stream = 0;
        GLsync fence = nullptr;
    };
    static constexpr unsigned int FrameCount = 2;
    std::array<FrameResources, FrameCount> frames{};
    unsigned int frameIndex = 0;
    const bool frameSync;
    unsigned int sceneProgram = 0, spriteProgram = 0, mixedSpriteProgram = 0, imageProgram = 0, fontProgram = 0, presentProgram = 0;
    unsigned int poolProgram = 0, poolVao = 0, poolVertices = 0, poolIndices = 0;
    std::array<int, 12> poolUniforms{};
    UniformLocations sceneUniforms{}, spriteUniforms{}, mixedSpriteUniforms{}, imageUniforms{}, fontUniforms{}, presentUniforms{};
    bool open = false, mainFrame = false, initialized = false, sceneInitialized = false;
    GLTexture* white = nullptr;
    GLTexture* scene = nullptr;
    GLTexture* selected = nullptr;
    GLTexture* causticSource = nullptr;
    unsigned long long causticTimeNs = 0;
    bool causticsQueued = false;
    GLTexture* causticTarget = nullptr;
    VulkanHdrParams hdr{80, 1000, 150, 1};
    std::array<GLTexture*, 4096> slots{};
    std::unordered_set<GLTexture*> textures;
    // Native names in queued batches remain valid through Flush, even if their
    // public texture was replaced/destroyed. No per-triangle reference counting.
    std::vector<GLTexture*> retired;
    std::vector<Vertex> vertices;
    std::vector<Batch> batches;
    Impl(GLContext& context, int width, int height);
    ~Impl();
    void Dimensions(int width, int height) const;
    void AllocateStorage(GLTexture& texture, int width, int height, unsigned int format);
    GLTexture* Allocate(int width, int height, unsigned int format, bool registerSlot);
    void Register(GLTexture* texture);
    void Upload(GLTexture* texture, const void* data, int pitch, unsigned int format);
    void BindTarget();
    void Flush();
    void Submit(std::span<const GpuVertex> input, GLTexture* source,
        GpuBlend blend, const GpuClip* clip, bool coverage, bool quads = false);
    unsigned int Program(std::string_view vertex, std::string_view fragment, bool mixed = false);
    void VertexBase(size_t first);
    void ImagePass(GLTexture* target, GLTexture* source, GLTexture* alpha,
        int operation, const GpuImageProcessing* processing = nullptr,
        unsigned int color = 0, bool colorOnly = false);
    void Quad();
    bool Present();
    void Snapshot(const GLTexture& source, GLTexture& copy);
    void Check([[maybe_unused]] const char* operation) const
    {
#if !defined(NDEBUG)
        GL3::CheckError(operation);
#endif
    }
};
}
