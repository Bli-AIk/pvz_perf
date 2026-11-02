#include "LawnCommon.h"
#include "Coin.h"
#include "Board.h"
#include "Plant.h"
#include "Zombie.h"
#include "GridItem.h"
#include "Cutscene.h"
#include "ZenGarden.h"
#include "LawnMower.h"
#include "Challenge.h"
#include "SeedPacket.h"
#include "Projectile.h"
#include "LawnApp.h"
#include "graphics/DDInterface.h"
#include "Resources.h"
#include "CursorObject.h"
#include "System/Music.h"
#include "ToolTipWidget.h"
#include "MessageWidget.h"
#include "GameConstants.h"
#include "System/PlayerInfo.h"
#include "Widget/GameButton.h"
#include "Widget/StoreScreen.h"
#include "Sexy.TodLib/TodDebug.h"
#include "Sexy.TodLib/TodFoley.h"
#include "Sexy.TodLib/TodCommon.h"
#include "graphics/Font.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "misc/Debug.h"
#include "misc/MTRand.h"
#include "misc/FrameCounterMath.h"
#include "Sexy.TodLib/TodParticle.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "Sexy.TodLib/generated/EntityStrings.h"
#include "widget/WidgetManager.h"
#include "Widget/AchievementsWidget.h"
#include <stdexcept>

int gZombieWaves[NUM_LEVELS] = {  //0x6A34E8
	4,  6,  8,  10, 8,  10, 20, 10, 20, 20,
	10, 20, 10, 20, 10, 10, 20, 10, 20, 20,
	10, 20, 20, 30, 20, 20, 30, 20, 30, 30,
	10, 20, 10, 20, 20, 10, 20, 10, 20, 20,
	10, 20, 20, 30, 20, 20, 30, 20, 30, 30,
};

ZombieAllowedLevels gZombieAllowedLevels[NUM_ZOMBIE_TYPES] = {  //0x6A35B0
	{ ZOMBIE_NORMAL,
		{
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		}
	},
	{ ZOMBIE_FLAG,
		{
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		}
	},
	{ ZOMBIE_TRAFFIC_CONE,
		{
			0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
			0, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
			1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
		}
	},
	{ ZOMBIE_POLEVAULTER,
		{
			0, 0, 0, 0, 0, 1, 1, 0, 1, 1,
			0, 0, 0, 1, 1, 0, 0, 0, 0, 0,
			0, 0, 0, 1, 0, 0, 0, 0, 1, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
		}
	},
	{ ZOMBIE_PAIL,
		{
			0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
			0, 1, 0, 0, 1, 0, 0, 0, 0, 0,
			0, 1, 0, 1, 0, 0, 1, 0, 1, 1,
			0, 0, 0, 0, 0, 0, 1, 0, 1, 1,
			0, 1, 0, 0, 1, 0, 0, 0, 1, 1,
		}
	},
	{ ZOMBIE_NEWSPAPER,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			1, 1, 0, 0, 1, 0, 0, 0, 0, 0,
			0, 1, 0, 1, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		}
	},
	{ ZOMBIE_DOOR,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 1, 1, 0, 0, 1, 0, 1, 1,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		}
	},
	{ ZOMBIE_FOOTBALL,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 1, 1, 0, 0, 1,
			0, 1, 0, 0, 1, 0, 0, 0, 0, 0,
			0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
		}
	},
	{ ZOMBIE_DANCER,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		}
	},
	{ ZOMBIE_BACKUP_DANCER,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0
		}
	},
	{ ZOMBIE_DUCKY_TUBE, { 0 } },
	{ ZOMBIE_SNORKEL,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 1, 1, 1, 0, 1, 0, 0, 1,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		}
	},
	{ ZOMBIE_ZAMBONI,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 1, 1, 0, 1, 1,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		}
	},
	{ ZOMBIE_BOBSLED,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 1, 1, 0, 1, 1,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		}
	},
	{ ZOMBIE_DOLPHIN_RIDER,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
			0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		}
	},
	{ ZOMBIE_JACK_IN_THE_BOX,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			1, 1, 0, 0, 0, 0, 1, 0, 0, 1,
			0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
		}
	},
	{ ZOMBIE_BALLOON,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 1, 1, 0, 0, 0, 0, 1, 1,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		}
	},
	{ ZOMBIE_DIGGER,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 1, 1, 0, 0, 1,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		}
	},
	{ ZOMBIE_POGO,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
			0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
		}
	},
	{ ZOMBIE_YETI, {0} },
	{ ZOMBIE_BUNGEE,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			1, 1, 0, 0, 0, 0, 1, 0, 1, 1,
		}
	},
	{ ZOMBIE_LADDER,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 1, 1, 1, 0, 1, 0, 1, 1,
		}
	},
	{ ZOMBIE_CATAPULT,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 1, 1, 0, 1, 1,
		}
	},
	{ ZOMBIE_GARGANTUAR,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
		}
	},
	{ ZOMBIE_IMP,
		{
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 1, 1, 1,
		}
	},
	{ ZOMBIE_BOSS, {0} },
	{ ZOMBIE_PEA_HEAD, {0} },
	{ ZOMBIE_WALLNUT_HEAD, {0} },
	{ ZOMBIE_JALAPENO_HEAD, {0} },
	{ ZOMBIE_GATLING_HEAD, {0} },
	{ ZOMBIE_SQUASH_HEAD, {0} },
	{ ZOMBIE_TALLNUT_HEAD, {0} },
	{ ZOMBIE_REDEYE_GARGANTUAR, {0} },
};

static SeedType gArtChallengeWallnut[MAX_GRID_SIZE_Y][MAX_GRID_SIZE_X] = {  //0x6A3260
	{SEED_NONE,     SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_WALLNUT,   SEED_WALLNUT,   SEED_WALLNUT,   SEED_NONE,      SEED_NONE},
	{SEED_NONE,     SEED_NONE,      SEED_NONE,      SEED_WALLNUT,   SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_WALLNUT,   SEED_NONE},
	{SEED_NONE,     SEED_NONE,      SEED_NONE,      SEED_WALLNUT,   SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_WALLNUT,   SEED_NONE},
	{SEED_NONE,     SEED_NONE,      SEED_NONE,      SEED_WALLNUT,   SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_WALLNUT,   SEED_NONE},
	{SEED_NONE,     SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_WALLNUT,   SEED_WALLNUT,   SEED_WALLNUT,   SEED_NONE,      SEED_NONE},
	{SEED_NONE,     SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE}
};

static SeedType gArtChallengeSunFlower[MAX_GRID_SIZE_Y][MAX_GRID_SIZE_X] = {  //0x6A3338
	{SEED_NONE,     SEED_NONE,      SEED_STARFRUIT, SEED_STARFRUIT, SEED_STARFRUIT, SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE},
	{SEED_NONE,     SEED_STARFRUIT, SEED_WALLNUT,   SEED_WALLNUT,   SEED_WALLNUT,   SEED_STARFRUIT, SEED_NONE,      SEED_NONE,      SEED_NONE},
	{SEED_NONE,     SEED_NONE,      SEED_STARFRUIT, SEED_STARFRUIT, SEED_STARFRUIT, SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE},
	{SEED_NONE,     SEED_NONE,      SEED_NONE,      SEED_UMBRELLA,  SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE},
	{SEED_NONE,     SEED_NONE,      SEED_UMBRELLA,  SEED_UMBRELLA,  SEED_UMBRELLA,  SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE},
	{SEED_NONE,     SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE}
};

static SeedType gArtChallengeStarFruit[MAX_GRID_SIZE_Y][MAX_GRID_SIZE_X] = {  //0x6A3410
	{SEED_NONE,     SEED_NONE,      SEED_NONE,      SEED_STARFRUIT, SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE},
	{SEED_NONE,     SEED_NONE,      SEED_NONE,      SEED_STARFRUIT, SEED_STARFRUIT, SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE},
	{SEED_NONE,     SEED_STARFRUIT, SEED_STARFRUIT, SEED_STARFRUIT, SEED_STARFRUIT, SEED_STARFRUIT, SEED_STARFRUIT, SEED_NONE,      SEED_NONE},
	{SEED_NONE,     SEED_NONE,      SEED_NONE,      SEED_STARFRUIT, SEED_STARFRUIT, SEED_STARFRUIT, SEED_NONE,      SEED_NONE,      SEED_NONE},
	{SEED_NONE,     SEED_NONE,      SEED_NONE,      SEED_STARFRUIT, SEED_NONE,      SEED_NONE,      SEED_STARFRUIT, SEED_NONE,      SEED_NONE},
	{SEED_NONE,     SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE,      SEED_NONE}
};

//0x41F1B0
Challenge::Challenge()
{
	mApp = (LawnApp*)gSexyAppBase;
	mBoard = mApp->mBoard;
	mBeghouledMouseCapture = false;
	mBeghouledMouseDownX = 0;
	mBeghouledMouseDownY = 0;
	mChallengeStateCounter = 0;
	mConveyorBeltCounter = 0;
	mChallengeScore = 0;
	mChallengeState = STATECHALLENGE_NORMAL;
	mShowBowlingLine = false;
	mLastConveyorSeedType = SEED_NONE;
	mSurvivalStage = 0;
	mSlotMachineRollCount = 0;
	mReanimChallenge = REANIMATIONID_NULL;
	mChallengeGridX = 0;
	mChallengeGridY = 0;
	mScaryPotterPots = 0;
	mBeghouledMatchesThisMove = 0;
	mRainCounter = 0;
	mTreeOfWisdomTalkIndex = 0;
	for (int i = 0; i < 6; i++)
		mReanimClouds[i] = REANIMATIONID_NULL;
	memset(mBeghouledEated, 0, sizeof(mBeghouledEated));
	for (int i = 0; i < (int)BeghouledUpgrade::NUM_BEGHOULED_UPGRADES; i++)
		mBeghouledPurcasedUpgrade[i] = false;

	if (mApp->mBoard && mApp->mGameMode == GAMEMODE_MINIGAME_SLOT_MACHINE)
	{
		ReanimatorEnsureDefinitionLoaded(REANIM_SLOT_MACHINE_HANDLE, true);
		// The authored handle origin is local to the seed bank, independent of
		// the current viewport and the Board camera used by hit testing.
		Reanimation* aHandleReanim = mApp->AddReanimation(96, -20, 0, REANIM_SLOT_MACHINE_HANDLE, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
		aHandleReanim->mIsAttachment = true;
		aHandleReanim->ApplyRate(0.0f);
		mReanimChallenge = mApp->ReanimationGetID(aHandleReanim);
	}
}

//0x41F320
static constexpr ReanimTrackId aGrowLayers[] = {
		ReanimTrackId::anim_grow1,
		ReanimTrackId::anim_grow2,
		ReanimTrackId::anim_grow3,
		ReanimTrackId::anim_grow4,
		ReanimTrackId::anim_grow5,
		ReanimTrackId::anim_grow6,
		ReanimTrackId::anim_grow7,
		ReanimTrackId::anim_grow8,
		ReanimTrackId::anim_grow9,
		ReanimTrackId::anim_grow10,
		ReanimTrackId::anim_grow11,
		ReanimTrackId::anim_grow12,
		ReanimTrackId::anim_grow13,
		ReanimTrackId::anim_grow14,
		ReanimTrackId::anim_grow15,
		ReanimTrackId::anim_grow16,
		ReanimTrackId::anim_grow17,
		ReanimTrackId::anim_grow18,
		ReanimTrackId::anim_grow19,
		ReanimTrackId::anim_grow20,
		ReanimTrackId::anim_grow21,
		ReanimTrackId::anim_grow22,
		ReanimTrackId::anim_grow23,
		ReanimTrackId::anim_grow24,
		ReanimTrackId::anim_grow25,
		ReanimTrackId::anim_grow26,
		ReanimTrackId::anim_grow27,
		ReanimTrackId::anim_grow28,
		ReanimTrackId::anim_grow29,
		ReanimTrackId::anim_grow30,
		ReanimTrackId::anim_grow31,
		ReanimTrackId::anim_grow32,
		ReanimTrackId::anim_grow33,
		ReanimTrackId::anim_grow34,
		ReanimTrackId::anim_grow35,
		ReanimTrackId::anim_grow36,
		ReanimTrackId::anim_grow37,
		ReanimTrackId::anim_grow38,
		ReanimTrackId::anim_grow39,
		ReanimTrackId::anim_grow40,
		ReanimTrackId::anim_grow41,
		ReanimTrackId::anim_grow42,
		ReanimTrackId::anim_grow43,
		ReanimTrackId::anim_grow44,
		ReanimTrackId::anim_grow45,
		ReanimTrackId::anim_grow46,
		ReanimTrackId::anim_grow47,
		ReanimTrackId::anim_grow48,
		ReanimTrackId::anim_grow49,
		ReanimTrackId::anim_grow50,
		ReanimTrackId::anim_grow51,
	};

void Challenge::LoadBeghouledBoardState(BeghouledBoardState* theBoardState)
{
	for (int i = 0; i < MAX_GRID_SIZE_X; i++)
		for (int j = 0; j < MAX_GRID_SIZE_Y; j++)
			theBoardState->mSeedType[i][j] = SEED_NONE;

	Plant* aPlant = nullptr;
	while (mBoard->IteratePlants(aPlant))
		theBoardState->mSeedType[aPlant->mPlantCol][aPlant->mRow] = aPlant->mSeedType;
}

//0x41F380
void Challenge::InitLevel()
{
	if (mApp->mGameMode == GAMEMODE_MINIGAME_RAINING_SEEDS)
	{
		mChallengeStateCounter = 100;
		mApp->PlayFoley(FOLEY_RAIN);
	}
	if (mApp->IsStormyNightLevel())
	{
		mChallengeState = STATECHALLENGE_STORM_FLASH_2;
		mChallengeStateCounter = 100;
		mApp->PlayFoley(FOLEY_RAIN);
	}
	if (mApp->IsFinalBossLevel())
	{
		mBoard->mSeedBank->AddSeed(SEED_CABBAGEPULT);
		mBoard->mSeedBank->AddSeed(SEED_JALAPENO);
		mBoard->mSeedBank->AddSeed(SEED_CABBAGEPULT);
		mBoard->mSeedBank->AddSeed(SEED_ICESHROOM);
		mConveyorBeltCounter = 1000;
	}
	if (mApp->mGameMode == GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		mApp->mZenGarden->mGardenType = GARDEN_MAIN;
		mApp->mZenGarden->ZenGardenInitLevel();
	}
	if (mApp->mGameMode == GAMEMODE_MINIGAME_COLUMN)
	{
		mBoard->mSeedBank->AddSeed(SEED_POTATOMINE);
		mBoard->mSeedBank->AddSeed(SEED_TALLNUT);
		mBoard->mSeedBank->AddSeed(SEED_MELONPULT);
		mBoard->mSeedBank->AddSeed(SEED_MAGNETSHROOM);
		mBoard->mSeedBank->AddSeed(SEED_INSTANT_COFFEE);
		mBoard->mSeedBank->AddSeed(SEED_MELONPULT);
		mConveyorBeltCounter = 1000;
	}
	if (mApp->mGameMode == GAMEMODE_MINIGAME_INVISIGHOUL)
	{
		mBoard->mSeedBank->AddSeed(SEED_PEASHOOTER);
		mBoard->mSeedBank->AddSeed(SEED_ICESHROOM);
		mConveyorBeltCounter = 1000;
	}
	if (mApp->IsIZombieLevel())
	{
		IZombieInitLevel();
	}
	if (mApp->IsScaryPotterLevel())
	{
		ScaryPotterPopulate();
	}
	if (mApp->IsFirstTimeAdventureMode() && mBoard->mLevel == 5)
	{
		mBoard->NewPlant(5, 1, SEED_PEASHOOTER, SEED_NONE);
		mBoard->NewPlant(7, 2, SEED_PEASHOOTER, SEED_NONE);
		mBoard->NewPlant(6, 3, SEED_PEASHOOTER, SEED_NONE);
	}
	if (mApp->mGameMode == GAMEMODE_MINIGAME_BEGHOULED_TWIST)
	{
		mChallengeGridX = -1;
		mChallengeGridY = -1;
	}
	if (mApp->mGameMode == GAMEMODE_TREE_OF_WISDOM)
	{
		TreeOfWisdomInit();
	}
}

//0x41F6E0
void Challenge::StartLevel()
{
	if (mApp->IsWhackAZombieLevel())
	{
		mBoard->mCursorObject->mCursorType = CURSOR_TYPE_HAMMER;
		mBoard->mZombieCountDown = 200;
		mBoard->mZombieCountDownStart = mBoard->mZombieCountDown;
	}
	if (mApp->IsStormyNightLevel())
	{
		mChallengeState = STATECHALLENGE_STORM_FLASH_1;
		mChallengeStateCounter = 400;
	}
	GameMode aGameMode = mApp->mGameMode;
	if (aGameMode == GAMEMODE_MINIGAME_BOBSLED_BONANZA)
	{
		for (int i = 0; i < MAX_GRID_SIZE_Y; i++)
		{
			if (mBoard->mPlantRow[i] != PLANTROW_POOL)
			{
				mBoard->mIceMinX[i] = 400;
				mBoard->mIceTimer[i] = 0x7FFFFFFF;
			}
		}
	}
	if (mApp->IsWallnutBowlingLevel())
	{
		mBoard->mZombieCountDown = 200;
		mBoard->mZombieCountDownStart = mBoard->mZombieCountDown;
		mBoard->mSeedBank->AddSeed(SEED_WALLNUT);
		mConveyorBeltCounter = 400;
		mShowBowlingLine = true;
	}
	if (aGameMode == GAMEMODE_LIMBO_SHOVEL || aGameMode == GAMEMODE_LIMBO_SQUIRREL)
	{
		ShovelAddWallnuts();
	}
	if (mApp->IsScaryPotterLevel())
	{
		ScaryPotterStart();
	}
	if (mApp->IsLittleTroubleLevel() || mApp->IsStormyNightLevel() || mApp->IsBungeeBlitzLevel() || aGameMode == GAMEMODE_MINIGAME_INVISIGHOUL)
	{
		mBoard->mZombieCountDown = 200;
		mBoard->mZombieCountDownStart = mBoard->mZombieCountDown;
		mConveyorBeltCounter = 200;
	}
	if (mApp->IsSurvivalMode() && mSurvivalStage == 0)
	{
		SexyString aMessage =
			mApp->IsSurvivalNormal(aGameMode) ? TodReplaceNumberString(gStr.ADVICE_SURVIVE_FLAGS, UiTextPlaceholder::Flags, SURVIVAL_NORMAL_FLAGS) :
			mApp->IsSurvivalHard(aGameMode) ? TodReplaceNumberString(gStr.ADVICE_SURVIVE_FLAGS, UiTextPlaceholder::Flags, SURVIVAL_HARD_FLAGS) :
			gStr.ADVICE_SURVIVE_ENDLESS;
		mBoard->DisplayAdvice(aMessage, MESSAGE_STYLE_HINT_FAST, ADVICE_SURVIVE_FLAGS);
	}
	if (aGameMode == GAMEMODE_MINIGAME_LAST_STAND && mSurvivalStage == 0)
	{
		mBoard->DisplayAdvice(TodReplaceNumberString(gStr.ADVICE_SURVIVE_FLAGS, UiTextPlaceholder::Flags, LAST_STAND_FLAGS), MESSAGE_STYLE_BIG_MIDDLE_FAST, ADVICE_SURVIVE_FLAGS);
	}
	if (aGameMode == GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT)
	{
		mBoard->DisplayAdvice(gStr.ADVICE_FILL_IN_WALLNUTS, MESSAGE_STYLE_HINT_FAST, ADVICE_NONE);
	}
	if (aGameMode == GAMEMODE_LIMBO_ART_CHALLENGE_SUNFLOWER)
	{
		mBoard->DisplayAdvice(gStr.ADVICE_FILL_IN_SPACES, MESSAGE_STYLE_HINT_FAST, ADVICE_NONE);
	}
	if (aGameMode == GAMEMODE_MINIGAME_SEEING_STARS)
	{
		mBoard->DisplayAdvice(gStr.ADVICE_FILL_IN_STARFRUIT, MESSAGE_STYLE_HINT_FAST, ADVICE_NONE);
	}
	if (mApp->IsSlotMachineLevel())
	{
		mBoard->SetTutorialState(TUTORIAL_SLOT_MACHINE_PULL);
	}
	if (aGameMode == GAMEMODE_MINIGAME_BEGHOULED || aGameMode == GAMEMODE_MINIGAME_BEGHOULED_TWIST)
	{
		mBoard->mZombieCountDown = 200;
		mBoard->mZombieCountDownStart = mBoard->mZombieCountDown;
		mChallengeStateCounter = 1500;
		BeghouledMakeStartBoard();
		BeghouledUpdateCraters();

		if (aGameMode == GAMEMODE_MINIGAME_BEGHOULED)
		{
			mBoard->DisplayAdvice(gStr.ADVICE_BEGHOULED_DRAG_TO_MATCH_3, MESSAGE_STYLE_HINT_FAST, ADVICE_NONE);
		}
		else if (aGameMode == GAMEMODE_MINIGAME_BEGHOULED_TWIST)
		{
			mBoard->DisplayAdvice(gStr.ADVICE_BEGHOULED_TWIST_TO_MATCH_3, MESSAGE_STYLE_HINT_FAST, ADVICE_NONE);
		}
	}
	if (mApp->IsMiniBossLevel())
	{
		mBoard->mZombieCountDown = 100;
		mBoard->mZombieCountDownStart = mBoard->mZombieCountDown;
		mConveyorBeltCounter = 200;
	}
	if (aGameMode == GAMEMODE_MINIGAME_PORTAL_COMBAT)
	{
		PortalStart();
	}
	if (aGameMode == GAMEMODE_MINIGAME_COLUMN)
	{
		mBoard->mCurrentWave = 9;
		mBoard->mZombieCountDown = 2400;
	}
	if (aGameMode == GAMEMODE_LIMBO_AIR_RAID || aGameMode == GAMEMODE_MINIGAME_BOBSLED_BONANZA)
	{
		mBoard->mZombieCountDown = 4500;
	}
	if (aGameMode == GAMEMODE_MINIGAME_POGO_PARTY)
	{
		mBoard->mZombieCountDown = 5500;
	}
	if (aGameMode == GAMEMODE_MINIGAME_ZOMBIQUARIUM)
	{
		mBoard->DisplayAdvice(gStr.ADVICE_ZOMBIQUARIUM_CLICK_TO_FEED, MESSAGE_STYLE_HINT_TALL_FAST, ADVICE_ZOMBIQUARIUM_CLICK_TO_FEED);
		ZombiquariumSpawnSnorkle();
		ZombiquariumSpawnSnorkle();
	}
	if (mApp->IsIZombieLevel())
	{
		IZombieStart();
	}
	if (mApp->IsSquirrelLevel())
	{
		SquirrelStart();
	}
}

//0x420150
int Challenge::BeghouledTwistValidMove(int theGridX, int theGridY, BeghouledBoardState* theBoardState)
{
	if (theGridY == -1 || theGridX > 6 || theGridY > 3)
		return false;

	return
		theBoardState->mSeedType[theGridX][theGridY] != SEED_NONE &&
		theBoardState->mSeedType[theGridX + 1][theGridY] != SEED_NONE &&
		theBoardState->mSeedType[theGridX][theGridY + 1] != SEED_NONE &&
		theBoardState->mSeedType[theGridX + 1][theGridY + 1] != SEED_NONE;
}

//0x420190
int Challenge::BeghouledTwistMoveCausesMatch(int theGridX, int theGridY, BeghouledBoardState* theBoardState)
{
	// 1073 00422CD0 also rejects the left-edge sentinel before reading cells.
	if (theGridX == -1 || !BeghouledTwistValidMove(theGridX, theGridY, theBoardState))
		return false;

	SeedType aSeed1 = theBoardState->mSeedType[theGridX][theGridY];
	SeedType aSeed2 = theBoardState->mSeedType[theGridX + 1][theGridY];
	SeedType aSeed3 = theBoardState->mSeedType[theGridX][theGridY + 1];
	SeedType aSeed4 = theBoardState->mSeedType[theGridX + 1][theGridY + 1];

	theBoardState->mSeedType[theGridX + 1][theGridY] = aSeed1;
	theBoardState->mSeedType[theGridX + 1][theGridY + 1] = aSeed2;
	theBoardState->mSeedType[theGridX][theGridY] = aSeed3;
	theBoardState->mSeedType[theGridX][theGridY + 1] = aSeed4;

	int aHasMatch = BeghouledBoardHasMatch(theBoardState);

	theBoardState->mSeedType[theGridX][theGridY] = aSeed1;
	theBoardState->mSeedType[theGridX + 1][theGridY] = aSeed2;
	theBoardState->mSeedType[theGridX][theGridY + 1] = aSeed3;
	theBoardState->mSeedType[theGridX + 1][theGridY + 1] = aSeed4;

	return aHasMatch;
}

//0x420220
int Challenge::BeghouledTwistSquareFromMouse(int theX, int theY, int& theGridX, int& theGridY)
{
	theGridX = mBoard->PixelToGridX(theX - 40, theY - 40);
	theGridY = mBoard->PixelToGridY(theX - 40, theY - 40);
	if (theGridX == -1 || theGridY == -1 || theGridX > 6 || theGridY > 3)
	{
		theGridX = -1;
		theGridY = -1;
		return false;
	}
	return true;
}

//0x420280
void Challenge::BeghouledTwistMouseDown(int x, int y)
{
	if (mBoard->HasLevelAwardDropped())
		return;

	BeghouledBoardState aBoardState;
	LoadBeghouledBoardState(&aBoardState);

	int aGridX, aGridY;
	if (BeghouledTwistSquareFromMouse(x, y, aGridX, aGridY) && BeghouledTwistValidMove(aGridX, aGridY, &aBoardState))
	{
		Plant* aPlant1 = mBoard->GetTopPlantAt(aGridX, aGridY, TOPPLANT_ANY);
		Plant* aPlant2 = mBoard->GetTopPlantAt(aGridX + 1, aGridY, TOPPLANT_ANY);
		Plant* aPlant3 = mBoard->GetTopPlantAt(aGridX, aGridY + 1, TOPPLANT_ANY);
		Plant* aPlant4 = mBoard->GetTopPlantAt(aGridX + 1, aGridY + 1, TOPPLANT_ANY);
		if (!BeghouledTwistMoveCausesMatch(aGridX, aGridY, &aBoardState))
		{
			{ aPlant1->mX = (mBoard->GridToPixelX(aPlant1->mPlantCol, aPlant1->mRow) + 20); aPlant1->mRootBindingGeneration = 0; }
			aPlant2->mY = (mBoard->GridToPixelY(aPlant2->mPlantCol, aPlant2->mRow) + 20);
			aPlant3->mY = (mBoard->GridToPixelY(aPlant3->mPlantCol, aPlant3->mRow) - 20);
			{ aPlant4->mX = (mBoard->GridToPixelX(aPlant4->mPlantCol, aPlant4->mRow) - 20); aPlant4->mRootBindingGeneration = 0; }
			mApp->PlayFoley(FOLEY_FLOOP);
		}
		else
		{
			// 格桶同步（2026-10-04）：twist 旋转四连移直写坐标——普通拖拽
			// 交换已挂桶，此 twist 变体漏挂（同 FallIntoSquare 病）
			const int anOldCol1 = aPlant1->mPlantCol;
			aPlant1->mPlantCol++;
			aPlant1->mRenderOrder = aPlant1->CalcRenderOrder();
			mBoard->MovePlantCellBucket(aPlant1, aPlant1->mRow, anOldCol1);
			const int anOldRow2 = aPlant2->mRow;
			aPlant2->mRow++;
			aPlant2->mRenderOrder = aPlant2->CalcRenderOrder();
			mBoard->MovePlantCellBucket(aPlant2, anOldRow2, aPlant2->mPlantCol);
			const int anOldRow3 = aPlant3->mRow;
			aPlant3->mRow--;
			aPlant3->mRenderOrder = aPlant3->CalcRenderOrder();
			mBoard->MovePlantCellBucket(aPlant3, anOldRow3, aPlant3->mPlantCol);
			const int anOldCol4 = aPlant4->mPlantCol;
			aPlant4->mPlantCol--;
			aPlant4->mRenderOrder = aPlant4->CalcRenderOrder();
			mBoard->MovePlantCellBucket(aPlant4, aPlant4->mRow, anOldCol4);
			BeghouledStartFalling(STATECHALLENGE_BEGHOULED_MOVING);
		}
	}
}

//0x420670
void Challenge::BeghouledStartFalling(ChallengeState theState)
{
	mChallengeState = theState;
	mChallengeStateCounter = 100;
	BeghouledCancelMatchFlashing();
	mBoard->ClearAdvice(AdviceType::ADVICE_BEGHOULED_NO_MOVES);
}

//0x4206E0
int Challenge::BeghouledIsValidMove(int theFromX, int theFromY, int theToX, int theToY, BeghouledBoardState* theBoardState)
{
	if (theFromX < 0 || theFromX > BEGHOULED_MAX_GRIDSIZEX || theToX < 0 || theToX > BEGHOULED_MAX_GRIDSIZEX ||
		theFromY < 0 || theFromY > BEGHOULED_MAX_GRIDSIZEY || theToY < 0 || theToY > BEGHOULED_MAX_GRIDSIZEY ||
		mBeghouledEated[theFromX][theFromY] || mBeghouledEated[theToX][theToY])
		return false;

	SeedType aSeedFrom = theBoardState->mSeedType[theFromX][theFromY];
	SeedType aSeedTo = theBoardState->mSeedType[theToX][theToY];
	if (aSeedFrom == SEED_NONE)
		return false;

	theBoardState->mSeedType[theFromX][theFromY] = aSeedTo;
	theBoardState->mSeedType[theToX][theToY] = aSeedFrom;

	int aValid = BeghouledBoardHasMatch(theBoardState);

	theBoardState->mSeedType[theFromX][theFromY] = aSeedFrom;
	theBoardState->mSeedType[theToX][theToY] = aSeedTo;

	return aValid;
}

//0x420760
void Challenge::BeghouledDragUpdate(int x, int y)
{
	int aDeltaX = x - mBeghouledMouseDownX;
	int aDeltaY = y - mBeghouledMouseDownY;
	if (abs(aDeltaX) >= 10 || abs(aDeltaY) >= 10)
	{
		mBoard->ClearAdvice(ADVICE_BEGHOULED_DRAG_TO_MATCH_3);
		mBeghouledMouseCapture = false;

		int aGridXFrom = mBoard->PixelToGridX(mBeghouledMouseDownX, mBeghouledMouseDownY);
		int aGridYFrom = mBoard->PixelToGridY(mBeghouledMouseDownX, mBeghouledMouseDownY);
		int aGridXTo, aGridYTo;
		if (abs(aDeltaX) > abs(aDeltaY))
		{
			aGridXTo = aGridXFrom + (aDeltaX > 0 ? 1 : -1);
			aGridYTo = aGridYFrom;
		}
		else
		{
			aGridXTo = aGridXFrom;
			aGridYTo = aGridYFrom + (aDeltaY > 0 ? 1 : -1);
		}

		BeghouledBoardState aBoardState;
		LoadBeghouledBoardState(&aBoardState);
		Plant* aPlantFrom = mBoard->GetTopPlantAt(aGridXFrom, aGridYFrom, TOPPLANT_ANY);
		if (!BeghouledIsValidMove(aGridXFrom, aGridYFrom, aGridXTo, aGridYTo, &aBoardState))
		{
			if (aPlantFrom)
			{
				if (aGridXTo > aGridXFrom)			{ aPlantFrom->mX += (30); aPlantFrom->mRootBindingGeneration = 0; }
				else if (aGridXTo < aGridXFrom)		{ aPlantFrom->mX -= (30); aPlantFrom->mRootBindingGeneration = 0; }
				else if (aGridYTo > aGridYFrom)		aPlantFrom->mY += (30);
				else if (aGridYTo < aGridYFrom)		aPlantFrom->mY -= (30);

				mApp->PlayFoley(FOLEY_FLOOP);
			}
		}
		else
		{
			Plant* aPlantTo = mBoard->GetTopPlantAt(aGridXTo, aGridYTo, TOPPLANT_ANY);

			if (aPlantFrom)
			{
				const int anOldRow = aPlantFrom->mRow, anOldCol = aPlantFrom->mPlantCol;
				aPlantFrom->mPlantCol = aGridXTo;
				aPlantFrom->mRow = aGridYTo;
				aPlantFrom->mRenderOrder = aPlantFrom->CalcRenderOrder();
				mBoard->MovePlantCellBucket(aPlantFrom, anOldRow, anOldCol);
			}

			if (aPlantTo)
			{
				const int anOldRow = aPlantTo->mRow, anOldCol = aPlantTo->mPlantCol;
				aPlantTo->mPlantCol = aGridXFrom;
				aPlantTo->mRow = aGridYFrom;
				aPlantTo->mRenderOrder = aPlantTo->CalcRenderOrder();
				mBoard->MovePlantCellBucket(aPlantTo, anOldRow, anOldCol);
			}

			BeghouledStartFalling(STATECHALLENGE_BEGHOULED_MOVING);
		}
	}
}

SeedType Challenge::BeghouledGetPlantAt(int theGridX, int theGridY, BeghouledBoardState* theBoardState)
{
	if (theGridX < 0 || theGridX > BEGHOULED_MAX_GRIDSIZEX || theGridY < 0 || theGridY > BEGHOULED_MAX_GRIDSIZEY)
		return SEED_NONE;

	return theBoardState->mSeedType[theGridX][theGridY];
}

//0x420A50
void Challenge::BeghouledRemoveHorizontalMatch(int theGridX, int theGridY, BeghouledBoardState* theBoardState)
{
	SeedType aSeedType = BeghouledGetPlantAt(theGridX, theGridY, theBoardState);
	do
	{
		Plant* aPlant = mBoard->GetTopPlantAt(theGridX, theGridY, TOPPLANT_ANY);
		if (aPlant)
		{
			aPlant->Die(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));

		}
		theGridX++;
	} while (BeghouledGetPlantAt(theGridX, theGridY, theBoardState) == aSeedType);
}

//0x420B60
void Challenge::BeghouledRemoveVerticalMatch(int theGridX, int theGridY, BeghouledBoardState* theBoardState)
{
	SeedType aSeedType = BeghouledGetPlantAt(theGridX, theGridY, theBoardState);
	do
	{
		Plant* aPlant = mBoard->GetTopPlantAt(theGridX, theGridY, TOPPLANT_ANY);
		if (aPlant)
		{
			aPlant->Die(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
		}
		theGridY++;
	} while (BeghouledGetPlantAt(theGridX, theGridY, theBoardState) == aSeedType);
}

//0x420C60
void Challenge::BeghouledFallIntoSquare(int theGridX, int theGridY, BeghouledBoardState* theBoardState)
{
	if (mBeghouledEated[theGridX][theGridY])
		return;

	for (int aGridY = theGridY - 1; aGridY >= 0; aGridY--)
	{
		Plant* aPlant = mBoard->GetTopPlantAt(theGridX, aGridY, TOPPLANT_ANY);
		if (aPlant)
		{
			// 格桶同步（2026-10-04 B 树实测抓获：下落直写 mRow 不过桶→桶失步
			// →GetTopPlantAt 全瞎→消除 Die 错对象→匹配永存→卡死。格桶落地时
			// 本站漏挂——拖拽交换/禅园两站已挂，唯此站漏）
			const int anOldRow = aPlant->mRow;
			aPlant->mRow = theGridY;
			aPlant->mRenderOrder = aPlant->CalcRenderOrder();
			mBoard->MovePlantCellBucket(aPlant, anOldRow, aPlant->mPlantCol);
			theBoardState->mSeedType[theGridX][theGridY] = aPlant->mSeedType;
			theBoardState->mSeedType[theGridX][aGridY] = SEED_NONE;
			BeghouledStartFalling(STATECHALLENGE_BEGHOULED_FALLING);
			break;
		}
	}
}

//0x420D50
void Challenge::BeghouledMakePlantsFall(BeghouledBoardState* theBoardState)
{
	for (int aGridY = BEGHOULED_MAX_GRIDSIZEY - 1; aGridY >= 0; aGridY--)
	{
		for (int aGridX = 0; aGridX < BEGHOULED_MAX_GRIDSIZEX; aGridX++)
		{
			if (BeghouledGetPlantAt(aGridX, aGridY, theBoardState) == SEED_NONE)
			{
				BeghouledFallIntoSquare(aGridX, aGridY, theBoardState);
			}
		}
	}
}

//0x420DB0
void Challenge::BeghouledUpdateCraters()
{
	if (mBoard->mSeedBank->mNumPackets != 5)
		return;

	SeedPacket* aSeedPacket = &mBoard->mSeedBank->mSeedPackets[4];
	TOD_ASSERT(aSeedPacket->mPacketType == SeedType::SEED_BEGHOULED_BUTTON_CRATER);
	aSeedPacket->SetActivate(BeghouledCanClearCrater());
}

//0x420E10
void Challenge::BeghouledClearCrater(int theCount)
{
	mBoard->ClearAdvice(ADVICE_BEGHOULED_USE_CRATER_1);
	mBoard->ClearAdvice(ADVICE_BEGHOULED_USE_CRATER_2);

	for (int aGridX = 0; aGridX < MAX_GRID_SIZE_X; aGridX++)
	{
		for (int aGridY = 0; aGridY < BEGHOULED_MAX_GRIDSIZEY; aGridY++)
		{
			if (mBeghouledEated[aGridX][aGridY])
			{
				mBeghouledEated[aGridX][aGridY] = false;
				if (--theCount == 0)
				{
					BeghouledUpdateCraters();
					return;
				}
			}
		}
	}
}

//0x420EF0
void Challenge::BeghouledScore(int theGridX, int theGridY, int theNumPlants, int theIsHorizontal)
{
	mApp->PlayFoley(FOLEY_ART_CHALLENGE);

	float aPosX = mBoard->GridToPixelX(theGridX, theGridY);
	float aPosY = mBoard->GridToPixelY(theGridX, theGridY);
	if (theIsHorizontal)
	{
		aPosX += theNumPlants == 3 ? 80.0f : theNumPlants == 4 ? 120.0f : 160.0f;
	}
	else
	{
		aPosY += theNumPlants == 3 ? 80.0f : theNumPlants == 4 ? 120.0f : 160.0f;
	}

	mChallengeScore++;
	if (mBoard->mSeedBank->mNumPackets == 0)
	{
		mBoard->mSeedBank->mSeedPackets[0].SetPacketType(SEED_REPEATER, SEED_NONE);
		mBoard->mSeedBank->mSeedPackets[1].SetPacketType(SEED_FUMESHROOM, SEED_NONE);
		mBoard->mSeedBank->mSeedPackets[2].SetPacketType(SEED_TALLNUT, SEED_NONE);
		mBoard->mSeedBank->mSeedPackets[3].SetPacketType(SEED_BEGHOULED_BUTTON_SHUFFLE, SEED_NONE);
		mBoard->mSeedBank->mNumPackets = 4;

		mBoard->DisplayAdvice(gStr.ADVICE_BEGHOULED_SAVE_SUN, MESSAGE_STYLE_HINT_FAST, ADVICE_BEGHOULED_SAVE_SUN);

		if (BeghouledCanClearCrater())
		{
			mBoard->mSeedBank->mSeedPackets[4].SetPacketType(SEED_BEGHOULED_BUTTON_CRATER, SEED_NONE);
			mBoard->mSeedBank->mNumPackets = 5;
		}
	}
	else
	{
		if (!mBoard->mAdvice->IsBeingDisplayed())
		{
			SexyString aMsg = TodReplaceNumberString(gStr.ADVICE_BEGHOULED_MATCH_3, UiTextPlaceholder::Score, BEGHOULED_WINNING_SCORE);
			mBoard->DisplayAdvice(aMsg, MESSAGE_STYLE_HINT_FAST, ADVICE_BEGHOULED_MATCH_3);
		}
		if (mChallengeScore >= BEGHOULED_WINNING_SCORE - 5)
		{
			mBoard->DisplayAdvice(gStr.ADVICE_BEGHOULED_ALMOST_THERE, MESSAGE_STYLE_HINT_FAST, ADVICE_ALMOST_THERE);
		}
	}

	if (mChallengeScore >= BEGHOULED_WINNING_SCORE)
	{
		mChallengeScore = BEGHOULED_WINNING_SCORE;
		SpawnLevelAward(theGridX, theGridY);
		mBoard->ClearAdvice(ADVICE_NONE);
	}
	else
	{
		int aNumSuns = theNumPlants - 2 + mBeghouledMatchesThisMove;
		if (theNumPlants >= 5)
		{
			aNumSuns += 2;
		}
		aNumSuns = ClampInt(aNumSuns, 1, 5);

		for (int i = 0; i < aNumSuns; i++)
		{
			mBoard->AddCoin(aPosX + 20 * i - 10, aPosY, COIN_SUN, COIN_MOTION_COIN);
		}
	}

	mBeghouledMatchesThisMove++;
}

//0x421430
void Challenge::BeghouledRemoveMatches(BeghouledBoardState* theBoardState)
{
	for (int aGridY = 0; aGridY < BEGHOULED_MAX_GRIDSIZEY; aGridY++)
	{
		for (int aGridX = 0; aGridX < BEGHOULED_MAX_GRIDSIZEX; aGridX++)
		{
			int aHorMatchLength = BeghouledHorizontalMatchLength(aGridX, aGridY, theBoardState);
			if (aHorMatchLength >= 3)
			{
				BeghouledRemoveHorizontalMatch(aGridX, aGridY, theBoardState);
				BeghouledScore(aGridX, aGridY, aHorMatchLength, true);
			}

			int aVerMatchLength = BeghouledVerticalMatchLength(aGridX, aGridY, theBoardState);
			if (aVerMatchLength >= 3)
			{
				BeghouledRemoveVerticalMatch(aGridX, aGridY, theBoardState);
				BeghouledScore(aGridX, aGridY, aVerMatchLength, false);
			}
		}
	}
}

//0x4214C0
int Challenge::BeghouledHorizontalMatchLength(int theGridX, int theGridY, BeghouledBoardState* theBoardState)
{
	SeedType aSeedType = BeghouledGetPlantAt(theGridX, theGridY, theBoardState);
	if (aSeedType == SEED_NONE || BeghouledGetPlantAt(theGridX - 1, theGridY, theBoardState) == aSeedType)
		return 0;

	int aLength = 1;
	while (BeghouledGetPlantAt(theGridX + aLength, theGridY, theBoardState) == aSeedType)
		aLength++;
	return aLength;
}

//0x421520
int Challenge::BeghouledVerticalMatchLength(int theGridX, int theGridY, BeghouledBoardState* theBoardState)
{
	SeedType aSeedType = BeghouledGetPlantAt(theGridX, theGridY, theBoardState);
	if (aSeedType == SEED_NONE || BeghouledGetPlantAt(theGridX, theGridY - 1, theBoardState) == aSeedType)
		return 0;

	int aLength = 1;
	while (BeghouledGetPlantAt(theGridX, theGridY + aLength, theBoardState) == aSeedType)
		aLength++;
	return aLength;
}

//0x421590
int Challenge::BeghouledBoardHasMatch(BeghouledBoardState* theBoardState)
{
	for (int aCol = 0; aCol < 8; aCol++)
	{
		for (int aRow = 0; aRow < 5; aRow++)
		{
			if (BeghouledHorizontalMatchLength(aCol, aRow, theBoardState) >= 3 ||
				BeghouledVerticalMatchLength(aCol, aRow, theBoardState) >= 3)
				return true;
		}
	}
	return false;
}

namespace
{
	// BeghouledPickSeed 候选种子表：原六 case 同构常量赋值 -> 数据表（行序 = i
	// 槽位 0..5）；default TOD_ASSERT 语义由分发点域断言保持。
	constexpr SeedType kBeghouledSeedChoices[6] = {
		SeedType::SEED_PUFFSHROOM,   // 0
		SeedType::SEED_STARFRUIT,    // 1
		SeedType::SEED_MAGNETSHROOM, // 2
		SeedType::SEED_SNOWPEA,      // 3
		SeedType::SEED_WALLNUT,      // 4
		SeedType::SEED_PEASHOOTER,   // 5
	};
	static_assert(sizeof(kBeghouledSeedChoices) / sizeof(kBeghouledSeedChoices[0]) == 6,
		"kBeghouledSeedChoices 槽位须与 Beghouled 候选数同步");
}

//0x4215E0
SeedType Challenge::BeghouledPickSeed(int theGridX, int theGridY, BeghouledBoardState* theBoardState, int theAllowMatches)
{
	TOD_ASSERT(theBoardState->mSeedType[theGridX][theGridY] == SEED_NONE);
	// SeedType* aSeedState = &theBoardState->mSeedType[theGridX][theGridY];
	int aCount = 0;
	SeedType aPickArray[6];

	for (int i = 0; i < 6; i++)
	{
		// B 形态 -> 常量表（上方 kBeghouledSeedChoices）：i 循环域 0..5 直下标；
		// 原 default TOD_ASSERT 语义由域断言保持。
		TOD_ASSERT((unsigned)i < 6);
		SeedType aSeedType = kBeghouledSeedChoices[i];

		if (mBeghouledPurcasedUpgrade[(int)BeghouledUpgrade::BEGHOULED_UPGRADE_REPEATER] && aSeedType == SeedType::SEED_PEASHOOTER)
		{
			aSeedType = SeedType::SEED_REPEATER;
		}
		if (mBeghouledPurcasedUpgrade[(int)BeghouledUpgrade::BEGHOULED_UPGRADE_FUMESHROOM] && aSeedType == SeedType::SEED_PUFFSHROOM)
		{
			aSeedType = SeedType::SEED_FUMESHROOM;
		}
		if (mBeghouledPurcasedUpgrade[(int)BeghouledUpgrade::BEGHOULED_UPGRADE_TALLNUT] && aSeedType == SeedType::SEED_WALLNUT)
		{
			aSeedType = SeedType::SEED_TALLNUT;
		}

		theBoardState->mSeedType[theGridX][theGridY] = aSeedType;

		if (theAllowMatches || !BeghouledBoardHasMatch(theBoardState))
		{
			aPickArray[aCount] = aSeedType;
			aCount++;
		}
	}

	theBoardState->mSeedType[theGridX][theGridY] = SEED_NONE;
	return TodPickFromArray(aPickArray, aCount);
}

//0x4216E0
void Challenge::BeghouledFillHoles(BeghouledBoardState* theBoardState, int theAllowMatches)
{
	for (int aCol = 0; aCol < BEGHOULED_MAX_GRIDSIZEX; aCol++)
	{
		for (int aRow = 0; aRow < BEGHOULED_MAX_GRIDSIZEY; aRow++)
		{
			if (theBoardState->mSeedType[aCol][aRow] == SeedType::SEED_NONE && !mBeghouledEated[aCol][aRow])
			{
				theBoardState->mSeedType[aCol][aRow] = BeghouledPickSeed(aCol, aRow, theBoardState, theAllowMatches);
			}
		}
	}
}

//0x421750
void Challenge::BeghouledCreatePlants(BeghouledBoardState* theOldBoardState, BeghouledBoardState* theNewBoardState)
{
	for (int aCol = 0; aCol < BEGHOULED_MAX_GRIDSIZEX; aCol++)
	{
		int aFallY = 80;
		for (int aRow = BEGHOULED_MAX_GRIDSIZEY - 1; aRow >= 0; aRow--)
		{
			SeedType aSeedType = theNewBoardState->mSeedType[aCol][aRow];
			if (theOldBoardState->mSeedType[aCol][aRow] == SEED_NONE && aSeedType != SEED_NONE)
			{
				aFallY -= 100;
				mBoard->NewPlant(aCol, aRow, aSeedType, SEED_NONE)->mY = aFallY;
				BeghouledStartFalling(STATECHALLENGE_BEGHOULED_FALLING);
			}
		}
	}
}

void Challenge::BeghouledMakeStartBoard()
{
	BeghouledBoardState aEmptyBoardState;
	LoadBeghouledBoardState(&aEmptyBoardState);
	BeghouledBoardState aBoardState;
	LoadBeghouledBoardState(&aBoardState);

	BeghouledFillHoles(&aBoardState, false);
	TOD_ASSERT(!BeghouledBoardHasMatch(&aBoardState));

	if (!BeghouledBoardHasMatch(&aBoardState))
	{
		BeghouledCreatePlants(&aEmptyBoardState, &aBoardState);
	}
}

//0x421810
void Challenge::BeghouledPopulateBoard()
{
	BeghouledBoardState aEmptyBoardState, aBoardState;
	LoadBeghouledBoardState(&aEmptyBoardState);
	int aAllowGeneratedCascades = BeghouledBoardHasMatch(&aEmptyBoardState);

	for (int i = 0; i < 2; i++)
	{
		LoadBeghouledBoardState(&aBoardState);
		BeghouledFillHoles(&aBoardState, aAllowGeneratedCascades);
		// 填充后，若不存在可消除的移动，则重新填充 1 次
		if (BeghouledCheckForPossibleMoves(&aBoardState))
			break;
	}

	// 根据填充的结果开始执行创建植物
	BeghouledCreatePlants(&aEmptyBoardState, &aBoardState);
}

//0x421890
int Challenge::BeghouledCheckForPossibleMoves(BeghouledBoardState* theBoardState)
{
	GameMode aGameMode = mApp->mGameMode;

	for (int aRow = 0; aRow < BEGHOULED_MAX_GRIDSIZEY; aRow++)
	{
		for (int aCol = 0; aCol < BEGHOULED_MAX_GRIDSIZEX; aCol++)
		{
			if (aGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED)
			{
				if (BeghouledIsValidMove(aCol, aRow, aCol + 1, aRow, theBoardState) || BeghouledIsValidMove(aCol, aRow, aCol, aRow + 1, theBoardState))
				{
					return true;
				}
			}
			else if (aGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST)
			{
				if (BeghouledTwistMoveCausesMatch(aCol, aRow, theBoardState))
				{
					return true;
				}
			}
			else
			{
				return false;
			}
		}
	}

	return false;
}

void Challenge::BeghouledDragStart(int x, int y)
{
	if (!mBoard->HasLevelAwardDropped())
	{
		mBeghouledMouseCapture = true;
		mBeghouledMouseDownX = x;
		mBeghouledMouseDownY = y;
	}
}

//0x421920
int Challenge::MouseMove(int x, int y)
{
	if (mApp->mGameMode == GAMEMODE_MINIGAME_BEGHOULED && !mBoard->HasLevelAwardDropped())
	{
		if (mBeghouledMouseCapture)
		{
			BeghouledDragUpdate(x, y);
			return true;
		}

		HitResult aHitResult;
		mBoard->MouseHitTest(x, y, &aHitResult);
		if (aHitResult.mObjectType == OBJECT_TYPE_PLANT)
			return true;
	}

	if (mApp->mGameMode == GAMEMODE_LIMBO_ZEN_GARDEN && mApp->mActive)
	{
		if (mChallengeState == STATECHALLENGE_ZEN_FADING)
			mChallengeState = STATECHALLENGE_NORMAL;
		mChallengeStateCounter = 3000;
	}
	return false;
}

//0x4219B0
int Challenge::UpdateToolTip(int theX, int theY)
{
	if (!mApp->IsSlotMachineLevel())
		return false;

	HitResult aHitResult;
	mBoard->MouseHitTest(theX, theY, &aHitResult);
	if (aHitResult.mObjectType != GameObjectType::OBJECT_TYPE_SLOT_MACHINE_HANDLE ||
		mBoard->mCursorObject->mCursorType != CursorType::CURSOR_TYPE_NORMAL ||
		mChallengeState != ChallengeState::STATECHALLENGE_NORMAL)
		return false;

	if (!mBoard->CanTakeSunMoney(25))
	{
		mBoard->mToolTip->SetWarningText(gStr.NOT_ENOUGH_SUN);
	}

	Rect aSlotMachineHandleRect = SlotMachineGetHandleRect();
	mBoard->mToolTip->SetLabel(gStr.SLOT_MACHINE_PULL_TOOLTIP);
	mBoard->mToolTip->mX = aSlotMachineHandleRect.mX + 15;
	mBoard->mToolTip->mY = aSlotMachineHandleRect.mY + 65;
	mBoard->mToolTip->mVisible = true;
	mBoard->mToolTip->mCenter = true;
	return true;
}

//0x421B10
void Challenge::MouseDownWhackAZombie(int theX, int theY)
{
	mApp->ReanimationTryToGet(mBoard->mCursorObject->mReanimCursorID)->Seek(200);
	mApp->PlayFoley(FOLEY_SWING);

	Zombie* aZombie = nullptr;
	Zombie* aTopZombie = nullptr;
	while (mBoard->IterateZombies(aZombie))
	{
		if (!aZombie->IsDeadOrDying())
		{
			Rect aZombieRect = aZombie->GetZombieRect();
			if (GetCircleRectOverlap(theX, theY - 20, 45, aZombieRect))
			{
				if (aTopZombie == nullptr || aZombie->mRenderOrder >= aTopZombie->mRenderOrder)
				{
					aTopZombie = aZombie;
				}
			}
		}
	}

	if (aTopZombie)
	{
		if (aTopZombie->mHelmType != HELMTYPE_NONE)
		{
			if (aTopZombie->mHelmType == HELMTYPE_PAIL)
			{
				mApp->PlayFoley(FOLEY_SHIELD_HIT);
			}
			else if (aTopZombie->mHelmType == HELMTYPE_TRAFFIC_CONE)
			{
				mApp->PlayFoley(FOLEY_PLASTIC_HIT);
			}

			aTopZombie->TakeHelmDamage(static_cast<std::int64_t>(mBoard->mLevelTimeNs), 900, 0U);
		}
		else
		{
			mApp->PlayFoley(FOLEY_BONK);
			mApp->AddTodParticle(theX - 3, theY + 9, RENDER_LAYER_ABOVE_UI, PARTICLE_POW, static_cast<std::int64_t>(mBoard->mLevelTimeNs));
			aTopZombie->DieWithLoot(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
			mBoard->ClearCursor();
		}
	}
}

//0x421E10
void Challenge::AdvanceCrazyDaveDialog()
{
	if (!mBoard->IsScaryPotterDaveTalking() || mApp->mCrazyDaveMessageIndex == -1)
		return;

	if (!mApp->AdvanceCrazyDaveText())
	{
		mApp->CrazyDaveLeave();
		return;
	}

	// “来，我会给你更多花瓶的。” || “这应该他们最后一波了。”
	if (mApp->mCrazyDaveMessageIndex == 2702 || mApp->mCrazyDaveMessageIndex == 2801)
	{
		ScaryPotterPopulate();
		mApp->PlayFoley(FOLEY_PLANT);
		mBoard->PlaceRake();
	}
}

//0x421F10
int Challenge::MouseDown(int x, int y, int theClickCount, HitResult* theHitResult)
{
	if (mApp->mGameMode == GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		return mApp->mZenGarden->MouseDownZenGarden(x, y, theClickCount, theHitResult);
	}

	if (mApp->mGameScene != SCENE_PLAYING)
		return false;

	if (mBoard->IsScaryPotterDaveTalking() && mApp->mCrazyDaveMessageIndex != -1)
	{
		AdvanceCrazyDaveDialog();
		return true;
	}

	if (theHitResult->mObjectType == OBJECT_TYPE_COIN && theClickCount >= 0)
		return false;

	if (mApp->mGameMode == GAMEMODE_MINIGAME_BEGHOULED)
	{
		if (mChallengeState != STATECHALLENGE_NORMAL)
			return false;

		if (theHitResult->mObjectType == OBJECT_TYPE_PLANT)
		{
			BeghouledDragStart(x, y);
			return true;
		}
	}

	if (mApp->mGameMode == GAMEMODE_MINIGAME_BEGHOULED_TWIST)
	{
		if (mChallengeState != STATECHALLENGE_NORMAL)
			return false;

		BeghouledTwistMouseDown(x, y);
	}

	if (mApp->IsSlotMachineLevel() && theHitResult->mObjectType == OBJECT_TYPE_SLOT_MACHINE_HANDLE &&
		mBoard->mCursorObject->mCursorType == CURSOR_TYPE_NORMAL && mChallengeState == STATECHALLENGE_NORMAL)
	{
		if (mBoard->TakeSunMoney(25))
		{
			for (int i = 0; i < 3; i++)
			{
				mBoard->mSeedBank->mSeedPackets[i].SlotMachineStart();
			}

			mApp->ReanimationGet(mReanimChallenge)->PlayReanim(ReanimTrackId::anim_pull, REANIM_PLAY_ONCE_AND_HOLD, 0, 36.0f);
			mChallengeState = STATECHALLENGE_SLOT_MACHINE_ROLLING;
			mBoard->SetTutorialState(TUTORIAL_SLOT_MACHINE_COMPLETED);
			mBoard->ClearAdvice(ADVICE_NONE);

			mSlotMachineRollCount++;
			mApp->PlaySample(Sexy::SOUND_SLOT_MACHINE);
		}

		return true;
	}

	if (mApp->IsWhackAZombieLevel() && theHitResult->mObjectType == OBJECT_TYPE_NONE &&
		mBoard->mCursorObject->mCursorType == CURSOR_TYPE_HAMMER && theClickCount >= 0)
	{
		MouseDownWhackAZombie(x, y);
		return true;
	}

	if (mApp->mGameMode == GAMEMODE_MINIGAME_ZOMBIQUARIUM)
	{
		// 1073 00424C2E: right click only taps glass; left click on empty
		// aquarium space reaches the paid feeding path (00424C5C).
		if (theClickCount < 0)
		{
			mApp->PlaySample(Sexy::SOUND_TAPGLASS);
			return true;
		}
		if (theHitResult->mObjectType == OBJECT_TYPE_NONE)
		{
			ZombiquariumMouseDown(x, y);
			return true;
		}
	}

	if (mApp->IsScaryPotterLevel() && theHitResult->mObjectType == OBJECT_TYPE_SCARY_POT)
	{
		ScaryPotterMalletPot((GridItem*)theHitResult->mObject);
		return true;
	}

	return false;
}

//0x4221B0
Rect Challenge::SlotMachineGetHandleRect()
{
	// MouseHitTest takes Board-local coordinates; the handle belongs to the
	// viewport's left HUD, so invert the camera only for this HUD rectangle.
	return Rect(mApp->mScreenBounds.mX - mBoard->mX + mBoard->mSeedBank->mX + 473,
		mApp->mScreenBounds.mY - mBoard->mY + mBoard->mSeedBank->mY, 55, 80);
}

//0x4221E0
int Challenge::MouseUp(int x, int y)
{
	(void)x;(void)y;
	if (mApp->mGameMode == GAMEMODE_MINIGAME_BEGHOULED)
	{
		if (mBeghouledMouseCapture && !mBoard->mAdvice->IsBeingDisplayed() && mChallengeScore == 0)
		{
			mBoard->DisplayAdvice(gStr.ADVICE_BEGHOULED_DRAG_TO_MATCH_3, MESSAGE_STYLE_HINT_FAST, ADVICE_BEGHOULED_DRAG_TO_MATCH_3);
		}

		mBeghouledMouseCapture = false;
	}

	return false;
}

//0x422290
void Challenge::ClearCursor()
{
	if (mApp->mGameMode == GAMEMODE_MINIGAME_BEGHOULED)
	{
		mBeghouledMouseCapture = false;
	}

	if (mApp->IsWhackAZombieLevel() && !mBoard->HasLevelAwardDropped())
	{
		mBoard->mCursorObject->mCursorType = CURSOR_TYPE_HAMMER;
	}
}

//0x4222F0
int Challenge::UpdateBeghouledPlant(Plant* thePlant)
{
	int aMoving = false;
	const float aDiffX = mBoard->GridToPixelX(thePlant->mPlantCol, thePlant->mRow) - thePlant->mX;
	const float aDiffY = mBoard->GridToPixelY(thePlant->mPlantCol, thePlant->mRow) - thePlant->mY;
	// Speeds are pixels per second; every update contributes its actual duration.
	const float aSpeed = mChallengeState == STATECHALLENGE_BEGHOULED_MOVING ? 300.0f
		: TodAnimateCurveNs(900'000'000, 300'000'000, mChallengeStateCounter.mNanoseconds, 100.0f, 2000.0f, Curves::EaseIn);
	const float aDelta = aSpeed * (static_cast<float>(Sexy::gUpdateIntervalNs) * 1.0e-9f);
	float aDeltaX = 0, aDeltaY = 0;

	if (aDiffX > 0)
	{
		aDeltaX = std::min(aDelta, aDiffX);
		{ thePlant->mX += (aDeltaX); thePlant->mRootBindingGeneration = 0; }
		aMoving = true;
	}
	else if (aDiffX < 0)
	{
		aDeltaX = -std::min(aDelta, -aDiffX);
		{ thePlant->mX += (aDeltaX); thePlant->mRootBindingGeneration = 0; }
		aMoving = true;
	}
	if (aDiffY > 0)
	{
		aDeltaY = std::min(aDelta, aDiffY);
		thePlant->mY += (aDeltaY);
		aMoving = true;
	}
	else if (aDiffY < 0)
	{
		aDeltaY = -std::min(aDelta, -aDiffY);
		thePlant->mY += (aDeltaY);
		aMoving = true;
	}

	if (thePlant->mState == STATE_MAGNETSHROOM_CHARGING || thePlant->mState == STATE_MAGNETSHROOM_SUCKING)
	{
		thePlant->mMagnetItems[0].mPosX += aDeltaX;
		thePlant->mMagnetItems[0].mPosY += aDeltaY;
	}

	return aMoving;
}

//0x422480
void Challenge::BeghouledFlashPlant(int theFlashX, int theFlashY, int theFromX, int theFromY, int theToX, int theToY)
{
	if (theFlashX == theFromX && theFlashY == theFromY)
	{
		theFlashX = theToX;
		theFlashY = theToY;
	}
	else if (theFlashX == theToX && theFlashY == theToY)
	{
		theFlashX = theFromX;
		theFlashY = theFromY;
	}

	Plant* aFlashPlant = mBoard->GetTopPlantAt(theFlashX, theFlashY, PlantPriority::TOPPLANT_ONLY_NORMAL_POSITION);
	if (aFlashPlant && std::max<std::int64_t>(0, aFlashPlant->mBeghouledFlashEndTimeNs - static_cast<std::int64_t>(aFlashPlant->mBoard->mLevelTimeNs)) <= 3000000000LL)
		aFlashPlant->mBeghouledFlashEndTimeNs = static_cast<std::int64_t>(aFlashPlant->mBoard->mLevelTimeNs) + 3000000000LL;
}

//0x422510
int Challenge::BeghouledTwistFlashMatch(BeghouledBoardState* theBoardState, int theGridX, int theGridY)
{
	if (!BeghouledTwistMoveCausesMatch(theGridX, theGridY, theBoardState))
		return false;

	for (int i = 0; i < 4; i++)
	{
		Plant* aPlant = mBoard->GetTopPlantAt(theGridX + (i % 2), theGridY + (i / 2), PlantPriority::TOPPLANT_ANY);
		if (aPlant && std::max<std::int64_t>(0, aPlant->mBeghouledFlashEndTimeNs - static_cast<std::int64_t>(aPlant->mBoard->mLevelTimeNs)) <= 3000000000LL)
		{
			aPlant->mBeghouledFlashEndTimeNs = static_cast<std::int64_t>(aPlant->mBoard->mLevelTimeNs) + 3000000000LL;
		}
	}
	return true;
}

//0x422770
int Challenge::BeghouledFlashFromBoardState(BeghouledBoardState* theBoardState, int theFromX, int theFromY, int theToX, int theToY)
{
	TOD_ASSERT(theFromX >= 0 && theFromX < BEGHOULED_MAX_GRIDSIZEX && theFromY >= 0 && theFromY < BEGHOULED_MAX_GRIDSIZEY);
	TOD_ASSERT(theToX >= 0 && theToX < BEGHOULED_MAX_GRIDSIZEX && theToY >= 0 && theToY < BEGHOULED_MAX_GRIDSIZEY);

	if (mBeghouledEated[theFromX][theFromY] || mBeghouledEated[theToX][theToY])
		return false;

	SeedType aFromSeedType = theBoardState->mSeedType[theFromX][theFromY];
	SeedType aToSeedType = theBoardState->mSeedType[theToX][theToY];
	theBoardState->mSeedType[theFromX][theFromY] = aToSeedType;
	theBoardState->mSeedType[theToX][theToY] = aFromSeedType;

	int aHasMatch = false;
	for (int aRow = 0; aRow < BEGHOULED_MAX_GRIDSIZEY; aRow++)
	{
		for (int aCol = 0; aCol < BEGHOULED_MAX_GRIDSIZEX; aCol++)
		{
			if (BeghouledHorizontalMatchLength(aCol, aRow, theBoardState) >= 3)
			{
				for (int i = 0; i < 3; i++)
				{
					BeghouledFlashPlant(aCol + i, aRow, theFromX, theFromY, theToX, theToY);
				}
			}

			else if (BeghouledVerticalMatchLength(aCol, aRow, theBoardState) >= 3)
			{
				for (int i = 0; i < 3; i++)
				{
					BeghouledFlashPlant(aCol, aRow + i, theFromX, theFromY, theToX, theToY);
				}
			}
			else continue;

			aHasMatch = true;
			break;
		}
		// 1073 00425260 exits both scans after the first hinted triple.
		if (aHasMatch)
			break;
	}

	theBoardState->mSeedType[theFromX][theFromY] = aFromSeedType;
	theBoardState->mSeedType[theToX][theToY] = aToSeedType;
	return aHasMatch;
}

//0x4228E0
void Challenge::BeghouledCancelMatchFlashing()
{
	Plant* aPlant = nullptr;
	while (mBoard->IteratePlants(aPlant))
	{
		if (std::max<std::int64_t>(0, aPlant->mBeghouledFlashEndTimeNs - static_cast<std::int64_t>(aPlant->mBoard->mLevelTimeNs)) >= 250000000LL)
			aPlant->mBeghouledFlashEndTimeNs = static_cast<std::int64_t>(aPlant->mBoard->mLevelTimeNs) + 250000000LL;
	}
}

//0x422930
void Challenge::BeghouledFlashAMatch()
{
	BeghouledBoardState aBoardState;
	LoadBeghouledBoardState(&aBoardState);

	GameMode aGameMode = mApp->mGameMode;
	if (aGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED)
	{
		for (int aRow = 0; aRow <= BEGHOULED_MAX_GRIDSIZEY - 1; aRow++)
		{
			for (int aCol = 0; aCol <= BEGHOULED_MAX_GRIDSIZEX - 1; aCol++)
			{
				if ((aCol < BEGHOULED_MAX_GRIDSIZEX - 1 && BeghouledFlashFromBoardState(&aBoardState, aCol, aRow, aCol + 1, aRow)) ||
					(aRow < BEGHOULED_MAX_GRIDSIZEY - 1 && BeghouledFlashFromBoardState(&aBoardState, aCol, aRow, aCol, aRow + 1)))
					return;
			}
		}
	}
	else if (aGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST)
	{
		for (int aRow = 0; aRow <= BEGHOULED_MAX_GRIDSIZEY - 1; aRow++)
		{
			for (int aCol = 0; aCol <= BEGHOULED_MAX_GRIDSIZEX - 1; aCol++)
			{
				if (BeghouledTwistFlashMatch(&aBoardState, aCol, aRow))
					return;
			}
		}
	}
}

//0x4229F0
void Challenge::UpdateBeghouled()
{
	mBoard->mProgressMeterWidth = TodAnimateCurve(0, BEGHOULED_WINNING_SCORE, mChallengeScore, 0, PROGRESS_METER_COUNTER, Curves::Linear);

	int aMovingPlant = false;
	Plant* aPlant = nullptr;
	while (mBoard->IteratePlants(aPlant))
	{
		if (UpdateBeghouledPlant(aPlant))
		{
			aMovingPlant = true;
		}
	}

	if (mBoard->mSeedBank->mNumPackets > 4 && !mBoard->mAdvice->IsBeingDisplayed() && !mBoard->mHelpDisplayed[ADVICE_BEGHOULED_USE_CRATER_2])
	{
		int aCost = mBoard->GetCurrentPlantCost(SEED_BEGHOULED_BUTTON_CRATER, SEED_NONE);
		if (mBoard->CanTakeSunMoney(aCost) && BeghouledCanClearCrater() && !mBoard->HasLevelAwardDropped())
		{
			mBoard->DisplayAdvice(gStr.ADVICE_BEGHOULED_USE_CRATER_2, MESSAGE_STYLE_HINT_FAST, ADVICE_BEGHOULED_USE_CRATER_2);
		}
	}

	if (mApp->mGameMode == GAMEMODE_MINIGAME_BEGHOULED_TWIST && mChallengeState == STATECHALLENGE_NORMAL)
	{
		if (BeghouledTwistSquareFromMouse(mApp->mWidgetManager->mLastMouseX - mBoard->mX,
			mApp->mWidgetManager->mLastMouseY - mBoard->mY, mChallengeGridX, mChallengeGridY))
		{
			BeghouledBoardState aBoardState;
			LoadBeghouledBoardState(&aBoardState);
			if (!BeghouledTwistValidMove(mChallengeGridX, mChallengeGridY, &aBoardState))
			{
				mChallengeGridX = -1;
				mChallengeGridY = -1;
			}
		}
	}
	else
	{
		mChallengeGridX = -1;
		mChallengeGridY = -1;
	}

	if (!aMovingPlant && (mChallengeState == STATECHALLENGE_BEGHOULED_FALLING || mChallengeState == STATECHALLENGE_BEGHOULED_MOVING))
	{
		mChallengeState = STATECHALLENGE_NORMAL;
		mChallengeStateCounter = 1500;

		BeghouledBoardState aBoardState;
		LoadBeghouledBoardState(&aBoardState);
		BeghouledRemoveMatches(&aBoardState);
		LoadBeghouledBoardState(&aBoardState);
		BeghouledMakePlantsFall(&aBoardState);
		BeghouledPopulateBoard();

		if (mChallengeState == STATECHALLENGE_BEGHOULED_FALLING)
			return;

		mChallengeStateCounter = 1500;
		mBeghouledMatchesThisMove = 0;
		BeghouledCheckStuckState();
	}

	if (mChallengeStateCounter)
	{
		mChallengeStateCounter--;
		if (mChallengeStateCounter <= 0 && !mBoard->HasLevelAwardDropped())
		{
			if (mChallengeState == STATECHALLENGE_NORMAL)
			{
				BeghouledFlashAMatch();
				mChallengeStateCounter = 1500;
			}
			else if (mChallengeState == STATECHALLENGE_BEGHOULED_NO_MATCHES)
			{
				mApp->AddTodParticle(BOARD_WIDTH / 2, BOARD_HEIGHT / 2, RENDER_LAYER_TOP, PARTICLE_SCREEN_FLASH, static_cast<std::int64_t>(mBoard->mLevelTimeNs));
				BeghouledShuffle();
			}
		}
	}
}

namespace
{
	// 传送带出卡权重行（B 形态）：mSeed = 候选卡（写入 TodWeightedArray::mItem 域），mWeight = 权重。
	// 下方 14 组序列 = 原 UpdateConveyorBelt 14 支链体逐值迁移（行序 = 原数组下标序）。
	struct ConveyorSeedRow
	{
		SeedType mSeed;
		int mWeight;
	};
	struct ConveyorSeedSet
	{
		const ConveyorSeedRow* mRows;
		int mCount;
	};

	constexpr ConveyorSeedRow kConveyorRowsLevel10[] = {
		{ SeedType::SEED_PEASHOOTER, 20 }, { SeedType::SEED_CHERRYBOMB, 20 }, { SeedType::SEED_WALLNUT, 15 },
		{ SeedType::SEED_REPEATER, 20 }, { SeedType::SEED_SNOWPEA, 10 }, { SeedType::SEED_CHOMPER, 5 },
		{ SeedType::SEED_POTATOMINE, 10 },
	};
	constexpr ConveyorSeedRow kConveyorRowsLevel20[] = {
		{ SeedType::SEED_GRAVEBUSTER, 20 }, { SeedType::SEED_ICESHROOM, 15 }, { SeedType::SEED_DOOMSHROOM, 15 },
		{ SeedType::SEED_HYPNOSHROOM, 10 }, { SeedType::SEED_SCAREDYSHROOM, 15 }, { SeedType::SEED_FUMESHROOM, 15 },
		{ SeedType::SEED_PUFFSHROOM, 10 },
	};
	constexpr ConveyorSeedRow kConveyorRowsLevel30[] = {
		{ SeedType::SEED_LILYPAD, 25 }, { SeedType::SEED_SQUASH, 5 }, { SeedType::SEED_THREEPEATER, 25 },
		{ SeedType::SEED_TANGLEKELP, 5 }, { SeedType::SEED_JALAPENO, 10 }, { SeedType::SEED_SPIKEWEED, 10 },
		{ SeedType::SEED_TORCHWOOD, 10 }, { SeedType::SEED_TALLNUT, 10 },
	};
	constexpr ConveyorSeedRow kConveyorRowsLevel40[] = {
		{ SeedType::SEED_LILYPAD, 25 }, { SeedType::SEED_SEASHROOM, 10 }, { SeedType::SEED_MAGNETSHROOM, 5 },
		{ SeedType::SEED_BLOVER, 5 }, { SeedType::SEED_CACTUS, 15 }, { SeedType::SEED_STARFRUIT, 25 },
		{ SeedType::SEED_SPLITPEA, 5 }, { SeedType::SEED_PUMPKINSHELL, 10 },
	};
	constexpr ConveyorSeedRow kConveyorRowsFinalBoss[] = {
		{ SeedType::SEED_FLOWERPOT, 55 }, { SeedType::SEED_MELONPULT, 10 }, { SeedType::SEED_JALAPENO, 12 },
		{ SeedType::SEED_CABBAGEPULT, 10 }, { SeedType::SEED_KERNELPULT, 5 }, { SeedType::SEED_ICESHROOM, 8 },
	};
	constexpr ConveyorSeedRow kConveyorRowsShovel[] = {
		{ SeedType::SEED_PEASHOOTER, 100 },
	};
	constexpr ConveyorSeedRow kConveyorRowsBowling2[] = {
		{ SeedType::SEED_WALLNUT, 85 }, { SeedType::SEED_EXPLODE_O_NUT, 15 }, { SeedType::SEED_GIANT_WALLNUT, 15 },
	};
	constexpr ConveyorSeedRow kConveyorRowsBowling[] = {
		{ SeedType::SEED_WALLNUT, 85 }, { SeedType::SEED_EXPLODE_O_NUT, 15 },
	};
	constexpr ConveyorSeedRow kConveyorRowsLittleTrouble[] = {
		{ SeedType::SEED_LILYPAD, 25 }, { SeedType::SEED_WALLNUT, 15 }, { SeedType::SEED_PEASHOOTER, 25 },
		{ SeedType::SEED_CHERRYBOMB, 35 },
	};
	constexpr ConveyorSeedRow kConveyorRowsStormyNight[] = {
		{ SeedType::SEED_LILYPAD, 30 }, { SeedType::SEED_CACTUS, 10 }, { SeedType::SEED_PEASHOOTER, 20 },
		{ SeedType::SEED_PUFFSHROOM, 15 }, { SeedType::SEED_CHERRYBOMB, 25 },
	};
	constexpr ConveyorSeedRow kConveyorRowsBungeeBlitz[] = {
		{ SeedType::SEED_FLOWERPOT, 50 }, { SeedType::SEED_CHOMPER, 25 }, { SeedType::SEED_PUMPKINSHELL, 15 },
		{ SeedType::SEED_CHERRYBOMB, 10 },
	};
	constexpr ConveyorSeedRow kConveyorRowsPortalCombat[] = {
		{ SeedType::SEED_PEASHOOTER, 25 }, { SeedType::SEED_REPEATER, 20 }, { SeedType::SEED_TORCHWOOD, 10 },
		{ SeedType::SEED_CACTUS, 15 }, { SeedType::SEED_WALLNUT, 15 }, { SeedType::SEED_CHERRYBOMB, 15 },
	};
	constexpr ConveyorSeedRow kConveyorRowsColumn[] = {
		{ SeedType::SEED_FLOWERPOT, 155 }, { SeedType::SEED_MELONPULT, 5 }, { SeedType::SEED_CHOMPER, 5 },
		{ SeedType::SEED_PUMPKINSHELL, 15 }, { SeedType::SEED_JALAPENO, 10 }, { SeedType::SEED_SQUASH, 10 },
	};
	constexpr ConveyorSeedRow kConveyorRowsInvisighoul[] = {
		{ SeedType::SEED_PEASHOOTER, 25 }, { SeedType::SEED_WALLNUT, 15 }, { SeedType::SEED_KERNELPULT, 5 },
		{ SeedType::SEED_SQUASH, 15 }, { SeedType::SEED_LILYPAD, 30 }, { SeedType::SEED_ICESHROOM, 10 },
	};

	// 冒险关卡→出卡序列表（B 形态）：mLevel-1 直下标（1..50）；mRows == nullptr = 原链落空（断言）。
	// 槽值推导（关键遮蔽序）：非冒险模式 mBoard->mLevel 恒 0（Board::InitLevel），关卡支只在冒险命中；
	// 冒险重定向谓词按原链先后并入：5→IsWallnutBowlingLevel、25→IsLittleTroubleLevel、
	// 45→IsBungeeBlitzLevel、50→IsFinalBossLevel；40 槽 = 原 mLevel==40 关卡支
	// （IsStormyNightLevel 的冒险 40 级重定向在其后被遮蔽，仍为关卡 40 序列）。
	constexpr ConveyorSeedSet kConveyorSeedsPerLevel[FINAL_LEVEL] = {
		/* [1..4]   无传送带关卡（原链落空）                                    */ { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 },
		/* [5]      冒险保龄球关（IsWallnutBowlingLevel 重定向）                */ { kConveyorRowsBowling, 2 },
		/* [6..9]   无传送带关卡（原链落空）                                    */ { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 },
		/* [10]     冒险 10 关                                                  */ { kConveyorRowsLevel10, 7 },
		/* [11..19] 无传送带关卡（原链落空）                                    */ { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 },
		/* [20]     冒险 20 关                                                  */ { kConveyorRowsLevel20, 7 },
		/* [21..24] 无传送带关卡（原链落空）                                    */ { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 },
		/* [25]     冒险小麻烦关（IsLittleTroubleLevel 重定向）                 */ { kConveyorRowsLittleTrouble, 4 },
		/* [26..29] 无传送带关卡（原链落空）                                    */ { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 },
		/* [30]     冒险 30 关                                                  */ { kConveyorRowsLevel30, 8 },
		/* [31..39] 无传送带关卡（原链落空）                                    */ { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 },
		/* [40]     冒险 40 关（风暴之夜重定向被本支遮蔽）                      */ { kConveyorRowsLevel40, 8 },
		/* [41..44] 无传送带关卡（原链落空）                                    */ { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 },
		/* [45]     冒险蹦极闪击关（IsBungeeBlitzLevel 重定向）                 */ { kConveyorRowsBungeeBlitz, 4 },
		/* [46..49] 无传送带关卡（原链落空）                                    */ { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 }, { nullptr, 0 },
		/* [50]     冒险最终 Boss 关（IsFinalBossLevel 重定向）                 */ { kConveyorRowsFinalBoss, 6 },
	};
	static_assert(sizeof(kConveyorSeedsPerLevel) / sizeof(kConveyorSeedsPerLevel[0]) == FINAL_LEVEL,
		"冒险关卡→传送带出卡表槽位须与 FINAL_LEVEL 同步");
	static_assert(kConveyorSeedsPerLevel[10 - 1].mRows == kConveyorRowsLevel10 &&
		kConveyorSeedsPerLevel[40 - 1].mRows == kConveyorRowsLevel40 &&
		kConveyorSeedsPerLevel[50 - 1].mRows == kConveyorRowsFinalBoss &&
		kConveyorSeedsPerLevel[5 - 1].mCount == 2,
		"冒险关卡→传送带出卡表关键槽锚（10/40/50 序列与 5 槽长度）须与原链一致");

	// 模式→出卡序列表（B 形态）：GameMode 直下标；仅非冒险分支查表（冒险 [0] 表外走关卡表）；
	// mRows == nullptr = 原链落空（断言）。槽值 = 原链各模式谓词支（挑战模式互斥，先后序无遮蔽）。
	constexpr ConveyorSeedSet kConveyorSeedsPerMode[GameMode::NUM_GAME_MODES] = {
	/* [ 0] GAMEMODE_ADVENTURE  (v1 0) */ { nullptr, 0 },
	/* [ 1] GAMEMODE_MINIGAME_BEGHOULED  (v1 20) */ { nullptr, 0 },
	/* [ 2] GAMEMODE_MINIGAME_BEGHOULED_TWIST  (v1 24) */ { nullptr, 0 },
	/* [ 3] GAMEMODE_MINIGAME_BOBSLED_BONANZA  (v1 28) */ { nullptr, 0 },
	/* [ 4] GAMEMODE_MINIGAME_COLUMN  (v1 27) */ { kConveyorRowsColumn, 6 },
	/* [ 5] GAMEMODE_MINIGAME_FINAL_BOSS  (v1 35) */ { kConveyorRowsFinalBoss, 6 },
	/* [ 6] GAMEMODE_MINIGAME_INVISIGHOUL  (v1 21) */ { kConveyorRowsInvisighoul, 6 },
	/* [ 7] GAMEMODE_MINIGAME_LAST_STAND  (v1 31) */ { nullptr, 0 },
	/* [ 8] GAMEMODE_MINIGAME_LITTLE_TROUBLE  (v1 25) */ { kConveyorRowsLittleTrouble, 4 },
	/* [ 9] GAMEMODE_MINIGAME_POGO_PARTY  (v1 34) */ { nullptr, 0 },
	/* [10] GAMEMODE_MINIGAME_PORTAL_COMBAT  (v1 26) */ { kConveyorRowsPortalCombat, 6 },
	/* [11] GAMEMODE_MINIGAME_RAINING_SEEDS  (v1 19) */ { nullptr, 0 },
	/* [12] GAMEMODE_MINIGAME_SEEING_STARS  (v1 22) */ { nullptr, 0 },
	/* [13] GAMEMODE_MINIGAME_SLOT_MACHINE  (v1 18) */ { nullptr, 0 },
	/* [14] GAMEMODE_MINIGAME_SPEED  (v1 29) */ { nullptr, 0 },
	/* [15] GAMEMODE_MINIGAME_WALLNUT_BOWLING  (v1 17) */ { kConveyorRowsBowling, 2 },
	/* [16] GAMEMODE_MINIGAME_WALLNUT_BOWLING_2  (v1 33) */ { kConveyorRowsBowling2, 3 },
	/* [17] GAMEMODE_MINIGAME_WAR_AND_PEAS  (v1 16) */ { nullptr, 0 },
	/* [18] GAMEMODE_MINIGAME_WAR_AND_PEAS_2  (v1 32) */ { nullptr, 0 },
	/* [19] GAMEMODE_MINIGAME_WHACK_A_ZOMBIE  (v1 30) */ { nullptr, 0 },
	/* [20] GAMEMODE_MINIGAME_ZOMBIQUARIUM  (v1 23) */ { nullptr, 0 },
	/* [21] GAMEMODE_LIMBO_AIR_RAID  (v1 41) */ { nullptr, 0 },
	/* [22] GAMEMODE_LIMBO_ART_CHALLENGE_SUNFLOWER  (v1 40) */ { nullptr, 0 },
	/* [23] GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT  (v1 36) */ { nullptr, 0 },
	/* [24] GAMEMODE_LIMBO_BIG_TIME  (v1 39) */ { nullptr, 0 },
	/* [25] GAMEMODE_LIMBO_BUNGEE_BLITZ  (v1 48) */ { kConveyorRowsBungeeBlitz, 4 },
	/* [26] GAMEMODE_LIMBO_GRAVE_DANGER  (v1 45) */ { nullptr, 0 },
	/* [27] GAMEMODE_LIMBO_HIGH_GRAVITY  (v1 44) */ { nullptr, 0 },
	/* [28] GAMEMODE_LIMBO_ICE  (v1 42) */ { nullptr, 0 },
	/* [29] GAMEMODE_LIMBO_RESODDED  (v1 38) */ { nullptr, 0 },
	/* [30] GAMEMODE_LIMBO_SHOVEL  (v1 46) */ { kConveyorRowsShovel, 1 },
	/* [31] GAMEMODE_LIMBO_SQUIRREL  (v1 49) */ { nullptr, 0 },
	/* [32] GAMEMODE_LIMBO_STORMY_NIGHT  (v1 47) */ { kConveyorRowsStormyNight, 5 },
	/* [33] GAMEMODE_LIMBO_SUNNY_DAY  (v1 37) */ { nullptr, 0 },
	/* [34] GAMEMODE_LIMBO_ZEN_GARDEN  (v1 43) */ { nullptr, 0 },
	/* [35] GAMEMODE_INTRO  (v1 71) */ { nullptr, 0 },
	/* [36] GAMEMODE_PUZZLE_I_ZOMBIE_1  (v1 61) */ { nullptr, 0 },
	/* [37] GAMEMODE_PUZZLE_I_ZOMBIE_2  (v1 62) */ { nullptr, 0 },
	/* [38] GAMEMODE_PUZZLE_I_ZOMBIE_3  (v1 63) */ { nullptr, 0 },
	/* [39] GAMEMODE_PUZZLE_I_ZOMBIE_4  (v1 64) */ { nullptr, 0 },
	/* [40] GAMEMODE_PUZZLE_I_ZOMBIE_5  (v1 65) */ { nullptr, 0 },
	/* [41] GAMEMODE_PUZZLE_I_ZOMBIE_6  (v1 66) */ { nullptr, 0 },
	/* [42] GAMEMODE_PUZZLE_I_ZOMBIE_7  (v1 67) */ { nullptr, 0 },
	/* [43] GAMEMODE_PUZZLE_I_ZOMBIE_8  (v1 68) */ { nullptr, 0 },
	/* [44] GAMEMODE_PUZZLE_I_ZOMBIE_9  (v1 69) */ { nullptr, 0 },
	/* [45] GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS  (v1 70) */ { nullptr, 0 },
	/* [46] GAMEMODE_SCARY_POTTER_1  (v1 51) */ { nullptr, 0 },
	/* [47] GAMEMODE_SCARY_POTTER_2  (v1 52) */ { nullptr, 0 },
	/* [48] GAMEMODE_SCARY_POTTER_3  (v1 53) */ { nullptr, 0 },
	/* [49] GAMEMODE_SCARY_POTTER_4  (v1 54) */ { nullptr, 0 },
	/* [50] GAMEMODE_SCARY_POTTER_5  (v1 55) */ { nullptr, 0 },
	/* [51] GAMEMODE_SCARY_POTTER_6  (v1 56) */ { nullptr, 0 },
	/* [52] GAMEMODE_SCARY_POTTER_7  (v1 57) */ { nullptr, 0 },
	/* [53] GAMEMODE_SCARY_POTTER_8  (v1 58) */ { nullptr, 0 },
	/* [54] GAMEMODE_SCARY_POTTER_9  (v1 59) */ { nullptr, 0 },
	/* [55] GAMEMODE_SCARY_POTTER_ENDLESS  (v1 60) */ { nullptr, 0 },
	/* [56] GAMEMODE_SURVIVAL_ENDLESS_STAGE_1  (v1 11) */ { nullptr, 0 },
	/* [57] GAMEMODE_SURVIVAL_ENDLESS_STAGE_2  (v1 12) */ { nullptr, 0 },
	/* [58] GAMEMODE_SURVIVAL_ENDLESS_STAGE_3  (v1 13) */ { nullptr, 0 },
	/* [59] GAMEMODE_SURVIVAL_ENDLESS_STAGE_4  (v1 14) */ { nullptr, 0 },
	/* [60] GAMEMODE_SURVIVAL_ENDLESS_STAGE_5  (v1 15) */ { nullptr, 0 },
	/* [61] GAMEMODE_SURVIVAL_HARD_STAGE_1  (v1 6) */ { nullptr, 0 },
	/* [62] GAMEMODE_SURVIVAL_HARD_STAGE_2  (v1 7) */ { nullptr, 0 },
	/* [63] GAMEMODE_SURVIVAL_HARD_STAGE_3  (v1 8) */ { nullptr, 0 },
	/* [64] GAMEMODE_SURVIVAL_HARD_STAGE_4  (v1 9) */ { nullptr, 0 },
	/* [65] GAMEMODE_SURVIVAL_HARD_STAGE_5  (v1 10) */ { nullptr, 0 },
	/* [66] GAMEMODE_SURVIVAL_NORMAL_STAGE_1  (v1 1) */ { nullptr, 0 },
	/* [67] GAMEMODE_SURVIVAL_NORMAL_STAGE_2  (v1 2) */ { nullptr, 0 },
	/* [68] GAMEMODE_SURVIVAL_NORMAL_STAGE_3  (v1 3) */ { nullptr, 0 },
	/* [69] GAMEMODE_SURVIVAL_NORMAL_STAGE_4  (v1 4) */ { nullptr, 0 },
	/* [70] GAMEMODE_SURVIVAL_NORMAL_STAGE_5  (v1 5) */ { nullptr, 0 },
	/* [71] GAMEMODE_TREE_OF_WISDOM  (v1 50) */ { nullptr, 0 },
	/* [72] GAMEMODE_CUSTOM_LEVEL（哨兵；策略层覆写）*/ { nullptr, 0 },
};
	static_assert(sizeof(kConveyorSeedsPerMode) / sizeof(kConveyorSeedsPerMode[0]) == (int)GameMode::NUM_GAME_MODES,
		"模式→传送带出卡表槽位须与 GameMode 枚举尾同步");
	static_assert(kConveyorSeedsPerMode[(int)GameMode::GAMEMODE_MINIGAME_COLUMN].mRows == kConveyorRowsColumn &&
		kConveyorSeedsPerMode[(int)GameMode::GAMEMODE_LIMBO_SHOVEL].mCount == 1 &&
		kConveyorSeedsPerMode[(int)GameMode::GAMEMODE_MINIGAME_FINAL_BOSS].mRows == kConveyorRowsFinalBoss,
		"模式→传送带出卡表关键槽锚（柱列/铲子/Boss 序列）须与原链一致");
}  // namespace

//0x422CD0
void Challenge::UpdateConveyorBelt()
{
	if (mBoard->HasLevelAwardDropped())
		return;

	mBoard->mSeedBank->UpdateConveyorBelt();
	mConveyorBeltCounter--;
	if (mConveyorBeltCounter > 0)
		return;

	// 表外特例（保留原链）：速度倍率判别式混排——IsShovelLevel 谓词与 PORTAL_COMBAT 模式
	// 在同一支内 || 复合，且 IsFinalBossLevel 为模式+关卡复合谓词，不并入下方出卡数据表。
	float aConveyorSpeedMultiplier = 1;
	if (mApp->IsFinalBossLevel())
	{
		aConveyorSpeedMultiplier = 0.875f;
	}
	else if (mApp->IsShovelLevel() || mApp->mGameMode == GAMEMODE_MINIGAME_PORTAL_COMBAT)
	{
		aConveyorSpeedMultiplier = 1.5f;
	}
	else if (mApp->mGameMode == GAMEMODE_MINIGAME_INVISIGHOUL)
	{
		aConveyorSpeedMultiplier = 2.0f;
	}
	else if (mApp->mGameMode == GAMEMODE_MINIGAME_COLUMN)
	{
		aConveyorSpeedMultiplier = 3.0f;
	}
	int aNumSeedsOnConveyor = mBoard->mSeedBank->GetNumSeedsOnConveyorBelt();
	mConveyorBeltCounter = aConveyorSpeedMultiplier * (aNumSeedsOnConveyor > 8 ? 1000 : aNumSeedsOnConveyor > 6 ? 500 : aNumSeedsOnConveyor > 4 ? 425 : 400);

	TodWeightedArray aSeedPickArray[20];
	int aSeedPickCount = 0;
	// B 形态 -> 常量表（上方 kConveyorSeedsPerLevel / kConveyorSeedsPerMode）：
	// 冒险按 mLevel-1 直下标，非冒险按 GameMode 直下标（非冒险 mLevel 恒 0，关卡支不可能命中，
	// 原链各谓词支的冒险重定向已并入关卡表槽值）；mRows == nullptr = 原链落空 else TOD_ASSERT(false)。
	const ConveyorSeedSet* aSeedSet = nullptr;
	if (mApp->IsAdventureMode())
	{
		int aLevelIndex = mBoard->mLevel - 1;
		if ((unsigned)aLevelIndex < (unsigned)(sizeof(kConveyorSeedsPerLevel) / sizeof(kConveyorSeedsPerLevel[0])))
		{
			aSeedSet = &kConveyorSeedsPerLevel[aLevelIndex];
		}
	}
	else
	{
		int aMode = (int)mApp->mGameMode;
		if ((unsigned)aMode < (unsigned)(sizeof(kConveyorSeedsPerMode) / sizeof(kConveyorSeedsPerMode[0])))
		{
			aSeedSet = &kConveyorSeedsPerMode[aMode];
		}
	}
	TOD_ASSERT(aSeedSet != nullptr && aSeedSet->mRows != nullptr);
	if (aSeedSet != nullptr && aSeedSet->mRows != nullptr)
	{
		aSeedPickCount = aSeedSet->mCount;
		for (int i = 0; i < aSeedPickCount; i++)
		{
			aSeedPickArray[i].mItem = aSeedSet->mRows[i].mSeed;
			aSeedPickArray[i].mWeight = aSeedSet->mRows[i].mWeight;
		}
	}

	for (int i = 0; i < aSeedPickCount; i++)
	{
		TodWeightedArray& aSeedPick = aSeedPickArray[i];
		SeedType aSeedType = (SeedType)aSeedPick.mItem;
		int aCountInBank = mBoard->mSeedBank->CountOfTypeOnConveyorBelt(aSeedType);
		int aTotalCount = mBoard->CountPlantByType(aSeedType) + aCountInBank;

		if (aSeedType == SEED_GRAVEBUSTER)
		{
			if (mBoard->GetGraveStonesCount() <= aTotalCount)
			{
				aSeedPick.mWeight = 0;
				continue;
			}
		}
		else if (aSeedType == SEED_LILYPAD)
		{
			aSeedPick.mWeight = TodAnimateCurve(0, 18, aTotalCount, aSeedPick.mWeight, 1, Curves::Linear);
		}
		else if (aSeedType == SEED_FLOWERPOT)
		{
			aSeedPick.mWeight = TodAnimateCurve(0, mApp->mGameMode == GAMEMODE_MINIGAME_COLUMN ? 45 : 35, aTotalCount, aSeedPick.mWeight, 1, Curves::Linear);
		}

		if (mApp->IsFinalBossLevel())
		{
			if (aSeedType == SEED_MELONPULT || aSeedType == SEED_KERNELPULT || aSeedType == SEED_CABBAGEPULT)
			{
				int aEmptyPots = mBoard->CountEmptyPotsOrLilies(SEED_FLOWERPOT);
				if (aEmptyPots <= 2)
				{
					aSeedPick.mWeight /= 5;
				}
				else if (aEmptyPots <= 5)
				{
					aSeedPick.mWeight /= 3;
				}
			}

			if (aSeedType == SEED_FLOWERPOT && mBoard->GetBossZombie()->mZombiePhase == PHASE_BOSS_DROP_RV)
			{
				aSeedPick.mWeight = 500;
			}
		}

		if (aSeedPickCount > 2)
		{
			if (aCountInBank >= 4)
			{
				aSeedPick.mWeight = 1;
			}
			else if (aCountInBank >= 3)
			{
				aSeedPick.mWeight = 5;
			}
			else if (aSeedType == mLastConveyorSeedType)
			{
				aSeedPick.mWeight /= 2;
			}
		}
	}

	SeedType aSeedType = (SeedType)TodPickFromWeightedArray(aSeedPickArray, aSeedPickCount);
	mBoard->mSeedBank->AddSeed(aSeedType);
	mLastConveyorSeedType = aSeedType;
}

//0x4234A0
void Challenge::UpdateRainingSeeds()
{
	if (mBoard->HasLevelAwardDropped() || --mChallengeStateCounter != 0)
		return;

	mChallengeStateCounter = RandRangeInt(500, 999);

	Coin* aCoin = mBoard->AddCoin(RandRangeInt(100, 649), 60, COIN_USABLE_SEED_PACKET, COIN_MOTION_FROM_SKY_SLOW);

	SeedType aSeedType;
	do
		aSeedType = (SeedType)Rand(mApp->GetSeedsAvailable());
	while (
		mBoard->SeedNotRecommendedForLevel(aSeedType) ||
		!mApp->SeedTypeAvailable(aSeedType) ||
		Plant::IsUpgrade(aSeedType) ||
		aSeedType == SEED_SUNFLOWER ||
		aSeedType == SEED_TWINSUNFLOWER ||
		aSeedType == SEED_INSTANT_COFFEE ||
		aSeedType == SEED_UMBRELLA ||
		aSeedType == SEED_SUNSHROOM ||
		aSeedType == SEED_IMITATER
		);
	if (Rand(100) < TodAnimateCurve(0, 18, mBoard->CountPlantByType(SEED_LILYPAD), 30, 1, Curves::Linear))
		aSeedType = SEED_LILYPAD;

	aCoin->mUsableSeedType = aSeedType;
}

//0x423670
void Challenge::UpdateStormyNight()
{
	if (mBoard->mPaused)
	{
		if (mChallengeStateCounter == 1)
			return;

		if (mChallengeStateCounter == 150 && mChallengeState == ChallengeState::STATECHALLENGE_STORM_FLASH_1)
		{
			mChallengeStateCounter = 1;
			return;
		}
	}

	mChallengeStateCounter--;
	if ((mChallengeStateCounter == 300 && (mChallengeState == STATECHALLENGE_STORM_FLASH_1 || mChallengeState == STATECHALLENGE_STORM_FLASH_2)) ||
		(mChallengeStateCounter == 150 && (mChallengeState == STATECHALLENGE_STORM_FLASH_1 || mChallengeState == STATECHALLENGE_STORM_FLASH_3)))
	{
		mApp->PlayFoley(FoleyType::FOLEY_THUNDER);
	}
	if (mChallengeStateCounter > 0)
		return;

	if (mApp->mGameScene == GameScenes::SCENE_ZOMBIES_WON)
	{
		mChallengeStateCounter = STORM_FLASH_TIME + RandRangeInt(-50, 50);
		mChallengeState = ChallengeState::STATECHALLENGE_STORM_FLASH_3;
		return;
	}
	else if (mApp->mGameScene != GameScenes::SCENE_PLAYING)
	{
		mChallengeState = ChallengeState::STATECHALLENGE_NORMAL;
		mChallengeStateCounter = 0;
		return;
	}
	else
	{
		//判断是否存在雪人僵尸，存在则状态倒计时从 350~450 随机
		if (mBoard->CountZombieByType(ZombieType::ZOMBIE_YETI))
		{
			mChallengeStateCounter = STORM_FLASH_TIME + RandRangeInt(200, 300);
		}
		else
		{
			int aMaxDur = Sexy::Rand(2) ? 400 : 750;
			mChallengeStateCounter = STORM_FLASH_TIME + RandRangeInt(300, aMaxDur);
		}

		mChallengeState = (ChallengeState)RandRangeInt((int)STATECHALLENGE_STORM_FLASH_1, (int)STATECHALLENGE_STORM_FLASH_3);
	}
}

//0x423800
// GOTY @Patoke: 0x426320
void Challenge::UpdateSlotMachine()
{
	int aSunMoney = ClampInt(mBoard->mSunMoney, 0, 2000);
	if (aSunMoney >= SLOT_MACHINE_WINNING_SCORE - 100)
	{
		mBoard->DisplayAdvice(gStr.ADVICE_ALMOST_THERE, MESSAGE_STYLE_HINT_FAST, ADVICE_ALMOST_THERE);
	}
	if (aSunMoney >= 2000)
	{
		SpawnLevelAward(4, 2);
		mBoard->ClearAdvice(ADVICE_NONE);
	}
	mBoard->mProgressMeterWidth = TodAnimateCurve(0, SLOT_MACHINE_WINNING_SCORE, aSunMoney, 0, PROGRESS_METER_COUNTER, Curves::Linear);

	if (!mBoard->mAdvice->IsBeingDisplayed())
	{
		SexyString aMsg = TodReplaceNumberString(gStr.ADVICE_SLOT_MACHINE_COLLECT_SUN, UiTextPlaceholder::Score, SLOT_MACHINE_WINNING_SCORE);
		mBoard->DisplayAdvice(aMsg, MESSAGE_STYLE_SLOT_MACHINE, ADVICE_SLOT_MACHINE_COLLECT_SUN);
	}

	if (mChallengeState != STATECHALLENGE_SLOT_MACHINE_ROLLING)
	{
		if (!mBoard->mAdvice->IsBeingDisplayed() && !mBoard->HasLevelAwardDropped())
		{
			mBoard->DisplayAdviceAgain(gStr.ADVICE_SLOT_MACHINE_SPIN_AGAIN, MESSAGE_STYLE_SLOT_MACHINE, ADVICE_SLOT_MACHINE_SPIN_AGAIN);
		}
	}
	else if (mBoard->mSeedBank->mSeedPackets[0].mSlotMachineEndTimeNs == 0)
	{
		mApp->ReanimationGet(mReanimChallenge)->PlayReanim(ReanimTrackId::anim_return, REANIM_PLAY_ONCE_AND_HOLD, 0, 24.0f);
		mChallengeState = STATECHALLENGE_NORMAL;

		SeedType aPacket1 = mBoard->mSeedBank->mSeedPackets[0].mPacketType;
		SeedType aPacket2 = mBoard->mSeedBank->mSeedPackets[1].mPacketType;
		SeedType aPacket3 = mBoard->mSeedBank->mSeedPackets[2].mPacketType;
		if (aPacket1 != aPacket2 || aPacket2 != aPacket3)
		{
			if (aPacket1 == aPacket2 || aPacket2 == aPacket3 || aPacket1 == aPacket3)
			{
				mApp->PlayFoley(FOLEY_ART_CHALLENGE);
				SeedType aSeedType = (aPacket1 == aPacket2 || aPacket1 == aPacket3) ? aPacket1 : aPacket2;
				if (aSeedType == SEED_SLOT_MACHINE_DIAMOND)
				{
					mBoard->DisplayAdvice(gStr.ADVICE_SLOT_MACHINE_2_DIAMONDS, MESSAGE_STYLE_SLOT_MACHINE, ADVICE_NONE);
					mBoard->AddCoin(360, 85, COIN_DIAMOND, COIN_MOTION_COIN);
				}
				else if (aSeedType == SEED_SLOT_MACHINE_SUN)
				{
					mBoard->DisplayAdvice(gStr.ADVICE_SLOT_MACHINE_2_SUNS, MESSAGE_STYLE_SLOT_MACHINE, ADVICE_NONE);
					for (int i = 0; i < 4; i++)
					{
						mBoard->AddCoin(320 + i * 15, 85, COIN_SUN, COIN_MOTION_COIN);
					}
				}
				else
				{
					mBoard->DisplayAdvice(gStr.ADVICE_SLOT_MACHINE_2_OF_A_KIND, MESSAGE_STYLE_SLOT_MACHINE, ADVICE_NONE);
					mBoard->AddCoin(360, 85, COIN_USABLE_SEED_PACKET, COIN_MOTION_COIN)->mUsableSeedType = aSeedType;
				}
			}
		}
		else
		{
			mApp->PlayFoley(FOLEY_ART_CHALLENGE);
			if (aPacket1 == SEED_SLOT_MACHINE_DIAMOND)
			{
				mBoard->DisplayAdvice(gStr.ADVICE_SLOT_MACHINE_DIAMOND_JACKPOT, MESSAGE_STYLE_SLOT_MACHINE, ADVICE_NONE);
				for (int i = 0; i < 5; i++)
				{
					mBoard->AddCoin(320 + i * 12, 85, COIN_DIAMOND, COIN_MOTION_COIN);
				}
			}
			else if (aPacket1 == SEED_SLOT_MACHINE_SUN)
			{
				mBoard->DisplayAdvice(gStr.ADVICE_SLOT_MACHINE_SUN_JACKPOT, MESSAGE_STYLE_SLOT_MACHINE, ADVICE_NONE);
				for (int i = 0; i < 20; i++)
				{
					mBoard->AddCoin(320 + i * 3, 85, COIN_SUN, COIN_MOTION_COIN);
				}
			}
			else
			{
				mBoard->DisplayAdvice(gStr.ADVICE_SLOT_MACHINE_3_OF_A_KIND, MESSAGE_STYLE_SLOT_MACHINE, ADVICE_NONE);
				// @Patoke: fix silly bug XD
				for (int i = 0; i < 3; i++)
				{
					mBoard->AddCoin(320 + i * 20, 85, COIN_USABLE_SEED_PACKET, COIN_MOTION_COIN)->mUsableSeedType = aPacket1;
				}
			}
		}
	}
}

//0x4244C0
void Challenge::BeghouledCheckStuckState()
{
	if (mChallengeState != STATECHALLENGE_NORMAL || mBoard->HasLevelAwardDropped())
		return;

	BeghouledBoardState aBoardState;
	LoadBeghouledBoardState(&aBoardState);
	if (!BeghouledCheckForPossibleMoves(&aBoardState))
	{
		mChallengeState = STATECHALLENGE_BEGHOULED_NO_MATCHES;
		mChallengeStateCounter = 500;
		mBoard->DisplayAdviceAgain(gStr.ADVICE_BEGHOULED_NO_MOVES, MESSAGE_STYLE_HINT_FAST, ADVICE_BEGHOULED_NO_MOVES);
	}
}

//0x424590
void Challenge::ZombieAtePlant(/*Zombie* theZombie,*/ Plant* thePlant)
{
	// theZombie 参数在原版中被优化删去，此处参考内测版将其保留。注意：传入的 theZombie 可能为 nullptr
	if (mApp->mGameMode != GAMEMODE_MINIGAME_BEGHOULED && mApp->mGameMode != GAMEMODE_MINIGAME_BEGHOULED_TWIST)
		return;

	mBeghouledEated[thePlant->mPlantCol][thePlant->mRow] = true;

	if (mBoard->mSeedBank->mNumPackets == 4)
	{
		mBoard->mSeedBank->mSeedPackets[4].SetPacketType(SEED_BEGHOULED_BUTTON_CRATER);
		mBoard->mSeedBank->mNumPackets = 5;
		mBoard->DisplayAdvice(gStr.ADVICE_BEGHOULED_USE_CRATER_1, MESSAGE_STYLE_HINT_FAST, ADVICE_BEGHOULED_USE_CRATER_1);
	}

	BeghouledCheckStuckState();
	BeghouledUpdateCraters();
}

namespace
{
void UpdateChallengeDefault(Challenge&, std::uint64_t)
{
}

void UpdateChallengeSeedBank(Challenge& challenge)
{
	auto& board=*challenge.mBoard;
	if (board.mSeedBank->mY < 0 &&
		(board.mSunMoney + board.CountSunBeingCollected() > 0 || board.mSeedBank->mY > Sexy::IMAGE_SEEDBANK->mWidth))
	{
		board.mSeedBank->mY += 2;
		if (board.mSeedBank->mY > 0)
			board.mSeedBank->mY = 0;
	}
}

void UpdateChallengeVases(Challenge& challenge, std::uint64_t)
{
	challenge.ScaryPotterUpdate();
	UpdateChallengeSeedBank(challenge);
}

void UpdateChallengeWhack(Challenge& challenge, std::uint64_t)
{
	// Its tutorial update does not read/write the HUD position, sun or coins.
	challenge.WhackAZombieUpdate();
	UpdateChallengeSeedBank(challenge);
}

void UpdateChallengeBeghouled(Challenge& challenge, std::uint64_t)
{
	challenge.UpdateBeghouled();
}

void UpdateChallengeIZombie(Challenge& challenge, std::uint64_t)
{
	challenge.IZombieUpdate();
}

void UpdateChallengeAdventure(Challenge& challenge, std::uint64_t deltaNs)
{
	// Match the old predicates' profile level, including out-of-range no-ops.
	const auto index=static_cast<unsigned>(challenge.mApp->mPlayerInfo->mLevel)-1;
	if (index < Challenge::kAdventureChallengeUpdates.size())
		Challenge::kAdventureChallengeUpdates[index](challenge, deltaNs);
}
}

// Static member declarations remain in the header; constexpr definitions and
// handler bodies stay here. Neither table needs runtime initialization.
constexpr std::array<Challenge::UpdateFn, NUM_LEVELS> Challenge::kAdventureChallengeUpdates = [] {
	std::array<Challenge::UpdateFn, NUM_LEVELS> updates{};
	updates.fill(UpdateChallengeDefault);
	updates[15-1] = UpdateChallengeWhack;
	updates[35-1] = UpdateChallengeVases;
	return updates;
}();
static_assert(Challenge::kAdventureChallengeUpdates[15-1] == UpdateChallengeWhack &&
	Challenge::kAdventureChallengeUpdates[35-1] == UpdateChallengeVases &&
	Challenge::kAdventureChallengeUpdates[1-1] == UpdateChallengeDefault);

constexpr std::array<Challenge::UpdateFn, NUM_GAME_MODES> Challenge::kChallengeUpdates = [] {
	std::array<Challenge::UpdateFn, NUM_GAME_MODES> updates{};
	updates.fill(UpdateChallengeDefault);
	updates[GAMEMODE_ADVENTURE] = UpdateChallengeAdventure;
	updates[GAMEMODE_MINIGAME_BEGHOULED] =
		updates[GAMEMODE_MINIGAME_BEGHOULED_TWIST] = UpdateChallengeBeghouled;
	for (int mode=GAMEMODE_SCARY_POTTER_1; mode<=GAMEMODE_SCARY_POTTER_ENDLESS; ++mode)
		updates[mode] = UpdateChallengeVases;
	updates[GAMEMODE_MINIGAME_WHACK_A_ZOMBIE] = UpdateChallengeWhack;
	for (int mode=GAMEMODE_PUZZLE_I_ZOMBIE_1; mode<=GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS; ++mode)
		updates[mode] = UpdateChallengeIZombie;
	updates[GAMEMODE_MINIGAME_SLOT_MACHINE] = +[](Challenge& c, std::uint64_t) { c.UpdateSlotMachine(); };
	updates[GAMEMODE_MINIGAME_SPEED] = +[](Challenge& c, std::uint64_t deltaNs) {
		// Keep the second object update; independent effects retain normal age.
		auto& board=*c.mBoard;
		if (board.mLevelTimeNs > std::uint64_t(INT64_MAX) ||
			deltaNs > std::uint64_t(INT64_MAX) - board.mLevelTimeNs)
			throw std::overflow_error("speed-mode battle timestamp exceeds clock range");
		board.mLevelTimeNs += deltaNs;
		c.mApp->mEffectSystem->mReanimationHolder->mBattleTimeNs = static_cast<std::int64_t>(board.mLevelTimeNs);
		c.mApp->mEffectSystem->RebaseIndependentBattleEffects(static_cast<std::int64_t>(board.mLevelTimeNs));
		board.UpdateGame(deltaNs);
	};
	updates[GAMEMODE_MINIGAME_RAINING_SEEDS] = +[](Challenge& c, std::uint64_t) { c.UpdateRainingSeeds(); };
	updates[GAMEMODE_MINIGAME_PORTAL_COMBAT] = +[](Challenge& c, std::uint64_t) { c.UpdatePortalCombat(); };
	updates[GAMEMODE_LIMBO_SQUIRREL] = +[](Challenge& c, std::uint64_t) { c.SquirrelUpdate(); };
	updates[GAMEMODE_MINIGAME_ZOMBIQUARIUM] = +[](Challenge& c, std::uint64_t) { c.ZombiquariumUpdate(); };
	updates[GAMEMODE_TREE_OF_WISDOM] = +[](Challenge& c, std::uint64_t) { c.TreeOfWisdomUpdate(); };
	updates[GAMEMODE_LIMBO_ICE] = +[](Challenge& c, std::uint64_t deltaNs) {
		if (c.mBoard->mLevelTimeNs - deltaNs < 30'000'000'000 && c.mBoard->mLevelTimeNs >= 30'000'000'000)
		{
			c.mApp->PlayFoley(FOLEY_FLOOP);
			c.mApp->PlaySample(Sexy::SOUND_LOSEMUSIC);
		}
	};
	updates[GAMEMODE_MINIGAME_LAST_STAND] = +[](Challenge& c, std::uint64_t) { c.LastStandUpdate(); };
	return updates;
}();
static_assert(Challenge::kChallengeUpdates[GAMEMODE_SCARY_POTTER_1] ==
	Challenge::kAdventureChallengeUpdates[35-1] &&
	Challenge::kChallengeUpdates[GAMEMODE_MINIGAME_WHACK_A_ZOMBIE] ==
	Challenge::kAdventureChallengeUpdates[15-1]);

//0x4246B0
void Challenge::Update(std::uint64_t theLevelDeltaNs)
{
	if (mApp->IsStormyNightLevel())
		UpdateStormyNight();

	if (mBoard->mPaused)
	{
		if (mApp->mGameMode == GAMEMODE_MINIGAME_BEGHOULED_TWIST)
		{
			mChallengeGridX = -1;
			mChallengeGridY = -1;
		}
		return;
	}

	if (mApp->mGameMode == GAMEMODE_MINIGAME_RAINING_SEEDS || mApp->IsStormyNightLevel())
	{
		UpdateRain();
	}

	if (mApp->mGameScene != SCENE_PLAYING && mApp->mGameMode != GAMEMODE_TREE_OF_WISDOM)
		return;

	if (mBoard->HasConveyorBeltSeedBank())
	{
		UpdateConveyorBelt();
	}
	kChallengeUpdates[mApp->mGameMode](*this, theLevelDeltaNs);

	Reanimation* aReanim = mApp->ReanimationTryToGet(mReanimChallenge);
	if (aReanim && aReanim->mIsAttachment)
	{
		aReanim->UpdateAt((mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
	}
}

//0x4249F0
SeedType Challenge::GetArtChallengeSeed(int theGridX, int theGridY)
{
	if (theGridY < 6)
	{
		TOD_ASSERT(theGridX >= 0 && theGridX < ART_CHALLEGE_SIZE_X && theGridY >= 0);

		GameMode aGameMode = mApp->mGameMode;
		if (aGameMode == GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT)		return gArtChallengeWallnut[theGridY][theGridX];
		if (aGameMode == GAMEMODE_LIMBO_ART_CHALLENGE_SUNFLOWER)	return gArtChallengeSunFlower[theGridY][theGridX];
		if (aGameMode == GAMEMODE_MINIGAME_SEEING_STARS)				return gArtChallengeStarFruit[theGridY][theGridX];
	}
	return SEED_NONE;
}

//0x424A40
void Challenge::SpawnLevelAward(int theGridX, int theGridY)
{
	if (mBoard->HasLevelAwardDropped())
		return;

	float aPosX = mBoard->GridToPixelX(theGridX, theGridY) + 40;
	float aPosY = mBoard->GridToPixelY(theGridX, theGridY) + 40;
	CoinType aCoinType =
		mApp->IsFirstTimeAdventureMode() ? COIN_FINAL_SEED_PACKET :
		mApp->IsAdventureMode() || mApp->HasBeatenChallenge(mApp->mGameMode) ? COIN_AWARD_MONEY_BAG : COIN_TROPHY;

	mBoard->mLevelAwardSpawned = true;
	mApp->mBoardResult = BOARDRESULT_WON;
	mApp->PlayFoley(FOLEY_SPAWN_SUN);
	Coin* aCoin = mBoard->AddCoin(aPosX, aPosY, aCoinType, COIN_MOTION_COIN);
	mApp->AddTodParticle(BOARD_WIDTH / 2, BOARD_HEIGHT / 2, RENDER_LAYER_TOP, PARTICLE_SCREEN_FLASH, static_cast<std::int64_t>(mBoard->mLevelTimeNs));

	if (mApp->mGameMode == GAMEMODE_MINIGAME_ZOMBIQUARIUM)
	{
		aCoin->Collect();
	}
	else if (!mApp->IsIZombieLevel())
	{
		Zombie* aZombie = nullptr;
		while (mBoard->IterateZombies(aZombie))
		{
			if (!aZombie->IsDeadOrDying())
			{
				aZombie->TakeDamage(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), 1800, 0U);
			}
		}
	}
}

//0x424C90
void Challenge::CheckForCompleteArtChallenge(int theGridX, int theGridY)
{
	if (mBoard->HasLevelAwardDropped())
		return;

	for (int aGridY = 0; aGridY < MAX_GRID_SIZE_Y; aGridY++)
	{
		for (int aGridX = 0; aGridX < MAX_GRID_SIZE_X; aGridX++)
		{
			SeedType aSeedType = GetArtChallengeSeed(aGridX, aGridY);
			if (aSeedType != SEED_NONE)
			{
				Plant* aPlant = mBoard->GetTopPlantAt(aGridX, aGridY, TOPPLANT_ONLY_NORMAL_POSITION);
				if (aPlant == nullptr || aPlant->mSeedType != aSeedType)
				{
					return;
				}
			}
		}
	}
	SpawnLevelAward(theGridX, theGridY);
}

//0x424DE0
void Challenge::DrawArtChallenge(Graphics* g)
{
	g->mColorizeImages = true;
	g->mColor = Color(255, 255, 255, 100);

	for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
	{
		for (int aCol = 0; aCol < MAX_GRID_SIZE_X; aCol++)
		{
			SeedType aSeedType = GetArtChallengeSeed(aCol, aRow);
			if (aSeedType != SEED_NONE && mBoard->GetTopPlantAt(aCol, aRow, TOPPLANT_ONLY_NORMAL_POSITION) == nullptr)
			{
				Plant::DrawSeedType(g, aSeedType, SEED_NONE, VARIATION_NORMAL, mBoard->GridToPixelX(aCol, aRow), mBoard->GridToPixelY(aCol, aRow));
			}
		}
	}

	if (mApp->mGameMode == GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT)
	{
		// @Patoke: assets, removed, perhaps add back?
		//g->DrawImage(Sexy::IMAGE_GOOGLYEYE, 357, 174);
		//g->DrawImage(Sexy::IMAGE_GOOGLYEYE, 516, 174);
	}

	g->mColorizeImages = false;
}

//0x424FB0
void Challenge::DrawBeghouled(Graphics* g)
{
	for (int aGridY = 0; aGridY < MAX_GRID_SIZE_Y; aGridY++)
	{
		for (int aGridX = 0; aGridX < MAX_GRID_SIZE_X; aGridX++)
		{
			if (mBeghouledEated[aGridX][aGridY])
			{
				g->DrawImage(Sexy::IMAGE_CRATER, mBoard->GridToPixelX(aGridX, aGridY) - 8, mBoard->GridToPixelY(aGridX, aGridY) + 40, Rect(90, 0, 90, 61));
			}
		}
	}

	if (mApp->mGameMode == GAMEMODE_MINIGAME_BEGHOULED_TWIST)
	{
		HitResult aHitResult;
		mBoard->MouseHitTest(mApp->mWidgetManager->mLastMouseX - mBoard->mX,
			mApp->mWidgetManager->mLastMouseY - mBoard->mY, &aHitResult);
		if (mChallengeGridX != -1 && mChallengeGridY != -1 && aHitResult.mObjectType != OBJECT_TYPE_COIN)
		{
			float aPixelX = mBoard->GridToPixelX(mChallengeGridX, mChallengeGridY) + 80;
			float aPixelY = mBoard->GridToPixelY(mChallengeGridX, mChallengeGridY) + 100;

			// 1073 uses 0.001 here: one full turn every 1000 updates.
			constexpr std::uint64_t BEGHOULED_ROTATION_PERIOD_NS = 10'000'000'000;
			const float aRotation =
				-static_cast<float>(double(mBoard->mLevelTimeNs % BEGHOULED_ROTATION_PERIOD_NS) /
					BEGHOULED_ROTATION_PERIOD_NS) * 2 * PI;
			SexyTransform2D aTransform;
			TodScaleRotateTransformMatrix(aTransform, aPixelX, aPixelY, aRotation, 1, 1);

			Image* aImageOverlay = Sexy::IMAGE_BEGHOULED_TWIST_OVERLAY;
			Rect aSrcRect = Rect(0, 0, aImageOverlay->mWidth, aImageOverlay->mHeight);
			TodBltMatrix(g, aImageOverlay, aTransform, g->mClipRect, Color(255, 255, 255, 128), g->mDrawMode, aSrcRect);
		}
	}
}

//0x4251E0
void Challenge::DrawSlotMachine(Graphics* g)
{
	if (mApp->mGameScene == SCENE_ZOMBIES_WON)
		return;

	Graphics gBoardParent = Graphics(*g);
	if (mSlotMachineRollCount < 3 && mBoard->mCursorObject->mCursorType == CURSOR_TYPE_NORMAL &&
		mChallengeState != STATECHALLENGE_SLOT_MACHINE_ROLLING && !mBoard->HasLevelAwardDropped())
	{
		gBoardParent.mColor = GetFlashingColorNs(mBoard->mLevelTimeNs, 750'000'000);
		gBoardParent.mColorizeImages = true;
	}
	gBoardParent.Translate(mApp->mScreenBounds.mX - mBoard->mX + mBoard->mSeedBank->mX,
		mApp->mScreenBounds.mY - mBoard->mY + mBoard->mSeedBank->mY);
	mApp->ReanimationGet(mReanimChallenge)->Draw(&gBoardParent);
}

//0x425300
void Challenge::DrawBackdrop(Graphics* g)
{
	GameMode aGameMode = mApp->mGameMode;
	if (mApp->IsArtChallenge())
	{
		DrawArtChallenge(g);
	}
	if (mApp->mGameMode == GAMEMODE_TREE_OF_WISDOM)
	{
		TreeOfWisdomDraw(g);
	}
	if (mApp->mGameMode == GAMEMODE_MINIGAME_BEGHOULED || mApp->mGameMode == GAMEMODE_MINIGAME_BEGHOULED_TWIST)
	{
		DrawBeghouled(g);
	}

	if (mApp->IsWallnutBowlingLevel() && mShowBowlingLine)
	{
		g->DrawImage(Sexy::IMAGE_WALLNUT_BOWLINGSTRIPE, 268, 77);
	}
	if (mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_1 || mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_2 || mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_3 ||
		mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_4 || mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_5)
	{
		g->DrawImage(Sexy::IMAGE_WALLNUT_BOWLINGSTRIPE, 352, 73);
	}
	if (mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_6 || mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_7 || mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_8 ||
		mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS)
	{
		g->DrawImage(Sexy::IMAGE_WALLNUT_BOWLINGSTRIPE, 432, 73);
	}
	if (mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_9)
	{
		g->DrawImage(Sexy::IMAGE_WALLNUT_BOWLINGSTRIPE, 512, 73);
	}

	if (aGameMode == GAMEMODE_MINIGAME_SLOT_MACHINE)
	{
		DrawSlotMachine(g);
	}
	if (aGameMode == GAMEMODE_LIMBO_ZEN_GARDEN)
	{
		mApp->mZenGarden->DrawBackdrop(g);
	}
}

//0x425460
void Challenge::PlantAdded(Plant* thePlant)
{
	if (mApp->IsArtChallenge())
	{
		SeedType anArtSeed = GetArtChallengeSeed(thePlant->mPlantCol, thePlant->mRow);
		if (anArtSeed != SEED_NONE && anArtSeed == thePlant->mSeedType)
		{
			mApp->PlayFoley(FOLEY_ART_CHALLENGE);
			mApp->AddTodParticle((thePlant->mX) + 40, (thePlant->mY) + 40, RENDER_LAYER_TOP, PARTICLE_PRESENT_PICKUP, static_cast<std::int64_t>(mBoard->mLevelTimeNs));
			CheckForCompleteArtChallenge(thePlant->mPlantCol, thePlant->mRow);
		}
	}
}

//0x425550
PlantingReason Challenge::CanPlantAt(int theGridX, int theGridY, SeedType theSeedType)
{
	if (mApp->IsWallnutBowlingLevel())
	{
		return theGridX > 2 ? PLANTING_NOT_PASSED_LINE : PLANTING_OK;
	}
	else if (mApp->IsIZombieLevel())
	{
		int aLimit = 6;
		if (mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_1 || mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_2 || mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_3 ||
			mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_4 || mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_5)
		{
			aLimit = 4;
		}
		if (mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_6 || mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_7 || mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_8 ||
			mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS)
		{
			aLimit = 5;
		}

		if (theSeedType == SEED_ZOMBIE_BUNGEE)
		{
			return theGridX < aLimit ? PLANTING_OK : PLANTING_NOT_HERE;
		}
		else if (IsZombieSeedType(theSeedType))
		{
			return theGridX >= aLimit ? PLANTING_OK : PLANTING_NOT_HERE;
		}
	}
	else if (mApp->IsArtChallenge())
	{
		SeedType anArtSeed = GetArtChallengeSeed(theGridX, theGridY);
		if (anArtSeed != SEED_NONE && anArtSeed != theSeedType && theSeedType != SEED_LILYPAD && theSeedType != SEED_PUMPKINSHELL)
		{
			return PLANTING_NOT_ON_ART;
		}
		if (mApp->mGameMode == GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT)
		{
			if ((theGridX == 4 || theGridX == 6) && theGridY == 1)
			{
				return PLANTING_NOT_HERE;
			}
		}
	}
	else if (mApp->IsFinalBossLevel() && theGridX >= 8)
	{
		return PLANTING_NOT_HERE;
	}

	return PLANTING_OK;
}

//0x425690
void Challenge::InitZombieWavesSurvival()
{
	mBoard->mZombieAllowed[ZOMBIE_NORMAL] = true;
	MTRand aLevelRNG = MTRand(mBoard->GetLevelRandSeed());

	if (aLevelRNG.Next((unsigned int)5) == 0)
	{
		mBoard->mZombieAllowed[ZOMBIE_NEWSPAPER] = true;
	}
	else
	{
		mBoard->mZombieAllowed[ZOMBIE_TRAFFIC_CONE] = true;
	}

	int aCapacity = std::min(mSurvivalStage + 1, 9);
	while (aCapacity > 0)
	{
		ZombieType aRandZombie = (ZombieType)aLevelRNG.Next((unsigned int)NUM_ZOMBIE_TYPES);
		if (mBoard->mZombieAllowed[aRandZombie])																	continue;
		if (mBoard->IsZombieTypePoolOnly(aRandZombie) && !mBoard->StageHasPool())									continue;
		if (mBoard->StageHasRoof() && (aRandZombie == ZOMBIE_DIGGER || aRandZombie == ZOMBIE_DANCER))				continue;
		if (mBoard->StageHasGraveStones() && aRandZombie == ZOMBIE_ZAMBONI)											continue;
		if (!mBoard->StageHasRoof() && !mApp->IsSurvivalEndless(mApp->mGameMode) && aRandZombie == ZOMBIE_BUNGEE)	continue;
		if (mBoard->GetSurvivalFlagsCompleted() < 10 && aRandZombie == ZOMBIE_REDEYE_GARGANTUAR)								continue;
		if (mApp->IsSurvivalNormal(mApp->mGameMode) && aRandZombie > ZOMBIE_SNORKEL)								continue;
		if (mBoard->IsZombieTypeSpawnedOnly(aRandZombie) || Zombie::IsZombotany(aRandZombie) ||
			aRandZombie == ZOMBIE_DUCKY_TUBE || aRandZombie == ZOMBIE_YETI)											continue;

		mBoard->mZombieAllowed[aRandZombie] = true;
		aCapacity--;
	}
}

void Challenge::InitZombieWavesFromList(ZombieType* theZombieList, int theListLength)
{
	for (int i = 0; i < theListLength; i++)
	{
		mBoard->mZombieAllowed[(int)theZombieList[i]] = true;
	}
}

namespace
{
// 模式→允许僵尸位掩码表（B 形态）：GameMode 直下标，1ULL<<ZombieType 逐位对应 mZombieAllowed。
// 未列模式（含 I_ZOMBIE/SCARY_POTTER/ZEN_GARDEN 等）= kZombieMaskDefault（原 else 组）。
// 表外：生存 [1..15]（RNG 生成，见调用点）与冒险 [0]（关卡重定向链，见调用点）。
constexpr uint64_t kZombieMaskDefault =
	(1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_PAIL);
constexpr uint64_t kModeZombieMask[GameMode::NUM_GAME_MODES] = {
	/* [ 0] GAMEMODE_ADVENTURE  (v1 0) */ kZombieMaskDefault,
	/* [ 1] GAMEMODE_MINIGAME_BEGHOULED  (v1 20) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_PAIL) | (1ULL << ZOMBIE_DOOR) | (1ULL << ZOMBIE_FOOTBALL) | (1ULL << ZOMBIE_NEWSPAPER),
	/* [ 2] GAMEMODE_MINIGAME_BEGHOULED_TWIST  (v1 24) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_PAIL) | (1ULL << ZOMBIE_DOOR) | (1ULL << ZOMBIE_FOOTBALL) | (1ULL << ZOMBIE_NEWSPAPER),
	/* [ 3] GAMEMODE_MINIGAME_BOBSLED_BONANZA  (v1 28) */ (1ULL << ZOMBIE_BOBSLED) | (1ULL << ZOMBIE_ZAMBONI),
	/* [ 4] GAMEMODE_MINIGAME_COLUMN  (v1 27) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_PAIL) | (1ULL << ZOMBIE_FOOTBALL),
	/* [ 5] GAMEMODE_MINIGAME_FINAL_BOSS  (v1 35) */ kZombieMaskDefault,
	/* [ 6] GAMEMODE_MINIGAME_INVISIGHOUL  (v1 21) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_PAIL) | (1ULL << ZOMBIE_DOLPHIN_RIDER) | (1ULL << ZOMBIE_ZAMBONI) | (1ULL << ZOMBIE_JACK_IN_THE_BOX),
	/* [ 7] GAMEMODE_MINIGAME_LAST_STAND  (v1 31) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_PAIL) | (1ULL << ZOMBIE_DOOR) | (1ULL << ZOMBIE_FOOTBALL) | (1ULL << ZOMBIE_NEWSPAPER) | (1ULL << ZOMBIE_JACK_IN_THE_BOX) | (1ULL << ZOMBIE_POLEVAULTER) | (1ULL << ZOMBIE_DOLPHIN_RIDER) | (1ULL << ZOMBIE_LADDER),
	/* [ 8] GAMEMODE_MINIGAME_LITTLE_TROUBLE  (v1 25) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_FOOTBALL) | (1ULL << ZOMBIE_SNORKEL),
	/* [ 9] GAMEMODE_MINIGAME_POGO_PARTY  (v1 34) */ (1ULL << ZOMBIE_POGO),
	/* [10] GAMEMODE_MINIGAME_PORTAL_COMBAT  (v1 26) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_PAIL) | (1ULL << ZOMBIE_FOOTBALL) | (1ULL << ZOMBIE_BALLOON),
	/* [11] GAMEMODE_MINIGAME_RAINING_SEEDS  (v1 19) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_PAIL) | (1ULL << ZOMBIE_DOOR) | (1ULL << ZOMBIE_FOOTBALL) | (1ULL << ZOMBIE_NEWSPAPER) | (1ULL << ZOMBIE_JACK_IN_THE_BOX) | (1ULL << ZOMBIE_BUNGEE),
	/* [12] GAMEMODE_MINIGAME_SEEING_STARS  (v1 22) */ kZombieMaskDefault,
	/* [13] GAMEMODE_MINIGAME_SLOT_MACHINE  (v1 18) */ kZombieMaskDefault,
	/* [14] GAMEMODE_MINIGAME_SPEED  (v1 29) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_DOLPHIN_RIDER) | (1ULL << ZOMBIE_POLEVAULTER),
	/* [15] GAMEMODE_MINIGAME_WALLNUT_BOWLING  (v1 17) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_PAIL) | (1ULL << ZOMBIE_POLEVAULTER) | (1ULL << ZOMBIE_NEWSPAPER),
	/* [16] GAMEMODE_MINIGAME_WALLNUT_BOWLING_2  (v1 33) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_PAIL) | (1ULL << ZOMBIE_POLEVAULTER) | (1ULL << ZOMBIE_NEWSPAPER) | (1ULL << ZOMBIE_DANCER) | (1ULL << ZOMBIE_DOOR),
	/* [17] GAMEMODE_MINIGAME_WAR_AND_PEAS  (v1 16) */ (1ULL << ZOMBIE_PEA_HEAD) | (1ULL << ZOMBIE_WALLNUT_HEAD),
	/* [18] GAMEMODE_MINIGAME_WAR_AND_PEAS_2  (v1 32) */ (1ULL << ZOMBIE_PEA_HEAD) | (1ULL << ZOMBIE_WALLNUT_HEAD) | (1ULL << ZOMBIE_JALAPENO_HEAD) | (1ULL << ZOMBIE_GATLING_HEAD) | (1ULL << ZOMBIE_SQUASH_HEAD) | (1ULL << ZOMBIE_TALLNUT_HEAD),
	/* [19] GAMEMODE_MINIGAME_WHACK_A_ZOMBIE  (v1 30) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_PAIL),
	/* [20] GAMEMODE_MINIGAME_ZOMBIQUARIUM  (v1 23) */ kZombieMaskDefault,
	/* [21] GAMEMODE_LIMBO_AIR_RAID  (v1 41) */ (1ULL << ZOMBIE_BALLOON),
	/* [22] GAMEMODE_LIMBO_ART_CHALLENGE_SUNFLOWER  (v1 40) */ kZombieMaskDefault,
	/* [23] GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT  (v1 36) */ kZombieMaskDefault,
	/* [24] GAMEMODE_LIMBO_BIG_TIME  (v1 39) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_PAIL) | (1ULL << ZOMBIE_DOOR) | (1ULL << ZOMBIE_FOOTBALL) | (1ULL << ZOMBIE_JACK_IN_THE_BOX),
	/* [25] GAMEMODE_LIMBO_BUNGEE_BLITZ  (v1 48) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_PAIL) | (1ULL << ZOMBIE_LADDER),
	/* [26] GAMEMODE_LIMBO_GRAVE_DANGER  (v1 45) */ kZombieMaskDefault,
	/* [27] GAMEMODE_LIMBO_HIGH_GRAVITY  (v1 44) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_PAIL) | (1ULL << ZOMBIE_DOOR) | (1ULL << ZOMBIE_BALLOON),
	/* [28] GAMEMODE_LIMBO_ICE  (v1 42) */ kZombieMaskDefault,
	/* [29] GAMEMODE_LIMBO_RESODDED  (v1 38) */ kZombieMaskDefault,
	/* [30] GAMEMODE_LIMBO_SHOVEL  (v1 46) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE),
	/* [31] GAMEMODE_LIMBO_SQUIRREL  (v1 49) */ kZombieMaskDefault,
	/* [32] GAMEMODE_LIMBO_STORMY_NIGHT  (v1 47) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_DOLPHIN_RIDER) | (1ULL << ZOMBIE_BALLOON),
	/* [33] GAMEMODE_LIMBO_SUNNY_DAY  (v1 37) */ (1ULL << ZOMBIE_NORMAL) | (1ULL << ZOMBIE_PAIL) | (1ULL << ZOMBIE_TRAFFIC_CONE) | (1ULL << ZOMBIE_FOOTBALL) | (1ULL << ZOMBIE_POLEVAULTER) | (1ULL << ZOMBIE_JACK_IN_THE_BOX),
	/* [34] GAMEMODE_LIMBO_ZEN_GARDEN  (v1 43) */ kZombieMaskDefault,
	/* [35] GAMEMODE_INTRO  (v1 71) */ kZombieMaskDefault,
	/* [36] GAMEMODE_PUZZLE_I_ZOMBIE_1  (v1 61) */ kZombieMaskDefault,
	/* [37] GAMEMODE_PUZZLE_I_ZOMBIE_2  (v1 62) */ kZombieMaskDefault,
	/* [38] GAMEMODE_PUZZLE_I_ZOMBIE_3  (v1 63) */ kZombieMaskDefault,
	/* [39] GAMEMODE_PUZZLE_I_ZOMBIE_4  (v1 64) */ kZombieMaskDefault,
	/* [40] GAMEMODE_PUZZLE_I_ZOMBIE_5  (v1 65) */ kZombieMaskDefault,
	/* [41] GAMEMODE_PUZZLE_I_ZOMBIE_6  (v1 66) */ kZombieMaskDefault,
	/* [42] GAMEMODE_PUZZLE_I_ZOMBIE_7  (v1 67) */ kZombieMaskDefault,
	/* [43] GAMEMODE_PUZZLE_I_ZOMBIE_8  (v1 68) */ kZombieMaskDefault,
	/* [44] GAMEMODE_PUZZLE_I_ZOMBIE_9  (v1 69) */ kZombieMaskDefault,
	/* [45] GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS  (v1 70) */ kZombieMaskDefault,
	/* [46] GAMEMODE_SCARY_POTTER_1  (v1 51) */ kZombieMaskDefault,
	/* [47] GAMEMODE_SCARY_POTTER_2  (v1 52) */ kZombieMaskDefault,
	/* [48] GAMEMODE_SCARY_POTTER_3  (v1 53) */ kZombieMaskDefault,
	/* [49] GAMEMODE_SCARY_POTTER_4  (v1 54) */ kZombieMaskDefault,
	/* [50] GAMEMODE_SCARY_POTTER_5  (v1 55) */ kZombieMaskDefault,
	/* [51] GAMEMODE_SCARY_POTTER_6  (v1 56) */ kZombieMaskDefault,
	/* [52] GAMEMODE_SCARY_POTTER_7  (v1 57) */ kZombieMaskDefault,
	/* [53] GAMEMODE_SCARY_POTTER_8  (v1 58) */ kZombieMaskDefault,
	/* [54] GAMEMODE_SCARY_POTTER_9  (v1 59) */ kZombieMaskDefault,
	/* [55] GAMEMODE_SCARY_POTTER_ENDLESS  (v1 60) */ kZombieMaskDefault,
	/* [56] GAMEMODE_SURVIVAL_ENDLESS_STAGE_1  (v1 11) */ kZombieMaskDefault,
	/* [57] GAMEMODE_SURVIVAL_ENDLESS_STAGE_2  (v1 12) */ kZombieMaskDefault,
	/* [58] GAMEMODE_SURVIVAL_ENDLESS_STAGE_3  (v1 13) */ kZombieMaskDefault,
	/* [59] GAMEMODE_SURVIVAL_ENDLESS_STAGE_4  (v1 14) */ kZombieMaskDefault,
	/* [60] GAMEMODE_SURVIVAL_ENDLESS_STAGE_5  (v1 15) */ kZombieMaskDefault,
	/* [61] GAMEMODE_SURVIVAL_HARD_STAGE_1  (v1 6) */ kZombieMaskDefault,
	/* [62] GAMEMODE_SURVIVAL_HARD_STAGE_2  (v1 7) */ kZombieMaskDefault,
	/* [63] GAMEMODE_SURVIVAL_HARD_STAGE_3  (v1 8) */ kZombieMaskDefault,
	/* [64] GAMEMODE_SURVIVAL_HARD_STAGE_4  (v1 9) */ kZombieMaskDefault,
	/* [65] GAMEMODE_SURVIVAL_HARD_STAGE_5  (v1 10) */ kZombieMaskDefault,
	/* [66] GAMEMODE_SURVIVAL_NORMAL_STAGE_1  (v1 1) */ kZombieMaskDefault,
	/* [67] GAMEMODE_SURVIVAL_NORMAL_STAGE_2  (v1 2) */ kZombieMaskDefault,
	/* [68] GAMEMODE_SURVIVAL_NORMAL_STAGE_3  (v1 3) */ kZombieMaskDefault,
	/* [69] GAMEMODE_SURVIVAL_NORMAL_STAGE_4  (v1 4) */ kZombieMaskDefault,
	/* [70] GAMEMODE_SURVIVAL_NORMAL_STAGE_5  (v1 5) */ kZombieMaskDefault,
	/* [71] GAMEMODE_TREE_OF_WISDOM  (v1 50) */ kZombieMaskDefault,
	/* [72] GAMEMODE_CUSTOM_LEVEL（哨兵；策略层覆写）*/ kZombieMaskDefault,
};
static_assert(sizeof(kModeZombieMask) / sizeof(kModeZombieMask[0]) == (int)GameMode::NUM_GAME_MODES,
	"模式→僵尸掩码表槽位须与 GameMode 枚举尾同步");

// 位掩码展开到 mZombieAllowed（仅置 true，不触碰其余槽位，与原逐句赋值等价）
void ApplyZombieMask(bool* theList, uint64_t theMask)
{
	for (int aType = 0; aType < NUM_ZOMBIE_TYPES; aType++)
	{
		if (theMask & (1ULL << aType))
		{
			theList[aType] = true;
		}
	}
}
}  // namespace

//0x425840
void Challenge::InitZombieWaves()
{
	GameMode aGameMode = mApp->mGameMode;
	bool* aList = mBoard->mZombieAllowed;
	if (mApp->IsSurvivalMode())
	{
		if (mSurvivalStage == 0)
		{
			if (mApp->IsSurvivalNormal(mApp->mGameMode))
			{
				aList[ZOMBIE_NORMAL] = true;
				aList[ZOMBIE_TRAFFIC_CONE] = true;
			}
			else
			{
				aList[ZOMBIE_NORMAL] = true;
				aList[ZOMBIE_TRAFFIC_CONE] = true;
				aList[ZOMBIE_PAIL] = true;
			}
		}
		else InitZombieWavesSurvival();
	}
	else if (mApp->IsAdventureMode())
	{
		// 冒险模式：关卡重定向链（表外特例；45/25/15/5/40 级各至多命中一个，次序无冲突）；
		// 重定向模式直接复用对应模式槽的掩码，未重定向关卡 = 默认组
		if (mApp->IsBungeeBlitzLevel())
			ApplyZombieMask(aList, kModeZombieMask[GAMEMODE_LIMBO_BUNGEE_BLITZ]);
		else if (mApp->IsLittleTroubleLevel())
			ApplyZombieMask(aList, kModeZombieMask[GAMEMODE_MINIGAME_LITTLE_TROUBLE]);
		else if (mApp->IsWhackAZombieLevel())
			ApplyZombieMask(aList, kModeZombieMask[GAMEMODE_MINIGAME_WHACK_A_ZOMBIE]);
		else if (mApp->IsWallnutBowlingLevel())
			ApplyZombieMask(aList, kModeZombieMask[GAMEMODE_MINIGAME_WALLNUT_BOWLING]);
		else if (mApp->IsStormyNightLevel())
			ApplyZombieMask(aList, kModeZombieMask[GAMEMODE_LIMBO_STORMY_NIGHT]);
		else
			ApplyZombieMask(aList, kZombieMaskDefault);
	}
	else
	{
		// B 形态 -> 位掩码表（上方 kModeZombieMask，GameMode 直下标；未列模式 = 默认组）
		ApplyZombieMask(aList, kModeZombieMask[aGameMode]);
	}

	if (mApp->CanSpawnYetis() && !mApp->IsWhackAZombieLevel() && !mApp->IsLittleTroubleLevel())
		aList[ZOMBIE_YETI] = true;
}

//0x425DA0
void Challenge::WhackAZombiePlaceGraves(int theGraveCount)
{
	int aPickCount = 0;
	TodWeightedGridArray aPicks[36];

	for (int aCol = 3; aCol < MAX_GRID_SIZE_X; aCol++)
	{
		for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
		{
			if (!mBoard->CanAddGraveStoneAt(aCol, aRow))
				continue;

			TodWeightedGridArray* aPick = &aPicks[aPickCount++];
			aPick->mWeight = mBoard->GetTopPlantAt(aCol, aRow, TOPPLANT_ANY) ? 1 : 100000;
			aPick->mX = aCol;
			aPick->mY = aRow;
		}
	}

	if (theGraveCount > aPickCount)
		theGraveCount = aPickCount;

	if (aPickCount == 0 || theGraveCount <= 0)
		return;

	for (int i = 0; i < theGraveCount; i++)
	{
		TodWeightedGridArray* aPick = TodPickFromWeightedGridArray(aPicks, aPickCount);

		// 格桶链枚举（2026-10-03 第二批）：原为全池扫 mPlantCol==aPick->mX
		// && mRow==aPick->mY 同格全层全株 Die（墓碑压植物）——改该格
		// 4 层链全成员（KillAllPlantsNearDoom 同款；含影子株=原池可见集；
		// 原不过滤 NotOnGround，链枚举也不过滤；Die 不摘链，防御性前取）。
		for (int aLayer = 0; aLayer < PLANT_CELL_COUNT; aLayer++)
		{
			Plant* aPlant = mBoard->GetCellPlant(aPick->mY, aPick->mX, aLayer);
			while (aPlant != nullptr)
			{
				Plant* aNext = mBoard->NextCellPlant(aPlant);  // 先取：Die 不摘链但防御性前取
				aPlant->Die(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
				aPlant = aNext;
			}
		}

		mBoard->AddAGraveStone(aPick->mX, aPick->mY);
		aPick->mWeight = 0;
	}
}

//0x425FF0
void Challenge::WhackAZombieSpawning()
{
	TOD_ASSERT(mApp->IsWhackAZombieLevel());
	TOD_ASSERT(mBoard->mCurrentWave >= 0 && mBoard->mCurrentWave <= mBoard->mNumWaves);

	if (mBoard->mCurrentWave == mBoard->mNumWaves && mBoard->mZombieCountDown == 0)
		return;

	mBoard->mZombieCountDown--;
	if (mBoard->mZombieCountDown == 100 && mBoard->mCurrentWave > 0)
	{
		int aNumGraves = 5 - mBoard->GetGraveStonesCount();
		WhackAZombiePlaceGraves(std::max(1, aNumGraves));
	}
	if (mBoard->mZombieCountDown == 5)
	{
		mBoard->NextWaveComing();
	}
	if (mBoard->mZombieCountDown == 0)
	{
		mBoard->mZombieCountDown = 2000;
		mBoard->mZombieCountDownStart = mBoard->mZombieCountDown;
		mBoard->mCurrentWave++;
		mChallengeStateCounter = mBoard->mCurrentWave == mBoard->mNumWaves ? 300 : 1;
	}
	else if (mBoard->mZombieCountDown < 300)
	{
		return;
	}

	if (--mChallengeStateCounter == 0)
	{
		// 根据当前波数计算当前处于的阶段
		int aPhase = ClampInt((mBoard->mCurrentWave - 1) * 6 / 12, 0, 5);
		// 在不同阶段下出现路障、铁桶僵尸及出现二、三只僵尸的权重
        static constexpr int aDoubleChance[6] = { 0, 30, 10, 10, 15, 18 };
		static constexpr int aTripleChance[6] = { 0, 0, 0, 0, 10, 13 };
		static constexpr int aPailChance[6] = { 0, 0, 0, 10, 15, 15 };
		static constexpr int aConeChance[6] = { 0, 0, 30, 30, 30, 30 };
		// 默认的僵尸数量为 1 只
		int aZombieCount = 1;
		// 默认的僵尸类型为普通僵尸
		ZombieType aZombieType = ZOMBIE_NORMAL;
		// 分别取僵尸类型及僵尸数量的随机值
		int aNumHit = Rand(100);
		int aTypeHit = Rand(100);
		// 判断是否为最后一波
		int aIsFinalWave = mBoard->mCurrentWave == mBoard->mNumWaves;

		// 确定僵尸数量
		if (aIsFinalWave)
		{
			aZombieCount = 20;
		}
		else if (aNumHit < aTripleChance[aPhase])
		{
			aZombieCount = 3;
		}
		else if (aNumHit < aTripleChance[aPhase] + aDoubleChance[aPhase])
		{
			aZombieCount = 2;
		}

		// 确定僵尸类型
		if (aTypeHit < aPailChance[aPhase] && aZombieCount < 3)
		{
			aZombieType = ZOMBIE_PAIL;
		}
		else if (aTypeHit < aPailChance[aPhase] + aConeChance[aPhase])
		{
			aZombieType = ZOMBIE_TRAFFIC_CONE;
		}

		TodWeightedArray aGridPicks[MAX_GRAVE_STONES];
		int aGridPicksCount = 0;

		GridItem* aGridItem = nullptr;
		while(mBoard->IterateGridItems(aGridItem))
		{
			if (aGridItem->mGridItemType == GRIDITEM_GRAVESTONE)
			{
				Plant* aPlant = mBoard->GetTopPlantAt(aGridItem->mGridX, aGridItem->mGridY, TOPPLANT_ONLY_NORMAL_POSITION);
				if (aPlant == nullptr || aPlant->mSeedType != SEED_GRAVEBUSTER)
				{
					aGridPicks[aGridPicksCount].mItem = (intptr_t)aGridItem;
					aGridPicks[aGridPicksCount].mWeight = 1;
					aGridPicksCount++;
				}
			}
		}
		float aMaxSpeed = TodAnimateCurve(1, 12, mBoard->mCurrentWave, 1, 3, Curves::EaseIn);
		if (aZombieCount > aGridPicksCount)
		{
			aZombieCount = aGridPicksCount;
		}

		for (int i = 0; i < aZombieCount; i++)
		{
			TodWeightedArray* aGrid = TodPickArrayItemFromWeightedArray(aGridPicks, aGridPicksCount);
			GridItem* aGraveStone = (GridItem*)aGrid->mItem;
			aGrid->mWeight = 0;

			if (aIsFinalWave)
			{
				aZombieType = Rand(2) == 0 ? ZOMBIE_TRAFFIC_CONE : ZOMBIE_PAIL;
				aMaxSpeed = 2;
			}

			Zombie* aZombie = mBoard->AddZombie(aZombieType, mBoard->mCurrentWave);
			if (aZombie == nullptr)
				break;

			aZombie->RiseFromGrave(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), aGraveStone->mGridX, aGraveStone->mGridY);
			aZombie->mPhaseCounter = 50;
			aZombie->mVelX = RandRangeFloat(0.5f, aMaxSpeed);
			aZombie->UpdateAnimSpeed(static_cast<std::int64_t>(mBoard->mLevelTimeNs));
		}

		int aStateCounterMin = TodAnimateCurve(1, 12, mBoard->mCurrentWave, 100, 30, Curves::Linear);
		int aStateCounterMax = TodAnimateCurve(1, 12, mBoard->mCurrentWave, 200, 60, Curves::Linear);
		mChallengeStateCounter = RandRangeInt(aStateCounterMin, aStateCounterMax);
		if (aIsFinalWave)
		{
			mBoard->mZombieCountDown = 0;
			mChallengeStateCounter = 0;
		}
	}
}

//0x426580
ZombieSpawningUpdateResult Challenge::UpdateZombieSpawning()
{
	if (mApp->IsWhackAZombieLevel())
	{
		WhackAZombieSpawning();
		return ZombieSpawningUpdateResult::HANDLED_BY_CHALLENGE;
	}

	if (mApp->IsFinalBossLevel() ||
		mApp->mGameMode == GAMEMODE_LIMBO_ICE ||
		mApp->mGameMode == GAMEMODE_LIMBO_ZEN_GARDEN ||
		mApp->mGameMode == GAMEMODE_TREE_OF_WISDOM ||
		mApp->mGameMode == GAMEMODE_MINIGAME_ZOMBIQUARIUM ||
		mApp->IsIZombieLevel() ||
		mApp->IsSquirrelLevel() ||
		mApp->IsScaryPotterLevel() ||
		(mApp->mGameMode == GAMEMODE_MINIGAME_LAST_STAND && mChallengeState != STATECHALLENGE_LAST_STAND_ONSLAUGHT))
	{
		return ZombieSpawningUpdateResult::HANDLED_BY_CHALLENGE;
	}

	return ZombieSpawningUpdateResult::USE_BOARD_WAVE_SCHEDULER;
}

//0x426620
void Challenge::GraveDangerSpawnGraveAt(int theGridX, int theGridY)
{
	// 格桶链枚举（2026-10-03 第二批）：原为全池扫 mPlantCol==theGridX
	// && mRow==theGridY 同格全层全株 Die（墓碑让位）——改该格 4 层链
	// 全成员（KillAllPlantsNearDoom 同款；含影子株=原池可见集；原不过滤
	// NotOnGround，链枚举也不过滤；Die 不摘链，防御性前取）。
	for (int aLayer = 0; aLayer < PLANT_CELL_COUNT; aLayer++)
	{
		Plant* aPlant = mBoard->GetCellPlant(theGridY, theGridX, aLayer);
		while (aPlant != nullptr)
		{
			Plant* aNext = mBoard->NextCellPlant(aPlant);  // 先取：Die 不摘链但防御性前取
			aPlant->Die(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
			aPlant = aNext;
		}
	}

	mBoard->mEnableGraveStones = true;
	GridItem* aGraveStone = mBoard->AddAGraveStone(theGridX, theGridY);
	if (aGraveStone)
	{
		aGraveStone->AddGraveStoneParticles();
	}
}

//0x4266C0
void Challenge::GraveDangerSpawnRandomGrave()
{
	int aPickCount = 0;
	TodWeightedGridArray aPicks[30];

	for (int aCol = 4; aCol < MAX_GRID_SIZE_X; aCol++)
	{
		for (int aRow = 0; aRow < MAX_GRID_SIZE_Y; aRow++)
		{
			if (mBoard->CanAddGraveStoneAt(aCol, aRow))
			{
				aPicks[aPickCount].mWeight = mBoard->GetTopPlantAt(aCol, aRow, TOPPLANT_ANY) ? 1 : 100000;
				aPicks[aPickCount].mX = aCol;
				aPicks[aPickCount].mY = aRow;
				aPickCount++;
			}
		}
	}

	if (aPickCount)
	{
		TodWeightedGridArray* aPick = TodPickFromWeightedGridArray(aPicks, aPickCount);
		GraveDangerSpawnGraveAt(aPick->mX, aPick->mY);
	}
}

//0x426850
void Challenge::SpawnZombieWave()
{
	if (mApp->IsContinuousChallenge() && mBoard->mCurrentWave == mBoard->mNumWaves)
	{
		mBoard->mCurrentWave = mBoard->mNumWaves - 1;
		for (int i = 0; i < MAX_ZOMBIES_IN_WAVE; i++)
		{
			ZombieType aWaveZombie = mBoard->mZombiesInWave[mBoard->mCurrentWave][i];
			if (aWaveZombie == ZOMBIE_INVALID)
			{
				break;
			}
			if (aWaveZombie == ZOMBIE_FLAG)
			{
				mBoard->mZombiesInWave[mBoard->mCurrentWave][i] = ZOMBIE_NORMAL;
			}
		}
	}

	int aIsFlagWave = mBoard->IsFlagWave(mBoard->mCurrentWave);
	if (mApp->mGameMode == GAMEMODE_LIMBO_GRAVE_DANGER && mBoard->mCurrentWave != mBoard->mNumWaves - 1)
	{
		if (aIsFlagWave)
		{
			mBoard->SpawnZombiesFromGraves();
		}
		else if (mBoard->mCurrentWave > 5)
		{
			GraveDangerSpawnRandomGrave();
		}
	}
	if (mApp->IsSurvivalMode() && mBoard->mBackground == BACKGROUND_2_NIGHT && mBoard->mCurrentWave == mBoard->mNumWaves - 1)
	{
		int aNumGraves = mBoard->GetGraveStonesCount();
		const int aGraveLimit = mApp->IsSurvivalNormal(mApp->mGameMode) ? 8 : 12;
		if (aNumGraves < aGraveLimit)
		{
			GraveDangerSpawnRandomGrave();
		}
	}
	if (mApp->IsBungeeBlitzLevel() && aIsFlagWave)
	{
		mBoard->DisplayAdvice(gStr.ADVICE_BUNGEES_INCOMING, MESSAGE_STYLE_HINT_FAST, ADVICE_NONE);
	}
}

//0x426A20
void Challenge::DrawStormFlash(Graphics* g, std::int64_t theTimeNs, int theMaxAmount)
{
	MTRand aDrawRand = MTRand(static_cast<unsigned long>(mBoard->mLevelTimeNs / 60'000'000));
	int aDarkness = TodAnimateCurveNs(1'500'000'000, 0, theTimeNs, 255 - theMaxAmount, 255, Curves::Linear) + aDrawRand.NextNoAssert((unsigned int)64) - 32;
	// 设置暴风雨阴暗的颜色
	g->mColor = Color(0, 0, 0, ClampInt(aDarkness, 0, 255));
	// 绘制暴风雨阴暗的主色
	g->FillRect(-1000, -1000, 2800, 2600);

	// 设置闪电亮光的颜色
	g->mColor = Color(255, 255, 255, TodAnimateCurveNs(1'500'000'000, 750'000'000, theTimeNs, theMaxAmount, 0, Curves::Linear));
	// 绘制闪光
	g->FillRect(-1000, -1000, 2800, 2600);
}

//0x426B20
void Challenge::DrawWeather(Graphics* g)
{
	if (mApp->IsStormyNightLevel() || mApp->mGameMode == GAMEMODE_MINIGAME_RAINING_SEEDS)
		DrawRain(g);

	if (mApp->IsStormyNightLevel())
		DrawStormNight(g);
}

//0x426B90
void Challenge::DrawRain(Graphics* g)
{
	if (mBoard->mCutScene->IsBeforePreloading())
		return;

	// 1073 00429640: positive board X includes one extra tile to the left.
	const int aBoardOffsetX = (mBoard->mX > 0 ? mBoard->mX + 100 : mBoard->mX) / 100 * -100;
	// 1073 按有符号 int 取模（反汇编 00429640: `*(int*)(...) % 0xa1`），负计数器的
	// 负分数经曲线钳制回起点。曾改 uint 读取取"模恒非负"——0x80000000 起的计数器
	// 无符号取模为正、雨纹相位整体偏移全屏不符 1073（PortableRainCoffeeSmoke
	// 对拍参照实证）。直接在 ns 中保留同一有符号回卷区间，不先舍入到 tick。
	const auto aUnsignedTimeNs = mBoard->mEffectCounter.mNanoseconds;
	const auto aTimeNs = static_cast<std::int64_t>(aUnsignedTimeNs) -
		(aUnsignedTimeNs >= (std::uint64_t(1) << 31) * 10'000'000
			? (std::int64_t(1) << 32) * 10'000'000 : 0);
	int aTimeOffsetXEst = TodAnimateCurveNs(0, 1'000'000'000, aTimeNs % 1'000'000'000, 0, -100, Curves::Linear);
	int aTimeOffsetYEst = TodAnimateCurveNs(0, 200'000'000, aTimeNs % 200'000'000, -100, 0, Curves::Linear);

	// 绘制远景的雨
	for (int aHorCnt = 0; aHorCnt < 9; aHorCnt++)
	{
		for (int aVerCnt = 0; aVerCnt < 7; aVerCnt++)
		{
			int aImageX = aTimeOffsetXEst + 100 * aHorCnt + aBoardOffsetX;
			int aImageY = aTimeOffsetYEst + 100 * aVerCnt;
			g->DrawImage(Sexy::IMAGE_RAIN, aImageX, aImageY);
		}
	}

	float aTimeOffsetXCls = TodAnimateCurveNs(0, 1'610'000'000, aTimeNs % 1'610'000'000, 0, -100, Curves::Linear);
	float aTimeOffsetYCls = TodAnimateCurveNs(0, 330'000'000, aTimeNs % 330'000'000, -100, 0, Curves::Linear);
	// 绘制近景的雨
	for (int aHorCnt = 0; aHorCnt < 9; aHorCnt++)
	{
		for (int aVerCnt = 0; aVerCnt < 7; aVerCnt++)
		{
			float aRainScaleCls = 1.5f;
			float aImageClsX = (aHorCnt * 100 + aTimeOffsetXCls) * aRainScaleCls + aBoardOffsetX;
			float aImageClsY = (aVerCnt * 100 + aTimeOffsetYCls) * aRainScaleCls;
			TodDrawImageScaledF(g, Sexy::IMAGE_RAIN, aImageClsX, aImageClsY, aRainScaleCls, aRainScaleCls);
		}
	}
}

//0x426E90
void Challenge::DrawStormNight(Graphics* g)
{
	if (mChallengeState == STATECHALLENGE_STORM_FLASH_1 && mChallengeStateCounter < 300)
	{
		if (mChallengeStateCounter > STORM_FLASH_TIME)
		{
			DrawStormFlash(g, mChallengeStateCounter.mNanoseconds - std::int64_t(STORM_FLASH_TIME) * 10'000'000, 255);
		}
		else
		{
			DrawStormFlash(g, mChallengeStateCounter.mNanoseconds, 92);
		}
	}
	else if (mChallengeState == STATECHALLENGE_STORM_FLASH_2 && mChallengeStateCounter < 300)
	{
		DrawStormFlash(g, mChallengeStateCounter.mNanoseconds / 2, 255);
	}
	else if (mChallengeState == STATECHALLENGE_STORM_FLASH_3 && mChallengeStateCounter < 150)
	{
		DrawStormFlash(g, mChallengeStateCounter.mNanoseconds, 255);
	}
	else
	{
		g->mColor = Color::Black;
		g->FillRect(-1000, -1000, BOARD_WIDTH + 2000, BOARD_HEIGHT + 2000);
	}

	mBoard->DrawUIBottom(g);
	mBoard->DrawTopRightUI(g);
}

//0x426F60
void Challenge::PlayBossEnter()
{
	// 1073 零售版实测（tools/dis1073.py @0x429a10，GOTY 地址系）：直呼
	// DataArrayAlloc + ZombieInitialize(0, ZOMBIE_BOSS, !Rand(5), null, 0)
	// ——僵王行号是字面量 0，不经 AddZombie/PickRowForNewZombie 随机挑选
	// （现代树此前误走随机行=移植偏离，2026-10-04 用户以原版行为纠正）。
	// AddZombieInRow(BOSS, 0, 0) 等价收编：池满守卫/变体骰与零售逐字一致。
	mBoard->AddZombieInRow(ZombieType::ZOMBIE_BOSS, 0, 0);
}

//0x426FC0
void Challenge::PortalStart()
{
	mChallengeStateCounter = 9000;
	GridItem* aPortal;

	aPortal = mBoard->mGridItems.DataArrayAlloc();
	aPortal->mGridItemType = GRIDITEM_PORTAL_SQUARE;
	aPortal->mGridX = 2;
	aPortal->mGridY = 0;
	aPortal->mRenderOrder = mBoard->MakeRenderOrder(RENDER_LAYER_PARTICLE, aPortal->mGridY, 0);
	aPortal->OpenPortal();

	aPortal = mBoard->mGridItems.DataArrayAlloc();
	aPortal->mGridItemType = GRIDITEM_PORTAL_SQUARE;
	aPortal->mGridX = 9;
	aPortal->mGridY = 1;
	aPortal->mRenderOrder = mBoard->MakeRenderOrder(RENDER_LAYER_PARTICLE, aPortal->mGridY, 0);
	aPortal->OpenPortal();

	aPortal = mBoard->mGridItems.DataArrayAlloc();
	aPortal->mGridItemType = GRIDITEM_PORTAL_CIRCLE;
	aPortal->mGridX = 9;
	aPortal->mGridY = 3;
	aPortal->mRenderOrder = mBoard->MakeRenderOrder(RENDER_LAYER_PARTICLE, aPortal->mGridY, 0);
	aPortal->OpenPortal();

	aPortal = mBoard->mGridItems.DataArrayAlloc();
	aPortal->mGridItemType = GRIDITEM_PORTAL_CIRCLE;
	aPortal->mGridX = 2;
	aPortal->mGridY = 4;
	aPortal->mRenderOrder = mBoard->MakeRenderOrder(RENDER_LAYER_PARTICLE, aPortal->mGridY, 0);
	aPortal->OpenPortal();

	mBoard->mZombieCountDown = 200;
	mBoard->mZombieCountDownStart = mBoard->mZombieCountDown;
	mConveyorBeltCounter = 200;
}

//0x4270A0
void Challenge::UpdatePortal(GridItem* thePortal)
{
	GridItem* anOtherPortal = GetOtherPortal(thePortal);
	if (anOtherPortal == nullptr)
		return;

	Zombie* aZombie = nullptr;
	while (mBoard->IterateZombies(aZombie))
	{
		if (aZombie->mRow == thePortal->mGridY && aZombie->mLastPortalX != thePortal->mGridX)
		{
			Rect aZombieRect = aZombie->GetZombieRect();
			int aZombieX = aZombieRect.mX + aZombieRect.mWidth / 2;
			int aPortalX = thePortal->mGridX * 80 + 25;
			if (abs(aZombieX - aPortalX) <= 45)
			{
				int aDiffX = aZombieX - (aZombie->mX);
				if (aZombie->IsWalkingBackwards()) aDiffX -= 60;
				{ aZombie->mX = (anOtherPortal->mGridX * 80 - aDiffX); aZombie->mRootBindingGeneration = 0; }

				aZombie->SetRow(anOtherPortal->mGridY);
				aZombie->mY = (aZombie->GetPosYBasedOnRow(anOtherPortal->mGridY));
				// Teleportation ends the old world-position segment. Zamboni
				// reuses X's slot for its tire-speed ramp, not position.
				if (aZombie->mZombieType != ZOMBIE_ZAMBONI)
					aZombie->mVelocityX.bound = false;
				aZombie->mVelocityY.bound = false;

				aZombie->mLastPortalX = anOtherPortal->mGridX;
			}
		}
	}

	Projectile* aProjectile = nullptr;
	while(mBoard->IterateProjectiles(aProjectile))
	{
		if (aProjectile->mMotionType == MOTION_STRAIGHT && aProjectile->mRow == thePortal->mGridY && aProjectile->mLastPortalX != thePortal->mGridX)
		{
			Rect aProjectileRect = aProjectile->GetProjectileRect();
			int aProjectileX = aProjectileRect.mX + aProjectileRect.mWidth / 2;
			int aPortalX = thePortal->mGridX * 80 + 55;
			if (abs(aProjectileX - aPortalX) <= 40)
			{
				int aDeltaY = (anOtherPortal->mGridY - thePortal->mGridY) * 100;
				aProjectile->mX += anOtherPortal->mGridX * 80 - aProjectileX + 60;
				aProjectile->mPosX = aProjectile->mX;

				aProjectile->mRow = anOtherPortal->mGridY;
				aProjectile->mY += aDeltaY;
				aProjectile->mPosY = aProjectile->mY;
				aProjectile->mShadowY += aDeltaY;

				aProjectile->mLastPortalX = anOtherPortal->mGridX;
				aProjectile->mRenderOrder = Board::MakeRenderOrder(RENDER_LAYER_PROJECTILE, aProjectile->mRow, 0);
			}
		}
	}

	LawnMower* aLawnMower = nullptr;
	while (mBoard->IterateLawnMowers(aLawnMower))
	{
		if (aLawnMower->mMowerState == MOWER_TRIGGERED && aLawnMower->mRow == thePortal->mGridY && aLawnMower->mLastPortalX != thePortal->mGridX)
		{
			int aMowerX = aLawnMower->mPosX + 45;
			int aPortalX = thePortal->mGridX * 80 + 25;
			if (abs(aMowerX - aPortalX) <= 20)
			{
				aLawnMower->mPosX = anOtherPortal->mGridX * 80 + 25;
				aLawnMower->mRow = anOtherPortal->mGridY;
				aLawnMower->mPosY = (anOtherPortal->mGridY - thePortal->mGridY) * 100;
				aLawnMower->mLastPortalX = anOtherPortal->mGridX;
				aLawnMower->mRenderOrder = Board::MakeRenderOrder(RENDER_LAYER_LAWN_MOWER, aLawnMower->mRow, 0);
				aLawnMower->Update();
			}
		}
	}
}

//0x4273C0
GridItem* Challenge::GetOtherPortal(GridItem* thePortal)
{
	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem != thePortal && aGridItem->mGridItemType == thePortal->mGridItemType)
		{
			return aGridItem;
		}
	}
	return nullptr;
}

//0x427410
GridItem* Challenge::GetPortalAt(int theGridX, int theGridY)
{
	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridX == theGridX && aGridItem->mGridY == theGridY && aGridItem->IsOpenPortal())
		{
			return aGridItem;
		}
	}
	return nullptr;
}

//0x427470
void Challenge::MoveAPortal()
{
	TodWeightedArray aPickArray[MAX_PORTALS];
	int aNumpicks = 0;

	GridItem* aGridItem = nullptr;
	while(mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->IsOpenPortal())
		{
			TOD_ASSERT(aNumpicks < MAX_PORTALS);
			aPickArray[aNumpicks].mWeight = 1;
			aPickArray[aNumpicks].mItem = (intptr_t)aGridItem;
			aNumpicks++;
		}
	}

	TOD_ASSERT(aNumpicks);
	GridItem* aPortal = (GridItem*)TodPickFromWeightedArray(aPickArray, aNumpicks);
	GridItem* aOtherPortal = GetOtherPortal(aPortal);
	TOD_ASSERT(aOtherPortal);

	TodWeightedGridArray aGridArray[MAX_PICK_GRID_SIZE];
	int aGridArrayCount = 0;
	for (int aGridX = 0; aGridX < 10; aGridX++)
	{
		for (int aGridY = 0; aGridY < 5; aGridY++)
		{
			// 1073 0x429FC5/0x429FCE: exclude the partner's row and the
			// relocating portal's old row; all columns 0..9 remain eligible.
			if (GetPortalAt(aGridX, aGridY) == nullptr &&
				aOtherPortal->mGridY != aGridY && aPortal->mGridY != aGridY)
			{
				TOD_ASSERT(aGridArrayCount < MAX_PICK_GRID_SIZE);
				aGridArray[aGridArrayCount].mX = aGridX;
				aGridArray[aGridArrayCount].mY = aGridY;
				aGridArray[aGridArrayCount].mWeight = 1;
				aGridArrayCount++;
			}
		}
	}

	TodWeightedGridArray* aGrid = TodPickFromWeightedGridArray(aGridArray, aGridArrayCount);
	GridItem* aNewPortal = mBoard->mGridItems.DataArrayAlloc();
	aNewPortal->mGridItemType = aPortal->mGridItemType;
	aNewPortal->mGridX = aGrid->mX;
	aNewPortal->mGridY = aGrid->mY;
	aNewPortal->mRenderOrder = Board::MakeRenderOrder(RENDER_LAYER_PARTICLE, aNewPortal->mGridY, 0);
	aNewPortal->OpenPortal();
	aPortal->ClosePortal();
}

//0x4275E0
void Challenge::UpdatePortalCombat()
{
	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->IsOpenPortal())
		{
			UpdatePortal(aGridItem);
		}
	}

	if (mBoard->HasLevelAwardDropped())
	{
		mBoard->ClearAdvice(ADVICE_PORTAL_RELOCATING);
	}
	else
	{
		mChallengeStateCounter--;
		if (mChallengeStateCounter == 500)
		{
			mBoard->DisplayAdviceAgain(gStr.ADVICE_PORTAL_RELOCATING, MESSAGE_STYLE_HINT_FAST, ADVICE_PORTAL_RELOCATING);
		}
		else if (mChallengeStateCounter <= 0)
		{
			mBoard->ClearAdvice(ADVICE_PORTAL_RELOCATING);
			mChallengeStateCounter = 6000;
			MoveAPortal();
		}
	}
}

//0x427800
float Challenge::PortalCombatRowSpawnWeight(int theGridY)
{
	if (GetPortalDistanceToMower(theGridY) < 5)
		return 0.01f;

	GridItem* aGridItem = 0;
	while (mBoard->IterateGridItems(aGridItem))
		if (aGridItem->IsOpenPortal() && aGridItem->mGridY == theGridY)
			return 1.0f;

	return 0.2f;
}

//0x427870
GridItem* Challenge::GetPortalToLeft(int theGridX, int theGridY)
{
	GridItem* aGridItemRecord = nullptr;

	GridItem* aGridItem = 0;
	while (mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->IsOpenPortal() && aGridItem->mGridX < theGridX && aGridItem->mGridY == theGridY)
		{
			if (aGridItemRecord == nullptr || aGridItemRecord->mGridX < aGridItem->mGridX)
			{
				aGridItemRecord = aGridItem;
			}
		}
	}

	return aGridItemRecord;
}

//0x4278E0
int Challenge::GetPortalDistanceToMower(int theGridY)
{
	int aGridX = 10;
	int aGridY = theGridY;
	int aDistance = 0;

	while (aDistance < 40)
	{
		GridItem* aPortal = GetPortalToLeft(aGridX, aGridY);
		if (aPortal == nullptr)
		{
			aDistance += aGridX;
			break;
		}

		GridItem* aOtherPortal = GetOtherPortal(aPortal);
		TOD_ASSERT(aOtherPortal);

		aDistance += aGridX - aPortal->mGridX;
		aGridX = aOtherPortal->mGridX;
		aGridY = aOtherPortal->mGridY;
	}

	return aDistance;
}

//0x427970
GridItem* Challenge::GetPortalToRight(int theGridX, int theGridY)
{
	GridItem* aGridItemRecord = nullptr;

	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->IsOpenPortal() && aGridItem->mGridX > theGridX && aGridItem->mGridY == theGridY)
		{
			if (!aGridItemRecord || aGridItemRecord->mGridX > aGridItem->mGridX)
			{
				aGridItemRecord = aGridItem;
			}
		}
	}

	return aGridItemRecord;
}

// BONUS
GridItem* Challenge::GetPortalLeftRight(int theGridX, int theGridY, int theToLeft)
{
	GridItem* aGridItemRecord = nullptr;

	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		int aPortalX = aGridItem->mGridX;
		if (aPortalX == theGridX)
		{
			continue;  // 排除传送门与 theGridX 在同一列的情况
		}

		int aIsDir = (aPortalX > theGridX) ^ theToLeft;
		if (aGridItem->IsOpenPortal() && aIsDir && aGridItem->mGridY == theGridY)
		{
			int aIsCls = (aGridItemRecord->mGridX > aPortalX) ^ theToLeft;
			if (!aGridItemRecord || aIsCls)
			{
				aGridItemRecord = aGridItem;
			}
		}
	}

	return aGridItemRecord;
}
// BONUS_END

//0x4279E0
int Challenge::CanTargetZombieWithPortals(Plant* thePlant, Zombie* theZombie)
{
	int aGridX = thePlant->mPlantCol;
	int aGridY = thePlant->mRow;

	for (int i = 0; i < 3; i++)
	{
		GridItem* aPortal = GetPortalToRight(aGridX, aGridY);
		if (aGridY == theZombie->mRow)
		{
			int aRangeLeft = aGridX * 80;
			int aRangeRight = aPortal ? aPortal->mGridX * 80 : 800;
			if ((theZombie->mX) > aRangeLeft&& (theZombie->mX) < aRangeRight)
				return true;
		}

		if (aPortal)
		{
			GridItem* aOtherPortal = GetOtherPortal(aPortal);
			if (aOtherPortal)
			{
				aGridX = aOtherPortal->mGridX;
				aGridY = aOtherPortal->mGridY;
				continue;
			}
		}
		break;
	}
	return false;
}

namespace
{
// Beghouled 升级卡表（B 形态）：mPacketType 匹配行 → 统一升级流程
// （置已购标记 → 场上基础植物 Die 后按升级卡重种 → 停用卡牌）。
// SHUFFLE/CRATER 为异构按钮动作，表外保留（见调用点分支）。
struct BeghouledUpgradeRow
{
	SeedType mPacketType;         // 升级卡（重种的目标卡）
	BeghouledUpgrade mUpgrade;    // 已购标记槽
	SeedType mBaseSeedType;       // 被替换的基础植物
};
constexpr BeghouledUpgradeRow kBeghouledUpgrades[3] = {
	{ SeedType::SEED_REPEATER,   BeghouledUpgrade::BEGHOULED_UPGRADE_REPEATER,   SeedType::SEED_PEASHOOTER },
	{ SeedType::SEED_FUMESHROOM, BeghouledUpgrade::BEGHOULED_UPGRADE_FUMESHROOM, SeedType::SEED_PUFFSHROOM },
	{ SeedType::SEED_TALLNUT,    BeghouledUpgrade::BEGHOULED_UPGRADE_TALLNUT,    SeedType::SEED_WALLNUT },
};
static_assert(sizeof(kBeghouledUpgrades) / sizeof(kBeghouledUpgrades[0]) == 3,
	"Beghouled 升级卡表须为 3 行（双发/大喷菇/高坚果）");
}  // namespace

//0x427A60
void Challenge::BeghouledPacketClicked(SeedPacket* theSeedPacket)
{
	/*
	SeedType aPacketType = theSeedPacket->mPacketType;
	int aPacketCost = mBoard->GetCurrentPlantCost(aPacketType, SEED_NONE);
	if (!mBoard->CanTakeSunMoney(aPacketCost))
		return;

	int aUpgrade = aPacketType == SEED_REPEATER ? 0 : aPacketType == SEED_FUMESHROOM ? 1 : aPacketType == SEED_TALLNUT ? 2 : -1;
	if (aPacketType == SEED_BEGHOULED_BUTTON_SHUFFLE)
	{
		if (mChallengeState == STATECHALLENGE_BEGHOULED_FALLING || mChallengeState == STATECHALLENGE_BEGHOULED_MOVING)
			return;
		BeghouledShuffle();
	}
	else if (aPacketType == SEED_BEGHOULED_BUTTON_CRATER)
	{
		if (!BeghouledCanClearCrater() || mChallengeState == STATECHALLENGE_BEGHOULED_FALLING || mChallengeState == STATECHALLENGE_BEGHOULED_MOVING)
			return;
		BeghouledClearCrater(1);
		BeghouledStartFalling(STATECHALLENGE_BEGHOULED_FALLING);
	}
	else if (aUpgrade != -1 && !mBeghouledPurcasedUpgrade[aUpgrade])
	{
		mBeghouledPurcasedUpgrade[aUpgrade] = true;
		static constexpr SeedType gUpgradableSeedTypes[3] = { SEED_PEASHOOTER, SEED_PUFFSHROOM, SEED_WALLNUT };
		SeedType aSeedPrimary = gUpgradableSeedTypes[aUpgrade];

		Plant* aPlant = nullptr;
		while (mBoard->IteratePlants(aPlant))
		{
			if (aPlant->mSeedType == aSeedPrimary)
			{
				aPlant->Die(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
				mBoard->AddPlant(aPlant->mPlantCol, aPlant->mRow, aPacketType, SEED_NONE);
			}
		}
		theSeedPacket->SetActivate(false);
	}
	*/

	int aPacketCost = mBoard->GetCurrentPlantCost(theSeedPacket->mPacketType, SEED_NONE);
	if (!mBoard->CanTakeSunMoney(aPacketCost))
		return;

	// B 形态 -> 常量表（上方 kBeghouledUpgrades）：三张升级卡统一流程；
	// 未匹配/已购 = 原链落空（继续走按钮分支或末尾扣阳光）
	const BeghouledUpgradeRow* aUpgradeRow = nullptr;
	for (const BeghouledUpgradeRow& aRow : kBeghouledUpgrades)
	{
		if (theSeedPacket->mPacketType == aRow.mPacketType)
		{
			aUpgradeRow = &aRow;
			break;
		}
	}

	if (aUpgradeRow != nullptr && !mBoard->mChallenge->mBeghouledPurcasedUpgrade[(int)aUpgradeRow->mUpgrade])
	{
		mBoard->mChallenge->mBeghouledPurcasedUpgrade[(int)aUpgradeRow->mUpgrade] = true;

		Plant* aPlant = nullptr;
		while (mBoard->IteratePlants(aPlant))
		{
			if (aPlant->mSeedType == aUpgradeRow->mBaseSeedType)
			{
				aPlant->Die(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
				mBoard->AddPlant(aPlant->mPlantCol, aPlant->mRow, theSeedPacket->mPacketType, SEED_NONE);
			}
		}
		theSeedPacket->SetActivate(false);
	}
	else if (theSeedPacket->mPacketType == SEED_BEGHOULED_BUTTON_SHUFFLE)
	{
		if (mChallengeState == STATECHALLENGE_BEGHOULED_FALLING || mChallengeState == STATECHALLENGE_BEGHOULED_MOVING)
			return;

		BeghouledShuffle();
	}
	else if (theSeedPacket->mPacketType == SEED_BEGHOULED_BUTTON_CRATER)
	{
		if (!BeghouledCanClearCrater() || mChallengeState == STATECHALLENGE_BEGHOULED_FALLING || mChallengeState == STATECHALLENGE_BEGHOULED_MOVING)
			return;

		BeghouledClearCrater(1);
		BeghouledStartFalling(STATECHALLENGE_BEGHOULED_FALLING);
	}

	mBoard->TakeSunMoney(aPacketCost);
}

//0x427C70
void Challenge::BeghouledShuffle()
{
	mBoard->ClearAdvice(ADVICE_NONE);

	Plant* aPlant = nullptr;
	while (mBoard->IteratePlants(aPlant))
	{
		aPlant->Die(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
	}

	BeghouledStartFalling(STATECHALLENGE_BEGHOULED_FALLING);
}

//0x427D00
int Challenge::BeghouledCanClearCrater()
{
	for (int aRow = 0; aRow < 5; aRow++)
	{
		for (int aCol = 0; aCol < 8; aCol++)
		{
			if (mBeghouledEated[aCol][aRow])
			{
				return true;
			}
		}
	}

	return false;
}

//0x427D30
Zombie* Challenge::ZombiquariumSpawnSnorkle()
{
	Zombie* aZombie = mBoard->AddZombieInRow(ZOMBIE_SNORKEL, 0, 0);
	{ aZombie->mX = (RandRangeFloat(50, 650)); aZombie->mRootBindingGeneration = 0; }
	aZombie->mY = (RandRangeFloat(100, 400));
	return aZombie;
}

//0x427DD0
void Challenge::ZombiquariumPacketClicked(SeedPacket* theSeedPacket)
{
	int aCost = mBoard->GetCurrentPlantCost(theSeedPacket->mPacketType, SEED_NONE);
	if (mBoard->CanTakeSunMoney(aCost))
	{
		if (theSeedPacket->mPacketType == SEED_ZOMBIQUARIUM_SNORKLE)
		{
			if (mBoard->CountZombiesOnScreen() > 100)
				return;

			if (mBoard->mTutorialState == TUTORIAL_ZOMBIQUARIUM_BUY_SNORKEL)
			{
				mBoard->ClearAdvice(ADVICE_ZOMBIQUARIUM_BUY_SNORKEL);
				mBoard->TutorialArrowRemove();
				mBoard->mTutorialState = TUTORIAL_ZOMBIQUARIUM_BOUGHT_SNORKEL;
			}

			Zombie* aZombie = ZombiquariumSpawnSnorkle();
			mApp->PlayFoley(FOLEY_ZOMBIESPLASH);
			mApp->AddTodParticle((aZombie->mX) + 60.0f, (aZombie->mY) + 20.0f, RENDER_LAYER_TOP, PARTICLE_PLANTING_POOL, static_cast<std::int64_t>(mBoard->mLevelTimeNs));
		}
		else if (theSeedPacket->mPacketType == SEED_ZOMBIQUARIUM_TROPHY)
		{
			SpawnLevelAward(2, 0);
			mBoard->ClearAdvice(ADVICE_NONE);
		}

		mBoard->TakeSunMoney(aCost);
	}
}

//0x427F60
void Challenge::ZombiquariumDropBrain(int x, int y)
{
	mBoard->ClearAdvice(ADVICE_ZOMBIQUARIUM_CLICK_TO_FEED);
	GridItem* aBrain = mBoard->mGridItems.DataArrayAlloc();
	aBrain->mGridItemType = GRIDITEM_BRAIN;
	aBrain->mRenderOrder = 400000;
	aBrain->mGridX = 0;
	aBrain->mGridY = 0;
	aBrain->mStateStartTimeNs = static_cast<std::int64_t>(mBoard->mLevelTimeNs);
	aBrain->mPosX = x - 15.0f;
	aBrain->mPosY = y - 15.0f;
	aBrain->mRootGeneration = 0;
	mApp->PlaySample(Sexy::SOUND_TAP);
}

//0x428010
void Challenge::ZombiquariumMouseDown(int x, int y)
{
	if (x < 80 || x > 720 || y < 90 || y > 430)
		return;

	int aBrainsCount = 0;
	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GRIDITEM_BRAIN)
		{
			aBrainsCount++;
		}
	}
	if (aBrainsCount < 3 && mBoard->TakeSunMoney(5))
	{
		ZombiquariumDropBrain(x, y);
	}
}

//0x4280A0
void Challenge::ZombiquariumUpdate()
{
	if (mBoard->mZombies.mSize == 0 && !mBoard->HasLevelAwardDropped())
	{
		mBoard->ZombiesWon(nullptr);
		return;
	}
	if (!mBoard->mAdvice->IsBeingDisplayed() && !mBoard->mHelpDisplayed[ADVICE_ZOMBIQUARIUM_COLLECT_SUN])
	{
		SexyString aMsg = TodReplaceNumberString(gStr.ADVICE_ZOMBIQUARIUM_COLLECT_SUN, UiTextPlaceholder::Score, ZOMBIQUARIUM_WINNING_SCORE);
		mBoard->DisplayAdvice(aMsg, MESSAGE_STYLE_HINT_TALL_FAST, ADVICE_ZOMBIQUARIUM_COLLECT_SUN);
	}

	int aScore = ClampInt(mBoard->mSunMoney, 0, ZOMBIQUARIUM_WINNING_SCORE);
	mBoard->mProgressMeterWidth = TodAnimateCurve(0, ZOMBIQUARIUM_WINNING_SCORE, aScore, 0, PROGRESS_METER_COUNTER, Curves::Linear);
	if (aScore >= ZOMBIQUARIUM_WINNING_SCORE - 100)
	{
		mBoard->DisplayAdvice(gStr.ADVICE_ALMOST_THERE, MESSAGE_STYLE_HINT_TALL_FAST, ADVICE_ALMOST_THERE);
	}
	if (aScore >= 110 && mBoard->mTutorialState == TUTORIAL_OFF)
	{
		mBoard->mTutorialState = TUTORIAL_ZOMBIQUARIUM_BUY_SNORKEL;
		float aPosX = mBoard->mSeedBank->mX + mBoard->mSeedBank->mSeedPackets[0].mX;
		float aPosY = mBoard->mSeedBank->mY + mBoard->mSeedBank->mSeedPackets[0].mY;
		mBoard->TutorialArrowShow(aPosX, aPosY);
		mBoard->DisplayAdvice(gStr.ADVICE_ZOMBIQUARIUM_BUY_SNORKEL, MESSAGE_STYLE_HINT_TALL_FAST, ADVICE_ZOMBIQUARIUM_BUY_SNORKEL);
	}
	else if (aScore < 100 && mBoard->mTutorialState == TUTORIAL_ZOMBIQUARIUM_BUY_SNORKEL)
	{
		mBoard->TutorialArrowRemove();
		mBoard->ClearAdvice(ADVICE_ZOMBIQUARIUM_BUY_SNORKEL);
		mBoard->mTutorialState = TUTORIAL_OFF;
	}
	if (aScore >= ZOMBIQUARIUM_WINNING_SCORE && mBoard->mTutorialState == TUTORIAL_ZOMBIQUARIUM_BOUGHT_SNORKEL)
	{
		mBoard->mTutorialState = TUTORIAL_ZOMBIQUARIUM_CLICK_TROPHY;
		float aPosX = mBoard->mSeedBank->mX + mBoard->mSeedBank->mSeedPackets[1].mX;
		float aPosY = mBoard->mSeedBank->mY + mBoard->mSeedBank->mSeedPackets[1].mY;
		mBoard->TutorialArrowShow(aPosX, aPosY);
		mBoard->DisplayAdvice(gStr.ADVICE_ZOMBIQUARIUM_CLICK_TROPHY, MESSAGE_STYLE_HINT_TALL_FAST, ADVICE_ZOMBIQUARIUM_CLICK_TROPHY);
	}
	else if (aScore < ZOMBIQUARIUM_WINNING_SCORE && mBoard->mTutorialState == TUTORIAL_ZOMBIQUARIUM_CLICK_TROPHY)
	{
		mBoard->TutorialArrowRemove();
		mBoard->ClearAdvice(ADVICE_ZOMBIQUARIUM_CLICK_TROPHY);
		// 1073 retains the purchase milestone, allowing the trophy hint to return.
		mBoard->mTutorialState = TUTORIAL_ZOMBIQUARIUM_BOUGHT_SNORKEL;
	}

	GridItem* aGridItem = nullptr;
	while(mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GRIDITEM_BRAIN)
		{
			// GRIDITEM_BRAIN and Stinky are mutually exclusive types. Reuse the
			// saved root-motion fields as stable Y origin / integer fall time /
			// last published Y / binding flag; never integrate rounded mPosY.
			if (!aGridItem->mRootGeneration ||
				aGridItem->mPosY != aGridItem->mRootLastOutputX)
			{
				aGridItem->mRootWorldX = aGridItem->mPosY;
				aGridItem->mRootPeriodNs = static_cast<std::int64_t>(mBoard->mLevelTimeNs)-static_cast<std::int64_t>(Sexy::gUpdateIntervalNs);
				aGridItem->mRootGeneration = 1;
			}
			aGridItem->mPosY = static_cast<float>(aGridItem->mRootWorldX +
				double(0.15f) * double(static_cast<std::int64_t>(mBoard->mLevelTimeNs)-aGridItem->mRootPeriodNs) / 10'000'000.0);
			aGridItem->mRootLastOutputX = aGridItem->mPosY;
			if (aGridItem->mPosY >= 500.0f)
			{
				aGridItem->GridItemDie();
			}
		}
	}
}

//0x428510
void Challenge::ShovelAddWallnuts()
{
	// GOTY 1073 (0042AF70): these day-board challenges have five rows, not six pool rows.
	for (int aCol = 0; aCol < MAX_GRID_SIZE_X; aCol++)
	{
		for (int aRow = 0; aRow < 5; aRow++)
		{
			mBoard->AddPlant(aCol, aRow, SEED_WALLNUT, SEED_NONE);
		}
	}
}

void Challenge::ScaryPotterDontPlaceInCol(int theCol, TodWeightedGridArray* theGridArray, int theGridArrayCount)
{
	for (int i = 0; i < theGridArrayCount; i++)
	{
		if (theGridArray[i].mX == theCol)
		{
			theGridArray[i].mWeight = 0;
		}
	}
}

void Challenge::ScaryPotterFillColumnWithPlant(int theCol, SeedType theSeedType, TodWeightedGridArray* theGridArray, int theGridArrayCount)
{
	ScaryPotterDontPlaceInCol(theCol, theGridArray, theGridArrayCount);

	for (int i = 0; i < MAX_GRID_SIZE_Y - 1; i++)
	{
		Plant* aPlant = mBoard->NewPlant(theCol, i, theSeedType, SeedType::SEED_NONE);
		if (theSeedType == SeedType::SEED_POTATOMINE)
		{
			aPlant->mStateCountdown = 10;
		}
	}
}

//0x428540
// GOTY @Patoke: 0x42AFA0
void Challenge::ScaryPotterPlacePot(ScaryPotType theScaryPotType, ZombieType theZombieType, SeedType theSeedType, int theCount, TodWeightedGridArray* theGridArray, int theGridArrayCount)
{
	ScaryPotType aPotType = theScaryPotType;
	while (theCount > 0)
	{
		TodWeightedGridArray* aGrid = TodPickFromWeightedGridArray(theGridArray, theGridArrayCount);

		GridItem* aScaryPot = mBoard->mGridItems.DataArrayAlloc();
		aScaryPot->mGridItemType = GRIDITEM_SCARY_POT;
		aScaryPot->mGridItemState = GRIDITEM_STATE_SCARY_POT_QUESTION;
		aScaryPot->mGridX = aGrid->mX;
		aScaryPot->mGridY = aGrid->mY;
		aGrid->mWeight = 0;
		aScaryPot->mRenderOrder = mBoard->MakeRenderOrder(RENDER_LAYER_PLANT, aGrid->mY, 0);
		aScaryPot->mZombieType = theZombieType; aScaryPot->mSeedType = theSeedType; aScaryPot->mScaryPotType = aPotType;
		if (aPotType == SCARYPOT_SUN)
		{
			aScaryPot->mSunCount = Rand(3) + 1;
		}
		--theCount;
	}
}

//0x428620
// GOTY @Patoke: 0x42B040
void Challenge::ScaryPotterChangePotType(GridItemState thePotType, int theCount)
{
	TodWeightedArray aPotArray[MAX_SCARY_POTS];
	int aPotCount = 0;

	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GRIDITEM_SCARY_POT)
		{
			if ((thePotType == GRIDITEM_STATE_SCARY_POT_LEAF && aGridItem->mScaryPotType == SCARYPOT_SEED) ||
				(thePotType == GRIDITEM_STATE_SCARY_POT_ZOMBIE && aGridItem->mZombieType == ZOMBIE_GARGANTUAR))
			{
				aPotArray[aPotCount].mItem = (intptr_t)aGridItem;
				aPotArray[aPotCount].mWeight = 1;
				aPotCount++;
			}
		}
	}
	if (theCount > aPotCount)
	{
		theCount = aPotCount;
	}

	for (int i = 0; i < theCount; i++)
	{
		TodWeightedArray* aScaryPotArray = TodPickArrayItemFromWeightedArray(aPotArray, aPotCount);
		aScaryPotArray->mWeight = 0;
		((GridItem*)(aScaryPotArray->mItem))->mGridItemState = thePotType;
	}
}

namespace
{
	// ScaryPotterPopulate 九关布阵表（槽 0..8 = GAMEMODE_SCARY_POTTER_1..9）：
	// 每关禁列 + PlacePot 常量序列（行序 = 原 case 调用序）；十关中 ENDLESS
	// 动态关（Gargantuar 随 mSurvivalStage 增长 + 15 关成就）表外特例。
	struct ScaryPotterPot
	{
		ScaryPotType mPotType;
		ZombieType mZombieType;
		SeedType mSeedType;
		int mCount;
	};
	// 每关 ScaryPotterDontPlaceInCol 禁列（-1 终止）
	constexpr int kScaryPotterDontPlaceCols[9][4] = {
		{0, 1, 2, 3},  // 0 SCARY_POTTER_1
		{0, 1, 2, 8},  // 1 SCARY_POTTER_2
		{0, 1, 2, -1},  // 2 SCARY_POTTER_3
		{0, 1, -1, -1},  // 3 SCARY_POTTER_4
		{0, 1, -1, -1},  // 4 SCARY_POTTER_5
		{0, 1, -1, -1},  // 5 SCARY_POTTER_6
		{0, 1, 2, -1},  // 6 SCARY_POTTER_7
		{0, 1, -1, -1},  // 7 SCARY_POTTER_8
		{0, 1, -1, -1},  // 8 SCARY_POTTER_9
	};
	// 每关 ScaryPotterPlacePot 序列（扁平表 + kScaryPotterPotBegin 段界）
	constexpr ScaryPotterPot kScaryPotterPots[] = {
	// 0 SCARY_POTTER_1
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_PEASHOOTER, 5},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SNOWPEA, 5},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SQUASH, 5},
	{SCARYPOT_ZOMBIE, ZOMBIE_NORMAL, SEED_NONE, 6},
	{SCARYPOT_ZOMBIE, ZOMBIE_PAIL, SEED_NONE, 3},
	{SCARYPOT_ZOMBIE, ZOMBIE_JACK_IN_THE_BOX, SEED_NONE, 1},
	// 1 SCARY_POTTER_2
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_LEFTPEATER, 7},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SNOWPEA, 3},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_WALLNUT, 3},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_POTATOMINE, 2},
	{SCARYPOT_ZOMBIE, ZOMBIE_NORMAL, SEED_NONE, 6},
	{SCARYPOT_ZOMBIE, ZOMBIE_PAIL, SEED_NONE, 3},
	{SCARYPOT_ZOMBIE, ZOMBIE_JACK_IN_THE_BOX, SEED_NONE, 1},
	// 2 SCARY_POTTER_3
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_LEFTPEATER, 6},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SNOWPEA, 4},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SQUASH, 2},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_HYPNOSHROOM, 3},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_WALLNUT, 3},
	{SCARYPOT_ZOMBIE, ZOMBIE_NORMAL, SEED_NONE, 8},
	{SCARYPOT_ZOMBIE, ZOMBIE_PAIL, SEED_NONE, 2},
	{SCARYPOT_ZOMBIE, ZOMBIE_DANCER, SEED_NONE, 1},
	{SCARYPOT_ZOMBIE, ZOMBIE_JACK_IN_THE_BOX, SEED_NONE, 1},
	// 3 SCARY_POTTER_4
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_PUFFSHROOM, 11},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_HYPNOSHROOM, 4},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_LEFTPEATER, 4},
	{SCARYPOT_ZOMBIE, ZOMBIE_JACK_IN_THE_BOX, SEED_NONE, 8},
	{SCARYPOT_ZOMBIE, ZOMBIE_NORMAL, SEED_NONE, 7},
	{SCARYPOT_ZOMBIE, ZOMBIE_FOOTBALL, SEED_NONE, 1},
	// 4 SCARY_POTTER_5
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_LEFTPEATER, 6},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_PUMPKINSHELL, 3},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SQUASH, 4},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_HYPNOSHROOM, 2},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SNOWPEA, 2},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_MAGNETSHROOM, 3},
	{SCARYPOT_ZOMBIE, ZOMBIE_NORMAL, SEED_NONE, 6},
	{SCARYPOT_ZOMBIE, ZOMBIE_PAIL, SEED_NONE, 5},
	{SCARYPOT_ZOMBIE, ZOMBIE_JACK_IN_THE_BOX, SEED_NONE, 1},
	{SCARYPOT_ZOMBIE, ZOMBIE_FOOTBALL, SEED_NONE, 3},
	// 5 SCARY_POTTER_6
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_LEFTPEATER, 7},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SQUASH, 2},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_TALLNUT, 5},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_THREEPEATER, 2},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_TORCHWOOD, 4},
	{SCARYPOT_ZOMBIE, ZOMBIE_NORMAL, SEED_NONE, 7},
	{SCARYPOT_ZOMBIE, ZOMBIE_POLEVAULTER, SEED_NONE, 5},
	{SCARYPOT_ZOMBIE, ZOMBIE_FOOTBALL, SEED_NONE, 2},  // 1073 0042BB5A/0042BB6A: 2 football + 1 jack complete 35 available cells.
	{SCARYPOT_ZOMBIE, ZOMBIE_JACK_IN_THE_BOX, SEED_NONE, 1},
	// 6 SCARY_POTTER_7
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SPIKEWEED, 13},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_WALLNUT, 3},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SQUASH, 3},
	{SCARYPOT_ZOMBIE, ZOMBIE_NORMAL, SEED_NONE, 10},
	{SCARYPOT_ZOMBIE, ZOMBIE_PAIL, SEED_NONE, 1},
	// 7 SCARY_POTTER_8
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_PUFFSHROOM, 7},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_WALLNUT, 3},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SQUASH, 5},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_LEFTPEATER, 4},
	{SCARYPOT_ZOMBIE, ZOMBIE_JACK_IN_THE_BOX, SEED_NONE, 8},
	{SCARYPOT_ZOMBIE, ZOMBIE_NORMAL, SEED_NONE, 4},
	{SCARYPOT_ZOMBIE, ZOMBIE_POGO, SEED_NONE, 4},
	// 8 SCARY_POTTER_9
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_LEFTPEATER, 6},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SNOWPEA, 2},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_PEASHOOTER, 2},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_THREEPEATER, 2},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SQUASH, 5},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_POTATOMINE, 1},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_WALLNUT, 1},
	{SCARYPOT_SEED, ZOMBIE_INVALID, SEED_PLANTERN, 1},
	{SCARYPOT_ZOMBIE, ZOMBIE_NORMAL, SEED_NONE, 8},
	{SCARYPOT_ZOMBIE, ZOMBIE_PAIL, SEED_NONE, 5},
	{SCARYPOT_ZOMBIE, ZOMBIE_JACK_IN_THE_BOX, SEED_NONE, 1},
	{SCARYPOT_ZOMBIE, ZOMBIE_GARGANTUAR, SEED_NONE, 1},
	};
	constexpr int kScaryPotterPotBegin[10] = {0, 6, 13, 22, 28, 38, 47, 52, 59, 71};
	static_assert(sizeof(kScaryPotterPots) / sizeof(kScaryPotterPots[0]) == kScaryPotterPotBegin[9],
		"kScaryPotterPots 槽位须与 kScaryPotterPotBegin 段界同步");
	static_assert(sizeof(kScaryPotterDontPlaceCols) / sizeof(kScaryPotterDontPlaceCols[0]) == 9,
		"kScaryPotterDontPlaceCols 槽位须与九关同步");
}  // namespace

//0x4286F0
// GOTY @Patoke: 0x42B0E0
void Challenge::ScaryPotterPopulate()
{
	int aGridArrayCount = 0;
	TodWeightedGridArray aGridArray[MAX_SCARY_POTS];
	for (int aGridX = 0; aGridX < MAX_GRID_SIZE_X; aGridX++)
	{
		for (int aGridY = 0; aGridY < MAX_GRID_SIZE_Y - 1; aGridY++)
		{
			aGridArray[aGridArrayCount].mX = aGridX;
			aGridArray[aGridArrayCount].mY = aGridY;
			aGridArray[aGridArrayCount].mWeight = 1;
			aGridArrayCount++;

			TOD_ASSERT(aGridArrayCount <= MAX_SCARY_POTS);
		}
	}

	if (mApp->IsAdventureMode() && mBoard->mLevel == 35)
	{
		switch (mSurvivalStage)
		{
		case 0:
			ScaryPotterDontPlaceInCol(0, aGridArray, aGridArrayCount);
			ScaryPotterDontPlaceInCol(1, aGridArray, aGridArrayCount);
			ScaryPotterDontPlaceInCol(2, aGridArray, aGridArrayCount);
			ScaryPotterDontPlaceInCol(3, aGridArray, aGridArrayCount);
			ScaryPotterDontPlaceInCol(4, aGridArray, aGridArrayCount);
			ScaryPotterDontPlaceInCol(5, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_PEASHOOTER, 5, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SQUASH, 5, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_ZOMBIE, ZOMBIE_NORMAL, SEED_NONE, 4, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_ZOMBIE, ZOMBIE_PAIL, SEED_NONE, 1, aGridArray, aGridArrayCount);
			break;
		case 1:
			ScaryPotterDontPlaceInCol(0, aGridArray, aGridArrayCount);
			ScaryPotterDontPlaceInCol(1, aGridArray, aGridArrayCount);
			ScaryPotterDontPlaceInCol(2, aGridArray, aGridArrayCount);
			ScaryPotterDontPlaceInCol(3, aGridArray, aGridArrayCount);
			ScaryPotterDontPlaceInCol(4, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_PEASHOOTER, 4, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SNOWPEA, 5, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SQUASH, 4, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_ZOMBIE, ZOMBIE_NORMAL, SEED_NONE, 5, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_ZOMBIE, ZOMBIE_PAIL, SEED_NONE, 1, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_ZOMBIE, ZOMBIE_FOOTBALL, SEED_NONE, 1, aGridArray, aGridArrayCount);
			ScaryPotterChangePotType(GRIDITEM_STATE_SCARY_POT_LEAF, 2);
			break;
		case 2:
			ScaryPotterDontPlaceInCol(0, aGridArray, aGridArrayCount);
			ScaryPotterDontPlaceInCol(1, aGridArray, aGridArrayCount);
			ScaryPotterDontPlaceInCol(2, aGridArray, aGridArrayCount);
			ScaryPotterDontPlaceInCol(3, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_PEASHOOTER, 5, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SNOWPEA, 5, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_HYPNOSHROOM, 5, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_ZOMBIE, ZOMBIE_NORMAL, SEED_NONE, 6, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_ZOMBIE, ZOMBIE_PAIL, SEED_NONE, 2, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_ZOMBIE, ZOMBIE_DANCER, SEED_NONE, 1, aGridArray, aGridArrayCount);
			ScaryPotterPlacePot(SCARYPOT_ZOMBIE, ZOMBIE_JACK_IN_THE_BOX, SEED_NONE, 1, aGridArray, aGridArrayCount);
			ScaryPotterChangePotType(GRIDITEM_STATE_SCARY_POT_LEAF, 3);
			break;
		}
	}
	else if (mApp->mGameMode == GAMEMODE_SCARY_POTTER_ENDLESS)
	{
		// ENDLESS 动态关表外特例（Gargantuar 数随 mSurvivalStage 增长 +
		// 15 关成就）——体逐句保持原 case。
		int aNumExtraGargantuars = ClampInt(mSurvivalStage / 10, 0, 8);
		ScaryPotterDontPlaceInCol(0, aGridArray, aGridArrayCount);
		ScaryPotterDontPlaceInCol(1, aGridArray, aGridArrayCount);
		ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_LEFTPEATER, 6, aGridArray, aGridArrayCount);
		ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SNOWPEA, 2, aGridArray, aGridArrayCount);
		ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_PEASHOOTER, 1, aGridArray, aGridArrayCount);
		ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_THREEPEATER, 2, aGridArray, aGridArrayCount);
		ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_SQUASH, 5, aGridArray, aGridArrayCount);
		ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_POTATOMINE, 1, aGridArray, aGridArrayCount);
		ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_WALLNUT, 1, aGridArray, aGridArrayCount);
		ScaryPotterPlacePot(SCARYPOT_SEED, ZOMBIE_INVALID, SEED_PLANTERN, 1, aGridArray, aGridArrayCount);
		ScaryPotterPlacePot(SCARYPOT_SUN, ZOMBIE_INVALID, SEED_NONE, 1, aGridArray, aGridArrayCount);
		ScaryPotterPlacePot(SCARYPOT_ZOMBIE, ZOMBIE_NORMAL, SEED_NONE, 8 - aNumExtraGargantuars, aGridArray, aGridArrayCount);
		ScaryPotterPlacePot(SCARYPOT_ZOMBIE, ZOMBIE_PAIL, SEED_NONE, 5, aGridArray, aGridArrayCount);
		ScaryPotterPlacePot(SCARYPOT_ZOMBIE, ZOMBIE_JACK_IN_THE_BOX, SEED_NONE, 1, aGridArray, aGridArrayCount);
		ScaryPotterPlacePot(SCARYPOT_ZOMBIE, ZOMBIE_GARGANTUAR, SEED_NONE, 1 + aNumExtraGargantuars, aGridArray, aGridArrayCount);
		ScaryPotterChangePotType(GRIDITEM_STATE_SCARY_POT_LEAF, 2);

		if (mSurvivalStage == 15) // @Patoke: add achievement
			ReportAchievement::GiveAchievement(mApp, ChinaShop, true);

	}
	else
	{
		// B 形态 -> 常量表（上方 kScaryPotterDontPlaceCols / kScaryPotterPots，
		// 槽 0..8 = GAMEMODE_SCARY_POTTER_1..9，行序 = 原 case 调用序）；
		// 表外值（非 scary-potter 模式）落 TOD_ASSERT = 原 default。
		const int aLevelIndex = (int)mApp->mGameMode - (int)GAMEMODE_SCARY_POTTER_1;
		if (aLevelIndex < 0 || aLevelIndex > 8)
		{
			TOD_ASSERT(false);
		}
		else
		{
			for (int k = 0; k < 4 && kScaryPotterDontPlaceCols[aLevelIndex][k] != -1; k++)
			{
				ScaryPotterDontPlaceInCol(kScaryPotterDontPlaceCols[aLevelIndex][k], aGridArray, aGridArrayCount);
			}
			for (int k = kScaryPotterPotBegin[aLevelIndex]; k < kScaryPotterPotBegin[aLevelIndex + 1]; k++)
			{
				const ScaryPotterPot& aPot = kScaryPotterPots[k];
				ScaryPotterPlacePot(aPot.mPotType, aPot.mZombieType, aPot.mSeedType, aPot.mCount, aGridArray, aGridArrayCount);
			}
			ScaryPotterChangePotType(GRIDITEM_STATE_SCARY_POT_LEAF, 2);
		}
	}

	mScaryPotterPots = ScaryPotterCountPots();
}

//0x429500
void Challenge::ScaryPotterStart()
{
	if (mApp->IsAdventureMode())
	{
		mBoard->DisplayAdvice(gStr.ADVICE_USE_SHOVEL_ON_POTS, MESSAGE_STYLE_HINT_STAY, ADVICE_USE_SHOVEL_ON_POTS);
	}
}

//0x4296A0
int Challenge::ScaryPotterIsCompleted()
{
	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GRIDITEM_SCARY_POT)
		{
			return false;
		}
	}

	return !mBoard->AreEnemyZombiesOnScreen();
}

//0x4296F0
void Challenge::ScaryPotterUpdate()
{
	if (mChallengeState == STATECHALLENGE_SCARY_POTTER_MALLETING)
	{
		Reanimation* aMalletReanim = mApp->ReanimationGet(mReanimChallenge);
		if (aMalletReanim->mLoopCount > 0)
		{
			GridItem* aScaryPot = mBoard->GetGridItemAt(GRIDITEM_SCARY_POT, mChallengeGridX, mChallengeGridY);
			if (aScaryPot)
			{
				ScaryPotterOpenPot(aScaryPot);
			}

			mChallengeGridX = 0;
			mChallengeGridY = 0;
			aMalletReanim->ReanimationDie();
			mReanimChallenge = REANIMATIONID_NULL;
			mChallengeState = STATECHALLENGE_NORMAL;
		}
	}
}

//0x429760
void Challenge::ScaryPotterMalletPot(GridItem* theScaryPot)
{
	mChallengeGridX = theScaryPot->mGridX;
	mChallengeGridY = theScaryPot->mGridY;
	int aXPos = mBoard->GridToPixelX(theScaryPot->mGridX, theScaryPot->mGridY);
	int aYPos = mBoard->GridToPixelY(theScaryPot->mGridX, theScaryPot->mGridY);
	Reanimation* aMalletReanim = mApp->AddReanimation(aXPos, aYPos, RENDER_LAYER_TOP, REANIM_HAMMER, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
	// 1073 0042C432 selects the real anim_open_pot track, not a missing-name fallback.
	aMalletReanim->PlayReanim(ReanimTrackId::anim_open_pot, REANIM_PLAY_ONCE_AND_HOLD, 0, 40.0f);
	mReanimChallenge = mApp->ReanimationGetID(aMalletReanim);
	mChallengeState = STATECHALLENGE_SCARY_POTTER_MALLETING;
	mApp->PlayFoley(FOLEY_SWING);
}

//0x4298E0
int Challenge::ScaryPotterCountPots()
{
	int aCount = 0;
	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GRIDITEM_SCARY_POT)
		{
			aCount++;
		}
	}
	return aCount;
}

//0x429930
int Challenge::PuzzleIsAwardStage()
{
	if (mApp->IsAdventureMode())
		return false;

	int aGoal = mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS ? 3 : mApp->mGameMode == GAMEMODE_SCARY_POTTER_ENDLESS ? 10 : 1;
	// 1073 0042C4D0/0042C520 test the just-completed, one-based stage.
	// Preserve the original 32-bit ADD before signed remainder, without UB.
	return Sexy::FrameCounterMath::IncrementSigned32(mSurvivalStage) % aGoal == 0;
}

//0x429980
void Challenge::PuzzlePhaseComplete(int theGridX, int theGridY)
{
	if (PuzzleIsAwardStage())
	{
		CoinType aCoinType;
		int aHit = Rand(100);
		if (aHit < 15)
		{
			aCoinType = mApp->mZenGarden->CanDropPottedPlantLoot() ? COIN_AWARD_PRESENT : COIN_AWARD_MONEY_BAG;
		}
		else if (aHit < 30)
		{
			aCoinType = mApp->mZenGarden->CanDropChocolate() ? COIN_AWARD_CHOCOLATE : COIN_AWARD_MONEY_BAG;
		}
		else
		{
			aCoinType = COIN_AWARD_BAG_DIAMOND;
		}

		int aPosX = mBoard->GridToPixelX(theGridX, theGridY) + 40;
		int aPosY = mBoard->GridToPixelY(theGridX, theGridY) + 40;
		mBoard->AddCoin(aPosX, aPosY, aCoinType, COIN_MOTION_COIN);
	}
	else
	{
		mBoard->FadeOutLevel();
	}
}

//0x429AC0
void Challenge::ScaryPotterOpenPot(GridItem* theScaryPot)
{
	int aXPos = mBoard->GridToPixelX(theScaryPot->mGridX, theScaryPot->mGridY);
	int aYPos = mBoard->GridToPixelY(theScaryPot->mGridX, theScaryPot->mGridY);
	switch (theScaryPot->mScaryPotType)
	{
	case SCARYPOT_SEED:
		mBoard->AddCoin(aXPos + 20, aYPos, COIN_USABLE_SEED_PACKET, COIN_MOTION_FROM_PLANT)->mUsableSeedType = theScaryPot->mSeedType;
		break;
	case SCARYPOT_ZOMBIE:
		{
			auto* zombie = mBoard->AddZombieInRow(theScaryPot->mZombieType, theScaryPot->mGridY, 0);
			zombie->mX = aXPos;
			zombie->mRootBindingGeneration = 0;
		}
		break;
	case SCARYPOT_SUN:
	{
		int aSunCount = theScaryPot->mSunCount;
		for (int i = 0; i < aSunCount; i++)
		{
			mBoard->AddCoin(aXPos + 15 * i, aYPos, COIN_SUN, COIN_MOTION_FROM_PLANT);
		}
		break;
	}
	default:
		TOD_ASSERT(false);
		break;
	}

	theScaryPot->GridItemDie();

	if (mBoard->mHelpIndex == ADVICE_USE_SHOVEL_ON_POTS)
	{
		mBoard->DisplayAdvice(gStr.ADVICE_DESTROY_POTS_TO_FINISH_LEVEL, MESSAGE_STYLE_HINT_FAST, ADVICE_DESTORY_POTS_TO_FINISIH_LEVEL);
	}
	if (ScaryPotterIsCompleted())
	{
		if (mApp->IsScaryPotterLevel() && !mBoard->IsFinalScaryPotterStage())
		{
			PuzzlePhaseComplete(theScaryPot->mGridX, theScaryPot->mGridY);
		}
		else
		{
			SpawnLevelAward(theScaryPot->mGridX, theScaryPot->mGridY);
		}
	}

	mApp->PlaySample(Sexy::SOUND_BONK);
	mApp->PlayFoley(FOLEY_VASE_BREAKING);
	ParticleEffect anEffect =
		theScaryPot->mGridItemState == GRIDITEM_STATE_SCARY_POT_LEAF ? PARTICLE_VASE_SHATTER_LEAF :
		theScaryPot->mGridItemState == GRIDITEM_STATE_SCARY_POT_ZOMBIE ? PARTICLE_VASE_SHATTER_ZOMBIE :
		PARTICLE_VASE_SHATTER;
	mApp->AddTodParticle(aXPos + 20, aYPos, RENDER_LAYER_GROUND, anEffect, static_cast<std::int64_t>(mBoard->mLevelTimeNs));
}

//0x429DB0
void Challenge::ScaryPotterJackExplode(int thePosX, int thePosY)
{
	int aGridX = mBoard->PixelToGridX(thePosX, thePosY);
	int aGridY = mBoard->PixelToGridY(thePosX, thePosY);

	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GRIDITEM_SCARY_POT && GridInRange(aGridItem->mGridX, aGridItem->mGridY, aGridX, aGridY, 1, 1))
		{
			ScaryPotterOpenPot(aGridItem);
		}
	}
}

//0x429E50
// GOTY @Patoke: 0x42C9F0
void Challenge::PuzzleNextStageClear()
{
	mApp->PlaySample(Sexy::SOUND_HUGE_WAVE);
	mBoard->mNextSurvivalStageCounter = 0;
	mBoard->mProgressMeterWidth = 0;

	{
		Zombie* aZombie = nullptr;
		while (mBoard->IterateZombies(aZombie))
		{
			if (aZombie->IsOnBoard())
			{
				aZombie->DieNoLoot(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
			}
		}
	}
	{
		Plant* aPlant = nullptr;
		while (mBoard->IteratePlants(aPlant))
		{
			if (aPlant->mIsOnBoard)
			{
				aPlant->Die(static_cast<std::int64_t>(mBoard->mLevelTimeNs), mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs, (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
			}
		}
	}
	mBoard->RefreshSeedPacketFromCursor();
	{
		Coin* aCoin = nullptr;
		while (mBoard->IterateCoins(aCoin))
		{
			if (aCoin->mType == COIN_USABLE_SEED_PACKET)
			{
				aCoin->Die();
			}
			else
			{
				aCoin->TryAutoCollectAfterLevelAward();
			}
		}
	}
	{
		GridItem* aGridItem = nullptr;
		while (mBoard->IterateGridItems(aGridItem))
		{
			aGridItem->GridItemDie();
		}
	}

	mSurvivalStage++;
	mBoard->ClearAdviceImmediately();
	mBoard->mLevelAwardSpawned = false;
	mApp->AddTodParticle(BOARD_WIDTH / 2, BOARD_HEIGHT / 2, RENDER_LAYER_TOP, PARTICLE_SCREEN_FLASH, static_cast<std::int64_t>(mBoard->mLevelTimeNs));
}

//0x42A040
ZombieType Challenge::IZombieSeedTypeToZombieType(SeedType theSeedType)
{
	// 僵尸种子（尾段连续区 SEED_ZOMBIE_NORMAL..SEED_ZOMBIE_IMP = 枚举序 60..74）
	// → 僵尸类型：按槽位直查（行序 = 枚举序）。
	static constexpr ZombieType kIZombieTypes[] = {
		ZOMBIE_NORMAL,        // SEED_ZOMBIE_NORMAL
		ZOMBIE_TRAFFIC_CONE,  // SEED_ZOMBIE_TRAFFIC_CONE
		ZOMBIE_POLEVAULTER,   // SEED_ZOMBIE_POLEVAULTER
		ZOMBIE_PAIL,          // SEED_ZOMBIE_PAIL
		ZOMBIE_LADDER,        // SEED_ZOMBIE_LADDER
		ZOMBIE_DIGGER,        // SEED_ZOMBIE_DIGGER
		ZOMBIE_BUNGEE,        // SEED_ZOMBIE_BUNGEE
		ZOMBIE_FOOTBALL,      // SEED_ZOMBIE_FOOTBALL
		ZOMBIE_BALLOON,       // SEED_ZOMBIE_BALLOON
		ZOMBIE_DOOR,          // SEED_ZOMBIE_SCREEN_DOOR
		ZOMBIE_ZAMBONI,       // SEED_ZOMBONI
		ZOMBIE_POGO,          // SEED_ZOMBIE_POGO
		ZOMBIE_DANCER,        // SEED_ZOMBIE_DANCER
		ZOMBIE_GARGANTUAR,    // SEED_ZOMBIE_GARGANTUAR
		ZOMBIE_IMP,           // SEED_ZOMBIE_IMP
	};
	static_assert(sizeof(kIZombieTypes) / sizeof(kIZombieTypes[0])
		== (int)SeedType::SEED_ZOMBIE_IMP - (int)SeedType::SEED_ZOMBIE_NORMAL + 1,
		"I, ZOMBIE 映射槽位须与 SeedType 枚举尾段同步");
	if (theSeedType >= SeedType::SEED_ZOMBIE_NORMAL && theSeedType <= SeedType::SEED_ZOMBIE_IMP)
	{
		return kIZombieTypes[(int)theSeedType - (int)SeedType::SEED_ZOMBIE_NORMAL];
	}
	TOD_ASSERT(false);
	return ZombieType::ZOMBIE_INVALID;
}

//0x42A0F0
// GOTY 1073: 0x42CC90
void Challenge::IZombiePlaceZombie(ZombieType theZombieType, int theGridX, int theGridY)
{
	Zombie* aZombie = mBoard->AddZombieInRow(theZombieType, theGridY, 0);
	if (theZombieType == ZOMBIE_BUNGEE)
	{
		aZombie->mTargetCol = theGridX;
		aZombie->SetRow(theGridY);
		{ aZombie->mX = (mBoard->GridToPixelX(theGridX, theGridY)); aZombie->mRootBindingGeneration = 0; }
		aZombie->mY = (aZombie->GetPosYBasedOnRow(theGridY));
		aZombie->mRenderOrder = Board::MakeRenderOrder(RENDER_LAYER_GRAVE_STONE, theGridY, 7);
	}
	else
	{
		{ aZombie->mX = (mBoard->GridToPixelX(theGridX, theGridY) - 30.0f); aZombie->mRootBindingGeneration = 0; }
	}
}

//0x42A210
void Challenge::IZombieMouseDownWithZombie(int theX, int theY, int theClickCount)
{
	if (theClickCount >= 0)
	{
		SeedType aSeedType = mBoard->mCursorObject->mType;
		int aGridX = mBoard->PlantingPixelToGridX(theX, theY, aSeedType);
		int aGridY = mBoard->PlantingPixelToGridY(theX, theY, aSeedType);
		if (aGridX != -1 && aGridY != -1 && theClickCount)
		{
			if (CanPlantAt(aGridX, aGridY, aSeedType) == PLANTING_OK)
			{
				if (mApp->mEasyPlantingCheat || mBoard->TakeSunMoney(mBoard->GetCurrentPlantCost(aSeedType, SEED_NONE)))
				{
					mBoard->ClearAdvice(ADVICE_I_ZOMBIE_LEFT_OF_LINE);
					mBoard->ClearAdvice(ADVICE_I_ZOMBIE_NOT_PASSED_LINE);
					ZombieType aZombieType = IZombieSeedTypeToZombieType(aSeedType);
					IZombiePlaceZombie(aZombieType, aGridX, aGridY);

					TOD_ASSERT(mBoard->mCursorObject->mSeedBankIndex >= 0 && mBoard->mCursorObject->mSeedBankIndex < mBoard->mSeedBank->mNumPackets);
					mBoard->mSeedBank->mSeedPackets[mBoard->mCursorObject->mSeedBankIndex].WasPlanted();
					mApp->PlayFoley(FOLEY_PLANT);
					mBoard->ClearCursor();
				}
			}
			else
			{
				mBoard->ClearAdvice(ADVICE_NONE);
				if (aSeedType == SEED_ZOMBIE_BUNGEE)
				{
					mBoard->DisplayAdvice(gStr.ADVICE_I_ZOMBIE_LEFT_OF_LINE, MESSAGE_STYLE_HINT_LONG, ADVICE_I_ZOMBIE_LEFT_OF_LINE);
				}
				else
				{
					mBoard->DisplayAdvice(gStr.ADVICE_I_ZOMBIE_NOT_PASSED_LINE, MESSAGE_STYLE_HINT_LONG, ADVICE_I_ZOMBIE_NOT_PASSED_LINE);
				}
			}

			return;
		}
	}
	mBoard->RefreshSeedPacketFromCursor();
	mApp->PlayFoley(FOLEY_DROP);
}

//0x42A530
void Challenge::IZombieSetupPlant(Plant* thePlant)
{
	const auto animationNowNs = (mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs);
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(thePlant->mBodyReanimID);
	Reanimation* aHeadReanim = mApp->ReanimationTryToGet(thePlant->mHeadReanimID);
	Reanimation* aHeadReanim2 = mApp->ReanimationTryToGet(thePlant->mHeadReanimID2);
	Reanimation* aHeadReanim3 = mApp->ReanimationTryToGet(thePlant->mHeadReanimID3);
	if (aBodyReanim) { aBodyReanim->PrepareTimeAt(animationNowNs); aBodyReanim->ApplyRate(0.0f); }
	if (aHeadReanim) { aHeadReanim->PrepareTimeAt(animationNowNs); aHeadReanim->ApplyRate(0.0f); }
	if (aHeadReanim2) { aHeadReanim2->PrepareTimeAt(animationNowNs); aHeadReanim2->ApplyRate(0.0f); }
	if (aHeadReanim3) { aHeadReanim3->PrepareTimeAt(animationNowNs); aHeadReanim3->ApplyRate(0.0f); }

	if (thePlant->mSeedType == SEED_POTATOMINE)
	{
		thePlant->PlayBodyReanim(static_cast<std::int64_t>(mBoard->mLevelTimeNs), ReanimTrackId::anim_armed, REANIM_LOOP, 0, 0);
		thePlant->mState = STATE_POTATO_ARMED;
	}

	thePlant->mBlinkCountdown = 0;
	thePlant->UpdateReanim(static_cast<std::int64_t>(mBoard->mLevelTimeNs), animationNowNs);
}

//0x42A660
// GOTY @Patoke: 0x42D250
void Challenge::IZombiePlacePlantInSquare(SeedType theSeedType, int theGridX, int theGridY)
{
	if (mBoard->CanPlantAt(theGridX, theGridY, theSeedType) == PLANTING_OK)
	{
		IZombieSetupPlant(mBoard->NewPlant(theGridX, theGridY, theSeedType));
	}
}

//0x42A6C0
// GOTY @Patoke: 0x42D2B0
void Challenge::IZombiePlacePlants(SeedType theSeedType, int theCount, int theGridY)
{
	// 关卡中植物（或限制红线）的列数
	int aColumns = 6;
	GameMode aGameMode = mApp->mGameMode;
	if (aGameMode >= GAMEMODE_PUZZLE_I_ZOMBIE_1 && aGameMode <= GAMEMODE_PUZZLE_I_ZOMBIE_5)
	{
		aColumns = 4;
	}
	else if (aGameMode != GAMEMODE_PUZZLE_I_ZOMBIE_9)
	{
		aColumns = 5;
	}

	// 只能生成在 aMinGridY 行与 aMaxGridY 行之间
	int aMinGridY, aMaxGridY;
	if (theGridY == -1)  // 参数 theGridY 使用默认的 -1 时，不限定生成范围
	{
		aMinGridY = 0;  // 从第 0 行开始
		aMaxGridY = 4;  // 至第 4 行结束
	}
	else
	{
		aMinGridY = theGridY;
		aMaxGridY = theGridY;  // 可种植范围从 theGridY 行至 theGridY 行，表示限定种植的行只能为 theGridY
	}

	int aGridArrayCount = 0;
	TodWeightedGridArray aGridArray[MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y];
	// 将从 theGridY 行至 aMaxGridY 行的选择数据写入格子数组中
	for (int aRow = aMinGridY; aRow <= aMaxGridY; aRow++)
	{
		// 对于每一行，写入红线左侧每一列的选择数据
		for (int aCol = 0; aCol < aColumns; aCol++)
		{
			// 当且仅当格子内可以种植目标植物时
			if (mBoard->CanPlantAt(aCol, aRow, theSeedType) == PLANTING_OK)
			{
				// 坚果和火炬树桩只会出现在靠右侧的 3 列
				if ((theSeedType != SEED_WALLNUT && theSeedType != SEED_TORCHWOOD) || aColumns - aCol <= 3)
				{
					aGridArray[aGridArrayCount].mX = aCol;
					aGridArray[aGridArrayCount].mY = aRow;
					aGridArray[aGridArrayCount].mWeight = 1;
					aGridArrayCount++;
				}
			}
		}
	}

	if (theCount > aGridArrayCount)
		theCount = aGridArrayCount;

	// 依次选择 theCount 个格子并在这些格子中放置植物
	for (int i = 0; i < theCount; i++)
	{
		TodWeightedGridArray* aGrid = TodPickFromWeightedGridArray(aGridArray, aGridArrayCount);
		IZombiePlacePlantInSquare(theSeedType, aGrid->mX, aGrid->mY);
		aGrid->mWeight = 0;
	}
}

namespace
{
	// IZombieInitLevel 九关布阵表（槽 0..8 = GAMEMODE_PUZZLE_I_ZOMBIE_1..9）：
	// 每关先定点 InSquare 再限额 PlacePlants（原 case 内两类调用各自成段、段内
	// 序保持）；ENDLESS 动态关（随机阵型 + 10 关成就）表外特例。
	struct IZombieSquare
	{
		SeedType mSeedType;
		int mGridX;
		int mGridY;
	};
	struct IZombieRow
	{
		SeedType mSeedType;
		int mCount;
		int mGridY;
	};
	constexpr IZombieSquare kIZombieSquares[] = {
	// 0 PUZZLE_I_ZOMBIE_1
	{SEED_SUNFLOWER, 3, 2},
	{SEED_SUNFLOWER, 3, 3},
	// 1 PUZZLE_I_ZOMBIE_2
	{SEED_SPIKEWEED, 3, 0},
	{SEED_SUNFLOWER, 2, 0},
	{SEED_SUNFLOWER, 3, 3},
	// 2 PUZZLE_I_ZOMBIE_3
	{SEED_POTATOMINE, 3, 0},
	{SEED_SUNFLOWER, 2, 0},
	{SEED_POTATOMINE, 2, 2},
	{SEED_SUNFLOWER, 2, 4},
	{SEED_TORCHWOOD, 3, 3},
	// 3 PUZZLE_I_ZOMBIE_4
	{SEED_WALLNUT, 3, 0},
	{SEED_SUNFLOWER, 2, 0},
	{SEED_WALLNUT, 3, 1},
	{SEED_WALLNUT, 3, 2},
	{SEED_SUNFLOWER, 2, 2},
	{SEED_WALLNUT, 3, 3},
	{SEED_WALLNUT, 3, 4},
	{SEED_SUNFLOWER, 2, 4},
	// 4 PUZZLE_I_ZOMBIE_5
	{SEED_SUNFLOWER, 3, 2},
	{SEED_SUNFLOWER, 3, 3},
	// 5 PUZZLE_I_ZOMBIE_6
	{SEED_GARLIC, 4, 1},
	{SEED_GARLIC, 4, 3},
	// 6 PUZZLE_I_ZOMBIE_7
	{SEED_SUNFLOWER, 4, 2},
	{SEED_SUNFLOWER, 4, 4},
	// 7 PUZZLE_I_ZOMBIE_8
	// 8 PUZZLE_I_ZOMBIE_9
	{SEED_TALLNUT, 5, 1},
	{SEED_TORCHWOOD, 5, 3},
	};
	constexpr int kIZombieSquareBegin[10] = {0, 2, 5, 10, 18, 20, 22, 24, 24, 26};
	constexpr IZombieRow kIZombieRows[] = {
	// 0 PUZZLE_I_ZOMBIE_1
	{SEED_SUNFLOWER, 7, -1},
	{SEED_SQUASH, 3, -1},
	{SEED_PEASHOOTER, 6, -1},
	{SEED_SNOWPEA, 2, -1},
	// 1 PUZZLE_I_ZOMBIE_2
	{SEED_SPIKEWEED, 1, 0},
	{SEED_PEASHOOTER, 1, 0},
	{SEED_SNOWPEA, 2, 3},
	{SEED_SUNFLOWER, 1, 3},
	{SEED_SUNFLOWER, 4, -1},
	{SEED_SPIKEWEED, 2, -1},
	{SEED_SNOWPEA, 2, -1},
	{SEED_PEASHOOTER, 4, -1},
	// 2 PUZZLE_I_ZOMBIE_3
	{SEED_TORCHWOOD, 2, -1},
	{SEED_SUNFLOWER, 5, -1},
	{SEED_PEASHOOTER, 7, -1},
	{SEED_SPLITPEA, 1, -1},
	// 3 PUZZLE_I_ZOMBIE_4
	{SEED_PEASHOOTER, 1, 0},
	{SEED_SNOWPEA, 1, 1},
	{SEED_FUMESHROOM, 2, 2},
	{SEED_SNOWPEA, 1, 3},
	{SEED_PEASHOOTER, 1, 4},
	{SEED_PEASHOOTER, 2, -1},
	{SEED_SUNFLOWER, 4, -1},
	// 4 PUZZLE_I_ZOMBIE_5
	{SEED_CACTUS, 1, 1},
	{SEED_CACTUS, 1, 4},
	{SEED_MAGNETSHROOM, 1, -1},
	{SEED_SUNFLOWER, 5, -1},
	{SEED_PEASHOOTER, 8, -1},
	{SEED_SNOWPEA, 2, -1},
	// 5 PUZZLE_I_ZOMBIE_6
	{SEED_SUNFLOWER, 3, 1},
	{SEED_SUNFLOWER, 3, 3},
	{SEED_TORCHWOOD, 2, -1},
	{SEED_SUNFLOWER, 2, -1},
	{SEED_SPIKEWEED, 3, -1},
	{SEED_SNOWPEA, 1, -1},
	{SEED_PEASHOOTER, 5, -1},
	{SEED_SQUASH, 2, -1},
	{SEED_KERNELPULT, 2, -1},
	// 6 PUZZLE_I_ZOMBIE_7
	{SEED_SUNFLOWER, 6, -1},
	{SEED_POTATOMINE, 9, -1},
	{SEED_CHOMPER, 8, -1},
	// 7 PUZZLE_I_ZOMBIE_8
	{SEED_WALLNUT, 3, -1},
	{SEED_MAGNETSHROOM, 2, -1},
	{SEED_PEASHOOTER, 8, -1},
	{SEED_SQUASH, 2, -1},
	{SEED_POTATOMINE, 2, -1},
	{SEED_SUNFLOWER, 8, -1},
	// 8 PUZZLE_I_ZOMBIE_9
	{SEED_POTATOMINE, 4, 0},
	{SEED_SUNFLOWER, 2, 0},
	{SEED_SUNFLOWER, 2, 1},
	{SEED_THREEPEATER, 1, 1},
	{SEED_SNOWPEA, 1, 1},
	{SEED_SPLITPEA, 1, 1},
	{SEED_CHOMPER, 3, 2},
	{SEED_SUNFLOWER, 2, 2},
	{SEED_SQUASH, 1, 2},
	{SEED_PEASHOOTER, 3, 3},
	{SEED_SUNFLOWER, 2, 3},
	{SEED_SUNFLOWER, 1, 4},
	{SEED_FUMESHROOM, 1, 4},
	{SEED_SCAREDYSHROOM, 1, 4},
	{SEED_STARFRUIT, 1, 4},
	{SEED_SPLITPEA, 1, 4},
	{SEED_MAGNETSHROOM, 1, 4},
	};
	constexpr int kIZombieRowBegin[10] = {0, 4, 12, 16, 23, 29, 38, 41, 47, 64};
	static_assert(sizeof(kIZombieSquares) / sizeof(kIZombieSquares[0]) == kIZombieSquareBegin[9],
		"kIZombieSquares 槽位须与 kIZombieSquareBegin 段界同步");
	static_assert(sizeof(kIZombieRows) / sizeof(kIZombieRows[0]) == kIZombieRowBegin[9],
		"kIZombieRows 槽位须与 kIZombieRowBegin 段界同步");
}  // namespace

//0x42A890
// GOTY @Patoke: 0x42D440
void Challenge::IZombieInitLevel()
{
	mChallengeScore = 0;
	for (int aRow = 0; aRow < I_ZOMBIE_WINNING_SCORE; aRow++)
	{
		GridItem* aBrain = mBoard->mGridItems.DataArrayAlloc();
		aBrain->mGridItemType = GRIDITEM_IZOMBIE_BRAIN;
		aBrain->mGridX = 0;
		aBrain->mGridY = aRow;
		aBrain->mRenderOrder = Board::MakeRenderOrder(RENDER_LAYER_PLANT, aRow, 0);
		aBrain->mBrainHealth = 70;
		aBrain->mPosX = mBoard->GridToPixelX(0, aRow) - 40;
		aBrain->mPosY = mBoard->GridToPixelY(0, aRow) + 40;
	}

	if (mApp->mGameMode == GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS)
	{
		// ENDLESS 动态关表外特例（随机阵型 + 10 关成就）——体逐句保持原
		// case（内层阵型 switch 随体保留）。
		if (mSurvivalStage == 10) // @Patoke: add achievement
			ReportAchievement::GiveAchievement(mApp, BetterOffDead, true);

		int aFormationHit = RandRangeInt(0, 4);

		int aPuffshroomCount = RandRangeInt(ClampInt(2 + mSurvivalStage / 3, 2, 4), ClampInt(3 + mSurvivalStage / 2, 2, 6));
		if (mSurvivalStage == 0)
		{
			aPuffshroomCount = 0;
		}
		else if (mSurvivalStage == 1)
		{
			aPuffshroomCount = 1;
		}
		else if (mSurvivalStage >= 10)
		{
			aPuffshroomCount = RandRangeInt(ClampInt(2 + mSurvivalStage / 3, 2, 5), ClampInt(3 + mSurvivalStage / 2, 2, 7));
		}
		int aSunflowerCount = 8 - aPuffshroomCount;
		IZombiePlacePlants(SEED_SUNFLOWER, aSunflowerCount, -1);
		IZombiePlacePlants(SEED_PUFFSHROOM, aPuffshroomCount, -1);

		if (aFormationHit == 0 && mSurvivalStage >= 1)
		{
			switch (RandRangeInt(0, 4))
			{
			case 0:  // 输出阵
				IZombiePlacePlants(SEED_SNOWPEA, 9, -1);
				IZombiePlacePlants(SEED_SPLITPEA, 4, -1);
				IZombiePlacePlants(SEED_REPEATER, 4, -1);
				break;
			case 1:  // 爆炸阵
				IZombiePlacePlants(SEED_POTATOMINE, 9, -1);
				IZombiePlacePlants(SEED_CHOMPER, 8, -1);
				break;
			case 2:  // 倾斜阵
				IZombiePlacePlants(SEED_SPIKEWEED, 9, -1);
				IZombiePlacePlants(SEED_STARFRUIT, 8, -1);
				break;
			case 3:  // 穿刺阵
				IZombiePlacePlants(SEED_FUMESHROOM, 9, -1);
				IZombiePlacePlants(SEED_MAGNETSHROOM, 8, -1);
				break;
			default:  // 回复阵
				IZombiePlacePlants(SEED_SCAREDYSHROOM, 12, -1);
				IZombiePlacePlants(SEED_SUNFLOWER, 5, -1);
			}
		}
		else
		{
			switch (RandRangeInt(0, 5))
			{
			case 0:
			case 1:
			case 2:  // 综合阵
				IZombiePlacePlants(SEED_WALLNUT, 1, -1);
				IZombiePlacePlants(SEED_TORCHWOOD, 1, -1);
				IZombiePlacePlants(SEED_POTATOMINE, 1, -1);
				IZombiePlacePlants(SEED_CHOMPER, 2, -1);
				IZombiePlacePlants(SEED_PEASHOOTER, 1, -1);
				IZombiePlacePlants(SEED_SPLITPEA, 1, -1);
				IZombiePlacePlants(SEED_KERNELPULT, 1, -1);
				IZombiePlacePlants(SEED_THREEPEATER, 1, -1);
				IZombiePlacePlants(SEED_SNOWPEA, 1, -1);
				IZombiePlacePlants(SEED_SQUASH, 1, -1);
				IZombiePlacePlants(SEED_FUMESHROOM, 1, -1);
				IZombiePlacePlants(SEED_UMBRELLA, 1, -1);
				IZombiePlacePlants(SEED_STARFRUIT, 1, -1);
				IZombiePlacePlants(SEED_MAGNETSHROOM, 1, -1);
				IZombiePlacePlants(SEED_SPIKEWEED, 2, -1);
				break;
			case 3:
			case 4:  // 控制阵
				IZombiePlacePlants(SEED_TORCHWOOD, 1, -1);
				IZombiePlacePlants(SEED_SPLITPEA, 3, -1);
				IZombiePlacePlants(SEED_REPEATER, 1, -1);
				IZombiePlacePlants(SEED_KERNELPULT, 3, -1);
				IZombiePlacePlants(SEED_THREEPEATER, 1, -1);
				IZombiePlacePlants(SEED_SNOWPEA, 3, -1);
				IZombiePlacePlants(SEED_UMBRELLA, 1, -1);
				IZombiePlacePlants(SEED_MAGNETSHROOM, 1, -1);
				IZombiePlacePlants(SEED_SPIKEWEED, 3, -1);
				break;
			default:  // 即死阵
				IZombiePlacePlants(SEED_POTATOMINE, 4, -1);
				IZombiePlacePlants(SEED_CHOMPER, 3, -1);
				IZombiePlacePlants(SEED_SQUASH, 3, -1);
				IZombiePlacePlants(SEED_FUMESHROOM, 4, -1);
				IZombiePlacePlants(SEED_SPIKEWEED, 3, -1);
				break;
			}
		}
	}
	else
	{
		// B 形态 -> 常量表（上方 kIZombieSquares / kIZombieRows，槽 0..8 =
		// GAMEMODE_PUZZLE_I_ZOMBIE_1..9）；表外值（非 I, ZOMBIE 模式）落
		// TOD_ASSERT = 原 default。
		const int aLevelIndex = (int)mApp->mGameMode - (int)GAMEMODE_PUZZLE_I_ZOMBIE_1;
		if (aLevelIndex < 0 || aLevelIndex > 8)
		{
			TOD_ASSERT(false);
		}
		else
		{
			for (int k = kIZombieSquareBegin[aLevelIndex]; k < kIZombieSquareBegin[aLevelIndex + 1]; k++)
			{
				IZombiePlacePlantInSquare(kIZombieSquares[k].mSeedType, kIZombieSquares[k].mGridX, kIZombieSquares[k].mGridY);
			}
			for (int k = kIZombieRowBegin[aLevelIndex]; k < kIZombieRowBegin[aLevelIndex + 1]; k++)
			{
				IZombiePlacePlants(kIZombieRows[k].mSeedType, kIZombieRows[k].mCount, kIZombieRows[k].mGridY);
			}
		}
	}

	mBoard->mBonusLawnMowersRemaining = 0;
}

//0x42B290
void Challenge::IZombieStart()
{
	mBoard->DisplayAdvice(gStr.ADVICE_I_ZOMBIE_EAT_ALL_BRAINS, MESSAGE_STYLE_HINT_FAST, ADVICE_I_ZOMBIE_EAT_ALL_BRAINS);
}

//0x42B340
void Challenge::IZombieUpdate()
{
	// GOTY 1.2.0.1073 counts every live sun coin on the lawn before deciding
	// that the player can no longer afford a zombie.  CountSunBeingCollected()
	// only includes coins already flying toward the counter and reproduces the
	// older false-loss bug when a sunflower's sun is still resting on the lawn.
	int aSunMoney = mBoard->mSunMoney;
	Coin* aSunCoin = nullptr;
	while (mBoard->IterateCoins(aSunCoin))
	{
		if (aSunCoin->IsSun())
		{
			aSunMoney += aSunCoin->GetSunValue();
		}
	}

	Zombie* aZombie = nullptr;
	while (mBoard->IterateZombies(aZombie))
	{
		if (!aZombie->IsDeadOrDying() && aZombie->mZombiePhase != PHASE_POLEVAULTER_IN_VAULT && !aZombie->mIsEating && static_cast<std::int64_t>(mBoard->mLevelTimeNs) - aZombie->mShotFlashEndTimeNs > 5'000'000'000LL)
		{
			aZombie->PickRandomSpeed(static_cast<std::int64_t>(mBoard->mLevelTimeNs));
		}
	}

	int anActive = false;
	Plant* aPlant = nullptr;
	while (mBoard->IteratePlants(aPlant))
	{
		PlantState aState = aPlant->mState;
		if (aState == STATE_SQUASH_FALLING || aState == STATE_SQUASH_DONE_FALLING || aState == STATE_CHOMPER_BITING || aState == STATE_CHOMPER_BITING_GOT_ONE)
		{
			anActive = true;
		}
	}
	TodParticleSystem* aParticle = nullptr;
	while (mBoard->IterateParticles(aParticle))
	{
		if (aParticle->mEffectType == PARTICLE_POTATO_MINE)
		{
			anActive = true;
		}
	}

	if (mBoard->mZombies.mSize == 0 && aSunMoney < 50 && !mBoard->HasLevelAwardDropped() && !anActive)
	{
		Coin* aCoin = nullptr;
		while (mBoard->IterateCoins(aCoin))
		{
			if (aCoin->IsMoney())
			{
				aCoin->Die();
			}
		}

		mBoard->ZombiesWon(nullptr);
	}
}

//0x42B530
int Challenge::IsZombieSeedType(SeedType theSeedType)
{
	return
		theSeedType == SEED_ZOMBIQUARIUM_SNORKLE ||
		theSeedType == SEED_ZOMBIQUARIUM_TROPHY ||
		theSeedType == SEED_ZOMBIE_NORMAL ||
		theSeedType == SEED_ZOMBIE_TRAFFIC_CONE ||
		theSeedType == SEED_ZOMBIE_POLEVAULTER ||
		theSeedType == SEED_ZOMBIE_PAIL ||
		theSeedType == SEED_ZOMBIE_LADDER ||
		theSeedType == SEED_ZOMBIE_DIGGER ||
		theSeedType == SEED_ZOMBIE_BUNGEE ||
		theSeedType == SEED_ZOMBIE_FOOTBALL ||
		theSeedType == SEED_ZOMBIE_BALLOON ||
		theSeedType == SEED_ZOMBIE_SCREEN_DOOR ||
		// SEED_ZOMBONI / SEED_ZOMBIE_POGO 已删（不可达：I,Zombie 全关卡不投放）
		theSeedType == SEED_ZOMBIE_DANCER ||
		theSeedType == SEED_ZOMBIE_GARGANTUAR ||
		theSeedType == SEED_ZOMBIE_IMP;
}

//0x42B590
// GOTY @Patoke: 0x42E280
void Challenge::IZombieSetPlantFilterEffect(Plant* thePlant, FilterEffect theFilterEffect)
{
	Reanimation* aBodyReanim = mApp->ReanimationTryToGet(thePlant->mBodyReanimID);
	Reanimation* aHeadReanim = mApp->ReanimationTryToGet(thePlant->mHeadReanimID);
	Reanimation* aHeadReanim2 = mApp->ReanimationTryToGet(thePlant->mHeadReanimID2);
	Reanimation* aHeadReanim3 = mApp->ReanimationTryToGet(thePlant->mHeadReanimID3);
	if (aBodyReanim)	aBodyReanim->mFilterEffect = theFilterEffect;
	if (aHeadReanim)	aHeadReanim->mFilterEffect = theFilterEffect;
	if (aHeadReanim2)	aHeadReanim2->mFilterEffect = theFilterEffect;
	if (aHeadReanim3)	aHeadReanim3->mFilterEffect = theFilterEffect;
}

//0x42B690
// GOTY @Patoke: 0x42E3D4
void Challenge::IZombieDrawPlant(Graphics* g, Plant* thePlant)
{
	Reanimation* aReanim = mApp->ReanimationTryToGet(thePlant->mBodyReanimID);
	if (aReanim)
	{
		IZombieSetPlantFilterEffect(thePlant, FILTER_EFFECT_WHITE);
		g->mColorizeImages = true;

		float aOffsetX = g->mTransX, aOffsetY = g->mTransY;
		g->mTransX = aOffsetX + 4;
		g->mTransY = aOffsetY + 4;
		g->mColor = Color(122, 86, 58);
		aReanim->DrawRenderGroup(g, 0);

		g->mTransX = aOffsetX + 2;
		g->mTransY = aOffsetY + 2;
		g->mColor = Color(171, 135, 107);
		aReanim->DrawRenderGroup(g, 0);

		g->mTransX = aOffsetX - 2;
		g->mTransY = aOffsetY - 2;
		g->mColor = Color(171, 135, 107);
		aReanim->DrawRenderGroup(g, 0);

		g->mTransX = aOffsetX;
		g->mTransY = aOffsetY;
		g->mColor = Color(255, 201, 160);
		IZombieSetPlantFilterEffect(thePlant, FILTER_EFFECT_NONE);
		aReanim->DrawRenderGroup(g, 0);

		IZombieSetPlantFilterEffect(thePlant, FILTER_EFFECT_NONE);
		g->mDrawMode = Graphics::DRAWMODE_NORMAL;
		g->mColorizeImages = false;
	}
}

//0x42B810
GridItem* Challenge::IZombieGetBrainTarget(Zombie* theZombie)
{
	if (theZombie->mZombieType == ZOMBIE_BUNGEE || theZombie->IsWalkingBackwards())
		return nullptr;

	Rect aZombieRect = theZombie->GetZombieAttackRect();
	if (theZombie->mZombiePhase == PHASE_POLEVAULTER_PRE_VAULT)
	{
		aZombieRect = Rect(50 + (theZombie->mX), 0, 20, 115);
	}
	if (theZombie->mZombieType == ZOMBIE_BALLOON)
	{
		aZombieRect.mX += 25;
	}

	if (aZombieRect.mX > 20)
		return nullptr;

	GridItem* aBrain = mBoard->GetGridItemAt(GRIDITEM_IZOMBIE_BRAIN, 0, theZombie->mRow);
	return (aBrain && aBrain->mGridItemState != GRIDITEM_STATE_BRAIN_SQUISHED) ? aBrain : nullptr;
}

//0x42B8B0
void Challenge::IZombieScoreBrain(GridItem* theBrain)
{
	mBoard->mChallenge->mChallengeScore++;
	mBoard->mProgressMeterWidth = TodAnimateCurve(0, I_ZOMBIE_WINNING_SCORE, mBoard->mChallenge->mChallengeScore, 0, PROGRESS_METER_COUNTER, Curves::Linear);

	if (mBoard->mChallenge->mChallengeScore == I_ZOMBIE_WINNING_SCORE)
	{
		if (mApp->IsEndlessIZombie(mApp->mGameMode))
		{
			PuzzlePhaseComplete(0, theBrain->mGridY);
		}
		else
		{
			mBoard->mChallenge->SpawnLevelAward(0, theBrain->mGridY);
		}
	}

	if (mBoard->mChallenge->mChallengeScore != I_ZOMBIE_WINNING_SCORE || !PuzzleIsAwardStage())
	{
		mBoard->DropLootPiece(theBrain->mPosX + 40, theBrain->mPosY - 50, 12);
	}
}

//0x42B970
int Challenge::IZombieEatBrain(Zombie* theZombie)
{
	GridItem* aBrain = IZombieGetBrainTarget(theZombie);
	if (aBrain == nullptr)
		return false;

	theZombie->StartEating(static_cast<std::int64_t>(mBoard->mLevelTimeNs));
	--aBrain->mBrainHealth;
	if (aBrain->mBrainHealth <= 0)
	{
		mApp->PlaySample(Sexy::SOUND_GULP);
		aBrain->GridItemDie();
		IZombieScoreBrain(aBrain);
	}
	return true;
}

//0x42B9D0
void Challenge::IZombiePlantDropRemainingSun(Plant* thePlant)
{
	if (thePlant->mSeedType == SEED_SUNFLOWER)
	{
		int aSunCount = thePlant->mPlantHealth / 40 + 1;
		for (int i = 0; i < aSunCount; i++)
		{
			mBoard->AddCoin((thePlant->mX) + 5 * i, (thePlant->mY), COIN_SUN, COIN_MOTION_FROM_PLANT);
		}
	}
}

//0x42BA30
void Challenge::IZombieSquishBrain(GridItem* theBrain)
{
	theBrain->mRenderOrder = Board::MakeRenderOrder(RENDER_LAYER_GRAVE_STONE, theBrain->mGridY, 0);
	theBrain->mGridItemState = GRIDITEM_STATE_BRAIN_SQUISHED;
	theBrain->mStateEndTimeNs = static_cast<std::int64_t>(mBoard->mLevelTimeNs)+5'000'000'000;
	theBrain->mApp->PlayFoley(FOLEY_SQUISH);
	IZombieScoreBrain(theBrain);
}

//0x42BAC0
// GOTY 1073: 0042E820
int Challenge::SquirrelCountUncaught()
{
	int aCount = 0;

	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GRIDITEM_SQUIRREL &&
			aGridItem->mGridItemState != GRIDITEM_STATE_SQUIRREL_CAUGHT &&
			aGridItem->mGridItemState != GRIDITEM_STATE_SQUIRREL_ZOMBIE)
		{
			aCount++;
		}
	}

	return aCount;
}

//0x42BB10
// GOTY 1073: 0042E870
void Challenge::SquirrelStart()
{
	int aPicksCount = 0;
	TodWeightedGridArray aPicks[MAX_GRID_SIZE_X * (MAX_GRID_SIZE_Y - 1)];
	for (int aCol = 0; aCol < MAX_GRID_SIZE_X; aCol++)
	{
		for (int aRow = 0; aRow < MAX_GRID_SIZE_Y - 1; aRow++)
		{
			aPicks[aPicksCount].mX = aCol;
			aPicks[aPicksCount].mY = aRow;
			aPicks[aPicksCount].mWeight = 1;
			aPicksCount++;
		}
	}

	for (int aCount = MAX_SQUIRRELS; aCount > 0; aCount--)
	{
		TodWeightedGridArray* aGrid = TodPickFromWeightedGridArray(aPicks, aPicksCount);
		aGrid->mWeight = 0;

		GridItem* aSquirrel = mBoard->mGridItems.DataArrayAlloc();
		aSquirrel->mGridItemType = GRIDITEM_SQUIRREL;
		aSquirrel->mGridItemState = GRIDITEM_STATE_SQUIRREL_WAITING;
		aSquirrel->mGridX = aGrid->mX;
		aSquirrel->mGridY = aGrid->mY;
		aSquirrel->mStateEndTimeNs = static_cast<std::int64_t>(mBoard->mLevelTimeNs)+static_cast<std::int64_t>(RandRangeInt(100, 500))*10'000'000;
		aSquirrel->mRenderOrder = mBoard->MakeRenderOrder(RENDER_LAYER_GRAVE_STONE, aSquirrel->mGridY, 1);
	}

	for (int i = 0; i < aPicksCount; i++)
	{
		if (aPicks[i].mX < 4)
		{
			aPicks[i].mWeight = 0;
		}
	}

	TodWeightedGridArray* aGrid = TodPickFromWeightedGridArray(aPicks, aPicksCount);
	GridItem* aSquirrel = mBoard->mGridItems.DataArrayAlloc();
	aSquirrel->mGridItemType = GRIDITEM_SQUIRREL;
	aSquirrel->mGridItemState = GRIDITEM_STATE_SQUIRREL_ZOMBIE;
	aSquirrel->mGridX = aGrid->mX;
	aSquirrel->mGridY = aGrid->mY;
	aSquirrel->mRenderOrder = Board::MakeRenderOrder(RENDER_LAYER_GRAVE_STONE, aGrid->mY, 1);
}

//0x42BCB0
// GOTY 1073: 0042E9B0
void Challenge::SquirrelChew(GridItem* theSquirrel)
{
	theSquirrel->mStateEndTimeNs = static_cast<std::int64_t>(mBoard->mLevelTimeNs)+static_cast<std::int64_t>(RandRangeInt(100, 500))*10'000'000;

	Plant* aPlant = mBoard->GetTopPlantAt(theSquirrel->mGridX, theSquirrel->mGridY, TOPPLANT_EATING_ORDER);
	if (aPlant)
	{
		float aPosX = mBoard->GridToPixelX(theSquirrel->mGridX, theSquirrel->mGridY);
		float aPosY = mBoard->GridToPixelY(theSquirrel->mGridX, theSquirrel->mGridY);
		mApp->AddTodParticle(aPosX + 40, aPosY + 40, aPlant->mRenderOrder + 1, PARTICLE_WALLNUT_EAT_SMALL, static_cast<std::int64_t>(mBoard->mLevelTimeNs));

		if (std::max<std::int64_t>(0, aPlant->mEatenFlashEndTimeNs - static_cast<std::int64_t>(aPlant->mBoard->mLevelTimeNs)) <= 250000000LL)
			aPlant->mEatenFlashEndTimeNs = static_cast<std::int64_t>(aPlant->mBoard->mLevelTimeNs) + 250000000LL;
	}
}

namespace
{
	// SquirrelFound 四向格步进表（0=左 1=右 2=上 3=下，{dX, dY}）：原四 case
	// 同构增量运算 -> 数据表（i 循环域 0..3 直下标，原 switch 无 default）。
	constexpr int kSquirrelStep[4][2] = {
		{-1, 0},  // 0 左
		{1, 0},   // 1 右
		{0, -1},  // 2 上
		{0, 1},   // 3 下
	};
	static_assert(sizeof(kSquirrelStep) / sizeof(kSquirrelStep[0]) == 4,
		"kSquirrelStep 槽位须与四向同步");
}

//0x42BE10
// GOTY 1073: 0042EB10
void Challenge::SquirrelFound(GridItem* theSquirrel)
{
	if (theSquirrel->mGridItemState == GRIDITEM_STATE_SQUIRREL_ZOMBIE)
	{
		Zombie* aZombie = mBoard->AddZombieInRow(ZOMBIE_NORMAL, theSquirrel->mGridY, 0);
		{ aZombie->mX = (mBoard->GridToPixelX(theSquirrel->mGridX, theSquirrel->mGridY)); aZombie->mRootBindingGeneration = 0; }
		theSquirrel->GridItemDie();
		mBoard->DisplayAdvice(gStr.ADVICE_SQUIRREL_ZOMBIE, MESSAGE_STYLE_HINT_FAST, ADVICE_NONE);
	}
	else
	{
		TodWeightedGridArray aPicks[4];
		int aPickCount = 0;
		for (int i = 0; i < 4; i++)
		{
			// B 形态 -> 常量表（上方 kSquirrelStep）：i 循环域 0..3 直下标
			//（左/右/上/下格子增量）。
			const int* aStep = kSquirrelStep[i];
			int aGridX = theSquirrel->mGridX + aStep[0];
			int aGridY = theSquirrel->mGridY + aStep[1];

			if (!mBoard->GetGridItemAt(GRIDITEM_SQUIRREL, aGridX, aGridY) && mBoard->GetTopPlantAt(aGridX, aGridY, TOPPLANT_EATING_ORDER))
			{
				aPicks[aPickCount].mX = aGridX;
				aPicks[aPickCount].mY = aGridY;
				aPicks[aPickCount].mWeight = 1;
				aPickCount++;
			}
		}

		if (aPickCount > 0)
		{
			TodWeightedGridArray* aGrid = TodPickFromWeightedGridArray(aPicks, aPickCount);

			theSquirrel->mGridItemState =
				aGrid->mX < theSquirrel->mGridX ? GRIDITEM_STATE_SQUIRREL_RUNNING_LEFT :
				aGrid->mX > theSquirrel->mGridX ? GRIDITEM_STATE_SQUIRREL_RUNNING_RIGHT :
				aGrid->mY < theSquirrel->mGridY ? GRIDITEM_STATE_SQUIRREL_RUNNING_UP : GRIDITEM_STATE_SQUIRREL_RUNNING_DOWN;

			theSquirrel->mStateEndTimeNs = static_cast<std::int64_t>(mBoard->mLevelTimeNs)+500'000'000;
			theSquirrel->mGridX = aGrid->mX;
			theSquirrel->mGridY = aGrid->mY;
			theSquirrel->mRenderOrder = Board::MakeRenderOrder(RENDER_LAYER_GRAVE_STONE, aGrid->mY, 1);
		}
		else
		{
			theSquirrel->mGridItemState = GRIDITEM_STATE_SQUIRREL_CAUGHT;
			theSquirrel->mStateEndTimeNs = static_cast<std::int64_t>(mBoard->mLevelTimeNs)+1'000'000'000;

			int aSquirrelsRemaining = SquirrelCountUncaught();
			if (aSquirrelsRemaining)
			{
				SexyString aMessage = mApp->Pluralize(aSquirrelsRemaining, gStr.ADVICE_SQUIRRELS_ONE_LEFT.c_str(), gStr.ADVICE_SQUIRRELS_LEFT.c_str());
				mBoard->DisplayAdvice(aMessage, MESSAGE_STYLE_HINT_FAST, ADVICE_NONE);
			}
			else
			{
				mBoard->ClearAdvice(ADVICE_NONE);
				SpawnLevelAward(theSquirrel->mGridX, theSquirrel->mGridY);
			}
		}
	}
}

//0x42C1B0
void Challenge::SquirrelUpdateOne(GridItem* theSquirrel)
{
	auto& aCounter = theSquirrel->mStateEndTimeNs;
    const auto nowNs=static_cast<std::int64_t>(mBoard->mLevelTimeNs);

	GridItemState& aState = theSquirrel->mGridItemState;
	if (aState == GRIDITEM_STATE_SQUIRREL_WAITING || aState == GRIDITEM_STATE_SQUIRREL_ZOMBIE)
	{
		if (!mBoard->GetTopPlantAt(theSquirrel->mGridX, theSquirrel->mGridY, TOPPLANT_EATING_ORDER))
		{
			SquirrelFound(theSquirrel);
		}
		if (aCounter <= nowNs)
		{
			if (Rand(2) && aState != GRIDITEM_STATE_SQUIRREL_ZOMBIE)
			{
				theSquirrel->mStateEndTimeNs = static_cast<std::int64_t>(mBoard->mLevelTimeNs)+500'000'000;
				theSquirrel->mGridItemState = GRIDITEM_STATE_SQUIRREL_PEEKING;
			}
			else
			{
				SquirrelChew(theSquirrel);
			}
		}
	}

	if (aState >= GRIDITEM_STATE_SQUIRREL_PEEKING && aState <= GRIDITEM_STATE_SQUIRREL_RUNNING_RIGHT && aCounter <= nowNs)
	{
		aState = GRIDITEM_STATE_SQUIRREL_WAITING;
		aCounter = nowNs+static_cast<std::int64_t>(RandRangeInt(100, 500))*10'000'000;
	}
	if (aState == GRIDITEM_STATE_SQUIRREL_CAUGHT && aCounter <= nowNs)
	{
		theSquirrel->GridItemDie();
	}
}

//0x42C2C0
void Challenge::SquirrelUpdate()
{
	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GRIDITEM_SQUIRREL)
		{
			SquirrelUpdateOne(aGridItem);
		}
	}

	mChallengeScore = MAX_SQUIRRELS - SquirrelCountUncaught();
	mBoard->mProgressMeterWidth = TodAnimateCurve(0, MAX_SQUIRRELS, mChallengeScore, 0, PROGRESS_METER_COUNTER, Curves::Linear);
}

//0x42C340
void Challenge::UpdateRain()
{
	mRainCounter--;
	if (mRainCounter < 0 && !mBoard->mCutScene->IsBeforePreloading())
	{
		float aPosX = RandRangeFloat(40.0f, 740.0f);
		float aPosY = RandRangeFloat(90.0f, 240.0f);
		Reanimation* aSplashReanim = mApp->AddReanimation(aPosX, aPosY, RENDER_LAYER_GROUND, REANIM_RAIN_SPLASH, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
		aSplashReanim->mColorOverride = Color(255, 255, 255, RandRangeInt(100, 200));
		float aScale = RandRangeFloat(0.7f, 1.2f);
		aSplashReanim->mOverlayMatrix.m00 = aScale;
		aSplashReanim->mOverlayMatrix.m11 = aScale;

		aPosX = RandRangeFloat(40.0f, 740.0f);
		aPosY = RandRangeFloat(290.0f, 410.0f);
		Reanimation* aCircleReanim = mApp->AddReanimation(aPosX, aPosY, RENDER_LAYER_GROUND, REANIM_RAIN_CIRCLE, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
		(void)aCircleReanim; // unused
		aSplashReanim->mColorOverride = Color(255, 255, 255, RandRangeInt(50, 150));
		aScale = RandRangeFloat(0.7f, 1.1f);
		aSplashReanim->mOverlayMatrix.m00 = aScale;
		aSplashReanim->mOverlayMatrix.m11 = aScale;

		aPosX = RandRangeFloat(40.0f, 740.0f);
		aPosY = RandRangeFloat(450.0f, 580.0f);
		Reanimation* aRainReanim = mApp->AddReanimation(aPosX, aPosY, RENDER_LAYER_GROUND, REANIM_RAIN_SPLASH, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
		(void)aRainReanim; // unused
		aSplashReanim->mColorOverride = Color(255, 255, 255, RandRangeInt(100, 200));
		aScale = RandRangeFloat(0.7f, 1.2f);
		aSplashReanim->mOverlayMatrix.m00 = aScale;
		aSplashReanim->mOverlayMatrix.m11 = aScale;

        mRainCounter = RandRangeInt(10, 20);
	}
}

//0x42C5C0
void Challenge::LastStandUpdate()
{
	if (mBoard->mNextSurvivalStageCounter == 0 && mChallengeState == STATECHALLENGE_NORMAL && mBoard->mStoreButton->mBtnNoDraw)
	{
		GameButton* aButton = mBoard->mStoreButton;
		aButton->mBtnNoDraw = false;
		aButton->mDisabled = false;

		if (mSurvivalStage == 0)
		{
			aButton->mLabel = gStr.START_ONSLAUGHT;
			aButton->mX = 300;
			aButton->mY = 559;
			aButton->mWidth = 210;
			aButton->mHeight = 46;
		}
		else
		{
			aButton->mLabel = gStr.CONTINUE_ONSLAUGHT;
			aButton->mX = 270;
			aButton->mY = 559;
			aButton->mWidth = 257;
			aButton->mHeight = 46;
		}
	}

	if (mChallengeState == STATECHALLENGE_LAST_STAND_ONSLAUGHT && mApp->mGameScene == SCENE_PLAYING)
		mChallengeStateCounter++;
}

//0x42C6F0
void Challenge::LastStandCompletedStage()
{
	mApp->PlaySample(Sexy::SOUND_HUGE_WAVE);
	mChallengeState = STATECHALLENGE_NORMAL;
	mBoard->mSeedBank->RefreshAllPackets();

	Plant* aPlant = nullptr;
	while (mBoard->IteratePlants(aPlant))
	{
		if (aPlant->mState == STATE_CHOMPER_DIGESTING || aPlant->mState == STATE_COBCANNON_ARMING ||
			aPlant->mState == STATE_MAGNETSHROOM_SUCKING || aPlant->mState == STATE_MAGNETSHROOM_CHARGING)
		{
			aPlant->mStateCountdown = std::min<Sexy::TimeCounter>(aPlant->mStateCountdown, 200);
		}
	}

	SexyString aFlagStr = mApp->Pluralize(mBoard->GetSurvivalFlagsCompleted(), gStr.ONE_FLAG.c_str(), gStr.COUNT_FLAGS.c_str());
	SexyString aMsg = TodReplaceString(gStr.SUCCESSFULLY_DEFENDED, UiTextPlaceholder::Flags, aFlagStr);
	mBoard->DisplayAdvice(aMsg, MESSAGE_STYLE_BIG_MIDDLE_FAST, ADVICE_NONE);

	mSurvivalStage++;
	mBoard->mLevelComplete = false;
	mBoard->InitZombieWaves();
}

//0x42C9B0
void Challenge::WhackAZombieUpdate()
{
	if (mBoard->mSunMoney > 0 && mBoard->mTutorialState == TUTORIAL_OFF)
	{
		mBoard->SetTutorialState(TUTORIAL_WHACK_A_ZOMBIE_BEFORE_PICK_SEED);
		mBoard->mTutorialTimer = 1500;
	}
	if (mBoard->mTutorialState == TUTORIAL_WHACK_A_ZOMBIE_BEFORE_PICK_SEED && mBoard->mTutorialTimer == 0)
	{
		mBoard->SetTutorialState(TUTORIAL_WHACK_A_ZOMBIE_PICK_SEED);
		mBoard->mTutorialTimer = 400;
	}
	if (mBoard->mTutorialState == TUTORIAL_WHACK_A_ZOMBIE_PICK_SEED && mBoard->mTutorialTimer == 0)
	{
		mBoard->SetTutorialState(TUTORIAL_WHACK_A_ZOMBIE_COMPLETED);
	}
}

int Challenge::TreeOfWisdomMouseOn(int theX, int theY)
{
	HitResult aHitResult;
	mBoard->MouseHitTest(theX, theY, &aHitResult);
	return (aHitResult.mObjectType == OBJECT_TYPE_TREE_OF_WISDOM && mBoard->mCursorObject->mCursorType == CURSOR_TYPE_TREE_FOOD);
}

int Challenge::TreeOfWisdomGetSize()
{
	return mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()];
}

//0x42CA30
void Challenge::TreeOfWisdomDraw(Graphics* g)
{
	int aMouseOn = TreeOfWisdomMouseOn(mApp->mWidgetManager->mLastMouseX - mBoard->mX, mApp->mWidgetManager->mLastMouseY - mBoard->mY);

	Reanimation* aReanimTree = mApp->ReanimationGet(mReanimChallenge);
	aReanimTree->mEnableExtraOverlayDraw = false;
	const Rect& view = mApp->mScreenBounds;
	// Ultra uses this same gradient sky; only the backdrop fills the viewport.
	g->DrawImage(Sexy::IMAGE_REANIM_TREE_BG,
		Rect(view.mX - mBoard->mX, view.mY - mBoard->mY, view.mWidth, view.mHeight),
		Rect(0, 0, Sexy::IMAGE_REANIM_TREE_BG->mWidth, Sexy::IMAGE_REANIM_TREE_BG->mHeight));
	for (int i = 0; i < 6; i++)
	{
		mApp->ReanimationGet(mReanimClouds[i])->Draw(g);
	}

	int aHeight = TreeOfWisdomGetSize();
	if (aMouseOn)
	{
		aReanimTree->mExtraOverlayColor = Color(255, 255, 255, aHeight < 18 ? 128 : 48);
		aReanimTree->mEnableExtraOverlayDraw = true;
	}
	else
	{
		aReanimTree->mEnableExtraOverlayDraw = false;
	}
	aReanimTree->DrawRenderGroup(g, 2);  // 绘制树干

	aReanimTree->mEnableExtraOverlayDraw = false;
	aReanimTree->DrawRenderGroup(g, 3);  // 绘制土地

	if (aMouseOn)
	{
		aReanimTree->mExtraOverlayColor = Color(255, 255, 255, 32);
		aReanimTree->mEnableExtraOverlayDraw = true;
	}
	else
	{
		aReanimTree->mEnableExtraOverlayDraw = false;
	}
	aReanimTree->DrawRenderGroup(g, 4);  // 绘制根部

	if (mChallengeState == STATECHALLENGE_TREE_GIVE_WISDOM || mChallengeState == STATECHALLENGE_TREE_BABBLING)
	{
		int aPosX, aPosY;
		if (aHeight < 7)
		{
			aPosX = 400;
			aPosY = 152;
		}
		else if (aHeight < 12)
		{
			aPosX = 395;
			aPosY = 60;
		}
		else
		{
			aPosX = 390;
			aPosY = 52;
		}

		g->DrawImage(Sexy::IMAGE_STORE_SPEECHBUBBLE2, aPosX, aPosY);
		SexyString aText = EntityTreeOfWisdomText(mTreeOfWisdomTalkIndex);
		TodDrawStringWrapped(g, aText, Rect(aPosX + 25, aPosY + 6, 233, 144), Sexy::FONT_BRIANNETOD16, Color::Black, DS_ALIGN_CENTER_VERTICAL_MIDDLE);
	}

	int aCurSize = aHeight;
	float aScale = 1.0f;
	if (mChallengeState == STATECHALLENGE_TREE_JUST_GREW)
	{
		if (mChallengeStateCounter > 30)
		{
			aCurSize--;
		}
		aScale = TodAnimateCurveNs(550'000'000, 200'000'000, mChallengeStateCounter.mNanoseconds, 1.0f, 1.2f, Curves::Bounce);
	}
	if (aCurSize >= 50)
	{
		SexyString aSizeStr = TodReplaceNumberString(gStr.TREE_OF_WISDOM_HIEGHT, UiTextPlaceholder::Height, aCurSize);
		float aStrWidth = Sexy::FONT_HOUSEOFTERROR16->StringWidth(aSizeStr) * aScale;
		float aStrHeight = Sexy::FONT_HOUSEOFTERROR16->mAscent * aScale;

		SexyTransform2D aMatrix;
		TodScaleTransformMatrix(aMatrix, 400.0f - aStrWidth * 0.5f, 20.0f + aStrHeight * 0.5f, aScale, aScale);
		aMatrix.m02 += g->mTransX;
		aMatrix.m12 += g->mTransY;
		TodDrawStringMatrix(g, Sexy::FONT_HOUSEOFTERROR16, aMatrix, aSizeStr, Color(255, 255, 255));
	}
}

//0x42CEE0
void Challenge::TreeOfWisdomInit()
{
	ReanimatorEnsureDefinitionLoaded(REANIM_TREEOFWISDOM, true);
	Reanimation* aReanimTree = mApp->AddReanimation(0.5f, 0.5f, 0, REANIM_TREEOFWISDOM, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
	aReanimTree->mIsAttachment = true;
	aReanimTree->AssignRenderGroupToRange(ReanimTrackRanges::bg, 1);
	aReanimTree->AssignRenderGroupToRange(ReanimTrackRanges::tree, 2);
	aReanimTree->AssignRenderGroupToRange(ReanimTrackRanges::grass, 3);
	aReanimTree->AssignRenderGroupToRange(ReanimTrackRanges::overlay, 4);
	aReanimTree->AssignRenderGroupToRange(ReanimTrackRanges::leaf, 4);
	aReanimTree->AssignRenderGroupToRange(ReanimTrackRanges::bunch, 4);
	aReanimTree->SetTruncateDisappearingFramesAll( false);
	mReanimChallenge = mApp->ReanimationGetID(aReanimTree);

	int aTreeSize = ClampInt(TreeOfWisdomGetSize(), 1, 50);
	aReanimTree->PlayReanim(aGrowLayers[aTreeSize - 1], REANIM_PLAY_ONCE_AND_HOLD, 0, 18.0f);
	// The original clamped size cannot enter its size-zero introduction branch.
	// Returning to the garden displays the completed tree; food plays growth.
	aReanimTree->Seek(990);

	for (int i = 0; i < 6; i++)
	{
		static constexpr ReanimTrackId aCloudLayers[] = {
			ReanimTrackId::Cloud1, ReanimTrackId::Cloud2, ReanimTrackId::Cloud3,
			ReanimTrackId::Cloud4, ReanimTrackId::Cloud5, ReanimTrackId::Cloud6,
			ReanimTrackId::Cloud7};
		Reanimation* aReanimCloud = mApp->AddReanimation(0, 0, 0, REANIM_TREEOFWISDOM_CLOUDS, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
		aReanimCloud->PlayReanim(aCloudLayers[i], REANIM_PLAY_ONCE_AND_HOLD, 0, 0);
		mReanimClouds[i] = mApp->ReanimationGetID(aReanimCloud);

		int aCloudCounter = RandRangeInt(-6000, 2000);
		mCloudsCounter[i] = aCloudCounter;
		if (aCloudCounter < 0)
		{
			aReanimCloud->Seek(-aCloudCounter, 6000);
			aReanimCloud->ApplyRate(0.2f);
			mCloudsCounter[i] = 0;
		}
		else
		{
			aReanimCloud->ApplyRate(0.0f);
		}
	}

	mChallengeState = STATECHALLENGE_TREE_WAITING_TO_BABBLE;
	mChallengeStateCounter = RandRangeInt(700, 1500);
}

//0x42D1F0
// GOTY @Patoke: 0x42FF00
void Challenge::TreeOfWisdomGrow()
{
	const auto domain = mBoard->AnimationTimeDomain();
	const auto nowNs = domain == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) :
		mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	Reanimation* aTree = mApp->ReanimationGet(mReanimChallenge);
	mApp->mEffectSystem->mReanimationHolder->BindAnimationTime(*aTree, domain, nowNs);
	aTree->PrepareTimeAt(nowNs);
	mApp->mPlayerInfo->mChallengeRecords[mApp->GetCurrentChallengeIndex()]++;
	int aTreeSize = TreeOfWisdomGetSize();
	aTree->PlayReanim(aGrowLayers[ClampInt(aTreeSize, 1, 51) - 1], REANIM_PLAY_ONCE_AND_HOLD, 0, 8.0f);
	mApp->PlayFoley(FOLEY_PLANTGROW);

	if (aTreeSize > 1)
	{
		mChallengeState = STATECHALLENGE_TREE_JUST_GREW;
		mChallengeStateCounter = 120;
	}
	else
	{
		mChallengeState = STATECHALLENGE_NORMAL;
	}

	if (aTreeSize == 100) // @Patoke: add achievement
		ReportAchievement::GiveAchievement(mApp, ToweringWisdom, true);
}

//0x42D360
void Challenge::TreeOfWisdomFertilize()
{
	GridItem* aTreeFood = mBoard->mGridItems.DataArrayAlloc();
	aTreeFood->mPosX = 340.0f;
	aTreeFood->mPosY = 300.0f;
	aTreeFood->mGridItemType = GRIDITEM_ZEN_TOOL;
	aTreeFood->mGridX = 0;
	aTreeFood->mGridY = 0;
	aTreeFood->mRenderOrder = Board::MakeRenderOrder(RENDER_LAYER_ABOVE_UI, 0, 0);
	Reanimation* aReanim = mApp->AddReanimation(340.0f, 300.0f, 0, REANIM_TREEOFWISDOM_TREEFOOD, (mBoard && mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs), (mBoard ? mBoard->AnimationTimeDomain() : ReanimTimeDomain::Visual));
	aReanim->ChangeLoopType(REANIM_PLAY_ONCE_AND_HOLD);
	aTreeFood->mGridItemReanimID = mApp->ReanimationGetID(aReanim);
	aTreeFood->mGridItemState = GRIDITEM_STATE_ZEN_TOOL_FERTILIZER;

	mApp->PlayFoley(FOLEY_FERTILIZER);
	mApp->mPlayerInfo->mPurchases[STORE_ITEM_TREE_FOOD]--;
	mChallengeState = STATECHALLENGE_NORMAL;
	mBoard->ClearCursor();
}

//0x42D480
void Challenge::TreeOfWisdomBabble()
{
	mChallengeState = STATECHALLENGE_TREE_BABBLING;
	mChallengeStateCounter = 400;

	int aTreeSize = TreeOfWisdomGetSize();
	int aBabbleHit = RandRangeInt(0, 2);
	if (aTreeSize <= 1)
	{
		mTreeOfWisdomTalkIndex = 600;
	}
	else if (aBabbleHit == 0 && aTreeSize >= 5)
	{
		mTreeOfWisdomTalkIndex = 500;
	}
	else if (aBabbleHit == 1)
	{
		mTreeOfWisdomTalkIndex = RandRangeInt(101, 110);
	}
	else
	{
		mTreeOfWisdomTalkIndex = Rand(5) + (aTreeSize < 12 ? 201 : aTreeSize < 50 ? 301 : 401);
	}
}

namespace
{
// 树高里程碑→智慧串索引表（B 形态；精确匹配支逐值入表）：
// 命中 → kTreeWisdomMilestoneTalk；超过最大里程碑 → 1100；其余 → ClampInt(树高-1, 1, 49)
// （后两者为原表外/else 支，见调用点）。
constexpr int kTreeWisdomMilestoneSize[3] = { 100, 500, 1000 };
constexpr int kTreeWisdomMilestoneTalk[3] = { 800, 900, 1000 };
static_assert(sizeof(kTreeWisdomMilestoneSize) / sizeof(kTreeWisdomMilestoneSize[0]) == 3 &&
	sizeof(kTreeWisdomMilestoneTalk) / sizeof(kTreeWisdomMilestoneTalk[0]) == 3,
	"树高里程碑表须为 3 槽（100/500/1000）");
}  // namespace

//0x42D530
void Challenge::TreeOfWisdomGiveWisdom()
{
	mChallengeState = STATECHALLENGE_TREE_GIVE_WISDOM;
	mChallengeStateCounter = 1000;

	int aTreeSize = TreeOfWisdomGetSize();
	// B 形态 -> 里程碑表（上方 kTreeWisdomMilestoneSize/kTreeWisdomMilestoneTalk）；
	// 超里程碑（>1000 → 1100）与常规支（ClampInt）表外保留
	mTreeOfWisdomTalkIndex = -1;
	for (int i = 0; i < 3; i++)
	{
		if (aTreeSize == kTreeWisdomMilestoneSize[i])
		{
			mTreeOfWisdomTalkIndex = kTreeWisdomMilestoneTalk[i];
			break;
		}
	}
	if (mTreeOfWisdomTalkIndex == -1)
	{
		mTreeOfWisdomTalkIndex = aTreeSize > kTreeWisdomMilestoneSize[2] ? 1100 : ClampInt(aTreeSize - 1, 1, 49);
	}
}

//0x42D5C0
void Challenge::TreeOfWisdomSayRepeat()
{
	int aTreeSize = TreeOfWisdomGetSize();
	if (aTreeSize >= 100 && Rand(47) == 0)
	{
		mTreeOfWisdomTalkIndex = 800;
	}
	else if (aTreeSize >= 500 && Rand(47) == 0)
	{
		mTreeOfWisdomTalkIndex = 900;
	}
	else if (aTreeSize >= 1000 && Rand(47) == 0)
	{
		mTreeOfWisdomTalkIndex = 1000;
	}
	else
	{
		mTreeOfWisdomTalkIndex = RandRangeInt(2, ClampInt(aTreeSize, 3, 49) - 1);
	}

	mChallengeStateCounter = 600;
}

void Challenge::TreeOfWisdomToolUpdate(GridItem* theZenTool)
{
	Reanimation* aReanim = mApp->ReanimationTryToGet(theZenTool->mGridItemReanimID);
	if (aReanim && aReanim->mLoopCount > 0)
	{
		TreeOfWisdomGrow();
		theZenTool->GridItemDie();
	}
}

//0x42D680
void Challenge::TreeOfWisdomUpdate()
{
	const auto domain = mBoard->AnimationTimeDomain();
	const auto nowNs = domain == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) :
		mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	// Initialization can run on visual time before the playing scene starts.
	// Rebase the existing pose before Challenge::Update supplies battle time.
	Reanimation* aTree = mApp->ReanimationGet(mReanimChallenge);
	mApp->mEffectSystem->mReanimationHolder->BindAnimationTime(*aTree, domain, nowNs);
	aTree->PrepareTimeAt(nowNs);
	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		TreeOfWisdomToolUpdate(aGridItem);
	}

	if (mApp->mGameScene == SCENE_PLAYING && mChallengeStateCounter > 0)
	{
		mChallengeStateCounter--;
	}
	if (mChallengeStateCounter == 0)
	{
		if (mChallengeState == STATECHALLENGE_TREE_JUST_GREW)
		{
			TreeOfWisdomGiveWisdom();
		}
		else if (mChallengeState == STATECHALLENGE_TREE_WAITING_TO_BABBLE)
		{
			TreeOfWisdomBabble();
		}
		else if (mChallengeState == STATECHALLENGE_TREE_BABBLING || mChallengeState == STATECHALLENGE_TREE_GIVE_WISDOM)
		{
			if (mTreeOfWisdomTalkIndex == 500)
			{
				TreeOfWisdomSayRepeat();
			}
			else
			{
				mChallengeState = STATECHALLENGE_TREE_WAITING_TO_BABBLE;
				mChallengeStateCounter = RandRangeInt(700, 1000);
			}
		}
	}

	for (int i = 5; i >= 0; i--) // Off by one error!
	{
		Reanimation* aReanimCloud = mApp->ReanimationGet(mReanimClouds[i]);
		mApp->mEffectSystem->mReanimationHolder->BindAnimationTime(*aReanimCloud, domain, nowNs);
		aReanimCloud->PrepareTimeAt(nowNs);
		if (mCloudsCounter[i] > 0)
		{
			mCloudsCounter[i]--;
			if (mCloudsCounter[i] == 0)
			{
				aReanimCloud->mLoopCount = 0;
				aReanimCloud->Seek(0);
				aReanimCloud->ApplyRate(0.2f);
			}
		}
		else if (aReanimCloud->mLoopCount > 0)
		{
			mCloudsCounter[i] = RandRangeInt(2000, 4000);
		}

		aReanimCloud->UpdateAt(nowNs);
	}
}

//0x42D7E0
void Challenge::TreeOfWisdomLeave()
{
	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GRIDITEM_ZEN_TOOL)
		{
			TreeOfWisdomGrow();
			aGridItem->GridItemDie();
		}
	}
}

//0x42D830
void Challenge::TreeOfWisdomNextGarden()
{
	TreeOfWisdomLeave();
	mApp->KillBoard();
	mApp->PreNewGame(GAMEMODE_LIMBO_ZEN_GARDEN, false);
}

//0x42D8C0
void Challenge::TreeOfWisdomOpenStore()
{
	TreeOfWisdomLeave();
	StoreScreen* aStore = mApp->ShowStoreScreen();
	aStore->mBackButton->mLabel = gStr.STORE_BACK_TO_GAME;
	aStore->mPage = STORE_PAGE_ZEN2;
	aStore->WaitForResult(true);
	mApp->mMusic->MakeSureMusicIsPlaying(MUSIC_TUNE_ZEN_GARDEN);
}

//0x42D9A0
void Challenge::TreeOfWisdomTool(int theMouseX, int theMouseY)
{
	if (TreeOfWisdomMouseOn(theMouseX, theMouseY))
	{
		TreeOfWisdomFertilize();
	}

	mBoard->ClearCursor();
}

//0x42D9E0
int Challenge::TreeOfWisdomHitTest(int theX, int theY, HitResult* theHitResult)
{
	Rect aTreeRect;
	int aTreeSize = TreeOfWisdomGetSize();
	// GOTY 1073 (00430710): x, y, width, height; y and width were transposed.
	if (aTreeSize <= 1)			aTreeRect = Rect(310, 275, 175, 175);
	else if (aTreeSize < 7)		aTreeRect = Rect(290, 255, 205, 195);
	else if (aTreeSize < 12)	aTreeRect = Rect(290, 215, 205, 225);
	else						aTreeRect = Rect(280, 155, 225, 305);

	if (aTreeRect.Contains(theX, theY))
	{
		theHitResult->mObject = nullptr;
		theHitResult->mObjectType = OBJECT_TYPE_TREE_OF_WISDOM;
		return true;
	}
	else
	{
		theHitResult->mObject = nullptr;
		theHitResult->mObjectType = OBJECT_TYPE_NONE;
		return false;
	}
}

//0x42DA90
int Challenge::TreeOfWisdomCanFeed()
{
	if (mChallengeState == STATECHALLENGE_TREE_JUST_GREW)
		return false;

	GridItem* aGridItem = nullptr;
	while (mBoard->IterateGridItems(aGridItem))
	{
		if (aGridItem->mGridItemType == GRIDITEM_ZEN_TOOL)
		{
			return false;
		}
	}

	return true;
}
