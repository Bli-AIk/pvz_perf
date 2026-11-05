#pragma once
#ifdef _WIN32
#include "windows/audio/DSoundManager.h"
#include "windows/audio/DSoundInstance.h"
namespace Sexy {
using NativeSoundManager = DSoundManager;
using NativeSoundInstance = DSoundInstance;
}
#elif defined(SEXY_WINDOW_BROWSER)
#include "web/WebAudio.h"
namespace Sexy {
using NativeSoundManager = WebSoundManager;
using NativeSoundInstance = WebSoundInstance;
}
#elif defined(__APPLE__)
#include "macos/audio/AudioUnitSoundManager.h"
namespace Sexy {
using NativeSoundManager = AudioUnitSoundManager;
using NativeSoundInstance = AudioUnitSoundInstance;
}
#elif defined(__ANDROID__)
// Android 音效：与桌面 Linux 同构——DecodeSound 出 float PCM，OpenAL
// 播放（openal-soft 经 vcpkg，AAudio/OpenSL 后端）。BASS 采样通道在本
// 平台"播放成功但无声"（流路径可响但每次播放建流），故 SFX 归 OpenAL；
// 音乐仍走 BASS（MusicPlayback.h 的安卓分支）。
#include "audio/openal/OpenALSoundManager.h"
#include "sound/BassLoader.h"
namespace Sexy {
using NativeSoundManager = OpenALSoundManager;
using NativeSoundInstance = OpenALSoundInstance;
// 离焦（后台/锁屏）挂起 BASS 输出设备（音乐）；OpenAL 音效走游戏
// 自身的离焦静音路径（NativeFocusChanged），与桌面一致。
inline void SetAndroidAudioSuspended(bool suspended)
{
    if (!gBass) return;
    if (suspended) gBass->BASS_Pause();
    else gBass->BASS_Start();
}
}
#else
#include "audio/openal/OpenALSoundManager.h"
namespace Sexy {
using NativeSoundManager = OpenALSoundManager;
using NativeSoundInstance = OpenALSoundInstance;
}
#endif
