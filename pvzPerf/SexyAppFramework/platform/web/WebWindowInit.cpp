#include "SexyAppBase.h"
#include "platform/NativeWindow.h"
#include "graphics/GpuBackend.h"
#include "graphics/GameViewport.h"
#include "graphics/DDInterface.h"
#include "graphics/RenderInterface.h"
#include "widget/WidgetManager.h"
#include <stdexcept>
using namespace Sexy;
void SexyAppBase::MakeWindow() {
    if (mNativeWindow || mGpuContext) throw std::logic_error("Browser game window already exists");
    NativeWindowConfig config;
    config.title = mTitle;
    config.width = GameViewport::MaximumWidth;
    config.height = mHeight;
    config.visible = false;
    config.callback = QueueNativeWindowEvent;
    config.callbackContext = this;
    mNativeWindow = std::make_unique<NativeWindow>(config);
    mHWnd = mNativeWindow.get();
    mGpuContext = std::make_unique<GpuContext>(mHWnd, false, false);
    mUseExternalHWND = false;
    mExternalHWND = nullptr;
    mIsPhysWindowed = true;
    mDDInterface = new DDInterface(this);
    if (InitDDInterface() != DDInterface::RESULT_OK) throw std::runtime_error("WebGL display initialization failed");
    mWidgetManager->mImage = mDDInterface->mScreenImage;
    mWidgetManager->MarkAllDirty();
    mActive = mNativeWindow->Focused;
    mPhysMinimized = mMinimized = false;
    mNativeWindowReady = true;
    RehupFocus();
    EnforceCursor();
}
