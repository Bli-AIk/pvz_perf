#include "WebGPUContext.h"
#include "platform/NativeWindow.h"
#include <emscripten.h>
#include <stdexcept>
namespace Sexy
{
WebGPUContext::WebGPUContext(NativeWindowHandle native, bool, bool) : window(native), instance(wgpu::CreateInstance())
{
    bool ready = false;
    instance.RequestAdapter(nullptr, wgpu::CallbackMode::AllowSpontaneous,
                            [&](wgpu::RequestAdapterStatus status, wgpu::Adapter value, wgpu::StringView message) {
                                if (status == wgpu::RequestAdapterStatus::Success)
                                    adapter = std::move(value);
                                else
                                    error.assign(message.data, message.length);
                                ready = true;
                            });
    while (!ready)
        emscripten_sleep(1);
    if (!adapter)
        throw std::runtime_error("WebGPU adapter unavailable: " + error);
    wgpu::DeviceDescriptor desc;
    const wgpu::FeatureName dual = wgpu::FeatureName::DualSourceBlending;
    dualSource = adapter.HasFeature(dual) &&
                 !EM_ASM_INT({ return new URL(location.href).searchParams.get('dualSource') == 'off'; });
    if (dualSource)
    {
        desc.requiredFeatureCount = 1;
        desc.requiredFeatures = &dual;
    }
    desc.SetUncapturedErrorCallback(
        [](const wgpu::Device &, wgpu::ErrorType, wgpu::StringView message, WebGPUContext *self) {
            self->error.assign(message.data, message.length);
            emscripten_log(EM_LOG_ERROR, "WebGPU: %s", self->error.c_str());
        },
        this);
    ready = false;
    adapter.RequestDevice(&desc, wgpu::CallbackMode::AllowSpontaneous,
                          [&](wgpu::RequestDeviceStatus status, wgpu::Device value, wgpu::StringView message) {
                              if (status == wgpu::RequestDeviceStatus::Success)
                                  device = std::move(value);
                              else
                                  error.assign(message.data, message.length);
                              ready = true;
                          });
    while (!ready)
        emscripten_sleep(1);
    if (!device)
        throw std::runtime_error("WebGPU device unavailable: " + error);
    queue = device.GetQueue();
    wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvas;
    canvas.selector = "#canvas";
    wgpu::SurfaceDescriptor sd;
    sd.nextInChain = &canvas;
    surface = instance.CreateSurface(&sd);
    wgpu::SurfaceCapabilities caps;
    surface.GetCapabilities(adapter, &caps);
    if (!caps.formatCount)
        throw std::runtime_error("WebGPU canvas has no present format");
    format = caps.formats[0];
    RecreateSwapchain();
    EM_ASM(
        {
            Module.pvzGraphicsBackend = 'webgpu';
            Module.pvzWebGPUFeatures = {};
            Module.pvzWebGPUFeatures.dualSource = !!$0;
            Module.pvzWebGPUFeatures.framesInFlight = 2;
            Module.pvzWebGPUFeatures.bindless = false;
        },
        dualSource);
}
WebGPUContext::~WebGPUContext()
{
    WaitIdle();
    surface.Unconfigure();
}
void WebGPUContext::MakeCurrent()
{
}
void WebGPUContext::Check() const
{
    if (!error.empty())
        throw std::runtime_error("WebGPU: " + error);
}
bool WebGPUContext::RecreateSwapchain()
{
    Check();
    if (width == static_cast<unsigned>(window->Width) && height == static_cast<unsigned>(window->Height))
        return true;
    width = window->Width;
    height = window->Height;
    wgpu::SurfaceConfiguration config;
    config.device = device;
    config.format = format;
    config.usage = wgpu::TextureUsage::RenderAttachment;
    config.width = width;
    config.height = height;
    config.alphaMode = wgpu::CompositeAlphaMode::Opaque;
    surface.Configure(&config);
    ++swapchainGeneration;
    return true;
}
void WebGPUContext::WaitIdle()
{
    if (!queue)
        return;
    bool done = false;
    queue.OnSubmittedWorkDone(wgpu::CallbackMode::AllowSpontaneous,
                              [&](wgpu::QueueWorkDoneStatus, wgpu::StringView) { done = true; });
    while (!done)
        emscripten_sleep(1);
}
bool WebGPUContext::BeginPresent()
{
    Check();
    return !surfaceSuspended && RecreateSwapchain();
}
void WebGPUContext::Present()
{
} // The browser presents the current canvas texture.
void WebGPUContext::NotifySurfaceSuspended(bool value)
{
    surfaceSuspended = value;
}
} // namespace Sexy
