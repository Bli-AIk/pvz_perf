#include "SexyAppFramework/graphics/PoolMesh.h"
#include "D3d12Renderer.h"
#include "D3d12Context.h"
#include "D3d12Check.h"
#include "D3d12ScalePipeline.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace Sexy {
namespace {
using Microsoft::WRL::ComPtr;
constexpr UINT TextureSlots = 4096;
constexpr UINT HeapSlots = 16384;
constexpr UINT InvalidSlot = UINT_MAX;
constexpr auto SampleState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

void Check(HRESULT hr, const char* operation, ID3D12Device* device) {
    D3d12Check(hr, operation, FAILED(hr) ? device->GetDeviceRemovedReason() : S_OK);
}

std::vector<char> Shader(const std::filesystem::path& directory, const char* name) {
    const auto path = directory / (std::string(name) + ".dxil");
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file || file.tellg() <= 0)
        throw std::runtime_error("Cannot read D3D12 shader: " + path.string());
    std::vector<char> bytes(static_cast<size_t>(file.tellg()));
    file.seekg(0);
    if (!file.read(bytes.data(), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Incomplete D3D12 shader: " + path.string());
    return bytes;
}

UINT64 Align(UINT64 size, UINT64 alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

constexpr std::array<D3D12_INPUT_ELEMENT_DESC, 5> VertexLayout{{
    {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 8, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 1, DXGI_FORMAT_R32_UINT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    {"TEXCOORD", 2, DXGI_FORMAT_R32_UINT, 0, 36, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
}};
}

struct D3d12Texture {
    ComPtr<ID3D12Resource> resource;
    D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    int width = 0, height = 0;
    UINT slot = InvalidSlot;
    UINT staticSlot = InvalidSlot;
    UINT rtv = InvalidSlot;
    std::array<float, 4> clearColor{};
    bool opaque = false, generated = false, sampled = false;
};

struct D3d12Renderer::Impl {
    struct Page {
        ComPtr<ID3D12Resource> resource;
        std::byte* mapped = nullptr;
        UINT64 size = 0, used = 0;
    };
    struct Run {
        Page* page = nullptr;
        UINT first = 0, count = 0;
        GpuBlend blend = GpuBlend::Normal;
        D3D12_RECT scissor{};
        bool coverage = false;
        bool simple = false;
        enum Geometry { Triangles, Quads, Mixed } geometry = Triangles;
        Page* indices = nullptr;
        UINT firstIndex = 0, indexCount = 0;
    };
    struct Kernel {
        ComPtr<ID3D12RootSignature> root;
        ComPtr<ID3D12PipelineState> pipeline;
        UINT constants = 0, inputs = 0;
    };
    struct LoadingPipelines {};
    struct ScaleMode {
        std::unique_ptr<D3d12ScalePipeline> pipeline;
        std::unique_ptr<D3d12Texture> sampledOutput;
        bool warm = false;
    };

    D3d12Context& context;
    ID3D12Device* device;
    std::filesystem::path directory;
    int width;
    int height;
    ComPtr<ID3D12DescriptorHeap> heap, samplerHeap, rtvHeap;
    UINT descriptorStride = 0, rtvStride = 0;
    UINT temporary = TextureSlots, staticBase = TextureSlots;
    std::array<D3d12Texture*, TextureSlots> owners{};
    std::vector<UINT> freeRtvs;
    std::array<bool, TextureSlots> allocated{};
    std::unordered_map<D3d12Texture*, std::unique_ptr<D3d12Texture>> textures, raw;
    std::vector<std::unique_ptr<D3d12Texture>> retired;
    std::unique_ptr<D3d12Texture> scene;
    D3d12Texture* white = nullptr;
    D3d12Texture* renderImage = nullptr;
    D3d12Texture* boundTarget = nullptr;
    ID3D12GraphicsCommandList* commands = nullptr;
    bool sceneOpen = false;
    bool imageOnly = false, sceneInitialized = false, inFlight = false;
    std::vector<std::unique_ptr<Page>> staging, vertices, indices;
    ComPtr<ID3D12Resource> staticIndices;
    // Reused by Draw; avoids a heap allocation for every submitted primitive.
    std::vector<D3d12Texture*> drawSources;
    Run run;
    ComPtr<ID3D12RootSignature> gameRoot, presentRoot;
    std::array<ComPtr<ID3D12PipelineState>, 3> scenePipelines, imagePipelines;
    ComPtr<ID3D12PipelineState> spriteOpaquePipeline, mixedSpritePipeline;
    ComPtr<ID3D12PipelineState> presentPipeline;
    ComPtr<ID3D12RootSignature> poolRoot;
    std::array<ComPtr<ID3D12PipelineState>, 3> poolScenePipelines, poolImagePipelines;
    ComPtr<ID3D12Resource> poolVertices, poolIndices;
    Kernel colorKernel, alphaKernel, fontKernel, causticKernel;
    GpuHdrParams hdr{200, 1000, 200, 1};
    std::unique_ptr<D3d12ScaleLibrary> scaleLibrary;
    std::array<std::unique_ptr<ScaleMode>, 2> scaleModes;
    bool scaleEnabled = false;
    unsigned scaleMode = 0;
    D3d12Texture* causticTarget = nullptr;
    D3d12Texture* causticSource = nullptr;
    unsigned long long causticTimeNs = 0;
    bool causticQueued = false;
    ID3D12PipelineState* boundPipeline = nullptr;
    Page* boundVertex = nullptr;
    ID3D12Resource* boundIndex = nullptr;
    bool gameBound = false;
    std::array<UINT, 4> boundConstants{};
    bool hasConstants = false;
    D3D12_RECT boundScissor{};
    bool hasScissor = false;

    Impl(D3d12Context& ctx, int w, int h,
        const std::filesystem::path& shaders)
        : context(ctx), device(ctx.device), directory(shaders),
          width(w), height(h) {
        Dimensions(w, h);
        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        desc.NumDescriptors = HeapSlots;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        Check(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap)),
            "CreateDescriptorHeap(renderer SRV/UAV)", device);
        descriptorStride = device->GetDescriptorHandleIncrementSize(desc.Type);
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        desc.NumDescriptors = 4;
        Check(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&samplerHeap)),
            "CreateDescriptorHeap(renderer samplers)", device);
        auto samplerHandle = samplerHeap->GetCPUDescriptorHandleForHeapStart();
        const UINT samplerStride = device->GetDescriptorHandleIncrementSize(desc.Type);
        for (UINT i = 0; i < 4; ++i) {
            D3D12_SAMPLER_DESC sampler{};
            sampler.Filter = (i & 1) ? D3D12_FILTER_MIN_MAG_MIP_LINEAR :
                D3D12_FILTER_MIN_MAG_MIP_POINT;
            sampler.AddressU = sampler.AddressV = sampler.AddressW = (i & 2) ?
                D3D12_TEXTURE_ADDRESS_MODE_WRAP : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            sampler.MaxAnisotropy = 1;
            sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            sampler.MaxLOD = D3D12_FLOAT32_MAX;
            device->CreateSampler(&sampler, samplerHandle);
            samplerHandle.ptr += samplerStride;
        }
        desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        desc.NumDescriptors = TextureSlots;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        Check(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&rtvHeap)),
            "CreateDescriptorHeap(renderer RTV)", device);
        rtvStride = device->GetDescriptorHandleIncrementSize(desc.Type);
        for (UINT i = TextureSlots; i > 0; --i)
            freeRtvs.push_back(i - 1);
        // Initialize every array element: tier-2 hardware may validate unused SRVs.
        D3D12_SHADER_RESOURCE_VIEW_DESC nullView{};
        nullView.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        nullView.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        nullView.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        nullView.Texture2D.MipLevels = 1;
        for (UINT i = 0; i < TextureSlots; ++i)
            device->CreateShaderResourceView(nullptr, &nullView, Cpu(i));
        InitGraphics();
        InitKernel(causticKernel, "game_caustic_cs", 3, 1, LoadingPipelines{});
        scene = Image(w, h, DXGI_FORMAT_B8G8R8A8_UNORM, true, false, false,
            {0, 0, 0, 1});
    }

    ~Impl() {
        try {
            Flush();
            if (commands) Finish(false);
        } catch (...) {
            // COM ownership still releases correctly after device removal.
        }
        try {
            Drain();
        } catch (...) {
            // Preserve the original failure while attempting the final queue wait.
        }
    }

    void Dimensions(int w, int h) const {
        if (w <= 0 || h <= 0 || w > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
            h > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
            throw std::invalid_argument("Invalid D3D12 image dimensions");
    }

    D3D12_CPU_DESCRIPTOR_HANDLE Cpu(UINT index) const {
        auto result = heap->GetCPUDescriptorHandleForHeapStart();
        result.ptr += SIZE_T(index) * descriptorStride;
        return result;
    }
    D3D12_GPU_DESCRIPTOR_HANDLE Gpu(UINT index) const {
        auto result = heap->GetGPUDescriptorHandleForHeapStart();
        result.ptr += UINT64(index) * descriptorStride;
        return result;
    }
    D3D12_CPU_DESCRIPTOR_HANDLE Rtv(UINT index) const {
        auto result = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        result.ptr += SIZE_T(index) * rtvStride;
        return result;
    }

    ComPtr<ID3D12RootSignature> Root(UINT constants, UINT inputs, bool game,
        bool sampler, bool compute, bool pool = false) {
        std::array<D3D12_DESCRIPTOR_RANGE, 2> ranges{};
        ranges[0] = {D3D12_DESCRIPTOR_RANGE_TYPE_SRV, inputs, 0, 0, 0};
        ranges[1] = {game ? D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER :
            D3D12_DESCRIPTOR_RANGE_TYPE_UAV, game ? 4u : 1u, 0, 0, 0};
        std::array<D3D12_ROOT_PARAMETER, 4> params{};
        params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[0].Constants = {0, 0, constants};
        params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
        params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[1].DescriptorTable = {1, &ranges[0]};
        params[1].ShaderVisibility = compute ? D3D12_SHADER_VISIBILITY_ALL :
            D3D12_SHADER_VISIBILITY_PIXEL;
        params[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        params[2].DescriptorTable = {1, &ranges[1]};
        params[2].ShaderVisibility = game ? D3D12_SHADER_VISIBILITY_PIXEL :
            D3D12_SHADER_VISIBILITY_ALL;
        params[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        params[3].Constants = {1, 0, 16};
        params[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
        std::array<D3D12_STATIC_SAMPLER_DESC, 4> samplers{};
        for (UINT i = 0; i < samplers.size(); ++i) {
            auto& s = samplers[i];
            s.Filter = (i & 1) || !game ? D3D12_FILTER_MIN_MAG_MIP_LINEAR :
                D3D12_FILTER_MIN_MAG_MIP_POINT;
            s.AddressU = s.AddressV = s.AddressW = (i & 2) ?
                D3D12_TEXTURE_ADDRESS_MODE_WRAP : D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
            s.MaxAnisotropy = 1;
            s.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            s.MaxLOD = D3D12_FLOAT32_MAX;
            s.ShaderRegister = i;
            s.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
        }
        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.NumParameters = pool ? 4 : (compute || game ? 3 : 2);
        desc.pParameters = params.data();
        desc.NumStaticSamplers = sampler && !game ? 1 : 0;
        desc.pStaticSamplers = samplers.data();
        desc.Flags = compute ? D3D12_ROOT_SIGNATURE_FLAG_NONE :
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ComPtr<ID3DBlob> blob, errors;
        const auto hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1,
            &blob, &errors);
        if (FAILED(hr) && errors)
            throw std::runtime_error(std::string(static_cast<const char*>(errors->GetBufferPointer()),
                errors->GetBufferSize()));
        Check(hr, "D3D12SerializeRootSignature", device);
        ComPtr<ID3D12RootSignature> root;
        Check(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(),
            IID_PPV_ARGS(&root)), "CreateRootSignature", device);
        return root;
    }

    ComPtr<ID3D12PipelineState> Graphics(ID3D12RootSignature* root,
        const std::vector<char>& vs, const std::vector<char>& ps,
        DXGI_FORMAT format, GpuBlend blend, bool premultiplied, bool pool = false,
        const std::vector<char>* gs = nullptr, bool dualSource = false) {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = root;
        desc.VS = {vs.data(), vs.size()};
        desc.PS = {ps.data(), ps.size()};
        if (gs) desc.GS = {gs->data(), gs->size()};
        desc.InputLayout = {VertexLayout.data(), UINT(VertexLayout.size())};
        const D3D12_INPUT_ELEMENT_DESC poolLayout{
            "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,
            D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0};
        if (pool) desc.InputLayout = {&poolLayout, 1};
        desc.SampleMask = UINT_MAX;
        desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        desc.RasterizerState.DepthClipEnable = TRUE;
        desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        desc.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        desc.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        desc.DepthStencilState.FrontFace.StencilFailOp =
            desc.DepthStencilState.FrontFace.StencilDepthFailOp =
            desc.DepthStencilState.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
        desc.DepthStencilState.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        desc.DepthStencilState.BackFace = desc.DepthStencilState.FrontFace;
        auto& rt = desc.BlendState.RenderTarget[0];
        rt.BlendEnable = blend != GpuBlend::Opaque;
        rt.SrcBlend = premultiplied || dualSource ? D3D12_BLEND_ONE : D3D12_BLEND_SRC_ALPHA;
        rt.DestBlend = dualSource ? D3D12_BLEND_INV_SRC1_ALPHA : blend == GpuBlend::Additive ? D3D12_BLEND_ONE :
            D3D12_BLEND_INV_SRC_ALPHA;
        rt.BlendOp = rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D12_BLEND_ONE;
        rt.DestBlendAlpha = dualSource ? D3D12_BLEND_INV_SRC1_ALPHA : blend == GpuBlend::Additive ? D3D12_BLEND_ONE :
            D3D12_BLEND_INV_SRC_ALPHA;
        rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
        desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets = 1;
        desc.RTVFormats[0] = format;
        desc.SampleDesc.Count = 1;
        ComPtr<ID3D12PipelineState> result;
        Check(device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&result)),
            "CreateGraphicsPipelineState", device);
        return result;
    }

    void InitGraphics() {
        constexpr UINT staticIndexCount = 65536 + 65536 / 4 * 6;
        staticIndices = Buffer(staticIndexCount * sizeof(unsigned short), D3D12_HEAP_TYPE_UPLOAD);
        unsigned short* pattern = nullptr;
        const D3D12_RANGE noRead{0, 0};
        Check(staticIndices->Map(0, &noRead, reinterpret_cast<void**>(&pattern)),
            "Map immutable primitive indices", device);
        for (UINT i = 0; i < 65536; ++i) pattern[i] = static_cast<unsigned short>(i);
        auto* quads = pattern + 65536;
        for (UINT i = 0; i < 65536; i += 4) {
            const unsigned short quad[]{static_cast<unsigned short>(i), static_cast<unsigned short>(i + 1),
                static_cast<unsigned short>(i + 2), static_cast<unsigned short>(i + 1),
                static_cast<unsigned short>(i + 2), static_cast<unsigned short>(i + 3)};
            std::memcpy(quads, quad, sizeof(quad)); quads += 6;
        }
        staticIndices->Unmap(0, nullptr);
        gameRoot = Root(4, TextureSlots, true, true, false);
        const auto vs = Shader(directory, "game_vs"), ps = Shader(directory, "game_ps");
        for (UINT i = 0; i < 3; ++i) {
            scenePipelines[i] = Graphics(gameRoot.Get(), vs, ps,
                DXGI_FORMAT_B8G8R8A8_UNORM, static_cast<GpuBlend>(i), false);
            imagePipelines[i] = Graphics(gameRoot.Get(), vs, ps,
                DXGI_FORMAT_R16G16B16A16_FLOAT, static_cast<GpuBlend>(i), true);
        }
        spriteOpaquePipeline = Graphics(gameRoot.Get(), vs, Shader(directory, "game_sprite_ps"),
            DXGI_FORMAT_B8G8R8A8_UNORM, GpuBlend::Opaque, false);
        mixedSpritePipeline = Graphics(gameRoot.Get(), vs, Shader(directory, "game_sprite_mixed_ps"),
            DXGI_FORMAT_B8G8R8A8_UNORM, GpuBlend::Normal, false, false, nullptr, true);
        if (context.hdr) {
            presentRoot = Root(8, 1, false, true, false);
            presentPipeline = Graphics(presentRoot.Get(), Shader(directory, "game_present_vs"),
                Shader(directory, "game_present_ps"), DXGI_FORMAT_R16G16B16A16_FLOAT,
                GpuBlend::Opaque, false);
        }
    }

    void InitPool() {
        if (poolVertices) return;
        poolRoot = Root(4, TextureSlots, true, true, false, true);
        const auto vs = Shader(directory, "game_pool_vs"), ps = Shader(directory, "game_ps");
        for (UINT i = 0; i < 3; ++i) {
            poolScenePipelines[i] = Graphics(poolRoot.Get(), vs, ps,
                DXGI_FORMAT_B8G8R8A8_UNORM, GpuBlend(i), false, true);
            poolImagePipelines[i] = Graphics(poolRoot.Get(), vs, ps,
                DXGI_FORMAT_R16G16B16A16_FLOAT, GpuBlend(i), true, true);
        }
        auto upload = [&](const void* data, size_t size) {
            auto buffer = Buffer(size, D3D12_HEAP_TYPE_UPLOAD);
            void* mapped = nullptr;
            const D3D12_RANGE readRange{0, 0};
            Check(buffer->Map(0, &readRange, &mapped), "Map immutable pool mesh", device);
            std::memcpy(mapped, data, size);
            buffer->Unmap(0, nullptr);
            return buffer;
        };
        poolIndices = upload(GpuPoolIndices.data(), sizeof(GpuPoolIndices));
        poolVertices = upload(GpuPoolVertices.data(), sizeof(GpuPoolVertices));
    }

    UINT Slot() {
        for (UINT i = 1; i < staticBase; ++i)
            if (!allocated[i]) {
                allocated[i] = true;
                return i;
            }
        throw std::runtime_error("D3D12 texture descriptor slots exhausted");
    }

    void Srv(D3d12Texture* texture, UINT index) {
        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};
        desc.Format = texture->format;
        desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        desc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(texture->resource.Get(), &desc, Cpu(index));
    }

    void Register(D3d12Texture* texture, UINT staticSlot = InvalidSlot) {
        if (inFlight) Begin();
        texture->staticSlot = staticSlot;
        const bool preserve = staticSlot != InvalidSlot && owners[staticSlot] &&
            (owners[staticSlot]->sampled || owners[staticSlot]->slot != staticSlot);
        texture->slot = staticSlot == InvalidSlot || preserve ? Slot() : staticSlot;
        allocated[texture->slot] = true;
        owners[texture->slot] = texture;
        Srv(texture, texture->slot);
        if (staticSlot != InvalidSlot) owners[staticSlot] = texture;
    }

    std::unique_ptr<D3d12Texture> Image(int w, int h, DXGI_FORMAT format,
        bool target, bool uav, bool registered,
        std::array<float, 4> clearColor = {0, 0, 0, 0}) {
        Dimensions(w, h);
        if (inFlight) Begin();
        auto image = std::make_unique<D3d12Texture>();
        image->width = w;
        image->height = h;
        image->format = format;
        image->clearColor = clearColor;
        image->generated = format == DXGI_FORMAT_R16G16B16A16_FLOAT;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width = UINT64(w);
        desc.Height = UINT(h);
        desc.DepthOrArraySize = desc.MipLevels = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Flags = (target ? D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET :
            D3D12_RESOURCE_FLAG_NONE) |
            (uav ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE);
        D3D12_HEAP_PROPERTIES properties{};
        properties.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_CLEAR_VALUE optimizedClear{};
        optimizedClear.Format = format;
        std::copy(clearColor.begin(), clearColor.end(), optimizedClear.Color);
        Check(device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &desc,
            D3D12_RESOURCE_STATE_COMMON, target ? &optimizedClear : nullptr,
            IID_PPV_ARGS(&image->resource)),
            "CreateCommittedResource(texture)", device);
        if (target) {
            if (freeRtvs.empty())
                throw std::runtime_error("D3D12 RTV slots exhausted");
            image->rtv = freeRtvs.back();
            freeRtvs.pop_back();
            device->CreateRenderTargetView(image->resource.Get(), nullptr, Rtv(image->rtv));
        }
        if (registered) Register(image.get());
        return image;
    }

    D3d12Texture* Own(std::unique_ptr<D3d12Texture> texture, bool registered) {
        auto* handle = texture.get();
        (registered ? textures : raw).emplace(handle, std::move(texture));
        return handle;
    }

    bool Known(D3d12Texture* texture) const {
        if (!texture) return false;
        if (textures.contains(texture) || raw.contains(texture) || texture == scene.get())
            return true;
        for (const auto& mode : scaleModes)
            if (mode && texture == mode->sampledOutput.get()) return true;
        return false;
    }
    void Require(D3d12Texture* texture) const {
        if (!Known(texture))
            throw std::invalid_argument("Foreign or destroyed D3D12 texture");
    }

    void InvalidateDrawState() {
        boundTarget = nullptr;
        boundPipeline = nullptr;
        boundVertex = nullptr;
        boundIndex = nullptr;
        gameBound = hasScissor = hasConstants = false;
    }
    void Begin() {
        if (commands) return;
        commands = context.BeginCommands();
        if (inFlight) {
            Completed(false);
            inFlight = false;
        }
        BindHeaps();
        InvalidateDrawState();
    }
    void BindHeaps() {
        ID3D12DescriptorHeap* heaps[] = {heap.Get(), samplerHeap.Get()};
        commands->SetDescriptorHeaps(2, heaps);
    }
    void Transition(D3d12Texture* texture, D3D12_RESOURCE_STATES state) {
        if (texture->state == state) return;
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = texture->resource.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = texture->state;
        barrier.Transition.StateAfter = state;
        commands->ResourceBarrier(1, &barrier);
        texture->state = state;
    }
    void UavBarrier(D3d12Texture* texture) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = texture->resource.Get();
        commands->ResourceBarrier(1, &barrier);
    }

    ComPtr<ID3D12Resource> Buffer(UINT64 bytes, D3D12_HEAP_TYPE type) {
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = bytes;
        desc.Height = desc.DepthOrArraySize = desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        D3D12_HEAP_PROPERTIES properties{};
        properties.Type = type;
        ComPtr<ID3D12Resource> result;
        Check(device->CreateCommittedResource(&properties, D3D12_HEAP_FLAG_NONE, &desc,
            type == D3D12_HEAP_TYPE_READBACK ? D3D12_RESOURCE_STATE_COPY_DEST :
                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&result)),
            "CreateCommittedResource(buffer)", device);
        return result;
    }
    Page* Allocate(std::vector<std::unique_ptr<Page>>& pages, UINT64 bytes,
        UINT64 alignment) {
        for (auto& page : pages) {
            const auto offset = Align(page->used, alignment);
            if (offset + bytes <= page->size) {
                page->used = offset;
                return page.get();
            }
        }
        auto page = std::make_unique<Page>();
        page->size = std::max<UINT64>(8 * 1024 * 1024, Align(bytes, alignment));
        page->resource = Buffer(page->size, D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RANGE read{0, 0};
        Check(page->resource->Map(0, &read, reinterpret_cast<void**>(&page->mapped)),
            "Map(upload buffer)", device);
        auto* result = page.get();
        pages.push_back(std::move(page));
        return result;
    }

    void Upload(D3d12Texture* texture, const void* source, int pitch, UINT pixelBytes) {
        if (!source || pitch < int64_t(texture->width) * pixelBytes)
            throw std::invalid_argument("Invalid texture upload pitch/source");
        Begin();
        Flush();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT rows = 0;
        UINT64 rowBytes = 0, bytes = 0;
        const auto desc = texture->resource->GetDesc();
        device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &rows, &rowBytes, &bytes);
        auto* page = Allocate(staging, bytes, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
        footprint.Offset = page->used;
        for (UINT y = 0; y < rows; ++y)
            std::memcpy(page->mapped + footprint.Offset + UINT64(y) * footprint.Footprint.RowPitch,
                static_cast<const std::byte*>(source) + size_t(y) * pitch, size_t(rowBytes));
        page->used += bytes;
        Transition(texture, D3D12_RESOURCE_STATE_COPY_DEST);
        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = page->resource.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = footprint;
        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = texture->resource.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        commands->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        Transition(texture, SampleState);
    }

    void ReleaseSlot(D3d12Texture* texture, UINT slot) {
        if (slot == InvalidSlot || owners[slot] != texture) return;
        owners[slot] = nullptr;
        allocated[slot] = false;
        D3D12_SHADER_RESOURCE_VIEW_DESC view{};
        view.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        view.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        view.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        view.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(nullptr, &view, Cpu(slot));
    }
    void ReleaseSlots(D3d12Texture* texture) {
        ReleaseSlot(texture, texture->slot);
        if (texture->staticSlot != texture->slot) ReleaseSlot(texture, texture->staticSlot);
        if (texture->rtv != InvalidSlot) freeRtvs.push_back(texture->rtv);
    }
    void Completed(bool keepScratch) {
        // Only mutable versions need temporary dynamic slots. Once the fence
        // completes, restore their compiled static index and release the spare.
        for (auto& [handle, texture] : textures)
            if (texture->staticSlot != InvalidSlot && texture->slot != texture->staticSlot &&
                owners[texture->staticSlot] == texture.get()) {
                ReleaseSlot(texture.get(), texture->slot);
                texture->slot = texture->staticSlot;
                allocated[texture->slot] = true;
                Srv(texture.get(), texture->slot);
            }
        for (auto& texture : retired) ReleaseSlots(texture.get());
        retired.clear();
        for (auto& [handle, texture] : textures) texture->sampled = false;
        for (auto& [handle, texture] : raw) texture->sampled = false;
        if (!keepScratch) raw.clear();
        scene->sampled = false;
        for (auto& mode : scaleModes)
            if (mode) mode->sampledOutput->sampled = false;
        for (auto& page : staging) page->used = 0;
        for (auto& page : vertices) page->used = 0;
        for (auto& page : indices) page->used = 0;
        temporary = TextureSlots;
        InvalidateDrawState();
    }
    void Finish(bool present, bool keepScratch = false) {
        Flush();
        // Present can fail after submission. Protect the batch before entering
        // the context so destruction still waits for any work already queued.
        inFlight = true;
        try {
            if (present) context.Present();
            else context.SubmitAndWait();
        } catch (...) {
            commands = nullptr;
            throw;
        }
        commands = nullptr;
        if (!present) {
            Completed(keepScratch);
            inFlight = false;
        }
    }

    void Drain() {
        if (commands) {
            Finish(false);
            return;
        }
        context.WaitIdle();
        if (inFlight) {
            Completed(false);
            inFlight = false;
        }
    }

    UINT Temporary(UINT count) {
        if (temporary + count > HeapSlots) {
            // A bounded descriptor arena is recycled only after its fence.
            Finish(false, true);
            Begin();
        }
        const auto result = temporary;
        temporary += count;
        return result;
    }

    void Snapshot(D3d12Texture* image, bool force = false) {
        if (!force && !image->sampled) return;
        Flush();
        Begin();
        auto next = Image(image->width, image->height, image->format, true, true,
            image->slot != InvalidSlot, image->clearColor);
        Transition(image, D3D12_RESOURCE_STATE_COPY_SOURCE);
        Transition(next.get(), D3D12_RESOURCE_STATE_COPY_DEST);
        commands->CopyResource(next->resource.Get(), image->resource.Get());
        Transition(image, SampleState);
        next->opaque = image->opaque;
        // Move the old version, including its immutable SRV, behind the fence.
        auto old = std::make_unique<D3d12Texture>(std::move(*image));
        *image = std::move(*next);
        image->staticSlot = old->staticSlot;
        old->staticSlot = InvalidSlot;
        if (old->slot != InvalidSlot) owners[old->slot] = old.get();
        if (image->slot != InvalidSlot) owners[image->slot] = image;
        if (image->staticSlot != InvalidSlot) owners[image->staticSlot] = image;
        retired.push_back(std::move(old));
        InvalidateDrawState();
    }

    D3d12Texture* Output() const { return renderImage ? renderImage : scene.get(); }
    ScaleMode* ActiveScale() const {
        return scaleEnabled ? scaleModes[scaleMode].get() : nullptr;
    }
    void InitializeScene() {
        if (sceneInitialized) return;
        Transition(scene.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
        static constexpr float clear[]{0, 0, 0, 1};
        commands->ClearRenderTargetView(Rtv(scene->rtv), clear, 0, nullptr);
        sceneInitialized = true;
    }
    void RecordScale(ScaleMode& mode) {
        Flush();
        mode.pipeline->Record(commands, UINT(scene->state));
        scene->state = D3D12_RESOURCE_STATES(mode.pipeline->inputState);
        mode.sampledOutput->state = D3D12_RESOURCE_STATES(mode.pipeline->outputState);
        BindHeaps();
        InvalidateDrawState();
    }
    void RecordCaustics() {
        if (!causticQueued || !causticTarget) return;
        if (causticTarget->sampled) Snapshot(causticTarget);
        const std::array inputs{causticSource};
        const auto offsets=EvaluatePoolCaustics(causticTimeNs);
        Compute(causticKernel, causticTarget, inputs, &offsets, 16, 16);
        causticQueued = false;
    }

    void BindTarget(D3d12Texture* target) {
        if (boundTarget == target && target->state == D3D12_RESOURCE_STATE_RENDER_TARGET)
            return;
        Transition(target, D3D12_RESOURCE_STATE_RENDER_TARGET);
        const auto rtv = Rtv(target->rtv);
        commands->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        D3D12_VIEWPORT viewport{0, 0, float(target->width), float(target->height), 0, 1};
        commands->RSSetViewports(1, &viewport);
        boundTarget = target;
        gameBound = false;
    }

    void Flush() {
        if (!run.count) return;
        auto* target = Output();
        BindTarget(target);
        if (!gameBound) {
            auto* root = gameRoot.Get();
            commands->SetGraphicsRootSignature(root);
            commands->SetGraphicsRootDescriptorTable(1, Gpu(0));
            commands->SetGraphicsRootDescriptorTable(2,
                samplerHeap->GetGPUDescriptorHandleForHeapStart());
            commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            gameBound = true;
            hasConstants = false;
        }
        const std::array<UINT, 4> constants{
            std::bit_cast<UINT>(float(target->width)),
            std::bit_cast<UINT>(float(target->height)),
            target->generated ? 1u : 0u, run.coverage ? 1u : 0u};
        if (!hasConstants || boundConstants != constants) {
            commands->SetGraphicsRoot32BitConstants(0, 4, constants.data(), 0);
            boundConstants = constants;
            hasConstants = true;
        }
        auto* pipeline = run.simple ? (run.blend == GpuBlend::Opaque ?
            spriteOpaquePipeline.Get() : mixedSpritePipeline.Get()) :
            (target->generated ? imagePipelines : scenePipelines).at(size_t(run.blend)).Get();
        if (boundPipeline != pipeline) {
            commands->SetPipelineState(pipeline);
            boundPipeline = pipeline;
        }
        if (!hasScissor || std::memcmp(&boundScissor, &run.scissor, sizeof(boundScissor))) {
            commands->RSSetScissorRects(1, &run.scissor);
            boundScissor = run.scissor;
            hasScissor = true;
        }
        if (boundVertex != run.page) {
            D3D12_VERTEX_BUFFER_VIEW view{run.page->resource->GetGPUVirtualAddress(),
                UINT(run.page->size), sizeof(GpuVertex)};
            commands->IASetVertexBuffers(0, 1, &view);
            boundVertex = run.page;
        }
        const auto indexBuffer = run.indices ? run.indices->resource.Get() : staticIndices.Get();
        if (boundIndex != indexBuffer) {
            D3D12_INDEX_BUFFER_VIEW indexView{indexBuffer->GetGPUVirtualAddress(),
                run.indices ? UINT(run.indices->size) : UINT((65536 + 65536 / 4 * 6) * sizeof(unsigned short)),
                DXGI_FORMAT_R16_UINT};
            commands->IASetIndexBuffer(&indexView);
            boundIndex = indexBuffer;
        }
        commands->DrawIndexedInstanced(run.indexCount, 1,
            run.indices ? run.firstIndex : run.geometry == Run::Quads ? 65536u : 0u,
            INT(run.first), 0);
        run.count = 0;
    }

    // Indices are local to a run, capped at the full uint16 address range.
    // Pure runs use immutable patterns. Only a mixed run writes an index stream.
    template<bool Quads>
    void Append(Page* page, UINT first, UINT count, GpuBlend blend,
        D3D12_RECT scissor, bool coverage, bool simple) {
        constexpr UINT primitive = Quads ? 4 : 3;
        constexpr auto geometry = Quads ? Run::Quads : Run::Triangles;
        while (count) {
            if (!run.count) {
                run = {};
                run.page = page; run.first = first; run.blend = blend;
                run.scissor = scissor; run.coverage = coverage; run.simple = simple;
                run.geometry = geometry;
            }
            UINT chunk = std::min(count, 65536u - run.count);
            chunk -= chunk % primitive;
            if (!chunk) { Flush(); continue; }
            auto write = [](unsigned short* output, UINT base, UINT vertices, bool quads) {
                if (quads) for (UINT i = 0; i < vertices; i += 4) {
                    const auto a = base + i;
                    const unsigned short quad[]{static_cast<unsigned short>(a), static_cast<unsigned short>(a+1),
                        static_cast<unsigned short>(a+2), static_cast<unsigned short>(a+1),
                        static_cast<unsigned short>(a+2), static_cast<unsigned short>(a+3)};
                    std::memcpy(output, quad, sizeof(quad)); output += 6;
                } else for (UINT i = 0; i < vertices; ++i) output[i] = static_cast<unsigned short>(base + i);
            };
            if (run.geometry != geometry && run.geometry != Run::Mixed) {
                run.indices = Allocate(indices, 65536 * 3, sizeof(unsigned short));
                run.firstIndex = UINT(run.indices->used / sizeof(unsigned short));
                write(reinterpret_cast<unsigned short*>(run.indices->mapped + run.indices->used),
                    0, run.count, run.geometry == Run::Quads);
                run.indices->used += run.indexCount * sizeof(unsigned short);
                run.geometry = Run::Mixed;
            }
            const UINT indexCount = Quads ? chunk / 4 * 6 : chunk;
            if (run.geometry == Run::Mixed) {
                write(reinterpret_cast<unsigned short*>(run.indices->mapped + run.indices->used),
                    run.count, chunk, Quads);
                run.indices->used += indexCount * sizeof(unsigned short);
            }
            run.count += chunk; run.indexCount += indexCount;
            first += chunk; count -= chunk;
        }
    }

    template<bool Quads = false>
    void Draw(std::span<const GpuVertex> input, D3d12Texture* texture,
        GpuBlend blend, const GpuClip* clip, bool coverage) {
        if (!sceneOpen) throw std::logic_error("D3D12 draw outside a scene");
        Require(texture);
        if (size_t(blend) >= 3 || input.size() % (Quads ? 4 : 3) ||
            input.size_bytes() > UINT_MAX - sizeof(GpuVertex))
            throw std::invalid_argument("Invalid D3D12 triangle list/blend");
        if (input.empty()) return;
        auto* target = Output();
        D3D12_RECT scissor{0, 0, target->width, target->height};
        if (clip) {
            scissor.left = LONG(std::clamp<int64_t>(clip->x, 0, target->width));
            scissor.top = LONG(std::clamp<int64_t>(clip->y, 0, target->height));
            scissor.right = LONG(std::clamp<int64_t>(int64_t(clip->x) + clip->width, 0, target->width));
            scissor.bottom = LONG(std::clamp<int64_t>(int64_t(clip->y) + clip->height, 0, target->height));
        }
        if (scissor.right <= scissor.left || scissor.bottom <= scissor.top) return;
        constexpr UINT valid = 0xfc000fffu;
        constexpr UINT keep = GpuVertex::kIndependentColorFilter |
            GpuVertex::kColorFilterMask | GpuVertex::kAlphaEdgeFilter;
        bool feedback = false;
        auto& sources = drawSources;
        sources.clear();
        sources.reserve(input.size());
        bool simple = !coverage && !target->generated;
        for (const auto& vertex : input) {
            if ((vertex.slot & ~valid) || vertex.sampler > 3 ||
                !owners[vertex.slot & 4095])
                throw std::invalid_argument("Invalid D3D12 vertex descriptor/sampler");
            sources.push_back(owners[vertex.slot & 4095]);
            feedback |= sources.back() == target;
            simple &= !sources.back()->generated && (vertex.slot & keep) == 0;
        }
        if (feedback) {
            Snapshot(target, true);
            auto* oldTarget = retired.back().get();
            for (auto*& source : sources)
                if (source == target) source = oldTarget;
        }
        else if (target->sampled) Snapshot(target);
        simple &= !target->generated;
        // Each source supplies its actual descriptor. Mutable static versions
        // temporarily use a dynamic slot while recorded older versions survive.
        for (auto* source : sources) {
            if (source->state != SampleState) {
                Flush();
                Transition(source, SampleState);
            }
            source->sampled = true;
        }
        auto* page = Allocate(vertices, input.size_bytes(), sizeof(float));
        // Vertex allocations are contiguous multiples of the 40-byte contract.
        const auto first = UINT(page->used / sizeof(GpuVertex));
        // Only ordinary normal/additive draws share the dual-source pipeline.
        // The additive decision is carried per vertex, preserving paint order.
        const auto batchBlend = simple && blend != GpuBlend::Opaque ? GpuBlend::Normal : blend;
        if (run.count && (run.page != page || run.first + run.count != first ||
            run.blend != batchBlend || run.coverage != coverage || run.simple != simple ||
            std::memcmp(&run.scissor, &scissor, sizeof(scissor)) ||
            uint64_t(run.count) + input.size() > UINT_MAX))
            Flush();
        auto* copied = reinterpret_cast<GpuVertex*>(page->mapped + page->used);
        for (size_t i = 0; i < input.size(); ++i) {
            copied[i] = input[i];
            copied[i].slot = sources[i]->slot |
                (sources[i]->generated ? 0x80000000u : 0u) |
                (sources[i]->opaque ? GpuVertex::kOpaqueInput : 0u) | (input[i].slot & keep) |
                (simple && blend == GpuBlend::Additive ? 0x02000000u : 0u);
        }
        page->used += input.size_bytes();
        Append<Quads>(page, first, UINT(input.size()), batchBlend, scissor, coverage, simple);
    }

    // Upload primitives keep their existing calls; only the loading overload creates.
    void InitKernel(Kernel& kernel, const char* shader, unsigned int, unsigned int) const {
        if (!kernel.pipeline)
            throw std::logic_error(std::string("D3D12 compute pipeline was not prepared during loading: ") +
                shader);
    }

    void InitKernel(Kernel& kernel, const char* shader, unsigned int constants,
        unsigned int inputs, LoadingPipelines) {
        if (kernel.pipeline) return;
        kernel.constants = constants;
        kernel.inputs = inputs;
        kernel.root = Root(constants, inputs, false, false, true);
        const auto code = Shader(directory, shader);
        D3D12_COMPUTE_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature = kernel.root.Get();
        desc.CS = {code.data(), code.size()};
        Check(device->CreateComputePipelineState(&desc, IID_PPV_ARGS(&kernel.pipeline)),
            "CreateComputePipelineState", device);
    }

    void Compute(Kernel& kernel, D3d12Texture* target,
        std::span<D3d12Texture* const> inputs, const void* constants, UINT gx, UINT gy) {
        Begin();
        Flush();
        const UINT base = Temporary(kernel.inputs + 1);
        for (UINT i = 0; i < kernel.inputs; ++i) {
            Transition(inputs[i], SampleState);
            inputs[i]->sampled = true;
            Srv(inputs[i], base + i);
        }
        Transition(target, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        D3D12_UNORDERED_ACCESS_VIEW_DESC view{};
        view.Format = target->format;
        view.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(target->resource.Get(), nullptr, &view,
            Cpu(base + kernel.inputs));
        commands->SetComputeRootSignature(kernel.root.Get());
        commands->SetPipelineState(kernel.pipeline.Get());
        commands->SetComputeRoot32BitConstants(0, kernel.constants, constants, 0);
        commands->SetComputeRootDescriptorTable(1, Gpu(base));
        commands->SetComputeRootDescriptorTable(2, Gpu(base + kernel.inputs));
        commands->Dispatch(gx, gy, 1);
        UavBarrier(target);
        Transition(target, SampleState);
        InvalidateDrawState();
    }

    D3d12Texture* Process(D3d12Texture* source, const GpuImageProcessing& processing,
        bool registered) {
        if (inFlight) Begin();
        Require(source);
        Dimensions(processing.width, processing.height);
        if (UINT(processing.operation) > UINT(GpuImageOperation::Colorize))
            throw std::invalid_argument("Invalid D3D12 image operation");
        InitKernel(colorKernel, "game_color_cs", 12, 1);
        auto* target = Own(Image(processing.width, processing.height,
            DXGI_FORMAT_R8G8B8A8_UNORM, false, true, registered), registered);
        struct Constants {
            GpuImageProcessing processing;
            int width, height;
            UINT slot;
        } constants{processing, source->width, source->height, D3d12Renderer::SlotOf(source)};
        static_assert(sizeof(constants) == 48);
        const std::array inputs{source};
        Compute(colorKernel, target, inputs, &constants,
            (UINT(target->width) + 7) / 8, (UINT(target->height) + 7) / 8);
        return target;
    }

    GpuClip Rectangle(int w, int h) const {
        if (w <= 0 || h <= 0) return {0, 0, 0, 0};
        if (const auto* mode = ActiveScale()) {
            const auto image = mode->pipeline->output;
            return {(w - int(image.width)) / 2, (h - int(image.height)) / 2,
                int(image.width), int(image.height)};
        }
        int rw = w, rh = h;
        if (int64_t(w) * height <= int64_t(h) * width)
            rh = std::max(1, int(int64_t(w) * height / width));
        else rw = std::max(1, int(int64_t(h) * width / height));
        return {(w - rw) / 2, (h - rh) / 2, rw, rh};
    }

    bool Present() {
        Flush();
        if (Output() != scene.get()) Transition(Output(), SampleState);
        // Reserve before BeginPresent: recycling must never submit an active back buffer.
        const UINT descriptor = context.hdr ? Temporary(1) : 0;
        D3d12Texture* source = scene.get();
        if (auto* mode = ActiveScale()) {
            if (!mode->warm) throw std::logic_error("D3D12 scale chain has not completed warm-up");
            RecordScale(*mode);
            source = mode->sampledOutput.get();
        } else Transition(scene.get(), SampleState);
        if (!context.BeginPresent()) return false;
        const D3D12_CPU_DESCRIPTOR_HANDLE rtv{SIZE_T(context.BackBufferRtv())};
        static constexpr float black[]{0, 0, 0, 1};
        commands->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
        commands->ClearRenderTargetView(rtv, black, 0, nullptr);
        const auto rect = Rectangle(int(context.width), int(context.height));
        const float x = float(rect.x), y = float(rect.y);
        const float w = float(rect.width), h = float(rect.height);
        const UINT slot = source->slot;
        const std::array<GpuVertex, 6> quad{{
            {x,y,1,1,1,1,0,0,slot,1}, {x+w,y,1,1,1,1,1,0,slot,1},
            {x+w,y+h,1,1,1,1,1,1,slot,1}, {x,y,1,1,1,1,0,0,slot,1},
            {x+w,y+h,1,1,1,1,1,1,slot,1}, {x,y+h,1,1,1,1,0,1,slot,1}
        }};
        auto* page = Allocate(vertices, sizeof(quad), sizeof(float));
        const UINT first = UINT(page->used / sizeof(GpuVertex));
        std::memcpy(page->mapped + page->used, quad.data(), sizeof(quad));
        page->used += sizeof(quad);
        D3D12_VERTEX_BUFFER_VIEW vertexView{page->resource->GetGPUVirtualAddress(),
            UINT(page->size), sizeof(GpuVertex)};
        D3D12_VIEWPORT viewport{0, 0, float(context.width), float(context.height), 0, 1};
        D3D12_RECT scissor{0, 0, LONG(context.width), LONG(context.height)};
        commands->RSSetViewports(1, &viewport);
        commands->RSSetScissorRects(1, &scissor);
        commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commands->IASetVertexBuffers(0, 1, &vertexView);
        if (context.hdr) {
            Srv(source, descriptor);
            const float constants[]{float(context.width), float(context.height),
                0, 0, hdr.paperWhiteNits, hdr.peakNits, hdr.reinhardPaperNits, hdr.strength};
            commands->SetGraphicsRootSignature(presentRoot.Get());
            commands->SetGraphicsRoot32BitConstants(0, 8, constants, 0);
            commands->SetGraphicsRootDescriptorTable(1, Gpu(descriptor));
            commands->SetPipelineState(presentPipeline.Get());
        } else {
            const UINT constants[]{std::bit_cast<UINT>(float(context.width)),
                std::bit_cast<UINT>(float(context.height)), 0, 0};
            commands->SetGraphicsRootSignature(gameRoot.Get());
            commands->SetGraphicsRoot32BitConstants(0, 4, constants, 0);
            commands->SetGraphicsRootDescriptorTable(1, Gpu(0));
            commands->SetGraphicsRootDescriptorTable(2,
                samplerHeap->GetGPUDescriptorHandleForHeapStart());
            commands->SetPipelineState(scenePipelines[size_t(GpuBlend::Opaque)].Get());
        }
        commands->DrawInstanced(6, 1, first, 0);
        InvalidateDrawState();
        return true;
    }

    ImagePixels Read(D3d12Texture* original) {
        if (sceneOpen) throw std::logic_error("Readback requires a closed D3D12 scene");
        Begin();
        Require(original);
        Flush();
        D3d12Texture* source = original;
        if (source->generated) {
            const GpuImageProcessing copy{0, 0, source->width, source->height,
                255, 255, 255, 255, GpuImageOperation::Copy};
            source = Process(source, copy, false);
        }
        if (source->format != DXGI_FORMAT_B8G8R8A8_UNORM &&
            source->format != DXGI_FORMAT_R8G8B8A8_UNORM)
            throw std::invalid_argument("BGRA export requires a color image");
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT64 bytes = 0;
        const auto desc = source->resource->GetDesc();
        device->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, nullptr, nullptr, &bytes);
        auto buffer = Buffer(bytes, D3D12_HEAP_TYPE_READBACK);
        Transition(source, D3D12_RESOURCE_STATE_COPY_SOURCE);
        D3D12_TEXTURE_COPY_LOCATION src{};
        src.pResource = source->resource.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        D3D12_TEXTURE_COPY_LOCATION dst{};
        dst.pResource = buffer.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        dst.PlacedFootprint = footprint;
        commands->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        Transition(source, SampleState);
        // Diagnostic scratch must remain alive until its mapped bytes have been copied.
        auto originalScratch = raw.extract(original);
        auto convertedScratch = raw.extract(source);
        Finish(false);
        ImagePixels result(original->width, original->height);
        void* mapped = nullptr;
        D3D12_RANGE range{0, SIZE_T(bytes)};
        Check(buffer->Map(0, &range, &mapped), "Map(readback buffer)", device);
        for (int y = 0; y < result.height; ++y)
            std::memcpy(result.pixels.data() + size_t(y) * result.width,
                static_cast<const std::byte*>(mapped) + footprint.Offset +
                    size_t(y) * footprint.Footprint.RowPitch,
                size_t(result.width) * 4);
        D3D12_RANGE written{0, 0};
        buffer->Unmap(0, &written);
        if (source->format == DXGI_FORMAT_R8G8B8A8_UNORM)
            for (auto& pixel : result.pixels)
                pixel = (pixel & 0xff00ff00u) | ((pixel & 255u) << 16) |
                    ((pixel >> 16) & 255u);
        if (original->opaque)
            for (auto& pixel : result.pixels) pixel |= 0xff000000u;
        return result;
    }
};

D3d12Renderer::D3d12Renderer(D3d12Context& context, int width, int height,
    const std::filesystem::path& directory, const GpuHdrParams* hdr)
    : impl_(std::make_unique<Impl>(context, width, height, directory)),
      whiteTexture(impl_->white), causticsTexture(impl_->causticTarget),
      sceneOpen(impl_->sceneOpen),
      sceneWidth(impl_->width), sceneHeight(impl_->height) {
    if (hdr) {
        hdr->Validate();
        if (!context.hdr)
            throw std::invalid_argument("HDR parameters require an linear scRGB D3D12 context");
        impl_->hdr = *hdr;
    }
    impl_->Register(impl_->scene.get());
    const uint32_t white = 0xffffffffu;
    impl_->white = CreateTexture(1, 1, &white, 4, true);
}
D3d12Renderer::~D3d12Renderer() = default;

void D3d12Renderer::PrepareLoadingPipelines() {
    auto& s = *impl_;
    if (s.sceneOpen)
        throw std::logic_error("Prepare D3D12 loading pipelines before opening a scene");
    s.InitPool();
    if (!s.fontKernel.pipeline) {
        D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
        Check(s.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,
            &options, sizeof(options)), "CheckFeatureSupport(font double precision)", s.device);
        if (!options.DoublePrecisionFloatShaderOps)
            throw std::runtime_error("D3D12 font atlas shader requires double precision");
        s.InitKernel(s.fontKernel, "game_font_cs", 13, 1, Impl::LoadingPipelines{});
    }
    s.InitKernel(s.colorKernel, "game_color_cs", 12, 1, Impl::LoadingPipelines{});
    s.InitKernel(s.alphaKernel, "game_alpha_cs", 6, 2, Impl::LoadingPipelines{});
}

D3d12Texture* D3d12Renderer::CreateTexture(int width, int height, const void* bgra,
    int rowPitch, bool opaque) {
    auto& s = *impl_;
    s.Dimensions(width, height);
    if (!bgra || rowPitch < int64_t(width) * 4)
        throw std::invalid_argument("Invalid BGRA texture upload");
    auto* texture = s.Own(s.Image(width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
        false, false, true), true);
    texture->opaque = opaque;
    s.Upload(texture, bgra, rowPitch, 4);
    return texture;
}
D3d12Texture* D3d12Renderer::CreateRawTexture(int width, int height, const void* bgra,
    int rowPitch) {
    auto& s = *impl_;
    s.Dimensions(width, height);
    if (!bgra || rowPitch < int64_t(width) * 4)
        throw std::invalid_argument("Invalid raw BGRA texture upload");
    auto* texture = s.Own(s.Image(width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
        false, false, false), false);
    s.Upload(texture, bgra, rowPitch, 4);
    return texture;
}
D3d12Texture* D3d12Renderer::ReplaceTexture(D3d12Texture* previous, int width,
    int height, const void* bgra, int rowPitch, bool opaque) {
    auto& s = *impl_;
    s.Dimensions(width, height);
    if (!bgra || rowPitch < int64_t(width) * 4)
        throw std::invalid_argument("Invalid replacement BGRA texture");
    if (!previous) return CreateTexture(width, height, bgra, rowPitch, opaque);
    if (!s.textures.contains(previous) || previous == s.white || previous->generated)
        throw std::invalid_argument("Invalid D3D12 replacement texture");
    if (s.inFlight) s.Begin();
    if (!previous->sampled && previous->format == DXGI_FORMAT_B8G8R8A8_UNORM &&
        previous->width == width && previous->height == height && previous->opaque == opaque) {
        s.Upload(previous, bgra, rowPitch, 4);
        return previous;
    }
    auto* next = previous->staticSlot != InvalidSlot ?
        CreateTextureStatic(width, height, bgra, rowPitch, opaque,
            previous->staticSlot - s.staticBase) :
        CreateTexture(width, height, bgra, rowPitch, opaque);
    DestroyTexture(previous);
    return next;
}
void D3d12Renderer::ReserveStaticTextureSlots(uint32_t count) {
    auto& s = *impl_;
    if (count >= TextureSlots - 1)
        throw std::invalid_argument("D3D12 static texture region too large");
    const UINT base = TextureSlots - count - 1;
    if (s.staticBase != TextureSlots && s.staticBase != base)
        throw std::logic_error("D3D12 static texture region already reserved");
    for (UINT i = base; i < TextureSlots; ++i)
        if (s.allocated[i]) throw std::logic_error("Dynamic descriptors overlap static region");
    s.staticBase = base;
}
void D3d12Renderer::BindStaticTexture(D3d12Texture* texture, uint32_t staticSlot) {
    auto& s = *impl_;
    if (!s.textures.contains(texture) || !staticSlot ||
        uint64_t(s.staticBase) + staticSlot >= TextureSlots)
        throw std::invalid_argument("Invalid static D3D12 texture binding");
    if (texture->staticSlot != InvalidSlot && texture->staticSlot != s.staticBase + staticSlot)
        throw std::invalid_argument("D3D12 texture already has another static symbol");
    if (s.inFlight) s.Begin();
    const UINT slot = s.staticBase + staticSlot;
    if (texture->staticSlot == slot) return;
    if (texture->sampled)
        throw std::logic_error("Cannot migrate a recorded D3D12 texture descriptor");
    const bool preserve = s.owners[slot] &&
        (s.owners[slot]->sampled || s.owners[slot]->slot != slot);
    if (!preserve) {
        s.ReleaseSlot(texture, texture->slot);
        texture->slot = slot;
        s.allocated[slot] = true;
        s.Srv(texture, slot);
    }
    texture->staticSlot = slot;
    s.owners[slot] = texture;
}
D3d12Texture* D3d12Renderer::CreateTextureStatic(int width, int height, const void* bgra,
    int rowPitch, bool opaque, uint32_t staticSlot) {
    auto& s = *impl_;
    s.Dimensions(width, height);
    if (!bgra || rowPitch < int64_t(width) * 4)
        throw std::invalid_argument("Invalid static BGRA texture upload");
    if (!staticSlot || uint64_t(s.staticBase) + staticSlot >= TextureSlots)
        throw std::invalid_argument("Invalid static D3D12 texture symbol");
    auto allocation = s.Image(width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
        false, false, false);
    s.Register(allocation.get(), s.staticBase + staticSlot);
    auto* texture = s.Own(std::move(allocation), true);
    texture->opaque = opaque;
    s.Upload(texture, bgra, rowPitch, 4);
    return texture;
}
D3d12Texture* D3d12Renderer::CreateCoverageMask(int width, int height,
    std::span<const uint8_t> coverage, int rowPitch) {
    auto& s = *impl_;
    s.Dimensions(width, height);
    if (rowPitch < width || coverage.size() <
        uint64_t(height - 1) * uint64_t(rowPitch) + uint64_t(width))
        throw std::invalid_argument("Invalid D3D12 coverage source");
    auto* texture = s.Own(s.Image(width, height, DXGI_FORMAT_R8_UNORM,
        false, false, true), true);
    s.Upload(texture, coverage.data(), rowPitch, 1);
    return texture;
}
D3d12Texture* D3d12Renderer::CreateRenderImage(int width, int height) {
    auto& s = *impl_;
    auto* texture = s.Own(s.Image(width, height, DXGI_FORMAT_R16G16B16A16_FLOAT,
        true, true, true), true);
    s.Begin();
    s.Flush();
    s.Transition(texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
    static constexpr float clear[]{0, 0, 0, 0};
    s.commands->ClearRenderTargetView(s.Rtv(texture->rtv), clear, 0, nullptr);
    s.Transition(texture, SampleState);
    return texture;
}
void D3d12Renderer::SetRenderImage(D3d12Texture* image) {
    auto& s = *impl_;
    if (image && (!s.textures.contains(image) || !image->generated))
        throw std::invalid_argument("D3D12 render target must be a live render image");
    if (image == s.renderImage) return;
    if (s.sceneOpen) {
        s.Flush();
        s.Transition(s.Output(), SampleState);
        if (s.imageOnly && !image) {
            s.Finish(false);
            s.sceneOpen = false;
        }
    }
    s.renderImage = image;
    s.InvalidateDrawState();
    if (s.sceneOpen && image) s.Snapshot(image);
}
D3d12Texture* D3d12Renderer::CopyTextureRegion(D3d12Texture* source,
    int x, int y, int width, int height) {
    auto& s = *impl_;
    if (s.inFlight) s.Begin();
    s.Require(source);
    s.Dimensions(width, height);
    if (x < 0 || y < 0 || x >= source->width || y >= source->height ||
        source->format == DXGI_FORMAT_R8_UNORM)
        throw std::invalid_argument("Invalid D3D12 color image copy region");
    auto* target = s.Own(s.Image(width, height, source->format, true,
        source->generated, true), true);
    target->opaque = source->opaque;
    s.Begin();
    s.Flush();
    const int copyWidth = std::min(width, source->width - x);
    const int copyHeight = std::min(height, source->height - y);
    if (copyWidth != width || copyHeight != height) {
        s.Transition(target, D3D12_RESOURCE_STATE_RENDER_TARGET);
        static constexpr float zero[]{0, 0, 0, 0};
        s.commands->ClearRenderTargetView(s.Rtv(target->rtv), zero, 0, nullptr);
    }
    s.Transition(source, D3D12_RESOURCE_STATE_COPY_SOURCE);
    s.Transition(target, D3D12_RESOURCE_STATE_COPY_DEST);
    D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
    src.pResource = source->resource.Get();
    src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dst.pResource = target->resource.Get();
    dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    const D3D12_BOX box{UINT(x), UINT(y), 0, UINT(x + copyWidth), UINT(y + copyHeight), 1};
    s.commands->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);
    s.Transition(source, SampleState);
    s.Transition(target, SampleState);
    source->sampled = true;
    s.InvalidateDrawState();
    return target;
}
D3d12Texture* D3d12Renderer::ProcessImage(D3d12Texture* source,
    const GpuImageProcessing& processing) {
    return impl_->Process(source, processing, true);
}
D3d12Texture* D3d12Renderer::ComposeAlphaRaw(D3d12Texture* base, D3d12Texture* alpha,
    uint32_t color, bool colorOnly) {
    auto& s = *impl_;
    if (s.inFlight) s.Begin();
    s.Require(base);
    s.Require(alpha);
    if (base->width != alpha->width || base->height != alpha->height)
        throw std::invalid_argument("Invalid D3D12 alpha composition dimensions");
    s.InitKernel(s.alphaKernel, "game_alpha_cs", 4, 2);
    auto* target = s.Own(s.Image(base->width, base->height, DXGI_FORMAT_R8G8B8A8_UNORM,
        false, true, false), false);
    struct Constants { int width, height; UINT color, colorOnly; };
    const Constants constants{base->width, base->height, color,
        colorOnly ? 1u : 0u};
    const std::array inputs{base, alpha};
    s.Compute(s.alphaKernel, target, inputs, &constants,
        (UINT(base->width) + 7) / 8, (UINT(base->height) + 7) / 8);
    return target;
}
D3d12Texture* D3d12Renderer::AdoptComposedTexture(D3d12Texture* composed) {
    auto& s = *impl_;
    if (s.inFlight) s.Begin();
    const auto found = s.raw.find(composed);
    if (found == s.raw.end() || composed->format != DXGI_FORMAT_R8G8B8A8_UNORM)
        throw std::invalid_argument("Adopt requires a renderer-owned raw composition");
    s.Register(composed);
    auto node = s.raw.extract(found);
    s.textures.insert(std::move(node));
    return composed;
}
D3d12Texture* D3d12Renderer::ComposeAlpha(D3d12Texture* base, D3d12Texture* alpha,
    uint32_t color, bool colorOnly) {
    return AdoptComposedTexture(ComposeAlphaRaw(base, alpha, color, colorOnly));
}
void D3d12Renderer::RenderFontAtlas(D3d12Texture* target, D3d12Texture* source,
    std::span<const GpuFontGlyph> glyphs, uint32_t samplingFlags, bool forceWhite) {
    auto& s = *impl_;
    s.Require(source);
    if (!s.sceneOpen || s.renderImage != target || !target || !target->generated ||
        target == source)
        throw std::invalid_argument("Font atlas requires an open render-image scene");
    for (const auto& g : glyphs)
        if (g.sourceX < 0 || g.sourceY < 0 || g.sourceWidth <= 0 || g.sourceHeight <= 0 ||
            int64_t(g.sourceX) + g.sourceWidth > source->width ||
            int64_t(g.sourceY) + g.sourceHeight > source->height ||
            g.targetX < 0 || g.targetY < 0 || g.targetWidth <= 0 || g.targetHeight <= 0 ||
            int64_t(g.targetX) + g.targetWidth > target->width ||
            int64_t(g.targetY) + g.targetHeight > target->height)
            throw std::invalid_argument("Invalid D3D12 font atlas glyph rectangle");
    if (glyphs.empty()) return;
    s.InitKernel(s.fontKernel, "game_font_cs", 13, 1);
    s.Snapshot(target);
    for (const auto& glyph : glyphs) {
        struct Constants {
            GpuFontGlyph glyph;
            UINT white;
            int width, height;
            UINT flags, slot;
        } constants{glyph, forceWhite ? 1u : 0u, source->width, source->height,
            samplingFlags, SlotOf(source)};
        static_assert(sizeof(constants) == 52);
        const std::array inputs{source};
        s.Compute(s.fontKernel, target, inputs, &constants,
            (UINT(glyph.targetWidth) + 7) / 8, (UINT(glyph.targetHeight) + 7) / 8);
    }
}
void D3d12Renderer::DestroyTexture(D3d12Texture* texture) {
    if (!texture) return;
    auto& s = *impl_;
    if (texture == s.white) throw std::invalid_argument("Cannot destroy the renderer white texture");
    auto* container = s.textures.contains(texture) ? &s.textures : &s.raw;
    const auto found = container->find(texture);
    if (found == container->end()) throw std::invalid_argument("Foreign or destroyed D3D12 texture");
    if (s.renderImage == texture) {
        if (s.sceneOpen) throw std::logic_error("Cannot destroy an active D3D12 render image");
        s.renderImage = nullptr;
    }
    if (texture == s.causticTarget || texture == s.causticSource) {
        s.causticTarget = s.causticSource = nullptr;
        s.causticQueued = false;
    }
    if (s.commands || s.inFlight) s.retired.push_back(std::move(found->second));
    else s.ReleaseSlots(texture);
    container->erase(found);
}
uint32_t D3d12Renderer::SlotOf(const D3d12Texture* texture) {
    if (!texture || texture->slot == InvalidSlot) return 0;
    return texture->slot |
        (texture->generated ? 0x80000000u : 0u) |
        (texture->opaque ? GpuVertex::kOpaqueInput : 0u);
}
D3d12Texture* D3d12Renderer::CreateCausticsTexture(int width, int height,
    D3d12Texture* source) {
    auto& s = *impl_;
    if (s.sceneOpen || !s.textures.contains(source) || source->width != 256 ||
        source->height != 256 || width != 128 || height != 64)
        throw std::invalid_argument("Caustics require a 256x256 source and a 128x64 output");
    auto* target = s.Own(s.Image(width, height, DXGI_FORMAT_R8G8B8A8_UNORM,
        true, true, true, {1, 1, 1, 0}), true);
    s.Begin();
    s.Flush();
    s.Transition(target, D3D12_RESOURCE_STATE_RENDER_TARGET);
    static constexpr float clear[]{1, 1, 1, 0};
    s.commands->ClearRenderTargetView(s.Rtv(target->rtv), clear, 0, nullptr);
    s.Transition(target, SampleState);
    UseCausticsTexture(target, source);
    return target;
}
void D3d12Renderer::UseCausticsTexture(D3d12Texture* output, D3d12Texture* source) {
    auto& s = *impl_;
    if (s.sceneOpen || !s.textures.contains(output) || !s.textures.contains(source) ||
        output == source || output->width != 128 || output->height != 64 ||
        source->width != 256 || source->height != 256 ||
        output->format != DXGI_FORMAT_R8G8B8A8_UNORM ||
        !(output->resource->GetDesc().Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS))
        throw std::invalid_argument("Invalid D3D12 GPU caustics selection");
    s.causticTarget = output;
    s.causticSource = source;
}
void D3d12Renderer::SetCausticsTimeNs(unsigned long long timeNs) {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("Set caustics nanosecond time between D3D12 scenes");
    s.causticTimeNs = timeNs;
    s.causticQueued = s.causticTarget != nullptr;
}
bool D3d12Renderer::HasCausticsCompute() const { return impl_->causticKernel.pipeline != nullptr; }
bool D3d12Renderer::BeginScene() {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("D3D12 scene already open");
    s.Begin();
    if (!s.renderImage && !s.context.BackBuffer()) return false;
    s.sceneOpen = true;
    s.imageOnly = s.renderImage != nullptr;
    s.InvalidateDrawState();
    s.RecordCaustics();
    if (s.renderImage) s.Snapshot(s.renderImage);
    else s.InitializeScene();
    return true;
}
void D3d12Renderer::SubmitTriangles(std::span<const GpuVertex> vertices,
    D3d12Texture* texture, bool linear, bool wrap, GpuBlend blend, const GpuClip* clip) {
    // Sampling is encoded independently for each vertex in the shared contract.
    (void)linear;
    (void)wrap;
    impl_->Draw(vertices, texture, blend, clip, false);
}
void D3d12Renderer::SubmitQuads(std::span<const GpuVertex> vertices,
    D3d12Texture* texture, bool linear, bool wrap, GpuBlend blend, const GpuClip* clip) {
    (void)linear;
    (void)wrap;
    impl_->Draw<true>(vertices, texture, blend, clip, false);
}
void D3d12Renderer::SubmitCoverageTriangles(std::span<const GpuVertex> vertices,
    D3d12Texture* mask, GpuBlend blend, const GpuClip* clip) {
    impl_->Require(mask);
    if (mask->format != DXGI_FORMAT_R8_UNORM)
        throw std::invalid_argument("D3D12 coverage draw requires an R8 mask");
    impl_->Draw(vertices, mask, blend, clip, true);
}
void D3d12Renderer::SubmitPool(D3d12Texture* texture, const GpuPoolParams& p) {
    auto& s = *impl_;
    if (!s.sceneOpen) throw std::logic_error("D3D12 pool draw outside a scene");
    s.Require(texture);
    if (!s.poolVertices) throw std::logic_error("Prepare D3D12 pool pipelines during loading");
    if (p.layer > 2 || p.sampler > 3 || size_t(p.blend) >= 3)
        throw std::invalid_argument("Invalid D3D12 pool draw parameters");
    s.Flush();
    auto* target = s.Output();
    auto* source = texture;
    if (source == target) {
        s.Snapshot(target, true);
        source = s.retired.back().get();
    } else if (target->sampled) s.Snapshot(target);
    s.Transition(source, SampleState);
    source->sampled = true;
    s.BindTarget(target);
    s.commands->SetGraphicsRootSignature(s.poolRoot.Get());
    s.commands->SetGraphicsRootDescriptorTable(1, s.Gpu(0));
    s.commands->SetGraphicsRootDescriptorTable(2,
        s.samplerHeap->GetGPUDescriptorHandleForHeapStart());
    const std::array<UINT, 4> scene{
        std::bit_cast<UINT>(float(target->width)), std::bit_cast<UINT>(float(target->height)),
        target->generated ? 1u : 0u, 0u};
    s.commands->SetGraphicsRoot32BitConstants(0, 4, scene.data(), 0);
    constexpr UINT filters = GpuVertex::kIndependentColorFilter |
        GpuVertex::kColorFilterMask | GpuVertex::kAlphaEdgeFilter;
    const std::array<UINT, 16> constants{
        std::bit_cast<UINT>(p.width), std::bit_cast<UINT>(p.height),
        std::bit_cast<UINT>(p.transX), std::bit_cast<UINT>(p.transY),
        std::bit_cast<UINT>(p.time1), std::bit_cast<UINT>(p.time2),
        std::bit_cast<UINT>(p.time3), std::bit_cast<UINT>(p.time5),
        std::bit_cast<UINT>(float(p.clip.x)), std::bit_cast<UINT>(float(p.clip.y)),
        std::bit_cast<UINT>(float(p.clip.width)), std::bit_cast<UINT>(float(p.clip.height)),
        p.layer, p.night, source->slot | (p.samplingFlags & filters) |
            (source->generated ? 0x80000000u : 0u) |
            (source->opaque ? GpuVertex::kOpaqueInput : 0u), p.sampler};
    s.commands->SetGraphicsRoot32BitConstants(3, 16, constants.data(), 0);
    s.commands->SetPipelineState((target->generated ? s.poolImagePipelines :
        s.poolScenePipelines).at(size_t(p.blend)).Get());
    const D3D12_VERTEX_BUFFER_VIEW vertices{s.poolVertices->GetGPUVirtualAddress(),
        UINT(sizeof(GpuPoolVertices)), sizeof(GpuPoolVertex)};
    const D3D12_INDEX_BUFFER_VIEW indices{s.poolIndices->GetGPUVirtualAddress(),
        UINT(sizeof(GpuPoolIndices)), DXGI_FORMAT_R16_UINT};
    s.commands->IASetVertexBuffers(0, 1, &vertices);
    s.commands->IASetIndexBuffer(&indices);
    s.commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    const D3D12_RECT scissor{0, 0, target->width, target->height};
    s.commands->RSSetScissorRects(1, &scissor);
    s.commands->DrawIndexedInstanced(UINT(GpuPoolIndices.size()), 1, 0, 0, 0);
    s.InvalidateDrawState();
}
void D3d12Renderer::EndSceneAndPresent() {
    auto& s = *impl_;
    if (!s.sceneOpen) throw std::logic_error("No open scene");
    s.Flush();
    if (s.imageOnly) {
        s.Transition(s.renderImage, SampleState);
        s.Finish(false);
    } else {
        s.Present();
        s.Finish(true);
    }
    s.sceneOpen = false;
}
bool D3d12Renderer::PresentRetainedScene() {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("Retained presentation requires a closed scene");
    if (!s.sceneInitialized) return false;
    s.Begin();
    const bool presented = s.Present();
    s.Finish(true);
    return presented;
}
bool D3d12Renderer::ResizePresent() {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("Resize requires a closed D3D12 scene");
    s.Drain();
    s.context.RecreateSwapchain();
    return s.context.BackBuffer() != nullptr;
}
void D3d12Renderer::ResizeScene(int width, int height) {
    auto& s = *impl_;
    if (s.sceneOpen || s.renderImage)
        throw std::logic_error("Resize the main D3D12 canvas between scenes");
    if (s.width == width && s.height == height) return;
    s.Dimensions(width, height);
    s.Drain();
    auto next = s.Image(width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
        true, false, false, {0, 0, 0, 1});
    for (auto& mode : s.scaleModes) {
        if (mode) s.ReleaseSlots(mode->sampledOutput.get());
        mode.reset();
    }
    s.scaleEnabled = false;
    s.ReleaseSlots(s.scene.get());
    s.scene = std::move(next);
    s.Register(s.scene.get());
    s.width = width;
    s.height = height;
    s.sceneInitialized = false;
    s.InvalidateDrawState();
}
void D3d12Renderer::SetHdrParams(const GpuHdrParams& params) {
    params.Validate();
    auto& s = *impl_;
    if (!s.context.hdr) throw std::logic_error("SDR D3D12 context has no HDR parameters");
    if (s.sceneOpen) throw std::logic_error("Change HDR parameters between D3D12 scenes");
    s.hdr = params;
}
GpuHdrParams D3d12Renderer::HdrParams() const {
    if (!impl_->context.hdr) throw std::logic_error("SDR D3D12 context has no HDR parameters");
    return impl_->hdr;
}
void D3d12Renderer::PrepareScale(bool fullscreen, std::span<const D3d12ScaleStep> steps,
    uint32_t targetWidth, uint32_t targetHeight) {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("Prepare scale between D3D12 scenes");
    s.Drain();
    if (!s.scaleLibrary)
        s.scaleLibrary = std::make_unique<D3d12ScaleLibrary>(D3d12ScaleDevice{s.device});
    std::unique_ptr<Impl::ScaleMode> next;
    if (!steps.empty()) {
        next = std::make_unique<Impl::ScaleMode>();
        const D3d12ScaleImage input{s.scene->resource.Get(), UINT(s.scene->format),
            UINT(s.width), UINT(s.height)};
        next->pipeline = std::make_unique<D3d12ScalePipeline>(*s.scaleLibrary,
            input, steps, targetWidth, targetHeight);
        const auto output = next->pipeline->output;
        if (!output.resource || !output.width || !output.height)
            throw std::runtime_error("D3D12 scale pipeline has no output");
        next->sampledOutput = std::make_unique<D3d12Texture>();
        auto* texture = next->sampledOutput.get();
        texture->resource = output.resource;
        texture->format = DXGI_FORMAT(output.format);
        texture->width = int(output.width);
        texture->height = int(output.height);
        texture->state = D3D12_RESOURCE_STATES(next->pipeline->outputState);
        s.Register(texture);
    }
    auto& previous = s.scaleModes[unsigned(fullscreen)];
    if (previous) s.ReleaseSlots(previous->sampledOutput.get());
    previous = std::move(next);
    if (s.scaleMode == unsigned(fullscreen)) s.scaleEnabled = false;
}
void D3d12Renderer::WarmScale(bool fullscreen) {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("Warm scale between D3D12 scenes");
    auto* mode = s.scaleModes[unsigned(fullscreen)].get();
    if (!mode || mode->warm) return;
    s.Begin();
    s.InitializeScene();
    s.RecordScale(*mode);
    s.Finish(false);
    mode->warm = true;
}
void D3d12Renderer::SetScaleMode(bool enabled, bool fullscreen) {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("Switch scale modes between D3D12 scenes");
    const auto* mode = s.scaleModes[unsigned(fullscreen)].get();
    if (enabled && mode && !mode->warm)
        throw std::logic_error("D3D12 scale chain has not completed warm-up");
    s.scaleMode = unsigned(fullscreen);
    s.scaleEnabled = enabled && mode;
}
D3d12ScaleImage D3d12Renderer::ScaleOutput(bool fullscreen) const {
    const auto* mode = impl_->scaleModes[unsigned(fullscreen)].get();
    return mode ? mode->pipeline->output : D3d12ScaleImage{};
}
D3d12Renderer::ScaleCounts D3d12Renderer::GetScaleCounts() const {
    const auto& s = *impl_;
    ScaleCounts counts{s.scaleLibrary ? s.scaleLibrary->ProgramCount() : 0,
        s.scaleLibrary ? s.scaleLibrary->LookupCount() : 0, 0};
    for (const auto& mode : s.scaleModes)
        if (mode) counts.writableImages += mode->pipeline->WritableImageCount();
    return counts;
}
GpuClip D3d12Renderer::PresentRect(int width, int height) const {
    return impl_->Rectangle(width, height);
}
std::vector<uint32_t> D3d12Renderer::ReadScenePixels() {
    if (!impl_->sceneInitialized)
        throw std::logic_error("Scene readback requires an initialized D3D12 scene");
    return impl_->Read(impl_->scene.get()).pixels;
}
ImagePixels D3d12Renderer::ReadTexturePixels(D3d12Texture* texture) {
    return impl_->Read(texture);
}
std::vector<uint32_t> D3d12Renderer::ReadScalePixels(bool fullscreen) {
    auto& s = *impl_;
    auto* mode = s.scaleModes[unsigned(fullscreen)].get();
    if (s.sceneOpen || !mode || !mode->warm)
        throw std::logic_error("Scale readback requires a completed D3D12 scale pipeline");
    auto result = s.Read(mode->sampledOutput.get());
    mode->pipeline->outputState = UINT(mode->sampledOutput->state);
    return std::move(result.pixels);
}
uint32_t D3d12Renderer::MaxImageDimension2D() const {
    return D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
}
D3d12Renderer::ResourceCounts D3d12Renderer::GetResourceCounts() const {
    return {impl_->textures.size(), impl_->retired.size() + impl_->raw.size(),
        3, impl_->vertices.size()};
}
}
