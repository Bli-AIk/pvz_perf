#pragma once
#include "SexyAppFramework/platform/NativeHandles.h"
#include <vulkan/vulkan.h>
#include <cstdint>
#include <optional>
#include <vector>
#include <array>

using VulkanGpuFilter = bool (*)(VkPhysicalDevice);
struct VulkanInitConfig {
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "VulkanContext",
        .applicationVersion = VK_MAKE_VERSION(0, 1, 0),
#if defined(PVZ_VULKAN_LEGACY)
        .apiVersion = VK_API_VERSION_1_0,
#else
        .apiVersion = VK_API_VERSION_1_3,
#endif
    };
    VkInstanceCreateInfo instanceInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
    };
    std::vector<const char*> extraExtensions;
    std::vector<const char*> extraLayers;
    std::vector<const char*> deviceExtensions;
    bool enableValidation = false;
    VulkanGpuFilter gpuFilter = [](VkPhysicalDevice) { return true; };
    VkPhysicalDeviceFeatures features{};
    PFN_vkDebugUtilsMessengerCallbackEXT debugCallback = &DefaultDebugCallback;
    static VkBool32 VKAPI_CALL DefaultDebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT types,
        const VkDebugUtilsMessengerCallbackDataEXT* data, void* userData);
};

// Owns Vulkan resources only. The application's native window must outlive this context
// and every renderer using its device; the application owns all window actions.
// WSI is selected at build time; Linux also borrows the window's connection.
class VulkanContext {
public:
    VulkanContext(const VulkanInitConfig& config, NativeWindowHandle borrowedWindow,
                  const VkSwapchainCreateInfoKHR& swapchainConfig);
    ~VulkanContext();
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    VulkanContext(VulkanContext&&) = delete;
    VulkanContext& operator=(VulkanContext&&) = delete;

    // Android 恢复会换绑新的 ANativeWindow（NotifySurfaceWindow）；其他平台恒定。
    NativeWindowHandle hwnd;
    // Shared GPU lifetime is retained for the renderer's existing resources.
    static inline VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    static inline VkDevice device = VK_NULL_HANDLE;
    // Device commands are resolved at startup: older Android loader stubs do
    // not export 1.3 symbols even when the installed driver supports them.
    static PFN_vkQueueSubmit2 queueSubmit2;
    static PFN_vkCmdPipelineBarrier2 pipelineBarrier2;
    static inline bool dualSourceBlendEnabled = false;
    static inline uint32_t graphicsFamily = 0;
    static inline uint32_t presentFamily = 0;
    static inline VkQueue graphicsQueue = VK_NULL_HANDLE;
    static inline VkQueue presentQueue = VK_NULL_HANDLE;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainFormat = VK_FORMAT_UNDEFINED;
    VkColorSpaceKHR swapchainColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkExtent2D swapchainExtent{};
    // 创建交换链时的窗口原始查询值（安卓横屏锁定的竖屏窗口=1116x2480，
    // 交换链经 preTransform 修正是 2480x1116——两者恒不等）。重建判定必须
    // "查询值对查询值"，否则每帧都触发销毁重建（nubia 实机逐帧重建案）。
    VkExtent2D drawableExtentAtCreate{};
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainViews;
    VkRenderPass renderPass = VK_NULL_HANDLE;

    static constexpr unsigned int kFramesInFlight = 2;
    struct FrameContext {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        uint32_t imageIndex = 0;
        VkExtent2D extent{};
        bool valid = false;
        unsigned long long serial = 0;
        unsigned int resourceIndex = 0;
    };
    // Zero-size/minimized surfaces defer without acquiring or destroying assets.
    bool RecreateSwapchain();
    // Application notification only: retains the existing suspension/rebuild
    // state without performing any window action or GPU operation.
    void NotifySurfaceSuspended(bool suspended);
    // Android resume hands back a NEW ANativeWindow; the old VkSurfaceKHR is
    // backed by the released window. Rebuild surface + swapchain on the new one.
    void NotifySurfaceWindow(NativeWindowHandle window);
    FrameContext BeginCommands();
    void BeginPresentPass(VkClearColorValue clearColor);
    void WaitForFrame();
    void EndFrame();
    void AbortFrame() noexcept;
    unsigned long long submittedSerial = 0;
    unsigned long long completedSerial = 0;
    unsigned long long recordingSerial = 0;
    uint64_t swapchainGeneration = 0;

private:
    static void InitializeVulkan(const VulkanInitConfig& config);
    void DestroyAssets() noexcept;
    void CreateSurfaceAssets();
    VkExtent2D QueryDrawableExtent() const;
    std::optional<VkExtent2D> QuerySwapchainExtent(VkSurfaceCapabilitiesKHR& caps) const;
    void CreateSwapchain(const VkSurfaceCapabilitiesKHR& caps, VkExtent2D extent);
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateFrameLoopAssets();
    void CreatePresentSemaphores();

    VkSwapchainCreateInfoKHR swapchainConfig_{};
    bool counted_ = false;
    std::vector<VkFramebuffer> framebuffers_;
    struct FrameResources {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkSemaphore acquired = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        unsigned long long serial = 0;

    };
    std::array<FrameResources, kFramesInFlight> frameResources_{};
    unsigned int frameResourceIndex_ = 0;
    std::vector<VkSemaphore> renderFinished_;
    bool frameActive_ = false;
    bool frameLoopFailed_ = false;
    bool presentPassActive_ = false;
    bool swapchainDirty_ = false;
    bool surfaceSuspended_ = false;
    uint32_t currentImageIndex_ = 0;
};
