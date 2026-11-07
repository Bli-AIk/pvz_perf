#include "platform/ApplicationHost.h"
#include "WebAudio.h"
#include "Common.h"
#include "SexyAppBase.h"
#include "sound/BassMusicInterface.h"
#include "sound/DummyMusicInterface.h"
#include "misc/FloatingPointMode.h"
#include "Sexy.TodLib/TodCommon.h"
#include "Sexy.TodLib/Reanimator.h"
#include <emscripten.h>
#include <chrono>
namespace {
bool browserFrameReady = false;
EM_ASYNC_JS(void, WaitForBrowserFrame, (), {
    await new Promise(resolve => requestAnimationFrame(time => { Module.pvzObserveFrame?.(time); resolve(); }));
});
}
namespace Sexy {
void HostSleepFor(unsigned int milliseconds) { emscripten_sleep(milliseconds); }
void HostSleepUntil(FrameDeadline::TimePoint deadline) {
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(deadline - FrameDeadline::Clock::now()).count();
    if (ns > 0) {
        WaitForBrowserFrame();
        browserFrameReady = true;
    }
}
void HostYield() {
    // A timer-driven loop can submit several canvas frames between browser
    // paints. Yield to the compositor, not a nested sleep(0) timer. If the
    // scheduler already awaited this browser frame, do not wait a second time.
    if (gSexyAppBase && gSexyAppBase->mUpdateAppState == UPDATESTATE_PROCESS_2) {
        if (!browserFrameReady) WaitForBrowserFrame();
        browserFrameReady = false;
    }
}
void HostStartLoading(SexyAppBase& app, void (*entry)(void*)) {
    EM_ASM({ Module.pvzStartupPhase?.('加载游戏资源'); });
    entry(&app);
}
void HostFinishResourcePrewarm(SexyAppBase&, const std::atomic<bool>&) {
    EM_ASM({ Module.pvzStartupPhase?.('解码音效与上传纹理'); });
    FinishWebSoundLoading();
    TodWarmDrainAll();
    ReanimatorUploadAllTexturesToGPU();
    TodWarmMarkSweepClean();
}
MusicInterface* HostCreateMusic(bool disabled, NativeWindowHandle window) {
    if(disabled)return new DummyMusicInterface;
    return new BassMusicInterface(window);
}
std::string GetExecutableDirectory() { return "/"; }
void EnableDenormalFlushForCurrentThread() noexcept {}
}
