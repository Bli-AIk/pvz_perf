#include "D3d12Context.h"
#include "D3d12Check.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <array>
#include <limits>
#include <utility>
#include <cstdio>

namespace Sexy {
namespace {

using Microsoft::WRL::ComPtr;
constexpr UINT BackBufferCount = 2;
void CALLBACK ValidationMessage(D3D12_MESSAGE_CATEGORY, D3D12_MESSAGE_SEVERITY severity,
    D3D12_MESSAGE_ID id, LPCSTR description, void*) {
    if (severity <= D3D12_MESSAGE_SEVERITY_ERROR)
        std::fprintf(stderr, "[validation] D3D12 id=%u %s\n", unsigned(id), description);
    else if (severity == D3D12_MESSAGE_SEVERITY_WARNING)
        std::fprintf(stderr, "[d3d12-warning] id=%u %s\n", unsigned(id), description);
}

void Check(HRESULT result, const char* operation, ID3D12Device* device = nullptr) {
    const HRESULT reason = FAILED(result) && device
        ? device->GetDeviceRemovedReason() : S_OK;
    D3d12Check(static_cast<std::int32_t>(result), operation,
               static_cast<std::int32_t>(reason));
}

void CheckWin32(const char* operation) {
    const DWORD error = GetLastError();
    Check(HRESULT_FROM_WIN32(error ? error : ERROR_GEN_FAILURE), operation);
}

std::string AdapterName(const DXGI_ADAPTER_DESC1& description) {
    const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        description.Description, -1, nullptr, 0, nullptr, nullptr);
    if (!length)
        CheckWin32("WideCharToMultiByte(adapter description size)");
    std::string name(static_cast<std::size_t>(length), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        description.Description, -1, name.data(), length, nullptr, nullptr))
        CheckWin32("WideCharToMultiByte(adapter description)");
    name.pop_back();
    return name;
}

void RequireFormat(ID3D12Device* device, DXGI_FORMAT format, UINT support1,
                   UINT support2, const char* name) {
    D3D12_FEATURE_DATA_FORMAT_SUPPORT support{};
    support.Format = format;
    const std::string operation = std::string("CheckFeatureSupport(format ") + name + ')';
    Check(device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,
        &support, sizeof(support)), operation.c_str(), device);
    if ((static_cast<UINT>(support.Support1) & support1) != support1 ||
        (static_cast<UINT>(support.Support2) & support2) != support2) {
        std::ostringstream message;
        message << "Required D3D12 format " << name
                << " unsupported: Support1=0x" << std::hex
                << static_cast<UINT>(support.Support1) << " (need 0x" << support1
                << "), Support2=0x" << static_cast<UINT>(support.Support2)
                << " (need 0x" << support2 << ')';
        throw std::runtime_error(message.str());
    }
}

D3D_SHADER_MODEL CheckCapabilities(ID3D12Device* device, bool hdr) {
    D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
    Check(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,
        &options, sizeof(options)), "CheckFeatureSupport(D3D12_OPTIONS)", device);
    if (options.ResourceBindingTier < D3D12_RESOURCE_BINDING_TIER_2)
        throw std::runtime_error("D3D12 requires resource binding tier >= 2");
    if (!options.DoublePrecisionFloatShaderOps)
        throw std::runtime_error("D3D12 requires double precision for font filtering");

    D3D12_FEATURE_DATA_SHADER_MODEL shaders{D3D_SHADER_MODEL_6_0};
    const HRESULT result = device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,
        &shaders, sizeof(shaders));
    Check(result, "CheckFeatureSupport(SHADER_MODEL)", device);
    // The native renderer consumes DXIL compiled with *_6_0 profiles.
    if (shaders.HighestShaderModel < D3D_SHADER_MODEL_6_0)
        throw std::runtime_error("D3D12 requires usable shader model >= 6.0 for native DXIL shaders");

    const UINT sampled = D3D12_FORMAT_SUPPORT1_TEXTURE2D |
        D3D12_FORMAT_SUPPORT1_SHADER_LOAD | D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE;
    const UINT renderTarget = sampled | D3D12_FORMAT_SUPPORT1_RENDER_TARGET |
        D3D12_FORMAT_SUPPORT1_BLENDABLE;
    const UINT uav = D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW;
    const UINT typedUav = D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD |
        D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE;
    RequireFormat(device, DXGI_FORMAT_B8G8R8A8_UNORM, renderTarget, 0, "BGRA8 UNORM");
    RequireFormat(device, DXGI_FORMAT_R8G8B8A8_UNORM,
        renderTarget | uav, typedUav, "RGBA8 UNORM");
    RequireFormat(device, DXGI_FORMAT_R8_UNORM, sampled, 0, "R8 UNORM");
    RequireFormat(device, DXGI_FORMAT_R16G16B16A16_FLOAT,
        renderTarget | uav, typedUav, "RGBA16F");
    if (hdr)
        RequireFormat(device, DXGI_FORMAT_R16G16B16A16_FLOAT,
            renderTarget, 0, "RGBA16F scRGB");

    return shaders.HighestShaderModel;
}

struct FenceEvent {
    HANDLE handle = nullptr;
    ~FenceEvent() {
        if (handle)
            CloseHandle(handle);
    }
    FenceEvent() = default;
    FenceEvent(const FenceEvent&) = delete;
    FenceEvent& operator=(const FenceEvent&) = delete;
};

} // namespace

struct D3d12Context::Impl {
    NativeWindowHandle const hwnd;
    bool const& hdr;
    bool& tearingSupported;
    bool& tearingEnabled;
    bool& surfaceSuspended;
    bool surfaceDrawable = false;
    bool recording = false;
    bool presentActive = false;
    std::uint32_t& width;
    std::uint32_t& height;
    UINT currentBuffer = 0;
    UINT rtvStride = 0;
    std::uint64_t& swapchainGeneration;
    UINT64 fenceValue = 0;
    std::uint32_t& shaderModel;
    std::string& adapterDescription;
    ComPtr<IDXGIFactory4> factory;
    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12InfoQueue1> validationQueue;
    DWORD validationCookie = 0;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;
    ComPtr<ID3D12Fence> fence;
    FenceEvent fenceEvent;
    ComPtr<IDXGISwapChain3> swapchain;
    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    std::array<ComPtr<ID3D12Resource>, BackBufferCount> backBuffers;

    Impl(D3d12Context& owner, NativeWindowHandle window, bool validation)
        : hwnd(window), hdr(owner.hdr), tearingSupported(owner.tearingSupported),
          tearingEnabled(owner.tearingEnabled), surfaceSuspended(owner.surfaceSuspended),
          width(owner.width), height(owner.height),
          swapchainGeneration(owner.swapchainGeneration), shaderModel(owner.shaderModel),
          adapterDescription(owner.adapterDescription) {
        if (!hwnd || !IsWindow(hwnd))
            throw std::invalid_argument("D3d12Context requires a live borrowed HWND");
        if (validation) {
            ComPtr<ID3D12Debug> debug;
            Check(D3D12GetDebugInterface(IID_PPV_ARGS(debug.GetAddressOf())),
                "D3D12GetDebugInterface(validation requested)");
            debug->EnableDebugLayer();
        }
        Check(CreateDXGIFactory2(validation ? DXGI_CREATE_FACTORY_DEBUG : 0,
            IID_PPV_ARGS(factory.GetAddressOf())), "CreateDXGIFactory2");
        SelectDevice();
        if (validation) {
            Check(device.As(&validationQueue), "QueryInterface(ID3D12InfoQueue1)", device.Get());
            Check(validationQueue->RegisterMessageCallback(ValidationMessage,
                D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &validationCookie),
                "RegisterMessageCallback", device.Get());
        }
        CheckTearing();
        CreateCommands();
        RecreateSwapchain();
    }

    ~Impl() {
        if (queue && fence && fenceEvent.handle) {
            try {
                WaitIdle();
            } catch (...) {
                // Destruction must release COM resources even after device loss.
            }
        }
        if (validationQueue)
            validationQueue->UnregisterMessageCallback(validationCookie);
    }

    void CheckDevice() const {
        const HRESULT reason = device->GetDeviceRemovedReason();
        Check(reason, "ID3D12Device::GetDeviceRemovedReason", device.Get());
    }

    void SelectDevice() {
        ComPtr<IDXGIFactory6> factory6;
        const HRESULT query = factory.As(&factory6);
        if (query != E_NOINTERFACE)
            Check(query, "QueryInterface(IDXGIFactory6)");
        std::string rejected;
        for (UINT index = 0;; ++index) {
            ComPtr<IDXGIAdapter1> candidate;
            const HRESULT enumerated = factory6
                ? factory6->EnumAdapterByGpuPreference(index,
                    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                    IID_PPV_ARGS(candidate.GetAddressOf()))
                : factory->EnumAdapters1(index, candidate.GetAddressOf());
            if (enumerated == DXGI_ERROR_NOT_FOUND)
                break;
            Check(enumerated, "DXGI hardware adapter enumeration");
            DXGI_ADAPTER_DESC1 description{};
            Check(candidate->GetDesc1(&description), "IDXGIAdapter1::GetDesc1");
            if (description.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;
            const std::string name = AdapterName(description);
            try {
                ComPtr<ID3D12Device> candidateDevice;
                Check(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0,
                    IID_PPV_ARGS(candidateDevice.GetAddressOf())), "D3D12CreateDevice");
                const D3D_SHADER_MODEL supported =
                    CheckCapabilities(candidateDevice.Get(), hdr);
                adapter = std::move(candidate);
                device = std::move(candidateDevice);
                shaderModel = supported;
                adapterDescription = name;
                return;
            } catch (const std::runtime_error& error) {
                rejected += "\n  " + name + ": " + error.what();
            }
        }
        throw std::runtime_error(std::string("No hardware adapter satisfies D3D12") +
            (hdr ? " with linear scRGB" : "") +
            (rejected.empty() ? ": no eligible hardware adapters enumerated" : rejected));
    }

    void CheckTearing() {
        ComPtr<IDXGIFactory5> factory5;
        const HRESULT query = factory.As(&factory5);
        if (query == E_NOINTERFACE)
            return;
        Check(query, "QueryInterface(IDXGIFactory5)");
        BOOL supported = FALSE;
        const HRESULT result = factory5->CheckFeatureSupport(
            DXGI_FEATURE_PRESENT_ALLOW_TEARING, &supported, sizeof(supported));
        if (result == E_INVALIDARG || result == DXGI_ERROR_UNSUPPORTED)
            return;
        Check(result, "CheckFeatureSupport(PRESENT_ALLOW_TEARING)", device.Get());
        tearingSupported = supported != FALSE;
    }

    void CreateCommands() {
        D3D12_COMMAND_QUEUE_DESC queueDescription{};
        queueDescription.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        Check(device->CreateCommandQueue(&queueDescription,
            IID_PPV_ARGS(queue.GetAddressOf())), "CreateCommandQueue", device.Get());
        Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(allocator.GetAddressOf())), "CreateCommandAllocator", device.Get());
        Check(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator.Get(), nullptr, IID_PPV_ARGS(commandList.GetAddressOf())),
            "CreateCommandList", device.Get());
        Check(commandList->Close(), "Close(initial command list)", device.Get());
        Check(device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(fence.GetAddressOf())), "CreateFence", device.Get());
        fenceEvent.handle = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!fenceEvent.handle)
            CheckWin32("CreateEventW(D3D12 fence)");
        D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
        heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDescription.NumDescriptors = BackBufferCount;
        Check(device->CreateDescriptorHeap(&heapDescription,
            IID_PPV_ARGS(rtvHeap.GetAddressOf())), "CreateDescriptorHeap(RTV)", device.Get());
        rtvStride = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    void WaitForFence(UINT64 value) {
        const auto completed = [this]() {
            const UINT64 completion = fence->GetCompletedValue();
            if (completion == (std::numeric_limits<UINT64>::max)()) {
                Check(DXGI_ERROR_DEVICE_REMOVED, "ID3D12Fence::GetCompletedValue",
                    device.Get());
            }
            return completion;
        };
        CheckDevice();
        if (completed() >= value)
            return;
        Check(fence->SetEventOnCompletion(value, fenceEvent.handle),
            "SetEventOnCompletion", device.Get());
        for (;;) {
            // A removed device must not leave an infinite Win32 event wait.
            const DWORD result = WaitForSingleObject(fenceEvent.handle, 1000);
            if (result == WAIT_FAILED)
                CheckWin32("WaitForSingleObject(D3D12 fence)");
            CheckDevice();
            if (completed() >= value)
                return;
            if (result != WAIT_OBJECT_0 && result != WAIT_TIMEOUT)
                throw std::runtime_error("Unexpected D3D12 fence event wait result");
        }
    }

    UINT64 Signal() {
        CheckDevice();
        if (fenceValue == (std::numeric_limits<UINT64>::max)() - 1)
            throw std::overflow_error("D3D12 fence value exhausted");
        const UINT64 value = fenceValue + 1;
        Check(queue->Signal(fence.Get(), value), "ID3D12CommandQueue::Signal", device.Get());
        fenceValue = value;
        return value;
    }

    void WaitIdle() {
        WaitForFence(Signal());
    }

    bool DrawableSize(UINT& drawableWidth, UINT& drawableHeight) const {
        drawableWidth = 0;
        drawableHeight = 0;
        if (surfaceSuspended)
            return false;
        RECT client{};
        if (!GetClientRect(hwnd, &client))
            CheckWin32("GetClientRect(D3D12 borrowed HWND)");
        if (IsIconic(hwnd) || client.right <= client.left || client.bottom <= client.top)
            return false;
        drawableWidth = static_cast<UINT>(client.right - client.left);
        drawableHeight = static_cast<UINT>(client.bottom - client.top);
        return true;
    }

    DXGI_FORMAT SwapchainFormat() const noexcept {
        return hdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM;
    }

    void SetColorSpace() {
        const DXGI_COLOR_SPACE_TYPE space = hdr
            ? DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
        UINT support = 0;
        Check(swapchain->CheckColorSpaceSupport(space, &support),
            "IDXGISwapChain3::CheckColorSpaceSupport", device.Get());
        if (!(support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT))
            throw std::runtime_error(hdr ? "linear scRGB swapchain presentation unsupported"
                                         : "SDR swapchain color space unsupported");
        Check(swapchain->SetColorSpace1(space), "IDXGISwapChain3::SetColorSpace1", device.Get());
    }

    bool RecreateSwapchain() {
        UINT drawableWidth = 0;
        UINT drawableHeight = 0;
        surfaceDrawable = DrawableSize(drawableWidth, drawableHeight);
        if (!surfaceDrawable)
            return false;
        if (swapchain && width == drawableWidth && height == drawableHeight)
            return false;
        if (recording)
            throw std::logic_error("RecreateSwapchain requires closed command recording");
        WaitIdle();
        const UINT flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
        if (swapchain) {
            // Reset releases indirect references retained by the previous list.
            Check(allocator->Reset(), "Reset(command allocator for resize)", device.Get());
            Check(commandList->Reset(allocator.Get(), nullptr),
                "Reset(command list for resize)", device.Get());
            Check(commandList->Close(), "Close(command list for resize)", device.Get());
            for (auto& buffer : backBuffers)
                buffer.Reset();
            width = 0;
            height = 0;
            surfaceDrawable = false;
            Check(swapchain->ResizeBuffers(BackBufferCount, drawableWidth, drawableHeight,
                SwapchainFormat(), flags), "IDXGISwapChain::ResizeBuffers", device.Get());
        } else {
            DXGI_SWAP_CHAIN_DESC1 description{};
            description.Width = drawableWidth;
            description.Height = drawableHeight;
            description.Format = SwapchainFormat();
            description.SampleDesc.Count = 1;
            description.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            description.BufferCount = BackBufferCount;
            description.Scaling = DXGI_SCALING_STRETCH;
            description.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            description.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
            description.Flags = flags;
            ComPtr<IDXGISwapChain1> created;
            Check(factory->CreateSwapChainForHwnd(queue.Get(), hwnd, &description,
                nullptr, nullptr, created.GetAddressOf()), "CreateSwapChainForHwnd", device.Get());
            Check(created.As(&swapchain), "QueryInterface(IDXGISwapChain3)", device.Get());
            Check(factory->MakeWindowAssociation(hwnd,
                DXGI_MWA_NO_WINDOW_CHANGES | DXGI_MWA_NO_ALT_ENTER),
                "MakeWindowAssociation(disable DXGI window actions)", device.Get());
        }
        SetColorSpace();
        std::array<ComPtr<ID3D12Resource>, BackBufferCount> buffers;
        D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT index = 0; index < BackBufferCount; ++index) {
            Check(swapchain->GetBuffer(index, IID_PPV_ARGS(buffers[index].GetAddressOf())),
                "IDXGISwapChain::GetBuffer", device.Get());
            device->CreateRenderTargetView(buffers[index].Get(), nullptr, rtv);
            rtv.ptr += rtvStride;
        }
        CheckDevice();
        backBuffers = std::move(buffers);
        width = drawableWidth;
        height = drawableHeight;
        currentBuffer = swapchain->GetCurrentBackBufferIndex();
        surfaceDrawable = true;
        ++swapchainGeneration;
        return true;
    }

    ID3D12GraphicsCommandList* BeginCommands() {
        if (recording)
            throw std::logic_error("BeginCommands requires the previous list to be submitted");
        RecreateSwapchain();
        WaitForFence(fenceValue);
        Check(allocator->Reset(), "ID3D12CommandAllocator::Reset", device.Get());
        Check(commandList->Reset(allocator.Get(), nullptr),
            "ID3D12GraphicsCommandList::Reset", device.Get());
        recording = true;
        presentActive = false;
        return commandList.Get();
    }

    ID3D12Resource* BackBuffer() const noexcept {
        if (!swapchain || surfaceSuspended || !surfaceDrawable)
            return nullptr;
        return backBuffers[currentBuffer].Get();
    }

    D3D12_CPU_DESCRIPTOR_HANDLE BackBufferRtv() const noexcept {
        D3D12_CPU_DESCRIPTOR_HANDLE rtv{};
        if (BackBuffer()) {
            rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
            rtv.ptr += static_cast<SIZE_T>(currentBuffer) * rtvStride;
        }
        return rtv;
    }

    void TransitionBackBuffer(D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = backBuffers[currentBuffer].Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = before;
        barrier.Transition.StateAfter = after;
        commandList->ResourceBarrier(1, &barrier);
    }

    ID3D12Resource* BeginPresent() {
        if (!recording)
            throw std::logic_error("BeginPresent requires BeginCommands");
        if (presentActive)
            throw std::logic_error("BeginPresent already active");
        UINT drawableWidth = 0;
        UINT drawableHeight = 0;
        surfaceDrawable = DrawableSize(drawableWidth, drawableHeight);
        if (!surfaceDrawable || !swapchain || !width || !height)
            return nullptr;
        currentBuffer = swapchain->GetCurrentBackBufferIndex();
        TransitionBackBuffer(D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
        presentActive = true;
        return backBuffers[currentBuffer].Get();
    }

    UINT64 Submit() {
        if (!recording)
            throw std::logic_error("Submit requires BeginCommands");
        if (presentActive)
            throw std::logic_error("An active back buffer must be submitted with Present");
        if (fenceValue == (std::numeric_limits<UINT64>::max)() - 1)
            throw std::overflow_error("D3D12 fence value exhausted");
        Check(commandList->Close(), "ID3D12GraphicsCommandList::Close", device.Get());
        recording = false;
        ID3D12CommandList* lists[] = {commandList.Get()};
        queue->ExecuteCommandLists(1, lists);
        return Signal();
    }

    void SubmitAndWait() {
        WaitForFence(Submit());
    }

    void Present() {
        if (!recording)
            throw std::logic_error("Present requires BeginCommands");
        const bool presenting = presentActive;
        if (presenting) {
            TransitionBackBuffer(D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            presentActive = false;
        }
        Submit();
        if (!presenting)
            return;
        UINT drawableWidth = 0;
        UINT drawableHeight = 0;
        surfaceDrawable = DrawableSize(drawableWidth, drawableHeight);
        if (!surfaceDrawable)
            return;
        const bool tearing = tearingSupported && tearingEnabled;
        const UINT interval = tearing ? 0 : 1;
        const UINT flags = tearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
        Check(swapchain->Present(interval, flags), "IDXGISwapChain::Present", device.Get());
        CheckDevice();
        currentBuffer = swapchain->GetCurrentBackBufferIndex();
    }
};

D3d12Context::D3d12Context(NativeWindowHandle hwnd, bool validation, bool hdr)
    : hdr(hdr), impl_(std::make_unique<Impl>(*this, hwnd, validation)),
      device(impl_->device.Get()), queue(impl_->queue.Get()) {}

D3d12Context::~D3d12Context() = default;

ID3D12GraphicsCommandList* D3d12Context::BeginCommands() { return impl_->BeginCommands(); }
void D3d12Context::SubmitAndWait() { impl_->SubmitAndWait(); }
ID3D12Resource* D3d12Context::BeginPresent() { return impl_->BeginPresent(); }
ID3D12Resource* D3d12Context::BackBuffer() const noexcept { return impl_->BackBuffer(); }
std::uint64_t D3d12Context::BackBufferRtv() const noexcept { return impl_->BackBufferRtv().ptr; }
void D3d12Context::Present() { impl_->Present(); }
bool D3d12Context::RecreateSwapchain() { return impl_->RecreateSwapchain(); }
void D3d12Context::WaitIdle() { impl_->WaitIdle(); }

} // namespace Sexy
