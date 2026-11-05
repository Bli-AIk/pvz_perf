#include "D3d9RendererInternal.h"
#include "graphics/vulkan/VulkanViewport.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>

namespace Sexy {
using namespace DX9;
namespace S = D3d9Shaders;
namespace {
std::vector<DWORD> Bytecode(const std::filesystem::path& path, DWORD version)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("Missing precompiled D3D9 shader: " + path.string());
    const auto size = input.tellg();
    if (size < 8 || size % 4 || size > 1024 * 1024)
        throw std::runtime_error("Invalid D3D9 shader bytecode size: " + path.string());
    std::vector<DWORD> code(static_cast<size_t>(size) / sizeof(DWORD));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(code.data()), size) || code.front() != version ||
        code.back() != D3DSIO_END)
        throw std::runtime_error("Invalid SM3 D3D9 bytecode: " + path.string());
    return code;
}
bool Same(const GpuClip& a, const GpuClip& b)
{
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}
unsigned long long FontInstructionBound(const std::vector<DWORD>& code)
{
    unsigned long long instructions = 0;
    std::array<DWORD, 16> repeatCounts{};
    static constexpr std::array<DWORD, 4> expectedCounts{4, 129, 129, 6};
    size_t depth = 0;
    for (size_t i = 1; i < code.size();) {
        const DWORD token = code[i++];
        const auto opcode = token & D3DSI_OPCODE_MASK;
        if (opcode == D3DSIO_END) break;
        const auto length = opcode == D3DSIO_COMMENT ?
            (token & D3DSI_COMMENTSIZE_MASK) >> D3DSI_COMMENTSIZE_SHIFT :
            (token & D3DSI_INSTLENGTH_MASK) >> D3DSI_INSTLENGTH_SHIFT;
        if (length > code.size() - i) throw std::runtime_error("Malformed D3D9 shader instruction");
        if (opcode == D3DSIO_COMMENT) { i += length; continue; }
        unsigned int slots = 1;
        switch (opcode) {
        case D3DSIO_DCL: case D3DSIO_DEF: case D3DSIO_DEFI:
            slots = 0;
            break;
        case D3DSIO_TEXLDL: case D3DSIO_ENDREP:
            slots = 2;
            break;
        case D3DSIO_REP: case D3DSIO_IFC: case D3DSIO_BREAKC:
            slots = 3;
            break;
        case D3DSIO_ABS: case D3DSIO_ADD: case D3DSIO_BREAK: case D3DSIO_CMP:
        case D3DSIO_ENDIF: case D3DSIO_FRC: case D3DSIO_IF: case D3DSIO_MAX:
        case D3DSIO_MIN: case D3DSIO_MOV: case D3DSIO_MUL: case D3DSIO_RCP:
        case D3DSIO_TEXKILL:
            break;
        default:
            throw std::runtime_error("Unsupported instruction in D3D9 font execution budget");
        }
        instructions += slots * (depth == 4 ? 6 : 1);
        if (opcode == D3DSIO_DEFI) {
            if (length != 5 || (code[i] & D3DSP_REGNUM_MASK) >= repeatCounts.size())
                throw std::runtime_error("Invalid D3D9 font loop constant");
            repeatCounts[code[i] & D3DSP_REGNUM_MASK] = code[i + 1];
        } else if (opcode == D3DSIO_REP) {
            if (length != 1 || depth == expectedCounts.size() ||
                (code[i] & D3DSP_REGNUM_MASK) >= repeatCounts.size() ||
                repeatCounts[code[i] & D3DSP_REGNUM_MASK] != expectedCounts[depth])
                throw std::runtime_error("Unsupported D3D9 font loop execution budget");
            ++depth;
        } else if (opcode == D3DSIO_ENDREP) {
            if (!depth) throw std::runtime_error("Unbalanced D3D9 font loop");
            --depth;
        }
        i += length;
    }
    if (depth) throw std::runtime_error("Unbalanced D3D9 font loop");
    // The first three loops traverse tap pairs linearly. Only the fourth loop
    // repeats work within one pair (six alpha-edge neighbors). Count setup,
    // branches, and outer-loop boundaries once per pair as an overestimate.
    return std::max(1ULL, instructions);
}
}
void D3d9Renderer::Impl::Dimensions(int w, int h) const
{
    if (w <= 0 || h <= 0 || w > maximum || h > maximum)
        throw std::invalid_argument("D3D9 texture dimensions exceed hardware limits");
}
void D3d9Renderer::Impl::InitializeTexture(D3d9Texture& result, int w, int h, D3DFORMAT format)
{
    Dimensions(w, h);
    result.width = w; result.height = h; result.format = format;
    Check(device->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT,
        result.image.put(), nullptr), "Allocate D3D9 render texture");
    Check(result.image->GetSurfaceLevel(0, result.surface.put()), "Get D3D9 texture surface");
}
void D3d9Renderer::Impl::Register(D3d9Texture* image)
{
    for (unsigned int slot = 1; slot < staticBase; ++slot)
        if (!slots[slot]) {
            image->slot = slot; slots[slot] = image;
            return;
        }
    throw std::runtime_error("D3D9 logical texture slots exhausted");
}
D3d9Texture* D3d9Renderer::Impl::Allocate(int w, int h, D3DFORMAT format,
    bool registerSlot)
{
    auto* image = new D3d9Texture;
    try {
        InitializeTexture(*image, w, h, format);
        textures.insert(image);
        if (registerSlot) Register(image);
    } catch (...) {
        textures.erase(image);
        delete image;
        throw;
    }
    return image;
}
void D3d9Renderer::Impl::Retire(D3d9Texture* image)
{
    if (!batches.empty()) retiredTextures.push_back(image);
    if (image == causticTarget) { causticTarget = nullptr; causticsQueued = false; }
    if (image == causticSource) causticSource = nullptr;
    if (image->slot != ~0U && slots[image->slot] == image) slots[image->slot] = nullptr;
    textures.erase(image);
    if (batches.empty()) delete image;
}
void D3d9Renderer::Impl::Upload(D3d9Texture* image, const void* pixels, int pitch)
{
    if (!pixels || pitch < static_cast<long long>(image->width) * 4)
        throw std::invalid_argument("Invalid D3D9 BGRA upload");
    Com<IDirect3DSurface9> staging;
    Check(device->CreateOffscreenPlainSurface(image->width, image->height,
        D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, staging.put(), nullptr), "Allocate D3D9 upload staging");
    D3DLOCKED_RECT lock{};
    Check(staging->LockRect(&lock, nullptr, 0), "Lock D3D9 upload staging");
    for (int row = 0; row < image->height; ++row)
        std::memcpy(static_cast<unsigned char*>(lock.pBits) + size_t(row) * lock.Pitch,
            static_cast<const unsigned char*>(pixels) + size_t(row) * pitch, size_t(image->width) * 4);
    Check(staging->UnlockRect(), "Unlock D3D9 upload staging");
    Check(device->UpdateSurface(staging.value, nullptr, image->surface.value, nullptr),
        "Upload D3D9 texture");
    // Staging and caller pixels are disposable immediately after UpdateSurface.
}
D3d9Renderer::Impl::Impl(D3d9Context& ctx, int w, int h, const std::filesystem::path& shaders)
    : context(ctx), device(ctx.device), width(w), height(h), shaderDirectory(shaders)
{
    context.MakeCurrent();
    if (!device || context.hdr) throw std::runtime_error("D3D9 renderer requires an SDR hardware context");
    maximum = static_cast<int>(std::min(context.caps.maxTextureWidth, context.caps.maxTextureHeight));
    if (context.caps.maxSimultaneousTextures < 5 ||
        context.caps.maxPixelShaderInstructionSlots < 512 ||
        context.caps.maxPixelShaderInstructionsExecuted < 65535 ||
        context.caps.pixelShaderDynamicFlowControlDepth < 4)
        throw std::runtime_error("D3D9 SM3 instruction/sampler limits cannot support exact image operations");
    static constexpr D3DVERTEXELEMENT9 elements[]{
        {0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
        {0, 8, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_COLOR, 0},
        {0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
        {0, 32, D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
        {0, 48, D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 2},
        D3DDECL_END()};
    Check(device->CreateVertexDeclaration(elements, declaration.put()), "Create D3D9 vertex declaration");
    vertices.reserve(16384); batches.reserve(1024);
    InitializeStates();
}
D3d9Renderer::Impl::~Impl()
{
    for (unsigned int i = 0; i < 5; ++i) device->SetTexture(i, nullptr);
    device->SetStreamSource(0, nullptr, 0, 0);
    device->SetIndices(nullptr);
    device->SetVertexShader(nullptr); device->SetPixelShader(nullptr);
    device->SetVertexDeclaration(nullptr);
    if (context.backBuffer) device->SetRenderTarget(0, context.backBuffer);
    for (auto* image : retiredTextures) delete image;
    for (auto* image : textures) delete image;
}
void D3d9Renderer::Impl::InitializeStates()
{
    Check(device->SetRenderState(D3DRS_ZENABLE, FALSE), "Disable D3D9 depth");
    Check(device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE), "Disable D3D9 depth writes");
    Check(device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE), "Disable D3D9 culling");
    Check(device->SetRenderState(D3DRS_LIGHTING, FALSE), "Disable D3D9 lighting");
    Check(device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE), "Disable D3D9 alpha test");
    Check(device->SetRenderState(D3DRS_FOGENABLE, FALSE), "Disable D3D9 fog");
    Check(device->SetRenderState(D3DRS_DITHERENABLE, FALSE), "Disable D3D9 dithering");
    Check(device->SetRenderState(D3DRS_SRGBWRITEENABLE, FALSE), "Disable D3D9 sRGB write");
    Check(device->SetRenderState(D3DRS_COLORWRITEENABLE, 15), "Enable D3D9 RGBA writes");
    Check(device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD), "Set D3D9 RGB blend operation");
    Check(device->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD), "Set D3D9 alpha blend operation");
    Check(device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE), "Enable D3D9 separate alpha blend");
    Check(device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE), "Set D3D9 alpha source blend");
    Check(device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA), "Set D3D9 alpha destination blend");
    for (unsigned int i = 0; i < 5; ++i) {
        Check(device->SetSamplerState(i, D3DSAMP_MINFILTER, D3DTEXF_POINT), "Set D3D9 point minification");
        Check(device->SetSamplerState(i, D3DSAMP_MAGFILTER, D3DTEXF_POINT), "Set D3D9 point magnification");
        Check(device->SetSamplerState(i, D3DSAMP_MIPFILTER, D3DTEXF_NONE), "Disable D3D9 mip filtering");
        Check(device->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP), "Set D3D9 U clamp");
        Check(device->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP), "Set D3D9 V clamp");
        Check(device->SetSamplerState(i, D3DSAMP_SRGBTEXTURE, FALSE), "Disable D3D9 sRGB sampling");
    }
}
void D3d9Renderer::Impl::Target(IDirect3DSurface9* surface, int w, int h)
{
    Check(device->SetVertexDeclaration(declaration.value), "Bind D3D9 vertex declaration");
    for (unsigned int i = 0; i < 5; ++i)
        Check(device->SetTexture(i, nullptr), "Unbind D3D9 target feedback");
    Check(device->SetDepthStencilSurface(nullptr), "Unbind D3D9 depth surface");
    Check(device->SetRenderTarget(0, surface), "Bind D3D9 target");
    D3DVIEWPORT9 viewport{0, 0, static_cast<DWORD>(w), static_cast<DWORD>(h), 0, 1};
    Check(device->SetViewport(&viewport), "Set D3D9 viewport");
    Check(device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE), "Disable stale D3D9 target scissor");
}
void D3d9Renderer::Impl::BindTarget()
{
    const auto* target = selected ? selected : scene;
    Target(target->surface.value, target->width, target->height);
}
void D3d9Renderer::Impl::Source(Constants& c, const D3d9Texture* image, unsigned int sampler) const
{
    const auto flags = image->flags;
    c[S::SourceRect] = {0, 0, float(image->width), float(image->height)};
    c[S::SourceTexture] = {1.0f / float(image->width), 1.0f / float(image->height), 0, 0};
    c[S::SourceFlags] = {float(bool(flags & 0x80000000U)), float(bool(flags & GpuVertex::kOpaqueInput)),
        float(bool(flags & GpuVertex::kAlphaEdgeFilter)), float((flags >> 28) & 3)};
    c[S::Sampling] = {float(sampler & 1), float((sampler >> 1) & 1),
        float(bool(flags & GpuVertex::kIndependentColorFilter)), 0};
}
void D3d9Renderer::Impl::SetConstants(const Constants& constants)
{
    Check(device->SetPixelShaderConstantF(0, &constants[0].x, static_cast<UINT>(constants.size())),
        "Set D3D9 pixel constants");
}
void D3d9Renderer::Impl::Projection(int w, int h)
{
    const float projection[]{2.0f / float(w), -2.0f / float(h),
        -1.0f - 1.0f / float(w), 1.0f + 1.0f / float(h)};
    Check(device->SetVertexShaderConstantF(S::Projection, projection, 1), "Set D3D9 projection");
}
void D3d9Renderer::Impl::Flush()
{
    if (vertices.empty()) {
        for (auto* image : retiredTextures) delete image;
        retiredTextures.clear();
        return;
    }
    context.MakeCurrent();
    BindTarget();
    const auto* target = selected ? selected : scene;
    const auto bytes = vertices.size() * sizeof(Vertex);
    if (bytes > std::numeric_limits<UINT>::max()) throw std::runtime_error("D3D9 vertex stream too large");
    if (streamCapacity < bytes) {
        streamCapacity = std::max<size_t>(bytes, 1024 * 1024);
        Check(device->CreateVertexBuffer(static_cast<UINT>(streamCapacity),
            D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, 0, D3DPOOL_DEFAULT, stream.put(), nullptr),
            "Allocate D3D9 vertex stream");
    }
    void* data = nullptr;
    Check(stream->Lock(0, static_cast<UINT>(bytes), &data, D3DLOCK_DISCARD), "Lock D3D9 vertex stream");
    std::memcpy(data, vertices.data(), bytes);
    Check(stream->Unlock(), "Unlock D3D9 vertex stream");
    Check(device->SetStreamSource(0, stream.value, 0, sizeof(Vertex)), "Bind D3D9 vertex stream");
    Check(device->SetVertexShader(sceneVS.value), "Bind D3D9 scene vertex shader");
    Check(device->SetPixelShader(scenePS.value), "Bind D3D9 scene pixel shader");
    Projection(target->width, target->height);
    Check(device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE), "Enable D3D9 scissor");
    Check(device->SetRenderState(D3DRS_SRCBLEND, target->render ? D3DBLEND_ONE : D3DBLEND_SRCALPHA),
        "Set D3D9 source blend");
    Check(device->BeginScene(), "Begin D3D9 draw commands");
    try {
        const Batch* previous = nullptr;
        for (const auto& batch : batches) {
            const bool textureChanged = !previous || previous->image != batch.image;
            if(textureChanged || previous->coverage != batch.coverage) {
                std::array<S::Float4,4> constants{};
                constants[0] = {1.0f / float(batch.width), 1.0f / float(batch.height), 0, 0};
                constants[S::Sampling-S::SourceTexture].w = float(target->render);
                constants[S::Modes-S::SourceTexture].x = float(batch.coverage);
                Check(device->SetPixelShaderConstantF(S::SourceTexture,
                    reinterpret_cast<const float*>(constants.data()),static_cast<UINT>(constants.size())),
                    "Set D3D9 scene constants");
            }
            if(textureChanged)
                Check(device->SetTexture(S::SourceImage, batch.image), "Bind D3D9 batch image");
            if(!previous || !Same(previous->clip,batch.clip)) {
                RECT clip{batch.clip.x, batch.clip.y, batch.clip.x + batch.clip.width, batch.clip.y + batch.clip.height};
                Check(device->SetScissorRect(&clip), "Set D3D9 scissor");
            }
            if(!previous || previous->blend != batch.blend) {
                Check(device->SetRenderState(D3DRS_ALPHABLENDENABLE, batch.blend != GpuBlend::Opaque), "Set D3D9 blending");
                Check(device->SetRenderState(D3DRS_DESTBLEND,
                    batch.blend == GpuBlend::Additive ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA), "Set D3D9 destination blend");
            }
            const size_t limit = std::max(1U, context.caps.maxPrimitiveCount);
            for (size_t first = batch.first, remaining = batch.count / 3; remaining;) {
                const auto count = static_cast<UINT>(std::min(remaining, limit));
                Check(device->DrawPrimitive(D3DPT_TRIANGLELIST, static_cast<UINT>(first), count), "Draw D3D9 triangles");
                remaining -= count; first += size_t(count) * 3;
            }
            previous = &batch;
        }
    } catch (...) { device->EndScene(); throw; }
    Check(device->EndScene(), "End D3D9 draw commands");
    vertices.clear(); batches.clear();
    for (auto* image : retiredTextures) delete image;
    retiredTextures.clear();
}
void D3d9Renderer::Impl::Snapshot(D3d9Texture& copy, D3d9Texture* image)
{
    Flush();
    InitializeTexture(copy, image->width, image->height, image->format);
    copy.flags = image->flags; copy.render = image->render;
    Check(device->StretchRect(image->surface.value, nullptr,
        copy.surface.value, nullptr, D3DTEXF_NONE), "Snapshot D3D9 texture");
}
void D3d9Renderer::Impl::Submit(std::span<const GpuVertex> input, D3d9Texture*,
    GpuBlend blend, const GpuClip* requested, bool coverage)
{
    if (!open || input.size() % 3) throw std::logic_error("D3D9 submission requires an open triangle scene");
    auto* target = selected ? selected : scene;
    GpuClip clip = requested ? *requested : GpuClip{0, 0, target->width, target->height};
    const auto right = std::clamp<long long>(static_cast<long long>(clip.x) + std::max(0, clip.width), 0, target->width);
    const auto bottom = std::clamp<long long>(static_cast<long long>(clip.y) + std::max(0, clip.height), 0, target->height);
    clip.x = std::clamp(clip.x, 0, target->width); clip.y = std::clamp(clip.y, 0, target->height);
    clip.width = static_cast<int>(std::max<long long>(0, right - clip.x));
    clip.height = static_cast<int>(std::max<long long>(0, bottom - clip.y));
    if (!clip.width || !clip.height) return;
    D3d9Texture* feedback = nullptr;
    for (size_t index = 0; index < input.size(); index += 3) {
        const auto& firstVertex = input[index];
        auto* image = slots[firstVertex.slot & 4095];
        if (!image) throw std::logic_error("D3D9 draw references an unbound logical texture");
        auto* texture = image;
        if (image == target) {
            if (!feedback) {
                feedback = new D3d9Texture;
                try {
                    Snapshot(*feedback, image);
                    retiredTextures.push_back(feedback);
                } catch (...) { delete feedback; throw; }
            }
            texture = feedback;
        }
        const auto first = vertices.size();
        const auto flags = firstVertex.slot;
        const auto sampler = coverage ? 0U : firstVertex.sampler;
        const unsigned int packed =
            unsigned(bool(flags & 0x80000000U)) * S::Generated +
            unsigned(bool(flags & GpuVertex::kOpaqueInput)) * S::Opaque +
            unsigned(bool(flags & GpuVertex::kAlphaEdgeFilter)) * S::AlphaEdge +
            ((flags >> 28) & 3) * S::FilterKindUnit +
            unsigned(bool(flags & GpuVertex::kIndependentColorFilter)) * S::Independent +
            (sampler & 1) * S::Linear + ((sampler >> 1) & 1) * S::Repeat;
        for (size_t corner = 0; corner < 3; ++corner) {
            const auto& v = input[index + corner];
            const auto coordinate = [&](float uv, int extent) {
                if (!std::isfinite(uv))
                    throw std::invalid_argument("Non-finite D3D9 source coordinate");
                if (sampler & 2) {
                    const float pixel = uv * float(extent);
                    if (!std::isfinite(pixel) ||
                        std::abs(std::floor(double(pixel))) + 3.0 * extent + 2.0 > 16777216.0)
                        throw std::invalid_argument("D3D9 repeat coordinate exceeds exact float32 modulo range");
                }
            };
            coordinate(v.u, image->width); coordinate(v.v, image->height);
        }
        for (size_t corner = 0; corner < 3; ++corner) {
            const auto& v = input[index + corner];
            vertices.push_back({v.x, v.y, v.r, v.g, v.b, v.a, v.u, v.v,
                {0, 0, float(image->width), float(image->height)}, float(packed)});
        }
        if (!batches.empty() && batches.back().image == texture->image.value && batches.back().blend == blend &&
            batches.back().coverage == coverage && Same(batches.back().clip, clip)) batches.back().count += 3;
        else batches.push_back({texture->image.value, texture->width, texture->height,
            first, 3, blend, clip, coverage});
    }
}
D3d9Renderer::D3d9Renderer(D3d9Context& context, int w, int h,
    const std::filesystem::path& shaders, const VulkanHdrParams* params)
    : impl_(std::make_unique<Impl>(context, w, h, shaders)), whiteTexture(impl_->white),
      causticsTexture(impl_->causticTarget), sceneOpen(impl_->open),
      sceneWidth(impl_->width), sceneHeight(impl_->height)
{
    if (params) { params->Validate(); impl_->hdr = *params; }
    PrepareLoadingPipelines();
    const unsigned int white = ~0U;
    impl_->white = CreateTexture(1, 1, &white, 4);
    impl_->slots[impl_->white->slot] = nullptr;
    impl_->white->slot = 0; impl_->slots[0] = impl_->white;
    impl_->scene = impl_->Allocate(w, h, D3DFMT_A8R8G8B8, false);
    impl_->BindTarget();
    Check(context.device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff000000, 1, 0), "Clear D3D9 scene");
}
D3d9Renderer::~D3d9Renderer() = default;
void D3d9Renderer::PrepareLoadingPipelines()
{
    auto& s = *impl_;
    if (s.initialized) return;
    const auto vs = [&](size_t index, Com<IDirect3DVertexShader9>& output) {
        auto code = Bytecode(s.shaderDirectory / S::Programs[index].bytecode, D3DVS_VERSION(3, 0));
        Check(s.device->CreateVertexShader(code.data(), output.put()), "Create precompiled D3D9 vertex shader");
    };
    const auto ps = [&](size_t index, Com<IDirect3DPixelShader9>& output) {
        auto code = Bytecode(s.shaderDirectory / S::Programs[index].bytecode, D3DPS_VERSION(3, 0));
        if (index == 7) s.fontInstructionsPerTap = FontInstructionBound(code);
        Check(s.device->CreatePixelShader(code.data(), output.put()), "Create precompiled D3D9 pixel shader");
    };
    vs(0, s.sceneVS); vs(1, s.imageVS);
    ps(2, s.scenePS); ps(3, s.copyPS); ps(4, s.colorizePS); ps(5, s.composePS);
    ps(6, s.causticsPS); ps(7, s.fontPS); ps(8, s.presentPS);
    vs(9, s.poolVS);
    static constexpr D3DVERTEXELEMENT9 elements[]{
        {0,0,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
        {0,8,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0},D3DDECL_END()};
    Check(s.device->CreateVertexDeclaration(elements,s.poolDeclaration.put()),"Create D3D9 pool declaration");
    Check(s.device->CreateVertexBuffer(sizeof(GpuPoolVertices),D3DUSAGE_WRITEONLY,0,
        D3DPOOL_DEFAULT,s.poolVertices.put(),nullptr),"Create static D3D9 pool vertices");
    Check(s.device->CreateIndexBuffer(sizeof(GpuPoolIndices),D3DUSAGE_WRITEONLY,D3DFMT_INDEX16,
        D3DPOOL_DEFAULT,s.poolIndices.put(),nullptr),"Create static D3D9 pool indices");
    void* data=nullptr;
    Check(s.poolVertices->Lock(0,sizeof(GpuPoolVertices),&data,0),"Lock static D3D9 pool vertices");
    std::memcpy(data,GpuPoolVertices.data(),sizeof(GpuPoolVertices));
    Check(s.poolVertices->Unlock(),"Unlock static D3D9 pool vertices");
    Check(s.poolIndices->Lock(0,sizeof(GpuPoolIndices),&data,0),"Lock static D3D9 pool indices");
    std::memcpy(data,GpuPoolIndices.data(),sizeof(GpuPoolIndices));
    Check(s.poolIndices->Unlock(),"Unlock static D3D9 pool indices");
    s.initialized = true;
}
D3d9Texture* D3d9Renderer::CreateTexture(int w, int h, const void* pixels, int pitch, bool opaque)
{
    auto* image = impl_->Allocate(w, h, D3DFMT_A8R8G8B8, true);
    try { impl_->Upload(image, pixels, pitch); }
    catch (...) { DestroyTexture(image); throw; }
    image->flags = opaque ? GpuVertex::kOpaqueInput : 0;
    return image;
}
D3d9Texture* D3d9Renderer::CreateRawTexture(int w, int h, const void* pixels, int pitch)
{
    auto* image = impl_->Allocate(w, h, D3DFMT_A8R8G8B8, false);
    try { impl_->Upload(image, pixels, pitch); }
    catch (...) { DestroyTexture(image); throw; }
    return image;
}
void D3d9Renderer::ReserveStaticTextureSlots(unsigned int count)
{
    auto& s = *impl_;
    if (count >= 4095) throw std::invalid_argument("Too many D3D9 static texture slots");
    const unsigned int base = 4096 - count - 1;
    if (s.staticBase != 4096 && s.staticBase != base) throw std::logic_error("D3D9 static table already reserved");
    for (unsigned int i = base; i < 4096; ++i)
        if (s.slots[i]) throw std::logic_error("D3D9 static/dynamic slot overlap");
    s.staticBase = base;
}
void D3d9Renderer::BindStaticTexture(D3d9Texture* image, unsigned int slot)
{
    auto& s = *impl_;
    if (!image || !s.textures.contains(image) || !slot ||
        static_cast<unsigned long long>(s.staticBase) + slot >= 4096)
        throw std::invalid_argument("Invalid D3D9 static texture symbol");
    if (image->staticSlot != ~0U && image->staticSlot != s.staticBase + slot)
        throw std::invalid_argument("D3D9 image already has a different static symbol");
    if (image->slot != ~0U && s.slots[image->slot] == image) s.slots[image->slot] = nullptr;
    image->slot = image->staticSlot = s.staticBase + slot;
    s.slots[image->slot] = image;
}
D3d9Texture* D3d9Renderer::CreateTextureStatic(int w, int h, const void* pixels,
    int pitch, bool opaque, unsigned int slot)
{
    auto* image = impl_->Allocate(w, h, D3DFMT_A8R8G8B8, false);
    image->flags = opaque ? GpuVertex::kOpaqueInput : 0;
    try { impl_->Upload(image, pixels, pitch); BindStaticTexture(image, slot); }
    catch (...) { DestroyTexture(image); throw; }
    return image;
}
D3d9Texture* D3d9Renderer::ReplaceTexture(D3d9Texture* old, int w, int h,
    const void* pixels, int pitch, bool opaque)
{
    if (!old) return CreateTexture(w, h, pixels, pitch, opaque);
    auto& s = *impl_;
    if (!s.textures.contains(old) || old == s.white || old->render || old->caustic)
        throw std::invalid_argument("Cannot host-replace this D3D9 image");
    // Always create a new version. Retire keeps queued draws' previous texture alive.
    auto* image = s.Allocate(w, h, D3DFMT_A8R8G8B8, false);
    image->flags = opaque ? GpuVertex::kOpaqueInput : 0;
    try {
        s.Upload(image, pixels, pitch);
        if (old->staticSlot != ~0U) BindStaticTexture(image, old->staticSlot - s.staticBase);
        else { image->slot = old->slot; if (image->slot != ~0U) s.slots[image->slot] = image; }
    } catch (...) { DestroyTexture(image); throw; }
    DestroyTexture(old);
    return image;
}
D3d9Texture* D3d9Renderer::CreateCoverageMask(int w, int h, std::span<const unsigned char> coverage, int pitch)
{
    impl_->Dimensions(w, h);
    if (pitch < w || coverage.size() < size_t(pitch) * (h - 1) + w)
        throw std::invalid_argument("Incomplete D3D9 coverage mask");
    std::vector<unsigned int> pixels(size_t(w) * h);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
        pixels[size_t(y) * w + x] = 0xff000000U | (unsigned(coverage[size_t(y) * pitch + x]) << 16);
    auto* image = impl_->Allocate(w, h, D3DFMT_A8R8G8B8, true);
    try { impl_->Upload(image, pixels.data(), w * 4); }
    catch (...) { DestroyTexture(image); throw; }
    return image;
}
D3d9Texture* D3d9Renderer::CreateRenderImage(int w, int h)
{
    auto& s = *impl_;
    s.Flush();
    auto* image = s.Allocate(w, h, D3DFMT_A32B32G32R32F, true);
    image->render = true; image->flags = 0x80000000U;
    try {
        s.Target(image->surface.value, w, h);
        Check(s.device->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1, 0), "Clear D3D9 float render image");
    } catch (...) { DestroyTexture(image); throw; }
    return image;
}
void D3d9Renderer::DestroyTexture(D3d9Texture* image)
{
    if (!image) return;
    auto& s = *impl_;
    if (!s.textures.contains(image) || image == s.white)
        throw std::invalid_argument("Cannot destroy white/foreign D3D9 image");
    if (image == s.selected) throw std::logic_error("Deselect D3D9 render image before destruction");
    s.Retire(image);
}
unsigned int D3d9Renderer::SlotOf(const D3d9Texture* image)
{
    if (!image || image->slot == ~0U) throw std::logic_error("D3D9 raw image has no drawable slot");
    return image->slot | image->flags;
}
void D3d9Renderer::SetRenderImage(D3d9Texture* image)
{
    auto& s = *impl_;
    if (image && (!s.textures.contains(image) || !image->render))
        throw std::invalid_argument("D3D9 target is not a live float render image");
    if (image == s.selected) return;
    s.Flush(); s.selected = image;
    if (s.open && !s.mainFrame && !image) { s.context.WaitIdle(); s.open = false; }
}
bool D3d9Renderer::BeginScene()
{
    auto& s = *impl_;
    if (s.open) throw std::logic_error("D3D9 scene already open");
    if (!s.selected && !s.context.BeginPresent()) return false;
    s.context.MakeCurrent();
    if (!s.selected) {
        auto& frame = s.frames[s.nextFrame];
        if (frame.pending) {
            const auto status = frame.fence->GetData(nullptr, 0, 0);
            if (status == S_FALSE) return false;
            Check(status, "Poll D3D9 frame completion");
            frame.pending = false;
        }
        if (s.activeFrame != s.nextFrame) {
            std::swap(s.stream, s.frames[s.activeFrame].stream);
            std::swap(s.streamCapacity, s.frames[s.activeFrame].streamCapacity);
            s.activeFrame = s.nextFrame;
            std::swap(s.stream, frame.stream);
            std::swap(s.streamCapacity, frame.streamCapacity);
        }
        if (!frame.fence)
            Check(s.device->CreateQuery(D3DQUERYTYPE_EVENT, frame.fence.put()), "Create D3D9 frame fence");
    }
    s.open = true; s.mainFrame = !s.selected;
    try {
        if (s.causticsQueued && s.causticTarget && s.causticSource) {
            s.ImagePass(s.causticTarget, s.causticSource, nullptr, 3);
            s.causticsQueued = false;
        }
        s.BindTarget();
    } catch (...) { s.open = false; throw; }
    return true;
}
void D3d9Renderer::SubmitTriangles(std::span<const GpuVertex> input, D3d9Texture* source,
    bool, bool, GpuBlend blend, const GpuClip* clip) { impl_->Submit(input, source, blend, clip, false); }
void D3d9Renderer::SubmitPool(D3d9Texture* texture,const GpuPoolParams& params)
{
    auto& s=*impl_;
    if (!s.open || !texture || !s.textures.contains(texture))
        throw std::logic_error("D3D9 pool requires an open scene and live source texture");
    if (params.layer>2 || params.sampler>3 ||
        static_cast<unsigned>(params.blend)>static_cast<unsigned>(GpuBlend::Opaque))
        throw std::invalid_argument("Invalid D3D9 pool layer/sampler/blend");
    s.Flush();
    auto* target=s.selected ? s.selected : s.scene;
    D3d9Texture feedback;
    if (texture==target) {
        s.Snapshot(feedback,texture);
    }
    s.BindTarget();
    auto* d=s.device;
    Check(d->SetVertexDeclaration(s.poolDeclaration.value),"Bind D3D9 pool declaration");
    Check(d->SetStreamSource(0,s.poolVertices.value,0,sizeof(GpuPoolVertex)),"Bind static D3D9 pool grid");
    Check(d->SetIndices(s.poolIndices.value),"Bind static D3D9 pool indices");
    Check(d->SetVertexShader(s.poolVS.value),"Bind D3D9 pool vertex shader");
    Check(d->SetPixelShader(s.scenePS.value),"Bind D3D9 pool scene pixel shader");
    s.Projection(target->width,target->height);
    const auto flags=texture->flags|params.samplingFlags;
    const unsigned packed=
        unsigned(bool(flags&0x80000000U))*S::Generated+
        unsigned(bool(flags&GpuVertex::kOpaqueInput))*S::Opaque+
        unsigned(bool(flags&GpuVertex::kAlphaEdgeFilter))*S::AlphaEdge+
        ((flags>>28)&3)*S::FilterKindUnit+
        unsigned(bool(flags&GpuVertex::kIndependentColorFilter))*S::Independent+
        (params.sampler&1)*S::Linear+((params.sampler>>1)&1)*S::Repeat;
    const S::Float4 constants[]{
        {params.width,params.height,params.transX,params.transY},
        {params.time1,params.time2,params.time3,params.time5},
        {float(params.clip.x),float(params.clip.y),float(double(params.clip.x)+params.clip.width),
            float(double(params.clip.y)+params.clip.height)},
        {float(params.layer),float(params.night),float(texture->width),float(texture->height)},
        {float(packed),0,0,0}};
    Check(d->SetVertexShaderConstantF(1,&constants[0].x,5),"Set D3D9 pool constants");
    std::array<S::Float4,4> pixel{};
    pixel[0]={1.0f/texture->width,1.0f/texture->height,0,0};
    pixel[S::Sampling-S::SourceTexture].w=float(target->render);
    Check(d->SetPixelShaderConstantF(S::SourceTexture,&pixel[0].x,UINT(pixel.size())),
        "Set D3D9 pool pixel constants");
    Check(d->SetTexture(0,feedback.image ? feedback.image.value : texture->image.value),"Bind D3D9 pool texture");
    Check(d->SetRenderState(D3DRS_SCISSORTESTENABLE,FALSE),"Use pool vertex clip alpha");
    Check(d->SetRenderState(D3DRS_ALPHABLENDENABLE,params.blend!=GpuBlend::Opaque),"Set D3D9 pool blending");
    Check(d->SetRenderState(D3DRS_SRCBLEND,target->render ? D3DBLEND_ONE : D3DBLEND_SRCALPHA),
        "Set D3D9 pool source blend");
    Check(d->SetRenderState(D3DRS_DESTBLEND,params.blend==GpuBlend::Additive ? D3DBLEND_ONE : D3DBLEND_INVSRCALPHA),
        "Set D3D9 pool destination blend");
    Check(d->BeginScene(),"Begin D3D9 pool draw");
    const HRESULT drawn=d->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,96,0,150);
    const HRESULT ended=d->EndScene();
    Check(drawn,"Draw D3D9 indexed pool"); Check(ended,"End D3D9 pool draw");
    // BindTarget/Flush and image passes restore their declaration/stream/shaders.
}
void D3d9Renderer::SubmitCoverageTriangles(std::span<const GpuVertex> input, D3d9Texture* source,
    GpuBlend blend, const GpuClip* clip) { impl_->Submit(input, source, blend, clip, true); }
void D3d9Renderer::Impl::Quad(int w, int h)
{
    const Vertex quad[]{
        {0, 0, 1, 1, 1, 1, 0, 0, {}, 0},
        {float(w), 0, 1, 1, 1, 1, 1, 0, {}, 0},
        {0, float(h), 1, 1, 1, 1, 0, 1, {}, 0},
        {float(w), float(h), 1, 1, 1, 1, 1, 1, {}, 0}};
    Check(device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE), "Disable D3D9 image blending");
    Check(device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE), "Disable D3D9 image scissor");
    Projection(w, h);
    Check(device->BeginScene(), "Begin D3D9 image pass");
    const HRESULT result = device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(Vertex));
    const HRESULT ended = device->EndScene();
    Check(result, "Draw D3D9 image quad"); Check(ended, "End D3D9 image pass");
}
bool D3d9Renderer::Impl::Present()
{
    if (!context.BeginPresent()) return false;
    Target(context.backBuffer, context.width, context.height);
    Check(device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff000000, 1, 0), "Clear D3D9 letterbox");
    const auto rect = FitSceneToSurface(width, height, context.width, context.height);
    D3DVIEWPORT9 viewport{DWORD(rect.x), DWORD(rect.y), DWORD(rect.width), DWORD(rect.height), 0, 1};
    Check(device->SetViewport(&viewport), "Set D3D9 presentation viewport");
    Check(device->SetVertexShader(imageVS.value), "Bind D3D9 presentation vertex shader");
    Check(device->SetPixelShader(presentPS.value), "Bind D3D9 presentation pixel shader");
    Constants c{}; Source(c, scene, 1); c[S::Presentation] = {0, 0, 0, 0}; SetConstants(c);
    Check(device->SetTexture(S::SourceImage, scene->image.value), "Bind retained D3D9 scene");
    Quad(rect.width, rect.height);
    // Present flushes this query with the commands it protects. A zero-flag
    // poll on the next reuse never forces a driver flush or waits on the CPU.
    auto& frame = frames[activeFrame];
    if (frame.fence) {
        Check(frame.fence->Issue(D3DISSUE_END), "Signal D3D9 frame completion");
        frame.pending = true;
    }
    context.Present();
    return true;
}
void D3d9Renderer::EndSceneAndPresent()
{
    auto& s = *impl_;
    if (!s.open) throw std::logic_error("No D3D9 scene to end");
    s.Flush();
    if (s.mainFrame) {
        s.Present(); s.sceneInitialized = true;
        s.nextFrame = (s.nextFrame + 1) % s.frames.size();
    }
    else s.context.WaitIdle();
    s.open = false;
}
bool D3d9Renderer::PresentRetainedScene()
{
    auto& s = *impl_;
    if (s.open || s.selected) throw std::logic_error("Retained D3D9 presentation requires a closed main scene");
    return s.sceneInitialized && s.Present();
}
bool D3d9Renderer::ResizePresent()
{
    if (impl_->open) throw std::logic_error("Resize D3D9 between scenes");
    return impl_->context.BeginPresent();
}
void D3d9Renderer::ResizeScene(int width, int height)
{
    auto& s = *impl_;
    if (s.open || s.selected)
        throw std::logic_error("Resize the main D3D9 canvas between scenes");
    if (s.width == width && s.height == height) return;
    auto* next = s.Allocate(width, height, D3DFMT_A8R8G8B8, false);
    auto* previous = s.scene;
    s.scene = next;
    s.width = width;
    s.height = height;
    s.sceneInitialized = false;
    DestroyTexture(previous);
    s.BindTarget();
    Check(s.context.device->Clear(0, nullptr, D3DCLEAR_TARGET, 0xff000000, 1, 0),
        "Clear resized D3D9 canvas");
}
void D3d9Renderer::SetHdrParams(const VulkanHdrParams& params) { params.Validate(); impl_->hdr = params; }
VulkanHdrParams D3d9Renderer::HdrParams() const { return impl_->hdr; }
void D3d9Renderer::PrepareScale(bool, std::span<const D3d9ScaleStep> steps, unsigned int, unsigned int)
{
    if (!steps.empty()) throw std::runtime_error("Direct3D9Ex cannot execute the configured HLSL compute scaling chain");
}
void D3d9Renderer::WarmScale(bool) {}
void D3d9Renderer::SetScaleMode(bool enabled, bool)
{
    if (enabled) throw std::runtime_error("Direct3D9Ex custom compute scaling is unsupported");
}
D3d9ScaleImage D3d9Renderer::ScaleOutput(bool) const { return {}; }
D3d9Renderer::ScaleCounts D3d9Renderer::GetScaleCounts() const { return {}; }
GpuClip D3d9Renderer::PresentRect(int w, int h) const { return FitSceneToSurface(impl_->width, impl_->height, w, h); }
unsigned int D3d9Renderer::MaxImageDimension2D() const { return impl_->maximum; }
D3d9Renderer::ResourceCounts D3d9Renderer::GetResourceCounts() const
{
    size_t buffers = size_t(bool(impl_->stream)) + size_t(bool(impl_->poolVertices));
    for (const auto& frame : impl_->frames) buffers += size_t(bool(frame.stream));
    return {impl_->textures.size(), impl_->retiredTextures.size(), 0,
        buffers, impl_->batches.size()};
}
}
