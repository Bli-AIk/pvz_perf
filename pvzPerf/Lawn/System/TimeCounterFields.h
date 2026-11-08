#pragma once
#include <type_traits>
#include <cstring>
#include <cstddef>

// Included after the save record classes. This describes native 64-bit time
// members, including durations and time snapshots. Wire encoding is handled by
// Dat64TimeSchema/SaveRecordMapping, never by overwriting native 4-byte slots.
template<class T, class Fn>
void VisitTimeCounterOffsets(Fn&& visit)
{
    using Type = std::remove_cv_t<T>;
#define TIMER(field) visit(offsetof(Type, field), std::is_unsigned_v<typename decltype(Type::field)::Nanoseconds>)
    if constexpr (std::is_same_v<Type, Board>)
    {
        TIMER(mFogBlownCountDown); TIMER(mSunCountDown); TIMER(mShakeCounter);
        visit(offsetof(Type, mLevelTimeNs), true);
        TIMER(mEffectCounter); TIMER(mRiseFromGraveCounter);
        TIMER(mOutOfMoneyCounter); TIMER(mTutorialTimer); TIMER(mZombieCountDown);
        TIMER(mHugeWaveCountDown); TIMER(mCoinBankFadeCount); TIMER(mBoardFadeOutCounter);
        TIMER(mNextSurvivalStageCounter); TIMER(mScoreNextMowerCounter);
        TIMER(mFlagRaiseCounter); TIMER(mIceTrapCounter); TIMER(mFwooshCountDown);
        TIMER(mTimeStopCounter); TIMER(mFinalWaveSoundCounter); TIMER(mCobCannonCursorDelayCounter);
        TIMER(mZombieCountDownStart); TIMER(mPlayTimeActiveLevel); TIMER(mPlayTimeInactiveLevel);
        TIMER(mStartDrawTime); TIMER(mIntervalDrawTime); TIMER(mPreloadTime);
        for (size_t i = 0; i < MAX_GRID_SIZE_Y; ++i)
            visit(offsetof(Type, mIceTimer) + i * sizeof(Sexy::TimeCounter), false);
    }
    else if constexpr (std::is_same_v<Type, Zombie>)
    {
        TIMER(mAnimCounter); TIMER(mGroanCounter); visit(offsetof(Type, mShotFlashEndTimeNs), false);
        visit(offsetof(Type, mShieldFlashEndTimeNs), false); visit(offsetof(Type, mShieldRecoilEndTimeNs), false); visit(offsetof(Type, mBirthTimeNs), false);
        TIMER(mPhaseCounter); visit(offsetof(Type, mFadeEndTimeNs), false); visit(offsetof(Type, mChilledEndTimeNs), false); visit(offsetof(Type, mButteredEndTimeNs), false);
        visit(offsetof(Type, mIceTrapEndTimeNs), false); visit(offsetof(Type, mYuckyFaceStartTimeNs), false); TIMER(mSummonCounter);
        TIMER(mBossBungeeCounter); TIMER(mBossStompCounter); TIMER(mBossHeadCounter);
        TIMER(mAnimTicksPerFrame);
    }
    else if constexpr (std::is_same_v<Type, Plant>)
    {
        TIMER(mAnimCounter); visit(offsetof(Type, mDisappearEndTimeNs), false); TIMER(mDoSpecialCountdown);
        TIMER(mStateCountdown); TIMER(mZenProductionCountdown); visit(offsetof(Type, mShootingEndTimeNs), false);
        TIMER(mBlinkCountdown); visit(offsetof(Type, mRecentlyEatenEndTimeNs), false); visit(offsetof(Type, mEatenFlashEndTimeNs), false);
        visit(offsetof(Type, mBeghouledFlashEndTimeNs), false); visit(offsetof(Type, mWakeUpEndTimeNs), false);
        TIMER(mFrameLength); TIMER(mLaunchRate);
    }
    else if constexpr (std::is_same_v<Type, Projectile>)
    {
        // Frozen wire timers are projections; runtime owns absolute timestamps.
    }
    else if constexpr (std::is_same_v<Type, Coin>)
    {
        visit(offsetof(Type, mFadeEndTimeNs), false); visit(offsetof(Type, mBirthTimeNs), false); visit(offsetof(Type, mDisappearStartTimeNs), false);
    }
    else if constexpr (std::is_same_v<Type, LawnMower>)
    {
        visit(offsetof(Type, mChompEndTimeNs), false); visit(offsetof(Type, mRollingInStartTimeNs), false); visit(offsetof(Type, mSquishEndTimeNs), false);
        TIMER(mAnimTicksPerFrame);
    }
    else if constexpr (std::is_same_v<Type, GridItem>)
    {
        visit(offsetof(Type, mStateStartTimeNs), false); visit(offsetof(Type, mTransparencyTimeNs), false);
    }
    else if constexpr (std::is_same_v<Type, TodParticle>)
    {
        visit(offsetof(Type, mBirthTimeNs), false);
        visit(offsetof(Type, mParticleDurationNs), false);
        visit(offsetof(Type, mCrossFadeDurationNs), false);
    }
    else if constexpr (std::is_same_v<Type, TodParticleEmitter>)
    {
        visit(offsetof(Type, mBirthTimeNs), false);
        visit(offsetof(Type, mCrossFadeEndTimeNs), false);
        visit(offsetof(Type, mSystemDurationNs), false);
    }
    else if constexpr (std::is_same_v<Type, ReanimatorTrackInstance>)
    {
        TIMER(mBlendCounter); TIMER(mBlendTime);
    }
    else if constexpr (std::is_same_v<Type, CursorObject>)
        TIMER(mHammerDownCounter);
    else if constexpr (std::is_same_v<Type, MessageWidget>)
    {
        TIMER(mDisplayTime); visit(offsetof(Type, mEndTimeNs), false); TIMER(mSlideOffTime);
    }
    else if constexpr (std::is_same_v<Type, SeedPacket>)
    {
        visit(offsetof(Type, mRefreshStartTimeNs), false); visit(offsetof(Type, mSlotMachineEndTimeNs), false);
        TIMER(mRefreshTime);
    }
    else if constexpr (std::is_same_v<Type, SeedBank>)
    {
        TIMER(mConveyorBeltCounter);
        for (size_t i = 0; i < 10; ++i)
            VisitTimeCounterOffsets<SeedPacket>([&](size_t offset, bool isUnsigned) {
                visit(offsetof(Type, mSeedPackets) + i * sizeof(SeedPacket) + offset, isUnsigned);
            });
    }
    else if constexpr (std::is_same_v<Type, Challenge>)
    {
        TIMER(mChallengeStateCounter); TIMER(mConveyorBeltCounter); TIMER(mRainCounter);
        for (size_t i = 0; i < 6; ++i)
            visit(offsetof(Type, mCloudsCounter) + i * sizeof(Sexy::TimeCounter), false);
    }
    else if constexpr (std::is_same_v<Type, Music>)
    {
        visit(offsetof(Type, mBurstEndTimeNs), false); visit(offsetof(Type, mDrumsEndTimeNs), false); visit(offsetof(Type, mFadeOutEndTimeNs), false);
        visit(offsetof(Type, mFadeOutDurationNs), false);
    }
#undef TIMER
}
