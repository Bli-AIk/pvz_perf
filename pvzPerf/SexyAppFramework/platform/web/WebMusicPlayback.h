#pragma once
namespace Sexy {
class NativeMusicPlayback final {
public:
    unsigned int mHandle = 0;
    static constexpr bool PerVoiceMasterGain = false;
    NativeMusicPlayback() = default;
    NativeMusicPlayback(const NativeMusicPlayback&) = delete;
    NativeMusicPlayback& operator=(const NativeMusicPlayback&) = delete;
    ~NativeMusicPlayback();
    void Initialize(unsigned int handle, bool module);
    void Close();
    void Play(bool restart = false);
    void Stop();
    void Pause();
    void Resume();
    unsigned int State() const;
    void SetGain(float value);
    bool Seek(unsigned long long position, unsigned int mode);
    unsigned long long Position(unsigned int mode) const;
    void Check() const;
    static void SetMasterGain(float value);
};
}
