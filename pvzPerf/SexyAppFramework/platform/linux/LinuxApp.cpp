#include "SexyAppBase.h"
#include "SexyAppFramework/platform/NativeWindow.h"
#if defined(__ANDROID__)
#include "SexyAppFramework/platform/android/AndroidWindow.h"
#include "SexyAppFramework/platform/Audio.h"
#include <android/log.h>
#endif
#include "LinuxInput.h"
#include "graphics/DDInterface.h"
#include "graphics/GpuBackend.h"
#include "graphics/GameViewport.h"
#include "graphics/RenderInterface.h"
#include "widget/WidgetManager.h"
#include "misc/KeyCodes.h"
#include "misc/StableTime.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <vector>

using namespace Sexy;

namespace {

// Linux has no shared X11/Wayland desktop setting for double-click thresholds.
constexpr double DoubleClickSeconds = 0.5;
constexpr double DoubleClickDistance = 4.0;

Point LogicalMouse(SexyAppBase& app, double x, double y)
{
    int sceneX = 0, sceneY = 0;
    app.MapWindowMouse(x, y, sceneX, sceneY);
    return {sceneX, sceneY};
}

void RecordMousePosition(SexyAppBase& app, int x, int y)
{
    if (!app.mRecordingDemoBuffer || app.mShutdown)
        return;
    const int dx = x - app.mLastDemoMouseX, dy = y - app.mLastDemoMouseY;
    if (std::abs(dx) < 32 && std::abs(dy) < 32) {
        if (dx || dy) {
            app.WriteDemoTimingBlock();
            app.mDemoBuffer.WriteNumBits(1, 1);
            app.mDemoBuffer.WriteNumBits(0, 1);
            app.mDemoBuffer.WriteNumBits(dx, 6);
            app.mDemoBuffer.WriteNumBits(dy, 6);
        }
    } else {
        app.WriteDemoTimingBlock();
        app.mDemoBuffer.WriteNumBits(0, 1);
        app.mDemoBuffer.WriteNumBits(DEMO_MOUSE_POSITION, 5);
        app.mDemoBuffer.WriteNumBits(x, 12);
        app.mDemoBuffer.WriteNumBits(y, 12);
    }
    app.mLastDemoMouseX = x;
    app.mLastDemoMouseY = y;
}

void RecordCommand(SexyAppBase& app, int command)
{
    app.WriteDemoTimingBlock();
    app.mDemoBuffer.WriteNumBits(0, 1);
    app.mDemoBuffer.WriteNumBits(command, 5);
}

void ApplyMousePosition(SexyAppBase& app, double x, double y, bool nativeInside)
{
    if (app.mSwitchingScreenMode) return;
    if (app.mNativeResizePending && !app.mPhysMinimized)
        app.UpdateWindowGeometry();
    const auto position = LogicalMouse(app, x, y);
    const auto& window = *app.mNativeWindow;
    const bool inside = nativeInside && x >= 0 && y >= 0 &&
        x < window.Width && y < window.Height &&
        app.mScreenBounds.Contains(position.mX, position.mY);
    const bool moved = position.mX != app.mWidgetManager->mLastMouseX ||
        position.mY != app.mWidgetManager->mLastMouseY || inside != app.mMouseIn;
    app.mMouseIn = inside;
    app.mDDInterface->mNextCursorX = app.mDDInterface->mCursorX = position.mX;
    app.mDDInterface->mNextCursorY = app.mDDInterface->mCursorY = position.mY;
    if (inside || app.mWidgetManager->mDownButtons) {
        app.mWidgetManager->MouseMove(position.mX, position.mY);
    } else {
        app.mWidgetManager->mLastMouseX = position.mX;
        app.mWidgetManager->mLastMouseY = position.mY;
    }
    if (!inside)
        app.mWidgetManager->MouseExit(position.mX, position.mY);
    if (moved)
        app.mLastUserInputTick = SexyGetTickCount();
}

} // namespace

void Sexy::QueueNativeWindowEvent(void* context, const NativeWindowEvent& event)
{
    auto& app = *static_cast<SexyAppBase*>(context);
    // Stop accepting application input after close, just like the Win32 queue.
    if (!app.mDeferredMessages.empty() &&
        app.mDeferredMessages.back().kind == NativeWindowEventKind::Close)
        return;
    app.mDeferredMessages.push_back(event);
}

void SexyAppBase::RequestNativeClose()
{
    NativeWindowEvent event{};
    event.kind = NativeWindowEventKind::Close;
    QueueNativeWindowEvent(this, event);
}

void SexyAppBase::ProcessNativeWindowEvent(const NativeWindowEvent& event)
{
    auto& app = *this;
    using Kind = NativeWindowEventKind;
    if (event.kind == Kind::Close) {
        if (app.mRecordingDemoBuffer && !app.mShutdown)
            RecordCommand(app, DEMO_CLOSE);
        app.CloseRequestAsync();
        app.mManualShutdown = true;
        app.Shutdown();
        return;
    }
    if (event.kind == Kind::Resize) {
#if defined(__ANDROID__)
        // Touch coordinates are window-local. Keep their mapping extent in
        // sync with the resized native window, including split/freeform mode.
        if (!event.minimized && event.width > 0 && event.height > 0) {
            app.mNativeWindow->Width = event.width;
            app.mNativeWindow->Height = event.height;
        }
#endif
        app.mNativeResizePending = true;
        app.mPhysMinimized = event.minimized;
        app.mGpuContext->NotifySurfaceSuspended(event.minimized);
#if defined(__ANDROID__)
        // 恢复时 glue 交回的可能是新的 ANativeWindow；旧 VkSurface 绑死
        // 已释放的旧窗口——只重建交换链不够，必须连 surface 一起换绑。
        if (!event.minimized) {
            if (auto* resumed = AttachedAndroidNativeWindow(); resumed != app.mHWnd) {
                app.mGpuContext->NotifySurfaceWindow(resumed);
                app.mHWnd = resumed;
                if (app.mDDInterface) app.mDDInterface->mHWnd = resumed;
            }
        }
#endif
        if (app.mMinimized != event.minimized) {
            app.mMinimized = event.minimized;
            if (app.mMuteOnLostFocus) {
                if (event.minimized) app.Mute(true);
                else app.Unmute(true);
            }
            if (!event.minimized) app.mWidgetManager->MarkAllDirty();
        }
        app.RehupFocus();
        if (app.mRecordingDemoBuffer && !app.mShutdown) {
            RecordCommand(app, DEMO_SIZE);
            app.mDemoBuffer.WriteBoolean(event.minimized);
        }
        return;
    }
    if (event.kind == Kind::Focus) {
#if defined(__ANDROID__)
        // 后台/回焦：挂起/恢复 BASS 输出（音乐+音效一起停，与桌面离焦静音对齐）
        SetAndroidAudioSuspended(!event.focused);
#endif
        app.NativeFocusChanged(event.focused);
        if (app.mRecordingDemoBuffer && !app.mShutdown) {
            RecordCommand(app, DEMO_ACTIVATE_APP);
            app.mDemoBuffer.WriteNumBits(event.focused ? 1 : 0, 1);
        }
        return;
    }
    if (app.mPlayingDemoBuffer)
        return;
    switch (event.kind) {
    case Kind::MouseCanceled:
        app.mWidgetManager->CancelMouseDownState();
        break;
    case Kind::TouchBegan: case Kind::TouchMoved: case Kind::TouchEnded: case Kind::TouchCanceled: {
        if (app.mSEHOccured) break;
        if (app.mNativeResizePending && !app.mPhysMinimized) app.UpdateWindowGeometry();
        const auto position = LogicalMouse(app, event.x, event.y);
        const TouchEvent touch{event.touchId, position.mX, position.mY};
        app.mLastUserInputTick = SexyGetTickCount();
        switch (event.kind) {
        case Kind::TouchBegan:
            if (app.mScreenBounds.Contains(touch.x, touch.y)) app.mWidgetManager->TouchBegan(touch);
            break;
        case Kind::TouchMoved: app.mWidgetManager->TouchMoved(touch); break;
        case Kind::TouchEnded: app.mWidgetManager->TouchEnded(touch); break;
        case Kind::TouchCanceled: app.mWidgetManager->TouchCanceled(touch); break;
        default: break;
        }
        break;
    }
    case Kind::MouseButton:
        if (!app.mSEHOccured) {
            app.mLastUserInputTick = SexyGetTickCount();
            app.NativeMouseButton(event.button, event.pressed, event.x, event.y,
                                  static_cast<double>(event.timeMs) / 1000.0);
        }
        break;
    case Kind::MouseWheel: {
        // NativeWindow wheel Y is already in steps; keep smooth remainders.
        app.mNativeWheelRemainder += event.y;
        const int steps = static_cast<int>(app.mNativeWheelRemainder);
        app.mNativeWheelRemainder -= steps;
        if (steps) {
            if (app.mRecordingDemoBuffer && !app.mShutdown) {
                RecordCommand(app, DEMO_MOUSE_WHEEL);
                app.mDemoBuffer.WriteNumBits(steps, 8);
            }
            app.mWidgetManager->MouseWheel(steps);
        }
        app.mLastUserInputTick = SexyGetTickCount();
        break;
    }
    case Kind::Key: {
        const auto value = NativeInput::EngineKeyFromNative(event.key);
        if (!value) break;
        const auto key = static_cast<KeyCode>(value);
        app.mLastUserInputTick = SexyGetTickCount();
        // A held space must not open the pause dialog and then dismiss it via
        // auto-repeat. Keep Text events repeating for editable text fields.
        if (key == KEYCODE_SPACE && event.pressed &&
            (event.repeat || app.mWidgetManager->mKeyDown[KEYCODE_SPACE]))
            break;
        if (key == KEYCODE_CONTROL) app.mCtrlDown = event.pressed;
        if (key == KEYCODE_MENU) app.mAltDown = event.pressed;
        if (key == KEYCODE_RETURN && event.pressed && app.mAltDown) {
            if (!app.mAltEnterDown) {
                app.mAltEnterDown = true;
                app.RequestScreenMode(!app.mIsWindowed, app.mUseCustomScale);
            }
            break;
        }
        if (key == KEYCODE_RETURN && !event.pressed && app.mAltEnterDown) {
            app.mAltEnterDown = false;
            break;
        }
        if (event.pressed && key == KEYCODE_F4 && app.mAltDown) {
            if (app.mRecordingDemoBuffer && !app.mShutdown)
                RecordCommand(app, DEMO_CLOSE);
            app.CloseRequestAsync();
            app.mManualShutdown = true;
            app.Shutdown();
            break;
        }
        if (app.mRecordingDemoBuffer && !app.mShutdown) {
            RecordCommand(app, event.pressed ? DEMO_KEY_DOWN : DEMO_KEY_UP);
            app.mDemoBuffer.WriteNumBits(value, 8);
        }
        if (event.pressed) {
            app.DispatchKeyDown(value);
        } else {
            app.mWidgetManager->KeyUp(key);
        }
        break;
    }
    case Kind::Text:
        if constexpr (sizeof(SexyChar) == 1)
            if (event.text >= 128) break;
        // Ctrl+Shift+D is consumed by DispatchKeyDown, including its text event.
        if (app.mWidgetManager->mKeyDown['D'] &&
            app.mWidgetManager->mKeyDown[KEYCODE_CONTROL] &&
            app.mWidgetManager->mKeyDown[KEYCODE_SHIFT] &&
            !app.mWidgetManager->mKeyDown[KEYCODE_MENU] &&
            (event.text == 4 || event.text == 'D' || event.text == 'd'))
            break;
        if (app.mAltEnterDown) break;
        app.mLastUserInputTick = SexyGetTickCount();
        if (app.mRecordingDemoBuffer && !app.mShutdown) {
            RecordCommand(app, DEMO_KEY_CHAR);
            app.mDemoBuffer.WriteNumBits(sizeof(SexyChar) == 2, 1);
            app.mDemoBuffer.WriteNumBits(event.text, sizeof(SexyChar) * 8);
        }
        app.mWidgetManager->KeyChar(static_cast<SexyChar>(event.text));
        break;
    case Kind::MouseLeave: case Kind::MouseEnter:
        // Native focus/cursor state is already current. Repeated boundary
        // crossings must not drive widget hover work outside the heartbeat.
        break;
    case Kind::MouseMove:
        throw std::logic_error("Native motion must not drive Linux UI callbacks");
    case Kind::Close: case Kind::Resize: case Kind::Focus:
        break;
    }
}

void SexyAppBase::MapWindowMouse(double x, double y, int& sceneX, int& sceneY) const
{
    const int width = mNativeWindow->Width, height = mNativeWindow->Height;
    const auto rect = mDDInterface->mRenderInterface->mRenderer->PresentRect(width, height);
    const auto position = WindowPointToScene(x, y, width, height,
        width, height, rect, mScreenBounds.mWidth, mScreenBounds.mHeight);
    sceneX = position.x + mScreenBounds.mX;
    sceneY = position.y + mScreenBounds.mY;
}

void SexyAppBase::PumpWindowEvents()
{
    mNativeWindow->PumpEvents();
    const auto now = SexyGetTickCount();
    if (now - mLastTimerTime > 500) mLastBigDelayTime = now;
    mLastTimerTime = now;
    // Application actions are deferred until the common FIFO drain point.
    // Geometry is committed there, after all preceding native events.
}

bool SexyAppBase::ProcessDeferredMessages(bool singleMessage)
{
    while (!mShutdown && !mDeferredMessages.empty()) {
        const auto event = mDeferredMessages.front();
        mDeferredMessages.pop_front();
        ProcessNativeWindowEvent(event);
        if (singleMessage) break;
    }
    if (!mShutdown && mNativeResizePending && !mPhysMinimized &&
        !mSwitchingScreenMode && (mDeferredMessages.empty() || !singleMessage))
        UpdateWindowGeometry();
    return !mDeferredMessages.empty();
}

void SexyAppBase::ResizeWindowClient(int width, int height)
{
    if (mNativeFullscreen)
        throw std::logic_error("Cannot resize a fullscreen client as a window");
    mNativeWindow->Resize(width, height);
}

void SexyAppBase::SetWindowFullscreen(bool fullscreen)
{
    if (fullscreen == mNativeFullscreen) return;
    mNativeWindow->SetFullscreen(fullscreen);
    mNativeFullscreen = fullscreen;
}

void SexyAppBase::PrepareFullscreenScaleForMonitor()
{
    if (!mModernScalePrepared || !mNativeFullscreen) return;
    const int width = mNativeWindow->Width, height = mNativeWindow->Height;
    if (width == mModernScaleTargetW && height == mModernScaleTargetH) return;
    std::vector<ScaleEffectRuntimeDesc*> effects;
    std::vector<ScaleEffectScaling> scaling;
    if (mScaleFullscreenConfig.mDeclared &&
        !BuildScalePipelineForMode(false, effects, scaling))
        throw std::runtime_error("Cannot resolve Linux fullscreen scale pipeline");
    std::vector<GpuScaleStep> steps;
    for (size_t i = 0; i < effects.size(); ++i)
        steps.push_back({effects[i], scaling[i]});
    auto& renderer = *mDDInterface->mRenderInterface->mRenderer;
    renderer.PrepareScale(true, steps, width, height);
    renderer.WarmScale(true);
    mModernScaleTargetW = width;
    mModernScaleTargetH = height;
}

void SexyAppBase::UpdateWindowGeometry()
{
    mNativeResizePending = true;
    auto& render = *mDDInterface->mRenderInterface;
    if (render.mSceneBegun) render.Flush();
    const int prepared = mDDInterface->PrepareScreenMode(mHWnd, mIsWindowed);
    if (prepared == DDInterface::RESULT_DEFERRED) return;
    if (prepared != DDInterface::RESULT_OK)
        throw NativeDisplayError(NativeDisplayOperation::PrepareScreenMode, prepared);
    PrepareFullscreenScaleForMonitor();
    const int resized = mDDInterface->ResizePreparedPresent();
    if (resized == DDInterface::RESULT_DEFERRED) return;
    if (resized != DDInterface::RESULT_OK)
        throw NativeDisplayError(NativeDisplayOperation::ResizePreparedPresent, resized);
    mNativeResizePending = false;
    mWidgetManager->Resize(mScreenBounds, mDDInterface->mVisibleGameRect);
    mWidgetManager->MarkAllDirty();
}

void SexyAppBase::UpdateMonitorState()
{
    UpdateWindowGeometry();
    mNativeTopologyPending = false;
    ClearUpdateBacklog(false);
}

void SexyAppBase::UpdateHdrDisplay()
{
#if !defined(__ANDROID__)
    if (mScaleHDREnabled)
        throw std::logic_error("Linux HDR output is not implemented");
#endif
}

void SexyAppBase::SwitchScreenMode(bool windowed, bool force)
{
    if (mSwitchingScreenMode) return;
    if (mForceWindowed) windowed = true;
    else if (mForceFullscreen) windowed = false;
    if (mIsWindowed == windowed && !force) return;
    mSwitchingScreenMode = true;
    struct Reset { bool& flag; ~Reset() { flag = false; } } reset{mSwitchingScreenMode};
    mWidgetManager->CancelMouseDownState();
    ClearKeysDown();
    auto& render = *mDDInterface->mRenderInterface;
    render.Flush();
    SetWindowFullscreen(!windowed);
    if (windowed) {
        const auto scaled = render.mRenderer->ScaleOutput(false);
        ResizeWindowClient(IsScalePresentActiveForMode(true) ? static_cast<int>(scaled.width) : render.mRenderer->sceneWidth,
                           IsScalePresentActiveForMode(true) ? static_cast<int>(scaled.height) : render.mRenderer->sceneHeight);
    }
    mIsWindowed = windowed;
    mIsPhysWindowed = true;
    render.mRenderer->SetScaleMode(IsScalePresentActiveForMode(windowed), !windowed);
    mWidgetManager->MarkAllDirty();
    // Wait for the compositor's actual configure dimensions before resizing WSI.
    ClearUpdateBacklog(false);
}

POINT SexyAppBase::ReadNativeMousePosition()
{
    const auto pointer = mNativeWindow->SamplePointer();
    return {static_cast<int>(std::floor(pointer.x)), static_cast<int>(std::floor(pointer.y))};
}

void SexyAppBase::SampleNativeMousePosition()
{
    if (mPlayingDemoBuffer || !mActive || mPhysMinimized || mSwitchingScreenMode) return;
    if (!mNativeWindowShown) return;
    const auto pointer = mNativeWindow->SamplePointer();
    RecordMousePosition(*this, static_cast<int>(std::floor(pointer.x)),
                        static_cast<int>(std::floor(pointer.y)));
    ApplyMousePosition(*this, pointer.x, pointer.y, pointer.inside);
    EnforceCursor();
}

void SexyAppBase::NativeMouseMove(double x, double y)
{
    ApplyMousePosition(*this, x, y, true);
    EnforceCursor();
}

void SexyAppBase::NativeMouseLeave()
{
    mMouseIn = false;
    mWidgetManager->MouseExit(mWidgetManager->mLastMouseX, mWidgetManager->mLastMouseY);
    EnforceCursor();
}

void SexyAppBase::NativeMouseButton(int button, bool pressed, double x, double y, double time)
{
    if (button < 0 || button > 2)
        throw std::invalid_argument("Unknown Linux mouse button");
    if (mSwitchingScreenMode) return;
    const unsigned mask = button == 0 ? 1u : button == 1 ? 2u : 4u;
    if (!pressed && !((mWidgetManager->mActualDownButtons |
                      mWidgetManager->mDownButtons) & mask))
        return;
    // This is the event's coordinate, never a fresh pointer query.
    NativeMouseMove(x, y);
    if (!mMouseIn) {
        if (!pressed) {
            const auto position = LogicalMouse(*this, x, y);
            mWidgetManager->MouseUp(std::clamp(position.mX, 0, mWidth - 1),
                std::clamp(position.mY, 0, mHeight - 1), button == 1 ? -1 : button == 2 ? 3 : 1);
            mWidgetManager->CancelMouseDownState();
        }
        return;
    }
    const auto position = LogicalMouse(*this, x, y);
    int count = button == 1 ? -1 : button == 2 ? 3 : 1;
    if (pressed) {
        if (button != 2 && time >= mNativeLastClickTime[button] &&
            time - mNativeLastClickTime[button] <= DoubleClickSeconds &&
            std::abs(x - mNativeLastClickX[button]) <= DoubleClickDistance &&
            std::abs(y - mNativeLastClickY[button]) <= DoubleClickDistance) {
            count *= 2;
            mNativeLastClickTime[button] = -1000;
        } else {
            mNativeLastClickTime[button] = time;
        }
        mNativeLastClickX[button] = x;
        mNativeLastClickY[button] = y;
    }
    if (mRecordingDemoBuffer && !mShutdown) {
        RecordMousePosition(*this, static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)));
        WriteDemoTimingBlock();
        mDemoBuffer.WriteNumBits(1, 1);
        mDemoBuffer.WriteNumBits(1, 1);
        mDemoBuffer.WriteNumBits(pressed ? 1 : 0, 1);
        mDemoBuffer.WriteNumBits(count, 3);
    }
    // X11 and Wayland deliver the implicit button grab through release.
    if (pressed) mWidgetManager->MouseDown(position.mX, position.mY, count);
    else mWidgetManager->MouseUp(position.mX, position.mY, count);
    EnforceCursor();
}

void SexyAppBase::NativeMouseWheel(int delta)
{
    // Compatibility entry for existing synthetic input: 120 units per step.
    mNativeWheelRemainder += delta / 120.0;
    const int steps = static_cast<int>(mNativeWheelRemainder);
    mNativeWheelRemainder -= steps;
    if (steps) mWidgetManager->MouseWheel(steps);
}

void SexyAppBase::NativeFocusChanged(bool focused)
{
    mActive = focused;
    if (!focused) {
        mAltEnterDown = false;
        mCtrlDown = mAltDown = false;
        mWidgetManager->CancelTouches();
        mWidgetManager->CancelMouseDownState();
        mNativeLastClickTime.fill(-1000);
        ClearKeysDown();
        NativeMouseLeave();
    }
    RehupFocus();
}

void SexyAppBase::EnforceCursor()
{
    UpdateNativeCursorSelection();
}

void SexyAppBase::UpdateNativeCursorSelection()
{
    if (!mNativeWindow) return;
    if (mOverrideCursor)
        throw std::logic_error("Linux cursor overrides must use NativeCursor, not borrowed native handles");
    NativeCursor cursor = NativeCursor::Arrow;
    const int selected = mSEHOccured || !mMouseIn ? CURSOR_POINTER : mCursorNum;
    switch (selected) {
    case CURSOR_POINTER: break;
    case CURSOR_HAND: cursor = NativeCursor::Hand; break;
    case CURSOR_TEXT: cursor = NativeCursor::Text; break;
    case CURSOR_DRAGGING: case CURSOR_SIZEALL: cursor = NativeCursor::Move; break;
    case CURSOR_CIRCLE_SLASH: cursor = NativeCursor::Forbidden; break;
    case CURSOR_SIZEWE: cursor = NativeCursor::ResizeHorizontal; break;
    case CURSOR_SIZENS: cursor = NativeCursor::ResizeVertical; break;
    case CURSOR_SIZENESW: cursor = NativeCursor::ResizeNorthEast; break;
    case CURSOR_SIZENWSE: cursor = NativeCursor::ResizeNorthWest; break;
    case CURSOR_WAIT: cursor = NativeCursor::Wait; break;
    case CURSOR_NONE: case CURSOR_CUSTOM: cursor = NativeCursor::Hidden; break;
    default: throw std::invalid_argument("Unknown game cursor");
    }
    mNativeWindow->ApplyCursor(cursor);
    mNativeCursorIndex = static_cast<int>(cursor);
    mSysCursor = cursor != NativeCursor::Hidden;
}

void SexyAppBase::DestroyGameWindow()
{
    mNativeWindowReady = false;
    mNativeWindowShown = false;
    mGpuContext.reset();
    mHWnd = nullptr;
    mNativeWindow.reset();
}
