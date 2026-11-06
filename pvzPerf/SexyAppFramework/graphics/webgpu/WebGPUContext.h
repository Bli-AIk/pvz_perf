#pragma once
#include "platform/NativeHandles.h"
#include <string>
#include <webgpu/webgpu_cpp.h>
namespace Sexy
{
class WebGPUContext final
{
  public:
    NativeWindowHandle window;
    unsigned int width = 0, height = 0;
    bool surfaceSuspended = false;
    bool dualSource = false;
    const bool hdr = false;
    bool tearingEnabled = false, tearingSupported = false;
    unsigned long long swapchainGeneration = 0;
    std::string adapterDescription = "WebGPU", error;
    int majorVersion = 1, minorVersion = 0;
    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::Queue queue;
    wgpu::Surface surface;
    wgpu::TextureFormat format = wgpu::TextureFormat::BGRA8Unorm;
    WebGPUContext(NativeWindowHandle window, bool validation, bool hdr = false);
    ~WebGPUContext();
    void MakeCurrent();
    bool RecreateSwapchain();
    void WaitIdle();
    bool BeginPresent();
    void Present();
    void NotifySurfaceSuspended(bool suspended);
    void Check() const;
};
} // namespace Sexy
