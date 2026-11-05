#pragma once
#include "D3d9Renderer.h"
#include "D3d9Api.h"
#include "D3d9Shaders.h"
#include <array>
#include <unordered_set>

namespace Sexy {
struct D3d9Texture {
    DX9::Com<IDirect3DTexture9> image;
    DX9::Com<IDirect3DSurface9> surface;
    int width = 0, height = 0;
    D3DFORMAT format = D3DFMT_UNKNOWN;
    unsigned int slot = ~0U, staticSlot = ~0U, flags = 0;
    bool render = false, caustic = false;
};
struct D3d9Renderer::Impl {
    using Vertex = D3d9Shaders::Vertex;
    static_assert(sizeof(Vertex) == 52 && offsetof(Vertex, rect) == 32 && offsetof(Vertex, flags) == 48);
    using Constants = std::array<D3d9Shaders::Float4, D3d9Shaders::PixelConstantCount>;
    struct Batch {
        IDirect3DTexture9* image = nullptr;
        int width = 0, height = 0;
        size_t first = 0, count = 0;
        GpuBlend blend{};
        GpuClip clip{};
        bool coverage = false;
    };
    D3d9Context& context;
    IDirect3DDevice9Ex* device;
    int width, height, maximum = 0;
    unsigned int staticBase = 4096;
    size_t streamCapacity = 0;
    unsigned long long fontInstructionsPerTap = 0;
    std::filesystem::path shaderDirectory;
    DX9::Com<IDirect3DVertexDeclaration9> declaration;
    DX9::Com<IDirect3DVertexBuffer9> stream;
    struct FrameResources {
        DX9::Com<IDirect3DVertexBuffer9> stream;
        DX9::Com<IDirect3DQuery9> fence;
        size_t streamCapacity = 0;
        bool pending = false;
    };
    std::array<FrameResources, 2> frames{};
    unsigned int activeFrame = 0, nextFrame = 0;
    DX9::Com<IDirect3DVertexShader9> sceneVS, imageVS;
    DX9::Com<IDirect3DVertexShader9> poolVS;
    DX9::Com<IDirect3DVertexDeclaration9> poolDeclaration;
    DX9::Com<IDirect3DVertexBuffer9> poolVertices;
    DX9::Com<IDirect3DIndexBuffer9> poolIndices;
    DX9::Com<IDirect3DPixelShader9> scenePS, copyPS, colorizePS, composePS, causticsPS, fontPS, presentPS;
    bool open = false, mainFrame = false, initialized = false, sceneInitialized = false;
    D3d9Texture* white = nullptr;
    D3d9Texture* scene = nullptr;
    D3d9Texture* selected = nullptr;
    D3d9Texture* causticSource = nullptr;
    unsigned long long causticTimeNs = 0;
    bool causticsQueued = false;
    D3d9Texture* causticTarget = nullptr;
    VulkanHdrParams hdr{80, 1000, 150, 1};
    std::array<D3d9Texture*, 4096> slots{};
    std::unordered_set<D3d9Texture*> textures;
    // Batches borrow native textures. Destroyed versions and feedback copies
    // remain owned here until every queued draw has been issued by Flush.
    std::vector<D3d9Texture*> retiredTextures;
    std::vector<Vertex> vertices;
    std::vector<Batch> batches;
    Impl(D3d9Context&, int, int, const std::filesystem::path&);
    ~Impl();
    void Dimensions(int, int) const;
    void InitializeTexture(D3d9Texture&, int, int, D3DFORMAT);
    D3d9Texture* Allocate(int, int, D3DFORMAT, bool registerSlot);
    void Retire(D3d9Texture*);
    void Register(D3d9Texture*);
    void Upload(D3d9Texture*, const void*, int);
    void BindTarget();
    void Target(IDirect3DSurface9*, int, int);
    void InitializeStates();
    void Flush();
    void Submit(std::span<const GpuVertex>, D3d9Texture*, GpuBlend, const GpuClip*, bool);
    void Source(Constants&, const D3d9Texture*, unsigned int sampler = 0) const;
    void SetConstants(const Constants&);
    void Projection(int, int);
    void Quad(int, int);
    void ImagePass(D3d9Texture*, D3d9Texture*, D3d9Texture*, int,
        const GpuImageProcessing* = nullptr, unsigned int = 0, bool = false);
    void Snapshot(D3d9Texture&, D3d9Texture*);
    bool Present();
};
}
