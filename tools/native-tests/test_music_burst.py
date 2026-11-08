"""Compile the real music controller against deterministic playback/board doubles."""
from pathlib import Path
import ctypes
import os
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


class MusicBurstTests(unittest.TestCase):
    def test_shipped_night_module(self):
        if os.name != "nt":
            self.skipTest("This decoder probe uses the shipped Windows BASS library")
        candidates = list((ROOT / "out").glob("*/Release/bass.dll"))
        if not candidates:
            self.skipTest("Build the game to obtain the shipped BASS decoder")
        bass = ctypes.WinDLL(str(candidates[0]))
        bass.BASS_Init.argtypes = [ctypes.c_int, ctypes.c_uint, ctypes.c_uint,
                                  ctypes.c_void_p, ctypes.c_void_p]
        bass.BASS_MusicLoad.argtypes = [ctypes.c_int, ctypes.c_void_p, ctypes.c_ulonglong,
                                       ctypes.c_uint, ctypes.c_uint, ctypes.c_uint]
        bass.BASS_ChannelSetPosition.argtypes = [ctypes.c_uint, ctypes.c_ulonglong, ctypes.c_uint]
        bass.BASS_ChannelGetPosition.argtypes = [ctypes.c_uint, ctypes.c_uint]
        bass.BASS_ChannelGetPosition.restype = ctypes.c_ulonglong
        bass.BASS_ChannelSetAttribute.argtypes = [ctypes.c_uint, ctypes.c_uint, ctypes.c_float]
        bass.BASS_ChannelGetData.argtypes = [ctypes.c_uint, ctypes.c_void_p, ctypes.c_uint]
        bass.BASS_ChannelGetData.restype = ctypes.c_uint
        # Device 0 is BASS's silent device; decode channels never produce output.
        self.assertTrue(bass.BASS_Init(0, 44100, 0, None, None))
        try:
            module = str(ROOT / "assets/runtime/sounds/mainmusic.mo3").encode()
            # DECODE | RAMP | LOOP, matching the production module plus decode.
            handle = bass.BASS_MusicLoad(0, module, 0, 0, 0x200204, 44100)
            self.assertNotEqual(handle, 0)
            try:
                self.assertTrue(bass.BASS_ChannelSetAttribute(handle, 0x102, 4))
                pcm = (ctypes.c_int16 * (44100 * 2))()

                def decode_second():
                    self.assertEqual(bass.BASS_ChannelGetData(handle, pcm, ctypes.sizeof(pcm)),
                                     ctypes.sizeof(pcm))
                    return max(abs(sample) for sample in pcm)

                self.assertTrue(bass.BASS_ChannelSetPosition(handle, 92, 1))
                for _ in range(20):
                    self.assertEqual(decode_second(), 0, "Night holding pattern became audible")
                self.assertEqual(bass.BASS_ChannelGetPosition(handle, 1) & 0xffff, 92)
                for order in (76, 77):
                    self.assertTrue(bass.BASS_ChannelSetPosition(handle, order, 1))
                    self.assertGreater(decode_second(), 1000, "Night climax contains no audible samples")
                    position = bass.BASS_ChannelGetPosition(handle, 1)
                    self.assertEqual(position & 0xffff, order)
                    self.assertGreater(position >> 16, 0, "Night module clock did not advance")
                print("PASS shipped night module: silent holding loop and both audible climax entry orders")
            finally:
                bass.BASS_MusicFree(handle)
        finally:
            bass.BASS_Free()

    def test_production_gain_fade(self):
        production = (ROOT / "pvzPerf/SexyAppFramework/sound/BassMusicInterface.cpp").read_text(encoding="utf-8")
        start = production.index("void ApplyVolumeFade(")
        opening = production.index("{", start)
        depth, end = 1, opening + 1
        while depth:
            depth += (production[end] == "{") - (production[end] == "}")
            end += 1
        source = r'''
#include <algorithm>
#include <cstdint>
#include <stdexcept>
struct Playback {
    float gain = -1;
    int stops = 0;
    void SetGain(float value) { gain = value; }
    void Stop() { ++stops; }
};
struct BassMusicInfo {
    float mVolume = 0, mVolumeCap = 1, mFadeOrigin = 0, mFadeTarget = 1;
    std::int64_t mFadeStartTimeNs = 0;
    std::uint64_t mFadeDurationNs = 1'000'000'000;
    bool mStopOnFade = false;
    Playback mPlayback;
};
'''
        source += production[start:end]
        source += r'''
int main() {
    BassMusicInfo first, second;
    first.mFadeStartTimeNs = second.mFadeStartTimeNs = 9'000'000'000;
    ApplyVolumeFade(&first, 9'250'000'000);
    for (auto now : {9'000'000'001LL, 9'100'000'000LL, 9'250'000'000LL})
        ApplyVolumeFade(&second, now);
    if (first.mVolume != .25f || first.mVolume != second.mVolume)
        throw std::runtime_error("gain depends on number of updates");
    first.mFadeOrigin = first.mVolume;
    first.mFadeTarget = 0;
    first.mStopOnFade = true;
    first.mFadeStartTimeNs = 9'250'000'000;
    ApplyVolumeFade(&first, 9'750'000'000);
    if (first.mVolume != .125f || first.mPlayback.stops)
        throw std::runtime_error("interrupted float fade lost its origin");
    ApplyVolumeFade(&first, 11'000'000'000);
    ApplyVolumeFade(&first, 12'000'000'000);
    if (first.mVolume != 0 || first.mPlayback.stops != 1 || first.mFadeDurationNs != 0)
        throw std::runtime_error("fade completion did not clamp/stop once");
}
'''
        compiler = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
        vcvars = Path("C:/UserProgram/Visual Studuio/2026/Community/VC/Auxiliary/Build/vcvars64.bat")
        with tempfile.TemporaryDirectory(prefix="pvz-music-gain-") as temp:
            source_path = Path(temp) / "music_gain.cpp"
            executable = Path(temp) / ("music_gain.exe" if os.name == "nt" else "music_gain")
            source_path.write_text(source, encoding="utf-8")
            if compiler:
                command = [compiler, "-std=c++20", str(source_path), "-o", str(executable)]
            elif vcvars.exists():
                toolset = Path('C:/UserProgram/Visual Studuio/2026/Community/VC/Tools/MSVC/14.51.36231')
                build = (f'call "{vcvars}" >nul && set "INCLUDE={toolset}/include;!INCLUDE!" && '
                         f'set "LIB={toolset}/lib/x64;!LIB!" && "{toolset}/bin/Hostx64/x64/cl.exe" '
                         f'/nologo /EHsc /std:c++20 "{source_path}" /Fo"{temp}/music_gain.obj" /Fe"{executable}"')
                command = f'"{os.environ.get("COMSPEC", "cmd.exe")}" /d /v:on /s /c "{build}"'
            else:
                self.skipTest("A C++20 compiler is required")
            subprocess.run(command, cwd=temp, check=True)
            subprocess.run([str(executable)], cwd=temp, check=True)

    def test_production_controller(self):
        compiler = shutil.which("c++") or shutil.which("g++") or shutil.which("clang++")
        vcvars = Path("C:/UserProgram/Visual Studuio/2026/Community/VC/Auxiliary/Build/vcvars64.bat")
        if not compiler and not vcvars.exists():
            self.skipTest("A C++20 compiler is required")
        production = (ROOT / "pvzPerf/Lawn/System/Music.cpp").read_text(encoding="utf-8")

        def definition(name, result="void"):
            start = production.index(f"{result} Music::{name}(")
            opening = production.index("{", start)
            depth = 1
            end = opening + 1
            while depth:
                depth += (production[end] == "{") - (production[end] == "}")
                end += 1
            return production[start:end]

        source = (ROOT / "Tests/native/MusicBurstTests.cpp").read_text(encoding="utf-8")
        source += "\nusing namespace Sexy;\n"
        source += definition("GetMusicOrder", "unsigned int") + "\n"
        source += definition("StartBurst") + "\n" + definition("UpdateMusicBurst") + "\n" + definition("MusicUpdate") + "\n"
        source += r'''
void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
int main()
{
    for (unsigned fps : {30u, 60u, 100u, 144u, 240u, 250u, 1000u})
    {
        Sexy::gUpdateIntervalNs = 1'000'000'000ULL / fps;
        Board board;
        LawnApp app;
        app.mBoard = &board;
        Sexy::BassMusicInterface playback;
        app.mMusicInterface = &playback;
        Music music;
        music.mApp = &app;
        music.mMusicInterface = &playback;
        const auto step = [&] {
            app.mApplicationTimeNs += static_cast<std::int64_t>(Sexy::gUpdateIntervalNs);
            music.UpdateMusicBurst(app.mApplicationTimeNs);
        };
        currentOrder = MAKELONG(48, 20);
        step();
        Require(music.mMusicBurstState == MUSIC_BURST_STARTING, "Crowd did not trigger night burst");
        step();
        Require(music.mMusicDrumsState == MUSIC_DRUMS_ON_QUEUED, "Night layer was not queued");
        for (unsigned i = 0; i < fps * 2; ++i) step();
        Require(music.mBurstEndTimeNs - app.mApplicationTimeNs == 4'000'000'000LL, "Queue wait consumed night crossfade");
        Require(playback.mMusicSlots[2]->mPlayback.seeks == 0, "Night layer jumped before bar boundary");
        currentOrder = MAKELONG(49, 0);
        step();
        Require(music.mMusicDrumsState == MUSIC_DRUMS_ON, "Night layer did not start at boundary");
        Require(playback.mMusicSlots[2]->mPlayback.order == 77, "Odd night bar selected wrong climax");
        Require(playback.volume[2] == 1.0, "Night climax remained muted");
        for (unsigned i = 0; i < fps * 5; ++i) step();
        Require(music.mMusicBurstState == MUSIC_BURST_ON, "Night crossfade never finished");
        Require(playback.volume[1] == 0.0 && playback.volume[2] == 1.0,
            "Night climax gains are incorrect");
        board.zombies = 0;
        for (unsigned i = 0; i < fps * 22; ++i) step();
        Require(music.mMusicBurstState == MUSIC_BURST_OFF &&
            music.mMusicDrumsState == MUSIC_DRUMS_OFF, "Night climax never returned to calm");
        Require(playback.volume[1] == 1.0 && playback.volume[2] == 0.0,
            "Night calm gains are incorrect");
        music.StartBurst();
        step();
        currentOrder = MAKELONG(50, 0);
        step();
        Require(playback.mMusicSlots[2]->mPlayback.order == 76, "Even night bar selected wrong climax");
        Require(playback.mMusicSlots[2]->mPlayback.seeks == 2, "Climax did not retrigger exactly once");
        std::cout << "PASS night music burst/crossfade/release/retrigger at " << fps << " FPS\n";
        // Advance the application clock through a board pause. Generic fade
        // completes; controller deadlines retain their remaining durations.
        music.mMusicBurstState = MUSIC_BURST_OFF;
        music.StartBurst();
        music.mLastUpdateTimeNs = app.mApplicationTimeNs;
        music.mFadeOutDurationNs = 1'000'000'000;
        music.mFadeOutEndTimeNs = app.mApplicationTimeNs + music.mFadeOutDurationNs;
        const auto burstRemaining = music.mBurstEndTimeNs - app.mApplicationTimeNs;
        board.mPaused = true;
        app.mApplicationTimeNs += 2'000'000'000;
        music.MusicUpdate(app.mApplicationTimeNs);
        Require(music.mFadeOutEndTimeNs == 0, "Board pause stalled ordinary music fade");
        Require(music.mBurstEndTimeNs - app.mApplicationTimeNs == burstRemaining,
            "Board pause consumed burst controller time");
        board.mPaused = false;
        for (const auto tune : {MUSIC_TUNE_DAY_GRASSWALK, MUSIC_TUNE_POOL_WATERYGRAVES,
                MUSIC_TUNE_FOG_RIGORMORMIST, MUSIC_TUNE_ROOF_GRAZETHEROOF}) {
        currentOrder = MAKELONG(2, 16);
        music.mMusicBurstState = MUSIC_BURST_OFF;
        music.mMusicDrumsState = MUSIC_DRUMS_OFF;
        music.mCurMusicTune = tune;
        board.zombies = 10;
        step();
        // Skip directly over the one-second-remaining boundary. Queue once.
        app.mApplicationTimeNs = music.mBurstEndTimeNs - 999'999'999;
        music.UpdateMusicBurst(app.mApplicationTimeNs);
        Require(music.mMusicDrumsState == MUSIC_DRUMS_ON_QUEUED,
            "Day burst missed threshold under an uneven step");
        const auto queuedOrder = music.mQueuedDrumTrackPackedOrder;
        music.UpdateMusicBurst(app.mApplicationTimeNs);
        Require(music.mQueuedDrumTrackPackedOrder == queuedOrder,
            "Day burst repeatedly reset queued drum order");
        // Real decoders expose rows 0..63. The controller uses the original
        // quarter-row position contract (128 units = 32 decoder rows).
        currentOrder = MAKELONG(2, 31);
        music.mQueuedDrumTrackPackedOrder = music.GetMusicOrder(MUSIC_FILE_MAIN_MUSIC);
        music.UpdateMusicBurst(app.mApplicationTimeNs);
        Require(music.mMusicDrumsState == MUSIC_DRUMS_ON_QUEUED,
            "Day drums started before the half-pattern boundary");
        currentOrder = MAKELONG(2, 32);
        step();
        Require(music.mMusicDrumsState == MUSIC_DRUMS_ON && playback.volume[2] == 1,
            "Day drums never started when raw decoder rows crossed 31 to 32");
        for (unsigned i = 0; i < fps * 12; ++i) step();
        Require(music.mMusicBurstState == MUSIC_BURST_ON && playback.volume[2] == 1,
            "Day crowd lost its drum layer");
        board.zombies = 0;
        step();
        Require(music.mMusicDrumsState == MUSIC_DRUMS_OFF_QUEUED,
            "Day calm did not queue drum release");
        currentOrder = MAKELONG(3, 0);
        step();
        Require(music.mMusicDrumsState == MUSIC_DRUMS_FADING,
            "Day drums remained on across raw decoder row wrap");
        for (unsigned i = 0; i < fps + 1; ++i) step();
        Require(music.mMusicDrumsState == MUSIC_DRUMS_OFF && playback.volume[2] == 0,
            "Day drums failed to complete their release");
        for (unsigned i = 0; i < fps * 9; ++i) step();
        Require(music.mMusicBurstState == MUSIC_BURST_OFF && playback.volume[3] == 0,
            "Day hihats did not return to calm");
        board.zombies = 10;
        step();
        app.mApplicationTimeNs = music.mBurstEndTimeNs - 500'000'000;
        music.UpdateMusicBurst(app.mApplicationTimeNs);
        currentOrder = MAKELONG(3, 32);
        step();
        Require(music.mMusicDrumsState == MUSIC_DRUMS_ON,
            "Day drums could not retrigger after a completed crowd cycle");
        currentOrder = 0xffffffffu;
        Require(music.GetMusicOrder(MUSIC_FILE_MAIN_MUSIC) == 0xffffffffu,
            "Invalid decoder position must remain the invalid sentinel");
        std::cout << "PASS scheme-1 decoder-row burst entry/release/retrigger tune=" << tune << " at " << fps << " FPS\n";
        }

    }
}
'''
        source = source.replace("int main()\n{", "int main() try\n{")
        source += '\ncatch (const std::exception& error) { std::cerr << error.what() << "\\n"; return 1; }\n'
        with tempfile.TemporaryDirectory(prefix="pvz-music-burst-") as temp:
            source_path = Path(temp) / "music_burst.cpp"
            executable = Path(temp) / ("music_burst.exe" if os.name == "nt" else "music_burst")
            source_path.write_text(source, encoding="utf-8")
            includes = [ROOT / "pvzPerf", ROOT / "pvzPerf/SexyAppFramework"]
            if compiler:
                command = [compiler, "-std=c++20", *[f"-I{path}" for path in includes],
                           str(source_path), "-o", str(executable)]
            else:
                toolset = Path('C:/UserProgram/Visual Studuio/2026/Community/VC/Tools/MSVC/14.51.36231')
                build = (f'call "{vcvars}" >nul && set "INCLUDE={toolset}/include;!INCLUDE!" && set "LIB={toolset}/lib/x64;!LIB!" && "{toolset}/bin/Hostx64/x64/cl.exe" /nologo /EHsc /std:c++20 '
                         + " ".join(f'/I"{path}"' for path in includes)
                         + f' "{source_path}" /Fo"{temp}/music_burst.obj" /Fe"{executable}"')
                command = f'"{os.environ.get("COMSPEC", "cmd.exe")}" /d /v:on /s /c "{build}"'
            subprocess.run(command, cwd=temp, check=True)
            subprocess.run([str(executable)], cwd=temp, check=True)


if __name__ == "__main__":
    unittest.main()
