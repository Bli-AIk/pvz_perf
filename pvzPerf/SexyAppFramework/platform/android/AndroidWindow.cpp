#include "../NativeWindow.h"
#include "AndroidWindow.h"
#include "AndroidTouchInput.h"
#include "SexyAppFramework/Common.h"
#include "widget/EditWidget.h"
#include <cmath>
#include <atomic>
#include <mutex>
#include <string>
#include <android/input.h>
#include <android/log.h>
#include <android_native_app_glue.h>

namespace Sexy {
namespace {
android_app* gApp = nullptr;
ANativeWindow* gWindow = nullptr;
NativeWindowEventCallback gEventCallback = nullptr;
void* gEventContext = nullptr;
// Physical mouse sampling is independent of finger/stylus contacts.
NativePointerState gPointer{};
unsigned int gMouseButtons = 0;
AndroidTouchInput gTouches;
bool gGameGesture = false;
std::atomic<unsigned long long> gSurfaceOrigin{0};
// android.view.MotionEvent.FLAG_CANCELED (API 33); absent from NDK r27's enum.
constexpr int MotionFlagCanceled = 0x20;
EditWidget* gEditor = nullptr; // Only dereferenced on the native game thread.
jlong gEditorSession = 0;
std::string gEditorText;
struct TextEdit {
    jlong session = 0;
    std::string text;
    bool done = false;
    bool pending = false;
};
std::mutex gTextMutex;
TextEdit gPendingText;

void PublishEditor(bool show)
{
    if (!gApp || !gApp->activity) return;
    JavaVM* vm = gApp->activity->vm;
    JNIEnv* env = nullptr;
    bool attached = false;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
        attached = true;
    }
    if (!env) return;
    if (env->PushLocalFrame(4) == JNI_OK) {
        jclass activityClass = env->GetObjectClass(gApp->activity->clazz);
        if (show) {
            jmethodID method = env->GetMethodID(activityClass, "showTextInput", "(J[BIIIZ)V");
            if (method) {
                jbyteArray text = env->NewByteArray(static_cast<jsize>(gEditorText.size()));
                if (text) {
                    env->SetByteArrayRegion(text, 0, static_cast<jsize>(gEditorText.size()),
                        reinterpret_cast<const jbyte*>(gEditorText.data()));
                    env->CallVoidMethod(gApp->activity->clazz, method, gEditorSession, text,
                        gEditor->mCursorPos, gEditor->mHilitePos, gEditor->mMaxChars,
                        static_cast<jboolean>(gEditor->mPasswordChar != 0));
                }
            }
        } else {
            jmethodID method = env->GetMethodID(activityClass, "hideTextInput", "(J)V");
            if (method) env->CallVoidMethod(gApp->activity->clazz, method, gEditorSession);
        }
        env->PopLocalFrame(nullptr);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (attached) vm->DetachCurrentThread();
}

void ConsumeTextEdit()
{
    if (!gEditor) return; // No lock or mailbox work in ordinary game frames.
    TextEdit edit;
    {
        std::lock_guard lock(gTextMutex);
        if (!gPendingText.pending) return;
        edit = std::move(gPendingText);
        gPendingText = {};
    }
    if (!gEditor || edit.session != gEditorSession) return;
    EditWidget* editor = gEditor;
    // Keep the original editor's validation, capitalization, font and width
    // limits. UTF-8 bytes must never be inserted directly into bitmap-font text.
    const std::string oldText = editor->mString;
    size_t prefix = 0;
    while (prefix < oldText.size() && prefix < edit.text.size() && oldText[prefix] == edit.text[prefix])
        ++prefix;
    size_t suffix = 0;
    while (suffix < oldText.size() - prefix && suffix < edit.text.size() - prefix &&
        oldText[oldText.size() - suffix - 1] == edit.text[edit.text.size() - suffix - 1])
        ++suffix;
    editor->mCursorPos = static_cast<int>(oldText.size() - suffix);
    editor->mHilitePos = static_cast<int>(prefix);
    if (oldText.size() - suffix > prefix) editor->KeyDown(KEYCODE_BACK);
    for (size_t i = prefix; i < edit.text.size() - suffix; ++i) {
        const auto value = static_cast<unsigned char>(edit.text[i]);
        if (value >= 32 && value <= 126) editor->KeyChar(static_cast<char>(value));
    }
    editor->mHilitePos = -1;
    gEditorText = edit.text;
    if (editor->mString != gEditorText) SyncAndroidTextInput(editor);
    if (edit.done) {
        EndAndroidTextInput(editor);
        editor->KeyDown(KEYCODE_RETURN); // May close and destroy the widget.
    }
}

void EmitEvent(const NativeWindowEvent& event)
{
    if (gEventCallback)
        gEventCallback(gEventContext, event);
}
}

void BindAndroidApp(android_app* state) { gApp = state; }

extern "C" JNIEXPORT void JNICALL
Java_com_openpvz_perf_PerfActivity_nativeSurfaceOriginChanged(JNIEnv*, jclass, jint left, jint top)
{
    gSurfaceOrigin.store((static_cast<unsigned long long>(static_cast<unsigned int>(left)) << 32)
        | static_cast<unsigned int>(top), std::memory_order_relaxed);
}

extern "C" JNIEXPORT void JNICALL
Java_com_openpvz_perf_PerfActivity_nativeTextChanged(JNIEnv* env, jclass,
    jlong session, jbyteArray bytes, jboolean done)
{
    TextEdit edit;
    edit.session = session;
    edit.text.resize(static_cast<size_t>(env->GetArrayLength(bytes)));
    env->GetByteArrayRegion(bytes, 0, static_cast<jsize>(edit.text.size()),
        reinterpret_cast<jbyte*>(edit.text.data()));
    if (env->ExceptionCheck()) return;
    edit.done = done;
    edit.pending = true;
    std::lock_guard lock(gTextMutex);
    gPendingText = std::move(edit);
}

void BeginAndroidTextInput(EditWidget* editor)
{
    if (gEditor != editor) ++gEditorSession;
    gEditor = editor;
    gEditorText = editor->mString;
    PublishEditor(true);
}

void EndAndroidTextInput(EditWidget* editor)
{
    if (gEditor != editor) return;
    ++gEditorSession; // Reject callbacks already queued by the previous editor.
    gEditor = nullptr;
    gEditorText.clear();
    PublishEditor(false);
}

void SyncAndroidTextInput(EditWidget* editor)
{
    if (gEditor != editor || gEditorText == editor->mString) return;
    gEditorText = editor->mString;
    PublishEditor(true);
}

void AttachAndroidNativeWindow(ANativeWindow* window) { gWindow = window; }
ANativeWindow* AttachedAndroidNativeWindow() { return gWindow; }

void AndroidAppCommand(android_app* state, int32_t command)
{
    switch (command) {
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
    case APP_CMD_INIT_WINDOW:
        gTouches.CancelAll();
        gGameGesture = false;
        gWindow = state->window;

        // Resize(未最小化) 驱动 NotifySurfaceSuspended(false) + 几何重建；
        // 挂起期间 BeginCommands 拒绝取图，恢复后交换链按脏标记重建。
        if (gWindow) {
            NativeWindowEvent event{};
            event.kind = NativeWindowEventKind::Resize;
            event.width = ANativeWindow_getWidth(gWindow);
            event.height = ANativeWindow_getHeight(gWindow);
            EmitEvent(event);
        }
        break;
    case APP_CMD_TERM_WINDOW:
        // 必须先于 glue 释放 ANativeWindow 停止呈现；Resize(最小化) 在
        // 本帧排水点把 surface 标记挂起，之后无人再触碰该窗口指针。
        gWindow = nullptr;
        gGameGesture = false;
        gTouches.CancelAll();
        gMouseButtons = 0;
        gPointer.inside = false;
        {
            NativeWindowEvent event{};
            event.kind = NativeWindowEventKind::Resize;
            event.minimized = true;
            EmitEvent(event);
        }
        break;
    case APP_CMD_GAINED_FOCUS: {
        NativeWindowEvent event{};
        event.kind = NativeWindowEventKind::Focus;
        event.focused = true;
        EmitEvent(event);
        break;
    }
    case APP_CMD_LOST_FOCUS: {
        gTouches.CancelAll();
        gGameGesture = false;
        gMouseButtons = 0;
        gPointer.inside = false;
        if (gEditor) EndAndroidTextInput(gEditor);
        NativeWindowEvent event{};
        event.kind = NativeWindowEventKind::Focus;
        event.focused = false;
        EmitEvent(event);
        break;
    }
    case APP_CMD_DESTROY: {
        gTouches.CancelAll();
        NativeWindowEvent event{};
        event.kind = NativeWindowEventKind::Close;
        EmitEvent(event);
        break;
    }
    default:
        break;
    }
}

int32_t AndroidAppInput(android_app*, AInputEvent* event)
{
    const int32_t type = AInputEvent_getType(event);
    if (type == AINPUT_EVENT_TYPE_KEY)
    {
        gTouches.FlushMoves(); // Preserve pending motion before a key/IME action.
        const int32_t action = AKeyEvent_getAction(event);
        if (action != AKEY_EVENT_ACTION_DOWN && action != AKEY_EVENT_ACTION_UP)
            return 0;
        const bool pressed = action == AKEY_EVENT_ACTION_DOWN;
        const int32_t keycode = AKeyEvent_getKeyCode(event);
        const int32_t meta = AKeyEvent_getMetaState(event);
        // Key 事件携带原生 AKEYCODE，由消费端的 constexpr
        // NativeInput::EngineKeyFromNative 单跳映射；此处只补 Text 字符。
        unsigned text = 0;
        if (keycode >= AKEYCODE_A && keycode <= AKEYCODE_Z) {
            const int letter = keycode - AKEYCODE_A;
            text = static_cast<unsigned>(((meta & AMETA_SHIFT_ON) ? 'A' : 'a') + letter);
        } else if (keycode >= AKEYCODE_0 && keycode <= AKEYCODE_9) {
            text = static_cast<unsigned>('0' + (keycode - AKEYCODE_0));
        } else switch (keycode) {
            case AKEYCODE_SPACE: text = 0x20; break;
            case AKEYCODE_MINUS: text = '-'; break;
            case AKEYCODE_PERIOD: text = '.'; break;
            case AKEYCODE_COMMA: text = ','; break;
            case AKEYCODE_SLASH: text = '/'; break;
            case AKEYCODE_PLUS: text = '+'; break;
            case AKEYCODE_EQUALS: text = '='; break;
            default:
                // Non-text keys (Back/Escape, Enter, Delete, arrows) still
                // belong to the game's key path. Leave volume/system keys alone.
                if (NativeInput::EngineKeyFromNative(static_cast<unsigned>(keycode)) == 0)
                    return 0;
                break;
        }
        const unsigned long long timeMs = static_cast<unsigned long long>(
            AKeyEvent_getEventTime(event) / 1000000);
        NativeWindowEvent key{};
        key.kind = NativeWindowEventKind::Key;
        key.key = static_cast<unsigned>(keycode);
        key.pressed = pressed;
        key.timeMs = timeMs;
        EmitEvent(key);
        if (pressed && text) {
            NativeWindowEvent out{};
            out.kind = NativeWindowEventKind::Text;
            out.text = text;
            out.timeMs = timeMs;
            EmitEvent(out);
        }
        return 1;
    }
    if (type != AINPUT_EVENT_TYPE_MOTION)
        return 0;
    const auto origin = gSurfaceOrigin.load(std::memory_order_relaxed);
    const float originX = static_cast<int>(origin >> 32);
    const float originY = static_cast<int>(static_cast<unsigned int>(origin));
    const int32_t action = AMotionEvent_getAction(event);
    const int32_t masked = action & AMOTION_EVENT_ACTION_MASK;
    const auto source = AInputEvent_getSource(event);
    if (masked == AMOTION_EVENT_ACTION_CANCEL)
    {
        if ((source & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE)
        {
            gMouseButtons = 0;
            NativeWindowEvent canceled{};
            canceled.kind = NativeWindowEventKind::MouseCanceled;
            EmitEvent(canceled);
        }
        else
        {
            gTouches.CancelAll();
            gGameGesture = false;
        }
        return 1;
    }
    const auto pointerCount = AMotionEvent_getPointerCount(event);
    if (pointerCount == 0) return 0;
    const auto inside = [&](double x, double y) {
        return gWindow && x >= 0 && y >= 0 && x < ANativeWindow_getWidth(gWindow) &&
            y < ANativeWindow_getHeight(gWindow);
    };
    if ((source & AINPUT_SOURCE_MOUSE) == AINPUT_SOURCE_MOUSE)
    {
        gTouches.FlushMoves();
        const double x = AMotionEvent_getX(event, 0) - originX;
        const double y = AMotionEvent_getY(event, 0) - originY;
        gPointer = {x, y, masked != AMOTION_EVENT_ACTION_HOVER_EXIT && inside(x, y)};
        // The system caption owns a mouse press that starts outside the game.
        if (!gPointer.inside && gMouseButtons == 0) return 0;
        if (masked == AMOTION_EVENT_ACTION_SCROLL)
        {
            NativeWindowEvent wheel{};
            wheel.kind = NativeWindowEventKind::MouseWheel;
            wheel.x = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HSCROLL, 0);
            wheel.y = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_VSCROLL, 0);
            EmitEvent(wheel);
        }
        unsigned int buttons = static_cast<unsigned int>(AMotionEvent_getButtonState(event));
        if (masked == AMOTION_EVENT_ACTION_DOWN && buttons == 0)
            buttons = AMOTION_EVENT_BUTTON_PRIMARY; // Also accepts shell-generated mouse taps.
        if (masked == AMOTION_EVENT_ACTION_UP) buttons = 0;
        static constexpr unsigned int nativeButtons[]{AMOTION_EVENT_BUTTON_PRIMARY,
            AMOTION_EVENT_BUTTON_SECONDARY, AMOTION_EVENT_BUTTON_TERTIARY};
        const unsigned int changed = buttons ^ gMouseButtons;
        for (int button = 0; button < 3; ++button)
            if ((changed & nativeButtons[button]) != 0)
            {
                NativeWindowEvent out{};
                out.kind = NativeWindowEventKind::MouseButton;
                out.button = button;
                out.pressed = (buttons & nativeButtons[button]) != 0;
                out.x = x;
                out.y = y;
                out.timeMs = static_cast<unsigned long long>(AMotionEvent_getEventTime(event) / 1000000);
                EmitEvent(out);
            }
        gMouseButtons = buttons;
        return gPointer.inside || changed != 0;
    }
    if ((source & AINPUT_SOURCE_TOUCHSCREEN) != AINPUT_SOURCE_TOUCHSCREEN &&
        (source & AINPUT_SOURCE_STYLUS) != AINPUT_SOURCE_STYLUS) return 0;
    const auto pointerIndex = static_cast<size_t>((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
        >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
    if (pointerIndex >= pointerCount) return 0;
    const double x = AMotionEvent_getX(event, pointerIndex) - originX;
    const double y = AMotionEvent_getY(event, pointerIndex) - originY;
    if (masked == AMOTION_EVENT_ACTION_DOWN)
    {
        gTouches.CancelAll();
        gGameGesture = inside(x, y);
    }
    // Caption gestures remain owned by Android for their entire lifetime.
    if (!gGameGesture) return 0;
    const unsigned int device = static_cast<unsigned int>(AInputEvent_getDeviceId(event));
    const auto id = [&](size_t index) {
        return (1ULL << 63) | (static_cast<unsigned long long>(device) << 32) |
            static_cast<unsigned int>(AMotionEvent_getPointerId(event, index));
    };
    switch (masked)
    {
    case AMOTION_EVENT_ACTION_DOWN: case AMOTION_EVENT_ACTION_POINTER_DOWN:
        if (inside(x, y)) gTouches.Begin(id(pointerIndex), x, y);
        break;
    case AMOTION_EVENT_ACTION_MOVE:
        for (size_t index = 0; index < pointerCount; ++index)
            gTouches.Move(id(index), AMotionEvent_getX(event, index) - originX,
                AMotionEvent_getY(event, index) - originY);
        break;
    case AMOTION_EVENT_ACTION_UP: case AMOTION_EVENT_ACTION_POINTER_UP:
        if ((AMotionEvent_getFlags(event) & MotionFlagCanceled) != 0)
            gTouches.Cancel(id(pointerIndex));
        else gTouches.End(id(pointerIndex), x, y);
        if (masked == AMOTION_EVENT_ACTION_UP) gGameGesture = false;
        break;
    case AMOTION_EVENT_ACTION_CANCEL:
        gTouches.CancelAll();
        gGameGesture = false;
        break;
    default: break;
    }
    return 1;
}

struct NativeWindow::Impl {};
NativeWindow::NativeWindow(const NativeWindowConfig& config) : impl_(new Impl) {
    if (!gWindow) throw std::runtime_error("Android NativeWindow requires an attached ANativeWindow");
    Width = ANativeWindow_getWidth(gWindow);
    Height = ANativeWindow_getHeight(gWindow);
    if (Width <= 0) Width = config.width;
    if (Height <= 0) Height = config.height;
    // 启动即前台；焦点由 GAINED/LOST_FOCUS 校正。
    Focused = true;
    Visible = config.visible;
    gEventCallback = config.callback;
    gEventContext = config.callbackContext;
    gTouches.CancelAll();
    gTouches.callback = config.callback;
    gTouches.context = config.callbackContext;
}
NativeWindow::~NativeWindow()
{
    gEventCallback = nullptr;
    gEventContext = nullptr;
    gTouches.callback = nullptr;
    gTouches.context = nullptr;
    gTouches.CancelAll();
}
void NativeWindow::PumpEvents()
{
    if (!gApp) return;
    ConsumeTextEdit();
    // 非阻塞排空：命令/输入在本线程分发，分发即入队，渲染在同一帧稍后消费。
    int events = 0;
    android_poll_source* source = nullptr;
    while (ALooper_pollOnce(0, nullptr, &events,
            reinterpret_cast<void**>(&source)) >= 0) {
        if (source) source->process(gApp, source);
        if (gApp->destroyRequested) return;
    }
    gTouches.FlushMoves();
}
NativePointerState NativeWindow::SamplePointer() { return gPointer; }
void NativeWindow::Show() { Visible = true; }
void NativeWindow::ApplyCursor(NativeCursor) {}
void NativeWindow::SetTitle(const std::string&) {}
void NativeWindow::SetFullscreen(bool) {}
void NativeWindow::Resize(int, int)
{
    // 窗口尺寸归系统所有；几何以 QueryNativeClientExtent 直读为准。
}
double NativeWindow::RefreshRate() const
{
    if (!gApp || !gApp->activity || !gApp->activity->vm || !gApp->activity->clazz)
        return 0.0;

    JavaVM* vm = gApp->activity->vm;
    JNIEnv* env = nullptr;
    bool attachedHere = false;
    const jint status = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED)
    {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
            return 0.0;
        attachedHere = true;
    }
    else if (status != JNI_OK)
        return 0.0;

    double refreshRate = 0.0;
    if (env->PushLocalFrame(8) == JNI_OK)
    {
        // The default-display API also works on Android releases before API 30.
        // This query is made at window creation, outside the frame loop.
        do
        {
            const jclass activityClass = env->GetObjectClass(gApp->activity->clazz);
            if (env->ExceptionCheck() || !activityClass) break;
            const jmethodID getWindowManager = env->GetMethodID(activityClass,
                "getWindowManager", "()Landroid/view/WindowManager;");
            if (env->ExceptionCheck() || !getWindowManager) break;
            const jobject manager = env->CallObjectMethod(gApp->activity->clazz, getWindowManager);
            if (env->ExceptionCheck() || !manager) break;
            const jclass managerClass = env->GetObjectClass(manager);
            if (env->ExceptionCheck() || !managerClass) break;
            const jmethodID getDefaultDisplay = env->GetMethodID(managerClass,
                "getDefaultDisplay", "()Landroid/view/Display;");
            if (env->ExceptionCheck() || !getDefaultDisplay) break;
            const jobject display = env->CallObjectMethod(manager, getDefaultDisplay);
            if (env->ExceptionCheck() || !display) break;
            const jclass displayClass = env->GetObjectClass(display);
            if (env->ExceptionCheck() || !displayClass) break;
            const jmethodID getRefreshRate = env->GetMethodID(displayClass, "getRefreshRate", "()F");
            if (env->ExceptionCheck() || !getRefreshRate) break;
            const jfloat value = env->CallFloatMethod(display, getRefreshRate);
            if (env->ExceptionCheck()) break;
            if (std::isfinite(value) && value > 0.0f)
                refreshRate = value;
        } while (false);
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->PopLocalFrame(nullptr);
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    if (attachedHere) vm->DetachCurrentThread();
    return refreshRate;
}
bool NativeWindow::Minimized() const { return gWindow == nullptr; }
NativeWindowBackend NativeWindow::Backend() const { return NativeWindowBackend::Android; }
NativeSurfaceHandles NativeWindow::SurfaceHandles() const { return {nullptr, gWindow, 0}; }
}
