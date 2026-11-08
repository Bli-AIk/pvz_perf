#include "SaveClockProjection.h"
#include "LawnApp.h"
#include "Lawn/Board.h"
#include "Lawn/Plant.h"
#include "Lawn/Zombie.h"
#include "Lawn/Coin.h"
#include "Lawn/LawnMower.h"
#include "Lawn/GridItem.h"
#include "Lawn/SeedPacket.h"
#include "Lawn/MessageWidget.h"
#include "Lawn/System/Music.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/ReanimClock.h"
#include <algorithm>
#include <cstring>
namespace SaveClockProjection
{
namespace
{
enum class Kind { None, Raw, ShootingEnd, Start, End, SignedEnd, Fade, InfiniteEnd, DormantStart, Refresh, SlotEnd, Transparency, Health, Disappear };
template<class T, class F> void Fields(const T& v, F&& f)
{
#define CLOCK(field,kind) f(offsetof(T, field),Kind::kind)
    if constexpr(std::is_same_v<T,Plant>) {
        CLOCK(mDisappearEndTimeNs,Disappear); CLOCK(mRecentlyEatenEndTimeNs,End);
        CLOCK(mEatenFlashEndTimeNs,End); CLOCK(mBeghouledFlashEndTimeNs,End); CLOCK(mWakeUpEndTimeNs,End);
        CLOCK(mMagnetStartTimeNs,Start); CLOCK(mShootingEndTimeNs,ShootingEnd);
    } else if constexpr(std::is_same_v<T,Zombie>) {
        CLOCK(mShotFlashEndTimeNs,SignedEnd); CLOCK(mShieldFlashEndTimeNs,End); CLOCK(mShieldRecoilEndTimeNs,End);
        CLOCK(mBirthTimeNs,Start); CLOCK(mFadeEndTimeNs,Fade); CLOCK(mChilledEndTimeNs,End);
        CLOCK(mButteredEndTimeNs,End); CLOCK(mIceTrapEndTimeNs,End); CLOCK(mYuckyFaceStartTimeNs,Start);
        f(offsetof(T,mVelocityX)+offsetof(TimedLinearMotion,startTimeNs),v.mFlatTires?Kind::Raw:Kind::Start);
        f(offsetof(T,mVelocityY)+offsetof(TimedLinearMotion,startTimeNs),Kind::Start);
        f(offsetof(T,mAltitudeMotion)+offsetof(TimedLinearMotion,startTimeNs),Kind::Start);
    } else if constexpr(std::is_same_v<T,Coin>) {
        CLOCK(mBirthTimeNs,Start); CLOCK(mFadeEndTimeNs,End); CLOCK(mDisappearStartTimeNs,DormantStart);
    } else if constexpr(std::is_same_v<T,LawnMower>) {
        CLOCK(mChompEndTimeNs,End); CLOCK(mSquishEndTimeNs,End); CLOCK(mRollingInStartTimeNs,Start);
    } else if constexpr(std::is_same_v<T,GridItem>) {
        const bool age=v.mGridItemType==GridItemType::GRIDITEM_GRAVESTONE ||
            v.mGridItemType==GridItemType::GRIDITEM_BRAIN;
        f(offsetof(T,mStateStartTimeNs),v.mGridItemType==GridItemType::GRIDITEM_IZOMBIE_BRAIN && v.mGridItemState!=GridItemState::GRIDITEM_STATE_BRAIN_SQUISHED?Kind::Health:age?Kind::Start:Kind::End);
        CLOCK(mTransparencyTimeNs,Transparency);
        if(v.mGridItemType==GridItemType::GRIDITEM_BRAIN) CLOCK(mRootPeriodNs,Start);
    } else if constexpr(std::is_same_v<T,SeedPacket>) {
        CLOCK(mRefreshStartTimeNs,Refresh); CLOCK(mSlotMachineEndTimeNs,SlotEnd);
    } else if constexpr(std::is_same_v<T,SeedBank>) {
        for(std::size_t i=0;i<10;++i) Fields(v.mSeedPackets[i],[&](std::size_t p,Kind k){f(offsetof(T,mSeedPackets)+i*sizeof(SeedPacket)+p,k);});
    } else if constexpr(std::is_same_v<T,MessageWidget>) {CLOCK(mEndTimeNs,InfiniteEnd);}
    else if constexpr(std::is_same_v<T,Music>) {
        CLOCK(mBurstEndTimeNs,End); CLOCK(mDrumsEndTimeNs,End); CLOCK(mFadeOutEndTimeNs,End); CLOCK(mFadeOutDurationNs,Raw);
    }
#undef CLOCK
}
template<class T> Kind FieldKind(const T& v,std::size_t p) {
    Kind result=Kind::None; Fields(v,[&](std::size_t n,Kind k){if(n==p)result=k;}); return result;
}
template<class T> std::int64_t Now(const T& v,std::size_t p) {
    const auto visual=[&]{return v.mApp && v.mApp->mEffectSystem ? v.mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs : std::int64_t{0};};
    if constexpr(std::is_same_v<T,MessageWidget>) return v.mMessageStyle==MessageStyle::MESSAGE_STYLE_HOUSE_NAME ? visual() : v.mApp && v.mApp->mBoard ? static_cast<std::int64_t>(v.mApp->mBoard->mLevelTimeNs) : 0;
    else if constexpr(std::is_same_v<T,Music>) return v.mApp?v.mApp->mApplicationTimeNs:0;
    else if constexpr(std::is_same_v<T,Coin>) return visual();
    else if constexpr(std::is_same_v<T,Zombie>) return v.mFromWave==Zombie::ZOMBIE_WAVE_CUTSCENE || v.mFromWave==Zombie::ZOMBIE_WAVE_UI ? visual() : v.mBoard?static_cast<std::int64_t>(v.mBoard->mLevelTimeNs):visual();
    else if constexpr(std::is_same_v<T,LawnMower>) return p==offsetof(T,mRollingInStartTimeNs)?visual():v.mBoard?static_cast<std::int64_t>(v.mBoard->mLevelTimeNs):0;
    else if constexpr(std::is_same_v<T,GridItem>) return v.mGridItemType==GridItemType::GRIDITEM_STINKY?visual():v.mBoard?static_cast<std::int64_t>(v.mBoard->mLevelTimeNs):0;
    else return v.mBoard?static_cast<std::int64_t>(v.mBoard->mLevelTimeNs):visual();
}
template<class T> std::int64_t EncodeAt(const T& v,std::size_t p,std::int64_t raw) {
    const auto k=FieldKind(v,p);
    const auto now=Now(v,p);
    if(k==Kind::None||k==Kind::Raw)return raw;
    if constexpr(std::is_same_v<T,GridItem>) if(k==Kind::Health)return static_cast<std::int64_t>(v.mBrainHealth)*10'000'000;
    if constexpr(std::is_same_v<T,Plant>) if(k==Kind::Disappear && raw==0)return 2'000'000'000;
    if constexpr(std::is_same_v<T,Zombie>) {
        if(p==offsetof(T,mYuckyFaceStartTimeNs)&&!v.mYuckyFace)return 0;
        if(p==offsetof(T,mVelocityX)+offsetof(TimedLinearMotion,startTimeNs)&&!v.mVelocityX.bound)return 0;
        if(p==offsetof(T,mVelocityY)+offsetof(TimedLinearMotion,startTimeNs)&&!v.mVelocityY.bound)return 0;
        if(p==offsetof(T,mAltitudeMotion)+offsetof(TimedLinearMotion,startTimeNs)&&!v.mAltitudeMotion.bound)return 0;
    }
    if constexpr(std::is_same_v<T,SeedPacket>) if(k==Kind::Refresh&&!v.mRefreshing)return 0;
    if constexpr(std::is_same_v<T,SeedBank>) if(k==Kind::Refresh && !v.mSeedPackets[(p-offsetof(T,mSeedPackets))/sizeof(SeedPacket)].mRefreshing)return 0;
    if constexpr(std::is_same_v<T,GridItem>) if(k==Kind::Transparency) return v.mTransparencyIncreasing
        ?std::clamp<std::int64_t>(ReanimClock::Subtract(now,raw),0,500'000'000):std::max<std::int64_t>(0,ReanimClock::Subtract(raw,now));
    if(k==Kind::Start||k==Kind::Refresh||k==Kind::DormantStart) return k==Kind::DormantStart&&raw==INT64_MAX?0:ReanimClock::Subtract(now,raw);
    if(k==Kind::Fade&&raw==-1)return -10'000'000;
    if(k==Kind::InfiniteEnd&&raw==INT64_MAX)return 100'000'000'000LL;
    // Preserve the established remaining-time wire field. A one-ns sentinel
    // keeps expired attacks active while awaiting animation completion. It is
    // below every shot event offset; elapsed waiting age is never consumed.
    if(k==Kind::ShootingEnd)return raw==0?0:std::max<std::int64_t>(1,ReanimClock::Subtract(raw,now));
    if(k==Kind::SignedEnd)return ReanimClock::Subtract(raw,now);
    // Zero is inactive. Preserve signed remaining time for an expired slot
    // that is still finishing its last turn. INT64_MIN encodes active exactly
    // at expiry, distinct from inactive; no runtime member or extra wire byte.
    if(k==Kind::SlotEnd)
    {
        if(raw==0)return 0;
        const auto remaining=ReanimClock::Subtract(raw,now);
        return remaining==0?INT64_MIN:remaining;
    }
    return raw==0?0:std::max<std::int64_t>(0,ReanimClock::Subtract(raw,now));
}
template<class T> std::int64_t DecodeAt(const T& v,std::size_t p,std::int64_t wire) {
    const auto k=FieldKind(v,p);
    if(k==Kind::Health)return wire/10'000'000;
    if(k==Kind::Start||k==Kind::Refresh)return ReanimClock::Subtract(0,wire);
    if(k==Kind::DormantStart)return wire==0?INT64_MAX:ReanimClock::Subtract(0,wire);
    if(k==Kind::Fade&&wire<0)return -1;
    if(k==Kind::InfiniteEnd&&wire>=100'000'000'000LL)return INT64_MAX;
    return wire;
}
template<class T> void BindAt(T& v) {
    Fields(v,[&](std::size_t p,Kind k){
        if(k==Kind::None||k==Kind::Raw||k==Kind::Health)return;
        std::int64_t raw;std::memcpy(&raw,reinterpret_cast<unsigned char*>(&v)+p,8);
        if((k==Kind::End||k==Kind::ShootingEnd||k==Kind::SlotEnd||k==Kind::Disappear||k==Kind::Transparency)&&raw==0)return;
        if(k==Kind::Fade&&raw==-1)return;
        if(k==Kind::SlotEnd&&raw==INT64_MIN)
        {
            raw=Now(v,p);
            std::memcpy(reinterpret_cast<unsigned char*>(&v)+p,&raw,8);
            return;
        }
        if((k==Kind::InfiniteEnd||k==Kind::DormantStart)&&raw==INT64_MAX)return;
        raw=ReanimClock::Add(raw,Now(v,p));std::memcpy(reinterpret_cast<unsigned char*>(&v)+p,&raw,8);
    });
    if constexpr(std::is_same_v<T,Music>)v.mLastUpdateTimeNs=Now(v,0);
    if constexpr(std::is_same_v<T,GridItem>)v.mTransparencyIncreasing=false;
    if constexpr(std::is_same_v<T,Plant>) if(v.mState!=PlantState::STATE_DOINGSPECIAL&&!v.mSquished)v.mDisappearEndTimeNs=0;
}
}
bool IsRawTime(const Plant& v,std::size_t p) {return FieldKind(v,p)!=Kind::None;}
std::int64_t Encode(const Plant& v,std::size_t p,std::int64_t raw) {return EncodeAt(v,p,raw);}
std::int64_t Decode(const Plant& v,std::size_t p,std::int64_t wire) {return DecodeAt(v,p,wire);}
void Bind(Plant& v) {BindAt(v);}
bool IsRawTime(const Zombie& v,std::size_t p) {return FieldKind(v,p)!=Kind::None;}
std::int64_t Encode(const Zombie& v,std::size_t p,std::int64_t raw) {return EncodeAt(v,p,raw);}
std::int64_t Decode(const Zombie& v,std::size_t p,std::int64_t wire) {return DecodeAt(v,p,wire);}
void Bind(Zombie& v) {BindAt(v);}
bool IsRawTime(const Coin& v,std::size_t p) {return FieldKind(v,p)!=Kind::None;}
std::int64_t Encode(const Coin& v,std::size_t p,std::int64_t raw) {return EncodeAt(v,p,raw);}
std::int64_t Decode(const Coin& v,std::size_t p,std::int64_t wire) {return DecodeAt(v,p,wire);}
void Bind(Coin& v) {BindAt(v);}
bool IsRawTime(const LawnMower& v,std::size_t p) {return FieldKind(v,p)!=Kind::None;}
std::int64_t Encode(const LawnMower& v,std::size_t p,std::int64_t raw) {return EncodeAt(v,p,raw);}
std::int64_t Decode(const LawnMower& v,std::size_t p,std::int64_t wire) {return DecodeAt(v,p,wire);}
void Bind(LawnMower& v) {BindAt(v);}
bool IsRawTime(const GridItem& v,std::size_t p) {return FieldKind(v,p)!=Kind::None;}
std::int64_t Encode(const GridItem& v,std::size_t p,std::int64_t raw) {return EncodeAt(v,p,raw);}
std::int64_t Decode(const GridItem& v,std::size_t p,std::int64_t wire) {return DecodeAt(v,p,wire);}
void Bind(GridItem& v) {BindAt(v);}
bool IsRawTime(const SeedPacket& v,std::size_t p) {return FieldKind(v,p)!=Kind::None;}
std::int64_t Encode(const SeedPacket& v,std::size_t p,std::int64_t raw) {return EncodeAt(v,p,raw);}
std::int64_t Decode(const SeedPacket& v,std::size_t p,std::int64_t wire) {return DecodeAt(v,p,wire);}
void Bind(SeedPacket& v) {BindAt(v);}
bool IsRawTime(const SeedBank& v,std::size_t p) {return FieldKind(v,p)!=Kind::None;}
std::int64_t Encode(const SeedBank& v,std::size_t p,std::int64_t raw) {return EncodeAt(v,p,raw);}
std::int64_t Decode(const SeedBank& v,std::size_t p,std::int64_t wire) {return DecodeAt(v,p,wire);}
void Bind(SeedBank& v) {BindAt(v);}
bool IsRawTime(const MessageWidget& v,std::size_t p) {return FieldKind(v,p)!=Kind::None;}
std::int64_t Encode(const MessageWidget& v,std::size_t p,std::int64_t raw) {return EncodeAt(v,p,raw);}
std::int64_t Decode(const MessageWidget& v,std::size_t p,std::int64_t wire) {return DecodeAt(v,p,wire);}
void Bind(MessageWidget& v) {BindAt(v);}
bool IsRawTime(const Music& v,std::size_t p) {return FieldKind(v,p)!=Kind::None;}
std::int64_t Encode(const Music& v,std::size_t p,std::int64_t raw) {return EncodeAt(v,p,raw);}
std::int64_t Decode(const Music& v,std::size_t p,std::int64_t wire) {return DecodeAt(v,p,wire);}
void Bind(Music& v) {BindAt(v);}
}
