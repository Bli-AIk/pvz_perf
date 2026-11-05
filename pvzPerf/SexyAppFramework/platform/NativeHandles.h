#pragma once

#ifdef _WIN32
#include "windows/NativeHandles.h"
#elif defined(__APPLE__)
namespace Sexy {
struct MacWindow;
struct MacMonitor;
}
using NativeWindowHandle = Sexy::MacWindow*;
using NativeMonitorHandle = Sexy::MacMonitor*;
#elif defined(__ANDROID__)
#include <android/native_window.h>
using NativeWindowHandle = ANativeWindow*;
using NativeMonitorHandle = void*;
#elif defined(__linux__) || defined(SEXY_WINDOW_BROWSER)
namespace Sexy {
class NativeWindow;
struct NativeMonitor;
}
using NativeWindowHandle = Sexy::NativeWindow*;
// Opaque identity only. Linux HDR monitor metadata is not implemented.
using NativeMonitorHandle = Sexy::NativeMonitor*;
#else
#error Unsupported native window platform
#endif
