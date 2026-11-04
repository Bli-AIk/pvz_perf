#include "EffectSystem.h"
#include "Reanimator.h"
#include "ReanimClock.h"
#include "Sexy.TodLib/generated/EntityStrings.h"
#include "Lawn/Board.h"
#include "Lawn/Zombie.h"
#include "GameButton.h"
#include "StoreScreen.h"
#include "Lawn/Cutscene.h"
#include "Lawn/SeedPacket.h"
#include "LawnApp.h"
#include "Almanac.h"
#include "ImitaterDialog.h"
#include "Lawn/System/Music.h"
#include "Resources.h"
#include "Lawn/Plant.h"
#include "Lawn/ToolTipWidget.h"
#include "SeedChooserScreen.h"
#include "SeedChooserLayout.h"
#include "Lawn/LandscapeHud.h"
#include "GameConstants.h"
#include "Lawn/System/PlayerInfo.h"
#include "Lawn/System/PopDRMComm.h"
#include "misc/Debug.h"
#include "widget/Dialog.h"
#include "misc/MTRand.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "widget/WidgetManager.h"
#include "widget/WidgetLifetimeHold.h"
#include "graphics/AtlasDraw.h"
#include "Sexy.TodLib/generated/SceneAtlasData.h"
#include <algorithm>

namespace
{
static constexpr SeedChooserCardLayout kClassicCards = {{22,128,424,365},22,128,8,53,73,50,70};
static constexpr SeedChooserCardLayout kExpandedClassicCards = {{22,123,424,420},22,123,8,53,70,50,70};
static constexpr SeedChooserCardLayout kLandscapeCards = {{118,53,347,450},122,55,4,85,55,81,53};
void RefreshChooserHudLayout(SeedChooserScreen& theChooser)
{
	const Rect& aBounds = theChooser.mApp->mScreenBounds;
	theChooser.Move(aBounds.mX, theChooser.mY);
	theChooser.mAlmanacButton->mX = theChooser.mBoard->mLandscapeHud ? 114 : SeedChooserLayout::RightButtonX(560, aBounds.mWidth);
	theChooser.mStoreButton->mX = theChooser.mBoard->mLandscapeHud ? 346 : SeedChooserLayout::RightButtonX(680, aBounds.mWidth);
	theChooser.mMenuButton->mX = 681 + aBounds.mX + aBounds.mWidth - BOARD_WIDTH;
	theChooser.mMouseInsets.mLeft = std::min(0, aBounds.mX - theChooser.mX);
	theChooser.mMouseInsets.mRight = std::min(0,
		theChooser.mX + theChooser.mWidth - aBounds.mX - aBounds.mWidth);
	theChooser.mToolTip->mMinLeft = aBounds.mX;
}

int ChosenSeedPresentationX(const SeedChooserScreen& theChooser, const ChosenSeed& theSeed)
{
	const int aLeft = SeedChooserLayout::RebaseX(0, theChooser.mApp->mScreenBounds.mX, theChooser.mX);
	if (theSeed.mSeedState == SEED_IN_BANK)
		return theSeed.mX + aLeft;
	if (theSeed.mSeedState != SEED_FLYING_TO_BANK && theSeed.mSeedState != SEED_FLYING_TO_CHOOSER)
		return theSeed.mX;

	// Keep flight coordinates canonical, including bank-to-bank gap closing.
	// Apply the current viewport offset to the appropriate curve endpoints.
	const bool aToBank = theSeed.mSeedState == SEED_FLYING_TO_BANK;
	const bool aFromBank = !aToBank || theSeed.mStartY == theSeed.mEndY;
	const auto aDurationNs = theSeed.mTimeEndMotion - theSeed.mTimeStartMotion;
	const auto aElapsedNs = theChooser.mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs - theSeed.mTimeStartMotion;
	return TodAnimateCurveNs(0, static_cast<std::int64_t>(aDurationNs),
		static_cast<std::int64_t>(std::min(aElapsedNs, aDurationNs)),
		theSeed.mStartX + (aFromBank ? aLeft : 0),
		theSeed.mEndX + (aToBank ? aLeft : 0), Curves::EaseInOut);
}

// A modal wait can dispose a Board even when its Widget has not yet been
// deleted. Retaining the chooser is necessary, but its Board/session must also
// still be current before a result is applied.
class ChooserModalSession
{
    WidgetLifetimeHold mCaller;
    SeedChooserScreen& mChooser;
    LawnApp& mApp;
    Board* mBoard;
    std::uint64_t mBoardRevision;
    PlayerInfo* mPlayer;
    unsigned int mPlayerId;
    unsigned int mPlayerUseSeq;
public:
    explicit ChooserModalSession(SeedChooserScreen& chooser)
        : mCaller(chooser), mChooser(chooser), mApp(*chooser.mApp), mBoard(chooser.mBoard),
          mBoardRevision(mApp.mBoardRevision), mPlayer(mApp.mPlayerInfo),
          mPlayerId(mPlayer ? mPlayer->mId : 0), mPlayerUseSeq(mPlayer ? mPlayer->mUseSeq : 0) {}
    bool IsCurrent() const
    {
        return !mApp.mShutdown && !mApp.mCloseRequest && mBoard &&
            mApp.mSeedChooserScreen == &mChooser && mChooser.mMouseVisible &&
            mApp.mBoard == mBoard && mApp.mBoardRevision == mBoardRevision &&
            mApp.mPlayerInfo == mPlayer && mPlayer &&
            mPlayer->mId == mPlayerId && mPlayer->mUseSeq == mPlayerUseSeq;
    }
};
}

//0x483380
// GOTY @Patoke: 0x48E020
SeedChooserScreen::SeedChooserScreen()
{
	mApp = (LawnApp*)gSexyAppBase;
	mBoard = mApp->mBoard;
    RefreshCardLayout();
	mClip = false;
	// 1073 leaves this clock uninitialized. Keep its 15/25-tick animations,
	// not its dependency on old heap bytes or signed-overflow deadlines.
	mSeedsInFlight = 0;
	mSeedsInBank = 0;
	mLastMouseX = -1;
	mLastMouseY = -1;
	mChooseState = CHOOSE_NORMAL;
	mViewLawnStartTimeNs = 0;
	mViewLawnReturnTimeNs = -1;
	mToolTip = new ToolTipWidget();
	mToolTipSeed = -1;

	mStartButton = new GameButton(SeedChooserScreen::SeedChooserScreen_Start);
	mStartButton->mLabel = gStr.LETS_ROCK_BUTTON; // @Patoke: wrong local name
	mStartButton->mNormalRegion = CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::SEEDCHOOSER_BUTTON)];
	mStartButton->mOverImage = nullptr;
	mStartButton->mDownImage = nullptr;
	mStartButton->mDisabledRegion = CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::SEEDCHOOSER_BUTTON_DISABLED)];
	mStartButton->SetFont(Sexy::FONT_DWARVENTODCRAFT18YELLOW);
	mStartButton->mColors[GameButton::COLOR_LABEL_HILITE] = Color::White;
	mStartButton->mX = 154;
	mStartButton->mY = 545;
	mStartButton->mWidth = 156;
	mStartButton->mHeight = 42;
	mStartButton->mParentWidget = this;
	mStartButton->mTextOffsetY = -1;
	EnableStartButton(false);

	mMenuButton = new GameButton(SeedChooserScreen::SeedChooserScreen_Menu);
	mMenuButton->mLabel = gStr.MENU_BUTTON;
	mMenuButton->mX = 681;
	mMenuButton->mY = -10;
	mMenuButton->mWidth = 117;
	mMenuButton->mHeight = 46;
	mMenuButton->mDrawStoneButton = true;

	mRandomButton = new GameButton(SeedChooserScreen::SeedChooserScreen_Random);
	mRandomButton->mLabel = ("(Debug Play)");
	mRandomButton->mButtonImage = Sexy::IMAGE_BLANK;
	mRandomButton->mOverImage = Sexy::IMAGE_BLANK;
	mRandomButton->mDownImage = Sexy::IMAGE_BLANK;
	mRandomButton->SetFont(Sexy::FONT_BRIANNETOD12);
	mRandomButton->mColors[0] = Color(255, 240, 0);
	mRandomButton->mColors[1] = Color(200, 200, 255);
	mRandomButton->mX = 332;
	mRandomButton->mY = 546;
	mRandomButton->mWidth = 100;
	mRandomButton->mHeight = 30;
	mRandomButton->mParentWidget = this;
	if (!mApp->mTodCheatKeys)
	{
		mRandomButton->mBtnNoDraw = true;
		mRandomButton->mDisabled = true;
	}

	Color aBtnColor = Color(42, 42, 90);
	const TextureRegion& button = CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::SEEDCHOOSER_BUTTON2)];
	const TextureRegion& over = CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::SEEDCHOOSER_BUTTON2_GLOW)];
	int aImageWidth = button.mWidth;
	int aImageHeight = over.mHeight;

	mViewLawnButton = new GameButton(SeedChooserScreen::SeedChooserScreen_ViewLawn);
	mViewLawnButton->mLabel = gStr.VIEW_LAWN;
	mViewLawnButton->mNormalRegion = button;
	mViewLawnButton->mOverRegion = over;
	mViewLawnButton->mDownImage = nullptr;
	mViewLawnButton->SetFont(Sexy::FONT_BRIANNETOD12);
	mViewLawnButton->mColors[0] = aBtnColor;
	mViewLawnButton->mColors[1] = aBtnColor;
	mViewLawnButton->mX = 22;
	mViewLawnButton->mY = 561;
	mViewLawnButton->mWidth = aImageWidth;
	mViewLawnButton->mHeight = aImageHeight;
	mViewLawnButton->mParentWidget = this;
	mViewLawnButton->mTextOffsetY = 1;
	if (!mBoard->mCutScene->IsSurvivalRepick())
	{
		mViewLawnButton->mBtnNoDraw = true;
		mViewLawnButton->mDisabled = true;
	}

	mAlmanacButton = new GameButton(SeedChooserScreen::SeedChooserScreen_Almanac);
	mAlmanacButton->mLabel = gStr.ALMANAC_BUTTON;
	mAlmanacButton->mNormalRegion = button;
	mAlmanacButton->mOverRegion = over;
	mAlmanacButton->mDownImage = nullptr;
	mAlmanacButton->SetFont(Sexy::FONT_BRIANNETOD12);
	mAlmanacButton->mColors[0] = aBtnColor;
	mAlmanacButton->mColors[1] = aBtnColor;
	mAlmanacButton->mX = 560;
	mAlmanacButton->mY = 572;
	mAlmanacButton->mWidth = aImageWidth;
	mAlmanacButton->mHeight = aImageHeight;
	mAlmanacButton->mParentWidget = this;
	mAlmanacButton->mTextOffsetY = 1;

	mStoreButton = new GameButton(SeedChooserScreen::SeedChooserScreen_Store);
	mStoreButton->mLabel = gStr.SHOP_BUTTON;
	mStoreButton->mNormalRegion = button;
	mStoreButton->mOverRegion = over;
	mStoreButton->mDownImage = nullptr;
	mStoreButton->SetFont(Sexy::FONT_BRIANNETOD12);
	mStoreButton->mColors[0] = aBtnColor;
	mStoreButton->mColors[1] = aBtnColor;
	mStoreButton->mX = 680;
	mStoreButton->mY = 572;
	mStoreButton->mWidth = aImageWidth;
	mStoreButton->mHeight = aImageHeight;
	mStoreButton->mParentWidget = this;
	mStoreButton->mTextOffsetY = 1;

	mImitaterButton = new GameButton(SeedChooserScreen::SeedChooserScreen_Imitater);
	mImitaterButton->mButtonImage = Sexy::IMAGE_IMITATERSEED;
	mImitaterButton->mOverImage = Sexy::IMAGE_IMITATERSEED;
	mImitaterButton->mDownImage = Sexy::IMAGE_IMITATERSEED;
	mImitaterButton->mDisabledImage = Sexy::IMAGE_IMITATERSEEDDISABLED;
	mImitaterButton->mX = 464;
	mImitaterButton->mY = 515;
	mImitaterButton->mWidth = Sexy::IMAGE_IMITATERSEED->mWidth;
	mImitaterButton->mHeight = Sexy::IMAGE_IMITATERSEED->mHeight;
	mImitaterButton->mParentWidget = this;

	if (!mApp->CanShowAlmanac())
	{
		mAlmanacButton->mBtnNoDraw = true;
		mAlmanacButton->mDisabled = true;
	}
	if (!mApp->CanShowStore())
	{
		mStoreButton->mBtnNoDraw = true;
		mStoreButton->mDisabled = true;
	}

	if(mBoard->mLandscapeHud) LandscapeHud::ConfigureChooser(*this);
	DBG_ASSERT(mApp->GetSeedsAvailable() < NUM_SEED_TYPES);
	memset(mChosenSeeds, 0, sizeof(mChosenSeeds));
	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		aChosenSeed.mSeedType = aSeedType;
		GetSeedPositionInChooser(aSeedType, aChosenSeed.mX, aChosenSeed.mY);
		aChosenSeed.mTimeStartMotion = 0;
		aChosenSeed.mTimeEndMotion = 0;
		aChosenSeed.mStartX = aChosenSeed.mX;
		aChosenSeed.mStartY = aChosenSeed.mY;
		aChosenSeed.mEndX = aChosenSeed.mX;
		aChosenSeed.mEndY = aChosenSeed.mY;
		aChosenSeed.mSeedState = aSeedType == SEED_IMITATER ? SEED_PACKET_HIDDEN : SEED_IN_CHOOSER;
		aChosenSeed.mSeedIndexInBank = 0;
		aChosenSeed.mRefreshStartTimeNs = 0;
		aChosenSeed.mRefreshing = false;
		aChosenSeed.mImitaterType = SEED_NONE;
		aChosenSeed.mCrazyDavePicked = false;
	}
	if (mBoard->mCutScene->IsSurvivalRepick())
	{
		for (int anIdx = 0; anIdx < mBoard->mSeedBank->mNumPackets; anIdx++)
		{
			SeedPacket* aSeedPacket = &mBoard->mSeedBank->mSeedPackets[anIdx];
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedPacket->mPacketType];
			aChosenSeed.mRefreshing = aSeedPacket->mRefreshing;
			aChosenSeed.mRefreshStartTimeNs = aSeedPacket->mRefreshStartTimeNs;
		}
		mBoard->mSeedBank->mNumPackets = 0;
	}
	if (mApp->mGameMode == GAMEMODE_MINIGAME_SEEING_STARS)
	{
		ChosenSeed& aStarFruit = mChosenSeeds[SEED_STARFRUIT];
		int aX,aY;GetSeedPositionInBank(0,aX,aY);
		aStarFruit.mX = aX, aStarFruit.mY = aY;
		aStarFruit.mStartX = aX, aStarFruit.mStartY = aY;
		aStarFruit.mEndX = aX, aStarFruit.mEndY = aY;
		aStarFruit.mSeedState = SEED_IN_BANK;
		aStarFruit.mSeedIndexInBank = 0;
		mSeedsInBank++;
	}
	if (mApp->IsAdventureMode() && !mApp->IsFirstTimeAdventureMode())
		CrazyDavePickSeeds();
	UpdateImitaterButton();
	RefreshChooserHudLayout(*this);
}

int SeedChooserScreen::PickFromWeightedArrayUsingSpecialRandSeed(TodWeightedArray* theArray, int theCount, MTRand& theLevelRNG)
{
	int aTotalWeight = 0;
	for (int i = 0; i < theCount; i++)
		aTotalWeight += theArray[i].mWeight;
	DBG_ASSERT(aTotalWeight > 0);

	int aRndResult = theLevelRNG.Next(static_cast<unsigned int>(aTotalWeight));
	int aWeight = 0;
	for (int j = 0; j < theCount; j++)
	{
		aWeight += theArray[j].mWeight;
		if (aWeight > aRndResult) return theArray[j].mItem;
	}
	DBG_ASSERT(false);
	return theCount > 0 ? theArray[theCount - 1].mItem : 0;
}

//0x483F70
void SeedChooserScreen::CrazyDavePickSeeds()
{
	TodWeightedArray aSeedArray[NUM_SEED_TYPES];
	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		aSeedArray[aSeedType].mItem = aSeedType;
		if ((aSeedType == SEED_GATLINGPEA && !mApp->mPlayerInfo->mPurchases[STORE_ITEM_PLANT_GATLINGPEA]) || !mApp->SeedTypeAvailable(aSeedType) ||
			SeedNotRecommendedToPick(aSeedType) || SeedNotAllowedToPick(aSeedType) || Plant::IsUpgrade(aSeedType) ||
			aSeedType == SEED_IMITATER || aSeedType == SEED_UMBRELLA || aSeedType == SEED_BLOVER)
		{
			aSeedArray[aSeedType].mWeight = 0;
		}
		else
		{
			aSeedArray[aSeedType].mWeight = 1;
		}
	}
	if (mBoard->mZombieAllowed[ZOMBIE_BUNGEE] || mBoard->mZombieAllowed[ZOMBIE_CATAPULT])
	{
		aSeedArray[SEED_UMBRELLA].mWeight = 1;
	}
	if (mBoard->mZombieAllowed[ZOMBIE_BALLOON] || mBoard->StageHasFog())
	{
		aSeedArray[SEED_BLOVER].mWeight = 1;
	}
	if (mBoard->StageHasRoof())
	{
		aSeedArray[SEED_TORCHWOOD].mWeight = 0;
	}
	
	MTRand aLevelRNG = MTRand(mBoard->GetLevelRandSeed());
	for (int i = 0; i < 3; i++)
	{
		SeedType aPickedSeed = (SeedType)PickFromWeightedArrayUsingSpecialRandSeed(aSeedArray, NUM_SEEDS_IN_CHOOSER, aLevelRNG);
		aSeedArray[aPickedSeed].mWeight = 0;
		ChosenSeed& aChosenSeed = mChosenSeeds[aPickedSeed];

		int aPosX,aPosY;GetSeedPositionInBank(i,aPosX,aPosY);
		aChosenSeed.mX = aPosX;
		aChosenSeed.mY = aPosY;
		aChosenSeed.mStartX = aPosX;
		aChosenSeed.mStartY = aPosY;
		aChosenSeed.mEndX = aPosX;
		aChosenSeed.mEndY = aPosY;
		aChosenSeed.mSeedState = SEED_IN_BANK;
		aChosenSeed.mSeedIndexInBank = i;
		aChosenSeed.mCrazyDavePicked = true;
		mSeedsInBank++;
	}
}

//0x484220
bool SeedChooserScreen::Has7Rows()
{
	// PlayerInfo* aPlayer = mApp->mPlayerInfo; // unused
	if (mApp->HasFinishedAdventure() || mApp->mPlayerInfo->mPurchases[STORE_ITEM_PLANT_GATLINGPEA]) return true;
	for (SeedType aSeedType = SEED_TWINSUNFLOWER; aSeedType < SEED_COBCANNON; aSeedType = (SeedType)(aSeedType + 1))
		if (aSeedType != SEED_SPIKEROCK && mApp->SeedTypeAvailable(aSeedType)) return true;
	return false;
}

//0x484400
void SeedChooserScreen::GetSeedPositionInChooser(int theIndex, int& x, int& y)
{
    if(theIndex==SEED_IMITATER) {x=mImitaterButton->mX;y=mImitaterButton->mY;return;}
    const auto& layout=*mCardLayout;
    x=layout.originX+(theIndex%layout.columns)*layout.pitchX;
    y=layout.originY+(theIndex/layout.columns)*layout.pitchY+FloatRoundToInt(mCardScroll.position);
}

//0x484480
void SeedChooserScreen::GetSeedPositionInBank(int theIndex, int& x, int& y)
{
    if(mBoard->mLandscapeHud) {
        const auto rect=LandscapeHud::PacketBounds(*mBoard,theIndex);
        x=rect.mX+mBoard->mX-mApp->mScreenBounds.mX;y=rect.mY+mBoard->mY;return;
    }
	// Flight endpoints stay canonical when the chooser moves with the viewport.
	x = mBoard->mSeedBank->mX + mBoard->GetSeedPacketPositionX(theIndex);
	y = mBoard->mSeedBank->mY - mY + 8;
}

//0x4844D0
SeedChooserScreen::~SeedChooserScreen()
{
	if (mStartButton) delete mStartButton;
	if (mRandomButton) delete mRandomButton;
	if (mViewLawnButton) delete mViewLawnButton;
	if (mAlmanacButton) delete mAlmanacButton;
	if (mImitaterButton) delete mImitaterButton;
	if (mStoreButton) delete mStoreButton;
	if (mToolTip) delete mToolTip;
	if (mMenuButton) delete mMenuButton;
}

//0x4845E0
unsigned int SeedChooserScreen::SeedNotRecommendedToPick(SeedType theSeedType)
{
	uint aRecFlags = mBoard->SeedNotRecommendedForLevel(theSeedType);
	if (TestBit(aRecFlags, NOT_RECOMMENDED_NOCTURNAL) && PickedPlantType(SEED_INSTANT_COFFEE))
		SetBit(aRecFlags, NOT_RECOMMENDED_NOCTURNAL, false);
	return aRecFlags;
}

//0x484620
bool SeedChooserScreen::SeedNotAllowedToPick(SeedType theSeedType)
{
	return mApp->mGameMode == GAMEMODE_MINIGAME_LAST_STAND && (theSeedType == SEED_SUNFLOWER || theSeedType == SEED_SUNSHROOM || 
		theSeedType == SEED_TWINSUNFLOWER || theSeedType == SEED_SEASHROOM || theSeedType == SEED_PUFFSHROOM);
}

//0x484640
bool SeedChooserScreen::SeedNotAllowedDuringTrial(SeedType theSeedType)
{
	return mApp->IsTrialStageLocked() && (theSeedType == SEED_SQUASH || theSeedType == SEED_THREEPEATER);
}

//0x484690
void SeedChooserScreen::Draw(Graphics* g)
{
	const int aPreviousX = mX;
	RefreshChooserHudLayout(*this);
	Graphics aChooserG(*g);
	aChooserG.Translate(mX - aPreviousX, 0);
	g = &aChooserG;
	if (mApp->GetDialog(DIALOG_STORE) || mApp->GetDialog(DIALOG_ALMANAC))
		return;

	g->mLinearBlend = true;
	if (!mBoard->ChooseSeedsOnCurrentLevel() || (mBoard->mCutScene && mBoard->mCutScene->IsBeforePreloading()))
		return;

    const auto drawPacket=mBoard->mLandscapeHud ? &LandscapeHud::DrawPacket : &DrawSeedPacket;
    const TextureSource chooserButtons{mBoard->mLandscapeHud ? LandscapeUIAtlas::Texture : CommonUIAtlas::Texture,0};
    if(mBoard->mLandscapeHud) LandscapeHud::DrawChooserBackground(g);
    else {
        Sexy::DrawAtlasImage(g,{DesktopUIAtlas::Texture,0},DesktopUIAtlas::Images[size_t(DesktopUIAtlas::Sprite::SEEDCHOOSER_BACKGROUND)],0,87);
        if(mApp->SeedTypeAvailable(SEED_IMITATER))Sexy::DrawAtlasImage(g,{DesktopUIAtlas::Texture,0},DesktopUIAtlas::Images[size_t(DesktopUIAtlas::Sprite::SEEDCHOOSER_IMITATERADDON)],459,503);
        TodDrawString(g,gStr.CHOOSE_YOUR_PLANTS,229,110,FONT_DWARVENTODCRAFT18YELLOW,Color::White,DS_ALIGN_CENTER);
    }

	Graphics listGraphics(*g);
    const bool clippedList=mBoard->mLandscapeHud||mCardScroll.position!=0;
    if(clippedList)listGraphics.ClipRect(mCardLayout->viewport);
    Graphics* list=clippedList?&listGraphics:g;
	int aNumSeeds = mCardCount;
	for (SeedType aSeedShadow = SEED_PEASHOOTER; aSeedShadow < aNumSeeds; aSeedShadow = (SeedType)(aSeedShadow + 1))
	{
		int x, y;
		GetSeedPositionInChooser(aSeedShadow, x, y);
		if (aSeedShadow == SEED_IMITATER)
		{
			continue;
		}

		if (mApp->SeedTypeAvailable(aSeedShadow))
		{
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedShadow];
			if (aChosenSeed.mSeedState != SEED_IN_CHOOSER)
			{
				drawPacket(list, x, y, aSeedShadow, SEED_NONE, 0, 55, true, false);
			}
		}
		else
		{
			if(mBoard->mLandscapeHud) LandscapeHud::DrawSilhouette(list,x,y);
            else Sexy::DrawAtlasImage(list,{DesktopUIAtlas::Texture,0},DesktopUIAtlas::Images[size_t(DesktopUIAtlas::Sprite::SEEDPACKETSILHOUETTE)],x,y);
		}
	}

	int aNumSeedsInBank = mBoard->mSeedBank->mNumPackets;
	for (int anIndex = 0; anIndex < aNumSeedsInBank; anIndex++)
	{
		if (FindSeedInBank(anIndex) == SEED_NONE)
		{
			int x, y;
			GetSeedPositionInBank(anIndex, x, y);
			x = SeedChooserLayout::RebaseX(x, mApp->mScreenBounds.mX, mX);
			if(mBoard->mLandscapeHud) LandscapeHud::DrawSilhouette(g,x,y);
            else Sexy::DrawAtlasImage(g,{DesktopUIAtlas::Texture,0},DesktopUIAtlas::Images[size_t(DesktopUIAtlas::Sprite::SEEDPACKETSILHOUETTE)],x,y);
		}
	}

	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		ChosenSeedState aSeedState = aChosenSeed.mSeedState;
		if (mApp->SeedTypeAvailable(aSeedType) && aSeedState != SEED_FLYING_TO_BANK && aSeedState != SEED_FLYING_TO_CHOOSER && 
			aSeedState != SEED_PACKET_HIDDEN && (aSeedState == SEED_IN_CHOOSER || mBoard->mCutScene->mSeedChoosing))
		{
			bool aGrayed = false;
			if (((SeedNotRecommendedToPick(aSeedType) || SeedNotAllowedToPick(aSeedType)) && aSeedState == SEED_IN_CHOOSER) ||
				SeedNotAllowedDuringTrial(aSeedType))
				aGrayed = true;
			
			int aPosX = ChosenSeedPresentationX(*this, aChosenSeed);
			int aPosY = aChosenSeed.mY;
			if (aSeedState == SEED_IN_BANK)
			{
				aPosY -= mY;
			}
			if(mBoard->mLandscapeHud && aSeedState==SEED_IN_BANK)
                LandscapeHud::DrawPacket(g,aPosX,aPosY,aChosenSeed.mSeedType,aChosenSeed.mImitaterType,0,aGrayed?115:255,true,false);
            else drawPacket(aSeedState==SEED_IN_CHOOSER?list:g, aPosX, aPosY, aChosenSeed.mSeedType, aChosenSeed.mImitaterType, 0, aGrayed ? 115 : 255, true, false);
		}
	}

	if(mBoard->mLandscapeHud) {
        if(!mImitaterButton->mBtnNoDraw) LandscapeHud::DrawImitaterAddon(g);
        mImitaterButton->DrawAtlas(g,chooserButtons);
    }
    else mImitaterButton->Draw(g);
	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		ChosenSeedState aSeedState = aChosenSeed.mSeedState;
		if (mApp->SeedTypeAvailable(aSeedType) && (aSeedState == SEED_FLYING_TO_BANK || aSeedState == SEED_FLYING_TO_CHOOSER))
		{
			drawPacket(g, ChosenSeedPresentationX(*this, aChosenSeed), aChosenSeed.mY, aChosenSeed.mSeedType, aChosenSeed.mImitaterType, 0, 255, true, false);
		}
	}

	mStartButton->DrawAtlas(g, chooserButtons,
		mBoard->mLandscapeHud?nullptr:&CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::SEEDCHOOSER_BUTTON_GLOW)]);
	mRandomButton->Draw(g);
	mViewLawnButton->DrawAtlas(g, chooserButtons);
	mAlmanacButton->DrawAtlas(g, chooserButtons);
	mStoreButton->DrawAtlas(g, chooserButtons);
	Graphics aBoardFrameG = Graphics(*g);
	aBoardFrameG.mTransX -= mX;
	aBoardFrameG.mTransY -= mY;
	mMenuButton->Draw(&aBoardFrameG);
	if (mToolTip->mVisible)
		mToolTip->Draw(g,
			Rect(mApp->mScreenBounds.mX - mX, -mY, mApp->mScreenBounds.mWidth, mToolTip->mMaxBottom));
}

void SeedChooserScreen::DrawZombieToolTip(Graphics* g)
{
	ToolTipWidget& aTip = *mBoard->mToolTip;
	if (!aTip.mVisible || mToolTip->mVisible || mChooseState == CHOOSE_VIEW_LAWN)
		return;
	// Called with Board's graphics, so both the anchor and bounds are board local.
	const int aRight = SeedChooserLayout::RebaseX(mApp->mScreenBounds.mWidth,
		mApp->mScreenBounds.mX, mBoard->mX);
	aTip.Draw(g,
		Rect(aTip.mMinLeft, -mBoard->mY, aRight - aTip.mMinLeft, aTip.mMaxBottom));
}

//0x484C30
void SeedChooserScreen::UpdateViewLawn()
{
	if (mChooseState != CHOOSE_VIEW_LAWN) return;
	const auto nowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	const auto elapsedNs = nowNs - mViewLawnStartTimeNs;
	if (mViewLawnReturnTimeNs < 0 && elapsedNs >= 1'000'000'000)
	{
		mBoard->DisplayAdviceAgain(gStr.CLICK_TO_CONTINUE, MESSAGE_STYLE_HINT_STAY, ADVICE_CLICK_TO_CONTINUE);
		mViewLawnReturnTimeNs = 0; // Hint consumed; awaiting the return click.
	}
	const int hiddenY = mBoard->mLandscapeHud ? BOARD_HEIGHT : SEED_CHOOSER_OFFSET_Y;
	const int chooserY = mBoard->mLandscapeHud ? 0 : SEED_CHOOSER_OFFSET_Y - DesktopUIAtlas::Images[size_t(DesktopUIAtlas::Sprite::SEEDCHOOSER_BACKGROUND)].mHeight;
	if (mViewLawnReturnTimeNs <= 0)
	{
		Move(mApp->mScreenBounds.mX, TodAnimateCurveNs(0, 400'000'000,
			elapsedNs, chooserY, hiddenY, Curves::EaseInOut));
		return;
	}
	const auto returnPhaseNs = 2'510'000'000 + nowNs - mViewLawnReturnTimeNs;
	if (returnPhaseNs <= 3'500'000'000)
	{
		mBoard->ClearAdvice(ADVICE_CLICK_TO_CONTINUE);
		Move(mApp->mScreenBounds.mX, TodAnimateCurveNs(3'100'000'000, 3'500'000'000,
			returnPhaseNs, hiddenY, chooserY, Curves::EaseInOut));
	}
	else
	{
		mChooseState = CHOOSE_NORMAL;
		mViewLawnStartTimeNs = 0;
		mViewLawnReturnTimeNs = -1;
		mMenuButton->mDisabled = false;
	}
}

//0x484FD0
void SeedChooserScreen::LandFlyingSeed(ChosenSeed& theChosenSeed)
{
	if (theChosenSeed.mSeedState == SEED_FLYING_TO_BANK)
	{
		theChosenSeed.mX = theChosenSeed.mEndX;
		theChosenSeed.mY = theChosenSeed.mEndY;
		theChosenSeed.mTimeStartMotion = 0;
		theChosenSeed.mTimeEndMotion = 0;
		theChosenSeed.mSeedState = SEED_IN_BANK;
		mSeedsInFlight--;
	}
	else if (theChosenSeed.mSeedState == SEED_FLYING_TO_CHOOSER)
	{
		theChosenSeed.mX = theChosenSeed.mEndX;
		theChosenSeed.mY = theChosenSeed.mEndY;
		theChosenSeed.mTimeStartMotion = 0;
		theChosenSeed.mTimeEndMotion = 0;
		theChosenSeed.mSeedState = SEED_IN_CHOOSER;
		mSeedsInFlight--;
		if (theChosenSeed.mSeedType == SEED_IMITATER)
		{
			theChosenSeed.mSeedState = SEED_PACKET_HIDDEN;
			theChosenSeed.mImitaterType = SEED_NONE;
			UpdateImitaterButton();
		}
	}
}

//0x485040
void SeedChooserScreen::UpdateCursor()
{
	if (mApp->GetDialogCount() || mBoard->mCutScene->IsInShovelTutorial()) return;
	SeedType aMouseSeedType = SeedHitTest(mLastMouseX, mLastMouseY);
	if (aMouseSeedType != SEED_NONE)
	{
		ChosenSeed& aMouseChosenSeed = mChosenSeeds[aMouseSeedType];
		if (aMouseChosenSeed.mSeedState == SEED_IN_BANK && aMouseChosenSeed.mCrazyDavePicked)
			aMouseSeedType = SEED_NONE;
	}

	if (mMouseVisible && mChooseState != CHOOSE_VIEW_LAWN && ((aMouseSeedType != SEED_NONE && !SeedNotAllowedToPick(aMouseSeedType)) ||
		mRandomButton->IsMouseOver() || mViewLawnButton->IsMouseOver() || mAlmanacButton->IsMouseOver() || mImitaterButton->IsMouseOver() ||
		mStoreButton->IsMouseOver() || mMenuButton->IsMouseOver() || mStartButton->IsMouseOver()))
		mApp->SetCursor(CURSOR_HAND);
	else
		mApp->SetCursor(CURSOR_POINTER);
}

//0x4851A0
void SeedChooserScreen::Update()
{
	RefreshChooserHudLayout(*this);
    if(mCardScroll.velocity!=0 && !mCardScroll.pressed) {
        const float minimum=mCardScrollMinimum;
        const int previous=FloatRoundToInt(mCardScroll.position);
        mCardScroll.Advance(ScrollTimeNs(),minimum,0);
        if(previous!=FloatRoundToInt(mCardScroll.position))ScrollCardsTo(mCardScroll.position);
    }
	Widget::Update();
	UpdateViewLawn();

	mLastMouseX = SeedChooserLayout::RebaseX(mApp->mWidgetManager->mLastMouseX, 0, mX);
	mLastMouseY = mApp->mWidgetManager->mLastMouseY - mY;

	mToolTip->Update();

	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		if (mApp->SeedTypeAvailable(aSeedType))
		{
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
			if (aChosenSeed.mSeedState == SEED_FLYING_TO_BANK || aChosenSeed.mSeedState == SEED_FLYING_TO_CHOOSER)
			{
				const auto aDurationNs = aChosenSeed.mTimeEndMotion - aChosenSeed.mTimeStartMotion;
				const auto aElapsedNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs - aChosenSeed.mTimeStartMotion;
				const auto aCurveTimeNs = static_cast<std::int64_t>(std::min(aElapsedNs, aDurationNs));
				aChosenSeed.mX = TodAnimateCurveNs(0, static_cast<std::int64_t>(aDurationNs), aCurveTimeNs, aChosenSeed.mStartX, aChosenSeed.mEndX, Curves::EaseInOut);
				aChosenSeed.mY = TodAnimateCurveNs(0, static_cast<std::int64_t>(aDurationNs), aCurveTimeNs, aChosenSeed.mStartY, aChosenSeed.mEndY, Curves::EaseInOut);
				if (aElapsedNs >= aDurationNs)
				{
					LandFlyingSeed(aChosenSeed);
				}
			}
		}
	}

	mStartButton->Update();
	mRandomButton->Update();
	mViewLawnButton->Update();
	mAlmanacButton->Update();
	mImitaterButton->Update();
	mStoreButton->Update();
	mMenuButton->Update();
	mBoard->mCutScene->UpdateSeedChooserCamera();
	ShowToolTip();
	if (mApp->mGameScene == SCENE_LEVEL_INTRO && mBoard->mCutScene->mSeedChoosing)
	{
		// Resolve the hit and label after the final camera and button positions.
		mBoard->UpdateToolTip();
	}
	UpdateCursor();
	MarkDirty();
}

//0x485340
bool SeedChooserScreen::DisplayRepickWarningDialog(const SexyChar* theMessage)
{
    ChooserModalSession aSession(*this);
    if (!aSession.IsCurrent())
        return false;
    const int aResult = mApp->LawnMessageBox(
		Dialogs::DIALOG_CHOOSER_WARNING, 
		gStr.DIALOG_WARNING.c_str(), 
		theMessage, 
		gStr.DIALOG_BUTTON_YES.c_str(), 
		gStr.REPICK_BUTTON.c_str(), 
		Dialog::BUTTONS_YES_NO
    );
    return aSession.IsCurrent() && aResult == Dialog::ID_YES;
}

//0x485370
bool SeedChooserScreen::FlyersAreComming()
{
	for (int aWave = 0; aWave < mBoard->mNumWaves; aWave++)
	{
		for (int anIndex = 0; anIndex < MAX_ZOMBIES_IN_WAVE; anIndex++)
		{
			ZombieType aZombieType = mBoard->mZombiesInWave[aWave][anIndex];
			if (aZombieType == ZOMBIE_INVALID)
				break;

			if (aZombieType == ZOMBIE_BALLOON)
				return true;
		}
	}
	return false;
}

//0x4853D0
bool SeedChooserScreen::FlyProtectionCurrentlyPlanted()
{
	Plant* aPlant = nullptr;
	while (mBoard->IteratePlants(aPlant))
	{
		if (aPlant->mSeedType == SEED_CATTAIL || aPlant->mSeedType == SEED_CACTUS)
		{
			return true;
		}
	}
	return false;
}

//0x485420
bool SeedChooserScreen::CheckSeedUpgrade(SeedType theSeedTypeTo, SeedType theSeedTypeFrom)
{
	if (mApp->IsSurvivalMode() || !PickedPlantType(theSeedTypeTo) || PickedPlantType(theSeedTypeFrom))
		return true;

	SexyString aWarning = gStr.SEED_CHOOSER_UPGRADE_WARNING;
	aWarning = TodReplaceString(aWarning, UiTextPlaceholder::UpgradeTo, Plant::GetNameString(theSeedTypeTo));
	aWarning = TodReplaceString(aWarning, UiTextPlaceholder::UpgradeFrom, Plant::GetNameString(theSeedTypeFrom));
	return DisplayRepickWarningDialog(aWarning.c_str());
}

//0x485600
void SeedChooserScreen::OnStartButton()
{
    ChooserModalSession aSession(*this);
    if (!aSession.IsCurrent())
        return;
	if (mApp->mGameMode == GAMEMODE_MINIGAME_SEEING_STARS && !PickedPlantType(SEED_STARFRUIT))
	{
		if (!DisplayRepickWarningDialog(gStr.SEED_CHOOSER_SEEING_STARS_WARNING.c_str()))
		{
			return;
		}
	}

	if (mApp->IsFirstTimeAdventureMode() && mBoard->mLevel == 11 && !PickedPlantType(SEED_PUFFSHROOM))
	{
		if (!DisplayRepickWarningDialog(gStr.SEED_CHOOSER_PUFFSHROOM_WARNING.c_str()))
		{
			return;
		}
	}
	if (!PickedPlantType(SEED_SUNFLOWER) && !PickedPlantType(SEED_TWINSUNFLOWER) && !PickedPlantType(SEED_SUNSHROOM) &&
		!mBoard->mCutScene->IsSurvivalRepick() && mApp->mGameMode != GAMEMODE_MINIGAME_LAST_STAND)
	{
		if (mApp->IsFirstTimeAdventureMode() && mBoard->mLevel == 11)
		{
			if (!DisplayRepickWarningDialog(gStr.SEED_CHOOSER_NIGHT_SUN_WARNING.c_str()))
			{
				return;
			}
		}
		else if (!DisplayRepickWarningDialog(gStr.SEED_CHOOSER_SUN_WARNING.c_str()))
		{
			return;
		}
	}
	if (mBoard->StageHasPool() && !PickedPlantType(SEED_LILYPAD) && !PickedPlantType(SEED_SEASHROOM) && !PickedPlantType(SEED_TANGLEKELP) && !mBoard->mCutScene->IsSurvivalRepick())
	{
		if (mApp->IsFirstTimeAdventureMode() && mBoard->mLevel == 21)
		{
			if (!DisplayRepickWarningDialog(gStr.SEED_CHOOSER_LILY_WARNING.c_str()))
			{
				return;
			}
		}
		else if (!DisplayRepickWarningDialog(gStr.SEED_CHOOSER_POOL_WARNING.c_str()))
		{
			return;
		}
	}
	if (mBoard->StageHasRoof() && !PickedPlantType(SEED_FLOWERPOT) && mApp->SeedTypeAvailable(SEED_FLOWERPOT))
	{
		if (!DisplayRepickWarningDialog(gStr.SEED_CHOOSER_ROOF_WARNING.c_str()))
		{
			return;
		}
	}

	if (mApp->mGameMode == GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT && !PickedPlantType(SEED_WALLNUT))
	{
		if (!DisplayRepickWarningDialog(gStr.SEED_CHOOSER_ART_WALLNUT_WARNING.c_str()))
		{
			return;
		}
	}
	if (mApp->mGameMode == GAMEMODE_LIMBO_ART_CHALLENGE_SUNFLOWER && 
		(!PickedPlantType(SEED_STARFRUIT) || !PickedPlantType(SEED_UMBRELLA) || !PickedPlantType(SEED_WALLNUT)))
	{
		if (!DisplayRepickWarningDialog(gStr.SEED_CHOOSER_ART_2_WARNING.c_str()))
		{
			return;
		}
	}

	if (FlyersAreComming() && !FlyProtectionCurrentlyPlanted() && !PickedPlantType(SEED_CATTAIL) && !PickedPlantType(SEED_CACTUS) && !PickedPlantType(SEED_BLOVER))
	{
		if (!DisplayRepickWarningDialog(gStr.SEED_CHOOSER_FLYER_WARNING.c_str()))
		{
			return;
		}
	}

	if (!CheckSeedUpgrade(SEED_GATLINGPEA, SEED_REPEATER) || 
		!CheckSeedUpgrade(SEED_WINTERMELON, SEED_MELONPULT) || 
		!CheckSeedUpgrade(SEED_TWINSUNFLOWER, SEED_SUNFLOWER) || 
		!CheckSeedUpgrade(SEED_SPIKEROCK, SEED_SPIKEWEED) || 
		!CheckSeedUpgrade(SEED_COBCANNON, SEED_KERNELPULT) || 
		!CheckSeedUpgrade(SEED_GOLD_MAGNET, SEED_MAGNETSHROOM) || 
		!CheckSeedUpgrade(SEED_GLOOMSHROOM, SEED_FUMESHROOM) || 
		!CheckSeedUpgrade(SEED_CATTAIL, SEED_LILYPAD))
		return;

	CloseSeedChooser();
}

//0x4859B0
void SeedChooserScreen::PickRandomSeeds()
{
	for (int anIndex = mSeedsInBank; anIndex < mBoard->mSeedBank->mNumPackets; anIndex++)
	{
		SeedType aSeedType;
		do aSeedType = (SeedType)Rand(mApp->GetSeedsAvailable());
		while (!mApp->SeedTypeAvailable(aSeedType) || aSeedType == SEED_IMITATER || mChosenSeeds[aSeedType].mSeedState != SEED_IN_CHOOSER);
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		aChosenSeed.mTimeStartMotion = 0;
		aChosenSeed.mTimeEndMotion = 0;
		aChosenSeed.mStartX = aChosenSeed.mX;
		aChosenSeed.mStartY = aChosenSeed.mY;
		GetSeedPositionInBank(anIndex, aChosenSeed.mEndX, aChosenSeed.mEndY);
		aChosenSeed.mSeedState = SEED_IN_BANK;
		aChosenSeed.mSeedIndexInBank = anIndex;
		mSeedsInBank++;
	}
	for (SeedType aSeedFlying = SEED_PEASHOOTER; aSeedFlying < NUM_SEEDS_IN_CHOOSER; aSeedFlying = (SeedType)(aSeedFlying + 1))
		LandFlyingSeed(mChosenSeeds[aSeedFlying]);
	CloseSeedChooser();
}

//0x485BB0
void SeedChooserScreen::ButtonDepress(int theId)
{
    ChooserModalSession aSession(*this);
    if (!aSession.IsCurrent())
        return;
	if (mSeedsInFlight > 0 || mChooseState == CHOOSE_VIEW_LAWN || !mMouseVisible)
		return;

    // C 形态 -> 成员函数内匿名 lambda 表（private 按钮枚举故表在函数内；
    // 下标平移 theId - SeedChooserScreen_ViewLawn）：ViewLawn/Almanac/Menu/
    // Store 四支；lambda 返回 false = 原支内 return（会话失效中止）。
    // Start/Random 支（GetSeedsAvailable 前置混排）表外保留。
    using SeedChooserBtnFn = bool (*)(SeedChooserScreen&, ChooserModalSession&);
    static constexpr SeedChooserBtnFn kChooserButtonCases[
        (int)SeedChooserScreen_Store - (int)SeedChooserScreen_ViewLawn + 1] = {
        [](SeedChooserScreen& s, ChooserModalSession& aSession) -> bool
        {
            s.mChooseState = CHOOSE_VIEW_LAWN;
            s.mMenuButton->mDisabled = true;
            s.mViewLawnStartTimeNs = s.mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
            s.mViewLawnReturnTimeNs = -1;
            return true;
        },  // 102 SeedChooserScreen_ViewLawn
        [](SeedChooserScreen& s, ChooserModalSession& aSession) -> bool
        {
            s.mApp->DoAlmanacDialog()->WaitForResult(true);
            if (!aSession.IsCurrent())
                return false;
            s.mApp->mMusic->MakeSureMusicIsPlaying(MUSIC_TUNE_CHOOSE_YOUR_SEEDS);
            return true;
        },  // 103 SeedChooserScreen_Almanac
        [](SeedChooserScreen& s, ChooserModalSession& aSession) -> bool
        {
            s.mMenuButton->mIsOver = false;
            s.mMenuButton->mIsDown = false;
            s.UpdateCursor();
            s.mApp->DoNewOptions(false);
            return true;
        },  // 104 SeedChooserScreen_Menu
        [](SeedChooserScreen& s, ChooserModalSession& aSession) -> bool
        {
            StoreScreen* aStore = s.mApp->ShowStoreScreen();
            aStore->mBackButton->mLabel = gStr.STORE_BACK_TO_GAME;
            aStore->WaitForResult();
            if (!aSession.IsCurrent())
                return false;
            if (aStore->mGoToTreeNow)
            {
                s.mApp->KillBoard();
                s.mApp->PreNewGame(GAMEMODE_TREE_OF_WISDOM, false);
            }
            else s.mApp->mMusic->MakeSureMusicIsPlaying(MUSIC_TUNE_CHOOSE_YOUR_SEEDS);
            return true;
        },  // 105 SeedChooserScreen_Store
    };
    static_assert(sizeof(kChooserButtonCases) / sizeof(kChooserButtonCases[0]) ==
        (int)SeedChooserScreen_Store - (int)SeedChooserScreen_ViewLawn + 1,
        "kChooserButtonCases 槽位须与 102..105 按钮枚举段同步");

    const int aButtonIndex = theId - (int)SeedChooserScreen_ViewLawn;
    if ((unsigned)aButtonIndex < sizeof(kChooserButtonCases) / sizeof(kChooserButtonCases[0]))
    {
        if (!kChooserButtonCases[aButtonIndex](*this, aSession))
            return;
    }
    else if (mApp->GetSeedsAvailable() >= mBoard->mSeedBank->mNumPackets)
    {
        if (theId == SeedChooserScreen::SeedChooserScreen_Start)
            OnStartButton();
        else if (theId == SeedChooserScreen::SeedChooserScreen_Random)
            PickRandomSeeds();
    }
}

//0x485D80
SeedType SeedChooserScreen::SeedHitTest(int x, int y)
{
	if (mMouseVisible)
	{
		for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
		{
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
			if (!mApp->SeedTypeAvailable(aSeedType) || aChosenSeed.mSeedState == SEED_PACKET_HIDDEN) continue;
			if((mBoard->mLandscapeHud||mCardScroll.position!=0) && aChosenSeed.mSeedState==SEED_IN_CHOOSER &&
               !mCardLayout->viewport.Contains(x,y)) continue;
            const int width=mCardLayout->cardWidth;
            const int height=mCardLayout->cardHeight;
            if (Rect(ChosenSeedPresentationX(*this, aChosenSeed), aChosenSeed.mY, width, height).Contains(x, y)) return aSeedType;
		}
	}
	return SEED_NONE;
}

//0x485E20
SeedType SeedChooserScreen::FindSeedInBank(int theIndexInBank)
{
	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		if (mApp->SeedTypeAvailable(aSeedType))
		{
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
			if (aChosenSeed.mSeedState == SEED_IN_BANK && aChosenSeed.mSeedIndexInBank == theIndexInBank) return aSeedType;
		}
	}
	return SEED_NONE;
}

void SeedChooserScreen::EnableStartButton(bool theEnabled)
{
	mStartButton->mDisabled = !theEnabled;
	if (theEnabled) mStartButton->mColors[GameButton::COLOR_LABEL] = Color::White;
	else mStartButton->mColors[GameButton::COLOR_LABEL] = Color(64, 64, 64);
}

//0x485E90
void SeedChooserScreen::ClickedSeedInBank(ChosenSeed& theChosenSeed)
{
    if(mBoard->mLandscapeHud && theChosenSeed.mSeedType!=SEED_IMITATER) {
        int x,y;GetSeedPositionInChooser(theChosenSeed.mSeedType,x,y);
        const auto viewport=LandscapeHud::ChooserViewport();
        if(y<viewport.mY)ScrollCardsTo(mCardScroll.position+viewport.mY-y);
        else if(y+53>viewport.mY+viewport.mHeight)ScrollCardsTo(mCardScroll.position+viewport.mY+viewport.mHeight-y-53);
        mCardScroll.Cancel();
    }
	for (int anIndex = theChosenSeed.mSeedIndexInBank + 1; anIndex < mBoard->mSeedBank->mNumPackets; anIndex++)
	{
		SeedType aSeedType = FindSeedInBank(anIndex);
		if (aSeedType != SEED_NONE)
		{
			ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
			aChosenSeed.mTimeStartMotion = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
			aChosenSeed.mTimeEndMotion = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
			aChosenSeed.mTimeEndMotion = ReanimClock::Add(aChosenSeed.mTimeEndMotion, 150'000'000);
			aChosenSeed.mStartX = aChosenSeed.mX;
			aChosenSeed.mStartY = aChosenSeed.mY;
			GetSeedPositionInBank(anIndex - 1, aChosenSeed.mEndX, aChosenSeed.mEndY);
			aChosenSeed.mSeedState = SEED_FLYING_TO_BANK;
			aChosenSeed.mSeedIndexInBank = anIndex - 1;
			mSeedsInFlight++;
		}
	}
	theChosenSeed.mTimeStartMotion = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	theChosenSeed.mTimeEndMotion = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	theChosenSeed.mTimeEndMotion = ReanimClock::Add(theChosenSeed.mTimeEndMotion, 250'000'000);
	theChosenSeed.mStartX = theChosenSeed.mX;
	theChosenSeed.mStartY = theChosenSeed.mY;
	GetSeedPositionInChooser(theChosenSeed.mSeedType, theChosenSeed.mEndX, theChosenSeed.mEndY);
	theChosenSeed.mSeedState = SEED_FLYING_TO_CHOOSER;
	theChosenSeed.mSeedIndexInBank = 0;
	mSeedsInBank--;
	mSeedsInFlight++;
	mToolTip->mVisible = false;
	mToolTipSeed = SEED_NONE;
	EnableStartButton(false);
	mApp->PlaySample(Sexy::SOUND_TAP);
}

//0x486030
void SeedChooserScreen::ClickedSeedInChooser(ChosenSeed& theChosenSeed)
{
	if (mSeedsInBank == mBoard->mSeedBank->mNumPackets)
		return;

	theChosenSeed.mTimeStartMotion = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	theChosenSeed.mTimeEndMotion = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	theChosenSeed.mTimeEndMotion = ReanimClock::Add(theChosenSeed.mTimeEndMotion, 250'000'000);
	theChosenSeed.mStartX = theChosenSeed.mX;
	theChosenSeed.mStartY = theChosenSeed.mY;
	GetSeedPositionInBank(mSeedsInBank, theChosenSeed.mEndX, theChosenSeed.mEndY);
	theChosenSeed.mSeedState = SEED_FLYING_TO_BANK;
	theChosenSeed.mSeedIndexInBank = mSeedsInBank;
	mSeedsInFlight++;
	mSeedsInBank++;

	mToolTip->mVisible = false;
	mToolTipSeed = SEED_NONE;
	mApp->PlaySample(Sexy::SOUND_TAP);
	if (mSeedsInBank == mBoard->mSeedBank->mNumPackets)
		EnableStartButton(true);
}

//0x486150
void SeedChooserScreen::ShowToolTip()
{
	if (!mApp->mWidgetManager->mMouseIn || !mApp->mActive || mApp->GetDialogCount() > 0 || mChooseState == CHOOSE_VIEW_LAWN)
	{
		mToolTip->mVisible = false;
		mToolTipSeed = SEED_NONE;
	}
	else if (mSeedsInFlight <= 0)
	{
		if (mImitaterButton->IsMouseOver() && mMouseVisible)
		{
			mToolTip->SetLabel(Plant::GetToolTip(SEED_IMITATER));
			mToolTip->SetTitle(Plant::GetNameString(SEED_IMITATER));
			mToolTip->SetWarningText((""));
			mToolTip->mX = (SEED_PACKET_WIDTH - mToolTip->mWidth) / 2 + mImitaterButton->mX;
			mToolTip->mY = mImitaterButton->mY - mToolTip->mHeight;
			mToolTip->mVisible = true;
		}
		else
		{
			SeedType aSeedType = SeedHitTest(mLastMouseX, mLastMouseY);
			if (aSeedType == SEED_NONE)
			{
				mToolTip->mVisible = false;
				mToolTipSeed = SEED_NONE;
			}
			else if (aSeedType != mToolTipSeed)
			{
				mToolTip->mVisible = false;
				mToolTipSeed = SEED_NONE;
				ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
				uint aRecFlags = SeedNotRecommendedToPick(aSeedType);
				if (SeedNotAllowedToPick(aSeedType))
				{
					mToolTip->SetWarningText(gStr.NOT_ALLOWED_ON_THIS_LEVEL);
				}
				else if (SeedNotAllowedDuringTrial(aSeedType))
				{
					mToolTip->SetWarningText(gStr.FULL_VERSION_ONLY);
				}
				else if (aChosenSeed.mSeedState == SEED_IN_BANK && aChosenSeed.mCrazyDavePicked)
				{
					mToolTip->SetWarningText(gStr.CRAZY_DAVE_WANTS);
				}
				else if (aRecFlags != 0U)
				{
					if (TestBit(aRecFlags, NOT_RECOMMENDED_NOCTURNAL))
					{
						mToolTip->SetWarningText(gStr.NOCTURNAL_WARNING);
					}
					else
					{
						// @Patoke: fix local name
						mToolTip->SetWarningText(gStr.NOT_RECOMMENDED_FOR_LEVEL);
					}
				}
				else
				{
					mToolTip->SetWarningText((""));
				}

				if (aSeedType == SEED_IMITATER)
				{
					mToolTip->SetTitle(Plant::GetNameString(aSeedType, aChosenSeed.mImitaterType));
					mToolTip->SetLabel(Plant::GetToolTip(aChosenSeed.mImitaterType));
				}
				else
				{
					mToolTip->SetTitle(Plant::GetNameString(aSeedType, SEED_NONE));
					mToolTip->SetLabel(Plant::GetToolTip(aSeedType));
				}
			}
			if (aSeedType != SEED_NONE)
			{
				ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
				int aSeedX, aSeedY;
				if (aChosenSeed.mSeedState == SEED_IN_BANK)
				{
					GetSeedPositionInBank(aChosenSeed.mSeedIndexInBank, aSeedX, aSeedY);
					aSeedX = SeedChooserLayout::RebaseX(aSeedX, mApp->mScreenBounds.mX, mX);
				}
				else
				{
					GetSeedPositionInChooser(aSeedType, aSeedX, aSeedY);
				}

				mToolTip->mX = ClampInt(((mBoard->mLandscapeHud ? 81 : SEED_PACKET_WIDTH) - mToolTip->mWidth) / 2 + aSeedX,
					mApp->mScreenBounds.mX - mX,
					mApp->mScreenBounds.mX + mApp->mScreenBounds.mWidth - mX - mToolTip->mWidth);
				mToolTip->mY = aSeedY + (mBoard->mLandscapeHud ? 53 : 70);
				mToolTip->mVisible = true;
				mToolTipSeed = aSeedType;
			}
		}
	}
}

//0x486600
void SeedChooserScreen::CancelLawnView()
{
	if (mChooseState == CHOOSE_VIEW_LAWN && mViewLawnReturnTimeNs == 0 &&
        mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs - mViewLawnStartTimeNs > 1'000'000'000)
        mViewLawnReturnTimeNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
}

//0x486630
void SeedChooserScreen::MouseUp(int x, int y, int theClickCount)
{
	const int aPreviousX = mX;
	RefreshChooserHudLayout(*this);
	x = SeedChooserLayout::RebaseX(x, aPreviousX, mX);
	if (theClickCount == 1)
	{
		if (mMenuButton->HitTest(x, y, this)) ButtonDepress(SeedChooserScreen::SeedChooserScreen_Menu);
		else if (mStartButton->HitTest(x, y, this)) ButtonDepress(SeedChooserScreen::SeedChooserScreen_Start);
		else if (mAlmanacButton->HitTest(x, y, this)) ButtonDepress(SeedChooserScreen::SeedChooserScreen_Almanac);
		else if (mStoreButton->HitTest(x, y, this)) ButtonDepress(SeedChooserScreen::SeedChooserScreen_Store);
	}
}

//0x4866E0
void SeedChooserScreen::UpdateImitaterButton()
{
	if (!mApp->SeedTypeAvailable(SEED_IMITATER))
	{
		mImitaterButton->mBtnNoDraw = true;
		mImitaterButton->mDisabled = true;
	}
	else
	{
		mImitaterButton->mBtnNoDraw = false;
		mImitaterButton->mDisabled = mChosenSeeds[SEED_IMITATER].mSeedState != SEED_PACKET_HIDDEN;
	}
}

//0x486770
void SeedChooserScreen::MouseDown(int x, int y, int theClickCount)
{
    if(mBoard->mLandscapeHud)mCardScroll.Cancel();
	const int aPreviousX = mX;
	RefreshChooserHudLayout(*this);
	x = SeedChooserLayout::RebaseX(x, aPreviousX, mX);
    ChooserModalSession aSession(*this);
    if (!aSession.IsCurrent())
        return;
	Widget::MouseDown(x, y, theClickCount);

	if (mSeedsInFlight > 0)
	{
		for (int i = 0; i < NUM_SEEDS_IN_CHOOSER; i++)
		{
			LandFlyingSeed(mChosenSeeds[i]);
		}
	}

	if (mChooseState == CHOOSE_VIEW_LAWN)
	{
		CancelLawnView();
	}
	else if (mRandomButton->HitTest(x, y, this))
	{
		mApp->PlaySample(Sexy::SOUND_TAP);
		ButtonDepress(SeedChooserScreen::SeedChooserScreen_Random);
	}
	else if (mViewLawnButton->HitTest(x, y, this))
	{
		mApp->PlaySample(Sexy::SOUND_TAP);
		ButtonDepress(SeedChooserScreen::SeedChooserScreen_ViewLawn);
	}
	else if (mMenuButton->HitTest(x, y, this))
	{
		mApp->PlaySample(Sexy::SOUND_GRAVEBUTTON);
	}
	else if (mStartButton->HitTest(x, y, this) || mAlmanacButton->HitTest(x, y, this) || mStoreButton->HitTest(x, y, this))
	{
		mApp->PlaySample(Sexy::SOUND_TAP);
	}
	else if (mImitaterButton->HitTest(x, y, this))
	{
		if (mSeedsInBank != mBoard->mSeedBank->mNumPackets)
		{
			mApp->PlaySample(Sexy::SOUND_TAP);
			ImitaterDialog* aDialog = new ImitaterDialog();
			mApp->AddDialog(aDialog->mId, aDialog);
			aDialog->Resize((mWidth - aDialog->mWidth) / 2, (mHeight - aDialog->mHeight) / 2, aDialog->mWidth, aDialog->mHeight);
			mApp->mWidgetManager->SetFocus(aDialog);
		}
	}
	else
	{
		if (!mBoard->mSeedBank->ContainsPoint(x+mX-mBoard->mX, y+mY-mBoard->mY) && !mAlmanacButton->HitTest(x, y, this) && !mStoreButton->HitTest(x, y, this) && mApp->CanShowAlmanac())
		{
			Zombie* aZombie = mBoard->ZombieHitTest(
				SeedChooserLayout::RebaseX(x, mX, mBoard->mX), y + mY - mBoard->mY);
			if (aZombie && aZombie->mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE && aZombie->mZombieType != ZOMBIE_REDEYE_GARGANTUAR)
			{
				mApp->PlaySample(Sexy::SOUND_TAP);
                mApp->DoAlmanacDialog(SEED_NONE, aZombie->mZombieType)->WaitForResult(true);
                if (!aSession.IsCurrent())
                    return;
				mApp->mMusic->MakeSureMusicIsPlaying(MUSIC_TUNE_CHOOSE_YOUR_SEEDS);
				return;
			}
		}
		
		SeedType aSeedType = SeedHitTest(x, y);
		if (aSeedType != SEED_NONE && !SeedNotAllowedToPick(aSeedType))
		{
			if (SeedNotAllowedDuringTrial(aSeedType))
			{
				mApp->PlaySample(Sexy::SOUND_TAP);
                const int aResult = mApp->LawnMessageBox(
					DIALOG_MESSAGE, 
					gStr.GET_FULL_VERSION_TITLE.c_str(), 
					gStr.GET_FULL_VERSION_BODY.c_str(), 
					gStr.GET_FULL_VERSION_YES_BUTTON.c_str(),
					gStr.GET_FULL_VERSION_NO_BUTTON.c_str(), 
					Dialog::BUTTONS_YES_NO
                );
                if (!aSession.IsCurrent())
                    return;
                if (aResult == Dialog::ID_YES)
				{
					if (mApp->mDRM)
					{
						mApp->mDRM->BuyGame();
					}
					mApp->DoBackToMain();
				}
			}
			else
			{
				ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
				if (aChosenSeed.mSeedState == SEED_IN_BANK)
				{
					if (aChosenSeed.mCrazyDavePicked)
					{
						mApp->PlaySample(Sexy::SOUND_BUZZER);
						mToolTip->mWarningFlashCounter = 70;
					}
					else ClickedSeedInBank(aChosenSeed);
				}
				else if (aChosenSeed.mSeedState == SEED_IN_CHOOSER)
					ClickedSeedInChooser(aChosenSeed);
			}
		}
	}
}

//0x486CE0
bool SeedChooserScreen::PickedPlantType(SeedType theSeedType)
{
	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		if (aChosenSeed.mSeedState == SEED_IN_BANK)
		{
			if (aChosenSeed.mSeedType == theSeedType || (aChosenSeed.mSeedType == SEED_IMITATER && aChosenSeed.mImitaterType == theSeedType))
			{
				return true;
			}
		}
	}
	return false;
}

//0x486D20
void SeedChooserScreen::CloseSeedChooser()
{
	DBG_ASSERT(mBoard->mSeedBank->mNumPackets == mBoard->GetNumSeedsInBank());
	for (int anIndex = 0; anIndex < mBoard->mSeedBank->mNumPackets; anIndex++)
	{
		SeedType aSeedType = FindSeedInBank(anIndex);
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		SeedPacket& aSeedPacket = mBoard->mSeedBank->mSeedPackets[anIndex];
		aSeedPacket.SetPacketType(aSeedType, aChosenSeed.mImitaterType);
		if (aChosenSeed.mRefreshing)
		{
			aSeedPacket.mRefreshStartTimeNs = aChosenSeed.mRefreshStartTimeNs;
			aSeedPacket.mRefreshTime = Plant::GetRefreshTime(aSeedPacket.mPacketType, aSeedPacket.mImitaterType);
			aSeedPacket.mRefreshing = true;
			aSeedPacket.mActive = false;
		}
	}
	mBoard->mCutScene->EndSeedChooser();
}

//0x486E80
void SeedChooserScreen::KeyDown(KeyCode theKey)
{
	mBoard->DoTypingCheck(theKey);
}

//0x486EA0
void SeedChooserScreen::KeyChar(SexyChar theChar)
{
	if (mChooseState == CHOOSE_VIEW_LAWN && (theChar == ' ' || theChar == '\r' || theChar == '\u001B'))
		CancelLawnView();
	else if (mApp->mTodCheatKeys && theChar == '\u001B')
		PickRandomSeeds();
	else mBoard->KeyChar(theChar);
}

//0x486F10
void SeedChooserScreen::UpdateAfterPurchase()
{
    RefreshCardLayout();
	for (SeedType aSeedType = SEED_PEASHOOTER; aSeedType < NUM_SEEDS_IN_CHOOSER; aSeedType = (SeedType)(aSeedType + 1))
	{
		ChosenSeed& aChosenSeed = mChosenSeeds[aSeedType];
		if (aChosenSeed.mSeedState == SEED_IN_BANK)
			GetSeedPositionInBank(aChosenSeed.mSeedIndexInBank, aChosenSeed.mX, aChosenSeed.mY);
		else if (aChosenSeed.mSeedState == SEED_IN_CHOOSER)
			GetSeedPositionInChooser(aSeedType, aChosenSeed.mX, aChosenSeed.mY);
		else continue;
		aChosenSeed.mStartX = aChosenSeed.mX;
		aChosenSeed.mStartY = aChosenSeed.mY;
		aChosenSeed.mEndX = aChosenSeed.mX;
		aChosenSeed.mEndY = aChosenSeed.mY;
	}
	EnableStartButton(mSeedsInBank == mBoard->mSeedBank->mNumPackets);
	UpdateImitaterButton();
}

void SeedChooserScreen::RefreshCardLayout()
{
    const bool expanded=Has7Rows();
    mCardLayout=mBoard->mLandscapeHud?&kLandscapeCards:expanded?&kExpandedClassicCards:&kClassicCards;
    mCardCount=expanded?NUM_SEEDS_IN_CHOOSER-1:std::min(40,NUM_SEEDS_IN_CHOOSER-1);
    mCardScrollMinimum=static_cast<float>(std::min(0,mCardLayout->viewport.mHeight-
        ((mCardCount+mCardLayout->columns-1)/mCardLayout->columns)*mCardLayout->pitchY));
    mCardScroll.position=std::clamp(mCardScroll.position,mCardScrollMinimum,0.0f);
}

void SeedChooserScreen::ScrollCardsTo(float position)
{
    const float minimum=mCardScrollMinimum;
    mCardScroll.position=std::clamp(position,minimum,0.0f);
    for(int i=0;i<NUM_SEEDS_IN_CHOOSER;++i) {
        auto& seed=mChosenSeeds[i];
        if(seed.mSeedState==SEED_IN_CHOOSER) {
            GetSeedPositionInChooser(i,seed.mX,seed.mY);
            seed.mStartX=seed.mEndX=seed.mX;seed.mStartY=seed.mEndY=seed.mY;
        } else if(seed.mSeedState==SEED_FLYING_TO_CHOOSER) {
            GetSeedPositionInChooser(i,seed.mEndX,seed.mEndY);
        }
    }
    MarkDirty();
}
void SeedChooserScreen::MouseWheel(int delta)
{
    if(mChooseState!=CHOOSE_NORMAL || !mMouseVisible) return;
    mCardScroll.Cancel();ScrollCardsTo(mCardScroll.position+delta*static_cast<float>(mCardLayout->pitchY));
}
const void* SeedChooserScreen::TouchHitTest(int x, int y)
{
    if(mCardScroll.dragging)return this;
    if (mMenuButton->HitTest(x, y, this)) return mMenuButton;
    if (mStartButton->HitTest(x, y, this)) return mStartButton;
    if (mAlmanacButton->HitTest(x, y, this)) return mAlmanacButton;
    if (mStoreButton->HitTest(x, y, this)) return mStoreButton;
    if (mRandomButton->HitTest(x, y, this)) return mRandomButton;
    if (mViewLawnButton->HitTest(x, y, this)) return mViewLawnButton;
    if (mImitaterButton->HitTest(x, y, this)) return mImitaterButton;
    return this;
}
void SeedChooserScreen::TouchBegan(const TouchEvent& event)
{
    if(mCardScroll.pressed)return;
    if(mBoard->mLandscapeHud && mChooseState==CHOOSE_NORMAL &&
       LandscapeHud::ChooserViewport().Contains(event.x,event.y)) {
        mTouchSeed=SeedHitTest(event.x,event.y);
        mCardScroll.Begin(event.id,event.x+mX,event.y+mY,ScrollTimeNs());
        return; // Select only after distinguishing a tap from a scroll.
    }
    if (TouchHitTest(event.x, event.y) == this) MouseDown(event.x,event.y,1);
}
void SeedChooserScreen::TouchMoved(const TouchEvent& event)
{
    if(!mCardScroll.pressed || mCardScroll.touchId!=event.id)return;
    const float minimum=mCardScrollMinimum;
    mCardScroll.Move(event.id,event.x+mX,event.y+mY,ScrollTimeNs(),minimum,0);
    ScrollCardsTo(mCardScroll.position);
}
void SeedChooserScreen::TouchEnded(const TouchEvent& event)
{
    if(mCardScroll.pressed) {
        if(mCardScroll.touchId!=event.id)return;
        TouchMoved(event);
        const bool moved=mCardScroll.End(event.id,ScrollTimeNs());
        if(!moved && mTouchSeed!=SEED_NONE && SeedHitTest(event.x,event.y)==mTouchSeed)
            MouseDown(event.x,event.y,1);
        mTouchSeed=SEED_NONE;
        return;
    }
    if (TouchHitTest(event.x,event.y)!=this) MouseDown(event.x,event.y,1);
    if (mApp->mSeedChooserScreen==this && mMouseVisible) MouseUp(event.x,event.y,1);
}
void SeedChooserScreen::TouchCanceled(const TouchEvent& event)
{
    if(mCardScroll.pressed && mCardScroll.touchId==event.id) {
        mCardScroll.Cancel();mTouchSeed=SEED_NONE;
    }
}
