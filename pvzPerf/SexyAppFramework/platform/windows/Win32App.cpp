#include "SexyAppBase.h"
#include "graphics/DDInterface.h"
#include "graphics/Image.h"
#include "graphics/RenderInterface.h"
#include "graphics/GpuBackend.h"
#include "graphics/GameViewport.h"
#include "widget/WidgetManager.h"
#include "misc/StableTime.h"
#include "misc/Debug.h"
#include "misc/FrameCounterMath.h"
#include "misc/DemoDialogTemplate.h"
#include "misc/DemoJumpTime.h"
#include "WindowApi.h"
#include <windowsx.h>
#include <mmsystem.h>
#include <regstr.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

using namespace Sexy;

void SexyAppBase::RequestNativeClose() {
    SendMessageW(mHWnd, WM_CLOSE, 0, 0);
}

namespace {
constexpr UINT NativeTouchMessage = WM_APP + 0x305;
constexpr unsigned int TouchCancel = 0x80000000u;
constexpr unsigned int TouchCancelAll = 0x40000000u;
bool TouchPromotedMouse() {
    return (static_cast<ULONG_PTR>(GetMessageExtraInfo()) & 0xFFFFFF80u) == 0xFF515780u;
}
constexpr char MainWindowClass[] = "pvz_perf";
constexpr char HiddenWindowClass[] = "pvz_perf_hidden";
[[noreturn]] void WindowFailure(const char* operation) {
    throw std::runtime_error(std::string(operation)+" failed: "+std::to_string(GetLastError()));
}
RECT ClientRect(HWND window) {
    RECT rect{};
    if(!GetClientRect(window,&rect)) WindowFailure("GetClientRect");
    return rect;
}
WindowTrackLimits WindowClientLimits(const SexyAppBase& app) {
    int height=600;
    if(app.mModernScalePrepared && app.IsScalePresentActiveForMode(true) &&
        app.mDDInterface && app.mDDInterface->mRenderInterface &&
        app.mDDInterface->mRenderInterface->mRenderer) {
        const auto output=app.mDDInterface->mRenderInterface->mRenderer->ScaleOutput(false);
        if(output.height>0) height=int(output.height);
    }
    return WindowTrackLimits::FromScaleOutputHeight(height);
}
Point LogicalMouse(SexyAppBase& app,double x,double y) {
    int sx=0,sy=0;
    app.MapWindowMouse(x,y,sx,sy);
    return Point(sx,sy);
}
void SetStyle(HWND window,int index,LONG_PTR style) {
    SetLastError(ERROR_SUCCESS);
    const auto previous=SetWindowLongPtrW(window,index,style);
    if(!previous && GetLastError()!=ERROR_SUCCESS) WindowFailure("SetWindowLongPtr");
}
// The application owns one main native window; MakeWindow resets this state.
GameViewport nativeStartupViewport;


}

void SexyAppBase::MapWindowMouse(double x,double y,int& sx,int& sy) const {
    const auto client=ClientRect(mHWnd);
    const int width=client.right,height=client.bottom;
    if(width<=0 || height<=0) {sx=sy=-1;return;}
    const auto rect=mDDInterface->mRenderInterface->mRenderer->PresentRect(width,height);
    const auto point=WindowPointToScene(x,y,width,height,width,height,rect,
        mScreenBounds.mWidth,mScreenBounds.mHeight);
    sx=point.x+mScreenBounds.mX;sy=point.y+mScreenBounds.mY;
}

void SexyAppBase::MakeWindow() {
    if(mHWnd || mGpuContext) throw std::logic_error("Game window already exists");
    if constexpr (!NativeWindowsWindowApi::HdrOutput) mScaleHDREnabled=false;
    mTitle="pvz_perf";
    const DWORD style=WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|
        WS_THICKFRAME;
    const DWORD exStyle=WS_EX_APPWINDOW;
    const POINT preferred{mPreferredX < 0 ? 0 : mPreferredX,
        mPreferredY < 0 ? 0 : mPreferredY};
    const auto monitor = MonitorFromPoint(preferred, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO monitorInfo{sizeof(monitorInfo)};
    if(!monitor || !GetMonitorInfoW(monitor,&monitorInfo)) WindowFailure("GetMonitorInfo(startup)");
    const auto& display = monitorInfo.rcMonitor;
    nativeStartupViewport = GameViewport::Fit(display.right-display.left,display.bottom-display.top);
    RECT bounds{0,0,nativeStartupViewport.width,nativeStartupViewport.height};
    if(!NativeWindowsWindowApi::AdjustClientRect(bounds,style,exStyle,nullptr))
        WindowFailure("AdjustClientRect");
    mHWnd=CreateWindowExA(exStyle,MainWindowClass,mTitle.c_str(),style,
        mPreferredX<0?CW_USEDEFAULT:mPreferredX,mPreferredY<0?CW_USEDEFAULT:mPreferredY,
        bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,gHInstance,this);
    if(!mHWnd) WindowFailure("CreateWindowEx");
#if defined(PVZ_GPU_D3D9)
    mScaleHDREnabled=false;
    mGpuContext=std::make_unique<GpuContext>(mHWnd,false,false);
#elif defined(PVZ_GPU_OPENGL)
    mScaleHDREnabled=false;
    mGpuContext=std::make_unique<GpuContext>(mHWnd,
        getenv("PVZ_OPENGL_VALIDATION")!=nullptr,false);
#elif defined(PVZ_GPU_D3D12)
    mGpuContext=std::make_unique<GpuContext>(mHWnd,
        getenv("PVZ_VULKAN_VALIDATION")!=nullptr,mScaleHDREnabled);
#else
    VulkanInitConfig init{};
    init.appInfo.pApplicationName="pvz_perf";
    init.enableValidation=getenv("PVZ_VULKAN_VALIDATION")!=nullptr;
    // 不再要求 shaderFloat64：字体内核已是纯整数实现，fp64 是 NVIDIA 有而
    // Intel 消费卡（Arc 核显）没有的特性，曾被此行全灭候选卡。
    if(!mScaleEffects.empty()) {
        init.features.shaderStorageImageExtendedFormats=VK_TRUE;
        init.features.shaderImageGatherExtended=VK_TRUE;
    }
    VkSwapchainCreateInfoKHR swapchain{
        .sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .imageFormat=VK_FORMAT_B8G8R8A8_UNORM,
        .imageColorSpace=VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        .imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode=VK_PRESENT_MODE_MAILBOX_KHR,
        .clipped=VK_TRUE
    };
    if(mScaleHDREnabled) {
        init.extraExtensions.push_back(VK_EXT_SWAPCHAIN_COLOR_SPACE_EXTENSION_NAME);
        swapchain.imageFormat=VK_FORMAT_R16G16B16A16_SFLOAT;
        swapchain.imageColorSpace=VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
    }
    mGpuContext=std::make_unique<GpuContext>(init,mHWnd,swapchain);
    mScaleHDREnabled = mScaleHDREnabled &&
        mGpuContext->swapchainColorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
#endif
    ResolveHDRAutoParams();
    mUseExternalHWND=false;mExternalHWND=nullptr;mIsPhysWindowed=true;
    mDDInterface=new DDInterface(this);
    if(InitDDInterface()!=DDInterface::RESULT_OK)
        throw std::runtime_error("Native Vulkan display initialization failed");
    mWidgetManager->mImage=mDDInterface->mScreenImage;
    mWidgetManager->MarkAllDirty();
    mActive=GetFocus()==mHWnd;
    mPhysMinimized=mMinimized=false;
    mNativeWindowReady=true;
#if _WIN32_WINNT < 0x0602
    if (!RegisterTouchWindow(mHWnd, 0)) WindowFailure("RegisterTouchWindow");
#endif
    RehupFocus();
    EnforceCursor();
}

void SexyAppBase::RegisterWindowClasses() {
    mIsWideWindow=sizeof(SexyChar)==sizeof(wchar_t);
    WNDCLASSA windowClass{};
    windowClass.style=CS_DBLCLKS;
#if defined(PVZ_GPU_OPENGL)
    windowClass.style|=CS_OWNDC;
#endif
    windowClass.hIcon=LoadIconA(gHInstance,"IDI_MAIN_ICON");
    windowClass.hInstance=gHInstance;
    windowClass.lpfnWndProc=WindowProc;
    windowClass.lpszClassName=MainWindowClass;
    if(!RegisterClassA(&windowClass)) WindowFailure("RegisterClass(pvz_perf)");
    windowClass.style=0;windowClass.hIcon=nullptr;
    windowClass.lpszClassName=HiddenWindowClass;
    if(!RegisterClassA(&windowClass)) WindowFailure("RegisterClass(pvz_perf_hidden)");
    mInvisHWnd=CreateWindowExA(0,HiddenWindowClass,mTitle.c_str(),0,0,0,0,0,
        nullptr,nullptr,gHInstance,nullptr);
    if(!mInvisHWnd) WindowFailure("CreateWindowEx(pvz_perf_hidden)");
    SetStyle(mInvisHWnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(this));
}

void SexyAppBase::PumpWindowEvents() {
    MSG message{};
    // Always drain the OS queue, including mouse movement. Hover is sampled
    // separately, but queued input must not remain pending on the window thread.
    while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) {
        if(message.message==WM_QUIT) {
            Shutdown();
            break;
        }
        if(message.hwnd==mHWnd &&
            (message.message==WM_MOUSEMOVE || message.message==WM_MOUSELEAVE)) {
            if (message.message == WM_MOUSEMOVE && !TouchPromotedMouse()) {
                mNativeMouseClientPosition = {GET_X_LPARAM(message.lParam), GET_Y_LPARAM(message.lParam)};
                mNativeMouseMotionPending = true;
            }
            continue;
        }
        TranslateMessage(&message);
        DispatchMessageW(&message);
        if(mWindowMessageError)
            std::rethrow_exception(std::exchange(mWindowMessageError,{}));
    }
    if(mWindowMessageError)
        std::rethrow_exception(std::exchange(mWindowMessageError,{}));
    if(mShutdown || !mNativeWindowReady) return;
    if(mNativeTopologyPending) {
        UpdateMonitorState();
    }
    {
        UpdateHdrDisplay();
    }
    if(mNativeResizePending && !mPhysMinimized && !mSwitchingScreenMode) {
        UpdateWindowGeometry();
    }
}

HMONITOR SexyAppBase::TargetMonitor() const {
    const auto monitor=MonitorFromWindow(mHWnd,MONITOR_DEFAULTTONEAREST);
    if(!monitor) WindowFailure("MonitorFromWindow");
    return monitor;
}

DEVMODEW SexyAppBase::TargetVideoMode() const {
    MONITORINFOEXW info{};
    info.cbSize=sizeof(info);
    if(!GetMonitorInfoW(TargetMonitor(),&info)) WindowFailure("GetMonitorInfo");
    DEVMODEW mode{};
    mode.dmSize=sizeof(mode);
    if(!EnumDisplaySettingsW(info.szDevice,ENUM_CURRENT_SETTINGS,&mode))
        WindowFailure("EnumDisplaySettings");
    return mode;
}

void SexyAppBase::ResizeWindowClient(int width,int height) {
    if(width<=0 || height<=0) throw std::invalid_argument("Invalid window client size");
    if(mNativeFullscreen) throw std::logic_error("Cannot resize a fullscreen client as a window");
    const auto limits=WindowClientLimits(*this);
    // WarmScalePipelinesBeforeShow resizes the hidden window a second time.
    // Keep the chosen monitor ratio while applying its prepared output height.
    if(!mNativeWindowShown)
        width=WindowStartupSize::FromMonitor(
            nativeStartupViewport.width,nativeStartupViewport.height,limits.height).width;
    width=limits.ConstrainWidth(width);
    height=limits.height;
    RECT rect{0,0,width,height};
    const auto style=DWORD(GetWindowLongPtrW(mHWnd,GWL_STYLE));
    const auto exStyle=DWORD(GetWindowLongPtrW(mHWnd,GWL_EXSTYLE));
    if(!NativeWindowsWindowApi::AdjustClientRect(rect,style,exStyle,mHWnd))
        WindowFailure("AdjustClientRect");
    if(!SetWindowPos(mHWnd,nullptr,0,0,rect.right-rect.left,rect.bottom-rect.top,
        SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE)) WindowFailure("SetWindowPos");
    mNativeResizePending=true;
}

void SexyAppBase::SetWindowFullscreen(bool fullscreen) {
    if(fullscreen==mNativeFullscreen) return;
    const auto currentStyle=GetWindowLongPtrW(mHWnd,GWL_STYLE);
    const auto visibility=currentStyle&WS_VISIBLE;
    if(fullscreen) {
        MONITORINFO info{sizeof(info)};
        if(!GetMonitorInfoW(TargetMonitor(),&info)) WindowFailure("GetMonitorInfo");
        if(!GetWindowRect(mHWnd,&mNativeSavedWindowRect)) WindowFailure("GetWindowRect");
        mNativeSavedWindowStyle=currentStyle;
        mNativeSavedWindowExStyle=GetWindowLongPtrW(mHWnd,GWL_EXSTYLE);
        SetStyle(mHWnd,GWL_STYLE,WS_POPUP|WS_CLIPSIBLINGS|WS_CLIPCHILDREN|visibility);
        SetStyle(mHWnd,GWL_EXSTYLE,WS_EX_APPWINDOW);
        const auto& rect=info.rcMonitor;
        if(!SetWindowPos(mHWnd,HWND_TOP,rect.left,rect.top,rect.right-rect.left,
            rect.bottom-rect.top,SWP_FRAMECHANGED|SWP_NOACTIVATE)) WindowFailure("SetWindowPos");
    }else{
        SetStyle(mHWnd,GWL_STYLE,(mNativeSavedWindowStyle&~LONG_PTR(WS_VISIBLE))|visibility);
        SetStyle(mHWnd,GWL_EXSTYLE,mNativeSavedWindowExStyle);
        const auto& rect=mNativeSavedWindowRect;
        if(!SetWindowPos(mHWnd,nullptr,rect.left,rect.top,rect.right-rect.left,
            rect.bottom-rect.top,SWP_FRAMECHANGED|SWP_NOZORDER|SWP_NOACTIVATE))
            WindowFailure("SetWindowPos");
    }
    mNativeFullscreen=fullscreen;
    mNativeResizePending=true;
    mHdrTargetPending=true;
}

void SexyAppBase::UpdateHdrDisplay() {
    if constexpr (NativeWindowsWindowApi::HdrOutput) {
        UpdateWindowsHdr(mNativeHdrState,*this,[this] { return TargetMonitor(); });
    }
}

void SexyAppBase::PrepareFullscreenScaleForMonitor() {
    if(!mModernScalePrepared) return;
    const auto mode=TargetVideoMode();
    if(int(mode.dmPelsWidth)==mModernScaleTargetW && int(mode.dmPelsHeight)==mModernScaleTargetH) return;
    std::vector<ScaleEffectRuntimeDesc*> effects;
    std::vector<ScaleEffectScaling> scaling;
    if(mScaleFullscreenConfig.mDeclared && !BuildScalePipelineForMode(false,effects,scaling))
        throw std::runtime_error("Cannot resolve resident fullscreen pipeline");
    std::vector<GpuScaleStep> steps;
    for(size_t i=0;i<effects.size();++i) steps.push_back({effects[i],scaling[i]});
    auto& renderer=*mDDInterface->mRenderInterface->mRenderer;
    renderer.PrepareScale(true,steps,mode.dmPelsWidth,mode.dmPelsHeight);
    renderer.WarmScale(true);
    mModernScaleTargetW=int(mode.dmPelsWidth);mModernScaleTargetH=int(mode.dmPelsHeight);
}

void SexyAppBase::UpdateMonitorState() {
    if(mSwitchingScreenMode) return;
    mSwitchingScreenMode=true;
    struct Reset {bool& flag;~Reset(){flag=false;}} reset{mSwitchingScreenMode};
    auto& render=*mDDInterface->mRenderInterface;
    render.Flush();
    if(mNativeFullscreen) {
        MONITORINFO info{sizeof(info)};
        if(!GetMonitorInfoW(TargetMonitor(),&info)) WindowFailure("GetMonitorInfo");
        const auto& rect=info.rcMonitor;
        if(!SetWindowPos(mHWnd,nullptr,rect.left,rect.top,rect.right-rect.left,
            rect.bottom-rect.top,SWP_NOZORDER|SWP_NOACTIVATE)) WindowFailure("SetWindowPos");
    }
    PrepareFullscreenScaleForMonitor();
    render.mRenderer->SetScaleMode(IsScalePresentActive(),!mIsWindowed);
    UpdateWindowGeometry();
    mNativeTopologyPending=false;
    mWidgetManager->MarkAllDirty();
    ClearUpdateBacklog(false);
}

void SexyAppBase::UpdateWindowGeometry() {
    mNativeResizePending=true;
    auto& render=*mDDInterface->mRenderInterface;
    if(render.mSceneBegun) render.Flush();
    const int prepared=mDDInterface->PrepareScreenMode(mHWnd,mIsWindowed);
    if(prepared==DDInterface::RESULT_DEFERRED) return;
    if(prepared!=DDInterface::RESULT_OK)
        throw NativeDisplayError(NativeDisplayOperation::PrepareScreenMode,prepared);
    const int resized=mDDInterface->ResizePreparedPresent();
    if(resized==DDInterface::RESULT_DEFERRED) return;
    if(resized!=DDInterface::RESULT_OK)
        throw NativeDisplayError(NativeDisplayOperation::ResizePreparedPresent,resized);
    mNativeResizePending=false;
    mWidgetManager->Resize(mScreenBounds,mDDInterface->mVisibleGameRect);
    mWidgetManager->MarkAllDirty();
}

void SexyAppBase::SwitchScreenMode(bool windowed,bool force) {
    if(mSwitchingScreenMode) return;
    if(mForceWindowed) windowed=true;
    else if(mForceFullscreen) windowed=false;
    if(mIsWindowed==windowed && !force) return;
    mSwitchingScreenMode=true;
    struct Reset {bool& flag;~Reset(){flag=false;}} reset{mSwitchingScreenMode};
    ReleaseCursorClip();
    mWidgetManager->CancelMouseDownState();ClearKeysDown();
    auto& render=*mDDInterface->mRenderInterface;
    render.Flush();
    auto& renderer=*render.mRenderer;
    if(!windowed) PrepareFullscreenScaleForMonitor();
    SetWindowFullscreen(!windowed);
    if(windowed) {
        const auto scaled=renderer.ScaleOutput(false);
        ResizeWindowClient(IsScalePresentActiveForMode(true)?int(scaled.width):renderer.sceneWidth,
            IsScalePresentActiveForMode(true)?int(scaled.height):renderer.sceneHeight);
    }
    renderer.SetScaleMode(IsScalePresentActiveForMode(windowed),!windowed);
    mIsWindowed=windowed;mIsPhysWindowed=true;
    UpdateWindowGeometry();
    UpdateHdrDisplay();
    render.PresentRetainedFrame();
    ClearUpdateBacklog(false);
}

POINT SexyAppBase::ReadNativeMousePosition() {
    POINT position{};
    if(!GetCursorPos(&position)) WindowFailure("GetCursorPos");
    if(!ScreenToClient(mHWnd,&position)) WindowFailure("ScreenToClient");
    return position;
}
void SexyAppBase::SampleNativeMousePosition() {
    if(mPlayingDemoBuffer || !mActive || mPhysMinimized || mSwitchingScreenMode) return;
    if(!mNativeWindowShown) return;
    if (mNativeTouchOwnsCursor && !mNativeMouseMotionPending) return;
    const auto position = mNativeMouseMotionPending ? mNativeMouseClientPosition : ReadNativeMousePosition();
    mNativeMouseMotionPending = false;
    if(mRecordingDemoBuffer && !mShutdown) {
        const int dx=position.x-mLastDemoMouseX,dy=position.y-mLastDemoMouseY;
        if(std::abs(dx)<32 && std::abs(dy)<32) {
            if(dx || dy) {
                WriteDemoTimingBlock();
                mDemoBuffer.WriteNumBits(1,1);
                mDemoBuffer.WriteNumBits(0,1);
                mDemoBuffer.WriteNumBits(dx,6);
                mDemoBuffer.WriteNumBits(dy,6);
            }
        } else {
            WriteDemoTimingBlock();
            mDemoBuffer.WriteNumBits(0,1);
            mDemoBuffer.WriteNumBits(DEMO_MOUSE_POSITION,5);
            mDemoBuffer.WriteNumBits(position.x,12);
            mDemoBuffer.WriteNumBits(position.y,12);
        }
        mLastDemoMouseX=position.x;mLastDemoMouseY=position.y;
    }
    {
        NativeMouseMove(position.x,position.y);
    }
    // Client WM_SETCURSOR is suppressed; the heartbeat owns native application.
    if(mNativeWindowShown && (mMouseIn || GetCapture()==mHWnd)) {
        ::SetCursor(mNativeAppliedCursor);
    }
}
void SexyAppBase::NativeMouseMove(double x,double y) {
    if(mSwitchingScreenMode) return;
    mNativeTouchOwnsCursor = false;
    if(mNativeResizePending && !mPhysMinimized) UpdateWindowGeometry();
    const auto pos=LogicalMouse(*this,x,y);
    const auto client=ClientRect(mHWnd);
    const bool inside=x>=0 && y>=0 && x<client.right && y<client.bottom &&
        mScreenBounds.Contains(pos.mX,pos.mY);
    const bool moved=pos.mX!=mWidgetManager->mLastMouseX ||
        pos.mY!=mWidgetManager->mLastMouseY || inside!=mMouseIn;
    mMouseIn=inside;
    mDDInterface->mNextCursorX = pos.mX;
    mDDInterface->mNextCursorY = pos.mY;
    mDDInterface->mCursorX = pos.mX;
    mDDInterface->mCursorY = pos.mY;
    if(inside || mWidgetManager->mDownButtons) mWidgetManager->MouseMove(pos.mX,pos.mY);
    else {mWidgetManager->mLastMouseX=pos.mX;mWidgetManager->mLastMouseY=pos.mY;}
    if(!inside) mWidgetManager->MouseExit(pos.mX,pos.mY);
    EnforceCursor();
    if(moved) mLastUserInputTick=SexyGetTickCount();
}
void SexyAppBase::QueueNativeTouch(unsigned int id, unsigned int flags, double x, double y) {
    if (!mNativeWindowReady || mSwitchingScreenMode) return;
    mNativeTouchOwnsCursor = true;
    const auto point = LogicalMouse(*this, x, y);
    if ((flags & TOUCHEVENTF_DOWN) && !mScreenBounds.Contains(point.mX, point.mY)) return;
    MSG message{};
    message.hwnd = mHWnd;
    message.message = NativeTouchMessage;
    message.wParam = WPARAM(id) | (WPARAM(flags) << 32);
    message.lParam = LPARAM(static_cast<unsigned int>(point.mX)) |
        LPARAM(static_cast<unsigned long long>(static_cast<unsigned int>(point.mY)) << 32);
    mDeferredMessages.push_back(message);
    mNativeTouchOwnsCursor = true;
}
void SexyAppBase::NativeMouseLeave() {
    mMouseIn=false;
    mWidgetManager->MouseExit(mWidgetManager->mLastMouseX,mWidgetManager->mLastMouseY);
    EnforceCursor();
}
void SexyAppBase::NativeMouseButton(int button,bool pressed,double x,double y,double time) {
    if(button<0 || button>2) throw std::invalid_argument("Unknown mouse button");
    if(mSwitchingScreenMode) return;
    const unsigned mask=button==0?1u:button==1?2u:4u;
    if(!pressed && !((mWidgetManager->mActualDownButtons|mWidgetManager->mDownButtons)&mask)) return;
    NativeMouseMove(x,y);
    if(!mMouseIn) {
        if(!pressed) {
            const auto pos=LogicalMouse(*this,x,y);
            mWidgetManager->MouseUp(std::clamp(pos.mX,0,mWidth-1),
                std::clamp(pos.mY,0,mHeight-1),button==1?-1:button==2?3:1);
            mWidgetManager->CancelMouseDownState();
            if(GetCapture()==mHWnd) ReleaseCapture();
        }
        return;
    }
    const auto pos=LogicalMouse(*this,x,y);
    int count=button==1?-1:button==2?3:1;
    if(pressed) {
        if(button!=2 && time>=mNativeLastClickTime[button] &&
            time-mNativeLastClickTime[button]<=GetDoubleClickTime()/1000.0 &&
            std::abs(x-mNativeLastClickX[button])<=GetSystemMetrics(SM_CXDOUBLECLK)/2.0 &&
            std::abs(y-mNativeLastClickY[button])<=GetSystemMetrics(SM_CYDOUBLECLK)/2.0) {
            count*=2;mNativeLastClickTime[button]=-1000;
        }else mNativeLastClickTime[button]=time;
        mNativeLastClickX[button]=x;mNativeLastClickY[button]=y;
        if(mNativeWindowShown) SetCapture(mHWnd);
        mWidgetManager->MouseDown(pos.mX,pos.mY,count);
    }else{
        mWidgetManager->MouseUp(pos.mX,pos.mY,count);
        if(!mWidgetManager->mDownButtons && GetCapture()==mHWnd) ReleaseCapture();
    }
    EnforceCursor();
}
void SexyAppBase::NativeMouseWheel(int delta) {
    mNativeWheelRemainder+=delta/double(WHEEL_DELTA);
    const int steps=int(mNativeWheelRemainder);
    mNativeWheelRemainder-=steps;
    if(steps) mWidgetManager->MouseWheel(steps);
}
void SexyAppBase::NativeFocusChanged(bool focused) {
    mActive=focused;
    if(focused) mHdrTargetPending=true;
    else {
        mWidgetManager->CancelTouches();
        mWidgetManager->CancelMouseDownState();mNativeLastClickTime.fill(-1000);
        ClearKeysDown();ReleaseCursorClip();NativeMouseLeave();
    }
    RehupFocus();
}

void SexyAppBase::EnforceCursor() { UpdateNativeCursorSelection(); }
void SexyAppBase::UpdateNativeCursorSelection() {
    if(!mHWnd) return;
    const int cursor=(mSEHOccured || !mMouseIn)?CURSOR_POINTER:mCursorNum;
    const bool overrideCursor=mMouseIn && !mSEHOccured && mOverrideCursor;
    const bool hidden=!overrideCursor && (cursor==CURSOR_NONE || cursor==CURSOR_CUSTOM);
    LPCWSTR resource=reinterpret_cast<LPCWSTR>(IDC_ARROW);
    int index=0;
    if(!overrideCursor) switch(cursor) {
    case CURSOR_HAND:resource=reinterpret_cast<LPCWSTR>(IDC_HAND);index=1;break;
    case CURSOR_TEXT:resource=reinterpret_cast<LPCWSTR>(IDC_IBEAM);index=2;break;
    case CURSOR_DRAGGING:case CURSOR_SIZEALL:resource=reinterpret_cast<LPCWSTR>(IDC_SIZEALL);index=3;break;
    case CURSOR_CIRCLE_SLASH:resource=reinterpret_cast<LPCWSTR>(IDC_NO);index=4;break;
    case CURSOR_SIZEWE:resource=reinterpret_cast<LPCWSTR>(IDC_SIZEWE);index=5;break;
    case CURSOR_SIZENS:resource=reinterpret_cast<LPCWSTR>(IDC_SIZENS);index=6;break;
    case CURSOR_SIZENESW:resource=reinterpret_cast<LPCWSTR>(IDC_SIZENESW);index=7;break;
    case CURSOR_SIZENWSE:resource=reinterpret_cast<LPCWSTR>(IDC_SIZENWSE);index=8;break;
    case CURSOR_WAIT:resource=reinterpret_cast<LPCWSTR>(IDC_WAIT);index=9;break;
    case CURSOR_POINTER:case CURSOR_NONE:case CURSOR_CUSTOM:break;
    default:throw std::invalid_argument("Unknown game cursor");
    }
    HCURSOR selected=mOverrideCursor;
    if(!overrideCursor) {
        if(!mNativeCursors[index]) {
            mNativeCursors[index]=LoadCursorW(nullptr,resource);
            if(!mNativeCursors[index]) WindowFailure("LoadCursor");
        }
        selected=hidden?nullptr:mNativeCursors[index];
    }else index=-2;
    mNativeAppliedCursor=selected;mNativeCursorIndex=index;mSysCursor=!hidden;
}

void SexyAppBase::DestroyGameWindow() {
    mNativeWindowReady=false;mNativeWindowShown=false;
    ResetWindowsHdr(mNativeHdrState);
    mGpuContext.reset();
    if(mHWnd) {
        SetWindowLongPtrW(mHWnd,GWLP_USERDATA,0);
        if(!DestroyWindow(mHWnd)) WindowFailure("DestroyWindow");
        mHWnd=nullptr;
    }
    mNativeCursors.fill(nullptr);mNativeAppliedCursor=nullptr;
}

static INT_PTR CALLBACK MarkerListDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			HWND aListBox = GetDlgItem(hwnd,100);

			DWORD       dwExtent = 0;
			HDC         hDCListBox;
			HFONT       hFontOld, hFontNew;
			TEXTMETRIC  tm; 
			RECT aRect;
			SIZE aSize;

			hDCListBox = GetDC(aListBox); 
			hFontNew = (HFONT)SendMessage(aListBox, WM_GETFONT, 0, 0); 
			hFontOld = (HFONT)SelectObject(hDCListBox, hFontNew); 
			GetTextMetrics(hDCListBox, (LPTEXTMETRIC)&tm); 
			GetClientRect(hwnd, &aRect);
			MoveWindow(aListBox,10,10,aRect.right-aRect.left-20,aRect.bottom-aRect.top-20,FALSE);
			for (SexyAppBase::DemoMarkerList::iterator anItr = gSexyAppBase->mDemoMarkerList.begin(); anItr != gSexyAppBase->mDemoMarkerList.end(); ++anItr)
			{
				if (anItr->second <= gSexyAppBase->mUpdateCount)
					continue;

				const auto aTotalSeconds = FrameCounterMath::RemainingSeconds(gSexyAppBase->mDemoLength, anItr->second, gSexyAppBase->mFrameTime);
				const auto aSeconds = aTotalSeconds%60;
				const auto aMinutes = (aTotalSeconds/60)%60;
				const auto anHours = (aTotalSeconds/3600);

				SexyString aStr = StrFormat(("%s (%02llu:%02llu:%02llu)"), anItr->first.c_str(), static_cast<unsigned long long>(anHours), static_cast<unsigned long long>(aMinutes), static_cast<unsigned long long>(aSeconds));
				GetTextExtentPoint32(hDCListBox, aStr.c_str(), aStr.length(), &aSize);
				dwExtent = std::max (aSize.cx + tm.tmAveCharWidth, (LONG)dwExtent);
				SendMessage(aListBox, LB_SETHORIZONTALEXTENT, dwExtent, 0);
				LRESULT anIndex = SendMessage(aListBox, LB_ADDSTRING, 0, (LPARAM)aStr.c_str());
				SendMessage(aListBox, LB_SETITEMDATA, anIndex, anItr->second);
			}
	
			SelectObject(hDCListBox, hFontOld);
			ReleaseDC(aListBox, hDCListBox); 

			return TRUE;
		}

		case WM_CLOSE:
			EndDialog(hwnd,0);
			return TRUE;

		case WM_COMMAND:
			if (HIWORD(wParam)==LBN_DBLCLK)
			{
				HWND aListBox = GetDlgItem(hwnd,100);

				int anIndex = SendMessage(aListBox,LB_GETCURSEL,0,0);
				if (anIndex >= 0)
				{
					const auto aMarkerData = SendMessage(aListBox,LB_GETITEMDATA,anIndex,0);
					if (aMarkerData == LB_ERR) return TRUE;
					const auto anUpdateTime = static_cast<std::uint32_t>(aMarkerData);
					if (anUpdateTime > gSexyAppBase->mUpdateCount)
					{
						gSexyAppBase->mFastForwardToUpdateNum = anUpdateTime;
						EndDialog(hwnd,0);
					}
				}
				return TRUE;
			}
			break;

	}

	return FALSE;
}


static int ListDemoMarkers()
{
    alignas(DWORD) std::array<std::byte, DemoTools::DialogTemplateCapacity> storage{};
    const auto* dialog = DemoTools::BuildDialogTemplate(
        storage, DemoTools::DialogKind::Markers);
    const INT_PTR result = DialogBoxIndirect(
        gHInstance, dialog, gSexyAppBase->mHWnd, MarkerListDialogProc);
    gSexyAppBase->mLastTime = SexyGetTickCount();
    return static_cast<int>(result);
}

static INT_PTR CALLBACK JumpToTimeDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	(void)lParam;
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			HWND anEdit = GetDlgItem(hwnd,100);
			SendMessage(anEdit, EM_SETLIMITTEXT, 499, 0);
			HKEY aGameKey;
			std::string aKeyName = RemoveTrailingSlash("SOFTWARE\\" + gSexyAppBase->mRegKey);
			if (RegOpenKeyExA(HKEY_CURRENT_USER, aKeyName.c_str(), 0, KEY_READ | KEY_WRITE, &aGameKey) == ERROR_SUCCESS)
			{
				char aBuf[1024];
				DWORD aLength = 1000;
				DWORD aType = REG_SZ;
				if (RegQueryValueExA(aGameKey, "DemoJumpTime", 0, &aType, (uchar*) aBuf, &aLength) == ERROR_SUCCESS)
				{
					aBuf[aLength] = 0;
					SetWindowTextA(anEdit,aBuf);
					SendMessage(anEdit,EM_SETSEL,0,-1);
				}
				RegCloseKey(aGameKey);
			}
			return TRUE;
		}
		break;

		case WM_CLOSE:
			EndDialog(hwnd,0);
			return TRUE;

		case WM_COMMAND:
			if (HIWORD(wParam)==BN_CLICKED)
			{
				if (LOWORD(wParam)==IDOK)
				{
					char aBuf[512] = {};
					HWND anEdit = GetDlgItem(hwnd,100);
					GetWindowTextA(anEdit,aBuf,500);
					std::optional<std::uint32_t> aTarget;
					if (GetWindowTextLengthA(anEdit) < 500)
						aTarget = DemoTools::ParseJumpTarget(aBuf, gSexyAppBase->mDemoLength, gSexyAppBase->mFrameTime);
					if (!aTarget) {
						MessageBoxA(hwnd, "Enter non-negative whole-number time components separated by colons (for example 90 or 1:30). The value must fit in 64 bits.",
							"Invalid demo time", MB_OK | MB_ICONWARNING);
						return TRUE;
					}

					HKEY aGameKey;
					std::string aKeyName = RemoveTrailingSlash("SOFTWARE\\" + gSexyAppBase->mRegKey);
					if (RegOpenKeyExA(HKEY_CURRENT_USER, aKeyName.c_str(), 0, KEY_READ | KEY_WRITE, &aGameKey) == ERROR_SUCCESS)
					{
						RegSetValueExA(aGameKey, "DemoJumpTime", 0, REG_SZ, (const BYTE*)aBuf, strlen(aBuf)+1);
						RegCloseKey(aGameKey);
					}

					gSexyAppBase->mFastForwardToUpdateNum = *aTarget;


				}
					
				EndDialog(hwnd,0);
				return TRUE;
			}
			break;
	}

	return FALSE;
}

static int DemoJumpToTime()
{
    alignas(DWORD) std::array<std::byte, DemoTools::DialogTemplateCapacity> storage{};
    const auto* dialog = DemoTools::BuildDialogTemplate(
        storage, DemoTools::DialogKind::JumpTime);
    const INT_PTR result = DialogBoxIndirect(
        gHInstance, dialog, gSexyAppBase->mHWnd, JumpToTimeDialogProc);
    gSexyAppBase->mLastTime = SexyGetTickCount();
    return static_cast<int>(result);
}

static void ToggleDemoSoundVolume()
{
	if (gSexyAppBase->mMusicVolume == 0.0)
		gSexyAppBase->SetMusicVolume(gSexyAppBase->mDemoMusicVolume);
	else
	{
		gSexyAppBase->mDemoMusicVolume = gSexyAppBase->mMusicVolume;
		gSexyAppBase->SetMusicVolume(0.0);
	}

	if (gSexyAppBase->mSfxVolume == 0.0)
		gSexyAppBase->SetSfxVolume(gSexyAppBase->mDemoSfxVolume);
	else
	{
		gSexyAppBase->mDemoSfxVolume = gSexyAppBase->mSfxVolume;
		gSexyAppBase->SetSfxVolume(0.0);
	}
}

static DWORD gPowerSaveTick = 0;
static bool ScreenSaverWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT &theResult)
{
	static bool gCreated = false;
	static int gMouseMoveCount = 0;
	static int gLastMouseX = 0, gLastMouseY = 0;
	static bool gClosed = false;
	typedef BOOL (WINAPI *VERIFYPWDPROC)(HWND);
	static VERIFYPWDPROC aPasswordFunc = NULL;
	HMODULE aPasswordLib = NULL;

	if (gClosed)
		return false;

	switch (uMsg)
	{
		case WM_CREATE:
		{
			if (gCreated)
				return false;

			gCreated = true;
			POINT aMousePoint;
			GetCursorPos(&aMousePoint);
			gLastMouseX = aMousePoint.x;
			gLastMouseY = aMousePoint.y;

			// Password checking stuff for 95/98/ME
			OSVERSIONINFO aVersion;
			aVersion.dwOSVersionInfoSize = sizeof(aVersion);
			GetVersionEx(&aVersion);
			if (aVersion.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS)
			{
				HKEY hKey;
				if (RegOpenKey(HKEY_CURRENT_USER, REGSTR_PATH_SCREENSAVE ,&hKey) == ERROR_SUCCESS)
				{
					DWORD aCheckPwd = 0;
					DWORD aSize = sizeof(DWORD);
					DWORD aType;
					LONG aResult = RegQueryValueEx(hKey, REGSTR_VALUE_USESCRPASSWORD, NULL, &aType, (PBYTE) &aCheckPwd, &aSize);
					if (aResult==ERROR_SUCCESS && aCheckPwd)
					{
						aPasswordLib = LoadLibrary(TEXT("PASSWORD.CPL"));
						if (aPasswordLib)
						{
							aPasswordFunc = (VERIFYPWDPROC)GetProcAddress(aPasswordLib, "VerifyScreenSavePwd");
							// prevents user from ctrl-alt-deleting the screensaver etc to avoid typing in a password
							int aPrev;
							SystemParametersInfo(SPI_SCREENSAVERRUNNING, TRUE, &aPrev, 0); 
						}

					}
					RegCloseKey(hKey);
				}
			}
			return false;
		}
		break;

		case WM_SYSCOMMAND:
		{
			switch (wParam)
			{
				case SC_CLOSE:
				case SC_SCREENSAVE:
				case SC_NEXTWINDOW:
				case SC_PREVWINDOW:
					theResult = FALSE;
					return true;

				default:
					return false;
			}
		}
		break;

		case WM_MOUSEMOVE:
		{
			int aMouseX = LOWORD(lParam);
			int aMouseY = HIWORD(lParam);
			if (aMouseX!=gLastMouseX || aMouseY!=gLastMouseY)
			{
				gLastMouseX = aMouseX;
				gLastMouseY = aMouseY;
				gMouseMoveCount++;
			}

			if (gMouseMoveCount < 4)
			{
				theResult = 0;
				return true;
			}
		}
		break;

		case WM_NCACTIVATE:
		case WM_ACTIVATE:
		case WM_ACTIVATEAPP:
		{
			if (wParam != FALSE)
				return false;
		}
		break;

		case WM_CLOSE:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			break;

		default:
			return false;
	}

	if (gSexyAppBase!=NULL && gSexyAppBase->mHWnd!=hWnd) // wrong window
		return false;

	if (GetTickCount()-gPowerSaveTick < 1000) // powersave just went on so ignore certain messages that seem to come on certain os's at that time
	{
		switch (uMsg)
		{
			case WM_MOUSEMOVE: 
			case WM_NCACTIVATE:
			case WM_ACTIVATE: 
			case WM_ACTIVATEAPP:
			case WM_CLOSE: 
				return false;
		}
	}

	if (aPasswordFunc && gSexyAppBase!=NULL && gSexyAppBase->mInitialized) // need to verify password before closing
	{
		gSexyAppBase->mNoDefer = true;							// so the app doesn't draw over the password dialog
	    
		gClosed = true; // prevent this function from doing anything while in the password dialog
		BOOL aPasswordResult = aPasswordFunc(hWnd);
		gClosed = false; // let this functino work again

		if (gSexyAppBase!=NULL)
		{
			gSexyAppBase->mNoDefer = false;
			gSexyAppBase->ClearUpdateBacklog();
		}

		if (!aPasswordResult) // bad password
		{
			// Get new mouse coordinate
			POINT aMousePoint;
			GetCursorPos(&aMousePoint);
			gLastMouseX = aMousePoint.x;
			gLastMouseY = aMousePoint.y;
			gMouseMoveCount = 0;

			return false;
		}


		// can turn this SPI_SCREENSAVERRUNNING off now since screensaver is about to stop
		int aPrev;
		SystemParametersInfo(SPI_SCREENSAVERRUNNING, FALSE, &aPrev, 0); 

		// good password -> close and unload dll
		FreeLibrary(aPasswordLib);
		aPasswordLib = NULL;
		aPasswordFunc = NULL;
	}
	
	// Screen saver should shutdown
	gClosed = true;
	PostMessage(hWnd, WM_CLOSE, 0, 0);

	return false;
}

LRESULT CALLBACK SexyAppBase::WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST && TouchPromotedMouse()) return 0;
	if (uMsg == WM_MOUSEMOVE || uMsg == WM_MOUSELEAVE)
		return 0;
	if (uMsg == WM_SETCURSOR && LOWORD(lParam) == HTCLIENT)
		return TRUE;
	if (gSexyAppBase!=NULL && gSexyAppBase->mIsScreenSaver)
	{
		LRESULT aResult;
		if (ScreenSaverWindowProc(hWnd,uMsg,wParam,lParam,aResult))
			return aResult;
	}

	SexyAppBase* aSexyApp = (SexyAppBase*)(intptr_t)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (uMsg == WM_NCCREATE)
	{
		const auto* creation = reinterpret_cast<const CREATESTRUCTA*>(lParam);
		aSexyApp = static_cast<SexyAppBase*>(creation->lpCreateParams);
		if (aSexyApp)
			SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(aSexyApp));
	}
	// CreateWindow sends messages before MakeWindow has constructed the renderer.
	if (!aSexyApp)
	{
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	try
	{
	if ((uMsg == WM_GETMINMAXINFO || uMsg == WM_SIZING) &&
		!aSexyApp->mNativeFullscreen &&
		(GetWindowLongPtrW(hWnd, GWL_STYLE) & WS_THICKFRAME) &&
		(hWnd == aSexyApp->mHWnd || !aSexyApp->mHWnd))
	{
		const auto limits = WindowClientLimits(*aSexyApp);
		RECT border{};
		if (!NativeWindowsWindowApi::AdjustClientRect(border,
			DWORD(GetWindowLongPtrW(hWnd, GWL_STYLE)),
			DWORD(GetWindowLongPtrW(hWnd, GWL_EXSTYLE)), hWnd))
			WindowFailure("AdjustClientRect");
		const int borderWidth = border.right - border.left;
		const int windowHeight = limits.height + border.bottom - border.top;
		if (uMsg == WM_GETMINMAXINFO)
		{
			auto& info = *reinterpret_cast<MINMAXINFO*>(lParam);
			info.ptMinTrackSize = {limits.minimumWidth + borderWidth, windowHeight};
			info.ptMaxTrackSize = {limits.maximumWidth + borderWidth, windowHeight};
			return 0;
		}
		auto& rect = *reinterpret_cast<RECT*>(lParam);
		const int width = limits.ConstrainWidth(
			int(rect.right - rect.left) - borderWidth) + borderWidth;
		if (wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT)
			rect.left = rect.right - width;
		else
			rect.right = rect.left + width;
		if (wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT)
			rect.top = rect.bottom - windowHeight;
		else
			rect.bottom = rect.top + windowHeight;
		return TRUE;
	}
	if (!aSexyApp->mNativeWindowReady)
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

#if _WIN32_WINNT >= 0x0602
    if (uMsg == WM_POINTERCAPTURECHANGED) {
        aSexyApp->QueueNativeTouch(GET_POINTERID_WPARAM(wParam), TouchCancel, 0, 0);
        return 0;
    }
    if (uMsg == WM_POINTERDOWN || uMsg == WM_POINTERUPDATE || uMsg == WM_POINTERUP) {
        const auto id = GET_POINTERID_WPARAM(wParam);
        POINTER_INPUT_TYPE type{};
        if (GetPointerType(id, &type) && type == PT_TOUCH) {
            POINTER_TOUCH_INFO points[32]{};
            unsigned int count = 32;
            if (!GetPointerFrameTouchInfo(id, &count, points)) {
                aSexyApp->QueueNativeTouch(0, TouchCancelAll, 0, 0);
                return 0;
            }
            POINT origin{};
            if (!ClientToScreen(hWnd, &origin)) {
                aSexyApp->QueueNativeTouch(0, TouchCancelAll, 0, 0);
                return 0;
            }
            for (unsigned int i = 0; i < count; ++i) {
                const auto& info = points[i].pointerInfo;
                unsigned int flags = 0;
                if (info.pointerFlags & POINTER_FLAG_CANCELED) flags = TouchCancel;
                else if (info.pointerFlags & POINTER_FLAG_DOWN) flags = TOUCHEVENTF_DOWN;
                else if (info.pointerFlags & POINTER_FLAG_UP) flags = TOUCHEVENTF_UP;
                else if (info.pointerFlags & POINTER_FLAG_INCONTACT) flags = TOUCHEVENTF_MOVE;
                if (!flags) continue;
                const auto point = info.ptPixelLocation;
                aSexyApp->QueueNativeTouch(info.pointerId, flags, point.x - origin.x, point.y - origin.y);
            }
            SkipPointerFrameMessages(id);
            return 0;
        }
    }
#else
    if (uMsg == WM_TOUCH) {
        const auto handle = reinterpret_cast<HTOUCHINPUT>(lParam);
        struct CloseTouch { HTOUCHINPUT handle; ~CloseTouch(){CloseTouchInputHandle(handle);} } close{handle};
        TOUCHINPUT points[32]{};
        const unsigned int count = LOWORD(wParam);
        if (count > 32 || !GetTouchInputInfo(handle, count, points, sizeof(TOUCHINPUT))) {
            aSexyApp->QueueNativeTouch(0, TouchCancelAll, 0, 0);
            return 0;
        }
        POINT origin{};
        if (!ClientToScreen(hWnd, &origin)) return 0;
        for (unsigned int i = 0; i < count; ++i)
            aSexyApp->QueueNativeTouch(points[i].dwID, points[i].dwFlags,
                points[i].x / 100.0 - origin.x, points[i].y / 100.0 - origin.y);
        return 0;
    }
#endif

	auto deferMessage = [&] {
		MSG message{};
		message.hwnd = hWnd; message.message = uMsg;
		message.wParam = wParam; message.lParam = lParam;
		aSexyApp->mDeferredMessages.push_back(message);
	};
	switch (uMsg)
	{
	case WM_KILLFOCUS:
		if (aSexyApp != NULL && hWnd == aSexyApp->mHWnd)
		{
			aSexyApp->mAltEnterDown = false;
			aSexyApp->mActive = false;
			aSexyApp->ReleaseCursorClip();
			deferMessage();
		}
		break;
	case WM_CANCELMODE:
		// DefWindowProc cancels native menu/scroll modes and releases mouse
		// capture. Capture and ClipCursor are independent state machines: the
		// application is still the active fullscreen foreground window here, so
		// cancelling capture must not release its visible-game-area confinement.
        aSexyApp->mWidgetManager->CancelTouches();
		break;
	case WM_SETFOCUS:
		if (aSexyApp != NULL && hWnd == aSexyApp->mHWnd)
		{
			aSexyApp->mActive = true;
			aSexyApp->UpdateCursorClip();
			deferMessage();
		}
		break;
	case WM_CAPTURECHANGED:
		if (reinterpret_cast<HWND>(lParam) != hWnd)
        {
			aSexyApp->mWidgetManager->CancelMouseDownState();
        }
		break;
	case WM_DPICHANGED:
		if (!aSexyApp->mNativeFullscreen)
		{
			const auto& rect = *reinterpret_cast<const RECT*>(lParam);
			const auto client = ClientRect(hWnd);
			const auto limits = WindowClientLimits(*aSexyApp);
			RECT bounds{0, 0, limits.ConstrainWidth(client.right), limits.height};
			if (!NativeWindowsWindowApi::AdjustClientRect(bounds,
				DWORD(GetWindowLongPtrW(hWnd, GWL_STYLE)),
				DWORD(GetWindowLongPtrW(hWnd, GWL_EXSTYLE)), hWnd))
				WindowFailure("AdjustClientRect");
			if (!SetWindowPos(hWnd, nullptr, rect.left, rect.top,
				bounds.right - bounds.left, bounds.bottom - bounds.top,
				SWP_NOZORDER | SWP_NOACTIVATE))
				throw std::runtime_error("WM_DPICHANGED SetWindowPos failed");
		}
		aSexyApp->mNativeResizePending = true;
		aSexyApp->mHdrTargetPending = true;
		return 0;

	//  TODO: switch to killfocus/setfocus?
//	case WM_KILLFOCUS:
//	case WM_SETFOCUS:
//	if ((aSexyApp != NULL) && (!aSexyApp->mPlayingDemoBuffer))
//	{
//		if (hWnd == aSexyApp->mHWnd)
//			aSexyApp->mActive = uMsg==WM_SETFOCUS;
//	}
//	//Fallthrough

	case WM_TIMER:
		// The timer is only a wake-up plus mouse enter/exit bookkeeping. Handle
		// it synchronously instead of parking it in the
		// deferred queue: a parked timer splits otherwise-consecutive mouse
		// messages and taxes the deferred drain one 10ms tick per message,
		// which turns click bursts into visible stutter.
		if ((aSexyApp != NULL) && (!aSexyApp->mNoDefer))
			aSexyApp->ProcessTimerMessage(hWnd);
		return 0;

	case WM_ACTIVATEAPP:
		if ((aSexyApp != NULL) && (!aSexyApp->mPlayingDemoBuffer))
		{
			if (hWnd == aSexyApp->mHWnd)
			{
				aSexyApp->mActive = wParam != 0;
				if (wParam == 0)
					aSexyApp->ReleaseCursorClip();
				else
					aSexyApp->UpdateCursorClip();
			}
		}
		//Fallthrough	

	case WM_SIZE:
	case WM_MOVE:
	case WM_LBUTTONDOWN:		
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:		
	case WM_MBUTTONUP:		
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
	case WM_CHAR:	
	case WM_CLOSE:
	case WM_MOUSEWHEEL:
	case WM_DISPLAYCHANGE:
	case WM_SYSCOLORCHANGE:
		{
/*			if (aSexyApp!=NULL && aSexyApp->mProcessInTimer && !aSexyApp->mShutdown && aSexyApp->mRunning)
			{
				if (uMsg==WM_TIMER && wParam==101)
				{
					for (int i=0; i<5; i++)
					{
						if (GetQueueStatus(QS_INPUT | QS_PAINT))
							break;

						if (!aSexyApp->Process(false))
							break;
					}
				}
					
				break;
			}*/

			/*if ((aSexyApp != NULL) && (aSexyApp->mNoDefer))
			{
				// Check to see if we should be windowed
				WINDOWPLACEMENT aWindowPlacement;
				aWindowPlacement.length = sizeof(aWindowPlacement);
				if (GetWindowPlacement(aSexyApp->mHWnd, &aWindowPlacement))
				{
					if (aWindowPlacement.showCmd == SW_SHOWMINIMIZED)
					{
						aSexyApp->Redraw(NULL);
					}
				}
			}*/

			if ((aSexyApp != NULL) && (!aSexyApp->mNoDefer))
			{
				if (hWnd == aSexyApp->mHWnd)
				{
					if (uMsg == WM_SIZE) aSexyApp->mNativeResizePending = true;
					if (uMsg == WM_MOVE) aSexyApp->mHdrTargetPending = true;
					if (uMsg == WM_DISPLAYCHANGE)
					{
						++aSexyApp->mNativeMonitorGeneration;
						aSexyApp->mNativeTopologyPending = true;
						aSexyApp->mHdrTargetPending = true;
					}
				}
				bool keyDown = (uMsg==WM_KEYDOWN) || (uMsg==WM_SYSKEYDOWN);

				if ((keyDown) || (uMsg==WM_KEYUP) || (uMsg == WM_SYSKEYUP))
				{	
					if (wParam == VK_CONTROL)
						aSexyApp->mCtrlDown = keyDown;
					if (wParam == VK_MENU)
						aSexyApp->mAltDown = keyDown;
				}

				/*
				if ((keyDown) && (aSexyApp->DebugKeyDownAsync(wParam, aSexyApp->mCtrlDown, aSexyApp->mAltDown)))
					return 0;
				*/
				
				if (aSexyApp->mPlayingDemoBuffer)
				{
					if (uMsg==WM_CHAR)
					{
						switch (wParam)
						{
						case '+':
							aSexyApp->mUpdateMultiplier *= 1.5;
							break;
						case '-':
							aSexyApp->mUpdateMultiplier /= 1.5;
							break;
						case '=':
							aSexyApp->mUpdateMultiplier = 1;
							break;
						case 'p':
						case 'P':
							aSexyApp->mPaused = !aSexyApp->mPaused;
							aSexyApp->ClearUpdateBacklog();
							break;

						case 'n':
						case 'N':
							aSexyApp->mFastForwardToUpdateNum = aSexyApp->mUpdateCount+1;
							aSexyApp->mFastForwardStep = true;
							break;

						case 'm':
						case 'M':
							aSexyApp->mFastForwardToMarker = true;
							break;

						case 'l':
						case 'L':
							ListDemoMarkers();
							break;

						case 'j':
						case 'J':
							DemoJumpToTime();
							break;

						case 's':
						case 'S':
							ToggleDemoSoundVolume();
							break;

						case '4':
							// Fast foward to 120 seconds before it ends
							aSexyApp->mFastForwardToUpdateNum = FrameCounterMath::BeforeEnd(aSexyApp->mDemoLength, 120000 / aSexyApp->mFrameTime);
							break;
						case '5':
							// Fast foward to 90 seconds before it ends
							aSexyApp->mFastForwardToUpdateNum = FrameCounterMath::BeforeEnd(aSexyApp->mDemoLength, 90000 / aSexyApp->mFrameTime);
							break;
						case '6':
							// Fast foward to 60 seconds before it ends
							aSexyApp->mFastForwardToUpdateNum = FrameCounterMath::BeforeEnd(aSexyApp->mDemoLength, 60000 / aSexyApp->mFrameTime);
							break;
						case '7':
							// Fast foward to 30 seconds before it ends
							aSexyApp->mFastForwardToUpdateNum = FrameCounterMath::BeforeEnd(aSexyApp->mDemoLength, 30000 / aSexyApp->mFrameTime);
							break;
						case '8':
							// Fast foward to 10 seconds before it ends
							aSexyApp->mFastForwardToUpdateNum = FrameCounterMath::BeforeEnd(aSexyApp->mDemoLength, 10000 / aSexyApp->mFrameTime);
							break;
						case '9':
							// Fast foward to 5 seconds before it ends
							aSexyApp->mFastForwardToUpdateNum = FrameCounterMath::BeforeEnd(aSexyApp->mDemoLength, 5000 / aSexyApp->mFrameTime);
							break;
						case '0':						
							// Fast forward to the end
							aSexyApp->mFastForwardToUpdateNum = aSexyApp->mDemoLength;
							break;

						}
					}
					else if (uMsg==WM_KEYDOWN)
						aSexyApp->DebugKeyDown(wParam);
				}

				bool pushMessage = true;

				if (aSexyApp->mDeferredMessages.size() > 0)
				{
					// Don't add any more messages after WM_CLOSE
					MSG* aMsg = &aSexyApp->mDeferredMessages.back();

					if (aMsg->message == WM_CLOSE)
						pushMessage = false;
					if ((uMsg == WM_TIMER) && (uMsg == aMsg->message))
						pushMessage = false; // Don't need more timer messages


					if (pushMessage && (uMsg==WM_SYSCOLORCHANGE || uMsg==WM_DISPLAYCHANGE)) // kill duplicate SysColorChange() events.
					{
						WindowsMessageList::iterator aMsgListItr = aSexyApp->mDeferredMessages.begin();
						while (pushMessage && aMsgListItr != aSexyApp->mDeferredMessages.end())
						{
							MSG& aMsg = *aMsgListItr;

							if (aMsg.message == WM_SYSCOLORCHANGE || aMsg.message == WM_DISPLAYCHANGE)
								pushMessage = false;

							++aMsgListItr;
						}
					}
				}

					if (uMsg == WM_SYSKEYDOWN && wParam == VK_RETURN)
					{
						WindowsMessageList::iterator anItr =
							aSexyApp->mDeferredMessages.begin();
						while (anItr != aSexyApp->mDeferredMessages.end())
						{
							const MSG& aQueuedMsg = *anItr;
							if (aQueuedMsg.message == WM_SYSKEYDOWN &&
								aQueuedMsg.wParam == VK_RETURN)
							{
								// A held Alt+Enter can produce repeated system-key
								// messages before the deferred queue is drained. Keep
								// only the first queued rising edge.
								pushMessage = false;
								break;
							}
							++anItr;
						}
					}


				if (pushMessage)
				{
					MSG msg;
					msg.hwnd = hWnd;
					msg.message = uMsg;
					msg.lParam = lParam;
					msg.wParam = wParam;

					aSexyApp->mDeferredMessages.push_back(msg);
				}

				if (uMsg == WM_SIZE)
				{
					aSexyApp->mPhysMinimized = wParam == SIZE_MINIMIZED;
					if (hWnd == aSexyApp->mHWnd && aSexyApp->mGpuContext)
					{
#if defined(PVZ_GPU_D3D12)
						aSexyApp->mGpuContext->surfaceSuspended = aSexyApp->mPhysMinimized;
#else
						aSexyApp->mGpuContext->NotifySurfaceSuspended(aSexyApp->mPhysMinimized);
#endif
					}
					if (aSexyApp->mPhysMinimized)
						aSexyApp->ReleaseCursorClip();
				}
					else if (uMsg == WM_SYSKEYDOWN || uMsg == WM_SYSKEYUP)
					{
						// Own the complete Alt+Enter chord. Letting Return/Alt key-up
						// reach DefWindowProc after the mode transaction can re-enter
						// native menu/capture handling and undo the final focus/clip state.
						if (wParam == VK_RETURN || wParam == VK_MENU)
							return 0;
						if (uMsg == WM_SYSKEYDOWN && wParam != VK_F4)
							return 0;
					}

				else if (uMsg == WM_CLOSE)
				{
					aSexyApp->ReleaseCursorClip();
					
					aSexyApp->CloseRequestAsync();
					return 0;
				}
			}			
		}
		break;

	case WM_ENABLE:
		if (aSexyApp != NULL)
		{
			aSexyApp->mIsDisabled = wParam == 0;
			if (aSexyApp->mIsDisabled)
				aSexyApp->ReleaseCursorClip();
			else
				aSexyApp->UpdateCursorClip();
		}
		break;

	case WM_QUERYOPEN:
		if ((aSexyApp != NULL) && (!aSexyApp->AppCanRestore()))
			return 0;
		break;

	case WM_SYSCHAR:
		if ((aSexyApp != NULL) && (aSexyApp->IsAltKeyUsed(wParam)))
			return 0;
		break;

	case WM_NCLBUTTONDOWN:
		if (aSexyApp!=NULL)
		{
//			aSexyApp->mProcessInTimer = true;
			LRESULT aResult = DefWindowProc(hWnd, uMsg, wParam, lParam);
//			aSexyApp->mProcessInTimer = false;
			aSexyApp->ClearUpdateBacklog();
			return aResult;
		}
		break;


	case WM_SYSCOMMAND:
		if (wParam==SC_MONITORPOWER)
		{
			gPowerSaveTick = GetTickCount();
			if (aSexyApp!=NULL && (!aSexyApp->mAllowMonitorPowersave || !aSexyApp->mLoaded))
				return FALSE;
		}
		if (wParam==SC_SCREENSAVE && aSexyApp!=NULL && (!aSexyApp->mLoaded || !aSexyApp->mIsPhysWindowed))
			return FALSE;

		break;

	case WM_DESTROY:
		{
			if (aSexyApp != NULL)
				aSexyApp->ReleaseCursorClip();
		}
		break;	
	case WM_ERASEBKGND:
		return TRUE;		
	case WM_ENDSESSION:
		aSexyApp->ReleaseCursorClip();
		aSexyApp->Shutdown();
		break;
	case WM_PAINT:
		if ((aSexyApp->mInitialized) && (!gInAssert) && (!aSexyApp->mSEHOccured))
		{
			RECT aClientRect;
			GetClientRect(hWnd, &aClientRect);			

			PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);			
			
			aSexyApp->mWidgetManager->MarkAllDirty();


			EndPaint(hWnd, &ps);

			return 0;
		}
		break;
	}

	if (aSexyApp->mNotifyGameMessage != 0 &&
		uMsg == aSexyApp->mNotifyGameMessage && hWnd == aSexyApp->mHWnd)
	{
		// Oh, we are trying to open another instance of ourselves.
		// Bring up the original window instead
		aSexyApp->HandleNotifyGameMessage(wParam);		
		return 0;
	}

	if (gSexyAppBase->mIsWideWindow)
		return DefWindowProcW(hWnd, uMsg, wParam, lParam);
	else
		return DefWindowProcA(hWnd, uMsg, wParam, lParam);
	}
	catch (...)
	{
		aSexyApp->mWindowMessageError = std::current_exception();
		return 0;
	}
}


void SexyAppBase::ProcessTimerMessage(HWND)
{
    // Mouse position and enter/leave are owned by the logical update heartbeat.
    const DWORD now = SexyGetTickCount();
    if (now - mLastTimerTime > 500) mLastBigDelayTime = now;
    mLastTimerTime = now;
}

// Widget actions run after the native pump, outside WindowProc. This retains
// button ordering without re-entering the game from a synchronous OS message.
void SexyAppBase::ProcessMessageNow(UINT uMsg, WPARAM wParam, LPARAM lParam, HWND hWnd)
{
	// WM_CLOSE must always shut the app down, even while a demo buffer is
	// playing (the original deferred path handled it outside the demo guard).
	if (uMsg == WM_CLOSE)
	{
		if ((hWnd == mHWnd) || (hWnd == mInvisHWnd))
		{
			// This should short-circuit all demo calls, otherwise we will get
			//  all sorts of weird asserts because we are changing
			//  program flow
			mManualShutdown = true;

			Shutdown();
		}
		return;
	}

	if (mPlayingDemoBuffer)
		return;

	switch (uMsg)
    {
    case NativeTouchMessage: {
        const unsigned int flags = static_cast<unsigned int>(wParam >> 32);
        const TouchEvent event{0x100000000ULL | static_cast<unsigned int>(wParam),
            static_cast<int>(static_cast<unsigned int>(lParam)),
            static_cast<int>(static_cast<unsigned long long>(lParam) >> 32)};
        mNativeTouchOwnsCursor = true;
        mLastUserInputTick = SexyGetTickCount();
        if (flags & TouchCancelAll) mWidgetManager->CancelTouches();
        else if (flags & TouchCancel) mWidgetManager->TouchCanceled(event);
        else if (flags & TOUCHEVENTF_DOWN) mWidgetManager->TouchBegan(event);
        else if (flags & TOUCHEVENTF_UP) mWidgetManager->TouchEnded(event);
        else if (flags & TOUCHEVENTF_MOVE) mWidgetManager->TouchMoved(event);
        break;
    }
	case WM_KILLFOCUS:
		NativeFocusChanged(false);
		break;
	case WM_SETFOCUS:
		NativeFocusChanged(true);
		break;
	case WM_MOUSELEAVE:
		NativeMouseLeave();
		break;
//  TODO: switch to killfocus/setfocus?
//	case WM_KILLFOCUS:
//	case WM_SETFOCUS:
	case WM_ACTIVATEAPP:
		if ((hWnd == mHWnd) && (!gInAssert) && (!mSEHOccured) && (!mShutdown))
		{
			NativeFocusChanged(wParam != 0);

			if ((mActive) && (!mIsWindowed))
				mWidgetManager->MarkAllDirty();

		}
		break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDBLCLK:
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
		if ((!gInAssert) && (!mSEHOccured))
		{
			const int x = GET_X_LPARAM(lParam);
			const int y = GET_Y_LPARAM(lParam);
			mLastUserInputTick = SexyGetTickCount();
			const int button=(uMsg==WM_RBUTTONDOWN || uMsg==WM_RBUTTONUP ||
				uMsg==WM_RBUTTONDBLCLK)?1:(uMsg==WM_MBUTTONDOWN || uMsg==WM_MBUTTONUP)?2:0;
			const bool pressed=uMsg!=WM_LBUTTONUP && uMsg!=WM_RBUTTONUP && uMsg!=WM_MBUTTONUP;
			NativeMouseButton(button,pressed,x,y,GetTickCount64()/1000.0);
		}
		break;
	case WM_MOUSEWHEEL:
		NativeMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
		break;
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		mLastUserInputTick = SexyGetTickCount();

		// Alt+Enter is an application-owned fullscreen toggle.  Route it through
		// the same transaction used by the options checkbox, rather than letting
		// the focused widget interpret the Return key independently.
		if (uMsg == WM_SYSKEYDOWN && wParam == VK_RETURN)
		{
			if (!mAltEnterDown)
			{
				mAltEnterDown = true;
				RequestScreenMode(!mIsWindowed, mUseCustomScale);
			}
			break;
		}

		DispatchKeyDown(static_cast<int>(wParam));
		break;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		mLastUserInputTick = SexyGetTickCount();
		if (uMsg == WM_SYSKEYUP && wParam == VK_RETURN)
		{
			mAltEnterDown = false;
			break;
		}
		mWidgetManager->KeyUp((KeyCode) wParam);
		break;
	case WM_CHAR:
		mLastUserInputTick = SexyGetTickCount();
		if constexpr (sizeof(SexyChar) == 1)
			if (wParam >= 128) break;
		mWidgetManager->KeyChar((SexyChar) wParam);
		break;
	case WM_MOVE:
		{
			if ((hWnd == mHWnd) && (mIsWindowed))
			{
				WINDOWPLACEMENT aWindowPlacment;
				aWindowPlacment.length = sizeof(aWindowPlacment);

				GetWindowPlacement(hWnd, &aWindowPlacment);
				if ((aWindowPlacment.showCmd == SW_SHOW) ||
					(aWindowPlacment.showCmd == SW_SHOWNORMAL))
				{
					mPreferredX = aWindowPlacment.rcNormalPosition.left;
					mPreferredY = aWindowPlacment.rcNormalPosition.top;
				}
			}
			if (hWnd == mHWnd && !mIsWindowed)
				UpdateCursorClip();
		}
		break;
	case WM_SIZE:
		{
			bool isMinimized = wParam == SIZE_MINIMIZED;

				if ((hWnd == mHWnd) && (!mShutdown) && (isMinimized != mMinimized))
				{
					mMinimized = isMinimized;

					// We don't want any sounds (or music) playing while its minimized
					if (mMinimized)
					{
						if (mMuteOnLostFocus)
							Mute(true);
					}
					else
					{
						if (mMuteOnLostFocus)
							Unmute(true);

						mWidgetManager->MarkAllDirty();
					}
				}

				RehupFocus();
				if (hWnd == mHWnd && !isMinimized && !mIsWindowed)
					UpdateCursorClip();


		}
		break;
	case WM_TIMER:
		ProcessTimerMessage(hWnd);
		break;

	case WM_SYSCOLORCHANGE:
	case WM_DISPLAYCHANGE:
		mWidgetManager->SysColorChangedAll();
		mWidgetManager->MarkAllDirty();
		if (uMsg == WM_DISPLAYCHANGE && !mIsWindowed)
			UpdateCursorClip();
		break;
	}

}

// Why did I defer messages?  Oh, incase a dialog comes up such as a crash
//  it won't keep crashing and stuff
bool SexyAppBase::ProcessDeferredMessages(bool singleMessage)
{
	while (mDeferredMessages.size() > 0)
	{
		MSG aMsg = mDeferredMessages.front();
		mDeferredMessages.pop_front();



		UINT uMsg = aMsg.message;
		LPARAM lParam = aMsg.lParam;
		WPARAM wParam = aMsg.wParam;
		HWND hWnd = aMsg.hwnd;

		if ((mRecordingDemoBuffer) && (!mShutdown))
		{
			switch (uMsg)
			{
//  TODO: switch to killfocus/setfocus?
//			case WM_SETFOCUS:
//			case WM_KILLFOCUS:
//				if (hWnd == mHWnd)
//				{					
//					WriteDemoTimingBlock();
//					mDemoBuffer.WriteNumBits(0, 1);
//					mDemoBuffer.WriteNumBits(DEMO_ACTIVATE_APP, 5);
//					mDemoBuffer.WriteNumBits(uMsg==WM_SETFOCUS ? 1 : 0, 1);
//				}
//				break;			
	
			case WM_ACTIVATEAPP:
				if (hWnd == mHWnd)
				{					
					WriteDemoTimingBlock();
					mDemoBuffer.WriteNumBits(0, 1);
					mDemoBuffer.WriteNumBits(DEMO_ACTIVATE_APP, 5);
					mDemoBuffer.WriteNumBits((wParam != 0) ? 1 : 0, 1);
				}
				break;			

			case WM_SIZE:
				{
					bool isMinimized = wParam == SIZE_MINIMIZED;

					WriteDemoTimingBlock();
					mDemoBuffer.WriteNumBits(0, 1);
					mDemoBuffer.WriteNumBits(DEMO_SIZE, 5);
					mDemoBuffer.WriteBoolean(isMinimized);		
				}
				break;
			case WM_LBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_MBUTTONDOWN:
			case WM_LBUTTONDBLCLK:
			case WM_RBUTTONDBLCLK:
			case WM_LBUTTONUP:
			case WM_RBUTTONUP:
			case WM_MBUTTONUP:
			case WM_MOUSEMOVE:
				{
					int aCurX = (short) LOWORD(lParam);
					int aCurY = (short) HIWORD(lParam);

					int aDiffX = aCurX - mLastDemoMouseX;
					int aDiffY = aCurY - mLastDemoMouseY;

					if ((abs(aCurX - mLastDemoMouseX) < 32) && (abs(aCurY - mLastDemoMouseY) < 32))
					{
						if ((aDiffX != 0) || (aDiffY != 0))
						{
							WriteDemoTimingBlock();
							mDemoBuffer.WriteNumBits(1, 1);
							mDemoBuffer.WriteNumBits(0, 1);
							mDemoBuffer.WriteNumBits(aDiffX, 6);
							mDemoBuffer.WriteNumBits(aDiffY, 6);
						}						
					}
					else
					{
						WriteDemoTimingBlock();
						mDemoBuffer.WriteNumBits(0, 1);
						mDemoBuffer.WriteNumBits(DEMO_MOUSE_POSITION, 5);
						mDemoBuffer.WriteNumBits(aCurX, 12);
						mDemoBuffer.WriteNumBits(aCurY, 12);
					}

					bool down = true;
					int aBtnNum = 0;
					switch (uMsg)
					{
						case WM_LBUTTONDOWN:
							aBtnNum = 1;
							break;
						case WM_RBUTTONDOWN:
							aBtnNum = -1;
							break;
						case WM_MBUTTONDOWN:
							aBtnNum = 3;
							break;
						case WM_LBUTTONDBLCLK:
							aBtnNum = 2;
							break;
						case WM_RBUTTONDBLCLK:
							aBtnNum = -2;
							break;
						case WM_LBUTTONUP:
							aBtnNum = 1;
							down = false;
							break;
						case WM_RBUTTONUP:
							aBtnNum = -1;
							down = false;
							break;
						case WM_MBUTTONUP:
							aBtnNum = 3;
							down = false;
							break;
					}

					if (aBtnNum != 0)
					{
						WriteDemoTimingBlock();
						mDemoBuffer.WriteNumBits(1, 1);
						mDemoBuffer.WriteNumBits(1, 1);
						mDemoBuffer.WriteNumBits(down ? 1 : 0, 1);
						mDemoBuffer.WriteNumBits(aBtnNum, 3);
					}

					mLastDemoMouseX = aCurX;
					mLastDemoMouseY = aCurY;
				}
				break;
			case WM_MOUSEWHEEL:
				{
					int aZDelta = ((short)HIWORD(wParam)) / 120;

					WriteDemoTimingBlock();
					mDemoBuffer.WriteNumBits(0, 1);
					mDemoBuffer.WriteNumBits(DEMO_MOUSE_WHEEL, 5);
					mDemoBuffer.WriteNumBits(aZDelta, 8);
				}
				break;
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
				{
					KeyCode aKeyCode = (KeyCode) wParam;

					WriteDemoTimingBlock();
					mDemoBuffer.WriteNumBits(0, 1);
					mDemoBuffer.WriteNumBits(DEMO_KEY_DOWN, 5);
					mDemoBuffer.WriteNumBits(aKeyCode, 8);
				}
				break;
			case WM_KEYUP:	
			case WM_SYSKEYUP:
				{
					KeyCode aKeyCode = (KeyCode) wParam;

					WriteDemoTimingBlock();
					mDemoBuffer.WriteNumBits(0, 1);
					mDemoBuffer.WriteNumBits(DEMO_KEY_UP, 5);
					mDemoBuffer.WriteNumBits((int) aKeyCode, 8);
				}
				break;
			case WM_CHAR:
				{
					SexyChar aChar = (SexyChar) wParam;

					WriteDemoTimingBlock();
					mDemoBuffer.WriteNumBits(0, 1);
					mDemoBuffer.WriteNumBits(DEMO_KEY_CHAR, 5);
					mDemoBuffer.WriteNumBits(sizeof(SexyChar) == 2, 1);
					mDemoBuffer.WriteNumBits(aChar, sizeof(SexyChar)*8);
				}				
				break;	
			case WM_CLOSE:
				if ((hWnd == mHWnd) || (hWnd == mInvisHWnd))
				{
					WriteDemoTimingBlock();
					mDemoBuffer.WriteNumBits(0, 1);
					mDemoBuffer.WriteNumBits(DEMO_CLOSE, 5);
				}
				break;
			}

			// int aBufferSize = mDemoBuffer.GetDataLen(); // Why?
		}

		if (!mPlayingDemoBuffer || uMsg == WM_CLOSE)
			ProcessMessageNow(uMsg, wParam, lParam, hWnd);

		if (singleMessage)
			break;
	}

	return (mDeferredMessages.size() > 0);
}

