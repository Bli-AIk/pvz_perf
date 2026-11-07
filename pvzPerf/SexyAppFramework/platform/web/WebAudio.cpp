#include "WebAudio.h"
#include "WebOggDecoder.h"
#include "paklib/PakInterface.h"
#include <emscripten.h>
#include <algorithm>
#include <vector>
#include <cmath>
#include <cstring>
#include <stdexcept>

EM_JS(void, web_sound_load, (int id, const unsigned char* data, int size), {
    Module.pvzMusic.sound.load(id,HEAPU8.slice(Number(data),Number(data)+size));
});
EM_JS(void, web_sound_pcm, (int id, const float* data, int samples, int channels, int rate), {
    Module.pvzMusic.sound.loadPCM(id,HEAPF32.subarray(Number(data)/4,Number(data)/4+samples),channels,rate);
});
EM_JS(void, web_sound_release, (int id), { Module.pvzMusic.sound.release(id); });
EM_JS(int, web_sound_play, (int id,int channel,int loop,double volume,double pitch,int pan), {
    return Module.pvzMusic.sound.play(id,channel,!!loop,volume,pitch,pan);
});
EM_JS(void, web_sound_stop, (int channel), { Module.pvzMusic.sound.stop(channel); });
EM_JS(int, web_sound_playing, (int channel), { return Module.pvzMusic.sound.playing(channel); });
EM_JS(void, web_sound_volume, (double volume), { Module.pvzMusic.sound.volume(volume); });
EM_JS(int, web_sound_pause, (int channel), { return Module.pvzMusic.sound.pause(channel); });
EM_JS(void, web_sound_resume, (int channel,int offset,int loop), { Module.pvzMusic.sound.resume(channel,offset,!!loop); });
EM_JS(void, web_sound_parameters, (int channel,double volume,double pitch,int pan), { Module.pvzMusic.sound.parameters(channel,volume,pitch,pan); });
EM_ASYNC_JS(int, web_sound_finish, (), {
    try { await Module.pvzMusic.sound.finish(); return 1; }
    catch(error) { Module.pvzOnError?.('音效加载失败：'+String(error)); return 0; }
});

namespace Sexy {
void FinishWebSoundLoading() {
    if (!web_sound_finish()) throw std::runtime_error("Web sound loading failed");
}
void WebSoundInstance::Release() { Stop(); released = true; }
void WebSoundInstance::SetBaseVolume(double v) { baseVolume=v; web_sound_parameters(channel,baseVolume*volume,pitch,basePan+pan); }
void WebSoundInstance::SetBasePan(int v) { basePan=v; web_sound_parameters(channel,baseVolume*volume,pitch,basePan+pan); }
void WebSoundInstance::AdjustPitch(double v) { pitch=v; web_sound_parameters(channel,baseVolume*volume,pitch,basePan+pan); }
void WebSoundInstance::SetVolume(double v) { volume=v; web_sound_parameters(channel,baseVolume*volume,pitch,basePan+pan); }
void WebSoundInstance::SetPan(int v) { pan=v; web_sound_parameters(channel,baseVolume*volume,pitch,basePan+pan); }
bool WebSoundInstance::Play(bool loop, bool releaseWhenDone) { autoRelease = releaseWhenDone; return web_sound_play(source, channel, loop, baseVolume * volume, pitch, basePan+pan); }
void WebSoundInstance::Stop() { web_sound_stop(channel); }
bool WebSoundInstance::IsPlaying() { return web_sound_playing(channel); }
bool WebSoundInstance::IsReleased() { return released; }
void WebSoundInstance::PausePlayback(int& offset) { offset = web_sound_pause(channel); }
void WebSoundInstance::ResumePlayback(int offset, bool loop) { web_sound_resume(channel, offset, loop); }
WebSoundManager::WebSoundManager(NativeWindowHandle) {
    mInitialized = true; volumes.fill(1);
    for (int i = 0; i < MAX_CHANNELS; ++i) channels[i].channel = i;
}
WebSoundManager::~WebSoundManager() { ReleaseChannels(); ReleaseSounds(); }
bool WebSoundManager::LoadSound(unsigned int id, const std::string& filename) {
    if (id >= loaded.size()) return false;
    PFILE* file = nullptr;
    for (const char* extension : {".ogg", ".wav", ".au"}) {
        file = p_fopen((filename + extension).c_str(), "rb");
        if (file) break;
    }
    if (!file) return false;
    p_fseek(file, 0, SEEK_END); const auto size = p_ftell(file); p_fseek(file, 0, SEEK_SET);
    std::vector<unsigned char> data(size);
    const auto read = p_fread(data.data(), 1, data.size(), file); p_fclose(file);
    if (read != data.size()) return false;
    try {
        if (data.size() >= 4 && std::memcmp(data.data(), "OggS", 4) == 0) {
            const auto pcm = DecodeWebOgg(data);
            web_sound_pcm(id, pcm.samples.data(), static_cast<int>(pcm.samples.size()), pcm.channels, pcm.sampleRate);
        } else web_sound_load(id, data.data(), data.size());
    } catch (const std::exception& error) {
        throw std::runtime_error("Sound load failed: " + filename + ": " + error.what());
    }
    loaded[id] = true; return true;
}
int WebSoundManager::LoadSound(const std::string& name) { const int id = GetFreeSoundId(); return id >= 0 && LoadSound(id, name) ? id : -1; }
void WebSoundManager::ReleaseSound(unsigned int id) { if (id < loaded.size() && loaded[id]) { web_sound_release(id); loaded[id] = false; } }
void WebSoundManager::SetVolume(double v) { web_sound_volume(v); }
bool WebSoundManager::SetBaseVolume(unsigned int id, double v) { if (id >= volumes.size()) return false; volumes[id] = v; return true; }
bool WebSoundManager::SetBasePan(unsigned int id, int value) { if(id>=pans.size())return false;pans[id]=value;return true; }
SoundInstance* WebSoundManager::GetSoundInstance(unsigned int id) {
    if (id >= loaded.size() || !loaded[id]) return nullptr;
    for (auto& c : channels) if (c.released || (c.autoRelease && !c.IsPlaying())) {
        c.source = id; c.baseVolume = volumes[id]; c.volume = 1; c.pitch = 0; c.basePan=pans[id]; c.pan=0;
        c.released = false; c.autoRelease = false; return &c;
    }
    return nullptr;
}
void WebSoundManager::ReleaseSounds() { for (unsigned i = 0; i < loaded.size(); ++i) ReleaseSound(i); }
void WebSoundManager::ReleaseChannels() { for (auto& c : channels) c.Release(); }
void WebSoundManager::Flush() {}
void WebSoundManager::SetCooperativeWindow(NativeWindowHandle) {}
void WebSoundManager::StopAllSounds() { ReleaseChannels(); }
int WebSoundManager::GetFreeSoundId() { for (int i = 0; i < MAX_SOURCE_SOUNDS; ++i) if (!loaded[i]) return i; return -1; }
int WebSoundManager::GetNumSounds() { return std::count(loaded.begin(), loaded.end(), true); }
}
