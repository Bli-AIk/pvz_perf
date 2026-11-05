#include "WebGLContext.h"
#include "WebGLApi.h"
#include "platform/NativeWindow.h"
namespace Sexy {
WebGLContext::WebGLContext(NativeWindowHandle native, bool, bool)
    : window(native), width(native->Width), height(native->Height), adapterDescription("WebGL 2") {}
WebGLContext::~WebGLContext() = default;
void WebGLContext::MakeCurrent() {}
bool WebGLContext::RecreateSwapchain() { width = window->Width; height = window->Height; return true; }
void WebGLContext::WaitIdle() { glFinish(); }
bool WebGLContext::BeginPresent() { RecreateSwapchain(); return !surfaceSuspended; }
void WebGLContext::Present() { glFlush(); }
void WebGLContext::NotifySurfaceSuspended(bool suspended) { surfaceSuspended = suspended; }
}
