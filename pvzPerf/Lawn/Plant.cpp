#include "Sexy.TodLib/generated/EntityStrings.h"  // 动态键实体表（阶段 3）
#include "Sexy.TodLib/generated/ReanimVariantOptions.h"
#include "Coin.h"
#include "Plant.h"
#include "PlantLaunchClock.h"
#include "Motion.h"
#include "MagnetMotion.h"
#include "PlantFlags.h"
#include "Board.h"
#include "Zombie.h"
#include "ZombieFlags.h"
#include "Cutscene.h"
#include "GridItem.h"
#include "ZenGarden.h"
#include "Challenge.h"
#include "Projectile.h"
#include "SeedPacket.h"
#include "LawnApp.h"
#include "graphics/DDInterface.h"
#include "CursorObject.h"
#include "GameConstants.h"
#include "System/PlayerInfo.h"
#include "System/ReanimationLawn.h"
#include "Sexy.TodLib/TodFoley.h"
#include "Sexy.TodLib/TodDebug.h"
#include "Sexy.TodLib/Attachment.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/TodParticle.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "Widget/AchievementsWidget.h"
#include "Sexy.TodLib/generated/SceneAtlasData.h"
#include "graphics/AtlasDraw.h"

namespace
{
constexpr MagnetAtlas::Sprite kMagnetSprites[] = {
    MagnetAtlas::Sprite::Count,
    MagnetAtlas::Sprite::REANIM_ZOMBIE_BUCKET1,
    MagnetAtlas::Sprite::REANIM_ZOMBIE_BUCKET2,
    MagnetAtlas::Sprite::REANIM_ZOMBIE_BUCKET3,
    MagnetAtlas::Sprite::REANIM_ZOMBIE_FOOTBALL_HELMET,
    MagnetAtlas::Sprite::REANIM_ZOMBIE_FOOTBALL_HELMET2,
    MagnetAtlas::Sprite::REANIM_ZOMBIE_FOOTBALL_HELMET3,
    MagnetAtlas::Sprite::REANIM_ZOMBIE_SCREENDOOR1,
    MagnetAtlas::Sprite::REANIM_ZOMBIE_SCREENDOOR2,
    MagnetAtlas::Sprite::REANIM_ZOMBIE_SCREENDOOR3,
    MagnetAtlas::Sprite::ZOMBIEPOGO_1,
    MagnetAtlas::Sprite::ZOMBIEPOGO_2,
    MagnetAtlas::Sprite::ZOMBIEPOGO_3,
    MagnetAtlas::Sprite::REANIM_ZOMBIE_JACKBOX_BOX,
    MagnetAtlas::Sprite::REANIM_ZOMBIE_LADDER_1,
    MagnetAtlas::Sprite::REANIM_ZOMBIE_LADDER_1_DAMAGE1,
    MagnetAtlas::Sprite::REANIM_ZOMBIE_LADDER_1_DAMAGE2,
    MagnetAtlas::Sprite::REANIM_ZOMBIE_LADDER_5,
    MagnetAtlas::Sprite::REANIM_COIN_SILVER_DOLLAR,
    MagnetAtlas::Sprite::REANIM_COIN_GOLD_DOLLAR,
    MagnetAtlas::Sprite::REANIM_DIAMOND,
    MagnetAtlas::Sprite::REANIM_ZOMBIE_DIGGER_PICKAXE
};
static_assert(std::size(kMagnetSprites) == MAGNET_ITEM_PICK_AXE + 1,
    "MagnetItemType needs a matching atlas sprite");
}

static PlantDefinition gPlantDefs[SeedType::NUM_SEED_TYPES] = {  //0x69F2B0
    { SeedType::SEED_PEASHOOTER,         ReanimationType::REANIM_PEASHOOTER,    0,  100,    750,    PlantSubClass::SUBCLASS_SHOOTER,    150 },
    { SeedType::SEED_SUNFLOWER,          ReanimationType::REANIM_SUNFLOWER,     1,  50,     750,    PlantSubClass::SUBCLASS_NORMAL,     2500 },
    { SeedType::SEED_CHERRYBOMB,         ReanimationType::REANIM_CHERRYBOMB,    3,  150,    5000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_WALLNUT,            ReanimationType::REANIM_WALLNUT,       2,  50,     3000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_POTATOMINE,         ReanimationType::REANIM_POTATOMINE,    37, 25,     3000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_SNOWPEA,            ReanimationType::REANIM_SNOWPEA,       4,  175,    750,    PlantSubClass::SUBCLASS_SHOOTER,    150 },
    { SeedType::SEED_CHOMPER,            ReanimationType::REANIM_CHOMPER,       31, 150,    750,    PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_REPEATER,           ReanimationType::REANIM_REPEATER,      5,  200,    750,    PlantSubClass::SUBCLASS_SHOOTER,    150 },
    { SeedType::SEED_PUFFSHROOM,         ReanimationType::REANIM_PUFFSHROOM,    6,  0,      750,    PlantSubClass::SUBCLASS_SHOOTER,    150 },
    { SeedType::SEED_SUNSHROOM,          ReanimationType::REANIM_SUNSHROOM,     7,  25,     750,    PlantSubClass::SUBCLASS_NORMAL,     2500 },
    { SeedType::SEED_FUMESHROOM,         ReanimationType::REANIM_FUMESHROOM,    9,  75,     750,    PlantSubClass::SUBCLASS_SHOOTER,    150 },
    { SeedType::SEED_GRAVEBUSTER,        ReanimationType::REANIM_GRAVE_BUSTER,  40, 75,     750,    PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_HYPNOSHROOM,        ReanimationType::REANIM_HYPNOSHROOM,   10, 75,     3000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_SCAREDYSHROOM,      ReanimationType::REANIM_SCRAREYSHROOM, 33, 25,     750,    PlantSubClass::SUBCLASS_SHOOTER,    150 },
    { SeedType::SEED_ICESHROOM,          ReanimationType::REANIM_ICESHROOM,     36, 75,     5000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_DOOMSHROOM,         ReanimationType::REANIM_DOOMSHROOM,    20, 125,    5000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_LILYPAD,            ReanimationType::REANIM_LILYPAD,       19, 25,     750,    PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_SQUASH,             ReanimationType::REANIM_SQUASH,        21, 50,     3000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_THREEPEATER,        ReanimationType::REANIM_THREEPEATER,   12, 325,    750,    PlantSubClass::SUBCLASS_SHOOTER,    150 },
    { SeedType::SEED_TANGLEKELP,         ReanimationType::REANIM_TANGLEKELP,    17, 25,     3000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_JALAPENO,           ReanimationType::REANIM_JALAPENO,      11, 125,    5000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_SPIKEWEED,          ReanimationType::REANIM_SPIKEWEED,     22, 100,    750,    PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_TORCHWOOD,          ReanimationType::REANIM_TORCHWOOD,     29, 175,    750,    PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_TALLNUT,            ReanimationType::REANIM_TALLNUT,       28, 125,    3000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_SEASHROOM,          ReanimationType::REANIM_SEASHROOM,     39, 0,      3000,   PlantSubClass::SUBCLASS_SHOOTER,    150 },
    { SeedType::SEED_PLANTERN,           ReanimationType::REANIM_PLANTERN,      38, 25,     3000,   PlantSubClass::SUBCLASS_NORMAL,     2500 },
    { SeedType::SEED_CACTUS,             ReanimationType::REANIM_CACTUS,        15, 125,    750,    PlantSubClass::SUBCLASS_SHOOTER,    150 },
    { SeedType::SEED_BLOVER,             ReanimationType::REANIM_BLOVER,        18, 100,    750,    PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_SPLITPEA,           ReanimationType::REANIM_SPLITPEA,      32, 125,    750,    PlantSubClass::SUBCLASS_SHOOTER,    150 },
    { SeedType::SEED_STARFRUIT,          ReanimationType::REANIM_STARFRUIT,     30, 125,    750,    PlantSubClass::SUBCLASS_SHOOTER,    150 },
    { SeedType::SEED_PUMPKINSHELL,       ReanimationType::REANIM_PUMPKIN,       25, 125,    3000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_MAGNETSHROOM,       ReanimationType::REANIM_MAGNETSHROOM,  35, 100,    750,    PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_CABBAGEPULT,        ReanimationType::REANIM_CABBAGEPULT,   13, 100,    750,    PlantSubClass::SUBCLASS_SHOOTER,    300 },
    { SeedType::SEED_FLOWERPOT,          ReanimationType::REANIM_FLOWER_POT,    33, 25,     750,    PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_KERNELPULT,         ReanimationType::REANIM_KERNELPULT,    13, 100,    750,    PlantSubClass::SUBCLASS_SHOOTER,    300 },
    { SeedType::SEED_INSTANT_COFFEE,     ReanimationType::REANIM_COFFEEBEAN,    33, 75,     750,    PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_GARLIC,             ReanimationType::REANIM_GARLIC,        8,  50,     750,    PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_UMBRELLA,           ReanimationType::REANIM_UMBRELLALEAF,  23, 100,    750,    PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_MARIGOLD,           ReanimationType::REANIM_MARIGOLD,      24, 50,     3000,   PlantSubClass::SUBCLASS_NORMAL,     2500 },
    { SeedType::SEED_MELONPULT,          ReanimationType::REANIM_MELONPULT,     14, 300,    750,    PlantSubClass::SUBCLASS_SHOOTER,    300 },
    { SeedType::SEED_GATLINGPEA,         ReanimationType::REANIM_GATLINGPEA,    5,  250,    5000,   PlantSubClass::SUBCLASS_SHOOTER,    150 },
    { SeedType::SEED_TWINSUNFLOWER,      ReanimationType::REANIM_TWIN_SUNFLOWER,1,  150,    5000,   PlantSubClass::SUBCLASS_NORMAL,     2500 },
    { SeedType::SEED_GLOOMSHROOM,        ReanimationType::REANIM_GLOOMSHROOM,   27, 150,    5000,   PlantSubClass::SUBCLASS_SHOOTER,    200 },
    { SeedType::SEED_CATTAIL,            ReanimationType::REANIM_CATTAIL,       27, 225,    5000,   PlantSubClass::SUBCLASS_SHOOTER,    150 },
    { SeedType::SEED_WINTERMELON,        ReanimationType::REANIM_WINTER_MELON,  27, 200,    5000,   PlantSubClass::SUBCLASS_SHOOTER,    300 },
    { SeedType::SEED_GOLD_MAGNET,        ReanimationType::REANIM_GOLD_MAGNET,   27, 50,     5000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_SPIKEROCK,          ReanimationType::REANIM_SPIKEROCK,     27, 125,    5000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_COBCANNON,          ReanimationType::REANIM_COBCANNON,     16, 500,    5000,   PlantSubClass::SUBCLASS_NORMAL,     600 },
    { SeedType::SEED_IMITATER,           ReanimationType::REANIM_IMITATER,      33, 0,      750,    PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_EXPLODE_O_NUT,      ReanimationType::REANIM_WALLNUT,       2,  0,      3000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_GIANT_WALLNUT,      ReanimationType::REANIM_WALLNUT,       2,  0,      3000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_SPROUT,             ReanimationType::REANIM_ZENGARDEN_SPROUT,          33, 0,      3000,   PlantSubClass::SUBCLASS_NORMAL,     0 },
    { SeedType::SEED_LEFTPEATER,         ReanimationType::REANIM_REPEATER,      5,  200,    750,    PlantSubClass::SUBCLASS_SHOOTER,    150 }
};

//0x401B20
Plant::Plant()
{
}


//0x45DB60
// GOTY @Patoke: 0x461483
void Plant::PlantInitialize(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType)
{
    // PlantInitialize 槽位上下文：switch 外提引用（主体 reanim 指针、定义动画
    // 型）经此传递。表内匿名 lambda = 原 case 体逐句迁移（生成器 + 逆变换
    // token 对拍 0 差异），标签共体组与 default 具名（lambda 无法跨槽复用）。
    struct PlantInitLocals
    {
    Reanimation* mBodyReanim;
    ReanimationType mReanimationType;
    };
    using PlantInitCaseFn = void (*)(std::int64_t, std::int64_t, std::int64_t, Plant&, PlantInitLocals&);

    // 标签共体（匿名 lambda 无法跨槽复用，复用即具名）
    // 原 case SEED_PEASHOOTER, SEED_SNOWPEA, SEED_REPEATER, SEED_LEFTPEATER, SEED_GATLINGPEA
    static constexpr auto PlantInitPeashooter = [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    if (lc.mBodyReanim)
    {
    lc.mBodyReanim->ApplyRate(RandRangeFloat(15.0f, 20.0f));
    Reanimation* aHeadReanim = p.mApp->AddReanimation(0.0f, 0.0f, p.mRenderOrder + 2, lc.mReanimationType, animationNowNs);
    aHeadReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
    aHeadReanim->ApplyRate(lc.mBodyReanim->mAnimRate);
    aHeadReanim->SetFramesForLayer(ReanimTrackId::anim_head_idle);
    p.mHeadReanimID = p.mApp->ReanimationGetID(aHeadReanim);

    if (lc.mBodyReanim->TrackExists(ReanimTrackId::anim_stem))
    aHeadReanim->AttachToAnotherReanimation(lc.mBodyReanim, ReanimTrackId::anim_stem);
    else if (lc.mBodyReanim->TrackExists(ReanimTrackId::anim_idle))
    aHeadReanim->AttachToAnotherReanimation(lc.mBodyReanim, ReanimTrackId::anim_idle);
    }
    };

    // 标签共体（匿名 lambda 无法跨槽复用，复用即具名）
    // 原 case SEED_CHERRYBOMB, SEED_JALAPENO
    static constexpr auto PlantInitCherrybomb = [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    TOD_ASSERT(lc.mBodyReanim);

    if (p.IsInPlay())
    {
    p.mDoSpecialCountdown = 100;

    lc.mBodyReanim->SetFramesForLayer(ReanimTrackId::anim_explode);
    lc.mBodyReanim->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);

    p.mApp->PlayFoley(FoleyType::FOLEY_REVERSE_EXPLOSION);
    }
    };

    // 标签共体（匿名 lambda 无法跨槽复用，复用即具名）
    // 原 case SEED_PUFFSHROOM, SEED_SEASHROOM
    static constexpr auto PlantInitPuffshroom = [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    if (p.IsInPlay())
    {
    { p.mX += (Sexy::Rand(10) - 5); p.mRootBindingGeneration = 0; }
    p.mY += (Sexy::Rand(6) - 3);
    }
    };

    // 原 default（无 case 的 15 型（空操作）共用）
    static constexpr auto PlantInitDefault = [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {

    };

    // 匿名 lambda 函数指针表，行序 = SeedType 枚举值序（0..52）。
    static constexpr PlantInitCaseFn kPlantInitCases[(int)SeedType::NUM_SEED_TYPES] = {
    PlantInitPeashooter,  // 0 SEED_PEASHOOTER（共体）
    PlantInitDefault,  // 1 SEED_SUNFLOWER（原 default）
    PlantInitCherrybomb,  // 2 SEED_CHERRYBOMB（共体）
    // 原 case SEED_WALLNUT
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    p.mPlantHealth = 4000;
    p.mBlinkCountdown = 1000 + Sexy::Rand(1000);
    },  // 3 SEED_WALLNUT
    // 原 case SEED_POTATOMINE
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    TOD_ASSERT(lc.mBodyReanim);

    lc.mBodyReanim->ApplyRate(12.0f);

    if (p.IsInPlay())
    {
    lc.mBodyReanim->AssignRenderGroupToTrack(ReanimTrackId::anim_glow, RENDER_GROUP_HIDDEN);
    p.mStateCountdown = 1500;
    }
    else
    {
    lc.mBodyReanim->SetFramesForLayer(ReanimTrackId::anim_armed);
    p.mState = PlantState::STATE_POTATO_ARMED;
    }
    },  // 4 SEED_POTATOMINE
    PlantInitPeashooter,  // 5 SEED_SNOWPEA（共体）
    // 原 case SEED_CHOMPER
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    p.mState = PlantState::STATE_READY;
    },  // 6 SEED_CHOMPER
    PlantInitPeashooter,  // 7 SEED_REPEATER（共体）
    PlantInitPuffshroom,  // 8 SEED_PUFFSHROOM（共体）
    // 原 case SEED_SUNSHROOM
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    TOD_ASSERT(lc.mBodyReanim);

    lc.mBodyReanim->mFrameBasePose = 6;

    if (p.IsInPlay())
    {
    { p.mX += (Sexy::Rand(10) - 5); p.mRootBindingGeneration = 0; }
    p.mY += (Sexy::Rand(10) - 5);
    }
    else if (p.mIsAsleep)
    lc.mBodyReanim->SetFramesForLayer(ReanimTrackId::anim_bigsleep);
    else
    lc.mBodyReanim->SetFramesForLayer(ReanimTrackId::anim_bigidle);

    p.mState = PlantState::STATE_SUNSHROOM_SMALL;
    p.mStateCountdown = 12000;
    },  // 9 SEED_SUNSHROOM
    PlantInitDefault,  // 10 SEED_FUMESHROOM（原 default）
    // 原 case SEED_GRAVEBUSTER
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    TOD_ASSERT(lc.mBodyReanim);

    if (p.IsInPlay())
    {
    lc.mBodyReanim->SetFramesForLayer(ReanimTrackId::anim_land);
    lc.mBodyReanim->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);

    p.mState = PlantState::STATE_GRAVEBUSTER_LANDING;
    p.mApp->PlayFoley(FoleyType::FOLEY_GRAVEBUSTERCHOMP);
    }
    },  // 11 SEED_GRAVEBUSTER
    PlantInitDefault,  // 12 SEED_HYPNOSHROOM（原 default）
    // 原 case SEED_SCAREDYSHROOM
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    p.mState = PlantState::STATE_READY;
    },  // 13 SEED_SCAREDYSHROOM
    PlantInitDefault,  // 14 SEED_ICESHROOM（原 default）
    PlantInitDefault,  // 15 SEED_DOOMSHROOM（原 default）
    // 原 case SEED_LILYPAD
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    if (p.IsInPlay())
    {
    p.mState = PlantState::STATE_LILYPAD_INVULNERABLE;
    p.mStateCountdown = 100;
    }
    },  // 16 SEED_LILYPAD
    PlantInitDefault,  // 17 SEED_SQUASH（原 default）
    // 原 case SEED_THREEPEATER
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    TOD_ASSERT(lc.mBodyReanim);

    lc.mBodyReanim->ApplyRate(RandRangeFloat(15.0f, 20.0f));
    Reanimation* aHeadReanim1 = p.mApp->AddReanimation(0.0f, 0.0f, p.mRenderOrder + 2, lc.mReanimationType, animationNowNs);
    aHeadReanim1->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
    aHeadReanim1->ApplyRate(lc.mBodyReanim->mAnimRate);
    aHeadReanim1->SetFramesForLayer(ReanimTrackId::anim_head_idle1);
    aHeadReanim1->AttachToAnotherReanimation(lc.mBodyReanim, ReanimTrackId::anim_head1);
    p.mHeadReanimID = p.mApp->ReanimationGetID(aHeadReanim1);

    Reanimation* aHeadReanim2 = p.mApp->AddReanimation(0.0f, 0.0f, p.mRenderOrder + 2, lc.mReanimationType, animationNowNs);
    aHeadReanim2->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
    aHeadReanim2->ApplyRate(lc.mBodyReanim->mAnimRate);
    aHeadReanim2->SetFramesForLayer(ReanimTrackId::anim_head_idle2);
    aHeadReanim2->AttachToAnotherReanimation(lc.mBodyReanim, ReanimTrackId::anim_head2);
    p.mHeadReanimID2 = p.mApp->ReanimationGetID(aHeadReanim2);

    Reanimation* aHeadReanim3 = p.mApp->AddReanimation(0.0f, 0.0f, p.mRenderOrder + 2, lc.mReanimationType, animationNowNs);
    aHeadReanim3->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
    aHeadReanim3->ApplyRate(lc.mBodyReanim->mAnimRate);
    aHeadReanim3->SetFramesForLayer(ReanimTrackId::anim_head_idle3);
    aHeadReanim3->AttachToAnotherReanimation(lc.mBodyReanim, ReanimTrackId::anim_head3);
    p.mHeadReanimID3 = p.mApp->ReanimationGetID(aHeadReanim3);
    },  // 18 SEED_THREEPEATER
    // 原 case SEED_TANGLEKELP
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    TOD_ASSERT(lc.mBodyReanim);
    lc.mBodyReanim->SetTruncateDisappearingFramesAll(false);
    },  // 19 SEED_TANGLEKELP
    PlantInitCherrybomb,  // 20 SEED_JALAPENO（共体）
    PlantInitDefault,  // 21 SEED_SPIKEWEED（原 default）
    // 原 case SEED_TORCHWOOD
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {

    },  // 22 SEED_TORCHWOOD
    // 原 case SEED_TALLNUT
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    p.mPlantHealth = 8000;
    p.mHeight = 80;
    p.mBlinkCountdown = 1000 + Sexy::Rand(1000);
    },  // 23 SEED_TALLNUT
    PlantInitPuffshroom,  // 24 SEED_SEASHROOM（共体）
    // 原 case SEED_PLANTERN
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    p.mStateCountdown = 50;

    if (!p.mIsOnBoard || p.mApp->mGameMode != GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
    {
    p.AddAttachedParticle(ownerNowNs, (p.mX) + 40, (p.mY) + 40, (int)RenderLayer::RENDER_LAYER_FOG + 1, ParticleEffect::PARTICLE_LANTERN_SHINE);
    }
    if (p.IsInPlay())
    {
    p.mApp->PlaySample(Sexy::SOUND_PLANTERN);
    }
    },  // 25 SEED_PLANTERN
    // 原 case SEED_CACTUS
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    p.mState = PlantState::STATE_CACTUS_LOW;
    },  // 26 SEED_CACTUS
    // 原 case SEED_BLOVER
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    p.mDoSpecialCountdown = 50;

    if (p.IsInPlay())
    {
    lc.mBodyReanim->SetFramesForLayer(ReanimTrackId::anim_blow);
    lc.mBodyReanim->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
    lc.mBodyReanim->ApplyRate(20.0f);
    }
    else
    {
    lc.mBodyReanim->SetFramesForLayer(ReanimTrackId::anim_idle);
    lc.mBodyReanim->ApplyRate(10.0f);
    }
    },  // 27 SEED_BLOVER
    // 原 case SEED_SPLITPEA
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    TOD_ASSERT(lc.mBodyReanim);

    lc.mBodyReanim->ApplyRate(RandRangeFloat(15.0f, 20.0f));
    Reanimation* aHeadReanim1 = p.mApp->AddReanimation(0.0f, 0.0f, p.mRenderOrder + 2, lc.mReanimationType, animationNowNs);
    aHeadReanim1->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
    aHeadReanim1->ApplyRate(lc.mBodyReanim->mAnimRate);
    aHeadReanim1->SetFramesForLayer(ReanimTrackId::anim_head_idle);
    aHeadReanim1->AttachToAnotherReanimation(lc.mBodyReanim, ReanimTrackId::anim_idle);
    p.mHeadReanimID = p.mApp->ReanimationGetID(aHeadReanim1);

    Reanimation* aHeadReanim2 = p.mApp->AddReanimation(0.0f, 0.0f, p.mRenderOrder + 2, lc.mReanimationType, animationNowNs);
    aHeadReanim2->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
    aHeadReanim2->ApplyRate(lc.mBodyReanim->mAnimRate);
    aHeadReanim2->SetFramesForLayer(ReanimTrackId::anim_splitpea_idle);
    aHeadReanim2->AttachToAnotherReanimation(lc.mBodyReanim, ReanimTrackId::anim_idle);
    p.mHeadReanimID2 = p.mApp->ReanimationGetID(aHeadReanim2);
    },  // 28 SEED_SPLITPEA
    PlantInitDefault,  // 29 SEED_STARFRUIT（原 default）
    // 原 case SEED_PUMPKINSHELL
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    p.mPlantHealth = 4000;
    p.mWidth = 120;

    TOD_ASSERT(lc.mBodyReanim);
    lc.mBodyReanim->AssignRenderGroupToTrack(ReanimTrackId::Pumpkin_back, 1);
    },  // 30 SEED_PUMPKINSHELL
    // 原 case SEED_MAGNETSHROOM
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    TOD_ASSERT(lc.mBodyReanim);
    lc.mBodyReanim->SetTruncateDisappearingFramesAll(false);
    },  // 31 SEED_MAGNETSHROOM
    PlantInitDefault,  // 32 SEED_CABBAGEPULT（原 default）
    // 原 case SEED_FLOWERPOT
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    if (p.IsInPlay())
    {
    p.mState = PlantState::STATE_FLOWERPOT_INVULNERABLE;
    p.mStateCountdown = 100;
    }
    },  // 33 SEED_FLOWERPOT
    // 原 case SEED_KERNELPULT
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    TOD_ASSERT(lc.mBodyReanim);
    lc.mBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Cornpult_butter, RENDER_GROUP_HIDDEN);
    },  // 34 SEED_KERNELPULT
    // 原 case SEED_INSTANT_COFFEE
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    p.mDoSpecialCountdown = 100;
    },  // 35 SEED_INSTANT_COFFEE
    // 原 case SEED_GARLIC
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    TOD_ASSERT(lc.mBodyReanim);
    p.mPlantHealth = 400;
    lc.mBodyReanim->SetTruncateDisappearingFramesAll(false);
    },  // 36 SEED_GARLIC
    PlantInitDefault,  // 37 SEED_UMBRELLA（原 default）
    // 原 case SEED_MARIGOLD
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    TOD_ASSERT(lc.mBodyReanim);
    lc.mBodyReanim->ApplyRate(RandRangeFloat(15.0f, 20.0f));
    },  // 38 SEED_MARIGOLD
    PlantInitDefault,  // 39 SEED_MELONPULT（原 default）
    PlantInitPeashooter,  // 40 SEED_GATLINGPEA（共体）
    PlantInitDefault,  // 41 SEED_TWINSUNFLOWER（原 default）
    PlantInitDefault,  // 42 SEED_GLOOMSHROOM（原 default）
    PlantInitDefault,  // 43 SEED_CATTAIL（原 default）
    PlantInitDefault,  // 44 SEED_WINTERMELON（原 default）
    // 原 case SEED_GOLD_MAGNET
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    TOD_ASSERT(lc.mBodyReanim);
    lc.mBodyReanim->SetTruncateDisappearingFramesAll(false);
    },  // 45 SEED_GOLD_MAGNET
    // 原 case SEED_SPIKEROCK
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    p.mPlantHealth = 450;
    TOD_ASSERT(lc.mBodyReanim);
    },  // 46 SEED_SPIKEROCK
    // 原 case SEED_COBCANNON
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    if (p.IsInPlay())
    {
    p.mState = PlantState::STATE_COBCANNON_ARMING;
    p.mStateCountdown = 500;

    TOD_ASSERT(lc.mBodyReanim);
    lc.mBodyReanim->SetFramesForLayer(ReanimTrackId::anim_unarmed_idle);
    }
    },  // 47 SEED_COBCANNON
    // 原 case SEED_IMITATER
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    TOD_ASSERT(lc.mBodyReanim);
    lc.mBodyReanim->ApplyRate(RandRangeFloat(25.0f, 30.0f));
    p.mStateCountdown = 200;
    },  // 48 SEED_IMITATER
    // 原 case SEED_EXPLODE_O_NUT
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    p.mPlantHealth = 4000;
    p.mBlinkCountdown = 1000 + Sexy::Rand(1000);
    lc.mBodyReanim->mColorOverride = Color(255, 64, 64);
    },  // 49 SEED_EXPLODE_O_NUT
    // 原 case SEED_GIANT_WALLNUT
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {
    p.mPlantHealth = 4000;
    p.mBlinkCountdown = 1000 + Sexy::Rand(1000);
    },  // 50 SEED_GIANT_WALLNUT
    // 原 case SEED_SPROUT
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantInitLocals& lc)
    {

    },  // 51 SEED_SPROUT
    PlantInitPeashooter,  // 52 SEED_LEFTPEATER（共体）
    };
    static_assert(sizeof(kPlantInitCases) / sizeof(kPlantInitCases[0]) ==
    (int)SeedType::NUM_SEED_TYPES, "kPlantInitCases 槽位数须与 SeedType 枚举同步");

    mPlantCol = theGridX;
    mRow = theGridY;
    if (mBoard)
    {
        { mX = (mBoard->GridToPixelX(theGridX, theGridY)); mRootBindingGeneration = 0; }
        mY = (mBoard->GridToPixelY(theGridX, theGridY));
    }
    mAnimCounter = 0;
    mAnimPing = true;
    mFrame = 0;
    mShootingEndTimeNs = 0;
    mShakeOffsetX = 0.0f;
    mShakeOffsetY = 0.0f;
    mFrameLength = RandRangeInt(12, 18);
    mTargetX = -1;
    mTargetY = -1;
    mStartRow = mRow;
    mNumFrames = 5;
    mState = PlantState::STATE_NOTREADY;
    mBowlingRowArrivalTimeNs = 0;
    mDead = false;
    mSquished = false;
    mSeedType = theSeedType;
    mImitaterType = theImitaterType;
    mPlantHealth = 300;
    mDoSpecialCountdown = 0;
    mDisappearEndTimeNs = 0;
    mStateCountdown = 0;
    mParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
    mBodyReanimID = ReanimationID::REANIMATIONID_NULL; mRootBindingGeneration = 0;
    mHeadReanimID = ReanimationID::REANIMATIONID_NULL;
    mHeadReanimID2 = ReanimationID::REANIMATIONID_NULL;
    mHeadReanimID3 = ReanimationID::REANIMATIONID_NULL;
    mBlinkReanimID = ReanimationID::REANIMATIONID_NULL;
    mLightReanimID = ReanimationID::REANIMATIONID_NULL;
    mSleepingReanimID = ReanimationID::REANIMATIONID_NULL;
    mBlinkCountdown = 0;
    mRecentlyEatenEndTimeNs = 0;
    mEatenFlashEndTimeNs = 0;
    mBeghouledFlashEndTimeNs = 0;
    mWidth = 80;
    mHeight = 80;
    memset(mMagnetItems, 0, sizeof(mMagnetItems));
    mMagnetStartTimeNs = ownerNowNs;
    const PlantDefinition& aPlantDef = GetPlantDefinition(theSeedType);
    mIsAsleep = false;
    mWakeUpEndTimeNs = 0;
    mOnBungeeState = PlantOnBungeeState::NOT_ON_BUNGEE;
    mPottedPlantIndex = -1;
    mLaunchRate = aPlantDef.mLaunchRate;
    mSubclass = aPlantDef.mSubClass;
    mRenderOrder = CalcRenderOrder();

    Reanimation* aBodyReanim = nullptr;
    if (aPlantDef.mReanimationType != ReanimationType::REANIM_NONE)
    {
        float aOffsetY = PlantDrawHeightOffset(ownerNowNs, mBoard, this, mSeedType, mPlantCol, mRow);
        aBodyReanim = mApp->AddReanimation(0.0f, aOffsetY, mRenderOrder + 1, aPlantDef.mReanimationType, animationNowNs);
        aBodyReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
        aBodyReanim->ApplyRate(RandRangeFloat(10.0f, 15.0f));

        if (aBodyReanim->TrackExists(ReanimTrackId::anim_idle))
            aBodyReanim->SetFramesForLayer(ReanimTrackId::anim_idle);

        if (mApp->IsWallnutBowlingLevel() && aBodyReanim->TrackExists(ReanimTrackId::_ground))
        {
            aBodyReanim->SetFramesForLayer(ReanimTrackId::_ground);
            if (mSeedType == SeedType::SEED_WALLNUT || mSeedType == SeedType::SEED_EXPLODE_O_NUT)
                aBodyReanim->ApplyRate(RandRangeFloat(12.0f, 18.0f));
            else if (mSeedType == SeedType::SEED_GIANT_WALLNUT)
                aBodyReanim->ApplyRate(RandRangeFloat(6.0f, 10.0f));
        }

        aBodyReanim->mIsAttachment = true;
        mBodyReanimID = mApp->ReanimationGetID(aBodyReanim); mRootBindingGeneration = 0;
        if (mApp->IsWallnutBowlingLevel() && aBodyReanim->TrackExists(ReanimTrackId::_ground))
            AdvanceRoot(*this, *aBodyReanim,
                mSeedType == SeedType::SEED_GIANT_WALLNUT ? -2.0f : -1.0f, 0);
        mBlinkCountdown = 400 + Sexy::Rand(400);
    }

    if (IsNocturnal(mSeedType) && mBoard && !mBoard->StageIsNight())
        SetSleeping(ownerNowNs, animationNowNs, true);

    const auto aLaunchStartNs = ownerNowNs;
    mLaunchWindowStartNs = aLaunchStartNs;
    mLaunchClockActive = false;
    mZenProductionCountdown = 0;
    std::int64_t aInitialLaunchDelay = 0;
    if (mLaunchRate > 0)
    {
        if (MakesSun())
            aInitialLaunchDelay = PlantLaunchClock::Uniform(300 * PlantLaunchClock::Tick,
                mLaunchRate.mNanoseconds / 2, [] { return Sexy::Rand(); });
        else
            aInitialLaunchDelay = PlantLaunchClock::Uniform(0, mLaunchRate.mNanoseconds,
                [] { return Sexy::Rand(); });
    }
    mNextLaunchTimeNs = PlantLaunchClock::Add(aLaunchStartNs, aInitialLaunchDelay);

    // C 形态 -> 匿名 lambda 函数指针表：行序 = SeedType 枚举序（0..52），
    // 越界（BEGHOULED 按钮等哨兵）落表外守卫 = 原 default 无操作。
    PlantInitLocals aInitLocals{aBodyReanim, aPlantDef.mReanimationType};
    const unsigned aSeedIndex = static_cast<unsigned>(theSeedType);
    if (aSeedIndex < static_cast<unsigned>(SeedType::NUM_SEED_TYPES))
    {
        kPlantInitCases[aSeedIndex](ownerNowNs, visualNowNs, animationNowNs, *this, aInitLocals);
    }


    if ((mApp->mGameMode == GameMode::GAMEMODE_LIMBO_BIG_TIME) &&
        (theSeedType == SeedType::SEED_WALLNUT || theSeedType == SeedType::SEED_SUNFLOWER || theSeedType == SeedType::SEED_MARIGOLD))
    {
        mPlantHealth *= 2;
    }
    mPlantMaxHealth = mPlantHealth;

    if (mSeedType != SeedType::SEED_FLOWERPOT && mIsOnBoard)
    {
        TOD_ASSERT(mBoard);
        Plant* aFlowerPot = mBoard->GetFlowerPotAt(mPlantCol, mRow);
        if (aFlowerPot)
            mApp->ReanimationGet(aFlowerPot->mBodyReanimID)->ApplyRate(0.0f);
    }
}

//0x45E7C0
int Plant::CalcRenderOrder()
{
    PLANT_ORDER anOrder = PLANT_ORDER::PLANT_ORDER_NORMAL;
    RenderLayer aLayer = RenderLayer::RENDER_LAYER_PLANT;

    SeedType aSeedType = mSeedType;
    if (mSeedType == SeedType::SEED_IMITATER && mImitaterType != SeedType::SEED_NONE)
        aSeedType = mImitaterType;

    if (mApp->IsWallnutBowlingLevel())
    {
        aLayer = RenderLayer::RENDER_LAYER_PROJECTILE;
    }
    else if (aSeedType == SeedType::SEED_PUMPKINSHELL)
    {
        anOrder = PLANT_ORDER::PLANT_ORDER_PUMPKIN;
    }
    else if (IsFlying(aSeedType))
    {
        anOrder = PLANT_ORDER::PLANT_ORDER_FLYER;
    }
    else if (aSeedType == SeedType::SEED_FLOWERPOT || (aSeedType == SeedType::SEED_LILYPAD && mApp->mGameMode != GameMode::GAMEMODE_LIMBO_ZEN_GARDEN))
    {
        anOrder = PLANT_ORDER::PLANT_ORDER_LILYPAD;
    }

    return Board::MakeRenderOrder(aLayer, mRow, anOrder * 5 - (mX) + 800);
}

//0x45E860
void Plant::SetSleeping(std::int64_t ownerNowNs, std::int64_t animationNowNs, bool theIsAsleep)
{
    if (mIsAsleep == theIsAsleep || NotOnGround())
        return;

    mIsAsleep = theIsAsleep;
    if (theIsAsleep)
    {
        float aPosX = (mX) + 50.0f;
        float aPosY = (mY) + 40.0f;
        if (mSeedType == SeedType::SEED_FUMESHROOM)
            aPosX += 12.0f;
        else if (mSeedType == SeedType::SEED_SCAREDYSHROOM)
            aPosY -= 20.0f;
        else if (mSeedType == SeedType::SEED_GLOOMSHROOM)
            aPosY -= 12.0f;

        Reanimation* aSleepReanim = mApp->AddReanimation(aPosX, aPosY, mRenderOrder + 2, ReanimationType::REANIM_SLEEPING, animationNowNs);

        aSleepReanim->mIsAttachment = true;
        aSleepReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
        aSleepReanim->ApplyRate(RandRangeFloat(6.0f, 8.0f));
        aSleepReanim->SeekRatio(RandRangeFloat(0.0f, 0.9f));
        mSleepingReanimID = mApp->ReanimationGetID(aSleepReanim);
    }
    else
    {
        mApp->RemoveReanimation(mSleepingReanimID);
        mSleepingReanimID = ReanimationID::REANIMATIONID_NULL;
    }

    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

    if (aBodyReanim == nullptr)
        return;

    if (theIsAsleep)
    {
        if (!IsInPlay() && mSeedType == SeedType::SEED_SUNSHROOM)
        {
            aBodyReanim->SetFramesForLayer(ReanimTrackId::anim_bigsleep);
        }
        else if (aBodyReanim->TrackExists(ReanimTrackId::anim_sleep))
        {
            const auto aAnimPhase = aBodyReanim->SamplePlayback().phaseNs;
            const auto aAnimPeriod = aBodyReanim->mPeriodNs;
            aBodyReanim->StartBlend(20);
            aBodyReanim->SetFramesForLayer(ReanimTrackId::anim_sleep);
            aBodyReanim->Seek(aAnimPhase, aAnimPeriod);
        }
        else
        {
            aBodyReanim->ApplyRate(1.0f);
        }

        EndBlink();
    }
    else
    {
        if (!IsInPlay() && mSeedType == SeedType::SEED_SUNSHROOM)
        {
            aBodyReanim->SetFramesForLayer(ReanimTrackId::anim_bigidle);
        }
        else if (aBodyReanim->TrackExists(ReanimTrackId::anim_idle))
        {
            const auto aAnimPhase = aBodyReanim->SamplePlayback().phaseNs;
            const auto aAnimPeriod = aBodyReanim->mPeriodNs;
            aBodyReanim->StartBlend(20);
            aBodyReanim->SetFramesForLayer(ReanimTrackId::anim_idle);
            aBodyReanim->Seek(aAnimPhase, aAnimPeriod);
        }

        if (aBodyReanim->mAnimRate < 2.0f && IsInPlay())
            aBodyReanim->ApplyRate(RandRangeFloat(10.0f, 15.0f));
    }
}

//0x45EB10
int Plant::GetDamageRangeFlags(PlantWeapon thePlantWeapon)
{
	// 伤害范围标志：按 SeedType × PlantWeapon（0=主 1=副）双列直查，行序 = 枚举序。
	// 原 switch 的 CACTUS 武器条件支与 default=1 已并入表（未列出类型 {1,1}）。
	static constexpr uint8_t kDamageRangeFlags[][2] = {
		/* SEED_PEASHOOTER        */ { 1, 1 },
		/* SEED_SUNFLOWER         */ { 1, 1 },
		/* SEED_CHERRYBOMB        */ { 127, 127 },
		/* SEED_WALLNUT           */ { 1, 1 },
		/* SEED_POTATOMINE        */ { 77, 77 },
		/* SEED_SNOWPEA           */ { 1, 1 },
		/* SEED_CHOMPER           */ { 9, 9 },
		/* SEED_REPEATER          */ { 1, 1 },
		/* SEED_PUFFSHROOM        */ { 9, 9 },
		/* SEED_SUNSHROOM         */ { 1, 1 },
		/* SEED_FUMESHROOM        */ { 9, 9 },
		/* SEED_GRAVEBUSTER       */ { 1, 1 },
		/* SEED_HYPNOSHROOM       */ { 1, 1 },
		/* SEED_SCAREDYSHROOM     */ { 1, 1 },
		/* SEED_ICESHROOM         */ { 1, 1 },
		/* SEED_DOOMSHROOM        */ { 127, 127 },
		/* SEED_LILYPAD           */ { 1, 1 },
		/* SEED_SQUASH            */ { 13, 13 },
		/* SEED_THREEPEATER       */ { 1, 1 },
		/* SEED_TANGLEKELP        */ { 5, 5 },
		/* SEED_JALAPENO          */ { 127, 127 },
		/* SEED_SPIKEWEED         */ { 1, 1 },
		/* SEED_TORCHWOOD         */ { 1, 1 },
		/* SEED_TALLNUT           */ { 1, 1 },
		/* SEED_SEASHROOM         */ { 9, 9 },
		/* SEED_PLANTERN          */ { 1, 1 },
		/* SEED_CACTUS            */ { 2, 1 },
		/* SEED_BLOVER            */ { 1, 1 },
		/* SEED_SPLITPEA          */ { 1, 1 },
		/* SEED_STARFRUIT         */ { 1, 1 },
		/* SEED_PUMPKINSHELL      */ { 1, 1 },
		/* SEED_MAGNETSHROOM      */ { 1, 1 },
		/* SEED_CABBAGEPULT       */ { 13, 13 },
		/* SEED_FLOWERPOT         */ { 1, 1 },
		/* SEED_KERNELPULT        */ { 13, 13 },
		/* SEED_INSTANT_COFFEE    */ { 1, 1 },
		/* SEED_GARLIC            */ { 1, 1 },
		/* SEED_UMBRELLA          */ { 1, 1 },
		/* SEED_MARIGOLD          */ { 1, 1 },
		/* SEED_MELONPULT         */ { 13, 13 },
		/* SEED_GATLINGPEA        */ { 1, 1 },
		/* SEED_TWINSUNFLOWER     */ { 1, 1 },
		/* SEED_GLOOMSHROOM       */ { 9, 9 },
		/* SEED_CATTAIL           */ { 11, 11 },
		/* SEED_WINTERMELON       */ { 13, 13 },
		/* SEED_GOLD_MAGNET       */ { 1, 1 },
		/* SEED_SPIKEROCK         */ { 1, 1 },
		/* SEED_COBCANNON         */ { 127, 127 },
		/* SEED_IMITATER          */ { 1, 1 },
		/* SEED_EXPLODE_O_NUT     */ { 1, 1 },
		/* SEED_GIANT_WALLNUT     */ { 17, 17 },
		/* SEED_SPROUT            */ { 1, 1 },
		/* SEED_LEFTPEATER        */ { 1, 1 },
	};
	static_assert(sizeof(kDamageRangeFlags) / sizeof(kDamageRangeFlags[0]) == (int)SeedType::NUM_SEED_TYPES,
		"伤害范围槽位须与 SeedType 枚举同步");
	return kDamageRangeFlags[mSeedType][(int)thePlantWeapon];
}
//0x45EBD0
bool Plant::IsOnHighGround()
{
    return mBoard && mBoard->mGridSquareType[mPlantCol][mRow] == GridSquareType::GRIDSQUARE_HIGH_GROUND;
}

//0x45EC00
void Plant::SpikeRockTakeDamage(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);


    SpikeweedAttack(ownerNowNs);

    mPlantHealth -= 50;
    if (mPlantHealth <= 300)
    {
        aBodyReanim->AssignRenderGroupToTrack(ReanimTrackId::bigspike3, RENDER_GROUP_HIDDEN);
    }
    if (mPlantHealth <= 150)
    {
        aBodyReanim->AssignRenderGroupToTrack(ReanimTrackId::bigspike2, RENDER_GROUP_HIDDEN);
    }
    if (mPlantHealth <= 0)
    {
        mApp->PlayFoley(FoleyType::FOLEY_SQUISH);
        Die(ownerNowNs, visualNowNs, animationNowNs);
    }
}

bool Plant::IsSpiky()
{
    // 2026-10-04 标志字表化：裸 mSeedType 判定保留（模仿者地刺不认=原语义）
    return (PlantFlagsOf(mSeedType) & PlantFlags::Spiky) != 0;
}

//0x45ED00
void Plant::DoRowAreaDamage(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theDamage, unsigned int theDamageFlags)
{
    int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
    Rect aAttackRect = GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);

    Zombie* aZombie = nullptr;
    while (mBoard->IterateZombies(aZombie))
    {
        int aDiffY = (((ZombieFlagsOf(aZombie->mZombieType) & ZombieFlags::Boss) != 0)) ? 0 : (aZombie->mRow - mRow);
        if (mSeedType == SeedType::SEED_GLOOMSHROOM)
        {
            if (aDiffY < -1 || aDiffY > 1)
                continue;
        }
        else if (aDiffY)
            continue;

        if (aZombie->mOnHighGround == IsOnHighGround() && aZombie->EffectedByDamage(aDamageRangeFlags))
        {
            Rect aZombieRect = aZombie->GetZombieRect();
            if (GetRectOverlap(aAttackRect, aZombieRect) > 0)
            {
                int aDamage = theDamage;
                if ((aZombie->mZombieType == ZombieType::ZOMBIE_ZAMBONI || aZombie->mZombieType == ZombieType::ZOMBIE_CATAPULT) &&
                    (TestBit(theDamageFlags, DamageFlags::DAMAGE_SPIKE)))
                {
                    aDamage = 1800;

                    if (mSeedType == SeedType::SEED_SPIKEROCK)
                    {
                        SpikeRockTakeDamage(ownerNowNs, visualNowNs, animationNowNs);
                    }
                    else
                    {
                        Die(ownerNowNs, visualNowNs, animationNowNs);
                    }
                }

                aZombie->TakeDamage(ownerNowNs, visualNowNs, animationNowNs, aDamage, theDamageFlags);
                mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
            }
        }
    }
}

//0x45EEA0
TodParticleSystem* Plant::AddAttachedParticle(std::int64_t ownerNowNs, int thePosX, int thePosY, int theRenderPosition, ParticleEffect theEffect)
{
    TodParticleSystem* aParticle = mApp->ParticleTryToGet(mParticleID);
    if (aParticle)
        aParticle->ParticleSystemDie();

    TodParticleSystem* aNewParticle = mApp->AddTodParticle(thePosX, thePosY, theRenderPosition, theEffect, ownerNowNs);
    if (aNewParticle)
        mParticleID = mApp->ParticleGetID(aNewParticle);

    return aNewParticle;
}

//0x45EF10
bool Plant::FindTargetAndFire(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theRow, PlantWeapon thePlantWeapon)
{
    Zombie* aZombie = FindTargetZombie(theRow, thePlantWeapon);
    if (aZombie == nullptr)
        return false;

    EndBlink();
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

    Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mHeadReanimID);


    if (mSeedType == SeedType::SEED_SPLITPEA && thePlantWeapon == PlantWeapon::WEAPON_SECONDARY)
    {
        Reanimation* aHeadReanim2 = mApp->ReanimationGet(mHeadReanimID2);

        aHeadReanim2->StartBlend(20);
        aHeadReanim2->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
        aHeadReanim2->ApplyRate(35.0f);
        aHeadReanim2->SetFramesForLayer(ReanimTrackId::anim_splitpea_shooting);
        mShootingEndTimeNs = ReanimClock::Add(ownerNowNs, 26 * PlantLaunchClock::Tick);
    }
    else if (aHeadReanim && aHeadReanim->TrackExists(ReanimTrackId::anim_shooting))
    {
        aHeadReanim->StartBlend(20);
        aHeadReanim->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
        aHeadReanim->ApplyRate(35.0f);
        aHeadReanim->SetFramesForLayer(ReanimTrackId::anim_shooting);

        mShootingEndTimeNs = ReanimClock::Add(ownerNowNs, 35 * PlantLaunchClock::Tick);
        if (mSeedType == SeedType::SEED_REPEATER || mSeedType == SeedType::SEED_SPLITPEA || mSeedType == SeedType::SEED_LEFTPEATER)
        {
            aHeadReanim->ApplyRate(45.0f);
            mShootingEndTimeNs = ReanimClock::Add(ownerNowNs, 26 * PlantLaunchClock::Tick);
        }
        else if (mSeedType == SeedType::SEED_GATLINGPEA)
        {
            aHeadReanim->ApplyRate(38.0f);
            mShootingEndTimeNs = ReanimClock::Add(ownerNowNs, 100 * PlantLaunchClock::Tick);
        }
    }
    else if (mState == PlantState::STATE_CACTUS_HIGH)
    {
        PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_shootinghigh, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 35.0f);
        mShootingEndTimeNs = ReanimClock::Add(ownerNowNs, 23 * PlantLaunchClock::Tick);
    }
    else if (mSeedType == SeedType::SEED_GLOOMSHROOM)
    {
        PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_shooting, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 14.0f);
        mShootingEndTimeNs = ReanimClock::Add(ownerNowNs, 200 * PlantLaunchClock::Tick);
    }
    else if (mSeedType == SeedType::SEED_CATTAIL)
    {
        PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_shooting, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 30.0f);
        mShootingEndTimeNs = ReanimClock::Add(ownerNowNs, 50 * PlantLaunchClock::Tick);
    }
    else if (aBodyReanim && aBodyReanim->TrackExists(ReanimTrackId::anim_shooting))
    {
        PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_shooting, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 35.0f);

        // 射击节奏（cs）：按 SeedType 槽位直查（行序 = 枚举序，未列出 = 29）。
        // 玉米粒炮 1/4 概率黄油弹道特判保留在表外。
        if (mSeedType == SeedType::SEED_KERNELPULT && Sexy::Rand(4) == 0)
        {
            aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
            aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Cornpult_butter, RENDER_GROUP_NORMAL);
            aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Cornpult_kernal, RENDER_GROUP_HIDDEN);
            mState = PlantState::STATE_KERNELPULT_BUTTER;
        }
        static constexpr uint8_t kShootingCounter[] = {
            29,  // SEED_PEASHOOTER
            29,  // SEED_SUNFLOWER
            29,  // SEED_CHERRYBOMB
            29,  // SEED_WALLNUT
            29,  // SEED_POTATOMINE
            29,  // SEED_SNOWPEA
            29,  // SEED_CHOMPER
            29,  // SEED_REPEATER
            29,  // SEED_PUFFSHROOM
            29,  // SEED_SUNSHROOM
            50,  // SEED_FUMESHROOM
            29,  // SEED_GRAVEBUSTER
            29,  // SEED_HYPNOSHROOM
            25,  // SEED_SCAREDYSHROOM
            29,  // SEED_ICESHROOM
            29,  // SEED_DOOMSHROOM
            29,  // SEED_LILYPAD
            29,  // SEED_SQUASH
            29,  // SEED_THREEPEATER
            29,  // SEED_TANGLEKELP
            29,  // SEED_JALAPENO
            29,  // SEED_SPIKEWEED
            29,  // SEED_TORCHWOOD
            29,  // SEED_TALLNUT
            29,  // SEED_SEASHROOM
            29,  // SEED_PLANTERN
            35,  // SEED_CACTUS
            29,  // SEED_BLOVER
            29,  // SEED_SPLITPEA
            29,  // SEED_STARFRUIT
            29,  // SEED_PUMPKINSHELL
            29,  // SEED_MAGNETSHROOM
            32,  // SEED_CABBAGEPULT
            29,  // SEED_FLOWERPOT
            30,  // SEED_KERNELPULT
            29,  // SEED_INSTANT_COFFEE
            29,  // SEED_GARLIC
            29,  // SEED_UMBRELLA
            29,  // SEED_MARIGOLD
            36,  // SEED_MELONPULT
            29,  // SEED_GATLINGPEA
            29,  // SEED_TWINSUNFLOWER
            29,  // SEED_GLOOMSHROOM
            29,  // SEED_CATTAIL
            36,  // SEED_WINTERMELON
            29,  // SEED_GOLD_MAGNET
            29,  // SEED_SPIKEROCK
            29,  // SEED_COBCANNON
            29,  // SEED_IMITATER
            29,  // SEED_EXPLODE_O_NUT
            29,  // SEED_GIANT_WALLNUT
            29,  // SEED_SPROUT
            29,  // SEED_LEFTPEATER
        };
        static_assert(sizeof(kShootingCounter) / sizeof(kShootingCounter[0]) == (int)SeedType::NUM_SEED_TYPES,
            "射击节奏槽位须与 SeedType 枚举同步");
        mShootingEndTimeNs = ReanimClock::Add(ownerNowNs, kShootingCounter[mSeedType] * PlantLaunchClock::Tick);
    }
    else
        Fire(ownerNowNs, visualNowNs, animationNowNs, aZombie, theRow, thePlantWeapon);

    return true;
}

//0x45F2A0
void Plant::LaunchThreepeater(std::int64_t ownerNowNs)
{
    int rowAbove = mRow - 1;
    int rowBelow = mRow + 1;

    if ((FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY)) ||
        (mBoard->RowCanHaveZombies(rowAbove) && FindTargetZombie(rowAbove, PlantWeapon::WEAPON_PRIMARY)) ||
        (mBoard->RowCanHaveZombies(rowBelow) && FindTargetZombie(rowBelow, PlantWeapon::WEAPON_PRIMARY)))
    {
        Reanimation* aHeadReanim1 = mApp->ReanimationGet(mHeadReanimID);

        Reanimation* aHeadReanim2 = mApp->ReanimationGet(mHeadReanimID2);

        Reanimation* aHeadReanim3 = mApp->ReanimationGet(mHeadReanimID3);


        if (mBoard->RowCanHaveZombies(rowBelow))
        {
            aHeadReanim1->StartBlend(10);
            aHeadReanim1->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
            aHeadReanim1->ApplyRate(20.0f);
            aHeadReanim1->SetFramesForLayer(ReanimTrackId::anim_shooting1);
        }

        aHeadReanim2->StartBlend(10);
        aHeadReanim2->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
        aHeadReanim2->ApplyRate(20.0f);
        aHeadReanim2->SetFramesForLayer(ReanimTrackId::anim_shooting2);

        if (mBoard->RowCanHaveZombies(rowAbove))
        {
            aHeadReanim3->StartBlend(10);
            aHeadReanim3->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
            aHeadReanim3->ApplyRate(20.0f);
            aHeadReanim3->SetFramesForLayer(ReanimTrackId::anim_shooting3);
        }

        mShootingEndTimeNs = ReanimClock::Add(ownerNowNs, 35 * PlantLaunchClock::Tick);
    }
}

//0x45F470
bool Plant::FindStarFruitTarget(std::int64_t ownerNowNs)
{
    if (mRecentlyEatenEndTimeNs > ownerNowNs)
        return true;

    int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
    int aCenterStarX = (mX) + 40;
    int aCenterStarY = (mY) + 40;

    Zombie* aZombie = nullptr;
    while (mBoard->IterateZombies(aZombie))
    {
        Rect aZombieRect = aZombie->GetZombieRect();
        if (aZombie->EffectedByDamage(aDamageRangeFlags))
        {
            if (((ZombieFlagsOf(aZombie->mZombieType) & ZombieFlags::Boss) != 0) && mPlantCol >= 5)
                return true;

            if (aZombie->mRow == mRow)
            {
                if (aZombieRect.mX + aZombieRect.mWidth < aCenterStarX)
                    return true;
            }
            else
            {
                if (aZombie->mZombieType == ZombieType::ZOMBIE_DIGGER)
                    aZombieRect.mWidth += 10;

                float aProjectileTime = Distance2D(aCenterStarX, aCenterStarY, aZombieRect.mX + aZombieRect.mWidth / 2, aZombieRect.mY + aZombieRect.mHeight / 2) / 3.33f;
                int aZombieHitX = aZombie->ZombieTargetLeadX(ownerNowNs, aProjectileTime) - aZombieRect.mWidth / 2;
                if ((aZombieHitX + aZombieRect.mWidth > aCenterStarX) && (aZombieHitX < aCenterStarX))
                    return true;

                int aCenterZombieX = aZombieHitX + aZombieRect.mWidth / 2;
                int aCenterZombieY = aZombieRect.mY + aZombieRect.mHeight / 2;
                float angle = RAD_TO_DEG(atan2(aCenterZombieY - aCenterStarY, aCenterZombieX - aCenterStarX));
                if (abs(aZombie->mRow - mRow) < 2)
                {
                    if ((angle > 20.0f && angle < 40.0f) || (angle < -25.0f && angle > -45.0f))
                        return true;
                }
                else
                {
                    if ((angle > 25.0f && angle < 35.0f) || (angle < -28.0f && angle > -38.0f))
                        return true;
                }
            }
        }
    }

    return false;
}

//0x45F6E0
void Plant::LaunchStarFruit(std::int64_t ownerNowNs)
{
    if (FindStarFruitTarget(ownerNowNs))
    {
        PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_shoot, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 28.0f);
        mShootingEndTimeNs = ReanimClock::Add(ownerNowNs, 40 * PlantLaunchClock::Tick);
    }
}

//0x45F720
void Plant::StarFruitFire()
{
    mApp->PlayFoley(FoleyType::FOLEY_THROW);

    float aShootAngleX = cos(DEG_TO_RAD(30.0f)) * 3.33f;
    float aShootAngleY = sin(DEG_TO_RAD(30.0f)) * 3.33f;
    for (int i = 0; i < 5; i++)
    {
        Projectile* aProjectile = mBoard->AddProjectile((mX) + 25, (mY) + 25, mRenderOrder - 1, mRow, ProjectileType::PROJECTILE_STAR);
        aProjectile->mDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
        aProjectile->mMotionType = ProjectileMotion::MOTION_STAR;

        switch (i)
        {
        case 0:     aProjectile->mVelX = -3.33f;         aProjectile->mVelY = 0.0f;             break;
        case 1:     aProjectile->mVelX = 0.0f;          aProjectile->mVelY = 3.33f;             break;
        case 2:     aProjectile->mVelX = 0.0f;          aProjectile->mVelY = -3.33f;            break;
        case 3:     aProjectile->mVelX = aShootAngleX;  aProjectile->mVelY = aShootAngleY;      break;
        case 4:     aProjectile->mVelX = aShootAngleX;  aProjectile->mVelY = -aShootAngleY;     break;
        default:    TOD_ASSERT(false);                                                               break;
        }
    }
}

//0x45F8A0
void Plant::UpdateShooter(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    mLaunchClockActive = true;
    const auto aNow = ownerNowNs;
    auto aWindowStart = mLaunchWindowStartNs;
    const auto aSecondaryFire = [&](std::int64_t aWindowEnd)
    {
        if (mSeedType == SeedType::SEED_CATTAIL &&
            PlantLaunchClock::Crossed(aWindowStart, aWindowEnd,
                mNextLaunchTimeNs - 50 * PlantLaunchClock::Tick))
            FindTargetAndFire(ownerNowNs, visualNowNs, animationNowNs, mRow, PlantWeapon::WEAPON_PRIMARY);
        const auto aFirstShotStart = mNextLaunchTimeNs - 25 * PlantLaunchClock::Tick;
        if (PlantLaunchClock::Crossed(aWindowStart, aWindowEnd, aFirstShotStart))
        {
            bool aStartedShooting = false;
            if (mSeedType == SeedType::SEED_REPEATER || mSeedType == SeedType::SEED_LEFTPEATER)
                aStartedShooting = FindTargetAndFire(ownerNowNs, visualNowNs, animationNowNs, mRow, PlantWeapon::WEAPON_PRIMARY);
            else if (mSeedType == SeedType::SEED_SPLITPEA)
            {
                aStartedShooting = FindTargetAndFire(ownerNowNs, visualNowNs, animationNowNs, mRow, PlantWeapon::WEAPON_PRIMARY);
                aStartedShooting |= FindTargetAndFire(ownerNowNs, visualNowNs, animationNowNs, mRow, PlantWeapon::WEAPON_SECONDARY);
            }
            // The first shot starts at its deadline inside this update, not
            // at the update endpoint. Otherwise the next trigger can reset
            // its 26-unit timer before it crosses the one-unit fire event.
            if (aStartedShooting)
                mShootingEndTimeNs = ReanimClock::Add(mShootingEndTimeNs, aFirstShotStart - aNow);
        }
    };
    int aEvents = 0;
    while (mNextLaunchTimeNs <= aNow && aEvents < PlantLaunchClock::MaxEventsPerUpdate)
    {
        const auto aDue = mNextLaunchTimeNs;
        aSecondaryFire(aDue);
        // The launch deadline is independent of the shooting animation timer.
        // In particular, Gloom's next volley starts before its 200-tick timer
        // finishes. Gating on that timer silently discards every other volley.
        mNextLaunchTimeNs = PlantLaunchClock::Add(aDue,
            PlantLaunchClock::Uniform(mLaunchRate.mNanoseconds - 14 * PlantLaunchClock::Tick,
                mLaunchRate.mNanoseconds, [] { return Sexy::Rand(); }));

        // C 形态 -> 匿名 lambda 函数指针表 kShooterFireCases（仿 kSeedTypeUpdates
        // 范式）：行序 = SeedType 枚举序 0..52；无 case 槽 = 原 else 支（正面
        // 开火，constexpr 指针变量填满）。SPLITPEA/CACTUS 两支体积较大整体入槽。
        static constexpr auto kShooterDefaultFire = [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p)
        {
            p->FindTargetAndFire(ownerNowNs, visualNowNs, animationNowNs, p->mRow, PlantWeapon::WEAPON_PRIMARY);
        };
        static constexpr void (*kShooterFireCases[])(std::int64_t, std::int64_t, std::int64_t, Plant*) = {
			/* SEED_PEASHOOTER        豌豆射手     */ kShooterDefaultFire,
			/* SEED_SUNFLOWER         向日葵      */ kShooterDefaultFire,
			/* SEED_CHERRYBOMB        樱桃炸弹     */ kShooterDefaultFire,
			/* SEED_WALLNUT           坚果       */ kShooterDefaultFire,
			/* SEED_POTATOMINE        土豆地雷     */ kShooterDefaultFire,
			/* SEED_SNOWPEA           寒冰射手     */ kShooterDefaultFire,
			/* SEED_CHOMPER           大嘴花      */ kShooterDefaultFire,
			/* SEED_REPEATER          双发射手     */ kShooterDefaultFire,
			/* SEED_PUFFSHROOM        小喷菇      */ kShooterDefaultFire,
			/* SEED_SUNSHROOM         阳光菇      */ kShooterDefaultFire,
			/* SEED_FUMESHROOM        大喷菇      */ kShooterDefaultFire,
			/* SEED_GRAVEBUSTER       墓碑吞噬者    */ kShooterDefaultFire,
			/* SEED_HYPNOSHROOM       魅惑菇      */ kShooterDefaultFire,
			/* SEED_SCAREDYSHROOM     胆小菇      */ kShooterDefaultFire,
			/* SEED_ICESHROOM         寒冰菇      */ kShooterDefaultFire,
			/* SEED_DOOMSHROOM        毁灭菇      */ kShooterDefaultFire,
			/* SEED_LILYPAD           睡莲       */ kShooterDefaultFire,
			/* SEED_SQUASH            窝瓜       */ kShooterDefaultFire,
			/* SEED_THREEPEATER       三线射手     */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->LaunchThreepeater(ownerNowNs); },
			/* SEED_TANGLEKELP        缠绕海草     */ kShooterDefaultFire,
			/* SEED_JALAPENO          火爆辣椒     */ kShooterDefaultFire,
			/* SEED_SPIKEWEED         地刺       */ kShooterDefaultFire,
			/* SEED_TORCHWOOD         火炬树桩     */ kShooterDefaultFire,
			/* SEED_TALLNUT                    */ kShooterDefaultFire,
			/* SEED_SEASHROOM                  */ kShooterDefaultFire,
			/* SEED_PLANTERN                   */ kShooterDefaultFire,
			/* SEED_CACTUS                     */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p)
            {
                if (p->mState == PlantState::STATE_CACTUS_HIGH)
                {
                    p->FindTargetAndFire(ownerNowNs, visualNowNs, animationNowNs, p->mRow, PlantWeapon::WEAPON_PRIMARY);
                }
                else if (p->mState == PlantState::STATE_CACTUS_LOW)
                {
                    p->FindTargetAndFire(ownerNowNs, visualNowNs, animationNowNs, p->mRow, PlantWeapon::WEAPON_SECONDARY);
                }
            },
			/* SEED_BLOVER                     */ kShooterDefaultFire,
			/* SEED_SPLITPEA                   */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p)
            {
                p->FindTargetAndFire(ownerNowNs, visualNowNs, animationNowNs, p->mRow, PlantWeapon::WEAPON_SECONDARY);

                // 1073 00463200: the second back shot must not reuse the front
                // head's previous shooting state. Keep the 25-tick front trigger.
                Reanimation* aHeadReanim = p->mApp->ReanimationGet(p->mHeadReanimID);

                Reanimation* aBodyReanim = p->mApp->ReanimationGet(p->mBodyReanimID);

                aHeadReanim->StartBlend(20);
                aHeadReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
                aHeadReanim->SetFramesForLayer(ReanimTrackId::anim_head_idle);
                aHeadReanim->ApplyRate(aBodyReanim->mAnimRate);
                aHeadReanim->SynchronizePhase(*aBodyReanim);
            },
			/* SEED_STARFRUIT                  */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->LaunchStarFruit(ownerNowNs); },
			/* SEED_PUMPKINSHELL               */ kShooterDefaultFire,
			/* SEED_MAGNETSHROOM               */ kShooterDefaultFire,
			/* SEED_CABBAGEPULT                */ kShooterDefaultFire,
			/* SEED_FLOWERPOT                  */ kShooterDefaultFire,
			/* SEED_KERNELPULT                 */ kShooterDefaultFire,
			/* SEED_INSTANT_COFFEE             */ kShooterDefaultFire,
			/* SEED_GARLIC                     */ kShooterDefaultFire,
			/* SEED_UMBRELLA                   */ kShooterDefaultFire,
			/* SEED_MARIGOLD                   */ kShooterDefaultFire,
			/* SEED_MELONPULT                  */ kShooterDefaultFire,
			/* SEED_GATLINGPEA                 */ kShooterDefaultFire,
			/* SEED_TWINSUNFLOWER              */ kShooterDefaultFire,
			/* SEED_GLOOMSHROOM                */ kShooterDefaultFire,
			/* SEED_CATTAIL                    */ kShooterDefaultFire,
			/* SEED_WINTERMELON                */ kShooterDefaultFire,
			/* SEED_GOLD_MAGNET                */ kShooterDefaultFire,
			/* SEED_SPIKEROCK                  */ kShooterDefaultFire,
			/* SEED_COBCANNON                  */ kShooterDefaultFire,
			/* SEED_IMITATER                   */ kShooterDefaultFire,
			/* SEED_EXPLODE_O_NUT              */ kShooterDefaultFire,
			/* SEED_GIANT_WALLNUT              */ kShooterDefaultFire,
			/* SEED_SPROUT                     */ kShooterDefaultFire,
			/* SEED_LEFTPEATER                 */ kShooterDefaultFire,
        };
        static_assert(sizeof(kShooterFireCases) / sizeof(kShooterFireCases[0]) == (int)SeedType::NUM_SEED_TYPES,
            "射手开火槽位须与 SeedType 枚举同步");

        if ((unsigned)mSeedType < sizeof(kShooterFireCases) / sizeof(kShooterFireCases[0]))
        {
            kShooterFireCases[mSeedType](ownerNowNs, visualNowNs, animationNowNs, this);
        }
        else
        {
            kShooterDefaultFire(ownerNowNs, visualNowNs, animationNowNs, this);  // 原 else 支（哨兵越界等价）
        }
        aWindowStart = aDue;
        ++aEvents;
    }
    if (mNextLaunchTimeNs > aNow)
    {
        aSecondaryFire(aNow);
        aWindowStart = aNow;
    }
    // Preserve a bounded backlog instead of dropping overdue periods.
    mLaunchWindowStartNs = aWindowStart;
}

//0x45F980
bool Plant::MakesSun()
{
    return mSeedType == SeedType::SEED_SUNFLOWER || mSeedType == SeedType::SEED_TWINSUNFLOWER || mSeedType == SeedType::SEED_SUNSHROOM;
}

//0x45F9A0
void Plant::UpdateProductionPlant(std::int64_t ownerNowNs)
{
    if (!IsInPlay() || mApp->IsIZombieLevel() || mApp->mGameMode == GameMode::GAMEMODE_INTRO)
        return;

    if (mBoard->HasLevelAwardDropped())
        return;

    if (mSeedType == SeedType::SEED_MARIGOLD && mBoard->mCurrentWave == mBoard->mNumWaves)
    {
        if (mState != PlantState::STATE_MARIGOLD_ENDING)
        {
            mState = PlantState::STATE_MARIGOLD_ENDING;
            mStateCountdown = 6000;
        }
        else if (mStateCountdown <= 0)
            return;
    }

    if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND && mBoard->mChallenge->mChallengeState != ChallengeState::STATECHALLENGE_LAST_STAND_ONSLAUGHT)
        return;

    mLaunchClockActive = true;
    const auto aNow = ownerNowNs;
    const auto aRemaining = mNextLaunchTimeNs - aNow;
    if (aRemaining <= 100 * PlantLaunchClock::Tick)
    {
        int aFlashCountdown = static_cast<int>(std::clamp<std::int64_t>(
            100 - aRemaining / PlantLaunchClock::Tick, 0, 100));
        mEatenFlashEndTimeNs = std::max(mEatenFlashEndTimeNs, ownerNowNs + static_cast<std::int64_t>(aFlashCountdown) * 10'000'000);
    }
    int aEvents = 0;
    while (mNextLaunchTimeNs <= aNow && aEvents++ < PlantLaunchClock::MaxEventsPerUpdate)
    {
        mNextLaunchTimeNs = PlantLaunchClock::Add(mNextLaunchTimeNs,
            PlantLaunchClock::Uniform(mLaunchRate.mNanoseconds - 150 * PlantLaunchClock::Tick,
                mLaunchRate.mNanoseconds, [] { return Sexy::Rand(); }));
        mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);

        // B 形态 -> 常量表 kProductionCoins（原 4 支链合并）：行序 = SeedType
        // 枚举序 0..52；mPick=NONE = 无 case 槽（不产币）。币种二选一的两支
        // 编码为行内选择位：SUNSHROOM 按 STATE_SUNSHROOM_SMALL 体型分支
        //（SMALLSUN/SUN），MARIGOLD 按 Sexy::Rand(100)<10 分支（GOLD/SILVER，
        // 仅此槽消耗一次 RNG，与原链抽签次序一致）；TWINSUNFLOWER 同值双槽
        // 产两枚。GAMEMODE_LIMBO_BIG_TIME 加成链判别式混排（mGameMode
        // 前置），表外保留。
        enum class ProductionCoinPick : unsigned char
        {
            NONE,        // 无 case 槽
            FIXED,       // 恒 mCoinA
            SMALL_STATE, // SUNSHROOM：STATE_SUNSHROOM_SMALL 取 mCoinA 否则 mCoinB
            RANDOM10     // MARIGOLD：Sexy::Rand(100)<10 取 mCoinA 否则 mCoinB
        };
        static constexpr struct
        {
            ProductionCoinPick mPick;
            CoinType mCoinA;
            CoinType mCoinB;
            CoinMotion mMotion;
            int mCount;
        } kProductionCoins[(int)SeedType::NUM_SEED_TYPES] = {
            /* SEED_PEASHOOTER      */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_SUNFLOWER       */ { ProductionCoinPick::FIXED, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 1 },
            /* SEED_CHERRYBOMB      */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_WALLNUT         */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_POTATOMINE      */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_SNOWPEA         */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_CHOMPER         */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_REPEATER        */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_PUFFSHROOM      */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_SUNSHROOM       */ { ProductionCoinPick::SMALL_STATE, CoinType::COIN_SMALLSUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 1 },
            /* SEED_FUMESHROOM      */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_GRAVEBUSTER     */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_HYPNOSHROOM     */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_SCAREDYSHROOM   */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_ICESHROOM       */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_DOOMSHROOM      */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_LILYPAD         */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_SQUASH          */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_THREEPEATER     */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_TANGLEKELP      */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_JALAPENO        */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_SPIKEWEED       */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_TORCHWOOD       */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_TALLNUT         */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_SEASHROOM       */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_PLANTERN        */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_CACTUS          */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_BLOVER          */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_SPLITPEA        */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_STARFRUIT       */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_PUMPKINSHELL    */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_MAGNETSHROOM    */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_CABBAGEPULT     */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_FLOWERPOT       */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_KERNELPULT      */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_INSTANT_COFFEE  */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_GARLIC          */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_UMBRELLA        */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_MARIGOLD        */ { ProductionCoinPick::RANDOM10, CoinType::COIN_GOLD, CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN, 1 },
            /* SEED_MELONPULT       */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_GATLINGPEA      */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_TWINSUNFLOWER   */ { ProductionCoinPick::FIXED, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 2 },
            /* SEED_GLOOMSHROOM     */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_CATTAIL         */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_WINTERMELON     */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_GOLD_MAGNET     */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_SPIKEROCK       */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_COBCANNON       */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_IMITATER        */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_EXPLODE_O_NUT   */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_GIANT_WALLNUT   */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_SPROUT          */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
            /* SEED_LEFTPEATER      */ { ProductionCoinPick::NONE, CoinType::COIN_SUN, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT, 0 },
        };
        static_assert(sizeof(kProductionCoins) / sizeof(kProductionCoins[0]) == (int)SeedType::NUM_SEED_TYPES,
            "产币槽位须与 SeedType 枚举同步");

        if ((unsigned)mSeedType < sizeof(kProductionCoins) / sizeof(kProductionCoins[0]))
        {
            const auto& aRow = kProductionCoins[mSeedType];
            if (aRow.mPick != ProductionCoinPick::NONE)
            {
                CoinType aCoin = aRow.mCoinA;
                if (aRow.mPick == ProductionCoinPick::SMALL_STATE)
                {
                    aCoin = (mState == PlantState::STATE_SUNSHROOM_SMALL) ? aRow.mCoinA : aRow.mCoinB;
                }
                else if (aRow.mPick == ProductionCoinPick::RANDOM10)
                {
                    aCoin = (Sexy::Rand(100) < 10) ? aRow.mCoinA : aRow.mCoinB;
                }
                for (int i = 0; i < aRow.mCount; i++)
                {
                    mBoard->AddCoin((mX), (mY), aCoin, aRow.mMotion);
                }
            }
        }

        if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_BIG_TIME)
        {
            if (mSeedType == SeedType::SEED_SUNFLOWER)
            {
                mBoard->AddCoin((mX), (mY), CoinType::COIN_SUN, CoinMotion::COIN_MOTION_FROM_PLANT);
            }
            else if (mSeedType == SeedType::SEED_MARIGOLD)
            {
                mBoard->AddCoin((mX), (mY), CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
            }
        }
    }
}

//0x45FB70
void Plant::UpdateSunShroom(std::int64_t ownerNowNs)
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

    if (mState == PlantState::STATE_SUNSHROOM_SMALL)
    {
        if (mStateCountdown == 0)
        {
            PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_grow, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 12.0f);
            mState = PlantState::STATE_SUNSHROOM_GROWING;
            mApp->PlayFoley(FoleyType::FOLEY_PLANTGROW);
        }

        UpdateProductionPlant(ownerNowNs);
    }
    else if (mState == PlantState::STATE_SUNSHROOM_GROWING)
    {
        if (aBodyReanim->mLoopCount > 0)
        {
            PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_bigidle, ReanimLoopType::REANIM_LOOP, 10, RandRangeFloat(12.0f, 15.0f));
            mState = PlantState::STATE_SUNSHROOM_BIG;
        }
    }
    else
    {
        UpdateProductionPlant(ownerNowNs);
    }
}

//0x45FC70
void Plant::UpdateGraveBuster(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (mState == PlantState::STATE_GRAVEBUSTER_LANDING)
    {
        if (mApp->ReanimationGet(mBodyReanimID)->mLoopCount > 0)
        {
            PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 10, 12.0f);
            mStateCountdown = 400;
            mState = PlantState::STATE_GRAVEBUSTER_EATING;
            AddAttachedParticle(ownerNowNs, (mX) + 40, (mY) + 40, mRenderOrder + 4, ParticleEffect::PARTICLE_GRAVE_BUSTER);
        }
    }
    else if (mState == PlantState::STATE_GRAVEBUSTER_EATING && mStateCountdown == 0)
    {
        GridItem* aGraveStone = mBoard->GetGraveStoneAt(mPlantCol, mRow);
        if (aGraveStone)
        {
            aGraveStone->GridItemDie();
            mBoard->mGravesCleared++;
        }

        mApp->AddTodParticle((mX) + 40, (mY) + 40, mRenderOrder + 4, ParticleEffect::PARTICLE_GRAVE_BUSTER_DIE, ownerNowNs);
        Die(ownerNowNs, visualNowNs, animationNowNs);
        mBoard->DropLootPiece((mX) + 40, (mY), 12);
    }
}

//0x45FD90
// GOTY @Patoke: 0x463760
void Plant::PlayBodyReanim(std::int64_t ownerNowNs, ReanimTrackId theTrackId, ReanimLoopType theLoopType, int theBlendTime, float theAnimRate)
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);


    if (theBlendTime > 0)
        aBodyReanim->StartBlend(theBlendTime);
    if (theAnimRate > 0.0f)
        aBodyReanim->ApplyRate(theAnimRate);

    aBodyReanim->ChangeLoopType(theLoopType);
    aBodyReanim->mLoopCount = 0;
    aBodyReanim->SetFramesForLayer(theTrackId);
}

//0x45FE20
void Plant::UpdatePotato(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

    const auto setLightFrameCount = [animationNowNs](Reanimation& reanimation, int frameCount)
    {
        if (frameCount <= 0)
            throw std::invalid_argument("animation frame count must be positive");
        if (frameCount == reanimation.mFrameCount)
            return;
        const auto nowNs = animationNowNs;
        const auto oldPeriod = reanimation.mPeriodNs;
        const auto position = reanimation.SamplePlaybackAt(nowNs);
        const auto rootDisplacement = reanimation.mRootDisplacement;
        reanimation.mFrameCount = frameCount;
        const auto newPeriod = ReanimClock::PeriodFromRate(
            reanimation.mFrameCount, reanimation.mAnimRate);
        if (newPeriod <= 0)
            throw std::overflow_error("animation period below one nanosecond");
        const bool full = reanimation.mLoopType == REANIM_LOOP_FULL_LAST_FRAME ||
            reanimation.mLoopType == REANIM_PLAY_ONCE_FULL_LAST_FRAME ||
            reanimation.mLoopType == REANIM_PLAY_ONCE_FULL_LAST_FRAME_AND_HOLD;
        const int intervals = std::max(1, full ? frameCount : frameCount - 1);
        reanimation.mFramePeriodNs = std::max<std::int64_t>(1, newPeriod / intervals);
        reanimation.mPeriodNs = reanimation.mFramePeriodNs * intervals;
        reanimation.mRootFrameScale = full ? 1.0 : double(frameCount) / intervals;
        reanimation.Seek(position.phaseNs, oldPeriod);
        reanimation.mRootDisplacement = rootDisplacement;

    };

    if (mState == PlantState::STATE_NOTREADY)
    {
        if (mStateCountdown == 0)
        {
            mApp->AddTodParticle((mX) + mWidth / 2, (mY) + mHeight / 2, mRenderOrder, ParticleEffect::PARTICLE_POTATO_MINE_RISE, ownerNowNs);
            PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_rise, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 18.0f);
            mState = PlantState::STATE_POTATO_RISING;
            mApp->PlayFoley(FoleyType::FOLEY_DIRT_RISE);
        }
    }
    else if (mState == PlantState::STATE_POTATO_RISING)
    {
        if (aBodyReanim->mLoopCount > 0)
        {
            float aRate = RandRangeFloat(12.0f, 15.0f);
            PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_armed, ReanimLoopType::REANIM_LOOP, 0, aRate);

            Reanimation* aLightReanim = mApp->AddReanimation(0.0f, 0.0f, mRenderOrder + 2, GetPlantDefinition(mSeedType).mReanimationType, animationNowNs);

            aLightReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
            aLightReanim->ApplyRate(aRate - 2.0f);
            aLightReanim->SetFramesForLayer(ReanimTrackId::anim_glow);
            setLightFrameCount(*aLightReanim, 10);
            aLightReanim->ShowOnlyTrack(ReanimTrackId::anim_glow);
            aLightReanim->SetTruncateDisappearingFrames(ReanimTrackId::anim_glow, false);
            mLightReanimID = mApp->ReanimationGetID(aLightReanim);
            aLightReanim->AttachToAnotherReanimation(aBodyReanim, ReanimTrackId::anim_light);

            mState = PlantState::STATE_POTATO_ARMED;
            mBlinkCountdown = 400 + Sexy::Rand(4000);
        }
    }
    else if (mState == PlantState::STATE_POTATO_ARMED)
    {
        if (FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY))
        {
            DoSpecial(ownerNowNs, visualNowNs, animationNowNs);
        }
        else
        {
            Reanimation* aLightReanim = mApp->ReanimationTryToGet(mLightReanimID);

            if (aLightReanim)
            {
                setLightFrameCount(*aLightReanim,
                    TodAnimateCurve(200, 50, DistanceToClosestZombie(), 10, 3, Curves::Linear));
            }
        }
    }
}

//0x460060
void Plant::UpdateTanglekelp(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (mState != PlantState::STATE_TANGLEKELP_GRABBING)
    {
        Zombie* aZombie = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY);
        if (aZombie)
        {
            mApp->PlayFoley(FoleyType::FOLEY_FLOOP);
            mState = PlantState::STATE_TANGLEKELP_GRABBING;
            mStateCountdown = 100;
            aZombie->PoolSplash(ownerNowNs, visualNowNs, animationNowNs, false);

            float aVinesPosX = -13.0f;
            float aVinesPosY = 15.0f;
            if (aZombie->mZombieType == ZombieType::ZOMBIE_SNORKEL)
            {
                aVinesPosX = -43.0f;
                aVinesPosY = 55.0f;
            }
            if (aZombie->mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING)
            {
                aVinesPosX = -20.0f;
                aVinesPosY = 37.0f;
            }
            Reanimation* aGrabReanim = aZombie->AddAttachedReanim(ownerNowNs, visualNowNs, animationNowNs, aVinesPosX, aVinesPosY, ReanimationType::REANIM_TANGLEKELP);
            if (aGrabReanim)
            {
                aGrabReanim->SetFramesForLayer(ReanimTrackId::anim_grab);
                aGrabReanim->ApplyRate(24.0f);
                aGrabReanim->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
            }

            mTargetZombieID = mBoard->ZombieGetID(aZombie);
        }
    }
    else
    {
        if (mStateCountdown == 50)
        {
            Zombie* aZombie = mBoard->ZombieTryToGet(mTargetZombieID);
            if (aZombie)
            {
                aZombie->DragUnder(ownerNowNs);
                aZombie->PoolSplash(ownerNowNs, visualNowNs, animationNowNs, false);
            }
        }

        if (mStateCountdown == 20)
        {
            int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mRow, 0);
            Reanimation* aSplashReanim = mApp->AddReanimation((mX) - 23, (mY) + 7, aRenderPosition, ReanimationType::REANIM_SPLASH, visualNowNs);

            aSplashReanim->mOverlayMatrix.m00 = 1.3f;
            aSplashReanim->mOverlayMatrix.m11 = 1.3f;

            mApp->AddTodParticle((mX) + 31, (mY) + 64, aRenderPosition, ParticleEffect::PARTICLE_PLANTING_POOL, ownerNowNs);
            mApp->PlayFoley(FoleyType::FOLEY_ZOMBIE_ENTERING_WATER);
        }

        if (mStateCountdown == 0)
        {
            Die(ownerNowNs, visualNowNs, animationNowNs);

            Zombie* aZombie = mBoard->ZombieTryToGet(mTargetZombieID);
            if (aZombie)
            {
                aZombie->DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
            }
        }
    }
}

//0x460320
void Plant::SpikeweedAttack(std::int64_t ownerNowNs)
{
    TOD_ASSERT(IsSpiky());

    if (mState != PlantState::STATE_SPIKEWEED_ATTACKING)
    {
        PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_attack, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 18.0f);
        mApp->PlaySample(SOUND_THROW);

        mState = PlantState::STATE_SPIKEWEED_ATTACKING;
        mStateCountdown = 100;
    }
}

//0x460370
void Plant::UpdateSpikeweed(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

    if (mState == PlantState::STATE_SPIKEWEED_ATTACKING)
    {
        if (mStateCountdown == 0)
        {
            mState = PlantState::STATE_NOTREADY;
        }
        else if (mSeedType == SeedType::SEED_SPIKEROCK)
        {
            if (mStateCountdown == 70 || mStateCountdown == 32)
            {
                DoRowAreaDamage(ownerNowNs, visualNowNs, animationNowNs, 20, 33U);
            }
        }
        else if (mStateCountdown == 75)
        {
            DoRowAreaDamage(ownerNowNs, visualNowNs, animationNowNs, 20, 33U);
        }

        if (aBodyReanim->mLoopCount > 0)
        {
            PlayIdleAnim(ownerNowNs, RandRangeFloat(12.0f, 15.0f));
        }
    }
    else if (FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY))
    {
        SpikeweedAttack(ownerNowNs);
    }
}

//0x460420
void Plant::UpdateScaredyShroom(std::int64_t ownerNowNs)
{
    if (mShootingEndTimeNs != 0)
        return;

    bool aHasZombieNearby = false;

    Zombie* aZombie = nullptr;
    // 行段区间路径（2026-10-04 全量转换）：±1 行+Boss 行偏差归零；布尔
    // 判定（break 即真）与评估序无关
    while (mBoard->IterateZombiesInRowRange(mRow - 1, mRow + 1, aZombie))
    {
        Rect aZombieRect = aZombie->GetZombieRect();
        int aDiffY = (((ZombieFlagsOf(aZombie->mZombieType) & ZombieFlags::Boss) != 0)) ? 0 : (aZombie->mRow - mRow);
        if (!aZombie->mMindControlled && !aZombie->IsDeadOrDying() && aDiffY <= 1 && aDiffY >= -1 && GetCircleRectOverlap((mX), (mY) + 20.0f, 120, aZombieRect))
        {
            aHasZombieNearby = true;
            break;
        }
    }

    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

    // C 形态 -> 匿名 lambda 函数指针表 kScaredyStateCases（仿 kSeedTypeUpdates
    // 范式）：行序 = PlantState 枚举序 0..48（count_last=STATE_LILYPAD_
    // INVULNERABLE 锚定，无 NUM 哨兵）；4 支状态机分散（READY=1，LOWERING
    // ..RAISING=20..22）故取全宽表；nullptr = 无 case 槽（无操作 = 原链
    // 落空）。aBodyReanim 与 aHasZombieNearby 两个链前局部经参数传入。
    using ScaredyStateFn = void (*)(std::int64_t, Plant&, Reanimation*, bool);
    static constexpr ScaredyStateFn kScaredyStateCases[(int)PlantState::STATE_LILYPAD_INVULNERABLE + 1] = {
        nullptr,  // 0 STATE_NOTREADY（无 case）
        // 原 if STATE_READY
        [](std::int64_t ownerNowNs, Plant& p, Reanimation* aBodyReanim, bool aHasZombieNearby)
        {
            if (aHasZombieNearby)
            {
                p.mState = PlantState::STATE_SCAREDYSHROOM_LOWERING;
                p.PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_scared, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 10.0f);
            }
        },  // 1 STATE_READY
        nullptr,  // 2 STATE_DOINGSPECIAL（无 case）
        nullptr,  // 3 STATE_SQUASH_LOOK（无 case）
        nullptr,  // 4 STATE_SQUASH_PRE_LAUNCH（无 case）
        nullptr,  // 5 STATE_SQUASH_RISING（无 case）
        nullptr,  // 6 STATE_SQUASH_FALLING（无 case）
        nullptr,  // 7 STATE_SQUASH_DONE_FALLING（无 case）
        nullptr,  // 8 STATE_GRAVEBUSTER_LANDING（无 case）
        nullptr,  // 9 STATE_GRAVEBUSTER_EATING（无 case）
        nullptr,  // 10 STATE_CHOMPER_BITING（无 case）
        nullptr,  // 11 STATE_CHOMPER_BITING_GOT_ONE（无 case）
        nullptr,  // 12 STATE_CHOMPER_BITING_MISSED（无 case）
        nullptr,  // 13 STATE_CHOMPER_DIGESTING（无 case）
        nullptr,  // 14 STATE_CHOMPER_SWALLOWING（无 case）
        nullptr,  // 15 STATE_POTATO_RISING（无 case）
        nullptr,  // 16 STATE_POTATO_ARMED（无 case）
        nullptr,  // 17 STATE_POTATO_MASHED（无 case）
        nullptr,  // 18 STATE_SPIKEWEED_ATTACKING（无 case）
        nullptr,  // 19 STATE_SPIKEWEED_ATTACKING_2（无 case）
        // 原 else-if STATE_SCAREDYSHROOM_LOWERING
        [](std::int64_t ownerNowNs, Plant& p, Reanimation* aBodyReanim, bool aHasZombieNearby)
        {
            if (aBodyReanim->mLoopCount > 0)
            {
                p.mState = PlantState::STATE_SCAREDYSHROOM_SCARED;
                p.PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_scaredidle, ReanimLoopType::REANIM_LOOP, 10, 0.0f);
            }
        },  // 20 STATE_SCAREDYSHROOM_LOWERING
        // 原 else-if STATE_SCAREDYSHROOM_SCARED
        [](std::int64_t ownerNowNs, Plant& p, Reanimation* aBodyReanim, bool aHasZombieNearby)
        {
            if (!aHasZombieNearby)
            {
                p.mState = PlantState::STATE_SCAREDYSHROOM_RAISING;

                float aAnimRate = RandRangeFloat(7.0f, 12.0f);
                p.PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_grow, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, aAnimRate);
            }
        },  // 21 STATE_SCAREDYSHROOM_SCARED
        // 原 else-if STATE_SCAREDYSHROOM_RAISING
        [](std::int64_t ownerNowNs, Plant& p, Reanimation* aBodyReanim, bool aHasZombieNearby)
        {
            if (aBodyReanim->mLoopCount > 0)
            {
                p.mState = PlantState::STATE_READY;

                float aAnimRate = RandRangeFloat(10.0f, 15.0f);
                p.PlayIdleAnim(ownerNowNs, aAnimRate);
            }
        },  // 22 STATE_SCAREDYSHROOM_RAISING
        nullptr,  // 23 STATE_SUNSHROOM_SMALL（无 case）
        nullptr,  // 24 STATE_SUNSHROOM_GROWING（无 case）
        nullptr,  // 25 STATE_SUNSHROOM_BIG（无 case）
        nullptr,  // 26 STATE_MAGNETSHROOM_SUCKING（无 case）
        nullptr,  // 27 STATE_MAGNETSHROOM_CHARGING（无 case）
        nullptr,  // 28 STATE_BOWLING_UP（无 case）
        nullptr,  // 29 STATE_BOWLING_DOWN（无 case）
        nullptr,  // 30 STATE_CACTUS_LOW（无 case）
        nullptr,  // 31 STATE_CACTUS_RISING（无 case）
        nullptr,  // 32 STATE_CACTUS_HIGH（无 case）
        nullptr,  // 33 STATE_CACTUS_LOWERING（无 case）
        nullptr,  // 34 STATE_TANGLEKELP_GRABBING（无 case）
        nullptr,  // 35 STATE_COBCANNON_ARMING（无 case）
        nullptr,  // 36 STATE_COBCANNON_LOADING（无 case）
        nullptr,  // 37 STATE_COBCANNON_READY（无 case）
        nullptr,  // 38 STATE_COBCANNON_FIRING（无 case）
        nullptr,  // 39 STATE_KERNELPULT_BUTTER（无 case）
        nullptr,  // 40 STATE_UMBRELLA_TRIGGERED（无 case）
        nullptr,  // 41 STATE_UMBRELLA_REFLECTING（无 case）
        nullptr,  // 42 STATE_IMITATER_MORPHING（无 case）
        nullptr,  // 43 STATE_ZEN_GARDEN_WATERED（无 case）
        nullptr,  // 44 STATE_ZEN_GARDEN_NEEDY（无 case）
        nullptr,  // 45 STATE_ZEN_GARDEN_HAPPY（无 case）
        nullptr,  // 46 STATE_MARIGOLD_ENDING（无 case）
        nullptr,  // 47 STATE_FLOWERPOT_INVULNERABLE（无 case）
        nullptr,  // 48 STATE_LILYPAD_INVULNERABLE（无 case）
    };
    static_assert(sizeof(kScaredyStateCases) / sizeof(kScaredyStateCases[0]) ==
        (int)PlantState::STATE_LILYPAD_INVULNERABLE + 1, "kScaredyStateCases 槽位须与 PlantState 枚举尾同步");

    const unsigned aStateIndex = static_cast<unsigned>(mState);
    if (aStateIndex < sizeof(kScaredyStateCases) / sizeof(kScaredyStateCases[0]) &&
        kScaredyStateCases[aStateIndex] != nullptr)
    {
        kScaredyStateCases[aStateIndex](ownerNowNs, *this, aBodyReanim, aHasZombieNearby);
    }

    if (mState != PlantState::STATE_READY)
    {
        mNextLaunchTimeNs = PlantLaunchClock::Add(ownerNowNs,
            mLaunchRate.mNanoseconds);
    }
}

//0x460610
void Plant::UpdateTorchwood()
{
    Rect aAttackRect = GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);

    Projectile* aProjectile = nullptr;
    while (mBoard->IterateProjectiles(aProjectile))
    {
        if ((aProjectile->mRow == mRow) &&
            (aProjectile->mProjectileType == ProjectileType::PROJECTILE_PEA || aProjectile->mProjectileType == ProjectileType::PROJECTILE_SNOWPEA))
        {
            Rect aProjectileRect = aProjectile->GetProjectileRect();
            if (GetRectOverlap(aAttackRect, aProjectileRect) >= 10)
            {
                if (aProjectile->mProjectileType == ProjectileType::PROJECTILE_PEA)
                {
                    aProjectile->ConvertToFireball(mPlantCol);
                }
                else if (aProjectile->mProjectileType == ProjectileType::PROJECTILE_SNOWPEA)
                {
                    aProjectile->ConvertToPea(mPlantCol);
                }
            }
        }
    }
}

//0x4606F0
void Plant::DoSquashDamage(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
    Rect aAttackRect = GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);

    Zombie* aZombie = nullptr;
    // 行段路径（2026-10-04 全量转换）：Boss‖mRow 候选集，apply-all 与序无关
    while (mBoard->IterateZombiesInRow(mRow, aZombie))
    {
        if ((aZombie->mRow == mRow || ((ZombieFlagsOf(aZombie->mZombieType) & ZombieFlags::Boss) != 0)) && aZombie->EffectedByDamage(aDamageRangeFlags))
        {
            Rect aZombieRect = aZombie->GetZombieRect();
            if (GetRectOverlap(aAttackRect, aZombieRect) > (aZombie->mZombieType == ZombieType::ZOMBIE_FOOTBALL ? -20 : 0))
            {
                aZombie->TakeDamage(ownerNowNs, visualNowNs, animationNowNs, 1800, 18U);
            }
        }
    }
}

//0x4607E0
Zombie* Plant::FindSquashTarget()
{
    int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
    Rect aAttackRect = GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);

    int aClosestRange = 0;
    Zombie* aClosestZombie = nullptr;

    Zombie* aZombie = nullptr;
    // 行段路径（2026-10-04 全量转换）：Boss‖mRow 候选集=原过滤集，段内
    // 槽位升序=池扫序（含 Boss 归并），黑名单链与决胜逻辑零改动
    while (mBoard->IterateZombiesInRow(mRow, aZombie))
    {
        if ((aZombie->mRow == mRow || ((ZombieFlagsOf(aZombie->mZombieType) & ZombieFlags::Boss) != 0)) &&
            aZombie->mHasHead && !aZombie->IsTangleKelpTarget() && aZombie->EffectedByDamage(aDamageRangeFlags))
        {
            Rect aZombieRect = aZombie->GetZombieRect();

            if ((
                    aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT && aZombieRect.mX < (mX) + 20
                ) || (
                    aZombie->mZombiePhase != ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT &&
                    aZombie->mZombiePhase != ZombiePhase::PHASE_POLEVAULTER_IN_VAULT &&
                    aZombie->mZombiePhase != ZombiePhase::PHASE_SNORKEL_INTO_POOL &&
                    aZombie->mZombiePhase != ZombiePhase::PHASE_DOLPHIN_INTO_POOL &&
                    aZombie->mZombiePhase != ZombiePhase::PHASE_DOLPHIN_RIDING &&
                    aZombie->mZombiePhase != ZombiePhase::PHASE_DOLPHIN_IN_JUMP &&
                    !aZombie->IsBobsledTeamWithSled()
                ))
            {
                int aRange = -GetRectOverlap(aAttackRect, aZombieRect);
                if (aRange <= (aZombie->mIsEating ? 110 : 70))
                {
                    int aPlantX = aAttackRect.mX;
                    if (aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_POST_VAULT || aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT ||
                        aZombie->mZombiePhase == ZombiePhase::PHASE_DOLPHIN_WALKING_IN_POOL || aZombie->mZombieType == ZombieType::ZOMBIE_IMP ||
                        aZombie->mZombieType == ZombieType::ZOMBIE_FOOTBALL || mApp->IsScaryPotterLevel())
                    {
                        aPlantX = aAttackRect.mX - 60;
                    }

                    if (aZombie->IsWalkingBackwards() || aZombieRect.mX + aZombieRect.mWidth >= aPlantX)
                    {
                        if (mBoard->ZombieGetID(aZombie) == mTargetZombieID)
                            return aZombie;  // 是锁定的目标僵尸，则直接返回该僵尸

                        if (aClosestZombie == nullptr || aRange < aClosestRange)
                        {
                            aClosestZombie = aZombie;
                            aClosestRange = aRange;
                        }
                    }
                }
            }
        }
    }

    return aClosestZombie;
}

//0x4609D0
void Plant::UpdateSquash(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

    (void)aBodyReanim; // unused in Release mode
    TOD_ASSERT(aBodyReanim);

    if (mState == PlantState::STATE_NOTREADY)
    {
        Zombie* aZombie = FindSquashTarget();
        if (aZombie)
        {
            mTargetZombieID = mBoard->ZombieGetID(aZombie);
            mTargetX = aZombie->ZombieTargetLeadX(ownerNowNs, 0.0f) - mWidth / 2;
            mState = PlantState::STATE_SQUASH_LOOK;
            mStateCountdown = 80;
            PlayBodyReanim(ownerNowNs, mTargetX < (mX) ? ReanimTrackId::anim_lookleft : ReanimTrackId::anim_lookright, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 24.0f);
            mApp->PlayFoley(FoleyType::FOLEY_SQUASH_HMM);
        }
    }
    else if (mState == PlantState::STATE_SQUASH_LOOK)
    {
        if (mStateCountdown <= 0)
        {
            PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_jumpup, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
            mState = PlantState::STATE_SQUASH_PRE_LAUNCH;
            mStateCountdown = 45;
        }
    }
    else if (mState == PlantState::STATE_SQUASH_PRE_LAUNCH)
    {
        if (mStateCountdown <= 0)
        {
            Zombie* aZombie = FindSquashTarget();
            if (aZombie)
            {
                mTargetX = aZombie->ZombieTargetLeadX(ownerNowNs, 30.0f) - mWidth / 2;
            }

            mState = PlantState::STATE_SQUASH_RISING;
            mStateCountdown = 50;
            mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mRow, 0);
        }
    }
    else
    {
        int aTargetCol = mBoard->PixelToGridXKeepOnBoard(mTargetX, (mY));
        int aDestY = mBoard->GridToPixelY(aTargetCol, mRow) + 8;

        if (mState == PlantState::STATE_SQUASH_RISING)
        {
            { mX = (TodAnimateCurveNs(500'000'000, 200'000'000, mStateCountdown.mNanoseconds, mBoard->GridToPixelX(mPlantCol, mStartRow), static_cast<int>(mTargetX), Curves::EaseInOut)); mRootBindingGeneration = 0; }
            mY = (TodAnimateCurveNs(500'000'000, 200'000'000, mStateCountdown.mNanoseconds, mBoard->GridToPixelY(mPlantCol, mStartRow), aDestY - 120, Curves::EaseInOut));

            if (mStateCountdown == 0)
            {
                PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_jumpdown, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 60.0f);
                mState = PlantState::STATE_SQUASH_FALLING;
                mStateCountdown = 10;
            }
        }
        else if (mState == PlantState::STATE_SQUASH_FALLING)
        {
            mY = (TodAnimateCurveNs(100'000'000, 0, mStateCountdown.mNanoseconds, aDestY - 120, aDestY, Curves::Linear));

            if (mStateCountdown == 5)
            {
                DoSquashDamage(ownerNowNs, visualNowNs, animationNowNs);
            }

            if (mStateCountdown == 0)
            {
                if (mBoard->IsPoolSquare(aTargetCol, mRow))
                {
                    mApp->AddReanimation((mX) - 11, (mY) + 20, mRenderOrder + 1, ReanimationType::REANIM_SPLASH, visualNowNs);
                    mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
                    mApp->PlaySample(SOUND_ZOMBIESPLASH);

                    Die(ownerNowNs, visualNowNs, animationNowNs);
                }
                else
                {
                    mState = PlantState::STATE_SQUASH_DONE_FALLING;
                    mStateCountdown = 100;

                    mBoard->mShakeCounter = 12;
                    mBoard->mShakeAmountX = 1;
                    mBoard->mShakeAmountY = 4;
                    mApp->PlayFoley(FoleyType::FOLEY_THUMP);
                    float aOffsetY = mBoard->StageHasRoof() ? 69.0f : 80.0f;
                    mApp->AddTodParticle((mX) + 40, (mY) + aOffsetY, mRenderOrder + 4, ParticleEffect::PARTICLE_DUST_SQUASH, ownerNowNs);
                }
            }
        }
        else if (mState == PlantState::STATE_SQUASH_DONE_FALLING)
        {
            if (mStateCountdown == 0)
            {
                Die(ownerNowNs, visualNowNs, animationNowNs);
            }
        }
    }
}

//0x460DD0
void Plant::UpdateDoomShroom(std::int64_t ownerNowNs)
{
    if (mIsAsleep || mState == PlantState::STATE_DOINGSPECIAL)
        return;

    mState = PlantState::STATE_DOINGSPECIAL;
    mDoSpecialCountdown = 100;

    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

    TOD_ASSERT(aBodyReanim);

    aBodyReanim->SetFramesForLayer(ReanimTrackId::anim_explode);
    aBodyReanim->ApplyRate(23.0f);
    aBodyReanim->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
    aBodyReanim->SetShakeOverride(ReanimTrackId::DoomShroom_head1, 1.0f);
    aBodyReanim->SetShakeOverride(ReanimTrackId::DoomShroom_head2, 2.0f);
    aBodyReanim->SetShakeOverride(ReanimTrackId::DoomShroom_head3, 2.0f);
    mApp->PlayFoley(FoleyType::FOLEY_REVERSE_EXPLOSION);
}

void Plant::UpdateIceShroom(std::int64_t ownerNowNs)
{
    if (!mIsAsleep && mState != PlantState::STATE_DOINGSPECIAL)
    {
        mState = PlantState::STATE_DOINGSPECIAL;
        mDoSpecialCountdown = 100;
    }
}

//0x460F00
void Plant::UpdateBlover(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

    if (aBodyReanim->mLoopCount > 0 && aBodyReanim->mLoopType != ReanimLoopType::REANIM_LOOP)
    {
        aBodyReanim->SetFramesForLayer(ReanimTrackId::anim_loop);
        aBodyReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
    }

    // 1073 UpdateBlover (0x4648D0) tests Plant+0x50, the special-action timer.
    if (mState != PlantState::STATE_DOINGSPECIAL && mDoSpecialCountdown == 0)
    {
        DoSpecial(ownerNowNs, visualNowNs, animationNowNs);
    }
}

void Plant::UpdateFlowerPot()
{
    if (mState == PlantState::STATE_FLOWERPOT_INVULNERABLE && mStateCountdown == 0)
        mState = PlantState::STATE_NOTREADY;
}

void Plant::UpdateLilypad()
{
    if (mState == PlantState::STATE_LILYPAD_INVULNERABLE && mStateCountdown == 0)
        mState = PlantState::STATE_NOTREADY;
}

//0x460F60
void Plant::UpdateCoffeeBean(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (mState == PlantState::STATE_DOINGSPECIAL)
    {
        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

        if (aBodyReanim->mLoopCount > 0)
        {
            Die(ownerNowNs, visualNowNs, animationNowNs);
        }
    }
}

//0x460FA0
void Plant::UpdateUmbrella(std::int64_t ownerNowNs)
{
    if (mState == PlantState::STATE_UMBRELLA_TRIGGERED)
    {
        if (mStateCountdown == 0)
        {
            mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PROJECTILE, mRow + 1, 0);
            mState = PlantState::STATE_UMBRELLA_REFLECTING;
        }
    }
    else if (mState == PlantState::STATE_UMBRELLA_REFLECTING)
    {
        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

        if (aBodyReanim->mLoopCount > 0)
        {
            PlayIdleAnim(ownerNowNs, 0.0f);
            mState = PlantState::STATE_NOTREADY;
            mRenderOrder = CalcRenderOrder();
        }
    }
}

//0x461020
// GOTY @Patoke: 0x4649F0
void Plant::UpdateCobCannon(std::int64_t ownerNowNs)
{
    // C 形态 -> 匿名 lambda 函数指针表 kCobCannonStateCases（仿 kBossPhaseCases
    // 范式）：行序 = PlantState 枚举序 STATE_COBCANNON_ARMING..
    // STATE_COBCANNON_FIRING（4 状态连续，全覆盖无空槽）；各支原在支内自取
    // aBodyReanim，lambda 内保持原样。
    using CobCannonStateFn = void (*)(std::int64_t, Plant&);
    static constexpr CobCannonStateFn kCobCannonStateCases[(int)PlantState::STATE_COBCANNON_FIRING - (int)PlantState::STATE_COBCANNON_ARMING + 1] = {
        // 原 if STATE_COBCANNON_ARMING
        [](std::int64_t ownerNowNs, Plant& p)
        {
            if (p.mStateCountdown == 0)
            {
                p.mState = PlantState::STATE_COBCANNON_LOADING;
                p.PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_charge, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
            }
        },  // STATE_COBCANNON_ARMING
        // 原 else-if STATE_COBCANNON_LOADING
        [](std::int64_t ownerNowNs, Plant& p)
        {
            Reanimation* aBodyReanim = p.mApp->ReanimationGet(p.mBodyReanimID);

            if (aBodyReanim->ShouldTriggerTimedEvent(0.5f))
            {
                p.mApp->PlayFoley(FoleyType::FOLEY_SHOOP);
            }
            if (aBodyReanim->mLoopCount > 0)
            {
                p.mState = PlantState::STATE_COBCANNON_READY;
                p.PlayIdleAnim(ownerNowNs, 12.0f);
            }
        },  // STATE_COBCANNON_LOADING
        // 原 else-if STATE_COBCANNON_READY
        [](std::int64_t ownerNowNs, Plant& p)
        {
            Reanimation* aBodyReanim = p.mApp->ReanimationGet(p.mBodyReanimID);

            ReanimatorTrackInstance* aCobTrack = aBodyReanim->GetTrackInstanceByName(ReanimTrackId::CobCannon_cob);
            aCobTrack->mTrackColor = GetFlashingColorNs(ownerNowNs, 750'000'000);
        },  // STATE_COBCANNON_READY
        // 原 else-if STATE_COBCANNON_FIRING
        [](std::int64_t ownerNowNs, Plant& p)
        {
            Reanimation* aBodyReanim = p.mApp->ReanimationGet(p.mBodyReanimID);

            if (aBodyReanim->ShouldTriggerTimedEvent(0.48f))
            {
                p.mApp->PlayFoley(FoleyType::FOLEY_COB_LAUNCH);
            }
        },  // STATE_COBCANNON_FIRING
    };
    static_assert(sizeof(kCobCannonStateCases) / sizeof(kCobCannonStateCases[0]) ==
        (int)PlantState::STATE_COBCANNON_FIRING - (int)PlantState::STATE_COBCANNON_ARMING + 1,
        "kCobCannonStateCases 槽位须与 PlantState COBCANNON 段同步");

    const unsigned aStateIndex = static_cast<unsigned>(mState) - static_cast<unsigned>(PlantState::STATE_COBCANNON_ARMING);
    if (aStateIndex < sizeof(kCobCannonStateCases) / sizeof(kCobCannonStateCases[0]))
    {
        kCobCannonStateCases[aStateIndex](ownerNowNs, *this);
    }
}

//0x4611F0
void Plant::UpdateCactus(std::int64_t ownerNowNs)
{
    if (mShootingEndTimeNs != 0)
        return;

    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

    // C 形态 -> 匿名 lambda 函数指针表 kCactusStateCases（仿 kBossPhaseCases
    // 范式）：行序 = PlantState 枚举序 STATE_CACTUS_RISING..
    // STATE_CACTUS_LOWERING（3 状态连续）；表外相位（含 STATE_CACTUS_LOW 与
    // 未列状态）一律走 kCactusRiseOnTarget 具名默认支（= 原 else 支的条件
    // 起跳，未命中目标时无操作）。aBodyReanim 为链前局部，经参数传入。
    using CactusStateFn = void (*)(std::int64_t, Plant&, Reanimation*);
    static constexpr CactusStateFn kCactusRiseOnTarget = [](std::int64_t ownerNowNs, Plant& p, Reanimation* aBodyReanim)
    {
        if (p.FindTargetZombie(p.mRow, PlantWeapon::WEAPON_PRIMARY))
        {
            p.mState = PlantState::STATE_CACTUS_RISING;
            p.PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_rise, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, aBodyReanim->mDefinition->mFPS);
            p.mApp->PlayFoley(FoleyType::FOLEY_PLANTGROW);
        }
    };
    static constexpr CactusStateFn kCactusStateCases[(int)PlantState::STATE_CACTUS_LOWERING - (int)PlantState::STATE_CACTUS_RISING + 1] = {
        // 原 if STATE_CACTUS_RISING
        [](std::int64_t ownerNowNs, Plant& p, Reanimation* aBodyReanim)
        {
            if (aBodyReanim->mLoopCount > 0)
            {
                p.mState = PlantState::STATE_CACTUS_HIGH;
                p.PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_idlehigh, ReanimLoopType::REANIM_LOOP, 20, 0.0f);
                if (p.mApp->IsIZombieLevel())
                {
                    aBodyReanim->ApplyRate(0);
                }

                p.mNextLaunchTimeNs = PlantLaunchClock::Add(
                    ownerNowNs, PlantLaunchClock::Tick);
            }
        },  // STATE_CACTUS_RISING
        // 原 else-if STATE_CACTUS_HIGH
        [](std::int64_t ownerNowNs, Plant& p, Reanimation* aBodyReanim)
        {
            if (p.FindTargetZombie(p.mRow, PlantWeapon::WEAPON_PRIMARY) == nullptr)
            {
                p.mState = PlantState::STATE_CACTUS_LOWERING;
                p.PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_lower, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, aBodyReanim->mDefinition->mFPS);
            }
        },  // STATE_CACTUS_HIGH
        // 原 else-if STATE_CACTUS_LOWERING
        [](std::int64_t ownerNowNs, Plant& p, Reanimation* aBodyReanim)
        {
            if (aBodyReanim->mLoopCount > 0)
            {
                p.mState = PlantState::STATE_CACTUS_LOW;
                p.PlayIdleAnim(ownerNowNs, 0.0f);
            }
        },  // STATE_CACTUS_LOWERING
    };
    static_assert(sizeof(kCactusStateCases) / sizeof(kCactusStateCases[0]) ==
        (int)PlantState::STATE_CACTUS_LOWERING - (int)PlantState::STATE_CACTUS_RISING + 1,
        "kCactusStateCases 槽位须与 PlantState CACTUS 段同步");

    const unsigned aStateIndex = static_cast<unsigned>(mState) - static_cast<unsigned>(PlantState::STATE_CACTUS_RISING);
    if (aStateIndex < sizeof(kCactusStateCases) / sizeof(kCactusStateCases[0]))
    {
        kCactusStateCases[aStateIndex](ownerNowNs, *this, aBodyReanim);
    }
    else
    {
        kCactusRiseOnTarget(ownerNowNs, *this, aBodyReanim);  // 原 else 支（CACTUS_LOW 及未列状态）
    }
}

//0x461320
void Plant::UpdateChomper(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

    // C 形态 -> 匿名 lambda 函数指针表 kChomperStateCases（仿 kSeedTypeUpdates
    // 范式）：行序 = PlantState 枚举序 0..48（count_last=STATE_LILYPAD_
    // INVULNERABLE 锚定）；5 支中 READY=1 与 CHOMPER 段 10..14 分散故取全宽表；
    // 末支 (SWALLOWING || BITING_MISSED) && mLoopCount>0 为共体 + 复合条件，
    // 两槽同填（constexpr 指针变量），mLoopCount 判入 lambda；nullptr = 无
    // case 槽（无操作）。aBodyReanim 为链前局部（TryToGet），经参数传入。
    using ChomperStateFn = void (*)(std::int64_t, std::int64_t, std::int64_t, Plant&, Reanimation*);
    static constexpr ChomperStateFn kChomperSwallowDone = [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, Reanimation* aBodyReanim)
    {
        if (aBodyReanim->mLoopCount > 0)
        {
            p.PlayIdleAnim(ownerNowNs, aBodyReanim->mDefinition->mFPS);
            p.mState = PlantState::STATE_READY;
        }
    };
    static constexpr ChomperStateFn kChomperStateCases[(int)PlantState::STATE_LILYPAD_INVULNERABLE + 1] = {
        nullptr,  // 0 STATE_NOTREADY（无 case）
        // 原 if STATE_READY
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, Reanimation* aBodyReanim)
        {
            if (p.FindTargetZombie(p.mRow, PlantWeapon::WEAPON_PRIMARY))
            {
                p.PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_bite, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 24.0f);
                p.mState = PlantState::STATE_CHOMPER_BITING;
                p.mStateCountdown = 70;
            }
        },  // 1 STATE_READY
        nullptr,  // 2 STATE_DOINGSPECIAL（无 case）
        nullptr,  // 3 STATE_SQUASH_LOOK（无 case）
        nullptr,  // 4 STATE_SQUASH_PRE_LAUNCH（无 case）
        nullptr,  // 5 STATE_SQUASH_RISING（无 case）
        nullptr,  // 6 STATE_SQUASH_FALLING（无 case）
        nullptr,  // 7 STATE_SQUASH_DONE_FALLING（无 case）
        nullptr,  // 8 STATE_GRAVEBUSTER_LANDING（无 case）
        nullptr,  // 9 STATE_GRAVEBUSTER_EATING（无 case）
        // 原 else-if STATE_CHOMPER_BITING
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, Reanimation* aBodyReanim)
        {
            if (p.mStateCountdown == 0)
            {
                p.mApp->PlayFoley(FoleyType::FOLEY_BIGCHOMP);

                Zombie* aZombie = p.FindTargetZombie(p.mRow, PlantWeapon::WEAPON_PRIMARY);
                bool doBite = false;
                if (aZombie)
                {
                    if (aZombie->mZombieType == ZombieType::ZOMBIE_GARGANTUAR || aZombie->mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR ||
                        ((ZombieFlagsOf(aZombie->mZombieType) & ZombieFlags::Boss) != 0))
                    {
                        doBite = true;
                    }
                }
                bool doMiss = false;
                if (aZombie == nullptr)
                {
                    doMiss = true;
                }
                else if (!aZombie->IsImmobilizied(ownerNowNs))
                {
                    if (aZombie->IsBouncingPogo() ||
                        aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT || aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT)
                    {
                        doMiss = true;
                    }
                }

                if (doBite)
                {
                    p.mApp->PlayFoley(FoleyType::FOLEY_SPLAT);
                    aZombie->TakeDamage(ownerNowNs, visualNowNs, animationNowNs, 40, 0U);
                    p.mState = PlantState::STATE_CHOMPER_BITING_MISSED;
                }
                else if (doMiss)
                {
                    p.mState = PlantState::STATE_CHOMPER_BITING_MISSED;
                }
                else
                {
                    aZombie->DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
                    p.mState = PlantState::STATE_CHOMPER_BITING_GOT_ONE;
                }
            }
        },  // 10 STATE_CHOMPER_BITING
        // 原 else-if STATE_CHOMPER_BITING_GOT_ONE
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, Reanimation* aBodyReanim)
        {
            if (aBodyReanim->mLoopCount > 0)
            {
                p.PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_chew, ReanimLoopType::REANIM_LOOP, 0, 15.0f);
                if (p.mApp->IsIZombieLevel())
                {
                    aBodyReanim->ApplyRate(0);
                }

                p.mState = PlantState::STATE_CHOMPER_DIGESTING;
                p.mStateCountdown = 4000;
            }
        },  // 11 STATE_CHOMPER_BITING_GOT_ONE
        kChomperSwallowDone,  // 12 STATE_CHOMPER_BITING_MISSED（末支共体）
        // 原 else-if STATE_CHOMPER_DIGESTING
        [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, Reanimation* aBodyReanim)
        {
            if (p.mStateCountdown == 0)
            {
                p.PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_swallow, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
                p.mState = PlantState::STATE_CHOMPER_SWALLOWING;
            }
        },  // 13 STATE_CHOMPER_DIGESTING
        kChomperSwallowDone,  // 14 STATE_CHOMPER_SWALLOWING（末支共体）
        nullptr,  // 15 STATE_POTATO_RISING（无 case）
        nullptr,  // 16 STATE_POTATO_ARMED（无 case）
        nullptr,  // 17 STATE_POTATO_MASHED（无 case）
        nullptr,  // 18 STATE_SPIKEWEED_ATTACKING（无 case）
        nullptr,  // 19 STATE_SPIKEWEED_ATTACKING_2（无 case）
        nullptr,  // 20 STATE_SCAREDYSHROOM_LOWERING（无 case）
        nullptr,  // 21 STATE_SCAREDYSHROOM_SCARED（无 case）
        nullptr,  // 22 STATE_SCAREDYSHROOM_RAISING（无 case）
        nullptr,  // 23 STATE_SUNSHROOM_SMALL（无 case）
        nullptr,  // 24 STATE_SUNSHROOM_GROWING（无 case）
        nullptr,  // 25 STATE_SUNSHROOM_BIG（无 case）
        nullptr,  // 26 STATE_MAGNETSHROOM_SUCKING（无 case）
        nullptr,  // 27 STATE_MAGNETSHROOM_CHARGING（无 case）
        nullptr,  // 28 STATE_BOWLING_UP（无 case）
        nullptr,  // 29 STATE_BOWLING_DOWN（无 case）
        nullptr,  // 30 STATE_CACTUS_LOW（无 case）
        nullptr,  // 31 STATE_CACTUS_RISING（无 case）
        nullptr,  // 32 STATE_CACTUS_HIGH（无 case）
        nullptr,  // 33 STATE_CACTUS_LOWERING（无 case）
        nullptr,  // 34 STATE_TANGLEKELP_GRABBING（无 case）
        nullptr,  // 35 STATE_COBCANNON_ARMING（无 case）
        nullptr,  // 36 STATE_COBCANNON_LOADING（无 case）
        nullptr,  // 37 STATE_COBCANNON_READY（无 case）
        nullptr,  // 38 STATE_COBCANNON_FIRING（无 case）
        nullptr,  // 39 STATE_KERNELPULT_BUTTER（无 case）
        nullptr,  // 40 STATE_UMBRELLA_TRIGGERED（无 case）
        nullptr,  // 41 STATE_UMBRELLA_REFLECTING（无 case）
        nullptr,  // 42 STATE_IMITATER_MORPHING（无 case）
        nullptr,  // 43 STATE_ZEN_GARDEN_WATERED（无 case）
        nullptr,  // 44 STATE_ZEN_GARDEN_NEEDY（无 case）
        nullptr,  // 45 STATE_ZEN_GARDEN_HAPPY（无 case）
        nullptr,  // 46 STATE_MARIGOLD_ENDING（无 case）
        nullptr,  // 47 STATE_FLOWERPOT_INVULNERABLE（无 case）
        nullptr,  // 48 STATE_LILYPAD_INVULNERABLE（无 case）
    };
    static_assert(sizeof(kChomperStateCases) / sizeof(kChomperStateCases[0]) ==
        (int)PlantState::STATE_LILYPAD_INVULNERABLE + 1, "kChomperStateCases 槽位须与 PlantState 枚举尾同步");

    const unsigned aStateIndex = static_cast<unsigned>(mState);
    if (aStateIndex < sizeof(kChomperStateCases) / sizeof(kChomperStateCases[0]) &&
        kChomperStateCases[aStateIndex] != nullptr)
    {
        kChomperStateCases[aStateIndex](ownerNowNs, visualNowNs, animationNowNs, *this, aBodyReanim);
    }
}

//0x4615D0
MagnetItem* Plant::GetFreeMagnetItem()
{
    if (mSeedType == SeedType::SEED_GOLD_MAGNET)
    {
        for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
        {
            if (mMagnetItems[i].mItemType == MagnetItemType::MAGNET_ITEM_NONE)
            {
                return &mMagnetItems[i];
            }
        }

        return nullptr;
    }

    return &mMagnetItems[0];
}

//0x460610
// GOTY 1073: 0x464FC0. Original offsets and reconstruction differences:
// docs/research/1073-magnet-coordinates.md.
void Plant::MagnetShroomAttactItem(std::int64_t ownerNowNs, Zombie* theZombie)
{
    mState = PlantState::STATE_MAGNETSHROOM_SUCKING;
    mStateCountdown = 1500;
    PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_shooting, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
    mApp->PlayFoley(FoleyType::FOLEY_MAGNETSHROOM);

    MagnetItem* aMagnetItem = GetFreeMagnetItem();
    const float aDrawHeight = PlantDrawHeightOffset(ownerNowNs, mBoard, this, mSeedType, mPlantCol, mRow);
    const auto aTakeTrack = [&](ReanimTrackId track, MagnetItemType type)
    {
        // Capture the visible track center before removing equipment or changing
        // animation. Magnet items draw from their scaled top-left, with the
        // receiving plant's height applied again by DrawMagnetItems.
        theZombie->GetTrackPosition(track, aMagnetItem->mPosX, aMagnetItem->mPosY);
        const auto sprite = static_cast<size_t>(kMagnetSprites[static_cast<size_t>(type)]);
        const TextureRegion& rect = MagnetAtlas::Images[sprite];
        const float scale = MagnetAtlas::Scales[sprite];
        aMagnetItem->mPosX -= rect.mWidth * scale * 0.5f;
        aMagnetItem->mPosY -= rect.mHeight * scale * 0.5f + aDrawHeight;
        aMagnetItem->mItemType = type;
    };
    if (theZombie->mHelmType == HelmType::HELMTYPE_PAIL)
    {
        int aDamageIndex = theZombie->GetHelmDamageIndex();
        aTakeTrack(ReanimTrackId::anim_bucket,
            (MagnetItemType)((int)MagnetItemType::MAGNET_ITEM_PAIL_1 + aDamageIndex));

        theZombie->mHelmHealth = 0;
        theZombie->mHelmType = HelmType::HELMTYPE_NONE;
        theZombie->ReanimShowRange(ReanimTrackRanges::anim_bucket, RENDER_GROUP_HIDDEN);
        theZombie->ReanimShowRange(ReanimTrackRanges::anim_hair, RENDER_GROUP_NORMAL);

        aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 25.0f;
        aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f) + 20.0f;
    }
    else if (theZombie->mHelmType == HelmType::HELMTYPE_FOOTBALL)
    {
        int aDamageIndex = theZombie->GetHelmDamageIndex();
        aTakeTrack(ReanimTrackId::zombie_football_helmet,
            (MagnetItemType)((int)MagnetItemType::MAGNET_ITEM_FOOTBALL_HELMET_1 + aDamageIndex));

        theZombie->mHelmHealth = 0;
        theZombie->mHelmType = HelmType::HELMTYPE_NONE;
        theZombie->ReanimShowRange(ReanimTrackRanges::zombie_football_helmet, RENDER_GROUP_HIDDEN);
        theZombie->ReanimShowRange(ReanimTrackRanges::anim_hair, RENDER_GROUP_NORMAL);

        aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 20.0f;
        aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f) + 20.0f;
    }
    else if (theZombie->mShieldType == ShieldType::SHIELDTYPE_DOOR)
    {
        int aDamageIndex = theZombie->GetShieldDamageIndex();
        aTakeTrack(ReanimTrackId::anim_screendoor,
            (MagnetItemType)((int)MagnetItemType::MAGNET_ITEM_DOOR_1 + aDamageIndex));

        theZombie->DetachShield(ownerNowNs);
        theZombie->mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
        if (!theZombie->mIsEating)
        {
            TOD_ASSERT(theZombie->mZombieHeight == ZombieHeight::HEIGHT_ZOMBIE_NORMAL);
            theZombie->StartWalkAnim(ownerNowNs, 0);
        }
        aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 30.0f;
        aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f);
    }
    else if (theZombie->mShieldType == ShieldType::SHIELDTYPE_LADDER)
    {
        int aDamageIndex = theZombie->GetShieldDamageIndex();
        aTakeTrack(ReanimTrackId::Zombie_ladder_1,
            (MagnetItemType)((int)MagnetItemType::MAGNET_ITEM_LADDER_1 + aDamageIndex));

        theZombie->DetachShield(ownerNowNs);

        aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 30.0f;
        aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f);
    }
    else if (theZombie->mZombieType == ZombieType::ZOMBIE_POGO)
    {
        aTakeTrack(ReanimTrackId::Zombie_pogo_stick,
            theZombie->mHasArm ? MagnetItemType::MAGNET_ITEM_POGO_1 : MagnetItemType::MAGNET_ITEM_POGO_3);
        theZombie->PogoBreak(ownerNowNs, 16U);
        aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 30.0f;
        aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f);
    }
    else if (theZombie->mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_RUNNING)
    {
        aTakeTrack(ReanimTrackId::Zombie_jackbox_box, MagnetItemType::MAGNET_ITEM_JACK_IN_THE_BOX);
        theZombie->StopZombieSound();
        theZombie->PickRandomSpeed(ownerNowNs);
        theZombie->mZombiePhase = ZombiePhase::PHASE_ZOMBIE_NORMAL;
        theZombie->ReanimShowRange(ReanimTrackRanges::Zombie_jackbox_box, RENDER_GROUP_HIDDEN);
        theZombie->ReanimShowRange(ReanimTrackRanges::Zombie_jackbox_handle, RENDER_GROUP_HIDDEN);
        aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 20.0f;
        aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f) + 15.0f;
    }
    else if (theZombie->mZombieType == ZombieType::ZOMBIE_DIGGER)
    {
        aTakeTrack(ReanimTrackId::Zombie_digger_pickaxe, MagnetItemType::MAGNET_ITEM_PICK_AXE);
        theZombie->DiggerLoseAxe(ownerNowNs);
        aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 45.0f;
        aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f) + 15.0f;
    }
    MagnetMotion::Bind(*aMagnetItem, mX + aMagnetItem->mDestOffsetX, mY + aMagnetItem->mDestOffsetY);
    mMagnetStartTimeNs = ownerNowNs;
}

//0x461CD0
// GOTY @Patoke: 0x4656B0
bool Plant::DrawMagnetItemsOnTop()
{
    if (mSeedType == SeedType::SEED_GOLD_MAGNET)
    {
        for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
        {
            if (mMagnetItems[i].mItemType != MagnetItemType::MAGNET_ITEM_NONE)
            {
                return true;
            }
        }

        return false;
    }

    if (mSeedType == SeedType::SEED_MAGNETSHROOM)
    {
        for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
        {
            MagnetItem* aMagnetItem = &mMagnetItems[i];
            if (aMagnetItem->mItemType != MagnetItemType::MAGNET_ITEM_NONE)
            {
                SexyVector2 aVectorToPlant((mX) + aMagnetItem->mDestOffsetX - aMagnetItem->mPosX, (mY) + aMagnetItem->mDestOffsetY - aMagnetItem->mPosY);
                if (aVectorToPlant.Magnitude() > MagnetMotion::CaptureRadius)
                {
                    return true;
                }
            }
        }

        return false;
    }

    return false;
}

//0x461D90
void Plant::UpdateMagnetShroom(std::int64_t ownerNowNs)
{
    const auto magnetAgeNs = ownerNowNs - mMagnetStartTimeNs;
    for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
    {
        MagnetItem* aMagnetItem = &mMagnetItems[i];
        if (aMagnetItem->mItemType != MagnetItemType::MAGNET_ITEM_NONE)
        {
            MagnetMotion::Advance(*aMagnetItem, mX + aMagnetItem->mDestOffsetX,
                mY + aMagnetItem->mDestOffsetY, magnetAgeNs, Sexy::gUpdateIntervalNs);
        }
    }

    if (mState == PlantState::STATE_MAGNETSHROOM_CHARGING)
    {
        if (mStateCountdown == 0)
        {
            mState = PlantState::STATE_READY;

            float aAnimRate = RandRangeFloat(10.0f, 15.0f);
            PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 30, aAnimRate);
            if (mApp->IsIZombieLevel())
            {
                Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

                aBodyReanim->ApplyRate(0.0f);
            }

            mMagnetItems[0].mItemType = MagnetItemType::MAGNET_ITEM_NONE;
        }
    }
    else if (mState == PlantState::STATE_MAGNETSHROOM_SUCKING)
    {
        Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

        if (aBodyReanim->mLoopCount > 0)
        {
            PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_nonactive_idle2, ReanimLoopType::REANIM_LOOP, 20, 2.0f);
            if (mApp->IsIZombieLevel())
            {
                aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
                aBodyReanim->ApplyRate(0.0f);
            }

            mState = PlantState::STATE_MAGNETSHROOM_CHARGING;
        }
    }
    else
    {
        float aClosestDistance = 0.0f;
        Zombie* aClosestZombie = nullptr;

        Zombie* aZombie = nullptr;
        // 行段区间路径（2026-10-04 全量转换；用户裁决简化后统一尾吐 Boss）：
        // 体内无 Boss 豁免——真实 diffY 门按真实行滤掉 Boss（原版同构，
        // Boss 亦过不了道具链）；行主序≠池扫全序，选择点加槽位决胜
        while (mBoard->IterateZombiesInRowRange(mRow - 2, mRow + 2, aZombie))
        {
            int aDiffY = aZombie->mRow - mRow;
            Rect aZombieRect = aZombie->GetZombieRect();

            if (aZombie->mMindControlled)
                continue;

            if (!aZombie->mHasHead)
                continue;

            if (aZombie->mZombieHeight != ZombieHeight::HEIGHT_ZOMBIE_NORMAL || aZombie->mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE)
                continue;

            if (aZombie->IsDeadOrDying())
                continue;

            if (aZombieRect.mX > BOARD_WIDTH || aDiffY > 2 || aDiffY < -2)
                continue;

            if (aZombie->mZombiePhase == ZombiePhase::PHASE_DIGGER_TUNNELING ||
                aZombie->mZombiePhase == ZombiePhase::PHASE_DIGGER_STUNNED ||
                aZombie->mZombiePhase == ZombiePhase::PHASE_DIGGER_WALKING ||
                aZombie->mZombieType == ZombieType::ZOMBIE_POGO)
            {
                if (!aZombie->mHasObject)
                    continue;
            }
            else if (!(aZombie->mHelmType == HelmType::HELMTYPE_PAIL ||
                aZombie->mHelmType == HelmType::HELMTYPE_FOOTBALL ||
                aZombie->mShieldType == ShieldType::SHIELDTYPE_DOOR ||
                aZombie->mShieldType == ShieldType::SHIELDTYPE_LADDER ||
                aZombie->mZombiePhase == ZombiePhase::PHASE_JACK_IN_THE_BOX_RUNNING))
                continue;

            int aRadius = aZombie->mIsEating ? 320 : 270;
            if (GetCircleRectOverlap((mX), (mY) + 20, aRadius, aZombieRect))
            {
                float aDistance = Distance2D((mX), (mY), aZombieRect.mX, aZombieRect.mY);
                aDistance += abs(aDiffY) * 80.0f;

                // 槽位决胜：行主序≠池扫全序，平局取小槽=池扫严格<保留先见者
                if (aClosestZombie == nullptr || aDistance < aClosestDistance ||
                    (aDistance == aClosestDistance && mBoard->ZombieSlotOf(aZombie) < mBoard->ZombieSlotOf(aClosestZombie)))
                {
                    aClosestZombie = aZombie;
                    aClosestDistance = aDistance;
                }
            }
        }

        if (aClosestZombie)
        {
            MagnetShroomAttactItem(ownerNowNs, aClosestZombie);
            return;
        }

        ////////////////////

        float aClosestLadderDist = 0.0f;
        GridItem* aClosestLadder = nullptr;

        GridItem* aGridItem = nullptr;
        while (mBoard->IterateGridItems(aGridItem))
        {
            if (aGridItem->mGridItemType == GridItemType::GRIDITEM_LADDER)
            {
                int aDiffX = abs(aGridItem->mGridX - mPlantCol);
                int aDiffY = abs(aGridItem->mGridY - mRow);
                int aSquareDistance = std::max(aDiffX, aDiffY);
                if (aSquareDistance <= 2)
                {
                    float aDistance = aSquareDistance + aDiffY * 0.05f;
                    if (aClosestLadder == nullptr || aDistance < aClosestLadderDist)
                    {
                        aClosestLadder = aGridItem;
                        aClosestLadderDist = aDistance;
                    }
                }
            }
        }

        if (aClosestLadder)
        {
            mState = PlantState::STATE_MAGNETSHROOM_SUCKING;
            mStateCountdown = 1500;
            PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_shooting, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
            mApp->PlayFoley(FoleyType::FOLEY_MAGNETSHROOM);

            aClosestLadder->GridItemDie();

            MagnetItem* aMagnetItem = GetFreeMagnetItem();
            // Match GridItem::DrawLadder's actual top-left before transfer.
            aMagnetItem->mPosX = mBoard->GridToPixelX(aClosestLadder->mGridX, aClosestLadder->mGridY) + 25.0f;
            aMagnetItem->mPosY = mBoard->GridToPixelY(aClosestLadder->mGridX, aClosestLadder->mGridY) - 4.0f -
                PlantDrawHeightOffset(ownerNowNs, mBoard, this, mSeedType, mPlantCol, mRow);
            aMagnetItem->mDestOffsetX = RandRangeFloat(-10.0f, 10.0f) + 10.0f;
            aMagnetItem->mDestOffsetY = RandRangeFloat(-10.0f, 10.0f);
            aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_LADDER_PLACED;
            MagnetMotion::Bind(*aMagnetItem, mX + aMagnetItem->mDestOffsetX, mY + aMagnetItem->mDestOffsetY);
            mMagnetStartTimeNs = ownerNowNs;
        }
    }
}

//0x462390
Coin* Plant::FindGoldMagnetTarget(std::int64_t visualNowNs)
{
    Coin* aClosestCoin = nullptr;
    float aClosestDistance = 0.0f;

    Coin* aCoin = nullptr;
    while (mBoard->IterateCoins(aCoin))
    {
        if (aCoin->IsMoney() && aCoin->mCoinMotion != CoinMotion::COIN_MOTION_FROM_PRESENT && !aCoin->mIsBeingCollected && visualNowNs - aCoin->mBirthTimeNs >= 500'000'000)
        {
            float aDistance = Distance2D((mX) + mWidth / 2, (mY) + mHeight / 2, aCoin->mPosX + aCoin->mWidth / 2, aCoin->mPosY + aCoin->mHeight / 2);
            if (aClosestCoin == nullptr || aDistance < aClosestDistance)
            {
                aClosestCoin = aCoin;
                aClosestDistance = aDistance;
            }
        }
    }

    return aClosestCoin;
}

//0x4624B0
void Plant::GoldMagnetFindTargets(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (GetFreeMagnetItem() == nullptr)
    {
        TOD_ASSERT(false);
        return;
    }

    for (;;)
    {
        MagnetItem* aMagnetItem = GetFreeMagnetItem();
        if (aMagnetItem == nullptr)
            break;

        Coin* aCoin = FindGoldMagnetTarget(visualNowNs);
        if (aCoin == nullptr)
            break;

        aMagnetItem->mPosX = aCoin->mPosX + 15.0f;
        aMagnetItem->mPosY = aCoin->mPosY + 15.0f;
        aMagnetItem->mDestOffsetX = RandRangeFloat(20.0f, 40.0f);
        aMagnetItem->mDestOffsetY = RandRangeFloat(-20.0f, 0.0f) + 20.0f;

        switch (aCoin->mType)
        {
        case CoinType::COIN_SILVER:     aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_SILVER_COIN;   break;
        case CoinType::COIN_GOLD:       aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_GOLD_COIN;     break;
        case CoinType::COIN_DIAMOND:    aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_DIAMOND;       break;
        default:                        TOD_ASSERT(false);                                                       return;
        }

        MagnetMotion::Bind(*aMagnetItem, mX + aMagnetItem->mDestOffsetX, mY + aMagnetItem->mDestOffsetY);
        mMagnetStartTimeNs = ownerNowNs;
        aCoin->Die();
    }
}

//0x4625D0
bool Plant::IsAGoldMagnetAboutToSuck(std::int64_t ownerNowNs)
{
    Plant* aPlant = nullptr;
    while (mBoard->IteratePlants(aPlant))
    {
        if (!aPlant->NotOnGround() && aPlant->mSeedType == SeedType::SEED_GOLD_MAGNET && aPlant->mState == PlantState::STATE_MAGNETSHROOM_SUCKING)
        {
            Reanimation* aBodyReanim = mApp->ReanimationGet(aPlant->mBodyReanimID);

            if ((static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs) < 0.5f)
            {
                return true;
            }
        }
    }

    return false;
}

//0x462680
void Plant::UpdateGoldMagnetShroom(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    const auto magnetAgeNs = ownerNowNs - mMagnetStartTimeNs;
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);


    bool aIsSuckingCoin = false;
    for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
    {
        MagnetItem* aMagnetItem = &mMagnetItems[i];
        if (aMagnetItem->mItemType != MagnetItemType::MAGNET_ITEM_NONE)
        {
            if (MagnetMotion::Advance(*aMagnetItem, mX + aMagnetItem->mDestOffsetX,
                mY + aMagnetItem->mDestOffsetY, magnetAgeNs, Sexy::gUpdateIntervalNs))
            {
                CoinType aCoinType;
                switch (aMagnetItem->mItemType)
                {
                case MagnetItemType::MAGNET_ITEM_SILVER_COIN:   aCoinType = CoinType::COIN_SILVER;      break;
                case MagnetItemType::MAGNET_ITEM_GOLD_COIN:     aCoinType = CoinType::COIN_GOLD;        break;
                case MagnetItemType::MAGNET_ITEM_DIAMOND:       aCoinType = CoinType::COIN_DIAMOND;     break;
                default:                                        TOD_ASSERT(false);                           return;
                }

                int aValue = Coin::GetCoinValue(aCoinType);
                mApp->mPlayerInfo->AddCoins(aValue);
                mBoard->mCoinsCollected += aValue;
                mApp->PlayFoley(FoleyType::FOLEY_COIN);

                aMagnetItem->mItemType = MagnetItemType::MAGNET_ITEM_NONE;
            }
            else
            {
                aIsSuckingCoin = true;
            }
        }
    }

    if (mState == PlantState::STATE_MAGNETSHROOM_CHARGING)
    {
        if (mStateCountdown == 0)
        {
            mState = PlantState::STATE_READY;
        }
    }
    else if (mState == PlantState::STATE_MAGNETSHROOM_SUCKING)
    {
        if (aBodyReanim->ShouldTriggerTimedEvent(0.4f))
        {
            mApp->PlayFoley(FoleyType::FOLEY_MAGNETSHROOM);
            GoldMagnetFindTargets(ownerNowNs, visualNowNs, animationNowNs);
        }

        if (aBodyReanim->mLoopCount > 0 && !aIsSuckingCoin)
        {
            PlayIdleAnim(ownerNowNs, 14.0f);
            mState = PlantState::STATE_MAGNETSHROOM_CHARGING;
            mStateCountdown = RandRangeInt(200, 300);
        }
    }
    else if (!IsAGoldMagnetAboutToSuck(ownerNowNs) && Sexy::UpdateChance(50) && FindGoldMagnetTarget(visualNowNs))
    {
        mBoard->mCoinBankFadeCount = 1000;
        mState = PlantState::STATE_MAGNETSHROOM_SUCKING;
        PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_attract, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);
    }
}

//0x4629F0
void Plant::RemoveEffects()
{
    mApp->RemoveParticle(mParticleID);
    mApp->RemoveReanimation(mBodyReanimID);
    mApp->RemoveReanimation(mHeadReanimID);
    mApp->RemoveReanimation(mHeadReanimID2);
    mApp->RemoveReanimation(mHeadReanimID3);
    mApp->RemoveReanimation(mLightReanimID);
    mApp->RemoveReanimation(mBlinkReanimID);
    mApp->RemoveReanimation(mSleepingReanimID);
}

//0x462B80
void Plant::Squish(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (NotOnGround())
        return;

    if (!mIsAsleep)
    {
        if (mSeedType == SeedType::SEED_CHERRYBOMB || mSeedType == SeedType::SEED_JALAPENO ||
            mSeedType == SeedType::SEED_DOOMSHROOM || mSeedType == SeedType::SEED_ICESHROOM)
        {
            DoSpecial(ownerNowNs, visualNowNs, animationNowNs);
            return;
        }
        else if (mSeedType == SeedType::SEED_POTATOMINE && mState != PlantState::STATE_NOTREADY)
        {
            DoSpecial(ownerNowNs, visualNowNs, animationNowNs);
            return;
        }
    }

    if (mSeedType == SeedType::SEED_SQUASH && mState != PlantState::STATE_NOTREADY)
        return;

    mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, mRow, 8);
    mSquished = true;
    mDisappearEndTimeNs = ownerNowNs + 5'000'000'000;
    mApp->PlayFoley(FoleyType::FOLEY_SQUISH);
    RemoveEffects();

    GridItem* aLadder = mBoard->GetLadderAt(mPlantCol, mRow);
    if (aLadder)
    {
        aLadder->GridItemDie();
    }

    if (mApp->IsIZombieLevel())
    {
        mBoard->mChallenge->IZombiePlantDropRemainingSun(this);
    }
}

//0x462CE0
// GOTY @Patoke: 0x4666E0
void Plant::UpdateBowling(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

    const bool aHasRoot = aBodyReanim && aBodyReanim->TrackExists(ReanimTrackId::_ground);
    const float aRootScale = mSeedType == SeedType::SEED_GIANT_WALLNUT ? -2.0f : -1.0f;
    if (Sexy::gUpdateIntervalNs > INT64_MAX)
        throw std::overflow_error("bowling simulation interval exceeds clock range");
    const auto aNow = ownerNowNs;
    const auto aTravelTime = [](double distance) -> std::int64_t {
        const auto duration = std::abs(distance) * 5'000'000.0; // 200 pixels/second.
        if (!std::isfinite(duration) || duration >= 9223372036854775808.0)
            throw std::overflow_error("bowling row travel exceeds clock range");
        return std::llround(duration);
    };
    const auto aSampleRoot = [&](std::int64_t eventTime) {
        if (aHasRoot)
            AdvanceRoot(*this, *aBodyReanim, aRootScale, ReanimClock::Subtract(eventTime, aNow));
        if (mX > 800)
            Die(ownerNowNs, visualNowNs, animationNowNs);
    };
    const auto aProcessRow = [&](std::int64_t eventTime, bool arrived) {
        aSampleRoot(eventTime);
        if (mDead)
            return;
        const double aTargetY = mBoard->GridToPixelY(0, mRow);
        if (arrived)
            mY = static_cast<float>(aTargetY);
        else if (std::abs(aTargetY - mY) > 2.0)
            return;

        PlantState aNewState = arrived ? mState : PlantState::STATE_NOTREADY;
        if (arrived && mState == PlantState::STATE_BOWLING_UP && mRow <= 0)
        {
            aNewState = PlantState::STATE_BOWLING_DOWN;
        }
        else if (arrived && mState == PlantState::STATE_BOWLING_DOWN && mRow >= 4)
        {
            aNewState = PlantState::STATE_BOWLING_UP;
        }

        Zombie* aZombie = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY);
        if (aZombie)
        {
            int aPosX = (mX) + mWidth / 2;
            int aPosY = (mY) + mHeight / 2;

            if (mSeedType == SeedType::SEED_EXPLODE_O_NUT)
            {
                mApp->PlayFoley(FoleyType::FOLEY_CHERRYBOMB);
                mApp->PlaySample(SOUND_BOWLINGIMPACT2);

                int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY) | 32U;
                mBoard->KillAllZombiesInRadius(mRow, aPosX, aPosY, 90, 1, true, aDamageRangeFlags);
                mApp->AddTodParticle(aPosX, aPosY, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_POWIE, ownerNowNs);
                mBoard->mShakeCounter = 12;
                mBoard->mShakeAmountX = 3;
                mBoard->mShakeAmountY = -4;

                Die(ownerNowNs, visualNowNs, animationNowNs);

                return;
            }

            mApp->PlayFoley(FoleyType::FOLEY_BOWLINGIMPACT);
            mBoard->mShakeCounter = 12;
            mBoard->mShakeAmountX = 1;
            mBoard->mShakeAmountY = -2;

            if (mSeedType == SeedType::SEED_GIANT_WALLNUT)
            {
                aZombie->TakeDamage(ownerNowNs, visualNowNs, animationNowNs, 1'000'000, 0U);
            }
            else if (aZombie->mShieldType == ShieldType::SHIELDTYPE_DOOR && mState != PlantState::STATE_NOTREADY)
            {
                aZombie->TakeDamage(ownerNowNs, visualNowNs, animationNowNs, 1800, 0U);
            }
            else if (aZombie->mShieldType != ShieldType::SHIELDTYPE_NONE)
            {
                aZombie->TakeShieldDamage(ownerNowNs, visualNowNs, animationNowNs, 400, 0U);
            }
            else if (aZombie->mHelmType != HelmType::HELMTYPE_NONE)
            {
                if (aZombie->mHelmType == HelmType::HELMTYPE_PAIL)
                {
                    mApp->PlayFoley(FoleyType::FOLEY_SHIELD_HIT);
                }
                else if (aZombie->mHelmType == HelmType::HELMTYPE_TRAFFIC_CONE)
                {
                    mApp->PlayFoley(FoleyType::FOLEY_PLASTIC_HIT);
                }

                aZombie->TakeHelmDamage(ownerNowNs, 900, 0U);
            }
            else
            {
                aZombie->TakeDamage(ownerNowNs, visualNowNs, animationNowNs, 1800, 0U);
            }

            if ((!mApp->IsFirstTimeAdventureMode() || mApp->mPlayerInfo->mLevel > 10) && mSeedType == SeedType::SEED_WALLNUT)
            {
                // Bowling and ZenGarden are mutually exclusive: this original
                // auxiliary slot counts impacts here, in legacy integer units.
                mZenProductionCountdown += 1;
                // B 形态 -> 常量表 kBowlingBounceCoins（原 4 支链合并）：行序 =
                // impact count-2 直下标（==2/==3/==4 各一槽，>=5 收敛末槽）；
                // 四支共有的 SPAWN_SUN 音效提至分发前；>=5 支的成就发放为唯一
                // 动作差异，以 mAchv 位承载。多枚币按偏移升序逐枚投放在循环中
                // 保持原调用次序。
                static constexpr struct
                {
                    int mCount;
                    CoinType mCoin;
                    int mOffsets[3];
                    bool mAchv;
                } kBowlingBounceCoins[] = {
                    /* impacts == 2 */ { 1, CoinType::COIN_SILVER, { 0, 0, 0 }, false },
                    /* impacts == 3 */ { 2, CoinType::COIN_SILVER, { -5, 5, 0 }, false },
                    /* impacts == 4 */ { 3, CoinType::COIN_SILVER, { -10, 0, 10 }, false },
                    /* impacts >= 5 */ { 1, CoinType::COIN_GOLD, { 0, 0, 0 }, true },
                };
                static_assert(sizeof(kBowlingBounceCoins) / sizeof(kBowlingBounceCoins[0]) == 4, "kBowlingBounceCoins 槽位（2/3/4/>=5）");

                if (mZenProductionCountdown >= 2)
                {
                    const unsigned aBounceSlot = mZenProductionCountdown >= 5 ? 3u :
                        static_cast<unsigned>(static_cast<int>(mZenProductionCountdown)) - 2u;
                    const auto& aRow = kBowlingBounceCoins[aBounceSlot];
                    mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
                    for (int i = 0; i < aRow.mCount; i++)
                    {
                        mBoard->AddCoin(aPosX + aRow.mOffsets[i], aPosY, aRow.mCoin, CoinMotion::COIN_MOTION_COIN);
                    }
                    if (aRow.mAchv)
                    {
                        ReportAchievement::GiveAchievement(mApp, RollSomeHeads, true); // @Patoke: add achievement
                    }
                }
            }

            if (mSeedType != SeedType::SEED_GIANT_WALLNUT)
            {
                if (mRow == 4 || mState == PlantState::STATE_BOWLING_DOWN)
                {
                    aNewState = PlantState::STATE_BOWLING_UP;
                }
                else if (mRow == 0 || mState == PlantState::STATE_BOWLING_UP)
                {
                    aNewState = PlantState::STATE_BOWLING_DOWN;
                }
                else
                {
                    aNewState = Sexy::Rand(2) ? PlantState::STATE_BOWLING_UP : PlantState::STATE_BOWLING_DOWN;
                }
            }
        }

        // 格桶同步（2026-10-04 B 树实机抓获保龄球卡死）：弹跳换行直写 mRow
        // 曾不过桶——滚动坚果桶位置永留发射格，Die 按错误坐标摘链→桶链腐坏
        // 可成环→链走死循环。mPlantCol 全程不变，仅行迁移
        if (aNewState == PlantState::STATE_BOWLING_UP)
        {
            const int anOldRow = mRow;
            mRow--;
            mState = PlantState::STATE_BOWLING_UP;
            mRenderOrder = CalcRenderOrder();
            mBoard->MovePlantCellBucket(this, anOldRow, mPlantCol);
            mBowlingRowArrivalTimeNs = ReanimClock::Add(eventTime,
                aTravelTime(double(mBoard->GridToPixelY(0, mRow)) - mY));
        }
        else if (aNewState == PlantState::STATE_BOWLING_DOWN)
        {
            mState = PlantState::STATE_BOWLING_DOWN;
            mRenderOrder = CalcRenderOrder();
            const int anOldRow = mRow;
            mRow++;
            mBoard->MovePlantCellBucket(this, anOldRow, mPlantCol);
            mBowlingRowArrivalTimeNs = ReanimClock::Add(eventTime,
                aTravelTime(double(mBoard->GridToPixelY(0, mRow)) - mY));
        }
    };
    const bool aMoving = mState == PlantState::STATE_BOWLING_UP || mState == PlantState::STATE_BOWLING_DOWN;
    if (aMoving && mBowlingRowArrivalTimeNs == 0)
    {
        const auto aStart = ReanimClock::Subtract(aNow, static_cast<std::int64_t>(Sexy::gUpdateIntervalNs));
        mBowlingRowArrivalTimeNs = ReanimClock::Add(aStart,
            aTravelTime(double(mBoard->GridToPixelY(0, mRow)) - mY));
    }
    bool aProcessedNow = false;
    while (!mDead && (mState == PlantState::STATE_BOWLING_UP || mState == PlantState::STATE_BOWLING_DOWN) &&
        mBowlingRowArrivalTimeNs <= aNow)
    {
        const auto aArrival = mBowlingRowArrivalTimeNs;
        aProcessRow(aArrival, true);
        aProcessedNow = aArrival == aNow;
        if (!mDead && mBowlingRowArrivalTimeNs <= aArrival)
            throw std::overflow_error("bowling row arrival did not advance");
    }
    if (mDead)
        return;
    if (mState == PlantState::STATE_BOWLING_UP || mState == PlantState::STATE_BOWLING_DOWN)
    {
        const double aRemaining = ReanimClock::Subtract(mBowlingRowArrivalTimeNs, aNow) / 5'000'000.0;
        mY = CheckedPosition(double(mBoard->GridToPixelY(0, mRow)) +
            (mState == PlantState::STATE_BOWLING_UP ? aRemaining : -aRemaining));
    }
    if (!aProcessedNow)
        aProcessRow(aNow, false);
}

//0x463150
// GOTY @Patoke: 0x466B70
void Plant::UpdateAbilities(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (!IsInPlay())
        return;

    if (mState == PlantState::STATE_DOINGSPECIAL || mSquished)
    {
        const auto nowNs = ownerNowNs;
        if (mDisappearEndTimeNs == 0)
            mDisappearEndTimeNs = nowNs - static_cast<std::int64_t>(Sexy::gUpdateIntervalNs) + 2'000'000'000;
        if (nowNs > mDisappearEndTimeNs)
        {
            Die(ownerNowNs, visualNowNs, animationNowNs);
            return;
        }
    }

    if (mWakeUpEndTimeNs != 0)
    {
        const auto nowNs = ownerNowNs;
        const auto beforeNs = nowNs - static_cast<std::int64_t>(Sexy::gUpdateIntervalNs);
        const auto soundNs = mWakeUpEndTimeNs - 600'000'000;
        if (beforeNs < soundNs && nowNs >= soundNs)
            mApp->PlayFoley(FoleyType::FOLEY_WAKEUP);
        if (nowNs >= mWakeUpEndTimeNs)
        {
            mWakeUpEndTimeNs = 0;
            SetSleeping(ownerNowNs, animationNowNs, false);
        }
    }

    if (mIsAsleep || mSquished || mOnBungeeState != PlantOnBungeeState::NOT_ON_BUNGEE)
        return;

    UpdateShooting(ownerNowNs, visualNowNs, animationNowNs);

    if (mStateCountdown > 0)
        mStateCountdown--;

    if (mApp->IsWallnutBowlingLevel())
    {
        UpdateBowling(ownerNowNs, visualNowNs, animationNowNs);
        return;
    }

    // 各类型专属更新：按 SeedType 槽位直取成员函数指针（行序 = 枚举序 0..52）。
    // 空槽 = 无专属更新（射手走下方 SUBCLASS_SHOOTER 段，其余公共路径）。
    // 原 22 级 if-else 阶梯对未命中类型全链比较——射手/坚果/投手等常见类型
    // 每帧每株走完全部 22 次比较；MakesSun()||MARIGOLD 支已展开为生产槽位
    // （SUNSHROOM 被更早支截走，不占生产槽）。
        // 各类型专属更新：按 SeedType 槽位直取（行序 = 枚举序 0..52，槽内 lambda
    // 转发到命名成员函数，空槽 = 无专属更新——射手走 SUBCLASS_SHOOTER 段）。
    // 共享专属更新：多类型同函数（向日葵/双子/金盏花=生产，地刺/地刺王）——
    // 命名 constexpr lambda 复用同一闭包，不重复生成 thunk。
    static constexpr auto kProductionUpdate = [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateProductionPlant(ownerNowNs); };
    static constexpr auto kSpikeweedUpdate = [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateSpikeweed(ownerNowNs, visualNowNs, animationNowNs); };
    static constexpr void (*kSeedTypeUpdates[] )(std::int64_t, std::int64_t, std::int64_t, Plant*) = {
		/* SEED_PEASHOOTER        豌豆射手     */ nullptr,
		/* SEED_SUNFLOWER         向日葵      */ kProductionUpdate,
		/* SEED_CHERRYBOMB        樱桃炸弹     */ nullptr,
		/* SEED_WALLNUT           坚果       */ nullptr,
		/* SEED_POTATOMINE        土豆地雷     */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdatePotato(ownerNowNs, visualNowNs, animationNowNs); },
		/* SEED_SNOWPEA           寒冰射手     */ nullptr,
		/* SEED_CHOMPER           大嘴花      */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateChomper(ownerNowNs, visualNowNs, animationNowNs); },
		/* SEED_REPEATER          双发射手     */ nullptr,
		/* SEED_PUFFSHROOM        小喷菇      */ nullptr,
		/* SEED_SUNSHROOM         阳光菇      */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateSunShroom(ownerNowNs); },
		/* SEED_FUMESHROOM        大喷菇      */ nullptr,
		/* SEED_GRAVEBUSTER       墓碑吞噬者    */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateGraveBuster(ownerNowNs, visualNowNs, animationNowNs); },
		/* SEED_HYPNOSHROOM       魅惑菇      */ nullptr,
		/* SEED_SCAREDYSHROOM     胆小菇      */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateScaredyShroom(ownerNowNs); },
		/* SEED_ICESHROOM         寒冰菇      */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateIceShroom(ownerNowNs); },
		/* SEED_DOOMSHROOM        毁灭菇      */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateDoomShroom(ownerNowNs); },
		/* SEED_LILYPAD           睡莲       */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateLilypad(); },
		/* SEED_SQUASH            窝瓜       */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateSquash(ownerNowNs, visualNowNs, animationNowNs); },
		/* SEED_THREEPEATER       三线射手     */ nullptr,
		/* SEED_TANGLEKELP        缠绕海草     */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateTanglekelp(ownerNowNs, visualNowNs, animationNowNs); },
		/* SEED_JALAPENO          火爆辣椒     */ nullptr,
		/* SEED_SPIKEWEED         地刺       */ kSpikeweedUpdate,
		/* SEED_TORCHWOOD         火炬树桩     */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateTorchwood(); },
		/* SEED_TALLNUT                    */ nullptr,
		/* SEED_SEASHROOM                  */ nullptr,
		/* SEED_PLANTERN                   */ nullptr,
		/* SEED_CACTUS                     */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateCactus(ownerNowNs); },
		/* SEED_BLOVER                     */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateBlover(ownerNowNs, visualNowNs, animationNowNs); },
		/* SEED_SPLITPEA                   */ nullptr,
		/* SEED_STARFRUIT                  */ nullptr,
		/* SEED_PUMPKINSHELL               */ nullptr,
		/* SEED_MAGNETSHROOM               */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateMagnetShroom(ownerNowNs); },
		/* SEED_CABBAGEPULT                */ nullptr,
		/* SEED_FLOWERPOT                  */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateFlowerPot(); },
		/* SEED_KERNELPULT                 */ nullptr,
		/* SEED_INSTANT_COFFEE             */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateCoffeeBean(ownerNowNs, visualNowNs, animationNowNs); },
		/* SEED_GARLIC                     */ nullptr,
		/* SEED_UMBRELLA                   */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateUmbrella(ownerNowNs); },
		/* SEED_MARIGOLD                   */ kProductionUpdate,
		/* SEED_MELONPULT                  */ nullptr,
		/* SEED_GATLINGPEA                 */ nullptr,
		/* SEED_TWINSUNFLOWER              */ kProductionUpdate,
		/* SEED_GLOOMSHROOM                */ nullptr,
		/* SEED_CATTAIL                    */ nullptr,
		/* SEED_WINTERMELON                */ nullptr,
		/* SEED_GOLD_MAGNET                */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateGoldMagnetShroom(ownerNowNs, visualNowNs, animationNowNs); },
		/* SEED_SPIKEROCK                  */ kSpikeweedUpdate,
		/* SEED_COBCANNON                  */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateCobCannon(ownerNowNs); },
		/* SEED_IMITATER                   */ [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* p) { p->UpdateImitater(ownerNowNs, visualNowNs, animationNowNs); },
		/* SEED_EXPLODE_O_NUT              */ nullptr,
		/* SEED_GIANT_WALLNUT              */ nullptr,
		/* SEED_SPROUT                     */ nullptr,
		/* SEED_LEFTPEATER                 */ nullptr,
    };
    static_assert(sizeof(kSeedTypeUpdates) / sizeof(kSeedTypeUpdates[0]) == (int)SeedType::NUM_SEED_TYPES,
        "专属更新槽位须与 SeedType 枚举同步");
    if (void (*aUpdate )(std::int64_t, std::int64_t, std::int64_t, Plant*) = kSeedTypeUpdates[mSeedType])
    {
        aUpdate(ownerNowNs, visualNowNs, animationNowNs, this);
    }
    if (mSubclass == PlantSubClass::SUBCLASS_SHOOTER)
    {
        UpdateShooter(ownerNowNs, visualNowNs, animationNowNs);
    }
    if (mDoSpecialCountdown > 0)
    {
        mDoSpecialCountdown--;
        if (mDoSpecialCountdown == 0)
        {
            DoSpecial(ownerNowNs, visualNowNs, animationNowNs);
        }
    }
}

//0x463420
bool Plant::IsPartOfUpgradableTo(SeedType theUpgradedType)
{
    if (theUpgradedType == SeedType::SEED_COBCANNON && mSeedType == SeedType::SEED_KERNELPULT)
    {
        return mBoard->IsValidCobCannonSpot(mPlantCol, mRow) || mBoard->IsValidCobCannonSpot(mPlantCol - 1, mRow);
    }

    return IsUpgradableTo(theUpgradedType);
}

//0x463470
bool Plant::IsUpgradableTo(SeedType theUpgradedType)
{
    if (theUpgradedType == SeedType::SEED_GATLINGPEA && mSeedType == SeedType::SEED_REPEATER)
    {
        return true;
    }
    if (theUpgradedType == SeedType::SEED_WINTERMELON && mSeedType == SeedType::SEED_MELONPULT)
    {
        return true;
    }
    if (theUpgradedType == SeedType::SEED_TWINSUNFLOWER && mSeedType == SeedType::SEED_SUNFLOWER)
    {
        return true;
    }
    if (theUpgradedType == SeedType::SEED_SPIKEROCK && mSeedType == SeedType::SEED_SPIKEWEED)
    {
        return true;
    }
    if (theUpgradedType == SeedType::SEED_COBCANNON && mSeedType == SeedType::SEED_KERNELPULT)
    {
        return mBoard->IsValidCobCannonSpot(mPlantCol, mRow);
    }
    if (theUpgradedType == SeedType::SEED_GOLD_MAGNET && mSeedType == SeedType::SEED_MAGNETSHROOM)
    {
        return true;
    }
    if (theUpgradedType == SeedType::SEED_GLOOMSHROOM && mSeedType == SeedType::SEED_FUMESHROOM)
    {
        return true;
    }
    if (theUpgradedType == SeedType::SEED_CATTAIL && mSeedType == SeedType::SEED_LILYPAD)
    {
        Plant* aPlant = mBoard->GetTopPlantAt(mPlantCol, mRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
        return aPlant == nullptr || aPlant->mSeedType != SeedType::SEED_CATTAIL;
    }
    return false;
}

//0x4635C0
// GOTY @Patoke: 0x466FD0
void Plant::UpdateReanimColor(std::int64_t ownerNowNs)
{
    if (!mIsOnBoard)
        return;

    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

    if (aBodyReanim == nullptr)
        return;

    SeedType aSeedType = mBoard->GetSeedTypeInCursor();
    Color aColorOverride;

    bool isOnGlove = false;
    if (mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE)
    {
        Plant* aPlant = mBoard->mPlants.DataArrayTryToGet((unsigned int)mBoard->mCursorObject->mGlovePlantID);
        if (aPlant && aPlant->mPlantCol == mPlantCol && aPlant->mRow == mRow)
        {
            isOnGlove = true;
        }
    }

    if (isOnGlove)
    {
        aColorOverride = Color(128, 128, 128);
    }
    else if (IsPartOfUpgradableTo(aSeedType) && mBoard->CanPlantAt(mPlantCol, mRow, aSeedType) == PLANTING_OK)
    {
        aColorOverride = GetFlashingColorNs(ownerNowNs, 900'000'000);
    }
    else if (aSeedType == SeedType::SEED_COBCANNON && mSeedType == SeedType::SEED_KERNELPULT && mBoard->CanPlantAt(mPlantCol - 1, mRow, aSeedType) == PLANTING_OK)
    {
        aColorOverride = GetFlashingColorNs(ownerNowNs, 900'000'000);
    }
    else if (mSeedType == SeedType::SEED_EXPLODE_O_NUT)
    {
        aColorOverride = Color(255, 64, 64);
    }
    else
    {
        aColorOverride = Color(255, 255, 255);
    }

    aBodyReanim->mColorOverride = aColorOverride;

    if (mHighlighted)
    {
        aBodyReanim->mExtraAdditiveColor = Color(255, 255, 255, 196);
        aBodyReanim->mEnableExtraAdditiveDraw = true;
        if (mImitaterType == SeedType::SEED_IMITATER)
        {
            aBodyReanim->mExtraAdditiveColor = Color(255, 255, 255, 92);
        }
    }
    else if (mBeghouledFlashEndTimeNs > ownerNowNs)
    {
        int anAlpha = TodAnimateCurveNs(500'000'000, 0, std::max<std::int64_t>(0, mBeghouledFlashEndTimeNs - ownerNowNs) % 500'000'000, 0, 128, Curves::Bounce);
        aBodyReanim->mExtraAdditiveColor = Color(255, 255, 255, anAlpha);
        aBodyReanim->mEnableExtraAdditiveDraw = true;
    }
    else if (mEatenFlashEndTimeNs > ownerNowNs)
    {
        int aGrayness = ClampInt(static_cast<int>(std::max<std::int64_t>(0, mEatenFlashEndTimeNs - ownerNowNs) / 10'000'000) * 3, 0, mImitaterType == SeedType::SEED_IMITATER ? 128 : 255);
        aBodyReanim->mExtraAdditiveColor = Color(aGrayness, aGrayness, aGrayness);
        aBodyReanim->mEnableExtraAdditiveDraw = true;
    }
    else
    {
        aBodyReanim->mEnableExtraAdditiveDraw = false;
    }

    if (mBeghouledFlashEndTimeNs > ownerNowNs)
    {
        int anAlpha = TodAnimateCurveNs(500'000'000, 0, std::max<std::int64_t>(0, mBeghouledFlashEndTimeNs - ownerNowNs) % 500'000'000, 0, 128, Curves::Bounce);
        aBodyReanim->mExtraOverlayColor = Color(255, 255, 255, anAlpha);
        aBodyReanim->mEnableExtraOverlayDraw = true;
    }
    else
    {
        aBodyReanim->mEnableExtraOverlayDraw = false;
    }

    aBodyReanim->PropogateColorToAttachments();
}

//0x4638F0
bool Plant::IsInPlay()
{
    return mIsOnBoard && mApp->mGameMode != GameMode::GAMEMODE_LIMBO_ZEN_GARDEN && mApp->mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM;
}

//0x463920
void Plant::UpdateReanim(std::int64_t ownerNowNs, std::int64_t animationNowNs)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

    if (aBodyReanim == nullptr)
        return;

    UpdateReanimColor(ownerNowNs);

    float aOffsetX = mShakeOffsetX;
    float aOffsetY = PlantDrawHeightOffset(ownerNowNs, mBoard, this, mSeedType, mPlantCol, mRow);
    float aScaleX = 1.0f, aScaleY = 1.0f;
    if ((mApp->mGameMode == GameMode::GAMEMODE_LIMBO_BIG_TIME) &&
        (mSeedType == SeedType::SEED_WALLNUT || mSeedType == SeedType::SEED_SUNFLOWER || mSeedType == SeedType::SEED_MARIGOLD))
    {
        aScaleX = 1.5f;
        aScaleY = 1.5f;
        aOffsetX -= 20.0f;
        aOffsetY -= 40.0f;
    }
    if (mSeedType == SeedType::SEED_GIANT_WALLNUT)
    {
        aScaleX = 2.0f;
        aScaleY = 2.0f;
        aOffsetX -= 76.0f;
        aOffsetY -= 64.0f;
    }
    if (mSeedType == SeedType::SEED_INSTANT_COFFEE)
    {
        aScaleX = 0.8f;
        aScaleY = 0.8f;
        aOffsetX += 12.0f;
        aOffsetY += 10.0f;
    }
    if (mSeedType == SeedType::SEED_POTATOMINE)
    {
        aScaleX = 0.8f;
        aScaleY = 0.8f;
        aOffsetX += 12.0f;
        aOffsetY += 12.0f;
    }
    if (mState == PlantState::STATE_GRAVEBUSTER_EATING)
    {
        aOffsetY += TodAnimateCurveNs(4'000'000'000LL, 0, mStateCountdown.mNanoseconds, 0.0f, 30.0f, Curves::Linear);
    }
    if (mWakeUpEndTimeNs > 0 &&
        mWakeUpEndTimeNs > ownerNowNs)
    {
        float aScaleFactor = TodAnimateCurveNs(700'000'000, 0, std::max<std::int64_t>(0, mWakeUpEndTimeNs - ownerNowNs), 1.0f, 0.8f, Curves::EaseSinWave);
        aScaleY *= aScaleFactor;
        aOffsetY += 80.0f - 80.0f * aScaleFactor;
    }

    aBodyReanim->UpdateAt(animationNowNs);
    if (auto* sleepingReanim = mApp->ReanimationTryToGet(mSleepingReanimID))
        sleepingReanim->UpdateAt(animationNowNs);

    if (mSeedType == SeedType::SEED_LEFTPEATER)
    {
        aOffsetX += 80.0f * aScaleX;
        aScaleX *= -1.0f;
    }

    if (mPottedPlantIndex != -1)
    {
        PottedPlant* aPottedPlant = &mApp->mPlayerInfo->mPottedPlant[mPottedPlantIndex];

        if (aPottedPlant->mFacing == PottedPlant::FacingDirection::FACING_LEFT)
        {
            aOffsetX += 80.0f * aScaleX;
            aScaleX *= -1.0f;
        }

        // B 形态 -> 常量表 kPottedPlantAgeClip（原 3 支 6 赋值链；行序 =
        // PottedPlantAge 枚举数值序 SPROUT(0)..FULL(3) 全段 4 槽）。原链无
        // SPROUT 独立支：SPROUT 与表外值均落原 else 支（= FULL 参数），
        // 三处共体以具名行 kPottedPlantFullGrownClip 复用，任意输入语义
        // 与原链逐值一致。
        struct PottedPlantClip
        {
            float mOffsetXStart;
            float mOffsetXEnd;
            float mOffsetYStart;
            float mOffsetYEnd;
            float mScaleStart;
            float mScaleEnd;
        };
        static constexpr PottedPlantClip kPottedPlantFullGrownClip = { 10.0f, 0.0f, 20.0f, 0.0f, 0.75f, 1.0f };
        static constexpr PottedPlantClip kPottedPlantAgeClip[(int)PottedPlantAge::PLANTAGE_FULL + 1] = {
            /* [0] PLANTAGE_SPROUT（原链落 else 支） */ kPottedPlantFullGrownClip,
            /* [1] PLANTAGE_SMALL  */ { 20.0f, 20.0f, 40.0f, 40.0f, 0.5f, 0.5f },
            /* [2] PLANTAGE_MEDIUM */ { 20.0f, 10.0f, 40.0f, 20.0f, 0.5f, 0.75f },
            /* [3] PLANTAGE_FULL（原 else 支） */ kPottedPlantFullGrownClip,
        };
        static_assert((int)PottedPlantAge::PLANTAGE_SPROUT == 0 && (int)PottedPlantAge::PLANTAGE_SMALL == 1 &&
            (int)PottedPlantAge::PLANTAGE_MEDIUM == 2 && (int)PottedPlantAge::PLANTAGE_FULL == 3,
            "PottedPlantAge 键值须与 kPottedPlantAgeClip 行序一致");

        float aOffsetXStart, aOffsetXEnd;
        float aOffsetYStart, aOffsetYEnd;
        float aScaleStart, aScaleEnd;
        const unsigned aAgeIndex = static_cast<unsigned>(aPottedPlant->mPlantAge);
        const PottedPlantClip& aClip = aAgeIndex < sizeof(kPottedPlantAgeClip) / sizeof(kPottedPlantAgeClip[0]) ?
            kPottedPlantAgeClip[aAgeIndex] : kPottedPlantFullGrownClip;
        aOffsetXStart = aClip.mOffsetXStart;
        aOffsetXEnd = aClip.mOffsetXEnd;
        aOffsetYStart = aClip.mOffsetYStart;
        aOffsetYEnd = aClip.mOffsetYEnd;
        aScaleStart = aClip.mScaleStart;
        aScaleEnd = aClip.mScaleEnd;

        float aAnimatedOffsetX = TodAnimateCurveNs(1'000'000'000, 0, mStateCountdown.mNanoseconds, aOffsetXStart, aOffsetXEnd, Curves::Linear);
        float aAnimatedOffsetY = TodAnimateCurveNs(1'000'000'000, 0, mStateCountdown.mNanoseconds, aOffsetYStart, aOffsetYEnd, Curves::Linear);
        float aAnimatedScale = TodAnimateCurveNs(1'000'000'000, 0, mStateCountdown.mNanoseconds, aScaleStart, aScaleEnd, Curves::Linear);

        aOffsetX += aAnimatedOffsetX * aScaleX;
        aOffsetY += aAnimatedOffsetY * aScaleY;
        aScaleX *= aAnimatedScale;
        aScaleY *= aAnimatedScale;
        aOffsetX += mApp->mZenGarden->ZenPlantOffsetX(aPottedPlant);
        aOffsetY += mApp->mZenGarden->PlantPottedDrawHeightOffset(mSeedType, aScaleY);
    }

    aBodyReanim->mOverlayMatrix.m02 = aOffsetX;
    aBodyReanim->mOverlayMatrix.m12 = aOffsetY;
    aBodyReanim->mOverlayMatrix.m00 = aScaleX;
    aBodyReanim->mOverlayMatrix.m11 = aScaleY;
}

//0x463E40
void Plant::Update(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, std::uint64_t theLevelDeltaNs)
{
    mLaunchClockActive = false;
    bool doUpdate = false;
    if (mIsOnBoard && mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && mApp->IsWallnutBowlingLevel())
        doUpdate = true;
    else if (mIsOnBoard && mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
        doUpdate = true;
    else if (mIsOnBoard && mBoard->mCutScene->ShouldRunUpsellBoard())
        doUpdate = true;
    else if (!mIsOnBoard || mApp->mGameScene == GameScenes::SCENE_PLAYING)
        doUpdate = true;

    for (const auto id : {mBodyReanimID, mHeadReanimID, mHeadReanimID2, mHeadReanimID3,
        mBlinkReanimID, mLightReanimID, mSleepingReanimID})
        if (auto* animation = mApp->ReanimationTryToGet(id))
        {
            animation->PrepareTimeAt(animationNowNs);
        }
    if (mShootingEndTimeNs != 0 && (!doUpdate || mIsAsleep || mSquished ||
        mOnBungeeState != PlantOnBungeeState::NOT_ON_BUNGEE || NotOnGround()))
        mShootingEndTimeNs = ReanimClock::Add(mShootingEndTimeNs, static_cast<std::int64_t>(theLevelDeltaNs));
    if (doUpdate)
    {
        UpdateAbilities(ownerNowNs, visualNowNs, animationNowNs);
        Animate(ownerNowNs, animationNowNs);

        if (mPlantHealth < 0)
            Die(ownerNowNs, visualNowNs, animationNowNs);

        UpdateReanim(ownerNowNs, animationNowNs);
    }
    if (!mLaunchClockActive)
    {
        // A plant born during this step already starts at now, not at step begin.
        const auto aNow = ownerNowNs;
        const auto aFreezeNs = std::min<std::uint64_t>(theLevelDeltaNs,
            aNow > mLaunchWindowStartNs ? aNow - mLaunchWindowStartNs : 0);
        mNextLaunchTimeNs = PlantLaunchClock::Add(mNextLaunchTimeNs, aFreezeNs);
        mLaunchWindowStartNs = PlantLaunchClock::Add(mLaunchWindowStartNs, aFreezeNs);
    }
}

//0x463EF0
bool Plant::NotOnGround()
{
    if (mSeedType == SeedType::SEED_SQUASH)
    {
        if (mState == PlantState::STATE_SQUASH_RISING || mState == PlantState::STATE_SQUASH_FALLING || mState == PlantState::STATE_SQUASH_DONE_FALLING)
            return true;
    }

    return mSquished || mOnBungeeState == PlantOnBungeeState::RISING_WITH_BUNGEE || mDead;
}

//0x463F30
Reanimation* Plant::AttachBlinkAnim(std::int64_t ownerNowNs, std::int64_t animationNowNs, Reanimation* theReanimBody)
{
    const PlantDefinition& aPlantDef = GetPlantDefinition(mSeedType);
    LawnApp* aApp = (LawnApp*)gSexyAppBase;
    Reanimation* aAnimToAttach = theReanimBody;
    ReanimTrackId aTrackToPlay = ReanimTrackId::anim_blink;
    ReanimTrackId aTrackToAttach = ReanimTrackId::None;

    // C 形态 -> 匿名 lambda 函数指针表 kBlinkCases（仿 kSeedTypeUpdates 范式）：
    // 行序 = SeedType 枚举序 0..52；nullptr = 无 case 槽（保持初始三元组 =
    // 原链落空语义）。扫描报告原判 B 形态，但五支均含 Rand/TrackExists/
    // FindReanimAttachment 运行期逻辑（非纯 {track,image} 数据，数据表需
    // 内嵌解释器），按规则 2 落 C 形态。WALLNUT 族四种子与豌豆族五种子为
    // 共体（constexpr 指针变量复用）。三元组选择结果经 BlinkSelection 回写。
    struct BlinkSelection
    {
        Reanimation* mAnimToAttach;
        ReanimTrackId mTrackToPlay;
        ReanimTrackId mTrackToAttach;
    };
    using BlinkCaseFn = void (*)(std::int64_t, Plant&, Reanimation*, BlinkSelection&);
    static constexpr BlinkCaseFn kBlinkWallnutFamily = [](std::int64_t ownerNowNs, Plant& p, Reanimation* theReanimBody, BlinkSelection& aSel)
    {
        int aHit = Rand(10);
        if (aHit < 1 && theReanimBody->TrackExists(ReanimTrackId::anim_blink_twitch))
        {
            aSel.mTrackToPlay = ReanimTrackId::anim_blink_twitch;
        }
        else
        {
            aSel.mTrackToPlay = aHit < 7 ? ReanimTrackId::anim_blink_twice : ReanimTrackId::anim_blink_thrice;
        }
    };
    static constexpr BlinkCaseFn kBlinkThreepeater = [](std::int64_t ownerNowNs, Plant& p, Reanimation* theReanimBody, BlinkSelection& aSel)
    {
        int aHit = Rand(3);
        if (aHit == 0)
        {
            aSel.mTrackToPlay = ReanimTrackId::anim_blink1;
            aSel.mTrackToAttach = ReanimTrackId::anim_face1;
            ReanimatorTrackInstance* aTrackInstance = theReanimBody->GetTrackInstanceByName(ReanimTrackId::anim_head1);
            aSel.mAnimToAttach = FindReanimAttachment(aTrackInstance->mAttachmentID);
        }
        else if (aHit == 1)
        {
            aSel.mTrackToPlay = ReanimTrackId::anim_blink2;
            aSel.mTrackToAttach = ReanimTrackId::anim_face2;
            ReanimatorTrackInstance* aTrackInstance = theReanimBody->GetTrackInstanceByName(ReanimTrackId::anim_head2);
            aSel.mAnimToAttach = FindReanimAttachment(aTrackInstance->mAttachmentID);
        }
        else
        {
            aSel.mTrackToPlay = ReanimTrackId::anim_blink3;
            aSel.mTrackToAttach = ReanimTrackId::anim_face3;
            ReanimatorTrackInstance* aTrackInstance = theReanimBody->GetTrackInstanceByName(ReanimTrackId::anim_head3);
            aSel.mAnimToAttach = FindReanimAttachment(aTrackInstance->mAttachmentID);
        }
    };
    static constexpr BlinkCaseFn kBlinkSplitPea = [](std::int64_t ownerNowNs, Plant& p, Reanimation* theReanimBody, BlinkSelection& aSel)
    {
        if (Rand(2) == 0)
        {
            aSel.mTrackToPlay = ReanimTrackId::anim_blink;
            aSel.mTrackToAttach = ReanimTrackId::anim_face;
            aSel.mAnimToAttach = p.mApp->ReanimationTryToGet(p.mHeadReanimID);
        }
        else
        {
            aSel.mTrackToPlay = ReanimTrackId::anim_blink2;
            aSel.mTrackToAttach = ReanimTrackId::anim_face2;
            aSel.mAnimToAttach = p.mApp->ReanimationTryToGet(p.mHeadReanimID2);
        }
    };
    static constexpr BlinkCaseFn kBlinkTwinSunflower = [](std::int64_t ownerNowNs, Plant& p, Reanimation* theReanimBody, BlinkSelection& aSel)
    {
        if (Rand(2) == 0)
        {
            aSel.mTrackToPlay = ReanimTrackId::anim_blink;
            aSel.mTrackToAttach = ReanimTrackId::anim_face;
        }
        else
        {
            aSel.mTrackToPlay = ReanimTrackId::anim_blink2;
            aSel.mTrackToAttach = ReanimTrackId::anim_face2;
        }
    };
    static constexpr BlinkCaseFn kBlinkPeaFamily = [](std::int64_t ownerNowNs, Plant& p, Reanimation* theReanimBody, BlinkSelection& aSel)
    {
        if (theReanimBody->TrackExists(ReanimTrackId::anim_stem))
        {
            ReanimatorTrackInstance* aTrackInstance = theReanimBody->GetTrackInstanceByName(ReanimTrackId::anim_stem);
            aSel.mAnimToAttach = FindReanimAttachment(aTrackInstance->mAttachmentID);
        }
        else if (theReanimBody->TrackExists(ReanimTrackId::anim_idle))
        {
            ReanimatorTrackInstance* aTrackInstance = theReanimBody->GetTrackInstanceByName(ReanimTrackId::anim_idle);
            aSel.mAnimToAttach = FindReanimAttachment(aTrackInstance->mAttachmentID);
        }
    };
    static constexpr BlinkCaseFn kBlinkCases[(int)SeedType::NUM_SEED_TYPES] = {
        /* SEED_PEASHOOTER        豌豆射手     */ kBlinkPeaFamily,
        /* SEED_SUNFLOWER         向日葵      */ nullptr,
        /* SEED_CHERRYBOMB        樱桃炸弹     */ nullptr,
        /* SEED_WALLNUT           坚果       */ kBlinkWallnutFamily,
        /* SEED_POTATOMINE        土豆地雷     */ nullptr,
        /* SEED_SNOWPEA           寒冰射手     */ kBlinkPeaFamily,
        /* SEED_CHOMPER           大嘴花      */ nullptr,
        /* SEED_REPEATER          双发射手     */ kBlinkPeaFamily,
        /* SEED_PUFFSHROOM        小喷菇      */ nullptr,
        /* SEED_SUNSHROOM         阳光菇      */ nullptr,
        /* SEED_FUMESHROOM        大喷菇      */ nullptr,
        /* SEED_GRAVEBUSTER       墓碑吞噬者    */ nullptr,
        /* SEED_HYPNOSHROOM       魅惑菇      */ nullptr,
        /* SEED_SCAREDYSHROOM     胆小菇      */ nullptr,
        /* SEED_ICESHROOM         寒冰菇      */ nullptr,
        /* SEED_DOOMSHROOM        毁灭菇      */ nullptr,
        /* SEED_LILYPAD           睡莲       */ nullptr,
        /* SEED_SQUASH            窝瓜       */ nullptr,
        /* SEED_THREEPEATER       三线射手     */ kBlinkThreepeater,
        /* SEED_TANGLEKELP        缠绕海草     */ nullptr,
        /* SEED_JALAPENO          火爆辣椒     */ nullptr,
        /* SEED_SPIKEWEED         地刺       */ nullptr,
        /* SEED_TORCHWOOD         火炬树桩     */ nullptr,
        /* SEED_TALLNUT                    */ kBlinkWallnutFamily,
        /* SEED_SEASHROOM                  */ nullptr,
        /* SEED_PLANTERN                   */ nullptr,
        /* SEED_CACTUS                     */ nullptr,
        /* SEED_BLOVER                     */ nullptr,
        /* SEED_SPLITPEA                   */ kBlinkSplitPea,
        /* SEED_STARFRUIT                  */ nullptr,
        /* SEED_PUMPKINSHELL               */ nullptr,
        /* SEED_MAGNETSHROOM               */ nullptr,
        /* SEED_CABBAGEPULT                */ nullptr,
        /* SEED_FLOWERPOT                  */ nullptr,
        /* SEED_KERNELPULT                 */ nullptr,
        /* SEED_INSTANT_COFFEE             */ nullptr,
        /* SEED_GARLIC                     */ nullptr,
        /* SEED_UMBRELLA                   */ nullptr,
        /* SEED_MARIGOLD                   */ nullptr,
        /* SEED_MELONPULT                  */ nullptr,
        /* SEED_GATLINGPEA                 */ kBlinkPeaFamily,
        /* SEED_TWINSUNFLOWER              */ kBlinkTwinSunflower,
        /* SEED_GLOOMSHROOM                */ nullptr,
        /* SEED_CATTAIL                    */ nullptr,
        /* SEED_WINTERMELON                */ nullptr,
        /* SEED_GOLD_MAGNET                */ nullptr,
        /* SEED_SPIKEROCK                  */ nullptr,
        /* SEED_COBCANNON                  */ nullptr,
        /* SEED_IMITATER                   */ nullptr,
        /* SEED_EXPLODE_O_NUT              */ kBlinkWallnutFamily,
        /* SEED_GIANT_WALLNUT              */ kBlinkWallnutFamily,
        /* SEED_SPROUT                     */ nullptr,
        /* SEED_LEFTPEATER                 */ kBlinkPeaFamily,
    };
    static_assert(sizeof(kBlinkCases) / sizeof(kBlinkCases[0]) == (int)SeedType::NUM_SEED_TYPES,
        "眨眼槽位须与 SeedType 枚举同步");

    BlinkSelection aSelection{aAnimToAttach, aTrackToPlay, aTrackToAttach};
    if ((unsigned)mSeedType < sizeof(kBlinkCases) / sizeof(kBlinkCases[0]) &&
        kBlinkCases[mSeedType] != nullptr)
    {
        kBlinkCases[mSeedType](ownerNowNs, *this, theReanimBody, aSelection);
    }
    aAnimToAttach = aSelection.mAnimToAttach;
    aTrackToPlay = aSelection.mTrackToPlay;
    aTrackToAttach = aSelection.mTrackToAttach;

    if (aAnimToAttach == nullptr)
    {
        TodTrace("Missing head anim");
        return nullptr;
    }

    if (!theReanimBody->TrackExists(aTrackToPlay))
        return nullptr;

    Reanimation* aBlinkReanim = aApp->AddReanimation(0.0f, 0.0f, 0, aPlantDef.mReanimationType, animationNowNs);
    aBlinkReanim->SetFramesForLayer(aTrackToPlay);
    aBlinkReanim->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME_AND_HOLD);
    aBlinkReanim->ApplyRate(15.0f);
    aBlinkReanim->mColorOverride = theReanimBody->mColorOverride;

    if (aTrackToAttach != ReanimTrackId::None && aAnimToAttach->TrackExists(aTrackToAttach))
    {
        aBlinkReanim->AttachToAnotherReanimation(aAnimToAttach, aTrackToAttach);
    }
    else if (aAnimToAttach->TrackExists(ReanimTrackId::anim_face))
    {
        aBlinkReanim->AttachToAnotherReanimation(aAnimToAttach, ReanimTrackId::anim_face);
    }
    else if (aAnimToAttach->TrackExists(ReanimTrackId::anim_idle))
    {
        aBlinkReanim->AttachToAnotherReanimation(aAnimToAttach, ReanimTrackId::anim_idle);
    }
    else
    {
        TodTrace("Missing anim_idle for blink");
    }

    aBlinkReanim->mFilterEffect = theReanimBody->mFilterEffect;
    return aBlinkReanim;
}

//0x464230
void Plant::DoBlink(std::int64_t ownerNowNs, std::int64_t animationNowNs)
{
    mBlinkCountdown = 400 + Rand(400);

    if (NotOnGround() || mShootingEndTimeNs != 0)
        return;

    if (mSeedType == SeedType::SEED_POTATOMINE && mState != PlantState::STATE_POTATO_ARMED)
        return;

    if (mState == PlantState::STATE_CACTUS_RISING || mState == PlantState::STATE_CACTUS_HIGH || mState == PlantState::STATE_CACTUS_LOWERING ||
        mState == PlantState::STATE_MAGNETSHROOM_SUCKING || mState == PlantState::STATE_MAGNETSHROOM_CHARGING)
        return;

    EndBlink();
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

    if (aBodyReanim == nullptr)
        return;

    if ((mSeedType == SeedType::SEED_TALLNUT && aBodyReanim->VariantOption(ReanimTrackId::anim_idle) == ReanimOptions::Tallnut::anim_idle::TALLNUT_CRACKED2) ||
        (mSeedType == SeedType::SEED_GARLIC && aBodyReanim->VariantOption(ReanimTrackId::anim_face) == ReanimOptions::Garlic::anim_face::GARLIC_BODY3))
        return;

    if (mSeedType == SeedType::SEED_WALLNUT || mSeedType == SeedType::SEED_TALLNUT ||
        mSeedType == SeedType::SEED_EXPLODE_O_NUT || mSeedType == SeedType::SEED_GIANT_WALLNUT)
    {
        mBlinkCountdown = 1000 + Rand(1000);
    }

    Reanimation* aBlinkReanim = AttachBlinkAnim(ownerNowNs, animationNowNs, aBodyReanim);
    if (aBlinkReanim)
    {
        mBlinkReanimID = mApp->ReanimationGetID(aBlinkReanim);
    }
    aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::anim_eye, RENDER_GROUP_HIDDEN);
}

//0x464390
void Plant::EndBlink()
{
    if (mBlinkReanimID != ReanimationID::REANIMATIONID_NULL)
    {
        mApp->RemoveReanimation(mBlinkReanimID);
        mBlinkReanimID = ReanimationID::REANIMATIONID_NULL;

        Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);
        if (aBodyReanim)
        {
            aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::anim_eye, RENDER_GROUP_NORMAL);
        }
    }
}

//0x464410
void Plant::UpdateBlink(std::int64_t ownerNowNs, std::int64_t animationNowNs)
{
    if (mBlinkReanimID != ReanimationID::REANIMATIONID_NULL)
    {
        Reanimation* aBlinkReanim = mApp->ReanimationTryToGet(mBlinkReanimID);

        if (aBlinkReanim == nullptr || aBlinkReanim->mLoopCount > 0)
        {
            EndBlink();
        }
    }

    if (mIsAsleep)
        return;

    if (mBlinkCountdown > 0)
    {
        mBlinkCountdown--;
        if (mBlinkCountdown == 0)
        {
            DoBlink(ownerNowNs, animationNowNs);
        }
    }
}

//0x464480
void Plant::AnimateNuts(std::int64_t ownerNowNs)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

    if (aBodyReanim == nullptr)
        return;

    std::uint32_t aCracked1;
    std::uint32_t aCracked2;
    ReanimTrackId aTrackToOverride;
    if (mSeedType == SeedType::SEED_WALLNUT)
    {
        aCracked1 = ReanimOptions::Wallnut::anim_face::WALLNUT_CRACKED1;
        aCracked2 = ReanimOptions::Wallnut::anim_face::WALLNUT_CRACKED2;
        aTrackToOverride = ReanimTrackId::anim_face;
    }
    else if (mSeedType == SeedType::SEED_TALLNUT)
    {
        aCracked1 = ReanimOptions::Tallnut::anim_idle::TALLNUT_CRACKED1;
        aCracked2 = ReanimOptions::Tallnut::anim_idle::TALLNUT_CRACKED2;
        aTrackToOverride = ReanimTrackId::anim_idle;
    }
    else return;

    int aPosX = (mX) + 40;
    int aPosY = (mY) + 10;
    if (mSeedType == SeedType::SEED_TALLNUT)
    {
        aPosY -= 32;
    }

    const auto aAppearance = aBodyReanim->VariantOption(aTrackToOverride);
    if (mPlantHealth < mPlantMaxHealth / 3)
    {
        if (aAppearance != aCracked2)
        {
            aBodyReanim->SetVariantOption(aTrackToOverride, aCracked2);
            mApp->AddTodParticle(aPosX, aPosY, mRenderOrder + 4, ParticleEffect::PARTICLE_WALLNUT_EAT_LARGE, ownerNowNs);
        }
    }
    else if (mPlantHealth < mPlantMaxHealth * 2 / 3)
    {
        if (aAppearance != aCracked1)
        {
            aBodyReanim->SetVariantOption(aTrackToOverride, aCracked1);
            mApp->AddTodParticle(aPosX, aPosY, mRenderOrder + 4, ParticleEffect::PARTICLE_WALLNUT_EAT_LARGE, ownerNowNs);
        }
    }
    else
    {
        aBodyReanim->SetVariantOption(aTrackToOverride, 0);
    }

    if (IsInPlay() && !mApp->IsIZombieLevel())
    {
        if (mRecentlyEatenEndTimeNs > ownerNowNs)
        {
            aBodyReanim->ApplyRate(0.1f);
            return;
        }

        if (aBodyReanim->mAnimRate < 1.0f && mOnBungeeState != PlantOnBungeeState::RISING_WITH_BUNGEE)
        {
            aBodyReanim->ApplyRate(RandRangeFloat(10.0f, 15.0f));
        }
    }
}

//0x464680
void Plant::AnimateGarlic()
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    const auto aAppearance = aBodyReanim->VariantOption(ReanimTrackId::anim_face);

    if (mPlantHealth < mPlantMaxHealth / 3)
    {
        if (aAppearance != ReanimOptions::Garlic::anim_face::GARLIC_BODY3)
        {
            aBodyReanim->SetVariantOption(ReanimTrackId::anim_face, ReanimOptions::Garlic::anim_face::GARLIC_BODY3);
            aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Garlic_stem, RENDER_GROUP_HIDDEN);
        }
    }
    else if (mPlantHealth < mPlantMaxHealth * 2 / 3)
    {
        if (aAppearance != ReanimOptions::Garlic::anim_face::GARLIC_BODY2)
        {
            aBodyReanim->SetVariantOption(ReanimTrackId::anim_face, ReanimOptions::Garlic::anim_face::GARLIC_BODY2);
        }
    }
    else
    {
        aBodyReanim->SetVariantOption(ReanimTrackId::anim_face, 0);
    }
}

//0x464760
void Plant::AnimatePumpkin()
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);
    const auto aAppearance = aBodyReanim->VariantOption(ReanimTrackId::Pumpkin_front);

    if (mPlantHealth < mPlantMaxHealth / 3)
    {
        if (aAppearance != ReanimOptions::Pumpkin::Pumpkin_front::PUMPKIN_DAMAGE3)
            aBodyReanim->SetVariantOption(ReanimTrackId::Pumpkin_front, ReanimOptions::Pumpkin::Pumpkin_front::PUMPKIN_DAMAGE3);
    }
    else if (mPlantHealth < mPlantMaxHealth * 2 / 3)
    {
        if (aAppearance != ReanimOptions::Pumpkin::Pumpkin_front::PUMPKIN_DAMAGE1)
            aBodyReanim->SetVariantOption(ReanimTrackId::Pumpkin_front, ReanimOptions::Pumpkin::Pumpkin_front::PUMPKIN_DAMAGE1);
    }
    else
    {
        aBodyReanim->SetVariantOption(ReanimTrackId::Pumpkin_front, 0);
    }
}

//0x464820
void Plant::UpdateShooting(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (NotOnGround() || mShootingEndTimeNs == 0)
        return;

    // Use the owner's interval, never decrement a stored countdown. Keep an
    // expired deadline active until animation cleanup completes. Zero alone
    // is inactive; old invalid checkpoints are repaired in DevTools only.
    const auto beforeNs = ownerNowNs - static_cast<std::int64_t>(Sexy::gUpdateIntervalNs);
    const auto crossed = [&](std::int64_t remainingUnits) {
        const auto eventNs = mShootingEndTimeNs - remainingUnits * PlantLaunchClock::Tick;
        return beforeNs < eventNs && eventNs <= ownerNowNs;
    };

    if (mSeedType == SeedType::SEED_FUMESHROOM && crossed(15))
    {
        int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mRow, 0);
        AddAttachedParticle(ownerNowNs, (mX) + 85, (mY) + 31, aRenderPosition, ParticleEffect::PARTICLE_FUMECLOUD);
    }

    if (mSeedType == SeedType::SEED_GLOOMSHROOM)
    {
        for (const auto offset : {136, 108, 80, 52}) if (crossed(offset))
        {
            int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, mRow, 0);
            AddAttachedParticle(ownerNowNs, (mX) + 40, (mY) + 40, aRenderPosition, ParticleEffect::PARTICLE_GLOOMCLOUD);
        }
        for (const auto offset : {126, 98, 70, 42}) if (crossed(offset))
        {
            Fire(ownerNowNs, visualNowNs, animationNowNs, nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
        }
    }
    else if (mSeedType == SeedType::SEED_GATLINGPEA)
    {
        for (const auto offset : {68, 51, 35, 18}) if (crossed(offset))
        {
            Fire(ownerNowNs, visualNowNs, animationNowNs, nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
        }
    }
    else if (mSeedType == SeedType::SEED_CATTAIL)
    {
        if (crossed(19))
        {
            Zombie* aZombie = FindTargetZombie(mRow, PlantWeapon::WEAPON_PRIMARY);
            if (aZombie)
            {
                Fire(ownerNowNs, visualNowNs, animationNowNs, aZombie, mRow, PlantWeapon::WEAPON_PRIMARY);
            }
        }
    }
    else if (crossed(1))
    {
        if (mSeedType == SeedType::SEED_THREEPEATER)
        {
            int rowAbove = mRow - 1;
            int rowBelow = mRow + 1;
            Reanimation* aHeadReanim2 = mApp->ReanimationGet(mHeadReanimID2);

            Reanimation* aHeadReanim3 = mApp->ReanimationGet(mHeadReanimID3);

            Reanimation* aHeadReanim1 = mApp->ReanimationGet(mHeadReanimID);


            if (aHeadReanim1->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
            {
                Fire(ownerNowNs, visualNowNs, animationNowNs, nullptr, rowBelow, PlantWeapon::WEAPON_PRIMARY);
            }
            if (aHeadReanim2->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
            {
                Fire(ownerNowNs, visualNowNs, animationNowNs, nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
            }
            if (aHeadReanim3->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
            {
                Fire(ownerNowNs, visualNowNs, animationNowNs, nullptr, rowAbove, PlantWeapon::WEAPON_PRIMARY);
            }
        }
        else if (mSeedType == SeedType::SEED_SPLITPEA)
        {
            Reanimation* aHeadBackReanim = mApp->ReanimationTryToGet(mHeadReanimID2);

            Reanimation* aHeadFrontReanim = mApp->ReanimationTryToGet(mHeadReanimID);

            if (aHeadFrontReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
            {
                Fire(ownerNowNs, visualNowNs, animationNowNs, nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
            }
            if (aHeadBackReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
            {
                Fire(ownerNowNs, visualNowNs, animationNowNs, nullptr, mRow, PlantWeapon::WEAPON_SECONDARY);
            }
        }
        else if (mState == PlantState::STATE_CACTUS_LOW)
        {
            Fire(ownerNowNs, visualNowNs, animationNowNs, nullptr, mRow, PlantWeapon::WEAPON_SECONDARY);
        }
        else if (mSeedType == SeedType::SEED_CABBAGEPULT || mSeedType == SeedType::SEED_KERNELPULT || mSeedType == SeedType::SEED_MELONPULT || mSeedType == SeedType::SEED_WINTERMELON)
        {
            PlantWeapon aPlantWeapon = PlantWeapon::WEAPON_PRIMARY;
            if (mState == PlantState::STATE_KERNELPULT_BUTTER)
            {
                Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

                aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Cornpult_butter, RENDER_GROUP_HIDDEN);
                aBodyReanim->AssignRenderGroupToRange(ReanimTrackRanges::Cornpult_kernal, RENDER_GROUP_NORMAL);
                mState = PlantState::STATE_NOTREADY;
                aPlantWeapon = PlantWeapon::WEAPON_SECONDARY;
            }

            Zombie* aZombie = FindTargetZombie(mRow, aPlantWeapon);
            Fire(ownerNowNs, visualNowNs, animationNowNs, aZombie, mRow, aPlantWeapon);
        }
        else
        {
            Fire(ownerNowNs, visualNowNs, animationNowNs, nullptr, mRow, PlantWeapon::WEAPON_PRIMARY);
        }

        // A variable step can cross both the shot threshold and expiry. Keep
        // processing zero below; returning here would leave the held attack
        // animation/cob state permanently unprocessed on the next call's guard.
    }

    if (ownerNowNs < mShootingEndTimeNs)
        return;

    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

    Reanimation* aHeadReanim = mApp->ReanimationTryToGet(mHeadReanimID);

    if (mSeedType == SeedType::SEED_THREEPEATER)
    {
        Reanimation* aHeadReanim2 = mApp->ReanimationGet(mHeadReanimID2);

        Reanimation* aHeadReanim3 = mApp->ReanimationGet(mHeadReanimID3);


        if (aHeadReanim2->mLoopCount > 0)
        {
            if (aHeadReanim->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
            {
                aHeadReanim->StartBlend(20);
                aHeadReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
                aHeadReanim->SetFramesForLayer(ReanimTrackId::anim_head_idle1);
                aHeadReanim->ApplyRate(aBodyReanim->mAnimRate);
                aHeadReanim->SynchronizePhase(*aBodyReanim);
            }

            aHeadReanim2->StartBlend(20);
            aHeadReanim2->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
            aHeadReanim2->SetFramesForLayer(ReanimTrackId::anim_head_idle2);
            aHeadReanim2->ApplyRate(aBodyReanim->mAnimRate);
            aHeadReanim2->SynchronizePhase(*aBodyReanim);

            if (aHeadReanim3->mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
            {
                aHeadReanim3->StartBlend(20);
                aHeadReanim3->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
                aHeadReanim3->SetFramesForLayer(ReanimTrackId::anim_head_idle3);
                aHeadReanim3->ApplyRate(aBodyReanim->mAnimRate);
                aHeadReanim3->SynchronizePhase(*aBodyReanim);
            }

            mShootingEndTimeNs = 0;
            return;
        }
    }
    else if (mSeedType == SeedType::SEED_SPLITPEA)
    {
        Reanimation* aHeadReanim2 = mApp->ReanimationGet(mHeadReanimID2);


        if (aHeadReanim->mLoopCount > 0)
        {
            aHeadReanim->StartBlend(20);
            aHeadReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
            aHeadReanim->SetFramesForLayer(ReanimTrackId::anim_head_idle);
            aHeadReanim->ApplyRate(aBodyReanim->mAnimRate);
            aHeadReanim->SynchronizePhase(*aBodyReanim);
        }

        if (aHeadReanim2->mLoopCount > 0)
        {
            aHeadReanim2->StartBlend(20);
            aHeadReanim2->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
            aHeadReanim2->SetFramesForLayer(ReanimTrackId::anim_splitpea_idle);
            aHeadReanim2->ApplyRate(aBodyReanim->mAnimRate);
            aHeadReanim2->SynchronizePhase(*aBodyReanim);
        }

        if (aHeadReanim->mLoopType != ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD &&
            aHeadReanim2->mLoopType != ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD)
            mShootingEndTimeNs = 0;
        return;
    }
    else if (mState == PlantState::STATE_CACTUS_HIGH)
    {
        if (aBodyReanim->mLoopCount > 0)
        {
            PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_idlehigh, ReanimLoopType::REANIM_LOOP, 20, 0.0f);

            aBodyReanim->ApplyRate(aBodyReanim->mDefinition->mFPS);
            if (mApp->IsIZombieLevel())
            {
                aBodyReanim->ApplyRate(0.0f);
            }

            mShootingEndTimeNs = 0;
            return;
        }
    }
    else if (aHeadReanim)
    {
        if (aHeadReanim->mLoopCount > 0)
        {
            aHeadReanim->StartBlend(20);
            aHeadReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
            aHeadReanim->SetFramesForLayer(ReanimTrackId::anim_head_idle);
            aHeadReanim->ApplyRate(aBodyReanim->mAnimRate);
            aHeadReanim->SynchronizePhase(*aBodyReanim);
            mShootingEndTimeNs = 0;
            return;
        }
    }
    else if (mSeedType == SeedType::SEED_COBCANNON)
    {
        if (aBodyReanim->mLoopCount > 0)
        {
            mState = PlantState::STATE_COBCANNON_ARMING;
            mStateCountdown = 3000;
            PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_unarmed_idle, ReanimLoopType::REANIM_LOOP, 20, aBodyReanim->mDefinition->mFPS);
            mShootingEndTimeNs = 0;
            return;
        }
    }
    else if (aBodyReanim && aBodyReanim->mLoopCount > 0)
    {
        PlayIdleAnim(ownerNowNs, aBodyReanim->mDefinition->mFPS);
        mShootingEndTimeNs = 0;
        return;
    }

    // Still waiting: retain the expired deadline without extending it.
}

//0x464DB0
void Plant::Animate(std::int64_t ownerNowNs, std::int64_t animationNowNs)
{
    if ((mSeedType == SeedType::SEED_CHERRYBOMB || mSeedType == SeedType::SEED_JALAPENO) && mApp->mGameMode != GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
    {
        mShakeOffsetX = RandRangeFloat(-1.0f, 1.0f);
        mShakeOffsetY = RandRangeFloat(-1.0f, 1.0f);
    }


    if (mSquished)
    {
        mFrame = 0;
        return;
    }

    if (mSeedType == SeedType::SEED_WALLNUT || mSeedType == SeedType::SEED_TALLNUT)
    {
        AnimateNuts(ownerNowNs);
    }
    else if (mSeedType == SeedType::SEED_GARLIC)
    {
        AnimateGarlic();
    }
    else if (mSeedType == SeedType::SEED_PUMPKINSHELL)
    {
        AnimatePumpkin();
    }

    UpdateBlink(ownerNowNs, animationNowNs);

    if (mAnimPing)
    {
        if (mAnimCounter < mFrameLength * mNumFrames - 1)
        {
            mAnimCounter++;
        }
        else
        {
            mAnimPing = false;
            mAnimCounter -= mFrameLength;
        }
    }
    else if (mAnimCounter > 0)
    {
        mAnimCounter--;
    }
    else
    {
        mAnimPing = true;
        mAnimCounter += mFrameLength;
    }
    mFrame = mAnimCounter / mFrameLength;
}

//0x464EF0
float PlantFlowerPotHeightOffset(SeedType theSeedType, float theFlowerPotScale)
{
    // 盆栽高度/缩放修正：按 SeedType 槽位直查（行序 = 枚举序 0..52），零分支。
    static constexpr struct { float mHeightAdjust; float mScaleFix; } kFlowerPotAdjust[] = {
        { 0, -8 },   // SEED_PEASHOOTER
        { 0, -8 },   // SEED_SUNFLOWER
        { 0, 0 },    // SEED_CHERRYBOMB
        { 0, 0 },    // SEED_WALLNUT
        { 0, -4 },   // SEED_POTATOMINE
        { 0, -8 },   // SEED_SNOWPEA
        { -5, 0 },   // SEED_CHOMPER
        { 0, -8 },   // SEED_REPEATER
        { 0, -4 },   // SEED_PUFFSHROOM
        { 0, -4 },   // SEED_SUNSHROOM
        { 0, 0 },    // SEED_FUMESHROOM
        { 0, 0 },    // SEED_GRAVEBUSTER
        { 0, -8 },   // SEED_HYPNOSHROOM
        { 5, -8 },   // SEED_SCAREDYSHROOM
        { 0, 0 },    // SEED_ICESHROOM
        { 0, 0 },    // SEED_DOOMSHROOM
        { 0, -16 },  // SEED_LILYPAD
        { 0, 0 },    // SEED_SQUASH
        { 0, -8 },   // SEED_THREEPEATER
        { 0, -8 },   // SEED_TANGLEKELP
        { 0, 0 },    // SEED_JALAPENO
        { 0, -8 },   // SEED_SPIKEWEED
        { 0, 0 },    // SEED_TORCHWOOD
        { 0, 0 },    // SEED_TALLNUT
        { 0, -4 },   // SEED_SEASHROOM
        { -5, 0 },   // SEED_PLANTERN
        { 0, 0 },    // SEED_CACTUS
        { 0, -8 },   // SEED_BLOVER
        { 0, 0 },    // SEED_SPLITPEA
        { 0, 0 },    // SEED_STARFRUIT
        { 0, 0 },    // SEED_PUMPKINSHELL
        { 0, -8 },   // SEED_MAGNETSHROOM
        { 0, -8 },   // SEED_CABBAGEPULT
        { 0, 0 },    // SEED_FLOWERPOT
        { 0, 0 },    // SEED_KERNELPULT
        { 0, -20 },  // SEED_INSTANT_COFFEE
        { 0, 0 },    // SEED_GARLIC
        { 0, 0 },    // SEED_UMBRELLA
        { 0, -8 },   // SEED_MARIGOLD
        { 0, -8 },   // SEED_MELONPULT
        { 0, 0 },    // SEED_GATLINGPEA
        { 0, 0 },    // SEED_TWINSUNFLOWER
        { 0, 0 },    // SEED_GLOOMSHROOM
        { 0, 0 },    // SEED_CATTAIL
        { 0, 0 },    // SEED_WINTERMELON
        { 0, 0 },    // SEED_GOLD_MAGNET
        { 0, 0 },    // SEED_SPIKEROCK
        { 0, 0 },    // SEED_COBCANNON
        { 0, 0 },    // SEED_IMITATER
        { 0, 0 },    // SEED_EXPLODE_O_NUT
        { 0, 0 },    // SEED_GIANT_WALLNUT
        { 0, 0 },    // SEED_SPROUT
        { 0, -8 },   // SEED_LEFTPEATER
    };
    static_assert(sizeof(kFlowerPotAdjust) / sizeof(kFlowerPotAdjust[0]) == (int)SeedType::NUM_SEED_TYPES,
        "槽位数须与 SeedType 枚举同步");
    float aHeightAdjust = 0.0f;
    float aScaleOffsetFix = 0.0f;
    if ((unsigned)theSeedType < sizeof(kFlowerPotAdjust) / sizeof(kFlowerPotAdjust[0]))
    {
        aHeightAdjust = kFlowerPotAdjust[theSeedType].mHeightAdjust;
        aScaleOffsetFix = kFlowerPotAdjust[theSeedType].mScaleFix;
    }

    float aHeightOffset = -5.0f * theFlowerPotScale + aHeightAdjust;
    return aHeightOffset + (theFlowerPotScale * aScaleOffsetFix - aScaleOffsetFix);
}

//0x465040
// GOTY @Patoke: 0x468A90
float PlantDrawHeightOffset(std::int64_t ownerNowNs, Board* theBoard, Plant* thePlant, SeedType theSeedType, int theCol, int theRow)
{
    float aHeightOffset = 0.0f;

    bool doFloating = false;
    if (Plant::IsFlying(theSeedType))
    {
        doFloating = false;
    }
    else if (theBoard == nullptr)
    {
        if (Plant::IsAquatic(theSeedType))
        {
            doFloating = true;
        }
    }
    else if (theBoard->IsPoolSquare(theCol, theRow))
    {
        doFloating = true;
    }
    else if (thePlant != nullptr &&
             theBoard->mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM &&
             theCol < 5)
    {
        doFloating = true;
    }

    if (doFloating)
    {
		const std::uint64_t aTimeNs = ownerNowNs;
		constexpr std::uint64_t FLOATING_PLANT_PERIOD_NS = 2'000'000'000;
        float aPos = theRow * PI + theCol * 0.25f * PI;
		float aTime = static_cast<float>(double(aTimeNs % FLOATING_PLANT_PERIOD_NS) /
			FLOATING_PLANT_PERIOD_NS) * 2.0f * PI;
        float aFloatingHeight = sin(aPos + aTime) * 2.0f;
        aHeightOffset += aFloatingHeight;
    }

    if (theBoard && (thePlant == nullptr || !thePlant->mSquished))
    {
        Plant* aPot = theBoard->GetFlowerPotAt(theCol, theRow);
        if (aPot && !aPot->mSquished && theSeedType != SeedType::SEED_FLOWERPOT)
        {
            aHeightOffset += PlantFlowerPotHeightOffset(theSeedType, 1.0f);
        }
    }

    // B 形态 -> 常量表 kPlantHeightOffsets（原 10 支常量链合并；行序 =
    // SeedType 枚举序 0..52，0 = 无调整）。CACTUS 原早退 = 不调整（表后
    // 仅剩 return，等价）；SPIKEWEED/SPIKEROCK 嵌套地形双支特例表外。
    // 原链另有一支被注释的 IsFlying -= 30（历史遗留，随链退役）。
    static constexpr float kPlantHeightOffsets[] = {
        0,     // 0 SEED_PEASHOOTER
        0,     // 1 SEED_SUNFLOWER
        0,     // 2 SEED_CHERRYBOMB
        0,     // 3 SEED_WALLNUT
        0,     // 4 SEED_POTATOMINE
        0,     // 5 SEED_SNOWPEA
        0,     // 6 SEED_CHOMPER
        0,     // 7 SEED_REPEATER
        5,     // 8 SEED_PUFFSHROOM
        0,     // 9 SEED_SUNSHROOM
        0,     // 10 SEED_FUMESHROOM
        -40,   // 11 SEED_GRAVEBUSTER
        0,     // 12 SEED_HYPNOSHROOM
        -14,   // 13 SEED_SCAREDYSHROOM
        0,     // 14 SEED_ICESHROOM
        0,     // 15 SEED_DOOMSHROOM
        25,    // 16 SEED_LILYPAD
        0,     // 17 SEED_SQUASH
        0,     // 18 SEED_THREEPEATER
        24,    // 19 SEED_TANGLEKELP
        0,     // 20 SEED_JALAPENO
        0,     // 21 SEED_SPIKEWEED（特例表外，槽位不可达）
        0,     // 22 SEED_TORCHWOOD
        0,     // 23 SEED_TALLNUT
        28,    // 24 SEED_SEASHROOM
        0,     // 25 SEED_PLANTERN
        0,     // 26 SEED_CACTUS（原早退 = 不调整）
        0,     // 27 SEED_BLOVER
        0,     // 28 SEED_SPLITPEA
        10,    // 29 SEED_STARFRUIT
        15,    // 30 SEED_PUMPKINSHELL
        0,     // 31 SEED_MAGNETSHROOM
        0,     // 32 SEED_CABBAGEPULT
        26,    // 33 SEED_FLOWERPOT
        0,     // 34 SEED_KERNELPULT
        -20,   // 35 SEED_INSTANT_COFFEE
        0,     // 36 SEED_GARLIC
        0,     // 37 SEED_UMBRELLA
        0,     // 38 SEED_MARIGOLD
        0,     // 39 SEED_MELONPULT
        0,     // 40 SEED_GATLINGPEA
        0,     // 41 SEED_TWINSUNFLOWER
        0,     // 42 SEED_GLOOMSHROOM
        0,     // 43 SEED_CATTAIL
        0,     // 44 SEED_WINTERMELON
        0,     // 45 SEED_GOLD_MAGNET
        0,     // 46 SEED_SPIKEROCK（特例表外，槽位不可达）
        0,     // 47 SEED_COBCANNON
        0,     // 48 SEED_IMITATER
        0,     // 49 SEED_EXPLODE_O_NUT
        0,     // 50 SEED_GIANT_WALLNUT
        0,     // 51 SEED_SPROUT
        0,     // 52 SEED_LEFTPEATER
    };
    static_assert(sizeof(kPlantHeightOffsets) / sizeof(kPlantHeightOffsets[0]) == (int)SeedType::NUM_SEED_TYPES,
        "kPlantHeightOffsets 槽位须与 SeedType 枚举同步");

    if (theSeedType == SeedType::SEED_SPIKEWEED || theSeedType == SeedType::SEED_SPIKEROCK)
    {
        int aBottomRow = 4;
        if (theBoard && theBoard->StageHas6Rows())
        {
            aBottomRow = 5;
        }

        if (theSeedType == SeedType::SEED_SPIKEROCK)
        {
            aHeightOffset += 6.0f;
        }

        if (theBoard && theBoard->GetFlowerPotAt(theCol, theRow) && gLawnApp->mGameMode != GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
        {
            aHeightOffset += 5.0f;
        }
        else if (theBoard && theBoard->StageHasRoof())
        {
            aHeightOffset += 15.0f;
        }
        else if (theBoard && theBoard->IsPoolSquare(theCol, theRow))
        {
            aHeightOffset += 0.0f;
        }
        else if (theRow == aBottomRow)
        {
            aHeightOffset += 12.0f;
        }
        else
        {
            aHeightOffset += 15.0f;
        }
    }
    else if ((unsigned)theSeedType < sizeof(kPlantHeightOffsets) / sizeof(kPlantHeightOffsets[0]))
    {
        aHeightOffset += kPlantHeightOffsets[(int)theSeedType];
    }

    return aHeightOffset;
}

//0x465380
void Plant::GetPeaHeadOffset(std::int64_t ownerNowNs, int& theOffsetX, int& theOffsetY)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);


    int aTrackIndex = 0;
    if (aBodyReanim->TrackExists(ReanimTrackId::anim_stem))
    {
        aTrackIndex = aBodyReanim->FindTrackIndex(ReanimTrackId::anim_stem);
    }
    else if(aBodyReanim->TrackExists(ReanimTrackId::anim_idle))
    {
        aTrackIndex = aBodyReanim->FindTrackIndex(ReanimTrackId::anim_idle);
    }

    ReanimatorTransform aTransform;
    aBodyReanim->GetCurrentTransform(aTrackIndex, &aTransform);
    theOffsetX = aTransform.mTransX;
    theOffsetY = aTransform.mTransY;
}

//0x465460
// GOTY @Patoke: 0x468EB0
// The gameplay enum selects an authored atlas sprite; rectangles and scales come
// from Magnet.atlas.dsl. The old Image* table/if-chain measurements do not measure
// this atlas path, so this change makes no CPU timing claim.
void Plant::DrawMagnetItems(std::int64_t ownerNowNs, Graphics* g)
{
    const TextureSource aTexture{MagnetAtlas::Texture, 0};
    const auto ageNs = ownerNowNs - mMagnetStartTimeNs;
    const float sizeFactor = MagnetMotion::SizeAt(ageNs);
    if (sizeFactor <= 0.0f) return;
    float aOffsetX = 0.0f;
    float aOffsetY = PlantDrawHeightOffset(ownerNowNs, mBoard, this, mSeedType, mPlantCol, mRow);

    for (int i = 0; i < MAX_MAGNET_ITEMS; i++)
    {
        MagnetItem* aMagnetItem = &mMagnetItems[i];
        if (aMagnetItem->mItemType != MagnetItemType::MAGNET_ITEM_NONE)
        {
            const int aType=static_cast<int>(aMagnetItem->mItemType);
            TOD_ASSERT(aType > 0 && aType < static_cast<int>(std::size(kMagnetSprites)));
            const auto aSprite=static_cast<unsigned>(kMagnetSprites[aType]);
            const TextureRegion& aSrcRect=MagnetAtlas::Images[aSprite];
            const float baseScale=MagnetAtlas::Scales[aSprite];
            const float aScale=baseScale * sizeFactor;
            // Preserve the original sprite center while its geometry shrinks.
            const float drawX=aMagnetItem->mPosX-mX+aOffsetX +
                aSrcRect.mWidth * baseScale * (1.0f-sizeFactor) * 0.5f;
            const float drawY=aMagnetItem->mPosY-mY+aOffsetY +
                aSrcRect.mHeight * baseScale * (1.0f-sizeFactor) * 0.5f;
            if (aScale == 1.0f)
            {
                DrawAtlasImage(g,aTexture,aSrcRect,drawX,drawY);
            }
            else
            {
                DrawAtlasImageScaled(g,aTexture,aSrcRect,drawX,drawY,aScale,aScale);
            }
        }
    }
}

//0x465680
void Plant::DrawShadow(std::int64_t ownerNowNs, Sexy::Graphics* g, float theOffsetX, float theOffsetY)
{
    if (mSeedType == SeedType::SEED_LILYPAD || mSeedType == SeedType::SEED_STARFRUIT || mSeedType == SeedType::SEED_TANGLEKELP ||
        mSeedType == SeedType::SEED_SEASHROOM || mSeedType == SeedType::SEED_COBCANNON || mSeedType == SeedType::SEED_SPIKEWEED ||
        mSeedType == SeedType::SEED_SPIKEROCK || mSeedType == SeedType::SEED_GRAVEBUSTER || mSeedType == SeedType::SEED_CATTAIL ||
        mOnBungeeState == PlantOnBungeeState::RISING_WITH_BUNGEE)
        return;

    if (mIsOnBoard && mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN && mApp->mZenGarden->mGardenType == GardenType::GARDEN_MAIN)
        return;

    int aShadowType = 0;
    float aShadowOffsetX = -3.0f;
    float aShadowOffsetY = 51.0f;
    float aScale = 1.0f;
    if (mBoard && mBoard->StageIsNight())
    {
        aShadowType = 1;
    }

    // B 形态 -> 常量表 kPlantShadow（原 14 支常量链合并；行序 = SeedType
    // 枚举序 0..52，{1,-3,51} = 原 default 初值即无行；SQUASH/SUNSHROOM
    // 动态缩放两支特例表外保留，其槽位不可达）。
    static constexpr struct { float mScale; float mOffX; float mOffY; } kPlantShadow[] = {
        { 1.0f,  -3.0f, 51.0f },  // 0 SEED_PEASHOOTER（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 1 SEED_SUNFLOWER（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 2 SEED_CHERRYBOMB（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 3 SEED_WALLNUT（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 4 SEED_POTATOMINE（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 5 SEED_SNOWPEA（原 default）
        { 1.0f, -21.0f, 57.0f },  // 6 SEED_CHOMPER
        { 1.0f,  -3.0f, 51.0f },  // 7 SEED_REPEATER（原 default）
        { 0.5f,  -3.0f, 42.0f },  // 8 SEED_PUFFSHROOM
        { 1.0f,  -3.0f, 51.0f },  // 9 SEED_SUNSHROOM（特例表外，槽位不可达）
        { 1.3f,  -3.0f, 47.0f },  // 10 SEED_FUMESHROOM
        { 1.0f,  -3.0f, 51.0f },  // 11 SEED_GRAVEBUSTER（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 12 SEED_HYPNOSHROOM（原 default）
        { 1.0f,  -9.0f, 55.0f },  // 13 SEED_SCAREDYSHROOM
        { 1.0f,  -3.0f, 51.0f },  // 14 SEED_ICESHROOM（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 15 SEED_DOOMSHROOM（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 16 SEED_LILYPAD（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 17 SEED_SQUASH（特例表外，槽位不可达）
        { 1.0f,  -3.0f, 51.0f },  // 18 SEED_THREEPEATER（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 19 SEED_TANGLEKELP（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 20 SEED_JALAPENO（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 21 SEED_SPIKEWEED（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 22 SEED_TORCHWOOD（原 default）
        { 1.3f,  -3.0f, 54.0f },  // 23 SEED_TALLNUT
        { 1.0f,  -3.0f, 51.0f },  // 24 SEED_SEASHROOM（原 default）
        { 1.0f,  -3.0f, 57.0f },  // 25 SEED_PLANTERN
        { 1.0f,  -8.0f, 50.0f },  // 26 SEED_CACTUS
        { 1.0f,  -3.0f, 51.0f },  // 27 SEED_BLOVER（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 28 SEED_SPLITPEA（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 29 SEED_STARFRUIT（原 default）
        { 1.4f,  -3.0f, 46.0f },  // 30 SEED_PUMPKINSHELL
        { 1.0f,  -3.0f, 51.0f },  // 31 SEED_MAGNETSHROOM（原 default）
        { 1.0f,  -3.0f, 47.0f },  // 32 SEED_CABBAGEPULT
        { 1.0f,  -4.0f, 46.0f },  // 33 SEED_FLOWERPOT
        { 1.0f,   0.0f, 47.0f },  // 34 SEED_KERNELPULT
        { 1.0f,  -3.0f, 71.0f },  // 35 SEED_INSTANT_COFFEE
        { 1.0f,  -3.0f, 51.0f },  // 36 SEED_GARLIC（原 default）
        { 0.5f,  -7.0f, 52.0f },  // 37 SEED_UMBRELLA
        { 1.0f,  -3.0f, 51.0f },  // 38 SEED_MARIGOLD（原 default）
        { 1.0f,  -3.0f, 47.0f },  // 39 SEED_MELONPULT（共体）
        { 1.0f,  -3.0f, 51.0f },  // 40 SEED_GATLINGPEA（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 41 SEED_TWINSUNFLOWER（原 default）
        { 1.3f,  -3.0f, 47.0f },  // 42 SEED_GLOOMSHROOM（共体）
        { 1.0f,  -3.0f, 51.0f },  // 43 SEED_CATTAIL（原 default）
        { 1.0f,  -3.0f, 47.0f },  // 44 SEED_WINTERMELON（共体）
        { 1.0f,  -3.0f, 51.0f },  // 45 SEED_GOLD_MAGNET（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 46 SEED_SPIKEROCK（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 47 SEED_COBCANNON（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 48 SEED_IMITATER（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 49 SEED_EXPLODE_O_NUT（原 default）
        { 1.7f, -33.0f, 56.0f },  // 50 SEED_GIANT_WALLNUT
        { 1.0f,  -3.0f, 51.0f },  // 51 SEED_SPROUT（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 52 SEED_LEFTPEATER（原 default）
    };
    static_assert(sizeof(kPlantShadow) / sizeof(kPlantShadow[0]) == (int)SeedType::NUM_SEED_TYPES,
        "kPlantShadow 槽位须与 SeedType 枚举同步");

    if (mSeedType == SeedType::SEED_SQUASH)
    {
        if (mBoard)
        {
            aShadowOffsetY += mBoard->GridToPixelY(mPlantCol, mRow) - (mY);
        }
        aShadowOffsetY += 5.0f;
    }
    else if (mSeedType == SeedType::SEED_SUNSHROOM)
    {
        aShadowOffsetY = 42.0f;
        if (mState == PlantState::STATE_SUNSHROOM_SMALL)
        {
            aScale = 0.5f;
        }
        else if (mState == PlantState::STATE_SUNSHROOM_GROWING)
        {
            Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

            aScale = 0.5f + 0.5f * (static_cast<double>(aBodyReanim->SamplePlayback().phaseNs) / aBodyReanim->mPeriodNs);
        }
    }
    else if ((unsigned)mSeedType < sizeof(kPlantShadow) / sizeof(kPlantShadow[0]))
    {
        const auto& aRow = kPlantShadow[(int)mSeedType];
        aScale = aRow.mScale;
        aShadowOffsetX = aRow.mOffX;
        aShadowOffsetY = aRow.mOffY;
    }

    if (Plant::IsFlying(mSeedType))
    {
        aShadowOffsetY += 10.0f;
        if (mBoard && (mBoard->GetTopPlantAt(mPlantCol, mRow, TOPPLANT_ONLY_NORMAL_POSITION) || mBoard->GetTopPlantAt(mPlantCol, mRow, TOPPLANT_ONLY_PUMPKIN)))
            return;
    }

    if (aShadowType == 0)
    {
        TodDrawImageRectCenterScaledF(g, IMAGE_PLANTSHADOW, theOffsetX + aShadowOffsetX, theOffsetY + aShadowOffsetY,
            Rect(0, 0, IMAGE_PLANTSHADOW->mWidth, IMAGE_PLANTSHADOW->mHeight), aScale, aScale);
    }
    else
    {
        TodDrawImageRectCenterScaledF(g, IMAGE_PLANTSHADOW2, theOffsetX + aShadowOffsetX, theOffsetY + aShadowOffsetY,
            Rect(0, 0, IMAGE_PLANTSHADOW2->mWidth, IMAGE_PLANTSHADOW2->mHeight), aScale, aScale);
    }
}

//0x465A90
// GOTY @Patoke: 0x469500
void Plant::Draw(std::int64_t ownerNowNs, Graphics* g)
{
    float aOffsetX = 0.0f;
    float aOffsetY = PlantDrawHeightOffset(ownerNowNs, mBoard, this, mSeedType, mPlantCol, mRow);
    if (Plant::IsFlying(mSeedType) && mSquished)
    {
        aOffsetY += 30.0f;
    }


    if (mSquished)
    {
        if (mSeedType == SeedType::SEED_FLOWERPOT)
        {
            aOffsetY -= 15.0f;
        }
        if (mSeedType == SeedType::SEED_INSTANT_COFFEE)
        {
            aOffsetY -= 20.0f;
        }

        g->SetScale(1.0f, 0.25f, 0.0f, 0.0f);
        DrawSeedType(g, mSeedType, mImitaterType, DrawVariation::VARIATION_NORMAL, aOffsetX, 60.0f + aOffsetY);
        g->SetScale(1.0f, 1.0f, 0.0f, 0.0f);
    }
    else
    {
        bool aDrawPumpkinBack = false;
        Plant* aPumpkin = nullptr;

        if (mIsOnBoard)
        {
            aPumpkin = mBoard->GetPumpkinAt(mPlantCol, mRow);
            if (aPumpkin)
            {
                Plant* aPlantInPumpkin = mBoard->GetTopPlantAt(mPlantCol, mRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
                if (aPlantInPumpkin)
                {
                    if (aPlantInPumpkin->mRenderOrder > aPumpkin->mRenderOrder || aPlantInPumpkin->mOnBungeeState == GETTING_GRABBED_BY_BUNGEE)
                    {
                        aPlantInPumpkin = nullptr;
                    }
                }

                if (aPlantInPumpkin == this)
                {
                    aDrawPumpkinBack = true;
                }
                if (aPlantInPumpkin == nullptr && mSeedType == SeedType::SEED_PUMPKINSHELL)
                {
                    aDrawPumpkinBack = true;
                }
            }
            else if (mSeedType == SeedType::SEED_PUMPKINSHELL)
            {
                aDrawPumpkinBack = true;
                aPumpkin = this;
            }
        }
        else if (mSeedType == SeedType::SEED_PUMPKINSHELL)
        {
            aDrawPumpkinBack = true;
            aPumpkin = this;
        }

        DrawShadow(ownerNowNs, g, aOffsetX, aOffsetY);

        if (Plant::IsFlying(mSeedType))
        {
			const std::uint64_t aTimeNs = ownerNowNs;
            // Reduce in double before converting the bounded drawing phase to float.
            const double aPhase = fmod(
                (mRow * 97.0 + mPlantCol * 61.0) * 0.03 +
				static_cast<double>(aTimeNs) * 3e-9, 2.0 * PI);
            float aWave = sin(static_cast<float>(aPhase)) * 2.0f;
            aOffsetY += aWave;
        }

        if (aDrawPumpkinBack)
        {
            Reanimation* aPumpkinReanim = mApp->ReanimationGet(aPumpkin->mBodyReanimID);

            Graphics aPumpkinGraphics(*g);
            aPumpkinGraphics.mTransX += (aPumpkin->mX) - (mX);
            aPumpkinGraphics.mTransY += (aPumpkin->mY) - (mY);
            aPumpkinReanim->DrawRenderGroup(&aPumpkinGraphics, 1);
        }

        aOffsetX += mShakeOffsetX;
        aOffsetY += mShakeOffsetY;
        if (IsInPlay() && mApp->IsIZombieLevel())
        {
            mBoard->mChallenge->IZombieDrawPlant(g, this);
        }
        else if (mBodyReanimID != ReanimationID::REANIMATIONID_NULL)
        {
            Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

            if (aBodyReanim)
            {
                // GOTY 1073 Plant::Draw (0x00469500) draws only render
                // group 0 here. Group 1 is Pumpkin_back and was already
                // drawn behind the contained plant above.
                aBodyReanim->DrawRenderGroup(g, RENDER_GROUP_NORMAL);
            }
        }

        if (mSeedType == SeedType::SEED_MAGNETSHROOM && !DrawMagnetItemsOnTop())
        {
            DrawMagnetItems(ownerNowNs, g);
        }
    }
}

//0x4660B0
// GOTY @Patoke: 0x469AF0
void Plant::DrawSeedType(Graphics* g, SeedType theSeedType, SeedType theImitaterType, DrawVariation theDrawVariation, float thePosX, float thePosY)
{
    Graphics aSeedG(*g);
    int aCelRow = 0;
    int aCelCol = 2;
    float aOffsetX = 0.0f;
    float aOffsetY = 0.0f;
    SeedType aSeedType = theSeedType;
    DrawVariation aDrawVariation = theDrawVariation;

    if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
    {
        aSeedType = theImitaterType;
        aDrawVariation = DrawVariation::VARIATION_IMITATER;
        if (theImitaterType == SeedType::SEED_HYPNOSHROOM || theImitaterType == SeedType::SEED_SQUASH || theImitaterType == SeedType::SEED_POTATOMINE ||
            theImitaterType == SeedType::SEED_GARLIC || theImitaterType == SeedType::SEED_LILYPAD)
            aDrawVariation = DrawVariation::VARIATION_IMITATER_LESS;
    }
    else if (theDrawVariation == DrawVariation::VARIATION_NORMAL && theSeedType == SeedType::SEED_TANGLEKELP)
    {
        aDrawVariation = DrawVariation::VARIATION_AQUARIUM;
    }

    if (((LawnApp*)gSexyAppBase)->mGameMode == GameMode::GAMEMODE_LIMBO_BIG_TIME &&
        (aSeedType == SeedType::SEED_WALLNUT || aSeedType == SeedType::SEED_SUNFLOWER || aSeedType == SeedType::SEED_MARIGOLD))
    {
        aSeedG.mScaleX *= 1.5f;
        aSeedG.mScaleY *= 1.5f;
        aOffsetX = -20.0f;
        aOffsetY = -40.0f;
    }
    if (aSeedType == SeedType::SEED_LEFTPEATER)
    {
        aOffsetX += aSeedG.mScaleX * 80.0f;
        aSeedG.mScaleX *= -1.0f;
    }

    if (Challenge::IsZombieSeedType(aSeedType))
    {
        ZombieType aZombieType = Challenge::IZombieSeedTypeToZombieType(aSeedType);
        if (aZombieType == ZombieType::ZOMBIE_DANCER)
        {
            aSeedG.mScaleX *= 0.8f;
            aSeedG.mScaleY *= 0.8f;
            aOffsetX = 20.0f;
            aOffsetY = 40.0f;
        }
        gLawnApp->mReanimatorCache->DrawCachedZombie(&aSeedG, thePosX + aOffsetX, thePosY + aOffsetY, aZombieType);
    }
    else
    {
        const PlantDefinition& aPlantDef = GetPlantDefinition(aSeedType);

        if (aSeedType == SeedType::SEED_GIANT_WALLNUT)
        {
            aSeedG.mScaleX *= 1.4f;
            aSeedG.mScaleY *= 1.4f;
            TodDrawImageScaledF(&aSeedG, IMAGE_REANIM_WALLNUT_BODY, thePosX - 53.0f, thePosY - 56.0f, aSeedG.mScaleX, aSeedG.mScaleY);
        }
        else if (aPlantDef.mReanimationType != ReanimationType::REANIM_NONE)
        {
            gLawnApp->mReanimatorCache->DrawCachedPlant(&aSeedG, thePosX + aOffsetX, thePosY + aOffsetY, aSeedType, aDrawVariation);
        }
    }
}

//0x466390
void Plant::MouseDown(int x, int y, int theClickCount)
{
    if (theClickCount < 0)
        return;

    if (mState == PlantState::STATE_COBCANNON_READY)
    {
        mBoard->ClearCursor();
        mBoard->mCursorObject->mType = SeedType::SEED_NONE;
        mBoard->mCursorObject->mCursorType = CursorType::CURSOR_TYPE_COBCANNON_TARGET;
        mBoard->mCursorObject->mSeedBankIndex = -1;
        mBoard->mCursorObject->mCoinID = CoinID::COINID_NULL;
        mBoard->mCursorObject->mCobCannonPlantID = (PlantID)mBoard->mPlants.DataArrayGetID(this);
        mBoard->mCobCannonCursorDelayCounter = 30;
        mBoard->mCobCannonMouseX = x;
        mBoard->mCobCannonMouseY = y;
    }
}

//0x466420
void Plant::IceZombies(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
	Zombie* aZombie = nullptr;
    while (mBoard->IterateZombies(aZombie))
    {
        aZombie->HitIceTrap(ownerNowNs, visualNowNs, animationNowNs);
    }

    mBoard->mIceTrapCounter = 300;
    TodParticleSystem* aPoolSparklyParticle = mApp->ParticleTryToGet(mBoard->mPoolSparklyParticleID);
    if (aPoolSparklyParticle)
    {
        // GOTY 1073 0x469EFF: freeze pool sparkles while the ice timer runs.
        aPoolSparklyParticle->mDontUpdate = true;
    }

    Zombie* aBossZombie = mBoard->GetBossZombie();
    if (aBossZombie)
    {
        aBossZombie->BossDestroyFireball(ownerNowNs, visualNowNs, animationNowNs);
    }
}

//0x4664B0
void Plant::BurnRow(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theRow)
{
    int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);

    Zombie* aZombie = nullptr;
    // 行段路径（2026-10-04 全量转换）：Boss‖theRow 候选集，apply-all 与序无关
    while (mBoard->IterateZombiesInRow(theRow, aZombie))
    {
        if ((((ZombieFlagsOf(aZombie->mZombieType) & ZombieFlags::Boss) != 0) || aZombie->mRow == theRow) && aZombie->EffectedByDamage(aDamageRangeFlags))
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
        // 注：原版中将 Zombie::BossDestroyIceballInRow(int) 函数改为了 Zombie::BossDestroyIceball()，冰球是否位于目标行的判断则移动至此处进行
        aBossZombie->BossDestroyIceballInRow(ownerNowNs);
    }
}

//0x4665B0
void Plant::BlowAwayFliers()
{
    Zombie* aZombie = nullptr;
    while (mBoard->IterateZombies(aZombie))
    {
        if (!aZombie->IsDeadOrDying())
        {
            // GOTY 1.2.0.1073 only blows away balloons that are still flying.
            // PHASE_BALLOON_POPPING is considered flying by IsFlying() for
            // collision purposes, but the falling zombie must be allowed to land.
            if (aZombie->mZombiePhase == ZombiePhase::PHASE_BALLOON_FLYING)
            {
                aZombie->mBlowingAway = true;
            }
        }
    }

    mApp->PlaySample(SOUND_BLOVER);
    mBoard->mFogBlownCountDown = 4000;
}

//0x466650
void Plant::KillAllPlantsNearDoom(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    // 格桶链枚举（2026-10-03 第二批）：原为全池扫 mRow==mRow &&
    // mPlantCol==mPlantCol 同格全层全株 Die——改该格 4 层链全成员
    // （SquishAllInSquare 同款；含影子株=原池可见集，毁灭菇自身在链
    // =原池扫同帧已入池同样自灭）。原不过滤 NotOnGround，链枚举也不过
    // 滤（Die 无腾空守卫，垂死未 free 株原池扫本就可见，再 Die 幂等）。
    // 玉米炮锚列-1 右列链成员按桶语义命中（铁律3 已验证）。Die 不摘链
    // （ProcessDeleteQueue 摘），防御性前取。
    for (int aLayer = 0; aLayer < PLANT_CELL_COUNT; aLayer++)
    {
        Plant* aPlant = mBoard->GetCellPlant(mRow, mPlantCol, aLayer);
        while (aPlant != nullptr)
        {
            Plant* aNext = mBoard->NextCellPlant(aPlant);  // 先取：Die 不摘链但防御性前取
            aPlant->Die(ownerNowNs, visualNowNs, animationNowNs);
            aPlant = aNext;
        }
    }
}

namespace
{
// Plant::DoSpecial 分发表：表内匿名 lambda = 原 case 体逐句迁移（生成器 +
// 逆变换 token 对拍 0 差异）；default 空操作具名补缺槽。
struct PlantSpecialLocals
{
    int mPosX;
    int mPosY;
    int mDamageRangeFlags;
};
using PlantSpecialCaseFn = void (*)(std::int64_t, std::int64_t, std::int64_t, Plant&, PlantSpecialLocals&);

// 原 default（无 case 的 45 型（空操作）共用）
void PlantSpecialDefault(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantSpecialLocals& lc)
{

}

// 匿名 lambda 函数指针表，行序 = SeedType 枚举值序（0..52）。
constexpr PlantSpecialCaseFn kPlantSpecialCases[(int)SeedType::NUM_SEED_TYPES] = {
    &PlantSpecialDefault,  // 0 SEED_PEASHOOTER（原 default）
    &PlantSpecialDefault,  // 1 SEED_SUNFLOWER（原 default）
    // 原 case SEED_CHERRYBOMB
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantSpecialLocals& lc)
    {
            p.mApp->PlayFoley(FoleyType::FOLEY_CHERRYBOMB);
            p.mApp->PlayFoley(FoleyType::FOLEY_JUICY);

            if (p.mBoard->KillAllZombiesInRadius(p.mRow, lc.mPosX, lc.mPosY, 115, 1, true, lc.mDamageRangeFlags) >= 10)
                ReportAchievement::GiveAchievement(p.mApp, Explodonator, true); // @Patoke: add achievement

            p.mApp->AddTodParticle(lc.mPosX, lc.mPosY, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_POWIE, ownerNowNs);
            p.mBoard->mShakeCounter = 12;
            p.mBoard->mShakeAmountX = 3;
            p.mBoard->mShakeAmountY = -4;

            p.Die(ownerNowNs, visualNowNs, animationNowNs);
    },  // 2 SEED_CHERRYBOMB
    &PlantSpecialDefault,  // 3 SEED_WALLNUT（原 default）
    // 原 case SEED_POTATOMINE
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantSpecialLocals& lc)
    {
            lc.mPosX = (p.mX) + p.mWidth / 2 - 20;
            lc.mPosY = (p.mY) + p.mHeight / 2;

            p.mApp->PlaySample(SOUND_POTATO_MINE);
            if (p.mBoard->KillAllZombiesInRadius(p.mRow, lc.mPosX, lc.mPosY, 60, 0, false, lc.mDamageRangeFlags) >= 1)
                ReportAchievement::GiveAchievement(p.mApp, Spudow, true); // @Patoke: add achievement

            int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, p.mRow, 0);
            p.mApp->AddTodParticle(lc.mPosX + 20.0f, lc.mPosY, aRenderPosition, ParticleEffect::PARTICLE_POTATO_MINE, ownerNowNs);
            p.mBoard->mShakeCounter = 12;
            p.mBoard->mShakeAmountX = 3;
            p.mBoard->mShakeAmountY = -4;

            p.Die(ownerNowNs, visualNowNs, animationNowNs);
    },  // 4 SEED_POTATOMINE
    &PlantSpecialDefault,  // 5 SEED_SNOWPEA（原 default）
    &PlantSpecialDefault,  // 6 SEED_CHOMPER（原 default）
    &PlantSpecialDefault,  // 7 SEED_REPEATER（原 default）
    &PlantSpecialDefault,  // 8 SEED_PUFFSHROOM（原 default）
    &PlantSpecialDefault,  // 9 SEED_SUNSHROOM（原 default）
    &PlantSpecialDefault,  // 10 SEED_FUMESHROOM（原 default）
    &PlantSpecialDefault,  // 11 SEED_GRAVEBUSTER（原 default）
    &PlantSpecialDefault,  // 12 SEED_HYPNOSHROOM（原 default）
    &PlantSpecialDefault,  // 13 SEED_SCAREDYSHROOM（原 default）
    // 原 case SEED_ICESHROOM
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantSpecialLocals& lc)
    {
            p.mApp->PlayFoley(FoleyType::FOLEY_FROZEN);
            p.IceZombies(ownerNowNs, visualNowNs, animationNowNs);
            p.mApp->AddTodParticle(lc.mPosX, lc.mPosY, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_ICE_TRAP, ownerNowNs);

            p.Die(ownerNowNs, visualNowNs, animationNowNs);
    },  // 14 SEED_ICESHROOM
    // 原 case SEED_DOOMSHROOM
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantSpecialLocals& lc)
    {
            p.mApp->PlaySample(SOUND_DOOMSHROOM);

            p.mBoard->KillAllZombiesInRadius(p.mRow, lc.mPosX, lc.mPosY, 250, 3, true, lc.mDamageRangeFlags);
            p.KillAllPlantsNearDoom(ownerNowNs, visualNowNs, animationNowNs);

            p.mApp->AddTodParticle(lc.mPosX, lc.mPosY, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_DOOM, ownerNowNs);
            p.mBoard->AddACrater(p.mPlantCol, p.mRow)->mStateEndTimeNs = ownerNowNs + 180'000'000'000LL;
            p.mBoard->mShakeCounter = 12;
            p.mBoard->mShakeAmountX = 3;
            p.mBoard->mShakeAmountY = -4;

            p.Die(ownerNowNs, visualNowNs, animationNowNs);
    },  // 15 SEED_DOOMSHROOM
    &PlantSpecialDefault,  // 16 SEED_LILYPAD（原 default）
    &PlantSpecialDefault,  // 17 SEED_SQUASH（原 default）
    &PlantSpecialDefault,  // 18 SEED_THREEPEATER（原 default）
    &PlantSpecialDefault,  // 19 SEED_TANGLEKELP（原 default）
    // 原 case SEED_JALAPENO
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantSpecialLocals& lc)
    {
            p.mApp->PlayFoley(FoleyType::FOLEY_JALAPENO_IGNITE);
            p.mApp->PlayFoley(FoleyType::FOLEY_JUICY);

            p.mBoard->DoFwoosh(p.mRow);
            p.mBoard->mShakeCounter = 12;
            p.mBoard->mShakeAmountX = 3;
            p.mBoard->mShakeAmountY = -4;

            p.BurnRow(ownerNowNs, visualNowNs, animationNowNs, p.mRow);
            p.mBoard->mIceTimer[p.mRow] = 20;

            p.Die(ownerNowNs, visualNowNs, animationNowNs);
    },  // 20 SEED_JALAPENO
    &PlantSpecialDefault,  // 21 SEED_SPIKEWEED（原 default）
    &PlantSpecialDefault,  // 22 SEED_TORCHWOOD（原 default）
    &PlantSpecialDefault,  // 23 SEED_TALLNUT（原 default）
    &PlantSpecialDefault,  // 24 SEED_SEASHROOM（原 default）
    &PlantSpecialDefault,  // 25 SEED_PLANTERN（原 default）
    &PlantSpecialDefault,  // 26 SEED_CACTUS（原 default）
    // 原 case SEED_BLOVER
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantSpecialLocals& lc)
    {
            if (p.mState != PlantState::STATE_DOINGSPECIAL)
            {
                p.mState = PlantState::STATE_DOINGSPECIAL;
                p.BlowAwayFliers();
            }
    },  // 27 SEED_BLOVER
    &PlantSpecialDefault,  // 28 SEED_SPLITPEA（原 default）
    &PlantSpecialDefault,  // 29 SEED_STARFRUIT（原 default）
    &PlantSpecialDefault,  // 30 SEED_PUMPKINSHELL（原 default）
    &PlantSpecialDefault,  // 31 SEED_MAGNETSHROOM（原 default）
    &PlantSpecialDefault,  // 32 SEED_CABBAGEPULT（原 default）
    &PlantSpecialDefault,  // 33 SEED_FLOWERPOT（原 default）
    &PlantSpecialDefault,  // 34 SEED_KERNELPULT（原 default）
    // 原 case SEED_INSTANT_COFFEE
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantSpecialLocals& lc)
    {
            Plant* aPlant = p.mBoard->GetTopPlantAt(p.mPlantCol, p.mRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
            if (aPlant && aPlant->mIsAsleep)
            {
                aPlant->mWakeUpEndTimeNs = ownerNowNs + 1'000'000'000;
            }

            p.mState = PlantState::STATE_DOINGSPECIAL;
            p.PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_crumble, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 22.0f);
            p.mApp->PlayFoley(FoleyType::FOLEY_COFFEE);
    },  // 35 SEED_INSTANT_COFFEE
    &PlantSpecialDefault,  // 36 SEED_GARLIC（原 default）
    // 原 case SEED_UMBRELLA
    [](std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant& p, PlantSpecialLocals& lc)
    {
            if (p.mState != PlantState::STATE_UMBRELLA_TRIGGERED && p.mState != PlantState::STATE_UMBRELLA_REFLECTING)
            {
                p.mState = PlantState::STATE_UMBRELLA_TRIGGERED;
                p.mStateCountdown = 5;

                p.PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_block, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 22.0f);
            }
    },  // 37 SEED_UMBRELLA
    &PlantSpecialDefault,  // 38 SEED_MARIGOLD（原 default）
    &PlantSpecialDefault,  // 39 SEED_MELONPULT（原 default）
    &PlantSpecialDefault,  // 40 SEED_GATLINGPEA（原 default）
    &PlantSpecialDefault,  // 41 SEED_TWINSUNFLOWER（原 default）
    &PlantSpecialDefault,  // 42 SEED_GLOOMSHROOM（原 default）
    &PlantSpecialDefault,  // 43 SEED_CATTAIL（原 default）
    &PlantSpecialDefault,  // 44 SEED_WINTERMELON（原 default）
    &PlantSpecialDefault,  // 45 SEED_GOLD_MAGNET（原 default）
    &PlantSpecialDefault,  // 46 SEED_SPIKEROCK（原 default）
    &PlantSpecialDefault,  // 47 SEED_COBCANNON（原 default）
    &PlantSpecialDefault,  // 48 SEED_IMITATER（原 default）
    &PlantSpecialDefault,  // 49 SEED_EXPLODE_O_NUT（原 default）
    &PlantSpecialDefault,  // 50 SEED_GIANT_WALLNUT（原 default）
    &PlantSpecialDefault,  // 51 SEED_SPROUT（原 default）
    &PlantSpecialDefault,  // 52 SEED_LEFTPEATER（原 default）
};
static_assert(sizeof(kPlantSpecialCases) / sizeof(kPlantSpecialCases[0]) ==
    (int)SeedType::NUM_SEED_TYPES, "kPlantSpecialCases 槽位数须与 SeedType 枚举同步");
}  // namespace

//0x4666A0
// GOTY @Patoke: 0x46A110
void Plant::DoSpecial(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    int aPosX = (mX) + mWidth / 2;
    int aPosY = (mY) + mHeight / 2;
    int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);

    // C 形态 -> 匿名 lambda 函数指针表：行序 = SeedType 枚举序（0..52）。
    // 原 default 空操作——缺槽（非即时植物）与表外哨兵均无操作。三 switch
    // 外提局部经 PlantSpecialLocals 传值（switch 后不再读；POTATOMINE 槽
    // 内改写仅槽内可见 = 原 case 局部作用域）。
    PlantSpecialLocals aSpecialLocals{aPosX, aPosY, aDamageRangeFlags};
    const unsigned aSeedIndex = static_cast<unsigned>(mSeedType);
    if (aSeedIndex < static_cast<unsigned>(SeedType::NUM_SEED_TYPES))
    {
        kPlantSpecialCases[aSeedIndex](ownerNowNs, visualNowNs, animationNowNs, *this, aSpecialLocals);
    }

}

//0x466B80
void Plant::ImitaterMorph(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    Die(ownerNowNs, visualNowNs, animationNowNs);
    Plant* aPlant = mBoard->AddPlant(mPlantCol, mRow, mImitaterType, SeedType::SEED_IMITATER);

    FilterEffect aFilter = FilterEffect::FILTER_EFFECT_WASHED_OUT;
    if (mImitaterType == SeedType::SEED_HYPNOSHROOM || mImitaterType == SeedType::SEED_SQUASH || mImitaterType == SeedType::SEED_POTATOMINE ||
        mImitaterType == SeedType::SEED_GARLIC || mImitaterType == SeedType::SEED_LILYPAD)
        aFilter = FilterEffect::FILTER_EFFECT_LESS_WASHED_OUT;

    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(aPlant->mBodyReanimID);

    if (aBodyReanim)
    {
        aBodyReanim->mFilterEffect = aFilter;
    }
    Reanimation* aHeadReanim = mApp->ReanimationTryToGet(aPlant->mHeadReanimID);

    if (aHeadReanim)
    {
        aHeadReanim->mFilterEffect = aFilter;
    }Reanimation* aHeadReanim2 = mApp->ReanimationTryToGet(aPlant->mHeadReanimID2);

    if (aHeadReanim2)
    {
        aHeadReanim2->mFilterEffect = aFilter;
    }Reanimation* aHeadReanim3 = mApp->ReanimationTryToGet(aPlant->mHeadReanimID3);

    if (aHeadReanim3)
    {
        aHeadReanim3->mFilterEffect = aFilter;
    }
}

//0x466CA0
void Plant::UpdateImitater(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    if (mState != PlantState::STATE_IMITATER_MORPHING)
    {
        if (mStateCountdown == 0)
        {
            mState = PlantState::STATE_IMITATER_MORPHING;
            PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_explode, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 26.0f);
        }
    }
    else
    {
        Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

        if (aBodyReanim->ShouldTriggerTimedEvent(0.8f))
        {
            mApp->AddTodParticle((mX) + 40, (mY) + 40, (int)RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_IMITATER_MORPH, ownerNowNs);
        }
        if (aBodyReanim->mLoopCount > 0)
        {
            ImitaterMorph(ownerNowNs, visualNowNs, animationNowNs);
        }
    }
}

//0x466D50
void Plant::CobCannonFire(std::int64_t ownerNowNs, int theTargetX, int theTargetY)
{
    TOD_ASSERT(mState == PlantState::STATE_COBCANNON_READY);

    mState = PlantState::STATE_COBCANNON_FIRING;
    mShootingEndTimeNs = ReanimClock::Add(ownerNowNs, 206 * PlantLaunchClock::Tick);
    PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_shooting, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);

    mTargetX = theTargetX - 47.0f;
    mTargetY = theTargetY;

    Reanimation* aBodyReanim = mApp->ReanimationGet(mBodyReanimID);

    ReanimatorTrackInstance* aTrackInstance = aBodyReanim->GetTrackInstanceByName(ReanimTrackId::CobCannon_cob);
    aTrackInstance->mTrackColor = Color::White;
}

//0x466E00
void Plant::Fire(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie* theTargetZombie, int theRow, PlantWeapon thePlantWeapon)
{
    if (mSeedType == SeedType::SEED_FUMESHROOM)
    {
        DoRowAreaDamage(ownerNowNs, visualNowNs, animationNowNs, 20, 2U);
        mApp->PlayFoley(FoleyType::FOLEY_FUME);
        return;
    }
    if (mSeedType == SeedType::SEED_GLOOMSHROOM)
    {
        DoRowAreaDamage(ownerNowNs, visualNowNs, animationNowNs, 20, 2U);
        return;
    }
    if (mSeedType == SeedType::SEED_STARFRUIT)
    {
        StarFruitFire();
        return;
    }

    ProjectileType aProjectileType;
    // 弹种映射：按 SeedType × PlantWeapon（0=主 1=副）双列直查，行序 = 枚举序。
    // KERNELPULT 副武器黄油覆盖并入次列；非射手槽定值 PEA（原版此处
    // assert+未初始化不可达，表化顺带消除 UB）。
    static constexpr ProjectileType kFireProjectiles[][2] = {
		/* SEED_PEASHOOTER        */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SUNFLOWER         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_CHERRYBOMB        */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_WALLNUT           */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_POTATOMINE        */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SNOWPEA           */ { PROJECTILE_SNOWPEA, PROJECTILE_SNOWPEA },
		/* SEED_CHOMPER           */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_REPEATER          */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_PUFFSHROOM        */ { PROJECTILE_PUFF, PROJECTILE_PUFF },
		/* SEED_SUNSHROOM         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_FUMESHROOM        */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_GRAVEBUSTER       */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_HYPNOSHROOM       */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SCAREDYSHROOM     */ { PROJECTILE_PUFF, PROJECTILE_PUFF },
		/* SEED_ICESHROOM         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_DOOMSHROOM        */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_LILYPAD           */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SQUASH            */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_THREEPEATER       */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_TANGLEKELP        */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_JALAPENO          */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SPIKEWEED         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_TORCHWOOD         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_TALLNUT           */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SEASHROOM         */ { PROJECTILE_PUFF, PROJECTILE_PUFF },
		/* SEED_PLANTERN          */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_CACTUS            */ { PROJECTILE_SPIKE, PROJECTILE_SPIKE },
		/* SEED_BLOVER            */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SPLITPEA          */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_STARFRUIT         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_PUMPKINSHELL      */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_MAGNETSHROOM      */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_CABBAGEPULT       */ { PROJECTILE_CABBAGE, PROJECTILE_CABBAGE },
		/* SEED_FLOWERPOT         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_KERNELPULT        */ { PROJECTILE_KERNEL, PROJECTILE_BUTTER },
		/* SEED_INSTANT_COFFEE    */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_GARLIC            */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_UMBRELLA          */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_MARIGOLD          */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_MELONPULT         */ { PROJECTILE_MELON, PROJECTILE_MELON },
		/* SEED_GATLINGPEA        */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_TWINSUNFLOWER     */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_GLOOMSHROOM       */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_CATTAIL           */ { PROJECTILE_SPIKE, PROJECTILE_SPIKE },
		/* SEED_WINTERMELON       */ { PROJECTILE_WINTERMELON, PROJECTILE_WINTERMELON },
		/* SEED_GOLD_MAGNET       */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SPIKEROCK         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_COBCANNON         */ { PROJECTILE_COBBIG, PROJECTILE_COBBIG },
		/* SEED_IMITATER          */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_EXPLODE_O_NUT     */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_GIANT_WALLNUT     */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SPROUT            */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_LEFTPEATER        */ { PROJECTILE_PEA, PROJECTILE_PEA },
    };
    static_assert(sizeof(kFireProjectiles) / sizeof(kFireProjectiles[0]) == (int)SeedType::NUM_SEED_TYPES,
        "弹种槽位须与 SeedType 枚举同步");
    if ((unsigned)mSeedType < sizeof(kFireProjectiles) / sizeof(kFireProjectiles[0]))
    {
        aProjectileType = kFireProjectiles[mSeedType][(int)thePlantWeapon];
    }
    else
    {
        aProjectileType = ProjectileType::PROJECTILE_PEA;   // 原版不可达 default 定值
    }

    mApp->PlayFoley(FoleyType::FOLEY_THROW);
    if (mSeedType == SeedType::SEED_SNOWPEA || mSeedType == SeedType::SEED_WINTERMELON)
    {
        mApp->PlayFoley(FoleyType::FOLEY_SNOW_PEA_SPARKLES);
    }
    else if (mSeedType == SeedType::SEED_PUFFSHROOM || mSeedType == SeedType::SEED_SCAREDYSHROOM || mSeedType == SeedType::SEED_SEASHROOM)
    {
        mApp->PlayFoley(FoleyType::FOLEY_PUFF);
    }

    // B 形态 -> 二维常量表 kFireOrigin[seed][weapon]：发射原点 {dx,dy} 与
    // 运动型 mMotion 合并一表（原 15 支原点链 + 7 支运动型链退役）。行序 =
    // SeedType 枚举序 0..52 × {WEAPON_PRIMARY, WEAPON_SECONDARY}；
    // kFireOriginDefault = 原原点链 else 兜底 {10,5}；kFireMotionNone =
    // 原运动型链无该分支（不设，保持 AddProjectile 默认）。
    // 表外特例：豌豆头族 6 种（PEASHOOTER/SNOWPEA/REPEATER/LEFTPEATER/
    // GATLINGPEA/SPLITPEA）原点需运行时 GetPeaHeadOffset——原支保留下方；
    // SPLITPEA/LEFTPEATER 的原点槽不可达但运动型槽有效（MOTION_BACKWARDS）。
    static constexpr ProjectileMotion kFireMotionNone = (ProjectileMotion)-1;
    struct FireOriginEntry { int mDx; int mDy; ProjectileMotion mMotion; };
    static constexpr FireOriginEntry kFireOriginDefault = { 10, 5, kFireMotionNone };
    static constexpr FireOriginEntry kFireOrigin[(int)SeedType::NUM_SEED_TYPES][2] = {
        /* SEED_PEASHOOTER        */ { kFireOriginDefault, kFireOriginDefault },  // 原点特例表外（豌豆头族）
        /* SEED_SUNFLOWER         */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_CHERRYBOMB        */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_WALLNUT           */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_POTATOMINE        */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_SNOWPEA           */ { kFireOriginDefault, kFireOriginDefault },  // 原点特例表外（豌豆头族）
        /* SEED_CHOMPER           */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_REPEATER          */ { kFireOriginDefault, kFireOriginDefault },  // 原点特例表外（豌豆头族）
        /* SEED_PUFFSHROOM        */ { { 40, 40, ProjectileMotion::MOTION_PUFF }, { 40, 40, ProjectileMotion::MOTION_PUFF } },
        /* SEED_SUNSHROOM         */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_FUMESHROOM        */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_GRAVEBUSTER       */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_HYPNOSHROOM       */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_SCAREDYSHROOM     */ { { 29, 21, kFireMotionNone }, { 29, 21, kFireMotionNone } },
        /* SEED_ICESHROOM         */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_DOOMSHROOM        */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_LILYPAD           */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_SQUASH            */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_THREEPEATER       */ { { 45, 10, kFireMotionNone }, { 45, 10, kFireMotionNone } },  // 运动型表外（仅 theRow!=mRow 设 MOTION_THREEPEATER——同行豌豆保持默认，2026-10-03 对拍代理发现恒设回归）
        /* SEED_TANGLEKELP        */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_JALAPENO          */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_SPIKEWEED         */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_TORCHWOOD         */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_TALLNUT           */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_SEASHROOM         */ { { 45, 63, ProjectileMotion::MOTION_PUFF }, { 45, 63, ProjectileMotion::MOTION_PUFF } },
        /* SEED_PLANTERN          */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_CACTUS            */ { { 93, -50, kFireMotionNone }, { 70, 23, kFireMotionNone } },
        /* SEED_BLOVER            */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_SPLITPEA          */ { { 10, 5, kFireMotionNone }, { 10, 5, ProjectileMotion::MOTION_BACKWARDS } },  // 原点特例表外；副武器 MOTION_BACKWARDS
        /* SEED_STARFRUIT         */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_PUMPKINSHELL      */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_MAGNETSHROOM      */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_CABBAGEPULT       */ { { 5, -12, ProjectileMotion::MOTION_LOBBED }, { 5, -12, ProjectileMotion::MOTION_LOBBED } },
        /* SEED_FLOWERPOT         */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_KERNELPULT        */ { { 19, -37, ProjectileMotion::MOTION_LOBBED }, { 12, -56, ProjectileMotion::MOTION_LOBBED } },
        /* SEED_INSTANT_COFFEE    */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_GARLIC            */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_UMBRELLA          */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_MARIGOLD          */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_MELONPULT         */ { { 25, -46, ProjectileMotion::MOTION_LOBBED }, { 25, -46, ProjectileMotion::MOTION_LOBBED } },
        /* SEED_GATLINGPEA        */ { kFireOriginDefault, kFireOriginDefault },  // 原点特例表外（豌豆头族）
        /* SEED_TWINSUNFLOWER     */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_GLOOMSHROOM       */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_CATTAIL           */ { { 20, -3, ProjectileMotion::MOTION_HOMING }, { 20, -3, ProjectileMotion::MOTION_HOMING } },
        /* SEED_WINTERMELON       */ { { 25, -46, ProjectileMotion::MOTION_LOBBED }, { 25, -46, ProjectileMotion::MOTION_LOBBED } },
        /* SEED_GOLD_MAGNET       */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_SPIKEROCK         */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_COBCANNON         */ { { -44, -184, ProjectileMotion::MOTION_LOBBED }, { -44, -184, ProjectileMotion::MOTION_LOBBED } },
        /* SEED_IMITATER          */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_EXPLODE_O_NUT     */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_GIANT_WALLNUT     */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_SPROUT            */ { kFireOriginDefault, kFireOriginDefault },
        /* SEED_LEFTPEATER        */ { { 10, 5, ProjectileMotion::MOTION_BACKWARDS }, { 10, 5, ProjectileMotion::MOTION_BACKWARDS } },  // 原点特例表外
    };
    static_assert(sizeof(kFireOrigin) / sizeof(kFireOrigin[0]) == (int)SeedType::NUM_SEED_TYPES,
        "kFireOrigin 槽位须与 SeedType 枚举同步");
    const FireOriginEntry& aFireRow = (unsigned)mSeedType < (int)SeedType::NUM_SEED_TYPES
        ? kFireOrigin[(int)mSeedType][(int)thePlantWeapon]
        : kFireOriginDefault;

    int aOriginX, aOriginY;
    if (mSeedType == SeedType::SEED_PEASHOOTER || mSeedType == SeedType::SEED_SNOWPEA || mSeedType == SeedType::SEED_REPEATER)
    {
        int aOffsetX, aOffsetY;
        GetPeaHeadOffset(ownerNowNs, aOffsetX, aOffsetY);
        aOriginX = (mX) + aOffsetX + 24;
        aOriginY = (mY) + aOffsetY - 33;
    }
    else if (mSeedType == SeedType::SEED_LEFTPEATER)
    {
        int aOffsetX, aOffsetY;
        GetPeaHeadOffset(ownerNowNs, aOffsetX, aOffsetY);
        aOriginX = (mX) - aOffsetX + 27;
        aOriginY = (mY) + aOffsetY - 33;
    }
    else if (mSeedType == SeedType::SEED_GATLINGPEA)
    {
        int aOffsetX, aOffsetY;
        GetPeaHeadOffset(ownerNowNs, aOffsetX, aOffsetY);
        aOriginX = (mX) + aOffsetX + 34;
        aOriginY = (mY) + aOffsetY - 33;
    }
    else if (mSeedType == SeedType::SEED_SPLITPEA)
    {
        int aOffsetX, aOffsetY;
        GetPeaHeadOffset(ownerNowNs, aOffsetX, aOffsetY);
        aOriginY = (mY) + aOffsetY - 33;

        if (thePlantWeapon == PlantWeapon::WEAPON_SECONDARY)
        {
            aOriginX = (mX) + aOffsetX - 64;
        }
        else
        {
            aOriginX = (mX) + aOffsetX + 24;
        }
    }
    else
    {
        aOriginX = (mX) + aFireRow.mDx;
        aOriginY = (mY) + aFireRow.mDy;
    }
    if (mBoard->GetFlowerPotAt(mPlantCol, mRow))
    {
        aOriginY -= 5;
    }

    if (mSeedType == SeedType::SEED_SNOWPEA)
    {
        int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_LAWN_MOWER, mRow, 1);
        mApp->AddTodParticle(aOriginX + 8, aOriginY + 13, aRenderPosition, ParticleEffect::PARTICLE_SNOWPEA_PUFF, ownerNowNs);
    }
    else if (mSeedType == SeedType::SEED_PUFFSHROOM)
    {
        int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_LAWN_MOWER, mRow, 1);
        mApp->AddTodParticle(aOriginX + 18, aOriginY + 13, aRenderPosition, ParticleEffect::PARTICLE_PUFFSHROOM_MUZZLE, ownerNowNs);
    }
    else if (mSeedType == SeedType::SEED_SCAREDYSHROOM)
    {
        int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_LAWN_MOWER, mRow, 1);
        mApp->AddTodParticle(aOriginX + 27, aOriginY + 13, aRenderPosition, ParticleEffect::PARTICLE_PUFFSHROOM_MUZZLE, ownerNowNs);
    }

    Projectile* aProjectile = mBoard->AddProjectile(aOriginX, aOriginY, mRenderOrder - 1, theRow, aProjectileType);
    aProjectile->mDamageRangeFlags = GetDamageRangeFlags(thePlantWeapon);

    // 运动型列随 kFireOrigin 表分派（纯赋值三支已入表退役：PUFFSHROOM/
    // SEASHROOM=MOTION_PUFF、SPLITPEA 副武器/LEFTPEATER=MOTION_BACKWARDS）；
    // 未列种子不设（原链无 else）。以下仅保留动力学特例四支。
    if (aFireRow.mMotion != kFireMotionNone)
    {
        aProjectile->mMotionType = aFireRow.mMotion;
    }

    if (mSeedType == SeedType::SEED_CABBAGEPULT || mSeedType == SeedType::SEED_KERNELPULT ||
        mSeedType == SeedType::SEED_MELONPULT || mSeedType == SeedType::SEED_WINTERMELON)
    {
        float aRangeX, aRangeY;
        if (theTargetZombie)
        {
            Rect aZombieRect = theTargetZombie->GetZombieRect();
            aRangeX = theTargetZombie->ZombieTargetLeadX(ownerNowNs, 50.0f) - aOriginX - 30.0f;
            aRangeY = aZombieRect.mY - aOriginY;

            if (theTargetZombie->mZombiePhase == ZombiePhase::PHASE_DOLPHIN_RIDING)
            {
                aRangeX -= 60.0f;
            }
            if (theTargetZombie->mZombieType == ZombieType::ZOMBIE_POGO && theTargetZombie->mHasObject)
            {
                aRangeX -= 60.0f;
            }
            if (theTargetZombie->mZombiePhase == ZombiePhase::PHASE_SNORKEL_WALKING_IN_POOL)
            {
                aRangeX -= 40.0f;
            }
            if (((ZombieFlagsOf(theTargetZombie->mZombieType) & ZombieFlags::Boss) != 0))
            {
                aRangeY = mBoard->GridToPixelY(8, mRow) - aOriginY;
            }
        }
        else
        {
            aRangeX = 700.0f - aOriginX;
            aRangeY = 0.0f;
        }
        if (aRangeX < 40.0f)
        {
            aRangeX = 40.0f;
        }

        aProjectile->mMotionType = ProjectileMotion::MOTION_LOBBED;
        aProjectile->mVelX = aRangeX / 120.0f;
        aProjectile->mVelY = 0.0f;
        aProjectile->mVelZ = aRangeY / 120.0f - 7.0f;
        aProjectile->mAccZ = 0.115f;
    }
    else if (mSeedType == SeedType::SEED_THREEPEATER)
    {
        if (theRow < mRow)
        {
            aProjectile->mMotionType = ProjectileMotion::MOTION_THREEPEATER;
            aProjectile->mVelY = -3.0f;
            aProjectile->mShadowY += 80.0f;
        }
        else if (theRow > mRow)
        {
            aProjectile->mMotionType = ProjectileMotion::MOTION_THREEPEATER;
            aProjectile->mVelY = 3.0f;
            aProjectile->mShadowY -= 80.0f;
        }
    }
    else if (mSeedType == SeedType::SEED_CATTAIL)
    {
        aProjectile->mVelX = 2.0f;
        aProjectile->mMotionType = ProjectileMotion::MOTION_HOMING;
        aProjectile->mTargetZombieID = mBoard->ZombieGetID(theTargetZombie);
    }
    else if (mSeedType == SeedType::SEED_COBCANNON)
    {
        aProjectile->mVelX = 0.001f;
        aProjectile->mDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
        aProjectile->mMotionType = ProjectileMotion::MOTION_LOBBED;
        aProjectile->mVelY = 0.0f;
        aProjectile->mAccZ = 0.0f;
        aProjectile->mVelZ = -8.0f;
        aProjectile->mCobTargetX = mTargetX - 40;
        aProjectile->mCobTargetRow = mBoard->PixelToGridYKeepOnBoard(mTargetX, mTargetY);
    }
}

//0x4675C0
Zombie* Plant::FindTargetZombie(int theRow, PlantWeapon thePlantWeapon)
{
    int aDamageRangeFlags = GetDamageRangeFlags(thePlantWeapon);
    Rect aAttackRect = GetPlantAttackRect(thePlantWeapon);
    int aHighestWeight = 0;
    Zombie* aBestZombie = nullptr;

    // 传送门判定只依赖植物与模式，提到循环外（逐僵尸重复求值语义等价）
    bool needPortalCheck = false;
    if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_PORTAL_COMBAT)
    {
        if (mSeedType == SeedType::SEED_PEASHOOTER || mSeedType == SeedType::SEED_CACTUS || mSeedType == SeedType::SEED_REPEATER)
        {
            needPortalCheck = true;
        }
    }

    // 行段路径（2026-10-04 用户方案；同日全量转换扩忧郁支）：精确行射手
    // 走 IterateZombiesInRow（候选集与评估序都与原全池扫逐字一致）；忧郁
    // 蘑菇 ±1 行+Boss 行偏差归零走区间迭代（行主序≠池扫全序——选择点
    // 加槽位决胜后等价）；香蒲/传送门保持全池
    const bool aGloomScan = mSeedType == SeedType::SEED_GLOOMSHROOM;
    const bool aRowSegmentScan = mSeedType != SeedType::SEED_CATTAIL && !aGloomScan && !needPortalCheck;

    Zombie* aZombie = nullptr;
    while (aRowSegmentScan ? mBoard->IterateZombiesInRow(theRow, aZombie)
        : aGloomScan ? mBoard->IterateZombiesInRowRange(theRow - 1, theRow + 1, aZombie)
        : mBoard->IterateZombies(aZombie))
    {
        int aRowDeviation = aZombie->mRow - theRow;
        if (((ZombieFlagsOf(aZombie->mZombieType) & ZombieFlags::Boss) != 0))
        {
            aRowDeviation = 0;
        }

        if (!aZombie->mHasHead || aZombie->IsTangleKelpTarget())
        {
            if (mSeedType == SeedType::SEED_POTATOMINE || mSeedType == SeedType::SEED_CHOMPER || mSeedType == SeedType::SEED_TANGLEKELP)
            {
                continue;
            }
        }

        if (mSeedType != SeedType::SEED_CATTAIL)
        {
            if (mSeedType == SeedType::SEED_GLOOMSHROOM)
            {
                if (aRowDeviation < -1 || aRowDeviation > 1)
                {
                    continue;
                }
            }
            else if (needPortalCheck)
            {
                if (!mBoard->mChallenge->CanTargetZombieWithPortals(this, aZombie))
                {
                    continue;
                }
            }
            else if (aRowDeviation)
            {
                continue;
            }
        }

        if (aZombie->EffectedByDamage(aDamageRangeFlags))
        {
            int aExtraRange = 0;

            if (mSeedType == SeedType::SEED_CHOMPER)
            {
                if (aZombie->mZombiePhase == ZombiePhase::PHASE_DIGGER_WALKING)
                {
                    aAttackRect.mX += 20;
                    aAttackRect.mWidth -= 20;
                }

                if (aZombie->mZombiePhase == ZombiePhase::PHASE_POGO_BOUNCING || (aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE && aZombie->mTargetCol == mPlantCol))
                {
                    continue;
                }

                if (aZombie->mIsEating || mState == PlantState::STATE_CHOMPER_BITING)
                {
                    aExtraRange = 60;
                }
            }

            if (mSeedType == SeedType::SEED_POTATOMINE)
            {
                if ((aZombie->mZombieType == ZombieType::ZOMBIE_POGO && aZombie->mHasObject) ||
                    aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT || aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_PRE_VAULT)
                {
                    continue;
                }

                if (aZombie->mZombieType == ZombieType::ZOMBIE_POLEVAULTER)
                {
                    aAttackRect.mX += 40;
                    aAttackRect.mWidth -= 40;  // 原版经典土豆地雷 Bug 及“四撑杆引雷”的原理
                }

                if (aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE && aZombie->mTargetCol != mPlantCol)
                {
                    continue;
                }

                if (aZombie->mIsEating)
                {
                    aExtraRange = 30;
                }
            }

            if ((mSeedType == SeedType::SEED_EXPLODE_O_NUT && aZombie->mZombiePhase == ZombiePhase::PHASE_POLEVAULTER_IN_VAULT) ||
                (mSeedType == SeedType::SEED_TANGLEKELP && !aZombie->mInPool))
            {
                continue;
            }

            Rect aZombieRect = aZombie->GetZombieRect();
            if (!needPortalCheck && GetRectOverlap(aAttackRect, aZombieRect) < -aExtraRange)
            {
                continue;
            }

            ////////////////////

            int aWeight = -aZombieRect.mX;
            if (mSeedType == SeedType::SEED_CATTAIL)
            {
                aWeight = -Distance2D((mX) + 40.0f, (mY) + 40.0f, aZombieRect.mX + aZombieRect.mWidth / 2, aZombieRect.mY + aZombieRect.mHeight / 2);
                if (aZombie->IsFlying())
                {
                    aWeight += 10000;  // 优先攻击飞行单位
                }
            }

            // 槽位决胜：忧郁支行主序≠池扫全序，平局取小槽 = 池扫严格>
            // 保留先见者的同一结果（精确行支/全池支升序下此条件恒假=无操作）
            if (aBestZombie == nullptr || aWeight > aHighestWeight ||
                (aWeight == aHighestWeight && mBoard->ZombieSlotOf(aZombie) < mBoard->ZombieSlotOf(aBestZombie)))
            {
                aHighestWeight = aWeight;
                aBestZombie = aZombie;
            }
        }
    }

    return aBestZombie;
}

//0x4678B0
int Plant::DistanceToClosestZombie()
{
    int aDamageRangeFlags = GetDamageRangeFlags(PlantWeapon::WEAPON_PRIMARY);
    Rect aAttackRect = GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);
    int aClosestDistance = 1000;

    Zombie* aZombie = nullptr;
    // 行段纯行路径（2026-10-04 用户裁决：主动偏离 1073，删 Boss 检测——
    // 土豆雷灯光检测僵王意义不明；纯行段迭代不吐 Boss）
    while (mBoard->IterateZombiesInRowOnly(mRow, aZombie))
    {
        if (aZombie->EffectedByDamage(aDamageRangeFlags))
        {
            Rect aZombieRect = aZombie->GetZombieRect();
            int aDistance = -GetRectOverlap(aAttackRect, aZombieRect);
            if (aDistance < aClosestDistance)
            {
                aClosestDistance = std::max(aDistance, 0);
            }
        }
    }

    return aClosestDistance;
}

//0x4679B0
void Plant::Die(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs)
{
    // PSS 修复（用户方案）：玉米炮死亡即清空瞄准该炮的发射状态——防
    // DataArray 槽位回收复用后 mCobCannonPlantID 悬空（1073 原版：模仿者
    // 土豆雷复用同 ID 会被 CobCannonFire 强改状态卡出无敌土豆，且土豆
    // reanim 无 CobCannon_cob 轨道导致越界写）。
    if (mIsOnBoard && mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_COBCANNON_TARGET &&
        mBoard->mCursorObject->mCobCannonPlantID == (PlantID)mBoard->mPlants.DataArrayGetID(this))
    {
        mBoard->ClearCursor();
    }

    if (mIsOnBoard && mSeedType == SeedType::SEED_TANGLEKELP)
    {
        Zombie* aZombie = mBoard->ZombieTryToGet(mTargetZombieID);
        if (aZombie)
        {
            aZombie->DieWithLoot(ownerNowNs, visualNowNs, animationNowNs);
        }
    }

    mDead = true;
    RemoveEffects();

    if (!Plant::IsFlying(mSeedType) && mIsOnBoard)
    {
        GridItem* aLadder = mBoard->GetLadderAt(mPlantCol, mRow);
        if (aLadder)
        {
            aLadder->GridItemDie();
        }
    }

    if (mIsOnBoard)
    {
        Plant* aTopPlant = mBoard->GetTopPlantAt(mPlantCol, mRow, PlantPriority::TOPPLANT_BUNGEE_ORDER);
        Plant* aFlowerPot = mBoard->GetFlowerPotAt(mPlantCol, mRow);
        if (aFlowerPot && aTopPlant == aFlowerPot)
        {
            Reanimation* aPotReanim = mApp->ReanimationGet(aFlowerPot->mBodyReanimID);

            aPotReanim->ApplyRate(RandRangeFloat(10.0f, 15.0f));
        }
    }
}

PlantDefinition& GetPlantDefinition(SeedType theSeedType)
{
    TOD_ASSERT(gPlantDefs[theSeedType].mSeedType == theSeedType);
    TOD_ASSERT(theSeedType >= 0 && theSeedType < (int)SeedType::NUM_SEED_TYPES);

    // 界内守卫（2026-10-04 用户实机崩溃链修复的防再踩半边）：TOD_ASSERT 在
    // release 展开为空——僵尸卡（SeedType 60..74）曾因 kSeedToolTip 少行
    // 落 nullptr 而流入此处，gPlantDefs[53] 越界读垃圾 mPlantName 解引用。
    // 表已修对（正常键域 0..52），本守卫为改版/新路径的兜底，越界回落
    // 0 号定义（语义无用但不崩，调用方只取名串/刷新期等展示值）。
    if ((unsigned)theSeedType >= (int)SeedType::NUM_SEED_TYPES)
        return gPlantDefs[0];

    return gPlantDefs[theSeedType];
}

//0x467B00
int Plant::GetCost(SeedType theSeedType, SeedType theImitaterType)
{
    if (gLawnApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED || gLawnApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST)
    {
        // 原版 5 支链保留（对拍 2026-10-03：键值对表即便 static 后仍慢 60%——
        // 5 支短链分支预测近乎全中，线性键值扫描 + 结构体解包纯倒贴；短链
        // 不转表先例见账本裁剪函数 3-case 行）
        if (theSeedType == SeedType::SEED_REPEATER)
        {
            return 1000;
        }
        else if (theSeedType == SeedType::SEED_FUMESHROOM)
        {
            return 500;
        }
        else if (theSeedType == SeedType::SEED_TALLNUT)
        {
            return 250;
        }
        else if (theSeedType == SeedType::SEED_BEGHOULED_BUTTON_SHUFFLE)
        {
            return 100;
        }
        else if (theSeedType == SeedType::SEED_BEGHOULED_BUTTON_CRATER)
        {
            return 200;
        }
    }

    // 覆盖价（尾段连续区 SEED_SLOT_MACHINE_SUN..SEED_ZOMBIE_IMP = 枚举序 56..74）：
    // 按槽位直查（行序 = 枚举序）；其余种子走植物定义表（含模仿者特判）。
    static constexpr int kSeedCostOverrides[] = {
        0,     // SEED_SLOT_MACHINE_SUN
        0,     // SEED_SLOT_MACHINE_DIAMOND
        100,   // SEED_ZOMBIQUARIUM_SNORKLE
        1000,  // SEED_ZOMBIQUARIUM_TROPHY
        50,    // SEED_ZOMBIE_NORMAL
        75,    // SEED_ZOMBIE_TRAFFIC_CONE
        75,    // SEED_ZOMBIE_POLEVAULTER
        125,   // SEED_ZOMBIE_PAIL
        150,   // SEED_ZOMBIE_LADDER
        125,   // SEED_ZOMBIE_DIGGER
        125,   // SEED_ZOMBIE_BUNGEE
        175,   // SEED_ZOMBIE_FOOTBALL
        150,   // SEED_ZOMBIE_BALLOON
        100,   // SEED_ZOMBIE_SCREEN_DOOR
        175,   // SEED_ZOMBONI
        200,   // SEED_ZOMBIE_POGO
        350,   // SEED_ZOMBIE_DANCER
        300,   // SEED_ZOMBIE_GARGANTUAR
        50,    // SEED_ZOMBIE_IMP
    };
    static_assert(sizeof(kSeedCostOverrides) / sizeof(kSeedCostOverrides[0])
        == (int)SeedType::SEED_ZOMBIE_IMP - (int)SeedType::SEED_SLOT_MACHINE_SUN + 1,
        "覆盖价槽位须与 SeedType 枚举尾段同步");
    if (theSeedType >= SeedType::SEED_SLOT_MACHINE_SUN && theSeedType <= SeedType::SEED_ZOMBIE_IMP)
    {
        return kSeedCostOverrides[(int)theSeedType - (int)SeedType::SEED_SLOT_MACHINE_SUN];
    }

    if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
    {
        const PlantDefinition& aPlantDef = GetPlantDefinition(theImitaterType);
        return aPlantDef.mSeedCost;
    }
    const PlantDefinition& aPlantDef = GetPlantDefinition(theSeedType);
    return aPlantDef.mSeedCost;
}

//0x467C00
// GOTY @Patoke: 0x46B6C0
SexyString Plant::GetNameString(SeedType theSeedType, SeedType theImitaterType)
{
    SexyString aTranslatedName = EntityPlantName(theSeedType);

    if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
    {
        const std::string& aTranslatedImitaterName = EntityPlantName(theImitaterType);
        return StrFormat(("%s %s"), aTranslatedName.c_str(), aTranslatedImitaterName.c_str());
    }

    return aTranslatedName;
}

//0x467DB0
SexyString Plant::GetToolTip(SeedType theSeedType)
{
    return EntityPlantToolTip(theSeedType);
}

//0x467E30
int Plant::GetRefreshTime(SeedType theSeedType, SeedType theImitaterType)
{
    if (Challenge::IsZombieSeedType(theSeedType))
    {
        return 0;
    }

    if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
    {
        const PlantDefinition& aPlantDef = GetPlantDefinition(theImitaterType);
        return aPlantDef.mRefreshTime;
    }
    else
    {
        const PlantDefinition& aPlantDef = GetPlantDefinition(theSeedType);
        return aPlantDef.mRefreshTime;
    }
}

//0x467E60
bool Plant::IsNocturnal(SeedType theSeedtype)
{
    // 2026-10-04 标志字表化（用户方案）：OR 链 → kPlantFlags 位判定；
    // 语义按构造等价（表行=原链成员集，PlantFlags.h static_assert 锚）。
    return (PlantFlagsOf(theSeedtype) & PlantFlags::Nocturnal) != 0;
}

// GOTY @Patoke: inlined 0x40FB2B
bool Plant::IsFungus(SeedType theSeedType)
{
    // 2026-10-04 标志字表化（用户方案）：OR 链 → kPlantFlags 位判定；
    // 语义按构造等价（表行=原链成员集，PlantFlags.h static_assert 锚）。
    return (PlantFlagsOf(theSeedType) & PlantFlags::Fungus) != 0;
}

//0x467EA0
bool Plant::IsAquatic(SeedType theSeedType)
{
    // 2026-10-04 标志字表化（用户方案）：OR 链 → kPlantFlags 位判定；
    // 语义按构造等价（表行=原链成员集，PlantFlags.h static_assert 锚）。
    return (PlantFlagsOf(theSeedType) & PlantFlags::Aquatic) != 0;
}

// GOTY @Patoke: 0x469543
bool Plant::IsFlying(SeedType theSeedtype)
{
    // 2026-10-04 标志字表化（用户方案）：OR 链 → kPlantFlags 位判定；
    // 语义按构造等价（表行=原链成员集，PlantFlags.h static_assert 锚）。
    return (PlantFlagsOf(theSeedtype) & PlantFlags::Flying) != 0;
}

//0x467EC0
bool Plant::IsUpgrade(SeedType theSeedtype)
{
    // 2026-10-04 标志字表化（用户方案）：OR 链 → kPlantFlags 位判定；
    // 语义按构造等价（表行=原链成员集，PlantFlags.h static_assert 锚）。
    return (PlantFlagsOf(theSeedtype) & PlantFlags::Upgrade) != 0;
}

//0x467EF0

namespace
{
// GetPlantAttackRect 槽位表：每槽 = 原 case 的 Rect 表达式（生成器迁移 +
// 逆变换 token 对拍 0 差异）；default 表达式具名并被越界分支复用。
using AttackRectCaseFn = Rect (*)(const Plant&);

// 标签共体（匿名 lambda 无法跨槽复用，复用即具名）
// 原 case SEED_SPIKEWEED, SEED_SPIKEROCK
Rect AttackRectSpikeweed(const Plant& p)
{
    return Rect((p.mX) + 20,       (p.mY),             p.mWidth - 50,        p.mHeight);
}

// 标签共体（匿名 lambda 无法跨槽复用，复用即具名）
// 原 case SEED_PUFFSHROOM, SEED_SEASHROOM
Rect AttackRectPuffshroom(const Plant& p)
{
    return Rect((p.mX) + 60,       (p.mY),             230,                p.mHeight);
}

// 原 default（无 case 的 40 型与表外哨兵共用）
Rect AttackRectDefault(const Plant& p)
{
    return Rect((p.mX) + 60,       (p.mY),             BOARD_WIDTH,        p.mHeight);
}

// 匿名 lambda 函数指针表，行序 = SeedType 枚举值序（0..52）。
constexpr AttackRectCaseFn kAttackRectCases[(int)SeedType::NUM_SEED_TYPES] = {
    &AttackRectDefault,  // 0 SEED_PEASHOOTER（原 default）
    &AttackRectDefault,  // 1 SEED_SUNFLOWER（原 default）
    &AttackRectDefault,  // 2 SEED_CHERRYBOMB（原 default）
    &AttackRectDefault,  // 3 SEED_WALLNUT（原 default）
    // 原 case SEED_POTATOMINE
    [](const Plant& p)
    {
    return Rect((p.mX),            (p.mY),             p.mWidth - 25,        p.mHeight);
    },  // 4 SEED_POTATOMINE
    &AttackRectDefault,  // 5 SEED_SNOWPEA（原 default）
    // 原 case SEED_CHOMPER
    [](const Plant& p)
    {
    return Rect((p.mX) + 80,       (p.mY),             40,                 p.mHeight);
    },  // 6 SEED_CHOMPER
    &AttackRectDefault,  // 7 SEED_REPEATER（原 default）
    &AttackRectPuffshroom,  // 8 SEED_PUFFSHROOM（共体）
    &AttackRectDefault,  // 9 SEED_SUNSHROOM（原 default）
    // 原 case SEED_FUMESHROOM
    [](const Plant& p)
    {
    return Rect((p.mX) + 60,       (p.mY),             340,                p.mHeight);
    },  // 10 SEED_FUMESHROOM
    &AttackRectDefault,  // 11 SEED_GRAVEBUSTER（原 default）
    &AttackRectDefault,  // 12 SEED_HYPNOSHROOM（原 default）
    &AttackRectDefault,  // 13 SEED_SCAREDYSHROOM（原 default）
    &AttackRectDefault,  // 14 SEED_ICESHROOM（原 default）
    &AttackRectDefault,  // 15 SEED_DOOMSHROOM（原 default）
    &AttackRectDefault,  // 16 SEED_LILYPAD（原 default）
    // 原 case SEED_SQUASH
    [](const Plant& p)
    {
    return Rect((p.mX) + 20,       (p.mY),             p.mWidth - 35,        p.mHeight);
    },  // 17 SEED_SQUASH
    &AttackRectDefault,  // 18 SEED_THREEPEATER（原 default）
    // 原 case SEED_TANGLEKELP
    [](const Plant& p)
    {
    return Rect((p.mX),            (p.mY),             p.mWidth,             p.mHeight);
    },  // 19 SEED_TANGLEKELP
    &AttackRectDefault,  // 20 SEED_JALAPENO（原 default）
    &AttackRectSpikeweed,  // 21 SEED_SPIKEWEED（共体）
    // 原 case SEED_TORCHWOOD
    [](const Plant& p)
    {
    return Rect((p.mX) + 50,       (p.mY),             30,                 p.mHeight);
    },  // 22 SEED_TORCHWOOD
    &AttackRectDefault,  // 23 SEED_TALLNUT（原 default）
    &AttackRectPuffshroom,  // 24 SEED_SEASHROOM（共体）
    &AttackRectDefault,  // 25 SEED_PLANTERN（原 default）
    &AttackRectDefault,  // 26 SEED_CACTUS（原 default）
    &AttackRectDefault,  // 27 SEED_BLOVER（原 default）
    &AttackRectDefault,  // 28 SEED_SPLITPEA（原 default）
    &AttackRectDefault,  // 29 SEED_STARFRUIT（原 default）
    &AttackRectDefault,  // 30 SEED_PUMPKINSHELL（原 default）
    &AttackRectDefault,  // 31 SEED_MAGNETSHROOM（原 default）
    &AttackRectDefault,  // 32 SEED_CABBAGEPULT（原 default）
    &AttackRectDefault,  // 33 SEED_FLOWERPOT（原 default）
    &AttackRectDefault,  // 34 SEED_KERNELPULT（原 default）
    &AttackRectDefault,  // 35 SEED_INSTANT_COFFEE（原 default）
    &AttackRectDefault,  // 36 SEED_GARLIC（原 default）
    &AttackRectDefault,  // 37 SEED_UMBRELLA（原 default）
    &AttackRectDefault,  // 38 SEED_MARIGOLD（原 default）
    &AttackRectDefault,  // 39 SEED_MELONPULT（原 default）
    &AttackRectDefault,  // 40 SEED_GATLINGPEA（原 default）
    &AttackRectDefault,  // 41 SEED_TWINSUNFLOWER（原 default）
    // 原 case SEED_GLOOMSHROOM
    [](const Plant& p)
    {
    return Rect((p.mX) - 80,       (p.mY) - 80,        240,                240);
    },  // 42 SEED_GLOOMSHROOM
    // 原 case SEED_CATTAIL
    [](const Plant& p)
    {
    return Rect(-BOARD_WIDTH,  -BOARD_HEIGHT,  BOARD_WIDTH * 2,    BOARD_HEIGHT * 2);
    },  // 43 SEED_CATTAIL
    &AttackRectDefault,  // 44 SEED_WINTERMELON（原 default）
    &AttackRectDefault,  // 45 SEED_GOLD_MAGNET（原 default）
    &AttackRectSpikeweed,  // 46 SEED_SPIKEROCK（共体）
    &AttackRectDefault,  // 47 SEED_COBCANNON（原 default）
    &AttackRectDefault,  // 48 SEED_IMITATER（原 default）
    &AttackRectDefault,  // 49 SEED_EXPLODE_O_NUT（原 default）
    &AttackRectDefault,  // 50 SEED_GIANT_WALLNUT（原 default）
    &AttackRectDefault,  // 51 SEED_SPROUT（原 default）
    // 原 case SEED_LEFTPEATER
    [](const Plant& p)
    {
    return Rect(0,             (p.mY),             (p.mX),                 p.mHeight);
    },  // 52 SEED_LEFTPEATER
};
static_assert(sizeof(kAttackRectCases) / sizeof(kAttackRectCases[0]) ==
    (int)SeedType::NUM_SEED_TYPES, "kAttackRectCases 槽位数须与 SeedType 枚举同步");
}  // namespace

//0x467F90
Rect Plant::GetPlantAttackRect(PlantWeapon thePlantWeapon)
{
    Rect aRect;
    if (mApp->IsWallnutBowlingLevel())
    {
        aRect = Rect((mX), (mY), mWidth - 20, mHeight);
    }
    else if (thePlantWeapon == PlantWeapon::WEAPON_SECONDARY && mSeedType == SeedType::SEED_SPLITPEA)
    {
        aRect = Rect(0, (mY), (mX) + 16, mHeight);
    }
    else
    {
        // C 形态 -> 匿名 lambda 函数指针表：行序 = SeedType 枚举序（0..52）。
        // 原 default 非空——无 case 的型（含 BEGHOULED 按钮等哨兵）均走它。
        const unsigned aSeedIndex = static_cast<unsigned>(mSeedType);
        if (aSeedIndex < static_cast<unsigned>(SeedType::NUM_SEED_TYPES))
        {
            return kAttackRectCases[aSeedIndex](*this);
        }
        return AttackRectDefault(*this);
    }


    return aRect;
}

//0x4681E0
void Plant::PreloadPlantResources(SeedType theSeedType)
{
    const PlantDefinition& aPlantDef = GetPlantDefinition(theSeedType);
    if (aPlantDef.mReanimationType != ReanimationType::REANIM_NONE)
    {
        ReanimatorEnsureDefinitionLoaded(aPlantDef.mReanimationType, true);
    }

    // B 形态 -> 常量二维表 kPlantPreloadExtras（原 3 支种子等值链合并，仿
    // Zombie 域 kZombiePreloadExtras 范式）：行序 = SeedType 枚举序 0..52
    //（NUM 哨兵锚定），每行至多 2 个附加 reanim 定义，REANIM_NONE = 行尾
    // 哨兵/无 case。第 4 支 IsNocturnal 谓词（菇类集合）与前三支枚举集合
    // 不相交，异构特例表外保留（表后 if）。
    static constexpr ReanimationType kPlantPreloadExtras[(int)SeedType::NUM_SEED_TYPES][2] = {
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 0 SEED_PEASHOOTER（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 1 SEED_SUNFLOWER（无 case）
        { ReanimationType::REANIM_ZOMBIE_CHARRED, ReanimationType::REANIM_NONE },  // 2 SEED_CHERRYBOMB
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 3 SEED_WALLNUT（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 4 SEED_POTATOMINE（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 5 SEED_SNOWPEA（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 6 SEED_CHOMPER（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 7 SEED_REPEATER（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 8 SEED_PUFFSHROOM（夜行性，REANIM_SLEEPING 表外）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 9 SEED_SUNSHROOM（夜行性，REANIM_SLEEPING 表外）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 10 SEED_FUMESHROOM（夜行性，REANIM_SLEEPING 表外）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 11 SEED_GRAVEBUSTER（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 12 SEED_HYPNOSHROOM（夜行性，REANIM_SLEEPING 表外）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 13 SEED_SCAREDYSHROOM（夜行性，REANIM_SLEEPING 表外）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 14 SEED_ICESHROOM（夜行性，REANIM_SLEEPING 表外）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 15 SEED_DOOMSHROOM（夜行性，REANIM_SLEEPING 表外）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 16 SEED_LILYPAD（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 17 SEED_SQUASH（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 18 SEED_THREEPEATER（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 19 SEED_TANGLEKELP（无 case）
        { ReanimationType::REANIM_JALAPENO_FIRE, ReanimationType::REANIM_NONE },  // 20 SEED_JALAPENO
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 21 SEED_SPIKEWEED（无 case）
        { ReanimationType::REANIM_FIRE_PEA, ReanimationType::REANIM_JALAPENO_FIRE },  // 22 SEED_TORCHWOOD
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 23 SEED_TALLNUT（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 24 SEED_SEASHROOM（夜行性，REANIM_SLEEPING 表外）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 25 SEED_PLANTERN（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 26 SEED_CACTUS（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 27 SEED_BLOVER（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 28 SEED_SPLITPEA（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 29 SEED_STARFRUIT（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 30 SEED_PUMPKINSHELL（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 31 SEED_MAGNETSHROOM（夜行性，REANIM_SLEEPING 表外）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 32 SEED_CABBAGEPULT（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 33 SEED_FLOWERPOT（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 34 SEED_KERNELPULT（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 35 SEED_INSTANT_COFFEE（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 36 SEED_GARLIC（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 37 SEED_UMBRELLA（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 38 SEED_MARIGOLD（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 39 SEED_MELONPULT（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 40 SEED_GATLINGPEA（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 41 SEED_TWINSUNFLOWER（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 42 SEED_GLOOMSHROOM（夜行性，REANIM_SLEEPING 表外）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 43 SEED_CATTAIL（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 44 SEED_WINTERMELON（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 45 SEED_GOLD_MAGNET（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 46 SEED_SPIKEROCK（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 47 SEED_COBCANNON（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 48 SEED_IMITATER（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 49 SEED_EXPLODE_O_NUT（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 50 SEED_GIANT_WALLNUT（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 51 SEED_SPROUT（无 case）
        { ReanimationType::REANIM_NONE, ReanimationType::REANIM_NONE },  // 52 SEED_LEFTPEATER（无 case）
    };
    static_assert(sizeof(kPlantPreloadExtras) / sizeof(kPlantPreloadExtras[0]) ==
        (int)SeedType::NUM_SEED_TYPES, "kPlantPreloadExtras 槽位须与 SeedType 枚举同步");
    static_assert(kPlantPreloadExtras[(int)SeedType::SEED_CHERRYBOMB][0] == ReanimationType::REANIM_ZOMBIE_CHARRED &&
        kPlantPreloadExtras[(int)SeedType::SEED_JALAPENO][0] == ReanimationType::REANIM_JALAPENO_FIRE &&
        kPlantPreloadExtras[(int)SeedType::SEED_TORCHWOOD][0] == ReanimationType::REANIM_FIRE_PEA &&
        kPlantPreloadExtras[(int)SeedType::SEED_TORCHWOOD][1] == ReanimationType::REANIM_JALAPENO_FIRE &&
        kPlantPreloadExtras[(int)SeedType::SEED_PEASHOOTER][0] == ReanimationType::REANIM_NONE,
        "kPlantPreloadExtras 关键槽位锚点错位");

    const unsigned aSeedIndex = static_cast<unsigned>(theSeedType);
    if (aSeedIndex < (int)SeedType::NUM_SEED_TYPES)
    {
        for (ReanimationType anExtra : kPlantPreloadExtras[aSeedIndex])
        {
            if (anExtra != ReanimationType::REANIM_NONE)
            {
                ReanimatorEnsureDefinitionLoaded(anExtra, true);
            }
        }
    }

    // 表外特例：IsNocturnal 谓词支（菇类集合，非种子等值键，集合与表支不相交）
    if (Plant::IsNocturnal(theSeedType))
    {
        ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SLEEPING, true);
    }
}

//0x468280
void Plant::PlayIdleAnim(std::int64_t ownerNowNs, float theRate)
{
    Reanimation* aBodyReanim = mApp->ReanimationTryToGet(mBodyReanimID);

    if (aBodyReanim)
    {
        PlayBodyReanim(ownerNowNs, ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 20, theRate);
        if (mApp->IsIZombieLevel())
        {
            aBodyReanim->ApplyRate(0.0f);
        }
    }
}
