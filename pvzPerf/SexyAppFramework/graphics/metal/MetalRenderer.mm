#include "SexyAppFramework/graphics/PoolMesh.h"
#include "MetalRenderer.h"
#include "MetalContext.h"
#include "MetalShaderTypes.h"
#include "MetalScalePipeline.h"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace Sexy {
namespace {
constexpr uint32_t SlotCount = 4096, InvalidSlot = UINT32_MAX;
constexpr uint32_t Generated = 0x80000000u;
[[noreturn]] void MetalError(NSError* error, const char* operation) {
    throw std::runtime_error(std::string(operation) + ": " +
        (error ? error.localizedDescription.UTF8String : "Metal returned nil"));
}
}
struct MetalTexture {
    id<MTLTexture> resource;
    int width{}, height{};
    uint32_t slot = InvalidSlot, staticSlot = InvalidSlot;
    bool generated = false, opaque = false, sampled = false, renderable = false;
};
struct MetalRenderer::Impl {
    MetalContext& context;
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    id<MTLLibrary> library;
    id<MTLFunction> vertexFunction, fragmentFunction;
    id<MTLArgumentEncoder> argumentEncoder;
    id<MTLRenderPipelineState> pipelines[2][3];
    id<MTLRenderPipelineState> poolPipelines[2][3];
    id<MTLBuffer> poolVertices, poolIndices, quadIndices;
    id<MTLSamplerState> samplers[4];
    id<MTLComputePipelineState> colorKernel, alphaKernel, fontKernel, causticKernel;
    id<MTLCommandBuffer> commands;
    struct FrameResources {
        id<MTLCommandBuffer> submitted;
        NSMutableArray* resources;
        id<MTLBuffer> vertices;
        id<MTLBuffer> presentationVertices;
        size_t vertexCount = 0;
        std::vector<id<MTLBuffer>> arguments;
        size_t argumentCount = 0;
    };
    std::array<FrameResources, 2> frames{};
    unsigned int frameIndex = 0;
    // Explicit retention also covers resources referenced indirectly via argument buffers.
    NSMutableArray* recordingResources = [NSMutableArray new];
    std::unordered_map<MetalTexture*, std::unique_ptr<MetalTexture>> textures, raw;
    std::vector<std::unique_ptr<MetalTexture>> retired;
    std::array<MetalTexture*, SlotCount> owners{};
    uint32_t staticBase = SlotCount;
    std::unique_ptr<MetalTexture> scene;
    std::unique_ptr<MetalScaleLibrary> scaleLibrary;
    struct ScaleMode {
        std::unique_ptr<MetalScalePipeline> pipeline;
        bool warm = false;
    };
    std::array<ScaleMode, 2> scaleModes;
    bool scaleEnabled = false, scaleFullscreen = false;
    MetalTexture* white = nullptr;
    MetalTexture* renderImage = nullptr;
    MetalTexture* causticSource = nullptr;
    int width, height;
    bool sceneOpen = false, imageOnly = false, initialized = false;
    bool causticQueued = false;
    unsigned long long causticTimeNs = 0;
    MetalTexture* causticTarget = nullptr;

    Impl(MetalContext& c, int w, int h, const std::filesystem::path& directory)
        : context(c), device((__bridge id<MTLDevice>)c.device),
          queue((__bridge id<MTLCommandQueue>)c.queue), width(w), height(h) {
        Dimensions(w, h);
        if (device.argumentBuffersSupport != MTLArgumentBuffersTier2)
            throw std::runtime_error("Metal renderer requires Tier 2 argument buffers");
        const auto path = (directory / "game.metallib").string();
        NSString* name = [[NSString alloc] initWithBytes:path.data() length:path.size()
            encoding:NSUTF8StringEncoding];
        NSError* error = nil;
        library = [device newLibraryWithURL:[NSURL fileURLWithPath:name] error:&error];
        if (!library) MetalError(error, "Load game.metallib");
        vertexFunction = Function(@"pvz_game_vertex");
        fragmentFunction = Function(@"pvz_game_fragment");
        argumentEncoder = [fragmentFunction newArgumentEncoderWithBufferIndex:2];
        if (!argumentEncoder) MetalError(nil, "Create Metal image argument encoder");
        std::vector<unsigned short> quadPattern(65536 / 4 * 6);
        for (unsigned int quad = 0; quad < 65536 / 4; ++quad) {
            const unsigned int vertex = quad * 4;
            const std::array<unsigned int, 6> pattern{vertex, vertex+1, vertex+2, vertex+2, vertex+1, vertex+3};
            for (unsigned int i = 0; i < pattern.size(); ++i)
                quadPattern[quad * 6 + i] = static_cast<unsigned short>(pattern[i]);
        }
        quadIndices = [device newBufferWithBytes:quadPattern.data()
            length:quadPattern.size() * sizeof(unsigned short) options:MTLResourceStorageModeShared];
        if (!quadIndices) MetalError(nil, "Allocate immutable Metal quad indices");
        for (unsigned generated = 0; generated < 2; ++generated) {
            for (unsigned blend = 0; blend < 3; ++blend) {
                MTLRenderPipelineDescriptor* desc = [MTLRenderPipelineDescriptor new];
                desc.vertexFunction = vertexFunction;
                desc.fragmentFunction = fragmentFunction;
                auto* attachment = desc.colorAttachments[0];
                attachment.pixelFormat = generated ? MTLPixelFormatRGBA16Float : MTLPixelFormatBGRA8Unorm;
                attachment.blendingEnabled = blend != unsigned(GpuBlend::Opaque);
                attachment.rgbBlendOperation = MTLBlendOperationAdd;
                attachment.alphaBlendOperation = MTLBlendOperationAdd;
                attachment.sourceRGBBlendFactor = generated ? MTLBlendFactorOne : MTLBlendFactorSourceAlpha;
                attachment.destinationRGBBlendFactor = blend == unsigned(GpuBlend::Additive) ?
                    MTLBlendFactorOne : MTLBlendFactorOneMinusSourceAlpha;
                attachment.sourceAlphaBlendFactor = MTLBlendFactorOne;
                attachment.destinationAlphaBlendFactor = blend == unsigned(GpuBlend::Additive) ?
                    MTLBlendFactorOne : MTLBlendFactorOneMinusSourceAlpha;
                pipelines[generated][blend] = [device newRenderPipelineStateWithDescriptor:desc error:&error];
                if (!pipelines[generated][blend]) MetalError(error, "Create Metal draw pipeline");
                desc.vertexFunction = Function(@"pvz_pool_vertex");
                poolPipelines[generated][blend] =
                    [device newRenderPipelineStateWithDescriptor:desc error:&error];
                if (!poolPipelines[generated][blend]) MetalError(error, "Create Metal pool pipeline");
            }
        }
        for (unsigned i = 0; i < 4; ++i) {
            MTLSamplerDescriptor* desc = [MTLSamplerDescriptor new];
            desc.minFilter = desc.magFilter = (i & 1) ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
            desc.sAddressMode = desc.tAddressMode = (i & 2) ? MTLSamplerAddressModeRepeat : MTLSamplerAddressModeClampToEdge;
            samplers[i] = [device newSamplerStateWithDescriptor:desc];
            if (!samplers[i]) MetalError(nil, "Create Metal sampler");
        }
        static_assert(sizeof(GpuPoolVertex) == sizeof(MetalPoolVertex));
        poolVertices = [device newBufferWithBytes:GpuPoolVertices.data()
            length:sizeof(GpuPoolVertices) options:MTLResourceStorageModeShared];
        poolIndices = [device newBufferWithBytes:GpuPoolIndices.data()
            length:sizeof(GpuPoolIndices) options:MTLResourceStorageModeShared];
        if (!poolVertices || !poolIndices) MetalError(nil, "Allocate immutable Metal pool mesh");
        scene = Image(w, h, MTLPixelFormatBGRA8Unorm, false, true);
        scene->slot = 0;
        owners[0] = scene.get();
    }
    ~Impl() {
        // Submit already encoded work, including out-of-scene loading operations.
        if (commands) {
            [commands commit];
            [commands waitUntilCompleted];
        }
        for (auto& frame : frames)
            if (frame.submitted) [frame.submitted waitUntilCompleted];
    }
    id<MTLFunction> Function(NSString* name) {
        id<MTLFunction> result = [library newFunctionWithName:name];
        if (!result) throw std::runtime_error(std::string("Missing MSL function ") + name.UTF8String);
        return result;
    }
    id<MTLComputePipelineState> Kernel(NSString* name) {
        NSError* error = nil;
        id<MTLComputePipelineState> result =
            [device newComputePipelineStateWithFunction:Function(name) error:&error];
        if (!result) MetalError(error, "Create Metal compute pipeline");
        return result;
    }
    void Dimensions(int w, int h) const {
        // Apple GPU family native 2D texture limit.
        if (w <= 0 || h <= 0 || w > 16384 || h > 16384)
            throw std::invalid_argument("Invalid Metal image extent (maximum 16384)");
    }
    void Require(MetalTexture* texture) const {
        if (!texture || (!textures.contains(texture) && !raw.contains(texture) && texture != scene.get()))
            throw std::invalid_argument("Foreign or destroyed Metal texture");
    }
    std::unique_ptr<MetalTexture> Image(int w, int h, MTLPixelFormat format, bool generated, bool renderable) {
        Dimensions(w, h);
        auto result = std::make_unique<MetalTexture>();
        MTLTextureDescriptor* desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
            width:NSUInteger(w) height:NSUInteger(h) mipmapped:NO];
        desc.storageMode = MTLStorageModePrivate;
        desc.usage = MTLTextureUsageShaderRead;
        if (renderable) desc.usage |= MTLTextureUsageRenderTarget;
        if (format == MTLPixelFormatRGBA8Unorm || format == MTLPixelFormatRGBA16Float ||
            format == MTLPixelFormatRGBA32Float)
            desc.usage |= MTLTextureUsageShaderWrite;
        result->resource = [device newTextureWithDescriptor:desc];
        if (!result->resource) MetalError(nil, "Allocate Metal texture");
        result->width = w;
        result->height = h;
        result->generated = generated;
        result->renderable = renderable;
        return result;
    }
    void Register(MetalTexture* image, uint32_t requested = InvalidSlot) {
        uint32_t slot = requested;
        if (slot == InvalidSlot) {
            for (slot = 1; slot < staticBase && owners[slot]; ++slot) {}
            if (slot >= staticBase) throw std::runtime_error("Metal dynamic texture slots exhausted (4096 total)");
        }
        if (slot >= SlotCount) throw std::invalid_argument("Invalid Metal texture slot");
        image->slot = slot;
        owners[slot] = image;
    }
    MetalTexture* Own(std::unique_ptr<MetalTexture> image, bool drawable) {
        Begin();
        if (drawable) Register(image.get());
        auto* result = image.get();
        (drawable ? textures : raw).emplace(result, std::move(image));
        return result;
    }
    void ReleaseSlots(MetalTexture* image) {
        for (auto& owner : owners) if (owner == image) owner = nullptr;
    }
    void Completed() {
        for (auto& image : retired) ReleaseSlots(image.get());
        retired.clear();
        for (auto& entry : textures) entry.first->sampled = false;
        for (auto& entry : raw) entry.first->sampled = false;
        scene->sampled = false;
    }
    void Wait() {
        for (auto& frame : frames) {
            if (!frame.submitted) continue;
            [frame.submitted waitUntilCompleted];
            if (frame.submitted.status == MTLCommandBufferStatusError)
                MetalError(frame.submitted.error, "Metal command submission");
            frame.submitted = nil;
            frame.resources = nil;
        }
        Completed();
    }
    void Begin() {
        if (commands) return;
        auto& frame = frames[frameIndex];
        if (frame.submitted) {
            // Main-frame callers already polled this slot in BeginScene.
            // Synchronous image/readback operations may deliberately drain it.
            [frame.submitted waitUntilCompleted];
            if (frame.submitted.status == MTLCommandBufferStatusError)
                MetalError(frame.submitted.error, "Metal command submission");
            frame.submitted = nil; frame.resources = nil;
        }
        Completed();
        frame.vertexCount = 0; frame.argumentCount = 0;
        MTLCommandBufferDescriptor* desc = [MTLCommandBufferDescriptor new];
        desc.retainedReferences = YES;
        if (context.validation) desc.errorOptions = MTLCommandBufferErrorOptionEncoderExecutionStatus;
        commands = [queue commandBufferWithDescriptor:desc];
        if (!commands) MetalError(nil, "Allocate Metal command buffer");
        recordingResources = [NSMutableArray new];
    }
    void Keep(id object) {
        if (object) [recordingResources addObject:object];
    }
    void Finish(bool wait) {
        if (!commands) { if (wait) Wait(); return; }
        auto& frame = frames[frameIndex];
        frame.submitted = commands;
        commands = nil;
        frame.resources = recordingResources;
        recordingResources = nil;
        [frame.submitted commit];
        frameIndex = (frameIndex + 1) % frames.size();
        if (wait) Wait();
    }
    id<MTLBuffer> DrawArguments() {
        auto& frame = frames[frameIndex];
        if (frame.argumentCount == frame.arguments.size()) {
            id<MTLBuffer> buffer = [device newBufferWithLength:argumentEncoder.encodedLength
                options:MTLResourceStorageModeShared];
            if (!buffer) MetalError(nil, "Allocate reusable Metal argument buffer");
            frame.arguments.push_back(buffer);
        }
        return frame.arguments[frame.argumentCount++];
    }
    id<MTLBlitCommandEncoder> Blit() {
        Begin();
        id<MTLBlitCommandEncoder> encoder = [commands blitCommandEncoder];
        if (!encoder) MetalError(nil, "Create Metal blit encoder");
        return encoder;
    }
    void Clear(MetalTexture* target, MTLClearColor color = MTLClearColorMake(0, 0, 0, 0)) {
        Begin();
        MTLRenderPassDescriptor* desc = [MTLRenderPassDescriptor renderPassDescriptor];
        desc.colorAttachments[0].texture = target->resource;
        desc.colorAttachments[0].loadAction = MTLLoadActionClear;
        desc.colorAttachments[0].storeAction = MTLStoreActionStore;
        desc.colorAttachments[0].clearColor = color;
        id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:desc];
        if (!encoder) MetalError(nil, "Create Metal clear encoder");
        [encoder endEncoding];
        Keep(target->resource);
    }
    // Give the stable handle a new writable resource and slot. Old vertex slots
    // continue to address their immutable version until the command fence.
    void Snapshot(MetalTexture* image, bool force = false) {
        if (!force && !image->sampled) return;
        auto next = Image(image->width, image->height, image->resource.pixelFormat,
            image->generated, image->renderable);
        Register(next.get());
        next->opaque = image->opaque;
        id<MTLBlitCommandEncoder> encoder = Blit();
        [encoder copyFromTexture:image->resource sourceSlice:0 sourceLevel:0
            sourceOrigin:MTLOriginMake(0, 0, 0)
            sourceSize:MTLSizeMake(image->width, image->height, 1)
            toTexture:next->resource destinationSlice:0 destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];
        [encoder endEncoding];
        Keep(image->resource);
        auto old = std::make_unique<MetalTexture>(std::move(*image));
        *image = std::move(*next);
        image->staticSlot = old->staticSlot;
        old->staticSlot = InvalidSlot;
        owners[old->slot] = old.get();
        owners[image->slot] = image;
        if (image->staticSlot != InvalidSlot) owners[image->staticSlot] = image;
        retired.push_back(std::move(old));
    }
    void Upload(MetalTexture* texture, const void* source, int pitch, unsigned bytes) {
        if (!source || pitch < int64_t(texture->width) * bytes)
            throw std::invalid_argument("Invalid Metal texture upload");
        Begin();
        const size_t row = (size_t(texture->width) * bytes + 255) & ~size_t(255);
        const size_t size = row * size_t(texture->height);
        id<MTLBuffer> staging = [device newBufferWithLength:size options:MTLResourceStorageModeShared];
        if (!staging) MetalError(nil, "Allocate Metal image staging");
        std::memset(staging.contents, 0, size);
        for (int y = 0; y < texture->height; ++y)
            std::memcpy(static_cast<char*>(staging.contents) + size_t(y) * row,
                static_cast<const char*>(source) + size_t(y) * size_t(pitch), size_t(texture->width) * bytes);
        id<MTLBlitCommandEncoder> encoder = Blit();
        [encoder copyFromBuffer:staging sourceOffset:0 sourceBytesPerRow:row
            sourceBytesPerImage:size sourceSize:MTLSizeMake(texture->width, texture->height, 1)
            toTexture:texture->resource destinationSlice:0 destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];
        [encoder endEncoding];
        Keep(staging);
        Keep(texture->resource);
        // Submit one image at a time; callers may release their source immediately.
        Finish(true);
    }
    void Compute(id<MTLComputePipelineState> pipeline, MetalTexture* target,
        std::span<MetalTexture* const> inputs, const void* constants, size_t size,
        int w, int h, unsigned groupHeight = 8) {
        if (!pipeline) throw std::logic_error("Call PrepareLoadingPipelines before Metal image processing");
        Begin();
        id<MTLComputeCommandEncoder> encoder = [commands computeCommandEncoder];
        if (!encoder) MetalError(nil, "Create Metal compute encoder");
        [encoder setComputePipelineState:pipeline];
        [encoder setBytes:constants length:size atIndex:0];
        NSUInteger index = 0;
        for (auto* input : inputs) {
            [encoder setTexture:input->resource atIndex:index++];
            Keep(input->resource);
            input->sampled = true;
        }
        [encoder setTexture:target->resource atIndex:index];
        Keep(target->resource);
        [encoder dispatchThreadgroups:MTLSizeMake((w + 7) / 8, (h + groupHeight - 1) / groupHeight, 1)
            threadsPerThreadgroup:MTLSizeMake(8, groupHeight, 1)];
        [encoder endEncoding];
    }
    MetalTexture* Output() const { return renderImage ? renderImage : scene.get(); }
    GpuClip Rectangle(int w, int h) const {
        if (w <= 0 || h <= 0) return {};
        if (scaleEnabled) {
            const auto& image = scaleModes[scaleFullscreen ? 1 : 0].pipeline->output;
            return {(w - int(image.width)) / 2, (h - int(image.height)) / 2,
                int(image.width), int(image.height)};
        }
        int rw = w, rh = h;
        if (int64_t(w) * height <= int64_t(h) * width)
            rh = std::max(1, int(int64_t(w) * height / width));
        else
            rw = std::max(1, int(int64_t(h) * width / height));
        return {(w - rw) / 2, (h - rh) / 2, rw, rh};
    }
    void Draw(std::span<const GpuVertex> vertices, MetalTexture* texture,
        GpuBlend blend, const GpuClip* clip, bool coverage, bool quads = false) {
        if (!sceneOpen) throw std::logic_error("Metal draw requires an open scene");
        Require(texture);
        if (vertices.size() % (quads ? 4 : 3) || size_t(blend) >= 3)
            throw std::invalid_argument("Invalid Metal triangle list or blend");
        if (vertices.empty()) return;
        auto* target = Output();
        int64_t left = 0, top = 0, right = target->width, bottom = target->height;
        if (clip) {
            left = std::clamp<int64_t>(clip->x, 0, right);
            top = std::clamp<int64_t>(clip->y, 0, bottom);
            right = std::clamp<int64_t>(int64_t(clip->x) + clip->width, 0, right);
            bottom = std::clamp<int64_t>(int64_t(clip->y) + clip->height, 0, bottom);
        }
        if (right <= left || bottom <= top) return;
        Begin();
        std::array<MetalTexture*, SlotCount> used{};
        bool feedback = false;
        for (const auto& vertex : vertices) {
            const uint32_t slot = vertex.slot & 4095;
            if ((vertex.slot & ~0xfc000fffu) || vertex.sampler > 3 || !owners[slot])
                throw std::invalid_argument("Invalid Metal vertex texture slot or sampler");
            used[slot] = owners[slot];
            feedback |= used[slot] == target;
        }
        // Resolve the sampling table before replacing a feedback target.
        if (feedback || target->sampled) {
            // Snapshot retires a complete texture handle; redirect saved aliases.
            Snapshot(target, feedback);
            auto* old = retired.back().get();
            for (auto& input : used) if (input == target) input = old;
        }
        auto& frame = frames[frameIndex];
        if (vertices.size() > 65536 - frame.vertexCount)
            throw std::runtime_error("Metal frame exceeds the fixed 65536-vertex stream");
        if (!frame.vertices) {
            frame.vertices = [device newBufferWithLength:65536 * sizeof(GpuVertex)
                options:MTLResourceStorageModeShared];
            if (!frame.vertices) MetalError(nil, "Allocate reusable Metal vertex stream");
        }
        id<MTLBuffer> arguments = DrawArguments();
        id<MTLBuffer> stream = frame.vertices;
        const size_t vertexOffset = frame.vertexCount * sizeof(GpuVertex);
        auto* copied = static_cast<GpuVertex*>(stream.contents) + frame.vertexCount;
        std::memcpy(copied, vertices.data(), vertices.size_bytes());
        frame.vertexCount += vertices.size();
        constexpr uint32_t filters = GpuVertex::kIndependentColorFilter |
            GpuVertex::kColorFilterMask | GpuVertex::kAlphaEdgeFilter;
        for (size_t i = 0; i < vertices.size(); ++i) {
            const uint32_t slot = vertices[i].slot & 4095;
            const auto* source = used[slot];
            copied[i].slot = slot | (vertices[i].slot & filters) |
                (source->generated ? Generated : 0) |
                (source->opaque ? GpuVertex::kOpaqueInput : 0);
        }
        std::memset(arguments.contents, 0, arguments.length);
        [argumentEncoder setArgumentBuffer:arguments offset:0];
        for (uint32_t i = 0; i < SlotCount; ++i)
            if (used[i]) [argumentEncoder setTexture:used[i]->resource atIndex:i];
        MTLRenderPassDescriptor* desc = [MTLRenderPassDescriptor renderPassDescriptor];
        desc.colorAttachments[0].texture = target->resource;
        desc.colorAttachments[0].loadAction = MTLLoadActionLoad;
        desc.colorAttachments[0].storeAction = MTLStoreActionStore;
        id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:desc];
        if (!encoder) MetalError(nil, "Create Metal draw encoder");
        const MetalSceneConstants constants{float(target->width), float(target->height),
            target->generated ? 1u : 0u, coverage ? 1u : 0u};
        [encoder setRenderPipelineState:pipelines[target->generated ? 1 : 0][size_t(blend)]];
        [encoder setVertexBuffer:stream offset:vertexOffset atIndex:0];
        [encoder setVertexBytes:&constants length:sizeof(constants) atIndex:1];
        [encoder setFragmentBytes:&constants length:sizeof(constants) atIndex:1];
        [encoder setFragmentBuffer:arguments offset:0 atIndex:2];
        for (unsigned i = 0; i < 4; ++i) [encoder setFragmentSamplerState:samplers[i] atIndex:i];
        for (auto* input : used) if (input) {
            [encoder useResource:input->resource usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
            Keep(input->resource);
            input->sampled = true;
        }
        const MTLViewport viewport{0, 0, double(target->width), double(target->height), 0, 1};
        const MTLScissorRect scissor{NSUInteger(left), NSUInteger(top),
            NSUInteger(right - left), NSUInteger(bottom - top)};
        [encoder setViewport:viewport];
        [encoder setScissorRect:scissor];
        if (quads) {
            [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:vertices.size() / 4 * 6
                indexType:MTLIndexTypeUInt16 indexBuffer:quadIndices indexBufferOffset:0];
        } else [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertices.size()];
        [encoder endEncoding];
        Keep(arguments);
        Keep(stream);
        Keep(target->resource);
    }
    bool Present() {
        context.RecreateSwapchain();
        if (context.surfaceSuspended) return false;
        CAMetalLayer* layer = (__bridge CAMetalLayer*)context.layer;
        id<CAMetalDrawable> drawable = [layer nextDrawable];
        if (!drawable) return false;
        Begin();
        id<MTLTexture> source = scene->resource;
        if (scaleEnabled) {
            auto& mode = scaleModes[scaleFullscreen ? 1 : 0];
            if (!mode.warm) throw std::logic_error("Metal scale pipeline was not warmed during loading");
            mode.pipeline->Record((__bridge void*)commands);
            source = (__bridge id<MTLTexture>)mode.pipeline->output.texture;
        }
        const auto rectangle = Rectangle(int(drawable.texture.width), int(drawable.texture.height));
        // Use the game shader for SDR presentation so generated/straight-alpha
        // semantics remain identical to normal scene sampling.
        const float x = float(rectangle.x), y = float(rectangle.y);
        const float r = x + rectangle.width, b = y + rectangle.height;
        const std::array<GpuVertex, 6> vertices{{
            {x,y,1,1,1,1,0,0,0,1}, {r,y,1,1,1,1,1,0,0,1}, {x,b,1,1,1,1,0,1,0,1},
            {x,b,1,1,1,1,0,1,0,1}, {r,y,1,1,1,1,1,0,0,1}, {r,b,1,1,1,1,1,1,0,1}
        }};
        auto& frame = frames[frameIndex];
        id<MTLBuffer> args = DrawArguments();
        if (!frame.presentationVertices) {
            frame.presentationVertices = [device newBufferWithLength:sizeof(vertices)
                options:MTLResourceStorageModeShared];
            if (!frame.presentationVertices) MetalError(nil, "Allocate Metal presentation vertices");
        }
        id<MTLBuffer> stream = frame.presentationVertices;
        std::memcpy(stream.contents, vertices.data(), sizeof(vertices));
        std::memset(args.contents, 0, args.length);
        [argumentEncoder setArgumentBuffer:args offset:0];
        [argumentEncoder setTexture:source atIndex:0];
        MTLRenderPassDescriptor* desc = [MTLRenderPassDescriptor renderPassDescriptor];
        desc.colorAttachments[0].texture = drawable.texture;
        desc.colorAttachments[0].loadAction = MTLLoadActionClear;
        desc.colorAttachments[0].storeAction = MTLStoreActionStore;
        desc.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
        id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:desc];
        if (!encoder) MetalError(nil, "Create Metal presentation encoder");
        const MetalSceneConstants constants{float(drawable.texture.width), float(drawable.texture.height), 0, 0};
        [encoder setRenderPipelineState:pipelines[0][unsigned(GpuBlend::Opaque)]];
        [encoder setVertexBuffer:stream offset:0 atIndex:0];
        [encoder setVertexBytes:&constants length:sizeof(constants) atIndex:1];
        [encoder setFragmentBytes:&constants length:sizeof(constants) atIndex:1];
        [encoder setFragmentBuffer:args offset:0 atIndex:2];
        for (unsigned i = 0; i < 4; ++i) [encoder setFragmentSamplerState:samplers[i] atIndex:i];
        [encoder useResource:source usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
        const MTLViewport viewport{0, 0, double(drawable.texture.width), double(drawable.texture.height), 0, 1};
        [encoder setViewport:viewport];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
        [encoder endEncoding];
        Keep(args); Keep(stream); Keep(source); Keep(drawable);
        [commands presentDrawable:drawable];
        return true;
    }
    ImagePixels Read(MetalTexture* texture) {
        if (sceneOpen) throw std::logic_error("Metal readback requires a closed scene");
        Finish(true);
        const MTLPixelFormat format = texture->resource.pixelFormat;
        const size_t bytes = format == MTLPixelFormatRGBA32Float ? 16 :
            format == MTLPixelFormatRGBA16Float ? 8 : format == MTLPixelFormatR8Unorm ? 1 : 4;
        const size_t row = (size_t(texture->width) * bytes + 255) & ~size_t(255);
        id<MTLBuffer> buffer = [device newBufferWithLength:row * size_t(texture->height)
            options:MTLResourceStorageModeShared];
        if (!buffer) MetalError(nil, "Allocate Metal diagnostic readback");
        id<MTLBlitCommandEncoder> encoder = Blit();
        [encoder copyFromTexture:texture->resource sourceSlice:0 sourceLevel:0
            sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(texture->width, texture->height, 1)
            toBuffer:buffer destinationOffset:0 destinationBytesPerRow:row
            destinationBytesPerImage:row * size_t(texture->height)];
        [encoder endEncoding];
        Keep(buffer); Keep(texture->resource);
        Finish(true);
        ImagePixels result(texture->width, texture->height);
        const auto halfToFloat = [](uint16_t value) -> float {
            const uint32_t exponent = (value >> 10) & 31u;
            const uint32_t mantissa = value & 1023u;
            const float magnitude = exponent == 0 ? std::ldexp(float(mantissa), -24) :
                exponent == 31 ? (mantissa ? std::numeric_limits<float>::quiet_NaN() :
                    std::numeric_limits<float>::infinity()) :
                std::ldexp(float(1024u + mantissa), int(exponent) - 25);
            return (value & 0x8000u) ? -magnitude : magnitude;
        };
        const auto quantize = [](float f) -> uint32_t {
            if (!std::isfinite(f)) return 0;
            return uint32_t(std::lround(std::clamp(f, 0.0f, 1.0f) * 255.0f));
        };
        for (int y = 0; y < texture->height; ++y) {
            const auto* line = static_cast<const uint8_t*>(buffer.contents) + size_t(y) * row;
            for (int x = 0; x < texture->width; ++x) {
                uint32_t r, g, b, a;
                if (format == MTLPixelFormatRGBA16Float || format == MTLPixelFormatRGBA32Float) {
                    const auto* h = reinterpret_cast<const uint16_t*>(line) + size_t(x) * 4;
                    float f[4];
                    for (unsigned int component = 0; component < 4; ++component)
                        f[component] = format == MTLPixelFormatRGBA16Float ?
                            halfToFloat(h[component]) :
                            reinterpret_cast<const float*>(line)[size_t(x) * 4 + component];
                    const float alpha = texture->opaque ? 1.0f : f[3];
                    const float scale = texture->generated ? (alpha > 0 ? 1.0f / alpha : 0) : 1;
                    r = quantize(f[0] * scale); g = quantize(f[1] * scale);
                    b = quantize(f[2] * scale); a = quantize(alpha);
                } else if (format == MTLPixelFormatR8Unorm) {
                    r = g = b = line[x]; a = 255;
                } else {
                    const auto* p = line + size_t(x) * 4;
                    r = p[format == MTLPixelFormatBGRA8Unorm ? 2 : 0]; g = p[1];
                    b = p[format == MTLPixelFormatBGRA8Unorm ? 0 : 2]; a = texture->opaque ? 255 : p[3];
                    if (texture->generated) {
                        r = a ? std::min(255u, (r * 255u + a / 2) / a) : 0;
                        g = a ? std::min(255u, (g * 255u + a / 2) / a) : 0;
                        b = a ? std::min(255u, (b * 255u + a / 2) / a) : 0;
                    }
                }
                result.pixels[size_t(y) * texture->width + x] = (a << 24) | (r << 16) | (g << 8) | b;
            }
        }
        return result;
    }
};

MetalRenderer::MetalRenderer(MetalContext& context, int w, int h,
    const std::filesystem::path& shaders, const GpuHdrParams* hdr)
    : impl_(std::make_unique<Impl>(context, w, h, shaders)), whiteTexture(impl_->white),
      causticsTexture(impl_->causticTarget), sceneOpen(impl_->sceneOpen),
      sceneWidth(impl_->width), sceneHeight(impl_->height) {
    if (hdr || context.hdr) throw std::runtime_error("Metal HDR is not supported in this milestone");
    const uint32_t white = 0xffffffffu;
    impl_->white = CreateTexture(1, 1, &white, 4, true);
}
MetalRenderer::~MetalRenderer() = default;
void MetalRenderer::PrepareLoadingPipelines() {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("Prepare Metal pipelines between scenes");
    if (s.colorKernel && s.alphaKernel && s.fontKernel && s.causticKernel) return;
    s.colorKernel = s.Kernel(@"pvz_color_kernel");
    s.alphaKernel = s.Kernel(@"pvz_alpha_kernel");
    s.fontKernel = s.Kernel(@"pvz_font_kernel");
    s.causticKernel = s.Kernel(@"pvz_caustic_kernel");
}
MetalTexture* MetalRenderer::CreateTexture(int w, int h, const void* bytes, int pitch, bool opaque) {
    auto& s = *impl_;
    s.Dimensions(w, h);
    if (!bytes || pitch < int64_t(w) * 4) throw std::invalid_argument("Invalid Metal BGRA upload");
    auto* texture = s.Own(s.Image(w, h, MTLPixelFormatBGRA8Unorm, false, true), true);
    texture->opaque = opaque;
    s.Upload(texture, bytes, pitch, 4);
    return texture;
}
MetalTexture* MetalRenderer::CreateRawTexture(int w, int h, const void* bytes, int pitch) {
    auto& s = *impl_;
    s.Dimensions(w, h);
    if (!bytes || pitch < int64_t(w) * 4) throw std::invalid_argument("Invalid Metal raw upload");
    auto* texture = s.Own(s.Image(w, h, MTLPixelFormatBGRA8Unorm, false, true), false);
    s.Upload(texture, bytes, pitch, 4);
    return texture;
}
MetalTexture* MetalRenderer::ReplaceTexture(MetalTexture* previous, int w, int h,
    const void* bytes, int pitch, bool opaque) {
    auto& s = *impl_;
    if (!previous) return CreateTexture(w, h, bytes, pitch, opaque);
    s.Require(previous);
    s.Dimensions(w, h);
    if (!bytes || pitch < int64_t(w) * 4 || previous == s.white || previous->generated)
        throw std::invalid_argument("Invalid Metal texture replacement");
    if (previous->width == w && previous->height == h &&
        previous->resource.pixelFormat == MTLPixelFormatBGRA8Unorm && !previous->sampled) {
        previous->opaque = opaque;
        s.Upload(previous, bytes, pitch, 4);
        return previous;
    }
    auto* next = previous->staticSlot != InvalidSlot ?
        CreateTextureStatic(w, h, bytes, pitch, opaque, previous->staticSlot - s.staticBase) :
        CreateTexture(w, h, bytes, pitch, opaque);
    DestroyTexture(previous);
    return next;
}
void MetalRenderer::ReserveStaticTextureSlots(uint32_t count) {
    auto& s = *impl_;
    if (count >= SlotCount - 1) throw std::invalid_argument("Metal static texture region too large");
    const uint32_t base = SlotCount - count - 1;
    if (s.staticBase != SlotCount && s.staticBase != base)
        throw std::logic_error("Metal static texture region already reserved");
    for (uint32_t i = base; i < SlotCount; ++i)
        if (s.owners[i]) throw std::logic_error("Metal dynamic slots overlap static region");
    s.staticBase = base;
}
void MetalRenderer::BindStaticTexture(MetalTexture* texture, uint32_t symbol) {
    auto& s = *impl_;
    s.Require(texture);
    if (!s.textures.contains(texture) || !symbol || uint64_t(s.staticBase) + symbol >= SlotCount)
        throw std::invalid_argument("Invalid Metal static symbol");
    const auto slot = s.staticBase + symbol;
    if (texture->staticSlot != InvalidSlot && texture->staticSlot != slot)
        throw std::invalid_argument("Metal texture already has a different static symbol");
    if (texture->sampled) throw std::logic_error("Cannot migrate a recorded Metal texture");
    if (s.owners[slot] && s.owners[slot] != texture)
        throw std::logic_error("Metal static symbol already occupied");
    s.ReleaseSlots(texture);
    texture->slot = texture->staticSlot = slot;
    s.owners[slot] = texture;
}
MetalTexture* MetalRenderer::CreateTextureStatic(int w, int h, const void* bytes,
    int pitch, bool opaque, uint32_t symbol) {
    auto& s = *impl_;
    s.Dimensions(w, h);
    if (!bytes || pitch < int64_t(w) * 4 || !symbol || uint64_t(s.staticBase) + symbol >= SlotCount)
        throw std::invalid_argument("Invalid Metal static texture upload");
    // Finish existing draws before replacing the identity at a static symbol.
    s.Finish(true);
    auto allocation = s.Image(w, h, MTLPixelFormatBGRA8Unorm, false, true);
    auto* texture = allocation.get();
    texture->opaque = opaque;
    texture->staticSlot = s.staticBase + symbol;
    s.Register(texture, texture->staticSlot);
    s.textures.emplace(texture, std::move(allocation));
    s.Upload(texture, bytes, pitch, 4);
    return texture;
}
MetalTexture* MetalRenderer::CreateCoverageMask(int w, int h, std::span<const uint8_t> data, int pitch) {
    auto& s = *impl_;
    s.Dimensions(w, h);
    if (pitch < w || data.size() < uint64_t(h - 1) * uint64_t(pitch) + uint64_t(w))
        throw std::invalid_argument("Invalid Metal coverage upload");
    auto* texture = s.Own(s.Image(w, h, MTLPixelFormatR8Unorm, false, false), true);
    s.Upload(texture, data.data(), pitch, 1);
    return texture;
}
MetalTexture* MetalRenderer::CreateRenderImage(int w, int h) {
    auto& s = *impl_;
    auto* texture = s.Own(s.Image(w, h, MTLPixelFormatRGBA16Float, true, true), true);
    s.Clear(texture);
    return texture;
}
void MetalRenderer::SetRenderImage(MetalTexture* image) {
    auto& s = *impl_;
    if (image && (!s.textures.contains(image) || !image->renderable ||
        image->resource.pixelFormat != MTLPixelFormatRGBA16Float))
        throw std::invalid_argument("Metal render target must be a live RGBA16F render image");
    if (image == s.renderImage) return;
    if (s.sceneOpen && s.imageOnly && !image) {
        s.Finish(true);
        s.sceneOpen = false;
    }
    s.renderImage = image;
}
MetalTexture* MetalRenderer::CopyTextureRegion(MetalTexture* source, int x, int y, int w, int h) {
    auto& s = *impl_;
    s.Require(source);
    s.Dimensions(w, h);
    if (x < 0 || y < 0 || x >= source->width || y >= source->height ||
        source->resource.pixelFormat == MTLPixelFormatR8Unorm)
        throw std::invalid_argument("Invalid Metal image copy region");
    auto* target = s.Own(s.Image(w, h, source->resource.pixelFormat, source->generated, true), true);
    target->opaque = source->opaque;
    s.Clear(target);
    auto encoder = s.Blit();
    [encoder copyFromTexture:source->resource sourceSlice:0 sourceLevel:0
        sourceOrigin:MTLOriginMake(x, y, 0)
        sourceSize:MTLSizeMake(std::min(w, source->width - x), std::min(h, source->height - y), 1)
        toTexture:target->resource destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
    [encoder endEncoding];
    s.Keep(source->resource); s.Keep(target->resource);
    source->sampled = true;
    return target;
}
MetalTexture* MetalRenderer::ProcessImage(MetalTexture* source, const GpuImageProcessing& p) {
    auto& s = *impl_;
    s.Require(source);
    s.Dimensions(p.width, p.height);
    if (p.operation != GpuImageOperation::Copy && p.operation != GpuImageOperation::Colorize)
        throw std::invalid_argument("Unknown Metal image operation");
    auto* target = s.Own(s.Image(p.width, p.height, MTLPixelFormatRGBA8Unorm, false, false), true);
    const MetalColorConstants constants{{p.x, p.y, p.width, p.height}, {p.red, p.green, p.blue, p.alpha},
        uint32_t(p.operation), source->width, source->height, SlotOf(source)};
    const std::array inputs{source};
    s.Compute(s.colorKernel, target, inputs, &constants, sizeof(constants), p.width, p.height);
    return target;
}
MetalTexture* MetalRenderer::ComposeAlphaRaw(MetalTexture* base, MetalTexture* alpha,
    uint32_t color, bool colorOnly) {
    auto& s = *impl_;
    s.Require(base); s.Require(alpha);
    if (base->width != alpha->width || base->height != alpha->height ||
        base->generated || alpha->generated)
        throw std::invalid_argument("Metal alpha composition requires raw images and matching dimensions");
    auto* target = s.Own(s.Image(base->width, base->height, MTLPixelFormatRGBA8Unorm, false, false), false);
    const MetalAlphaConstants constants{base->width, base->height, color, colorOnly ? 1u : 0u};
    const std::array inputs{base, alpha};
    s.Compute(s.alphaKernel, target, inputs, &constants, sizeof(constants), base->width, base->height);
    return target;
}
MetalTexture* MetalRenderer::AdoptComposedTexture(MetalTexture* texture) {
    auto& s = *impl_;
    auto found = s.raw.find(texture);
    if (found == s.raw.end() || texture->resource.pixelFormat != MTLPixelFormatRGBA8Unorm)
        throw std::invalid_argument("Metal adoption requires a raw alpha composition");
    s.Register(texture);
    auto node = s.raw.extract(found);
    s.textures.insert(std::move(node));
    return texture;
}
MetalTexture* MetalRenderer::ComposeAlpha(MetalTexture* base, MetalTexture* alpha,
    uint32_t color, bool colorOnly) {
    return AdoptComposedTexture(ComposeAlphaRaw(base, alpha, color, colorOnly));
}
void MetalRenderer::RenderFontAtlas(MetalTexture* target, MetalTexture* source,
    std::span<const GpuFontGlyph> glyphs, uint32_t flags, bool white) {
    auto& s = *impl_;
    s.Require(source);
    if (!s.sceneOpen || s.renderImage != target || !target || !target->generated || target == source)
        throw std::invalid_argument("Metal font atlas requires an open render-image scene");
    for (const auto& g : glyphs)
        if (g.sourceX < 0 || g.sourceY < 0 || g.sourceWidth <= 0 || g.sourceHeight <= 0 ||
            int64_t(g.sourceX) + g.sourceWidth > source->width ||
            int64_t(g.sourceY) + g.sourceHeight > source->height ||
            g.targetX < 0 || g.targetY < 0 || g.targetWidth <= 0 || g.targetHeight <= 0 ||
            int64_t(g.targetX) + g.targetWidth > target->width ||
            int64_t(g.targetY) + g.targetHeight > target->height)
            throw std::invalid_argument("Invalid Metal font atlas glyph rectangle");
    if (glyphs.empty()) return;
    s.Begin();
    s.Snapshot(target);
    for (const auto& g : glyphs) {
        const MetalFontConstants constants{{g.sourceX, g.sourceY, g.sourceWidth, g.sourceHeight},
            {g.targetX, g.targetY, g.targetWidth, g.targetHeight}, white ? 1u : 0u,
            source->width, source->height, flags, SlotOf(source)};
        const std::array inputs{source};
        s.Compute(s.fontKernel, target, inputs, &constants, sizeof(constants), g.targetWidth, g.targetHeight);
    }
}
void MetalRenderer::DestroyTexture(MetalTexture* texture) {
    if (!texture) return;
    auto& s = *impl_;
    s.Require(texture);
    if (texture == s.white || texture == s.scene.get())
        throw std::invalid_argument("Cannot destroy an internal Metal texture");
    if (texture == s.renderImage) {
        if (s.sceneOpen) throw std::logic_error("Cannot destroy active Metal render image");
        s.renderImage = nullptr;
    }
    if (texture == s.causticTarget || texture == s.causticSource) {
        s.causticTarget = s.causticSource = nullptr;
        s.causticQueued = false;
    }
    auto& container = s.textures.contains(texture) ? s.textures : s.raw;
    auto found = container.find(texture);
    if (s.commands || s.frames[0].submitted || s.frames[1].submitted)
        s.retired.push_back(std::move(found->second));
    else s.ReleaseSlots(texture);
    container.erase(found);
}
uint32_t MetalRenderer::SlotOf(const MetalTexture* texture) {
    return !texture || texture->slot == InvalidSlot ? 0 :
        texture->slot | (texture->generated ? Generated : 0) | (texture->opaque ? GpuVertex::kOpaqueInput : 0);
}
MetalTexture* MetalRenderer::CreateCausticsTexture(int w, int h, MetalTexture* source) {
    auto& s = *impl_;
    s.Require(source);
    if (s.sceneOpen || w != 128 || h != 64 || source->width != 256 || source->height != 256)
        throw std::invalid_argument("Metal caustics require 256x256 input and 128x64 output");
    auto* target = s.Own(s.Image(w, h, MTLPixelFormatRGBA8Unorm, false, true), true);
    s.Clear(target, MTLClearColorMake(1, 1, 1, 0));
    UseCausticsTexture(target, source);
    return target;
}
void MetalRenderer::UseCausticsTexture(MetalTexture* target, MetalTexture* source) {
    auto& s = *impl_;
    s.Require(target); s.Require(source);
    if (s.sceneOpen || target == source || target->width != 128 || target->height != 64 ||
        source->width != 256 || source->height != 256 ||
        target->resource.pixelFormat != MTLPixelFormatRGBA8Unorm)
        throw std::invalid_argument("Invalid Metal GPU caustics selection");
    s.causticTarget = target;
    s.causticSource = source;
}
void MetalRenderer::SetCausticsTimeNs(unsigned long long timeNs) {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("Set Metal caustics nanosecond time between scenes");
    s.causticTimeNs = timeNs;
    s.causticQueued = s.causticTarget != nullptr;
}
bool MetalRenderer::HasCausticsCompute() const { return impl_->causticKernel != nil; }
bool MetalRenderer::BeginScene() {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("Metal scene already open");
    s.context.RecreateSwapchain();
    if (!s.renderImage && s.context.surfaceSuspended) return false;
    if (!s.renderImage && !s.commands) {
        const auto& frame = s.frames[s.frameIndex];
        if (frame.submitted) {
            const auto status = frame.submitted.status;
            if (status == MTLCommandBufferStatusError)
                MetalError(frame.submitted.error, "Metal command submission");
            if (status != MTLCommandBufferStatusCompleted) return false;
        }
    }
    s.Begin();
    if (s.causticQueued && s.causticTarget) {
        s.Snapshot(s.causticTarget);
        const auto offsets=EvaluatePoolCaustics(s.causticTimeNs);
        const MetalCausticConstants constants{offsets.u0,offsets.u1,offsets.v1};
        const std::array inputs{s.causticSource};
        s.Compute(s.causticKernel, s.causticTarget, inputs, &constants, sizeof(constants), 128, 64, 4);
        s.causticQueued = false;
    }
    if (!s.renderImage && !s.initialized) {
        s.Clear(s.scene.get(), MTLClearColorMake(0, 0, 0, 1));
        s.initialized = true;
    }
    s.imageOnly = s.renderImage != nullptr;
    s.sceneOpen = true;
    return true;
}
void MetalRenderer::SubmitTriangles(std::span<const GpuVertex> vertices, MetalTexture* texture,
    bool, bool, GpuBlend blend, const GpuClip* clip) {
    impl_->Draw(vertices, texture, blend, clip, false);
}
void MetalRenderer::SubmitQuads(std::span<const GpuVertex> vertices, MetalTexture* texture,
    bool, bool, GpuBlend blend, const GpuClip* clip) {
    impl_->Draw(vertices, texture, blend, clip, false, true);
}
void MetalRenderer::SubmitCoverageTriangles(std::span<const GpuVertex> vertices,
    MetalTexture* mask, GpuBlend blend, const GpuClip* clip) {
    impl_->Require(mask);
    if (mask->resource.pixelFormat != MTLPixelFormatR8Unorm)
        throw std::invalid_argument("Metal coverage draw requires an R8 mask");
    impl_->Draw(vertices, mask, blend, clip, true);
}
void MetalRenderer::SubmitPool(MetalTexture* texture, const GpuPoolParams& p) {
    auto& s = *impl_;
    if (!s.sceneOpen) throw std::logic_error("Metal pool draw outside a scene");
    s.Require(texture);
    if (p.layer > 2 || p.sampler > 3 || size_t(p.blend) >= 3)
        throw std::invalid_argument("Invalid Metal pool draw parameters");
    s.Begin();
    auto* target = s.Output();
    auto* source = texture;
    if (source == target) {
        s.Snapshot(target, true);
        source = s.retired.back().get();
    } else if (target->sampled) s.Snapshot(target);
    id<MTLBuffer> arguments = s.DrawArguments();
    std::memset(arguments.contents, 0, arguments.length);
    [s.argumentEncoder setArgumentBuffer:arguments offset:0];
    [s.argumentEncoder setTexture:source->resource atIndex:source->slot];
    MTLRenderPassDescriptor* desc = [MTLRenderPassDescriptor renderPassDescriptor];
    desc.colorAttachments[0].texture = target->resource;
    desc.colorAttachments[0].loadAction = MTLLoadActionLoad;
    desc.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> encoder = [s.commands renderCommandEncoderWithDescriptor:desc];
    if (!encoder) MetalError(nil, "Create Metal pool encoder");
    const MetalSceneConstants scene{float(target->width), float(target->height),
        target->generated ? 1u : 0u, 0u};
    constexpr uint32_t filters = GpuVertex::kIndependentColorFilter |
        GpuVertex::kColorFilterMask | GpuVertex::kAlphaEdgeFilter;
    const MetalPoolConstants constants{
        p.width, p.height, p.transX, p.transY, p.time1, p.time2, p.time3, p.time5,
        float(p.clip.x), float(p.clip.y), float(p.clip.width), float(p.clip.height),
        p.layer, p.night, source->slot | (p.samplingFlags & filters) |
            (source->generated ? Generated : 0u) |
            (source->opaque ? GpuVertex::kOpaqueInput : 0u), p.sampler};
    [encoder setRenderPipelineState:s.poolPipelines[target->generated ? 1 : 0][size_t(p.blend)]];
    [encoder setVertexBuffer:s.poolVertices offset:0 atIndex:0];
    [encoder setVertexBytes:&scene length:sizeof(scene) atIndex:1];
    [encoder setVertexBytes:&constants length:sizeof(constants) atIndex:3];
    [encoder setFragmentBytes:&scene length:sizeof(scene) atIndex:1];
    [encoder setFragmentBuffer:arguments offset:0 atIndex:2];
    for (unsigned i = 0; i < 4; ++i)
        [encoder setFragmentSamplerState:s.samplers[i] atIndex:i];
    [encoder useResource:source->resource usage:MTLResourceUsageRead stages:MTLRenderStageFragment];
    const MTLViewport viewport{0, 0, double(target->width), double(target->height), 0, 1};
    const MTLScissorRect scissor{0, 0, NSUInteger(target->width), NSUInteger(target->height)};
    [encoder setViewport:viewport];
    [encoder setScissorRect:scissor];
    [encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle indexCount:GpuPoolIndices.size()
        indexType:MTLIndexTypeUInt16 indexBuffer:s.poolIndices indexBufferOffset:0];
    [encoder endEncoding];
    source->sampled = true;
    s.Keep(source->resource); s.Keep(target->resource); s.Keep(arguments);
    s.Keep(s.poolVertices); s.Keep(s.poolIndices);
}
void MetalRenderer::EndSceneAndPresent() {
    auto& s = *impl_;
    if (!s.sceneOpen) throw std::logic_error("No open Metal scene");
    if (!s.imageOnly) s.Present();
    s.Finish(s.imageOnly);
    s.sceneOpen = false;
    // Raw loading scratch is valid for its batch; encoded resources are retained.
    s.raw.clear();
}
bool MetalRenderer::PresentRetainedScene() {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("Retained Metal presentation requires a closed scene");
    if (!s.initialized) return false;
    s.Begin();
    const bool presented = s.Present();
    s.Finish(false);
    return presented;
}
bool MetalRenderer::ResizePresent() {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("Resize Metal surface between scenes");
    s.Finish(true);
    s.context.RecreateSwapchain();
    return !s.context.surfaceSuspended;
}
void MetalRenderer::ResizeScene(int width, int height) {
    auto& s = *impl_;
    if (s.sceneOpen || s.renderImage)
        throw std::logic_error("Resize the main Metal canvas between scenes");
    if (s.width == width && s.height == height) return;
    s.Finish(true);
    auto next = s.Image(width, height, MTLPixelFormatBGRA8Unorm, false, true);
    next->slot = 0;
    s.scaleModes = {};
    s.scaleEnabled = false;
    s.scene = std::move(next);
    s.owners[0] = s.scene.get();
    s.width = width;
    s.height = height;
    s.initialized = false;
}
void MetalRenderer::SetHdrParams(const GpuHdrParams&) {
    throw std::runtime_error("Metal HDR is not supported in this milestone");
}
GpuHdrParams MetalRenderer::HdrParams() const {
    throw std::runtime_error("Metal HDR is not supported in this milestone");
}
void MetalRenderer::PrepareScale(bool fullscreen, std::span<const MetalScaleStep> steps,
    uint32_t width, uint32_t height) {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("Prepare Metal scale between scenes");
    s.Finish(true);
    auto& mode = s.scaleModes[fullscreen ? 1 : 0];
    mode.pipeline.reset();
    mode.warm = false;
    if (steps.empty()) return;
    if (!s.scaleLibrary)
        s.scaleLibrary = std::make_unique<MetalScaleLibrary>((__bridge void*)s.device);
    const MetalScaleImage input{(__bridge void*)s.scene->resource,
        uint32_t(s.scene->resource.pixelFormat), uint32_t(s.width), uint32_t(s.height)};
    mode.pipeline = std::make_unique<MetalScalePipeline>(*s.scaleLibrary, input, steps, width, height);
}
void MetalRenderer::WarmScale(bool fullscreen) {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("Warm Metal scale between scenes");
    auto& mode = s.scaleModes[fullscreen ? 1 : 0];
    if (mode.pipeline) {
        s.Begin();
        mode.pipeline->Record((__bridge void*)s.commands);
        s.Finish(true);
    }
    mode.warm = true;
}
void MetalRenderer::SetScaleMode(bool enabled, bool fullscreen) {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("Switch Metal scale modes between scenes");
    const auto& mode = s.scaleModes[fullscreen ? 1 : 0];
    if (enabled && (!mode.pipeline || !mode.warm))
        throw std::logic_error("Metal scaling must be prepared and warmed during loading");
    s.scaleEnabled = enabled;
    s.scaleFullscreen = fullscreen;
}
MetalScaleImage MetalRenderer::ScaleOutput(bool fullscreen) const {
    const auto& mode = impl_->scaleModes[fullscreen ? 1 : 0];
    return mode.pipeline ? mode.pipeline->output : MetalScaleImage{};
}
MetalRenderer::ScaleCounts MetalRenderer::GetScaleCounts() const {
    const auto& s = *impl_;
    size_t images = 0;
    for (const auto& mode : s.scaleModes)
        if (mode.pipeline) images += mode.pipeline->WritableImageCount();
    return {s.scaleLibrary ? s.scaleLibrary->ProgramCount() : 0,
        s.scaleLibrary ? s.scaleLibrary->LookupCount() : 0, images};
}
GpuClip MetalRenderer::PresentRect(int w, int h) const { return impl_->Rectangle(w, h); }
std::vector<uint32_t> MetalRenderer::ReadScenePixels() {
    if (!impl_->initialized) throw std::logic_error("No initialized Metal scene to read");
    return impl_->Read(impl_->scene.get()).pixels;
}
ImagePixels MetalRenderer::ReadTexturePixels(MetalTexture* texture) {
    impl_->Require(texture);
    return impl_->Read(texture);
}
std::vector<uint32_t> MetalRenderer::ReadScalePixels(bool fullscreen) {
    auto& s = *impl_;
    if (s.sceneOpen) throw std::logic_error("Read Metal scale pixels between scenes");
    const auto& mode = s.scaleModes[fullscreen ? 1 : 0];
    if (!mode.pipeline || !mode.warm) throw std::logic_error("No warmed Metal scale output");
    const auto& image = mode.pipeline->output;
    MetalTexture source;
    source.resource = (__bridge id<MTLTexture>)image.texture;
    source.width = int(image.width);
    source.height = int(image.height);
    auto converted = s.Image(source.width, source.height, MTLPixelFormatRGBA8Unorm, false, false);
    const MetalColorConstants constants{{0, 0, source.width, source.height},
        {255, 255, 255, 255}, 0, source.width, source.height, 0};
    const std::array inputs{&source};
    s.Compute(s.colorKernel, converted.get(), inputs, &constants, sizeof(constants),
        source.width, source.height);
    return s.Read(converted.get()).pixels;
}
uint32_t MetalRenderer::MaxImageDimension2D() const { return 16384; }
MetalRenderer::ResourceCounts MetalRenderer::GetResourceCounts() const {
    const auto& s = *impl_;
    return {s.textures.size(), s.retired.size() + s.raw.size(), 0};
}
}
