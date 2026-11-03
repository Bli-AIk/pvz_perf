#pragma once

#include "SexyAppFramework/platform/windows/NativeHandles.h"

#include <cstdint>
#include <memory>
#include <string>

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12GraphicsCommandList;
struct ID3D12Resource;

namespace Sexy {

// The application owns the HWND and must keep it alive until destruction.
// Calls and member access use one render thread; all COM pointers are borrowed.
class D3d12Context {
public:
    D3d12Context(NativeWindowHandle hwnd, bool validation, bool hdr = false);
    ~D3d12Context();
    D3d12Context(const D3d12Context&) = delete;
    D3d12Context& operator=(const D3d12Context&) = delete;
    D3d12Context(D3d12Context&&) = delete;
    D3d12Context& operator=(D3d12Context&&) = delete;

    // Commits any drawable resize, waits for the last submitted fence without
    // signaling another fence, and resets recording. One list may be in flight.
    ID3D12GraphicsCommandList* BeginCommands();
    // Upload/compute submission. Use Present() after a successful BeginPresent().
    void SubmitAndWait();
    // Requires BeginCommands(). Transitions PRESENT -> RENDER_TARGET; nullptr
    // means presentation is suspended. No viewport, RTV binding, or clear is set.
    ID3D12Resource* BeginPresent();
    ID3D12Resource* BackBuffer() const noexcept;
    std::uint64_t BackBufferRtv() const noexcept;
    // Transitions back, closes, executes, signals its fence, then presents
    // without a host fence wait. Keep referenced resources/descriptors alive
    // until BeginCommands() or WaitIdle() completes. When
    // BeginPresent() returned nullptr, only the recorded commands are submitted.
    void Present();

    // Allocated buffer size, retained while the native surface is suspended.
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    // Queries native client size. Zero-size/minimized/suspended surfaces defer.
    // Returns true only after a successful create/resize; no window actions.
    // A resize requires no active recording and no external back-buffer refs.
    bool RecreateSwapchain();
    bool surfaceSuspended = false;
    std::uint64_t swapchainGeneration = 0;
    // Waits for submitted work, without submitting any open command list.
    void WaitIdle();

    std::string adapterDescription;
    // SDR: BGRA8 UNORM. HDR: RGBA16F, linear scRGB.
    const bool hdr;
    // Encoded D3D_SHADER_MODEL guaranteed by the SM 6.0 feature query (0x60).
    std::uint32_t shaderModel = 0;
    bool tearingSupported = false;
    // Default true: supported windowed presentation uses interval 0 + tearing.
    // Disabled or unsupported tearing uses interval 1. No swapchain rebuild.
    bool tearingEnabled = true;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

public:
    // Borrowed pointers; Impl retains COM ownership for this context's lifetime.
    ID3D12Device* const device;
    ID3D12CommandQueue* const queue;
};

} // namespace Sexy
