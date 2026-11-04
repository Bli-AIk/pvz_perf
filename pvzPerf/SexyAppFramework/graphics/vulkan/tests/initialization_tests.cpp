#include "TestWin32Window.h"
#include "VulkanEnumeration.h"
#include <atomic>
#include <iostream>
#include <string>

namespace {
std::atomic<unsigned> validationErrors = 0;
unsigned rejectedGpuCandidates = 0;
VkBool32 VKAPI_CALL Validate(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT types, const VkDebugUtilsMessengerCallbackDataEXT* data, void* user) {
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) ++validationErrors;
    return VulkanInitConfig::DefaultDebugCallback(severity, types, data, user);
}
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
void NoSharedDevice() {
    Require(!VulkanContext::device && !VulkanContext::physicalDevice &&
        !VulkanContext::graphicsQueue && !VulkanContext::presentQueue,
        "Failed initialization retained shared Vulkan device handles");
}
void Run() {
    VulkanInitConfig config;
    config.enableValidation = true;
    config.debugCallback = Validate;
    VulkanTest::Window native(64,64);
    const auto swapchain = VulkanTest::SwapchainConfig();

#if defined(PVZ_VULKAN_LEGACY)
    Require(config.appInfo.apiVersion == VK_API_VERSION_1_0, "Legacy instance version is not 1.0");
#else
    // The modern implementation must not downgrade its instance version while
    // continuing to use descriptor indexing and core Submit2/barrier2.
    auto legacy = config;
    legacy.appInfo.apiVersion = VK_API_VERSION_1_2;
    bool rejectedLegacy = false;
    try { VulkanContext rejected(legacy,native.hwnd,swapchain); }
    catch (const std::runtime_error& error) {
        rejectedLegacy = std::string(error.what()).find("requires Vulkan 1.3") != std::string::npos;
    }
    Require(rejectedLegacy, "Vulkan 1.3 requirement was bypassed");
    NoSharedDevice();
#endif

    for (int attempt = 0; attempt < 3; ++attempt) {
        bool caught = false;
        auto missing = config;
        missing.extraLayers.push_back("VK_LAYER_PVZ_intentionally_missing_initialization_test");
        try { VulkanContext rejected(missing,native.hwnd,swapchain); }
        catch (const VulkanCallError& error) {
            caught = error.result == VK_ERROR_LAYER_NOT_PRESENT &&
                std::string(error.what()).find("VK_LAYER_PVZ_intentionally_missing") != std::string::npos;
        }
        Require(caught, "Missing required layer was hidden by a fallback");
        NoSharedDevice();

        caught = false;
        auto noCallback = config;
        noCallback.debugCallback = nullptr;
        try { VulkanContext rejected(noCallback,native.hwnd,swapchain); }
        catch (const std::invalid_argument& error) {
            caught = std::string(error.what()).find("callback is null") != std::string::npos;
        }
        Require(caught, "Required diagnostic callback failed silently");
        NoSharedDevice();

        caught = false;
        auto noGpu = config;
        rejectedGpuCandidates = 0;
        noGpu.gpuFilter = [](VkPhysicalDevice) {
            ++rejectedGpuCandidates;
            return false;
        };
        std::string rejectionReason;
        try { VulkanContext rejected(noGpu,native.hwnd,swapchain); }
        catch (const std::runtime_error& error) {
            caught = true;
            rejectionReason = error.what();
        }
        Require(caught, "Reject-all GPU filter did not fail initialization");
        NoSharedDevice();
        Require(rejectedGpuCandidates > 0, "Reject-all GPU filter was never evaluated");
        if (rejectionReason.find("No GPU supports Vulkan") == std::string::npos)
            throw std::runtime_error("Unexpected GPU rejection reason: " + rejectionReason);

        // Borrowed HWNDs must survive both failed and successful GPU lifetimes.
        {
            VulkanContext first(config,native.hwnd,swapchain);
            const auto device = VulkanContext::device;
            {
                VulkanTest::Window sibling(64,64);
                VulkanContext second(config,sibling.hwnd,swapchain);
                Require(device == VulkanContext::device && first.surface != second.surface,
                    "Concurrent windows did not keep independent surfaces and one device");
            }
            Require(device == VulkanContext::device, "Second-context teardown destroyed the shared device");
            Require(first.RecreateSwapchain(), "Remaining window could not rebuild after sibling teardown");
        }
        NoSharedDevice();
    }
    // Normal distribution has no validation-layer requirement. A callback is
    // irrelevant when diagnostics were not requested, and must not be called.
    config.enableValidation = false; config.debugCallback = nullptr;
    { VulkanContext normal(config,native.hwnd,swapchain); Require(normal.swapchain != VK_NULL_HANDLE, "Release initialization failed"); }
    Require(IsWindow(native.hwnd),"Context teardown destroyed its borrowed HWND");
    NoSharedDevice();
    Require(validationErrors == 0, "Vulkan validation reported a lifecycle error");
}
}

int main() {
    try {
        Run();
        std::cout << "PASS Vulkan initialization: required diagnostics, failed-init cleanup, shared device, release launch\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
