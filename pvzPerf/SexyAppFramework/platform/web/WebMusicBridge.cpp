#include "sound/BassLoader.h"
#include "WebOggDecoder.h"
#include <emscripten.h>
#include <cstring>
// ABI adapter for the existing music controller. No BASS binary is loaded.
EM_JS(unsigned int, web_music_load, (const void* data,unsigned int size,int stream), {
    return Module.pvzMusic.load(HEAPU8.slice(Number(data),Number(data)+size),!!stream);
});
EM_JS(unsigned int, web_music_pcm, (const float* data,int samples,int channels,int rate), {
    return Module.pvzMusic.loadStream(HEAPF32.subarray(Number(data)/4,Number(data)/4+samples),channels,rate);
});
EM_JS(double, web_music_command, (unsigned int id,const char* op,double value,int extra), {
    return Module.pvzMusic.command(id,UTF8ToString(Number(op)),value,extra);
});
EM_ASYNC_JS(double, web_music_attribute, (unsigned int id,const char* op), {
    await Module.pvzMusic.flush();
    return Module.pvzMusic.command(id,UTF8ToString(Number(op)));
});
namespace {
int references=0;
unsigned int LoadModule(BASSBOOL memory,const void* data,QWORD offset,DWORD size,DWORD,DWORD) {
    if(!memory||!data||!size||offset)return 0;
    return web_music_load(data,size,false);
}
unsigned int LoadStream(BASSBOOL memory,const void* data,QWORD offset,QWORD size,DWORD) {
    if(!memory||!data||!size||size>0xffffffffULL||offset)return 0;
    const auto pcm=Sexy::DecodeWebOgg({static_cast<const unsigned char*>(data),static_cast<size_t>(size)});
    return web_music_pcm(pcm.samples.data(),static_cast<int>(pcm.samples.size()),pcm.channels,pcm.sampleRate);
}
BASSBOOL FreeVoice(DWORD id) { return web_music_command(id,"free",0,0); }
BASSBOOL Play(DWORD id,BASSBOOL restart) { return web_music_command(id,"play",restart,0); }
BASSBOOL Stop(DWORD id) { return web_music_command(id,"stop",0,0); }
BASSBOOL Pause(DWORD id) { return web_music_command(id,"pause",0,0); }
DWORD State(DWORD id) { return web_music_command(id,"state",0,0); }
BASSBOOL SetAttribute(DWORD id,DWORD attribute,float value) {
    if(attribute>=BASS_ATTRIB_MUSIC_VOL_CHAN)return web_music_command(id,"channel",value,attribute-BASS_ATTRIB_MUSIC_VOL_CHAN);
    switch(attribute){
    case BASS_ATTRIB_VOL:return web_music_command(id,"gain",value,0);
    case BASS_ATTRIB_MUSIC_BPM:return web_music_command(id,"tempo",value,0);
    case BASS_ATTRIB_MUSIC_SPEED:return web_music_command(id,"speed",value,0);
    case BASS_ATTRIB_MUSIC_AMPLIFY:return web_music_command(id,"amplify",value/50.0f,0);
    case BASS_ATTRIB_MUSIC_PSCALER:return web_music_command(id,"pscaler",value,0);
    default:return false;
    }
}
BASSBOOL GetAttribute(DWORD id,DWORD attribute,float* value) {
    if(!value)return false;
    if(attribute==BASS_ATTRIB_MUSIC_BPM)*value=web_music_attribute(id,"tempoGet");
    else if(attribute==BASS_ATTRIB_MUSIC_SPEED)*value=web_music_attribute(id,"speedGet");
    else return false;
    return true;
}
BASSBOOL Seek(DWORD id,QWORD position,DWORD mode) {
    if((mode&0xffff)!=BASS_POS_BYTE&&(mode&0xffff)!=BASS_POS_MUSIC_ORDER)return false;
    return web_music_command(id,"seek",static_cast<double>(position),0);
}
QWORD Position(DWORD id,DWORD) { return static_cast<QWORD>(web_music_command(id,"position",0,0)); }
DWORD Flags(DWORD id,DWORD flags,DWORD mask) { return web_music_command(id,"flags",flags,mask); }
BASSBOOL Configure(DWORD option,DWORD) { return option==BASS_CONFIG_BUFFER; }
int ErrorCode() { return 0; }
}
namespace Sexy {
BASS_INSTANCE* gBass=nullptr;
BASS_INSTANCE::BASS_INSTANCE(const char*) {
    std::memset(this,0,sizeof(*this));
    BASS_MusicLoad=LoadModule;BASS_MusicFree=FreeVoice;
    BASS_StreamCreateFile=LoadStream;BASS_StreamFree=FreeVoice;
    BASS_ChannelPlay=Play;BASS_ChannelStop=Stop;BASS_ChannelPause=Pause;BASS_ChannelIsActive=State;
    BASS_ChannelSetAttribute=SetAttribute;BASS_ChannelGetAttribute=GetAttribute;
    BASS_ChannelSetPosition=Seek;BASS_ChannelGetPosition=Position;BASS_ChannelFlags=Flags;
    BASS_SetConfig=Configure;BASS_ErrorGetCode=ErrorCode;
}
BASS_INSTANCE::~BASS_INSTANCE() = default;
BASSBOOL BASS_INSTANCE::BASS_MusicSetAmplify(HMUSIC id,float value) { return SetAttribute(id,BASS_ATTRIB_MUSIC_AMPLIFY,value); }
BASSBOOL BASS_INSTANCE::BASS_ChannelResume(DWORD id) { return web_music_command(id,"resume",0,0); }
void LoadBassDLL() { if(references++==0)gBass=new BASS_INSTANCE(nullptr); }
void FreeBassDLL() { if(references>0&&--references==0){delete gBass;gBass=nullptr;} }
}
