#include "SexyAppBase.h"
#include "platform/NativeWindow.h"
#include "graphics/GpuBackend.h"
#include "graphics/GameViewport.h"
#include "graphics/DDInterface.h"
#include "graphics/RenderInterface.h"
#include "widget/WidgetManager.h"
#include <cstdlib>
#include <stdexcept>
#ifdef __ANDROID__
#include "platform/android/AndroidWindow.h"
#include <android/log.h>
#endif
using namespace Sexy;
void SexyAppBase::MakeWindow()
{
    if (mNativeWindow || mGpuContext)
        throw std::logic_error("Linux game window already exists");
#if !defined(__ANDROID__)
    if (mScaleHDREnabled)
        throw std::logic_error("Linux HDR output is not implemented");
#endif
    NativeWindowConfig config;
    config.title = mTitle;
    config.width = GameViewport::MaximumWidth;
    config.height = mHeight;
    config.visible = false;
    config.callback = QueueNativeWindowEvent;
    config.callbackContext = this;
    mNativeWindow = std::make_unique<NativeWindow>(config);
#if defined(__ANDROID__)
    // Android：NativeWindowHandle 是裸 ANativeWindow*，由 android_main 在
    // APP_CMD_INIT_WINDOW 挂接；窗口本体归 native_app_glue 所有，不可重建。
    mHWnd = AttachedAndroidNativeWindow();
#else
    mHWnd = mNativeWindow.get(); // NativeWindowHandle, never a Win32 HWND.
#endif
    VulkanInitConfig init{};
    init.appInfo.pApplicationName = "pvz_perf";
    init.enableValidation = std::getenv("PVZ_VULKAN_VALIDATION") != nullptr;
    if (!mScaleEffects.empty()) {
        init.features.shaderStorageImageExtendedFormats = VK_TRUE;
        init.features.shaderImageGatherExtended = VK_TRUE;
    }
    VkSwapchainCreateInfoKHR swapchain{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
        .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        // MAILBOX：循环不锁帧（菜单动画持续渲染，按用户要求保持不锁；
        // 2026-10-27 FIFO 实验在均衡模式下反而劣化到 ~21fps 上屏，已回退）。
        .presentMode = VK_PRESENT_MODE_MAILBOX_KHR,
        .clipped = VK_TRUE
    };
#if defined(__ANDROID__)
#if defined(PVZ_ANDROID_HDR_TEST)
    mScaleHDREnabled=true;
    mScaleHDRPaperWhite=80.0f;
    mScaleHDRMaxNits=420.0f;
    mScaleHDRReinhardPaperNits=150.0f;
    mScaleHDRStrength=1.0f;
    init.extraExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
    swapchain.imageFormat=VK_FORMAT_R16G16B16A16_SFLOAT;
    swapchain.imageColorSpace=VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
#else
    // Android HDR calibration is not ready for the v3.3 release.
    mScaleHDREnabled = false;
#endif
#endif
    mGpuContext = std::make_unique<GpuContext>(init, mHWnd, swapchain);
#if defined(PVZ_ANDROID_HDR_TEST)
    __android_log_print(ANDROID_LOG_INFO,"pvz_hdr_test","swapchain format=%u colorSpace=%u; paper=80 peak=420 nits",
        unsigned(mGpuContext->swapchainFormat),unsigned(mGpuContext->swapchainColorSpace));
#endif
    mUseExternalHWND = false;
    mExternalHWND = nullptr;
    mIsPhysWindowed = true;
    mDDInterface = new DDInterface(this);
    if (InitDDInterface() != DDInterface::RESULT_OK)
        throw std::runtime_error("Linux Vulkan display initialization failed");
    mWidgetManager->mImage = mDDInterface->mScreenImage;
    mWidgetManager->MarkAllDirty();
    mActive = mNativeWindow->Focused;
    mPhysMinimized = mMinimized = false;
    mNativeWindowReady = true;
    RehupFocus();
    EnforceCursor();
}
