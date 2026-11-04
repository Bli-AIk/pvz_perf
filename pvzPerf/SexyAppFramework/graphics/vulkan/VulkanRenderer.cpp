#include "VulkanRenderer.h"
#include "SexyAppFramework/graphics/PoolMesh.h"
#if defined(__ANDROID__)
namespace {
#if defined(PVZ_VULKAN_LEGACY)
constexpr const char* kPipelineCacheName="pipeline_cache_v10.bin";
#else
constexpr const char* kPipelineCacheName="pipeline_cache_v13.bin";
#endif
}
#include "SexyAppFramework/Common.h"
#include <chrono>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <optional>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace Sexy {
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
VkQueryPool VulkanRenderer::benchmarkQueries = VK_NULL_HANDLE;
bool VulkanRenderer::benchmarkSimpleGenerated = false;
bool VulkanRenderer::benchmarkSeparateBlend = false;
bool VulkanRenderer::benchmarkManualBakedFilters = false;
bool VulkanRenderer::benchmarkNonIndexedTriangles = false;
bool VulkanRenderer::benchmark32BitQuads = false;
bool VulkanRenderer::benchmarkPackedColor = false;
bool VulkanRenderer::benchmarkPackedColor16 = false;
bool VulkanRenderer::benchmarkDiscardScene = false;
bool VulkanRenderer::benchmarkInstancedQuads = false;
bool VulkanRenderer::benchmarkPresentCopy = false;
bool VulkanRenderer::benchmarkSolidFill = false;
bool VulkanRenderer::benchmarkGenerateIndexPrefix = true;
unsigned int VulkanRenderer::benchmarkDrawCalls = 0, VulkanRenderer::benchmarkSceneDraws = 0;
unsigned int VulkanRenderer::benchmarkTriangles = 0, VulkanRenderer::benchmarkSceneTriangles = 0;
#endif
namespace {
// Each frame slot owns one fixed geometry allocation; capacity exhaustion is an error.
constexpr unsigned int kFrameVertexCapacity = 65536;
constexpr VkDeviceSize kFrameVertexBytes = VkDeviceSize{kFrameVertexCapacity} * sizeof(VulkanVertex);
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
struct PackedColorVertex {
    float x,y;
    unsigned char color[4];
    float u,v;
    unsigned int slot,sampler;
};
static_assert(sizeof(PackedColorVertex)==28);
struct PackedColor16Vertex {
    float x,y;
    unsigned short color[4];
    float u,v;
    unsigned int slot,sampler;
};
static_assert(sizeof(PackedColor16Vertex)==32);
// Explicit corners preserve CPU transforms and UVs without reconstructing angles.
using SpriteInstance=VulkanSpriteInstance;
static_assert(sizeof(SpriteInstance)==88);
#endif
void Check(VkResult result, const char* operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + " failed: " + std::to_string(result));
}
#define VK_REQUIRE(call) Check((call), #call)

uint32_t MemoryType(uint32_t mask, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(VulkanContext::physicalDevice, &properties);
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i)
        if ((mask & (1u << i)) && (properties.memoryTypes[i].propertyFlags & flags) == flags)
            return i;
    throw std::runtime_error("No suitable Vulkan memory type");
}

struct Buffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0, used = 0;
    void* mapped = nullptr;
    ~Buffer() {
        if (mapped) vkUnmapMemory(VulkanContext::device, memory);
        if (buffer) vkDestroyBuffer(VulkanContext::device, buffer, nullptr);
        if (memory) vkFreeMemory(VulkanContext::device, memory, nullptr);
    }
    void Create(VkDeviceSize bytes, VkBufferUsageFlags usage, bool host = true) {
        size = bytes;
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = size;
        info.usage = usage;
        VK_REQUIRE(vkCreateBuffer(VulkanContext::device, &info, nullptr, &buffer));
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(VulkanContext::device, buffer, &req);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = MemoryType(req.memoryTypeBits, host ?
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT :
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VK_REQUIRE(vkAllocateMemory(VulkanContext::device, &alloc, nullptr, &memory));
        VK_REQUIRE(vkBindBufferMemory(VulkanContext::device, buffer, memory, 0));
        if (host) VK_REQUIRE(vkMapMemory(VulkanContext::device, memory, 0, size, 0, &mapped));
    }
};

struct DrawStream {
    Buffer vertices;
    Buffer indices;
    DrawStream();
};

DrawStream::DrawStream() {
    vertices.Create(kFrameVertexBytes,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    indices.Create(VkDeviceSize{kFrameVertexCapacity / 4} * 6 * sizeof(unsigned short),VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
}

void Barrier(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout,
             VkImageLayout newLayout, VkPipelineStageFlags2 srcStage,
             VkPipelineStageFlags2 dstStage, VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess) {
    VkImageMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    barrier.srcStageMask = oldLayout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_2_NONE : srcStage;
    barrier.dstStageMask = dstStage;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;
    VulkanContext::pipelineBarrier2(cmd, &dependency);
}

void HostReadBarrier(VkCommandBuffer cmd) {
    VkMemoryBarrier2 barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency.memoryBarrierCount = 1;
    dependency.pMemoryBarriers = &barrier;
    VulkanContext::pipelineBarrier2(cmd, &dependency);
}

std::vector<uint32_t> ReadShader(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) throw std::runtime_error("Missing SPIR-V: " + path);
    const auto size = file.tellg();
    if (size <= 0 || size % 4 != 0) throw std::runtime_error("Invalid SPIR-V length: " + path);
    std::vector<uint32_t> words(static_cast<size_t>(size) / 4);
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(words.data()), size))
        throw std::runtime_error("Cannot read SPIR-V: " + path);
    return words;
}
}

struct TextureMemoryPool;

// 纹理专用子分配池：移动端专用 vkAllocateMemory 单次数毫~数十毫秒，且
// maxMemoryAllocationCount 有限（Adreno≈4096）——数千纹理必须共享大块
// DEVICE_LOCAL 内存。仅主线程访问（CreateImageTexture 的线程约束同源）。
struct TextureMemoryPool {
    struct Block { VkDeviceSize offset, size; bool free; };
    struct Chunk { VkDeviceMemory memory; uint32_t typeIndex; std::vector<Block> blocks; };
    std::vector<Chunk> chunks;
    static constexpr VkDeviceSize kChunkBytes = 64ull << 20;

    // 返回 VK_NULL_HANDLE = 池无法服务（超大/无类型），调用方回退专用分配
    VkDeviceMemory Allocate(const VkMemoryRequirements& req, VkDeviceSize& outOffset,
                            VkDeviceSize& outBytes) {
        const uint32_t type = MemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        const VkDeviceSize align = req.alignment ? req.alignment : 1;
        const VkDeviceSize alignedSize = (req.size + align - 1) / align * align;
        if (alignedSize > kChunkBytes) return VK_NULL_HANDLE;
        for (auto& chunk : chunks)
            if (chunk.typeIndex == type && Carve(chunk, align, alignedSize, outOffset)) {
                outBytes = alignedSize;
                return chunk.memory;
            }
        Chunk chunk{};
        chunk.typeIndex = type;
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.allocationSize = kChunkBytes;
        alloc.memoryTypeIndex = type;
        VK_REQUIRE(vkAllocateMemory(VulkanContext::device, &alloc, nullptr, &chunk.memory));
        chunk.blocks.push_back({0, kChunkBytes, true});
        chunks.push_back(std::move(chunk));
        const bool carved = Carve(chunks.back(), align, alignedSize, outOffset);
        // 全新空块必然成功
        if (!carved) throw std::runtime_error("TextureMemoryPool fresh chunk carve failed");
        outBytes = alignedSize;
        return chunks.back().memory;
    }

    void Free(VkDeviceMemory memory, VkDeviceSize offset, VkDeviceSize) {
        for (auto& chunk : chunks) {
            if (chunk.memory != memory) continue;
            for (auto& block : chunk.blocks)
                if (block.offset == offset && !block.free) {
                    block.free = true;
                    Coalesce(chunk);
                    return;
                }
            return;
        }
    }

    ~TextureMemoryPool() {
        for (auto& chunk : chunks)
            if (chunk.memory) vkFreeMemory(VulkanContext::device, chunk.memory, nullptr);
    }
private:
    static bool Carve(Chunk& chunk, VkDeviceSize align, VkDeviceSize size, VkDeviceSize& outOffset) {
        for (size_t i = 0; i < chunk.blocks.size(); ++i) {
            auto& block = chunk.blocks[i];
            if (!block.free) continue;
            const VkDeviceSize start = (block.offset + align - 1) / align * align;
            if (start + size > block.offset + block.size) continue;
            // 就地切分为 [左残片][占用][右残片]，保持 offset 有序
            std::vector<Block> replacement;
            replacement.reserve(3);
            if (start > block.offset)
                replacement.push_back({block.offset, start - block.offset, true});
            replacement.push_back({start, size, false});
            if (start + size < block.offset + block.size)
                replacement.push_back({start + size, block.offset + block.size - start - size, true});
            chunk.blocks.erase(chunk.blocks.begin() + i);
            chunk.blocks.insert(chunk.blocks.begin() + i,
                replacement.begin(), replacement.end());
            outOffset = start;
            return true;
        }
        return false;
    }
    static void Coalesce(Chunk& chunk) {
        for (size_t i = 0; i + 1 < chunk.blocks.size();) {
            if (chunk.blocks[i].free && chunk.blocks[i + 1].free) {
                chunk.blocks[i].size += chunk.blocks[i + 1].size;
                chunk.blocks.erase(chunk.blocks.begin() + i + 1);
            } else ++i;
        }
    }
};

struct VulkanTexture {
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    TextureMemoryPool* pool = nullptr;      // 非空=子分配（析构归还池）
    VkDeviceSize memoryOffset = 0, pooledBytes = 0;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkDescriptorPool computePool = VK_NULL_HANDLE;
    VkDescriptorSet computeSet = VK_NULL_HANDLE;
#if defined(PVZ_VULKAN_LEGACY)
    VkDescriptorSet imageSet = VK_NULL_HANDLE;
#endif
    std::unique_ptr<Buffer> computeMetadata;
    // bindless：纹理不再持有独立描述符集，只占纹理数组的一个槽位。
    // 槽位在创建时写入数组元素，销毁走 retired → ReapRetired 后回收
    // （在途帧的顶点还携带旧槽号，提前复用会让未完成的绘制采到新纹理）。
    uint32_t slot = UINT32_MAX;
    VulkanTexture** slotOwner = nullptr;
    int width = 0, height = 0;
    VkFormat format = VK_FORMAT_B8G8R8A8_UNORM;
    uint32_t texelBytes = 4;
    // Sampled-descriptor layout: GENERAL for compute-written images (caustics),
    // SHADER_READ_ONLY for upload-owned ones. Readback and draw both honor it.
    VkImageLayout sampledLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    bool ownsImage = true;
    bool generated = false;
    bool opaque = false, uploaded = false;
    size_t pendingUpload = SIZE_MAX;
    uint64_t lastDraw = 0;
    unsigned long long retireAfterSerial = 0;
    ~VulkanTexture() {
        if (computeSet) vkFreeDescriptorSets(VulkanContext::device,computePool,1,&computeSet);
        if (slotOwner && *slotOwner == this) *slotOwner = nullptr;
        if (framebuffer) vkDestroyFramebuffer(VulkanContext::device, framebuffer, nullptr);
        if (ownsImage && view) vkDestroyImageView(VulkanContext::device, view, nullptr);
        if (ownsImage && image) vkDestroyImage(VulkanContext::device, image, nullptr);
        if (memory) {
            if (pool) pool->Free(memory, memoryOffset, pooledBytes);
            else vkFreeMemory(VulkanContext::device, memory, nullptr);
        }
    }
};

struct VulkanRenderer::Impl {
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    bool simpleGeneratedExperiment = false, manualFilterExperiment = false;
    VkShaderModule generatedExperimentShader = VK_NULL_HANDLE;
    VkShaderModule solidExperimentShader = VK_NULL_HANDLE;
    VkPipeline solidExperimentPipeline = VK_NULL_HANDLE;
    bool solidFillExperiment = false, generateIndexPrefixExperiment = false;
    std::array<VkPipeline,3> generatedExperimentPipelines{};
    bool nonIndexedExperiment = false, wideIndexExperiment = false, packedColorExperiment = false, packedColor16Experiment = false, discardSceneExperiment = false, instancedExperiment = false, presentCopyExperiment = false;
    VulkanTexture* benchmarkOutput = nullptr;
    VkRenderPass benchmarkOutputPass = VK_NULL_HANDLE;
    VkPipeline benchmarkOutputPipeline = VK_NULL_HANDLE;
    void RecordBenchmarkPresentCopy();
    Buffer wideQuadIndices;
#endif
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT) || defined(PVZ_VULKAN_SEPARATE_BLEND_TEST)
    VkShaderModule separateBlendExperimentShader = VK_NULL_HANDLE;
#endif
    VulkanContext& window;
    TextureMemoryPool texturePool;
    VkDevice device;
    int width, height;
    bool sceneInitialized = false;
    uint64_t frameSerial = 0;
    VulkanContext::FrameContext frame{};
    bool imageOnly = false, targetPassOpen = false;
    VulkanTexture* renderImage = nullptr;
    VkRenderPass scenePass = VK_NULL_HANDLE;
    VkRenderPass imagePass = VK_NULL_HANDLE;
    VkFramebuffer sceneFramebuffer = VK_NULL_HANDLE;
    VkDescriptorSetLayout imageLayout = VK_NULL_HANDLE, samplerLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    std::array<VkPipeline, 3> pipelines{};
    std::array<VkPipeline, 3> spritePipelines{};
    VkPipeline mixedSpritePipeline = VK_NULL_HANDLE;
    std::array<VkPipeline, 3> coveragePipelines{};
    std::array<VkPipeline, 3> imagePipelines{}, imageCoveragePipelines{};
    VkPipeline presentPipeline = VK_NULL_HANDLE;
    VkShaderModule vertexShader = VK_NULL_HANDLE, fragmentShader = VK_NULL_HANDLE;
    VkShaderModule spriteFragmentShader = VK_NULL_HANDLE;
    VkShaderModule mixedSpriteFragmentShader = VK_NULL_HANDLE;
    VkShaderModule hdrFragmentShader = VK_NULL_HANDLE;
    VkShaderModule poolVertexShader = VK_NULL_HANDLE;
    VkPipelineLayout poolLayout = VK_NULL_HANDLE;
    std::array<VkPipeline,3> poolPipelines{}, poolImagePipelines{};
    VkBuffer poolIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory poolIndexMemory = VK_NULL_HANDLE;
    std::string poolShaderPath;
    std::optional<VulkanHdrParams> hdr;
    std::array<VkSampler, 4> samplers{};
#if defined(PVZ_VULKAN_LEGACY)
    std::array<VkDescriptorSet,4> samplerSets{};
    std::vector<VkDescriptorSet> freeImageSets;
#else
    VkDescriptorSet samplerSet = VK_NULL_HANDLE;  // 4 采样器一数组，逐顶点声明
#endif
    std::vector<VkDescriptorPool> pools;
    std::vector<uint32_t> poolFreeSets;
    // bindless 发号器（2026-10-05 用户两轮裁决终态：复用 + 永不烧穿）。
    // 空闲号栈是账本的最小形态——不是 map，无 handle 反查：AllocSlot 优先
    // 弹栈，栈空才递增发新号；Free 只在纹理销毁的同一道闸门调用
    // （ReapRetired 围栏后 / ReleaseScale 的 WaitForFrame 后），安全性搭
    // 纹理销毁既有不变量的便车——那道闸门已经保证无在途帧引用。复用后
    // 65536 的烧穿条件从"全会话累计创建"变为"同时存活数"，实际永不
    // 触发；栈体量=累计创建-存活-再用，长会话几十 KB。DataArray.h 传递
    // 依赖游戏层，渲染器不可反向引入
    // 容量实测（2026-10-05 热机三轮中位）：1024/4096/65536 三档帧时间
    // 全部 ~230 带内重合（数组大小不进热路径）；4096=存活峰值 ~200 的
    // 20 倍余量，模组极端场景亦无虞
    static constexpr uint32_t kTextureSlots = 4096;
    static constexpr uint32_t kSlotMask = kTextureSlots - 1;
    static constexpr uint32_t kGeneratedPremult = 0x80000000u;
    static constexpr uint32_t kMixedAdditive = 0x02000000u;
    std::array<VulkanTexture*, kTextureSlots> slotOwners{};
    // reanim-v4 静态槽位区（顶部）：[staticBase, kTextureSlots) 恒等映射
    // 图像符号号（编译记录烘焙最终下标，零运行时翻译）。默认无静态区。
    uint32_t staticBase = kTextureSlots;
    uint32_t nextSlot = 0;
    std::vector<uint32_t> freeSlots;
    uint32_t AllocSlot() {
        if (!freeSlots.empty()) {
            const uint32_t slot = freeSlots.back();
            freeSlots.pop_back();
            return slot;
        }
        if (nextSlot >= staticBase)
            throw std::runtime_error("Texture slot table exhausted (dynamic region full)");
        return nextSlot++;
    }
    // 全场纹理数组描述符：binding0 = SAMPLED_IMAGE[kCount]，PARTIALLY_BOUND
    // +UPDATE_AFTER_BIND，每纹理创建时写自己的元素，绘制时顶点携带槽位号。
#if !defined(PVZ_VULKAN_LEGACY)
    VkDescriptorPool textureArrayPool = VK_NULL_HANDLE;
    VkDescriptorSet textureArray = VK_NULL_HANDLE;
#endif
    VkFormat presentFormat = VK_FORMAT_UNDEFINED;
    VkCommandPool transferPool = VK_NULL_HANDLE;
    VkFence transferFence = VK_NULL_HANDLE;
    VkCommandBuffer transferCommand = VK_NULL_HANDLE;
    // 上传+合成共用一条一次性命令缓冲（2026-10-11 装载回归修复）：原实现每张
    // 合成图 FlushTextureUploads(提交+等待)+BeginTransfer+FinishTransfer(提交+
    // 等待)两次全量 CPU-GPU 往返，2249 张图 ≈4500 次栅栏等待 ≈5s。改为
    // 累积开放缓冲、批量提交、 CompleteTransfers 单点等待。
    bool transferOpen = false;      // 录制中，未提交
    bool transferInFlight = false;  // 已提交，栅栏未等待
    std::vector<VkDescriptorSet> pendingComposeSets;
    // 合成链裸图（底图/掩码/中间结果，2026-10-11 用户裁决）：不进全局纹理
    // 表、不占槽，走 retired 延迟销毁——ReapRetired 在栅栏等待之后才析构，
    // 时序天然安全（提前 clear 会在 dispatch 录制前销毁仍要引用的裸图）。
    VkCommandPool imageCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer imageCommand = VK_NULL_HANDLE;
    VkFence imageFence = VK_NULL_HANDLE;
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    unsigned int vertexStride = sizeof(VulkanVertex);
#else
    static constexpr unsigned int vertexStride = sizeof(VulkanVertex);
#endif
    // CPU-written geometry only. Textures, pipelines, static indices and GPU
    // targets are shared; their existing GPU barriers order consecutive frames.
    std::array<DrawStream*, VulkanContext::kFramesInFlight> drawStreams{};
    bool recordingFailed = false;
    Buffer staticIndices;
    // 上传 staging 轮换环：提交后的旧 arena 仍被在途批读取，轮换到下一个
    // 即可无等待覆写（单在途批约束下，环回时旧批必已完成）。
    std::vector<std::unique_ptr<Buffer>> stagingRing;
    size_t stagingIndex = 0;
    static constexpr size_t kStagingRingSize = 4;
    Buffer* Staging() { return stagingIndex < stagingRing.size() ? stagingRing[stagingIndex].get() : nullptr; }
    // 管线缓存：Adreno 单条几何/计算管线编译秒级（12 条 GS 管线曾冻结
    // 装载屏 39 秒）。首启编译后落盘，二次启动直接命中。
    mutable VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    void SavePipelineCache();
    // reanim/粒子定义上传的常驻 scratch：此前每定义一次 vkCreateBuffer+
    // vkAllocateMemory，Adreno 单次数十毫秒，五百粒子定义冻结装载屏 39 秒。
    std::unique_ptr<Buffer> reanimScratch;
    VkDeviceSize reanimScratchUsed = 0;
    struct TextureUpload { VulkanTexture* texture; VkDeviceSize offset; size_t arena; };
    std::vector<TextureUpload> pendingUploads;
    // Native command-buffer state, not emulated DX state. All scene/present
    // pipelines share pipelineLayout and use dynamic viewport/scissor. Reset
    // on each recording and whenever another owner records graphics state.
    struct DrawState {
        int width=0,height=0;
        bool hasScissor=false;
        VkRect2D scissor{};
        VkPipeline pipeline=VK_NULL_HANDLE;
        std::array<VkDescriptorSet,2> descriptors{};
        VkBuffer indexBuffer=VK_NULL_HANDLE;
        VkBuffer vertexBuffer=VK_NULL_HANDLE; // 冲刷游程时的已绑定流缓冲
    } drawState;
    std::unique_ptr<VulkanTexture> scene;
    std::unordered_map<VulkanTexture*, std::unique_ptr<VulkanTexture>> textures;
    std::vector<std::unique_ptr<VulkanTexture>> retired;
    struct RetiredDescriptorPool {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        unsigned long long serial = 0;
    };
    std::vector<RetiredDescriptorPool> retiredDescriptorPools;
    void Retire(std::unique_ptr<VulkanTexture> texture) {
        texture->retireAfterSerial = std::max({window.submittedSerial, window.recordingSerial, frame.serial});
        retired.push_back(std::move(texture));
    }
    void GuardStaticSlot(unsigned int slot) {
        // Fresh and fully retired dynamic slots are immutable while referenced.
        // Static overwrites are administrative: drain pending draws, and reject
        // overwriting an entry already recorded by this unsubmitted frame.
        const auto* previous = slotOwners[slot];
        if (!previous) return;
        if (frame.valid && previous->lastDraw == frameSerial)
            throw std::logic_error("Cannot overwrite a static texture slot used by the recording frame");
        window.WaitForFrame();
        if (transferOpen) FinishTransfer();
        CompleteTransfers();
    }
    VulkanTexture* white = nullptr;
    // Pool caustics compute path: one pipeline, one LUT buffer, one target.
    // The target stays in GENERAL layout so the compute write and the scene's
    // fragment sampling share one layout without per-frame transitions.
    struct Caustics {
        VkShaderModule shader = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VkDescriptorSet set = VK_NULL_HANDLE;
        VulkanTexture* source = nullptr;
        VulkanTexture* target = nullptr;
        unsigned long long timeNs = 0;
        bool frameQueued = false;
    } caustics;
    struct ImageKernel {
        VkShaderModule shader=VK_NULL_HANDLE;
        VkPipeline pipeline=VK_NULL_HANDLE;
        VkPipelineLayout layout=VK_NULL_HANDLE;
        VkDescriptorSetLayout setLayout=VK_NULL_HANDLE;
        VkDescriptorPool pool=VK_NULL_HANDLE;
        std::string shaderPath;
    } fontKernel, colorKernel;
    struct AlphaKernel {
        VkShaderModule shader=VK_NULL_HANDLE;
        VkPipeline pipeline=VK_NULL_HANDLE;
        VkPipelineLayout layout=VK_NULL_HANDLE;
        VkDescriptorSetLayout setLayout=VK_NULL_HANDLE;
        VkDescriptorPool pool=VK_NULL_HANDLE;
        std::string shaderPath;
    } alphaKernel;
    struct ScaleMode {
        std::unique_ptr<VulkanScalePipeline> pipeline;
        std::unique_ptr<VulkanTexture> sampledOutput;
        bool warm = false;
    };
    std::unique_ptr<VulkanScaleLibrary> scaleLibrary;
    std::array<std::unique_ptr<ScaleMode>,2> scaleModes;
    bool scaleEnabled = false;
    unsigned scaleMode = 0;
    ScaleMode* ActiveScale() const { return scaleEnabled ? scaleModes[scaleMode].get() : nullptr; }
    void ReleaseScale(unsigned index) {
        auto& mode = scaleModes[index];
        if (!mode) return;
        // 调用方先 WaitForFrame 再到这里：无在途绘制引用，槽号可回收
        if (mode->sampledOutput) freeSlots.push_back(mode->sampledOutput->slot);
        mode.reset(); // borrowed image lives in the scale pipeline.
    }

    Impl(VulkanContext& w, int sw, int sh) : window(w), device(w.device), width(sw), height(sh) {}
    ~Impl() {
        if (frame.valid) window.AbortFrame();
        // Lifetime failure is not a fallback: this only releases partial or
        // completed initialization after the caller reports the original error.
        if (device) vkDeviceWaitIdle(device);
        // Pending uploads only own host snapshots; no unfinished transfer can
        // reference this buffer because FlushTextureUploads waits for its fence.
        pendingUploads.clear();
        stagingRing.clear();
        reanimScratch.reset();
        ReleaseScale(0); ReleaseScale(1);
        scaleLibrary.reset();
        retired.clear();
        for (const auto& retiredPool : retiredDescriptorPools)
            vkDestroyDescriptorPool(device, retiredPool.pool, nullptr);
        retiredDescriptorPools.clear();
        textures.clear();
        if (sceneFramebuffer) vkDestroyFramebuffer(device, sceneFramebuffer, nullptr);
        scene.reset();
        for (auto*& stream : drawStreams) { delete stream; stream = nullptr; }
        for (auto pipeline : pipelines) if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
        for (auto pipeline : spritePipelines) if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
        if (mixedSpritePipeline) vkDestroyPipeline(device,mixedSpritePipeline,nullptr);
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
        for (auto pipeline : generatedExperimentPipelines)
            if (pipeline) vkDestroyPipeline(device,pipeline,nullptr);
        if (generatedExperimentShader) vkDestroyShaderModule(device,generatedExperimentShader,nullptr);
        if (solidExperimentPipeline) vkDestroyPipeline(device,solidExperimentPipeline,nullptr);
        if (solidExperimentShader) vkDestroyShaderModule(device,solidExperimentShader,nullptr);
        if (benchmarkOutputPipeline) vkDestroyPipeline(device,benchmarkOutputPipeline,nullptr);
        delete benchmarkOutput;
        if (benchmarkOutputPass) vkDestroyRenderPass(device,benchmarkOutputPass,nullptr);
#endif
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT) || defined(PVZ_VULKAN_SEPARATE_BLEND_TEST)
        if (separateBlendExperimentShader) vkDestroyShaderModule(device,separateBlendExperimentShader,nullptr);
#endif
        for (auto pipeline : coveragePipelines) if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
        for (auto pipeline : imagePipelines) if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
        for (auto pipeline : imageCoveragePipelines) if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
        ReleasePool();
        if (presentPipeline) vkDestroyPipeline(device, presentPipeline, nullptr);
        if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        if (scenePass) vkDestroyRenderPass(device, scenePass, nullptr);
        if (imagePass) vkDestroyRenderPass(device, imagePass, nullptr);
        if (caustics.pipeline) vkDestroyPipeline(device, caustics.pipeline, nullptr);
        if (caustics.layout) vkDestroyPipelineLayout(device, caustics.layout, nullptr);
        if (caustics.set) vkFreeDescriptorSets(device, caustics.pool, 1, &caustics.set);
        if (caustics.pool) vkDestroyDescriptorPool(device, caustics.pool, nullptr);
        if (caustics.setLayout) vkDestroyDescriptorSetLayout(device, caustics.setLayout, nullptr);
        if (caustics.shader) vkDestroyShaderModule(device, caustics.shader, nullptr);
        if (fontKernel.pipeline) vkDestroyPipeline(device,fontKernel.pipeline,nullptr);
        if (fontKernel.layout) vkDestroyPipelineLayout(device,fontKernel.layout,nullptr);
        if (fontKernel.pool) vkDestroyDescriptorPool(device,fontKernel.pool,nullptr);
        if (fontKernel.setLayout) vkDestroyDescriptorSetLayout(device,fontKernel.setLayout,nullptr);
        if (fontKernel.shader) vkDestroyShaderModule(device,fontKernel.shader,nullptr);
        if (colorKernel.pipeline) vkDestroyPipeline(device,colorKernel.pipeline,nullptr);
        if (colorKernel.layout) vkDestroyPipelineLayout(device,colorKernel.layout,nullptr);
        if (colorKernel.pool) vkDestroyDescriptorPool(device,colorKernel.pool,nullptr);
        if (colorKernel.setLayout) vkDestroyDescriptorSetLayout(device,colorKernel.setLayout,nullptr);
        if (colorKernel.shader) vkDestroyShaderModule(device,colorKernel.shader,nullptr);
        if (alphaKernel.pipeline) vkDestroyPipeline(device,alphaKernel.pipeline,nullptr);
        if (alphaKernel.layout) vkDestroyPipelineLayout(device,alphaKernel.layout,nullptr);
        if (alphaKernel.pool) vkDestroyDescriptorPool(device,alphaKernel.pool,nullptr);
        if (alphaKernel.setLayout) vkDestroyDescriptorSetLayout(device,alphaKernel.setLayout,nullptr);
        if (alphaKernel.shader) vkDestroyShaderModule(device,alphaKernel.shader,nullptr);
        if (vertexShader) vkDestroyShaderModule(device, vertexShader, nullptr);
        if (fragmentShader) vkDestroyShaderModule(device, fragmentShader, nullptr);
        if (spriteFragmentShader) vkDestroyShaderModule(device, spriteFragmentShader, nullptr);
        if (mixedSpriteFragmentShader) vkDestroyShaderModule(device,mixedSpriteFragmentShader,nullptr);
        if (hdrFragmentShader) vkDestroyShaderModule(device, hdrFragmentShader, nullptr);
        for (auto pool : pools) vkDestroyDescriptorPool(device, pool, nullptr);
        for (auto sampler : samplers) if (sampler) vkDestroySampler(device, sampler, nullptr);
#if !defined(PVZ_VULKAN_LEGACY)
        if (textureArrayPool) vkDestroyDescriptorPool(device, textureArrayPool, nullptr);
#endif
        if (imageLayout) vkDestroyDescriptorSetLayout(device, imageLayout, nullptr);
        if (samplerLayout) vkDestroyDescriptorSetLayout(device, samplerLayout, nullptr);
        if (transferFence) vkDestroyFence(device, transferFence, nullptr);
        if (transferPool) vkDestroyCommandPool(device, transferPool, nullptr);
        if (imageFence) vkDestroyFence(device, imageFence, nullptr);
        if (imageCommandPool) vkDestroyCommandPool(device, imageCommandPool, nullptr);
    }

    VkDescriptorSet AllocateSet(VkDescriptorSetLayout layout, VkDescriptorPool* outPool = nullptr) {
        size_t index = 0;
        while (index < pools.size() && poolFreeSets[index] == 0) ++index;
        if (index == pools.size()) {
            static constexpr VkDescriptorPoolSize sizes[] = {
                {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1024}, {VK_DESCRIPTOR_TYPE_SAMPLER, 1024}};
            VkDescriptorPoolCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            info.maxSets = 1024;
            info.poolSizeCount = 2;
            info.pPoolSizes = sizes;
            VkDescriptorPool pool{};
            VK_REQUIRE(vkCreateDescriptorPool(device, &info, nullptr, &pool));
            pools.push_back(pool);
            poolFreeSets.push_back(1024);
        }
        VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        alloc.descriptorPool = pools[index];
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts = &layout;
        VkDescriptorSet set{};
        VK_REQUIRE(vkAllocateDescriptorSets(device, &alloc, &set));
        --poolFreeSets[index];
        if (outPool) *outPool = alloc.descriptorPool;
        return set;
    }

    void UpdateImageDescriptor(VulkanTexture& image) {
        const VkDescriptorImageInfo binding{VK_NULL_HANDLE,image.view,image.sampledLayout};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
#if defined(PVZ_VULKAN_LEGACY)
        if (!image.imageSet) {
            if (freeImageSets.empty()) image.imageSet=AllocateSet(imageLayout);
            else {image.imageSet=freeImageSets.back();freeImageSets.pop_back();}
        }
        write.dstSet=image.imageSet;
#else
        write.dstSet=textureArray;
        write.dstArrayElement=image.slot;
#endif
        write.descriptorCount=1;
        write.descriptorType=VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo=&binding;
        vkUpdateDescriptorSets(device,1,&write,0,nullptr);
    }

    void ReapRetired() {
        // Retire includes the recording submission even when an old backing
        // was only copied, not sampled. Never free a recording resource after merely
        // waiting a transfer or image-only batch that does not cover it.
        if (!retired.empty() || !retiredDescriptorPools.empty()) CompleteTransfers();
        std::erase_if(retired, [&](const auto& texture) {
            if (texture->retireAfterSerial > window.completedSerial) return false;
#if defined(PVZ_VULKAN_LEGACY)
            if (texture->imageSet) freeImageSets.push_back(texture->imageSet);
#endif
            if (texture->slot < kTextureSlots) {
                if (slotOwners[texture->slot] == texture.get()) slotOwners[texture->slot] = nullptr;
                if (texture->slot < staticBase) freeSlots.push_back(texture->slot);
            }
            return true;
        });
        std::erase_if(retiredDescriptorPools, [&](const auto& retiredPool) {
            if (retiredPool.serial > window.completedSerial) return false;
            vkDestroyDescriptorPool(device, retiredPool.pool, nullptr);
            return true;
        });
    }

    // 池水焦散 compute（一次性装配）：管线 + LUT SSBO + 目标图描述集。
    // LUT 是 256x256 灰度字节，主机写入先于任何提交，HOST_COHERENT 直读。
    void InitCaustics(VulkanTexture* target, VulkanTexture* source) {
        auto& c = caustics;
        const bool replacingBindings = c.set != VK_NULL_HANDLE;
        c.target = target;
        c.source = source;
        c.frameQueued = true; // 装配后首帧即产出内容（counter=0），杜绝白帧
        if (!c.setLayout) {
            static constexpr VkDescriptorSetLayoutBinding bindings[] = {
                {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
                {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            };
            VkDescriptorSetLayoutCreateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
            info.bindingCount = 2;
            info.pBindings = bindings;
            VK_REQUIRE(vkCreateDescriptorSetLayout(device, &info, nullptr, &c.setLayout));
            VkDescriptorPoolSize sizes[] = {
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};
            VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            pool.maxSets = 1;
            pool.poolSizeCount = 2;
            pool.pPoolSizes = sizes;
            VK_REQUIRE(vkCreateDescriptorPool(device, &pool, nullptr, &c.pool));
            VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            alloc.descriptorPool = c.pool;
            alloc.descriptorSetCount = 1;
            alloc.pSetLayouts = &c.setLayout;
            VK_REQUIRE(vkAllocateDescriptorSets(device, &alloc, &c.set));
            const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PoolCausticOffsets)};
            VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layoutInfo.setLayoutCount = 1;
            layoutInfo.pSetLayouts = &c.setLayout;
            layoutInfo.pushConstantRangeCount = 1;
            layoutInfo.pPushConstantRanges = &push;
            VK_REQUIRE(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &c.layout));
            VkComputePipelineCreateInfo compute{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            compute.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
            compute.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            compute.stage.module = c.shader;
            compute.stage.pName = "main";
            compute.layout = c.layout;
            VK_REQUIRE(vkCreateComputePipelines(device, pipelineCache, 1, &compute, nullptr, &c.pipeline));
        }
        if (replacingBindings) {
            VkDescriptorPoolSize sizes[] = {
                {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
                {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}};
            VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
            pool.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
            pool.maxSets = 1; pool.poolSizeCount = 2; pool.pPoolSizes = sizes;
            VkDescriptorPool next = VK_NULL_HANDLE;
            VK_REQUIRE(vkCreateDescriptorPool(device, &pool, nullptr, &next));
            VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            allocate.descriptorPool = next; allocate.descriptorSetCount = 1;
            allocate.pSetLayouts = &c.setLayout;
            VkDescriptorSet set = VK_NULL_HANDLE;
            const auto result = vkAllocateDescriptorSets(device, &allocate, &set);
            if (result != VK_SUCCESS) { vkDestroyDescriptorPool(device, next, nullptr); VK_REQUIRE(result); }
            try {
                retiredDescriptorPools.push_back({c.pool,
                    std::max({window.submittedSerial, window.recordingSerial, frame.serial})});
            } catch (...) { vkDestroyDescriptorPool(device, next, nullptr); throw; }
            c.pool = next; c.set = set;
        }
        // 目标可能换代（重开关卡）：改写 storage image 绑定即可，管线与 LUT 复用。
        VkDescriptorImageInfo sourceInfo{samplers[0],source->view,source->sampledLayout};
        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = VK_NULL_HANDLE;
        imageInfo.imageView = target->view;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkWriteDescriptorSet writes[2]{};
        writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[0].dstSet = c.set;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo = &sourceInfo;
        writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writes[1].dstSet = c.set;
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(device, 2, writes, 0, nullptr);
    }

    // 每帧一次，BeginScene 里 render pass 之前记录（dispatch 不能进 pass）。
    // 目标常驻 GENERAL（初始上传也落 GENERAL，见 FlushTextureUploads），
    // 这里只做访问域屏障：上帧片元采样/计算写 → 本帧计算写 → 本帧片元采样。
    void RecordCaustics(VkCommandBuffer cmd) {
        auto& c = caustics;
        if (!c.frameQueued || !c.target || !c.pipeline) return;
        c.frameQueued = false;
        Barrier(cmd, c.target->image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, c.pipeline);
        const auto offsets = EvaluatePoolCaustics(c.timeNs);
        vkCmdPushConstants(cmd,c.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(offsets),&offsets);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, c.layout, 0, 1, &c.set, 0, nullptr);
        vkCmdDispatch(cmd, (128 + 7) / 8, (64 + 3) / 4, 1);
        Barrier(cmd, c.target->image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    }

    void CheckPresentEncoding() const {
        if (hdr) {
            hdr->Validate();
            if (window.swapchainColorSpace!=VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT ||
                window.swapchainFormat!=VK_FORMAT_R16G16B16A16_SFLOAT)
                throw std::runtime_error("HDR renderer requires floating-point linear scRGB");
        } else if (window.swapchainColorSpace!=VK_COLOR_SPACE_SRGB_NONLINEAR_KHR ||
                   (window.swapchainFormat!=VK_FORMAT_B8G8R8A8_UNORM &&
                    window.swapchainFormat!=VK_FORMAT_R8G8B8A8_UNORM))
            throw std::runtime_error("SDR renderer requires an UNORM SDR surface");
    }
    void RefreshPresentPipeline() {
        CheckPresentEncoding();
        if (presentFormat == window.swapchainFormat) return;
        // WSI has waited for old work and rebuilt its render pass.
        if (presentPipeline) vkDestroyPipeline(device, presentPipeline, nullptr);
        presentPipeline = VK_NULL_HANDLE;
        presentPipeline = Pipeline(window.renderPass,VulkanBlend::Opaque,hdr ? hdrFragmentShader : fragmentShader);
        presentFormat = window.swapchainFormat;
    }

    std::unique_ptr<VulkanTexture> Image(int w, int h, VkImageUsageFlags usage, bool opaque,
        VkFormat format=VK_FORMAT_B8G8R8A8_UNORM,
        VkImageLayout sampledLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        uint32_t staticSlot=0xFFFFFFFFu, bool registerSlot=true) {
        auto image = std::make_unique<VulkanTexture>();
        image->width = w;
        image->height = h;
        image->opaque = opaque;
        image->format = format;
        image->texelBytes = format==VK_FORMAT_R8_UNORM?1:
            format==VK_FORMAT_R32G32B32A32_SFLOAT?16:
            format==VK_FORMAT_R16G16B16A16_SFLOAT?8:4;
        VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        info.imageType = VK_IMAGE_TYPE_2D;
        info.format = format;
        info.extent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
        info.mipLevels = info.arrayLayers = 1;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.tiling = VK_IMAGE_TILING_OPTIMAL;
        info.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;
        VK_REQUIRE(vkCreateImage(device, &info, nullptr, &image->image));
        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(device, image->image, &req);
        VkDeviceSize poolOffset = 0, poolBytes = 0;
        VkDeviceMemory pooled = texturePool.Allocate(req, poolOffset, poolBytes);
        if (pooled != VK_NULL_HANDLE) {
            image->memory = pooled;
            image->memoryOffset = poolOffset;
            image->pooledBytes = poolBytes;
            image->pool = &texturePool;
            VK_REQUIRE(vkBindImageMemory(device, image->image, pooled, poolOffset));
        } else {
            VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            alloc.allocationSize = req.size;
            alloc.memoryTypeIndex = MemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            VK_REQUIRE(vkAllocateMemory(device, &alloc, nullptr, &image->memory));
            VK_REQUIRE(vkBindImageMemory(device, image->image, image->memory, 0));
        }
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = image->image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = info.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VK_REQUIRE(vkCreateImageView(device, &view, nullptr, &image->view));
        image->sampledLayout = sampledLayout;
        if (!registerSlot) return image;  // 裸图：合成中间体，不占槽不进描述符数组
        image->slot = staticSlot == 0xFFFFFFFFu ? AllocSlot() : staticBase + staticSlot;
        if (image->slot >= kTextureSlots)
            throw std::runtime_error("Static texture slot out of range");
        if (staticSlot != 0xFFFFFFFFu) GuardStaticSlot(image->slot);
        UpdateImageDescriptor(*image);
        slotOwners[image->slot] = image.get();
        image->slotOwner = &slotOwners[image->slot];
        return image;
    }

    VkCommandBuffer BeginTransfer() {
        // 开放缓冲复用：已有未提交录制直接续写；否则先等在途批次再重置池
        // （池重置与 staging 复用的安全性都来自这一次等待）。
        if (transferOpen) return transferCommand;
        if (transferInFlight) CompleteTransfers();
        VK_REQUIRE(vkResetCommandPool(device, transferPool, 0));
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_REQUIRE(vkBeginCommandBuffer(transferCommand, &begin));
        transferOpen = true;
        return transferCommand;
    }
    void FinishTransfer() {
        if (!transferOpen) return;
        VK_REQUIRE(vkEndCommandBuffer(transferCommand));
        VK_REQUIRE(vkResetFences(device, 1, &transferFence));
        VkCommandBufferSubmitInfo commands{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
        commands.commandBuffer = transferCommand;
        VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
        submit.commandBufferInfoCount = 1;
        submit.pCommandBufferInfos = &commands;
        VK_REQUIRE(VulkanContext::queueSubmit2(window.graphicsQueue, 1, &submit, transferFence));
        transferOpen = false;
        transferInFlight = true;
        // 不在此等待：完成点统一在 CompleteTransfers（帧界/批界/销毁前）。
    }
    void CompleteTransfers() {
        if (transferInFlight) {
            VK_REQUIRE(vkWaitForFences(device, 1, &transferFence, VK_TRUE, UINT64_MAX));
            transferInFlight = false;
        }
        if (!transferOpen && !pendingComposeSets.empty()) {
            VK_REQUIRE(vkFreeDescriptorSets(device, alphaKernel.pool,
                static_cast<unsigned int>(pendingComposeSets.size()), pendingComposeSets.data()));
            pendingComposeSets.clear();
        }
    }

    void RecordTextureUploads(VkCommandBuffer cmd) {
        for (const auto& upload:pendingUploads) {
            const auto& image=*upload.texture;
            // An updated resident image can have earlier fragment reads or a
            // preceding upload on this same graphics queue. Order both before
            // the new write; a brand-new image has no prior layout/access.
            // Compute-written images (caustics) rest in GENERAL, not
            // SHADER_READ_ONLY, matching their sampled descriptors.
            Barrier(cmd,image.image,
                image.uploaded?image.sampledLayout:VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                image.uploaded?(VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT):VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                image.uploaded?(VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT):0,VK_ACCESS_TRANSFER_WRITE_BIT);
            VkBufferImageCopy copy{};
            copy.bufferOffset=upload.offset;
            copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
            copy.imageExtent={uint32_t(image.width),uint32_t(image.height),1};
            vkCmdCopyBufferToImage(cmd,stagingRing[upload.arena]->buffer,image.image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
            Barrier(cmd,image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,image.sampledLayout,
                VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT,
                VK_ACCESS_SHADER_READ_BIT|(image.sampledLayout==VK_IMAGE_LAYOUT_GENERAL?VK_ACCESS_SHADER_WRITE_BIT:0));
        }
    }

    void FlushTextureUploads() {
        if (pendingUploads.empty()) return;
        const auto cmd = BeginTransfer();
        RecordTextureUploads(cmd);
        FinishTransfer();
        for (const auto& upload:pendingUploads) {
            upload.texture->uploaded=true;
            upload.texture->pendingUpload=SIZE_MAX;
        }
        pendingUploads.clear();
        stagingRing[stagingIndex]->used=0;
        // 推进轮换：本批已提交、仍在读此 arena；下一批写别的 arena。
        stagingIndex=(stagingIndex+1)%kStagingRingSize;
    }

    void ReserveTextureUpload(VkDeviceSize bytes) {
        // Bound accumulated host copies: 4 MiB arenas in a rotation ring,
        // enlarged only for a single larger texture. Overflow submits the
        // existing batch and rotates——旧 arena 仍被在途批读取，新批写别的
        // arena，无需等待（单在途批约束；环回时旧批必已完成）。原先此处
        // 每张纹理都 CompleteTransfers：Adreno 每次往返数十毫秒，数千张
        // 纹理被串行拖成 40 秒（2026-10-26 实机 sweep/imgstats 定罪）。
        constexpr VkDeviceSize defaultCapacity=4*1024*1024;
        if (const Buffer* staging=Staging(); staging && bytes>staging->size-staging->used)
            FlushTextureUploads();
        const Buffer* staging=Staging();
        if (!staging || bytes>staging->size) {
            auto next=std::make_unique<Buffer>();
            next->Create(std::max(defaultCapacity,bytes),VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            if (stagingRing.size()<kStagingRingSize) {
                stagingIndex=stagingRing.size();          // 新槽：不覆盖在途 arena
                stagingRing.push_back(std::move(next));
            } else {
                if (transferInFlight) CompleteTransfers(); // 槽重建：旧内存可能被在途批引用
                stagingRing[stagingIndex]=std::move(next);
            }
        }
    }

    void QueueTextureUpload(VulkanTexture* texture,const void* pixels,int pitch) {
        const auto imageWidth=texture->width,imageHeight=texture->height;
        const VkDeviceSize rowBytes=VkDeviceSize(imageWidth)*texture->texelBytes;
        const VkDeviceSize bytes=rowBytes*imageHeight;
        // Odd-sized R8 masks must not misalign the next BGRA8 image copy.
        const VkDeviceSize reserved=(bytes+3)&~VkDeviceSize(3);
        VkDeviceSize offset;
        if (texture->pendingUpload!=SIZE_MAX) {
            // No draw can refer to the superseded version: ReplaceTexture uses
            // a distinct image once that version is recorded in this frame.
            offset=pendingUploads[texture->pendingUpload].offset;
        } else {
            ReserveTextureUpload(reserved);
            offset=stagingRing[stagingIndex]->used;
            pendingUploads.push_back({texture,offset,stagingIndex});
            texture->pendingUpload=pendingUploads.size()-1;
            stagingRing[stagingIndex]->used+=reserved;
        }
        auto* destination=static_cast<std::byte*>(stagingRing[stagingIndex]->mapped)+offset;
        if (VkDeviceSize(pitch)==rowBytes) std::memcpy(destination,pixels,size_t(bytes));
        else for (int y=0;y<imageHeight;++y)
            std::memcpy(destination+size_t(y)*rowBytes,
                static_cast<const std::byte*>(pixels)+size_t(y)*pitch,size_t(rowBytes));
    }

    VkPipeline Pipeline(VkRenderPass pass, VulkanBlend blend, VkShaderModule pixelShader = VK_NULL_HANDLE,
        bool premultiplied = false, bool coverage = false, bool pool = false,
        bool dualSource = false) {
        const VkBool32 specializations[] = {premultiplied ? VK_TRUE : VK_FALSE,
            coverage ? VK_TRUE : VK_FALSE};
        static constexpr VkSpecializationMapEntry entries[] = {
            {0, 0, sizeof(VkBool32)}, {1, sizeof(VkBool32), sizeof(VkBool32)}};
        const VkSpecializationInfo specialization{2, entries, sizeof(specializations), specializations};
        VkPipelineShaderStageCreateInfo shaders[] = {
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                VK_SHADER_STAGE_VERTEX_BIT, pool ? poolVertexShader : vertexShader, "main", nullptr},
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                VK_SHADER_STAGE_FRAGMENT_BIT, pixelShader ? pixelShader : fragmentShader, "main",
                premultiplied || coverage ? &specialization : nullptr},{}};
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT) || defined(PVZ_VULKAN_SEPARATE_BLEND_TEST)
        if (pixelShader && pixelShader == separateBlendExperimentShader)
            shaders[1].pSpecializationInfo = nullptr;
#endif
        VkVertexInputBindingDescription binding{0,
            pool ? uint32_t(sizeof(PoolMeshVertex)) : vertexStride, VK_VERTEX_INPUT_RATE_VERTEX};
        VkVertexInputAttributeDescription attributes[] = {
            {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(VulkanVertex, x)},
            {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(VulkanVertex, r)},
            {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(VulkanVertex, u)},
            {3, 0, VK_FORMAT_R32_UINT, offsetof(VulkanVertex, slot)},
            {4, 0, VK_FORMAT_R32_UINT, offsetof(VulkanVertex, sampler)}};
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
        if (packedColorExperiment) {
            attributes[1].format=packedColor16Experiment ? VK_FORMAT_R16G16B16A16_UNORM : VK_FORMAT_R8G8B8A8_UNORM;
            attributes[1].offset=offsetof(PackedColorVertex,color);
            attributes[2].offset=packedColor16Experiment ? offsetof(PackedColor16Vertex,u) : offsetof(PackedColorVertex,u);
            attributes[3].offset=packedColor16Experiment ? offsetof(PackedColor16Vertex,slot) : offsetof(PackedColorVertex,slot);
            attributes[4].offset=packedColor16Experiment ? offsetof(PackedColor16Vertex,sampler) : offsetof(PackedColorVertex,sampler);
        }
#endif
        static constexpr VkVertexInputAttributeDescription poolAttributes[] = {
            {0,0,VK_FORMAT_R32G32_SFLOAT,offsetof(PoolMeshVertex,x)},
            {1,0,VK_FORMAT_R32G32_SFLOAT,offsetof(PoolMeshVertex,u)},
            {2,0,VK_FORMAT_R32_SFLOAT,offsetof(PoolMeshVertex,alpha)}};
        VkPipelineVertexInputStateCreateInfo input{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        input.vertexBindingDescriptionCount = 1;
        input.pVertexBindingDescriptions = &binding;
        input.vertexAttributeDescriptionCount = pool ? 3 : 5;
        input.pVertexAttributeDescriptions = pool ? poolAttributes : attributes;
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
        static constexpr VkVertexInputAttributeDescription instanceAttributes[]{
            {0,0,VK_FORMAT_R32G32B32A32_SFLOAT,0},
            {1,0,VK_FORMAT_R32G32B32A32_SFLOAT,16},
            {2,0,VK_FORMAT_R32G32B32A32_SFLOAT,32},
            {3,0,VK_FORMAT_R32G32B32A32_SFLOAT,48},
            {4,0,VK_FORMAT_R32G32B32A32_SFLOAT,64},
            {5,0,VK_FORMAT_R32_UINT,80},{6,0,VK_FORMAT_R32_UINT,84}};
        if (instancedExperiment && !pool) {
            binding.inputRate=VK_VERTEX_INPUT_RATE_INSTANCE;
            input.vertexAttributeDescriptionCount=7;
            input.pVertexAttributeDescriptions=instanceAttributes;
        }
#endif
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo msaa{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = viewport.scissorCount = 1;
        static constexpr VkDynamicState states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = states;
        VkPipelineColorBlendAttachmentState attachment{};
        attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        attachment.blendEnable = blend != VulkanBlend::Opaque;
        attachment.srcColorBlendFactor = premultiplied || dualSource ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC_ALPHA;
        attachment.dstColorBlendFactor = dualSource ? VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA :
            blend == VulkanBlend::Additive ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.colorBlendOp = attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        VkPipelineColorBlendStateCreateInfo blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blending.attachmentCount = 1;
        blending.pAttachments = &attachment;
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount = 2;
        info.pStages = shaders;
        info.pVertexInputState = &input;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &msaa;
        info.pColorBlendState = &blending;
        info.pDynamicState = &dynamic;
        info.layout = pool ? poolLayout : pipelineLayout;
        info.renderPass = pass;
        VkPipeline pipeline{};
        VK_REQUIRE(vkCreateGraphicsPipelines(device, pipelineCache, 1, &info, nullptr, &pipeline));
        return pipeline;
    }

    void Initialize(const std::string& vsPath, const std::string& psPath);
    void InitPool();
    void ReleasePool();
    void InitImagePass();
    std::unique_ptr<VulkanTexture> RenderImage(int w, int h);
    void SnapshotRenderImage(VulkanTexture* image, bool force = false);
    void BeginTargetPass();
    void EndTargetPass();
    struct LoadingPipelines {};
    // Upload primitives keep their existing calls; only this tag permits creation.
    void InitImageKernel(ImageKernel& kernel) const;
    void InitImageKernel(ImageKernel& kernel, LoadingPipelines);
    void InitAlphaKernel() const;
    void InitAlphaKernel(LoadingPipelines);
    void FinishImageBatch();
    template<bool Quads = false>
    void Draw(std::span<const VulkanVertex>, VulkanTexture*,
              VkPipeline, const VulkanClip*, int viewportWidth, int viewportHeight
    );
    template<bool Quads>
    void AppendRun(DrawStream*, VkDeviceSize, uint32_t, VkPipeline,
        VkRect2D, int, int
#if defined(PVZ_VULKAN_LEGACY)
        , VkDescriptorSet, VkDescriptorSet
#endif
    );
    // P5 游程合并（2026-10-05）：连续同状态（流连续+管线/描述符/剪刀/视口未变）
    // 的提交延迟 BindVertexBuffers+Draw 合一——图元顺序逐字保持（三角列表同序
    // 合批=帧缓冲混合序列等价），任何状态变化/换流/帧尾先冲刷旧游程
    enum class RunGeometry { Triangles, Quads, Mixed };
    struct PendingRun {
        DrawStream* stream = nullptr;
        VkDeviceSize offset = 0, indexOffset = 0;
        unsigned int count = 0, indexCount = 0;
        RunGeometry geometry = RunGeometry::Triangles;
    };
    PendingRun pendingRun;
    void FlushPendingRun(VkCommandBuffer);
};

void VulkanRenderer::Impl::Initialize(const std::string& vsPath, const std::string& psPath) {
    static_assert(sizeof(unsigned short)==2);
    // Immutable tables cover every possible local uint16 vertex address.
    staticIndices.Create((65536u+98304u)*sizeof(unsigned short),VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    auto* table=static_cast<unsigned short*>(staticIndices.mapped);
    for (unsigned int i=0;i<65536;++i) table[i]=static_cast<unsigned short>(i);
    auto* quad=table+65536;
    for (unsigned int i=0;i<65536;i+=4)
        for (unsigned int corner : {0u,1u,2u,1u,2u,3u}) *quad++=static_cast<unsigned short>(i+corner);
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    generateIndexPrefixExperiment=VulkanRenderer::benchmarkGenerateIndexPrefix;
    presentCopyExperiment=VulkanRenderer::benchmarkPresentCopy;
    instancedExperiment=VulkanRenderer::benchmarkInstancedQuads;
    if (instancedExperiment) vertexStride=sizeof(SpriteInstance);
    discardSceneExperiment=VulkanRenderer::benchmarkDiscardScene;
    packedColor16Experiment=VulkanRenderer::benchmarkPackedColor16;
    packedColorExperiment=VulkanRenderer::benchmarkPackedColor || packedColor16Experiment;
    if (packedColorExperiment) vertexStride=packedColor16Experiment ? sizeof(PackedColor16Vertex) : sizeof(PackedColorVertex);
    nonIndexedExperiment = VulkanRenderer::benchmarkNonIndexedTriangles;
    wideIndexExperiment=VulkanRenderer::benchmark32BitQuads;
    if (wideIndexExperiment) {
        wideQuadIndices.Create(65536u*6*sizeof(unsigned int),VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        auto* wide=static_cast<unsigned int*>(wideQuadIndices.mapped);
        for(unsigned int i=0;i<262144;i+=4)
            for(unsigned int corner:{0u,1u,2u,1u,2u,3u}) *wide++=i+corner;
    }
#endif
    if (width <= 0 || height <= 0) throw std::invalid_argument("Invalid game scene extent");
#if defined(__ANDROID__)
    {
        // Vulkan 1.3 shader target starts a fresh driver cache. Never reuse
        // the previous target's on-device cache after an APK replacement.
        std::error_code cacheError;
        std::filesystem::remove(GetExecutableDirectory()+"pipeline_cache.bin", cacheError);
        std::vector<std::byte> cached;
        if (std::ifstream in(GetExecutableDirectory()+kPipelineCacheName, std::ios::binary); in) {
            in.seekg(0, std::ios::end);
            const auto size = in.tellg();
            in.seekg(0);
            if (size > 0) {
                cached.resize(static_cast<size_t>(size));
                in.read(reinterpret_cast<char*>(cached.data()), size);
            }
        }
        VkPipelineCacheCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
        info.initialDataSize = cached.size();
        info.pInitialData = cached.data();
        VK_REQUIRE(vkCreatePipelineCache(device, &info, nullptr, &pipelineCache));
    }
#endif
    poolShaderPath = (std::filesystem::path(vsPath).parent_path()/"game_pool_vs.spv").string();
    CheckPresentEncoding();
    // 采样器数组（4 元素）：与纹理数组同法消除采样器切换断点——
    // bit0=线性过滤 bit1=重复寻址，索引由顶点携带
    VkDescriptorSetLayoutBinding binding{};
#if defined(PVZ_VULKAN_LEGACY)
    binding.descriptorCount = 1;
#else
    binding.descriptorCount = 4;
#endif
    binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo layout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layout.bindingCount = 1;
    layout.pBindings = &binding;
    VK_REQUIRE(vkCreateDescriptorSetLayout(device, &layout, nullptr, &samplerLayout));
    // bindless 纹理数组：binding0 = SAMPLED_IMAGE[65536]。PARTIALLY_BOUND 允许
    // 未写入的槽存在（不可访问）；UPDATE_AFTER_BIND 允许纹理创建期逐元素
    // 写入而不打断在途帧。需要设备特性 descriptor indexing（见 VulkanContext）。
    VkDescriptorSetLayoutBinding arrayBinding{};
    arrayBinding.binding = 0;
#if defined(PVZ_VULKAN_LEGACY)
    arrayBinding.descriptorCount = 1;
#else
    arrayBinding.descriptorCount = kTextureSlots;
#endif
    arrayBinding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    arrayBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
#if !defined(PVZ_VULKAN_LEGACY)
    VkDescriptorBindingFlags bindFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlags{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO};
    bindingFlags.bindingCount = 1;
    bindingFlags.pBindingFlags = &bindFlags;
#endif
    VkDescriptorSetLayoutCreateInfo arrayLayout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
#if !defined(PVZ_VULKAN_LEGACY)
    arrayLayout.pNext = &bindingFlags;
    arrayLayout.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
#endif
    arrayLayout.bindingCount = 1;
    arrayLayout.pBindings = &arrayBinding;
    VK_REQUIRE(vkCreateDescriptorSetLayout(device, &arrayLayout, nullptr, &imageLayout));
#if !defined(PVZ_VULKAN_LEGACY)
    VkDescriptorPoolSize arraySize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, kTextureSlots};
    VkDescriptorPoolCreateInfo arrayPool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    arrayPool.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    arrayPool.maxSets = 1;
    arrayPool.poolSizeCount = 1;
    arrayPool.pPoolSizes = &arraySize;
    VK_REQUIRE(vkCreateDescriptorPool(device, &arrayPool, nullptr, &textureArrayPool));
    VkDescriptorSetAllocateInfo arrayAlloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    arrayAlloc.descriptorPool = textureArrayPool;
    arrayAlloc.descriptorSetCount = 1;
    arrayAlloc.pSetLayouts = &imageLayout;
    VK_REQUIRE(vkAllocateDescriptorSets(device, &arrayAlloc, &textureArray));
#endif
    for (unsigned i = 0; i < 4; ++i) {
        VkSamplerCreateInfo sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        sampler.magFilter = sampler.minFilter = (i & 1) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        sampler.addressModeU = sampler.addressModeV = sampler.addressModeW = (i & 2) ?
            VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VK_REQUIRE(vkCreateSampler(device, &sampler, nullptr, &samplers[i]));
    }
#if !defined(PVZ_VULKAN_LEGACY)
    samplerSet = AllocateSet(samplerLayout);
#endif
    for (unsigned i = 0; i < 4; ++i) {
        VkDescriptorImageInfo imageBinding{};
        imageBinding.sampler = samplers[i];
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
#if defined(PVZ_VULKAN_LEGACY)
        samplerSets[i]=AllocateSet(samplerLayout);
        write.dstSet=samplerSets[i];
#else
        write.dstSet = samplerSet;
        write.dstArrayElement = i;
#endif
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        write.pImageInfo = &imageBinding;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }
    const VkPushConstantRange push{VkShaderStageFlags(hdr ? VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT),
        0,hdr ? 32u : 8u};
    const VkDescriptorSetLayout layouts[] = {imageLayout, samplerLayout};
    VkPipelineLayoutCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipelineInfo.setLayoutCount = 2;
    pipelineInfo.pSetLayouts = layouts;
    pipelineInfo.pushConstantRangeCount = 1;
    pipelineInfo.pPushConstantRanges = &push;
    VK_REQUIRE(vkCreatePipelineLayout(device, &pipelineInfo, nullptr, &pipelineLayout));
    const auto createShader = [&](const std::string& path, VkShaderModule& shader) {
        auto words = ReadShader(path);
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = words.size() * sizeof(uint32_t);
        info.pCode = words.data();
        VK_REQUIRE(vkCreateShaderModule(device, &info, nullptr, &shader));
    };
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    createShader(instancedExperiment ? PVZ_INSTANCE_EXPERIMENT_SPV : vsPath,vertexShader);
#else
    createShader(vsPath, vertexShader);
#endif
    createShader(psPath, fragmentShader);
    createShader((std::filesystem::path(psPath).parent_path()/"game_sprite_ps.spv").string(),spriteFragmentShader);
#if defined(PVZ_VULKAN_SEPARATE_BLEND_TEST)
    createShader((std::filesystem::path(psPath).parent_path()/"game_sprite_single_output_ps.spv").string(),separateBlendExperimentShader);
#else
    if (window.dualSourceBlendEnabled)
        createShader((std::filesystem::path(psPath).parent_path()/"game_sprite_mixed_ps.spv").string(),mixedSpriteFragmentShader);
#endif
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    if (VulkanRenderer::benchmarkSeparateBlend)
        createShader(PVZ_SEPARATE_BLEND_EXPERIMENT_SPV,separateBlendExperimentShader);
    solidFillExperiment=VulkanRenderer::benchmarkSolidFill;
    if (solidFillExperiment) createShader(PVZ_SOLID_EXPERIMENT_SPV,solidExperimentShader);
    simpleGeneratedExperiment = VulkanRenderer::benchmarkSimpleGenerated;
    manualFilterExperiment=VulkanRenderer::benchmarkManualBakedFilters;
    if (simpleGeneratedExperiment || manualFilterExperiment)
        createShader(manualFilterExperiment ? PVZ_FILTER_EXPERIMENT_SPV : PVZ_SPRITE_EXPERIMENT_SPV,generatedExperimentShader);
#endif
    createShader((std::filesystem::path(psPath).parent_path()/"game_caustic_cs.spv").string(),caustics.shader);
    fontKernel.shaderPath=(std::filesystem::path(psPath).parent_path()/"game_font_cs.spv").string();
    colorKernel.shaderPath=(std::filesystem::path(psPath).parent_path()/"game_color_cs.spv").string();
    alphaKernel.shaderPath=(std::filesystem::path(psPath).parent_path()/"game_alpha_cs.spv").string();
    if (hdr) createShader((std::filesystem::path(psPath).parent_path()/"game_hdr_linear_ps.spv").string(),hdrFragmentShader);
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pool.queueFamilyIndex = window.graphicsFamily;
    VK_REQUIRE(vkCreateCommandPool(device, &pool, nullptr, &transferPool));
    VkCommandBufferAllocateInfo command{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    command.commandPool = transferPool;
    command.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command.commandBufferCount = 1;
    VK_REQUIRE(vkAllocateCommandBuffers(device, &command, &transferCommand));
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VK_REQUIRE(vkCreateFence(device, &fence, nullptr, &transferFence));
    VK_REQUIRE(vkCreateCommandPool(device, &pool, nullptr, &imageCommandPool));
    command.commandPool = imageCommandPool;
    VK_REQUIRE(vkAllocateCommandBuffers(device, &command, &imageCommand));
    VK_REQUIRE(vkCreateFence(device, &fence, nullptr, &imageFence));

    scene = Image(width, height, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false);
    VkAttachmentDescription attachment{};
    attachment.format = VK_FORMAT_B8G8R8A8_UNORM;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // Dirty-region rendering retains the last scene.
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    if (discardSceneExperiment) attachment.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
#endif
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &ref;
    VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    pass.attachmentCount = 1;
    pass.pAttachments = &attachment;
    pass.subpassCount = 1;
    pass.pSubpasses = &subpass;
    VK_REQUIRE(vkCreateRenderPass(device, &pass, nullptr, &scenePass));
    VkFramebufferCreateInfo fb{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fb.renderPass = scenePass;
    fb.attachmentCount = 1;
    fb.pAttachments = &scene->view;
    fb.width = width;
    fb.height = height;
    fb.layers = 1;
    VK_REQUIRE(vkCreateFramebuffer(device, &fb, nullptr, &sceneFramebuffer));
    for (unsigned i = 0; i < pipelines.size(); ++i) pipelines[i] = Pipeline(scenePass, static_cast<VulkanBlend>(i));
#if !defined(PVZ_VULKAN_SEPARATE_BLEND_TEST)
    if (window.dualSourceBlendEnabled
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
        && !VulkanRenderer::benchmarkSeparateBlend
#endif
        )
        mixedSpritePipeline = Pipeline(scenePass,VulkanBlend::Normal,mixedSpriteFragmentShader,
            false,false,false,true);
#endif
    for (unsigned i = 0; i < spritePipelines.size(); ++i)
        if (!mixedSpritePipeline || i == unsigned(VulkanBlend::Opaque)) {
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT) || defined(PVZ_VULKAN_SEPARATE_BLEND_TEST)
            if (separateBlendExperimentShader && i != unsigned(VulkanBlend::Opaque))
                spritePipelines[i] = Pipeline(scenePass,static_cast<VulkanBlend>(i),separateBlendExperimentShader,true);
            else
#endif
                spritePipelines[i] = Pipeline(scenePass,static_cast<VulkanBlend>(i),spriteFragmentShader);
        }
    for (unsigned i = 0; i < coveragePipelines.size(); ++i)
        coveragePipelines[i] = Pipeline(scenePass,static_cast<VulkanBlend>(i),fragmentShader,false,true);
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    if (solidFillExperiment)
        solidExperimentPipeline=Pipeline(scenePass,VulkanBlend::Normal,solidExperimentShader,true);
    if (simpleGeneratedExperiment || manualFilterExperiment)
        for (unsigned int i=0;i<3;++i)
            generatedExperimentPipelines[i]=Pipeline(scenePass,static_cast<VulkanBlend>(i),generatedExperimentShader);
#endif
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    if (presentCopyExperiment) {
        benchmarkOutput=Image(width,height,VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT,false).release();
        attachment.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VK_REQUIRE(vkCreateRenderPass(device,&pass,nullptr,&benchmarkOutputPass));
        fb.renderPass=benchmarkOutputPass;fb.pAttachments=&benchmarkOutput->view;
        VK_REQUIRE(vkCreateFramebuffer(device,&fb,nullptr,&benchmarkOutput->framebuffer));
        benchmarkOutputPipeline=Pipeline(benchmarkOutputPass,VulkanBlend::Opaque,fragmentShader);
    }
#endif
    RefreshPresentPipeline();
}

void VulkanRenderer::Impl::InitImagePass() {
    if (imagePass) return;
    constexpr VkFormatFeatureFlags required = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
#if !defined(PVZ_VULKAN_LEGACY)
        VK_FORMAT_FEATURE_TRANSFER_SRC_BIT | VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
#endif
        VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
    constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    constexpr VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(window.physicalDevice, format, &properties);
    VkImageFormatProperties imageProperties{};
    if ((properties.optimalTilingFeatures & required) != required ||
        vkGetPhysicalDeviceImageFormatProperties(window.physicalDevice, format,
            VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0, &imageProperties) != VK_SUCCESS)
        throw std::runtime_error("GPU render images require RGBA16F attachment, blend, sampling, linear filtering, storage and transfers");
    VkAttachmentDescription attachment{};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    const VkAttachmentReference reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &reference;
    VkRenderPassCreateInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    pass.attachmentCount = pass.subpassCount = 1;
    pass.pAttachments = &attachment;
    pass.pSubpasses = &subpass;
    VK_REQUIRE(vkCreateRenderPass(device, &pass, nullptr, &imagePass));
    try {
        for (size_t i = 0; i < imagePipelines.size(); ++i) {
            imagePipelines[i] = Pipeline(imagePass, static_cast<VulkanBlend>(i), fragmentShader, true);
            imageCoveragePipelines[i] = Pipeline(imagePass, static_cast<VulkanBlend>(i), fragmentShader, true, true);
        }
    } catch (...) {
        for (auto& pipeline : imagePipelines) {
            if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
        for (auto& pipeline : imageCoveragePipelines) {
            if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
            pipeline = VK_NULL_HANDLE;
        }
        vkDestroyRenderPass(device, imagePass, nullptr);
        imagePass = VK_NULL_HANDLE;
        throw;
    }
}

std::unique_ptr<VulkanTexture> VulkanRenderer::Impl::RenderImage(int w, int h) {
    if (w <= 0 || h <= 0) throw std::invalid_argument("Invalid GPU render image extent");
    InitImagePass();
    constexpr VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT;
    VkImageFormatProperties properties{};
    VK_REQUIRE(vkGetPhysicalDeviceImageFormatProperties(window.physicalDevice,
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0, &properties));
    VkPhysicalDeviceProperties deviceProperties{};
    vkGetPhysicalDeviceProperties(window.physicalDevice, &deviceProperties);
    if (uint32_t(w) > std::min(properties.maxExtent.width, deviceProperties.limits.maxFramebufferWidth) ||
        uint32_t(h) > std::min(properties.maxExtent.height, deviceProperties.limits.maxFramebufferHeight) ||
        uint32_t(w) > deviceProperties.limits.maxViewportDimensions[0] ||
        uint32_t(h) > deviceProperties.limits.maxViewportDimensions[1])
        throw std::invalid_argument("GPU render image exceeds device image/framebuffer limits");
    auto image = Image(w, h, usage, false, VK_FORMAT_R16G16B16A16_SFLOAT);
    image->generated = true;
    VkFramebufferCreateInfo framebuffer{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebuffer.renderPass = imagePass;
    framebuffer.attachmentCount = 1;
    framebuffer.pAttachments = &image->view;
    framebuffer.width = uint32_t(w);
    framebuffer.height = uint32_t(h);
    framebuffer.layers = 1;
    VK_REQUIRE(vkCreateFramebuffer(device, &framebuffer, nullptr, &image->framebuffer));
    return image;
}

void VulkanRenderer::Impl::SnapshotRenderImage(VulkanTexture* image, bool force) {
    if (!force && image->lastDraw != frameSerial) return;
    if (targetPassOpen) throw std::logic_error("GPU snapshot requires an ended target pass");
    auto next = RenderImage(image->width, image->height);
    // Keep every earlier descriptor and recorded vertex unchanged. Only the
    // stable public handle acquires the fresh writable backing and slot.
    retired.reserve(retired.size() + 1);
    Barrier(frame.cmd, image->image, image->layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    Barrier(frame.cmd, next->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageCopy copy{};
    copy.srcSubresource = copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.extent = {uint32_t(image->width), uint32_t(image->height), 1};
    vkCmdCopyImage(frame.cmd, image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        next->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    Barrier(frame.cmd, image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image->sampledLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT);
    Barrier(frame.cmd, next->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, next->sampledLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    image->layout = image->sampledLayout;
    next->layout = next->sampledLayout;
    next->uploaded = true;
    std::swap(image->image, next->image);
    std::swap(image->view, next->view);
#if defined(PVZ_VULKAN_LEGACY)
    std::swap(image->imageSet, next->imageSet);
#endif
    std::swap(image->memory, next->memory);
    std::swap(image->framebuffer, next->framebuffer);
    std::swap(image->slot, next->slot);
    std::swap(image->slotOwner, next->slotOwner);
    std::swap(image->lastDraw, next->lastDraw);
    std::swap(image->computeSet,next->computeSet);
    std::swap(image->computePool,next->computePool);
    std::swap(image->computeMetadata,next->computeMetadata);
    slotOwners[image->slot] = image;
    slotOwners[next->slot] = next.get();
    Retire(std::move(next));
}

void VulkanRenderer::Impl::BeginTargetPass() {
    if (!frame.valid || targetPassOpen) throw std::logic_error("Invalid GPU target pass state");
    VulkanTexture* target = renderImage ? renderImage : scene.get();
    if (renderImage) SnapshotRenderImage(target);
    if (!renderImage && !sceneInitialized) {
        Barrier(frame.cmd, target->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
        const VkClearColorValue clear{{0, 0, 0, 1}};
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(frame.cmd, target->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
        target->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        sceneInitialized = true;
    }
    // The scene has one writer and tracked layouts. Do not stall unrelated
    // shader/transfer work with an ALL_COMMANDS dependency when returning to it.
    if (!renderImage) {
        VkPipelineStageFlags2 sourceStage=VK_PIPELINE_STAGE_2_NONE;
        VkAccessFlags2 sourceAccess=VK_ACCESS_2_NONE;
        switch (target->layout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            sourceStage=VK_PIPELINE_STAGE_TRANSFER_BIT;sourceAccess=VK_ACCESS_TRANSFER_WRITE_BIT;break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            sourceStage=VK_PIPELINE_STAGE_TRANSFER_BIT;sourceAccess=VK_ACCESS_TRANSFER_READ_BIT;break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            sourceStage=VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;sourceAccess=VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            sourceStage=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            sourceAccess=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;break;
        default: throw std::logic_error("Unexpected scene layout");
        }
        Barrier(frame.cmd,target->image,target->layout,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            sourceStage,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,sourceAccess,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    } else {
    Barrier(frame.cmd, target->image, target->layout, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    }
    target->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass = renderImage ? imagePass : scenePass;
    pass.framebuffer = renderImage ? target->framebuffer : sceneFramebuffer;
    pass.renderArea.extent = {uint32_t(target->width), uint32_t(target->height)};
    vkCmdBeginRenderPass(frame.cmd, &pass, VK_SUBPASS_CONTENTS_INLINE);
    targetPassOpen = true;
    drawState = {};
}

void VulkanRenderer::Impl::EndTargetPass() {
    if (!targetPassOpen) throw std::logic_error("No GPU target pass to end");
    FlushPendingRun(frame.cmd);
    vkCmdEndRenderPass(frame.cmd);
    VulkanTexture* target = renderImage ? renderImage : scene.get();
    Barrier(frame.cmd, target->image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, target->sampledLayout,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT);
    target->layout = target->sampledLayout;
    targetPassOpen = false;
    drawState = {};
}

void VulkanRenderer::Impl::FinishImageBatch() {
    if (!frame.valid || !imageOnly) throw std::logic_error("No GPU image-only batch to finish");
    EndTargetPass();
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    if (presentCopyExperiment && !renderImage) RecordBenchmarkPresentCopy();
#endif
    // 帧批引用的合成/上传必须先于帧批提交（队列同序）。
    if (transferOpen) FinishTransfer();
    FlushTextureUploads();
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    VulkanRenderer::benchmarkSceneDraws=VulkanRenderer::benchmarkDrawCalls;
    VulkanRenderer::benchmarkSceneTriangles=VulkanRenderer::benchmarkTriangles;
    if (VulkanRenderer::benchmarkQueries)
        vkCmdWriteTimestamp(imageCommand,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,VulkanRenderer::benchmarkQueries,1);
#endif
    VK_REQUIRE(vkEndCommandBuffer(imageCommand));
    VK_REQUIRE(vkResetFences(device, 1, &imageFence));
    VkCommandBufferSubmitInfo commands{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    commands.commandBuffer = imageCommand;
    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &commands;
    VK_REQUIRE(VulkanContext::queueSubmit2(window.graphicsQueue, 1, &submit, imageFence));
    VK_REQUIRE(vkWaitForFences(device, 1, &imageFence, VK_TRUE, UINT64_MAX));
    frame = {};
    imageOnly = false;
}

#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
void VulkanRenderer::Impl::RecordBenchmarkPresentCopy() {
    // Models an identity, SDR fullscreen sample pass; no WSI/acquire/present cost.
    auto* output=benchmarkOutput;
    Barrier(frame.cmd,output->image,output->layout,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        output->layout==VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    VkRenderPassBeginInfo pass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    pass.renderPass=benchmarkOutputPass;pass.framebuffer=output->framebuffer;
    pass.renderArea.extent={static_cast<unsigned int>(width),static_cast<unsigned int>(height)};
    vkCmdBeginRenderPass(frame.cmd,&pass,VK_SUBPASS_CONTENTS_INLINE);
    drawState={};
    const float w=float(width),h=float(height);
    const VulkanVertex vertices[]{
        {w,0,1,1,1,1,1,0,scene->slot,1},{w,h,1,1,1,1,1,1,scene->slot,1},
        {0,0,1,1,1,1,0,0,scene->slot,1},{0,h,1,1,1,1,0,1,scene->slot,1}};
    Draw<true>(vertices,scene.get(),benchmarkOutputPipeline,nullptr,width,height);
    FlushPendingRun(frame.cmd);
    vkCmdEndRenderPass(frame.cmd);
    Barrier(frame.cmd,output->image,VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,output->sampledLayout,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT);
    output->layout=output->sampledLayout;
    drawState={};
}
#endif

VulkanRenderer::VulkanRenderer(VulkanContext& window, int width, int height,
    const std::string& vs, const std::string& ps,const VulkanHdrParams* hdr)
    : impl_(std::make_unique<Impl>(window, width, height)),
      sceneOpen(impl_->frame.valid),
      whiteTexture(impl_->white),
      causticsTexture(impl_->caustics.target), sceneWidth(impl_->width),
      sceneHeight(impl_->height) {
    if (hdr) { hdr->Validate(); impl_->hdr=*hdr; }
    impl_->Initialize(vs, ps);
    const uint32_t pixel = 0xffffffff;
    impl_->white = CreateTexture(1, 1, &pixel, 4, true);
}
VulkanRenderer::~VulkanRenderer() = default;
void VulkanRenderer::Impl::ReleasePool() {
    for (auto& pipeline : poolPipelines)
        if (pipeline) vkDestroyPipeline(device,std::exchange(pipeline,VK_NULL_HANDLE),nullptr);
    for (auto& pipeline : poolImagePipelines)
        if (pipeline) vkDestroyPipeline(device,std::exchange(pipeline,VK_NULL_HANDLE),nullptr);
    if (poolLayout) vkDestroyPipelineLayout(device,std::exchange(poolLayout,VK_NULL_HANDLE),nullptr);
    if (poolVertexShader) vkDestroyShaderModule(device,std::exchange(poolVertexShader,VK_NULL_HANDLE),nullptr);
    if (poolIndexBuffer) vkDestroyBuffer(device,std::exchange(poolIndexBuffer,VK_NULL_HANDLE),nullptr);
    if (poolIndexMemory) vkFreeMemory(device,std::exchange(poolIndexMemory,VK_NULL_HANDLE),nullptr);
}
void VulkanRenderer::Impl::InitPool() {
    if (poolIndexBuffer) return;
    try {
        const auto words = ReadShader(poolShaderPath);
        VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        shader.codeSize=words.size()*sizeof(uint32_t); shader.pCode=words.data();
        VK_REQUIRE(vkCreateShaderModule(device,&shader,nullptr,&poolVertexShader));
        const VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT,0,16};
        const VkDescriptorSetLayout sets[]{imageLayout,samplerLayout};
        VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layout.setLayoutCount=2; layout.pSetLayouts=sets;
        layout.pushConstantRangeCount=1; layout.pPushConstantRanges=&push;
        VK_REQUIRE(vkCreatePipelineLayout(device,&layout,nullptr,&poolLayout));
        InitImagePass();
        for (unsigned i=0;i<3;++i) {
            poolPipelines[i]=Pipeline(scenePass,static_cast<VulkanBlend>(i),fragmentShader,false,false,true);
            poolImagePipelines[i]=Pipeline(imagePass,static_cast<VulkanBlend>(i),fragmentShader,true,false,true);
        }
        const auto upload=[&](const auto& data,VkBufferUsageFlags usage,
                              VkBuffer& buffer,VkDeviceMemory& memory) {
            Buffer staging;
            staging.Create(sizeof(data),usage);
            std::memcpy(staging.mapped,data.data(),sizeof(data));
            vkUnmapMemory(device,staging.memory);
            staging.mapped=nullptr;
            buffer=std::exchange(staging.buffer,VK_NULL_HANDLE);
            memory=std::exchange(staging.memory,VK_NULL_HANDLE);
        };
        upload(GpuPoolIndices,VK_BUFFER_USAGE_INDEX_BUFFER_BIT,poolIndexBuffer,poolIndexMemory);
    } catch (...) {
        ReleasePool();
        throw;
    }
}
void VulkanRenderer::PrepareLoadingPipelines() {
    auto& s = *impl_;
    if (s.frame.valid)
        throw std::logic_error("Prepare Vulkan loading pipelines before opening a scene");
    s.InitImagePass(); // Select the matching floating-point font storage format first.
    s.InitImageKernel(s.fontKernel, Impl::LoadingPipelines{});
    s.InitImageKernel(s.colorKernel, Impl::LoadingPipelines{});
    s.InitAlphaKernel(Impl::LoadingPipelines{});
    s.InitPool();
}
uint32_t VulkanRenderer::SlotOf(const VulkanTexture* texture) {
    return texture ? texture->slot | (texture->generated ? Impl::kGeneratedPremult : 0u) |
        (texture->opaque ? VulkanVertex::kOpaqueInput : 0u) : 0u;
}
uint32_t VulkanRenderer::MaxImageDimension2D() const {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(impl_->window.physicalDevice,&properties);
    return properties.limits.maxImageDimension2D;
}
VulkanRenderer::ResourceCounts VulkanRenderer::GetResourceCounts() const {
    const auto& s = *impl_;
    size_t streamCount = 0;
    for (auto* stream : s.drawStreams) streamCount += stream != nullptr;
    return {s.textures.size(),s.retired.size(),s.pools.size(),streamCount};
}

#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
VulkanTexture* VulkanRenderer::ConvertBenchmarkTexture(VulkanTexture* source,VkFormat format) {
    auto& s=*impl_;
    if (s.frame.valid || !s.textures.contains(source))
        throw std::logic_error("Benchmark conversion requires a completed source image");
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(s.window.physicalDevice,format,&properties);
    const auto required=VK_FORMAT_FEATURE_BLIT_DST_BIT|VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    if ((properties.optimalTilingFeatures&required)!=required)
        throw std::runtime_error("Requested benchmark texture format lacks filtered blit support");
    s.FlushTextureUploads();s.CompleteTransfers();s.window.WaitForFrame();
    auto target=s.Image(source->width,source->height,VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        false,format);
    target->generated=true; // Conversion preserves the source's premultiplied pixels.
    target->texelBytes=format==VK_FORMAT_R16G16B16A16_SFLOAT ? 8 : 4;
    const auto cmd=s.BeginTransfer();
    Barrier(cmd,source->image,source->sampledLayout,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    Barrier(cmd,target->image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT);
    VkImageBlit blit{};
    blit.srcSubresource=blit.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    blit.srcOffsets[1]=blit.dstOffsets[1]={source->width,source->height,1};
    vkCmdBlitImage(cmd,source->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,target->image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_NEAREST);
    Barrier(cmd,source->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,source->sampledLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
    Barrier(cmd,target->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,target->sampledLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
    s.FinishTransfer();s.CompleteTransfers();
    auto* result=target.get();s.textures.emplace(result,std::move(target));return result;
}
#endif

VulkanTexture* VulkanRenderer::CreateRenderImage(int width, int height) {
    auto& s = *impl_;
    auto image = s.RenderImage(width, height);
    // A separate synchronous transfer touches only this new allocation. It never
    // submits the open parent frame or rewinds its mapped vertex streams.
    const auto cmd = s.BeginTransfer();
    Barrier(cmd, image->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);
    const VkClearColorValue clear{{0, 0, 0, 0}};
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cmd, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
    Barrier(cmd, image->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, image->sampledLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
    s.FinishTransfer();
    image->layout = image->sampledLayout;
    image->uploaded = true;
    auto* handle = image.get();
    s.textures.emplace(handle, std::move(image));
    return handle;
}

void VulkanRenderer::SetRenderImage(VulkanTexture* image) {
    auto& s = *impl_;
    if (image && (!s.textures.contains(image) || !image->generated))
        throw std::invalid_argument("Render target must be a live GPU render image in this renderer");
    if (image == s.renderImage) return;
    if (s.frame.valid && s.imageOnly && !image) {
        s.FinishImageBatch();
        s.renderImage = nullptr;
        return;
    }
    if (s.frame.valid) s.EndTargetPass();
    s.renderImage = image;
    if (s.frame.valid) s.BeginTargetPass();
}

void VulkanRenderer::Impl::InitImageKernel(ImageKernel& kernel) const {
    if (!kernel.pipeline)
        throw std::logic_error("Vulkan compute pipeline was not prepared during loading: " +
            kernel.shaderPath);
}

void VulkanRenderer::Impl::InitImageKernel(ImageKernel& kernel, LoadingPipelines) {
    if (kernel.pipeline) return;
    const auto words=ReadShader(kernel.shaderPath);
    VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shader.codeSize=words.size()*sizeof(uint32_t);shader.pCode=words.data();
    VK_REQUIRE(vkCreateShaderModule(device,&shader,nullptr,&kernel.shader));
    static constexpr VkDescriptorSetLayoutBinding bindings[]={
        {0,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
        {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo setLayout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setLayout.bindingCount=2;setLayout.pBindings=bindings;
    VK_REQUIRE(vkCreateDescriptorSetLayout(device,&setLayout,nullptr,&kernel.setLayout));
    const VkDescriptorSetLayout sets[]={imageLayout,samplerLayout,kernel.setLayout};
    const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(VulkanFontGlyph)+sizeof(uint32_t)};
    VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout.setLayoutCount=3;layout.pSetLayouts=sets;
    layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&push;
    VK_REQUIRE(vkCreatePipelineLayout(device,&layout,nullptr,&kernel.layout));
    static constexpr VkDescriptorPoolSize sizes[]={
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,kTextureSlots},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,kTextureSlots}};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.flags=VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool.maxSets=kTextureSlots;pool.poolSizeCount=2;pool.pPoolSizes=sizes;
    VK_REQUIRE(vkCreateDescriptorPool(device,&pool,nullptr,&kernel.pool));
    VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipeline.layout=kernel.layout;
    pipeline.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,
        VK_SHADER_STAGE_COMPUTE_BIT,kernel.shader,"main",nullptr};
    VK_REQUIRE(vkCreateComputePipelines(device,pipelineCache,1,&pipeline,nullptr,&kernel.pipeline));
}

void VulkanRenderer::Impl::InitAlphaKernel() const {
    if (!alphaKernel.pipeline)
        throw std::logic_error("Vulkan alpha pipeline requires PrepareLoadingPipelines");
}

void VulkanRenderer::Impl::InitAlphaKernel(LoadingPipelines) {
    auto& kernel=alphaKernel;
    if(kernel.pipeline) return;
    const auto words=ReadShader(kernel.shaderPath);
    VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    shader.codeSize=words.size()*sizeof(uint32_t);shader.pCode=words.data();
    VK_REQUIRE(vkCreateShaderModule(device,&shader,nullptr,&kernel.shader));
    static constexpr VkDescriptorSetLayoutBinding bindings[]={
        {0,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
        {1,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr},
        {2,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr}};
    VkDescriptorSetLayoutCreateInfo setLayout{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setLayout.bindingCount=3;setLayout.pBindings=bindings;
    VK_REQUIRE(vkCreateDescriptorSetLayout(device,&setLayout,nullptr,&kernel.setLayout));
    const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT,0,16};
    VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout.setLayoutCount=1;layout.pSetLayouts=&kernel.setLayout;
    layout.pushConstantRangeCount=1;layout.pPushConstantRanges=&push;
    VK_REQUIRE(vkCreatePipelineLayout(device,&layout,nullptr,&kernel.layout));
    static constexpr VkDescriptorPoolSize sizes[]={
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,2u*kTextureSlots},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,kTextureSlots}};
    VkDescriptorPoolCreateInfo pool{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pool.flags=VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool.maxSets=kTextureSlots;pool.poolSizeCount=2;
    pool.pPoolSizes=sizes;
    VK_REQUIRE(vkCreateDescriptorPool(device,&pool,nullptr,&kernel.pool));
    VkComputePipelineCreateInfo pipeline{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    pipeline.layout=kernel.layout;
    pipeline.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,nullptr,0,
        VK_SHADER_STAGE_COMPUTE_BIT,kernel.shader,"main",nullptr};
    VK_REQUIRE(vkCreateComputePipelines(device,pipelineCache,1,&pipeline,nullptr,&kernel.pipeline));
}

VulkanTexture* VulkanRenderer::ComposeAlphaRaw(VulkanTexture* base,VulkanTexture* alpha,
    uint32_t color,bool colorOnly) {
    auto& s=*impl_;
    if(!base || !alpha || base->width!=alpha->width || base->height!=alpha->height)
        throw std::invalid_argument("Invalid Vulkan alpha composition dimensions");
    s.InitAlphaKernel();
    // Each queued dispatch owns its bindings until this transfer batch completes.
    if (s.pendingComposeSets.size() >= 1024) {
        s.FinishTransfer();
        s.CompleteTransfers();
    }
    const auto cmd=s.BeginTransfer();
    s.RecordTextureUploads(cmd);
    auto target=s.Image(base->width,base->height,VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,false,
        VK_FORMAT_R8G8B8A8_UNORM,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,0xFFFFFFFFu,false);
    const VkDescriptorImageInfo images[]={
        {s.samplers[0],base->view,base->sampledLayout},
        {s.samplers[0],alpha->view,alpha->sampledLayout},
        {VK_NULL_HANDLE,target->view,VK_IMAGE_LAYOUT_GENERAL}};
    VkDescriptorSet composeSet=VK_NULL_HANDLE;
    VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocate.descriptorPool=s.alphaKernel.pool;allocate.descriptorSetCount=1;
    allocate.pSetLayouts=&s.alphaKernel.setLayout;
    VK_REQUIRE(vkAllocateDescriptorSets(s.device,&allocate,&composeSet));
    std::array<VkWriteDescriptorSet,3> writes{};
    for(uint32_t i=0;i<3;++i) {
        writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet=composeSet;writes[i].dstBinding=i;writes[i].descriptorCount=1;
        writes[i].descriptorType=i==2?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo=&images[i];
    }
    vkUpdateDescriptorSets(s.device,3,writes.data(),0,nullptr);
    s.pendingComposeSets.push_back(composeSet);
    Barrier(cmd,target->image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,VK_ACCESS_SHADER_WRITE_BIT);
    for(auto* input:{base,alpha})
        Barrier(cmd,input->image,input->sampledLayout,input->sampledLayout,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,s.alphaKernel.pipeline);
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,s.alphaKernel.layout,0,1,&composeSet,0,nullptr);
    struct Push { int width,height;uint32_t color,colorOnly; };
    const Push push{base->width,base->height,color,colorOnly};
    static_assert(sizeof(Push)==16);
    vkCmdPushConstants(cmd,s.alphaKernel.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),&push);
    vkCmdDispatch(cmd,(uint32_t(base->width)+7)/8,(uint32_t(base->height)+7)/8,1);
    Barrier(cmd,target->image,VK_IMAGE_LAYOUT_GENERAL,target->sampledLayout,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
    target->layout=target->sampledLayout;target->uploaded=true;
    auto* handle=target.get();
    s.Retire(std::move(target));
    return handle;
}

VulkanTexture* VulkanRenderer::AdoptComposedTexture(VulkanTexture* composed) {
    auto& s=*impl_;
    const auto owned=std::find_if(s.retired.begin(),s.retired.end(),
        [composed](const std::unique_ptr<VulkanTexture>& texture){return texture.get()==composed;});
    if(owned==s.retired.end())
        throw std::invalid_argument("Adopt requires a renderer-owned raw compose image");
    auto texture=std::move(*owned);
    s.retired.erase(owned);
    auto* handle=texture.get();
    handle->slot=s.AllocSlot();
    if(handle->slot>=s.kTextureSlots) throw std::runtime_error("Out of texture slots");
    s.UpdateImageDescriptor(*handle);
    s.slotOwners[handle->slot]=handle;
    handle->slotOwner=&s.slotOwners[handle->slot];
    s.textures.emplace(handle,std::move(texture));
    return handle;
}

VulkanTexture* VulkanRenderer::ComposeAlpha(VulkanTexture* base,VulkanTexture* alpha,
    uint32_t color,bool colorOnly) {
    auto& s=*impl_;
    if(!base || !alpha || !s.textures.contains(base) || !s.textures.contains(alpha))
        throw std::exception{};
    // 注册版=裸图合成+采纳：语义与批量化前逐位一致，仅中间产物不再全局登记。
    return AdoptComposedTexture(ComposeAlphaRaw(base,alpha,color,colorOnly));
}

void VulkanRenderer::BindStaticTexture(VulkanTexture* texture,uint32_t staticSlot) {
    auto& s=*impl_;
    const uint32_t slot=s.staticBase+staticSlot;
    if(!s.textures.contains(texture) || slot>=s.kTextureSlots || texture->lastDraw || texture->slot>=s.staticBase)
        throw std::exception{};
    s.GuardStaticSlot(slot);
    // lastDraw==0 does not exclude references in an off-frame compute batch.
    if (s.transferOpen) s.FinishTransfer();
    s.CompleteTransfers();
    s.slotOwners[texture->slot]=nullptr;s.freeSlots.push_back(texture->slot);
    texture->slot=slot;s.slotOwners[slot]=texture;texture->slotOwner=&s.slotOwners[slot];
    s.UpdateImageDescriptor(*texture);
}

VulkanTexture* VulkanRenderer::CopyTextureRegion(VulkanTexture* source,int x,int y,int width,int height) {
    auto& s=*impl_;
    if(!s.textures.contains(source) ||
        x<0 || y<0 || width<=0 || height<=0 || x>=source->width || y>=source->height)
        throw std::exception{};
    s.FlushTextureUploads();
    auto allocated=s.Image(width,height,VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT,source->opaque,source->format);
    auto* target=allocated.get();
    s.textures.emplace(target,std::move(allocated));
    target->generated=source->generated;
    const bool activeFrame=s.frame.valid;
    if(activeFrame) s.EndTargetPass();
    const auto cmd=activeFrame?s.frame.cmd:s.BeginTransfer();
    Barrier(cmd,source->image,source->sampledLayout,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    Barrier(cmd,target->image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT);
    const int copyWidth=std::min(width,source->width-x),copyHeight=std::min(height,source->height-y);
    if(copyWidth!=width || copyHeight!=height) {
        const VkClearColorValue clear{{0,0,0,0}};
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdClearColorImage(cmd,target->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&clear,1,&range);
        Barrier(cmd,target->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
    }
    VkImageCopy copy{};
    copy.srcSubresource=copy.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    copy.srcOffset={x,y,0};
    copy.extent={uint32_t(copyWidth),uint32_t(copyHeight),1};
    vkCmdCopyImage(cmd,source->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        target->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
    Barrier(cmd,source->image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,source->sampledLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
    Barrier(cmd,target->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,target->sampledLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
    if(activeFrame) {
        source->lastDraw=target->lastDraw=s.frameSerial;
        s.BeginTargetPass();
    } else s.FinishTransfer();
    target->layout=target->sampledLayout;target->uploaded=true;
    return target;
}

VulkanTexture* VulkanRenderer::ProcessImage(VulkanTexture* source,
    const VulkanImageProcessing& processing) {
    auto& s=*impl_;
    if(!source || !s.textures.contains(source) ||
        processing.width<=0 || processing.height<=0 ||
        uint32_t(processing.operation)>uint32_t(VulkanImageOperation::Colorize))
        throw std::exception{};
    s.InitImageKernel(s.colorKernel);
    s.FlushTextureUploads();
    auto target=s.Image(processing.width,processing.height,VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,false,VK_FORMAT_R8G8B8A8_UNORM);
    const auto bytes=3*sizeof(uint32_t);
    target->computeMetadata=std::make_unique<Buffer>();
    target->computeMetadata->Create(bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto* words=static_cast<uint32_t*>(target->computeMetadata->mapped);
    const std::array<uint32_t,3> metadata{uint32_t(source->width),uint32_t(source->height),
        SlotOf(source)};
    std::copy(metadata.begin(),metadata.end(),words);
    VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocate.descriptorPool=s.colorKernel.pool;allocate.descriptorSetCount=1;
    allocate.pSetLayouts=&s.colorKernel.setLayout;
    VK_REQUIRE(vkAllocateDescriptorSets(s.device,&allocate,&target->computeSet));
    target->computePool=s.colorKernel.pool;
    const VkDescriptorImageInfo image{VK_NULL_HANDLE,target->view,VK_IMAGE_LAYOUT_GENERAL};
    const VkDescriptorBufferInfo buffer{target->computeMetadata->buffer,0,bytes};
    std::array<VkWriteDescriptorSet,2> writes{};
    for(auto& write:writes) {
        write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet=target->computeSet;write.descriptorCount=1;
    }
    writes[0].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;writes[0].pImageInfo=&image;
    writes[1].dstBinding=1;writes[1].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[1].pBufferInfo=&buffer;
    vkUpdateDescriptorSets(s.device,uint32_t(writes.size()),writes.data(),0,nullptr);
    auto* handle=target.get();
    s.textures.emplace(handle,std::move(target));
    const bool activeFrame=s.frame.valid;
    if(activeFrame) s.EndTargetPass();
    const auto cmd=activeFrame?s.frame.cmd:s.BeginTransfer();
    Barrier(cmd,source->image,source->sampledLayout,source->sampledLayout,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
    if(activeFrame) source->lastDraw=s.frameSerial;
    Barrier(cmd,handle->image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,VK_ACCESS_SHADER_WRITE_BIT);
    vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,s.colorKernel.pipeline);
#if defined(PVZ_VULKAN_LEGACY)
    const VkDescriptorSet sets[]={source->imageSet,s.samplerSets[0],handle->computeSet};
#else
    const VkDescriptorSet sets[]={s.textureArray,s.samplerSet,handle->computeSet};
#endif
    vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_COMPUTE,s.colorKernel.layout,0,3,sets,0,nullptr);
    static_assert(sizeof(VulkanImageProcessing)==36);
    vkCmdPushConstants(cmd,s.colorKernel.layout,VK_SHADER_STAGE_COMPUTE_BIT,
        0,sizeof(processing),&processing);
    vkCmdDispatch(cmd,(uint32_t(processing.width)+7)/8,(uint32_t(processing.height)+7)/8,1);
    Barrier(cmd,handle->image,VK_IMAGE_LAYOUT_GENERAL,handle->sampledLayout,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT);
    handle->layout=handle->sampledLayout;handle->uploaded=true;
    if(activeFrame) {
        handle->lastDraw=s.frameSerial;
        s.BeginTargetPass();
    } else s.FinishTransfer();
    return handle;
}

void VulkanRenderer::RenderFontAtlas(VulkanTexture* target,VulkanTexture* source,
    std::span<const VulkanFontGlyph> glyphs,uint32_t samplingFlags,bool forceWhite) {
    auto& s=*impl_;
    if (!target || !s.textures.contains(target) ||
        !s.frame.valid || s.renderImage!=target || !target->generated ||
        !source || source==target || !s.textures.contains(source))
        throw std::exception{};
    for (const auto& glyph:glyphs)
        if (glyph.sourceX<0 || glyph.sourceY<0 || glyph.sourceWidth<=0 || glyph.sourceHeight<=0 ||
            int64_t(glyph.sourceX)+glyph.sourceWidth>source->width ||
            int64_t(glyph.sourceY)+glyph.sourceHeight>source->height ||
            glyph.targetX<0 || glyph.targetY<0 || glyph.targetWidth<=0 || glyph.targetHeight<=0 ||
            int64_t(glyph.targetX)+glyph.targetWidth>target->width ||
            int64_t(glyph.targetY)+glyph.targetHeight>target->height)
            throw std::exception{};
    s.InitImageKernel(s.fontKernel);
    s.FlushTextureUploads();
    s.EndTargetPass();
    const auto bytes=4*sizeof(uint32_t);
    auto metadata=std::make_unique<Buffer>();
    metadata->Create(bytes,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    auto* words=static_cast<uint32_t*>(metadata->mapped);
    const std::array<uint32_t,4> header{uint32_t(source->width),uint32_t(source->height),
        samplingFlags,SlotOf(source)};
    std::copy(header.begin(),header.end(),words);
    source->lastDraw=s.frameSerial;
    Barrier(s.frame.cmd,source->image,source->sampledLayout,source->sampledLayout,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT);
    if (target->computeSet) s.SnapshotRenderImage(target, true);
    target->computeMetadata=std::move(metadata);
    if (!target->computeSet) {
        VkDescriptorSetAllocateInfo allocate{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocate.descriptorPool=s.fontKernel.pool;allocate.descriptorSetCount=1;
        allocate.pSetLayouts=&s.fontKernel.setLayout;
        VK_REQUIRE(vkAllocateDescriptorSets(s.device,&allocate,&target->computeSet));
        target->computePool=s.fontKernel.pool;
    }
    const VkDescriptorImageInfo image{VK_NULL_HANDLE,target->view,VK_IMAGE_LAYOUT_GENERAL};
    const VkDescriptorBufferInfo buffer{target->computeMetadata->buffer,0,bytes};
    std::array<VkWriteDescriptorSet,2> writes{};
    for (auto& write:writes) {
        write.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet=target->computeSet;write.descriptorCount=1;
    }
    writes[0].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;writes[0].pImageInfo=&image;
    writes[1].dstBinding=1;writes[1].descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;writes[1].pBufferInfo=&buffer;
    vkUpdateDescriptorSets(s.device,uint32_t(writes.size()),writes.data(),0,nullptr);
    Barrier(s.frame.cmd,target->image,target->layout,VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT);
    target->layout=VK_IMAGE_LAYOUT_GENERAL;
    vkCmdBindPipeline(s.frame.cmd,VK_PIPELINE_BIND_POINT_COMPUTE,s.fontKernel.pipeline);
#if defined(PVZ_VULKAN_LEGACY)
    const VkDescriptorSet sets[]={source->imageSet,s.samplerSets[0],target->computeSet};
#else
    const VkDescriptorSet sets[]={s.textureArray,s.samplerSet,target->computeSet};
#endif
    vkCmdBindDescriptorSets(s.frame.cmd,VK_PIPELINE_BIND_POINT_COMPUTE,s.fontKernel.layout,
        0,3,sets,0,nullptr);
    struct Push { VulkanFontGlyph glyph;uint32_t white; };
    static_assert(sizeof(Push)==36);
    for (const auto& glyph:glyphs) {
        const Push push{glyph,forceWhite};
        vkCmdPushConstants(s.frame.cmd,s.fontKernel.layout,VK_SHADER_STAGE_COMPUTE_BIT,0,sizeof(push),&push);
        vkCmdDispatch(s.frame.cmd,(uint32_t(glyph.targetWidth)+7)/8,(uint32_t(glyph.targetHeight)+7)/8,1);
    }
    Barrier(s.frame.cmd,target->image,VK_IMAGE_LAYOUT_GENERAL,target->sampledLayout,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT);
    target->layout=target->sampledLayout;
    s.BeginTargetPass();
    target->lastDraw=s.frameSerial;
}
VulkanRenderer::TextureUploadState VulkanRenderer::GetTextureUploadState() const {
    const auto& s=*impl_;
    // Ring rotation can grow any slot, not necessarily back(). This diagnostic
    // reports the largest arena capacity, preserving its single-arena meaning.
    uint64_t capacity = 0;
    for (const auto& arena : s.stagingRing) capacity = std::max(capacity,uint64_t(arena->size));
    return {s.pendingUploads.size(),capacity};
}

VulkanTexture* VulkanRenderer::CreateTextureStatic(int width, int height, const void* bgra, int pitch, bool opaque, uint32_t staticSlot) {
    if (width <= 0 || height <= 0 || !bgra || pitch < int64_t(width) * 4)
        throw std::invalid_argument("Invalid BGRA texture source");
    if (staticSlot == 0xFFFFFFFFu)
        throw std::invalid_argument("Static texture slot required");
    auto& s = *impl_;
    const VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    auto texture = s.Image(width, height, usage, opaque,
        VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, staticSlot);
    auto* handle = texture.get();
    const auto position=s.textures.emplace(handle,std::move(texture)).first;
    try {s.QueueTextureUpload(handle,bgra,pitch);}
    catch (...) {s.textures.erase(position);throw;}
    return handle;
}

void VulkanRenderer::ReserveStaticTextureSlots(uint32_t count) {
    auto& s = *impl_;
    // count = 最高符号号（id 从 1 起，None=0 不占静态区）；内部 +1 补哨兵
    const uint32_t aSlots = count + 1;
    if (aSlots >= s.kTextureSlots)
        throw std::invalid_argument("Static region too large");
    s.staticBase = s.kTextureSlots - aSlots;
    if (s.nextSlot > s.staticBase)
        throw std::runtime_error("Dynamic slots already exceed static base");
}

VulkanTexture* VulkanRenderer::CreateTexture(int width, int height, const void* bgra, int pitch, bool opaque) {
    if (width <= 0 || height <= 0 || !bgra || pitch < int64_t(width) * 4)
        throw std::invalid_argument("Invalid BGRA texture source");
    auto& s = *impl_;
    auto texture = s.Image(width, height, VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT, opaque);
    texture->opaque=opaque;
    auto* handle = texture.get();
    const auto position=s.textures.emplace(handle,std::move(texture)).first;
    try {s.QueueTextureUpload(handle,bgra,pitch);}
    catch (...) {s.textures.erase(position);throw;}
    return handle;
}

VulkanTexture* VulkanRenderer::CreateRawTexture(int width, int height, const void* bgra, int pitch) {
    if (width <= 0 || height <= 0 || !bgra || pitch < int64_t(width) * 4)
        throw std::invalid_argument("Invalid BGRA texture source");
    auto& s = *impl_;
    // 裸图（registerSlot=false）：合成链中间载体，不占槽、不进全局表，
    // rawComposeScratch 持有至批次完成（CompleteTransfers）销毁。
    auto texture = s.Image(width, height,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT, false,
        VK_FORMAT_B8G8R8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0xFFFFFFFFu, false);
    auto* handle = texture.get();
    s.Retire(std::move(texture));
    s.QueueTextureUpload(handle,bgra,pitch);
    return handle;
}

VulkanTexture* VulkanRenderer::ReplaceTexture(VulkanTexture* previous, int width, int height,
    const void* bgra, int pitch, bool opaque) {
    if (width<=0 || height<=0 || !bgra || pitch<int64_t(width)*4)
        throw std::invalid_argument("Invalid replacement BGRA texture source");
    if (!previous) return CreateTexture(width,height,bgra,pitch,opaque);
    auto& s=*impl_;
    if (previous==s.white || !s.textures.contains(previous) || previous->generated)
        throw std::invalid_argument("Cannot host-replace a renderer-owned, foreign or generated GPU texture");
    const bool recordedInThisFrame=s.frame.valid && previous->lastDraw==s.frameSerial;
    if (!recordedInThisFrame && previous->format==VK_FORMAT_B8G8R8A8_UNORM &&
        previous->width==width && previous->height==height && previous->opaque==opaque) {
        s.QueueTextureUpload(previous,bgra,pitch);
        return previous;
    }
    auto* next=CreateTexture(width,height,bgra,pitch,opaque);
    DestroyTexture(previous);
    return next;
}

VulkanTexture* VulkanRenderer::CreateCoverageMask(int width,int height,
    std::span<const uint8_t> coverage,int pitch) {
    if(width<=0 || height<=0 || pitch<width ||
        coverage.size()<uint64_t(height-1)*uint64_t(pitch)+uint64_t(width))
        throw std::invalid_argument("Invalid R8 coverage mask source");
    auto& s=*impl_;
    auto texture=s.Image(width,height,VK_IMAGE_USAGE_TRANSFER_DST_BIT,false,VK_FORMAT_R8_UNORM);
    auto* handle=texture.get();
    const auto position=s.textures.emplace(handle,std::move(texture)).first;
    try {s.QueueTextureUpload(handle,coverage.data(),pitch);}
    catch (...) {s.textures.erase(position);throw;}
    return handle;
}

void VulkanRenderer::DestroyTexture(VulkanTexture* texture) {
    if (!texture) return;
    auto& s = *impl_;
    if (texture == s.white) throw std::invalid_argument("Cannot destroy renderer white texture");
    if (texture == s.renderImage)
        throw std::logic_error("Select another render target before destroying the selected GPU image");
    const auto found = s.textures.find(texture);
    if (found == s.textures.end()) throw std::invalid_argument("Texture does not belong to this renderer");
    // unique_ptr's noexcept move keeps push_back's strong guarantee: failed
    // growth leaves live ownership/tokens intact, with amortized capacity growth.
    s.Retire(std::move(found->second));
    s.textures.erase(found);
    // 池水焦散目标被销毁（重开关卡）时先解关联，描述集等换代纹理改写。
    if (texture == s.caustics.target) {
        s.caustics.target = nullptr;
        s.caustics.frameQueued = false;
    }
    // The current command buffer may still reference this texture even before
    // queue submission. ReapRetired preserves it until the frame fence wait.
}

VulkanTexture* VulkanRenderer::CreateCausticsTexture(int width, int height, VulkanTexture* source) {
    if (width <= 0 || height <= 0 || source==nullptr || source->width!=256 || source->height!=256)
        throw std::invalid_argument("Invalid caustics texture source");
    auto& s = *impl_;
    if (!s.textures.contains(source)) throw std::invalid_argument("Caustics source belongs to another renderer");
    if (!s.caustics.shader) throw std::logic_error("Caustics compute shader not loaded");
    // STORAGE 用途 + 常驻 GENERAL：compute 直写与场景采样共用同一布局。
    // RGBA8 而非 BGRA8：焦散 shader 是 GLSL（HLSL 前端无法标注 storage
    // image 格式），rgba8 限定符要求视图同为 RGBA8；写入值 RGB 全等、仅
    // 字节序不同，采样结果与 BGRA8 版逐位一致。
    auto texture = s.Image(width, height,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_STORAGE_BIT, false, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_LAYOUT_GENERAL);
    texture->opaque = false;
    auto* handle = texture.get();
    const auto position = s.textures.emplace(handle, std::move(texture)).first;
    try {
        s.FlushTextureUploads();
        auto command=s.BeginTransfer();
        Barrier(command,handle->image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT);
        const VkClearColorValue clear{{1,1,1,0}};
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdClearColorImage(command,handle->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&clear,1,&range);
        Barrier(command,handle->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT|VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_SHADER_READ_BIT);
        s.FinishTransfer();
        handle->layout=VK_IMAGE_LAYOUT_GENERAL;
        s.InitCaustics(handle,source);
    }
    catch (...) { s.textures.erase(position); throw; }
    return handle;
}

void VulkanRenderer::SetCausticsTimeNs(unsigned long long timeNs) {
    auto& s = *impl_;
    s.caustics.timeNs = timeNs;
    s.caustics.frameQueued = s.caustics.target != nullptr;
}

void VulkanRenderer::UseCausticsTexture(VulkanTexture* output,VulkanTexture* source) {
    auto& s=*impl_;
    if(s.frame.valid || !s.textures.contains(output) || !s.textures.contains(source) ||
        output->sampledLayout!=VK_IMAGE_LAYOUT_GENERAL || source->width!=256 || source->height!=256)
        throw std::logic_error("Invalid GPU caustics selection");
    s.window.WaitForFrame();
    s.InitCaustics(output,source);
}

bool VulkanRenderer::HasCausticsCompute() const { return impl_->caustics.shader != VK_NULL_HANDLE; }

bool VulkanRenderer::BeginScene() try {
    auto& s = *impl_;
    if (s.recordingFailed) throw std::logic_error("Cannot continue a failed Vulkan recording");
    if (s.frame.valid) throw std::logic_error("Scene already open");
    // 新帧命令会引用累积批里的上传/合成结果：先提交开放传输缓冲，保持
    // 队列提交序（传输批 → 帧批）。
    if (s.transferOpen) s.FinishTransfer();
    s.CompleteTransfers();  // 回收上批合成描述集；栅栏多半已 signaled，代价≈0
    if (s.renderImage
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
        || true // This isolated executable always renders offscreen.
#endif
    ) {
        // No swapchain acquire, surface queries or presentation semaphores.
        // The prior main-frame fence protects all shared vertices/descriptors.
        s.window.WaitForFrame();
        s.FlushTextureUploads();
        VK_REQUIRE(vkResetCommandPool(s.device, s.imageCommandPool, 0));
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_REQUIRE(vkBeginCommandBuffer(s.imageCommand, &begin));
        auto* batchTarget=s.renderImage ? s.renderImage : s.scene.get();
        s.frame = {s.imageCommand, 0, {uint32_t(batchTarget->width), uint32_t(batchTarget->height)}, true};
        s.imageOnly = true;
    } else {
        s.frame = s.window.BeginCommands();
        if (!s.frame.valid) return false;
        s.imageOnly = false;
        s.FlushTextureUploads();
        s.RefreshPresentPipeline();
    }
    ++s.frameSerial;
    // Complete uploads before reclaiming retired textures: a texture may have
    // been queued and destroyed outside a frame, including while minimized.
    s.drawState = {}; // Vulkan command buffers do not inherit last frame's state.
    s.ReapRetired();
    if (auto* stream = s.drawStreams[s.frame.resourceIndex]) { stream->vertices.used = 0; stream->indices.used = 0; }
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    benchmarkDrawCalls = 0;
    benchmarkTriangles = 0;
    if (benchmarkQueries) {
        vkCmdResetQueryPool(s.frame.cmd,benchmarkQueries,0,2);
        vkCmdWriteTimestamp(s.frame.cmd,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,benchmarkQueries,0);
    }
#endif
    s.RecordCaustics(s.frame.cmd);
    s.BeginTargetPass();
    return true;
} catch (...) {
    auto& s = *impl_;
    s.window.AbortFrame();
    s.recordingFailed = true;
    s.frame = {};
    s.targetPassOpen = false;
    s.pendingRun = {};
    s.drawState = {};
    throw;
}

template<bool Quads>
void VulkanRenderer::Impl::Draw(std::span<const VulkanVertex> vertices, VulkanTexture* texture,
    VkPipeline pipeline, const VulkanClip* clip, int vw, int vh
) {
    if (!frame.valid) throw std::logic_error("Draw outside a frame");
    if (!texture) throw std::invalid_argument("Missing draw texture");
    if (vertices.empty()) { return; }
    if (vertices.size() % (Quads ? 4 : 3) || vertices.size() > UINT32_MAX)
        throw std::invalid_argument("Expected a triangle list");
#if defined(PVZ_VULKAN_LEGACY)
    // Preserve painter order. Only adjacent primitives with the same image
    // and sampler can share a Vulkan 1.0 draw; no nonuniform indexing feature.
    constexpr size_t primitiveSize=Quads?4:3;
    size_t first=0;
    while (first<vertices.size()) {
        const auto slot=vertices[first].slot & kSlotMask;
        const auto sampler=vertices[first].sampler;
        size_t end=first;
        do {
            for(size_t v=0;v<primitiveSize;++v)
                if ((vertices[end+v].slot & kSlotMask)!=slot || vertices[end+v].sampler!=sampler)
                    throw std::invalid_argument("A primitive cannot sample different image bindings");
            end+=primitiveSize;
        } while(end<vertices.size() && (vertices[end].slot & kSlotMask)==slot && vertices[end].sampler==sampler);
        if (first==0 && end==vertices.size()) break;
        Draw<Quads>(vertices.subspan(first,end-first),slotOwners[slot],pipeline,clip,vw,vh);
        first=end;
    }
    if (first!=0) return;
#endif
    int64_t left = 0, top = 0, right = vw, bottom = vh;
    if (clip) {
        left = std::max<int64_t>(0, clip->x);
        top = std::max<int64_t>(0, clip->y);
        right = std::min<int64_t>(vw, int64_t(clip->x) + clip->width);
        bottom = std::min<int64_t>(vh, int64_t(clip->y) + clip->height);
    }
    if (right <= left || bottom <= top) { return; }
    // 提交热路径单遍化（2026-10-11 回归定罪）：此前 校验/lastDraw 盖章/
    // memcpy/slot 解析 共四遍逐顶点循环，SubmitImplDraw 1.26→3.46µs/次。
    // 反馈检测按次——两个提交入口（SubmitTriangles/SubmitCoverageTriangles）
    // 每次调用只携带一个纹理，其顶点 slot 下标恒等于该纹理槽位。
    if (texture == renderImage) {
        // Feedback reads observe the complete pre-draw image, never an image
        // simultaneously bound as this pass's color attachment.
        EndTargetPass();
        SnapshotRenderImage(renderImage, true);
        BeginTargetPass();
    }
    VkDeviceSize bytes = VkDeviceSize(vertices.size())*vertexStride;
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    if (instancedExperiment) {
        if constexpr (!Quads) throw std::invalid_argument("Instance experiment requires explicit quads");
        bytes/=4;
    }
#endif
    auto*& stream = drawStreams[frame.resourceIndex];
    if (!stream) stream = new DrawStream;
    if (bytes > stream->vertices.size - stream->vertices.used)
        throw std::overflow_error("Vulkan frame vertex capacity exceeded (65536 vertices)");
    auto offset = stream->vertices.used;
    auto* copiedVertices = reinterpret_cast<VulkanVertex*>(static_cast<std::byte*>(stream->vertices.mapped) + offset);
    constexpr uint32_t kVertexSlotValid = kSlotMask | kGeneratedPremult |
        VulkanVertex::kIndependentColorFilter | VulkanVertex::kColorFilterMask |
        VulkanVertex::kAlphaEdgeFilter | VulkanVertex::kOpaqueInput;
    constexpr uint32_t kVertexSlotKeep = VulkanVertex::kIndependentColorFilter |
        VulkanVertex::kColorFilterMask | VulkanVertex::kAlphaEdgeFilter;
    const VulkanTexture* lastSource = nullptr;
    uint32_t ownerSlotBits = 0;
    uint32_t sourceFeatures = 0;
    const uint32_t primitiveFlags = mixedSpritePipeline && pipeline == pipelines[1] ? kMixedAdditive : 0u;
    for (size_t i = 0; i < vertices.size(); ++i) {
        const VulkanVertex& vertex = vertices[i];
        if ((vertex.slot & ~kVertexSlotValid) != 0 || vertex.sampler >= samplers.size())
            throw std::invalid_argument("Invalid encoded texture slot or sampler");
        auto* source = slotOwners[vertex.slot & kSlotMask];
        if (!source) throw std::invalid_argument("Draw refers to an unallocated texture slot");
        if (source->lastDraw != frameSerial) source->lastDraw = frameSerial;
        if (source != lastSource) {
            lastSource = source;
            ownerSlotBits = VulkanRenderer::SlotOf(source);
        }
        const auto slot=ownerSlotBits | (vertex.slot & kVertexSlotKeep) | primitiveFlags;
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
        if (instancedExperiment) {
            auto& sprite=reinterpret_cast<SpriteInstance*>(copiedVertices)[i/4];
            const auto& first=vertices[i-i%4];
            if (vertex.r!=first.r || vertex.g!=first.g || vertex.b!=first.b || vertex.a!=first.a ||
                vertex.slot!=first.slot || vertex.sampler!=first.sampler)
                throw std::invalid_argument("Instance experiment requires uniform quad color and sampling");
            sprite.positions[i%4*2]=vertex.x;sprite.positions[i%4*2+1]=vertex.y;
            sprite.uvs[i%4*2]=vertex.u;sprite.uvs[i%4*2+1]=vertex.v;
            if (i%4==0) {
                sprite.color[0]=vertex.r;sprite.color[1]=vertex.g;
                sprite.color[2]=vertex.b;sprite.color[3]=vertex.a;
                sprite.slot=slot;sprite.sampler=vertex.sampler;
            }
        } else if (packedColorExperiment) {
            auto* packed=reinterpret_cast<PackedColorVertex*>(copiedVertices);
            auto* packed16=reinterpret_cast<PackedColor16Vertex*>(copiedVertices);
            if (packedColor16Experiment) packed16[i]={vertex.x,vertex.y,{},vertex.u,vertex.v,slot,vertex.sampler};
            else packed[i]={vertex.x,vertex.y,{},vertex.u,vertex.v,slot,vertex.sampler};
            const float colors[]{vertex.r,vertex.g,vertex.b,vertex.a};
            for(unsigned int c=0;c<4;++c) {
                if (!(colors[c]>=0 && colors[c]<=1)) throw std::invalid_argument("Packed color outside UNORM range");
                if (packedColor16Experiment) packed16[i].color[c]=static_cast<unsigned short>(colors[c]*65535+0.5f);
                else packed[i].color[c]=static_cast<unsigned char>(colors[c]*255+0.5f);
            }
        } else
#endif
        { copiedVertices[i]=vertex;copiedVertices[i].slot=slot; }
        sourceFeatures |= slot;
    }
    stream->vertices.used += bytes;
    // Classify the whole submitted span; never route mixed/filter/font draws
    // through the straight sprite shader. Keep adjacent eligible spans batched.
    uint32_t simpleFeatures = kSlotMask | VulkanVertex::kOpaqueInput | kMixedAdditive;
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    if (manualFilterExperiment && (sourceFeatures & VulkanVertex::kColorFilterMask) &&
        (sourceFeatures & ~(simpleFeatures | VulkanVertex::kColorFilterMask))==0)
        for (unsigned int i=0;i<3;++i)
            if (pipeline==pipelines[i]) {pipeline=generatedExperimentPipelines[i];break;}
    if (simpleGeneratedExperiment && (sourceFeatures & kGeneratedPremult) &&
        (sourceFeatures & ~(simpleFeatures | kGeneratedPremult)) == 0)
        for (unsigned int i=0;i<3;++i)
            if (pipeline == pipelines[i]) { pipeline=generatedExperimentPipelines[i]; break; }
#endif
    if ((sourceFeatures & ~simpleFeatures) == 0) {
        for (unsigned int i = 0; i < pipelines.size(); ++i)
            if (pipeline == pipelines[i]) {
                pipeline = mixedSpritePipeline && i < 2 ? mixedSpritePipeline : spritePipelines[i];
                break;
            }
    }
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    if (solidFillExperiment && texture==white && pipeline==mixedSpritePipeline &&
        (sourceFeatures & kMixedAdditive)==0)
        pipeline=solidExperimentPipeline;
#endif
    VkRect2D scissor{{static_cast<int32_t>(left), static_cast<int32_t>(top)},
        {static_cast<uint32_t>(right-left), static_cast<uint32_t>(bottom-top)}};
    AppendRun<Quads>(stream,offset,static_cast<uint32_t>(vertices.size()),pipeline,scissor,vw,vh
#if defined(PVZ_VULKAN_LEGACY)
        , slotOwners[copiedVertices[0].slot & kSlotMask]->imageSet,samplerSets[copiedVertices[0].sampler]
#endif
    );
}

template<bool Quads>
void VulkanRenderer::Impl::AppendRun(DrawStream* stream, VkDeviceSize offset, uint32_t count,
    VkPipeline pipeline, VkRect2D scissor, int vw, int vh
#if defined(PVZ_VULKAN_LEGACY)
    , VkDescriptorSet imageSet, VkDescriptorSet selectedSampler
#endif
) {
    auto cmd = frame.cmd;
    // P5 游程：状态变化检测前置（先判后绑）。bindless+采样器数组后描述符
    // 对恒为 {纹理数组, 采样器数组}——首绘绑定一次，帧内不再变化；断点
    // 只剩 管线/剪刀/视口
#if defined(PVZ_VULKAN_LEGACY)
    const std::array sets{imageSet,selectedSampler};
#else
    const std::array sets{textureArray,samplerSet};
#endif
    const bool aVpChange = drawState.width!=vw || drawState.height!=vh;
    const bool aScChange = !drawState.hasScissor || drawState.scissor.offset.x!=scissor.offset.x ||
        drawState.scissor.offset.y!=scissor.offset.y || drawState.scissor.extent.width!=scissor.extent.width ||
        drawState.scissor.extent.height!=scissor.extent.height;
    const bool aPlChange = drawState.pipeline!=pipeline;
    const bool aDsChange = drawState.descriptors!=sets;
    if (pendingRun.count != 0 && (aVpChange || aScChange || aPlChange || aDsChange))
        FlushPendingRun(cmd);
    if (aVpChange) {
        const VkViewport viewport{0,0,static_cast<float>(vw),static_cast<float>(vh),0,1};
        vkCmdSetViewport(cmd,0,1,&viewport);
        const float extent[]={static_cast<float>(vw),static_cast<float>(vh)};
        vkCmdPushConstants(cmd,
            pipelineLayout,
            hdr ? VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT : VK_SHADER_STAGE_VERTEX_BIT,
            0,sizeof(extent),extent);
        drawState.width=vw;drawState.height=vh;
    }
    if (aScChange) {
        vkCmdSetScissor(cmd,0,1,&scissor);
        drawState.scissor=scissor;drawState.hasScissor=true;
    }
    if (aPlChange) {
        vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipeline);
        drawState.pipeline=pipeline;
    }
    if (aDsChange) {
        vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelineLayout,0,2,sets.data(),0,nullptr);
        drawState.descriptors=sets;
    }
    // All ordinary geometry uses one uint16 index stream. Mixed triangles and
    // quads keep their paint order in one run; no topology-change draw break.
    constexpr unsigned int primitiveVertices=Quads ? 4u : 3u;
    while (count) {
        if (!pendingRun.count)
            pendingRun={stream,offset,stream->indices.used,0,0,Quads ? RunGeometry::Quads : RunGeometry::Triangles};
        unsigned int limit=65536;
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
        if (wideIndexExperiment) {
            if constexpr (!Quads) throw std::logic_error("32-bit comparison accepts explicit quads only");
            limit=262144;
        }
#endif
        unsigned int chunk=std::min(count,limit-pendingRun.count);
        chunk-=chunk%primitiveVertices;
        if (!chunk) { FlushPendingRun(cmd); continue; }
        const auto indexCount=Quads ? chunk/4*6 : chunk;
        constexpr auto geometry=Quads ? RunGeometry::Quads : RunGeometry::Triangles;
        if (pendingRun.geometry!=geometry && pendingRun.geometry!=RunGeometry::Mixed) {
            // Materialize the already queued pure prefix only when topology
            // actually mixes. Pure runs keep using the immutable GPU table.
            const auto prefixBytes=VkDeviceSize(pendingRun.indexCount)*sizeof(unsigned short);
            if (prefixBytes>stream->indices.size-stream->indices.used)
                throw std::logic_error("Index prefix capacity exceeded");
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
            if (!generateIndexPrefixExperiment) {
                const auto* prefix=static_cast<const unsigned short*>(staticIndices.mapped)+
                    (pendingRun.geometry==RunGeometry::Quads ? 65536 : 0);
                std::memcpy(static_cast<std::byte*>(stream->indices.mapped)+stream->indices.used,prefix,size_t(prefixBytes));
            } else
#endif
            {
                // Mapped upload storage is write-only in the frame hot path.
                // Generate this tiny pattern rather than reading the GPU table.
                auto* out=reinterpret_cast<unsigned short*>(static_cast<std::byte*>(stream->indices.mapped)+stream->indices.used);
                if (pendingRun.geometry==RunGeometry::Triangles) {
                    for(unsigned int i=0;i<pendingRun.count;++i) out[i]=static_cast<unsigned short>(i);
                } else {
                    for(unsigned int i=0;i<pendingRun.count;i+=4) {
                        const unsigned short a=static_cast<unsigned short>(i);
                        const unsigned short quad[]{a,static_cast<unsigned short>(a+1),static_cast<unsigned short>(a+2),
                            static_cast<unsigned short>(a+1),static_cast<unsigned short>(a+2),static_cast<unsigned short>(a+3)};
                        std::memcpy(out,quad,sizeof(quad));out+=6;
                    }
                }
            }
            stream->indices.used+=prefixBytes;
            pendingRun.geometry=RunGeometry::Mixed;
        }
        if (pendingRun.geometry==RunGeometry::Mixed) {
            const auto indexBytes=VkDeviceSize(indexCount)*sizeof(unsigned short);
            if (indexBytes>stream->indices.size-stream->indices.used)
                throw std::logic_error("Vertex/index stream capacities disagree");
            auto* indices=reinterpret_cast<unsigned short*>(
                static_cast<std::byte*>(stream->indices.mapped)+stream->indices.used);
            const auto base=pendingRun.count;
            if constexpr (Quads) {
                for (unsigned int i=0;i<chunk;i+=4) {
                    const unsigned short a=static_cast<unsigned short>(base+i);
                    // Input is a four-corner strip, matching the original frontend.
                    const unsigned short quad[]{a,static_cast<unsigned short>(a+1),static_cast<unsigned short>(a+2),
                        static_cast<unsigned short>(a+1),static_cast<unsigned short>(a+2),static_cast<unsigned short>(a+3)};
                    std::memcpy(indices,quad,sizeof(quad)); indices+=6;
                }
            } else {
                for (unsigned int i=0;i<chunk;++i) indices[i]=static_cast<unsigned short>(base+i);
            }
            stream->indices.used+=indexBytes;
        }
        pendingRun.count+=chunk; pendingRun.indexCount+=indexCount;
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
        offset+=VkDeviceSize(instancedExperiment ? chunk/4 : chunk)*vertexStride;
#else
        offset+=VkDeviceSize(chunk)*vertexStride;
#endif
        count-=chunk;
    }
}

void VulkanRenderer::Impl::FlushPendingRun(VkCommandBuffer cmd) {
    if (pendingRun.count == 0) return;
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    ++VulkanRenderer::benchmarkDrawCalls;
    VulkanRenderer::benchmarkTriangles += pendingRun.indexCount / 3;
#endif
    // 游程顶点总在流缓冲内连续追加：整个缓冲绑定一次，游程用 firstVertex
    // 偏移取址（绑 22 次/帧纯属结构浪费）；仅换流缓冲时重绑
    if (drawState.vertexBuffer != pendingRun.stream->vertices.buffer) {
        const VkDeviceSize zero = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &pendingRun.stream->vertices.buffer, &zero);
        drawState.vertexBuffer = pendingRun.stream->vertices.buffer;
    }
    auto indexBuffer=pendingRun.geometry==RunGeometry::Mixed ? pendingRun.stream->indices.buffer : staticIndices.buffer;
    auto firstIndex=pendingRun.geometry==RunGeometry::Mixed ?
        static_cast<uint32_t>(pendingRun.indexOffset/sizeof(unsigned short)) :
        pendingRun.geometry==RunGeometry::Quads ? 65536u : 0u;
    auto indexType=VK_INDEX_TYPE_UINT16;
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    if (wideIndexExperiment) {
        if (pendingRun.geometry!=RunGeometry::Quads) throw std::logic_error("Mixed 32-bit fixture unsupported");
        indexBuffer=wideQuadIndices.buffer;firstIndex=0;indexType=VK_INDEX_TYPE_UINT32;
    }
#endif
    if (drawState.indexBuffer!=indexBuffer) {
        vkCmdBindIndexBuffer(cmd,indexBuffer,0,indexType);
        drawState.indexBuffer=indexBuffer;
    }
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    if (instancedExperiment) {
        if (pendingRun.geometry!=RunGeometry::Quads) throw std::logic_error("Instance experiment expects pure quads");
        vkCmdDrawIndexed(cmd,6,pendingRun.count/4,65536,0,
            static_cast<unsigned int>(pendingRun.offset/sizeof(SpriteInstance)));
    } else if (nonIndexedExperiment) {
        vkCmdDraw(cmd,pendingRun.count,1,static_cast<uint32_t>(pendingRun.offset/vertexStride),0);
    } else
#endif
        vkCmdDrawIndexed(cmd,pendingRun.indexCount,1,firstIndex,
            static_cast<int32_t>(pendingRun.offset/vertexStride),0);
    pendingRun.count = 0;
}


// 管线缓存落盘与 resident 无关（桌面为空操作），置于 UNIFIED 块外编译。
void VulkanRenderer::Impl::SavePipelineCache() {
#if !defined(__ANDROID__)
    (void)0;  // 桌面暂不落盘缓存（VK_NULL_HANDLE 对创建调用天然合法）
#else
    if (pipelineCache == VK_NULL_HANDLE) return;
    std::size_t size = 0;
    if (vkGetPipelineCacheData(device, pipelineCache, &size, nullptr) != VK_SUCCESS || size == 0)
        return;
    std::vector<std::byte> data(size);
    if (vkGetPipelineCacheData(device, pipelineCache, &size, data.data()) != VK_SUCCESS)
        return;
    std::ofstream out(GetExecutableDirectory()+kPipelineCacheName,
        std::ios::binary | std::ios::trunc);
    if (out) out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(size));
#endif
}

void VulkanRenderer::SavePipelineCache() { impl_->SavePipelineCache(); }

void VulkanRenderer::SubmitTriangles(std::span<const VulkanVertex> vertices, VulkanTexture* texture,
    bool linear, bool wrap, VulkanBlend blend, const VulkanClip* clip) {
    (void)linear; (void)wrap;  // 采样器随顶点携带（RenderInterface 填充），不再是绑定状态
    auto& s = *impl_;
    auto* output = s.renderImage ? s.renderImage : s.scene.get();
    const auto& pipelines = s.renderImage ? s.imagePipelines : s.pipelines;
    s.Draw(vertices, texture, pipelines.at(static_cast<size_t>(blend)), clip, output->width, output->height
    );
}

void VulkanRenderer::SubmitQuads(std::span<const VulkanVertex> vertices,VulkanTexture* texture,
    bool linear,bool wrap,VulkanBlend blend,const VulkanClip* clip) {
    (void)linear; (void)wrap;
    auto& s=*impl_;
    s.Draw<true>(vertices,texture,(s.renderImage ? s.imagePipelines : s.pipelines).at(size_t(blend)),
        clip,(s.renderImage ? s.renderImage->width : s.width),(s.renderImage ? s.renderImage->height : s.height));
}

#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
void VulkanRenderer::SubmitBenchmarkSprite(const VulkanSpriteInstance& sprite,VulkanTexture* texture,
    VulkanBlend blend,const VulkanClip* clip) {
    auto& s=*impl_;
    if (!s.instancedExperiment || !s.frame.valid || s.renderImage)
        throw std::logic_error("Direct instance experiment requires an open main scene");
    constexpr auto valid=Impl::kSlotMask | Impl::kGeneratedPremult | VulkanVertex::kIndependentColorFilter |
        VulkanVertex::kColorFilterMask | VulkanVertex::kAlphaEdgeFilter | VulkanVertex::kOpaqueInput;
    constexpr auto keep=VulkanVertex::kIndependentColorFilter | VulkanVertex::kColorFilterMask |
        VulkanVertex::kAlphaEdgeFilter;
    if (!texture || (sprite.slot & ~valid) || sprite.sampler>=s.samplers.size() ||
        s.slotOwners[sprite.slot & Impl::kSlotMask]!=texture)
        throw std::invalid_argument("Invalid direct instance texture or sampling metadata");
    int64_t left=0,top=0,right=s.width,bottom=s.height;
    if (clip) {
        left=std::max<int64_t>(0,clip->x);top=std::max<int64_t>(0,clip->y);
        right=std::min<int64_t>(right,int64_t(clip->x)+clip->width);
        bottom=std::min<int64_t>(bottom,int64_t(clip->y)+clip->height);
    }
    if (right<=left || bottom<=top) return;
    auto*& stream=s.drawStreams[s.frame.resourceIndex];
    if (!stream) stream=new DrawStream;
    if (sizeof(sprite)>stream->vertices.size-stream->vertices.used)
        throw std::overflow_error("Vulkan frame vertex capacity exceeded (65536 vertices)");
    texture->lastDraw=s.frameSerial;
    const auto offset=stream->vertices.used;
    auto* stored=reinterpret_cast<VulkanSpriteInstance*>(static_cast<std::byte*>(stream->vertices.mapped)+offset);
    *stored=sprite;
    const auto encodedSlot=SlotOf(texture) | (sprite.slot & keep) |
        (s.mixedSpritePipeline && blend==VulkanBlend::Additive ? Impl::kMixedAdditive : 0u);
    stored->slot=encodedSlot;
    stream->vertices.used+=sizeof(sprite);
    const auto index=static_cast<size_t>(blend);
    auto pipeline=s.pipelines.at(index);
    constexpr auto simple=Impl::kSlotMask | VulkanVertex::kOpaqueInput | Impl::kMixedAdditive;
    if ((encodedSlot & ~simple)==0)
        pipeline=s.mixedSpritePipeline && index<2 ? s.mixedSpritePipeline : s.spritePipelines[index];
    const VkRect2D scissor{{static_cast<int>(left),static_cast<int>(top)},
        {static_cast<unsigned int>(right-left),static_cast<unsigned int>(bottom-top)}};
    s.AppendRun<true>(stream,offset,4,pipeline,scissor,s.width,s.height);
}
#endif

void VulkanRenderer::SubmitPool(VulkanTexture* texture,const GpuPoolParams& p) {
    auto& s=*impl_;
    if (!s.frame.valid) throw std::logic_error("Pool drawing requires an open scene");
    if (!s.poolIndexBuffer) throw std::logic_error("Prepare pool pipelines while loading");
    if (!texture || !s.textures.contains(texture) || p.layer>=3 || p.sampler>=4)
        throw std::invalid_argument("Invalid pool texture, layer or sampler");
    auto* output=s.renderImage ? s.renderImage : s.scene.get();
    const auto left=std::max<int64_t>(0,p.clip.x);
    const auto top=std::max<int64_t>(0,p.clip.y);
    const auto right=std::min<int64_t>(output->width,int64_t(p.clip.x)+p.clip.width);
    const auto bottom=std::min<int64_t>(output->height,int64_t(p.clip.y)+p.clip.height);
    if (right<=left || bottom<=top) return;
    if (texture==s.renderImage) {
        const uint32_t sourceSlot=texture->slot;
        s.EndTargetPass();
        s.SnapshotRenderImage(s.renderImage,true);
        s.BeginTargetPass();
        texture=s.slotOwners[sourceSlot];
    }
    output=s.renderImage ? s.renderImage : s.scene.get();
    s.FlushPendingRun(s.frame.cmd);
    texture->lastDraw=s.frameSerial;
    const auto& pipelines=s.renderImage ? s.poolImagePipelines : s.poolPipelines;
    const VkViewport viewport{0,0,float(output->width),float(output->height),0,1};
    const VkRect2D scissor{{int32_t(left),int32_t(top)},
        {uint32_t(right-left),uint32_t(bottom-top)}};
    vkCmdSetViewport(s.frame.cmd,0,1,&viewport);
    vkCmdSetScissor(s.frame.cmd,0,1,&scissor);
    vkCmdBindPipeline(s.frame.cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,pipelines.at(size_t(p.blend)));
#if defined(PVZ_VULKAN_LEGACY)
    const VkDescriptorSet sets[]{texture->imageSet,s.samplerSets[p.sampler]};
#else
    const VkDescriptorSet sets[]{s.textureArray,s.samplerSet};
#endif
    vkCmdBindDescriptorSets(s.frame.cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,s.poolLayout,0,2,sets,0,nullptr);
    std::array<PoolMeshVertex,96> vertices;
    BuildPoolMesh(p,vertices);
    // Append each draw's mesh to the fixed frame buffer. Never overwrite a
    // previous layer/credits draw while its command buffer still references it.
    static_assert(sizeof(vertices)%sizeof(VulkanVertex)==0);
    auto*& stream = s.drawStreams[s.frame.resourceIndex];
    if (!stream) stream = new DrawStream;
    if (sizeof(vertices) > stream->vertices.size - stream->vertices.used)
        throw std::overflow_error("Vulkan frame vertex capacity exceeded (65536 vertices)");
    const VkDeviceSize offset = stream->vertices.used;
    std::memcpy(static_cast<std::byte*>(stream->vertices.mapped)+offset,vertices.data(),sizeof(vertices));
    stream->vertices.used += sizeof(vertices);
    vkCmdBindVertexBuffers(s.frame.cmd,0,1,&stream->vertices.buffer,&offset);
    vkCmdBindIndexBuffer(s.frame.cmd,s.poolIndexBuffer,0,VK_INDEX_TYPE_UINT16);
    struct PoolPush {
        float extent[2];
        unsigned int slot, sampler;
    };
    static_assert(sizeof(PoolPush)==16);
    const PoolPush push{{float(output->width),float(output->height)},SlotOf(texture)|p.samplingFlags,p.sampler};
    vkCmdPushConstants(s.frame.cmd,s.poolLayout,VK_SHADER_STAGE_VERTEX_BIT,0,sizeof(push),&push);
    vkCmdDrawIndexed(s.frame.cmd,uint32_t(GpuPoolIndices.size()),1,0,0,0);
    // A dedicated layout/vertex format was bound; ordinary draws restore their
    // own state without adding a branch to their vertex processing.
    s.drawState={};
}

void VulkanRenderer::SubmitCoverageTriangles(std::span<const VulkanVertex> vertices,VulkanTexture* mask,
    VulkanBlend blend,const VulkanClip* clip) {
    auto& s=*impl_;
    if(!mask || !s.textures.contains(mask) || mask->format!=VK_FORMAT_R8_UNORM)
        throw std::invalid_argument("Coverage drawing requires a live R8 mask");
    auto* output = s.renderImage ? s.renderImage : s.scene.get();
    const auto& pipelines = s.renderImage ? s.imageCoveragePipelines : s.coveragePipelines;
    s.Draw(vertices,mask,pipelines.at(static_cast<size_t>(blend)),clip,output->width,output->height
    );
}

void VulkanRenderer::EndSceneAndPresent() try {
    auto& s = *impl_;
    if (!s.frame.valid) throw std::logic_error("No open scene");
    // 帧批（含 present）引用的合成/上传结果必须先提交（队列同序）。
    if (s.transferOpen) s.FinishTransfer();
    if (s.imageOnly) {
        s.FinishImageBatch();
        return;
    }
    s.EndTargetPass();
    auto* scale = s.ActiveScale();
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    benchmarkSceneDraws = benchmarkDrawCalls;
    benchmarkSceneTriangles = benchmarkTriangles;
    if (benchmarkQueries)
        vkCmdWriteTimestamp(s.frame.cmd,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,benchmarkQueries,1);
#endif
    VulkanTexture* presentImage = s.scene.get();
    if (scale) {
        if (!scale->warm) throw std::logic_error("Activating a scale chain before GPU warm-up");
        scale->pipeline->Record(s.frame.cmd,s.scene->layout,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT);
        presentImage = scale->sampledOutput.get();
    }
    s.scene->layout = s.scene->sampledLayout;
    s.window.BeginPresentPass({{0, 0, 0, 1}});
    // Scale records through a different owner and BeginPresentPass sets the
    // window viewport/scissor (and optionally a graphics pipeline). Re-establish
    // all state before presenting, including when surface == logical canvas.
    s.drawState = {};
    const auto rect = PresentRect(int(s.frame.extent.width),int(s.frame.extent.height));
    std::array<VkClearRect,4> borders{};
    unsigned int borderCount=0;
    for(const auto& border:PresentBorderRects(rect,int(s.frame.extent.width),int(s.frame.extent.height)))
        if(border.width>0 && border.height>0)
            borders[borderCount++]={{{border.x,border.y},
                {static_cast<unsigned int>(border.width),static_cast<unsigned int>(border.height)}},0,1};
    if(borderCount) {
        VkClearAttachment black{};
        black.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
        black.clearValue.color={{0,0,0,1}};
        vkCmdClearAttachments(s.frame.cmd,1,&black,borderCount,borders.data());
    }
    const float w = float(rect.width), h = float(rect.height);
    const float x = float(rect.x), y = float(rect.y);
    const VulkanVertex vertices[] = {
        {x,y,1,1,1,1,0,0,presentImage->slot,1}, {x+w,y,1,1,1,1,1,0,presentImage->slot,1}, {x+w,y+h,1,1,1,1,1,1,presentImage->slot,1},
        {x,y,1,1,1,1,0,0,presentImage->slot,1}, {x+w,y+h,1,1,1,1,1,1,presentImage->slot,1}, {x,y+h,1,1,1,1,0,1,presentImage->slot,1}};
    if (s.hdr) {
        const float push[]={float(s.frame.extent.width),float(s.frame.extent.height),0,0,
            s.hdr->paperWhiteNits,s.hdr->peakNits,s.hdr->reinhardPaperNits,s.hdr->strength};
        vkCmdPushConstants(s.frame.cmd,s.pipelineLayout,VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,sizeof(push),push);
    }
    s.Draw(vertices, presentImage, s.presentPipeline, nullptr, s.frame.extent.width, s.frame.extent.height
    );
    s.FlushPendingRun(s.frame.cmd);  // P5：present 四边形入令后冲刷
    s.FlushTextureUploads(); // Includes textures first encountered during this scene.
    s.window.EndFrame();
    s.frame = {};
    s.sceneInitialized = true;
} catch (...) {
    auto& s = *impl_;
    s.window.AbortFrame();
    s.recordingFailed = true;
    s.frame = {};
    s.targetPassOpen = false;
    s.pendingRun = {};
    s.drawState = {};
    throw;
}

bool VulkanRenderer::PresentRetainedScene() {
    if (impl_->frame.valid || impl_->renderImage)
        throw std::logic_error("Retained presentation requires the main scene selected and no open batch");
    if (!impl_->sceneInitialized) return false;
    if (!BeginScene()) return false;
    EndSceneAndPresent();
    return true;
}
bool VulkanRenderer::ResizePresent() {
    auto& s = *impl_;
    if (s.frame.valid) throw std::logic_error("Resize during game drawing");
    if (!s.window.RecreateSwapchain()) return false;
    s.RefreshPresentPipeline();
    return true;
}
void VulkanRenderer::ResizeScene(int width, int height) {
    auto& s = *impl_;
    if (s.frame.valid || s.renderImage)
        throw std::logic_error("Resize the main Vulkan canvas between scenes");
    if (s.width == width && s.height == height) return;
    if (width <= 0 || height <= 0 ||
        unsigned(width) > MaxImageDimension2D() || unsigned(height) > MaxImageDimension2D())
        throw std::invalid_argument("Invalid Vulkan canvas size");
    s.FlushTextureUploads();
    if (s.transferOpen) s.FinishTransfer();
    s.CompleteTransfers();
    s.window.WaitForFrame();
    auto next = s.Image(width, height, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false);
    VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    info.renderPass = s.scenePass;
    info.attachmentCount = 1;
    info.pAttachments = &next->view;
    info.width = unsigned(width);
    info.height = unsigned(height);
    info.layers = 1;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VK_REQUIRE(vkCreateFramebuffer(s.device, &info, nullptr, &framebuffer));
    s.ReleaseScale(0);
    s.ReleaseScale(1);
    s.scaleEnabled = false;
    vkDestroyFramebuffer(s.device, s.sceneFramebuffer, nullptr);
    const auto oldSlot = s.scene->slot;
    s.scene = std::move(next);
    s.freeSlots.push_back(oldSlot);
    s.sceneFramebuffer = framebuffer;
    s.width = width;
    s.height = height;
    s.sceneInitialized = false;
}
void VulkanRenderer::SetHdrParams(const VulkanHdrParams& params) {
    params.Validate();
    auto& s=*impl_;
    if (!s.hdr) throw std::logic_error("Cannot set HDR metadata on an SDR renderer");
    if (s.frame.valid) throw std::logic_error("Cannot change HDR metadata during a scene");
    s.CheckPresentEncoding();
    s.hdr=params;
}
VulkanHdrParams VulkanRenderer::HdrParams() const {
    if (!impl_->hdr) throw std::logic_error("SDR renderer has no HDR parameters");
    return *impl_->hdr;
}

void VulkanRenderer::PrepareScale(bool fullscreen,std::span<const VulkanScaleStep> steps,
    uint32_t targetW,uint32_t targetH) {
    auto& s=*impl_;
    if (s.frame.valid) throw std::logic_error("Preparing scale during game drawing");
    if (!s.scaleLibrary) s.scaleLibrary=std::make_unique<VulkanScaleLibrary>(
        VulkanScaleDevice{s.window.physicalDevice,s.device,s.window.graphicsQueue,s.window.graphicsFamily});
    std::unique_ptr<Impl::ScaleMode> next;
    if (!steps.empty()) {
        next=std::make_unique<Impl::ScaleMode>();
        const VulkanScaleImage input{s.scene->image,s.scene->view,VK_FORMAT_B8G8R8A8_UNORM,uint32_t(s.width),uint32_t(s.height)};
        next->pipeline=std::make_unique<VulkanScalePipeline>(*s.scaleLibrary,input,steps,targetW,targetH);
        const auto output=next->pipeline->output;
        next->sampledOutput=std::make_unique<VulkanTexture>();
        auto& texture=*next->sampledOutput;
        texture.ownsImage=false; texture.image=output.image; texture.view=output.view;
        texture.width=int(output.width); texture.height=int(output.height);
        texture.slot = s.AllocSlot();
        texture.format = output.format;
        texture.layout = texture.sampledLayout;
        s.slotOwners[texture.slot] = &texture;
        texture.slotOwner = &s.slotOwners[texture.slot];
        s.UpdateImageDescriptor(texture);
    }
    s.window.WaitForFrame();
    s.ReleaseScale(unsigned(fullscreen));
    s.scaleModes[unsigned(fullscreen)]=std::move(next);
    if (s.scaleMode==unsigned(fullscreen)) s.scaleEnabled=false;
}
void VulkanRenderer::WarmScale(bool fullscreen) {
    auto& s=*impl_;
    auto* mode=s.scaleModes[unsigned(fullscreen)].get();
    if (!mode || mode->warm) return;
    if (s.frame.valid) throw std::logic_error("Warming scale during game drawing");
    s.window.WaitForFrame();
    auto cmd=s.BeginTransfer();
    if (!s.sceneInitialized) {
        Barrier(cmd,s.scene->image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT);
        const VkClearColorValue clear{{0,0,0,1}};
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
        vkCmdClearColorImage(cmd,s.scene->image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&clear,1,&range);
        mode->pipeline->Record(cmd,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT);
    } else {
        mode->pipeline->Record(cmd,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT);
    }
    s.FinishTransfer();
    s.CompleteTransfers();
    s.sceneInitialized=true;
    s.scene->layout=s.scene->sampledLayout;
    mode->warm=true;
}
void VulkanRenderer::SetScaleMode(bool enabled,bool fullscreen) {
    auto& s=*impl_;
    if (s.frame.valid) throw std::logic_error("Switching scale during game drawing");
    const auto* mode=s.scaleModes[unsigned(fullscreen)].get();
    if (enabled && mode && !mode->warm) throw std::logic_error("Scale chain has not completed warm-up");
    // Empty mode sections intentionally use direct present.
    s.scaleMode=unsigned(fullscreen);
    s.scaleEnabled=enabled && mode;
}
VulkanScaleImage VulkanRenderer::ScaleOutput(bool fullscreen) const {
    const auto* mode=impl_->scaleModes[unsigned(fullscreen)].get();
    return mode?mode->pipeline->output:VulkanScaleImage{};
}
VulkanClip VulkanRenderer::PresentRect(int fw,int fh) const {
    if (fw<=0 || fh<=0) throw std::invalid_argument("Invalid scale surface dimensions");
    const auto& s=*impl_;
    if (const auto* mode=s.ActiveScale()) {
        const auto image=mode->pipeline->output;
        // User-defined output size: center and clip/letterbox, never rescale.
        return {(fw-int(image.width))/2,(fh-int(image.height))/2,int(image.width),int(image.height)};
    }
    return FitSceneToSurface(s.width,s.height,fw,fh);
}
VulkanRenderer::ScaleCounts VulkanRenderer::GetScaleCounts() const {
    const auto& s=*impl_;
    ScaleCounts counts{s.scaleLibrary?s.scaleLibrary->ProgramCount():0,s.scaleLibrary?s.scaleLibrary->LookupCount():0,0};
    for (const auto& mode:s.scaleModes) if (mode) counts.writableImages+=mode->pipeline->WritableImageCount();
    return counts;
}

std::vector<uint32_t> VulkanRenderer::ReadScalePixels(bool fullscreen) {
    auto& s=*impl_;
    const auto image=ScaleOutput(fullscreen);
    if (s.frame.valid || !image.image) throw std::logic_error("Scale readback requires a completed pipeline");
    if (image.format!=VK_FORMAT_R8G8B8A8_UNORM)
        throw std::runtime_error("Scale diagnostic readback currently requires RGBA8");
    s.window.WaitForFrame();
    Buffer readback;
    readback.Create(VkDeviceSize(image.width)*image.height*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    auto cmd=s.BeginTransfer();
    Barrier(cmd,image.image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy copy{};
    copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={image.width,image.height,1};
    vkCmdCopyImageToBuffer(cmd,image.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
    Barrier(cmd,image.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT);
    HostReadBarrier(cmd);
    s.FinishTransfer();
    s.CompleteTransfers();
    const auto* source=static_cast<const uint32_t*>(readback.mapped);
    std::vector<uint32_t> pixels(size_t(image.width)*image.height);
    // Diagnostic PNG writer expects BGRA words. No CPU conversion in present.
    for(size_t i=0;i<pixels.size();++i)
        pixels[i]=(source[i]&0xff00ff00)|((source[i]&255)<<16)|((source[i]>>16)&255);
    return pixels;
}

ImagePixels VulkanRenderer::ReadTexturePixels(VulkanTexture* handle) {
    auto& s=*impl_;
    if(s.frame.valid)throw std::logic_error("Texture readback requires a completed scene");
    const auto found=s.textures.find(handle);
    if(found==s.textures.end())throw std::invalid_argument("Readback texture is not live in this renderer");
    auto& texture=*found->second;
    const bool generated=texture.generated && texture.format==VK_FORMAT_R16G16B16A16_SFLOAT;
    if(texture.format!=VK_FORMAT_B8G8R8A8_UNORM && texture.format!=VK_FORMAT_R8G8B8A8_UNORM && !generated)
        throw std::invalid_argument("Texture diagnostic readback requires BGRA8, RGBA8 or a generated floating-point image");
    const bool swizzle=texture.format==VK_FORMAT_R8G8B8A8_UNORM;
    ImagePixels result(texture.width,texture.height);
    s.window.WaitForFrame();
    s.FlushTextureUploads(); // A diagnostic can read an image before its first draw.
    // Compute-written images (caustics) live in GENERAL and carry shader writes;
    // upload-owned ones sit in SHADER_READ_ONLY with fragment reads only.
    const bool general=texture.sampledLayout==VK_IMAGE_LAYOUT_GENERAL;
    const VkAccessFlags sampledAccess=VK_ACCESS_SHADER_READ_BIT|
        (general?VK_ACCESS_SHADER_WRITE_BIT:0);
    Buffer readback;readback.Create(VkDeviceSize(result.pixels.size())*texture.texelBytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    auto cmd=s.BeginTransfer();
    Barrier(cmd,texture.image,texture.sampledLayout,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy copy{};
    copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};
    copy.imageExtent={uint32_t(texture.width),uint32_t(texture.height),1};
    vkCmdCopyImageToBuffer(cmd,texture.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
    Barrier(cmd,texture.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,texture.sampledLayout,
        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT|VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_TRANSFER_READ_BIT,sampledAccess);
    HostReadBarrier(cmd);
    s.FinishTransfer();
    s.CompleteTransfers();
    if(generated) {
        // Explicit diagnostics/export only: rendering keeps all generated pixels
        // in floating-point GPU memory and never routes through this conversion.
        const auto component=[&](size_t index) -> float {
            const unsigned int bits=static_cast<const unsigned short*>(readback.mapped)[index];
            const unsigned int exponent=(bits>>10)&31u, mantissa=bits&1023u;
            const float value=exponent==31u ? (mantissa ? std::numeric_limits<float>::quiet_NaN() :
                std::numeric_limits<float>::infinity()) : exponent==0u ?
                std::ldexp(float(mantissa),-24) : std::ldexp(float(1024u+mantissa),int(exponent)-25);
            return bits&0x8000u ? -value : value;
        };
        const auto byte=[](float value) -> uint32_t {
            if(!std::isfinite(value))return 0;
            return uint32_t(std::lround(std::clamp(value,0.0f,1.0f)*255.0f));
        };
        for(size_t i=0;i<result.pixels.size();++i) {
            const float pixel[]{component(i*4),component(i*4+1),component(i*4+2),component(i*4+3)};
            const float alpha=pixel[3];
            const float inverse=alpha>0.0f?1.0f/alpha:0.0f;
            result.pixels[i]=(byte(alpha)<<24)|(byte(pixel[0]*inverse)<<16)|
                (byte(pixel[1]*inverse)<<8)|byte(pixel[2]*inverse);
        }
    } else {
        std::memcpy(result.pixels.data(),readback.mapped,result.pixels.size()*4);
    }
    if(swizzle) // RGBA8 源转 ImagePixels 的 BGRA 字节序契约
        for(auto& pixel:result.pixels)
            pixel=(pixel&0xFF00FF00u)|((pixel&0x00FF0000u)>>16)|((pixel&0x000000FFu)<<16);
    if(texture.opaque)for(auto& pixel:result.pixels)pixel|=0xff000000u;
    return result;
}

std::vector<uint32_t> VulkanRenderer::ReadScenePixels() {
    auto& s = *impl_;
    if (s.frame.valid || !s.sceneInitialized) throw std::logic_error("Readback requires a completed scene");
    auto* image=s.scene.get();
#if defined(PVZ_VULKAN_SPRITE_EXPERIMENT)
    if (s.presentCopyExperiment) image=s.benchmarkOutput;
#endif
    s.window.WaitForFrame();
    Buffer readback;
    readback.Create(VkDeviceSize(s.width) * s.height * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    auto cmd = s.BeginTransfer();
    Barrier(cmd, image->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {static_cast<uint32_t>(s.width), static_cast<uint32_t>(s.height), 1};
    vkCmdCopyImageToBuffer(cmd, image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.buffer, 1, &copy);
    Barrier(cmd, image->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT);
    HostReadBarrier(cmd);
    s.FinishTransfer();
    s.CompleteTransfers();
    std::vector<uint32_t> pixels(size_t(s.width) * s.height);
    std::memcpy(pixels.data(), readback.mapped, pixels.size() * 4);
    return pixels;
}
}
