#include "ReanimClock.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Board.h"
#include "BoardTouch.h"
#include "Cutscene.h"
#include "Challenge.h"
#include "SeedPacket.h"
#include "LandscapeHud.h"
#include "Sexy.TodLib/generated/SceneAtlasData.h"
#include "graphics/AtlasDraw.h"
#include "LawnCommon.h"
#include "LawnApp.h"
#include "CursorObject.h"
#include "Resources.h"
#include "MessageWidget.h"
#include "graphics/Font.h"
#include "graphics/DDInterface.h"
#include "graphics/RenderInterface.h"
#include "misc/SexyMatrix.h"

SeedPacket::SeedPacket()
{
	mPacketType = SeedType::SEED_NONE;
	mImitaterType = SeedType::SEED_NONE;
	mIndex = -1;
	mSlotMachiningNextSeed = SeedType::SEED_NONE;
	mWidth = 50;
	mHeight = 70;
	mRefreshStartTimeNs = mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : 0;
	mRefreshTime = 0;
	mRefreshing = false;
	mActive = true;
	mOffsetX = 0;
	mSlotMachineEndTimeNs = 0;
	mSlotMachiningPosition = 0;
	mSlotMachiningNextSeed = SeedType::SEED_NONE;
	mTimesUsed = 0;
}

//0x487070
void SeedPacket::PickNextSlotMachineSeed()
{
	int aPeasCount = mBoard->CountPlantByType(SeedType::SEED_PEASHOOTER);

	SeedType SLOT_SEED_TYPES[] = {
		SeedType::SEED_SUNFLOWER,
		SeedType::SEED_PEASHOOTER,
		SeedType::SEED_SNOWPEA,
		SeedType::SEED_WALLNUT,
		SeedType::SEED_SLOT_MACHINE_SUN,
		SeedType::SEED_SLOT_MACHINE_DIAMOND
	};

	int aSeedsCount = 0;
	TodWeightedArray aSeedWeightArray[(int)SeedType::NUM_SEED_TYPES];
	for (size_t i = 0; i < LENGTH(SLOT_SEED_TYPES); i++)
	{
		SeedType aSeedType = SLOT_SEED_TYPES[i];

		int aWeight = 100;
		if (aSeedType == SeedType::SEED_PEASHOOTER)
		{
			aWeight = TodAnimateCurve(0, 5, aPeasCount, 200, 100, Curves::Linear);
		}
		else if (aSeedType == SeedType::SEED_SLOT_MACHINE_DIAMOND)
		{
			aWeight = 30;
		}

		if (mIndex == 2 && aSeedType != SeedType::SEED_SLOT_MACHINE_DIAMOND)
		{
			if (aSeedType == mBoard->mSeedBank->mSeedPackets[0].mSlotMachiningNextSeed || aSeedType == mBoard->mSeedBank->mSeedPackets[1].mSlotMachiningNextSeed)
			{
				aWeight += aWeight / 2;
			}
		}

		aSeedWeightArray[aSeedsCount].mItem = (int)aSeedType;
		aSeedWeightArray[aSeedsCount].mWeight = aWeight;
		aSeedsCount++;
	}

	mSlotMachiningNextSeed = (SeedType)TodPickFromWeightedArray(aSeedWeightArray, aSeedsCount);
}

void SeedPacket::SlotMachineStart()
{
	mSlotMachineEndTimeNs = ReanimClock::Add(static_cast<std::int64_t>(mBoard->mLevelTimeNs), SlotMachineMotion::DurationNs);
	mSlotMachiningPosition = 0;
	PickNextSlotMachineSeed();
}

//0x487190
void SeedPacket::FlashIfReady()
{
	if (!CanPickUp() || mApp->mEasyPlantingCheat)
		return;
    if (mBoard->mLandscapeHud) {
        if (mIndex >= 0 && mIndex < SEEDBANK_MAX) mBoard->mHudFlashStartNs[mIndex]=static_cast<std::int64_t>(mBoard->mLevelTimeNs);
    }
	else if (!mBoard->HasConveyorBeltSeedBank())
	{
		int aRenderPosition = Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_UI_BOTTOM, 0, 2);
		mApp->AddTodParticle(mX + mBoard->mSeedBank->mX, mY + mBoard->mSeedBank->mY, aRenderPosition, ParticleEffect::PARTICLE_SEED_PACKET_FLASH, static_cast<std::int64_t>(mBoard->mLevelTimeNs));
	}
	
	// Flash rendering differs by HUD; cooldown completion must advance the same
	// tutorial state for both. Returning from the landscape flash branch leaves
	// level 1-2 in REFRESH_SUNFLOWER, so later planting never checks the sun count.
	if (mBoard->mTutorialState == TutorialState::TUTORIAL_LEVEL_1_REFRESH_PEASHOOTER)
	{
		mBoard->SetTutorialState(TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER);
	}
	else if (mBoard->mTutorialState == TutorialState::TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER && mPacketType == SeedType::SEED_SUNFLOWER)
	{
		mBoard->SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER);
	}
	else if (mBoard->mTutorialState == TutorialState::TUTORIAL_MORESUN_REFRESH_SUNFLOWER && mPacketType == SeedType::SEED_SUNFLOWER)
	{
		mBoard->SetTutorialState(TutorialState::TUTORIAL_MORESUN_PICK_UP_SUNFLOWER);
	}
}

void SeedPacket::Activate()
{
	TOD_ASSERT(mPacketType != SeedType::SEED_NONE);
	mActive = true;
}

void SeedPacket::SetActivate(bool theActive)
{
	if (theActive)
	{
		Activate();
	}
	else
	{
		mActive = false;
		mRefreshStartTimeNs = mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : 0;
		mRefreshTime = 0;
		mRefreshing = false;
	}
}

//0x487250
void SeedPacket::Update()
{
	if (mApp->mGameScene != GameScenes::SCENE_PLAYING || mPacketType == SeedType::SEED_NONE)
	{
		return;
	}

	if (mBoard->mLevelTimeNs == 0)
	{
		FlashIfReady();
	}

	if (!mActive && mRefreshing)
	{
		if (static_cast<std::int64_t>(mBoard->mLevelTimeNs) - mRefreshStartTimeNs > mRefreshTime.mNanoseconds)
		{
			mRefreshStartTimeNs = mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : 0;
			mRefreshing = false;
			Activate();
			FlashIfReady();
		}
	}

	if (mSlotMachineEndTimeNs > 0)
	{
		const auto nowNs = static_cast<std::int64_t>(mBoard->mLevelTimeNs);
		const auto remainingNs = std::max<std::int64_t>(0, mSlotMachineEndTimeNs - nowNs);
		const float speed = SlotMachineMotion::Speed(remainingNs);
		// Position is deliberately simulated. At 100Hz this is the original
		// float addition and decrement-before-speed order; other step sizes may differ.
		mSlotMachiningPosition += speed * static_cast<float>(
			static_cast<double>(Sexy::gUpdateIntervalNs) / 1'000'000'000.0);
		while (mSlotMachiningPosition >= 1.0f)
		{
			mPacketType = mSlotMachiningNextSeed;
			if (remainingNs == 0)
			{
				mSlotMachiningPosition = 0;
				mSlotMachineEndTimeNs = 0;
				Activate();
				return;
			}
			mSlotMachiningPosition -= 1.0f;
			PickNextSlotMachineSeed();
		}
		// Keep the original deadline expired while finishing this turn. Re-arming
		// it each update could place every wrap before expiry at non-100Hz rates.
	}

}

//0x487380
static void SeedPacketDrawSeed(Graphics* g, float x, float y, SeedType theSeedType, SeedType theImitaterType, float theOffsetX, float theOffsetY, float theScale)
{
	Image* aImage = IMAGE_PACKET_PLANTS;
	SeedType aSeedType = theSeedType;
	GpuImageColorFilter aColorFilter = GpuImageColorFilter::None;
	if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
	{
		aSeedType = theImitaterType;
		aColorFilter = GpuImageColorFilter::WashedOut;
		if (theImitaterType == SeedType::SEED_HYPNOSHROOM || theImitaterType == SeedType::SEED_SQUASH || theImitaterType == SeedType::SEED_POTATOMINE ||
			theImitaterType == SeedType::SEED_GARLIC || theImitaterType == SeedType::SEED_LILYPAD)
			aColorFilter = GpuImageColorFilter::LessWashedOut;
	}

	// B 形态 -> cel 列号表 kSeedPacketCel（原 13 支同判别式链合并；行序 =
	// SeedType 枚举序 0..52，仅 cel 号不同）。0xFF = 无行（其余种子/小体型
	// 走下方 DrawSeedType 兜底 = 原 else）；g->mScaleX <= 1.0f 门槛保留在
	// 判别式里（原 13 支均带此条件）。
	static constexpr uint8_t kSeedPacketCelNone = 0xFF;
	static constexpr uint8_t kSeedPacketCel[(int)SeedType::NUM_SEED_TYPES] = {
		kSeedPacketCelNone,  // 0 SEED_PEASHOOTER
		kSeedPacketCelNone,  // 1 SEED_SUNFLOWER
		kSeedPacketCelNone,  // 2 SEED_CHERRYBOMB
		kSeedPacketCelNone,  // 3 SEED_WALLNUT
		0,                   // 4 SEED_POTATOMINE
		kSeedPacketCelNone,  // 5 SEED_SNOWPEA
		1,                   // 6 SEED_CHOMPER
		kSeedPacketCelNone,  // 7 SEED_REPEATER
		kSeedPacketCelNone,  // 8 SEED_PUFFSHROOM
		kSeedPacketCelNone,  // 9 SEED_SUNSHROOM
		kSeedPacketCelNone,  // 10 SEED_FUMESHROOM
		kSeedPacketCelNone,  // 11 SEED_GRAVEBUSTER
		2,                   // 12 SEED_HYPNOSHROOM
		kSeedPacketCelNone,  // 13 SEED_SCAREDYSHROOM
		kSeedPacketCelNone,  // 14 SEED_ICESHROOM
		kSeedPacketCelNone,  // 15 SEED_DOOMSHROOM
		kSeedPacketCelNone,  // 16 SEED_LILYPAD
		kSeedPacketCelNone,  // 17 SEED_SQUASH
		kSeedPacketCelNone,  // 18 SEED_THREEPEATER
		kSeedPacketCelNone,  // 19 SEED_TANGLEKELP
		kSeedPacketCelNone,  // 20 SEED_JALAPENO
		kSeedPacketCelNone,  // 21 SEED_SPIKEWEED
		kSeedPacketCelNone,  // 22 SEED_TORCHWOOD
		3,                   // 23 SEED_TALLNUT
		kSeedPacketCelNone,  // 24 SEED_SEASHROOM
		kSeedPacketCelNone,  // 25 SEED_PLANTERN
		kSeedPacketCelNone,  // 26 SEED_CACTUS
		4,                   // 27 SEED_BLOVER
		kSeedPacketCelNone,  // 28 SEED_SPLITPEA
		kSeedPacketCelNone,  // 29 SEED_STARFRUIT
		5,                   // 30 SEED_PUMPKINSHELL
		kSeedPacketCelNone,  // 31 SEED_MAGNETSHROOM
		8,                   // 32 SEED_CABBAGEPULT
		kSeedPacketCelNone,  // 33 SEED_FLOWERPOT
		9,                   // 34 SEED_KERNELPULT
		kSeedPacketCelNone,  // 35 SEED_INSTANT_COFFEE
		kSeedPacketCelNone,  // 36 SEED_GARLIC
		kSeedPacketCelNone,  // 37 SEED_UMBRELLA
		kSeedPacketCelNone,  // 38 SEED_MARIGOLD
		10,                  // 39 SEED_MELONPULT
		kSeedPacketCelNone,  // 40 SEED_GATLINGPEA
		6,                   // 41 SEED_TWINSUNFLOWER
		kSeedPacketCelNone,  // 42 SEED_GLOOMSHROOM
		kSeedPacketCelNone,  // 43 SEED_CATTAIL
		11,                  // 44 SEED_WINTERMELON
		kSeedPacketCelNone,  // 45 SEED_GOLD_MAGNET
		12,                  // 46 SEED_SPIKEROCK
		7,                   // 47 SEED_COBCANNON
		kSeedPacketCelNone,  // 48 SEED_IMITATER
		kSeedPacketCelNone,  // 49 SEED_EXPLODE_O_NUT
		kSeedPacketCelNone,  // 50 SEED_GIANT_WALLNUT
		kSeedPacketCelNone,  // 51 SEED_SPROUT
		kSeedPacketCelNone,  // 52 SEED_LEFTPEATER
	};
	static_assert(sizeof(kSeedPacketCel) / sizeof(kSeedPacketCel[0]) == (int)SeedType::NUM_SEED_TYPES,
		"kSeedPacketCel 槽位须与 SeedType 枚举同步");
	if (g->mScaleX <= 1.0f &&
		(unsigned)aSeedType < sizeof(kSeedPacketCel) / sizeof(kSeedPacketCel[0]) &&
		kSeedPacketCel[(int)aSeedType] != kSeedPacketCelNone)
	{
		auto& aRender = *gSexyAppBase->mDDInterface->mRenderInterface;
		auto aSamplingFlags = aRender.sImageSamplingFlags;
		if (aColorFilter != GpuImageColorFilter::None)
			aSamplingFlags = (aSamplingFlags & ~0x30000000u) | static_cast<uint32_t>(aColorFilter);
		GpuImageSamplingScope aSamplingScope(aRender, aSamplingFlags);
		TodDrawImageRectScaledF(g, aImage, x, y, Rect(kSeedPacketCel[(int)aSeedType] * 50, 0, 50, 70), g->mScaleX, g->mScaleY);
	}
	else
	{
		Graphics aSeedG(*g);
		aSeedG.mScaleX = theScale * g->mScaleX;
		aSeedG.mScaleY = theScale * g->mScaleY;
		Plant::DrawSeedType(&aSeedG, theSeedType, theImitaterType, DrawVariation::VARIATION_NORMAL, x + theOffsetX, y + theOffsetY);
	}
}

//0x4876F0
void DrawSeedPacket(Graphics* g, float x, float y, SeedType theSeedType, SeedType theImitaterType, float thePercentDark, int theGrayness, bool theDrawCost, bool theUseCurrentCost)
{
	SeedType aSeedType = theSeedType;
	if (aSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
	{
		aSeedType = theImitaterType;
	}

	if (theGrayness != 255)
	{
		g->mColor = Color(theGrayness, theGrayness, theGrayness);
		g->mColorizeImages = true;
	}
	else if (thePercentDark > 0)
	{
		g->mColor = Color(128, 128, 128, 255);
		g->mColorizeImages = true;
	}

	int aPacketBackground =
		theSeedType == SeedType::SEED_IMITATER ? 0 :
		Plant::IsUpgrade(aSeedType) ? 1 :
		theSeedType == SeedType::SEED_BEGHOULED_BUTTON_CRATER ? 3 :
		theSeedType == SeedType::SEED_BEGHOULED_BUTTON_SHUFFLE ? 4 :
		theSeedType == SeedType::SEED_SLOT_MACHINE_SUN ? 5 :
		theSeedType == SeedType::SEED_SLOT_MACHINE_DIAMOND ? 6 :
		theSeedType == SeedType::SEED_ZOMBIQUARIUM_SNORKLE ? 7 :
		theSeedType == SeedType::SEED_ZOMBIQUARIUM_TROPHY ? 8 : 2;

	if (g->mScaleX > 1)
	{
		TodDrawImageRectScaledF(g, Sexy::IMAGE_SEEDPACKET_LARGER, x, y, Rect(0, 0, IMAGE_SEEDPACKET_LARGER->mWidth, IMAGE_SEEDPACKET_LARGER->mHeight), g->mScaleX * 0.5f, g->mScaleY * 0.5f);
	}
	else
	{
		TodDrawImageRectScaledF(g, Sexy::IMAGE_SEEDS, x, y, Rect(aPacketBackground * 50, 0, 50, 70), g->mScaleX, g->mScaleY);
	}

	// 种子卡内图绘制参数（缩放/偏移）：按 SeedType 槽位直查，原 switch 长链退役。
	// 行序 = 枚举序（0..74）；53 是 NUM_SEED_TYPES 哨兵位，无种子；SEED_NONE=-1 越界走默认。
	struct SeedPacketDrawMetrics
	{
		float mScale;
		float mOffsetX;
		float mOffsetY;
		bool mDrawInMiddle;
	};
	static constexpr SeedPacketDrawMetrics kSeedPacketDefault = { 0.5f, 5.0f, 8.0f, true };
	static constexpr SeedPacketDrawMetrics kSeedPacketDrawMetrics[] = {
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_PEASHOOTER
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_SUNFLOWER
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_CHERRYBOMB
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_WALLNUT
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_POTATOMINE
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_SNOWPEA
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_CHOMPER
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_REPEATER
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_PUFFSHROOM
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_SUNSHROOM
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_FUMESHROOM
		{ 0.4f, 10.0f,  15.0f, true },   // SEED_GRAVEBUSTER
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_HYPNOSHROOM
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_SCAREDYSHROOM
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_ICESHROOM
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_DOOMSHROOM
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_LILYPAD
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_SQUASH
		{ 0.5f,  5.0f,  10.0f, true },   // SEED_THREEPEATER
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_TANGLEKELP
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_JALAPENO
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_SPIKEWEED
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_TORCHWOOD
		{ 0.3f, 12.0f,  22.0f, true },   // SEED_TALLNUT
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_SEASHROOM
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_PLANTERN
		{ 0.5f,  9.0f,  13.0f, true },   // SEED_CACTUS
		{ 0.4f,  8.0f,  17.0f, true },   // SEED_BLOVER
		{ 0.45f, 12.0f,  12.0f, true },  // SEED_SPLITPEA
		{ 0.5f,  6.0f,   8.0f, true },   // SEED_STARFRUIT
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_PUMPKINSHELL
		{ 0.5f,  5.0f,  12.0f, true },   // SEED_MAGNETSHROOM
		{ 0.4f, 15.0f,  14.0f, true },   // SEED_CABBAGEPULT
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_FLOWERPOT
		{ 0.4f, 13.0f,  14.0f, true },   // SEED_KERNELPULT
		{ 0.55f, 0.0f,   9.0f, true },   // SEED_INSTANT_COFFEE
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_GARLIC
		{ 0.5f,  5.0f,  10.0f, true },   // SEED_UMBRELLA
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_MARIGOLD
		{ 0.35f, 18.0f,  19.0f, true },  // SEED_MELONPULT
		{ 0.5f,  2.0f,   8.0f, true },   // SEED_GATLINGPEA
		{ 0.45f, 7.0f,  14.0f, true },   // SEED_TWINSUNFLOWER
		{ 0.45f, 7.0f,  14.0f, true },   // SEED_GLOOMSHROOM
		{ 0.45f, 8.0f,  13.0f, true },   // SEED_CATTAIL
		{ 0.35f, 18.0f,  19.0f, true },  // SEED_WINTERMELON
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_GOLD_MAGNET
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_SPIKEROCK
		{ 0.26f, 6.0f,  22.0f, true },   // SEED_COBCANNON
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_IMITATER
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_EXPLODE_O_NUT
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_GIANT_WALLNUT
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_SPROUT
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_LEFTPEATER
		kSeedPacketDefault,              // 53 = NUM_SEED_TYPES 哨兵位（无种子）
		{ 0.5f,  5.0f,   8.0f, false },  // SEED_BEGHOULED_BUTTON_SHUFFLE
		{ 0.5f,  5.0f,   8.0f, false },  // SEED_BEGHOULED_BUTTON_CRATER
		{ 0.5f,  5.0f,   8.0f, false },  // SEED_SLOT_MACHINE_SUN
		{ 0.5f,  5.0f,   8.0f, false },  // SEED_SLOT_MACHINE_DIAMOND
		{ 0.5f,  5.0f,   8.0f, false },  // SEED_ZOMBIQUARIUM_SNORKLE
		{ 0.5f,  5.0f,   8.0f, false },  // SEED_ZOMBIQUARIUM_TROPHY
		{ 0.35f, -3.0f,  -7.0f, true },  // SEED_ZOMBIE_NORMAL
		{ 0.35f, -3.0f,  -7.0f, true },  // SEED_ZOMBIE_TRAFFIC_CONE
		{ 0.35f, -8.0f, -12.0f, true },  // SEED_ZOMBIE_POLEVAULTER
		{ 0.35f, -3.0f,  -7.0f, true },  // SEED_ZOMBIE_PAIL
		{ 0.35f, -3.0f, -10.0f, true },  // SEED_ZOMBIE_LADDER
		{ 0.35f, -3.0f, -10.0f, true },  // SEED_ZOMBIE_DIGGER
		{ 0.3f,  1.0f,  -1.0f, true },   // SEED_ZOMBIE_BUNGEE
		{ 0.33f, -7.0f,  -9.0f, true },  // SEED_ZOMBIE_FOOTBALL
		{ 0.35f, -3.0f,  -5.0f, true },  // SEED_ZOMBIE_BALLOON
		{ 0.35f, -3.0f, -10.0f, true },  // SEED_ZOMBIE_SCREEN_DOOR
		{ 0.23f, 12.0f,   3.0f, true },  // SEED_ZOMBONI
		{ 0.35f, -3.0f, -10.0f, true },  // SEED_ZOMBIE_POGO
		{ 0.37f, -18.0f, -38.0f, true }, // SEED_ZOMBIE_DANCER
		{ 0.23f,  4.0f,   3.0f, true },  // SEED_ZOMBIE_GARGANTUAR
		{ 0.4f, -12.0f, -17.0f, true },  // SEED_ZOMBIE_IMP
	};
	static_assert(sizeof(kSeedPacketDrawMetrics) / sizeof(kSeedPacketDrawMetrics[0]) == (int)SeedType::SEED_ZOMBIE_IMP + 1,
		"槽位数须与 SeedType 枚举尾段同步");
	const SeedPacketDrawMetrics& aMetrics = (unsigned)aSeedType < sizeof(kSeedPacketDrawMetrics) / sizeof(kSeedPacketDrawMetrics[0])
		? kSeedPacketDrawMetrics[aSeedType] : kSeedPacketDefault;
	float aScale = aMetrics.mScale;
	bool aDrawSeedInMiddle = aMetrics.mDrawInMiddle;
	float aOffsetX = aMetrics.mOffsetX;
	float aOffsetY = aMetrics.mOffsetY;
	if (((LawnApp*)gSexyAppBase)->mGameMode == GameMode::GAMEMODE_LIMBO_BIG_TIME)
	{
		if (aSeedType == SeedType::SEED_WALLNUT || aSeedType == SeedType::SEED_SUNFLOWER || aSeedType == SeedType::SEED_MARIGOLD)
		{
			aOffsetX = 16.0f;
			aOffsetY = 34.0f;
		}
	}
	if (aSeedType == SeedType::SEED_GIANT_WALLNUT)
	{
		aScale *= 0.75f;
		aOffsetX = 52.0f;
		aOffsetY = 58.0f;
	}
	aOffsetX = g->mScaleX * aOffsetX;
	aOffsetY = g->mScaleY * (aOffsetY + 1.0f);
	if (aDrawSeedInMiddle)
	{
		SeedPacketDrawSeed(g, x, y, theSeedType, theImitaterType, aOffsetX, aOffsetY, aScale);
	}

	if (thePercentDark > 0.0f)
	{
		int aDarknessHeight = FloatRoundToInt(68.0f * thePercentDark) + 2;
		Graphics aPlantG(*g);
		aPlantG.mColor = Color(64, 64, 64, 255);
		aPlantG.mColorizeImages = true;
		aPlantG.ClipRect(x, y, SEED_PACKET_WIDTH, aDarknessHeight);
		TodDrawImageRectScaledF(&aPlantG, Sexy::IMAGE_SEEDS, x, y, Rect(aPacketBackground * 50, 0, 50, 70), aPlantG.mScaleX, aPlantG.mScaleY);
		if (aDrawSeedInMiddle)
		{
			SeedPacketDrawSeed(&aPlantG, x, y, theSeedType, theImitaterType, aOffsetX, aOffsetY, aScale);
		}
	}

	if (theDrawCost)
	{
		// C++23 第一批·条目1/9：成本串按种子类型直取缓存，值变才经 to_chars 重建
		// （原 StrFormat(("%d"))/(("%d+")) 每次调用重建；键=(种子,模仿者,价,加号)，输出逐字节等价）。
		// 注：函数域缓存而非 SeedPacket 成员——本自由函数被种子栏/选卡/图鉴/颁奖共 6 处调用，
		// 且 SeedPacket 布局被存档层锁定（SaveGame static_assert(sizeof==96) + ClearNativeObject memset）。
		struct CostStrCacheEntry
		{
			int         mImitaterKey = -1;
			int         mCostKey = -1;
			bool        mPlusKey = false;
			SexyString  mStr;
		};
		static CostStrCacheEntry aCostCache[(int)SeedType::NUM_SEED_TYPES];

		int aCost;
		bool aPlus;
		if (gLawnApp->mBoard && gLawnApp->mBoard->PlantUsesAcceleratedPricing(aSeedType))
		{
			if (theUseCurrentCost)
			{
				aCost = gLawnApp->mBoard->GetCurrentPlantCost(theSeedType, theImitaterType);
				aPlus = false;
			}
			else
			{
				aCost = Plant::GetCost(theSeedType, theImitaterType);
				aPlus = true;
			}
		}
		else
		{
			aCost = Plant::GetCost(theSeedType, theImitaterType);
			aPlus = false;
		}

		// 注意：SEED_NONE=-1 等越界类型必须绕过缓存（原实现无数组下标，任意类型安全），
		// 越界直接走无缓存格式化，输出与缓存路径逐字节等价。
		const int aCacheIndex = (int)aSeedType;
		const bool aCacheUsable = aCacheIndex >= 0 && aCacheIndex < (int)SeedType::NUM_SEED_TYPES;

		SexyString aCostStrScratch;  // 仅越界类型使用
		const SexyString* aCostStrPtr;
		if (aCacheUsable)
		{
			CostStrCacheEntry& aCacheEntry = aCostCache[aCacheIndex];
			if (aCacheEntry.mImitaterKey != (int)theImitaterType || aCacheEntry.mCostKey != aCost ||
				aCacheEntry.mPlusKey != aPlus || aCacheEntry.mStr.empty())
			{
				aCacheEntry.mImitaterKey = (int)theImitaterType;
				aCacheEntry.mCostKey = aCost;
				aCacheEntry.mPlusKey = aPlus;
				char aCostBuf[16];
				aCacheEntry.mStr.assign(FormatInt(aCostBuf, aCost));
				if (aPlus)
				{
					aCacheEntry.mStr += ('+');
				}
			}
			aCostStrPtr = &aCacheEntry.mStr;
		}
		else
		{
			char aCostBuf[16];
			aCostStrScratch.assign(FormatInt(aCostBuf, aCost));
			if (aPlus)
			{
				aCostStrScratch += ('+');
			}
			aCostStrPtr = &aCostStrScratch;
		}
		const SexyString& aCostStr = *aCostStrPtr;

		_Font* aTextFont = Sexy::FONT_PICO129;
		int aTextOffsetX = 32 - aTextFont->StringWidth(aCostStr);
		int aTextOffsetY = aTextFont->mAscent + 54;
		if (g->mScaleX == 1.0f && g->mScaleY == 1.0f)
		{
			TodDrawString(g, aCostStr, x + aTextOffsetX, y + aTextOffsetY, aTextFont, Color::Black, DS_ALIGN_LEFT);
		}
		else
		{
			SexyMatrix3 aMatrix;
			// Matrix text needs the same destination translation as the packet image.
			TodScaleTransformMatrix(aMatrix,
				aTextOffsetX * g->mScaleX + x + g->mTransX,
				aTextOffsetY * g->mScaleY + y + g->mTransY,
				g->mScaleX, g->mScaleY);
			if (g->mScaleX > 1.8f)
			{
				g->mLinearBlend = false;
			}
			TodDrawStringMatrix(g, aTextFont, aMatrix, aCostStr, Color::Black);
			g->mLinearBlend = true;
		}
	}

	g->mColorizeImages = false;
}

//0x488220
void SeedPacket::Draw(Graphics* g)
{
	float aPercentDark = 0.0f;
	const auto selected = [&](const CursorObject* cursor) {
		return cursor && cursor->mCursorType == CURSOR_TYPE_PLANT_FROM_BANK && cursor->mSeedBankIndex == mIndex;
	};
	const bool held = selected(mBoard->mCursorObject) || (mBoard->mTouchState && mBoard->mTouchState->selection &&
		selected(mBoard->mTouchState->selection->cursor));
	if (!mActive && !held)
	{
		if (mRefreshTime == 0)
		{
			aPercentDark = 1.0f;
		}
		else
		{
			aPercentDark = static_cast<float>(double(mRefreshTime.mNanoseconds -
				(static_cast<std::int64_t>(mBoard->mLevelTimeNs) - mRefreshStartTimeNs)) / mRefreshTime.mNanoseconds);
		}
	}

	if (mSlotMachineEndTimeNs > 0)
	{
		int aOffsetY = FloatRoundToInt(-mHeight * mSlotMachiningPosition);

		Graphics aClipG(*g);
		aClipG.ClipRect(0, 0, mWidth, mHeight);

		DrawSeedPacket(&aClipG, 0.0f, aOffsetY, mPacketType, SeedType::SEED_NONE, 0.0f, 128, false, false);
		DrawSeedPacket(&aClipG, 0.0f, mHeight + aOffsetY, mSlotMachiningNextSeed, SeedType::SEED_NONE, 0.0f, 128, false, false);
	}
	else
	{
		SeedType aUseSeedType = mPacketType;
		if (mPacketType == SeedType::SEED_IMITATER && mImitaterType != SeedType::SEED_NONE)
		{
			aUseSeedType = mImitaterType;
		}

		bool aDrawCost = true;
		if (mBoard->HasConveyorBeltSeedBank() || mApp->IsSlotMachineLevel())
		{
			aDrawCost = false;
		}
		int aGrayness = 255;
		if ((mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED && !mActive) ||
			(mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST && !mActive))
		{
			aGrayness = 64;
		}
		else if (mApp->mGameScene != GameScenes::SCENE_PLAYING)
		{
			aGrayness = mBoard->mSeedBank->mCutSceneDarken;
			aPercentDark = 0.0f;
		}
		else if (mBoard->mTutorialState == TutorialState::TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER && mBoard->mTutorialTimer == -1 && mPacketType == SeedType::SEED_PEASHOOTER)
		{
			aGrayness = GetFlashingColorNs(mBoard->mLevelTimeNs, 750'000'000).mRed;
		}
		else if (mBoard->mTutorialState == TutorialState::TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER && mPacketType == SeedType::SEED_SUNFLOWER)
		{
			aGrayness = GetFlashingColorNs(mBoard->mLevelTimeNs, 750'000'000).mRed;
		}
		else if (mBoard->mTutorialState == TutorialState::TUTORIAL_MORESUN_PICK_UP_SUNFLOWER && mPacketType == SeedType::SEED_SUNFLOWER)
		{
			aGrayness = GetFlashingColorNs(mBoard->mLevelTimeNs, 750'000'000).mRed;
		}
		else if (mBoard->mTutorialState == TutorialState::TUTORIAL_WHACK_A_ZOMBIE_PICK_SEED)
		{
			aGrayness = GetFlashingColorNs(mBoard->mLevelTimeNs, 750'000'000).mRed;
		}
		else if (mApp->mEasyPlantingCheat)
		{
			aPercentDark = 0.0f;
		}
		else if (aPercentDark > 0.0f)
		{
			// Cooldown has one appearance, independent of the current sun balance.
			aGrayness = 128;
		}
		else if ((aDrawCost && !mBoard->CanTakeSunMoney(mBoard->GetCurrentPlantCost(mPacketType, mImitaterType))) || !mBoard->Met(aUseSeedType))
		{
			aGrayness = 128;
		}

		if (mBoard->mLandscapeHud)
            LandscapeHud::DrawPacket(g,0,0,mPacketType,mImitaterType,aPercentDark,aGrayness,aDrawCost,true);
        else
            DrawSeedPacket(g, mOffsetX, 0.0f, mPacketType, mImitaterType, aPercentDark, aGrayness, aDrawCost, true);
	}
}

//0x488500
bool SeedPacket::CanPickUp()
{
	if (mBoard->mPaused || mApp->mGameScene != GameScenes::SCENE_PLAYING || mPacketType == SeedType::SEED_NONE)
	{
		return false;
	}

	SeedType aUseSeedType = mPacketType;
	if (mPacketType == SeedType::SEED_IMITATER && mImitaterType != SeedType::SEED_NONE)
	{
		aUseSeedType = mImitaterType;
	}

	if (mApp->IsSlotMachineLevel())
	{
		return false;
	}

	if (!mApp->mEasyPlantingCheat)
	{
		if (!mActive)
		{
			return false;
		}

		int aCost = mBoard->GetCurrentPlantCost(mPacketType, mImitaterType);
		if (!mBoard->CanTakeSunMoney(aCost) && !mBoard->HasConveyorBeltSeedBank())
		{
			return false;
		}

		if (!mBoard->Met(aUseSeedType))
		{
			return false;
		}
	}

	return true;
}

namespace
{
// SeedPacket::MouseDown 升级种子未达成提示表：下标 = aUseSeedType -
// SEED_GATLINGPEA，行序 = 升级种子连续枚举段 40..47（8 槽全满，段外落
// 分发点 else = 原 default TOD_ASSERT）。
constexpr struct { const std::string* mStr; AdviceType mAdvice; } kSeedNeedsAdvice[
	(int)SeedType::SEED_COBCANNON - (int)SeedType::SEED_GATLINGPEA + 1] = {
	{ &gStr.ADVICE_PLANT_NEEDS_REPEATER,     AdviceType::ADVICE_PLANT_NEEDS_REPEATER },      // 40 SEED_GATLINGPEA
	{ &gStr.ADVICE_PLANT_NEEDS_SUNFLOWER,    AdviceType::ADVICE_PLANT_NEEDS_SUNFLOWER },     // 41 SEED_TWINSUNFLOWER
	{ &gStr.ADVICE_PLANT_NEEDS_FUMESHROOM,   AdviceType::ADVICE_PLANT_NEEDS_FUMESHROOM },    // 42 SEED_GLOOMSHROOM
	{ &gStr.ADVICE_PLANT_NEEDS_LILYPAD,      AdviceType::ADVICE_PLANT_NEEDS_LILYPAD },       // 43 SEED_CATTAIL
	{ &gStr.ADVICE_PLANT_NEEDS_MELONPULT,    AdviceType::ADVICE_PLANT_NEEDS_MELONPULT },     // 44 SEED_WINTERMELON
	{ &gStr.ADVICE_PLANT_NEEDS_MAGNETSHROOM, AdviceType::ADVICE_PLANT_NEEDS_MAGNETSHROOM },  // 45 SEED_GOLD_MAGNET
	{ &gStr.ADVICE_PLANT_NEEDS_SPIKEWEED,    AdviceType::ADVICE_PLANT_NEEDS_SPIKEWEED },     // 46 SEED_SPIKEROCK
	{ &gStr.ADVICE_PLANT_NEEDS_KERNELPULT,   AdviceType::ADVICE_PLANT_NEEDS_KERNELPULT },    // 47 SEED_COBCANNON
};
static_assert(sizeof(kSeedNeedsAdvice) / sizeof(kSeedNeedsAdvice[0]) ==
	(int)SeedType::SEED_COBCANNON - (int)SeedType::SEED_GATLINGPEA + 1,
	"kSeedNeedsAdvice 槽位须与升级种子枚举段同步");

// SeedPacket::MouseDown 教程推进表：表内匿名 lambda = 原 case 体逐句迁移；
// nullptr 槽 = 原链不匹配（无 default 体）。
using TutorialAdvanceFn = void (*)(SeedPacket&);

constexpr TutorialAdvanceFn kTutorialAdvanceCases[(int)TutorialState::TUTORIAL_WHACK_A_ZOMBIE_COMPLETED + 1] = {
	nullptr,  // 0 TUTORIAL_OFF
	[](SeedPacket& p)
	{
		p.mBoard->SetTutorialState(TutorialState::TUTORIAL_LEVEL_1_PLANT_PEASHOOTER);
	},  // 1 TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER
	nullptr,  // 2 TUTORIAL_LEVEL_1_PLANT_PEASHOOTER
	nullptr,  // 3 TUTORIAL_LEVEL_1_REFRESH_PEASHOOTER
	nullptr,  // 4 TUTORIAL_LEVEL_1_COMPLETED
	[](SeedPacket& p)
	{
		if (p.mPacketType == SeedType::SEED_SUNFLOWER)
		{
			p.mBoard->SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_PLANT_SUNFLOWER);
		}
		else
		{
			p.mBoard->SetTutorialState(TutorialState::TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER);
		}
	},  // 5 TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER
	nullptr,  // 6 TUTORIAL_LEVEL_2_PLANT_SUNFLOWER
	nullptr,  // 7 TUTORIAL_LEVEL_2_REFRESH_SUNFLOWER
	nullptr,  // 8 TUTORIAL_LEVEL_2_COMPLETED
	[](SeedPacket& p)
	{
		if (p.mPacketType == SeedType::SEED_SUNFLOWER)
		{
			p.mBoard->SetTutorialState(TutorialState::TUTORIAL_MORESUN_PLANT_SUNFLOWER);
		}
		else
		{
			p.mBoard->SetTutorialState(TutorialState::TUTORIAL_MORESUN_REFRESH_SUNFLOWER);
		}
	},  // 9 TUTORIAL_MORESUN_PICK_UP_SUNFLOWER
	nullptr,  // 10 TUTORIAL_MORESUN_PLANT_SUNFLOWER
	nullptr,  // 11 TUTORIAL_MORESUN_REFRESH_SUNFLOWER
	nullptr,  // 12 TUTORIAL_MORESUN_COMPLETED
	nullptr,  // 13 TUTORIAL_SLOT_MACHINE_PULL
	nullptr,  // 14 TUTORIAL_SLOT_MACHINE_COMPLETED
	nullptr,  // 15 TUTORIAL_SHOVEL_PICKUP
	nullptr,  // 16 TUTORIAL_SHOVEL_DIG
	nullptr,  // 17 TUTORIAL_SHOVEL_KEEP_DIGGING
	nullptr,  // 18 TUTORIAL_SHOVEL_COMPLETED
	nullptr,  // 19 TUTORIAL_ZOMBIQUARIUM_BUY_SNORKEL
	nullptr,  // 20 TUTORIAL_ZOMBIQUARIUM_BOUGHT_SNORKEL
	nullptr,  // 21 TUTORIAL_ZOMBIQUARIUM_CLICK_TROPHY
	nullptr,  // 22 TUTORIAL_ZEN_GARDEN_PICKUP_WATER
	nullptr,  // 23 TUTORIAL_ZEN_GARDEN_WATER_PLANT
	nullptr,  // 24 TUTORIAL_ZEN_GARDEN_KEEP_WATERING
	nullptr,  // 25 TUTORIAL_ZEN_GARDEN_VISIT_STORE
	nullptr,  // 26 TUTORIAL_ZEN_GARDEN_FERTILIZE_PLANTS
	nullptr,  // 27 TUTORIAL_ZEN_GARDEN_COMPLETED
	[](SeedPacket& p)
	{
		p.mBoard->SetTutorialState(TutorialState::TUTORIAL_WHACK_A_ZOMBIE_COMPLETED);
	},  // 28 TUTORIAL_WHACK_A_ZOMBIE_BEFORE_PICK_SEED
	[](SeedPacket& p)
	{
		p.mBoard->SetTutorialState(TutorialState::TUTORIAL_WHACK_A_ZOMBIE_COMPLETED);
	},  // 29 TUTORIAL_WHACK_A_ZOMBIE_PICK_SEED（共体）
	nullptr,  // 30 TUTORIAL_WHACK_A_ZOMBIE_COMPLETED
};
static_assert(sizeof(kTutorialAdvanceCases) / sizeof(kTutorialAdvanceCases[0]) ==
	(int)TutorialState::TUTORIAL_WHACK_A_ZOMBIE_COMPLETED + 1,
	"kTutorialAdvanceCases 槽位数须与 TutorialState 枚举同步");
}  // namespace

//0x488590
// GOTY @Patoke: 0x4931C0
void SeedPacket::MouseDown(int x, int y, int theClickCount)
{
	(void)x;(void)y;(void)theClickCount;
	if (mBoard->mPaused || mApp->mGameScene != GameScenes::SCENE_PLAYING || mPacketType == SeedType::SEED_NONE)
	{
		return;
	}

	if (mApp->IsSlotMachineLevel())
	{
		if (!mBoard->mAdvice->IsBeingDisplayed())
		{
			mBoard->DisplayAdvice(gStr.ADVICE_SLOT_MACHINE_PULL, MessageStyle::MESSAGE_STYLE_HINT_TALL_FAST, AdviceType::ADVICE_NONE);
		}
		mBoard->mChallenge->mSlotMachineRollCount = std::min(mBoard->mChallenge->mSlotMachineRollCount, 2);
		return;
	}

	SeedType aUseSeedType = mPacketType;
	if (mPacketType == SeedType::SEED_IMITATER && mImitaterType != SeedType::SEED_NONE)
	{
		aUseSeedType = mImitaterType;
	}

	if (!mApp->mEasyPlantingCheat)
	{
		if (!mActive)
		{
			mApp->PlaySample(SOUND_BUZZER);
			if (mApp->IsFirstTimeAdventureMode() && mBoard->mLevel == 1 && mBoard->mHelpDisplayed[(int)AdviceType::ADVICE_CLICK_ON_SUN])
			{
				mBoard->DisplayAdvice(gStr.ADVICE_SEED_REFRESH, MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1, AdviceType::ADVICE_SEED_REFRESH);
			}
			return;
		}

		int aCost = mBoard->GetCurrentPlantCost(mPacketType, mImitaterType);
		if (!mBoard->CanTakeSunMoney(aCost) && !mBoard->HasConveyorBeltSeedBank())
		{
			mApp->PlaySample(SOUND_BUZZER);
			mBoard->mOutOfMoneyCounter = 70;
			if (mApp->IsFirstTimeAdventureMode() && mBoard->mLevel == 1 && mBoard->mHelpDisplayed[(int)AdviceType::ADVICE_CLICK_ON_SUN])
			{
				mBoard->DisplayAdvice(gStr.ADVICE_CANT_AFFORD_PLANT, MessageStyle::MESSAGE_STYLE_TUTORIAL_LEVEL1, AdviceType::ADVICE_CANT_AFFORD_PLANT);
			}
			return;
		}

		if (!mBoard->Met(aUseSeedType))
		{
			mApp->PlaySample(SOUND_BUZZER);

			// B 形态 -> 常量表（上方 kSeedNeedsAdvice）：下标平移
			// SEED_GATLINGPEA..SEED_COBCANNON 升级种子段；段外落 else =
			// 原 default TOD_ASSERT。
			const int anUpgradeIndex = (int)aUseSeedType - (int)SeedType::SEED_GATLINGPEA;
			if ((unsigned)anUpgradeIndex < sizeof(kSeedNeedsAdvice) / sizeof(kSeedNeedsAdvice[0]))
			{
				mBoard->DisplayAdvice(*kSeedNeedsAdvice[anUpgradeIndex].mStr,
					MessageStyle::MESSAGE_STYLE_HINT_LONG, kSeedNeedsAdvice[anUpgradeIndex].mAdvice);
			}
			else
			{
				TOD_ASSERT(false);
			}

			return;
		}
	}

	mBoard->ClearAdvice(AdviceType::ADVICE_CANT_AFFORD_PLANT);
	mBoard->ClearAdvice(AdviceType::ADVICE_PLANT_NEEDS_REPEATER);
	mBoard->ClearAdvice(AdviceType::ADVICE_PLANT_NEEDS_MELONPULT);
	mBoard->ClearAdvice(AdviceType::ADVICE_PLANT_NEEDS_SUNFLOWER);
	mBoard->ClearAdvice(AdviceType::ADVICE_PLANT_NEEDS_KERNELPULT);
	mBoard->ClearAdvice(AdviceType::ADVICE_PLANT_NEEDS_SPIKEWEED);
	mBoard->ClearAdvice(AdviceType::ADVICE_PLANT_NEEDS_MAGNETSHROOM);
	mBoard->ClearAdvice(AdviceType::ADVICE_PLANT_NEEDS_FUMESHROOM);
	mBoard->ClearAdvice(AdviceType::ADVICE_PLANT_NEEDS_LILYPAD);

	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST)
	{
		mBoard->mChallenge->BeghouledPacketClicked(this);
	}
	else if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM)
	{
		mBoard->mChallenge->ZombiquariumPacketClicked(this);
	}
	else
	{
		mBoard->mCursorObject->mType = mPacketType;
		mBoard->mCursorObject->mImitaterType = mImitaterType;
		mBoard->mCursorObject->mCursorType = CursorType::CURSOR_TYPE_PLANT_FROM_BANK;
		mBoard->mCursorObject->mSeedBankIndex = mIndex;
		mApp->PlaySample(SOUND_SEEDLIFT);

		// C 形态 -> 匿名 lambda 表（上方 kTutorialAdvanceCases）：行序 =
		// TutorialState 枚举序（0..30，无 NUM 哨兵=末值 WHACK_A_ZOMBIE_COMPLETED
		// 锚定）；nullptr 槽/越界 = 原链不匹配。
		const unsigned aTutorialIndex = static_cast<unsigned>(mBoard->mTutorialState);
		if (aTutorialIndex <= static_cast<unsigned>(TutorialState::TUTORIAL_WHACK_A_ZOMBIE_COMPLETED) &&
			kTutorialAdvanceCases[aTutorialIndex] != nullptr)
		{
			kTutorialAdvanceCases[aTutorialIndex](*this);
		}

		mActive = false;
		mRefreshStartTimeNs = mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : 0;
		mRefreshTime = 0;
		mRefreshing = false;
	}
}

//0x488EC0
void SeedPacket::WasPlanted()
{
	TOD_ASSERT(mPacketType != SeedType::SEED_NONE);

	if (mBoard->HasConveyorBeltSeedBank())
	{
		mBoard->mSeedBank->RemoveSeed(mIndex);
	}
	else if (mApp->IsSlotMachineLevel())
	{
		mActive = false;
		mRefreshStartTimeNs = mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : 0;
		mRefreshTime = 0;
		mRefreshing = false;
	}
	else if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND && mBoard->mChallenge->mChallengeState != ChallengeState::STATECHALLENGE_LAST_STAND_ONSLAUGHT)
	{
		mTimesUsed++;
		mActive = true;
		FlashIfReady();
	}
	else
	{
		mTimesUsed++;
		mRefreshing = true;
		mRefreshStartTimeNs = static_cast<std::int64_t>(mBoard->mLevelTimeNs);
		mRefreshTime = Plant::GetRefreshTime(mPacketType, mImitaterType);
	}
}

//0x488F90
bool SeedPacket::MouseHitTest(int theX, int theY, HitResult* theHitResult)
{
	if (mSlotMachineEndTimeNs > 0 || mPacketType == SeedType::SEED_NONE)
	{
		theHitResult->mObject = nullptr;
		theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_NONE;
		return false;
	}

	if (theX >= mX + mOffsetX && theX < mX + mOffsetX + mWidth && theY >= mY && theY < mY + mHeight)
	{
		theHitResult->mObject = this;
		theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_SEEDPACKET;
		return true;
	}

	theHitResult->mObject = nullptr;
	theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_NONE;
	return false;
}

//0x489000
SeedBank::SeedBank()
{
	mWidth = IMAGE_SEEDBANK->mWidth;
	mHeight = IMAGE_SEEDBANK->mHeight;

	mNumPackets = 0;
	mConveyorBeltCounter = 0;
	mCutSceneDarken = 255;
}

//0x489630
void SeedBank::Draw(Graphics* g)
{
	if (mBoard->mCutScene && mBoard->mCutScene->IsBeforePreloading())
	{
		return;
	}

    if (mBoard->mLandscapeHud) { LandscapeHud::DrawBank(*this,g); return; }
    Graphics* sunGraphics=g;
    Graphics aBankGraphics(*g);
    g = &aBankGraphics;
    g->Translate(mApp->mScreenBounds.mX-mBoard->mX,-mBoard->mY);

	if (mBoard->HasConveyorBeltSeedBank())
	{
		g->DrawImage(IMAGE_CONVEYORBELT_BACKDROP, 83, 0);
		g->DrawImage(IMAGE_CONVEYORBELT, 90, 63, Rect(0, (mConveyorBeltCounter / 4 % 6) * 16, 502, 16));
		g->SetClipRect(90, 0, 501, BOARD_HEIGHT);
	}
	else if (!mApp->IsSlotMachineLevel())
	{
		int aExtraWidth = mBoard->GetSeedBankExtraWidth();
		Rect theSrcRect(IMAGE_SEEDBANK->mWidth - aExtraWidth - 12, 0, aExtraWidth + 12, IMAGE_SEEDBANK->mHeight);
		g->DrawImage(IMAGE_SEEDBANK, 0, 0);
		g->DrawImage(IMAGE_SEEDBANK, IMAGE_SEEDBANK->mWidth - 12, 0, theSrcRect);
	}

	for (int i = 0; i < mNumPackets; i++)
	{
		SeedPacket* aSeedPacket = &mSeedPackets[i];
		const float aParentTransX = g->mTransX, aParentTransY = g->mTransY;
		if (aSeedPacket->mPacketType != SeedType::SEED_NONE && aSeedPacket->BeginDraw(g))
		{
			aSeedPacket->Draw(g);
			aSeedPacket->EndDraw(g, aParentTransX, aParentTransY);
		}
	}

	g->ClearClipRect();
	if (mApp->IsSlotMachineLevel() && mY > -IMAGE_SEEDBANK->mHeight)
	{
		g->DrawImage(IMAGE_SLOTMACHINE_OVERLAY, 189, -2);
	}

    DrawSun(sunGraphics);
}

void SeedBank::DrawSun(Graphics* graphics)
{
    if(!mApp->mLandscapeUIActive&&mBoard->HasConveyorBeltSeedBank())return;
    Graphics g(*graphics);
    int labelX=34,labelY=78;
    if(mApp->mLandscapeUIActive) {
        g.Translate(-mX,-mY);
        const auto bounds=LandscapeHud::SunBounds(*mBoard);
        g.mColorizeImages=false;g.mLinearBlend=false;
        DrawAtlasImage(&g,{LandscapeUIAtlas::Texture,0},
            LandscapeUIAtlas::Images[size_t(LandscapeUIAtlas::Sprite::SUNBANK)],bounds.mX,bounds.mY);
        labelX=bounds.mX+53;labelY=bounds.mY+23;
    } else {
        g.Translate(mApp->mScreenBounds.mX-mBoard->mX,-mBoard->mY);
        if(mApp->IsSlotMachineLevel())g.DrawImage(IMAGE_SUNBANK,0,0);
    }
    // Both layouts share one value-change cache and one shortage flash rule.
    static int lastSun=-1;static SexyString label;
    const int value=std::max(mBoard->mSunMoney,0);
    if(value!=lastSun) {char buffer[16];label.assign(FormatInt(buffer,value));lastSun=value;}
    const auto color=mBoard->mOutOfMoneyCounter>0&&mBoard->mOutOfMoneyCounter%20<10?
        Color(255,0,0):Color::Black;
    TodDrawString(&g,label,labelX,labelY,FONT_CONTINUUMBOLD14,color,DS_ALIGN_CENTER);
}

//0x489970
bool SeedBank::MouseHitTest(int x, int y, HitResult* theHitResult)
{
    if (mBoard->mLandscapeHud) {
        theHitResult->mObject=nullptr;theHitResult->mObjectType=OBJECT_TYPE_NONE;
        if (!LandscapeHud::BankBounds(*mBoard).Contains(x,y)) return false;
        for(int i=0;i<mNumPackets;++i) {
            auto& packet=mSeedPackets[i];
            if(packet.mPacketType!=SEED_NONE && packet.mSlotMachineEndTimeNs==0 &&
               LandscapeHud::PacketBounds(*mBoard,i).Contains(x,y)) {
                theHitResult->mObject=&packet;theHitResult->mObjectType=OBJECT_TYPE_SEEDPACKET;return true;
            }
        }
        return false;
    }
	x -= mApp->mScreenBounds.mX-mBoard->mX;
	if (x - mX <= mWidth - 5 && mNumPackets > 0)
	{
		for (SeedPacket& aSeedPacket : mSeedPackets)
		{
			if (aSeedPacket.MouseHitTest(x - mX, y + mBoard->mY - mY, theHitResult))
			{
				return true;
			}
		}
	}

	theHitResult->mObject = nullptr;
	theHitResult->mObjectType = GameObjectType::OBJECT_TYPE_NONE;
	return false;
}

//0x489A20
bool SeedBank::ContainsPoint(int theX, int theY)
{
    if(mBoard->mLandscapeHud)return LandscapeHud::BankBounds(*mBoard).Contains(theX,theY);
	theX -= mApp->mScreenBounds.mX-mBoard->mX;
    theY += mBoard->mY;
	return theX >= mX && theX < mX + mWidth && theY >= mY && theY < mY + mHeight;
}

//0x489A50
void SeedBank::AddSeed(SeedType theSeedType, bool thePlaceOnLeft)
{
	TOD_ASSERT(mBoard->HasConveyorBeltSeedBank());
	TOD_ASSERT(theSeedType != SeedType::SEED_NONE);

	int aNumSeeds = GetNumSeedsOnConveyorBelt();
	if (aNumSeeds == mNumPackets)
	{
		return;
	}

	SeedPacket* aSeedPacket = &mSeedPackets[aNumSeeds];
	aSeedPacket->mPacketType = theSeedType;
	aSeedPacket->mRefreshStartTimeNs = mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : 0;
	aSeedPacket->mRefreshTime = 0;
	aSeedPacket->mRefreshing = false;
	aSeedPacket->mActive = true;
	aSeedPacket->mOffsetX = 515 - (SEED_PACKET_WIDTH + 1) * aNumSeeds;
	if (thePlaceOnLeft)
	{
		aSeedPacket->mOffsetX = 0;
	}
	if (aNumSeeds > 0)
	{
		SeedPacket* aPrevPacket = &mSeedPackets[aNumSeeds - 1];
		if (aSeedPacket->mOffsetX < aPrevPacket->mOffsetX)
		{
			aSeedPacket->mOffsetX = aPrevPacket->mOffsetX + 40;
		}
	}
}

//0x489AC0
void SeedBank::RemoveSeed(int theIndex)
{
	TOD_ASSERT(mBoard->HasConveyorBeltSeedBank());
	TOD_ASSERT(theIndex >= 0 && theIndex < GetNumSeedsOnConveyorBelt());

	for (int i = theIndex; i < mNumPackets; i++)
	{
		SeedPacket* aSeedPacket = &mSeedPackets[i];
		if (aSeedPacket->mPacketType == SeedType::SEED_NONE)
		{
			break;
		}

		if (i == mNumPackets - 1)
		{
			aSeedPacket->mPacketType = SeedType::SEED_NONE;
			aSeedPacket->mOffsetX = 0;
		}
		else
		{
			SeedPacket* aNextPacket = &mSeedPackets[i + 1];
			aSeedPacket->mPacketType = aNextPacket->mPacketType;
			aSeedPacket->mOffsetX = aNextPacket->mOffsetX + SEED_PACKET_WIDTH + 1;
		}

		aSeedPacket->mRefreshStartTimeNs = mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : 0;
		aSeedPacket->mRefreshTime = 0;
		aSeedPacket->mRefreshing = false;
		aSeedPacket->mActive = true;
	}
    mBoard->ReindexTouchConveyorCards(theIndex);
}

//0x489B20
int SeedBank::GetNumSeedsOnConveyorBelt()
{
	for (int i = 0; i < mNumPackets; i++)
	{
		if (mSeedPackets[i].mPacketType == SeedType::SEED_NONE)
		{
			return i;
		}
	}
	return mNumPackets;
}

int SeedBank::CountOfTypeOnConveyorBelt(SeedType theSeedType)
{
	int aCount = 0;
	for (int i = 0; i < mNumPackets; i++)
	{
		if (mSeedPackets[i].mPacketType == theSeedType)
		{
			aCount++;
		}
	}
	return aCount;
}

//0x489B50
void SeedPacket::SetPacketType(SeedType theSeedType, SeedType theImitaterType)
{
	mPacketType = theSeedType;
	mImitaterType = theImitaterType;
	mRefreshStartTimeNs = mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : 0;
	mRefreshTime = 0;
	mRefreshing = false;
	mActive = true;

	SeedType aUseSeedType = theSeedType;
	if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
	{
		aUseSeedType = theImitaterType;
	}

	if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST || 
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM || mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_LAST_STAND || 
		mApp->IsIZombieLevel() || mApp->IsScaryPotterLevel() || mApp->IsWhackAZombieLevel() || (mApp->IsSurvivalMode() && mBoard->mChallenge->mSurvivalStage > 0))
		return;

	if ((Plant::IsUpgrade(aUseSeedType) && !gLawnApp->IsSurvivalMode()) || Plant::GetRefreshTime(mPacketType, mImitaterType) == 5000)
	{
		mRefreshTime = 3500;
		mRefreshing = true;
		mActive = false;
	}
	else if (Plant::IsUpgrade(aUseSeedType) && gLawnApp->IsSurvivalMode())
	{
		mRefreshTime = 8000;
		mRefreshing = true;
		mActive = false;
	}
	else if (Plant::GetRefreshTime(mPacketType, mImitaterType) == 3000)
	{
		mRefreshTime = 2000;
		mRefreshing = true;
		mActive = false;
	}
}

//0x489C70
void SeedBank::UpdateConveyorBelt()
{
	mConveyorBeltCounter++;
	if (mConveyorBeltCounter % CONVEYOR_SPEED == 0)
	{
		for (int i = 0; i < mNumPackets; i++)
		{
			SeedPacket* aSeedPacket = &mSeedPackets[i];
			if (aSeedPacket->mOffsetX > 0)
			{
				aSeedPacket->mOffsetX = std::max(aSeedPacket->mOffsetX - 1, 0);
			}
		}

		mBoard->UpdateToolTip();
	}
}

//0x489CD0
void SeedBank::UpdateWidth()
{
	mNumPackets = mBoard->GetNumSeedsInBank();
	mWidth = IMAGE_SEEDBANK->mWidth + mBoard->GetSeedBankExtraWidth();
	for (int i = 0; i < mNumPackets; i++)
	{
		mSeedPackets[i].mX = mBoard->GetSeedPacketPositionX(i);
	}
}

//0x489D50
void SeedBank::RefreshAllPackets()
{
	for (int i = 0; i < mNumPackets; i++)
	{
		SeedPacket* aSeedPacket = &mSeedPackets[i];
		if (aSeedPacket->mPacketType == SeedType::SEED_NONE)
		{
			break;
		}

		if (aSeedPacket->mRefreshing)
		{
			aSeedPacket->mRefreshStartTimeNs = mBoard ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : 0;
			aSeedPacket->mRefreshing = false;
			aSeedPacket->mActive = true;
			aSeedPacket->FlashIfReady();
		}
	}
}
