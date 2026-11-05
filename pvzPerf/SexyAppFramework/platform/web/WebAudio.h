#pragma once
#include "sound/SoundManager.h"
#include "sound/SoundInstance.h"
#include <array>
namespace Sexy {
void FinishWebSoundLoading();
class WebSoundInstance final : public SoundInstance {
public:
    int source = -1, channel = -1, basePan=0, pan=0;
    double baseVolume = 1, volume = 1, pitch = 0;
    bool released = true, autoRelease = false;
    void Release() override;
    void SetBaseVolume(double value) override;
    void SetBasePan(int value) override;
    void AdjustPitch(double value) override;
    void SetVolume(double value) override;
    void SetPan(int value) override;
    bool Play(bool looping, bool autoRelease) override;
    void Stop() override;
    bool IsPlaying() override;
    bool IsReleased() override;
    void PausePlayback(int& offset);
    void ResumePlayback(int offset, bool looping);
};
class WebSoundManager final : public SoundManager {
public:
    std::array<bool, MAX_SOURCE_SOUNDS> loaded{};
    std::array<double, MAX_SOURCE_SOUNDS> volumes{};
    std::array<int, MAX_SOURCE_SOUNDS> pans{};
    std::array<WebSoundInstance, MAX_CHANNELS> channels;
    explicit WebSoundManager(NativeWindowHandle window);
    ~WebSoundManager();
    bool LoadSound(unsigned int id, const std::string& filename) override;
    int LoadSound(const std::string& filename) override;
    void ReleaseSound(unsigned int id) override;
    void SetVolume(double value) override;
    bool SetBaseVolume(unsigned int id, double value) override;
    bool SetBasePan(unsigned int id, int value) override;
    SoundInstance* GetSoundInstance(unsigned int id) override;
    void ReleaseSounds() override;
    void ReleaseChannels() override;
    void Flush() override;
    void SetCooperativeWindow(NativeWindowHandle window) override;
    void StopAllSounds() override;
    int GetFreeSoundId() override;
    int GetNumSounds() override;
};
}
