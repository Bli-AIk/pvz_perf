#include "platform/NativeWindow.h"
#include "platform/TabletCaret.h"
#include "WebTextInput.h"
#include "SexyAppBase.h"
#include "platform/UserDirectories.h"
#include "platform/ErrorDialog.h"
#include <SDL.h>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <algorithm>
#include <stdexcept>

EM_JS(int, WebViewportWidth, (), { return Module.pvzViewport?.width || 800; });
EM_JS(double, WebRefreshRate, (), { return Module.pvzRefreshRate || 60; });

namespace Sexy {
namespace {
int pendingWidth = 0, pendingHeight = 0;
unsigned int pendingRefreshRate = 0;
}
extern "C" EMSCRIPTEN_KEEPALIVE void pvz_web_refresh_rate(unsigned int rate) {
    pendingRefreshRate = std::clamp(rate, 30U, 1000U);
}
extern "C" EMSCRIPTEN_KEEPALIVE void pvz_web_resize(int width, int height) {
    // DOM events only publish dimensions. Apply them in the normal event phase,
    // never from a JS callback in the middle of a game draw/Asyncify unwind.
    pendingWidth = std::clamp(width, 800, 1400);
    pendingHeight = 600;
}
extern "C" EMSCRIPTEN_KEEPALIVE void pvz_web_pointer(int kind, int id, double x, double y, int button, double time) {
    SDL_Window* window = SDL_GetKeyboardFocus();
    if (!window) return;
    int width, height; SDL_GetWindowSize(window, &width, &height);
    SDL_Event event{};
    if (kind == 0) {
        event.type = SDL_MOUSEMOTION;
        event.motion.x = static_cast<int>(x * width); event.motion.y = static_cast<int>(y * height);
    } else if (kind < 3) {
        event.type = kind == 1 ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
        event.button.button = static_cast<unsigned char>(button);
        event.button.x = static_cast<int>(x * width); event.button.y = static_cast<int>(y * height);
    } else if (kind == 6 || kind == 7) {
        event.type = kind == 6 ? SDL_USEREVENT : SDL_USEREVENT + 1; event.user.code = id;
    } else {
        event.type = kind == 3 ? SDL_FINGERDOWN : kind == 4 ? SDL_FINGERMOTION : SDL_FINGERUP;
        event.tfinger.fingerId = id; event.tfinger.x = static_cast<float>(x); event.tfinger.y = static_cast<float>(y);
    }
    event.common.timestamp = static_cast<unsigned int>(time);
    SDL_PushEvent(&event);
}
struct NativeWindow::Impl {
    SDL_Window* window = nullptr;
    SDL_GLContext context = nullptr;
    NativeWindowConfig config;
    NativePointerState pointer;
};
NativeWindow::NativeWindow(const NativeWindowConfig& config) : impl_(new Impl) {
    impl_->config = config;
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) throw std::runtime_error(SDL_GetError());
#if !defined(PVZ_GPU_WEBGPU)
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
    Width = std::clamp(WebViewportWidth(), 800, 1400); Height = 600; Focused = true;
    impl_->window = SDL_CreateWindow(config.title.c_str(), 0, 0, Width, Height,
#if defined(PVZ_GPU_WEBGPU)
        0
#else
        SDL_WINDOW_OPENGL
#endif
    );
    if (!impl_->window) throw std::runtime_error(SDL_GetError());

#if !defined(PVZ_GPU_WEBGPU)
    impl_->context = SDL_GL_CreateContext(impl_->window);
    if (!impl_->context) throw std::runtime_error(SDL_GetError());
    emscripten_webgl_enable_extension(emscripten_webgl_get_current_context(), "EXT_color_buffer_float");
    emscripten_webgl_enable_extension(emscripten_webgl_get_current_context(), "OES_texture_float_linear");
#endif
}
NativeWindow::~NativeWindow() {
#if !defined(PVZ_GPU_WEBGPU)
    SDL_GL_DeleteContext(impl_->context);
#endif
    SDL_DestroyWindow(impl_->window);
}
void NativeWindow::PumpEvents() {
    ProcessWebTextInput();
    if (pendingWidth) {
        const int w = pendingWidth, h = pendingHeight;
        pendingWidth = pendingHeight = 0;
        if (w != Width || h != Height) {
            Resize(w, h);
            NativeWindowEvent resized{};
            resized.kind = NativeWindowEventKind::Resize;
            resized.width = w; resized.height = h;
            if (impl_->config.callback) impl_->config.callback(impl_->config.callbackContext, resized);
        }
    }
    if (pendingRefreshRate) {
        // Apply JS observations only at the normal native event boundary, never
        // re-enter C++ while Asyncify has suspended an update/draw. Retain all
        // wall-time backlog and already simulated time across cadence changes.
        if (gSexyAppBase && gSexyAppBase->mAutoDetectFrameRate)
            gSexyAppBase->mFrameTiming = FrameTiming::FromDisplayRefreshRate(pendingRefreshRate);
        pendingRefreshRate = 0;
    }
    SDL_Event input;
    while (SDL_PollEvent(&input)) {
        NativeWindowEvent event{}; bool emit = true;
        switch (input.type) {
        case SDL_USEREVENT + 1:
            event.kind = NativeWindowEventKind::MouseCanceled; break;
        case SDL_USEREVENT:
            event.kind = NativeWindowEventKind::TouchCanceled;
            event.touchId = static_cast<unsigned long long>(input.user.code); break;
        case SDL_QUIT: event.kind = NativeWindowEventKind::Close; break;
        case SDL_MOUSEMOTION:
            if (input.motion.which == SDL_TOUCH_MOUSEID) { emit = false; break; }
            impl_->pointer = {double(input.motion.x), double(input.motion.y), true}; emit = false; break;
        case SDL_MOUSEBUTTONDOWN: case SDL_MOUSEBUTTONUP:
            if (input.button.which == SDL_TOUCH_MOUSEID) { emit = false; break; }
            event.kind = NativeWindowEventKind::MouseButton;
            event.x = input.button.x; event.y = input.button.y; event.timeMs = input.button.timestamp;
            event.pressed = input.type == SDL_MOUSEBUTTONDOWN;
            event.button = input.button.button == SDL_BUTTON_RIGHT ? 1 : input.button.button == SDL_BUTTON_MIDDLE ? 2 : 0;
            impl_->pointer = {event.x, event.y, true}; break;
        case SDL_MOUSEWHEEL: event.kind = NativeWindowEventKind::MouseWheel; event.y = input.wheel.y; break;
        case SDL_KEYDOWN: case SDL_KEYUP:
            event.kind = NativeWindowEventKind::Key; event.pressed = input.type == SDL_KEYDOWN;
            event.key = input.key.keysym.sym;
            if (event.key == SDLK_ESCAPE) event.key = 0xff1b;
            else if (event.key == SDLK_RETURN) event.key = 0xff0d;
            else if (event.key == SDLK_BACKSPACE) event.key = 0xff08;
            else if (event.key == SDLK_TAB) event.key = 0xff09;
            else if (event.key == SDLK_LEFT) event.key = 0xff51;
            else if (event.key == SDLK_UP) event.key = 0xff52;
            else if (event.key == SDLK_RIGHT) event.key = 0xff53;
            else if (event.key == SDLK_DOWN) event.key = 0xff54;
            else if (event.key == SDLK_DELETE) event.key = 0xffff;
            else if (event.key == SDLK_HOME) event.key = 0xff50;
            else if (event.key == SDLK_END) event.key = 0xff57;
            else if (event.key == SDLK_LSHIFT || event.key == SDLK_RSHIFT) event.key = 0xffe1;
            else if (event.key == SDLK_LCTRL || event.key == SDLK_RCTRL) event.key = 0xffe3;
            else if (event.key == SDLK_LALT || event.key == SDLK_RALT) event.key = 0xffe9;
            event.repeat = input.key.repeat; break;
        case SDL_FINGERDOWN: case SDL_FINGERMOTION: case SDL_FINGERUP:
            event.kind = input.type == SDL_FINGERDOWN ? NativeWindowEventKind::TouchBegan :
                input.type == SDL_FINGERUP ? NativeWindowEventKind::TouchEnded : NativeWindowEventKind::TouchMoved;
            event.touchId = static_cast<unsigned long long>(input.tfinger.fingerId);
            event.x = input.tfinger.x * Width; event.y = input.tfinger.y * Height; break;
        case SDL_WINDOWEVENT:
            if (input.window.event == SDL_WINDOWEVENT_FOCUS_GAINED || input.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                event.kind = NativeWindowEventKind::Focus;
                Focused = event.focused = input.window.event == SDL_WINDOWEVENT_FOCUS_GAINED;
            } else emit = false;
            break;
        case SDL_TEXTINPUT:
            for (const unsigned char* p = reinterpret_cast<unsigned char*>(input.text.text); *p; ++p) {
                if (*p >= 128) continue;
                event.kind = NativeWindowEventKind::Text; event.text = *p;
                if (impl_->config.callback) impl_->config.callback(impl_->config.callbackContext, event);
            }
            emit = false; break;
        default: emit = false; break;
        }
        if (emit && impl_->config.callback) impl_->config.callback(impl_->config.callbackContext, event);
    }
}
NativePointerState NativeWindow::SamplePointer() { return impl_->pointer; }
void NativeWindow::Show() { Visible = true; SDL_StartTextInput(); }
void NativeWindow::ApplyCursor(NativeCursor cursor) { SDL_ShowCursor(cursor == NativeCursor::Hidden ? SDL_DISABLE : SDL_ENABLE); }
void NativeWindow::SetTitle(const std::string& title) { SDL_SetWindowTitle(impl_->window, title.c_str()); }
void NativeWindow::SetFullscreen(bool fullscreen) { EM_ASM({ Module.pvzSetFullscreen(!!$0); }, fullscreen); }
void NativeWindow::Resize(int w, int h) { Width = w; Height = h; SDL_SetWindowSize(impl_->window, w, h); }
bool NativeWindow::Minimized() const { return false; }
double NativeWindow::RefreshRate() const { return WebRefreshRate(); }
NativeWindowBackend NativeWindow::Backend() const { return NativeWindowBackend::Browser; }
NativeSurfaceHandles NativeWindow::SurfaceHandles() const { return {}; }
bool ShowNativeErrorDialog(const char* message, const char*) { emscripten_log(EM_LOG_ERROR, "%s", message); return true; }
std::optional<std::string> ResolveDesktopDirectory() { return std::nullopt; }
}
