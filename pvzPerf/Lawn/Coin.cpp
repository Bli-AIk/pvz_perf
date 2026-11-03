#include "Coin.h"
#include "Sexy.TodLib/ReanimClock.h"
#include "Board.h"
#include "Cutscene.h"
#include "ZenGarden.h"
#include "Challenge.h"
#include "LawnApp.h"
#include "graphics/DDInterface.h"
#include "graphics/AtlasDraw.h"
#include "graphics/RenderInterface.h"
#include "SeedPacket.h"
#include "CursorObject.h"
#include "Resources.h"
#include "MessageWidget.h"
#include "GameConstants.h"
#include "System/PlayerInfo.h"
#include "Sexy.TodLib/TodFoley.h"
#include "Sexy.TodLib/TodDebug.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/Attachment.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Widget/AchievementsWidget.h"
#include <array>
#include <algorithm>
#include <cmath>

// 礼物植物候选表：各背景礼盒开出的盆栽种子（原版五连 else-if 各内联一张数组）。
// 未列背景（Boss/蘑菇园/温室/水族箱/树智慧）为 nullptr 行——走禅意花园随机
// （等价原 else）。改版加背景：枚举尾加值 + 此表加行（static_assert 锚尾）。
namespace
{
constexpr SeedType kPresentSeedsDay[] = {
    SeedType::SEED_PEASHOOTER, SeedType::SEED_SUNFLOWER, SeedType::SEED_CHERRYBOMB, SeedType::SEED_WALLNUT,
    SeedType::SEED_REPEATER, SeedType::SEED_POTATOMINE, SeedType::SEED_SNOWPEA, SeedType::SEED_CHOMPER };
constexpr SeedType kPresentSeedsNight[] = {
    SeedType::SEED_PUFFSHROOM, SeedType::SEED_SUNSHROOM, SeedType::SEED_FUMESHROOM, SeedType::SEED_GRAVEBUSTER,
    SeedType::SEED_HYPNOSHROOM, SeedType::SEED_SCAREDYSHROOM, SeedType::SEED_ICESHROOM, SeedType::SEED_DOOMSHROOM };
constexpr SeedType kPresentSeedsPool[] = {
    SeedType::SEED_LILYPAD, SeedType::SEED_SQUASH, SeedType::SEED_THREEPEATER, SeedType::SEED_TANGLEKELP,
    SeedType::SEED_JALAPENO, SeedType::SEED_SPIKEWEED, SeedType::SEED_TORCHWOOD, SeedType::SEED_TALLNUT };
constexpr SeedType kPresentSeedsFog[] = {
    SeedType::SEED_SEASHROOM, SeedType::SEED_PLANTERN, SeedType::SEED_CACTUS, SeedType::SEED_BLOVER,
    SeedType::SEED_SPLITPEA, SeedType::SEED_STARFRUIT, SeedType::SEED_PUMPKINSHELL, SeedType::SEED_MAGNETSHROOM };
constexpr SeedType kPresentSeedsRoof[] = {
    SeedType::SEED_CABBAGEPULT, SeedType::SEED_KERNELPULT, SeedType::SEED_INSTANT_COFFEE,
    SeedType::SEED_GARLIC, SeedType::SEED_UMBRELLA, SeedType::SEED_MELONPULT };

constexpr struct
{
    const SeedType* mSeedList;
    int mCount;
} kPresentSeeds[] = {
    /* BACKGROUND_1_DAY           */ { kPresentSeedsDay,   LENGTH(kPresentSeedsDay) },
    /* BACKGROUND_2_NIGHT         */ { kPresentSeedsNight, LENGTH(kPresentSeedsNight) },
    /* BACKGROUND_3_POOL          */ { kPresentSeedsPool,  LENGTH(kPresentSeedsPool) },
    /* BACKGROUND_4_FOG           */ { kPresentSeedsFog,   LENGTH(kPresentSeedsFog) },
    /* BACKGROUND_5_ROOF          */ { kPresentSeedsRoof,  LENGTH(kPresentSeedsRoof) },
    /* BACKGROUND_6_BOSS          */ { nullptr, 0 },
    /* BACKGROUND_MUSHROOM_GARDEN */ { nullptr, 0 },
    /* BACKGROUND_GREENHOUSE      */ { nullptr, 0 },
    /* BACKGROUND_ZOMBIQUARIUM    */ { nullptr, 0 },
    /* BACKGROUND_TREEOFWISDOM    */ { nullptr, 0 },
};
static_assert(sizeof(kPresentSeeds) / sizeof(kPresentSeeds[0]) == (int)BackgroundType::BACKGROUND_TREEOFWISDOM + 1,
    "礼物种子槽位须与 BackgroundType 枚举尾同步");
}  // namespace

Coin::Coin()
{
}

Coin::~Coin()
{
	AttachmentDie(mAttachmentID);
}


//0x42FF60
void Coin::CoinInitialize(int theX, int theY, CoinType theCoinType, CoinMotion theCoinMotion)
{
    // Coin::CoinInitialize 分发表：表内匿名 lambda = 原 case 体逐句迁移（生成器
    // + 逆变换 token 对拍 0 差异）；default 具名并被缺槽/越界复用。
    using CoinInitCaseFn = void (*)(Coin&);

    // 原 default（无 case 的 0 型与表外哨兵共用）
    static constexpr auto CoinInitDefault = [](Coin& c)
    {
    TOD_ASSERT(false);
    };

    // 匿名 lambda 函数指针表，行序 = CoinMotion 枚举值序（0..6）。
    static constexpr CoinInitCaseFn kCoinInitCases[(int)CoinMotion::COIN_MOTION_FROM_BOSS + 1] = {
    // 原 case COIN_MOTION_FROM_SKY
    [](Coin& c)
    {
    c.mVelY = 0.67f;
    c.mVelX = 0.0f;
    c.mGroundY = Rand(250) + 300;
    },  // 0 COIN_MOTION_FROM_SKY
    // 原 case COIN_MOTION_FROM_SKY_SLOW
    [](Coin& c)
    {
    c.mVelY = 0.33f;
    c.mVelX = 0.0f;
    c.mGroundY = Rand(250) + 300;
    },  // 1 COIN_MOTION_FROM_SKY_SLOW
    // 原 case COIN_MOTION_FROM_PLANT
    [](Coin& c)
    {
    c.mVelY = -1.7f - RandRangeFloat(0.0f, 1.7f);
    c.mVelX = -0.4f + RandRangeFloat(0.0f, 0.8f);
    c.mGroundY = c.mPosY + 15 + Rand(20);
    c.mScale = 0.4f;
    },  // 2 COIN_MOTION_FROM_PLANT
    // 原 case COIN_MOTION_COIN
    [](Coin& c)
    {
    c.mVelY = -3.0f - RandRangeFloat(0.0f, 2.0f);
    c.mVelX = -0.5f + RandRangeFloat(0.0f, 1.0f);
    c.mGroundY = c.mPosY + 45 + Rand(20);
    if (c.mGroundY > 521)
    {
    c.mGroundY = 521;
    }
    if (c.mGroundY < 80)
    {
    c.mGroundY = 80;
    }
    if (c.mType == CoinType::COIN_AWARD_SILVER_SUNFLOWER || c.mType == CoinType::COIN_AWARD_GOLD_SUNFLOWER)
    {
    c.mPosY -= 100.0f;
    c.mGroundY = c.mPosY + 45.0f;
    if (c.mGroundY > 400)
    {
    c.mGroundY = 400;
    }
    }
    if (c.mType == CoinType::COIN_FINAL_SEED_PACKET || 
    c.mType == CoinType::COIN_USABLE_SEED_PACKET || 
    c.mType == CoinType::COIN_TROPHY || 
    c.mType == CoinType::COIN_SHOVEL || 
    c.mType == CoinType::COIN_CARKEYS || 
    c.mType == CoinType::COIN_ALMANAC || 
    c.mType == CoinType::COIN_VASE || 
    c.mType == CoinType::COIN_WATERING_CAN || 
    c.mType == CoinType::COIN_TACO || 
    c.mType == CoinType::COIN_NOTE)
    {
    c.mGroundY -= 30;
    }
    },  // 3 COIN_MOTION_COIN
    // 原 case COIN_MOTION_LAWNMOWER_COIN
    [](Coin& c)
    {
    c.mVelY = 0.0f;
    c.mVelX = 0.0f;
    c.mGroundY = 600;
    c.Collect();
    },  // 4 COIN_MOTION_LAWNMOWER_COIN
    // 原 case COIN_MOTION_FROM_PRESENT
    [](Coin& c)
    {
    c.mVelY = 0.0f;
    c.mVelX = 0.0f;
    c.mGroundY = 600;
    c.mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 1);
    },  // 5 COIN_MOTION_FROM_PRESENT
    // 原 case COIN_MOTION_FROM_BOSS
    [](Coin& c)
    {
    c.mVelY = -5.0f;
    c.mVelX = -3.0f;
    c.mPosX = 750.0f;
    c.mPosY = c.mType == CoinType::COIN_AWARD_SILVER_SUNFLOWER ? 130.0f : 245.0f;
    c.mGroundY = c.mPosY + 40;
    },  // 6 COIN_MOTION_FROM_BOSS
    };
    static_assert(sizeof(kCoinInitCases) / sizeof(kCoinInitCases[0]) ==
    (int)CoinMotion::COIN_MOTION_FROM_BOSS + 1, "kCoinInitCases 槽位数须与 CoinMotion 枚举同步");

	mPosX = theX;
	mPosY = theY;
	mType = theCoinType;
	mCollectionDistance = 0.0f;
	mDead = false;
	mWidth = 60;
	mHeight = 60;
	mDisappearStartTimeNs = INT64_MAX;
	mIsBeingCollected = false;
	mFadeEndTimeNs = 0;
	mCoinMotion = theCoinMotion;
	mBirthTimeNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	mAttachmentID = AttachmentID::ATTACHMENTID_NULL;
	mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_COIN_BANK, 0, 1);
    mScale = 1.0f;
	mUsableSeedType = SeedType::SEED_NONE;
	mNeedsBouncyArrow = false;
	mHasBouncyArrow = false;
	mHitGround = false;
	mTimesDropped = 0;
	mPottedPlantSpec.InitializePottedPlant(SeedType::SEED_NONE);

    if (IsSun())
    {
        float aPosX = mWidth * 0.5f;
        float aPosY = mHeight * 0.5f;
        Reanimation* aSunReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_SUN, (mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? ReanimTimeDomain::Battle : ReanimTimeDomain::Visual));
        aSunReanim->mOverlayMatrix.m02 = mPosX + aPosX;
        aSunReanim->mOverlayMatrix.m12 = mPosY + aPosY;
        aSunReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
        aSunReanim->ApplyRate(6.0f);
        AttachReanim(mAttachmentID, aSunReanim, aPosX, aPosY);
    }
    else if (mType == CoinType::COIN_SILVER)
    {
        mPosX -= 10.0f;
        mPosY -= 8.0f;

        float aPosX = 9.0f;
        float aPosY = 9.0f;
        Reanimation* aCoinReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_COIN_SILVER, (mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? ReanimTimeDomain::Battle : ReanimTimeDomain::Visual));
        aCoinReanim->mOverlayMatrix.m02 = mPosX + aPosX;
        aCoinReanim->mOverlayMatrix.m12 = mPosY + aPosY;
        aCoinReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
        aCoinReanim->SeekRatio(RandRangeFloat(0.0f, 0.99f));
        aCoinReanim->ApplyRate(aCoinReanim->mAnimRate * RandRangeFloat(0.6f, 1.0f));
        AttachReanim(mAttachmentID, aCoinReanim, aPosX, aPosY);
    }
    else if (mType == CoinType::COIN_GOLD)
    {
        mPosX -= 10.0f;
        mPosY -= 8.0f;

        float aPosX = 9.0f;
        float aPosY = 9.0f;
        Reanimation* aCoinReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_COIN_GOLD, (mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? ReanimTimeDomain::Battle : ReanimTimeDomain::Visual));
        aCoinReanim->mOverlayMatrix.m02 = mPosX + aPosX;
        aCoinReanim->mOverlayMatrix.m12 = mPosY + aPosY;
        aCoinReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
        aCoinReanim->SeekRatio(RandRangeFloat(0.0f, 0.99f));
        aCoinReanim->ApplyRate(aCoinReanim->mAnimRate * RandRangeFloat(0.6f, 1.0f));
        AttachReanim(mAttachmentID, aCoinReanim, aPosX, aPosY);
    }
    else if (mType == CoinType::COIN_DIAMOND)
    {
        mPosX -= 15.0f;
        mPosY -= 15.0f;

        float aPosX = -3.0f;
        float aPosY = 4.0f;
        Reanimation* aCoinReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_DIAMOND, (mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? ReanimTimeDomain::Battle : ReanimTimeDomain::Visual));
        aCoinReanim->mOverlayMatrix.m02 = mPosX + aPosX;
        aCoinReanim->mOverlayMatrix.m12 = mPosY + aPosY;
        aCoinReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
        aCoinReanim->SeekRatio(RandRangeFloat(0.0f, 0.99f));
        aCoinReanim->ApplyRate(RandRangeFloat(50.0f, 80.0f));
        AttachReanim(mAttachmentID, aCoinReanim, aPosX, aPosY);
    }

    if (mApp->IsStormyNightLevel())
    {
        mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0);
    }

    // B 形态 -> 常量表 kCoinSizeRows（原 15 支链合并；行序 = CoinType 枚举序
    // 0..27，count_last=末值 PRESENT_SURVIVAL_MODE 锚定）。零尺寸槽 = 原链
    // 不匹配（无 default 体）。PRESENT_PLANT/AWARD_PRESENT 原链只取 W/H 不改
    // renderOrder（mSetOrder=false），礼物内容挑选特例保留在表外。
    static constexpr struct { int mWidth; int mHeight; RenderLayer mLayer; uint8_t mRow; uint8_t mLayerOffset; bool mSetOrder; } kCoinSizeRows[] = {
        {   0,   0, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, false },  // 0 COIN_NONE（原链不匹配）
        {   0,   0, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, false },  // 1 COIN_SILVER（原链不匹配）
        {   0,   0, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, false },  // 2 COIN_GOLD（原链不匹配）
        {   0,   0, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, false },  // 3 COIN_DIAMOND（原链不匹配）
        {   0,   0, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, false },  // 4 COIN_SUN（原链不匹配）
        {   0,   0, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, false },  // 5 COIN_SMALLSUN（原链不匹配）
        {   0,   0, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, false },  // 6 COIN_LARGESUN（原链不匹配）
        {  50,  70, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 7 COIN_FINAL_SEED_PACKET
        {  83,  63, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 8 COIN_TROPHY
        {  80,  80, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 9 COIN_SHOVEL
        {  80,  80, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 10 COIN_ALMANAC
        {  80,  80, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 11 COIN_CARKEYS
        {  80, 101, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 12 COIN_VASE
        {  80,  80, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 13 COIN_WATERING_CAN
        {  80,  80, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 14 COIN_TACO
        {  78,  52, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 15 COIN_NOTE
        {  50,  70, RenderLayer::RENDER_LAYER_FOG,      0, 2, true  },  // 16 COIN_USABLE_SEED_PACKET
        {  80,  80, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, false },  // 17 COIN_PRESENT_PLANT（不改 renderOrder）
        {  91,  78, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 18 COIN_AWARD_MONEY_BAG
        {  80,  80, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, false },  // 19 COIN_AWARD_PRESENT（不改 renderOrder）
        {  91,  78, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 20 COIN_AWARD_BAG_DIAMOND（共体）
        { 157, 269, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 21 COIN_AWARD_SILVER_SUNFLOWER
        { 157, 269, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 22 COIN_AWARD_GOLD_SUNFLOWER（共体）
        {  57,  66, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 23 COIN_CHOCOLATE
        {  57,  66, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 24 COIN_AWARD_CHOCOLATE（共体）
        {  80,  80, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 25 COIN_PRESENT_MINIGAMES
        {  80,  80, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 26 COIN_PRESENT_PUZZLE_MODE
        {  80,  80, RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0, true  },  // 27 COIN_PRESENT_SURVIVAL_MODE
    };
    static_assert(sizeof(kCoinSizeRows) / sizeof(kCoinSizeRows[0]) == (int)CoinType::COIN_PRESENT_SURVIVAL_MODE + 1,
        "kCoinSizeRows 槽位须与 CoinType 枚举同步");

    if ((unsigned)mType < sizeof(kCoinSizeRows) / sizeof(kCoinSizeRows[0]) && kCoinSizeRows[(int)mType].mWidth != 0)
    {
        const auto& aRow = kCoinSizeRows[(int)mType];
        mWidth = aRow.mWidth;
        mHeight = aRow.mHeight;
        if (aRow.mSetOrder)
        {
            mRenderOrder = Board::MakeRenderOrder(aRow.mLayer, aRow.mRow, aRow.mLayerOffset);
        }
    }

    // 表外特例：礼物内容挑选（原 COIN_PRESENT_PLANT/COIN_AWARD_PRESENT 分支体）
    if (mType == CoinType::COIN_PRESENT_PLANT || mType == CoinType::COIN_AWARD_PRESENT)
    {
        TOD_ASSERT(mBoard);

        if (mApp->IsSurvivalEndless(mApp->mGameMode) || mApp->IsEndlessIZombie(mApp->mGameMode) || mApp->IsEndlessScaryPotter(mApp->mGameMode))
        {
            SeedType aSeedType = mApp->mZenGarden->PickRandomSeedType();
            mPottedPlantSpec.InitializePottedPlant(aSeedType);
        }
        else
        {
            // 背景直下标礼物候选表；未列背景走禅意花园随机（等价原 else 链）
            const int aBackground = (int)mBoard->mBackground;
            if ((unsigned)aBackground < (unsigned)(sizeof(kPresentSeeds) / sizeof(kPresentSeeds[0])) &&
                kPresentSeeds[aBackground].mSeedList != nullptr)
            {
                mPottedPlantSpec.InitializePottedPlant(
                    TodPickFromList(kPresentSeeds[aBackground].mSeedList, kPresentSeeds[aBackground].mCount));
            }
            else
            {
                SeedType aSeedType = mApp->mZenGarden->PickRandomSeedType();
                mPottedPlantSpec.InitializePottedPlant(aSeedType);
            }
        }
    }

    // C 形态 -> 匿名 lambda 表：行序 = CoinMotion 枚举序（0..6，无 NUM
    // 哨兵=末值 FROM_BOSS 锚定）；default（TOD_ASSERT）具名并被缺槽/越界复用。
    const unsigned aMotionIndex = static_cast<unsigned>(mCoinMotion);
    if (aMotionIndex <= static_cast<unsigned>(CoinMotion::COIN_MOTION_FROM_BOSS))
    {
        kCoinInitCases[aMotionIndex](*this);
    }
    else
    {
        CoinInitDefault(*this);
    }


    float aScale;
    if (mType == CoinType::COIN_SMALLSUN)
    {
        aScale = 0.5f;
    }
    else if (mType == CoinType::COIN_LARGESUN)
    {
        aScale = 2.0f;
    }
    else
    {
        aScale = 1.0f;
    }
    mScale *= aScale;

    if (CoinGetsBouncyArrow())
    {
        mNeedsBouncyArrow = true;
    }

    if (mCoinMotion != CoinMotion::COIN_MOTION_FROM_PRESENT)
    {
        PlayLaunchSound();
    }
}

bool Coin::IsMoney(CoinType theType)
{
    // 这个函数原本是完全没有的，但是由于在 Board 中涉及到，并且感觉能在需要添加新钱币种类的时候提供方便，所以就留下来了
    return theType == CoinType::COIN_SILVER || theType == CoinType::COIN_GOLD || theType == CoinType::COIN_DIAMOND;
}

//0x430970
bool Coin::IsMoney()
{
    return IsMoney(mType);
}

//0x430990
bool Coin::IsSun()
{
    return mType == CoinType::COIN_SUN || mType == CoinType::COIN_SMALLSUN || mType == CoinType::COIN_LARGESUN;
}

//0x4309B0
bool Coin::IsPresentWithAdvice()
{
    return mType == CoinType::COIN_PRESENT_MINIGAMES || mType == CoinType::COIN_PRESENT_PUZZLE_MODE || mType == CoinType::COIN_PRESENT_SURVIVAL_MODE;
}

//0x4309D0
// GOTY @Patoke: 0x4336C0
void Coin::ScoreCoin()
{
    Die();

    if (IsSun())
    {
        int aSunValue = GetSunValue();
        mBoard->AddSunMoney(aSunValue);
    }
    else if (IsMoney())
    {
        int aCoinValue = Coin::GetCoinValue(mType);
        mApp->mPlayerInfo->AddCoins(aCoinValue);
        if (mBoard)
        {
            mBoard->mCoinsCollected += aCoinValue;

            // @Patoke: implemented
            if (mType == CoinType::COIN_SILVER || mType == CoinType::COIN_GOLD) {
                mBoard->mLevelCoinsCollected++;
                if (mBoard->mLevelCoinsCollected == 30 && mApp->mPlayerInfo->mCoins != 0)
                    ReportAchievement::GiveAchievement(mApp, PennyPincher, true);
            }
        }
    }

    if (mType == CoinType::COIN_DIAMOND && mBoard)
    {
        mBoard->mDiamondsCollected++;
    }
}

void Coin::UpdateFade()
{
    const auto nowNs=mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
    if (mApp->IsEndlessIZombie(mApp->mGameMode) || mApp->IsEndlessScaryPotter(mApp->mGameMode) || mType == CoinType::COIN_NOTE || !IsLevelAward())
    {
        if (nowNs >= mFadeEndTimeNs)
        {
            Die();
        }
    }
    else if(mFadeEndTimeNs!=0)
        mFadeEndTimeNs=ReanimClock::Add(mFadeEndTimeNs,static_cast<std::int64_t>(Sexy::gUpdateIntervalNs));
}

//0x430AC0
void Coin::UpdateFall()
{
    const auto nowNs=mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
    if (Sexy::gUpdateIntervalNs == 0) return;
    bool disappearActive=false;
    if (mCoinMotion == CoinMotion::COIN_MOTION_FROM_PRESENT)
    {
        mPosX += Sexy::UpdateDelta(mVelX);
        mPosY += Sexy::UpdateDelta(mVelY);
        const float decay = std::max(0.0f, 1.0f - Sexy::UpdateDelta(0.05f));
        mVelX *= decay;
        mVelY *= decay;
        if (ReanimClock::Subtract(nowNs,mBirthTimeNs) >= 800'000'000)
        {
            Collect();
        }
    }
    else if (mPosY + Sexy::UpdateDelta(mVelY) < mGroundY)
    {
        mPosY += Sexy::UpdateDelta(mVelY);
        if (mCoinMotion == CoinMotion::COIN_MOTION_FROM_PLANT)
        {
            mVelY += Sexy::UpdateDelta(0.09f);
        }
        else if (mCoinMotion == CoinMotion::COIN_MOTION_COIN || mCoinMotion == CoinMotion::COIN_MOTION_FROM_BOSS)
        {
            mVelY += Sexy::UpdateDelta(0.15f);
        }

        mPosX += Sexy::UpdateDelta(mVelX);
        if (mPosX > BOARD_WIDTH - mWidth && mCoinMotion != CoinMotion::COIN_MOTION_FROM_BOSS)
        {
            mPosX = BOARD_WIDTH - mWidth;
            mVelX = -0.4f - RandRangeFloat(0.0f, 0.4f);
        }
        else if (mPosX < 0.0f)
        {
            mPosX = 0.0f;
            mVelX = 0.4f + RandRangeFloat(0.0f, 0.4f);
        }
    }
    else
    {
        if (mNeedsBouncyArrow && !mHasBouncyArrow)
        {
            float aParticleOffsetX = mWidth / 2;
            float aParticleOffsetY = mHeight / 2 - 60;

            // B 形态 -> 常量表 kCoinArrowOffsets（原 5 支链合并；行序 = CoinType
            // 枚举序 0..27，count_last=末值 PRESENT_SURVIVAL_MODE 锚定）。IsMoney()/
            // IsPresentWithAdvice() 谓词槽位与字面 mType 槽位互不重叠，{0,0} 槽 =
            // 原链不匹配（不修正）。
            static constexpr struct { float mOffX; float mOffY; } kCoinArrowOffsets[] = {
                {  0.0f,   0.0f },  // 0 COIN_NONE（原链不匹配）
                { 12.0f,  21.0f },  // 1 COIN_SILVER（原 IsMoney 支）
                { 12.0f,  21.0f },  // 2 COIN_GOLD（原 IsMoney 支）
                { 12.0f,  21.0f },  // 3 COIN_DIAMOND（原 IsMoney 支）
                {  0.0f,   0.0f },  // 4 COIN_SUN（原链不匹配）
                {  0.0f,   0.0f },  // 5 COIN_SMALLSUN（原链不匹配）
                {  0.0f,   0.0f },  // 6 COIN_LARGESUN（原链不匹配）
                {  0.0f,   0.0f },  // 7 COIN_FINAL_SEED_PACKET（原链不匹配）
                {  2.0f,   0.0f },  // 8 COIN_TROPHY
                {  0.0f,   0.0f },  // 9 COIN_SHOVEL（原链不匹配）
                {  0.0f,   0.0f },  // 10 COIN_ALMANAC（原链不匹配）
                {  0.0f,   0.0f },  // 11 COIN_CARKEYS（原链不匹配）
                {  0.0f,   0.0f },  // 12 COIN_VASE（原链不匹配）
                {  0.0f,   0.0f },  // 13 COIN_WATERING_CAN（原链不匹配）
                {  0.0f,   0.0f },  // 14 COIN_TACO（原链不匹配）
                {  0.0f,   0.0f },  // 15 COIN_NOTE（原链不匹配）
                {  0.0f,   0.0f },  // 16 COIN_USABLE_SEED_PACKET（原链不匹配）
                {  0.0f,   0.0f },  // 17 COIN_PRESENT_PLANT（原链不匹配）
                {  2.0f,  -2.0f },  // 18 COIN_AWARD_MONEY_BAG
                {  0.0f, -20.0f },  // 19 COIN_AWARD_PRESENT
                {  2.0f,  -2.0f },  // 20 COIN_AWARD_BAG_DIAMOND（共体）
                { -6.0f, -40.0f },  // 21 COIN_AWARD_SILVER_SUNFLOWER
                { -6.0f, -40.0f },  // 22 COIN_AWARD_GOLD_SUNFLOWER（共体）
                {  0.0f,   0.0f },  // 23 COIN_CHOCOLATE（原链不匹配）
                {  0.0f,   0.0f },  // 24 COIN_AWARD_CHOCOLATE（原链不匹配）
                {  0.0f, -20.0f },  // 25 COIN_PRESENT_MINIGAMES（原 IsPresentWithAdvice 支）
                {  0.0f, -20.0f },  // 26 COIN_PRESENT_PUZZLE_MODE（共体）
                {  0.0f, -20.0f },  // 27 COIN_PRESENT_SURVIVAL_MODE（共体）
            };
            static_assert(sizeof(kCoinArrowOffsets) / sizeof(kCoinArrowOffsets[0]) == (int)CoinType::COIN_PRESENT_SURVIVAL_MODE + 1,
                "kCoinArrowOffsets 槽位须与 CoinType 枚举同步");

            if ((unsigned)mType < sizeof(kCoinArrowOffsets) / sizeof(kCoinArrowOffsets[0]))
            {
                aParticleOffsetX += kCoinArrowOffsets[(int)mType].mOffX;
                aParticleOffsetY += kCoinArrowOffsets[(int)mType].mOffY;
            }

            ParticleEffect aEffect;
            if (mType == CoinType::COIN_FINAL_SEED_PACKET)
            {
                aEffect = ParticleEffect::PARTICLE_SEED_PACKET;
            }
            else if (IsMoney())
            {
                aEffect = ParticleEffect::PARTICLE_COIN_PICKUP_ARROW;
            }
            else
            {
                aEffect = ParticleEffect::PARTICLE_AWARD_PICKUP_ARROW;
            }

            TodParticleSystem* aParticle = mApp->AddTodParticle(mPosX + aParticleOffsetX, mPosY + aParticleOffsetY, 0, aEffect, (mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
            AttachParticle(mAttachmentID, aParticle, aParticleOffsetX, aParticleOffsetY);
            mHasBouncyArrow = true;
        }

        if (!mHitGround)
        {
            mHitGround = true;
            PlayGroundSound();
        }

        mPosY = mGroundY;
        mPosX = FloatRoundToInt(mPosX);


        if (mApp->mGameMode != GameMode::GAMEMODE_MINIGAME_LAST_STAND || mBoard == nullptr || 
            mBoard->mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_LAST_STAND_ONSLAUGHT)
        {
            if (!IsLevelAward() && !IsPresentWithAdvice())
            {
                disappearActive=true;
                if(mDisappearStartTimeNs==INT64_MAX)
                    mDisappearStartTimeNs=ReanimClock::Subtract(nowNs,static_cast<std::int64_t>(Sexy::gUpdateIntervalNs));
                if (ReanimClock::Subtract(nowNs,mDisappearStartTimeNs) >= static_cast<std::int64_t>(GetDisappearTime())*10'000'000)
                {
                    mFadeEndTimeNs = ReanimClock::Add(nowNs,150'000'000);
                }
            }
        }
    }

    if (mCoinMotion == CoinMotion::COIN_MOTION_FROM_PLANT)
    {
        float aFinalScale = GetSunScale();
        if (mScale < aFinalScale)
        {
            mScale = std::min(aFinalScale, mScale + Sexy::UpdateDelta(0.02f));
        }
        else
        {
            mScale = aFinalScale;
        }
    }
    if(!disappearActive && mDisappearStartTimeNs!=INT64_MAX)
        mDisappearStartTimeNs=ReanimClock::Add(mDisappearStartTimeNs,static_cast<std::int64_t>(Sexy::gUpdateIntervalNs));

}

//0x430E40
// GOTY @Patoke: 0x433BD0
void Coin::UpdateCollected()
{
    const auto nowNs=mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
    if (Sexy::gUpdateIntervalNs == 0) return;
    const bool disappearActive=mType==CoinType::COIN_AWARD_PRESENT || mType==CoinType::COIN_PRESENT_PLANT ||
        mType==CoinType::COIN_USABLE_SEED_PACKET || IsLevelAward();
    if(!disappearActive && mDisappearStartTimeNs!=INT64_MAX)
        mDisappearStartTimeNs=ReanimClock::Add(mDisappearStartTimeNs,static_cast<std::int64_t>(Sexy::gUpdateIntervalNs));
    int aDestX, aDestY;
    if (IsSun())
    {
        aDestX = 15;
        aDestY = 0;
    }
    else if (IsMoney())
    {
        aDestX = 39;
        aDestY = 558;

        if (mApp->GetDialog((int)Dialogs::DIALOG_STORE))
        {
            aDestX = 662;
            aDestY = 546;
        }
        else if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN || mApp->mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_OFF)
        {
            aDestX = 442;
        }
    }
    else if (IsPresentWithAdvice())
    {
        aDestX = 35;
        aDestY = 487;
    }
    else if(mType == CoinType::COIN_AWARD_PRESENT || mType == CoinType::COIN_PRESENT_PLANT)
    {
        if(mDisappearStartTimeNs==INT64_MAX)
            mDisappearStartTimeNs=ReanimClock::Subtract(nowNs,static_cast<std::int64_t>(Sexy::gUpdateIntervalNs));
        if (ReanimClock::Subtract(nowNs,mDisappearStartTimeNs) >= 2'000'000'000)
        {
            mFadeEndTimeNs = ReanimClock::Add(nowNs,150'000'000);
        }

        return;
    }
    else if (!IsLevelAward())
    {
        if (mType == CoinType::COIN_USABLE_SEED_PACKET)
        {
            if(mDisappearStartTimeNs==INT64_MAX)
            mDisappearStartTimeNs=ReanimClock::Subtract(nowNs,static_cast<std::int64_t>(Sexy::gUpdateIntervalNs));
        }

        return;
    }
    else
    {
        aDestX = 400 - mWidth / 2;
        aDestY = 200 - mHeight / 2;
        if(mDisappearStartTimeNs==INT64_MAX)
            mDisappearStartTimeNs=ReanimClock::Subtract(nowNs,static_cast<std::int64_t>(Sexy::gUpdateIntervalNs));
    }

    if (IsLevelAward())
    {
        mScale = TodAnimateCurveNs(0, 4'000'000'000LL, ReanimClock::Subtract(nowNs,mDisappearStartTimeNs),
            1.01f, 2.0f, Curves::EaseInOut);
        mPosX = TodAnimateCurveNs(0, 3'500'000'000LL, ReanimClock::Subtract(nowNs,mDisappearStartTimeNs),
            mCollectX, static_cast<float>(aDestX), Curves::EaseOut);
        mPosY = TodAnimateCurveNs(0, 3'500'000'000LL, ReanimClock::Subtract(nowNs,mDisappearStartTimeNs),
            mCollectY, static_cast<float>(aDestY), Curves::EaseOut);
        return;
    }

    const float deltaX = static_cast<float>(aDestX) - mPosX;
    const float deltaY = static_cast<float>(aDestY) - mPosY;
    mCollectionDistance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
    mPosX += Sexy::UpdateDelta(deltaX / 21.0f);
    mPosY += Sexy::UpdateDelta(deltaY / 21.0f);

    if (IsPresentWithAdvice())
    {
        if (mCollectionDistance < 15.0f)
        {
            if (!mBoard->mHelpDisplayed[(int)AdviceType::ADVICE_UNLOCKED_MODE])
            {
                if (mType == CoinType::COIN_PRESENT_MINIGAMES)
                {
                    // 注：此处的 theMessageStyle 参数，原版中为 MESSAGE_STYLE_HINT_TALL_UNLOCKMESSAGE，内测版中为 MESSAGE_STYLE_HINT_TALL_8SECONDS
                    mBoard->DisplayAdvice(gStr.UNLOCKED_MINIGAMES, MessageStyle::MESSAGE_STYLE_HINT_TALL_UNLOCKMESSAGE, AdviceType::ADVICE_UNLOCKED_MODE);
                }
                else if (mType == CoinType::COIN_PRESENT_PUZZLE_MODE)
                {
                    mBoard->DisplayAdvice(gStr.UNLOCKED_PUZZLE_MODE, MessageStyle::MESSAGE_STYLE_HINT_TALL_UNLOCKMESSAGE, AdviceType::ADVICE_UNLOCKED_MODE);
                }
                else { // @Patoke: add case
                    mBoard->DisplayAdvice(gStr.UNLOCKED_SURVIVAL_MODE, MessageStyle::MESSAGE_STYLE_HINT_TALL_UNLOCKMESSAGE, AdviceType::ADVICE_UNLOCKED_MODE);
                }
            }
            else if (mBoard->mHelpIndex != AdviceType::ADVICE_UNLOCKED_MODE || !mBoard->mAdvice->IsBeingDisplayed())
            {
                Die();
            }
        }
    }
    else
    {
        float aScoringDistance = 8.0f;
        if (IsMoney())
        {
            aScoringDistance = 12.0f;
        }

        if (mCollectionDistance < aScoringDistance)
        {
            ScoreCoin();
        }

        mScale = ClampFloat(mCollectionDistance * 0.05f, 0.5f, 1.0f);
        mScale *= GetSunScale();
    }
}

//0x431500
void Coin::Update()
{
    const auto nowNs=mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
    if (mApp->mGameScene != GameScenes::SCENE_PLAYING && mApp->mGameScene != GameScenes::SCENE_AWARD && mBoard && !mBoard->mCutScene->ShouldRunUpsellBoard())
    {
        if(mFadeEndTimeNs!=0) mFadeEndTimeNs=ReanimClock::Add(mFadeEndTimeNs,static_cast<std::int64_t>(Sexy::gUpdateIntervalNs));
        if(mDisappearStartTimeNs!=INT64_MAX) mDisappearStartTimeNs=ReanimClock::Add(mDisappearStartTimeNs,static_cast<std::int64_t>(Sexy::gUpdateIntervalNs));
        return;
    }

    if (mFadeEndTimeNs != 0)
    {
        if(mDisappearStartTimeNs!=INT64_MAX) mDisappearStartTimeNs=ReanimClock::Add(mDisappearStartTimeNs,static_cast<std::int64_t>(Sexy::gUpdateIntervalNs));
        UpdateFade();
    }
    else if (!mIsBeingCollected)
    {
        UpdateFall();
    }
    else
    {
        UpdateCollected();
    }

    if (mAttachmentID != AttachmentID::ATTACHMENTID_NULL)
    {
        float aOffsetX = 0.0f;
        float aOffsetY = 0.0f;
        if (mType == CoinType::COIN_DIAMOND)
        {
            aOffsetX = 18.0f - 18.0f * mScale;
            aOffsetY = 13.0f - 13.0f * mScale;
        }

        AttachmentUpdateAndMoveAt(mAttachmentID, mPosX + aOffsetX, mPosY + aOffsetY,
            mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs);
        AttachmentOverrideColor(mAttachmentID, GetColor());
        AttachmentOverrideScale(mAttachmentID, mScale);

        if ((!mHitGround || mIsBeingCollected) && (mType == CoinType::COIN_SILVER || mType == CoinType::COIN_GOLD))
        {
            AttachmentOverrideColor(mAttachmentID, Color(0, 0, 0, 0));  // 运动中的金币和银币使用贴图，故以此法隐藏附件的动画
        }
    }
}

//0x4316F0
Color Coin::GetColor()
{
    const auto nowNs=mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
    if ((IsSun() || IsMoney()) && mIsBeingCollected)
    {
        float aAlpha = ClampFloat(mCollectionDistance * 0.035f, 0.35f, 1.0f) * 255.0f;
        return Color(255, 255, 255, aAlpha);
    }

    if (mFadeEndTimeNs != 0)
    {
        int aAlpha = TodAnimateCurveNs(150'000'000, 0, std::max<std::int64_t>(0,ReanimClock::Subtract(mFadeEndTimeNs,nowNs)), 255, 0, Curves::Linear);
        return Color(255, 255, 255, aAlpha);
    }

    return Color::White;
}

//0x4317D0
SeedType Coin::GetFinalSeedPacketType()
{
    if (mApp->IsFirstTimeAdventureMode() && mBoard && mBoard->mLevel <= 50)
    {
        return mApp->GetAwardSeedForLevel(mBoard->mLevel);
    }

    return SeedType::SEED_NONE;
}

//0x431810
void Coin::Draw(Graphics* g)
{
    const auto nowNs=mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
    struct RestoreTranslation
    {
        Graphics* mGraphics;
        float mTransX;
        ~RestoreTranslation() { mGraphics->mTransX = mTransX; }
    } aRestoreTranslation{g, g->mTransX};

    if (mIsBeingCollected && mApp->mScreenBounds.mX != 0 &&
        mApp->mDialogMap.find((int)Dialogs::DIALOG_STORE) == mApp->mDialogMap.end())
    {
        const bool aSun = IsSun();
        const bool aOrdinaryMoney = IsMoney() &&
            mApp->mGameMode != GameMode::GAMEMODE_LIMBO_ZEN_GARDEN &&
            mApp->mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_OFF;
        if (aSun || aOrdinaryMoney)
        {
            const float aFlightX = (aSun ? 15.0f : 39.0f) - mCollectX;
            const float aFlightY = (aSun ? 0.0f : 558.0f) - mCollectY;
            const float aFlightLengthSquared = aFlightX * aFlightX + aFlightY * aFlightY;
            // Project the simulated flight onto its original vector; shift only rendering.
            const float aProgress = aFlightLengthSquared > 0.0f ?
                ClampFloat(((mPosX - mCollectX) * aFlightX +
                    (mPosY - mCollectY) * aFlightY) / aFlightLengthSquared, 0.0f, 1.0f) : 1.0f;
            g->TranslateF(mApp->mScreenBounds.mX * aProgress, 0.0f);
        }
    }

    g->mColor = GetColor();
    if (mType == CoinType::COIN_DIAMOND)
    {
        g->mColorizeImages = true;
        g->DrawImage(IMAGE_AWARDPICKUPGLOW, mPosX - 56.0f, mPosY - 66.0f);
        g->mColorizeImages = false;
    }
    if (mType == CoinType::COIN_PRESENT_PLANT)
    {
        g->mColorizeImages = true;
        g->DrawImage(IMAGE_AWARDPICKUPGLOW, mPosX - 50.0f, mPosY - 64.0f);
        g->mColorizeImages = false;
    }
    if (mType == CoinType::COIN_AWARD_PRESENT && mIsBeingCollected)
    {
        g->mColorizeImages = true;
        g->DrawImage(IMAGE_AWARDPICKUPGLOW, mPosX - 50.0f, mPosY - 64.0f);
        g->mColorizeImages = false;
    }
    if (mType == CoinType::COIN_CHOCOLATE || mType == CoinType::COIN_AWARD_CHOCOLATE)
    {
        g->mColorizeImages = true;
        g->DrawImage(IMAGE_AWARDPICKUPGLOW, mPosX - 56.0f, mPosY - 50.0f);
        g->mColorizeImages = false;
    }

    if (mAttachmentID != AttachmentID::ATTACHMENTID_NULL)
    {
        Graphics theAttachmentGraphics(*g);
        MakeParentGraphicsFrame(&theAttachmentGraphics);
        AttachmentDraw(mAttachmentID, &theAttachmentGraphics, false);
    }

    if ((mType == CoinType::COIN_SILVER || mType == CoinType::COIN_GOLD) && mHitGround && !mIsBeingCollected)
    {
        return;
    }

    if (mType == CoinType::COIN_DIAMOND)
    {
        return;
    }

    if (IsLevelAward() && !mIsBeingCollected)
    {
        Color aFlashingColor = GetFlashingColorNs(ReanimClock::Subtract(nowNs,mBirthTimeNs),750'000'000);
        g->mColor = aFlashingColor;
    }

    if (mType == CoinType::COIN_SILVER || mType == CoinType::COIN_GOLD)
    {
        g->mColorizeImages = true;
        TodDrawImageCenterScaledF(g, IMAGE_REANIM_COINGLOW, mPosX - 14.0f, mPosY - 12.0f, mScale, mScale);
        g->mColorizeImages = false;
    }

    // 表外早退/特绘预检（原链 Sun/FinalSeedPacket/Present(收集态)/UsableSeedPacket
    // 四支自定义绘制路径，提前于表分派，语义与原链顺序等价——各判别值互斥）。
    if (IsSun())
    {
        return;
    }

    if (mType == CoinType::COIN_FINAL_SEED_PACKET)
    {
        SeedType aSeedType = GetFinalSeedPacketType();
        g->SetScale(mScale, mScale, 0.0f, 0.0f);
        DrawSeedPacket(g, 0.5f * (mWidth - mScale * mWidth) + mPosX, 0.5f * (mHeight - mScale * mHeight) + mPosY, aSeedType, SeedType::SEED_NONE, 0.0f, 255, true, false);
        g->SetScale(1.0f, 1.0f, 0.0f, 0.0f);
        return;
    }

    if ((mType == CoinType::COIN_PRESENT_PLANT || mType == CoinType::COIN_AWARD_PRESENT) && mIsBeingCollected)
    {
        mApp->mZenGarden->DrawPottedPlantIcon(static_cast<std::int64_t>(mBoard->mLevelTimeNs), g, mPosX + 10.0f, mPosY - 20.0f, &mPottedPlantSpec);
        return;
    }

    if (mType == CoinType::COIN_USABLE_SEED_PACKET)
    {
        int aGrayness = 255;
        if (!mIsBeingCollected)
        {
            int aDisappearTime = GetDisappearTime();
            if (mDisappearStartTimeNs!=INT64_MAX && ReanimClock::Subtract(nowNs,mDisappearStartTimeNs) > static_cast<std::int64_t>(aDisappearTime-300)*10'000'000 && ReanimClock::Subtract(nowNs,mDisappearStartTimeNs)%600'000'000 < 300'000'000)
            {
                aGrayness = 192;
            }
        }

        g->mColorizeImages = true;
        DrawSeedPacket(g, (int)mPosX, (int)mPosY, mUsableSeedType, SeedType::SEED_NONE, 0.0f, aGrayness, false, false);
        g->mColorizeImages = false;
        if (mIsBeingCollected)
            mBoard->DrawFieldCardSelection(g, *this);

        return;
    }

    // B 形态 -> 常量表 kCoinDrawRows（原 19 支链合并；行序 = CoinType 枚举序
    // 0..27，count_last=末值 PRESENT_SURVIVAL_MODE 锚定）。mCentered = 原链
    // "-= mWidth/2、-= mHeight/2"（整数除法保留在应用处）；nullptr 槽 = 表外
    // 预检（Sun 族/两种卡包）或原 default（NONE/DIAMOND 不在此绘制）。
    // Source PNG dimensions are resolved offline: Scary_Pot is 240x202 and
    // Sunflower_trophy is 314x269; other entries use their complete image.
    static constexpr struct { Image* const* mImage; TextureRegion mSource; float mOffX; float mOffY; float mScaleMul; bool mCentered; } kCoinDrawRows[] = {
        { nullptr,                         {},                           0.0f,   0.0f, 1.0f, false },  // 0 COIN_NONE
        { &IMAGE_REANIM_COIN_SILVER_DOLLAR, {0, 0, 1, 1, 43, 40},         8.0f,  10.0f, 1.0f, false },  // 1 COIN_SILVER
        { &IMAGE_REANIM_COIN_GOLD_DOLLAR,   {0, 0, 1, 1, 45, 45},         8.0f,  10.0f, 1.0f, false },  // 2 COIN_GOLD
        { nullptr,                         {},                           0.0f,   0.0f, 1.0f, false },  // 3 COIN_DIAMOND
        { nullptr,                         {},                           0.0f,   0.0f, 1.0f, false },  // 4 COIN_SUN
        { nullptr,                         {},                           0.0f,   0.0f, 1.0f, false },  // 5 COIN_SMALLSUN
        { nullptr,                         {},                           0.0f,   0.0f, 1.0f, false },  // 6 COIN_LARGESUN
        { nullptr,                         {},                           0.0f,   0.0f, 1.0f, false },  // 7 COIN_FINAL_SEED_PACKET
        { &IMAGE_TROPHY_HI_RES,             {0, 0, 1, 1, 166, 136},       0.0f,   0.0f, 0.5f, true  },  // 8 COIN_TROPHY
        { &IMAGE_SHOVEL_HI_RES,             {0, 0, 1, 1, 116, 125},     -20.0f, -20.0f, 0.5f, false },  // 9 COIN_SHOVEL
        { &IMAGE_ALMANAC,                   {0, 0, 1, 1, 80, 80},         0.0f,   0.0f, 1.0f, false },  // 10 COIN_ALMANAC
        { &IMAGE_CARKEYS,                   {0, 0, 1, 1, 80, 80},         0.0f,   0.0f, 1.0f, false },  // 11 COIN_CARKEYS
        { &IMAGE_SCARY_POT,                 {0, 0, 1.0f/3.0f, .5f, 80, 101}, 0.0f, 0.0f, 1.0f, false },  // 12 COIN_VASE
        { &IMAGE_WATERINGCAN,               {0, 0, 1, 1, 80, 80},         0.0f,   0.0f, 1.0f, false },  // 13 COIN_WATERING_CAN
        { &IMAGE_TACO,                      {0, 0, 1, 1, 80, 80},         0.0f,   0.0f, 1.0f, false },  // 14 COIN_TACO
        { &IMAGE_ZOMBIE_NOTE_SMALL,         {0, 0, 1, 1, 78, 52},         0.0f,   0.0f, 1.0f, false },  // 15 COIN_NOTE
        { nullptr,                         {},                           0.0f,   0.0f, 1.0f, false },  // 16 COIN_USABLE_SEED_PACKET
        { &IMAGE_PRESENT,                   {0, 0, 1, 1, 80, 80},         0.0f, -20.0f, 1.0f, false },  // 17 COIN_PRESENT_PLANT
        { &IMAGE_MONEYBAG_HI_RES,           {0, 0, 1, 1, 182, 171},       0.0f,   0.0f, 0.5f, true  },  // 18 COIN_AWARD_MONEY_BAG
        { &IMAGE_PRESENT,                   {0, 0, 1, 1, 80, 80},         0.0f, -20.0f, 1.0f, false },  // 19 COIN_AWARD_PRESENT
        { &IMAGE_MONEYBAG_HI_RES,           {0, 0, 1, 1, 182, 171},       0.0f,   0.0f, 0.5f, true  },  // 20 COIN_AWARD_BAG_DIAMOND
        { &IMAGE_SUNFLOWER_TROPHY,          {0, 0, .5f, 1, 157, 269},    -5.0f,   0.0f, 0.6f, false },  // 21 COIN_AWARD_SILVER_SUNFLOWER
        { &IMAGE_SUNFLOWER_TROPHY,          {.5f, 0, 1, 1, 157, 269},    -5.0f,   0.0f, 0.6f, false },  // 22 COIN_AWARD_GOLD_SUNFLOWER
        { &IMAGE_CHOCOLATE,                 {0, 0, 1, 1, 57, 66},         0.0f,   0.0f, 1.0f, false },  // 23 COIN_CHOCOLATE
        { &IMAGE_CHOCOLATE,                 {0, 0, 1, 1, 57, 66},         0.0f,   0.0f, 1.0f, false },  // 24 COIN_AWARD_CHOCOLATE
        { &IMAGE_PRESENT,                   {0, 0, 1, 1, 80, 80},         0.0f, -20.0f, 1.0f, false },  // 25 COIN_PRESENT_MINIGAMES
        { &IMAGE_PRESENT,                   {0, 0, 1, 1, 80, 80},         0.0f, -20.0f, 1.0f, false },  // 26 COIN_PRESENT_PUZZLE_MODE
        { &IMAGE_PRESENT,                   {0, 0, 1, 1, 80, 80},         0.0f, -20.0f, 1.0f, false },  // 27 COIN_PRESENT_SURVIVAL_MODE
    };
    static_assert(sizeof(kCoinDrawRows) / sizeof(kCoinDrawRows[0]) == (int)CoinType::COIN_PRESENT_SURVIVAL_MODE + 1,
        "kCoinDrawRows 槽位须与 CoinType 枚举同步");

    Image* aImage = nullptr;
    TextureRegion aSourceRegion;
    float aDrawScale = mScale;
    float aOffsetX = 0.0f;
    float aOffsetY = 0.0f;
    if ((unsigned)mType < sizeof(kCoinDrawRows) / sizeof(kCoinDrawRows[0]) && kCoinDrawRows[(int)mType].mImage != nullptr)
    {
        const auto& aRow = kCoinDrawRows[(int)mType];
        aImage = *aRow.mImage;
        aSourceRegion = aRow.mSource;
        aDrawScale = mScale * aRow.mScaleMul;
        aOffsetX = aRow.mOffX;
        aOffsetY = aRow.mOffY;
        if (aRow.mCentered)
        {
            aOffsetX -= mWidth / 2;
            aOffsetY -= mHeight / 2;
        }
    }
    else
    {
        TOD_ASSERT(false);  // 原 else 兜底
    }

    // 建议礼盒收集态：开盖图 + 平移（原链 IsPresentWithAdvice 分支内嵌值）
    if (IsPresentWithAdvice() && mIsBeingCollected)
    {
        aImage = IMAGE_PRESENTOPEN;
        aSourceRegion = {0, 0, 1, 1, 100, 100};
        aOffsetX = -10.0f;
        aOffsetY = -10.0f;
    }

    g->mColorizeImages = true;
    if (!g->mDestImage || !g->mDestImage->IsGpuDrawTarget())
        throw NonScreenDrawError(DrawOperation::BltMatrix);
    auto& render = *g->mDestImage->mRenderOwner;
    const TextureSource texture{render.CreateImageTexture(aImage), aImage->mGpuSamplingFlags};
    const float aDrawX = mPosX + aOffsetX;
    const float aDrawY = mPosY + aOffsetY;
    if (aDrawScale == 1.0f)
        DrawAtlasImageScaled(g, texture, aSourceRegion, aDrawX, aDrawY, 1.0f, 1.0f);
    else
    {
        SexyTransform2D transform;
        transform.Scale(aDrawScale, aDrawScale);
        transform.m02 = aSourceRegion.mWidth * 0.5f + aDrawX + g->mTransX;
        transform.m12 = aSourceRegion.mHeight * 0.5f + aDrawY + g->mTransY;
        TodBltMatrix(g, texture, transform, g->mClipRect, g->mColor, g->mDrawMode, aSourceRegion);
    }
    g->mColorizeImages = false;
}

//0x431F30
void Coin::FanOutCoins(CoinType theCoinType, int theNumCoins)
{
    TOD_ASSERT(mBoard);

    for (int i = 0; i < theNumCoins; i++)
    {
        float aAngle = PI / 2 + PI * (i + 1) / (theNumCoins + 1);
        float aPosX = mPosX + 20.0f;
        float aPosY = mPosY;
        Coin* aCoin = mBoard->AddCoin((int)aPosX, (int)aPosY, theCoinType, CoinMotion::COIN_MOTION_FROM_PRESENT);
        aCoin->mVelX = 5.0f * sin(aAngle);
        aCoin->mVelY = 5.0f * cos(aAngle);
    }
}

//0x432000
void Coin::TryAutoCollectAfterLevelAward()
{
    bool aCanBeAutoCollected = false;
    if (IsMoney() && mCoinMotion != CoinMotion::COIN_MOTION_FROM_PRESENT)
    {
        aCanBeAutoCollected = true;
    }
    if (IsSun())
    {
        aCanBeAutoCollected = true;
    }
    if (mType == CoinType::COIN_PRESENT_PLANT || mType == CoinType::COIN_CHOCOLATE || IsPresentWithAdvice())
    {
        aCanBeAutoCollected = true;
    }

    if (aCanBeAutoCollected)
    {
        PlayCollectSound();
        Collect();
    }
}

//0x432060
// GOTY @Patoke: 0x434DC0
void Coin::Collect()
{
    const auto nowNs=mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
    if (mDead)
        return;

    mCollectX = mPosX;
    mCollectY = mPosY;
    mIsBeingCollected = true;

    bool aIsEndlessAward = false;
    if ((mApp->IsEndlessIZombie(mApp->mGameMode) || mApp->IsEndlessScaryPotter(mApp->mGameMode)) && IsLevelAward())
    {
        aIsEndlessAward = true;
    }

    if (mType == CoinType::COIN_PRESENT_PLANT || mType == CoinType::COIN_AWARD_PRESENT)
    {
        TOD_ASSERT(mBoard);

        if (mApp->mZenGarden->IsZenGardenFull(false))
        {
            mBoard->DisplayAdvice(gStr.DIALOG_ZEN_GARDEN_FULL, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_NONE);
        }
        else
        {
            mBoard->mPottedPlantsCollected++;
            mBoard->DisplayAdvice(gStr.ADVICE_FOUND_PLANT, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_NONE);
            mApp->AddTodParticle(mPosX + 30.0f, mPosY + 30.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_PRESENT_PICKUP, (mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
            mApp->mZenGarden->AddPottedPlant(&mPottedPlantSpec);
        }

        mDisappearStartTimeNs = INT64_MAX;
        mFadeEndTimeNs = 0;
        if (aIsEndlessAward)
        {
            AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_AWARD_PICKUP_ARROW);
            mBoard->FadeOutLevel();
        }

        return;
    }

    if (mType == CoinType::COIN_PRESENT_MINIGAMES)
    {
        TOD_ASSERT(mBoard);

        mApp->AddTodParticle(mPosX + 30.0f, mPosY + 30.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_PRESENT_PICKUP, (mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));

        mDisappearStartTimeNs = INT64_MAX;
        mFadeEndTimeNs = 0;

        AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_AWARD_PICKUP_ARROW);
        mApp->mPlayerInfo->mHasUnlockedMinigames = 1;

        return;
    }
    if (mType == CoinType::COIN_PRESENT_PUZZLE_MODE)
    {
        TOD_ASSERT(mBoard);

        mApp->AddTodParticle(mPosX + 30.0f, mPosY + 30.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_PRESENT_PICKUP, (mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));

        mDisappearStartTimeNs = INT64_MAX;
        mFadeEndTimeNs = 0;

        AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_AWARD_PICKUP_ARROW);
        mApp->mPlayerInfo->mHasUnlockedPuzzleMode = 1;

        return;
    }
    if (mType == CoinType::COIN_PRESENT_SURVIVAL_MODE)
    {
        TOD_ASSERT(mBoard);

        mApp->AddTodParticle(mPosX + 30.0f, mPosY + 30.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_PRESENT_PICKUP, (mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));

        mDisappearStartTimeNs = INT64_MAX;
        mFadeEndTimeNs = 0;

        AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_AWARD_PICKUP_ARROW);
        mApp->mPlayerInfo->mHasUnlockedSurvivalMode = 1;

        return;
    }

    if (mType == CoinType::COIN_CHOCOLATE || mType == CoinType::COIN_AWARD_CHOCOLATE)
    {
        TOD_ASSERT(mBoard);

        mBoard->mChocolateCollected++;
        mApp->AddTodParticle(mPosX + 30.0f, mPosY + 30.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_PRESENT_PICKUP, (mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));

        if (mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_CHOCOLATE] < PURCHASE_COUNT_OFFSET)
        {
            mBoard->DisplayAdvice(gStr.ADVICE_FOUND_CHOCOLATE, MessageStyle::MESSAGE_STYLE_HINT_TALL_FAST, AdviceType::ADVICE_NONE);
            mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_CHOCOLATE] = PURCHASE_COUNT_OFFSET + 1;
        }
        else
        {
            mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_CHOCOLATE]++;
        }

        mDisappearStartTimeNs = INT64_MAX;
        mFadeEndTimeNs = ReanimClock::Add(nowNs,150'000'000);

        if (aIsEndlessAward)
        {
            AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_AWARD_PICKUP_ARROW);
            mBoard->FadeOutLevel();
        }

        return;
    }

    if (IsLevelAward())
    {
        if (aIsEndlessAward)
        {
            if (mType == CoinType::COIN_AWARD_BAG_DIAMOND)
            {
                mApp->PlaySample(SOUND_DIAMOND);
                FanOutCoins(CoinType::COIN_DIAMOND, 1);
                mFadeEndTimeNs = ReanimClock::Add(nowNs,150'000'000);
            }
            else if (mType == CoinType::COIN_AWARD_MONEY_BAG)
            {
                mApp->PlayFoley(FoleyType::FOLEY_COIN);
                FanOutCoins(CoinType::COIN_GOLD, 5);
                mFadeEndTimeNs = ReanimClock::Add(nowNs,150'000'000);
            }
        }
        else if (mApp->IsScaryPotterLevel())
        {
            if (mType == CoinType::COIN_TROPHY)
            {
                mApp->PlayFoley(FoleyType::FOLEY_COIN);
                FanOutCoins(CoinType::COIN_GOLD, 5);
            }
            else if (mType == CoinType::COIN_AWARD_MONEY_BAG)
            {
                // 1073 00435046: ordinary vase bags contain two gold coins.
                // The separate endless-award branch above still contains five.
                mApp->PlayFoley(FoleyType::FOLEY_COIN);
                FanOutCoins(CoinType::COIN_GOLD, 2);
            }
        }
        else if (mApp->IsAdventureMode() && mBoard->mLevel == 50)
        {
            FanOutCoins(CoinType::COIN_DIAMOND, 3);
        }
        else if (mType == CoinType::COIN_AWARD_GOLD_SUNFLOWER)
        {
            FanOutCoins(CoinType::COIN_DIAMOND, 5);
        }
        else if (mApp->IsFirstTimeAdventureMode() && mBoard->mLevel == 4)
        {
            mApp->PlaySample(SOUND_SHOVEL);
        }
        else if (mApp->IsFirstTimeAdventureMode() && (mBoard->mLevel == 24 || mBoard->mLevel == 34 || mBoard->mLevel == 44))
        {
            mApp->PlaySample(SOUND_TAP2);
        }
        else if (mType == CoinType::COIN_TROPHY)
        {
            mApp->PlaySample(SOUND_DIAMOND);
            FanOutCoins(CoinType::COIN_DIAMOND, 1);
        }
        else if (mType == CoinType::COIN_AWARD_MONEY_BAG)
        {
            mApp->PlayFoley(FoleyType::FOLEY_COIN);
            FanOutCoins(CoinType::COIN_GOLD, 5);
        }
        else
        {
            mApp->PlaySample(SOUND_SEEDLIFT);
            mApp->PlaySample(SOUND_TAP2);
        }

        mApp->AddTodParticle(mPosX + 30.0f, mPosY + 30.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_STARBURST, (mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));

        TOD_ASSERT(mBoard);

        mBoard->FadeOutLevel();
        AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_SEED_PACKET);
        AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_AWARD_PICKUP_ARROW);
        AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_COIN_PICKUP_ARROW);

        if (mType == CoinType::COIN_NOTE)
        {
            mApp->AddTodParticle(mPosX + 30.0f, mPosY + 30.0f, mRenderOrder + 1, ParticleEffect::PARTICLE_PRESENT_PICKUP, (mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
            mFadeEndTimeNs = ReanimClock::Add(nowNs,150'000'000);
        }
        else if (!aIsEndlessAward)
        {
            float aParticleOffsetX = mWidth / 2;
            float aParticleOffsetY = mHeight / 2;
            TodParticleSystem* aParticle = mApp->AddTodParticle(mPosX + aParticleOffsetX, mPosY + aParticleOffsetY, mRenderOrder - 1, PARTICLE_SEED_PACKET_PICKUP, (mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
            AttachParticle(mAttachmentID, aParticle, aParticleOffsetX, aParticleOffsetY);
        }

        mDisappearStartTimeNs = INT64_MAX;
        return;
    }

    if (mType == CoinType::COIN_USABLE_SEED_PACKET)
    {
        TOD_ASSERT(mBoard);

        mBoard->mCursorObject->mType = mUsableSeedType;
        mBoard->mCursorObject->mCursorType = CursorType::CURSOR_TYPE_PLANT_FROM_USABLE_COIN;
        mBoard->mCursorObject->mCoinID = (CoinID)mBoard->mCoins.DataArrayGetID(this);

        mGroundY = (int)mPosY;
        mFadeEndTimeNs = 0;
        return;
    }

    if (IsMoney() && mBoard)
    {
        mBoard->mCoinBankFadeCount = 1000;
    }

    // Collection stops ground-disappearance counting; movement uses current coordinates.
    mDisappearStartTimeNs = INT64_MAX;
    mFadeEndTimeNs = 0;

    if (IsSun() && mBoard && !mBoard->HasConveyorBeltSeedBank())
    {
        for (int i = 0; i < mBoard->mSeedBank->mNumPackets; i++)
        {
            SeedPacket* aSeedPacket = &mBoard->mSeedBank->mSeedPackets[i];
            int aCost = mBoard->GetCurrentPlantCost(aSeedPacket->mPacketType, aSeedPacket->mImitaterType);
            int aSunProfit = mBoard->mSunMoney + mBoard->CountSunBeingCollected() - aCost;
            if (aSunProfit >= 0 && aSunProfit < GetSunValue())
            {
                aSeedPacket->FlashIfReady();
            }
        }

        if (mBoard->StageHasFog())
        {
            mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 2);
        }
    }

    AttachmentDetachCrossFadeParticleType(mAttachmentID, ParticleEffect::PARTICLE_COIN_PICKUP_ARROW);
    if (mApp->IsFirstTimeAdventureMode() && mBoard && mBoard->mLevel == 11 && (mType == CoinType::COIN_GOLD || mType == CoinType::COIN_SILVER))
    {
        mBoard->DisplayAdvice(gStr.ADVICE_CLICKED_ON_COIN, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_CLICKED_ON_COIN);
    }
}

//0x4329D0

//0x432A00
void Coin::PlayLaunchSound()
{
    if (mType == CoinType::COIN_DIAMOND || 
        mType == CoinType::COIN_CHOCOLATE || 
        mType == CoinType::COIN_AWARD_CHOCOLATE || 
        mType == CoinType::COIN_PRESENT_PLANT ||
        mType == CoinType::COIN_AWARD_PRESENT || 
        IsPresentWithAdvice())
    {
        mApp->PlayFoley(FoleyType::FOLEY_CHIME);
    }
}

//0x432A90
void Coin::PlayGroundSound()
{
    if (mType == CoinType::COIN_GOLD)
    {
        mApp->PlayFoley(FoleyType::FOLEY_MONEYFALLS);
        return;
    }
}

// GOTY @Patoke: 0x435850
void Coin::PlayCollectSound()
{
    if (mType == CoinType::COIN_USABLE_SEED_PACKET)
    {
        mApp->PlaySample(SOUND_SEEDLIFT);
        return;
    }

    if (mType == CoinType::COIN_SILVER || mType == CoinType::COIN_GOLD)
    {
        mApp->PlayFoley(FoleyType::FOLEY_COIN);
        return;
    }

    if (mType == CoinType::COIN_DIAMOND)
    {
        mApp->PlaySample(SOUND_DIAMOND);
        return;
    }

    if (IsSun())
    {
        mApp->PlayFoley(FoleyType::FOLEY_SUN);
        return;
    }

    if (mType == CoinType::COIN_CHOCOLATE || 
        mType == CoinType::COIN_PRESENT_PLANT || 
        IsPresentWithAdvice() || 
        mType == CoinType::COIN_AWARD_PRESENT || 
        mType == CoinType::COIN_AWARD_CHOCOLATE)
    {
        mApp->PlayFoley(FoleyType::FOLEY_PRIZE);
        return;
    }

    if (IsSun())
    {
        mApp->PlayFoley(FoleyType::FOLEY_SUN);
    }
}

void Coin::DroppedUsableSeed()
{
    const auto nowNs=mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
    mIsBeingCollected = false;

    if (mTimesDropped == 0)
    {
        if(mDisappearStartTimeNs!=INT64_MAX)
            mDisappearStartTimeNs=std::max(mDisappearStartTimeNs,ReanimClock::Subtract(nowNs,12'000'000'000));
    }

    mTimesDropped++;
}

//0x432C00
void Coin::MouseDown(int x, int y, int theClickCount)
{
    (void)x;(void)y;
    if (mBoard == nullptr || mBoard->mPaused || mApp->mGameScene != GameScenes::SCENE_PLAYING || mDead)
    {
        return;
    }

    if (theClickCount >= 0 && !mIsBeingCollected)
    {
        PlayCollectSound();
        Collect();

        if (mApp->IsFirstTimeAdventureMode() && mBoard->mLevel == 1)
        {
            mBoard->DisplayAdvice(gStr.ADVICE_CLICKED_ON_SUN, MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY, AdviceType::ADVICE_CLICKED_ON_SUN);
        }
    }
}

//0x432DD0
// GOTY @Patoke: 0x435B20
void Coin::Die()
{
    TOD_ASSERT(!mBoard || mBoard->mCursorObject->mCoinID != (CoinID)mBoard->mCoins.DataArrayGetID(this));

    mDead = true;
    AttachmentDie(mAttachmentID);
}

namespace {
struct CoinHitBounds {
    float left, top, right, bottom;
};

CoinHitBounds HitBounds(const Coin& coin)
{
    const auto type = coin.mType;
    const int offsetY = type == COIN_PRESENT_PLANT || type == COIN_AWARD_PRESENT ||
        type == COIN_PRESENT_MINIGAMES || type == COIN_PRESENT_PUZZLE_MODE ||
        type == COIN_PRESENT_SURVIVAL_MODE ? -20 : 0;
    const bool whack = coin.mApp->IsWhackAZombieLevel();
    const int margin = whack || type == COIN_SUN ? 15 : 0;
    return {coin.mPosX - margin, coin.mPosY + offsetY - margin,
        coin.mPosX + coin.mWidth + margin,
        coin.mPosY + coin.mHeight + offsetY + margin + (whack ? 30 : 0)};
}
}

//0x432E20
bool Coin::MouseHitTest(int theX, int theY, HitResult* theHitResult)
{
    const auto bounds = HitBounds(*this);

    bool aCanHitCoin = true;
    if (mDead || mIsBeingCollected)
    {
        aCanHitCoin = false;
    }
    if (mType == CoinType::COIN_USABLE_SEED_PACKET && mBoard)
    {
        if (mBoard->mCursorObject->mCursorType != CursorType::CURSOR_TYPE_NORMAL && !mApp->IsWhackAZombieLevel())
        {
            aCanHitCoin = false;
        }
    }

    if (aCanHitCoin &&
        theX >= bounds.left && theX < bounds.right &&
        theY >= bounds.top && theY < bounds.bottom)
    {
        theHitResult->mObject = this;
        theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_COIN;
        return true;
    }

    theHitResult->mObject = nullptr;
    theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_NONE;
    return false;
}

bool Coin::TouchHitTest(int fromX, int fromY, int toX, int toY)
{
    if (mDead || mIsBeingCollected || (!IsSun() && !IsMoney() && !IsLevelAward())) return false;
    const auto bounds = HitBounds(*this);
    if (std::max(fromX, toX) < bounds.left || std::min(fromX, toX) >= bounds.right ||
        std::max(fromY, toY) < bounds.top || std::min(fromY, toY) >= bounds.bottom)
        return false;

    // Clip the reported movement segment, so a fast drag can collect an item
    // even when both sampled endpoints lie outside it. Preserve half-open edges.
    float enter = 0, leave = 1;
    const auto clip = [&](float origin, float delta, float low, float high) {
        if (delta == 0) return origin >= low && origin < high;
        float first = (low - origin) / delta, last = (high - origin) / delta;
        if (first > last) std::swap(first, last);
        enter = std::max(enter, first);
        leave = std::min(leave, last);
        return enter <= leave;
    };
    const float dx = float(toX) - float(fromX), dy = float(toY) - float(fromY);
    if (!clip(float(fromX), dx, bounds.left, bounds.right) ||
        !clip(float(fromY), dy, bounds.top, bounds.bottom)) return false;
    const float middle = (enter + leave) * 0.5f;
    const float x = float(fromX) + dx * middle, y = float(fromY) + dy * middle;
    return x >= bounds.left && x < bounds.right && y >= bounds.top && y < bounds.bottom;
}

//0x432F80
bool Coin::IsLevelAward()
{
    return 
        mType == CoinType::COIN_FINAL_SEED_PACKET || 
        mType == CoinType::COIN_TROPHY || 
        mType == CoinType::COIN_AWARD_SILVER_SUNFLOWER || 
        mType == CoinType::COIN_AWARD_GOLD_SUNFLOWER || 
        mType == CoinType::COIN_SHOVEL || 
        mType == CoinType::COIN_CARKEYS || 
        mType == CoinType::COIN_ALMANAC || 
        mType == CoinType::COIN_VASE || 
        mType == CoinType::COIN_WATERING_CAN || 
        mType == CoinType::COIN_TACO || 
        mType == CoinType::COIN_NOTE || 
        mType == CoinType::COIN_AWARD_MONEY_BAG || 
        mType == CoinType::COIN_AWARD_BAG_DIAMOND || 
        mType == CoinType::COIN_AWARD_PRESENT || 
        mType == CoinType::COIN_AWARD_CHOCOLATE;
}

//0x432FE0
bool Coin::CoinGetsBouncyArrow()
{
    if (IsLevelAward())
        return true;

    if (mType == CoinType::COIN_SILVER || mType == CoinType::COIN_GOLD)
    {
        if (mApp->IsFirstTimeAdventureMode() && mBoard && mBoard->mLevel == 11 && !mBoard->mDroppedFirstCoin)
        {
            return true;
        }
    }

    return IsPresentWithAdvice();
}

//0x433050
int Coin::GetDisappearTime()
{
    int aTime = 750;

    if (mType == CoinType::COIN_DIAMOND || mType == CoinType::COIN_PRESENT_PLANT || mType == CoinType::COIN_CHOCOLATE || mHasBouncyArrow)
    {
        aTime = 1500;
    }

    if ((mApp->IsScaryPotterLevel() || mApp->IsSlotMachineLevel()) && mType == CoinType::COIN_USABLE_SEED_PACKET)
    {
        aTime = 1500;
    }

    if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
    {
        aTime = 6000;
    }

    return aTime;
}
