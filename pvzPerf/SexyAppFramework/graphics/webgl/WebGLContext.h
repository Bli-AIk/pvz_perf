#pragma once
#include "platform/NativeHandles.h"
#include <string>
namespace Sexy {
class WebGLContext final {
public:
    NativeWindowHandle window;
    unsigned int width, height;
    bool surfaceSuspended = false;
    const bool hdr = false;
    bool tearingEnabled = false, tearingSupported = false;
    unsigned long long swapchainGeneration = 0;
    std::string adapterDescription;
    int majorVersion = 3, minorVersion = 0;
    WebGLContext(NativeWindowHandle window, bool validation, bool hdr = false);
    ~WebGLContext();
    void MakeCurrent();
    bool RecreateSwapchain();
    void WaitIdle();
    bool BeginPresent();
    void Present();
    void NotifySurfaceSuspended(bool suspended);
};
}
