#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <set>
#include <algorithm>
#include "VulkanContext.h"
#include "VulkanSurfaceFormat.h"
#include "VulkanSurfaceExtent.h"
#include "VulkanEnumeration.h"
#include "platform/SurfacePlatform.h"

// 宏在预处理器阶段做文本替换，namespace 对它无效——习惯上放在 include 之后
#define VK_CHECK(expr) \
    do { if (const VkResult result_ = (expr); result_ != VK_SUCCESS) \
        throw VulkanCallError(result_, #expr); } while (0)

namespace {
    static constexpr char kValidationLayerName[] = "VK_LAYER_KHRONOS_validation";

    // ---- 进程级内部状态：static = 进程级共享存储，应用不可见。对外共享
    // 的进程级句柄（physicalDevice/device/队列族/队列）是类的 public
    // static 成员，声明在头文件，不在这里。 ----
    static bool appInitialized = false;
    static int contextCount = 0;
    static VulkanInitConfig initConfigSnapshot{};  // init 的配置快照（首窗口选卡要用）
    static VkInstance instance = VK_NULL_HANDLE;
    static VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    static VulkanGpuFilter gpuFilter =
        [](VkPhysicalDevice) { return true; };
    static VkPhysicalDeviceFeatures deviceFeatures{};

    // ---- 纯创建函数：吃参数、返结果，不碰 VulkanContext 的任何成员 ----
    // 放无名 namespace = 内部链接，只属于本文件，不进全局符号表。

    VkInstance CreateInstanceImpl(const VulkanInitConfig& config,
                                  const std::vector<const char*>& extensions,
                                  const std::vector<const char*>& layers) {
        unsigned int loaderVersion = 0;
        VK_CHECK(vkEnumerateInstanceVersion(&loaderVersion));
        if (loaderVersion < VK_API_VERSION_1_3 || config.appInfo.apiVersion < VK_API_VERSION_1_3)
            throw std::runtime_error("This renderer requires Vulkan 1.3");
        // Instance WSI extensions and requested validation/extra layers.
        VkInstanceCreateInfo createInfo = config.instanceInfo;
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;   // 强制
        createInfo.pApplicationInfo = &config.appInfo;               // 强制指向配置内的 appInfo
        createInfo.enabledExtensionCount =
            static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();
        createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
        createInfo.ppEnabledLayerNames = layers.data();

        VkInstance instance = VK_NULL_HANDLE;
        VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));
        return instance;
    }

    // Explicit diagnostics are a requirement, not a best-effort fallback.
    // A normal release launch leaves enableValidation false and needs no SDK.
    VkInstance CreateConfiguredInstance(const VulkanInitConfig& config) {
        const auto layers = EnumerateVulkan<VkLayerProperties>(
            "vkEnumerateInstanceLayerProperties", vkEnumerateInstanceLayerProperties);
        std::vector<const char*> layerNames = config.extraLayers;
        if (config.enableValidation) layerNames.push_back(kValidationLayerName);
        for (const char* name : layerNames) {
            if (!name) throw std::invalid_argument("Vulkan layer name is null");
            if (std::none_of(layers.begin(), layers.end(), [name](const auto& layer) {
                return std::strcmp(layer.layerName, name) == 0;
            }))
                throw VulkanCallError(VK_ERROR_LAYER_NOT_PRESENT, std::string("Required Vulkan layer ") + name);
        }
        std::vector<const char*> extensions{
            VK_KHR_SURFACE_EXTENSION_NAME, VulkanSurfacePlatform::extension};
        if (config.enableValidation) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        extensions.insert(extensions.end(), config.extraExtensions.begin(),
                          config.extraExtensions.end());
        return CreateInstanceImpl(config, extensions, layerNames);
    }

    // 注册调试回调：错误/警告按严重级别过滤输出。回调函数指针由配置给定。
    // 这个扩展不是核心 API，需要经 vkGetInstanceProcAddr 取函数指针。
    VkDebugUtilsMessengerEXT RegisterDebugMessenger(
        VkInstance instance, PFN_vkDebugUtilsMessengerCallbackEXT callback) {
        if (!callback) throw std::invalid_argument("Vulkan validation callback is null");
        VkDebugUtilsMessengerCreateInfoEXT debugInfo{
            VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        debugInfo.messageSeverity =
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugInfo.messageType =
            VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugInfo.pfnUserCallback = callback;
        auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance,
                                  "vkCreateDebugUtilsMessengerEXT"));
        VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
        if (!create) throw std::runtime_error("Requested Vulkan debug messenger entry point is unavailable");
        VK_CHECK(create(instance, &debugInfo, nullptr, &messenger));
        return messenger;
    }

    // 自动过滤器 1：候选卡必须支持全部要求的设备扩展
    bool DeviceExtensionsSupported(VkPhysicalDevice candidate,
                                   const std::vector<const char*>& required) {
        const auto avail = EnumerateVulkan<VkExtensionProperties>(
            "vkEnumerateDeviceExtensionProperties", [candidate](uint32_t* count, VkExtensionProperties* values) {
                return vkEnumerateDeviceExtensionProperties(candidate, nullptr, count, values);
            });
        for (const char* name : required)
        {
            bool found = false;
            for (const auto& e : avail)
                if (std::strcmp(e.extensionName, name) == 0) { found = true; break; }
            if (!found)
                return false;
        }
        return true;
    }

    // 自动过滤器 2：请求开启的每个功能位都必须受支持。
    // VkPhysicalDeviceFeatures 的成员全是 VkBool32，规范布局无填充，
    // 按数组逐位比较安全且完备。
    bool DeviceFeaturesSupported(VkPhysicalDevice candidate,
                                 const VkPhysicalDeviceFeatures& wanted) {
        VkPhysicalDeviceFeatures supported{};
        vkGetPhysicalDeviceFeatures(candidate, &supported);
        constexpr uint32_t kBoolCount =
            sizeof(VkPhysicalDeviceFeatures) / sizeof(VkBool32);
        const VkBool32* w = reinterpret_cast<const VkBool32*>(&wanted);
        const VkBool32* s = reinterpret_cast<const VkBool32*>(&supported);
        for (uint32_t i = 0; i < kBoolCount; ++i)
            if (w[i] == VK_TRUE && s[i] != VK_TRUE)
                return false;
        return true;
    }

    bool RequiredFeaturesSupported(VkPhysicalDevice candidate) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(candidate, &properties);
        if (properties.apiVersion < VK_API_VERSION_1_3)
            return false;
        VkPhysicalDeviceVulkan13Features v13{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceVulkan12Features v12{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        v12.pNext = &v13;
        VkPhysicalDeviceFeatures2 features2{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &v12};
        vkGetPhysicalDeviceFeatures2(candidate, &features2);
        return v13.synchronization2 == VK_TRUE &&
               v12.descriptorBindingPartiallyBound == VK_TRUE &&
               v12.descriptorBindingSampledImageUpdateAfterBind == VK_TRUE &&
               v12.shaderSampledImageArrayNonUniformIndexing == VK_TRUE;
    }

    VkPhysicalDeviceVulkan12Features DescriptorIndexingEnable() {
        VkPhysicalDeviceVulkan12Features v12{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        v12.descriptorBindingPartiallyBound = VK_TRUE;
        v12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
        v12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
        return v12;
    }

    // 选卡：打分制（独显 +1000），同时带回 graphics/present 队列族下标。
    // 过滤顺序：自动生成的扩展/功能过滤器 → 调用者的 gpuFilter。
    VkPhysicalDevice PickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface,
                                        VulkanGpuFilter gpuFilter,
                                        const std::vector<const char*>& requiredExtensions,
                                        const VkPhysicalDeviceFeatures& wantedFeatures,
                                        uint32_t& outGraphicsFamily,
                                        uint32_t& outPresentFamily) {
        const auto devices = EnumerateVulkan<VkPhysicalDevice>(
            "vkEnumeratePhysicalDevices", [instance](uint32_t* count, VkPhysicalDevice* values) {
                return vkEnumeratePhysicalDevices(instance, count, values);
            });
        if (devices.empty())
            throw std::runtime_error("没有找到支持 Vulkan 的显卡");

        int bestScore = -1;
        VkPhysicalDevice best = VK_NULL_HANDLE;
        for (VkPhysicalDevice candidate : devices)
        {
            // 自动过滤器先行：扩展/功能/descriptor indexing 不支持的候选根本不进打分
            if (!DeviceExtensionsSupported(candidate, requiredExtensions))
                continue;
            if (!DeviceFeaturesSupported(candidate, wantedFeatures))
                continue;
            if (!RequiredFeaturesSupported(candidate))
                continue;

            if (!gpuFilter(candidate))
                continue;                    // 调用者的谓词有一票否决权

            uint32_t graphics = UINT32_MAX, present = UINT32_MAX;
            uint32_t familyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount,
                                                     nullptr);
            std::vector<VkQueueFamilyProperties> families(familyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount,
                                                     families.data());
            for (uint32_t i = 0; i < familyCount; ++i)
            {
                if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    graphics = i;
                VkBool32 presentSupport = VK_FALSE;
                VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface,
                                                     &presentSupport));
                if (presentSupport)
                    present = i;
                if (graphics != UINT32_MAX && present != UINT32_MAX)
                    break;
            }
            if (graphics == UINT32_MAX || present == UINT32_MAX)
                continue;

            // 交换链支持是硬性要求
            uint32_t formatCount = 0, modeCount = 0;
            VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(candidate, surface,
                                                 &formatCount, nullptr));
            VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(candidate, surface,
                                                      &modeCount, nullptr));
            if (formatCount == 0 || modeCount == 0)
                continue;

            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(candidate, &props);
            int score = 0;
            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                score += 1000;
            score += static_cast<int>(props.limits.maxImageDimension2D);

            if (score > bestScore)
            {
                bestScore = score;
                best = candidate;
                outGraphicsFamily = graphics;
                outPresentFamily = present;
            }
        }
        if (best == VK_NULL_HANDLE)
            throw std::runtime_error("No GPU supports Vulkan 1.3, descriptor indexing and graphics/present");

        return best;
    }

    // 逻辑设备：按两个队列族各建一条队列，启用配置要求的设备扩展。
    // VK_KHR_swapchain 由库强制加上（窗口呈现的底线）。
    // 设备功能（wantedFeatures）默认全关——要用各向异性过滤等功能时，
    // 由 InitConfig.features 置位传入。
    VkDevice CreateLogicalDevice(VkPhysicalDevice physicalDevice,
                                 uint32_t graphicsFamily, uint32_t presentFamily,
                                 const VkPhysicalDeviceFeatures& wantedFeatures,
                                 const std::vector<const char*>& deviceExtensions,
                                 VkQueue* outGraphicsQueue, VkQueue* outPresentQueue) {
        float priority = 1.0F;   // 1.0 = 最高调度优先级（单队列场景实际无效果）
        std::set<uint32_t> uniqueFamilies = {graphicsFamily, presentFamily};
        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        for (uint32_t family : uniqueFamilies)
        {
            VkDeviceQueueCreateInfo info{
                VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            info.queueFamilyIndex = family;
            info.queueCount = 1;
            info.pQueuePriorities = &priority;
            queueInfos.push_back(info);
        }

        // swapchain 扩展由库强制加上；其余来自配置
        std::vector<const char*> extensions = deviceExtensions;
        if (std::find(extensions.begin(), extensions.end(),
                      VK_KHR_SWAPCHAIN_EXTENSION_NAME) == extensions.end())
            extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

        VkPhysicalDeviceVulkan12Features v12 = DescriptorIndexingEnable();
        VkPhysicalDeviceVulkan13Features v13{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        v13.synchronization2 = VK_TRUE;
        v12.pNext = &v13;

        VkDeviceCreateInfo createInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        createInfo.pNext = &v12;
        createInfo.queueCreateInfoCount =
            static_cast<uint32_t>(queueInfos.size());
        createInfo.pQueueCreateInfos = queueInfos.data();
        createInfo.pEnabledFeatures = &wantedFeatures;
        createInfo.enabledExtensionCount =
            static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        VkDevice device = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device));
        vkGetDeviceQueue(device, graphicsFamily, 0, outGraphicsQueue);
        vkGetDeviceQueue(device, presentFamily, 0, outPresentQueue);
        return device;
    }
}

// ---- 配置里的默认回调实现：放 .cpp，头文件就不必为 std::cerr 引头文件 ----
PFN_vkQueueSubmit2 VulkanContext::queueSubmit2 = nullptr;
PFN_vkCmdPipelineBarrier2 VulkanContext::pipelineBarrier2 = nullptr;

VkBool32 VKAPI_CALL VulkanInitConfig::DefaultDebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* data, void*)
{
    // Never globally suppress a VUID: the same diagnostic may describe a real
    // image-format/usage mismatch in a different resource.
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::cerr << "[validation] " << data->pMessage << '\n';
    return VK_FALSE;
}

// Instance state is shared; the first attached surface selects the GPU.
void VulkanContext::InitializeVulkan(const VulkanInitConfig& config) {
    if (appInitialized)
        return;                               // 幂等：重复调用直接跳过
    initConfigSnapshot = config;                     // 首窗口选卡时要读 deviceExtensions/features

    try {
        instance = CreateConfiguredInstance(config);
        if (config.enableValidation)
            debugMessenger = RegisterDebugMessenger(instance, config.debugCallback);
        gpuFilter = config.gpuFilter;
        deviceFeatures = config.features;
        appInitialized = true;
    } catch (...) {
        // Unwind the partially initialized instance and preserve the failure.
        if (debugMessenger) {
            auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroy) destroy(instance, debugMessenger, nullptr);
            debugMessenger = VK_NULL_HANDLE;
        }
        if (instance) vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
        throw;
    }
}

// ---- 每窗口：swapchain + 图像视图 ----
VkExtent2D VulkanContext::QueryDrawableExtent() const {
    return VulkanSurfacePlatform::DrawableExtent(hwnd, surfaceSuspended_);
}

std::optional<VkExtent2D> VulkanContext::QuerySwapchainExtent(VkSurfaceCapabilitiesKHR& caps) const {
    const auto drawable = QueryDrawableExtent();
    if (!drawable.width || !drawable.height)
        return std::nullopt;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice,surface,&caps));
    const auto extent = ChooseVulkanSurfaceExtent(caps,static_cast<int>(drawable.width),
                                                static_cast<int>(drawable.height));
#if defined(__ANDROID__)
    if (!extent) return std::nullopt;
    // Android WSI scales swapchain images to the surface. Submit at the game's
    // native 600-pixel height, preserving the surface aspect and letterboxing.
    // Do not resize ANativeWindow: touch/client coordinates stay in screen pixels.
    constexpr unsigned int nativeHeight = 600;
    const auto nativeWidth = (static_cast<unsigned long long>(extent->width) * nativeHeight +
                              extent->height / 2) / extent->height;
    if (nativeWidth < caps.minImageExtent.width || nativeWidth > caps.maxImageExtent.width ||
        nativeHeight < caps.minImageExtent.height || nativeHeight > caps.maxImageExtent.height)
        throw std::runtime_error("Android surface cannot present the native game resolution");
    return VkExtent2D{static_cast<unsigned int>(nativeWidth),nativeHeight};
#else
    return extent;
#endif
}

void VulkanContext::CreateSwapchain(const VkSurfaceCapabilitiesKHR& caps, VkExtent2D extent) {

    // 从用户预填的创建信息出发；依赖 caps/窗口的字段随后由库强制覆写
    VkSwapchainCreateInfoKHR scInfo = swapchainConfig_;
    scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;   // 强制
    scInfo.surface = surface;

    const auto formats = EnumerateVulkan<VkSurfaceFormatKHR>(
        "vkGetPhysicalDeviceSurfaceFormatsKHR", [this](uint32_t* count, VkSurfaceFormatKHR* values) {
            return vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, count, values);
        });
    if (formats.empty()) throw std::runtime_error("Vulkan surface advertises no formats");
    if (swapchainGeneration == 0 && scInfo.imageColorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT) {
        const bool supported = std::any_of(formats.begin(), formats.end(), [](const auto& format) {
            return format.format == VK_FORMAT_R16G16B16A16_SFLOAT &&
                format.colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
        });
        if (!supported) {
            scInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
            scInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        }
    }
    const auto chosenFormat=ChooseVulkanSurfaceFormat(formats,scInfo.imageFormat,scInfo.imageColorSpace);
    // Resume/recreation must retain the renderer's original encoding. In
    // particular, an SDR fallback cannot silently become HDR after resume.
    swapchainConfig_.imageFormat = chosenFormat.format;
    swapchainConfig_.imageColorSpace = chosenFormat.colorSpace;
    scInfo.imageFormat=swapchainFormat=chosenFormat.format;
    scInfo.imageColorSpace=swapchainColorSpace=chosenFormat.colorSpace;

    // 呈现模式：FIFO 规范保证可用；请求的模式不支持时自然回退 FIFO
    const auto modes = EnumerateVulkan<VkPresentModeKHR>(
        "vkGetPhysicalDeviceSurfacePresentModesKHR", [this](uint32_t* count, VkPresentModeKHR* values) {
            return vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, count, values);
        });
    if (modes.empty()) throw std::runtime_error("Vulkan surface advertises no present modes");
    VkPresentModeKHR chosenMode = VK_PRESENT_MODE_FIFO_KHR;
    for (VkPresentModeKHR mode : modes)
        if (mode == scInfo.presentMode)
            chosenMode = mode;
    scInfo.presentMode = chosenMode;

    // Extent and capabilities are one validated snapshot, queried before
    // dismantling the old resources. Window configuration is not a fallback.
    scInfo.imageExtent = extent;
    swapchainExtent = extent;
    drawableExtentAtCreate = QueryDrawableExtent();

    // 图像数 = 最小值 + 1：避免 CPU 等待 GPU 释放图像；上限再钳一次
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;
    scInfo.minImageCount = imageCount;

    scInfo.imageArrayLayers = 1;
    // 图像属于哪个队列族：graphics 与 present 同族时 EXCLUSIVE 最高效
    uint32_t families[] = {graphicsFamily, presentFamily};
    if (graphicsFamily != presentFamily)
    {
        scInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        scInfo.queueFamilyIndexCount = 2;
        scInfo.pQueueFamilyIndices = families;
    }
    else
    {
        scInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    // 90°/270° 变换要求应用侧预旋转内容；本引擎按正向画面渲染，
    // 改请求 IDENTITY 交给合成器旋转（各安卓驱动普遍支持）。
    scInfo.preTransform =
        (caps.currentTransform == VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR ||
         caps.currentTransform == VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
            ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
            : caps.currentTransform;
    // Some Linux compositors do not advertise OPAQUE. Preserve the caller's
    // supported mode; otherwise select one the presentation engine accepts.
    if (!(caps.supportedCompositeAlpha & scInfo.compositeAlpha)) {
        static constexpr VkCompositeAlphaFlagBitsKHR candidates[]{
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
        for (const VkCompositeAlphaFlagBitsKHR candidate : candidates)
            if (caps.supportedCompositeAlpha & candidate) {
                scInfo.compositeAlpha = candidate;
                break;
            }
        if (!(caps.supportedCompositeAlpha & scInfo.compositeAlpha))
            throw std::runtime_error("Vulkan surface advertises no supported composite alpha mode");
    }

    VK_CHECK(vkCreateSwapchainKHR(device, &scInfo, nullptr, &swapchain));

    // 实际图像数量可能与 minImageCount+1 不同，再查一次拿真值
    swapchainImages = EnumerateVulkan<VkImage>(
        "vkGetSwapchainImagesKHR", [this](uint32_t* count, VkImage* values) {
            return vkGetSwapchainImagesKHR(device, swapchain, count, values);
        });
    if (swapchainImages.empty()) throw std::runtime_error("Vulkan swapchain has no images");

    // 视图：包一层让 shader/attachment 能引用图像（短，并入 swapchain 流程）
    swapchainViews.resize(swapchainImages.size());
    for (size_t i = 0; i < swapchainImages.size(); ++i)
    {
        VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        view.image = swapchainImages[i];
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = swapchainFormat;
        view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view.subresourceRange.levelCount = 1;
        view.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device, &view, nullptr, &swapchainViews[i]));
    }
    ++swapchainGeneration;
}

void VulkanContext::CreateRenderPass() {
    VkAttachmentDescription color{};
    color.format = swapchainFormat;
    color.samples = VK_SAMPLE_COUNT_1_BIT;
    // Presentation overwrites the game rectangle and explicitly clears any
    // letterbox bands. No swapchain pixel depends on its previous contents.
    color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;   // 不关心旧内容
    color.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // Explicit release after the pass.

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    // The acquire semaphore is waited at COLOR_ATTACHMENT_OUTPUT. The implicit
    // UNDEFINED -> COLOR_ATTACHMENT layout transition must itself depend on
    // that wait, not only the later color writes. A default TOP_OF_PIPE
    // external dependency allows the transition to race presentation reads.
    VkSubpassDependency acquireDependency{};
    acquireDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    acquireDependency.dstSubpass = 0;
    acquireDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    acquireDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    acquireDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{
        VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &color;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &acquireDependency;
    VK_CHECK(vkCreateRenderPass(device, &rpInfo, nullptr, &renderPass));
}

// ---- 帧循环托管设施：framebuffers（随交换链重建）+ 命令/同步（一次性）----
void VulkanContext::CreateFramebuffers() {
    framebuffers_.resize(swapchainViews.size());
    for (size_t i = 0; i < swapchainViews.size(); ++i) {
        VkFramebufferCreateInfo info{
            VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        info.renderPass = renderPass;
        info.attachmentCount = 1;
        info.pAttachments = &swapchainViews[i];
        info.width = swapchainExtent.width;
        info.height = swapchainExtent.height;
        info.layers = 1;
        VK_CHECK(vkCreateFramebuffer(device, &info, nullptr,
                                     &framebuffers_[i]));
    }
}

void VulkanContext::CreateFrameLoopAssets() {
    for (auto& resource : frameResources_) {
        VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool.queueFamilyIndex = graphicsFamily;
        VK_CHECK(vkCreateCommandPool(device, &pool, nullptr, &resource.pool));
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = resource.pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(device, &allocate, &resource.cmd));
        VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(device, &semaphore, nullptr, &resource.acquired));
        VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(device, &fence, nullptr, &resource.fence));

    }
    CreatePresentSemaphores();
}

// 呈现信号量按图像索引建：MAILBOX 下旧图的 present 可能还"占用"着上一份
// 信号量，共用一份会触发复用警告（校验层 VUID-vkQueueSubmit-pSignalSemaphores）
void VulkanContext::CreatePresentSemaphores() {
    for (VkSemaphore s : renderFinished_)
        if (s) vkDestroySemaphore(device, s, nullptr);
    renderFinished_.clear();
    renderFinished_.resize(swapchainImages.size());
    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    for (VkSemaphore& s : renderFinished_)
        VK_CHECK(vkCreateSemaphore(device, &semInfo, nullptr, &s));
}

VulkanContext::VulkanContext(const VulkanInitConfig& config, NativeWindowHandle borrowedWindow,
                             const VkSwapchainCreateInfoKHR& swapchainConfig)
    : hwnd(borrowedWindow), swapchainConfig_(swapchainConfig) {
    VulkanSurfacePlatform::Validate(hwnd);
    try {
        InitializeVulkan(config);
        ++contextCount;
        counted_ = true;
        CreateSurfaceAssets();
    } catch (...) { DestroyAssets(); throw; }
}

void VulkanContext::CreateSurfaceAssets() {
    surface = VulkanSurfacePlatform::Create(instance, hwnd);

    if (device == VK_NULL_HANDLE)
    {
        // 首窗口：直接用本窗口的 surface 选卡——不需要任何引导窗口。
        std::vector<const char*> requiredExtensions = initConfigSnapshot.deviceExtensions;
        requiredExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        physicalDevice = PickPhysicalDevice(instance, surface, gpuFilter,
                                            requiredExtensions, deviceFeatures,
                                            graphicsFamily, presentFamily);
        VkPhysicalDeviceFeatures supported{};
        vkGetPhysicalDeviceFeatures(physicalDevice,&supported);
        deviceFeatures.dualSrcBlend = supported.dualSrcBlend;
        device = CreateLogicalDevice(physicalDevice, graphicsFamily,
                                     presentFamily, deviceFeatures,
                                     requiredExtensions,
                                     &graphicsQueue, &presentQueue);
        queueSubmit2 = reinterpret_cast<PFN_vkQueueSubmit2>(
            vkGetDeviceProcAddr(device, "vkQueueSubmit2"));
        pipelineBarrier2 = reinterpret_cast<PFN_vkCmdPipelineBarrier2>(
            vkGetDeviceProcAddr(device, "vkCmdPipelineBarrier2"));
        if (!queueSubmit2 || !pipelineBarrier2)
            throw std::runtime_error("Vulkan 1.3 synchronization entry points are unavailable");
        dualSourceBlendEnabled = deviceFeatures.dualSrcBlend == VK_TRUE;
    }

    // present 守门员：后续窗口可能落在另一块显卡驱动的显示器上（混合显卡
    // 的边角场景），这里检查不过就大声失败。
    VkBool32 presentOK = VK_FALSE;
    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, presentFamily, surface,
                                         &presentOK));
    if (!presentOK)
        throw std::runtime_error("所选显卡无法呈现到该窗口");

    VkSurfaceCapabilitiesKHR caps{};
    const auto extent=QuerySwapchainExtent(caps);
    if (!extent)
        throw std::runtime_error("Initial Vulkan surface has no drawable extent");
    CreateSwapchain(caps,*extent);     // Initial window must supply a drawable surface.
    CreateRenderPass();                // 每窗口：renderPass
    CreateFramebuffers();              // 帧循环托管：framebuffers + 命令 + 同步
    CreateFrameLoopAssets();
}

// ---- 交换链及下游重建：等空闲 → 销毁视图/交换链 → 重建 → 格式变了连 renderPass。
// All surface framebuffers/renderPass assets are owned by this GPU context.
bool VulkanContext::RecreateSwapchain() {
    if (frameActive_)
        throw std::logic_error("Cannot rebuild a swapchain while recording a frame");
    if (device == VK_NULL_HANDLE || swapchain == VK_NULL_HANDLE)
        throw std::logic_error("Cannot recreate an uninitialized Vulkan swapchain");
    swapchainDirty_=true;
    VkSurfaceCapabilitiesKHR caps{};
    auto extent=QuerySwapchainExtent(caps);
    if (!extent) return false; // No wait, destruction, or invented dimensions.
    VK_CHECK(vkDeviceWaitIdle(device));
    completedSerial = submittedSerial;       // Report device loss, do not destroy/rebuild as if it succeeded.
    // The surface can change while the GPU drains. Recheck before destruction.
    extent=QuerySwapchainExtent(caps);
    if (!extent) return false;

    for (VkFramebuffer fb : framebuffers_)   // framebuffer 引用着视图，先拆
        if (fb) vkDestroyFramebuffer(device, fb, nullptr);
    framebuffers_.clear();
    for (VkImageView view : swapchainViews)
        if (view) vkDestroyImageView(device, view, nullptr);
    swapchainViews.clear();
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = VK_NULL_HANDLE;

    const VkFormat oldFormat = swapchainFormat;
    CreateSwapchain(caps,*extent);
    if (oldFormat != swapchainFormat) {       // 格式变了 → renderPass 兼容性破坏
        vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
        CreateRenderPass();
    }
    CreateFramebuffers();   // 新尺寸（或新格式 → 新 renderPass）下重包 framebuffer
    CreatePresentSemaphores();   // 图像数量可能变了；已等 idle，拆建安全
    swapchainDirty_ = false;
    return true;
}

void VulkanContext::WaitForFrame() {
    if (frameLoopFailed_) {
        VK_CHECK(vkDeviceWaitIdle(device));
        completedSerial = submittedSerial;
        return;
    }
    // Explicit drain for readback, resource mutation and offscreen operations.
    // Normal frame recording only polls the slot it is about to reuse.
    for (auto& resource : frameResources_)
        if (resource.serial > completedSerial) {
            VK_CHECK(vkWaitForFences(device, 1, &resource.fence, VK_TRUE, UINT64_MAX));
            completedSerial = std::max(completedSerial, resource.serial);
        }
}

void VulkanContext::AbortFrame() noexcept {
    frameLoopFailed_ = true;
    if (!frameActive_) return;
    // Terminal recording error: CPU image-layout/version bookkeeping cannot be
    // rolled back reliably. Drain the acquired semaphore and release the held
    // image by subsequent swapchain destruction, then prohibit further frames.
    auto& resource = frameResources_[frameResourceIndex_];
    VkSemaphoreSubmitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    wait.semaphore = resource.acquired;
    wait.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submit.waitSemaphoreInfoCount = 1;
    submit.pWaitSemaphoreInfos = &wait;
    auto result = VulkanContext::queueSubmit2(graphicsQueue, 1, &submit, VK_NULL_HANDLE);
    if (result == VK_SUCCESS) result = vkQueueWaitIdle(graphicsQueue);
    if (result == VK_SUCCESS) {
        submittedSerial = std::max(submittedSerial, recordingSerial);
        completedSerial = submittedSerial;
    }
    frameActive_ = presentPassActive_ = false;
    recordingSerial = 0;
    swapchainDirty_ = true;
}

void VulkanContext::NotifySurfaceSuspended(bool suspended) {
    if (surfaceSuspended_ == suspended) return;
    surfaceSuspended_ = suspended;
    swapchainDirty_ = true;
}

void VulkanContext::NotifySurfaceWindow(NativeWindowHandle window) {
    if (window == nullptr || window == hwnd) return;
    if (device == VK_NULL_HANDLE)
        throw std::logic_error("Cannot rebind a surface before initialization");
    if (frameActive_)
        throw std::logic_error("Cannot rebind the surface while recording a frame");
    VK_CHECK(vkDeviceWaitIdle(device));
    completedSerial = submittedSerial;
    // 拆除顺序：framebuffer → 视图 → 交换链 → renderPass → surface。
    for (VkFramebuffer fb : framebuffers_)
        if (fb) vkDestroyFramebuffer(device, fb, nullptr);
    framebuffers_.clear();
    for (VkImageView view : swapchainViews)
        if (view) vkDestroyImageView(device, view, nullptr);
    swapchainViews.clear();
    if (swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
    if (renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }
    if (surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
        surface = VK_NULL_HANDLE;
    }
    hwnd = window;
    surfaceSuspended_ = false;
    // 与 CreateSurfaceAssets 相同的重建序列；逻辑设备与帧循环对象跨窗口复用。
    surface = VulkanSurfacePlatform::Create(instance, hwnd);
    VkBool32 presentOK = VK_FALSE;
    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, presentFamily,
                                                  surface, &presentOK));
    if (!presentOK)
        throw std::runtime_error("所选显卡无法呈现到恢复后的窗口");
    VkSurfaceCapabilitiesKHR caps{};
    const auto extent = QuerySwapchainExtent(caps);
    if (!extent)
        throw std::runtime_error("Resumed Vulkan surface has no drawable extent");
    CreateSwapchain(caps, *extent);
    CreateRenderPass();
    CreateFramebuffers();
    CreatePresentSemaphores();
    swapchainDirty_ = false;
}

VulkanContext::FrameContext VulkanContext::BeginCommands() {
    if (frameLoopFailed_) throw std::logic_error("Cannot continue after a Vulkan frame recording failure");
    auto& resource = frameResources_[frameResourceIndex_];
    if (frameActive_)
        throw std::runtime_error("上一帧还没 EndFrame");
    const auto drawable = QueryDrawableExtent();
    if (!drawable.width || !drawable.height)
        return {}; // Do not acquire images from a minimized surface.
    if (swapchainDirty_ || drawable.width != drawableExtentAtCreate.width ||
        drawable.height != drawableExtentAtCreate.height)
        if (!RecreateSwapchain()) return {};
    // Reuse only this slot, and never stall the application for an unfinished
    // frame. A skipped attempt leaves the slot, command buffer, acquire
    // semaphore and serials untouched; the next draw retries the same slot.
    if (resource.serial > completedSerial) {
        VkResult status;
        {
            status = vkGetFenceStatus(device, resource.fence);
        }
        if (status == VK_NOT_READY) {
            return {};
        }
        VK_CHECK(status);
        completedSerial = std::max(completedSerial, resource.serial);
    }

    uint32_t imageIndex = 0;
    VkResult acquired;
    {
        acquired = vkAcquireNextImageKHR(device, swapchain, 0,
            resource.acquired, VK_NULL_HANDLE, &imageIndex);
    }
    // No image was acquired, so the semaphore remains unsignaled and can be
    // reused unchanged. Do not convert presentation backpressure into a wait.
    if (acquired == VK_NOT_READY || acquired == VK_TIMEOUT) {
        return {};
    }
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchainDirty_=true;     // Next frame rebuilds only when the surface is drawable.
        return {};                // valid=false
    }
    if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR)
        throw std::runtime_error("vkAcquireNextImageKHR 失败: " +
                                 std::to_string(static_cast<int>(acquired)));

    frameActive_ = true;
    recordingSerial = submittedSerial + 1;

    // SUBOPTIMAL 在 preTransform 强制 IDENTITY 的安卓形态下是常态（合成器
    // 代为旋转），不触发重建——否则逐帧销毁重建交换链（nubia 实机 3000 次/秒）。
    swapchainDirty_ = false;
    // Keep the fence signaled throughout recording. Reset only immediately
    // before submit: a recording failure must not leave a permanently unsignaled fence.
    VK_CHECK(vkResetCommandBuffer(resource.cmd, 0));
    VkCommandBufferBeginInfo beginInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(resource.cmd, &beginInfo));

    frameActive_ = true;
    currentImageIndex_ = imageIndex;
    return {resource.cmd, imageIndex, swapchainExtent, true, recordingSerial, frameResourceIndex_};
}

void VulkanContext::BeginPresentPass(VkClearColorValue clearColor) {
    auto& resource = frameResources_[frameResourceIndex_];
    if (!frameActive_ || presentPassActive_)
        throw std::logic_error("Invalid presentation pass state");

    VkClearValue clear{};
    clear.color = clearColor;
    VkRenderPassBeginInfo passBegin{
        VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    passBegin.renderPass = renderPass;
    passBegin.framebuffer = framebuffers_[currentImageIndex_];
    passBegin.renderArea.offset = {0, 0};
    passBegin.renderArea.extent = swapchainExtent;
    passBegin.clearValueCount = 1;
    passBegin.pClearValues = &clear;
    vkCmdBeginRenderPass(resource.cmd, &passBegin, VK_SUBPASS_CONTENTS_INLINE);

    // viewport/scissor 恒动态：每帧按当前交换链尺寸设
    VkViewport viewport{};
    viewport.width = static_cast<float>(swapchainExtent.width);
    viewport.height = static_cast<float>(swapchainExtent.height);
    viewport.maxDepth = 1.0F;
    VkRect2D scissor{{0, 0}, swapchainExtent};
    vkCmdSetViewport(resource.cmd, 0, 1, &viewport);
    vkCmdSetScissor(resource.cmd, 0, 1, &scissor);

    presentPassActive_ = true;
}

void VulkanContext::EndFrame() {
    auto& resource = frameResources_[frameResourceIndex_];
    if (!frameActive_)
        return;                   // 交换链过期那帧没活可干；重复调用也安全
    if (!presentPassActive_)
        throw std::logic_error("A frame must have a presentation pass before submit");

    vkCmdEndRenderPass(resource.cmd);
    presentPassActive_ = false;
    // Make the final layout transition an explicit release before signaling
    // renderFinished. The presentation engine reads the whole swapchain image.
    VkImageMemoryBarrier2 release{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    release.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    release.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    release.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
    release.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    release.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    release.srcQueueFamilyIndex = release.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    release.image = swapchainImages[currentImageIndex_];
    release.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    VkDependencyInfo dependency{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &release;
    VulkanContext::pipelineBarrier2(resource.cmd, &dependency);
    VK_CHECK(vkEndCommandBuffer(resource.cmd));

    // 提交：等 imageAvailable（等到才能往这幅图上画）→ 执行 → 交出本图的
    // 呈现信号量（每图一份，防 MAILBOX 下呈现引擎还占用旧信号量）
    VkSemaphoreSubmitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    wait.semaphore = resource.acquired;
    wait.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    signal.semaphore = renderFinished_[currentImageIndex_];
    // Include the explicit present-layout release, not only color writes.
    signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    VkCommandBufferSubmitInfo commands{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    commands.commandBuffer = resource.cmd;
    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submit.waitSemaphoreInfoCount = 1;
    submit.pWaitSemaphoreInfos = &wait;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &commands;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &signal;
    {
        VK_CHECK(vkResetFences(device, 1, &resource.fence));
        VK_CHECK(VulkanContext::queueSubmit2(graphicsQueue, 1, &submit, resource.fence));
    }
    resource.serial = submittedSerial = recordingSerial;
    recordingSerial = 0;
    frameActive_ = false;

    // 呈现：本图的 renderFinished 一到就把这幅图换上屏
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinished_[currentImageIndex_];
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain;
    uint32_t imageIndex = currentImageIndex_;
    present.pImageIndices = &imageIndex;
    VkResult result;
    {
        result = vkQueuePresentKHR(presentQueue, &present);
    }
    // 仅 OUT_OF_DATE 触发重建；SUBOPTIMAL 在安卓强制 IDENTITY（合成器旋转）
    // 形态下每次 present 都返回，当作正常状态——标脏即逐帧重建风暴，
    // 且 VK_CHECK 拒收 SUBOPTIMAL 会直接致命（"result failed: 1000001003"）。
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
        swapchainDirty_ = true;
    else if (result != VK_SUBOPTIMAL_KHR)
        VK_CHECK(result);
    frameResourceIndex_ = (frameResourceIndex_ + 1) % kFramesInFlight;
}

VulkanContext::~VulkanContext() { DestroyAssets(); }

void VulkanContext::DestroyAssets() noexcept {
    // Also called after partial initialization fails; no fallback rendering.
    if (frameActive_) AbortFrame();
    if (device) vkDeviceWaitIdle(device);   // 等 GPU 把在途帧画完（托管帧资产还在用）
    for (auto& resource : frameResources_) {
        if (resource.pool) vkDestroyCommandPool(device, resource.pool, nullptr);
        if (resource.acquired) vkDestroySemaphore(device, resource.acquired, nullptr);
        if (resource.fence) vkDestroyFence(device, resource.fence, nullptr);
    }

    for (VkFramebuffer fb : framebuffers_)
        if (fb) vkDestroyFramebuffer(device, fb, nullptr);
    for (VkSemaphore semaphore : renderFinished_)
        if (semaphore) vkDestroySemaphore(device, semaphore, nullptr);
    for (VkImageView view : swapchainViews)
        if (view) vkDestroyImageView(device, view, nullptr);
    if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
    if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);
    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);

    if (counted_) { --contextCount; counted_ = false; }
    if (contextCount == 0) {
        if (device) vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
        queueSubmit2 = nullptr;
        pipelineBarrier2 = nullptr;
        dualSourceBlendEnabled = false;
        physicalDevice = VK_NULL_HANDLE;
        graphicsQueue = presentQueue = VK_NULL_HANDLE;
        if (debugMessenger) {
            auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroy) destroy(instance, debugMessenger, nullptr);
            debugMessenger = VK_NULL_HANDLE;
        }
        if (instance) vkDestroyInstance(instance, nullptr);
        instance = VK_NULL_HANDLE;
        appInitialized = false;
    }
}
