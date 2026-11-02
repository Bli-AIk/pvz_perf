#include "EffectSystem.h"
#include "Sexy.TodLib/generated/EntityStrings.h"
#include "Board.h"
#include "Plant.h"
#include "Zombie.h"
#include "GridItem.h"
#include "Cutscene.h"
#include "Challenge.h"
#include "LawnMower.h"
#include "SeedPacket.h"
#include "LawnApp.h"
#include "System/Music.h"
#include "Resources.h"
#include "MessageWidget.h"
#include "GameConstants.h"
#include "Widget/LawnDialog.h"
#include "Widget/ZombiesWonLayout.h"
#include "Widget/GameButton.h"
#include "System/PlayerInfo.h"
#include "Widget/StoreScreen.h"
#include "Widget/ChallengeScreen.h"
#include "Sexy.TodLib/TodFoley.h"
#include "Widget/SeedChooserScreen.h"
#include "Sexy.TodLib/TodCommon.h"
#include "Sexy.TodLib/Attachment.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/TodParticle.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "widget/WidgetManager.h"
#include "widget/WidgetLifetimeHold.h"
#include "misc/FrameTiming.h"
#include <algorithm>

namespace
{
// The canonical backdrop spans [-220, 1180]. Extended art retains this core;
// do not count its side padding again when choosing camera endpoints.
constexpr int ConstrainSeedChooserCameraX(int theX, int theViewportLeft, int theViewportWidth)
{
	return std::clamp(theX, theViewportLeft + theViewportWidth - BOARD_IMAGE_WIDTH_OFFSET,
		theViewportLeft + BOARD_OFFSET);
}

static_assert(ConstrainSeedChooserCameraX(-380, -133, 1066) == -247);
static_assert(ConstrainSeedChooserCameraX(220, -133, 1066) == 87);
static_assert(ConstrainSeedChooserCameraX(-380, 0, 801) == -379);

// CutScene is owned by Board, not itself a Widget. Retain the Board across
// the nested wait, but reject results after DisposeBoard changes the session.
class DaveUpgradeSession
{
    WidgetLifetimeHold mBoardLifetime;
    CutScene& mCutScene;
    LawnApp& mApp;
    Board* mBoard;
    std::uint64_t mRevision;
    PlayerInfo* mPlayer;
    unsigned int mPlayerId;
    unsigned int mPlayerUseSeq;
    int mMessage;
public:
    explicit DaveUpgradeSession(CutScene& cutScene)
        : mBoardLifetime(*cutScene.mBoard), mCutScene(cutScene), mApp(*cutScene.mApp),
          mBoard(cutScene.mBoard), mRevision(mApp.mBoardRevision), mPlayer(mApp.mPlayerInfo),
          mPlayerId(mPlayer ? mPlayer->mId : 0), mPlayerUseSeq(mPlayer ? mPlayer->mUseSeq : 0),
          mMessage(mApp.mCrazyDaveMessageIndex) {}
    bool IsCurrent() const
    {
        return !mApp.mShutdown && !mApp.mCloseRequest && mApp.mBoard == mBoard &&
            mApp.mBoardRevision == mRevision && mBoard->mCutScene == &mCutScene &&
            mApp.mPlayerInfo == mPlayer && mPlayer && mPlayer->mId == mPlayerId &&
            mPlayer->mUseSeq == mPlayerUseSeq && mApp.mCrazyDaveMessageIndex == mMessage;
    }
};

struct DaveUpgradeDialog
{
    LawnApp& app;
    Dialog* dialog;
    WidgetLifetimeHold lifetime;
    DaveUpgradeDialog(LawnApp& owner, Dialog* child) : app(owner), dialog(child), lifetime(*child) {}
    ~DaveUpgradeDialog() { app.SexyAppBase::KillDialog(dialog); }
};
}

static const int	TimePanRightStart				= 1500;
static const int	TimePanRightEnd					= 3500;		//[0x6A4FFC]
static const int	TimeEarlyDaveEnterStart			= 2000;		//[0x6A5000]
static const int	TimeEarlyDaveEnterEnd			= 2750;		//[0x6AA220]
//static const int	TimeEarlyDaveLeaveStart			= 3250;		//[0x6AA224]
static const int	TimeEarlyDaveLeaveEnd			= 4000;		//[0x6AA228]
static const int	TimeSeedChoserSlideOnStart		= 4000;		//[0x6AA22C]
static const int	TimeSeedChoserSlideOnEnd		= 4250;		//[0x6AA230]
static const int	TimeSeedChoserSlideOffStart		= 4500;		//[0x6AA234]
static const int	TimeSeedChoserSlideOffEnd		= 4750;		//[0x6AA238]
static const int	TimeSeedBankOnStart				= 4000;		//[0x6AA23C]
static const int	TimeSeedBankOnEnd				= 4250;		//[0x6AA240]
static const int	TimePanLeftStart				= 4500;		//[0x6AA244]
static const int	TimePanLeftEnd					= 6000;		//[0x6AA248]
static const int	TimeSeedBankRightStart			= 4750;		//[0x6AA24C]
static const int	TimeSeedBankRightEnd			= 6000;		//[0x6AA250]
static const int	TimeRollSodStart				= 6000;		//[0x6AA254]
static const int	TimeRollSodEnd					= 8000;		//[0x6AA258]
static const int	TimeReadySetPlantStart			= 6000;		//[0x6AA264]
static const int	TimeReadySetPlantEnd			= 7830;		//[0x6AA268]
static const int	TimeFogRollIn					= 5950;		//[0x6AA26C]
//static const int	TimeCrazyDaveEnterStart			= 6500;		//[0x6AA270]
//static const int	TimeCrazyDaveEnterEnd			= 7250;		//[0x6AA274]
//static const int	TimeCrazyDaveLeaveStart			= 7750;		//[0x6AA278]
//static const int	TimeCrazyDaveLeaveEnd			= 8500;		//[0x6AA27C]
static const int	TimeIntroEnd					= 6000;		//[0x6AA280]
static const int	LostTimePanRightStart			= 1500;
static const int	LostTimePanRightEnd				= 3500;		//[0x6A5004]
static const int	LostTimeBrainGraphicStart		= 6000;		//[0x6AA284]
static const int	LostTimeBrainGraphicShake		= 7000;		//[0x6AA288]
static const int	LostTimeBrainGraphicCancelShake	= 8000;		//[0x6AA28C]
static const int	LostTimeBrainGraphicEnd			= 11000;	//[0x6AA290]
static const int	LostTimeEnd						= 11000;	//[0x6AA294]
static const int	TimeIntro_PresentsFadeIn		= 1000;
static const int	TimeIntro_LogoStart				= 5500;		//[0x6A5008]
static const int	TimeIntro_LogoEnd				= 5900;		//[0x6AA298]
static const int	TimeIntro_PanRightStart			= 5890;		//[0x6A500C]
static const int	TimeIntro_PanRightEnd			= 11890;	//[0x6AA29C]
static const int	TimeIntro_FadeOut				= 10890;	//[0x6AA2A0]
static const int	TimeIntro_FadeOutEnd			= 11890;	//[0x6AA2A4]
static const int	TimeIntro_End					= 13890;	//[0x6AA2A8]
static const int	TimeLawnMowerDuration			= 250;
static const int	TimeLawnMowerStart[6]			= { 6300, 6250, 6200, 6150, 6100, 6050 };	//[0x6AA2AC]

//0x4390E0
CutScene::CutScene()
{
	mApp = (LawnApp*)gSexyAppBase;
	mBoard = mApp->mBoard;
	mCutsceneTime = 0;
	mSodTime = 0;
	mFogTime = 0;
	mBossTime = 0;
	mCrazyDaveTime = 0;
	mReadySetPlantTime = 0;
	mLawnMowerTime = 0;
	mCrazyDaveDialogStart = -1;
	mSeedChoosing = false;
	mZombiesWonReanimID = REANIMATIONID_NULL;
	mPreloaded = false;
	mPlacedZombies = false;
	mPlacedLawnItems = false;
	mCrazyDaveCountDown = 0;
	mCrazyDaveLastTalkIndex = -1;
	mUpsellHideBoard = false;
	mUpsellChallengeScreen = nullptr;
	mPreUpdatingBoard = false;
}

CutScene::~CutScene()
{
	if (mApp && mApp->mEffectSystem)
		for (const auto id : mVisualParticleIDs)
			if (auto* particle=mApp->ParticleTryToGet(id)) particle->ParticleSystemDie();
	if (mUpsellChallengeScreen)
	{
		delete mUpsellChallengeScreen;
	}
	mApp->mMuteSoundsForCutscene = false;
}

//0x439140
void CutScene::PlaceAZombie(ZombieType theZombieType, int theGridX, int theGridY)
{
	bool aPutOnDuckyTube = false;
	if (theZombieType == ZombieType::ZOMBIE_DUCKY_TUBE && mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_WAR_AND_PEAS_2)
	{
		theZombieType = ZombieType::ZOMBIE_PEA_HEAD;
		aPutOnDuckyTube = true;
	}

	// 1073 0043C389: previews use logical row 0; theGridY only places the sprite.
	Zombie* aZombie = mBoard->AddZombieInRow(theZombieType, 0, -2);
	TOD_ASSERT(aZombie);
	{ aZombie->mX = (theGridX * 56 + 830); aZombie->mRootBindingGeneration = 0; }
	aZombie->mY = (theGridY * 90 + 70);
	if (theGridX % 2 == 1)
	{
		aZombie->mY += (30.0f);
	}

	if (aPutOnDuckyTube)
	{
		//aZombie->ReanimShowTrack("Zombie_duckytube", 0);
		mApp->ReanimationGet(aZombie->mBodyReanimID)->AssignRenderGroupToTrack(ReanimTrackId::Zombie_duckytube, RENDER_GROUP_NORMAL);
	}
	if (mBoard->StageHasRoof())
	{
		aZombie->mY -= (theGridY * 2 - theGridX * 7 + 30);  //7 * (5 - theGridX) - 2 * (5 - theGridY) + 5;
		{ aZombie->mX -= (5.0f); aZombie->mRootBindingGeneration = 0; }
	}
	if (theZombieType == ZombieType::ZOMBIE_ZAMBONI)
	{
		aZombie->mY -= (10.0f);
		{ aZombie->mX -= (30.0f); aZombie->mRootBindingGeneration = 0; }
	}
	else if (mApp->IsLittleTroubleLevel())
	{
		aZombie->mY += (Rand(50) - 25);  //RandRangeInt(-25, 24);
		{ aZombie->mX += (Rand(50) - 25); aZombie->mRootBindingGeneration = 0; }  //RandRangeInt(-25, 24);
	}
	else if (Is2x2Zombie(theZombieType))
	{
		{ aZombie->mX += (Rand(15) - 20); aZombie->mRootBindingGeneration = 0; }  //RandRangeInt(-20, -6);
	}
	else if (theGridY == 4 && (mApp->CanShowAlmanac() || mApp->CanShowStore()))
	{
		{ aZombie->mX += (Rand(15)); aZombie->mRootBindingGeneration = 0; }  //RandRangeInt(0, 14);
	}
	else
	{
		aZombie->mY += (Rand(15));  //RandRangeInt(0, 14);
		{ aZombie->mX += (Rand(15)); aZombie->mRootBindingGeneration = 0; }  //RandRangeInt(0, 14);
	}
	aZombie->mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_LAWN, 0, (theGridX % 2) * 2 + theGridY * 4);

	if (theZombieType == ZombieType::ZOMBIE_BUNGEE)
	{
		aZombie->mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_GROUND, 0, 0);
		aZombie->mRow = 0;
		{ aZombie->mX = (theGridX * 50.0f + 950.0f); aZombie->mRootBindingGeneration = 0; }
		aZombie->mY = (50.0f);
	}
	else if (theZombieType == ZombieType::ZOMBIE_BOBSLED)
	{
		aZombie->mRenderOrder = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_LAWN, 0, 1000);
		aZombie->mRow = 0;
		{ aZombie->mX = (1105.0f); aZombie->mRootBindingGeneration = 0; }
		aZombie->mY = (480.0f);
	}
}

//0x4393D0
bool CutScene::CanZombieGoInGridSpot(ZombieType theZombieType, int theGridX, int theGridY, bool theZombieGrid[5][5])
{
	if (theZombieGrid[theGridX][theGridY])
	{
		return false;
	}

	if (Is2x2Zombie(theZombieType))
	{
		if (theGridX == 0 || theGridY == 0)
		{
			return false;
		}

		if (theZombieGrid[theGridX - 1][theGridY] || theZombieGrid[theGridX][theGridY - 1] || theZombieGrid[theGridX - 1][theGridY - 1])
		{
			return false;
		}
	}

	if (theGridX == 4)
	{
		if (theGridY == 0)
		{
			return false;
		}
	}
	else
	{
		if (theZombieType == ZombieType::ZOMBIE_ZAMBONI)
		{
			return false;
		}

		if (theGridX == 0 && mBoard->StageHasPool())
		{
			return false;
		}
	}

	if (mBoard->StageHasRoof() && theGridX == 0 && theGridY == 0)
	{
		return false;
	}
	if (theGridX == 4 && mBoard->StageHasFog() && theZombieType == ZombieType::ZOMBIE_BALLOON)
	{
		return false;
	}

	if (Is2x2Zombie(theZombieType) ||
		theZombieType == ZombieType::ZOMBIE_ZAMBONI ||
		theZombieType == ZombieType::ZOMBIE_BOBSLED ||
		theZombieType == ZombieType::ZOMBIE_POLEVAULTER)
	{
		if (theGridX == 0)
		{
			return false;
		}
		if (theGridX == 1 && (mBoard->StageHasPool() || theGridY == 0))
		{
			return false;
		}
	}

	return true;
}

//0x4394A0
void CutScene::FindPlaceForStreetZombies(ZombieType theZombieType, bool theZombieGrid[5][5], int& thePosX, int& thePosY)
{
	if (theZombieType == ZOMBIE_BUNGEE)
	{
		thePosX = 0;
		thePosY = 0;
		return;
	}

	int aPicksCount = 0;
	TodWeightedGridArray aPicks[25];
	for (int aGridX = 0; aGridX < 5; aGridX++)
	{
		for (int aGridY = 0; aGridY < 5; aGridY++)
		{
			if (CanZombieGoInGridSpot(theZombieType, aGridX, aGridY, theZombieGrid))
			{
				aPicks[aPicksCount].mX = aGridX;
				aPicks[aPicksCount].mY = aGridY;
				aPicks[aPicksCount].mWeight = 1;
				aPicksCount++;
			}
		}
	}

	if (aPicksCount == 0)
	{
		TodTrace("No place for street zombie!!");
		thePosX = 2;
		thePosY = 2;
	}
	else
	{
		TodWeightedGridArray* aGrid = TodPickFromWeightedGridArray(aPicks, aPicksCount);
		thePosX = aGrid->mX;
		thePosY = aGrid->mY;
	}
}

//0x439590
void CutScene::FindAndPlaceZombie(ZombieType theZombieType, bool theZombieGrid[5][5])
{
	int aGridX, aGridY;
	FindPlaceForStreetZombies(theZombieType, theZombieGrid, aGridX, aGridY);

	if (theZombieType != ZombieType::ZOMBIE_BUNGEE)
	{
		theZombieGrid[aGridX][aGridY] = true;
	}
	if (Is2x2Zombie(theZombieType))
	{
		TOD_ASSERT(aGridX > 0 && aGridY > 0);
		theZombieGrid[aGridX - 1][aGridY] = true;
		theZombieGrid[aGridX][aGridY - 1] = true;
		theZombieGrid[aGridX - 1][aGridY - 1] = true;
	}

	PlaceAZombie(theZombieType, aGridX, aGridY);
	if (theZombieType == ZombieType::ZOMBIE_BUNGEE && mApp->IsBungeeBlitzLevel())
	{
		PlaceAZombie(ZombieType::ZOMBIE_BUNGEE, 1, aGridY);
		PlaceAZombie(ZombieType::ZOMBIE_BUNGEE, 2, aGridY);
	}
}

bool CutScene::Is2x2Zombie(ZombieType theZombieType)
{
	return theZombieType == ZombieType::ZOMBIE_GARGANTUAR || theZombieType == ZombieType::ZOMBIE_REDEYE_GARGANTUAR;
}

//0x439640
// GOTY @Patoke: 0x43C840
void CutScene::PreloadResources()
{

	if (mPreloaded)
	{
		return;
	}
	mPreloaded = true;

	for (int aWave = 0; aWave < mBoard->mNumWaves; aWave++)
	{
		for (int aZombieIndex = 0; aZombieIndex < MAX_ZOMBIES_IN_WAVE; aZombieIndex++)
		{
			ZombieType aZombieType = mBoard->mZombiesInWave[aWave][aZombieIndex];
			if (aZombieType == ZombieType::ZOMBIE_INVALID)
			{
				break;
			}
			Zombie::PreloadZombieResources(aZombieType);
		}
	}

	for (SeedType aSeedType = SeedType::SEED_PEASHOOTER; aSeedType < SeedType::NUM_SEED_TYPES; aSeedType = (SeedType)((int)aSeedType + 1))
	{
		if (mApp->SeedTypeAvailable(aSeedType))
		{
			Plant::PreloadPlantResources(aSeedType);
		}
	}

	if (mApp->IsFirstTimeAdventureMode() && mBoard->mLevel <= 50)
	{
		Plant::PreloadPlantResources(mApp->GetAwardSeedForLevel(mBoard->mLevel));
	}

	if (mCrazyDaveDialogStart != -1)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_CRAZY_DAVE, true);
	}
	if (mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_RAKE])
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_RAKE, true);
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		Plant::PreloadPlantResources(SeedType::SEED_SPROUT);
		Plant::PreloadPlantResources(SeedType::SEED_MARIGOLD);
	}

	if (mBoard->StageHasRoof())
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ROOF_CLEANER, true);
	}
	else
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_LAWNMOWER, true);
	}
	if (mBoard->StageHasPool())
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SPLASH, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_POOL_CLEANER, true);
	}

	if (mBoard->CanDropLoot())
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_COIN_SILVER, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_COIN_GOLD, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_DIAMOND, true);
	}

	if (mSodTime > 0)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_SODROLL, true);
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_PORTAL_COMBAT)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_PORTAL_CIRCLE, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_PORTAL_SQUARE, true);
	}
	if (mApp->IsWhackAZombieLevel() || mApp->IsScaryPotterLevel())
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_HAMMER, true);
	}
	if (mApp->IsStormyNightLevel() || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_RAINING_SEEDS)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_RAIN_CIRCLE, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_RAIN_SPLASH, true);
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZENGARDEN_WATERINGCAN, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZENGARDEN_FERTILIZER, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZENGARDEN_BUGSPRAY, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZENGARDEN_PHONOGRAPH, true);
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_STINKY, true);
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZENGARDEN_FERTILIZER, true);
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_INTRO)
	{
		TodLoadResources(ResourceGroupId::DelayLoad_Background3);
		TodLoadResources(ResourceGroupId::DelayLoad_Credits);
		Zombie::PreloadZombieResources(ZombieType::ZOMBIE_NORMAL);
		Zombie::PreloadZombieResources(ZombieType::ZOMBIE_TRAFFIC_CONE);
		Zombie::PreloadZombieResources(ZombieType::ZOMBIE_PAIL);
		Zombie::PreloadZombieResources(ZombieType::ZOMBIE_ZAMBONI);
		Plant::PreloadPlantResources(SeedType::SEED_SUNFLOWER);
		Plant::PreloadPlantResources(SeedType::SEED_PEASHOOTER);
		Plant::PreloadPlantResources(SeedType::SEED_SQUASH);
		Plant::PreloadPlantResources(SeedType::SEED_THREEPEATER);
		Plant::PreloadPlantResources(SeedType::SEED_LILYPAD);
		Plant::PreloadPlantResources(SeedType::SEED_TORCHWOOD);
		Plant::PreloadPlantResources(SeedType::SEED_SPIKEWEED);
		Plant::PreloadPlantResources(SeedType::SEED_TANGLEKELP);
	}

	PlaceStreetZombies();

	// 关卡放行门（2026-10-09 用户规格"加载界面卡住不放行，首绘路径不得
	// 存在"）：过场预载批次（僵尸定义/植物资源/棋盘阴影/选卡图/字体）刚
	// 入队完毕——立即排干+清扫到位。过场为此可以卡住；不留任何图给
	// intro 播放期的首绘在绘制路径上传（2ms/帧的泵追不上首绘的教训）。
	TodWarmDrainAll();
}

//0x43A140
void CutScene::PlaceStreetZombies()
{
	if (mPlacedZombies)
		return;

	mPlacedZombies = true;
	if (mApp->IsFinalBossLevel())
		return;

	// 以下统计出怪列表中各种可预览的僵尸的数量
	// int aZombieValueTotal = 0;
	int aTotalZombieCount = 0;
	int aZombieTypeCount[(int)ZombieType::NUM_ZOMBIE_TYPES] = { 0 };
	TOD_ASSERT(mBoard->mNumWaves <= MAX_ZOMBIE_WAVES);

	for (int aWave = 0; aWave < mBoard->mNumWaves; aWave++)
	{
		for (int aZombieIndex = 0; aZombieIndex < MAX_ZOMBIES_IN_WAVE; aZombieIndex++)
		{
			ZombieType aZombieType = mBoard->mZombiesInWave[aWave][aZombieIndex];
			if (aZombieType == ZombieType::ZOMBIE_INVALID)
			{
				break;
			}

			// aZombieValueTotal += GetZombieDefinition(aZombieType).mZombieValue;
			// (void)aZombieValueTotal; // Unused

			if (aZombieType == ZombieType::ZOMBIE_FLAG)
			{
				continue;
			}
			if (aZombieType == ZombieType::ZOMBIE_YETI && !mApp->IsStormyNightLevel())
			{
				continue;
			}
			if (aZombieType == ZombieType::ZOMBIE_BOBSLED && mApp->mGameMode != GameMode::GAMEMODE_MINIGAME_BOBSLED_BONANZA)
			{
				continue;
			}

			TOD_ASSERT(aZombieType >= 0 && aZombieType < ZombieType::NUM_ZOMBIE_TYPES);

			++aZombieTypeCount[aZombieType];
			++aTotalZombieCount;
			if (aZombieType == ZombieType::ZOMBIE_BUNGEE || aZombieType == ZombieType::ZOMBIE_BOBSLED)
			{
				aZombieTypeCount[aZombieType] = 1;  // 蹦极僵尸和雪橇僵尸至多仅允许有 1 只预览僵尸
			}
		}
	}

	// 谁笑到最后关卡，除雪人僵尸外，所有允许出怪的僵尸类型至少计入 1 只僵尸
	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND)
	{
		for (int aZombieType = 0; aZombieType < (int)ZombieType::NUM_ZOMBIE_TYPES; aZombieType++)
		{
			if (aZombieType != ZombieType::ZOMBIE_YETI && mBoard->mZombieAllowed[aZombieType])
			{
				aZombieTypeCount[aZombieType] = std::max(aZombieTypeCount[aZombieType], 1);
			}
		}
	}
	if (mBoard->StageHasPool())
	{
		aZombieTypeCount[(int)ZombieType::ZOMBIE_DUCKY_TUBE] = 1;  // 泳池关卡，必定出现鸭子僵尸预览
	}

	bool aZombieGrid[5][5] = {{ false }};
	int aPreviewCapacity = 10;
	if (mApp->IsLittleTroubleLevel())
	{
		aPreviewCapacity = 15;
	}
	else if ((mApp->IsStormyNightLevel() && mApp->IsAdventureMode()) || mApp->IsMiniBossLevel())
	{
		aPreviewCapacity = 18;
	}

	// 优先放置较大体型的僵尸，然后再放置较小体型的僵尸
	for (ZombieType aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType = (ZombieType)((int)aZombieType + 1))
	{
		if (aZombieTypeCount[(int)aZombieType] && (Is2x2Zombie(aZombieType) || aZombieType == ZombieType::ZOMBIE_ZAMBONI))
		{
			FindAndPlaceZombie(aZombieType, aZombieGrid);
		}
	}
	for (ZombieType aZombieType = ZombieType::ZOMBIE_NORMAL; aZombieType < ZombieType::NUM_ZOMBIE_TYPES; aZombieType = (ZombieType)((int)aZombieType + 1))
	{
		if (aZombieTypeCount[(int)aZombieType] && !Is2x2Zombie(aZombieType) && aZombieType != ZombieType::ZOMBIE_ZAMBONI)
		{
			int aZombieNumInWave = aZombieTypeCount[(int)aZombieType];
			int aZombiePreviewNum = aZombieNumInWave * aPreviewCapacity / aTotalZombieCount;
			aZombiePreviewNum = ClampInt(aZombiePreviewNum, 1, aZombieNumInWave);
			for (int i = 0; i < aZombiePreviewNum; i++)
			{
				FindAndPlaceZombie(aZombieType, aZombieGrid);
			}
		}
	}
}

//0x43A690
void CutScene::PlaceLawnItems()
{
	if (mPlacedLawnItems)
	{
		return;
	}
	mPlacedLawnItems = true;

	if (!IsSurvivalRepick())
	{
		mBoard->InitLawnMowers();
		AddFlowerPots();
	}

	if (!IsSurvivalRepick())
	{
		mBoard->PlaceRake();
	}
}

//0x43A710
bool CutScene::IsSurvivalRepick()
{
	return (mApp->IsSurvivalMode() && mBoard->mChallenge->mSurvivalStage > 0 && mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO);
}

//0x43A740
bool CutScene::IsNonScrollingCutscene()
{
	return
		mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ICE ||
		mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM ||
		mApp->IsScaryPotterLevel() ||
		mApp->IsIZombieLevel() ||
		mApp->IsWhackAZombieLevel() ||
		mApp->IsShovelLevel() ||
		mApp->IsSquirrelLevel() ||
		mApp->IsWallnutBowlingLevel();
}

//0x43A7B0
bool CutScene::IsScrolledLeftAtStart()
{
	if (mBoard->mChallenge->mSurvivalStage > 0 && mApp->IsSurvivalMode())
		return false;  // 非首轮的生存模式的过场，屏幕滚动从屏幕中央开始

	return !IsNonScrollingCutscene();
}

//0x43A820
bool CutScene::CanGetPacketUpgrade()
{
	int aCost = StoreScreen::GetItemCost(StoreItem::STORE_ITEM_PACKET_UPGRADE);

	return
		mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PACKET_UPGRADE] == 0 &&
		mApp->mPlayerInfo->mCoins >= aCost &&
		mApp->mPlayerInfo->mDidntPurchasePacketUpgrade < 2;
}

//0x43A890
bool CutScene::CanGetSecondPacketUpgrade()
{
	int aCost = StoreScreen::GetItemCost(StoreItem::STORE_ITEM_PACKET_UPGRADE);

	return
		mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PACKET_UPGRADE] == 1 &&
		mApp->mPlayerInfo->mCoins >= aCost &&
		mApp->mPlayerInfo->mDidntPurchasePacketUpgrade < 2;
}

bool CutScene::CanGetPacketUpgrade(int theUpgradeIndex)
{
	int aCost = StoreScreen::GetItemCost(StoreItem::STORE_ITEM_PACKET_UPGRADE);

	return
		mApp->mPlayerInfo->mPurchases[StoreItem::STORE_ITEM_PACKET_UPGRADE] == theUpgradeIndex &&  // theUpgradeIndex 从首次为 0 开始计算
		mApp->mPlayerInfo->mCoins >= aCost &&
		mApp->mPlayerInfo->mDidntPurchasePacketUpgrade < 2;
}

namespace
{
// CutScene::StartLevelIntro 疯狂戴夫开场白分发表：表内匿名 lambda = 原链支体逐句迁移
// （各支 IsFirstTimeAdventureMode / 升级资格 / isRestart 复合守卫随支体入槽）；
// nullptr = 原链落空（mCrazyDaveDialogStart 保持进函数前值）。
// 键序警示：本表按键值（关卡号）排列，行序 ≠ 原链书写序；原链先后遮蔽关系已并入槽值
// （40 槽为风暴之夜 1101 而非关卡 40 无对白；13/14 槽无二段升级回退；20 槽落空）。
using CrazyDaveDialogFn = void (*)(CutScene&, bool /* aIsRestart */);

// 共体槽 [13][14]：原链 FTAM && aLevel∈13..24(≠15,20,21) && CanGetPacketUpgrade() 支
void CrazyDaveDialogPacketUpgrade(CutScene& aCutScene, bool)
{
	if (aCutScene.mApp->IsFirstTimeAdventureMode() && aCutScene.CanGetPacketUpgrade())
	{
		aCutScene.mCrazyDaveDialogStart = 1501;
	}
}

// 共体槽 [16..19][22..24]：原链 CanGetPacketUpgrade 支在前、CanGetSecondPacketUpgrade 支在后的叠加序
void CrazyDaveDialogPacketOrSecond(CutScene& aCutScene, bool)
{
	if (aCutScene.mApp->IsFirstTimeAdventureMode())
	{
		if (aCutScene.CanGetPacketUpgrade())
		{
			aCutScene.mCrazyDaveDialogStart = 1501;
		}
		else if (aCutScene.CanGetSecondPacketUpgrade())
		{
			aCutScene.mCrazyDaveDialogStart = 1551;
		}
	}
}

// 冒险关卡→开场白动作表（C 形态）：mLevel-1 直下标（1..50）；仅冒险分支查表。
constexpr CrazyDaveDialogFn kCrazyDaveDialogPerLevel[FINAL_LEVEL] = {
	/* [1]  重玩 1 关（原 !FTAM && aLevel==1 支）                             */
	[](CutScene& aCutScene, bool)
	{
		if (!aCutScene.mApp->IsFirstTimeAdventureMode())
		{
			aCutScene.mCrazyDaveDialogStart = 1601;
		}
	},
	/* [2..4]  无开场白（原链落空）                                           */ nullptr, nullptr, nullptr,
	/* [5]  保龄球关（原 IsWallnutBowlingLevel && 冒险支；带标线/铲子副作用）    */
	[](CutScene& aCutScene, bool)
	{
		if (aCutScene.mApp->IsFirstTimeAdventureMode())
		{
			aCutScene.mCrazyDaveDialogStart = 2400;
		}
		else
		{
			aCutScene.mCrazyDaveDialogStart = 2411;
			aCutScene.mBoard->mChallenge->mShowBowlingLine = true;
		}
		aCutScene.mBoard->mShowShovel = true;
	},
	/* [6..10] 无开场白（原链落空）                                           */ nullptr, nullptr, nullptr, nullptr, nullptr,
	/* [11] 首通 11 关                                                        */
	[](CutScene& aCutScene, bool)
	{
		if (aCutScene.mApp->IsFirstTimeAdventureMode())
		{
			aCutScene.mCrazyDaveDialogStart = 201;
		}
	},
	/* [12] 首通 12 关                                                        */
	[](CutScene& aCutScene, bool)
	{
		if (aCutScene.mApp->IsFirstTimeAdventureMode())
		{
			aCutScene.mCrazyDaveDialogStart = 1401;
		}
	},
	/* [13] 首通 + 可购首套卡槽升级                                           */ CrazyDaveDialogPacketUpgrade,
	/* [14] 首通 + 可购首套卡槽升级                                           */ CrazyDaveDialogPacketUpgrade,
	/* [15] 打地鼠关（原 IsWhackAZombieLevel && 冒险支，无 FTAM 条件）          */
	[](CutScene& aCutScene, bool)
	{
		aCutScene.mCrazyDaveDialogStart = 401;
	},
	/* [16..19] 首通 + 两段卡槽升级叠加序                                     */ CrazyDaveDialogPacketOrSecond, CrazyDaveDialogPacketOrSecond, CrazyDaveDialogPacketOrSecond, CrazyDaveDialogPacketOrSecond,
	/* [20] 无开场白（原链 3/4 支均显式排除 20）                              */ nullptr,
	/* [21] 首通 21 关                                                        */
	[](CutScene& aCutScene, bool)
	{
		if (aCutScene.mApp->IsFirstTimeAdventureMode())
		{
			aCutScene.mCrazyDaveDialogStart = 501;
		}
	},
	/* [22..24] 首通 + 两段卡槽升级叠加序                                     */ CrazyDaveDialogPacketOrSecond, CrazyDaveDialogPacketOrSecond, CrazyDaveDialogPacketOrSecond,
	/* [25] 小麻烦关（原 IsLittleTroubleLevel && 冒险支）                      */
	[](CutScene& aCutScene, bool)
	{
		aCutScene.mCrazyDaveDialogStart = 701;
	},
	/* [26..30] 无开场白（原链落空）                                          */ nullptr, nullptr, nullptr, nullptr, nullptr,
	/* [31] 首通 31 关                                                        */
	[](CutScene& aCutScene, bool)
	{
		if (aCutScene.mApp->IsFirstTimeAdventureMode())
		{
			aCutScene.mCrazyDaveDialogStart = 801;
		}
	},
	/* [32..34] 无开场白（原链落空）                                          */ nullptr, nullptr, nullptr,
	/* [35] 砸罐子关（原 IsScaryPotterLevel && 冒险支）                        */
	[](CutScene& aCutScene, bool)
	{
		aCutScene.mCrazyDaveDialogStart = 2500;
	},
	/* [36..39] 无开场白（原链落空）                                          */ nullptr, nullptr, nullptr, nullptr,
	/* [40] 风暴之夜关（原 IsStormyNightLevel && 冒险支）                      */
	[](CutScene& aCutScene, bool)
	{
		aCutScene.mCrazyDaveDialogStart = 1101;
	},
	/* [41] 首通 41 关                                                        */
	[](CutScene& aCutScene, bool)
	{
		if (aCutScene.mApp->IsFirstTimeAdventureMode())
		{
			aCutScene.mCrazyDaveDialogStart = 1201;
		}
	},
	/* [42..44] 无开场白（原链落空）                                          */ nullptr, nullptr, nullptr,
	/* [45] 蹦极闪击关（原 IsBungeeBlitzLevel && 冒险支，FTAM 二选一）          */
	[](CutScene& aCutScene, bool)
	{
		aCutScene.mCrazyDaveDialogStart = aCutScene.mApp->IsFirstTimeAdventureMode() ? 1301 : 1304;
	},
	/* [46..49] 无开场白（原链落空）                                          */ nullptr, nullptr, nullptr, nullptr,
	/* [50] 最终 Boss 关（原 IsFinalBossLevel && 冒险 && !isRestart 支）        */
	[](CutScene& aCutScene, bool aIsRestart)
	{
		if (!aIsRestart)
		{
			aCutScene.mCrazyDaveDialogStart = 2300;
		}
	},
};
static_assert(sizeof(kCrazyDaveDialogPerLevel) / sizeof(kCrazyDaveDialogPerLevel[0]) == FINAL_LEVEL,
	"冒险关卡→戴夫开场白表槽位须与 FINAL_LEVEL 同步");
static_assert(kCrazyDaveDialogPerLevel[13 - 1] == CrazyDaveDialogPacketUpgrade &&
	kCrazyDaveDialogPerLevel[16 - 1] == CrazyDaveDialogPacketOrSecond &&
	kCrazyDaveDialogPerLevel[24 - 1] == CrazyDaveDialogPacketOrSecond &&
	kCrazyDaveDialogPerLevel[20 - 1] == nullptr &&
	kCrazyDaveDialogPerLevel[40 - 1] != nullptr && kCrazyDaveDialogPerLevel[50 - 1] != nullptr,
	"冒险关卡→戴夫开场白表关键槽锚（升级叠加序/20 落空/40 风暴/50 Boss）须与原链一致");
}  // namespace

//0x43A900
// GOTY @Patoke: 0x43DB50
void CutScene::StartLevelIntro()
{
	mCutsceneTime = 0;
	mBoard->mSeedBank->mX = SEED_BANK_OFFSET_X;
	mBoard->mSeedBank->mY = -IMAGE_SEEDBANK->mHeight;
	mBoard->mMenuButton->mBtnNoDraw = true;
	mApp->mSeedChooserScreen->mMouseVisible = false;
	mApp->mSeedChooserScreen->Move(mApp->mScreenBounds.mX, SEED_CHOOSER_OFFSET_Y);
	mApp->mSeedChooserScreen->mMenuButton->mBtnNoDraw = true;
	mBoard->mShowShovel = false;
	mBoard->mSeedBank->mCutSceneDarken = 255;
	mPlacedZombies = false;
	mPreloaded = false;
	mPlacedLawnItems = false;
	mApp->mWidgetManager->SetFocus(mBoard);

	int aLevel = mBoard->mLevel;
	if (mApp->IsFirstTimeAdventureMode() && (aLevel == 1 || aLevel == 2 || aLevel == 4))
	{
		mSodTime = TimeRollSodEnd - TimeRollSodStart;
		mBoard->mSodPosition = 0;
	}
	else
	{
		mSodTime = 0;
		mBoard->mSodPosition = 1000;
	}

	// Initial graves are already standing; there is no reveal phase to wait for.
	mBoard->mEnableGraveStones = true;

	if (mApp->IsFirstTimeAdventureMode() && aLevel <= 2)
	{
	mReadySetPlantTime = 0;
	}
	else if (mApp->IsShovelLevel() ||
		mApp->IsSquirrelLevel() ||
		mApp->IsWallnutBowlingLevel() ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		mApp->IsIZombieLevel() ||
		mApp->IsWhackAZombieLevel() ||
		mApp->IsScaryPotterLevel())
	{
		mReadySetPlantTime = 0;
	}
	else
	{
		mReadySetPlantTime = TimeReadySetPlantEnd - TimeReadySetPlantStart;
	}

	mLawnMowerTime = 0;
	if (!IsSurvivalRepick())
	{
		mLawnMowerTime = 550;
	}

	bool isRestart = false;
	if (mBoard->mPrevBoardResult == BoardResult::BOARDRESULT_LOST || mBoard->mPrevBoardResult == BoardResult::BOARDRESULT_RESTART)
	{
		isRestart = true;
	}

	// C 形态 -> 函数指针表（上方 kCrazyDaveDialogPerLevel）：冒险按 mLevel-1 直下标，
	// nullptr = 原链落空（保持进函数前值，不写 -1）；原链各复合守卫已随支体入槽。
	if (mApp->IsAdventureMode())
	{
		int aLevelIndex = aLevel - 1;
		if ((unsigned)aLevelIndex < (unsigned)(sizeof(kCrazyDaveDialogPerLevel) / sizeof(kCrazyDaveDialogPerLevel[0])))
		{
			CrazyDaveDialogFn aDialogFn = kCrazyDaveDialogPerLevel[aLevelIndex];
			if (aDialogFn != nullptr)
			{
				aDialogFn(*this, isRestart);
			}
		}
	}

	// 表外保留（混排）：3 支纯模式支按原链序（IZombie-1 / 未通关砸罐-1 / 智慧树）；
	// 冒险模式（mLevel 恒非 0 走上方关卡表）不可能命中，与原链 else-if 先后序等价。
	if (mApp->mGameMode == GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_1)
	{
		mCrazyDaveDialogStart = 2200;
	}
	else if (mApp->mGameMode == GameMode::GAMEMODE_SCARY_POTTER_1 && !mApp->HasBeatenChallenge(GameMode::GAMEMODE_SCARY_POTTER_1))
	{
		mCrazyDaveDialogStart = 3000;
	}
	else if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		if (mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_TREE_FOOD] < PURCHASE_COUNT_OFFSET)
		{
			mCrazyDaveDialogStart = 3200;
			mBoard->mStoreButton->mBtnNoDraw = true;
		}
	}
	if (mCrazyDaveDialogStart != -1)
	{
		mCrazyDaveTime = TimeEarlyDaveLeaveEnd - TimePanRightStart;
		if (mApp->IsFinalBossLevel() && mApp->IsAdventureMode())
		{
			mCrazyDaveTime += 4000;
		}
	}

	if (mBoard->StageHasFog())
	{
		mFogTime = TimeFogRollIn - mSodTime - mLawnMowerTime - TimeReadySetPlantStart + 2000;
	}
	else
	{
		mFogTime = 0;
	}

	if (mApp->IsFinalBossLevel())
	{
		mBossTime = 4000;
	}
	else
	{
		mBossTime = 0;
	}

	if (IsScrolledLeftAtStart())
	{
		mBoard->Move(ConstrainSeedChooserCameraX(BOARD_OFFSET,
			mApp->mScreenBounds.mX, mApp->mScreenBounds.mWidth), 0);
	}
	if (IsNonScrollingCutscene() && mCrazyDaveTime == 0)
	{
		CancelIntro();
		return;
	}

	if (mApp->IsFinalBossLevel() || mApp->IsScaryPotterLevel() || mApp->IsWallnutBowlingLevel())
	{
		PreloadResources();
		PlaceLawnItems();
	}

	SexyString aHouseMessage;
	if (mCrazyDaveTime <= 0 && mApp->mGameMode != GameMode::GAMEMODE_INTRO)
	{
		if (mApp->IsSurvivalMode())
		{
			aHouseMessage = mApp->GetCurrentChallengeDef().mChallengeName;
		}
		else if (mApp->IsAdventureMode())
		{
			if (mBoard->mBackground == BackgroundType::BACKGROUND_1_DAY || mBoard->mBackground == BackgroundType::BACKGROUND_2_NIGHT)
			{
				aHouseMessage = EntityTranslate(gStr.PLAYERS_HOUSE);
			}
			else if (mBoard->mBackground == BackgroundType::BACKGROUND_3_POOL || mBoard->mBackground == BackgroundType::BACKGROUND_4_FOG)
			{
				aHouseMessage = EntityTranslate(gStr.PLAYERS_BACKYARD);
			}
			else if (mBoard->mBackground == BackgroundType::BACKGROUND_5_ROOF || mBoard->mBackground == BackgroundType::BACKGROUND_6_BOSS)
			{
				aHouseMessage = EntityTranslate(gStr.PLAYERS_ROOF);
			}
			else
			{
				TOD_ASSERT(false);
			}
		}
		else
		{
			aHouseMessage = mApp->GetCurrentChallengeDef().mChallengeName;
		}
	}

	aHouseMessage = TodReplaceString(aHouseMessage, UiTextPlaceholder::Player, mApp->mPlayerInfo->mName);
	if (!aHouseMessage.empty())
	{
		mBoard->DisplayAdvice(aHouseMessage, MessageStyle::MESSAGE_STYLE_HOUSE_NAME, AdviceType::ADVICE_NONE);
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
	{
		mApp->mMusic->MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_ZEN_GARDEN);
	}
	else if (mApp->mGameMode == GameMode::GAMEMODE_INTRO)
	{
		mApp->mMusic->MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_POOL_WATERYGRAVES);
	}
	else if (mCrazyDaveTime > 0)
	{
		mApp->mMusic->MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_TITLE_CRAZY_DAVE_MAIN_THEME);
	}
	else if (mApp->IsFinalBossLevel())
	{
		mApp->mMusic->StopAllMusic();
	}
	else
	{
		mApp->mMusic->MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_CHOOSE_YOUR_SEEDS);
	}
}

//0x43B3A0
bool CutScene::IsBeforePreloading()
{
	return mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && !mPreloaded;
}

//0x43B3C0
void CutScene::CancelIntro()
{
	PreloadResources();
	PlaceStreetZombies();
	if (mCutsceneTime < mCrazyDaveTime + TimePanRightEnd)
	{
		mCutsceneTime = TimeSeedChoserSlideOnEnd + mCrazyDaveTime - 20;
		if (!IsNonScrollingCutscene())
		{
			UpdateSeedChooserCamera();
		}
		if (mBoard->mAdvice->mMessageStyle == MessageStyle::MESSAGE_STYLE_HOUSE_NAME)
		{
			mBoard->ClearAdvice(AdviceType::ADVICE_NONE);
		}

		if (mCrazyDaveDialogStart != -1)
		{
			if (mApp->mCrazyDaveState == CrazyDaveState::CRAZY_DAVE_OFF)
			{
				mApp->CrazyDaveEnter();
			}
			mApp->mCrazyDaveMessageIndex = mCrazyDaveDialogStart;
		}
		while (mApp->mCrazyDaveMessageIndex != -1)
		{
			AdvanceCrazyDaveDialog(true);
		}

		if (mBoard->mLevel == 5)
		{
			Plant* aPlant = nullptr;
			while (mBoard->IteratePlants(aPlant))
			{
				aPlant->Die(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
			}
			mBoard->mChallenge->mShowBowlingLine = true;
		}
	}
	mApp->CrazyDaveDie();

	if (mCutsceneTime > mCrazyDaveTime + TimePanLeftStart || !mBoard->ChooseSeedsOnCurrentLevel())
	{
		// 将过场时间快进至关卡引入结束时
		mCutsceneTime = TimeIntroEnd + mLawnMowerTime + mSodTime + mCrazyDaveTime + mFogTime + mBossTime + mReadySetPlantTime - 20;

		PlaceLawnItems();
		if (mApp->IsStormyNightLevel())
		{
			mBoard->mChallenge->mChallengeStateCounter = 0;
		}
		if (mApp->IsFinalBossLevel())
		{
			mBoard->mChallenge->PlayBossEnter();
		}
		if (!mApp->IsChallengeWithoutSeedBank())
		{
			mBoard->mSeedBank->mX = SEED_BANK_OFFSET_X_END;
			mBoard->mSeedBank->mY = 0;
		}

		mBoard->mEnableGraveStones = true;
		ShowShovel();

		if (mApp->IsFinalBossLevel())
		{
			mApp->mMusic->StartGameMusic();
		}

		if (mBoard->mFogBlownCountDown > 0)
		{
			mBoard->mFogBlownCountDown = 0;
			mBoard->mFogOffset = 0;
		}

		if (mBoard->mTutorialState != TutorialState::TUTORIAL_ZEN_GARDEN_PICKUP_WATER)
		{
			mBoard->mMenuButton->mBtnNoDraw = false;
		}
		mApp->mSoundSystem->StopFoley(FoleyType::FOLEY_DIGGER);
	}
	UpdateSeedChooserCamera();
}

namespace
{
	// 冒险屋顶关→花盆列数表（B 形态）：mLevel-1 直下标（1..50）；0 = 原关卡支落空
	// （41/42/43..50 之外的冒险关与非冒险 mLevel 恒 0 均落空，续走 COLUMN/屋顶背景判别）。
	constexpr int kFlowerPotColumnsPerLevel[FINAL_LEVEL] = {
		/* [1..40]  非屋顶关（原关卡支落空）                                    */
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		/* [41]     41 关（屋顶首关）                                            */ 5,
		/* [42]     42 关                                                        */ 4,
		/* [43..50] 屋顶尾声关                                                   */ 3, 3, 3, 3, 3, 3, 3, 3,
	};
	static_assert(sizeof(kFlowerPotColumnsPerLevel) / sizeof(kFlowerPotColumnsPerLevel[0]) == FINAL_LEVEL,
		"冒险屋顶关→花盆列数表槽位须与 FINAL_LEVEL 同步");
	static_assert(kFlowerPotColumnsPerLevel[41 - 1] == 5 && kFlowerPotColumnsPerLevel[42 - 1] == 4 &&
		kFlowerPotColumnsPerLevel[43 - 1] == 3 && kFlowerPotColumnsPerLevel[50 - 1] == 3 &&
		kFlowerPotColumnsPerLevel[40 - 1] == 0,
		"冒险屋顶关→花盆列数表关键槽锚（41=5/42=4/43..50=3/40 落空）须与原链一致");
}  // namespace

//0x43B6F0
void CutScene::AddFlowerPots()
{
	int aPotColumns = 0;
	// B 形态 -> 常量表（上方 kFlowerPotColumnsPerLevel）：mLevel-1 直下标；0 槽 = 原关卡支
	// 落空，续走原 COLUMN 模式与屋顶背景两支（判别式异源，表外保留）。
	int aLevelIndex = mBoard->mLevel - 1;
	if ((unsigned)aLevelIndex < (unsigned)(sizeof(kFlowerPotColumnsPerLevel) / sizeof(kFlowerPotColumnsPerLevel[0])))
	{
		aPotColumns = kFlowerPotColumnsPerLevel[aLevelIndex];
	}
	if (aPotColumns == 0)
	{
		if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_COLUMN)
		{
			aPotColumns = 8;
		}
		else if (mBoard->StageHasRoof())
		{
			aPotColumns = 3;
		}
	}

	for (int x = 0; x < aPotColumns; x++)
	{
		for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
		{
			if (mBoard->CanPlantAt(x, y, SeedType::SEED_FLOWERPOT) == PlantingReason::PLANTING_OK)
			{
				mBoard->NewPlant(x, y, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
			}
		}
	}
}

int CutScene::CalcPosition(std::int64_t theStartNs, std::int64_t theEndNs, int thePositionStart, int thePositionEnd)
{
	return TodAnimateCurveNs(theStartNs, theEndNs, mCutsceneTime.mNanoseconds, thePositionStart, thePositionEnd, Curves::EaseInOut);
}

void CutScene::UpdateSeedChooserCamera()
{
	if (mApp->mGameScene != GameScenes::SCENE_LEVEL_INTRO ||
		mApp->mGameMode == GameMode::GAMEMODE_INTRO)
		return;

	if (mBoard->mBackground >= BackgroundType::BACKGROUND_MUSHROOM_GARDEN)
	{
		// Gardens have centered artwork, not the battle backdrop's [-220,1180] camera.
		mBoard->Move(0, 0);
		return;
	}

	const Rect& aBounds = mApp->mScreenBounds;
	const int aChooserX = ConstrainSeedChooserCameraX(BOARD_WIDTH - BOARD_IMAGE_WIDTH_OFFSET,
		aBounds.mX, aBounds.mWidth);
	const int aStartX = ConstrainSeedChooserCameraX(IsScrolledLeftAtStart() ? BOARD_OFFSET : 0,
		aBounds.mX, aBounds.mWidth);
	int aCameraX;
	const SeedChooserScreen* aChooser = mApp->mSeedChooserScreen;
	if (mSeedChoosing && aChooser && aChooser->mChooseState == CHOOSE_VIEW_LAWN)
	{
		const auto nowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
		if (aChooser->mViewLawnReturnTimeNs > 0)
			aCameraX = TodAnimateCurveNs(0, 1'000'000'000, nowNs-aChooser->mViewLawnReturnTimeNs, 0, aChooserX, Curves::EaseInOut);
		else if (nowNs-aChooser->mViewLawnStartTimeNs <= 1'000'000'000)
			aCameraX = TodAnimateCurveNs(0, 1'000'000'000, nowNs-aChooser->mViewLawnStartTimeNs, aChooserX, 0, Curves::EaseInOut);
		else aCameraX = 0;
	}
	else if (mCutsceneTime <= TimePanRightStart + mCrazyDaveTime)
		aCameraX = aStartX;
	else if (mCutsceneTime <= TimePanRightEnd + mCrazyDaveTime)
		aCameraX = CalcPosition(std::int64_t(TimePanRightStart + mCrazyDaveTime) * 10'000'000,
			std::int64_t(TimePanRightEnd + mCrazyDaveTime) * 10'000'000, aStartX, aChooserX);
	else if (mCutsceneTime <= TimePanLeftStart + mCrazyDaveTime)
		aCameraX = aChooserX;
	else
		aCameraX = CalcPosition(std::int64_t(TimePanLeftStart + mCrazyDaveTime) * 10'000'000,
			std::int64_t(TimePanLeftEnd + mCrazyDaveTime) * 10'000'000, aChooserX, 0);

	// Board::Update applies shake after CutScene::Update. The chooser reapplies
	// this camera afterwards, including the tick that resets shake to zero.
	// The native backdrop is exactly 600 high, so vertical shake has no margin.
	if (mBoard->mShakeCounter > 0)
		aCameraX += TodAnimateCurveNs(120'000'000, 0, mBoard->mShakeCounter.mNanoseconds, 0, mBoard->mShakeAmountX, Curves::Bounce);
	mBoard->Move(ConstrainSeedChooserCameraX(aCameraX, aBounds.mX, aBounds.mWidth), 0);
}

//0x43B7D0
void CutScene::AnimateBoard()
{
	int aTimePanRightEnd = TimePanRightEnd + mCrazyDaveTime;
	int aTimePanLeftStart = TimePanLeftStart + mCrazyDaveTime;
	int aTimePanLeftEnd = TimePanLeftEnd + mCrazyDaveTime;

	// ====================================================================================================
	// ▲ 疯狂戴夫动态的更新
	// ====================================================================================================
	if (mCrazyDaveTime > 0)
	{
		if (mCutsceneTime == TimeEarlyDaveEnterStart)
		{
			mApp->CrazyDaveEnter();
		}

		if (mCutsceneTime == TimeEarlyDaveEnterEnd && mCrazyDaveDialogStart != -1)
		{
			mApp->CrazyDaveTalkIndex(mCrazyDaveDialogStart);
			mCrazyDaveDialogStart = -1;
		}

		if (mCutsceneTime == TimeEarlyDaveLeaveEnd && IsNonScrollingCutscene())
		{
			mCutsceneTime = aTimePanLeftEnd;
		}
	}

	// ====================================================================================================
	// ▲ 关卡界面右移的更新
	// ====================================================================================================
	UpdateSeedChooserCamera();

	// ====================================================================================================
	// ▲ 选卡界面动态的更新
	// ====================================================================================================
	if (mBoard->ChooseSeedsOnCurrentLevel())
	{
		int aTimeSeedChoserSlideOnStart = TimeSeedChoserSlideOnStart + mCrazyDaveTime;
		int aTimeSeedChoserSlideOnEnd = TimeSeedChoserSlideOnEnd + mCrazyDaveTime;
		SeedChooserScreen* aSeedChoser = mApp->mSeedChooserScreen;
		// ====================================================================================================
		// △ 选卡界面滑出
		// ====================================================================================================
		if (mCutsceneTime > aTimeSeedChoserSlideOnStart && mCutsceneTime <= aTimeSeedChoserSlideOnEnd)
		{
			aSeedChoser->Move(mApp->mScreenBounds.mX, CalcPosition(std::int64_t(aTimeSeedChoserSlideOnStart) * 10'000'000, std::int64_t(aTimeSeedChoserSlideOnEnd) * 10'000'000, SEED_CHOOSER_OFFSET_Y, 0));
			aSeedChoser->mMenuButton->mY = CalcPosition(std::int64_t(aTimeSeedChoserSlideOnStart) * 10'000'000, std::int64_t(aTimeSeedChoserSlideOnEnd) * 10'000'000, -50, -10);
			aSeedChoser->mMenuButton->mBtnNoDraw = false;
		}
		// ====================================================================================================
		// △ 选卡界面滑落
		// ====================================================================================================
		int aTimeSeedChoserSlideOffStart = TimeSeedChoserSlideOffStart + mCrazyDaveTime;
		int aTimeSeedChoserSlideOffEnd = TimeSeedChoserSlideOffEnd + mCrazyDaveTime;
		if (mCutsceneTime > aTimeSeedChoserSlideOffStart && mCutsceneTime <= aTimeSeedChoserSlideOffEnd)
		{
			aSeedChoser->Move(mApp->mScreenBounds.mX, CalcPosition(std::int64_t(aTimeSeedChoserSlideOffStart) * 10'000'000, std::int64_t(aTimeSeedChoserSlideOffEnd) * 10'000'000, 0, SEED_CHOOSER_OFFSET_Y));
			aSeedChoser->mMenuButton->mDisabled = true;
		}
	}

	// ====================================================================================================
	// ▲ 卡槽动态的更新
	// ====================================================================================================
	int aTimePrepareEnd = 0;
	if (!mBoard->ChooseSeedsOnCurrentLevel())
	{
		aTimePrepareEnd = mBossTime + mFogTime + mSodTime - TimeSeedChoserSlideOnStart + TimePanLeftEnd;
	}
	int aTimeSeedBankOnStart = TimeSeedBankOnStart + aTimePrepareEnd + mCrazyDaveTime;
	int aTimeSeedBankOnEnd = TimeSeedBankOnEnd + aTimePrepareEnd + mCrazyDaveTime;
	if (!mApp->IsChallengeWithoutSeedBank() && mCutsceneTime > aTimeSeedBankOnStart && mCutsceneTime <= aTimeSeedBankOnEnd)
	{
		int aSeedBankY = CalcPosition(std::int64_t(aTimeSeedBankOnStart) * 10'000'000, std::int64_t(aTimeSeedBankOnEnd) * 10'000'000, -IMAGE_SEEDBANK->mHeight, 0);
		mBoard->mSeedBank->mX = SEED_BANK_OFFSET_X;
		mBoard->mSeedBank->mY = aSeedBankY;
	}
	int aTimeSeedBankRightStart = TimeSeedBankRightStart + mCrazyDaveTime;
	int aTimeSeedBankRightEnd = TimeSeedBankRightEnd + mCrazyDaveTime;
	if (mCutsceneTime > aTimeSeedBankRightStart)
	{
		int aSeedBankX = CalcPosition(std::int64_t(aTimeSeedBankRightStart) * 10'000'000, std::int64_t(aTimeSeedBankRightEnd) * 10'000'000, SEED_BANK_OFFSET_X, SEED_BANK_OFFSET_X_END);
		int aDarken = TodAnimateCurveNs(std::int64_t(aTimeSeedBankRightStart) * 10'000'000, std::int64_t(aTimeSeedBankRightEnd) * 10'000'000, mCutsceneTime.mNanoseconds, 255, 128, Curves::EaseOut);
		mBoard->mSeedBank->mCutSceneDarken = aDarken;
		mBoard->mSeedBank->mX = aSeedBankX;
	}

	// ====================================================================================================
	// ▲ 冒险模式初期关卡铺草皮的更新
	// ====================================================================================================
	if (mSodTime > 0)
	{
		int aTimeRollSodStart = TimeRollSodStart + mCrazyDaveTime;
		int aTimeRollSodEnd = TimeRollSodEnd + mCrazyDaveTime;
		mBoard->mSodPosition = TodAnimateCurveNs(std::int64_t(aTimeRollSodStart) * 10'000'000, std::int64_t(aTimeRollSodEnd) * 10'000'000, mCutsceneTime.mNanoseconds, 0, 1000, Curves::Linear);

		if (mCutsceneTime == aTimeRollSodStart)
		{
			mApp->PlayFoley(FoleyType::FOLEY_DIGGER);
			if (mBoard->mLevel == 1)
			{
				mApp->AddReanimation(0, 0, Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 0), ReanimationType::REANIM_SODROLL, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
				AddVisualParticle(35, 348, Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 1), ParticleEffect::PARTICLE_SOD_ROLL);
			}
			else if (mBoard->mLevel == 2)
			{
				mApp->AddReanimation(0, -102, Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 0), ReanimationType::REANIM_SODROLL, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
				mApp->AddReanimation(0, 111, Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 0), ReanimationType::REANIM_SODROLL, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
				AddVisualParticle(35, 246, Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 1), ParticleEffect::PARTICLE_SOD_ROLL);
				AddVisualParticle(35, 459, Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 1), ParticleEffect::PARTICLE_SOD_ROLL);
			}
			else if (mBoard->mLevel == 4)
			{
				mApp->AddReanimation(-3, -198, Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 0), ReanimationType::REANIM_SODROLL, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
				mApp->AddReanimation(-3, 203, Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 0), ReanimationType::REANIM_SODROLL, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
				AddVisualParticle(32, 150, Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 1), ParticleEffect::PARTICLE_SOD_ROLL);
				AddVisualParticle(32, 511, Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 1), ParticleEffect::PARTICLE_SOD_ROLL);
			}
		}

		if (mCutsceneTime == aTimeRollSodEnd)
		{
			mApp->mSoundSystem->StopFoley(FoleyType::FOLEY_DIGGER);
		}
	}

	// ====================================================================================================
	// ▲ 画面开始向左滚动时，创建战场物品
	// ====================================================================================================
	if (mCutsceneTime == aTimePanLeftStart)
	{
		PlaceLawnItems();
	}

	// ====================================================================================================
	// ▲ 每一行小推车的启动的更新
	// ====================================================================================================
	if (!IsSurvivalRepick())
	{
		for (int aGridY = 0; aGridY < MAX_GRID_SIZE_Y; aGridY++)
		{
			int aTimeLawnMowerStart = TimeLawnMowerStart[aGridY] + mSodTime + mCrazyDaveTime;
			if (mCutsceneTime > aTimeLawnMowerStart)
			{
				LawnMower* aLawnMower = mBoard->FindLawnMowerInRow(aGridY);
				if (aLawnMower)
				{
					aLawnMower->mVisible = true;
					aLawnMower->mPosX = CalcPosition(std::int64_t(aTimeLawnMowerStart) * 10'000'000, std::int64_t(aTimeLawnMowerStart + TimeLawnMowerDuration) * 10'000'000, -80, -21);
				}
			}
		}
	}

	// ====================================================================================================
	// ▲ 浓雾的更新
	// ====================================================================================================
	if (mBoard->mFogBlownCountDown > 0)
	{
		int aTimeFogRollIn = TimeFogRollIn + mSodTime + mCrazyDaveTime;
		if (mCutsceneTime > aTimeFogRollIn)
		{
			if (mBoard->mFogBlownCountDown > 200)
			{
				mBoard->mFogBlownCountDown = 200;
			}
			mBoard->mFogBlownCountDown--;
		}
	}

	// ====================================================================================================
	// ▲ 暴风雨的更新
	// ====================================================================================================
	if (mApp->IsStormyNightLevel() && (mCutsceneTime == aTimePanRightEnd - 1000 || mCutsceneTime == aTimePanLeftEnd))
	{
		mBoard->mChallenge->mChallengeState = ChallengeState::STATECHALLENGE_STORM_FLASH_2;
		mBoard->mChallenge->mChallengeStateCounter = 310;
	}

	// ====================================================================================================
	// ▲ 僵王博士的入场
	// ====================================================================================================
	if (mBossTime > 0)
	{
		int aTimeBossEnter = TimeReadySetPlantStart + mLawnMowerTime + mCrazyDaveTime;
		if (mCutsceneTime == aTimeBossEnter)
		{
			mBoard->mChallenge->PlayBossEnter();
		}
	}

	// ====================================================================================================
	// ▲ 僵王博士关卡背景音乐的播放
	// ====================================================================================================
	if (mApp->IsFinalBossLevel() && mCutsceneTime == aTimeSeedBankOnStart)
	{
		mApp->mMusic->StartGameMusic();
	}

	// ====================================================================================================
	// ▲ Ready Set Plant 动画的播放
	// ====================================================================================================
	int aTimeReadySetPlant = TimeReadySetPlantStart + mLawnMowerTime + mSodTime + mCrazyDaveTime + mFogTime + mBossTime;
	if (mReadySetPlantTime > 0 && mCutsceneTime == aTimeReadySetPlant)
	{
		mApp->AddReanimation(400, 324, Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_SCREEN_FADE, 0, 0), ReanimationType::REANIM_READYSETPLANT, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
		mApp->PlaySample(SOUND_READYSETPLANT);
		if (!mApp->IsFinalBossLevel())
		{
			mApp->mMusic->FadeOut(150);
		}
	}
	if (mReadySetPlantTime == 0 && mCutsceneTime == aTimeReadySetPlant - 2000)
	{
		if (!mApp->IsFinalBossLevel())
		{
			mApp->mMusic->FadeOut(200);
		}
	}

	// ====================================================================================================
	// ▲ 将选卡界面移动至顶层显示
	// ====================================================================================================
	mApp->mSeedChooserScreen->mParent->BringToFront(mApp->mSeedChooserScreen);
}

//0x43C140
void CutScene::ShowShovel()
{
	if (mApp->IsWhackAZombieLevel() ||
		mApp->IsWallnutBowlingLevel() ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST ||
		mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM ||
		mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM ||
		mApp->IsIZombieLevel())
		return;

	if (!mApp->IsFirstTimeAdventureMode() || mBoard->mLevel > 4)
	{
		mBoard->mShowShovel = true;
	}
}

//0x43C1E0
bool CutScene::IsInShovelTutorial()
{
	return
		mBoard->mTutorialState == TutorialState::TUTORIAL_SHOVEL_PICKUP ||
		mBoard->mTutorialState == TutorialState::TUTORIAL_SHOVEL_DIG ||
		mBoard->mTutorialState == TutorialState::TUTORIAL_SHOVEL_KEEP_DIGGING;
}

void CutScene::StartSeedChooser()
{
	mApp->mSeedChooserScreen->mMouseVisible = true;
	mSeedChoosing = true;
	mApp->mWidgetManager->SetFocus(mApp->mSeedChooserScreen);
}

void CutScene::EndSeedChooser()
{
	mApp->mSeedChooserScreen->mMouseVisible = false;
	mSeedChoosing = false;
	mCutsceneTime = mCrazyDaveTime + TimeSeedChoserSlideOnEnd + 10;
	mApp->mWidgetManager->SetFocus(mBoard);
}

bool CutScene::IsShowingCrazyDave()
{
	return mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && (mCrazyDaveTime > 0 && mCutsceneTime < TimePanRightEnd + mCrazyDaveTime);
}

TodParticleSystem* CutScene::AddVisualParticle(float x, float y, int renderOrder, ParticleEffect effect)
{
    auto* particle=mApp->AddTodParticle(x,y,renderOrder,effect,
        mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs);
    if (particle) {
        particle->mIsAttachment=true; // UpdateVisualParticles is its sole update owner.
        mVisualParticleIDs.push_back(mApp->ParticleGetID(particle));
    }
    return particle;
}

void CutScene::UpdateVisualParticles(std::int64_t nowNs, bool simulate)
{
    for (auto it=mVisualParticleIDs.begin();it!=mVisualParticleIDs.end();) {
        auto* particle=mApp->ParticleTryToGet(*it);
        if (!particle || particle->mDead) { it=mVisualParticleIDs.erase(it); continue; }
        const auto deltaNs=simulate && nowNs>particle->mLastUpdateTimeNs
            ? static_cast<std::uint64_t>(nowNs-particle->mLastUpdateTimeNs) : 0;
        particle->Update(deltaNs,nowNs);
        ++it;
    }
}

//0x43C200
// GOTY @Patoke: 0x43F440
void CutScene::Update()
{
	if (mPreUpdatingBoard)
		return;
	UpdateVisualParticles(mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs,!mBoard->mPaused);

	// 更新疯狂戴夫
	if (IsShowingCrazyDave() && !mBoard->mPaused)
	{
		mApp->UpdateCrazyDave();
	}

	if (mBoard->mPaused)
		return;

	// 僵尸进家过场的更新
	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
	{
		mCutsceneTime.Advance(10);
		UpdateZombiesWon();
		return;
	}

	if (mApp->mGameScene != GameScenes::SCENE_LEVEL_INTRO || mBoard->mDrawCount == 0)
		return;

	// 进行预加载
	if (!mPreloaded)
	{
		PreloadResources();
	}
	// 放置预览僵尸
	if (!mPlacedZombies)
	{
		PlaceStreetZombies();
	}
	// 放置战场物品
	if (IsNonScrollingCutscene() || !mBoard->ChooseSeedsOnCurrentLevel())
	{
		PlaceLawnItems();
	}

	// 选卡之前的更新
	bool aCutsceneTimeStop = false;
	if (mSeedChoosing || mApp->mCrazyDaveMessageIndex != -1 || IsInShovelTutorial())
	{
		aCutsceneTimeStop = true;
	}
	if (mApp->mGameMode == GameMode::GAMEMODE_INTRO)
	{
		mCutsceneTime.Advance(10);
		UpdateIntro();
		return;
	}
	if (!aCutsceneTimeStop)
	{
		mCutsceneTime.Advance(10);
		if (mCutsceneTime == TimeSeedChoserSlideOnEnd + mCrazyDaveTime && mBoard->ChooseSeedsOnCurrentLevel())
		{
			StartSeedChooser();
		}
	}

	// 过场结束的判定
	int aTimeStart = TimeIntroEnd + mLawnMowerTime + mSodTime + mCrazyDaveTime + mFogTime + mBossTime + mReadySetPlantTime;
	if (mCutsceneTime >= aTimeStart)
	{
		mBoard->RemoveCutsceneZombies();
		if (mBoard->mTutorialState != TutorialState::TUTORIAL_ZEN_GARDEN_PICKUP_WATER)
		{
			mBoard->mMenuButton->mBtnNoDraw = false;
		}

		ShowShovel();
		mApp->StartPlaying();
		return;
	}

	AnimateBoard();
}

//0x43C3C0
void CutScene::StartZombiesWon()
{
	mCutsceneTime = 0;
	mBoard->mMenuButton->mBtnNoDraw = true;
	mBoard->mShowShovel = false;
	mApp->mMusic->StopAllMusic();
	mBoard->StopAllZombieSounds();
	mApp->PlaySample(SOUND_LOSEMUSIC);
}

//0x43C410
void CutScene::UpdateZombiesWon()
{
	// Retail 1073 (0043F650) pans 0 -> 220 with the original easing.
	// Keep that curve, constrained by the backdrop visible at this width.
	const Rect& aView = mApp->mScreenBounds;
	mBoard->Move(ZombiesWonLayout::CameraX(
		CalcPosition(std::int64_t(LostTimePanRightStart) * 10'000'000, std::int64_t(LostTimePanRightEnd) * 10'000'000, 0, BOARD_OFFSET),
		aView.mX, aView.mWidth), 0);
	if (Reanimation* aWon = mApp->ReanimationTryToGet(mZombiesWonReanimID))
	{
		aWon->mOverlayMatrix.m02 = ZombiesWonLayout::CanvasX(aView.mX, aView.mWidth, mBoard->mX);
	}

	// 啃食脑子的音效
	if (mCutsceneTime == LostTimeBrainGraphicStart - 400 || mCutsceneTime == LostTimeBrainGraphicStart - 900)
	{
		mApp->PlayFoley(FoleyType::FOLEY_CHOMP);
	}

	// 食脑的动画及惨叫的音效
	if (mCutsceneTime == LostTimeBrainGraphicStart)
	{
		ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_ZOMBIES_WON, true);
		int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_SCREEN_FADE, 0, 0);
		Reanimation* aReanimation = mApp->AddReanimation(
			ZombiesWonLayout::CanvasX(aView.mX, aView.mWidth, mBoard->mX),
			0, aRenderPosition, ReanimationType::REANIM_ZOMBIES_WON, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
		aReanimation->ApplyRate(12.0f);
		aReanimation->ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
		aReanimation->GetTrackInstanceByName(ReanimTrackId::fullscreen)->mTrackColor = Color::Black;
		mZombiesWonReanimID = mApp->ReanimationGetID(aReanimation);
		aReanimation->SetFramesForLayer(ReanimTrackId::ZombiesWon);
		mApp->PlayFoley(FoleyType::FOLEY_SCREAM);
	}

	// 食脑动画开始抖动
	if (mCutsceneTime == LostTimeBrainGraphicShake)
	{
		mApp->ReanimationGet(mZombiesWonReanimID)->SetShakeOverride(ReanimTrackId::ZombiesWon, 1.0f);
	}
	// 食脑动画结束抖动
	if (mCutsceneTime == LostTimeBrainGraphicCancelShake)
	{
		mApp->ReanimationGet(mZombiesWonReanimID)->SetShakeOverride(ReanimTrackId::ZombiesWon, 0.0f);
	}
	// 食脑动画结束
	if (mCutsceneTime == LostTimeBrainGraphicEnd)
	{
		mApp->ReanimationGet(mZombiesWonReanimID)->SetFramesForLayer(ReanimTrackId::anim_screen);
	}

	// 过场结束，游戏失败
	if (mCutsceneTime == LostTimeEnd)
	{
		if (mApp->IsSurvivalMode())
		{
			int aFlagsCompleted = mBoard->GetSurvivalFlagsCompleted();
			SexyString aFlagsStr = mApp->Pluralize(aFlagsCompleted, gStr.ONE_FLAG.c_str(), gStr.COUNT_FLAGS.c_str());
			SexyString aStr = TodReplaceString(gStr.SURVIVAL_DEATH_MESSAGE, UiTextPlaceholder::Flags, aFlagsStr);
			GameOverDialog* aDialog = new GameOverDialog(aStr, true);
			mApp->AddDialog(Dialogs::DIALOG_GAME_OVER, aDialog);
		}
		else
		{
			GameOverDialog* aDialog = new GameOverDialog((""), false);
			mApp->AddDialog(Dialogs::DIALOG_GAME_OVER, aDialog);
		}
	}
}

bool CutScene::IsCutSceneOver()
{
	TOD_ASSERT(mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON);
	return mCutsceneTime >= LostTimeEnd;
}

//0x43C910
void CutScene::ZombieWonClick()
{
	if (IsCutSceneOver() || mApp->mTodCheatKeys)
	{
		mApp->EndLevel();
	}
}

//0x43C950
void CutScene::AdvanceCrazyDaveDialog(bool theJustSkipping)
{
	if (mApp->mCrazyDaveMessageIndex == -1)
		return;
    // Do not advance the speaker behind his outstanding confirmation.
    // The explicit skip loop must still be able to advance to its terminator.
    if (!theJustSkipping && mApp->GetDialog(DIALOG_PURCHASE_PACKET_SLOT))
        return;

	// “拿起铲子开始挖吧”
	if (mApp->mCrazyDaveMessageIndex == 2406 && !theJustSkipping)
	{
		mBoard->SetTutorialState(TutorialState::TUTORIAL_SHOVEL_PICKUP);
		mApp->CrazyDaveLeave();
		return;
	}
	// “这是你的智慧树，我会给你一些肥料让你开始的”
	if (mApp->mCrazyDaveMessageIndex == 3200)
	{
		// GOTY 1073 (0043FB90): profile +0x258 is tree food (28), not first aid (29).
		mApp->mPlayerInfo->mPurchases[STORE_ITEM_TREE_FOOD] = PURCHASE_COUNT_OFFSET + 5;
		mBoard->mMenuButton->mBtnNoDraw = false;
		mBoard->mStoreButton->mBtnNoDraw = false;
	}

	// 推进戴夫对话，若不存在下一句则令戴夫退出
	if (!mApp->AdvanceCrazyDaveText())
	{
		mApp->CrazyDaveLeave();
		if (mApp->IsFinalBossLevel() && mApp->IsAdventureMode())
		{
			Reanimation* aCrazyDaveReanim = mApp->ReanimationTryToGet(mApp->mCrazyDaveReanimID);
			aCrazyDaveReanim->PlayReanim(ReanimTrackId::anim_grab, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 0, 18.0f);

			mApp->mMusic->FadeOut(50);
			if (!theJustSkipping)
			{
				mApp->PlaySample(SOUND_BUNGEE_SCREAM);
			}
		}
		else if (mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
		{
			mApp->mMusic->MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_ZEN_GARDEN);
		}
		else
		{
			if (mBoard->ChooseSeedsOnCurrentLevel())
			{
				mApp->mMusic->MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_CHOOSE_YOUR_SEEDS);
			}
			else if (IsNonScrollingCutscene())
			{
				mApp->mMusic->FadeOut(50);
			}
		}
		return;
	}

	// 更新为最新的一句话的编号
	int aMessageIndex = mApp->mCrazyDaveMessageIndex;
	// Now_Unused
	if (aMessageIndex == 107 || aMessageIndex == 2407)
	{
		mBoard->mChallenge->ShovelAddWallnuts();
	}
	// “并且不是铁锹，是短槌” || “我们去玩保龄球！”
	if (aMessageIndex == 405 || aMessageIndex == 2411)
	{
		mBoard->mChallenge->mShowBowlingLine = true;
	}
	// （推销卡槽）“听起来怎么样”
	if ((aMessageIndex == 1503 || aMessageIndex == 1553) && !theJustSkipping)
	{
        DaveUpgradeSession aSession(*this);
        if (!aSession.IsCurrent())
            return;
		int aCost = StoreScreen::GetItemCost(StoreItem::STORE_ITEM_PACKET_UPGRADE);
		int aNumPackets = mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PACKET_UPGRADE];
		SexyString aBodyString = TodReplaceNumberString(gStr.UPGRADE_DIALOG_BODY, UiTextPlaceholder::Slots, aNumPackets + 7);
		SexyString aAmountString = mApp->GetMoneyString(aCost);
		// 创建询问是否升级卡槽格数的对话
		Dialog* aDialog = mApp->DoDialog(Dialogs::DIALOG_PURCHASE_PACKET_SLOT, true, aAmountString, aBodyString, (""), Dialog::BUTTONS_YES_NO);
        DaveUpgradeDialog aDialogLifetime(*mApp, aDialog);
		aDialog->mX += 120;
		aDialog->mY += 130;
		mBoard->mCoinBankFadeCount = 100;
		// 等待返回选择的选项
		int aResult = aDialog->WaitForResult();
        if (!aSession.IsCurrent() || mApp->GetDialog(DIALOG_PURCHASE_PACKET_SLOT) ||
            mApp->mPlayerInfo->mPurchases[STORE_ITEM_PACKET_UPGRADE] != aNumPackets ||
            StoreScreen::GetItemCost(STORE_ITEM_PACKET_UPGRADE) != aCost)
            return;
        // AddCoins clamps negative balances; that is not permission to grant
        // an upgrade that the player can no longer afford.
		if (aResult == Dialog::ID_YES && mApp->mPlayerInfo->mCoins >= aCost)
		{
			mApp->mPlayerInfo->AddCoins(-aCost);
			mApp->mPlayerInfo->mPurchases[(int)StoreItem::STORE_ITEM_PACKET_UPGRADE]++;
			mApp->WriteCurrentUserConfig();
			mBoard->mSeedBank->UpdateWidth();

			if (aMessageIndex == 1503)
			{
				mApp->CrazyDaveTalkIndex(1510);
			}
			else if (aMessageIndex == 1553)
			{
				mApp->CrazyDaveTalkIndex(1560);
			}
		}
		else
		{
			mApp->mPlayerInfo->mDidntPurchasePacketUpgrade++;
			if (aMessageIndex == 1503)
			{
				mApp->CrazyDaveTalkIndex(1520);
			}
			else if (aMessageIndex == 1553)
			{
				mApp->CrazyDaveTalkIndex(1570);
			}
		}
	}
}

//0x43CE00
void CutScene::MouseDown(int theX, int theY)
{
	(void)theX;(void)theY;
	if (false) // GAMEMODE_UPSELL 已删除
	{
		if (mCrazyDaveCountDown > 1)
		{
			mCrazyDaveCountDown = 1;
		}
	}
	else
	{
		if (IsShowingCrazyDave())
		{
			AdvanceCrazyDaveDialog(false);
		}
		else if (mApp->mTodCheatKeys)
		{
			CancelIntro();
		}
	}
}

//0x43D280
void CutScene::KeyDown(KeyCode theKey)
{
	if (false) // GAMEMODE_UPSELL 已删除
	{
		if (mApp->mTodCheatKeys && theKey == KeyCode::KEYCODE_ESCAPE)
		{
			mCrazyDaveLastTalkIndex = 3316; // “这足够把你的脑子吹到火星，再吹回来！”
			mCrazyDaveCountDown = 1;
		}
		else if (theKey == KeyCode::KEYCODE_SPACE || theKey == KeyCode::KEYCODE_RETURN || theKey == KeyCode::KEYCODE_ESCAPE)
		{
			mApp->CrazyDaveStopSound();
			mApp->PlaySample(SOUND_PAUSE);
			mApp->mMusic->GameMusicPause(true);

			int aResult = mApp->LawnMessageBox(
				Dialogs::DIALOG_MESSAGE,
				gStr.UPSELL_PAUSE_HEADER.c_str(),
				gStr.UPSELL_PAUSE_BODY.c_str(),
				gStr.UPSELL_RESUME_BUTTON.c_str(),
				gStr.MAIN_MENU_BUTTON.c_str(),
				Dialog::BUTTONS_YES_NO
			);
			if (aResult == Dialog::ID_NO)
			{
				mApp->KillCreditScreen();
				mApp->DoBackToMain();
			}

			mApp->mMusic->GameMusicPause(false);
		}
	}
	else
	{
		if ((theKey == KeyCode::KEYCODE_SPACE || theKey == KeyCode::KEYCODE_RETURN) && IsShowingCrazyDave())
		{
			AdvanceCrazyDaveDialog(false);
		}
		else if (mApp->mTodCheatKeys && (theKey == KeyCode::KEYCODE_SPACE || theKey == KeyCode::KEYCODE_RETURN || theKey == KeyCode::KEYCODE_ESCAPE))
		{
			CancelIntro();
		}
	}
}

//0x43D830
int CutScene::ParseDelayTimeFromMessage()
{
	const DaveDialogueMetadata& aMetadata = GetDaveDialogueMetadata(mCrazyDaveLastTalkIndex);
	if ((aMetadata.flags & DaveDialogueHasDelay) != 0)
	{
		mCrazyDaveCountDown = aMetadata.delay;
		return mCrazyDaveCountDown;
	}
	return 100;
}

//0x43D940
int CutScene::ParseTalkTimeFromMessage()
{
	const DaveDialogueMetadata& aMetadata = GetDaveDialogueMetadata(mCrazyDaveLastTalkIndex);
	if ((aMetadata.flags & DaveDialogueHasTime) != 0)
	{
		mCrazyDaveCountDown = aMetadata.time;
		return mCrazyDaveCountDown;
	}
	return 100;
}

//0x43DA50
void CutScene::ClearUpsellBoard()
{
	for (int i = 0; i < MAX_GRID_SIZE_Y; i++)
	{
		mBoard->mIceTimer[i] = 0;
		mBoard->mIceMinX[i] = BOARD_WIDTH;
	}

	mBoard->mZombies.DataArrayFreeAll();
	mBoard->RebuildZombieRowBucket();  // 行段桶：池清空后桶必须同步清（intro 案：街头预览僵尸
	                                   // 清池后留幽灵条目，演示僵尸同号槽注册被陈旧 mSlotIdx 拒，
	                                   // 死亡摘除错位搬移 → 续走死循环，shadow 对拍 bucket=9 pool=4 定罪）
	mBoard->mPlants.DataArrayFreeAll();
	mBoard->RebuildPlantCellBucket();  // 格桶：池清空后桶必须同步清
	mBoard->mCoins.DataArrayFreeAll();
	mBoard->mProjectiles.DataArrayFreeAll();
	mBoard->mGridItems.DataArrayFreeAll();
	mBoard->mLawnMowers.DataArrayFreeAll();

	TodParticleSystem* aParticle = nullptr;
	while (mBoard->IterateParticles(aParticle))
	{
		aParticle->ParticleSystemDie();
	}
	Reanimation* aReanim = nullptr;
	while (mBoard->IterateReanimations(aReanim))
	{
		// 1073 preserves the animation type, not just the app's two Dave IDs.
		if (aReanim->mReanimationType != ReanimationType::REANIM_CRAZY_DAVE)
			aReanim->ReanimationDie();
	}
	mBoard->mPoolSparklyParticleID = ParticleSystemID::PARTICLESYSTEMID_NULL;

	if (mUpsellChallengeScreen)
	{
		delete mUpsellChallengeScreen;
		mUpsellChallengeScreen = nullptr;
	}
}

void CutScene::AddUpsellZombie(ZombieType theZombieType, int thePixelX, int theGridY)
{
	Zombie* aZombie = mBoard->AddZombieInRow(theZombieType, theGridY, 0);
	{ aZombie->mX = (thePixelX); aZombie->mRootBindingGeneration = 0; }
	aZombie->mY = (aZombie->GetPosYBasedOnRow(theGridY));
	aZombie->SetRow(theGridY);


}

//0x43DBA0
void CutScene::LoadIntroBoard()
{
	ClearUpsellBoard();
	mApp->mMuteSoundsForCutscene = true;

	mBoard->NewPlant(0, 1, SeedType::SEED_THREEPEATER, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 2, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 2, SeedType::SEED_PEASHOOTER, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 3, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 3, SeedType::SEED_PEASHOOTER, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 4, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 0, SeedType::SEED_THREEPEATER, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 1, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 2, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 2, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 4, SeedType::SEED_THREEPEATER, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 5, SeedType::SEED_THREEPEATER, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 0, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 1, SeedType::SEED_PEASHOOTER, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 3, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 3, SeedType::SEED_PEASHOOTER, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 4, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 5, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 0, SeedType::SEED_TORCHWOOD, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 4, SeedType::SEED_THREEPEATER, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 2, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 2, SeedType::SEED_TORCHWOOD, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 1, SeedType::SEED_TORCHWOOD, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 4, SeedType::SEED_TORCHWOOD, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 5, SeedType::SEED_TORCHWOOD, SeedType::SEED_NONE);
	mBoard->NewPlant(6, 0, SeedType::SEED_SPIKEWEED, SeedType::SEED_NONE);
	mBoard->NewPlant(6, 4, SeedType::SEED_SPIKEWEED, SeedType::SEED_NONE);
	mBoard->NewPlant(7, 1, SeedType::SEED_SPIKEWEED, SeedType::SEED_NONE);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 460, 0);
	AddUpsellZombie(ZombieType::ZOMBIE_FOOTBALL, 680, 0);
	AddUpsellZombie(ZombieType::ZOMBIE_TRAFFIC_CONE, 730, 0);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 810, 0);
	AddUpsellZombie(ZombieType::ZOMBIE_TRAFFIC_CONE, 670, 1);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 740, 1);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 880, 1);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 500, 2);
	AddUpsellZombie(ZombieType::ZOMBIE_TRAFFIC_CONE, 680, 2);
	AddUpsellZombie(ZombieType::ZOMBIE_PAIL, 604, 3);
	AddUpsellZombie(ZombieType::ZOMBIE_SNORKEL, 880, 3);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 600, 4);
	AddUpsellZombie(ZombieType::ZOMBIE_PAIL, 690, 4);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 780, 4);
	AddUpsellZombie(ZombieType::ZOMBIE_CATAPULT, 730, 5);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 590, 5);

	// Settle the demonstration for one authored second, independent of the
	// caller's frame rate. Only the nested simulation advances; the movie and
	// application clocks remain owned by the outer update.
	const auto aIntervalNs = Sexy::gUpdateIntervalNs;
	const auto aTickPhaseNs = Sexy::gLegacyTickPhaseNs;
	const auto aWholeTicks = Sexy::gWholeLegacyTicks;
	const auto aUpdateTicks = Sexy::gUpdateTicks;
	const auto aStepsPerTick = Sexy::gStepsPerLegacyTick;
	const auto aRestoreStep = [&]() {
		Sexy::gUpdateIntervalNs = aIntervalNs;
		Sexy::gLegacyTickPhaseNs = aTickPhaseNs;
		Sexy::gWholeLegacyTicks = aWholeTicks;
		Sexy::gUpdateTicks = aUpdateTicks;
		Sexy::gStepsPerLegacyTick = aStepsPerTick;
		mPreUpdatingBoard = false;
	};
	mPreUpdatingBoard = true;
	try
	{
		for (unsigned long long aRemainingNs = 1'000'000'000ULL; aRemainingNs;)
		{
			const auto aStepNs = std::min(aRemainingNs, aIntervalNs ? aIntervalNs : aRemainingNs);
			Sexy::ApplySimulationStep(aStepNs);
			Sexy::BeginTimedUpdate();
			mBoard->Update();
			aRemainingNs -= aStepNs;
		}
	}
	catch (...)
	{
		aRestoreStep();
		throw;
	}
	aRestoreStep();
}

//0x43E890
void CutScene::LoadUpsellBoardPool()
{
	ClearUpsellBoard();
	mApp->mMuteSoundsForCutscene = true;

	mBoard->NewPlant(0, 1, SeedType::SEED_THREEPEATER, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 2, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 2, SeedType::SEED_PEASHOOTER, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 3, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 3, SeedType::SEED_PEASHOOTER, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 4, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 0, SeedType::SEED_THREEPEATER, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 1, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 2, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 2, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 4, SeedType::SEED_THREEPEATER, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 5, SeedType::SEED_THREEPEATER, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 0, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 1, SeedType::SEED_PEASHOOTER, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 3, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 3, SeedType::SEED_PEASHOOTER, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 4, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 5, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 4, SeedType::SEED_THREEPEATER, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 0, SeedType::SEED_TORCHWOOD, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 2, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 2, SeedType::SEED_TORCHWOOD, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 1, SeedType::SEED_TORCHWOOD, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 4, SeedType::SEED_TORCHWOOD, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 5, SeedType::SEED_TORCHWOOD, SeedType::SEED_NONE);
	mBoard->NewPlant(6, 0, SeedType::SEED_SPIKEWEED, SeedType::SEED_NONE);
	mBoard->NewPlant(6, 3, SeedType::SEED_TANGLEKELP, SeedType::SEED_NONE);
	mBoard->NewPlant(6, 4, SeedType::SEED_SPIKEWEED, SeedType::SEED_NONE);
	mBoard->NewPlant(6, 5, SeedType::SEED_SQUASH, SeedType::SEED_NONE);
	mBoard->NewPlant(7, 1, SeedType::SEED_SPIKEWEED, SeedType::SEED_NONE);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 460, 0);
	AddUpsellZombie(ZombieType::ZOMBIE_ZAMBONI, 680, 0);
	AddUpsellZombie(ZombieType::ZOMBIE_TRAFFIC_CONE, 670, 1);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 740, 1);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 500, 2);
	AddUpsellZombie(ZombieType::ZOMBIE_TRAFFIC_CONE, 680, 2);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 604, 3);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 690, 4);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 740, 4);
	AddUpsellZombie(ZombieType::ZOMBIE_PAIL, 730, 5);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 590, 5);

	mPreUpdatingBoard = true;
	for (int i = 0; i < 100; i++)
	{
		mBoard->Update();
	}
	mPreUpdatingBoard = false;
	mApp->mMuteSoundsForCutscene = false;
}

//0x43F310
void CutScene::LoadUpsellBoardFog()
{
	ClearUpsellBoard();
	mApp->mMuteSoundsForCutscene = true;

	mBoard->mBackground = BackgroundType::BACKGROUND_4_FOG;
	mBoard->LoadBackgroundImages();

	mBoard->NewPlant(0, 1, SeedType::SEED_SUNSHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 4, SeedType::SEED_SUNSHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 0, SeedType::SEED_SUNSHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 1, SeedType::SEED_SUNSHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 2, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 2, SeedType::SEED_CACTUS, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 4, SeedType::SEED_SUNSHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 5, SeedType::SEED_SUNSHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 0, SeedType::SEED_CACTUS, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 4, SeedType::SEED_CACTUS, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 5, SeedType::SEED_FUMESHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 1, SeedType::SEED_FUMESHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 2, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 3, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 3, SeedType::SEED_CACTUS, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 5, SeedType::SEED_PUFFSHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 0, SeedType::SEED_PUFFSHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 1, SeedType::SEED_MAGNETSHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 2, SeedType::SEED_SEASHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 5, SeedType::SEED_PUFFSHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 1, SeedType::SEED_PUFFSHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 2, SeedType::SEED_LILYPAD, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 2, SeedType::SEED_PLANTERN, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 3, SeedType::SEED_SEASHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(6, 2, SeedType::SEED_SEASHROOM, SeedType::SEED_NONE);
	mBoard->NewPlant(6, 3, SeedType::SEED_SEASHROOM, SeedType::SEED_NONE);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 460, 0);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 680, 0);
	AddUpsellZombie(ZombieType::ZOMBIE_BALLOON, 780, 0);
	AddUpsellZombie(ZombieType::ZOMBIE_TRAFFIC_CONE, 670, 1);
	AddUpsellZombie(ZombieType::ZOMBIE_BALLOON, 640, 1);
	AddUpsellZombie(ZombieType::ZOMBIE_PAIL, 640, 2);
	AddUpsellZombie(ZombieType::ZOMBIE_TRAFFIC_CONE, 780, 3);
	AddUpsellZombie(ZombieType::ZOMBIE_BALLOON, 704, 4);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 690, 4);
	AddUpsellZombie(ZombieType::ZOMBIE_PAIL, 590, 5);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 740, 5);

	mPreUpdatingBoard = true;
	for (int i = 0; i < 100; i++)
	{
		mBoard->Update();
	}
	mPreUpdatingBoard = false;
	mApp->mMuteSoundsForCutscene = false;
}

//0x43FD20
void CutScene::LoadUpsellChallengeScreen()
{
	ClearUpsellBoard();
	mUpsellChallengeScreen = new ChallengeScreen(mApp, ChallengePage::CHALLENGE_PAGE_CHALLENGE);
}

//0x43FD90
void CutScene::LoadUpsellBoardRoof()
{
	ClearUpsellBoard();
	mApp->mMuteSoundsForCutscene = true;

	mBoard->mBackground = BackgroundType::BACKGROUND_5_ROOF;
	mBoard->LoadBackgroundImages();
	mBoard->mPlantRow[0] = PlantRowType::PLANTROW_NORMAL;
	mBoard->mPlantRow[1] = PlantRowType::PLANTROW_NORMAL;
	mBoard->mPlantRow[2] = PlantRowType::PLANTROW_NORMAL;
	mBoard->mPlantRow[3] = PlantRowType::PLANTROW_NORMAL;
	mBoard->mPlantRow[4] = PlantRowType::PLANTROW_NORMAL;
	mBoard->mPlantRow[5] = PlantRowType::PLANTROW_DIRT;
	for (int x = 0; x < MAX_GRID_SIZE_X; x++)
	{
		for (int y = 0; y < MAX_GRID_SIZE_Y; y++)
		{
			if (mBoard->mPlantRow[y] == PlantRowType::PLANTROW_DIRT)
			{
				mBoard->mGridSquareType[x][y] = GridSquareType::GRIDSQUARE_DIRT;
			}
			else
			{
				mBoard->mGridSquareType[x][y] = GridSquareType::GRIDSQUARE_GRASS;
			}
		}
	}

	mBoard->NewPlant(0, 0, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 0, SeedType::SEED_CABBAGEPULT, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 1, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 1, SeedType::SEED_CABBAGEPULT, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 2, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 2, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 3, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 3, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 4, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(0, 4, SeedType::SEED_CABBAGEPULT, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 0, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 0, SeedType::SEED_CABBAGEPULT, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 1, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 1, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 2, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 2, SeedType::SEED_CABBAGEPULT, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 3, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 3, SeedType::SEED_CABBAGEPULT, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 4, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(1, 4, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 0, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 0, SeedType::SEED_CABBAGEPULT, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 1, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 1, SeedType::SEED_CABBAGEPULT, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 2, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 2, SeedType::SEED_CABBAGEPULT, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 3, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 3, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 4, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(2, 4, SeedType::SEED_CABBAGEPULT, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 1, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 1, SeedType::SEED_CABBAGEPULT, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 2, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 2, SeedType::SEED_CABBAGEPULT, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 3, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 3, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 4, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(3, 4, SeedType::SEED_CABBAGEPULT, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 0, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 0, SeedType::SEED_CHOMPER, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 1, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 1, SeedType::SEED_CHOMPER, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 2, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 2, SeedType::SEED_REPEATER, SeedType::SEED_NONE);
	mBoard->NewPlant(4, 3, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 2, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 2, SeedType::SEED_WALLNUT, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 3, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 3, SeedType::SEED_THREEPEATER, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 4, SeedType::SEED_FLOWERPOT, SeedType::SEED_NONE);
	mBoard->NewPlant(5, 4, SeedType::SEED_WALLNUT, SeedType::SEED_NONE);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 460, 0);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 680, 0);
	AddUpsellZombie(ZombieType::ZOMBIE_CATAPULT, 780, 1);
	AddUpsellZombie(ZombieType::ZOMBIE_TRAFFIC_CONE, 670, 1);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 580, 0);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 540, 1);
	AddUpsellZombie(ZombieType::ZOMBIE_PAIL, 500, 1);
	AddUpsellZombie(ZombieType::ZOMBIE_PAIL, 640, 2);
	AddUpsellZombie(ZombieType::ZOMBIE_TRAFFIC_CONE, 780, 3);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 380, 3);
	AddUpsellZombie(ZombieType::ZOMBIE_CATAPULT, 704, 4);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 690, 4);
	AddUpsellZombie(ZombieType::ZOMBIE_NORMAL, 590, 4);

	mPreUpdatingBoard = true;
	for (int k = 0; k < 100; k++)
	{
		mBoard->Update();
	}
	mPreUpdatingBoard = false;
	mApp->mMuteSoundsForCutscene = false;
}

// GAMEMODE_UPSELL + UpdateUpsell 已删除（试玩版促销页，全库零赋值死代码）


//0x441320
void CutScene::DrawUpsell(Graphics* g)
{
	if (mCrazyDaveLastTalkIndex == 3315)  // “大地科塔！”
	{
		Reanimation aReanim;
		aReanim.ReanimationInitializeType(565, 360, ReanimationType::REANIM_FLOWER_POT, 0);
		aReanim.SetFramesForLayer(ReanimTrackId::anim_zengarden);
		aReanim.mOverlayMatrix.m00 = 1.3f;
		aReanim.mOverlayMatrix.m11 = 1.3f;
		aReanim.Draw(g);
		mBoard->mMenuButton->Draw(g);
		aReanim.ReanimationDie();
	}

	if (mUpsellChallengeScreen)
	{
		mUpsellChallengeScreen->Draw(g);
		mBoard->mMenuButton->Draw(g);
	}
}

//0x441480
void CutScene::UpdateIntro()
{
	// Retain the authored intro pan. At the widest viewport the native backdrop
	// has no spare pan margin, so uncovered sides intentionally remain black.
	mBoard->Move(-TodAnimateCurveNs(std::int64_t(TimeIntro_PanRightStart) * 10'000'000, std::int64_t(TimeIntro_PanRightEnd) * 10'000'000, mCutsceneTime.mNanoseconds, -100, 100, Curves::Linear), 0);

	if (mCutsceneTime == 10)
	{
		LoadIntroBoard();
	}
	if (mCutsceneTime == TimeIntro_FadeOut)
	{
		mApp->mMusic->FadeOut(250);
	}
	if (mCutsceneTime == TimeIntro_LogoEnd)
	{
		int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_TOP, 0, 0);
		AddVisualParticle(400, 300, aRenderPosition, ParticleEffect::PARTICLE_SCREEN_FLASH);

		mApp->mMuteSoundsForCutscene = false;
		mApp->PlaySample(SOUND_HUGE_WAVE);
		mApp->mMuteSoundsForCutscene = true;
	}
	if (mCutsceneTime == TimeIntro_FadeOut - 200)
	{
		mApp->mMuteSoundsForCutscene = false;
		mApp->PlaySample(SOUND_SIREN);
		mApp->mMuteSoundsForCutscene = true;
	}
	if (mCutsceneTime == TimeIntro_End)
	{
		mApp->PreNewGame(GameMode::GAMEMODE_ADVENTURE, false);
	}
}

//0x441590
void CutScene::DrawIntro(Graphics* g)
{
	const Rect& aBounds = mApp->mScreenBounds;
	const Rect aScreenRect(aBounds.mX - mBoard->mX, aBounds.mY - mBoard->mY,
		aBounds.mWidth, aBounds.mHeight);
	if (mCutsceneTime <= TimeIntro_PanRightStart || mCutsceneTime > TimeIntro_FadeOutEnd)
	{
		g->mColor = Color::Black;
		g->FillRect(aScreenRect);
	}

	// 绘制“PopCap Games 出品”字样
	int aTimePanRightStart = TimeIntro_PanRightStart - TimeIntro_PresentsFadeIn;
	if (mCutsceneTime > TimeIntro_PresentsFadeIn && mCutsceneTime <= aTimePanRightStart)
	{
		int anAlpha = mCutsceneTime < aTimePanRightStart - 600 ?
					  TodAnimateCurveNs(std::int64_t(TimeIntro_PresentsFadeIn) * 10'000'000, std::int64_t(TimeIntro_PresentsFadeIn + 300) * 10'000'000, mCutsceneTime.mNanoseconds, 0, 255, Curves::Linear) :
					  TodAnimateCurveNs(std::int64_t(aTimePanRightStart - 600) * 10'000'000, std::int64_t(aTimePanRightStart - 300) * 10'000'000, mCutsceneTime.mNanoseconds, 255, 0, Curves::Linear);

		TodDrawString(
			g,
			gStr.INTRO_PRESENTS,
			BOARD_WIDTH / 2 - mBoard->mX,
			310 - mBoard->mY,
			FONT_BRIANNETOD32,
			Color(255, 255, 255, anAlpha),
			DrawStringJustification::DS_ALIGN_CENTER
		);
	}

	// 绘制“Plants Vs Zombies”的 Logo
	if (mCutsceneTime > TimeIntro_LogoStart && mCutsceneTime <= TimeIntro_PanRightEnd)
	{
		float aScale = TodAnimateCurveNs(std::int64_t(TimeIntro_LogoStart) * 10'000'000, std::int64_t(TimeIntro_LogoEnd) * 10'000'000, mCutsceneTime.mNanoseconds, 5.0f, 1.0f, Curves::EaseOut);
		float aCenter = aScale * 0.5;
		int aOffsetX = BOARD_WIDTH / 2 - mBoard->mX, aOffsetY = BOARD_HEIGHT / 2 - mBoard->mY;
		const Rect aRect(aScreenRect.mX, aOffsetY - 75 * aScale,
			aScreenRect.mWidth, 150 * aScale);
		g->mColor = Color(0, 0, 0, 128);
		g->FillRect(aRect);
		Image* aImage = IMAGE_PVZ_LOGO;
		TodDrawImageScaledF(g, aImage, aOffsetX - aImage->mWidth * aCenter, aOffsetY - aImage->mHeight * aCenter, aScale, aScale);
	}

	if (mCutsceneTime > TimeIntro_FadeOut && mCutsceneTime <= TimeIntro_FadeOutEnd)
	{
		g->mColor = Color(0, 0, 0, TodAnimateCurveNs(std::int64_t(TimeIntro_FadeOut) * 10'000'000, std::int64_t(TimeIntro_FadeOutEnd) * 10'000'000, mCutsceneTime.mNanoseconds, 0, 255, Curves::Linear));
		g->FillRect(aScreenRect);
	}
}

bool CutScene::ShouldRunUpsellBoard()
{
	return mApp->mGameMode == GameMode::GAMEMODE_INTRO && !mUpsellHideBoard;
}

bool CutScene::IsAfterSeedChooser()
{
	return mCutsceneTime > TimeSeedChoserSlideOffStart + mCrazyDaveTime;
}

bool CutScene::ShowZombieWalking()
{
	return mCutsceneTime > LostTimePanRightStart;
}
