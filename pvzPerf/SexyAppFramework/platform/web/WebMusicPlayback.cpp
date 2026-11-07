#include "WebMusicPlayback.h"
#include "sound/BassLoader.h"
#include <emscripten.h>
#include <stdexcept>
EM_JS(void, web_music_master, (double value), { Module.pvzMusic.command(0,'master',value); });
// The Web adapter stops a failed music node and reports a nonfatal warning.
// Sound effects, game state and rendering remain usable without BGM.
EM_JS(int, web_music_failed, (), { return !!Module.pvzMusic.error && !Module.pvzMusic.disabled; });
namespace Sexy {
NativeMusicPlayback::~NativeMusicPlayback() = default;
void NativeMusicPlayback::Initialize(unsigned int handle, bool) { mHandle=handle; }
void NativeMusicPlayback::Close() { Stop(); }
void NativeMusicPlayback::Play(bool restart) { gBass->BASS_ChannelPlay(mHandle,restart); }
void NativeMusicPlayback::Stop() { if(mHandle)gBass->BASS_ChannelStop(mHandle); }
void NativeMusicPlayback::Pause() { gBass->BASS_ChannelPause(mHandle); }
void NativeMusicPlayback::Resume() { gBass->BASS_ChannelResume(mHandle); }
unsigned int NativeMusicPlayback::State() const { return gBass->BASS_ChannelIsActive(mHandle); }
void NativeMusicPlayback::SetGain(float value) { gBass->BASS_ChannelSetAttribute(mHandle,BASS_ATTRIB_VOL,value); }
bool NativeMusicPlayback::Seek(unsigned long long position,unsigned int mode) { return gBass->BASS_ChannelSetPosition(mHandle,position,mode); }
unsigned long long NativeMusicPlayback::Position(unsigned int mode) const { return gBass->BASS_ChannelGetPosition(mHandle,mode); }
void NativeMusicPlayback::Check() const { if(web_music_failed())throw std::runtime_error("Web music decoder failed; see browser audio error"); }
void NativeMusicPlayback::SetMasterGain(float value) { web_music_master(value); }
}
