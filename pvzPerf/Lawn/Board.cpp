#include <time.h>
#include <type_traits>
#include "ZenGarden.h"
#include "LandscapeHud.h"
#include "Sexy.TodLib/generated/SceneAtlasData.h"
#include "GardenHudLayout.h"
#include "BoardInclude.h"
#include "graphics/DDInterface.h"
#include "graphics/GameViewport.h"
#include "graphics/Image.h"
#include "ZombieFlags.h"
#include "LawnCommon.h"
#include "System/Music.h"
#include "System/SaveGame.h"
#include "Widget/LawnDialog.h"
#include "System/PlayerInfo.h"
#include "System/PoolEffect.h"
#include "System/PopDRMComm.h"
#include "Sexy.TodLib/generated/EntityStrings.h"  // 动态键实体表（阶段 3）
#include "System/TypingCheck.h"
#include "Widget/StoreScreen.h"
#include "Widget/AwardScreen.h"
#include "Widget/ChallengeScreen.h"
#include "Sexy.TodLib/TodDebug.h"
#include "Sexy.TodLib/TodFoley.h"
#include "Widget/SeedChooserScreen.h"
#include "Widget/SeedChooserLayout.h"
#include "Sexy.TodLib/Attachment.h"
#include "Sexy.TodLib/Reanimator.h"
#include "widget/Dialog.h"
#include "misc/MTRand.h"
#include "misc/FrameCounterMath.h"
#include "Sexy.TodLib/TodParticle.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "graphics/ImageFont.h"
#include "sound/BassLoader.h"
#include "sound/SoundManager.h"
#include "widget/ButtonWidget.h"
#include "widget/WidgetManager.h"
#include "sound/SoundInstance.h"

#include "Widget/AchievementsWidget.h"
#include <chrono>

// 命名污染审计：BoardIntervalFps 仅本 TU 使用，自 misc/FrameCounterMath.h 下沉（内部链接）。
namespace Sexy::FrameCounterMath
{
// 1073 0041D859: FILD frame delta; FMUL 1000.0; FADD 500.0; FIDIV elapsed;
// FSTP float. The caller retains the original signed elapsed > 10000 gate.
static constexpr float BoardIntervalFps(std::int32_t draws, std::int32_t elapsed) {
    return static_cast<float>((static_cast<double>(draws) * 1000.0 + 500.0) / elapsed);
}
}


//#define SEXY_MEMTRACE


//0x407B50
// GOTY @Patoke: 0x40A3C0
Board::Board(LawnApp* theApp)
{
    mPoolIntroVisual = true;
	mApp = theApp;
    mLandscapeHud = theApp->mLandscapeUIActive && theApp->mGameMode != GAMEMODE_MINIGAME_SLOT_MACHINE &&
        theApp->mGameMode != GAMEMODE_LIMBO_ZEN_GARDEN && theApp->mGameMode != GAMEMODE_TREE_OF_WISDOM;
    mHudFlashStartNs.fill(-1);
	mApp->mBoard = this;

	mZombies.DataArrayInitialize(ZOMBIE_POOL_CAPACITY, DataArrayKind::Zombies);
	mPlants.DataArrayInitialize(PLANT_POOL_CAPACITY, DataArrayKind::Plants);
	mProjectiles.DataArrayInitialize(1024U, DataArrayKind::Projectiles);
	mCoins.DataArrayInitialize(1024U, DataArrayKind::Coins);
	RebuildPlantCellBucket();  // 函数域 static 桶跨 Board 存续，构造时必须清
	RebuildZombieRowBucket();  // 行段桶同款：构造时清跨局残留
	mLawnMowers.DataArrayInitialize(32U, DataArrayKind::LawnMowers);
	mGridItems.DataArrayInitialize(128U, DataArrayKind::GridItems);

	mApp->mEffectSystem->EffectSystemFreeAll();
	mBoardRandSeed = mApp->mAppRandSeed;
	if (mApp->IsSurvivalMode())
	{
		mBoardRandSeed = Rand();
	}
	mCoinBankFadeCount = 0;
	mLevel = 0;
	mCursorObject = new CursorObject();
	mCursorPreview = new CursorPreview();
	mSeedBank = new SeedBank();
	mCutScene = new CutScene();
	mSpecialGraveStoneX = -1;
	mSpecialGraveStoneY = -1;
	for (int i = 0; i < MAX_GRID_SIZE_X; i++)
	{
		for (int j = 0; j < MAX_GRID_SIZE_Y; j++)
		{
			mGridSquareType[i][j] = GridSquareType::GRIDSQUARE_GRASS;
			mGridCelLook[i][j] = Rand(20);
			mGridCelOffset[i][j][0] = Rand(10) - 5;
			mGridCelOffset[i][j][1] = Rand(10) - 5;
		}

		for (int k = 0; k < MAX_GRID_SIZE_Y + 1; k++)
		{
			mGridCelFog[i][k] = 0;
		}
	}
	mFogOffset = 0.0f;
	mSunCountDown = 0;
	mShakeCounter = 0;
	mShakeAmountX = 0;
	mShakeAmountY = 0;
	mPaused = false;
	mLevelAwardSpawned = false;
	mFlagRaiseCounter = 0;
	mIceTrapCounter = 0;
	mLevelComplete = false;
	mBoardFadeOutCounter = -1;
	mNextSurvivalStageCounter = 0;
	mScoreNextMowerCounter = 0;
	mProgressMeterWidth = 0;
	mPoolSparklyParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
	mFogBlownCountDown = 0;
	mFwooshCountDown = 0;
	mTimeStopCounter = 0;
	mCobCannonCursorDelayCounter = 0;
	mCobCannonMouseX = 0;
	mCobCannonMouseY = 0;
	mDroppedFirstCoin = false;
	mBonusLawnMowersRemaining = 0;
	mEnableGraveStones = false;
	mHelpIndex = AdviceType::ADVICE_NONE;
	mEffectCounter = 0;
	mDrawCount = 0;
	mRiseFromGraveCounter = 0;
	mFinalWaveSoundCounter = 0;
	mKilledYeti = false;
	mTriggeredLawnMowers = 0;
	mPlayTimeActiveLevel = 0;
	mPlayTimeInactiveLevel = 0;
	mMaxSunPlants = 0;
	mStartDrawTime = 0;
	mIntervalDrawTime = 0;
	mIntervalDrawCountStart = 0;
	mPreloadTime = 0;
	mGameID = std::time(nullptr);
	mMinFPS = 1000.0f;
	mGravesCleared = 0;
	mPlantsEaten = 0;
	mPlantsShoveled = 0;
	mCoinsCollected = 0;
	mDiamondsCollected = 0;
	mPottedPlantsCollected = 0;
	mChocolateCollected = 0;
	mPeaShooterUsed = false; // @Patoke: added construct
	mCatapultPlantsUsed = false; // @Patoke: added construct
	mLevelCoinsCollected = 0;
	mGargantuarsKillsByCornCob = 0;
	mMushroomAndCoffeeBeansOnly = true; // @Patoke: added construct
	mMushroomsUsed = false; // @Patoke: added construct
	for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
	{
		for (int x = 0; x < 12; x++)
		{
			mFwooshID[y][x] = ReanimationID::REANIMATIONID_NULL;
		}
	}
	mFinalBossKilled = false;
	mPrevMouseX = -1;
	mPrevMouseY = -1;
	mMustacheMode = mApp->mMustacheMode;
	mSuperMowerMode = mApp->mSuperMowerMode;
	mFutureMode = mApp->mFutureMode;
	mPinataMode = mApp->mPinataMode;
	mDanceMode = mApp->mDanceMode;
	mDaisyMode = mApp->mDaisyMode;
	mSukhbirMode = mApp->mSukhbirMode;
	mShowShovel = false;
	mToolTip = new ToolTipWidget();
	mDebugFont = mApp->GetDebugFont()->Duplicate();
	mAdvice = new MessageWidget(mApp);
	mBackground = BackgroundType::BACKGROUND_1_DAY;
	mLevelTimeNs = 0;
	mTutorialState = TutorialState::TUTORIAL_OFF;
	mTutorialTimer = -1;
	mTutorialParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
	mChallenge = new Challenge();
	mClip = false;
	mDebugTextMode = DebugTextMode::DEBUG_TEXT_NONE;
	mMenuButton = new GameButton(0);
	mMenuButton->mDrawStoneButton = true;
	mStoreButton = nullptr;
	mIgnoreMouseUp = false;

	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		mMenuButton->mLabel = gStr.MAIN_MENU_BUTTON;
		mMenuButton->mParentWidget = this;
		mMenuButton->mX = 628;
		mMenuButton->mY = -10;
		mMenuButton->mWidth = 163;
		mMenuButton->mHeight = 46;

		mStoreButton = new GameButton(1);
		mStoreButton->mButtonImage = IMAGE_ZENSHOPBUTTON;
		mStoreButton->mOverImage = IMAGE_ZENSHOPBUTTON_HIGHLIGHT;
		mStoreButton->mDownImage = IMAGE_ZENSHOPBUTTON_HIGHLIGHT;
		mStoreButton->mParentWidget = this;
		mStoreButton->mX = 678;
		mStoreButton->mY = 33;
		mStoreButton->mWidth = IMAGE_ZENSHOPBUTTON->mWidth;
		mStoreButton->mHeight = 40;
	}
	else
	{
		mMenuButton->mLabel = gStr.MENU_BUTTON;
		mMenuButton->mX = 681;
		mMenuButton->mY = -10;
		mMenuButton->mWidth = 117;
		mMenuButton->mHeight = 46;
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND)
	{
		mStoreButton = new GameButton(1);
		mStoreButton->mDrawStoneButton = true;
		mStoreButton->mBtnNoDraw = true;
		mStoreButton->mDisabled = true;
	}

	RefreshHudLayout();
}

//0x408670、0x408690
Board::~Board()
{
    if (mWidgetManager) mWidgetManager->CancelTouches(this);
    DisposeTouchState();
	delete mAdvice;
	delete mCursorObject;
	delete mCursorPreview;
	delete mSeedBank;
	if (mMenuButton)
	{
		delete mMenuButton;
	}
	if (mStoreButton)
	{
		delete mStoreButton;
	}
	mZombies.DataArrayDispose();
	mPlants.DataArrayDispose();
	mProjectiles.DataArrayDispose();
	mCoins.DataArrayDispose();
	mLawnMowers.DataArrayDispose();
	mGridItems.DataArrayDispose();
	if (mToolTip)
	{
		delete mToolTip;
	}
	if (mDebugFont)
	{
		delete mDebugFont;
	}
	delete mCutScene;
	delete mChallenge;
}

//0x408A70
// GOTY @Patoke: 0x40B320
void Board::DisposeBoard()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
		mApp->mZenGarden->LeaveGarden();
	if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
		mChallenge->TreeOfWisdomLeave();

	mApp->mSoundSystem->StopFoley(FoleyType::FOLEY_RAIN);
	mApp->mZenGarden->mBoard = nullptr;
	mApp->CrazyDaveDie();
	mApp->mEffectSystem->EffectSystemFreeAll();
}

//0x408B00
bool Board::AreEnemyZombiesOnScreen()
{
	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		if (aZombie->mHasHead && !aZombie->IsDeadOrDying() && !aZombie->mMindControlled)
		{
			return true;
		}
	}
	return false;
}

//0x408B60
// GOTY @Patoke: 0x40B4A0
int Board::CountZombiesOnScreen()
{
	int aCount = 0;
	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		if (aZombie->mHasHead && !aZombie->IsDeadOrDying() && !aZombie->mMindControlled && aZombie->IsOnBoard())
		{
			aCount++;
		}
	}
	return aCount;
}

// GOTY @Patoke: 0x40B3B0
int Board::GetLiveGargantuarCount() {
	int aCount = 0;
	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		if (!aZombie->mDead && aZombie->mHasHead && !aZombie->IsDeadOrDying() && aZombie->IsOnBoard() && (aZombie->mZombieType == ZombieType::ZOMBIE_GARGANTUAR || aZombie->mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR))
		{
			aCount++;
		}
	}
	return aCount;
}

//0x408BF0
int Board::CountUntriggerLawnMowers()
{
	int aCount = 0;
	LawnMower* aLawnMower = nullptr;
	while (IterateLawnMowers(aLawnMower))
	{
		if (aLawnMower->mMowerState != LawnMowerState::MOWER_TRIGGERED && aLawnMower->mMowerState != LawnMowerState::MOWER_SQUISHED)
		{
			aCount++;
		}
	}
	return aCount;
}

//0x408C30
void Board::TryToSaveGame()
{
	std::string aFileName = GetSavedGameName(mApp->mGameMode, mApp->mPlayerInfo->mId);

	if (NeedSaveGame())
	{
		if (mBoardFadeOutCounter > 0)
		{
			CompleteEndLevelSequenceForSaving();
			return;
		}

		MkDir(GetAppDataFolder());
		mApp->mMusic->GameMusicPause(true);
		LawnSaveGame(this, aFileName);
		mApp->ClearUpdateBacklog();
		SurvivalSaveScore();
	}
}

//0x408DA0
bool Board::NeedSaveGame()
{
	return
		mApp->mGameMode != GameMode::GAMEMODE_LIMBO_ICE &&
		mApp->mGameMode != GameMode::GAMEMODE_INTRO &&
		mApp->mGameMode != GameMode::GAMEMODE_LIMBO_ZEN_GARDEN &&
		mApp->mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM &&
		mApp->mGameScene == GameScenes::SCENE_PLAYING;
}

void Board::SaveGame(const std::string& theFileName)
{
	LawnSaveGame(this, theFileName);
}

// GOTY @Patoke: 0x40B739
void Board::ResetFPSStats()
{
	unsigned int aTickCount = SexyGetTickCount();
	mStartDrawTime = aTickCount;
	mIntervalDrawTime = aTickCount;
	mDrawCount = 1;
	mIntervalDrawCountStart = 1;
}

//0x408DE0
// GOTY @Patoke: 0x40B710
bool Board::LoadGame(const std::string& theFileName)
{
	if (!LawnLoadGame(this, theFileName))
		return false;

	LoadBackgroundImages();
	mApp->ClearUpdateBacklog();
	ResetFPSStats();
	UpdateLayers();
	return true;
}

//0x408E40
GridItem* Board::GetGridItemAt(GridItemType theGridItemType, int theGridX, int theGridY)
{
	GridItem* aGridItem = nullptr;
	while (IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridX == theGridX && aGridItem->mGridY == theGridY && aGridItem->mGridItemType == theGridItemType)
		{
			return aGridItem;
		}
	}
	return nullptr;
}

//0x408E90
GridItem* Board::GetRake()
{
	GridItem* aGridItem = nullptr;
	while (IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_RAKE)
		{
			return aGridItem;
		}
	}
	return nullptr;
}

GridItem* Board::GetCraterAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_CRATER, theGridX, theGridY);
}

GridItem* Board::GetGraveStoneAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_GRAVESTONE, theGridX, theGridY);
}

GridItem* Board::GetLadderAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_LADDER, theGridX, theGridY);
}

GridItem* Board::GetScaryPotAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_SCARY_POT, theGridX, theGridY);
}

/*
GridItem* Board::GetSquirrelAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_SQUIRREL, theGridX, theGridY);
}
*/

GridItem* Board::GetZenToolAt(int theGridX, int theGridY)
{
	return GetGridItemAt(GridItemType::GRIDITEM_ZEN_TOOL, theGridX, theGridY);
}

//0x408ED0
bool Board::CanAddGraveStoneAt(int theGridX, int theGridY)
{
	if (mGridSquareType[theGridX][theGridY] != GridSquareType::GRIDSQUARE_GRASS && mGridSquareType[theGridX][theGridY] != GridSquareType::GRIDSQUARE_HIGH_GROUND)
	{
		return false;
	}

	GridItem* aGridItem = nullptr;
	while (IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridX == theGridX && aGridItem->mGridY == theGridY)
		{
			if (aGridItem->mGridItemType == GridItemType::GRIDITEM_GRAVESTONE ||
				aGridItem->mGridItemType == GridItemType::GRIDITEM_CRATER ||
				aGridItem->mGridItemType == GridItemType::GRIDITEM_LADDER)
				return false;
		}
	}
	return true;
}

int Board::MakeRenderOrder(RenderLayer theRenderLayer, int theRow, int theLayerOffset)
{
	return theRow * (int)RenderLayer::RENDER_LAYER_ROW_OFFSET + theRenderLayer + theLayerOffset;
}

//0x408F40
GridItem* Board::AddALadder(int theGridX, int theGridY)
{
	GridItem* aLadder = mGridItems.DataArrayAlloc();
	aLadder->mGridItemType = GridItemType::GRIDITEM_LADDER;
	aLadder->mRenderOrder = MakeRenderOrder(RenderLayer::RENDER_LAYER_PLANT, theGridY, 800);
	aLadder->mGridX = theGridX;
	aLadder->mGridY = theGridY;
	return aLadder;
}

//0x408F80
GridItem* Board::AddACrater(int theGridX, int theGridY)
{
	GridItem* aCrater = mGridItems.DataArrayAlloc();
	aCrater->mGridItemType = GridItemType::GRIDITEM_CRATER;
	aCrater->mRenderOrder = MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, theGridY, 1);
	aCrater->mGridX = theGridX;
	aCrater->mGridY = theGridY;
	return aCrater;
}

GridItem* Board::AddAGraveStone(int theGridX, int theGridY)
{
	GridItem* aGraveStone = mGridItems.DataArrayAlloc();
	aGraveStone->mGridItemType = GridItemType::GRIDITEM_GRAVESTONE;
	aGraveStone->mStateStartTimeNs = static_cast<std::int64_t>(mLevelTimeNs) + static_cast<std::int64_t>(Rand(50)) * 10'000'000;
	// Before the intro enables runtime graves, scenery is already fully risen.
	// Keep the random draw above so later gameplay RNG is unchanged.
	if (!mEnableGraveStones)
		aGraveStone->mStateStartTimeNs = static_cast<std::int64_t>(mLevelTimeNs) - 1'000'000'000;
	aGraveStone->mRenderOrder = MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, theGridY, 3);
	aGraveStone->mGridX = theGridX;
	aGraveStone->mGridY = theGridY;
	return aGraveStone;
}

//0x408FC0
void Board::AddGraveStones(int theGridX, int theCount, MTRand& theLevelRNG)
{
	TOD_ASSERT(theCount <= MAX_GRID_SIZE_Y);

	// 这里姑且加一个原版没有的、对于本列能否生成墓碑的判断
	// 如果没有这个判断，当本列不存在足够多的格子可以放置墓碑时，游戏会卡死
	//GridItem* aGridItem = nullptr;
	//bool aAllowGraveStone[MAX_GRID_SIZE_Y] = { false };
	int aGridAllowGraveStonesCount = 0;
	for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
	{
		if (CanAddGraveStoneAt(theGridX, y))
		{
			aGridAllowGraveStonesCount++;
		}
	}
	theCount = std::min(theCount, aGridAllowGraveStonesCount);

	int i = 0;
	while (i < theCount)
	{
		int aGridY = theLevelRNG.Next((unsigned int)MAX_GRID_SIZE_Y);
		//if (aAllowGraveStone[aGridY])
		//{
		//	aAllowGraveStone[aGridY] = false;
		//	GridItem* aGraveStone = AddAGraveStone(theGridX, aGridY);
		//	++i;
		//}
		// 上述写法虽然效率更高，但当 AddAGraveStone() 函数被修改后，不能保证 aAllowGraveStone 仍然有效
		// 故这里仍然采用如下的原版的写法，仅在上面对 theCount 进行修正
		if (CanAddGraveStoneAt(theGridX, aGridY))
		{
			GridItem* aGraveStone = AddAGraveStone(theGridX, aGridY);
			(void)aGraveStone; // unused
			++i;
		}
	}
}

//0x409050
int Board::GetNumWavesPerFlag()
{
	return (mApp->IsFirstTimeAdventureMode() && mNumWaves < 10) ? mNumWaves : 10;
}

//0x409080
bool Board::IsFlagWave(int theWaveNumber)
{
	if (mApp->IsFirstTimeAdventureMode() && mLevel == 1)
		return false;

	int aWavesPerFlag = GetNumWavesPerFlag();
	return theWaveNumber % aWavesPerFlag == aWavesPerFlag - 1;
}

//0x4090F0
void ZombiePickerInitForWave(ZombiePicker* theZombiePicker)
{
	// GOTY 1073 (0040BA20): only reset this wave; 0040BAA0 resets the cumulative array.
	// PickZombieType needs earlier waves' totals for the survival red-eye limit.
	theZombiePicker->mZombieCount = 0;
	theZombiePicker->mZombiePoints = 0;
	memset(theZombiePicker->mZombieTypeCount, 0, sizeof(theZombiePicker->mZombieTypeCount));
}

//0x409170
void ZombiePickerInit(ZombiePicker* theZombiePicker)
{
	ZombiePickerInitForWave(theZombiePicker);
	memset(theZombiePicker->mAllWavesZombieTypeCount, 0, sizeof(theZombiePicker->mAllWavesZombieTypeCount));
}

//0x409240
void Board::PutZombieInWave(ZombieType theZombieType, int theWaveNumber, ZombiePicker* theZombiePicker)
{
	TOD_ASSERT(theWaveNumber < MAX_ZOMBIE_WAVES && theZombiePicker->mZombieCount < MAX_ZOMBIES_IN_WAVE);
	mZombiesInWave[theWaveNumber][theZombiePicker->mZombieCount++] = theZombieType;
	if (theZombiePicker->mZombieCount < MAX_ZOMBIES_IN_WAVE)
	{
		mZombiesInWave[theWaveNumber][theZombiePicker->mZombieCount] = ZombieType::ZOMBIE_INVALID;
	}
	theZombiePicker->mZombiePoints -= GetZombieDefinition(theZombieType).mZombieValue;
	theZombiePicker->mZombieTypeCount[theZombieType]++;
	theZombiePicker->mAllWavesZombieTypeCount[theZombieType]++;
}

//0x409290
void Board::PutInMissingZombies(int theWaveNumber, ZombiePicker* theZombiePicker)
{
	for (ZombieType aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType = (ZombieType)((int)aZombieType + 1))
	{
		if (theZombiePicker->mZombieTypeCount[(int)aZombieType] <= 0 && aZombieType != ZombieType::ZOMBIE_YETI && CanZombieSpawnOnLevel(aZombieType, mLevel))
		{
			PutZombieInWave(aZombieType, theWaveNumber, theZombiePicker);
		}
	}
}

namespace
{
// 模式→关卡总波数表（B 形态；仅非冒险分支查表）：GameMode 直下标；40 = 原兜底 else。
// 表外：冒险 [0]（外层 gZombieWaves 关卡阶梯）、生存 [1..15] 与坚不可摧 [31]
// （= GetNumWavesPerSurvivalStage()，见调用点前置分支）。其余未列模式 = 原链落空 40。
constexpr int kNumWavesPerMode[GameMode::NUM_GAME_MODES] = {
	/* [ 0] GAMEMODE_ADVENTURE  (v2 0) */ 40,
	/* [ 1] GAMEMODE_MINIGAME_BEGHOULED  (v2 4) */ 40,
	/* [ 2] GAMEMODE_MINIGAME_BEGHOULED_TWIST  (v2 5) */ 40,
	/* [ 3] GAMEMODE_MINIGAME_BOBSLED_BONANZA  (v2 7) */ 40,
	/* [ 4] GAMEMODE_MINIGAME_COLUMN  (v2 9) */ 30,
	/* [ 5] GAMEMODE_MINIGAME_FINAL_BOSS  (v2 10) */ 40,
	/* [ 6] GAMEMODE_MINIGAME_INVISIGHOUL  (v2 14) */ 20,
	/* [ 7] GAMEMODE_MINIGAME_LAST_STAND  (v2 15) */ 40,
	/* [ 8] GAMEMODE_MINIGAME_LITTLE_TROUBLE  (v2 16) */ 30,
	/* [ 9] GAMEMODE_MINIGAME_POGO_PARTY  (v2 17) */ 30,
	/* [10] GAMEMODE_MINIGAME_PORTAL_COMBAT  (v2 18) */ 20,
	/* [11] GAMEMODE_MINIGAME_RAINING_SEEDS  (v2 19) */ 40,
	/* [12] GAMEMODE_MINIGAME_SEEING_STARS  (v2 21) */ 40,
	/* [13] GAMEMODE_MINIGAME_SLOT_MACHINE  (v2 23) */ 40,
	/* [14] GAMEMODE_MINIGAME_SPEED  (v2 24) */ 40,
	/* [15] GAMEMODE_MINIGAME_WALLNUT_BOWLING  (v2 28) */ 20,
	/* [16] GAMEMODE_MINIGAME_WALLNUT_BOWLING_2  (v2 29) */ 30,
	/* [17] GAMEMODE_MINIGAME_WAR_AND_PEAS  (v2 30) */ 20,
	/* [18] GAMEMODE_MINIGAME_WAR_AND_PEAS_2  (v2 31) */ 30,
	/* [19] GAMEMODE_MINIGAME_WHACK_A_ZOMBIE  (v2 32) */ 12,
	/* [20] GAMEMODE_MINIGAME_ZOMBIQUARIUM  (v2 34) */ 40,
	/* [21] GAMEMODE_LIMBO_AIR_RAID  (v2 1) */ 20,
	/* [22] GAMEMODE_LIMBO_ART_CHALLENGE_SUNFLOWER  (v2 2) */ 40,
	/* [23] GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT  (v2 3) */ 40,
	/* [24] GAMEMODE_LIMBO_BIG_TIME  (v2 6) */ 40,
	/* [25] GAMEMODE_LIMBO_BUNGEE_BLITZ  (v2 8) */ 30,
	/* [26] GAMEMODE_LIMBO_GRAVE_DANGER  (v2 11) */ 20,
	/* [27] GAMEMODE_LIMBO_HIGH_GRAVITY  (v2 12) */ 20,
	/* [28] GAMEMODE_LIMBO_ICE  (v2 13) */ 40,
	/* [29] GAMEMODE_LIMBO_RESODDED  (v2 20) */ 40,
	/* [30] GAMEMODE_LIMBO_SHOVEL  (v2 22) */ 30,
	/* [31] GAMEMODE_LIMBO_SQUIRREL  (v2 25) */ 0,
	/* [32] GAMEMODE_LIMBO_STORMY_NIGHT  (v2 26) */ 30,
	/* [33] GAMEMODE_LIMBO_SUNNY_DAY  (v2 27) */ 40,
	/* [34] GAMEMODE_LIMBO_ZEN_GARDEN  (v2 33) */ 0,
	/* [35] GAMEMODE_INTRO  (v2 35) */ 40,
	/* [36] GAMEMODE_PUZZLE_I_ZOMBIE_1  (v2 36) */ 40,
	/* [37] GAMEMODE_PUZZLE_I_ZOMBIE_2  (v2 37) */ 40,
	/* [38] GAMEMODE_PUZZLE_I_ZOMBIE_3  (v2 38) */ 40,
	/* [39] GAMEMODE_PUZZLE_I_ZOMBIE_4  (v2 39) */ 40,
	/* [40] GAMEMODE_PUZZLE_I_ZOMBIE_5  (v2 40) */ 40,
	/* [41] GAMEMODE_PUZZLE_I_ZOMBIE_6  (v2 41) */ 40,
	/* [42] GAMEMODE_PUZZLE_I_ZOMBIE_7  (v2 42) */ 40,
	/* [43] GAMEMODE_PUZZLE_I_ZOMBIE_8  (v2 43) */ 40,
	/* [44] GAMEMODE_PUZZLE_I_ZOMBIE_9  (v2 44) */ 40,
	/* [45] GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS  (v2 45) */ 40,
	/* [46] GAMEMODE_SCARY_POTTER_1  (v2 46) */ 40,
	/* [47] GAMEMODE_SCARY_POTTER_2  (v2 47) */ 40,
	/* [48] GAMEMODE_SCARY_POTTER_3  (v2 48) */ 40,
	/* [49] GAMEMODE_SCARY_POTTER_4  (v2 49) */ 40,
	/* [50] GAMEMODE_SCARY_POTTER_5  (v2 50) */ 40,
	/* [51] GAMEMODE_SCARY_POTTER_6  (v2 51) */ 40,
	/* [52] GAMEMODE_SCARY_POTTER_7  (v2 52) */ 40,
	/* [53] GAMEMODE_SCARY_POTTER_8  (v2 53) */ 40,
	/* [54] GAMEMODE_SCARY_POTTER_9  (v2 54) */ 40,
	/* [55] GAMEMODE_SCARY_POTTER_ENDLESS  (v2 55) */ 40,
	/* [56] GAMEMODE_SURVIVAL_ENDLESS_STAGE_1  (v2 56) */ 40,
	/* [57] GAMEMODE_SURVIVAL_ENDLESS_STAGE_2  (v2 57) */ 40,
	/* [58] GAMEMODE_SURVIVAL_ENDLESS_STAGE_3  (v2 58) */ 40,
	/* [59] GAMEMODE_SURVIVAL_ENDLESS_STAGE_4  (v2 59) */ 40,
	/* [60] GAMEMODE_SURVIVAL_ENDLESS_STAGE_5  (v2 60) */ 40,
	/* [61] GAMEMODE_SURVIVAL_HARD_STAGE_1  (v2 61) */ 40,
	/* [62] GAMEMODE_SURVIVAL_HARD_STAGE_2  (v2 62) */ 40,
	/* [63] GAMEMODE_SURVIVAL_HARD_STAGE_3  (v2 63) */ 40,
	/* [64] GAMEMODE_SURVIVAL_HARD_STAGE_4  (v2 64) */ 40,
	/* [65] GAMEMODE_SURVIVAL_HARD_STAGE_5  (v2 65) */ 40,
	/* [66] GAMEMODE_SURVIVAL_NORMAL_STAGE_1  (v2 66) */ 40,
	/* [67] GAMEMODE_SURVIVAL_NORMAL_STAGE_2  (v2 67) */ 40,
	/* [68] GAMEMODE_SURVIVAL_NORMAL_STAGE_3  (v2 68) */ 40,
	/* [69] GAMEMODE_SURVIVAL_NORMAL_STAGE_4  (v2 69) */ 40,
	/* [70] GAMEMODE_SURVIVAL_NORMAL_STAGE_5  (v2 70) */ 40,
	/* [71] GAMEMODE_TREE_OF_WISDOM  (v2 71) */ 0,
	/* [72] GAMEMODE_CUSTOM_LEVEL（哨兵；策略层覆写）*/ 10,
};
static_assert(sizeof(kNumWavesPerMode) / sizeof(kNumWavesPerMode[0]) == (int)GameMode::NUM_GAME_MODES,
	"总波数表槽位须与 GameMode 枚举尾同步");

// 模式→波点数倍率表（B 形态；仅非冒险分支查表）：GameMode 直下标；1 = 无倍率（原链落空）。
// 表外：冒险 [0] 的关卡重定向倍率（LittleTrouble/WallnutBowling 4、MiniBoss/StormyNight 3、
// BungeeBlitz 2）次序敏感，保留 if 链（见调用点）。
constexpr int kWavePointsMultPerMode[GameMode::NUM_GAME_MODES] = {
	/* [ 0] GAMEMODE_ADVENTURE  (v2 0) */ 1,
	/* [ 1] GAMEMODE_MINIGAME_BEGHOULED  (v2 4) */ 1,
	/* [ 2] GAMEMODE_MINIGAME_BEGHOULED_TWIST  (v2 5) */ 1,
	/* [ 3] GAMEMODE_MINIGAME_BOBSLED_BONANZA  (v2 7) */ 1,
	/* [ 4] GAMEMODE_MINIGAME_COLUMN  (v2 9) */ 6,
	/* [ 5] GAMEMODE_MINIGAME_FINAL_BOSS  (v2 10) */ 1,
	/* [ 6] GAMEMODE_MINIGAME_INVISIGHOUL  (v2 14) */ 2,
	/* [ 7] GAMEMODE_MINIGAME_LAST_STAND  (v2 15) */ 1,
	/* [ 8] GAMEMODE_MINIGAME_LITTLE_TROUBLE  (v2 16) */ 4,
	/* [ 9] GAMEMODE_MINIGAME_POGO_PARTY  (v2 17) */ 1,
	/* [10] GAMEMODE_MINIGAME_PORTAL_COMBAT  (v2 18) */ 2,
	/* [11] GAMEMODE_MINIGAME_RAINING_SEEDS  (v2 19) */ 1,
	/* [12] GAMEMODE_MINIGAME_SEEING_STARS  (v2 21) */ 1,
	/* [13] GAMEMODE_MINIGAME_SLOT_MACHINE  (v2 23) */ 1,
	/* [14] GAMEMODE_MINIGAME_SPEED  (v2 24) */ 1,
	/* [15] GAMEMODE_MINIGAME_WALLNUT_BOWLING  (v2 28) */ 4,
	/* [16] GAMEMODE_MINIGAME_WALLNUT_BOWLING_2  (v2 29) */ 4,
	/* [17] GAMEMODE_MINIGAME_WAR_AND_PEAS  (v2 30) */ 1,
	/* [18] GAMEMODE_MINIGAME_WAR_AND_PEAS_2  (v2 31) */ 1,
	/* [19] GAMEMODE_MINIGAME_WHACK_A_ZOMBIE  (v2 32) */ 1,
	/* [20] GAMEMODE_MINIGAME_ZOMBIQUARIUM  (v2 34) */ 1,
	/* [21] GAMEMODE_LIMBO_AIR_RAID  (v2 1) */ 1,
	/* [22] GAMEMODE_LIMBO_ART_CHALLENGE_SUNFLOWER  (v2 2) */ 1,
	/* [23] GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT  (v2 3) */ 1,
	/* [24] GAMEMODE_LIMBO_BIG_TIME  (v2 6) */ 1,
	/* [25] GAMEMODE_LIMBO_BUNGEE_BLITZ  (v2 8) */ 2,
	/* [26] GAMEMODE_LIMBO_GRAVE_DANGER  (v2 11) */ 1,
	/* [27] GAMEMODE_LIMBO_HIGH_GRAVITY  (v2 12) */ 1,
	/* [28] GAMEMODE_LIMBO_ICE  (v2 13) */ 1,
	/* [29] GAMEMODE_LIMBO_RESODDED  (v2 20) */ 1,
	/* [30] GAMEMODE_LIMBO_SHOVEL  (v2 22) */ 2,
	/* [31] GAMEMODE_LIMBO_SQUIRREL  (v2 25) */ 1,
	/* [32] GAMEMODE_LIMBO_STORMY_NIGHT  (v2 26) */ 1,
	/* [33] GAMEMODE_LIMBO_SUNNY_DAY  (v2 27) */ 1,
	/* [34] GAMEMODE_LIMBO_ZEN_GARDEN  (v2 33) */ 1,
	/* [35] GAMEMODE_INTRO  (v2 35) */ 1,
	/* [36] GAMEMODE_PUZZLE_I_ZOMBIE_1  (v2 36) */ 1,
	/* [37] GAMEMODE_PUZZLE_I_ZOMBIE_2  (v2 37) */ 1,
	/* [38] GAMEMODE_PUZZLE_I_ZOMBIE_3  (v2 38) */ 1,
	/* [39] GAMEMODE_PUZZLE_I_ZOMBIE_4  (v2 39) */ 1,
	/* [40] GAMEMODE_PUZZLE_I_ZOMBIE_5  (v2 40) */ 1,
	/* [41] GAMEMODE_PUZZLE_I_ZOMBIE_6  (v2 41) */ 1,
	/* [42] GAMEMODE_PUZZLE_I_ZOMBIE_7  (v2 42) */ 1,
	/* [43] GAMEMODE_PUZZLE_I_ZOMBIE_8  (v2 43) */ 1,
	/* [44] GAMEMODE_PUZZLE_I_ZOMBIE_9  (v2 44) */ 1,
	/* [45] GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS  (v2 45) */ 1,
	/* [46] GAMEMODE_SCARY_POTTER_1  (v2 46) */ 1,
	/* [47] GAMEMODE_SCARY_POTTER_2  (v2 47) */ 1,
	/* [48] GAMEMODE_SCARY_POTTER_3  (v2 48) */ 1,
	/* [49] GAMEMODE_SCARY_POTTER_4  (v2 49) */ 1,
	/* [50] GAMEMODE_SCARY_POTTER_5  (v2 50) */ 1,
	/* [51] GAMEMODE_SCARY_POTTER_6  (v2 51) */ 1,
	/* [52] GAMEMODE_SCARY_POTTER_7  (v2 52) */ 1,
	/* [53] GAMEMODE_SCARY_POTTER_8  (v2 53) */ 1,
	/* [54] GAMEMODE_SCARY_POTTER_9  (v2 54) */ 1,
	/* [55] GAMEMODE_SCARY_POTTER_ENDLESS  (v2 55) */ 1,
	/* [56] GAMEMODE_SURVIVAL_ENDLESS_STAGE_1  (v2 56) */ 1,
	/* [57] GAMEMODE_SURVIVAL_ENDLESS_STAGE_2  (v2 57) */ 1,
	/* [58] GAMEMODE_SURVIVAL_ENDLESS_STAGE_3  (v2 58) */ 1,
	/* [59] GAMEMODE_SURVIVAL_ENDLESS_STAGE_4  (v2 59) */ 1,
	/* [60] GAMEMODE_SURVIVAL_ENDLESS_STAGE_5  (v2 60) */ 1,
	/* [61] GAMEMODE_SURVIVAL_HARD_STAGE_1  (v2 61) */ 1,
	/* [62] GAMEMODE_SURVIVAL_HARD_STAGE_2  (v2 62) */ 1,
	/* [63] GAMEMODE_SURVIVAL_HARD_STAGE_3  (v2 63) */ 1,
	/* [64] GAMEMODE_SURVIVAL_HARD_STAGE_4  (v2 64) */ 1,
	/* [65] GAMEMODE_SURVIVAL_HARD_STAGE_5  (v2 65) */ 1,
	/* [66] GAMEMODE_SURVIVAL_NORMAL_STAGE_1  (v2 66) */ 1,
	/* [67] GAMEMODE_SURVIVAL_NORMAL_STAGE_2  (v2 67) */ 1,
	/* [68] GAMEMODE_SURVIVAL_NORMAL_STAGE_3  (v2 68) */ 1,
	/* [69] GAMEMODE_SURVIVAL_NORMAL_STAGE_4  (v2 69) */ 1,
	/* [70] GAMEMODE_SURVIVAL_NORMAL_STAGE_5  (v2 70) */ 1,
	/* [71] GAMEMODE_TREE_OF_WISDOM  (v2 71) */ 1,
	/* [72] GAMEMODE_CUSTOM_LEVEL（哨兵；策略层覆写）*/ 1,
};
static_assert(sizeof(kWavePointsMultPerMode) / sizeof(kWavePointsMultPerMode[0]) == (int)GameMode::NUM_GAME_MODES,
	"波点数倍率表槽位须与 GameMode 枚举尾同步");
}  // namespace

//0x4092E0
void Board::PickZombieWaves()
{
	// ====================================================================================================
	// ▲ 设定关卡总波数
	// ====================================================================================================
	if (mApp->IsAdventureMode())
	{
		if (mApp->IsWhackAZombieLevel())
		{
			mNumWaves = 8;
		}
		else
		{
			mNumWaves = gZombieWaves[ClampInt(mLevel - 1, 0, 49)];
			if (!mApp->IsFirstTimeAdventureMode() && !mApp->IsMiniBossLevel())
			{
				mNumWaves = mNumWaves < 10 ? 20 : mNumWaves + 10;
			}
		}
	}
	else
	{
		// B 形态 -> 常量表（上方 kNumWavesPerMode，GameMode 直下标；40 = 原兜底 else）；
		// 生存/坚不可摧 = GetNumWavesPerSurvivalStage()，异构调用表外前置
		if (mApp->IsSurvivalMode() || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND)
			mNumWaves = GetNumWavesPerSurvivalStage();
		else
			mNumWaves = kNumWavesPerMode[(int)mApp->mGameMode];
	}

	// ====================================================================================================
	// ▲ 一些准备工作
	// ====================================================================================================
	ZombiePicker aZombiePicker;
	ZombiePickerInit(&aZombiePicker);
	ZombieType aIntroZombieType = GetIntroducedZombieType();
	TOD_ASSERT(mNumWaves <= MAX_ZOMBIE_WAVES);

	// ====================================================================================================
	// ▲ 遍历每一波并填充每波的出怪列表
	// ====================================================================================================
	for (int aWave = 0; aWave < mNumWaves; aWave++)
	{
		ZombiePickerInitForWave(&aZombiePicker);
		mZombiesInWave[aWave][0] = ZombieType::ZOMBIE_INVALID;

		bool aIsFlagWave = IsFlagWave(aWave);
		bool aIsFinalWave = aWave == mNumWaves - 1;

		if (mApp->IsBungeeBlitzLevel() && aIsFlagWave)
		{
			// 蹦极闪电战关卡的每大波固定刷出 5 只蹦极僵尸
			for (int _i = 0; _i < 5; _i++)
				PutZombieInWave(ZombieType::ZOMBIE_BUNGEE, aWave, &aZombiePicker);

			if (!aIsFinalWave)
				continue;
		}

		// ------------------------------------------------------------------------------------------------
		// △ 计算该波的僵尸总点数
		// ------------------------------------------------------------------------------------------------
		int& aZombiePoints = aZombiePicker.mZombiePoints;
		// 根据关卡计算本波的基础僵尸点数
		if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND)
		{
			aZombiePoints = (mChallenge->mSurvivalStage * GetNumWavesPerSurvivalStage() + aWave + 10) * 2 / 5 + 1;
		}
		else if (mApp->IsSurvivalMode() && mChallenge->mSurvivalStage > 0)
		{
			aZombiePoints = (mChallenge->mSurvivalStage * GetNumWavesPerSurvivalStage() + aWave) * 2 / 5 + 1;
		}
		else if (mApp->IsAdventureMode() && mApp->HasFinishedAdventure() && mLevel != 5)
		{
			aZombiePoints = aWave * 2 / 5 + 1;
		}
		else
		{
			aZombiePoints = aWave / 3 + 1;
		}

		// 旗帜波的特殊调整
		if (aIsFlagWave)
		{
			int aPlainZombiesNum = std::min(aZombiePoints, 8);
			aZombiePoints *= 2.5f;

			if (mApp->mGameMode != GameMode::GAMEMODE_MINIGAME_WAR_AND_PEAS && mApp->mGameMode != GameMode::GAMEMODE_MINIGAME_WAR_AND_PEAS_2)
			{
				for (int _i = 0; _i < aPlainZombiesNum; _i++)
				{
					PutZombieInWave(ZombieType::ZOMBIE_NORMAL, aWave, &aZombiePicker);
				}
				PutZombieInWave(ZombieType::ZOMBIE_FLAG, aWave, &aZombiePicker);
			}
		}

		// 部分关卡的多倍出怪
		// B 形态 -> 常量表（上方 kWavePointsMultPerMode，GameMode 直下标；1 = 原链落空）；
		// 冒险模式的关卡重定向倍率次序敏感（25/5 级 4 倍 → 小 boss 3 倍 → 40 级 3 倍 → 45 级 2 倍），表外保留 if 链
		if (mApp->IsAdventureMode())
		{
			if (mApp->IsLittleTroubleLevel() || mApp->IsWallnutBowlingLevel())
			{
				aZombiePoints *= 4;
			}
			else if (mApp->IsMiniBossLevel())
			{
				aZombiePoints *= 3;
			}
			else if (mApp->IsStormyNightLevel())
			{
				aZombiePoints *= 3;
			}
			else if (mApp->IsBungeeBlitzLevel())
			{
				aZombiePoints *= 2;
			}
		}
		else
		{
			int aWaveMult = kWavePointsMultPerMode[(int)mApp->mGameMode];
			if (aWaveMult != 1)
			{
				aZombiePoints *= aWaveMult;
			}
		}

		// ------------------------------------------------------------------------------------------------
		// △ 向出怪列表中加入固定刷出的僵尸
		// ------------------------------------------------------------------------------------------------
		// 部分新出现的僵尸会在特定波固定刷出
		if (aIntroZombieType != ZombieType::ZOMBIE_INVALID && aIntroZombieType != ZombieType::ZOMBIE_DUCKY_TUBE)
		{
			bool aSpawnIntro = false;
			if ((aIntroZombieType == ZombieType::ZOMBIE_DIGGER || aIntroZombieType == ZombieType::ZOMBIE_BALLOON))
			{
				if (aWave + 1== 7 || aIsFinalWave)
				{
					aSpawnIntro = true;
				}
			}
			else if (aIntroZombieType == ZombieType::ZOMBIE_YETI)
			{
				if (aWave == mNumWaves / 2 && !mApp->mSawYeti)
				{
					aSpawnIntro = true;
				}
			}
			else if (aWave == mNumWaves / 2 || aIsFinalWave)
			{
				aSpawnIntro = true;
			}

			if (aSpawnIntro)
			{
				PutZombieInWave(aIntroZombieType, aWave, &aZombiePicker);
			}
		}

		// 5-10 关卡的最后一波加入一只伽刚特尔
		if (mLevel == 50 && aIsFinalWave)
		{
			PutZombieInWave(ZombieType::ZOMBIE_GARGANTUAR, aWave, &aZombiePicker);
		}
		// 冒险模式关卡的最后一波会出现本关卡可能出现的所有僵尸
		if (mApp->IsAdventureMode() && aIsFinalWave)
		{
			PutInMissingZombies(aWave, &aZombiePicker);
		}
		// 柱子关卡的特殊出怪
		if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_COLUMN)
		{
			// 每大波的第 5 小波，固定出现 10 只扶梯僵尸
			if (aWave % 10 == 5)
			{
				for (int _i = 0; _i < 10; _i++)
				{
					PutZombieInWave(ZombieType::ZOMBIE_LADDER, aWave, &aZombiePicker);
				}
			}

			// 每大波的第 8 小波，固定出现 10 只玩偶匣僵尸
			if (aWave % 10 == 8)
			{
				for (int _i = 0; _i < 10; _i++)
				{
					PutZombieInWave(ZombieType::ZOMBIE_JACK_IN_THE_BOX, aWave, &aZombiePicker);
				}
			}

			// 第 19/29 小波，固定出现 3/5 只伽刚特尔
			if (aWave == 19)
			{
				for (int _i = 0; _i < 3; _i++)
				{
					PutZombieInWave(ZombieType::ZOMBIE_GARGANTUAR, aWave, &aZombiePicker);
				}
			}
			if (aWave == 29)
			{
				for (int _i = 0; _i < 5; _i++)
				{
					PutZombieInWave(ZombieType::ZOMBIE_GARGANTUAR, aWave, &aZombiePicker);
				}
			}
		}

		// ------------------------------------------------------------------------------------------------
		// △ 剩余的僵尸点数用于向列表中补充随机僵尸
		// ------------------------------------------------------------------------------------------------
		while (aZombiePoints > 0 && aZombiePicker.mZombieCount < MAX_ZOMBIES_IN_WAVE)
		{
			ZombieType aZombieType = PickZombieType(aZombiePoints, aWave, &aZombiePicker);
			PutZombieInWave(aZombieType, aWave, &aZombiePicker);
		}
	}
}

//0x40A110
int Board::GetLevelRandSeed()
{
	int aRndSeed = mApp->mPlayerInfo->mId + mBoardRandSeed;
	if (mApp->IsAdventureMode())
	{
		aRndSeed += mApp->mPlayerInfo->mFinishedAdventure * 101 + mLevel;
	}
	else
	{
		aRndSeed += mChallenge->mSurvivalStage * 101 + mApp->mGameMode;
	}
	return aRndSeed;
}

//0x40A160
// GOTY @Patoke: 0x40C9F0
namespace
{
// ============================================================================
// 背景信息合并表（用户方案：ECS 平行表→OOP 结构体，一次 cache line 拿全部）
// 消费点：DrawBackdrop（贴图）、InitLevel（行型）、LoadBackgroundImages（资源组）
// 模式→背景映射（kBackgroundPerMode）为独立关注点，不并入（键是 GameMode 非本表键）
// ============================================================================
struct BackgroundInfo
{
	Image* const* mTopImage;               // DrawBackdrop 直取（运行期装载全局地址）
	PlantRowType mRows[MAX_GRID_SIZE_Y];   // 行型
	ResourceGroupId mMainResource;          // 装载资源组
	ResourceGroupId mOverlayResource;       // 叠加装载组（Invalid = 无）
};
constexpr BackgroundInfo kBackgrounds[] = {
	/* BACKGROUND_1_DAY           */ { &Sexy::IMAGE_BACKGROUND1,    { PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_DIRT }, ResourceGroupId::DelayLoad_Background1, ResourceGroupId::Invalid },
	/* BACKGROUND_2_NIGHT         */ { &Sexy::IMAGE_BACKGROUND2,    { PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_DIRT }, ResourceGroupId::DelayLoad_Background2, ResourceGroupId::Invalid },
	/* BACKGROUND_3_POOL          */ { &Sexy::IMAGE_BACKGROUND3,    { PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_POOL,   PlantRowType::PLANTROW_POOL,   PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL }, ResourceGroupId::DelayLoad_Background3, ResourceGroupId::Invalid },
	/* BACKGROUND_4_FOG           */ { &Sexy::IMAGE_BACKGROUND4,    { PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_POOL,   PlantRowType::PLANTROW_POOL,   PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL }, ResourceGroupId::DelayLoad_Background4, ResourceGroupId::Invalid },
	/* BACKGROUND_5_ROOF          */ { &Sexy::IMAGE_BACKGROUND5,    { PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_DIRT }, ResourceGroupId::DelayLoad_Background5, ResourceGroupId::Invalid },
	/* BACKGROUND_6_BOSS          */ { &Sexy::IMAGE_BACKGROUND6BOSS,{ PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_DIRT }, ResourceGroupId::DelayLoad_Background6, ResourceGroupId::Invalid },
	/* BACKGROUND_MUSHROOM_GARDEN */ { &Sexy::IMAGE_BACKGROUND_MUSHROOMGARDEN, { PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_DIRT }, ResourceGroupId::DelayLoad_MushroomGarden, ResourceGroupId::Invalid },
	/* BACKGROUND_GREENHOUSE      */ { &Sexy::IMAGE_BACKGROUND_GREENHOUSE,     { PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_DIRT }, ResourceGroupId::DelayLoad_GreenHouseGarden, ResourceGroupId::DelayLoad_GreenHouseOverlay },
	/* BACKGROUND_ZOMBIQUARIUM    */ { &Sexy::IMAGE_AQUARIUM1,      { PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_POOL,   PlantRowType::PLANTROW_POOL,   PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL }, ResourceGroupId::DelayLoad_Zombiquarium, ResourceGroupId::DelayLoad_GreenHouseOverlay },
	/* BACKGROUND_TREEOFWISDOM    */ { nullptr,                      { PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_NORMAL, PlantRowType::PLANTROW_DIRT }, ResourceGroupId::Invalid, ResourceGroupId::Invalid },
};
static_assert(sizeof(kBackgrounds) / sizeof(kBackgrounds[0]) == (int)BackgroundType::BACKGROUND_TREEOFWISDOM + 1,
	"背景信息表槽位须与 BackgroundType 枚举尾同步");
}  // namespace

//（已并入 kBackgrounds）


void Board::LoadBackgroundImages()
{
	int aBackground = (int)mBackground;
	if ((unsigned)aBackground >= (unsigned)(sizeof(kBackgrounds) / sizeof(kBackgrounds[0])))
	{
		TOD_ASSERT(false);
		return;
	}
	const auto& aRow = kBackgrounds[aBackground];
	if (aRow.mMainResource != ResourceGroupId::Invalid)
	{
		TodLoadResources(aRow.mMainResource);
		if (aRow.mOverlayResource != ResourceGroupId::Invalid)
		{
			TodLoadResources(aRow.mOverlayResource);
		}
	}
	else
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_TREEOFWISDOM, true);
	}
	if (mBackground == BackgroundType::BACKGROUND_1_DAY &&
		((mApp->IsAdventureMode() && mLevel <= 4) || mApp->mGameMode == GameMode::GAMEMODE_LIMBO_RESODDED))
	{
		TodLoadResources(ResourceGroupId::DelayLoad_BackgroundUnsodded);
	}
}

//0x40A550
namespace
{
// 模式→背景表（B 形态）：73 模式除冒险（关卡阶梯，表外 if 链）外全覆盖，
// 行由脚本从原 switch 落空组解析生成（tools/gen_pickbackground_rows.txt 流程），
// 枚举真值剥块注释后按序下标。kBackgroundInvalid = 表外/非法（等价原 default 断言，
// Release 下 mBackground 保持不变）。
constexpr BackgroundType kBackgroundInvalid = (BackgroundType)-1;
constexpr BackgroundType kBackgroundPerMode[GameMode::NUM_GAME_MODES] = {
	/* [ 0] GAMEMODE_ADVENTURE  (v1 0) */ kBackgroundInvalid,
	/* [ 1] GAMEMODE_MINIGAME_BEGHOULED  (v1 20) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [ 2] GAMEMODE_MINIGAME_BEGHOULED_TWIST  (v1 24) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [ 3] GAMEMODE_MINIGAME_BOBSLED_BONANZA  (v1 28) */ BackgroundType::BACKGROUND_3_POOL,
	/* [ 4] GAMEMODE_MINIGAME_COLUMN  (v1 27) */ BackgroundType::BACKGROUND_5_ROOF,
	/* [ 5] GAMEMODE_MINIGAME_FINAL_BOSS  (v1 35) */ BackgroundType::BACKGROUND_6_BOSS,
	/* [ 6] GAMEMODE_MINIGAME_INVISIGHOUL  (v1 21) */ BackgroundType::BACKGROUND_4_FOG,
	/* [ 7] GAMEMODE_MINIGAME_LAST_STAND  (v1 31) */ BackgroundType::BACKGROUND_3_POOL,
	/* [ 8] GAMEMODE_MINIGAME_LITTLE_TROUBLE  (v1 25) */ BackgroundType::BACKGROUND_3_POOL,
	/* [ 9] GAMEMODE_MINIGAME_POGO_PARTY  (v1 34) */ BackgroundType::BACKGROUND_5_ROOF,
	/* [10] GAMEMODE_MINIGAME_PORTAL_COMBAT  (v1 26) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [11] GAMEMODE_MINIGAME_RAINING_SEEDS  (v1 19) */ BackgroundType::BACKGROUND_4_FOG,
	/* [12] GAMEMODE_MINIGAME_SEEING_STARS  (v1 22) */ BackgroundType::BACKGROUND_1_DAY,
	/* [13] GAMEMODE_MINIGAME_SLOT_MACHINE  (v1 18) */ BackgroundType::BACKGROUND_1_DAY,
	/* [14] GAMEMODE_MINIGAME_SPEED  (v1 29) */ BackgroundType::BACKGROUND_3_POOL,
	/* [15] GAMEMODE_MINIGAME_WALLNUT_BOWLING  (v1 17) */ BackgroundType::BACKGROUND_1_DAY,
	/* [16] GAMEMODE_MINIGAME_WALLNUT_BOWLING_2  (v1 33) */ BackgroundType::BACKGROUND_1_DAY,
	/* [17] GAMEMODE_MINIGAME_WAR_AND_PEAS  (v1 16) */ BackgroundType::BACKGROUND_1_DAY,
	/* [18] GAMEMODE_MINIGAME_WAR_AND_PEAS_2  (v1 32) */ BackgroundType::BACKGROUND_3_POOL,
	/* [19] GAMEMODE_MINIGAME_WHACK_A_ZOMBIE  (v1 30) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [20] GAMEMODE_MINIGAME_ZOMBIQUARIUM  (v1 23) */ BackgroundType::BACKGROUND_ZOMBIQUARIUM,
	/* [21] GAMEMODE_LIMBO_AIR_RAID  (v1 41) */ BackgroundType::BACKGROUND_4_FOG,
	/* [22] GAMEMODE_LIMBO_ART_CHALLENGE_SUNFLOWER  (v1 40) */ BackgroundType::BACKGROUND_1_DAY,
	/* [23] GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT  (v1 36) */ BackgroundType::BACKGROUND_1_DAY,
	/* [24] GAMEMODE_LIMBO_BIG_TIME  (v1 39) */ BackgroundType::BACKGROUND_1_DAY,
	/* [25] GAMEMODE_LIMBO_BUNGEE_BLITZ  (v1 48) */ BackgroundType::BACKGROUND_5_ROOF,
	/* [26] GAMEMODE_LIMBO_GRAVE_DANGER  (v1 45) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [27] GAMEMODE_LIMBO_HIGH_GRAVITY  (v1 44) */ BackgroundType::BACKGROUND_5_ROOF,
	/* [28] GAMEMODE_LIMBO_ICE  (v1 42) */ BackgroundType::BACKGROUND_1_DAY,
	/* [29] GAMEMODE_LIMBO_RESODDED  (v1 38) */ BackgroundType::BACKGROUND_1_DAY,
	/* [30] GAMEMODE_LIMBO_SHOVEL  (v1 46) */ BackgroundType::BACKGROUND_1_DAY,
	/* [31] GAMEMODE_LIMBO_SQUIRREL  (v1 49) */ BackgroundType::BACKGROUND_1_DAY,
	/* [32] GAMEMODE_LIMBO_STORMY_NIGHT  (v1 47) */ BackgroundType::BACKGROUND_4_FOG,
	/* [33] GAMEMODE_LIMBO_SUNNY_DAY  (v1 37) */ BackgroundType::BACKGROUND_1_DAY,
	/* [34] GAMEMODE_LIMBO_ZEN_GARDEN  (v1 43) */ BackgroundType::BACKGROUND_GREENHOUSE,
	/* [35] GAMEMODE_INTRO  (v1 71) */ BackgroundType::BACKGROUND_3_POOL,
	/* [36] GAMEMODE_PUZZLE_I_ZOMBIE_1  (v1 61) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [37] GAMEMODE_PUZZLE_I_ZOMBIE_2  (v1 62) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [38] GAMEMODE_PUZZLE_I_ZOMBIE_3  (v1 63) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [39] GAMEMODE_PUZZLE_I_ZOMBIE_4  (v1 64) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [40] GAMEMODE_PUZZLE_I_ZOMBIE_5  (v1 65) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [41] GAMEMODE_PUZZLE_I_ZOMBIE_6  (v1 66) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [42] GAMEMODE_PUZZLE_I_ZOMBIE_7  (v1 67) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [43] GAMEMODE_PUZZLE_I_ZOMBIE_8  (v1 68) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [44] GAMEMODE_PUZZLE_I_ZOMBIE_9  (v1 69) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [45] GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS  (v1 70) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [46] GAMEMODE_SCARY_POTTER_1  (v1 51) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [47] GAMEMODE_SCARY_POTTER_2  (v1 52) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [48] GAMEMODE_SCARY_POTTER_3  (v1 53) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [49] GAMEMODE_SCARY_POTTER_4  (v1 54) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [50] GAMEMODE_SCARY_POTTER_5  (v1 55) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [51] GAMEMODE_SCARY_POTTER_6  (v1 56) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [52] GAMEMODE_SCARY_POTTER_7  (v1 57) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [53] GAMEMODE_SCARY_POTTER_8  (v1 58) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [54] GAMEMODE_SCARY_POTTER_9  (v1 59) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [55] GAMEMODE_SCARY_POTTER_ENDLESS  (v1 60) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [56] GAMEMODE_SURVIVAL_ENDLESS_STAGE_1  (v1 11) */ BackgroundType::BACKGROUND_1_DAY,
	/* [57] GAMEMODE_SURVIVAL_ENDLESS_STAGE_2  (v1 12) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [58] GAMEMODE_SURVIVAL_ENDLESS_STAGE_3  (v1 13) */ BackgroundType::BACKGROUND_3_POOL,
	/* [59] GAMEMODE_SURVIVAL_ENDLESS_STAGE_4  (v1 14) */ BackgroundType::BACKGROUND_4_FOG,
	/* [60] GAMEMODE_SURVIVAL_ENDLESS_STAGE_5  (v1 15) */ BackgroundType::BACKGROUND_5_ROOF,
	/* [61] GAMEMODE_SURVIVAL_HARD_STAGE_1  (v1 6) */ BackgroundType::BACKGROUND_1_DAY,
	/* [62] GAMEMODE_SURVIVAL_HARD_STAGE_2  (v1 7) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [63] GAMEMODE_SURVIVAL_HARD_STAGE_3  (v1 8) */ BackgroundType::BACKGROUND_3_POOL,
	/* [64] GAMEMODE_SURVIVAL_HARD_STAGE_4  (v1 9) */ BackgroundType::BACKGROUND_4_FOG,
	/* [65] GAMEMODE_SURVIVAL_HARD_STAGE_5  (v1 10) */ BackgroundType::BACKGROUND_5_ROOF,
	/* [66] GAMEMODE_SURVIVAL_NORMAL_STAGE_1  (v1 1) */ BackgroundType::BACKGROUND_1_DAY,
	/* [67] GAMEMODE_SURVIVAL_NORMAL_STAGE_2  (v1 2) */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [68] GAMEMODE_SURVIVAL_NORMAL_STAGE_3  (v1 3) */ BackgroundType::BACKGROUND_3_POOL,
	/* [69] GAMEMODE_SURVIVAL_NORMAL_STAGE_4  (v1 4) */ BackgroundType::BACKGROUND_4_FOG,
	/* [70] GAMEMODE_SURVIVAL_NORMAL_STAGE_5  (v1 5) */ BackgroundType::BACKGROUND_5_ROOF,
	/* [71] GAMEMODE_TREE_OF_WISDOM  (v1 50) */ BackgroundType::BACKGROUND_TREEOFWISDOM,
	/* [72] GAMEMODE_CUSTOM_LEVEL（哨兵；策略层覆写）*/ BackgroundType::BACKGROUND_1_DAY,
};
static_assert(sizeof(kBackgroundPerMode) / sizeof(kBackgroundPerMode[0]) == (int)GameMode::NUM_GAME_MODES,
	"模式→背景槽位须与 GameMode 枚举尾同步");

// 冒险关卡→背景表（B 形态）：原区间支（<=1/2/3/4*LEVELS_PER_AREA、<FINAL_LEVEL、==FINAL_LEVEL）
// 逐槽展开为 mLevel-1 直下标；35 槽 = IsScaryPotterLevel 冒险 35 级重定向（夜）；越界（<=0 / >50）表外 = 原末支 else。
constexpr BackgroundType kBackgroundPerLevel[FINAL_LEVEL] = {
	/* [1..10]  1 区白天                                                       */ BackgroundType::BACKGROUND_1_DAY, BackgroundType::BACKGROUND_1_DAY, BackgroundType::BACKGROUND_1_DAY, BackgroundType::BACKGROUND_1_DAY, BackgroundType::BACKGROUND_1_DAY, BackgroundType::BACKGROUND_1_DAY, BackgroundType::BACKGROUND_1_DAY, BackgroundType::BACKGROUND_1_DAY, BackgroundType::BACKGROUND_1_DAY, BackgroundType::BACKGROUND_1_DAY,
	/* [11..20] 2 区夜晚                                                       */ BackgroundType::BACKGROUND_2_NIGHT, BackgroundType::BACKGROUND_2_NIGHT, BackgroundType::BACKGROUND_2_NIGHT, BackgroundType::BACKGROUND_2_NIGHT, BackgroundType::BACKGROUND_2_NIGHT, BackgroundType::BACKGROUND_2_NIGHT, BackgroundType::BACKGROUND_2_NIGHT, BackgroundType::BACKGROUND_2_NIGHT, BackgroundType::BACKGROUND_2_NIGHT, BackgroundType::BACKGROUND_2_NIGHT,
	/* [21..30] 3 区泳池                                                       */ BackgroundType::BACKGROUND_3_POOL, BackgroundType::BACKGROUND_3_POOL, BackgroundType::BACKGROUND_3_POOL, BackgroundType::BACKGROUND_3_POOL, BackgroundType::BACKGROUND_3_POOL, BackgroundType::BACKGROUND_3_POOL, BackgroundType::BACKGROUND_3_POOL, BackgroundType::BACKGROUND_3_POOL, BackgroundType::BACKGROUND_3_POOL, BackgroundType::BACKGROUND_3_POOL,
	/* [31..34] 4 区迷雾                                                       */ BackgroundType::BACKGROUND_4_FOG, BackgroundType::BACKGROUND_4_FOG, BackgroundType::BACKGROUND_4_FOG, BackgroundType::BACKGROUND_4_FOG,
	/* [35]     35 级砸罐子重定向（原 IsScaryPotterLevel 分支先于 <=4 区判定）  */ BackgroundType::BACKGROUND_2_NIGHT,
	/* [36..40] 4 区迷雾（续）                                                  */ BackgroundType::BACKGROUND_4_FOG, BackgroundType::BACKGROUND_4_FOG, BackgroundType::BACKGROUND_4_FOG, BackgroundType::BACKGROUND_4_FOG, BackgroundType::BACKGROUND_4_FOG,
	/* [41..49] 5 区屋顶                                                       */ BackgroundType::BACKGROUND_5_ROOF, BackgroundType::BACKGROUND_5_ROOF, BackgroundType::BACKGROUND_5_ROOF, BackgroundType::BACKGROUND_5_ROOF, BackgroundType::BACKGROUND_5_ROOF, BackgroundType::BACKGROUND_5_ROOF, BackgroundType::BACKGROUND_5_ROOF, BackgroundType::BACKGROUND_5_ROOF, BackgroundType::BACKGROUND_5_ROOF,
	/* [50]     最终关                                                        */ BackgroundType::BACKGROUND_6_BOSS,
};
static_assert(sizeof(kBackgroundPerLevel) / sizeof(kBackgroundPerLevel[0]) == FINAL_LEVEL,
	"冒险关卡→背景槽位须与 FINAL_LEVEL 同步");
}  // namespace

namespace
{
	// 夜晚关墓石布局表（B 形态）：每行 = 一种布局对 3..8 六列的墓石数（0 = 该列不放）。
	// 行序即原链支序：0 = 生存普通支、1 = 非冒险支、2..5 = 冒险 11..13 / 14,16 / 17..19 / >=20 支。
	// 调用序保持列号升序、跳过 0 列，与原逐句 AddGraveStones 调用序一致（RNG 消耗序不变）。
	constexpr int kNightGravesPerColumn[6][6] = {
		/* [0] 生存普通（5,1 6,1 7,1 8,2）                                    */ { 0, 0, 1, 1, 1, 2 },
		/* [1] 非冒险（4,1 5,1 6,2 7,2 8,3）                                  */ { 0, 1, 1, 2, 2, 3 },
		/* [2] 冒险 11..13（6,1 7,1 8,2）                                     */ { 0, 0, 0, 1, 1, 2 },
		/* [3] 冒险 14,16（5,1 6,1 7,2 8,3）                                  */ { 0, 0, 1, 1, 2, 3 },
		/* [4] 冒险 17..19（4,1 5,2 6,2 7,3 8,3）                             */ { 0, 1, 2, 2, 3, 3 },
		/* [5] 冒险 >=20（3,1 4,2 5,2 6,2 7,3 8,3）                           */ { 1, 2, 2, 2, 3, 3 },
	};

	// 冒险夜晚关→墓石布局表（B 形态）：mLevel-1 直下标（1..50）；kNightGravePatternNone =
	// 原链落空 else TOD_ASSERT(false)（1..10 与 15 槽：15 关打地鼠在外层 IsWhackAZombieLevel
	// 支已被截走，仍按原链字面语义保留落空断言；>=20 支按字面语义展开为 20..50 全 P5）。
	constexpr int kNightGravePatternNone = -1;
	constexpr int kNightGravePatternPerLevel[FINAL_LEVEL] = {
		/* [1..10]  白天区（原链落空）                                          */
		kNightGravePatternNone, kNightGravePatternNone, kNightGravePatternNone, kNightGravePatternNone, kNightGravePatternNone,
		kNightGravePatternNone, kNightGravePatternNone, kNightGravePatternNone, kNightGravePatternNone, kNightGravePatternNone,
		/* [11..13] 夜晚区首段（原 11||12||13 支）                              */ 2, 2, 2,
		/* [14]     原 14||16 支                                                */ 3,
		/* [15]     打地鼠关（外层支截走，原链字面落空）                          */ kNightGravePatternNone,
		/* [16]     原 14||16 支                                                */ 3,
		/* [17..19] 夜晚区尾段（原 17||18||19 支）                              */ 4, 4, 4,
		/* [20..50] 原链 mLevel>=20 末支（字面语义：仅 31..40 夜背景可达）       */
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
	};
	static_assert(sizeof(kNightGravePatternPerLevel) / sizeof(kNightGravePatternPerLevel[0]) == FINAL_LEVEL,
		"冒险夜晚关→墓石布局表槽位须与 FINAL_LEVEL 同步");
	static_assert(kNightGravePatternPerLevel[11 - 1] == 2 && kNightGravePatternPerLevel[16 - 1] == 3 &&
		kNightGravePatternPerLevel[17 - 1] == 4 && kNightGravePatternPerLevel[20 - 1] == 5 &&
		kNightGravePatternPerLevel[40 - 1] == 5 && kNightGravePatternPerLevel[15 - 1] == kNightGravePatternNone,
		"冒险夜晚关→墓石布局表关键槽锚（11/16/17/20/40 布局与 15 落空）须与原链一致");
}  // namespace

void Board::PickBackground()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_ADVENTURE)
	{
		// 冒险模式按关卡阶梯选背景（B 形态 -> 常量表 上方 kBackgroundPerLevel，mLevel-1 直下标；
		// 区间支已逐槽展开，35 槽为砸罐子重定向特例；越界走表外 else = 原末支）
		if (mLevel >= 1 && mLevel <= FINAL_LEVEL)
		{
			mBackground = kBackgroundPerLevel[mLevel - 1];
		}
		else
		{
			mBackground = BackgroundType::BACKGROUND_1_DAY;
		}
	}
	else
	{
		int aMode = (int)mApp->mGameMode;
		if ((unsigned)aMode < (unsigned)(sizeof(kBackgroundPerMode) / sizeof(kBackgroundPerMode[0])) &&
			kBackgroundPerMode[aMode] != kBackgroundInvalid)
		{
			mBackground = kBackgroundPerMode[aMode];
		}
		else
		{
			TOD_ASSERT(false);
		}
	}
	LoadBackgroundImages();

	// 背景行型表：背景 → 六行种植行类型（原四组 if 链落为直下标整行拷贝）。
	// 蘑菇园背景无模式映射（原版 default 断言），守卫保持语义；首次冒险铺草皮
	// 与重铺草皮的行覆写为异构条件，表外保留（仅作用于第一组行型）。
	//（已并入 kBackgrounds）

	if (mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN)
	{
		TOD_ASSERT(false);  // 原版 default：无模式映射到蘑菇园背景
	}
	else
	{
		for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
		{
			mPlantRow[y] = kBackgrounds[(int)mBackground].mRows[y];
		}
	}

	if (mBackground == BackgroundType::BACKGROUND_1_DAY || mBackground == BackgroundType::BACKGROUND_GREENHOUSE || mBackground == BackgroundType::BACKGROUND_TREEOFWISDOM)
	{
		if (mApp->IsAdventureMode() && mApp->IsFirstTimeAdventureMode())
		{
			if (mLevel == 1)
			{
				mPlantRow[0] = PlantRowType::PLANTROW_DIRT;
				mPlantRow[1] = PlantRowType::PLANTROW_DIRT;
				mPlantRow[3] = PlantRowType::PLANTROW_DIRT;
				mPlantRow[4] = PlantRowType::PLANTROW_DIRT;
			}
			else if (mLevel == 2 || mLevel == 3)
			{
				mPlantRow[0] = PlantRowType::PLANTROW_DIRT;
				mPlantRow[4] = PlantRowType::PLANTROW_DIRT;
			}
		}
		else if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_RESODDED)
		{
			mPlantRow[0] = PlantRowType::PLANTROW_DIRT;
			mPlantRow[4] = PlantRowType::PLANTROW_DIRT;
		}
	}

	for (int x = 0; x < MAX_GRID_SIZE_X; x++)
	{
		for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
		{
			if (mPlantRow[y] == PlantRowType::PLANTROW_DIRT)
			{
				mGridSquareType[x][y] = GridSquareType::GRIDSQUARE_DIRT;
			}
			else if (mPlantRow[y] == PlantRowType::PLANTROW_POOL && x >= 0 && x <= 8)
			{
				mGridSquareType[x][y] = GridSquareType::GRIDSQUARE_POOL;
			}
			else if (mPlantRow[y] == PlantRowType::PLANTROW_HIGH_GROUND && x >= 4 && x <= 8)
			{
				mGridSquareType[x][y] = GridSquareType::GRIDSQUARE_HIGH_GROUND;
			}
		}
	}

	MTRand aLevelRNG(GetLevelRandSeed());
	if (StageHasGraveStones())
	{
		if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_GRAVE_DANGER)
		{
			AddGraveStones(6, RandRangeInt(1, 2), aLevelRNG);
			AddGraveStones(7, RandRangeInt(1, 3), aLevelRNG);
			AddGraveStones(8, RandRangeInt(2, 3), aLevelRNG);
		}
		else if (mApp->IsWhackAZombieLevel())
		{
			mChallenge->WhackAZombiePlaceGraves(9);
		}
		else if (mBackground == BackgroundType::BACKGROUND_2_NIGHT)
		{
			// B 形态 -> 常量表（上方 kNightGravesPerColumn / kNightGravePatternPerLevel）：
			// 前 2 支模式谓词（IsSurvivalNormal / 非冒险）判别式异源，守卫表外前置定行号；
			// 冒险支按 mLevel-1 直下标，kNightGravePatternNone = 原链落空 else TOD_ASSERT(false)。
			// 列循环升序跳 0，AddGraveStones 调用序与原逐句一致（RNG 消耗序不变）。
			int aGravePattern = kNightGravePatternNone;
			if (mApp->IsSurvivalNormal(mApp->mGameMode))
			{
				aGravePattern = 0;
			}
			else if (!mApp->IsAdventureMode())
			{
				aGravePattern = 1;
			}
			else
			{
				int aLevelIndex = mLevel - 1;
				if ((unsigned)aLevelIndex < (unsigned)(sizeof(kNightGravePatternPerLevel) / sizeof(kNightGravePatternPerLevel[0])))
				{
					aGravePattern = kNightGravePatternPerLevel[aLevelIndex];
				}
				TOD_ASSERT(aGravePattern != kNightGravePatternNone);
			}
			if (aGravePattern != kNightGravePatternNone)
			{
				for (int aColumn = 3; aColumn <= 8; aColumn++)
				{
					int aCount = kNightGravesPerColumn[aGravePattern][aColumn - 3];
					if (aCount > 0)
					{
						AddGraveStones(aColumn, aCount, aLevelRNG);
					}
				}
			}
		}
	}
	PickSpecialGraveStone();
}

//0x40AB10
void Board::InitZombieWavesForLevel(int theForLevel)
{
	if (mApp->IsWhackAZombieLevel() || (mApp->IsWallnutBowlingLevel() && !mApp->IsFirstTimeAdventureMode()))
	{
		mChallenge->InitZombieWaves();
		return;
	}

	for (int aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType++)
	{
		mZombieAllowed[aZombieType] = CanZombieSpawnOnLevel((ZombieType)aZombieType, theForLevel);
	}

	// 进场预载：本关允许的僵尸类型在此（加载屏时段）完成定义装载与 atlas 构建，
	// 消除首例生成时主线程同步"读记录+合成图集"的进场掉帧（credits 入口同款手法）。
	for (int aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType++)
	{
		if (mZombieAllowed[aZombieType])
			ReanimationPreload(GetZombieDefinition((ZombieType)aZombieType).mReanimationType);
	}
}

bool Board::IsZombieWaveDistributionOk()
{
	if (!mApp->IsAdventureMode())
		return true;

	int aZombieTypeCount[(int)ZombieType::NUM_ZOMBIE_TYPES] = { 0 };
	for (int aWave = 0; aWave < mNumWaves; aWave++)
	{
		for (int aIndex = 0; aIndex < MAX_ZOMBIES_IN_WAVE; aIndex++)
		{
			ZombieType aZombieType = mZombiesInWave[aWave][aIndex];
			if (aZombieType == ZombieType::ZOMBIE_INVALID)
			{
				break;
			}

			TOD_ASSERT(aZombieType >= 0 && aZombieType < ZombieType::NUM_ZOMBIE_TYPES);
			aZombieTypeCount[(int)aZombieType]++;
		}
	}

	for (ZombieType aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType = (ZombieType)((int)aZombieType + 1))
	{
		if (aZombieType != ZombieType::ZOMBIE_YETI && CanZombieSpawnOnLevel(aZombieType, mLevel) && aZombieTypeCount[(int)aZombieType] == 0)
		{
			TodTraceAndLog("Didn't spawn required zombie %s, level %d", EntityZombieName(aZombieType).c_str(), mLevel);
			return false;
		}
	}
	return true;
}

//0x40ABB0
void Board::InitZombieWaves()
{
	memset(mZombieAllowed, false, sizeof(mZombieAllowed));
	if (mApp->IsAdventureMode())
	{
		InitZombieWavesForLevel(mLevel);
	}
	else
	{
		mChallenge->InitZombieWaves();
	}
	PickZombieWaves();
	TOD_ASSERT(IsZombieWaveDistributionOk());

	mCurrentWave = 0;
	mTotalSpawnedWaves = 0;
	mApp->mSawYeti = false;
	if (mApp->IsFirstTimeAdventureMode() && mLevel == 2)
	{
		mZombieCountDown = ZOMBIE_COUNTDOWN * 2;
	}
	else if (mApp->IsSurvivalMode() && mChallenge->mSurvivalStage > 0)
	{
		mZombieCountDown = ZOMBIE_COUNTDOWN_RANGE;
	}
	else
	{
		mZombieCountDown = ZOMBIE_COUNTDOWN_FIRST_WAVE;
	}

	mZombieHealthWaveStart = 0;
	mLastBungeeWave = 0;
	mProgressMeterWidth = 0;
	mHugeWaveCountDown = 0;
	mLevelAwardSpawned = false;
	mZombieCountDownStart = mZombieCountDown;
	mZombieHealthToNextWave = -1;
}

//0x40ACB0
void Board::FreezeEffectsForCutscene(bool theFreeze)
{
	TodParticleSystem* aParticle = nullptr;
	while (IterateParticles(aParticle))
	{
		if (aParticle->mEffectType == ParticleEffect::PARTICLE_GRAVE_BUSTER)
		{
			aParticle->mDontUpdate = theFreeze;
		}
		else if (aParticle->mEffectType == ParticleEffect::PARTICLE_POOL_SPARKLY && mIceTrapCounter == 0)
		{
            aParticle->mDontUpdate = theFreeze &&
                !(mPoolIntroVisual && mApp->mGameScene == SCENE_LEVEL_INTRO && !mCutScene->IsSurvivalRepick());
		}
	}

	Reanimation* aReanim = nullptr;
	while (IterateReanimations(aReanim))
	{
		if (aReanim->mReanimationType == ReanimationType::REANIM_SLEEPING)
		{
			aReanim->ApplyRate(theFreeze ? 0.0f : RandRangeFloat(6, 8));
		}
	}
}

//0x40AD60
void Board::InitSurvivalStage()
{
	RefreshSeedPacketFromCursor();
	mApp->mSoundSystem->GamePause(true);
	FreezeEffectsForCutscene(true);
	mLevelComplete = false;
	InitZombieWaves();
	mApp->mGameScene = GameScenes::SCENE_LEVEL_INTRO;
	mApp->ShowSeedChooserScreen();
	mCutScene->StartLevelIntro();
	mSeedBank->UpdateWidth();

	for (int i = 0; i < SEEDBANK_MAX; i++)
	{
		SeedPacket* aPacket = &mSeedBank->mSeedPackets[i];
		aPacket->mX = GetSeedPacketPositionX(i);
		aPacket->mPacketType = SeedType::SEED_NONE;
	}

	if (StageHasFog())
	{
		mFogBlownCountDown = FOG_BLOW_RETURN_TIME;
	}
	for (int j = 0; j < MAX_GRID_SIZE_Y; j++)
	{
		mWaveRowGotLawnMowered[j] = -100;
	}
}

//0x40AE70
Rect Board::GetShovelButtonRect()
{
    if (mLandscapeHud) return LandscapeHud::ShovelBounds(*this);
	// GOTY 1073 (0040D720): the shovel/tool bank is 70 x 72, not the 87-high seed bank.
	Rect aRect(GetSeedBankExtraWidth() + 456, 0, Sexy::IMAGE_SHOVELBANK->mWidth, Sexy::IMAGE_SHOVELBANK->mHeight);
	if (mApp->IsSlotMachineLevel() || mApp->IsSquirrelLevel())
	{
		aRect.mX = 600;
	}
	aRect.mX += mApp->mScreenBounds.mX-mX;
    aRect.mY-=mY;
	return aRect;
}

//0x40AF00
// GOTY 1073: 0x40D7B0.
void Board::GetZenButtonRect(GameObjectType theObjectType, Rect& theRect)
{
	// 此函数与内测版的差异在于，内测版在此函数中通过下列语句先取得了铲子按钮矩形：
	// Rect aRect = GetShovelButtonRect();
	// 而原版需要在函数调用前先自行取得铲子按钮矩形，并将该矩形作为参数传递给此函数，
	// 原版中此函数有将 theRect 的引用作为返回值，但并无直接使用返回值的情况。
	// 此处为了防止误用返回值而出现问题，故删除其返回值，如需调用可按照如下方式：
	// Rect aButtonRect = GetShovelButtonRect();
	// GetZenButtonRect(xxx, aButtonRect);

	const auto frame = GardenHudLayout::ViewportInBoard(
		mApp->mScreenBounds.mX, mApp->mScreenBounds.mWidth, mX, mY);
	theRect.mY = frame.top;
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN &&
		mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_ZEN_FADING)
		theRect.mY += TodAnimateCurveNs(500'000'000, 0, mChallenge->mChallengeStateCounter.mNanoseconds, 0, -72, Curves::EaseInOut);
	if (theObjectType == GameObjectType::OBJECT_TYPE_NEXT_GARDEN)
	{
		theRect.mX = frame.left + 564;
		return;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		// Only tree food occupies x30 in the original left-anchored toolbar.
		// NEXT_GARDEN is handled above for both drawing and input.
		if (theObjectType == GameObjectType::OBJECT_TYPE_TREE_FOOD)
		{
			theRect.mX = frame.left + 30;
		}
		return;
	}

	theRect.mX = frame.left + 30;
	bool usable = true;
	for (int anObject = GameObjectType::OBJECT_TYPE_WATERING_CAN; anObject <= GameObjectType::OBJECT_TYPE_NEXT_GARDEN; anObject++)
	{
		if (anObject != GameObjectType::OBJECT_TYPE_TREE_FOOD && !CanUseGameObject((GameObjectType)anObject))
		{
			usable = false;
			break;
		}
	}
	if (usable)
	{
		theRect.mX = frame.left;
	}

	for (int anObject = GameObjectType::OBJECT_TYPE_WATERING_CAN; anObject < theObjectType; anObject++)
	{
		if (CanUseGameObject((GameObjectType)anObject))
		{
			theRect.mX += 70;
		}
	}
}

namespace
{
// 各模式固定卡牌表（B 形态；GameMode 直下标，{} = 该模式无固定卡牌 = 原链落空）。
// mCount 同时为原分支 TOD_ASSERT 的期望值；mAssert=false 仅 ZOMBIQUARIUM（原分支无断言）。
// 表外保留：冒险 35 级（IsScaryPotterLevel）/15 级（IsWhackAZombieLevel）重定向、
// Whack 第 3 槽冒险=樱桃炸弹特例、ChooseSeeds/传送带兜底分支（见调用点）。
struct InitLevelSeedRow
{
    const SeedType* mSeeds;
    int mCount;
    bool mAssert;
};
constexpr SeedType kInitLevelSeeds[] = {
	// [18] GAMEMODE_MINIGAME_SLOT_MACHINE
	SEED_SUNFLOWER, SEED_PEASHOOTER, SEED_SNOWPEA,
	// [23] GAMEMODE_MINIGAME_ZOMBIQUARIUM（1073 InitLevel installs the purchase/trophy
	// packets, not ordinary seeds；原分支无 TOD_ASSERT）
	SEED_ZOMBIQUARIUM_SNORKLE, SEED_ZOMBIQUARIUM_TROPHY,
	// [30] GAMEMODE_MINIGAME_WHACK_A_ZOMBIE（第 3 槽表外按冒险修正为樱桃炸弹）
	SEED_POTATOMINE, SEED_GRAVEBUSTER, SEED_ICESHROOM,
	// [42] GAMEMODE_LIMBO_ICE
	SEED_PEASHOOTER, SEED_CHERRYBOMB, SEED_WALLNUT, SEED_REPEATER, SEED_SNOWPEA, SEED_CHOMPER,
	// [51..60] GAMEMODE_SCARY_POTTER_1..ENDLESS（共体分支，同值填多槽）
	SEED_CHERRYBOMB,
	// [61] GAMEMODE_PUZZLE_I_ZOMBIE_1
	SEED_ZOMBIE_NORMAL, SEED_ZOMBIE_PAIL, SEED_ZOMBIE_FOOTBALL,
	// [62] GAMEMODE_PUZZLE_I_ZOMBIE_2
	SEED_ZOMBIE_NORMAL, SEED_ZOMBIE_SCREEN_DOOR, SEED_ZOMBIE_PAIL,
	// [63] GAMEMODE_PUZZLE_I_ZOMBIE_3
	SEED_ZOMBIE_NORMAL, SEED_ZOMBIE_PAIL, SEED_ZOMBIE_DIGGER,
	// [64] GAMEMODE_PUZZLE_I_ZOMBIE_4
	SEED_ZOMBIE_NORMAL, SEED_ZOMBIE_PAIL, SEED_ZOMBIE_LADDER,
	// [65] GAMEMODE_PUZZLE_I_ZOMBIE_5
	SEED_ZOMBIE_NORMAL, SEED_ZOMBIE_PAIL, SEED_ZOMBIE_BUNGEE, SEED_ZOMBIE_BALLOON,
	// [66] GAMEMODE_PUZZLE_I_ZOMBIE_6
	SEED_ZOMBIE_NORMAL, SEED_ZOMBIE_POLEVAULTER, SEED_ZOMBIE_PAIL, SEED_ZOMBIE_GARGANTUAR,
	// [67] GAMEMODE_PUZZLE_I_ZOMBIE_7
	SEED_ZOMBIE_NORMAL, SEED_ZOMBIE_POLEVAULTER, SEED_ZOMBIE_PAIL, SEED_ZOMBIE_DANCER,
	// [68] GAMEMODE_PUZZLE_I_ZOMBIE_8
	SEED_ZOMBIE_IMP, SEED_ZOMBIE_TRAFFIC_CONE, SEED_ZOMBIE_PAIL, SEED_ZOMBIE_BUNGEE, SEED_ZOMBIE_DIGGER, SEED_ZOMBIE_LADDER,
	// [69] GAMEMODE_PUZZLE_I_ZOMBIE_9
	SEED_ZOMBIE_IMP, SEED_ZOMBIE_TRAFFIC_CONE, SEED_ZOMBIE_POLEVAULTER, SEED_ZOMBIE_PAIL, SEED_ZOMBIE_BUNGEE, SEED_ZOMBIE_DIGGER, SEED_ZOMBIE_LADDER, SEED_ZOMBIE_FOOTBALL,
	// [70] GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS
	SEED_ZOMBIE_IMP, SEED_ZOMBIE_TRAFFIC_CONE, SEED_ZOMBIE_POLEVAULTER, SEED_ZOMBIE_PAIL, SEED_ZOMBIE_BUNGEE, SEED_ZOMBIE_DIGGER, SEED_ZOMBIE_LADDER, SEED_ZOMBIE_FOOTBALL, SEED_ZOMBIE_DANCER,
};
constexpr InitLevelSeedRow kInitLevelSeedRows[GameMode::NUM_GAME_MODES] = {
	/* [ 0] GAMEMODE_ADVENTURE  (v1 0) */ {},
	/* [ 1] GAMEMODE_MINIGAME_BEGHOULED  (v1 20) */ {},
	/* [ 2] GAMEMODE_MINIGAME_BEGHOULED_TWIST  (v1 24) */ {},
	/* [ 3] GAMEMODE_MINIGAME_BOBSLED_BONANZA  (v1 28) */ {},
	/* [ 4] GAMEMODE_MINIGAME_COLUMN  (v1 27) */ {},
	/* [ 5] GAMEMODE_MINIGAME_FINAL_BOSS  (v1 35) */ {},
	/* [ 6] GAMEMODE_MINIGAME_INVISIGHOUL  (v1 21) */ {},
	/* [ 7] GAMEMODE_MINIGAME_LAST_STAND  (v1 31) */ {},
	/* [ 8] GAMEMODE_MINIGAME_LITTLE_TROUBLE  (v1 25) */ {},
	/* [ 9] GAMEMODE_MINIGAME_POGO_PARTY  (v1 34) */ {},
	/* [10] GAMEMODE_MINIGAME_PORTAL_COMBAT  (v1 26) */ {},
	/* [11] GAMEMODE_MINIGAME_RAINING_SEEDS  (v1 19) */ {},
	/* [12] GAMEMODE_MINIGAME_SEEING_STARS  (v1 22) */ {},
	/* [13] GAMEMODE_MINIGAME_SLOT_MACHINE  (v1 18) */ { &kInitLevelSeeds[0], 3, true },
	/* [14] GAMEMODE_MINIGAME_SPEED  (v1 29) */ {},
	/* [15] GAMEMODE_MINIGAME_WALLNUT_BOWLING  (v1 17) */ {},
	/* [16] GAMEMODE_MINIGAME_WALLNUT_BOWLING_2  (v1 33) */ {},
	/* [17] GAMEMODE_MINIGAME_WAR_AND_PEAS  (v1 16) */ {},
	/* [18] GAMEMODE_MINIGAME_WAR_AND_PEAS_2  (v1 32) */ {},
	/* [19] GAMEMODE_MINIGAME_WHACK_A_ZOMBIE  (v1 30) */ { &kInitLevelSeeds[5], 3, true },
	/* [20] GAMEMODE_MINIGAME_ZOMBIQUARIUM  (v1 23) */ { &kInitLevelSeeds[3], 2, false },
	/* [21] GAMEMODE_LIMBO_AIR_RAID  (v1 41) */ {},
	/* [22] GAMEMODE_LIMBO_ART_CHALLENGE_SUNFLOWER  (v1 40) */ {},
	/* [23] GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT  (v1 36) */ {},
	/* [24] GAMEMODE_LIMBO_BIG_TIME  (v1 39) */ {},
	/* [25] GAMEMODE_LIMBO_BUNGEE_BLITZ  (v1 48) */ {},
	/* [26] GAMEMODE_LIMBO_GRAVE_DANGER  (v1 45) */ {},
	/* [27] GAMEMODE_LIMBO_HIGH_GRAVITY  (v1 44) */ {},
	/* [28] GAMEMODE_LIMBO_ICE  (v1 42) */ { &kInitLevelSeeds[8], 6, true },
	/* [29] GAMEMODE_LIMBO_RESODDED  (v1 38) */ {},
	/* [30] GAMEMODE_LIMBO_SHOVEL  (v1 46) */ {},
	/* [31] GAMEMODE_LIMBO_SQUIRREL  (v1 49) */ {},
	/* [32] GAMEMODE_LIMBO_STORMY_NIGHT  (v1 47) */ {},
	/* [33] GAMEMODE_LIMBO_SUNNY_DAY  (v1 37) */ {},
	/* [34] GAMEMODE_LIMBO_ZEN_GARDEN  (v1 43) */ {},
	/* [35] GAMEMODE_INTRO  (v1 71) */ {},
	/* [36] GAMEMODE_PUZZLE_I_ZOMBIE_1  (v1 61) */ { &kInitLevelSeeds[15], 3, true },
	/* [37] GAMEMODE_PUZZLE_I_ZOMBIE_2  (v1 62) */ { &kInitLevelSeeds[18], 3, true },
	/* [38] GAMEMODE_PUZZLE_I_ZOMBIE_3  (v1 63) */ { &kInitLevelSeeds[21], 3, true },
	/* [39] GAMEMODE_PUZZLE_I_ZOMBIE_4  (v1 64) */ { &kInitLevelSeeds[24], 3, true },
	/* [40] GAMEMODE_PUZZLE_I_ZOMBIE_5  (v1 65) */ { &kInitLevelSeeds[27], 4, true },
	/* [41] GAMEMODE_PUZZLE_I_ZOMBIE_6  (v1 66) */ { &kInitLevelSeeds[31], 4, true },
	/* [42] GAMEMODE_PUZZLE_I_ZOMBIE_7  (v1 67) */ { &kInitLevelSeeds[35], 4, true },
	/* [43] GAMEMODE_PUZZLE_I_ZOMBIE_8  (v1 68) */ { &kInitLevelSeeds[39], 6, true },
	/* [44] GAMEMODE_PUZZLE_I_ZOMBIE_9  (v1 69) */ { &kInitLevelSeeds[45], 8, true },
	/* [45] GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS  (v1 70) */ { &kInitLevelSeeds[53], 9, true },
	/* [46] GAMEMODE_SCARY_POTTER_1  (v1 51) */ { &kInitLevelSeeds[14], 1, true },
	/* [47] GAMEMODE_SCARY_POTTER_2  (v1 52) */ { &kInitLevelSeeds[14], 1, true },
	/* [48] GAMEMODE_SCARY_POTTER_3  (v1 53) */ { &kInitLevelSeeds[14], 1, true },
	/* [49] GAMEMODE_SCARY_POTTER_4  (v1 54) */ { &kInitLevelSeeds[14], 1, true },
	/* [50] GAMEMODE_SCARY_POTTER_5  (v1 55) */ { &kInitLevelSeeds[14], 1, true },
	/* [51] GAMEMODE_SCARY_POTTER_6  (v1 56) */ { &kInitLevelSeeds[14], 1, true },
	/* [52] GAMEMODE_SCARY_POTTER_7  (v1 57) */ { &kInitLevelSeeds[14], 1, true },
	/* [53] GAMEMODE_SCARY_POTTER_8  (v1 58) */ { &kInitLevelSeeds[14], 1, true },
	/* [54] GAMEMODE_SCARY_POTTER_9  (v1 59) */ { &kInitLevelSeeds[14], 1, true },
	/* [55] GAMEMODE_SCARY_POTTER_ENDLESS  (v1 60) */ { &kInitLevelSeeds[14], 1, true },
	/* [56] GAMEMODE_SURVIVAL_ENDLESS_STAGE_1  (v1 11) */ {},
	/* [57] GAMEMODE_SURVIVAL_ENDLESS_STAGE_2  (v1 12) */ {},
	/* [58] GAMEMODE_SURVIVAL_ENDLESS_STAGE_3  (v1 13) */ {},
	/* [59] GAMEMODE_SURVIVAL_ENDLESS_STAGE_4  (v1 14) */ {},
	/* [60] GAMEMODE_SURVIVAL_ENDLESS_STAGE_5  (v1 15) */ {},
	/* [61] GAMEMODE_SURVIVAL_HARD_STAGE_1  (v1 6) */ {},
	/* [62] GAMEMODE_SURVIVAL_HARD_STAGE_2  (v1 7) */ {},
	/* [63] GAMEMODE_SURVIVAL_HARD_STAGE_3  (v1 8) */ {},
	/* [64] GAMEMODE_SURVIVAL_HARD_STAGE_4  (v1 9) */ {},
	/* [65] GAMEMODE_SURVIVAL_HARD_STAGE_5  (v1 10) */ {},
	/* [66] GAMEMODE_SURVIVAL_NORMAL_STAGE_1  (v1 1) */ {},
	/* [67] GAMEMODE_SURVIVAL_NORMAL_STAGE_2  (v1 2) */ {},
	/* [68] GAMEMODE_SURVIVAL_NORMAL_STAGE_3  (v1 3) */ {},
	/* [69] GAMEMODE_SURVIVAL_NORMAL_STAGE_4  (v1 4) */ {},
	/* [70] GAMEMODE_SURVIVAL_NORMAL_STAGE_5  (v1 5) */ {},
	/* [71] GAMEMODE_TREE_OF_WISDOM  (v1 50) */ {},
	/* [72] GAMEMODE_CUSTOM_LEVEL（哨兵；策略层覆写）*/ {},
};
static_assert(sizeof(kInitLevelSeedRows) / sizeof(kInitLevelSeedRows[0]) == (int)GameMode::NUM_GAME_MODES,
	"固定卡牌表槽位须与 GameMode 枚举尾同步");
static_assert(sizeof(kInitLevelSeeds) / sizeof(kInitLevelSeeds[0]) == 62,
	"固定卡牌扁平段须与 kInitLevelSeedRows 各段界同步（0+3+2+3+6+1+3*4+4*3+6+8+9）");
}  // namespace

//0x40AF90
// GOTY @Patoke: 0x40D840
void Board::InitLevel()
{
	mLevelTimeNs = 0;
	mEnableGraveStones = false;
	mSodPosition = 0;
	mPrevBoardResult = mApp->mBoardResult;

	GameMode aGameMode = mApp->mGameMode;
	if (aGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM && aGameMode != GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		mApp->mMusic->StopAllMusic();
	}
	// 赋值当前关卡
	mLevel = mApp->IsAdventureMode() ? mApp->mPlayerInfo->mLevel : 0;
	// 设定关卡背景
	PickBackground();
	// 设定关卡出怪
	InitZombieWaves();
	// 设定关卡初始阳光数量
	if (aGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED || aGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST ||
		mApp->IsScaryPotterLevel() || mApp->IsWhackAZombieLevel())
	{
		mSunMoney = 0;
	}
	else if (aGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND)
	{
		mSunMoney = 5000;
	}
	else if (mApp->IsIZombieLevel())
	{
		mSunMoney = 150;
	}
	else if (mApp->IsFirstTimeAdventureMode() && mLevel == 1)
	{
		mSunMoney = 150;
	}
	else
	{
		mSunMoney = 50;
	}

	// 初始化行选择数组
	memset(mRowPickingArray, 0, sizeof(mRowPickingArray));
	// 初始化每行的基础数据
	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		mWaveRowGotLawnMowered[aRow] = -100;
		mIceMinX[aRow] = BOARD_ICE_START;
		mIceTimer[aRow] = 0;
		mIceParticleID[aRow] = ParticleSystemID::PARTICLESYSTEMID_NULL;
		mRowPickingArray[aRow].mItem = aRow;
	}
	// 初始化阳光掉落
	mNumSunsFallen = 0;
	if (!StageIsNight())
	{
		mSunCountDown = RandRangeInt(425, 700);
	}
	// 初始化字幕播放记录
	memset(mHelpDisplayed, 0, sizeof(mHelpDisplayed));
	// 初始化卡槽及卡牌
	mSeedBank->mNumPackets = GetNumSeedsInBank();
	mSeedBank->UpdateWidth();
	for (int i = 0; i < SEEDBANK_MAX; i++)
	{
		SeedPacket* aPacket = &mSeedBank->mSeedPackets[i];
		aPacket->mIndex = i;
		aPacket->mX = GetSeedPacketPositionX(i);
		aPacket->mY = 8;
		aPacket->mPacketType = SeedType::SEED_NONE;
	}
	// 设定固定卡牌
	// B 形态 -> 常量表（上方 kInitLevelSeedRows，GameMode 直下标；{} = 无固定卡牌 = 原链落空）；
	// 冒险模式 0 号槽为空，35/15 级由 IsScaryPotterLevel/IsWhackAZombieLevel 重定向取行（表外特例）
	const InitLevelSeedRow* aFixedRow = nullptr;
	if (aGameMode != GameMode::GAMEMODE_ADVENTURE)
	{
		aFixedRow = &kInitLevelSeedRows[(int)aGameMode];
		if (aFixedRow->mSeeds == nullptr)
		{
			aFixedRow = nullptr;
		}
	}
	else if (mApp->IsScaryPotterLevel())  // 冒险 35 级重定向
	{
		aFixedRow = &kInitLevelSeedRows[(int)GameMode::GAMEMODE_SCARY_POTTER_1];
	}
	else if (mApp->IsWhackAZombieLevel())  // 冒险 15 级重定向
	{
		aFixedRow = &kInitLevelSeedRows[(int)GameMode::GAMEMODE_MINIGAME_WHACK_A_ZOMBIE];
	}

	if (aFixedRow != nullptr)
	{
		if (aFixedRow->mAssert)
		{
			TOD_ASSERT(mSeedBank->mNumPackets == aFixedRow->mCount);
		}
		for (int i = 0; i < aFixedRow->mCount; i++)
		{
			mSeedBank->mSeedPackets[i].SetPacketType(aFixedRow->mSeeds[i]);
		}
		// 冒险 15 级砸僵尸重定向：第 3 槽为樱桃炸弹（挑战模式为寒冰菇，表外特例）
		if (aGameMode == GameMode::GAMEMODE_ADVENTURE && mApp->IsWhackAZombieLevel())
		{
			mSeedBank->mSeedPackets[2].SetPacketType(SeedType::SEED_CHERRYBOMB);
		}
	}
	else if (!ChooseSeedsOnCurrentLevel() && !HasConveyorBeltSeedBank())
	{
		mSeedBank->mNumPackets = GetNumSeedsInBank();
		// 卡槽错误的关卡，依次填充所有卡牌
		for (int i = 0; i < mSeedBank->mNumPackets; i++)
		{
			mSeedBank->mSeedPackets[i].SetPacketType((SeedType)i);
		}
	}
	// 将所有子控件标记为已变动
	MarkAllDirty();

	mPaused = false;
	mOutOfMoneyCounter = 0;
	if (StageHasFog())
	{
		mFogBlownCountDown = 200;
		mFogOffset = 1065 - LeftFogColumn() * 80;
	}
	// 关卡玩法相关的初始化
	mChallenge->InitLevel();
}

Reanimation* Board::CreateRakeReanim(float theRakeX, float theRakeY, int theRenderOrder)
{
	Reanimation* aReanim = mApp->AddReanimation(theRakeX + 20, theRakeY, theRenderOrder, REANIM_RAKE, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), AnimationTimeDomain());
	aReanim->ApplyRate(0.0f);
	aReanim->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
	aReanim->mIsAttachment = true;
	return aReanim;
}

//0x40B9C0
void Board::PlaceRake()
{
	if (!mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_RAKE])
		return;

	int aGridX = 7;
	if (mApp->IsScaryPotterLevel())
	{
		GridItem* aGridItem = nullptr;
		while (IterateGridItems(aGridItem))
		{
			if (aGridItem->mGridItemType == GridItemType::GRIDITEM_SCARY_POT && aGridItem->mGridX <= aGridX && aGridItem->mGridX > 0)
			{
				aGridX = aGridItem->mGridX - 1;
			}
		}
	}
	else
	{
		if (!StageHasZombieWalkInFromRight() || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED ||
			mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BOBSLED_BONANZA)
			return;
	}

	int aPickCount = 0;
	TodWeightedArray aPickArray[MAX_GRID_SIZE_Y];
	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		if (aRow != 5 && mPlantRow[aRow] == PlantRowType::PLANTROW_NORMAL)
		{
			aPickArray[aPickCount].mWeight = 1;
			aPickArray[aPickCount].mItem = aRow;
			aPickCount++;
		}
	}
	if (aPickCount == 0)
		return;

	int aGridY = TodPickFromWeightedArray(aPickArray, aPickCount);
	mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_RAKE]--;
	GridItem* aRake = mGridItems.DataArrayAlloc();
	aRake->mGridItemType = GridItemType::GRIDITEM_RAKE;
	aRake->mGridX = aGridX;
	aRake->mGridY = aGridY;
	aRake->mPosX = GridToPixelX(aGridX, aGridY);
	aRake->mPosY = GridToPixelY(aGridX, aGridY);
	aRake->mRenderOrder = MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, aGridY, 9);
	aRake->mGridItemReanimID = mApp->ReanimationGetID(CreateRakeReanim(aRake->mPosX, aRake->mPosY, 0)); // Lmao gotta pass in the right coords
	aRake->mGridItemState = GridItemState::GRIDITEM_STATE_RAKE_ATTRACTING;
}

//0x40BC70
void Board::InitLawnMowers()
{
	GameMode aGameMode = mApp->mGameMode;
	// 这里优化一下原版的代码，事先列举一些不创建小推车的关卡
	if (aGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED || aGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST ||
		aGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN || aGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		aGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND || aGameMode == GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM ||
		mApp->IsSquirrelLevel() || mApp->IsIZombieLevel() || (StageHasRoof() && !mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_ROOF_CLEANER]))
		return;

	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		if ((aGameMode == GameMode::GAMEMODE_LIMBO_RESODDED && aRow <= 4) ||
			(mApp->IsAdventureMode() && mLevel == 35) ||   // 这里原版没有对于行的判断，故冒险模式 4-5 关卡有 6 行小推车
			(!mApp->IsScaryPotterLevel() && mPlantRow[aRow] != PlantRowType::PLANTROW_DIRT))  // 除冒险模式 4-5 关卡外的破罐者模式关卡无小推车
		{
			LawnMower* aLawnMower = mLawnMowers.DataArrayAlloc();
			aLawnMower->LawnMowerInitialize(aRow);
			aLawnMower->mVisible = false;
		}
	}
}

//0x40BD30
bool Board::ChooseSeedsOnCurrentLevel()
{
	if (mApp->IsChallengeWithoutSeedBank() || HasConveyorBeltSeedBank())
		return false;

	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ICE ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM)
		return false;

	if (mApp->IsIZombieLevel() || mApp->IsSlotMachineLevel())
		return false;

	return (!mApp->IsFirstTimeAdventureMode() || mLevel > 7);
}

//0x40BE00
// GOTY @Patoke: 0x40E6A0
void Board::StartLevel()
{
	mCoinBankFadeCount = 0;
	mApp->mLastLevelStats->mUnusedLawnMowers = 0;
	mChallenge->StartLevel();

	if (mApp->mGameMode == GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_1 ||
		mApp->mGameMode == GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_2 ||
		mApp->mGameMode == GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_3 ||
		mApp->mGameMode == GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_4 ||
		mApp->mGameMode == GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_5)
	{
		if (GetSurvivalFlagsCompleted() >= 20) {
			if (mApp->mPlayerInfo && !mApp->mPlayerInfo->mEarnedAchievements[Immortal])
			{
				ReportAchievement::GiveAchievement(mApp, Immortal, true);
			}
		}
	}

	if (mApp->IsSurvivalMode() && mChallenge->mSurvivalStage > 0)
	{
		mApp->EraseFile(GetSavedGameName(mApp->mGameMode, mApp->mPlayerInfo->mId));
		FreezeEffectsForCutscene(false);
		mApp->mSoundSystem->GamePause(false);
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ICE ||
		mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		mApp->mGameMode == GameMode::GAMEMODE_INTRO ||
		mApp->IsFinalBossLevel())
		return;

	mApp->mMusic->StartGameMusic();
}

//0x40BF10
LawnMower* Board::GetBottomLawnMower()
{
	LawnMower* aLawnMower = nullptr;
	LawnMower* aBottomMower = nullptr;
	while (IterateLawnMowers(aLawnMower))
	{
		if (aLawnMower->mMowerState == LawnMowerState::MOWER_TRIGGERED || aLawnMower->mMowerState == LawnMowerState::MOWER_SQUISHED)
			continue;

		if (aBottomMower == nullptr || aBottomMower->mRow < aLawnMower->mRow)
		{
			aBottomMower = aLawnMower;
		}
	}
	return aBottomMower;
}

//0x40BF60
// GOTY @Patoke: 0x40E860
void Board::UpdateLevelEndSequence()
{
	if (mNextSurvivalStageCounter > 0)
	{
		if (!IsScaryPotterDaveTalking())
		{
			mNextSurvivalStageCounter--;
			if (mApp->IsAdventureMode() && mApp->IsScaryPotterLevel() && mNextSurvivalStageCounter == 300)
			{
				mApp->CrazyDaveEnter();
				mApp->CrazyDaveTalkIndex(mChallenge->mSurvivalStage == 0 ? 2700 : 2800);
				mChallenge->PuzzleNextStageClear();
				mNextSurvivalStageCounter = 100;
			}
		}

		if (mNextSurvivalStageCounter == 1 && mApp->IsSurvivalMode())
		{
			TryToSaveGame();
		}

		if (!mNextSurvivalStageCounter)
		{
			if (mApp->IsScaryPotterLevel())
			{
				if (mApp->IsAdventureMode())
					return;

				if (!IsFinalScaryPotterStage())
				{
					mChallenge->PuzzleNextStageClear();
					mChallenge->ScaryPotterPopulate();
				}
			}
			else if (LawnApp::IsEndlessIZombie(mApp->mGameMode))
			{
				mChallenge->PuzzleNextStageClear();
				mChallenge->IZombieInitLevel();
			}
			else if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND)
			{
				ClearAdvice(AdviceType::ADVICE_NONE);
			}
			else
			{
				mLevelComplete = true;
				RemoveZombiesForRepick();
			}
			return;
		}
	}

	if (mBoardFadeOutCounter < 0)
		return;

	mBoardFadeOutCounter--;
	if (mBoardFadeOutCounter == 0)
	{
		mLevelComplete = true;
		return;
	}
	if (mBoardFadeOutCounter == 300)
	{
		if (!IsSurvivalStageWithRepick() && !(mLevel == 9 || mLevel == 19 || mLevel == 29 || mLevel == 39 || mLevel == 49))
		{
			mApp->PlaySample(Sexy::SOUND_LIGHTFILL);
		}
	}

	if (mScoreNextMowerCounter > 0)
	{
		mScoreNextMowerCounter--;
		if (mScoreNextMowerCounter)
		{
			return;
		}
	}

	if (CanDropLoot() && !IsSurvivalStageWithRepick())
	{
		mScoreNextMowerCounter = 40;
		LawnMower* aLawnMower = GetBottomLawnMower();
		if (aLawnMower)
		{
			AddCoin(aLawnMower->mPosX + 40, aLawnMower->mPosY + 40, CoinType::COIN_GOLD, CoinMotion::COIN_MOTION_LAWNMOWER_COIN);
			SoundInstance* aSoundInstance = mApp->mSoundManager->GetSoundInstance(Sexy::SOUND_POINTS);
			if (aSoundInstance)
			{
				aSoundInstance->Play(false, true);
				float aPitch = ClampFloat(6 - CountUntriggerLawnMowers(), 0.0f, 6.0f);
				aSoundInstance->AdjustPitch(aPitch);
			}
			aLawnMower->Die();
		}
	}
}

//0x40C2C0
void Board::CompleteEndLevelSequenceForSaving()
{
	if (CanDropLoot())
	{
		LawnMower* aLawnMower = nullptr;
		while (IterateLawnMowers(aLawnMower))
		{
			if (aLawnMower->mMowerState != LawnMowerState::MOWER_TRIGGERED && aLawnMower->mMowerState != LawnMowerState::MOWER_SQUISHED)
			{
				int aCoinValue = Coin::GetCoinValue(CoinType::COIN_GOLD);
				mApp->mPlayerInfo->AddCoins(aCoinValue);
				mCoinsCollected += aCoinValue;
			}
		}
	}

	Coin* aCoin = nullptr;
	while (IterateCoins(aCoin))
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

	mApp->UpdatePlayerProfileForFinishingLevel();
}

//0x40C3E0
void Board::FadeOutLevel()
{
	if (mApp->mGameScene != GameScenes::SCENE_PLAYING)
	{
		RefreshSeedPacketFromCursor();
		mApp->mLastLevelStats->mUnusedLawnMowers = 0;
		mLevelComplete = true;
	}

	bool aNeedSoundEffect = true;
	if (mApp->IsScaryPotterLevel() && !IsFinalScaryPotterStage())
	{
		aNeedSoundEffect = false;
	}
	else if (IsSurvivalStageWithRepick() || IsLastStandStageWithRepick() || mApp->IsEndlessIZombie(mApp->mGameMode))
	{
		aNeedSoundEffect = false;
	}
	if (aNeedSoundEffect)
	{
		mApp->mMusic->StopAllMusic();
		if (mApp->IsAdventureMode() && mLevel == 50)
		{
			mApp->PlayFoley(FoleyType::FOLEY_FINAL_FANFARE);
		}
		else if (mApp->TrophiesNeedForGoldSunflower() == 1)
		{
			mApp->PlayFoley(FoleyType::FOLEY_FINAL_FANFARE);
		}
		else
		{
			mApp->PlayFoley(FoleyType::FOLEY_WINMUSIC);
		}
	}

	if (mApp->IsScaryPotterLevel() && !IsFinalScaryPotterStage())
	{
		mNextSurvivalStageCounter = 500;
		if (mApp->IsAdventureMode())
		{
			ClearAdvice(AdviceType::ADVICE_NONE);
		}
		else
		{
			mLevelAwardSpawned = true;
			SexyString aStreakStr = mApp->IsEndlessScaryPotter(mApp->mGameMode) ? gStr.ADVICE_MORE_SCARY_POTS : gStr.ADVICE_3_IN_A_ROW;
			SexyString aMessage = TodReplaceNumberString(aStreakStr, UiTextPlaceholder::Streak, mChallenge->mSurvivalStage + 1);
			PuzzleSaveStreak();
			ClearAdvice(AdviceType::ADVICE_NONE);
			DisplayAdvice(aMessage, MessageStyle::MESSAGE_STYLE_BIG_MIDDLE, AdviceType::ADVICE_NONE);
		}
		return;
	}

	if (mApp->IsEndlessIZombie(mApp->mGameMode))
	{
		mNextSurvivalStageCounter = 500;
		SexyString aMessage = TodReplaceNumberString(gStr.ADVICE_MORE_IZOMBIE, UiTextPlaceholder::Streak, mChallenge->mSurvivalStage + 1);
		PuzzleSaveStreak();
		ClearAdvice(AdviceType::ADVICE_NONE);
		DisplayAdvice(aMessage, MessageStyle::MESSAGE_STYLE_BIG_MIDDLE, AdviceType::ADVICE_NONE);
		return;
	}

	if (IsLastStandStageWithRepick())
	{
		mNextSurvivalStageCounter = 500;
		mChallenge->LastStandCompletedStage();
		return;
	}

	if (!IsSurvivalStageWithRepick())
	{
		RefreshSeedPacketFromCursor();
		mApp->mLastLevelStats->mUnusedLawnMowers = CountUntriggerLawnMowers();

		mBoardFadeOutCounter = 600;
		if (mLevel == 9 || mLevel == 19 || mLevel == 29 || mLevel == 39 || mLevel == 49)
		{
			mBoardFadeOutCounter = 500;
		}

		if (CanDropLoot())
		{
			mScoreNextMowerCounter = 200;
		}

		Coin* aCoin = nullptr;
		while (IterateCoins(aCoin))
		{
			aCoin->TryAutoCollectAfterLevelAward();
		}
	}
	else
	{
		TOD_ASSERT(mApp->IsSurvivalMode());
		mNextSurvivalStageCounter = 500;
		DisplayAdvice(gStr.ADVICE_MORE_ZOMBIES, MessageStyle::MESSAGE_STYLE_BIG_MIDDLE, AdviceType::ADVICE_NONE);
		mApp->mMusic->FadeOut(500);
		mApp->PlaySample(Sexy::SOUND_HUGE_WAVE);
		for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
		{
			mIceTimer[aRow] = mNextSurvivalStageCounter;
		}
	}

	mApp->SetCursor(CURSOR_POINTER);
}

void Board::DisplayAdvice(const SexyString& theAdvice, MessageStyle theMessageStyle, AdviceType theHelpIndex)
{
	if (theHelpIndex != AdviceType::ADVICE_NONE)
	{
		if (mHelpDisplayed[theHelpIndex])
			return;

		mHelpDisplayed[theHelpIndex] = true;
	}

	mAdvice->SetLabel(theAdvice, theMessageStyle);
	mHelpIndex = theHelpIndex;
}

//0x40CA10
void Board::DisplayAdviceAgain(const SexyString& theAdvice, MessageStyle theMessageStyle, AdviceType theHelpIndex)
{
	if (theHelpIndex != AdviceType::ADVICE_NONE)
	{
		mHelpDisplayed[theHelpIndex] = false;
	}
	DisplayAdvice(theAdvice, theMessageStyle, theHelpIndex);
}

//0x40CA50
void Board::ClearAdviceImmediately()
{
	ClearAdvice(AdviceType::ADVICE_NONE);
	mAdvice->mEndTimeNs = 0;
}

//0x40CAB0
void Board::ClearAdvice(AdviceType theHelpIndex)
{
	if (theHelpIndex == AdviceType::ADVICE_NONE || theHelpIndex == mHelpIndex)
	{
		mAdvice->ClearLabel();
		mHelpIndex = AdviceType::ADVICE_NONE;
	}
}

//0x40CB10
Coin* Board::AddCoin(int theX, int theY, CoinType theCoinType, CoinMotion theCoinMotion)
{
	Coin* aCoin = mCoins.DataArrayAlloc();
	aCoin->CoinInitialize(theX, theY, theCoinType, theCoinMotion);
	if (mApp->IsFirstTimeAdventureMode() && mLevel == 1)
	{
		DisplayAdvice(gStr.ADVICE_CLICK_ON_SUN, MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY, AdviceType::ADVICE_CLICK_ON_SUN);
	}
	return aCoin;
}

//0x40CCE0
bool Board::IsPlantInCursor()
{
	return
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_USABLE_COIN ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_DUPLICATOR ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW;
}

//0x40CD10
// GOTY @Patoke: 0x40F600
SeedType Board::GetSeedTypeInCursor()
{
	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_WHEEELBARROW)
	{
		PottedPlant* aPottedPlant = mApp->mZenGarden->GetPottedPlantInWheelbarrow();
		if (aPottedPlant)
		{
			return aPottedPlant->mSeedType;
		}
	}

	if (!IsPlantInCursor())
	{
		return SeedType::SEED_NONE;
	}
	return mCursorObject->mType == SeedType::SEED_IMITATER ? mCursorObject->mImitaterType : mCursorObject->mType;
}

//0x40CD80
void Board::RefreshSeedPacketFromCursor()
{
	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_USABLE_COIN)
	{
		mCoins.DataArrayTryToGet(mCursorObject->mCoinID)->DroppedUsableSeed();
	}
	else if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK)
	{
		TOD_ASSERT(mCursorObject->mSeedBankIndex >= 0 && mCursorObject->mSeedBankIndex < mSeedBank->mNumPackets);
		mSeedBank->mSeedPackets[mCursorObject->mSeedBankIndex].Activate();
	}
	ClearCursor();
}

//0x40CE00
bool Board::IsPoolSquare(int theGridX, int theGridY)
{
	if (theGridX >= 0 && theGridY >= 0)
	{
		TOD_ASSERT(theGridX < MAX_GRID_SIZE_X && theGridY < MAX_GRID_SIZE_Y);
		return mGridSquareType[theGridX][theGridY] == GridSquareType::GRIDSQUARE_POOL;
	}
	return false;
}

//0x40CE20
Plant* Board::NewPlant(int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType)
{
	const auto domain = AnimationTimeDomain();
	const auto visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	const auto animationNowNs = domain == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : visualNowNs;
	Plant* aPlant = mPlants.DataArrayAlloc();
	aPlant->mIsOnBoard = true;
	aPlant->PlantInitialize(static_cast<std::int64_t>(mLevelTimeNs), visualNowNs, animationNowNs, theGridX, theGridY, theSeedType, theImitaterType);
	// Creation already supplied timestamps in this owner's clock. Deferring the
	// binding to the next update would rebase at its end and discard that step.
	auto& holder = *mApp->mEffectSystem->mReanimationHolder;
	for (const auto id : {aPlant->mBodyReanimID, aPlant->mHeadReanimID, aPlant->mHeadReanimID2,
		aPlant->mHeadReanimID3, aPlant->mBlinkReanimID, aPlant->mLightReanimID, aPlant->mSleepingReanimID})
		if (auto* animation = mApp->ReanimationTryToGet(id))
			holder.BindAnimationTime(*animation, animation->mIsAttachment ? domain : ReanimTimeDomain::Visual,
				animation->mIsAttachment ? animationNowNs : visualNowNs);
	RegisterPlantCellBucket(aPlant);
	return aPlant;
}

//0x40CE60
// 格桶第二批审计（2026-10-03）：本函数无植物池扫描（种植特效只发
// 音效/粒子，不同格找植物）——桶化名单所列"种植特效找同格植物"形态
// 不存在，无改造对象。AddPlant 内 NewPlant 先注册进链再调本函数的
// 时序问题随之不适用。
void Board::DoPlantingEffects(int theGridX, int theGridY, Plant* thePlant)
{
	int aXPos = GridToPixelX(theGridX, theGridY) + 41;
	int aYPos = GridToPixelY(theGridX, theGridY) + 74;
	if (thePlant)
	{
		if (thePlant->mSeedType == SeedType::SEED_LILYPAD)
		{
			aYPos += 15;
		}
		else if (thePlant->mSeedType == SeedType::SEED_FLOWERPOT)
		{
			aYPos += 30;
		}
	}

	if (mBackground == BackgroundType::BACKGROUND_GREENHOUSE)
	{
		mApp->PlayFoley(FoleyType::FOLEY_CERAMIC);
		return;
	}
	if (mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
	{
		mApp->PlayFoley(FoleyType::FOLEY_PLANT_WATER);
		return;
	}
	if (Plant::IsFlying(thePlant->mSeedType))
	{
		mApp->PlayFoley(FoleyType::FOLEY_PLANT);
		return;
	}

	if (IsPoolSquare(theGridX, theGridY))
	{
		mApp->PlayFoley(FoleyType::FOLEY_PLANT_WATER);
		mApp->AddTodParticle(aXPos, aYPos, RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_PLANTING_POOL, static_cast<std::int64_t>(mLevelTimeNs));
	}
	else
	{
		mApp->PlayFoley(FoleyType::FOLEY_PLANT);
		mApp->AddTodParticle(aXPos, aYPos, RenderLayer::RENDER_LAYER_TOP, ParticleEffect::PARTICLE_PLANTING, static_cast<std::int64_t>(mLevelTimeNs));
	}
}

//0x40D120
// GOTY @Patoke: 0x40FA10
Plant* Board::AddPlant(int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType)
{
	Plant* aPlant = NewPlant(theGridX, theGridY, theSeedType, theImitaterType);
	DoPlantingEffects(theGridX, theGridY, aPlant);
	mChallenge->PlantAdded(aPlant);

	int aSunPlantsCount = CountPlantByType(SeedType::SEED_SUNSHROOM) + CountPlantByType(SeedType::SEED_SUNFLOWER);
	if (aSunPlantsCount > mMaxSunPlants)
	{
		mMaxSunPlants = aSunPlantsCount;  //mMaxSunPlants = max(aSunPlantsCount, mMaxSunPlants);
	}

	// @Patoke: implemented
	if (theSeedType == SeedType::SEED_PEASHOOTER ||
		theSeedType == SeedType::SEED_SNOWPEA ||
		theSeedType == SeedType::SEED_REPEATER ||
		theSeedType == SeedType::SEED_THREEPEATER ||
		theSeedType == SeedType::SEED_SPLITPEA ||
		theSeedType == SeedType::SEED_GATLINGPEA)
	{
		mPeaShooterUsed = true;
	}
	if (theSeedType == SeedType::SEED_CABBAGEPULT ||
		theSeedType == SeedType::SEED_KERNELPULT ||
		theSeedType == SeedType::SEED_MELONPULT ||
		theSeedType == SeedType::SEED_WINTERMELON)
	{
		mCatapultPlantsUsed = true;
	}

	bool aIsFungi = Plant::IsFungus(theSeedType);
	if (!Plant::IsFlying(theSeedType) && !aIsFungi) {
		mMushroomAndCoffeeBeansOnly = false;
	}
	if (aIsFungi) {
		mMushroomsUsed = true;
	}

	return aPlant;
}

//0x40D1A0
// GOTY @Patoke: 0x40FBA0
Plant* Board::GetPumpkinAt(int theGridX, int theGridY)
{
	// O(1) 桶读（南瓜层）：mSeedType==SEED_PUMPKINSHELL 判定保留——
	// 模仿者南瓜解析进南瓜层，但原全池扫按裸类型不认它，语义逐字保持。
	Plant* aPlant = GetCellPlant(theGridY, theGridX, 2 /* PLANT_CELL_PUMPKIN */);
	if (aPlant != nullptr && !aPlant->NotOnGround() && aPlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		return aPlant;
	}
	return nullptr;
}

//0x40D220
Plant* Board::GetFlowerPotAt(int theGridX, int theGridY)
{
	Plant* aPlant = nullptr;
	while (IteratePlants(aPlant))
	{
		if (aPlant->mPlantCol == theGridX && aPlant->mRow == theGridY && !aPlant->NotOnGround() && aPlant->mSeedType == SeedType::SEED_FLOWERPOT)
		{
			return aPlant;
		}
	}
	return nullptr;
}

//0x40D2A0
namespace
{
// 植物格桶（2026-10-03 用户方案，层特化）：0=底座(花盆/荷叶) 1=普通
// 2=南瓜壳 3=飞行(咖啡豆)。sizeof(Board) 受 dat64 整块镜像硬校验不可加
// 成员（Board.h 注），桶体放函数域 static——条目3/4 缓存先例；单 Board
// 存活期约定下安全，Board 构造/Dispose/池 FreeAll 后必须 Clear 或 Rebuild。
// 槽位号链（2026-10-03 用户方案）：next[槽号] = 同格同层链的下一株槽号
// （降序排列=原池扫可见序；0xFFFF=链尾）。桶头即链头，头死 O(1) 顶替，
// 全事件零重扫；改版可用链遍历叠种名册。uint16 槽号 2KB。
constexpr uint16_t PLANT_CELL_NEXT_NONE = 0xFFFF;
// 桶表尺寸统一走 Board.h 的 PLANT_POOL_CAPACITY（改版扩容单点改）
constexpr int PLANT_POOL_SLOTS = PLANT_POOL_CAPACITY;
struct PlantCellBucket
{
	// 头槽与链同域（2026-10-03 用户方案）：uint16 池槽位号，0xFFFF=空——
	// 全结构纯索引域，无指针；槽位号经 PlantAtSlot 换 Plant*（纪律保证
	// 结构内索引恒为活槽：摘链先于 free）
	uint16_t mCells[MAX_GRID_SIZE_Y][MAX_GRID_SIZE_X][PLANT_CELL_COUNT];
	uint16_t mNext[PLANT_POOL_SLOTS];
	PlantCellBucket() { Clear(); }
	void Clear()
	{
		std::memset(mCells, 0xFF, sizeof(mCells));
		std::memset(mNext, 0xFF, sizeof(mNext));
	}
};

PlantCellBucket& ThePlantCellBucket()
{
	static PlantCellBucket sBucket;
	return sBucket;
}


// 玉米炮（含模仿者解析后）占锚列+右列两格（原查询 mPlantCol∈[x-1,x]）
bool PlantCoversTwoCols(const Plant* thePlant)
{
	SeedType aSeedType = thePlant->mSeedType;
	if (aSeedType == SeedType::SEED_IMITATER && thePlant->mImitaterType != SeedType::SEED_NONE)
	{
		aSeedType = thePlant->mImitaterType;
	}
	return aSeedType == SeedType::SEED_COBCANNON;
}

// Cell layer classification for bucket registration; the seed type has
// already been resolved for imitater plants.
int ResolvePlantCellLayer(SeedType theSeedType, GameMode theGameMode)
{
	if (Plant::IsFlying(theSeedType))
	{
		return PLANT_CELL_FLYING;
	}
	if (theSeedType == SeedType::SEED_FLOWERPOT || (theSeedType == SeedType::SEED_LILYPAD && theGameMode != GameMode::GAMEMODE_LIMBO_ZEN_GARDEN))
	{
		return PLANT_CELL_UNDER;
	}
	if (theSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		return PLANT_CELL_PUMPKIN;
	}
	return PLANT_CELL_NORMAL;
}

SeedType ResolvePlantSeedType(const Plant* thePlant)
{
	if (thePlant->mSeedType == SeedType::SEED_IMITATER && thePlant->mImitaterType != SeedType::SEED_NONE)
	{
		return thePlant->mImitaterType;
	}
	return thePlant->mSeedType;
}

// ── 僵尸行段桶（2026-10-04 用户方案，行段形态；千尸局基准：全池扫
// 290ns/查询 vs 行段 92ns，格桶式 next 长链 232ns 被否）──
// 每行一段槽位号**升序**紧凑数组（升序 = 原全池扫 IterateNext 升序槽位
// 按行过滤后的可见序——迭代序等价由构造保证，读侧免决胜重排）；
// Boss 僵尸独占侧数组（原查询对 Boss 行偏差归零 = 跨行候选，不入行段
// 防双计）；mSlotIdx 反向指针（槽→行内下标，0xFFFF=不在桶）供换行/删除
// O(定位) 与迭代续走。插入/删除搬移时同步维护反向指针。用户裁决：植物
// 格桶的 next 链是叠种特化，僵尸无"影子僵尸"，纯行段即可。
constexpr uint16_t ZOMBIE_ROWSEG_NONE = 0xFFFF;
// Boss 只在侧数组（2026-10-04 用户裁决否决双成员）：裸 row== 查询走归并
// 迭代器（行段∪全部 Boss），循环体原样保留的 row 检查按真实行过滤 Boss
// ——与 1073 池扫"评估后过滤"逐字同构；"行偏差归零"类查询的 Boss 豁免
// 在体内。每查询多评估 ≤活 Boss 只（实践 ≤1）。
struct ZombieRowBucket
{
	uint16_t mRows[MAX_GRID_SIZE_Y][ZOMBIE_POOL_CAPACITY];
	uint16_t mRowCount[MAX_GRID_SIZE_Y];
	uint16_t mBosses[ZOMBIE_POOL_CAPACITY];  // 上限防御性取池容量；实际活 Boss ≤1
	uint16_t mBossCount;
	uint16_t mSlotIdx[ZOMBIE_POOL_CAPACITY];  // 槽→侧数组下标（Boss）/ 行段下标（其余）
	ZombieRowBucket() { Clear(); }
	void Clear()
	{
		std::memset(mRowCount, 0, sizeof(mRowCount));
		mBossCount = 0;
		std::memset(mSlotIdx, 0xFF, sizeof(mSlotIdx));
	}
};

ZombieRowBucket& TheZombieRowBucket()
{
	static ZombieRowBucket sBucket;
	return sBucket;
}

// 升序插入/删除：搬移同步回写反向指针；定位用反向指针 O(1)
void RowSegInsert(uint16_t* theArr, uint16_t& theCount, uint16_t theSlot, uint16_t* theSlotIdx)
{
	TOD_ASSERT(theCount < ZOMBIE_POOL_CAPACITY);
	uint16_t aPos = theCount;
	while (aPos > 0 && theArr[aPos - 1] > theSlot)
	{
		theArr[aPos] = theArr[aPos - 1];
		theSlotIdx[theArr[aPos]] = aPos;
		aPos--;
	}
	theArr[aPos] = theSlot;
	theSlotIdx[theSlot] = aPos;
	theCount++;
}

void RowSegRemove(uint16_t* theArr, uint16_t& theCount, uint16_t theSlot, uint16_t* theSlotIdx)
{
	const uint16_t aPos = theSlotIdx[theSlot];
	TOD_ASSERT(aPos != ZOMBIE_ROWSEG_NONE && aPos < theCount && theArr[aPos] == theSlot);
	for (uint16_t i = (uint16_t)(aPos + 1); i < theCount; i++)
	{
		theArr[i - 1] = theArr[i];
		theSlotIdx[theArr[i - 1]] = (uint16_t)(i - 1);
	}
	theCount--;
	theSlotIdx[theSlot] = ZOMBIE_ROWSEG_NONE;
}


}  // namespace

void Board::GetPlantsOnLawn(int theGridX, int theGridY, PlantsOnLawn* thePlantOnLawn)
{
	thePlantOnLawn->mUnderPlant = nullptr;
	thePlantOnLawn->mPumpkinPlant = nullptr;
	thePlantOnLawn->mFlyingPlant = nullptr;
	thePlantOnLawn->mNormalPlant = nullptr;

	if (theGridX < 0 || theGridX >= MAX_GRID_SIZE_X || theGridY < 0 || theGridY >= MAX_GRID_SIZE_Y)
		return;

	if (mApp->IsWallnutBowlingLevel() && !mCutScene->IsInShovelTutorial())
		return;

	// 桶链读（2026-10-03）：每层沿槽位号降序链找**第一个落地**的株
	// （=原池扫在该层的可见者——腾空株池扫亦跳过；链头腾空时顺位下移，
	// shadow 于 Beghouled 腾空窗口抓获此差后修正）。正常玩法链长 1。
	PlantCellBucket& aBucket = ThePlantCellBucket();
	for (int aLayer = 0; aLayer < PLANT_CELL_COUNT; aLayer++)
	{
		uint16_t aIdx = aBucket.mCells[theGridY][theGridX][aLayer];
		Plant* aPlant = PlantAtSlot(aIdx);
		while (aPlant != nullptr && aPlant->NotOnGround())
		{
			aIdx = aBucket.mNext[aIdx];
			aPlant = PlantAtSlot(aIdx);
		}
		if (aPlant == nullptr)
			continue;
		switch (aLayer)
		{
		case PLANT_CELL_UNDER: thePlantOnLawn->mUnderPlant = aPlant; break;
		case PLANT_CELL_PUMPKIN: thePlantOnLawn->mPumpkinPlant = aPlant; break;
		case PLANT_CELL_FLYING: thePlantOnLawn->mFlyingPlant = aPlant; break;
		default: thePlantOnLawn->mNormalPlant = aPlant; break;
		}
	}
}

// 桶维护：注册/注销/重建。调用点 = NewPlant（注册）、ProcessDeleteQueue
// （注销）、Beghouled 交换与禅园搬盆（先注销后改坐标再注册）、池
// FreeAll/装载后（Clear+Rebuild）。
Plant* Board::GetCellPlant(int theRow, int theCol, int theLayer)
{
	if ((unsigned)theRow >= MAX_GRID_SIZE_Y || (unsigned)theCol >= MAX_GRID_SIZE_X ||
		(unsigned)theLayer >= PLANT_CELL_COUNT)
		return nullptr;
	return PlantAtSlot(ThePlantCellBucket().mCells[theRow][theCol][theLayer]);
}

// 桶维护 = 纯索引域链操作（2026-10-03 用户方案：头槽与 next 同为 uint16
// 槽位号）。链按槽位号降序 = 原池扫可见序；头死 O(1) 顶替，全事件零重扫。
Plant* Board::PlantAtSlot(unsigned int theSlot)
{
	if (theSlot >= mPlants.mMaxSize)
		return nullptr;
	return &mPlants.mBlock[theSlot].mItem;
}

unsigned int Board::PlantSlotOf(const Plant* thePlant)
{
	return mPlants.DataArrayIndexOf(thePlant);
}

Plant* Board::NextCellPlant(const Plant* thePlant)
{
	// 同格同层链的下一株（槽位号降序）；链尾返回 nullptr
	const uint16_t aNext = ThePlantCellBucket().mNext[PlantSlotOf(thePlant)];
	return aNext == PLANT_CELL_NEXT_NONE ? nullptr : PlantAtSlot(aNext);
}


void Board::InsertCellChain(int theRow, int theCol, int theLayer, unsigned int theSlot)
{
	// 原语边界契约（2026-10-06 玉米炮双格修复）：越界格（含玉米炮右格
	// 越出最右列）无桶可挂——此前会写进相邻行头或 mNext 损坏链（密集表
	// 边界保护方针）
	if ((unsigned)theRow >= MAX_GRID_SIZE_Y || (unsigned)theCol >= MAX_GRID_SIZE_X ||
		(unsigned)theLayer >= PLANT_CELL_COUNT || theSlot >= PLANT_POOL_SLOTS)
		return;
	PlantCellBucket& aBucket = ThePlantCellBucket();
	uint16_t* aHead = &aBucket.mCells[theRow][theCol][theLayer];
	if (*aHead == PLANT_CELL_NEXT_NONE || theSlot > *aHead)
	{
		aBucket.mNext[theSlot] = *aHead;  // 空槽写入 NONE 亦正确
		*aHead = (uint16_t)theSlot;
		return;
	}
	// 已有更大头：沿链找插入位（正常玩法链长 1，走 0 步）
	unsigned int aCur = *aHead;
	for (;;)
	{
		const uint16_t aNext = aBucket.mNext[aCur];
		if (aNext == PLANT_CELL_NEXT_NONE || aNext < theSlot)
		{
			aBucket.mNext[theSlot] = aNext;
			aBucket.mNext[aCur] = (uint16_t)theSlot;
			return;
		}
		aCur = aNext;
	}
}

void Board::RemoveCellChain(int theRow, int theCol, int theLayer, unsigned int theSlot)
{
	// 原语边界契约（与 InsertCellChain 同判）——越界格无链可摘
	if ((unsigned)theRow >= MAX_GRID_SIZE_Y || (unsigned)theCol >= MAX_GRID_SIZE_X ||
		(unsigned)theLayer >= PLANT_CELL_COUNT || theSlot >= PLANT_POOL_SLOTS)
		return;
	PlantCellBucket& aBucket = ThePlantCellBucket();
	uint16_t* aHead = &aBucket.mCells[theRow][theCol][theLayer];
	if (*aHead == theSlot)
	{
		*aHead = aBucket.mNext[theSlot];
		aBucket.mNext[theSlot] = PLANT_CELL_NEXT_NONE;
		return;
	}
	unsigned int aCur = *aHead;
	while (aCur != PLANT_CELL_NEXT_NONE)
	{
		const uint16_t aNext = aBucket.mNext[aCur];
		if (aNext == theSlot)
		{
			aBucket.mNext[aCur] = aBucket.mNext[theSlot];
			aBucket.mNext[theSlot] = PLANT_CELL_NEXT_NONE;
			return;
		}
		aCur = aNext;
	}
}

void Board::RegisterPlantCellBucket(Plant* thePlant)
{
	if (thePlant == nullptr || (unsigned)thePlant->mPlantCol >= MAX_GRID_SIZE_X ||
		(unsigned)thePlant->mRow >= MAX_GRID_SIZE_Y)
		return;
	const unsigned int aSlot = PlantSlotOf(thePlant);
	const int aLayer = ResolvePlantCellLayer(ResolvePlantSeedType(thePlant), mApp->mGameMode);
	InsertCellChain(thePlant->mRow, thePlant->mPlantCol, aLayer, aSlot);
	// 玉米炮占锚列+右列两格（原查询 mPlantCol∈[x-1,x] 的双格命中）；
	// 右格越出最右列时由 InsertCellChain 边界兜底弃挂（2026-10-06 修复：
	// 此处曾可写越界格损坏相邻链）
	if (PlantCoversTwoCols(thePlant))
	{
		InsertCellChain(thePlant->mRow, thePlant->mPlantCol + 1, aLayer, aSlot);
	}
}

// 注销 = 注册的镜像（2026-10-06 抽出）：锚格+玉米炮右格对称摘除，
// 死亡/铲除/移动三路共用——消灭三处手抄重复（重复是漏摘/不对称的温床）
void Board::UnregisterPlantCellBucket(Plant* thePlant, int theRow, int theOldCol)
{
	if (thePlant == nullptr)
		return;
	const unsigned int aSlot = PlantSlotOf(thePlant);
	const int aLayer = ResolvePlantCellLayer(ResolvePlantSeedType(thePlant), mApp->mGameMode);
	RemoveCellChain(theRow, theOldCol, aLayer, aSlot);
	if (PlantCoversTwoCols(thePlant))
	{
		RemoveCellChain(theRow, theOldCol + 1, aLayer, aSlot);
	}
}

void Board::MovePlantCellBucket(Plant* thePlant, int theOldRow, int theOldCol)
{
	// 移动点专用（坐标字段已改写后调用）：层归属不随移动变，先从旧格
	// 链摘除（含旧右格），再注册进新格
	UnregisterPlantCellBucket(thePlant, theOldRow, theOldCol);
	RegisterPlantCellBucket(thePlant);
}

void Board::RebuildPlantCellBucket()
{
	ThePlantCellBucket().Clear();
	Plant* aPlant = nullptr;
	while (IteratePlants(aPlant))
	{
		RegisterPlantCellBucket(aPlant);
	}
}

// ── 僵尸行段桶读写口（2026-10-04 用户方案）──
Zombie* Board::ZombieAtSlot(unsigned int theSlot)
{
	if (theSlot >= (unsigned)mZombies.mMaxSize)
		return nullptr;
	return &mZombies.mBlock[theSlot].mItem;
}

unsigned int Board::ZombieSlotOf(const Zombie* theZombie)
{
	return mZombies.DataArrayIndexOf(theZombie);
}

// 迭代序 = 行段 ∪ Boss 侧数组按槽位**归并**（两段各自升序，Boss 通常 ≤1，
// 归并成本可忽略）= 原全池扫 "Boss||row==theRow" 过滤集的升序全序——
// 候选集与评估序都与池扫逐字一致，读侧无需任何决胜重排，连大嘴花/土豆雷
// 攻击矩形随评估累积位移的原版时序都原样保留。
bool Board::IterateZombiesInRow(int theRow, Zombie*& theZombie)
{
	if ((unsigned)theRow >= MAX_GRID_SIZE_Y)
	{
		theZombie = nullptr;
		return false;
	}
	ZombieRowBucket& aBucket = TheZombieRowBucket();
	uint16_t aRowIdx = 0;
	uint16_t aBossIdx = 0;
	if (theZombie != nullptr)
	{
		// 续走协议：归并升序输出 ⇒ 两侧"已消费" = 各自槽位小于前槽的元素。
		// 前任所在侧 O(1)（反向指针+1）；对侧线性推进——行成员续走推 Boss 侧
		// （实践 0-1 只），Boss 续走推行侧（每 Boss 每查询一次，~行内只数）
		const unsigned aPrevSlot = ZombieSlotOf(theZombie);
		if ((ZombieFlagsOf(theZombie->mZombieType) & ZombieFlags::Boss) != 0)
		{
			aBossIdx = (uint16_t)(aBucket.mSlotIdx[aPrevSlot] + 1);
			while (aRowIdx < aBucket.mRowCount[theRow] && aBucket.mRows[theRow][aRowIdx] < aPrevSlot)
			{
				aRowIdx++;
			}
		}
		else
		{
			aRowIdx = (uint16_t)(aBucket.mSlotIdx[aPrevSlot] + 1);
			while (aBossIdx < aBucket.mBossCount && aBucket.mBosses[aBossIdx] < aPrevSlot)
			{
				aBossIdx++;
			}
		}
	}
	for (;;)
	{
		const uint16_t aRowSlot = aRowIdx < aBucket.mRowCount[theRow] ? aBucket.mRows[theRow][aRowIdx] : ZOMBIE_ROWSEG_NONE;
		const uint16_t aBossSlot = aBossIdx < aBucket.mBossCount ? aBucket.mBosses[aBossIdx] : ZOMBIE_ROWSEG_NONE;
		uint16_t aPick;
		if (aRowSlot == ZOMBIE_ROWSEG_NONE && aBossSlot == ZOMBIE_ROWSEG_NONE)
		{
			theZombie = nullptr;
			return false;
		}
		else if (aBossSlot == ZOMBIE_ROWSEG_NONE || (aRowSlot != ZOMBIE_ROWSEG_NONE && aRowSlot < aBossSlot))
		{
			aPick = aRowSlot;
			aRowIdx++;
		}
		else
		{
			aPick = aBossSlot;
			aBossIdx++;
		}
		theZombie = ZombieAtSlot(aPick);
		if (theZombie != nullptr)
			return true;
		// 防御：槽位越界/空槽跳过（纪律下不可达）
	}
}


// 纯行段迭代（不含侧 Boss；Boss 不在行段内，天然不吐）：供"主动排除
// Boss"的裸 row== 查询（2026-10-04 用户裁决：割草机/钉耙/土豆灯删 Boss
// 检测——割草机与钉耙不能击杀/伤害僵王、土豆灯检测僵王意义不明；主动
// 偏离 1073。魅惑近战 FindZombieTarget 保留 Boss@真实行=原版行为）。
// 段槽位升序=池扫过滤后的序。
bool Board::IterateZombiesInRowOnly(int theRow, Zombie*& theZombie)
{
	if ((unsigned)theRow >= MAX_GRID_SIZE_Y)
	{
		theZombie = nullptr;
		return false;
	}
	ZombieRowBucket& aBucket = TheZombieRowBucket();
	const uint16_t aIdx = theZombie == nullptr
		? 0
		: (uint16_t)(aBucket.mSlotIdx[ZombieSlotOf(theZombie)] + 1);
	if (aIdx >= aBucket.mRowCount[theRow])
	{
		theZombie = nullptr;
		return false;
	}
	theZombie = ZombieAtSlot(aBucket.mRows[theRow][aIdx]);
	return theZombie != nullptr;
}

// 多行区间迭代（行主序：rLo..rHi 各行段依次走完，尾吐全部侧 Boss）。
// 统一"候选超集"纪律（2026-10-04 用户裁决否决双成员后定型）：迭代器吐
// 行区间∪全部 Boss，行过滤交给循环体原样保留的检查——"±k 行+Boss 行
// 偏差归零"类体内的豁免让 Boss 通过，无豁免类（磁力菇）体内的真实
// diffY 门按真实行滤掉 Boss，皆与 1073 池扫逐字同构。行主序≠池扫全序：
// 布尔/apply-all 查询无差；选择型查询配槽位决胜（平局取小槽=池扫严格
// 比较保留先见者）。
bool Board::IterateZombiesInRowRange(int theRowLo, int theRowHi, Zombie*& theZombie)
{
	if (theRowLo > theRowHi)
	{
		theZombie = nullptr;
		return false;
	}
	const int aLo = std::max(theRowLo, 0);
	const int aHi = std::min(theRowHi, MAX_GRID_SIZE_Y - 1);
	ZombieRowBucket& aBucket = TheZombieRowBucket();
	uint16_t aBossIdx = 0;
	int aRow = aLo;
	uint16_t aIdx = 0;
	if (theZombie != nullptr)
	{
		const unsigned aPrevSlot = ZombieSlotOf(theZombie);
		if ((ZombieFlagsOf(theZombie->mZombieType) & ZombieFlags::Boss) != 0)
		{
			aBossIdx = (uint16_t)(aBucket.mSlotIdx[aPrevSlot] + 1);
			aRow = aHi + 1;  // 前任是侧 Boss，已处尾段
		}
		else
		{
			aRow = theZombie->mRow;
			aIdx = (uint16_t)(aBucket.mSlotIdx[aPrevSlot] + 1);
		}
	}
	for (; aRow <= aHi; aRow++)
	{
		if (aIdx >= aBucket.mRowCount[aRow])
		{
			aIdx = 0;
			continue;
		}
		theZombie = ZombieAtSlot(aBucket.mRows[aRow][aIdx]);
		if (theZombie != nullptr)
			return true;
		aIdx = 0;
	}
	while (aBossIdx < aBucket.mBossCount)
	{
		theZombie = ZombieAtSlot(aBucket.mBosses[aBossIdx]);
		aBossIdx++;
		if (theZombie != nullptr)
			return true;
	}
	theZombie = nullptr;
	return false;
}

void Board::RegisterZombieRowBucket(Zombie* theZombie)
{
	if (theZombie == nullptr)
		return;
	const unsigned aSlot = ZombieSlotOf(theZombie);
	if (aSlot >= ZOMBIE_POOL_CAPACITY)
		return;
	ZombieRowBucket& aBucket = TheZombieRowBucket();
	if (aBucket.mSlotIdx[aSlot] != ZOMBIE_ROWSEG_NONE)
		return;  // 幂等（Rebuild 与初始化尾重叠场景）
	if ((ZombieFlagsOf(theZombie->mZombieType) & ZombieFlags::Boss) != 0)
	{
		RowSegInsert(aBucket.mBosses, aBucket.mBossCount, (uint16_t)aSlot, aBucket.mSlotIdx);
	}
	else if ((unsigned)theZombie->mRow < MAX_GRID_SIZE_Y)
	{
		RowSegInsert(aBucket.mRows[theZombie->mRow], aBucket.mRowCount[theZombie->mRow], (uint16_t)aSlot, aBucket.mSlotIdx);
	}
	// 越界行不入场（非 Boss）：原池扫 row==查询行 亦不可见，等价
}

void Board::RemoveZombieRowBucket(Zombie* theZombie)
{
	if (theZombie == nullptr)
		return;
	const unsigned aSlot = ZombieSlotOf(theZombie);
	if (aSlot >= ZOMBIE_POOL_CAPACITY)
		return;
	ZombieRowBucket& aBucket = TheZombieRowBucket();
	if (aBucket.mSlotIdx[aSlot] == ZOMBIE_ROWSEG_NONE)
		return;
	if ((ZombieFlagsOf(theZombie->mZombieType) & ZombieFlags::Boss) != 0)
	{
		RowSegRemove(aBucket.mBosses, aBucket.mBossCount, (uint16_t)aSlot, aBucket.mSlotIdx);
	}
	else
	{
		TOD_ASSERT((unsigned)theZombie->mRow < MAX_GRID_SIZE_Y);
		RowSegRemove(aBucket.mRows[theZombie->mRow], aBucket.mRowCount[theZombie->mRow], (uint16_t)aSlot, aBucket.mSlotIdx);
	}
}

void Board::MoveZombieRowBucket(Zombie* theZombie, int theOldRow)
{
	if (theZombie == nullptr)
		return;
	const unsigned aSlot = ZombieSlotOf(theZombie);
	if (aSlot >= ZOMBIE_POOL_CAPACITY)
		return;
	ZombieRowBucket& aBucket = TheZombieRowBucket();
	if ((ZombieFlagsOf(theZombie->mZombieType) & ZombieFlags::Boss) != 0)
		return;  // Boss 只在侧数组，与行无关
	if (aBucket.mSlotIdx[aSlot] != ZOMBIE_ROWSEG_NONE)
	{
		TOD_ASSERT((unsigned)theOldRow < MAX_GRID_SIZE_Y);
		RowSegRemove(aBucket.mRows[theOldRow], aBucket.mRowCount[theOldRow], (uint16_t)aSlot, aBucket.mSlotIdx);
	}
	// 未注册（越界行折返/异常路径）时此处即首次入场
	if ((unsigned)theZombie->mRow < MAX_GRID_SIZE_Y)
	{
		RowSegInsert(aBucket.mRows[theZombie->mRow], aBucket.mRowCount[theZombie->mRow], (uint16_t)aSlot, aBucket.mSlotIdx);
	}
}

void Board::RebuildZombieRowBucket()
{
	TheZombieRowBucket().Clear();
	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		RegisterZombieRowBucket(aZombie);
	}
}

Plant* Board::GetTopPlantAt(int theGridX, int theGridY, PlantPriority thePriority)
{
	if (theGridX < 0 || theGridX >= MAX_GRID_SIZE_X || theGridY < 0 || theGridY >= MAX_GRID_SIZE_Y)
		return nullptr;

	if (mApp->IsWallnutBowlingLevel() && !mCutScene->IsInShovelTutorial())
		return nullptr;

	PlantsOnLawn aPlantOnLawn;
	GetPlantsOnLawn(theGridX, theGridY, &aPlantOnLawn);

	// 层序表：每个优先级 = 4 个植物层的选取顺序（层号经成员指针直取）。
	// 层号（2026-10-03 桶特化序）：0=底座 1=普通 2=南瓜壳 3=飞行；
	// -1 = 序列终止（短于 4 层的优先级）。
	static constexpr Plant* PlantsOnLawn::* kPlantLayers[] = {
		&PlantsOnLawn::mUnderPlant,                                                                    // 0 底座
		&PlantsOnLawn::mNormalPlant,                                                                  // 1 普通
		&PlantsOnLawn::mPumpkinPlant,                                                                 // 2 南瓜壳
		&PlantsOnLawn::mFlyingPlant,                                                                  // 3 飞行
	};
	static constexpr int8_t kTopPlantLayerOrder[][4] = {
		/* TOPPLANT_EATING_ORDER         */ { 2, 1, 0, -1 },  // 南瓜壳→普通→底座
		/* TOPPLANT_DIGGING_ORDER        */ { 1, 0, -1, -1 }, // 普通→底座
		/* TOPPLANT_BUNGEE_ORDER         */ { 3, 1, 2, 0 },   // 飞行→普通→南瓜壳→底座
		/* TOPPLANT_CATAPULT_ORDER       */ { 3, 1, 2, 0 },
		/* TOPPLANT_ZEN_TOOL_ORDER       */ { 3, 2, 1, 0 },   // 飞行→南瓜壳→普通→底座
		/* TOPPLANT_ANY                  */ { 3, 1, 2, 0 },
		/* TOPPLANT_ONLY_NORMAL_POSITION */ { 1, -1, -1, -1 },
		/* TOPPLANT_ONLY_FLYING          */ { 3, -1, -1, -1 },
		/* TOPPLANT_ONLY_PUMPKIN         */ { 2, -1, -1, -1 },
		/* TOPPLANT_ONLY_UNDER_PLANT     */ { 0, -1, -1, -1 },
	};
	static_assert(sizeof(kTopPlantLayerOrder) / sizeof(kTopPlantLayerOrder[0]) == PlantPriority::TOPPLANT_ONLY_UNDER_PLANT + 1,
		"层序槽位须与 PlantPriority 枚举同步");
	if ((unsigned)thePriority < sizeof(kTopPlantLayerOrder) / sizeof(kTopPlantLayerOrder[0]))
	{
		for (int8_t aLayer : kTopPlantLayerOrder[thePriority])
		{
			if (aLayer < 0)
				break;
			Plant* aPlant = aPlantOnLawn.*kPlantLayers[aLayer];
			if (aPlant)
				return aPlant;
		}
		return nullptr;
	}
	TOD_ASSERT(false);
	return nullptr;
}

//0x40D3A0
int Board::CountSunFlowers()
{
	int aCount = 0;
	Plant* aPlant = nullptr;
	while (IteratePlants(aPlant))
	{
		if (aPlant->MakesSun())
		{
			aCount++;
		}
	}
	return aCount;
}

//0x40D3F0
int Board::CountPlantByType(SeedType theSeedType)
{
	int aCount = 0;
	Plant* aPlant = nullptr;
	while (IteratePlants(aPlant))
	{
		if (aPlant->mSeedType == theSeedType)
		{
			aCount++;
		}
	}
	return aCount;
}

// 基座检查：紫卡（升级种子段 40..47）要求基础植物在场，表内 lambda 各自烘焙检查；
// 非紫卡短路直返 true（等价原版 switch default，零查表零调用）。阳光
// （CanTakeSunMoney）与冷却（SeedPacket::mActive）是平级前置，地面规则在
// CanPlantAt——均不在此。常量参数调用会被编译器折叠为直呼。
// 头文件不留实现：lambda/函数指针表须单一副本（见 TodCommon.cpp 曲线事故）。
bool Board::Met(SeedType theSeedType)
{
	if (theSeedType < SeedType::SEED_GATLINGPEA || theSeedType > SeedType::SEED_COBCANNON)
	{
		return true;
	}
	static constexpr bool (*kUpgradeBasisReqs[])(Board*) = {
		/* SEED_GATLINGPEA    */ [](Board* aBoard) { return aBoard->CountPlantByType(SeedType::SEED_REPEATER) != 0; },
		/* SEED_TWINSUNFLOWER */ [](Board* aBoard) { return aBoard->CountPlantByType(SeedType::SEED_SUNFLOWER) != 0; },
		/* SEED_GLOOMSHROOM   */ [](Board* aBoard) { return aBoard->CountPlantByType(SeedType::SEED_FUMESHROOM) != 0; },
		/* SEED_CATTAIL       */ [](Board* aBoard) { return aBoard->CountEmptyPotsOrLilies(SeedType::SEED_LILYPAD) != 0; },
		/* SEED_WINTERMELON   */ [](Board* aBoard) { return aBoard->CountPlantByType(SeedType::SEED_MELONPULT) != 0; },
		/* SEED_GOLD_MAGNET   */ [](Board* aBoard) { return aBoard->CountPlantByType(SeedType::SEED_MAGNETSHROOM) != 0; },
		/* SEED_SPIKEROCK     */ [](Board* aBoard) { return aBoard->CountPlantByType(SeedType::SEED_SPIKEWEED) != 0; },
		/* SEED_COBCANNON     */ [](Board* aBoard) { return aBoard->HasValidCobCannonSpot(); },
	};
	static_assert(sizeof(kUpgradeBasisReqs) / sizeof(kUpgradeBasisReqs[0])
		== (int)SeedType::SEED_COBCANNON - (int)SeedType::SEED_GATLINGPEA + 1,
		"基座检查槽位须与 SeedType 升级段同步");
	return kUpgradeBasisReqs[(int)theSeedType - (int)SeedType::SEED_GATLINGPEA](this);
}

//0x40D430
int Board::CountEmptyPotsOrLilies(SeedType theSeedType)
{
	int aCount = 0;
	Plant* aPlant = nullptr;
	while (IteratePlants(aPlant))
	{
		if (aPlant->mSeedType == theSeedType && !GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION))
		{
			aCount++;
		}
	}
	return aCount;
}

//0x40D4F0
bool Board::IsValidCobCannonSpotHelper(int theGridX, int theGridY)
{
	PlantsOnLawn aPlantOnLawn;
	GetPlantsOnLawn(theGridX, theGridY, &aPlantOnLawn);
	if (aPlantOnLawn.mPumpkinPlant)
		return false;

	if (aPlantOnLawn.mNormalPlant && aPlantOnLawn.mNormalPlant->mSeedType == SeedType::SEED_KERNELPULT)
		return true;

	return mApp->mEasyPlantingCheat && CanPlantAt(theGridX, theGridY, SeedType::SEED_KERNELPULT) == PlantingReason::PLANTING_OK;
}

//0x40D550
bool Board::IsValidCobCannonSpot(int theGridX, int theGridY)
{
	if (!IsValidCobCannonSpotHelper(theGridX, theGridY) || !IsValidCobCannonSpotHelper(theGridX + 1, theGridY))
		return false;

	return !GetFlowerPotAt(theGridX, theGridY) == !GetFlowerPotAt(theGridX + 1, theGridY);
}

//0x40D5C0
bool Board::HasValidCobCannonSpot()
{
	Plant* aPlant = nullptr;
	while (IteratePlants(aPlant))
	{
		if (aPlant->mSeedType == SeedType::SEED_KERNELPULT && IsValidCobCannonSpot(aPlant->mPlantCol, aPlant->mRow))
		{
			return true;
		}
	}
	return false;
}

//0x40D620
Projectile* Board::AddProjectile(int theX, int theY, int theRenderOrder, int theRow, ProjectileType theProjectileType)
{
	Projectile* aProjectile = mProjectiles.DataArrayAlloc();
	aProjectile->ProjectileInitialize(theX, theY, theRenderOrder, theRow, theProjectileType);
	return aProjectile;
}

//0x40D660
bool Board::CanZombieSpawnOnLevel(ZombieType theZombieType, int theLevel)
{
	const ZombieDefinition& aZombieDef = GetZombieDefinition(theZombieType);
	if (theZombieType == ZombieType::ZOMBIE_YETI)
	{
		return gLawnApp->CanSpawnYetis();
	}

	if (theLevel < aZombieDef.mStartingLevel || aZombieDef.mPickWeight == 0)
	{
		return false;
	}

	TOD_ASSERT(gZombieAllowedLevels[theZombieType].mZombieType == theZombieType);
	return gZombieAllowedLevels[theZombieType].mAllowedOnLevel[ClampInt(theLevel - 1, 0, 49)];
}

//0x40D6F0
ZombieType Board::GetIntroducedZombieType()
{
	if (!mApp->IsAdventureMode() || mLevel == 1)
	{
		return ZombieType::ZOMBIE_INVALID;
	}

	for (ZombieType aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType = (ZombieType)((int)aZombieType + 1))
	{
		const ZombieDefinition& aZombieDef = GetZombieDefinition(aZombieType);
		if ((aZombieType != ZombieType::ZOMBIE_YETI || mApp->CanSpawnYetis()) && aZombieDef.mStartingLevel == mLevel)
		{
			return aZombieType;
		}
	}
	return ZombieType::ZOMBIE_INVALID;
}

//0x40D770
ZombieType Board::PickGraveRisingZombieType()
{
	TodWeightedArray aZombieWeightArray[(int)ZombieType::NUM_ZOMBIE_TYPES];
	int aCount = 2;
	aZombieWeightArray[0].mItem = ZombieType::ZOMBIE_NORMAL;
	aZombieWeightArray[0].mWeight = GetZombieDefinition(ZombieType::ZOMBIE_NORMAL).mPickWeight;
	aZombieWeightArray[1].mItem = ZombieType::ZOMBIE_TRAFFIC_CONE;
	aZombieWeightArray[1].mWeight = GetZombieDefinition(ZombieType::ZOMBIE_TRAFFIC_CONE).mPickWeight;
	if (!StageHasGraveStones())
	{
		aZombieWeightArray[2].mItem = ZombieType::ZOMBIE_PAIL;
		aZombieWeightArray[2].mWeight = GetZombieDefinition(ZombieType::ZOMBIE_PAIL).mPickWeight;
		aCount++;
	}

	for (int i = 0; i < aCount; i++)
	{
		ZombieType aZombieType = (ZombieType)aZombieWeightArray[i].mItem;
		const ZombieDefinition& aZombieDef = GetZombieDefinition(aZombieType);
		if ((mApp->IsFirstTimeAdventureMode() && mLevel < aZombieDef.mStartingLevel) || (!mZombieAllowed[aZombieType] && aZombieType != ZombieType::ZOMBIE_NORMAL))
		{
			aZombieWeightArray[i].mWeight = 0;
		}
	}

	return (ZombieType)TodPickFromWeightedArray(aZombieWeightArray, aCount);
}

//0x40D8A0
ZombieType Board::PickZombieType(int theZombiePoints, int theWaveIndex, ZombiePicker* theZombiePicker)
{
	int aPickCount = 0;
	TodWeightedArray aZombieWeightArray[ZombieType::NUM_ZOMBIE_TYPES];
	for (int aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType++)
	{
		if (!mZombieAllowed[aZombieType])
			continue;

		const ZombieDefinition& aZombieDef = GetZombieDefinition((ZombieType)aZombieType);

		// ================================================================================================
		// ▲ 将不符合出怪限制或超出剩余点数的僵尸类型排除
		// ================================================================================================
		GameMode aGameMode = mApp->mGameMode;
		// 蹦极僵尸在无尽模式中仅在旗帜波出现
		if (aZombieType == ZombieType::ZOMBIE_BUNGEE && mApp->IsSurvivalEndless(aGameMode))
		{
			if (!IsFlagWave(theWaveIndex))
			{
				continue;
			}
		}
		// 僵尸最早出现的波数的限制（出怪限制）
		else if (aGameMode != GameMode::GAMEMODE_MINIGAME_POGO_PARTY && aGameMode != GameMode::GAMEMODE_MINIGAME_BOBSLED_BONANZA && aGameMode != GameMode::GAMEMODE_LIMBO_AIR_RAID)
		{
			int aFirstAllowedWave = aZombieDef.mFirstAllowedWave;
			// 无尽模式中，僵尸最早可出现的波数逐渐前移
			if (mApp->IsSurvivalEndless(aGameMode))
			{
				int aFlags = GetSurvivalFlagsCompleted();
				int aAllowedWave = aFirstAllowedWave - TodAnimateCurve(18, 50, aFlags, 0, 15, Curves::Linear);
				aFirstAllowedWave = std::max(aAllowedWave, 1);
			}
			if (theWaveIndex + 1 < aFirstAllowedWave || theZombiePoints < aZombieDef.mZombieValue)
			{
				continue;
			}
		}

		// ================================================================================================
		// ▲ 生存模式中，根据当前旗帜数等重新计算僵尸的权重
		// ================================================================================================
		int aPickWeight = aZombieDef.mPickWeight;
		if (mApp->IsSurvivalMode())
		{
			int aFlags = GetSurvivalFlagsCompleted();
			// 伽刚特尔和雪橇车僵尸的每波出怪上限
			if (aZombieType == ZombieType::ZOMBIE_GARGANTUAR || aZombieType == ZombieType::ZOMBIE_ZAMBONI)
			{
				if (theZombiePicker->mZombieTypeCount[aZombieType] >= TodAnimateCurve(10, 50, aFlags, 2, 50, Curves::Linear))
				{
					continue;
				}
			}
			// 红眼的旗帜波出怪上限和非旗帜波出怪总和上限
			else if (aZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
			{
				if (IsFlagWave(theWaveIndex))
				{
					if (theZombiePicker->mZombieTypeCount[aZombieType] >= TodAnimateCurve(14, 100, aFlags, 1, 50, Curves::Linear))
					{
						continue;
					}
				}
				else
				{
					if (theZombiePicker->mAllWavesZombieTypeCount[aZombieType] >= TodAnimateCurve(10, 110, aFlags, 1, 50, Curves::Linear))
					{
						continue;
					}
					aPickWeight = 1000;
				}
			}
			// 普通僵尸和路障僵尸的权重衰减
			else if (aZombieType == ZombieType::ZOMBIE_NORMAL)
			{
				aPickWeight = TodAnimateCurve(10, 50, aFlags, aPickWeight, aPickWeight / 10, Curves::Linear);
			}
			else if (aZombieType == ZombieType::ZOMBIE_TRAFFIC_CONE)
			{
				aPickWeight = TodAnimateCurve(10, 50, aFlags, aPickWeight, aPickWeight / 4, Curves::Linear);
			}
		}
		aZombieWeightArray[aPickCount].mItem = aZombieType;
		aZombieWeightArray[aPickCount].mWeight = aPickWeight;
		aPickCount++;
	}

	// 加权随机地取得一种可能的僵尸类型并返回
	return (ZombieType)TodPickFromWeightedArray(aZombieWeightArray, aPickCount);
}

bool Board::IsZombieTypePoolOnly(ZombieType theZombieType)
{
	return (theZombieType == ZombieType::ZOMBIE_SNORKEL || theZombieType == ZombieType::ZOMBIE_DOLPHIN_RIDER);
}

//0x40DB20
bool Board::RowCanHaveZombieType(int theRow, ZombieType theZombieType)
{
	if (!RowCanHaveZombies(theRow))
	{
		return false;
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_RESODDED && mPlantRow[theRow] == PlantRowType::PLANTROW_DIRT && mCurrentWave < 5)
	{
		return false;  // 无草皮之地关卡，无草皮的行在前 5 波不刷出僵尸
	}
	if (mPlantRow[theRow] == PlantRowType::PLANTROW_POOL && !Zombie::ZombieTypeCanGoInPool(theZombieType) && theZombieType != ZombieType::ZOMBIE_BALLOON)
	{
		return false;  // 水路不会刷出不能进入泳池的僵尸
	}
	if (mPlantRow[theRow] == PlantRowType::PLANTROW_HIGH_GROUND && !Zombie::ZombieTypeCanGoOnHighGround(theZombieType))
	{
		return false;  // 高地不会刷出不能走上高地的僵尸
	}

	int aCurrentWave = mCurrentWave;
	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND)
	{
		aCurrentWave += mChallenge->mSurvivalStage * GetNumWavesPerSurvivalStage();
	}
	// 非水路不能刷出水路僵尸；前 5 小波，水面仅刷出潜水僵尸或海豚骑士僵尸
	if (mPlantRow[theRow] == PlantRowType::PLANTROW_POOL)
	{
		if (aCurrentWave < 5 && !IsZombieTypePoolOnly(theZombieType))
		{
			return false;
		}
	}
	else if (IsZombieTypePoolOnly(theZombieType))
	{
		return false;
	}
	// 雪橇僵尸小队仅能在有冰道的行刷出
	if (theZombieType == ZOMBIE_BOBSLED && !mIceTimer[theRow])
	{
		return false;
	}
	// “自古一路无巨人”（生存模式除外）
	if (theRow == 0 && !mApp->IsSurvivalMode())
	{
		if (theZombieType == ZombieType::ZOMBIE_GARGANTUAR || theZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
		{
			return false;
		}
	}
	// 非舞王僵尸或当前为泳池关卡，则可以刷出该僵尸
	if (theZombieType != ZombieType::ZOMBIE_DANCER || StageHasPool())
	{
		return true;
	}
	// 舞王僵尸在非泳池关卡中，为保证能召唤伴舞僵尸，仅在中间三行刷出
	return RowCanHaveZombies(theRow - 1) && RowCanHaveZombies(theRow + 1);
}

//0x40DC50
int Board::PickRowForNewZombie(ZombieType theZombieType)
{
	// ====================================================================================================
	// ▲ 当存在正在寻找目标僵尸的钉耙，且僵尸可以出现在钉耙所在行时，优先出现在钉耙所在行
	// ====================================================================================================
	GridItem* aRake = GetRake();
	if (aRake && aRake->mGridItemState == GridItemState::GRIDITEM_STATE_RAKE_ATTRACTING && RowCanHaveZombieType(aRake->mGridY, theZombieType))
	{
		aRake->mGridItemState = GridItemState::GRIDITEM_STATE_RAKE_WAITING;
		TodUpdateSmoothArrayPick(mRowPickingArray, MAX_GRID_SIZE_Y, aRake->mGridY);
		return aRake->mGridY;
	}

	// ====================================================================================================
	// ▲ 遍历每一行，将所有能允许该僵尸出现的行及其对应权重写入挑选数组中
	// ====================================================================================================
	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		// 如果本行不能出现目标僵尸，则将本行权重置零，并继续下一行
		if (!RowCanHaveZombieType(aRow, theZombieType))
		{
			mRowPickingArray[aRow].mWeight = 0;
		}
		// 保护传送门关卡中，每行的出怪概率受传送门位置影响
		else if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_PORTAL_COMBAT)
		{
			mRowPickingArray[aRow].mWeight = mChallenge->PortalCombatRowSpawnWeight(aRow);
		}
		// 隐形食脑者关卡中，前 3 波第六路不出怪
		else if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_INVISIGHOUL && mCurrentWave <= 3 && aRow == 5)
		{
			mRowPickingArray[aRow].mWeight = 0;
		}
		// 丢车保护
		else
		{
			int aWavesMowered = mCurrentWave - mWaveRowGotLawnMowered[aRow];
			if (mApp->IsContinuousChallenge() && mCurrentWave == mNumWaves - 1)
			{
				aWavesMowered = 100;
			}

			if (aWavesMowered <= 1)
			{
				mRowPickingArray[aRow].mWeight = 0.01f;
			}
			else if (aWavesMowered <= 2)
			{
				mRowPickingArray[aRow].mWeight = 0.5f;
			}
			else
			{
				mRowPickingArray[aRow].mWeight = 1.0f;
			}
		}
	}
	return TodPickFromSmoothArray(mRowPickingArray, MAX_GRID_SIZE_Y);
}

//0x40DD90
bool Board::CanAddBobSled()
{
	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		if (mIceTimer[aRow] > 0 && mIceMinX[aRow] < 700)
		{
			return true;
		}
	}
	return false;
}

//0x40DDC0
// GOTY @Patoke: 0x410700
Zombie* Board::AddZombieInRow(ZombieType theZombieType, int theRow, int theFromWave)
{
	if (mZombies.mSize >= mZombies.mMaxSize - 1)
	{
		TodTrace("Too many zombies!!");
		return nullptr;
	}

	// @Patoke: implemented
	if (theZombieType == ZombieType::ZOMBIE_YETI) {
		if (mApp->IsAdventureMode() && mLevel == 40 && theFromWave >= 0)
			ReportAchievement::GiveAchievement(mApp, Zombologist, true);
	}

	bool aVariant = !Rand(5);
	const auto visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	const bool preview = theFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE || theFromWave == Zombie::ZOMBIE_WAVE_UI;
	const auto ownerNowNs = preview ? visualNowNs : static_cast<std::int64_t>(mLevelTimeNs);
	const auto domain = preview ? ReanimTimeDomain::Visual : AnimationTimeDomain();
	const auto animationNowNs = domain == ReanimTimeDomain::Battle ? ownerNowNs : visualNowNs;
	auto& holder = *mApp->mEffectSystem->mReanimationHolder;
	const auto bindBirth = [&](Zombie& zombie) {
		for (const auto id : {zombie.mBodyReanimID, zombie.mSpecialHeadReanimID, zombie.mZombatarHeadReanimID,
			zombie.mMoweredReanimID, zombie.mBossFireBallReanimID})
			if (auto* animation = mApp->ReanimationTryToGet(id))
				holder.BindAnimationTime(*animation, animation->mIsAttachment ? domain : ReanimTimeDomain::Visual,
					animation->mIsAttachment ? animationNowNs : visualNowNs);
		if (auto* attachment = mApp->mEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet(
			static_cast<unsigned int>(zombie.mAttachmentID)))
			for (int i = 0; i < attachment->mNumEffects; ++i)
				if (attachment->mEffectArray[i].mEffectType == EffectType::EFFECT_REANIM)
					if (auto* animation = holder.mReanimations.DataArrayTryToGet(attachment->mEffectArray[i].mEffectID))
						holder.BindAnimationTime(*animation, preview ? ReanimTimeDomain::Visual : ReanimTimeDomain::Battle, ownerNowNs);
	};
	Zombie* aZombie = mZombies.DataArrayAlloc();
	aZombie->ZombieInitialize(ownerNowNs, visualNowNs, animationNowNs, theRow, theZombieType, aVariant, nullptr, theFromWave);
	bindBirth(*aZombie);
	// 行段桶入场（2026-10-04 用户方案）：注册在池分配漏斗（池成员资格有
	// 保证——烟测夹具的栈上僵尸直调 ZombieInitialize 不入桶；存档整池
	// 镜像装载亦不走此，由装载侧 RebuildZombieRowBucket 兜底）
	RegisterZombieRowBucket(aZombie);
	if (theZombieType == ZombieType::ZOMBIE_BOBSLED && aZombie->IsOnBoard())
	{
		for (int _i = 0; _i < 3; _i++)
		{
			Zombie* aFollower = mZombies.DataArrayAlloc();
			aFollower->ZombieInitialize(ownerNowNs, visualNowNs, animationNowNs, theRow, ZombieType::ZOMBIE_BOBSLED, false, aZombie, theFromWave);
			bindBirth(*aFollower);
			RegisterZombieRowBucket(aFollower);
		}
	}
	return aZombie;
}

Zombie* Board::AddZombie(ZombieType theZombieType, int theFromWave)
{
	return AddZombieInRow(theZombieType, PickRowForNewZombie(theZombieType), theFromWave);
}

//0x40DEA0
void Board::RemoveAllZombies()
{
	const auto visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		if (!aZombie->IsDeadOrDying())
		{
			const bool onBoard = aZombie->IsOnBoard();
			aZombie->DieNoLoot(onBoard ? static_cast<std::int64_t>(mLevelTimeNs) : visualNowNs,
				visualNowNs, onBoard && AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : visualNowNs);
		}
	}
}

//0x40DF00
void Board::RemoveZombiesForRepick()
{
	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		if (!aZombie->IsDeadOrDying() && aZombie->mMindControlled && (aZombie->mX) > 720)
		{
			aZombie->DieNoLoot(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
		}
	}
}

//0x40DF70
void Board::RemoveCutsceneZombies()
{
	const auto visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		if (aZombie->mFromWave == Zombie::ZOMBIE_WAVE_CUTSCENE)
		{
			aZombie->DieNoLoot(visualNowNs, visualNowNs, visualNowNs);
		}
	}
}

//0x40DFC0
bool Board::IsIceAt(int theGridX, int theGridY)
{
	TOD_ASSERT(theGridY >= 0 && theGridY < MAX_GRID_SIZE_Y);
	if (mIceTimer[theGridY] == 0 || mIceMinX[theGridY] > 750)
		return false;

	return theGridX >= PixelToGridXKeepOnBoard(mIceMinX[theGridY] + 12, 0);
}

//0x40E020
PlantingReason Board::CanPlantAt(int theGridX, int theGridY, SeedType theSeedType)
{
	// 目标位置不在场地内，则返回“不能种在那里”
	if (theGridX < 0 || theGridX >= MAX_GRID_SIZE_X || theGridY < 0 || theGridY >= MAX_GRID_SIZE_Y)
	{
		return PlantingReason::PLANTING_NOT_HERE;
	}

	// 从关卡玩法上，判断能否种植
	PlantingReason aReason = mChallenge->CanPlantAt(theGridX, theGridY, theSeedType);
	if (aReason != PlantingReason::PLANTING_OK || Challenge::IsZombieSeedType(theSeedType))
	{
		return aReason;
	}

	PlantsOnLawn aPlantOnLawn;
	GetPlantsOnLawn(theGridX, theGridY, &aPlantOnLawn);
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		if (aPlantOnLawn.mUnderPlant || aPlantOnLawn.mPumpkinPlant || aPlantOnLawn.mFlyingPlant || aPlantOnLawn.mNormalPlant)
		{
			return PlantingReason::PLANTING_NOT_HERE;
		}
		if (mApp->mZenGarden->mGardenType == GARDEN_AQUARIUM && !Plant::IsAquatic(theSeedType))
		{
			return PlantingReason::PLANTING_NOT_ON_WATER;
		}

		return PlantingReason::PLANTING_OK;
	}

	// 墓碑吞噬者只能种植在墓碑上
	bool aHasGrave = GetGraveStoneAt(theGridX, theGridY);
	if (theSeedType == SeedType::SEED_GRAVEBUSTER)
	{
		if (aPlantOnLawn.mNormalPlant)
		{
			return PlantingReason::PLANTING_NOT_HERE;
		}

		return aHasGrave ? PlantingReason::PLANTING_OK : PlantingReason::PLANTING_ONLY_ON_GRAVES;
	}
	if (theSeedType == SeedType::SEED_INSTANT_COFFEE)
	{
		if (aPlantOnLawn.mFlyingPlant)
		{
			return PlantingReason::PLANTING_NOT_HERE;
		}

		if (!aPlantOnLawn.mNormalPlant || !aPlantOnLawn.mNormalPlant->mIsAsleep || aPlantOnLawn.mNormalPlant->mWakeUpEndTimeNs > static_cast<std::int64_t>(mLevelTimeNs) ||
			aPlantOnLawn.mNormalPlant->mOnBungeeState == PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
		{
			return PlantingReason::PLANTING_NEEDS_SLEEPING;
		}

		return PlantingReason::PLANTING_OK;
	}
	// 非墓碑吞噬者且非飞行植物，则不能种在墓碑上
	if (aHasGrave)
	{
		return Plant::IsFlying(theSeedType) ? PlantingReason::PLANTING_OK : PlantingReason::PLANTING_NOT_ON_GRAVE;
	}

	Plant* aUnderPlant = aPlantOnLawn.mUnderPlant;
	bool aHasLilypad, aHasFlowerPot;
	if (!aUnderPlant || aUnderPlant->mOnBungeeState == PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
	{
		aHasLilypad = false;
		aHasFlowerPot = false;
	}
	else
	{
		aHasLilypad = aUnderPlant->mSeedType == SeedType::SEED_LILYPAD;
		aHasFlowerPot = aUnderPlant->mSeedType == SeedType::SEED_FLOWERPOT;
	}
	// 部分情况下的格子中不能种植植物
	if (GetCraterAt(theGridX, theGridY))
	{
		return PlantingReason::PLANTING_NOT_ON_CRATER;
	}
	if (GetScaryPotAt(theGridX, theGridY) || IsIceAt(theGridX, theGridY))
	{
		return PlantingReason::PLANTING_NOT_HERE;
	}
	GridSquareType aGridSquare = mGridSquareType[theGridX][theGridY];
	if (aGridSquare == GridSquareType::GRIDSQUARE_DIRT || aGridSquare == GridSquareType::GRIDSQUARE_NONE)
	{
		return PlantingReason::PLANTING_NOT_HERE;
	}
	// 水生植物只能种在水上
	Plant* aNormalPlant = aPlantOnLawn.mNormalPlant;
	if (theSeedType == SeedType::SEED_LILYPAD || theSeedType == SeedType::SEED_TANGLEKELP || theSeedType == SeedType::SEED_SEASHROOM)
	{
		if (!IsPoolSquare(theGridX, theGridY))
		{
			return PlantingReason::PLANTING_ONLY_IN_POOL;
		}

		return (aNormalPlant || aUnderPlant) ? PlantingReason::PLANTING_NOT_HERE : PlantingReason::PLANTING_OK;
	}
	if (Plant::IsFlying(theSeedType))
	{
		return aPlantOnLawn.mFlyingPlant ? PlantingReason::PLANTING_NOT_HERE : PlantingReason::PLANTING_OK;
	}
	// 地刺/地刺王只能种在坚固的地面
	if (theSeedType == SeedType::SEED_SPIKEWEED || theSeedType == SeedType::SEED_SPIKEROCK)
	{
		if (aGridSquare == GridSquareType::GRIDSQUARE_POOL || StageHasRoof() || aUnderPlant)
		{
			return PlantingReason::PLANTING_NEEDS_GROUND;
		}
	}
	// 非水生植物不能种在水面上（南瓜头可以种在香蒲上）
	Plant* aPumpkinPlant = aPlantOnLawn.mPumpkinPlant;
	if (aGridSquare == GridSquareType::GRIDSQUARE_POOL && !aHasLilypad && theSeedType != SeedType::SEED_CATTAIL)
	{
		if (!aNormalPlant || aNormalPlant->mSeedType != SeedType::SEED_CATTAIL || theSeedType != SeedType::SEED_PUMPKINSHELL)
		{
			return PlantingReason::PLANTING_NOT_ON_WATER;
		}
	}
	// 花盆的种植条件
	if (theSeedType == SeedType::SEED_FLOWERPOT)
	{
		return (aNormalPlant || aUnderPlant || aPumpkinPlant) ? PlantingReason::PLANTING_NOT_HERE : PlantingReason::PLANTING_OK;
	}
	// 屋顶种植需要花盆
	if (StageHasRoof() && !aHasFlowerPot)
	{
		return PlantingReason::PLANTING_NEEDS_POT;
	}
	// 南瓜头的种植条件
	bool aAidPurchased = mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_FIRSTAID] > 0;
	if (theSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		// 不可种植在玉米加农炮上
		if (aNormalPlant && aNormalPlant->mSeedType == SeedType::SEED_COBCANNON)
		{
			return PlantingReason::PLANTING_NOT_HERE;
		}
		// 无南瓜头时，可以种植南瓜头
		if (!aPumpkinPlant)
		{
			return PlantingReason::PLANTING_OK;
		}
		// 南瓜头的坚果包扎术
		if (aAidPurchased && aPumpkinPlant->mPlantHealth < aPumpkinPlant->mPlantMaxHealth * 2 / 3 &&
			aPumpkinPlant->mSeedType == SeedType::SEED_PUMPKINSHELL && aPumpkinPlant->mOnBungeeState != PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
		{
			return PlantingReason::PLANTING_OK;
		}

		return PlantingReason::PLANTING_NOT_HERE;
	}
	// 土豆地雷只能种在陆地上
	if (aHasLilypad && theSeedType == SeedType::SEED_POTATOMINE)
	{
		return PlantingReason::PLANTING_ONLY_ON_GROUND;
	}

	if (aUnderPlant)
	{
		// 香蒲对底端植物的紫卡升级
		if (theSeedType == SeedType::SEED_CATTAIL)
		{
			if (aNormalPlant)
			{
				return PlantingReason::PLANTING_NOT_HERE;
			}
			if (aUnderPlant->IsUpgradableTo(theSeedType) && aUnderPlant->mOnBungeeState != PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
			{
				return PlantingReason::PLANTING_OK;
			}
			if (Plant::IsUpgrade(theSeedType))
			{
				return PlantingReason::PLANTING_NEEDS_UPGRADE;
			}
		}
		else
		{
			// 模仿中的模仿者不可作为花盆或睡莲
			if (aUnderPlant->mSeedType == SeedType::SEED_IMITATER)
			{
				return PlantingReason::PLANTING_NOT_HERE;
			}
		}
	}

	// 一般紫卡植物的更迭判断
	if (aNormalPlant)
	{
		// 紫卡植物的升级
		if (aNormalPlant->IsUpgradableTo(theSeedType) && aNormalPlant->mOnBungeeState != PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
		{
			return PlantingReason::PLANTING_OK;
		}
		if (Plant::IsUpgrade(theSeedType))
		{
			return PlantingReason::PLANTING_NEEDS_UPGRADE;
		}

		// 坚果包扎术
		if ((theSeedType == SeedType::SEED_WALLNUT || theSeedType == SeedType::SEED_TALLNUT) && aAidPurchased)
		{
			if (aNormalPlant->mPlantHealth < aNormalPlant->mPlantMaxHealth * 2 / 3 &&
				aNormalPlant->mSeedType == theSeedType && aNormalPlant->mOnBungeeState != PlantOnBungeeState::GETTING_GRABBED_BY_BUNGEE)
			{
				return PlantingReason::PLANTING_OK;
			}
		}

		return PlantingReason::PLANTING_NOT_HERE;
	}

	// 免费种植模式下紫卡的额外判断
	if (!mApp->mEasyPlantingCheat && Plant::IsUpgrade(theSeedType))
	{
		return PlantingReason::PLANTING_NEEDS_UPGRADE;
	}
	if (theSeedType == SeedType::SEED_COBCANNON && !IsValidCobCannonSpot(theGridX, theGridY))
	{
		return PlantingReason::PLANTING_NEEDS_UPGRADE;
	}
	else if (theSeedType == SeedType::SEED_CATTAIL && aGridSquare != GridSquareType::GRIDSQUARE_POOL)
	{
		return PlantingReason::PLANTING_NOT_HERE;
	}

	return PlantingReason::PLANTING_OK;
}

//0x40E520
void Board::UpdateCursor()
{
	int aMouseX = mApp->mWidgetManager->mLastMouseX - mX;
	int aMouseY = mApp->mWidgetManager->mLastMouseY - mY;
	bool aShowFinger = false;
	bool aShowDrag = false;
	bool aHideCursor = false;

	if (mApp->mSeedChooserScreen && mApp->mSeedChooserScreen->Contains(aMouseX + mX, aMouseY + mY))
		return;

	if (mApp->GetDialogCount() > 0)
		return;

	if (mPaused || mBoardFadeOutCounter >= 0 || mTimeStopCounter > 0 || mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
	{
		mApp->SetCursor(Sexy::CURSOR_POINTER);
		return;
	}

	HitResult aHitResult;
	MouseHitTest(aMouseX, aMouseY, &aHitResult);
	// 手形光标类型集：命中即显示手指（原 18 连落空 case）。位掩码锚枚举名，
	// GameObjectType 全 22 值 < 64 位；成员判定一次移位与运算取代跳转表。
	static constexpr unsigned long long kFingerCursorMask =
		(1ull << (int)GameObjectType::OBJECT_TYPE_MENU_BUTTON) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_STORE_BUTTON) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_SHOVEL) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_WATERING_CAN) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_FERTILIZER) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_BUG_SPRAY) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_PHONOGRAPH) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_CHOCOLATE) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_GLOVE) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_MONEY_SIGN) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_NEXT_GARDEN) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_WHEELBARROW) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_SLOT_MACHINE_HANDLE) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_TREE_FOOD) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_STINKY) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_TREE_OF_WISDOM) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_COIN) |
		(1ull << (int)GameObjectType::OBJECT_TYPE_PROJECTILE);
	static_assert((int)GameObjectType::OBJECT_TYPE_TREE_OF_WISDOM < 64, "光标位掩码须覆盖全部对象类型");
	GameObjectType aObjectType = aHitResult.mObjectType;
	if ((unsigned)aObjectType < 64 && (kFingerCursorMask >> (unsigned)aObjectType) & 1ull)
	{
		aShowFinger = true;
	}
	else
	{
		switch (aObjectType)
		{
		case GameObjectType::OBJECT_TYPE_SEEDPACKET:
			aShowFinger = ((SeedPacket*)aHitResult.mObject)->CanPickUp();
			break;

		case GameObjectType::OBJECT_TYPE_SCARY_POT:
			if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_NORMAL)
			{
				aShowFinger = true;
			}
			else if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_HAMMER)
			{
				aHideCursor = true;
			}
			break;

		case GameObjectType::OBJECT_TYPE_PLANT:
			if ((mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST) && !HasLevelAwardDropped())
			{
				aShowFinger = true;
			}
			if (((Plant*)aHitResult.mObject)->mState == PlantState::STATE_COBCANNON_READY)
			{
				aShowFinger = true;
			}
			break;

		default:
			if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_HAMMER)
			{
				aHideCursor = true;
			}
			break;
		}
	}

	if (mChallenge->mBeghouledMouseCapture || aShowDrag)
	{
		mApp->SetCursor(Sexy::CURSOR_DRAGGING);
	}
	else if (aShowFinger)
	{
		mApp->SetCursor(Sexy::CURSOR_HAND);
	}
	else if (aHideCursor)
	{
		mApp->SetCursor(Sexy::CURSOR_NONE);
	}
	else
	{
		mApp->SetCursor(Sexy::CURSOR_POINTER);
	}
}

//0x40E760
void Board::MouseMove(int x, int y)
{
	Widget::MouseMove(x, y);
	mChallenge->MouseMove(x, y);
}

//0x40E760
void Board::MouseDrag(int x, int y)
{
	Widget::MouseDrag(x, y);
	mChallenge->MouseMove(x, y);
}

//0x40E780
Zombie* Board::ZombieHitTest(int theMouseX, int theMouseY)
{
	Zombie* aZombie = nullptr;
	Zombie* aRecord = nullptr;
	while (IterateZombies(aZombie))
	{
		// 排除已死亡的僵尸
		if (aZombie->mDead || aZombie->IsDeadOrDying())
			continue;

		// 排除关卡引入阶段及选卡界面的植物僵尸
		if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && Zombie::IsZombotany(aZombie->mZombieType))
			continue;

		// 范围判定
		if (aZombie->GetZombieRect().Contains(theMouseX, theMouseY))
		{
			if (aRecord == nullptr || (aZombie->mY) > (aRecord->mY))
			{
				aRecord = aZombie;
			}
		}
	}

	return aRecord;
}

//0x40E880
bool Board::IsPlantInGoldWateringCanRange(int theMouseX, int theMouseY, Plant* thePlant)
{
	/*
	Rect aRect = Rect(theMouseX - 70, theMouseY - 80, 160, 160);
	if (GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, PlantPriority::TOPPLANT_ZEN_TOOL_ORDER) == thePlant)
	{
		return aRect.Contains((thePlant->mX) + 40, (thePlant->mY) + 40);
	}
	return false;
	*/

	int aMinX = theMouseX - 70;
	int aMaxX = theMouseX + 90;
	int aMinY = theMouseY - 80;
	int aMaxY = theMouseY + 80;

	if (GetTopPlantAt(thePlant->mPlantCol, thePlant->mRow, PlantPriority::TOPPLANT_ZEN_TOOL_ORDER) == thePlant)
	{
		return (thePlant->mX) + 40 >= aMinX && (thePlant->mX) + 40 < aMaxX && (thePlant->mY) + 40 >= aMinY && (thePlant->mY) + 40 < aMaxY;
	}
	return false;
}

//0x40E940
void Board::HighlightPlantsForMouse(int theMouseX, int theMouseY)
{
	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_WATERING_CAN && mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_GOLD_WATERINGCAN])
	{
		Plant* aPlant = nullptr;
		while (IteratePlants(aPlant))
		{
			if (IsPlantInGoldWateringCanRange(theMouseX, theMouseY, aPlant))
			{
				aPlant->mHighlighted = true;
				Plant* aFlowerPot = GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_ONLY_UNDER_PLANT);
				if (aFlowerPot)
				{
					aFlowerPot->mHighlighted = true;
				}
			}
		}
	}
	else
	{
		Plant* aPlant = ToolHitTest(theMouseX, theMouseY);
		if (aPlant)
		{
			aPlant->mHighlighted = true;
			if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
			{
				Plant* aFlowerPot = GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_ONLY_UNDER_PLANT);
				if (aFlowerPot)
				{
					aFlowerPot->mHighlighted = true;
				}
			}
		}
	}
}

//0x40EAB0
void Board::UpdateMousePosition()
{
	UpdateCursor();
	UpdateToolTip();
    if (!UpdateTouchHighlights())
        UpdatePointerHighlights(mApp->mWidgetManager->mLastMouseX - mX,
            mApp->mWidgetManager->mLastMouseY - mY);
}

void Board::UpdatePointerHighlights(int aMouseX, int aMouseY)
{
	Plant* aPlant = nullptr;
	while (IteratePlants(aPlant))
	{
		aPlant->mHighlighted = false;
	}

	SeedType aCursorSeedType = GetSeedTypeInCursor();
	// 破罐者关卡中，检测并高亮鼠标悬浮的罐子
	if (mApp->IsScaryPotterLevel())
	{
		GridItem* aGridItem = nullptr;
		while (IterateGridItems(aGridItem))
		{
			if (aGridItem->mGridItemType == GridItemType::GRIDITEM_SCARY_POT)
			{
				aGridItem->mHighlighted = false;
			}
		}

		HitResult aHitResult;
		MouseHitTest(aMouseX, aMouseY, &aHitResult);
		if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_SCARY_POT)
		{
			GridItem* aScaryPot = (GridItem*)aHitResult.mObject;
			aScaryPot->mHighlighted = true;
			return;
		}
	}

	// 禅境花园，设定蜗牛的高亮与否
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		GridItem* aStinky = mApp->mZenGarden->GetStinky();
		if (aStinky)
		{
			HitResult aHitResult;
			MouseHitTest(aMouseX, aMouseY, &aHitResult);
			aStinky->mHighlighted = aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_STINKY;
		}
	}

	// 手持铲子或花园工具时，令作用的植物高亮
	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_SHOVEL ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_WATERING_CAN ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_FERTILIZER ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_BUG_SPRAY ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PHONOGRAPH ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_CHOCOLATE ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_GLOVE ||
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_MONEY_SIGN ||
		(mCursorObject->mCursorType == CursorType::CURSOR_TYPE_WHEEELBARROW && !mApp->mZenGarden->GetPottedPlantInWheelbarrow()))
	{
		HighlightPlantsForMouse(aMouseX, aMouseY);
		return;
	}

	// 咖啡豆及坚果包扎术
	if (aCursorSeedType == SeedType::SEED_INSTANT_COFFEE)
	{
		int aGridX = PlantingPixelToGridX(aMouseX, aMouseY, aCursorSeedType);
		int aGridY = PlantingPixelToGridY(aMouseX, aMouseY, aCursorSeedType);

		Plant* aPlant = GetTopPlantAt(aGridX, aGridY, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
		if (aPlant && aPlant->mIsAsleep && CanPlantAt(aGridX, aGridY, SeedType::SEED_INSTANT_COFFEE) == PlantingReason::PLANTING_OK)
		{
			aPlant->mHighlighted = true;
		}
	}
	else if (aCursorSeedType == SeedType::SEED_WALLNUT || aCursorSeedType == SeedType::SEED_TALLNUT)
	{
		int aGridX = PlantingPixelToGridX(aMouseX, aMouseY, aCursorSeedType);
		int aGridY = PlantingPixelToGridY(aMouseX, aMouseY, aCursorSeedType);

		Plant* aPlant = GetTopPlantAt(aGridX, aGridY, PlantPriority::TOPPLANT_ONLY_PUMPKIN);
		if (aPlant && aPlant->mSeedType == aCursorSeedType && CanPlantAt(aGridX, aGridY, aCursorSeedType) == PlantingReason::PLANTING_OK)
		{
			aPlant->mHighlighted = true;
		}
	}
	else if (aCursorSeedType == SeedType::SEED_PUMPKINSHELL)
	{
		int aGridX = PlantingPixelToGridX(aMouseX, aMouseY, aCursorSeedType);
		int aGridY = PlantingPixelToGridY(aMouseX, aMouseY, aCursorSeedType);

		Plant* aPlant = GetTopPlantAt(aGridX, aGridY, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
		if (aPlant && aPlant->mSeedType == SeedType::SEED_PUMPKINSHELL && CanPlantAt(aGridX, aGridY, SeedType::SEED_PUMPKINSHELL) == PlantingReason::PLANTING_OK)
		{
			aPlant->mHighlighted = true;
		}
	}
}

namespace
{
// Beghouled 卡牌→提示串表（B 形态；SeedType 直下标 0..55，nullptr = 无提示 = 原链落空，
// 标签保持函数开头已重置的空串）。gStr 为全局结构体实例，成员地址编译期可取。
// 2026-10-04 子代理审计抓获：曾少一行 nullptr 致 SHUFFLE/CRATER 错位一格
// （洗牌钮显示陨石坑文案）；**无尺寸声明**让计数锚同时抓少行/多行——
// 显式宽声明的长度锚只校验维度不校验初始化器个数，编译器不报错。
constexpr const std::string* kBeghouledSeedToolTip[] = {
	/* [0..6]                                                                     */ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	/* [7]  SEED_REPEATER                                                         */ &gStr.BEGHOULED_REPEATER_UPGRADE_TOOLTIP,
	/* [8..9]                                                                     */ nullptr, nullptr,
	/* [10] SEED_FUMESHROOM                                                       */ &gStr.BEGHOULED_FUMESHROOM_UPGRADE_TOOLTIP,
	/* [11..22]                                                                   */ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	/* [23] SEED_TALLNUT                                                          */ &gStr.BEGHOULED_TALLNUT_UPGRADE_TOOLTIP,
	/* [24..53]                                                                   */ nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	/* [54] SEED_BEGHOULED_BUTTON_SHUFFLE                                         */ &gStr.BEGHOULED_SHUFFLE_TOOLTIP,
	/* [55] SEED_BEGHOULED_BUTTON_CRATER                                          */ &gStr.BEGHOULED_CRATER_TOOLTIP,
};
static_assert(sizeof(kBeghouledSeedToolTip) / sizeof(kBeghouledSeedToolTip[0]) ==
	(int)SeedType::SEED_BEGHOULED_BUTTON_CRATER + 1,
	"Beghouled 提示表初始化器数须恰为 SEED_BEGHOULED_BUTTON_CRATER+1（少行/多行皆编译期炸）");
static_assert(kBeghouledSeedToolTip[(int)SeedType::SEED_BEGHOULED_BUTTON_SHUFFLE] == &gStr.BEGHOULED_SHUFFLE_TOOLTIP &&
	kBeghouledSeedToolTip[(int)SeedType::SEED_BEGHOULED_BUTTON_CRATER] == &gStr.BEGHOULED_CRATER_TOOLTIP,
	"Beghouled 提示表关键槽锚");

// 扩展卡牌→提示串表（B 形态；SeedType 直下标 0..74，nullptr = 走默认 Plant::GetNameString；
// 注意 NUM_SEED_TYPES=53 位于枚举中部，故以 SEED_ZOMBIE_IMP 为尾锚 count_last）。
// 2026-10-04 子代理审计+用户实机抓获：头部曾少一行 nullptr 致 56..73 全段错位
// 一格、SEED_ZOMBIE_IMP(74) 槽落 nullptr——悬停 IMP 卡（I,Zombie 第 8/9 关与
// 无尽种子栏）落兜底 GetNameString(74)→gPlantDefs[74]（53 宽）越界读垃圾
// mPlantName → std::string 构造 memcpy(69B) ucrtbase 崩溃。**无尺寸声明**：
// 计数锚同时抓少行/多行（显式宽声明只校验维度不校验初始化器个数）。
constexpr const std::string* kSeedToolTip[] = {
	/* [0..55]（基础/升级/_beghouled 卡不在本表：Beghouled 模式之外走默认）         */
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	/* [56] SEED_SLOT_MACHINE_SUN                                                 */ &gStr.SLOT_MACHINE_SUN_TOOLTIP,
	/* [57] SEED_SLOT_MACHINE_DIAMOND                                             */ &gStr.SLOT_MACHINE_DIAMOND_TOOLTIP,
	/* [58] SEED_ZOMBIQUARIUM_SNORKLE                                             */ &gStr.ZOMBIQUARIUM_SNORKEL_TOOLTIP,
	/* [59] SEED_ZOMBIQUARIUM_TROPHY                                              */ &gStr.ZOMBIQUARIUM_TROPHY_TOOLTIP,
	/* [60] SEED_ZOMBIE_NORMAL                                                    */ &gStr.ZOMBIE,
	/* [61] SEED_ZOMBIE_TRAFFIC_CONE                                              */ &gStr.CONEHEAD_ZOMBIE,
	/* [62] SEED_ZOMBIE_POLEVAULTER                                               */ &gStr.POLE_VAULTING_ZOMBIE,
	/* [63] SEED_ZOMBIE_PAIL                                                      */ &gStr.BUCKETHEAD_ZOMBIE,
	/* [64] SEED_ZOMBIE_LADDER                                                    */ &gStr.LADDER_ZOMBIE,
	/* [65] SEED_ZOMBIE_DIGGER                                                    */ &gStr.DIGGER_ZOMBIE,
	/* [66] SEED_ZOMBIE_BUNGEE                                                    */ &gStr.BUNGEE_ZOMBIE,
	/* [67] SEED_ZOMBIE_FOOTBALL                                                  */ &gStr.FOOTBALL_ZOMBIE,
	/* [68] SEED_ZOMBIE_BALLOON                                                   */ &gStr.BALLOON_ZOMBIE,
	/* [69] SEED_ZOMBIE_SCREEN_DOOR                                               */ &gStr.SCREEN_DOOR_ZOMBIE,
	/* [70] SEED_ZOMBONI（不可达：全关卡不投放，原链无此支）                      */ nullptr,
	/* [71] SEED_ZOMBIE_POGO（不可达：同上）                                      */ nullptr,
	/* [72] SEED_ZOMBIE_DANCER                                                    */ &gStr.DANCING_ZOMBIE,
	/* [73] SEED_ZOMBIE_GARGANTUAR                                                */ &gStr.GARGANTUAR,
	/* [74] SEED_ZOMBIE_IMP                                                       */ &gStr.IMP,
};
static_assert(sizeof(kSeedToolTip) / sizeof(kSeedToolTip[0]) == (int)SeedType::SEED_ZOMBIE_IMP + 1,
	"卡牌提示表初始化器数须恰为 SEED_ZOMBIE_IMP+1（少行/多行皆编译期炸）");
static_assert(kSeedToolTip[(int)SeedType::SEED_SLOT_MACHINE_SUN] == &gStr.SLOT_MACHINE_SUN_TOOLTIP &&
	kSeedToolTip[(int)SeedType::SEED_ZOMBIE_NORMAL] == &gStr.ZOMBIE &&
	kSeedToolTip[(int)SeedType::SEED_ZOMBIE_IMP] == &gStr.IMP,
	"卡牌提示表关键槽锚（尾段头/僵尸段头/尾行）");

// 禅境花园工具→提示串表（B 形态；GameObjectType 直下标 0..21，本链覆盖 6..14 九种工具；
// nullptr = 原链落空（SHOVEL [5] / NEXT_GARDEN [15] 等其余类型在 UpdateToolTip 另有专支））。
constexpr const std::string* kZenToolToolTip[GameObjectType::OBJECT_TYPE_TREE_OF_WISDOM + 1] = {
	/* [0..5]  NONE/PLANT/PROJECTILE/COIN/SEEDPACKET/SHOVEL（SHOVEL 另有专支）    */
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
	/* [6]  OBJECT_TYPE_WATERING_CAN（原内链第 1 支）                          */ &gStr.WATERING_CAN_TOOLTIP,
	/* [7]  OBJECT_TYPE_FERTILIZER                                            */ &gStr.FERTILIZER_TOOLTIP,
	/* [8]  OBJECT_TYPE_BUG_SPRAY                                            */ &gStr.BUG_SPRAY_TOOLTIP,
	/* [9]  OBJECT_TYPE_PHONOGRAPH                                           */ &gStr.PHONOGRAPH_TOOLTIP,
	/* [10] OBJECT_TYPE_CHOCOLATE                                            */ &gStr.CHOCOLATE_TOOLTIP,
	/* [11] OBJECT_TYPE_GLOVE                                                */ &gStr.GLOVE_TOOLTIP,
	/* [12] OBJECT_TYPE_MONEY_SIGN                                           */ &gStr.MONEY_SIGN_TOOLTIP,
	/* [13] OBJECT_TYPE_WHEELBARROW                                          */ &gStr.WHEELBARROW_TOOLTIP,
	/* [14] OBJECT_TYPE_TREE_FOOD（原内链末支）                               */ &gStr.TREE_FERTILIZER_TOOLTIP,
	/* [15..21] NEXT_GARDEN（另有专支）/MENU/STORE/SLOT_HANDLE/SCARY_POT/STINKY/TREE */
	nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
};
static_assert(sizeof(kZenToolToolTip) / sizeof(kZenToolToolTip[0]) == GameObjectType::OBJECT_TYPE_TREE_OF_WISDOM + 1,
	"禅境花园工具提示表槽位须覆盖 OBJECT_TYPE_TREE_OF_WISDOM");
static_assert(kZenToolToolTip[GameObjectType::OBJECT_TYPE_WATERING_CAN] == &gStr.WATERING_CAN_TOOLTIP &&
	kZenToolToolTip[GameObjectType::OBJECT_TYPE_TREE_FOOD] == &gStr.TREE_FERTILIZER_TOOLTIP &&
	kZenToolToolTip[GameObjectType::OBJECT_TYPE_SHOVEL] == nullptr,
	"禅境花园工具提示表关键槽锚（首末工具与 SHOVEL 落空）须与原链一致");
}  // namespace

//0x40EF00
void Board::UpdateToolTip()
{
	if (!mApp->mWidgetManager->mMouseIn || !mApp->mActive || mTimeStopCounter > 0 || mApp->GetDialogCount() > 0 || mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
	{
		mToolTip->mVisible = false;
		return;
	}

	int aMouseX = mApp->mWidgetManager->mLastMouseX - mX;
	int aMouseY = mApp->mWidgetManager->mLastMouseY - mY;

	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
	{
		if (!mCutScene->mSeedChoosing)
		{
			mToolTip->mVisible = false;
			return;
		}

		if (mSeedBank->ContainsPoint(mWidgetManager->mLastMouseX-mX,
			mWidgetManager->mLastMouseY-mY) ||
			mApp->mSeedChooserScreen->mAlmanacButton->IsMouseOver() ||
			mApp->mSeedChooserScreen->mStoreButton->IsMouseOver() ||
			mApp->mSeedChooserScreen->mImitaterButton->IsMouseOver())
		{
			mToolTip->mVisible = false;
			return;
		}

		Zombie* aZombie = ZombieHitTest(aMouseX, aMouseY);
		if (aZombie == nullptr || aZombie->mFromWave != Zombie::ZOMBIE_WAVE_CUTSCENE)
		{
			mToolTip->mVisible = false;
			return;
		}

		SexyString aZombieName = EntityZombieName(aZombie->mZombieType);
		mToolTip->SetTitle(aZombieName);
		// GOTY 1073 (00411890): only the red-eye variant (32) lacks an almanac entry.
		if (mApp->CanShowAlmanac() && aZombie->mZombieType != ZombieType::ZOMBIE_REDEYE_GARGANTUAR)
		{
			mToolTip->SetLabel(gStr.CLICK_TO_VIEW);
		}
		else
		{
			mToolTip->SetLabel((""));
		}
		mToolTip->SetWarningText((""));

		Rect aRect = aZombie->GetZombieRect();
		mToolTip->mX = aRect.mWidth / 2 + aRect.mX + 5;
		mToolTip->mY = aRect.mHeight + aRect.mY - 10;
		if (aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE)
		{
			mToolTip->mY = (aZombie->mY);
		}

		mToolTip->mVisible = true;
		mToolTip->mCenter = true;

		// The chooser draws this overlay in Board's frame, including camera pan.
		mToolTip->mMinLeft = SeedChooserLayout::RebaseX(DesktopUIAtlas::Images[size_t(DesktopUIAtlas::Sprite::SEEDCHOOSER_BACKGROUND)].mWidth,
			mApp->mSeedChooserScreen->mX, mX);
		if (mApp->mSeedChooserScreen->mAlmanacButton->mBtnNoDraw && mApp->mSeedChooserScreen->mStoreButton->mBtnNoDraw)
		{
			mToolTip->mMaxBottom = 600;
		}
		else
		{
			mToolTip->mMaxBottom = 570;
		}
		if (!mApp->mSeedChooserScreen->mImitaterButton->mBtnNoDraw)
		{
			mToolTip->CalculateSize();
			if (mX + mToolTip->mX - mToolTip->mWidth / 2 < mApp->mSeedChooserScreen->mX + 524)
			{
				mToolTip->mMaxBottom = 503;
			}
		}

		return;
	}

	if (!CanInteractWithBoardButtons())
	{
		mToolTip->mVisible = false;
		return;
	}

	mToolTip->mMinLeft = mApp->mScreenBounds.mX;
	mToolTip->mMaxBottom = BOARD_HEIGHT;
	mToolTip->SetTitle((""));
	mToolTip->SetLabel((""));
	mToolTip->SetWarningText((""));
	mToolTip->mCenter = false;
	if (mChallenge->UpdateToolTip(aMouseX, aMouseY))
	{
		return;
	}

	HitResult aHitResult;
	MouseHitTest(aMouseX, aMouseY, &aHitResult);

	if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_SHOVEL)
	{
		mToolTip->SetLabel(gStr.SHOVEL_TOOLTIP);
		Rect aShovelButtonRect = GetShovelButtonRect();
		mToolTip->mX = aShovelButtonRect.mX + 35;
		mToolTip->mY = aShovelButtonRect.mY + 72;
		mToolTip->mCenter = true;
		mToolTip->mVisible = true;
		return;
	}

	if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_NEXT_GARDEN)
	{
		mToolTip->SetLabel(gStr.NEXT_GARDEN_TOOLTIP);
		Rect aButtonRect = GetShovelButtonRect();
		GetZenButtonRect(GameObjectType::OBJECT_TYPE_NEXT_GARDEN, aButtonRect);
		mToolTip->mX = aButtonRect.mX + 35;
		mToolTip->mY = aButtonRect.mY + 52;
		mToolTip->mCenter = true;
		mToolTip->mVisible = true;
		return;
	}

	// B 形态 -> 常量表（上方 kZenToolToolTip）：GameObjectType 直下标，一次查表合并
	// 原 9 类型 OR 门外支与 9 支内链（nullptr = 落空，走后续 SEEDPACKET 分支）。
	const std::string* aZenToolToolTip = nullptr;
	if ((unsigned)aHitResult.mObjectType < (unsigned)(sizeof(kZenToolToolTip) / sizeof(kZenToolToolTip[0])))
	{
		aZenToolToolTip = kZenToolToolTip[aHitResult.mObjectType];
	}
	if (aZenToolToolTip != nullptr)
	{
		mToolTip->SetLabel(*aZenToolToolTip);

		Rect aButtonRect = GetShovelButtonRect();
		GetZenButtonRect(aHitResult.mObjectType, aButtonRect);
		this->mToolTip->mX = aButtonRect.mX + 35;
		this->mToolTip->mY = aButtonRect.mY + 72;
		this->mToolTip->mCenter = true;
		this->mToolTip->mVisible = true;
		return;
	}

	if (aHitResult.mObjectType != GameObjectType::OBJECT_TYPE_SEEDPACKET)
	{
		mToolTip->mVisible = false;
		return;
	}

	SeedPacket* aSeedPacket = (SeedPacket*)aHitResult.mObject;
	SeedType aUseSeedType = aSeedPacket->mPacketType;
	if (aSeedPacket->mPacketType == SeedType::SEED_IMITATER && aSeedPacket->mImitaterType != SeedType::SEED_NONE)
	{
		aUseSeedType = aSeedPacket->mImitaterType;
	}

	if (gLawnApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED || gLawnApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST)
	{
		// B 形态 -> 常量表（上方 kBeghouledSeedToolTip，SeedType 直下标；
		// nullptr = 无提示 = 原链落空（标签保持已重置的空串））
		if ((unsigned)aUseSeedType < sizeof(kBeghouledSeedToolTip) / sizeof(kBeghouledSeedToolTip[0]) &&
			kBeghouledSeedToolTip[(int)aUseSeedType] != nullptr)
		{
			mToolTip->SetLabel(*kBeghouledSeedToolTip[(int)aUseSeedType]);
		}
	}
	else if ((unsigned)aUseSeedType < sizeof(kSeedToolTip) / sizeof(kSeedToolTip[0]) &&
		kSeedToolTip[(int)aUseSeedType] != nullptr)
	{
		// B 形态 -> 常量表（上方 kSeedToolTip，SeedType 直下标；nullptr = 走默认名）
		mToolTip->SetLabel(*kSeedToolTip[(int)aUseSeedType]);
	}
	else
	{
		// 1073 004120DB preserves both the packet identity and its Imitater target.
		mToolTip->SetLabel(Plant::GetNameString(aSeedPacket->mPacketType, aSeedPacket->mImitaterType));
	}

	int aPlantCost = GetCurrentPlantCost(aSeedPacket->mPacketType, aSeedPacket->mImitaterType);
	if (mApp->mEasyPlantingCheat)
	{
		mToolTip->SetWarningText(("FREE_PLANTING_CHEAT"));
	}
	else if (!aSeedPacket->mActive && (gLawnApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED || gLawnApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST))
	{
		if (aSeedPacket->mPacketType == SeedType::SEED_BEGHOULED_BUTTON_CRATER)
		{
			mToolTip->SetWarningText(gStr.BEGHOULED_NO_CRATERS);
		}
		else
		{
			mToolTip->SetWarningText(gStr.BEGHOULED_SEED_ALREADY_PURCHASED);
		}
	}
	else if (!aSeedPacket->mActive)
	{
		mToolTip->SetWarningText(gStr.WAITING_FOR_SEED);
	}
	else if (!CanTakeSunMoney(aPlantCost) && !HasConveyorBeltSeedBank() && !mApp->IsSlotMachineLevel())
	{
		mToolTip->SetWarningText(gStr.NOT_ENOUGH_SUN);
	}
	else if (aUseSeedType == SeedType::SEED_GATLINGPEA)
	{
		if (!Met(SeedType::SEED_GATLINGPEA))
		{
			mToolTip->SetWarningText(gStr.REQUIRES_REPEATER);
		}
	}
	else if (aUseSeedType == SeedType::SEED_WINTERMELON)
	{
		if (!Met(SeedType::SEED_WINTERMELON))
		{
			mToolTip->SetWarningText(gStr.REQUIRES_MELONPULT);
		}
	}
	else if (aUseSeedType == SeedType::SEED_TWINSUNFLOWER)
	{
		if (!Met(SeedType::SEED_TWINSUNFLOWER))
		{
			mToolTip->SetWarningText(gStr.REQUIRES_SUNFLOWER);
		}
	}
	else if (aUseSeedType == SeedType::SEED_SPIKEROCK)
	{
		if (!Met(SeedType::SEED_SPIKEROCK))
		{
			mToolTip->SetWarningText(gStr.REQUIRES_SPIKEWEED);
		}
	}
	else if (aUseSeedType == SeedType::SEED_COBCANNON)
	{
		if (!Met(SeedType::SEED_COBCANNON))
		{
			mToolTip->SetWarningText(gStr.REQUIRES_KERNELPULTS);
		}
	}
	else if (aUseSeedType == SeedType::SEED_GOLD_MAGNET)
	{
		if (!Met(SeedType::SEED_GOLD_MAGNET))
		{
			mToolTip->SetWarningText(gStr.REQUIRES_MAGNETSHROOM);
		}
	}
	else if (aUseSeedType == SeedType::SEED_GLOOMSHROOM)
	{
		if (!Met(SeedType::SEED_GLOOMSHROOM))
		{
			mToolTip->SetWarningText(gStr.REQUIRES_FUMESHROOM);
		}
	}
	else if (aUseSeedType == SeedType::SEED_CATTAIL)
	{
		if (!Met(SeedType::SEED_CATTAIL))
		{
			mToolTip->SetWarningText(gStr.REQUIRES_LILY_PAD);
		}
	}

	mToolTip->mX = (SEED_PACKET_WIDTH - mToolTip->mWidth) / 2 + mSeedBank->mX + aSeedPacket->mOffsetX + aSeedPacket->mX + mApp->mScreenBounds.mX;
	mToolTip->mY = mSeedBank->mY + aSeedPacket->mY + 70;
    if(mLandscapeHud) {
        const auto rect=LandscapeHud::PacketBounds(*this,aSeedPacket->mIndex);
        mToolTip->mX=rect.mX+rect.mWidth;mToolTip->mY=rect.mY;
    }
	mToolTip->mVisible = true;
}

//0x40FC70
void Board::MouseDownCobcannonFire(int x, int y, int theClickCount)
{
	if (theClickCount >= 0 && y >= 80)
	{
		if (mCobCannonCursorDelayCounter > 0 && Distance2D(x, y, mCobCannonMouseX, mCobCannonMouseY) < 100.0f)
		{
			return;  // 误点检测：点击加农炮后的 30cs 内，点击的位置和准心位置之间的距离小于 100 时，将被判定为误点
		}

		if (mCursorObject->mCursorType != CursorType::CURSOR_TYPE_PLANT_FROM_DUPLICATOR)
		{
			Plant* aCobcannon = mPlants.DataArrayTryToGet(mCursorObject->mCobCannonPlantID);
			if (aCobcannon && !aCobcannon->mDead && aCobcannon->mState == PlantState::STATE_COBCANNON_READY)
			{
				aCobcannon->CobCannonFire(static_cast<std::int64_t>(mLevelTimeNs), x, y);
			}
		}
	}
	ClearCursor();
}

namespace
{
// 紫卡（升级种子段 40..47）种植建议查询（2026-10-04 用户裁决改窄表）：
// 段外（含 I,Zombie 僵尸卡 60..74）一律"无建议"= 原 if-else 链 default；
// 8 宽段表收进函数域 constexpr static——曾为 53 宽全域直下标，僵尸卡
// 越界读相邻静态数据当 std::string* 解引用（B 树实机 ucrtbase 崩溃，
// memcpy ~74B 垃圾指针），改窄表+段守卫根绝。gStr 为全局结构体实例，
// 成员地址编译期可取；AdviceType 各值显式列（枚举非连续）。
struct UpgradeAdviceEntry
{
    const std::string* mStr;
    AdviceType mType;
};

bool TryGetUpgradeAdvice(SeedType theSeedType, const std::string*& theStr, AdviceType& theType)
{
    static constexpr UpgradeAdviceEntry kUpgradeAdvice[] = {
        { &gStr.ADVICE_ONLY_ON_REPEATERS,    AdviceType::ADVICE_PLANT_ONLY_ON_REPEATERS },    // 40 SEED_GATLINGPEA
        { &gStr.ADVICE_ONLY_ON_SUNFLOWER,    AdviceType::ADVICE_PLANT_ONLY_ON_SUNFLOWER },    // 41 SEED_TWINSUNFLOWER
        { &gStr.ADVICE_ONLY_ON_FUMESHROOM,   AdviceType::ADVICE_PLANT_ONLY_ON_FUMESHROOM },   // 42 SEED_GLOOMSHROOM
        { &gStr.ADVICE_ONLY_ON_LILYPAD,      AdviceType::ADVICE_PLANT_ONLY_ON_LILYPAD },      // 43 SEED_CATTAIL
        { &gStr.ADVICE_ONLY_ON_MELONPULT,    AdviceType::ADVICE_PLANT_ONLY_ON_MELONPULT },    // 44 SEED_WINTERMELON
        { &gStr.ADVICE_ONLY_ON_MAGNETSHROOM, AdviceType::ADVICE_PLANT_ONLY_ON_MAGNETSHROOM }, // 45 SEED_GOLD_MAGNET
        { &gStr.ADVICE_ONLY_ON_SPIKEWEED,    AdviceType::ADVICE_PLANT_ONLY_ON_SPIKEWEED },    // 46 SEED_SPIKEROCK
        { &gStr.ADVICE_ONLY_ON_KERNELPULT,   AdviceType::ADVICE_PLANT_ONLY_ON_KERNELPULT },   // 47 SEED_COBCANNON
    };
    static_assert(sizeof(kUpgradeAdvice) / sizeof(kUpgradeAdvice[0]) ==
        (int)SeedType::SEED_COBCANNON - (int)SeedType::SEED_GATLINGPEA + 1,
        "升级建议段表槽位须与紫卡枚举段同步");
    const int anIndex = (int)theSeedType - (int)SeedType::SEED_GATLINGPEA;
    if ((unsigned)anIndex >= sizeof(kUpgradeAdvice) / sizeof(kUpgradeAdvice[0]))
        return false;
    theStr = kUpgradeAdvice[anIndex].mStr;
    theType = kUpgradeAdvice[anIndex].mType;
    return true;
}
}  // namespace

//0x40FD30
// GOTY @Patoke: 0x4126F0
void Board::MouseDownWithPlant(int x, int y, int theClickCount)
{
	// 右击鼠标：放下卡牌
	if (theClickCount < 0)
	{
		RefreshSeedPacketFromCursor();
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
		return;
	}

	// 我是僵尸模式中，交由 Challenge 处理
	if (mApp->IsIZombieLevel())
	{
		mChallenge->IZombieMouseDownWithZombie(x, y, theClickCount);
		return;
	}

	SeedType aPlantingSeedType = GetSeedTypeInCursor();
	int aGridX = PlantingPixelToGridX(x, y, aPlantingSeedType);
	int aGridY = PlantingPixelToGridY(x, y, aPlantingSeedType);

	// 不在场地内的点击：放下卡牌
	if (aGridX < 0 || aGridX >= MAX_GRID_SIZE_X || aGridY < 0 || aGridY > MAX_GRID_SIZE_Y)
	{
		RefreshSeedPacketFromCursor();
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
		return;
	}

	PlantingReason aReason = CanPlantAt(aGridX, aGridY, aPlantingSeedType);
	if (aReason != PlantingReason::PLANTING_OK)
	{
		// 根据不同的种植原因播放相应的提示字幕
		if (aReason == PlantingReason::PLANTING_ONLY_ON_GRAVES)
		{
			DisplayAdvice(gStr.ADVICE_GRAVEBUSTERS_ON_GRAVES, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_GRAVEBUSTERS_ON_GRAVES);
		}
		else if (aPlantingSeedType == SeedType::SEED_LILYPAD)
		{
			if (aReason == PlantingReason::PLANTING_ONLY_IN_POOL)
			{
				DisplayAdvice(gStr.ADVICE_LILYPAD_ON_WATER, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_LILYPAD_ON_WATER);
			}
		}
		else if (aPlantingSeedType == SeedType::SEED_TANGLEKELP)
		{
			if (aReason == PlantingReason::PLANTING_ONLY_IN_POOL)
			{
				DisplayAdvice(gStr.ADVICE_TANGLEKELP_ON_WATER, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_TANGLEKELP_ON_WATER);
			}
		}
		else if (aPlantingSeedType == SeedType::SEED_SEASHROOM)
		{
			if (aReason == PlantingReason::PLANTING_ONLY_IN_POOL)
			{
				DisplayAdvice(gStr.ADVICE_SEASHROOM_ON_WATER, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_SEASHROOM_ON_WATER);
			}
		}
		else if (aReason == PlantingReason::PLANTING_ONLY_ON_GROUND)
		{
			DisplayAdvice(gStr.ADVICE_POTATO_MINE_ON_LILY, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_POTATOE_MINE_ON_LILY);
		}
		else if (aReason == PlantingReason::PLANTING_NOT_PASSED_LINE)
		{
			DisplayAdvice(gStr.ADVICE_NOT_PASSED_LINE, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_NOT_PASSED_LINE);
		}
		else if (aReason == PlantingReason::PLANTING_NEEDS_UPGRADE)
		{
			// 段表查询（上方 TryGetUpgradeAdvice）：紫卡段外（含僵尸卡）无建议
			const std::string* anAdviceStr = nullptr;
			AdviceType anAdviceType = AdviceType::ADVICE_NONE;
			if (TryGetUpgradeAdvice(aPlantingSeedType, anAdviceStr, anAdviceType))
			{
				DisplayAdvice(*anAdviceStr, MessageStyle::MESSAGE_STYLE_HINT_FAST, anAdviceType);
			}
		}
		else if (aReason == PlantingReason::PLANTING_NOT_ON_ART)
		{
			SexyString aSeedName = Plant::GetNameString(mChallenge->GetArtChallengeSeed(aGridX, aGridY), SeedType::SEED_NONE);
			SexyString aMessage = TodReplaceString(gStr.ADVICE_WRONG_ART_TYPE, UiTextPlaceholder::Seed, aSeedName);
			DisplayAdvice(aMessage, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_WRONG_ART_TYPE);
		}
		else if (aReason == PlantingReason::PLANTING_NEEDS_POT)
		{
			if (mApp->IsFirstTimeAdventureMode() && mLevel == 41)
			{
				DisplayAdvice(gStr.ADVICE_PLANT_NEED_POT1, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_NEED_POT);
			}
			else
			{
				DisplayAdvice(gStr.ADVICE_PLANT_NEED_POT2, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_NEED_POT);
			}
		}
		else if (aReason == PlantingReason::PLANTING_NOT_ON_GRAVE)
		{
			DisplayAdvice(gStr.ADVICE_PLANT_NOT_ON_GRAVE, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_NOT_ON_GRAVE);
		}
		else if (aReason == PlantingReason::PLANTING_NOT_ON_CRATER)
		{
			if (IsPoolSquare(aGridX, aGridY))
			{
				DisplayAdvice(gStr.ADVICE_CANT_PLANT_THERE, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_CANT_PLANT_THERE);
			}
			else
			{
				DisplayAdvice(gStr.ADVICE_PLANT_NOT_ON_CRATER, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_NOT_ON_CRATER);
			}
		}
		else if (aReason == PlantingReason::PLANTING_NOT_ON_WATER)
		{
			if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN && mApp->mZenGarden->mGardenType == GardenType::GARDEN_AQUARIUM)
			{
				DisplayAdvice(gStr.ZEN_ONLY_AQUATIC_PLANTS, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_NONE);
			}
			else if (aPlantingSeedType == SeedType::SEED_POTATOMINE)
			{
				DisplayAdvice(gStr.ADVICE_POTATO_MINE_ON_LILY, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_POTATOE_MINE_ON_LILY);
			}
			else
			{
				DisplayAdvice(gStr.ADVICE_PLANT_NOT_ON_WATER, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANT_NOT_ON_WATER);
			}
		}
		else if (aReason == PlantingReason::PLANTING_NEEDS_GROUND)
		{
			DisplayAdvice(gStr.ADVICE_PLANTING_NEEDS_GROUND, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANTING_NEEDS_GROUND);
		}
		else if (aReason == PlantingReason::PLANTING_NEEDS_SLEEPING)
		{
			DisplayAdvice(gStr.ADVICE_PLANTING_NEED_SLEEPING, MessageStyle::MESSAGE_STYLE_HINT_FAST, AdviceType::ADVICE_PLANTING_NEED_SLEEPING);
		}

		// 特定情况下，放下原有手持的植物
		if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE || mApp->IsWhackAZombieLevel())
		{
			RefreshSeedPacketFromCursor();
			mApp->PlayFoley(FoleyType::FOLEY_DROP);
		}
		// 不可种植的情况至此结束，直接跳转至返回
		return;
	}

	/* 以下为植物类型可以种植的情况 */
	// 清除种植相关的提示字幕
	ClearAdvice(AdviceType::ADVICE_PLANTING_NEED_SLEEPING);
	ClearAdvice(AdviceType::ADVICE_CANT_PLANT_THERE);
	ClearAdvice(AdviceType::ADVICE_PLANTING_NEEDS_GROUND);
	ClearAdvice(AdviceType::ADVICE_PLANT_NOT_ON_WATER);
	ClearAdvice(AdviceType::ADVICE_PLANT_NOT_ON_CRATER);
	ClearAdvice(AdviceType::ADVICE_PLANT_NOT_ON_GRAVE);
	ClearAdvice(AdviceType::ADVICE_PLANT_NEED_POT);
	ClearAdvice(AdviceType::ADVICE_PLANT_WRONG_ART_TYPE);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_LILYPAD);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_MAGNETSHROOM);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_FUMESHROOM);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_KERNELPULT);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_SUNFLOWER);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_SPIKEWEED);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_MELONPULT);
	ClearAdvice(AdviceType::ADVICE_PLANT_ONLY_ON_REPEATERS);
	ClearAdvice(AdviceType::ADVICE_PLANT_NOT_PASSED_LINE);
	ClearAdvice(AdviceType::ADVICE_PLANT_GRAVEBUSTERS_ON_GRAVES);
	ClearAdvice(AdviceType::ADVICE_PLANT_LILYPAD_ON_WATER);
	ClearAdvice(AdviceType::ADVICE_PLANT_TANGLEKELP_ON_WATER);
	ClearAdvice(AdviceType::ADVICE_PLANT_SEASHROOM_ON_WATER);
	ClearAdvice(AdviceType::ADVICE_PLANT_POTATOE_MINE_ON_LILY);
	ClearAdvice(AdviceType::ADVICE_SURVIVE_FLAGS);

	// 无免费种植、非传送带关卡的卡槽植物，判断阳光是否充足：充足则扣除阳光，不足则退出
	if (!mApp->mEasyPlantingCheat && mCursorObject->mCursorType == CursorType::CURSOR_TYPE_PLANT_FROM_BANK && !HasConveyorBeltSeedBank())
	{
		if (!TakeSunMoney(GetCurrentPlantCost(aPlantingSeedType, SeedType::SEED_NONE)))
		{
			return;
		}
	}

	// 升级种植或坚果包扎术等情况时，先将原植物销毁
	bool aIsAwake = false;
	std::int64_t aWakeUpEndTimeNs = 0;
	PlantsOnLawn aPlantOnLawn;
	GetPlantsOnLawn(aGridX, aGridY, &aPlantOnLawn);
	Plant* aNormalPlant = aPlantOnLawn.mNormalPlant;
	Plant* aPumpkinPlant = aPlantOnLawn.mPumpkinPlant;
	if (aNormalPlant && aNormalPlant->IsUpgradableTo(aPlantingSeedType))
	{
		if (aPlantingSeedType == SeedType::SEED_GLOOMSHROOM)
		{
			aIsAwake = !aNormalPlant->mIsAsleep;
			aWakeUpEndTimeNs = aNormalPlant->mWakeUpEndTimeNs;
		}
		aNormalPlant->Die(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
	}
	if ((aPlantingSeedType == SeedType::SEED_WALLNUT || aPlantingSeedType == SeedType::SEED_TALLNUT) && aNormalPlant)
	{
		if (aNormalPlant->mSeedType == aPlantingSeedType)
		{
			aNormalPlant->Die(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
		}
	}
	if (aPlantingSeedType == SeedType::SEED_PUMPKINSHELL && aPumpkinPlant)
	{
		if (aPumpkinPlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
		{
			aPumpkinPlant->Die(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
		}
	}
	if (aPlantingSeedType == SeedType::SEED_COBCANNON)
	{
		Plant* aRightPlant = GetTopPlantAt(aGridX + 1, aGridY, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
		if (aRightPlant)
		{
			aRightPlant->Die(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
		}
	}
	if (aPlantingSeedType == SeedType::SEED_CATTAIL)
	{
		if (aPlantOnLawn.mUnderPlant)
		{
			aPlantOnLawn.mUnderPlant->Die(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
		}
		if (aNormalPlant)
		{
			aNormalPlant->Die(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
		}
	}

	// C 形态 -> 匿名 lambda 函数指针表 kPlantFromCursorCases（仿 kCobCannonStateCases
	// 范式）：行序 = CursorType 枚举数值序 CURSOR_TYPE_PLANT_FROM_BANK(1)..
	// CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW(5)。注意原链文本顺序 GLOVE→BARROW→
	// COIN→BANK ≠ 枚举数值序（GameSelector 教训，靠下方 static_assert 锚键值）；
	// [4] PLANT_FROM_DUPLICATOR 与段外值同落原 else 支的 TOD_ASSERT(false)。
	// 各支所需 x/y/aGridX/aGridY/aIsAwake/aWakeUpEndTimeNs 原为函数局部值，
	// 经 PlantFromCursorCtx 传参（lambda 须免捕获才可转函数指针）。
	struct PlantFromCursorCtx
	{
		int mX;
		int mY;
		int mGridX;
		int mGridY;
		bool mIsAwake;
		std::int64_t mWakeUpEndTimeNs;
	};
	using PlantFromCursorFn = void (*)(Board&, const PlantFromCursorCtx&);
	static constexpr PlantFromCursorFn kPlantFromCursorCases[(int)CursorType::CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW - (int)CursorType::CURSOR_TYPE_PLANT_FROM_BANK + 1] = {
		/* [1] CURSOR_TYPE_PLANT_FROM_BANK */
		[](Board& b, const PlantFromCursorCtx& c)
		{
			Plant* aPlant = b.AddPlant(c.mGridX, c.mGridY, b.mCursorObject->mType, b.mCursorObject->mImitaterType);
			if (c.mIsAwake)
			{
				aPlant->SetSleeping(static_cast<std::int64_t>(b.mLevelTimeNs), static_cast<std::int64_t>(b.mLevelTimeNs), false);
			}
			else
			{
				aPlant->mWakeUpEndTimeNs = c.mWakeUpEndTimeNs;
			}

			b.mSeedBank->mSeedPackets[b.mCursorObject->mSeedBankIndex].WasPlanted();
		},
		/* [2] CURSOR_TYPE_PLANT_FROM_USABLE_COIN */
		[](Board& b, const PlantFromCursorCtx& c)
		{
			b.AddPlant(c.mGridX, c.mGridY, b.mCursorObject->mType, b.mCursorObject->mImitaterType);
			Coin* aCoin = b.mCoins.DataArrayTryToGet(b.mCursorObject->mCoinID);
			b.mCursorObject->mCoinID = CoinID::COINID_NULL;
			aCoin->Die();
		},
		/* [3] CURSOR_TYPE_PLANT_FROM_GLOVE */
		[](Board& b, const PlantFromCursorCtx& c)
		{
			b.mApp->mZenGarden->MovePlant(b.mPlants.DataArrayTryToGet(b.mCursorObject->mGlovePlantID), c.mGridX, c.mGridY);
		},
		/* [4] CURSOR_TYPE_PLANT_FROM_DUPLICATOR（原链无此支，落 default 断言） */
		[](Board&, const PlantFromCursorCtx&)
		{
			TOD_ASSERT(false);
		},
		/* [5] CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW */
		[](Board& b, const PlantFromCursorCtx& c)
		{
			b.mApp->mZenGarden->MouseDownWithFullWheelBarrow(c.mX, c.mY);
		},
	};
	static_assert((int)CursorType::CURSOR_TYPE_PLANT_FROM_BANK == 1 && (int)CursorType::CURSOR_TYPE_PLANT_FROM_USABLE_COIN == 2 &&
		(int)CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE == 3 && (int)CursorType::CURSOR_TYPE_PLANT_FROM_DUPLICATOR == 4 &&
		(int)CursorType::CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW == 5,
		"CursorType PLANT_FROM 段键值须与 kPlantFromCursorCases 行序一致");

	const PlantFromCursorCtx aCursorCtx{ x, y, aGridX, aGridY, aIsAwake, aWakeUpEndTimeNs };
	const unsigned aCursorIndex = static_cast<unsigned>(mCursorObject->mCursorType) - static_cast<unsigned>(CursorType::CURSOR_TYPE_PLANT_FROM_BANK);
	if (aCursorIndex < sizeof(kPlantFromCursorCases) / sizeof(kPlantFromCursorCases[0]))
	{
		kPlantFromCursorCases[aCursorIndex](*this, aCursorCtx);
	}
	else
	{
		TOD_ASSERT(false);
	}

	// 柱子关卡中，一列种植
	// 已知原版 bug（2026-10-04 用户裁决不修，留改版）：紫卡升级列内他行不
	// 生效——升级目标格 CanPlantAt 返回 NEEDS_UPGRADE 被 continue 跳过，且
	// 循环内仅坚果包扎/南瓜壳两支 Die 特例，缺单格路径的 IsUpgradableTo
	// 支（AddPlant 前处理）。1073 原样保留，叠种已另行支持。
	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_COLUMN)
	{
		for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
		{
			if (aRow == aGridY || CanPlantAt(aGridX, aRow, aPlantingSeedType) != PlantingReason::PLANTING_OK)
				continue;

			if (aPlantingSeedType == SeedType::SEED_WALLNUT || aPlantingSeedType == SeedType::SEED_TALLNUT)
			{
				aNormalPlant = GetTopPlantAt(aGridX, aRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
				if (aNormalPlant && aNormalPlant->mSeedType == aPlantingSeedType)
				{
					aNormalPlant->Die(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
				}
			}
			if (aPlantingSeedType == SeedType::SEED_PUMPKINSHELL)
			{
				aPumpkinPlant = GetTopPlantAt(aGridX, aRow, PlantPriority::TOPPLANT_ONLY_PUMPKIN);
				if (aPumpkinPlant && aPumpkinPlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
				{
					aPumpkinPlant->Die(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
				}
			}
			AddPlant(aGridX, aRow, mCursorObject->mType, mCursorObject->mImitaterType);
		}
	}

	// 设置教程状态相关
	// A successful commit is authoritative even when a retained touch selection
	// or a restored save still carries PICK_UP/REFRESH. Do not require the card's
	// visual cooldown callback to have advanced teaching before counting plants.
	if (mTutorialState >= TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER &&
        mTutorialState <= TutorialState::TUTORIAL_LEVEL_1_REFRESH_PEASHOOTER)
	{
		SetTutorialState(mPlants.mSize >= 2 ? TutorialState::TUTORIAL_LEVEL_1_COMPLETED : TutorialState::TUTORIAL_LEVEL_1_REFRESH_PEASHOOTER);
	}
	else if (mTutorialState >= TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER &&
             mTutorialState <= TutorialState::TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER)
	{
		int aSunFlowersCount = CountSunFlowers();
		if (aPlantingSeedType == SeedType::SEED_SUNFLOWER && aSunFlowersCount == 2)
		{
			DisplayAdvice(gStr.ADVICE_MORE_SUNFLOWERS, MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL2, AdviceType::ADVICE_NONE);
			if (!mSeedBank->mSeedPackets[1].CanPickUp())
			{
				SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER);
			}
			else
			{
				SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER);
			}
		}
		else if (aSunFlowersCount >= 3)
		{
			SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_COMPLETED);
		}
		else if (!mSeedBank->mSeedPackets[1].CanPickUp())
		{
			SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER);
		}
		else
		{
			SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER);
		}
	}
	else if (mTutorialState == TutorialState::TUTORIAL_MORESUN_PLANT_SUNFLOWER)
	{
		if (CountSunFlowers() >= 3)
		{
			SetTutorialState(TutorialState::TUTORIAL_MORESUN_COMPLETED);
			DisplayAdvice(gStr.ADVICE_PLANT_SUNFLOWER5, MessageStyle::MESSAGE_STYLE_TUTORIAL_LATER, AdviceType::ADVICE_PLANT_SUNFLOWER5);
			mTutorialTimer = -1;
		}
		else if (!mSeedBank->mSeedPackets[1].CanPickUp())
		{
			SetTutorialState(TutorialState::TUTORIAL_MORESUN_REFRESH_SUNFLOWER);
		}
		else
		{
			SetTutorialState(TutorialState::TUTORIAL_MORESUN_PICK_UP_SUNFLOWER);
		}
	}

	// 保龄球关卡，播放保龄球滚动的音效
	if (mApp->IsWallnutBowlingLevel())
	{
		mApp->PlaySample(Sexy::SOUND_BOWLING);
	}

	// 重置鼠标
	ClearCursor();
}

//0x410F70
Plant* Board::ToolHitTestHelper(HitResult* theHitResult)
{
	theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_PLANT;
	Plant* aPlant = (Plant*)theHitResult->mObject;
	return (aPlant->mSeedType != SeedType::SEED_GRAVEBUSTER || mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN) ? aPlant : nullptr;
}

Plant* Board::ToolHitTest(int theX, int theY)
{
	HitResult aHitResult;
	MouseHitTest(theX, theY, &aHitResult);
	if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_PLANT)
	{
		return ToolHitTestHelper(&aHitResult);
	}
	return nullptr;
}

//0x410FA0
void Board::TutorialArrowShow(int theX, int theY)
{
	TutorialArrowRemove();
	TodParticleSystem* aParticle = mApp->AddTodParticle(theX, theY, MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 0), ParticleEffect::PARTICLE_SEED_PACKET_PICK, static_cast<std::int64_t>(mLevelTimeNs));
	mTutorialParticleID = mApp->ParticleGetID(aParticle);
}

//0x411020
void Board::TutorialArrowRemove()
{
	mApp->RemoveParticle(mTutorialParticleID);
	mTutorialParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;
}

//0x411060
void Board::MouseDownWithTool(int x, int y, int theClickCount, CursorType theCursorType)
{
	if (theClickCount < 0)
	{
		ClearCursor();
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
		return;
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		mApp->mZenGarden->MouseDownWithTool(x, y, theCursorType);
		return;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		mChallenge->TreeOfWisdomTool(x, y);
		return;
	}

	Plant* aPlant = ToolHitTest(x, y);
	if (aPlant == nullptr)
	{
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
	}
	else if (theCursorType == CursorType::CURSOR_TYPE_SHOVEL)
	{
		mApp->PlayFoley(FoleyType::FOLEY_USE_SHOVEL);
		mPlantsShoveled++;
		aPlant->Die(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));

		if (aPlant->mSeedType == SeedType::SEED_CATTAIL && GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_ONLY_PUMPKIN))
		{
			NewPlant(aPlant->mPlantCol, aPlant->mRow, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
		}
		if (mTutorialState == TutorialState::TUTORIAL_SHOVEL_DIG || mTutorialState == TutorialState::TUTORIAL_SHOVEL_KEEP_DIGGING)
		{
			SetTutorialState(CountPlantByType(SeedType::SEED_PEASHOOTER) == 0 ? TutorialState::TUTORIAL_SHOVEL_COMPLETED : TutorialState::TUTORIAL_SHOVEL_KEEP_DIGGING);
		}
	}

	ClearCursor();
}

//0x411280
Plant* Board::SpecialPlantHitTest(int x, int y)
{
	Plant* aPlant = nullptr;
	while (IteratePlants(aPlant))
	{
		if (aPlant->mSeedType == SeedType::SEED_PUMPKINSHELL)
		{
			float aMinDist = GetTopPlantAt(aPlant->mPlantCol, aPlant->mRow, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION) ? 25 : 0;
			float aDistance = Distance2D(x, y, (aPlant->mX) + 40, (aPlant->mY) + 40);
			if (aDistance >= aMinDist && aDistance <= 50 && y > (aPlant->mY) + 25)
			{
				return aPlant;
			}
		}
		else if (Plant::IsFlying(aPlant->mSeedType))
		{
			if (Distance2D(x, y, (aPlant->mX) + 40, (aPlant->mY)) < 15)
			{
				return aPlant;
			}
		}
	}
	return nullptr;
}

//0x411470
bool Board::MouseHitTestPlant(int x, int y, HitResult* theHitResult)
{
	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_COBCANNON_TARGET || mCursorObject->mCursorType == CursorType::CURSOR_TYPE_HAMMER)
		return false;

	Plant* aPlant;
	aPlant = SpecialPlantHitTest(x, y);
	if (aPlant)
	{
		theHitResult->mObject = aPlant;
		theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_PLANT;
		return true;
	}

	int aGridX = PixelToGridX(x, y);
	int aGridY = PixelToGridY(x, y);
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		aPlant = GetTopPlantAt(aGridX, aGridY, PlantPriority::TOPPLANT_ZEN_TOOL_ORDER);
		if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_WATERING_CAN && (!aPlant || !mApp->mZenGarden->PlantCanBeWatered(aPlant)))
		{
			Plant* aTopPlant = GetTopPlantAt(PixelToGridX(x - 30, y - 20), PixelToGridY(x - 30, y - 20), PlantPriority::TOPPLANT_ZEN_TOOL_ORDER);
			if (aTopPlant && mApp->mZenGarden->PlantCanBeWatered(aTopPlant))
			{
				aPlant = aTopPlant;
			}
		}
	}
	else
	{
		aPlant = GetTopPlantAt(aGridX, aGridY, PlantPriority::TOPPLANT_DIGGING_ORDER);
		if (aPlant && (aPlant->mSeedType == SeedType::SEED_LILYPAD || aPlant->mSeedType == SeedType::SEED_FLOWERPOT))
		{
			if (GetTopPlantAt(aGridX, aGridY, PlantPriority::TOPPLANT_ONLY_PUMPKIN))
			{
				return false;
			}
		}
	}

	// 植物不存在，或者手持巧克力但植物不需要巧克力时，返回“否”
	if (aPlant == nullptr)
	{
		return false;
	}
	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_CHOCOLATE && !mApp->mZenGarden->PlantCanHaveChocolate(aPlant))
	{
		theHitResult->mObject = nullptr;
		theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_NONE;
		return false;
	}

	theHitResult->mObject = aPlant;
	theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_PLANT;
	return true;
}

//0x411730
bool Board::MouseHitTest(int x, int y, HitResult* theHitResult)
{
	RefreshHudLayout();
	if (mBoardFadeOutCounter >= 0 || IsScaryPotterDaveTalking())
	{
		theHitResult->mObject = nullptr;
		theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_NONE;
		return false;
	}

	if (mMenuButton->HitTest(x, y, this) && CanInteractWithBoardButtons())
	{
		theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_MENU_BUTTON;
		return true;
	}
	else if (mStoreButton && mStoreButton->HitTest(x, y, this) && CanInteractWithBoardButtons())
	{
		theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_STORE_BUTTON;
		return true;
	}

	Rect aShovelButtonRect = GetShovelButtonRect();
	if (mSeedBank->MouseHitTest(x, y, theHitResult))
	{
		if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_NORMAL ||
			mCursorObject->mCursorType == CursorType::CURSOR_TYPE_COBCANNON_TARGET ||
			mCursorObject->mCursorType == CursorType::CURSOR_TYPE_HAMMER)
			return true;
	}
	if (mShowShovel && aShovelButtonRect.Contains(x, y) && CanInteractWithBoardButtons())
	{
		theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_SHOVEL;
		return true;
	}

	if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_NORMAL || mCursorObject->mCursorType == CursorType::CURSOR_TYPE_HAMMER)
	{
		Coin* aCoin = nullptr;
		Coin* aTopCoin = nullptr;
		while (IterateCoins(aCoin))
		{
			HitResult aHitResultCoin;
			if (aCoin->MouseHitTest(x, y, &aHitResultCoin))
			{
				aCoin = (Coin*)aHitResultCoin.mObject;
				if (aTopCoin == nullptr || aCoin->mRenderOrder >= aTopCoin->mRenderOrder)
				{
					theHitResult->mObjectType = aHitResultCoin.mObjectType;
					theHitResult->mObject = aCoin;
					aTopCoin = aCoin;
				}
			}
		}
		if (aTopCoin)
		{
			return true;
		}
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		bool canClick = false;
		if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_CHOCOLATE && !mApp->mZenGarden->IsStinkyHighOnChocolate())
		{
			canClick = true;
		}
		else if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_NORMAL && mApp->mZenGarden->IsStinkySleeping())
		{
			canClick = true;
		}

		GridItem* aStinky = mApp->mZenGarden->GetStinky();
		if (canClick && aStinky)
		{
			Rect aStinkyRect(aStinky->mPosX - 6, aStinky->mPosY - 10, 84, 90);
			if (aStinkyRect.Contains(x, y))
			{
				theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_STINKY;
				return true;
			}
		}
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		if (mCursorObject->mCursorType == CursorType::CURSOR_TYPE_TREE_FOOD && mChallenge->TreeOfWisdomHitTest(x, y, theHitResult))
		{
			return true;
		}
	}
	if ((mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM) && CanInteractWithBoardButtons())
	{
		for (int i = (int)GameObjectType::OBJECT_TYPE_WATERING_CAN; i <= (int)GameObjectType::OBJECT_TYPE_NEXT_GARDEN; i++)
		{
			GameObjectType aTool = (GameObjectType)i;
			if (CanUseGameObject(aTool) && (aTool != GameObjectType::OBJECT_TYPE_TREE_FOOD || mChallenge->TreeOfWisdomCanFeed()))
			{
				Rect aButtonRect = GetShovelButtonRect();
				GetZenButtonRect(aTool, aButtonRect);

				if (aButtonRect.Contains(x, y))
				{
					theHitResult->mObjectType = (GameObjectType)aTool;
					return true;
				}
			}
		}
	}

	if (MouseHitTestPlant(x, y, theHitResult))
		return true;

	if (mApp->IsScaryPotterLevel() &&
		mCursorObject->mCursorType == CursorType::CURSOR_TYPE_NORMAL &&
		mChallenge->mChallengeState != ChallengeState::STATECHALLENGE_SCARY_POTTER_MALLETING &&
		mApp->mGameScene == GameScenes::SCENE_PLAYING &&
		mApp->GetDialog(Dialogs::DIALOG_GAME_OVER) == nullptr &&
		mApp->GetDialog(Dialogs::DIALOG_CONTINUE) == nullptr)
	{
		GridItem* aScaryPot = GetGridItemAt(GridItemType::GRIDITEM_SCARY_POT, PixelToGridX(x, y), PixelToGridY(x, y));
		if (aScaryPot)
		{
			theHitResult->mObject = aScaryPot;
			theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_SCARY_POT;
			return true;
		}
	}

	if (mApp->IsSlotMachineLevel())
	{
		Rect aSlotMachineHandleRect = mChallenge->SlotMachineGetHandleRect();
		if (aSlotMachineHandleRect.Contains(x, y) && mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_NORMAL && !HasLevelAwardDropped())
		{
			theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_SLOT_MACHINE_HANDLE;
			return true;
		}
	}

	theHitResult->mObject = nullptr;
	theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_NONE;
	return false;
}

namespace
{
// Board::PickUpTool 分发表：表内匿名 lambda = 原 case 体逐句迁移（生成器 +
// 逆变换 token 对拍 0 差异）；default 具名并被缺槽/越界复用。
using PickUpToolCaseFn = void (*)(Board&);

// 原 default（无 case 的 12 型与表外哨兵共用）
void PickUpToolDefault(Board& b)
{
    		TOD_ASSERT(false);
}

// 匿名 lambda 函数指针表，行序 = GameObjectType 枚举值序（0..21）。
constexpr PickUpToolCaseFn kPickUpToolCases[(int)GameObjectType::OBJECT_TYPE_TREE_OF_WISDOM + 1] = {
    &PickUpToolDefault,  // 0 OBJECT_TYPE_NONE（原 default）
    &PickUpToolDefault,  // 1 OBJECT_TYPE_PLANT（原 default）
    &PickUpToolDefault,  // 2 OBJECT_TYPE_PROJECTILE（原 default）
    &PickUpToolDefault,  // 3 OBJECT_TYPE_COIN（原 default）
    &PickUpToolDefault,  // 4 OBJECT_TYPE_SEEDPACKET（原 default）
    // 原 case OBJECT_TYPE_SHOVEL
    [](Board& b)
    {
    		if (b.mTutorialState == TutorialState::TUTORIAL_SHOVEL_PICKUP)
    		{
    			b.SetTutorialState(TutorialState::TUTORIAL_SHOVEL_DIG);
    		}
    		b.mCursorObject->mCursorType = CursorType::CURSOR_TYPE_SHOVEL;
    		b.mApp->PlayFoley(FoleyType::FOLEY_SHOVEL);
    },  // 5 OBJECT_TYPE_SHOVEL
    // 原 case OBJECT_TYPE_WATERING_CAN
    [](Board& b)
    {
    		if (b.mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_PICKUP_WATER)
    		{
    			b.mTutorialState = TutorialState::TUTORIAL_ZEN_GARDEN_WATER_PLANT;
    			b.DisplayAdvice(gStr.ADVICE_ZEN_GARDEN_WATER_PLANT, MessageStyle::MESSAGE_STYLE_ZEN_GARDEN_LONG, AdviceType::ADVICE_NONE);
    			b.TutorialArrowRemove();
    		}
    		b.mCursorObject->mCursorType = CursorType::CURSOR_TYPE_WATERING_CAN;
    		b.mApp->PlayFoley(FoleyType::FOLEY_DROP);
    },  // 6 OBJECT_TYPE_WATERING_CAN
    // 原 case OBJECT_TYPE_FERTILIZER
    [](Board& b)
    {
    		if (b.mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_FERTILIZER] > PURCHASE_COUNT_OFFSET)
    		{
    			b.mCursorObject->mCursorType = CursorType::CURSOR_TYPE_FERTILIZER;
    			b.mApp->PlayFoley(FoleyType::FOLEY_DROP);
    		}
    		else
    		{
    			b.mApp->PlaySample(Sexy::SOUND_BUZZER);
    		}
    },  // 7 OBJECT_TYPE_FERTILIZER
    // 原 case OBJECT_TYPE_BUG_SPRAY
    [](Board& b)
    {
    		if (b.mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_BUG_SPRAY] > PURCHASE_COUNT_OFFSET)
    		{
    			b.mCursorObject->mCursorType = CursorType::CURSOR_TYPE_BUG_SPRAY;
    			b.mApp->PlayFoley(FoleyType::FOLEY_DROP);
    		}
    		else
    		{
    			b.mApp->PlaySample(Sexy::SOUND_BUZZER);
    		}
    },  // 8 OBJECT_TYPE_BUG_SPRAY
    // 原 case OBJECT_TYPE_PHONOGRAPH
    [](Board& b)
    {
    		b.mCursorObject->mCursorType = CursorType::CURSOR_TYPE_PHONOGRAPH;
    		b.mApp->PlayFoley(FoleyType::FOLEY_DROP);
    },  // 9 OBJECT_TYPE_PHONOGRAPH
    // 原 case OBJECT_TYPE_CHOCOLATE
    [](Board& b)
    {
    		if (b.mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_CHOCOLATE] > PURCHASE_COUNT_OFFSET)
    		{
    			b.mCursorObject->mCursorType = CursorType::CURSOR_TYPE_CHOCOLATE;
    			b.mApp->PlayFoley(FoleyType::FOLEY_DROP);
    		}
    		else
    		{
    			b.mApp->PlaySample(Sexy::SOUND_BUZZER);
    		}
    },  // 10 OBJECT_TYPE_CHOCOLATE
    // 原 case OBJECT_TYPE_GLOVE
    [](Board& b)
    {
    		b.mCursorObject->mCursorType = CursorType::CURSOR_TYPE_GLOVE;
    		b.mApp->PlayFoley(FoleyType::FOLEY_DROP);
    },  // 11 OBJECT_TYPE_GLOVE
    // 原 case OBJECT_TYPE_MONEY_SIGN
    [](Board& b)
    {
    		b.mCursorObject->mCursorType = CursorType::CURSOR_TYPE_MONEY_SIGN;
    		b.mApp->PlayFoley(FoleyType::FOLEY_DROP);
    },  // 12 OBJECT_TYPE_MONEY_SIGN
    // 原 case OBJECT_TYPE_WHEELBARROW
    [](Board& b)
    {
    		b.mCursorObject->mCursorType = CursorType::CURSOR_TYPE_WHEEELBARROW;
    		b.mApp->PlayFoley(FoleyType::FOLEY_DROP);
    },  // 13 OBJECT_TYPE_WHEELBARROW
    // 原 case OBJECT_TYPE_TREE_FOOD
    [](Board& b)
    {
    		if (b.mChallenge->TreeOfWisdomCanFeed())
    		{
    			if (b.mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_TREE_FOOD] > PURCHASE_COUNT_OFFSET)
    			{
    				b.mCursorObject->mCursorType = CursorType::CURSOR_TYPE_TREE_FOOD;
    				b.mApp->PlayFoley(FoleyType::FOLEY_DROP);
    			}
    			else
    			{
    				b.mApp->PlaySample(Sexy::SOUND_BUZZER);
    			}
    		}
    },  // 14 OBJECT_TYPE_TREE_FOOD
    &PickUpToolDefault,  // 15 OBJECT_TYPE_NEXT_GARDEN（原 default）
    &PickUpToolDefault,  // 16 OBJECT_TYPE_MENU_BUTTON（原 default）
    &PickUpToolDefault,  // 17 OBJECT_TYPE_STORE_BUTTON（原 default）
    &PickUpToolDefault,  // 18 OBJECT_TYPE_SLOT_MACHINE_HANDLE（原 default）
    &PickUpToolDefault,  // 19 OBJECT_TYPE_SCARY_POT（原 default）
    &PickUpToolDefault,  // 20 OBJECT_TYPE_STINKY（原 default）
    &PickUpToolDefault,  // 21 OBJECT_TYPE_TREE_OF_WISDOM（原 default）
};
static_assert(sizeof(kPickUpToolCases) / sizeof(kPickUpToolCases[0]) ==
    (int)GameObjectType::OBJECT_TYPE_TREE_OF_WISDOM + 1, "kPickUpToolCases 槽位数须与 GameObjectType 枚举同步");
}  // namespace

//0x411C10
void Board::PickUpTool(GameObjectType theObjectType)
{
	if (mPaused || (mApp->mGameScene != GameScenes::SCENE_PLAYING && !mCutScene->IsInShovelTutorial()))
		return;

	// C 形态 -> 匿名 lambda 函数指针表：行序 = GameObjectType 枚举序（0..21）。
	// default（TOD_ASSERT）具名并被缺槽与越界分支复用。
	const unsigned anObjectTypeIndex = static_cast<unsigned>(theObjectType);
	if (anObjectTypeIndex <= static_cast<unsigned>(GameObjectType::OBJECT_TYPE_TREE_OF_WISDOM))
	{
		kPickUpToolCases[anObjectTypeIndex](*this);
	}
	else
	{
		PickUpToolDefault(*this);
	}


	mCursorObject->mType = SeedType::SEED_NONE;
}

//0x411F20
void Board::MouseDown(int x, int y, int theClickCount)
{
	PrepareMouseSelection();
	RefreshHudLayout();
	// Move and click may arrive between updates; feeding tools use highlights.
	UpdatePointerHighlights(x, y);
	Widget::MouseDown(x, y, theClickCount);
	mIgnoreMouseUp = !CanInteractWithBoardButtons();
	if (mTimeStopCounter > 0)
		return;

	HitResult aHitResult;
	MouseHitTest(x, y, &aHitResult);
	if (mChallenge->MouseDown(x, y, theClickCount, &aHitResult))
		return;

	if (mMenuButton->HitTest(x, y, this) && CanInteractWithBoardButtons() && theClickCount > 0)
	{
		mApp->PlaySample(Sexy::SOUND_GRAVEBUTTON);
	}
	else if (mStoreButton && mStoreButton->HitTest(x, y, this) && CanInteractWithBoardButtons() && theClickCount > 0)
	{
		if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
		{
			mApp->PlaySample(Sexy::SOUND_TAP);
		}
		else if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND)
		{
			mApp->PlaySample(Sexy::SOUND_GRAVEBUTTON);
		}
	}

	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && mApp->mSeedChooserScreen)
	{
		mApp->mSeedChooserScreen->CancelLawnView();
	}
	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
	{
		mCutScene->ZombieWonClick();
		return;
	}
	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
	{
		mCutScene->MouseDown(x, y);
	}

	if (mApp->mTodCheatKeys && !mApp->IsScaryPotterLevel() && mNextSurvivalStageCounter > 0)
	{
		mNextSurvivalStageCounter = 2;
		for (int i = 0; i < MAX_GRID_SIZE_Y; i++)
		{
			if (mIceTimer[i] > 2)
			{
				mIceTimer[i] = 2;
			}
		}
	}

	CursorType aCursor = mCursorObject->mCursorType;
	if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_NONE)
	{
		if (aCursor == CURSOR_TYPE_COBCANNON_TARGET)
		{
			MouseDownCobcannonFire(x, y, theClickCount);
			UpdateCursor();
			return;
		}
	}
	else if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_COIN && theClickCount >= 0)
	{
		Coin* aCoin = (Coin*)aHitResult.mObject;
		if (aCoin->mBoard)
		{
			aCoin->MouseDown(x, y, theClickCount);
		}
		UpdateCursor();
		return;
	}

	if (aCursor == CursorType::CURSOR_TYPE_SHOVEL ||
		aCursor == CursorType::CURSOR_TYPE_WATERING_CAN ||
		aCursor == CursorType::CURSOR_TYPE_FERTILIZER ||
		aCursor == CursorType::CURSOR_TYPE_BUG_SPRAY ||
		aCursor == CursorType::CURSOR_TYPE_PHONOGRAPH ||
		aCursor == CursorType::CURSOR_TYPE_CHOCOLATE ||
		aCursor == CursorType::CURSOR_TYPE_GLOVE ||
		aCursor == CursorType::CURSOR_TYPE_MONEY_SIGN ||
		aCursor == CursorType::CURSOR_TYPE_WHEEELBARROW ||
		aCursor == CursorType::CURSOR_TYPE_TREE_FOOD)
	{
		MouseDownWithTool(x, y, theClickCount, aCursor);
	}
	else if (IsPlantInCursor())
	{
		MouseDownWithPlant(x, y, theClickCount);
	}
	else if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_SEEDPACKET)
	{
		if (!mPaused)
		{
			((SeedPacket*)aHitResult.mObject)->MouseDown(x, y, theClickCount);
		}
	}
	else if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_NEXT_GARDEN)
	{
		if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
		{
			mApp->mZenGarden->GotoNextGarden();
		}
		else if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
		{
			mChallenge->TreeOfWisdomNextGarden();
		}
		mApp->PlaySample(Sexy::SOUND_TAP);
	}
	else if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_SHOVEL ||
		aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_WATERING_CAN ||
		aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_FERTILIZER ||
		aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_BUG_SPRAY ||
		aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_PHONOGRAPH ||
		aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_CHOCOLATE ||
		aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_GLOVE ||
		aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_MONEY_SIGN ||
		aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_WHEELBARROW ||
		aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_TREE_FOOD)
	{
		PickUpTool(aHitResult.mObjectType);
	}
	else if (aHitResult.mObjectType == GameObjectType::OBJECT_TYPE_PLANT)
	{
		((Plant*)aHitResult.mObject)->MouseDown(x, y, theClickCount);
	}

	UpdateCursor();
}

namespace
{
// 教程态回退表（B 形态）：mTutorialState 直下标（0..16）；哨兵 kTutorialReturnNone = 无转换 = 原链落空。
// mNextReady/mNextNotReady = 卡槽 1 CanPickUp() 真/假时的目标态（无条件转换两栏相同）。
constexpr TutorialState kTutorialReturnNone = (TutorialState)-1;
struct TutorialReturnRow
{
	TutorialState mNextReady;
	TutorialState mNextNotReady;
};
constexpr TutorialReturnRow kTutorialReturn[TutorialState::TUTORIAL_SHOVEL_DIG + 1] = {
	/* [0]  TUTORIAL_OFF                                                       */ { kTutorialReturnNone, kTutorialReturnNone },
	/* [1]  TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER                                */ { kTutorialReturnNone, kTutorialReturnNone },
	/* [2]  TUTORIAL_LEVEL_1_PLANT_PEASHOOTER（无条件回退）                     */ { TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER, TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER },
	/* [3]  TUTORIAL_LEVEL_1_REFRESH_PEASHOOTER                                */ { kTutorialReturnNone, kTutorialReturnNone },
	/* [4]  TUTORIAL_LEVEL_1_COMPLETED                                         */ { kTutorialReturnNone, kTutorialReturnNone },
	/* [5]  TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER                                 */ { kTutorialReturnNone, kTutorialReturnNone },
	/* [6]  TUTORIAL_LEVEL_2_PLANT_SUNFLOWER                                   */ { TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER, TutorialState::TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER },
	/* [7]  TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER                                 */ { TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER, TutorialState::TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER },
	/* [8]  TUTORIAL_LEVEL_2_COMPLETED                                         */ { kTutorialReturnNone, kTutorialReturnNone },
	/* [9]  TUTORIAL_MORESUN_PICK_UP_SUNFLOWER                                 */ { kTutorialReturnNone, kTutorialReturnNone },
	/* [10] TUTORIAL_MORESUN_PLANT_SUNFLOWER                                   */ { TutorialState::TUTORIAL_MORESUN_PICK_UP_SUNFLOWER, TutorialState::TUTORIAL_MORESUN_REFRESH_SUNFLOWER },
	/* [11] TUTORIAL_MORESUN_REFRESH_SUNFLOWER                                 */ { TutorialState::TUTORIAL_MORESUN_PICK_UP_SUNFLOWER, TutorialState::TUTORIAL_MORESUN_REFRESH_SUNFLOWER },
	/* [12] TUTORIAL_MORESUN_COMPLETED                                         */ { kTutorialReturnNone, kTutorialReturnNone },
	/* [13] TUTORIAL_SLOT_MACHINE_PULL                                         */ { kTutorialReturnNone, kTutorialReturnNone },
	/* [14] TUTORIAL_SLOT_MACHINE_COMPLETED                                    */ { kTutorialReturnNone, kTutorialReturnNone },
	/* [15] TUTORIAL_SHOVEL_PICKUP                                             */ { kTutorialReturnNone, kTutorialReturnNone },
	/* [16] TUTORIAL_SHOVEL_DIG（无条件回退）                                   */ { TutorialState::TUTORIAL_SHOVEL_PICKUP, TutorialState::TUTORIAL_SHOVEL_PICKUP },
};
static_assert(sizeof(kTutorialReturn) / sizeof(kTutorialReturn[0]) == TutorialState::TUTORIAL_SHOVEL_DIG + 1,
	"教程回退表槽位须锚定 TUTORIAL_SHOVEL_DIG");
}  // namespace

//0x412330
void Board::ClearCursor()
{
	if (mAdvice->mEndTimeNs != 0)
	{
		if (mHelpIndex == AdviceType::ADVICE_PLANT_GRAVEBUSTERS_ON_GRAVES ||
			mHelpIndex == AdviceType::ADVICE_PLANT_LILYPAD_ON_WATER ||
			mHelpIndex == AdviceType::ADVICE_PLANT_TANGLEKELP_ON_WATER ||
			mHelpIndex == AdviceType::ADVICE_PLANT_SEASHROOM_ON_WATER ||
			mHelpIndex == AdviceType::ADVICE_PLANT_POTATOE_MINE_ON_LILY ||
			mHelpIndex == AdviceType::ADVICE_PLANT_WRONG_ART_TYPE ||
			mHelpIndex == AdviceType::ADVICE_PLANT_NEED_POT ||
			mHelpIndex == AdviceType::ADVICE_PLANT_NOT_PASSED_LINE ||
			mHelpIndex == AdviceType::ADVICE_PLANT_ONLY_ON_REPEATERS ||
			mHelpIndex == AdviceType::ADVICE_PLANT_ONLY_ON_MELONPULT ||
			mHelpIndex == AdviceType::ADVICE_PLANT_ONLY_ON_SUNFLOWER ||
			mHelpIndex == AdviceType::ADVICE_PLANT_ONLY_ON_SPIKEWEED ||
			mHelpIndex == AdviceType::ADVICE_PLANT_ONLY_ON_KERNELPULT)
		{
			ClearAdvice(mHelpIndex);
		}
	}

	mCursorObject->mType = SeedType::SEED_NONE;
	mCursorObject->mCursorType = CursorType::CURSOR_TYPE_NORMAL;
	mCursorObject->mSeedBankIndex = -1;
	mCursorObject->mCoinID = CoinID::COINID_NULL;
	mCursorObject->mDuplicatorPlantID = PlantID::PLANTID_NULL;
	mCursorObject->mCobCannonPlantID = PlantID::PLANTID_NULL;
	mCursorObject->mGlovePlantID = PlantID::PLANTID_NULL;
	mApp->SetCursor(CURSOR_POINTER);
	mChallenge->ClearCursor();

	// 教程态回退（B 形态 -> 常量表 上方 kTutorialReturn，mTutorialState 直下标；
	// 哨兵 = 无转换 = 原链落空；两栏相同的无条件转换不评估 CanPickUp）
	if ((unsigned)mTutorialState < sizeof(kTutorialReturn) / sizeof(kTutorialReturn[0]))
	{
		const TutorialReturnRow& aReturn = kTutorialReturn[mTutorialState];
		if (aReturn.mNextReady != kTutorialReturnNone)
		{
			SetTutorialState(aReturn.mNextReady == aReturn.mNextNotReady || mSeedBank->mSeedPackets[1].CanPickUp()
				? aReturn.mNextReady
				: aReturn.mNextNotReady);
		}
	}
}

//0x412490
bool Board::CanInteractWithBoardButtons()
{
	if (mBoardFadeOutCounter >= 0)
		return false;

	if (mPaused || mApp->GetDialogCount() > 0)
		return false;

	if (mCursorObject->mCursorType != CursorType::CURSOR_TYPE_NORMAL &&
		mCursorObject->mCursorType != CursorType::CURSOR_TYPE_HAMMER &&
		mCursorObject->mCursorType != CursorType::CURSOR_TYPE_COBCANNON_TARGET)
		return false;

	if (mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_ZEN_FADING)
		return false;

	return mApp->mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_OFF;
}

namespace
{
// 商店按钮点击动作表（B/C 形态折衷——动作枚举表）：GameMode 直下标，None = 原链落空（不响应）。
// 异构动作体保留在调用点 switch（不改头文件签名）；原 UPSELL 分支（else if (false)）已删，
// 对应 GAMEMODE_UPSELL 枚举不存在，无槽可挂。
enum class StoreButtonAction
{
	None,
	OpenZenStore,   // 禅境花园：清提示并开商店
	OpenTreeStore,  // 智慧树：开商店
	StartLastStand, // 坚不可摧：进入激战并隐藏按钮
};
constexpr StoreButtonAction kStoreButtonAction[GameMode::NUM_GAME_MODES] = {
	/* [ 0] GAMEMODE_ADVENTURE  (v1 0) */ StoreButtonAction::None,
	/* [ 1] GAMEMODE_MINIGAME_BEGHOULED  (v1 20) */ StoreButtonAction::None,
	/* [ 2] GAMEMODE_MINIGAME_BEGHOULED_TWIST  (v1 24) */ StoreButtonAction::None,
	/* [ 3] GAMEMODE_MINIGAME_BOBSLED_BONANZA  (v1 28) */ StoreButtonAction::None,
	/* [ 4] GAMEMODE_MINIGAME_COLUMN  (v1 27) */ StoreButtonAction::None,
	/* [ 5] GAMEMODE_MINIGAME_FINAL_BOSS  (v1 35) */ StoreButtonAction::None,
	/* [ 6] GAMEMODE_MINIGAME_INVISIGHOUL  (v1 21) */ StoreButtonAction::None,
	/* [ 7] GAMEMODE_MINIGAME_LAST_STAND  (v1 31) */ StoreButtonAction::StartLastStand,
	/* [ 8] GAMEMODE_MINIGAME_LITTLE_TROUBLE  (v1 25) */ StoreButtonAction::None,
	/* [ 9] GAMEMODE_MINIGAME_POGO_PARTY  (v1 34) */ StoreButtonAction::None,
	/* [10] GAMEMODE_MINIGAME_PORTAL_COMBAT  (v1 26) */ StoreButtonAction::None,
	/* [11] GAMEMODE_MINIGAME_RAINING_SEEDS  (v1 19) */ StoreButtonAction::None,
	/* [12] GAMEMODE_MINIGAME_SEEING_STARS  (v1 22) */ StoreButtonAction::None,
	/* [13] GAMEMODE_MINIGAME_SLOT_MACHINE  (v1 18) */ StoreButtonAction::None,
	/* [14] GAMEMODE_MINIGAME_SPEED  (v1 29) */ StoreButtonAction::None,
	/* [15] GAMEMODE_MINIGAME_WALLNUT_BOWLING  (v1 17) */ StoreButtonAction::None,
	/* [16] GAMEMODE_MINIGAME_WALLNUT_BOWLING_2  (v1 33) */ StoreButtonAction::None,
	/* [17] GAMEMODE_MINIGAME_WAR_AND_PEAS  (v1 16) */ StoreButtonAction::None,
	/* [18] GAMEMODE_MINIGAME_WAR_AND_PEAS_2  (v1 32) */ StoreButtonAction::None,
	/* [19] GAMEMODE_MINIGAME_WHACK_A_ZOMBIE  (v1 30) */ StoreButtonAction::None,
	/* [20] GAMEMODE_MINIGAME_ZOMBIQUARIUM  (v1 23) */ StoreButtonAction::None,
	/* [21] GAMEMODE_LIMBO_AIR_RAID  (v1 41) */ StoreButtonAction::None,
	/* [22] GAMEMODE_LIMBO_ART_CHALLENGE_SUNFLOWER  (v1 40) */ StoreButtonAction::None,
	/* [23] GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT  (v1 36) */ StoreButtonAction::None,
	/* [24] GAMEMODE_LIMBO_BIG_TIME  (v1 39) */ StoreButtonAction::None,
	/* [25] GAMEMODE_LIMBO_BUNGEE_BLITZ  (v1 48) */ StoreButtonAction::None,
	/* [26] GAMEMODE_LIMBO_GRAVE_DANGER  (v1 45) */ StoreButtonAction::None,
	/* [27] GAMEMODE_LIMBO_HIGH_GRAVITY  (v1 44) */ StoreButtonAction::None,
	/* [28] GAMEMODE_LIMBO_ICE  (v1 42) */ StoreButtonAction::None,
	/* [29] GAMEMODE_LIMBO_RESODDED  (v1 38) */ StoreButtonAction::None,
	/* [30] GAMEMODE_LIMBO_SHOVEL  (v1 46) */ StoreButtonAction::None,
	/* [31] GAMEMODE_LIMBO_SQUIRREL  (v1 49) */ StoreButtonAction::None,
	/* [32] GAMEMODE_LIMBO_STORMY_NIGHT  (v1 47) */ StoreButtonAction::None,
	/* [33] GAMEMODE_LIMBO_SUNNY_DAY  (v1 37) */ StoreButtonAction::None,
	/* [34] GAMEMODE_LIMBO_ZEN_GARDEN  (v1 43) */ StoreButtonAction::OpenZenStore,
	/* [35] GAMEMODE_INTRO  (v1 71) */ StoreButtonAction::None,
	/* [36] GAMEMODE_PUZZLE_I_ZOMBIE_1  (v1 61) */ StoreButtonAction::None,
	/* [37] GAMEMODE_PUZZLE_I_ZOMBIE_2  (v1 62) */ StoreButtonAction::None,
	/* [38] GAMEMODE_PUZZLE_I_ZOMBIE_3  (v1 63) */ StoreButtonAction::None,
	/* [39] GAMEMODE_PUZZLE_I_ZOMBIE_4  (v1 64) */ StoreButtonAction::None,
	/* [40] GAMEMODE_PUZZLE_I_ZOMBIE_5  (v1 65) */ StoreButtonAction::None,
	/* [41] GAMEMODE_PUZZLE_I_ZOMBIE_6  (v1 66) */ StoreButtonAction::None,
	/* [42] GAMEMODE_PUZZLE_I_ZOMBIE_7  (v1 67) */ StoreButtonAction::None,
	/* [43] GAMEMODE_PUZZLE_I_ZOMBIE_8  (v1 68) */ StoreButtonAction::None,
	/* [44] GAMEMODE_PUZZLE_I_ZOMBIE_9  (v1 69) */ StoreButtonAction::None,
	/* [45] GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS  (v1 70) */ StoreButtonAction::None,
	/* [46] GAMEMODE_SCARY_POTTER_1  (v1 51) */ StoreButtonAction::None,
	/* [47] GAMEMODE_SCARY_POTTER_2  (v1 52) */ StoreButtonAction::None,
	/* [48] GAMEMODE_SCARY_POTTER_3  (v1 53) */ StoreButtonAction::None,
	/* [49] GAMEMODE_SCARY_POTTER_4  (v1 54) */ StoreButtonAction::None,
	/* [50] GAMEMODE_SCARY_POTTER_5  (v1 55) */ StoreButtonAction::None,
	/* [51] GAMEMODE_SCARY_POTTER_6  (v1 56) */ StoreButtonAction::None,
	/* [52] GAMEMODE_SCARY_POTTER_7  (v1 57) */ StoreButtonAction::None,
	/* [53] GAMEMODE_SCARY_POTTER_8  (v1 58) */ StoreButtonAction::None,
	/* [54] GAMEMODE_SCARY_POTTER_9  (v1 59) */ StoreButtonAction::None,
	/* [55] GAMEMODE_SCARY_POTTER_ENDLESS  (v1 60) */ StoreButtonAction::None,
	/* [56] GAMEMODE_SURVIVAL_ENDLESS_STAGE_1  (v1 11) */ StoreButtonAction::None,
	/* [57] GAMEMODE_SURVIVAL_ENDLESS_STAGE_2  (v1 12) */ StoreButtonAction::None,
	/* [58] GAMEMODE_SURVIVAL_ENDLESS_STAGE_3  (v1 13) */ StoreButtonAction::None,
	/* [59] GAMEMODE_SURVIVAL_ENDLESS_STAGE_4  (v1 14) */ StoreButtonAction::None,
	/* [60] GAMEMODE_SURVIVAL_ENDLESS_STAGE_5  (v1 15) */ StoreButtonAction::None,
	/* [61] GAMEMODE_SURVIVAL_HARD_STAGE_1  (v1 6) */ StoreButtonAction::None,
	/* [62] GAMEMODE_SURVIVAL_HARD_STAGE_2  (v1 7) */ StoreButtonAction::None,
	/* [63] GAMEMODE_SURVIVAL_HARD_STAGE_3  (v1 8) */ StoreButtonAction::None,
	/* [64] GAMEMODE_SURVIVAL_HARD_STAGE_4  (v1 9) */ StoreButtonAction::None,
	/* [65] GAMEMODE_SURVIVAL_HARD_STAGE_5  (v1 10) */ StoreButtonAction::None,
	/* [66] GAMEMODE_SURVIVAL_NORMAL_STAGE_1  (v1 1) */ StoreButtonAction::None,
	/* [67] GAMEMODE_SURVIVAL_NORMAL_STAGE_2  (v1 2) */ StoreButtonAction::None,
	/* [68] GAMEMODE_SURVIVAL_NORMAL_STAGE_3  (v1 3) */ StoreButtonAction::None,
	/* [69] GAMEMODE_SURVIVAL_NORMAL_STAGE_4  (v1 4) */ StoreButtonAction::None,
	/* [70] GAMEMODE_SURVIVAL_NORMAL_STAGE_5  (v1 5) */ StoreButtonAction::None,
	/* [71] GAMEMODE_TREE_OF_WISDOM  (v1 50) */ StoreButtonAction::OpenTreeStore,
	/* [72] GAMEMODE_CUSTOM_LEVEL（哨兵；策略层覆写）*/ StoreButtonAction::None,
};
static_assert(sizeof(kStoreButtonAction) / sizeof(kStoreButtonAction[0]) == (int)GameMode::NUM_GAME_MODES,
	"商店按钮动作表槽位须与 GameMode 枚举尾同步");
// 关键槽锚（2026-10-04 B 树实机抓获禅园商店按钮无响应：曾把禅园排进 [23]=
// 水族箱槽——真实枚举值 43；GameSelector ButtonDepress 槽位错位同款事故，
// 当时立的"关键槽锚"规矩在此表漏执行）
static_assert(kStoreButtonAction[(int)GameMode::GAMEMODE_ADVENTURE] == StoreButtonAction::None &&
	kStoreButtonAction[(int)GameMode::GAMEMODE_LIMBO_ZEN_GARDEN] == StoreButtonAction::OpenZenStore &&
	kStoreButtonAction[(int)GameMode::GAMEMODE_MINIGAME_LAST_STAND] == StoreButtonAction::StartLastStand &&
	kStoreButtonAction[(int)GameMode::GAMEMODE_TREE_OF_WISDOM] == StoreButtonAction::OpenTreeStore &&
	kStoreButtonAction[(int)GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM] == StoreButtonAction::None,
	"kStoreButtonAction 关键槽位错位");
}  // namespace

//0x412540
void Board::MouseUp(int x, int y, int theClickCount)
{
	RefreshHudLayout();
	Widget::MouseUp(x, y, theClickCount);
	if (mIgnoreMouseUp)
	{
		mIgnoreMouseUp = false;
		return;
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED && mChallenge->MouseUp(x, y) && theClickCount > 0)
		return;

	if (CanInteractWithBoardButtons() && theClickCount > 0)
	{
		if (mMenuButton->HitTest(x, y, this) && !mApp->GetDialog(Dialogs::DIALOG_GAME_OVER) && !mApp->GetDialog(Dialogs::DIALOG_LEVEL_COMPLETE))
		{
			mMenuButton->mIsOver = false;
			mMenuButton->mIsDown = false;
			UpdateCursor();
			ClearCursor();
			if (mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_COMPLETED)
			{
				mApp->FinishZenGardenToturial();
			}
			else if (mApp->mGameMode != GAMEMODE_LIMBO_ZEN_GARDEN && mApp->mGameMode != GAMEMODE_TREE_OF_WISDOM)
			{
				mApp->PlaySample(Sexy::SOUND_PAUSE);
				mApp->DoNewOptions(false);
			}
			else
			{
				mApp->mBoardResult = BoardResult::BOARDRESULT_QUIT;
				mApp->DoBackToMain();
			}
		}
		else if(mStoreButton && mStoreButton->HitTest(x, y, this))
		{
			// B 形态 -> 动作枚举表（上方 kStoreButtonAction，GameMode 直下标）；
			// 异构动作体保留 switch；原 UPSELL 死分支（else if (false)）随表移除（见上表注）
			switch (kStoreButtonAction[(int)mApp->mGameMode])
			{
			case StoreButtonAction::OpenZenStore:
				ClearAdviceImmediately();
				mApp->mZenGarden->OpenStore();
				break;
			case StoreButtonAction::OpenTreeStore:
				mChallenge->TreeOfWisdomOpenStore();
				break;
			case StoreButtonAction::StartLastStand:
				mChallenge->mChallengeState = ChallengeState::STATECHALLENGE_LAST_STAND_ONSLAUGHT;
				mStoreButton->mBtnNoDraw = true;
				mStoreButton->mDisabled = true;
				mZombieCountDown = 10;
				mZombieCountDownStart = 10;
				break;
			case StoreButtonAction::None:
			default:
				break;
			}
		}
	}
}

//0x4127A0
void Board::Pause(bool thePause)
{
	if (mPaused == thePause)
		return;
    if (thePause && mWidgetManager) mWidgetManager->CancelTouches(this);

	mPaused = thePause;
	if (thePause && mApp->mPlayerInfo->mCoins > 0)
	{
		mCoinBankFadeCount = 1000;
	}

	if (!thePause || mApp->mGameScene != GameScenes::SCENE_LEVEL_INTRO)
	{
		mApp->mSoundSystem->GamePause(thePause);
		mApp->mMusic->GameMusicPause(thePause);
	}
}

//0x412850
int Board::GetGraveStonesCount()
{
	int aCount = 0;

	GridItem* aGridItem = nullptr;
	while (IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_GRAVESTONE)
		{
			aCount++;
		}
	}

	return aCount;
}

//0x412890
void Board::PickSpecialGraveStone()
{
	GridItem* aGridItem = nullptr;
	GridItem* aPicks[MAX_GRAVE_STONES];
	int aPickCount = 0;
	while (IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_GRAVESTONE)
		{
			TOD_ASSERT(aPickCount < MAX_GRAVE_STONES);
			aPicks[aPickCount] = aGridItem;
			aPickCount++;
		}
	}

	if (aPickCount > 0)
	{
		TodPickFromArray(aPicks, aPickCount)->mGridItemState = GridItemState::GRIDITEM_STATE_GRAVESTONE_SPECIAL;
	}
}

//0x4128F0
void Board::SpawnZombiesFromPool()
{
	if (mIceTrapCounter > 0)
		return;

	int aCount, aZombiePoints;
	if (mLevel == 21 || mLevel == 22 || mLevel == 31 || mLevel == 32)
	{
		aCount = 2;
		aZombiePoints = 3;
	}
	else if (mLevel == 23 || mLevel == 24 || mLevel == 25 || mLevel == 33 || mLevel == 34 || mLevel == 35)
	{
		aCount = 3;
		aZombiePoints = 5;
	}
	else
	{
		aCount = 3;
		aZombiePoints = 7;
	}

	int aGridArrayCount = 0;
	TodWeightedGridArray aGridArray[MAX_POOL_GRID_SIZE];
	for (int aGridX = 5; aGridX < MAX_GRID_SIZE_X; aGridX++)
	{
		for (int aGridY = 2; aGridY <= 3; aGridY++)
		{
			aGridArray[aGridArrayCount].mX = aGridX;
			aGridArray[aGridArrayCount].mY = aGridY;
			aGridArray[aGridArrayCount].mWeight = 10000;
			aGridArrayCount++;
			TOD_ASSERT(aGridArrayCount <= MAX_POOL_GRID_SIZE);
		}
	}

	if (aGridArrayCount < 0)
	{
		aGridArrayCount = 0;
	}
	for (int i = 0; i < aCount; i++)
	{
		TodWeightedGridArray* aGrid = TodPickFromWeightedGridArray(aGridArray, aGridArrayCount);
		aGrid->mWeight = 0;

		ZombieType aZombieType = PickGraveRisingZombieType();
		Zombie* aZombie = AddZombieInRow(aZombieType, aGrid->mY, mCurrentWave);
		if (aZombie == nullptr)
		{
			return;
		}

		aZombie->RiseFromGrave(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), aGrid->mX, aGrid->mY);
		aZombiePoints -= GetZombieDefinition(aZombieType).mZombieValue;
		if (aZombiePoints < 1)
		{
			aZombiePoints = 1;
		}
	}
}

//0x412A90
void Board::SetupBungeeDrop(BungeeDropGrid* theBungeeDropGrid)
{
	theBungeeDropGrid->mGridArrayCount = 0;
	for (int aGridX = 4; aGridX < MAX_GRID_SIZE_X; aGridX++)
	{
		for (int aGridY = 0; aGridY <= 4; aGridY++)
		{
			int aCount = theBungeeDropGrid->mGridArrayCount;
			theBungeeDropGrid->mGridArray[aCount].mX = aGridX;
			theBungeeDropGrid->mGridArray[aCount].mY = aGridY;
			theBungeeDropGrid->mGridArray[aCount].mWeight = 10000;
			theBungeeDropGrid->mGridArrayCount++;
			TOD_ASSERT((size_t)theBungeeDropGrid->mGridArrayCount <= LENGTH(theBungeeDropGrid->mGridArray));
		}
	}
}

//0x412B60
void Board::BungeeDropZombie(BungeeDropGrid* theBungeeDropGrid, ZombieType theZombieType)
{
	TodWeightedGridArray* aGrid = TodPickFromWeightedGridArray(theBungeeDropGrid->mGridArray, theBungeeDropGrid->mGridArrayCount);
	aGrid->mWeight = 1;

	Zombie* aBungeeZombie = AddZombie(ZombieType::ZOMBIE_BUNGEE, mCurrentWave);
	Zombie* aZombie = AddZombie(theZombieType, mCurrentWave);
	TOD_ASSERT(aBungeeZombie && aZombie);

	aBungeeZombie->BungeeDropZombie(static_cast<std::int64_t>(mLevelTimeNs), aZombie, aGrid->mX, aGrid->mY);
}

//0x412C30
void Board::SpawnZombiesFromSky()
{
	if (mIceTrapCounter > 0)
		return;

	int aCount, aZombiePoints;
	if (mLevel == 41 || mLevel == 42)
	{
		aCount = 2;
		aZombiePoints = 3;
	}
	else if (mLevel == 43 || mLevel == 44 || mLevel == 45)
	{
		aCount = 3;
		aZombiePoints = 5;
	}
	else
	{
		aCount = 3;
		aZombiePoints = 7;
	}

	BungeeDropGrid aBungeeDropGrid;
	SetupBungeeDrop(&aBungeeDropGrid);
	if (aCount > aBungeeDropGrid.mGridArrayCount)
	{
		aCount = aBungeeDropGrid.mGridArrayCount;
	}

	if (aBungeeDropGrid.mGridArrayCount == 0 || aCount <= 0)
		return;

	for (int i = 0; i < aCount; i++)
	{
		ZombieType aZombieType = PickGraveRisingZombieType();
		BungeeDropZombie(&aBungeeDropGrid, aZombieType);
		aZombiePoints -= GetZombieDefinition(aZombieType).mZombieValue;
		if (aZombiePoints < 1)
		{
			aZombiePoints = 1;
		}
	}
}

//0x412CE0
void Board::SpawnZombiesFromGraves()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_WAR_AND_PEAS || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_WAR_AND_PEAS_2)
		return;

	if (StageHasRoof())
	{
		SpawnZombiesFromSky();
	}
	else if (StageHasPool())
	{
		SpawnZombiesFromPool();
		return;
	}

//	int aZombiePoints = GetGraveStonesCount();
	GridItem* aGridItem = nullptr;
	while (IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType != GridItemType::GRIDITEM_GRAVESTONE || static_cast<std::int64_t>(mLevelTimeNs) - aGridItem->mStateStartTimeNs < 1'000'000'000)
		{
			continue;
		}
		if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_GRAVE_DANGER && Rand(mNumWaves) > mCurrentWave)
		{
			continue;
		}

		ZombieType aZombieType = PickGraveRisingZombieType();
		Zombie* aZombie = AddZombie(aZombieType, mCurrentWave);
		if (aZombie == nullptr)
		{
			return;
		}

		aZombie->RiseFromGrave(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), aGridItem->mGridX, aGridItem->mGridY);
		/*
		aZombiePoints -= GetZombieDefinition(aZombieType).mZombieValue;
		if (aZombieType < 1)
		{
			aZombiePoints = 1;
		}
		*/
	}
}

//0x412E30
int Board::TotalZombiesHealthInWave(int theWaveIndex)
{
	int aTotalHealth = 0;
	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		if (aZombie->mFromWave == theWaveIndex && !aZombie->mMindControlled && !aZombie->IsDeadOrDying() &&
			aZombie->mZombieType != ZombieType::ZOMBIE_BUNGEE && aZombie->mRelatedZombieID == ZombieID::ZOMBIEID_NULL)
		{
			aTotalHealth += aZombie->mBodyHealth + aZombie->mHelmHealth + aZombie->mShieldHealth * 0.2f + aZombie->mFlyingHealth;
		}
	}
	return aTotalHealth;
}

//0x412EE0
void Board::SpawnZombieWave()
{
	// Whack-a-Zombie owns its countdown, wave index, and grave-bound spawns.
	// Reaching the ordinary wave builder here means the dispatch contract was violated.
	TOD_ASSERT(!mApp->IsWhackAZombieLevel());

	mChallenge->SpawnZombieWave();
	if (mApp->IsBungeeBlitzLevel())
	{
		BungeeDropGrid aBungeeDropGrid;
		SetupBungeeDrop(&aBungeeDropGrid);
		for (int i = 0; i < MAX_ZOMBIES_IN_WAVE; i++)
		{
			ZombieType aZombieType = mZombiesInWave[mCurrentWave][i];
			if (aZombieType == ZombieType::ZOMBIE_INVALID)
				break;

			if (aZombieType == ZombieType::ZOMBIE_BUNGEE || aZombieType == ZombieType::ZOMBIE_ZAMBONI)
			{
				AddZombie(aZombieType, mCurrentWave);
			}
			else
			{
				BungeeDropZombie(&aBungeeDropGrid, aZombieType);
			}
		}
	}
	else
	{
		TOD_ASSERT(mCurrentWave >= 0 && mCurrentWave < MAX_ZOMBIE_WAVES && mCurrentWave < mNumWaves);
		for (int i = 0; i < MAX_ZOMBIES_IN_WAVE; i++)
		{
			ZombieType aZombieType = mZombiesInWave[mCurrentWave][i];
			if (aZombieType == ZombieType::ZOMBIE_INVALID)
				break;

			if (aZombieType == ZombieType::ZOMBIE_BOBSLED && !CanAddBobSled())
			{
				for (int i = 0; i < MAX_ZOMBIE_FOLLOWERS; i++)
				{
					AddZombie(ZombieType::ZOMBIE_NORMAL, mCurrentWave);  // 生成 4 只普通僵尸以代替雪橇僵尸小队
				}
			}
			else
			{
				AddZombie(aZombieType, mCurrentWave);
			}
		}
	}

	if (mCurrentWave == mNumWaves - 1 && !mApp->IsContinuousChallenge())
	{
		// GOTY 1.2.0.1073 Board::SpawnZombieWave @ 0x00415870.
		mRiseFromGraveCounter = 200;
	}
	if (IsFlagWave(mCurrentWave))
	{
		mFlagRaiseCounter = FLAG_RAISE_TIME;
	}
	mCurrentWave++;
	mTotalSpawnedWaves++;
}

//0x4130D0
ReanimTimeDomain Board::AnimationTimeDomain() const
{
	// GOTY 1073 stops normal battle time on entering LEVEL_INTRO, including
	// the stationary interval before the repick camera starts moving.
	return mApp->mGameScene == GameScenes::SCENE_PLAYING || mCutScene->ShouldRunUpsellBoard()
		? ReanimTimeDomain::Battle : ReanimTimeDomain::Visual;
}

void Board::UpdateGameObjects(std::uint64_t theLevelDeltaNs)
{
	const auto ownerNowNs = static_cast<std::int64_t>(mLevelTimeNs);
	const auto visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	// Repick freezes retained actors, not the intro's UI/camera effects.
	const bool survivalRepick = mApp->IsSurvivalMode() && mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO;
	const auto animationDomain = survivalRepick ? ReanimTimeDomain::Battle : AnimationTimeDomain();
	const auto animationNowNs = animationDomain == ReanimTimeDomain::Battle ? ownerNowNs : visualNowNs;
	auto& holder = *mApp->mEffectSystem->mReanimationHolder;
	const auto bindAnimation = [&](ReanimationID id, ReanimTimeDomain domain, std::int64_t nowNs) {
		if (auto* animation=mApp->ReanimationTryToGet(id))
			holder.BindAnimationTime(*animation, animation->mIsAttachment ? domain : ReanimTimeDomain::Visual,
				animation->mIsAttachment ? nowNs : visualNowNs);
	};
	Plant* aPlant = nullptr;
	while (IteratePlants(aPlant))
	{
		for (const auto id : {aPlant->mBodyReanimID,aPlant->mHeadReanimID,aPlant->mHeadReanimID2,
			aPlant->mHeadReanimID3,aPlant->mBlinkReanimID,aPlant->mLightReanimID,aPlant->mSleepingReanimID})
			bindAnimation(id,animationDomain,animationNowNs);
		aPlant->Update(ownerNowNs, visualNowNs, animationNowNs, theLevelDeltaNs);
	}

	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		const bool onBoard = aZombie->IsOnBoard();
		const auto domain = onBoard ? animationDomain : ReanimTimeDomain::Visual;
		const auto nowNs = onBoard ? animationNowNs : visualNowNs;
		for (const auto id : {aZombie->mBodyReanimID,aZombie->mSpecialHeadReanimID,aZombie->mZombatarHeadReanimID,
			aZombie->mMoweredReanimID,aZombie->mBossFireBallReanimID}) bindAnimation(id,domain,nowNs);
		if (auto* attachment=mApp->mEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet(
			static_cast<unsigned int>(aZombie->mAttachmentID)))
			for (int i=0;i<attachment->mNumEffects;++i)
				if (attachment->mEffectArray[i].mEffectType==EffectType::EFFECT_REANIM)
					if (auto* animation=holder.mReanimations.DataArrayTryToGet(attachment->mEffectArray[i].mEffectID))
						holder.BindAnimationTime(*animation,onBoard ? ReanimTimeDomain::Battle : ReanimTimeDomain::Visual,
							onBoard ? ownerNowNs : visualNowNs);
		aZombie->Update(onBoard ? ownerNowNs : visualNowNs, visualNowNs,
			nowNs, onBoard ? theLevelDeltaNs : Sexy::gUpdateIntervalNs);
	}

	Projectile* aProjectile = nullptr;
	while (!survivalRepick && IterateProjectiles(aProjectile))
	{
		aProjectile->Update();
	}

	Coin* aCoin = nullptr;
	while (IterateCoins(aCoin))
	{
		aCoin->Update();
	}

	LawnMower* aLawnMower = nullptr;
	while (!survivalRepick && IterateLawnMowers(aLawnMower))
	{
		aLawnMower->Update();
	}

	mCursorPreview->Update();
	mCursorObject->Update();
    UpdateTouchCursors();

	for (int i = 0; !survivalRepick && i < mSeedBank->mNumPackets; i++)
	{
		mSeedBank->mSeedPackets[i].Update();
	}
}

//0x413220
void Board::StopAllZombieSounds()
{
	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		aZombie->StopZombieSound();
	}
}

//0x413260
// GOTY @Patoke: 0x415BD0
int Board::GetSurvivalFlagsCompleted()
{
	int aWavesPerFlag = GetNumWavesPerFlag();
	int aFlagsCompleted = mChallenge->mSurvivalStage * GetNumWavesPerSurvivalStage() / aWavesPerFlag;
	int aCurrentWave = mCurrentWave;
	if (IsFlagWave(aCurrentWave - 1) && mBoardFadeOutCounter < 0 && !mNextSurvivalStageCounter)
	{
		aCurrentWave -= 1;
	}
	return aCurrentWave / aWavesPerFlag + aFlagsCompleted;
}

//0x413320
void Board::SurvivalSaveScore()
{
	if (!mApp->IsSurvivalMode())
		return;

	int aFlagsCompleted = GetSurvivalFlagsCompleted();
	int& aFlagsRecord = mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()];
	if (aFlagsCompleted > aFlagsRecord)
	{
		aFlagsRecord = aFlagsCompleted;
		mApp->WriteCurrentUserConfig();
	}
}

//0x413390
void Board::PuzzleSaveStreak()
{
	if (!mApp->IsEndlessIZombie(mApp->mGameMode) && !mApp->IsEndlessScaryPotter(mApp->mGameMode))
		return;

	int aStreak = mChallenge->mSurvivalStage + 1;
	int& aRecord = mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()];
	if (aStreak > aRecord)
	{
		aRecord = aStreak;
		mApp->WriteCurrentUserConfig();
	}
}

namespace
{
// 僵尸进屋结算类型表（B/C 形态折衷——类型枚举表）：GameMode 直下标。
// [70] PUZZLE_I_ZOMBIE_ENDLESS 与 [60] SCARY_POTTER_ENDLESS → EndlessPuzzleMsg
// （原链 Endless 判定先于 IsIZombieLevel，次序并入槽位）；其余（含冒险）→ Cutscene = 原末支 else。
enum class ZombiesWonKind
{
	Cutscene,
	ZombiquariumMsg,
	LastStandMsg,
	EndlessPuzzleMsg,
	IZombieMsg,
};
constexpr ZombiesWonKind kZombiesWonKind[GameMode::NUM_GAME_MODES] = {
	/* [ 0] GAMEMODE_ADVENTURE  (v1 0) */ ZombiesWonKind::Cutscene,
	/* [ 1] GAMEMODE_MINIGAME_BEGHOULED  (v1 20) */ ZombiesWonKind::Cutscene,
	/* [ 2] GAMEMODE_MINIGAME_BEGHOULED_TWIST  (v1 24) */ ZombiesWonKind::Cutscene,
	/* [ 3] GAMEMODE_MINIGAME_BOBSLED_BONANZA  (v1 28) */ ZombiesWonKind::Cutscene,
	/* [ 4] GAMEMODE_MINIGAME_COLUMN  (v1 27) */ ZombiesWonKind::Cutscene,
	/* [ 5] GAMEMODE_MINIGAME_FINAL_BOSS  (v1 35) */ ZombiesWonKind::Cutscene,
	/* [ 6] GAMEMODE_MINIGAME_INVISIGHOUL  (v1 21) */ ZombiesWonKind::Cutscene,
	/* [ 7] GAMEMODE_MINIGAME_LAST_STAND  (v1 31) */ ZombiesWonKind::LastStandMsg,
	/* [ 8] GAMEMODE_MINIGAME_LITTLE_TROUBLE  (v1 25) */ ZombiesWonKind::Cutscene,
	/* [ 9] GAMEMODE_MINIGAME_POGO_PARTY  (v1 34) */ ZombiesWonKind::Cutscene,
	/* [10] GAMEMODE_MINIGAME_PORTAL_COMBAT  (v1 26) */ ZombiesWonKind::Cutscene,
	/* [11] GAMEMODE_MINIGAME_RAINING_SEEDS  (v1 19) */ ZombiesWonKind::Cutscene,
	/* [12] GAMEMODE_MINIGAME_SEEING_STARS  (v1 22) */ ZombiesWonKind::Cutscene,
	/* [13] GAMEMODE_MINIGAME_SLOT_MACHINE  (v1 18) */ ZombiesWonKind::Cutscene,
	/* [14] GAMEMODE_MINIGAME_SPEED  (v1 29) */ ZombiesWonKind::Cutscene,
	/* [15] GAMEMODE_MINIGAME_WALLNUT_BOWLING  (v1 17) */ ZombiesWonKind::Cutscene,
	/* [16] GAMEMODE_MINIGAME_WALLNUT_BOWLING_2  (v1 33) */ ZombiesWonKind::Cutscene,
	/* [17] GAMEMODE_MINIGAME_WAR_AND_PEAS  (v1 16) */ ZombiesWonKind::Cutscene,
	/* [18] GAMEMODE_MINIGAME_WAR_AND_PEAS_2  (v1 32) */ ZombiesWonKind::Cutscene,
	/* [19] GAMEMODE_MINIGAME_WHACK_A_ZOMBIE  (v1 30) */ ZombiesWonKind::Cutscene,
	/* [20] GAMEMODE_MINIGAME_ZOMBIQUARIUM  (v1 23) */ ZombiesWonKind::ZombiquariumMsg,
	/* [21] GAMEMODE_LIMBO_AIR_RAID  (v1 41) */ ZombiesWonKind::Cutscene,
	/* [22] GAMEMODE_LIMBO_ART_CHALLENGE_SUNFLOWER  (v1 40) */ ZombiesWonKind::Cutscene,
	/* [23] GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT  (v1 36) */ ZombiesWonKind::Cutscene,
	/* [24] GAMEMODE_LIMBO_BIG_TIME  (v1 39) */ ZombiesWonKind::Cutscene,
	/* [25] GAMEMODE_LIMBO_BUNGEE_BLITZ  (v1 48) */ ZombiesWonKind::Cutscene,
	/* [26] GAMEMODE_LIMBO_GRAVE_DANGER  (v1 45) */ ZombiesWonKind::Cutscene,
	/* [27] GAMEMODE_LIMBO_HIGH_GRAVITY  (v1 44) */ ZombiesWonKind::Cutscene,
	/* [28] GAMEMODE_LIMBO_ICE  (v1 42) */ ZombiesWonKind::Cutscene,
	/* [29] GAMEMODE_LIMBO_RESODDED  (v1 38) */ ZombiesWonKind::Cutscene,
	/* [30] GAMEMODE_LIMBO_SHOVEL  (v1 46) */ ZombiesWonKind::Cutscene,
	/* [31] GAMEMODE_LIMBO_SQUIRREL  (v1 49) */ ZombiesWonKind::Cutscene,
	/* [32] GAMEMODE_LIMBO_STORMY_NIGHT  (v1 47) */ ZombiesWonKind::Cutscene,
	/* [33] GAMEMODE_LIMBO_SUNNY_DAY  (v1 37) */ ZombiesWonKind::Cutscene,
	/* [34] GAMEMODE_LIMBO_ZEN_GARDEN  (v1 43) */ ZombiesWonKind::Cutscene,
	/* [35] GAMEMODE_INTRO  (v1 71) */ ZombiesWonKind::Cutscene,
	/* [36] GAMEMODE_PUZZLE_I_ZOMBIE_1  (v1 61) */ ZombiesWonKind::IZombieMsg,
	/* [37] GAMEMODE_PUZZLE_I_ZOMBIE_2  (v1 62) */ ZombiesWonKind::IZombieMsg,
	/* [38] GAMEMODE_PUZZLE_I_ZOMBIE_3  (v1 63) */ ZombiesWonKind::IZombieMsg,
	/* [39] GAMEMODE_PUZZLE_I_ZOMBIE_4  (v1 64) */ ZombiesWonKind::IZombieMsg,
	/* [40] GAMEMODE_PUZZLE_I_ZOMBIE_5  (v1 65) */ ZombiesWonKind::IZombieMsg,
	/* [41] GAMEMODE_PUZZLE_I_ZOMBIE_6  (v1 66) */ ZombiesWonKind::IZombieMsg,
	/* [42] GAMEMODE_PUZZLE_I_ZOMBIE_7  (v1 67) */ ZombiesWonKind::IZombieMsg,
	/* [43] GAMEMODE_PUZZLE_I_ZOMBIE_8  (v1 68) */ ZombiesWonKind::IZombieMsg,
	/* [44] GAMEMODE_PUZZLE_I_ZOMBIE_9  (v1 69) */ ZombiesWonKind::IZombieMsg,
	/* [45] GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS  (v1 70) */ ZombiesWonKind::EndlessPuzzleMsg,
	/* [46] GAMEMODE_SCARY_POTTER_1  (v1 51) */ ZombiesWonKind::Cutscene,
	/* [47] GAMEMODE_SCARY_POTTER_2  (v1 52) */ ZombiesWonKind::Cutscene,
	/* [48] GAMEMODE_SCARY_POTTER_3  (v1 53) */ ZombiesWonKind::Cutscene,
	/* [49] GAMEMODE_SCARY_POTTER_4  (v1 54) */ ZombiesWonKind::Cutscene,
	/* [50] GAMEMODE_SCARY_POTTER_5  (v1 55) */ ZombiesWonKind::Cutscene,
	/* [51] GAMEMODE_SCARY_POTTER_6  (v1 56) */ ZombiesWonKind::Cutscene,
	/* [52] GAMEMODE_SCARY_POTTER_7  (v1 57) */ ZombiesWonKind::Cutscene,
	/* [53] GAMEMODE_SCARY_POTTER_8  (v1 58) */ ZombiesWonKind::Cutscene,
	/* [54] GAMEMODE_SCARY_POTTER_9  (v1 59) */ ZombiesWonKind::Cutscene,
	/* [55] GAMEMODE_SCARY_POTTER_ENDLESS  (v1 60) */ ZombiesWonKind::EndlessPuzzleMsg,
	/* [56] GAMEMODE_SURVIVAL_ENDLESS_STAGE_1  (v1 11) */ ZombiesWonKind::Cutscene,
	/* [57] GAMEMODE_SURVIVAL_ENDLESS_STAGE_2  (v1 12) */ ZombiesWonKind::Cutscene,
	/* [58] GAMEMODE_SURVIVAL_ENDLESS_STAGE_3  (v1 13) */ ZombiesWonKind::Cutscene,
	/* [59] GAMEMODE_SURVIVAL_ENDLESS_STAGE_4  (v1 14) */ ZombiesWonKind::Cutscene,
	/* [60] GAMEMODE_SURVIVAL_ENDLESS_STAGE_5  (v1 15) */ ZombiesWonKind::Cutscene,
	/* [61] GAMEMODE_SURVIVAL_HARD_STAGE_1  (v1 6) */ ZombiesWonKind::Cutscene,
	/* [62] GAMEMODE_SURVIVAL_HARD_STAGE_2  (v1 7) */ ZombiesWonKind::Cutscene,
	/* [63] GAMEMODE_SURVIVAL_HARD_STAGE_3  (v1 8) */ ZombiesWonKind::Cutscene,
	/* [64] GAMEMODE_SURVIVAL_HARD_STAGE_4  (v1 9) */ ZombiesWonKind::Cutscene,
	/* [65] GAMEMODE_SURVIVAL_HARD_STAGE_5  (v1 10) */ ZombiesWonKind::Cutscene,
	/* [66] GAMEMODE_SURVIVAL_NORMAL_STAGE_1  (v1 1) */ ZombiesWonKind::Cutscene,
	/* [67] GAMEMODE_SURVIVAL_NORMAL_STAGE_2  (v1 2) */ ZombiesWonKind::Cutscene,
	/* [68] GAMEMODE_SURVIVAL_NORMAL_STAGE_3  (v1 3) */ ZombiesWonKind::Cutscene,
	/* [69] GAMEMODE_SURVIVAL_NORMAL_STAGE_4  (v1 4) */ ZombiesWonKind::Cutscene,
	/* [70] GAMEMODE_SURVIVAL_NORMAL_STAGE_5  (v1 5) */ ZombiesWonKind::Cutscene,
	/* [71] GAMEMODE_TREE_OF_WISDOM  (v1 50) */ ZombiesWonKind::Cutscene,
	/* [72] GAMEMODE_CUSTOM_LEVEL（哨兵；策略层覆写）*/ ZombiesWonKind::Cutscene,
};
static_assert(sizeof(kZombiesWonKind) / sizeof(kZombiesWonKind[0]) == (int)GameMode::NUM_GAME_MODES,
	"僵尸进屋结算类型表槽位须与 GameMode 枚举尾同步");
}  // namespace

//0x413400
void Board::ZombiesWon(Zombie* theZombie)
{
	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
		return;

	ClearAdvice(AdviceType::ADVICE_NONE);
	mApp->mBoardResult = BoardResult::BOARDRESULT_LOST;

	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		if (aZombie == theZombie)
			continue;

		if (aZombie->GetZombieRect().mX < -50 ||
			aZombie->mZombiePhase == ZombiePhase::PHASE_RISING_FROM_GRAVE ||
			aZombie->mZombiePhase == ZombiePhase::PHASE_DANCER_RISING)
		{
			if ((aZombie->mZombieType == ZombieType::ZOMBIE_GARGANTUAR || aZombie->mZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR) &&
				aZombie->IsDeadOrDying() && (aZombie->mX) < 140)
			{
				aZombie->DieNoLoot(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
			}
		}
	}
	SurvivalSaveScore();

	SexyString aGameOverMsg;
	// B 形态 -> 类型枚举表（上方 kZombiesWonKind，GameMode 直下标）；异构消息体/过场体保留 switch
	switch (kZombiesWonKind[(int)mApp->mGameMode])
	{
	case ZombiesWonKind::ZombiquariumMsg:
		aGameOverMsg = gStr.ZOMBIQUARIUM_DEATH_MESSAGE;
		break;
	case ZombiesWonKind::LastStandMsg:
	{
		SexyString aFlagStr = mApp->Pluralize(GetSurvivalFlagsCompleted(), gStr.ONE_FLAG.c_str(), gStr.COUNT_FLAGS.c_str());
		aGameOverMsg = TodReplaceString(gStr.LAST_STAND_DEATH_MESSAGE, UiTextPlaceholder::Flags, aFlagStr);
		break;
	}
	case ZombiesWonKind::EndlessPuzzleMsg:
		aGameOverMsg = TodReplaceNumberString(gStr.ENDLESS_PUZZLE_DEATH_MESSAGE, UiTextPlaceholder::Streak, mChallenge->mSurvivalStage);
		break;
	case ZombiesWonKind::IZombieMsg:
		aGameOverMsg = gStr.I_ZOMBIE_DEATH_MESSAGE;
		break;
	case ZombiesWonKind::Cutscene:
	default:
	{
		mApp->mGameScene = GameScenes::SCENE_ZOMBIES_WON;
		if (theZombie)  // 原版此处没有对 theZombie 进行空指针判断，但加上判断后便允许绕过僵尸而直接调用游戏失败
		{
			theZombie->WalkIntoHouse(static_cast<std::int64_t>(mLevelTimeNs));
		}

		ClearAdvice(AdviceType::ADVICE_NONE);
		mCutScene->StartZombiesWon();
		FreezeEffectsForCutscene(true);
		TutorialArrowRemove();
		UpdateCursor();
		return;
	}
	}

	GameOverDialog* aGameOverDialog = new GameOverDialog(aGameOverMsg, true);
	mApp->AddDialog(Dialogs::DIALOG_GAME_OVER, aGameOverDialog);

	mApp->mMusic->StopAllMusic();
	StopAllZombieSounds();
	mApp->PlaySample(Sexy::SOUND_LOSEMUSIC);

	ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIES_WON, true);
	Reanimation* aReanim = mApp->AddReanimation(-BOARD_OFFSET, 0, MakeRenderOrder(RenderLayer::RENDER_LAYER_SCREEN_FADE, 0, 0), ReanimationType::REANIM_ZOMBIES_WON, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), AnimationTimeDomain());
	aReanim->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
	aReanim->GetTrackInstanceByName(ReanimTrackId::fullscreen)->mTrackColor = Color::Black;
	aReanim->SetFramesForLayer(ReanimTrackId::anim_screen);
}

//0x4138D0
bool Board::IsFinalScaryPotterStage()
{
	if (!mApp->IsScaryPotterLevel())
		return false;

	if (mApp->IsAdventureMode())
	{
		return mChallenge->mSurvivalStage == 2;
	}

	return !mApp->IsEndlessScaryPotter(mApp->mGameMode);
}

//0x413920
bool Board::IsFinalSurvivalStage()
{
	if (!mApp->IsSurvivalMode())
		return false;

	int aFlags = GetNumWavesPerSurvivalStage() * (mChallenge->mSurvivalStage + 1) / GetNumWavesPerFlag();
	if (mApp->IsSurvivalNormal(mApp->mGameMode))
	{
		return aFlags >= 5;
	}
	if (mApp->IsSurvivalHard(mApp->mGameMode))
	{
		return aFlags >= 10;
	}

	return false;
}

bool Board::IsLastStandFinalStage()
{
	return mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND && mChallenge->mSurvivalStage == LAST_STAND_FLAGS - 1;
}

//0x4139E0
bool Board::IsSurvivalStageWithRepick()
{
	return mApp->IsSurvivalMode() && !IsFinalSurvivalStage();
}

//0x413A10
bool Board::IsLastStandStageWithRepick()
{
	return mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND && !IsLastStandFinalStage();
}

//0x413A40
bool Board::HasLevelAwardDropped()
{
	return mLevelAwardSpawned || mNextSurvivalStageCounter > 0 || mBoardFadeOutCounter >= 0;
}

//0x413A70
void Board::UpdateSunSpawning()
{
	if (StageIsNight() ||
		HasLevelAwardDropped() ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_RAINING_SEEDS ||
		mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ICE ||
		mApp->mGameMode == GameMode::GAMEMODE_INTRO ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM ||
		mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND ||
		mApp->IsIZombieLevel() ||
		mApp->IsScaryPotterLevel() ||
		mApp->IsSquirrelLevel() ||
		HasConveyorBeltSeedBank() ||
		mTutorialState == TutorialState::TUTORIAL_SLOT_MACHINE_PULL)
		return;

	if (mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER || mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PLANT_PEASHOOTER)
	{
		if (mPlants.mSize == 0)
		{
			return;
		}
	}

	mSunCountDown--;
	if (mSunCountDown != 0)
		return;

	mNumSunsFallen++;
	mSunCountDown = std::min(SUN_COUNTDOWN_MAX, SUN_COUNTDOWN + mNumSunsFallen * 10) + Rand(SUN_COUNTDOWN_RANGE);
	CoinType aSunType = mApp->mGameMode == GameMode::GAMEMODE_LIMBO_SUNNY_DAY ? CoinType::COIN_LARGESUN : CoinType::COIN_SUN;
	AddCoin(RandRangeInt(100, 649), 60, aSunType, CoinMotion::COIN_MOTION_FROM_SKY);
}

//0x413C00
void Board::NextWaveComing()
{
	if (mCurrentWave + 1 == mNumWaves)
	{
		if (!IsSurvivalStageWithRepick() && mApp->mGameMode != GameMode::GAMEMODE_MINIGAME_LAST_STAND && !mApp->IsContinuousChallenge())
		{
			// The authored animation is centered in the original 800px board.
			// Battle viewports can extend asymmetrically, so use their actual center.
			const auto& aBounds = mApp->mScreenBounds;
			const float aBannerX = aBounds.mX - mX + (aBounds.mWidth - BOARD_WIDTH) * 0.5f;
			mApp->AddReanimation(aBannerX, 30, MakeRenderOrder(RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0), ReanimationType::REANIM_FINAL_WAVE, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), AnimationTimeDomain());
			mFinalWaveSoundCounter = 60;
		}
	}

	if (mCurrentWave == 0)
	{
		mApp->PlaySample(Sexy::SOUND_AWOOGA);
	}
	else if (mApp->IsWhackAZombieLevel())
	{
		if (mCurrentWave == mNumWaves - 1)
		{
			mApp->PlaySample(Sexy::SOUND_SIREN);
		}
	}
	else if (IsFlagWave(mCurrentWave))
	{
		mApp->PlaySample(Sexy::SOUND_SIREN);
	}
}

//0x413D00
void Board::UpdateZombieSpawning()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_INTRO)
		return;

	if (mFinalWaveSoundCounter > 0)
	{
		mFinalWaveSoundCounter--;
		if (mFinalWaveSoundCounter == 0)
		{
			mApp->PlaySample(Sexy::SOUND_FINALWAVE);
		}
	}

	if (mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER ||
		mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PLANT_PEASHOOTER ||
		mTutorialState == TutorialState::TUTORIAL_LEVEL_1_REFRESH_PEASHOOTER ||
		mTutorialState == TutorialState::TUTORIAL_SLOT_MACHINE_PULL)
		return;

	if (HasLevelAwardDropped())
		return;

	if (mRiseFromGraveCounter > 0)
	{
		mRiseFromGraveCounter--;
		if (mRiseFromGraveCounter == 0)
		{
			SpawnZombiesFromGraves();
		}
	}

	if (mHugeWaveCountDown > 0)
	{
		mHugeWaveCountDown--;
		if (mHugeWaveCountDown == 0)
		{
			ClearAdvice(AdviceType::ADVICE_HUGE_WAVE);
			NextWaveComing();
			mZombieCountDown = 1;
		}
		else
		{
			if (mHugeWaveCountDown == 725)
			{
				mApp->PlaySample(Sexy::SOUND_HUGE_WAVE);
			}
			else
			{
				if (mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_DAY_GRASSWALK ||
					mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_POOL_WATERYGRAVES ||
					mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_FOG_RIGORMORMIST ||
					mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_ROOF_GRAZETHEROOF)
				{
					if (mHugeWaveCountDown == 400)
					{
						mApp->mMusic->StartBurst();
					}
				}
				else if (mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_NIGHT_MOONGRAINS)
				{
					if (mHugeWaveCountDown == 700)
					{
						mApp->mMusic->StartBurst();
					}
				}
				return;
			}
		}
	}

	const ZombieSpawningUpdateResult aChallengeResult = mChallenge->UpdateZombieSpawning();
	if (aChallengeResult == ZombieSpawningUpdateResult::HANDLED_BY_CHALLENGE)
		return;

	UpdateZombieWaveScheduler();
}

void Board::UpdateZombieWaveScheduler()
{
	if (mCurrentWave == mNumWaves)
	{
		if (IsFinalSurvivalStage())
		{
			return;
		}
		if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND)
		{
			return;
		}
		if (!mApp->IsSurvivalMode() && !mApp->IsContinuousChallenge())
		{
			return;
		}
	}

	mZombieCountDown--;
	if (mCurrentWave == mNumWaves && mApp->IsSurvivalMode())
	{
		if (mZombieCountDown == 0)
		{
			FadeOutLevel();
		}
		return;
	}

	if (mZombieCountDown > 200 && mZombieCountDownStart - mZombieCountDown > 400 && TotalZombiesHealthInWave(mCurrentWave - 1) <= mZombieHealthToNextWave)
	{
		mZombieCountDown = 200;
	}
	if (mZombieCountDown == 5)
	{
		if (IsFlagWave(mCurrentWave))
		{
			ClearAdviceImmediately();
			DisplayAdviceAgain(gStr.ADVICE_HUGE_WAVE, MessageStyle::MESSAGE_STYLE_HUGE_WAVE, AdviceType::ADVICE_HUGE_WAVE);
			mHugeWaveCountDown = 750;
			return;
		}
		NextWaveComing();
	}
	if (mZombieCountDown == 0)
	{
		SpawnZombieWave();
		mZombieHealthWaveStart = TotalZombiesHealthInWave(mCurrentWave - 1);

		if (mCurrentWave == mNumWaves && mApp->IsSurvivalMode())
		{
			mZombieHealthToNextWave = 0;
			mZombieCountDown = ZOMBIE_COUNTDOWN_BEFORE_REPICK + 1;
		}
		else if (IsFlagWave(mCurrentWave) && !(mApp->IsWallnutBowlingLevel() || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND))
		{
			mZombieHealthToNextWave = 0;
			mZombieCountDown = ZOMBIE_COUNTDOWN_BEFORE_FLAG;
		}
		else
		{
			mZombieHealthToNextWave = RandRangeFloat(0.5f, 0.65f) * mZombieHealthWaveStart;
			if (mApp->IsLittleTroubleLevel() || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_COLUMN || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND)
			{
				mZombieCountDown = 750;
			}
			else
			{
				mZombieCountDown = ZOMBIE_COUNTDOWN + Rand(ZOMBIE_COUNTDOWN_RANGE);
			}
		}
		mZombieCountDownStart = mZombieCountDown;
	}
}

//0x414100
void Board::UpdateIce()
{
	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		if (mIceTimer[aRow])
		{
			mIceTimer[aRow]--;
			TodParticleSystem* aParticleIce = mApp->ParticleTryToGet(mIceParticleID[aRow]);
			if (mIceTimer[aRow] == 0)
			{
				mIceMinX[aRow] = BOARD_ICE_START;
				if (aParticleIce)
				{
					aParticleIce->ParticleSystemDie();
				}
			}
			else
			{
				float aPosX = mIceMinX[aRow];
				float aPosY = GridToPixelY(8, aRow);
				if (aParticleIce)
				{
					aParticleIce->SystemMove(aPosX, aPosY);
				}
				else
				{
					int aRenderPosition = MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, aRow, 3);
					aParticleIce = mApp->AddTodParticle(aPosX, aPosY, aRenderPosition, ParticleEffect::PARTICLE_ICE_SPARKLE, static_cast<std::int64_t>(mLevelTimeNs));
					mIceParticleID[aRow] = mApp->ParticleGetID(aParticleIce);
				}
			}

			int anAlpha = ClampInt(mIceTimer[aRow] / 10, 0, 255);
			aParticleIce->OverrideColor(ParticleEmitterRanges::All, Color(255, 255, 255, anAlpha));
		}
	}
}

//0x414270
void Board::UpdateProgressMeter(std::uint64_t theDeltaNs)
{
	if (mApp->IsFinalBossLevel())
	{
		Zombie* aBoss = GetBossZombie();
		if (aBoss && !aBoss->IsDeadOrDying())
		{
			mProgressMeterWidth = 150 * (aBoss->mBodyMaxHealth - aBoss->mBodyHealth) / aBoss->mBodyMaxHealth;
		}
		else
		{
			mProgressMeterWidth = 150;
		}
	}
	else if (mCurrentWave != 0)
	{
		// 更新旗帜升起倒计时
		if (mFlagRaiseCounter > 0)
			mFlagRaiseCounter--;

        // Width only changes on these boundaries. Avoid scanning the current
        // wave's health and dividing progress geometry on intervening updates.
        const auto aPreviousTimeNs = mLevelTimeNs - theDeltaNs;
        const bool aFastBoundary = aPreviousTimeNs / 50'000'000 != mLevelTimeNs / 50'000'000;
        const bool aSlowBoundary = aPreviousTimeNs / 200'000'000 != mLevelTimeNs / 200'000'000;
        if (!aFastBoundary && !aSlowBoundary)
            return;

		int aTotalWidth = 150;  // 可用于平均分配给每一小波的进度条总长度
		int aNumWavesPerFlag = GetNumWavesPerFlag();  // 本关卡中每相邻两个旗帜波之前的小波数量
		bool aHasFlags = ProgressMeterHasFlags();  // 进度条标注旗帜时，旗帜波占用更长的进度条
		if (aHasFlags)
		{
			aTotalWidth -= 12 * mNumWaves / aNumWavesPerFlag;  // 从每个旗帜波分割出 12 单位的长度
		}

		int aWaveLength = aTotalWidth / (mNumWaves - 1);  // 每一小波占用的进度条长度
		int aCurrentWaveLength = (mCurrentWave - 1) * aTotalWidth / (mNumWaves - 1);  // 当前波开始时的进度条长度
		int aNextWaveLength = mCurrentWave * aTotalWidth / (mNumWaves - 1);  // 下一波开始时的进度条长度
		if (aHasFlags)
		{
			int anExtraLength = mCurrentWave / aNumWavesPerFlag * 12;  // 归还已刷新的旗帜波分割的长度
			aCurrentWaveLength += anExtraLength;
			aNextWaveLength += anExtraLength;
		}

		// 根据倒计时初步计算当前波已经过的比例
		float aFraction = (mZombieCountDownStart - mZombieCountDown) / (float)mZombieCountDownStart;
		if (mZombieHealthToNextWave != -1)
		{
			// 取得本波僵尸的当前血量
			int aHealthCurrent = TotalZombiesHealthInWave(mCurrentWave - 1);
			// 取得（本波开始时的僵尸总血量 - 下一波刷新时的僵尸总血量），即：本波刷新需要对僵尸造成的伤害
			int aDamageTarget = mZombieHealthWaveStart - mZombieHealthToNextWave;  //开始时的血量 - 刷新时的血量
			if (aDamageTarget < 1)
			{
				aDamageTarget = 1;  // 需要的伤害至少为 1
			}
			// 再次以刷新血量计算一次当前波已经过的比例
			// 血量比例 = [目标伤害 - (当前血量 - 刷新血量)] / 目标伤害 = (目标伤害 - 仍需造成的伤害) / 目标伤害 = 当前伤害 / 目标伤害
			float aHealthFraction = (aDamageTarget - aHealthCurrent + mZombieHealthToNextWave) / (float)aDamageTarget;
			// 最终比例取上述二者的较大值
			aFraction = std::max(aHealthFraction, aFraction);
		}

		// 计算当前应当的进度条长度，并将长度的范围限定在 [1, 150] 之间
		int aLength = ClampInt(aCurrentWaveLength + FloatRoundToInt((aNextWaveLength - aCurrentWaveLength) * aFraction), 1, 150);
		// 取得当前实际与理论的进度条长度之差
		int aDelta = aLength - mProgressMeterWidth;
		// 当差值不超过一波的长度时，每 20cs 调整一次长度；否则，每 5cs 调整一次长度
		if ((aDelta > aWaveLength && aFastBoundary) || (aDelta > 0 && aSlowBoundary))
		{
			mProgressMeterWidth++;
		}
	}
}

//0x4144E0
void Board::UpdateTutorial()
{
	if (mTutorialTimer > 0)
		mTutorialTimer--;

	if (mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER && mTutorialTimer == 0)
	{
		DisplayAdvice(gStr.ADVICE_CLICK_PEASHOOTER, MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY, AdviceType::ADVICE_NONE);
		// 1073 00416FB7/00416FBD read the first packet's position.
		const auto card = LandscapeHud::PacketBounds(*this,0);
        TutorialArrowShow(mLandscapeHud ? card.mX-mApp->mScreenBounds.mX : mSeedBank->mX+mSeedBank->mSeedPackets[0].mX,
                          mLandscapeHud ? card.mY : mSeedBank->mY+mSeedBank->mSeedPackets[0].mY);
		mTutorialTimer = -1;
	}
	else if (mTutorialState == TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER ||
		mTutorialState == TutorialState::TUTORIAL_LEVEL_2_PLANT_SUNFLOWER ||
		mTutorialState == TutorialState::TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER)
	{
		if (mTutorialTimer == 0)
		{
			DisplayAdvice(gStr.ADVICE_PLANT_SUNFLOWER2, MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL2, AdviceType::ADVICE_NONE);
			mTutorialTimer = -1;
		}
		else if (mZombieCountDown == 750 && mCurrentWave == 0)
		{
			DisplayAdvice(gStr.ADVICE_PLANT_SUNFLOWER3, MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL2, AdviceType::ADVICE_NONE);
		}
	}
	else if (mTutorialState == TutorialState::TUTORIAL_MORESUN_PICK_UP_SUNFLOWER ||
		mTutorialState == TutorialState::TUTORIAL_MORESUN_PLANT_SUNFLOWER ||
		mTutorialState == TutorialState::TUTORIAL_MORESUN_REFRESH_SUNFLOWER)
	{
		if (mTutorialTimer == 0)
		{
			DisplayAdvice(gStr.ADVICE_PLANT_SUNFLOWER5, MessageStyle::MESSAGE_STYLE_TUTORIAL_LATER, AdviceType::ADVICE_PLANT_SUNFLOWER5);
			mTutorialTimer = -1;
		}
	}

	// 冒险模式初期关卡，检测到向日葵数量小于 3 时，进入“更多向日葵”的教程
	if (mApp->IsFirstTimeAdventureMode() && mLevel >= 3 && mLevel != 5 && mLevel <= 7 && mTutorialState == TutorialState::TUTORIAL_OFF &&
		mCurrentWave >= 5 && !sShownMoreSunTutorial && mSeedBank->mSeedPackets[1].CanPickUp() && CountPlantByType(SeedType::SEED_SUNFLOWER) < 3)
	{
		TOD_ASSERT(!ChooseSeedsOnCurrentLevel());
		DisplayAdvice(gStr.ADVICE_PLANT_SUNFLOWER4, MessageStyle::MESSAGE_STYLE_TUTORIAL_LATER_STAY, AdviceType::ADVICE_NONE);
		sShownMoreSunTutorial = true;
		SetTutorialState(TutorialState::TUTORIAL_MORESUN_PICK_UP_SUNFLOWER);
		mTutorialTimer = 500;
	}
}

namespace
{
// Board::SetTutorialState 分发表：表内匿名 lambda = 原 case 体逐句迁移
//（生成器 + 逆变换 token 对拍 0 差异）；default 空操作具名补缺槽。
using TutorialStateCaseFn = void (*)(Board&);

// 标签共体（匿名 lambda 无法跨槽复用，复用即具名）
// 原 case TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER, TUTORIAL_MORESUN_PICK_UP_SUNFLOWER
void TutorialCaseTutorialLevel2PickUpSunflower(Board& b)
{
		const auto card = LandscapeHud::PacketBounds(b,1);
            b.TutorialArrowShow(b.mLandscapeHud ? card.mX-b.mApp->mScreenBounds.mX : b.mSeedBank->mX+b.mSeedBank->mSeedPackets[1].mX,
                                b.mLandscapeHud ? card.mY : b.mSeedBank->mY+b.mSeedBank->mSeedPackets[1].mY);
}

// 标签共体（匿名 lambda 无法跨槽复用，复用即具名）
// 原 case TUTORIAL_LEVEL_2_PLANT_SUNFLOWER, TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER, TUTORIAL_MORESUN_PLANT_SUNFLOWER, TUTORIAL_MORESUN_REFRESH_SUNFLOWER
void TutorialCaseTutorialLevel2PlantSunflower(Board& b)
{
    		b.TutorialArrowRemove();
}

// 原 default（无 case 的 14 型（空操作）共用）
void TutorialCaseDefault(Board& b)
{

}

// 匿名 lambda 函数指针表，行序 = TutorialState 枚举值序（0..30）。
constexpr TutorialStateCaseFn kTutorialStateCases[(int)TutorialState::TUTORIAL_WHACK_A_ZOMBIE_COMPLETED + 1] = {
    &TutorialCaseDefault,  // 0 TUTORIAL_OFF（原 default）
    // 原 case TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER
    [](Board& b)
    {
    		if (b.mPlants.mSize == 0)
    		{
    			float aPosX = b.mSeedBank->mX + b.mSeedBank->mSeedPackets[0].mX;
    			float aPosY = b.mSeedBank->mY + b.mSeedBank->mSeedPackets[0].mY;
			if (b.mLandscapeHud) {const auto card=LandscapeHud::PacketBounds(b,0);aPosX=card.mX-b.mApp->mScreenBounds.mX;aPosY=card.mY;}
            b.TutorialArrowShow(aPosX, aPosY);
    			b.DisplayAdvice(gStr.ADVICE_CLICK_SEED_PACKET, MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY, AdviceType::ADVICE_NONE);
    		}
    		else
    		{
    			b.DisplayAdvice(gStr.ADVICE_ENOUGH_SUN, MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY, AdviceType::ADVICE_NONE);
    			b.mTutorialTimer = 400;
    		}
    },  // 1 TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER
    // 原 case TUTORIAL_LEVEL_1_PLANT_PEASHOOTER
    [](Board& b)
    {
    		b.mTutorialTimer = -1;
    		b.TutorialArrowRemove();
    		if (b.mPlants.mSize == 0)
    		{
    			b.DisplayAdvice(gStr.ADVICE_CLICK_ON_GRASS, MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY, AdviceType::ADVICE_NONE);
    		}
    		else
    		{
    			b.ClearAdvice(AdviceType::ADVICE_NONE);
    		}
    },  // 2 TUTORIAL_LEVEL_1_PLANT_PEASHOOTER
    // 原 case TUTORIAL_LEVEL_1_REFRESH_PEASHOOTER
    [](Board& b)
    {
    		b.DisplayAdvice(gStr.ADVICE_PLANTED_PEASHOOTER, MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1_STAY, AdviceType::ADVICE_NONE);
    		b.mSunCountDown = 400;
    },  // 3 TUTORIAL_LEVEL_1_REFRESH_PEASHOOTER
    // 原 case TUTORIAL_LEVEL_1_COMPLETED
    [](Board& b)
    {
    		b.DisplayAdvice(gStr.ADVICE_ZOMBIE_ONSLAUGHT, MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1, AdviceType::ADVICE_NONE);
    		b.mZombieCountDown = 99;
    		b.mZombieCountDownStart = b.mZombieCountDown;
    },  // 4 TUTORIAL_LEVEL_1_COMPLETED
    &TutorialCaseTutorialLevel2PickUpSunflower,  // 5 TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER（共体）
    &TutorialCaseTutorialLevel2PlantSunflower,  // 6 TUTORIAL_LEVEL_2_PLANT_SUNFLOWER（共体）
    &TutorialCaseTutorialLevel2PlantSunflower,  // 7 TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER（共体）
    // 原 case TUTORIAL_LEVEL_2_COMPLETED
    [](Board& b)
    {
    		if (b.mCurrentWave == 0)
    		{
    			b.mZombieCountDown = 999;
    			b.mZombieCountDownStart = b.mZombieCountDown;
    		}
    },  // 8 TUTORIAL_LEVEL_2_COMPLETED
    &TutorialCaseTutorialLevel2PickUpSunflower,  // 9 TUTORIAL_MORESUN_PICK_UP_SUNFLOWER（共体）
    &TutorialCaseTutorialLevel2PlantSunflower,  // 10 TUTORIAL_MORESUN_PLANT_SUNFLOWER（共体）
    &TutorialCaseTutorialLevel2PlantSunflower,  // 11 TUTORIAL_MORESUN_REFRESH_SUNFLOWER（共体）
    &TutorialCaseDefault,  // 12 TUTORIAL_MORESUN_COMPLETED（原 default）
    // 原 case TUTORIAL_SLOT_MACHINE_PULL
    [](Board& b)
    {
    		b.DisplayAdvice(gStr.ADVICE_SLOT_MACHINE_PULL, MessageStyle::MESSAGE_STYLE_SLOT_MACHINE, AdviceType::ADVICE_SLOT_MACHINE_PULL);
    },  // 13 TUTORIAL_SLOT_MACHINE_PULL
    // 原 case TUTORIAL_SLOT_MACHINE_COMPLETED
    [](Board& b)
    {
    		b.ClearAdvice(AdviceType::ADVICE_SLOT_MACHINE_PULL);
    },  // 14 TUTORIAL_SLOT_MACHINE_COMPLETED
    // 原 case TUTORIAL_SHOVEL_PICKUP
    [](Board& b)
    {
    		b.DisplayAdvice(gStr.ADVICE_CLICK_SHOVEL, MessageStyle::MESSAGE_STYLE_HINT_STAY, AdviceType::ADVICE_NONE);
    		Rect aShovelButtonRect = b.GetShovelButtonRect();
			int aPosX = aShovelButtonRect.mX - b.mApp->mScreenBounds.mX + aShovelButtonRect.mWidth / 2 - 25;
    		int aPosY = aShovelButtonRect.mY + aShovelButtonRect.mHeight - 65;
    		b.TutorialArrowShow(aPosX, aPosY);
    },  // 15 TUTORIAL_SHOVEL_PICKUP
    // 原 case TUTORIAL_SHOVEL_DIG
    [](Board& b)
    {
    		b.DisplayAdvice(gStr.ADVICE_CLICK_PLANT, MessageStyle::MESSAGE_STYLE_HINT_STAY, AdviceType::ADVICE_NONE);
    		b.TutorialArrowRemove();
    },  // 16 TUTORIAL_SHOVEL_DIG
    // 原 case TUTORIAL_SHOVEL_KEEP_DIGGING
    [](Board& b)
    {
    		b.DisplayAdvice(gStr.ADVICE_KEEP_DIGGING, MessageStyle::MESSAGE_STYLE_HINT_STAY, AdviceType::ADVICE_NONE);
    },  // 17 TUTORIAL_SHOVEL_KEEP_DIGGING
    // 原 case TUTORIAL_SHOVEL_COMPLETED
    [](Board& b)
    {
    		b.ClearAdvice(AdviceType::ADVICE_NONE);
    		b.mCutScene->mCutsceneTime = 1500;
    		b.mCutScene->mCrazyDaveDialogStart = 2410;
    },  // 18 TUTORIAL_SHOVEL_COMPLETED
    &TutorialCaseDefault,  // 19 TUTORIAL_ZOMBIQUARIUM_BUY_SNORKEL（原 default）
    &TutorialCaseDefault,  // 20 TUTORIAL_ZOMBIQUARIUM_BOUGHT_SNORKEL（原 default）
    &TutorialCaseDefault,  // 21 TUTORIAL_ZOMBIQUARIUM_CLICK_TROPHY（原 default）
    &TutorialCaseDefault,  // 22 TUTORIAL_ZEN_GARDEN_PICKUP_WATER（原 default）
    &TutorialCaseDefault,  // 23 TUTORIAL_ZEN_GARDEN_WATER_PLANT（原 default）
    &TutorialCaseDefault,  // 24 TUTORIAL_ZEN_GARDEN_KEEP_WATERING（原 default）
    &TutorialCaseDefault,  // 25 TUTORIAL_ZEN_GARDEN_VISIT_STORE（原 default）
    &TutorialCaseDefault,  // 26 TUTORIAL_ZEN_GARDEN_FERTILIZE_PLANTS（原 default）
    &TutorialCaseDefault,  // 27 TUTORIAL_ZEN_GARDEN_COMPLETED（原 default）
    &TutorialCaseDefault,  // 28 TUTORIAL_WHACK_A_ZOMBIE_BEFORE_PICK_SEED（原 default）
    &TutorialCaseDefault,  // 29 TUTORIAL_WHACK_A_ZOMBIE_PICK_SEED（原 default）
    &TutorialCaseDefault,  // 30 TUTORIAL_WHACK_A_ZOMBIE_COMPLETED（原 default）
};
static_assert(sizeof(kTutorialStateCases) / sizeof(kTutorialStateCases[0]) ==
    (int)TutorialState::TUTORIAL_WHACK_A_ZOMBIE_COMPLETED + 1, "kTutorialStateCases 槽位数须与 TutorialState 枚举同步");
}  // namespace

//0x414CB0
void Board::SetTutorialState(TutorialState theTutorialState)
{
	// C 形态 -> 匿名 lambda 函数指针表：行序 = TutorialState 枚举序（0..30）。
	// 原 default 空操作——缺槽/越界均无操作。
	const unsigned aStateIndex = static_cast<unsigned>(theTutorialState);
	if (aStateIndex <= static_cast<unsigned>(TutorialState::TUTORIAL_WHACK_A_ZOMBIE_COMPLETED))
	{
		kTutorialStateCases[aStateIndex](*this);
	}


	mTutorialState = theTutorialState;
}

//0x415920
void Board::UpdateGame(std::uint64_t theDeltaNs)
{
	UpdateGameObjects(theDeltaNs);
	if (StageHasFog() && mFogBlownCountDown > 0)
	{
		float aMaxFogOffset = 1065.0f - LeftFogColumn() * 80.0f;
		if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
		{
			mFogOffset = TodAnimateCurveNs(2'000'000'000, 0, mFogBlownCountDown.mNanoseconds, aMaxFogOffset, 0.0f, Curves::EaseOut);
		}
		else if (mFogBlownCountDown < 2000)
		{
			mFogOffset = TodAnimateCurveNs(20'000'000'000LL, 0, mFogBlownCountDown.mNanoseconds, aMaxFogOffset, 0.0f, Curves::EaseOut);
		}
		else if (mFogOffset < aMaxFogOffset)
		{
			mFogOffset = TodAnimateCurveFloat(-5, aMaxFogOffset, mFogOffset * (1.0f + static_cast<float>(Sexy::gUpdateIntervalNs * 1.0e-8)), 0, aMaxFogOffset, Curves::Linear);
		}
	}

	if (mApp->mGameScene != GameScenes::SCENE_PLAYING && !mCutScene->ShouldRunUpsellBoard())
		return;

	UpdateSunSpawning();
	UpdateZombieSpawning();
	UpdateIce();
	if (mIceTrapCounter > 0)
	{
		mIceTrapCounter--;
		if (mIceTrapCounter == 0)
		{
			TodParticleSystem* aPoolSparklyParticle = mApp->ParticleTryToGet(mPoolSparklyParticleID);
			if (aPoolSparklyParticle)
			{
				aPoolSparklyParticle->mDontUpdate = false;
			}
		}
	}

	if (mFogBlownCountDown > 0)
	{
		mFogBlownCountDown--;
	}

	// Start on the first positive battle time, independent of update length.
	// The existing tutorial state consumes this one-shot trigger, including across saves.
	if (mLevelTimeNs > 0 && mTutorialState == TutorialState::TUTORIAL_OFF &&
		mApp->IsFirstTimeAdventureMode())
	{
		if (mLevel == 1)
		{
			SetTutorialState(TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER);
		}
		else if (mLevel == 2)
		{
			SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER);
			DisplayAdvice(gStr.ADVICE_PLANT_SUNFLOWER1, MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL2, AdviceType::ADVICE_NONE);
			mTutorialTimer = 500;
		}
	}

	UpdateProgressMeter(theDeltaNs);
}

void Board::UpdateScreenShake()
{
    // Shake around the current viewport's settled background position, never
    // assume that a widened or externally restored canvas is centered on x=0.
    int originX=0;
    if(mApp->mGameScene==SCENE_PLAYING&&mBackground>=BACKGROUND_1_DAY&&mBackground<=BACKGROUND_6_BOSS) {
        const auto& bounds=mApp->mScreenBounds;
        const Sexy::GameViewport canvas{bounds.mWidth,BOARD_HEIGHT,bounds.mX};
        originX=canvas.left-canvas.WithinWorld(-BOARD_OFFSET,BOARD_IMAGE_WIDTH_OFFSET).left;
    }
    --mShakeCounter;
    if(mShakeCounter==0) {mX=originX;mY=0;return;}
    if(Sexy::UpdateChance(3))mShakeAmountX=-mShakeAmountX;
    mX=originX+TodAnimateCurveNs(120'000'000,0,mShakeCounter.mNanoseconds,0,mShakeAmountX,Curves::Bounce);
    mY=TodAnimateCurveNs(120'000'000,0,mShakeCounter.mNanoseconds,0,mShakeAmountY,Curves::Bounce);
}

//0x415D40
void Board::Update()
{


	RefreshHudLayout();
	Widget::Update();
	MarkDirty();

    const auto aVisualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
    const auto waterNowNs = mPoolIntroVisual && mApp->mGameScene == SCENE_LEVEL_INTRO &&
        !mCutScene->IsSurvivalRepick() ? aVisualNowNs : static_cast<std::int64_t>(mLevelTimeNs);
	if (mBackground == BackgroundType::BACKGROUND_3_POOL && mPoolSparklyParticleID == ParticleSystemID::PARTICLESYSTEMID_NULL)
	{
		int aRenderPosition = MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, 2, 0);
		TodParticleSystem* aPoolParticle = mApp->AddTodParticle(450, 295, aRenderPosition, ParticleEffect::PARTICLE_POOL_SPARKLY, waterNowNs);
		mPoolSparklyParticleID = mApp->ParticleGetID(aPoolParticle);
	}

	mCutScene->Update();
    // Keep the first intro's visual age, then apply the first Battle delta.
    if (mPoolIntroVisual && mApp->mGameScene == SCENE_PLAYING)
    {
        if (auto* sparkle = mApp->ParticleTryToGet(mPoolSparklyParticleID))
            sparkle->Update(0, static_cast<std::int64_t>(mLevelTimeNs));
        if (StageHasPool() && mApp->mPoolEffect)
            mApp->mPoolEffect->RebaseClock(static_cast<long long>(mLevelTimeNs));
        mPoolIntroVisual = false;
    }
	UpdateMousePosition();
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		mApp->mZenGarden->ZenGardenUpdate();
	}
	if (IsScaryPotterDaveTalking())
	{
		mApp->UpdateCrazyDave();
	}

	if (mPaused)
	{
		mChallenge->Update(0);
		mCursorPreview->mVisible = false;
		mCursorObject->mVisible = false;
		return;
	}

	// Publish the current action timestamp before effects, grid items and
	// actors sample it. Visual effects have their separately published clock.
	const std::uint64_t aLevelDeltaNs =
		mTimeStopCounter == 0 &&
		AnimationTimeDomain() == ReanimTimeDomain::Battle
		? Sexy::gUpdateIntervalNs : 0;
	if (mLevelTimeNs > std::uint64_t(INT64_MAX) ||
		aLevelDeltaNs > std::uint64_t(INT64_MAX) - mLevelTimeNs)
		throw std::overflow_error("battle animation timestamp exceeds clock range");
	mLevelTimeNs += aLevelDeltaNs;
	mApp->mEffectSystem->mReanimationHolder->mBattleTimeNs =
		static_cast<std::int64_t>(mLevelTimeNs);

	bool aDisabled = !CanInteractWithBoardButtons() || mIgnoreMouseUp;
	if (!mMenuButton->mBtnNoDraw)
	{
		mMenuButton->mDisabled = aDisabled;
	}
	mMenuButton->Update();
	if (mStoreButton)
	{
		mStoreButton->mDisabled = aDisabled;
		mStoreButton->Update();
	}

    const bool waterUsesVisual = mPoolIntroVisual && mApp->mGameScene == SCENE_LEVEL_INTRO &&
        !mCutScene->IsSurvivalRepick();
    const auto waterSampleNs = waterUsesVisual ? aVisualNowNs : static_cast<std::int64_t>(mLevelTimeNs);
    const auto waterDeltaNs = waterUsesVisual ? Sexy::gUpdateIntervalNs : aLevelDeltaNs;
    mApp->mEffectSystem->UpdateAt(aVisualNowNs, static_cast<std::int64_t>(mLevelTimeNs), waterSampleNs);
	// The opening title starts during the intro, before the battle clock runs.
	mAdvice->Update(mAdvice->mMessageStyle == MessageStyle::MESSAGE_STYLE_HOUSE_NAME
		? aVisualNowNs : static_cast<std::int64_t>(mLevelTimeNs));
	UpdateTutorial();

	if (mCobCannonCursorDelayCounter > 0)
	{
		mCobCannonCursorDelayCounter--;
	}
	if (mOutOfMoneyCounter > 0)
	{
		mOutOfMoneyCounter--;
	}
    if(mShakeCounter>0)UpdateScreenShake();
	if (mCoinBankFadeCount > 0 && mApp->GetDialog(Dialogs::DIALOG_PURCHASE_PACKET_SLOT) == nullptr)
	{
		mCoinBankFadeCount--;
	}
	UpdateLayers();

	if (mTimeStopCounter > 0)
		return;

	++mEffectCounter;
	if (StageHasPool() && !mIceTrapCounter && mApp->mGameScene != GameScenes::SCENE_ZOMBIES_WON && !mCutScene->IsSurvivalRepick())
	{
        // First intro uses Visual; combat and retained/reloaded boards use Board.
        mApp->mPoolEffect->PoolEffectUpdate(waterSampleNs, waterDeltaNs);
	}

	if (!(mApp->IsSurvivalMode() && mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO))
	{
		UpdateGridItems();
		UpdateFwoosh();
	}
	UpdateGame(aLevelDeltaNs);
	UpdateFog();
	mChallenge->Update(aLevelDeltaNs);
	UpdateLevelEndSequence();
	mPrevMouseX = mApp->mWidgetManager->mLastMouseX;
	mPrevMouseY = mApp->mWidgetManager->mLastMouseY;
}

//0x416080
// GOTY @Patoke: 0x418940
void Board::UpdateLayers()
{
	if (mWidgetManager)
	{
		mWidgetManager->MarkAllDirty();

		for (DialogList::iterator anIter = mApp->mDialogList.begin(); anIter != mApp->mDialogList.end(); ++anIter)
		{
			Dialog* aDialog = *anIter;
			mWidgetManager->BringToFront(aDialog);
			aDialog->MarkDirty();
		}
	}
}

//0x416110
bool Board::RowCanHaveZombies(int theRow)
{
	if (theRow < 0 || theRow >= MAX_GRID_SIZE_Y)
		return false;

	return (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_RESODDED && theRow <= 4) || mPlantRow[theRow] != PlantRowType::PLANTROW_DIRT;
}

int Board::GetIceZPos(int theRow)
{
	return MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, theRow, 2);
}

//0x416140
void Board::DrawIce(Graphics* g, int theGridY)
{
	// Survival repick also starts timers on empty rows; their sentinel is not an ice cap.
	if (mIceTimer[theGridY] == 0 || mIceMinX[theGridY] == BOARD_ICE_START)
		return;

	int aPosY = GridToPixelY(8, theGridY) + 20;
	int aHeight = Sexy::IMAGE_ICE->mHeight;
	int aWidth = Sexy::IMAGE_ICE->mWidth;
	int anAlpha = ClampInt(255 * mIceTimer[theGridY] / 10, 0, 255);
	if (anAlpha < 255)
	{
		g->mColorizeImages = true;
		g->mColor = Color(255, 255, 255, anAlpha);
	}

	int aBeginningX = mIceMinX[theGridY] + 13, aDeltaX;
	for (int aPosX = aBeginningX; aPosX < BOARD_WIDTH; aPosX += aDeltaX)
	{
		if (aPosX == aBeginningX)
		{
			aDeltaX = (BOARD_WIDTH - aBeginningX) % aWidth;
			if (!aDeltaX) aDeltaX = aWidth;
		}
		else aDeltaX = aWidth;
		Rect aRepeatSrcRect(aWidth - aDeltaX, 0, aDeltaX, aHeight);
		Rect aRepeatDstRect(aPosX, aPosY, aDeltaX, aHeight);
		g->DrawImage(Sexy::IMAGE_ICE, aRepeatDstRect, aRepeatSrcRect);
	}
	g->DrawImage(Sexy::IMAGE_ICE_CAP, mIceMinX[theGridY], aPosY);
	g->mColorizeImages = false;
}

//0x416290
// 背景贴图表（每帧热路径）：BackgroundType → 运行期装载全局地址（Image* const*），
//（已并入 kBackgrounds）


void Board::DrawBackdrop(Graphics* g)
{
	Image* aBgImage = nullptr;
	int aBgType = (int)mBackground;
	if ((unsigned)aBgType < (unsigned)(sizeof(kBackgrounds) / sizeof(kBackgrounds[0])))
	{
		aBgImage = kBackgrounds[aBgType].mTopImage ? *kBackgrounds[aBgType].mTopImage : nullptr;
	}
	else
	{
		TOD_ASSERT(false);
	}

	if (mApp->mScreenBounds.mWidth > BOARD_WIDTH)
	{
		const Rect& aBounds = mApp->mScreenBounds;
		const int aLeft = aBounds.mX - mX;
		const int aTop = aBounds.mY - mY;
		const bool aUnsodded =
			(mLevel >= 1 && mLevel <= 4 && mApp->IsFirstTimeAdventureMode()) ||
			mApp->mGameMode == GameMode::GAMEMODE_LIMBO_RESODDED;
		Image* aBaseImage = aUnsodded ? Sexy::IMAGE_BACKGROUND1UNSODDED : aBgImage;
		const bool aGarden = aBaseImage &&
			(aBaseImage == Sexy::IMAGE_BACKGROUND_MUSHROOMGARDEN ||
			 aBaseImage == Sexy::IMAGE_BACKGROUND_GREENHOUSE ||
			 aBaseImage == Sexy::IMAGE_AQUARIUM1);
		const int aBaseX = aGarden ? (BOARD_WIDTH - aBaseImage->mWidth) / 2 : -BOARD_OFFSET;
		const Image* aBaseMemory = aBaseImage;
		// Skip the full-screen underlay only when an already committed, opaque
		// backdrop overwrites it completely. Unknown alpha and camera gaps retain it.
		const bool aCovered = aBaseMemory && !aBaseMemory->mBitsChanged &&
			!aBaseMemory->mPendingDecoded && !aBaseMemory->mHasAlpha && !aBaseMemory->mHasTrans &&
			g->mDrawMode == Graphics::DRAWMODE_NORMAL &&
			(!g->mColorizeImages || g->mColor.mAlpha == 255) &&
			g->mScaleX == 1.0f && g->mScaleY == 1.0f &&
			aLeft >= aBaseX && aLeft + aBounds.mWidth <= aBaseX + aBaseImage->mWidth &&
			aTop >= 0 && aTop + aBounds.mHeight <= aBaseImage->mHeight;
		if (!aCovered)
		{
			Graphics background(*g);
			background.mColor = Color::Black;
			background.FillRect(aLeft, aTop, aBounds.mWidth, aBounds.mHeight);
		}
	}

	if (mLevel == 1 && mApp->IsFirstTimeAdventureMode())
	{
		g->DrawImage(Sexy::IMAGE_BACKGROUND1UNSODDED, -BOARD_OFFSET, 0);
		int aWidth = TodAnimateCurve(0, 1000, mSodPosition, 0, Sexy::IMAGE_SOD1ROW->mWidth, Curves::Linear);
		Rect aSrcRect(0, 0, aWidth, Sexy::IMAGE_SOD1ROW->mHeight);
		g->DrawImage(Sexy::IMAGE_SOD1ROW, 239 - BOARD_OFFSET, 265, aSrcRect);
	}
	else if (((mLevel == 2 || mLevel == 3) && mApp->IsFirstTimeAdventureMode()) || mApp->mGameMode == GameMode::GAMEMODE_LIMBO_RESODDED)
	{
		g->DrawImage(Sexy::IMAGE_BACKGROUND1UNSODDED, -BOARD_OFFSET, 0);
		g->DrawImage(Sexy::IMAGE_SOD1ROW, 239 - BOARD_OFFSET, 265);
		int aWidth = TodAnimateCurve(0, 1000, mSodPosition, 0, Sexy::IMAGE_SOD3ROW->mWidth, Curves::Linear);
		Rect aSrcRect(0, 0, aWidth, Sexy::IMAGE_SOD3ROW->mHeight);
		g->DrawImage(Sexy::IMAGE_SOD3ROW, 235 - BOARD_OFFSET, 149, aSrcRect);
	}
	else if (mLevel == 4 && mApp->IsFirstTimeAdventureMode())
	{
		g->DrawImage(Sexy::IMAGE_BACKGROUND1UNSODDED, -BOARD_OFFSET, 0);
		g->DrawImage(Sexy::IMAGE_SOD3ROW, 235 - BOARD_OFFSET, 149);
		int aWidth = TodAnimateCurve(0, 1000, mSodPosition, 0, 773, Curves::Linear);
		Rect aSrcRect(232, 0, aWidth, Sexy::IMAGE_BACKGROUND1->mHeight);
		g->DrawImage(Sexy::IMAGE_BACKGROUND1, 232 - BOARD_OFFSET, 0, aSrcRect);
	}
	else if (aBgImage)
	{
		if (aBgImage == Sexy::IMAGE_BACKGROUND_MUSHROOMGARDEN || aBgImage == Sexy::IMAGE_BACKGROUND_GREENHOUSE || aBgImage == Sexy::IMAGE_AQUARIUM1)
		{
			// Centered extensions retain the original garden's 800-pixel core.
			g->DrawImage(aBgImage, (BOARD_WIDTH - aBgImage->mWidth) / 2, 0);
		}
		else
		{
			g->DrawImage(aBgImage, -BOARD_OFFSET, 0);
		}
	}

	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
	{
		DrawHouseDoorBottom(g);
	}
	if (StageHasPool())
	{
		mApp->mPoolEffect->PoolEffectDraw(g, StageIsNight());
	}
	if (mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PLANT_PEASHOOTER)
	{
		Graphics aClipG(*g);
		aClipG.mColorizeImages = true;
		aClipG.mColor = GetFlashingColorNs(mLevelTimeNs, 750'000'000);
		aClipG.DrawImage(Sexy::IMAGE_SOD1ROW, 239 - BOARD_OFFSET, 265);
		aClipG.mColorizeImages = false;
	}
	mChallenge->DrawBackdrop(g);
	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && StageHasGraveStones())
	{
		g->DrawImage(Sexy::IMAGE_NIGHT_GRAVE_GRAPHIC, 1092, 40);
	}
}

//0x416690
static bool RenderItemSortFunc(const RenderItem& theItem1, const RenderItem& theItem2)
{
	if (theItem1.mZPos == theItem2.mZPos)
	{
		return theItem1.mGameObject < theItem2.mGameObject;
	}

	return theItem1.mZPos < theItem2.mZPos;
}

//0x4166C0
void Board::AddBossRenderItem(RenderItem* theRenderList, int& theCurRenderItem, Zombie* theBossZombie)
{
	TOD_ASSERT(theCurRenderItem < MAX_RENDER_ITEMS);
	int aBackLegRow = 1;
	int aFrontLegRow = 3;
	int aBackArmRow = 4;
	if (theBossZombie->IsDeadOrDying())
	{
		aBackArmRow = 1;
	}
	else if (theBossZombie->mZombiePhase == ZombiePhase::PHASE_BOSS_STOMPING)
	{
		Reanimation* aBossReanim = mApp->ReanimationTryToGet(theBossZombie->mBodyReanimID);
		const auto aPhaseNs = aBossReanim->SamplePlayback().phaseNs;
		if (aPhaseNs > aBossReanim->mPeriodNs / 4 &&
			aPhaseNs < aBossReanim->mPeriodNs - aBossReanim->mPeriodNs / 4)
		{
			if (theBossZombie->mTargetRow == 1)
			{
				aBackLegRow = 2;
			}
			else if (theBossZombie->mTargetRow == 3)
			{
				aFrontLegRow = 4;
			}
		}
	}

	RenderItem* aItem = &theRenderList[theCurRenderItem];
	aItem->mRenderObjectType = RenderObjectType::RENDER_ITEM_BOSS_PART;
	aItem->mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_BOSS, aBackLegRow, 2);
	aItem->mBossPart = BossPart::BOSS_PART_BACK_LEG;
	theCurRenderItem++;
	aItem = &theRenderList[theCurRenderItem];
	aItem->mRenderObjectType = RenderObjectType::RENDER_ITEM_BOSS_PART;
	aItem->mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_BOSS, aFrontLegRow, 2);
	aItem->mBossPart = BossPart::BOSS_PART_FRONT_LEG;
	theCurRenderItem++;
	aItem = &theRenderList[theCurRenderItem];
	aItem->mRenderObjectType = RenderObjectType::RENDER_ITEM_BOSS_PART;
	aItem->mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_BOSS, 4, 2);
	aItem->mBossPart = BossPart::BOSS_PART_MAIN;
	theCurRenderItem++;
	aItem = &theRenderList[theCurRenderItem];
	aItem->mRenderObjectType = RenderObjectType::RENDER_ITEM_BOSS_PART;
	aItem->mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_BOSS, aBackArmRow, 3);
	aItem->mBossPart = BossPart::BOSS_PART_BACK_ARM;
	theCurRenderItem++;

	Reanimation* aBallReanim = mApp->ReanimationTryToGet(theBossZombie->mBossFireBallReanimID);
	if (aBallReanim)
	{
		RenderItem* aItem = &theRenderList[theCurRenderItem];
		aItem->mRenderObjectType = RenderObjectType::RENDER_ITEM_BOSS_PART;
		aItem->mZPos = aBallReanim->mRenderOrder;
		aItem->mBossPart = BossPart::BOSS_PART_FIREBALL;
		theCurRenderItem++;
	}
}

/*
[[maybe_unused]]
static inline void AddGameObjectRenderItem(RenderItem* theRenderList, int& theCurRenderItem, RenderObjectType theRenderObjectType, GameObject* theGameObject)
{
	TOD_ASSERT(theCurRenderItem < MAX_RENDER_ITEMS);
	RenderItem& aRenderItem = theRenderList[theCurRenderItem];
	aRenderItem.mRenderObjectType = theRenderObjectType;
	aRenderItem.mZPos = theGameObject->mRenderOrder;
	aRenderItem.mGameObject = theGameObject;
	theCurRenderItem++;
}
*/

static inline void AddGameObjectRenderItemCursorPreview(RenderItem* theRenderList, int& theCurRenderItem, RenderObjectType theRenderObjectType, GameObject* theGameObject)
{
	TOD_ASSERT(theCurRenderItem < MAX_RENDER_ITEMS);
	RenderItem& aRenderItem = theRenderList[theCurRenderItem];
	aRenderItem.mRenderObjectType = theRenderObjectType;
	aRenderItem.mZPos = theGameObject->mRenderOrder;
	aRenderItem.mGameObject = theGameObject;
	aRenderItem.mCursorPreview = (CursorPreview*)theGameObject;

	theCurRenderItem++;
}

static inline void AddGameObjectRenderItemPlant(RenderItem* theRenderList, int& theCurRenderItem, RenderObjectType theRenderObjectType, Plant* theGameObject)
{
	TOD_ASSERT(theCurRenderItem < MAX_RENDER_ITEMS);
	RenderItem& aRenderItem = theRenderList[theCurRenderItem];
	aRenderItem.mRenderObjectType = theRenderObjectType;
	aRenderItem.mZPos = theGameObject->mRenderOrder;
	aRenderItem.mPlant = theGameObject;

	theCurRenderItem++;
}

static inline void AddGameObjectRenderItemZombie(RenderItem* theRenderList, int& theCurRenderItem, RenderObjectType theRenderObjectType, Zombie* theGameObject)
{
	TOD_ASSERT(theCurRenderItem < MAX_RENDER_ITEMS);
	RenderItem& aRenderItem = theRenderList[theCurRenderItem];
	aRenderItem.mRenderObjectType = theRenderObjectType;
	aRenderItem.mZPos = theGameObject->mRenderOrder;
	aRenderItem.mZombie = theGameObject;
	theCurRenderItem++;
}

static inline void AddGameObjectRenderItemProjectile(RenderItem* theRenderList, int& theCurRenderItem, RenderObjectType theRenderObjectType, GameObject* theGameObject)
{
	TOD_ASSERT(theCurRenderItem < MAX_RENDER_ITEMS);
	RenderItem& aRenderItem = theRenderList[theCurRenderItem];
	aRenderItem.mRenderObjectType = theRenderObjectType;
	aRenderItem.mZPos = theGameObject->mRenderOrder;
	aRenderItem.mGameObject = theGameObject;
	aRenderItem.mProjectile = (Projectile*)theGameObject;
	theCurRenderItem++;
}

static inline void AddGameObjectRenderItemCoin(RenderItem* theRenderList, int& theCurRenderItem, RenderObjectType theRenderObjectType, GameObject* theGameObject)
{
	TOD_ASSERT(theCurRenderItem < MAX_RENDER_ITEMS);
	RenderItem& aRenderItem = theRenderList[theCurRenderItem];
	aRenderItem.mRenderObjectType = theRenderObjectType;
	aRenderItem.mZPos = theGameObject->mRenderOrder;
	aRenderItem.mGameObject = theGameObject;
	aRenderItem.mCoin = (Coin*)theGameObject;
	theCurRenderItem++;
}

static inline void AddUIRenderItem(RenderItem* theRenderList, int& theCurRenderItem, RenderObjectType theRenderObjectType, int thePosZ)
{
	TOD_ASSERT(theCurRenderItem < MAX_RENDER_ITEMS);
	RenderItem& aRenderItem = theRenderList[theCurRenderItem];
	aRenderItem.mRenderObjectType = theRenderObjectType;
	aRenderItem.mZPos = thePosZ;
	aRenderItem.mGameObject = nullptr;
	theCurRenderItem++;
}

//0x416880
void Board::DrawGameObjects(Graphics* g)
{

	RenderItem aRenderList[MAX_RENDER_ITEMS];
	int aRenderItemCount = 0;

	{
		Plant* aPlant = nullptr;
		while (IteratePlants(aPlant))
		{
			if (aPlant->mOnBungeeState == PlantOnBungeeState::NOT_ON_BUNGEE)
			{
				AddGameObjectRenderItemPlant(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_PLANT, aPlant);
				// Actor-managed sleep animations keep world-space rendering and
				// their original render order, while the plant supplies their time.
				if (auto* sleeping=mApp->ReanimationTryToGet(aPlant->mSleepingReanimID))
				{
					RenderItem& item=aRenderList[aRenderItemCount++];
					item.mRenderObjectType=RENDER_ITEM_REANIMATION;
					item.mZPos=sleeping->mRenderOrder;
					item.mReanimation=sleeping;
				}

				if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN && aPlant->mPottedPlantIndex != -1)
				{
					RenderItem& aRenderItem = aRenderList[aRenderItemCount];
					aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_PLANT_OVERLAY;
					aRenderItem.mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, 0, mY);
					aRenderItem.mPlant = aPlant;
					aRenderItemCount++;
				}

				if ((aPlant->mSeedType == SeedType::SEED_MAGNETSHROOM || aPlant->mSeedType == SeedType::SEED_GOLD_MAGNET) && aPlant->DrawMagnetItemsOnTop())
				{
					RenderItem& aRenderItem = aRenderList[aRenderItemCount];
					aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_PLANT_MAGNET_ITEMS;
					aRenderItem.mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, -1);
					aRenderItem.mPlant = aPlant;
					aRenderItemCount++;
				}
			}
		}
	}
	{
		Coin* aCoin = nullptr;
		while (IterateCoins(aCoin))
		{
			AddGameObjectRenderItemCoin(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_COIN, aCoin);
		}
	}
	{
		Zombie* aZombie = nullptr;
		while (IterateZombies(aZombie))
		{
			if (((ZombieFlagsOf(aZombie->mZombieType) & ZombieFlags::Boss) != 0))
			{
				AddBossRenderItem(aRenderList, aRenderItemCount, aZombie);
			}
			else
			{
				AddGameObjectRenderItemZombie(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_ZOMBIE, aZombie);

				if (aZombie->HasShadow())
				{
					RenderItem& aRenderItem = aRenderList[aRenderItemCount];
					aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_ZOMBIE_SHADOW;
					aRenderItem.mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, aZombie->mRow, 3);
					aRenderItem.mZombie = aZombie;
					aRenderItemCount++;
				}

				if (aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE)
				{
					RenderItem& aRenderItem = aRenderList[aRenderItemCount];
					aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_ZOMBIE_BUNGEE_TARGET;
					aRenderItem.mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_PROJECTILE, aZombie->mRow, 1);
					aRenderItem.mZombie = aZombie;
					aRenderItemCount++;
				}
			}
		}
	}
	{
		Projectile* aProjectile = nullptr;
		while (IterateProjectiles(aProjectile))
		{
			AddGameObjectRenderItemProjectile(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_PROJECTILE, aProjectile);

			RenderItem& aRenderItem = aRenderList[aRenderItemCount];
			aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_PROJECTILE_SHADOW;
			aRenderItem.mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, aProjectile->mRow, 3);
			aRenderItem.mProjectile = aProjectile;
			aRenderItemCount++;
		}
	}
	{
		LawnMower* aLawnMower = nullptr;
		while (IterateLawnMowers(aLawnMower))
		{
			RenderItem& aRenderItem = aRenderList[aRenderItemCount];
			aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_MOWER;
			aRenderItem.mZPos = aLawnMower->mRenderOrder;
			aRenderItem.mMower = aLawnMower;
			aRenderItemCount++;
		}
	}
	{
		TodParticleSystem* aParticle = nullptr;
		while (IterateParticles(aParticle))
		{
			if (!aParticle->mIsAttachment)
			{
				RenderItem& aRenderItem = aRenderList[aRenderItemCount];
				aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_PARTICLE;
				aRenderItem.mZPos = aParticle->mRenderOrder;
				aRenderItem.mParticleSytem = aParticle;
				aRenderItemCount++;
			}
		}
		for (const auto id : mCutScene->mVisualParticleIDs)
			if (auto* particle=mApp->ParticleTryToGet(id); particle && !particle->mDead)
			{
				RenderItem& item=aRenderList[aRenderItemCount++];
				item.mRenderObjectType=RENDER_ITEM_PARTICLE;
				item.mZPos=particle->mRenderOrder;
				item.mParticleSytem=particle;
			}
	}
	{
		Reanimation* aReanimation = nullptr;
		while (IterateReanimations(aReanimation))
		{
			if (!aReanimation->mIsAttachment)
			{
				RenderItem& aRenderItem = aRenderList[aRenderItemCount];
				aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_REANIMATION;
				aRenderItem.mZPos = aReanimation->mRenderOrder;
				aRenderItem.mReanimation = aReanimation;
				aRenderItemCount++;
			}
		}
	}
	{
		GridItem* aGridItem = nullptr;
		while (IterateGridItems(aGridItem))
		{
			RenderItem& aRenderItem = aRenderList[aRenderItemCount];
			aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_GRID_ITEM;
			aRenderItem.mZPos = aGridItem->mRenderOrder;
			aRenderItem.mGridItem = aGridItem;
			aRenderItemCount++;

			if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN && aGridItem->mGridItemType == GridItemType::GRIDITEM_STINKY)
			{
				RenderItem& aRenderItem = aRenderList[aRenderItemCount];
				aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_GRID_ITEM_OVERLAY;
				aRenderItem.mZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, 0, aGridItem->mPosY - 30.0f);
				aRenderItem.mGridItem = aGridItem;
				aRenderItemCount++;
			}
		}
	}
	for (int i = 0; i < MAX_GRID_SIZE_Y; i++)
	{
		if (mIceTimer[i])
		{
			RenderItem& aRenderItem = aRenderList[aRenderItemCount];
			aRenderItem.mRenderObjectType = RenderObjectType::RENDER_ITEM_ICE;
			aRenderItem.mBoardGridY = i;
			aRenderItem.mZPos = GetIceZPos(i);
			aRenderItemCount++;
		}
	}
	{
		int aZPos;
		if (mTimeStopCounter > 0)
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0);
		}
		else if (mApp->mGameScene == GameScenes::SCENE_PLAYING || mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_UI_BOTTOM, 0, 1);
		}
		else if (mCutScene->IsAfterSeedChooser() || mCutScene->IsInShovelTutorial() || mHelpIndex == AdviceType::ADVICE_CLICK_TO_CONTINUE)
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_UI_BOTTOM, 0, 1);
		}
		else
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_ABOVE_UI, 0, 0);
		}

		AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_BACKDROP, MakeRenderOrder(RenderLayer::RENDER_LAYER_UI_BOTTOM, 0, 0));
		AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_BOTTOM_UI, aZPos);
		AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_COIN_BANK, MakeRenderOrder(RenderLayer::RENDER_LAYER_COIN_BANK, 0, 0));
        // Ordinary sun/coins start at COIN_BANK+1 (not ABOVE_UI). Keep the
        // native HUD above world/fog but below that actual collectible layer.
        AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_TOP_UI,
            mLandscapeHud ? MakeRenderOrder(RENDER_LAYER_COIN_BANK,0,-1) :
                MakeRenderOrder(RENDER_LAYER_UI_TOP,0,0));
		AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_SCREEN_FADE, MakeRenderOrder(RenderLayer::RENDER_LAYER_SCREEN_FADE, 0, 0));
	}
	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
	{
		int aZPos;
		if (StageHasRoof())
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, 0, 4);
		}
		else
		{
			aZPos = MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE, 3, 2);
		}
		AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_DOOR_MASK, aZPos);
	}
	if (StageHasFog())
	{
		AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_FOG, MakeRenderOrder(RenderLayer::RENDER_LAYER_FOG, 0, 0));
	}
	if (mApp->IsStormyNightLevel() || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_RAINING_SEEDS)
	{
		AddUIRenderItem(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_STORM, MakeRenderOrder(RenderLayer::RENDER_LAYER_FOG, 0, 3));
	}
	AddGameObjectRenderItemCursorPreview(aRenderList, aRenderItemCount, RenderObjectType::RENDER_ITEM_CURSOR_PREVIEW, mCursorPreview);

	std::sort(aRenderList, aRenderList + aRenderItemCount, RenderItemSortFunc);

	// 渲染项绘制分发：25 类型 lambda 表——union 对象变体/板 UI/带参项统一形态，
	// 原 24 case 跳转表化为取指直呼，循环体只剩越界守卫 + 间接调用。槽位锚
	// 枚举首尾（RENDER_ITEM_COIN=0、GRID_ITEM_OVERLAY=24）；越界保持原
	// default 断言语义（Release 下安全跳过不绘制）。
	static_assert(RENDER_ITEM_COIN == 0, "渲染项表以枚举首项为基 0");
	static constexpr void (*kRenderItemDraws[])(Board*, Graphics*, RenderItem&) = {
		/* [0]  RENDER_ITEM_COIN                 */ [](Board*, Graphics* g, RenderItem& aRenderItem) { Coin* aCoin = aRenderItem.mCoin; const float x = g->mTransX, y = g->mTransY; if (aCoin->BeginDraw(g)) { aCoin->Draw(g); aCoin->EndDraw(g, x, y); } },
		/* [1]  RENDER_ITEM_PROJECTILE           */ [](Board*, Graphics* g, RenderItem& aRenderItem) { Projectile* aProjectile = aRenderItem.mProjectile; const float x = g->mTransX, y = g->mTransY; if (aProjectile->BeginDraw(g)) { aProjectile->Draw(g); aProjectile->EndDraw(g, x, y); } },
		/* [2]  RENDER_ITEM_ZOMBIE               */ [](Board* aBoard, Graphics* g, RenderItem& aRenderItem) { Zombie* aZombie = aRenderItem.mZombie; const auto nowNs = aZombie->IsOnBoard() ? static_cast<std::int64_t>(aBoard->mLevelTimeNs) : aBoard->mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs; const float x = g->mTransX, y = g->mTransY; if (aZombie->BeginDraw(g)) { aZombie->Draw(nowNs, g); aZombie->EndDraw(g, x, y); } },
		/* [3]  RENDER_ITEM_ZOMBIE_SHADOW        */ [](Board*, Graphics* g, RenderItem& aRenderItem) { Zombie* aZombie = aRenderItem.mZombie; const float x = g->mTransX, y = g->mTransY; if (aZombie->BeginDraw(g)) { aZombie->DrawShadow(g); aZombie->EndDraw(g, x, y); } },
		/* [4]  RENDER_ITEM_ZOMBIE_BUNGEE_TARGET */ [](Board*, Graphics* g, RenderItem& aRenderItem) { aRenderItem.mZombie->DrawBungeeTarget(g); },
		/* [5]  RENDER_ITEM_PLANT                */ [](Board* aBoard, Graphics* g, RenderItem& aRenderItem) { Plant* aPlant = aRenderItem.mPlant; const float x = g->mTransX, y = g->mTransY; if (aPlant->BeginDraw(g)) { aPlant->Draw(static_cast<std::int64_t>(aBoard->mLevelTimeNs), g); aPlant->EndDraw(g, x, y); } },
		/* [6]  RENDER_ITEM_PLANT_OVERLAY        */ [](Board* aBoard, Graphics* g, RenderItem& aRenderItem) { Plant* aPlant = aRenderItem.mPlant; const float x = g->mTransX, y = g->mTransY; if (aPlant->BeginDraw(g)) { aBoard->mApp->mZenGarden->DrawPlantOverlay(g, aPlant); aPlant->EndDraw(g, x, y); } },
		/* [7]  RENDER_ITEM_PLANT_MAGNET_ITEMS   */ [](Board* aBoard, Graphics* g, RenderItem& aRenderItem) { Plant* aPlant = aRenderItem.mPlant; const float x = g->mTransX, y = g->mTransY; if (aPlant->BeginDraw(g)) { aPlant->DrawMagnetItems(static_cast<std::int64_t>(aBoard->mLevelTimeNs), g); aPlant->EndDraw(g, x, y); } },
		/* [8]  RENDER_ITEM_CURSOR_PREVIEW       */ [](Board* board, Graphics* g, RenderItem& aRenderItem) { CursorPreview* aCursorPreview = aRenderItem.mCursorPreview; const float x = g->mTransX, y = g->mTransY; if (aCursorPreview->BeginDraw(g)) { aCursorPreview->Draw(g); aCursorPreview->EndDraw(g, x, y); } board->DrawTouchPreviews(g); },
		/* [9]  RENDER_ITEM_PARTICLE             */ [](Board* aBoard, Graphics* g, RenderItem& aRenderItem) {
			const ParticleEffect aEffect = aRenderItem.mParticleSytem->mEffectType;
			const bool aBankArrow = aEffect == ParticleEffect::PARTICLE_SEED_PACKET_PICK &&
				aBoard->mApp->mGameMode != GameMode::GAMEMODE_LIMBO_ZEN_GARDEN &&
				aBoard->mApp->mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM;
			if (aEffect == ParticleEffect::PARTICLE_SEED_PACKET_FLASH || aBankArrow)
			{
                Graphics aBankGraphics(*g);
                aBankGraphics.Translate(aBoard->mApp->mScreenBounds.mX, 0);
				aRenderItem.mParticleSytem->Draw(&aBankGraphics);
				return;
			}
			aRenderItem.mParticleSytem->Draw(g);
		},
		/* [10] RENDER_ITEM_REANIMATION          */ [](Board*, Graphics* g, RenderItem& aRenderItem) { aRenderItem.mReanimation->Draw(g); },
		/* [11] RENDER_ITEM_ICE                  */ [](Board* aBoard, Graphics* g, RenderItem& aRenderItem) { aBoard->DrawIce(g, aRenderItem.mBoardGridY); },
		/* [12] RENDER_ITEM_TOP_UI               */ [](Board* aBoard, Graphics* g, RenderItem&) { aBoard->DrawUITop(g); },
		/* [13] RENDER_ITEM_FOG                  */ [](Board* aBoard, Graphics* g, RenderItem&) { aBoard->DrawFog(g); },
		/* [14] RENDER_ITEM_STORM                */ [](Board* aBoard, Graphics* g, RenderItem&) { aBoard->mChallenge->DrawWeather(g); },
		/* [15] RENDER_ITEM_BOTTOM_UI            */ [](Board* aBoard, Graphics* g, RenderItem&) { aBoard->DrawUIBottom(g); },
		/* [16] RENDER_ITEM_BACKDROP             */ [](Board* aBoard, Graphics* g, RenderItem&) { aBoard->DrawBackdrop(g); },
		/* [17] RENDER_ITEM_DOOR_MASK            */ [](Board* aBoard, Graphics* g, RenderItem&) { aBoard->DrawHouseDoorTop(g); },
		/* [18] RENDER_ITEM_COIN_BANK            */ [](Board* aBoard, Graphics* g, RenderItem&) { aBoard->DrawUICoinBank(g); },
		/* [19] RENDER_ITEM_PROJECTILE_SHADOW    */ [](Board*, Graphics* g, RenderItem& aRenderItem) { Projectile* aProjectile = aRenderItem.mProjectile; const float x = g->mTransX, y = g->mTransY; if (aProjectile->BeginDraw(g)) { aProjectile->DrawShadow(g); aProjectile->EndDraw(g, x, y); } },
		/* [20] RENDER_ITEM_MOWER                */ [](Board*, Graphics* g, RenderItem& aRenderItem) { aRenderItem.mMower->Draw(g); },
		/* [21] RENDER_ITEM_SCREEN_FADE          */ [](Board* aBoard, Graphics* g, RenderItem&) { aBoard->DrawFadeOut(g); },
		/* [22] RENDER_ITEM_BOSS_PART            */ [](Board* aBoard, Graphics* g, RenderItem& aRenderItem) { Zombie* aBossZombie = aBoard->GetBossZombie(); const float x = g->mTransX, y = g->mTransY; if (aBossZombie && aBossZombie->BeginDraw(g)) { aBossZombie->DrawBossPart(static_cast<std::int64_t>(aBoard->mLevelTimeNs), g, aRenderItem.mBossPart); aBossZombie->EndDraw(g, x, y); } },
		/* [23] RENDER_ITEM_GRID_ITEM            */ [](Board*, Graphics* g, RenderItem& aRenderItem) { aRenderItem.mGridItem->DrawGridItem(g); },
		/* [24] RENDER_ITEM_GRID_ITEM_OVERLAY    */ [](Board*, Graphics* g, RenderItem& aRenderItem) { aRenderItem.mGridItem->DrawGridItemOverlay(g); },
	};
	static_assert(sizeof(kRenderItemDraws) / sizeof(kRenderItemDraws[0]) == RENDER_ITEM_GRID_ITEM_OVERLAY + 1,
		"渲染项绘制槽位须与 RenderObjectType 枚举尾同步");
	static constexpr int kRenderItemCount = (int)(sizeof(kRenderItemDraws) / sizeof(kRenderItemDraws[0]));
	for (int i = 0; i < aRenderItemCount; i++)
	{
		RenderItem& aRenderItem = aRenderList[i];
		int aType = (int)aRenderItem.mRenderObjectType;
		if ((unsigned)aType < (unsigned)kRenderItemCount)
		{
			kRenderItemDraws[aType](this, g, aRenderItem);
		}
		else
		{
			TOD_ASSERT(false);
		}
	}

}

//0x4173C0
bool Board::HasProgressMeter()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST ||
		mApp->IsFinalBossLevel() ||
		mApp->IsSlotMachineLevel() ||
		mApp->IsSquirrelLevel() ||
		mApp->IsIZombieLevel())
		return true;

	if (mProgressMeterWidth == 0)
		return false;

	if (mApp->IsContinuousChallenge() ||
		mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		mApp->IsScaryPotterLevel())
		return false;

	return true;
}

//0x417450
bool Board::ProgressMeterHasFlags()
{
	if (mApp->IsFirstTimeAdventureMode() && mLevel == 1)
		return false;

	if (mApp->IsWhackAZombieLevel() ||
		mApp->IsFinalBossLevel() ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST ||
		mApp->IsSlotMachineLevel() ||
		mApp->IsSquirrelLevel() ||
		mApp->IsIZombieLevel())
		return false;

	return true;
}

namespace
{
// 进度条文字配置表（B 形态）：kProgressMeterTag 为 GameMode 直下标的分支标记
// （-1 = 无文字，走旗帜绘制 = 原链落空）；kProgressMeterText 按标记直下标。
struct ProgressMeterTextRow
{
	int mMax;                   // %d/%d 分母（亦即分子上限）
	const std::string* mLabel;  // 单位串
	bool mUseSunMoney;          // 分子源：真 = ClampInt(mSunMoney, 0, mMax)，假 = mChallengeScore
};
constexpr ProgressMeterTextRow kProgressMeterText[5] = {
	{ 75,   &gStr.MATCHES,   false },  // 0 Beghouled / BeghouledTwist
	{ 7,    &gStr.SQUIRRELS, false },  // 1 Squirrel
	{ 2000, &gStr.SUN,       true  },  // 2 SlotMachine
	{ 1000, &gStr.SUN,       true  },  // 3 Zombiquarium
	{ 5,    &gStr.BRAINS,    false },  // 4 IZombie
};
constexpr int kProgressMeterTagInvalid = -1;
constexpr int kProgressMeterTag[GameMode::NUM_GAME_MODES] = {
	/* [ 0] GAMEMODE_ADVENTURE  (v1 0) */ -1,
	/* [ 1] GAMEMODE_MINIGAME_BEGHOULED  (v1 20) */ 0,
	/* [ 2] GAMEMODE_MINIGAME_BEGHOULED_TWIST  (v1 24) */ 0,
	/* [ 3] GAMEMODE_MINIGAME_BOBSLED_BONANZA  (v1 28) */ -1,
	/* [ 4] GAMEMODE_MINIGAME_COLUMN  (v1 27) */ -1,
	/* [ 5] GAMEMODE_MINIGAME_FINAL_BOSS  (v1 35) */ -1,
	/* [ 6] GAMEMODE_MINIGAME_INVISIGHOUL  (v1 21) */ -1,
	/* [ 7] GAMEMODE_MINIGAME_LAST_STAND  (v1 31) */ -1,
	/* [ 8] GAMEMODE_MINIGAME_LITTLE_TROUBLE  (v1 25) */ -1,
	/* [ 9] GAMEMODE_MINIGAME_POGO_PARTY  (v1 34) */ -1,
	/* [10] GAMEMODE_MINIGAME_PORTAL_COMBAT  (v1 26) */ -1,
	/* [11] GAMEMODE_MINIGAME_RAINING_SEEDS  (v1 19) */ -1,
	/* [12] GAMEMODE_MINIGAME_SEEING_STARS  (v1 22) */ -1,
	/* [13] GAMEMODE_MINIGAME_SLOT_MACHINE  (v1 18) */ 2,
	/* [14] GAMEMODE_MINIGAME_SPEED  (v1 29) */ -1,
	/* [15] GAMEMODE_MINIGAME_WALLNUT_BOWLING  (v1 17) */ -1,
	/* [16] GAMEMODE_MINIGAME_WALLNUT_BOWLING_2  (v1 33) */ -1,
	/* [17] GAMEMODE_MINIGAME_WAR_AND_PEAS  (v1 16) */ -1,
	/* [18] GAMEMODE_MINIGAME_WAR_AND_PEAS_2  (v1 32) */ -1,
	/* [19] GAMEMODE_MINIGAME_WHACK_A_ZOMBIE  (v1 30) */ -1,
	/* [20] GAMEMODE_MINIGAME_ZOMBIQUARIUM  (v1 23) */ 3,
	/* [21] GAMEMODE_LIMBO_AIR_RAID  (v1 41) */ -1,
	/* [22] GAMEMODE_LIMBO_ART_CHALLENGE_SUNFLOWER  (v1 40) */ -1,
	/* [23] GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT  (v1 36) */ -1,
	/* [24] GAMEMODE_LIMBO_BIG_TIME  (v1 39) */ -1,
	/* [25] GAMEMODE_LIMBO_BUNGEE_BLITZ  (v1 48) */ -1,
	/* [26] GAMEMODE_LIMBO_GRAVE_DANGER  (v1 45) */ -1,
	/* [27] GAMEMODE_LIMBO_HIGH_GRAVITY  (v1 44) */ -1,
	/* [28] GAMEMODE_LIMBO_ICE  (v1 42) */ -1,
	/* [29] GAMEMODE_LIMBO_RESODDED  (v1 38) */ -1,
	/* [30] GAMEMODE_LIMBO_SHOVEL  (v1 46) */ -1,
	/* [31] GAMEMODE_LIMBO_SQUIRREL  (v1 49) */ 1,
	/* [32] GAMEMODE_LIMBO_STORMY_NIGHT  (v1 47) */ -1,
	/* [33] GAMEMODE_LIMBO_SUNNY_DAY  (v1 37) */ -1,
	/* [34] GAMEMODE_LIMBO_ZEN_GARDEN  (v1 43) */ -1,
	/* [35] GAMEMODE_INTRO  (v1 71) */ -1,
	/* [36] GAMEMODE_PUZZLE_I_ZOMBIE_1  (v1 61) */ 4,
	/* [37] GAMEMODE_PUZZLE_I_ZOMBIE_2  (v1 62) */ 4,
	/* [38] GAMEMODE_PUZZLE_I_ZOMBIE_3  (v1 63) */ 4,
	/* [39] GAMEMODE_PUZZLE_I_ZOMBIE_4  (v1 64) */ 4,
	/* [40] GAMEMODE_PUZZLE_I_ZOMBIE_5  (v1 65) */ 4,
	/* [41] GAMEMODE_PUZZLE_I_ZOMBIE_6  (v1 66) */ 4,
	/* [42] GAMEMODE_PUZZLE_I_ZOMBIE_7  (v1 67) */ 4,
	/* [43] GAMEMODE_PUZZLE_I_ZOMBIE_8  (v1 68) */ 4,
	/* [44] GAMEMODE_PUZZLE_I_ZOMBIE_9  (v1 69) */ 4,
	/* [45] GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS  (v1 70) */ 4,
	/* [46] GAMEMODE_SCARY_POTTER_1  (v1 51) */ -1,
	/* [47] GAMEMODE_SCARY_POTTER_2  (v1 52) */ -1,
	/* [48] GAMEMODE_SCARY_POTTER_3  (v1 53) */ -1,
	/* [49] GAMEMODE_SCARY_POTTER_4  (v1 54) */ -1,
	/* [50] GAMEMODE_SCARY_POTTER_5  (v1 55) */ -1,
	/* [51] GAMEMODE_SCARY_POTTER_6  (v1 56) */ -1,
	/* [52] GAMEMODE_SCARY_POTTER_7  (v1 57) */ -1,
	/* [53] GAMEMODE_SCARY_POTTER_8  (v1 58) */ -1,
	/* [54] GAMEMODE_SCARY_POTTER_9  (v1 59) */ -1,
	/* [55] GAMEMODE_SCARY_POTTER_ENDLESS  (v1 60) */ -1,
	/* [56] GAMEMODE_SURVIVAL_ENDLESS_STAGE_1  (v1 11) */ -1,
	/* [57] GAMEMODE_SURVIVAL_ENDLESS_STAGE_2  (v1 12) */ -1,
	/* [58] GAMEMODE_SURVIVAL_ENDLESS_STAGE_3  (v1 13) */ -1,
	/* [59] GAMEMODE_SURVIVAL_ENDLESS_STAGE_4  (v1 14) */ -1,
	/* [60] GAMEMODE_SURVIVAL_ENDLESS_STAGE_5  (v1 15) */ -1,
	/* [61] GAMEMODE_SURVIVAL_HARD_STAGE_1  (v1 6) */ -1,
	/* [62] GAMEMODE_SURVIVAL_HARD_STAGE_2  (v1 7) */ -1,
	/* [63] GAMEMODE_SURVIVAL_HARD_STAGE_3  (v1 8) */ -1,
	/* [64] GAMEMODE_SURVIVAL_HARD_STAGE_4  (v1 9) */ -1,
	/* [65] GAMEMODE_SURVIVAL_HARD_STAGE_5  (v1 10) */ -1,
	/* [66] GAMEMODE_SURVIVAL_NORMAL_STAGE_1  (v1 1) */ -1,
	/* [67] GAMEMODE_SURVIVAL_NORMAL_STAGE_2  (v1 2) */ -1,
	/* [68] GAMEMODE_SURVIVAL_NORMAL_STAGE_3  (v1 3) */ -1,
	/* [69] GAMEMODE_SURVIVAL_NORMAL_STAGE_4  (v1 4) */ -1,
	/* [70] GAMEMODE_SURVIVAL_NORMAL_STAGE_5  (v1 5) */ -1,
	/* [71] GAMEMODE_TREE_OF_WISDOM  (v1 50) */ -1,
	/* [72] GAMEMODE_CUSTOM_LEVEL（哨兵；策略层覆写）*/ 0,
};

static_assert(sizeof(kProgressMeterTag) / sizeof(kProgressMeterTag[0]) == (int)GameMode::NUM_GAME_MODES,
	"进度条标记表槽位须与 GameMode 枚举尾同步");
static_assert(sizeof(kProgressMeterText) / sizeof(kProgressMeterText[0]) == 5,
	"进度条文字配置槽位须与标记值域 0..4 同步");
}  // namespace

//0x417500
// GOTY @Patoke: 0x419E30
void Board::DrawProgressMeter(Graphics* g)
{
	if (!HasProgressMeter())
		return;

    Graphics aHudGraphics(*g);
    g = &aHudGraphics;
	g->Translate(mApp->mScreenBounds.mX + mApp->mScreenBounds.mWidth - BOARD_WIDTH-mX, -mY);
	// ====================================================================================================
	// ▲ 绘制进度条进度部分的贴图
	// ====================================================================================================
	g->DrawImage(Sexy::IMAGE_FLAGMETER, 600, 575, Rect(0, 0, 158, 27));
	constexpr int aCelWidth = 158;
	constexpr int aCelHeight = 27;
	int aClipWidth = TodAnimateCurve(0, PROGRESS_METER_COUNTER, mProgressMeterWidth, 0, 143, Curves::Linear);
	Rect aSrcRect(aCelWidth - aClipWidth - 7, aCelHeight, aClipWidth, aCelHeight);
	Rect aDstRect(aCelWidth - aClipWidth + 593, 575, aClipWidth, aCelHeight);
	g->DrawImage(Sexy::IMAGE_FLAGMETER, aDstRect, aSrcRect);

	// ====================================================================================================
	// ▲ 根据不同关卡，绘制进度条上的文字或旗帜
	// ====================================================================================================
	int aPosX = aCelWidth / 2 + 600;
	Color aColor(224, 187, 98);
	// @Patoke: updated these
	// C++23 第一批·条目4：挑战分值串按 (分支标记, 分值) 缓存，值变才重建（含 %s 实体键，保留 StrFormat；输出逐字节等价）
	// B 形态 -> 常量表（上方 kProgressMeterTag/kProgressMeterText，GameMode 直下标；-1 = 原链落空）
	int aMeterTag = kProgressMeterTag[(int)mApp->mGameMode];
	int aMeterScore = 0;
	if (aMeterTag != kProgressMeterTagInvalid)
	{
		const ProgressMeterTextRow& aMeterRow = kProgressMeterText[aMeterTag];
		aMeterScore = aMeterRow.mUseSunMoney ? ClampInt(mSunMoney, 0, aMeterRow.mMax) : mChallenge->mChallengeScore;
	}

	if (aMeterTag >= 0)
	{
		// 条目4：函数域 static 缓存（Board 布局被 dat64 整块镜像锁定，不能加成员）；
		// 属主哨兵保证换 Board 实例时首帧重建。
		static Board*     sMeterOwner = nullptr;
		static int        sMeterKeyTag = 0;
		static int        sMeterKeyScore = 0;
		static SexyString sMeterStr;
		if (sMeterOwner != this || aMeterTag != sMeterKeyTag || aMeterScore != sMeterKeyScore)
		{
			sMeterOwner = this;
			sMeterKeyTag = aMeterTag;
			sMeterKeyScore = aMeterScore;
			// B 形态 -> 常量表（上方 kProgressMeterText，标记直下标；五行 case 合一）
			const ProgressMeterTextRow& aMeterRow = kProgressMeterText[aMeterTag];
			sMeterStr = StrFormat(("%d/%d %s"), aMeterScore, aMeterRow.mMax, EntityTranslate(*aMeterRow.mLabel).c_str());
		}
		TodDrawString(g, sMeterStr, aPosX, 589, Sexy::FONT_DWARVENTODCRAFT12, aColor, DrawStringJustification::DS_ALIGN_CENTER);
	}
	else if (ProgressMeterHasFlags())
	{
		int aNumWavesPerFlag = GetNumWavesPerFlag();
		int aNumFlagWaves = mNumWaves / aNumWavesPerFlag;
		int aFlagsPosEnd = 590 + aCelWidth;  // 旗帜区域的右界横坐标
		for (int aFlagWave = 1; aFlagWave <= aNumFlagWaves; aFlagWave++)
		{
			// 取得旗帜升起时的高度偏移
			int aHeight = 0;
			int aTotalWavesAtFlag = aFlagWave * aNumWavesPerFlag;
			if (aTotalWavesAtFlag < mCurrentWave)
			{
				aHeight = 14;
			}
			else if (aTotalWavesAtFlag == mCurrentWave)
			{
				aHeight = TodAnimateCurveNs(1'000'000'000, 0, mFlagRaiseCounter.mNanoseconds, 0, 14, Curves::Linear);
			}
			// 计算旗帜的横坐标
			int aPosX = TodAnimateCurve(0, mNumWaves, aTotalWavesAtFlag, aFlagsPosEnd, 606, Curves::Linear);
			// 绘制旗杆
			g->DrawImage(Sexy::IMAGE_FLAGMETERPARTS, aPosX, 571, Rect(25, 0, 25, 25));
			// 绘制旗帜
			g->DrawImage(Sexy::IMAGE_FLAGMETERPARTS, aPosX, 572 - aHeight, Rect(50, 0, 25, 25));
		}
	}

	// ====================================================================================================
	// ▲ 绘制进度条的额外部分
	// ====================================================================================================
	// 绘制“关卡进程”的小牌子
	g->DrawImage(Sexy::IMAGE_FLAGMETERLEVELPROGRESS, 638, 589);
	// 判断是否需要绘制进度条当前位置处的小僵尸头，不需要则直接返回
	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM ||
		mApp->IsSquirrelLevel() ||
		mApp->IsSlotMachineLevel() ||
		mApp->IsIZombieLevel() ||
		mApp->IsFinalBossLevel())
		return;
	// 绘制僵尸头
	int aHeadProgress = TodAnimateCurve(0, 150, mProgressMeterWidth, 0, 135, Curves::Linear);
	g->DrawImage(Sexy::IMAGE_FLAGMETERPARTS, aCelWidth - aHeadProgress + 580, 572, Rect(0, 0, 25, 25));
}

namespace
{
// 结算门贴图表（手工生成；BackgroundType 直下标 0..6，槽=7；无 case 背景行
// = nullptr 不绘；static_assert 锚首尾枚举）。
struct DoorImageEntry
{
    Image* const* mImage;
    int mX;
    int mY;
};
constexpr DoorImageEntry kDoorBottom[7] = {
    { &Sexy::IMAGE_BACKGROUND1_GAMEOVER_INTERIOR_OVERLAY, -126, 225 },  // 0 BACKGROUND_1_DAY
    { &Sexy::IMAGE_BACKGROUND2_GAMEOVER_INTERIOR_OVERLAY, -125, 196 },  // 1 BACKGROUND_2_NIGHT
    { &Sexy::IMAGE_BACKGROUND3_GAMEOVER_INTERIOR_OVERLAY, -171, 241 },  // 2 BACKGROUND_3_POOL
    { &Sexy::IMAGE_BACKGROUND4_GAMEOVER_INTERIOR_OVERLAY, -172, 246 },  // 3 BACKGROUND_4_FOG
    { nullptr, 0, 0 },                                                   // 4 BACKGROUND_5_ROOF
    { nullptr, 0, 0 },                                                   // 5 BACKGROUND_6_BOSS
    { nullptr, 0, 0 },                                                   // 6 MUSHROOM_GARDEN（原 default）
};
constexpr DoorImageEntry kDoorTop[7] = {
    { &Sexy::IMAGE_BACKGROUND1_GAMEOVER_MASK, -130, 202 },  // 0 BACKGROUND_1_DAY
    { &Sexy::IMAGE_BACKGROUND2_GAMEOVER_MASK, -128, 207 },  // 1 BACKGROUND_2_NIGHT
    { &Sexy::IMAGE_BACKGROUND3_GAMEOVER_MASK, -172, 234 },  // 2 BACKGROUND_3_POOL
    { &Sexy::IMAGE_BACKGROUND4_GAMEOVER_MASK, -173, 133 },  // 3 BACKGROUND_4_FOG
    { &Sexy::IMAGE_BACKGROUND5_GAMEOVER_MASK, -220, 81 },   // 4 BACKGROUND_5_ROOF
    { &Sexy::IMAGE_BACKGROUND6_GAMEOVER_MASK, -220, 81 },   // 5 BACKGROUND_6_BOSS
    { nullptr, 0, 0 },                                      // 6 MUSHROOM_GARDEN（原 default）
};
static_assert(sizeof(kDoorBottom) / sizeof(kDoorBottom[0]) == 7 &&
    kDoorBottom[0].mImage == &Sexy::IMAGE_BACKGROUND1_GAMEOVER_INTERIOR_OVERLAY &&
    kDoorBottom[3].mImage == &Sexy::IMAGE_BACKGROUND4_GAMEOVER_INTERIOR_OVERLAY, "门底表锚点");
static_assert(sizeof(kDoorTop) / sizeof(kDoorTop[0]) == 7 &&
    kDoorTop[0].mImage == &Sexy::IMAGE_BACKGROUND1_GAMEOVER_MASK &&
    kDoorTop[5].mImage == &Sexy::IMAGE_BACKGROUND6_GAMEOVER_MASK, "门顶表锚点");
}  // namespace

//0x417C80
void Board::DrawHouseDoorBottom(Graphics* g)
{
	// B 形态 -> 常量表（见上 kDoorBottom）；无 case 背景/越界 = nullptr 不绘。
	if (static_cast<unsigned>(mBackground) < 7 && kDoorBottom[(int)mBackground].mImage != nullptr)
	{
		g->DrawImage(*kDoorBottom[(int)mBackground].mImage, kDoorBottom[(int)mBackground].mX, kDoorBottom[(int)mBackground].mY);
	}
}

//0x417CF0
void Board::DrawHouseDoorTop(Graphics* g)
{
	// B 形态 -> 常量表（见上 kDoorTop）；无 case 背景/越界 = nullptr 不绘。
	if (static_cast<unsigned>(mBackground) < 7 && kDoorTop[(int)mBackground].mImage != nullptr)
	{
		g->DrawImage(*kDoorTop[(int)mBackground].mImage, kDoorTop[(int)mBackground].mX, kDoorTop[(int)mBackground].mY);
	}
}

//0x417DA0
// GOTY @Patoke: 0x41A700
void Board::DrawLevel(Graphics* g)
{
    Graphics aHudGraphics(*g);
    g = &aHudGraphics;
	g->Translate(mApp->mScreenBounds.mX + mApp->mScreenBounds.mWidth - BOARD_WIDTH-mX, -mY);
	// ====================================================================================================
	// ▲ 获取完整的关卡名称的字符串
	// C++23 第一批·条目3：值变才重建（键=mLevel+mGameMode+生存旗数/连击段），输出逐字节等价
	// ====================================================================================================
	int aAuxKey;
	if (mApp->IsAdventureMode())
	{
		aAuxKey = -1;
	}
	else if (mApp->IsSurvivalMode() || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND)
	{
		aAuxKey = GetSurvivalFlagsCompleted();
	}
	else if (mApp->IsEndlessIZombie(mApp->mGameMode) || mApp->IsEndlessScaryPotter(mApp->mGameMode))
	{
		aAuxKey = mChallenge->mSurvivalStage + (mNextSurvivalStageCounter > 0 ? 1 : 0);
	}
	else
	{
		aAuxKey = -1;
	}

	// 条目3：函数域 static 缓存（Board 布局被 dat64 整块镜像锁定，不能加成员）；
	// 属主哨兵保证换 Board 实例时首帧重建。
	static Board*     sLevelOwner = nullptr;
	static int        sLevelKeyLevel = 0;
	static int        sLevelKeyMode = 0;
	static int        sLevelKeyAux = 0;
	static SexyString sLevelStr;
	if (sLevelOwner != this || mLevel != sLevelKeyLevel || (int)mApp->mGameMode != sLevelKeyMode || aAuxKey != sLevelKeyAux)
	{
		sLevelOwner = this;
		sLevelKeyLevel = mLevel;
		sLevelKeyMode = (int)mApp->mGameMode;
		sLevelKeyAux = aAuxKey;

		SexyString& aLevelStr = sLevelStr;
		if (mApp->IsAdventureMode())
		{
			aLevelStr = EntityTranslate(gStr.LEVEL) + (" ") + mApp->GetStageString(mLevel);
		}
		else
		{
			aLevelStr = mApp->GetCurrentChallengeDef().mChallengeName;
			if (mApp->IsSurvivalMode() || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND)
			{
				int aFlags = GetSurvivalFlagsCompleted();
				if (aFlags > 0)
				{
					SexyString aFlagStr = mApp->Pluralize(aFlags, gStr.ONE_FLAG.c_str(), gStr.COUNT_FLAGS.c_str());
					SexyString aCompletedStr = TodReplaceString(gStr.FLAGS_COMPLETED, UiTextPlaceholder::Flags, aFlagStr);
					aLevelStr = StrFormat(("%s - %s"), EntityTranslate(aLevelStr).c_str(), aCompletedStr.c_str());
				}
			}
			else if (mApp->IsEndlessIZombie(mApp->mGameMode) || mApp->IsEndlessScaryPotter(mApp->mGameMode))
			{
				int aStreak = mChallenge->mSurvivalStage;
				if (mNextSurvivalStageCounter > 0)
				{
					aStreak++;
				}
				if (aStreak > 0)
				{
					SexyString aStreakStr = TodReplaceNumberString(gStr.ENDLESS_STREAK, UiTextPlaceholder::Streak, aStreak);
					aLevelStr = StrFormat(("%s - %s"), EntityTranslate(aLevelStr).c_str(), aStreakStr.c_str());
				}
			}
		}
	}

	// ====================================================================================================
	// ▲ 正式开始绘制关卡名称字符串
	// ====================================================================================================
	int aPosX = 780;
	int aPosY = 595;
	if (HasProgressMeter())
	{
		aPosX = 593;
	}
	if (mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_ZEN_FADING)
	{
		aPosY += TodAnimateCurveNs(500'000'000, 0, mChallenge->mChallengeStateCounter.mNanoseconds, 0, 50, Curves::EaseInOut);
	}
	TodDrawString(g, sLevelStr, aPosX, aPosY, Sexy::FONT_HOUSEOFTERROR16, Color(224, 187, 98), DrawStringJustification::DS_ALIGN_RIGHT);
}

//0x4182D0
void Board::DrawZenWheelBarrowButton(Graphics* g, const Rect& aButtonRect, int theOffsetY)
{
	PottedPlant* aPlant = mApp->mZenGarden->GetPottedPlantInWheelbarrow();
	if (aPlant && mCursorObject->mCursorType != CursorType::CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW)
	{
		if (mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_ZEN_FADING)
		{
			g->DrawImage(Sexy::IMAGE_ZEN_WHEELBARROW, aButtonRect.mX - 7, aButtonRect.mY + theOffsetY - 3);
		}
		else
		{
			g->DrawImage(Sexy::IMAGE_ZEN_WHEELBARROW, aButtonRect.mX - 7, aButtonRect.mY + theOffsetY + 4);
		}

		if (aPlant->mPlantAge == PottedPlantAge::PLANTAGE_SMALL)
		{
			mApp->mZenGarden->DrawPottedPlant(mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, g, aButtonRect.mX + 23, aButtonRect.mY + theOffsetY - 8, aPlant, 0.6f, true);
		}
		else if (aPlant->mPlantAge == PottedPlantAge::PLANTAGE_MEDIUM)
		{
			mApp->mZenGarden->DrawPottedPlant(mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, g, aButtonRect.mX + 28, aButtonRect.mY + theOffsetY + 2, aPlant, 0.5f, true);
		}
		else
		{
			mApp->mZenGarden->DrawPottedPlant(mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, g, aButtonRect.mX + 34, aButtonRect.mY + theOffsetY + 12, aPlant, 0.4f, true);
		}
	}
	else
	{
		g->DrawImage(Sexy::IMAGE_ZEN_WHEELBARROW, aButtonRect.mX - 7, aButtonRect.mY + theOffsetY - 3);
	}
}

namespace
{
// 禅境工具按钮绘制分发表（C 形态）：表内匿名 lambda = 原 if-else 分支体逐句迁移
// （仅 this 成员改经 aBoard 引用）；行序 = GameObjectType 枚举值序（WATERING_CAN..TREE_FOOD
// 共 9 槽，调用点按 aTool 平移下标）；NEXT_GARDEN 为异构体，保留在调用点。
using ZenToolDrawFn = void (*)(Board&, Graphics*, const Rect&, int);
constexpr ZenToolDrawFn kZenToolDraw[GameObjectType::OBJECT_TYPE_TREE_FOOD - GameObjectType::OBJECT_TYPE_WATERING_CAN + 1] = {
	// 原 if aTool == OBJECT_TYPE_WATERING_CAN
	[](Board& aBoard, Graphics* g, const Rect& aButtonRect, int aOffsetY)
	{
			if (aBoard.mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_GOLD_WATERINGCAN])
			{
				g->DrawImage(Sexy::IMAGE_WATERINGCANGOLD, aButtonRect.mX - 2, aButtonRect.mY + aOffsetY - 6);
			}
			else
			{
				g->DrawImage(Sexy::IMAGE_WATERINGCAN, aButtonRect.mX - 2, aButtonRect.mY + aOffsetY - 6);
			}
	},  // 6 OBJECT_TYPE_WATERING_CAN
	// 原 else if aTool == OBJECT_TYPE_FERTILIZER
	[](Board& aBoard, Graphics* g, const Rect& aButtonRect, int aOffsetY)
	{
			int aCharges = aBoard.mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_FERTILIZER] - PURCHASE_COUNT_OFFSET;
			if (aCharges == 0)
			{
				g->mColorizeImages = true;
				g->mColor = Color(96, 96, 96);
			}
			else if (aBoard.mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_FERTILIZE_PLANTS)
			{
				g->mColorizeImages = true;
				g->mColor = GetFlashingColorNs(aBoard.mLevelTimeNs, 750'000'000);
			}
			g->DrawImage(Sexy::IMAGE_FERTILIZER, aButtonRect.mX - 6, aButtonRect.mY + aOffsetY - 7);
			g->mColorizeImages = false;

			char aChargeBuf[16];
			SexyString aChargeString(1, ('x'));
			aChargeString += FormatInt(aChargeBuf, aCharges);
			TodDrawString(g, aChargeString, aButtonRect.mX + 64, aButtonRect.mY + aOffsetY + 65, Sexy::FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_RIGHT);
	},  // 7 OBJECT_TYPE_FERTILIZER
	// 原 else if aTool == OBJECT_TYPE_BUG_SPRAY
	[](Board& aBoard, Graphics* g, const Rect& aButtonRect, int aOffsetY)
	{
			int aCharges = aBoard.mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_BUG_SPRAY] - PURCHASE_COUNT_OFFSET;
			if (aCharges == 0)
			{
				g->mColorizeImages = true;
				g->mColor = Color(128, 128, 128);
			}
			g->DrawImage(Sexy::IMAGE_BUG_SPRAY, aButtonRect.mX, aButtonRect.mY + aOffsetY - 1);
			g->mColorizeImages = false;

			char aChargeBuf[16];
			SexyString aChargeString(1, ('x'));
			aChargeString += FormatInt(aChargeBuf, aCharges);
			TodDrawString(g, aChargeString, aButtonRect.mX + 64, aButtonRect.mY + aOffsetY + 65, Sexy::FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_RIGHT);
	},  // 8 OBJECT_TYPE_BUG_SPRAY
	// 原 else if aTool == OBJECT_TYPE_PHONOGRAPH
	[](Board& aBoard, Graphics* g, const Rect& aButtonRect, int aOffsetY)
	{
			g->DrawImage(Sexy::IMAGE_PHONOGRAPH, aButtonRect.mX + 2, aButtonRect.mY + aOffsetY + 2);
	},  // 9 OBJECT_TYPE_PHONOGRAPH
	// 原 else if aTool == OBJECT_TYPE_CHOCOLATE
	[](Board& aBoard, Graphics* g, const Rect& aButtonRect, int aOffsetY)
	{
			int aCharges = aBoard.mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_CHOCOLATE] - PURCHASE_COUNT_OFFSET;
			if (aCharges == 0)
			{
				g->mColorizeImages = true;
				g->mColor = Color(128, 128, 128);
			}
			g->DrawImage(Sexy::IMAGE_CHOCOLATE, aButtonRect.mX + 6, aButtonRect.mY + aOffsetY + 4);
			g->mColorizeImages = false;

			char aChargeBuf[16];
			SexyString aChargeString(1, ('x'));
			aChargeString += FormatInt(aChargeBuf, aCharges);
			TodDrawString(g, aChargeString, aButtonRect.mX + 64, aButtonRect.mY + aOffsetY + 65, Sexy::FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_RIGHT);
	},  // 10 OBJECT_TYPE_CHOCOLATE
	// 原 else if aTool == OBJECT_TYPE_GLOVE
	[](Board& aBoard, Graphics* g, const Rect& aButtonRect, int aOffsetY)
	{
			if (aBoard.mCursorObject->mCursorType != CursorType::CURSOR_TYPE_PLANT_FROM_GLOVE &&
				aBoard.mCursorObject->mCursorType != CursorType::CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW)
			{
				g->DrawImage(Sexy::IMAGE_ZEN_GARDENGLOVE, aButtonRect.mX - 6, aButtonRect.mY + aOffsetY - 4);
			}
	},  // 11 OBJECT_TYPE_GLOVE
	// 原 else if aTool == OBJECT_TYPE_MONEY_SIGN
	[](Board& aBoard, Graphics* g, const Rect& aButtonRect, int aOffsetY)
	{
			g->DrawImage(Sexy::IMAGE_ZEN_MONEYSIGN, aButtonRect.mX - 5, aButtonRect.mY + aOffsetY - 4);
	},  // 12 OBJECT_TYPE_MONEY_SIGN
	// 原 else if aTool == OBJECT_TYPE_WHEELBARROW
	[](Board& aBoard, Graphics* g, const Rect& aButtonRect, int aOffsetY)
	{
			aBoard.DrawZenWheelBarrowButton(g, aButtonRect, aOffsetY);
	},  // 13 OBJECT_TYPE_WHEELBARROW
	// 原 else if aTool == OBJECT_TYPE_TREE_FOOD
	[](Board& aBoard, Graphics* g, const Rect& aButtonRect, int aOffsetY)
	{
			int aCharges = aBoard.mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_TREE_FOOD] - PURCHASE_COUNT_OFFSET;
			if (aCharges <= 0)
			{
				g->mColorizeImages = true;
				g->mColor = Color(128, 128, 128);
				aCharges = 0;
			}
			if (!aBoard.mChallenge->TreeOfWisdomCanFeed())
			{
				g->mColorizeImages = true;
				g->mColor = Color(128, 128, 128);
			}
			g->DrawImage(Sexy::IMAGE_TREEFOOD, aButtonRect.mX - 6, aButtonRect.mY + aOffsetY - 7);
			g->mColorizeImages = false;

			char aChargeBuf[16];
			SexyString aChargeString(1, ('x'));
			aChargeString += FormatInt(aChargeBuf, aCharges);
			TodDrawString(g, aChargeString, aButtonRect.mX + 64, aButtonRect.mY + aOffsetY + 65, Sexy::FONT_HOUSEOFTERROR16, Color::White, DS_ALIGN_RIGHT);
	},  // 14 OBJECT_TYPE_TREE_FOOD
};
static_assert(sizeof(kZenToolDraw) / sizeof(kZenToolDraw[0]) == GameObjectType::OBJECT_TYPE_TREE_FOOD - GameObjectType::OBJECT_TYPE_WATERING_CAN + 1,
	"禅境工具绘制表槽位须覆盖 WATERING_CAN..TREE_FOOD");
}  // namespace

//0x418500
void Board::DrawZenButtons(Graphics* g)
{
	std::array<bool, 10> available{};
	static_assert(available.size() == OBJECT_TYPE_NEXT_GARDEN - OBJECT_TYPE_WATERING_CAN + 1);
	for (int slot = 0; slot < int(available.size()); ++slot)
		available[slot] = CanUseGameObject(GameObjectType(OBJECT_TYPE_WATERING_CAN + slot));
	const auto frame = GardenHudLayout::ViewportInBoard(
		mApp->mScreenBounds.mX, mApp->mScreenBounds.mWidth, mX, mY);
	int fadeOffset = 0;
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN &&
		mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_ZEN_FADING)
		fadeOffset = TodAnimateCurveNs(500'000'000, 0, mChallenge->mChallengeStateCounter.mNanoseconds, 0, -72, Curves::EaseInOut);
	const auto positions = GardenHudLayout::LayoutTools(
		available, frame, mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM, fadeOffset);
	Rect aButtonRect = GetShovelButtonRect();
	aButtonRect.mY = positions.y;
	for (GameObjectType aTool = GameObjectType::OBJECT_TYPE_WATERING_CAN; aTool <= GameObjectType::OBJECT_TYPE_NEXT_GARDEN; aTool = (GameObjectType)(aTool + 1))
	{
		const int slot = aTool - OBJECT_TYPE_WATERING_CAN;
		if (!available[slot])
			continue;

		aButtonRect.mX = positions.x[slot];
		if (aTool == GameObjectType::OBJECT_TYPE_NEXT_GARDEN)
		{
			if (!mMenuButton->mBtnNoDraw)
			{
				g->DrawImage(Sexy::IMAGE_ZEN_NEXTGARDEN, aButtonRect.mX + 2, aButtonRect.mY);
			}
		}
		else
		{
			g->DrawImage(Sexy::IMAGE_SHOVELBANK, aButtonRect.mX, aButtonRect.mY);
			if ((int)mCursorObject->mCursorType == (int)CursorType::CURSOR_TYPE_WATERING_CAN + (int)aTool - 6)
			{
				continue;  // 如果工具正在被手持，则跳过绘制
			}

			// C 形态 -> 函数指针表（上方 kZenToolDraw，aTool 平移下标 WATERING_CAN..TREE_FOOD）
			kZenToolDraw[(int)aTool - GameObjectType::OBJECT_TYPE_WATERING_CAN](*this, g, aButtonRect, 0);
		}
	}
}

//0x418B70
void Board::DrawShovel(Graphics* g)
{
    if (mLandscapeHud) { if (mShowShovel) LandscapeHud::DrawShovel(*this,g); return; }
	if (mApp->mGameMode != GameMode::GAMEMODE_LIMBO_ZEN_GARDEN && mApp->mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		if (mShowShovel)
		{
			Rect aShovelRect = GetShovelButtonRect();
			g->DrawImage(Sexy::IMAGE_SHOVELBANK, aShovelRect.mX, aShovelRect.mY);

			if (mCursorObject->mCursorType != CursorType::CURSOR_TYPE_SHOVEL)
			{
				if (mChallenge->mChallengeState == (ChallengeState)15)
				{
					g->mColorizeImages = true;
					g->mColor = GetFlashingColorNs(mLevelTimeNs, 750'000'000);
				}
				g->DrawImage(Sexy::IMAGE_SHOVEL, aShovelRect.mX - 7, aShovelRect.mY - 3);
				g->mColorizeImages = false;
			}
		}
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		DrawZenButtons(g);
	}
}

namespace
{
// Board::DrawDebugText 分发表：表内匿名 lambda = 原 case 体逐句迁移（生成器
// + 逆变换 token 对拍 0 差异）；NONE 槽空操作；default 具名并被越界复用。
using DebugTextCaseFn = void (*)(Board&, SexyString&);

// Music::mMusicBurstState -> 调试文案表（C 形态；MusicBurstState 直下标 0..3，
// 枚举全域无哨兵；表内匿名 lambda = 原 4 支 if 链体逐句迁移）。
using MusicStateLabelFn = void (*)(Music&, SexyString&);
constexpr MusicStateLabelFn kMusicBurstStateLabels[(int)MusicBurstState::MUSIC_BURST_FINISHING + 1] = {
    // 原 MUSIC_BURST_OFF 支
    [](Music& aMusic, SexyString& aText)
    {
	    		aText += StrFormat(("Off"));
    },  // 0 MUSIC_BURST_OFF
    // 原 MUSIC_BURST_STARTING 支
    [](Music& aMusic, SexyString& aText)
    {
				aText += StrFormat(("Starting %dms/%dms"), static_cast<int>(std::max<std::int64_t>(0, aMusic.mBurstEndTimeNs - aMusic.mApp->mApplicationTimeNs) / 1'000'000), 4000);
    },  // 1 MUSIC_BURST_STARTING
    // 原 MUSIC_BURST_ON 支
    [](Music& aMusic, SexyString& aText)
    {
				aText += StrFormat(("On at least until %dms/%dms"), static_cast<int>(std::max<std::int64_t>(0, aMusic.mBurstEndTimeNs - aMusic.mApp->mApplicationTimeNs) / 1'000'000), 8000);
    },  // 2 MUSIC_BURST_ON
    // 原 MUSIC_BURST_FINISHING 支
    [](Music& aMusic, SexyString& aText)
    {
				aText += StrFormat(("Finishing %dms/%dms"), static_cast<int>(std::max<std::int64_t>(0, aMusic.mBurstEndTimeNs - aMusic.mApp->mApplicationTimeNs) / 1'000'000), 4000);
    },  // 3 MUSIC_BURST_FINISHING
};
static_assert(sizeof(kMusicBurstStateLabels) / sizeof(kMusicBurstStateLabels[0]) ==
    (int)MusicBurstState::MUSIC_BURST_FINISHING + 1, "kMusicBurstStateLabels 槽位数须与 MusicBurstState 枚举同步");

// Music::mMusicDrumsState -> 调试文案表（C 形态；MusicDrumsState 直下标 0..4，
// 枚举全域无哨兵；表内匿名 lambda = 原 5 支 if 链体逐句迁移）。
constexpr MusicStateLabelFn kMusicDrumsStateLabels[(int)MusicDrumsState::MUSIC_DRUMS_FADING + 1] = {
    // 原 MUSIC_DRUMS_OFF 支
    [](Music& aMusic, SexyString& aText)
    {
	    		aText += StrFormat((", Drums off"));
    },  // 0 MUSIC_DRUMS_OFF
    // 原 MUSIC_DRUMS_ON_QUEUED 支
    [](Music& aMusic, SexyString& aText)
    {
	    		aText += StrFormat((", Drums queued on"));
    },  // 1 MUSIC_DRUMS_ON_QUEUED
    // 原 MUSIC_DRUMS_ON 支
    [](Music& aMusic, SexyString& aText)
    {
	    		aText += StrFormat((", Drums on"));
    },  // 2 MUSIC_DRUMS_ON
    // 原 MUSIC_DRUMS_OFF_QUEUED 支
    [](Music& aMusic, SexyString& aText)
    {
	    		aText += StrFormat((", Drums queued off"));
    },  // 3 MUSIC_DRUMS_OFF_QUEUED
    // 原 MUSIC_DRUMS_FADING 支
    [](Music& aMusic, SexyString& aText)
    {
				aText += StrFormat((", Drums fading off %dms/%dms"), static_cast<int>(std::max<std::int64_t>(0, aMusic.mDrumsEndTimeNs - aMusic.mApp->mApplicationTimeNs) / 1'000'000), 500);
    },  // 4 MUSIC_DRUMS_FADING
};
static_assert(sizeof(kMusicDrumsStateLabels) / sizeof(kMusicDrumsStateLabels[0]) ==
    (int)MusicDrumsState::MUSIC_DRUMS_FADING + 1, "kMusicDrumsStateLabels 槽位数须与 MusicDrumsState 枚举同步");

// 原 default（无 case 的 0 型与表外哨兵共用）
void DebugTextDefault(Board& aBoard, SexyString& aText)
{
    		TOD_ASSERT(false);
}

// 匿名 lambda 函数指针表，行序 = DebugTextMode 枚举值序（0..4）。
constexpr DebugTextCaseFn kDebugTextCases[(int)DebugTextMode::DEBUG_TEXT_COLLISION + 1] = {
    // 原 case DEBUG_TEXT_NONE
    [](Board& aBoard, SexyString& aText)
    {

    },  // 0 DEBUG_TEXT_NONE
    // 原 case DEBUG_TEXT_ZOMBIE_SPAWN
    [](Board& aBoard, SexyString& aText)
    {
    		int aTime = aBoard.mZombieCountDownStart - aBoard.mZombieCountDown;
    		float aCountDownFraction = (float)aTime / (float)aBoard.mZombieCountDownStart;

    		aText += StrFormat(("ZOMBIE SPAWNING DEBUG\n"));
    		aText += StrFormat(("CurrentWave: %d of %d\n"), aBoard.mCurrentWave, aBoard.mNumWaves);
    		aText += StrFormat(("TimeSinseLastSpawn: %d %s\n"), aTime, aTime > 400 ? ("") : ("(too soon)"));
		aText += StrFormat(("ZombieCountDown: %d/%d(%.0f%%)\n"), static_cast<int>(aBoard.mZombieCountDown), static_cast<int>(aBoard.mZombieCountDownStart), aCountDownFraction);

    		if (aBoard.mZombieHealthToNextWave != -1)
    		{
    			int aTotalHealth = aBoard.TotalZombiesHealthInWave(aBoard.mCurrentWave - 1);
    			int aHealthRange = std::max(aBoard.mZombieHealthWaveStart - aBoard.mZombieHealthToNextWave, 1);
    			float aHealthFraction = (float)(aBoard.mZombieHealthToNextWave - aTotalHealth + aHealthRange) / (float)aHealthRange;
			aText += StrFormat(("ZombieHealth: CurZombieHealth %d trigger %d(%.0f%%)\n"), aTotalHealth, aBoard.mZombieHealthToNextWave, aHealthFraction * 100);
    		}
    		else
    		{
    			aText += StrFormat(("ZombieHealth: before first wave\n"));
    		}

    		if (aBoard.mHugeWaveCountDown > 0)
    		{
				aText += StrFormat(("HugeWaveCountDown: %d\n"), static_cast<int>(aBoard.mHugeWaveCountDown));
    		}

    		Zombie* aBossZombie = aBoard.GetBossZombie();
    		if (aBossZombie)
    		{
				aText += StrFormat(("\nSpawn: %d\n"), static_cast<int>(aBossZombie->mSummonCounter));
				aText += StrFormat(("Stomp: %d\n"), static_cast<int>(aBossZombie->mBossStompCounter));
				aText += StrFormat(("Bungee: %d\n"), static_cast<int>(aBossZombie->mBossBungeeCounter));
				aText += StrFormat(("Head: %d\n"), static_cast<int>(aBossZombie->mBossHeadCounter));
    			aText += StrFormat(("Health: %d of %d\n"), aBossZombie->mBodyHealth, aBossZombie->mBodyMaxHealth);
    		}
    },  // 1 DEBUG_TEXT_ZOMBIE_SPAWN
    // 原 case DEBUG_TEXT_MUSIC
    [](Board& aBoard, SexyString& aText)
    {
    		aText += StrFormat(("MUSIC DEBUG\n"));
    		aText += StrFormat(("CurrentWave: %d of %d\n"), aBoard.mCurrentWave, aBoard.mNumWaves);

    		if (aBoard.mApp->mMusic->mCurMusicFileMain == MusicFile::MUSIC_FILE_NONE)
    		{
    			aText += StrFormat(("No music"));
    		}
    		else
    		{
				aText += StrFormat(("Music Burst: "));

				// C 形态 -> 函数指针表（上方 kMusicBurstStateLabels / kMusicDrumsStateLabels）：
				// 状态直下标（枚举全域），守卫保持原链落空（越界不追加）。
				int aBurstState = (int)aBoard.mApp->mMusic->mMusicBurstState;
				if ((unsigned)aBurstState < (unsigned)(sizeof(kMusicBurstStateLabels) / sizeof(kMusicBurstStateLabels[0])))
				{
					kMusicBurstStateLabels[aBurstState](*aBoard.mApp->mMusic, aText);
				}

				int aDrumsState = (int)aBoard.mApp->mMusic->mMusicDrumsState;
				if ((unsigned)aDrumsState < (unsigned)(sizeof(kMusicDrumsStateLabels) / sizeof(kMusicDrumsStateLabels[0])))
				{
					kMusicDrumsStateLabels[aDrumsState](*aBoard.mApp->mMusic, aText);
				}
				aText += StrFormat(("\n"));

    			int aPackedOrderMain = aBoard.mApp->mMusic->GetMusicOrder(aBoard.mApp->mMusic->mCurMusicFileMain);
    			int aCurrentOrder = LOWORD(aPackedOrderMain);
    			aText += StrFormat(("Music order %02d row %02d\n"), LOWORD(aPackedOrderMain), HIWORD(aPackedOrderMain) / 4);
    			if (aBoard.mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_DAY_GRASSWALK ||
    				aBoard.mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_POOL_WATERYGRAVES ||
    				aBoard.mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_FOG_RIGORMORMIST ||
    				aBoard.mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_ROOF_GRAZETHEROOF)
    			{
    				int aPackedOrderHihats = aBoard.mApp->mMusic->GetMusicOrder(aBoard.mApp->mMusic->mCurMusicFileHihats);
    				int aPackedOrderDrums = aBoard.mApp->mMusic->GetMusicOrder(aBoard.mApp->mMusic->mCurMusicFileDrums);
    				if (aCurrentOrder == LOWORD(aPackedOrderHihats) && aCurrentOrder == LOWORD(aPackedOrderDrums))
    				{
    					int aDiffHihats = HIWORD(aPackedOrderHihats) - HIWORD(aPackedOrderMain);
    					int aDiffDrums = HIWORD(aPackedOrderDrums) - HIWORD(aPackedOrderMain);
    					if (abs(aDiffHihats) > 1 || abs(aDiffDrums) > 1)
    					{
    						aText += StrFormat(("Music unsynced hihats %d drums %d\n"), aDiffHihats, aDiffDrums);
    					}
    				}

    				HMUSIC aMusicHandle1 = aBoard.mApp->mMusic->GetBassMusicHandle(aBoard.mApp->mMusic->mCurMusicFileMain);
    				HMUSIC aMusicHandle2 = aBoard.mApp->mMusic->GetBassMusicHandle(aBoard.mApp->mMusic->mCurMusicFileHihats);
    				HMUSIC aMusicHandle3 = aBoard.mApp->mMusic->GetBassMusicHandle(aBoard.mApp->mMusic->mCurMusicFileDrums);
    				float bpm1;
    				float bpm2;
    				float bpm3;
    				gBass->BASS_ChannelGetAttribute(aMusicHandle1, BASS_ATTRIB_MUSIC_BPM, &bpm1);
    				gBass->BASS_ChannelGetAttribute(aMusicHandle2, BASS_ATTRIB_MUSIC_BPM, &bpm2);
    				gBass->BASS_ChannelGetAttribute(aMusicHandle3, BASS_ATTRIB_MUSIC_BPM, &bpm3);
    				aText += StrFormat(("bpm1 %f bmp2 %f bpm3 %f\n"), bpm1, bpm2, bpm3);
    			}
    			else if (aBoard.mApp->mMusic->mCurMusicTune == MusicTune::MUSIC_TUNE_NIGHT_MOONGRAINS)
    			{
    				int aPackedOrderDrums = aBoard.mApp->mMusic->GetMusicOrder(aBoard.mApp->mMusic->mCurMusicFileDrums);
    				aText += StrFormat(("Drum order %02d row %02d\n"), LOWORD(aPackedOrderDrums), HIWORD(aPackedOrderDrums) / 4);
    				int aDiffDrums = HIWORD(aPackedOrderDrums) - HIWORD(aPackedOrderMain);
    				if (abs(aDiffDrums) > 0 && abs(aDiffDrums) <= 128)
    				{
    					aText += StrFormat(("Drums unsynced %d"), aDiffDrums);
    				}
    			}
    		}
    },  // 2 DEBUG_TEXT_MUSIC
    // 原 case DEBUG_TEXT_MEMORY
    [](Board& aBoard, SexyString& aText)
    {
    		aText += StrFormat(("MEMORY DEBUG\n"));
    		aText += StrFormat(("attachments %d\n"), aBoard.mApp->mEffectSystem->mAttachmentHolder->mAttachments.mSize);
    		aText += StrFormat(("emitters %d\n"), aBoard.mApp->mEffectSystem->mParticleHolder->mEmitters.mSize);
    		aText += StrFormat(("particles %d\n"), aBoard.mApp->mEffectSystem->mParticleHolder->mParticles.mSize);
    		aText += StrFormat(("particle systems %d\n"), aBoard.mApp->mEffectSystem->mParticleHolder->mParticleSystems.mSize);
    		aText += StrFormat(("reanimation %d\n"), aBoard.mApp->mEffectSystem->mReanimationHolder->mReanimations.mSize);
    		aText += StrFormat(("zombies %d\n"), aBoard.mZombies.mSize);
    		aText += StrFormat(("plants %d\n"), aBoard.mPlants.mSize);
    		aText += StrFormat(("projectiles %d\n"), aBoard.mProjectiles.mSize);
    		aText += StrFormat(("coins %d\n"), aBoard.mCoins.mSize);
    		aText += StrFormat(("lawn mowers %d\n"), aBoard.mLawnMowers.mSize);
    		aText += StrFormat(("grid items %d\n"), aBoard.mGridItems.mSize);
    },  // 3 DEBUG_TEXT_MEMORY
    // 原 case DEBUG_TEXT_COLLISION
    [](Board& aBoard, SexyString& aText)
    {
    		aText += StrFormat(("COLLISION DEBUG\n"));
    },  // 4 DEBUG_TEXT_COLLISION
};
static_assert(sizeof(kDebugTextCases) / sizeof(kDebugTextCases[0]) ==
    (int)DebugTextMode::DEBUG_TEXT_COLLISION + 1, "kDebugTextCases 槽位数须与 DebugTextMode 枚举同步");
}  // namespace

//0x418C70
void Board::DrawDebugText(Graphics* g)
{
	SexyString aText;

	// C 形态 -> 匿名 lambda 函数指针表：行序 = DebugTextMode 枚举序（0..4，
	// 全 case 覆盖，NONE 槽空操作）。aText（switch 外提局部，尾部统一
	// 描绘）经引用传入各槽追加；default（TOD_ASSERT）具名并被越界分支复用。
	const unsigned aDebugIndex = static_cast<unsigned>(mDebugTextMode);
	if (aDebugIndex <= static_cast<unsigned>(DebugTextMode::DEBUG_TEXT_COLLISION))
	{
		kDebugTextCases[aDebugIndex](*this, aText);
	}
	else
	{
		DebugTextDefault(*this, aText);
	}


	g->mFont = mDebugFont;
	g->mColor = Color::Black;
	g->DrawStringWordWrapped(aText, 10, 89);
	g->DrawStringWordWrapped(aText, 11, 91);
	g->DrawStringWordWrapped(aText, 9, 90);
	g->DrawStringWordWrapped(aText, 11, 90);
	g->mColor = Color(255, 255, 255);
	g->DrawStringWordWrapped(aText, 10, 90);
}

//0x419AE0
void Board::DrawDebugObjectRects(Graphics* g)
{
	if (mDebugTextMode != DebugTextMode::DEBUG_TEXT_COLLISION)
		return;

	{
		Plant* aPlant = nullptr;
		while (IteratePlants(aPlant))
		{
			Rect aRect = aPlant->GetPlantRect();
			g->mColor = Color(0, 255, 0);
			g->DrawRect(aRect);

			Rect aAttackRect = aPlant->GetPlantAttackRect(PlantWeapon::WEAPON_PRIMARY);
			if (aAttackRect.mWidth < BOARD_WIDTH)
			{
				g->mColor = Color(255, 0, 0);
				g->DrawRect(aAttackRect);
			}

			Rect aSecondaryRect = aPlant->GetPlantAttackRect(PlantWeapon::WEAPON_SECONDARY);
			if (aSecondaryRect.mWidth < BOARD_WIDTH)
			{
				g->mColor = Color(255, 0, 128);
				g->DrawRect(aSecondaryRect);
			}
		}
	}
	{
		Zombie* aZombie = nullptr;
		while (IterateZombies(aZombie))
		{
			if (!aZombie->IsDeadOrDying())
			{
				Rect aRect = aZombie->GetZombieRect();
				g->mColor = Color(0, 255, 0);
				g->DrawRect(aRect);

				Rect aAttackRect = aZombie->GetZombieAttackRect();
				g->mColor = Color(255, 0, 0);
				g->DrawRect(aAttackRect);
			}
		}
	}
	{
		LawnMower* aLawnMower = nullptr;
		while (IterateLawnMowers(aLawnMower))
		{
			Rect aAttackRect = aLawnMower->GetLawnMowerAttackRect();
			g->mColor = Color(255, 0, 0);
			g->DrawRect(aAttackRect);
		}
	}
	{
		Projectile* aProjectile = nullptr;
		while (IterateProjectiles(aProjectile))
		{
			g->mColor = Color(255, 0, 0);
			Rect aDamageRect = aProjectile->GetProjectileRect();
			g->DrawRect(aDamageRect);
		}
	}
}

//0x419EB0
void Board::DrawFadeOut(Graphics* g)
{
	if (mBoardFadeOutCounter < 0 || IsSurvivalStageWithRepick())
		return;

	int anAlpha = TodAnimateCurveNs(2'000'000'000, 0, mBoardFadeOutCounter.mNanoseconds, 0, 255, Curves::Linear);
	if (mLevel == 9 || mLevel == 19 || mLevel == 29 || mLevel == 39 || mLevel == 49)
	{
		g->mColor = Color(0, 0, 0, anAlpha);
	}
	else
	{
		g->mColor = Color(255, 255, 255, anAlpha);
	}
	Rect aScreenRect = mApp->mScreenBounds;
	aScreenRect.Offset(-mX, -mY);
	g->FillRect(aScreenRect);
}

void Board::RefreshHudLayout()
{
	const Rect& aBounds = mApp->mScreenBounds;
	const int aRightDelta = aBounds.mX + aBounds.mWidth - BOARD_WIDTH;
	const bool aZenMenu = mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM;
	mMenuButton->mX = (aZenMenu ? 628 : 681) + aRightDelta;
    if(!aZenMenu) {
        mMenuButton->mX-=mX;mMenuButton->mY=-10-mY;
    }
	if (aZenMenu)
	{
		const auto frame = GardenHudLayout::ViewportInBoard(
			aBounds.mX, aBounds.mWidth, mX, mY);
		const bool fading = mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN &&
			mChallenge->mChallengeState == ChallengeState::STATECHALLENGE_ZEN_FADING;
		mMenuButton->mX = GardenHudLayout::RightButtonX(628, frame);
		mMenuButton->mY = frame.top + (fading ?
			TodAnimateCurveNs(500'000'000, 0, mChallenge->mChallengeStateCounter.mNanoseconds, -10, -50, Curves::EaseInOut) : -10);
		if (mStoreButton)
		{
			const int start = GardenHudLayout::RightButtonX(678, frame);
			mStoreButton->mX = fading ?
				TodAnimateCurveNs(500'000'000, 0, mChallenge->mChallengeStateCounter.mNanoseconds, start, frame.right, Curves::EaseInOut) : start;
			mStoreButton->mY = frame.top + 33;
		}
		mMouseInsets.mTop = aBounds.mY - mY;
		mMouseInsets.mBottom = mY + mHeight - aBounds.mY - aBounds.mHeight;
	}
	// Negative insets admit HUD input in the margins without widening the world.
	mMouseInsets.mLeft = std::min(0, aBounds.mX - mX);
	mMouseInsets.mRight = std::min(0, mX - aRightDelta);
}

//0x419F60
void Board::DrawTopRightUI(Graphics* g)
{
	RefreshHudLayout();

	if (mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_COMPLETED)
	{
		g->mColorizeImages = true;
		g->mColor = GetFlashingColorNs(mLevelTimeNs, 750'000'000);
	}
	mMenuButton->Draw(g);
	g->mColorizeImages = false;

	if (mStoreButton && mApp->mGameMode != GameMode::GAMEMODE_MINIGAME_LAST_STAND)
	{
		if (mTutorialState == TutorialState::TUTORIAL_ZEN_GARDEN_VISIT_STORE)
		{
			g->mColorizeImages = true;
			g->mColor = GetFlashingColorNs(mLevelTimeNs, 750'000'000);
		}
		mStoreButton->Draw(g);
		g->mColorizeImages = false;
	}
}

//0x41A0A0
void Board::DrawUIBottom(Graphics* g)
{
	if (mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
	{
		int aWaveTime = std::abs(static_cast<int>((mLevelTimeNs / 80'000'000) % 22) - 11);
		const Rect aWaveSource(0, aWaveTime * 27, 160, 27);
		g->mDrawMode = Graphics::DRAWMODE_ADDITIVE;
		g->DrawImage(Sexy::IMAGE_WAVESIDE, 0, 40, aWaveSource);
		g->DrawImage(Sexy::IMAGE_WAVECENTER, 160, 40, aWaveSource);
		g->DrawImage(Sexy::IMAGE_WAVECENTER, 320, 40, aWaveSource);
		g->DrawImage(Sexy::IMAGE_WAVECENTER, 480, 40, aWaveSource);
		TodDrawImageRectScaled(g, Sexy::IMAGE_WAVESIDE, 800, 40, aWaveSource, -1.0f, 1.0f);
		if (mApp->mScreenBounds.mWidth > BOARD_WIDTH)
		{
			const int left = std::min(0, mApp->mScreenBounds.mX - mX);
			const int right = std::max(BOARD_WIDTH,
				mApp->mScreenBounds.mX + mApp->mScreenBounds.mWidth - mX);
			constexpr int waveWidth = 160;
			Graphics aLeftWaves(*g);
			aLeftWaves.ClipRect(left, 0, -left, BOARD_HEIGHT);
			for (int edge = 0; edge > left; edge -= waveWidth)
				TodDrawImageRectScaled(&aLeftWaves, Sexy::IMAGE_WAVESIDE, edge, 40, aWaveSource, -1.0f, 1.0f);
			Graphics aRightWaves(*g);
			aRightWaves.ClipRect(BOARD_WIDTH, 0, right - BOARD_WIDTH, BOARD_HEIGHT);
			for (int edge = BOARD_WIDTH; edge < right; edge += waveWidth)
				aRightWaves.DrawImage(Sexy::IMAGE_WAVESIDE, edge, 40, aWaveSource);
		}
		g->mDrawMode = Graphics::DRAWMODE_NORMAL;
	}

	if (mBackground == BackgroundType::BACKGROUND_GREENHOUSE || mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
	{
		g->mDrawMode = Graphics::DRAWMODE_ADDITIVE;
		g->DrawImage(
			IMAGE_BACKGROUND_GREENHOUSE_OVERLAY,
			Rect(0, 0, BOARD_WIDTH, BOARD_HEIGHT),
			Rect(0, 0, IMAGE_BACKGROUND_GREENHOUSE_OVERLAY->mWidth, IMAGE_BACKGROUND_GREENHOUSE_OVERLAY->mHeight)
		);
		if (mApp->mScreenBounds.mWidth > BOARD_WIDTH)
		{
			// Reflect only the margins, retaining the existing central light mask and scale.
			const int left = std::min(0, mApp->mScreenBounds.mX - mX);
			const int right = std::max(BOARD_WIDTH,
				mApp->mScreenBounds.mX + mApp->mScreenBounds.mWidth - mX);
			const Rect aSource(0, 0, IMAGE_BACKGROUND_GREENHOUSE_OVERLAY->mWidth,
				IMAGE_BACKGROUND_GREENHOUSE_OVERLAY->mHeight);
			if (left < 0)
			{
				Graphics aLeftLight(*g);
				aLeftLight.ClipRect(left, 0, -left, BOARD_HEIGHT);
				aLeftLight.DrawImageMirror(IMAGE_BACKGROUND_GREENHOUSE_OVERLAY,
					Rect(-BOARD_WIDTH, 0, BOARD_WIDTH, BOARD_HEIGHT), aSource);
			}
			if (right > BOARD_WIDTH)
			{
				Graphics aRightLight(*g);
				aRightLight.ClipRect(BOARD_WIDTH, 0, right - BOARD_WIDTH, BOARD_HEIGHT);
				aRightLight.DrawImageMirror(IMAGE_BACKGROUND_GREENHOUSE_OVERLAY,
					Rect(BOARD_WIDTH, 0, BOARD_WIDTH, BOARD_HEIGHT), aSource);
			}
		}
		g->mDrawMode = Graphics::DRAWMODE_NORMAL;
	}

    if(!mLandscapeHud)DrawCardHud(g);
	if (!StageHasFog()&&!mLandscapeHud)
	{
		DrawTopRightUI(g);
	}
}

void Board::DrawCardHud(Graphics* g)
{
	if (mApp->mGameScene != GameScenes::SCENE_ZOMBIES_WON)
	{
		const float aBeforeSeedBankX = g->mTransX, aBeforeSeedBankY = g->mTransY;
		if (mSeedBank->BeginDraw(g))
		{
			mSeedBank->Draw(g);
			mSeedBank->EndDraw(g, aBeforeSeedBankX, aBeforeSeedBankY);
		}

		if (mAdvice->mMessageStyle == MessageStyle::MESSAGE_STYLE_SLOT_MACHINE)
		{
			mAdvice->Draw(g);
		}
	}

	DrawShovel(g);
    // Cards, tools and selection corners always share the selected HUD layer.
    DrawTouchSelection(g);
}

//0x41A2A0
void Board::DrawUICoinBank(Graphics* g)
{
	if (mApp->mGameScene != GameScenes::SCENE_PLAYING && mApp->mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_OFF)
		return;

	if (mCoinBankFadeCount <= 0)
		return;

	int aPosX = 57 + mApp->mScreenBounds.mX;
	int aPosY = 599 - Sexy::IMAGE_COINBANK->mHeight;
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN || mApp->mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_OFF)
	{
		aPosX = 450 - mX;
	}

	g->mColorizeImages = true;
	int anAlpha = ClampInt(255 * mCoinBankFadeCount / 15, 0, 255);
	g->mColor = Color(255, 255, 255, anAlpha);
	g->DrawImage(Sexy::IMAGE_COINBANK, aPosX, aPosY);

	g->mColor = Color(180, 255, 90, anAlpha);
	g->mFont = Sexy::FONT_CONTINUUMBOLD14;
	SexyString aCoinLabel = mApp->GetMoneyString(mApp->mPlayerInfo->mCoins);
	g->DrawString(aCoinLabel, aPosX + 116 - Sexy::FONT_CONTINUUMBOLD14->StringWidth(aCoinLabel), aPosY + 24);
	g->mColorizeImages = false;
}

//0x41A430
void Board::ClearFogAroundPlant(Plant* thePlant, int theSize)
{
	//int aFogFadeOutSpeed = mFogBlownCountDown >= 2000 ? 40 : mFogBlownCountDown > 0 ? 2 : 6;
	int aFogFadeOutSpeed = 6;
	if (mFogBlownCountDown > 0 && mFogBlownCountDown < 2000)
	{
		aFogFadeOutSpeed = 2;
	}
	else if (mFogBlownCountDown > 0)
	{
		aFogFadeOutSpeed = 40;
	}

	int aLeft = LeftFogColumn();
	int aFogOffsetX = (mFogOffset + 50) / 100;
	int aStartX = thePlant->mPlantCol - theSize - aFogOffsetX;
	int aEndX = thePlant->mPlantCol + theSize - aFogOffsetX;
	aStartX = std::max(aStartX, aLeft);
	aEndX = std::min(aEndX, MAX_GRID_SIZE_X - 1);

	int aStartY = thePlant->mRow - theSize;
	int aEndY = thePlant->mRow + theSize;
	aStartY = std::max(aStartY, 0);
	aEndY = std::min(aEndY, MAX_GRID_SIZE_Y);

	for (int x = aStartX; x <= aEndX; x++)
	{
		for (int y = aStartY; y <= aEndY; y++)
		{
			int aDistX = abs(x + aFogOffsetX - thePlant->mPlantCol);
			int aDistY = abs(y - thePlant->mRow);
			if (theSize == 4)
			{
				if (aDistX > 3 || aDistY > 2)
				{
					continue;
				}
				if (aDistX + aDistY == 5)
				{
					continue;
				}
			}
			else if (aDistX + aDistY > theSize)
			{
				continue;
			}

			mGridCelFog[x][y] = std::max(mGridCelFog[x][y] - aFogFadeOutSpeed, 0);
		}
	}
}

//0x41A5D0
void Board::UpdateFog()
{
	if (!StageHasFog())
		return;

	//int aFogFadeInSpeed = mFogBlownCountDown >= 2000 ? 20 : mFogBlownCountDown > 0 ? 1 : 3;
	int aFogFadeInSpeed = 3;
	if (mFogBlownCountDown > 0 && mFogBlownCountDown < 2000)
	{
		aFogFadeInSpeed = 1;
	}
	else if (mFogBlownCountDown > 0)
	{
		aFogFadeInSpeed = 20;
	}

	int aLeft = LeftFogColumn();
	for (int x = aLeft; x < MAX_GRID_SIZE_X; x++)
	{
		for (int y = 0; y < MAX_GRID_SIZE_Y + 1; y++)
		{
			int aFogMax = x == aLeft ? 200 : 255;
			mGridCelFog[x][y] = std::min(mGridCelFog[x][y] + aFogFadeInSpeed, aFogMax);
		}
	}

	Plant* aPlant = nullptr;
	while (IteratePlants(aPlant))
	{
		if (aPlant->NotOnGround())
			continue;

		if (aPlant->mSeedType == SeedType::SEED_PLANTERN)
		{
			ClearFogAroundPlant(aPlant, 4);
		}
		else if (aPlant->mSeedType == SeedType::SEED_TORCHWOOD)
		{
			ClearFogAroundPlant(aPlant, 1);
		}
	}
}

//0x41A730
void Board::DrawFog(Graphics* g)
{
	Image* aImageFog = Sexy::IMAGE_FOG;
	const Rect aFogFrame(0, 0, 210, 190);
	for (int x = 0; x < MAX_GRID_SIZE_X; x++)
	{
		for (int y = 0; y < MAX_GRID_SIZE_Y + 1; y++)
		{
			int aFadeAmount = mGridCelFog[x][y];
			if (aFadeAmount == 0)
				continue;

			// 取得格子内的雾的形状（第 6 行的雾的形状采用与第 0 行相同）
			// { sub eax,edx } 向前 [y / 6] 列，但 y 超出上限 y - 5 行，故相当于列不变，行 = y % 6；
			int aCelLook = mGridCelLook[x][y % MAX_GRID_SIZE_Y];
			int aCelCol = aCelLook % 8;
			// 本格浓雾横坐标 = 列 * 80 + 浓雾偏移 - 15，纵坐标 = 行 * 85 + 20
			float aPosX = x * 80 + mFogOffset - 15;
			float aPosY = y * 85 + 20;
			// 900 帧和 500 帧两个周期的最小公倍数为 4500；先取模可避免长时间运行后 float 丢失相位精度。
			constexpr std::uint64_t FOG_ANIMATION_PERIOD_NS = 45'000'000'000;
			float aTime = static_cast<float>(double(mLevelTimeNs % FOG_ANIMATION_PERIOD_NS) /
				10'000'000.0) * PI * 2;
			// 与行、列有关的初始相位
			float aPhaseX = 6 * PI * x / MAX_GRID_SIZE_X;
			float aPhaseY = 6 * PI * y / (MAX_GRID_SIZE_Y + 1);
			// 根据初相和时间计算当前相位
			float aMotion = 13 + 4 * sin(aTime / 900 + aPhaseY) + 8 * sin(aTime / 500 + aPhaseX);

			int aColorVariant = 255 - aCelLook * 1.5 - aMotion * 1.5;
			int aLightnessVariant = 255 - aCelLook - aMotion;
			g->mColorizeImages = true;
			g->mColor = Color(aColorVariant, aColorVariant, aLightnessVariant, aFadeAmount);
			const Rect aFogSource(aCelCol * aFogFrame.mWidth, 0, aFogFrame.mWidth, aFogFrame.mHeight);
			g->DrawImage(aImageFog, aPosX, aPosY, aFogSource);

			if (x == MAX_GRID_SIZE_X - 1)
			{
				g->DrawImage(aImageFog, aPosX + 80, aPosY, aFogSource);
			}
			g->mColorizeImages = false;
		}
	}
}

//0x41AA00
bool Board::IsScaryPotterDaveTalking()
{
	return mApp->IsScaryPotterLevel() && mNextSurvivalStageCounter > 0 && mApp->mCrazyDaveState != CrazyDaveState::CRAZY_DAVE_OFF;
}

//0x41AA40
void Board::DrawUITop(Graphics* g)
{
	Rect aScreenRect = mApp->mScreenBounds;
	aScreenRect.Offset(-mX, -mY);

	if (StageHasFog()||mLandscapeHud)
	{
		DrawTopRightUI(g);
	}

	if (mTimeStopCounter > 0)
	{
		g->mColor = Color(200, 200, 200, 210);
		g->FillRect(aScreenRect);
	}

    if(mLandscapeHud)DrawCardHud(g);

	if (mApp->mGameScene == GameScenes::SCENE_PLAYING || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		DrawProgressMeter(g);
		DrawLevel(g);
	}
	if (mStoreButton && mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND)
	{
		mStoreButton->Draw(g);
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_INTRO && mCutScene->mUpsellHideBoard)
	{
		g->mColor = Color(0, 0, 0);
		g->FillRect(aScreenRect);
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_INTRO)
	{
		mCutScene->DrawIntro(g);
	}

	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO ||
		mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		IsScaryPotterDaveTalking())
	{
		Graphics aScreenSpace(*g);
		aScreenSpace.mTransX -= mX;
		aScreenSpace.mTransY -= mY;
		mApp->DrawCrazyDave(&aScreenSpace);
	}

	if (mAdvice->mMessageStyle != MessageStyle::MESSAGE_STYLE_SLOT_MACHINE)
	{
		mAdvice->Draw(g);
	}

	const float aBeforeCursorX = g->mTransX, aBeforeCursorY = g->mTransY;
    DrawTouchCursors(g);
	if (mTimeStopCounter == 0 && mCursorObject->BeginDraw(g))
	{
		mCursorObject->Draw(g);
		mCursorObject->EndDraw(g, aBeforeCursorX, aBeforeCursorY);
	}

	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && mApp->mSeedChooserScreen &&
		mCutScene->mSeedChoosing)
		mApp->mSeedChooserScreen->DrawZombieToolTip(g);
	else if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
		mToolTip->Draw(g, Rect(mApp->mScreenBounds.mX - mX, -mY,
			mApp->mScreenBounds.mWidth, std::min(BOARD_HEIGHT, mToolTip->mMaxBottom)));
	else
		mToolTip->Draw(g);
	DrawDebugText(g);
	DrawDebugObjectRects(g);
}

//0x41ACF0
void Board::Draw(Graphics* g)
{
	if (mApp->GetDialog(Dialogs::DIALOG_STORE) || mApp->GetDialog(Dialogs::DIALOG_ALMANAC))
		return;

	g->mLinearBlend = true;

	if (mDrawCount && mCutScene->mPreloaded)
	{
		static const unsigned int aTickCount = SexyGetTickCount();
		const int aIntervalDraws = Sexy::FrameCounterMath::SignedDelta32(
			static_cast<uint32_t>(mDrawCount), static_cast<uint32_t>(mIntervalDrawCountStart));
		const int aInterval = Sexy::FrameCounterMath::SignedDelta32(aTickCount, mIntervalDrawTime);
		if (aInterval > 10000)
		{
			const float aIntervalFPS = Sexy::FrameCounterMath::BoardIntervalFps(aIntervalDraws, aInterval);
			if (mMinFPS > aIntervalFPS)
			{
				mMinFPS = aIntervalFPS;
			}
			mIntervalDrawCountStart = mDrawCount;
			mIntervalDrawTime = aTickCount;
		}
	}
	else
	{
		ResetFPSStats();
	}

	mDrawCount = Sexy::FrameCounterMath::IncrementSigned32(mDrawCount);
	DrawGameObjects(g);
}

//0x41AE60
// GOTY @Patoke: 0x41D910
void Board::SetMustacheMode(bool theEnableMustache)
{
	mApp->PlayFoley(FoleyType::FOLEY_POLEVAULT);
	mMustacheMode = theEnableMustache;
	mApp->mMustacheMode = theEnableMustache;
	// 1073 SetMustacheMode (0x41D910) grants the immediate in-level popup here.
	ReportAchievement::GiveAchievement(mApp, MustacheMode, true);

	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		aZombie->EnableMustache(theEnableMustache);
	}
}

//0x41AF00
void Board::SetFutureMode(bool theEnableFuture)
{
	mApp->PlaySample(Sexy::SOUND_BOING);
	mFutureMode = theEnableFuture;
	mApp->mFutureMode = theEnableFuture;

	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		aZombie->EnableFuture(theEnableFuture);
	}
}

//0x41AF60
void Board::SetPinataMode(bool theEnablePinata)
{
	mApp->PlayFoley(FoleyType::FOLEY_JUICY);
	mPinataMode = theEnablePinata;
	mApp->mPinataMode = theEnablePinata;
}

//0x41AFD0
void Board::SetDanceMode(bool theEnableDance)
{
	mApp->PlayFoley(FoleyType::FOLEY_DANCER);
	mDanceMode = theEnableDance;
	mApp->mDanceMode = theEnableDance;

	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		if (!aZombie->mDead)
		{
			aZombie->EnableDance(static_cast<std::int64_t>(mLevelTimeNs));
		}
	}
}

//0x41B0D0
void Board::SetSuperMowerMode(bool theEnableSuperMower)
{
	mApp->PlayFoley(FoleyType::FOLEY_ZAMBONI);
	mSuperMowerMode = theEnableSuperMower;
	mApp->mSuperMowerMode = theEnableSuperMower;

	LawnMower* aLawnMower = nullptr;
	while (IterateLawnMowers(aLawnMower))
	{
		aLawnMower->EnableSuperMower(theEnableSuperMower);
	}
}

void Board::SetDaisyMode(bool theEnableDaisy)
{
	mApp->PlaySample(SOUND_LOADINGBAR_FLOWER);
	mDaisyMode = theEnableDaisy;
	mApp->mDaisyMode = theEnableDaisy;
}

void Board::SetSukhbirMode(bool theEnableSukhbir)
{
	mApp->PlaySample(Sexy::SOUND_SUKHBIR);
	mSukhbirMode = theEnableSukhbir;
	mApp->mSukhbirMode = theEnableSukhbir;
}

//0x41B1D0
void Board::DoTypingCheck(KeyCode theKey)
{
	if (mApp->mKonamiCheck->Check(theKey))
	{
		mApp->PlayFoley(FoleyType::FOLEY_DROP);
		return;
	}
	if (mApp->mMustacheCheck->Check(theKey) || mApp->mMoustacheCheck->Check(theKey))
	{
		SetMustacheMode(!mMustacheMode);
		return;
	}
	if (mApp->mSuperMowerCheck->Check(theKey) || mApp->mSuperMowerCheck2->Check(theKey))
	{
		SetSuperMowerMode(!mSuperMowerMode);
		return;
	}
	if (mApp->mFutureCheck->Check(theKey))
	{
		SetFutureMode(!mFutureMode);
		return;
	}
	if (mApp->mPinataCheck->Check(theKey))
	{
		if (mApp->CanDoPinataMode())
		{
			SetPinataMode(!mPinataMode);
			return;
		}
		else
		{
			if (mApp->mGameScene == GameScenes::SCENE_PLAYING)
			{
				DisplayAdvice(gStr.CANT_USE_CODE, MessageStyle::MESSAGE_STYLE_BIG_MIDDLE_FAST, AdviceType::ADVICE_NONE);
			}
			mApp->PlaySample(Sexy::SOUND_BUZZER);
			return;
		}
	}
	if (mApp->mDanceCheck->Check(theKey))
	{
		if (mApp->CanDoDanceMode())
		{
			SetDanceMode(!mDanceMode);
			return;
		}
		else
		{
			if (mApp->mGameScene == GameScenes::SCENE_PLAYING)
			{
				DisplayAdvice(gStr.CANT_USE_CODE, MessageStyle::MESSAGE_STYLE_BIG_MIDDLE_FAST, AdviceType::ADVICE_NONE);
			}
			mApp->PlaySample(Sexy::SOUND_BUZZER);
			return;
		}
	}
	if (mApp->mDaisyCheck->Check(theKey))
	{
		if (mApp->CanDoDaisyMode())
		{
			SetDaisyMode(!mDaisyMode);
			return;
		}
		else
		{
			if (mApp->mGameScene == GameScenes::SCENE_PLAYING)
			{
				DisplayAdvice(gStr.CANT_USE_CODE, MessageStyle::MESSAGE_STYLE_BIG_MIDDLE_FAST, AdviceType::ADVICE_NONE);
			}
			mApp->PlaySample(Sexy::SOUND_BUZZER);
			return;
		}
	}
	if (mApp->mSukhbirCheck->Check(theKey))
	{
		SetSukhbirMode(!mSukhbirMode);
		return;
	}
}

//0x41B820
void Board::KeyDown(KeyCode theKey)
{
	DoTypingCheck(theKey);

	if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO &&
		mApp->mGameMode != GameMode::GAMEMODE_LIMBO_ZEN_GARDEN &&
		mApp->mGameMode != GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		mCutScene->KeyDown(theKey);
	}
	else if (theKey == KeyCode::KEYCODE_RETURN || theKey == KeyCode::KEYCODE_SPACE)
	{
		if (IsScaryPotterDaveTalking() && mApp->mCrazyDaveMessageIndex != -1)
		{
			mChallenge->AdvanceCrazyDaveDialog();
		}
		else if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
		{
			mApp->mZenGarden->AdvanceCrazyDaveDialog();
		}
		else if (theKey == KeyCode::KEYCODE_SPACE && mApp->CanPauseNow())
		{
			mApp->PlaySample(Sexy::SOUND_PAUSE);
			mApp->DoPauseDialog();
		}
	}
	else if (theKey == KeyCode::KEYCODE_ESCAPE)
	{
		if (mCursorObject->mCursorType != CursorType::CURSOR_TYPE_NORMAL)
		{
			RefreshSeedPacketFromCursor();
		}
		else if (CanInteractWithBoardButtons() && mApp->mGameScene != GameScenes::SCENE_ZOMBIES_WON)
		{
			mApp->DoNewOptions(false);
		}
	}
}

static void TodCrash()
{
	TOD_ASSERT(false, "Crash%s", "!!!!");
}

namespace
{
// 智慧树作弊数字键 '0'-'8' → mChallengeRecords 记录值表（B 形态；平移下标 theChar-'0'）。
constexpr int kTreeWisdomCheatRecord[9] = { 0, 9, 19, 29, 39, 49, 98, 498, 998 };
static_assert(sizeof(kTreeWisdomCheatRecord) / sizeof(kTreeWisdomCheatRecord[0]) == 9,
	"智慧树作弊键表槽位须覆盖 '0'..'8'");
}  // namespace

//0x41B950（原版中废弃）
void Board::KeyChar(SexyChar theChar)
{
	if (!mApp->mDebugKeysEnabled)
		return;

	TodTraceAndLog("Board cheat key '%c'", theChar);

	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		if (theChar == ('m'))
		{
			if (!mApp->mZenGarden->IsZenGardenFull(true))
			{
				PottedPlant aPottedPlant;
				aPottedPlant.InitializePottedPlant(SeedType::SEED_MARIGOLD);
				aPottedPlant.mDrawVariation = (DrawVariation)RandRangeInt((int)DrawVariation::VARIATION_MARIGOLD_WHITE, (int)DrawVariation::VARIATION_MARIGOLD_LIGHT_GREEN);
				mApp->mZenGarden->AddPottedPlant(&aPottedPlant);
			}
			return;
		}

		if (theChar == ('+'))
		{
			if (!mApp->mZenGarden->IsZenGardenFull(true))
			{
				PottedPlant aPottedPlant;
				aPottedPlant.InitializePottedPlant(mApp->mZenGarden->PickRandomSeedType());
				mApp->mZenGarden->AddPottedPlant(&aPottedPlant);
			}
			return;
		}

		if (theChar == ('a'))
		{
			if (!mApp->mZenGarden->IsZenGardenFull(true))
			{
				PottedPlant aPottedPlant;
				aPottedPlant.InitializePottedPlant(mApp->mZenGarden->PickRandomSeedType());
				aPottedPlant.mPlantAge = PottedPlantAge::PLANTAGE_FULL;
				mApp->mZenGarden->AddPottedPlant(&aPottedPlant);
			}
			return;
		}

		if (theChar == ('f'))
		{
			Plant* aPlant = nullptr;
			while (IteratePlants(aPlant))
			{
				if (GetZenToolAt(aPlant->mPlantCol, aPlant->mRow) == nullptr && aPlant->mPottedPlantIndex >= 0)
				{
					PottedPlant* aPottedPlant = mApp->mZenGarden->PottedPlantFromIndex(aPlant->mPottedPlantIndex);
					PottedPlantNeed aNeed = mApp->mZenGarden->GetPlantsNeed(aPottedPlant);
					if (aNeed == PottedPlantNeed::PLANTNEED_WATER)
					{
						aPlant->mHighlighted = true;
						mApp->mZenGarden->MouseDownWithFeedingTool((aPlant->mX), (aPlant->mY), CursorType::CURSOR_TYPE_WATERING_CAN);
						return;
					}
					else if (aNeed == PottedPlantNeed::PLANTNEED_FERTILIZER)
					{
						aPlant->mHighlighted = true;
						if (mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_FERTILIZER] <= PURCHASE_COUNT_OFFSET)
						{
							mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_FERTILIZER] = PURCHASE_COUNT_OFFSET + 1;
						}
						mApp->mZenGarden->MouseDownWithFeedingTool((aPlant->mX), (aPlant->mY), CursorType::CURSOR_TYPE_FERTILIZER);
						return;
					}
					else if (aNeed == PottedPlantNeed::PLANTNEED_BUGSPRAY)
					{
						aPlant->mHighlighted = true;
						if (mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_BUG_SPRAY] <= PURCHASE_COUNT_OFFSET)
						{
							mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_BUG_SPRAY] = PURCHASE_COUNT_OFFSET + 1;
						}
						mApp->mZenGarden->MouseDownWithFeedingTool((aPlant->mX), (aPlant->mY), CursorType::CURSOR_TYPE_BUG_SPRAY);
						return;
					}
					else if (aNeed == PottedPlantNeed::PLANTNEED_PHONOGRAPH)
					{
						aPlant->mHighlighted = true;
						mApp->mZenGarden->MouseDownWithFeedingTool((aPlant->mX), (aPlant->mY), CursorType::CURSOR_TYPE_PHONOGRAPH);
						return;
					}
				}
			}
			return;
		}

		if (theChar == ('r'))
		{
			Plant* aPlant = nullptr;
			while (IteratePlants(aPlant))
			{
				if (aPlant->mPottedPlantIndex >= 0)
				{
					TOD_ASSERT(aPlant->mPottedPlantIndex < mApp->mPlayerInfo->mNumPottedPlants);
					PottedPlant* aPottedPlant = &mApp->mPlayerInfo->mPottedPlant[aPlant->mPottedPlantIndex];
					aPottedPlant->mLastWateredTime = 0;
					aPottedPlant->mLastNeedFulfilledTime = 0;
					aPottedPlant->mLastFertilizedTime = 0;
					aPottedPlant->mLastChocolateTime = 0;
				}
			}
			return;
		}

		if (theChar == ('s'))
		{
			if (mApp->mZenGarden->IsStinkySleeping())
			{
				mApp->mZenGarden->WakeStinky();
			}
			else
			{
				mApp->mZenGarden->ResetStinkyTimers();
			}
			return;
		}

		if (theChar == ('c'))
		{
			if (mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_CHOCOLATE] < PURCHASE_COUNT_OFFSET)
			{
				mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_CHOCOLATE] = PURCHASE_COUNT_OFFSET + 1;
			}
			else
			{
				mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_CHOCOLATE]++;
			}
			return;
		}

		if (theChar == (']'))
		{
			PottedPlant* aPottedPlant = mApp->mZenGarden->GetPottedPlantInWheelbarrow();
			if (aPottedPlant)
			{
				aPottedPlant->mSeedType = (SeedType)((int)aPottedPlant->mSeedType + 1);
				if (aPottedPlant->mSeedType == SeedType::SEED_GATLINGPEA)
				{
					aPottedPlant->mSeedType = SeedType::SEED_PEASHOOTER;
				}
				if (aPottedPlant->mSeedType == SeedType::SEED_FLOWERPOT)
				{
					aPottedPlant->mSeedType = SeedType::SEED_KERNELPULT;
				}
			}
			return;
		}
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		if (theChar == ('f'))
		{
			if (mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_TREE_FOOD] <= PURCHASE_COUNT_OFFSET)
			{
				mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_TREE_FOOD] = PURCHASE_COUNT_OFFSET + 1;
			}
			mChallenge->TreeOfWisdomFertilize();
		}
		else if (theChar == ('g'))
		{
			mChallenge->TreeOfWisdomGrow();
		}
		else if (theChar == ('b'))
		{
			mChallenge->mChallengeStateCounter = 1;
		}
		else if (theChar >= ('0') && theChar <= ('8'))
		{
			// B 形态 -> 常量表（上方 kTreeWisdomCheatRecord，theChar-'0' 平移下标）
			mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()] = kTreeWisdomCheatRecord[theChar - ('0')];
			mChallenge->TreeOfWisdomGrow();
		}

		return;
	}

	if (theChar == ('<'))
	{
		mApp->DoNewOptions(false);
	}
	else if (theChar == ('l'))
	{
		mApp->DoCheatDialog();
	}
	else if (theChar == ('#'))
	{
		if (mApp->IsSurvivalMode())
		{
			if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
			{
				return;
			}

			mCurrentWave = mNumWaves;
			mChallenge->mSurvivalStage += 5;
			RemoveAllZombies();
			FadeOutLevel();
		}
	}
	else if (theChar == ('!'))
	{
		mApp->mBoardResult = BoardResult::BOARDRESULT_CHEAT;
		if (IsLastStandStageWithRepick())
		{
			if (mNextSurvivalStageCounter == 0)
			{
				mCurrentWave = mNumWaves;
				RemoveAllZombies();
				FadeOutLevel();
			}
		}
		else if ((mApp->IsScaryPotterLevel() && !IsFinalScaryPotterStage()) || mApp->IsEndlessIZombie(mApp->mGameMode))
		{
			if (mNextSurvivalStageCounter == 0)
			{
				RemoveAllZombies();
				FadeOutLevel();
			}
		}
		else if (mApp->IsSurvivalMode())
		{
			if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
			{
				return;
			}

			mCurrentWave = mNumWaves;
			//if (!IsSurvivalStageWithRepick())
			//{
				RemoveAllZombies();
			//}
			FadeOutLevel();
		}
		else
		{
			RemoveAllZombies();
			FadeOutLevel();
			mBoardFadeOutCounter = 200;
		}
	}
	else if (theChar == ('+'))
	{
		mApp->mBoardResult = BoardResult::BOARDRESULT_CHEAT;
		if (IsLastStandStageWithRepick())
		{
			if (mNextSurvivalStageCounter == 0)
			{
				mCurrentWave = mNumWaves;
				RemoveAllZombies();
				FadeOutLevel();
			}
		}
		else if ((mApp->IsScaryPotterLevel() && !IsFinalScaryPotterStage()) || mApp->IsEndlessIZombie(mApp->mGameMode))
		{
			if (mNextSurvivalStageCounter == 0)
			{
				RemoveAllZombies();
				FadeOutLevel();
			}
		}
		else if (mApp->IsSurvivalEndless(mApp->mGameMode))
		{
			if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO)
			{
				return;
			}

			mCurrentWave = mNumWaves;
			RemoveAllZombies();
			FadeOutLevel();
		}
		else if (mApp->IsSurvivalMode())
		{
			mChallenge->mSurvivalStage = 5;
			RemoveAllZombies();
			FadeOutLevel();
			mBoardFadeOutCounter = 200;
		}
		else
		{
			RemoveAllZombies();
			FadeOutLevel();
			mBoardFadeOutCounter = 200;
		}
	}
	else
	{
		// C 形态 -> 函数指针配对表（参考 CreditScreen kKeyJumpRows 先例）：原 '8'/'7'/'6'/'z'
		// 4 支纯 char->动作支逐句迁移；键序 = 原链书写序（char 键互斥无遮蔽）。
		// 表外保留：前置 '<'/'l'/'#'/'!'/'+' 5 支判别式含复合模式谓词（见上方原链）。
		using CheatKeyFn = void (*)(Board&);
		static constexpr struct { char mChar; CheatKeyFn mAction; } kCheatKeyActions[] = {
			{ '8', [](Board& aBoard) { aBoard.mApp->mEasyPlantingCheat = !aBoard.mApp->mEasyPlantingCheat; } },  // 原 '8' 支
			{ '7', [](Board& aBoard) { aBoard.mApp->ToggleSlowMo(); } },                                       // 原 '7' 支
			{ '6', [](Board& aBoard) { aBoard.mApp->ToggleFastMo(); } },                                       // 原 '6' 支
			{ 'z', [](Board& aBoard)
				{
					aBoard.mDebugTextMode = (DebugTextMode)((int)aBoard.mDebugTextMode + 1);
					if (aBoard.mDebugTextMode > DebugTextMode::DEBUG_TEXT_COLLISION)
					{
						aBoard.mDebugTextMode = DebugTextMode::DEBUG_TEXT_NONE;
					}
				} },                                                                                          // 原 'z' 支
		};
		for (const auto& aKeyRow : kCheatKeyActions)
		{
			if (aKeyRow.mChar == theChar)
			{
				aKeyRow.mAction(*this);
				break;
			}
		}
	}

	if (mApp->mGameScene != GameScenes::SCENE_PLAYING)
	{
		return;
	}

	Zombie* aBossZombie = GetBossZombie();
	if (aBossZombie && !aBossZombie->IsDeadOrDying())
	{
		if (theChar == ('b'))
		{
			aBossZombie->mBossBungeeCounter = 0;
			return;
		}
		if (theChar == ('u'))
		{
			aBossZombie->mSummonCounter = 0;
			return;
		}
		if (theChar == ('s'))
		{
			aBossZombie->mBossStompCounter = 0;
			return;
		}
		if (theChar == ('r'))
		{
			aBossZombie->BossRVAttack(static_cast<std::int64_t>(mLevelTimeNs));
			return;
		}
		if (theChar == ('h'))
		{
			aBossZombie->mBossHeadCounter = 0;
			return;
		}
		if (theChar == ('d'))
		{
			aBossZombie->TakeDamage(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), 10000, 0U);
			return;
		}
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_WAR_AND_PEAS || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_WAR_AND_PEAS_2)
	{
		if (theChar == ('w'))
		{
			AddZombie(ZombieType::ZOMBIE_WALLNUT_HEAD, Zombie::ZOMBIE_WAVE_DEBUG);
			return;
		}
		if (theChar == ('t'))
		{
			AddZombie(ZombieType::ZOMBIE_TALLNUT_HEAD, Zombie::ZOMBIE_WAVE_DEBUG);
			return;
		}
		if (theChar == ('j'))
		{
			AddZombie(ZombieType::ZOMBIE_JALAPENO_HEAD, Zombie::ZOMBIE_WAVE_DEBUG);
			return;
		}
		if (theChar == ('g'))
		{
			AddZombie(ZombieType::ZOMBIE_GATLING_HEAD, Zombie::ZOMBIE_WAVE_DEBUG);
			return;
		}
		if (theChar == ('s'))
		{
			AddZombie(ZombieType::ZOMBIE_SQUASH_HEAD, Zombie::ZOMBIE_WAVE_DEBUG);
			return;
		}
	}

	if (theChar == ('q'))
	{
		if (mApp->IsSurvivalEndless(mApp->mGameMode))
		{
			mApp->mEasyPlantingCheat = true;
			for (int y = 0; y < MAX_GRID_SIZE_X; y++)
			{
				for (int x = 0; x < MAX_GRID_SIZE_Y; x++)
				{
					if (CanPlantAt(x, y, SeedType::SEED_LILYPAD) == PlantingReason::PLANTING_OK)
					{
						AddPlant(x, y, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
					}
					if (CanPlantAt(x, y, SeedType::SEED_PUMPKINSHELL) == PlantingReason::PLANTING_OK)
					{
						if (x <= 6 || IsPoolSquare(x, y))
						{
							AddPlant(x, y, SeedType::SEED_PUMPKINSHELL, SeedType::SEED_NONE);
						}
					}
					if (CanPlantAt(x, y, SeedType::SEED_GATLINGPEA) == PlantingReason::PLANTING_OK)
					{
						if (x < 5)
						{
							AddPlant(x, y, SeedType::SEED_GATLINGPEA, SeedType::SEED_NONE);
						}
						else if (x == 5)
						{
							AddPlant(x, y, SeedType::SEED_TORCHWOOD, SeedType::SEED_NONE);
						}
						else if (x == 6)
						{
							AddPlant(x, y, SeedType::SEED_SPLITPEA, SeedType::SEED_NONE);
						}
						else if (y == 2 || y == 3)
						{
							AddPlant(x, y, SeedType::SEED_GLOOMSHROOM, SeedType::SEED_NONE);
							if (CanPlantAt(x, y, SeedType::SEED_INSTANT_COFFEE) == PlantingReason::PLANTING_OK)
							{
								AddPlant(x, y, SeedType::SEED_INSTANT_COFFEE, SeedType::SEED_NONE);
							}
						}
					}
				}
			}
		}
		else if (mApp->IsIZombieLevel())
		{
			mApp->mEasyPlantingCheat = true;
			for (int i = 0; i < 5; i++)
			{
				mChallenge->IZombiePlaceZombie(ZombieType::ZOMBIE_FOOTBALL, 6, i);
			}
		}
		else
		{
			mApp->mEasyPlantingCheat = true;
			for (int y = 0; y < MAX_GRID_SIZE_Y; ++y)
			{
				for (int x = 0; x < MAX_GRID_SIZE_X; ++x)
				{
					if (StageHasRoof() && CanPlantAt(x, y, SeedType::SEED_FLOWERPOT) == PlantingReason::PLANTING_OK)
					{
						AddPlant(x, y, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
					}
					if (CanPlantAt(x, y, SeedType::SEED_LILYPAD) == PlantingReason::PLANTING_OK)
					{
						AddPlant(x, y, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
					}
					if (CanPlantAt(x, y, SeedType::SEED_THREEPEATER) == PlantingReason::PLANTING_OK)
					{
						AddPlant(x, y, SeedType::SEED_THREEPEATER, SeedType::SEED_NONE);
					}
				}
			}

			const ZombieSpawningUpdateResult aChallengeResult = mChallenge->UpdateZombieSpawning();
			if (aChallengeResult == ZombieSpawningUpdateResult::USE_BOARD_WAVE_SCHEDULER)
			{
				int aWavesRemaining = std::min(mNumWaves - mCurrentWave, 20);
				while (aWavesRemaining)
				{
					SpawnZombieWave();
					aWavesRemaining--;
				}
			}

			if (mApp->IsScaryPotterLevel())
			{
				GridItem* aGridItem = nullptr;
				while (IterateGridItems(aGridItem))
				{
					if (aGridItem->mGridItemType == GridItemType::GRIDITEM_SCARY_POT)
					{
						mChallenge->ScaryPotterOpenPot(aGridItem);
					}
				}
			}
		}

		return;
	}

	if (theChar == ('O'))
	{
		mApp->mEasyPlantingCheat = true;
		for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
		{
			for (int x = 0; x < 3; x++)
			{
				if (CanPlantAt(x, y, SeedType::SEED_FLOWERPOT) == PlantingReason::PLANTING_OK)
				{
					AddPlant(x, y, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
				}
			}
		}
		return;
	}

	if (theChar == ('b'))
	{
		AddZombie(ZombieType::ZOMBIE_BUNGEE, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('o'))
	{
		AddZombie(ZombieType::ZOMBIE_FOOTBALL, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('s'))
	{
		AddZombie(ZombieType::ZOMBIE_DOOR, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('L'))
	{
		AddZombie(ZombieType::ZOMBIE_LADDER, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('y'))
	{
		AddZombie(ZombieType::ZOMBIE_YETI, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('a'))
	{
		AddZombie(ZombieType::ZOMBIE_FLAG, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('w'))
	{
		AddZombie(ZombieType::ZOMBIE_NEWSPAPER, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('F'))
	{
		AddZombie(ZombieType::ZOMBIE_BALLOON, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('n'))
	{
		if (StageHasPool())
		{
			AddZombie(ZombieType::ZOMBIE_SNORKEL, Zombie::ZOMBIE_WAVE_DEBUG);
		}
	}
	if (theChar == ('c'))
	{
		AddZombie(ZombieType::ZOMBIE_TRAFFIC_CONE, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('m'))
	{
		AddZombie(ZombieType::ZOMBIE_DANCER, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('h'))
	{
		AddZombie(ZombieType::ZOMBIE_PAIL, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	//if (theChar == ('H')
	//{
	//	AddZombie(ZombieType::ZOMBIE_PAIL, Zombie::ZOMBIE_WAVE_DEBUG);
	//	AddZombie(ZombieType::ZOMBIE_PAIL, Zombie::ZOMBIE_WAVE_DEBUG);
	//	AddZombie(ZombieType::ZOMBIE_PAIL, Zombie::ZOMBIE_WAVE_DEBUG);
	//	AddZombie(ZombieType::ZOMBIE_PAIL, Zombie::ZOMBIE_WAVE_DEBUG);
	//	AddZombie(ZombieType::ZOMBIE_PAIL, Zombie::ZOMBIE_WAVE_DEBUG);
	//	return;
	//}
	if (theChar == ('D'))
	{
		AddZombie(ZombieType::ZOMBIE_DIGGER, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('p'))
	{
		AddZombie(ZombieType::ZOMBIE_POLEVAULTER, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('P'))
	{
		AddZombie(ZombieType::ZOMBIE_POGO, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('R'))
	{
		if (StageHasPool())
		{
			AddZombie(ZombieType::ZOMBIE_DOLPHIN_RIDER, Zombie::ZOMBIE_WAVE_DEBUG);
		}
		return;
	}
	else if(theChar == ('j'))
	{
		AddZombie(ZombieType::ZOMBIE_JACK_IN_THE_BOX, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('g'))
	{
		AddZombie(ZombieType::ZOMBIE_GARGANTUAR, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('G'))
	{
		AddZombie(ZombieType::ZOMBIE_REDEYE_GARGANTUAR, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('i'))
	{
		AddZombie(ZombieType::ZOMBIE_ZAMBONI, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('C'))
	{
		AddZombie(ZombieType::ZOMBIE_CATAPULT, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('1'))
	{
		Plant* aPlant = GetTopPlantAt(0, 0, PlantPriority::TOPPLANT_ANY);
		if (aPlant)
		{
			aPlant->Die(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
			mChallenge->ZombieAtePlant(aPlant);
			return;
		}
	}
	if (theChar == ('B'))
	{
		mFogBlownCountDown = 2200;
		return;
	}
	if (theChar == ('t'))
	{
		if (!CanAddBobSled())
		{
			int aRow = Rand(5);
			int aPos = 400;
			if (StageHasPool())
			{
				aRow = Rand(2);
			}
			else if (StageHasRoof())
			{
				aPos = 500;
			}
			mIceTimer[aRow] = 3000;
			mIceMinX[aRow] = aPos;
		}

		AddZombie(ZombieType::ZOMBIE_BOBSLED, Zombie::ZOMBIE_WAVE_DEBUG);
		return;
	}
	if (theChar == ('r'))
	{
		SpawnZombiesFromGraves();
		return;
	}
	if (theChar == ('0'))
	{
		AddSunMoney(100);
		mApp->PlaySample(SOUND_BUTTONCLICK);
		return;
	}
	if (theChar == ('9'))
	{
		AddSunMoney(999999);
		mApp->PlaySample(SOUND_BUTTONCLICK);
		return;
	}
	if (theChar == ('$'))
	{
		mApp->mPlayerInfo->AddCoins(100);
		mApp->PlaySample(SOUND_BUTTONCLICK);
		mCoinBankFadeCount = 1000;
		return;
	}
	if (theChar == ('-'))
	{
		mSunMoney -= 100;
		if (mSunMoney < 0)
		{
			mSunMoney = 0;
		}
		return;
	}
	if (theChar == ('M'))
	{
		mApp->mMusic->mBurstOverride -= 2 - (mApp->mMusic->mBurstOverride != 1);
		return;
	}

	if (theChar == ('\3') && mApp->mCtrlDown && mApp->mTodCheatKeys)
	{
		TodCrash();
	}
}

//0x41B960
// GOTY @Patoke: 0x41E6E0
void Board::AddSunMoney(int theAmount)
{
	mSunMoney += theAmount;
	if (mSunMoney > 9990)
	{
		mSunMoney = 9990;
	}
	if (mSunMoney >= 8000)
	{
		if (mApp->mPlayerInfo && !mApp->mPlayerInfo->mEarnedAchievements[SunnyDays])
		{
			ReportAchievement::GiveAchievement(mApp, SunnyDays, true);
		}
	}
}

//0x41B980
int Board::CountSunBeingCollected()
{
	int aCount = 0;
	Coin* aCoin = nullptr;
	while (IterateCoins(aCoin))
	{
		if (aCoin->mIsBeingCollected && aCoin->IsSun())
		{
			aCount += aCoin->GetSunValue();
		}
	}
	return aCount;
}

//0x41B9F0
int Board::CountCoinsBeingCollected()
{
	int aCount = 0;
	Coin* aCoin = nullptr;
	while (IterateCoins(aCoin))
	{
		if (aCoin->mIsBeingCollected && aCoin->IsMoney())
		{
			aCount += aCoin->GetCoinValue(aCoin->mType);
		}
	}
	return aCount;
}

//0x41BA60
bool Board::TakeSunMoney(int theAmount)
{
	if (CanTakeSunMoney(theAmount))
	{
		mSunMoney -= theAmount;
		return true;
	}

	mApp->PlaySample(Sexy::SOUND_BUZZER);
	mOutOfMoneyCounter = 70;
	return false;
}

//0x41BAB0
bool Board::CanTakeSunMoney(int theAmount)
{
	return theAmount <= mSunMoney + CountSunBeingCollected();
}

//0x41BAD0
void Board::ProcessDeleteQueue()
{
	{
		Plant* aPlant = nullptr;
		while (mPlants.IterateNext(aPlant))
		{
			if (aPlant->mDead)
			{
				// 格桶：链摘除在 free 前完成（free 后字节不可读）；摘除即
				// 链顶替 O(1)——桶中垂死株保持可见直至本帧 free，与原版
				// 池扫时间线一致（原版亦不滤 mDead）。摘链走共享注销
				// （2026-10-06 玉米炮双格修复：与注册完全镜像，含右格边界）
				UnregisterPlantCellBucket(aPlant, aPlant->mRow, aPlant->mPlantCol);
				mPlants.DataArrayFree(aPlant);
			}
		}
	}
	{
		Zombie* aZombie = nullptr;
		while (mZombies.IterateNext(aZombie))
		{
			if (aZombie->mDead)
			{
				// 行段桶：摘除在 free 前完成（free 后字节不可读）；桶中
				// 垂死僵尸保持可见直至本帧 free，与原版池扫时间线一致
				RemoveZombieRowBucket(aZombie);
				mZombies.DataArrayFree(aZombie);
			}
		}
	}
	{
		Projectile* aProjectile = nullptr;
		while (mProjectiles.IterateNext(aProjectile))
		{
			if (aProjectile->mDead)
			{
				mProjectiles.DataArrayFree(aProjectile);
			}
		}
	}
	{
		Coin* aCoin = nullptr;
		while (mCoins.IterateNext(aCoin))
		{
			if (aCoin->mDead)
			{
				mCoins.DataArrayFree(aCoin);
			}
		}
	}
	{
		LawnMower* aLawnMower = nullptr;
		while (mLawnMowers.IterateNext(aLawnMower))
		{
			if (aLawnMower->mDead)
			{
				mLawnMowers.DataArrayFree(aLawnMower);
			}
		}
	}
	{
		GridItem* aGridItem = nullptr;
		while (mGridItems.IterateNext(aGridItem))
		{
			if (aGridItem->mDead)
			{
				mGridItems.DataArrayFree(aGridItem);
			}
		}
	}
}

//0x41BE50
// GOTY @Patoke: 0x41EC10
bool Board::HasConveyorBeltSeedBank() const
{
	return
		mApp->IsFinalBossLevel() ||
		mApp->IsMiniBossLevel() ||
		mApp->IsShovelLevel() ||
		mApp->IsWallnutBowlingLevel() ||
		mApp->IsLittleTroubleLevel() ||
		mApp->IsStormyNightLevel() ||
		mApp->IsBungeeBlitzLevel() ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_PORTAL_COMBAT ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_COLUMN ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_INVISIGHOUL;
}

//0x41BEE0
int Board::GetNumSeedsInBank()
{
	if (mApp->IsScaryPotterLevel())
	{
		return 1;
	}
	if (mApp->IsWhackAZombieLevel())
	{
		return 3;
	}
	if (mApp->IsChallengeWithoutSeedBank())
	{
		return 0;
	}
	if (HasConveyorBeltSeedBank())
	{
		return 10;
	}
	if (mApp->IsSlotMachineLevel())
	{
		return 3;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ICE)
	{
		return 6;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST)
	{
		return 0;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM)
	{
		return 2;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_1 || mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_2 ||
		mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_3 || mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_4)
	{
		return 3;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_5 || mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_6 ||
		mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_7)
	{
		return 4;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_8)
	{
		return 6;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_9)
	{
		return 8;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS)
	{
		return 9;
	}

	int aNumSeeds = mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PACKET_UPGRADE] + 6;
	int aSeedsAvailable = mApp->GetSeedsAvailable();
	return std::min(aNumSeeds, aSeedsAvailable);
}

//0x41C010
bool Board::StageIsNight()
{
	return
		mBackground == BackgroundType::BACKGROUND_2_NIGHT ||
		mBackground == BackgroundType::BACKGROUND_4_FOG ||
		mBackground == BackgroundType::BACKGROUND_6_BOSS ||
		mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN ||
		mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM;
}

//0x41C040
bool Board::StageHasGraveStones()
{
	if (mApp->IsWallnutBowlingLevel() ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_POGO_PARTY ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_PORTAL_COMBAT ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND ||
		mApp->IsIZombieLevel() ||
		mApp->IsScaryPotterLevel())
		return false;

	return mBackground == BackgroundType::BACKGROUND_2_NIGHT;
}

//0x41C0B0
bool Board::StageHasRoof()
{
	return (mBackground == BackgroundType::BACKGROUND_5_ROOF || mBackground == BackgroundType::BACKGROUND_6_BOSS);
}

//0x41C0D0
bool Board::StageHasPool()
{
	return (mBackground == BackgroundType::BACKGROUND_3_POOL || mBackground == BackgroundType::BACKGROUND_4_FOG);
}

bool Board::StageHas6Rows()
{
	return (mBackground == BackgroundType::BACKGROUND_3_POOL || mBackground == BackgroundType::BACKGROUND_4_FOG);
}

//0x41C0F0
bool Board::StageHasZombieWalkInFromRight()
{
	if (mApp->IsWhackAZombieLevel() ||
		mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ICE ||
		mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM ||
		mApp->IsFinalBossLevel() ||
		mApp->IsIZombieLevel() ||
		mApp->IsSquirrelLevel() ||
		mApp->IsScaryPotterLevel())
		return false;

	return true;
}

//0x41C170
bool Board::StageHasFog()
{
	return !mApp->IsStormyNightLevel() && mApp->mGameMode != GameMode::GAMEMODE_MINIGAME_INVISIGHOUL && mBackground == BackgroundType::BACKGROUND_4_FOG;
}

// GOTY @Patoke: inlined 0x41E669
bool Board::StageIsDayWithoutPool() {
	return mBackground == BackgroundType::BACKGROUND_1_DAY;
}

// GOTY @Patoke: inlined 0x41E5E6
bool Board::StageIsDayWithPool() {
	return mBackground == BackgroundType::BACKGROUND_3_POOL;
}

//0x41C1C0
int Board::LeftFogColumn()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_AIR_RAID)		return 6;
	if (!mApp->IsAdventureMode())										return 5;
	if (mLevel == 31)													return 6;
	if (mLevel >= 32 && mLevel <= 36)									return 5;
	if (mLevel >= 37 && mLevel <= 40)									return 4;
	TOD_ASSERT(false);

	return 5;
}

//0x41C210
int Board::GetSeedPacketPositionX(int theIndex)
{
	if (mApp->IsSlotMachineLevel())			return theIndex * 59 + 247;
	if (HasConveyorBeltSeedBank())			return theIndex * 50 + 91;

	if (mSeedBank->mNumPackets <= 7)		return theIndex * 59 + 85;
	else if (mSeedBank->mNumPackets == 8)	return theIndex * 54 + 81;
	else if (mSeedBank->mNumPackets == 9)	return theIndex * 52 + 80;
	else									return theIndex * 51 + 79;
}

//0x41C290
int Board::GetSeedBankExtraWidth()
{
	int aNumPackets = mSeedBank->mNumPackets;
	return aNumPackets <= 6 ? 0 : aNumPackets == 7 ? 60 : aNumPackets == 8 ? 76 : aNumPackets == 9 ? 112 : 153;
}

void Board::OffsetYForPlanting(int& theY, SeedType theSeedType)
{
	if (Plant::IsFlying(theSeedType) || theSeedType == SeedType::SEED_GRAVEBUSTER)
	{
		theY += 15;
	}
	if (theSeedType == SeedType::SEED_SPIKEWEED || theSeedType == SeedType::SEED_SPIKEROCK)
	{
		theY -= 15;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN && mBackground == BackgroundType::BACKGROUND_GREENHOUSE)
	{
		theY -= 25;
	}
}

//0x41C2D0
int Board::PlantingPixelToGridX(int theX, int theY, SeedType theSeedType)
{
	OffsetYForPlanting(theY, theSeedType);
	return PixelToGridX(theX, theY);
}

//0x41C320
int Board::PlantingPixelToGridY(int theX, int theY, SeedType theSeedType)
{
	OffsetYForPlanting(theY, theSeedType);

	int aGridY = PixelToGridY(theX, theY);
	if (theSeedType == SeedType::SEED_INSTANT_COFFEE)
	{
		int aGridX = PixelToGridX(theX, theY);

		Plant* aPlant = GetTopPlantAt(aGridX, aGridY, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
		if (aPlant && aPlant->mIsAsleep)
		{
			return aGridY;
		}

		int aGridYDown = PixelToGridY(theX, theY + 30);
		if (aGridYDown != aGridY)
		{
			Plant* aPlantDown = GetTopPlantAt(aGridX, aGridYDown, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
			if (aPlantDown && aPlantDown->mIsAsleep)
			{
				return aGridYDown;
			}
		}

		int aGridYUp = PixelToGridY(theX, theY - 50);
		if (aGridYUp != aGridY)
		{
			Plant* aPlantUp = GetTopPlantAt(aGridX, aGridYUp, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
			if (aPlantUp && aPlantUp->mIsAsleep)
			{
				return aGridYUp;
			}
		}
	}
	return aGridY;
}

//0x41C4C0
int Board::PixelToGridX(int theX, int theY)
{
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		if (mBackground == BackgroundType::BACKGROUND_GREENHOUSE ||
			mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN ||
			mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
		{
			return mApp->mZenGarden->PixelToGridX(theX, theY);
		}
	}

	if (theX < LAWN_XMIN)
		return -1;

	return ClampInt((theX - LAWN_XMIN) / 80, 0, MAX_GRID_SIZE_X - 1);
}

//0x41C530
int Board::PixelToGridXKeepOnBoard(int theX, int theY)
{
	int aGridX = PixelToGridX(theX, theY);
	return std::max(aGridX, 0);
}

//0x41C550
int Board::PixelToGridY(int theX, int theY)
{
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		if (mBackground == BackgroundType::BACKGROUND_GREENHOUSE ||
			mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN ||
			mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
		{
			return mApp->mZenGarden->PixelToGridY(theX, theY);
		}
	}

	int aGridX = PixelToGridX(theX, theY);
	if (aGridX == -1 || theY < LAWN_YMIN)
		return -1;

	if (StageHasRoof())
	{
		if (aGridX < 5)
		{
			theY -= (4 - aGridX) * 20;
		}
		return ClampInt((theY - LAWN_YMIN) / 85, 0, MAX_GRID_SIZE_Y - 2);
	}
	else if (StageHasPool())
	{
		return ClampInt((theY - LAWN_YMIN) / 85, 0, MAX_GRID_SIZE_Y - 1);
	}
	else
	{
		return ClampInt((theY - LAWN_YMIN) / 100, 0, MAX_GRID_SIZE_Y - 2);
	}
}

//0x41C650
int Board::PixelToGridYKeepOnBoard(int theX, int theY)
{
	int aGridY = PixelToGridY(std::max(theX, 80), theY);
	return std::max(aGridY, 0);
}

//0x41C680
int Board::GridToPixelX(int theGridX, int theGridY)
{
	TOD_ASSERT(theGridX >= 0 && theGridX < MAX_GRID_SIZE_X);
	TOD_ASSERT(theGridY >= 0 && theGridY < MAX_GRID_SIZE_Y);
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		if (mBackground == BackgroundType::BACKGROUND_GREENHOUSE ||
			mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN ||
			mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
		{
			return mApp->mZenGarden->GridToPixelX(theGridX, theGridY);
		}
	}

	return theGridX * 80 + LAWN_XMIN;
}

//0x41C6C0
float Board::GetPosYBasedOnRow(float thePosX, int theRow)
{
	if (StageHasRoof())
	{
		float aSlopeOffset = 0.0f;
		if (thePosX < 440.0f)
		{
			aSlopeOffset = (440.0f - thePosX) * 0.25f;
		}

		return GridToPixelY(8, theRow) + aSlopeOffset;
	}

	return GridToPixelY(0, theRow);
}

//0x41C740
int Board::GridToPixelY(int theGridX, int theGridY)
{
	TOD_ASSERT(theGridX >= 0 && theGridX < MAX_GRID_SIZE_X);
	TOD_ASSERT(theGridY >= 0 && theGridY < MAX_GRID_SIZE_Y);
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		if (mBackground == BackgroundType::BACKGROUND_GREENHOUSE ||
			mBackground == BackgroundType::BACKGROUND_MUSHROOM_GARDEN ||
			mBackground == BackgroundType::BACKGROUND_ZOMBIQUARIUM)
		{
			return mApp->mZenGarden->GridToPixelY(theGridX, theGridY);
		}
	}

	int aY;
	if (StageHasRoof())
	{
		int aSlopeOffset;
		if (theGridX < 5)
		{
			aSlopeOffset = (5 - theGridX) * 20;
		}
		else
		{
			aSlopeOffset = 0;
		}
		aY = theGridY * 85 + aSlopeOffset + LAWN_YMIN - 10;
	}
	else if (StageHasPool())
	{
		aY = theGridY * 85 + LAWN_YMIN;
	}
	else
	{
		aY = theGridY * 100 + LAWN_YMIN;
	}

	if (theGridX != -1 && mGridSquareType[theGridX][theGridY] == GridSquareType::GRIDSQUARE_HIGH_GROUND)
	{
		aY -= HIGH_GROUND_HEIGHT;
	}

	return aY;
}

ZombieID Board::ZombieGetID(Zombie* theZombie)
{
	return (ZombieID)mZombies.DataArrayGetID(theZombie);
}

Zombie* Board::ZombieGet(ZombieID theZombieID)
{
	return mZombies.DataArrayGet((unsigned int)theZombieID);
}

//0x41C7F0
Zombie* Board::ZombieTryToGet(ZombieID theZombieID)
{
	return mZombies.DataArrayTryToGet((unsigned int)theZombieID);
}

//0x41C820
int GetRectOverlap(const Rect& rect1, const Rect& rect2)
{
	int xmax, rmin, rmax;

	if (rect1.mX < rect2.mX)
	{
		rmin = rect1.mX + rect1.mWidth;
		rmax = rect2.mX + rect2.mWidth;
		xmax = rect2.mX;
	}
	else
	{
		rmin = rect2.mX + rect2.mWidth;
		rmax = rect1.mX + rect1.mWidth;
		xmax = rect1.mX;
	}

	if (rmin > xmax && rmin > rmax)
	{
		rmin = rmax;
	}

	return rmin - xmax;
}

//0x41C850
bool GetCircleRectOverlap(int theCircleX, int theCircleY, int theRadius, const Rect& theRect)
{
	int dx = 0;  // 圆心与矩形较近一条纵边的横向距离
	int dy = 0;  // 圆心与矩形较近一条横边的纵向距离
	bool xOut = false;  // 圆心横坐标是否不在矩形范围内
	bool yOut = false;  // 圆心纵坐标是否不在矩形范围内

	if (theCircleX < theRect.mX)
	{
		xOut = true;
		dx = theRect.mX - theCircleX;
	}
	else if (theCircleX > theRect.mX + theRect.mWidth)
	{
		xOut = true;
		dx = theCircleX - theRect.mX - theRect.mWidth;
	}
	if (theCircleY < theRect.mY)
	{
		yOut = true;
		dy = theRect.mY - theCircleY;
	}
	else if (theCircleY > theRect.mY + theRect.mHeight)
	{
		yOut = true;
		dy = theCircleY - theRect.mY - theRect.mHeight;
	}

	if (!xOut && !yOut)  // 如果圆心在矩形内
	{
		return true;
	}
	else if (xOut && yOut)
	{
		return dx * dx + dy * dy <= theRadius * theRadius;
	}
	else if (xOut)
	{
		return dx <= theRadius;
	}
	else
	{
		return dy <= theRadius;
	}
}

//0x41C8F0
// GOTY @Patoke: 0x41F6B0
bool Board::IterateZombies(Zombie*& theZombie)
{
	while (mZombies.IterateNext(theZombie))
	{
		if (!theZombie->mDead)
		{
			return true;
		}
	}

	theZombie = (Zombie*)-1;
	return false;
}

//0x41C950
bool Board::IteratePlants(Plant*& thePlant)
{
	while (mPlants.IterateNext(thePlant))
	{
		if (!thePlant->mDead)
		{
			return true;
		}
	}

	thePlant = (Plant*)-1;
	return false;
}

//0x41C9B0
bool Board::IterateProjectiles(Projectile*& theProjectile)
{
	while (mProjectiles.IterateNext(theProjectile))
	{
		if (!theProjectile->mDead)
		{
			return true;
		}
	}

	theProjectile = (Projectile*)-1;
	return false;
}

//0x41CA10
bool Board::IterateCoins(Coin*& theCoin)
{
	while (mCoins.IterateNext(theCoin))
	{
		if (!theCoin->mDead)
		{
			return true;
		}
	}

	theCoin = (Coin*)-1;
	return false;
}

//0x41CA70
bool Board::IterateLawnMowers(LawnMower*& theLawnMower)
{
	while (mLawnMowers.IterateNext(theLawnMower))
	{
		if (!theLawnMower->mDead)
		{
			return true;
		}
	}

	theLawnMower = (LawnMower*)-1;
	return false;
}

//0x41CAD0
bool Board::IterateGridItems(GridItem*& theGridItem)
{
	while (mGridItems.IterateNext(theGridItem))
	{
		if (!theGridItem->mDead)
		{
			return true;
		}
	}

	theGridItem = (GridItem*)-1;
	return false;
}

//0x41CB30
bool Board::IterateParticles(TodParticleSystem*& theParticle)
{
	while (mApp->mEffectSystem->mParticleHolder->mParticleSystems.IterateNext(theParticle))
	{
		if (!theParticle->mDead)
		{
			return true;
		}
	}

	theParticle = (TodParticleSystem*)-1;
	return false;
}

bool Board::IterateReanimations(Reanimation*& theReanimation)
{
	while (mApp->mEffectSystem->mReanimationHolder->mReanimations.IterateNext(theReanimation))
	{
		if (!theReanimation->mDead)
		{
			return true;
		}
	}

	theReanimation = (Reanimation*)-1;
	return false;
}

//0x41CBF0
void Board::KillAllPlantsInRadius(int theX, int theY, int theRadius)
{
	Plant* aPlant = nullptr;
	while (IteratePlants(aPlant))
	{
		if (GetCircleRectOverlap(theX, theY, theRadius, aPlant->GetPlantRect()))
		{
			mPlantsEaten++;
			aPlant->Die(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
		}
	}
}

//0x41CC60
unsigned int Board::SeedNotRecommendedForLevel(SeedType theSeedType)
{
	unsigned int aNotRec = 0;
	if (Plant::IsNocturnal(theSeedType) && !StageIsNight())
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_NOCTURNAL, true);
	}
	if (theSeedType == SeedType::SEED_INSTANT_COFFEE && StageIsNight())
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_AT_NIGHT, true);
	}
	if (theSeedType == SeedType::SEED_GRAVEBUSTER && !StageHasGraveStones())
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_NEEDS_GRAVES, true);
	}
	if (theSeedType == SeedType::SEED_PLANTERN && !StageHasFog())
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_NEEDS_FOG, true);
	}
	if (theSeedType == SeedType::SEED_FLOWERPOT && !StageHasRoof())
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_NEEDS_ROOF, true);
	}
	if (StageHasRoof() && (theSeedType == SeedType::SEED_SPIKEWEED || theSeedType == SeedType::SEED_SPIKEROCK))
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_ON_ROOF, true);
	}
	if (!StageHasPool() && Plant::IsAquatic(theSeedType))
	{
		SetBit(aNotRec, NotRecommend::NOT_RECOMMENDED_NEEDS_POOL, true);
	}
	return aNotRec;
}

//0x41CD70
int Board::CountCoinByType(CoinType theCoinType)
{
	int aCount = 0;

	Coin* aCoin = nullptr;
	while (IterateCoins(aCoin))
	{
		if (aCoin->mType == theCoinType)
		{
			aCount++;
		}
	}

	return aCount;
}

int Board::GetGraveStoneCount()
{
	int aCount = 0;

	GridItem* aGridItem = nullptr;
	while (IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_GRAVESTONE)
		{
			aCount++;
		}
	}

	return aCount;
}

namespace
{
// 战利品档位表（B 形态）：档 i 的阈值为调用点 aLootChance[i]（升序），命中档位 → kLootCoinTier[i]。
// 档 2（钻石档）运行期按包裹升级购买态降级为金币（表外特例，见调用点）。
constexpr CoinType kLootCoinTier[5] = {
	CoinType::COIN_PRESENT_PLANT,  // 0 盆栽植物
	CoinType::COIN_CHOCOLATE,      // 1 巧克力
	CoinType::COIN_DIAMOND,        // 2 钻石（未购包裹升级时降级 COIN_GOLD）
	CoinType::COIN_GOLD,           // 3 金币
	CoinType::COIN_SILVER,         // 4 银币
};
static_assert(sizeof(kLootCoinTier) / sizeof(kLootCoinTier[0]) == 5, "战利品档位表须为 5 档");
}  // namespace

//0x41CDB0
void Board::DropLootPiece(int thePosX, int thePosY, int theDropFactor)
{
	if (mApp->IsFirstTimeAdventureMode())
	{
		if (mLevel == 22 && mCurrentWave > 5 && !mApp->mPlayerInfo->mHasUnlockedMinigames && CountCoinByType(CoinType::COIN_PRESENT_MINIGAMES) == 0)
		{
			mApp->PlayFoley(FoleyType::FOLEY_ART_CHALLENGE);
			AddCoin(thePosX, thePosY, CoinType::COIN_PRESENT_MINIGAMES, CoinMotion::COIN_MOTION_COIN);
			return;
		}
		if (mLevel == 36 && mCurrentWave > 5 && !mApp->mPlayerInfo->mHasUnlockedPuzzleMode && CountCoinByType(CoinType::COIN_PRESENT_PUZZLE_MODE) == 0)
		{
			mApp->PlayFoley(FoleyType::FOLEY_ART_CHALLENGE);
			AddCoin(thePosX, thePosY, CoinType::COIN_PRESENT_PUZZLE_MODE, CoinMotion::COIN_MOTION_COIN);
			return;
		}
	}

	int aDropHit = Rand(30000);
	if (mApp->IsFirstTimeAdventureMode() && mLevel == 11 && !mDroppedFirstCoin && mCurrentWave > 5)
	{
		aDropHit = 1000;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_COLUMN)
	{
		aDropHit *= 5;
	}

	if (mApp->IsWhackAZombieLevel())
	{
		int aSunChanceMin = 2500;
		int aSunChanceMax = mSunMoney > 500 ? 2800 : mSunMoney > 350 ? 3100 : mSunMoney > 200 ? 3700 : 5000;
		if (aDropHit >= aSunChanceMin * theDropFactor && aDropHit <= aSunChanceMax * theDropFactor)
		{
			mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
			AddCoin(thePosX - 20, thePosY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_COIN);
			AddCoin(thePosX - 40, thePosY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_COIN);
			AddCoin(thePosX - 60, thePosY, CoinType::COIN_SUN, CoinMotion::COIN_MOTION_COIN);
			return;
		}
	}

	if (mTotalSpawnedWaves > 70)
		return;

	int aPottedPlantChance;
	if (!mApp->mZenGarden->CanDropPottedPlantLoot())
	{
		aPottedPlantChance = 0;
	}
	else if (mApp->IsAdventureMode() && !mApp->IsFirstTimeAdventureMode())
	{
		aPottedPlantChance = 24;
	}
	else
	{
		aPottedPlantChance = mApp->IsSurvivalEndless(mApp->mGameMode) ? 3 : 12;
	}

	int aChocolateChance = aPottedPlantChance;
	if (mApp->mZenGarden->CanDropChocolate())
	{
		if (mApp->IsAdventureMode() && !mApp->IsFirstTimeAdventureMode())
		{
			aChocolateChance = aPottedPlantChance + 72;
		}
		else
		{
			aChocolateChance = aPottedPlantChance + (mApp->IsSurvivalEndless(mApp->mGameMode) ? 9 : 36);
		}
	}

	int aDiamondChance = aChocolateChance + 14;
	int aGoldChance = aChocolateChance + 250;
	int aSilverChance = aChocolateChance + 2500;

	CoinType aCoinType;
	// B 形态 -> 档位表（上方 kLootCoinTier）：升序阈值顺序判定（等价原 if-else 链），
	// 全档未命中 = 原 else return；钻石档降级特例表外修正
	const int aLootChance[5] = { aPottedPlantChance, aChocolateChance, aDiamondChance, aGoldChance, aSilverChance };
	int aLootTier = 0;
	while (aLootTier < 5 && aDropHit >= aLootChance[aLootTier] * theDropFactor)
	{
		aLootTier++;
	}
	if (aLootTier == 5)
		return;

	aCoinType = kLootCoinTier[aLootTier];
	if (aLootTier == 2 && mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PACKET_UPGRADE] < 1)
	{
		aCoinType = CoinType::COIN_GOLD;
	}

	if (mApp->IsWallnutBowlingLevel() && Coin::IsMoney(aCoinType))
		return;

	if (mApp->IsFirstTimeAdventureMode() && mLevel == 11)
	{
		int aMoney = Coin::GetCoinValue(CoinType::COIN_GOLD) * mLawnMowers.mSize;
		int aCost = StoreScreen::GetItemCost(StoreItem::STORE_ITEM_PACKET_UPGRADE);
		aMoney += mApp->mPlayerInfo->mCoins + CountCoinsBeingCollected();
		if (Coin::GetCoinValue(aCoinType) + aMoney >= aCost)
		{
			return;
		}
	}

	mApp->PlayFoley(FoleyType::FOLEY_SPAWN_SUN);
	AddCoin(thePosX - 40, thePosY, aCoinType, CoinMotion::COIN_MOTION_COIN);
	mDroppedFirstCoin = true;
}

//0x41D2C0
bool Board::CanDropLoot()
{
	return !mCutScene->ShouldRunUpsellBoard() && (!mApp->IsFirstTimeAdventureMode() || mLevel >= 11);
}

//0x41D320
// 格桶第二批审计（2026-10-03）：本函数是僵尸池扫描（IterateZombies 找
// 瞄准该格的蹦极僵尸），过滤"行+列"属实但作用域是僵尸池——植物格桶
// 不适用，保留池扫。其唯一植物侧调用链 PickBungeeZombieTarget 走
// GetTopPlantAt（已桶化）。附近 ~10422/10439 的两处植物池扫
// （KillAllPlantsInRadius 圆形半径 / FindUmbrellaPlant 3×3 矩形）
// 均为跨格矩形形态且不在本批 14 站点名单内，未动。
bool Board::BungeeIsTargetingCell(int theGridX, int theGridY)
{
	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		if (!aZombie->IsDeadOrDying() && aZombie->mZombieType == ZombieType::ZOMBIE_BUNGEE && aZombie->mRow == theGridY && aZombie->mTargetCol == theGridX)
		{
			return true;
		}
	}
	return false;
}

//0x41D390
Zombie* Board::GetBossZombie()
{
	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		if (((ZombieFlagsOf(aZombie->mZombieType) & ZombieFlags::Boss) != 0))
		{
			return aZombie;
		}
	}
	return nullptr;
}

//0x41D3D0
Plant* Board::FindUmbrellaPlant(int theGridX, int theGridY)
{
	Plant* aPlant = nullptr;
	while (IteratePlants(aPlant))
	{
		if (aPlant->mSeedType == SeedType::SEED_UMBRELLA && !aPlant->NotOnGround() && GridInRange(theGridX, theGridY, aPlant->mPlantCol, aPlant->mRow, 1, 1))
		{
			return aPlant;
		}
	}
	return nullptr;
}

//0x41D450
void Board::DoFwoosh(int theRow)
{
	int aRenderOrder = MakeRenderOrder(RenderLayer::RENDER_LAYER_PARTICLE, theRow, 1);
	for (int i = 0; i < 12; i++)
	{
		Reanimation* aOriReanim = mApp->ReanimationTryToGet(mFwooshID[theRow][i]);
		if (aOriReanim)
		{
			aOriReanim->ReanimationDie();
		}

		float aPosX = 750.0f * i / 11.0f + 10.0f;
		float aPosY = GetPosYBasedOnRow(aPosX + 10.0f, theRow) - 10.0f;
		Reanimation* aFwoosh = mApp->AddReanimation(aPosX, aPosY, aRenderOrder, ReanimationType::REANIM_JALAPENO_FIRE, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), AnimationTimeDomain());
		aFwoosh->SetFramesForLayer(ReanimTrackId::anim_flame);
		aFwoosh->ChangeLoopType(ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME);
		aFwoosh->ApplyRate(aFwoosh->mAnimRate * RandRangeFloat(0.7f, 1.3f));

		float aScale = RandRangeFloat(0.9f, 1.1f);
		float aFlip = Rand(2) ? 1.0f : -1.0f;
		aFwoosh->mOverlayMatrix.m00 = aScale * aFlip;
		aFwoosh->mOverlayMatrix.m11 = 1;

		mFwooshID[theRow][i] = mApp->ReanimationGetID(aFwoosh);
	}
	mFwooshCountDown = 100;
}

//0x41D630
void Board::UpdateFwoosh()
{
	if (mFwooshCountDown == 0)
		return;

	--mFwooshCountDown;
	int aFwooshRemaining = TodAnimateCurveNs(500'000'000, 0, mFwooshCountDown.mNanoseconds, 12, 0, Curves::Linear);
	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		for (int i = 0; i < 12 - aFwooshRemaining; i++)
		{
			Reanimation* aFwoosh = mApp->ReanimationTryToGet(mFwooshID[aRow][i]);
			if (aFwoosh)
			{
				aFwoosh->SetFramesForLayer(ReanimTrackId::anim_done);
				aFwoosh->ApplyRate(15.0f);
				aFwoosh->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME);
			}
			mFwooshID[aRow][i] = ReanimationID::REANIMATIONID_NULL;
		}
	}
}

//0x41D730
void Board::UpdateGridItems()
{
	GridItem* aGridItem = nullptr;
	while (IterateGridItems(aGridItem))
	{
		const auto nowNs = static_cast<std::int64_t>(mLevelTimeNs);
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_CRATER)
		{
			if (mApp->mGameScene == GameScenes::SCENE_PLAYING)
			{
				if (nowNs >= aGridItem->mStateEndTimeNs) aGridItem->GridItemDie();
			}
			else if (aGridItem->mStateEndTimeNs != 0)
				aGridItem->mStateEndTimeNs += static_cast<std::int64_t>(Sexy::gUpdateIntervalNs);
		}
		aGridItem->Update();
	}
}

//0x41D7D0（基座检查迁至 Board.h 的 PlantingBasisReqs；PlantingRequirementsMet 分派层删除——
// 静态已知处直呼命名函数，运行期类型经 PlantingBasisReqs::For 绑定一次）

//0x41D8A0
// GOTY @Patoke: 0x420670
int Board::KillAllZombiesInRadius(int theRow, int theX, int theY, int theRadius, int theRowRange, bool theBurn, int theDamageRangeFlags)
{
	Zombie* aZombie = nullptr;
	int aKilledZombies = 0; // @Patoke: implemented this
	while (IterateZombies(aZombie))
	{
		if (aZombie->EffectedByDamage(theDamageRangeFlags))
		{
			Rect aZombieRect = aZombie->GetZombieRect();
			int aRowDist = aZombie->mRow - theRow;
			if (((ZombieFlagsOf(aZombie->mZombieType) & ZombieFlags::Boss) != 0))
			{
				aRowDist = 0;
			}

			if (aRowDist <= theRowRange && aRowDist >= -theRowRange && GetCircleRectOverlap(theX, theY, theRadius, aZombieRect))
			{
				if (theBurn)
				{
					aZombie->ApplyBurn(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
				}
				else
				{
					aZombie->TakeDamage(static_cast<std::int64_t>(mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), 1800, 18U);
				}

				aKilledZombies++;
			}
		}
	}

	int aGridX = PixelToGridXKeepOnBoard(theX, theY);
	int aGridY = PixelToGridYKeepOnBoard(theX, theY);
	GridItem* aGridItem = nullptr;
	while (IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GridItemType::GRIDITEM_LADDER)
		{
			if (GridInRange(aGridItem->mGridX, aGridItem->mGridY, aGridX, aGridY, theRowRange, theRowRange))
			{
				aGridItem->GridItemDie();
			}
		}
	}

	return aKilledZombies;
}

//0x41DA10
int Board::GetNumWavesPerSurvivalStage()
{
	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND || mApp->IsSurvivalNormal(mApp->mGameMode))
	{
		return 10;
	}
	else if (mApp->IsSurvivalHard(mApp->mGameMode) || mApp->IsSurvivalEndless(mApp->mGameMode))
	{
		return 20;
	}

	TOD_ASSERT(false);

	return 0;
}

//0x41DA50
void Board::RemoveParticleByType(ParticleEffect theEffectType)
{
	TodParticleSystem* aParticle = nullptr;
	while (IterateParticles(aParticle))
	{
		if (aParticle->mEffectType == theEffectType)
		{
			aParticle->ParticleSystemDie();
		}
	}
}

//0x41DA90
bool Board::PlantUsesAcceleratedPricing(SeedType theSeedType)
{
	return Plant::IsUpgrade(theSeedType) && mApp->IsSurvivalEndless(mApp->mGameMode);
}

//0x41DAE0
int Board::GetCurrentPlantCost(SeedType theSeedType, SeedType theImitaterType)
{
	int aCost = Plant::GetCost(theSeedType, theImitaterType);
	if (PlantUsesAcceleratedPricing(theSeedType))
	{
		aCost += CountPlantByType(theSeedType) * 50;
	}
	return aCost;
}

//0x41DB40
bool Board::CanUseGameObject(GameObjectType theGameObject)
{
	if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		return (theGameObject == GameObjectType::OBJECT_TYPE_TREE_FOOD) || (theGameObject == GameObjectType::OBJECT_TYPE_NEXT_GARDEN);
	}
	if (mApp->mGameMode != GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		return false;
	}

	if (theGameObject == GameObjectType::OBJECT_TYPE_WATERING_CAN)
	{
		return true;
	}
	if (theGameObject == GameObjectType::OBJECT_TYPE_NEXT_GARDEN)
	{
		return
			mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_MUSHROOM_GARDEN] ||
			mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_AQUARIUM_GARDEN] ||
			mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_TREE_OF_WISDOM];
	}
	if (theGameObject == GameObjectType::OBJECT_TYPE_FERTILIZER)
	{
		return mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_FERTILIZER] > 0;
	}
	if (theGameObject == GameObjectType::OBJECT_TYPE_BUG_SPRAY)
	{
		return mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_BUG_SPRAY] > 0;
	}
	if (theGameObject == GameObjectType::OBJECT_TYPE_PHONOGRAPH)
	{
		return  mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PHONOGRAPH] > 0;
	}
	if (theGameObject == GameObjectType::OBJECT_TYPE_CHOCOLATE)
	{
		return mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_CHOCOLATE] > 0;
	}
	if (theGameObject == GameObjectType::OBJECT_TYPE_WHEELBARROW)
	{
		return mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_WHEEL_BARROW] > 0;
	}
	if (theGameObject == GameObjectType::OBJECT_TYPE_GLOVE)
	{
		return mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_GARDENING_GLOVE] > 0;
	}
	if (theGameObject == GameObjectType::OBJECT_TYPE_MONEY_SIGN)
	{
		return mApp->HasFinishedAdventure();
	}
	if (theGameObject == GameObjectType::OBJECT_TYPE_TREE_FOOD)
	{
		return false;
	}

	TOD_ASSERT(false);

	return false;
}


LawnMower* Board::FindLawnMowerInRow(int theRow)
{
	LawnMower* aLawnMower = nullptr;
	while (IterateLawnMowers(aLawnMower))
	{
		if (aLawnMower->mRow == theRow)
		{
			return aLawnMower;
		}
	}
	return nullptr;
}

Zombie* Board::GetWinningZombie()
{
	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		if (aZombie->mFromWave == Zombie::ZOMBIE_WAVE_WINNER)
		{
			return aZombie;
		}
	}
	return nullptr;
}

int Board::CountZombieByType(ZombieType theZombieType)
{
	int aCount = 0;

	Zombie* aZombie = nullptr;
	while (IterateZombies(aZombie))
	{
		if (aZombie->mZombieType == theZombieType)
		{
			aCount++;
		}
	}

	return aCount;
}

int Board::NumberZombiesInWave(int theWaveIndex)
{
	TOD_ASSERT(theWaveIndex >= 0 && theWaveIndex < MAX_ZOMBIE_WAVES && theWaveIndex < mNumWaves);

	for (int i = 0; i < MAX_ZOMBIES_IN_WAVE; i++)
	{
		if (mZombiesInWave[theWaveIndex][i] == ZombieType::ZOMBIE_INVALID)
		{
			return i;
		}
	}

	TOD_ASSERT(false);
	return 0;
}

bool Board::IsZombieTypeSpawnedOnly(ZombieType theZombieType)
{
	return (theZombieType == ZombieType::ZOMBIE_BACKUP_DANCER || theZombieType == ZombieType::ZOMBIE_BOBSLED || theZombieType == ZombieType::ZOMBIE_IMP);
}

bool Board::CheckForPostGameAchievements()
{
	bool aUnlockedNewChallenge = false;

	// @Patoke: implemented
	if (!mApp->IsAdventureMode() && !mApp->IsSurvivalMode() && !mApp->IsPuzzleMode())
	{
		// 2026-10-08 GameMode v3 改名聚簇：GAMEMODE_MINIGAME_* 区间即槽位
		//（用户裁决：关卡层面 limbo 同属挑战大类、存档层面不进小游戏成就
		// 账本——两簇分立、区间即语义）。转换器按 kGameModeRemapV2ToV3 的
		// 槽序换算旧档 mMiniGamesCompleted。
		if ((unsigned)(mApp->mGameMode - GameMode::GAMEMODE_MINIGAME_BEGHOULED) <=
			(unsigned)(GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM - GameMode::GAMEMODE_MINIGAME_BEGHOULED))
		{
			mApp->mPlayerInfo->mMiniGamesCompleted[
				(int)mApp->mGameMode - (int)GameMode::GAMEMODE_MINIGAME_BEGHOULED] = true;
		}

		bool aHasCompletedAllMinigames = true;
		for (int i = 0; i < 20; i++)
		{
			if (!mApp->mPlayerInfo->mMiniGamesCompleted[i])
			{
				aHasCompletedAllMinigames = false;
				break;
			}
		}

		if (aHasCompletedAllMinigames)
		{
			if (ReportAchievement::GiveAchievement(mApp, BeyondTheGrave, false))
			{
				aUnlockedNewChallenge = true;
			}
		}
	}

	if (mApp->IsWhackAZombieLevel() || mApp->IsScaryPotterLevel() || mApp->IsWallnutBowlingLevel())
	{
		return aUnlockedNewChallenge;
	}

	if (!mApp->IsAdventureMode() && !mApp->IsSurvivalMode() && !mApp->IsPuzzleMode())
	{
		return aUnlockedNewChallenge;
	}
	else if (mApp->IsAdventureMode() && mLevel == FINAL_LEVEL)
	{
		if (ReportAchievement::GiveAchievement(mApp, HomeSecurity, false))
		{
			aUnlockedNewChallenge = true;
		}
	}

	if (StageIsDayWithPool() && !mPeaShooterUsed)
	{
		if (ReportAchievement::GiveAchievement(mApp, DontPea, false))
		{
			aUnlockedNewChallenge = true;
		}
	}
	if (StageHasRoof() && !HasConveyorBeltSeedBank() && !mCatapultPlantsUsed)
	{
		if (ReportAchievement::GiveAchievement(mApp, Grounded, false))
		{
			aUnlockedNewChallenge = true;
		}
	}
	if (StageIsNight())
	{
		if (!mMushroomsUsed && ReportAchievement::GiveAchievement(mApp, NoFungusAmongUs, false))
		{
			aUnlockedNewChallenge = true;
		}
	}
	// 1073 0041E64C..0041E6B0: the opposite branch of the night-map test,
	// not just BACKGROUND_1_DAY. Pool/roof daytime maps are eligible too.
	else if (mMushroomAndCoffeeBeansOnly)
	{
		if (ReportAchievement::GiveAchievement(mApp, GoodMorning, false))
		{
			aUnlockedNewChallenge = true;
		}
	}

	return aUnlockedNewChallenge;
}
