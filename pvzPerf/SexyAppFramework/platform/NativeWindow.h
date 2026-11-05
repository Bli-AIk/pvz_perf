#pragma once
#include <memory>
#include <string>

namespace Sexy {
static_assert(sizeof(unsigned int) == 4);
static_assert(sizeof(unsigned long long) == 8);
enum class NativeWindowBackend { X11, Wayland, Android, Browser };
enum class NativeCursor {
    Arrow, Hand, Text, Move, Forbidden, ResizeHorizontal, ResizeVertical,
    ResizeNorthEast, ResizeNorthWest, Wait, Hidden, Count
};
enum class NativeWindowEventKind {
    Close, Resize, Focus, MouseMove, MouseLeave, MouseButton, MouseWheel, Key, Text,
    MouseEnter, TouchBegan, TouchMoved, TouchEnded, TouchCanceled, MouseCanceled
};
struct NativePointerState {
    double x = 0, y = 0;
    bool inside = false;
};
struct NativeWindowEvent {
    NativeWindowEventKind kind{};
    double x = 0, y = 0;
    int width = 0, height = 0;
    // Buttons: 0=left, 1=right, 2=middle. Key symbol is an XKB keysym.
    unsigned int key = 0, scanCode = 0, text = 0;
    // Native input milliseconds, extended across the server's 32-bit wrap.
    union {
        unsigned long long timeMs = 0;
        unsigned long long touchId; // Touch events carry identity, not mouse timing.
    };
    int button = 0;
    bool pressed = false, focused = false, minimized = false, repeat = false;
};
using NativeWindowEventCallback = void (*)(void*, const NativeWindowEvent&);
void QueueNativeWindowEvent(void* context, const NativeWindowEvent& event);
struct NativeWindowConfig {
    std::string title = "pvz_perf";
    int width = 800, height = 600;
    bool visible = true;
    void* callbackContext = nullptr;
    NativeWindowEventCallback callback = nullptr;
};
struct NativeSurfaceHandles {
    // X11: Display* + XID. Wayland: wl_display* + wl_surface*.
    void* connection = nullptr;
    void* surface = nullptr;
    unsigned long long windowId = 0;
};
// Exactly one implementation is selected at build time. No SDK in this header.
class NativeWindow final {
public:
    explicit NativeWindow(const NativeWindowConfig& config);
    ~NativeWindow();
    NativeWindow(const NativeWindow&) = delete;
    NativeWindow& operator=(const NativeWindow&) = delete;
    void PumpEvents();
    // Call once per logical update for hover/drag position. PumpEvents never
    // emits MouseMove; buttons keep their event-time coordinates in the FIFO.
    // X11 queries the server; Wayland returns the last dispatched surface state.
    NativePointerState SamplePointer();
    // X11 maps now; Wayland maps on the renderer's next buffer commit.
    void Show();
    void ApplyCursor(NativeCursor cursor);
    void SetTitle(const std::string& title);
    void SetFullscreen(bool fullscreen);
    // Application-driven size changes also replace the fixed windowed bounds.
    void Resize(int width, int height);
    int Width = 0;
    int Height = 0;
    bool Focused = false;
    bool Visible = false;
    // Throws if the native protocol cannot report this state.
    bool Minimized() const;
    double RefreshRate() const;
    NativeWindowBackend Backend() const;
    NativeSurfaceHandles SurfaceHandles() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
