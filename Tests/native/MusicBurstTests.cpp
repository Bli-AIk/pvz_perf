// Controller fixture compiled with the production StartBurst/UpdateMusicBurst
// definitions by tools/native-tests/test_music_burst.py. Playback and the board
// are deterministic doubles; timer storage and all controller code are real.
#include "Lawn/System/Music.h"
#include "Sexy.TodLib/ReanimClock.h"
#include <algorithm>
#include <cassert>
#define TOD_ASSERT(value) assert(value)
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <type_traits>

static_assert(std::is_same_v<decltype(Music::mBaseBPM), float>);
static_assert(std::is_same_v<decltype(Music::mBaseModSpeed), float>);

enum class GameMode { GAMEMODE_ADVENTURE, GAMEMODE_INTRO };
class Board
{
public:
    int zombies = 10;
    bool mPaused = false;
    int CountZombiesOnScreen() const { return zombies; }
};
namespace Sexy { class MusicInterface; }
class LawnApp
{
public:
    Board* mBoard = nullptr;
    Sexy::MusicInterface* mMusicInterface = nullptr;
    std::int64_t mApplicationTimeNs = 0;
    GameMode mGameMode = GameMode::GAMEMODE_ADVENTURE;
};
static unsigned currentOrder = 48; // Decoder contract: low word order, high word raw row.
namespace Sexy
{
class MusicInterface
{
public:
    std::array<float, 6> volume{};
    void SetSongVolume(int file, float gain) { volume.at(file) = gain; }
};
struct TestPlayback
{
    int order = -1;
    int seeks = 0;
    void Seek(unsigned long long position, unsigned)
    {
        order = static_cast<int>(position);
        ++seeks;
    }
};
struct TestSlot { TestPlayback mPlayback; };
class BassMusicInterface : public MusicInterface
{
public:
    std::array<std::unique_ptr<TestSlot>, 6> mMusicSlots;
    int GetMusicOrder(int) {
        // Native BASS has MUSIC_PSCALER=4 set by SetupMusicFileForTune.
        return currentOrder == 0xffffffffu ? -1 :
            int(MAKELONG(LOWORD(currentOrder), HIWORD(currentOrder) * 4));
    }
    BassMusicInterface()
    {
        for (auto& slot : mMusicSlots) slot = std::make_unique<TestSlot>();
    }
};
}
namespace Curves { inline constexpr int Linear = 0; }
float TodAnimateCurveNs(std::int64_t start, std::int64_t end, std::int64_t age,
    float first, float last, int)
{
    const auto progress = std::clamp(double(age - start) / double(end - start), 0.0, 1.0);
    return static_cast<float>(first + (double(last) - first) * progress);
}
Music::Music()
{
    mCurMusicTune = MUSIC_TUNE_NIGHT_MOONGRAINS;
    mCurMusicFileMain = MUSIC_FILE_MAIN_MUSIC;
    mCurMusicFileDrums = MUSIC_FILE_DRUMS;
    mCurMusicFileHihats = MUSIC_FILE_HIHATS;
    mBurstOverride = -1;
    mMusicBurstState = MUSIC_BURST_OFF;
    mMusicDrumsState = MUSIC_DRUMS_OFF;
    mBurstEndTimeNs = 0;
    mDrumsEndTimeNs = 0;
    mQueuedDrumTrackPackedOrder = -1;
    mMusicDisabled = false;
    mLastUpdateTimeNs = 0;
    mFadeOutEndTimeNs = 0;
    mFadeOutDurationNs = 0;
}

// The runner appends production controller definitions here.

void Music::MusicResync() {}
void Music::StopAllMusic() { mFadeOutEndTimeNs = 0; }
