#include "Plant.h"
#include "Board.h"
#include "Zombie.h"
#include "Motion.h"
#include "CatapultState.h"
#include "ZombieFlags.h"
#include "Cutscene.h"
#include "GridItem.h"
#include "LawnMower.h"
#include "Challenge.h"
#include "Projectile.h"
#include "LawnApp.h"
#include "Resources.h"
#include "System/Music.h"
#include "Widget/Almanac.h"
#include "Widget/ZombatarItems.h"
#include "Sexy.TodLib/TodFoley.h"
#include "Sexy.TodLib/TodDebug.h"
#include "Sexy.TodLib/TodCommon.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/generated/ReanimVariantOptions.h"
#include "Sexy.TodLib/generated/ParticleVariantId.h"
#include "Sexy.TodLib/Attachment.h"
#include "Sexy.TodLib/TodParticle.h"
#include "Sexy.TodLib/EffectSystem.h"
#include <array>
#include <stdexcept>

static ZombieDefinition gZombieDefs[NUM_ZOMBIE_TYPES] = {  //0x69DA80
    { ZOMBIE_NORMAL,            REANIM_ZOMBIE,              1,      1,      1,      4000 },
    { ZOMBIE_FLAG,              REANIM_ZOMBIE,              1,      1,      1,      0 },
    { ZOMBIE_TRAFFIC_CONE,      REANIM_ZOMBIE,              2,      3,      1,      4000 },
    { ZOMBIE_POLEVAULTER,       REANIM_POLEVAULTER,         2,      6,      5,      2000 },
    { ZOMBIE_PAIL,              REANIM_ZOMBIE,              4,      8,      1,      3000 },
    { ZOMBIE_NEWSPAPER,         REANIM_ZOMBIE_NEWSPAPER,    2,      11,     1,      1000 },
    { ZOMBIE_DOOR,              REANIM_ZOMBIE,              4,      13,     5,      3500 },
    { ZOMBIE_FOOTBALL,          REANIM_ZOMBIE_FOOTBALL,     7,      16,     5,      2000 },
    { ZOMBIE_DANCER,            REANIM_DANCER,              5,      18,     5,      1000 },
    { ZOMBIE_BACKUP_DANCER,     REANIM_BACKUP_DANCER,       1,      18,     1,      0 },
    { ZOMBIE_DUCKY_TUBE,        REANIM_ZOMBIE,              1,      21,     5,      0 },
    { ZOMBIE_SNORKEL,           REANIM_SNORKEL,             3,      23,     10,     2000 },
    { ZOMBIE_ZAMBONI,           REANIM_ZOMBIE_ZAMBONI,      7,      26,     10,     2000 },
    { ZOMBIE_BOBSLED,           REANIM_BOBSLED,             3,      26,     10,     2000 },
    { ZOMBIE_DOLPHIN_RIDER,     REANIM_ZOMBIE_DOLPHINRIDER, 3,      28,     10,     1500 },
    { ZOMBIE_JACK_IN_THE_BOX,   REANIM_JACKINTHEBOX,        3,      31,     10,     1000 },
    { ZOMBIE_BALLOON,           REANIM_BALLOON,             2,      33,     10,     2000 },
    { ZOMBIE_DIGGER,            REANIM_DIGGER,              4,      36,     10,     1000 },
    { ZOMBIE_POGO,              REANIM_POGO,                4,      38,     10,     1000 },
    { ZOMBIE_YETI,              REANIM_YETI,                4,      40,     1,      1 },
    { ZOMBIE_BUNGEE,            REANIM_BUNGEE,              3,      41,     10,     1000 },
    { ZOMBIE_LADDER,            REANIM_LADDER,              4,      43,     10,     1000 },
    { ZOMBIE_CATAPULT,          REANIM_CATAPULT,            5,      46,     10,     1500 },
    { ZOMBIE_GARGANTUAR,        REANIM_GARGANTUAR,          10,     48,     15,     1500 },
    { ZOMBIE_IMP,               REANIM_IMP,                 10,     48,     1,      0 },
    { ZOMBIE_BOSS,              REANIM_BOSS,                10,     50,     1,      0 },
    { ZOMBIE_PEA_HEAD,          REANIM_ZOMBIE,              1,      99,     1,      4000 },
    { ZOMBIE_WALLNUT_HEAD,      REANIM_ZOMBIE,              4,      99,     1,      3000 },
    { ZOMBIE_JALAPENO_HEAD,     REANIM_ZOMBIE,              3,      99,     10,     1000 },
    { ZOMBIE_GATLING_HEAD,      REANIM_ZOMBIE,              3,      99,     10,     2000 },
    { ZOMBIE_SQUASH_HEAD,       REANIM_ZOMBIE,              3,      99,     10,     2000 },
    { ZOMBIE_TALLNUT_HEAD,      REANIM_ZOMBIE,              4,      99,     10,     2000 },
    { ZOMBIE_REDEYE_GARGANTUAR, REANIM_GARGANTUAR,          10,     48,     15,     6000 },
    { ZOMBIE_ZOMBATAR,          REANIM_ZOMBIE,              1,      1,      1,      0 },
};

static ZombieType gBossZombieList[] = {  //0x69DE1C
    ZombieType::ZOMBIE_TRAFFIC_CONE,
    ZombieType::ZOMBIE_PAIL,
    ZombieType::ZOMBIE_FOOTBALL,
    ZombieType::ZOMBIE_POLEVAULTER,
    ZombieType::ZOMBIE_JACK_IN_THE_BOX,
    ZombieType::ZOMBIE_LADDER,
    ZombieType::ZOMBIE_ZAMBONI,
    ZombieType::ZOMBIE_CATAPULT,
    ZombieType::ZOMBIE_POGO,
    ZombieType::ZOMBIE_NEWSPAPER,
    ZombieType::ZOMBIE_DOOR,
    ZombieType::ZOMBIE_GARGANTUAR
};

ZombieDefinition& GetZombieDefinition(ZombieType theZombieType)
{
    TOD_ASSERT(theZombieType >= 0 && theZombieType < NUM_ZOMBIE_TYPES);
    TOD_ASSERT(gZombieDefs[theZombieType].mZombieType == theZombieType);

    return gZombieDefs[theZombieType];
}

namespace
{
bool ZombieAgeCrossedPeriod(std::int64_t ageNs, std::uint64_t deltaNs, std::int64_t periodNs)
{
    const auto previous = std::max<std::int64_t>(0, ageNs-static_cast<std::int64_t>(deltaNs));
    return ageNs == 0 || ageNs / periodNs != previous / periodNs;
}
bool ZombieAgeCrossedPoint(std::int64_t ageNs, std::uint64_t deltaNs, std::int64_t pointNs)
{
    return ageNs == pointNs || (ageNs > pointNs && ageNs-static_cast<std::int64_t>(deltaNs) < pointNs);
}
}

//0x522580
// GOTY @Patoke: 0x5329A0
void Zombie::ZombieInitialize(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theRow, ZombieType theType, bool theVariant, Zombie* theParentZombie, int theFromWave)
{
    // ZombieInitialize 槽位上下文：switch 外提局部（aRenderLayer/aRenderOffset）
    // 与跨段参数（定义动画型、父僵尸、来波号）经此传递。表内匿名 lambda = 原
    // case 体逐句迁移（生成器 + 逆变换 token 对拍 0 差异），1073 地址注释随体
    // 保留。尾部公共逻辑具名化（ZombieInitializeTail），各槽位结尾直呼（尾部固定
    // 无需回调传参）——return 原样即跳过直呼，BUNGEE 选靶自毙语义零改写。
    struct ZombieInitLocals
    {
    RenderLayer mRenderLayer;
    int mRenderOffset;
    ReanimationType mBodyReanimType;
    Zombie* mParentZombie;
    int mFromWave;
    };
    using ZombieInitCaseFn = void (*)(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie&, ZombieInitLocals&);

    // 尾部公共逻辑（原 switch 后至函数尾逐句迁移）：各槽位结尾直呼——尾部固定，
    // 无需回调传参；case 内 return 原样保留——不调即跳过尾部（=原 return 跳过
    // 的同一段）；哨兵/越界由分发点 else 直调。置于共体函数与表之前（引用者
    // 须后置）。
    static constexpr auto ZombieInitializeTail = [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {


    if (z.IsOnBoard() && z.mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM)
    {
    float aAnimRate = RandRangeFloat(8.0f, 10.0f);
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_aquarium_swim, ReanimLoopType::REANIM_LOOP, 0, aAnimRate);

    z.mZombieHeight = ZombieHeight::HEIGHT_ZOMBIQUARIUM;
    z.mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_DRIFT;
    z.mPhaseCounter = 200;
    z.mBodyHealth = 200;
    z.mSummonCounter = RandRangeInt(200, 400);
    }

    if (z.mApp->IsLittleTroubleLevel() && (z.IsOnBoard() || lc.mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE))
    {
    z.mScaleZombie = 0.5f;
    z.mBodyHealth /= 4;
    z.mHelmHealth /= 4;
    z.mShieldHealth /= 4;
    z.mFlyingHealth /= 4;
    }

    z.UpdateAnimSpeed(ownerNowNs);
    if (z.mVariant)
    {
    z.ReanimShowRange(ReanimTrackRanges::anim_tongue, RENDER_GROUP_NORMAL);
    }

    z.mHelmMaxHealth = z.mHelmHealth;
    z.mBodyMaxHealth = z.mBodyHealth;
    z.mShieldMaxHealth = z.mShieldHealth;
    z.mFlyingMaxHealth = z.mFlyingHealth;
    z.mDead = false;


    z.mRenderOrder = Board::MakeRenderOrder(lc.mRenderLayer, z.mRow, lc.mRenderOffset);
    if (z.mZombieHeight == ZombieHeight::HEIGHT_ZOMBIQUARIUM)
    {
    z.mBodyMaxHealth = 300;
    }

    if (z.IsOnBoard())
    {
    z.PlayZombieAppearSound();
    z.StartZombieSound();
    }

    z.UpdateReanim(ownerNowNs, visualNowNs, animationNowNs);

    };

    // 标签共体（匿名 lambda 无法跨槽复用，复用即具名）
    // 原 case ZOMBIE_NORMAL, ZOMBIE_DUCKY_TUBE
    static constexpr auto ZombieInitNormal = [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.LoadPlainZombieReanim();
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    };

    // 标签共体（匿名 lambda 无法跨槽复用，复用即具名）
    // 原 case ZOMBIE_GARGANTUAR, ZOMBIE_REDEYE_GARGANTUAR
    static constexpr auto ZombieInitGargantuar = [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mWidth = 180;
    z.mHeight = 180;
    z.mBodyHealth = 3000;
    z.mAnimFrames = 24;
    z.mAnimTicksPerFrame = 8;
    { z.mX = (WIDE_BOARD_WIDTH + 45 + Rand(10)); z.mRootBindingGeneration = 0; }
    z.mZombieRect = Rect(-17, -38, 125, 154);
    z.mZombieAttackRect = Rect(-30, -38, 89, 154);
    z.mVariant = false;
    lc.mRenderOffset = 8;
    z.mHasObject = true;

    int aPoleHit = Rand(100);
    int aPoleVariant;
    if (!z.IsOnBoard() || z.mBoard->mLevel == 48)
    {
    aPoleVariant = 0;
    }
    else
    {
    aPoleVariant = aPoleHit < 10 ? 2 : aPoleHit < 35 ? 1 : 0;
    }

    Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
    if (aPoleVariant == 2)
    {
    aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_gargantuar_telephonepole, ReanimOptions::Zombie_gargantuar::Zombie_gargantuar_telephonepole::ZOMBIE_GARGANTUAR_ZOMBIE);
    }
    else if (aPoleVariant == 1)
    {
    aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_gargantuar_telephonepole, ReanimOptions::Zombie_gargantuar::Zombie_gargantuar_telephonepole::ZOMBIE_GARGANTUAR_DUCKXING);
    }

    if (z.mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
    {
    aBodyReanim->SetVariantOption(ReanimTrackId::anim_head1, ReanimOptions::Zombie_gargantuar::anim_head1::ZOMBIE_GARGANTUAR_HEAD_REDEYE);
    z.mBodyHealth = 6000;
    }
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    };

    // 匿名 lambda 函数指针表，行序 = ZombieType 枚举值序（0..33）；
    // 哨兵（CACHED/NUM/INVALID）在表外，由分发守卫拦下 = 原 default 无操作，
    // 尾部仍由 else 分支直调。
    static constexpr ZombieInitCaseFn kZombieInitCases[(int)ZombieType::NUM_ZOMBIE_TYPES] = {
    ZombieInitNormal,  // 0 ZOMBIE_NORMAL（共体）
    // 原 case ZOMBIE_FLAG
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mHasObject = true;
    z.LoadPlainZombieReanim();

    Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
    Reanimation* aFlagReanim = z.mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_FLAG, animationNowNs);
    aFlagReanim->PlayReanim(ReanimTrackId::Zombie_flag, ReanimLoopType::REANIM_LOOP, 0, 15.0f);
    z.mSpecialHeadReanimID = z.mApp->ReanimationGetID(aFlagReanim);
    ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName(ReanimTrackId::Zombie_flaghand);
    AttachReanim(aTrackInstance->mAttachmentID, aFlagReanim, 0.0f, 0.0f);
    aBodyReanim->mFrameBasePose = 0;

    { z.mX = (WIDE_BOARD_WIDTH); z.mRootBindingGeneration = 0; }
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 1 ZOMBIE_FLAG
    // 原 case ZOMBIE_TRAFFIC_CONE
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.LoadPlainZombieReanim();
    z.ReanimShowRange(ReanimTrackRanges::anim_cone, RENDER_GROUP_NORMAL);
    z.ReanimShowRange(ReanimTrackRanges::anim_hair, RENDER_GROUP_HIDDEN);
    z.mHelmType = HelmType::HELMTYPE_TRAFFIC_CONE;
    z.mHelmHealth = 370;
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 2 ZOMBIE_TRAFFIC_CONE
    // 原 case ZOMBIE_POLEVAULTER
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mBodyHealth = 500;
    z.mAnimTicksPerFrame = 6;
    z.mZombiePhase = ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT;
    z.mHasObject = true;
    z.mVariant = false;
    { z.mX = (WIDE_BOARD_WIDTH + 70 + Rand(10)); z.mRootBindingGeneration = 0; }
    if (z.IsOnBoard())
    {
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_run, ReanimLoopType::REANIM_LOOP, 0, 0.0f);
    z.PickRandomSpeed(ownerNowNs);
    }
    if (z.mApp->IsWallnutBowlingLevel())
    {
    z.mZombieAttackRect = Rect(-229, 0, 270, 115);
    }
    else
    {
    z.mZombieAttackRect = Rect(-29, 0, 70, 115);
    }
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 3 ZOMBIE_POLEVAULTER
    // 原 case ZOMBIE_PAIL
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.LoadPlainZombieReanim();
    z.ReanimShowRange(ReanimTrackRanges::anim_bucket, RENDER_GROUP_NORMAL);
    z.ReanimShowRange(ReanimTrackRanges::anim_hair, RENDER_GROUP_HIDDEN);
    z.mHelmType = HelmType::HELMTYPE_PAIL;
    z.mHelmHealth = 1100;
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 4 ZOMBIE_PAIL
    // 原 case ZOMBIE_NEWSPAPER
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mZombieAttackRect = Rect(20, 0, 50, 115);
    z.mZombiePhase = ZombiePhase::PHASE_NEWSPAPER_READING;
    z.mShieldType = ShieldType::SHIELDTYPE_NEWSPAPER;
    z.mShieldHealth = 150;
    z.mVariant = false;
    z.AttachShield();
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 5 ZOMBIE_NEWSPAPER
    // 原 case ZOMBIE_DOOR
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mShieldType = ShieldType::SHIELDTYPE_DOOR;
    z.mShieldHealth = 1100;
    z.LoadPlainZombieReanim();
    z.AttachShield();
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 6 ZOMBIE_DOOR
    // 原 case ZOMBIE_FOOTBALL
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mZombieRect = Rect(50, 0, 57, 115);
    z.ReanimShowRange(ReanimTrackRanges::anim_hair, RENDER_GROUP_HIDDEN);
    z.mHelmType = HelmType::HELMTYPE_FOOTBALL;
    z.mHelmHealth = 1400;
    z.mAnimTicksPerFrame = 6;
    z.mVariant = false;
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 7 ZOMBIE_FOOTBALL
    // 原 case ZOMBIE_DANCER
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    // @Patoke: add scaling for new assets
    z.mScaleZombie = 0.8f;
    if (!z.IsOnBoard())
    {
    // @Patoke: oops
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_armraise, ReanimLoopType::REANIM_LOOP, 0, 12.0f);
    }
    else
    {
    z.mZombiePhase = ZombiePhase::PHASE_DANCER_DANCING_IN;
    z.mVelX = 0.5f;
    z.mPhaseCounter = 300 + Rand(12);
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_moonwalk, ReanimLoopType::REANIM_LOOP, 0, 24.0f);
    }
    z.mBodyHealth = 500;
    z.mVariant = false;
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 8 ZOMBIE_DANCER
    // 原 case ZOMBIE_BACKUP_DANCER
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    // @Patoke: add scaling for new assets
    z.mScaleZombie = 0.8f;
    if (!z.IsOnBoard())
    {
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_armraise, ReanimLoopType::REANIM_LOOP, 0, 12.0f);
    }
    z.mZombiePhase = ZombiePhase::PHASE_DANCER_DANCING_LEFT;
    z.mVariant = false;
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 9 ZOMBIE_BACKUP_DANCER
    ZombieInitNormal,  // 10 ZOMBIE_DUCKY_TUBE（共体）
    // 原 case ZOMBIE_SNORKEL
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mZombieRect = Rect(12, 0, 62, 115);
    z.mZombieAttackRect = Rect(-5, 0, 55, 115);
    z.SetupWaterTrack(ReanimTrackId::Zombie_snorkle_whitewater);
    z.SetupWaterTrack(ReanimTrackId::Zombie_snorkle_whitewater2);
    z.mVariant = false;
    z.mZombiePhase = ZombiePhase::PHASE_SNORKEL_WALKING;
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 11 ZOMBIE_SNORKEL
    // 原 case ZOMBIE_ZAMBONI
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mBodyHealth = 1350;
    z.mAnimFrames = 2;
    z.mAnimTicksPerFrame = 8;
    { z.mX = (WIDE_BOARD_WIDTH + Rand(10)); z.mRootBindingGeneration = 0; }
    lc.mRenderOffset = 8;
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_drive, ReanimLoopType::REANIM_LOOP, 0, 12.0f);
    z.mZombieRect = Rect(0, -13, 153, 140);
    z.mZombieAttackRect = Rect(10, -13, 133, 140);
    z.mVariant = false;
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 12 ZOMBIE_ZAMBONI
    // 原 case ZOMBIE_BOBSLED
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    lc.mRenderOffset = 3;

    if (lc.mParentZombie)
    {
    int aPosition = 0;
    while (aPosition < NUM_BOBSLED_FOLLOWERS && lc.mParentZombie->mFollowerZombieID[aPosition] != ZombieID::ZOMBIEID_NULL)
    {
    aPosition++;
    }
    TOD_ASSERT(aPosition < 3);
    lc.mParentZombie->mFollowerZombieID[aPosition] = z.mBoard->ZombieGetID(&z);
    z.mRelatedZombieID = z.mBoard->ZombieGetID(lc.mParentZombie);

    { z.mX = ((lc.mParentZombie->mX) + (aPosition + 1) * 50); z.mRootBindingGeneration = 0; }
    if (aPosition == 0)
    {
    lc.mRenderOffset = 1;
    z.mAltitude = 9.0f;
    }
    else if (aPosition == 1)
    {
    lc.mRenderOffset = 2;
    z.mAltitude = -7.0f;
    }
    else
    {
    lc.mRenderOffset = 0;
    z.mAltitude = 9.0f;
    }
    }
    else
    {
    { z.mX = (WIDE_BOARD_WIDTH + 80); z.mRootBindingGeneration = 0; }
    z.mZombieRect = Rect(-50, 0, 275, 115);
    z.mHelmType = HelmType::HELMTYPE_BOBSLED;
    z.mHelmHealth = 300;
    z.mAltitude = -10.0f;
    }

    z.mVelX = 0.6f;
    z.mZombiePhase = ZombiePhase::PHASE_BOBSLED_SLIDING;
    z.mPhaseCounter = 500;
    z.mVariant = false;

    if (z.mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE)
    {
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_jump, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 20.0f);
    z.mApp->ReanimationGet(z.mBodyReanimID)->SeekRatio(1.0f);
    z.mAltitude = 18.0f;
    }
    else if (z.IsOnBoard())
    {
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_push, ReanimLoopType::REANIM_LOOP, 0, 30.0f);
    }
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 13 ZOMBIE_BOBSLED
    // 原 case ZOMBIE_DOLPHIN_RIDER
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mBodyHealth = 500;
    z.mAnimTicksPerFrame = 6;
    z.mZombiePhase = ZombiePhase::PHASE_DOLPHIN_WALKING;
    z.mVariant = false;
    if (z.IsOnBoard())
    {
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_walkdolphin, ReanimLoopType::REANIM_LOOP, 0, 0.0f);
    z.PickRandomSpeed(ownerNowNs);
    }
    z.SetupWaterTrack(ReanimTrackId::Zombie_dolphinrider_whitewater);
    z.SetupWaterTrack(ReanimTrackId::Zombie_dolphinrider_dolphininwater);
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 14 ZOMBIE_DOLPHIN_RIDER
    // 原 case ZOMBIE_JACK_IN_THE_BOX
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mBodyHealth = 500;
    z.mAnimTicksPerFrame = 6;

    int aDistance = 450 + Rand(300);
    if (Rand(20) == 0)  // 早爆的概率
    {
    aDistance /= 3;
    }
    z.mPhaseCounter = (int)(aDistance / z.mVelX) * ZOMBIE_LIMP_SPEED_FACTOR;
    z.mZombieAttackRect = Rect(20, 0, 50, 115);

    if (z.mApp->IsScaryPotterLevel())
    {
    z.mPhaseCounter = 10;
    }
    if (z.IsOnBoard())
    {
    z.mZombiePhase = ZombiePhase::PHASE_JACK_IN_THE_BOX_RUNNING;
    }
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 15 ZOMBIE_JACK_IN_THE_BOX
    // 原 case ZOMBIE_BALLOON
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
    aBodyReanim->SetTruncateDisappearingFramesAll( false);

    if (z.IsOnBoard())
    {
    z.mAltitude = 25.0f;
    z.mZombiePhase = ZombiePhase::PHASE_BALLOON_FLYING;
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 0, aBodyReanim->mAnimRate);
    }
    else
    {
    float aAnimRate = RandRangeFloat(8.0f, 10.0f);
    z.SetAnimRate(ownerNowNs, aAnimRate);
    }

    Reanimation* aPropellerReanim = z.mApp->AddReanimation(0.0f, 0.0f, 0, lc.mBodyReanimType, animationNowNs);
    aPropellerReanim->SetFramesForLayer(ReanimTrackId::propeller);
    aPropellerReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME);
    aPropellerReanim->AttachToAnotherReanimation(aBodyReanim, ReanimTrackId::hat);

    z.mFlyingHealth = 20;
    z.mZombieRect = Rect(36, 30, 42, 115);
    z.mZombieAttackRect = Rect(20, 30, 50, 115);
    z.mVariant = false;
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 16 ZOMBIE_BALLOON
    // 原 case ZOMBIE_DIGGER
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mHelmType = HelmType::HELMTYPE_DIGGER;
    z.mHelmHealth = 100;
    z.mVariant = false;
    z.mHasObject = true;
    z.mZombieRect = Rect(50, 0, 28, 115);

    Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
    aBodyReanim->SetTruncateDisappearingFramesAll( false);

    if (!z.IsOnBoard())
    {
    z.mZombiePhase = ZombiePhase::PHASE_DIGGER_CUTSCENE;
    }
    else
    {
    z.mZombiePhase = ZombiePhase::PHASE_DIGGER_TUNNELING;
    z.AddAttachedParticle(ownerNowNs, 60, 100, ParticleEffect::PARTICLE_DIGGER_TUNNEL);
    lc.mRenderOffset = 7;
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_dig, ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME, 0, 12.0f);
    z.PickRandomSpeed(ownerNowNs);
    }
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 17 ZOMBIE_DIGGER
    // 原 case ZOMBIE_POGO
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mVariant = false;
    z.mZombiePhase = ZombiePhase::PHASE_POGO_BOUNCING;
    z.mPhaseCounter = Rand(POGO_BOUNCE_TIME) + 1;
    z.mHasObject = true;
    z.mBodyHealth = 500;
    z.mZombieAttackRect = Rect(10, 0, 30, 115);
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_pogo, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 40.0f);
    z.mApp->ReanimationGet(z.mBodyReanimID)->SeekRatio(1.0f);
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 18 ZOMBIE_POGO
    // 原 case ZOMBIE_YETI
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mBodyHealth = 1350;
    z.mPhaseCounter = RandRangeInt(1500, 2000);
    z.mHasObject = true;
    z.mZombieAttackRect = Rect(20, 0, 50, 115);
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 19 ZOMBIE_YETI
    // 原 case ZOMBIE_BUNGEE
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mBodyHealth = 450;
    z.mAnimFrames = 4;
    z.mAltitude = BUNGEE_ZOMBIE_HEIGHT + RandRangeInt(0, 150);
    z.mVelX = 0.0f;

    if (z.IsOnBoard())
    {
    z.PickBungeeZombieTarget(ownerNowNs, visualNowNs, animationNowNs, -1);

    if (z.mDead)
    {
    return;
    }

    z.mZombiePhase = ZombiePhase::PHASE_BUNGEE_DIVING;
    }
    else
    {
    z.mZombiePhase = ZombiePhase::PHASE_BUNGEE_CUTSCENE;
    z.mPhaseCounter = RandRangeInt(0, 200);
    }

    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_drop, ReanimLoopType::REANIM_LOOP, 0, 24.0f);
    Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
    aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Zombie_bungi_rightarm_lower2, RENDER_GROUP_ARMS);
    aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Zombie_bungi_rightarm_hand2, RENDER_GROUP_ARMS);
    aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Zombie_bungi_leftarm_lower2, RENDER_GROUP_ARMS);
    aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Zombie_bungi_leftarm_hand2, RENDER_GROUP_ARMS);
    aBodyReanim->SetTruncateDisappearingFramesAll( false);

    lc.mRenderLayer = RenderLayer::RENDER_LAYER_GRAVE_STONE;
    lc.mRenderOffset = 7;
    z.mZombieRect = Rect(-20, 22, 110, 94);
    z.mZombieAttackRect = Rect(0, 0, 0, 0);
    z.mVariant = false;
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 20 ZOMBIE_BUNGEE
    // 原 case ZOMBIE_LADDER
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mBodyHealth = 500;
    z.mShieldHealth = 500;
    z.mShieldType = ShieldType::SHIELDTYPE_LADDER;
    z.mZombieAttackRect = Rect(10, 0, 50, 115);
    if (z.IsOnBoard())
    {
    z.mZombiePhase = ZombiePhase::PHASE_LADDER_CARRYING;
    z.StartWalkAnim(ownerNowNs, 0);
    }
    z.AttachShield();
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 21 ZOMBIE_LADDER
    // 原 case ZOMBIE_CATAPULT
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mBodyHealth = 850;
    { z.mX = (WIDE_BOARD_WIDTH + 25 + Rand(10)); z.mRootBindingGeneration = 0; }
    z.mSummonCounter = 20;
    if (z.IsOnBoard())
    {
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_walk, ReanimLoopType::REANIM_LOOP, 0, 5.5f);
    }
    else
    {
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 0, 8.0f);
    }
    z.mZombieRect = Rect(0, -13, 153, 140);
    z.mZombieAttackRect = Rect(10, -13, 133, 140);
    z.mVariant = false;
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 22 ZOMBIE_CATAPULT
    ZombieInitGargantuar,  // 23 ZOMBIE_GARGANTUAR（共体）
    // 原 case ZOMBIE_IMP
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    if (!z.IsOnBoard())
    {
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_walk, ReanimLoopType::REANIM_LOOP, 0, 12.0f);
    }
    if (z.mApp->IsIZombieLevel())
    {
    z.mBodyHealth = 70;
    }
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 24 ZOMBIE_IMP
    // 原 case ZOMBIE_BOSS
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    { z.mX = (0.0f); z.mRootBindingGeneration = 0; }
    z.mY = (0.0f);
    z.mZombieRect = Rect(700, 80, 90, 430);
    z.mZombieAttackRect = Rect(0, 0, 0, 0);
    lc.mRenderLayer = RenderLayer::RENDER_LAYER_TOP;
    z.mBodyHealth = z.mApp->IsAdventureMode() ? 40000 : 60000;
    if (z.IsOnBoard())
    {
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_enter, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 12.0f);
    z.mSummonCounter = 500;
    z.mBossHeadCounter = 5000;
    z.mZombiePhase = ZombiePhase::PHASE_BOSS_ENTER;
    }
    else
    {
    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_head_idle, ReanimLoopType::REANIM_LOOP, 0, 12.0f);
    }
    z.BossSetupReanim(ownerNowNs, visualNowNs, animationNowNs);
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 25 ZOMBIE_BOSS
    // 原 case ZOMBIE_PEA_HEAD
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.LoadPlainZombieReanim();
    z.ReanimShowRange(ReanimTrackRanges::anim_hair, RENDER_GROUP_HIDDEN);
    z.ReanimShowRange(ReanimTrackRanges::anim_head2, RENDER_GROUP_HIDDEN);

    Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
    if (z.IsOnBoard())
    {
    aBodyReanim->SetFramesForLayer(ReanimTrackId::anim_walk2);
    }

    ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName(ReanimTrackId::anim_head1);
    aBodyReanim->SetVariantOption(ReanimTrackId::anim_head1, ReanimOptions::Zombie::anim_head1::BLANK);
    Reanimation* aHeadReanim = z.mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_PEASHOOTER, animationNowNs);
    aHeadReanim->PlayReanim(ReanimTrackId::anim_head_idle, ReanimLoopType::REANIM_LOOP, 0, 15.0f);
    z.mSpecialHeadReanimID = z.mApp->ReanimationGetID(aHeadReanim);
    AttachEffect* aAttachEffect = AttachReanim(aTrackInstance->mAttachmentID, aHeadReanim, 0.0f, 0.0f);
    aBodyReanim->mFrameBasePose = 0;
    TodScaleRotateTransformMatrix(aAttachEffect->mOffset, 65.0f, -5.0f, 0.2f, -1.0f, 1.0f);

    z.mPhaseCounter = 150;
    z.mVariant = false;
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 26 ZOMBIE_PEA_HEAD
    // 原 case ZOMBIE_WALLNUT_HEAD
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.LoadPlainZombieReanim();
    z.ReanimShowRange(ReanimTrackRanges::anim_hair, RENDER_GROUP_HIDDEN);
    z.ReanimShowRange(ReanimTrackRanges::anim_head, RENDER_GROUP_HIDDEN);
    z.ReanimShowRange(ReanimTrackRanges::Zombie_tie, RENDER_GROUP_HIDDEN);

    Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
    ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName(ReanimTrackId::Zombie_body);
    Reanimation* aHeadReanim = z.mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_WALLNUT, animationNowNs);
    aHeadReanim->PlayReanim(ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 0, 15.0f);
    z.mSpecialHeadReanimID = z.mApp->ReanimationGetID(aHeadReanim);
    AttachEffect* aAttachEffect = AttachReanim(aTrackInstance->mAttachmentID, aHeadReanim, 0.0f, 0.0f);
    aBodyReanim->mFrameBasePose = 0;
    TodScaleRotateTransformMatrix(aAttachEffect->mOffset, 50.0f, 0.0f, 0.2f, -0.8f, 0.8f);

    z.mHelmType = HelmType::HELMTYPE_WALLNUT;
    z.mHelmHealth = 1100;
    z.mVariant = false;
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 27 ZOMBIE_WALLNUT_HEAD
    // 原 case ZOMBIE_JALAPENO_HEAD
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.LoadPlainZombieReanim();
    z.ReanimShowRange(ReanimTrackRanges::anim_hair, RENDER_GROUP_HIDDEN);
    z.ReanimShowRange(ReanimTrackRanges::anim_head, RENDER_GROUP_HIDDEN);
    z.ReanimShowRange(ReanimTrackRanges::Zombie_tie, RENDER_GROUP_HIDDEN);

    Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
    ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName(ReanimTrackId::Zombie_body);
    Reanimation* aHeadReanim = z.mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_JALAPENO, animationNowNs);
    aHeadReanim->PlayReanim(ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 0, 15.0f);
    z.mSpecialHeadReanimID = z.mApp->ReanimationGetID(aHeadReanim);
    AttachEffect* aAttachEffect = AttachReanim(aTrackInstance->mAttachmentID, aHeadReanim, 0.0f, 0.0f);
    aBodyReanim->mFrameBasePose = 0;
    TodScaleRotateTransformMatrix(aAttachEffect->mOffset, 55.0f, -5.0f, 0.2f, -1.0f, 1.0f);

    z.mVariant = false;
    z.mBodyHealth = 500;
    int aDistance = 275 + Rand(175);
    z.mPhaseCounter = (int)(aDistance / z.mVelX) * ZOMBIE_LIMP_SPEED_FACTOR;
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 28 ZOMBIE_JALAPENO_HEAD
    // 原 case ZOMBIE_GATLING_HEAD
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.LoadPlainZombieReanim();
    z.ReanimShowRange(ReanimTrackRanges::anim_hair, RENDER_GROUP_HIDDEN);
    z.ReanimShowRange(ReanimTrackRanges::anim_head2, RENDER_GROUP_HIDDEN);

    Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
    if (z.IsOnBoard())
    {
    aBodyReanim->SetFramesForLayer(ReanimTrackId::anim_walk2);
    }

    ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName(ReanimTrackId::anim_head1);
    aBodyReanim->SetVariantOption(ReanimTrackId::anim_head1, ReanimOptions::Zombie::anim_head1::BLANK);
    Reanimation* aHeadReanim = z.mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_GATLINGPEA, animationNowNs);
    aHeadReanim->PlayReanim(ReanimTrackId::anim_head_idle, ReanimLoopType::REANIM_LOOP, 0, 15.0f);
    z.mSpecialHeadReanimID = z.mApp->ReanimationGetID(aHeadReanim);
    AttachEffect* aAttachEffect = AttachReanim(aTrackInstance->mAttachmentID, aHeadReanim, 0.0f, 0.0f);
    aBodyReanim->mFrameBasePose = 0;
    TodScaleRotateTransformMatrix(aAttachEffect->mOffset, 65.0f, -5.0f, 0.2f, -1.0f, 1.0f);

    z.mPhaseCounter = 150;
    z.mVariant = false;
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 29 ZOMBIE_GATLING_HEAD
    // 原 case ZOMBIE_SQUASH_HEAD
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.LoadPlainZombieReanim();
    z.ReanimShowRange(ReanimTrackRanges::anim_hair, RENDER_GROUP_HIDDEN);
    z.ReanimShowRange(ReanimTrackRanges::anim_head2, RENDER_GROUP_HIDDEN);

    Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
    if (z.IsOnBoard())
    {
    aBodyReanim->SetFramesForLayer(ReanimTrackId::anim_walk2);
    }

    ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName(ReanimTrackId::anim_head1);
    aBodyReanim->SetVariantOption(ReanimTrackId::anim_head1, ReanimOptions::Zombie::anim_head1::BLANK);
    Reanimation* aHeadReanim = z.mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_SQUASH, animationNowNs);
    aHeadReanim->PlayReanim(ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 0, 15.0f);
    z.mSpecialHeadReanimID = z.mApp->ReanimationGetID(aHeadReanim);
    AttachEffect* aAttachEffect = AttachReanim(aTrackInstance->mAttachmentID, aHeadReanim, 0.0f, 0.0f);
    aBodyReanim->mFrameBasePose = 0;
    TodScaleRotateTransformMatrix(aAttachEffect->mOffset, 55.0f, -15.0f, 0.2f, -0.75f, 0.75f);

    z.mZombiePhase = ZombiePhase::PHASE_SQUASH_PRE_LAUNCH;
    z.mVariant = false;
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 30 ZOMBIE_SQUASH_HEAD
    // 原 case ZOMBIE_TALLNUT_HEAD
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.LoadPlainZombieReanim();
    z.ReanimShowRange(ReanimTrackRanges::anim_hair, RENDER_GROUP_HIDDEN);
    z.ReanimShowRange(ReanimTrackRanges::anim_head, RENDER_GROUP_HIDDEN);
    z.ReanimShowRange(ReanimTrackRanges::Zombie_tie, RENDER_GROUP_HIDDEN);

    Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
    ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName(ReanimTrackId::Zombie_body);
    Reanimation* aHeadReanim = z.mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_TALLNUT, animationNowNs);
    aHeadReanim->PlayReanim(ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 0, 15.0f);
    z.mSpecialHeadReanimID = z.mApp->ReanimationGetID(aHeadReanim);
    AttachEffect* aAttachEffect = AttachReanim(aTrackInstance->mAttachmentID, aHeadReanim, 0.0f, 0.0f);
    aBodyReanim->mFrameBasePose = 0;
    TodScaleRotateTransformMatrix(aAttachEffect->mOffset, 37.0f, 0.0f, 0.2f, -0.8f, 0.8f);

    z.mHelmType = HelmType::HELMTYPE_TALLNUT;
    z.mHelmHealth = 2200;
    z.mVariant = false;
    { z.mX += (30.0f); z.mRootBindingGeneration = 0; }
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 31 ZOMBIE_TALLNUT_HEAD
    ZombieInitGargantuar,  // 32 ZOMBIE_REDEYE_GARGANTUAR（共体）
    // 原 case ZOMBIE_ZOMBATAR
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, ZombieInitLocals& lc)
    {
    z.mHasObject = true;
    z.LoadPlainZombieReanim();

    Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
    Reanimation* aFlagReanim = z.mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_FLAG, animationNowNs);
    aFlagReanim->PlayReanim(ReanimTrackId::Zombie_flag, ReanimLoopType::REANIM_LOOP, 0, 15.0f);
    z.mSpecialHeadReanimID = z.mApp->ReanimationGetID(aFlagReanim);
    ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName(ReanimTrackId::Zombie_flaghand);
    AttachReanim(aTrackInstance->mAttachmentID, aFlagReanim, 0.0f, 0.0f);
    aBodyReanim->mFrameBasePose = 0;

    { z.mX = (WIDE_BOARD_WIDTH); z.mRootBindingGeneration = 0; }

    z.ReanimShowRange(ReanimTrackRanges::anim_hair, RENDER_GROUP_HIDDEN);
    z.ReanimShowRange(ReanimTrackRanges::anim_head2, RENDER_GROUP_HIDDEN);

    if (z.IsOnBoard())
    {
    aBodyReanim->SetFramesForLayer(ReanimTrackId::anim_walk2);
    }

    ReanimatorTrackInstance* aHeadTrackInstance = aBodyReanim->GetTrackInstanceByName(ReanimTrackId::anim_head1);
    aBodyReanim->SetVariantOption(ReanimTrackId::anim_head1, ReanimOptions::Zombie::anim_head1::BLANK);

    Reanimation* aHeadReanim = z.mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_ZOMBATAR_HEAD, animationNowNs);
    // zombatar_zombie_head 无 anim_head_idle 轨道：原版此查询必 miss，
    // 靠"查不到回退轨道 0"落到 anim_head1（该定义轨道 0）。回退语义已删
    // 除，这里显式点名等效轨道，行为不变。
    aHeadReanim->PlayReanim(ReanimTrackId::anim_head1, ReanimLoopType::REANIM_LOOP, 0, 15.0f);
    aHeadReanim->AssignRenderGroupToTrack(ReanimTrackId::anim_hair, RENDER_GROUP_HIDDEN);

    z.mZombatarHeadReanimID = z.mApp->ReanimationGetID(aHeadReanim);
    AttachEffect* aAttachEffect = AttachReanim(aHeadTrackInstance->mAttachmentID, aHeadReanim, 0.0f, 0.0f);
    aBodyReanim->mFrameBasePose = 0; // @Patoke: should &z be aBodyReanim? &z could be a bug in the original code

    TodScaleRotateTransformMatrix(aAttachEffect->mOffset, M(-20.0f), M(-1.0f), M(0.2f), 1.0f, 1.0f);

    z.ResetZombatarHead(z.mApp);

    z.mPhaseCounter = 150;
    z.mVariant = false;

    if (z.mHasZombatar)
    {
    z.ApplyZombatarHead(z.mApp, &z.mApp->mPlayerInfo->mZombatars[z.mZombatarIndex]);
    }
    ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, z, lc);
    },  // 33 ZOMBIE_ZOMBATAR
    };
    static_assert(sizeof(kZombieInitCases) / sizeof(kZombieInitCases[0]) ==
    (int)ZombieType::NUM_ZOMBIE_TYPES, "ZombieInitialize 分发表槽位数须与 ZombieType 枚举同步");
    static_assert(kZombieInitCases[(int)ZombieType::ZOMBIE_NORMAL] == +ZombieInitNormal &&
    kZombieInitCases[(int)ZombieType::ZOMBIE_DUCKY_TUBE] == +ZombieInitNormal &&
    kZombieInitCases[(int)ZombieType::ZOMBIE_GARGANTUAR] == +ZombieInitGargantuar &&
    kZombieInitCases[(int)ZombieType::ZOMBIE_REDEYE_GARGANTUAR] == +ZombieInitGargantuar,
    "共体槽位锚点错位（匿名槽由生成器 token 对拍覆盖）");

    TOD_ASSERT(theType >= 0 && theType <= ZombieType::NUM_ZOMBIE_TYPES);

    mHasZombatar = false;
    // GOTY 1073 rewrites a flag zombie to the dedicated Zombatar type before
    // the rest of initialization, then selects one saved avatar at random.
    if (theType == ZombieType::ZOMBIE_FLAG && mApp->mPlayerInfo &&
        mApp->mPlayerInfo->mNumZombatars > 0)
    {
        theType = ZombieType::ZOMBIE_ZOMBATAR;
        mHasZombatar = true;
        mZombatarIndex = Rand(mApp->mPlayerInfo->mNumZombatars);
        if (mZombatarIndex >= mApp->mPlayerInfo->mNumZombatars)
            mZombatarIndex = mApp->mPlayerInfo->mNumZombatars - 1;
    }

    mRow = theRow;
    mFromWave = theFromWave;
    { mX = (780 + Rand(ZOMBIE_START_RANDOM_OFFSET)); mRootBindingGeneration = 0; }
    mY = (GetPosYBasedOnRow(theRow));
    mVelX = 0.0f;
    mVelZ = 0.0f;
    mWidth = 120;
    mHeight = 120;
    mFrame = 0;
    mPrevFrame = 0;
    mZombieType = theType;
    mVariant = theVariant;
    mIsEating = false;
    const auto aStatusNowNs = ownerNowNs;
    mShotFlashEndTimeNs = aStatusNowNs;
    mShieldFlashEndTimeNs = 0;
    mShieldRecoilEndTimeNs = 0;
    mChilledEndTimeNs = 0;
    mIceTrapEndTimeNs = 0;
    mButteredEndTimeNs = 0;
    mMindControlled = false;
    mBlowingAway = false;
    mHasHead = true;
    mHasArm = true;
    mHasObject = false;
    mInPool = false;
    mOnHighGround = false;
    mHelmType = HelmType::HELMTYPE_NONE;
    mShieldType = ShieldType::SHIELDTYPE_NONE;
    mYuckyFace = false;
    mYuckyFaceStartTimeNs = aStatusNowNs;
    mAnimCounter = 0;
    mGroanCounter = RandRangeInt(300, 400);
    mAnimTicksPerFrame = 12;
    mAnimFrames = 12;
    mBirthTimeNs = aStatusNowNs;
    mTargetCol = -1;
    mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
    mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
    mPhaseCounter = 0;
    mHitUmbrella = false;
    mDroppedLoot = false;
    mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
    mZombieRect = Rect(36, 0, 42, 115);
    mZombieAttackRect = Rect(50, 0, 20, 115);
    mPlayingSong = false;
    mFadeEndTimeNs = -1;
    mFlatTires = false;
    mScaleZombie = 1.0f;
    mUseLadderCol = -1;
    mShieldHealth = 0;
    mHelmHealth = 0;
    mAltitude = 0.0f;
    mAltitudeMotion = {};
    mFlyingHealth = 0;
    mOriginalAnimRate = 0.0f;
    mAttachmentID = AttachmentID::ATTACHMENTID_NULL;
    mSummonCounter = 0;
    mBossStompCounter = -1;
    mBossBungeeCounter = -1;
    mBossHeadCounter = -1;
    mBodyReanimID = ReanimationID::REANIMATIONID_NULL; mRootBindingGeneration = 0;
    mTargetPlantID = PlantID::PLANTID_NULL;
    mBossMode = 0;
    mBossFireBallReanimID = ReanimationID::REANIMATIONID_NULL;
    mSpecialHeadReanimID = ReanimationID::REANIMATIONID_NULL;
    mTargetRow = -1;
    mFireballRow = -1;
    mIsFireBall = false;
    mMoweredReanimID = ReanimationID::REANIMATIONID_NULL;
    mLastPortalX = -1;
    for (int i = 0; i < MAX_ZOMBIE_FOLLOWERS; i++)
    {
        mFollowerZombieID[i] = ZombieID::ZOMBIEID_NULL;
    }
    if (mBoard && mBoard->IsFlagWave(mFromWave))
    {
        { mX += (40.0f); mRootBindingGeneration = 0; }
    }
    PickRandomSpeed(ownerNowNs);
    mBodyHealth = 270;

    const ZombieDefinition& aZombieDef = GetZombieDefinition(mZombieType);
    if (aZombieDef.mReanimationType != ReanimationType::REANIM_NONE)
    {
        LoadReanim(ownerNowNs, visualNowNs, animationNowNs, aZombieDef.mReanimationType);
    }

    // C 形态 -> 匿名 lambda 表（固定尾部直呼形态）：尾部公共逻辑具名化，
    // 各槽位结尾直呼 ZombieInitializeTail；case 内 return 原样即跳过尾部；
    // 越界哨兵走 else 直调（原 default 无操作后尾部照跑）。
    ZombieInitLocals aInitLocals{RenderLayer::RENDER_LAYER_ZOMBIE, 4,
        aZombieDef.mReanimationType, theParentZombie, theFromWave};
    const unsigned aTypeIndex = static_cast<unsigned>(theType);
    if (aTypeIndex < static_cast<unsigned>(ZombieType::NUM_ZOMBIE_TYPES))
    {
        kZombieInitCases[aTypeIndex](ownerNowNs, visualNowNs, animationNowNs, *this, aInitLocals);
    }
    else
    {
        ZombieInitializeTail(ownerNowNs, visualNowNs, animationNowNs, *this, aInitLocals);
    }
}


//0x524040
void Zombie::SetupDoorArms(Reanimation* aReanim, bool theShow)
{
    int aArmGroup = RENDER_GROUP_NORMAL;
    int aDoorGroup = RENDER_GROUP_HIDDEN;
    if (theShow)
    {
        aArmGroup = RENDER_GROUP_HIDDEN;
        aDoorGroup = RENDER_GROUP_NORMAL;
    }

    aReanim->AssignRenderGroupToRange(ReanimTrackRanges::Zombie_outerarm_hand, aArmGroup);
    aReanim->AssignRenderGroupToRange(ReanimTrackRanges::Zombie_outerarm_lower, aArmGroup);
    aReanim->AssignRenderGroupToRange(ReanimTrackRanges::Zombie_outerarm_upper, aArmGroup);
    aReanim->AssignRenderGroupToRange(ReanimTrackRanges::anim_innerarm, aArmGroup);
    aReanim->AssignRenderGroupToRange(ReanimTrackRanges::Zombie_outerarm_screendoor, aDoorGroup);
    aReanim->AssignRenderGroupToRange(ReanimTrackRanges::Zombie_innerarm_screendoor, aDoorGroup);
    aReanim->AssignRenderGroupToRange(ReanimTrackRanges::Zombie_innerarm_screendoor_hand, aDoorGroup);
}

//0x5240B0
void Zombie::SetupReanimLayers(Reanimation* aReanim, ZombieType theZombieType)
{
    aReanim->AssignRenderGroupToRange(ReanimTrackRanges::anim_cone, RENDER_GROUP_HIDDEN);
    aReanim->AssignRenderGroupToRange(ReanimTrackRanges::anim_bucket, RENDER_GROUP_HIDDEN);
    aReanim->AssignRenderGroupToRange(ReanimTrackRanges::anim_screendoor, RENDER_GROUP_HIDDEN);
    aReanim->AssignRenderGroupToRange(ReanimTrackRanges::Zombie_flaghand, RENDER_GROUP_HIDDEN);
    aReanim->AssignRenderGroupToRange(ReanimTrackRanges::Zombie_duckytube, RENDER_GROUP_HIDDEN);
    aReanim->AssignRenderGroupToRange(ReanimTrackRanges::anim_tongue, RENDER_GROUP_HIDDEN);
    aReanim->AssignRenderGroupToRange(ReanimTrackRanges::Zombie_mustache, RENDER_GROUP_HIDDEN);
    SetupDoorArms(aReanim, false);

    // B 形态 -> 常量表 kZombieLayerOps（原 7 支链合并；行序 = ZombieType 枚举序
    // 0..33，每行至多 4 个渲染组操作，空区间（mLo==mHi）= 行尾哨兵/无 case。
    // 单轨操作（原 AssignRenderGroupToTrack）编码为 [t, t+1) 单元素区间——
    // 与按轨道号精确命中语义等价（Reanimator.cpp 两实现对照）；DOOR 支为
    // SetupDoorArms 整体调用，异构特例表外保留。
    struct ZombieLayerOp
    {
        ReanimTrackIdRange mRange;  // mLo == mHi = 空操作（行尾）
        int mGroup;
    };
    constexpr auto JustTrack = [](ReanimTrackId theTrack)
    {
        return ReanimTrackIdRange{theTrack, static_cast<ReanimTrackId>(static_cast<std::uint32_t>(theTrack) + 1)};
    };
    constexpr ZombieLayerOp kNoLayerOp{ReanimTrackIdRange{}, 0};
    static constexpr ZombieLayerOp kZombieLayerOps[(int)ZombieType::NUM_ZOMBIE_TYPES][4] = {
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 0 ZOMBIE_NORMAL（无 case）
        { { ReanimTrackRanges::anim_innerarm, RENDER_GROUP_HIDDEN }, { JustTrack(ReanimTrackId::Zombie_flaghand), RENDER_GROUP_NORMAL }, { JustTrack(ReanimTrackId::Zombie_innerarm_screendoor), RENDER_GROUP_NORMAL }, kNoLayerOp },  // 1 ZOMBIE_FLAG（后两支原为单轨 AssignRenderGroupToTrack）
        { { ReanimTrackRanges::anim_cone, RENDER_GROUP_NORMAL }, { ReanimTrackRanges::anim_hair, RENDER_GROUP_HIDDEN }, kNoLayerOp, kNoLayerOp },  // 2 ZOMBIE_TRAFFIC_CONE
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 3 ZOMBIE_POLEVAULTER（无 case）
        { { ReanimTrackRanges::anim_bucket, RENDER_GROUP_NORMAL }, { ReanimTrackRanges::anim_hair, RENDER_GROUP_HIDDEN }, kNoLayerOp, kNoLayerOp },  // 4 ZOMBIE_PAIL
        { { ReanimTrackRanges::Zombie_paper_paper, RENDER_GROUP_HIDDEN }, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 5 ZOMBIE_NEWSPAPER
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 6 ZOMBIE_DOOR（特例表外，槽位不可达）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 7 ZOMBIE_FOOTBALL（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 8 ZOMBIE_DANCER（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 9 ZOMBIE_BACKUP_DANCER（无 case）
        { { ReanimTrackRanges::Zombie_duckytube, RENDER_GROUP_NORMAL }, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 10 ZOMBIE_DUCKY_TUBE
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 11 ZOMBIE_SNORKEL（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 12 ZOMBIE_ZAMBONI（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 13 ZOMBIE_BOBSLED（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 14 ZOMBIE_DOLPHIN_RIDER（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 15 ZOMBIE_JACK_IN_THE_BOX（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 16 ZOMBIE_BALLOON（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 17 ZOMBIE_DIGGER（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 18 ZOMBIE_POGO（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 19 ZOMBIE_YETI（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 20 ZOMBIE_BUNGEE（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 21 ZOMBIE_LADDER（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 22 ZOMBIE_CATAPULT（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 23 ZOMBIE_GARGANTUAR（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 24 ZOMBIE_IMP（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 25 ZOMBIE_BOSS（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 26 ZOMBIE_PEA_HEAD（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 27 ZOMBIE_WALLNUT_HEAD（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 28 ZOMBIE_JALAPENO_HEAD（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 29 ZOMBIE_GATLING_HEAD（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 30 ZOMBIE_SQUASH_HEAD（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 31 ZOMBIE_TALLNUT_HEAD（无 case）
        { kNoLayerOp, kNoLayerOp, kNoLayerOp, kNoLayerOp },  // 32 ZOMBIE_REDEYE_GARGANTUAR（无 case）
        { { ReanimTrackRanges::anim_innerarm, RENDER_GROUP_HIDDEN }, { JustTrack(ReanimTrackId::Zombie_flaghand), RENDER_GROUP_NORMAL }, { JustTrack(ReanimTrackId::Zombie_innerarm_screendoor), RENDER_GROUP_NORMAL }, { JustTrack(ReanimTrackId::anim_head1), RENDER_GROUP_OVER_SHIELD } },  // 33 ZOMBIE_ZOMBATAR（FLAG 共体 + anim_head1 单轨）
    };
    static_assert(sizeof(kZombieLayerOps) / sizeof(kZombieLayerOps[0]) ==
        (int)ZombieType::NUM_ZOMBIE_TYPES, "kZombieLayerOps 槽位须与 ZombieType 枚举同步");

    if (theZombieType == ZombieType::ZOMBIE_DOOR)
    {
        SetupDoorArms(aReanim, true);
    }
    else
    {
        const unsigned aTypeIndex = static_cast<unsigned>(theZombieType);
        if (aTypeIndex < sizeof(kZombieLayerOps) / sizeof(kZombieLayerOps[0]))
        {
            for (const ZombieLayerOp& aOp : kZombieLayerOps[aTypeIndex])
            {
                if (aOp.mRange.mLo == aOp.mRange.mHi)  // 空区间 = 行尾
                {
                    break;
                }
                aReanim->AssignRenderGroupToRange(aOp.mRange, aOp.mGroup);
            }
        }
    }
}

//0x524210
void Zombie::ShowDoorArms(bool theShow)
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    if (aBodyReanim)
    {
        SetupDoorArms(aBodyReanim, theShow);
        if (!mHasArm)
        {
            ReanimShowRange(ReanimTrackRanges::Zombie_outerarm_lower, RENDER_GROUP_HIDDEN);
            ReanimShowRange(ReanimTrackRanges::Zombie_outerarm_hand, RENDER_GROUP_HIDDEN);
        }
    }
}

//0x524280
void Zombie::ReanimIgnoreClipRect(ReanimTrackId theTrackId, bool theIgnoreClipRect)
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    if (aBodyReanim == nullptr)
        return;

    // 同名（同枚举）轨道全部设置，与原 stricmp 全扫语义一致。
    for (int i = 0; i < aBodyReanim->mDefinition->mTracks.count; i++)
    {
        if (aBodyReanim->GetTrackId(i) == theTrackId)
        {
            aBodyReanim->mTrackInstances[i].mIgnoreClipRect = theIgnoreClipRect;
        }
    }
}

//0x524310
void Zombie::ReanimReenableClipping()
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    if (aBodyReanim == nullptr)
        return;

    for (int i = 0; i < aBodyReanim->mDefinition->mTracks.count; i++)
    {
        aBodyReanim->mTrackInstances[i].mIgnoreClipRect = false;
    }
}

//0x524370
void Zombie::LoadPlainZombieReanim()
{
    mZombieAttackRect = Rect(20, 0, 50, 115);
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    if (aBodyReanim == nullptr)
        return;

    SetupReanimLayers(aBodyReanim, mZombieType);
    if (mBoard)
    {
        EnableMustache(mBoard->mMustacheMode);
        EnableFuture(mBoard->mFutureMode);
    }

    if ((mBoard && mBoard->mPlantRow[mRow] == PlantRowType::PLANTROW_POOL) || mZombieType == ZombieType::ZOMBIE_DUCKY_TUBE)
    {
        ReanimShowRange(ReanimTrackRanges::zombie_duckytube, RENDER_GROUP_NORMAL);
        ReanimIgnoreClipRect(ReanimTrackId::Zombie_duckytube, true);
        ReanimIgnoreClipRect(ReanimTrackId::Zombie_outerarm_hand, true);
        ReanimIgnoreClipRect(ReanimTrackId::None /* Zombie_innerarm3 不在映射表：原版回退轨道 0 */, true);
        SetupWaterTrack(ReanimTrackId::Zombie_whitewater);
        SetupWaterTrack(ReanimTrackId::Zombie_whitewater2);
    }
}

//0x524470
Reanimation* Zombie::LoadReanim(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, ReanimationType theReanimationType)
{
    Reanimation* aBodyReanim = mApp->AddReanimation(0.0f, 0.0f, 0, theReanimationType, animationNowNs);
    mBodyReanimID = mApp->ReanimationGetID(aBodyReanim); mRootBindingGeneration = 0;
    aBodyReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
    aBodyReanim->mIsAttachment = true;

    if (!IsOnBoard())
    {
        if (Rand(4) > 0 && aBodyReanim->TrackExists(ReanimTrackId::anim_idle2))
        {
            float aRanimRate = RandRangeFloat(12.0f, 24.0f);
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_idle2, ReanimLoopType::REANIM_LOOP, 0, aRanimRate);
        }
        else if (aBodyReanim->TrackExists(ReanimTrackId::anim_idle))
        {
            float aRanimRate = RandRangeFloat(12.0f, 18.0f);
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 0, aRanimRate);
        }

        aBodyReanim->SeekRatio(RandRangeFloat(0.0f, 0.99f));
    }
    else
    {
        StartWalkAnim(ownerNowNs, 0);
    }

    if (IsOnBoard() && aBodyReanim->TrackExists(ReanimTrackId::_ground))
        mX = CheckedPosition(RootPosition(*aBodyReanim, IsWalkingBackwards() ? mScaleZombie : -mScaleZombie));
    return aBodyReanim;
}

//0x524590
int Zombie::CountBungeesTargetingSunFlowers()
{
    int aCount = 0;

    Zombie* aZombie = nullptr;
    while (mBoard->IterateZombies(aZombie))
    {
        if (!aZombie->IsDeadOrDying() && aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE && aZombie->mTargetCol != -1)
        {
            Plant* aPlant = mBoard->GetTopPlantAt(aZombie->mTargetCol, aZombie->mRow, PlantPriority::TOPPLANT_BUNGEE_ORDER);
            if (aPlant && aPlant->MakesSun())
            {
                aCount++;
            }
        }
    }

    return aCount;
}

//0x5246A0
void Zombie::PickBungeeZombieTarget(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theColumn)
{
    bool aAllowSunFlowerTarget = true;
    if (CountBungeesTargetingSunFlowers() == mBoard->CountSunFlowers() - 1)
    {
        aAllowSunFlowerTarget = false;
    }

    TodWeightedGridArray aPicks[MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y];
    int aPickCount = 0;

    for (int x = 0; x < MAX_GRID_SIZE_X; x++)
    {
        if (theColumn == -1 || theColumn == x)  // 限制仅能在 theColumn 列寻找目标，除非 theColumn 为 -1
        {
            for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
            {
                int aWeight = 1;
                if (mBoard->GetGraveStoneAt(x, y) || mBoard->mGridSquareType[x][y] == GridSquareType::GRIDSQUARE_DIRT)
                {
                    continue;
                }

                Plant* aPlant = mBoard->GetTopPlantAt(x, y, PlantPriority::TOPPLANT_BUNGEE_ORDER);
                if (aPlant)
                {
                    if (!aAllowSunFlowerTarget && aPlant->MakesSun())
                    {
                        continue;
                    }

                    if (aPlant->mSeedType == SeedType::SEED_GRAVEBUSTER || aPlant->mSeedType == SeedType::SEED_COBCANNON)
                    {
                        continue;
                    }

                    aWeight = 10000;
                }

                if (!mBoard->BungeeIsTargetingCell(x, y))
                {
                    aPicks[aPickCount].mX = x;
                    aPicks[aPickCount].mY = y;
                    aPicks[aPickCount].mWeight = aWeight;
                    aPickCount++;
                }
            }
        }
    }

    if (aPickCount == 0)
    {
        DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
        return;
    }

    TodWeightedGridArray* aGrid = TodPickFromWeightedGridArray(aPicks, aPickCount);
    mTargetCol = aGrid->mX;
    SetRow(aGrid->mY);
    { mX = (mBoard->GridToPixelX(mTargetCol, mRow)); mRootBindingGeneration = 0; }
    mY = (GetPosYBasedOnRow(mRow));
}

//0x524970
void Zombie::BungeeDropZombie(std::int64_t ownerNowNs, Zombie* theDroppedZombie, int theGridX, int theGridY)
{
    mTargetCol = theGridX;
    SetRow(theGridY);
    { mX = (mBoard->GridToPixelX(mTargetCol, mRow)); mRootBindingGeneration = 0; }
    mY = (GetPosYBasedOnRow(mRow));
    PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_raise, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 36.0f);
    mRelatedZombieID = mBoard->ZombieGetID(theDroppedZombie);

    { theDroppedZombie->mX = ((mX) - 15.0f); theDroppedZombie->mRootBindingGeneration = 0; }
    theDroppedZombie->SetRow(theGridY);
    theDroppedZombie->mY = (GetPosYBasedOnRow(theGridY));
    theDroppedZombie->mZombieHeight = ZombieHeight::HEIGHT_GETTING_BUNGEE_DROPPED;
    theDroppedZombie->PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 0, 0.0f);
    theDroppedZombie->mRenderOrder = mRenderOrder + 1;
}

//0x524A70
// GOTY @Patoke: 0x535110
void Zombie::PickRandomSpeed(std::int64_t ownerNowNs)
{
    // GOTY 1073 gives a surfaced snorkel zombie a deterministic post-eating
    // pool speed.  It does not apply this branch to dolphin riders.
    if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL)
    {
        mVelX = 0.3f;
    }
    else if (mZombiePhase == ZombiePhase::PHASE_DIGGER_WALKING)
    {
        if (mApp->IsIZombieLevel())
        {
            mVelX = 0.23f;
        }
        else
        {
            mVelX = 0.12f;
        }
    }
    else if (mZombieType == ZombieType::ZOMBIE_IMP && mApp->IsIZombieLevel())
    {
        mVelX = 0.9f;
    }
    else if (mZombiePhase == ZombiePhase::PHASE_YETI_RUNNING)
    {
        mVelX = 0.8f;
    }
    else if (mZombieType == ZombieType::ZOMBIE_YETI)
    {
        mVelX = 0.4f;
    }
    else if (mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER ||
        mZombieType == ZombieType::ZOMBIE_POGO || mZombieType == ZombieType::ZOMBIE_FLAG ||
        mZombieType == ZombieType::ZOMBIE_ZOMBATAR) // 1073 0x5351D3: custom flags share the fixed 0.45 speed.
    {
        mVelX = 0.45f;
    }
    else if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING || mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT ||
        mZombieType == ZombieType::ZOMBIE_FOOTBALL || mZombieType == ZombieType::ZOMBIE_SNORKEL || mZombieType == ZombieType::ZOMBIE_JACK_IN_THE_BOX)
    {
        mVelX = RandRangeFloat(0.66f, 0.68f);
    }
    else if (mZombiePhase == ZombiePhase::PHASE_LADDER_CARRYING || mZombieType == ZombieType::ZOMBIE_SQUASH_HEAD)
    {
        mVelX = RandRangeFloat(0.79f, 0.81f);
    }
    else if (mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MAD || mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING_WITHOUT_DOLPHIN)
    {
        mVelX = RandRangeFloat(0.89f, 0.91f);
    }
    else
    {
        mVelX = RandRangeFloat(0.23f, 0.37f);
        if (mVelX < 0.3f)
        {
            mAnimTicksPerFrame = 12;
        }
        else
        {
            mAnimTicksPerFrame = 15;
        }
    }

    UpdateAnimSpeed(ownerNowNs);
}

//0x524C70
void Zombie::BungeeStealTarget(std::int64_t ownerNowNs)
{
    PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_grab, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);

    Plant* aPlant = mBoard->GetTopPlantAt(mTargetCol, mRow, PlantPriority::TOPPLANT_BUNGEE_ORDER);
    if (aPlant && !aPlant->NotOnGround())
    {
        TOD_ASSERT(aPlant->mSeedType != SeedType::SEED_GRAVEBUSTER);

        if (aPlant->mSeedType != SeedType::SEED_COBCANNON && aPlant->mSeedType != SeedType::SEED_GRAVEBUSTER)
        {
            mTargetPlantID = (PlantID)mBoard->mPlants.DataArrayGetID(aPlant);
            aPlant->mOnBungeeState = PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE;
            mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PROJECTILE, mRow, 0);
        }
    }
}

//0x524D70
void Zombie::BungeeLiftTarget(std::int64_t ownerNowNs)
{
    PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_raise, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 36.0f);

    Plant* aPlant = mBoard->mPlants.DataArrayTryToGet((unsigned int)mTargetPlantID);
    if (aPlant == nullptr)
        return;

#ifdef DO_FIX_BUGS
    Zombie* aZombie = nullptr;
    while (mBoard->IterateZombies(aZombie))
    {
        if (aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE && aZombie != this && aZombie->mTargetPlantID == mTargetPlantID)
        {
            aZombie->mTargetPlantID = PlantID::PLANTID_NULL;  // 修复类似于 IZ 蹦极刷阳光的 Bug
        }
    }
#endif

    aPlant->mOnBungeeState = PlantOnBungeeState::RISING_WITH_BUNGEE;
    mApp->PlayFoley(FoleyType::FOLEY_FLOOP);

    Reanimation* aPlantReanim = mApp->ReanimationTryToGet(aPlant->mBodyReanimID);
    if (aPlantReanim)
    {

        aPlantReanim->ApplyRate(0.1f);
    }

    if (aPlant->mSeedType == SeedType::SEED_CATTAIL && mBoard->GetTopPlantAt(mTargetCol, mRow, PlantPriority::TOPPLANT_ONLY_PUMPKIN))
    {
        mBoard->NewPlant(mTargetCol, mRow, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
    }

    if (mApp->IsIZombieLevel())
    {
        mBoard->mChallenge->IZombiePlantDropRemainingSun(aPlant);
    }
}

//0x524EF0
void Zombie::BungeeLanding(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_DIVING && mAltitude < 1500.0f && !mApp->IsFinalBossLevel())
    {
        mApp->PlayFoley(FoleyType::FOLEY_BUNGEE_SCREAM);
        mZombiePhase = ZombiePhase::PHASE_BUNGEE_DIVING_SCREAMING;
    }

    if (mAltitude > 40.0f)
        return;

    Plant* aPlant = mBoard->FindUmbrellaPlant(mTargetCol, mRow);
    if (aPlant)
    {
        mApp->PlaySample(SOUND_BOING);
        mApp->PlayFoley(FoleyType::FOLEY_UMBRELLA);

        aPlant->DoSpecial(ownerNowNs, visualNowNs, animationNowNs);

        mZombiePhase = ZombiePhase::PHASE_BUNGEE_RISING;
        mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 1);
        mHitUmbrella = true;

        return;
    }

    if (mAltitude > 0.0f)
        return;

    mAltitude = 0.0f;
    Zombie* aZombie = mBoard->ZombieTryToGet(mRelatedZombieID);
    if (aZombie)  // 存在关联的僵尸时，释放空投的僵尸
    {
        aZombie->mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
        aZombie->StartWalkAnim(ownerNowNs, 0);

        mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
        mZombiePhase = ZombiePhase::PHASE_BUNGEE_RISING;
        PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_raise, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 36.0f);
    }
    else  // 不存在关联的僵尸时，开始偷取植物
    {
        mZombiePhase = ZombiePhase::PHASE_BUNGEE_AT_BOTTOM;
        mPhaseCounter = 300;
        PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 5, 24.0f);
        mApp->ReanimationGet(mBodyReanimID)->SeekRatio(0.5f);
    }
}

//0x525180
void Zombie::UpdateZombieBungee(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (IsDeadOrDying() || IsImmobilizied(ownerNowNs))
        return;

    // C 形态 -> 匿名 lambda 函数指针表 kBungeePhaseCases（仿 kBossPhaseCases
    // 范式）：行序 = ZombiePhase 枚举序 PHASE_BUNGEE_DIVING..PHASE_BUNGEE_CUTSCENE
    // （7 相位连续）；DIVING/DIVING_SCREAMING 共体（constexpr 指针变量复用同一
    // lambda）；HIT_OUCHY 槽 nullptr（不可达相位 + 原 1073 无此 case）；
    // 表外相位/越界 = 原无命中（无操作）。
    using BungeePhaseFn = void (*)(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie&);
    static constexpr BungeePhaseFn kBungeeDiving = [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z)
    {
        float aOldAltitude = z.mAltitude;
        AdvanceVelocity(z.mAltitude, -8.0f, Sexy::gUpdateIntervalNs, ownerNowNs, z.mAltitudeMotion);
        if (z.mAltitude <= BUNGEE_ZOMBIE_HEIGHT - 404.0f && aOldAltitude > BUNGEE_ZOMBIE_HEIGHT - 404.0f && z.mRelatedZombieID == ZombieID::ZOMBIEID_NULL)
        {
            z.mApp->PlayFoley(FoleyType::FOLEY_GRASSSTEP);  // 靶子扎地的音效
        }

        z.BungeeLanding(ownerNowNs, visualNowNs, animationNowNs);
    };
    static constexpr BungeePhaseFn kBungeePhaseCases[(int)ZombiePhase::PHASE_BUNGEE_CUTSCENE - (int)ZombiePhase::PHASE_BUNGEE_DIVING + 1] = {
        kBungeeDiving,  // PHASE_BUNGEE_DIVING（共体）
        kBungeeDiving,  // PHASE_BUNGEE_DIVING_SCREAMING（共体）
        // 原 else-if PHASE_BUNGEE_AT_BOTTOM
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z)
        {
            if (z.mPhaseCounter <= 0)
            {
                z.BungeeStealTarget(ownerNowNs);
                z.mZombiePhase = ZombiePhase::PHASE_BUNGEE_GRABBING;
            }
        },
        // 原 else-if PHASE_BUNGEE_GRABBING
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z)
        {
            Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
            if (aBodyReanim->mLoopCount > 0)
            {
                z.BungeeLiftTarget(ownerNowNs);
                z.mZombiePhase = ZombiePhase::PHASE_BUNGEE_RISING;
            }
        },
        // 原 else-if PHASE_BUNGEE_RISING
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z)
        {
            AdvanceVelocity(z.mAltitude, 8.0f, Sexy::gUpdateIntervalNs, ownerNowNs, z.mAltitudeMotion);
            if (z.mAltitude >= 600.0f)
            {
                z.DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
            }
        },
        nullptr,  // PHASE_BUNGEE_HIT_OUCHY（不可达：蹦极相位机不走此值；原 HIT_OUCHY 分支已删）
        // 原 else-if PHASE_BUNGEE_CUTSCENE
        [](std::int64_t , std::int64_t , std::int64_t , Zombie& z)
        {
            z.mAltitude = TodAnimateCurveNs(2'000'000'000, 0, z.mPhaseCounter.mNanoseconds, 40, 0, Curves::SinWave);
            if (z.mPhaseCounter <= 0)
            {
                z.mPhaseCounter = 200;
            }
        },
    };
    static_assert(sizeof(kBungeePhaseCases) / sizeof(kBungeePhaseCases[0]) ==
        (int)ZombiePhase::PHASE_BUNGEE_CUTSCENE - (int)ZombiePhase::PHASE_BUNGEE_DIVING + 1,
        "kBungeePhaseCases 槽位须与 ZombiePhase BUNGEE 段同步");

    const unsigned aPhaseIndex = static_cast<unsigned>(mZombiePhase) - static_cast<unsigned>(ZombiePhase::PHASE_BUNGEE_DIVING);
    if (aPhaseIndex < sizeof(kBungeePhaseCases) / sizeof(kBungeePhaseCases[0]) &&
        kBungeePhaseCases[aPhaseIndex] != nullptr)
    {
        kBungeePhaseCases[aPhaseIndex](ownerNowNs, visualNowNs, animationNowNs, *this);
    }



}

//0x525350
void Zombie::PogoBreak(std::int64_t ownerNowNs, unsigned int theDamageFlags)
{
    if (!mHasObject)
        return;

    if (!TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_LEAVE_BODY))
    {
        //ZombieDrawPosition aDrawPos;
        //GetDrawPos(aDrawPos);

        float aPosX, aPosY;
        GetTrackPosition(ReanimTrackId::Zombie_pogo_stick, aPosX, aPosY);
        TodParticleSystem* aParticle = mApp->AddTodParticle(aPosX, aPosY + 30.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZOMBIE_POGO, ownerNowNs);
        OverrideParticleScale(aParticle);
    }

    TOD_ASSERT(mZombiePhase != ZombiePhase::PHASE_ZOMBIE_DYING && mZombiePhase != ZombiePhase::PHASE_ZOMBIE_BURNED && !mDead);

    mZombieHeight = ZombieHeight::HEIGHT_FALLING;
    mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
    StartWalkAnim(ownerNowNs, 0);
    mZombieRect = Rect(36, 17, 42, 115);
    mZombieAttackRect = Rect(20, 17, 50, 115);
    mShieldHealth = 0;
    mShieldType = ShieldType::SHIELDTYPE_NONE;
    mHasObject = false;
}

//0x525460
bool Zombie::IsBouncingPogo()
{
    return mZombiePhase >= ZombiePhase::PHASE_POGO_BOUNCING && mZombiePhase <= ZombiePhase::PHASE_POGO_FORWARD_BOUNCE_2; // 上界收紧：BOUNCE_7+ 不可达
}

//0x525480
void Zombie::UpdateZombiePogo(std::int64_t ownerNowNs)
{
    if (IsDeadOrDying() || IsImmobilizied(ownerNowNs) || !IsBouncingPogo() || mZombieHeight == ZombieHeight::HEIGHT_IN_TO_CHIMNEY)
        return;

    float aHeight = 40.0f;
    if (mZombiePhase == ZombiePhase::PHASE_POGO_HIGH_BOUNCE_1) // 上界收紧：HIGH_BOUNCE_2+ 不可达（仅 1 被赋值）
    {
        aHeight = 50.0f + 20.0f * (mZombiePhase - ZombiePhase::PHASE_POGO_HIGH_BOUNCE_1);
    }
    else if (mZombiePhase == ZombiePhase::PHASE_POGO_FORWARD_BOUNCE_2)
    {
        aHeight = 90.0f;
    }
    // FORWARD_BOUNCE_7 分支已删（不可达）
    mAltitude = TodAnimateCurveNs(POGO_BOUNCE_TIME * 10'000'000LL, 0, mPhaseCounter.mNanoseconds, 9.0f, aHeight + 9.0f, Curves::BounceSlowMiddle);
    mFrame = ClampInt(3 - mAltitude / 3, 0, 3);

    if (mPhaseCounter == 7)
    {
        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        aBodyReanim->SeekRatio(0.0f);

        aBodyReanim->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
    }
    if (IsOnBoard() && mPhaseCounter == 5)
    {
        mApp->PlayFoley(FoleyType::FOLEY_POGO_ZOMBIE);
    }

    if (mZombieHeight == ZombieHeight::HEIGHT_UP_TO_HIGH_GROUND)
    {
        mAltitude += HIGH_GROUND_HEIGHT;
        mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
    }
    else if (mZombieHeight == ZombieHeight::HEIGHT_DOWN_OFF_HIGH_GROUND)
    {
        mOnHighGround = false;
        mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
    }
    else if (mOnHighGround)
    {
        mAltitude += HIGH_GROUND_HEIGHT;
    }

    if (mZombiePhase == ZombiePhase::PHASE_POGO_FORWARD_BOUNCE_2 && mPhaseCounter == 70)
    {
        Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_VAULT);
        if (aPlant && aPlant->mSeedType == SeedType::SEED_TALLNUT)
        {
            mApp->PlayFoley(FoleyType::FOLEY_BONK);
            mApp->AddTodParticle((aPlant->mX) + 60, (aPlant->mY) - 20, mRenderOrder + 1, ParticleEffect::PARTICLE_TALL_NUT_BLOCK, ownerNowNs);

            mShieldType = ShieldType::SHIELDTYPE_NONE;
            PogoBreak(ownerNowNs, 0U);
            return;
        }
    }

    if (mPhaseCounter != 0)
        return;

    Plant* aPlant = nullptr;
    if (IsOnBoard())
    {
        aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_VAULT);
    }
    if (aPlant == nullptr)
    {
        mZombiePhase = ZombiePhase::PHASE_POGO_BOUNCING;

        PickRandomSpeed(ownerNowNs);
        mPhaseCounter = POGO_BOUNCE_TIME;
        return;
    }

    if (mZombiePhase == ZombiePhase::PHASE_POGO_HIGH_BOUNCE_1)
    {
        mZombiePhase = ZombiePhase::PHASE_POGO_FORWARD_BOUNCE_2;
        mVelX = ((mX) - (aPlant->mX) + 60) / (float)POGO_BOUNCE_TIME;  // 速度 = 跳跃距离 / 跳跃时间
        mPhaseCounter = POGO_BOUNCE_TIME;
    }
    else
    {
        mZombiePhase = ZombiePhase::PHASE_POGO_HIGH_BOUNCE_1;
        mVelX = 0.0f;
        mPhaseCounter = POGO_BOUNCE_TIME;
    }
}

//0x525730
void Zombie::ZombieCatapultFire(Plant* thePlant)
{
    float aOriginX = (mX) + 113.0f;
    float aOriginY = (mY) - 44.0f;
    int aTargetX, aTargetY;
    if (thePlant)
    {
        aTargetX = (thePlant->mX);
        aTargetY = (thePlant->mY);
    }
    else
    {
        aTargetX = (mX) - 300.0f;
        aTargetY = 0.0f;
    }

    mApp->PlayFoley(FoleyType::FOLEY_BASKETBALL);

    Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder, mRow, ProjectileType::PROJECTILE_BASKETBALL);
    float aRangeX = aOriginX - aTargetX - 20.0f;
    float aRangeY = aTargetY - aOriginY;
    if (aRangeX < 40.0f)
    {
        aRangeX = 40.0f;
    }
    aProjectile->mMotionType = ProjectileMotion::MOTION_LOBBED;
    aProjectile->mVelX = -aRangeX / 120.0f;
    aProjectile->mVelY = 0.0f;
    aProjectile->mVelZ = aRangeY / 120.0f - 7.0f;
    aProjectile->mAccZ = 0.115f;
}

//0x525890
Plant* Zombie::FindCatapultTarget()
{
    // 格桶链枚举（2026-10-03 第二批）：原为全池扫 mRow==mRow 行过滤 +
    // (mX)>=(aPlant->mX)+100 像素窗 + !NotOnGround + !IsSpiky——行过滤改本行
    // 9 格×4 层链全成员，其余条件逐字保留（含影子株=原池可见集）。
    // 结果序无关：aTarget 恒为 GetTopPlantAt(最小合格列)（纯查询），
    // 玉米炮双格双链命中由 mPlantCol 比较天然去重。
    Plant* aTarget = nullptr;

    for (int aCol = 0; aCol < MAX_GRID_SIZE_X; aCol++)
    {
        for (int aLayer = 0; aLayer < PLANT_CELL_COUNT; aLayer++)
        {
            Plant* aPlant = mBoard->GetCellPlant(mRow, aCol, aLayer);
            while (aPlant != nullptr)
            {
                if ((mX) >= (aPlant->mX) + 100 && !aPlant->NotOnGround() && !aPlant->IsSpiky())
                {
                    if (aTarget == nullptr || aPlant->mPlantCol < aTarget->mPlantCol)
                    {
                        aTarget = mBoard->GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_CATAPULT_ORDER);
                    }
                }
                aPlant = mBoard->NextCellPlant(aPlant);
            }
        }
    }

    return aTarget;
}

//0x5259D0
void Zombie::UpdateZombieCatapult(std::int64_t ownerNowNs)
{
    if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_NORMAL)
    {
        if ((mX) <= 650 && FindCatapultTarget() && mSummonCounter > 0)
        {
            mZombiePhase = ZombiePhase::PHASE_CATAPULT_LAUNCHING;
            mPhaseCounter = 300;
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_shoot, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
        }
    }
    else if (mZombiePhase == ZombiePhase::PHASE_CATAPULT_LAUNCHING)
    {
        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        if (aBodyReanim->ShouldTriggerTimedEvent(CatapultCycle::ProjectileEvent))
        {
            Plant* aPlant = FindCatapultTarget();
            ZombieCatapultFire(aPlant);
        }
        if (aBodyReanim->mLoopCount > 0)
        {
            // Catapult reuses this timer slot as an ammunition count. One
            // completed launch consumes one ball, independent of step length.
            mSummonCounter -= 1;
            if (mSummonCounter == 4)
            {
                ReanimShowTrack(ReanimTrackId::Zombie_catapult_basketball, RENDER_GROUP_HIDDEN);
            }
            else if (mSummonCounter == 3)
            {
                ReanimShowTrack(ReanimTrackId::Zombie_catapult_basketball2, RENDER_GROUP_HIDDEN);
            }
            else if (mSummonCounter == 2)
            {
                ReanimShowTrack(ReanimTrackId::Zombie_catapult_basketball3, RENDER_GROUP_HIDDEN);
            }
            else if (mSummonCounter == 1)
            {
                ReanimShowTrack(ReanimTrackId::Zombie_catapult_basketball4, RENDER_GROUP_HIDDEN);
            }

            if (mSummonCounter == 0)
            {
                PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_walk, ReanimLoopType::REANIM_LOOP, 20, 6.0f);
                mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
            }
            else
            {
                PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 20, 12.0f);
                mZombiePhase = ZombiePhase::PHASE_CATAPULT_RELOADING;
            }
        }
    }
    else if (mZombiePhase == ZombiePhase::PHASE_CATAPULT_RELOADING && mPhaseCounter == 0)
    {
        Plant* aPlant = FindCatapultTarget();
        if (aPlant)
        {
            mZombiePhase = ZombiePhase::PHASE_CATAPULT_LAUNCHING;
            mPhaseCounter = 300;
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_shoot, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
        }
        else
        {
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_walk, ReanimLoopType::REANIM_LOOP, 20, 6.0f);
            mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
        }
    }
}

//0x525B60
void Zombie::LandFlyer(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, unsigned int theDamageFlags)
{
    if (!TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_LEAVE_BODY) && mZombiePhase == ZombiePhase::PHASE_BALLOON_FLYING)
    {
        mApp->PlaySample(SOUND_BALLOON_POP);
        mZombiePhase = ZombiePhase::PHASE_BALLOON_POPPING;
        PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_pop, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
    }

    if (mBoard->mPlantRow[mRow] == PlantRowType::PLANTROW_POOL)
    {
        DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
    }
    else
    {
        mZombieHeight = ZombieHeight::HEIGHT_FALLING;
    }
}

//0x525BE0
void Zombie::UpdateZombieFlyer(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, bool theAltitudeAlreadyAdvanced)
{
    if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_HIGH_GRAVITY && (mX) < 720.0f)
    {
        if (!theAltitudeAlreadyAdvanced)
            AdvanceVelocity(mAltitude, -0.1f, Sexy::gUpdateIntervalNs, ownerNowNs, mAltitudeMotion);
        if (mAltitude < -35.0f)
        {
            LandFlyer(ownerNowNs, visualNowNs, animationNowNs, 0U);
        }
    }

    if (mZombiePhase == ZombiePhase::PHASE_BALLOON_POPPING)
    {
        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        if (aBodyReanim->mLoopCount > 0)
        {
            mZombiePhase = ZombiePhase::PHASE_BALLOON_WALKING;
            StartWalkAnim(ownerNowNs, 0);
        }
    }

    if (mApp->IsIZombieLevel() && mZombiePhase == ZombiePhase::PHASE_BALLOON_FLYING && mBoard->mChallenge->IZombieGetBrainTarget(this))
    {
        LandFlyer(ownerNowNs, visualNowNs, animationNowNs, 0U);
    }
}

//0x525CE0
void Zombie::UpdateZombieNewspaper(std::int64_t ownerNowNs)
{
    if (mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MADDENING)
    {
        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        if (aBodyReanim->mLoopCount > 0)
        {
            mZombiePhase = ZombiePhase::PHASE_NEWSPAPER_MAD;
            if (mBoard->CountZombiesOnScreen() <= 10 && mHasHead)
            {
                mApp->PlayFoley(FoleyType::FOLEY_NEWSPAPER_RARRGH);
            }

            StartWalkAnim(ownerNowNs, 20);
            aBodyReanim->SetVariantOption(ReanimTrackId::anim_head1, ReanimOptions::Zombie_paper::anim_head1::ZOMBIE_PAPER_MADHEAD);
        }
    }
}

//0x525DC0
void Zombie::UpdateZombiePolevaulter(std::int64_t ownerNowNs)
{
    if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT && mHasHead && mZombieHeight == ZombieHeight::HEIGHT_ZOMBIE_NORMAL)
    {
        Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_VAULT);
        if (aPlant)
        {
            if (mBoard->GetLadderAt(aPlant->mPlantCol, aPlant->mRow))
            {
                float aPlantX = mBoard->GridToPixelX(aPlant->mPlantCol, aPlant->mRow) + 40;
                if (aPlantX > (mX) && mZombieHeight == ZombieHeight::HEIGHT_ZOMBIE_NORMAL && mUseLadderCol != aPlant->mPlantCol)
                {
                    mZombieHeight = ZombieHeight::HEIGHT_UP_LADDER;
                    mUseLadderCol = aPlant->mPlantCol;
                }
                return;
            }

            mZombiePhase = ZombiePhase::PHASE_POLEVAULTER_IN_VAULT;
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_jump, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);

            Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
            float aAnimDuration = aBodyReanim->mFrameCount / aBodyReanim->mAnimRate * 100.0f;
            int aJumpDistance = (mX) - (aPlant->mX) - 80;
            if (mApp->IsWallnutBowlingLevel())
            {
                aJumpDistance = 0;
            }
            mVelX = aJumpDistance / aAnimDuration;
            mHasObject = false;
        }

        if (mApp->IsIZombieLevel() && mBoard->mChallenge->IZombieGetBrainTarget(this))
        {
            mZombiePhase = ZombiePhase::PHASE_POLEVAULTER_POST_VAULT;
            StartWalkAnim(ownerNowNs, 0);
        }
    }
    else if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT)
    {
        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

        bool aJumpEnds = false;
        if ((static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs) > 0.6f && (static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs) <= 0.7f)
        {
            Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_VAULT);
            if (aPlant && aPlant->mSeedType == SeedType::SEED_TALLNUT)
            {
                mApp->PlayFoley(FoleyType::FOLEY_BONK);
                aJumpEnds = true;
                mApp->AddTodParticle((aPlant->mX) + 60, (aPlant->mY) - 20, mRenderOrder + 1, ParticleEffect::PARTICLE_TALL_NUT_BLOCK, ownerNowNs);

                mZombieHeight = ZombieHeight::HEIGHT_FALLING;
                { mX = ((aPlant->mX)); mRootBindingGeneration = 0; }
                mY -= (30.0f);
            }
        }

        if (aBodyReanim->mLoopCount > 0)
        {
            aJumpEnds = true;
            { mX -= (150.0f); mRootBindingGeneration = 0; }
        }
        if (aBodyReanim->ShouldTriggerTimedEvent(0.2f))
        {
            mApp->PlayFoley(FoleyType::FOLEY_GRASSSTEP);
        }
        if (aBodyReanim->ShouldTriggerTimedEvent(0.4f))
        {
            mApp->PlayFoley(FoleyType::FOLEY_POLEVAULT);
        }

        if (aJumpEnds)
        {

            mZombiePhase = ZombiePhase::PHASE_POLEVAULTER_POST_VAULT;
            mZombieAttackRect = Rect(50, 0, 20, 115);

            StartWalkAnim(ownerNowNs, 0);
        }
        else
        {
            float aOldPosX = (mX);
            { mX -= (150.0f * (static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs)); mRootBindingGeneration = 0; }
            mY = (GetPosYBasedOnRow(mRow));
            { mX = (aOldPosX); mRootBindingGeneration = 0; }
        }
    }
}

//0x526190
bool Zombie::IsTanglekelpTarget()
{
    Plant* aPlant = nullptr;
    while (mBoard->IteratePlants(aPlant))
    {
        if (aPlant->mSeedType == SeedType::SEED_TANGLEKELP && aPlant->mTargetZombieID == mBoard->ZombieGetID(this))
        {
            return true;
        }
    }

    return false;
}

//0x5261E0
void Zombie::UpdateZombieDolphinRider(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (IsTangleKelpTarget())
        return;

    // C 形态 -> 匿名 lambda 函数指针表 kDolphinPhaseCases（仿 kBossPhaseCases
    // 范式）：行序 = ZombiePhase 枚举序 PHASE_DOLPHIN_WALKING..
    // PHASE_DOLPHIN_WALKING_WITHOUT_DOLPHIN（6 相位连续）；WALKING_WITHOUT_
    // DOLPHIN 槽 nullptr（原无 case）。原链首两支的 aBackwards 判别移入对应
    // lambda（IsWalkingBackwards() 纯查询，分发前后无状态突变，取值等价）；
    // RIDING 支的 early return 在 lambda 内回归（分派即函数尾，语义等价）。
    using DolphinPhaseFn = void (*)(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie&);
    static constexpr DolphinPhaseFn kDolphinPhaseCases[(int)ZombiePhase::PHASE_DOLPHIN_WALKING_WITHOUT_DOLPHIN - (int)ZombiePhase::PHASE_DOLPHIN_WALKING + 1] = {
        // 原 if PHASE_DOLPHIN_WALKING && !aBackwards
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z)
        {
            if (!z.IsWalkingBackwards())
            {
                if ((z.mX) > 700 && (z.mX) <= 720)
                {
                    z.mZombiePhase = ZombiePhase::PHASE_DOLPHIN_INTO_POOL;
                    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_jumpinpool, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 16.0f);
                }
            }
        },  // PHASE_DOLPHIN_WALKING
        // 原 else-if PHASE_DOLPHIN_INTO_POOL
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z)
        {
            Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
            if (aBodyReanim->ShouldTriggerTimedEvent(0.56f))
            {
                Reanimation* aSplashReanim = z.mApp->AddReanimation((z.mX) - 83, (z.mY) + 73, z.mRenderOrder + 1, ReanimationType::REANIM_SPLASH, visualNowNs);
                aSplashReanim->mOverlayMatrix.m00 = 1.2f;
                aSplashReanim->mOverlayMatrix.m11 = 0.8f;
                z.mApp->AddTodParticle((z.mX) - 46, (z.mY) + 115, z.mRenderOrder + 1, ParticleEffect::PARTICLE_PLANTING_POOL, ownerNowNs);
                z.mApp->PlayFoley(FoleyType::FOLEY_ZOMBIE_ENTERING_WATER);
            }

            if (aBodyReanim->mLoopCount > 0)
            {
                { z.mX -= (70.0f); z.mRootBindingGeneration = 0; }
                z.mZombiePhase = ZombiePhase::PHASE_DOLPHIN_RIDING;
                z.mInPool = true;
                z.mZombieAttackRect = Rect(-29, 0, 70, 115);
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_ride, ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME, 0, 12.0f);
            }
        },  // PHASE_DOLPHIN_INTO_POOL
        // 原 else-if PHASE_DOLPHIN_RIDING
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z)
        {
            if ((z.mX) <= 10)
            {
                z.mAltitude = -40.0f;
                z.mZombieHeight = ZombieHeight::HEIGHT_OUT_OF_POOL;
                z.mZombiePhase = ZombiePhase::PHASE_DOLPHIN_WALKING;

                z.PoolSplash(ownerNowNs, visualNowNs, animationNowNs, false);
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_walkdolphin, ReanimLoopType::REANIM_LOOP, 0, 0.0f);
                z.PickRandomSpeed(ownerNowNs);
                return;  // 原 UpdateZombieDolphinRider 早退：分派为函数尾，等价
            }

            if (z.mHasHead && !z.IsTanglekelpTarget())
            {
                Plant* aPlant = z.FindPlantTarget(ZombieAttackType::ATTACKTYPE_VAULT);
                if (aPlant)
                {
                    z.mApp->PlayFoley(FoleyType::FOLEY_DOLPHIN_BEFORE_JUMPING);
                    z.mApp->PlayFoley(FoleyType::FOLEY_PLANT_WATER);

                    z.mVelX = 0.5f;
                    z.mZombiePhase = ZombiePhase::PHASE_DOLPHIN_IN_JUMP;
                    z.mPhaseCounter = DOLPHIN_JUMP_TIME;
                    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_dolphinjump, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 10.0f);
                }
            }
        },  // PHASE_DOLPHIN_RIDING
        // 原 else-if PHASE_DOLPHIN_IN_JUMP
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z)
        {
            Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
            z.mAltitude = TodAnimateCurveNs(DOLPHIN_JUMP_TIME * 10'000'000LL, 0, z.mPhaseCounter.mNanoseconds, 0.0f, 10.0f, Curves::Linear);

            bool aJumpEnds = false;
            if (aBodyReanim->ShouldTriggerTimedEvent(0.3f))
            {
                Plant* aPlant = z.FindPlantTarget(ZombieAttackType::ATTACKTYPE_VAULT);
                if (aPlant && aPlant->mSeedType == SeedType::SEED_TALLNUT)
                {
                    z.mApp->PlayFoley(FoleyType::FOLEY_BONK);
                    aJumpEnds = true;
                    z.mApp->AddTodParticle((aPlant->mX) + 60, (aPlant->mY) - 20, z.mRenderOrder + 1, ParticleEffect::PARTICLE_TALL_NUT_BLOCK, ownerNowNs);

                    z.mZombieHeight = ZombieHeight::HEIGHT_FALLING;
                    { z.mX = ((aPlant->mX) + 25.0f); z.mRootBindingGeneration = 0; }
                    z.mAltitude = 30.0f;
                }
            }
            else if (aBodyReanim->ShouldTriggerTimedEvent(0.49f))
            {
                Reanimation* aSplashReanim = z.mApp->AddReanimation((z.mX) - 63, (z.mY) + 73, z.mRenderOrder + 1, ReanimationType::REANIM_SPLASH, visualNowNs);
                aSplashReanim->mOverlayMatrix.m00 = 1.2f;
                aSplashReanim->mOverlayMatrix.m11 = 0.8f;
                z.mApp->AddTodParticle((z.mX) - 26, (z.mY) + 115, z.mRenderOrder + 1, ParticleEffect::PARTICLE_PLANTING_POOL, ownerNowNs);
                z.mApp->PlayFoley(FoleyType::FOLEY_ZOMBIE_ENTERING_WATER);
                z.mVelX = 0.0f;
            }
            else if (aBodyReanim->mLoopCount > 0)
            {
                aJumpEnds = true;
                { z.mX -= (94.0f); z.mRootBindingGeneration = 0; }
                z.mAltitude = 0.0f;
            }

            if (aJumpEnds)
            {
                z.mZombieAttackRect = Rect(30, 0, 30, 115);
                z.mZombieRect = Rect(20, 0, 42, 115);
                z.mZombiePhase = ZombiePhase::PHASE_DOLPHIN_WALKING_IN_POOL;
                z.StartWalkAnim(ownerNowNs, 0);
            }
        },  // PHASE_DOLPHIN_IN_JUMP
        // 原 else-if PHASE_DOLPHIN_WALKING_IN_POOL
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z)
        {
            bool aBackwards = z.IsWalkingBackwards();
            if (((z.mX) <= 10 && !aBackwards) || ((z.mX) > 680 && aBackwards))
            {
                z.mAltitude = -40.0f;
                z.mZombieHeight = ZombieHeight::HEIGHT_OUT_OF_POOL;
                z.mZombiePhase = ZombiePhase::PHASE_DOLPHIN_WALKING_WITHOUT_DOLPHIN;

                z.PoolSplash(ownerNowNs, visualNowNs, animationNowNs, false);
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_walk, ReanimLoopType::REANIM_LOOP, 0, 0.0f);
                z.PickRandomSpeed(ownerNowNs);
            }
        },  // PHASE_DOLPHIN_WALKING_IN_POOL
        nullptr,  // PHASE_DOLPHIN_WALKING_WITHOUT_DOLPHIN（原无 case）
    };
    static_assert(sizeof(kDolphinPhaseCases) / sizeof(kDolphinPhaseCases[0]) ==
        (int)ZombiePhase::PHASE_DOLPHIN_WALKING_WITHOUT_DOLPHIN - (int)ZombiePhase::PHASE_DOLPHIN_WALKING + 1,
        "kDolphinPhaseCases 槽位须与 ZombiePhase DOLPHIN 段同步");

    const unsigned aPhaseIndex = static_cast<unsigned>(mZombiePhase) - static_cast<unsigned>(ZombiePhase::PHASE_DOLPHIN_WALKING);
    if (aPhaseIndex < sizeof(kDolphinPhaseCases) / sizeof(kDolphinPhaseCases[0]) &&
        kDolphinPhaseCases[aPhaseIndex] != nullptr)
    {
        kDolphinPhaseCases[aPhaseIndex](ownerNowNs, visualNowNs, animationNowNs, *this);
    }
}

//0x526720
void Zombie::UpdateZombieSnorkel(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    // C 形态 -> 匿名 lambda 函数指针表 kSnorkelPhaseCases（仿 kBossPhaseCases
    // 范式）：行序 = ZombiePhase 枚举序 PHASE_SNORKEL_WALKING..
    // PHASE_SNORKEL_DOWN_FROM_EAT（6 相位连续，全覆盖无空槽）。原链首支的
    // !aBackwards 判别与第三支的 aBackwards 判别移入对应 lambda
    //（IsWalkingBackwards() 纯查询，分发前后无状态突变，取值等价）。
    using SnorkelPhaseFn = void (*)(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie&);
    static constexpr SnorkelPhaseFn kSnorkelPhaseCases[(int)ZombiePhase::PHASE_SNORKEL_DOWN_FROM_EAT - (int)ZombiePhase::PHASE_SNORKEL_WALKING + 1] = {
        // 原 if PHASE_SNORKEL_WALKING && !aBackwards
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z)
        {
            if (!z.IsWalkingBackwards())
            {
                if ((z.mX) > 700 && (z.mX) <= 720)
                {
                    z.mVelX = 0.2f;
                    z.mZombiePhase = ZombiePhase::PHASE_SNORKEL_INTO_POOL;
                    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_jumpinpool, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 16.0f);
                }
            }
        },  // PHASE_SNORKEL_WALKING
        // 原 else-if PHASE_SNORKEL_INTO_POOL
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z)
        {
            Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
            z.mAltitude = TodAnimateCurveFloat(0.0, 1.0, static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs, 0.0f, 10.0f, Curves::Linear);

            if (aBodyReanim->ShouldTriggerTimedEvent(0.83f))
            {
                Reanimation* aSplashReanim = z.mApp->AddReanimation((z.mX) - 47, (z.mY) + 73, z.mRenderOrder + 1, ReanimationType::REANIM_SPLASH, visualNowNs);
                aSplashReanim->mOverlayMatrix.m00 = 1.2f;
                aSplashReanim->mOverlayMatrix.m11 = 0.8f;
                z.mApp->AddTodParticle((z.mX) - 10, (z.mY) + 115, z.mRenderOrder + 1, ParticleEffect::PARTICLE_PLANTING_POOL, ownerNowNs);
                z.mApp->PlayFoley(FoleyType::FOLEY_ZOMBIE_ENTERING_WATER);
            }

            if (aBodyReanim->mLoopCount > 0)
            {
                z.mZombiePhase = ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL;
                z.mInPool = true;
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_swim, ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME, 0, 12.0f);
            }
        },  // PHASE_SNORKEL_INTO_POOL
        // 原 else-if PHASE_SNORKEL_WALKING_IN_POOL
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z)
        {
            if (!z.mHasHead)
            {
                z.TakeDamage(ownerNowNs, visualNowNs, animationNowNs, 1800, 9U);
            }
            else
            {
                bool aBackwards = z.IsWalkingBackwards();
                if ((z.mX) <= 25 && !aBackwards)
                {
                    z.mAltitude = -90.0f;
                    { z.mX -= (15.0f); z.mRootBindingGeneration = 0; }
                    z.mZombiePhase = ZombiePhase::PHASE_SNORKEL_WALKING;
                    z.mZombieHeight = ZombieHeight::HEIGHT_OUT_OF_POOL;

                    z.PoolSplash(ownerNowNs, visualNowNs, animationNowNs, false);
                    z.StartWalkAnim(ownerNowNs, 0);
                }
                else if ((z.mX) > 640 && aBackwards)
                {
                    z.mAltitude = -90.0f;
                    { z.mX += (15.0f); z.mRootBindingGeneration = 0; }
                    z.mZombiePhase = ZombiePhase::PHASE_SNORKEL_WALKING;
                    z.mZombieHeight = ZombieHeight::HEIGHT_OUT_OF_POOL;

                    z.PoolSplash(ownerNowNs, visualNowNs, animationNowNs, false);
                    z.StartWalkAnim(ownerNowNs, 0);
                }
                else if (z.mIsEating)
                {
                    z.mZombiePhase = ZombiePhase::PHASE_SNORKEL_UP_TO_EAT;
                    z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_uptoeat, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
                }
            }
        },  // PHASE_SNORKEL_WALKING_IN_POOL
        // 原 else-if PHASE_SNORKEL_UP_TO_EAT
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z)
        {
            Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
            if (!z.mIsEating)
            {
                z.mZombiePhase = ZombiePhase::PHASE_SNORKEL_DOWN_FROM_EAT;
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_uptoeat, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, -24.0f);
            }
            else if (aBodyReanim->mLoopCount > 0)
            {
                z.mZombiePhase = ZombiePhase::PHASE_SNORKEL_EATING_IN_POOL;
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_eat, ReanimLoopType::REANIM_LOOP, 0, 0.0f);
            }
        },  // PHASE_SNORKEL_UP_TO_EAT
        // 原 else-if PHASE_SNORKEL_EATING_IN_POOL
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z)
        {
            if (!z.mIsEating)
            {
                z.mZombiePhase = ZombiePhase::PHASE_SNORKEL_DOWN_FROM_EAT;
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_uptoeat, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, -24.0f);
            }
        },  // PHASE_SNORKEL_EATING_IN_POOL
        // 原 else-if PHASE_SNORKEL_DOWN_FROM_EAT
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z)
        {
            Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
            if (aBodyReanim->mLoopCount > 0)
            {
                z.mZombiePhase = ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL;
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_swim, ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME, 0, 0.0f);
                z.PickRandomSpeed(ownerNowNs);
            }
        },  // PHASE_SNORKEL_DOWN_FROM_EAT
    };
    static_assert(sizeof(kSnorkelPhaseCases) / sizeof(kSnorkelPhaseCases[0]) ==
        (int)ZombiePhase::PHASE_SNORKEL_DOWN_FROM_EAT - (int)ZombiePhase::PHASE_SNORKEL_WALKING + 1,
        "kSnorkelPhaseCases 槽位须与 ZombiePhase SNORKEL 段同步");

    const unsigned aPhaseIndex = static_cast<unsigned>(mZombiePhase) - static_cast<unsigned>(ZombiePhase::PHASE_SNORKEL_WALKING);
    if (aPhaseIndex < sizeof(kSnorkelPhaseCases) / sizeof(kSnorkelPhaseCases[0]))
    {
        kSnorkelPhaseCases[aPhaseIndex](ownerNowNs, visualNowNs, animationNowNs, *this);
    }
}

//0x526AE0
void Zombie::UpdateZombieJackInTheBox(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_RUNNING)
    {
        if (mPhaseCounter <= 0 && mHasHead)
        {
            mPhaseCounter = 110;
            mZombiePhase = ZombiePhase::PHASE_JACK_IN_THE_BOX_POPPING;

            StopZombieSound();
            mApp->PlaySample(SOUND_BOING);
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_pop, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 28.0f);
        }
    }
    else if (mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_POPPING)
    {
        if (mPhaseCounter == 80)
        {
            mApp->PlayFoley(FoleyType::FOLEY_JACK_SURPRISE);
        }

        if (mPhaseCounter <= 0)
        {
            mApp->PlayFoley(FoleyType::FOLEY_EXPLOSION);

            int aPosX = (mX) + mWidth / 2;
            int aPosY = (mY) + mHeight / 2;
            if (mMindControlled)
            {
                mBoard->KillAllZombiesInRadius(mRow, aPosX, aPosY, JackInTheBoxZombieRadius, 1, true, 127);
            }
            else
            {
                mBoard->KillAllZombiesInRadius(mRow, aPosX, aPosY, JackInTheBoxZombieRadius, 1, true, 255);
                mBoard->KillAllPlantsInRadius(aPosX, aPosY, JackInTheBoxPlantRadius);
            }

            mApp->AddTodParticle(aPosX, aPosY, Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 0), ParticleEffect::PARTICLE_JACKEXPLODE, ownerNowNs);
            mBoard->mShakeCounter = 12;
            mBoard->mShakeAmountX = 4;
            mBoard->mShakeAmountY = -6;
            DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);

            if (mApp->IsScaryPotterLevel())
            {
                mBoard->mChallenge->ScaryPotterJackExplode(aPosX, aPosY);
            }
        }
    }
}

//0x526D10
void Zombie::UpdateZombieGargantuar(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{

    if (mZombiePhase == ZombiePhase::PHASE_GARGANTUAR_SMASHING)
    {
        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        if (aBodyReanim->ShouldTriggerTimedEvent(0.64f))
        {
#ifdef DO_FIX_BUGS
            if (mMindControlled)  // 魅惑巨人砸僵尸
            {
                Zombie* aZombie = FindZombieTarget();
                if (aZombie)
                {
                    int aDamage = mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR ? 1000 : 500;
                    aZombie->TakeDamage(ownerNowNs, visualNowNs, animationNowNs, aDamage, 0U);
                }
            }
            else
            {
                Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_CHEW);
                if (aPlant)
                {
                    if (aPlant->mSeedType == SeedType::SEED_SPIKEROCK)
                    {
                        TakeDamage(ownerNowNs, visualNowNs, animationNowNs, 20, 32U);
                        aPlant->SpikeRockTakeDamage(ownerNowNs, visualNowNs, animationNowNs);
                        if (aPlant->mPlantHealth <= 0)
                        {
                            SquishAllInSquare(ownerNowNs, visualNowNs, animationNowNs, aPlant->mPlantCol, aPlant->mRow, ZombieAttackType::ATTACKTYPE_CHEW);
                        }
                    }
                    else
                    {
                        SquishAllInSquare(ownerNowNs, visualNowNs, animationNowNs, aPlant->mPlantCol, aPlant->mRow, ZombieAttackType::ATTACKTYPE_CHEW);
                    }
                }

                if (mApp->IsScaryPotterLevel())
                {
                    int aGridX = mBoard->PixelToGridX((mX), (mY));
                    GridItem* aScaryPot = mBoard->GetScaryPotAt(aGridX, mRow);
                    if (aScaryPot)
                    {
                        mBoard->mChallenge->ScaryPotterOpenPot(aScaryPot);
                    }
                }

                if (mApp->IsIZombieLevel())
                {
                    GridItem* aBrain = mBoard->mChallenge->IZombieGetBrainTarget(this);
                    if (aBrain)
                    {
                        mBoard->mChallenge->IZombieSquishBrain(aBrain);
                    }
                }
            }
#else
            Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_CHEW);
            if (aPlant)
            {
                if (aPlant->mSeedType == SeedType::SEED_SPIKEROCK)
                {
                    TakeDamage(ownerNowNs, visualNowNs, animationNowNs, 20, 32U);
                    aPlant->SpikeRockTakeDamage(ownerNowNs, visualNowNs, animationNowNs);
                    if (aPlant->mPlantHealth <= 0)
                    {
                        SquishAllInSquare(ownerNowNs, visualNowNs, animationNowNs, aPlant->mPlantCol, aPlant->mRow, ZombieAttackType::ATTACKTYPE_CHEW);
                    }
                }
                else
                {
                    SquishAllInSquare(ownerNowNs, visualNowNs, animationNowNs, aPlant->mPlantCol, aPlant->mRow, ZombieAttackType::ATTACKTYPE_CHEW);
                }
            }

            if (mApp->IsScaryPotterLevel())
            {
                int aGridX = mBoard->PixelToGridX((mX), (mY));
                GridItem* aScaryPot = mBoard->GetScaryPotAt(aGridX, mRow);
                if (aScaryPot)
                {
                    mBoard->mChallenge->ScaryPotterOpenPot(aScaryPot);
                }
            }

            if (mApp->IsIZombieLevel())
            {
                GridItem* aBrain = mBoard->mChallenge->IZombieGetBrainTarget(this);
                if (aBrain)
                {
                    mBoard->mChallenge->IZombieSquishBrain(aBrain);
                }
            }
#endif

            mApp->PlayFoley(FoleyType::FOLEY_THUMP);
            mBoard->mShakeCounter = 12;
            mBoard->mShakeAmountX = 0;
            mBoard->mShakeAmountY = 3;
        }

        if (aBodyReanim->mLoopCount > 0)
        {
            mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
            StartWalkAnim(ownerNowNs, 20);
        }

        return;
    }

    float aThrowingDistance = (mX) - 360.0f;
    if (mZombiePhase == ZombiePhase::PHASE_GARGANTUAR_THROWING)
    {
        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        if (aBodyReanim->ShouldTriggerTimedEvent(0.74f))
        {
            mHasObject = false;
            ReanimShowRange(ReanimTrackRanges::Zombie_imp, RENDER_GROUP_HIDDEN);
            ReanimShowTrack(ReanimTrackId::Zombie_gargantuar_whiterope, RENDER_GROUP_HIDDEN);
            mApp->PlayFoley(FoleyType::FOLEY_SWING);

            Zombie* aZombieImp = mBoard->AddZombie(ZombieType::ZOMBIE_IMP, mFromWave);
            if (aZombieImp == nullptr)
                return;

            float aMinThrowDistance = 40.0f;
            if (mBoard->StageHasRoof())
            {
                aThrowingDistance -= 180.0f;
                aMinThrowDistance = -140.0f;
            }
            if (aThrowingDistance < aMinThrowDistance)
            {
                aThrowingDistance = aMinThrowDistance;
            }
            else if (aThrowingDistance > 140.0f)
            {
                aThrowingDistance -= RandRangeFloat(0.0f, 100.0f);
            }

            { aZombieImp->mX = ((mX) - 133.0f); aZombieImp->mRootBindingGeneration = 0; }
            aZombieImp->mY = (GetPosYBasedOnRow(mRow));
            aZombieImp->SetRow(mRow);
            aZombieImp->mVariant = false;
            aZombieImp->mAltitude = 88.0f;
            aZombieImp->mAltitudeMotion = {};
            aZombieImp->mRenderOrder = mRenderOrder + 1;
            aZombieImp->mZombiePhase = ZombiePhase::PHASE_IMP_GETTING_THROWN;
#ifdef DO_FIX_BUGS
            aZombieImp->mScaleZombie = mScaleZombie;
            aZombieImp->mBodyHealth *= mScaleZombie * mScaleZombie;
            aZombieImp->mBodyMaxHealth *= mScaleZombie * mScaleZombie;

            if (mMindControlled)
            {
                { aZombieImp->mX = ((mX) + mWidth); aZombieImp->mRootBindingGeneration = 0; }
                aZombieImp->StartMindControlled();
                aZombieImp->mVelX = -3.0f;
            }
            else
            {
                aZombieImp->mVelX = 3.0f;
            }
#else
            aZombieImp->mVelX = 3.0f;
#endif
            aZombieImp->mChilledEndTimeNs = mChilledEndTimeNs;
            aZombieImp->mVelZ = 0.5f * (aThrowingDistance / aZombieImp->mVelX) * THOWN_ZOMBIE_GRAVITY;
            aZombieImp->PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_thrown, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 18.0f);
            aZombieImp->UpdateReanim(ownerNowNs, visualNowNs, animationNowNs);
            mApp->PlayFoley(FoleyType::FOLEY_IMP);
        }

        if (aBodyReanim->mLoopCount > 0)
        {
            mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
            StartWalkAnim(ownerNowNs, 20);
        }

        return;
    }

    if (IsImmobilizied(ownerNowNs) || !mHasHead)
        return;

    if (mHasObject && mBodyHealth < mBodyMaxHealth / 2 && aThrowingDistance > 40.0f)
    {
        mZombiePhase = ZombiePhase::PHASE_GARGANTUAR_THROWING;
        PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_throw, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
        return;
    }

#ifdef DO_FIX_BUGS
    bool doSmash = FindZombieTarget();
#else
    bool doSmash = false;
    if (FindPlantTarget(ZombieAttackType::ATTACKTYPE_CHEW))
    {
        doSmash = true;
    }
    else if (mApp->IsScaryPotterLevel())
    {
        int aGridX = mBoard->PixelToGridX((mX), (mY));
        if (mBoard->GetScaryPotAt(aGridX, mRow))
        {
            doSmash = true;
        }
    }
    else if (mApp->IsIZombieLevel())
    {
        if (mBoard->mChallenge->IZombieGetBrainTarget(this))
        {
            doSmash = true;
        }
    }
#endif

    if (doSmash)
    {
        mZombiePhase = ZombiePhase::PHASE_GARGANTUAR_SMASHING;
        mApp->PlayFoley(FoleyType::FOLEY_LOW_GROAN);
        PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_smash, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 16.0f);
    }
}

//0x5272E0
void Zombie::UpdateZombieImp(std::int64_t ownerNowNs)
{
    if (mZombiePhase == ZombiePhase::PHASE_IMP_GETTING_THROWN)
    {
        // Gravity is applied before movement in 1073:
        // z(n)=z0+n*v0-g*n*(n+1)/2, vz(n)=v0-g*n.
        auto& motion = mAltitudeMotion;
        const auto previousNowNs = ReanimClock::Subtract(ownerNowNs, static_cast<std::int64_t>(Sexy::gUpdateIntervalNs));
        const double previousTicks = double(ReanimClock::Subtract(previousNowNs, motion.startTimeNs)) / 10'000'000.0;
        const float previousVelocity = CheckedPosition(double(motion.rate) -
            double(THOWN_ZOMBIE_GRAVITY) * previousTicks);
        const float previousAltitude = CheckedPosition(motion.origin + double(mY) +
            double(motion.rate) * previousTicks -
            double(THOWN_ZOMBIE_GRAVITY) * previousTicks * (previousTicks + 1.0) * 0.5);
        if (!motion.bound || previousVelocity != mVelZ ||
            previousAltitude != mAltitude)
        {
            // Store altitude relative to row Y. Roof motion changes row Y
            // continuously; summing its rounded per-update differences would
            // reintroduce feedback even in a double accumulator.
            motion.origin = double(mAltitude) - mY;
            motion.startTimeNs = previousNowNs;
            motion.rate = mVelZ;
            motion.bound = true;
        }
        const double ticks = double(ReanimClock::Subtract(ownerNowNs, motion.startTimeNs)) / 10'000'000.0;
        mVelZ = CheckedPosition(double(motion.rate) - double(THOWN_ZOMBIE_GRAVITY) * ticks);
        mRootBindingGeneration = 0;
        AdvanceVelocity(mX, -mVelX,
            Sexy::gUpdateIntervalNs, ownerNowNs, mVelocityX);

        mY = GetPosYBasedOnRow(mRow);
        mAltitude = CheckedPosition(motion.origin + double(mY) + double(motion.rate) * ticks -
            double(THOWN_ZOMBIE_GRAVITY) * ticks * (ticks + 1.0) * 0.5);
        if (mAltitude <= 0.0f)
        {
            mAltitude = 0.0f;
            mAltitudeMotion = {};
            mZombiePhase = ZombiePhase::PHASE_IMP_LANDING;
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_land, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
        }
    }
    else if (mZombiePhase == ZombiePhase::PHASE_IMP_LANDING)
    {
        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        if (aBodyReanim->mLoopCount > 0)
        {
            mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
            StartWalkAnim(ownerNowNs, 0);
        }
    }
}

//0x5273D0
void Zombie::UpdateZombiePeaHead()
{
    if (!mHasHead)
        return;

    if (mPhaseCounter == 35)
    {
        Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);

        aHeadReanim->PlayReanim(ReanimTrackId::anim_shooting, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 35.0f);
    }
    else if (mPhaseCounter == 0)
    {
        Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);

        aHeadReanim->PlayReanim(ReanimTrackId::anim_head_idle, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 15.0f);
        mApp->PlayFoley(FoleyType::FOLEY_THROW);

        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        int aTrackIndex = aBodyReanim->FindTrackIndex(ReanimTrackId::anim_head1);
        ReanimatorTransform aTransform;
        aBodyReanim->GetCurrentTransform(aTrackIndex, &aTransform);

        float aOriginX = (mX) + aTransform.mTransX - 9.0f;
        float aOriginY = (mY) + aTransform.mTransY + 6.0f - mAltitude;
#ifdef DO_FIX_BUGS
        if (mMindControlled)  // 魅惑修复
        {
            aOriginX += 90.0f * mScaleZombie;
            Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder, mRow, ProjectileType::PROJECTILE_PEA);
            aProjectile->mDamageRangeFlags = 1;
        }
        else
        {
            Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder, mRow, ProjectileType::PROJECTILE_ZOMBIE_PEA);
            aProjectile->mMotionType = ProjectileMotion::MOTION_BACKWARDS;
        }
#else
        Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder, mRow, ProjectileType::PROJECTILE_ZOMBIE_PEA);
        aProjectile->mMotionType = ProjectileMotion::MOTION_BACKWARDS;
#endif

        mPhaseCounter = 150;
    }
}

void Zombie::BurnRow(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theRow)  // 此函数专用于在定义了 DO_FIX_BUGS 时修复火爆辣椒僵尸的 Bug
{
    Zombie* aZombie = nullptr;
    while (mBoard->IterateZombies(aZombie))
    {
        if ((((ZombieFlagsOf(aZombie->mZombieType) & ZombieFlags::Boss) != 0) || aZombie->mRow == theRow) && aZombie->EffectedByDamage(127))
        {
            aZombie->RemoveColdEffects(ownerNowNs);
            aZombie->ApplyBurn(ownerNowNs, visualNowNs, animationNowNs);
        }
    }

    GridItem* aGridItem = nullptr;
    while (mBoard->IterateGridItems(aGridItem))
    {
        if (aGridItem->mGridY == theRow && aGridItem->mGridItemType == GridItemType::GRIDITEM_LADDER)
        {
            aGridItem->GridItemDie();
        }
    }

    Zombie* aBossZombie = mBoard->GetBossZombie();
    if (aBossZombie && aBossZombie->mFireballRow == theRow)
    {
        aBossZombie->BossDestroyIceballInRow(ownerNowNs);
    }
}

//0x5275C0
void Zombie::UpdateZombieJalapenoHead(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (!mHasHead)
        return;

    if (mPhaseCounter == 0)
    {
        mApp->PlayFoley(FoleyType::FOLEY_JALAPENO_IGNITE);
        mApp->PlayFoley(FoleyType::FOLEY_JUICY);
        mBoard->DoFwoosh(mRow);
        mBoard->mShakeCounter = 12;
        mBoard->mShakeAmountX = 3;
        mBoard->mShakeAmountY = -4;

#ifdef DO_FIX_BUGS
        if (mMindControlled)
        {
            BurnRow(ownerNowNs, visualNowNs, animationNowNs, mRow);
        }
        else
        {
            // 格桶链枚举（2026-10-03 第二批）：原为全池扫 mRow==mRow
            // &&!NotOnGround 逐株 mPlantsEaten+++Die——行过滤改本行
            // 9 格×4 层链全成员（含影子株=原池可见集），NotOnGround 过滤
            // 逐字保留；Die 不摘链（ProcessDeleteQueue 摘），防御性前取；
            // 玉米炮双格双链两次命中，第二次由先死株的 NotOnGround 过滤
            // 兜住（=原单次命中）。
            for (int aCol = 0; aCol < MAX_GRID_SIZE_X; aCol++)
            {
                for (int aLayer = 0; aLayer < PLANT_CELL_COUNT; aLayer++)
                {
                    Plant* aPlant = mBoard->GetCellPlant(mRow, aCol, aLayer);
                    while (aPlant != nullptr)
                    {
                        Plant* aNext = mBoard->NextCellPlant(aPlant);  // 先取：Die 不摘链但防御性前取
                        //Rect aPlantRect = aPlant->GetPlantRect();
                        if (!aPlant->NotOnGround())
                        {
                            mBoard->mPlantsEaten++;
                            aPlant->Die(ownerNowNs, visualNowNs, animationNowNs);
                        }
                        aPlant = aNext;
                    }
                }
            }
        }
#else
        // 格桶链枚举（2026-10-03 第二批）：原为全池扫 mRow==mRow
        // &&!NotOnGround 逐株 mPlantsEaten+++Die——行过滤改本行
        // 9 格×4 层链全成员（含影子株=原池可见集），NotOnGround 过滤
        // 逐字保留；Die 不摘链（ProcessDeleteQueue 摘），防御性前取；
        // 玉米炮双格双链两次命中，第二次由先死株的 NotOnGround 过滤
        // 兜住（=原单次命中）。
        for (int aCol = 0; aCol < MAX_GRID_SIZE_X; aCol++)
        {
            for (int aLayer = 0; aLayer < PLANT_CELL_COUNT; aLayer++)
            {
                Plant* aPlant = mBoard->GetCellPlant(mRow, aCol, aLayer);
                while (aPlant != nullptr)
                {
                    Plant* aNext = mBoard->NextCellPlant(aPlant);  // 先取：Die 不摘链但防御性前取
                    //Rect aPlantRect = aPlant->GetPlantRect();
                    if (!aPlant->NotOnGround())
                    {
                        mBoard->mPlantsEaten++;
                        aPlant->Die(ownerNowNs, visualNowNs, animationNowNs);
                    }
                    aPlant = aNext;
                }
            }
        }
#endif

        DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
    }
}

//0x527750
void Zombie::UpdateZombieGatlingHead()
{
    if (!mHasHead)
        return;

    if (mPhaseCounter == 100)
    {
        Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);

        aHeadReanim->PlayReanim(ReanimTrackId::anim_shooting, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 38.0f);
    }
    else if (mPhaseCounter == 18 || mPhaseCounter == 35 || mPhaseCounter == 51 || mPhaseCounter == 68)
    {
        mApp->PlayFoley(FoleyType::FOLEY_THROW);

        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        int aTrackIndex = aBodyReanim->FindTrackIndex(ReanimTrackId::anim_head1);
        ReanimatorTransform aTransform;
        aBodyReanim->GetCurrentTransform(aTrackIndex, &aTransform);

        float aOriginX = (mX) + aTransform.mTransX - 9.0f;
        float aOriginY = (mY) + aTransform.mTransY + 6.0f;
#ifdef DO_FIX_BUGS
        if (mMindControlled)  // 魅惑修复
        {
            aOriginX += 90.0f * mScaleZombie;
            Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder, mRow, ProjectileType::PROJECTILE_PEA);
            aProjectile->mDamageRangeFlags = 1;
        }
        else
        {
            Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder, mRow, ProjectileType::PROJECTILE_ZOMBIE_PEA);
            aProjectile->mMotionType = ProjectileMotion::MOTION_BACKWARDS;
        }
#else
        Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder, mRow, ProjectileType::PROJECTILE_ZOMBIE_PEA);
        aProjectile->mMotionType = ProjectileMotion::MOTION_BACKWARDS;
#endif
    }
    else if (mPhaseCounter == 0)
    {
        Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);

        aHeadReanim->PlayReanim(ReanimTrackId::anim_head_idle, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 15.0f);
        mPhaseCounter = 150;
    }
}

//0x527AA0
void Zombie::UpdateZombieSquashHead(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (mHasHead && mIsEating && mZombiePhase == ZombiePhase::PHASE_SQUASH_PRE_LAUNCH)
    {
        StopEating(ownerNowNs);
        PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 20, 12.0f);
        mHasHead = false;

        Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);

        aHeadReanim->PlayReanim(ReanimTrackId::anim_jumpup, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
        aHeadReanim->mRenderOrder = mRenderOrder + 1;
        aHeadReanim->mOverlayMatrix.m02 = (mX) + 6.0f;
        aHeadReanim->mOverlayMatrix.m12 = (mY) - 21.0f;

        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName(ReanimTrackId::anim_head1);
        AttachmentDetach(aTrackInstance->mAttachmentID, ownerNowNs);
        aHeadReanim->mOverlayMatrix.m00 = 0.75f;
        aHeadReanim->mOverlayMatrix.m11 = 0.75f;
        aHeadReanim->mOverlayMatrix.m10 = 0.0f;

        mZombiePhase = ZombiePhase::PHASE_SQUASH_RISING;
        mPhaseCounter = 95;
    }

    if (mZombiePhase == ZombiePhase::PHASE_SQUASH_RISING)
    {
        int aDestX = mBoard->GridToPixelX(mBoard->PixelToGridXKeepOnBoard((mX), (mY)), mRow);
#ifdef DO_FIX_BUGS
        if (mMindControlled)
        {
            Zombie* aZombie = FindZombieTarget();
            if (aZombie)
            {
                aDestX = aZombie->ZombieTargetLeadX(ownerNowNs, 0.0f);
            }
            else
            {
                aDestX += 90.0f * mScaleZombie;
            }
        }
#endif
        int aPosX = TodAnimateCurveNs(500'000'000, 200'000'000, mPhaseCounter.mNanoseconds, 0, static_cast<int>(aDestX - mX), Curves::EaseInOut);
        int aPosY = TodAnimateCurveNs(500'000'000, 200'000'000, mPhaseCounter.mNanoseconds, 0, -20, Curves::EaseInOut);

        Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
        aHeadReanim->mOverlayMatrix.m02 = (mX) + aPosX + 6.0f;
        aHeadReanim->mOverlayMatrix.m12 = (mY) + aPosY - 21.0f;

        if (mPhaseCounter == 0)
        {
            aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);

            aHeadReanim->PlayReanim(ReanimTrackId::anim_jumpdown, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 60.0f);
            mZombiePhase = ZombiePhase::PHASE_SQUASH_FALLING;
            mPhaseCounter = 10;
        }
    }

    if (mZombiePhase == ZombiePhase::PHASE_SQUASH_FALLING)
    {
        int aPosY = TodAnimateCurveNs(100'000'000, 0, mPhaseCounter.mNanoseconds, -20, 74, Curves::Linear);
        int aDestX = mBoard->GridToPixelX(mBoard->PixelToGridXKeepOnBoard((mX), (mY)), mRow);
#ifdef DO_FIX_BUGS
        if (mMindControlled)
        {
            Zombie* aZombie = FindZombieTarget();
            if (aZombie)
            {
                aDestX = aZombie->ZombieTargetLeadX(ownerNowNs, 0.0f);
            }
            else
            {
                aDestX += 90.0f * mScaleZombie;
            }
        }
#endif

        Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
        aHeadReanim->mOverlayMatrix.m02 = (mX) + 6.0f + aDestX - (mX);
        aHeadReanim->mOverlayMatrix.m12 = (mY) - 21.0f + aPosY;

        if (mPhaseCounter == 2)
        {
#ifdef DO_FIX_BUGS
            if (mMindControlled)  // 魅惑修复
            {
                Rect aAttackRect(aDestX - 73, (mY) + 4, 65, 90);  // 具体数值未实测，待定

                Zombie* aZombie = nullptr;
                // 行段路径（2026-10-04 全量转换）：Boss‖mRow 候选集，apply-all 与序无关
                while (mBoard->IterateZombiesInRow(mRow, aZombie))
                {
                    if ((aZombie->mRow == mRow || ((ZombieFlagsOf(aZombie->mZombieType) & ZombieFlags::Boss) != 0)) && aZombie->EffectedByDamage(13U))
                    {
                        Rect aZombieRect = aZombie->GetZombieRect();
                        if (GetRectOverlap(aAttackRect, aZombieRect) > (aZombie->mZombieType == ZombieType::ZOMBIE_FOOTBALL ? -20 : 0))
                        {
                            aZombie->TakeDamage(ownerNowNs, visualNowNs, animationNowNs, 1800, 18U);
                        }
                    }
                }
            }
            else
            {
                SquishAllInSquare(ownerNowNs, visualNowNs, animationNowNs, mBoard->PixelToGridXKeepOnBoard((mX), (mY)), mRow, ZombieAttackType::ATTACKTYPE_CHEW);
            }
#else
            SquishAllInSquare(ownerNowNs, visualNowNs, animationNowNs, mBoard->PixelToGridXKeepOnBoard((mX), (mY)), mRow, ZombieAttackType::ATTACKTYPE_CHEW);
#endif
        }

        if (mPhaseCounter == 0)
        {
            mZombiePhase = ZombiePhase::PHASE_SQUASH_DONE_FALLING;
            mPhaseCounter = 100;

            mBoard->mShakeCounter = 12;
            mBoard->mShakeAmountX = 1;
            mBoard->mShakeAmountY = 4;
            mApp->PlayFoley(FoleyType::FOLEY_THUMP);
        }
    }

    if (mZombiePhase == ZombiePhase::PHASE_SQUASH_DONE_FALLING && mPhaseCounter == 0)
    {
        Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
        aHeadReanim->ReanimationDie();
        mSpecialHeadReanimID = ReanimationID::REANIMATIONID_NULL;

        TakeDamage(ownerNowNs, visualNowNs, animationNowNs, 1800, 9U);
    }
}

//0x527F20
void Zombie::BobsledCrash(std::int64_t ownerNowNs)
{
    mAltitude = 0.0f;
    mZombieRect = Rect(36, 0, 42, 115);
    mZombiePhase = ZombiePhase::PHASE_BOBSLED_CRASHING;
    mPhaseCounter = BOBSLED_CRASH_TIME;
    StartWalkAnim(ownerNowNs, 0);

    Reanimation* aLeaderReanim = mApp->ReanimationGet(mBodyReanimID);
    for (int i = 0; i < NUM_BOBSLED_FOLLOWERS; i++)
    {
        Zombie* aFollowerZombie = mBoard->ZombieGet(mFollowerZombieID[i]);
        aFollowerZombie->mZombiePhase = ZombiePhase::PHASE_BOBSLED_CRASHING;
        aFollowerZombie->mPhaseCounter = BOBSLED_CRASH_TIME;
        aFollowerZombie->mY = (GetPosYBasedOnRow(mRow));
        aFollowerZombie->mAltitude = 0.0f;
        aFollowerZombie->StartWalkAnim(ownerNowNs, 0);

        Reanimation* aFollowerReanim = mApp->ReanimationGet(aFollowerZombie->mBodyReanimID);
        if (aFollowerReanim)
        {
            aFollowerZombie->mVelX = mVelX;
            aFollowerReanim->SeekRatio(RandRangeFloat(0.0f, 1.0f));

            aFollowerReanim->ApplyRate(aLeaderReanim->mAnimRate);
        }
    }
}

//0x528050
void Zombie::UpdateZombieBobsled(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (mZombiePhase == ZombiePhase::PHASE_BOBSLED_CRASHING)
    {
        if (mPhaseCounter == 0)
        {
            mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
            if (GetBobsledPosition() == 0)
            {
                for (int i = 0; i < NUM_BOBSLED_FOLLOWERS; i++)
                {
                    Zombie* aZombie = mBoard->ZombieGet(mFollowerZombieID[i]);
                    aZombie->mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
                    mFollowerZombieID[i] = ZombieID::ZOMBIEID_NULL;
                    aZombie->PickRandomSpeed(ownerNowNs);
                }
                PickRandomSpeed(ownerNowNs);
            }
        }
        return;
    }

    if (mZombiePhase == ZombiePhase::PHASE_BOBSLED_SLIDING)
    {
        if (mPhaseCounter == 0)
        {
            mZombiePhase = ZombiePhase::PHASE_BOBSLED_BOARDING;
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_jump, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 20.0f);
        }
    }
    else
    {
        if (mZombiePhase != ZombiePhase::PHASE_BOBSLED_BOARDING)
            return;

        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        const double aPhase = static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs;
        int aPosition = GetBobsledPosition();
        if (aPosition == 1 || aPosition == 3)
        {
            mAltitude = TodAnimateCurveFloat(0.0, 1.0, aPhase, 8.0f, 18.0f, Curves::Linear);
        }
        else
        {
            mAltitude = TodAnimateCurveFloat(0.0, 1.0, aPhase, -9.0f, 18.0f, Curves::Linear);
        }
    }

    mBoard->mIceTimer[mRow] = std::max<Sexy::TimeCounter>(500, mBoard->mIceTimer[mRow]);
    if ((mX) + 10.0f < mBoard->mIceMinX[mRow] && GetBobsledPosition() == 0 && Sexy::gWholeLegacyTicks > 0)
    {
        TakeDamage(ownerNowNs, visualNowNs, animationNowNs, 6 * Sexy::gWholeLegacyTicks, 8U);
    }
}

//0x528240
void Zombie::DiggerLoseAxe(std::int64_t ownerNowNs)
{
    if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING)
    {
        mZombiePhase = ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE;
        mPhaseCounter = 200;
        SetAnimRate(ownerNowNs, 0.0f);
        UpdateAnimSpeed(ownerNowNs);
        AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_DIGGER_TUNNEL);
        StopZombieSound();
    }

    mHasObject = false;
    ReanimShowTrack(ReanimTrackId::Zombie_digger_pickaxe, RENDER_GROUP_HIDDEN);
    ReanimShowTrack(ReanimTrackId::Zombie_digger_dirt, RENDER_GROUP_HIDDEN);
}

//0x528310
void Zombie::UpdateZombieDigger(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    // C 形态 -> 匿名 lambda 函数指针表 kDiggerPhaseCases（仿 kBossPhaseCases
    // 范式）：行序 = ZombiePhase 枚举序 PHASE_DIGGER_TUNNELING..
    // PHASE_DIGGER_CUTSCENE（8 相位连续）；WALKING/WALKING_WITHOUT_AXE/
    // CUTSCENE 槽 nullptr（原无 case）；RISING 与 RISE_WITHOUT_AXE 两支
    // 末段相位迁移/动画参数不同，非共体，各占一槽。
    using DiggerPhaseFn = void (*)(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie&);
    static constexpr DiggerPhaseFn kDiggerPhaseCases[(int)ZombiePhase::PHASE_DIGGER_CUTSCENE - (int)ZombiePhase::PHASE_DIGGER_TUNNELING + 1] = {
        // 原 if PHASE_DIGGER_TUNNELING
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z)
        {
            if ((z.mX) < 10.0f)
            {
                z.mAltitude = -120.0f;
                z.mZombiePhase = ZombiePhase::PHASE_DIGGER_RISING;
                z.mPhaseCounter = 130;
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_drill, ReanimLoopType::REANIM_LOOP, 0, 20.0f);

                z.mApp->PlayFoley(FoleyType::FOLEY_DIRT_RISE);
                z.mApp->PlayFoley(FoleyType::FOLEY_WAKEUP);
                AttachmentDetachCrossFadeParticleType(z.mAttachmentID, ParticleEffect::PARTICLE_DIGGER_TUNNEL);
                z.StopZombieSound();

                z.mApp->AddTodParticle((z.mX) + 60.0f, (z.mY) + 118.0f, z.mRenderOrder + 1, ParticleEffect::PARTICLE_DIGGER_RISE, ownerNowNs);
                Reanimation* aDirtReanim = z.mApp->AddReanimation((z.mX) + 13.0f, (z.mY) + 97.0f, z.mRenderOrder + 1, ReanimationType::REANIM_DIGGER_DIRT, visualNowNs);

                aDirtReanim->ApplyRate(24.0f);
            }
        },  // PHASE_DIGGER_TUNNELING
        // 原 else-if PHASE_DIGGER_RISING
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z)
        {
            if (z.mPhaseCounter > 40)
            {
                z.mAltitude = TodAnimateCurveNs(1'300'000'000, 400'000'000, z.mPhaseCounter.mNanoseconds, -120, 20, Curves::EaseOut);
            }
            else
            {
                z.mAltitude = TodAnimateCurveNs(300'000'000, 0, z.mPhaseCounter.mNanoseconds, 20, 0, Curves::EaseIn);
            }

            if (z.mPhaseCounter == 30)
            {
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_landing, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 12.0f);
            }

            if (z.mPhaseCounter == 0)
            {
                z.mAltitude = 0.0f;
                z.mZombiePhase = ZombiePhase::PHASE_DIGGER_STUNNED;
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_dizzy, ReanimLoopType::REANIM_LOOP, 10, 12.0f);
            }
        },  // PHASE_DIGGER_RISING
        // 原 else-if PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z)
        {
            if (z.mPhaseCounter == 150)
            {
                z.AddAttachedReanim(ownerNowNs, visualNowNs, animationNowNs, 23, 93, ReanimationType::REANIM_ZOMBIE_SURPRISE);
            }

            if (z.mPhaseCounter == 0)
            {
                z.mAltitude = -120.f;
                z.mZombiePhase = ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE;
                z.mPhaseCounter = 130;
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_landing, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 0.0f);

                z.mApp->PlayFoley(FoleyType::FOLEY_DIRT_RISE);
                z.mApp->AddTodParticle((z.mX) + 60.0f, (z.mY) + 118.0f, z.mRenderOrder + 1, ParticleEffect::PARTICLE_DIGGER_RISE, ownerNowNs);
                Reanimation* aDirtReanim = z.mApp->AddReanimation((z.mX) + 13.0f, (z.mY) + 97.0f, z.mRenderOrder + 1, ReanimationType::REANIM_DIGGER_DIRT, visualNowNs);

                aDirtReanim->ApplyRate(24.0f);
            }
        },  // PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE
        // 原 else-if PHASE_DIGGER_RISE_WITHOUT_AXE
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z)
        {
            if (z.mPhaseCounter > 40)
            {
                z.mAltitude = TodAnimateCurveNs(1'300'000'000, 400'000'000, z.mPhaseCounter.mNanoseconds, -120, 20, Curves::EaseOut);
            }
            else
            {
                z.mAltitude = TodAnimateCurveNs(300'000'000, 0, z.mPhaseCounter.mNanoseconds, 20, 0, Curves::EaseIn);
            }

            if (z.mPhaseCounter == 30)
            {
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_landing, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
            }

            if (z.mPhaseCounter == 0)
            {
                z.mAltitude = 0.0f;
                z.mZombiePhase = ZombiePhase::PHASE_DIGGER_WALKING_WITHOUT_AXE;
                z.StartWalkAnim(ownerNowNs, 20);
            }
        },  // PHASE_DIGGER_RISE_WITHOUT_AXE
        // 原 else-if PHASE_DIGGER_STUNNED
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z)
        {
            Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
            if (aBodyReanim->mLoopCount > 1)
            {
                z.mZombiePhase = ZombiePhase::PHASE_DIGGER_WALKING;
                z.StartWalkAnim(ownerNowNs, 20);
            }
        },  // PHASE_DIGGER_STUNNED
        nullptr,  // PHASE_DIGGER_WALKING（原无 case）
        nullptr,  // PHASE_DIGGER_WALKING_WITHOUT_AXE（原无 case）
        nullptr,  // PHASE_DIGGER_CUTSCENE（原无 case）
    };
    static_assert(sizeof(kDiggerPhaseCases) / sizeof(kDiggerPhaseCases[0]) ==
        (int)ZombiePhase::PHASE_DIGGER_CUTSCENE - (int)ZombiePhase::PHASE_DIGGER_TUNNELING + 1,
        "kDiggerPhaseCases 槽位须与 ZombiePhase DIGGER 段同步");

    const unsigned aPhaseIndex = static_cast<unsigned>(mZombiePhase) - static_cast<unsigned>(ZombiePhase::PHASE_DIGGER_TUNNELING);
    if (aPhaseIndex < sizeof(kDiggerPhaseCases) / sizeof(kDiggerPhaseCases[0]) &&
        kDiggerPhaseCases[aPhaseIndex] != nullptr)
    {
        kDiggerPhaseCases[aPhaseIndex](ownerNowNs, visualNowNs, animationNowNs, *this);
    }
}

//0x528760
ZombieID Zombie::SummonBackupDancer(std::int64_t ownerNowNs, int theRow, int thePosX)
{
    if (!mBoard->RowCanHaveZombieType(theRow, ZombieType::ZOMBIE_BACKUP_DANCER))
        return ZombieID::ZOMBIEID_NULL;

    Zombie* aZombie = mBoard->AddZombie(ZombieType::ZOMBIE_BACKUP_DANCER, mFromWave);
    if (aZombie == nullptr)
        return ZombieID::ZOMBIEID_NULL;

    { aZombie->mX = (thePosX); aZombie->mRootBindingGeneration = 0; }
    aZombie->mY = (GetPosYBasedOnRow(theRow));
    aZombie->SetRow(theRow);



    aZombie->mAltitude = ZOMBIE_BACKUP_DANCER_RISE_HEIGHT;
    aZombie->mZombiePhase = ZombiePhase::PHASE_DANCER_RISING;
    aZombie->mPhaseCounter = 150;
    aZombie->mRelatedZombieID = mBoard->ZombieGetID(this);

    aZombie->SetAnimRate(ownerNowNs, 0.0f);
    aZombie->mMindControlled = mMindControlled;

    int aParticleX = (int)(aZombie->mX) + 60;
    int aParticleY = (int)(aZombie->mY) + 110;
    if (aZombie->IsOnHighGround())
    {
        aParticleY -= HIGH_GROUND_HEIGHT;
    }
    int aRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, theRow, 0);
    mApp->AddTodParticle(aParticleX, aParticleY, aRenderOrder, ParticleEffect::PARTICLE_DANCER_RISE, ownerNowNs);
    mApp->PlayFoley(FoleyType::FOLEY_GRAVESTONE_RUMBLE);

    return mBoard->ZombieGetID(aZombie);
}

//0x528970
// GOTY @Patoke: 0x539000
void Zombie::SummonBackupDancers(std::int64_t ownerNowNs)
{
    if (!mHasHead)
        return;

    // 伴舞召唤偏移表（段表化战役 2026-10-07）：行序 = 循环变量 i，列 =
    // {行偏移, X 偏移}。for 上界 NUM_BACKUP_DANCERS=4 封闭值域，原
    // default: TOD_ASSERT 为纯防御——i 由循环条件保证 0..3，直下标。
    static constexpr int kBackupDancerOffset[NUM_BACKUP_DANCERS][2] = {
        { -1, 0 },    // 0：上一行同列
        { 1, 0 },     // 1：下一行同列
        { 0, -100 },  // 2：同行左移
        { 0, 100 },   // 3：同行右移
    };
    static_assert(sizeof(kBackupDancerOffset) / sizeof(kBackupDancerOffset[0]) == NUM_BACKUP_DANCERS,
        "伴舞偏移槽位须与 NUM_BACKUP_DANCERS 同步");

    for (int i = 0; i < NUM_BACKUP_DANCERS; i++)
    {
        if (mBoard->ZombieTryToGet(mFollowerZombieID[i]) == nullptr)
        {
            const int aRow = mRow + kBackupDancerOffset[i][0];
            const int aPosX = (mX) + kBackupDancerOffset[i][1];
            mFollowerZombieID[i] = SummonBackupDancer(ownerNowNs, aRow, aPosX);
        }
    }
}

//0x528A50
// GOTY @Patoke: 0x5390E0
bool Zombie::NeedsMoreBackupDancers()
{
    for (int i = 0; i < NUM_BACKUP_DANCERS; i++)
    {
        if (mBoard->ZombieTryToGet(mFollowerZombieID[i]) == nullptr)
        {
            if (i == 0 && !mBoard->RowCanHaveZombieType(mRow - 1, ZombieType::ZOMBIE_BACKUP_DANCER))
            {
                continue;
            }

            if (i == 1 && !mBoard->RowCanHaveZombieType(mRow + 1, ZombieType::ZOMBIE_BACKUP_DANCER))
            {
                continue;
            }

            return true;
        }
    }

    return false;
}

//0x528B00
// GOTY @Patoke: 0x53919E
void Zombie::PlayZombieReanim(std::int64_t ownerNowNs, ReanimTrackId theTrackId, ReanimLoopType theLoopType, int theBlendTime, float theAnimRate)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (aBodyReanim == nullptr)
        return;

    aBodyReanim->PlayReanim(theTrackId, theLoopType, theBlendTime, theAnimRate);
    if (theAnimRate != 0.0f)
    {
        mOriginalAnimRate = theAnimRate;
    }
    UpdateAnimSpeed(ownerNowNs);
}


//0x528B80
// GOTY @Patoke: 0x539210
void Zombie::UpdateZombieBackupDancer(std::int64_t ownerNowNs)
{
    if (mIsEating)
        return;

    if (mZombiePhase == ZombiePhase::PHASE_DANCER_RISING)
    {
        mAltitude = TodAnimateCurveNs(1'500'000'000, 0, mPhaseCounter.mNanoseconds, ZOMBIE_BACKUP_DANCER_RISE_HEIGHT, 0, Curves::Linear);

        if (mPhaseCounter != 0)
            return;

        if (IsOnHighGround())
        {
            mAltitude = HIGH_GROUND_HEIGHT;
        }
    }

    ZombiePhase aDancerPhase = GetDancerPhase(ownerNowNs);
    if (aDancerPhase != mZombiePhase)
    {
        // Only a changed target phase restarts the animation. GetDancerPhase
        // returns the four dance phases; unrelated zombie phases do not enter
        // this dispatch as the old synthetic 94%-miss benchmark assumed.
        switch (aDancerPhase)
        {
        case ZombiePhase::PHASE_DANCER_DANCING_LEFT:
            mZombiePhase = aDancerPhase;
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_walk, ReanimLoopType::REANIM_LOOP, 10, 0.0f);
            break;

        case ZombiePhase::PHASE_DANCER_WALK_TO_RAISE:
            mZombiePhase = aDancerPhase;
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_armraise, ReanimLoopType::REANIM_LOOP, 10, 18.0f);
            mApp->ReanimationTryToGet(mBodyReanimID)->SeekRatio(0.6f);
            break;

        case ZombiePhase::PHASE_DANCER_RAISE_LEFT_1:
        case ZombiePhase::PHASE_DANCER_RAISE_RIGHT_1:
        case ZombiePhase::PHASE_DANCER_RAISE_LEFT_2:
        case ZombiePhase::PHASE_DANCER_RAISE_RIGHT_2:
            mZombiePhase = aDancerPhase;
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_armraise, ReanimLoopType::REANIM_LOOP, 10, 18.0f);
            break;
        default:
            break;
        }
    }
}

//0x528CA0
// GOTY @Patoke: 0x539360
void Zombie::UpdateZombieDancer(std::int64_t ownerNowNs)
{
    if (mIsEating)
        return;

    if (mSummonCounter > 0)
    {
        mSummonCounter--;
        if (mSummonCounter == 0)
        {
            if (GetDancerFrame(ownerNowNs) == 12 && mHasHead && (mX) < 700.0f)
            {
                mZombiePhase = ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_WITH_LIGHT;
                PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_point, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
            }
            else
            {
                mSummonCounter = 1;
            }
        }
    }

    if (mZombiePhase == ZombiePhase::PHASE_DANCER_DANCING_IN)
    {
        if (mHasHead && mPhaseCounter == 0)
        {
            mZombiePhase = ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS;
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_point, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
            PickRandomSpeed(ownerNowNs);
        }
    }
    else if (mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS || mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_WITH_LIGHT)
    {
        Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
        if (aBodyReanim->mLoopCount > 0)
        {
            if (mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS && mBoard->CountZombiesOnScreen() <= 15)
            {
                mApp->PlayFoley(FoleyType::FOLEY_DANCER);
            }

            SummonBackupDancers(ownerNowNs);
            mZombiePhase = ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_HOLD;
            mPhaseCounter = 200;
        }
    }
    else
    {
        if (mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_HOLD)
        {
            if (mPhaseCounter != 0)
                return;

            mZombiePhase = ZombiePhase::PHASE_DANCER_DANCING_LEFT;
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_walk, ReanimLoopType::REANIM_LOOP, 20, 0.0f);
        }

        ZombiePhase aDancerPhase = GetDancerPhase(ownerNowNs);
        if (aDancerPhase != mZombiePhase)
        {
            // Dispatch only a changed target dance phase. The archived random
            // phase-domain benchmark did not model this outer state-change gate.
            switch (aDancerPhase)
            {
            case ZombiePhase::PHASE_DANCER_DANCING_LEFT:
                mZombiePhase = aDancerPhase;
                PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_walk, ReanimLoopType::REANIM_LOOP, 10, 0.0f);
                break;

            case ZombiePhase::PHASE_DANCER_WALK_TO_RAISE:
                mZombiePhase = aDancerPhase;
                PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_armraise, ReanimLoopType::REANIM_LOOP, 10, 18.0f);
                mApp->ReanimationTryToGet(mBodyReanimID)->SeekRatio(0.6f);
                break;

            case ZombiePhase::PHASE_DANCER_RAISE_LEFT_1:
            case ZombiePhase::PHASE_DANCER_RAISE_RIGHT_1:
            case ZombiePhase::PHASE_DANCER_RAISE_LEFT_2:
            case ZombiePhase::PHASE_DANCER_RAISE_RIGHT_2:
                mZombiePhase = aDancerPhase;
                PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_armraise, ReanimLoopType::REANIM_LOOP, 10, 18.0f);
                break;
            default:
                break;
            }
        }

        if (mHasHead && mSummonCounter == 0 && NeedsMoreBackupDancers())
        {
            mSummonCounter = 100;
        }
    }
}

//0x528EC0
void Zombie::UpdateZombieRiseFromGrave()
{
    if (mInPool)
    {
        mAltitude = TodAnimateCurveNs(500'000'000, 0, mPhaseCounter.mNanoseconds, -150, -40, Curves::Linear) * mScaleZombie;
    }
    else
    {
        mAltitude = TodAnimateCurveNs(500'000'000, 0, mPhaseCounter.mNanoseconds, -200, 0, Curves::Linear);
    }

    if (mPhaseCounter == 0)
    {
        mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;

        if (IsOnHighGround())
        {
            mAltitude = HIGH_GROUND_HEIGHT;
        }

        if (mInPool)
        {
            ReanimIgnoreClipRect(ReanimTrackId::Zombie_duckytube, true);
            ReanimIgnoreClipRect(ReanimTrackId::Zombie_whitewater, true);
            ReanimIgnoreClipRect(ReanimTrackId::Zombie_outerarm_hand, true);
            ReanimIgnoreClipRect(ReanimTrackId::None /* Zombie_innerarm3 不在映射表：原版回退轨道 0 */, true);
        }
    }
}

void Zombie::DragUnder(std::int64_t ownerNowNs)
{
    mZombieHeight = ZombieHeight::HEIGHT_DRAGGED_UNDER;
    StopEating(ownerNowNs);
    ReanimReenableClipping();
}

//0x528F70
bool Zombie::ZombiquariumFindClosestBrain(std::int64_t ownerNowNs)
{
    if (mBoard->HasLevelAwardDropped() || mBodyHealth > 150)
        return false;

    GridItem* aBrainClosest = nullptr;
    float aDistanceClosest = 0.0f;
    GridItem* aGridItem = nullptr;
    while (mBoard->IterateGridItems(aGridItem))
    {
        if (aGridItem->mGridItemType == GridItemType::GRIDITEM_BRAIN &&
            ownerNowNs - aGridItem->mStateStartTimeNs >= 150'000'000)
        {
            float aDistance = Distance2D(aGridItem->mPosX + 15.0f, aGridItem->mPosY + 15.0f, (mX) + 50.0f, (mY) + 40.0f);
            if (aBrainClosest == nullptr || aDistance < aDistanceClosest)
            {
                aDistanceClosest = aDistance;
                aBrainClosest = aGridItem;
            }
        }
    }

    if (aBrainClosest)
    {
        if (aDistanceClosest < 50.0f)
        {
            aBrainClosest->GridItemDie();
            mApp->PlayFoley(FoleyType::FOLEY_SLURP);

            mBodyHealth += 200;
            if (mBodyHealth > mBodyMaxHealth)
            {
                mBodyHealth = mBodyMaxHealth;
            }

            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_aquarium_bite, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 24.0f);
            mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_BITE;
            mPhaseCounter = 200;
            return false;
        }

        float aRangeY = aBrainClosest->mPosY + 15.0f - ((mY) + 40.0f);
        float aRangeX = aBrainClosest->mPosX + 15.0f - ((mX) + 50.0f);
        mVelZ = atan2(aRangeY, aRangeX);
        if (mVelZ < 0.0f)
        {
            mVelZ += PI * 2;
        }

        mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_ACCEL;
        return true;
    }

    return false;
}

//0x5291E0
void Zombie::UpdateZombiquarium(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    if (IsDeadOrDying())
        return;

    //float& num2 = mVelX; // unused
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (mZombiePhase == ZombiePhase::PHASE_ZOMBIQUARIUM_BITE)
    {
        if (aBodyReanim->mLoopCount > 0)
        {
            float aAnimRate = RandRangeFloat(8.0f, 10.0f);
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_aquarium_swim, ReanimLoopType::REANIM_LOOP, 20, aAnimRate);

            mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_DRIFT;
            mPhaseCounter = 100;
        }
    }
    else if (!ZombiquariumFindClosestBrain(ownerNowNs) && mPhaseCounter == 0)
    {
        int aPhaseHit = Rand(7);
        if (aPhaseHit <= 4)
        {
            mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_ACCEL;
            mVelZ = RandRangeFloat(0.0f, PI * 2);
            mPhaseCounter = RandRangeInt(300, 1000);

            aBodyReanim->ApplyRate(RandRangeFloat(15.0f, 20.0f));
        }
        //else if (aPhaseHit == 4)
        //{
        //    mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_DRIFT;
        //    mVelZ = PI * 1.5f;
        //    mPhaseCounter = RandRangeInt(300, 1000);
        //    aBodyReanim->ApplyRate(RandRangeFloat(8.0f, 10.0f));
        //}
        else if (aPhaseHit == 5)
        {
            mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_BACK_AND_FORTH;
            mVelZ = 0.0f;
            mPhaseCounter = RandRangeInt(300, 1000);

            aBodyReanim->ApplyRate(RandRangeFloat(15.0f, 20.0f));
        }
        else
        {
            mZombiePhase = ZombiePhase::PHASE_ZOMBIQUARIUM_BACK_AND_FORTH;
            mVelZ = PI;
            mPhaseCounter = RandRangeInt(300, 1000);

            aBodyReanim->ApplyRate(RandRangeFloat(15.0f, 20.0f));
        }
    }

    float aVelX = cos(mVelZ);
    float aVelY = sin(mVelZ);
    bool aIsOutOfBounds = false;
    if ((mX) < 0.0f && aVelX < 0.0f)
    {
        aIsOutOfBounds = true;
    }
    else if ((mX) > 680.0f && aVelX > 0.0f)
    {
        aIsOutOfBounds = true;
    }
    else if ((mY) < 100.0f && aVelY < 0.0f)
    {
        aIsOutOfBounds = true;
    }
    else if ((mY) > 400.0f && aVelY > 0.0f)
    {
        aIsOutOfBounds = true;
    }

    float aMaxSpeed = 0.5f;
    if (aIsOutOfBounds)
    {
        aMaxSpeed = mVelX * 0.3f;
        mPhaseCounter = std::min<Sexy::TimeCounter>(100, mPhaseCounter);
    }
    else if (mZombiePhase == ZombiePhase::PHASE_ZOMBIQUARIUM_ACCEL)
    {
        aMaxSpeed = 0.5f;
    }
    else if (mZombiePhase == ZombiePhase::PHASE_ZOMBIQUARIUM_BACK_AND_FORTH)
    {
        if ((mX) < 200.0f && aVelX < 0.0f)
        {
            mVelZ = 0.0f;
        }

        if ((mX) > 550.0f && aVelX > 0.0f)
        {
            mVelZ = PI;
        }

        aMaxSpeed = 0.3f;
    }
    else if (mZombiePhase == ZombiePhase::PHASE_ZOMBIQUARIUM_DRIFT || mZombiePhase == ZombiePhase::PHASE_ZOMBIQUARIUM_BITE)
    {
        aMaxSpeed = 0.05f;
    }

    mVelX = std::min(aMaxSpeed, mVelX + Sexy::UpdateDelta(0.01f));
    aVelX *= mVelX;
    aVelY *= mVelX;
    mRootBindingGeneration = 0;
    mX += Sexy::UpdateDelta(aVelX);
    mY += Sexy::UpdateDelta(aVelY);

    if (!mBoard->HasLevelAwardDropped())
    {
        if (mSummonCounter > 0)
        {
            mSummonCounter--;
            if (mSummonCounter == 0)
            {
                mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
                mBoard->AddCoin((mX) + 50, (mY) + 40, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT);
                mSummonCounter = RandRangeInt(1000, 1500);
            }
        }

        if (ZombieAgeCrossedPeriod(aStatusNowNs-mBirthTimeNs, Sexy::gUpdateIntervalNs, 1'000'000'000))
        {
            TakeDamage(ownerNowNs, visualNowNs, animationNowNs, 10, 8U);
            if (IsDeadOrDying())
            {
                mApp->PlaySample(SOUND_ZOMBAQUARIUM_DIE);
            }
        }
    }
}

//0x5295F0
void Zombie::UpdateZombiePool(std::int64_t ownerNowNs)
{
    if (mZombieHeight == ZombieHeight::HEIGHT_OUT_OF_POOL)
    {
        AdvanceVelocity(mAltitude, mZombieType == ZombieType::ZOMBIE_SNORKEL ? 2.0f : 1.0f,
            Sexy::gUpdateIntervalNs, ownerNowNs, mAltitudeMotion);

        if (mAltitude >= 0.0f)
        {
            mAltitude = 0.0f;
            mAltitudeMotion = {};
            mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
            mInPool = false;
        }
    }
    else if (mZombieHeight == ZombieHeight::HEIGHT_IN_TO_POOL)
    {
        AdvanceVelocity(mAltitude, -1.0f, Sexy::gUpdateIntervalNs, ownerNowNs, mAltitudeMotion);
        int aDepth = -40 * mScaleZombie;
        if (mAltitude <= aDepth)
        {
            mAltitude = aDepth;
            mAltitudeMotion = {};
            mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
            StartWalkAnim(ownerNowNs, 0);
        }
    }
    else if (mZombieHeight == ZombieHeight::HEIGHT_DRAGGED_UNDER)
    {
        AdvanceVelocity(mAltitude, -1.0f, Sexy::gUpdateIntervalNs, ownerNowNs, mAltitudeMotion);
    }
}

//0x5296E0
void Zombie::UpdateZombieHighGround(std::int64_t ownerNowNs)
{
    if (mZombieType == ZombieType::ZOMBIE_POGO)
        return;

    if (mZombieHeight == ZombieHeight::HEIGHT_UP_TO_HIGH_GROUND)
    {
        AdvanceVelocity(mAltitude, 1.0f, Sexy::gUpdateIntervalNs, ownerNowNs, mAltitudeMotion);
        if (mAltitude >= HIGH_GROUND_HEIGHT)
        {
            mAltitude = HIGH_GROUND_HEIGHT;
            mAltitudeMotion = {};
            mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
        }
    }
    else if (mZombieHeight == ZombieHeight::HEIGHT_DOWN_OFF_HIGH_GROUND)
    {
        AdvanceVelocity(mAltitude, -1.0f, Sexy::gUpdateIntervalNs, ownerNowNs, mAltitudeMotion);
        if (mAltitude <= 0.0f)
        {
            mAltitude = 0.0f;
            mAltitudeMotion = {};
            mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
            mOnHighGround = false;
        }
    }
}

//0x529770
void Zombie::UpdateZombieFalling(std::int64_t ownerNowNs, float theAdditionalRate)
{
    const float fallingRate = mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT ? -2.0f : -1.0f;
    const float rate = fallingRate + theAdditionalRate;
    AdvanceVelocity(mAltitude, rate, Sexy::gUpdateIntervalNs, ownerNowNs, mAltitudeMotion);

    int aGroundHeight = 0;
    if (IsOnHighGround())
    {
        aGroundHeight = HIGH_GROUND_HEIGHT;
    }
    if (theAdditionalRate != 0)
    {
        // Falling runs before the flyer's additional gravity. Find the first
        // 10ms tick whose intermediate falling sample reaches the ground.
        // At that tick the legacy result is ground + additionalRate; carry
        // the remaining time on this new linear segment rather than clamping
        // to ground + additionalRate * the outer update's duration.
        const double eventTicks = std::max(1.0, std::ceil(1.0 +
            (mAltitudeMotion.origin + fallingRate - aGroundHeight) / -double(rate)));
        const double eventNs = eventTicks * 10'000'000.0;
        const auto ageNs = ReanimClock::Subtract(ownerNowNs, mAltitudeMotion.startTimeNs);
        if (eventNs <= double(ageNs))
        {
            const auto remaining = ageNs - static_cast<std::int64_t>(eventNs);
            mAltitudeMotion.origin = static_cast<float>(aGroundHeight + theAdditionalRate);
            mAltitudeMotion.startTimeNs = ReanimClock::Subtract(ownerNowNs, remaining);
            mAltitudeMotion.rate = theAdditionalRate;
            mAltitude = CheckedPosition(mAltitudeMotion.origin +
                double(theAdditionalRate) * double(remaining) / 10'000'000.0);
            mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
        }
    }
    else if (mAltitude <= aGroundHeight)
    {
        mAltitude = aGroundHeight;
        mAltitudeMotion = {};
        mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;
    }
}

//0x5297F0
// GOTY @Patoke: 0x522CE0
void Zombie::OverrideParticleScale(TodParticleSystem* aParticle)
{
    if (aParticle)
    {
        aParticle->OverrideScale(ParticleEmitterRanges::All, mScaleZombie);
    }
}

//0x529810
// GOTY @Patoke: 0x539F60
void Zombie::OverrideParticleColor(std::int64_t ownerNowNs, TodParticleSystem* aParticle)
{
    const auto aStatusNowNs = ownerNowNs;

    if (aParticle)
    {
        if (mMindControlled)
        {
            aParticle->OverrideColor(ParticleEmitterRanges::All, ZOMBIE_MINDCONTROLLED_COLOR);
            aParticle->OverrideExtraAdditiveDraw(ParticleEmitterRanges::All, true);
        }
        else if (mChilledEndTimeNs > aStatusNowNs || mIceTrapEndTimeNs > aStatusNowNs)
        {
            aParticle->OverrideColor(ParticleEmitterRanges::All, Color(75, 75, 255, 255));
            aParticle->OverrideExtraAdditiveDraw(ParticleEmitterRanges::All, true);
        }
    }
}

//0x529870
// GOTY @Patoke: 0x539FC0
void Zombie::DropFlag(std::int64_t ownerNowNs)
{
    // 1073 原版 DropFlag(ownerNowNs, 00539FC0) 对 ZOMBIE_FLAG 与 ZOMBIE_ZOMBATAR 一视
    // 同仁：存有 Zombatar 后，场上旗僵尸在 ZombieInitialize 已被改写为
    // 0x21，漏掉该类型会导致旗子不掉落、flaghand 不隐藏。
    if ((mZombieType != ZombieType::ZOMBIE_FLAG &&
            mZombieType != ZombieType::ZOMBIE_ZOMBATAR) || !mHasObject)
        return;

    mApp->RemoveReanimation(mSpecialHeadReanimID);
    ReanimShowRange(ReanimTrackRanges::anim_innerarm, RENDER_GROUP_NORMAL);
    ReanimShowTrack(ReanimTrackId::Zombie_flaghand, RENDER_GROUP_HIDDEN);
    ReanimShowTrack(ReanimTrackId::Zombie_innerarm_screendoor, RENDER_GROUP_HIDDEN);
    mHasObject = false;

    float aFlagPosX, aFlagPosY;
    GetTrackPosition(ReanimTrackId::Zombie_flaghand, aFlagPosX, aFlagPosY);
    TodParticleSystem* aParticle = mApp->AddTodParticle(aFlagPosX + 6.0f, aFlagPosY - 45.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZOMBIE_FLAG, ownerNowNs);
    OverrideParticleColor(ownerNowNs, aParticle);
    OverrideParticleScale(aParticle);
}

//0x529970
void Zombie::DropPole()
{
    if (mZombieType != ZombieType::ZOMBIE_POLEVAULTER)
        return;

    ReanimShowRange(ReanimTrackRanges::Zombie_polevaulter_innerarm, RENDER_GROUP_HIDDEN);
    ReanimShowRange(ReanimTrackRanges::Zombie_polevaulter_innerhand, RENDER_GROUP_HIDDEN);
    ReanimShowRange(ReanimTrackRanges::Zombie_polevaulter_pole, RENDER_GROUP_HIDDEN);
}

bool Zombie::CanLoseBodyParts()
{
    return
        mZombieType != ZombieType::ZOMBIE_ZAMBONI &&
        mZombieType != ZombieType::ZOMBIE_BUNGEE &&
        mZombieType != ZombieType::ZOMBIE_CATAPULT &&
        mZombieType != ZombieType::ZOMBIE_GARGANTUAR &&
        mZombieType != ZombieType::ZOMBIE_REDEYE_GARGANTUAR &&
        mZombieType != ZombieType::ZOMBIE_BOSS &&
        mZombieHeight != ZombieHeight::HEIGHT_ZOMBIQUARIUM &&
        !IsFlying() &&
        !IsBobsledTeamWithSled();
}

void Zombie::SetupReanimForLostHead()
{
    ReanimShowRange(ReanimTrackRanges::anim_head, RENDER_GROUP_HIDDEN);
    ReanimShowRange(ReanimTrackRanges::anim_hair, RENDER_GROUP_HIDDEN);
    ReanimShowRange(ReanimTrackRanges::anim_tongue, RENDER_GROUP_HIDDEN);
}

namespace
{
struct ZombieHeadParticleVariant
{
    std::uint32_t mNormal;
    std::uint32_t mMowered;
    std::uint32_t mPool;
};

#define ZOMBIE_HEAD_PARTICLE_VARIANT(type, suffix) \
    aVariants[ZombieType::ZOMBIE_##type] = { static_cast<std::uint32_t>(ParticleVariants::ZombieHead::ZombieHead##suffix), \
      static_cast<std::uint32_t>(ParticleVariants::MoweredZombieHead::MoweredZombieHead##suffix), \
      static_cast<std::uint32_t>(ParticleVariants::ZombieHeadPool::ZombieHeadPool##suffix) }
constexpr auto kZombieHeadParticleVariants = [] {
    std::array<ZombieHeadParticleVariant, ZombieType::NUM_ZOMBIE_TYPES> aVariants{};
    ZOMBIE_HEAD_PARTICLE_VARIANT(POLEVAULTER, Polevaulter);
    ZOMBIE_HEAD_PARTICLE_VARIANT(FOOTBALL, Football);
    ZOMBIE_HEAD_PARTICLE_VARIANT(DANCER, Dancer);
    ZOMBIE_HEAD_PARTICLE_VARIANT(BACKUP_DANCER, BackupDancer);
    ZOMBIE_HEAD_PARTICLE_VARIANT(SNORKEL, Snorkel);
    ZOMBIE_HEAD_PARTICLE_VARIANT(BOBSLED, Bobsled);
    ZOMBIE_HEAD_PARTICLE_VARIANT(DOLPHIN_RIDER, DolphinRider);
    ZOMBIE_HEAD_PARTICLE_VARIANT(DIGGER, Digger);
    ZOMBIE_HEAD_PARTICLE_VARIANT(YETI, Yeti);
    ZOMBIE_HEAD_PARTICLE_VARIANT(LADDER, Ladder);
    ZOMBIE_HEAD_PARTICLE_VARIANT(IMP, Imp);
    return aVariants;
}();
#undef ZOMBIE_HEAD_PARTICLE_VARIANT

constexpr std::uint32_t GetZombieHeadParticleVariant(ParticleEffect theBase, ZombieType theType)
{
    const unsigned aTypeIndex = static_cast<unsigned>(theType);
    if (aTypeIndex >= kZombieHeadParticleVariants.size())
    {
        return 0;
    }
    const ZombieHeadParticleVariant& aVariant = kZombieHeadParticleVariants[aTypeIndex];
    switch (theBase)
    {
    case ParticleEffect::PARTICLE_ZOMBIE_HEAD: return aVariant.mNormal;
    case ParticleEffect::PARTICLE_MOWERED_ZOMBIE_HEAD: return aVariant.mMowered;
    case ParticleEffect::PARTICLE_ZOMBIE_HEAD_POOL: return aVariant.mPool;
    default: return 0;
    }
}
} // namespace

//0x529A30
// GOTY @Patoke: 0x53A1DB
void Zombie::DropHead(std::int64_t ownerNowNs, unsigned int theDamageFlags)
{
    const auto aStatusNowNs = ownerNowNs;

    if (!CanLoseBodyParts() || !mHasHead)
        return;

    if (mButteredEndTimeNs > aStatusNowNs)
    {
        mButteredEndTimeNs = 0;
        UpdateAnimSpeed(ownerNowNs);
    }

    mHasHead = false;
    SetupReanimForLostHead();
    if (TestBit(theDamageFlags, DamageFlags::DAMAGE_DOESNT_LEAVE_BODY))
    {
        return;
    }

    if (Zombie::IsZombotany(mZombieType))
    {
        mApp->ReanimationGet(mSpecialHeadReanimID)->ReanimationDie();
        mSpecialHeadReanimID = ReanimationID::REANIMATIONID_NULL;
        return;
    }

    int aRenderOrder = mRenderOrder + 1;
    ZombieDrawPosition aDrawPos;
    GetDrawPos(aDrawPos);
    float aPosX = (mX) + aDrawPos.mImageOffsetX + aDrawPos.mHeadX + 11.0f;
    float aPosY = (mY) + aDrawPos.mImageOffsetY + aDrawPos.mHeadY + aDrawPos.mBodyY + 21.0f;
    if (mBodyReanimID != ReanimationID::REANIMATIONID_NULL)
    {
        GetTrackPosition(ReanimTrackId::anim_head1, aPosX, aPosY);
    }

    ParticleEffect aEffect = ParticleEffect::PARTICLE_ZOMBIE_HEAD;
    if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED)
    {
        aEffect = ParticleEffect::PARTICLE_MOWERED_ZOMBIE_HEAD;
    }
    else if (mInPool)
    {
        aEffect = ParticleEffect::PARTICLE_ZOMBIE_HEAD_POOL;
    }
    if (mZombieType == ZombieType::ZOMBIE_DANCER)
    {
        aRenderOrder = mRenderOrder - 1;
    }
    if (mZombieType == ZombieType::ZOMBIE_NEWSPAPER)
    {
        aEffect = ParticleEffect::PARTICLE_ZOMBIE_NEWSPAPER_HEAD;
    }
    else if (mZombieType == ZombieType::ZOMBIE_POGO)
    {
        PogoBreak(ownerNowNs, theDamageFlags);
        aEffect = ParticleEffect::PARTICLE_ZOMBIE_POGO_HEAD;
    }
    else if (mZombieType == ZombieType::ZOMBIE_BALLOON)
    {
        ReanimShowRange(ReanimTrackRanges::anim_hat, RENDER_GROUP_HIDDEN);
        ReanimShowRange(ReanimTrackRanges::hat, RENDER_GROUP_HIDDEN);
        aEffect = ParticleEffect::PARTICLE_ZOMBIE_BALLOON_HEAD;
    }
    else if (mZombieType == ZombieType::ZOMBIE_POLEVAULTER)
    {
        DropPole();
    }
    else if (mZombieType == ZombieType::ZOMBIE_FLAG ||
             mZombieType == ZombieType::ZOMBIE_ZOMBATAR)
    {
        DropFlag(ownerNowNs);
    }

    const std::uint32_t aVariant = GetZombieHeadParticleVariant(aEffect, mZombieType);
    TodParticleSystem* aParticle = mApp->AddTodParticle(aPosX, aPosY, aRenderOrder, aEffect, ownerNowNs, aVariant);
    OverrideParticleColor(ownerNowNs, aParticle);
    OverrideParticleScale(aParticle);
    if (aParticle)
    {
        if (mZombieType == ZombieType::ZOMBIE_DANCER)
        {
            // @Patoke: added new assets
            ReanimShowRange(ReanimTrackRanges::Zombie_disco_chops, RENDER_GROUP_HIDDEN);
            ReanimShowRange(ReanimTrackRanges::Zombie_disco_glasses, RENDER_GROUP_HIDDEN);
        }
        else if (mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
        {
            // @Patoke: added new assets
            ReanimShowRange(ReanimTrackRanges::Zombie_disco_chops, RENDER_GROUP_HIDDEN);
            ReanimShowRange(ReanimTrackRanges::Zombie_backup_stash, RENDER_GROUP_HIDDEN);
        }
    }

    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (mBoard->mMustacheMode && aBodyReanim->TrackExists(ReanimTrackId::Zombie_mustache))
    {
        ReanimShowRange(ReanimTrackRanges::Zombie_mustache, RENDER_GROUP_HIDDEN);

        const std::uint32_t aMustacheOption = aBodyReanim->VariantOption(ReanimTrackId::Zombie_mustache);
        auto aMustacheVariant = ParticleVariants::ZombieMustache::Default;
        if (aMustacheOption == ReanimOptions::Zombie::Zombie_mustache::ZOMBIE_MUSTACHE2)
        {
            aMustacheVariant = ParticleVariants::ZombieMustache::ZombieMustache2;
        }
        else if (aMustacheOption == ReanimOptions::Zombie::Zombie_mustache::ZOMBIE_MUSTACHE3)
        {
            aMustacheVariant = ParticleVariants::ZombieMustache::ZombieMustache3;
        }
        TodParticleSystem* aMustacheParticle = mApp->AddTodParticle(aPosX, aPosY, aRenderOrder,
            ParticleEffect::PARTICLE_ZOMBIE_MUSTACHE, ownerNowNs, static_cast<std::uint32_t>(aMustacheVariant));
        OverrideParticleColor(ownerNowNs, aMustacheParticle);
        OverrideParticleScale(aMustacheParticle);
    }
    if (mBoard->mFutureMode && aBodyReanim->mReanimationType == ReanimationType::REANIM_ZOMBIE)
    {
        const std::uint32_t aHeadOption = aBodyReanim->VariantOption(ReanimTrackId::anim_head1);
        int aFrame = -1;
        if (aHeadOption >= ReanimOptions::Zombie::anim_head1::ZOMBIE_HEAD_SUNGLASSES1 &&
            aHeadOption <= ReanimOptions::Zombie::anim_head1::ZOMBIE_HEAD_SUNGLASSES4)
        {
            aFrame = static_cast<int>(aHeadOption - ReanimOptions::Zombie::anim_head1::ZOMBIE_HEAD_SUNGLASSES1);
        }

        if (aFrame != -1)
        {
            TodParticleSystem* aSunglassParticle = mApp->AddTodParticle(aPosX, aPosY, aRenderOrder, ParticleEffect::PARTICLE_ZOMBIE_SUNGLASS, ownerNowNs);
            OverrideParticleColor(ownerNowNs, aSunglassParticle);
            OverrideParticleScale(aSunglassParticle);
            if (aSunglassParticle)
            {
                aSunglassParticle->OverrideFrame(ParticleEmitterRanges::All, aFrame);
            }
        }
    }
    if (mBoard->mPinataMode && mZombiePhase != ZombiePhase::PHASE_ZOMBIE_MOWERED)
    {
        TodParticleSystem* aPinataParticle = mApp->AddTodParticle(aPosX, aPosY, aRenderOrder, ParticleEffect::PARTICLE_ZOMBIE_PINATA, ownerNowNs);
        (void)aPinataParticle; // Unused
        OverrideParticleScale(aParticle); // Weird, TODO: test the Pinata Mode
    }

    mApp->PlayFoley(FoleyType::FOLEY_LIMBS_POP);
}

// GOTY @Patoke: 0x53A730
// 断臂外观表：{断口取位轨道, 外观轨道, 编译期选项}，按 ZombieType 直下标。
// 每个有断臂外观的 family 均将对应轨道的断臂选项编译为 1。
// 默认行=普通外臂（等价原 switch default）；POGO 额外三处覆写与 FLAG 头部旗面
// 为异构特例，表外保留（见函数体）。槽位脚本从枚举真值（剥块注释）生成。
namespace
{
struct LostArmOverride
{
    ReanimTrackId mPositionTrack;
    ReanimTrackId mOverrideTrack;
    std::uint32_t mArmOption;
};
constexpr LostArmOverride kLostArmOverrides[ZombieType::NUM_ZOMBIE_TYPES] = {
    /* [0]  ZOMBIE_NORMAL          */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [1]  ZOMBIE_FLAG            */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [2]  ZOMBIE_TRAFFIC_CONE    */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [3]  ZOMBIE_POLEVAULTER     */ { ReanimTrackId::Zombie_polevaulter_outerarm_lower, ReanimTrackId::Zombie_polevaulter_outerarm_upper, 1 },
    /* [4]  ZOMBIE_PAIL            */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [5]  ZOMBIE_NEWSPAPER       */ { ReanimTrackId::Zombie_paper_leftarm_lower, ReanimTrackId::Zombie_paper_leftarm_upper, 1 },
    /* [6]  ZOMBIE_DOOR            */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [7]  ZOMBIE_FOOTBALL        */ { ReanimTrackId::zombie_football_leftarm_hand, ReanimTrackId::zombie_football_leftarm_upper, 1 },
    /* [8]  ZOMBIE_DANCER          */ { ReanimTrackId::Zombie_disco_outerarm_lower, ReanimTrackId::Zombie_disco_outerarm_upper, 1 },
    /* [9]  ZOMBIE_BACKUP_DANCER   */ { ReanimTrackId::Zombie_disco_outerarm_lower, ReanimTrackId::Zombie_disco_outerarm_upper, 1 },
    /* [10] ZOMBIE_DUCKY_TUBE      */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [11] ZOMBIE_SNORKEL         */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_snorkle_outerarm_upper, 1 },
    /* [12] ZOMBIE_ZAMBONI         */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [13] ZOMBIE_BOBSLED         */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_dolphinrider_outerarm_upper, 1 },
    /* [14] ZOMBIE_DOLPHIN_RIDER   */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_dolphinrider_outerarm_upper, 1 },
    /* [15] ZOMBIE_JACK_IN_THE_BOX */ { ReanimTrackId::Zombie_jackbox_outerarm_lower, ReanimTrackId::Zombie_jackbox_outerarm_lower, 1 },
    /* [16] ZOMBIE_BALLOON         */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [17] ZOMBIE_DIGGER          */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_digger_outerarm_upper, 1 },
    /* [18] ZOMBIE_POGO            */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [19] ZOMBIE_YETI            */ { ReanimTrackId::Zombie_outerarm_hand, ReanimTrackId::Zombie_yeti_outerarm_upper, 1 },
    /* [20] ZOMBIE_BUNGEE          */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [21] ZOMBIE_LADDER          */ { ReanimTrackId::Zombie_outerarm_hand, ReanimTrackId::Zombie_ladder_outerarm_upper, 1 },
    /* [22] ZOMBIE_CATAPULT        */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [23] ZOMBIE_GARGANTUAR      */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [24] ZOMBIE_IMP             */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_imp_outerarm_upper, 1 },
    /* [25] ZOMBIE_BOSS            */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [26] ZOMBIE_PEA_HEAD        */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [27] ZOMBIE_WALLNUT_HEAD    */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [28] ZOMBIE_JALAPENO_HEAD   */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [29] ZOMBIE_GATLING_HEAD    */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [30] ZOMBIE_SQUASH_HEAD     */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [31] ZOMBIE_TALLNUT_HEAD    */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [32] ZOMBIE_REDEYE_GARGANTUAR */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
    /* [33] ZOMBIE_ZOMBATAR        */ { ReanimTrackId::Zombie_outerarm_lower, ReanimTrackId::Zombie_outerarm_upper, 1 },
};
static_assert(sizeof(kLostArmOverrides) / sizeof(kLostArmOverrides[0]) == (int)ZombieType::NUM_ZOMBIE_TYPES,
    "断臂覆写槽位须与 ZombieType 枚举尾同步");
}  // namespace

namespace
{
// SetupReanimForLostArm 第一段（藏臂轨道）分发表：表内匿名 lambda = 原
// case 体逐句迁移（生成器 + 逆变换 token 对拍 0 差异）；ShowRange/ShowTrack
// 混合调用保持各槽原样；default 非空故具名并被越界分支复用。
using LostArmHideCaseFn = void (*)(Zombie&);

// 原 default（无 case 的 29 型与表外哨兵共用）
void LostArmHideDefault(Zombie& z)
{
            z.ReanimShowRange(ReanimTrackRanges::Zombie_outerarm_lower, RENDER_GROUP_HIDDEN);
            z.ReanimShowRange(ReanimTrackRanges::Zombie_outerarm_hand, RENDER_GROUP_HIDDEN);
}

// 匿名 lambda 函数指针表，行序 = ZombieType 枚举值序（0..33）。
constexpr LostArmHideCaseFn kLostArmHideCases[(int)ZombieType::NUM_ZOMBIE_TYPES] = {
    &LostArmHideDefault,  // 0 ZOMBIE_NORMAL（原 default）
    &LostArmHideDefault,  // 1 ZOMBIE_FLAG（原 default）
    &LostArmHideDefault,  // 2 ZOMBIE_TRAFFIC_CONE（原 default）
    // 原 case ZOMBIE_POLEVAULTER
    [](Zombie& z)
    {
            z.ReanimShowTrack(ReanimTrackId::Zombie_polevaulter_outerarm_lower, RENDER_GROUP_HIDDEN);
            z.ReanimShowTrack(ReanimTrackId::Zombie_outerarm_hand, RENDER_GROUP_HIDDEN);

        // @Patoke: add cases
    },  // 3 ZOMBIE_POLEVAULTER
    &LostArmHideDefault,  // 4 ZOMBIE_PAIL（原 default）
    // 原 case ZOMBIE_NEWSPAPER
    [](Zombie& z)
    {
            z.ReanimShowTrack(ReanimTrackId::Zombie_paper_hands, RENDER_GROUP_HIDDEN);
            z.ReanimShowTrack(ReanimTrackId::Zombie_paper_leftarm_lower, RENDER_GROUP_HIDDEN);
    },  // 5 ZOMBIE_NEWSPAPER
    &LostArmHideDefault,  // 6 ZOMBIE_DOOR（原 default）
    // 原 case ZOMBIE_FOOTBALL
    [](Zombie& z)
    {
            z.ReanimShowRange(ReanimTrackRanges::Zombie_football_leftarm_lower, RENDER_GROUP_HIDDEN);
            z.ReanimShowRange(ReanimTrackRanges::Zombie_football_leftarm_hand, RENDER_GROUP_HIDDEN);
    },  // 7 ZOMBIE_FOOTBALL
    // 原 case ZOMBIE_DANCER
    [](Zombie& z)
    {
            z.ReanimShowTrack(ReanimTrackId::Zombie_disco_outerarm_lower, RENDER_GROUP_HIDDEN);
            z.ReanimShowTrack(ReanimTrackId::Zombie_disco_outerhand_point, RENDER_GROUP_HIDDEN);
    },  // 8 ZOMBIE_DANCER
    // 原 case ZOMBIE_BACKUP_DANCER
    [](Zombie& z)
    {
            z.ReanimShowTrack(ReanimTrackId::Zombie_disco_outerarm_lower, RENDER_GROUP_HIDDEN);
            z.ReanimShowTrack(ReanimTrackId::Zombie_disco_outerhand, RENDER_GROUP_HIDDEN);
    },  // 9 ZOMBIE_BACKUP_DANCER
    &LostArmHideDefault,  // 10 ZOMBIE_DUCKY_TUBE（原 default）
    &LostArmHideDefault,  // 11 ZOMBIE_SNORKEL（原 default）
    &LostArmHideDefault,  // 12 ZOMBIE_ZAMBONI（原 default）
    &LostArmHideDefault,  // 13 ZOMBIE_BOBSLED（原 default）
    &LostArmHideDefault,  // 14 ZOMBIE_DOLPHIN_RIDER（原 default）
    &LostArmHideDefault,  // 15 ZOMBIE_JACK_IN_THE_BOX（原 default）
    &LostArmHideDefault,  // 16 ZOMBIE_BALLOON（原 default）
    &LostArmHideDefault,  // 17 ZOMBIE_DIGGER（原 default）
    &LostArmHideDefault,  // 18 ZOMBIE_POGO（原 default）
    &LostArmHideDefault,  // 19 ZOMBIE_YETI（原 default）
    &LostArmHideDefault,  // 20 ZOMBIE_BUNGEE（原 default）
    &LostArmHideDefault,  // 21 ZOMBIE_LADDER（原 default）
    &LostArmHideDefault,  // 22 ZOMBIE_CATAPULT（原 default）
    &LostArmHideDefault,  // 23 ZOMBIE_GARGANTUAR（原 default）
    &LostArmHideDefault,  // 24 ZOMBIE_IMP（原 default）
    &LostArmHideDefault,  // 25 ZOMBIE_BOSS（原 default）
    &LostArmHideDefault,  // 26 ZOMBIE_PEA_HEAD（原 default）
    &LostArmHideDefault,  // 27 ZOMBIE_WALLNUT_HEAD（原 default）
    &LostArmHideDefault,  // 28 ZOMBIE_JALAPENO_HEAD（原 default）
    &LostArmHideDefault,  // 29 ZOMBIE_GATLING_HEAD（原 default）
    &LostArmHideDefault,  // 30 ZOMBIE_SQUASH_HEAD（原 default）
    &LostArmHideDefault,  // 31 ZOMBIE_TALLNUT_HEAD（原 default）
    &LostArmHideDefault,  // 32 ZOMBIE_REDEYE_GARGANTUAR（原 default）
    &LostArmHideDefault,  // 33 ZOMBIE_ZOMBATAR（原 default）
};
static_assert(sizeof(kLostArmHideCases) / sizeof(kLostArmHideCases[0]) ==
    (int)ZombieType::NUM_ZOMBIE_TYPES, "kLostArmHideCases 槽位数须与 ZombieType 枚举同步");
}  // namespace

namespace
{
struct ZombieArmParticleVariant
{
    std::uint32_t mNormal;
    std::uint32_t mMowered;
};

#define ZOMBIE_ARM_PARTICLE_VARIANT(type, suffix) \
    aVariants[ZombieType::ZOMBIE_##type] = { static_cast<std::uint32_t>(ParticleVariants::ZombieArm::ZombieArm##suffix), \
      static_cast<std::uint32_t>(ParticleVariants::MoweredZombieArm::MoweredZombieArm##suffix) }
constexpr auto kZombieArmParticleVariants = [] {
    std::array<ZombieArmParticleVariant, ZombieType::NUM_ZOMBIE_TYPES> aVariants{};
    ZOMBIE_ARM_PARTICLE_VARIANT(POLEVAULTER, Hand);
    ZOMBIE_ARM_PARTICLE_VARIANT(NEWSPAPER, Newspaper);
    ZOMBIE_ARM_PARTICLE_VARIANT(FOOTBALL, Football);
    ZOMBIE_ARM_PARTICLE_VARIANT(DANCER, Dancer);
    ZOMBIE_ARM_PARTICLE_VARIANT(BACKUP_DANCER, BackupDancer);
    ZOMBIE_ARM_PARTICLE_VARIANT(BOBSLED, Bobsled);
    ZOMBIE_ARM_PARTICLE_VARIANT(DOLPHIN_RIDER, Hand);
    ZOMBIE_ARM_PARTICLE_VARIANT(JACK_IN_THE_BOX, Jackbox);
    ZOMBIE_ARM_PARTICLE_VARIANT(BALLOON, Hand);
    ZOMBIE_ARM_PARTICLE_VARIANT(DIGGER, Digger);
    ZOMBIE_ARM_PARTICLE_VARIANT(POGO, Hand);
    ZOMBIE_ARM_PARTICLE_VARIANT(YETI, Yeti);
    ZOMBIE_ARM_PARTICLE_VARIANT(LADDER, Hand);
    ZOMBIE_ARM_PARTICLE_VARIANT(IMP, Imp);
    return aVariants;
}();
#undef ZOMBIE_ARM_PARTICLE_VARIANT

constexpr std::uint32_t GetZombieArmParticleVariant(ParticleEffect theBase, ZombieType theType)
{
    const unsigned aTypeIndex = static_cast<unsigned>(theType);
    if (aTypeIndex >= kZombieArmParticleVariants.size())
    {
        return 0;
    }
    const ZombieArmParticleVariant& aVariant = kZombieArmParticleVariants[aTypeIndex];
    switch (theBase)
    {
    case ParticleEffect::PARTICLE_ZOMBIE_ARM: return aVariant.mNormal;
    case ParticleEffect::PARTICLE_MOWERED_ZOMBIE_ARM: return aVariant.mMowered;
    default: return 0;
    }
}
}  // namespace

void Zombie::SetupReanimForLostArm(std::int64_t ownerNowNs, unsigned int theDamageFlags)
{
    // C 形态 -> 匿名 lambda 函数指针表：行序 = ZombieType 枚举序（0..33）。
    // 原 default 非空（藏外臂两轨道）——无 case 的型与哨兵均走它，故越界
    // 分支显式调 default 具名函数，逐字保持 switch 语义。
    const unsigned aTypeIndex = static_cast<unsigned>(mZombieType);
    if (aTypeIndex < static_cast<unsigned>(ZombieType::NUM_ZOMBIE_TYPES))
    {
        kLostArmHideCases[aTypeIndex](*this);
    }
    else
    {
        LostArmHideDefault(*this);
    }


    ZombieDrawPosition aDrawPos;
    GetDrawPos(aDrawPos);
    float aPosX = (mX) + aDrawPos.mImageOffsetX + 45.0f;
    float aPosY = (mY) + aDrawPos.mImageOffsetY + aDrawPos.mBodyY + 78.0f;
    if (IsWalkingBackwards())
    {
        aPosX += 36.0f;
    }

    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (aBodyReanim)
    {
        int aType = (int)mZombieType;
        if ((unsigned)aType < (unsigned)(sizeof(kLostArmOverrides) / sizeof(kLostArmOverrides[0])))
        {
            const LostArmOverride& aOverride = kLostArmOverrides[aType];
            GetTrackPosition(aOverride.mPositionTrack, aPosX, aPosY);
            aBodyReanim->SetVariantOption(aOverride.mOverrideTrack, aOverride.mArmOption);
            if (mZombieType == ZombieType::ZOMBIE_POGO)
            {
                aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_pogo_stickhands, ReanimOptions::Zombie_pogo::Zombie_pogo_stickhands::ZOMBIE_POGO_STICKHANDS2);
                aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_pogo_stick, ReanimOptions::Zombie_pogo::Zombie_pogo_stick::ZOMBIE_POGO_STICKDAMAGE2);
                aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_pogo_stick2, ReanimOptions::Zombie_pogo::Zombie_pogo_stick2::ZOMBIE_POGO_STICK2DAMAGE2);
            }
            else if (mZombieType == ZombieType::ZOMBIE_FLAG)
            {
                Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mSpecialHeadReanimID);
                if (aHeadReanim)
                {
                    aHeadReanim->SetVariantOption(ReanimTrackId::Zombie_flag, ReanimOptions::Zombie_flagpole::Zombie_flag::ZOMBIE_FLAG3);
                }
            }
        }
        else
        {
            GetTrackPosition(ReanimTrackId::Zombie_outerarm_lower, aPosX, aPosY);
            aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_outerarm_upper, ReanimOptions::Zombie::Zombie_outerarm_upper::ZOMBIE_OUTERARM_UPPER2);
        }
    }

    if (!mInPool && !TestBit(theDamageFlags, DamageFlags::DAMAGE_DOESNT_LEAVE_BODY))
    {
        ParticleEffect aEffect = ParticleEffect::PARTICLE_ZOMBIE_ARM;
        if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED)
        {
            aEffect = ParticleEffect::PARTICLE_MOWERED_ZOMBIE_ARM;
        }

        const std::uint32_t aVariant = GetZombieArmParticleVariant(aEffect, mZombieType);
        TodParticleSystem* aParticle = mApp->AddTodParticle(aPosX, aPosY, mRenderOrder + 1, aEffect, ownerNowNs, aVariant);
        OverrideParticleColor(ownerNowNs, aParticle);
        OverrideParticleScale(aParticle);
    }
}

//0x529EF0
void Zombie::DropArm(std::int64_t ownerNowNs, unsigned int theDamageFlags)
{
    if (!CanLoseBodyParts())
    {
        return;
    }
    if (mShieldType == ShieldType::SHIELDTYPE_DOOR || mShieldType == ShieldType::SHIELDTYPE_NEWSPAPER)
    {
        return;
    }
    if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL || mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_INTO_POOL || mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP || mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_READING)
    {
        return;
    }
    if (!mHasArm)
    {
        return;
    }

    mHasArm = false;
    SetupReanimForLostArm(ownerNowNs, theDamageFlags);
    mApp->PlayFoley(FoleyType::FOLEY_LIMBS_POP);
}

//0x52A470
void Zombie::UpdateDamageStates(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, unsigned int theDamageFlags)
{
    if (!CanLoseBodyParts())
        return;

    if (mHasArm && mBodyHealth < 2 * mBodyMaxHealth / 3 && mBodyHealth > 0)
    {
        DropArm(ownerNowNs, theDamageFlags);
    }

    if (mHasHead && mBodyHealth < mBodyMaxHealth / 3)
    {
        DropHead(ownerNowNs, theDamageFlags);
        DropLoot();
        StopZombieSound();

        if (mBoard->HasLevelAwardDropped())
        {
            PlayDeathAnim(ownerNowNs, visualNowNs, animationNowNs, theDamageFlags);
        }

        if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL)
        {
            DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
        }
    }
}

//0x52A580
float Zombie::ZombieTargetLeadX(std::int64_t ownerNowNs, float theTime)
{
    const auto aStatusNowNs = ownerNowNs;

    float aSpeed = mVelX;
    if (mChilledEndTimeNs > aStatusNowNs)
    {
        aSpeed *= CHILLED_SPEED_FACTOR;
    }
    if (IsWalkingBackwards())
    {
        aSpeed = -aSpeed;
    }
    if (ZombieNotWalking(ownerNowNs))
    {
        aSpeed = 0.0f;
    }

    Rect aZombieRect = GetZombieRect();
    float aCurrentPosX = aZombieRect.mX + aZombieRect.mWidth / 2;
    float aDisplacementX = aSpeed * theTime;
    return aCurrentPosX - aDisplacementX;
}

//0x52A610
bool Zombie::ZombieNotWalking(std::int64_t ownerNowNs)
{
    if (mIsEating || IsImmobilizied(ownerNowNs))
    {
        return true;
    }

    if (mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_POPPING ||
        mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MADDENING ||
        mZombiePhase == ZombiePhase::PHASE_GARGANTUAR_THROWING ||
        mZombiePhase == ZombiePhase::PHASE_GARGANTUAR_SMASHING ||
        mZombiePhase == ZombiePhase::PHASE_CATAPULT_LAUNCHING ||
        mZombiePhase == ZombiePhase::PHASE_CATAPULT_RELOADING ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_STUNNED ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_WITH_LIGHT ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_HOLD ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_RISING ||
        mZombiePhase == ZombiePhase::PHASE_IMP_GETTING_THROWN ||
        mZombiePhase == ZombiePhase::PHASE_IMP_LANDING ||
        mZombiePhase == ZombiePhase::PHASE_LADDER_PLACING ||
        mZombieHeight == ZombieHeight::HEIGHT_IN_TO_CHIMNEY ||
        mZombieHeight == ZombieHeight::HEIGHT_GETTING_BUNGEE_DROPPED ||
        mZombieHeight == ZombieHeight::HEIGHT_ZOMBIQUARIUM ||
        mZombieType == ZombieType::ZOMBIE_BUNGEE ||
        ((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0) ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_RAISE_LEFT_1 ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_WALK_TO_RAISE ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_RAISE_LEFT_2) // RAISE_RIGHT_1/2 已删（不可达：GetDancerPhase 仅返回 LEFT 系列）
    {
        return true;
    }

    if (mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
    {
        Zombie* aLeader = nullptr;
        if (mZombieType == ZombieType::ZOMBIE_DANCER)
        {
            aLeader = this;
        }
        else
        {
            aLeader = mBoard->ZombieTryToGet(mRelatedZombieID);
        }

        if (aLeader)
        {
            if (aLeader->IsImmobilizied(ownerNowNs) || aLeader->mIsEating)
            {
                return true;
            }

            for (int i = 0; i < NUM_BACKUP_DANCERS; i++)
            {
                Zombie* aDancer = mBoard->ZombieTryToGet(aLeader->mFollowerZombieID[i]);
                if (aDancer && (aDancer->IsImmobilizied(ownerNowNs) || aDancer->mIsEating))
                {
                    return true;
                }
            }
        }
    }

    return false;
}

//0x52A7B0
void Zombie::UpdateZamboni(std::int64_t ownerNowNs)
{
    if (mFlatTires)
    {
        // Loaded flat tires may have no anchor. Store the current speed once;
        // The same segment timestamp anchors the flat-tire speed ramp.
        auto& motion = mVelocityX;
        if (!motion.bound)
        {
            motion.rate = mVelX;
            motion.startTimeNs = ownerNowNs;
            motion.bound = true;
        }
        const auto elapsedNs = ownerNowNs - motion.startTimeNs;
        mVelX = static_cast<float>(std::max(0.0,
            double(motion.rate) - double(elapsedNs) * (0.0005 / 10'000'000.0)));
    }
    else
    {
        mVelocityX.bound = false;
        if (mX > 400.0f)
            mVelX = TodAnimateCurveFloat(700, 300, static_cast<int>(mX),
                0.25f, 0.05f, Curves::Linear);
    }

    int anIceX = (mX) + 118;
    if (mBoard->StageHasRoof())
    {
        anIceX = std::max(anIceX, 500);
    }
    else
    {
        anIceX = std::max(anIceX, 25);
    }
    if (anIceX < mBoard->mIceMinX[mRow])
    {
        mBoard->mIceMinX[mRow] = anIceX;
    }
    if (anIceX < 800)
    {
        mBoard->mIceTimer[mRow] = 3000;
        if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BOBSLED_BONANZA)
        {
            mBoard->mIceTimer[mRow] = INT_MAX;
        }
    }
}

//0x52A8E0
void Zombie::UpdateYeti(std::int64_t ownerNowNs)
{
    if (mMindControlled || !mHasHead || IsDeadOrDying())
        return;

    if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_NORMAL && mPhaseCounter == 0)
    {
        mZombiePhase = ZombiePhase::PHASE_YETI_RUNNING;
        mHasObject = false;
        PickRandomSpeed(ownerNowNs);
    }
}

//0x52A930
void Zombie::UpdateLadder(std::int64_t ownerNowNs)
{
    if (mMindControlled || !mHasHead || IsDeadOrDying())
        return;

    if (mZombiePhase == ZombiePhase::PHASE_LADDER_CARRYING && mZombieHeight == ZombieHeight::HEIGHT_ZOMBIE_NORMAL)
    {
        if (FindPlantTarget(ZombieAttackType::ATTACKTYPE_LADDER))
        {
            StopEating(ownerNowNs);
            mZombiePhase = ZombiePhase::PHASE_LADDER_PLACING;
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_placeladder, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 24.0f);
        }
    }
    else if (mZombiePhase == ZombiePhase::PHASE_LADDER_PLACING)
    {
        Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
        if (aBodyReanim->mLoopCount > 0)
        {
            Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_LADDER);
            if (aPlant)
            {
                mBoard->AddALadder(aPlant->mPlantCol, aPlant->mRow);
                mApp->PlaySample(SOUND_LADDER_ZOMBIE);
                mZombieHeight = ZombieHeight::HEIGHT_UP_LADDER;
                mUseLadderCol = aPlant->mPlantCol;
                DetachShield(ownerNowNs);
            }
            else
            {
                mZombiePhase = ZombiePhase::PHASE_LADDER_CARRYING;
                StartWalkAnim(ownerNowNs, 0);
            }
        }
    }
}

double Zombie::RootPosition(const Reanimation& animation, float signedScale)
{
    const double root = animation.RootMotion(ReanimTrackId::_ground, signedScale);
    if (!mRootBindingGeneration || mRootBindingGeneration != animation.mPlaybackGeneration ||
        mRootScale != signedScale)
    {
        // Reanimation settles completed-cycle displacement when its frame wraps.
        // This is only the actor's world-space intercept, not a playback counter.
        mRootOriginX = double(mX) - root;
        mRootBindingGeneration = animation.mPlaybackGeneration;
        mRootScale = signedScale;
    }
    return mRootOriginX + root;
}

//0x52AA40
void Zombie::UpdateZombieWalking(std::int64_t ownerNowNs)
{
    const float aAdditionalSpeed = mPendingAdditionalSpeedX + (mBlowingAway ? 10.0f : 0.0f);
    mPendingAdditionalSpeedX = 0;
    const bool aNotWalking = ZombieNotWalking(ownerNowNs);
    if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
    {
        const float speed = aNotWalking ? 0.0f :
            (IsWalkingBackwards() ? mVelX : -mVelX);
        mX += static_cast<float>(double(speed + aAdditionalSpeed) *
            double(Sexy::gUpdateIntervalNs) / 10'000'000.0);
        mRootBindingGeneration = 0;
        return;
    }
    if (aNotWalking)
    {
        // A held linear segment must not catch up the time spent stationary
        // when it resumes. Root motion uses this slot as an additional offset,
        // so switching that slot back to world-position motion also rebinds it.
        if (mRootBindingGeneration || aAdditionalSpeed == 0)
            mVelocityX.bound = false;
        mRootBindingGeneration = 0;
        if (aAdditionalSpeed != 0)
            AdvanceVelocity(mX, aAdditionalSpeed, Sexy::gUpdateIntervalNs, ownerNowNs, mVelocityX);
        return;
    }

    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (aBodyReanim)
    {
        float aSpeed = 0;
        bool aUseRoot = false;
        if (IsBouncingPogo() || mZombiePhase == ZombiePhase::PHASE_BALLOON_FLYING || mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING ||
            mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL || mZombieType == ZombieType::ZOMBIE_CATAPULT)
        {
            aSpeed = mVelX;
            if (IsMovingAtChilledSpeed(ownerNowNs))
            {
                aSpeed *= CHILLED_SPEED_FACTOR;
            }
        }
        else if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING || mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP ||
            IsBobsledTeamWithSled() || mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT || mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL)
        {
            aSpeed = mVelX;
        }
        else if (gReanimGroundTrackIndices[(unsigned int)aBodyReanim->mReanimationType] >= 0)
        {
            aUseRoot = true;
        }
        else
        {
            aSpeed = mVelX;
            if (IsMovingAtChilledSpeed(ownerNowNs))
            {
                aSpeed *= CHILLED_SPEED_FACTOR;
            }
        }

        const int aDirection = IsWalkingBackwards() || mZombiePhase == ZombiePhase::PHASE_DANCER_DANCING_IN ? 1 : -1;
        if (aUseRoot)
        {
            // Root and additional linear movement have independent anchors.
            // A root rebind absorbs the complete current world position.
            const float aScale = aDirection * mScaleZombie;
            if (!mRootBindingGeneration || mRootBindingGeneration != aBodyReanim->mPlaybackGeneration ||
                mRootScale != aScale)
            {
                mVelocityX = {};
            }
            const auto aRoot = RootPosition(*aBodyReanim, aScale);
            float aAdditionalOffset = mVelocityX.bound ?
                CheckedPosition(mVelocityX.origin +
                    double(mVelocityX.rate) * double(ReanimClock::Subtract(ReanimClock::Subtract(ownerNowNs, static_cast<std::int64_t>(Sexy::gUpdateIntervalNs)), mVelocityX.startTimeNs)) / 10'000'000.0) : 0.0f;
            AdvanceVelocity(aAdditionalOffset, aAdditionalSpeed, Sexy::gUpdateIntervalNs, ownerNowNs, mVelocityX);
            mX = CheckedPosition(aRoot +
                mVelocityX.origin + double(mVelocityX.rate) * ReanimClock::Subtract(ownerNowNs, mVelocityX.startTimeNs) / 10'000'000.0);
        }
        else
        {
            // Root walking stores an additional offset in mVelocityX; this
            // branch stores a world position. Begin a new segment even when
            // both rates are zero (a pole vault can have zero travel).
            if (mRootBindingGeneration)
                mVelocityX = {};
            mRootBindingGeneration = 0;
            AdvanceVelocity(mX, aDirection * aSpeed + aAdditionalSpeed,
                Sexy::gUpdateIntervalNs, ownerNowNs, mVelocityX);
        }

        if (mZombieType == ZombieType::ZOMBIE_FOOTBALL && mFromWave != Zombie::ZOMBIE_WAVE_WINNER)
        {
            if (aBodyReanim->ShouldTriggerTimedEvent(0.03f))
            {
                mApp->AddTodParticle((mX) + 81, (mY) + 106, mRenderOrder - 1, ParticleEffect::PARTICLE_DUST_FOOT, ownerNowNs);
            }
            if (aBodyReanim->ShouldTriggerTimedEvent(0.61f))
            {
                mApp->AddTodParticle((mX) + 87, (mY) + 110, mRenderOrder - 1, ParticleEffect::PARTICLE_DUST_FOOT, ownerNowNs);
            }
        }
        if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT)
        {
            if (aBodyReanim->ShouldTriggerTimedEvent(0.16f))
            {
                mApp->AddTodParticle((mX) + 81, (mY) + 106, mRenderOrder - 1, ParticleEffect::PARTICLE_DUST_FOOT, ownerNowNs);
            }
            if (aBodyReanim->ShouldTriggerTimedEvent(0.67f))
            {
                mApp->AddTodParticle((mX) + 87, (mY) + 110, mRenderOrder - 1, ParticleEffect::PARTICLE_DUST_FOOT, ownerNowNs);
            }
        }
    }
    else
    {
        bool doWalk = false;
        if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT ||
            mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING ||
            mZombieType == ZombieType::ZOMBIE_DANCER ||
            mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER ||
            mZombieType == ZombieType::ZOMBIE_BOBSLED ||
            mZombieType == ZombieType::ZOMBIE_POGO ||
            mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER ||
            mZombieType == ZombieType::ZOMBIE_BALLOON)
        {
            doWalk = true;
        }
        else if (mZombieType == ZombieType::ZOMBIE_SNORKEL && mInPool)
        {
            doWalk = true;
        }
        else if (mFrame >= 0 && mFrame <= 2)
        {
            doWalk = true;
        }
        else if (mFrame >= 6 && mFrame <= 8)
        {
            doWalk = true;
        }

        if (doWalk || aAdditionalSpeed != 0)
        {
            float aSpeed = mVelX;
            if (IsMovingAtChilledSpeed(ownerNowNs))
            {
                aSpeed *= CHILLED_SPEED_FACTOR;
            }

            mRootBindingGeneration = 0;
            AdvanceVelocity(mX, (doWalk ? (IsWalkingBackwards() ? 1 : -1) * aSpeed : 0.0f) + aAdditionalSpeed,
                Sexy::gUpdateIntervalNs, ownerNowNs, mVelocityX);
        }
    }
}

Plant* Zombie::IsStandingOnSpikeweed()
{
    if (mZombieType == ZombieType::ZOMBIE_ZAMBONI || mZombieType == ZombieType::ZOMBIE_CATAPULT)
        return nullptr;

    Rect aZombieRect = GetZombieRect();

    // 格桶链枚举（2026-10-03 第二批）：原为全池扫 mRow==mRow 行过滤找
    // 首个命中——改本行 9 格×4 层链全成员（含影子株=原池可见集），
    // IsSpiky/!NotOnGround/高台/矩形相交条件逐字保留；池扫=槽位升序
    // 首命中 → 最小槽位号决胜（地刺不可双格注册，无玉米炮去重问题）。
    Plant* aBest = nullptr;
    unsigned int aBestSlot = 0xFFFFFFFFu;
    for (int aCol = 0; aCol < MAX_GRID_SIZE_X; aCol++)
    {
        for (int aLayer = 0; aLayer < PLANT_CELL_COUNT; aLayer++)
        {
            Plant* aPlant = mBoard->GetCellPlant(mRow, aCol, aLayer);
            while (aPlant != nullptr)
            {
                if (aPlant->IsSpiky() && !aPlant->NotOnGround() && (!mOnHighGround || aPlant->IsOnHighGround()))
                {
                    Rect aPlantAttackRect = aPlant->GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);
                    if (GetRectOverlap(aPlantAttackRect, aZombieRect) > 0)
                    {
                        const unsigned int aSlot = mBoard->PlantSlotOf(aPlant);
                        if (aSlot < aBestSlot)
                        {
                            aBestSlot = aSlot;
                            aBest = aPlant;
                        }
                    }
                }
                aPlant = mBoard->NextCellPlant(aPlant);
            }
        }
    }

    return aBest;
}

void Zombie::CheckForZombieStep(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if ((mZombieType == ZombieType::ZOMBIE_ZAMBONI || mZombieType == ZombieType::ZOMBIE_CATAPULT) && !mFlatTires)
    {
        CheckSquish(ownerNowNs, visualNowNs, animationNowNs, ZombieAttackType::ATTACKTYPE_DRIVE_OVER);
    }
}

//0x52AD30
void Zombie::UpdateZombiePosition(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (mZombieType == ZombieType::ZOMBIE_BUNGEE || ((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0) ||
        mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE || mZombieHeight == ZombieHeight::HEIGHT_ZOMBIQUARIUM)
        return;

    UpdateZombieWalking(ownerNowNs);
    CheckForZombieStep(ownerNowNs, visualNowNs, animationNowNs);

    if (mBlowingAway)
    {
        if ((mX) > 850)
        {
            DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
            return;
        }
    }

    if (mZombieHeight == ZombieHeight::HEIGHT_ZOMBIE_NORMAL)
    {
        const auto aDesiredY = GetPosYBasedOnRow(mRow);
        const auto aDirection = (aDesiredY > mY) - (aDesiredY < mY);
        // Type actions can place Y directly on the row (notably a thrown imp
        // following a roof slope). A settled row has no active approach
        // segment; sampling an old zero-rate origin would undo that placement.
        if (aDirection == 0)
        {
            mVelocityY = {};
        }
        else
        {
            AdvanceVelocity(mY, static_cast<float>(aDirection), Sexy::gUpdateIntervalNs, ownerNowNs, mVelocityY);
            if ((aDirection > 0 && mY >= aDesiredY) || (aDirection < 0 && mY <= aDesiredY))
            {
                mY = aDesiredY;
                mVelocityY = {};
            }
        }
    }
}

//0x52AE40
bool Zombie::IsOnBoard()
{
    if (mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE || mFromWave == Zombie::ZOMBIE_WAVE_UI)
    {
        return false;
    }

    TOD_ASSERT(mBoard);
    return true;
}

void Zombie::UpdateBurn(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    mPhaseCounter--;
    if (mPhaseCounter == 0)
    {
        DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
    }
}

//0x52AE60
void Zombie::Update(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, std::uint64_t ownerDeltaNs)
{
    const auto aStatusNowNs = ownerNowNs;

    TOD_ASSERT(!mDead);
    bool doUpdate = false;
    bool playingUpdated = false;
    bool animated = false;
    if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && ((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0))
    {
        doUpdate = true;
    }
    else if (IsOnBoard() && mBoard->mCutScene->ShouldRunUpsellBoard())
    {
        doUpdate = true;
    }
    else if (mApp->mGameScene == GameScenes::SCENE_PLAYING || !IsOnBoard() || mFromWave == Zombie::ZOMBIE_WAVE_WINNER)
    {
        doUpdate = true;
    }

    for (const auto id : {mBodyReanimID, mSpecialHeadReanimID, mZombatarHeadReanimID,
        mMoweredReanimID, mBossFireBallReanimID})
        if (auto* animation = mApp->ReanimationTryToGet(id))
        {
            animation->PrepareTimeAt(animation->mIsAttachment ? animationNowNs : visualNowNs);
        }
    if (doUpdate)
    {
        if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED)
        {
            UpdateBurn(ownerNowNs, visualNowNs, animationNowNs);
        }
        else if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED)
        {
            UpdateMowered(ownerNowNs, visualNowNs, animationNowNs);
        }
        else if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING)
        {
            UpdateDeath(ownerNowNs, visualNowNs, animationNowNs);
            UpdateZombieWalking(ownerNowNs);
        }
        else
        {
            if (mPhaseCounter > 0 && !IsImmobilizied(ownerNowNs))
            {
                mPhaseCounter--;
            }

            if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
            {
                if (mBoard->mCutScene->ShowZombieWalking())
                {
                    UpdateZombieChimney();
                    UpdateZombieWalking(ownerNowNs);

                }
            }
            else if (IsOnBoard())
            {
                playingUpdated = true;
                UpdatePlaying(ownerNowNs, visualNowNs, animationNowNs);
            }

            if (mZombieType == ZombieType::ZOMBIE_BUNGEE)
            {
                UpdateZombieBungee(ownerNowNs, visualNowNs, animationNowNs);
            }
            if (mZombieType == ZombieType::ZOMBIE_POGO)
            {
                UpdateZombiePogo(ownerNowNs);
            }

            animated = true;
            Animate(ownerNowNs, visualNowNs, animationNowNs);
        }

        if (mFadeEndTimeNs >= 0 && aStatusNowNs >= mFadeEndTimeNs)
            DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);




        AttachmentUpdateAndMoveAt(mAttachmentID, mX, mY, ownerNowNs);
        UpdateReanim(ownerNowNs, visualNowNs, animationNowNs);
    }
    const auto statusStepNs = static_cast<std::int64_t>(ownerDeltaNs);
    if (!playingUpdated)
    {
        if (mChilledEndTimeNs != 0) mChilledEndTimeNs += statusStepNs;
        if (mButteredEndTimeNs != 0) mButteredEndTimeNs += statusStepNs;
        if (mIceTrapEndTimeNs != 0) mIceTrapEndTimeNs += statusStepNs;
    }
    if (!doUpdate)
    {
        // Preserve time since last hit as well as a still-active flash.
        mShotFlashEndTimeNs += statusStepNs;
        if (mShieldFlashEndTimeNs != 0) mShieldFlashEndTimeNs += statusStepNs;
        if (mShieldRecoilEndTimeNs != 0) mShieldRecoilEndTimeNs += statusStepNs;
        if (mFadeEndTimeNs >= 0) mFadeEndTimeNs += statusStepNs;
    }
    if (!animated && mYuckyFace) mYuckyFaceStartTimeNs += statusStepNs;

}

//0x52B010
void Zombie::UpdateClimbingLadder(std::int64_t ownerNowNs)
{
    float aDistOffGround = mAltitude;
    if (mOnHighGround)
    {
        aDistOffGround -= HIGH_GROUND_HEIGHT;
    }
    int aLadderOriginX = mBoard->PixelToGridXKeepOnBoard((mX) + 5 + aDistOffGround * 0.5f, (mY));
    if (mBoard->GetLadderAt(aLadderOriginX, mRow) == nullptr)
    {
        mZombieHeight = ZombieHeight::HEIGHT_FALLING;
        return;
    }

    AdvanceVelocity(mAltitude, 0.8f, Sexy::gUpdateIntervalNs, ownerNowNs, mAltitudeMotion);
    if (mVelX < 0.5f)
    {
        mPendingAdditionalSpeedX = -0.5f;
    }

    float aTargetHeight = 90.0f;
    if (mOnHighGround)
    {
        aTargetHeight += HIGH_GROUND_HEIGHT;
    }
    if (mAltitude >= aTargetHeight)
    {
        mZombieHeight = ZombieHeight::HEIGHT_FALLING;
    }
}

//0x52B110
// GOTY @Patoke: 0x53B9F1
void Zombie::UpdateActions(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    // High-gravity balloons have two altitude writers in this method. Sample
    // their combined rate once, then keep the original type-action table and
    // its post-falling threshold checks without advancing the clock twice.
    bool aFlyerAltitudeAdvanced = false;
    if (mZombieHeight == ZombieHeight::HEIGHT_UP_LADDER)
    {
        UpdateClimbingLadder(ownerNowNs);
    }
    if (mZombieHeight == ZombieHeight::HEIGHT_ZOMBIQUARIUM)
    {
        UpdateZombiquarium(ownerNowNs, visualNowNs, animationNowNs);
    }
    if (mZombieHeight == ZombieHeight::HEIGHT_OUT_OF_POOL || mZombieHeight == ZombieHeight::HEIGHT_IN_TO_POOL || mInPool)
    {
        UpdateZombiePool(ownerNowNs);
    }
    if (mZombieHeight == ZombieHeight::HEIGHT_UP_TO_HIGH_GROUND || mZombieHeight == ZombieHeight::HEIGHT_DOWN_OFF_HIGH_GROUND)
    {
        UpdateZombieHighGround(ownerNowNs);
    }
    if (mZombieHeight == ZombieHeight::HEIGHT_FALLING)
    {
        aFlyerAltitudeAdvanced = mZombieType == ZombieType::ZOMBIE_BALLOON &&
            mApp->mGameMode == GameMode::GAMEMODE_LIMBO_HIGH_GRAVITY && mX < 720.0f;
        UpdateZombieFalling(ownerNowNs, aFlyerAltitudeAdvanced ? -0.1f : 0);
    }
    if (mZombieHeight == ZombieHeight::HEIGHT_IN_TO_CHIMNEY)
    {
        UpdateZombieChimney();
    }

    // 各类型专属动作：按 ZombieType 真实枚举值（0..38）直取。注意 ZOMBIE_INVALID = -1
    // 不占槽、ZOMBIE_NORMAL = 0；槽内 lambda 转发到命名成员函数，空槽 = 无专属
    // 动作——普僵/路障/铁桶走公共行走路径。
    // 共享专属动作：伽刚特尔/红眼伽刚特尔同函数——命名 constexpr lambda
    // 复用同一闭包，不重复生成 thunk。
    static constexpr auto kGargantuarUpdate = [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie* z) { z->UpdateZombieGargantuar(ownerNowNs, visualNowNs, animationNowNs); };
    static constexpr void (*kZombieTypeActions[])(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie*) = {
		/*  0 ZOMBIE_NORMAL                    普通僵尸     */ nullptr,
		/*  1 ZOMBIE_FLAG                      旗帜僵尸     */ nullptr,
		/*  2 ZOMBIE_TRAFFIC_CONE              路障僵尸     */ nullptr,
		/*  3 ZOMBIE_POLEVAULTER               撑杆僵尸     */ [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie* z) { z->UpdateZombiePolevaulter(ownerNowNs); },
		/*  4 ZOMBIE_PAIL                      铁桶僵尸     */ nullptr,
		/*  5 ZOMBIE_NEWSPAPER                 读报僵尸     */ [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie* z) { z->UpdateZombieNewspaper(ownerNowNs); },
		/*  6 ZOMBIE_DOOR                      铁门僵尸     */ nullptr,
		/*  7 ZOMBIE_FOOTBALL                  橄榄球僵尸    */ nullptr,
		/*  8 ZOMBIE_DANCER                    舞王僵尸     */ [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie* z) { z->UpdateZombieDancer(ownerNowNs); },
		/*  9 ZOMBIE_BACKUP_DANCER             伴舞僵尸     */ [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie* z) { z->UpdateZombieBackupDancer(ownerNowNs); },
		/* 10 ZOMBIE_DUCKY_TUBE                鸭子救生圈僵尸  */ nullptr,
		/* 11 ZOMBIE_SNORKEL                   潜水僵尸     */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie* z) { z->UpdateZombieSnorkel(ownerNowNs, visualNowNs, animationNowNs); },
		/* 12 ZOMBIE_ZAMBONI                   雪橇车僵尸    */ [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie* z) { z->UpdateZamboni(ownerNowNs); },
		/* 13 ZOMBIE_BOBSLED                   雪橇小队僵尸   */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie* z) { z->UpdateZombieBobsled(ownerNowNs, visualNowNs, animationNowNs); },
		/* 14 ZOMBIE_DOLPHIN_RIDER             海豚骑士僵尸   */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie* z) { z->UpdateZombieDolphinRider(ownerNowNs, visualNowNs, animationNowNs); },
		/* 15 ZOMBIE_JACK_IN_THE_BOX           玩偶匣僵尸    */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie* z) { z->UpdateZombieJackInTheBox(ownerNowNs, visualNowNs, animationNowNs); },
		/* 16 ZOMBIE_BALLOON                   气球僵尸     */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie* z) { z->UpdateZombieFlyer(ownerNowNs, visualNowNs, animationNowNs); },
		/* 17 ZOMBIE_DIGGER                    矿工僵尸     */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie* z) { z->UpdateZombieDigger(ownerNowNs, visualNowNs, animationNowNs); },
		/* 18 ZOMBIE_POGO                      跳跳僵尸     */ nullptr,
		/* 19 ZOMBIE_YETI                      雪人僵尸     */ [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie* z) { z->UpdateYeti(ownerNowNs); },
		/* 20 ZOMBIE_BUNGEE                    蹦极僵尸     */ nullptr,
		/* 21 ZOMBIE_LADDER                    梯子僵尸     */ [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie* z) { z->UpdateLadder(ownerNowNs); },
		/* 22 ZOMBIE_CATAPULT                  投石车僵尸    */ [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie* z) { z->UpdateZombieCatapult(ownerNowNs); },
		/* 23 ZOMBIE_GARGANTUAR                伽刚特尔     */ kGargantuarUpdate,
		/* 24 ZOMBIE_IMP                       小鬼       */ [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie* z) { z->UpdateZombieImp(ownerNowNs); },
		/* 25 ZOMBIE_BOSS                      僵王博士     */ nullptr,
		/* 26 ZOMBIE_PEA_HEAD                  豌豆头      */ [](std::int64_t , std::int64_t , std::int64_t , Zombie* z) { z->UpdateZombiePeaHead(); },
		/* 27 ZOMBIE_WALLNUT_HEAD              坚果头      */ nullptr,
		/* 28 ZOMBIE_JALAPENO_HEAD             辣椒头      */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie* z) { z->UpdateZombieJalapenoHead(ownerNowNs, visualNowNs, animationNowNs); },
		/* 29 ZOMBIE_GATLING_HEAD              机枪头      */ [](std::int64_t , std::int64_t , std::int64_t , Zombie* z) { z->UpdateZombieGatlingHead(); },
		/* 30 ZOMBIE_SQUASH_HEAD               倭瓜头      */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie* z) { z->UpdateZombieSquashHead(ownerNowNs, visualNowNs, animationNowNs); },
		/* 31 ZOMBIE_TALLNUT_HEAD              高坚果头     */ nullptr,
		/* 32 ZOMBIE_REDEYE_GARGANTUAR         红眼伽刚特尔   */ kGargantuarUpdate,
		/* 33 ZOMBIE_ZOMBATAR                  自定义僵尸    */ nullptr,
		/* 34 NUM_ZOMBIE_TYPES                 哨兵位      */ nullptr,
		/* 35 ZOMBIE_CACHED_POLEVAULTER_WITH_POLE 缓存撑杆     */ nullptr,
    };
    static_assert(sizeof(kZombieTypeActions) / sizeof(kZombieTypeActions[0]) == (int)ZombieType::ZOMBIE_CACHED_POLEVAULTER_WITH_POLE + 1,
        "专属动作槽位须与 ZombieType 枚举（含缓存段）同步");
    // 边界守卫：DataArray 新槽 mZombieType 为堆垃圾（Zombie() 空构造），
    // 原版 switch 对垃圾值无匹配=安全跳过，此处同语义。
    if ((unsigned)mZombieType < sizeof(kZombieTypeActions) / sizeof(kZombieTypeActions[0]))
    {
        if (void (*aAction)(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie*) = kZombieTypeActions[mZombieType])
        {
            if (aFlyerAltitudeAdvanced)
                UpdateZombieFlyer(ownerNowNs, visualNowNs, animationNowNs, true);
            else
                aAction(ownerNowNs, visualNowNs, animationNowNs, this);
        }
    }}

//0x52B280
void Zombie::CheckForBoardEdge(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (IsWalkingBackwards() && (mX) > 850.0f)
    {
        DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
        return;
    }

    int aEdgeX = BOARD_EDGE;
    if (mZombieType == ZombieType::ZOMBIE_GARGANTUAR || mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR || mZombieType == ZombieType::ZOMBIE_POLEVAULTER)
    {
        aEdgeX = -150;
    }
    else if (mZombieType == ZombieType::ZOMBIE_CATAPULT || mZombieType == ZombieType::ZOMBIE_FOOTBALL || mZombieType == ZombieType::ZOMBIE_ZAMBONI)
    {
        aEdgeX = -175;
    }
    else if (mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER || mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_SNORKEL)
    {
        aEdgeX = -130;
    }

    if ((mX) <= aEdgeX && mHasHead)
    {
        if (mApp->IsIZombieLevel())
        {
            DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
        }
        else
        {
            mBoard->ZombiesWon(this);
        }
    }
    if ((mX) <= aEdgeX + 70 && !mHasHead)
    {
        TakeDamage(ownerNowNs, visualNowNs, animationNowNs, 1800, 9U);
    }
}

//0x52B340
void Zombie::UpdatePlaying(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    TOD_ASSERT(mBodyHealth > 0 || mZombiePhase == ZombiePhase::PHASE_BOBSLED_CRASHING);

    mGroanCounter--;
    int aZombiesCount = mBoard->mZombies.mSize;
    if (mGroanCounter == 0 && Rand(aZombiesCount) == 0 && mHasHead && mZombieType != ZombieType::ZOMBIE_BOSS && !mBoard->HasLevelAwardDropped())
    {
        float aPitch = 0.0f;
        if (mApp->IsLittleTroubleLevel())
        {
            aPitch = RandRangeFloat(40.0f, 50.0f);
        }

        if (mZombieType == ZombieType::ZOMBIE_GARGANTUAR)
        {
            mApp->PlayFoley(FoleyType::FOLEY_LOW_GROAN);
        }
        else if (mVariant)
        {
            mApp->PlayFoleyPitch(FoleyType::FOLEY_BRAINS, aPitch);
        }
        else if (mApp->mSukhbirMode)
        {
            mApp->PlayFoleyPitch(FoleyType::FOLEY_SUKHBIR, aPitch);
        }
        else
        {
            mApp->PlayFoleyPitch(FoleyType::FOLEY_GROAN, aPitch);
        }

        mGroanCounter = Rand(1000) + 500;
    }

    if (mIceTrapEndTimeNs != 0 && aStatusNowNs >= mIceTrapEndTimeNs)
    {
        mIceTrapEndTimeNs = 0;
        RemoveIceTrap(ownerNowNs);
        AddAttachedParticle(ownerNowNs, 75, 106, ParticleEffect::PARTICLE_ICE_TRAP_RELEASE);
    }
    if (mChilledEndTimeNs != 0 && aStatusNowNs >= mChilledEndTimeNs)
    {
        mChilledEndTimeNs = 0;
        UpdateAnimSpeed(ownerNowNs);
    }
    if (mButteredEndTimeNs != 0 && aStatusNowNs >= mButteredEndTimeNs)
    {
        mButteredEndTimeNs = 0;
        RemoveButter(ownerNowNs);
    }

    if (mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE)
    {
        UpdateZombieRiseFromGrave();
        return;
    }

    if (!IsImmobilizied(ownerNowNs))
    {
        UpdateActions(ownerNowNs, visualNowNs, animationNowNs);
        UpdateZombiePosition(ownerNowNs, visualNowNs, animationNowNs);
        CheckIfPreyCaught(ownerNowNs, visualNowNs, animationNowNs);
        CheckForPool(ownerNowNs, visualNowNs, animationNowNs);
        CheckForHighGround();
        CheckForBoardEdge(ownerNowNs, visualNowNs, animationNowNs);
    }
    else
    {
        // These linear segments do not run while immobilized. Rebind them
        // at the held position on resume instead of catching up frozen time.
        mVelocityX.bound = false;
        mVelocityY.bound = false;
        mAltitudeMotion.bound = false;
        // A root walker may have accumulated an additional linear offset
        // (for example on a ladder). Absorb it when binding the resumed root.
        mRootBindingGeneration = 0;
    }

    if (((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0))
    {
        UpdateBoss(ownerNowNs, visualNowNs, animationNowNs);
    }

    if (!IsDeadOrDying() && mFromWave != Zombie::ZOMBIE_WAVE_WINNER)
    {
        bool isDying = !mHasHead;
        if (mZombieType == ZombieType::ZOMBIE_ZAMBONI || mZombieType == ZombieType::ZOMBIE_CATAPULT)
        {
            if (mBodyHealth < 200)
            {
                isDying = true;
            }
        }

        if (isDying)
        {
            int aDamage = 1;
            if (mZombieType == ZombieType::ZOMBIE_YETI)
            {
                aDamage = 10;
            }
            if (mBodyMaxHealth >= 500)
            {
                aDamage = 3;
            }

            for (int tick = 0; tick < Sexy::gWholeLegacyTicks && !IsDeadOrDying(); ++tick)
            {
                if (Rand(5) == 0)
                    TakeDamage(ownerNowNs, visualNowNs, animationNowNs, aDamage, 9U);
            }
        }
    }
}

//0x52B560
bool Zombie::HasYuckyFaceImage()
{
    if (mBoard->mFutureMode)
        return false;

    return
        mZombieType == ZombieType::ZOMBIE_NORMAL ||
        mZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE ||
        mZombieType == ZombieType::ZOMBIE_PAIL ||
        mZombieType == ZombieType::ZOMBIE_FLAG ||
        mZombieType == ZombieType::ZOMBIE_DOOR ||
        mZombieType == ZombieType::ZOMBIE_DUCKY_TUBE ||
        mZombieType == ZombieType::ZOMBIE_DANCER ||
        mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER ||
        mZombieType == ZombieType::ZOMBIE_NEWSPAPER ||
        mZombieType == ZombieType::ZOMBIE_POLEVAULTER;
}

//0x52B5B0
void Zombie::ShowYuckyFace(bool theShow)
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    if (aBodyReanim == nullptr)
        return;

    if (HasYuckyFaceImage())
    {
        if (theShow)
        {
            aBodyReanim->SetVariantOption(ReanimTrackId::anim_head1, ReanimOptions::Zombie::anim_head1::ZOMBIE_HEAD_GROSSOUT);
            aBodyReanim->AssignRenderGroupToTrack(ReanimTrackId::anim_head2, RENDER_GROUP_HIDDEN);
            aBodyReanim->AssignRenderGroupToTrack(ReanimTrackId::anim_head_jaw, RENDER_GROUP_HIDDEN);
            aBodyReanim->AssignRenderGroupToTrack(ReanimTrackId::anim_tongue, RENDER_GROUP_HIDDEN);
        }
        else if (mHasHead)
        {
            aBodyReanim->SetVariantOption(ReanimTrackId::anim_head1, 0);
            aBodyReanim->AssignRenderGroupToTrack(ReanimTrackId::anim_head2, RENDER_GROUP_NORMAL);
            aBodyReanim->AssignRenderGroupToTrack(ReanimTrackId::anim_head_jaw, RENDER_GROUP_NORMAL);
            if (mVariant)
            {
                aBodyReanim->AssignRenderGroupToTrack(ReanimTrackId::anim_tongue, RENDER_GROUP_NORMAL);
            }
        }
    }
}

//0x52B6A0
void Zombie::UpdateYuckyFace(std::int64_t ownerNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    bool skippedFaceImage = false;
    if (aStatusNowNs-mYuckyFaceStartTimeNs > 200'000'000 && aStatusNowNs-mYuckyFaceStartTimeNs < 1'700'000'000 && !HasYuckyFaceImage())
    {
        StopEating(ownerNowNs);
        mYuckyFaceStartTimeNs = aStatusNowNs - 1'700'000'000;
        skippedFaceImage = true;
        if (mBoard->CountZombiesOnScreen() <= 5 && mHasHead)
        {
            mApp->PlayFoley(FoleyType::FOLEY_YUCK);
        }
        else if (mBoard->CountZombiesOnScreen() <= 10 && mHasHead && Rand(2) == 0)
        {
            mApp->PlayFoley(FoleyType::FOLEY_YUCK);
        }
    }

    if (aStatusNowNs-mYuckyFaceStartTimeNs > 2'700'000'000)
    {
        ShowYuckyFace(false);
        mYuckyFace = false;
        mYuckyFaceStartTimeNs = aStatusNowNs;
        return;
    }

    if (!skippedFaceImage && ZombieAgeCrossedPoint(aStatusNowNs-mYuckyFaceStartTimeNs, Sexy::gUpdateIntervalNs, 700000000LL))
    {
        StopEating(ownerNowNs);
        ShowYuckyFace(true);
        if (mBoard->CountZombiesOnScreen() <= 5 && mHasHead)
        {
            mApp->PlayFoley(FoleyType::FOLEY_YUCK);
        }
        else if (mBoard->CountZombiesOnScreen() <= 10 && mHasHead && Rand(2) == 0)
        {
            mApp->PlayFoley(FoleyType::FOLEY_YUCK);
        }
    }
    if (ZombieAgeCrossedPoint(aStatusNowNs-mYuckyFaceStartTimeNs, Sexy::gUpdateIntervalNs, 1700000000LL))
    {
        StartWalkAnim(ownerNowNs, 20);

        bool aCanGoUp = true;
        bool aCanGoDown = true;
        bool aIsPool = mBoard->mPlantRow[mRow] == PlantRowType::PLANTROW_POOL;
        if (!mBoard->RowCanHaveZombies(mRow - 1))
        {
            aCanGoUp = false;
        }
        else if (mBoard->mPlantRow[mRow - 1] == PlantRowType::PLANTROW_POOL && !aIsPool)
        {
            aCanGoUp = false;
        }
        else if (mBoard->mPlantRow[mRow - 1] != PlantRowType::PLANTROW_POOL && aIsPool)
        {
            aCanGoUp = false;
        }
        if (!mBoard->RowCanHaveZombies(mRow + 1))
        {
            aCanGoDown = false;
        }
        else if (mBoard->mPlantRow[mRow + 1] == PlantRowType::PLANTROW_POOL && !aIsPool)
        {
            aCanGoDown = false;
        }
        else if (mBoard->mPlantRow[mRow + 1] != PlantRowType::PLANTROW_POOL && aIsPool)
        {
            aCanGoDown = false;
        }

        if (aCanGoDown && !aCanGoUp)
        {
            // 1073 0053BF90: take the sole permitted neighbouring row.
            SetRow(mRow + 1);
        }
        else if (!aCanGoDown && aCanGoUp)
        {
            SetRow(mRow - 1);
        }
        else if (aCanGoDown && aCanGoUp)
        {
            SetRow((Rand(2) == 0) ? (mRow + 1) : (mRow - 1));
        }
        else
        {
            TOD_ASSERT(false);
        }
    }
}

//0x52B940
void Zombie::AnimateChewSound(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_UP_TO_EAT)
        return;

    Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_CHEW);
    if (aPlant)
    {
        if (aPlant->mSeedType == SeedType::SEED_HYPNOSHROOM && !aPlant->mIsAsleep)
        {
            mApp->PlayFoley(FoleyType::FOLEY_FLOOP);
            aPlant->Die(ownerNowNs, visualNowNs, animationNowNs);

            StartMindControlled();
            mApp->AddTodParticle((mX) + 60.0f, (mY) + 40.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_MIND_CONTROL, ownerNowNs);
            TrySpawnLevelAward();

            mVelX = 0.17f;
            mAnimTicksPerFrame = 18;
            UpdateAnimSpeed(ownerNowNs);
        }
        else if (aPlant->mSeedType == SeedType::SEED_GARLIC)
        {
            if (!mYuckyFace)
            {
                mYuckyFace = true;
                mYuckyFaceStartTimeNs = aStatusNowNs;
                UpdateAnimSpeed(ownerNowNs);
                mApp->PlayFoley(FoleyType::FOLEY_CHOMP);
            }
        }
        else
        {
            if (aPlant->mSeedType == SeedType::SEED_WALLNUT || aPlant->mSeedType == SeedType::SEED_TALLNUT || aPlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
            {
                mApp->PlayFoley(FoleyType::FOLEY_CHOMP_SOFT);
            }
            else
            {
                mApp->PlayFoley(FoleyType::FOLEY_CHOMP);
            }
        }
    }
    else
    {
        if (mMindControlled)
        {
            mApp->PlayFoley(FoleyType::FOLEY_CHOMP_SOFT);
        }
        else
        {
            mApp->PlayFoley(FoleyType::FOLEY_CHOMP);
        }
    }
}

//0x52BB40
void Zombie::AnimateChewEffect(std::int64_t ownerNowNs)
{
    if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_UP_TO_EAT)
        return;

    if (mApp->IsIZombieLevel())
    {
        GridItem* aBrain = mBoard->mChallenge->IZombieGetBrainTarget(this);
        if (aBrain)
        {
            aBrain->mTransparencyTimeNs = std::max<std::int64_t>(aBrain->mTransparencyTimeNs, ownerNowNs+250'000'000);
            return;
        }
    }

    Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_CHEW);
    if (aPlant)
    {
        if (aPlant->mSeedType == SeedType::SEED_WALLNUT || aPlant->mSeedType == SeedType::SEED_TALLNUT)
        {
            int aRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PROJECTILE, mRow, 0);
            ZombieDrawPosition aDrawPos;
            GetDrawPos(aDrawPos);

            float aPosX = (mX) + 37.0f;
            float aPosY = (mY) + 40.0f + aDrawPos.mBodyY;
            if (mZombieType == ZombieType::ZOMBIE_SNORKEL || mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER)
            {
                aPosX -= 7.0f;
                aPosY += 70.0f;
            }
            else if (IsWalkingBackwards())
            {
                aPosX += 47.0f;
            }
            else if (mZombieType == ZombieType::ZOMBIE_BALLOON)
            {
                aPosY += 47.0f;
            }
            else if (mZombieType == ZombieType::ZOMBIE_IMP)
            {
                aPosX += 24.0f;
                aPosY += 40.0f;
            }

            mApp->AddTodParticle(aPosX, aPosY, aRenderOrder, ParticleEffect::PARTICLE_WALLNUT_EAT_SMALL, ownerNowNs);
        }

        aPlant->mEatenFlashEndTimeNs = std::max<std::int64_t>(aPlant->mEatenFlashEndTimeNs, ownerNowNs+250'000'000);
    }
}

//0x52BCA0
void Zombie::Animate(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    mPrevFrame = mFrame;
    if (mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_POPPING ||
        mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MADDENING ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_STUNNED ||
        IsImmobilizied(ownerNowNs))
    {
        if (mYuckyFace)
            mYuckyFaceStartTimeNs += static_cast<std::int64_t>(Sexy::gUpdateIntervalNs);
        return;
    }

    mAnimCounter++;
    if (mYuckyFace)
    {
        UpdateYuckyFace(ownerNowNs);
    }

    if (mIsEating && mHasHead)
    {
        int aFrameLength = 6;
        if (mChilledEndTimeNs > aStatusNowNs)
        {
            aFrameLength = 12;
        }
        if (mAnimCounter >= mAnimFrames * aFrameLength)
        {
            mAnimCounter = aFrameLength;
        }
        mFrame = mAnimCounter / aFrameLength;

        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        if (aBodyReanim)
        {
            // B 形态 -> 常量表 kZombieChewTimes（原 5 支链合并）：行序 =
            // ZombieType 枚举序 0..33（NUM 哨兵锚定），无 case 槽 = 原链落空
            // 的默认值 {0.14f, 0.68f}；越界键（ZOMBIE_INVALID/缓存态 34）由
            // 分发点守卫回落默认值 = 原 else 语义。
            static constexpr struct { float mLeft; float mRight; } kZombieChewTimes[(int)ZombieType::NUM_ZOMBIE_TYPES] = {
                { 0.14f, 0.68f },  // 0 ZOMBIE_NORMAL（无 case，默认值）
                { 0.14f, 0.68f },  // 1 ZOMBIE_FLAG（无 case，默认值）
                { 0.14f, 0.68f },  // 2 ZOMBIE_TRAFFIC_CONE（无 case，默认值）
                { 0.38f, 0.80f },  // 3 ZOMBIE_POLEVAULTER
                { 0.14f, 0.68f },  // 4 ZOMBIE_PAIL（无 case，默认值）
                { 0.42f, 0.42f },  // 5 ZOMBIE_NEWSPAPER（与 21 LADDER 共值，原链同支）
                { 0.14f, 0.68f },  // 6 ZOMBIE_DOOR（无 case，默认值）
                { 0.14f, 0.68f },  // 7 ZOMBIE_FOOTBALL（无 case，默认值）
                { 0.14f, 0.68f },  // 8 ZOMBIE_DANCER（无 case，默认值）
                { 0.14f, 0.68f },  // 9 ZOMBIE_BACKUP_DANCER（无 case，默认值）
                { 0.14f, 0.68f },  // 10 ZOMBIE_DUCKY_TUBE（无 case，默认值）
                { 0.14f, 0.68f },  // 11 ZOMBIE_SNORKEL（无 case，默认值）
                { 0.14f, 0.68f },  // 12 ZOMBIE_ZAMBONI（无 case，默认值）
                { 0.33f, 0.83f },  // 13 ZOMBIE_BOBSLED
                { 0.14f, 0.68f },  // 14 ZOMBIE_DOLPHIN_RIDER（无 case，默认值）
                { 0.53f, 0.53f },  // 15 ZOMBIE_JACK_IN_THE_BOX
                { 0.14f, 0.68f },  // 16 ZOMBIE_BALLOON（无 case，默认值）
                { 0.14f, 0.68f },  // 17 ZOMBIE_DIGGER（无 case，默认值）
                { 0.14f, 0.68f },  // 18 ZOMBIE_POGO（无 case，默认值）
                { 0.14f, 0.68f },  // 19 ZOMBIE_YETI（无 case，默认值）
                { 0.14f, 0.68f },  // 20 ZOMBIE_BUNGEE（无 case，默认值）
                { 0.42f, 0.42f },  // 21 ZOMBIE_LADDER（与 5 NEWSPAPER 共值，原链同支）
                { 0.14f, 0.68f },  // 22 ZOMBIE_CATAPULT（无 case，默认值）
                { 0.14f, 0.68f },  // 23 ZOMBIE_GARGANTUAR（无 case，默认值）
                { 0.33f, 0.79f },  // 24 ZOMBIE_IMP
                { 0.14f, 0.68f },  // 25 ZOMBIE_BOSS（无 case，默认值）
                { 0.14f, 0.68f },  // 26 ZOMBIE_PEA_HEAD（无 case，默认值）
                { 0.14f, 0.68f },  // 27 ZOMBIE_WALLNUT_HEAD（无 case，默认值）
                { 0.14f, 0.68f },  // 28 ZOMBIE_JALAPENO_HEAD（无 case，默认值）
                { 0.14f, 0.68f },  // 29 ZOMBIE_GATLING_HEAD（无 case，默认值）
                { 0.14f, 0.68f },  // 30 ZOMBIE_SQUASH_HEAD（无 case，默认值）
                { 0.14f, 0.68f },  // 31 ZOMBIE_TALLNUT_HEAD（无 case，默认值）
                { 0.14f, 0.68f },  // 32 ZOMBIE_REDEYE_GARGANTUAR（无 case，默认值）
                { 0.14f, 0.68f },  // 33 ZOMBIE_ZOMBATAR（无 case，默认值）
            };
            static_assert(sizeof(kZombieChewTimes) / sizeof(kZombieChewTimes[0]) ==
                (int)ZombieType::NUM_ZOMBIE_TYPES, "kZombieChewTimes 槽位须与 ZombieType 枚举同步");
            static_assert(kZombieChewTimes[(int)ZombieType::ZOMBIE_NORMAL].mLeft == 0.14f &&
                kZombieChewTimes[(int)ZombieType::ZOMBIE_POLEVAULTER].mLeft == 0.38f &&
                kZombieChewTimes[(int)ZombieType::ZOMBIE_NEWSPAPER].mRight == 0.42f &&
                kZombieChewTimes[(int)ZombieType::ZOMBIE_LADDER].mRight == 0.42f &&
                kZombieChewTimes[(int)ZombieType::ZOMBIE_JACK_IN_THE_BOX].mLeft == 0.53f &&
                kZombieChewTimes[(int)ZombieType::ZOMBIE_BOBSLED].mRight == 0.83f &&
                kZombieChewTimes[(int)ZombieType::ZOMBIE_IMP].mRight == 0.79f,
                "kZombieChewTimes 关键槽位锚点错位");

            float aLeftHandTime = 0.14f;
            float aRightHandTime = 0.68f;
            const unsigned aTypeIndex = static_cast<unsigned>(mZombieType);
            if (aTypeIndex < sizeof(kZombieChewTimes) / sizeof(kZombieChewTimes[0]))
            {
                aLeftHandTime = kZombieChewTimes[aTypeIndex].mLeft;
                aRightHandTime = kZombieChewTimes[aTypeIndex].mRight;
            }

            if (aBodyReanim->ShouldTriggerTimedEvent(aLeftHandTime) || aBodyReanim->ShouldTriggerTimedEvent(aRightHandTime))
            {
                AnimateChewSound(ownerNowNs, visualNowNs, animationNowNs);
                AnimateChewEffect(ownerNowNs);
            }
        }
        else
        {
            if (mAnimCounter == 4 * aFrameLength)
            {
                AnimateChewSound(ownerNowNs, visualNowNs, animationNowNs);
            }
            if (mAnimCounter == 7 * aFrameLength && !mMindControlled)
            {
                AnimateChewEffect(ownerNowNs);
            }
        }
    }
    else
    {
        if (mAnimCounter >= mAnimFrames * mAnimTicksPerFrame)
        {
            mAnimCounter = 0;
        }
        mFrame = mAnimCounter / mAnimTicksPerFrame;
    }
}

//0x52BEE0
bool Zombie::IsWalkingBackwards()
{
    if (mMindControlled)
        return true;

    if (mZombieHeight == ZombieHeight::HEIGHT_ZOMBIQUARIUM)
    {
        if (mVelZ < 1.5707964f || mVelZ > 4.712389f)
        {
            return true;
        }
    }

    if (mZombieType == ZombieType::ZOMBIE_DIGGER)
    {
        if (mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING || mZombiePhase == ZombiePhase::PHASE_DIGGER_STUNNED || mZombiePhase == ZombiePhase::PHASE_DIGGER_WALKING)
        {
            return true;
        }
        else if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED)
        {
            return mHasObject;
        }

        return false;
    }

    return mZombieType == ZombieType::ZOMBIE_YETI && !mHasObject;
}

//0x52C310
void Zombie::UpdateReanim(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (aBodyReanim == nullptr || aBodyReanim->mDead)
        return;

    if (mZombieType == ZombieType::ZOMBIE_CATAPULT)
    {
        const CatapultLoadState loadState = CatapultCycle::Resolve(
            mZombiePhase == ZombiePhase::PHASE_CATAPULT_LAUNCHING,
            mSummonCounter > 0, (static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs));
        const bool loaded = loadState == CatapultLoadState::Loaded;
        if (GetBodyDamageIndex() == 2 || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING)
        {
            aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_catapult_pole, loaded
                ? ReanimOptions::Zombie_catapult::Zombie_catapult_pole::ZOMBIE_CATAPULT_POLE_DAMAGE_WITHBALL
                : ReanimOptions::Zombie_catapult::Zombie_catapult_pole::ZOMBIE_CATAPULT_POLE_DAMAGE);
        }
        else
        {
            aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_catapult_pole, loaded
                ? ReanimOptions::Zombie_catapult::Zombie_catapult_pole::ZOMBIE_CATAPULT_POLE_WITHBALL
                : ReanimOptions::Zombie_catapult::Zombie_catapult_pole::ZOMBIE_CATAPULT_POLE);
        }
    }

    ZombieDrawPosition aDrawPos;
    GetDrawPos(aDrawPos);
    float anOffsetX = aDrawPos.mImageOffsetX + 15.0f;
    float anOffsetY = aDrawPos.mImageOffsetY + aDrawPos.mBodyY - 28.0f + 20.0f;
    if ((mZombieType == ZombieType::ZOMBIE_ZAMBONI || mZombieType == ZombieType::ZOMBIE_CATAPULT) && mZombiePhase != ZombiePhase::PHASE_ZOMBIE_BURNED)
    {
        if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING)
        {
            float aShakeRange = TodAnimateCurveFloat(0.7, 1.0, (static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs), 0.0f, 1.0f, Curves::EaseOut);
            anOffsetX += RandRangeFloat(-aShakeRange, aShakeRange);
            anOffsetY += RandRangeFloat(-aShakeRange, aShakeRange);
        }
        else if (mBodyHealth < 200)
        {
            anOffsetX += RandRangeFloat(-1.0f, 1.0f);
            anOffsetY += RandRangeFloat(-1.0f, 1.0f);
        }
    }
    if (mZombieType == ZombieType::ZOMBIE_FOOTBALL && mScaleZombie < 1.0f)
    {
        anOffsetY += 20.0f - mScaleZombie * 20.0f;
    }

    bool anOpposite = false;
    if (IsWalkingBackwards())
    {
        anOpposite = true;
    }
    if (mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
    {
        anOpposite = false;

        // 1073 0053CC00 mirrors only the entry dance, not the retained 2F/31 phases.
        if (mZombiePhase == ZombiePhase::PHASE_DANCER_DANCING_IN)
        {
            if (!mIsEating)
            {
                anOpposite = true;
            }
        }

        if (mMindControlled)
        {
            anOpposite = !anOpposite;
        }
    }
    if (anOpposite)
    {
        anOffsetX += 90.0f * mScaleZombie;
    }

    aBodyReanim->mOverlayMatrix.m10 = 0.0f;
    aBodyReanim->mOverlayMatrix.m20 = 0.0f;
    aBodyReanim->mOverlayMatrix.m11 = 0.0f;
    aBodyReanim->mOverlayMatrix.m21 = 0.0f;
    aBodyReanim->mOverlayMatrix.m00 = mScaleZombie;
    aBodyReanim->mOverlayMatrix.m11 = mScaleZombie;
    aBodyReanim->mOverlayMatrix.m02 = anOffsetX + 30.0f - mScaleZombie * 30.0f;
    aBodyReanim->mOverlayMatrix.m12 = anOffsetY + 120.0f - mScaleZombie * 120.0f;
    if (anOpposite)
    {
        aBodyReanim->mOverlayMatrix.m00 = -mScaleZombie;
    }

    Reanimation* aMoweredReanim = mApp->ReanimationTryToGet(mMoweredReanimID);
    if (aMoweredReanim)
    {
        aMoweredReanim->UpdateAt(visualNowNs);

        SexyTransform2D aOverlayMatrix;
        aMoweredReanim->GetAttachmentOverlayMatrix(0, aOverlayMatrix);
        aOverlayMatrix.m00 *= aBodyReanim->mOverlayMatrix.m00;
        aOverlayMatrix.m10 *= aBodyReanim->mOverlayMatrix.m00;
        aOverlayMatrix.m01 *= aBodyReanim->mOverlayMatrix.m11;
        aOverlayMatrix.m11 *= aBodyReanim->mOverlayMatrix.m11;
        aOverlayMatrix.m02 *= aBodyReanim->mOverlayMatrix.m00;
        aOverlayMatrix.m12 *= aBodyReanim->mOverlayMatrix.m11;
        aOverlayMatrix.m02 += aBodyReanim->mOverlayMatrix.m11;
        aOverlayMatrix.m12 += aBodyReanim->mOverlayMatrix.m12;
        aBodyReanim->mOverlayMatrix = aOverlayMatrix;
    }

    aBodyReanim->UpdateAt(animationNowNs);
    aBodyReanim->PropogateColorToAttachments();
}

//0x52C6D0
void Zombie::DrawBobsledReanim(std::int64_t ownerNowNs, Graphics* g, const ZombieDrawPosition& theDrawPos, bool theBeforeZombie)
{
    const auto aStatusNowNs = ownerNowNs;

    int aPosition = GetBobsledPosition();
    bool aDrawFront = false;
    bool aDrawBack = false;
    Zombie* aZombieLeader;
    if (mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE)
    {
        aZombieLeader = this;
    }
    else
    {
        if (aPosition == -1)
        {
            return;
        }
        if (aPosition == 0)
        {
            aZombieLeader = this;
        }
        else
        {
            aZombieLeader = mBoard->ZombieGet(mRelatedZombieID);
        }
    }

    if (mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE)
    {
        if (theBeforeZombie)  // 选卡界面中，依次绘制雪橇背面、僵尸本体和雪橇正面
        {
            aDrawBack = true;
        }
        else
        {
            aDrawFront = true;
        }
    }
    else if (mZombiePhase == ZombiePhase::PHASE_BOBSLED_CRASHING)
    {
        if (aPosition == 0 && !theBeforeZombie)  // 雪橇损坏后，在绘制领头僵尸结束后绘制雪橇背面和雪橇正面
        {
            aDrawFront = true;
            aDrawBack = true;
        }
    }
    else if (mZombiePhase == ZombiePhase::PHASE_BOBSLED_SLIDING || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED)
    {
        if (aPosition == 2 && theBeforeZombie)  // 推行雪橇时或雪橇化为灰烬后，在绘制第 2 只僵尸之前绘制雪橇背面和雪橇正面
        {
            aDrawFront = true;
            aDrawBack = true;
        }
    }
    else if (mZombiePhase == ZombiePhase::PHASE_BOBSLED_BOARDING)
    {
        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        if ((static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs) < 0.5f)  // 起跳在空中时，绘制第 2 只僵尸之前绘制雪橇背面和雪橇正面
        {
            if (aPosition == 2 && theBeforeZombie)
            {
                aDrawFront = true;
                aDrawBack = true;
            }
        }
        else if (aPosition == 0 && !theBeforeZombie)  // 进入雪橇后，绘制领头僵尸之后绘制雪橇正面
        {
            aDrawFront = true;
        }
        else if (aPosition == 3 && theBeforeZombie)  // 进入雪橇后，绘制末尾僵尸之前绘制雪橇背面
        {
            aDrawBack = true;
        }
    }

    float aOffsetX = (aZombieLeader->mX) + theDrawPos.mImageOffsetX - (mX) - 76.0f;
    float aOffsetY = 15.0f;
    int aBobsledDamageStatus;
    if (mZombiePhase == ZombiePhase::PHASE_BOBSLED_CRASHING)
    {
        aBobsledDamageStatus = 3;
        int aAlpha = TodAnimateCurveNs(300'000'000, 0, mPhaseCounter.mNanoseconds, 255, 0, Curves::Linear);
        aOffsetX += (BOBSLED_CRASH_TIME - mPhaseCounter) * mVelX / ZOMBIE_LIMP_SPEED_FACTOR;  // 还原至雪橇损坏时的位置
        aOffsetX -= TodAnimateCurveNs(BOBSLED_CRASH_TIME * 10'000'000LL, 0, mPhaseCounter.mNanoseconds, 0.0f, 50.0f, Curves::EaseOut);  // 计算雪橇惯性产生的横向位移
        aOffsetY += TodAnimateCurveNs(BOBSLED_CRASH_TIME * 10'000'000LL, 750'000'000, mPhaseCounter.mNanoseconds, 5.0f, 10.0f, Curves::Linear);
        if (aAlpha != 255)
        {
            g->mColorizeImages = true;
            g->mColor = Color(255, 255, 255, aAlpha);
        }
    }
    else
    {
        aBobsledDamageStatus = aZombieLeader->GetHelmDamageIndex();
    }

    Image* aImage;
    if (aBobsledDamageStatus == 0)
    {
        aImage = IMAGE_ZOMBIE_BOBSLED1;
    }
    else if (aBobsledDamageStatus == 1)
    {
        aImage = IMAGE_ZOMBIE_BOBSLED2;
    }
    else if (aBobsledDamageStatus == 2)
    {
        aImage = IMAGE_ZOMBIE_BOBSLED3;
    }
    else
    {
        aImage = IMAGE_ZOMBIE_BOBSLED4;
    }

    if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED)
    {
        g->mColorizeImages = true;
        g->mColor = Color::Black;
    }

    if (aDrawBack && aBobsledDamageStatus != 3)
    {
        g->DrawImageF(IMAGE_ZOMBIE_BOBSLED_INSIDE, aOffsetX, aOffsetY);
    }
    if (aDrawFront)
    {
        g->DrawImageF(aImage, aOffsetX, aOffsetY);
    }

    if (aZombieLeader->mShotFlashEndTimeNs > aStatusNowNs)
    {
        g->mDrawMode = Graphics::DRAWMODE_ADDITIVE;
        g->mColorizeImages = true;
        int aGrayness = static_cast<int>((aZombieLeader->mShotFlashEndTimeNs - aStatusNowNs) / 1'000'000);
        g->mColor = Color(aGrayness, aGrayness, aGrayness, 255);

        if (aDrawBack && aBobsledDamageStatus != 3)
        {
            g->DrawImageF(IMAGE_ZOMBIE_BOBSLED_INSIDE, aOffsetX, aOffsetY);
        }
        if (aDrawFront)
        {
            g->DrawImageF(aImage, aOffsetX, aOffsetY);
        }

        g->mDrawMode = Graphics::DRAWMODE_NORMAL;
    }

    g->mColorizeImages = false;
}

//0x52CA90
void Zombie::DrawBungeeReanim(std::int64_t ownerNowNs, Graphics* g)
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    // float anOffsetY = theDrawPos.mBodyY + theDrawPos.mImageOffsetY + 14.0f;
    DrawBungeeCord(g, -22);
    aBodyReanim->DrawRenderGroup(g, RENDER_GROUP_NORMAL);

    Zombie* aDroppedZombie = mBoard->ZombieTryToGet(mRelatedZombieID);
    if (aDroppedZombie)
    {
        Graphics aDropGraphics(*g);
        aDropGraphics.mTransY -= mAltitude;
        aDropGraphics.mTransX += (aDroppedZombie->mX) - (mX);
        //aDropGraphics.Translate((aDroppedZombie->mX) - (mX), -mAltitude);

        ZombieDrawPosition aDroppedDrawPos;
        aDroppedZombie->GetDrawPos(aDroppedDrawPos);
        aDroppedZombie->DrawReanim(ownerNowNs, &aDropGraphics, aDroppedDrawPos, RENDER_GROUP_NORMAL);
    }
    else
    {
        Plant* aPlant = mBoard->mPlants.DataArrayTryToGet((unsigned int)mTargetPlantID);
        if (aPlant)
        {
            Graphics aPlantGraphics(*g);
            aPlantGraphics.mTransY += 30.0f - mAltitude;
            if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_RISING)
            {
                if (aPlant->mSeedType == SeedType::SEED_SPIKEWEED || aPlant->mSeedType == SeedType::SEED_SPIKEROCK)
                {
                    aPlantGraphics.mTransY -= 34.0f;
                }
            }
            if (aPlant->mPlantCol <= 4 && mBoard->StageHasRoof())
            {
                aPlantGraphics.mTransY += 10;
            }

            aPlant->Draw(ownerNowNs, &aPlantGraphics);
        }
    }

    aBodyReanim->DrawRenderGroup(g, RENDER_GROUP_ARMS);
}

//0x52CCA0
void Zombie::DrawBungeeTarget(Graphics* g)
{
    if (!IsOnBoard() || mApp->IsFinalBossLevel())
        return;

    if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_RISING) // HIT_OUCHY 已删（不可达）
        return;

    if (mRelatedZombieID != ZombieID::ZOMBIEID_NULL)
        return;

    ZombieDrawPosition aDrawPos;
    GetDrawPos(aDrawPos);
    float aTargetX = (mX) + 10.0f;
    float aTargetY = (mY) + 60.0f + aDrawPos.mBodyY + aDrawPos.mImageOffsetY;
    if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_DIVING || mZombiePhase == ZombiePhase::PHASE_BUNGEE_DIVING_SCREAMING)
    {
        aTargetX += TodAnimateCurveFloat(BUNGEE_ZOMBIE_HEIGHT, BUNGEE_ZOMBIE_HEIGHT - 400, (int)mAltitude, 30.0f, 0.0f, Curves::Linear);
        aTargetY += TodAnimateCurveFloat(BUNGEE_ZOMBIE_HEIGHT, BUNGEE_ZOMBIE_HEIGHT - 400, (int)mAltitude, -600.0f, 0.0f, Curves::Linear);
    }

    g->DrawImageF(IMAGE_BUNGEETARGET, aTargetX, aTargetY + mAltitude);
}

namespace
{
// 舞王聚光灯颜色表（手工生成；整型直下标 0..4，与原 switch 表达式
// mBirthTimeNs/100*7%5 的值域一一对应）
// Color 无 constexpr 构造——表为运行期初始化（一次性，静态存储）
const Color kDancerSpotlight[5] = {
    Color(250, 250, 160),  // 0
    Color(114, 234, 170),  // 1
    Color(216, 126, 202),  // 2
    Color(90, 110, 140),   // 3
    Color(240, 90, 130),   // 4
};
static_assert(sizeof(kDancerSpotlight) / sizeof(kDancerSpotlight[0]) == 5, "聚光灯表槽位");
}  // namespace

//0x52CDF0
// GOTY @Patoke: 0x53D710
void Zombie::DrawDancerReanim(std::int64_t ownerNowNs, Graphics* g)
{
    const auto aStatusNowNs = ownerNowNs;

    Color aSpotLightColor;
    bool aDrawSpotLight = false;
    if (mZombiePhase != ZombiePhase::PHASE_DANCER_DANCING_IN && mZombiePhase != ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS &&
        mZombiePhase != ZombiePhase::PHASE_ZOMBIE_NORMAL && mZombiePhase != ZombiePhase::PHASE_ZOMBIE_DYING && mApp->mGameScene != GameScenes::SCENE_ZOMBIES_WON)
    {
        aDrawSpotLight = true;

        // B 形态 -> 常量表（下方 kDancerSpotlight，整型直下标 0..4）
        aSpotLightColor = kDancerSpotlight[aStatusNowNs-mBirthTimeNs >= 7'000'000'000LL ? (aStatusNowNs-mBirthTimeNs) / 1'000'000'000 * 7 % 5 : 0];

        g->mColorizeImages = true;
        g->mColor = aSpotLightColor;
        TodDrawImageScaledF(g, IMAGE_SPOTLIGHT2, -30.0f, 60.0f, 4.0f, 4.0f);
        g->mColorizeImages = false;
    }

    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    aBodyReanim->DrawRenderGroup(g, RENDER_GROUP_NORMAL);

    if (aDrawSpotLight)
    {
        g->mColorizeImages = true;
        g->mColor = aSpotLightColor;
        TodDrawImageScaledF(g, IMAGE_SPOTLIGHT, -30.0f, -480.0f, 4.0f, 4.0f);
        g->mColorizeImages = false;
    }
}

//0x52D020
// GOTY @Patoke: 0x53D950
void Zombie::DrawReanim(std::int64_t ownerNowNs, Graphics* g, const ZombieDrawPosition& theDrawPos, int theBaseRenderGroup)
{
    const auto aStatusNowNs = ownerNowNs;

    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    if (aBodyReanim == nullptr)
    {
        // @Patoke: missing debug check (this string is not in the game binaries)
#ifdef _DEBUG
        TodTrace("Missing zombie reanimation");
#endif
        return;
    }

    if (theDrawPos.mClipHeight > CLIP_HEIGHT_LIMIT)
    {
        float aDrawHeight = 120.0f - theDrawPos.mClipHeight + 71.0f;
        g->SetClipRect(theDrawPos.mImageOffsetX - 200.0f, theDrawPos.mImageOffsetY + theDrawPos.mBodyY - 78.0f, 520, aDrawHeight);
    }

    int aFadeAlpha = 255;
    if (mFadeEndTimeNs >= 0)
    {
        aFadeAlpha = ClampInt(static_cast<int>(255 * std::max<std::int64_t>(0, mFadeEndTimeNs-aStatusNowNs) / 100'000'000), 0, 255);
    }

    Color aColorOverride(255, 255, 255, aFadeAlpha);
    Color aExtraAdditiveColor = Color::Black;
    bool aEnableExtraAdditiveDraw = false;
    if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED)
    {
        aColorOverride = Color(0, 0, 0, aFadeAlpha);
        aExtraAdditiveColor = Color::Black;
        aEnableExtraAdditiveDraw = false;
    }
    else if (((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0) && mZombiePhase != ZombiePhase::PHASE_ZOMBIE_DYING && mBodyHealth < mBodyMaxHealth / BOSS_FLASH_HEALTH_FRACTION)
    {
        int aGrayness = TodAnimateCurveNs(0, 390'000'000, ownerNowNs % 400'000'000, 155, 255, Curves::Bounce);
        if (mChilledEndTimeNs > aStatusNowNs || mIceTrapEndTimeNs > aStatusNowNs)
        {
            int aColdColor = TodAnimateCurveNs(0, 390'000'000, ownerNowNs % 400'000'000, 65, 75, Curves::Bounce);
            aColorOverride = Color(aColdColor, aColdColor, aGrayness, aFadeAlpha);
        }
        else
        {
            aColorOverride = Color(aGrayness, aGrayness, aGrayness, aFadeAlpha);
        }

        aExtraAdditiveColor = Color::Black;
        aEnableExtraAdditiveDraw = false;
    }
    else if (mMindControlled)
    {
        aColorOverride = ZOMBIE_MINDCONTROLLED_COLOR;
        aColorOverride.mAlpha = aFadeAlpha;
        aExtraAdditiveColor = aColorOverride;
        aEnableExtraAdditiveDraw = true;
    }
    else if (mChilledEndTimeNs > aStatusNowNs || mIceTrapEndTimeNs > aStatusNowNs)
    {
        aColorOverride = Color(75, 75, 255, aFadeAlpha);
        aExtraAdditiveColor = aColorOverride;
        aEnableExtraAdditiveDraw = true;
    }
    else if (mZombieHeight == ZombieHeight::HEIGHT_ZOMBIQUARIUM && mBodyHealth < 100)
    {
        aColorOverride = Color(100, 150, 25, aFadeAlpha);
        aExtraAdditiveColor = aColorOverride;
        aEnableExtraAdditiveDraw = true;
    }
    if (mShotFlashEndTimeNs > aStatusNowNs && !IsBobsledTeamWithSled())
    {
        int aGrayness = static_cast<int>((mShotFlashEndTimeNs - aStatusNowNs) / 1'000'000);
        Color aHighlightColor(aGrayness, aGrayness, aGrayness, 255);
        aExtraAdditiveColor = ColorAdd(aHighlightColor, aExtraAdditiveColor);
        aEnableExtraAdditiveDraw = true;
    }
    aBodyReanim->mColorOverride = aColorOverride;
    aBodyReanim->mExtraAdditiveColor = aExtraAdditiveColor;
    aBodyReanim->mEnableExtraAdditiveDraw = aEnableExtraAdditiveDraw;

    if (mZombieType == ZombieType::ZOMBIE_BOBSLED)
    {
        DrawBobsledReanim(ownerNowNs, g, theDrawPos, true);
        aBodyReanim->DrawRenderGroup(g, theBaseRenderGroup);
        DrawBobsledReanim(ownerNowNs, g, theDrawPos, false);
    }
    else if (mZombieType == ZombieType::ZOMBIE_BUNGEE)
    {
        DrawBungeeReanim(ownerNowNs, g);
    }
    else if (mZombieType == ZombieType::ZOMBIE_DANCER)
    {
        DrawDancerReanim(ownerNowNs, g);
    }
    else
    {
        aBodyReanim->DrawRenderGroup(g, theBaseRenderGroup);
    }

    if (mShieldType != ShieldType::SHIELDTYPE_NONE)
    {
        if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED)
        {
            aBodyReanim->mColorOverride = Color(0, 0, 0, aFadeAlpha);
            aBodyReanim->mExtraAdditiveColor = Color::Black;
            aBodyReanim->mEnableExtraAdditiveDraw = false;
        }
        else if (mShieldFlashEndTimeNs > aStatusNowNs)
        {
            int aGrayness = static_cast<int>((mShieldFlashEndTimeNs - aStatusNowNs) / 1'000'000);
            aBodyReanim->mColorOverride = Color(aGrayness, aGrayness, aGrayness, aFadeAlpha);
            aBodyReanim->mExtraAdditiveColor = Color::White;
            aBodyReanim->mEnableExtraAdditiveDraw = true;
        }
        else
        {
            aBodyReanim->mColorOverride = Color(255, 255, 255, aFadeAlpha);
            aBodyReanim->mExtraAdditiveColor = Color::Black;
            aBodyReanim->mEnableExtraAdditiveDraw = false;
        }

        float aShieldHitOffset = 0.0f;
        if (mShieldRecoilEndTimeNs > aStatusNowNs)
        {
            aShieldHitOffset = TodAnimateCurveNs(120'000'000, 0, std::max<std::int64_t>(0, mShieldRecoilEndTimeNs - aStatusNowNs), 3.0f, 0.0f, Curves::Linear);
        }

        const float aOriginalTransX = g->mTransX;
        g->mTransX = aOriginalTransX + aShieldHitOffset;
        aBodyReanim->DrawRenderGroup(g, RENDER_GROUP_SHIELD);
        // Restore the parent's exact translation rather than subtracting a
        // rounded offset and leaking the error into later render groups.
        g->mTransX = aOriginalTransX;
    }

    if (mShieldType == ShieldType::SHIELDTYPE_NEWSPAPER || mShieldType == ShieldType::SHIELDTYPE_DOOR || mShieldType == ShieldType::SHIELDTYPE_LADDER)
    {
        aBodyReanim->mColorOverride = aColorOverride;
        aBodyReanim->mExtraAdditiveColor = aExtraAdditiveColor;
        aBodyReanim->mEnableExtraAdditiveDraw = aEnableExtraAdditiveDraw;
        aBodyReanim->DrawRenderGroup(g, RENDER_GROUP_OVER_SHIELD);
    }

    // 1073 FUN_0053D950 has a dedicated Zombatar path after the normal
    // render group.  The Zombatar body keeps anim_head1 in render group 3
    // (with the custom head attached to it), and its extra accessory tracks
    // are in group 4.  The generic shield-only path above never reaches
    // these groups because a Zombatar has no shield; omitting them makes the
    // avatar preview render as a headless body.
    if (mZombieType == ZombieType::ZOMBIE_ZOMBATAR)
    {
        aBodyReanim->mColorOverride = aColorOverride;
        aBodyReanim->mExtraAdditiveColor = aExtraAdditiveColor;
        aBodyReanim->mEnableExtraAdditiveDraw = aEnableExtraAdditiveDraw;
        aBodyReanim->DrawRenderGroup(g, RENDER_GROUP_OVER_SHIELD);
        aBodyReanim->DrawRenderGroup(g, 4);
    }

    g->ClearClipRect();
}

//0x52D6D0

//0x52D710
int Zombie::GetBodyDamageIndex()
{
    if (((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0))
    {
        if (mBodyHealth < mBodyMaxHealth / 2)
        {
            return 2;
        }

        if (mBodyHealth < mBodyMaxHealth * 4 / 5)
        {
            return 1;
        }

        return 0;
    }
    else
    {
        if (mBodyHealth < mBodyMaxHealth / 3)
        {
            return 2;
        }

        if (mBodyHealth < mBodyMaxHealth * 2 / 3)
        {
            return 1;
        }

        return 0;
    }
}

//0x52D780

//0x52D7C0
void Zombie::DrawBungeeCord(Graphics* g, int theOffsetX)
{
    int aCordHeight = IMAGE_BUNGEECORD->mHeight * mScaleZombie;
    float aPosX, aPosY;
    GetTrackPosition(ReanimTrackId::Zombie_bungi_body, aPosX, aPosY);

    bool aSetClip = false;
    if (IsOnBoard() && mApp->IsFinalBossLevel())
    {
        Zombie* aBoss = mBoard->GetBossZombie();

        int aClipAmount = 55;
        if (aBoss->mZombiePhase == ZombiePhase::PHASE_BOSS_BUNGEES_LEAVE)
        {
            Reanimation* aBossReanim = mApp->ReanimationGet(aBoss->mBodyReanimID);
            aClipAmount = TodAnimateCurveFloat(0.0, 0.2, (static_cast<double>(aBossReanim->SamplePlayback().phaseNs) / aBossReanim->mPeriodNs), 55.0f, 0.0f, Curves::Linear);
        }

        if (mTargetCol > aBoss->mTargetCol)
        {
            g->SetClipRect(Rect(-g->mTransX, aClipAmount - g->mTransY, BOARD_WIDTH, BOARD_HEIGHT));
            aSetClip = true;
        }
    }

    for (float y = aPosY - aCordHeight; y > -aCordHeight; y -= aCordHeight)
    {
        TodDrawImageScaledF(g, IMAGE_BUNGEECORD, theOffsetX + 61.0f - 4.0f / mScaleZombie, y - (mY), mScaleZombie, mScaleZombie);
    }

    if (aSetClip)
    {
        g->ClearClipRect();
    }
}

//0x52D9E0
void Zombie::GetDrawPos(ZombieDrawPosition& theDrawPos)
{
    theDrawPos.mImageOffsetX = 0;
    theDrawPos.mImageOffsetY = 0;

    if (mIsEating)
    {
        theDrawPos.mHeadX = 47;
        theDrawPos.mHeadY = 4;
    }
    else
    {
        // 行走帧头部位置：按 mFrame（0..10）直查 {X, Y}，越界用默认（同 frame 9）。
        static constexpr int kWalkHeadPos[][2] = {
		{ 50, 2 },   // frame 0
		{ 49, 1 },   // frame 1
		{ 49, 2 },   // frame 2
		{ 48, 4 },   // frame 3
		{ 48, 5 },   // frame 4
		{ 48, 4 },   // frame 5
		{ 48, 2 },   // frame 6
		{ 49, 1 },   // frame 7
		{ 49, 2 },   // frame 8
		{ 50, 4 },   // frame 9
		{ 50, 5 },   // frame 10
        };
        if ((unsigned)mFrame < sizeof(kWalkHeadPos) / sizeof(kWalkHeadPos[0]))
        {
            theDrawPos.mHeadX = kWalkHeadPos[mFrame][0];
            theDrawPos.mHeadY = kWalkHeadPos[mFrame][1];
        }
        else
        {
            theDrawPos.mHeadX = 50;
            theDrawPos.mHeadY = 4;
        }
    }

    theDrawPos.mArmY = theDrawPos.mHeadY / 2;

    // 各类型体型贴图偏移修正：按 ZombieType 真实枚举值（0..38）直查，零分支。
    // 注意 ZOMBIE_INVALID = -1 不占槽、ZOMBIE_NORMAL = 0。
    static constexpr struct { float mDX; float mDY; } kZombieImageOffsetFix[] = {
		{ 0, 0 },   //  0 ZOMBIE_NORMAL                    普通僵尸
		{ 0, 0 },   //  1 ZOMBIE_FLAG                      旗帜僵尸
		{ 0, 0 },   //  2 ZOMBIE_TRAFFIC_CONE              路障僵尸
		{ -6, -11 },   //  3 ZOMBIE_POLEVAULTER               撑杆僵尸
		{ 0, 0 },   //  4 ZOMBIE_PAIL                      铁桶僵尸
		{ 0, 0 },   //  5 ZOMBIE_NEWSPAPER                 读报僵尸
		{ 0, 0 },   //  6 ZOMBIE_DOOR                      铁门僵尸
		{ 0, -16 },   //  7 ZOMBIE_FOOTBALL                  橄榄球僵尸
		{ 0, 0 },   //  8 ZOMBIE_DANCER                    舞王僵尸
		{ 0, 0 },   //  9 ZOMBIE_BACKUP_DANCER             伴舞僵尸
		{ 0, 0 },   // 10 ZOMBIE_DUCKY_TUBE                鸭子救生圈僵尸
		{ 0, 0 },   // 11 ZOMBIE_SNORKEL                   潜水僵尸
		{ 68, -23 },   // 12 ZOMBIE_ZAMBONI                   雪橇车僵尸
		{ 0, -12 },   // 13 ZOMBIE_BOBSLED                   雪橇小队僵尸
		{ 0, 0 },   // 14 ZOMBIE_DOLPHIN_RIDER             海豚骑士僵尸
		{ 0, 0 },   // 15 ZOMBIE_JACK_IN_THE_BOX           玩偶匣僵尸
		{ 0, 17 },   // 16 ZOMBIE_BALLOON                   气球僵尸
		{ 0, 0 },   // 17 ZOMBIE_DIGGER                    矿工僵尸
		{ 0, 16 },   // 18 ZOMBIE_POGO                      跳跳僵尸
		{ 0, -20 },   // 19 ZOMBIE_YETI                      雪人僵尸
		{ 0, 0 },   // 20 ZOMBIE_BUNGEE                    蹦极僵尸
		{ 0, 0 },   // 21 ZOMBIE_LADDER                    梯子僵尸
		{ -25, -18 },   // 22 ZOMBIE_CATAPULT                  投石车僵尸
		{ 0, -8 },   // 23 ZOMBIE_GARGANTUAR                伽刚特尔
		{ 0, 0 },   // 24 ZOMBIE_IMP                       小鬼
		{ 0, 0 },   // 25 ZOMBIE_BOSS                      僵王博士
		{ 0, 0 },   // 26 ZOMBIE_PEA_HEAD                  豌豆头
		{ 0, 0 },   // 27 ZOMBIE_WALLNUT_HEAD              坚果头
		{ 0, 0 },   // 28 ZOMBIE_JALAPENO_HEAD             辣椒头
		{ 0, 0 },   // 29 ZOMBIE_GATLING_HEAD              机枪头
		{ 0, 0 },   // 30 ZOMBIE_SQUASH_HEAD               倭瓜头
		{ 0, 0 },   // 31 ZOMBIE_TALLNUT_HEAD              高坚果头
		{ 0, -8 },   // 32 ZOMBIE_REDEYE_GARGANTUAR         红眼伽刚特尔
		{ 0, 0 },   // 33 ZOMBIE_ZOMBATAR                  自定义僵尸
		{ 0, 0 },   // 34 NUM_ZOMBIE_TYPES                 哨兵位
		{ 0, 0 },   // 35 ZOMBIE_CACHED_POLEVAULTER_WITH_POLE 缓存撑杆
    };
    if ((unsigned)mZombieType < sizeof(kZombieImageOffsetFix) / sizeof(kZombieImageOffsetFix[0]))
    {
        theDrawPos.mImageOffsetX += kZombieImageOffsetFix[mZombieType].mDX;
        theDrawPos.mImageOffsetY += kZombieImageOffsetFix[mZombieType].mDY;
    }

    if (mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE)
    {
        theDrawPos.mBodyY = -mAltitude;

        if (mInPool)
        {
            theDrawPos.mClipHeight = theDrawPos.mBodyY;
        }
        else
        {
            float aHeightLimit = std::min<Sexy::TimeCounter>(mPhaseCounter, 40);
            theDrawPos.mClipHeight = theDrawPos.mBodyY + aHeightLimit;
        }

        if (IsOnHighGround())
        {
            theDrawPos.mBodyY -= HIGH_GROUND_HEIGHT;
        }

        return;
    }

    if (mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER)
    {
        theDrawPos.mBodyY = -mAltitude;
        theDrawPos.mClipHeight = CLIP_HEIGHT_OFF;

        if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_INTO_POOL)
        {
            Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

            if ((static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs) >= 0.56f && (static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs) <= 0.65f)  // 跳上海豚的起跳过程
            {
                theDrawPos.mClipHeight = 0.0f;
            }
            else if ((static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs) >= 0.75f)  // 跳上海豚的下落过程
            {
                theDrawPos.mClipHeight = -mAltitude - 10.0f;
            }
        }
        else if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING)
        {
            theDrawPos.mImageOffsetX += 70.0f;  // Compensate the 70 pixel dolphin mounting offset.

            if (mZombieHeight == ZombieHeight::HEIGHT_DRAGGED_UNDER)
            {
                theDrawPos.mClipHeight = -mAltitude - 15.0f;
            }
            else
            {
                theDrawPos.mClipHeight = -mAltitude - 10.0f;
            }
        }
        else if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP)
        {
            theDrawPos.mImageOffsetX += 70.0f + mAltitude;

            Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
            if ((static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs) <= 0.06f)  // 起跳出水之前
            {
                theDrawPos.mClipHeight = -mAltitude - 10.0f;
            }
            else if ((static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs) >= 0.5f && (static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs) <= 0.76f) // 起跳过程中（脱离水面后至重新入水前）
            {
                theDrawPos.mClipHeight = -13.0f;
            }
        }
        else if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING_IN_POOL || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING)
        {
            theDrawPos.mImageOffsetY += 50.0f;  // 额外 50 像素的横坐标偏移用于弥补跳跃过程中前进的距离

            if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING)
            {
                theDrawPos.mClipHeight = -mAltitude + 44.0f;
            }
            else if (mZombieHeight == ZombieHeight::HEIGHT_DRAGGED_UNDER)
            {
                theDrawPos.mClipHeight = -mAltitude + 36.0f;
            }
        }
        else if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING && mZombieHeight == ZombieHeight::HEIGHT_OUT_OF_POOL)
        {
            theDrawPos.mClipHeight = -mAltitude;
        }
        else if (mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING_WITHOUT_DOLPHIN && mZombieHeight == ZombieHeight::HEIGHT_OUT_OF_POOL)
        {
            theDrawPos.mClipHeight = -mAltitude;
        }
    }
    else if (mZombieType == ZombieType::ZOMBIE_SNORKEL)
    {
        theDrawPos.mBodyY = -mAltitude;
        theDrawPos.mClipHeight = CLIP_HEIGHT_OFF;

        if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL)
        {
            Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
            if ((static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs) >= 0.8f)  // 入水后
            {
                theDrawPos.mClipHeight = -10.0f;
            }
        }
        else if (mInPool)
        {
            theDrawPos.mClipHeight = -mAltitude - 5.0f;
            theDrawPos.mClipHeight += 20.0f - 20.0f * mScaleZombie;
        }
    }
    else if (mInPool)
    {
        theDrawPos.mBodyY = -mAltitude;
        theDrawPos.mClipHeight = -mAltitude - 7.0f;
        theDrawPos.mClipHeight += 10.0f - 10.0f * mScaleZombie;

        if (mIsEating)
        {
            theDrawPos.mClipHeight += 7.0f;
        }
    }
    else if (mZombiePhase == ZombiePhase::PHASE_DANCER_RISING)
    {
        theDrawPos.mBodyY = -mAltitude;
        theDrawPos.mClipHeight = -mAltitude;

        if (IsOnHighGround())
        {
            theDrawPos.mBodyY -= HIGH_GROUND_HEIGHT;
        }
    }
    else if (mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING || mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE)
    {
        theDrawPos.mBodyY = -mAltitude;

        if (mPhaseCounter > 20)
        {
            theDrawPos.mClipHeight = -mAltitude;
        }
        else
        {
            theDrawPos.mClipHeight = CLIP_HEIGHT_OFF;
        }
    }
    else if (mZombieType == ZombieType::ZOMBIE_BUNGEE)
    {
        theDrawPos.mBodyY = -mAltitude;
        theDrawPos.mImageOffsetX -= 18.0f;

        if (IsOnHighGround())
        {
            theDrawPos.mBodyY -= HIGH_GROUND_HEIGHT;
        }

        theDrawPos.mClipHeight = CLIP_HEIGHT_OFF;
    }
    else
    {
        theDrawPos.mBodyY = -mAltitude;
        theDrawPos.mClipHeight = CLIP_HEIGHT_OFF;
    }
}

//0x52DF90
int Zombie::GetDancerFrame(std::int64_t ownerNowNs)
{
    if (mFromWave == Zombie::ZOMBIE_WAVE_UI || IsImmobilizied(ownerNowNs))
        return 0;

    int aFrameLength = 20;
    int aFramesCount = 23;
    if (mZombiePhase == ZombiePhase::PHASE_DANCER_DANCING_IN)
    {
        aFramesCount = 11;
        aFrameLength = 10;
    }

#ifdef DO_FIX_BUGS
    if (mBoard)
    {
        return static_cast<int>((ownerNowNs /
            (std::uint64_t(aFrameLength) * 10'000'000)) % aFramesCount);  // 修复“女仆秘籍”
    }
    else
    {
        return (mApp->mAppCounter % (aFrameLength * aFramesCount)) / aFrameLength;
    }
#else
    return (mApp->mAppCounter % (aFrameLength * aFramesCount)) / aFrameLength;
#endif
}

//0x52DFE0
// GOTY @Patoke: 0x53E900
ZombiePhase Zombie::GetDancerPhase(std::int64_t ownerNowNs)
{
    int aFrame = GetDancerFrame(ownerNowNs);

    return
        aFrame <= 11 ? ZombiePhase::PHASE_DANCER_DANCING_LEFT :
        aFrame <= 12 ? ZombiePhase::PHASE_DANCER_WALK_TO_RAISE :
        aFrame <= 18 ? ZombiePhase::PHASE_DANCER_RAISE_LEFT_1 :
                       ZombiePhase::PHASE_DANCER_RAISE_LEFT_2;
}

namespace
{
// Zombie::DrawIceTrap 偏移调整表：表内匿名 lambda = 原 case 体逐句迁移
//（生成器 + 逆变换 token 对拍 0 差异）；无 case 槽不调整（哨兵布尔表）。
struct IceTrapAdjust
{
    float mDx;
    float mDy;
    float mScale;
    bool mAdjust;   // false = 哨兵：不调整
};
using IceTrapAdjustFn = void (*)(Zombie&, IceTrapAdjust&);

// 标签共体（匿名 lambda 无法跨槽复用，复用即具名）
// 原 case ZOMBIE_GARGANTUAR, ZOMBIE_REDEYE_GARGANTUAR
void IceTrapAdjustCaseGargantuar(Zombie& , IceTrapAdjust& lc)
{
            lc.mDx -= 20.0f;
            lc.mDy -= 7.0f;
            lc.mScale = 1.6f;
}

// 原 default（无 case 的 27 型（空操作）共用）
void IceTrapNoAdjust(Zombie& , IceTrapAdjust& )
{

}

// 匿名 lambda 函数指针表，行序 = ZombieType 枚举值序（0..33）。
constexpr IceTrapAdjustFn kIceTrapAdjusts[(int)ZombieType::NUM_ZOMBIE_TYPES] = {
    &IceTrapNoAdjust,  // 0 ZOMBIE_NORMAL（原 default）
    &IceTrapNoAdjust,  // 1 ZOMBIE_FLAG（原 default）
    &IceTrapNoAdjust,  // 2 ZOMBIE_TRAFFIC_CONE（原 default）
    &IceTrapNoAdjust,  // 3 ZOMBIE_POLEVAULTER（原 default）
    &IceTrapNoAdjust,  // 4 ZOMBIE_PAIL（原 default）
    &IceTrapNoAdjust,  // 5 ZOMBIE_NEWSPAPER（原 default）
    &IceTrapNoAdjust,  // 6 ZOMBIE_DOOR（原 default）
    &IceTrapNoAdjust,  // 7 ZOMBIE_FOOTBALL（原 default）
    &IceTrapNoAdjust,  // 8 ZOMBIE_DANCER（原 default）
    &IceTrapNoAdjust,  // 9 ZOMBIE_BACKUP_DANCER（原 default）
    &IceTrapNoAdjust,  // 10 ZOMBIE_DUCKY_TUBE（原 default）
    &IceTrapNoAdjust,  // 11 ZOMBIE_SNORKEL（原 default）
    &IceTrapNoAdjust,  // 12 ZOMBIE_ZAMBONI（原 default）
    &IceTrapNoAdjust,  // 13 ZOMBIE_BOBSLED（原 default）
    &IceTrapNoAdjust,  // 14 ZOMBIE_DOLPHIN_RIDER（原 default）
    &IceTrapNoAdjust,  // 15 ZOMBIE_JACK_IN_THE_BOX（原 default）
    // 原 case ZOMBIE_BALLOON
    [](Zombie& , IceTrapAdjust& lc)
    {
            lc.mDx -= 9.0f;
            lc.mDy += 27.0f;
    },  // 16 ZOMBIE_BALLOON
    // 原 case ZOMBIE_DIGGER
    [](Zombie& , IceTrapAdjust& lc)
    {
            lc.mDx -= 27.0f;
    },  // 17 ZOMBIE_DIGGER
    // 原 case ZOMBIE_POGO
    [](Zombie& , IceTrapAdjust& lc)
    {
            lc.mDx -= 10.0f;
            lc.mDy += 20.0f;
    },  // 18 ZOMBIE_POGO
    &IceTrapNoAdjust,  // 19 ZOMBIE_YETI（原 default）
    // 原 case ZOMBIE_BUNGEE
    [](Zombie& , IceTrapAdjust& lc)
    {
            lc.mDx -= 45.0f;
            lc.mDy -= 23.0f;
            lc.mScale = 1.2f;
    },  // 20 ZOMBIE_BUNGEE
    &IceTrapNoAdjust,  // 21 ZOMBIE_LADDER（原 default）
    // 原 case ZOMBIE_CATAPULT
    [](Zombie& , IceTrapAdjust& lc)
    {
            lc.mDx += 32.0f;
    },  // 22 ZOMBIE_CATAPULT
    &IceTrapAdjustCaseGargantuar,  // 23 ZOMBIE_GARGANTUAR（共体）
    &IceTrapNoAdjust,  // 24 ZOMBIE_IMP（原 default）
    &IceTrapNoAdjust,  // 25 ZOMBIE_BOSS（原 default）
    &IceTrapNoAdjust,  // 26 ZOMBIE_PEA_HEAD（原 default）
    &IceTrapNoAdjust,  // 27 ZOMBIE_WALLNUT_HEAD（原 default）
    &IceTrapNoAdjust,  // 28 ZOMBIE_JALAPENO_HEAD（原 default）
    &IceTrapNoAdjust,  // 29 ZOMBIE_GATLING_HEAD（原 default）
    &IceTrapNoAdjust,  // 30 ZOMBIE_SQUASH_HEAD（原 default）
    &IceTrapNoAdjust,  // 31 ZOMBIE_TALLNUT_HEAD（原 default）
    &IceTrapAdjustCaseGargantuar,  // 32 ZOMBIE_REDEYE_GARGANTUAR（共体）
    &IceTrapNoAdjust,  // 33 ZOMBIE_ZOMBATAR（原 default）
};
static_assert(sizeof(kIceTrapAdjusts) / sizeof(kIceTrapAdjusts[0]) ==
    (int)ZombieType::NUM_ZOMBIE_TYPES, "kIceTrapAdjusts 槽位数须与 ZombieType 枚举同步");
}  // namespace

//0x52E020
void Zombie::DrawIceTrap(Graphics* g, const ZombieDrawPosition& theDrawPos, bool theFront)
{
    if (mInPool || ((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0))
        return;

    float aOffsetX = 46.0f;
    float aOffsetY = theDrawPos.mBodyY + 92.0f;
    float aScale = 1.0f;
    // C 形态 -> 匿名 lambda 表：行序 = ZombieType 枚举序（0..33）；无 case
    // 槽=IceTrapNoAdjust 具名空操作（=原 default 无操作）。三浮点经
    // IceTrapAdjust 传递（读写回写）。
    IceTrapAdjust aAdjust{aOffsetX, aOffsetY, aScale, false};
    const unsigned aTypeIndex = static_cast<unsigned>(mZombieType);
    if (aTypeIndex < static_cast<unsigned>(ZombieType::NUM_ZOMBIE_TYPES))
    {
        kIceTrapAdjusts[aTypeIndex](*this, aAdjust);
    }
    aOffsetX = aAdjust.mDx;
    aOffsetY = aAdjust.mDy;
    aScale = aAdjust.mScale;


    TodDrawImageScaledF(g, theFront ? IMAGE_ICETRAP : IMAGE_ICETRAP2, aOffsetX, aOffsetY, aScale, aScale);
}

namespace
{
// Zombie::DrawButter 偏移调整表：表内匿名 lambda = 原 case 体逐句迁移
//（生成器 + 逆变换 token 对拍 0 差异）；无 case 槽不调整。
struct ButterAdjust
{
    float mDx;
    float mDy;
    float mScale;
};
using ButterAdjustFn = void (*)(Zombie&, ButterAdjust&);

// 标签共体（匿名 lambda 无法跨槽复用，复用即具名）
// 原 case ZOMBIE_GARGANTUAR, ZOMBIE_REDEYE_GARGANTUAR
void ButterAdjustCaseGargantuar(Zombie& , ButterAdjust& lc)
{
            lc.mDx -= 5.0f;
            lc.mDy -= 15.0f;
            lc.mScale = 1.2f;
}

// 原 default（无 case 的 28 型（空操作）共用）
void ButterNoAdjust(Zombie& , ButterAdjust& )
{

}

// 匿名 lambda 函数指针表，行序 = ZombieType 枚举值序（0..33）。
constexpr ButterAdjustFn kButterAdjusts[(int)ZombieType::NUM_ZOMBIE_TYPES] = {
    &ButterNoAdjust,  // 0 ZOMBIE_NORMAL（原 default）
    &ButterNoAdjust,  // 1 ZOMBIE_FLAG（原 default）
    &ButterNoAdjust,  // 2 ZOMBIE_TRAFFIC_CONE（原 default）
    &ButterNoAdjust,  // 3 ZOMBIE_POLEVAULTER（原 default）
    &ButterNoAdjust,  // 4 ZOMBIE_PAIL（原 default）
    &ButterNoAdjust,  // 5 ZOMBIE_NEWSPAPER（原 default）
    &ButterNoAdjust,  // 6 ZOMBIE_DOOR（原 default）
    &ButterNoAdjust,  // 7 ZOMBIE_FOOTBALL（原 default）
    &ButterNoAdjust,  // 8 ZOMBIE_DANCER（原 default）
    &ButterNoAdjust,  // 9 ZOMBIE_BACKUP_DANCER（原 default）
    &ButterNoAdjust,  // 10 ZOMBIE_DUCKY_TUBE（原 default）
    &ButterNoAdjust,  // 11 ZOMBIE_SNORKEL（原 default）
    &ButterNoAdjust,  // 12 ZOMBIE_ZAMBONI（原 default）
    &ButterNoAdjust,  // 13 ZOMBIE_BOBSLED（原 default）
    &ButterNoAdjust,  // 14 ZOMBIE_DOLPHIN_RIDER（原 default）
    &ButterNoAdjust,  // 15 ZOMBIE_JACK_IN_THE_BOX（原 default）
    &ButterNoAdjust,  // 16 ZOMBIE_BALLOON（原 default）
    &ButterNoAdjust,  // 17 ZOMBIE_DIGGER（原 default）
    // 原 case ZOMBIE_POGO
    [](Zombie& , ButterAdjust& lc)
    {
            lc.mDy -= 5.0f;
    },  // 18 ZOMBIE_POGO
    &ButterNoAdjust,  // 19 ZOMBIE_YETI（原 default）
    &ButterNoAdjust,  // 20 ZOMBIE_BUNGEE（原 default）
    &ButterNoAdjust,  // 21 ZOMBIE_LADDER（原 default）
    &ButterNoAdjust,  // 22 ZOMBIE_CATAPULT（原 default）
    &ButterAdjustCaseGargantuar,  // 23 ZOMBIE_GARGANTUAR（共体）
    &ButterNoAdjust,  // 24 ZOMBIE_IMP（原 default）
    &ButterNoAdjust,  // 25 ZOMBIE_BOSS（原 default）
    &ButterNoAdjust,  // 26 ZOMBIE_PEA_HEAD（原 default）
    // 原 case ZOMBIE_WALLNUT_HEAD
    [](Zombie& , ButterAdjust& lc)
    {
            lc.mDx -= 6.0f;
            lc.mDy -= 1.0f;
    },  // 27 ZOMBIE_WALLNUT_HEAD
    &ButterNoAdjust,  // 28 ZOMBIE_JALAPENO_HEAD（原 default）
    &ButterNoAdjust,  // 29 ZOMBIE_GATLING_HEAD（原 default）
    // 原 case ZOMBIE_SQUASH_HEAD
    [](Zombie& , ButterAdjust& lc)
    {
            lc.mDx += 6.0f;
            lc.mDy -= 9.0f;
    },  // 30 ZOMBIE_SQUASH_HEAD
    // 原 case ZOMBIE_TALLNUT_HEAD
    [](Zombie& , ButterAdjust& lc)
    {
            lc.mDx -= 24.0f;
            lc.mDy -= 39.0f;
    },  // 31 ZOMBIE_TALLNUT_HEAD
    &ButterAdjustCaseGargantuar,  // 32 ZOMBIE_REDEYE_GARGANTUAR（共体）
    &ButterNoAdjust,  // 33 ZOMBIE_ZOMBATAR（原 default）
};
static_assert(sizeof(kButterAdjusts) / sizeof(kButterAdjusts[0]) ==
    (int)ZombieType::NUM_ZOMBIE_TYPES, "kButterAdjusts 槽位数须与 ZombieType 枚举同步");
}  // namespace

//0x52E150
// GOTY @Patoke: 0x53EAD0
void Zombie::DrawButter(Graphics* g, const ZombieDrawPosition& theDrawPos)
{
    float aOffsetX = (mX) + theDrawPos.mImageOffsetX + theDrawPos.mHeadX + 11.0f;
    float aOffsetY = (mY) + theDrawPos.mImageOffsetY + theDrawPos.mHeadY + theDrawPos.mBodyY + 21.0f;
    float aScale = 1.0f;
    if (mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MADDENING)
    {
        GetTrackPosition(ReanimTrackId::anim_head_look, aOffsetX, aOffsetY);
    }
    else if (mZombieType == ZombieType::ZOMBIE_CATAPULT)
    {
        GetTrackPosition(ReanimTrackId::Zombie_catapult_driver_head, aOffsetX, aOffsetY);
    }
    else if (mBodyReanimID != ReanimationID::REANIMATIONID_NULL)
    {
        GetTrackPosition(ReanimTrackId::anim_head1, aOffsetX, aOffsetY);
    }
    aOffsetX -= (mX) + 29.0f;
    aOffsetY -= (mY) + 36.0f;

    // C 形态 -> 匿名 lambda 表：行序 = ZombieType 枚举序（0..33）；无 case
    // 槽=ButterNoAdjust 具名空操作（=原 default）。
    ButterAdjust aAdjust{aOffsetX, aOffsetY, aScale};
    const unsigned aTypeIndex = static_cast<unsigned>(mZombieType);
    if (aTypeIndex < static_cast<unsigned>(ZombieType::NUM_ZOMBIE_TYPES))
    {
        kButterAdjusts[aTypeIndex](*this, aAdjust);
    }
    aOffsetX = aAdjust.mDx;
    aOffsetY = aAdjust.mDy;
    aScale = aAdjust.mScale;


    TodDrawImageScaledF(g, IMAGE_REANIM_CORNPULT_BUTTER_SPLAT, aOffsetX, aOffsetY, aScale, aScale);
}

//0x52E2E0
// GOTY @Patoke: 0x53EC85
void Zombie::Draw(std::int64_t ownerNowNs, Graphics* g)
{
    const auto aStatusNowNs = ownerNowNs;

    if (mZombieHeight == ZombieHeight::HEIGHT_GETTING_BUNGEE_DROPPED)
        return;

    ZombieDrawPosition aDrawPos;
    GetDrawPos(aDrawPos);

    if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON && !SetupDrawZombieWon(g))
        return;

    if (mIceTrapEndTimeNs > aStatusNowNs)
    {
        DrawIceTrap(g, aDrawPos, false);
    }
    if (mApp->mGameMode != GameMode::GAMEMODE_MINIGAME_INVISIGHOUL || mFromWave == Zombie::ZOMBIE_WAVE_UI)
    {
        if (mBodyReanimID != ReanimationID::REANIMATIONID_NULL)
        {
            DrawReanim(ownerNowNs, g, aDrawPos, RENDER_GROUP_NORMAL);
        }
        else
        {
            //DrawZombie(g, aDrawPos);
        }
    }
    if (mIceTrapEndTimeNs > aStatusNowNs)
    {
        DrawIceTrap(g, aDrawPos, true);
    }
    if (mButteredEndTimeNs > aStatusNowNs)
    {
        DrawButter(g, aDrawPos);
    }

    if (mAttachmentID != AttachmentID::ATTACHMENTID_NULL)
    {
        Graphics theParticleGraphics(*g);
        MakeParentGraphicsFrame(&theParticleGraphics);
        theParticleGraphics.mTransY += aDrawPos.mBodyY;

        if (aDrawPos.mClipHeight > CLIP_HEIGHT_LIMIT)
        {
            float aDrawHeight = 120.0f - aDrawPos.mClipHeight + 21.0f;
            theParticleGraphics.ClipRect((mX) + aDrawPos.mImageOffsetX - 400.0f, (mY) + aDrawPos.mImageOffsetY - 28.0f, 920, aDrawHeight);
        }

        AttachmentDraw(mAttachmentID, &theParticleGraphics, false);
    }

    g->ClearClipRect();
}

//0x52E4C0
bool Zombie::CanTargetPlant(Plant* thePlant, ZombieAttackType theAttackType)
{
    if (mApp->IsWallnutBowlingLevel() && theAttackType != ZombieAttackType::ATTACKTYPE_VAULT)
        return false;

    if (thePlant->NotOnGround() || thePlant->mSeedType == SeedType::SEED_TANGLEKELP)
        return false;

    if (!mInPool && mBoard->IsPoolSquare(thePlant->mPlantCol, thePlant->mRow))
        return false;

    if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING)
    {
        return thePlant->mSeedType == SeedType::SEED_POTATOMINE && thePlant->mState == PlantState::STATE_NOTREADY;
    }

    if (thePlant->IsSpiky())
    {
        return
            mZombieType == ZombieType::ZOMBIE_GARGANTUAR ||
            mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR ||
            mZombieType == ZombieType::ZOMBIE_ZAMBONI ||
            mBoard->IsPoolSquare(thePlant->mPlantCol, thePlant->mRow) ||
            mBoard->GetFlowerPotAt(thePlant->mPlantCol, thePlant->mRow);  // 扶梯僵尸给花盆上的地刺/地刺王搭梯的原理
    }

    if (theAttackType == ZombieAttackType::ATTACKTYPE_DRIVE_OVER)
    {
        if (thePlant->mSeedType == SeedType::SEED_CHERRYBOMB || thePlant->mSeedType == SeedType::SEED_JALAPENO ||
            thePlant->mSeedType == SeedType::SEED_BLOVER || thePlant->mSeedType == SeedType::SEED_SQUASH)
        {
            return false;
        }
        if (thePlant->mSeedType == SeedType::SEED_DOOMSHROOM || thePlant->mSeedType == SeedType::SEED_ICESHROOM)
        {
            return thePlant->mIsAsleep;
        }
    }

    if (mZombiePhase == ZombiePhase::PHASE_LADDER_CARRYING || mZombiePhase == ZombiePhase::PHASE_LADDER_PLACING)
    {
        bool aPlaceLadder = false;
        if (thePlant->mSeedType == SeedType::SEED_WALLNUT || thePlant->mSeedType == SeedType::SEED_TALLNUT || thePlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
        {
            aPlaceLadder = true;
        }

        if (mBoard->GetLadderAt(thePlant->mPlantCol, thePlant->mRow))
        {
            aPlaceLadder = false;
        }

        if ((theAttackType == ZombieAttackType::ATTACKTYPE_CHEW && aPlaceLadder) || (theAttackType == ZombieAttackType::ATTACKTYPE_LADDER && !aPlaceLadder))
        {
            return false;
        }
    }

    if (theAttackType == ZombieAttackType::ATTACKTYPE_CHEW)
    {
        Plant* aTopPlant = mBoard->GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, PlantPriority::TOPPLANT_EATING_ORDER);
        if (aTopPlant != thePlant && aTopPlant && CanTargetPlant(aTopPlant, theAttackType))
        {
            return false;
        }
    }

    if (theAttackType == ZombieAttackType::ATTACKTYPE_VAULT)
    {
        Plant* aTopPlant = mBoard->GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
        if (aTopPlant != thePlant && aTopPlant && CanTargetPlant(aTopPlant, theAttackType))
        {
            return false;
        }
    }

    return true;
}

//0x52E780
Plant* Zombie::FindPlantTarget(ZombieAttackType theAttackType)
{
    // 行扫改格桶（2026-10-03）：本行 9 格×4 层 ≤36 槽 vs 池扫 0..高水位
    // （mMaxUsedCount，典型几十；2026-10-03 修正：原版本就只扫到高水位，非 1024 全扫）。
    // 原全池按槽位序首命中；桶按列序遍历——多目标并列时以**槽位号**最小
    // 者决胜（2026-10-03 二次修正：初版误用完整 ID 最小=世代键字典序≈
    // 最早创建序，非槽位序；取 ID 低 16 位=槽位号才是原语义）。玉米炮
    // 双格注册由同一植物两槽命中、同槽位号天然去重。NotOnGround 原版
    // 此处不过滤，保持不过滤。
    Rect aAttackRect = GetZombieAttackRect();

    Plant* aBest = nullptr;
    unsigned int aBestSlot = 0xFFFFFFFFu;
    for (int aCol = 0; aCol < MAX_GRID_SIZE_X; aCol++)
    {
        for (int aLayer = 0; aLayer < PLANT_CELL_COUNT; aLayer++)
        {
            // 链成员遍历（2026-10-03 二次修正：叠种时原池扫可见该层全部
            // 成员，桶头单点会漏影子株——沿链走全成员，最小槽位决胜不变；
            // 玉米炮双格双链命中由同一植物同槽位天然去重）
            Plant* aPlant = mBoard->GetCellPlant(mRow, aCol, aLayer);
            while (aPlant != nullptr)
            {
                Rect aPlantRect = aPlant->GetPlantRect();
                if (GetRectOverlap(aAttackRect, aPlantRect) >= 20 && CanTargetPlant(aPlant, theAttackType))
                {
                    const unsigned int aSlot = mBoard->PlantSlotOf(aPlant);
                    if (aSlot < aBestSlot)
                    {
                        aBestSlot = aSlot;
                        aBest = aPlant;
                    }
                }
                aPlant = mBoard->NextCellPlant(aPlant);
            }
        }
    }

    return aBest;
}

//0x52E840
Zombie* Zombie::FindZombieTarget()
{
    if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING)
        return nullptr;

    Rect aAttackRect = GetZombieAttackRect();

    Zombie* aZombie = nullptr;
    // 行段路径（2026-10-04 用户裁决简化）：归并迭代（行段∪全部 Boss）+
    // 体内裸 row== 检查按真实行过滤 Boss——与 1073 池扫逐字同构（被控
    // 近战可啃真实行上的 Boss）；first-match 语义由槽位归并序逐字保持
    while (mBoard->IterateZombiesInRow(mRow, aZombie))
    {
        if (mMindControlled != aZombie->mMindControlled &&
            !aZombie->IsFlying() &&
            aZombie->mZombiePhase != ZombiePhase::PHASE_DIGGER_TUNNELING &&
            aZombie->mZombiePhase != ZombiePhase::PHASE_BUNGEE_DIVING &&
            aZombie->mZombiePhase != ZombiePhase::PHASE_BUNGEE_DIVING_SCREAMING &&
            aZombie->mZombiePhase != ZombiePhase::PHASE_BUNGEE_RISING &&
            aZombie->mZombieHeight != ZombieHeight::HEIGHT_GETTING_BUNGEE_DROPPED &&
            !aZombie->IsDeadOrDying() &&
            aZombie->mRow == mRow)
        {
            Rect aZombieRect = aZombie->GetZombieRect();
            int aOverlap = GetRectOverlap(aAttackRect, aZombieRect);
            if (aOverlap >= 20 || (aOverlap >= 0 && aZombie->mIsEating))
            {
                return aZombie;
            }
        }
    }

    return nullptr;
}

//0x52E920
void Zombie::SquishAllInSquare(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theX, int theY, ZombieAttackType theAttackType)
{
    // 2026-10-03 格桶链枚举（用户方案）：原全池行格扫 → 该格 4 层链全成员
    // （含影子株——原池扫本就可见全部成员；已压扁/被拎株的免疫由 Squish()
    // 头部 NotOnGround 守卫处理，与原调用面逐字一致；玉米炮双格双链命中
    // 同一株，Squish 幂等由该守卫保证）。O(链) 替 O(高水位)。
    for (int aLayer = 0; aLayer < PLANT_CELL_COUNT; aLayer++)
    {
        Plant* aPlant = mBoard->GetCellPlant(theY, theX, aLayer);
        while (aPlant != nullptr)
        {
            Plant* aNext = mBoard->NextCellPlant(aPlant);  // 先取：Squish 不摘链但防御性前取
            if (!(theAttackType == ZombieAttackType::ATTACKTYPE_DRIVE_OVER && aPlant->IsSpiky()) &&
                aPlant->mSeedType != SeedType::SEED_SPIKEROCK)
            {
                mBoard->mPlantsEaten++;
                aPlant->Squish(ownerNowNs, visualNowNs, animationNowNs);
            }
            aPlant = aNext;
        }
    }
}

//0x52E9A0
void Zombie::ZamboniDeath(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, unsigned int theDamageFlags)
{
    if (TestBit(theDamageFlags, DamageFlags::DAMAGE_SPIKE))
    {
        mFlatTires = true;
        mVelocityX = {};
        mApp->PlayFoley(FoleyType::FOLEY_TIRE_POP);
        mZombiePhase = ZombiePhase::PHASE_ZOMBIE_DYING;
        mApp->AddTodParticle((mX) + 29.0f, (mY) + 114.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZAMBONI_TIRE, ownerNowNs);
        mVelX = 0.0f;
        mVelocityX.rate = mVelX;
        mVelocityX.startTimeNs = ownerNowNs;
        mVelocityX.bound = true;

        if (Rand(4) == 0 && (mX) < 600.0f)
        {
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_wheelie2, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 10.0f);
            mPhaseCounter = 280;
        }
        else
        {
            Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
            TodParticleSystem* aParticle = mApp->AddTodParticle(0.0f, 0.0f, 0, ParticleEffect::PARTICLE_ZAMBONI_SMOKE, ownerNowNs);
            if (aParticle)
            {
                aBodyReanim->AttachParticleToTrack(ReanimTrackId::Zombie_zamboni_1, aParticle, 35.0f, 85.0f);
            }

            mPhaseCounter = 280;
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_wheelie1, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 12.0f);
        }
    }
    else
    {
        mApp->AddTodParticle((mX) + 80.0f, (mY) + 60.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZAMBONI_EXPLOSION, ownerNowNs);
        DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
        mApp->PlayFoley(FoleyType::FOLEY_EXPLOSION);
    }
}

//0x52EC00
void Zombie::CatapultDeath(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, unsigned int theDamageFlags)
{
    if (TestBit(theDamageFlags, DamageFlags::DAMAGE_SPIKE))
    {
        mApp->PlayFoley(FoleyType::FOLEY_TIRE_POP);
        mZombiePhase = ZombiePhase::PHASE_ZOMBIE_DYING;
        mApp->AddTodParticle((mX) + 29.0f, (mY) + 114.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZAMBONI_TIRE, ownerNowNs);
        mVelX = 0.0f;

        AddAttachedParticle(ownerNowNs, 47, 77, ParticleEffect::PARTICLE_ZAMBONI_SMOKE);
        mPhaseCounter = 280;
        PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_bounce, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 12.0f);
    }
    else
    {
        mApp->AddTodParticle((mX) + 80.0f, (mY) + 60.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_CATAPULT_EXPLOSION, ownerNowNs);
        DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
        mApp->PlayFoley(FoleyType::FOLEY_EXPLOSION);
    }
}

//0x52EDB0
void Zombie::CheckSquish(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, ZombieAttackType theAttackType)
{
    Rect aAttackRect = GetZombieAttackRect();

    // 格桶链枚举（2026-10-03 第二批）：原为全池扫 mRow==mRow + 攻击矩形
    // 相交>=20 + CanTargetPlant + !IsSpiky 首命中即以该株 mPlantCol 调
    // SquishAllInSquare 并 break——行过滤改本行 9 格×4 层链全成员（含
    // 影子株=原池可见集），其余条件逐字保留；原站点不过滤 NotOnGround
    // （守卫在 CanTargetPlant 内），链枚举同样不过滤；池扫=槽位升序首
    // 命中 → 最小槽位号决胜（玉米炮双格双链命中同槽位号天然去重）。
    Plant* aBest = nullptr;
    unsigned int aBestSlot = 0xFFFFFFFFu;
    for (int aCol = 0; aCol < MAX_GRID_SIZE_X; aCol++)
    {
        for (int aLayer = 0; aLayer < PLANT_CELL_COUNT; aLayer++)
        {
            Plant* aPlant = mBoard->GetCellPlant(mRow, aCol, aLayer);
            while (aPlant != nullptr)
            {
                Rect aPlantRect = aPlant->GetPlantRect();
                if (GetRectOverlap(aAttackRect, aPlantRect) >= 20 && CanTargetPlant(aPlant, theAttackType) && !aPlant->IsSpiky())
                {
                    const unsigned int aSlot = mBoard->PlantSlotOf(aPlant);
                    if (aSlot < aBestSlot)
                    {
                        aBestSlot = aSlot;
                        aBest = aPlant;
                    }
                }
                aPlant = mBoard->NextCellPlant(aPlant);
            }
        }
    }

    if (aBest != nullptr)
    {
        SquishAllInSquare(ownerNowNs, visualNowNs, animationNowNs, aBest->mPlantCol, aBest->mRow, theAttackType);
    }

    if (mApp->IsIZombieLevel())
    {
        GridItem* aBrain = mBoard->mChallenge->IZombieGetBrainTarget(this);
        if (aBrain)
        {
            mBoard->mChallenge->IZombieSquishBrain(aBrain);
        }
    }
}

//0x52EEF0
bool Zombie::IsImmobilizied(std::int64_t ownerNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    return mIceTrapEndTimeNs > aStatusNowNs || mButteredEndTimeNs > aStatusNowNs;
}

//0x52EF10
bool Zombie::IsMovingAtChilledSpeed(std::int64_t ownerNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    if (mChilledEndTimeNs > aStatusNowNs)
        return true;

    if (mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
    {
        Zombie* aLeader;
        if (mZombieType == ZombieType::ZOMBIE_DANCER)
        {
            aLeader = this;
        }
        else
        {
            aLeader = mBoard->ZombieTryToGet(mRelatedZombieID);
        }

        if (aLeader)
        {
            if (aLeader->mChilledEndTimeNs > aStatusNowNs)
            {
                return true;
            }

            for (int i = 0; i < NUM_BACKUP_DANCERS; i++)
            {
                Zombie* aDancer = mBoard->ZombieTryToGet(aLeader->mFollowerZombieID[i]);
                if (aDancer && aDancer->mChilledEndTimeNs > aStatusNowNs)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

//0x52EFD0
void Zombie::SetAnimRate(std::int64_t ownerNowNs, float theAnimRate)
{
    mOriginalAnimRate = theAnimRate;
    ApplyAnimRate(ownerNowNs, theAnimRate);
}

//0x52EFF0
void Zombie::ApplyAnimRate(std::int64_t ownerNowNs, float theAnimRate)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (aBodyReanim)
    {

        aBodyReanim->ApplyRate(IsMovingAtChilledSpeed(ownerNowNs) ? theAnimRate * 0.5f : theAnimRate);
    }
}

//0x52F050
void Zombie::UpdateAnimSpeed(std::int64_t ownerNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    if (!IsOnBoard())
        return;

    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (aBodyReanim == nullptr)
        return;

    if (IsImmobilizied(ownerNowNs) || (mYuckyFace && aStatusNowNs-mYuckyFaceStartTimeNs < 1'700'000'000))
    {
        ApplyAnimRate(ownerNowNs, 0.0f);
        return;
    }

    if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_UP_TO_EAT || mZombiePhase == ZombiePhase::PHASE_SNORKEL_DOWN_FROM_EAT || IsDeadOrDying())
    {
        ApplyAnimRate(ownerNowNs, mOriginalAnimRate);
        return;
    }

    if (mIsEating)
    {
        if (mZombieType == ZombieType::ZOMBIE_POLEVAULTER || mZombieType == ZombieType::ZOMBIE_BALLOON || mZombieType == ZombieType::ZOMBIE_IMP ||
            mZombieType == ZombieType::ZOMBIE_DIGGER || mZombieType == ZombieType::ZOMBIE_JACK_IN_THE_BOX || mZombieType == ZombieType::ZOMBIE_SNORKEL ||
            mZombieType == ZombieType::ZOMBIE_YETI)
        {
            ApplyAnimRate(ownerNowNs, 20.0f);
        }
        else
        {
            ApplyAnimRate(ownerNowNs, 36.0f);
        }
    }
    else
    {
        if (ZombieNotWalking(ownerNowNs) || IsBobsledTeamWithSled() || mZombieType == ZombieType::ZOMBIE_CATAPULT ||
            mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING || mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL)
        {
            ApplyAnimRate(ownerNowNs, mOriginalAnimRate);
        }
        else if (aBodyReanim->TrackExists(ReanimTrackId::_ground))
        {
            ReanimatorTrack* aTrack = &aBodyReanim->mDefinition->mTracks.tracks[aBodyReanim->FindTrackIndex(ReanimTrackId::_ground)];
            if (!aTrack->mTransforms.mTransforms)
                throw std::logic_error("Zombie::UpdateAnimSpeed requires CPU _ground keyframes");
            const double aDistance = static_cast<double>(
                aTrack->mTransforms.mTransforms[aBodyReanim->mFrameStart + aBodyReanim->mFrameCount - 1].mTransX) -
                static_cast<double>(aTrack->mTransforms.mTransforms[aBodyReanim->mFrameStart].mTransX);
            if (aDistance >= 1e-6f)
            {
                float aOneOverSpeed = static_cast<float>(aBodyReanim->mFrameCount / aDistance);
                float aAnimRate = mVelX * aOneOverSpeed * 47.0f / mScaleZombie;
                ApplyAnimRate(ownerNowNs, aAnimRate);
            }
        }
    }
}

void Zombie::ConvertToNormalZombie(std::int64_t ownerNowNs)
{
    StopZombieSound();
    mY = (GetPosYBasedOnRow(mRow));



    mZombieType = ZombieType::ZOMBIE_NORMAL;
    mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
    mZombieAttackRect = Rect(50, 0, 20, 115);

    mAnimFrames = 12;
    mAnimTicksPerFrame = 12;
    mPhaseCounter = 0;

    PickRandomSpeed(ownerNowNs);
}

//0x52F250
void Zombie::StartEating(std::int64_t ownerNowNs)
{
    if (mIsEating)
        return;

    mIsEating = true;

    if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING)
        return;

    if (mZombiePhase == ZombiePhase::PHASE_LADDER_CARRYING)
    {
        PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_laddereat, ReanimLoopType::REANIM_LOOP, 20, 0.0f);
    }
    else if (mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MAD)
    {
        PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_eat_nopaper, ReanimLoopType::REANIM_LOOP, 20, 0.0f);
    }
    else
    {
        if (mZombieType != ZombieType::ZOMBIE_SNORKEL)
        {
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_eat, ReanimLoopType::REANIM_LOOP, 20, 0.0f);
        }

        if (mShieldType == ShieldType::SHIELDTYPE_DOOR)
        {
            ShowDoorArms(false);
        }
    }
}

//0x52F2E0
void Zombie::StartWalkAnim(std::int64_t ownerNowNs, int theBlendTime)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (aBodyReanim == nullptr)
        return;

    PickRandomSpeed(ownerNowNs);
    if (mZombiePhase == ZombiePhase::PHASE_LADDER_CARRYING)
    {
        PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_ladderwalk, ReanimLoopType::REANIM_LOOP, theBlendTime, 0.0f);
    }
    else if (mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MAD)
    {
        PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_walk_nopaper, ReanimLoopType::REANIM_LOOP, theBlendTime, 0.0f);
    }
    else if (mInPool && mZombieHeight != ZombieHeight::HEIGHT_IN_TO_POOL && mZombieHeight != ZombieHeight::HEIGHT_OUT_OF_POOL && aBodyReanim->TrackExists(ReanimTrackId::anim_swim))
    {
        PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_swim, ReanimLoopType::REANIM_LOOP, theBlendTime, 0.0f);
    }
    else if ((mZombieType == ZombieType::ZOMBIE_NORMAL || mZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE || mZombieType == ZombieType::ZOMBIE_PAIL) && mBoard->mDanceMode)
    {
        PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_dance, ReanimLoopType::REANIM_LOOP, theBlendTime, 0.0f);
    }
    else
    {
        int aWalkAnimVariant = Rand(2);
        if (mZombieType == ZombieType::ZOMBIE_PEA_HEAD)
        {
            aWalkAnimVariant = 0;
        }
        if (mZombieType == ZombieType::ZOMBIE_FLAG)
        {
            aWalkAnimVariant = 0;
        }

        if (aWalkAnimVariant == 0 && aBodyReanim->TrackExists(ReanimTrackId::anim_walk2))
        {
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_walk2, ReanimLoopType::REANIM_LOOP, theBlendTime, 0.0f);
        }
        else if (aBodyReanim->TrackExists(ReanimTrackId::anim_walk))
        {
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_walk, ReanimLoopType::REANIM_LOOP, theBlendTime, 0.0f);
        }
    }
}

//0x52F440
void Zombie::StopEating(std::int64_t ownerNowNs)
{
    if (!mIsEating)
        return;

    mIsEating = false;
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

    if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING)
        return;

    if (aBodyReanim && mZombieType != ZombieType::ZOMBIE_SNORKEL)
    {
        StartWalkAnim(ownerNowNs, 20);
    }

    if (mShieldType == ShieldType::SHIELDTYPE_DOOR)
    {
        ShowDoorArms(true);
    }

    UpdateAnimSpeed(ownerNowNs);
}

//0x52F4C0
void Zombie::CheckIfPreyCaught(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    if (mZombieType == ZombieType::ZOMBIE_BUNGEE ||
        mZombieType == ZombieType::ZOMBIE_GARGANTUAR ||
        mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR ||
        mZombieType == ZombieType::ZOMBIE_ZAMBONI ||
        mZombieType == ZombieType::ZOMBIE_CATAPULT ||
        ((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0) ||
        IsBouncingPogo() ||
        IsBobsledTeamWithSled() ||
        mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT ||
        mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT ||
        mZombiePhase == ZombiePhase::PHASE_NEWSPAPER_MADDENING ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_STUNNED ||
        mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE ||
        mZombiePhase == ZombiePhase::PHASE_IMP_GETTING_THROWN ||
        mZombiePhase == ZombiePhase::PHASE_IMP_LANDING ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_RISING ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_WITH_LIGHT ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_SNAPPING_FINGERS_HOLD ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING_WITHOUT_DOLPHIN ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_INTO_POOL ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP ||
        mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL ||
        mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING ||
        mZombiePhase == ZombiePhase::PHASE_LADDER_PLACING ||
        mZombieHeight == ZombieHeight::HEIGHT_GETTING_BUNGEE_DROPPED ||
        mZombieHeight == ZombieHeight::HEIGHT_UP_LADDER ||
        mZombieHeight == ZombieHeight::HEIGHT_IN_TO_POOL ||
        mZombieHeight == ZombieHeight::HEIGHT_OUT_OF_POOL ||
        IsTangleKelpTarget() ||
        mZombieHeight == ZombieHeight::HEIGHT_FALLING ||
        !mHasHead ||
        IsFlying())
        return;

    int aTicksBetweenEats = TICKS_BETWEEN_EATS;
    if (mChilledEndTimeNs > aStatusNowNs)
    {
        aTicksBetweenEats *= 2;
    }
    if (!ZombieAgeCrossedPeriod(aStatusNowNs-mBirthTimeNs, Sexy::gUpdateIntervalNs, static_cast<std::int64_t>(aTicksBetweenEats)*10'000'000))
    {
        return;
    }

    Zombie* aZombie = FindZombieTarget();
    if (aZombie)
    {
        EatZombie(ownerNowNs, visualNowNs, animationNowNs, aZombie);
        return;
    }

    if (!mMindControlled)
    {
        Plant* aPlant = FindPlantTarget(ZombieAttackType::ATTACKTYPE_CHEW);
        if (aPlant)
        {
            EatPlant(ownerNowNs, visualNowNs, animationNowNs, aPlant);
            return;
        }
    }

    if (mApp->IsIZombieLevel() && mBoard->mChallenge->IZombieEatBrain(this))
    {
        return;
    }

    if (mIsEating)
    {
        StopEating(ownerNowNs);
    }
}

//0x52F6D0
void Zombie::PoolSplash(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, bool theInToPoolSound)
{
    float aOffsetX = 23.0f;
    float aOffsetY = 78.0f;
    if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL)
    {
        aOffsetX -= 37.0f;
        aOffsetY -= -8.0f;
    }

    Reanimation* aSplashReanim = mApp->AddReanimation((mX) + aOffsetX, (mY) + aOffsetY, mRenderOrder + 1, ReanimationType::REANIM_SPLASH, visualNowNs);
    aSplashReanim->mOverlayMatrix.m00 = 0.8f;
    aSplashReanim->mOverlayMatrix.m11 = 0.8f;
    mApp->AddTodParticle((mX) + aOffsetX + 37.0f, (mY) + aOffsetY + 42.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_PLANTING_POOL, ownerNowNs);

    if (theInToPoolSound)
    {
        mApp->PlayFoley(FoleyType::FOLEY_ZOMBIESPLASH);
    }
    else
    {
        mApp->PlayFoley(FoleyType::FOLEY_PLANT_WATER);
    }
}

//0x52F870
void Zombie::CheckForPool(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    if (!Zombie::ZombieTypeCanGoInPool(mZombieType) || IsFlying())
    {
        return;
    }
    if (mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER || mZombieType == ZombieType::ZOMBIE_SNORKEL)
    {
        return;
    }
    if (mZombieHeight == ZombieHeight::HEIGHT_IN_TO_POOL || mZombieHeight == ZombieHeight::HEIGHT_OUT_OF_POOL)
    {
        return;
    }

    bool aIsPoolSquare =
        mBoard->IsPoolSquare(mBoard->PixelToGridX((mX) + 75, (mY)), mRow) &&
        mBoard->IsPoolSquare(mBoard->PixelToGridX((mX) + 45, (mY)), mRow) &&
        (mX) < 680;

    if (!mInPool && aIsPoolSquare)
    {
        if (mBoard->mIceTrapCounter > 0)
        {
            mIceTrapEndTimeNs = aStatusNowNs + mBoard->mIceTrapCounter.mNanoseconds;
            ApplyChill(ownerNowNs, true);
        }
        else
        {
            mZombieHeight = ZombieHeight::HEIGHT_IN_TO_POOL;
            mInPool = true;
            PoolSplash(ownerNowNs, visualNowNs, animationNowNs, true);
        }
    }
    else if (mInPool && !aIsPoolSquare)
    {
        mZombieHeight = ZombieHeight::HEIGHT_OUT_OF_POOL;
        StartWalkAnim(ownerNowNs, 0);
        PoolSplash(ownerNowNs, visualNowNs, animationNowNs, false);
    }
}

//0x52F9C0
// GOTY @Patoke: 0x540350
bool Zombie::IsOnHighGround()
{
    return IsOnBoard() && mBoard->mGridSquareType[mBoard->PixelToGridXKeepOnBoard((mX) + 75, (mY))][mRow] == GridSquareType::GRIDSQUARE_HIGH_GROUND;
}

//0x52FA20
void Zombie::CheckForHighGround()
{
    if (mZombieHeight != ZombieHeight::HEIGHT_ZOMBIE_NORMAL || mZombieType == ZombieType::ZOMBIE_BUNGEE)
        return;

    bool aIsHighGround = IsOnHighGround();
    if (!mOnHighGround && aIsHighGround)
    {
        mZombieHeight = ZombieHeight::HEIGHT_UP_TO_HIGH_GROUND;
        mOnHighGround = true;
    }
    else if (mOnHighGround && !aIsHighGround)
    {
        mZombieHeight = ZombieHeight::HEIGHT_DOWN_OFF_HIGH_GROUND;
    }
}

//0x52FA60
void Zombie::StartMindControlled()
{
    mApp->PlaySample(SOUND_MINDCONTROLLED);
    mMindControlled = true;
    if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
        mVelocityX = {};
    mLastPortalX = -1;

    if (mZombieType == ZombieType::ZOMBIE_DANCER)
    {
        for (int i = 0; i < NUM_BACKUP_DANCERS; i++)
        {
            mFollowerZombieID[i] = ZombieID::ZOMBIEID_NULL;
        }
    }
    else if (mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
    {
        Zombie* aLeader = mBoard->ZombieTryToGet(mRelatedZombieID);
        if (aLeader)
        {
            ZombieID aId = mBoard->ZombieGetID(this);
            for (int i = 0; i < NUM_BACKUP_DANCERS; i++)
            {
                if (aLeader->mFollowerZombieID[i] == aId)
                {
                    aLeader->mFollowerZombieID[i] = ZombieID::ZOMBIEID_NULL;
                    break;
                }
            }
        }

        mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
    }
    else
    {
        Zombie* aZombie = mBoard->ZombieTryToGet(mRelatedZombieID);
        if (aZombie)
        {
            aZombie->mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
            mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
        }
    }
}

//0x52FB40
void Zombie::EatPlant(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* thePlant)
{
    const auto aStatusNowNs = ownerNowNs;

    if (mZombiePhase == ZombiePhase::PHASE_DANCER_DANCING_IN)
    {
        mPhaseCounter = 1;
        return;
    }

    if (mYuckyFace)
        return;

    if (mBoard->GetLadderAt(thePlant->mPlantCol, thePlant->mRow) && mZombieType != ZombieType::ZOMBIE_DIGGER)  // 矿工僵尸无视梯子
    {
        StopEating(ownerNowNs);

        if (mZombieHeight == ZombieHeight::HEIGHT_ZOMBIE_NORMAL && mUseLadderCol != thePlant->mPlantCol)
        {
            mZombieHeight = ZombieHeight::HEIGHT_UP_LADDER;
            mUseLadderCol = thePlant->mPlantCol;
        }

        return;
    }

    StartEating(ownerNowNs);
    if (thePlant->mSeedType == SeedType::SEED_JALAPENO ||
        thePlant->mSeedType == SeedType::SEED_CHERRYBOMB ||
        thePlant->mSeedType == SeedType::SEED_DOOMSHROOM ||
        thePlant->mSeedType == SeedType::SEED_ICESHROOM ||
        thePlant->mSeedType == SeedType::SEED_HYPNOSHROOM ||
        thePlant->mState == PlantState::STATE_FLOWERPOT_INVULNERABLE ||
        thePlant->mState == PlantState::STATE_LILYPAD_INVULNERABLE ||
        thePlant->mState == PlantState::STATE_SQUASH_LOOK ||
        thePlant->mState == PlantState::STATE_SQUASH_PRE_LAUNCH)
    {
        if (!thePlant->mIsAsleep)
        {
            return;
        }
    }
    if (thePlant->mSeedType == SeedType::SEED_POTATOMINE && thePlant->mState != PlantState::STATE_NOTREADY)
    {
        return;
    }

    bool triggered = false;
    if (thePlant->mSeedType == SeedType::SEED_BLOVER)
    {
        triggered = true;
    }
    if (thePlant->mSeedType == SeedType::SEED_ICESHROOM  && !thePlant->mIsAsleep)
    {
        triggered = true;
    }
    if (triggered)
    {
        thePlant->DoSpecial(ownerNowNs, visualNowNs, animationNowNs);
        return;
    }

    if (mChilledEndTimeNs > aStatusNowNs && !ZombieAgeCrossedPeriod(aStatusNowNs-mBirthTimeNs, Sexy::gUpdateIntervalNs, 20'000'000))
        return;

    if (mApp->IsIZombieLevel() && thePlant->mSeedType == SeedType::SEED_SUNFLOWER)
    {
        int aStageBeforeChew = thePlant->mPlantHealth / 40;
        int aStageAfterChew = (thePlant->mPlantHealth - DAMAGE_PER_EAT) / 40;
        if (aStageAfterChew < aStageBeforeChew || thePlant->mPlantHealth - DAMAGE_PER_EAT <= 0)  // 若本次啃食令植物血量下降了至少 1 个阶段
        {
            mBoard->AddCoin((thePlant->mX), (thePlant->mY), CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT);
        }
    }

    thePlant->mPlantHealth -= DAMAGE_PER_EAT;
    thePlant->mRecentlyEatenEndTimeNs = ownerNowNs+500'000'000;
    if (mApp->IsIZombieLevel() && aStatusNowNs - mShotFlashEndTimeNs > 5'000'000'000LL)
    {
        if (thePlant->mSeedType == SeedType::SEED_WALLNUT || thePlant->mSeedType == SeedType::SEED_TALLNUT || thePlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
        {
            thePlant->mPlantHealth -= DAMAGE_PER_EAT;
        }
    }

    if (thePlant->mPlantHealth <= 0)
    {
        mApp->PlaySample(SOUND_GULP);

        mBoard->mPlantsEaten++;
        thePlant->Die(ownerNowNs, visualNowNs, animationNowNs);
        mBoard->mChallenge->ZombieAtePlant(thePlant);

        if (mBoard->mLevel >= 2 && mBoard->mLevel <= 4 && mApp->IsFirstTimeAdventureMode())
        {
            if (thePlant->mPlantCol > 4 && mBoard->mPlants.mSize < 15 && thePlant->mSeedType == SeedType::SEED_PEASHOOTER)
            {
                mBoard->DisplayAdvice(gStr.ADVICE_PEASHOOTER_DIED, MessageStyle::MESSAGE_STYLE_HINT_TALL_FAST, AdviceType::ADVICE_PEASHOOTER_DIED);
            }
        }
    }
}

//0x52FE10
void Zombie::EatZombie(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie* theZombie)
{
    theZombie->TakeDamage(ownerNowNs, visualNowNs, animationNowNs, DAMAGE_PER_EAT, 9U);
    StartEating(ownerNowNs);
    if (theZombie->mBodyHealth <= 0)
    {
        mApp->PlaySample(SOUND_GULP);
    }
}

//0x52FE50
bool Zombie::TrySpawnLevelAward()
{
    if (!IsOnBoard() || mBoard->HasLevelAwardDropped() || mBoard->mLevelComplete || mDroppedLoot)
    {
        return false;
    }

    if (mApp->IsFinalBossLevel())
    {
        if (mZombieType != ZombieType::ZOMBIE_BOSS)
        {
            return false;
        }
    }
    else if (mApp->IsScaryPotterLevel())
    {
        if (!mBoard->mChallenge->ScaryPotterIsCompleted())
        {
            return false;
        }
    }
    else if (mApp->IsContinuousChallenge() || mBoard->mCurrentWave < mBoard->mNumWaves || mBoard->AreEnemyZombiesOnScreen())
    {
        return false;
    }

    if (mApp->IsWhackAZombieLevel() && mBoard->mZombieCountDown > 0)
    {
        return false;
    }

    mBoard->mLevelAwardSpawned = true;
    mApp->mBoardResult = BoardResult::BOARDRESULT_WON;

    Rect aZombieRect = GetZombieRect();
    int aCenterX = aZombieRect.mX + aZombieRect.mWidth / 2;
    int aCenterY = aZombieRect.mY + aZombieRect.mHeight / 2;

    if (!mBoard->IsSurvivalStageWithRepick())
    {
        mBoard->RemoveAllZombies();
    }

    CoinType aCoinType;
    if (mApp->IsScaryPotterLevel() && !mBoard->IsFinalScaryPotterStage())
    {
        aCoinType = CoinType::COIN_NONE;
        mBoard->mChallenge->PuzzlePhaseComplete(mBoard->PixelToGridXKeepOnBoard((mX) + 75, (mY)), mRow);
    }
    else if (mApp->IsAdventureMode() && mBoard->mLevel <= 50)
    {
        // B 形态 -> 常量表 kAdventureLevelAwards（原 8 支链合并）：行序 = 关卡
        // 号 0..50 直下标（0 槽占位，冒险模式关卡从 1 起）。表值 = 未通关冒险
        // 时的奖励；已通关（HasFinishedAdventure）覆盖规则表外保留：NOTE 关卡
        //（9/19/29/39/49）不受覆盖，其余一律 MONEY_BAG（原 50 关三元式与链上
        // HasFinishedAdventure 支的合并语义，两支同为 MONEY_BAG）。
        static constexpr CoinType kAdventureLevelAwards[51] = {
            CoinType::COIN_FINAL_SEED_PACKET,  // [0] 占位（冒险模式不可达，值同 default）
            CoinType::COIN_FINAL_SEED_PACKET,  // [1]
            CoinType::COIN_FINAL_SEED_PACKET,  // [2]
            CoinType::COIN_FINAL_SEED_PACKET,  // [3]
            CoinType::COIN_SHOVEL,             // [4] 铲子
            CoinType::COIN_FINAL_SEED_PACKET,  // [5]
            CoinType::COIN_FINAL_SEED_PACKET,  // [6]
            CoinType::COIN_FINAL_SEED_PACKET,  // [7]
            CoinType::COIN_FINAL_SEED_PACKET,  // [8]
            CoinType::COIN_NOTE,               // [9] 音乐盒纸条
            CoinType::COIN_FINAL_SEED_PACKET,  // [10]
            CoinType::COIN_FINAL_SEED_PACKET,  // [11]
            CoinType::COIN_FINAL_SEED_PACKET,  // [12]
            CoinType::COIN_FINAL_SEED_PACKET,  // [13]
            CoinType::COIN_ALMANAC,            // [14] 图鉴
            CoinType::COIN_FINAL_SEED_PACKET,  // [15]
            CoinType::COIN_FINAL_SEED_PACKET,  // [16]
            CoinType::COIN_FINAL_SEED_PACKET,  // [17]
            CoinType::COIN_FINAL_SEED_PACKET,  // [18]
            CoinType::COIN_NOTE,               // [19] 音乐盒纸条
            CoinType::COIN_FINAL_SEED_PACKET,  // [20]
            CoinType::COIN_FINAL_SEED_PACKET,  // [21]
            CoinType::COIN_FINAL_SEED_PACKET,  // [22]
            CoinType::COIN_FINAL_SEED_PACKET,  // [23]
            CoinType::COIN_CARKEYS,            // [24] 车钥匙
            CoinType::COIN_FINAL_SEED_PACKET,  // [25]
            CoinType::COIN_FINAL_SEED_PACKET,  // [26]
            CoinType::COIN_FINAL_SEED_PACKET,  // [27]
            CoinType::COIN_FINAL_SEED_PACKET,  // [28]
            CoinType::COIN_NOTE,               // [29] 音乐盒纸条
            CoinType::COIN_FINAL_SEED_PACKET,  // [30]
            CoinType::COIN_FINAL_SEED_PACKET,  // [31]
            CoinType::COIN_FINAL_SEED_PACKET,  // [32]
            CoinType::COIN_FINAL_SEED_PACKET,  // [33]
            CoinType::COIN_TACO,               // [34] 塔可
            CoinType::COIN_FINAL_SEED_PACKET,  // [35]
            CoinType::COIN_FINAL_SEED_PACKET,  // [36]
            CoinType::COIN_FINAL_SEED_PACKET,  // [37]
            CoinType::COIN_FINAL_SEED_PACKET,  // [38]
            CoinType::COIN_NOTE,               // [39] 音乐盒纸条
            CoinType::COIN_FINAL_SEED_PACKET,  // [40]
            CoinType::COIN_FINAL_SEED_PACKET,  // [41]
            CoinType::COIN_FINAL_SEED_PACKET,  // [42]
            CoinType::COIN_FINAL_SEED_PACKET,  // [43]
            CoinType::COIN_WATERING_CAN,       // [44] 浇水壶
            CoinType::COIN_FINAL_SEED_PACKET,  // [45]
            CoinType::COIN_FINAL_SEED_PACKET,  // [46]
            CoinType::COIN_FINAL_SEED_PACKET,  // [47]
            CoinType::COIN_FINAL_SEED_PACKET,  // [48]
            CoinType::COIN_NOTE,               // [49] 音乐盒纸条
            CoinType::COIN_AWARD_SILVER_SUNFLOWER,  // [50] 银色向日葵（通关后为 MONEY_BAG）
        };
        static_assert(sizeof(kAdventureLevelAwards) / sizeof(kAdventureLevelAwards[0]) == 51,
            "kAdventureLevelAwards 槽位须覆盖关卡 0..50");
        static_assert(kAdventureLevelAwards[4] == CoinType::COIN_SHOVEL &&
            kAdventureLevelAwards[9] == CoinType::COIN_NOTE &&
            kAdventureLevelAwards[14] == CoinType::COIN_ALMANAC &&
            kAdventureLevelAwards[19] == CoinType::COIN_NOTE &&
            kAdventureLevelAwards[24] == CoinType::COIN_CARKEYS &&
            kAdventureLevelAwards[29] == CoinType::COIN_NOTE &&
            kAdventureLevelAwards[34] == CoinType::COIN_TACO &&
            kAdventureLevelAwards[39] == CoinType::COIN_NOTE &&
            kAdventureLevelAwards[44] == CoinType::COIN_WATERING_CAN &&
            kAdventureLevelAwards[49] == CoinType::COIN_NOTE &&
            kAdventureLevelAwards[50] == CoinType::COIN_AWARD_SILVER_SUNFLOWER &&
            kAdventureLevelAwards[1] == CoinType::COIN_FINAL_SEED_PACKET &&
            kAdventureLevelAwards[25] == CoinType::COIN_FINAL_SEED_PACKET &&
            kAdventureLevelAwards[48] == CoinType::COIN_FINAL_SEED_PACKET,
            "kAdventureLevelAwards 关键槽位锚点错位");

        const unsigned aLevelIndex = static_cast<unsigned>(mBoard->mLevel);
        const CoinType aLevelAward = aLevelIndex < 51 ?
            kAdventureLevelAwards[aLevelIndex] : CoinType::COIN_FINAL_SEED_PACKET;
        if (aLevelAward == CoinType::COIN_NOTE)
        {
            aCoinType = aLevelAward;
        }
        else
        {
            aCoinType = mApp->HasFinishedAdventure() ? CoinType::COIN_AWARD_MONEY_BAG : aLevelAward;
        }
    }
    else if (mBoard->IsSurvivalStageWithRepick())
    {
        aCoinType = CoinType::COIN_NONE;
        mBoard->FadeOutLevel();
    }
    else if (mBoard->IsLastStandStageWithRepick())
    {
        aCoinType = CoinType::COIN_NONE;

        mBoard->FadeOutLevel();
        mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
        for (int i = 0; i < 10; i++)
        {
            mBoard->AddCoin(aCenterX + i * 5, aCenterY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_COIN);
        }
    }
    else if (!mApp->IsAdventureMode())
    {
        if (mApp->HasBeatenChallenge(mApp->mGameMode))
        {
            aCoinType = CoinType::COIN_AWARD_MONEY_BAG;
        }
        else if (mApp->TrophiesNeedForGoldSunflower() == 1)
        {
            aCoinType = CoinType::COIN_AWARD_GOLD_SUNFLOWER;
        }
        else
        {
            aCoinType = CoinType::COIN_TROPHY;
        }
    }
    else
    {
        aCoinType = CoinType::COIN_AWARD_MONEY_BAG;
    }

    CoinMotion aCoinMotion = CoinMotion::COIN_MOTION_COIN;
    if (((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0))
    {
        aCoinMotion = CoinMotion::COIN_MOTION_FROM_BOSS;
    }

    if (aCoinType != CoinType::COIN_NONE)
    {
        mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
        mBoard->AddCoin(aCenterX, aCenterY, aCoinType, aCoinMotion);
    }

    mDroppedLoot = true;
    return true;
}

//0x530170
void Zombie::DropLoot()
{
    if (!IsOnBoard())
        return;

    AlmanacPlayerDefeatedZombie(mZombieType);
    if (mZombieType == ZombieType::ZOMBIE_YETI)
    {
        mBoard->mKilledYeti = true;
    }

    TrySpawnLevelAward();
    if (mDroppedLoot || mBoard->HasLevelAwardDropped() || !mBoard->CanDropLoot())
        return;

    mDroppedLoot = true;
    int aZombieValue = GetZombieDefinition(mZombieType).mZombieValue;
    if (mApp->IsLittleTroubleLevel() && Rand(4) != 0)
    {
        return;
    }
    if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM || mApp->IsIZombieLevel())
    {
        return;
    }

    Rect aZombieRect = GetZombieRect();
    int aCenterX = aZombieRect.mX + aZombieRect.mWidth / 2;
    int aCenterY = aZombieRect.mY + aZombieRect.mHeight / 4;
    if (mZombieType == ZombieType::ZOMBIE_YETI)
    {
        mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
        mBoard->AddCoin(aCenterX - 20, aCenterY, CoinType::COIN_DIAMOND, CoinMotion::COIN_MOTION_COIN);
        mBoard->AddCoin(aCenterX - 30, aCenterY, CoinType::COIN_DIAMOND, CoinMotion::COIN_MOTION_COIN);
        mBoard->AddCoin(aCenterX - 40, aCenterY, CoinType::COIN_DIAMOND, CoinMotion::COIN_MOTION_COIN);
        mBoard->AddCoin(aCenterX - 50, aCenterY, CoinType::COIN_DIAMOND, CoinMotion::COIN_MOTION_COIN);
    }
    else
    {
        mBoard->DropLootPiece(aCenterX, aCenterY, aZombieValue);
    }
}

//0x5302F0
void Zombie::DieWithLoot(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
    DropLoot();
}

//0x530310
void Zombie::BobsledDie(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (!IsBobsledTeamWithSled() || !IsOnBoard())
        return;

    Zombie* aLeaderZombie;
    if (mRelatedZombieID == ZombieID::ZOMBIEID_NULL)
    {
        aLeaderZombie = this;
    }
    else
    {
        aLeaderZombie = mBoard->ZombieGet(mRelatedZombieID);
    }

    if (!aLeaderZombie->mDead)
    {
        aLeaderZombie->DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
    }
    for (int i = 0; i < NUM_BOBSLED_FOLLOWERS; i++)
    {
        Zombie* aZombie = mBoard->ZombieGet(aLeaderZombie->mFollowerZombieID[i]);
        if (!aZombie->mDead)
        {
            aZombie->DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
        }
    }
}

//0x5303A0
void Zombie::BobsledBurn(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (!IsBobsledTeamWithSled())
        return;

    Zombie* aLeaderZombie;
    if (mRelatedZombieID == ZombieID::ZOMBIEID_NULL)
    {
        aLeaderZombie = this;
    }
    else
    {
        aLeaderZombie = mBoard->ZombieGet(mRelatedZombieID);
    }

    aLeaderZombie->ApplyBurn(ownerNowNs, visualNowNs, animationNowNs);
    for (int i = 0; i < NUM_BOBSLED_FOLLOWERS; i++)
    {
        mBoard->ZombieGet(aLeaderZombie->mFollowerZombieID[i])->DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
    }
}

//0x530410
void Zombie::BungeeDropPlant(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (mZombiePhase == ZombiePhase::PHASE_BUNGEE_GRABBING)
    {
        Plant* aPlant = mBoard->mPlants.DataArrayTryToGet(mTargetPlantID);
        if (aPlant)
        {
            if (aPlant->mOnBungeeState == PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
            {
                aPlant->mOnBungeeState = PlantOnBungeeState::NOT_ON_BUNGEE;
            }
            else if (aPlant->mOnBungeeState == PlantOnBungeeState::RISING_WITH_BUNGEE)
            {
                aPlant->Die(ownerNowNs, visualNowNs, animationNowNs);
            }

            mTargetPlantID = PlantID::PLANTID_NULL;
        }
    }
}

//0x530480
void Zombie::BungeeDie(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    BungeeDropPlant(ownerNowNs, visualNowNs, animationNowNs);

    if (mBoard)  // 原版没有这个判断，因为 mBoard 为空时 DataArrayTryToGet() 不会实际用到 mBoard，此处为了确保安全就加上了这个判断
    {
        Plant* aPlant = mBoard->mPlants.DataArrayTryToGet((unsigned int)mTargetPlantID);
        if (aPlant)
        {
            mBoard->mPlantsEaten++;
            aPlant->Die(ownerNowNs, visualNowNs, animationNowNs);
        }
    }

    Zombie* aZombie = mBoard->ZombieTryToGet(mRelatedZombieID);
    if (aZombie && !aZombie->mDead)
    {
        aZombie->DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
    }
}

//0x530510
void Zombie::DieNoLoot(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    StopZombieSound();
    AttachmentDie(mAttachmentID);
    mApp->RemoveReanimation(mBodyReanimID);
    mApp->RemoveReanimation(mMoweredReanimID);
    mApp->RemoveReanimation(mSpecialHeadReanimID);

    mDead = true;
    TrySpawnLevelAward();
    if (mZombieType == ZombieType::ZOMBIE_BOBSLED)
    {
        BobsledDie(ownerNowNs, visualNowNs, animationNowNs);
    }
    if (mZombieType == ZombieType::ZOMBIE_BUNGEE)
    {
        BungeeDie(ownerNowNs, visualNowNs, animationNowNs);
    }
    if (((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0))
    {
        BossDie(ownerNowNs, visualNowNs, animationNowNs);
    }
}

//0x530640
void Zombie::PlayZombieAppearSound()
{
    if (mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER)
    {
        mApp->PlayFoley(FoleyType::FOLEY_DOLPHIN_APPEARS);
    }
    else if (mZombieType == ZombieType::ZOMBIE_BALLOON)
    {
        mApp->PlayFoley(FoleyType::FOLEY_BALLOONINFLATE);
    }
    else if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
    {
        mApp->PlayFoley(FoleyType::FOLEY_ZAMBONI);
    }
}

//0x530770
void Zombie::StartZombieSound()
{
    if (mPlayingSong)
        return;

    if (mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_RUNNING && mHasHead)
    {
        mApp->PlayFoley(FoleyType::FOLEY_JACKINTHEBOX);
        mPlayingSong = true;
    }
    else if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING)
    {
        mApp->PlayFoley(FoleyType::FOLEY_DIGGER);
        mPlayingSong = true;
    }
}

//0x530850
void Zombie::StopZombieSound()
{
    if (mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
    {
        bool aStopSound = true;

        if (mBoard)
        {
            Zombie* aZombie = nullptr;
            while (mBoard->IterateZombies(aZombie))
            {
                if (aZombie->mHasHead && !aZombie->IsDeadOrDying() && aZombie->IsOnBoard() &&
                    (aZombie->mZombieType == ZombieType::ZOMBIE_DANCER || aZombie->mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER))
                {
                    aStopSound = false;
                    break;
                }
            }
        }

        if (aStopSound)
        {
            mApp->mSoundSystem->StopFoley(FoleyType::FOLEY_DANCER);
        }
    }

    if (mPlayingSong)
    {
        mPlayingSong = false;

        if (mZombieType == ZombieType::ZOMBIE_JACK_IN_THE_BOX)
        {
            mApp->mSoundSystem->StopFoley(FoleyType::FOLEY_JACKINTHEBOX);
        }
        else if (mZombieType == ZombieType::ZOMBIE_DIGGER)
        {
            mApp->mSoundSystem->StopFoley(FoleyType::FOLEY_DIGGER);
        }
    }
}

//0x530950
void Zombie::ApplyChill(std::int64_t ownerNowNs, bool theIsIceTrap)
{
    const auto aStatusNowNs = ownerNowNs;

    if (!CanBeChilled())
        return;

    if (mChilledEndTimeNs <= aStatusNowNs)
    {
        mApp->PlayFoley(FoleyType::FOLEY_FROZEN);
    }

    int aChillTime = 1000;
    if (theIsIceTrap)
    {
        aChillTime = 2000;
    }
    mChilledEndTimeNs = std::max(aStatusNowNs + static_cast<std::int64_t>(aChillTime) * 10'000'000, mChilledEndTimeNs);

    UpdateAnimSpeed(ownerNowNs);
}

//0x530A00
void Zombie::DropShield(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, unsigned int theDamageFlags)
{
    const auto aStatusNowNs = ownerNowNs;

    if (mShieldType == ShieldType::SHIELDTYPE_NONE)
        return;

    //ZombieDrawPosition aDrawPos;
    //GetDrawPos(aDrawPos);
    if (mShieldType == ShieldType::SHIELDTYPE_DOOR)
    {
        DetachShield(ownerNowNs);
        if (!TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_LEAVE_BODY))
        {
            float aPosX, aPosY;
            GetTrackPosition(ReanimTrackId::anim_screendoor, aPosX, aPosY);
            TodParticleSystem* aParticle = mApp->AddTodParticle(aPosX, aPosY, mRenderOrder + 1, ParticleEffect::PARTICLE_ZOMBIE_DOOR, ownerNowNs);
            OverrideParticleScale(aParticle);
        }
    }
    else if (mShieldType == ShieldType::SHIELDTYPE_NEWSPAPER)
    {
        StopEating(ownerNowNs);
        if (mYuckyFace)
        {
            ShowYuckyFace(false);
            mYuckyFace = false;
            mYuckyFaceStartTimeNs = aStatusNowNs;
        }

        mZombiePhase = ZombiePhase::PHASE_NEWSPAPER_MADDENING;
        PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_gasp, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 8.0f);
        DetachShield(ownerNowNs);

        if (!TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_LEAVE_BODY))
        {
            float aPosX, aPosY;
            GetTrackPosition(ReanimTrackId::Zombie_paper_paper, aPosX, aPosY);
            TodParticleSystem* aParticle = mApp->AddTodParticle(aPosX, aPosY, mRenderOrder + 1, ParticleEffect::PARTICLE_ZOMBIE_NEWSPAPER, ownerNowNs);
            OverrideParticleScale(aParticle);
        }

        if (!TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_LEAVE_BODY) && !TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_BYPASSES_SHIELD))
        {
            mApp->PlayFoley(FoleyType::FOLEY_NEWSPAPER_RIP);
            AddAttachedReanim(ownerNowNs, visualNowNs, animationNowNs, -11, 0, ReanimationType::REANIM_ZOMBIE_SURPRISE);
        }
    }
    else if (mShieldType == ShieldType::SHIELDTYPE_LADDER)
    {
        DetachShield(ownerNowNs);
        if (!TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_LEAVE_BODY))
        {
            float aPosX = (mX) + 31.0f;
            float aPosY = (mY) + 80.0f;
            TodParticleSystem* aParticle = mApp->AddTodParticle(aPosX, aPosY, mRenderOrder + 1, ParticleEffect::PARTICLE_ZOMBIE_LADDER, ownerNowNs);
            OverrideParticleScale(aParticle);
        }
    }

    mShieldType = ShieldType::SHIELDTYPE_NONE;
}

//0x530BC0
int Zombie::TakeShieldDamage(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theDamage, unsigned int theDamageFlags)
{
    const auto aStatusNowNs = ownerNowNs;

    if (!TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_CAUSE_FLASH))
    {
        mShieldFlashEndTimeNs = aStatusNowNs + static_cast<std::int64_t>(25) * 10'000'000;
        if (mShotFlashEndTimeNs < aStatusNowNs)
        {
            mShotFlashEndTimeNs = aStatusNowNs;
        }
    }

    if (!TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_CAUSE_FLASH) && !TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_HITS_SHIELD_AND_BODY))
    {
        mShieldRecoilEndTimeNs = aStatusNowNs + static_cast<std::int64_t>(12) * 10'000'000;
        if (mShieldType == ShieldType::SHIELDTYPE_DOOR || mShieldType == ShieldType::SHIELDTYPE_LADDER)
        {
            mApp->PlayFoley(FoleyType::FOLEY_SHIELD_HIT);
        }
    }

    int aDamageIndexBeforeDamage = GetShieldDamageIndex();
    int aDamageActual = std::min(mShieldHealth, theDamage);
    int aDamageRemaining = theDamage - aDamageActual;
    mShieldHealth -= aDamageActual;
    if (mShieldHealth == 0)
    {
        DropShield(ownerNowNs, visualNowNs, animationNowNs, theDamageFlags);
        return aDamageRemaining;
    }

    int aDamageIndexAfterDamage = GetShieldDamageIndex();
    if (aDamageIndexAfterDamage != aDamageIndexBeforeDamage)
    {
        Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
        // B 形态 -> 常量二维表 kShieldDamageImages（原 6 支链合并）：行序 =
        // ShieldType 枚举序 0..3（count_last=SHIELDTYPE_LADDER 锚定，无 NUM
        // 哨兵），列序 = 损伤档 1..2；mTrack=None = 无 case 槽（default 无
        // 操作）。六支原均带 TOD_ASSERT(aBodyReanim)，统一保留一处。
        static constexpr struct
        {
            ReanimTrackId mTrack;
            std::uint32_t mOption;
        } kShieldDamageImages[(int)ShieldType::SHIELDTYPE_LADDER + 1][2] = {
            /* [0] SHIELDTYPE_NONE      */ { { ReanimTrackId::None, 0 }, { ReanimTrackId::None, 0 } },
            /* [1] SHIELDTYPE_DOOR  dmg1 */ { { ReanimTrackId::anim_screendoor, ReanimOptions::Zombie::anim_screendoor::ZOMBIE_SCREENDOOR2 },
                                              /*                     dmg2 */   { ReanimTrackId::anim_screendoor, ReanimOptions::Zombie::anim_screendoor::ZOMBIE_SCREENDOOR3 } },
            /* [2] SHIELDTYPE_NEWSPAPER  */ { { ReanimTrackId::Zombie_paper_paper, ReanimOptions::Zombie_paper::Zombie_paper_paper::ZOMBIE_PAPER_PAPER2 },
                                              /*                     dmg2 */   { ReanimTrackId::Zombie_paper_paper, ReanimOptions::Zombie_paper::Zombie_paper_paper::ZOMBIE_PAPER_PAPER3 } },
            /* [3] SHIELDTYPE_LADDER     */ { { ReanimTrackId::Zombie_ladder_1, ReanimOptions::Zombie_ladder::Zombie_ladder_1::ZOMBIE_LADDER_1_DAMAGE1 },
                                              /*                     dmg2 */   { ReanimTrackId::Zombie_ladder_1, ReanimOptions::Zombie_ladder::Zombie_ladder_1::ZOMBIE_LADDER_1_DAMAGE2 } },
        };
        static_assert(sizeof(kShieldDamageImages) / sizeof(kShieldDamageImages[0]) ==
            (int)ShieldType::SHIELDTYPE_LADDER + 1, "kShieldDamageImages 槽位须与 ShieldType 枚举尾同步");

        const unsigned aShieldIndex = static_cast<unsigned>(mShieldType);
        const unsigned aDamageSlot = static_cast<unsigned>(aDamageIndexAfterDamage) - 1;
        if (aShieldIndex < sizeof(kShieldDamageImages) / sizeof(kShieldDamageImages[0]) &&
            aDamageSlot < 2 && kShieldDamageImages[aShieldIndex][aDamageSlot].mTrack != ReanimTrackId::None)
        {
            TOD_ASSERT(aBodyReanim);
            aBodyReanim->SetVariantOption(kShieldDamageImages[aShieldIndex][aDamageSlot].mTrack,
                kShieldDamageImages[aShieldIndex][aDamageSlot].mOption);
        }
    }

    return aDamageRemaining;
}

namespace
{
// Zombie::DropHelm 掉落头盔分发表上下文：坐标（aPosX/aPosY）由槽内
// GetTrackPosition 原地更新，粒子特效经引用回写（表后统一发射）。
using DropHelmCaseFn = void (*)(std::int64_t ownerNowNs, Zombie&, float&, float&, unsigned int, ParticleEffect&);

// 共体（TRAFFIC_CONE/PAIL/FOOTBALL 三槽复用，仅轨名/区间/特效三常量差异）：
// 取饰件轨位置、藏饰件区间、亮出头发、置掉落粒子特效。
void DropHelmTakeProp(Zombie& z, float& thePosX, float& thePosY, ParticleEffect& theEffect,
    ReanimTrackId theTrackId, const ReanimTrackIdRange& theTrackRange, ParticleEffect theParticleEffect)
{
    z.GetTrackPosition(theTrackId, thePosX, thePosY);
    z.ReanimShowRange(theTrackRange, RENDER_GROUP_HIDDEN);
    z.ReanimShowRange(ReanimTrackRanges::anim_hair, RENDER_GROUP_NORMAL);
    theEffect = theParticleEffect;
}

// C 形态 -> 函数指针表 kDropHelmCases（原 5 支链合并）：行序 = HelmType
// 枚举序 0..9（尾值 HELMTYPE_TALLNUT 锚定，无 NUM 哨兵）；nullptr = 无 case
// 槽（原链不匹配 = 无操作，aEffect 保持 PARTICLE_NONE）。DIGGER 槽：单轨
// ReanimShowTrack 藏（非 Range）且不亮头发，原支体差异保留；BOBSLED 槽：
// BobsledCrash 带 DAMAGE_DOESNT_LEAVE_BODY 守卫（守卫随支体入槽）。
constexpr DropHelmCaseFn kDropHelmCases[(int)HelmType::HELMTYPE_TALLNUT + 1] = {
    nullptr,  // 0 HELMTYPE_NONE（函数头早退已拦截，占位对齐）
    [](std::int64_t , Zombie& z, float& x, float& y, unsigned int, ParticleEffect& e)
    {
        DropHelmTakeProp(z, x, y, e, ReanimTrackId::anim_cone, ReanimTrackRanges::anim_cone, ParticleEffect::PARTICLE_ZOMBIE_TRAFFIC_CONE);
    },  // 1 HELMTYPE_TRAFFIC_CONE
    [](std::int64_t , Zombie& z, float& x, float& y, unsigned int, ParticleEffect& e)
    {
        DropHelmTakeProp(z, x, y, e, ReanimTrackId::anim_bucket, ReanimTrackRanges::anim_bucket, ParticleEffect::PARTICLE_ZOMBIE_PAIL);
    },  // 2 HELMTYPE_PAIL
    [](std::int64_t , Zombie& z, float& x, float& y, unsigned int, ParticleEffect& e)
    {
        DropHelmTakeProp(z, x, y, e, ReanimTrackId::zombie_football_helmet, ReanimTrackRanges::zombie_football_helmet, ParticleEffect::PARTICLE_ZOMBIE_HELMET);
    },  // 3 HELMTYPE_FOOTBALL
    [](std::int64_t , Zombie& z, float& x, float& y, unsigned int, ParticleEffect& e)
    {
        z.GetTrackPosition(ReanimTrackId::Zombie_digger_hardhat, x, y);
        z.ReanimShowTrack(ReanimTrackId::Zombie_digger_hardhat, RENDER_GROUP_HIDDEN);
        e = ParticleEffect::PARTICLE_ZOMBIE_HEADLIGHT;
    },  // 4 HELMTYPE_DIGGER
    nullptr,  // 5 HELMTYPE_REDEYES（无 case）
    nullptr,  // 6 HELMTYPE_HEADBAND（无 case）
    [](std::int64_t ownerNowNs, Zombie& z, float&, float&, unsigned int theDamageFlags, ParticleEffect&)
    {
        if (!TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_LEAVE_BODY))
        {
            z.BobsledCrash(ownerNowNs);
        }
    },  // 7 HELMTYPE_BOBSLED
    nullptr,  // 8 HELMTYPE_WALLNUT（无 case）
    nullptr,  // 9 HELMTYPE_TALLNUT（无 case）
};
static_assert(sizeof(kDropHelmCases) / sizeof(kDropHelmCases[0]) ==
    (int)HelmType::HELMTYPE_TALLNUT + 1, "kDropHelmCases 槽位须与 HelmType 枚举尾同步");
static_assert(kDropHelmCases[(int)HelmType::HELMTYPE_TRAFFIC_CONE] != nullptr &&
    kDropHelmCases[(int)HelmType::HELMTYPE_PAIL] != nullptr &&
    kDropHelmCases[(int)HelmType::HELMTYPE_FOOTBALL] != nullptr &&
    kDropHelmCases[(int)HelmType::HELMTYPE_DIGGER] != nullptr &&
    kDropHelmCases[(int)HelmType::HELMTYPE_BOBSLED] != nullptr &&
    kDropHelmCases[(int)HelmType::HELMTYPE_NONE] == nullptr &&
    kDropHelmCases[(int)HelmType::HELMTYPE_REDEYES] == nullptr &&
    kDropHelmCases[(int)HelmType::HELMTYPE_HEADBAND] == nullptr &&
    kDropHelmCases[(int)HelmType::HELMTYPE_WALLNUT] == nullptr &&
    kDropHelmCases[(int)HelmType::HELMTYPE_TALLNUT] == nullptr,
    "kDropHelmCases 活槽/空槽锚点错位");
}  // namespace

//0x530E30
void Zombie::DropHelm(std::int64_t ownerNowNs, unsigned int theDamageFlags)
{
    if (mHelmType == HelmType::HELMTYPE_NONE)
        return;

    ZombieDrawPosition aDrawPos;
    GetDrawPos(aDrawPos);
    float aPosX = (mX) + aDrawPos.mImageOffsetX + aDrawPos.mHeadX + 14.0f;
    float aPosY = (mY) + aDrawPos.mImageOffsetY + aDrawPos.mHeadY + aDrawPos.mBodyY + 18.0f;
    ParticleEffect aEffect = ParticleEffect::PARTICLE_NONE;
    const unsigned aHelmIndex = static_cast<unsigned>(mHelmType);
    if (aHelmIndex < sizeof(kDropHelmCases) / sizeof(kDropHelmCases[0]) &&
        kDropHelmCases[aHelmIndex] != nullptr)
    {
        kDropHelmCases[aHelmIndex](ownerNowNs, *this, aPosX, aPosY, theDamageFlags, aEffect);
    }

    if (!TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_LEAVE_BODY) && aEffect != ParticleEffect::PARTICLE_NONE)
    {
        TodParticleSystem* aParticle = mApp->AddTodParticle(aPosX, aPosY, mRenderOrder + 1, aEffect, ownerNowNs);
        OverrideParticleScale(aParticle);
    }

    mHelmType = HelmType::HELMTYPE_NONE;
}

//0x530FE0
int Zombie::TakeHelmDamage(std::int64_t ownerNowNs, int theDamage, unsigned int theDamageFlags)
{
    const auto aStatusNowNs = ownerNowNs;

    if (!TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_CAUSE_FLASH))
    {
        mShotFlashEndTimeNs = aStatusNowNs + static_cast<std::int64_t>(25) * 10'000'000;
    }

    int aDamageIndexBeforeDamage = GetHelmDamageIndex();
    int aDamageActual = std::min(mHelmHealth, theDamage);
    int aDamageRemaining = theDamage - aDamageActual;
    mHelmHealth -= aDamageActual;
    if (TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_FREEZE))
    {
        ApplyChill(ownerNowNs, false);
    }
    if (mHelmHealth == 0)
    {
        DropHelm(ownerNowNs, theDamageFlags);
        return aDamageRemaining;
    }

    int aDamageIndexAfterDamage = GetHelmDamageIndex();
    if (aDamageIndexBeforeDamage != aDamageIndexAfterDamage)
    {
        Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
        // B 形态 -> 常量二维表 kHelmDamageImages（原 12 支链合并）：行序 =
        // HelmType 枚举序 0..9（count_last=HELMTYPE_TALLNUT 锚定，无 NUM 哨兵），
        // 列序 = 损伤档 1..2；mTrack=None = 无 case 槽（与有效轨道的默认选项0分开）。
        // WALLNUT/TALLNUT 四支目标为 mSpecialHeadReanimID
        // 头部 reanim（mUseHeadReanim 位）；TRAFFIC_CONE 两支原条件带
        // `&& aBodyReanim` 判空（mGuardNullBody 位，空时跳过 = 原落空）；
        // 其余支原 TOD_ASSERT 仅 debug 生效，统一保留一处。
        static constexpr struct
        {
            ReanimTrackId mTrack;
            std::uint32_t mOption;
            bool mUseHeadReanim;       // WALLNUT/TALLNUT：作用于头部 reanim
            bool mGuardNullBody;       // TRAFFIC_CONE：原链带 && aBodyReanim 判空
        } kHelmDamageImages[(int)HelmType::HELMTYPE_TALLNUT + 1][2] = {
            /* [0] HELMTYPE_NONE       */ { { ReanimTrackId::None, 0, false, false }, { ReanimTrackId::None, 0, false, false } },
            /* [1] HELMTYPE_TRAFFIC_CONE */ { { ReanimTrackId::anim_cone, ReanimOptions::Zombie::anim_cone::ZOMBIE_CONE2, false, true },
                                              { ReanimTrackId::anim_cone, ReanimOptions::Zombie::anim_cone::ZOMBIE_CONE3, false, true } },
            /* [2] HELMTYPE_PAIL       */ { { ReanimTrackId::anim_bucket, ReanimOptions::Zombie::anim_bucket::ZOMBIE_BUCKET2, false, false },
                                              { ReanimTrackId::anim_bucket, ReanimOptions::Zombie::anim_bucket::ZOMBIE_BUCKET3, false, false } },
            /* [3] HELMTYPE_FOOTBALL   */ { { ReanimTrackId::zombie_football_helmet, ReanimOptions::Zombie_football::zombie_football_helmet::ZOMBIE_FOOTBALL_HELMET2, false, false },
                                              { ReanimTrackId::zombie_football_helmet, ReanimOptions::Zombie_football::zombie_football_helmet::ZOMBIE_FOOTBALL_HELMET3, false, false } },
            /* [4] HELMTYPE_DIGGER     */ { { ReanimTrackId::Zombie_digger_hardhat, ReanimOptions::Zombie_digger::Zombie_digger_hardhat::ZOMBIE_DIGGER_HARDHAT2, false, false },
                                              { ReanimTrackId::Zombie_digger_hardhat, ReanimOptions::Zombie_digger::Zombie_digger_hardhat::ZOMBIE_DIGGER_HARDHAT3, false, false } },
            /* [5] HELMTYPE_REDEYES    */ { { ReanimTrackId::None, 0, false, false }, { ReanimTrackId::None, 0, false, false } },
            /* [6] HELMTYPE_HEADBAND   */ { { ReanimTrackId::None, 0, false, false }, { ReanimTrackId::None, 0, false, false } },
            /* [7] HELMTYPE_BOBSLED    */ { { ReanimTrackId::None, 0, false, false }, { ReanimTrackId::None, 0, false, false } },
            /* [8] HELMTYPE_WALLNUT    */ { { ReanimTrackId::anim_face, ReanimOptions::Wallnut::anim_face::WALLNUT_CRACKED1, true, false },
                                              { ReanimTrackId::anim_face, ReanimOptions::Wallnut::anim_face::WALLNUT_CRACKED2, true, false } },
            /* [9] HELMTYPE_TALLNUT    */ { { ReanimTrackId::anim_idle, ReanimOptions::Tallnut::anim_idle::TALLNUT_CRACKED1, true, false },
                                              { ReanimTrackId::anim_idle, ReanimOptions::Tallnut::anim_idle::TALLNUT_CRACKED2, true, false } },
        };
        static_assert(sizeof(kHelmDamageImages) / sizeof(kHelmDamageImages[0]) ==
            (int)HelmType::HELMTYPE_TALLNUT + 1, "kHelmDamageImages 槽位须与 HelmType 枚举尾同步");

        const unsigned aHelmIndex = static_cast<unsigned>(mHelmType);
        const unsigned aDamageSlot = static_cast<unsigned>(aDamageIndexAfterDamage) - 1;
        if (aHelmIndex < sizeof(kHelmDamageImages) / sizeof(kHelmDamageImages[0]) &&
            aDamageSlot < 2 && kHelmDamageImages[aHelmIndex][aDamageSlot].mTrack != ReanimTrackId::None &&
            (!kHelmDamageImages[aHelmIndex][aDamageSlot].mGuardNullBody || aBodyReanim != nullptr))
        {
            if (kHelmDamageImages[aHelmIndex][aDamageSlot].mUseHeadReanim)
            {
                Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
                aHeadReanim->SetVariantOption(kHelmDamageImages[aHelmIndex][aDamageSlot].mTrack,
                    kHelmDamageImages[aHelmIndex][aDamageSlot].mOption);
            }
            else
            {
                TOD_ASSERT(aBodyReanim);
                aBodyReanim->SetVariantOption(kHelmDamageImages[aHelmIndex][aDamageSlot].mTrack,
                    kHelmDamageImages[aHelmIndex][aDamageSlot].mOption);
            }
        }
    }
    return aDamageRemaining;
}

int Zombie::TakeFlyingDamage(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theDamage, unsigned int theDamageFlags)
{
    const auto aStatusNowNs = ownerNowNs;

    if (!TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_CAUSE_FLASH))
    {
        mShotFlashEndTimeNs = aStatusNowNs + static_cast<std::int64_t>(25) * 10'000'000;
    }

    int aDamageActual = std::min(mFlyingHealth, theDamage);
    int aDamageRemaining = theDamage - aDamageActual;
    mFlyingHealth -= aDamageActual;
    if (mFlyingHealth == 0)
    {
        LandFlyer(ownerNowNs, visualNowNs, animationNowNs, theDamageFlags);
    }

    return aDamageRemaining;
}

//0x5312D0
void Zombie::TakeBodyDamage(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theDamage, unsigned int theDamageFlags)
{
    const auto aStatusNowNs = ownerNowNs;

    if (!TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_CAUSE_FLASH))
    {
        mShotFlashEndTimeNs = aStatusNowNs + static_cast<std::int64_t>(25) * 10'000'000;
    }

    if (TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_FREEZE))
    {
        ApplyChill(ownerNowNs, false);
    }

    int aBodyHealthOrigin = mBodyHealth;
    int aDamageIndexBeforeDamage = GetBodyDamageIndex();
    mBodyHealth -= theDamage;
    int aDamageIndexAfterDamage = GetBodyDamageIndex();
    // C 形态 -> 匿名 lambda 函数指针表 kBodyDamageCases（仿 kIceTrapAdjusts
    // 范式）：行序 = ZombieType 枚举序 0..33；原 4 支异构大体积支整体入槽，
    // 无 case 槽 = nullptr（落空走原 else 支 UpdateDamageStates）。
    // GARGANTUAR/REDEYE 共体（constexpr 指针变量复用，内含 REDEYE 头像分叉）。
    // 四个链前局部经 BodyDamageLocals 传递。
    struct BodyDamageLocals
    {
        unsigned int mDamageFlags;
        int mBodyHealthOrigin;
        int mIndexBeforeDamage;
        int mIndexAfterDamage;
    };
    using BodyDamageCaseFn = void (*)(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie&, const BodyDamageLocals&);
    static constexpr BodyDamageCaseFn kBodyDamageCaseGargantuar = [](std::int64_t , std::int64_t , std::int64_t , Zombie& z, const BodyDamageLocals& lc)
    {
        Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
        if (lc.mIndexBeforeDamage != lc.mIndexAfterDamage)
        {
            if (lc.mIndexAfterDamage == 1)
            {
                aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_gargantua_body1, ReanimOptions::Zombie_gargantuar::Zombie_gargantua_body1::ZOMBIE_GARGANTUAR_BODY1_2);
                aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_gargantuar_outerarm_lower, ReanimOptions::Zombie_gargantuar::Zombie_gargantuar_outerarm_lower::ZOMBIE_GARGANTUAR_OUTERARM_LOWER2);
            }
            else if (lc.mIndexAfterDamage == 2)
            {
                aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_gargantua_body1, ReanimOptions::Zombie_gargantuar::Zombie_gargantua_body1::ZOMBIE_GARGANTUAR_BODY1_3);
                aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_gargantuar_outerleg_foot, ReanimOptions::Zombie_gargantuar::Zombie_gargantuar_outerleg_foot::ZOMBIE_GARGANTUAR_FOOT2);
                aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_gargantuar_outerarm_lower, ReanimOptions::Zombie_gargantuar::Zombie_gargantuar_outerarm_lower::ZOMBIE_GARGANTUAR_OUTERARM_LOWER2);
                if (z.mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
                {
                    aBodyReanim->SetVariantOption(ReanimTrackId::anim_head1, ReanimOptions::Zombie_gargantuar::anim_head1::ZOMBIE_GARGANTUAR_HEAD2_REDEYE);
                }
                else
                {
                    aBodyReanim->SetVariantOption(ReanimTrackId::anim_head1, ReanimOptions::Zombie_gargantuar::anim_head1::ZOMBIE_GARGANTUAR_HEAD2);
                }
            }
        }
    };
    static constexpr BodyDamageCaseFn kBodyDamageCases[(int)ZombieType::NUM_ZOMBIE_TYPES] = {
        nullptr,  // 0 ZOMBIE_NORMAL（原 default：UpdateDamageStates）
        nullptr,  // 1 ZOMBIE_FLAG（原 default）
        nullptr,  // 2 ZOMBIE_TRAFFIC_CONE（原 default）
        nullptr,  // 3 ZOMBIE_POLEVAULTER（原 default）
        nullptr,  // 4 ZOMBIE_PAIL（原 default）
        nullptr,  // 5 ZOMBIE_NEWSPAPER（原 default）
        nullptr,  // 6 ZOMBIE_DOOR（原 default）
        nullptr,  // 7 ZOMBIE_FOOTBALL（原 default）
        nullptr,  // 8 ZOMBIE_DANCER（原 default）
        nullptr,  // 9 ZOMBIE_BACKUP_DANCER（原 default）
        nullptr,  // 10 ZOMBIE_DUCKY_TUBE（原 default）
        nullptr,  // 11 ZOMBIE_SNORKEL（原 default）
        // 原 if ZOMBIE_ZAMBONI
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, const BodyDamageLocals& lc)
        {
            Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
            if (!TestBit(lc.mDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_CAUSE_FLASH))
            {
                z.mApp->PlayFoley(FoleyType::FOLEY_SHIELD_HIT);
            }

            if (TestBit(lc.mDamageFlags, (int)DamageFlags::DAMAGE_SPIKE))
            {
                aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_zamboni_1, ReanimOptions::Zombie_zamboni::Zombie_zamboni_1::ZOMBIE_ZAMBONI_1_DAMAGE2);
                aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_zamboni_2, ReanimOptions::Zombie_zamboni::Zombie_zamboni_2::ZOMBIE_ZAMBONI_2_DAMAGE2);
                z.ZamboniDeath(ownerNowNs, visualNowNs, animationNowNs, lc.mDamageFlags);
            }
            else if (z.mBodyHealth <= 0)
            {
                z.ZamboniDeath(ownerNowNs, visualNowNs, animationNowNs, lc.mDamageFlags);
            }
            else if (lc.mIndexBeforeDamage != lc.mIndexAfterDamage)
            {
                if (lc.mIndexAfterDamage == 1)
                {
                    aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_zamboni_1, ReanimOptions::Zombie_zamboni::Zombie_zamboni_1::ZOMBIE_ZAMBONI_1_DAMAGE1);
                    aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_zamboni_2, ReanimOptions::Zombie_zamboni::Zombie_zamboni_2::ZOMBIE_ZAMBONI_2_DAMAGE1);
                }
                else if (lc.mIndexAfterDamage == 2)
                {
                    aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_zamboni_1, ReanimOptions::Zombie_zamboni::Zombie_zamboni_1::ZOMBIE_ZAMBONI_1_DAMAGE2);
                    aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_zamboni_2, ReanimOptions::Zombie_zamboni::Zombie_zamboni_2::ZOMBIE_ZAMBONI_2_DAMAGE2);
                    z.AddAttachedParticle(ownerNowNs, 27, 72, ParticleEffect::PARTICLE_ZAMBONI_SMOKE);
                }
            }
        },  // 12 ZOMBIE_ZAMBONI
        nullptr,  // 13 ZOMBIE_BOBSLED（原 default）
        nullptr,  // 14 ZOMBIE_DOLPHIN_RIDER（原 default）
        nullptr,  // 15 ZOMBIE_JACK_IN_THE_BOX（原 default）
        nullptr,  // 16 ZOMBIE_BALLOON（原 default）
        nullptr,  // 17 ZOMBIE_DIGGER（原 default）
        nullptr,  // 18 ZOMBIE_POGO（原 default）
        nullptr,  // 19 ZOMBIE_YETI（原 default）
        nullptr,  // 20 ZOMBIE_BUNGEE（原 default）
        nullptr,  // 21 ZOMBIE_LADDER（原 default）
        // 原 else-if ZOMBIE_CATAPULT
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, const BodyDamageLocals& lc)
        {
            Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
            if (TestBit(lc.mDamageFlags, (int)DamageFlags::DAMAGE_SPIKE) || z.mBodyHealth <= 0)
            {
                aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_catapult_siding, ReanimOptions::Zombie_catapult::Zombie_catapult_siding::ZOMBIE_CATAPULT_SIDING_DAMAGE);
                z.CatapultDeath(ownerNowNs, visualNowNs, animationNowNs, lc.mDamageFlags);
            }
            else if (lc.mIndexBeforeDamage != lc.mIndexAfterDamage)
            {
                if (lc.mIndexAfterDamage == 1)
                {
                    aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_catapult_siding, ReanimOptions::Zombie_catapult::Zombie_catapult_siding::ZOMBIE_CATAPULT_SIDING_DAMAGE);
                }
                else if (lc.mIndexAfterDamage == 2)
                {
                    z.AddAttachedParticle(ownerNowNs, 47, 77, ParticleEffect::PARTICLE_ZAMBONI_SMOKE);
                }
            }
        },  // 22 ZOMBIE_CATAPULT
        kBodyDamageCaseGargantuar,  // 23 ZOMBIE_GARGANTUAR（共体）
        nullptr,  // 24 ZOMBIE_IMP（原 default）
        // 原 else-if ZOMBIE_BOSS
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z, const BodyDamageLocals& lc)
        {
            if (!TestBit(lc.mDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_CAUSE_FLASH))
            {
                z.mApp->PlayFoley(FoleyType::FOLEY_SHIELD_HIT);
            }

            Reanimation* aBodyReanim = z.mApp->ReanimationGet(z.mBodyReanimID);
            if (lc.mIndexBeforeDamage != lc.mIndexAfterDamage)
            {
                if (lc.mIndexAfterDamage == 1)
                {
                    aBodyReanim->SetVariantOption(ReanimTrackId::Boss_head, ReanimOptions::Zombie_boss::Boss_head::ZOMBIE_BOSS_HEAD_DAMAGE1);
                    aBodyReanim->SetVariantOption(ReanimTrackId::Boss_jaw, ReanimOptions::Zombie_boss::Boss_jaw::ZOMBIE_BOSS_JAW_DAMAGE1);
                    aBodyReanim->SetVariantOption(ReanimTrackId::Boss_outerarm_hand, ReanimOptions::Zombie_boss::Boss_outerarm_hand::ZOMBIE_BOSS_OUTERARM_HAND_DAMAGE1);
                    aBodyReanim->SetVariantOption(ReanimTrackId::Boss_outerarm_thumb2, ReanimOptions::Zombie_boss::Boss_outerarm_thumb2::ZOMBIE_BOSS_OUTERARM_THUMB_DAMAGE1);
                    aBodyReanim->SetVariantOption(ReanimTrackId::Boss_innerleg_foot, ReanimOptions::Zombie_boss::Boss_innerleg_foot::ZOMBIE_BOSS_FOOT_DAMAGE1);
                }
                else if (lc.mIndexAfterDamage == 2)
                {
                    aBodyReanim->SetVariantOption(ReanimTrackId::Boss_head, ReanimOptions::Zombie_boss::Boss_head::ZOMBIE_BOSS_HEAD_DAMAGE2);
                    aBodyReanim->SetVariantOption(ReanimTrackId::Boss_jaw, ReanimOptions::Zombie_boss::Boss_jaw::ZOMBIE_BOSS_JAW_DAMAGE2);
                    aBodyReanim->SetVariantOption(ReanimTrackId::Boss_outerarm_hand, ReanimOptions::Zombie_boss::Boss_outerarm_hand::ZOMBIE_BOSS_OUTERARM_HAND_DAMAGE2);
                    aBodyReanim->SetVariantOption(ReanimTrackId::Boss_outerarm_thumb2, ReanimOptions::Zombie_boss::Boss_outerarm_thumb2::ZOMBIE_BOSS_OUTERARM_THUMB_DAMAGE2);
                    aBodyReanim->SetVariantOption(ReanimTrackId::Boss_outerleg_foot, ReanimOptions::Zombie_boss::Boss_outerleg_foot::ZOMBIE_BOSS_FOOT_DAMAGE2);
                    z.ApplyBossSmokeParticles(ownerNowNs, true);
                }
            }

            if (lc.mBodyHealthOrigin >= z.mBodyMaxHealth / BOSS_FLASH_HEALTH_FRACTION && z.mBodyHealth < z.mBodyMaxHealth / BOSS_FLASH_HEALTH_FRACTION)
            {
                z.mApp->AddTodParticle(770.0f, 260.0f, Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 0), ParticleEffect::PARTICLE_BOSS_EXPLOSION, ownerNowNs);
                z.mApp->PlayFoley(FoleyType::FOLEY_BOSS_EXPLOSION_SMALL);
                z.ApplyBossSmokeParticles(ownerNowNs, true);
            }

            if (z.mBodyHealth <= 0)
            {
                z.mBodyHealth = 1;
            }
        },  // 25 ZOMBIE_BOSS
        nullptr,  // 26 ZOMBIE_PEA_HEAD（原 default）
        nullptr,  // 27 ZOMBIE_WALLNUT_HEAD（原 default）
        nullptr,  // 28 ZOMBIE_JALAPENO_HEAD（原 default）
        nullptr,  // 29 ZOMBIE_GATLING_HEAD（原 default）
        nullptr,  // 30 ZOMBIE_SQUASH_HEAD（原 default）
        nullptr,  // 31 ZOMBIE_TALLNUT_HEAD（原 default）
        kBodyDamageCaseGargantuar,  // 32 ZOMBIE_REDEYE_GARGANTUAR（共体）
        nullptr,  // 33 ZOMBIE_ZOMBATAR（原 default）
    };
    static_assert(sizeof(kBodyDamageCases) / sizeof(kBodyDamageCases[0]) ==
        (int)ZombieType::NUM_ZOMBIE_TYPES, "kBodyDamageCases 槽位须与 ZombieType 枚举同步");

    const BodyDamageLocals aLocals{theDamageFlags, aBodyHealthOrigin, aDamageIndexBeforeDamage, aDamageIndexAfterDamage};
    const unsigned aTypeIndex = static_cast<unsigned>(mZombieType);
    const BodyDamageCaseFn aCase =
        aTypeIndex < sizeof(kBodyDamageCases) / sizeof(kBodyDamageCases[0]) ? kBodyDamageCases[aTypeIndex] : nullptr;
    if (aCase != nullptr)
    {
        aCase(ownerNowNs, visualNowNs, animationNowNs, *this, aLocals);
    }
    else
    {
        UpdateDamageStates(ownerNowNs, visualNowNs, animationNowNs, theDamageFlags);
    }

    if (mBodyHealth <= 0)
    {
        mBodyHealth = 0;
        PlayDeathAnim(ownerNowNs, visualNowNs, animationNowNs, theDamageFlags);
        DropLoot();
    }
}

//0x5317C0
void Zombie::TakeDamage(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theDamage, unsigned int theDamageFlags)
{
    if (mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_POPPING || IsDeadOrDying())
        return;

    int aDamageRemaining = theDamage;

    if (IsFlying())
    {
        aDamageRemaining = TakeFlyingDamage(ownerNowNs, visualNowNs, animationNowNs, aDamageRemaining, theDamageFlags);
    }
    if (aDamageRemaining > 0 && mShieldType != ShieldType::SHIELDTYPE_NONE && !TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_BYPASSES_SHIELD))
    {
        aDamageRemaining = TakeShieldDamage(ownerNowNs, visualNowNs, animationNowNs, aDamageRemaining, theDamageFlags);
        if (TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_HITS_SHIELD_AND_BODY))
        {
            aDamageRemaining = theDamage;
        }
    }
    if (aDamageRemaining > 0 && mHelmType != HelmType::HELMTYPE_NONE)
    {
        aDamageRemaining = TakeHelmDamage(ownerNowNs, aDamageRemaining, theDamageFlags);
    }
    if (aDamageRemaining > 0)
    {
        TakeBodyDamage(ownerNowNs, visualNowNs, animationNowNs, aDamageRemaining, theDamageFlags);
    }
}

//0x531880
float Zombie::GetPosYBasedOnRow(int theRow)
{
    if (!IsOnBoard())
        return 0.0f;

    if (IsOnHighGround())
    {
        if (mAltitude < HIGH_GROUND_HEIGHT)
        {
            mZombieHeight = ZombieHeight::HEIGHT_UP_TO_HIGH_GROUND;
        }
        mOnHighGround = true;
    }

    float aPosY = mBoard->GetPosYBasedOnRow((mX) + 40.0f, theRow) - 30.0f;
    if (mZombieType == ZombieType::ZOMBIE_BALLOON)
    {
        aPosY -= 30.0f;
    }
    else if (mZombieType == ZombieType::ZOMBIE_POGO)
    {
        aPosY -= 16.0f;
    }

    return aPosY;
}

//0x531930
Zombie::~Zombie()
{
    AttachmentDie(mAttachmentID);
    StopZombieSound();
}

//0x531990
bool Zombie::CanBeChilled()
{
    if (mZombieType == ZombieType::ZOMBIE_ZAMBONI || IsBobsledTeamWithSled())
        return false;

    if (IsDeadOrDying())
        return false;

    if (mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE ||
        mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_RISING)
        return false;

    if (mMindControlled)
        return false;

    return
        mZombieType != ZombieType::ZOMBIE_BOSS ||
        mZombiePhase == ZombiePhase::PHASE_BOSS_HEAD_IDLE_BEFORE_SPIT ||
        mZombiePhase == ZombiePhase::PHASE_BOSS_HEAD_IDLE_AFTER_SPIT ||
        mZombiePhase == ZombiePhase::PHASE_BOSS_HEAD_SPIT;
}

//0x531A10
bool Zombie::CanBeFrozen()
{
    if (!CanBeChilled())
        return false;

    if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_INTO_POOL ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP ||
        mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL ||
        IsFlying() ||
        mZombiePhase == ZombiePhase::PHASE_IMP_GETTING_THROWN ||
        mZombiePhase == ZombiePhase::PHASE_IMP_LANDING ||
        mZombiePhase == ZombiePhase::PHASE_BOBSLED_CRASHING ||
        mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_POPPING ||
        mZombiePhase == ZombiePhase::PHASE_SQUASH_RISING ||
        mZombiePhase == ZombiePhase::PHASE_SQUASH_FALLING ||
        mZombiePhase == ZombiePhase::PHASE_SQUASH_DONE_FALLING ||
        IsBouncingPogo())
        return false;

    return mZombieType != ZombieType::ZOMBIE_BUNGEE || mZombiePhase == ZombiePhase::PHASE_BUNGEE_AT_BOTTOM;
}

//0x531A80
bool Zombie::EffectedByDamage(unsigned int theDamageRangeFlags)
{
    if (!TestBit(theDamageRangeFlags, (int)DamageRangeFlags::DAMAGES_DYING) && IsDeadOrDying())
    {
        return false;
    }

    if (TestBit(theDamageRangeFlags, (int)DamageRangeFlags::DAMAGES_ONLY_MINDCONTROLLED))
    {
        if (!mMindControlled)
        {
            return false;
        }
    }
    else if (mMindControlled)
    {
        return false;
    }

    if (mZombieType == ZombieType::ZOMBIE_BUNGEE && mZombiePhase != ZombiePhase::PHASE_BUNGEE_AT_BOTTOM && mZombiePhase != ZombiePhase::PHASE_BUNGEE_GRABBING)
    {
        return false;  // 蹦极僵尸只有在停留时才会受到攻击
    }

    if (mZombieHeight == ZombieHeight::HEIGHT_GETTING_BUNGEE_DROPPED)
    {
        return false;  // 被空投的过程中不会受到攻击
    }

    if (((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0))
    {
        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        if (mZombiePhase == ZombiePhase::PHASE_BOSS_HEAD_ENTER && (static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs) < 0.5f)
        {
            return false;
        }
        if (mZombiePhase == ZombiePhase::PHASE_BOSS_HEAD_LEAVE && (static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs) > 0.5f)
        {
            return false;
        }

        if (mZombiePhase != ZombiePhase::PHASE_BOSS_HEAD_IDLE_BEFORE_SPIT &&
            mZombiePhase != ZombiePhase::PHASE_BOSS_HEAD_IDLE_AFTER_SPIT &&
            mZombiePhase != ZombiePhase::PHASE_BOSS_HEAD_SPIT)
        {
            return false;  // 僵王博士只有在低头状态下才会受到攻击
        }
    }

    if (mZombieType == ZombieType::ZOMBIE_BOBSLED && GetBobsledPosition() > 0)
    {
        return false;  // 存在雪橇时，只有领头僵尸会受到攻击
    }

    if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT ||
        mZombiePhase == ZombiePhase::PHASE_IMP_GETTING_THROWN ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_INTO_POOL ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP ||
        mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL ||
        mZombiePhase == ZombiePhase::PHASE_BALLOON_POPPING ||
        mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE ||
        mZombiePhase == ZombiePhase::PHASE_BOBSLED_CRASHING ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_RISING)
    {
        return TestBit(theDamageRangeFlags, (int)DamageRangeFlags::DAMAGES_OFF_GROUND);
    }

    if (mZombieType != ZombieType::ZOMBIE_BOBSLED && GetZombieRect().mX > WIDE_BOARD_WIDTH)
    {
        return false;  // 除雪橇僵尸小队外，场外的僵尸不会受到攻击
    }

    bool submerged = mZombieType == ZombieType::ZOMBIE_SNORKEL && mInPool && !mIsEating;
    if (TestBit(theDamageRangeFlags, (int)DamageRangeFlags::DAMAGES_SUBMERGED) && submerged)
    {
        return true;
    }

    bool underground = mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING;
    if (TestBit(theDamageRangeFlags, (int)DamageRangeFlags::DAMAGES_UNDERGROUND) && underground)
    {
        return true;
    }

    if (TestBit(theDamageRangeFlags, (int)DamageRangeFlags::DAMAGES_FLYING) && IsFlying())
    {
        return true;
    }

    return TestBit(theDamageRangeFlags, (int)DamageRangeFlags::DAMAGES_GROUND) && !IsFlying() && !submerged && !underground;
}

void Zombie::SetRow(int theRow)
{
    TOD_ASSERT(theRow >= 0 && theRow <= MAX_GRID_SIZE_Y);

    // 行段桶换行钩子（2026-10-04）：SetRow 是唯一的行变更漏斗（大蒜/
    // 传送门/坟场升起/蹦极投放全走此）；未注册时内部按首次入场处理。
    // mBoard 判空守卫：烟测夹具有栈上僵尸直调本函数（1073 原体不触板）
    const int anOldRow = mRow;
    mRow = theRow;
    mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_ZOMBIE, mRow, 4);
    if (mBoard != nullptr)
    {
        mBoard->MoveZombieRowBucket(this, anOldRow);
    }
}

//0x531C90
void Zombie::RiseFromGrave(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theCol, int theRow)
{
    TOD_ASSERT(mZombiePhase == ZombiePhase::PHASE_ZOMBIE_NORMAL);

    { mX = (mBoard->GridToPixelX(theCol, mRow) - 25); mRootBindingGeneration = 0; }
    mY = (GetPosYBasedOnRow(theRow));
    SetRow(theRow);


    mAltitude = CLIP_HEIGHT_OFF;
    mZombiePhase = ZombiePhase::PHASE_RISING_FROM_GRAVE;
    mPhaseCounter = 150;

    if (mBoard->StageHasPool())
    {
        mAltitude = -150.0f;
        mInPool = true;
        mPhaseCounter = 50;
        mZombieHeight = ZombieHeight::HEIGHT_ZOMBIE_NORMAL;

        StartWalkAnim(ownerNowNs, 0);
        ReanimIgnoreClipRect(ReanimTrackId::Zombie_duckytube, false);
        ReanimIgnoreClipRect(ReanimTrackId::Zombie_whitewater, false);
        ReanimIgnoreClipRect(ReanimTrackId::Zombie_outerarm_hand, false);
        ReanimIgnoreClipRect(ReanimTrackId::None /* Zombie_innerarm3 不在映射表：原版回退轨道 0 */, false);

        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        TodParticleSystem* aParticle = mApp->AddTodParticle(0.0f, 0.0f, 0, ParticleEffect::PARTICLE_ZOMBIE_SEAWEED, ownerNowNs);
        OverrideParticleScale(aParticle);

        if (mZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE && aParticle)
        {
            aBodyReanim->AttachParticleToTrack(ReanimTrackId::anim_cone, aParticle, 37.0f, 20.0f);
        }
        else if (mZombieType == ZombieType::ZOMBIE_PAIL && aParticle)
        {
            aBodyReanim->AttachParticleToTrack(ReanimTrackId::anim_bucket, aParticle, 37.0f, 20.0f);
        }
        else if (aParticle)
        {
            aBodyReanim->AttachParticleToTrack(ReanimTrackId::anim_head1, aParticle, 30.0f, 20.0f);
        }

        TodParticleSystem* aParticle2 = mApp->AddTodParticle(0.0f, 0.0f, 0, ParticleEffect::PARTICLE_ZOMBIE_SEAWEED, ownerNowNs);
        if (aParticle2)
        {
            OverrideParticleScale(aParticle2);
            aBodyReanim->AttachParticleToTrack(ReanimTrackId::Zombie_outerarm_upper, aParticle2, 5.0f, 5.0f);
        }

        TodParticleSystem* aParticle3 = mApp->AddTodParticle(0.0f, 0.0f, 0, ParticleEffect::PARTICLE_ZOMBIE_SEAWEED, ownerNowNs);
        if (aParticle3)
        {
            OverrideParticleScale(aParticle3);
            aBodyReanim->AttachParticleToTrack(ReanimTrackId::Zombie_duckytube, aParticle3, 77.0f, 20.0f);
        }

        PoolSplash(ownerNowNs, visualNowNs, animationNowNs, false);
    }
    else
    {
        int aParticleX = (mX) + 60;
        int aParticleY = (mY) + 110;
        if (IsOnHighGround())
        {
            aParticleY -= HIGH_GROUND_HEIGHT;
        }

        int aRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, theRow, 0);
        if (mApp->IsWhackAZombieLevel())
        {
            mApp->PlayFoley(FoleyType::FOLEY_DIRT_RISE);
            mApp->AddTodParticle(aParticleX, aParticleY, aRenderOrder, ParticleEffect::PARTICLE_WHACK_A_ZOMBIE_RISE, ownerNowNs);
        }
        else
        {
            mApp->PlayFoley(FoleyType::FOLEY_GRAVESTONE_RUMBLE);
            mApp->AddTodParticle(aParticleX, aParticleY, aRenderOrder, ParticleEffect::PARTICLE_ZOMBIE_RISE, ownerNowNs);
        }
    }
}

bool Zombie::IsZombotany(ZombieType theZombieType)
{
    // 2026-10-04 标志字表化（用户方案）：OR 链 → kZombieFlags 位判定；
    // 语义按构造等价（表行=原链成员集，ZombieFlags.h static_assert 锚）。
    return (ZombieFlagsOf(theZombieType) & ZombieFlags::Zombotany) != 0;
}

//0x5320B0
bool Zombie::ZombieTypeCanGoInPool(ZombieType theZombieType)
{
    return
        theZombieType == ZombieType::ZOMBIE_NORMAL ||
        theZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE ||
        theZombieType == ZombieType::ZOMBIE_PAIL ||
        theZombieType == ZombieType::ZOMBIE_FLAG ||
        theZombieType == ZombieType::ZOMBIE_ZOMBATAR || // 1073 0x542A10 also accepts the saved-avatar flag type (0x21).
        theZombieType == ZombieType::ZOMBIE_SNORKEL ||
        theZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER ||
		theZombieType == ZombieType::ZOMBIE_BALLOON ||
        theZombieType == ZombieType::ZOMBIE_PEA_HEAD ||
        theZombieType == ZombieType::ZOMBIE_WALLNUT_HEAD ||
        theZombieType == ZombieType::ZOMBIE_JALAPENO_HEAD ||
        theZombieType == ZombieType::ZOMBIE_GATLING_HEAD ||
        theZombieType == ZombieType::ZOMBIE_TALLNUT_HEAD;
}

bool Zombie::ZombieTypeCanGoOnHighGround(ZombieType theZombieType)
{
    return theZombieType != ZombieType::ZOMBIE_ZAMBONI && theZombieType != ZombieType::ZOMBIE_BOBSLED;
}

//0x5320B0
Rect Zombie::GetZombieRect()
{
    Rect aZombieRect = mZombieRect;
    if (IsWalkingBackwards())
    {
        aZombieRect.mX = mWidth - aZombieRect.mX - aZombieRect.mWidth;
    }

    ZombieDrawPosition aDrawPos;
    GetDrawPos(aDrawPos);
    aZombieRect.Offset((mX), (mY) + aDrawPos.mBodyY);
    if (aDrawPos.mClipHeight > CLIP_HEIGHT_LIMIT)
    {
        aZombieRect.mHeight -= aDrawPos.mClipHeight;
        // 1073 00542A60 retains signed height during grave emergence. Our
        // GetCircleRectOverlap uses the original ordered branches, not
        // std::clamp; forcing zero here changes the lower hammer hit region.
    }

    return aZombieRect;
}

//0x532140
Rect Zombie::GetZombieAttackRect()
{
    Rect aAttackRect = mZombieAttackRect;
    if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT || mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP)
    {
        aAttackRect = Rect(-40, 0, 100, 115);
    }

    if (IsWalkingBackwards())
    {
        aAttackRect.mX = mWidth - aAttackRect.mX - aAttackRect.mWidth;
    }

    ZombieDrawPosition aDrawPos;
    GetDrawPos(aDrawPos);
    aAttackRect.Offset((mX), (mY) + aDrawPos.mBodyY);
    if (aDrawPos.mClipHeight > CLIP_HEIGHT_LIMIT)
    {
        aAttackRect.mHeight -= aDrawPos.mClipHeight;
    }

    return aAttackRect;
}

//0x5321F0
TodParticleSystem* Zombie::AddAttachedParticle(std::int64_t ownerNowNs, int thePosX, int thePosY, ParticleEffect theEffect)
{
    if (mDead)
        return nullptr;

    if (IsFullOfAttachments(mAttachmentID))
        return nullptr;

    TodParticleSystem* aParticle = mApp->AddTodParticle((mX) + thePosX, (mY) + thePosY, 0, theEffect, ownerNowNs);
    if (aParticle)
    {
        AttachParticle(mAttachmentID, aParticle, thePosX, thePosY);
    }

    return aParticle;
}

//0x5322C0
Reanimation* Zombie::AddAttachedReanim(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int thePosX, int thePosY, ReanimationType theReanimType)
{
    if (mDead)
        return nullptr;

    Reanimation* aReanim = mApp->AddReanimation((mX) + thePosX, (mY) + thePosY, 0, theReanimType, ownerNowNs);
    if (aReanim)
    {
        AttachReanim(mAttachmentID, aReanim, thePosX, thePosY);
    }

    return aReanim;
}

//0x532350
void Zombie::RemoveIceTrap(std::int64_t ownerNowNs)
{

    mIceTrapEndTimeNs = 0;
    if (mZombieType == ZombieType::ZOMBIE_BALLOON)
    {
        BalloonPropellerHatSpin(true);
    }

    UpdateAnimSpeed(ownerNowNs);
    StartZombieSound();
}

//0x5323C0
void Zombie::HitIceTrap(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    bool cold = false;
    if (mChilledEndTimeNs > aStatusNowNs || mIceTrapEndTimeNs > aStatusNowNs)
    {
        cold = true;
    }

    ApplyChill(ownerNowNs, true);
    if (!CanBeFrozen())
        return;

    if (mInPool)
    {
        mIceTrapEndTimeNs = aStatusNowNs + static_cast<std::int64_t>(300) * 10'000'000;
    }
    else if (cold)
    {
        mIceTrapEndTimeNs = aStatusNowNs + static_cast<std::int64_t>(RandRangeInt(300, 400)) * 10'000'000;
    }
    else
    {
        mIceTrapEndTimeNs = aStatusNowNs + static_cast<std::int64_t>(RandRangeInt(400, 600)) * 10'000'000;
    }

    StopZombieSound();
    if (mZombieType == ZombieType::ZOMBIE_BALLOON)
    {
        BalloonPropellerHatSpin(false);
    }
    if (mZombiePhase == ZombiePhase::PHASE_BOSS_HEAD_SPIT)
    {
        mBoard->RemoveParticleByType(ParticleEffect::PARTICLE_ZOMBIE_BOSS_FIREBALL);
    }

    TakeDamage(ownerNowNs, visualNowNs, animationNowNs, 20, 1U);
    UpdateAnimSpeed(ownerNowNs);
}

//0x5324B0
bool Zombie::IsTangleKelpTarget()
{
    if (mZombieHeight == ZombieHeight::HEIGHT_DRAGGED_UNDER)
        return true;

    Plant* aPlant = nullptr;
    while (mBoard->IteratePlants(aPlant))
    {
        if (aPlant->mSeedType == SeedType::SEED_TANGLEKELP && aPlant->mTargetZombieID == mBoard->ZombieGetID(this))
        {
            return true;
        }
    }

    return false;
}

bool Zombie::IsSquashTarget(Plant* theExcept)
{
    ZombieID anId = mBoard->ZombieGetID(this);

    Plant* aPlant = nullptr;
    while (mBoard->IteratePlants(aPlant))
    {
        if (aPlant != theExcept && aPlant->mSeedType == SeedType::SEED_SQUASH && aPlant->mTargetZombieID == anId)
        {
            return true;
        }
    }

    return false;
}

bool Zombie::IsFireResistant()
{
    // 2026-10-04 标志字表化：类型半边查表（投石车/冰车），盾半边保持
    // 运行时判定（盾是动态装备属性，同一僵尸不同时刻不同——与
    // Zombie::IsFlying 同理不入类型表）。
    return (ZombieFlagsOf(mZombieType) & ZombieFlags::FireResistant) != 0 ||
        mShieldType == ShieldType::SHIELDTYPE_DOOR ||
        mShieldType == ShieldType::SHIELDTYPE_LADDER;
}

//0x532510
void Zombie::BalloonPropellerHatSpin(bool theSpinning)
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    ReanimatorTrackInstance* aHatTrackInstance = aBodyReanim->GetTrackInstanceByName(ReanimTrackId::hat);
    Reanimation* aPropellerReanim = FindReanimAttachment(aHatTrackInstance->mAttachmentID);
    if (aPropellerReanim)
    {
        if (theSpinning)
        {

            aPropellerReanim->ApplyRate(aPropellerReanim->mDefinition->mFPS);
        }
        else
        {

            aPropellerReanim->ApplyRate(0.0f);
        }
    }
}

//0x532570
void Zombie::RemoveButter(std::int64_t ownerNowNs)
{
    if (mZombieType == ZombieType::ZOMBIE_BALLOON)
    {
        BalloonPropellerHatSpin(true);
    }

    if (Zombie::IsZombotany(mZombieType))
    {
        Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mSpecialHeadReanimID);
        if (aHeadReanim)
        {
            if (mZombieType == ZombieType::ZOMBIE_PEA_HEAD && aHeadReanim->IsAnimPlaying(ReanimTrackId::anim_shooting))
            {

                aHeadReanim->ApplyRate(35.0f);
            }
            else if (mZombieType == ZombieType::ZOMBIE_GATLING_HEAD && aHeadReanim->IsAnimPlaying(ReanimTrackId::anim_shooting))
            {

                aHeadReanim->ApplyRate(38.0f);
            }
            else
            {

                aHeadReanim->ApplyRate(15.0f);
            }
        }
    }

    UpdateAnimSpeed(ownerNowNs);
    StartZombieSound();
}

//0x5326D0
void Zombie::ApplyButter(std::int64_t ownerNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    if (!mHasHead || !CanBeFrozen())
        return;

    if (mZombieType == ZombieType::ZOMBIE_ZAMBONI || ((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0) || IsTangleKelpTarget() || IsBobsledTeamWithSled() || IsFlying())
        return;

    mButteredEndTimeNs = aStatusNowNs + static_cast<std::int64_t>(400) * 10'000'000;
    Zombie* aZombie = mBoard->ZombieTryToGet(mRelatedZombieID);
    if (aZombie)
    {
        aZombie->mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
        mRelatedZombieID = ZombieID::ZOMBIEID_NULL;
    }

    if (mZombieType == ZombieType::ZOMBIE_POGO)
    {
        mAltitude = 0.0f;
        if (mOnHighGround)
        {
            mAltitude += HIGH_GROUND_HEIGHT;
        }
    }
    else if (mZombieType == ZombieType::ZOMBIE_BALLOON)
    {
        BalloonPropellerHatSpin(false);
    }
    else if (Zombie::IsZombotany(mZombieType))
    {
        Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mSpecialHeadReanimID);
        if (aHeadReanim)
        {

            aHeadReanim->ApplyRate(0.0f);
        }
    }

    UpdateAnimSpeed(ownerNowNs);
    StopZombieSound();
}

//0x5327E0
void Zombie::MowDown(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    if (mDead || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED || ((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0))
        return;

    if (mZombieType == ZombieType::ZOMBIE_CATAPULT)
    {
        mApp->AddTodParticle((mX) + 80.0f, (mY) + 60.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_CATAPULT_EXPLOSION, ownerNowNs);
        mApp->PlayFoley(FoleyType::FOLEY_EXPLOSION);
        DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
        return;
    }

    if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
    {
        mApp->AddTodParticle((mX) + 80.0f, (mY) + 60.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZAMBONI_EXPLOSION, ownerNowNs);
        mApp->PlayFoley(FoleyType::FOLEY_EXPLOSION);
        DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
        return;
    }

    if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING ||
        mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT ||
        mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_RISING ||
        mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL ||
        mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED ||
        mZombieType == ZombieType::ZOMBIE_GARGANTUAR ||
        mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR ||
        mZombieType == ZombieType::ZOMBIE_BUNGEE ||
        mZombieType == ZombieType::ZOMBIE_DIGGER ||
        mZombieType == ZombieType::ZOMBIE_IMP ||
        mZombieType == ZombieType::ZOMBIE_YETI ||
        mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER ||
        IsBobsledTeamWithSled() ||
        IsFlying() ||
        mInPool)
    {
        Reanimation* aPuffReanim = mApp->AddReanimation((mX) - 73.0f, (mY) - 56.0f, mRenderOrder + 2, ReanimationType::REANIM_PUFF, visualNowNs);

        aPuffReanim->SetFramesForLayer(ReanimTrackId::anim_puff);
        mApp->AddTodParticle((mX) + 110.0f, (mY) + 0.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_MOWER_CLOUD, ownerNowNs);

        if (mBoard->mPlantRow[mRow] != PlantRowType::PLANTROW_POOL)
        {
            DropHead(ownerNowNs, 0U);
            DropArm(ownerNowNs, 0U);
            DropHelm(ownerNowNs, 0U);
            DropShield(ownerNowNs, visualNowNs, animationNowNs, 0U);
        }

        DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
        return;
    }

    if (mIceTrapEndTimeNs > aStatusNowNs)
    {
        RemoveIceTrap(ownerNowNs);
    }
    if (mButteredEndTimeNs > aStatusNowNs)
    {
        mButteredEndTimeNs = 0;
    }

    DropShield(ownerNowNs, visualNowNs, animationNowNs, 0U);
    DropHelm(ownerNowNs, 0U);
    // C 形态 -> 匿名 lambda 函数指针表 kMowDownCases（仿 kSeedTypeUpdates
    // 范式）：行序 = ZombieType 枚举序 0..33；原 4 支中 FLAG/ZOMBATAR 与
    // NEWSPAPER/BALLOON 各为共体（constexpr 指针变量复用同一 lambda）；
    // 无 case 槽 = nullptr（原链无命中 = 无操作）。
    using MowDownCaseFn = void (*)(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie&);
    static constexpr MowDownCaseFn kMowDownDropFlag = [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z) { z.DropFlag(ownerNowNs); };
    static constexpr MowDownCaseFn kMowDownDropHead = [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z) { z.DropHead(ownerNowNs, 0U); };
    static constexpr MowDownCaseFn kMowDownCases[(int)ZombieType::NUM_ZOMBIE_TYPES] = {
        nullptr,  // 0 ZOMBIE_NORMAL（无 case）
        kMowDownDropFlag,  // 1 ZOMBIE_FLAG（共体）
        nullptr,  // 2 ZOMBIE_TRAFFIC_CONE（无 case）
        // 原 else-if ZOMBIE_POLEVAULTER
        [](std::int64_t , std::int64_t , std::int64_t , Zombie& z) { z.DropPole(); },  // 3 ZOMBIE_POLEVAULTER
        nullptr,  // 4 ZOMBIE_PAIL（无 case）
        kMowDownDropHead,  // 5 ZOMBIE_NEWSPAPER（共体）
        nullptr,  // 6 ZOMBIE_DOOR（无 case）
        nullptr,  // 7 ZOMBIE_FOOTBALL（无 case）
        nullptr,  // 8 ZOMBIE_DANCER（无 case）
        nullptr,  // 9 ZOMBIE_BACKUP_DANCER（无 case）
        nullptr,  // 10 ZOMBIE_DUCKY_TUBE（无 case）
        nullptr,  // 11 ZOMBIE_SNORKEL（无 case）
        nullptr,  // 12 ZOMBIE_ZAMBONI（无 case）
        nullptr,  // 13 ZOMBIE_BOBSLED（无 case）
        nullptr,  // 14 ZOMBIE_DOLPHIN_RIDER（无 case）
        nullptr,  // 15 ZOMBIE_JACK_IN_THE_BOX（无 case）
        kMowDownDropHead,  // 16 ZOMBIE_BALLOON（共体）
        nullptr,  // 17 ZOMBIE_DIGGER（无 case）
        // 原 else-if ZOMBIE_POGO
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z)
        {
            z.DropHead(ownerNowNs, 0U);
            z.mAltitude = 0.0f;
        },  // 18 ZOMBIE_POGO
        nullptr,  // 19 ZOMBIE_YETI（无 case）
        nullptr,  // 20 ZOMBIE_BUNGEE（无 case）
        nullptr,  // 21 ZOMBIE_LADDER（无 case）
        nullptr,  // 22 ZOMBIE_CATAPULT（无 case）
        nullptr,  // 23 ZOMBIE_GARGANTUAR（无 case）
        nullptr,  // 24 ZOMBIE_IMP（无 case）
        nullptr,  // 25 ZOMBIE_BOSS（无 case）
        nullptr,  // 26 ZOMBIE_PEA_HEAD（无 case）
        nullptr,  // 27 ZOMBIE_WALLNUT_HEAD（无 case）
        nullptr,  // 28 ZOMBIE_JALAPENO_HEAD（无 case）
        nullptr,  // 29 ZOMBIE_GATLING_HEAD（无 case）
        nullptr,  // 30 ZOMBIE_SQUASH_HEAD（无 case）
        nullptr,  // 31 ZOMBIE_TALLNUT_HEAD（无 case）
        nullptr,  // 32 ZOMBIE_REDEYE_GARGANTUAR（无 case）
        kMowDownDropFlag,  // 33 ZOMBIE_ZOMBATAR（共体）
    };
    static_assert(sizeof(kMowDownCases) / sizeof(kMowDownCases[0]) ==
        (int)ZombieType::NUM_ZOMBIE_TYPES, "kMowDownCases 槽位须与 ZombieType 枚举同步");

    const unsigned aTypeIndex = static_cast<unsigned>(mZombieType);
    if (aTypeIndex < sizeof(kMowDownCases) / sizeof(kMowDownCases[0]) &&
        kMowDownCases[aTypeIndex] != nullptr)
    {
        kMowDownCases[aTypeIndex](ownerNowNs, visualNowNs, animationNowNs, *this);
    }

    Reanimation* aMoweredReanim = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder, ReanimationType::REANIM_LAWN_MOWERED_ZOMBIE, visualNowNs);
    aMoweredReanim->ApplyRate(8.0f);
    aMoweredReanim->mIsAttachment = false;
    aMoweredReanim->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
    mMoweredReanimID = mApp->ReanimationGetID(aMoweredReanim);
    mZombiePhase = ZombiePhase::PHASE_ZOMBIE_MOWERED;
    DropLoot();
}

//0x532B40
void Zombie::RemoveColdEffects(std::int64_t ownerNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    if (mIceTrapEndTimeNs > aStatusNowNs)
    {
        RemoveIceTrap(ownerNowNs);
    }

    if (mChilledEndTimeNs > aStatusNowNs)
    {
        mChilledEndTimeNs = 0;
        UpdateAnimSpeed(ownerNowNs);
    }
}

//0x532B70
void Zombie::ApplyBurn(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    if (mDead || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED)
        return;

    if (mBodyHealth >= 1800 || ((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0))
    {
        TakeDamage(ownerNowNs, visualNowNs, animationNowNs, 1800, 18U);
        return;
    }

    if (mZombieType == ZombieType::ZOMBIE_SQUASH_HEAD && !mHasHead)
    {
        mApp->RemoveReanimation(mSpecialHeadReanimID);
        mSpecialHeadReanimID = ReanimationID::REANIMATIONID_NULL;
    }

    if (mIceTrapEndTimeNs > aStatusNowNs)
    {
        RemoveIceTrap(ownerNowNs);
    }
    if (mButteredEndTimeNs > aStatusNowNs)
    {
        mButteredEndTimeNs = 0;
    }

    AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_ZAMBONI_SMOKE);
    BungeeDropPlant(ownerNowNs, visualNowNs, animationNowNs);

    if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING ||
        mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT ||
        mZombiePhase == ZombiePhase::PHASE_IMP_GETTING_THROWN ||
        mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_RISING ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_INTO_POOL ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_IN_JUMP ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING ||
        mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE ||
        mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED ||
        mInPool)
    {
        DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
    }
    else if (mZombieType == ZOMBIE_BUNGEE || mZombieType == ZOMBIE_YETI || Zombie::IsZombotany(mZombieType) || IsBobsledTeamWithSled() || IsFlying() || !mHasHead)
    {
        SetAnimRate(ownerNowNs, 0.0f);
        Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mSpecialHeadReanimID);
        if (aHeadReanim)
        {

            aHeadReanim->ApplyRate(0.0f);
        }

        mZombiePhase = ZombiePhase::PHASE_ZOMBIE_BURNED;
        mPhaseCounter = 300;
        mShotFlashEndTimeNs = aStatusNowNs;
        DropLoot();

        if (mZombieType == ZombieType::ZOMBIE_BALLOON)
        {
            BalloonPropellerHatSpin(false);
        }
    }
    else
    {
        ReanimationType aReanimType = ReanimationType::REANIM_ZOMBIE_CHARRED;
        float aCharredPosX = (mX) + 22.0f;
        float aCharredPosY = (mY) - 10.0f;
        if (mZombieType == ZombieType::ZOMBIE_BALLOON)
        {
            aCharredPosY += 31.0f;
        }
        if (mZombieType == ZombieType::ZOMBIE_IMP)
        {
            aCharredPosX -= 6.0f;
            aReanimType = ReanimationType::REANIM_ZOMBIE_CHARRED_IMP;
        }
        if (mZombieType == ZombieType::ZOMBIE_DIGGER)
        {
            if (IsWalkingBackwards())
            {
                aCharredPosX += 14.0f;
            }
            aReanimType = ReanimationType::REANIM_ZOMBIE_CHARRED_DIGGER;
        }
        if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
        {
            aReanimType = ReanimationType::REANIM_ZOMBIE_CHARRED_ZAMBONI;
            aCharredPosX += 61.0f;
            aCharredPosY -= 16.0f;
        }
        if (mZombieType == ZombieType::ZOMBIE_CATAPULT)
        {
            aReanimType = ReanimationType::REANIM_ZOMBIE_CHARRED_CATAPULT;
            aCharredPosX -= 36.0f;
            aCharredPosY -= 20.0f;
        }
        if (mZombieType == ZombieType::ZOMBIE_GARGANTUAR || mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
        {
            aReanimType = ReanimationType::REANIM_ZOMBIE_CHARRED_GARGANTUAR;
            aCharredPosX -= 15.0f;
            aCharredPosY -= 10.0f;
        }

        Reanimation* aCharredReanim = mApp->AddReanimation(aCharredPosX, aCharredPosY, mRenderOrder, aReanimType, visualNowNs);

        aCharredReanim->ApplyRate(aCharredReanim->mAnimRate * (RandRangeFloat(0.9f, 1.1f)));
        if (mZombiePhase == ZombiePhase::PHASE_DIGGER_WALKING_WITHOUT_AXE)
        {

            aCharredReanim->SetFramesForLayer(ReanimTrackId::anim_crumble_noaxe);
        }
        else if (mZombieType == ZombieType::ZOMBIE_DIGGER)
        {

            aCharredReanim->SetFramesForLayer(ReanimTrackId::anim_crumble);
        }
        else if ((mZombieType == ZombieType::ZOMBIE_GARGANTUAR || mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR) && !mHasObject)
        {
            aCharredReanim->SetVariantOption(ReanimTrackId::impblink, ReanimOptions::Zombie_charred_gargantuar::impblink::BLANK);
            aCharredReanim->SetVariantOption(ReanimTrackId::imphead, ReanimOptions::Zombie_charred_gargantuar::imphead::BLANK);
        }

        // GOTY 1073 resets both dancer variants to the unscaled charred-body
        // size.  Their live reanimations use a special preview/body scale that
        // must not be inherited by the generic burn silhouette.
        if (mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
        {
            mScaleZombie = 1.0f;
        }

        if (mScaleZombie != 1.0f)
        {
            aCharredReanim->mOverlayMatrix.m00 = mScaleZombie;
            aCharredReanim->mOverlayMatrix.m11 = mScaleZombie;
            aCharredReanim->mOverlayMatrix.m02 += 20.0f - mScaleZombie * 20.0f;
            aCharredReanim->mOverlayMatrix.m12 += 120.0f - mScaleZombie * 120.0f;
            aCharredReanim->mOverlayMatrix.m00 = mScaleZombie;
            aCharredReanim->mOverlayMatrix.m11 = mScaleZombie;
        }

        if (IsWalkingBackwards())
        {
            aCharredReanim->mOverlayMatrix.m00 = -mScaleZombie;
            aCharredReanim->mOverlayMatrix.m11 = mScaleZombie;
            aCharredReanim->mOverlayMatrix.m02 += 60.0f * mScaleZombie;
        }

        DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
    }

    if (mZombieType == ZombieType::ZOMBIE_BOBSLED)
    {
        BobsledBurn(ownerNowNs, visualNowNs, animationNowNs);
    }
}

//0x533000
void Zombie::AttachShield()
{
    ReanimTrackId aTrackName{};  // 显式零初始化（clang -Wsometimes-uninitialized：else 分支 TOD_ASSERT(false) 后不可达但编译器仍告警）
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    if (mShieldType == ShieldType::SHIELDTYPE_DOOR)
    {
        ShowDoorArms(true);
        ReanimShowRange(ReanimTrackRanges::Zombie_outerarm_screendoor, RENDER_GROUP_OVER_SHIELD);
        aTrackName = ReanimTrackId::anim_screendoor;
    }
    else if (mShieldType == ShieldType::SHIELDTYPE_NEWSPAPER)
    {
        ReanimShowRange(ReanimTrackRanges::Zombie_paper_hands, RENDER_GROUP_OVER_SHIELD);
        aTrackName = ReanimTrackId::Zombie_paper_paper;
    }
    else if (mShieldType == ShieldType::SHIELDTYPE_LADDER)
    {
        ReanimShowRange(ReanimTrackRanges::Zombie_outerarm, RENDER_GROUP_OVER_SHIELD);
        aTrackName = ReanimTrackId::Zombie_ladder_1;
    }
    else
    {
        TOD_ASSERT(false);
    }

    aBodyReanim->AssignRenderGroupToTrack(aTrackName, RENDER_GROUP_SHIELD);
}

//0x5330E0
void Zombie::DetachShield(std::int64_t ownerNowNs)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (aBodyReanim)
    {
        if (mShieldType == ShieldType::SHIELDTYPE_DOOR)
        {
            ShowDoorArms(false);
        }
        else if (mShieldType == ShieldType::SHIELDTYPE_NEWSPAPER)
        {
            ReanimShowRange(ReanimTrackRanges::Zombie_paper_hands, RENDER_GROUP_NORMAL);
        }
        else if (mShieldType == ShieldType::SHIELDTYPE_LADDER)
        {
            // 扶梯断臂重生修复（用户裁决 2026-10-03 启用，偏离 1073 原版）：
            // 原版无条件 re-show 外臂轨道——已断臂的扶梯僵尸走此分支时断掉的
            // 手臂"重生"。仅存臂时才恢复外臂显示。
            if (mHasArm)
            {
                ReanimShowRange(ReanimTrackRanges::Zombie_outerarm, RENDER_GROUP_NORMAL);
            }
            mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
            if (mIsEating)
            {
                PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_eat, ReanimLoopType::REANIM_LOOP, 20, 0.0f);
            }
            else
            {
                StartWalkAnim(ownerNowNs, 0);
            }
        }
        else
        {
            TOD_ASSERT(false);
        }
    }

    mShieldType = ShieldType::SHIELDTYPE_NONE;
    mShieldHealth = 0;
}

//0x5331C0
// GOTY @Patoke: 0x543BB0
void Zombie::ReanimShowRange(const ReanimTrackIdRange& theTrackRange, int theRenderGroup)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (aBodyReanim)
    {
        aBodyReanim->AssignRenderGroupToRange(theTrackRange, theRenderGroup);
    }
}

//0x533200
// GOTY @Patoke: 0x543C00
void Zombie::ReanimShowTrack(ReanimTrackId theTrackId, int theRenderGroup)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (aBodyReanim)
    {
        aBodyReanim->AssignRenderGroupToTrack(theTrackId, theRenderGroup);
    }
}

//0x533240
void Zombie::PlayDeathAnim(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, unsigned int theDamageFlags)
{
    const auto aStatusNowNs = ownerNowNs;

    if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED || mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED)
        return;

    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (aBodyReanim == nullptr || !aBodyReanim->TrackExists(ReanimTrackId::anim_death))
    {
        DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
        return;
    }
    if (mZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER && mZombiePhase != ZombiePhase::PHASE_DOLPHIN_WALKING_IN_POOL)
    {
        DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
        return;
    }
    if (mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL || mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING)
    {
        DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
        return;
    }

    if (mIceTrapEndTimeNs > aStatusNowNs)
    {
        AddAttachedParticle(ownerNowNs, 75, 106, ParticleEffect::PARTICLE_ICE_TRAP_RELEASE);
        mIceTrapEndTimeNs = 0;
    }
    if (mButteredEndTimeNs > aStatusNowNs)
    {
        mButteredEndTimeNs = 0;
    }
    if (mYuckyFace)
    {
        ShowYuckyFace(false);
        mYuckyFace = false;
        mYuckyFaceStartTimeNs = aStatusNowNs;
    }

    if (TestBit(theDamageFlags, (int)DamageFlags::DAMAGE_DOESNT_LEAVE_BODY))
    {
        if (mZombieType != ZombieType::ZOMBIE_BOSS && mZombieType != ZombieType::ZOMBIE_GARGANTUAR && mZombieType != ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
        {
            DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
            return;
        }
    }

    if (mZombieType == ZombieType::ZOMBIE_POGO)
    {
        mAltitude = 0.0f;
    }

    AttachmentReanimTypeDie(mAttachmentID, ReanimationType::REANIM_ZOMBIE_SURPRISE);
    StopEating(ownerNowNs);

    if (mShieldType != ShieldType::SHIELDTYPE_NONE)
    {
        DropShield(ownerNowNs, visualNowNs, animationNowNs, 1U);
    }
    if (mZombieType == ZombieType::ZOMBIE_SQUASH_HEAD && !mHasHead)
    {
        mApp->RemoveReanimation(mSpecialHeadReanimID);
        mSpecialHeadReanimID = ReanimationID::REANIMATIONID_NULL;
    }

    mVelX = 0.0f;
    mZombiePhase = ZombiePhase::PHASE_ZOMBIE_DYING;
    if (mZombieHeight == ZombieHeight::HEIGHT_ZOMBIQUARIUM)
    {
        PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_aquarium_death, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 14.0f);
        return;
    }
    if (mZombieHeight == ZombieHeight::HEIGHT_UP_LADDER)
    {
        mZombieHeight = ZombieHeight::HEIGHT_FALLING;
    }

    // C 形态 -> 匿名 lambda 函数指针表 kDeathAnimRateCases（仿 kIceTrapAdjusts
    // 范式）：行序 = ZombieType 枚举序 0..33；无 case 槽 = 原 else 支（随机
    // 死亡速率，constexpr 指针变量填满）。GARGANTUAR/REDEYE 共体（速率 14 +
    // 巨人死亡音效）。速率经 float& 出参回写。
    using DeathAnimRateFn = void (*)(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie&, float&);
    static constexpr DeathAnimRateFn kDeathAnimRateRandom = [](std::int64_t , std::int64_t , std::int64_t , Zombie& , float& theRate)
    {
        theRate = RandRangeFloat(24.0f, 30.0f);
    };
    static constexpr DeathAnimRateFn kDeathAnimRateGargantuar = [](std::int64_t , std::int64_t , std::int64_t , Zombie& z, float& theRate)
    {
        theRate = 14.0f;
        z.mApp->PlayFoley(FoleyType::FOLEY_GARGANTUDEATH);
    };
    static constexpr DeathAnimRateFn kDeathAnimRateCases[(int)ZombieType::NUM_ZOMBIE_TYPES] = {
        kDeathAnimRateRandom,  // 0 ZOMBIE_NORMAL（原 else）
        kDeathAnimRateRandom,  // 1 ZOMBIE_FLAG（原 else）
        kDeathAnimRateRandom,  // 2 ZOMBIE_TRAFFIC_CONE（原 else）
        kDeathAnimRateRandom,  // 3 ZOMBIE_POLEVAULTER（原 else）
        kDeathAnimRateRandom,  // 4 ZOMBIE_PAIL（原 else）
        kDeathAnimRateRandom,  // 5 ZOMBIE_NEWSPAPER（原 else）
        kDeathAnimRateRandom,  // 6 ZOMBIE_DOOR（原 else）
        // 原 if ZOMBIE_FOOTBALL
        [](std::int64_t , std::int64_t , std::int64_t , Zombie& , float& theRate) { theRate = 24.0f; },  // 7 ZOMBIE_FOOTBALL
        kDeathAnimRateRandom,  // 8 ZOMBIE_DANCER（原 else）
        kDeathAnimRateRandom,  // 9 ZOMBIE_BACKUP_DANCER（原 else）
        kDeathAnimRateRandom,  // 10 ZOMBIE_DUCKY_TUBE（原 else）
        [](std::int64_t , std::int64_t , std::int64_t , Zombie& , float& theRate) { theRate = 14.0f; },  // 11 ZOMBIE_SNORKEL
        kDeathAnimRateRandom,  // 12 ZOMBIE_ZAMBONI（原 else）
        kDeathAnimRateRandom,  // 13 ZOMBIE_BOBSLED（原 else）
        kDeathAnimRateRandom,  // 14 ZOMBIE_DOLPHIN_RIDER（原 else）
        kDeathAnimRateRandom,  // 15 ZOMBIE_JACK_IN_THE_BOX（原 else）
        kDeathAnimRateRandom,  // 16 ZOMBIE_BALLOON（原 else）
        [](std::int64_t , std::int64_t , std::int64_t , Zombie& , float& theRate) { theRate = 18.0f; },  // 17 ZOMBIE_DIGGER
        kDeathAnimRateRandom,  // 18 ZOMBIE_POGO（原 else）
        [](std::int64_t , std::int64_t , std::int64_t , Zombie& , float& theRate) { theRate = 14.0f; },  // 19 ZOMBIE_YETI
        kDeathAnimRateRandom,  // 20 ZOMBIE_BUNGEE（原 else）
        kDeathAnimRateRandom,  // 21 ZOMBIE_LADDER（原 else）
        kDeathAnimRateRandom,  // 22 ZOMBIE_CATAPULT（原 else）
        kDeathAnimRateGargantuar,  // 23 ZOMBIE_GARGANTUAR（共体）
        kDeathAnimRateRandom,  // 24 ZOMBIE_IMP（原 else）
        // 原 else-if ZOMBIE_BOSS
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, float& theRate)
        {
            theRate = 18.0f;

            z.BossDie(ownerNowNs, visualNowNs, animationNowNs);
            Reanimation* aHeadReanim = z.mApp->ReanimationGet(z.mSpecialHeadReanimID);

            aHeadReanim->PlayReanim(ReanimTrackId::anim_death, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, theRate);
        },  // 25 ZOMBIE_BOSS
        kDeathAnimRateRandom,  // 26 ZOMBIE_PEA_HEAD（原 else）
        kDeathAnimRateRandom,  // 27 ZOMBIE_WALLNUT_HEAD（原 else）
        kDeathAnimRateRandom,  // 28 ZOMBIE_JALAPENO_HEAD（原 else）
        kDeathAnimRateRandom,  // 29 ZOMBIE_GATLING_HEAD（原 else）
        kDeathAnimRateRandom,  // 30 ZOMBIE_SQUASH_HEAD（原 else）
        kDeathAnimRateRandom,  // 31 ZOMBIE_TALLNUT_HEAD（原 else）
        kDeathAnimRateGargantuar,  // 32 ZOMBIE_REDEYE_GARGANTUAR（共体）
        kDeathAnimRateRandom,  // 33 ZOMBIE_ZOMBATAR（原 else）
    };
    static_assert(sizeof(kDeathAnimRateCases) / sizeof(kDeathAnimRateCases[0]) ==
        (int)ZombieType::NUM_ZOMBIE_TYPES, "kDeathAnimRateCases 槽位须与 ZombieType 枚举同步");

    float aDeathAnimRate = 0.0f;
    const unsigned aTypeIndex = static_cast<unsigned>(mZombieType);
    if (aTypeIndex < sizeof(kDeathAnimRateCases) / sizeof(kDeathAnimRateCases[0]))
    {
        kDeathAnimRateCases[aTypeIndex](ownerNowNs, visualNowNs, animationNowNs, *this, aDeathAnimRate);
    }
    else
    {
        aDeathAnimRate = RandRangeFloat(24.0f, 30.0f);  // 原 else 支（哨兵越界等价）
    }

    ReanimTrackId aDeathTrackName = ReanimTrackId::anim_death;
    int aDeathAnimHit = Rand(100);
    bool aCanDoSuperLongDeath = mApp->HasFinishedAdventure() || mBoard->mLevel > 5;
    if (mInPool && aBodyReanim->TrackExists(ReanimTrackId::anim_waterdeath))
    {
        aDeathTrackName = ReanimTrackId::anim_waterdeath;
        ReanimIgnoreClipRect(ReanimTrackId::Zombie_duckytube, false);
    }
    else if (aDeathAnimHit == 99 && aBodyReanim->TrackExists(ReanimTrackId::anim_superlongdeath) && aCanDoSuperLongDeath && mChilledEndTimeNs <= aStatusNowNs && mBoard->CountZombiesOnScreen() <= 5)
    {
        aDeathAnimRate = 14.0f;
        aDeathTrackName = ReanimTrackId::anim_superlongdeath;
    }
    else if (aDeathAnimHit > 50 && aBodyReanim->TrackExists(ReanimTrackId::anim_death2))
    {
        aDeathTrackName = ReanimTrackId::anim_death2;
    }

    PlayZombieReanim(ownerNowNs, aDeathTrackName, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, aDeathAnimRate);
    ReanimShowRange(ReanimTrackRanges::anim_tongue, RENDER_GROUP_HIDDEN);
}

//0x5335A0
void Zombie::DoDaisies(std::int64_t ownerNowNs)
{
    if (IsWalkingBackwards())
        return;

    if (mBoard->mPlantRow[mRow] == PlantRowType::PLANTROW_POOL)
        return;

    if (mZombieType == ZombieType::ZOMBIE_BOBSLED || mZombieType == ZombieType::ZOMBIE_ZAMBONI || mZombieType == ZombieType::ZOMBIE_CATAPULT)
        return;

    if (mBoard->StageHasRoof())
        return;

    float aOffsetX = 20.0f;
    float aOffsetY = 100.0f;
    if (mZombieType == ZombieType::ZOMBIE_FOOTBALL || mZombieType == ZombieType::ZOMBIE_DANCER || mZombieType == ZombieType::ZOMBIE_BACKUP_DANCER)
    {
        aOffsetX += 160.0f;
    }
    else if (mZombieType == ZombieType::ZOMBIE_POGO)
    {
        aOffsetY += 120.0f;
    }
    else if (mZombieType == ZombieType::ZOMBIE_BALLOON)
    {
        aOffsetY += 30.0f;
        aOffsetX += 110.0f;
    }
    if (mBoard->StageHasGraveStones())
    {
        aOffsetY += 15.0f;
    }

    int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, mRow, 5);
    mApp->AddTodParticle((mX) + aOffsetX, (mY) + aOffsetY, aRenderPosition, ParticleEffect::PARTICLE_ZOMBIE_DAISIES, ownerNowNs);
}

namespace
{
// UpdateDeath 落体时刻表：-1=无落体物理（原首组/default），0=依播放动画
// 三选一（原普通头组，逻辑在表外），其余=每型常量。生成器解析原 switch 生成，
// 表值逐型 + 动画组块 token 对拍 0 差异（tools/rewrite_falltime_table.py）。
constexpr float kFallTimeNone = -1.0f;
constexpr float kFallTimeByAnim = 0.0f;
constexpr float kZombieFallTime[(int)ZombieType::NUM_ZOMBIE_TYPES] = {
    kFallTimeByAnim,  // 0 ZOMBIE_NORMAL
    kFallTimeByAnim,  // 1 ZOMBIE_FLAG
    kFallTimeByAnim,  // 2 ZOMBIE_TRAFFIC_CONE
    0.68f,  // 3 ZOMBIE_POLEVAULTER
    kFallTimeByAnim,  // 4 ZOMBIE_PAIL
    0.63f,  // 5 ZOMBIE_NEWSPAPER
    kFallTimeByAnim,  // 6 ZOMBIE_DOOR
    0.52f,  // 7 ZOMBIE_FOOTBALL
    0.83f,  // 8 ZOMBIE_DANCER
    0.83f,  // 9 ZOMBIE_BACKUP_DANCER
    kFallTimeByAnim,  // 10 ZOMBIE_DUCKY_TUBE
    kFallTimeNone,  // 11 ZOMBIE_SNORKEL
    kFallTimeNone,  // 12 ZOMBIE_ZAMBONI
    0.81f,  // 13 ZOMBIE_BOBSLED
    kFallTimeNone,  // 14 ZOMBIE_DOLPHIN_RIDER
    0.64f,  // 15 ZOMBIE_JACK_IN_THE_BOX
    0.68f,  // 16 ZOMBIE_BALLOON
    0.85f,  // 17 ZOMBIE_DIGGER
    0.84f,  // 18 ZOMBIE_POGO
    0.68f,  // 19 ZOMBIE_YETI
    kFallTimeNone,  // 20 ZOMBIE_BUNGEE
    0.62f,  // 21 ZOMBIE_LADDER
    kFallTimeNone,  // 22 ZOMBIE_CATAPULT
    0.86f,  // 23 ZOMBIE_GARGANTUAR
    kFallTimeNone,  // 24 ZOMBIE_IMP
    kFallTimeNone,  // 25 ZOMBIE_BOSS
    kFallTimeByAnim,  // 26 ZOMBIE_PEA_HEAD
    kFallTimeByAnim,  // 27 ZOMBIE_WALLNUT_HEAD
    kFallTimeByAnim,  // 28 ZOMBIE_JALAPENO_HEAD
    kFallTimeByAnim,  // 29 ZOMBIE_GATLING_HEAD
    kFallTimeByAnim,  // 30 ZOMBIE_SQUASH_HEAD
    kFallTimeByAnim,  // 31 ZOMBIE_TALLNUT_HEAD
    0.86f,  // 32 ZOMBIE_REDEYE_GARGANTUAR
    kFallTimeByAnim,  // 33 ZOMBIE_ZOMBATAR
};
}  // namespace

//0x5336B0
void Zombie::UpdateDeath(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (aBodyReanim == nullptr)
    {
        DieNoLoot(ownerNowNs, visualNowNs, animationNowNs);
        return;
    }

    if (mZombieHeight == ZombieHeight::HEIGHT_FALLING)
    {
        UpdateZombieFalling(ownerNowNs);
    }
    if (mZombieType == ZombieType::ZOMBIE_GARGANTUAR || mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
    {
        if (aBodyReanim->ShouldTriggerTimedEvent(0.89f))
        {
            mBoard->mShakeCounter = 12;
            mBoard->mShakeAmountX = 0;
            mBoard->mShakeAmountY = 3;
        }
        else if (aBodyReanim->ShouldTriggerTimedEvent(0.98f))
        {
            mBoard->mShakeCounter = 12;
            mBoard->mShakeAmountX = 0;
            mBoard->mShakeAmountY = 1;
        }
    }

    if (!mInPool)
    {
                // C 形态内嵌稠密 switch -> float 数据表（见上方 kZombieFallTime）：
        // 哨兵 kFallTimeNone/kFallTimeByAnim 之外即每型常量；依动画三选一
        // 组（表外）逐句保留。
        float aFallTime = kFallTimeNone;
        const unsigned aTypeIndex = static_cast<unsigned>(mZombieType);
        if (aTypeIndex < static_cast<unsigned>(ZombieType::NUM_ZOMBIE_TYPES))
        {
            aFallTime = kZombieFallTime[aTypeIndex];
        }
        if (aFallTime == kFallTimeByAnim)
        {
            if (aBodyReanim->IsAnimPlaying(ReanimTrackId::anim_superlongdeath))
            {
                aFallTime = 0.788f;
            }
            else if (aBodyReanim->IsAnimPlaying(ReanimTrackId::anim_death2))
            {
                aFallTime = 0.71f;
            }
            else
            {
                aFallTime = 0.77f;
            }
        }


        if (aFallTime > 0 && aBodyReanim->ShouldTriggerTimedEvent(aFallTime))
        {
            mApp->PlayFoley(FoleyType::FOLEY_ZOMBIE_FALLING);
            if (mZombieType == ZombieType::ZOMBIE_GARGANTUAR || mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
            {
                mApp->PlayFoley(FoleyType::FOLEY_THUMP);
            }

            if (mBoard->mDaisyMode)
            {
                DoDaisies(ownerNowNs);
            }
        }
    }

    if (((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0))
    {
        if (aBodyReanim->ShouldTriggerTimedEvent(0.1f) ||
            aBodyReanim->ShouldTriggerTimedEvent(0.12f) ||
            aBodyReanim->ShouldTriggerTimedEvent(0.15f) ||
            aBodyReanim->ShouldTriggerTimedEvent(0.19f) ||
            aBodyReanim->ShouldTriggerTimedEvent(0.2f) ||
            aBodyReanim->ShouldTriggerTimedEvent(0.26f) ||
            aBodyReanim->ShouldTriggerTimedEvent(0.3f) ||
            aBodyReanim->ShouldTriggerTimedEvent(0.4f) ||
            aBodyReanim->ShouldTriggerTimedEvent(0.42f) ||
            aBodyReanim->ShouldTriggerTimedEvent(0.5f) ||
            aBodyReanim->ShouldTriggerTimedEvent(0.58f) ||
            aBodyReanim->ShouldTriggerTimedEvent(0.61f) ||
            aBodyReanim->ShouldTriggerTimedEvent(0.71f))
        {
            float aExplosionPosX = RandRangeFloat(600.0f, 750.0f);
            float aExplosionPosY = RandRangeFloat(50.0f, 300.0f);
            mApp->AddTodParticle(aExplosionPosX, aExplosionPosY, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_BOSS_EXPLOSION, ownerNowNs);
            mApp->PlayFoley(FoleyType::FOLEY_BOSS_EXPLOSION_SMALL);
        }

        Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mSpecialHeadReanimID);
        if (aBodyReanim->ShouldTriggerTimedEvent(0.93f))
        {
            mBoard->mShakeCounter = 12;
            mBoard->mShakeAmountX = 1;
            mBoard->mShakeAmountY = 2;
            mApp->PlayFoley(FoleyType::FOLEY_BOSS_EXPLOSION_SMALL);
            mApp->PlayFoley(FoleyType::FOLEY_THUMP);
        }

        if (aBodyReanim->ShouldTriggerTimedEvent(0.99f))
        {

            aHeadReanim->PlayReanim(ReanimTrackId::anim_flag, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 30.0f);
        }

        if (aHeadReanim->IsAnimPlaying(ReanimTrackId::anim_flag) && aHeadReanim->mLoopCount > 0)
        {

            aHeadReanim->PlayReanim(ReanimTrackId::anim_flag_loop, ReanimLoopType::REANIM_LOOP, 20, 17.0f);
        }

        if (aBodyReanim->mLoopCount > 0)
        {
            DropLoot();
        }
    }

    if (mZombieType == ZombieType::ZOMBIE_ZAMBONI && mPhaseCounter > 0)
    {
        mPhaseCounter--;
        if (mPhaseCounter == 0)
        {
            aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
            if (aBodyReanim->IsTrackShowing(ReanimTrackId::anim_wheelie2))
            {
                mApp->AddTodParticle((mX) + 80.0f, (mY) + 60.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZAMBONI_EXPLOSION2, ownerNowNs);
            }
            else
            {
                mApp->AddTodParticle((mX) + 80.0f, (mY) + 60.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_ZAMBONI_EXPLOSION, ownerNowNs);
            }

            DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
            mApp->PlayFoley(FoleyType::FOLEY_EXPLOSION);
        }
    }
    else if (mZombieType == ZombieType::ZOMBIE_CATAPULT)
    {
        mPhaseCounter--;
        if (mPhaseCounter == 0)
        {
            mApp->AddTodParticle((mX) + 80.0f, (mY) + 60.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_CATAPULT_EXPLOSION, ownerNowNs);
            DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
            mApp->PlayFoley(FoleyType::FOLEY_EXPLOSION);
        }
    }
    else if (mFadeEndTimeNs == -1 && aBodyReanim->mLoopCount > 0 && mZombieType != ZombieType::ZOMBIE_BOSS)
    {
        mFadeEndTimeNs = aStatusNowNs + static_cast<std::int64_t>(mInPool ? 10 : 100) * 10'000'000;
    }
}

//0x533F10
void Zombie::UpdateMowered(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    Reanimation* aMoweredReanim = mApp->ReanimationTryToGet(mMoweredReanimID);
    if (aMoweredReanim == nullptr || aMoweredReanim->mLoopCount > 0)
    {
        DropHead(ownerNowNs, 0U);
        DropArm(ownerNowNs, 0U);
        DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
    }
}

//0x533F70
bool Zombie::HasShadow()
{
    if (mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_RISING ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING_PAUSE_WITHOUT_AXE ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_RISE_WITHOUT_AXE ||
        mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING ||
        mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE ||
        mZombiePhase == ZombiePhase::PHASE_DANCER_RISING ||
        mZombiePhase == ZombiePhase::PHASE_BOBSLED_BOARDING ||
        mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT ||
        mZombiePhase == ZombiePhase::PHASE_DOLPHIN_INTO_POOL ||
        mZombiePhase == ZombiePhase::PHASE_SNORKEL_INTO_POOL)
        return false;

    if (mZombieType == ZombieType::ZOMBIE_ZAMBONI ||
        mZombieType == ZombieType::ZOMBIE_CATAPULT ||
        ((ZombieFlagsOf(mZombieType) & ZombieFlags::Boss) != 0))
        return false;

    if (mZombieType == ZombieType::ZOMBIE_BUNGEE)
    {
        if (!IsOnBoard() || mHitUmbrella)
        {
            return false;
        }
    }

    if (mZombieHeight == ZombieHeight::HEIGHT_DRAGGED_UNDER ||
        mZombieHeight == ZombieHeight::HEIGHT_IN_TO_CHIMNEY ||
        mZombieHeight == ZombieHeight::HEIGHT_GETTING_BUNGEE_DROPPED)
        return false;

    if (mInPool)
        return false;

    // 1073 excludes swimming aquarium actors before building shadow draw items.
    // UI/cutscene preview actors still follow the normal shadow rules.
    if (IsOnBoard() && mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM)
        return false;

    return mApp->mGameMode != GameMode::GAMEMODE_MINIGAME_INVISIGHOUL || mFromWave == Zombie::ZOMBIE_WAVE_UI;
}

namespace
{
// 胜利剪裁表（手工生成；0 = 无 case 背景不剪裁=原 default；-220 行高度用
// 187（ROOF/BOSS），其余 BOARD_HEIGHT）
constexpr int kZombiesWonClip[7] = {
    -123,  // 0 BACKGROUND_1_DAY
    -123,  // 1 BACKGROUND_2_NIGHT
    -172,  // 2 BACKGROUND_3_POOL
    -172,  // 3 BACKGROUND_4_FOG
    -220,  // 4 BACKGROUND_5_ROOF
    -220,  // 5 BACKGROUND_6_BOSS
    0,     // 6 MUSHROOM_GARDEN（原 default）
};
static_assert(sizeof(kZombiesWonClip) / sizeof(kZombiesWonClip[0]) == 7 &&
    kZombiesWonClip[0] == -123 && kZombiesWonClip[5] == -220, "胜利剪裁表锚点");
}  // namespace

//0x534040
bool Zombie::SetupDrawZombieWon(Graphics* g)
{
    if (mFromWave != Zombie::ZOMBIE_WAVE_WINNER)
        return true;

    if (!mBoard->mCutScene->ShowZombieWalking())
        return false;

    // B 形态 -> 常量表（下方 kZombiesWonClip，BackgroundType 直下标 0..6；
    // 无 case 背景 = 0 不剪裁 = 原 default）
    if (static_cast<unsigned>(mBoard->mBackground) < 7 && kZombiesWonClip[(int)mBoard->mBackground] != 0)
    {
        const int aClipX = kZombiesWonClip[(int)mBoard->mBackground];
        g->ClipRect(aClipX - (mX), -(mY), BOARD_WIDTH, aClipX == -220 ? 187 : BOARD_HEIGHT);
    }

    return true;
}

//0x5340C0
void Zombie::DrawShadow(Graphics* g)
{
    ZombieDrawPosition aDrawPos;
    GetDrawPos(aDrawPos);
    if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON && !SetupDrawZombieWon(g))
        return;

    int aShadowType = 0;
    float aShadowOffsetX = aDrawPos.mImageOffsetX;
    float aShadowOffsetY = aDrawPos.mImageOffsetY + aDrawPos.mBodyY;
    float aScale = mScaleZombie;
    aShadowOffsetX += mScaleZombie * 20.0f - 20.0f;
    if (IsOnBoard() && mBoard->StageIsNight())
    {
        aShadowType = 1;
    }

    // B 形态 -> 常量表 kZombieShadowRows（原 11 支链合并；行序 = ZombieType
    // 枚举序 0..33 = NUM_ZOMBIE_TYPES；GARGANTUAR/REDEYE 共体同值填两槽；
    // mScaleMul = 0 槽 = 无行 → 原 reanim 二分 fallback；FOOTBALL 支含
    // mScaleZombie 公式，特例表外）。
    static constexpr struct { float mFwdX; float mBackX; float mOffY; float mScaleMul; } kZombieShadowRows[(int)ZombieType::NUM_ZOMBIE_TYPES] = {
        { 0, 0, 0, 0 },        // 0 ZOMBIE_NORMAL（无行）
        { 0, 0, 0, 0 },        // 1 ZOMBIE_FLAG（无行）
        { 0, 0, 0, 0 },        // 2 ZOMBIE_TRAFFIC_CONE（无行）
        { 36.0f, -5.0f, 11.0f, 1.0f },   // 3 ZOMBIE_POLEVAULTER
        { 0, 0, 0, 0 },        // 4 ZOMBIE_PAIL（无行）
        { 29.0f, 5.0f, 0.0f, 1.0f },     // 5 ZOMBIE_NEWSPAPER
        { 0, 0, 0, 0 },        // 6 ZOMBIE_DOOR（无行）
        { 0, 0, 0, 0 },        // 7 ZOMBIE_FOOTBALL（特例表外，槽位不可达）
        { 0, 0, 0, 0 },        // 8 ZOMBIE_DANCER（无行）
        { 0, 0, 0, 0 },        // 9 ZOMBIE_BACKUP_DANCER（无行）
        { 0, 0, 0, 0 },        // 10 ZOMBIE_DUCKY_TUBE（无行）
        { 35.0f, -2.0f, 5.0f, 1.0f },    // 11 ZOMBIE_SNORKEL
        { 0, 0, 0, 0 },        // 12 ZOMBIE_ZAMBONI（无行）
        { 20.0f, 13.0f, 13.0f, 1.0f },   // 13 ZOMBIE_BOBSLED
        { 19.0f, 15.0f, 11.0f, 1.0f },   // 14 ZOMBIE_DOLPHIN_RIDER
        { 0, 0, 0, 0 },        // 15 ZOMBIE_JACK_IN_THE_BOX（无行）
        { 0, 0, 0, 0 },        // 16 ZOMBIE_BALLOON（无行）
        { 17.0f, 14.0f, 5.0f, 1.0f },    // 17 ZOMBIE_DIGGER
        { 0, 0, 0, 0 },        // 18 ZOMBIE_POGO（无行）
        { 3.0f, 20.0f, 20.0f, 1.0f },    // 19 ZOMBIE_YETI（原链前进 +3、后退 +20）
        { 0, 0, 0, 0 },        // 20 ZOMBIE_BUNGEE（无行）
        { 0, 0, 0, 0 },        // 21 ZOMBIE_LADDER（无行）
        { 0, 0, 0, 0 },        // 22 ZOMBIE_CATAPULT（无行）
        { 27.0f, 27.0f, 7.0f, 1.5f },    // 23 ZOMBIE_GARGANTUAR（原链无前进/后退之分，共值）
        { 25.0f, 13.0f, 7.0f, 0.6f },    // 24 ZOMBIE_IMP
        { 0, 0, 0, 0 },        // 25 ZOMBIE_BOSS（无行）
        { 0, 0, 0, 0 },        // 26 ZOMBIE_PEA_HEAD（无行）
        { 0, 0, 0, 0 },        // 27 ZOMBIE_WALLNUT_HEAD（无行）
        { 0, 0, 0, 0 },        // 28 ZOMBIE_JALAPENO_HEAD（无行）
        { 0, 0, 0, 0 },        // 29 ZOMBIE_GATLING_HEAD（无行）
        { 0, 0, 0, 0 },        // 30 ZOMBIE_SQUASH_HEAD（无行）
        { 0, 0, 0, 0 },        // 31 ZOMBIE_TALLNUT_HEAD（无行）
        { 27.0f, 27.0f, 7.0f, 1.5f },    // 32 ZOMBIE_REDEYE_GARGANTUAR（共体）
        { 0, 0, 0, 0 },        // 33 ZOMBIE_ZOMBATAR（无行）
    };

    if (mZombieType == ZombieType::ZOMBIE_FOOTBALL)
    {
        if (IsWalkingBackwards())
        {
            aShadowOffsetX -= 11.0f * mScaleZombie;
        }
        else
        {
            aShadowOffsetX += 20.0f + 21.0f * mScaleZombie;
        }
        aShadowOffsetY += 16.0f;
    }
    else if ((unsigned)mZombieType < sizeof(kZombieShadowRows) / sizeof(kZombieShadowRows[0]) &&
        kZombieShadowRows[(int)mZombieType].mScaleMul != 0.0f)
    {
        const auto& aRow = kZombieShadowRows[(int)mZombieType];
        aShadowOffsetX += IsWalkingBackwards() ? aRow.mBackX : aRow.mFwdX;
        aShadowOffsetY += aRow.mOffY;
        aScale *= aRow.mScaleMul;
    }
    else if (mApp->ReanimationTryToGet(mBodyReanimID) != nullptr)
    {
        if (IsWalkingBackwards())
        {
            aShadowOffsetX += 11.0f;
        }
        else
        {
            aShadowOffsetX += 23.0f;
        }
    }
    else
    {
        if (IsWalkingBackwards())
        {
            aShadowOffsetX -= 2.0f;
        }
        else
        {
            aShadowOffsetX += 35.f;
        }
    }

    if (mZombieType == ZombieType::ZOMBIE_NEWSPAPER)
    {
        aShadowOffsetY += 4.0f;
    }
    else if (mZombieType == ZombieType::ZOMBIE_BALLOON)
    {
        aShadowOffsetY += 13.0f;
    }
    else if (mZombieType == ZombieType::ZOMBIE_BUNGEE)
    {
        aShadowOffsetX -= 12.0f;
        aScale = TodAnimateCurveFloat(BUNGEE_ZOMBIE_HEIGHT - 1000, 100, mAltitude, 0.1f, 1.5f, Curves::Linear);
    }

    if (mZombieHeight == ZombieHeight::HEIGHT_UP_LADDER || mZombieHeight == ZombieHeight::HEIGHT_FALLING ||
        mZombiePhase == ZombiePhase::PHASE_IMP_GETTING_THROWN || mZombieType == ZombieType::ZOMBIE_BUNGEE || IsBouncingPogo() || IsFlying())
    {
        aShadowOffsetY += mAltitude;
        if (mOnHighGround)
        {
            aShadowOffsetY -= HIGH_GROUND_HEIGHT;
        }
    }

    if (mInPool)
    {
        TodDrawImageCenterScaledF(g, IMAGE_WHITEWATER_SHADOW, aShadowOffsetX, aShadowOffsetY + 67.0f, aScale, aScale);
    }
    else
    {
        TodDrawImageCenterScaledF(g, aShadowType == 0 ? IMAGE_PLANTSHADOW : IMAGE_PLANTSHADOW2, aShadowOffsetX, aShadowOffsetY + 92.0f, aScale, aScale);
    }

    g->ClearClipRect();
}

//0x5345F0
// GOTY @Patoke: 0x54505E
void Zombie::GetTrackPosition(ReanimTrackId theTrackId, float& thePosX, float& thePosY)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (aBodyReanim == nullptr)
    {
        thePosX = (mX);
        thePosY = (mY);
        return;
    }

    int aTrackIndex = aBodyReanim->FindTrackIndex(theTrackId);
    SexyTransform2D aMatrix;
    aBodyReanim->GetTrackMatrix(aTrackIndex, aMatrix);
    thePosX = aMatrix.m02 + (mX);
    thePosY = aMatrix.m12 + (mY);
}

//0x534680
bool Zombie::IsFlying()
{
    // 2026-10-04 基准否决 CanFly 位前置过滤（tools/bench_phase_query.cpp，
    // MSVC x64 /O2 N=2M min-of-5 三轮稳定）：比较链被编译器并为区间判定
    // （p==60||61 → sub/cmp/setbe 三指令），标志字查表反多一次依赖加载
    //（0.89 → 1.17ns，+32%，超 10% 回退线）。整数相位比较已是最优形态。
    return mZombiePhase == ZombiePhase::PHASE_BALLOON_FLYING || mZombiePhase == ZombiePhase::PHASE_BALLOON_POPPING;
}

//0x5346A0
int Zombie::GetBobsledPosition()
{
    if (mZombieType != ZombieType::ZOMBIE_BOBSLED)
    {
        return -1;
    }

    if (mRelatedZombieID == ZombieID::ZOMBIEID_NULL && mFollowerZombieID[0] == ZombieID::ZOMBIEID_NULL)
    {
        return -1;
    }

    if (mRelatedZombieID == ZombieID::ZOMBIEID_NULL)
    {
        return 0;
    }

    ZombieID anId = mBoard->ZombieGetID(this);
    Zombie* aLeaderZombie = mBoard->ZombieGet(mRelatedZombieID);
    for (int i = 0; i < NUM_BOBSLED_FOLLOWERS; i++)
    {
        if (aLeaderZombie->mFollowerZombieID[i] == anId)
        {
            return i + 1;
        }
    }

    TOD_ASSERT(false);

	return -1;
}

bool Zombie::IsBobsledTeamWithSled()
{
    return GetBobsledPosition() != -1;
}

//0x534700
bool Zombie::IsDeadOrDying()
{
    return
        mDead ||
        mZombiePhase == ZombiePhase::PHASE_ZOMBIE_DYING ||
        mZombiePhase == ZombiePhase::PHASE_ZOMBIE_BURNED ||
        mZombiePhase == ZombiePhase::PHASE_ZOMBIE_MOWERED;
}

//0x534730
void Zombie::UpdateZombieChimney()
{
    if (mBoard->mBackground == BackgroundType::BACKGROUND_5_ROOF || mBoard->mBackground == BackgroundType::BACKGROUND_6_BOSS)
    {
        mAltitude = TodAnimateCurveNs(40'000'000'000LL, 50'000'000'000LL, mBoard->mCutScene->mCutsceneTime.mNanoseconds, 200, 0, Curves::EaseIn);
    }
}

//0x534780
void Zombie::WalkIntoHouse(std::int64_t ownerNowNs)
{
    AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_ZAMBONI_SMOKE);
    mFromWave = Zombie::ZOMBIE_WAVE_WINNER;
    ReanimReenableClipping();

    if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT)
    {
        mZombiePhase = ZombiePhase::PHASE_POLEVAULTER_POST_VAULT;
        StartWalkAnim(ownerNowNs, 0);
    }

    if (mBoard->mBackground == BackgroundType::BACKGROUND_1_DAY || mBoard->mBackground == BackgroundType::BACKGROUND_2_NIGHT ||
        mBoard->mBackground == BackgroundType::BACKGROUND_3_POOL || mBoard->mBackground == BackgroundType::BACKGROUND_4_FOG)
    {
        mY = (290.0f);
        mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_ZOMBIE, 2, 0);

        if (mZombieType == ZombieType::ZOMBIE_GARGANTUAR || mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
        {
            mY += (30.0f);
        }
        else if (mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT)
        {
            { mX += (35.0f); mRootBindingGeneration = 0; }
        }
        else if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
        {
            mY += (15.0f);
        }

        if (mBoard->StageHasPool())
        {
            if (mZombieType == ZombieType::ZOMBIE_FOOTBALL)
            {
                { mX -= (10.0f); mRootBindingGeneration = 0; }
            }
            else
            {
                { mX -= (80.0f); mRootBindingGeneration = 0; }
            }
        }
    }
    else if (mBoard->mBackground == BackgroundType::BACKGROUND_5_ROOF || mBoard->mBackground == BackgroundType::BACKGROUND_6_BOSS)
    {
        { mX = (-180.0f); mRootBindingGeneration = 0; }
        mY = (250.0f);
        mZombieHeight = ZombieHeight::HEIGHT_IN_TO_CHIMNEY;
        mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, 0, 2);

        if (mZombieType == ZombieType::ZOMBIE_GARGANTUAR || mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
        {
            mY += (5.0f);
        }
        else if (mZombieType == ZombieType::ZOMBIE_FOOTBALL)
        {
            { mX -= (14.0f); mRootBindingGeneration = 0; }
        }
        else if (mZombieType == ZombieType::ZOMBIE_ZAMBONI)
        {
            { mX -= (28.0f); mRootBindingGeneration = 0; }
        }

        Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
        if (aBodyReanim && aBodyReanim->TrackExists(ReanimTrackId::anim_idle) && mZombieType != ZombieType::ZOMBIE_POLEVAULTER)
        {
            PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 0, 15.0f);
        }
    }
}

//0x534920
void Zombie::BossPlayIdle(std::int64_t ownerNowNs)
{
    mZombiePhase = ZombiePhase::PHASE_BOSS_IDLE;
    mPhaseCounter = RandRangeInt(100, 200);
    PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 0, 6.0f);
}

//0x534960
void Zombie::DrawBossFireBall(Graphics* g)
{
    MakeParentGraphicsFrame(g);

    Reanimation* aFireBallReanim = mApp->ReanimationTryToGet(mBossFireBallReanimID);
    if (aFireBallReanim)
    {
        aFireBallReanim->DrawRenderGroup(g, RENDER_GROUP_NORMAL);

        g->mDrawMode = Graphics::DRAWMODE_ADDITIVE;
        aFireBallReanim->DrawRenderGroup(g, RENDER_GROUP_BOSS_FIREBALL_ADDITIVE);

        g->mDrawMode = Graphics::DRAWMODE_NORMAL;
        aFireBallReanim->DrawRenderGroup(g, RENDER_GROUP_BOSS_FIREBALL_TOP);
    }
}

//0x5349E0
void Zombie::DrawBossBackArm(std::int64_t ownerNowNs, Graphics* g, const ZombieDrawPosition& theDrawPos)
{
    float aImageOffsetX = 0.0f;
    float aImageOffsetY = 0.0f;
    if (mZombiePhase == PHASE_BOSS_DROP_RV)
    {
        aImageOffsetY = (mTargetRow - 1) * 85.0f - mTargetCol * 20.0f;
        aImageOffsetX = mTargetCol * 80.0f;
    }
    else if (mZombiePhase == PHASE_BOSS_BUNGEES_ENTER || mZombiePhase == PHASE_BOSS_BUNGEES_DROP || mZombiePhase == PHASE_BOSS_BUNGEES_LEAVE)
    {
        aImageOffsetX = mTargetCol * 80.0f - 23.0f;
    }

    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    const float aOriginalX = aBodyReanim->mOverlayMatrix.m02;
    const float aOriginalY = aBodyReanim->mOverlayMatrix.m12;
    aBodyReanim->mOverlayMatrix.m02 = aOriginalX + aImageOffsetX;
    aBodyReanim->mOverlayMatrix.m12 = aOriginalY + aImageOffsetY;
    DrawReanim(ownerNowNs, g, theDrawPos, RENDER_GROUP_BOSS_BACK_ARM);
    // Other render groups share this matrix. Subtraction cannot recover the
    // fractional translation lost while adding an attack offset.
    aBodyReanim->mOverlayMatrix.m02 = aOriginalX;
    aBodyReanim->mOverlayMatrix.m12 = aOriginalY;
}

//0x534AC0
void Zombie::BossRVAttack(std::int64_t ownerNowNs)
{
    RemoveColdEffects(ownerNowNs);
    mZombiePhase = ZombiePhase::PHASE_BOSS_DROP_RV;
#ifdef DO_FIX_BUGS
    mTargetRow = RandRangeInt(0, mBoard->StageHas6Rows() ? 4 : 3);  // 泳池僵王兼容
#else
    mTargetRow = RandRangeInt(0, 3);
#endif
    mTargetCol = RandRangeInt(0, 2);

    PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_RV_1, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 16.0f);
    mApp->PlayFoley(FoleyType::FOLEY_HYDRAULIC_SHORT);
}

//0x534B90
void Zombie::BossRVLanding(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    Plant* aPlant = nullptr;
    while (mBoard->IteratePlants(aPlant))
    {
        if (aPlant->mRow >= mTargetRow && aPlant->mRow <= mTargetRow + 1 && aPlant->mPlantCol >= mTargetCol && aPlant->mPlantCol <= mTargetCol + 2)
        {
            aPlant->Squish(ownerNowNs, visualNowNs, animationNowNs);
        }
    }

    mBoard->mShakeCounter = 12;
    mBoard->mShakeAmountX = 1;
    mBoard->mShakeAmountY = 2;
    mApp->PlaySample(SOUND_RVTHROW);

    mSummonCounter = 500;
    mBossHeadCounter = 5000;
    if (mBossMode >= 1)
    {
        mBossStompCounter = 4000;
    }
    if (mBossMode >= 2)
    {
        mBossBungeeCounter = 6500;
    }
}

namespace
{
// Boss 出招轨道名表（手工生成，5-case 整型直下标；static_assert 锚
// ReanimTrackId 序数；default 分支保留表外——DO_FIX_BUGS 语义见原处）。
constexpr ReanimTrackId kBossSpawnTracks[5] = {
    ReanimTrackId::anim_spawn_1,
    ReanimTrackId::anim_spawn_2,
    ReanimTrackId::anim_spawn_3,
    ReanimTrackId::anim_spawn_4,
    ReanimTrackId::anim_spawn_5,
};
static_assert(kBossSpawnTracks[0] == ReanimTrackId::anim_spawn_1 &&
    kBossSpawnTracks[4] == ReanimTrackId::anim_spawn_5, "spawn 轨道表锚点");
constexpr ReanimTrackId kBossHeadTracks[5] = {
    ReanimTrackId::anim_head_attack_1,
    ReanimTrackId::anim_head_attack_2,
    ReanimTrackId::anim_head_attack_3,
    ReanimTrackId::anim_head_attack_4,
    ReanimTrackId::anim_head_attack_5,
};
static_assert(kBossHeadTracks[0] == ReanimTrackId::anim_head_attack_1 &&
    kBossHeadTracks[4] == ReanimTrackId::anim_head_attack_5, "head 轨道表锚点");
}  // namespace

//0x534C70
void Zombie::BossSpawnAttack(std::int64_t ownerNowNs)
{
    RemoveColdEffects(ownerNowNs);
    mZombiePhase = ZombiePhase::PHASE_BOSS_SPAWNING;
    if (mBossMode == 0)
    {
        mSummonCounter = RandRangeInt(450, 550);
    }
    else if (mBossMode == 1)
    {
        mSummonCounter = RandRangeInt(350, 450);
    }
    else if (mBossMode == 2)
    {
        mSummonCounter = RandRangeInt(150, 250);
    }

    mTargetRow = mBoard->PickRowForNewZombie(ZombieType::ZOMBIE_NORMAL);

    // B 形态 -> 常量表 kBossSpawnTracks[5]（见函数前 namespace）：整型直下标；
    // default（DO_FIX_BUGS 回退/断言）保留表外。
    ReanimTrackId aTrackName{};  // 显式零初始化（clang -Wsometimes-uninitialized：else 分支 TOD_ASSERT(false) 后不可达但编译器仍告警）
    if (static_cast<unsigned>(mTargetRow) < 5)
    {
        aTrackName = kBossSpawnTracks[mTargetRow];
    }
    else
    {
#ifdef DO_FIX_BUGS
        aTrackName = ReanimTrackId::anim_spawn_5;  // 泳池场景放僵尸崩溃的一种妥协的修复方式（不修改动画时）
#else
        TOD_ASSERT(false);
#endif
    }
    PlayZombieReanim(ownerNowNs, aTrackName, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
    mApp->PlayFoley(FoleyType::FOLEY_HYDRAULIC_SHORT);
}

//0x534DC0
void Zombie::BossSpawnContact(std::int64_t ownerNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    ZombieType aZombieType;
    if (aStatusNowNs-mBirthTimeNs < 35000000000LL)
    {
        aZombieType = ZombieType::ZOMBIE_NORMAL;
    }
    else if (aStatusNowNs-mBirthTimeNs < 80000000000LL)
    {
        aZombieType = ZombieType::ZOMBIE_TRAFFIC_CONE;
    }
    else if (aStatusNowNs-mBirthTimeNs < 125000000000LL)
    {
        aZombieType = ZombieType::ZOMBIE_PAIL;
    }
    else
    {
        int aZombieTypeCount = LENGTH(gBossZombieList);
        if (mTargetRow == 0)
        {
            TOD_ASSERT(gBossZombieList[aZombieTypeCount - 1] == ZombieType::ZOMBIE_GARGANTUAR);
            aZombieTypeCount--;
        }

        aZombieType = TodPickFromArray(gBossZombieList, aZombieTypeCount);
    }

    Zombie* aZombie = mBoard->AddZombieInRow(aZombieType, mTargetRow, 0);
    { aZombie->mX = (600.0f); aZombie->mRootBindingGeneration = 0; }
}

//0x534E30
void Zombie::BossStompAttack(std::int64_t ownerNowNs)
{
    RemoveColdEffects(ownerNowNs);
    mZombiePhase = ZombiePhase::PHASE_BOSS_STOMPING;
    mBossStompCounter = RandRangeInt(5500, 6500);

    int aRowsCount = 0;
    int aRowArray[4];
    for (int i = 0; i < 4; i++)
    {
        if (BossCanStompRow(i))
        {
            aRowArray[aRowsCount] = i;
            aRowsCount++;
        }
    }

    if (aRowsCount == 0)
        return;

    mTargetRow = TodPickFromArray(aRowArray, aRowsCount);

    // 踩踏行->轨道直下标表（段表化战役 2026-10-07）：mTargetRow 源自
    // TodPickFromArray 于 BossCanStompRow 行号集（i=0..3），值域封闭。
    // 原 default: TOD_ASSERT 纯防御；release 下原为未初始化 aTrackName
    // 传入（UB），表化后越界守卫安全跳过踩踏动画——foley 播放位置在
    // 表外逐字保留（原 switch 后无条件执行）。
    static constexpr ReanimTrackId kBossStompTracks[4] = {
        ReanimTrackId::anim_stomp_1,  // 0
        ReanimTrackId::anim_stomp_2,  // 1
        ReanimTrackId::anim_stomp_3,  // 2
        ReanimTrackId::anim_stomp_4,  // 3
    };
    static_assert(sizeof(kBossStompTracks) / sizeof(kBossStompTracks[0]) == 4,
        "踩踏轨道槽位须与目标行值域同步");
    if ((unsigned)mTargetRow < 4)
    {
        PlayZombieReanim(ownerNowNs, kBossStompTracks[mTargetRow], ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
    }
    mApp->PlayFoley(FoleyType::FOLEY_HYDRAULIC_SHORT);
}

//0x534F60
bool Zombie::BossCanStompRow(int theRow)
{
    // 格桶链枚举（2026-10-03 第二批）：原为全池扫两行带过滤
    // （mRow∈[theRow,theRow+1] && mPlantCol>=5 && !NotOnGround）任意
    // 命中即真——行过滤改 theRow..theRow+1 各 9 格×4 层链全成员（含
    // 影子株=原池可见集），!NotOnGround 与 mPlantCol>=5 逐字保留。
    // mPlantCol>=5 必须逐株复检：玉米炮锚列 4 经右列链进入行枚举，
    // 原池扫按裸 mPlantCol 不认它。布尔结果，次序无关。
    for (int aRow = theRow; aRow <= theRow + 1; aRow++)
    {
        for (int aCol = 0; aCol < MAX_GRID_SIZE_X; aCol++)
        {
            for (int aLayer = 0; aLayer < PLANT_CELL_COUNT; aLayer++)
            {
                Plant* aPlant = mBoard->GetCellPlant(aRow, aCol, aLayer);
                while (aPlant != nullptr)
                {
                    if (!aPlant->NotOnGround() && aPlant->mPlantCol >= 5)
                    {
                        return true;
                    }
                    aPlant = mBoard->NextCellPlant(aPlant);
                }
            }
        }
    }
    return false;
}

//0x534FF0
void Zombie::BossStompContact(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    Plant* aPlant = nullptr;
    while (mBoard->IteratePlants(aPlant))
    {
        if (aPlant->mRow >= mTargetRow && aPlant->mRow <= mTargetRow + 1 && aPlant->mPlantCol >= 5)
        {
            aPlant->Squish(ownerNowNs, visualNowNs, animationNowNs);
        }
    }

    mBoard->mShakeCounter = 12;
    mBoard->mShakeAmountX = 1;
    mBoard->mShakeAmountY = 4;
    mApp->PlayFoley(FoleyType::FOLEY_THUMP);
}

//0x5350C0
void Zombie::BossBungeeAttack(std::int64_t ownerNowNs)
{
    RemoveColdEffects(ownerNowNs);
    mZombiePhase = ZombiePhase::PHASE_BOSS_BUNGEES_ENTER;
    mBossBungeeCounter = RandRangeInt(4000, 5000);
    mTargetCol = RandRangeInt(0, 2);

    PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_bungee_1_enter, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
    mApp->PlayFoley(FoleyType::FOLEY_HYDRAULIC_SHORT);
    mApp->PlayFoley(FoleyType::FOLEY_BUNGEE_SCREAM);
}

//0x5351F0
void Zombie::BossBungeeSpawn(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    mZombiePhase = ZombiePhase::PHASE_BOSS_BUNGEES_DROP;

    for (int i = 0; i < NUM_BOSS_BUNGEES; i++)
    {
        Zombie* aZombie = mBoard->AddZombieInRow(ZombieType::ZOMBIE_BUNGEE, 0, 0);
        aZombie->PickBungeeZombieTarget(ownerNowNs, visualNowNs, animationNowNs, mTargetCol + i);
        aZombie->mAltitude = (aZombie->mY) - 30.0f;
        mFollowerZombieID[i] = mBoard->ZombieGetID(aZombie);
    }
}

//0x5352B0
void Zombie::BossBungeeLeave(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    mZombiePhase = ZombiePhase::PHASE_BOSS_BUNGEES_LEAVE;

    for (int i = 0; i < NUM_BOSS_BUNGEES; i++)
    {
        Zombie* aZombie = mBoard->ZombieTryToGet(mFollowerZombieID[i]);
        if (aZombie && aZombie->mButteredEndTimeNs > aStatusNowNs)
        {
            aZombie->DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
        }
    }

    PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_bungee_1_leave, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 18.0f);
}

//0x535340
bool Zombie::BossAreBungeesDone()
{
    int aBungeesRemaining = 0;
    for (int i = 0; i < NUM_BOSS_BUNGEES; i++)
    {
        Zombie* aZombie = mBoard->ZombieTryToGet(mFollowerZombieID[i]);
        if (aZombie)
        {
            if (aZombie->mZombiePhase == ZombiePhase::PHASE_BUNGEE_RISING)
            {
                return true;
            }
            aBungeesRemaining++;
        }
    }

    return aBungeesRemaining == 0;
}

//0x5353A0
void Zombie::BossHeadAttack(std::int64_t ownerNowNs)
{
    mZombiePhase = ZombiePhase::PHASE_BOSS_HEAD_ENTER;
    mBossHeadCounter = RandRangeInt(4000, 5000);

    PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_head_enter, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
    mApp->PlayFoley(FoleyType::FOLEY_HYDRAULIC_SHORT);
}

//0x535440
void Zombie::BossHeadSpit(std::int64_t ownerNowNs)
{
    Reanimation* aFireBallReanim = mApp->ReanimationTryToGet(mBossFireBallReanimID);
    if (aFireBallReanim)
    {
        aFireBallReanim->ReanimationDie();
        mBossFireBallReanimID = ReanimationID::REANIMATIONID_NULL;
    }

    mZombiePhase = ZombiePhase::PHASE_BOSS_HEAD_SPIT;
#ifdef DO_FIX_BUGS
    mFireballRow = RandRangeInt(0, mBoard->StageHas6Rows() ? 5 : 4);  // 泳池僵王兼容
#else
    mFireballRow = RandRangeInt(0, 4);
#endif
    mIsFireBall = RandRangeInt(0, 1) == 0;

    // B 形态 -> 常量表 kBossHeadTracks[5]（见函数前 namespace）：整型直下标；
    // default（DO_FIX_BUGS 回退/断言）保留表外。
    ReanimTrackId aTrackName{};  // 显式零初始化（clang -Wsometimes-uninitialized：else 分支 TOD_ASSERT(false) 后不可达但编译器仍告警）
    if (static_cast<unsigned>(mFireballRow) < 5)
    {
        aTrackName = kBossHeadTracks[mFireballRow];
    }
    else
    {
#ifdef DO_FIX_BUGS
        aTrackName = ReanimTrackId::anim_head_attack_5;  // 泳池场景吐球的一种妥协的修复方式（不修改动画时）
#else
        TOD_ASSERT(false);
#endif
    }
    PlayZombieReanim(ownerNowNs, aTrackName, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);

    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    if (mIsFireBall)
    {
        aBodyReanim->SetVariantOption(ReanimTrackId::Boss_eyeglow_red, 0);
        aBodyReanim->SetVariantOption(ReanimTrackId::Boss_mouthglow_red, 0);
    }
    else
    {
        aBodyReanim->SetVariantOption(ReanimTrackId::Boss_eyeglow_red, ReanimOptions::Zombie_boss::Boss_eyeglow_red::ZOMBIE_BOSS_EYEGLOW_BLUE);
        aBodyReanim->SetVariantOption(ReanimTrackId::Boss_mouthglow_red, ReanimOptions::Zombie_boss::Boss_mouthglow_red::ZOMBIE_BOSS_MOUTHGLOW_BLUE);
    }

    Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mSpecialHeadReanimID);
    aHeadReanim->PlayReanim(ReanimTrackId::anim_drive, ReanimLoopType::REANIM_LOOP, 20, 36.0f);
}

//0x535630
void Zombie::BossDestroyIceballInRow(std::int64_t ownerNowNs)
{
    //if (theRow != mFireballRow)  // 此判断在原版中被移动至调用处进行，故参数的 theRow 亦被删去
    //    return;

    Reanimation* aFireBallReanim = mApp->ReanimationTryToGet(mBossFireBallReanimID);
    if (aFireBallReanim && !mIsFireBall)
    {
        float aPosX = aFireBallReanim->mOverlayMatrix.m02 + 80.0f;
        float aPosY = aFireBallReanim->mOverlayMatrix.m12 + 80.0f;
        mApp->AddTodParticle(aPosX, aPosY, 400000, ParticleEffect::PARTICLE_ICEBALL_DEATH, ownerNowNs);

        aFireBallReanim->ReanimationDie();
        mBossFireBallReanimID = ReanimationID::REANIMATIONID_NULL;
        mBoard->RemoveParticleByType(ParticleEffect::PARTICLE_ICEBALL_TRAIL);
    }
}

//0x5356D0
void Zombie::BossDestroyFireball(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    Reanimation* aFireBallReanim = mApp->ReanimationTryToGet(mBossFireBallReanimID);
    if (aFireBallReanim && mIsFireBall)
    {
        float aPosX = aFireBallReanim->mOverlayMatrix.m02 + 80.0f;
        float aPosY = aFireBallReanim->mOverlayMatrix.m12 + 40.0f;
        for (int i = 0; i < 6; i++)
        {
            float aAngle = 2 * PI * i / 6 + PI / 2;
            Reanimation* aReanim = mApp->AddReanimation(aPosX + 60.0f * sin(aAngle), aPosY + 60.0f * cos(aAngle), 400000, ReanimationType::REANIM_JALAPENO_FIRE, visualNowNs);
            aReanim->SeekRatio(0.2f);

            aReanim->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME);

            aReanim->ApplyRate(RandRangeFloat(20.0f, 25.0f));
        }

        aFireBallReanim->ReanimationDie();
        mBossFireBallReanimID = ReanimationID::REANIMATIONID_NULL;
        mBoard->RemoveParticleByType(ParticleEffect::PARTICLE_FIREBALL_TRAIL);
    }
}

//0x535870
void Zombie::BossHeadSpitEffect(std::int64_t ownerNowNs)
{
    if (mIsFireBall)
    {
        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        int aTrackIndex = aBodyReanim->FindTrackIndex(ReanimTrackId::Boss_jaw);
        ReanimatorTransform aTransform;
        aBodyReanim->GetCurrentTransform(aTrackIndex, &aTransform);
        float aFlamePosX = (mX) + aTransform.mTransX + 100.0f;
        float aFlamePosY = (mY) + aTransform.mTransY + 50.0f;
        mApp->AddTodParticle(aFlamePosX, aFlamePosY, mRenderOrder + 2, ParticleEffect::PARTICLE_ZOMBIE_BOSS_FIREBALL, ownerNowNs);
    }
    else
    {
        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
        int aTrackIndex = aBodyReanim->FindTrackIndex(ReanimTrackId::Boss_jaw);
        ReanimatorTransform aTransform;
        aBodyReanim->GetCurrentTransform(aTrackIndex, &aTransform);
        float aFlamePosX = (mX) + aTransform.mTransX + 100.0f;
        float aFlamePosY = (mY) + aTransform.mTransY + 50.0f;
        mApp->AddTodParticle(aFlamePosX, aFlamePosY, mRenderOrder + 2, ParticleEffect::PARTICLE_ZOMBIE_BOSS_FIREBALL, ownerNowNs,
            static_cast<std::uint32_t>(ParticleVariants::ZombieBossFireball::ZombieBossIceballBurst));
    }

    mApp->PlayFoley(FoleyType::FOLEY_BOSS_BOULDER_ATTACK);
}

//0x535A70
void Zombie::BossHeadSpitContact(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    TOD_ASSERT(!mApp->ReanimationTryToGet(mBossFireBallReanimID));

    float aPosY = mBoard->GetPosYBasedOnRow(550.0f, mFireballRow) - 90.0f;
    Reanimation* aFireBallReanim;
    if (mIsFireBall)
    {
        aFireBallReanim = mApp->AddReanimation(455.0f, aPosY, mRenderOrder + 1, ReanimationType::REANIM_BOSS_FIREBALL, animationNowNs);

        aFireBallReanim->PlayReanim(ReanimTrackId::anim_form, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 16.0f);
        aFireBallReanim->mIsAttachment = true;
        aFireBallReanim->AssignRenderGroupToTrack(ReanimTrackId::additive, RENDER_GROUP_BOSS_FIREBALL_ADDITIVE);
        aFireBallReanim->AssignRenderGroupToTrack(ReanimTrackId::superglow, RENDER_GROUP_BOSS_FIREBALL_ADDITIVE);
    }
    else
    {
        aFireBallReanim = mApp->AddReanimation(455.0f, aPosY, mRenderOrder + 1, ReanimationType::REANIM_BOSS_ICEBALL, animationNowNs);

        aFireBallReanim->PlayReanim(ReanimTrackId::anim_form, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 16.0f);
        aFireBallReanim->mIsAttachment = true;
        aFireBallReanim->AssignRenderGroupToTrack(ReanimTrackId::ice_highlight, RENDER_GROUP_BOSS_FIREBALL_ADDITIVE);
    }

    mBossFireBallReanimID = mApp->ReanimationGetID(aFireBallReanim);
    mRootBindingGeneration = 0;
    mApp->ReanimationTryToGet(mSpecialHeadReanimID)->PlayReanim(ReanimTrackId::anim_laugh, ReanimLoopType::REANIM_LOOP, 20, 18.0f);
    mApp->PlayFoley(FoleyType::FOLEY_HYDRAULIC_SHORT);
}

//0x535C60
void Zombie::UpdateBossFireball(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    Reanimation* aFireballReanim = mApp->ReanimationTryToGet(mBossFireBallReanimID);
    if (aFireballReanim == nullptr)
        return;

    // Bosses do not use walking motion, so their compact root binding belongs
    // to the active fireball while it is alive.
    if (!mRootBindingGeneration || mRootBindingGeneration != aFireballReanim->mPlaybackGeneration || mRootScale != -1.0f)
    {
        mRootOriginX = aFireballReanim->mOverlayMatrix.m02 -
            aFireballReanim->RootMotion(ReanimTrackId::_ground, -1.0f);
        mRootBindingGeneration = aFireballReanim->mPlaybackGeneration;
        mRootScale = -1.0f;
    }
    float aPosX = CheckedPosition(mRootOriginX +
        aFireballReanim->RootMotion(ReanimTrackId::_ground, -1.0f));
    aFireballReanim->mOverlayMatrix.m02 = aPosX;
    float aPosY = mBoard->GetPosYBasedOnRow(aPosX + 75.0f, mFireballRow) - 90.0f;
    aFireballReanim->mOverlayMatrix.m12 = aPosY;

    if (aPosX < -180.0f)
    {
        aFireballReanim->ReanimationDie();
        mBossFireBallReanimID = ReanimationID::REANIMATIONID_NULL;
    }

    SquishAllInSquare(ownerNowNs, visualNowNs, animationNowNs, mBoard->PixelToGridX(aPosX + 75, aPosY), mFireballRow, ZombieAttackType::ATTACKTYPE_DRIVE_OVER);

    LawnMower* aLawnMower = nullptr;
    while (mBoard->IterateLawnMowers(aLawnMower))
    {
        if (aLawnMower->mMowerState != LawnMowerState::MOWER_SQUISHED && aLawnMower->mRow == mFireballRow &&
            aLawnMower->mPosX > aPosX && aLawnMower->mPosX < aPosX + 50.0f)
        {
            aLawnMower->SquishMower();
        }
    }

    if (mIsFireBall)
    {
        if (aFireballReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD && aFireballReanim->mLoopCount > 0)
        {

            aFireballReanim->PlayReanim(ReanimTrackId::anim_role, ReanimLoopType::REANIM_LOOP, 0, 2.0f);
            aFireballReanim->mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mFireballRow, 0);
        }

        if (aFireballReanim->mLoopType == ReanimLoopType::REANIM_LOOP && Sexy::UpdateChance(10))
        {
            float aBallPosX = aPosX + 100.0f + RandRangeFloat(0.0f, 20.0f);
            float aBallPosY = mBoard->GetPosYBasedOnRow(aBallPosX - 40.0f, mFireballRow) + 90.0f + RandRangeFloat(-50.0f, 0.0f);
            int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, mFireballRow, 6);
            mApp->AddTodParticle(aBallPosX, aBallPosY, aRenderPosition, ParticleEffect::PARTICLE_FIREBALL_TRAIL, ownerNowNs);
        }
    }
    else
    {
        if (aFireballReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD && aFireballReanim->mLoopCount > 0)
        {

            aFireballReanim->PlayReanim(ReanimTrackId::anim_role, ReanimLoopType::REANIM_LOOP, 0, 2.0f);
            aFireballReanim->mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mFireballRow, 0);
        }

        if (aFireballReanim->mLoopType == ReanimLoopType::REANIM_LOOP && Sexy::UpdateChance(10))
        {
            float aBallPosX = aPosX + 100.0f + RandRangeFloat(0.0f, 20.0f);
            float aBallPosY = mBoard->GetPosYBasedOnRow(aBallPosX - 40.0f, mFireballRow) + 90.0f + RandRangeFloat(-50.0f, 0.0f);
            int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, mFireballRow, 6);
            mApp->AddTodParticle(aBallPosX, aBallPosY, aRenderPosition, ParticleEffect::PARTICLE_ICEBALL_TRAIL, ownerNowNs);
        }
    }

    aFireballReanim->UpdateAt(animationNowNs);
}

//0x535FB0
void Zombie::BossStartDeath(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    mZombiePhase = ZombiePhase::PHASE_BOSS_HEAD_LEAVE;
    PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_head_leave, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);

    mApp->AddTodParticle(700.0f, 150.0f, 400000, ParticleEffect::PARTICLE_BOSS_EXPLOSION, ownerNowNs);
    mApp->PlaySample(SOUND_BOSSEXPLOSION);
    mApp->PlayFoley(FoleyType::FOLEY_GARGANTUDEATH);

    BossDie(ownerNowNs, visualNowNs, animationNowNs);
}

//0x536080
void Zombie::UpdateBoss(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    const auto aStatusNowNs = ownerNowNs;

    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
    {
        if (aBodyReanim->ShouldTriggerTimedEvent(0.24f) || aBodyReanim->ShouldTriggerTimedEvent(0.79f))
        {
            mApp->PlayFoley(FoleyType::FOLEY_THUMP);
            mBoard->mShakeCounter = 12;
            mBoard->mShakeAmountX = 1;
            mBoard->mShakeAmountY = 4;
        }
        return;
    }

    Reanimation* aHeadReanim = mApp->ReanimationGet(mSpecialHeadReanimID);
    UpdateBossFireball(ownerNowNs, visualNowNs, animationNowNs);
    if (mIceTrapEndTimeNs <= aStatusNowNs)
    {
        if (mSummonCounter > 0)
        {
            mSummonCounter--;
        }
        if (mBossBungeeCounter > 0)
        {
            mBossBungeeCounter--;
        }
        if (mBossStompCounter > 0)
        {
            mBossStompCounter--;
        }
        if (mBossHeadCounter > 0)
        {
            mBossHeadCounter--;
        }

        if (mChilledEndTimeNs > aStatusNowNs)
        {

            aHeadReanim->ApplyRate(6.0f);
        }
        else if (aHeadReanim->mAnimRate == 0.0f)
        {

            aHeadReanim->ApplyRate(12.0f);
        }
    }
    else
    {

        aHeadReanim->ApplyRate(0.0f);
    }

    // C 形态 -> 成员函数内匿名 lambda 函数指针表 kBossPhaseCases（仿
    // kDancerPhaseCases 范式）：行序 = ZombiePhase 枚举序 78..90
    // （PHASE_BOSS_ENTER..PHASE_BOSS_LEAVE，13 相位连续）。表外相位/越界 =
    // 原 else TOD_ASSERT(false)。IDLE 相位原 early-return 在 lambda 内回归
    // （表分派即函数尾，语义等价）；HEAD_SPIT 相位自行重取头部 reanim。
    using BossPhaseCaseFn = void (*)(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie&, Reanimation*);
    static constexpr BossPhaseCaseFn kBossPhaseCases[(int)ZombiePhase::PHASE_BOSS_HEAD_LEAVE - (int)ZombiePhase::PHASE_BOSS_ENTER + 1] = {
        // 原 if PHASE_BOSS_ENTER
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z, Reanimation* )
        {
            z.BossPlayIdle(ownerNowNs);
        },  // 78 PHASE_BOSS_ENTER
        // 原 else-if PHASE_BOSS_IDLE
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, Reanimation* )
        {
            if (z.mBodyHealth == 1)
            {
                z.PlayDeathAnim(ownerNowNs, visualNowNs, animationNowNs, 0U);
                return;  // 原 UpdateBoss 早退：分派为函数尾，等价
            }

            if (z.mPhaseCounter > 0)
                return;

            int aDamageIndex = z.GetBodyDamageIndex();
            if (aDamageIndex != z.mBossMode)
            {
                z.mBossMode = aDamageIndex;
                if (z.mBossMode == 1)  // 进入一级损伤后，立即释放一次蹦极僵尸
                {
                    z.BossBungeeAttack(ownerNowNs);
                }
                else  // 进入二级损伤后，立即进行一次砸车
                {
                    z.BossRVAttack(ownerNowNs);
                }
            }
            else if (z.mBossStompCounter == 0)
            {
                z.BossStompAttack(ownerNowNs);
            }
            else if (z.mBossBungeeCounter == 0)
            {
                if (Rand(z.mApp->IsAdventureMode() ? 4 : 2) == 0)  // 1/2 概率砸车（冒险模式为 1/4 概率），否则释放蹦极僵尸
                {
                    z.mBossBungeeCounter = RandRangeInt(4000, 5000);
                    z.BossRVAttack(ownerNowNs);
                }
                else
                {
                    z.BossBungeeAttack(ownerNowNs);
                }
            }
            else if (z.mBossHeadCounter == 0)
            {
                z.BossHeadAttack(ownerNowNs);
            }
            else if (z.mSummonCounter == 0)
            {
                z.BossSpawnAttack(ownerNowNs);
            }
            else
            {
                z.mPhaseCounter = RandRangeInt(100, 200);
            }
        },  // 79 PHASE_BOSS_IDLE
        // 原 else-if PHASE_BOSS_SPAWNING
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z, Reanimation* aBodyReanim)
        {
            if (aBodyReanim->ShouldTriggerTimedEvent(0.6f))
            {
                z.BossSpawnContact(ownerNowNs);
            }

            if (aBodyReanim->mLoopCount > 0)
            {
                z.BossPlayIdle(ownerNowNs);
            }
        },  // 80 PHASE_BOSS_SPAWNING
        // 原 else-if PHASE_BOSS_STOMPING
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, Reanimation* aBodyReanim)
        {
            float aTrigger = 0.5f;
            if (z.mTargetRow >= 2)
            {
                aTrigger = 0.55f;
            }
            if (aBodyReanim->ShouldTriggerTimedEvent(aTrigger))
            {
                z.BossStompContact(ownerNowNs, visualNowNs, animationNowNs);
            }

            if (aBodyReanim->mLoopCount > 0)
            {
                z.BossPlayIdle(ownerNowNs);
            }
        },  // 81 PHASE_BOSS_STOMPING
        // 原 else-if PHASE_BOSS_BUNGEES_ENTER
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, Reanimation* aBodyReanim)
        {
            if (aBodyReanim->ShouldTriggerTimedEvent(0.4f))
            {
                z.BossBungeeSpawn(ownerNowNs, visualNowNs, animationNowNs);
            }
        },  // 82 PHASE_BOSS_BUNGEES_ENTER
        // 原 else-if PHASE_BOSS_BUNGEES_DROP
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, Reanimation* )
        {
            if (z.BossAreBungeesDone())
            {
                z.BossBungeeLeave(ownerNowNs, visualNowNs, animationNowNs);
            }
        },  // 83 PHASE_BOSS_BUNGEES_DROP
        // 原 else-if PHASE_BOSS_BUNGEES_LEAVE
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z, Reanimation* aBodyReanim)
        {
            if (aBodyReanim->mLoopCount > 0)
            {
                z.BossPlayIdle(ownerNowNs);
            }
        },  // 84 PHASE_BOSS_BUNGEES_LEAVE
        // 原 else-if PHASE_BOSS_DROP_RV
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, Reanimation* aBodyReanim)
        {
            if (aBodyReanim->ShouldTriggerTimedEvent(0.65f))
            {
                z.BossRVLanding(ownerNowNs, visualNowNs, animationNowNs);
            }

            if (aBodyReanim->mLoopCount > 0)
            {
                z.BossPlayIdle(ownerNowNs);
            }
        },  // 85 PHASE_BOSS_DROP_RV
        // 原 else-if PHASE_BOSS_HEAD_ENTER
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z, Reanimation* aBodyReanim)
        {
            if (z.GetBodyDamageIndex() == 2 && aBodyReanim->ShouldTriggerTimedEvent(0.37f))
            {
                z.ApplyBossSmokeParticles(ownerNowNs, true);
            }

            if (aBodyReanim->ShouldTriggerTimedEvent(0.55f))
            {
                z.mApp->PlayFoley(FoleyType::FOLEY_HYDRAULIC);
            }

            if (aBodyReanim->mLoopCount > 0)
            {
                z.mZombiePhase = ZombiePhase::PHASE_BOSS_HEAD_IDLE_BEFORE_SPIT;
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_head_idle, ReanimLoopType::REANIM_LOOP, 0, 12.0f);
                z.mPhaseCounter = 500;
            }
        },  // 86 PHASE_BOSS_HEAD_ENTER
        // 原 else-if PHASE_BOSS_HEAD_IDLE_BEFORE_SPIT
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, Reanimation* )
        {
            if (z.mBodyHealth == 1)
            {
                z.BossStartDeath(ownerNowNs, visualNowNs, animationNowNs);
            }
            else if (z.mPhaseCounter == 0)
            {
                z.BossHeadSpit(ownerNowNs);
            }
        },  // 87 PHASE_BOSS_HEAD_IDLE_BEFORE_SPIT
        // 原 else-if PHASE_BOSS_HEAD_IDLE_AFTER_SPIT
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, Reanimation* )
        {
            if (z.mBodyHealth == 1)
            {
                z.BossStartDeath(ownerNowNs, visualNowNs, animationNowNs);
            }
            else if (z.mPhaseCounter == 0)
            {
                z.mZombiePhase = ZombiePhase::PHASE_BOSS_HEAD_LEAVE;
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_head_leave, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 12.0f);
            }
        },  // 88 PHASE_BOSS_HEAD_IDLE_AFTER_SPIT
        // 原 else-if PHASE_BOSS_HEAD_SPIT
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie& z, Reanimation* aBodyReanim)
        {
            if (aBodyReanim->ShouldTriggerTimedEvent(0.37f))
            {
                z.BossHeadSpitEffect(ownerNowNs);
            }

            if (aBodyReanim->ShouldTriggerTimedEvent(0.42f))
            {
                z.BossHeadSpitContact(ownerNowNs, visualNowNs, animationNowNs);
            }

            if (aBodyReanim->mLoopCount > 0)
            {
                Reanimation* aHeadReanim = z.mApp->ReanimationTryToGet(z.mSpecialHeadReanimID);

                aHeadReanim->PlayReanim(ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 20, 18.0f);
                z.mZombiePhase = ZombiePhase::PHASE_BOSS_HEAD_IDLE_AFTER_SPIT;
                z.PlayZombieReanim(ownerNowNs, ReanimTrackId::anim_head_idle, ReanimLoopType::REANIM_LOOP, 0, 12.0f);
                z.mPhaseCounter = 300;
            }
        },  // 89 PHASE_BOSS_HEAD_SPIT
        // 原 else-if PHASE_BOSS_HEAD_LEAVE
        [](std::int64_t ownerNowNs, std::int64_t , std::int64_t , Zombie& z, Reanimation* aBodyReanim)
        {
            if (aBodyReanim->ShouldTriggerTimedEvent(0.23f))
            {
                z.mChilledEndTimeNs = 0;
                z.UpdateAnimSpeed(ownerNowNs);
            }

            if (aBodyReanim->ShouldTriggerTimedEvent(0.48f) || aBodyReanim->ShouldTriggerTimedEvent(0.8f))
            {
                z.mApp->PlayFoley(FoleyType::FOLEY_THUMP);
            }

            if (aBodyReanim->mLoopCount > 0)
            {
                z.ApplyBossSmokeParticles(ownerNowNs, false);
                z.BossPlayIdle(ownerNowNs);
            }
        },  // 90 PHASE_BOSS_HEAD_LEAVE
    };
    static_assert(sizeof(kBossPhaseCases) / sizeof(kBossPhaseCases[0]) ==
        (int)ZombiePhase::PHASE_BOSS_HEAD_LEAVE - (int)ZombiePhase::PHASE_BOSS_ENTER + 1,
        "kBossPhaseCases 槽位须与 ZombiePhase BOSS 段同步");

    const int aBossPhaseIndex = (int)mZombiePhase - (int)ZombiePhase::PHASE_BOSS_ENTER;
    if (aBossPhaseIndex >= 0 && aBossPhaseIndex < (int)(sizeof(kBossPhaseCases) / sizeof(kBossPhaseCases[0])))
    {
        kBossPhaseCases[aBossPhaseIndex](ownerNowNs, visualNowNs, animationNowNs, *this, aBodyReanim);
    }
    else
    {
        TOD_ASSERT(false);  // 原 else 兜底
    }
}

//0x5366D0
void Zombie::BossDie(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (!IsOnBoard())
        return;

    Reanimation* aFireBallReanim = mApp->ReanimationTryToGet(mBossFireBallReanimID);
    if (aFireBallReanim)
    {
        aFireBallReanim->ReanimationDie();
        mBossFireBallReanimID = ReanimationID::REANIMATIONID_NULL;

        BossDestroyIceballInRow(ownerNowNs);
        BossDestroyFireball(ownerNowNs, visualNowNs, animationNowNs);
    }

    mApp->mMusic->FadeOut(200);

    Zombie* aZombie = nullptr;
    while (mBoard->IterateZombies(aZombie))
    {
        if (aZombie != this && !aZombie->IsDeadOrDying())
        {
            aZombie->DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
        }
    }

    RemoveColdEffects(ownerNowNs);
}

//0x5367F0
void Zombie::BossSetupReanim(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Boss_innerleg, RENDER_GROUP_BOSS_BACK_LEG);
    aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Boss_outerleg, RENDER_GROUP_BOSS_FRONT_LEG);
    aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Boss_body2, RENDER_GROUP_BOSS_FRONT_LEG);
    aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Boss_innerarm, RENDER_GROUP_BOSS_BACK_ARM);
    aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Boss_RV, RENDER_GROUP_BOSS_BACK_ARM);

    Reanimation* aHeadReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_BOSS_DRIVER, animationNowNs);
    aHeadReanim->PlayReanim(ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 0, 18.0f);
    mSpecialHeadReanimID = mApp->ReanimationGetID(aHeadReanim);

    ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName(ReanimTrackId::Boss_head2);
    AttachEffect* aAttachEffect = AttachReanim(aTrackInstance->mAttachmentID, aHeadReanim, 28.0f, -84.0f);
    aBodyReanim->mFrameBasePose = 0;
    aAttachEffect->mOffset.m00 = 1.2f;
    aAttachEffect->mOffset.m11 = 1.2f;
    aAttachEffect->mDontDrawIfParentHidden = true;
}

namespace
{
// Boss 部件绘制分发表（段表化战役 2026-10-07）：行序 = BossPart 枚举值序
// （0..4）。BACK_LEG/FRONT_LEG/MAIN 三支同形——DrawReanim 仅渲染组常量
// 不同，lambda 各带常量逐句迁移；BACK_ARM/FIREBALL 异构体转发具名成员
// 函数。自由函数指针显式传 Zombie*，无成员指针调整层。
using BossPartDrawFn = void (*)(std::int64_t ownerNowNs, Zombie*, Graphics*, const ZombieDrawPosition&);
constexpr BossPartDrawFn kBossPartDraw[(int)BossPart::BOSS_PART_FIREBALL + 1] = {
    /* 0 BOSS_PART_BACK_LEG  */ [](std::int64_t ownerNowNs, Zombie* aZombie, Graphics* aG, const ZombieDrawPosition& aDrawPos) { aZombie->DrawReanim(ownerNowNs, aG, aDrawPos, RENDER_GROUP_BOSS_BACK_LEG); },
    /* 1 BOSS_PART_FRONT_LEG */ [](std::int64_t ownerNowNs, Zombie* aZombie, Graphics* aG, const ZombieDrawPosition& aDrawPos) { aZombie->DrawReanim(ownerNowNs, aG, aDrawPos, RENDER_GROUP_BOSS_FRONT_LEG); },
    /* 2 BOSS_PART_MAIN      */ [](std::int64_t ownerNowNs, Zombie* aZombie, Graphics* aG, const ZombieDrawPosition& aDrawPos) { aZombie->DrawReanim(ownerNowNs, aG, aDrawPos, 0); },
    /* 3 BOSS_PART_BACK_ARM  */ [](std::int64_t ownerNowNs, Zombie* aZombie, Graphics* aG, const ZombieDrawPosition& aDrawPos) { aZombie->DrawBossBackArm(ownerNowNs, aG, aDrawPos); },
    /* 4 BOSS_PART_FIREBALL  */ [](std::int64_t , Zombie* aZombie, Graphics* aG, const ZombieDrawPosition&) { aZombie->DrawBossFireBall(aG); },
};
static_assert(sizeof(kBossPartDraw) / sizeof(kBossPartDraw[0]) == 5,
    "kBossPartDraw 槽位须与 BossPart 枚举同步");
}  // namespace

//0x536940
void Zombie::DrawBossPart(std::int64_t ownerNowNs, Graphics* g, BossPart theBossPart)
{
    ZombieDrawPosition aDrawPos;
    GetDrawPos(aDrawPos);

    // C 形态 -> 自由函数指针表：BossPart 普通枚举 unsigned 化越界守卫，
    // 无匹配槽位 = 原 default 空操作（上游唯一调用点为 Board 渲染项表
    // 的 mBossPart，值域有界）。
    const unsigned aPart = static_cast<unsigned>(theBossPart);
    if (aPart < sizeof(kBossPartDraw) / sizeof(kBossPartDraw[0]))
    {
        kBossPartDraw[aPart](ownerNowNs, this, g, aDrawPos);
    }
}

//0x5369E0
void Zombie::PreloadZombieResources(ZombieType theZombieType)
{
    const ZombieDefinition& aZombieDef = GetZombieDefinition(theZombieType);
    if (aZombieDef.mReanimationType != ReanimationType::REANIM_NONE)
    {
        ReanimatorEnsureDefinitionLoaded(aZombieDef.mReanimationType, true);
    }

    // B 形态 -> 常量二维表 kZombiePreloadExtras（原 6 支链合并）：行序 =
    // ZombieType 枚举序 0..33，每行至多 3 个附加 reanim 定义，REANIM_NONE =
    // 行尾哨兵/无 case。GARGANTUAR/REDEYE 共体同值填两槽；BOSS 支含
    // gBossZombieList 循环装载，异构特例表外保留（表前 if）。
    static constexpr ReanimationType kZombiePreloadExtras[(int)ZombieType::NUM_ZOMBIE_TYPES][3] = {
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 0 ZOMBIE_NORMAL（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 1 ZOMBIE_FLAG（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 2 ZOMBIE_TRAFFIC_CONE（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 3 ZOMBIE_POLEVAULTER（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 4 ZOMBIE_PAIL（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 5 ZOMBIE_NEWSPAPER（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 6 ZOMBIE_DOOR（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 7 ZOMBIE_FOOTBALL（无 case）
        { ReanimationType::REANIM_BACKUP_DANCER, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 8 ZOMBIE_DANCER
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 9 ZOMBIE_BACKUP_DANCER（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 10 ZOMBIE_DUCKY_TUBE（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 11 ZOMBIE_SNORKEL（无 case）
        { ReanimationType::REANIM_IMP, ReanimationType::REANIM_ZOMBIE_CHARRED_ZAMBONI, ReanimationType::REANIM_NONE },  // 12 ZOMBIE_ZAMBONI
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 13 ZOMBIE_BOBSLED（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 14 ZOMBIE_DOLPHIN_RIDER（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 15 ZOMBIE_JACK_IN_THE_BOX（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 16 ZOMBIE_BALLOON（无 case）
        { ReanimationType::REANIM_DIGGER_DIRT, ReanimationType::REANIM_ZOMBIE_CHARRED_DIGGER, ReanimationType::REANIM_NONE },  // 17 ZOMBIE_DIGGER
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 18 ZOMBIE_POGO（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 19 ZOMBIE_YETI（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 20 ZOMBIE_BUNGEE（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 21 ZOMBIE_LADDER（无 case）
        { ReanimationType::REANIM_ZOMBIE_CHARRED_CATAPULT, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 22 ZOMBIE_CATAPULT
        { ReanimationType::REANIM_IMP, ReanimationType::REANIM_ZOMBIE_CHARRED_IMP, ReanimationType::REANIM_ZOMBIE_CHARRED_GARGANTUAR },  // 23 ZOMBIE_GARGANTUAR
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 24 ZOMBIE_IMP（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 25 ZOMBIE_BOSS（特例表外，槽位不可达）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 26 ZOMBIE_PEA_HEAD（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 27 ZOMBIE_WALLNUT_HEAD（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 28 ZOMBIE_JALAPENO_HEAD（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 29 ZOMBIE_GATLING_HEAD（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 30 ZOMBIE_SQUASH_HEAD（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 31 ZOMBIE_TALLNUT_HEAD（无 case）
        { ReanimationType::REANIM_IMP, ReanimationType::REANIM_ZOMBIE_CHARRED_IMP, ReanimationType::REANIM_ZOMBIE_CHARRED_GARGANTUAR },  // 32 ZOMBIE_REDEYE_GARGANTUAR（共体）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 33 ZOMBIE_ZOMBATAR（无 case）
    };
    static_assert(sizeof(kZombiePreloadExtras) / sizeof(kZombiePreloadExtras[0]) ==
        (int)ZombieType::NUM_ZOMBIE_TYPES, "kZombiePreloadExtras 槽位须与 ZombieType 枚举同步");

    if (theZombieType == ZombieType::ZOMBIE_BOSS)
    {
        ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_BOSS_DRIVER, true);
        ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_BOSS_FIREBALL, true);
        ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_BOSS_ICEBALL, true);

        for (size_t i = 0; i < LENGTH(gBossZombieList); i++)
        {
            const ZombieDefinition& aDef = GetZombieDefinition(gBossZombieList[i]);
            ReanimatorEnsureDefinitionLoaded(aDef.mReanimationType, true);
        }
    }
    else
    {
        const unsigned aTypeIndex = static_cast<unsigned>(theZombieType);
        if (aTypeIndex < sizeof(kZombiePreloadExtras) / sizeof(kZombiePreloadExtras[0]))
        {
            for (ReanimationType aExtraType : kZombiePreloadExtras[aTypeIndex])
            {
                if (aExtraType == ReanimationType::REANIM_NONE)  // 行尾哨兵
                {
                    break;
                }
                ReanimatorEnsureDefinitionLoaded(aExtraType, true);
            }
        }
    }

    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_PUFF, true);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIE_CHARRED, true);
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_LAWN_MOWERED_ZOMBIE, true);
}

//0x536B00
void Zombie::ApplyBossSmokeParticles(std::int64_t ownerNowNs, bool theEnable)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName(ReanimTrackId::Boss_head);
    AttachmentDetachCrossFadeParticleType(aTrackInstance->mAttachmentID, ParticleEffect::PARTICLE_ZAMBONI_SMOKE);

    if (theEnable)
    {
        TodParticleSystem* aParticle1 = mApp->AddTodParticle(0.0f, 0.0f, 0, ParticleEffect::PARTICLE_ZAMBONI_SMOKE, ownerNowNs);
        TodParticleSystem* aParticle2 = mApp->AddTodParticle(0.0f, 0.0f, 0, ParticleEffect::PARTICLE_ZAMBONI_SMOKE, ownerNowNs);
        if (aParticle1)
        {
            AttachEffect* aAttachEffect = aBodyReanim->AttachParticleToTrack(ReanimTrackId::Boss_head, aParticle1, 120.0f, 30.0f);
            aAttachEffect->mDontDrawIfParentHidden = true;
            aAttachEffect->mDontPropogateColor = true;
        }
        if (aParticle2)
        {
            AttachEffect* aAttachEffect = aBodyReanim->AttachParticleToTrack(ReanimTrackId::Boss_head, aParticle2, 205.0f, 58.0f);
            aAttachEffect->mDontDrawIfParentHidden = true;
            aAttachEffect->mDontPropogateColor = true;
        }

        if (mBodyHealth < mBodyMaxHealth / BOSS_FLASH_HEALTH_FRACTION)
        {
            TodParticleSystem* aParticle3 = mApp->AddTodParticle(0.0f, 0.0f, 0, ParticleEffect::PARTICLE_ZAMBONI_SMOKE, ownerNowNs);
            if (aParticle3)
            {
                AttachEffect* aAttachEffect = aBodyReanim->AttachParticleToTrack(ReanimTrackId::Boss_head, aParticle3, 193.0f, 27.0f);
                aAttachEffect->mDontDrawIfParentHidden = true;
                aAttachEffect->mDontPropogateColor = true;
            }
        }
    }
}

namespace
{
// 秘技编译期外观选项：太阳镜 %4、胡子 RandRangeInt(0,2) 直下标。
constexpr std::uint32_t kFutureSunglasses[4] = {
    ReanimOptions::Zombie::anim_head1::ZOMBIE_HEAD_SUNGLASSES1,
    ReanimOptions::Zombie::anim_head1::ZOMBIE_HEAD_SUNGLASSES2,
    ReanimOptions::Zombie::anim_head1::ZOMBIE_HEAD_SUNGLASSES3,
    ReanimOptions::Zombie::anim_head1::ZOMBIE_HEAD_SUNGLASSES4,
};
constexpr std::uint32_t kMustacheStyles[3] = {
    ReanimOptions::Zombie::Zombie_mustache::Default,
    ReanimOptions::Zombie::Zombie_mustache::ZOMBIE_MUSTACHE2,
    ReanimOptions::Zombie::Zombie_mustache::ZOMBIE_MUSTACHE3,
};
static_assert(kMustacheStyles[0] == 0 && kMustacheStyles[1] == 1 && kMustacheStyles[2] == 2);
static_assert(sizeof(kFutureSunglasses) / sizeof(kFutureSunglasses[0]) == 4, "太阳镜表槽位");
static_assert(sizeof(kMustacheStyles) / sizeof(kMustacheStyles[0]) == 3, "胡子表槽位");
}  // namespace

//0x536C90
void Zombie::EnableMustache(bool theEnableMustache)
{
    if (mFromWave == Zombie::ZOMBIE_WAVE_UI)
        return;

    if (!mHasHead || Zombie::IsZombotany(mZombieType))
        return;

    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (aBodyReanim == nullptr || !aBodyReanim->TrackExists(ReanimTrackId::Zombie_mustache))
        return;

    if (theEnableMustache)
    {
        aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Zombie_mustache, RENDER_GROUP_NORMAL);

        // 0 基随机下标直取（原 RandRangeInt(1,3) 改 0 基，消 -1 偏移；秘技路径
        // 一次性，RNG 流差异不可感知）
        aBodyReanim->SetVariantOption(ReanimTrackId::Zombie_mustache,
            kMustacheStyles[RandRangeInt(0, 2)]);
    }
    else
    {
        aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Zombie_mustache, RENDER_GROUP_HIDDEN);
    }
}

//0x536D90
void Zombie::EnableFuture(bool theEnableFuture)
{
    if (mFromWave == Zombie::ZOMBIE_WAVE_UI || Zombie::IsZombotany(mZombieType))
        return;

    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
    if (aBodyReanim == nullptr || aBodyReanim->mReanimationType != ReanimationType::REANIM_ZOMBIE)
        return;

    if (theEnableFuture)
    {
        // B 形态 -> 常量表（下方 kFutureSunglasses；%4 值域必在 0..3，default 断言废弃）
        aBodyReanim->SetVariantOption(ReanimTrackId::anim_head1,
            kFutureSunglasses[(unsigned int)mBoard->ZombieGetID(this) % 4]);
    }
    else
    {
        aBodyReanim->SetVariantOption(ReanimTrackId::anim_head1, 0);
    }
}

void Zombie::EnableDance(std::int64_t ownerNowNs)
{
    if (!IsOnBoard())
        return;

    if (ZombieNotWalking(ownerNowNs) || IsDeadOrDying())
        return;

    if (mZombieType == ZombieType::ZOMBIE_NORMAL || mZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE || mZombieType == ZombieType::ZOMBIE_PAIL)
    {
        StartWalkAnim(ownerNowNs, 0);
    }
}

//0x536EA0
void Zombie::SetupWaterTrack(ReanimTrackId theTrackId)
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName(theTrackId);
    aTrackInstance->mIgnoreExtraAdditiveColor = true;
    aTrackInstance->mIgnoreColorOverride = true;
    aTrackInstance->mIgnoreClipRect = true;
}

// GOTY @Patoke: 0x530C90
void Zombie::ResetZombatarHead(LawnApp* theApp)
{
	Reanimation* aHeadReanim = theApp->ReanimationTryToGet(mZombatarHeadReanimID);
	ResetZombatarReanimation(aHeadReanim);
}

// GOTY @Patoke: 0x531250
void Zombie::ApplyZombatarHead(LawnApp* theApp, Zombatar* theZombatar)
{
	if (!theZombatar)
		return;

	Reanimation* aHeadReanim = theApp->ReanimationTryToGet(mZombatarHeadReanimID);
	ApplyZombatarReanimation(aHeadReanim, *theZombatar);
}
