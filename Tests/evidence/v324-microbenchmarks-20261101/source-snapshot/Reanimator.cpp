#include "graphics/Graphics.h"
#include "TodDebug.h"
#include "TodCommon.h"
#include "Definition.h"
#include "TodAllocator.h"
#include "Reanimator.h"
#include <chrono>
#include "ReanimRenderSemantics.h"
#include "ReanimTrackMphfGlobal.h"  // 全局单表 MPHF（唯一 MPHF 层）：(type,id) 联合键 -> trackIndex + 语料成员位图（reanimc compile-v2 生成）
#include "generated/ReanimAttacherMasks.h"  // P4：每类型轨道数 + attacher 位掩码（编译期生成，与本 cpp 独占包含）
#include "generated/GeneratedDefinitions.h"
#include "Attachment.h"
#include "EffectSystem.h"
#include "GameConstants.h"
#include "graphics/Font.h"
#include <bit>
#include <array>
#include <vector>
#include "graphics/MemoryImage.h"
#include "graphics/DDInterface.h"
#include "graphics/RenderInterface.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <utility>
#include <vector>
#include <string>
#include <stdexcept>
#include "paklib/PakInterface.h"

static bool ReanimFullLast(ReanimLoopType loop);
static void ConfigureFramePeriod(Reanimation& animation, std::int64_t requestedPeriod);
static void BindFramePosition(Reanimation& animation, std::int64_t nowNs,
    std::int64_t phaseNs);

static unsigned int gReanimatorDefCount;                     //[0x6A9EE4]
ReanimatorDefinition* gReanimatorDefArray;   //[0x6A9EE8]
static unsigned int gReanimationParamArraySize;              //[0x6A9EEC]

static ReanimationParams* gReanimationParamArray;   //[0x6A9EF0]

std::uint32_t ReanimatorTrackIdAt(ReanimationType theReanimType, int theTrackIndex)
{
	if (gReanimatorDefArray == nullptr || (unsigned)theReanimType >= gReanimatorDefCount)
		return 0;
	const ReanimatorDefinition& aDef = gReanimatorDefArray[(int)theReanimType];
	if (aDef.mTracks.tracks == nullptr || (unsigned)theTrackIndex >= (unsigned)aDef.mTracks.count)
		return 0;
	return static_cast<std::uint32_t>(aDef.mTracks.tracks[theTrackIndex].mTrackId);
}

// Resolve the single root-motion track once at compile time, including misses.
// Uses the generated MPHF, so type order and first matching track stay identical.
extern constexpr std::array<int, (int)ReanimationType::NUM_REANIMS> gReanimGroundTrackIndices = [] {
    std::array<int, (int)ReanimationType::NUM_REANIMS> indices{};
    for (unsigned int type = 0; type < indices.size(); ++type)
        indices[type] = MphfLookupGlobal(type, (unsigned int)ReanimTrackId::_ground);
    return indices;
}();

static int ReanimatorTrackIndexOf(ReanimationType theReanimType, ReanimTrackId theTrackId)
{
	// 单条 memo 已删：实测命中率 5.0%（4870/92094，普通 25s 会话）远低于
	// ~30% 盈亏平衡点（miss 白付比较+回写 ~0.8ns，hit 仅省 ~1.9ns）；实战
	// 查找在几十类型/键间交错，单槽结构接不住。
	if (theReanimType < 0 || theReanimType >= (ReanimationType)gReanimatorDefCount)
	{
		return -1;
	}
	const int aType = (int)theReanimType;
	// 全局单表 MPHF（唯一查找层）：全语料 (type,id) 联合键（(type<<32)|id）
	// 一次查找（ReanimTrackMphfGlobal.h 烘焙；桶/槽均 2 的幂，取模全为掩码零
	// div；函数体与 reanimc 求解器逐运算一致）。键集 = 每类型首见该 id 的轨道
	// 下标。语料已覆盖全部 144 类型（含 Zombie_FlagPole/Disco/Backup/Zombatar），
	// 无兜底第二层：-1 即"该定义确无此轨道"的终答——探测式消费方
	//（TrackExists/AssignRenderGroupToTrack）取 false，直用式消费方经
	// FindTrackIndex 弹错退出，绝不静默回轨道 0。
	const int aIndex = MphfLookupGlobal((std::uint32_t)aType, (std::uint32_t)theTrackId);
	return aIndex;
}


ReanimationParams gLawnReanimationArray[(int)ReanimationType::NUM_REANIMS] = { //0x6A1340
	{ ReanimationType::REANIM_LOADBAR_SPROUT,                       "reanim\\LoadBar_sprout.reanim" },
	{ ReanimationType::REANIM_LOADBAR_ZOMBIEHEAD,                   "reanim\\LoadBar_Zombiehead.reanim" },
	{ ReanimationType::REANIM_SODROLL,                              "reanim\\SodRoll.reanim" },
	{ ReanimationType::REANIM_FINAL_WAVE,                           "reanim\\FinalWave.reanim" },
	{ ReanimationType::REANIM_PEASHOOTER,                           "reanim\\PeaShooterSingle.reanim" },
	{ ReanimationType::REANIM_WALLNUT,                              "reanim\\Wallnut.reanim" },
	{ ReanimationType::REANIM_LILYPAD,                              "reanim\\Lilypad.reanim" },
	{ ReanimationType::REANIM_SUNFLOWER,                            "reanim\\SunFlower.reanim" },
	{ ReanimationType::REANIM_LAWNMOWER,                            "reanim\\LawnMower.reanim" },
	{ ReanimationType::REANIM_READYSETPLANT,                        "reanim\\StartReadySetPlant.reanim" },
	{ ReanimationType::REANIM_CHERRYBOMB,                           "reanim\\CherryBomb.reanim" },
	{ ReanimationType::REANIM_SQUASH,                               "reanim\\Squash.reanim" },
	{ ReanimationType::REANIM_DOOMSHROOM,                           "reanim\\DoomShroom.reanim" },
	{ ReanimationType::REANIM_SNOWPEA,                              "reanim\\SnowPea.reanim" },
	{ ReanimationType::REANIM_REPEATER,                             "reanim\\PeaShooter.reanim" },
	{ ReanimationType::REANIM_SUNSHROOM,                            "reanim\\SunShroom.reanim" },
	{ ReanimationType::REANIM_TALLNUT,                              "reanim\\Tallnut.reanim" },
	{ ReanimationType::REANIM_FUMESHROOM,                           "reanim\\Fumeshroom.reanim" },
	{ ReanimationType::REANIM_PUFFSHROOM,                           "reanim\\Puffshroom.reanim" },
	{ ReanimationType::REANIM_HYPNOSHROOM,                          "reanim\\Hypnoshroom.reanim" },
	{ ReanimationType::REANIM_CHOMPER,                              "reanim\\Chomper.reanim" },
	{ ReanimationType::REANIM_ZOMBIE,                               "reanim\\Zombie.reanim" },
	{ ReanimationType::REANIM_SUN,                                  "reanim\\Sun.reanim" },
	{ ReanimationType::REANIM_POTATOMINE,                           "reanim\\PotatoMine.reanim" },
	{ ReanimationType::REANIM_SPIKEWEED,                            "reanim\\Caltrop.reanim" },
	{ ReanimationType::REANIM_SPIKEROCK,                            "reanim\\SpikeRock.reanim" },
	{ ReanimationType::REANIM_THREEPEATER,                          "reanim\\ThreePeater.reanim" },
	{ ReanimationType::REANIM_MARIGOLD,                             "reanim\\Marigold.reanim" },
	{ ReanimationType::REANIM_ICESHROOM,                            "reanim\\IceShroom.reanim" },
	{ ReanimationType::REANIM_ZOMBIE_FOOTBALL,                      "reanim\\Zombie_football.reanim" },
	{ ReanimationType::REANIM_ZOMBIE_NEWSPAPER,                     "reanim\\Zombie_paper.reanim" },
	{ ReanimationType::REANIM_ZOMBIE_ZAMBONI,                       "reanim\\Zombie_zamboni.reanim" },
	{ ReanimationType::REANIM_SPLASH,                               "reanim\\splash.reanim" },
	{ ReanimationType::REANIM_JALAPENO,                             "reanim\\Jalapeno.reanim" },
	{ ReanimationType::REANIM_JALAPENO_FIRE,                        "reanim\\fire.reanim" },
	{ ReanimationType::REANIM_COIN_SILVER,                          "reanim\\Coin_silver.reanim" },
	{ ReanimationType::REANIM_ZOMBIE_CHARRED,                       "reanim\\Zombie_charred.reanim" },
	{ ReanimationType::REANIM_ZOMBIE_CHARRED_IMP,                   "reanim\\Zombie_charred_imp.reanim" },
	{ ReanimationType::REANIM_ZOMBIE_CHARRED_DIGGER,                "reanim\\Zombie_charred_digger.reanim" },
	{ ReanimationType::REANIM_ZOMBIE_CHARRED_ZAMBONI,               "reanim\\Zombie_charred_zamboni.reanim" },
	{ ReanimationType::REANIM_ZOMBIE_CHARRED_CATAPULT,              "reanim\\Zombie_charred_catapult.reanim" },
	{ ReanimationType::REANIM_ZOMBIE_CHARRED_GARGANTUAR,            "reanim\\Zombie_charred_gargantuar.reanim" },
	{ ReanimationType::REANIM_SCRAREYSHROOM,                        "reanim\\ScaredyShroom.reanim" },
	{ ReanimationType::REANIM_PUMPKIN,                              "reanim\\Pumpkin.reanim" },
	{ ReanimationType::REANIM_PLANTERN,                             "reanim\\Plantern.reanim" },
	{ ReanimationType::REANIM_TORCHWOOD,                            "reanim\\Torchwood.reanim" },
	{ ReanimationType::REANIM_SPLITPEA,                             "reanim\\SplitPea.reanim" },
	{ ReanimationType::REANIM_SEASHROOM,                            "reanim\\SeaShroom.reanim" },
	{ ReanimationType::REANIM_BLOVER,                               "reanim\\Blover.reanim" },
	{ ReanimationType::REANIM_FLOWER_POT,                           "reanim\\Pot.reanim" },
	{ ReanimationType::REANIM_CACTUS,                               "reanim\\Cactus.reanim" },
	{ ReanimationType::REANIM_TANGLEKELP,                           "reanim\\Tanglekelp.reanim" },
	{ ReanimationType::REANIM_STARFRUIT,                            "reanim\\Starfruit.reanim" },
	{ ReanimationType::REANIM_POLEVAULTER,                          "reanim\\Zombie_polevaulter.reanim" },
	{ ReanimationType::REANIM_BALLOON,                              "reanim\\Zombie_balloon.reanim" },
	{ ReanimationType::REANIM_GARGANTUAR,                           "reanim\\Zombie_gargantuar.reanim" },
	{ ReanimationType::REANIM_IMP,                                  "reanim\\Zombie_imp.reanim" },
	{ ReanimationType::REANIM_DIGGER,                               "reanim\\Zombie_digger.reanim" },
	{ ReanimationType::REANIM_DIGGER_DIRT,                          "reanim\\Digger_rising_dirt.reanim" },
	{ ReanimationType::REANIM_ZOMBIE_DOLPHINRIDER,                  "reanim\\Zombie_dolphinrider.reanim" },
	{ ReanimationType::REANIM_POGO,                                 "reanim\\Zombie_pogo.reanim" },
	{ ReanimationType::REANIM_BOBSLED,                              "reanim\\Zombie_bobsled.reanim" },
	{ ReanimationType::REANIM_JACKINTHEBOX,                         "reanim\\Zombie_jackbox.reanim" },
	{ ReanimationType::REANIM_SNORKEL,                              "reanim\\Zombie_snorkle.reanim" },
	{ ReanimationType::REANIM_BUNGEE,                               "reanim\\Zombie_bungi.reanim" },
	{ ReanimationType::REANIM_CATAPULT,                             "reanim\\Zombie_catapult.reanim" },
	{ ReanimationType::REANIM_LADDER,                               "reanim\\Zombie_ladder.reanim" },
	{ ReanimationType::REANIM_PUFF,                                 "reanim\\Puff.reanim" },
	{ ReanimationType::REANIM_SLEEPING,                             "reanim\\Z.reanim" },
	{ ReanimationType::REANIM_GRAVE_BUSTER,                         "reanim\\Gravebuster.reanim" },
	{ ReanimationType::REANIM_ZOMBIES_WON,                          "reanim\\ZombiesWon.reanim" },
	{ ReanimationType::REANIM_MAGNETSHROOM,                         "reanim\\Magnetshroom.reanim" },
	{ ReanimationType::REANIM_BOSS,                                 "reanim\\Zombie_boss.reanim" },
	{ ReanimationType::REANIM_CABBAGEPULT,                          "reanim\\Cabbagepult.reanim" },
	{ ReanimationType::REANIM_KERNELPULT,                           "reanim\\Cornpult.reanim" },
	{ ReanimationType::REANIM_MELONPULT,                            "reanim\\Melonpult.reanim" },
	{ ReanimationType::REANIM_COFFEEBEAN,                           "reanim\\Coffeebean.reanim" },
	{ ReanimationType::REANIM_UMBRELLALEAF,                         "reanim\\Umbrellaleaf.reanim" },
	{ ReanimationType::REANIM_GATLINGPEA,                           "reanim\\GatlingPea.reanim" },
	{ ReanimationType::REANIM_CATTAIL,                              "reanim\\Cattail.reanim" },
	{ ReanimationType::REANIM_GLOOMSHROOM,                          "reanim\\GloomShroom.reanim" },
	{ ReanimationType::REANIM_BOSS_ICEBALL,                         "reanim\\Zombie_boss_iceball.reanim" },
	{ ReanimationType::REANIM_BOSS_FIREBALL,                        "reanim\\Zombie_boss_fireball.reanim" },
	{ ReanimationType::REANIM_COBCANNON,                            "reanim\\CobCannon.reanim" },
	{ ReanimationType::REANIM_GARLIC,                               "reanim\\Garlic.reanim" },
	{ ReanimationType::REANIM_GOLD_MAGNET,                          "reanim\\GoldMagnet.reanim" },
	{ ReanimationType::REANIM_WINTER_MELON,                         "reanim\\WinterMelon.reanim" },
	{ ReanimationType::REANIM_TWIN_SUNFLOWER,                       "reanim\\TwinSunflower.reanim" },
	{ ReanimationType::REANIM_POOL_CLEANER,                         "reanim\\PoolCleaner.reanim" },
	{ ReanimationType::REANIM_ROOF_CLEANER,                         "reanim\\RoofCleaner.reanim" },
	{ ReanimationType::REANIM_FIRE_PEA,                             "reanim\\FirePea.reanim" },
	{ ReanimationType::REANIM_IMITATER,                             "reanim\\Imitater.reanim" },
	{ ReanimationType::REANIM_YETI,                                 "reanim\\Zombie_yeti.reanim" },
	{ ReanimationType::REANIM_BOSS_DRIVER,                          "reanim\\Zombie_Boss_driver.reanim" },
	{ ReanimationType::REANIM_LAWN_MOWERED_ZOMBIE,                  "reanim\\LawnMoweredZombie.reanim" },
	{ ReanimationType::REANIM_CRAZY_DAVE,                           "reanim\\CrazyDave.reanim" },
	{ ReanimationType::REANIM_TEXT_FADE_ON,                         "reanim\\TextFadeOn.reanim" },
	{ ReanimationType::REANIM_HAMMER,                               "reanim\\Hammer.reanim" },
	{ ReanimationType::REANIM_SLOT_MACHINE_HANDLE,                  "reanim\\SlotMachine.reanim" },
	{ ReanimationType::REANIM_CREDITS_FOOTBALL,                     "reanim\\Credits_Football.reanim" },
	{ ReanimationType::REANIM_CREDITS_JACKBOX,                      "reanim\\Credits_Jackbox.reanim" },
	{ ReanimationType::REANIM_SELECTOR_SCREEN,                      "reanim\\SelectorScreen.reanim" },
	{ ReanimationType::REANIM_PORTAL_CIRCLE,                        "reanim\\Portal_Circle.reanim" },
	{ ReanimationType::REANIM_PORTAL_SQUARE,                        "reanim\\Portal_Square.reanim" },
	{ ReanimationType::REANIM_ZENGARDEN_SPROUT,                     "reanim\\ZenGarden_sprout.reanim" },
	{ ReanimationType::REANIM_ZENGARDEN_WATERINGCAN,                "reanim\\ZenGarden_wateringcan.reanim" },
	{ ReanimationType::REANIM_ZENGARDEN_FERTILIZER,                 "reanim\\ZenGarden_fertilizer.reanim" },
	{ ReanimationType::REANIM_ZENGARDEN_BUGSPRAY,                   "reanim\\ZenGarden_bugspray.reanim" },
	{ ReanimationType::REANIM_ZENGARDEN_PHONOGRAPH,                 "reanim\\ZenGarden_phonograph.reanim" },
	{ ReanimationType::REANIM_DIAMOND,                              "reanim\\Diamond.reanim" },
	{ ReanimationType::REANIM_ZOMBIE_HAND,                          "reanim\\Zombie_hand.reanim" },
	{ ReanimationType::REANIM_STINKY,                               "reanim\\Stinky.reanim" },
	{ ReanimationType::REANIM_RAKE,                                 "reanim\\Rake.reanim" },
	{ ReanimationType::REANIM_RAIN_CIRCLE,                          "reanim\\Rain_circle.reanim" },
	{ ReanimationType::REANIM_RAIN_SPLASH,                          "reanim\\Rain_splash.reanim" },
	{ ReanimationType::REANIM_ZOMBIE_SURPRISE,                      "reanim\\Zombie_surprise.reanim" },
	{ ReanimationType::REANIM_COIN_GOLD,                            "reanim\\Coin_gold.reanim" },
	{ ReanimationType::REANIM_TREEOFWISDOM,                         "reanim\\TreeOfWisdom.reanim" },
	{ ReanimationType::REANIM_TREEOFWISDOM_CLOUDS,                  "reanim\\TreeOfWisdomClouds.reanim" },
	{ ReanimationType::REANIM_TREEOFWISDOM_TREEFOOD,                "reanim\\TreeFood.reanim" },
	{ ReanimationType::REANIM_CREDITS_MAIN,                         "reanim\\Credits_Main.reanim" },
	{ ReanimationType::REANIM_CREDITS_MAIN2,                        "reanim\\Credits_Main2.reanim" },
	{ ReanimationType::REANIM_CREDITS_MAIN3,                        "reanim\\Credits_Main3.reanim" },
	{ ReanimationType::REANIM_ZOMBIE_CREDITS_DANCE,                 "reanim\\Zombie_credits_dance.reanim" },
	{ ReanimationType::REANIM_CREDITS_STAGE,                        "reanim\\Credits_stage.reanim" },
	{ ReanimationType::REANIM_CREDITS_BIGBRAIN,                     "reanim\\Credits_BigBrain.reanim" },
	{ ReanimationType::REANIM_CREDITS_FLOWER_PETALS,                "reanim\\Credits_Flower_petals.reanim" },
	{ ReanimationType::REANIM_CREDITS_INFANTRY,                     "reanim\\Credits_Infantry.reanim" },
	{ ReanimationType::REANIM_CREDITS_THROAT,                       "reanim\\Credits_Throat.reanim" },
	{ ReanimationType::REANIM_CREDITS_CRAZYDAVE,                    "reanim\\Credits_CrazyDave.reanim" },
	{ ReanimationType::REANIM_CREDITS_BOSSDANCE,                    "reanim\\Credits_Bossdance.reanim" },
	{ ReanimationType::REANIM_ZOMBIE_CREDITS_SCREEN_DOOR,           "reanim\\Zombie_Credits_Screendoor.reanim" },
	{ ReanimationType::REANIM_ZOMBIE_CREDITS_CONEHEAD,              "reanim\\Zombie_Credits_Conehead.reanim" },
	{ ReanimationType::REANIM_CREDITS_ZOMBIEARMY1,                  "reanim\\Credits_ZombieArmy1.reanim" },
	{ ReanimationType::REANIM_CREDITS_ZOMBIEARMY2,                  "reanim\\Credits_ZombieArmy2.reanim" },
	{ ReanimationType::REANIM_CREDITS_TOMBSTONES,                   "reanim\\Credits_Tombstones.reanim" },
	{ ReanimationType::REANIM_CREDITS_SOLARPOWER,                   "reanim\\Credits_SolarPower.reanim" },
	{ ReanimationType::REANIM_CREDITS_ANYHOUR,                      "reanim\\Credits_Anyhour.reanim" },
	{ ReanimationType::REANIM_CREDITS_WEARETHEUNDEAD,               "reanim\\Credits_WeAreTheUndead.reanim" },
	{ ReanimationType::REANIM_CREDITS_DISCOLIGHTS,                  "reanim\\Credits_DiscoLights.reanim" },
	{ ReanimationType::REANIM_FLAG,                                 "reanim\\Zombie_FlagPole.reanim" },
	{ ReanimationType::REANIM_DANCER,                               "reanim\\Zombie_disco.reanim" }, // @Patoke: GOTY has different reanim name
	{ ReanimationType::REANIM_BACKUP_DANCER,                        "reanim\\Zombie_backup.reanim" }, // @Patoke: GOTY has different reanim name
	{ ReanimationType::REANIM_ZOMBATAR_HEAD,                        "reanim\\zombatar_zombie_head.reanim" }, // @Patoke: GOTY has different reanim name
};

//0x471890
ReanimatorTrackInstance::ReanimatorTrackInstance()
{
	mBlendCounter = 0;
	mBlendTime = 0;
	mShakeOverride = 0.0f;
	mShakeX = 0.0f;
	mShakeY = 0.0f;
	mAttachmentID = AttachmentID::ATTACHMENTID_NULL;
	mRenderGroup = RENDER_GROUP_NORMAL;
	mIgnoreClipRect = false;
	mTruncateDisappearingFrames = true;
	mTrackColor = Color::White;
	mIgnoreColorOverride = false;
	mIgnoreExtraAdditiveColor = false;
}

//0x471920
// ── P4 掩码位操作（2026-10-02）：位=轨道下标 ──
inline void ReanimSetActiveBit(Reanimation& r, int theTrackIndex)
{
	r.mActiveMask[theTrackIndex >> 6] |= 1ull << (theTrackIndex & 63);
}
inline void ReanimClearActiveBit(Reanimation& r, int theTrackIndex)
{
	r.mActiveMask[theTrackIndex >> 6] &= ~(1ull << (theTrackIndex & 63));
}
// 分组占用：0..31 置位；负值（HIDDEN=-1）不占可绘分组。只置不清——
// 空分组多跑一遍无害，漏位才丢绘制。
inline void ReanimOccupyGroupBit(Reanimation& r, int theRenderGroup)
{
	if ((unsigned)theRenderGroup < 32u)
		r.mGroupOccMask |= 1u << theRenderGroup;
}

// P4 编译期锚：生成表与掩码存储容量一致（改版超限在此编译错，抬升
// Reanimator.h 的 kReanimActiveMaskCapacityBits 即可）
static_assert(kMaxReanimTracks <= kReanimActiveMaskCapacityBits, "reanim tracks exceed activity mask capacity - raise kReanimActiveMaskCapacityBits");
static_assert(kReanimAttacherMaskWords <= kReanimActiveMaskWords, "generated attacher mask wider than storage");

Reanimation::Reanimation()
{
	mStartTimeNs = 0;
	mLastUpdateTimeNs = 0;
	mPendingTiming = false;
	mCurrentFrame = 0;
	mFramePeriodNs = 1;
	mRootFrameScale = 1.0;
	mPeriodNs = 1;
	mAnimRate = 12.0f;
	mPlaybackGeneration = 0;
	mDefinition = nullptr;
	mLoopType = ReanimLoopType::REANIM_PLAY_ONCE;
	mLastFrameNs = 0;
	mCycleCrossings = 0;
    mRootDisplacement = 0;
	mHasLastFrame = false;
	mDead = false;
	mFrameStart = 0;
	mFrameCount = 0;
	mFrameBasePose = -1;
	for (uint64_t& aWord : mActiveMask)
		aWord = 0;
	mGroupOccMask = 0;
	mOverlayMatrix.LoadIdentity();
	mColorOverride = Color::White;
	mExtraAdditiveColor = Color::White;
	mEnableExtraAdditiveDraw = false;
	mExtraOverlayColor = Color::White;
	mEnableExtraOverlayDraw = false;
	mLoopCount = 0;
	mIsAttachment = false;
	mRenderOrder = 0;
	mReanimationHolder = nullptr;
	mTrackInstances = nullptr;
	mFilterEffect = FilterEffect::FILTER_EFFECT_NONE;
	mReanimationType = ReanimationType::REANIM_NONE;
	mVariant = 0;
}

//0x471A20
Reanimation::~Reanimation()
{
	ReanimationDie();
	ReanimationDelete();
}

void Reanimation::ReanimationDelete()
{
	TOD_ASSERT(mDead);
	if (mTrackInstances != nullptr)
	{
		int aItemSize = mDefinition->mTracks.count * sizeof(ReanimatorTrackInstance);
		FindGlobalAllocator(aItemSize)->Free(mTrackInstances, aItemSize);  // 由 TodAllocator 回收动画轨道的内存区域
		mTrackInstances = nullptr;
	}
}

//0x471A60
void Reanimation::ReanimationInitializeType(float theX, float theY, ReanimationType theReanimType, std::int64_t nowNs)
{
	TOD_ASSERT(theReanimType >= 0 && theReanimType < gReanimatorDefCount);
	ReanimatorEnsureDefinitionLoaded(theReanimType, false);
	mReanimationType = theReanimType;
	ReanimationInitialize(theX, theY, &gReanimatorDefArray[(int)theReanimType], nowNs);
}

void ReanimationPreload(ReanimationType theReanimationType)
{
	TOD_ASSERT(theReanimationType >= 0 && theReanimationType < gReanimatorDefCount);

	ReanimatorEnsureDefinitionLoaded(theReanimationType,true);
}

void Reanimation::ReanimationInitialize(float theX, float theY, ReanimatorDefinition* theDefinition, std::int64_t nowNs)
{
	TOD_ASSERT(mTrackInstances == nullptr);
	mDead = false;
	mOverlayMatrix.m02 = theX;
	mOverlayMatrix.m12 = theY;
	mDefinition = theDefinition;
	mVariant = 0;
	mAnimRate = theDefinition->mFPS;
	mLastFrameNs = 0;
	mHasLastFrame = false;

	if (theDefinition->mTracks.count != 0)
	{
		mFrameCount = mDefinition->mTracks.tracks[0].mTransforms.count;
		int aItemSize = theDefinition->mTracks.count * sizeof(ReanimatorTrackInstance);
		mTrackInstances = (ReanimatorTrackInstance*)FindGlobalAllocator(aItemSize)->Calloc(aItemSize);  // 申请动画轨道实例数组所需的内存
		for (int aTrackIndex = 0; aTrackIndex < mDefinition->mTracks.count; aTrackIndex++)  // 遍历初始化数组中每个轨道实例
		{
			ReanimatorTrackInstance* aTrack = &mTrackInstances[aTrackIndex];
			if (aTrack != nullptr)
				new (aTrack)ReanimatorTrackInstance();
		}
	}
	else
		mFrameCount = 0;
	ConfigureFramePeriod(*this, ReanimClock::PeriodFromRate(mFrameCount, mAnimRate == 0 ? 12.0f : mAnimRate));
	mStartTimeNs = mLastUpdateTimeNs = nowNs;
	mCurrentFrame = 0;
	mCycleCrossings = 0;
    mRootDisplacement = 0;
	mPendingTiming = false;

	// P4 派生掩码：新实例全零活动；轨道 ctor 全 RENDER_GROUP_NORMAL → 组 0 占位。
	// 超 256 轨（掩码字宽外）：全 1 高估——Update 自愈首帧收敛为精确位，
	// 行为等价原全扫（mod 超大 reanim 不破，仅退回全扫性能）
	if (theDefinition->mTracks.count > 256)
	{
		for (uint64_t& aWord : mActiveMask)
			aWord = ~0ull;
	}
	else
	{
		for (uint64_t& aWord : mActiveMask)
			aWord = 0;
	}
	mGroupOccMask = (theDefinition->mTracks.count != 0) ? (1u << 0) : 0u;
}

static bool ReanimFullLast(ReanimLoopType loop)
{
	return loop == ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME ||
		loop == ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME ||
		loop == ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME_AND_HOLD;
}

static int FrameIntervals(const Reanimation& animation)
{
    return std::max(1, ReanimFullLast(animation.mLoopType) ? animation.mFrameCount : animation.mFrameCount - 1);
}

static void ConfigureFramePeriod(Reanimation& animation, std::int64_t requestedPeriod)
{
    const int intervals = FrameIntervals(animation);
    animation.mFramePeriodNs = requestedPeriod / intervals;
    if (animation.mFramePeriodNs <= 0) throw std::overflow_error("keyframe duration below one nanosecond");
    animation.mPeriodNs = ReanimClock::Product(animation.mFramePeriodNs, intervals);
    animation.mRootFrameScale = ReanimFullLast(animation.mLoopType) ? 1.0 :
        double(animation.mFrameCount) / intervals;
}

static void BindFramePosition(Reanimation& animation, std::int64_t nowNs,
    std::int64_t phaseNs)
{
    phaseNs = std::clamp<std::int64_t>(phaseNs, 0, animation.mPeriodNs);
    const auto q = animation.mFramePeriodNs;
    const auto frame = phaseNs / q;
    const auto remainder = phaseNs % q;
    const bool reverse = animation.mAnimRate < 0;
    animation.mCurrentFrame = int(frame + (reverse && remainder != 0));
    const auto age = reverse && remainder != 0 ? q - remainder : remainder;
    animation.mStartTimeNs = ReanimClock::Subtract(nowNs, age);
    animation.mLastUpdateTimeNs = nowNs;
}

static std::int64_t FramePhase(const Reanimation& animation, const ReanimFramePosition& position)
{
    const auto start = std::int64_t(position.frame) * animation.mFramePeriodNs;
    return animation.mAnimRate < 0 ? start - position.ageNs : start + position.ageNs;
}

static void FrameCoordinates(const Reanimation& animation, const ReanimFramePosition& position,
    int& before, double& fraction)
{
    before = position.frame;
    fraction = double(position.ageNs) / double(animation.mFramePeriodNs);
    if (animation.mAnimRate < 0 && position.ageNs != 0)
    {
        --before;
        fraction = 1.0 - fraction;
    }
    before = std::clamp(before, 0, FrameIntervals(animation));
}

void Reanimation::Seek(std::int64_t numerator, std::int64_t denominator)
{
    ++mPlaybackGeneration;
    const auto phase = std::clamp<std::int64_t>(ReanimClock::SignedMulDiv(numerator, mPeriodNs, denominator), 0, mPeriodNs);
    const auto now = mLastUpdateTimeNs;
    BindFramePosition(*this, now, phase);
    mPendingTiming = false;
    mCycleCrossings = 0;
    mRootDisplacement = 0;
    mLastFrameNs = phase;
    mHasLastFrame = false;
}

void Reanimation::SeekRatio(float ratio)
{
    if (!std::isfinite(ratio)) throw std::out_of_range("nonfinite animation seek phase");
    ++mPlaybackGeneration;
    const double position = double(std::clamp(ratio, 0.0f, 1.0f)) * mPeriodNs;
    const auto phase = position >= double(mPeriodNs) ? mPeriodNs : std::int64_t(position);
    const auto now = mLastUpdateTimeNs;
    BindFramePosition(*this, now, phase);
    mPendingTiming = false;
    mCycleCrossings = 0;
    mRootDisplacement = 0;
    mLastFrameNs = phase;
    mHasLastFrame = false;
}

void Reanimation::SynchronizePhase(const Reanimation& source)
{
    ++mPlaybackGeneration;
    const auto position = source.SamplePlayback();
    const auto phase = source.mFrameCount > 0 ?
        ReanimClock::SignedMulDiv(position.phaseNs, mPeriodNs, source.mPeriodNs) : 0;
    const auto now = mLastUpdateTimeNs;
    BindFramePosition(*this, now, phase);
    mPendingTiming = true;
    mLastFrameNs = source.mFrameCount > 0 ?
        ReanimClock::SignedMulDiv(source.mLastFrameNs, mPeriodNs, source.mPeriodNs) : 0;
    mCycleCrossings = source.mCycleCrossings;
    mRootDisplacement = 0;
    mHasLastFrame = source.mHasLastFrame;
}

#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline, cold))
#endif
static ReanimFramePosition SampleFrameSigned(const Reanimation& animation, std::int64_t age)
{
    const auto q = animation.mFramePeriodNs;
    const int intervals = FrameIntervals(animation);
    const bool reverse = animation.mAnimRate < 0;
    const bool looping = animation.mLoopType == REANIM_LOOP || animation.mLoopType == REANIM_LOOP_FULL_LAST_FRAME;
    auto phase = std::int64_t(animation.mCurrentFrame) * q;
    std::int64_t cycles = 0;
    ReanimClock::Advance(phase, cycles, animation.mPeriodNs, reverse ? -1 : 1, age);
    if (!looping && cycles != 0) return {0, cycles > 0 ? intervals : 0, 0};
    const auto frame = phase / q, remainder = phase % q;
    return {cycles, int(frame + (reverse && remainder != 0)),
        reverse && remainder != 0 ? q - remainder : remainder};
}

static ReanimFramePosition SampleFrameByDivision(const Reanimation& animation,
    std::int64_t nowNs, std::int64_t offsetNs)
{
    if (animation.mFrameCount <= 0) return {0, animation.mCurrentFrame, 0};
    const auto requested = ReanimClock::Add(nowNs, offsetNs);
    const auto age = ReanimClock::Subtract(animation.mAnimRate == 0 ? animation.mLastUpdateTimeNs : requested, animation.mStartTimeNs);
    const auto q = animation.mFramePeriodNs;
    const int intervals = FrameIntervals(animation);
    const bool reverse = animation.mAnimRate < 0;
    const bool looping = animation.mLoopType == REANIM_LOOP || animation.mLoopType == REANIM_LOOP_FULL_LAST_FRAME;
    if (!reverse && age >= 0) {
        // Quotient/remainder advance any number of keyframes in one operation.
        // Keep a seek exactly at the endpoint until time actually advances.
        if (age == 0) return {0, animation.mCurrentFrame, 0};
        const auto elapsed = static_cast<unsigned long long>(age);
        const auto period = static_cast<unsigned long long>(q);
        const auto steps = elapsed / period;
        const auto remainder = elapsed % period;
        const auto frame = static_cast<unsigned long long>(animation.mCurrentFrame) + steps;
        if (frame < static_cast<unsigned long long>(intervals))
            return {0, int(frame), std::int64_t(remainder)};
        if (!looping) return {0, intervals, 0};
        const auto cycles = frame / static_cast<unsigned long long>(intervals);
        if (cycles > INT64_MAX) throw std::overflow_error("animation cycle overflow");
        return {std::int64_t(cycles), int(frame % static_cast<unsigned long long>(intervals)),
            std::int64_t(remainder)};
    }
    return SampleFrameSigned(animation, age);
}

ReanimFramePosition Reanimation::SampleFrameAt(std::int64_t nowNs, std::int64_t offsetNs) const
{
    // ConsumeTimeAt and all timing mutations publish a normalized frame cache.
    // Ordinary consumers of that same timestamp share it without another divide.
    if (offsetNs == 0 && mFrameCount > 0 &&
        (nowNs == mLastUpdateTimeNs || mAnimRate == 0))
        return {0, mCurrentFrame, ReanimClock::Subtract(mLastUpdateTimeNs, mStartTimeNs)};
    return SampleFrameByDivision(*this, nowNs, offsetNs);
}

ReanimPlaybackPosition Reanimation::SamplePlayback(std::int64_t offsetNs) const
{
    const auto nowNs = mLastUpdateTimeNs;
    return SamplePlaybackAt(nowNs, offsetNs);
}

ReanimPlaybackPosition Reanimation::SamplePlaybackAt(std::int64_t nowNs, std::int64_t offsetNs) const
{
    const auto position = SampleFrameAt(nowNs, offsetNs);
    return {position.cycles, FramePhase(*this, position)};
}

void Reanimation::RebaseTime(std::int64_t nowNs)
{
    mStartTimeNs = ReanimClock::Add(mStartTimeNs, ReanimClock::Subtract(nowNs, mLastUpdateTimeNs));
    mLastUpdateTimeNs = nowNs;
    mPendingTiming = false;
}

void ReanimationHolder::BindAnimationTime(Reanimation& animation, ReanimTimeDomain domain, std::int64_t nowNs)
{
    auto& current = mTimeDomains[mReanimations.DataArrayIndexOf(&animation)];
    if (domain == current) return;
    current = domain;
    animation.RebaseTime(nowNs);
    if (!gEffectSystem || !animation.mDefinition || !animation.mTrackInstances) return;
    for (int i=0;i<animation.mDefinition->mTracks.count;++i)
        if (auto* attachment=gEffectSystem->mAttachmentHolder->mAttachments.DataArrayTryToGet(
            static_cast<unsigned int>(animation.mTrackInstances[i].mAttachmentID)))
            for (int j=0;j<attachment->mNumEffects;++j)
            {
                if (attachment->mEffectArray[j].mEffectType == EffectType::EFFECT_REANIM)
                {
                    if (auto* child=mReanimations.DataArrayTryToGet(attachment->mEffectArray[j].mEffectID))
                        BindAnimationTime(*child,domain,nowNs);
                }
                else if (attachment->mEffectArray[j].mEffectType == EffectType::EFFECT_PARTICLE)
                    if (auto* particle=gEffectSystem->mParticleHolder->mParticleSystems.DataArrayTryToGet(
                        attachment->mEffectArray[j].mEffectID)) particle->Update(0,nowNs);
            }
    animation.mHasLastFrame = false;
}

void ReanimationHolder::AdoptAnimationTime(Reanimation& animation, const Reanimation& parent, std::int64_t nowNs)
{
    BindAnimationTime(animation,mTimeDomains[mReanimations.DataArrayIndexOf(&parent)],nowNs);
}

void Reanimation::ApplyRate(float rate)
{
    if (!std::isfinite(rate)) throw std::overflow_error("nonfinite animation rate");
    if (rate == mAnimRate) return;
    const auto now = mLastUpdateTimeNs;
    const auto position = SampleFrameAt(now);
    const auto oldFramePeriod = mFramePeriodNs;
    const bool reverse = mAnimRate < 0;
    ConfigureFramePeriod(*this, rate == 0 || mFrameCount <= 0 ? mPeriodNs :
        ReanimClock::PeriodFromRate(mFrameCount, rate));
    // Convert only the age inside one keyframe. Avoid multiplying whole-cycle
    // phase by another whole-cycle period merely to cancel the frame count.
    std::uint64_t remainder;
    const auto converted = ReanimClock::MulDiv(position.ageNs, mFramePeriodNs,
        oldFramePeriod, &remainder);
    const auto frameStart = std::int64_t(position.frame) * mFramePeriodNs;
    const auto phase = reverse ? frameStart - std::int64_t(converted + (remainder != 0)) :
        frameStart + std::int64_t(converted);
    const auto lastFrame = mLastFrameNs / oldFramePeriod;
    const auto lastAge = mLastFrameNs % oldFramePeriod;
    mLastFrameNs = lastFrame * mFramePeriodNs +
        ReanimClock::SignedMulDiv(lastAge, mFramePeriodNs, oldFramePeriod);

    mAnimRate = rate;
    BindFramePosition(*this, now, phase);
}

void Reanimation::ChangeLoopType(ReanimLoopType loop)
{
    if (loop == mLoopType) return;
    const auto position = SamplePlayback();
    const auto oldPeriod = mPeriodNs;
    mLoopType = loop;
    ConfigureFramePeriod(*this, mFrameCount > 0 && mAnimRate != 0 ?
        ReanimClock::PeriodFromRate(mFrameCount, mAnimRate) : oldPeriod);
    const auto phase = ReanimClock::SignedMulDiv(position.phaseNs, mPeriodNs, oldPeriod);
    const auto now = mLastUpdateTimeNs;
    BindFramePosition(*this, now, phase);
    mPendingTiming = false;
    mCycleCrossings = 0;
    mRootDisplacement = 0;
    mLastFrameNs = phase;
    mHasLastFrame = false;
    mLoopCount = 0;
    ++mPlaybackGeneration;
}

void Reanimation::Advance(std::int64_t deltaNs)
{
    ConsumeTimeAt(ReanimClock::Add(mLastUpdateTimeNs, deltaNs));
    mPendingTiming = false;
}

static double GroundCycleDisplacement(const Reanimation& animation)
{
    if (animation.mFrameCount <= 1 || !animation.mDefinition ||
        (unsigned int)animation.mReanimationType >= gReanimGroundTrackIndices.size()) return 0;
    const int index = gReanimGroundTrackIndices[(unsigned int)animation.mReanimationType];
    if (index < 0) return 0;
    const auto* keys = animation.mDefinition->mTracks.tracks[index].mTransforms.mTransforms;
    if (!keys) throw std::logic_error("ground displacement requires CPU keyframes");
    return (double(keys[animation.mFrameStart + animation.mFrameCount - 1].mTransX) -
        keys[animation.mFrameStart].mTransX) * animation.mRootFrameScale;
}

void Reanimation::ConsumeTimeAt(std::int64_t nowNs)
{
    if (mFrameCount <= 0 || mDead) return;
    const auto previousAge = ReanimClock::Subtract(mLastUpdateTimeNs, mStartTimeNs);
    const auto previousFrame = std::int64_t(mCurrentFrame) * mFramePeriodNs;
    mLastFrameNs = mAnimRate < 0 ? previousFrame - previousAge : previousFrame + previousAge;
    mHasLastFrame = true;
    const auto frame = SampleFrameByDivision(*this, nowNs, 0);
    const auto phase = FramePhase(*this, frame);
    const auto crossed = frame.cycles;
    mCycleCrossings = crossed;
    if (frame.cycles != 0)
        mRootDisplacement += double(frame.cycles) * GroundCycleDisplacement(*this);
    const bool looping = mLoopType == REANIM_LOOP || mLoopType == REANIM_LOOP_FULL_LAST_FRAME;
    // A held endpoint has no advancing age. Its frame index no longer changes,
    // but its birth must follow now or the cached phase grows beyond the action.
    const bool heldEndpoint = !looping &&
        ((mAnimRate > 0 && frame.frame == FrameIntervals(*this)) ||
         (mAnimRate < 0 && frame.frame == 0));
    if (frame.frame != mCurrentFrame || frame.cycles != 0 || mAnimRate == 0 || heldEndpoint)
    {
        mCurrentFrame = frame.frame;
        mStartTimeNs = ReanimClock::Subtract(nowNs, frame.ageNs);
    }
    mLastUpdateTimeNs = nowNs;
    if (looping && crossed != 0)
    {
        if (crossed < -INT_MAX || crossed > INT_MAX) throw std::overflow_error("animation loop counter overflow");
        const auto loops = ReanimClock::Add(mLoopCount, crossed < 0 ? -crossed : crossed);
        if (loops > INT_MAX) throw std::overflow_error("animation loop counter overflow");
        mLoopCount = int(loops);
    }
    else if (!looping && ((mAnimRate > 0 && phase == mPeriodNs) || (mAnimRate < 0 && phase == 0)))
    {
        mLoopCount = 1;
        mDead = mLoopType == REANIM_PLAY_ONCE || mLoopType == REANIM_PLAY_ONCE_FULL_LAST_FRAME;
    }
}

double Reanimation::RootMotion(ReanimTrackId trackId, float scale, std::int64_t offsetNs) const
{
    if (trackId != ReanimTrackId::_ground)
        throw std::invalid_argument("RootMotion only supports the _ground track");
    if (mFrameCount <= 1) return 0;
    const int groundIndex =
        (unsigned int)mReanimationType < gReanimGroundTrackIndices.size() ?
        gReanimGroundTrackIndices[(unsigned int)mReanimationType] : -1;
    if (groundIndex < 0) throw std::logic_error("RootMotion requires a _ground track");
    const auto* keys = mDefinition->mTracks.tracks[groundIndex].mTransforms.mTransforms;
    if (!keys) throw std::logic_error("RootMotion requires CPU keyframes");
    const auto now = mLastUpdateTimeNs;
    const auto position = SampleFrameAt(now, offsetNs);
    int frame; double fraction;
    FrameCoordinates(*this, position, frame, fraction);
    const double first = keys[mFrameStart].mTransX;
    const double before = keys[mFrameStart + std::min(frame, mFrameCount - 1)].mTransX;
    const double after = keys[mFrameStart + std::min(frame + 1, mFrameCount - 1)].mTransX;
    const double cycle = double(keys[mFrameStart + mFrameCount - 1].mTransX) - first;
    const double local = (before - first + (after - before) * fraction) * mRootFrameScale;
    const double pending = double(position.cycles) * (cycle * mRootFrameScale);
    return (mRootDisplacement + pending + local) * scale;
}

double Reanimation::RootMotionClamped(ReanimTrackId trackId, float scale,
    float minimumVelocityPixelsPerSecond, std::int64_t offsetNs) const
{
    if (trackId != ReanimTrackId::_ground)
        throw std::invalid_argument("RootMotionClamped only supports the _ground track");
    if (mFrameCount <= 1) return 0;
    const int groundIndex = (unsigned int)mReanimationType < gReanimGroundTrackIndices.size() ?
        gReanimGroundTrackIndices[(unsigned int)mReanimationType] : -1;
    if (groundIndex < 0) throw std::logic_error("RootMotionClamped requires a _ground track");
    const auto* keys = mDefinition->mTracks.tracks[groundIndex].mTransforms.mTransforms;
    if (!keys) throw std::logic_error("RootMotionClamped requires CPU keyframes");
    const auto now = mLastUpdateTimeNs;
    const auto position = SampleFrameAt(now, offsetNs);
    int frame; double fraction;
    FrameCoordinates(*this, position, frame, fraction);
    const int intervals = FrameIntervals(*this);
    const double minimum = double(minimumVelocityPixelsPerSecond) * double(mFramePeriodNs) / 1e9;
    double cyclePixels = 0, prefixPixels = 0, selectedPixels = 0;
    for (int i = 0; i < intervals; ++i)
    {
        const double delta = double(keys[mFrameStart + std::min(i + 1, mFrameCount - 1)].mTransX) - keys[mFrameStart + i].mTransX;
        const double displacement = std::max(delta * mRootFrameScale * scale, minimum);
        cyclePixels += displacement;
        if (i < frame) prefixPixels += displacement;
        else if (i == frame) selectedPixels = displacement;
    }
    // Only the chocolate snail uses the per-interval minimum. Its crawl has a
    // nonzero authored ground displacement; scale the physical completed root
    // base into this curve rather than retaining a lifetime cycle counter.
    const double normalizedCycle = (double(keys[mFrameStart + mFrameCount - 1].mTransX) -
        keys[mFrameStart].mTransX) * mRootFrameScale;
    if (normalizedCycle == 0)
    {
        if (cyclePixels != 0)
            throw std::logic_error("clamped root requires a nonzero authored cycle displacement");
        return prefixPixels + selectedPixels * fraction;
    }
    const double completed = (mRootDisplacement / normalizedCycle) * cyclePixels;
    return completed + double(position.cycles) * cyclePixels + prefixPixels + selectedPixels * fraction;
}

// Half-open intervals: forward [previous,current), reverse (current,previous].
// Return signed multiplicity; callers can dispatch each occurrence after a large step.
std::int64_t Reanimation::TimedEventCount(std::int64_t numerator, std::int64_t denominator) const
{
	if (!mHasLastFrame || mFrameCount <= 0) return 0;
	const auto nowNs = mLastUpdateTimeNs;
	const auto position = SamplePlaybackAt(nowNs);
	if (numerator < 0 || denominator <= 0 || numerator > denominator)
		throw std::out_of_range("animation event phase");
	const auto threshold = ReanimClock::CeilMulDiv(numerator, mPeriodNs, denominator);
	return ReanimClock::EventCount(mLastFrameNs, 0, position.phaseNs,
        ReanimClock::Add(mCycleCrossings, position.cycles), mPeriodNs, threshold);
}

//0x471BC0
// GOTY @Patoke: 0x4761C0
void Reanimation::PrepareTimeAt(std::int64_t nowNs)
{
    if (nowNs != mLastUpdateTimeNs)
        ConsumeTimeAt(nowNs);
    mPendingTiming = true;
}

void Reanimation::UpdateAt(std::int64_t nowNs)
{
	if (mFrameCount == 0 || mDead)
		return;

	if (!mPendingTiming || nowNs != mLastUpdateTimeNs)
		ConsumeTimeAt(nowNs);
	mPendingTiming = false;

	// P4 掩码轨道环：活动位（blend/shake/attach）与 def 级 attacher 位求或
	// 驱动；零即整实体跳过（bench_reanim_trackwalk：112B×N 步长全扫的
	// 11%~22%）。按位索引访问，体与原全量环逐字同序；访问发现三值全零则
	// 自愈清本位（attacher 常量位在生成表，不受影响；陈旧
	// 置位仅浪费一次空访，方向安全）。
	{
		TOD_ASSERT((unsigned)mReanimationType < (unsigned)kReanimAttacherTypeCount);
		const std::uint64_t* aAttacher = kReanimAttacherMasks[(int)mReanimationType];
		uint64_t aWords[kReanimActiveMaskWords];
		for (int w = 0; w < kReanimActiveMaskWords; w++)
			aWords[w] = mActiveMask[w] | (w < kReanimAttacherMaskWords ? aAttacher[w] : 0);
		for (int aWordIdx = 0; aWordIdx < 4; aWordIdx++)
		while (aWords[aWordIdx] != 0)
		{
			const int aTrackIndex = aWordIdx * 64 + (int)std::countr_zero(aWords[aWordIdx]);
			aWords[aWordIdx] &= aWords[aWordIdx] - 1;
			ReanimatorTrackInstance* aTrack = &mTrackInstances[aTrackIndex];
			bool aAnyActive = false;
			if (aTrack->mBlendCounter > 0)
			{
				aTrack->mBlendCounter--;  // 更新轨道的混合倒计时
				aAnyActive = true;
			}

			if (aTrack->mShakeOverride != 0.0f)  // 更新轨道震动
			{
				aTrack->mShakeX = RandRangeFloat(-aTrack->mShakeOverride, aTrack->mShakeOverride);
				aTrack->mShakeY = RandRangeFloat(-aTrack->mShakeOverride, aTrack->mShakeOverride);
				aAnyActive = true;
			}

			if (ReanimTrackRanges::attacher__.Contains(static_cast<ReanimTrackId>(ReanimatorTrackIdAt(mReanimationType, aTrackIndex))))  // IsAttacher：编译期 id 区间，两次整数比较
				UpdateAttacherTrack(aTrackIndex);

			if (aTrack->mAttachmentID != AttachmentID::ATTACHMENTID_NULL)
			{
				SexyTransform2D aOverlayMatrix;
				GetAttachmentOverlayMatrix(aTrackIndex, aOverlayMatrix);
				AttachmentUpdateAndSetMatrixAt(aTrack->mAttachmentID, aOverlayMatrix, nowNs);
				aAnyActive = true;
			}

			if (!aAnyActive)
				ReanimClearActiveBit(*this, aTrackIndex);  // 自愈：陈旧位收敛
		}
	}
}

// P4 掩码联动：Attachment.cpp 的 CreateEffectAttachment 漏斗调用——引用
// 多直指某 reanim 轨道实例的 mAttachmentID 字段（Zombie.cpp 头盔/旗帜/
// 盾牌等 17 处直挂），按"池=真值"反查引用归属并置活动位；局部变量引用
//（smoke 夹具）无归属则空过。清位方向由 Update 自愈承担。
void ReanimMarkAttachmentIdRefOwner(AttachmentID& theAttachmentID)
{
	if (gEffectSystem == nullptr || gEffectSystem->mReanimationHolder == nullptr)
		return;
	Reanimation* aReanim = nullptr;
	auto& aPool = gEffectSystem->mReanimationHolder->mReanimations;
	while (aPool.IterateNext(aReanim))
	{
		if (aReanim->mTrackInstances == nullptr || aReanim->mDefinition == nullptr)
			continue;
		const int aCount = aReanim->mDefinition->mTracks.count;
		for (int i = 0; i < aCount; i++)
			if (&aReanim->mTrackInstances[i].mAttachmentID == &theAttachmentID)
			{
				ReanimSetActiveBit(*aReanim, i);
				return;
			}
	}
}

// P4：按轨道字段现值精确重建两掩码（存档读回 / 夹具直写字段后调用；
// 派生态不入盘）。分组位只收 0..31，HIDDEN(-1) 天然不占。
void Reanimation::RebuildTrackActivityMasks()
{
	for (uint64_t& aWord : mActiveMask)
		aWord = 0;
	mGroupOccMask = 0;
	if (mTrackInstances == nullptr)
		return;
	const int aCount = mDefinition->mTracks.count;
	for (int i = 0; i < aCount; i++)
	{
		const ReanimatorTrackInstance& aTrack = mTrackInstances[i];
		if (aTrack.mBlendCounter > 0 || aTrack.mShakeOverride != 0.0f ||
			aTrack.mAttachmentID != AttachmentID::ATTACHMENTID_NULL)
			ReanimSetActiveBit(*this, i);
		ReanimOccupyGroupBit(*this, aTrack.mRenderGroup);
	}
}

//0x471E50
static void BlendTransform(ReanimatorTransform* theResult, const ReanimatorTransform& theTransform1, const ReanimatorTransform& theTransform2, float theBlendFactor)
{
	theResult->mTransX = FloatLerp(theTransform1.mTransX, theTransform2.mTransX, theBlendFactor);
	theResult->mTransY = FloatLerp(theTransform1.mTransY, theTransform2.mTransY, theBlendFactor);
	theResult->mScaleX = FloatLerp(theTransform1.mScaleX, theTransform2.mScaleX, theBlendFactor);
	theResult->mScaleY = FloatLerp(theTransform1.mScaleY, theTransform2.mScaleY, theBlendFactor);
	theResult->mAlpha = FloatLerp(theTransform1.mAlpha, theTransform2.mAlpha, theBlendFactor);

	float aSkewX2 = theTransform2.mSkewX;
	float aSkewY2 = theTransform2.mSkewY;
	// 推测这里原意是为了确保从两个变换之间的倾斜角度不超过 π（WP 版），
	// 原版（以及内测版）实际为，倾斜超过 π 时 theTransform2 变换无效
	while (aSkewX2 > theTransform1.mSkewX + 180.0f)
		aSkewX2 = theTransform1.mSkewX;  // （aSkewX2 -= 360.0f）
	while (aSkewX2 < theTransform1.mSkewX - 180.0f)
		aSkewX2 = theTransform1.mSkewX;  // （aSkewX2 += 360.0f）
	while (aSkewY2 > theTransform1.mSkewY + 180.0f)
		aSkewY2 = theTransform1.mSkewY;  // （aSkewY2 -= 360.0f）
	while (aSkewY2 < theTransform1.mSkewY - 180.0f)
		aSkewY2 = theTransform1.mSkewY;  // （aSkewY2 += 360.0f）

	theResult->mSkewX = FloatLerp(theTransform1.mSkewX, aSkewX2, theBlendFactor);
	theResult->mSkewY = FloatLerp(theTransform1.mSkewY, aSkewY2, theBlendFactor);
	theResult->mFrame = theTransform1.mFrame;
	theResult->mFont = theTransform1.mFont;
	theResult->mText = theTransform1.mText;
	theResult->mImageSlot = theTransform1.mImageSlot;
}

//0x471F90
// GOTY @Patoke: 0x476580
void Reanimation::GetCurrentTransform(int theTrackIndex, ReanimatorTransform* theTransformCurrent, const ReanimatorFrameTime* theFrameTime)
{
	ReanimatorFrameTime aFrameTime;
	if (theFrameTime == nullptr)
	{
		GetFrameTime(&aFrameTime);
		theFrameTime = &aFrameTime;
	}
	GetTransformAtTime(theTrackIndex, theTransformCurrent, theFrameTime);  // 结合两帧之间的自然补间取得基础变换
	
	ReanimatorTrackInstance* aTrack = &mTrackInstances[theTrackIndex];
	if (FloatRoundToInt(theTransformCurrent->mFrame) >= 0 && aTrack->mBlendCounter > 0)  // 若当前不为空白帧且轨道处于变换混合过程中
	{
		float aBlendFactor = aTrack->mBlendCounter.LegacyTicks() / (float)aTrack->mBlendTime;
		BlendTransform(theTransformCurrent, *theTransformCurrent, aTrack->mBlendTransform, aBlendFactor);  // 结合覆写变换计算混合后的实际变换
	}
}

//0x472020
void Reanimation::GetTransformAtTime(int theTrackIndex, ReanimatorTransform* theTransform, const ReanimatorFrameTime* theFrameTime)
{
	TOD_ASSERT(theTrackIndex >= 0 && theTrackIndex < mDefinition->mTracks.count);
	ReanimatorTrack* aTrack = &mDefinition->mTracks.tracks[theTrackIndex];
	if (aTrack->mTransforms.mTransforms == nullptr)
		throw std::logic_error("GetTransformAtTime requires CPU keyframes for track index " +
			std::to_string(theTrackIndex) + " (id " + std::to_string(static_cast<std::uint32_t>(aTrack->mTrackId)) + ")");
	TOD_ASSERT(aTrack->mTransforms.count == mDefinition->mTracks.tracks[0].mTransforms.count);
	const ReanimatorSourceTransform& aTransBefore = aTrack->mTransforms.mTransforms[theFrameTime->mAnimFrameBeforeInt];
	const ReanimatorSourceTransform& aTransAfter = aTrack->mTransforms.mTransforms[theFrameTime->mAnimFrameAfterInt];

	theTransform->mTransX = FloatLerp(aTransBefore.mTransX, aTransAfter.mTransX, theFrameTime->mFraction);
	theTransform->mTransY = FloatLerp(aTransBefore.mTransY, aTransAfter.mTransY, theFrameTime->mFraction);
	theTransform->mSkewX = FloatLerp(aTransBefore.mSkewX, aTransAfter.mSkewX, theFrameTime->mFraction);
	theTransform->mSkewY = FloatLerp(aTransBefore.mSkewY, aTransAfter.mSkewY, theFrameTime->mFraction);
	theTransform->mScaleX = FloatLerp(aTransBefore.mScaleX, aTransAfter.mScaleX, theFrameTime->mFraction);
	theTransform->mScaleY = FloatLerp(aTransBefore.mScaleY, aTransAfter.mScaleY, theFrameTime->mFraction);
	theTransform->mAlpha = FloatLerp(aTransBefore.mAlpha, aTransAfter.mAlpha, theFrameTime->mFraction);
	theTransform->mImageSlot = aTransBefore.mImageSlot;
	theTransform->mFont = aTransBefore.mFont;
	theTransform->mText = aTransBefore.mText;

	if (aTransBefore.mFrame != -1.0f && aTransAfter.mFrame == -1.0f && theFrameTime->mFraction > 0.0f && mTrackInstances[theTrackIndex].mTruncateDisappearingFrames)
		theTransform->mFrame = -1.0f;  // 当从一个非空白帧过渡至空白帧时，若轨道设置了截断消失帧，则删去过渡的过程
	else
		theTransform->mFrame = aTransBefore.mFrame;
}

//0x4720F0
void Reanimation::MatrixFromTransform(const ReanimatorTransform& theTransform, SexyMatrix3& theMatrix)
{
    // Evaluate each skew axis once; sin/cos share their angle reduction.
    // The resulting transform is reused for all four image corners.
	float aSkewX = -DEG_TO_RAD(theTransform.mSkewX);
	float aSkewY = -DEG_TO_RAD(theTransform.mSkewY);

    float sinX,cosX,sinY,cosY;
    FastSinCos(aSkewX,sinX,cosX);
    FastSinCos(aSkewY,sinY,cosY);
	theMatrix.m00 = cosX * theTransform.mScaleX;
	theMatrix.m10 = -sinX * theTransform.mScaleX;
	theMatrix.m20 = 0.0f;
	theMatrix.m01 = sinY * theTransform.mScaleY;
	theMatrix.m11 = cosY * theTransform.mScaleY;
	theMatrix.m21 = 0.0f;
	theMatrix.m02 = theTransform.mTransX;
	theMatrix.m12 = theTransform.mTransY;
	theMatrix.m22 = 1.0f;
}

//0x4723B0
// GOTY @Patoke: 0x4769B0
bool Reanimation::DrawTrack(Graphics* g, int theTrackIndex, int theRenderGroup,
	const ReanimatorFrameTime* theFrameTime )
{
	(void)theRenderGroup;
	const auto& trackData = mDefinition->mTracks.tracks[theTrackIndex].mTransforms;
	if (!trackData.mTransforms)
		throw std::logic_error("CPU reanimation track has no transform data");
	ReanimatorTrackInstance* aTrackInstance = &mTrackInstances[theTrackIndex];  // 目标轨道的指针
	ReanimatorFrameTime aFrameTime;
	if (theFrameTime == nullptr)
	{
		GetFrameTime(&aFrameTime);
		theFrameTime = &aFrameTime;
	}
	const auto& aBefore = trackData.mTransforms[theFrameTime->mAnimFrameBeforeInt];
	const auto& aAfter = trackData.mTransforms[theFrameTime->mAnimFrameAfterInt];
	// Frame/image/text metadata comes from the preceding keyframe even during
	// blending. Reject invisible/control tracks before interpolating transforms.
	if (aBefore.mFrame < 0 ||
		(aBefore.mFrame != -1 && aAfter.mFrame == -1 &&
			theFrameTime->mFraction > 0.0f && aTrackInstance->mTruncateDisappearingFrames))
		return false;
	const auto aSlot = aBefore.mImageSlot;
	const bool aHasImage = aSlot != 0;
	if (!aHasImage && (aBefore.mFont == nullptr || aBefore.mText == nullptr) &&
		!ReanimRenderSemantics::IsFullscreenTrack(
			static_cast<ReanimTrackId>(ReanimatorTrackIdAt(mReanimationType, theTrackIndex))))
		return false;
	ReanimatorTransform aTransform;
	GetCurrentTransform(theTrackIndex, &aTransform, theFrameTime);  // 取得当前动画变换

	Color aColor = aTrackInstance->mTrackColor;
	if (!aTrackInstance->mIgnoreColorOverride)  // 除非轨道无视动画的覆写颜色
	{
		aColor = ColorsMultiply(aColor, mColorOverride);  // 将轨道颜色与动画的覆写颜色进行正片叠底混合
	}
	if (g->mColorizeImages)  // 若 Graphics 着色
	{
		aColor = ColorsMultiply(aColor, g->mColor);  // 将颜色再与 Graphics 的颜色进行正片叠底混合
	}
	int aImageAlpha = ClampInt(FloatRoundToInt(aTransform.mAlpha * aColor.mAlpha), 0, 255);
	if (aImageAlpha <= 0)  // 当图像完全透明时，返回
	{
		return false;
	}
	aColor.mAlpha = aImageAlpha;
	const TextureRegion* aImageRect = nullptr;
	if (aHasImage)
	{
		aImageRect = &mDefinition->mImageRects[std::size_t(mVariant) * mDefinition->mImageSlotCount + aSlot];
		// Blank appearances keep attachments visible, but fully transparent
		// tracks above still report hidden. Neither case needs a matrix.
		if (aImageRect->mWidth == 0 || aImageRect->mHeight == 0)
			return true;
	}

	Color aExtraAdditiveColor;
	if (mEnableExtraAdditiveDraw)  // 如果动画启用额外叠加颜色（高亮）
	{
		aExtraAdditiveColor = mExtraAdditiveColor;
		aExtraAdditiveColor.mAlpha = ColorComponentMultiply(mExtraAdditiveColor.mAlpha, aImageAlpha);
	}
	Color aExtraOverlayColor;
	if (mEnableExtraOverlayDraw)
	{
		aExtraOverlayColor = mExtraOverlayColor;
		aExtraOverlayColor.mAlpha = ColorComponentMultiply(mExtraOverlayColor.mAlpha, aImageAlpha);
	}

	Rect aClipRect = g->mClipRect;
	if (aTrackInstance->mIgnoreClipRect)  // 如果轨道无视裁剪矩形
	{
		aClipRect = Rect(0, 0, BOARD_WIDTH, BOARD_HEIGHT);  // 裁剪矩形重置为屏幕矩形
	}

	const auto& aGeometry = mDefinition->mImageRects[aSlot];
	int aCelWidth = 0, aCelHeight = 0;
	SexyMatrix3 aMatrix;
	if (aHasImage)
	{
		aCelWidth = aGeometry.mWidth;
		aCelHeight = aGeometry.mHeight;
		aMatrix.LoadIdentity();
		SexyMatrix3Translation(aMatrix, aCelWidth * 0.5f, aCelHeight * 0.5f);  // 将矩阵变换的坐标设定在贴图的中心位置
	}
	else if (aTransform.mFont != nullptr && aTransform.mText != nullptr)  // 如果存在字体且文本不为空
	{
		aMatrix.LoadIdentity();
		int aWidth = aTransform.mFont->StringWidth(aTransform.mText);
		SexyMatrix3Translation(aMatrix, -aWidth * 0.5f, aTransform.mFont->mAscent);
	}
	else
	{
		// Fullscreen fills destination space. Its authored rotation, scale and
		// position never participate, so skip their interpolation-to-matrix
		// conversion and trigonometry entirely.
		Color aOldColor = g->mColor;
		g->mColor = aColor;
		g->FillRect(-g->mTransX, -g->mTransY,
			g->mDestImage->mWidth, g->mDestImage->mHeight);
		g->mColor = aOldColor;
		return true;
	}

	SexyMatrix3 aTransformMatrix;
	MatrixFromTransform(aTransform, aTransformMatrix);
	SexyMatrix3Multiply(aMatrix, aTransformMatrix, aMatrix);  // 以动画变换矩阵作用 aMatrix
	SexyMatrix3Multiply(aMatrix, mOverlayMatrix, aMatrix);  // 以动画覆写矩阵作用 aMatrix
	SexyMatrix3Translation(aMatrix, aTrackInstance->mShakeX + g->mTransX - 0.5f, aTrackInstance->mShakeY + g->mTransY - 0.5f);  // 轨道震动及 g 的影响

	if (aHasImage)
	{
		const TextureRegion& aSrcRect = *aImageRect;
		auto* const aTexture = mDefinition->mTextures[mVariant];
		if (!aTexture)
			throw std::logic_error("reanimation appearance atlas is not uploaded");
		TextureSource source{aTexture,0};
		static constexpr std::uint32_t filterFlags[] = {
			0, std::uint32_t(GpuImageColorFilter::WashedOut),
			std::uint32_t(GpuImageColorFilter::LessWashedOut),
			std::uint32_t(GpuImageColorFilter::White)
		};
		if (mFilterEffect != FilterEffect::FILTER_EFFECT_NONE)
		{
			source.mSamplingFlags = (source.mSamplingFlags & ~0x30000000u) |
				filterFlags[static_cast<int>(mFilterEffect) + 1];
		}
		TodBltMatrix(g, source, aMatrix, aClipRect, aColor, g->mDrawMode, aSrcRect);
		if (mEnableExtraAdditiveDraw)
		{
			TodBltMatrix(g, source, aMatrix, aClipRect, aExtraAdditiveColor, Graphics::DRAWMODE_ADDITIVE, aSrcRect);
		}
		if (mEnableExtraOverlayDraw)
		{
			source.mSamplingFlags = (source.mSamplingFlags & ~0x30000000u) |
				std::uint32_t(GpuImageColorFilter::White);
			TodBltMatrix(g, source, aMatrix, aClipRect, aExtraOverlayColor, Graphics::DRAWMODE_NORMAL, aSrcRect);
		}
	}
	else if (aTransform.mFont != nullptr && aTransform.mText != nullptr)  // 如果不存在图像但存在文本
	{
		TodDrawStringMatrix(g, aTransform.mFont, aMatrix, aTransform.mText, aColor);
		if (mEnableExtraAdditiveDraw)
		{
			int aOldMode = g->mDrawMode;  // 备份绘制模式
			g->mDrawMode = Graphics::DRAWMODE_ADDITIVE;
			TodDrawStringMatrix(g, aTransform.mFont, aMatrix, aTransform.mText, aExtraAdditiveColor);
			g->mDrawMode = aOldMode;  // 还原绘制模式
		}
	}
	return true;
}

//0x472C00
void Reanimation::GetTrackMatrix(int theTrackIndex, SexyTransform2D& theMatrix)
{
	ReanimatorTrackInstance* aTrackInstance = &mTrackInstances[theTrackIndex];
	ReanimatorTransform aTransform;
	GetCurrentTransform(theTrackIndex, &aTransform);
	int aImageFrame = FloatRoundToInt(aTransform.mFrame);
	const auto* aRegion = &mDefinition->mImageRects[aTransform.mImageSlot];

	theMatrix.LoadIdentity();
	if (aRegion->mWidth != 0 && aImageFrame >= 0)
	{
		int aCelWidth = aRegion->mWidth;
		int aCelHeight = aRegion->mHeight;
		SexyMatrix3Translation(theMatrix, aCelWidth * 0.5f, aCelHeight * 0.5f);  // 将矩阵变换的坐标设定在贴图的中心位置
	}
	else if (aTransform.mFont != nullptr && aTransform.mText != nullptr)
		SexyMatrix3Translation(theMatrix, 0.0f, aTransform.mFont->mAscent);

	SexyTransform2D aTransformMatrix;
	MatrixFromTransform(aTransform, aTransformMatrix);
	SexyMatrix3Multiply(theMatrix, aTransformMatrix, theMatrix);  // 以动画变换矩阵作用 theMatrix
	SexyMatrix3Multiply(theMatrix, mOverlayMatrix, theMatrix);  // 以动画覆写矩阵作用 theMatrix
	SexyMatrix3Translation(theMatrix, aTrackInstance->mShakeX - 0.5f, aTrackInstance->mShakeY - 0.5f);  // 轨道震动的影响
}

//0x472D90
void Reanimation::GetFrameTime(ReanimatorFrameTime* theFrameTime)
{
	TOD_ASSERT(mFrameStart + mFrameCount <= mDefinition->mTracks.tracks[0].mTransforms.count);
    const auto now = mLastUpdateTimeNs;
    const auto position = SampleFrameAt(now);
    int frame; double fraction;
    FrameCoordinates(*this, position, frame, fraction);
    theFrameTime->mFraction = float(fraction);
	theFrameTime->mAnimFrameBeforeInt = mFrameStart + static_cast<int>(frame);
	if (theFrameTime->mAnimFrameBeforeInt >= mFrameStart + mFrameCount - 1)  // 如果当前处于结束的一帧
	{
		theFrameTime->mAnimFrameBeforeInt = mFrameStart + mFrameCount - 1;
		theFrameTime->mAnimFrameAfterInt = theFrameTime->mAnimFrameBeforeInt;  // 将前、后的整数帧均赋值为最后一帧
	}
	else
		theFrameTime->mAnimFrameAfterInt = theFrameTime->mAnimFrameBeforeInt + 1;  // 后一整数帧等于前一整数帧的后一帧
	TOD_ASSERT(theFrameTime->mAnimFrameBeforeInt >= 0 && theFrameTime->mAnimFrameAfterInt < mDefinition->mTracks.tracks[0].mTransforms.count);
}

//0x472E40

void Reanimation::DrawRenderGroup(Graphics* g, int theRenderGroup)
{
	if (mDead)
		return;

	ReanimatorFrameTime aFrameTime;
	bool aHasFrameTime = false;
	for (int aTrackIndex = 0; aTrackIndex < mDefinition->mTracks.count; aTrackIndex++)
	{
		ReanimatorTrackInstance* aTrackInstance = &mTrackInstances[aTrackIndex];
		if (aTrackInstance->mRenderGroup == theRenderGroup)
		{
			// Every matching track in this draw uses the same animation clock.
			// Keep this lazy: an empty/nonmatching group never indexed frame data
			// in 1073 either. Attachments calculate their own independent time.
			if (!aHasFrameTime)
			{
				GetFrameTime(&aFrameTime);
				aHasFrameTime = true;
			}
			bool aTrackDrawn = DrawTrack(g, aTrackIndex, theRenderGroup, &aFrameTime);
			if (aTrackInstance->mAttachmentID != AttachmentID::ATTACHMENTID_NULL)
			{
				AttachmentDraw(aTrackInstance->mAttachmentID, g, !aTrackDrawn);
			}
		}
	}
}

void Reanimation::Draw(Graphics* g)
{
	// GOTY 1073 FUN_00477490 draws every supported render group in order.
	// The older reconstructed path drew only group zero, which made attached
	// Zombatar heads lose every selected component: their tracks intentionally
	// occupy groups 2, 4, 6, 8, 10 and 12 to preserve layer ordering.
	// P4：按分组占用掩码遍历（原 0..17 全扫；空分组整遍跳过——GOTY 18 组
	// 仅 Zombatar 部件用 2..12，常态仅组 0 占用，bench 2.1×）
	uint32_t aGroups = mGroupOccMask & ((1u << 18) - 1u);
	while (aGroups != 0)
	{
		const unsigned aRenderGroup = (unsigned)std::countr_zero(aGroups);
		aGroups &= aGroups - 1;
		DrawRenderGroup(g, (int)aRenderGroup);
	}
}

//0x472F30
// GOTY @Patoke: 0x477640
int Reanimation::FindTrackIndex(ReanimTrackId theTrackId)
{
	const int aTrackIndex = ReanimatorTrackIndexOf(mReanimationType, theTrackId);
	if (aTrackIndex < 0)
	{
		// 1073 原版此处 TodTrace 后返回 0（静默回轨道 0），是查错轨道这类
		// bug 的掩盖器；现改为硬错退出。正确性由 bench 全域穷举对拍背书
		//（全部已装载定义的合法 (type,id) 查询零 miss）。需要"查不到则
		// 无操作"语义的调用点应使用 TrackExists 预判。
		TodErrorMessageBox(StrFormat(
				"FindTrackIndex: track id=%u not in reanim type %d ('%s')\n"
				"Caller bug — no silent fallback to track 0 anymore.",
				(unsigned)theTrackId, (int)mReanimationType,
			(int)mReanimationType >= 0 && (int)mReanimationType < (int)gReanimatorDefCount
				? gReanimationParamArray[(int)mReanimationType].mReanimFileName
				: "<def freed>").c_str(), "Error");
		exit(0);
	}
	return aTrackIndex;
}

// GOTY @Patoke: 0x464B18
ReanimatorTrackInstance* Reanimation::GetTrackInstanceByName(ReanimTrackId theTrackId)
{
	return &mTrackInstances[FindTrackIndex(theTrackId)];
}

ReanimTrackId Reanimation::GetTrackId(int theTrackIndex) const
{
	if (mDefinition == nullptr || mDefinition->mTracks.tracks == nullptr ||
		(unsigned)theTrackIndex >= (unsigned)mDefinition->mTracks.count)
		return ReanimTrackId::None;
	return mDefinition->mTracks.tracks[theTrackIndex].mTrackId;
}

//0x472F80
void Reanimation::AttachToAnotherReanimation(Reanimation* theAttachReanim, ReanimTrackId theTrackId)
{
	if (theAttachReanim->mDefinition->mTracks.count <= 0)
		return;

	if (theAttachReanim->mFrameBasePose == -1)
		theAttachReanim->mFrameBasePose = theAttachReanim->mFrameStart;  // 将当前动作的起始帧作为变换基准帧
	const int aTrackIndex = theAttachReanim->FindTrackIndex(theTrackId);
	AttachReanim(theAttachReanim->mTrackInstances[aTrackIndex].mAttachmentID, this, 0.0f, 0.0f);
	mReanimationHolder->AdoptAnimationTime(*this,*theAttachReanim,theAttachReanim->mLastUpdateTimeNs);
	ReanimSetActiveBit(*theAttachReanim, aTrackIndex);  // P4：挂附件置位
}

void Reanimation::SetBasePoseFromAnim(ReanimTrackId theTrackId)
{
	int aFrameStart, aFrameCount;
	GetFramesForLayer(theTrackId, aFrameStart, aFrameCount);
	mFrameBasePose = aFrameStart;  // 将当前轨道动画的起始帧作为变换基准帧
}

//0x472FD0
void Reanimation::GetTrackBasePoseMatrix(int theTrackIndex, SexyTransform2D& theBasePosMatrix)
{
	if (mFrameBasePose == NO_BASE_POSE)
	{
		theBasePosMatrix.LoadIdentity();
		return;
	}

	int aBasePos = mFrameBasePose == -1 ? mFrameStart : mFrameBasePose;
	ReanimatorFrameTime aStartTime = { 0.0f, aBasePos, aBasePos + 1 };
	ReanimatorTransform aTransformStart;
	GetTransformAtTime(theTrackIndex, &aTransformStart, &aStartTime);
	MatrixFromTransform(aTransformStart, theBasePosMatrix);
}

//0x473070
AttachEffect* Reanimation::AttachParticleToTrack(ReanimTrackId theTrackId, TodParticleSystem* theParticleSystem, float thePosX, float thePosY)
{
	int aTrackIndex = FindTrackIndex(theTrackId);
	ReanimatorTrackInstance* aTrackInstance = &mTrackInstances[aTrackIndex];
	SexyTransform2D aBasePoseMatrix;
	GetTrackBasePoseMatrix(aTrackIndex, aBasePoseMatrix);  // 取得轨道基础形态的变换矩阵
	SexyVector2 aPosition = aBasePoseMatrix * SexyVector2(thePosX, thePosY);  // 以基础形态的矩阵变换位置向量
	AttachEffect* aEffect = AttachParticle(aTrackInstance->mAttachmentID, theParticleSystem, aPosition.x, aPosition.y);
	ReanimSetActiveBit(*this, aTrackIndex);  // P4：挂附件置位（空挂失败由自愈收敛）
	return aEffect;
}

//0x473110
// GOTY @Patoke: 0x477810
void Reanimation::GetAttachmentOverlayMatrix(int theTrackIndex, SexyTransform2D& theOverlayMatrix)
{
	ReanimatorTransform aTransform;
	GetCurrentTransform(theTrackIndex, &aTransform);  // 取得含混合、不含覆写的自然变换
	SexyTransform2D aTransformMatrix;
	MatrixFromTransform(aTransform, aTransformMatrix);
	SexyMatrix3Multiply(aTransformMatrix, mOverlayMatrix, aTransformMatrix);  // 以动画覆写矩阵作用于动画变换矩阵

	SexyTransform2D aBasePoseMatrix;
	GetTrackBasePoseMatrix(theTrackIndex, aBasePoseMatrix);  // 取得轨道基础形态的变换矩阵
	SexyTransform2D aBasePoseMatrixInv;
	SexyMatrix3Inverse(aBasePoseMatrix, aBasePoseMatrixInv);  // 取得基础形态矩阵的逆
	theOverlayMatrix = aTransformMatrix * aBasePoseMatrixInv;
}

//0x4731D0
void Reanimation::GetFramesForLayer(ReanimTrackId theTrackId, int& theFrameStart, int& theFrameCount)
{
	// 原版对 0 轨道定义先回 (0,0)：现语料全部 144 定义均非空（smoke:
	// defs=144 tracks=2641），空定义上的层查询按调用点 bug 硬错（FindTrackIndex）。
	GetFramesForTrackIndex(FindTrackIndex(theTrackId), theFrameStart, theFrameCount);
}

void Reanimation::GetFramesForTrackIndex(int theTrackIndex, int& theFrameStart, int& theFrameCount)
{
	TOD_ASSERT(theTrackIndex >= 0 && theTrackIndex < mDefinition->mTracks.count);
	ReanimatorTrack* aTrack = &mDefinition->mTracks.tracks[theTrackIndex];
	if (aTrack->mTransforms.mTransforms == nullptr)
		throw std::logic_error("GetFramesForTrackIndex requires CPU keyframes for track index " +
			std::to_string(theTrackIndex) + " (id " + std::to_string(static_cast<std::uint32_t>(aTrack->mTrackId)) + ")");
	theFrameStart = 0;
	theFrameCount = 1;
	for (int i = 0; i < aTrack->mTransforms.count; i++)
		if (aTrack->mTransforms.mTransforms[i].mFrame >= 0.0f)
		{
			theFrameStart = i;  // 取轨道上的首个非空白帧作为起始帧
			break;
		}
	for (int j = theFrameStart; j < aTrack->mTransforms.count; j++)
		if (aTrack->mTransforms.mTransforms[j].mFrame >= 0.0f)
			theFrameCount = j - theFrameStart + 1;  // 取从起始帧至轨道最后一个非空白帧之间为帧数量
}

//0x473280
void Reanimation::SetFramesForLayer(ReanimTrackId theTrackId)
{
	// 原 0 轨道定义传 0 的分支已删（"count==0 ? 0 : FindTrackIndex"）：
	// 空定义上的层查询是调用点 bug，miss 由 FindTrackIndex 硬错。
	SetFramesForLayerIndex(FindTrackIndex(theTrackId));
}

// index 直达版：时间复位逻辑自 SetFramesForLayer 原样抽出，id 版内部共用。
void Reanimation::SetFramesForLayerIndex(int theTrackIndex)
{
	++mPlaybackGeneration;
	mHasLastFrame = false;
	mCycleCrossings = 0;
    mRootDisplacement = 0;
	mStartTimeNs = mLastUpdateTimeNs;
	mPendingTiming = false;
	if (mDefinition->mTracks.count == 0)  // 如果动画没有轨道
	{
		mFrameStart = 0;
		mFrameCount = 0;
		return;
	}
	GetFramesForTrackIndex(theTrackIndex, mFrameStart, mFrameCount);
	if (mAnimRate != 0)
		ConfigureFramePeriod(*this, ReanimClock::PeriodFromRate(mFrameCount, mAnimRate));
	ConfigureFramePeriod(*this, mPeriodNs);
    BindFramePosition(*this, mLastUpdateTimeNs, mAnimRate >= 0 ? 0 : mPeriodNs - 1);
	mLastFrameNs = SamplePlayback().phaseNs;
}

//0x4732C0
bool Reanimation::TrackExists(ReanimTrackId theTrackId)
{
	return ReanimatorTrackIndexOf(mReanimationType, theTrackId) >= 0;
}

//0x473310
void Reanimation::StartBlend(int theBlendTime)
{
	for (int aTrackIndex = 0; aTrackIndex < mDefinition->mTracks.count; aTrackIndex++)
	{
		ReanimatorTransform aTransform;
		GetCurrentTransform(aTrackIndex, &aTransform);
		if (FloatRoundToInt(aTransform.mFrame) >= 0)  // 若当前轨道当前不处于空白帧
		{
			ReanimatorTrackInstance* aTrackInstance = &mTrackInstances[aTrackIndex];
			aTrackInstance->mBlendTransform = aTransform;  // 记录当前变换为混合的初始（源）变换
			aTrackInstance->mBlendTime = theBlendTime;
			aTrackInstance->mBlendCounter = theBlendTime;
			ReanimSetActiveBit(*this, aTrackIndex);  // P4：混合置位；归零后由 Update 自愈清位
			aTrackInstance->mBlendTransform.mFont = nullptr;
			aTrackInstance->mBlendTransform.mText = nullptr;
			aTrackInstance->mBlendTransform.mImageSlot = 0;
		}
	}
}

//0x4733F0
void Reanimation::ReanimationDie()
{
	if (!mDead)
	{
		mDead = true;
		if (mDefinition == nullptr)
			return;
		for (int aTrackIndex = 0; aTrackIndex < mDefinition->mTracks.count; aTrackIndex++)
		{
			TOD_ASSERT(mTrackInstances);
			AttachmentDie(mTrackInstances[aTrackIndex].mAttachmentID);
		}
	}
}

void Reanimation::SetShakeOverride(ReanimTrackId theTrackId, float theShakeAmount)
{
	const int aTrackIndex = FindTrackIndex(theTrackId);
	mTrackInstances[aTrackIndex].mShakeOverride = theShakeAmount;
	ReanimSetActiveBit(*this, aTrackIndex);  // P4：置位；归零由 Update 自愈
}

//0x473470
void Reanimation::SelectVariant(std::uint32_t theVariant)
{
	if (!mDefinition || theVariant >= mDefinition->mVariantCount)
		throw std::out_of_range("reanimation appearance variant out of range");
	mVariant = theVariant;
}

//0x473490
// GOTY @Patoke: 0x477BB0
void Reanimation::SetVariantOption(ReanimTrackId theTrackId, std::uint32_t theOption)
{
	const int track = FindTrackIndex(theTrackId);
	if (theOption >= mDefinition->mVariantActionCounts[track])
	{
		if (theOption == 0 && mDefinition->mVariantActionCounts[track] == 0)
			return;
		throw std::out_of_range("reanimation appearance track option out of range");
	}
	mVariant = mDefinition->mVariantTransitions[
		std::size_t(mVariant) * mDefinition->mVariantActionCount +
		mDefinition->mVariantActionStarts[track] + theOption];
}

std::uint32_t Reanimation::VariantOption(ReanimTrackId theTrackId)
{
	const int track = FindTrackIndex(theTrackId);
	return mDefinition->mVariantOptions[std::size_t(mVariant) * mDefinition->mTracks.count + track];
}

//0x4734B0
void Reanimation::SetTruncateDisappearingFrames(ReanimTrackId theTrackId, bool theTruncateDisappearingFrames)
{
	GetTrackInstanceByName(theTrackId)->mTruncateDisappearingFrames = theTruncateDisappearingFrames;
}

void Reanimation::SetTruncateDisappearingFramesAll(bool theTruncateDisappearingFrames)
{
	for (int aTrackIndex = 0; aTrackIndex < mDefinition->mTracks.count; aTrackIndex++)  // 依次设置每一轨道
		mTrackInstances[aTrackIndex].mTruncateDisappearingFrames = theTruncateDisappearingFrames;
}

void ReanimationHolder::DisposeHolder()
{
	mReanimations.DataArrayDispose();
}

//0x473500
ReanimationHolder::~ReanimationHolder()
{
	DisposeHolder();
}

void ReanimationHolder::InitializeHolder()
{
	mReanimations.DataArrayInitialize(1024U, DataArrayKind::Reanimations);
}

//0x473590
Reanimation* ReanimationHolder::AllocReanimation(float theX, float theY, int theRenderOrder, ReanimationType theReanimationType, std::int64_t nowNs, ReanimTimeDomain domain)
{
	TOD_ASSERT(mReanimations.mSize != mReanimations.mMaxSize);
	Reanimation* aReanim = mReanimations.DataArrayAlloc();
	aReanim->mRenderOrder = theRenderOrder;
	aReanim->mReanimationHolder = this;
	mTimeDomains[mReanimations.DataArrayIndexOf(aReanim)] = domain;
	aReanim->ReanimationInitializeType(theX, theY, theReanimationType, nowNs);
	return aReanim;
}

//0x4735E0
void ReanimatorEnsureDefinitionLoaded(ReanimationType theReanimType, bool theIsPreloading)
{
	TOD_ASSERT(theReanimType >= 0 && theReanimType < gReanimatorDefCount);
	if (theIsPreloading && gSexyAppBase && (gSexyAppBase->mShutdown || gAppCloseRequest()))
		return;
	GeneratedDefinitions::LoadReanimationImages((int)theReanimType);
}

//0x473750
void ReanimatorLoadDefinitions(ReanimationParams* theReanimationParamArray, int theReanimationParamArraySize)
{
	if (gReanimatorDefArray)
		return;
	if (theReanimationParamArray != gLawnReanimationArray ||
		theReanimationParamArraySize != (int)ReanimationType::NUM_REANIMS)
		throw std::invalid_argument("invalid static reanimation catalog");
	gReanimationParamArraySize = theReanimationParamArraySize;
	gReanimationParamArray = theReanimationParamArray;
	gReanimatorDefCount = theReanimationParamArraySize;
	gReanimatorDefArray = GeneratedDefinitions::Reanimations;

	// P4：attacher 位掩码已前移编译期（generated/ReanimAttacherMasks.h，
	// 由 MPHF 头 + attacher__ 区间生成；reanimc 重跑经 CMake 依赖联动再生）
	TOD_ASSERT(theReanimationParamArraySize <= kReanimAttacherTypeCount);

	// Definitions are static. Early loading-screen atlases bind on the primary
	// thread; the remaining PNGs decode on the loading thread.
}

// Decode definitions and their owned atlas PNGs on the loading thread.
// 就绪门（原子）：UploadAll（主线程 gate）在装载线程写 def 条目期间
// 禁止遍历，防数据竞争（tracks 指针半写窗口）。
static std::atomic<bool> gReanimatorAllDefinitionDataLoaded{false};

void ReanimatorLoadAllDefinitionData()
{
	if (gReanimatorDefArray == nullptr || gReanimationParamArray == nullptr)
		return;

	GeneratedDefinitions::LoadAllImages();
	gReanimatorAllDefinitionDataLoaded.store(true, std::memory_order_release);

}

void ReanimatorPreloadAll()
{
	if (gReanimatorDefArray == nullptr || gReanimationParamArray == nullptr)
		return;

	for (unsigned int i = 0; i < gReanimatorDefCount; ++i)
	{
		const ReanimationType aType = (ReanimationType)i;
		ReanimatorEnsureDefinitionLoaded(aType, true);
		ReanimationPreload(aType);
	}
}

int ReanimatorUploadAllTexturesToGPU()
{
	if (gSexyAppBase == nullptr || gSexyAppBase->mDDInterface == nullptr ||
		gSexyAppBase->mDDInterface->mRenderInterface == nullptr ||
		gSexyAppBase->mDDInterface->mRenderInterface->mRenderer == nullptr ||
		gReanimatorDefArray == nullptr)
		return -1;
	// 就绪门：全量定义装载（装载线程）未完成前禁止遍历（tracks 半写窗口）
	if (!gReanimatorAllDefinitionDataLoaded.load(std::memory_order_acquire) ||
		!gParticleAllDefinitionDataLoaded.load(std::memory_order_acquire))
		return -1;

	const int uploaded = DefinitionUploadAtlasImages();
	return uploaded;
}

//0x473870
void ReanimatorFreeDefinitions()
{
	gReanimatorAllDefinitionDataLoaded.store(false,std::memory_order_release);
	GeneratedDefinitions::ReleaseReanimationImages();
	gReanimatorDefArray = nullptr;
	gReanimatorDefCount = 0;
	gReanimationParamArray = nullptr;
	gReanimationParamArraySize = 0;
}

//0x473930
bool Reanimation::IsTrackShowing(ReanimTrackId theTrackId)
{
	ReanimatorFrameTime aFrameTime;
	GetFrameTime(&aFrameTime);
	int aTrackIndex = FindTrackIndex(theTrackId);
	TOD_ASSERT(aTrackIndex >= 0 && aTrackIndex < mDefinition->mTracks.count);
	const auto& aTrack = mDefinition->mTracks.tracks[aTrackIndex];
	if (aTrack.mTransforms.mTransforms == nullptr)
		throw std::logic_error("IsTrackShowing requires CPU keyframes for track index " +
			std::to_string(aTrackIndex) + " (id " + std::to_string(static_cast<std::uint32_t>(aTrack.mTrackId)) + ")");
	return aTrack.mTransforms.mTransforms[aFrameTime.mAnimFrameAfterInt].mFrame >= 0.0f;  // 返回下一整数帧是否存在图像
}

//0x473980
void Reanimation::ShowOnlyTrack(ReanimTrackId theTrackId)
{
	// Duplicate IDs retain the original all-matching-tracks behavior.
	for (int i = 0; i < mDefinition->mTracks.count; i++)
		mTrackInstances[i].mRenderGroup = mDefinition->mTracks.tracks[i].mTrackId == theTrackId
			? RENDER_GROUP_NORMAL : RENDER_GROUP_HIDDEN;
	ReanimOccupyGroupBit(*this, RENDER_GROUP_NORMAL);  // P4：目标轨占组 0（其余 HIDDEN 不占）
}

//0x4739E0
// GOTY @Patoke: 0x478120
void Reanimation::AssignRenderGroupToTrack(ReanimTrackId theTrackId, int theRenderGroup)
{
	const int aTrackIndex = ReanimatorTrackIndexOf(mReanimationType, theTrackId);
	if (aTrackIndex >= 0)  // 找不到时保持原行为：静默无操作
	{
		mTrackInstances[aTrackIndex].mRenderGroup = theRenderGroup;
		ReanimOccupyGroupBit(*this, theRenderGroup);  // P4：占用置位（只置不清，方向安全）
	}
}

void Reanimation::AssignRenderGroupToRange(const ReanimTrackIdRange& theRange, int theRenderGroup)
{
	for (int i = 0; i < mDefinition->mTracks.count; i++)
	{
		if (theRange.Contains(mDefinition->mTracks.tracks[i].mTrackId))
			mTrackInstances[i].mRenderGroup = theRenderGroup;
	}
	ReanimOccupyGroupBit(*this, theRenderGroup);  // P4：占用置位（只置不清，方向安全）
}

//0x473AE0
void Reanimation::PropogateColorToAttachments()
{
	for (int i = 0; i < mDefinition->mTracks.count; i++)
		AttachmentPropogateColor(
			mTrackInstances[i].mAttachmentID, mColorOverride, mEnableExtraAdditiveDraw, mExtraAdditiveColor, mEnableExtraOverlayDraw, mExtraOverlayColor
		);
}

//0x473B70
bool Reanimation::ShouldTriggerTimedEvent(float theEventTime)
{
	TOD_ASSERT(theEventTime >= 0.0f && theEventTime <= 1.0f);
	return TimedEventCount(static_cast<std::int64_t>(std::llround(theEventTime * 1'000'000.0)), 1'000'000) != 0;
}
//0x473BF0
// GOTY @Patoke: 0x478310
void Reanimation::PlayReanim(ReanimTrackId theTrackId, ReanimLoopType theLoopType, int theBlendTime, float theAnimRate)
{
	if (theBlendTime > 0)  // 当需要补间过渡时，开始混合
		StartBlend(theBlendTime);
	if (theAnimRate != 0)
		ApplyRate(theAnimRate);

	ChangeLoopType(theLoopType);
	mLoopCount = 0;
	SetFramesForLayer(theTrackId);
}

//0x473C60
//0x473EB0
void Reanimation::AttacherSynchWalkSpeed(int theTrackIndex, Reanimation* theAttachReanim, int theTextRunStart, int theTextRunEnd)
{
	ReanimatorTrack* aTrack = &mDefinition->mTracks.tracks[theTrackIndex];
	if (aTrack->mTransforms.mTransforms == nullptr)
		throw std::logic_error("AttacherSynchWalkSpeed requires CPU keyframes for parent track index " +
			std::to_string(theTrackIndex) + " (id " + std::to_string(static_cast<std::uint32_t>(aTrack->mTrackId)) + ")");
	const std::int64_t aParentEnd = static_cast<std::int64_t>(mFrameStart) + mFrameCount;
	const int aPlaceHolderFrameStart = std::max(theTextRunStart, mFrameStart);
	const std::int64_t aRunEnd = std::min<std::int64_t>(theTextRunEnd, aParentEnd);
	if (aPlaceHolderFrameStart < 0 || aRunEnd <= aPlaceHolderFrameStart ||
		aRunEnd > aTrack->mTransforms.count)
	{
		theAttachReanim->ApplyRate(0);
		return;
	}
	const int aPlaceHolderFrameEnd = static_cast<int>(aRunEnd) - 1;
	int aPlaceHolderFrameCount = aPlaceHolderFrameEnd - aPlaceHolderFrameStart;
	const auto& aPlaceHolderStartTrans = aTrack->mTransforms.mTransforms[aPlaceHolderFrameStart];
	const auto& aPlaceHolderEndTrans = aTrack->mTransforms.mTransforms[aPlaceHolderFrameEnd];
	if (mAnimRate == 0)
	{
		theAttachReanim->ApplyRate(0);
		return;
	}
	double aPlaceHolderDistance = static_cast<double>(aPlaceHolderStartTrans.mTransX) -
		static_cast<double>(aPlaceHolderEndTrans.mTransX);
	double aPlaceHolderSeconds = aPlaceHolderFrameCount / static_cast<double>(mAnimRate);
	if (FloatApproxEqual(aPlaceHolderSeconds, 0.0f))  // 如果当前所在区间不存在任何帧
	{
		theAttachReanim->ApplyRate(0);
		return;
	}

	int aGroundTrackIndex = theAttachReanim->FindTrackIndex(ReanimTrackId::_ground);
	ReanimatorTrack* aGroundTrack = &theAttachReanim->mDefinition->mTracks.tracks[aGroundTrackIndex];
	if (aGroundTrack->mTransforms.mTransforms == nullptr)
		throw std::logic_error("AttacherSynchWalkSpeed requires CPU keyframes for attached ground track index " +
			std::to_string(aGroundTrackIndex) + " (id " + std::to_string(static_cast<std::uint32_t>(aGroundTrack->mTrackId)) + ")");
	const auto& aTransformGuyStart = aGroundTrack->mTransforms.mTransforms[theAttachReanim->mFrameStart];
	const auto& aTransformGuyEnd = aGroundTrack->mTransforms.mTransforms[theAttachReanim->mFrameStart + theAttachReanim->mFrameCount - 1];
	double aGuyDistance = static_cast<double>(aTransformGuyEnd.mTransX) -
		static_cast<double>(aTransformGuyStart.mTransX);
	if (aGuyDistance < FLT_EPSILON || aPlaceHolderDistance < FLT_EPSILON)  // 如果占位位移为 0 或实际动画周期位移为 0，则附属动画无法移动
	{
		theAttachReanim->ApplyRate(0);
		return;
	}

	double aLoops = aPlaceHolderDistance / aGuyDistance;  // 以附属动画目标位移（占位位移）除以其周期位移，得到附属动画需要循环的周期数
	ReanimatorTransform aTransformGuyCurrent;
	theAttachReanim->GetCurrentTransform(aGroundTrackIndex, &aTransformGuyCurrent);
	AttachEffect* aAttachEffect = FindFirstAttachment(mTrackInstances[theTrackIndex].mAttachmentID);
	if (aAttachEffect != nullptr)
	{
		double aGuyCurrentDistance = static_cast<double>(aTransformGuyCurrent.mTransX) -
			static_cast<double>(aTransformGuyStart.mTransX);
		double aGuyExpectedDistance = aGuyDistance *
			static_cast<double>(theAttachReanim->SamplePlayback().phaseNs) /
			static_cast<double>(theAttachReanim->mPeriodNs);
		aAttachEffect->mOffset.m02 = static_cast<float>(aGuyExpectedDistance - aGuyCurrentDistance);
	}
	theAttachReanim->ApplyRate(static_cast<float>(aLoops * theAttachReanim->mFrameCount / aPlaceHolderSeconds));
}

//0x4740B0
void Reanimation::UpdateAttacherTrack(int theTrackIndex)
{
	ReanimatorTrackInstance* aTrackInstance = &mTrackInstances[theTrackIndex];
	ReanimatorFrameTime aFrameTime;
	GetFrameTime(&aFrameTime);
	ReanimatorTransform aTransform;
	GetTransformAtTime(theTrackIndex, &aTransform, &aFrameTime);  // same frame sample for alpha and segment

	// IDT3：编译期链接的段表。当前帧号 -> 段（少量段线性查找），全部整数。
	const std::uint32_t aFrame = static_cast<std::uint32_t>(aFrameTime.mAnimFrameBeforeInt);

	// 1073 FUN_004787D0 的第一判定：GetTransformAtTime 产出的 mFrame 含截断消失帧
	// 规则（before 可见 → after 隐藏且 fraction>0 即强制 -1），解析前先查它。
	// 段表路径必须同样先查：只按整数帧号入段销毁会晚整整一个源帧的插值区间
	// （credits 7fps 下 ≈143ms 残留）。v1 legacy 路径同款检查在函数首。
	// 注意：隐藏帧上的 kill/respawn 循环是 1073 原生编舞（credits 开场 one two
	// three four 每个数一个僵尸在不同挂点弹出），不可保活抑制。
	// 2026-10-06 复审回退：段表驱动生死的版本在 credits 终场产生附件重影
	//（undead 歌手/Zombie12/Zombie22 的 125 拍、大军 217/300 眨拍按整数帧
	// 杀旧挂新，旧子动画未即逝→双歌手+双气泡+红字重影，比大军缺失更糟，
	// 用户裁决回退本判定）。
	if (aTransform.mFrame == -1.0f)
	{
		AttachmentDie(aTrackInstance->mAttachmentID);
		return;
	}

	// 段表空窗 = 继承：源数据里 attacher 编码文本在占位区间内连续重复，
	// 折叠段表内部的空窗是编码过渡的伪影，不是“清除附件”指令；
	// 只有显式 targetType=REANIM_NONE 的段才代表原版的名字解析失败。
	// （与 v1 运行时对拍：v1 在这些帧重复解析同一编码，附件保持。）
	AttacherSegmentRT* aSeg = nullptr;
	for (AttacherSegmentRT& seg : GeneratedDefinitions::AttacherSegments[(int)mReanimationType])
		if (seg.trackIndex == static_cast<std::uint32_t>(theTrackIndex) &&
		    seg.startFrame <= aFrame && aFrame < seg.endFrame)
		{
			aSeg = &seg;
			break;
		}
	if (aSeg == nullptr)
		return;  // 继承：保持现有附件不变

	const ReanimationType aReanimationType = static_cast<ReanimationType>(aSeg->targetType);
	if (aReanimationType == ReanimationType::REANIM_NONE)
	{
		AttachmentDie(aTrackInstance->mAttachmentID);  // 清除附件
		return;
	}
	const ReanimTrackId aActionId = static_cast<ReanimTrackId>(aSeg->actionTrackId);
	const float aAnimRate = aSeg->rate;
	const ReanimLoopType aLoopType =
	    aSeg->loop == 1 ? ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD
	    : aSeg->loop == 2 ? ReanimLoopType::REANIM_PLAY_ONCE
	                      : ReanimLoopType::REANIM_LOOP;

	Reanimation* aAttachReanim = FindReanimAttachment(aTrackInstance->mAttachmentID);
	if (aAttachReanim == nullptr || aAttachReanim->mReanimationType != aReanimationType)  // 如果原先没有附属动画，或原附属动画不是上述设定的动画
	{
		AttachmentDie(aTrackInstance->mAttachmentID);  // 清除原有附件
		aAttachReanim = mReanimationHolder->AllocReanimation(0.0f, 0.0f, 0, aReanimationType, mLastUpdateTimeNs);
		mReanimationHolder->AdoptAnimationTime(*aAttachReanim,*this,mLastUpdateTimeNs);
		aAttachReanim->ChangeLoopType(aLoopType);
		aAttachReanim->ApplyRate(aAnimRate);
		AttachReanim(aTrackInstance->mAttachmentID, aAttachReanim, 0.0f, 0.0f);
		ReanimSetActiveBit(*this, theTrackIndex);  // P4：attacher 挂附件置位
		mFrameBasePose = NO_BASE_POSE;  // 设定附属动画后，自身不再存在基准帧
	}

    // Consume the parent's supplied time before changing a live child's rate or
    // layer. Its later attachment update retains this interval for event queries.
    aAttachReanim->PrepareTimeAt(mLastUpdateTimeNs);

	if (aActionId != ReanimTrackId::None)  // 如果定义了附属动画的动作轨道
	{
		// The generated segment already contains its target action's frame range.
		const int aAnimFrameStart = aSeg->mActionFrameStart;
		const int aAnimFrameCount = aSeg->mActionFrameCount;
		if (aAttachReanim->mFrameStart != aAnimFrameStart || aAttachReanim->mFrameCount != aAnimFrameCount)  // if (!aAttachReanim->IsAnimPlaying(……))
		{
			aAttachReanim->StartBlend(20);
			aAttachReanim->SetFramesForLayer(aActionId);  // 播放指定轨道上的动作
		}

		if (aAttachReanim->mAnimRate == 12.0f && aActionId == ReanimTrackId::anim_walk && aAttachReanim->TrackExists(ReanimTrackId::_ground))
		{
			AttacherSynchWalkSpeed(theTrackIndex, aAttachReanim,
				static_cast<int>(aSeg->textRunStart), static_cast<int>(aSeg->textRunEnd));
		}
		else
			aAttachReanim->ApplyRate(aAnimRate);
		aAttachReanim->ChangeLoopType(aLoopType);
	}

	Color aColor = ColorsMultiply(mColorOverride, aTrackInstance->mTrackColor);
	aColor.mAlpha = ClampInt(FloatRoundToInt(aTransform.mAlpha * aColor.mAlpha), 0, 255);
	AttachmentPropogateColor(aTrackInstance->mAttachmentID, aColor, mEnableExtraAdditiveDraw, mExtraAdditiveColor, mEnableExtraOverlayDraw, mExtraOverlayColor);
}

//0x4745B0
bool Reanimation::IsAnimPlaying(ReanimTrackId theTrackId)
{
	int aFrameStart, aFrameCount;
	GetFramesForLayer(theTrackId, aFrameStart, aFrameCount);
	return mFrameStart == aFrameStart && mFrameCount == aFrameCount;
}

//0x4745F0
Reanimation* Reanimation::FindSubReanim(ReanimationType theReanimType)
{
	if (mReanimationType == theReanimType)
		return this;

	for (int i = 0; i < mDefinition->mTracks.count; i++)
	{
		Reanimation* aReanimation = FindReanimAttachment(mTrackInstances[i].mAttachmentID);
		if (aReanimation != nullptr)
		{
			Reanimation* aSubReanim = aReanimation->FindSubReanim(theReanimType);
			if (aSubReanim != nullptr)
				return aSubReanim;
		}
	}

	return nullptr;
}
