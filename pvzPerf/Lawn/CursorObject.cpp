#include "Board.h"
#include "Cutscene.h"
#include "ZenGarden.h"
#include "LawnApp.h"
#include "CursorObject.h"
#include "GardenHudLayout.h"
#include "Resources.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "widget/WidgetManager.h"

//0x438640
CursorObject::CursorObject()
{
    mType = SeedType::SEED_NONE;
    mImitaterType = SeedType::SEED_NONE;
    mSeedBankIndex = -1;
    mX = 0;
    mY = 0;
    mCursorType = CursorType::CURSOR_TYPE_NORMAL;
    mCoinID = CoinID::COINID_NULL;
    mDuplicatorPlantID = PlantID::PLANTID_NULL;
    mCobCannonPlantID = PlantID::PLANTID_NULL;
    mGlovePlantID = PlantID::PLANTID_NULL;
    mReanimCursorID = ReanimationID::REANIMATIONID_NULL;
    mHammerDownCounter = 0;
    
    if (mApp->IsWhackAZombieLevel()) CreateHammerAnimation();

    mWidth = 80;
    mHeight = 80;
}

void CursorObject::CreateHammerAnimation()
{
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_HAMMER, true);
    Reanimation* animation = mApp->AddReanimation(-25.0f, 16.0f, 0,
        ReanimationType::REANIM_HAMMER, mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs,
        ReanimTimeDomain::Visual);
    animation->mIsAttachment = true;
    animation->PlayReanim(ReanimTrackId::anim_whack_zombie, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
    animation->Seek(1000);
    mReanimCursorID = mApp->ReanimationGetID(animation);
}

//0x438780
void CursorObject::Update()
{
    if (!mApp->mWidgetManager->mMouseIn)
    {
        mVisible = false;
        return;
    }
    Update(mApp->mWidgetManager->mLastMouseX - mBoard->mX,
        mApp->mWidgetManager->mLastMouseY - mBoard->mY);
}

void CursorObject::Update(int x, int y)
{
    if (mApp->mGameScene != GameScenes::SCENE_PLAYING && !mBoard->mCutScene->IsInShovelTutorial())
    {
        mVisible = false;
        return;
    }

    Reanimation* aCursorReanim = mApp->ReanimationTryToGet(mReanimCursorID);
    if (aCursorReanim)
    {
        aCursorReanim->UpdateAt(mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs);
    }

    mVisible = true;
    // BeginDraw inherits Board's origin; the input position is already local.
    mX = x - 25;
    mY = y - 35;
}

void CursorObject::Die()
{
    mApp->RemoveReanimation(mReanimCursorID);
    mReanimCursorID = ReanimationID::REANIMATIONID_NULL;
}


//0x438820
void CursorObject::Draw(Graphics* g)
{
    Draw(g, mApp->mWidgetManager->mLastMouseX - mBoard->mX,
        mApp->mWidgetManager->mLastMouseY - mBoard->mY);
}

void CursorObject::Draw(Graphics* g, int x, int y)
{
    // CursorObject::Draw 分发表：表内匿名 lambda = 原 case 体逐句迁移（生成器
    // + 逆变换 token 对拍 0 差异）；PLANT_FROM_BANK 三标签共体具名。
    using CursorDrawCaseFn = void (*)(CursorObject&, Graphics* g);

    // 标签共体（匿名 lambda 无法跨槽复用，复用即具名）
    // 原 case CURSOR_TYPE_PLANT_FROM_BANK, CURSOR_TYPE_PLANT_FROM_USABLE_COIN, CURSOR_TYPE_PLANT_FROM_DUPLICATOR
    static constexpr auto CursorDrawCursorTypePlantFromBank = [](CursorObject& c, Graphics* g)
    {
    float aOffsetX = -10.0f;
    float aOffsetY = PlantDrawHeightOffset(c.mBoard ? static_cast<std::int64_t>(c.mBoard->mLevelTimeNs) : c.mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, c.mBoard, nullptr, c.mType, -1, -1) - 10.0f;
    if (Plant::IsFlying(c.mType) || c.mType == SeedType::SEED_GRAVEBUSTER)
    {
    aOffsetY += 30.0f;
    }
    aOffsetY -= 15.0f;
    if (c.mApp->IsIZombieLevel())
    {
    aOffsetX -= 55.0f;
    aOffsetY -= 70.0f;
    }

    Plant::DrawSeedType(g, c.mType, c.mImitaterType, DrawVariation::VARIATION_NORMAL, aOffsetX, aOffsetY);
    };

    // 匿名 lambda 函数指针表，行序 = CursorType 枚举值序（0..17）。
    static constexpr CursorDrawCaseFn kCursorDrawCases[(int)CursorType::CURSOR_TYPE_TREE_FOOD + 1] = {
    // 原 case CURSOR_TYPE_NORMAL
    [](CursorObject& c, Graphics* g)
    {

    },  // 0 CURSOR_TYPE_NORMAL
    CursorDrawCursorTypePlantFromBank,  // 1 CURSOR_TYPE_PLANT_FROM_BANK（共体）
    CursorDrawCursorTypePlantFromBank,  // 2 CURSOR_TYPE_PLANT_FROM_USABLE_COIN（共体）
    // 原 case CURSOR_TYPE_PLANT_FROM_GLOVE
    [](CursorObject& c, Graphics* g)
    {
    Plant* aPlant = c.mBoard->mPlants.DataArrayGet((unsigned int)c.mGlovePlantID);
    PottedPlant* aPottedPlant = &c.mApp->mPlayerInfo->mPottedPlant[aPlant->mPottedPlantIndex];
    if (c.mBoard->mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN || c.mBoard->mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
    {
    c.mApp->mZenGarden->DrawPottedPlant(c.mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, g, -10.0f, -10.0f, aPottedPlant, 1.0f, false);
    }
    else
    {
    c.mApp->mZenGarden->DrawPottedPlant(c.mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, g, -22.0f, -38.0f, aPottedPlant, 1.0f, true);
    }
    },  // 3 CURSOR_TYPE_PLANT_FROM_GLOVE
    CursorDrawCursorTypePlantFromBank,  // 4 CURSOR_TYPE_PLANT_FROM_DUPLICATOR（共体）
    // 原 case CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW
    [](CursorObject& c, Graphics* g)
    {
    PottedPlant* aPottedPlant = c.mApp->mZenGarden->GetPottedPlantInWheelbarrow();
    if (c.mBoard->mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN || c.mBoard->mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
    {
    c.mApp->mZenGarden->DrawPottedPlant(c.mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, g, -10.0f, -10.0f, aPottedPlant, 1.0f, false);
    }
    else
    {
    c.mApp->mZenGarden->DrawPottedPlant(c.mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, g, -22.0f, -38.0f, aPottedPlant, 1.0f, true);
    }
    },  // 5 CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW
    // 原 case CURSOR_TYPE_SHOVEL
    [](CursorObject& c, Graphics* g)
    {
    g->DrawImage(IMAGE_SHOVEL, 10, -30);
    },  // 6 CURSOR_TYPE_SHOVEL
    // 原 case CURSOR_TYPE_HAMMER
    [](CursorObject& c, Graphics* g)
    {
    c.mApp->ReanimationGet(c.mReanimCursorID)->Draw(g);
    },  // 7 CURSOR_TYPE_HAMMER
    // 原 case CURSOR_TYPE_COBCANNON_TARGET
    [](CursorObject& c, Graphics* g)
    {
    HitResult aHitResult;
    c.mBoard->MouseHitTest(c.mX + 25, c.mY + 35, &aHitResult);
    if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_NONE && c.mY + 35 >= 80)
    {
    g->DrawImage(IMAGE_COBCANNON_TARGET, -11, 7, Rect(0, 0, IMAGE_COBCANNON_TARGET->mWidth, IMAGE_COBCANNON_TARGET->mHeight));
    }
    },  // 8 CURSOR_TYPE_COBCANNON_TARGET
    // 原 case CURSOR_TYPE_WATERING_CAN
    [](CursorObject& c, Graphics* g)
    {
    if (c.mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_GOLD_WATERINGCAN])
    {
    g->DrawImage(IMAGE_ZEN_GOLDTOOLRETICLE, -62, -37);
    g->DrawImage(IMAGE_WATERINGCANGOLD, -3, 12);
    }
    else
    {
    g->DrawImage(IMAGE_WATERINGCAN, -3, 12);
    }
    },  // 9 CURSOR_TYPE_WATERING_CAN
    // 原 case CURSOR_TYPE_FERTILIZER
    [](CursorObject& c, Graphics* g)
    {
    g->DrawImage(IMAGE_FERTILIZER, -15, 0);
    },  // 10 CURSOR_TYPE_FERTILIZER
    // 原 case CURSOR_TYPE_BUG_SPRAY
    [](CursorObject& c, Graphics* g)
    {
    g->DrawImage(IMAGE_BUG_SPRAY, -9, -1);
    },  // 11 CURSOR_TYPE_BUG_SPRAY
    // 原 case CURSOR_TYPE_PHONOGRAPH
    [](CursorObject& c, Graphics* g)
    {
    g->DrawImage(IMAGE_PHONOGRAPH, -17, 10);
    },  // 12 CURSOR_TYPE_PHONOGRAPH
    // 原 case CURSOR_TYPE_CHOCOLATE
    [](CursorObject& c, Graphics* g)
    {
    g->DrawImage(IMAGE_CHOCOLATE, -2, -8);
    },  // 13 CURSOR_TYPE_CHOCOLATE
    // 原 case CURSOR_TYPE_GLOVE
    [](CursorObject& c, Graphics* g)
    {
    g->DrawImage(IMAGE_ZEN_GARDENGLOVE, -17, 15);
    },  // 14 CURSOR_TYPE_GLOVE
    // 原 case CURSOR_TYPE_MONEY_SIGN
    [](CursorObject& c, Graphics* g)
    {
    g->DrawImage(IMAGE_ZEN_MONEYSIGN, -17, -10);
    },  // 15 CURSOR_TYPE_MONEY_SIGN
    // 原 case CURSOR_TYPE_WHEEELBARROW
    [](CursorObject& c, Graphics* g)
    {
    PottedPlant* aPottedPlant = c.mApp->mZenGarden->GetPottedPlantInWheelbarrow();
    if (aPottedPlant)
    {
    g->DrawImage(IMAGE_ZEN_WHEELBARROW, -20, -23);
    if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_SMALL)
    {
    c.mApp->mZenGarden->DrawPottedPlant(c.mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, g, 10.0f, -35.0f, aPottedPlant, 0.6f, true);
    }
    else if (aPottedPlant->mPlantAge == PottedPlantAge::PLANTAGE_MEDIUM)
    {
    c.mApp->mZenGarden->DrawPottedPlant(c.mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, g, 15.0f, -25.0f, aPottedPlant, 0.5f, true);
    }
    else
    {
    c.mApp->mZenGarden->DrawPottedPlant(c.mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, g, 21.0f, -15.0f, aPottedPlant, 0.4f, true);
    }
    }
    else
    {
    g->DrawImage(IMAGE_ZEN_WHEELBARROW, -20, -30);
    }
    },  // 16 CURSOR_TYPE_WHEEELBARROW
    // 原 case CURSOR_TYPE_TREE_FOOD
    [](CursorObject& c, Graphics* g)
    {
    g->DrawImage(IMAGE_TREEFOOD, -15, 0);
    },  // 17 CURSOR_TYPE_TREE_FOOD
    };
    static_assert(sizeof(kCursorDrawCases) / sizeof(kCursorDrawCases[0]) ==
    (int)CursorType::CURSOR_TYPE_TREE_FOOD + 1, "kCursorDrawCases 槽位数须与 CursorType 枚举同步");

    // C 形态 -> 匿名 lambda 函数指针表：行序 = CursorType 枚举序（0..17，
    // 全覆盖无 default）；Graphics* 经参传入。
    const unsigned aCursorIndex = static_cast<unsigned>(mCursorType);
    if (aCursorIndex <= static_cast<unsigned>(CursorType::CURSOR_TYPE_TREE_FOOD))
    {
        if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN ||
            mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
        {
            // BeginDraw already applied the stored cursor origin. Refresh only
            // its presentation against this pointer, without shifting plants.
            Graphics cursor(*g);
            cursor.Translate(x - 25 - mX, y - 35 - mY);
            kCursorDrawCases[aCursorIndex](*this, &cursor);
        }
        else
            kCursorDrawCases[aCursorIndex](*this, g);
    }

}

//0x438D50
CursorPreview::CursorPreview()
{
    mX = 0;
    mY = 0;
    mGridX = 0;
    mGridY = 0;
    mVisible = false;
    mWidth = 80;
    mHeight = 80;
}

//0x438DA0
void CursorPreview::Update()
{
    Update(mApp->mWidgetManager->mLastMouseX - mBoard->mX,
        mApp->mWidgetManager->mLastMouseY - mBoard->mY);
}

void CursorPreview::Update(int x, int y)
{
    if (mApp->mGameScene != GameScenes::SCENE_PLAYING && !mBoard->mCutScene->IsInShovelTutorial())
    {
        mVisible = false;
        return;
    }

    SeedType aSeedType = mBoard->GetSeedTypeInCursor();
    mGridX = mBoard->PlantingPixelToGridX(x, y, aSeedType);
    mGridY = mBoard->PlantingPixelToGridY(x, y, aSeedType);
    if (mGridX >= 0 && mGridX < MAX_GRID_SIZE_X && mGridY >= 0 && mGridY < MAX_GRID_SIZE_Y)
    {
        bool aShow = false;
        if (mBoard->IsPlantInCursor() && mBoard->CanPlantAt(mGridX, mGridY, aSeedType) == PlantingReason::PLANTING_OK)
        {
            aShow = true;
        }
        else if (mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_WHEEELBARROW)
        {
            if (mApp->mZenGarden->GetPottedPlantInWheelbarrow() && mBoard->CanPlantAt(mGridX, mGridY, aSeedType) == PlantingReason::PLANTING_OK)
            {
                aShow = true;
            }
        }

        if (aShow)
        {
            mX = mBoard->GridToPixelX(mGridX, mGridY);
            mY = mBoard->GridToPixelY(mGridX, mGridY);
            mVisible = true;
            return;
        }
    }

    mVisible = false;
}

//0x438EB0
void CursorPreview::Draw(Graphics* g)
{
    SeedType aSeedType = mBoard->GetSeedTypeInCursor();
    if (aSeedType == SeedType::SEED_NONE)
        return;

    g->mColorizeImages = true;
    g->mColor = Color(255, 255, 255, 100);

    PottedPlant* aPottedPlant = nullptr;
    if (mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_WHEEELBARROW || mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW)
    {
        aPottedPlant = mApp->mZenGarden->GetPottedPlantInWheelbarrow();
    }
    else if (mBoard->mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE)
    {
        aPottedPlant = &mApp->mPlayerInfo->mPottedPlant[mBoard->mPlants.DataArrayGet((unsigned int)mBoard->mCursorObject->mGlovePlantID)->mPottedPlantIndex];
    }

    if (aPottedPlant)
    {
        bool aDrawPot = true;
        if (mBoard->mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN || mBoard->mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
        {
            aDrawPot = false;
        }
        mApp->mZenGarden->DrawPottedPlant(mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, g, 0.0f, 0.0f, aPottedPlant, 1.0f, aDrawPot);
    }
    else
    {
        float aOffsetX, aOffsetY;
        if (mApp->IsIZombieLevel())
        {
            float aHeight = PlantDrawHeightOffset(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mBoard, nullptr, aSeedType, mGridX, mGridY);
            if (aSeedType == SeedType::SEED_ZOMBIE_GARGANTUAR)
            {
                aHeight -= 30.0f;
            }

            aOffsetY = aHeight - 78.0f;
            aOffsetX = -49.0f;
        }
        else
        {
            aOffsetY = PlantDrawHeightOffset(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mBoard, nullptr, aSeedType, mGridX, mGridY);
            aOffsetX = 0.0f;
        }

        Plant::DrawSeedType(g, mBoard->mCursorObject->mType, mBoard->mCursorObject->mImitaterType, DrawVariation::VARIATION_NORMAL, aOffsetX, aOffsetY);
    }

    if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_COLUMN)
    {
        for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
        {
            if (y != mGridY && mBoard->CanPlantAt(mGridX, y, aSeedType) == PlantingReason::PLANTING_OK)
            {
                float aOffsetY = 85.0f * (y - mGridY) + PlantDrawHeightOffset(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mBoard, nullptr, aSeedType, mGridX, y);
                Plant::DrawSeedType(g, mBoard->mCursorObject->mType, mBoard->mCursorObject->mImitaterType, DrawVariation::VARIATION_NORMAL, 0.0f, aOffsetY);
            }
        }
    }

    g->mColorizeImages = false;
}
