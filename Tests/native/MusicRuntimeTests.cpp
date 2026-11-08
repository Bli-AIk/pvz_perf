#include "LawnApp.h"
#include "Lawn/Board.h"
#include "Lawn/System/Music.h"
#include "Lawn/System/PlayerInfo.h"
#include "Sexy.TodLib/Attachment.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/TodParticle.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "SexyAppFramework/graphics/Font.h"
#include "SexyAppFramework/graphics/Image.h"
#include "SexyAppFramework/graphics/DDInterface.h"
#include "SexyAppFramework/sound/BassMusicInterface.h"
#include "SexyAppFramework/sound/BassLoader.h"
#include "SexyAppFramework/sound/SoundDecode.h"
#include "Resources.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <cstdio>
#include <array>
#include <cmath>
#include <type_traits>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

bool (*gAppCloseRequest)() = nullptr;
static_assert(std::is_same_v<decltype(Music::mBaseBPM), float>);
static_assert(std::is_same_v<decltype(Music::mBaseModSpeed), float>);
static_assert(std::is_same_v<decltype(Sexy::BASS_INSTANCE::BASS_ChannelSetAttribute),
    BASSBOOL(WINAPI*)(DWORD, DWORD, float)>);
static_assert(std::is_same_v<decltype(Sexy::BASS_INSTANCE::BASS_ChannelGetAttribute),
    BASSBOOL(WINAPI*)(DWORD, DWORD, float*)>);
namespace
{
LONG CALLBACK SuppressFixtureCrashDialogs(EXCEPTION_POINTERS* exception)
{
    const DWORD code = exception->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR ||
        code == EXCEPTION_ILLEGAL_INSTRUCTION || code == EXCEPTION_STACK_OVERFLOW ||
        code == EXCEPTION_INT_DIVIDE_BY_ZERO)
    {
        std::fprintf(stderr, "FAIL music fixture: hardware exception %08lX\n", code);
        if (SymInitialize(GetCurrentProcess(), nullptr, TRUE))
        {
            alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
            auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage);
            symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
            symbol->MaxNameLen = MAX_SYM_NAME;
            DWORD64 displacement = 0;
            if (SymFromAddr(GetCurrentProcess(),
                reinterpret_cast<DWORD64>(exception->ExceptionRecord->ExceptionAddress),
                &displacement, symbol))
                std::fprintf(stderr, "Fault: %s + %llX\n", symbol->Name, displacement);
            void* frames[24]{};
            const auto count = CaptureStackBackTrace(0, 24, frames, nullptr);
            for (USHORT frame = 0; frame < count; ++frame)
                if (SymFromAddr(GetCurrentProcess(), reinterpret_cast<DWORD64>(frames[frame]),
                    &displacement, symbol))
                    std::fprintf(stderr, "Frame: %s + %llX\n", symbol->Name, displacement);
        }
        std::fflush(stderr);
        TerminateProcess(GetCurrentProcess(), code);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}
void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
class FixtureFont final : public Sexy::_Font
{
public:
    _Font* Duplicate() override { return new FixtureFont(*this); }
};
float Gain(Sexy::BassMusicInterface& playback, MusicFile file)
{
    float value = -1;
    Require(Sexy::gBass->BASS_ChannelGetAttribute(playback.mMusicSlots[file]->mHMusic,
        BASS_ATTRIB_VOL, &value), "Cannot query actual BASS gain");
    return value;
}
int PcmPeak(Sexy::BassMusicInterface& playback, MusicFile file)
{
    std::array<short, 16384> samples{};
    auto handle = playback.mMusicSlots[file]->mHMusic;
    const DWORD received = Sexy::gBass->BASS_ChannelGetData(handle, samples.data(), sizeof(samples));
    Require(received != DWORD(-1), "Cannot read actual production playback PCM");
    int peak = 0;
    for (size_t i = 0; i < received / sizeof(short); ++i)
        peak = std::max(peak, std::abs(static_cast<int>(samples[i])));
    return peak;
}
}
int main(int argc, char** argv)
{
    // A fixture failure must be returned to its isolated runner without a
    // game crash reporter or Windows Error Reporting dialog. This handler is
    // installed before LawnApp's constructor installs its own SEH filter.
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    AddVectoredExceptionHandler(1, SuppressFixtureCrashDialogs);
    try
    {
        Require(argc == 2, "Pass the unpacked runtime asset directory");
        LawnApp app;
        gLawnApp = &app;
        app.mReadFromRegistry = false;
        app.mNoSoundNeeded = false;
        PlayerInfo profile;
        profile.mLevel = 11;
        app.mPlayerInfo = &profile;
        FixtureFont font;
        FONT_TINYBOLD = &font;
        // Image registration uses DDInterface's critical section even when
        // no renderer/window is initialized.
        app.mDDInterface = new Sexy::DDInterface(&app);
        // SeedBank's constructor reads these two dimensions. The Board test
        // never draws; no resource renderer or resource pack is required.
        Sexy::Image seedBank(&app);
        seedBank.mWidth = 446;
        seedBank.mHeight = 87;
        IMAGE_SEEDBANK = &seedBank;
        app.mEffectSystem = new EffectSystem;
        app.mEffectSystem->EffectSystemInitialize();
        app.mMusicInterface = new Sexy::BassMusicInterface(nullptr);
        auto& playback = *static_cast<Sexy::BassMusicInterface*>(app.mMusicInterface);
        // This changes BASS's process-local master gain, never the OS mixer or
        // the running game's separate BASS instance.
        playback.SetVolume(0);
        app.mMusic = new Music;
        auto& music = *app.mMusic;
        const std::string assets = argv[1];
        music.LoadSong(MUSIC_FILE_MAIN_MUSIC, assets + "/sounds/mainmusic.mo3");
        std::exception_ptr loadError;
        unsigned workerAfterMusic = ~0U, workerAfterDecode = ~0U;
        std::thread loader([&] {
            try
            {
                music.LoadSong(MUSIC_FILE_DRUMS, assets + "/sounds/mainmusic.mo3");
                music.LoadSong(MUSIC_FILE_HIHATS, assets + "/sounds/mainmusic_hihats.mo3");
                music.LoadSong(MUSIC_FILE_CREDITS_ZOMBIES_ON_YOUR_LAWN, assets + "/sounds/ZombiesOnYourLawn.ogg");
                workerAfterMusic = Sexy::gBass->BASS_GetDevice();
                Sexy::DecodedSound decoded;
                Require(Sexy::DecodeSound(assets + "/sounds/winmusic", decoded),
                    "Worker sound decode failed");
                workerAfterDecode = Sexy::gBass->BASS_GetDevice();
            }
            catch (...) { loadError = std::current_exception(); }
        });
        loader.join();
        if (loadError) std::rethrow_exception(loadError);
        using ChannelDevice = DWORD(WINAPI*)(DWORD);
        auto channelDevice = reinterpret_cast<ChannelDevice>(
            GetProcAddress(Sexy::gBass->mModule, "BASS_ChannelGetDevice"));
        Require(channelDevice != nullptr, "BASS_ChannelGetDevice missing");
        const auto outputDevice = channelDevice(playback.mMusicSlots[MUSIC_FILE_MAIN_MUSIC]->GetHandle());
        Require(outputDevice != 0 && outputDevice == Sexy::NativeMusicPlayback::OutputDevice,
            "Main music is not bound to the initialized audible output device");
        for (auto file : {MUSIC_FILE_DRUMS, MUSIC_FILE_HIHATS, MUSIC_FILE_CREDITS_ZOMBIES_ON_YOUR_LAWN})
        {
            const auto actual = channelDevice(playback.mMusicSlots[file]->GetHandle());
            if (actual != outputDevice)
                std::cerr << "Device mismatch: file=" << int(file) << " actual=" << actual
                    << " expected=" << outputDevice << '\n';
            Require(actual == outputDevice,
                "Worker-loaded music is bound to the silent decoder device");
        }
        Require(workerAfterMusic == outputDevice && workerAfterDecode == outputDevice,
            "Sound decoding did not restore the worker's output device");
        std::cout << "PASS real worker module/stream output device=" << outputDevice
            << " and sound-decoder device restoration\n";
        Require(!music.IsMusicDisabled(), "Production music loader disabled playback");
        playback.PublishSlots();
        struct DetachFixture
        {
            LawnApp& app;
            ~DetachFixture()
            {
                // Application teardown must never try to save this test
                // board, including when Board construction itself fails.
                app.mBoard = nullptr;
                app.mPlayerInfo = nullptr;
                FONT_TINYBOLD = nullptr;
                IMAGE_SEEDBANK = nullptr;
                gLawnApp = nullptr;
            }
        } detach{app};
        Board board(&app);
        music.PlayMusic(MUSIC_TUNE_DAY_GRASSWALK);
        auto& dayMain = playback.mMusicSlots[MUSIC_FILE_MAIN_MUSIC]->mPlayback;
        const auto dayRow = [&](unsigned order, unsigned row) {
            dayMain.Pause();
            Require(dayMain.Seek(MAKELONG(order, row), BASS_POS_MUSIC_ORDER), "Cannot seek day fixture row");
            const auto normalized = music.GetMusicOrder(MUSIC_FILE_MAIN_MUSIC);
            if (normalized != unsigned(MAKELONG(order, row * 4)))
                std::cerr << "Day seek diagnostics: wanted=" << order << ':' << row
                    << " BASS-scaled=" << dayMain.Position(BASS_POS_MUSIC_ORDER) << " normalized=" << normalized << '\n';
            Require(normalized == unsigned(MAKELONG(order, row * 4)),
                "Real BASS rows were not normalized to the controller's quarter-row contract");
        };
        dayRow(0, 31);
        music.mBurstOverride = 1;
        music.StartBurst();
        app.mApplicationTimeNs = music.mBurstEndTimeNs - 500'000'000;
        music.UpdateMusicBurst(app.mApplicationTimeNs);
        Require(music.mMusicDrumsState == MUSIC_DRUMS_ON_QUEUED && Gain(playback, MUSIC_FILE_DRUMS) == 0,
            "Real daytime drums did not wait for the next half-pattern");
        dayRow(0, 32);
        music.UpdateMusicBurst(app.mApplicationTimeNs);
        Require(music.mMusicDrumsState == MUSIC_DRUMS_ON && Gain(playback, MUSIC_FILE_DRUMS) == 1,
            "Real daytime drums did not enter at decoder row 32");
        app.mApplicationTimeNs += 1'000'000'000;
        music.UpdateMusicBurst(app.mApplicationTimeNs);
        app.mApplicationTimeNs += 9'000'000'000;
        music.mBurstOverride = 2;
        music.UpdateMusicBurst(app.mApplicationTimeNs);
        Require(music.mMusicDrumsState == MUSIC_DRUMS_OFF_QUEUED,
            "Real daytime drums did not queue their release");
        dayRow(1, 0);
        music.UpdateMusicBurst(app.mApplicationTimeNs);
        Require(music.mMusicDrumsState == MUSIC_DRUMS_FADING,
            "Real daytime drums did not release across pattern wrap");
        app.mApplicationTimeNs += 500'000'000;
        music.UpdateMusicBurst(app.mApplicationTimeNs);
        Require(music.mMusicDrumsState == MUSIC_DRUMS_OFF && Gain(playback, MUSIC_FILE_DRUMS) == 0,
            "Real daytime drums remained audible after release");
        dayRow(1, 40);
        music.GameMusicPause(true);
        Require(HIWORD(music.mPauseOffset) == 40,
            "Day pause stored a row scaled down twice");
        music.GameMusicPause(false);
        dayMain.Pause();
        Require(HIWORD(dayMain.Position(BASS_POS_MUSIC_ORDER)) / 4 == 40,
            "Day resume returned to a different raw decoder row");
        std::cout << "PASS production day BASS: raw row 31/32 entry, pattern-wrap release, zero gain and exact pause row\n";
        music.StopAllMusic();
        music.PlayMusic(MUSIC_TUNE_NIGHT_MOONGRAINS);
        Require(music.mCurMusicFileMain == MUSIC_FILE_MAIN_MUSIC &&
            music.mCurMusicFileDrums == MUSIC_FILE_DRUMS &&
            music.mCurMusicFileHihats == MUSIC_FILE_NONE, "Production night layer IDs changed");
        Require(LOWORD(music.GetMusicOrder(MUSIC_FILE_MAIN_MUSIC)) == 48 &&
            LOWORD(music.GetMusicOrder(MUSIC_FILE_DRUMS)) == 92, "Production night start positions changed");
        Require(playback.IsPlaying(MUSIC_FILE_MAIN_MUSIC) && playback.IsPlaying(MUSIC_FILE_DRUMS),
            "Production night playback did not start both channels");
        Require(Gain(playback, MUSIC_FILE_MAIN_MUSIC) == 1 &&
            Gain(playback, MUSIC_FILE_DRUMS) == 0,
            "Production night initial gains changed");
        float nativeBpm = 0, nativeSpeed = 0;
        const auto mainHandle = playback.mMusicSlots[MUSIC_FILE_MAIN_MUSIC]->mHMusic;
        Require(Sexy::gBass->BASS_ChannelGetAttribute(mainHandle, BASS_ATTRIB_MUSIC_BPM, &nativeBpm) &&
            Sexy::gBass->BASS_ChannelGetAttribute(mainHandle, BASS_ATTRIB_MUSIC_SPEED, &nativeSpeed),
            "Cannot read native module tempo attributes");
        Require(music.mBaseBPM == nativeBpm && music.mBaseModSpeed == nativeSpeed &&
            nativeBpm > 0 && nativeSpeed > 0,
            "Music did not retain BASS float tempo attributes");
        for (int channel = 0; channel < 30; ++channel)
        {
            float gain = -1;
            Require(Sexy::gBass->BASS_ChannelGetAttribute(
                playback.mMusicSlots[MUSIC_FILE_DRUMS]->mHMusic,
                BASS_ATTRIB_MUSIC_VOL_CHAN + channel, &gain) && gain == 1,
                "Production track setup muted a night climax channel");
        }
        // Keep the real, initially muted playback channel in the silent holding
        // pattern long enough to expose decoder/effect-state history problems.
        std::this_thread::sleep_for(std::chrono::seconds(21));
        Require(playback.IsPlaying(MUSIC_FILE_DRUMS), "Silent night holding channel stopped");
        Require(LOWORD(music.GetMusicOrder(MUSIC_FILE_DRUMS)) == 92,
            "Silent night holding channel left order 92");
        for (unsigned fps : {30u, 60u, 100u, 144u, 240u, 1000u})
        {
            Sexy::gUpdateIntervalNs = 1'000'000'000ULL / fps;
            if (fps != 30)
            {
                music.StopAllMusic();
                music.PlayMusic(MUSIC_TUNE_NIGHT_MOONGRAINS);
            }
            music.mBurstOverride = 1;
            music.StartBurst();
            music.UpdateMusicBurst(app.mApplicationTimeNs);
            Require(music.mMusicDrumsState == MUSIC_DRUMS_ON_QUEUED, "Production climax was not queued");
            playback.mMusicSlots[MUSIC_FILE_MAIN_MUSIC]->mPlayback.Seek(49, BASS_POS_MUSIC_ORDER);
            music.UpdateMusicBurst(app.mApplicationTimeNs);
            Require(music.mMusicDrumsState == MUSIC_DRUMS_ON, "Production climax did not start");
            Require(LOWORD(music.GetMusicOrder(MUSIC_FILE_DRUMS)) == 77,
                "Production climax seek missed order 77");
            for (unsigned i = 0; i < fps * 5; ++i)
            {
                app.mApplicationTimeNs += static_cast<std::int64_t>(Sexy::gUpdateIntervalNs);
                music.MusicUpdate(app.mApplicationTimeNs);
            }
            Require(music.mMusicBurstState == MUSIC_BURST_ON, "Production night crossfade stalled");
            Require(Gain(playback, MUSIC_FILE_MAIN_MUSIC) == 0 &&
                Gain(playback, MUSIC_FILE_DRUMS) == 1, "Actual BASS climax gains are incorrect");
            Require(playback.IsPlaying(MUSIC_FILE_DRUMS), "Actual BASS climax channel stopped");
            const auto previous = music.GetMusicOrder(MUSIC_FILE_DRUMS);
            for (int poll = 0; poll < 50 && music.GetMusicOrder(MUSIC_FILE_DRUMS) == previous; ++poll)
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            if (music.GetMusicOrder(MUSIC_FILE_DRUMS) == previous)
            {
                float bpm = -1, speed = -1;
                auto handle = playback.mMusicSlots[MUSIC_FILE_DRUMS]->mHMusic;
                Sexy::gBass->BASS_ChannelGetAttribute(handle, BASS_ATTRIB_MUSIC_BPM, &bpm);
                Sexy::gBass->BASS_ChannelGetAttribute(handle, BASS_ATTRIB_MUSIC_SPEED, &speed);
                std::cerr << "Clock diagnostics: position=" << previous << " bpm=" << bpm
                    << " speed=" << speed << " baseBpm=" << music.mBaseBPM << '\n';
            }
            Require(music.GetMusicOrder(MUSIC_FILE_DRUMS) != previous, "Actual climax playback clock stalled");
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            float global = -1, amplify = -1;
            auto handle = playback.mMusicSlots[MUSIC_FILE_DRUMS]->mHMusic;
            Sexy::gBass->BASS_ChannelGetAttribute(handle, BASS_ATTRIB_MUSIC_VOL_GLOBAL, &global);
            Sexy::gBass->BASS_ChannelGetAttribute(handle, BASS_ATTRIB_MUSIC_AMPLIFY, &amplify);
            const int peak = PcmPeak(playback, MUSIC_FILE_DRUMS);
            if (peak <= 100)
                std::cerr << "Silent climax: peak=" << peak << " moduleGlobal=" << global
                    << " amplify=" << amplify << '\n';
            Require(peak > 100, "Actual production climax output samples are silent");
            music.GameMusicPause(true);
            Require(music.mPaused && !playback.IsPlaying(MUSIC_FILE_DRUMS),
                "Production pause did not stop the climax");
            music.GameMusicPause(false);
            music.MusicUpdate(app.mApplicationTimeNs);
            Require(!music.mPaused && playback.IsPlaying(MUSIC_FILE_DRUMS) &&
                Gain(playback, MUSIC_FILE_DRUMS) == 1, "Production resume lost the climax");
            std::cout << "PASS production Music/BassMusicInterface: night setup, seek, gains, clock, pause/resume at "
                << fps << " FPS; PCM peak=" << peak << " moduleGlobal=" << global
                << " amplify=" << amplify << '\n';
        }
        music.StopAllMusic();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL production music runtime: " << error.what() << '\n';
        return 1;
    }
}
