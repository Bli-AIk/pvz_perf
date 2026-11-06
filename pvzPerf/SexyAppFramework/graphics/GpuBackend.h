#pragma once
#include "GpuTypes.h"
#include "GpuBackendAdapter.h"
#include "vulkan/VulkanHdr.h"
#include "vulkan/VulkanViewport.h"
#include <cstdint>
#include <type_traits>
#if defined(PVZ_GPU_D3D9)
#include "d3d9/D3d9Context.h"
#include "d3d9/D3d9Renderer.h"
#include "d3d9/D3d9ScaleTypes.h"
#elif defined(PVZ_GPU_WEBGPU)
#include "webgpu/WebGPUContext.h"
#include "webgpu/WebGPURenderer.h"
#include "webgpu/WebGPUScaleTypes.h"
#elif defined(PVZ_GPU_WEBGL)
#include "webgl/WebGLContext.h"
#include "webgl/WebGLRenderer.h"
#include "webgl/WebGLScaleTypes.h"
#elif defined(PVZ_GPU_OPENGL)
#include "opengl/OpenGLContext.h"
#include "opengl/OpenGLRenderer.h"
#include "opengl/OpenGLScaleTypes.h"
#elif defined(PVZ_GPU_D3D12)
#include "d3d12/D3d12Context.h"
#include "d3d12/D3d12Renderer.h"
#include "d3d12/D3d12ScalePipeline.h"
#elif defined(PVZ_GPU_METAL)
#include "metal/MetalContext.h"
#include "metal/MetalRenderer.h"
#include "metal/MetalScaleTypes.h"
#else
#define PVZ_GPU_VULKAN 1
#include "vulkan/VulkanContext.h"
#include "vulkan/VulkanRenderer.h"
#endif

namespace Sexy {
// CMake selects one concrete implementation; there is no per-draw dispatch.
#if defined(PVZ_GPU_D3D9)
using GpuContext = D3d9Context;
using GpuRenderer = D3d9Renderer;
using GpuTexture = D3d9Texture;
using GpuScaleStep = D3d9ScaleStep;
using GpuScaleImage = D3d9ScaleImage;
#elif defined(PVZ_GPU_WEBGPU)
using GpuContext = WebGPUContext;
using GpuRenderer = WebGPURenderer;
using GpuTexture = WebGPUTexture;
using GpuScaleStep = WebGPUScaleStep;
using GpuScaleImage = WebGPUScaleImage;
#elif defined(PVZ_GPU_WEBGL)
using GpuContext = WebGLContext;
using GpuRenderer = WebGLRenderer;
using GpuTexture = WebGLTexture;
using GpuScaleStep = WebGLScaleStep;
using GpuScaleImage = WebGLScaleImage;
#elif defined(PVZ_GPU_OPENGL)
using GpuContext = OpenGLContext;
using GpuRenderer = OpenGLRenderer;
using GpuTexture = OpenGLTexture;
using GpuScaleStep = OpenGLScaleStep;
using GpuScaleImage = OpenGLScaleImage;
#elif defined(PVZ_GPU_D3D12)
using GpuContext = D3d12Context;
using GpuRenderer = D3d12Renderer;
using GpuTexture = D3d12Texture;
using GpuScaleStep = D3d12ScaleStep;
using GpuScaleImage = D3d12ScaleImage;
#elif defined(PVZ_GPU_METAL)
using GpuContext = MetalContext;
using GpuRenderer = MetalRenderer;
using GpuTexture = MetalTexture;
using GpuScaleStep = MetalScaleStep;
using GpuScaleImage = MetalScaleImage;
#else
using GpuContext = ::VulkanContext;
using GpuRenderer = VulkanRenderer;
using GpuTexture = VulkanTexture;
using GpuScaleStep = VulkanScaleStep;
using GpuScaleImage = VulkanScaleImage;
#endif
using GpuHdrParams = VulkanHdrParams;
#ifdef _WIN32
using GpuHdrDisplayObserver = VulkanHdrDisplayObserver;
#endif
template<class NativeHandle>
inline unsigned long long GpuHandleIdentity(NativeHandle handle) {
    static_assert(sizeof(unsigned long long) == 8);
    static_assert(sizeof(NativeHandle) <= sizeof(unsigned long long));
    if constexpr (std::is_pointer_v<NativeHandle>)
        return reinterpret_cast<unsigned long long>(handle);
    else
        return static_cast<unsigned long long>(handle);
}
inline unsigned long long GpuScaleIdentity(const GpuScaleImage& image) {
    return GpuHandleIdentity(GpuBackendDetail::ScaleHandle<BuildConfig::Graphics>(image));
}
}
