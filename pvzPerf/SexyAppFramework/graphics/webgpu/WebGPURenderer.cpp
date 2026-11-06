#include "WebGPURenderer.h"
#include "WebGPUShaders.h"
#include "graphics/FontWeights.h"
#include "graphics/PoolMesh.h"
#include "graphics/vulkan/VulkanViewport.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <emscripten.h>
#include <stdexcept>
#include <unordered_set>
namespace Sexy
{
using namespace wgpu;
namespace
{
constexpr unsigned int Capacity = 65536;
Buffer MakeBuffer(const Device &device, unsigned long long bytes, BufferUsage usage, const void *data = nullptr)
{
    BufferDescriptor desc;
    desc.size = (bytes + 3) & ~3ULL;
    desc.usage = usage | BufferUsage::CopyDst;
    auto result = device.CreateBuffer(&desc);
    if (data)
        device.GetQueue().WriteBuffer(result, 0, data, bytes);
    return result;
}
ShaderModule Shader(const Device &device, std::string_view code)
{
    ShaderSourceWGSL wgsl;
    wgsl.code = StringView(code.data(), code.size());
    ShaderModuleDescriptor desc;
    desc.nextInChain = &wgsl;
    return device.CreateShaderModule(&desc);
}
void Submit(const Queue &queue, const CommandEncoder &encoder)
{
    auto commands = encoder.Finish();
    queue.Submit(1, &commands);
}
RenderPassEncoder Pass(const CommandEncoder &encoder, const TextureView &target, bool clear = false)
{
    RenderPassColorAttachment color;
    color.view = target;
    color.loadOp = clear ? LoadOp::Clear : LoadOp::Load;
    color.storeOp = StoreOp::Store;
    color.clearValue = {0, 0, 0, 0};
    RenderPassDescriptor desc;
    desc.colorAttachmentCount = 1;
    desc.colorAttachments = &color;
    return encoder.BeginRenderPass(&desc);
}
RenderPipeline FullscreenPipeline(const Device &device, const ShaderModule &shader, TextureFormat format,
                                  const PipelineLayout &layout = {})
{
    ColorTargetState color;
    color.format = format;
    FragmentState fragment;
    fragment.module = shader;
    fragment.entryPoint = "fragment";
    fragment.targetCount = 1;
    fragment.targets = &color;
    RenderPipelineDescriptor desc;
    desc.layout = layout;
    desc.vertex.module = shader;
    desc.vertex.entryPoint = "vertex";
    desc.fragment = &fragment;
    return device.CreateRenderPipeline(&desc);
}
} // namespace
struct WebGPUTexture
{
    Texture image;
    TextureView view;
    Buffer targetParams;
    BindGroup targetGroup;
    std::array<BindGroup, 4> sourceGroups;
    int width = 0, height = 0;
    unsigned int slot = ~0U, staticSlot = ~0U, flags = 0;
    bool render = false, caustic = false;
    TextureFormat format = TextureFormat::RGBA8Unorm;
};
struct WebGPURenderer::Impl
{
    struct Batch
    {
        WebGPUTexture *texture;
        unsigned int first, count, sampler;
        GpuClip clip;
        GpuBlend blend;
        bool quads;
        unsigned int shader;
    };
    struct Frame
    {
        Buffer vertices;
        Impl *owner = nullptr;
        unsigned int cursor = 0;
        bool busy = false;
    };
    WebGPUContext &context;
    int width, height;
    unsigned int maximum = 8192, staticBase = 4096, frameIndex = 0;
    bool open = false, mainFrame = false, sceneInitialized = false, initialized = false, causticsQueued = false;
    WebGPUTexture *white = nullptr;
    WebGPUTexture *scene = nullptr;
    WebGPUTexture *selected = nullptr;
    WebGPUTexture *causticTarget = nullptr;
    WebGPUTexture *causticSource = nullptr;
    unsigned long long causticTimeNs = 0;
    VulkanHdrParams hdr{80, 1000, 150, 1};
    std::array<WebGPUTexture *, 4096> slots{};
    std::unordered_set<WebGPUTexture *> textures;
    std::vector<WebGPUTexture *> retired;
    std::vector<GpuVertex> vertices;
    std::vector<Batch> batches;
    std::array<Frame, 2> frames;
    Buffer indices, causticParams;
    BindGroup causticGroup;
    CommandEncoder pendingEncoder;
    BindGroupLayout targetLayout, sourceLayout;
    std::array<Sampler, 4> samplers;
    // Target format, blend, generic/sprite/mixed/coverage. No frame-time shader compilation.
    RenderPipeline pipelines[2][3][4];
    RenderPipeline imagePipeline, fontPipeline, presentPipeline;
    Impl(WebGPUContext &c, int w, int h) : context(c), width(w), height(h)
    {
        for (auto &frame : frames) frame.owner = this;
    }
    ~Impl()
    {
        DrainPending();
        context.WaitIdle();
        for (auto *t : textures)
            delete t;
        for (auto *t : retired)
            delete t;
    }
    void DrainPending()
    {
        if (!pendingEncoder) return;
        Submit(context.queue, pendingEncoder);
        pendingEncoder = nullptr;
    }
    void Register(WebGPUTexture *t)
    {
        for (unsigned int i = 1; i < staticBase; i++)
            if (!slots[i])
            {
                t->slot = i;
                slots[i] = t;
                return;
            }
        throw std::runtime_error("WebGPU texture slots exhausted");
    }
    WebGPUTexture *Allocate(int w, int h, TextureFormat format, bool drawable, bool render = false)
    {
        if (w <= 0 || h <= 0 || static_cast<unsigned>(w) > maximum || static_cast<unsigned>(h) > maximum)
            throw std::invalid_argument("Invalid WebGPU texture dimensions");
        auto *t = new WebGPUTexture;
        try
        {
            t->width = w;
            t->height = h;
            t->format = format;
            t->render = render;
            t->flags = render ? 0x80000000U : 0;
            TextureDescriptor desc;
            desc.size = {static_cast<unsigned>(w), static_cast<unsigned>(h), 1};
            desc.format = format;
            desc.usage = TextureUsage::CopySrc | TextureUsage::CopyDst | TextureUsage::TextureBinding |
                         TextureUsage::RenderAttachment;
            t->image = context.device.CreateTexture(&desc);
            t->view = t->image.CreateView();
            const std::array<float,4> params{float(w),float(h),0,0};
            t->targetParams = MakeBuffer(context.device, sizeof(params), BufferUsage::Uniform, &params);
            BindGroupEntry entry;
            entry.binding = 0;
            entry.buffer = t->targetParams;
            entry.size = sizeof(params);
            BindGroupDescriptor bg;
            bg.layout = targetLayout;
            bg.entryCount = 1;
            bg.entries = &entry;
            t->targetGroup = context.device.CreateBindGroup(&bg);
            textures.insert(t);
            if (drawable)
                Register(t);
        }
        catch (...)
        {
            textures.erase(t);
            if (t->slot != ~0U)
                slots[t->slot] = nullptr;
            delete t;
            throw;
        }
        return t;
    }
    void Upload(WebGPUTexture *t, const void *pixels, int pitch, bool coverage = false)
    {
        const int bytes = coverage ? 1 : 4;
        if (!pixels || pitch < t->width * bytes)
            throw std::invalid_argument("Invalid WebGPU upload pitch");
        std::vector<unsigned char> rgba(size_t(t->width) * t->height * bytes);
        for (int y = 0; y < t->height; y++)
        {
            const auto *source = static_cast<const unsigned char *>(pixels) + size_t(y) * pitch;
            auto *out = rgba.data() + size_t(y) * t->width * bytes;
            if (coverage)
                std::memcpy(out, source, t->width);
            else
                for (int x = 0; x < t->width; x++)
                {
                    out[x * 4] = source[x * 4 + 2];
                    out[x * 4 + 1] = source[x * 4 + 1];
                    out[x * 4 + 2] = source[x * 4];
                    out[x * 4 + 3] = source[x * 4 + 3];
                }
        }
        TexelCopyTextureInfo dest;
        dest.texture = t->image;
        TexelCopyBufferLayout layout;
        layout.bytesPerRow = t->width * bytes;
        layout.rowsPerImage = t->height;
        Extent3D extent{static_cast<unsigned>(t->width), static_cast<unsigned>(t->height), 1};
        context.queue.WriteTexture(&dest, rgba.data(), rgba.size(), &layout, &extent);
    }
    const BindGroup &SourceGroup(WebGPUTexture *t, unsigned int sampling)
    {
        auto &group = t->sourceGroups[sampling & 3U];
        if (!group)
        {
            BindGroupEntry entries[2];
            entries[0].binding = 0;
            entries[0].textureView = t->view;
            entries[1].binding = 1;
            entries[1].sampler = samplers[sampling & 3U];
            BindGroupDescriptor desc;
            desc.layout = sourceLayout;
            desc.entryCount = 2;
            desc.entries = entries;
            group = context.device.CreateBindGroup(&desc);
        }
        return group;
    }
    void Clear(WebGPUTexture *t)
    {
        Flush();
        DrainPending();
        auto encoder = context.device.CreateCommandEncoder();
        auto pass = Pass(encoder, t->view, true);
        pass.End();
        Submit(context.queue, encoder);
    }
    void Flush()
    {
        if (vertices.empty())
            return;
        context.Check();
        auto *target = selected ? selected : scene;
        auto &frame = frames[frameIndex];
        const bool deferred = open && mainFrame;
        if (!deferred) frame.cursor = 0;
        if (frame.cursor + vertices.size() > Capacity)
        {
            // Fixed-capacity stream: submit before reusing its storage, never allocate a page.
            DrainPending();
            frame.cursor = 0;
        }
        const auto vertexOffset = static_cast<unsigned long long>(frame.cursor) * sizeof(GpuVertex);
        const auto vertexBytes = vertices.size() * sizeof(GpuVertex);
        context.queue.WriteBuffer(frame.vertices, vertexOffset, vertices.data(), vertexBytes);
        if (deferred && !pendingEncoder) pendingEncoder = context.device.CreateCommandEncoder();
        auto encoder = deferred ? pendingEncoder : context.device.CreateCommandEncoder();
        auto pass = Pass(encoder, target->view);
        pass.SetBindGroup(0, target->targetGroup);
        pass.SetIndexBuffer(indices, IndexFormat::Uint16);
        pass.SetVertexBuffer(0, frame.vertices, vertexOffset, vertexBytes);
        const unsigned int format = target->format == TextureFormat::RGBA16Float ? 1 : 0;
        WGPURenderPipeline previousPipeline = nullptr;
        WGPUBindGroup previousGroup = nullptr;
        GpuClip previousClip{-1, -1, -1, -1};
        for (const auto &batch : batches)
        {
            const auto &pipeline = pipelines[format][static_cast<unsigned>(batch.blend)][batch.shader];
            if (pipeline.Get() != previousPipeline)
            {
                pass.SetPipeline(pipeline);
                previousPipeline = pipeline.Get();
            }
            const auto &group = SourceGroup(batch.texture, batch.sampler);
            if (group.Get() != previousGroup)
            {
                pass.SetBindGroup(1, group);
                previousGroup = group.Get();
            }
            if (std::memcmp(&previousClip, &batch.clip, sizeof(GpuClip)))
            {
                pass.SetScissorRect(batch.clip.x, batch.clip.y, batch.clip.width, batch.clip.height);
                previousClip = batch.clip;
            }
            if (batch.quads)
                pass.DrawIndexed(batch.count / 4 * 6, 1, 0, static_cast<int>(batch.first));
            else
                pass.Draw(batch.count, 1, batch.first);
        }
        pass.End();
        if (!deferred) Submit(context.queue, encoder);
        frame.cursor += static_cast<unsigned int>(vertices.size());
        vertices.clear();
        batches.clear();
        for (auto *t : retired)
            delete t;
        retired.clear();
    }
    WebGPUTexture *Snapshot(WebGPUTexture *source)
    {
        Flush();
        DrainPending();
        auto *copy = Allocate(source->width, source->height, source->format, false, source->render);
        copy->flags = source->flags;
        auto encoder = context.device.CreateCommandEncoder();
        TexelCopyTextureInfo from, to;
        from.texture = source->image;
        to.texture = copy->image;
        Extent3D extent{static_cast<unsigned>(source->width), static_cast<unsigned>(source->height), 1};
        encoder.CopyTextureToTexture(&from, &to, &extent);
        Submit(context.queue, encoder);
        return copy;
    }
    void Draw(std::span<const GpuVertex> input, GpuBlend blend, const GpuClip *requested, bool coverage, bool quads)
    {
        const unsigned int primitive = quads ? 4 : 3;
        if (!open || input.size() % primitive)
            throw std::logic_error("WebGPU draw needs an open scene and complete primitives");
        auto *target = selected ? selected : scene;
        GpuClip clip = requested ? *requested : GpuClip{0, 0, target->width, target->height};
        const long long right =
            std::min<long long>(target->width, static_cast<long long>(clip.x) + std::max(0, clip.width));
        const long long bottom =
            std::min<long long>(target->height, static_cast<long long>(clip.y) + std::max(0, clip.height));
        clip.x = std::max(0, clip.x);
        clip.y = std::max(0, clip.y);
        clip.width = static_cast<int>(std::max(0LL, right - clip.x));
        clip.height = static_cast<int>(std::max(0LL, bottom - clip.y));
        if (!clip.width || !clip.height)
            return;
        WebGPUTexture *feedback = nullptr;
        for (size_t first = 0; first < input.size();)
        {
            const auto sourceFlags = input[first].slot;
            auto flags = sourceFlags;
            const unsigned int sampler = coverage ? 0U : input[first].sampler;
            auto *texture = slots[flags & 4095U];
            if (!texture)
                throw std::logic_error("Unbound WebGPU texture slot");
            if (texture == target)
            {
                if (!feedback)
                    feedback = Snapshot(texture);
                texture = feedback;
            }
            size_t end = first + primitive;
            while (end < input.size() && input[end].slot == sourceFlags && input[end].sampler == input[first].sampler)
                end += primitive;
            const bool sprite = !coverage && !target->render && (flags & 0xf8000000U) == 0;
            const bool mixed = context.dualSource && sprite && blend != GpuBlend::Opaque;
            const unsigned int shader = coverage ? 3U : mixed ? 2U : static_cast<unsigned int>(sprite);
            const GpuBlend batchBlend = mixed ? GpuBlend::Normal : blend;
            if (mixed && blend == GpuBlend::Additive)
                flags |= 0x02000000U;
            while (first < end)
            {
                const unsigned int room = (Capacity - static_cast<unsigned>(vertices.size())) / primitive * primitive;
                if (!room)
                {
                    Flush();
                    continue;
                }
                const unsigned int count = static_cast<unsigned>(std::min<size_t>(end - first, room));
                const unsigned int base = static_cast<unsigned>(vertices.size());
                vertices.insert(vertices.end(), input.begin() + first, input.begin() + first + count);
                for (unsigned int i = base; i < base + count; i += primitive)
                {
                    vertices[i].slot = flags;
                    vertices[i].sampler = sampler;
                    if (quads)
                    {
                        // WebGPU's flat attributes use the first vertex of each indexed triangle.
                        vertices[i + 2].slot = flags;
                        vertices[i + 2].sampler = sampler;
                    }
                }
                if (!batches.empty())
                {
                    auto &last = batches.back();
                    if (last.texture == texture && last.blend == batchBlend && last.quads == quads &&
                        last.shader == shader && last.sampler == sampler &&
                        !std::memcmp(&last.clip, &clip, sizeof(clip)))
                    {
                        last.count += count;
                        first += count;
                        continue;
                    }
                }
                batches.push_back({texture, base, count, sampler, clip, batchBlend, quads, shader});
                first += count;
            }
        }
        if (feedback)
        {
            textures.erase(feedback);
            retired.push_back(feedback);
        }
    }
    struct ImageParams
    {
        int x = 0, y = 0;
        unsigned int operation = 0, color = 0;
        unsigned int tint[4]{255, 255, 255, 255};
        unsigned int flags = 0, colorOnly = 0, u0 = 0, u1 = 0, v1 = 0, pad[3]{};
    };
    void ImagePass(WebGPUTexture *target, WebGPUTexture *source, WebGPUTexture *alpha, const ImageParams &params)
    {
        Flush();
        if (params.operation == 3)
        {
            if (!causticParams) causticParams = MakeBuffer(context.device, sizeof(params), BufferUsage::Uniform);
            context.queue.WriteBuffer(causticParams, 0, &params, sizeof(params));
            if (!causticGroup)
            {
                BindGroupEntry entries[3];
                entries[0].binding = 0; entries[0].buffer = causticParams; entries[0].size = sizeof(params);
                entries[1].binding = 1; entries[1].textureView = source->view;
                entries[2].binding = 2; entries[2].textureView = source->view;
                BindGroupDescriptor desc; desc.layout = imagePipeline.GetBindGroupLayout(0);
                desc.entryCount = 3; desc.entries = entries;
                causticGroup = context.device.CreateBindGroup(&desc);
            }
            const bool deferred = open && mainFrame;
            if (deferred && !pendingEncoder) pendingEncoder = context.device.CreateCommandEncoder();
            auto encoder = deferred ? pendingEncoder : context.device.CreateCommandEncoder();
            auto pass = Pass(encoder, target->view);
            pass.SetPipeline(imagePipeline); pass.SetBindGroup(0, causticGroup); pass.Draw(3); pass.End();
            if (!deferred) Submit(context.queue, encoder);
            return;
        }
        DrainPending();
        auto buffer = MakeBuffer(context.device, sizeof(params), BufferUsage::Uniform, &params);
        BindGroupEntry entries[3];
        entries[0].binding = 0;
        entries[0].buffer = buffer;
        entries[0].size = sizeof(params);
        entries[1].binding = 1;
        entries[1].textureView = source->view;
        entries[2].binding = 2;
        entries[2].textureView = (alpha ? alpha : source)->view;
        BindGroupDescriptor desc;
        desc.layout = imagePipeline.GetBindGroupLayout(0);
        desc.entryCount = 3;
        desc.entries = entries;
        auto group = context.device.CreateBindGroup(&desc);
        auto encoder = context.device.CreateCommandEncoder();
        auto pass = Pass(encoder, target->view);
        pass.SetPipeline(imagePipeline);
        pass.SetBindGroup(0, group);
        pass.Draw(3);
        pass.End();
        Submit(context.queue, encoder);
    }
    bool Present()
    {
        if (!context.BeginPresent())
        {
            DrainPending();
            return false;
        }
        SurfaceTexture surface;
        context.surface.GetCurrentTexture(&surface);
        if (!surface.texture)
        {
            DrainPending();
            return false;
        }
        const auto rect = FitSceneToSurface(width, height, context.width, context.height);
        auto &frame = frames[frameIndex];
        if (!pendingEncoder) pendingEncoder = context.device.CreateCommandEncoder();
        auto pass = Pass(pendingEncoder, surface.texture.CreateView(), true);
        pass.SetPipeline(presentPipeline);
        pass.SetBindGroup(0, SourceGroup(scene, 1));
        pass.SetViewport(rect.x, rect.y, rect.width, rect.height, 0, 1);
        pass.Draw(3);
        pass.End();
        DrainPending();
        frame.busy = true;
        context.queue.OnSubmittedWorkDone(CallbackMode::AllowSpontaneous,
                                          [](QueueWorkDoneStatus status, StringView, Frame *completed) {
                                              completed->busy = false;
                                              if (status != QueueWorkDoneStatus::Success)
                                                  completed->owner->context.error = "WebGPU queue completion failed";
                                          }, &frame);
        frameIndex ^= 1U;
        return true;
    }
};

WebGPURenderer::WebGPURenderer(WebGPUContext &c, int w, int h, const std::filesystem::path &,
                               const VulkanHdrParams *hdr)
    : impl_(new Impl(c, w, h)), whiteTexture(impl_->white), causticsTexture(impl_->causticTarget),
      sceneOpen(impl_->open), sceneWidth(impl_->width), sceneHeight(impl_->height)
{
    try
    {
        if (hdr)
            impl_->hdr = *hdr;
        PrepareLoadingPipelines();
        impl_->scene = impl_->Allocate(w, h, TextureFormat::RGBA8Unorm, false);
        impl_->Clear(impl_->scene);
        const unsigned int white = ~0U;
        impl_->white = CreateTexture(1, 1, &white, 4, true);
    }
    catch (...)
    {
        delete impl_;
        throw;
    }
}
WebGPURenderer::~WebGPURenderer()
{
    delete impl_;
}
void WebGPURenderer::PrepareLoadingPipelines()
{
    auto &s = *impl_;
    if (s.initialized)
        return;
    auto &d = s.context.device;
    Limits limits;
    s.context.device.GetLimits(&limits);
    s.maximum = limits.maxTextureDimension2D;
    BindGroupLayoutEntry target;
    target.binding = 0;
    target.visibility = ShaderStage::Vertex | ShaderStage::Fragment;
    target.buffer.type = BufferBindingType::Uniform;
    target.buffer.minBindingSize = 16;
    BindGroupLayoutDescriptor td;
    td.entryCount = 1;
    td.entries = &target;
    s.targetLayout = d.CreateBindGroupLayout(&td);
    BindGroupLayoutEntry source[2];
    source[0].binding = 0;
    source[0].visibility = ShaderStage::Fragment;
    source[0].texture.sampleType = TextureSampleType::Float;
    source[0].texture.viewDimension = TextureViewDimension::e2D;
    source[1].binding = 1;
    source[1].visibility = ShaderStage::Fragment;
    source[1].sampler.type = SamplerBindingType::Filtering;
    BindGroupLayoutDescriptor sd;
    sd.entryCount = 2;
    sd.entries = source;
    s.sourceLayout = d.CreateBindGroupLayout(&sd);
    BindGroupLayout layouts[]{s.targetLayout, s.sourceLayout};
    PipelineLayoutDescriptor ld;
    ld.bindGroupLayoutCount = 2;
    ld.bindGroupLayouts = layouts;
    auto layout = d.CreatePipelineLayout(&ld);
    auto shader = Shader(d, WebGPUShaders::Scene());
    ShaderModule mixedShader;
    if (s.context.dualSource)
        mixedShader = Shader(d, std::string("enable dual_source_blending;\n") + std::string(WebGPUShaders::Scene()) +
                                    std::string(WebGPUShaders::Mixed()));
    VertexAttribute attrs[5];
    const VertexFormat formats[]{VertexFormat::Float32x2, VertexFormat::Float32x4, VertexFormat::Float32x2,
                                 VertexFormat::Uint32, VertexFormat::Uint32};
    const unsigned int offsets[]{0, 8, 24, 32, 36};
    for (unsigned int i = 0; i < 5; i++)
    {
        attrs[i].format = formats[i];
        attrs[i].offset = offsets[i];
        attrs[i].shaderLocation = i;
    }
    VertexBufferLayout vb;
    vb.arrayStride = sizeof(GpuVertex);
    vb.attributeCount = 5;
    vb.attributes = attrs;
    for (unsigned int f = 0; f < 2; f++)
        for (unsigned int blend = 0; blend < 3; blend++)
            for (unsigned int sprite = 0; sprite < 4U; sprite++)
            {
                if(sprite == 2U && !s.context.dualSource) continue;
                BlendState bs;
                bs.color.srcFactor = (f == 1 || sprite == 2) ? BlendFactor::One : BlendFactor::SrcAlpha;
                bs.color.dstFactor = sprite == 2  ? BlendFactor::OneMinusSrc1Alpha
                                     : blend == 1 ? BlendFactor::One
                                                  : BlendFactor::OneMinusSrcAlpha;
                bs.alpha.srcFactor = BlendFactor::One;
                bs.alpha.dstFactor = BlendFactor::OneMinusSrcAlpha;
                ColorTargetState color;
                color.format = f == 0 ? TextureFormat::RGBA8Unorm : TextureFormat::RGBA16Float;
                if (blend != static_cast<unsigned>(GpuBlend::Opaque))
                    color.blend = &bs;
                FragmentState fs;
                fs.module = sprite == 2 ? mixedShader : shader;
                fs.entryPoint = sprite == 3 ? "coverage" : sprite == 2 ? "mixedSprite" : sprite ? "sprite" : "scene";
                ConstantEntry premultiplied;
                premultiplied.key = "premultipliedOutput";
                premultiplied.value = f == 1;
                if (sprite == 0 || sprite == 3) { fs.constantCount = 1; fs.constants = &premultiplied; }
                fs.targetCount = 1;
                fs.targets = &color;
                RenderPipelineDescriptor pd;
                pd.layout = layout;
                pd.vertex.module = shader;
                pd.vertex.entryPoint = "vertex";
                pd.vertex.bufferCount = 1;
                pd.vertex.buffers = &vb;
                pd.fragment = &fs;
                s.pipelines[f][blend][sprite] = d.CreateRenderPipeline(&pd);
            }
    for (unsigned int i = 0; i < 4; i++)
    {
        SamplerDescriptor desc;
        desc.minFilter = desc.magFilter = (i & 1) ? FilterMode::Linear : FilterMode::Nearest;
        desc.addressModeU = desc.addressModeV = (i & 2) ? AddressMode::Repeat : AddressMode::ClampToEdge;
        s.samplers[i] = d.CreateSampler(&desc);
    }
    std::vector<unsigned short> indices(Capacity / 4 * 6);
    for (unsigned int q = 0; q < Capacity / 4; q++)
    {
        const unsigned int b = q * 4;
        const unsigned short v[]{static_cast<unsigned short>(b),     static_cast<unsigned short>(b + 1),
                                 static_cast<unsigned short>(b + 2), static_cast<unsigned short>(b + 2),
                                 static_cast<unsigned short>(b + 1), static_cast<unsigned short>(b + 3)};
        std::copy(v, v + 6, indices.begin() + q * 6);
    }
    s.indices = MakeBuffer(d, indices.size() * 2, BufferUsage::Index, indices.data());
    for (auto &frame : s.frames)
        frame.vertices = MakeBuffer(d, Capacity * sizeof(GpuVertex), BufferUsage::Vertex);
    s.vertices.reserve(Capacity);
    s.batches.reserve(1024);
    s.imagePipeline = FullscreenPipeline(d, Shader(d, WebGPUShaders::Image()), TextureFormat::RGBA8Unorm);
    s.fontPipeline = FullscreenPipeline(d, Shader(d, WebGPUShaders::Font()), TextureFormat::RGBA16Float);
    PipelineLayoutDescriptor presentLayout;
    presentLayout.bindGroupLayoutCount = 1;
    presentLayout.bindGroupLayouts = &s.sourceLayout;
    s.presentPipeline = FullscreenPipeline(d, Shader(d, WebGPUShaders::Present()), s.context.format,
                                          d.CreatePipelineLayout(&presentLayout));
    s.initialized = true;
}
WebGPUTexture *WebGPURenderer::CreateTexture(int w, int h, const void *pixels, int pitch, bool opaque)
{
    auto *t = impl_->Allocate(w, h, TextureFormat::RGBA8Unorm, true);
    impl_->Upload(t, pixels, pitch);
    t->flags = opaque ? GpuVertex::kOpaqueInput : 0;
    return t;
}
WebGPUTexture *WebGPURenderer::CreateRawTexture(int w, int h, const void *pixels, int pitch)
{
    auto *t = impl_->Allocate(w, h, TextureFormat::RGBA8Unorm, false);
    impl_->Upload(t, pixels, pitch);
    return t;
}
void WebGPURenderer::ReserveStaticTextureSlots(unsigned int count)
{
    if (count >= 4095)
        throw std::invalid_argument("Too many WebGPU static slots");
    impl_->staticBase = 4096 - count - 1;
}
void WebGPURenderer::BindStaticTexture(WebGPUTexture *t, unsigned int slot)
{
    auto &s = *impl_;
    if (!t || !slot || s.staticBase + slot >= 4096)
        throw std::invalid_argument("Invalid WebGPU static slot");
    if (t->slot != ~0U)
        s.slots[t->slot] = nullptr;
    t->slot = t->staticSlot = s.staticBase + slot;
    s.slots[t->slot] = t;
}
WebGPUTexture *WebGPURenderer::CreateTextureStatic(int w, int h, const void *p, int pitch, bool opaque,
                                                   unsigned int slot)
{
    auto *t = CreateRawTexture(w, h, p, pitch);
    t->flags = opaque ? GpuVertex::kOpaqueInput : 0;
    BindStaticTexture(t, slot);
    return t;
}
WebGPUTexture *WebGPURenderer::ReplaceTexture(WebGPUTexture *old, int w, int h, const void *p, int pitch, bool opaque)
{
    if (!old)
        return CreateTexture(w, h, p, pitch, opaque);
    auto *t = CreateRawTexture(w, h, p, pitch);
    t->flags = opaque ? GpuVertex::kOpaqueInput : 0;
    t->slot = old->slot;
    t->staticSlot = old->staticSlot;
    impl_->slots[t->slot] = t;
    DestroyTexture(old);
    return t;
}
WebGPUTexture *WebGPURenderer::CreateCoverageMask(int w, int h, std::span<const unsigned char> pixels, int pitch)
{
    if (pitch < w || pixels.size() < size_t(pitch) * (h - 1) + w)
        throw std::invalid_argument("Invalid WebGPU coverage data");
    auto *t = impl_->Allocate(w, h, TextureFormat::R8Unorm, true);
    impl_->Upload(t, pixels.data(), pitch, true);
    return t;
}
WebGPUTexture *WebGPURenderer::CreateRenderImage(int w, int h)
{
    auto *t = impl_->Allocate(w, h, TextureFormat::RGBA16Float, true, true);
    impl_->Clear(t);
    return t;
}
void WebGPURenderer::SetRenderImage(WebGPUTexture *t)
{
    auto &s = *impl_;
    if (t == s.selected)
        return;
    s.Flush();
    if (t && !t->render)
        throw std::invalid_argument("Not a WebGPU render image");
    s.selected = t;
    if (s.open && !s.mainFrame && !t)
        s.open = false;
}
void WebGPURenderer::DestroyTexture(WebGPUTexture *t)
{
    if (!t)
        return;
    auto &s = *impl_;
    if (t == s.white || t == s.selected || !s.textures.erase(t))
        throw std::invalid_argument("Invalid WebGPU texture destruction");
    if (t->slot != ~0U && s.slots[t->slot] == t)
        s.slots[t->slot] = nullptr;
    if (t == s.causticTarget)
    {
        s.causticTarget = nullptr;
        s.causticGroup = nullptr;
    }
    if (t == s.causticSource)
    {
        s.causticSource = nullptr;
        s.causticGroup = nullptr;
    }
    if (s.batches.empty())
        delete t;
    else
        s.retired.push_back(t);
}
unsigned int WebGPURenderer::SlotOf(const WebGPUTexture *t)
{
    if (!t || t->slot == ~0U)
        throw std::logic_error("WebGPU raw image not adopted");
    return t->slot | t->flags;
}
WebGPUTexture *WebGPURenderer::CopyTextureRegion(WebGPUTexture *source, int x, int y, int w, int h)
{
    auto &s = *impl_;
    s.Flush();
    s.DrainPending();
    auto *t = s.Allocate(w, h, source->format, true, source->render);
    t->flags = source->flags;
    s.Clear(t);
    const int left = std::max(0, x), top = std::max(0, y);
    const int right = std::min(source->width, x + w), bottom = std::min(source->height, y + h);
    if (right > left && bottom > top)
    {
        auto encoder = s.context.device.CreateCommandEncoder();
        TexelCopyTextureInfo from, to;
        from.texture = source->image;
        from.origin = {static_cast<unsigned>(left), static_cast<unsigned>(top), 0};
        to.texture = t->image;
        to.origin = {static_cast<unsigned>(left - x), static_cast<unsigned>(top - y), 0};
        Extent3D size{static_cast<unsigned>(right - left), static_cast<unsigned>(bottom - top), 1};
        encoder.CopyTextureToTexture(&from, &to, &size);
        Submit(s.context.queue, encoder);
    }
    return t;
}
WebGPUTexture *WebGPURenderer::ProcessImage(WebGPUTexture *source, const GpuImageProcessing &processing)
{
    auto &s = *impl_;
    auto *t = s.Allocate(processing.width, processing.height, TextureFormat::RGBA8Unorm, true);
    Impl::ImageParams p;
    p.x = processing.x;
    p.y = processing.y;
    p.operation = processing.operation == GpuImageOperation::Colorize ? 1 : 0;
    p.flags = source->flags;
    p.tint[0] = processing.red;
    p.tint[1] = processing.green;
    p.tint[2] = processing.blue;
    p.tint[3] = processing.alpha;
    s.ImagePass(t, source, nullptr, p);
    return t;
}
WebGPUTexture *WebGPURenderer::ComposeAlphaRaw(WebGPUTexture *base, WebGPUTexture *alpha, unsigned int color,
                                               bool colorOnly)
{
    auto &s = *impl_;
    if (base->width != alpha->width || base->height != alpha->height)
        throw std::invalid_argument("WebGPU alpha dimensions mismatch");
    auto *t = s.Allocate(base->width, base->height, TextureFormat::RGBA8Unorm, false);
    Impl::ImageParams p;
    p.operation = 2;
    p.color = color;
    p.colorOnly = colorOnly;
    s.ImagePass(t, base, alpha, p);
    return t;
}
WebGPUTexture *WebGPURenderer::AdoptComposedTexture(WebGPUTexture *t)
{
    auto &s = *impl_;
    s.Register(t);
    for (auto it = s.textures.begin(); it != s.textures.end();)
    {
        auto *other = *it;
        if (other != s.scene && other != s.selected && other->slot == ~0U)
        {
            delete other;
            it = s.textures.erase(it);
        }
        else
            ++it;
    }
    return t;
}
WebGPUTexture *WebGPURenderer::ComposeAlpha(WebGPUTexture *base, WebGPUTexture *alpha, unsigned int color,
                                            bool colorOnly)
{
    return AdoptComposedTexture(ComposeAlphaRaw(base, alpha, color, colorOnly));
}
void WebGPURenderer::RenderFontAtlas(WebGPUTexture *target, WebGPUTexture *source, std::span<const GpuFontGlyph> glyphs,
                                     unsigned int flags, bool forceWhite)
{
    auto &s = *impl_;
    s.Flush();
    s.DrainPending();
    auto *copy = target == source ? s.Snapshot(source) : nullptr;
    if (copy)
        source = copy;
    for (const auto &g : glyphs)
    {
        if (g.sourceWidth <= 0 || g.sourceHeight <= 0 || g.targetWidth <= 0 || g.targetHeight <= 0 || g.targetX < 0 ||
            g.targetY < 0 || g.targetX + g.targetWidth > target->width || g.targetY + g.targetHeight > target->height)
            throw std::invalid_argument("Invalid WebGPU font glyph");
        const auto x = BuildFontAxisWeights(g.sourceWidth, g.targetWidth, g.sourceX, source->width, 257);
        const auto y = BuildFontAxisWeights(g.sourceHeight, g.targetHeight, g.sourceY, source->height, 256);
        struct
        {
            int src[4], dst[4];
            unsigned int flags, sampling, white, pad;
            unsigned int offsets[4];
        } p{{g.sourceX, g.sourceY, g.sourceWidth, g.sourceHeight},
            {g.targetX, g.targetY, g.targetWidth, g.targetHeight},
            source->flags,
            flags,
            forceWhite,
            0,
            {}};
        std::vector<unsigned int> data = x.offsets;
        p.offsets[1] = data.size();
        data.insert(data.end(), y.offsets.begin(), y.offsets.end());
        p.offsets[2] = data.size();
        for (auto tap : x.taps)
        {
            data.push_back(tap.index);
            data.push_back(tap.weight);
        }
        p.offsets[3] = data.size();
        for (auto tap : y.taps)
        {
            data.push_back(tap.index);
            data.push_back(tap.weight);
        }
        auto uniform = MakeBuffer(s.context.device, sizeof(p), BufferUsage::Uniform, &p);
        auto weights = MakeBuffer(s.context.device, data.size() * 4, BufferUsage::Storage, data.data());
        BindGroupEntry entries[3];
        entries[0].binding = 0;
        entries[0].buffer = uniform;
        entries[0].size = sizeof(p);
        entries[1].binding = 1;
        entries[1].textureView = source->view;
        entries[2].binding = 2;
        entries[2].buffer = weights;
        entries[2].size = data.size() * 4;
        BindGroupDescriptor desc;
        desc.layout = s.fontPipeline.GetBindGroupLayout(0);
        desc.entryCount = 3;
        desc.entries = entries;
        auto group = s.context.device.CreateBindGroup(&desc);
        auto encoder = s.context.device.CreateCommandEncoder();
        auto pass = Pass(encoder, target->view);
        pass.SetPipeline(s.fontPipeline);
        pass.SetBindGroup(0, group);
        pass.SetViewport(g.targetX, g.targetY, g.targetWidth, g.targetHeight, 0, 1);
        pass.SetScissorRect(g.targetX, g.targetY, g.targetWidth, g.targetHeight);
        pass.Draw(3);
        pass.End();
        Submit(s.context.queue, encoder);
    }
    if (copy)
        DestroyTexture(copy);
}
WebGPUTexture *WebGPURenderer::CreateCausticsTexture(int w, int h, WebGPUTexture *source)
{
    if (w != 128 || h != 64)
        throw std::invalid_argument("Invalid WebGPU caustics size");
    auto *t = impl_->Allocate(w, h, TextureFormat::RGBA8Unorm, true);
    t->caustic = true;
    impl_->Clear(t);
    UseCausticsTexture(t, source);
    return t;
}
void WebGPURenderer::UseCausticsTexture(WebGPUTexture *output, WebGPUTexture *source)
{
    if (!output || !source || !output->caustic || output->width != 128 || output->height != 64 ||
        source->width != 256 || source->height != 256)
        throw std::invalid_argument("Invalid WebGPU caustics source/output");
    impl_->causticTarget = output;
    impl_->causticSource = source;
    impl_->causticGroup = nullptr;
    impl_->causticsQueued = true;
}
void WebGPURenderer::SetCausticsTimeNs(unsigned long long time)
{
    impl_->causticTimeNs = time;
    impl_->causticsQueued = impl_->causticTarget != nullptr;
}
bool WebGPURenderer::HasCausticsCompute() const
{
    return true;
}
bool WebGPURenderer::BeginScene()
{
    auto &s = *impl_;
    if (s.open)
        throw std::logic_error("WebGPU scene already open");
    s.context.Check();
    if (!s.selected && (!s.context.BeginPresent() || s.frames[s.frameIndex].busy))
        return false;
    s.open = true;
    s.mainFrame = !s.selected;
    if (s.mainFrame) s.frames[s.frameIndex].cursor = 0;
    if (s.causticsQueued && s.causticTarget && s.causticSource)
    {
        const auto offsets = EvaluatePoolCaustics(s.causticTimeNs);
        Impl::ImageParams p;
        p.operation = 3;
        p.u0 = offsets.u0;
        p.u1 = offsets.u1;
        p.v1 = offsets.v1;
        s.ImagePass(s.causticTarget, s.causticSource, nullptr, p);
        s.causticsQueued = false;
    }
    return true;
}
void WebGPURenderer::SubmitTriangles(std::span<const GpuVertex> v, WebGPUTexture *, bool, bool, GpuBlend b,
                                     const GpuClip *c)
{
    impl_->Draw(v, b, c, false, false);
}
void WebGPURenderer::SubmitQuads(std::span<const GpuVertex> v, WebGPUTexture *, bool, bool, GpuBlend b,
                                 const GpuClip *c)
{
    impl_->Draw(v, b, c, false, true);
}
void WebGPURenderer::SubmitCoverageTriangles(std::span<const GpuVertex> v, WebGPUTexture *, GpuBlend b,
                                             const GpuClip *c)
{
    impl_->Draw(v, b, c, true, false);
}
void WebGPURenderer::SubmitPool(WebGPUTexture *texture, const GpuPoolParams &p)
{
    std::array<PoolMeshVertex, 96> mesh;
    BuildPoolMesh(p, mesh);
    std::array<GpuVertex, 450> v;
    for (size_t i = 0; i < v.size(); i++)
    {
        const auto &m = mesh[GpuPoolIndices[i]];
        const int x = static_cast<int>(m.x - p.transX), y = static_cast<int>(m.y - p.transY);
        const bool visible = x >= p.clip.x && y >= p.clip.y && x < static_cast<long long>(p.clip.x) + p.clip.width &&
                             y < static_cast<long long>(p.clip.y) + p.clip.height;
        v[i] = {m.x, m.y, 1, 1, 1, visible ? m.alpha : 0.0f, m.u, m.v, SlotOf(texture) | p.samplingFlags, p.sampler};
    }
    impl_->Draw(v, p.blend, nullptr, false, false);
}
void WebGPURenderer::EndSceneAndPresent()
{
    auto &s = *impl_;
    if (!s.open)
        throw std::logic_error("WebGPU scene not open");
    s.Flush();
    if (s.mainFrame)
    {
        s.Present();
        s.sceneInitialized = true;
    }
    s.open = false;
}
bool WebGPURenderer::PresentRetainedScene()
{
    auto &s = *impl_;
    if (s.open || s.selected || !s.sceneInitialized || s.frames[s.frameIndex].busy)
        return false;
    return s.Present();
}
bool WebGPURenderer::ResizePresent()
{
    return impl_->context.BeginPresent();
}
void WebGPURenderer::ResizeScene(int w, int h)
{
    auto &s = *impl_;
    if (s.width == w && s.height == h)
        return;
    if (s.open || s.selected)
        throw std::logic_error("Resize WebGPU between scenes");
    auto *old = s.scene;
    s.scene = s.Allocate(w, h, TextureFormat::RGBA8Unorm, false);
    s.width = w;
    s.height = h;
    s.sceneInitialized = false;
    s.Clear(s.scene);
    DestroyTexture(old);
}
void WebGPURenderer::SetHdrParams(const VulkanHdrParams &p)
{
    impl_->hdr = p;
}
VulkanHdrParams WebGPURenderer::HdrParams() const
{
    return impl_->hdr;
}
void WebGPURenderer::PrepareScale(bool, std::span<const WebGPUScaleStep> steps, unsigned int, unsigned int)
{
    if (!steps.empty())
        throw std::runtime_error("WebGPU custom scale not supported");
}
void WebGPURenderer::WarmScale(bool)
{
}
void WebGPURenderer::SetScaleMode(bool enabled, bool)
{
    if (enabled)
        throw std::runtime_error("WebGPU custom scale not supported");
}
WebGPUScaleImage WebGPURenderer::ScaleOutput(bool) const
{
    return {};
}
WebGPURenderer::ScaleCounts WebGPURenderer::GetScaleCounts() const
{
    return {};
}
GpuClip WebGPURenderer::PresentRect(int w, int h) const
{
    return FitSceneToSurface(impl_->width, impl_->height, w, h);
}
unsigned int WebGPURenderer::MaxImageDimension2D() const
{
    return impl_->maximum;
}
WebGPURenderer::ResourceCounts WebGPURenderer::GetResourceCounts() const
{
    return {impl_->textures.size(), impl_->retired.size(), 0, 2, impl_->batches.size()};
}
std::vector<float> WebGPURenderer::ReadTextureFloats(WebGPUTexture *t)
{
    auto &s = *impl_;
    s.Flush();
    s.DrainPending();
    const unsigned int bytes = t->format == TextureFormat::RGBA16Float ? 8
                               : t->format == TextureFormat::R8Unorm   ? 1
                                                                       : 4;
    const unsigned int pitch = (t->width * bytes + 255) & ~255U;
    BufferDescriptor bd;
    bd.size = size_t(pitch) * t->height;
    bd.usage = BufferUsage::CopyDst | BufferUsage::MapRead;
    auto buffer = s.context.device.CreateBuffer(&bd);
    auto encoder = s.context.device.CreateCommandEncoder();
    TexelCopyTextureInfo source;
    source.texture = t->image;
    TexelCopyBufferInfo dest;
    dest.buffer = buffer;
    dest.layout.bytesPerRow = pitch;
    dest.layout.rowsPerImage = t->height;
    Extent3D size{static_cast<unsigned>(t->width), static_cast<unsigned>(t->height), 1};
    encoder.CopyTextureToBuffer(&source, &dest, &size);
    Submit(s.context.queue, encoder);
    bool ready = false, ok = false;
    buffer.MapAsync(MapMode::Read, 0, bd.size, CallbackMode::AllowSpontaneous, [&](MapAsyncStatus status, StringView) {
        ok = status == MapAsyncStatus::Success;
        ready = true;
    });
    while (!ready)
        emscripten_sleep(1);
    if (!ok)
        throw std::runtime_error("WebGPU readback failed");
    const auto *data = static_cast<const unsigned char *>(buffer.GetConstMappedRange());
    std::vector<float> result(size_t(t->width) * t->height * 4);
    for (int y = 0; y < t->height; y++)
        for (int x = 0; x < t->width; x++)
            for (unsigned int c = 0; c < 4; c++)
            {
                float v = 0;
                const auto *pixel = data + size_t(y) * pitch + x * bytes;
                if (bytes == 8)
                {
                    unsigned short half;
                    std::memcpy(&half, pixel + c * 2, 2);
                    const unsigned int exp = (half >> 10) & 31, frac = half & 1023;
                    v = exp == 0 ? std::ldexp(float(frac), -24) : std::ldexp(float(1024 + frac), int(exp) - 25);
                    if (half & 0x8000)
                        v = -v;
                }
                else
                    v = bytes == 1 ? (c == 0 ? pixel[0] / 255.0f : c == 3 ? 1.0f : 0.0f) : pixel[c] / 255.0f;
                result[(size_t(y) * t->width + x) * 4 + c] = v;
            }
    buffer.Unmap();
    return result;
}
ImagePixels WebGPURenderer::ReadTexturePixels(WebGPUTexture *t)
{
    const auto values = ReadTextureFloats(t);
    ImagePixels result(t->width, t->height);
    for (size_t i = 0; i < result.pixels.size(); i++)
    {
        const float a = (t->flags & GpuVertex::kOpaqueInput) ? 1 : values[i * 4 + 3];
        const float factor = t->render ? (a > 0 ? 1 / a : 0) : 1;
        const auto byte = [](float v) {
            return static_cast<unsigned int>(std::round(std::clamp(v, 0.0f, 1.0f) * 255));
        };
        result.pixels[i] = (byte(a) << 24) | (byte(values[i * 4] * factor) << 16) |
                           (byte(values[i * 4 + 1] * factor) << 8) | byte(values[i * 4 + 2] * factor);
    }
    return result;
}
std::vector<unsigned int> WebGPURenderer::ReadScenePixels()
{
    return ReadTexturePixels(impl_->scene).pixels;
}
std::vector<unsigned int> WebGPURenderer::ReadScalePixels(bool)
{
    throw std::logic_error("No WebGPU scale output");
}
} // namespace Sexy
