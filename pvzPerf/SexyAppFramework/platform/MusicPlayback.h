#pragma once
#ifdef _WIN32
#include "windows/audio/MusicPlayback.h"
#elif defined(SEXY_WINDOW_BROWSER)
#include "web/WebMusicPlayback.h"
#elif defined(__APPLE__)
#include "macos/audio/AudioUnitMusicPlayback.h"
#elif defined(__ANDROID__)
// Android 音乐输出：BASS 自带 AAudio 后端（LoadBassDLL 的 BASS_Init(-1)
// 已建好输出设备），通道操作与 Windows 同构——此前误走 Linux 的
// DECODE+自绘输出路径，导致通道只解码不发声。
#include "sound/BassLoader.h"
namespace Sexy {
class NativeMusicPlayback final {
    unsigned int mHandle = 0;
public:
    static constexpr bool PerVoiceMasterGain = false;
    NativeMusicPlayback() = default;
    NativeMusicPlayback(const NativeMusicPlayback&) = delete;
    NativeMusicPlayback& operator=(const NativeMusicPlayback&) = delete;
    ~NativeMusicPlayback() = default;
    void Initialize(unsigned int handle, bool) { mHandle = handle; }
    void Close() {}  // 输出设备生命周期归 LoadBassDLL/FreeBassDLL 所有
    void Play(bool restart = false) { gBass->BASS_ChannelPlay(mHandle, restart); }
    void Stop() { gBass->BASS_ChannelStop(mHandle); }
    void Pause() { gBass->BASS_ChannelPause(mHandle); }
    void Resume() { gBass->BASS_ChannelResume(mHandle); }
    unsigned int State() const { return gBass->BASS_ChannelIsActive(mHandle); }
    void SetGain(float value);
    bool Seek(unsigned long long position, unsigned int mode) {
        return gBass->BASS_ChannelSetPosition(mHandle, position, mode) != FALSE;
    }
    unsigned long long Position(unsigned int mode) const {
        return gBass->BASS_ChannelGetPosition(mHandle, mode);
    }
    void Check() const {}
    static void SetMasterGain(float value);
};
}
#else
#include "audio/openal/OpenALMusicPlayback.h"
#endif
