#pragma once
// Included by the game-linked static-definition fixture. These checks execute
// real Plant/Zombie methods, real Board pools/buckets and real source animation.
#include "Lawn/Board.h"
#include "Resources.h"
#include "Lawn/Plant.h"
#include "Lawn/PlantLaunchClock.h"
#include "Lawn/System/PlayerInfo.h"
#include "Lawn/Projectile.h"
#include "Lawn/SeedPacket.h"
#include "Lawn/Coin.h"
#include "Lawn/LawnMower.h"
#include "Lawn/GridItem.h"
#include "Lawn/ZenGarden.h"
#include "Lawn/System/StinkyTimestamp.h"
#include "Lawn/CursorObject.h"
#include "Lawn/MessageWidget.h"
#include "Lawn/System/Music.h"
#include "Lawn/Challenge.h"
#include "Lawn/Zombie.h"
#include "Lawn/Motion.h"
#include "Lawn/MagnetMotion.h"
#include "Sexy.TodLib/Attachment.h"
#include "Sexy.TodLib/TodParticle.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/generated/SceneAtlasData.h"
#include "Lawn/SlotMachineMotion.h"
#include "Lawn/System/SaveRecordMapping.h"
#include "SexyAppFramework/misc/ResourceManager.h"
#include <array>
#include <bit>
#include <ctime>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>
#include "CoinFallRuntimeChecks.h"
#include "MowerMotionRuntimeChecks.h"
#include "ZombieAltitudeRuntimeChecks.h"
#include "CreditsScrollRuntimeChecks.h"
#include "ZamboniRuntimeChecks.h"


namespace MowerMotionRuntime
{
inline void SaveContinuation(LawnApp& app, Board& board)
{
    const auto scene = app.mGameScene;
    app.mGameScene = GameScenes::SCENE_PLAYING;
    for (const bool poolTransition : {false, true})
    {
        MowerFixture fixture(app, board, LawnMowerType::LAWNMOWER_POOL, 50);
        auto& mower = fixture.mower;
        if (poolTransition)
        {
            mower.mMowerHeight = MowerHeight::MOWER_HEIGHT_DOWN_TO_POOL;
            mower.mPosX = 100;
        }
        const auto update = [&] {
            if (poolTransition) mower.UpdatePool();
            else mower.Update();
        };
        Schedule(144, 73'456'789, update);
        const auto record = SaveRecordMapping::WriteSaveRecord(mower);
        const auto savedBoardTime=board.mLevelTimeNs;
        const auto savedVisualTime=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        const auto sample = [&] {
            Schedule(1000, 23'456'789, update);
            return std::array<std::uint64_t, 4>{
                Bits(mower.mPosX), Bits(mower.mAltitude),
                static_cast<std::uint64_t>(std::max<std::int64_t>(0,mower.mChompEndTimeNs-static_cast<std::int64_t>(board.mLevelTimeNs))),
                static_cast<std::uint64_t>(mower.mMowerHeight)};
        };
        const auto uninterrupted = sample();
        SaveRecordMapping::ReadSaveRecord(mower, record.data());
        mower.mApp = &app;
        mower.mBoard = &board;
        board.mLevelTimeNs=savedBoardTime;
        app.mEffectSystem->mReanimationHolder->mVisualTimeNs=savedVisualTime;
        SaveClockProjection::Bind(mower);
        Require(sample() == uninterrupted,
            "Mower flight/chomp/pool phase changed after actual native save/reload");
    }
    app.mGameScene = scene;
}
}

namespace ActorFloatRuntime
{
inline void Require(bool condition, const char* text)
{
    if (!condition) throw std::runtime_error(text);
}

template<class Step>
void Schedule(int frequency, std::int64_t duration, Step step)
{
    std::mt19937_64 random(0x60191);
    for (std::int64_t elapsed = 0, n = 1; elapsed < duration; ++n)
    {
        const auto end = frequency ? std::min(duration, n * 1'000'000'000 / frequency) :
            std::min(duration, elapsed + static_cast<std::int64_t>(random() % 71'000'003 + 1));
        Sexy::gUpdateIntervalNs = end - elapsed;
        Sexy::gUpdateTicks = static_cast<double>(end - elapsed) / 10'000'000;
        Sexy::gWholeLegacyTicks = 0;
        step(end - elapsed);
        elapsed = end;
    }
}

inline std::uint64_t Bits(float value) { return std::bit_cast<std::uint32_t>(value); }
inline std::uint64_t Bits(double value) { return std::bit_cast<std::uint64_t>(value); }

inline void PublishTime(LawnApp& app, Board& board, std::int64_t delta)
{
    board.mLevelTimeNs += delta;
    auto& clocks = *app.mEffectSystem->mReanimationHolder;
    clocks.mBattleTimeNs = ReanimClock::Add(clocks.mBattleTimeNs, delta);
    clocks.mVisualTimeNs = ReanimClock::Add(clocks.mVisualTimeNs, delta);
}

inline void StinkyGardenReentry(LawnApp& app, Board& board)
{
    PlayerInfo profile;
    ZenGarden garden;
    auto& clocks = *app.mEffectSystem->mReanimationHolder;
    struct RestoreStinkyContext {
        LawnApp& app;
        Board& board;
        PlayerInfo* profile;
        ZenGarden* garden;
        GameMode mode;
        GameScenes scene;
        BackgroundType background;
        std::uint64_t level;
        std::int64_t visual, battle;
        ~RestoreStinkyContext() {
            board.mGridItems.DataArrayFreeAll();
            app.mEffectSystem->EffectSystemFreeAll();
            app.mBoard = &board;
            app.mPlayerInfo = profile;
            app.mZenGarden = garden;
            app.mGameMode = mode;
            app.mGameScene = scene;
            board.mBackground = background;
            board.mLevelTimeNs = level;
            auto& clocks = *app.mEffectSystem->mReanimationHolder;
            clocks.mVisualTimeNs = visual;
            clocks.mBattleTimeNs = battle;
        }
    } restore{app, board, app.mPlayerInfo, app.mZenGarden, app.mGameMode,
        app.mGameScene, board.mBackground, board.mLevelTimeNs,
        clocks.mVisualTimeNs, clocks.mBattleTimeNs};
    app.mPlayerInfo = &profile;
    app.mZenGarden = &garden;
    app.mGameMode = GAMEMODE_LIMBO_ZEN_GARDEN;
    app.mGameScene = SCENE_PLAYING; // The Board selects Battle; Stinky must remain Visual.
    board.mBackground = BACKGROUND_GREENHOUSE;
    board.mLevelTimeNs = 0;
    clocks.mBattleTimeNs = 0;
    clocks.mVisualTimeNs = 123'456'789'000;
    garden.mApp = &app;
    garden.mBoard = &board;
    garden.mGardenType = GARDEN_MAIN;
    garden.RefreshTimeCache();
    profile.mHasSeenStinky = 1;
    profile.mHasWokenStinky = 1;
    profile.mPurchases[STORE_ITEM_STINKY_THE_SNAIL] =
        StinkyTimestamp::EncodeWakeTime(std::time(nullptr));
    profile.mStinkyPosX = 400;
    profile.mStinkyPosY = 300;
    garden.AddStinky();
    auto* stinky = garden.GetStinky();
    Require(stinky && stinky->mPosX == 400 && stinky->mPosY == 300,
        "Stinky failed to restore a valid saved garden position");
    auto* animation = app.ReanimationGet(stinky->mGridItemReanimID);
    Require((*app.mEffectSystem->mReanimationHolder).mTimeDomains[(*app.mEffectSystem->mReanimationHolder).mReanimations.DataArrayIndexOf(animation)] == ReanimTimeDomain::Visual &&
        animation->mLastUpdateTimeNs == clocks.mVisualTimeNs,
        "Stinky creation used the reset Board clock instead of nonzero visual time");
    stinky->mGoalX = 200;
    stinky->mGoalY = 300;
    stinky->mStateEndTimeNs = clocks.mVisualTimeNs + 60'000'000'000;
    const auto initialX = stinky->mPosX;
    Schedule(100, 500'000'000, [&](std::int64_t delta) {
        clocks.mVisualTimeNs += delta;
        // Execute the actual owner/EffectSystem/GridItem order with a reset level clock.
        garden.StinkyUpdate(stinky);
        Require(animation->mLastUpdateTimeNs == clocks.mVisualTimeNs,
            "Stinky owner did not prepare the supplied visual instant before root motion");
        app.mEffectSystem->UpdateAt(clocks.mVisualTimeNs, 0);
        stinky->Update();
        Require(animation->mLastUpdateTimeNs == clocks.mVisualTimeNs &&
            (*app.mEffectSystem->mReanimationHolder).mTimeDomains[(*app.mEffectSystem->mReanimationHolder).mReanimations.DataArrayIndexOf(animation)] == ReanimTimeDomain::Visual && !animation->mDead &&
            stinky->mPosX <= initialX && stinky->mPosX >= stinky->mGoalX,
            "GridItem rewound Stinky onto Board time or its source root escaped the goal segment");
    });
    Require(stinky->mPosX < initialX,
        "Stinky source horizontal root stopped advancing on its visual timeline");
    garden.LeaveGarden();
    const int savedX = profile.mStinkyPosX, savedY = profile.mStinkyPosY;
    Require(!garden.GetStinky(), "Leaving the garden retained a live Stinky runtime actor");
    board.mGridItems.DataArrayFreeAll();
    app.mEffectSystem->EffectSystemFreeAll();
    {
        Board reentered(&app); // A genuinely new Board resets level time, not visual time.
        reentered.mBackground = BACKGROUND_GREENHOUSE;
        reentered.mLevelTimeNs = 0;
        clocks.mBattleTimeNs = 0;
        clocks.mVisualTimeNs += 60'000'000'000;
        garden.mBoard = &reentered;
        garden.AddStinky();
        stinky = garden.GetStinky();
        animation = app.ReanimationGet(stinky->mGridItemReanimID);
        Require(stinky->mPosX == savedX && stinky->mPosY == savedY &&
            animation->mLastUpdateTimeNs == clocks.mVisualTimeNs &&
            (*app.mEffectSystem->mReanimationHolder).mTimeDomains[(*app.mEffectSystem->mReanimationHolder).mReanimations.DataArrayIndexOf(animation)] == ReanimTimeDomain::Visual &&
            stinky->mRootReanimID == ReanimationID::REANIMATIONID_NULL,
            "New-board garden re-entry lost saved coordinates or retained an old root binding");
        garden.StinkyUpdate(stinky);
        stinky->Update();
        Require(!animation->mDead && animation->mLastUpdateTimeNs == clocks.mVisualTimeNs &&
            stinky->mPosX == savedX && stinky->mPosY == savedY,
            "First re-entry update jumped the saved snail onto an old root/time origin");
        // Ordinary grid owners must still rebase from an intro's Visual domain
        // to their Board's Battle clock, without replaying their entry phase.
        GridItem ordinary;
        ordinary.mBoard = &reentered;
        ordinary.mGridItemType = GRIDITEM_LADDER;
        auto* ordinaryAnim = app.AddReanimation(0, 0, 0, REANIM_STINKY,
            clocks.mVisualTimeNs, ReanimTimeDomain::Visual);
        ordinaryAnim->PlayReanim(ReanimTrackId::anim_crawl, REANIM_LOOP, 0, 6.0f);
        ordinaryAnim->SeekRatio(0.25f);
        const auto ordinaryPhase = ordinaryAnim->SamplePlayback().phaseNs;
        ordinary.mGridItemReanimID = app.ReanimationGetID(ordinaryAnim);
        ordinary.Update();
        Require((*app.mEffectSystem->mReanimationHolder).mTimeDomains[(*app.mEffectSystem->mReanimationHolder).mReanimations.DataArrayIndexOf(ordinaryAnim)] == ReanimTimeDomain::Battle &&
            ordinaryAnim->mLastUpdateTimeNs == 0 &&
            ordinaryAnim->SamplePlayback().phaseNs == ordinaryPhase,
            "Ordinary grid item failed its Visual-intro to Battle owner clock transition");
        ordinary.GridItemDie();
        for (const auto invalid : {std::array<int,2>{-4000,300},
                std::array<int,2>{800,300}, std::array<int,2>{400,600}})
        {
            garden.LeaveGarden();
            reentered.mGridItems.DataArrayFreeAll();
            app.mEffectSystem->EffectSystemFreeAll();
            profile.mStinkyPosX = invalid[0];
            profile.mStinkyPosY = invalid[1];
            garden.AddStinky();
            stinky = garden.GetStinky();
            Require(stinky && stinky->mPosX > 0 && stinky->mPosX < BOARD_WIDTH &&
                stinky->mPosY > 0 && stinky->mPosY < BOARD_HEIGHT &&
                stinky->mGoalX == stinky->mPosX && stinky->mGoalY == stinky->mPosY &&
                profile.mStinkyPosX == stinky->mPosX && profile.mStinkyPosY == stinky->mPosY,
                "Impossible saved Stinky position did not recover through original default goals");
        }
        // Coin chasing may legitimately leave the raised platform: preserve that save.
        garden.LeaveGarden();
        reentered.mGridItems.DataArrayFreeAll();
        app.mEffectSystem->EffectSystemFreeAll();
        profile.mStinkyPosX = 10;
        profile.mStinkyPosY = 500;
        garden.AddStinky();
        stinky = garden.GetStinky();
        Require(stinky && stinky->mPosX == 10 && stinky->mPosY == 500,
            "Stinky load recovery clamped a legitimate on-canvas coin-chasing position");
        garden.LeaveGarden();
    }
    app.mBoard = &board;
    std::cout << "PASS Stinky: visual root ownership, new-Board re-entry, saved-position preservation and invalid-save recovery\n";
}

inline void TreeOfWisdomEntryPose(LawnApp& app, Board& board)
{
    PlayerInfo profile;
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct RestoreTreeContext {
        LawnApp& app;
        Board& board;
        PlayerInfo* profile;
        GameMode mode;
        GameScenes scene;
        bool mute;
        std::uint64_t level;
        std::int64_t visual, battle;
        ~RestoreTreeContext() {
            app.mEffectSystem->EffectSystemFreeAll();
            app.mPlayerInfo=profile;
            app.mGameMode=mode;
            app.mGameScene=scene;
            app.mMuteSoundsForCutscene=mute;
            board.mLevelTimeNs=level;
            auto& clocks=*app.mEffectSystem->mReanimationHolder;
            clocks.mVisualTimeNs=visual;
            clocks.mBattleTimeNs=battle;
        }
    } restore{app,board,app.mPlayerInfo,app.mGameMode,app.mGameScene,
        app.mMuteSoundsForCutscene,board.mLevelTimeNs,clocks.mVisualTimeNs,clocks.mBattleTimeNs};
    app.mPlayerInfo=&profile;
    app.mGameMode=GAMEMODE_TREE_OF_WISDOM;
    app.mMuteSoundsForCutscene=true;
    const int record=app.GetCurrentChallengeIndex();
    for (const int height:{0,1,12,50,100})
    {
        app.mEffectSystem->EffectSystemFreeAll();
        app.mGameScene=SCENE_LEVEL_INTRO;
        board.mLevelTimeNs=0;
        clocks.mBattleTimeNs=0;
        clocks.mVisualTimeNs=123'456'789'000;
        profile.mChallengeRecords[record]=height;
        profile.mPurchases[STORE_ITEM_TREE_FOOD]=0;
        Challenge challenge;
        challenge.mBoard=&board;
        challenge.TreeOfWisdomInit();
        auto* tree=app.ReanimationGet(challenge.mReanimChallenge);
        const auto entry=tree->SamplePlayback().phaseNs;
        Require(entry==ReanimClock::SignedMulDiv(990,tree->mPeriodNs,1000) &&
            tree->mLastUpdateTimeNs==clocks.mVisualTimeNs &&
            (*app.mEffectSystem->mReanimationHolder).mTimeDomains[(*app.mEffectSystem->mReanimationHolder).mReanimations.DataArrayIndexOf(tree)]==ReanimTimeDomain::Visual,
            "Tree entry replayed growth instead of selecting the original completed pose");
        app.mGameScene=SCENE_PLAYING;
        board.mLevelTimeNs=10'000'000;
        challenge.Update(10'000'000);
        Require((*app.mEffectSystem->mReanimationHolder).mTimeDomains[(*app.mEffectSystem->mReanimationHolder).mReanimations.DataArrayIndexOf(tree)]==ReanimTimeDomain::Battle &&
            tree->mLastUpdateTimeNs==static_cast<std::int64_t>(board.mLevelTimeNs) &&
            tree->SamplePlayback().phaseNs==entry &&
            profile.mChallengeRecords[record]==height &&
            challenge.mChallengeState==STATECHALLENGE_TREE_WAITING_TO_BABBLE,
            "Tree playing-clock transition rewound the entry pose or fabricated growth");
        challenge.TreeOfWisdomGrow();
        Require(profile.mChallengeRecords[record]==height+1 &&
            tree->SamplePlayback().phaseNs==0 && tree->mAnimRate==8.0f &&
            tree->mLoopType==REANIM_PLAY_ONCE_AND_HOLD &&
            challenge.mChallengeState==(height ? STATECHALLENGE_TREE_JUST_GREW : STATECHALLENGE_NORMAL),
            "Actual Tree food no longer starts its original growth animation");
        board.mLevelTimeNs+=1;
        challenge.Update(1);
        Require(tree->SamplePlayback().phaseNs==1,
            "Actual Tree growth did not advance from the supplied owner timestamp");
    }
    std::cout<<"PASS Tree of Wisdom: completed entry pose, visual-to-battle rebase and food-only growth at five heights\n";
}

inline void SharedClockConsumers(Board& board)
{
    const auto oldTime = board.mLevelTimeNs;
    const auto oldWave = board.mCurrentWave, oldWaves = board.mNumWaves;
    const auto oldWidth = board.mProgressMeterWidth;
    const auto oldHealth = board.mZombieHealthToNextWave;
    const auto oldCountdown = board.mZombieCountDown, oldStart = board.mZombieCountDownStart;
    const auto oldFlag = board.mFlagRaiseCounter;
    board.mCurrentWave = 2;
    board.mNumWaves = 10;
    board.mZombieHealthToNextWave = -1;
    board.mZombieCountDownStart = 100;
    board.mZombieCountDown = 0;
    board.mFlagRaiseCounter = 4;
    board.mProgressMeterWidth = 0;
    board.mLevelTimeNs = 49'999'999;
    auto expectedFlag = board.mFlagRaiseCounter;
    --expectedFlag; // TimeCounter decrements by the published nanosecond step.
    board.UpdateProgressMeter(1);
    Require(board.mProgressMeterWidth == 0, "Progress event fired before its nanosecond boundary");
    Require(board.mFlagRaiseCounter == expectedFlag, "Progress width early exit skipped flag animation countdown");
    board.mLevelTimeNs += 1;
    board.UpdateProgressMeter(1);
    Require(board.mProgressMeterWidth == 1, "Progress event missed an exact nanosecond boundary");
    board.mLevelTimeNs += 1;
    board.UpdateProgressMeter(1);
    board.UpdateProgressMeter(0);
    Require(board.mProgressMeterWidth == 1, "Progress event repeated within the same interval");
    board.mLevelTimeNs += 71'234'567;
    board.UpdateProgressMeter(71'234'567);
    Require(board.mProgressMeterWidth == 2, "Progress event missed a boundary crossed by a variable step");
    board.mLevelTimeNs = oldTime;
    board.mCurrentWave = oldWave;
    board.mNumWaves = oldWaves;
    board.mProgressMeterWidth = oldWidth;
    board.mZombieHealthToNextWave = oldHealth;
    board.mZombieCountDown = oldCountdown;
    board.mZombieCountDownStart = oldStart;
    board.mFlagRaiseCounter = oldFlag;

    GameObject object;
    object.mX = 12.25f;
    object.mY = -3.5f;
    Sexy::Graphics graphics;
    const auto x = graphics.mTransX, y = graphics.mTransY;
    Require(object.BeginDraw(&graphics) && graphics.mTransX == x + object.mX &&
        graphics.mTransY == y + object.mY, "GameObject drawing discarded fractional coordinates");
    object.EndDraw(&graphics, x, y);
    Require(graphics.mTransX == x && graphics.mTransY == y,
        "GameObject drawing did not restore its floating-point translation");
    // A fractional parent translation is lost by (parent + large offset) -
    // large offset. Later objects must still see the original float bits.
    graphics.mTransX = 0.1f;
    graphics.mTransY = -0.3f;
    const float parentX = graphics.mTransX, parentY = graphics.mTransY;
    for (int i = 0; i < 1024; ++i)
    {
        object.mX = i % 2 ? 8192.25f : -16384.5f;
        object.mY = i % 2 ? -32768.25f : 4096.125f;
        Require(object.BeginDraw(&graphics), "Visible object failed to begin drawing");
        object.EndDraw(&graphics, parentX, parentY);
        Require(graphics.mTransX == parentX && graphics.mTransY == parentY,
            "Repeated object drawing accumulated parent translation rounding");
    }
    std::cout << "PASS raw level-clock consumers: variable-step boundary events, zero-step stability, float drawing\n";
}

inline void LaunchDeadlines(LawnApp& app, Board& board)
{
    const auto previousMode = app.mGameMode;
    const auto previousScene = app.mGameScene;
    const auto previousProfile = app.mPlayerInfo;
    // Adventure target acquisition calls IsWallnutBowlingLevel, which reads
    // the active profile's level. The definition-only fixture has no profile.
    PlayerInfo profile;
    profile.mLevel = 1;
    app.mPlayerInfo = &profile;
    struct RestoreLaunchContext
    {
        LawnApp& app;
        GameMode mode;
        GameScenes scene;
        PlayerInfo* profile;
        ~RestoreLaunchContext()
        {
            app.mGameMode = mode;
            app.mGameScene = scene;
            app.mPlayerInfo = profile;
        }
    } restoreContext{app, previousMode, previousScene, previousProfile};
    app.mGameMode = GameMode::GAMEMODE_ADVENTURE;
    app.mGameScene = GameScenes::SCENE_PLAYING;
    board.mZombies.DataArrayFreeAll();
    board.RebuildZombieRowBucket();
    const auto run = [&](int frequency) {
        Sexy::SRand(0x601982);
        board.mLevelTimeNs = 0;
        Plant plant;
        plant.mSeedType = SeedType::SEED_PEASHOOTER;
        plant.mRow = 2;
        plant.mShootingCounter = 0;
        plant.mLaunchRate = 150;
        plant.mNextLaunchTimeNs = 123'456'789;
        plant.mLaunchWindowStartNs = 0;
        Schedule(frequency, 12'718'281'829, [&](std::int64_t delta) {
            board.mLevelTimeNs += delta;
            plant.UpdateShooter(static_cast<std::int64_t>(board.mLevelTimeNs), app.mEffectSystem->mReanimationHolder->mVisualTimeNs, (board.AnimationTimeDomain()==ReanimTimeDomain::Battle ? static_cast<std::int64_t>(board.mLevelTimeNs) : app.mEffectSystem->mReanimationHolder->mVisualTimeNs));
        });
        Require(plant.mNextLaunchTimeNs > board.mLevelTimeNs,
            "Production shooter did not consume elapsed firing periods");
        Require(plant.mNextLaunchTimeNs % PlantLaunchClock::Tick != 0,
            "Production shooter randomized only on legacy tick boundaries");
        return plant.mNextLaunchTimeNs;
    };
    const auto expected = run(100);
    for (int frequency : {30, 60, 144, 200, 240, 1000, 0})
        Require(run(frequency) == expected,
            "Production shooter deadline depends on outer update partition");

    Plant paused;
    paused.mSeedType = SeedType::SEED_PEASHOOTER;
    paused.mState = PlantState::STATE_READY;
    paused.mIsOnBoard = true;
    paused.mIsAsleep = true;
    paused.mSquished = true;
    paused.mDisappearEndTimeNs = 11'237'192'835;
    paused.mWakeUpEndTimeNs = 0;
    paused.mPlantHealth = 300;
    paused.mRecentlyEatenEndTimeNs = 0;
    paused.mEatenFlashEndTimeNs = 0;
    paused.mBeghouledFlashEndTimeNs = 0;
    paused.mBodyReanimID = ReanimationID::REANIMATIONID_NULL;
    paused.mLaunchWindowStartNs = 1'000'000'000;
    paused.mNextLaunchTimeNs = 1'712'345'678;
    board.mLevelTimeNs = 1'237'192'835;
    paused.Update(static_cast<std::int64_t>(board.mLevelTimeNs), app.mEffectSystem->mReanimationHolder->mVisualTimeNs, (board.AnimationTimeDomain()==ReanimTimeDomain::Battle ? static_cast<std::int64_t>(board.mLevelTimeNs) : app.mEffectSystem->mReanimationHolder->mVisualTimeNs), 237'192'835);
    Require(paused.mNextLaunchTimeNs == 1'949'538'513 &&
        paused.mLaunchWindowStartNs == board.mLevelTimeNs,
        "Production paused plant did not preserve its exact remaining launch time");
    const auto frozenDeadline = paused.mNextLaunchTimeNs;
    paused.Update(static_cast<std::int64_t>(board.mLevelTimeNs), app.mEffectSystem->mReanimationHolder->mVisualTimeNs, (board.AnimationTimeDomain()==ReanimTimeDomain::Battle ? static_cast<std::int64_t>(board.mLevelTimeNs) : app.mEffectSystem->mReanimationHolder->mVisualTimeNs), 0);
    Require(paused.mNextLaunchTimeNs == frozenDeadline,
        "Plant shifted a deadline while the level clock was stopped");
    paused.Update(static_cast<std::int64_t>(board.mLevelTimeNs), app.mEffectSystem->mReanimationHolder->mVisualTimeNs, (board.AnimationTimeDomain()==ReanimTimeDomain::Battle ? static_cast<std::int64_t>(board.mLevelTimeNs) : app.mEffectSystem->mReanimationHolder->mVisualTimeNs), 237'192'835);
    Require(paused.mNextLaunchTimeNs == frozenDeadline,
        "Plant newly anchored at this step's endpoint was frozen for time before its creation");
    std::cout << "PASS actual Plant launch deadlines: ns randomness, exact partitions, "
        "paused remaining time\n";

    // The 1073 shooter starts on its launch deadline even while the previous
    // shooting timer is positive. Gloom's 200-tick timer deliberately overlaps
    // its next 186..200-tick period; waiting for zero drops every other volley.
    const bool oldMute = app.mMuteSoundsForCutscene;
    app.mMuteSoundsForCutscene = true;
    struct RestoreMute
    {
        LawnApp& app;
        bool mute;
        ~RestoreMute() { app.mMuteSoundsForCutscene = mute; }
    } restoreMute{app, oldMute};
    auto* target = board.mZombies.DataArrayAlloc();
    target->mZombieType = ZombieType::ZOMBIE_NORMAL;
    target->mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
    target->mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
    target->mScaleZombie = 1;
    target->mRow = 2;
    target->mX = 340;
    target->mY = static_cast<float>(board.GridToPixelY(0, 2));
    target->mZombieRect = Rect(0, 0, 40, 100);
    target->mWidth = 40;
    target->mHeight = 100;
    target->mBodyHealth = target->mBodyMaxHealth = 10'000'000;
    target->mHasHead = true;
    board.RebuildZombieRowBucket();
    for (const auto seed : {SeedType::SEED_GLOOMSHROOM, SeedType::SEED_GATLINGPEA})
    for (const int frequency : {100, 30, 144, 200, 1000, 0})
    {
        app.mEffectSystem->EffectSystemFreeAll();
        board.mLevelTimeNs = 0;
        Sexy::SRand(0x601982);
        Plant gloom;
        gloom.PlantInitialize(static_cast<std::int64_t>(board.mLevelTimeNs), app.mEffectSystem->mReanimationHolder->mVisualTimeNs, (board.AnimationTimeDomain()==ReanimTimeDomain::Battle ? static_cast<std::int64_t>(board.mLevelTimeNs) : app.mEffectSystem->mReanimationHolder->mVisualTimeNs), 3, 2, seed, SeedType::SEED_NONE);
        gloom.mIsAsleep = false;
        gloom.mNextLaunchTimeNs = 100'000'000;
        gloom.mLaunchWindowStartNs = 0;
        Require(gloom.FindTargetZombie(2, PlantWeapon::WEAPON_PRIMARY) == target,
            "Gloom cadence fixture did not acquire its real target");
        const bool isGloom = seed == SeedType::SEED_GLOOMSHROOM;
        const auto pulseTicks = isGloom ? std::array{74, 102, 130, 158} :
            std::array{32, 49, 65, 82};
        std::int64_t volleyStart = 0;
        int volleys = 0, hits = 0, completedVolleys = 0;
        Schedule(frequency, 12'000'000'000, [&](std::int64_t delta) {
            board.mLevelTimeNs += delta;
            const int health = target->mBodyHealth;
            const auto projectiles = board.mProjectiles.mSize;
            gloom.UpdateShooting(static_cast<std::int64_t>(board.mLevelTimeNs), app.mEffectSystem->mReanimationHolder->mVisualTimeNs, (board.AnimationTimeDomain()==ReanimTimeDomain::Battle ? static_cast<std::int64_t>(board.mLevelTimeNs) : app.mEffectSystem->mReanimationHolder->mVisualTimeNs));
            if (isGloom ? target->mBodyHealth != health : board.mProjectiles.mSize != projectiles)
            {
                Require(isGloom ? target->mBodyHealth == health - 20 :
                    board.mProjectiles.mSize == projectiles + 1,
                    "Volley pulse changed its original damage or projectile count");
                const auto age = board.mLevelTimeNs - volleyStart;
                Require(hits < 4 && age >= pulseTicks[hits] * PlantLaunchClock::Tick &&
                    age - delta < pulseTicks[hits] * PlantLaunchClock::Tick,
                    "Gloom/Gatling pulse did not cross its original firing instant");
                ++hits;
            }
            const auto due = gloom.mNextLaunchTimeNs;
            gloom.UpdateShooter(static_cast<std::int64_t>(board.mLevelTimeNs), app.mEffectSystem->mReanimationHolder->mVisualTimeNs, (board.AnimationTimeDomain()==ReanimTimeDomain::Battle ? static_cast<std::int64_t>(board.mLevelTimeNs) : app.mEffectSystem->mReanimationHolder->mVisualTimeNs));
            if (due <= board.mLevelTimeNs)
            {
                if (volleys)
                {
                    Require(hits == 4, "Gloom/Gatling volley lost one of its four pulses");
                    ++completedVolleys;
                }
                Require(gloom.mShootingCounter.mNanoseconds ==
                    (isGloom ? 200 : 100) * PlantLaunchClock::Tick,
                    "Gloom/Gatling skipped a launch deadline while its previous shooting timer was positive");
                const auto period = gloom.mNextLaunchTimeNs - due;
                const auto rate = isGloom ? 200 : 150;
                Require(period >= (rate - 14) * PlantLaunchClock::Tick &&
                    period <= rate * PlantLaunchClock::Tick,
                    "Gloom/Gatling changed its original launch period");
                ++volleys;
                hits = 0;
                volleyStart = board.mLevelTimeNs;
            }
        });
        Require((isGloom ? volleys == 7 : volleys >= 8) && completedVolleys == volleys - 1,
            "Gloom/Gatling firing cadence dropped below its original rate");
        board.mProjectiles.DataArrayFreeAll();
    }
    for (const auto seed : {SeedType::SEED_REPEATER, SeedType::SEED_CATTAIL})
    {
        app.mEffectSystem->EffectSystemFreeAll();
        board.mLevelTimeNs = 0;
        Plant plant;
        plant.PlantInitialize(static_cast<std::int64_t>(board.mLevelTimeNs), app.mEffectSystem->mReanimationHolder->mVisualTimeNs, (board.AnimationTimeDomain()==ReanimTimeDomain::Battle ? static_cast<std::int64_t>(board.mLevelTimeNs) : app.mEffectSystem->mReanimationHolder->mVisualTimeNs), 3, 2, seed, SeedType::SEED_NONE);
        plant.mNextLaunchTimeNs = 1'000'000'000;
        plant.mLaunchWindowStartNs = 0;
        plant.mShootingCounter = 10;
        board.mLevelTimeNs = seed == SeedType::SEED_CATTAIL ? 500'000'000 : 750'000'000;
        plant.UpdateShooter(static_cast<std::int64_t>(board.mLevelTimeNs), app.mEffectSystem->mReanimationHolder->mVisualTimeNs, (board.AnimationTimeDomain()==ReanimTimeDomain::Battle ? static_cast<std::int64_t>(board.mLevelTimeNs) : app.mEffectSystem->mReanimationHolder->mVisualTimeNs));
        Require(plant.mShootingCounter.mNanoseconds ==
            (seed == SeedType::SEED_CATTAIL ? 50 : 26) * PlantLaunchClock::Tick,
            "Secondary launch was suppressed by an unfinished shooting timer");
    }
    for (const auto seed : {SeedType::SEED_REPEATER, SeedType::SEED_LEFTPEATER})
    for (const int frequency : {100, 144, 240, 1000, 30, 0})
    {
        app.mEffectSystem->EffectSystemFreeAll();
        board.mProjectiles.DataArrayFreeAll();
        board.mLevelTimeNs = 0;
        Sexy::SRand(0x601982);
        Plant plant;
        plant.PlantInitialize(0, 0, 0, 3, 2, seed, SeedType::SEED_NONE);
        target->mX = seed == SeedType::SEED_LEFTPEATER ? 150 : 340;
        plant.mNextLaunchTimeNs = 1'013'456'789;
        plant.mLaunchWindowStartNs = 0;
        Require(plant.FindTargetZombie(2, PlantWeapon::WEAPON_PRIMARY) == target,
            "Repeater fixture did not acquire its real target");
        int attacks = 0, shots = 0;
        Schedule(frequency, 8'000'000'000, [&](std::int64_t delta) {
            board.mLevelTimeNs += delta;
            plant.UpdateShooting(board.mLevelTimeNs, board.mLevelTimeNs, board.mLevelTimeNs);
            const auto due = plant.mNextLaunchTimeNs;
            plant.UpdateShooter(board.mLevelTimeNs, board.mLevelTimeNs, board.mLevelTimeNs);
            if (due <= board.mLevelTimeNs)
            {
                Require(board.mProjectiles.mSize == 1,
                    "Repeater lost its first projectile when the second shot restarted its timer");
                Require(shots == attacks * 2,
                    "Repeater lost or duplicated a projectile in its preceding attack pair");
                ++attacks;
            }
            if (board.mProjectiles.mSize)
            {
                Require(board.mProjectiles.mSize == 1,
                    "Repeater emitted duplicate projectiles in one firing interval");
                ++shots;
                board.mProjectiles.DataArrayFreeAll();
            }
        });
        Require(attacks >= 5 && shots >= attacks * 2 - 1,
            "Repeater fixture did not cover complete attack pairs");
    }
    target->mX = 340;
    app.mEffectSystem->EffectSystemFreeAll();
    board.mZombies.DataArrayFreeAll();
    board.RebuildZombieRowBucket();
    std::cout << "PASS actual Gloom/Gatling cadence: original deadlines and four pulse instants, "
        "Gloom seven volleys in 12s, at 30/100/144/200/1000Hz and irregular steps; "
        "Repeater/Cattail secondary deadlines preserve 1073 overlap\n";
}

inline std::array<std::uint64_t, 8> Bowling(LawnApp& app, Board& board,
    int frequency, SeedType seed)
{
    const auto previousGround = board.mGridSquareType[0][1];
    struct RestoreBowlingGround
    {
        Board& board;
        GridSquareType type;
        ~RestoreBowlingGround() { board.mGridSquareType[0][1] = type; }
    } restoreGround{board, previousGround};
    board.mGridSquareType[0][1] = GridSquareType::GRIDSQUARE_HIGH_GROUND;
    board.mPlants.DataArrayFreeAll();
    board.RebuildPlantCellBucket();
    app.mEffectSystem->EffectSystemFreeAll();
    board.mLevelTimeNs = 0;
    app.mEffectSystem->mReanimationHolder->mBattleTimeNs = 0;
    app.mEffectSystem->mReanimationHolder->mVisualTimeNs = 0;
    auto* plant = board.mPlants.DataArrayAlloc();
    plant->mSeedType = seed;
    plant->mState = PlantState::STATE_BOWLING_UP;
    plant->mRow = 2;
    plant->mPlantCol = 0;
    plant->mX = -500.123413f;
    plant->mY = static_cast<float>(board.GridToPixelY(0, 2));
    plant->mWidth = plant->mHeight = 80;
    plant->mDead = false;
    plant->mIsOnBoard = false;
    board.RebuildPlantCellBucket();
    auto* animation = app.AddReanimation(0, 0, 0, ReanimationType::REANIM_WALLNUT, app.mEffectSystem->mReanimationHolder->mBattleTimeNs, ReanimTimeDomain::Battle);
    (*app.mEffectSystem->mReanimationHolder).BindAnimationTime(*animation,ReanimTimeDomain::Battle, app.mEffectSystem->mReanimationHolder->mBattleTimeNs);
    animation->SetFramesForLayer(ReanimTrackId::_ground);
    animation->ApplyRate(15.3072289f);
    animation->SeekRatio(0.3372819f);
    plant->mBodyReanimID = app.ReanimationGetID(animation);
    const auto scale = seed == SeedType::SEED_GIANT_WALLNUT ? -2.0f : -1.0f;
    const auto initialRoot = animation->RootMotion(ReanimTrackId::_ground, scale);
    constexpr std::int64_t duration = 1'234'567'891;
    const auto expectedX = CheckedPosition((double(plant->mX) - initialRoot) +
        animation->RootMotion(ReanimTrackId::_ground, scale, duration));
    AdvanceRoot(*plant, *animation, scale, 0);
    Schedule(frequency, duration, [&](std::int64_t delta) {
        PublishTime(app, board, delta);
        animation->PrepareTimeAt(static_cast<std::int64_t>(board.mLevelTimeNs));
        plant->UpdateBowling(static_cast<std::int64_t>(board.mLevelTimeNs), app.mEffectSystem->mReanimationHolder->mVisualTimeNs, (board.AnimationTimeDomain()==ReanimTimeDomain::Battle ? static_cast<std::int64_t>(board.mLevelTimeNs) : app.mEffectSystem->mReanimationHolder->mVisualTimeNs));
        animation->UpdateAt(app.mEffectSystem->mReanimationHolder->mBattleTimeNs);
        if (app.mEffectSystem->mReanimationHolder->mBattleTimeNs < 10'000'000)
            Require(plant->mY != static_cast<float>(board.GridToPixelY(0, 2)),
                "Production bowling waits for a fixed 10ms vertical substep");
    });
    Require(!plant->mDead && plant->mX == expectedX,
        "Production bowling position differs from anchored root interpolation");
    Require(plant->mRow != 2 && plant->mBowlingRowArrivalTimeNs > duration &&
        plant->mY == CheckedPosition(double(board.GridToPixelY(0, plant->mRow)) +
            (plant->mState == PlantState::STATE_BOWLING_UP ? 1.0 : -1.0) *
            (plant->mBowlingRowArrivalTimeNs - duration) / 5'000'000.0),
        "Production bowling did not interpolate its absolute row-arrival deadline");
    Require(board.GetCellPlant(plant->mRow, plant->mPlantCol, PLANT_CELL_NORMAL) == plant,
        "Production bowling row transition did not update the real plant bucket");
    return {Bits(plant->mX), Bits(plant->mY), std::uint64_t(plant->mRow), std::uint64_t(plant->mState),
        std::uint64_t(plant->mBowlingRowArrivalTimeNs), Bits(plant->mRootOriginX),
        std::uint64_t(animation->SamplePlayback().phaseNs),
        std::uint64_t(animation->SamplePlayback().cycles)};
}

inline void GiantBowlingImpact(LawnApp& app, Board& board)
{
    // The game-linked fixture has renderer/resources but no audio device.
    struct RestoreBowlingMute
    {
        LawnApp& app;
        bool previous;
        ~RestoreBowlingMute() { app.mMuteSoundsForCutscene = previous; }
    } restoreMute{app, app.mMuteSoundsForCutscene};
    app.mMuteSoundsForCutscene = true;
    board.mPlants.DataArrayFreeAll();
    board.RebuildPlantCellBucket();
    board.mZombies.DataArrayFreeAll();
    board.RebuildZombieRowBucket();
    app.mEffectSystem->EffectSystemFreeAll();
    auto* plant = board.mPlants.DataArrayAlloc();
    plant->mApp = &app;
    plant->mBoard = &board;
    plant->mSeedType = SeedType::SEED_GIANT_WALLNUT;
    plant->mState = PlantState::STATE_NOTREADY;
    plant->mRow = 2;
    plant->mPlantCol = 0;
    plant->mX = 300;
    plant->mY = static_cast<float>(board.GridToPixelY(0, 2));
    plant->mWidth = plant->mHeight = 80;
    plant->mIsOnBoard = false;
    board.RebuildPlantCellBucket();
    auto* zombie = board.mZombies.DataArrayAlloc();
    zombie->mApp = &app;
    zombie->mBoard = &board;
    zombie->mZombieType = ZombieType::ZOMBIE_NORMAL;
    zombie->mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
    zombie->mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
    zombie->mScaleZombie = 1;
    zombie->mRow = 2;
    zombie->mX = 320;
    zombie->mY = plant->mY;
    zombie->mZombieRect = Rect(0, 0, 40, 100);
    zombie->mWidth = 40;
    zombie->mHeight = 100;
    zombie->mBodyHealth = zombie->mBodyMaxHealth = 10'000'000;
    zombie->mHasHead = true;
    board.RebuildZombieRowBucket();
    Require(plant->FindTargetZombie(2, PlantWeapon::WEAPON_PRIMARY) == zombie,
        "Production giant bowling fixture did not acquire its real collision target");
    for (const std::int64_t delta : {1LL, 1'234'567LL, 900'000'001LL})
    {
        Sexy::gUpdateIntervalNs = delta;
        PublishTime(app, board, delta);
        const int before = zombie->mBodyHealth;
        plant->UpdateBowling(static_cast<std::int64_t>(board.mLevelTimeNs), app.mEffectSystem->mReanimationHolder->mVisualTimeNs, (board.AnimationTimeDomain()==ReanimTimeDomain::Battle ? static_cast<std::int64_t>(board.mLevelTimeNs) : app.mEffectSystem->mReanimationHolder->mVisualTimeNs));
        Require(zombie->mBodyHealth == before - 1'000'000 &&
            plant->mState == PlantState::STATE_NOTREADY && plant->mBowlingRowArrivalTimeNs == 0,
            "Production giant bowling damage depends on 100Hz substeps or a hit cooldown");
    }
    board.mPlants.DataArrayFreeAll();
    board.RebuildPlantCellBucket();
    board.mZombies.DataArrayFreeAll();
    board.RebuildZombieRowBucket();
    std::cout << "PASS actual giant bowling collision: one million damage per outer step, "
        "including a one-nanosecond step, without a cooldown\n";
}

inline std::array<std::uint64_t, 6> Walking(LawnApp& app, Board& board, int frequency,
    bool additionalMotion = false)
{
    app.mEffectSystem->EffectSystemFreeAll();
    board.mLevelTimeNs = 0;
    app.mEffectSystem->mReanimationHolder->mBattleTimeNs = 0;
    app.mEffectSystem->mReanimationHolder->mVisualTimeNs = 0;
    Zombie zombie{};
    zombie.mZombieType = ZombieType::ZOMBIE_NORMAL;
    zombie.mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
    zombie.mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
    zombie.mScaleZombie = 1.3372819f;
    zombie.mX = 712.349243f;
    zombie.mY = 200.0f;
    zombie.mFromWave = 0;
    zombie.mBlowingAway = additionalMotion;
    auto* animation = app.AddReanimation(0, 0, 0, ReanimationType::REANIM_ZOMBIE, static_cast<std::int64_t>(board.mLevelTimeNs), ReanimTimeDomain::Battle);
    (*app.mEffectSystem->mReanimationHolder).BindAnimationTime(*animation,ReanimTimeDomain::Battle, static_cast<std::int64_t>(board.mLevelTimeNs));
    animation->PlayReanim(ReanimTrackId::anim_walk, ReanimLoopType::REANIM_LOOP, 0, 13.3072289f);
    animation->SeekRatio(0.3372819f);
    zombie.mBodyReanimID = app.ReanimationGetID(animation);
    const auto initialRoot = animation->RootMotion(ReanimTrackId::_ground, -zombie.mScaleZombie);
    constexpr std::int64_t duration = 2'718'281'829;
    const auto expectedX = CheckedPosition((double(zombie.mX) - initialRoot) +
        animation->RootMotion(ReanimTrackId::_ground, -zombie.mScaleZombie, duration) +
        (additionalMotion ? 9.5 * duration / 10'000'000.0 : 0.0));
    zombie.mX = CheckedPosition(zombie.RootPosition(*animation, -zombie.mScaleZombie));
    Schedule(frequency, duration, [&](std::int64_t delta) {
        PublishTime(app, board, delta);
        if (additionalMotion) zombie.mPendingAdditionalSpeedX = -0.5f;
        animation->PrepareTimeAt(static_cast<std::int64_t>(board.mLevelTimeNs));
        zombie.UpdateZombieWalking(static_cast<std::int64_t>(zombie.mBoard->mLevelTimeNs));
        animation->UpdateAt(static_cast<std::int64_t>(board.mLevelTimeNs));
    });
    Require(zombie.mX == expectedX,
        "Production zombie walking differs from anchored root interpolation");
    // A direct teleport invalidates its binding at the write event.
    zombie.mX += 91.123413f;
    zombie.mRootBindingGeneration = 0;
    const auto teleported = zombie.mX;
    Sexy::gUpdateIntervalNs = 0;
    animation->PrepareTimeAt(static_cast<std::int64_t>(board.mLevelTimeNs));
    zombie.UpdateZombieWalking(static_cast<std::int64_t>(zombie.mBoard->mLevelTimeNs));
    Require(zombie.mX == teleported, "Root binding erased an explicit teleport");
    return {Bits(zombie.mX), Bits(zombie.mY), Bits(zombie.mRootOriginX),
        Bits(zombie.mRootScale), std::uint64_t(animation->SamplePlayback().phaseNs),
        std::uint64_t(animation->SamplePlayback().cycles)};
}

inline void VaultAndRoofImpSegments(LawnApp& app, Board& board)
{
    const auto oldBackground = board.mBackground;
    for (const int frequency : {100, 240, 480})
    {
        for (const bool priorWalking : {false, true})
        {
            app.mEffectSystem->EffectSystemFreeAll();
            Zombie zombie{};
            ZombieAltitudeRuntime::Initialize(zombie, app, board, ZombieAltitudeRuntime::Path::Imp);
            zombie.mFromWave = 0;
            zombie.mZombieType = ZombieType::ZOMBIE_POLEVAULTER;
            zombie.mZombiePhase = ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT;
            zombie.mX = 279.5f;
            zombie.mVelX = .67f;
            auto* animation = app.AddReanimation(0, 0, 0, ReanimationType::REANIM_POLEVAULTER, static_cast<std::int64_t>(board.mLevelTimeNs), ReanimTimeDomain::Battle);
            (*app.mEffectSystem->mReanimationHolder).BindAnimationTime(*animation,ReanimTimeDomain::Battle, static_cast<std::int64_t>(board.mLevelTimeNs));
            animation->PlayReanim(ReanimTrackId::anim_run, ReanimLoopType::REANIM_LOOP, 0, 12);
            zombie.mBodyReanimID = app.ReanimationGetID(animation);
            if (priorWalking)
            {
                Schedule(frequency, 270'000'000, [&](std::int64_t delta) {
                    PublishTime(app, board, delta);
                    animation->PrepareTimeAt(static_cast<std::int64_t>(board.mLevelTimeNs));
                    zombie.UpdateZombieWalking(static_cast<std::int64_t>(zombie.mBoard->mLevelTimeNs));
                    animation->UpdateAt(static_cast<std::int64_t>(board.mLevelTimeNs));
                });
                Require(zombie.mRootBindingGeneration && zombie.mVelocityX.bound &&
                    zombie.mVelocityX.rate == 0 && static_cast<std::int64_t>(board.mLevelTimeNs) > zombie.mVelocityX.startTimeNs,
                    "Vault regression did not establish a prior root/offset segment");
            }
            const float launchX = zombie.mX;
            // Ordinary contact can yield int(zombieX - plantX - 80) == 0.
            // This is the real root-to-linear handoff on the entry update.
            zombie.mZombiePhase = ZombiePhase::PHASE_POLEVAULTER_IN_VAULT;
            zombie.mVelX = 0;
            animation->PlayReanim(ReanimTrackId::anim_jump,
                ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24);
            Sexy::gUpdateIntervalNs = 1'000'000'000 / frequency;
            animation->PrepareTimeAt(static_cast<std::int64_t>(board.mLevelTimeNs));
            zombie.UpdateZombieWalking(static_cast<std::int64_t>(zombie.mBoard->mLevelTimeNs));
            Require(zombie.mX == launchX, "Zero-travel vault reused a root offset as world X");
            Schedule(frequency, 250'000'000, [&](std::int64_t delta) {
                PublishTime(app, board, delta);
                animation->PrepareTimeAt(static_cast<std::int64_t>(board.mLevelTimeNs));
                zombie.UpdateZombiePolevaulter(static_cast<std::int64_t>(zombie.mBoard->mLevelTimeNs));
                animation->PrepareTimeAt(static_cast<std::int64_t>(board.mLevelTimeNs));
                zombie.UpdateZombieWalking(static_cast<std::int64_t>(zombie.mBoard->mLevelTimeNs));
                animation->UpdateAt(static_cast<std::int64_t>(board.mLevelTimeNs));
                Require(zombie.mX == launchX, "Zero-travel vault lost its world-position segment");
            });
            animation->mLoopCount = 1;
            animation->PrepareTimeAt(static_cast<std::int64_t>(board.mLevelTimeNs));
            zombie.UpdateZombiePolevaulter(static_cast<std::int64_t>(zombie.mBoard->mLevelTimeNs));
            Require(zombie.mX == launchX - 150 && zombie.mX > -150,
                "Vault landing used the offset origin and crossed the house edge");
        }

        board.mBackground = BackgroundType::BACKGROUND_5_ROOF;
        Zombie imp{};
        ZombieAltitudeRuntime::Initialize(imp, app, board, ZombieAltitudeRuntime::Path::Imp);
        imp.mFromWave = 0;
        imp.mX = 667;
        imp.mY = imp.GetPosYBasedOnRow(imp.mRow);
        imp.mAltitude = 88;
        imp.mVelX = 3;
        imp.mVelZ = .5f * (440.0f / 3) * THOWN_ZOMBIE_GRAVITY;
        const double initialY = imp.mY, initialVelocity = imp.mVelZ;
        double previousAltitude = imp.mAltitude;
        std::int64_t elapsed = 0;
        Schedule(frequency, 1'800'000'000, [&](std::int64_t delta) {
            elapsed += delta;
            board.mLevelTimeNs += delta;
            // Both production calls are required: the previous isolated
            // altitude checks could not expose the stale row-approach origin.
            imp.UpdateZombieImp(static_cast<std::int64_t>(imp.mBoard->mLevelTimeNs));
            imp.UpdateZombiePosition(static_cast<std::int64_t>(imp.mBoard->mLevelTimeNs), imp.mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (imp.mBoard->AnimationTimeDomain()==ReanimTimeDomain::Battle ? static_cast<std::int64_t>(imp.mBoard->mLevelTimeNs) : imp.mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
            const double ticks = double(elapsed) / 10'000'000;
            const double rowY = imp.GetPosYBasedOnRow(imp.mRow);
            const auto expectedAltitude = CheckedPosition(88 - initialY + rowY +
                initialVelocity * ticks - double(THOWN_ZOMBIE_GRAVITY) * ticks * (ticks + 1) * .5);
            Require(imp.mY == rowY && std::abs(imp.mAltitude - expectedAltitude) < .001f,
                "Roof imp row placement was overwritten by a stale Y segment");
            if (imp.mVelZ < -.8f)
                Require(imp.mAltitude <= previousAltitude,
                    "Descending roof imp rebounded before landing");
            previousAltitude = imp.mAltitude;
        });
        Require(imp.mX > 100 && imp.mAltitude > 0 && imp.mAltitude < 5,
            "Roof imp flight did not approach the expected first landing");
        board.mBackground = oldBackground;
    }
    std::cerr << "PASS fresh/moving zero-travel vault and full roof Imp position updates at 100/240/480Hz\n";
}

inline void Projectiles(LawnApp& app, Board& board)
{
    // Use the production mute switch: impact and chill still execute their real
    // gameplay paths without requiring the fixture to initialize an audio device.
    struct RestoreMute
    {
        LawnApp& app;
        bool previous;
        ~RestoreMute() { app.mMuteSoundsForCutscene = previous; }
    } restoreMute{app, app.mMuteSoundsForCutscene};
    app.mMuteSoundsForCutscene = true;
    Sexy::gUpdateIntervalNs = 10'000'000;
    Sexy::gUpdateTicks = 1.0;
    Sexy::gWholeLegacyTicks = 1;
    const auto originalBackground = board.mBackground;
    const auto originalMode = app.mGameMode;
    const auto run = [&](ProjectileMotion motion, ProjectileType type, int frequency,
        bool roof, bool highGravity) {
        board.mProjectiles.DataArrayFreeAll();
        board.mZombies.DataArrayFreeAll();
        board.RebuildZombieRowBucket();
        app.mEffectSystem->EffectSystemFreeAll();
        board.mLevelTimeNs = 0;
        board.mBackground = roof ? BackgroundType::BACKGROUND_5_ROOF : originalBackground;
        app.mGameMode = highGravity ? GameMode::GAMEMODE_LIMBO_HIGH_GRAVITY : originalMode;
        auto* shot = board.AddProjectile(321, 251, 0, 2, type);
        shot->mMotionType = motion;
        shot->mVelX = 3.312713f;
        shot->mVelY = motion == ProjectileMotion::MOTION_THREEPEATER ? -3.0f : 0.312713f;
        shot->mVelZ = motion == ProjectileMotion::MOTION_FLOAT_OVER ? -0.1317f : -7.318291f;
        shot->mAccZ = 0.115f;
        shot->mRotation = 0.312713f;
        shot->mRotationSpeed = 0.037281f;
        if (type == ProjectileType::PROJECTILE_COBBIG)
        {
            shot->mVelX = 0.001f;
            shot->mVelY = 0;
            shot->mVelZ = -8;
            shot->mAccZ = 0;
            shot->mCobTargetX = 600;
            shot->mCobTargetRow = 3;
        }
        const auto duration = type == ProjectileType::PROJECTILE_COBBIG ? 950'000'000LL : 130'123'457LL;
        Schedule(frequency, duration, [&](std::int64_t delta) {
            board.mLevelTimeNs += delta;
            shot->UpdateMotion();
        });
        Require(!shot->mDead && shot->mX == int(shot->mPosX) &&
            shot->mY == int(shot->mPosY + shot->mPosZ),
            "Discrete production projectile lost its collision pixel cache");
        Require(std::isfinite(shot->mPosX) && std::isfinite(shot->mPosY) &&
            std::isfinite(shot->mPosZ) && std::isfinite(shot->mRotation),
            "Discrete production projectile produced a non-finite state");
        if (type == ProjectileType::PROJECTILE_COBBIG)
            Require(shot->mVelZ == 8 && shot->mRow == 3 && shot->mPosX >= 600,
                "Production cob changed its original ascent/teleport/descent event");
        if (motion == ProjectileMotion::MOTION_STRAIGHT)
            Require(std::abs(shot->mPosX - (321.0 + shot->mVelX * (double(duration) / 10'000'000))) < .01,
                "Discrete straight projectile stopped using its configured horizontal velocity");
        if (motion == ProjectileMotion::MOTION_BACKWARDS)
            Require(shot->mPosX < 321, "Backward projectile moved in the forward direction");
        return std::array<std::uint64_t, 8>{Bits(shot->mPosX), Bits(shot->mPosY), Bits(shot->mPosZ),
            Bits(shot->mShadowY), Bits(shot->mRotation), std::uint64_t(shot->mRow),
            std::uint64_t(shot->mCreationTimeNs), Bits(shot->mVelZ)};
    };
    const struct { ProjectileMotion motion; ProjectileType type; } cases[]{
        {ProjectileMotion::MOTION_STRAIGHT, ProjectileType::PROJECTILE_SNOWPEA},
        {ProjectileMotion::MOTION_BACKWARDS, ProjectileType::PROJECTILE_PEA},
        {ProjectileMotion::MOTION_STAR, ProjectileType::PROJECTILE_STAR},
        {ProjectileMotion::MOTION_LOBBED, ProjectileType::PROJECTILE_KERNEL},
        {ProjectileMotion::MOTION_THREEPEATER, ProjectileType::PROJECTILE_PEA},
        {ProjectileMotion::MOTION_BEE, ProjectileType::PROJECTILE_PEA},
        {ProjectileMotion::MOTION_BEE_BACKWARDS, ProjectileType::PROJECTILE_PEA},
        {ProjectileMotion::MOTION_FLOAT_OVER, ProjectileType::PROJECTILE_PEA},
        {ProjectileMotion::MOTION_LOBBED, ProjectileType::PROJECTILE_COBBIG}
    };
    for (const auto& item : cases)
      for (bool roof : {false, true})
       for (bool highGravity : {false, true})
    {
        // Different step sizes intentionally follow different discrete paths.
        // Replay the same schedule to catch stale origins or unreset state.
        for (int frequency : {30, 60, 100, 144, 200, 240, 1000, 0})
        {
            const auto expected = run(item.motion, item.type, frequency, roof, highGravity);
            Require(run(item.motion, item.type, frequency, roof, highGravity) == expected,
                "Production discrete projectile failed to replay the same update schedule");
        }
    }
    board.mBackground = originalBackground;
    app.mGameMode = originalMode;
    for (const auto type : {ProjectileType::PROJECTILE_SNOWPEA,
        ProjectileType::PROJECTILE_KERNEL})
    {
        board.mProjectiles.DataArrayFreeAll();
        board.mZombies.DataArrayFreeAll();
        board.RebuildZombieRowBucket();
        app.mEffectSystem->EffectSystemFreeAll();
        board.mLevelTimeNs = 0;
        auto* projectile = board.AddProjectile(321, 251, 0, 2, type);
        Require(projectile->mX == 321 && projectile->mY == 251,
            "Production projectile birth did not synchronize pixel coordinates");
        if (type == ProjectileType::PROJECTILE_KERNEL)
        {
            projectile->mMotionType = ProjectileMotion::MOTION_LOBBED;
            projectile->mVelX = 3.25f;
            projectile->mVelY = 0.5f;
            projectile->mVelZ = -1.5f;
            projectile->mAccZ = 0.1f;
        }
        projectile->mDamageRangeFlags = 1 << int(DamageRangeFlags::DAMAGES_GROUND);
        const auto initialRect = projectile->GetProjectileRect();
        board.mLevelTimeNs = 10'000'000;
        projectile->UpdateMotion();
        const auto movedRect = projectile->GetProjectileRect();
        Require(!projectile->mDead && projectile->mPosX > 321 &&
            projectile->mX == int(projectile->mPosX) &&
            projectile->mY == int(projectile->mPosY + projectile->mPosZ),
            "Production projectile motion did not synchronize pixel coordinates");
        Require(movedRect.mX - initialRect.mX == projectile->mX - 321 &&
            movedRect.mX > initialRect.mX,
            "Production projectile collision rectangle did not follow motion");

        auto* zombie = board.mZombies.DataArrayAlloc();
        zombie->mZombieType = ZombieType::ZOMBIE_NORMAL;
        zombie->mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
        zombie->mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
        zombie->mScaleZombie = 1.0f;
        zombie->mRow = projectile->mRow;
        zombie->mX = static_cast<float>(movedRect.mX);
        zombie->mY = static_cast<float>(movedRect.mY);
        zombie->mZombieRect = Rect(0, 0, 40, 100);
        zombie->mWidth = 40;
        zombie->mHeight = 100;
        zombie->mBodyHealth = zombie->mBodyMaxHealth = 1000;
        zombie->mHasHead = true;
        board.RebuildZombieRowBucket();
        Require(projectile->FindCollisionTarget() == zombie,
            "Production projectile cannot find a target at its moved collision rectangle");
        const auto expectedHealth = zombie->mBodyHealth - projectile->GetProjectileDef().mDamage;
        projectile->DoImpact(zombie);
        Require(projectile->mDead && zombie->mBodyHealth == expectedHealth,
            "Production projectile impact did not retire the shot and apply its damage");
        Require(type != ProjectileType::PROJECTILE_SNOWPEA || zombie->mChilledEndTimeNs > static_cast<std::int64_t>(board.mLevelTimeNs),
            "Production snowpea impact did not chill its target");
    }
    board.mProjectiles.DataArrayFreeAll();
    board.mZombies.DataArrayFreeAll();
    board.RebuildZombieRowBucket();
    board.mLevelTimeNs = 0;
    auto* portalShot = board.AddProjectile(360, 251, 0, 2, ProjectileType::PROJECTILE_PEA);
    auto* entrance = board.mGridItems.DataArrayAlloc();
    auto* exit = board.mGridItems.DataArrayAlloc();
    entrance->mGridItemType = exit->mGridItemType = GridItemType::GRIDITEM_PORTAL_CIRCLE;
    entrance->mGridX = 4; entrance->mGridY = 2;
    exit->mGridX = 7; exit->mGridY = 3;
    const auto birth = portalShot->mCreationTimeNs;
    board.mChallenge->UpdatePortal(entrance);
    Require(portalShot->mRow == 3 && portalShot->mCreationTimeNs == birth,
        "Production portal changed the projectile's birth clock");
    const auto teleportedX = portalShot->mPosX;
    board.mLevelTimeNs = 10'000'000;
    Sexy::gUpdateIntervalNs = 10'000'000;
    Sexy::gUpdateTicks = 1.0;
    portalShot->UpdateMotion();
    Require(portalShot->mPosX == float(double(teleportedX) + portalShot->mVelX),
        "Production portal failed to continue motion from its destination");
    board.mGridItems.DataArrayFreeAll();
    board.mProjectiles.DataArrayFreeAll();
    board.mLevelTimeNs = 0;
    auto* homingTarget = board.mZombies.DataArrayAlloc();
    homingTarget->mZombieType = ZombieType::ZOMBIE_NORMAL;
    homingTarget->mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
    homingTarget->mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
    homingTarget->mScaleZombie = 1;
    homingTarget->mRow = 2;
    homingTarget->mX = 550;
    homingTarget->mY = 180;
    homingTarget->mZombieRect = Rect(0, 0, 40, 100);
    homingTarget->mWidth = 40;
    homingTarget->mHeight = 100;
    homingTarget->mBodyHealth = homingTarget->mBodyMaxHealth = 1000;
    homingTarget->mHasHead = true;
    board.RebuildZombieRowBucket();
    auto* homing = board.AddProjectile(321, 251, 0, 2, ProjectileType::PROJECTILE_SPIKE);
    homing->mMotionType = ProjectileMotion::MOTION_HOMING;
    homing->mVelX = 2;
    homing->mTargetZombieID = board.ZombieGetID(homingTarget);
    homing->mDamageRangeFlags = 1 << int(DamageRangeFlags::DAMAGES_GROUND);
    Schedule(144, 100'000'000, [&](std::int64_t delta) {
        board.mLevelTimeNs += delta;
        homing->UpdateMotion();
    });
    Require(!homing->mDead && homing->mPosX > 321 && homing->mVelY < 0,
        "Production homing projectile did not steer toward its real target");
    homingTarget->mX = homing->mPosX;
    homingTarget->mY = homing->mPosY - 10;
    homingTarget->mRow = homing->mRow;
    board.RebuildZombieRowBucket();
    const auto homingHealth = homingTarget->mBodyHealth - homing->GetProjectileDef().mDamage;
    homing->CheckForCollision();
    Require(homing->mDead && homingTarget->mBodyHealth == homingHealth,
        "Production homing projectile failed to hit its acquired target");
    board.mProjectiles.DataArrayFreeAll();
    board.mZombies.DataArrayFreeAll();
    board.RebuildZombieRowBucket();
    std::cout << "PASS actual projectile: 9 discrete modes, roofs/high gravity, 8 schedules, "
        "cob transition, portal motion, homing steering/hits, Snowpea/Kernel damage and chill\n";
}

inline void MagnetTransferOrigins(LawnApp& app, Board& board)
{
    using Sprite = MagnetAtlas::Sprite;
    const struct {
        ZombieType zombie;
        ReanimationType animation;
        ReanimTrackId track;
        Sprite sprite;
    } cases[] = {
        {ZOMBIE_PAIL, REANIM_ZOMBIE, ReanimTrackId::anim_bucket, Sprite::REANIM_ZOMBIE_BUCKET1},
        {ZOMBIE_FOOTBALL, REANIM_ZOMBIE_FOOTBALL, ReanimTrackId::zombie_football_helmet,
            Sprite::REANIM_ZOMBIE_FOOTBALL_HELMET},
        {ZOMBIE_DOOR, REANIM_ZOMBIE, ReanimTrackId::anim_screendoor, Sprite::REANIM_ZOMBIE_SCREENDOOR1},
        {ZOMBIE_LADDER, REANIM_LADDER, ReanimTrackId::Zombie_ladder_1, Sprite::REANIM_ZOMBIE_LADDER_1},
        {ZOMBIE_POGO, REANIM_POGO, ReanimTrackId::Zombie_pogo_stick, Sprite::ZOMBIEPOGO_1},
        {ZOMBIE_JACK_IN_THE_BOX, REANIM_JACKINTHEBOX, ReanimTrackId::Zombie_jackbox_box,
            Sprite::REANIM_ZOMBIE_JACKBOX_BOX},
        {ZOMBIE_DIGGER, REANIM_DIGGER, ReanimTrackId::Zombie_digger_pickaxe,
            Sprite::REANIM_ZOMBIE_DIGGER_PICKAXE}
    };
    auto* magnet = app.AddReanimation(0, 0, 0, REANIM_MAGNETSHROOM, app.mEffectSystem->mReanimationHolder->mBattleTimeNs, ReanimTimeDomain::Battle);
    for (const auto& sample : cases)
    {
        auto* body = app.AddReanimation(0, 0, 0, sample.animation, app.mEffectSystem->mReanimationHolder->mBattleTimeNs, ReanimTimeDomain::Battle);
        for (const float scale : {0.5f, 1.0f, -1.3f})
        {
            auto plant = std::bit_cast<Plant>(std::array<std::byte, sizeof(Plant)>{});
            plant.mApp = &app;
            plant.mBoard = &board;
            plant.mSeedType = SEED_MAGNETSHROOM;
            plant.mX = 100.25f;
            plant.mY = 200.75f;
            plant.mPlantCol = 1;
            plant.mRow = 2;
            plant.mBodyReanimID = app.ReanimationGetID(magnet);
            Zombie zombie{};
            zombie.mApp = &app;
            zombie.mBoard = &board;
            zombie.mZombieType = sample.zombie;
            zombie.mX = 550.125f;
            zombie.mY = 190.625f;
            zombie.mBodyReanimID = app.ReanimationGetID(body);
            zombie.mScaleZombie = std::abs(scale);
            zombie.mHasArm = zombie.mHasObject = zombie.mHasHead = true;
            zombie.mZombieHeight = HEIGHT_ZOMBIE_NORMAL;
            zombie.mZombiePhase = sample.zombie == ZOMBIE_JACK_IN_THE_BOX ?
                PHASE_JACK_IN_THE_BOX_RUNNING : PHASE_ZOMBIE_NORMAL;
            zombie.mHelmType = sample.zombie == ZOMBIE_PAIL ? HELMTYPE_PAIL :
                sample.zombie == ZOMBIE_FOOTBALL ? HELMTYPE_FOOTBALL : HELMTYPE_NONE;
            zombie.mShieldType = sample.zombie == ZOMBIE_DOOR ? SHIELDTYPE_DOOR :
                sample.zombie == ZOMBIE_LADDER ? SHIELDTYPE_LADDER : SHIELDTYPE_NONE;
            zombie.mHelmHealth = zombie.mHelmMaxHealth = 100;
            zombie.mShieldHealth = zombie.mShieldMaxHealth = 100;
            body->PlayReanim(sample.zombie == ZOMBIE_POGO ? ReanimTrackId::anim_pogo :
                sample.zombie == ZOMBIE_LADDER ? ReanimTrackId::anim_ladderwalk :
                ReanimTrackId::anim_walk, REANIM_LOOP, 0, 12.0f);
            body->SeekRatio(0.3372819f);
            body->mOverlayMatrix.LoadIdentity();
            body->mOverlayMatrix.m00 = scale;
            body->mOverlayMatrix.m11 = std::abs(scale);
            body->mOverlayMatrix.m02 = 21.25f;
            body->mOverlayMatrix.m12 = -17.75f;
            // Independently sample the exact center used by the source draw,
            // before the gameplay method removes equipment or switches pose.
            SexyTransform2D matrix;
            body->GetTrackMatrix(body->FindTrackIndex(sample.track), matrix);
            const float sourceX = zombie.mX + matrix.m02;
            const float sourceY = zombie.mY + matrix.m12;
            plant.MagnetShroomAttactItem(static_cast<std::int64_t>(board.mLevelTimeNs), &zombie);
            const auto& item = plant.mMagnetItems[0];
            const auto sprite = static_cast<size_t>(sample.sprite);
            const auto& rect = MagnetAtlas::Images[sprite];
            const float drawScale = MagnetAtlas::Scales[sprite];
            Require(std::abs(item.mPosX + rect.mWidth * drawScale * 0.5f - sourceX) < 0.0001f &&
                std::abs(item.mPosY + PlantDrawHeightOffset(static_cast<std::int64_t>(board.mLevelTimeNs), &board, &plant, plant.mSeedType,
                    plant.mPlantCol, plant.mRow) + rect.mHeight * drawScale * 0.5f - sourceY) < 0.0001f,
                "Equipment jumped away from its visible source center on magnet transfer");
            auto firstFrame = item;
            MagnetMotion::Advance(firstFrame, plant.mX + item.mDestOffsetX,
                plant.mY + item.mDestOffsetY, 0, 0);
            Require(std::abs(firstFrame.mPosX - item.mPosX) < 0.0001f &&
                std::abs(firstFrame.mPosY - item.mPosY) < 0.0001f,
                "Magnet simulation moved its transferred source position at zero step");
        }
        body->ReanimationDie();
    }
    auto plant = std::bit_cast<Plant>(std::array<std::byte, sizeof(Plant)>{});
    plant.mApp = &app;
    plant.mBoard = &board;
    plant.mSeedType = SEED_MAGNETSHROOM;
    plant.mState = STATE_READY;
    plant.mX = 100;
    plant.mY = 200;
    plant.mPlantCol = 1;
    plant.mRow = 2;
    plant.mBodyReanimID = app.ReanimationGetID(magnet);
    auto* ladder = board.AddALadder(2, 2);
    const float ladderX = board.GridToPixelX(2, 2) + 25.0f;
    const float ladderY = board.GridToPixelY(2, 2) - 4.0f;
    Sexy::gUpdateIntervalNs = 0;
    plant.UpdateMagnetShroom(static_cast<std::int64_t>(board.mLevelTimeNs));
    const auto& item = plant.mMagnetItems[0];
    Require(ladder->mDead && item.mItemType == MAGNET_ITEM_LADDER_PLACED &&
        std::abs(item.mPosX - ladderX) < 0.0001f &&
        std::abs(item.mPosY + PlantDrawHeightOffset(static_cast<std::int64_t>(board.mLevelTimeNs), &board, &plant,
            plant.mSeedType, plant.mPlantCol, plant.mRow) - ladderY) < 0.0001f,
        "Placed ladder jumped away from GridItem::DrawLadder's top-left on transfer");
    board.mGridItems.DataArrayFree(ladder);
    magnet->ReanimationDie();
    std::cerr << "PASS actual magnet equipment transfer: seven source tracks, scaled/mirrored poses, placed ladder\n";
}

inline void MagnetAttraction(LawnApp& app, Board& board)
{
    const auto oldProfile = app.mPlayerInfo;
    const auto oldMute = app.mMuteSoundsForCutscene;
    const auto oldCollected = board.mCoinsCollected;
    const auto oldLevelCoins = board.mLevelCoinsCollected;
    PlayerInfo profile;
    app.mPlayerInfo = &profile;
    app.mMuteSoundsForCutscene = true;
    struct RestoreMagnet
    {
        LawnApp& app;
        Board& board;
        PlayerInfo* profile;
        bool mute;
        int collected, levelCoins;
        ~RestoreMagnet()
        {
            app.mPlayerInfo = profile;
            app.mMuteSoundsForCutscene = mute;
            board.mCoinsCollected = collected;
            board.mLevelCoinsCollected = levelCoins;
        }
    } restore{app, board, oldProfile, oldMute, oldCollected, oldLevelCoins};
    MagnetTransferOrigins(app, board);
    auto* animation = app.AddReanimation(0, 0, 0, ReanimationType::REANIM_GOLD_MAGNET, app.mEffectSystem->mReanimationHolder->mBattleTimeNs, ReanimTimeDomain::Battle);
    const auto run = [&](bool gold, int frequency, std::int64_t duration) {
        board.mLevelTimeNs=0;
        auto plant = std::bit_cast<Plant>(std::array<std::byte, sizeof(Plant)>{});
        plant.mApp = &app;
        plant.mBoard = &board;
        plant.mX = plant.mY = 100;
        plant.mSeedType = gold ? SeedType::SEED_GOLD_MAGNET : SeedType::SEED_MAGNETSHROOM;
        plant.mState = PlantState::STATE_MAGNETSHROOM_CHARGING;
        plant.mStateCountdown = 100;
        plant.mBodyReanimID = app.ReanimationGetID(animation);
        auto& item = plant.mMagnetItems[0];
        item.mPosX = 900;
        item.mPosY = 100;
        item.mItemType = gold ? MagnetItemType::MAGNET_ITEM_GOLD_COIN : MagnetItemType::MAGNET_ITEM_PAIL_1;
        MagnetMotion::Bind(item, 100, 100);
        profile.mCoins = 0;
        Schedule(frequency, duration, [&](std::int64_t delta) {
            board.mLevelTimeNs+=delta;
            if (gold) plant.UpdateGoldMagnetShroom(static_cast<std::int64_t>(board.mLevelTimeNs), app.mEffectSystem->mReanimationHolder->mVisualTimeNs, (board.AnimationTimeDomain()==ReanimTimeDomain::Battle ? static_cast<std::int64_t>(board.mLevelTimeNs) : app.mEffectSystem->mReanimationHolder->mVisualTimeNs));
            else plant.UpdateMagnetShroom(static_cast<std::int64_t>(board.mLevelTimeNs));
            if(frequency==100 && board.mLevelTimeNs==10'000'000)
                Require(item.mVelocityX==-15.625f && item.mVelocityY==0 && item.mPosX==899.84375f,
                    "Production 100Hz magnet differs from inverse-square velocity-before-position baseline");
            Require(std::isfinite(item.mPosX) && item.mPosX>=100 && item.mPosX<=900 &&
                std::isfinite(item.mVelocityX) && std::isfinite(item.mVelocityY),
                "Production actual-step magnet left its legal flight segment");
        });
        if (duration == 900'000'000)
            Require(gold ? item.mItemType == MagnetItemType::MAGNET_ITEM_NONE && profile.mCoins == 5 :
                item.mPosX == 100 && item.mPosY == 100,
                "Production magnet did not capture its distant item below one second");
        else
        {
            const auto saved = SaveRecordMapping::WriteSaveRecord(plant);
            const auto saveTimeNs=board.mLevelTimeNs;
            Sexy::gUpdateIntervalNs = 23'456'789;
            board.mLevelTimeNs+=Sexy::gUpdateIntervalNs;
            if (gold) plant.UpdateGoldMagnetShroom(static_cast<std::int64_t>(board.mLevelTimeNs), app.mEffectSystem->mReanimationHolder->mVisualTimeNs, (board.AnimationTimeDomain()==ReanimTimeDomain::Battle ? static_cast<std::int64_t>(board.mLevelTimeNs) : app.mEffectSystem->mReanimationHolder->mVisualTimeNs)); else plant.UpdateMagnetShroom(static_cast<std::int64_t>(board.mLevelTimeNs));
            const auto expected = std::array<std::uint64_t, 3>{
                Bits(item.mPosX), Bits(item.mPosY), static_cast<std::uint64_t>(plant.mMagnetStartTimeNs)};
            SaveRecordMapping::ReadSaveRecord(plant, saved.data());
            plant.mApp = &app;
            plant.mBoard = &board;
            board.mLevelTimeNs=saveTimeNs;
            SaveClockProjection::Bind(plant);
            board.mLevelTimeNs+=Sexy::gUpdateIntervalNs;
            if (gold) plant.UpdateGoldMagnetShroom(static_cast<std::int64_t>(board.mLevelTimeNs), app.mEffectSystem->mReanimationHolder->mVisualTimeNs, (board.AnimationTimeDomain()==ReanimTimeDomain::Battle ? static_cast<std::int64_t>(board.mLevelTimeNs) : app.mEffectSystem->mReanimationHolder->mVisualTimeNs)); else plant.UpdateMagnetShroom(static_cast<std::int64_t>(board.mLevelTimeNs));
            Require(std::array<std::uint64_t, 3>{Bits(item.mPosX), Bits(item.mPosY),
                static_cast<std::uint64_t>(plant.mMagnetStartTimeNs)} == expected, "Magnet flight changed after native save/reload");
        }
        return std::array<std::uint64_t, 4>{
              Bits(item.mPosX), Bits(item.mPosY), static_cast<std::uint64_t>(plant.mMagnetStartTimeNs),
              static_cast<std::uint64_t>(profile.mCoins)};
    };
    for (bool gold : {false, true})
        for (const auto duration : {400'000'000LL, 900'000'000LL})
        {
            for (int frequency : {1, 30, 60, 100, 144, 200, 240, 1000, 0})
                run(gold, frequency, duration);
        }
    animation->ReanimationDie();
    std::cerr << "PASS actual discrete inverse-square magnet / sub-second capture / save continuation\n";
}

inline void PickupMotion(LawnApp& app, Board& board)
{
    const auto oldProfile = app.mPlayerInfo;
    const auto oldMode = app.mGameMode;
    const auto oldDave = app.mCrazyDaveState;
    const auto oldSun = board.mSunMoney;
    const auto oldCollected = board.mCoinsCollected;
    const auto oldLevelCoins = board.mLevelCoinsCollected;
    PlayerInfo profile;
    app.mPlayerInfo = &profile;
    app.mCrazyDaveState = CrazyDaveState::CRAZY_DAVE_OFF;
    struct RestorePickup
    {
        LawnApp& app;
        Board& board;
        PlayerInfo* profile;
        GameMode mode;
        CrazyDaveState dave;
        int sun, collected, levelCoins;
        ~RestorePickup()
        {
            board.mCoins.DataArrayFreeAll();
            app.mPlayerInfo = profile;
            app.mGameMode = mode;
            app.mCrazyDaveState = dave;
            board.mSunMoney = sun;
            board.mCoinsCollected = collected;
            board.mLevelCoinsCollected = levelCoins;
        }
    } restore{app, board, oldProfile, oldMode, oldDave, oldSun, oldCollected, oldLevelCoins};
    const auto make = [&](CoinType type) {
        board.mCoins.DataArrayFreeAll();
        board.mSunMoney = 100;
        board.mCoinsCollected = 0;
        board.mLevelCoinsCollected = 0;
        profile.mCoins = 0;
        app.mGameMode = GameMode::GAMEMODE_MINIGAME_WALLNUT_BOWLING;
        auto* coin = board.mCoins.DataArrayAlloc();
        coin->mApp = &app;
        coin->mBoard = &board;
        coin->mType = type;
        coin->mAttachmentID = AttachmentID::ATTACHMENTID_NULL;
        coin->mDead = false;
        coin->mIsBeingCollected = true;
        coin->mCollectX = coin->mPosX = type == CoinType::COIN_SUN ? 415.0f : 439.0f;
        coin->mCollectY = coin->mPosY = type == CoinType::COIN_SUN ? 0.0f : 558.0f;
        coin->mDisappearStartTimeNs=INT64_MAX;
        return coin;
    };
    const auto setStep = [](unsigned long long ns) {
        Sexy::gUpdateIntervalNs = ns;
        Sexy::gUpdateTicks = double(ns) / 10'000'000;
    };
    const auto state = [](const Coin& coin) {
        return std::array<std::uint64_t, 5>{Bits(coin.mPosX), Bits(coin.mPosY),
            Bits(coin.mCollectionDistance), Bits(coin.mScale),
            static_cast<std::uint64_t>(coin.mDisappearStartTimeNs==INT64_MAX ? 0 : coin.mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs-coin.mDisappearStartTimeNs)};
    };
    for (const auto type : {CoinType::COIN_SUN, CoinType::COIN_GOLD})
    {
        const auto sample = [&](int frequency) {
            auto* coin = make(type);
            float expectedX = coin->mPosX, expectedY = coin->mPosY;
            const float targetX = type == CoinType::COIN_SUN ? 15.f : 39.f;
            const float targetY = type == CoinType::COIN_SUN ? 0.f : 558.f;
            Schedule(frequency, 137'192'835, [&](std::int64_t ns) {
                // Reference integrates each actual nanosecond step from its
                // current coordinate, never from the original collection point.
                const float dx = targetX - expectedX, dy = targetY - expectedY;
                expectedX += (dx / 21.f) * static_cast<float>(double(ns) / 10'000'000);
                expectedY += (dy / 21.f) * static_cast<float>(double(ns) / 10'000'000);
                coin->UpdateCollected();
                Require(Bits(coin->mPosX) == Bits(expectedX) && Bits(coin->mPosY) == Bits(expectedY),
                    "Pickup did not accumulate its current nanosecond step");
            });
            Require(!coin->mDead, "Pickup completed before the original distance threshold");
            return state(*coin);
        };
        for (const auto frequency : {1, 30, 60, 100, 144, 200, 240, 1000, 0})
            Require(sample(frequency) == sample(frequency),
                "Pickup failed to replay the same discrete update schedule");

        auto* savedCoin = make(type);
        setStep(123'456'789);
        savedCoin->UpdateCollected();
        const auto saved = SaveRecordMapping::WriteSaveRecord(*savedCoin);
        const auto continuedState = [&] {
            savedCoin->UpdateCollected();
            return state(*savedCoin);
        };
        setStep(23'456'789);
        const auto continued = continuedState();
        SaveRecordMapping::ReadSaveRecord(*savedCoin, saved.data());
        savedCoin->mApp = &app;
        savedCoin->mBoard = &board;
        SaveClockProjection::Bind(*savedCoin);
        Require(continuedState() == continued, "Pickup changed its discrete state after native save/reload");
        const auto pausedState = state(*savedCoin);
        setStep(0);
        savedCoin->UpdateCollected();
        Require(state(*savedCoin) == pausedState, "Zero-step pickup advanced");

        auto* coin = make(type);
        setStep(10'000'000);
        int updates = 0;
        while (!coin->mDead && updates < 200)
        {
            coin->UpdateCollected();
            ++updates;
        }
        Require(updates == (type == CoinType::COIN_SUN ? 82 : 73),
            "400px pickup changed the original 100Hz completion update");
        Require(type == CoinType::COIN_SUN ? board.mSunMoney == 125 : profile.mCoins == 5,
            "Pickup did not award its value exactly once");
    }
    auto* coin = make(CoinType::COIN_GOLD);
    setStep(123'456'789);
    coin->UpdateCollected();
    const auto previousX = coin->mPosX, previousY = coin->mPosY;
    app.mGameMode = GameMode::GAMEMODE_LIMBO_ZEN_GARDEN;
    setStep(7'192'835);
    coin->UpdateCollected();
    const float expectedX = previousX + ((442.f - previousX) / 21.f) *
        static_cast<float>(double(7'192'835) / 10'000'000);
    const float expectedY = previousY + ((558.f - previousY) / 21.f) *
        static_cast<float>(double(7'192'835) / 10'000'000);
    Require(Bits(coin->mPosX) == Bits(expectedX) && Bits(coin->mPosY) == Bits(expectedY),
        "Pickup destination switch did not continue from the current position");
    coin = make(CoinType::COIN_GOLD);
    coin->mDisappearStartTimeNs=app.mEffectSystem->mReanimationHolder->mVisualTimeNs+10'000'000;
    const auto untouched = state(*coin);
    setStep(0);
    coin->UpdateCollected();
    Require(!coin->mDead && state(*coin) == untouched,
        "Zero nanosecond step changed a historical pickup timer or coordinate");
    std::cerr << "PASS actual pickup discrete nanosecond steps / 1073 thresholds / native save continuation / destination changes\n";
}

inline void SlotMachine(LawnApp& app, Board& board)
{
    const auto oldScene = app.mGameScene;
    const auto oldTime = board.mLevelTimeNs;
    app.mGameScene = GameScenes::SCENE_PLAYING;
    const auto sample = [&](int frequency, std::int64_t duration, bool original) {
        board.mLevelTimeNs = 1;
        Sexy::SRand(0x5191);
        SeedPacket packet;
        packet.mApp = &app;
        packet.mBoard = &board;
        packet.mPacketType = SeedType::SEED_PEASHOOTER;
        packet.mIndex = 0;
        packet.mActive = false;
        packet.SlotMachineStart();
        int countdown = 400;
        Schedule(frequency, duration, [&](std::int64_t delta) {
            board.mLevelTimeNs += delta;
            if (!original) {packet.Update(); return;}
            if (countdown == 0) return;
            --countdown;
            const float warped = (countdown - 400) / -400.0f;
            const float speed = 6.0f + (2.0f - 6.0f) * warped;
            packet.mSlotMachiningPosition += speed * .01f;
            if (packet.mSlotMachiningPosition >= 1.0f)
            {
                packet.mPacketType = packet.mSlotMachiningNextSeed;
                if (countdown == 0) {packet.Activate();packet.mSlotMachiningPosition=0;}
                else {packet.mSlotMachiningPosition -= 1.0f;packet.PickNextSlotMachineSeed();}
            }
            else if (countdown == 0) countdown = 1;
        });
        const auto remaining = original ? countdown * 10'000'000LL :
            packet.mSlotMachineEndTimeNs == 0 ? 0 : packet.mSlotMachineEndTimeNs <= static_cast<std::int64_t>(board.mLevelTimeNs)
                ? 10'000'000 : packet.mSlotMachineEndTimeNs - static_cast<std::int64_t>(board.mLevelTimeNs);
        return std::array<std::int64_t, 6>{static_cast<int>(packet.mPacketType),
            static_cast<int>(packet.mSlotMachiningNextSeed), remaining, packet.mActive,
            Sexy::Rand(), static_cast<std::int64_t>(Bits(packet.mSlotMachiningPosition))};
    };
    for (const auto duration : {1'230'000'000LL, 3'100'000'000LL,
        4'000'000'000LL, 4'010'000'000LL, 4'020'000'000LL})
        Require(sample(100,duration,false) == sample(100,duration,true),
            "100Hz slot phase, random seed sequence or completion differs from1073");
    {
        board.mLevelTimeNs = 1;
        SeedPacket packet;
        packet.mApp=&app; packet.mBoard=&board; packet.mIndex=0;
        packet.mPacketType=SeedType::SEED_PEASHOOTER; packet.mActive=false;
        packet.SlotMachineStart();
        Schedule(144,4'000'000'000LL,[&](std::int64_t delta){board.mLevelTimeNs+=delta;packet.Update();});
        Require(packet.mSlotMachineEndTimeNs!=0 &&
            packet.mSlotMachineEndTimeNs==static_cast<std::int64_t>(board.mLevelTimeNs),
            "144Hz slot fixture did not reach its active expiry boundary");
        for (const auto overdue : {0LL,1'234'567LL})
        {
            if (overdue) {Sexy::gUpdateIntervalNs=overdue;board.mLevelTimeNs+=overdue;packet.Update();}
            const auto bytes=SaveRecordMapping::WriteSaveRecord(packet);
            SeedPacket restored;
            SaveRecordMapping::ReadSaveRecord(restored,bytes.data());
            restored.mApp=&app; restored.mBoard=&board;
            SaveClockProjection::Bind(restored);
            Require(restored.mSlotMachineEndTimeNs==packet.mSlotMachineEndTimeNs &&
                Bits(restored.mSlotMachiningPosition)==Bits(packet.mSlotMachiningPosition),
                "Slot save re-armed an expired final-turn deadline");
            Sexy::gUpdateIntervalNs=1; ++board.mLevelTimeNs;
            packet.Update(); restored.Update();
            Require(Bits(restored.mSlotMachiningPosition)==Bits(packet.mSlotMachiningPosition) &&
                restored.mSlotMachineEndTimeNs==packet.mSlotMachineEndTimeNs,
                "Slot final-turn simulation changed after save/reload");
        }
    }
    for (const int frequency : {1,30,60,144,200,240,1000,0})
    {
        const auto result = sample(frequency,6'000'000'000LL,false);
        if (!(result[2] == 0 && result[3] == 1 && result[5] == Bits(0.0f)))
            std::cerr << "Slot completion frequency=" << frequency << " remaining=" << result[2]
                << " active=" << result[3] << " phaseBits=" << result[5] << '\n';
        Require(result[2] == 0 && result[3] == 1 && result[5] == Bits(0.0f),
            "Discrete nanosecond slot failed to stop at a whole turn");
    }
    board.mLevelTimeNs = oldTime;
    app.mGameScene = oldScene;
    std::cerr << "PASS discrete slot100Hz original phase/RNG and other-frequency legal completion\n";
}

inline void ChangingVelocity()
{
    constexpr std::uint64_t deltaNs = 1'250'000;
    std::int64_t nowNs = 0;
    float position = 1'048'576;
    TimedLinearMotion motion;
    for (std::uint64_t i=1;i<=4096;++i)
    {
        nowNs += deltaNs;
        AdvanceVelocity(position,.125f,deltaNs,nowNs,motion);
        Require(position==static_cast<float>(1'048'576.0+.125*double(i*deltaNs)/10'000'000),
            "Constant velocity lost its birth-clock endpoint");
    }
    constexpr std::array<float,4> rates{.125f,-.0625f,.25f,-.125f};
    position=1'048'576;motion={};nowNs=0;
    for (size_t i=0;i<4096;++i)
    {
        const float rate=rates[i%rates.size()];
        const double origin=position;
        nowNs += deltaNs;
        AdvanceVelocity(position,rate,deltaNs,nowNs,motion);
        Require(position==static_cast<float>(origin+double(rate)*deltaNs/10'000'000) &&
            motion.origin==origin && motion.startTimeNs==nowNs-static_cast<std::int64_t>(deltaNs),
            "Changed velocity did not rebind its segment origin");
        const auto before=Bits(position);
        AdvanceVelocity(position,rate,0,nowNs,motion);
        Require(Bits(position)==before,"Zero time advanced a linear segment");
    }
    position=53.375f;motion={};nowNs+=deltaNs;
    AdvanceVelocity(position,.25f,deltaNs,nowNs,motion);
    Require(position==53.40625f && motion.origin==53.375,"Relocation lost its new segment origin");
    position=1'048'576;motion={};nowNs+=deltaNs;
    AdvanceVelocity(position,.125f,deltaNs,nowNs,motion);
    const float reset=position;
    motion.bound=false;nowNs+=deltaNs;
    AdvanceVelocity(position,-.125f,deltaNs,nowNs,motion);
    Require(motion.origin==double(reset)&&position==reset-.015625f,"Invalidation failed to rebind");
    const auto birth=motion.startTimeNs;
    AdvanceVelocity(position,-.125f,1,nowNs+50'000'000,motion);
    Require(motion.startTimeNs==birth && position==CheckedPosition(motion.origin-.125*double(nowNs+50'000'000-birth)/10'000'000),
        "Linear motion accumulated delta instead of sampling the supplied clock");
}

inline void ChangingVelocitySave(LawnApp& app, Board& board)
{
    Zombie original{};
    original.mApp = &app; original.mBoard = &board;
    original.mZombieType = ZombieType::ZOMBIE_NORMAL;
    original.mX = 1'048'576;
    // Save the sub-ULP clock state needed to continue the same-rate segment.
    board.mLevelTimeNs += 1'250'000;
    AdvanceVelocity(original.mX, .125f, 1'250'000, board.mLevelTimeNs, original.mVelocityX);
    const auto record = SaveRecordMapping::WriteSaveRecord(original);
    Zombie resumed{};
    SaveRecordMapping::ReadSaveRecord(resumed, record.data());
    resumed.mApp = &app; resumed.mBoard = &board;
    SaveClockProjection::Bind(resumed);
    const auto state = [](const Zombie& zombie) {
        const auto& motion = zombie.mVelocityX;
        return std::array<std::uint64_t, 5>{Bits(zombie.mX), Bits(motion.origin),
            static_cast<std::uint64_t>(motion.startTimeNs), Bits(motion.rate),
            std::uint64_t(motion.bound)};
    };
    Require(state(original) == state(resumed), "Save lost a sub-ULP linear segment");
    board.mLevelTimeNs += 1'000'000'000;
    AdvanceVelocity(original.mX, .125f, 1'000'000'000, board.mLevelTimeNs, original.mVelocityX);
    AdvanceVelocity(resumed.mX, .125f, 1'000'000'000, board.mLevelTimeNs, resumed.mVelocityX);
    Require(state(original) == state(resumed) &&
        resumed.mX == static_cast<float>(1'048'576.0 + .125 * 1'001'250'000 / 10'000'000),
        "Save continuation lost the original constant-rate nanosecond endpoint");
    constexpr std::array<float, 4> rates{.25f, -.125f, .125f, -.0625f};
    for (size_t i = 0; i < 4096; ++i)
    {
        const auto rate = rates[i % rates.size()];
        const double origin = original.mX;
        const auto expected = static_cast<float>(origin + double(rate) * 1'250'000 / 10'000'000);
        board.mLevelTimeNs += 1'250'000;
        AdvanceVelocity(original.mX, rate, 1'250'000, board.mLevelTimeNs, original.mVelocityX);
        AdvanceVelocity(resumed.mX, rate, 1'250'000, board.mLevelTimeNs, resumed.mVelocityX);
        Require(state(original) == state(resumed) &&
            resumed.mX == expected && resumed.mVelocityX.origin == origin,
            "Changing-rate continuation diverged after actual native save/reload");
    }
}

inline void Check(LawnApp& app)
{
    ChangingVelocity();
    for (int frequency : {30, 60, 100, 144, 200, 240, 1000, 0})
    {
        float position = 91.123413f;
        TimedLinearMotion motion;
        constexpr std::int64_t duration = 2'718'281'829;
        std::int64_t nowNs=0;
        Schedule(frequency, duration, [&](std::int64_t delta) {
            nowNs+=delta;
            AdvanceVelocity(position, -0.7137289f, delta, nowNs, motion);
        });
        Require(position == CheckedPosition(double(91.123413f) +
            double(-0.7137289f) * duration / 10'000'000.0),
            "Timed float velocity depends on update partition");
        position = 177.283919f;
        motion = {}; // Explicitly invalidate the old segment at relocation.
        AdvanceVelocity(position, 0.3127131f, 1'234'567, nowNs+1'234'567, motion);
        Require(position == CheckedPosition(double(177.283919f) +
            double(0.3127131f) * 1'234'567 / 10'000'000.0),
            "Timed float velocity ignored an explicit teleport or changed rate");
    }
    struct ResourceGroups
    {
        LawnApp& app;
        bool fonts, images;
        ~ResourceGroups()
        {
            if (images) app.mResourceManager->DeleteResources(ResourceGroupId::LoadingImages);
            if (fonts) app.mResourceManager->DeleteResources(ResourceGroupId::LoadingFonts);
            app.CleanSharedImages();
        }
    } resourceGroups{app,
        !app.mResourceManager->IsGroupLoaded(ResourceGroupId::LoadingFonts),
        !app.mResourceManager->IsGroupLoaded(ResourceGroupId::LoadingImages)};
    Require(app.mResourceManager->LoadResources(ResourceGroupId::LoadingFonts),
        "Cannot load production fonts for the real Board constructor");
    std::cerr << "PASS actor fixture production fonts\n";
    Require(app.mResourceManager->LoadResources(ResourceGroupId::LoadingImages) &&
        Sexy::IMAGE_SEEDBANK, "Cannot load real SeedBank image required by Board construction");
    std::cerr << "PASS actor fixture production Board images\n";
    const auto previousBoard = app.mBoard;
    const auto previousMode = app.mGameMode;
    const auto previousTicks = Sexy::gUpdateTicks;
    const auto previousDelta = Sexy::gUpdateIntervalNs;
    const auto previousWholeTicks = Sexy::gWholeLegacyTicks;
    std::unique_ptr<EffectSystem> ownedEffects;
    if (!app.mEffectSystem)
    {
        ownedEffects = std::make_unique<EffectSystem>();
        ownedEffects->EffectSystemInitialize();
        app.mEffectSystem = ownedEffects.get();
    }
    std::cerr << "PASS actor fixture real effect pools\n";
    struct Restore
    {
        LawnApp& app;
        Board* board;
        GameMode mode;
        double ticks;
        unsigned long long delta;
        int whole;
        bool owned;
        ~Restore()
        {
            app.mBoard = board;
            app.mGameMode = mode;
            if (owned) app.mEffectSystem = nullptr;
            Sexy::gUpdateTicks = ticks;
            Sexy::gUpdateIntervalNs = delta;
            Sexy::gWholeLegacyTicks = whole;
        }
    } restore{app, previousBoard, previousMode, previousTicks, previousDelta,
        previousWholeTicks, bool(ownedEffects)};
    app.mGameMode = GameMode::GAMEMODE_MINIGAME_WALLNUT_BOWLING;
    Board board(&app);
    std::cerr << "PASS actor fixture real Board construction\n";
    TreeOfWisdomEntryPose(app,board);
    StinkyGardenReentry(app,board);
    ChangingVelocitySave(app, board);
    std::cerr << "PASS changing-rate linear segments: exact binary oracle, zero steps, "
        "teleport, invalidation, nonlinear handoff and native save continuation\n";
    SharedClockConsumers(board);
    CreditsScrollRuntime::Check();
    CoinFallRuntime::Check(app,board);
    std::cerr << "PASS actual analytic Coin/Sun falling and wall events\n";
    MowerMotionRuntime::Run(app,board);
    MowerMotionRuntime::SaveContinuation(app,board);
    std::cerr << "PASS actual analytic mower motion, pool height and brain falling\n";
    ZombieAltitudeRuntime::Check(app,board);
    std::cerr << "PASS actual Zombie altitude: 14 paths, roof Imp, combined Balloon, save continuation\n";
    VaultAndRoofImpSegments(app, board);
    ZamboniRuntime::Check(app,board);
    std::cerr << "PASS actual Zamboni discrete movement, timestamp tire speed and save continuation\n";
    LaunchDeadlines(app, board);
    SlotMachine(app, board);
    PickupMotion(app, board);
    MagnetAttraction(app, board);
    for (const auto seed : {SeedType::SEED_WALLNUT, SeedType::SEED_GIANT_WALLNUT})
    {
        const auto expected = Bowling(app, board, 100, seed);
        std::cerr << "PASS actor bowling reference: seed " << int(seed) << '\n';
        for (int frequency : {1, 30, 60, 144, 200, 240, 1000, 0})
            Require(Bowling(app, board, frequency, seed) == expected,
                "Production bowling depends on outer update partition");
    }
    GiantBowlingImpact(app, board);
    board.mPlants.DataArrayFreeAll();
    board.RebuildPlantCellBucket();
    const auto walking = Walking(app, board, 100);
    std::cerr << "PASS actor walking reference\n";
    for (int frequency : {30, 60, 144, 200, 240, 1000, 0})
        Require(Walking(app, board, frequency) == walking,
            "Production zombie walking depends on outer update partition");
    const auto additionalWalking = Walking(app, board, 100, true);
    for (int frequency : {30, 60, 144, 200, 240, 1000, 0})
        Require(Walking(app, board, frequency, true) == additionalWalking,
            "Production root plus additional movement depends on outer update partition");
    Projectiles(app, board);
    std::cout << "PASS actual Plant bowling / Zombie walking: float coordinates, source roots, "
        "canonical bounce / row bucket, teleport rebinding and exact partition states\n";
}
}
