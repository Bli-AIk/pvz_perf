#include "Sexy.TodLib/generated/EntityStrings.h"
#include "Sexy.TodLib/generated/ReanimVariantOptions.h"
#include <time.h>
#include "platform/CalendarTime.h"
#include <iterator>
#include <stdexcept>
#include <cmath>
#include "Plant.h"
#include "Motion.h"
#include "Board.h"
#include "Challenge.h"
#include "ZenGarden.h"
#include "LawnApp.h"
#include "CursorObject.h"
#include "System/Music.h"
#include "Widget/GameButton.h"
#include "System/PlayerInfo.h"
#include "System/StinkyTimestamp.h"
#include "Widget/LawnDialog.h"
#include "Widget/StoreScreen.h"
#include "System/ReanimationLawn.h"
#include "Sexy.TodLib/TodFoley.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/Attachment.h"
#include "Sexy.TodLib/TodParticle.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "graphics/Graphics.h"
#include "Sexy.TodLib/TodStringFile.h"

static constexpr SpecialGridPlacement gGreenhouseGridPlacement[] =  //0x69DE50
{
    {  73,  73, 0, 0 },
    { 155,  71, 1, 0 },
    { 239,  68, 2, 0 },
    { 321,  73, 3, 0 },
    { 406,  71, 4, 0 },
    { 484,  67, 5, 0 },
    { 566,  70, 6, 0 },
    { 648,  72, 7, 0 },
    {  67, 168, 0, 1 },
    { 150, 165, 1, 1 },
    { 232, 170, 2, 1 },
    { 314, 175, 3, 1 },
    { 416, 173, 4, 1 },
    { 497, 170, 5, 1 },
    { 578, 164, 6, 1 },
    { 660, 168, 7, 1 },
    {  41, 268, 0, 2 },
    { 130, 266, 1, 2 },
    { 219, 260, 2, 2 },
    { 310, 266, 3, 2 },
    { 416, 267, 4, 2 },
    { 504, 261, 5, 2 },
    { 594, 265, 6, 2 },
    { 684, 269, 7, 2 },
    {  37, 371, 0, 3 },
    { 124, 369, 1, 3 },
    { 211, 368, 2, 3 },
    { 302, 369, 3, 3 },
    { 425, 375, 4, 3 },
    { 512, 368, 5, 3 },
    { 602, 365, 6, 3 },
    { 691, 368, 7, 3 }
};

static constexpr SpecialGridPlacement gMushroomGridPlacement[] = {  //0x69E050
    { 110, 441, 0, 0 },
    { 237, 360, 1, 0 },
    { 298, 458, 2, 0 },
    { 355, 296, 3, 0 },
    { 387, 203, 4, 0 },
    { 460, 385, 5, 0 },
    { 486, 478, 6, 0 },
    { 552, 283, 7, 0 }
};

static constexpr SpecialGridPlacement gAquariumGridPlacement[] = {  //0x69E0D0
    { 113, 185, 0, 0 },
    { 306,  85, 1, 0 },
    { 356, 213, 2, 0 },
    { 522, 105, 3, 0 },
    { 669, 270, 4, 0 },
    { 122, 355, 5, 0 },
    { 365, 458, 6, 0 },
    { 504, 417, 7, 0 }
};

static_assert(std::size(gGreenhouseGridPlacement) <= MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y);
static_assert(std::size(gMushroomGridPlacement) <= MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y);
static_assert(std::size(gAquariumGridPlacement) <= MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y);

ZenGarden::ZenGarden()
{
    mApp = (LawnApp*)gSexyAppBase;
    mBoard = nullptr;
    mGardenType = GardenType::GARDEN_MAIN;
	RefreshTimeCache();
}

void ZenGarden::RefreshTimeCache()
{
	mFrameTime = std::time(nullptr);
	mFrameLocalTime = Sexy::LocalCalendarTime(mFrameTime);
}

//0x51D0E0
void ZenGarden::DrawPottedPlantIcon(std::int64_t nowNs, Graphics* g, float x, float y, PottedPlant* thePottedPlant)
{
    DrawPottedPlant(nowNs, g, x, y, thePottedPlant, 0.7f, true);
}

//0x51D110
void ZenGarden::DrawPottedPlant(std::int64_t nowNs, Graphics* g, float x, float y, PottedPlant* thePottedPlant, float theScale, bool theDrawPot)
{
    Graphics aPottedPlantG(*g);
    aPottedPlantG.mScaleX = theScale;
    aPottedPlantG.mScaleY = theScale;

    DrawVariation aPlantVariation = DrawVariation::VARIATION_NORMAL;
    SeedType aSeedType = thePottedPlant->mSeedType;
    if (thePottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_SPROUT)
    {
        aSeedType = SeedType::SEED_SPROUT;
        if (thePottedPlant->mSeedType != SeedType::SEED_MARIGOLD)
        {
            aPlantVariation = DrawVariation::VARIATION_SPROUT_NO_FLOWER;
        }
    }
    else if ((aSeedType == SeedType::SEED_TANGLEKELP || aSeedType == SeedType::SEED_SEASHROOM) && thePottedPlant->mWhichZenGarden == GardenType::GARDEN_AQUARIUM)
    {
        aPlantVariation = DrawVariation::VARIATION_AQUARIUM;
    }
    else
    {
        aPlantVariation = thePottedPlant->mDrawVariation;
    }

    float aOffsetX = 0.0f;
    // Picked-up gifts and shop icons have no active garden Board. Their owner
    // supplies time; the optional garden Board only describes draw context.
    float aOffsetY = PlantDrawHeightOffset(nowNs, mBoard, nullptr, aSeedType, -1, -1);
    if (theDrawPot)
    {
        float aPotOffsetY = theScale * 0.0f - 0.0f + 0.0f;
        aPotOffsetY += PlantDrawHeightOffset(nowNs, mBoard, nullptr, SeedType::SEED_FLOWERPOT, -1, -1);

        DrawVariation aPotVariation2 = DrawVariation::VARIATION_ZEN_GARDEN;
        if (Plant::IsAquatic(aSeedType))
        {
            aPotVariation2 = DrawVariation::VARIATION_ZEN_GARDEN_WATER;
        }

        Plant::DrawSeedType(&aPottedPlantG, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE, aPotVariation2, x, y + aPotOffsetY * theScale);
    }

    if (thePottedPlant->mFacing == PottedPlant::FacingDirection::FACING_LEFT)
    {
        aPottedPlantG.mScaleX = -theScale;
        aOffsetX += 80.0f * theScale;
    }

    if (thePottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_SMALL)
    {
        aOffsetX += 20.0f * aPottedPlantG.mScaleX;
        aOffsetY += 40.0f * aPottedPlantG.mScaleY;
        aPottedPlantG.mScaleX *= 0.5f;
        aPottedPlantG.mScaleY *= 0.5f;
    }
    else if (thePottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_MEDIUM)
    {
        aOffsetX += 10.0f * aPottedPlantG.mScaleX;
        aOffsetY += 20.0f * aPottedPlantG.mScaleY;
        aPottedPlantG.mScaleX *= 0.75f;
        aPottedPlantG.mScaleY *= 0.75f;
    }

    if (theDrawPot)
    {
        aOffsetY += PlantFlowerPotHeightOffset(aSeedType, theScale);
    }
    aOffsetY += PlantPottedDrawHeightOffset(aSeedType, aPottedPlantG.mScaleY);

    Plant::DrawSeedType(&aPottedPlantG, aSeedType, SeedType::SEED_NONE, aPlantVariation, x + aOffsetX, y + aOffsetY);
}

void ZenGarden::PlantSetLaunchCounter(Plant* thePlant)
{
    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
    const uint64_t aElapsedSeconds = mFrameTime > aPottedPlant->mLastNeedFulfilledTime && !PlantHighOnChocolate(aPottedPlant)
        ? static_cast<uint64_t>(mFrameTime) - static_cast<uint64_t>(aPottedPlant->mLastNeedFulfilledTime) : 0;
    const int64_t aAgeNs = static_cast<int64_t>(aElapsedSeconds > 1800 ? 1800 : aElapsedSeconds) * 1'000'000'000LL;
    int aCounterMax = TodAnimateCurveNs(300'000'000'000LL, 1'800'000'000'000LL, aAgeNs, 3000, 15000, Curves::Linear);
    thePlant->mZenProductionCountdown = RandRangeInt(1800, aCounterMax);
}

//0x51D3A0
Plant* ZenGarden::PlacePottedPlant(intptr_t thePottedPlantIndex)
{
    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePottedPlantIndex);
    SeedType aSeedType = aPottedPlant->mSeedType;
    if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_SPROUT)
    {
        aSeedType = SeedType::SEED_SPROUT;
    }

    bool needPot = true;
    if (mGardenType == GardenType::GARDEN_MUSHROOM && !Plant::IsAquatic(aSeedType))
    {
        needPot = false;
    }
    else if (mGardenType == GardenType::GARDEN_AQUARIUM)
    {
        needPot = false;
    }

    if (needPot)
    {
        Plant* aPot = mBoard->NewPlant(aPottedPlant->mX, aPottedPlant->mY, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
        aPot->mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PLANT, 0, (aPot->mY));
        aPot->mStateCountdown = 0;

        Reanimation* aPotReanim = mApp->ReanimationGet(aPot->mBodyReanimID);
        if (Plant::IsAquatic(aSeedType))
        {
            aPotReanim->SetFramesForLayer(ReanimTrackId::anim_waterplants);
        }
        else
        {
            aPotReanim->SetFramesForLayer(ReanimTrackId::anim_zengarden);
        }
    }

    Plant* aPlant = mBoard->NewPlant(aPottedPlant->mX, aPottedPlant->mY, aSeedType, SeedType::SEED_NONE);
    aPlant->mPottedPlantIndex = thePottedPlantIndex;
    aPlant->mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PLANT, 0, (aPlant->mY) + 1);
    aPlant->mStateCountdown = 0;

    Reanimation* aPlantReanim = mApp->ReanimationTryToGet(aPlant->mBodyReanimID);
    if (aPlantReanim)
    {
        if (aSeedType == SeedType::SEED_SPROUT)
        {
            if (aPottedPlant->mSeedType != SeedType::SEED_MARIGOLD)
            {
                aPlantReanim->SetFramesForLayer(ReanimTrackId::anim_idle_noflower);
            }
        }
        else if ((aSeedType == SeedType::SEED_TANGLEKELP || aSeedType == SeedType::SEED_SEASHROOM) && mGardenType == GardenType::GARDEN_AQUARIUM)
        {
            aPlantReanim->SetFramesForLayer(ReanimTrackId::anim_idle_aquarium);
        }
        else if (aPottedPlant->mDrawVariation != DrawVariation::VARIATION_NORMAL)
        {
            mApp->mReanimatorCache->UpdateReanimationForVariation(aPlantReanim, aPottedPlant->mDrawVariation);
        }

        aPlant->UpdateReanim(static_cast<std::int64_t>(mBoard->mLevelTimeNs), (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
        aPlantReanim->UpdateAt((mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
    }

    PlantSetLaunchCounter(aPlant);
    UpdatePlantEffectState(aPlant);
    return aPlant;
}

//0x51D5C0
void ZenGarden::RemovePottedPlant(Plant* thePlant)
{
    thePlant->Die(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
    Plant* aPot = mBoard->GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, PlantPriority::TOPPLANT_ONLY_UNDER_PLANT);
    if (aPot)
    {
        aPot->Die(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
    }
}

PottedPlant* ZenGarden::PottedPlantFromIndex(intptr_t thePottedPlantIndex)
{
    // 1073 原版此处仅 debug 断言，release 裸奔负/越界下标——禅境花园速通
    // 写档 bug（卖盆留花→负计数→时间字段越界写 PlayerInfo）的越界通道。
    // 按"bug 要响"纪律常开硬错；坏档（计数被污染）在此当场暴露而非静默写。
    if (thePottedPlantIndex < 0 ||
        thePottedPlantIndex >= mApp->mPlayerInfo->mNumPottedPlants ||
        thePottedPlantIndex >= MAX_POTTED_PLANTS)
    {
        TodErrorMessageBox(StrFormat(
            "Zen garden potted-plant index out of range: index=%d count=%d",
            (int)thePottedPlantIndex, mApp->mPlayerInfo->mNumPottedPlants).c_str(), "Error");
        exit(0);
    }
    return &mApp->mPlayerInfo->mPottedPlant[thePottedPlantIndex];
}

//0x51D630
void ZenGarden::ZenGardenInitLevel()
{
    CancelPendingSale();
    mBoard = mApp->mBoard;
    // Placement uses the clock before the first ZenGardenUpdate of this visit.
    RefreshTimeCache();
    for (int i = 0; i < mApp->mPlayerInfo->mNumPottedPlants; i++)
    {
        PottedPlant* aPottedPlant = PottedPlantFromIndex(i);
        if (aPottedPlant->mWhichZenGarden == mGardenType)
        {
            PlacePottedPlant(i);
        }
    }

    mBoard->mChallenge->mChallengeStateCounter = 3000;
    AddStinky();
    mApp->mMusic->StartGameMusic();
}

void ZenGarden::ZenGardenStart()
{
}

//0x51D6B0
bool ZenGarden::PlantCanHaveChocolate(Plant* thePlant)
{
    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
    return aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_FULL && WasPlantNeedFulfilledToday(aPottedPlant) && !PlantHighOnChocolate(aPottedPlant);
}

//0x51D710
bool ZenGarden::CanDropChocolate()
{
    return HasPurchasedStinky() && mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_CHOCOLATE] - PURCHASE_COUNT_OFFSET < 10;
}

//0x51D740
bool ZenGarden::IsZenGardenFull(bool theIncludeDroppedPresents)
{
    int aNumDroppedPresents = 0;
    if (mBoard && theIncludeDroppedPresents)
    {
        aNumDroppedPresents += mBoard->CountCoinByType(CoinType::COIN_AWARD_PRESENT);
        aNumDroppedPresents += mBoard->CountCoinByType(CoinType::COIN_PRESENT_PLANT);
    }

    int aNumPottedPlantsInGarden = 0;
    for (int i = 0; i < mApp->mPlayerInfo->mNumPottedPlants; i++)
    {
        PottedPlant* aPottedPlant = PottedPlantFromIndex(i);
        if (aPottedPlant->mWhichZenGarden == GardenType::GARDEN_MAIN)
        {
            aNumPottedPlantsInGarden++;
        }
    }

    return aNumDroppedPresents + aNumPottedPlantsInGarden >= ZEN_MAX_GRIDSIZE_X * ZEN_MAX_GRIDSIZE_Y;
}

bool ZenGarden::CanDropPottedPlantLoot()
{
    return mApp->HasFinishedAdventure() && !IsZenGardenFull(true);
}

//0x51D7B0
void ZenGarden::FindOpenZenGardenSpot(int& theSpotX, int& theSpotY)
{
    TodWeightedGridArray aPicks[ZEN_MAX_GRIDSIZE_X * ZEN_MAX_GRIDSIZE_Y];
    int aPickCount = 0;

    for (int x = 0; x < ZEN_MAX_GRIDSIZE_X; x++)
    {
        for (int y = 0; y < ZEN_MAX_GRIDSIZE_Y; y++)
        {
            if (mApp->mCrazyDaveMessageIndex != -1 && (x < 2 || y < 1))
            {
                goto _m_skip_plant_pick;  // 忽略被戴夫遮挡住的部分
            }

            for (int i = 0; i < mApp->mPlayerInfo->mNumPottedPlants; i++)
            {
                PottedPlant* aPottedPlant = PottedPlantFromIndex(i);
                if (aPottedPlant->mWhichZenGarden == GardenType::GARDEN_MAIN && aPottedPlant->mX == x && aPottedPlant->mY == y)
                {
                    goto _m_skip_plant_pick;  // 格子内已有盆栽植物则不可选择
                }
            }

            aPicks[aPickCount].mX = x;
            aPicks[aPickCount].mY = y;
            aPicks[aPickCount].mWeight = 1;
            aPickCount++;
        _m_skip_plant_pick:
            ; // @Minerscale fixed broken Zen Garden plant picker. Now I have to fix my save file...
        }
    }

    TodWeightedGridArray* aSpot = TodPickFromWeightedGridArray(aPicks, aPickCount);
    theSpotX = aSpot->mX;
    theSpotY = aSpot->mY;
}

//0x51D8C0
void ZenGarden::AddPottedPlant(PottedPlant* thePottedPlant)
{
    TOD_ASSERT(mApp->mPlayerInfo->mNumPottedPlants < MAX_POTTED_PLANTS);
    // 负计数 = 存档已受越界卖出污染（速通写档链终点）。绝不带负值做
    // &mPottedPlant[负数] 的越界写，按回退清零纪律硬错并保住现场。
    if (mApp->mPlayerInfo->mNumPottedPlants < 0 ||
        mApp->mPlayerInfo->mNumPottedPlants >= MAX_POTTED_PLANTS)
    {
        TodErrorMessageBox(StrFormat(
            "Zen garden plant count corrupt: %d. The profile was damaged by an "
            "out-of-bounds sell exploit; keep the file and recover manually.",
            mApp->mPlayerInfo->mNumPottedPlants).c_str(), "Error");
        exit(0);
    }

    int aPottedPlantIndex = mApp->mPlayerInfo->mNumPottedPlants;
    PottedPlant* aPottedPlant = &mApp->mPlayerInfo->mPottedPlant[aPottedPlantIndex];
    *aPottedPlant = *thePottedPlant;
    aPottedPlant->mWhichZenGarden = GardenType::GARDEN_MAIN;
    aPottedPlant->mLastWateredTime = 0;
    FindOpenZenGardenSpot(aPottedPlant->mX, aPottedPlant->mY);
    mApp->mPlayerInfo->mNumPottedPlants++;

    if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN && mBoard && aPottedPlant->mWhichZenGarden == mGardenType)
    {
        Plant* aPlant = PlacePottedPlant(aPottedPlantIndex);
        if (mApp->GetDialog(Dialogs::DIALOG_STORE) == nullptr)
        {
            mBoard->DoPlantingEffects(aPottedPlant->mX, aPottedPlant->mY, aPlant);
        }
    }
}

//0x51D970
int ZenGarden::GetPlantSellPrice(Plant* thePlant)
{
    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
    if (aPottedPlant->mSeedType == SeedType::SEED_MARIGOLD)
    {
        if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_SPROUT)
        {
            return 150;
        }
        if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_SMALL)
        {
            return 200;
        }
        if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_MEDIUM)
        {
            return 250;
        }
        if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_FULL)
        {
            return 300;
        }
        TOD_ASSERT(false);
    }
    if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_SPROUT)
    {
        return 150;
    }
    if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_SMALL)
    {
        return 300;
    }
    if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_MEDIUM)
    {
        return 500;
    }
    if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_FULL)
    {
        if (Plant::IsNocturnal(aPottedPlant->mSeedType) || Plant::IsAquatic(aPottedPlant->mSeedType))
        {
            return 1000;
        }
        return 800;
    }
    TOD_ASSERT(false);

	return 0;
}

//0x51DA00
void ZenGarden::MouseDownWithMoneySign(Plant* thePlant)
{
    mBoard->ClearCursor();

    SexyString aHeader = gStr.ZEN_SELL_HEADER;
    SexyString aLines = gStr.ZEN_SELL_LINES;
    int aPrice = GetPlantSellPrice(thePlant);
    if (mApp->mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_OFF)
    {
        mApp->CrazyDaveEnter();
    }

    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
    SexyString aMessageText = mApp->GetCrazyDaveText(1700);
    aMessageText = TodReplaceNumberString(aMessageText, UiTextPlaceholder::SellPrice, aPrice);

    SexyString aPlantName;
    if (thePlant->mSeedType == SeedType::SEED_SPROUT && aPottedPlant->mSeedType == SeedType::SEED_MARIGOLD)
    {
        aPlantName = gStr.MARIGOLD_SPROUT;
    }
    else
    {
        aPlantName = Plant::GetNameString(thePlant->mSeedType, thePlant->mImitaterType);
    }
    aMessageText = TodReplaceString(aMessageText, UiTextPlaceholder::PlantType, aPlantName);

    mApp->CrazyDaveTalkMessage(aMessageText, GetDaveDialogueMetadata(1700));
    Reanimation* aCrazyDaveReanim = mApp->ReanimationGet(mApp->mCrazyDaveReanimID);
    aCrazyDaveReanim->PlayReanim(ReanimTrackId::anim_blahblah, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 12.0f);

	const PlantID aPlantID = static_cast<PlantID>(
		mBoard->mPlants.DataArrayGetID(thePlant));
	// 身份快照：记录索引随植物对象存活，槽位复用/施肥重放置都会换对象。
	// 确认时除 ID 外还要求解析出的植物仍持有同一记录索引（防卖错无辜记录）。
	const intptr_t aSaleIndex = thePlant->mPottedPlantIndex;
    const auto aSaleRevision = mSaleRevision;
    const PlayerInfo* aSalePlayer = mApp->mPlayerInfo;
    Dialog* aDialog = mApp->DoDialog(Dialogs::DIALOG_ZEN_SELL, true, aHeader, aLines, (""), Dialog::BUTTONS_YES_NO);
    aDialog->mX += 120;
    aDialog->mY += 60;
    mBoard->mCoinBankFadeCount = 1000;
    int aResult = aDialog->WaitForResult(true);
    // A Yes callback may have closed the dialog before a later message replaced
    // its board. Full PlantIDs are unique only within one DataArray lifetime;
    // do not commit an old sale against a new garden, even if its ID was reused.
    if (aSaleRevision != mSaleRevision || aSalePlayer != mApp->mPlayerInfo)
        return;
    mApp->CrazyDaveLeave();

    if (aResult == Dialog::ID_YES)
    {
		Plant* aSellPlant = mBoard->mPlants.DataArrayTryToGet(
			static_cast<unsigned int>(aPlantID));
		if (aSellPlant == nullptr || aSellPlant->mDead ||
			aSellPlant->mPottedPlantIndex != aSaleIndex ||
			aSellPlant->mPottedPlantIndex < 0 ||
			aSellPlant->mPottedPlantIndex >= mApp->mPlayerInfo->mNumPottedPlants)
		{
			return;
		}
		aPottedPlant = PottedPlantFromIndex(aSellPlant->mPottedPlantIndex);
        mApp->mPlayerInfo->AddCoins(aPrice);
        mBoard->mCoinsCollected += aPrice;

		int aNumPlantsAfterThis = mApp->mPlayerInfo->mNumPottedPlants - aSellPlant->mPottedPlantIndex - 1;
        if (aNumPlantsAfterThis > 0)
        {
			memmove(aPottedPlant, aPottedPlant + 1,
				aNumPlantsAfterThis * sizeof(PottedPlant));

            Plant* aUpdatePlant = nullptr;
            while (mBoard->IteratePlants(aUpdatePlant))
            {
				if (!aUpdatePlant->mDead &&
					aUpdatePlant->mPottedPlantIndex > aSellPlant->mPottedPlantIndex)
                {
                    aUpdatePlant->mPottedPlantIndex--;
                }
            }
        }

        mApp->mPlayerInfo->mNumPottedPlants--;
        mApp->PlayFoley(FoleyType::FOLEY_USE_SHOVEL);
        //mApp->PlaySample(SOUND_PLANT2);
		RemovePottedPlant(aSellPlant);
    }
}

//0x51DF40
void ZenGarden::PlantFertilized(Plant* thePlant)
{
    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
    aPottedPlant->mLastFertilizedTime = std::time(nullptr);
    aPottedPlant->mPlantAge = (PottedPlantAge)((int)aPottedPlant->mPlantAge + 1);
    aPottedPlant->mPlantNeed = PottedPlantNeed::PLANTNEED_NONE;
    aPottedPlant->mTimesFed = 0;

    if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_SMALL)
    {
		const intptr_t aPottedPlantIndex = thePlant->mPottedPlantIndex;
		const PlantID anOldPlantID = static_cast<PlantID>(
			mBoard->mPlants.DataArrayGetID(thePlant));
        RemovePottedPlant(thePlant);
		Plant* aNewPlant = PlacePottedPlant(aPottedPlantIndex);
		if (mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE &&
			mBoard->mCursorObject->mGlovePlantID == anOldPlantID)
		{
			mBoard->mCursorObject->mGlovePlantID = static_cast<PlantID>(
				mBoard->mPlants.DataArrayGetID(aNewPlant));
		}
		thePlant = aNewPlant;
        mApp->PlaySample(SOUND_LOADINGBAR_FLOWER);
    }
    else
    {
        thePlant->mStateCountdown = 100;
        mApp->PlayFoley(FoleyType::FOLEY_PLANTGROW);
    }

    mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
    if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_SMALL)
    {
        mBoard->AddCoin((thePlant->mX) + 40, (thePlant->mY), CoinType::COIN_GOLD, CoinMotion::COIN_MOTION_COIN);
    }
    else if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_MEDIUM)
    {
        mBoard->AddCoin((thePlant->mX) + 30, (thePlant->mY), CoinType::COIN_GOLD, CoinMotion::COIN_MOTION_COIN);
        mBoard->AddCoin((thePlant->mX) + 50, (thePlant->mY), CoinType::COIN_GOLD, CoinMotion::COIN_MOTION_COIN);
    }
    else if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_FULL)
    {
        if (aPottedPlant->mSeedType == SeedType::SEED_MARIGOLD)
        {
            mBoard->AddCoin((thePlant->mX) + 40, (thePlant->mY), CoinType::COIN_DIAMOND, CoinMotion::COIN_MOTION_COIN);
        }
        else
        {
            mBoard->AddCoin((thePlant->mX) + 10, (thePlant->mY), CoinType::COIN_DIAMOND, CoinMotion::COIN_MOTION_COIN);
            mBoard->AddCoin((thePlant->mX) + 70, (thePlant->mY), CoinType::COIN_DIAMOND, CoinMotion::COIN_MOTION_COIN);
        }
    }
}

//0x51E110
void ZenGarden::PlantFulfillNeed(Plant* thePlant)
{
    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
    aPottedPlant->mLastNeedFulfilledTime = std::time(nullptr);
    aPottedPlant->mPlantNeed = PottedPlantNeed::PLANTNEED_NONE;
    aPottedPlant->mTimesFed = 0;

    mApp->PlayFoley(FoleyType::FOLEY_PRIZE);
    mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
    mBoard->AddCoin((thePlant->mX) + 40, (thePlant->mY), CoinType::COIN_GOLD, CoinMotion::COIN_MOTION_COIN);
    if (Plant::IsNocturnal(thePlant->mSeedType) || Plant::IsAquatic(thePlant->mSeedType))
    {
        mBoard->AddCoin((thePlant->mX) + 10, (thePlant->mY), CoinType::COIN_GOLD, CoinMotion::COIN_MOTION_COIN);
        mBoard->AddCoin((thePlant->mX) + 70, (thePlant->mY), CoinType::COIN_GOLD, CoinMotion::COIN_MOTION_COIN);
    }
}

//0x51E290
bool ZenGarden::PlantsNeedWater()
{
    for (int i = 0; i < mApp->mPlayerInfo->mNumPottedPlants; i++)
    {
        PottedPlant* aPottedPlant = PottedPlantFromIndex(i);
        if (mApp->mZenGarden->GetPlantsNeed(aPottedPlant) == PottedPlantNeed::PLANTNEED_WATER)
        {
            return true;
        }
    }
    return false;
}

//0x51E2F0
bool ZenGarden::PlantCanBeWatered(Plant* thePlant)
{
    if (thePlant->mPottedPlantIndex == -1)
    {
        return false;
    }

    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
    return mApp->mZenGarden->GetPlantsNeed(aPottedPlant) == PottedPlantNeed::PLANTNEED_WATER;
}

//0x51E320
int ZenGarden::CountPlantsNeedingFertilizer()
{
    int aCount = 0;
    for (int i = 0; i < mApp->mPlayerInfo->mNumPottedPlants; i++)
    {
        PottedPlant* aPottedPlant = PottedPlantFromIndex(i);
        if (mApp->mZenGarden->GetPlantsNeed(aPottedPlant) == PottedPlantNeed::PLANTNEED_FERTILIZER)
        {
            aCount++;
        }
    }
    return aCount;
}

//0x51E390
bool ZenGarden::AllPlantsHaveBeenFertilized()
{
    for (int i = 0; i < mApp->mPlayerInfo->mNumPottedPlants; i++)
    {
        PottedPlant* aPottedPlant = PottedPlantFromIndex(i);
        if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_SPROUT)
        {
            return false;
        }
    }
    return true;
}

//0x51E3D0
void ZenGarden::PlantWatered(Plant* thePlant)
{
    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
    aPottedPlant->mTimesFed++;
    int aTimeSpan = RandRangeInt(0, 8);
    if (mBoard->mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_WATER_PLANT || mBoard->mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_KEEP_WATERING)
    {
        aTimeSpan = 9;
    }
    aPottedPlant->mLastWateredTime = std::time(nullptr) - (long long)aTimeSpan;

    mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
    mBoard->AddCoin((thePlant->mX) + 40, (thePlant->mY), CoinType::COIN_SILVER, CoinMotion::COIN_MOTION_COIN);
    if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_FULL && aPottedPlant->mPlantNeed == PottedPlantNeed::PLANTNEED_NONE)
    {
        aPottedPlant->mPlantNeed = (PottedPlantNeed)RandRangeInt((int)PottedPlantNeed::PLANTNEED_BUGSPRAY, (int)PottedPlantNeed::PLANTNEED_PHONOGRAPH);
    }

    if (mBoard->mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_WATER_PLANT)
    {
        mBoard->mTutorialState = TutorialState::TUTORIAL_ZEN_GARDEN_KEEP_WATERING;
        mBoard->DisplayAdvice(gStr.ADVICE_ZEN_GARDEN_KEEP_WATERING, MessageStyle::MESSAGE_STYLE_ZEN_GARDEN_LONG, AdviceType::ADVICE_NONE);
    }
}

//0x51E560
void ZenGarden::UpdatePlantEffectState(Plant* thePlant)
{
    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
    PlantState aOriginalState = thePlant->mState;
    PottedPlantNeed aPlantNeed = GetPlantsNeed(aPottedPlant);
    if (aPlantNeed == PottedPlantNeed::PLANTNEED_WATER)
    {
        thePlant->mState = PlantState::STATE_NOTREADY;
    }
    else if (aPlantNeed == PottedPlantNeed::PLANTNEED_NONE)
    {
        if (WasPlantNeedFulfilledToday(aPottedPlant))
        {
            thePlant->mState = PlantState::STATE_ZEN_GARDEN_HAPPY;
        }
        else if (thePlant->mIsAsleep)
        {
            thePlant->mState = PlantState::STATE_NOTREADY;
        }
        else
        {
            thePlant->mState = PlantState::STATE_ZEN_GARDEN_WATERED;
        }
    }
    else
    {
        thePlant->mState = PlantState::STATE_ZEN_GARDEN_NEEDY;
    }
    if (aOriginalState == thePlant->mState)
    {
        return;
    }

    Plant* aFlowerPot = mBoard->GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, PlantPriority::TOPPLANT_ONLY_UNDER_PLANT);
    if (aFlowerPot && !Plant::IsAquatic(thePlant->mSeedType))
    {
        Reanimation* aPotReanim = mApp->ReanimationGet(aFlowerPot->mBodyReanimID);
        if (thePlant->mState == PlantState::STATE_ZEN_GARDEN_WATERED ||
            thePlant->mState == PlantState::STATE_ZEN_GARDEN_NEEDY ||
            thePlant->mState == PlantState::STATE_ZEN_GARDEN_HAPPY)
        {
            aPotReanim->SetVariantOption(ReanimTrackId::Pot_top, ReanimOptions::Pot::Pot_top::POT_TOP_DARK);
        }
        else
        {
            aPotReanim->SetVariantOption(ReanimTrackId::Pot_top, 0);
        }
    }
    if (aOriginalState == PlantState::STATE_ZEN_GARDEN_HAPPY)
    {
        RemoveHappyEffect(thePlant);
    }

    if (thePlant->mState == PlantState::STATE_ZEN_GARDEN_HAPPY)
    {
        thePlant->SetSleeping(static_cast<std::int64_t>(mBoard->mLevelTimeNs), (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), false);
        AddHappyEffect(thePlant);
    }
    else if (Plant::IsNocturnal(thePlant->mSeedType) && !mBoard->StageIsNight())
    {
        thePlant->SetSleeping(static_cast<std::int64_t>(mBoard->mLevelTimeNs), (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), true);
    }
}

//0x51E730
void ZenGarden::AddHappyEffect(Plant* thePlant)
{
    const auto ownerNowNs = static_cast<std::int64_t>(mBoard->mLevelTimeNs);
    Plant* aFlowerPot = mBoard->GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, PlantPriority::TOPPLANT_ONLY_UNDER_PLANT);
    if (aFlowerPot == nullptr)
    {
        thePlant->AddAttachedParticle(ownerNowNs, (thePlant->mX) + 40, (thePlant->mY) + 60, thePlant->mRenderOrder - 1, ParticleEffect::PARTICLE_POTTED_ZEN_GLOW);
    }
    else if (Plant::IsAquatic(thePlant->mSeedType))
    {
        aFlowerPot->AddAttachedParticle(ownerNowNs, (aFlowerPot->mX) + 40, (aFlowerPot->mY) + 61, aFlowerPot->mRenderOrder - 1, ParticleEffect::PARTICLE_POTTED_WATER_PLANT_GLOW);
    }
    else
    {
        aFlowerPot->AddAttachedParticle(ownerNowNs, (aFlowerPot->mX) + 40, (aFlowerPot->mY) + 63, aFlowerPot->mRenderOrder - 1, ParticleEffect::PARTICLE_POTTED_ZEN_GLOW);
    }
}

//0x45E7E0
void ZenGarden::RemoveHappyEffect(Plant* thePlant)
{
    Plant* aFlowerPot = mBoard->GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, PlantPriority::TOPPLANT_ONLY_UNDER_PLANT);
    TodParticleSystem* aParticleSystem;
    if (aFlowerPot)
    {
        aParticleSystem = mApp->ParticleTryToGet(aFlowerPot->mParticleID);
    }
    else
    {
        aParticleSystem = mApp->ParticleTryToGet(thePlant->mParticleID);
    }
    if (aParticleSystem)
    {
        aParticleSystem->ParticleSystemDie();
    }
}

//0x51E890
bool ZenGarden::WasPlantNeedFulfilledToday(PottedPlant* thePottedPlant)
{
	const long long aNow = mFrameTime;
    if (aNow - thePottedPlant->mLastNeedFulfilledTime < 3600)
    {
        return true;
    }

	const auto aLastNeedFulfilledTM = Sexy::LocalCalendarTime(thePottedPlant->mLastNeedFulfilledTime);
	return mFrameLocalTime.tm_year <= aLastNeedFulfilledTM.tm_year &&
		mFrameLocalTime.tm_yday <= aLastNeedFulfilledTM.tm_yday;
}

//0x51E910
bool ZenGarden::PlantShouldRefreshNeed(PottedPlant* thePottedPlant)
{
	const long long aNow = mFrameTime;
    if (aNow - thePottedPlant->mLastWateredTime < 3600)
    {
        return false;
    }

	const auto aLastWateredTM = Sexy::LocalCalendarTime(thePottedPlant->mLastWateredTime);
	return mFrameLocalTime.tm_year > aLastWateredTM.tm_year ||
		mFrameLocalTime.tm_yday > aLastWateredTM.tm_yday;
}

// GOTY @Patoke: 0x5292A0
void ZenGarden::RefreshPlantNeeds(PottedPlant* thePottedPlant)
{
    if (thePottedPlant->mPlantAge != PottedPlantAge::PLANTAGE_FULL || !PlantShouldRefreshNeed(thePottedPlant))
    {
        return;
    }

    if (Plant::IsAquatic(thePottedPlant->mSeedType))
    {
		thePottedPlant->mLastWateredTime = mFrameTime;
        thePottedPlant->mPlantNeed = (PottedPlantNeed)RandRangeInt((int)PottedPlantNeed::PLANTNEED_BUGSPRAY, (int)PottedPlantNeed::PLANTNEED_PHONOGRAPH);
    }
    else
    {
        thePottedPlant->mTimesFed = 0;
        thePottedPlant->mPlantNeed = PottedPlantNeed::PLANTNEED_NONE;
    }
}

//0x51E990
void ZenGarden::UpdatePlantNeeds()
{
    if (mApp->mPlayerInfo == nullptr)
    {
        return;
    }

	RefreshTimeCache();

    for (int i = 0; i < mApp->mPlayerInfo->mNumPottedPlants; i++)
    {
        PottedPlant* aPottedPlant = PottedPlantFromIndex(i);
        RefreshPlantNeeds(aPottedPlant);
    }
}

bool ZenGarden::WasPlantFertilizedInLastHour(PottedPlant* thePottedPlant)
{
	return mFrameTime - thePottedPlant->mLastFertilizedTime < 3600;
}

//0x51EA30
PottedPlantNeed ZenGarden::GetPlantsNeed(PottedPlant* thePottedPlant)
{
    if (thePottedPlant->mPlantAge != PottedPlantAge::PLANTAGE_SPROUT &&
        Plant::IsNocturnal(thePottedPlant->mSeedType) &&
        thePottedPlant->mWhichZenGarden == GardenType::GARDEN_MAIN)
    {
        return PottedPlantNeed::PLANTNEED_NONE;
    }
    if (thePottedPlant->mWhichZenGarden == GardenType::GARDEN_WHEELBARROW)
    {
        return PottedPlantNeed::PLANTNEED_NONE;
    }

	const long long aNow = mFrameTime;
    bool aTooLongSinceWatering = aNow - thePottedPlant->mLastWateredTime > 15;
    bool aTooShortSinceWatering = aNow - thePottedPlant->mLastWateredTime < 3;

    if (WasPlantFertilizedInLastHour(thePottedPlant) || WasPlantNeedFulfilledToday(thePottedPlant))
    {
        return PottedPlantNeed::PLANTNEED_NONE;
    }
    if (Plant::IsAquatic(thePottedPlant->mSeedType) && thePottedPlant->mPlantAge != PottedPlantAge::PLANTAGE_SPROUT)
    {
        if (thePottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_FULL)
        {
            if (PlantShouldRefreshNeed(thePottedPlant))
            {
                return PottedPlantNeed::PLANTNEED_NONE;
            }
            return thePottedPlant->mPlantNeed;
        }
        else
        {
            if (thePottedPlant->mWhichZenGarden != GardenType::GARDEN_AQUARIUM)
            {
                return PottedPlantNeed::PLANTNEED_NONE;
            }
            return PottedPlantNeed::PLANTNEED_FERTILIZER;
        }
    }
    if (!aTooLongSinceWatering)
    {
        return PottedPlantNeed::PLANTNEED_NONE;
    }
    if (thePottedPlant->mTimesFed < thePottedPlant->mFeedingsPerGrow)
    {
        return PottedPlantNeed::PLANTNEED_WATER;
    }
    if (aTooShortSinceWatering)
    {
        return PottedPlantNeed::PLANTNEED_NONE;
    }
    if (thePottedPlant->mPlantAge != PottedPlantAge::PLANTAGE_FULL)
    {
        return PottedPlantNeed::PLANTNEED_FERTILIZER;
    }
    if (PlantShouldRefreshNeed(thePottedPlant))
    {
        return PottedPlantNeed::PLANTNEED_NONE;
    }
    if (thePottedPlant->mPlantNeed != PottedPlantNeed::PLANTNEED_NONE)
    {
        return thePottedPlant->mPlantNeed;
    }
    return PottedPlantNeed::PLANTNEED_WATER;
}

//0x51EB70
void ZenGarden::MouseDownWithFeedingTool(int x, int y, CursorType theCursorType)
{
    Plant* aPlantToFeed = nullptr;
    {
        Plant* aPlant = nullptr;
        while (mBoard->IteratePlants(aPlant))
        {
            if (aPlant->mHighlighted && aPlant->mPottedPlantIndex != -1)
            {
                aPlantToFeed = aPlant;
                break;
            }
        }
    }

    if (theCursorType == CursorType::CURSOR_TYPE_CHOCOLATE)
    {
        TOD_ASSERT(mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_CHOCOLATE] > PURCHASE_COUNT_OFFSET);

        GridItem* aStinky = GetStinky();
        if (aStinky && aStinky->mHighlighted)
        {
            WakeStinky();
            mApp->AddTodParticle(aStinky->mPosX + 40.0f, aStinky->mPosY + 40.0f, aStinky->mRenderOrder + 1, ParticleEffect::PARTICLE_PRESENT_PICKUP, static_cast<std::int64_t>(mBoard->mLevelTimeNs));
            mApp->mPlayerInfo->mLastStinkyChocolateTime = static_cast<std::int32_t>(std::time(nullptr));
            mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_CHOCOLATE]--;

            mApp->PlayFoley(FoleyType::FOLEY_WAKEUP);
            mApp->PlaySample(SOUND_MINDCONTROLLED);
        }

        if (aPlantToFeed)
        {
            mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_CHOCOLATE]--;
            FeedChocolateToPlant(aPlantToFeed);
            mApp->PlayFoley(FoleyType::FOLEY_WAKEUP);
        }
    }

    if (aPlantToFeed)
    {
        GridItem* aZenTool = mBoard->mGridItems.DataArrayAlloc();
        aZenTool->mGridItemType = GridItemType::GRIDITEM_ZEN_TOOL;
        aZenTool->mGridX = aPlantToFeed->mPlantCol;
        aZenTool->mGridY = aPlantToFeed->mRow;
        aZenTool->mPosX = (aPlantToFeed->mX) + 40;
        aZenTool->mPosY = (aPlantToFeed->mY) + 40;
        aZenTool->mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0);

        if (theCursorType == CursorType::CURSOR_TYPE_WATERING_CAN)
        {
            // 表外特例：金水壶双形态（购买态改落点/水域轨道），不并入工具表
            if (mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_GOLD_WATERINGCAN])
            {
                aZenTool->mPosX = x;
                aZenTool->mPosY = y;
                Reanimation* aWateringCanReanim = mApp->AddReanimation(x, y, 0, ReanimationType::REANIM_ZENGARDEN_WATERINGCAN, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
                aWateringCanReanim->PlayReanim(ReanimTrackId::anim_water_area, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 8.0f);
                aZenTool->mGridItemReanimID = mApp->ReanimationGetID(aWateringCanReanim);
                aZenTool->mGridItemState = GridItemState::GRIDITEM_STATE_ZEN_TOOL_GOLD_WATERING_CAN;
                mApp->PlayFoley(FoleyType::FOLEY_WATERING);
            }
            else
            {
                Reanimation* aWateringCanReanim = mApp->AddReanimation((aPlantToFeed->mX) + 32, (aPlantToFeed->mY), 0, ReanimationType::REANIM_ZENGARDEN_WATERINGCAN, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
                aWateringCanReanim->PlayReanim(ReanimTrackId::anim_water, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 0.0f);
                aZenTool->mGridItemReanimID = mApp->ReanimationGetID(aWateringCanReanim);
                // 1073 00529848: normal animation must feed only the target pot.
                aZenTool->mGridItemState = GridItemState::GRIDITEM_STATE_ZEN_TOOL_WATERING_CAN;
                mApp->PlayFoley(FoleyType::FOLEY_WATERING);
            }
        }
        else
        {
            // B 形态 -> 常量表 kZenToolRows（原 FERTILIZER/BUG_SPRAY/PHONOGRAPH
            // 三支同构链合并；下标平移 theCursorType - CURSOR_TYPE_FERTILIZER，
            // 行序 = CursorType 枚举连续段 10..12）。mAnimRate 0 槽 = 不设置；
            // mPurchaseItem -1 槽 = 不扣库存。
            static constexpr struct {
                ReanimationType mReanimType;
                float mOffX;             // 相对 (aPlantToFeed->mX)
                float mOffY;
                GridItemState mToolState;
                FoleyType mFoley;
                ReanimLoopType mLoopType;
                float mAnimRate;
                int mPurchaseItem;       // StoreItem；-1 = 不扣
            } kZenToolRows[(int)CursorType::CURSOR_TYPE_PHONOGRAPH - (int)CursorType::CURSOR_TYPE_FERTILIZER + 1] = {
                { ReanimationType::REANIM_ZENGARDEN_FERTILIZER,  0.0f,  0.0f, GridItemState::GRIDITEM_STATE_ZEN_TOOL_FERTILIZER, FoleyType::FOLEY_FERTILIZER, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0.0f,  (int)StoreItem::STORE_ITEM_FERTILIZER },  // 10 CURSOR_TYPE_FERTILIZER
                { ReanimationType::REANIM_ZENGARDEN_BUGSPRAY,   54.0f,  0.0f, GridItemState::GRIDITEM_STATE_ZEN_TOOL_BUG_SPRAY,  FoleyType::FOLEY_BUGSPRAY,   ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0.0f,  (int)StoreItem::STORE_ITEM_BUG_SPRAY },     // 11 CURSOR_TYPE_BUG_SPRAY
                { ReanimationType::REANIM_ZENGARDEN_PHONOGRAPH, 20.0f, 34.0f, GridItemState::GRIDITEM_STATE_ZEN_TOOL_PHONOGRAPH,  FoleyType::FOLEY_PHONOGRAPH,  ReanimLoopType::REANIM_LOOP,                  20.0f, -1 },                                        // 12 CURSOR_TYPE_PHONOGRAPH
            };
            static_assert(sizeof(kZenToolRows) / sizeof(kZenToolRows[0]) ==
                (int)CursorType::CURSOR_TYPE_PHONOGRAPH - (int)CursorType::CURSOR_TYPE_FERTILIZER + 1,
                "kZenToolRows 槽位须与 CursorType 喂养工具段同步");

            const int aToolIndex = (int)theCursorType - (int)CursorType::CURSOR_TYPE_FERTILIZER;
            if ((unsigned)aToolIndex < sizeof(kZenToolRows) / sizeof(kZenToolRows[0]))
            {
                const auto& aRow = kZenToolRows[aToolIndex];
                Reanimation* aToolReanim = mApp->AddReanimation((aPlantToFeed->mX) + aRow.mOffX, (aPlantToFeed->mY) + aRow.mOffY, 0, aRow.mReanimType, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
                if (aRow.mAnimRate != 0.0f)
                {
                    aToolReanim->ApplyRate(aRow.mAnimRate);
                }
                aToolReanim->ChangeLoopType(aRow.mLoopType);
                aZenTool->mGridItemReanimID = mApp->ReanimationGetID(aToolReanim);
                aZenTool->mGridItemState = aRow.mToolState;
                mApp->PlayFoley(aRow.mFoley);

                if (aRow.mPurchaseItem >= 0)
                {
                    TOD_ASSERT(mApp->mPlayerInfo->mPurchases[aRow.mPurchaseItem] > PURCHASE_COUNT_OFFSET);
                    mApp->mPlayerInfo->mPurchases[aRow.mPurchaseItem]--;
                }
            }
        }
    }

    mBoard->ClearCursor();
}

//0x51F220
void ZenGarden::FeedChocolateToPlant(Plant* thePlant)
{
    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
    aPottedPlant->mLastChocolateTime = std::time(nullptr);
    thePlant->mZenProductionCountdown = 60;
    mApp->AddTodParticle((thePlant->mX) + 40.0f, (thePlant->mY) + 40.0f, thePlant->mRenderOrder + 1, ParticleEffect::PARTICLE_PRESENT_PICKUP, static_cast<std::int64_t>(mBoard->mLevelTimeNs));
}

//0x51F2A0
void ZenGarden::DoFeedingTool(int x, int y, GridItemState theToolType)
{
    if (theToolType == GridItemState::GRIDITEM_STATE_ZEN_TOOL_GOLD_WATERING_CAN)
    {
        Plant* aPlant = nullptr;
        while (mBoard->IteratePlants(aPlant))
        {
			if (aPlant->mDead || aPlant->mPottedPlantIndex < 0)
				continue;
            if (mBoard->IsPlantInGoldWateringCanRange(x, y, aPlant))
            {
                PottedPlant* aPottedPlant = PottedPlantFromIndex(aPlant->mPottedPlantIndex);
                if (GetPlantsNeed(aPottedPlant) == PottedPlantNeed::PLANTNEED_WATER)
                {
                    PlantWatered(aPlant);
                }
            }
        }
        return;
    }

    int aGridX = PixelToGridX(x, y);
    int aGridY = PixelToGridY(x, y);
    Plant* aPlant = mBoard->GetTopPlantAt(aGridX, aGridY, PlantPriority::TOPPLANT_ZEN_TOOL_ORDER);
	if (aPlant && !aPlant->mDead && aPlant->mPottedPlantIndex >= 0)
    {
        PottedPlant* aPottedPlant = PottedPlantFromIndex(aPlant->mPottedPlantIndex);
        PottedPlantNeed aNeed = GetPlantsNeed(aPottedPlant);

        // B 形态 -> 成员函数指针配对表 kFeedingCases（原 4 支 need×tool 配对链
        // 合并；BUGSPRAY/PHONOGRAPH 共体 PlantFulfillNeed）；不配对 = 原链各支
        // 均不命中（无 default 体）。
        static constexpr struct { PottedPlantNeed mNeed; GridItemState mTool; void (ZenGarden::*mFeed)(Plant*); } kFeedingCases[] = {
            { PottedPlantNeed::PLANTNEED_WATER,      GridItemState::GRIDITEM_STATE_ZEN_TOOL_WATERING_CAN, &ZenGarden::PlantWatered },      // 原第 1 支
            { PottedPlantNeed::PLANTNEED_FERTILIZER, GridItemState::GRIDITEM_STATE_ZEN_TOOL_FERTILIZER,   &ZenGarden::PlantFertilized },   // 原第 2 支
            { PottedPlantNeed::PLANTNEED_BUGSPRAY,   GridItemState::GRIDITEM_STATE_ZEN_TOOL_BUG_SPRAY,    &ZenGarden::PlantFulfillNeed },  // 原第 3 支
            { PottedPlantNeed::PLANTNEED_PHONOGRAPH, GridItemState::GRIDITEM_STATE_ZEN_TOOL_PHONOGRAPH,   &ZenGarden::PlantFulfillNeed },  // 原第 4 支（共体）
        };

        for (const auto& aCase : kFeedingCases)
        {
            if (aCase.mNeed == aNeed && aCase.mTool == theToolType)
            {
                (this->*aCase.mFeed)(aPlant);
                break;
            }
        }

        if (mBoard->mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_FERTILIZE_PLANTS && theToolType == GridItemState::GRIDITEM_STATE_ZEN_TOOL_FERTILIZER)
        {
            if (AllPlantsHaveBeenFertilized())
            {
                mApp->mBoard->mTutorialState = TutorialState::TUTORIAL_ZEN_GARDEN_COMPLETED;
                mApp->mBoard->DisplayAdvice(gStr.ADVICE_ZEN_GARDEN_CONTINUE_ADVENTURE, MessageStyle::MESSAGE_STYLE_HINT_TALL_FAST, AdviceType::ADVICE_NONE);
                mBoard->mMenuButton->mDisabled = false;
                mBoard->mMenuButton->mBtnNoDraw = false;
            }
            else if (mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_FERTILIZER] == PURCHASE_COUNT_OFFSET)
            {
                mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_FERTILIZER] = PURCHASE_COUNT_OFFSET + 5;
                mApp->mBoard->DisplayAdvice(gStr.ADVICE_ZEN_GARDEN_NEED_MORE_FERTILIZER, MessageStyle::MESSAGE_STYLE_HINT_TALL_FAST, AdviceType::ADVICE_NONE);
            }
        }
    }
}

//0x51F580
void ZenGarden::MouseDownWithTool(int x, int y, CursorType theCursorType)
{
    if (theCursorType == CursorType::CURSOR_TYPE_WHEEELBARROW && GetPottedPlantInWheelbarrow())
    {
        MouseDownWithFullWheelBarrow(x, y);
        mBoard->ClearCursor();
        return;
    }

    if (theCursorType == CursorType::CURSOR_TYPE_WATERING_CAN ||
        theCursorType == CursorType::CURSOR_TYPE_FERTILIZER ||
        theCursorType == CursorType::CURSOR_TYPE_BUG_SPRAY ||
        theCursorType == CursorType::CURSOR_TYPE_PHONOGRAPH ||
        theCursorType == CursorType::CURSOR_TYPE_CHOCOLATE)
    {
        MouseDownWithFeedingTool(x, y, theCursorType);
        return;
    }

    Plant* aPlant = mBoard->ToolHitTest(x, y);
    if (aPlant == nullptr || aPlant->mPottedPlantIndex == -1)
    {
        mApp->PlayFoley(FoleyType::FOLEY_DROP);
        mBoard->ClearCursor();
        return;
    }

    if (theCursorType == CursorType::CURSOR_TYPE_MONEY_SIGN)
    {
        MouseDownWithMoneySign(aPlant);
    }
    else if (theCursorType == CursorType::CURSOR_TYPE_WHEEELBARROW)
    {
        MouseDownWithEmptyWheelBarrow(aPlant);
        mBoard->ClearCursor();
    }
    else if (theCursorType == CursorType::CURSOR_TYPE_GLOVE)
    {
        mBoard->mCursorObject->mType = aPlant->mSeedType;
        mBoard->mCursorObject->mImitaterType = aPlant->mImitaterType;
        mBoard->mCursorObject->mCursorType = CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE;
        mBoard->mCursorObject->mGlovePlantID = (PlantID)mBoard->mPlants.DataArrayGetID(aPlant);
        //mBoard->mIgnoreMouseUp = true;
        mApp->PlaySample(SOUND_TAP);
    }
}

//0x51F700
void ZenGarden::MovePlant(Plant* thePlant, int theGridX, int theGridY)
{
    if (mApp->mGameMode != GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
    {
        return;
    }

    int aPosX = mBoard->GridToPixelX(theGridX, theGridY);
    int aPosY = mBoard->GridToPixelY(theGridX, theGridY);
    TOD_ASSERT(mBoard->GetTopPlantAt(theGridX, theGridY, PlantPriority::TOPPLANT_ANY) == nullptr);

    //bool aIsSleeping = thePlant->mIsAsleep; // unused
    thePlant->SetSleeping(static_cast<std::int64_t>(mBoard->mLevelTimeNs), (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), false);
    Plant* aTopPlantAtGrid = mBoard->GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, PlantPriority::TOPPLANT_ONLY_UNDER_PLANT);
    if (aTopPlantAtGrid)
    {
        const int anOldRow = aTopPlantAtGrid->mRow, anOldCol = aTopPlantAtGrid->mPlantCol;
        { aTopPlantAtGrid->mX = (aPosX); aTopPlantAtGrid->mRootBindingGeneration = 0; }
        aTopPlantAtGrid->mY = (aPosY);
        aTopPlantAtGrid->mPlantCol = theGridX;
        aTopPlantAtGrid->mRow = theGridY;
        aTopPlantAtGrid->mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PLANT, 0, aPosY);
        mBoard->MovePlantCellBucket(aTopPlantAtGrid, anOldRow, anOldCol);
    }
    float aDeltaX = aPosX - (thePlant->mX);
    float aDeltaY = aPosY - (thePlant->mY);
    const int anOldRow = thePlant->mRow, anOldCol = thePlant->mPlantCol;
    { thePlant->mX = (aPosX); thePlant->mRootBindingGeneration = 0; }
    thePlant->mY = (aPosY);
    thePlant->mPlantCol = theGridX;
    thePlant->mRow = theGridY;
    thePlant->mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PLANT, 0, aPosY + 1);
    mBoard->MovePlantCellBucket(thePlant, anOldRow, anOldCol);

    TodParticleSystem* aParticle = mApp->ParticleTryToGet(thePlant->mParticleID);
    if (aParticle && aParticle->EmitterRosterCount() > 0)
    {
        TodParticleEmitter* aEmitter = aParticle->EmitterRosterEmitterAt(0);
        aParticle->SystemMove(aEmitter->mSystemCenter.x + aDeltaX, aEmitter->mSystemCenter.y + aDeltaY);
    }

    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
    aPottedPlant->mX = theGridX;
    aPottedPlant->mY = theGridY;
    if (thePlant->mState == PlantState::STATE_ZEN_GARDEN_HAPPY)
    {
        RemoveHappyEffect(thePlant);
        AddHappyEffect(thePlant);
    }

    if (aTopPlantAtGrid)
    {
        mBoard->DoPlantingEffects(theGridX, theGridY, aTopPlantAtGrid);
    }
    else
    {
        mBoard->DoPlantingEffects(theGridX, theGridY, thePlant);
    }
}

//0x51F950
float ZenGarden::PlantPottedDrawHeightOffset(SeedType theSeedType, float theScale)
{
    // B 形态 -> 常量表 kPotOffsets（原 17 支链合并；行序 = SeedType 枚举序
    // 0..52，共体分支（菇族 {0,10}、投石族 {3,10} 等）同值填多槽，{0,0} =
    // 原链无操作）。仿 PlantFlowerPotHeightOffset 的 kFlowerPotAdjust 范式。
    static constexpr struct { float mHeight; float mScaleFix; } kPotOffsets[] = {
        {  0.0f, 10.0f },  // 0 SEED_PEASHOOTER（共体：菇族偏移）
        {  0.0f, 10.0f },  // 1 SEED_SUNFLOWER（共体）
        {  0.0f,  0.0f },  // 2 SEED_CHERRYBOMB
        {  0.0f,  0.0f },  // 3 SEED_WALLNUT
        {  0.0f,  5.0f },  // 4 SEED_POTATOMINE
        {  0.0f, 10.0f },  // 5 SEED_SNOWPEA（共体）
        {  0.0f,  0.0f },  // 6 SEED_CHOMPER（原 aScaleOffsetFix += 0.0f）
        {  0.0f, 10.0f },  // 7 SEED_REPEATER（共体）
        { 10.0f, 24.0f },  // 8 SEED_PUFFSHROOM
        { 10.0f, 17.0f },  // 9 SEED_SUNSHROOM
        {  0.0f,  0.0f },  // 10 SEED_FUMESHROOM
        { 50.0f, 15.0f },  // 11 SEED_GRAVEBUSTER
        {  0.0f, 10.0f },  // 12 SEED_HYPNOSHROOM（共体）
        {  5.0f,  5.0f },  // 13 SEED_SCAREDYSHROOM
        {  0.0f,  0.0f },  // 14 SEED_ICESHROOM
        {  0.0f,  0.0f },  // 15 SEED_DOOMSHROOM
        {-10.0f, 30.0f },  // 16 SEED_LILYPAD
        {  0.0f,  0.0f },  // 17 SEED_SQUASH
        {  0.0f, 10.0f },  // 18 SEED_THREEPEATER（共体）
        {-18.0f, 20.0f },  // 19 SEED_TANGLEKELP
        {  0.0f,  0.0f },  // 20 SEED_JALAPENO
        {-13.0f, 10.0f },  // 21 SEED_SPIKEWEED
        {  0.0f,  3.0f },  // 22 SEED_TORCHWOOD
        {  0.0f,  0.0f },  // 23 SEED_TALLNUT
        {-20.0f, 15.0f },  // 24 SEED_SEASHROOM
        {  0.0f, -1.0f },  // 25 SEED_PLANTERN
        {  0.0f,  0.0f },  // 26 SEED_CACTUS
        {  0.0f, 10.0f },  // 27 SEED_BLOVER
        {  0.0f,  0.0f },  // 28 SEED_SPLITPEA
        { 10.0f, 24.0f },  // 29 SEED_STARFRUIT
        {  0.0f, 20.0f },  // 30 SEED_PUMPKINSHELL
        {  0.0f,  0.0f },  // 31 SEED_MAGNETSHROOM
        {  3.0f, 10.0f },  // 32 SEED_CABBAGEPULT（共体：投石族）
        {  0.0f,  0.0f },  // 33 SEED_FLOWERPOT
        {  0.0f,  0.0f },  // 34 SEED_KERNELPULT
        {  0.0f,  0.0f },  // 35 SEED_INSTANT_COFFEE
        {  0.0f,  0.0f },  // 36 SEED_GARLIC
        {  0.0f,  0.0f },  // 37 SEED_UMBRELLA
        {  0.0f, 10.0f },  // 38 SEED_MARIGOLD（共体；原链重复列出，去重）
        {  3.0f, 10.0f },  // 39 SEED_MELONPULT（共体）
        {  0.0f,  0.0f },  // 40 SEED_GATLINGPEA
        {  0.0f,  0.0f },  // 41 SEED_TWINSUNFLOWER
        {  0.0f,  0.0f },  // 42 SEED_GLOOMSHROOM
        {  0.0f,  0.0f },  // 43 SEED_CATTAIL
        {  0.0f,  0.0f },  // 44 SEED_WINTERMELON
        {  0.0f,  0.0f },  // 45 SEED_GOLD_MAGNET
        {  0.0f,  0.0f },  // 46 SEED_SPIKEROCK
        {  0.0f,  0.0f },  // 47 SEED_COBCANNON
        {  0.0f,  0.0f },  // 48 SEED_IMITATER
        {  0.0f,  0.0f },  // 49 SEED_EXPLODE_O_NUT
        {  0.0f,  0.0f },  // 50 SEED_GIANT_WALLNUT
        {  0.0f,  0.0f },  // 51 SEED_SPROUT
        {  0.0f, 10.0f },  // 52 SEED_LEFTPEATER（共体）
    };
    static_assert(sizeof(kPotOffsets) / sizeof(kPotOffsets[0]) == (int)SeedType::NUM_SEED_TYPES,
        "kPotOffsets 槽位须与 SeedType 枚举同步");
    float aScaleOffsetFix = 0.0f;
    float aHeightOffset = 0.0f;
    if ((unsigned)theSeedType < sizeof(kPotOffsets) / sizeof(kPotOffsets[0]))
    {
        aHeightOffset += kPotOffsets[(int)theSeedType].mHeight;
        aScaleOffsetFix += kPotOffsets[(int)theSeedType].mScaleFix;
    }

    return aHeightOffset + (aScaleOffsetFix * theScale - aScaleOffsetFix);
}

float ZenGarden::ZenPlantOffsetX(PottedPlant* thePottedPlant)
{
    int aOffsetX = 0;
    if (thePottedPlant->mFacing == PottedPlant::FacingDirection::FACING_LEFT && thePottedPlant->mSeedType == SeedType::SEED_POTATOMINE)
    {
        aOffsetX -= 6;
    }
    return aOffsetX;
}

bool ZenGarden::HasPurchasedStinky()
{
    return mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_STINKY_THE_SNAIL] != 0;
}

//0x51FB40
void ZenGarden::AddStinky()
{
    if (!HasPurchasedStinky() || mGardenType != GardenType::GARDEN_MAIN)
    {
        return;
    }

    if (!mApp->mPlayerInfo->mHasSeenStinky)
    {
        mApp->mPlayerInfo->mHasSeenStinky = 1;
        mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_STINKY_THE_SNAIL] = StinkyTimestamp::EncodeWakeTime(std::time(nullptr));
    }

    const int savedX = mApp->mPlayerInfo->mStinkyPosX;
    const int savedY = mApp->mPlayerInfo->mStinkyPosY;
    // A previous crossed-clock root sample could have saved Stinky offscreen.
    // Keep on-canvas coin-chasing positions; recover only impossible saved ones
    // through the original first-visit goal selection, never a movement clamp.
    const bool recoverSavedPosition = savedX < 0 || savedX >= BOARD_WIDTH ||
        savedY < 0 || savedY >= BOARD_HEIGHT;
    const bool pickInitialGoal = savedX == 0 || recoverSavedPosition;
    GridItem* aStinky = mBoard->mGridItems.DataArrayAlloc();
    aStinky->mGridItemType = GridItemType::GRIDITEM_STINKY;
    aStinky->mPosX = pickInitialGoal ? 0.0f : static_cast<float>(savedX);
    aStinky->mPosY = pickInitialGoal ? 0.0f : static_cast<float>(savedY);
    aStinky->mGoalX = aStinky->mPosX;
    aStinky->mGoalY = aStinky->mPosY;
    // Stinky belongs to the garden's visual timeline, which survives board re-entry.
    const auto visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
    Reanimation* aReanimStinky = mApp->AddReanimation(aStinky->mPosX, aStinky->mPosY, 0, ReanimationType::REANIM_STINKY, visualNowNs, ReanimTimeDomain::Visual);
    aReanimStinky->mOverlayMatrix.m00 = 0.8f;
    aReanimStinky->mOverlayMatrix.m11 = 0.8f;
    aStinky->mGridItemReanimID = mApp->ReanimationGetID(aReanimStinky);

    if (pickInitialGoal)
    {
        StinkyPickGoal(aStinky);
        aStinky->mPosX = aStinky->mGoalX;
        aStinky->mPosY = aStinky->mGoalY;
    }

    if (ShouldStinkyBeAwake())
    {
        aReanimStinky->PlayReanim(ReanimTrackId::anim_crawl, ReanimLoopType::REANIM_LOOP, 0, 6.0f);
        aStinky->mGridItemState = GridItemState::GRIDITEM_STINKY_WALKING_LEFT;
    }
    else
    {
        aStinky->mPosY = STINKY_SLEEP_POS_Y;
        StinkyFinishFallingAsleep(aStinky, 0);
    }

    if (recoverSavedPosition)
    {
        aStinky->mGoalX = aStinky->mPosX;
        aStinky->mGoalY = aStinky->mPosY;
        mApp->mPlayerInfo->mStinkyPosX = static_cast<int>(aStinky->mPosX);
        mApp->mPlayerInfo->mStinkyPosY = static_cast<int>(aStinky->mPosY);
    }
    aStinky->mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PLANT, 0, aStinky->mPosY - 30.0f);
    aReanimStinky->mOverlayMatrix.m02 = aStinky->mPosX;
    aReanimStinky->mOverlayMatrix.m12 = aStinky->mPosY;
}

//0x51FCD0
void ZenGarden::StinkyPickGoal(GridItem* theStinky)
{
    float aCurDistToGoal = Distance2D(theStinky->mGoalX, theStinky->mGoalY, theStinky->mPosX, theStinky->mPosY);

    Coin* aBestCoin = nullptr;
    float aCurWeight = 0.0f;
    {
        Coin* aCoin = nullptr;
        while (mBoard->IterateCoins(aCoin))
        {
            if (!aCoin->mIsBeingCollected && aCoin->mPosY == aCoin->mGroundY)
            {
                float aWeight = Distance2D(aCoin->mPosX, aCoin->mPosY + 30.0f, theStinky->mPosX, theStinky->mPosY);
                if (aCoin->mType == CoinType::COIN_GOLD)
                {
                    aWeight -= 40.0f;
                }
                else if (aCoin->mType == CoinType::COIN_DIAMOND)
                {
                    aWeight -= 80.0f;
                }

                float aDistFromLastGoal = Distance2D(aCoin->mPosX, aCoin->mPosY + 30.0f, theStinky->mGoalX, theStinky->mGoalY);
                if (aDistFromLastGoal < 5.0f)
                {
                    aWeight -= 20.0f;
                    aWeight += TodAnimateCurveNs(30'000'000'000LL, 60'000'000'000LL, (aCoin->mDisappearStartTimeNs==INT64_MAX ? 0 : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs-aCoin->mDisappearStartTimeNs), 0, -40, Curves::Linear);
                }

                if (aBestCoin == nullptr || aWeight < aCurWeight)
                {
                    aBestCoin = aCoin;
                    aCurWeight = aWeight;
                }
            }
        }
    }

    if (aBestCoin)
    {
        theStinky->mGoalX = aBestCoin->mPosX;
        theStinky->mGoalY = aBestCoin->mPosY + 30.0f;
    }
    else
    {
        if (aCurDistToGoal > 10.0f)
        {
            return;
        }

        TodWeightedGridArray aPicks[MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y];
        int aPickCount = 0;

        for (const SpecialGridPlacement& aGrid : GetSpecialGridPlacements())
        {
            Plant* aPlant = mBoard->GetTopPlantAt(aGrid.mGridX, aGrid.mGridY, PlantPriority::TOPPLANT_ANY);
            aPicks[aPickCount].mX = aGrid.mPixelX + 15;
            aPicks[aPickCount].mY = aGrid.mPixelY + 80;

            if (aPlant)
            {
                aPicks[aPickCount].mWeight = 2000 - abs(aPicks[aPickCount].mY - theStinky->mPosY);
            }
            else
            {
                aPicks[aPickCount].mWeight = 1;
            }

            aPickCount++;
        }

        TodWeightedGridArray* aTarget = TodPickFromWeightedGridArray(aPicks, aPickCount);
        theStinky->mGoalX = aTarget->mX;
        theStinky->mGoalY = aTarget->mY;
    }

    theStinky->mStateEndTimeNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs+1'000'000'000;
    if (theStinky->mGoalX < theStinky->mPosX && theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_RIGHT)
    {
        Reanimation* aStinyReanim = mApp->ReanimationGet(theStinky->mGridItemReanimID);
        theStinky->mGridItemState = GridItemState::GRIDITEM_STINKY_TURNING_LEFT;
        aStinyReanim->PlayReanim(ReanimTrackId::turn, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 6.0f);
        theStinky->mMotionTrailCount = 0;
    }
    else if (theStinky->mGoalX > theStinky->mPosX && theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_LEFT)
    {
        Reanimation* aStinyReanim = mApp->ReanimationGet(theStinky->mGridItemReanimID);
        theStinky->mGridItemState = GridItemState::GRIDITEM_STINKY_TURNING_RIGHT;
        aStinyReanim->PlayReanim(ReanimTrackId::turn, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 6.0f);
        theStinky->mMotionTrailCount = 0;
    }
}

//0x520120
void ZenGarden::StinkyWakeUp(GridItem* theStinky)
{
    Reanimation* aStinkyReanim = mApp->ReanimationGet(theStinky->mGridItemReanimID);
    aStinkyReanim->PlayReanim(ReanimTrackId::anim_out, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 6.0f);
    theStinky->mGridItemState = GridItemState::GRIDITEM_STINKY_WAKING_UP;

    Reanimation* aSleepingReanim = FindReanimAttachment(aStinkyReanim->GetTrackInstanceByName(ReanimTrackId::shell)->mAttachmentID);
    aSleepingReanim->ReanimationDie();

    gLawnApp->mPlayerInfo->mHasWokenStinky = TRUE;
}

//0x5201D0
void ZenGarden::StinkyStartFallingAsleep(GridItem* theStinky)
{
    Reanimation* aStinkyReanim = mApp->ReanimationGet(theStinky->mGridItemReanimID);
    aStinkyReanim->PlayReanim(ReanimTrackId::anim_in, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 20, 6.0f);
    theStinky->mGridItemState = GridItemState::GRIDITEM_STINKY_FALLING_ASLEEP;
}

//0x520240
void ZenGarden::StinkyFinishFallingAsleep(GridItem* theStinky, int theBlendTime)
{
    Reanimation* aStinkyReanim = mApp->ReanimationGet(theStinky->mGridItemReanimID);
    aStinkyReanim->PlayReanim(ReanimTrackId::anim_out, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, theBlendTime, 0.0f);
    aStinkyReanim->ApplyRate(0.0f);

    Reanimation* aSleepingReanim = mApp->AddReanimation(0.0f, 0.0f, 0, ReanimationType::REANIM_SLEEPING, mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, ReanimTimeDomain::Visual);
    aSleepingReanim->ApplyRate(3.0f);
    aSleepingReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
    AttachReanim(aStinkyReanim->GetTrackInstanceByName(ReanimTrackId::shell)->mAttachmentID, aSleepingReanim, 34.0f, 39.0f);

    theStinky->mGridItemState = GridItemState::GRIDITEM_STINKY_SLEEPING;
    if (!gLawnApp->mPlayerInfo->mHasWokenStinky)
    {
        mApp->mBoard->DisplayAdvice(gStr.ADVICE_STINKY_SLEEPING, MessageStyle::MESSAGE_STYLE_HINT_LONG, AdviceType::ADVICE_STINKY_SLEEPING);
    }
}

//0x5203E0
void ZenGarden::UpdateStinkyMotionTrail(GridItem* theStinky, bool theStinkyHighOnChocolate)
{
    Reanimation* aStinkyReanim = mApp->ReanimationGet(theStinky->mGridItemReanimID);
    if (!theStinkyHighOnChocolate)
    {
        theStinky->mMotionTrailCount = 0;
        return;
    }
    if (theStinky->mGridItemState != GridItemState::GRIDITEM_STINKY_WALKING_RIGHT && theStinky->mGridItemState != GridItemState::GRIDITEM_STINKY_WALKING_LEFT)
    {
        theStinky->mMotionTrailCount = 0;
        return;
    }

    if (theStinky->mMotionTrailCount == NUM_MOTION_TRAIL_FRAMES)
    {
        theStinky->mMotionTrailCount--;
    }
    if (theStinky->mMotionTrailCount > 0)
    {
        memmove(theStinky->mMotionTrailFrames + 1, theStinky->mMotionTrailFrames, theStinky->mMotionTrailCount * sizeof(MotionTrailFrame));
    }

    theStinky->mMotionTrailFrames[0].mPosX = theStinky->mPosX;
    theStinky->mMotionTrailFrames[0].mPosY = theStinky->mPosY;
    // Only the persisted trail representation needs the old source-frame scale.
    theStinky->mMotionTrailFrames[0].mAnimClock = ReanimClock::SignedMulDiv(
        aStinkyReanim->SamplePlayback().phaseNs,
        std::int64_t(aStinkyReanim->mFrameCount) * ReanimClock::Frame, aStinkyReanim->mPeriodNs);
    theStinky->mMotionTrailCount++;
}

//0x520470
void ZenGarden::StinkyAnimRateUpdate(GridItem* theStinky)
{
    Reanimation* aStinkyReanim = mApp->ReanimationGet(theStinky->mGridItemReanimID);
    if (IsStinkyHighOnChocolate())
    {
        if (theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_LEFT ||
            theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_RIGHT ||
            theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_TURNING_RIGHT ||
            theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_TURNING_LEFT)
        {
            aStinkyReanim->ApplyRate(12.0f);
        }
    }
    else
    {
        if (theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_LEFT ||
            theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_RIGHT ||
            theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_TURNING_RIGHT ||
            theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_TURNING_LEFT)
        {
            aStinkyReanim->ApplyRate(6.0f);
        }
    }
}

void ZenGarden::ResetStinkyTimers()
{
    mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_STINKY_THE_SNAIL] = 2;
    mApp->mPlayerInfo->mLastStinkyChocolateTime = 0;
}

//0x520500
void ZenGarden::StinkyUpdate(GridItem* theStinky)
{
    const auto oldDeadline=theStinky->mStateEndTimeNs;
    Reanimation* aStinkyReanim = mApp->ReanimationGet(theStinky->mGridItemReanimID);

	const long long aNow = mFrameTime;
    const auto visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
    mApp->mEffectSystem->mReanimationHolder->BindAnimationTime(*aStinkyReanim, ReanimTimeDomain::Visual, visualNowNs);
    // Consume this visit's supplied visual instant before changing rates or
    // sampling root motion; GridItem's later track update keeps this interval.
    aStinkyReanim->PrepareTimeAt(visualNowNs);
    if (StinkyTimestamp::DecodeTime(mApp->mPlayerInfo->mLastStinkyChocolateTime) > aNow ||
        StinkyTimestamp::DecodeTime(mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_STINKY_THE_SNAIL]) > aNow)
    {
        ResetStinkyTimers();
    }

    bool aStinkyHighOnChocolate = IsStinkyHighOnChocolate();
    UpdateStinkyMotionTrail(theStinky, aStinkyHighOnChocolate);

    if (theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_FALLING_ASLEEP)
    {
        if (aStinkyReanim->mLoopCount > 0)
        {
            StinkyFinishFallingAsleep(theStinky, 20);
        }
        if(theStinky->mStateEndTimeNs==oldDeadline && oldDeadline!=0)
            theStinky->mStateEndTimeNs+=static_cast<std::int64_t>(Sexy::gUpdateIntervalNs);
        return;
    }

    if (theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_SLEEPING)
    {
        Reanimation* aSleepingReanim = FindReanimAttachment(aStinkyReanim->GetTrackInstanceByName(ReanimTrackId::shell)->mAttachmentID);
        TOD_ASSERT(aSleepingReanim);

        if (mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_CHOCOLATE)
        {
            aSleepingReanim->AssignRenderGroupToRange(ReanimTrackRanges::z, RENDER_GROUP_HIDDEN);
        }
        else
        {
            aSleepingReanim->AssignRenderGroupToRange(ReanimTrackRanges::z, RENDER_GROUP_NORMAL);
        }

        if (ShouldStinkyBeAwake())
        {
            StinkyWakeUp(theStinky);
        }
        if(theStinky->mStateEndTimeNs==oldDeadline && oldDeadline!=0)
            theStinky->mStateEndTimeNs+=static_cast<std::int64_t>(Sexy::gUpdateIntervalNs);
        return;
    }

    if (theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WAKING_UP)
    {
        if (aStinkyReanim->mLoopCount > 0)
        {
            theStinky->mGridItemState = GridItemState::GRIDITEM_STINKY_WALKING_LEFT;
            aStinkyReanim->PlayReanim(ReanimTrackId::anim_crawl, ReanimLoopType::REANIM_LOOP, 10, 6.0f);
            StinkyPickGoal(theStinky);
        }
        if(theStinky->mStateEndTimeNs==oldDeadline && oldDeadline!=0)
            theStinky->mStateEndTimeNs+=static_cast<std::int64_t>(Sexy::gUpdateIntervalNs);
        return;
    }

    if (!ShouldStinkyBeAwake())
    {
        if (theStinky->mPosY < STINKY_SLEEP_POS_Y)
        {
            if (theStinky->mGoalY != STINKY_SLEEP_POS_Y)
            {
                theStinky->mGoalY = STINKY_SLEEP_POS_Y + 10.0f;
            }
        }
        else
        {
            if (theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_LEFT)
            {
                StinkyStartFallingAsleep(theStinky);
                if(theStinky->mStateEndTimeNs==oldDeadline && oldDeadline!=0)
                    theStinky->mStateEndTimeNs+=static_cast<std::int64_t>(Sexy::gUpdateIntervalNs);
                return;
            }
            else if (theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_RIGHT)
            {
                Reanimation* aReanim = mApp->ReanimationGet(theStinky->mGridItemReanimID);
                theStinky->mGridItemState = GridItemState::GRIDITEM_STINKY_TURNING_LEFT;
                aReanim->PlayReanim(ReanimTrackId::turn, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 10, 6.0f);
                theStinky->mMotionTrailCount = 0;
                theStinky->mGoalX = theStinky->mPosX;
                theStinky->mGoalY = theStinky->mPosY;
                if(theStinky->mStateEndTimeNs==oldDeadline && oldDeadline!=0)
                    theStinky->mStateEndTimeNs+=static_cast<std::int64_t>(Sexy::gUpdateIntervalNs);
                return;
            }
        }
    }


    Coin* aCoin = nullptr;
    while (mBoard->IterateCoins(aCoin))
    {
        if (!aCoin->mIsBeingCollected && Distance2D(aCoin->mPosX, aCoin->mPosY + 30.0f, theStinky->mPosX, theStinky->mPosY) < 20.0f)
        {
            aCoin->PlayCollectSound();
            aCoin->Collect();
        }
    }

    if (theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_LEFT || theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_RIGHT)
    {
        if (mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_CHOCOLATE && !IsStinkyHighOnChocolate())
        {
            if (!aStinkyReanim->IsAnimPlaying(ReanimTrackId::anim_idle))
            {
                aStinkyReanim->PlayReanim(ReanimTrackId::anim_idle, ReanimLoopType::REANIM_LOOP, 10, 6.0f);
            }
        }
        else if (!aStinkyReanim->IsAnimPlaying(ReanimTrackId::anim_crawl))
        {
            aStinkyReanim->PlayReanim(ReanimTrackId::anim_crawl, ReanimLoopType::REANIM_LOOP, 10, 6.0f);
        }
    }

    StinkyAnimRateUpdate(theStinky);
    // Only crawling moves Stinky. A turn has a stationary _ground track, so
    // applying chocolate's minimum crawling speed to it has no root cycle.
    if (theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_LEFT || theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_RIGHT)
    {
        float aDeltaX = theStinky->mPosX - theStinky->mGoalX;
        float aDeltaY = theStinky->mPosY - theStinky->mGoalY;
        float aSpeedY = 0.5f;
        const int direction = theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_LEFT ? -1 : 1;
        const auto currentX = theStinky->mPosX;
        const auto rootAt = [aStinkyReanim, aStinkyHighOnChocolate](std::int64_t offset) {
            return aStinkyHighOnChocolate ?
                aStinkyReanim->RootMotionClamped(ReanimTrackId::_ground, 15.0f, 50.0f, offset) :
                aStinkyReanim->RootMotion(ReanimTrackId::_ground, 15.0f, offset);
        };
        const auto rootNow = rootAt(0);
        if (theStinky->mRootReanimID != theStinky->mGridItemReanimID ||
            theStinky->mRootGeneration != aStinkyReanim->mPlaybackGeneration ||
            theStinky->mRootDirection != direction ||
            theStinky->mRootPeriodNs != aStinkyReanim->mPeriodNs ||
            theStinky->mRootLastOutputX != currentX ||
            theStinky->mRootChocolate != aStinkyHighOnChocolate)
        {
            theStinky->mRootWorldX = currentX;
            theStinky->mRootSample = rootNow;
            theStinky->mRootReanimID = theStinky->mGridItemReanimID;
            theStinky->mRootGeneration = aStinkyReanim->mPlaybackGeneration;
            theStinky->mRootDirection = direction;
            theStinky->mRootPeriodNs = aStinkyReanim->mPeriodNs;
            theStinky->mRootChocolate = aStinkyHighOnChocolate;
        }
        auto aMoveX = rootNow - theStinky->mRootSample;
        if (aStinkyHighOnChocolate)
        {
            aSpeedY = 1.0f;
        }
        else if (mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_CHOCOLATE)
        {
            aSpeedY = 0.0f;
            aMoveX = 0;
            theStinky->mRootWorldX = currentX;
            theStinky->mRootSample = rootNow;
        }
        aSpeedY *= TodAnimateCurveFloat(20.0, 5.0, fabs(aDeltaY), 1.0f, 0.2f, Curves::Linear);
        if (theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_LEFT)
        {
            theStinky->mPosX = CheckedPosition(theStinky->mRootWorldX - aMoveX);
            if (theStinky->mPosX < theStinky->mGoalX)
            {
                theStinky->mPosX = theStinky->mGoalX;
            }
        }
        else if (theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_RIGHT)
        {
            theStinky->mPosX = CheckedPosition(theStinky->mRootWorldX + aMoveX);
            if (theStinky->mPosX > theStinky->mGoalX)
            {
                theStinky->mPosX = theStinky->mGoalX;
            }
        }
        theStinky->mRootLastOutputX = theStinky->mPosX;

        if (fabs(aDeltaY) < aSpeedY)
        {
            theStinky->mPosY = theStinky->mGoalY;
        }
        else if (aDeltaY > 0.0f)
        {
            theStinky->mPosY -= Sexy::UpdateDelta(aSpeedY);
        }
        else
        {
            theStinky->mPosY += Sexy::UpdateDelta(aSpeedY);
        }

        if (fabs(aDeltaX) < 5.0f && fabs(aDeltaY) < 5.0f)
        {
            StinkyPickGoal(theStinky);
        }
        else if (theStinky->mStateEndTimeNs <= mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs)
        {
            StinkyPickGoal(theStinky);
        }
    }

    if (theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_TURNING_LEFT)
    {
        if (aStinkyReanim->mLoopCount > 0)
        {
            theStinky->mGridItemState = GridItemState::GRIDITEM_STINKY_WALKING_LEFT;
            aStinkyReanim->PlayReanim(ReanimTrackId::anim_crawl, ReanimLoopType::REANIM_LOOP, 10, 6.0f);
        }
    }
    else if (theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_TURNING_RIGHT)
    {
        if (aStinkyReanim->mLoopCount > 0)
        {
            theStinky->mGridItemState = GridItemState::GRIDITEM_STINKY_WALKING_RIGHT;
            aStinkyReanim->PlayReanim(ReanimTrackId::anim_crawl, ReanimLoopType::REANIM_LOOP, 10, 6.0f);
        }
    }

    if (theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_WALKING_RIGHT || theStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_TURNING_LEFT)
    {
        aStinkyReanim->mOverlayMatrix.m00 = -0.8f;
        aStinkyReanim->mOverlayMatrix.m11 = 0.8f;
        aStinkyReanim->mOverlayMatrix.m02 = theStinky->mPosX + 69.0f;
        aStinkyReanim->mOverlayMatrix.m12 = theStinky->mPosY;
    }
    else
    {
        aStinkyReanim->mOverlayMatrix.m00 = 0.8f;
        aStinkyReanim->mOverlayMatrix.m11 = 0.8f;
        aStinkyReanim->mOverlayMatrix.m02 = theStinky->mPosX;
        aStinkyReanim->mOverlayMatrix.m12 = theStinky->mPosY;
    }

    theStinky->mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_PLANT, 0, theStinky->mPosY - 30.0f);
}

//0x520AC0
void ZenGarden::ZenToolUpdate(GridItem* theZenTool)
{
    Reanimation* aToolReanim = mApp->ReanimationTryToGet(theZenTool->mGridItemReanimID);
    if (aToolReanim == nullptr)
    {
        return;
    }

    int aPlayTime = 1;
    if (theZenTool->mGridItemState == GridItemState::GRIDITEM_STATE_ZEN_TOOL_PHONOGRAPH)
    {
        aPlayTime = 2;
    }
    if (aToolReanim->mLoopCount >= aPlayTime)
    {
        DoFeedingTool(theZenTool->mPosX, theZenTool->mPosY, theZenTool->mGridItemState);
        theZenTool->GridItemDie();
    }
}

//0x520B30
void ZenGarden::ZenGardenUpdate()
{
    if (mApp->GetDialog(Dialogs::DIALOG_STORE))
    {
        return;
    }

    mApp->UpdateCrazyDave();
    if (mBoard->mCursorObject->mCursorType != CursorType::CURSOR_TYPE_NORMAL)
    {
        mBoard->mChallenge->mChallengeState = ChallengeState::STATECHALLENGE_NORMAL;
        mBoard->mChallenge->mChallengeStateCounter = 3000;
    }
    else if (mApp->mBoard->mTutorialState == TutorialState::TUTORIAL_OFF)
    {
        if (mBoard->mChallenge->mChallengeStateCounter > 0)
        {
            mBoard->mChallenge->mChallengeStateCounter--;
        }
        if (mBoard->mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_NORMAL && mBoard->mChallenge->mChallengeStateCounter == 0)
        {
            mBoard->mChallenge->mChallengeState = ChallengeState::STATECHALLENGE_ZEN_FADING;
            mBoard->mChallenge->mChallengeStateCounter = 50;
        }
    }

    UpdatePlantNeeds();
    {
        Plant* aPlant = nullptr;
        while (mBoard->IteratePlants(aPlant))
        {
            if (aPlant->mPottedPlantIndex != -1)
            {
                PottedPlantUpdate(aPlant);
            }
        }
    }
    {
        GridItem* aGridItem = nullptr;
        while (mBoard->IterateGridItems(aGridItem))
        {
            if (aGridItem->mGridItemType == GridItemType::GRIDITEM_ZEN_TOOL)
            {
                ZenToolUpdate(aGridItem);
            }
            else if (aGridItem->mGridItemType == GridItemType::GRIDITEM_STINKY)
            {
                StinkyUpdate(aGridItem);
            }
        }
    }

    if (mBoard->mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_KEEP_WATERING && CountPlantsNeedingFertilizer() > 0)
    {
        mBoard->DisplayAdvice(gStr.ADVICE_ZEN_GARDEN_VISIT_STORE, MessageStyle::MESSAGE_STYLE_HINT_TALL_LONG, AdviceType::ADVICE_NONE);
        mBoard->mTutorialState = TutorialState::TUTORIAL_ZEN_GARDEN_VISIT_STORE;
        mBoard->mStoreButton->mDisabled = false;
        mBoard->mStoreButton->mBtnNoDraw = false;
    }
}

//0x520CF0
GridItem* ZenGarden::GetStinky()
{
    GridItem* aGridItem = nullptr;
    while (mBoard->IterateGridItems(aGridItem))
    {
        if (aGridItem->mGridItemType == GridItemType::GRIDITEM_STINKY)
        {
            return aGridItem;
        }
    }
    return nullptr;
}

//0x520D30
void ZenGarden::GotoNextGarden()
{
    LeaveGarden();
    mBoard->ClearAdvice(AdviceType::ADVICE_NONE);
	mApp->CrazyDaveDie();
    mBoard->mPlants.DataArrayFreeAll();
    mBoard->RebuildPlantCellBucket();  // 格桶：池清空后桶必须同步清
    mBoard->mCoins.DataArrayFreeAll();
    mApp->mEffectSystem->EffectSystemFreeAll();

    bool aGoToTree = false;
    if (mGardenType == GardenType::GARDEN_MAIN)
    {
        if (mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_MUSHROOM_GARDEN])
        {
            mGardenType = GardenType::GARDEN_MUSHROOM;
            mBoard->mBackground = BackgroundType::BACKGROUND_MUSHROOM_GARDEN;
        }
        else if (mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_AQUARIUM_GARDEN])
        {
            mGardenType = GardenType::GARDEN_AQUARIUM;
            mBoard->mBackground = BackgroundType::BACKGROUND_ZOMBIQUARIUM;
        }
        else if (mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_TREE_OF_WISDOM])
        {
            aGoToTree = true;
        }
    }
    else if (mGardenType == GardenType::GARDEN_MUSHROOM)
    {
        if (mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_AQUARIUM_GARDEN])
        {
            mGardenType = GardenType::GARDEN_AQUARIUM;
            mBoard->mBackground = BackgroundType::BACKGROUND_ZOMBIQUARIUM;
        }
        else if (mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_TREE_OF_WISDOM])
        {
            aGoToTree = true;
        }
        else
        {
            mGardenType = GardenType::GARDEN_MAIN;
            mBoard->mBackground = BackgroundType::BACKGROUND_GREENHOUSE;
        }
    }
    else if (mGardenType == GardenType::GARDEN_AQUARIUM)
    {
        if (mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_TREE_OF_WISDOM])
        {
            aGoToTree = true;
        }
        else
        {
            mGardenType = GardenType::GARDEN_MAIN;
            mBoard->mBackground = BackgroundType::BACKGROUND_GREENHOUSE;
        }
    }
    if (aGoToTree)
    {
        mApp->KillBoard();
        mApp->PreNewGame(GameMode::GAMEMODE_TREE_OF_WISDOM, false);
        return;
    }

    if (mBoard->mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN)
    {
        TodLoadResources(ResourceGroupId::DelayLoad_MushroomGarden);
    }
    else if (mBoard->mBackground == BackgroundType::BACKGROUND_GREENHOUSE)
    {
        TodLoadResources(ResourceGroupId::DelayLoad_GreenHouseGarden);
        TodLoadResources(ResourceGroupId::DelayLoad_GreenHouseOverlay);
    }
    else if (mBoard->mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
    {
        TodLoadResources(ResourceGroupId::DelayLoad_Zombiquarium);
        TodLoadResources(ResourceGroupId::DelayLoad_GreenHouseOverlay);
    }
    else
    {
        TOD_ASSERT(false);
    }

    if ((mBoard->mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN || mBoard->mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM))
    {
        if (!mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_WHEEL_BARROW])
        {
            mBoard->DisplayAdvice(gStr.ADVICE_NEED_WHEELBARROW, MessageStyle::MESSAGE_STYLE_HINT_TALL_FAST, AdviceType::ADVICE_NEED_WHEELBARROW);
        }
    }

    ZenGardenInitLevel();
}

//0x5210F0
void ZenGarden::MouseDownWithFullWheelBarrow(int x, int y)
{
    PottedPlant* aPottedPlant = GetPottedPlantInWheelbarrow();
    TOD_ASSERT(aPottedPlant);

    if (mApp->mZenGarden->mGardenType == GardenType::GARDEN_AQUARIUM && !Plant::IsAquatic(aPottedPlant->mSeedType))
    {
        mBoard->DisplayAdvice(gStr.ZEN_ONLY_AQUATIC_PLANTS, MessageStyle::MESSAGE_STYLE_HINT_TALL_FAST, AdviceType::ADVICE_NONE);
        return;
    }

    int aGridX = mBoard->PixelToGridX(x, y);
    int aGridY = mBoard->PixelToGridY(x, y);
    if (aGridX == -1 || aGridY == -1 || mBoard->CanPlantAt(aGridX, aGridY, aPottedPlant->mSeedType) != PlantingReason::PLANTING_OK)
    {
        return;
    }

    aPottedPlant->mWhichZenGarden = mGardenType;
    aPottedPlant->mX = aGridX;
    aPottedPlant->mY = aGridY;
    intptr_t aPottedPlantIndex = ((intptr_t)aPottedPlant - (intptr_t)mApp->mPlayerInfo->mPottedPlant) / (intptr_t)sizeof(PottedPlant);
    Plant* aPlant = PlacePottedPlant(aPottedPlantIndex);
    mBoard->DoPlantingEffects(aPottedPlant->mX, aPottedPlant->mY, aPlant);
}

//0x521280
void ZenGarden::MouseDownWithEmptyWheelBarrow(Plant* thePlant)
{
    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
    RemovePottedPlant(thePlant);
    aPottedPlant->mWhichZenGarden = GardenType::GARDEN_WHEELBARROW;
    aPottedPlant->mX = 0;
    aPottedPlant->mY = 0;
    mApp->PlayFoley(FoleyType::FOLEY_PLANT);
}

//0x521310
PottedPlant* ZenGarden::GetPottedPlantInWheelbarrow()
{
    for (int i = 0; i < mApp->mPlayerInfo->mNumPottedPlants; i++)
    {
        PottedPlant* aPottedPlant = PottedPlantFromIndex(i);
        if (aPottedPlant->mWhichZenGarden == GardenType::GARDEN_WHEELBARROW)
        {
            return aPottedPlant;
        }
    }
    return nullptr;
}

std::span<const SpecialGridPlacement> ZenGarden::GetSpecialGridPlacements()
{
    if (!mBoard)
        throw std::logic_error("ZenGarden grid lookup requires an active board");
    switch (mBoard->mBackground)
    {
    case BackgroundType::BACKGROUND_MUSHROOM_GARDEN:
        return gMushroomGridPlacement;
    case BackgroundType::BACKGROUND_ZOMBIQUARIUM:
        return gAquariumGridPlacement;
    case BackgroundType::BACKGROUND_GREENHOUSE:
        return gGreenhouseGridPlacement;
    default:
        throw std::logic_error("ZenGarden grid lookup requires a garden background");
    }
}

//0x521350
int ZenGarden::PixelToGridX(int theX, int theY)
{
    for (const SpecialGridPlacement& aGrid : GetSpecialGridPlacements())
    {
        if (theX >= aGrid.mPixelX && theX <= aGrid.mPixelX + 80 && theY >= aGrid.mPixelY && theY <= aGrid.mPixelY + 85)
        {
            return aGrid.mGridX;
        }
    }
    return -1;
}

//0x5213D0
int ZenGarden::PixelToGridY(int theX, int theY)
{
    for (const SpecialGridPlacement& aGrid : GetSpecialGridPlacements())
    {
        if (theX >= aGrid.mPixelX && theX <= aGrid.mPixelX + 80 && theY >= aGrid.mPixelY && theY <= aGrid.mPixelY + 85)
        {
            return aGrid.mGridY;
        }
    }
    return -1;
}

//0x521450
int ZenGarden::GridToPixelX(int theGridX, int theGridY)
{
    for (const SpecialGridPlacement& aGrid : GetSpecialGridPlacements())
    {
        if (theGridX == aGrid.mGridX && theGridY == aGrid.mGridY)
        {
            return aGrid.mPixelX;
        }
    }
    return -1;
}

//0x5214C0
int ZenGarden::GridToPixelY(int theGridX, int theGridY)
{
    for (const SpecialGridPlacement& aGrid : GetSpecialGridPlacements())
    {
        if (theGridX == aGrid.mGridX && theGridY == aGrid.mGridY)
        {
            return aGrid.mPixelY;
        }
    }
    return -1;
}

//0x521530
void ZenGarden::DrawBackdrop(Graphics* g)
{
    if (mGardenType != GardenType::GARDEN_AQUARIUM)
    {
        return;
    }

    if (mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW ||
        mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_WHEEELBARROW ||
        mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE)
    {
        for (const SpecialGridPlacement& aGrid : GetSpecialGridPlacements())
        {
            if (mBoard->GetTopPlantAt(aGrid.mGridX, aGrid.mGridY, PlantPriority::TOPPLANT_ZEN_TOOL_ORDER) == nullptr)
            {
                TodDrawImageRectScaled(g, IMAGE_PLANTSHADOW, aGrid.mPixelX - 35, (aGrid.mPixelY + 33), Rect(0, 0, IMAGE_PLANTSHADOW->mWidth, IMAGE_PLANTSHADOW->mHeight), 1.7f, 1.7f);
            }
        }
    }
}

//0x521660
void ZenGarden::ShowTutorialArrowOnWateringCan()
{
    Rect aButtonRect = mBoard->GetShovelButtonRect();
    mBoard->GetZenButtonRect(GameObjectType::OBJECT_TYPE_WATERING_CAN, aButtonRect);
    mBoard->TutorialArrowShow(aButtonRect.mX + 10, aButtonRect.mY + 10);
    mBoard->DisplayAdvice(gStr.ADVICE_ZEN_GARDEN_PICK_UP_WATER, MessageStyle::MESSAGE_STYLE_ZEN_GARDEN_LONG, AdviceType::ADVICE_NONE);
    mBoard->mTutorialState = TutorialState::TUTORIAL_ZEN_GARDEN_PICKUP_WATER;
}

//0x5217A0
void ZenGarden::AdvanceCrazyDaveDialog()
{
    if (mApp->mCrazyDaveMessageIndex == -1 || mApp->GetDialog(Dialogs::DIALOG_STORE) || mApp->GetDialog(Dialogs::DIALOG_ZEN_SELL))
    {
        return;
    }

    if (mApp->mCrazyDaveMessageIndex == 2104)
    {
        ShowTutorialArrowOnWateringCan();
    }

    if (!mApp->AdvanceCrazyDaveText())
    {
        mApp->CrazyDaveLeave();
        return;
    }

    if (mApp->mCrazyDaveMessageIndex == 2102 && mApp->mPlayerInfo->mNumPottedPlants == 0)
    {
        for (int i = 0; i < 2; i++)
        {
            PottedPlant aPottedPlant;
            aPottedPlant.InitializePottedPlant(SeedType::SEED_MARIGOLD);
            aPottedPlant.mDrawVariation = (DrawVariation)RandRangeInt((int)DrawVariation::VARIATION_MARIGOLD_WHITE, (int)DrawVariation::VARIATION_MARIGOLD_LIGHT_GREEN);
            aPottedPlant.mFeedingsPerGrow = 3;
            mApp->mZenGarden->AddPottedPlant(&aPottedPlant);
        }
    }
}

//0x521880
bool ZenGarden::MouseDownZenGarden(int x, int y, int theClickCount, HitResult* theHitResult)
{
    if (mBoard->mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_ZEN_FADING)
    {
        mBoard->mChallenge->mChallengeState = ChallengeState::STATECHALLENGE_NORMAL;
    }
    mBoard->mChallenge->mChallengeStateCounter = 3000;

    if (theHitResult->mObjectType == GameObjectType::OBJECT_TYPE_STINKY && mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_NORMAL)
    {
        WakeStinky();
    }
    else if (mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_GLOVE)
    {
        if (mBoard->CanUseGameObject(GameObjectType::OBJECT_TYPE_WHEELBARROW))
        {
            Rect aButtonRect = mBoard->GetShovelButtonRect();
            mBoard->GetZenButtonRect(GameObjectType::OBJECT_TYPE_WHEELBARROW, aButtonRect);

            PottedPlant* aPottedPlant = GetPottedPlantInWheelbarrow();
            if (aButtonRect.Contains(x, y) && aPottedPlant)
            {
                mBoard->ClearCursor();
                mBoard->mCursorObject->mType = aPottedPlant->mSeedType;
                mBoard->mCursorObject->mImitaterType = SeedType::SEED_NONE;
                mBoard->mCursorObject->mCursorType = CursorType::CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW;
                return true;
            }
        }
    }
    else if (mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE)
    {
        if (mBoard->CanUseGameObject(GameObjectType::OBJECT_TYPE_WHEELBARROW))
        {
            Rect aButtonRect = mBoard->GetShovelButtonRect();
            mBoard->GetZenButtonRect(GameObjectType::OBJECT_TYPE_WHEELBARROW, aButtonRect);

            Plant* aPlant = mBoard->mPlants.DataArrayTryToGet(mBoard->mCursorObject->mGlovePlantID);
            if (aPlant && aButtonRect.Contains(x, y) && GetPottedPlantInWheelbarrow() == nullptr)
            {
                MouseDownWithEmptyWheelBarrow(aPlant);
                mBoard->ClearCursor();
                return true;
            }
        }
    }
    else if (theHitResult->mObjectType == GameObjectType::OBJECT_TYPE_NONE && mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_NORMAL &&
        mGardenType == GardenType::GARDEN_AQUARIUM && theClickCount <= -1)
    {
        mApp->PlaySample(SOUND_TAPGLASS);
    }

    if (mApp->mCrazyDaveMessageIndex != -1)
    {
        AdvanceCrazyDaveDialog();
        return true;
    }

    return false;
}

//0x521AC0
void ZenGarden::SetPlantAnimSpeed(Plant* thePlant)
{
    Reanimation* aBodyReanim = mApp->ReanimationGet(thePlant->mBodyReanimID);
    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
    bool aPlantHighOnChocolate = PlantHighOnChocolate(aPottedPlant);
    bool aPlantAtHighRate = aBodyReanim->mAnimRate >= 25.0f;
    if (aPlantAtHighRate == aPlantHighOnChocolate)
    {
        return;
    }

    float aTargetRate;
    if (thePlant->mSeedType == SeedType::SEED_PEASHOOTER || thePlant->mSeedType == SeedType::SEED_SNOWPEA || thePlant->mSeedType == SeedType::SEED_REPEATER ||
        thePlant->mSeedType == SeedType::SEED_LEFTPEATER || thePlant->mSeedType == SeedType::SEED_GATLINGPEA || thePlant->mSeedType == SeedType::SEED_SPLITPEA ||
        thePlant->mSeedType == SeedType::SEED_THREEPEATER || thePlant->mSeedType == SeedType::SEED_MARIGOLD)
    {
        aTargetRate = RandRangeFloat(15.0f, 20.0f);
    }
    else if (thePlant->mSeedType == SeedType::SEED_POTATOMINE)
    {
        aTargetRate = 12.0f;
    }
    else
    {
        aTargetRate = RandRangeFloat(10.0f, 15.0f);
    }

    if (aPlantHighOnChocolate)
    {
        aTargetRate *= 2.0f;
        aTargetRate = std::max(25.0f, aTargetRate);
    }

    aBodyReanim->ApplyRate(aTargetRate);
    Reanimation* aHeadReanim = mApp->ReanimationTryToGet(thePlant->mHeadReanimID);
    Reanimation* aHeadReanim2 = mApp->ReanimationTryToGet(thePlant->mHeadReanimID2);
    Reanimation* aHeadReanim3 = mApp->ReanimationTryToGet(thePlant->mHeadReanimID3);
    if (aHeadReanim)
    {
        aHeadReanim->ApplyRate(aBodyReanim->mAnimRate);
        aHeadReanim->SynchronizePhase(*aBodyReanim);
    }
    if (aHeadReanim2)
    {
        aHeadReanim2->ApplyRate(aBodyReanim->mAnimRate);
        aHeadReanim2->SynchronizePhase(*aBodyReanim);
    }
    if (aHeadReanim3)
    {
        aHeadReanim3->ApplyRate(aBodyReanim->mAnimRate);
        aHeadReanim3->SynchronizePhase(*aBodyReanim);
    }
}

//0x521D40
void ZenGarden::PlantUpdateProduction(Plant* thePlant)
{
    thePlant->mZenProductionCountdown--;
    SetPlantAnimSpeed(thePlant);
    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
    if (PlantHighOnChocolate(aPottedPlant))
    {
        thePlant->mZenProductionCountdown--;
    }

    if (thePlant->mZenProductionCountdown <= 0)
    {
        PlantSetLaunchCounter(thePlant);
        mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);

        int aCoinHit = Rand(1000);
        const uint64_t aElapsedSeconds = mFrameTime > aPottedPlant->mLastNeedFulfilledTime && !PlantHighOnChocolate(aPottedPlant)
            ? static_cast<uint64_t>(mFrameTime) - static_cast<uint64_t>(aPottedPlant->mLastNeedFulfilledTime) : 0;
        const int64_t aAgeNs = static_cast<int64_t>(aElapsedSeconds > 1800 ? 1800 : aElapsedSeconds) * 1'000'000'000LL;
        aCoinHit += TodAnimateCurveNs(300'000'000'000LL, 1'800'000'000'000LL, aAgeNs, 0, 80, Curves::Linear);
        CoinType aCoinType = CoinType::COIN_SILVER;
        if (aCoinHit < 100)
        {
            aCoinType = CoinType::COIN_GOLD;
        }
        mBoard->AddCoin((thePlant->mX), (thePlant->mY), aCoinType, CoinMotion::COIN_MOTION_COIN);
    }
}

//0x521E70
void ZenGarden::PottedPlantUpdate(Plant* thePlant)
{
    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
	const long long aNow = mFrameTime;
    if (aPottedPlant->mLastWateredTime > aNow ||
        aPottedPlant->mLastNeedFulfilledTime > aNow ||
        aPottedPlant->mLastFertilizedTime > aNow ||
        aPottedPlant->mLastChocolateTime > aNow)
    {
        aPottedPlant->mLastWateredTime = 0;
        aPottedPlant->mLastNeedFulfilledTime = 0;
        aPottedPlant->mLastFertilizedTime = 0;
        aPottedPlant->mLastChocolateTime = 0;
    }

    if (thePlant->mIsAsleep)
    {
        return;
    }
    if (thePlant->mStateCountdown > 0)
    {
        thePlant->mStateCountdown--;
    }
    if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_FULL && WasPlantNeedFulfilledToday(aPottedPlant))
    {
        PlantUpdateProduction(thePlant);
    }
    UpdatePlantEffectState(thePlant);
}

namespace
{
    // ZenGarden::DrawPlantOverlay 需求图标表：原 switch 五 case 中三槽同构
    // IMAGE_ZEN_NEED_ICONS 的 30x30 图标（行序 = PLANTNEED_
    // FERTILIZER..PHONOGRAPH 枚举段序 0..2）；WATER/NONE 不入表（见分发点）。
    struct ZenNeedIcon
    {
        int mX;
        int mCel;
    };
    static constexpr ZenNeedIcon kZenNeedIcon[(int)PottedPlantNeed::PLANTNEED_PHONOGRAPH -
                                       (int)PottedPlantNeed::PLANTNEED_FERTILIZER + 1] = {
        {61, 0},  // 0 PLANTNEED_FERTILIZER
        {61, 1},  // 1 PLANTNEED_BUGSPRAY
        {60, 2},  // 2 PLANTNEED_PHONOGRAPH
    };
    static_assert(sizeof(kZenNeedIcon) / sizeof(kZenNeedIcon[0]) ==
        (int)PottedPlantNeed::PLANTNEED_PHONOGRAPH - (int)PottedPlantNeed::PLANTNEED_FERTILIZER + 1,
        "kZenNeedIcon 槽位须与 PottedPlantNeed 三需求枚举段同步");
}  // namespace

//0x521F30
void ZenGarden::DrawPlantOverlay(Graphics* g, Plant* thePlant)
{
    if (thePlant->mPottedPlantIndex == -1)
    {
        return;
    }

    PottedPlant* aPottedPlant = PottedPlantFromIndex(thePlant->mPottedPlantIndex);
    PottedPlantNeed aPlantNeed = mApp->mZenGarden->GetPlantsNeed(aPottedPlant);
    if (aPlantNeed == PottedPlantNeed::PLANTNEED_NONE)
    {
        return;
    }

    g->DrawImage(IMAGE_PLANTSPEECHBUBBLE, 50, 0);
    // B 形态 -> 常量表（上方 kZenNeedIcon）：FERTILIZER/BUGSPRAY/PHONOGRAPH
    // 三槽同构源矩形；WATER 特例（DrawImage 单帧水滴）表外；NONE 已
    // 提前 return（原 NONE case 为防御空操作）。
    if (aPlantNeed == PottedPlantNeed::PLANTNEED_WATER)
    {
        g->DrawImage(IMAGE_WATERDROP, 67, 9);
    }
    else
    {
        const ZenNeedIcon& aIcon = kZenNeedIcon[(int)aPlantNeed - (int)PottedPlantNeed::PLANTNEED_FERTILIZER];
        g->DrawImage(IMAGE_ZEN_NEED_ICONS, aIcon.mX, 7, Rect(aIcon.mCel * 30, 0, 30, 30));
    }
}

//0x521FE0
void ZenGarden::WakeStinky()
{
    mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_STINKY_THE_SNAIL] = StinkyTimestamp::EncodeWakeTime(std::time(nullptr));
    mApp->PlaySample(SOUND_TAP);
    mBoard->ClearAdvice(AdviceType::ADVICE_STINKY_SLEEPING);
    gLawnApp->mPlayerInfo->mHasWokenStinky = TRUE;
}

//0x522090
bool ZenGarden::IsStinkyHighOnChocolate()
{
	return mFrameTime - StinkyTimestamp::DecodeTime(mApp->mPlayerInfo->mLastStinkyChocolateTime) < 3600;
}

bool ZenGarden::PlantHighOnChocolate(PottedPlant* thePottedPlant)
{
	return mFrameTime - thePottedPlant->mLastChocolateTime < 300;
}

bool ZenGarden::IsStinkySleeping()
{
    GridItem* aStinky = GetStinky();
    return aStinky && aStinky->mGridItemState == GridItemState::GRIDITEM_STINKY_SLEEPING;
}

//0x5220C0
bool ZenGarden::ShouldStinkyBeAwake()
{
    if (IsStinkyHighOnChocolate())
    {
        return true;
    }
	// 1073 0052CA7A/0052CA7F: signed elapsed seconds <= 180.
	return mFrameTime - StinkyTimestamp::DecodeTime(mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_STINKY_THE_SNAIL]) <= 180;
}

//0x522110
void ZenGarden::OpenStore()
{
    LeaveGarden();
    StoreScreen* aStore = mApp->ShowStoreScreen();
    if (mBoard->mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_VISIT_STORE)
    {
        aStore->SetupForIntro(2600);
        mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_FERTILIZER] = PURCHASE_COUNT_OFFSET + 5;
    }
    aStore->mBackButton->mLabel = gStr.STORE_BACK_TO_GAME;
    aStore->mPage = StorePages::STORE_PAGE_ZEN1;
    aStore->WaitForResult(true);

    if (aStore->mGoToTreeNow)
    {
        mApp->KillBoard();
        mApp->PreNewGame(GameMode::GAMEMODE_TREE_OF_WISDOM, false);
    }
    else
    {
        // The store skips garden updates; refresh before recreating Stinky.
        RefreshTimeCache();
        mApp->mMusic->MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_ZEN_GARDEN);
        if (mBoard->mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_VISIT_STORE)
        {
            mBoard->DisplayAdvice(gStr.ADVICE_ZEN_GARDEN_FERTILIZE, MessageStyle::MESSAGE_STYLE_ZEN_GARDEN_LONG, AdviceType::ADVICE_NONE);
            mBoard->mTutorialState = TutorialState::TUTORIAL_ZEN_GARDEN_FERTILIZE_PLANTS;
        }
        AddStinky();
    }
}

//0x5222D0
// GOTY @Patoke: 0x52CC50
void ZenGarden::SetupForZenTutorial()
{
    mBoard->mMenuButton->mLabel = gStr.CONTINUE_BUTTON;
    mBoard->mStoreButton->mDisabled = true;
    mBoard->mStoreButton->mBtnNoDraw = true;
    mBoard->mMenuButton->mDisabled = true;
    mBoard->mMenuButton->mBtnNoDraw = true;

    mApp->CrazyDaveEnter();
    mApp->CrazyDaveTalkIndex(2100);
}

//0x5223B0
SeedType ZenGarden::PickRandomSeedType()
{
    SeedType aSeedList[40];
    int aSeedCount = 0;
    for (int i = 0; i < 40; i++)
    {
        SeedType aSeedType = (SeedType)i;
        if (aSeedType != SeedType::SEED_MARIGOLD && aSeedType != SeedType::SEED_FLOWERPOT)
        {
            aSeedList[aSeedCount] = aSeedType;
            aSeedCount++;
        }
    }
    return TodPickFromArray(aSeedList, aSeedCount);
}

void ZenGarden::CancelPendingSale()
{
    // Invalidate even an accepted/unregistered dialog whose WaitForResult has
    // not unwound yet. Closing registered dialogs alone cannot cancel that case.
    ++mSaleRevision;
    mApp->KillDialog(Dialogs::DIALOG_ZEN_SELL);
}

//0x5223F0
void ZenGarden::LeaveGarden()
{
    CancelPendingSale();
    {
        GridItem* aGridItem = nullptr;
        while (mBoard->IterateGridItems(aGridItem))
        {
            if (aGridItem->mGridItemType == GridItemType::GRIDITEM_ZEN_TOOL)
            {
                DoFeedingTool(aGridItem->mPosX, aGridItem->mPosY, aGridItem->mGridItemState);
                aGridItem->GridItemDie();
            }
            else if (aGridItem->mGridItemType == GridItemType::GRIDITEM_STINKY)
            {
                mApp->mPlayerInfo->mStinkyPosX = aGridItem->mPosX;
                mApp->mPlayerInfo->mStinkyPosY = aGridItem->mPosY;
                aGridItem->GridItemDie();
            }
        }
    }
    {
        Coin* aCoin = nullptr;
        while (mBoard->IterateCoins(aCoin))
        {
            if (aCoin->mIsBeingCollected)
            {
                aCoin->ScoreCoin();
            }
            else
            {
                aCoin->Die();
            }
        }
    }
}
