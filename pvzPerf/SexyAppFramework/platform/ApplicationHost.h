#pragma once
#include "NativeHandles.h"
#include "misc/FrameTiming.h"
#include <atomic>
namespace Sexy {
class SexyAppBase;
class MusicInterface;
// Selected by the executable's platform target, with no per-frame dispatch.
void HostSleepFor(unsigned int milliseconds);
void HostSleepUntil(FrameDeadline::TimePoint deadline);
void HostYield();
void HostStartLoading(SexyAppBase& app, void (*entry)(void*));
void HostFinishResourcePrewarm(SexyAppBase& app, const std::atomic<bool>& closeRequest);
MusicInterface* HostCreateMusic(bool disabled, NativeWindowHandle window);
}
