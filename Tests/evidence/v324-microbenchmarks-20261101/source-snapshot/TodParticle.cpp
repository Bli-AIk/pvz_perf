#include "TodDebug.h"
#include "Definition.h"
#include "TodParticle.h"
#include "EffectSystem.h"
#include "Reanimator.h"
#include "ReanimClock.h"
#include "GameConstants.h"
#include "generated/GeneratedDefinitions.h"
#include "graphics/Graphics.h"
#include "graphics/DDInterface.h"
#include "graphics/RenderInterface.h"
#include <stdexcept>
#include <limits>

namespace
{
constexpr unsigned int PARTICLE_POOL_CAPACITY = 1024U;
// 原素材参数的 10ms 标度，仅用于单位换算，不是独立的模拟步长。
constexpr std::int64_t ParticleSourceIntervalNs = 10'000'000;
constexpr std::int64_t ParticleCycle = ParticleClock::Cycle;
double LifeFraction(std::int64_t age, std::int64_t duration)
{
	return age < 0 ? -1.0 : static_cast<double>(age) /
		std::max<std::int64_t>(1, duration);
}
std::uint32_t NextParticleRandom(std::uint32_t& state)
{
	// Explicit state belongs to the effect and is saved with it. No global
	// draws occur during zero-duration or paused updates.
	state = state * 1664525U + 1013904223U;
	return state;
}
float ShakeSample(std::uint32_t seed, std::int64_t age, unsigned axis)
{
	if (age < 0) return 0.0f;
	std::uint32_t value = seed ^ static_cast<std::uint32_t>(age) * 0x9e3779b9U ^
		static_cast<std::uint32_t>(std::uint64_t(age) >> 32) ^ axis * 0x85ebca6bU;
	value ^= value >> 16; value *= 0x7feb352dU;
	value ^= value >> 15; value *= 0x846ca68bU;
	value ^= value >> 16;
	return static_cast<float>(value >> 8) * (2.0f / 16777216.0f) - 1.0f;
}
std::int64_t SampleClockIncrement(float rate, std::uint64_t deltaNs)
{
	// Source rates are per second, independent of the current logic frequency.
	const double units = static_cast<double>(rate) * static_cast<double>(deltaNs) * 1000.0;
	if (!std::isfinite(units) || units >= static_cast<double>(std::numeric_limits<std::int64_t>::max()) ||
		units <= static_cast<double>(std::numeric_limits<std::int64_t>::min()))
		throw std::overflow_error("particle sampled rate exceeds clock range");
	return static_cast<std::int64_t>(std::llround(units));
}
}

float TodParticleEmitter::RandomFloat(float maximum)
{
	return static_cast<float>(NextParticleRandom(mParticleSystem->mRandomState) >> 8) *
		(1.0f / 16777216.0f) * maximum;
}
int TodParticleEmitter::RandomInt(int maximum)
{
	return maximum > 0 ? static_cast<int>(NextParticleRandom(mParticleSystem->mRandomState) %
		static_cast<std::uint32_t>(maximum)) : 0;
}
int gParticleDefCount;                      // [0x6A9F08]
TodParticleDefinition* gParticleDefArray;   // [0x6A9F0C]
std::atomic<bool> gParticleAllDefinitionDataLoaded{false};
static int gParticleParamArraySize;				// [0x6A9F10]
static ParticleParams* gParticleParamArray;		// [0x6A9F14]

ParticleParams gLawnParticleArray[(int)ParticleEffect::NUM_PARTICLES] = {
	{ ParticleEffect::PARTICLE_MELONSPLASH, "particles\\MelonImpact.xml" },
	{ ParticleEffect::PARTICLE_WINTERMELON, "particles\\WinterMelonImpact.xml" },
	{ ParticleEffect::PARTICLE_FUMECLOUD, "particles\\FumeCloud.xml" },
	{ ParticleEffect::PARTICLE_POPCORNSPLASH, "particles\\PopcornSplash.xml" },
	{ ParticleEffect::PARTICLE_POWIE, "particles\\Powie.xml" },
	{ ParticleEffect::PARTICLE_JACKEXPLODE, "particles\\JackExplode.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_HEAD, "particles\\ZombieHead.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_ARM, "particles\\ZombieArm.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_TRAFFIC_CONE, "particles\\ZombieTrafficCone.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_PAIL, "particles\\ZombiePail.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_HELMET, "particles\\ZombieHelmet.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_FLAG, "particles\\ZombieFlag.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_DOOR, "particles\\ZombieDoor.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_NEWSPAPER, "particles\\ZombieNewspaper.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_HEADLIGHT, "particles\\ZombieHeadLight.xml" },
	{ ParticleEffect::PARTICLE_POW, "particles\\Pow.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_POGO, "particles\\ZombiePogo.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_NEWSPAPER_HEAD, "particles\\ZombieNewspaperHead.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_BALLOON_HEAD, "particles\\ZombieBalloonHead.xml" },
	{ ParticleEffect::PARTICLE_SOD_ROLL, "particles\\SodRoll.xml" },
	{ ParticleEffect::PARTICLE_GRAVE_STONE_RISE, "particles\\GraveStoneRise.xml" },
	{ ParticleEffect::PARTICLE_PLANTING, "particles\\Planting.xml" },
	{ ParticleEffect::PARTICLE_PLANTING_POOL, "particles\\PlantingPool.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_RISE, "particles\\ZombieRise.xml" },
	{ ParticleEffect::PARTICLE_GRAVE_BUSTER, "particles\\GraveBuster.xml" },
	{ ParticleEffect::PARTICLE_GRAVE_BUSTER_DIE, "particles\\GraveBusterDie.xml" },
	{ ParticleEffect::PARTICLE_POOL_SPLASH, "particles\\PoolSplash.xml" },
	{ ParticleEffect::PARTICLE_ICE_SPARKLE, "particles\\IceSparkle.xml" },
	{ ParticleEffect::PARTICLE_SEED_PACKET, "particles\\SeedPacket.xml" },
	{ ParticleEffect::PARTICLE_TALL_NUT_BLOCK, "particles\\TallNutBlock.xml" },
	{ ParticleEffect::PARTICLE_DOOM, "particles\\Doom.xml" },
	{ ParticleEffect::PARTICLE_DIGGER_RISE, "particles\\DiggerRise.xml" },
	{ ParticleEffect::PARTICLE_DIGGER_TUNNEL, "particles\\DiggerTunnel.xml" },
	{ ParticleEffect::PARTICLE_DANCER_RISE, "particles\\DancerRise.xml" },
	{ ParticleEffect::PARTICLE_POOL_SPARKLY, "particles\\PoolSparkly.xml" },
	{ ParticleEffect::PARTICLE_WALLNUT_EAT_SMALL, "particles\\WallnutEatSmall.xml" },
	{ ParticleEffect::PARTICLE_WALLNUT_EAT_LARGE, "particles\\WallnutEatLarge.xml" },
	{ ParticleEffect::PARTICLE_PEA_SPLAT, "particles\\PeaSplat.xml" },
	{ ParticleEffect::PARTICLE_BUTTER_SPLAT, "particles\\ButterSplat.xml" },
	{ ParticleEffect::PARTICLE_CABBAGE_SPLAT, "particles\\CabbageSplat.xml" },
	{ ParticleEffect::PARTICLE_PUFF_SPLAT, "particles\\PuffSplat.xml" },
	{ ParticleEffect::PARTICLE_STAR_SPLAT, "particles\\StarSplat.xml" },
	{ ParticleEffect::PARTICLE_ICE_TRAP, "particles\\IceTrap.xml" },
	{ ParticleEffect::PARTICLE_SNOWPEA_SPLAT, "particles\\SnowPeaSplat.xml" },
	{ ParticleEffect::PARTICLE_SNOWPEA_PUFF, "particles\\SnowPeaPuff.xml" },
	{ ParticleEffect::PARTICLE_SNOWPEA_TRAIL, "particles\\SnowPeaTrail.xml" },
	{ ParticleEffect::PARTICLE_LANTERN_SHINE, "particles\\LanternShine.xml" },
	{ ParticleEffect::PARTICLE_SEED_PACKET_PICKUP, "particles\\Award.xml" },
	{ ParticleEffect::PARTICLE_POTATO_MINE, "particles\\PotatoMine.xml" },
	{ ParticleEffect::PARTICLE_POTATO_MINE_RISE, "particles\\PotatoMineRise.xml" },
	{ ParticleEffect::PARTICLE_PUFFSHROOM_TRAIL, "particles\\PuffShroomTrail.xml" },
	{ ParticleEffect::PARTICLE_PUFFSHROOM_MUZZLE, "particles\\PuffShroomMuzzle.xml" },
	{ ParticleEffect::PARTICLE_SEED_PACKET_FLASH, "particles\\SeedPacketFlash.xml" },
	{ ParticleEffect::PARTICLE_WHACK_A_ZOMBIE_RISE, "particles\\WhackAZombieRise.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_LADDER, "particles\\ZombieLadder.xml" },
	{ ParticleEffect::PARTICLE_UMBRELLA_REFLECT, "particles\\UmbrellaReflect.xml" },
	{ ParticleEffect::PARTICLE_SEED_PACKET_PICK, "particles\\SeedPacketPick.xml" },
	{ ParticleEffect::PARTICLE_ICE_TRAP_ZOMBIE, "particles\\IceTrapZombie.xml" },
	{ ParticleEffect::PARTICLE_ICE_TRAP_RELEASE, "particles\\IceTrapRelease.xml" },
	{ ParticleEffect::PARTICLE_ZAMBONI_SMOKE, "particles\\ZamboniSmoke.xml" },
	{ ParticleEffect::PARTICLE_GLOOMCLOUD, "particles\\GloomCloud.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_POGO_HEAD, "particles\\ZombiePogoHead.xml" },
	{ ParticleEffect::PARTICLE_ZAMBONI_TIRE, "particles\\ZamboniTire.xml" },
	{ ParticleEffect::PARTICLE_ZAMBONI_EXPLOSION, "particles\\ZamboniExplosion.xml" },
	{ ParticleEffect::PARTICLE_ZAMBONI_EXPLOSION2, "particles\\ZamboniExplosion2.xml" },
	{ ParticleEffect::PARTICLE_CATAPULT_EXPLOSION, "particles\\CatapultExplosion.xml" },
	{ ParticleEffect::PARTICLE_MOWER_CLOUD, "particles\\MowerCloud.xml" },
	{ ParticleEffect::PARTICLE_BOSS_ICE_BALL, "particles\\BossIceBallTrail.xml" },
	{ ParticleEffect::PARTICLE_BLASTMARK, "particles\\BlastMark.xml" },
	{ ParticleEffect::PARTICLE_COIN_PICKUP_ARROW, "particles\\CoinPickupArrow.xml" },
	{ ParticleEffect::PARTICLE_PRESENT_PICKUP, "particles\\PresentPickup.xml" },
	{ ParticleEffect::PARTICLE_IMITATER_MORPH, "particles\\ImitaterMorph.xml" },
	{ ParticleEffect::PARTICLE_MOWERED_ZOMBIE_HEAD, "particles\\MoweredZombieHead.xml" },
	{ ParticleEffect::PARTICLE_MOWERED_ZOMBIE_ARM, "particles\\MoweredZombieArm.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_HEAD_POOL, "particles\\ZombieHeadPool.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_BOSS_FIREBALL, "particles\\Zombie_boss_fireball.xml" },
	{ ParticleEffect::PARTICLE_FIREBALL_DEATH, "particles\\FireballDeath.xml" },
	{ ParticleEffect::PARTICLE_ICEBALL_DEATH, "particles\\IceballDeath.xml" },
	{ ParticleEffect::PARTICLE_ICEBALL_TRAIL, "particles\\Iceball_Trail.xml" },
	{ ParticleEffect::PARTICLE_FIREBALL_TRAIL, "particles\\Fireball_Trail.xml" },
	{ ParticleEffect::PARTICLE_BOSS_EXPLOSION, "particles\\BossExplosion.xml" },
	{ ParticleEffect::PARTICLE_SCREEN_FLASH, "particles\\ScreenFlash.xml" },
	{ ParticleEffect::PARTICLE_TROPHY_SPARKLE, "particles\\TrophySparkle.xml" },
	{ ParticleEffect::PARTICLE_PORTAL_CIRCLE, "particles\\PortalCircle.xml" },
	{ ParticleEffect::PARTICLE_PORTAL_SQUARE, "particles\\PortalSquare.xml" },
	{ ParticleEffect::PARTICLE_POTTED_PLANT_GLOW, "particles\\PottedPlantGlow.xml" },
	{ ParticleEffect::PARTICLE_POTTED_WATER_PLANT_GLOW, "particles\\PottedWaterPlantGlow.xml" },
	{ ParticleEffect::PARTICLE_POTTED_ZEN_GLOW, "particles\\PottedZenGlow.xml" },
	{ ParticleEffect::PARTICLE_MIND_CONTROL, "particles\\MindControl.xml" },
	{ ParticleEffect::PARTICLE_VASE_SHATTER, "particles\\VaseShatter.xml" },
	{ ParticleEffect::PARTICLE_VASE_SHATTER_LEAF, "particles\\VaseShatterLeaf.xml" },
	{ ParticleEffect::PARTICLE_VASE_SHATTER_ZOMBIE, "particles\\VaseShatterZombie.xml" },
	{ ParticleEffect::PARTICLE_AWARD_PICKUP_ARROW, "particles\\AwardPickupArrow.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_SEAWEED, "particles\\Zombie_seaweed.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_MUSTACHE, "particles\\ZombieMustache.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_SUNGLASS, "particles\\ZombieFutureGlasses.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_PINATA, "particles\\Pinata.xml" },
	{ ParticleEffect::PARTICLE_DUST_SQUASH, "particles\\Dust_Squash.xml" },
	{ ParticleEffect::PARTICLE_DUST_FOOT, "particles\\Dust_Foot.xml" },
	{ ParticleEffect::PARTICLE_ZOMBIE_DAISIES, "particles\\Daisy.xml" },
	{ ParticleEffect::PARTICLE_CREDIT_STROBE, "particles\\Credits_Strobe.xml" },
	{ ParticleEffect::PARTICLE_CREDITS_RAYSWIPE, "particles\\Credits_RaysWipe.xml" },
	{ ParticleEffect::PARTICLE_CREDITS_ZOMBIEHEADWIPE, "particles\\Credits_ZombieHeadWipe.xml" },
	{ ParticleEffect::PARTICLE_STARBURST, "particles\\Starburst.xml" },
	{ ParticleEffect::PARTICLE_CREDITS_FOG, "particles\\Credits_fog.xml" },
	{ ParticleEffect::PARTICLE_PERSENT_PICK_UP_ARROW, "particles\\UpsellArrow.xml" },
};

//0x515CE0
void TodParticleLoadDefinitions(ParticleParams* theParticleParamArray, int theParticleParamArraySize)
{
	if (gParticleParamArray || gParticleDefArray)
		return;
	if (theParticleParamArray != gLawnParticleArray ||
		theParticleParamArraySize != (int)ParticleEffect::NUM_PARTICLES)
		throw std::invalid_argument("invalid static particle catalog");
	gParticleAllDefinitionDataLoaded.store(false,std::memory_order_release);
	gParticleParamArraySize = theParticleParamArraySize;
	gParticleParamArray = theParticleParamArray;
	gParticleDefCount = theParticleParamArraySize;
	gParticleDefArray = GeneratedDefinitions::Particles;

	try
	{
	GeneratedDefinitions::LoadParticleImages();
	if (gSexyAppBase)
		gSexyAppBase->mCompletedLoadingThreadTasks += 6 * gParticleParamArraySize;
	gParticleAllDefinitionDataLoaded.store(true,std::memory_order_release);
	}
	catch (...)
	{
		TodParticleFreeDefinitions();
		throw;
	}
}

//0x515E30
void TodParticleFreeDefinitions()
{
	gParticleAllDefinitionDataLoaded.store(false,std::memory_order_release);
	GeneratedDefinitions::ReleaseParticleImages();
	gParticleDefArray = nullptr;
	gParticleDefCount = 0;
	gParticleParamArray = nullptr;
	gParticleParamArraySize = 0;
}

TodParticleSystem::TodParticleSystem()
{
	mEffectType = ParticleEffect::PARTICLE_NONE;
	mVariant = 0;
	mParticleDef = nullptr;
	mParticleHolder = nullptr;
	mDead = false;
	mDontUpdate = false;
	mIsAttachment = false;
	mRenderOrder = 0;
	mRandomState = 0;
	mLastUpdateTimeNs = 0;
}

TodParticleSystem::~TodParticleSystem()
{
	ParticleSystemDie();  // Die 内已清册（P3 名册化）；退役链表占位随 TodList 删除（2026-10-02）
}

//0x515E90 : (edx = theEffectType, *theDefinition, ecx = theRenderOrder, theY, theX, *this)
void TodParticleSystem::TodParticleInitializeFromDef(float theX, float theY, int theRenderOrder, TodParticleDefinition* theDefinition, ParticleEffect theEffectType, std::int64_t nowNs, std::uint32_t theVariant)
{
	TOD_ASSERT(mParticleHolder);
	mLastUpdateTimeNs = nowNs;
	if (!theDefinition || theVariant>=theDefinition->mVariantCount)
		throw std::out_of_range("particle appearance variant out of range");
	// 名册侧表：系统池槽位键天然就位，无需分配器（链表成员已退役）
	mParticleDef = theDefinition;
	mEffectType = theEffectType;
	mVariant = theVariant;
	mRenderOrder = theRenderOrder;
	mRandomState = static_cast<std::uint32_t>(Sexy::Rand(0x7fffffff));

	for (int i = 0; i < theDefinition->mEmitterDefCount; i++)
	{
		TodEmitterDefinition& aDef = theDefinition->mEmitterDefs[i];
		if (!FloatTrackIsSet(aDef.mCrossFadeDuration))
		{
			if (TestBit(aDef.mParticleFlags, (int)ParticleFlags::PARTICLE_DIE_IF_OVERLOADED) && mParticleHolder->IsOverLoaded())
			{
				ParticleSystemDie();
				break;
			}
			TodParticleEmitter* aEmitter = mParticleHolder->mEmitters.DataArrayAlloc();
			aEmitter->TodEmitterInitialize(theX, theY, this, &aDef);
			EmitterRosterAdd(aEmitter);
		}
	}
}

//0x515F70 : (*theEmitterDef, *theSystem, theY, theX, ecx = *this)
void TodParticleEmitter::TodEmitterInitialize(float theX, float theY, TodParticleSystem* theSystem, TodEmitterDefinition* theEmitterDef)
{
	mSpawnRemainder = 0;
	mParticlesSpawned = 0;
	mBirthTimeNs = theSystem->mLastUpdateTimeNs;
	mDead = false;
	mColorOverride = Sexy::Color::White;
	mSystemCenter.x = theX;
	mSystemCenter.y = theY;
	mSystemPositionOriginX = mSystemPositionOriginY = 0;
	mSystemPositionBound = 1;
	mCrossFadeEmitterID = ParticleEmitterID::PARTICLEEMITTERID_NULL;
	mCrossFadeEndTimeNs = 0;
	mFrameOverride = -1;
	mParticleSystem = theSystem;
	mScaleOverride = 1.0f;
	mExtraAdditiveDrawOverride = false;
	mSystemDurationNs = 0;
	mEmitterDef = theEmitterDef;
	// 粒子名册侧表：发射器池槽位键天然就位，无需分配器（链表成员已退役）

	if (FloatTrackIsSet(mEmitterDef->mSystemDuration))
		mSystemDurationNs = Sexy::TimeCounter::Encode(FloatTrackEvaluate(mEmitterDef->mSystemDuration, 0.0f, RandomFloat(1.0f)));
	else
		mSystemDurationNs = Sexy::TimeCounter::Encode(FloatTrackEvaluate(mEmitterDef->mParticleDuration, 0.0f, 1.0f));
	mSystemDurationNs = std::max(ParticleSourceIntervalNs, mSystemDurationNs);
	RefreshParticleLifeFraction(*this, theSystem->mLastUpdateTimeNs);

	for (int i = 0; i < mEmitterDef->mSystemFields.count; i++)
	{
		mSystemFieldInterp[i][0] = RandomFloat(1.0f);
		mSystemFieldInterp[i][1] = RandomFloat(1.0f);
	}
	for (int j = 0; j < 10; j++)
		mTrackInterp[j] = RandomFloat(1.0f);

	// Birth samples offsets and minimum-active births without advancing time.
	SampleSystemPosition(0.0);
	UpdateSpawning(0);
}

//0x5160C0
void TodParticleSystem::ParticleSystemDie()
{
	{
		const int aCount = EmitterRosterCount();
		for (int i = 0; i < aCount; i++)
		{
			TodParticleEmitter* aEmitter = EmitterRosterEmitterAt(i);
			aEmitter->DeleteAll();
			mParticleHolder->mEmitters.DataArrayFree(aEmitter);
		}
	}
	EmitterRosterClear();
	mDead = true;
}


//0x516160
TodParticle* TodParticleEmitter::SpawnParticle(int theIndex, int theSpawnCount)
{
	// TodParticleEmitter::SpawnParticle 发射几何分发表：表内匿名 lambda =
	// 原 case 体逐句迁移（生成器 + 逆变换 token 对拍 0 差异）；圆形三标签
	// 共体具名；default 具名并被越界复用。
	using ParticleSpawnPosCaseFn = void (*)(TodParticleEmitter&, float, float&, float&);

	// 标签共体（匿名 lambda 无法跨槽复用，复用即具名）
	// 原 case EMITTER_CIRCLE, EMITTER_CIRCLE_PATH, EMITTER_CIRCLE_EVEN_SPACING
	static constexpr auto ParticleSpawnPosEmitterCircle = [](TodParticleEmitter& e, float aLaunchAngle, float& aPosX, float& aPosY)
	{
	const auto& aHolder = *e.mParticleSystem->mParticleHolder;
	const auto& aData = *aHolder.mData;
	const double aSystemTime = aData.mEmitterLifeFractions[aHolder.mEmitters.DataArrayIndexOf(&e)];
	float aEmitterRadiusInterp = e.RandomFloat(1.0f);
	float aRadius = FloatTrackEvaluate(e.mEmitterDef->mEmitterRadius, aSystemTime, aEmitterRadiusInterp);
	// ★ 以竖直向下的方向为 0 角度
	aPosX = sin(aLaunchAngle) * aRadius;
	aPosY = cos(aLaunchAngle) * aRadius;
	};

	// 原 default（无 case 的 0 型与表外哨兵共用）
	static constexpr auto ParticleSpawnPosDefault = [](TodParticleEmitter& e, float aLaunchAngle, float& aPosX, float& aPosY)
	{
	TOD_ASSERT(false);
	};

	// 匿名 lambda 函数指针表，行序 = EmitterType 枚举值序（0..4）。
	static constexpr ParticleSpawnPosCaseFn kParticleSpawnPosCases[(int)EmitterType::EMITTER_CIRCLE_EVEN_SPACING + 1] = {
	ParticleSpawnPosEmitterCircle,  // 0 EMITTER_CIRCLE（共体）
	// 原 case EMITTER_BOX
	[](TodParticleEmitter& e, float aLaunchAngle, float& aPosX, float& aPosY)
	{
	const auto& aHolder = *e.mParticleSystem->mParticleHolder;
	const auto& aData = *aHolder.mData;
	const double aSystemTime = aData.mEmitterLifeFractions[aHolder.mEmitters.DataArrayIndexOf(&e)];
	float aEmitterBoxXInterp = e.RandomFloat(1.0f);
	float aEmitterBoxYInterp = e.RandomFloat(1.0f);
	aPosX = FloatTrackEvaluate(e.mEmitterDef->mEmitterBoxX, aSystemTime, aEmitterBoxXInterp);
	aPosY = FloatTrackEvaluate(e.mEmitterDef->mEmitterBoxY, aSystemTime, aEmitterBoxYInterp);
	},  // 1 EMITTER_BOX
	// 原 case EMITTER_BOX_PATH
	[](TodParticleEmitter& e, float aLaunchAngle, float& aPosX, float& aPosY)
	{
	const auto& aHolder = *e.mParticleSystem->mParticleHolder;
	const auto& aData = *aHolder.mData;
	const double aSystemTime = aData.mEmitterLifeFractions[aHolder.mEmitters.DataArrayIndexOf(&e)];
	float aEmitterPathPosition = FloatTrackEvaluate(e.mEmitterDef->mEmitterPath, aSystemTime, e.mTrackInterp[ParticleSystemTracks::TRACK_EMITTER_PATH]);
	float aMinX = FloatTrackEvaluate(e.mEmitterDef->mEmitterBoxX, aSystemTime, 0.0f);
	float aMaxX = FloatTrackEvaluate(e.mEmitterDef->mEmitterBoxX, aSystemTime, 1.0f);
	float aMinY = FloatTrackEvaluate(e.mEmitterDef->mEmitterBoxY, aSystemTime, 0.0f);
	float aMaxY = FloatTrackEvaluate(e.mEmitterDef->mEmitterBoxY, aSystemTime, 1.0f);
	float aDistanceX = aMaxX - aMinX;  // 路径矩形的横向宽度
	float aDistanceY = aMaxY - aMinY;  // 路径矩形的纵向高度
	float aPathPos = aEmitterPathPosition * (aDistanceY + aDistanceX + aDistanceY + aDistanceX);  // 发射点位于矩形边上的位置
	// ★ 注释规定：以矩形左上的顶点开始，按逆时针方向依次将矩形的四个顶点标记为 A、B、C、D，并标记发射点为 P
	// ★           如此，aPathPos 即为 P 点与 A 点在矩形路径上的距离。注意，游戏中取横向水平向右和纵向竖直向下为正方向。
	if (aPathPos < aDistanceY)  // 发射点落在矩形 AB 边（左边）上
	{
	aPosX = aMinX;  // 横坐标 = 矩形左端横坐标
	aPosY = aMinY + aPathPos;  // 纵坐标 = 矩形底端坐标 + |PA|
	}
	else if (aPathPos < aDistanceY + aDistanceX)  // 发射点落在矩形 BC 边（顶边）上
	{
	aPosX = aMinX + (aPathPos - aDistanceY);  // 横坐标 = 矩形左端横坐标 + |PB|
	aPosY = aMaxY;
	}
	else if (aPathPos < aDistanceY + aDistanceX + aDistanceY)  // 发射点落在矩形 CD 边（右边）上
	{
	aPosX = aMaxX;
	aPosY = aMaxY - (aPathPos - aDistanceY - aDistanceX);  // 纵坐标 = 矩形顶端纵坐标 - |PC|
	}
	else  // 发射点落在矩形 AD 边（底边）上
	{
	aPosX = aMaxX - (aPathPos - aDistanceY - aDistanceX - aDistanceY);  // 横坐标 = 矩形右端横坐标 - |PD|
	aPosY = aMinY;
	}
	},  // 2 EMITTER_BOX_PATH
	ParticleSpawnPosEmitterCircle,  // 3 EMITTER_CIRCLE_PATH（共体）
	ParticleSpawnPosEmitterCircle,  // 4 EMITTER_CIRCLE_EVEN_SPACING（共体）
	};
	static_assert(sizeof(kParticleSpawnPosCases) / sizeof(kParticleSpawnPosCases[0]) ==
	(int)EmitterType::EMITTER_CIRCLE_EVEN_SPACING + 1, "kParticleSpawnPosCases 槽位数须与 EmitterType 枚举同步");

	DataArray<TodParticle>& aDataArray = mParticleSystem->mParticleHolder->mParticles;
	if (aDataArray.mSize == aDataArray.mMaxSize)
	{
		TodTraceWithoutSpamming("Too many particles (emitter id=%u)\n", static_cast<unsigned>(mEmitterDef->mNameId));
		return nullptr;
	}

	TodParticle* aParticle = aDataArray.DataArrayAlloc();
	TOD_ASSERT(mEmitterDef->mParticleFields.count <= MAX_PARTICLE_FIELDS);
	for (int i = 0; i < mEmitterDef->mParticleFields.count; i++)
	{
		aParticle->mParticleFieldInterp[i][0] = RandomFloat(1.0f);  // 初始化每个粒子场的横向插值
		aParticle->mParticleFieldInterp[i][1] = RandomFloat(1.0f);  // 初始化每个粒子场的纵向插值
	}
	for (int i = 0; i < (int)ParticleTracks::NUM_PARTICLE_TRACKS; i++)
		aParticle->mParticleInterp[i] = RandomFloat(1.0f);  // 初始化每条通道的插值

	float aParticleDurationInterp = RandomFloat(1.0f);
	float aLaunchSpeedInterp = RandomFloat(1.0f);
	float aEmitterOffsetXInterp = RandomFloat(1.0f);
	float aEmitterOffsetYInterp = RandomFloat(1.0f);
	const auto& aHolder = *mParticleSystem->mParticleHolder;
	const auto& aData = *aHolder.mData;
	const double aSystemTime = aData.mEmitterLifeFractions[aHolder.mEmitters.DataArrayIndexOf(this)];
	aParticle->mParticleDurationNs = Sexy::TimeCounter::Encode(FloatTrackEvaluate(mEmitterDef->mParticleDuration, aSystemTime, aParticleDurationInterp));
	aParticle->mParticleDurationNs = std::max(ParticleSourceIntervalNs, aParticle->mParticleDurationNs);
	std::int64_t initialAgeNs = 0;
	aParticle->mParticleEmitter = this;
	aParticle->mRandomSeed = NextParticleRandom(mParticleSystem->mRandomState);
	if (TestBit(mEmitterDef->mParticleFlags, (int)ParticleFlags::PARTICLE_RANDOM_START_TIME))
		initialAgeNs = static_cast<std::int64_t>(ReanimClock::MulDiv(
			NextParticleRandom(mParticleSystem->mRandomState), aParticle->mParticleDurationNs,
			UINT64_C(1) << 32));
	aParticle->mBirthTimeNs = ReanimClock::Subtract(mParticleSystem->mLastUpdateTimeNs, initialAgeNs);
	RefreshParticleLifeFraction(*aParticle, mParticleSystem->mLastUpdateTimeNs);
	aParticle->mAnimationBirthTimeNs = mParticleSystem->mLastUpdateTimeNs;
	float aLaunchSpeed = FloatTrackEvaluate(mEmitterDef->mLaunchSpeed, aSystemTime, aLaunchSpeedInterp) * 0.01f;
	float aLaunchAngleInterp = RandomFloat(1.0f);

	float aLaunchAngle;
	if (mEmitterDef->mEmitterType == EmitterType::EMITTER_CIRCLE_PATH)
	{
		// 发射角度 = 根据路径定义计算的圆周上的基础角度 + 根据发射角度定义计算的额外偏移的角度
		aLaunchAngle = FloatTrackEvaluate(mEmitterDef->mEmitterPath, aSystemTime, mTrackInterp[ParticleSystemTracks::TRACK_EMITTER_PATH]) * 2 * PI;
		aLaunchAngle += DEG_TO_RAD(FloatTrackEvaluate(mEmitterDef->mLaunchAngle, aSystemTime, aLaunchAngleInterp));
	}
	else if (mEmitterDef->mEmitterType == EmitterType::EMITTER_CIRCLE_EVEN_SPACING)
		// 基础发射角度要使 theSpawnCount 个粒子平均布满圆周
		aLaunchAngle = 2 * PI * theIndex / theSpawnCount + DEG_TO_RAD(FloatTrackEvaluate(mEmitterDef->mLaunchAngle, aSystemTime, aLaunchAngleInterp));
	else if (FloatTrackIsConstantZero(mEmitterDef->mLaunchAngle))
		// 未定义的轨道，发射角度直接取 [0, 2π] 的随机值
		aLaunchAngle = RandomFloat((float)(2 * PI));
	else
		// 其他情况下，根据发射角度的定义值计算
		aLaunchAngle = DEG_TO_RAD(FloatTrackEvaluate(mEmitterDef->mLaunchAngle, aSystemTime, aLaunchAngleInterp));

	float aPosX, aPosY;
	// C 形态 -> 匿名 lambda 函数指针表：行序 = EmitterType 枚举序（0..4）。
	// 三种圆形发射器标签共体 → 具名（匿名 lambda 无法跨槽复用）；
	// aPosX/aPosY 经引用直写（分发点之后消费）；发射角经值参直传。
	// default（TOD_ASSERT）具名并被越界分支复用。
	const unsigned aEmitterKindIndex = static_cast<unsigned>(mEmitterDef->mEmitterType);
	if (aEmitterKindIndex <= static_cast<unsigned>(EmitterType::EMITTER_CIRCLE_EVEN_SPACING))
	{
		kParticleSpawnPosCases[aEmitterKindIndex](*this, aLaunchAngle, aPosX, aPosY);
	}
	else
	{
		ParticleSpawnPosDefault(*this, aLaunchAngle, aPosX, aPosY);
	}

	float aEmitterSkewXInterp = RandomFloat(1.0f);
	float aEmitterSkewYInterp = RandomFloat(1.0f);
	float aSkewX = FloatTrackEvaluate(mEmitterDef->mEmitterSkewX, aSystemTime, aEmitterSkewXInterp);
	float aSkewY = FloatTrackEvaluate(mEmitterDef->mEmitterSkewY, aSystemTime, aEmitterSkewYInterp);
	aParticle->mPosition.x = mSystemCenter.x + aPosX + aPosY * aSkewX;  // 横向（左右）倾斜的幅度受纵坐标影响
	aParticle->mPosition.y = mSystemCenter.y + aPosY + aPosX * aSkewY;  // 纵向（上下）倾斜的幅度受横坐标影响
	aParticle->mVelocity.x = sin(aLaunchAngle) * aLaunchSpeed;
	aParticle->mVelocity.y = cos(aLaunchAngle) * aLaunchSpeed;
	aParticle->mPosition.x += FloatTrackEvaluate(mEmitterDef->mEmitterOffsetX, aSystemTime, aEmitterOffsetXInterp);  // 横坐标加上横向偏移值
	aParticle->mPosition.y += FloatTrackEvaluate(mEmitterDef->mEmitterOffsetY, aSystemTime, aEmitterOffsetYInterp);  // 纵坐标加上纵向偏移值
	
	aParticle->mAnimationSampleIntegral = 0;
	if (mEmitterDef->mAnimated || FloatTrackIsSet(mEmitterDef->mAnimationRate))
		aParticle->mImageFrame = 0;  // 如果定义了粒子动态或动画速率，则粒子的当前帧将在后续根据粒子时间值或动画循环率实时计算，此处先初始化为 0
	else
		aParticle->mImageFrame = RandomInt(mEmitterDef->mImageFrames);  // 对于帧固定的粒子，在贴图的所有帧中随机取得一帧，后续一般不再变化

	if (TestBit(mEmitterDef->mParticleFlags, (int)ParticleFlags::PARTICLE_RANDOM_LAUNCH_SPIN))
		aParticle->mSpinPosition = RandomFloat((float)(2 * PI));  // 在 [0, 2π] 之间随机取得一个初始旋转角度
	else if (TestBit(mEmitterDef->mParticleFlags, (int)ParticleFlags::PARTICLE_ALIGN_LAUNCH_SPIN))
		aParticle->mSpinPosition = aLaunchAngle;  // 粒子旋转角度对齐发射角度
	else
		aParticle->mSpinPosition = 0.0f;  // 默认无初始旋转
	aParticle->mSpinVelocity = 0.0f;
	aParticle->mCrossFadeDurationNs = 0;
	aParticle->mCrossFadeParticleID = ParticleID::PARTICLEID_NULL;

	if (!ParticleRosterAdd(aParticle))
	{
		aDataArray.DataArrayFree(aParticle);  // 名册满回滚 = 池满同款（宁可丢不崩）
		return nullptr;
	}
	mParticlesSpawned++;
	// Initialize authored POSITION/SHAKE/angle offsets at birth without giving
	// the newly created particle a hidden physics or lifetime update.
	UpdateParticle(aParticle, 0, mParticleSystem->mLastUpdateTimeNs, -1);
	return aParticle;
}

namespace
{
// TodParticleEmitter::UpdateParticleField 物理场分发表：表内匿名 lambda =
// 原 case 体逐句迁移（生成器 + 逆变换 token 对拍 0 差异）；INVALID 槽空
// 操作；default 具名并被缺槽/越界复用。
using ParticleFieldCaseFn = void (*)(TodParticleEmitter&, TodParticle*, ParticleField*, double, float, float, float, float, std::uint64_t, std::int64_t);

// 原 default（无 case 的 1 型与表外哨兵共用）
void ParticleFieldDefault(TodParticleEmitter& e, TodParticle* theParticle, ParticleField* theParticleField, double theParticleTimeValue, float x, float y, float aInterpX, float aInterpY, std::uint64_t deltaNs, std::int64_t previousAgeNs)
{
    		TOD_ASSERT(0);
}

// 匿名 lambda 函数指针表，行序 = ParticleFieldType 枚举值序（0..11）。
constexpr ParticleFieldCaseFn kParticleFieldCases[(int)ParticleFieldType::PARTICLE_FIELD_COUNT] = {
    // 原 case FIELD_INVALID
    [](TodParticleEmitter& e, TodParticle* theParticle, ParticleField* theParticleField, double theParticleTimeValue, float x, float y, float aInterpX, float aInterpY, std::uint64_t deltaNs, std::int64_t previousAgeNs)
    {

    },  // 0 FIELD_INVALID
    // 原 case FIELD_FRICTION
    [](TodParticleEmitter& e, TodParticle* theParticle, ParticleField* theParticleField, double theParticleTimeValue, float x, float y, float aInterpX, float aInterpY, std::uint64_t deltaNs, std::int64_t previousAgeNs)
    {
        // 原素材摩擦系数按纳秒步长换算，每次主更新直接积分；大步长钳零，避免速度反向。
        theParticle->mVelocity.x *= static_cast<float>(std::max(0.0, 1.0 - x * static_cast<double>(deltaNs) / ParticleSourceIntervalNs));
        theParticle->mVelocity.y *= static_cast<float>(std::max(0.0, 1.0 - y * static_cast<double>(deltaNs) / ParticleSourceIntervalNs));
    },  // 1 FIELD_FRICTION
    // 原 case FIELD_ACCELERATION
    [](TodParticleEmitter& e, TodParticle* theParticle, ParticleField* theParticleField, double theParticleTimeValue, float x, float y, float aInterpX, float aInterpY, std::uint64_t deltaNs, std::int64_t previousAgeNs)
    {
    // 加速度场
        theParticle->mVelocity.x += static_cast<float>(0.01f * x * static_cast<double>(deltaNs) / ParticleSourceIntervalNs);
        theParticle->mVelocity.y += static_cast<float>(0.01f * y * static_cast<double>(deltaNs) / ParticleSourceIntervalNs);
    },  // 2 FIELD_ACCELERATION
    // 原 case FIELD_ATTRACTOR
    [](TodParticleEmitter& e, TodParticle* theParticle, ParticleField* theParticleField, double theParticleTimeValue, float x, float y, float aInterpX, float aInterpY, std::uint64_t deltaNs, std::int64_t previousAgeNs)
    {
    // 弹性力场
    		float aDiffX = x - (theParticle->mPosition.x - e.mSystemCenter.x);
    		float aDiffY = y - (theParticle->mPosition.y - e.mSystemCenter.y);
    		// 加速度的方向始终从粒子所在位置指向“标准位置”
        theParticle->mVelocity.x += static_cast<float>(0.01f * aDiffX * static_cast<double>(deltaNs) / ParticleSourceIntervalNs);
        theParticle->mVelocity.y += static_cast<float>(0.01f * aDiffY * static_cast<double>(deltaNs) / ParticleSourceIntervalNs);
    },  // 3 FIELD_ATTRACTOR
    // 原 case FIELD_MAX_VELOCITY
    [](TodParticleEmitter& e, TodParticle* theParticle, ParticleField* theParticleField, double theParticleTimeValue, float x, float y, float aInterpX, float aInterpY, std::uint64_t deltaNs, std::int64_t previousAgeNs)
    {
    // 限速场
    		theParticle->mVelocity.x = ClampFloat(theParticle->mVelocity.x, -x, x);
    		theParticle->mVelocity.y = ClampFloat(theParticle->mVelocity.y, -y, y);
    },  // 4 FIELD_MAX_VELOCITY
    // 原 case FIELD_VELOCITY
    [](TodParticleEmitter& e, TodParticle* theParticle, ParticleField* theParticleField, double theParticleTimeValue, float x, float y, float aInterpX, float aInterpY, std::uint64_t deltaNs, std::int64_t previousAgeNs)
    {
    // 匀速场
        theParticle->mPosition.x += static_cast<float>(0.01 * x * static_cast<double>(deltaNs) / ParticleSourceIntervalNs);
        theParticle->mPosition.y += static_cast<float>(0.01 * y * static_cast<double>(deltaNs) / ParticleSourceIntervalNs);
    },  // 5 FIELD_VELOCITY
    // 原 case FIELD_POSITION
    [](TodParticleEmitter& e, TodParticle* theParticle, ParticleField* theParticleField, double theParticleTimeValue, float x, float y, float aInterpX, float aInterpY, std::uint64_t deltaNs, std::int64_t previousAgeNs)
    {
    // 定位场
		float aLastX = FloatTrackEvaluateFromLastTime(theParticleField->mX, LifeFraction(previousAgeNs, theParticle->mParticleDurationNs), aInterpX);
		float aLastY = FloatTrackEvaluateFromLastTime(theParticleField->mY, LifeFraction(previousAgeNs, theParticle->mParticleDurationNs), aInterpY);
    		theParticle->mPosition.x += x - aLastX;
    		theParticle->mPosition.y += y - aLastY;
    },  // 6 FIELD_POSITION
    &ParticleFieldDefault,  // 7 FIELD_SYSTEM_POSITION（原 default）
    // 原 case FIELD_GROUND_CONSTRAINT
    [](TodParticleEmitter& e, TodParticle* theParticle, ParticleField* theParticleField, double theParticleTimeValue, float x, float y, float aInterpX, float aInterpY, std::uint64_t deltaNs, std::int64_t previousAgeNs)
    {
		if (deltaNs > 0 && theParticle->mPosition.y > e.mSystemCenter.y + y)
    		{
    			theParticle->mPosition.y = e.mSystemCenter.y + y;  // 将坐标重置至地面
    			float aCollisionReflect = FloatTrackEvaluate(
    				e.mEmitterDef->mCollisionReflect, theParticleTimeValue, theParticle->mParticleInterp[ParticleTracks::TRACK_PARTICLE_COLLISION_REFLECT]
    			);
    			float aCollisionSpin = FloatTrackEvaluate(
    				e.mEmitterDef->mCollisionSpin, theParticleTimeValue, theParticle->mParticleInterp[ParticleTracks::TRACK_PARTICLE_COLLISION_SPIN]
    			) / 1000.0f;
    			theParticle->mSpinVelocity = theParticle->mVelocity.y * aCollisionSpin;
    			theParticle->mVelocity.x *= aCollisionReflect;
    			theParticle->mVelocity.y *= -aCollisionReflect;
    		}
    },  // 8 FIELD_GROUND_CONSTRAINT
    // 原 case FIELD_SHAKE
    [](TodParticleEmitter& e, TodParticle* theParticle, ParticleField* theParticleField, double theParticleTimeValue, float x, float y, float aInterpX, float aInterpY, std::uint64_t deltaNs, std::int64_t previousAgeNs)
    {
    // 震动
		float aLastX = FloatTrackEvaluateFromLastTime(theParticleField->mX, LifeFraction(previousAgeNs, theParticle->mParticleDurationNs), aInterpX);
		float aLastY = FloatTrackEvaluateFromLastTime(theParticleField->mY, LifeFraction(previousAgeNs, theParticle->mParticleDurationNs), aInterpY);
    		// 先恢复上一次震动效果的影响
		theParticle->mPosition.x -= aLastX * ShakeSample(theParticle->mRandomSeed, previousAgeNs, 0);
		theParticle->mPosition.y -= aLastY * ShakeSample(theParticle->mRandomSeed, previousAgeNs, 1);
		theParticle->mPosition.x += x * ShakeSample(theParticle->mRandomSeed, ParticleLifeAgeAt(e.mParticleSystem->mLastUpdateTimeNs, theParticle->mBirthTimeNs, theParticle->mParticleDurationNs), 0);
		theParticle->mPosition.y += y * ShakeSample(theParticle->mRandomSeed, ParticleLifeAgeAt(e.mParticleSystem->mLastUpdateTimeNs, theParticle->mBirthTimeNs, theParticle->mParticleDurationNs), 1);
    },  // 9 FIELD_SHAKE
    // 原 case FIELD_CIRCLE
    [](TodParticleEmitter& e, TodParticle* theParticle, ParticleField* theParticleField, double theParticleTimeValue, float x, float y, float aInterpX, float aInterpY, std::uint64_t deltaNs, std::int64_t previousAgeNs)
    {
    // 圆周
    		SexyVector2 aToCenter = theParticle->mPosition - e.mSystemCenter;
    		SexyVector2 aMotion = aToCenter.Perp().Normalize();  // 标准化的法向量
    		float aRadius = aToCenter.Magnitude();
        aMotion *= static_cast<float>(0.01 * (x + aRadius * y) * static_cast<double>(deltaNs) / ParticleSourceIntervalNs);
    		theParticle->mPosition += aMotion;
    },  // 10 FIELD_CIRCLE
    // 原 case FIELD_AWAY
    [](TodParticleEmitter& e, TodParticle* theParticle, ParticleField* theParticleField, double theParticleTimeValue, float x, float y, float aInterpX, float aInterpY, std::uint64_t deltaNs, std::int64_t previousAgeNs)
    {
    // 远离
    		SexyVector2 aToCenter = theParticle->mPosition - e.mSystemCenter;
    		SexyVector2 aMotion = aToCenter.Normalize();  // 标准化的方向向量
    		float aRadius = aToCenter.Magnitude();
        aMotion *= static_cast<float>(0.01 * (x + aRadius * y) * static_cast<double>(deltaNs) / ParticleSourceIntervalNs);
    		theParticle->mPosition += aMotion;
    },  // 11 FIELD_AWAY
};
static_assert(sizeof(kParticleFieldCases) / sizeof(kParticleFieldCases[0]) ==
    (int)ParticleFieldType::PARTICLE_FIELD_COUNT, "kParticleFieldCases 槽位数须与 ParticleFieldType 枚举同步");
}  // namespace

//0x516820
void TodParticleEmitter::UpdateParticleField(TodParticle* theParticle, ParticleField* theParticleField, double theParticleTimeValue, int theFieldIndex, std::uint64_t deltaNs, std::int64_t previousAgeNs)
{
	TOD_ASSERT(theFieldIndex < MAX_PARTICLE_FIELDS);
	float aInterpX = theParticle->mParticleFieldInterp[theFieldIndex][0];
	float aInterpY = theParticle->mParticleFieldInterp[theFieldIndex][1];
	float x = FloatTrackEvaluate(theParticleField->mX, theParticleTimeValue, aInterpX);
	float y = FloatTrackEvaluate(theParticleField->mY, theParticleTimeValue, aInterpY);

	// C 形态 -> 匿名 lambda 函数指针表：行序 = ParticleFieldType 枚举序
	// （0..11；SYSTEM_POSITION 为 SampleSystemPosition 专属 → 缺槽走 default）。
	// 场值 x/y 与双插值经值参传入（体只读）；发射器成员经 e 直写。
	// default（TOD_ASSERT）具名并被缺槽与越界分支复用。
	const unsigned aFieldTypeIndex = static_cast<unsigned>(theParticleField->mFieldType);
	if (aFieldTypeIndex < static_cast<unsigned>(ParticleFieldType::PARTICLE_FIELD_COUNT))
	{
		kParticleFieldCases[aFieldTypeIndex](*this, theParticle, theParticleField, theParticleTimeValue, x, y, aInterpX, aInterpY, deltaNs, previousAgeNs);
	}
	else
	{
		ParticleFieldDefault(*this, theParticle, theParticleField, theParticleTimeValue, x, y, aInterpX, aInterpY, deltaNs, previousAgeNs);
	}

}

//0x516D70
void TodParticleEmitter::SampleSystemPosition(double theTimeValue)
{
	// 原版增量（2026-10-27 恢复）：位置场曲线按当前时间采样，与上次采样
	// 求差加进系统中心（v2.0 UpdateSystemPositionField 语义）。绝对值+
	// 双精度锚点重建已退役；mSystemPositionOrigin* 转用作"上次采样值"
	//（字段沿用，存档布局不变），mSystemPositionBound 即"已取基线"标志。
	if (mEmitterDef->mSystemFields.count == 0)
		return;
	double x = 0, y = 0;
	for (int i = 0; i < mEmitterDef->mSystemFields.count; ++i)
	{
		TOD_ASSERT(i < MAX_PARTICLE_FIELDS);
		const auto& field = mEmitterDef->mSystemFields.Fields[i];
		TOD_ASSERT(field.mFieldType == ParticleFieldType::FIELD_SYSTEM_POSITION);
		x += FloatTrackEvaluate(field.mX, theTimeValue, mSystemFieldInterp[i][0]);
		y += FloatTrackEvaluate(field.mY, theTimeValue, mSystemFieldInterp[i][1]);
	}
	if (!mSystemPositionBound)
	{
		mSystemPositionOriginX = x;
		mSystemPositionOriginY = y;
		mSystemPositionBound = 1;
		return;
	}
	mSystemCenter.x += static_cast<float>(x - mSystemPositionOriginX);
	mSystemCenter.y += static_cast<float>(y - mSystemPositionOriginY);
	mSystemPositionOriginX = x;
	mSystemPositionOriginY = y;
}


// IDT3 resolves the first target definition at compile time.
bool TodParticleEmitter::CrossFadeParticleOnDuration(TodParticle* theParticle)
{
	const int aIndex = mEmitterDef->mOnDurationEmitterIndex;
	if (aIndex >= 0 && aIndex < mParticleSystem->mParticleDef->mEmitterDefCount)
	{
		return CrossFadeParticleToDef(theParticle, &mParticleSystem->mParticleDef->mEmitterDefs[aIndex]);
	}
	return false;
}

// 按发射器 def 交叉淡入：分配目标发射器实例、入系统表并转移粒子。
bool TodParticleEmitter::CrossFadeParticleToDef(TodParticle* theParticle, TodEmitterDefinition* theToEmitterDef)
{
	if (mParticleSystem->mParticleHolder->mEmitters.mSize == mParticleSystem->mParticleHolder->mEmitters.mMaxSize)
	{
		TodTraceAndLog("Too many emitters to cross fade\n");
		return false;
	}

	TodParticleEmitter* aEmitter = mParticleSystem->mParticleHolder->mEmitters.DataArrayAlloc();
	aEmitter->TodEmitterInitialize(mSystemCenter.x, mSystemCenter.y, mParticleSystem, theToEmitterDef);
	ParticleEmitterID aEmitterID = (ParticleEmitterID)mParticleSystem->mParticleHolder->mEmitters.DataArrayGetID(aEmitter);
	if (!mParticleSystem->EmitterRosterAdd(aEmitter))
	{
		mParticleSystem->mParticleHolder->mEmitters.DataArrayFree(aEmitter);  // 名册满即整体回滚（防孤儿发射器）
		return false;
	}
	return CrossFadeParticle(theParticle, aEmitter);
}

//0x516F00
bool TodParticleEmitter::UpdateParticle(TodParticle* theParticle, std::uint64_t deltaNs, std::int64_t nowNs)
{
	return UpdateParticle(theParticle, deltaNs, nowNs, ParticleLifeAgeAt(ReanimClock::Subtract(nowNs, static_cast<std::int64_t>(deltaNs)), theParticle->mBirthTimeNs, theParticle->mParticleDurationNs));
}

bool TodParticleEmitter::UpdateParticle(TodParticle* theParticle, std::uint64_t deltaNs, std::int64_t nowNs, std::int64_t previousAgeNs)
{
	if (deltaNs > INT64_MAX) throw std::overflow_error("particle lifetime interval exceeds clock range");
	RefreshParticleLifeFraction(*theParticle, nowNs);
	auto ageNs = ReanimClock::Subtract(nowNs, theParticle->mBirthTimeNs);
	if (ageNs >= theParticle->mParticleDurationNs)  // 粒子的生命周期结束时
	{
		if (TestBit(mEmitterDef->mParticleFlags, (int)ParticleFlags::PARTICLE_PARTICLE_LOOPS))  // 判断粒子是否循环
			{
			ageNs %= theParticle->mParticleDurationNs;
			theParticle->mBirthTimeNs = ReanimClock::Subtract(nowNs, ageNs);
			RefreshParticleLifeFraction(*theParticle, nowNs);
		}
		else if (theParticle->mCrossFadeDurationNs > 0)  // 判断粒子是否处于交叉混合过程中
			ageNs = theParticle->mParticleDurationNs;
		else if (mEmitterDef->mOnDurationEmitterIndex < 0 || !CrossFadeParticleOnDuration(theParticle))  // 尝试进行交叉混合（IDT3 下标；无链接即无淡入）
			return false;
	}
	if (ageNs >= theParticle->mParticleDurationNs &&
		!TestBit(mEmitterDef->mParticleFlags, (int)ParticleFlags::PARTICLE_PARTICLE_LOOPS))
		ageNs = theParticle->mParticleDurationNs;
	if (theParticle->mCrossFadeParticleID != ParticleID::PARTICLEID_NULL &&
		mParticleSystem->mParticleHolder->mParticles.DataArrayTryToGet(theParticle->mCrossFadeParticleID) == nullptr)
		return false;  // 当粒子不存在交叉混合时，可以删除粒子

	const auto& aHolder = *mParticleSystem->mParticleHolder;
	const auto& aData = *aHolder.mData;
	const auto aParticleTime = aData.mParticleLifeFractions[aHolder.mParticles.DataArrayIndexOf(theParticle)];
	const int aParticleFieldCount = mEmitterDef->mParticleFields.count;  // 装载后 def 只读，场数外提免逐场重载
	// 所有力场、位移和旋转统一按当前逻辑步长逐帧累加。
	for (int i = 0; i < aParticleFieldCount; ++i)
		UpdateParticleField(theParticle, &mEmitterDef->mParticleFields.Fields[i], aParticleTime, i, deltaNs, previousAgeNs);
	const float aSpinSpeed = FloatTrackEvaluate(mEmitterDef->mParticleSpinSpeed, aParticleTime,
		theParticle->mParticleInterp[ParticleTracks::TRACK_PARTICLE_SPIN_SPEED]) * 0.01;
	const float aSpinAngle = FloatTrackEvaluate(mEmitterDef->mParticleSpinAngle, aParticleTime,
		theParticle->mParticleInterp[ParticleTracks::TRACK_PARTICLE_SPIN_ANGLE]);
	const auto previousSpinAngle = [&] {
		return FloatTrackEvaluateFromLastTime(mEmitterDef->mParticleSpinAngle,
			LifeFraction(previousAgeNs, theParticle->mParticleDurationNs), theParticle->mParticleInterp[ParticleTracks::TRACK_PARTICLE_SPIN_ANGLE]);
	};
	theParticle->mPosition += theParticle->mVelocity * static_cast<float>(static_cast<double>(deltaNs) / ParticleSourceIntervalNs);
	theParticle->mSpinPosition += static_cast<float>(DEG_TO_RAD(aSpinSpeed * static_cast<double>(deltaNs) / ParticleSourceIntervalNs +
		aSpinAngle - previousSpinAngle()) + theParticle->mSpinVelocity * static_cast<double>(deltaNs) / ParticleSourceIntervalNs);

	if (FloatTrackIsSet(mEmitterDef->mAnimationRate))  // 如果定义了动画速率
	{
		if (!TodParticleConstantAnimationRate(mEmitterDef->mAnimationRate))
		{
			const auto increment = SampleClockIncrement(FloatTrackEvaluate(mEmitterDef->mAnimationRate,
				aParticleTime, theParticle->mParticleInterp[ParticleTracks::TRACK_PARTICLE_ANIMATION_RATE]),
				deltaNs) % ParticleCycle;
			theParticle->mAnimationSampleIntegral = (theParticle->mAnimationSampleIntegral + increment) % ParticleCycle;
			if (theParticle->mAnimationSampleIntegral < 0) theParticle->mAnimationSampleIntegral += ParticleCycle;
		}
	}

	return true;
}

//0x517160
void TodParticleEmitter::UpdateSpawning(std::uint64_t deltaNs)
{
	TodParticleEmitter* aCrossFadeEmitter = mParticleSystem->mParticleHolder->mEmitters.DataArrayTryToGet((unsigned int)mCrossFadeEmitterID);
	TodParticleEmitter* aSpawningEmitter = !aCrossFadeEmitter ? this : aCrossFadeEmitter;  // 各项数据的计算均以此“主发射器”为准
	const auto& aHolder = *aSpawningEmitter->mParticleSystem->mParticleHolder;
	const auto& aData = *aHolder.mData;
	const double aSystemTime = aData.mEmitterLifeFractions[aHolder.mEmitters.DataArrayIndexOf(aSpawningEmitter)];
	const auto systemEvaluate = [aSpawningEmitter, aSystemTime](const FloatParameterTrack& track, ParticleSystemTracks index) {
		return FloatTrackEvaluate(track, aSystemTime, aSpawningEmitter->mTrackInterp[int(index)]);
	};
	const auto rate = systemEvaluate(
		aSpawningEmitter->mEmitterDef->mSpawnRate, ParticleSystemTracks::TRACK_SPAWN_RATE);
	// 发射计数累加制（2026-10-27 恢复）：当前速率采样 × 本步时长，定点
	// 余数累加（mSpawnRemainder 即"上次发射以来的相位"）。恒定速率的
	// 解析积分（ConstantRateIntegral 对 elapsed 求曲线原函数）已移除。
	std::int64_t whole, fraction;
	{
		const auto increment = SampleClockIncrement(rate, deltaNs);
		whole = increment / ParticleCycle;
		fraction = increment % ParticleCycle;
	}
	// Split before adding so even a valid near-int64 sampled rate cannot overflow.
	const auto fractional = mSpawnRemainder + fraction;
	auto count = ReanimClock::Add(whole, fractional / ParticleCycle);
	mSpawnRemainder = fractional % ParticleCycle;
	// A total-time phase can wrap backwards while its whole count increases.
	// Normalize the pair together; C++ truncation alone would emit too early.
	if (count > 0 && mSpawnRemainder < 0)
	{
		--count;
		mSpawnRemainder += ParticleCycle;
	}
	else if (count < 0 && mSpawnRemainder > 0)
	{
		++count;
		mSpawnRemainder -= ParticleCycle;
	}
	int aSpawnCount = static_cast<int>(std::clamp<std::int64_t>(count, 0, MAX_PARTICLES_SIZE));

	int aSpawnMinActive = (int)systemEvaluate(aSpawningEmitter->mEmitterDef->mSpawnMinActive, ParticleSystemTracks::TRACK_SPAWN_MIN_ACTIVE);
	if (aSpawnMinActive >= 0 && aSpawnCount < aSpawnMinActive - ParticleRosterCount())
		aSpawnCount = aSpawnMinActive - ParticleRosterCount();  // 至少确保将粒子数量增加至 aSpawnMinActive 个
	int aSpawnMaxActive = (int)systemEvaluate(aSpawningEmitter->mEmitterDef->mSpawnMaxActive, ParticleSystemTracks::TRACK_SPAWN_MAX_ACTIVE);
	if (aSpawnMaxActive >= 0 && aSpawnCount > aSpawnMaxActive - ParticleRosterCount())
		aSpawnCount = aSpawnMaxActive - ParticleRosterCount();  // 至多保证粒子数量不会超过 aSpawnMaxActive 个
	if (FloatTrackIsSet(aSpawningEmitter->mEmitterDef->mSpawnMaxLaunched))
	{
		int aSpawnMaxLaunched = systemEvaluate(aSpawningEmitter->mEmitterDef->mSpawnMaxLaunched, ParticleSystemTracks::TRACK_SPAWN_MAX_LAUNCHED);
		if (aSpawnCount > aSpawnMaxLaunched - mParticlesSpawned)
			aSpawnCount = aSpawnMaxLaunched - mParticlesSpawned;  // 确保发射数量不超过发射器总共能发射的最大数量
	}

	for (int i = 0; i < aSpawnCount; i++)
	{
		TodParticle* aParticle = SpawnParticle(i, aSpawnCount);
		if (aCrossFadeEmitter != nullptr && aParticle != nullptr)
			CrossFadeParticle(aParticle, aCrossFadeEmitter);
	}
}

//0x5172C0
void TodParticleEmitter::DeleteNonCrossFading()
{
	// 尾存倒序遍历（=原头→尾序）；下标直传 DeleteParticleAt（免逐粒 Find 扫）
	for (int i = ParticleRosterCount() - 1; i >= 0; i--)
	{
		TodParticle* aParticle = ParticleRosterParticleAt(i);
		if (aParticle->mCrossFadeDurationNs <= 0)  // 当粒子不处于交叉混合状态，则删除该粒子
			DeleteParticleAt(i, aParticle);
	}
}

//0x517300
void TodParticleEmitter::DeleteAll()
{
	while (ParticleRosterCount() != 0)
	{
		TodParticle* aParticle = ParticleRosterParticleAt(ParticleRosterCount() - 1);  // 尾=最新=原 RemoveHead
		ParticleRosterRemoveAt(ParticleRosterCount() - 1);
		mParticleSystem->mParticleHolder->mParticles.DataArrayFree(aParticle);
	}
}

//0x517370
// GOTY @Patoke: 0x521A20
void TodParticleSystem::Update(std::uint64_t deltaNs, std::int64_t nowNs)
{
	if (deltaNs > INT64_MAX) throw std::overflow_error("particle update exceeds clock range");
	// A frozen system follows clock changes by translating origins only.
	// Active lifetimes are sampled directly from nowNs, independently of deltaNs.
	const auto shift = mDontUpdate || deltaNs == 0
		? ReanimClock::Subtract(nowNs, mLastUpdateTimeNs) : 0;
	const auto previousNowNs = mLastUpdateTimeNs;
	mLastUpdateTimeNs = nowNs;
	if (shift != 0)
		for (int i = 0; i < EmitterRosterCount(); ++i)
		{
			auto* emitter = EmitterRosterEmitterAt(i);
			emitter->mBirthTimeNs = ReanimClock::Add(emitter->mBirthTimeNs, shift);
			if (emitter->mCrossFadeEndTimeNs != 0)
				emitter->mCrossFadeEndTimeNs = ReanimClock::Add(emitter->mCrossFadeEndTimeNs, shift);
			for (int j = 0; j < emitter->ParticleRosterCount(); ++j)
			{
				auto* particle = emitter->ParticleRosterParticleAt(j);
				particle->mBirthTimeNs = ReanimClock::Add(particle->mBirthTimeNs, shift);
				particle->mAnimationBirthTimeNs = ReanimClock::Add(particle->mAnimationBirthTimeNs, shift);
			}
		}
	if (mDontUpdate || deltaNs == 0) return;
	// Publish emitter ratios before any emitter samples a crossfade partner.
	for (int i = 0; i < EmitterRosterCount(); ++i)
		RefreshParticleLifeFraction(*EmitterRosterEmitterAt(i), nowNs);
	bool aEmitterAlive = false;
	const int aEmitterCount = EmitterRosterCount();
	// Capture identities before any emitter can create a crossfade target in
	// another existing emitter. New births receive their first step next time.
	std::array<ParticleID, PARTICLE_POOL_CAPACITY> particles;
	std::array<int, PARTICLE_POOL_CAPACITY + 1> particleOffsets;
	int particleCount = 0;
	for (int i = 0; i < aEmitterCount; ++i)
	{
		particleOffsets[i] = particleCount;
		const auto* emitter = EmitterRosterEmitterAt(i);
		for (int p = 0; p < emitter->ParticleRosterCount(); ++p)
			particles[particleCount++] = emitter->ParticleRosterIDAt(p);
	}
	particleOffsets[aEmitterCount] = particleCount;
	for (int i = 0; i < aEmitterCount; i++)
	{
		TodParticleEmitter* emitter = EmitterRosterEmitterAt(i);
		emitter->Update(deltaNs, nowNs, previousNowNs, std::span<const ParticleID>(particles.data() + particleOffsets[i],
			particleOffsets[i + 1] - particleOffsets[i]));
		if ((FloatTrackIsSet(emitter->mEmitterDef->mCrossFadeDuration) && emitter->ParticleRosterCount() > 0) || !emitter->mDead)
			aEmitterAlive = true;
	}
	if (!aEmitterAlive) mDead = true;
}

//0x5173E0
bool TodParticleEmitter::CrossFadeParticle(TodParticle* theParticle, TodParticleEmitter* theToEmitter)
{
	const auto& aHolder = *mParticleSystem->mParticleHolder;
	const auto& aData = *aHolder.mData;
	if (theParticle->mCrossFadeDurationNs > 0)  // 粒子已处于交叉混合的过程中
	{
		TodTrace("We don't support cross fading more than one at a time\n");
		return false;
	}
	if (!FloatTrackIsSet(theToEmitter->mEmitterDef->mCrossFadeDuration))  // 目标发射器未设定交叉混合时长轨道
	{
		TodTrace("Can't cross fade to emitter that doesn't have CrossFadeDuration");
		return false;
	}
	TOD_ASSERT(theToEmitter != this);  // 不能交叉混合至自身

	TodParticle* aToParticle = theToEmitter->SpawnParticle(0, 1);
	if (aToParticle == nullptr)
		return false;
	if (mCrossFadeEndTimeNs > mParticleSystem->mLastUpdateTimeNs)  // Existing fade uses its remaining duration.
		theParticle->mCrossFadeDurationNs = std::max<std::int64_t>(0, ReanimClock::Subtract(mCrossFadeEndTimeNs, mParticleSystem->mLastUpdateTimeNs));  // 源粒子的交叉混合的时长即为源发射器交叉混合的剩余时长
	else
	{
		float aCrossFadeDurationInterp = RandomFloat(1.0f);
		theParticle->mCrossFadeDurationNs = Sexy::TimeCounter::Encode(FloatTrackEvaluate(theToEmitter->mEmitterDef->mCrossFadeDuration,
			aData.mEmitterLifeFractions[aHolder.mEmitters.DataArrayIndexOf(this)], aCrossFadeDurationInterp));
		theParticle->mCrossFadeDurationNs = std::max(ParticleSourceIntervalNs, theParticle->mCrossFadeDurationNs);
	}
	if (!FloatTrackIsSet(theToEmitter->mEmitterDef->mParticleDuration))  // 如果目标发射器未定义粒子持续时间
	{
		aToParticle->mParticleDurationNs = theParticle->mCrossFadeDurationNs;  // 目标粒子的持续时间等于交叉混合的时间
		RefreshParticleLifeFraction(*aToParticle, mParticleSystem->mLastUpdateTimeNs);
	}
	aToParticle->mCrossFadeParticleID = (ParticleID)mParticleSystem->mParticleHolder->mParticles.DataArrayGetID(theParticle);  // 赋值交叉混合来源的粒子编号
	return true;
}

//0x517490
void TodParticleEmitter::DeleteParticle(TodParticle* theParticle)
{
	DeleteParticleAt(ParticleRosterFind(theParticle), theParticle);  // 线性找槽位（交叉源级联等下标未知方）
}

// 下标化删除核心：调用方（更新循环/DeleteNonCrossFading）已知名册下标直入，
// 省 Find 的槽位线性探测。交叉源递归删在移除**之前**发生、且可能经
// A→B→A 链删掉本行更旧条目使下标漂移——递归后槽位一次校验（一载一比），
// 漂移（罕见）才回退 Find。
void TodParticleEmitter::DeleteParticleAt(int theRosterIndex, TodParticle* theParticle)
{
	TodParticle* aCrossFadeParticle = mParticleSystem->mParticleHolder->mParticles.DataArrayTryToGet((unsigned int)theParticle->mCrossFadeParticleID);
	if (aCrossFadeParticle != nullptr)
	{
		aCrossFadeParticle->mParticleEmitter->DeleteParticle(aCrossFadeParticle);  // 同时删除交叉混合的源粒子
		theParticle->mCrossFadeParticleID = ParticleID::PARTICLEID_NULL;
	}

	if (theRosterIndex < 0 || theRosterIndex >= ParticleRosterCount() ||
		ParticleRosterParticleAt(theRosterIndex) != theParticle)  // 级联漂移校验
		theRosterIndex = ParticleRosterFind(theParticle);
	if (theRosterIndex >= 0)
		ParticleRosterRemoveAt(theRosterIndex);
	mParticleSystem->mParticleHolder->mParticles.DataArrayFree(theParticle);
}

//0x517550
void TodParticleEmitter::Update(std::uint64_t deltaNs, std::int64_t nowNs)
{
	const auto previousNowNs = mParticleSystem->mLastUpdateTimeNs;
	mParticleSystem->mLastUpdateTimeNs = nowNs;
	RefreshParticleLifeFraction(*this, nowNs);
	std::array<ParticleID, PARTICLE_POOL_CAPACITY> particles;
	const auto count = ParticleRosterCount();
	for (int i = 0; i < count; ++i) particles[i] = ParticleRosterIDAt(i);
	Update(deltaNs, nowNs, previousNowNs, std::span<const ParticleID>(particles.data(), count));
}

void TodParticleEmitter::Update(std::uint64_t deltaNs, std::int64_t nowNs, std::int64_t previousNowNs, std::span<const ParticleID> particles)
{
	const auto& aHolder = *mParticleSystem->mParticleHolder;
	const auto& aData = *aHolder.mData;
	if (mDead || deltaNs == 0)
		return;

	if (deltaNs > INT64_MAX)
		throw std::overflow_error("particle emitter lifetime exceeds clock range");
	const auto previousAgeNs = ParticleLifeAgeAt(previousNowNs, mBirthTimeNs, mSystemDurationNs);
	const bool loops = TestBit(mEmitterDef->mParticleFlags, (unsigned int)ParticleFlags::PARTICLE_SYSTEM_LOOPS);
	auto spawnDelta = loops ? deltaNs : std::min<std::uint64_t>(deltaNs,
		previousAgeNs >= mSystemDurationNs ? 0 : ReanimClock::Subtract(mSystemDurationNs, previousAgeNs));
	if (mCrossFadeEmitterID != ParticleEmitterID::PARTICLEEMITTERID_NULL)
		spawnDelta = std::min<std::uint64_t>(spawnDelta,
			std::max<std::int64_t>(0, ReanimClock::Subtract(mCrossFadeEndTimeNs, previousNowNs)));
	auto ageNs = ReanimClock::Subtract(nowNs, mBirthTimeNs);
	bool aDie = false;
	if (ageNs >= mSystemDurationNs)  // 发射器的生命周期结束时
	{
		if (loops)
			{
			ageNs %= mSystemDurationNs;
			mBirthTimeNs = ReanimClock::Subtract(nowNs, ageNs);
			RefreshParticleLifeFraction(*this, nowNs);
		}
		else
		{
			ageNs = mSystemDurationNs;
			aDie = true;
		}
	}

	if (mCrossFadeEndTimeNs != 0 && nowNs >= mCrossFadeEndTimeNs)
		aDie = true;
	if (mCrossFadeEmitterID != ParticleEmitterID::PARTICLEEMITTERID_NULL)
	{
		TodParticleEmitter* aCrossFadeEmitter = mParticleSystem->mParticleHolder->mEmitters.DataArrayTryToGet(mCrossFadeEmitterID);
		if (nowNs >= mCrossFadeEndTimeNs || aCrossFadeEmitter == nullptr || aCrossFadeEmitter->mDead)
			aDie = true;
	}

	SampleSystemPosition(mEmitterDef->mSystemFields.count ? aData.mEmitterLifeFractions[aHolder.mEmitters.DataArrayIndexOf(this)] : 0.0);
	// 尾存倒序遍历（=原头→尾序）；下标直传 DeleteParticleAt（免逐粒 Find 扫）；
	// DeleteParticle 递归删交叉源可能改本行更低（更旧）下标——memmove 后
	// i-- 恰落到下一个更旧粒子，语义与原"预存 next"逐字一致
	for (int i = static_cast<int>(particles.size()) - 1; i >= 0; i--)
	{
		TodParticle* aParticle = mParticleSystem->mParticleHolder->mParticles.DataArrayTryToGet(
			static_cast<unsigned int>(particles[i]));
		if (!aParticle || aParticle->mParticleEmitter != this) continue;
		if (!UpdateParticle(aParticle, deltaNs, nowNs, ParticleLifeAgeAt(previousNowNs, aParticle->mBirthTimeNs, aParticle->mParticleDurationNs)))
			DeleteParticleAt(i, aParticle);
	}
	if (spawnDelta != 0) UpdateSpawning(spawnDelta);

	if (aDie)
	{
		DeleteNonCrossFading();
		if (ParticleRosterCount() == 0)
		{
			mDead = true;
			return;
		}
	}
}

//0x518210
// 粒子轨道求值、三角函数和插值全部在 CPU 完成；
// GPU 只绘制 TodTriangleGroup 合批后的三角形。

namespace
{
// 交叉混合通道插值：未设定侧让位于已设定侧（v2.0 原语义）
float CrossFadeLerp(float theFrom, float theTo, bool theFromIsSet, bool theToIsSet, float theFraction)
{
    if (!theFromIsSet)
        return theTo;
    if (!theToIsSet)
        return theFrom;
    return theFrom + (theTo - theFrom) * theFraction;
}
}

bool TodParticleEmitter::GetRenderParams(TodParticle* theParticle, ParticleRenderParams* theParams)
{
	return GetRenderParamsDepth(theParticle, theParams, 0);
}

// 链式交叉混合求值带深度护栏：环/超长链（雾天无尽的连环 crossfade）在
// GPU 提交路径有显式环检测，CPU 递归版必须同等防护——否则 SIGSEGV 栈溢出
// 直杀进程（nubia 实机无尽进板即闪退、无 fatal 日志）。
bool TodParticleEmitter::GetRenderParamsDepth(TodParticle* theParticle, ParticleRenderParams* theParams, int theDepth)
{
	TodParticleEmitter* aEmitter = theParticle->mParticleEmitter;
	TodEmitterDefinition* aDef = aEmitter->mEmitterDef;

	// 快路：def 无任何已设渲染轨且覆写全默认——直给标准渲染参数，
	// 跳过下方 20 次轨道求值（无色无缩放的粒子 def 大多命中）
	if (aDef->mRenderTrackMask == 0 &&
		aEmitter->mColorOverride.mRed == 255 && aEmitter->mColorOverride.mGreen == 255 &&
		aEmitter->mColorOverride.mBlue == 255 && aEmitter->mColorOverride.mAlpha == 255 &&
		aEmitter->mScaleOverride == 1.0f)
	{
		theParams->mRedIsSet = theParams->mGreenIsSet = theParams->mBlueIsSet =
			theParams->mAlphaIsSet = theParams->mParticleScaleIsSet =
			theParams->mParticleStretchIsSet = theParams->mSpinPositionIsSet = false;
		theParams->mPositionIsSet = aDef->mParticleFields.count > 0 ||
			FloatTrackIsSet(aDef->mEmitterRadius) || FloatTrackIsSet(aDef->mEmitterOffsetX) ||
			FloatTrackIsSet(aDef->mEmitterOffsetY) || FloatTrackIsSet(aDef->mEmitterBoxX) ||
			FloatTrackIsSet(aDef->mEmitterBoxY);
		theParams->mRed = theParams->mGreen = theParams->mBlue = theParams->mAlpha = 255.0f;
		theParams->mParticleScale = 1.0f;
		// An omitted stretch track evaluates to one. Zero collapses the entire
		// vertical transform (for example the unadorned PowieWord emitter).
		theParams->mParticleStretch = 1.0f;
		theParams->mSpinPosition = theParticle->mSpinPosition;
		theParams->mPosX = theParticle->mPosition.x;
		theParams->mPosY = theParticle->mPosition.y;
		return true;
	}

	// 颜色。系统轨道、粒子轨道和覆写任一有定义即认为该通道已设定。
	theParams->mRedIsSet = FloatTrackIsSet(aDef->mSystemRed) || FloatTrackIsSet(aDef->mParticleRed) ||
		aEmitter->mColorOverride.mRed != 255.0f;
	theParams->mGreenIsSet = FloatTrackIsSet(aDef->mSystemGreen) || FloatTrackIsSet(aDef->mParticleGreen) ||
		aEmitter->mColorOverride.mGreen != 255.0f;
	theParams->mBlueIsSet = FloatTrackIsSet(aDef->mSystemBlue) || FloatTrackIsSet(aDef->mParticleBlue) ||
		aEmitter->mColorOverride.mBlue != 255.0f;
	theParams->mAlphaIsSet = FloatTrackIsSet(aDef->mSystemAlpha) || FloatTrackIsSet(aDef->mParticleAlpha) ||
		aEmitter->mColorOverride.mAlpha != 255.0f;
	// 缩放/拉伸
	theParams->mParticleScaleIsSet = FloatTrackIsSet(aDef->mParticleScale) || aEmitter->mScaleOverride != 1.0f;
	theParams->mParticleStretchIsSet = FloatTrackIsSet(aDef->mParticleStretch);
	// 旋转角度（随机初始角/对齐发射角也算已设定）
	theParams->mSpinPositionIsSet = FloatTrackIsSet(aDef->mParticleSpinSpeed) ||
		FloatTrackIsSet(aDef->mParticleSpinAngle) ||
		TestBit(aDef->mParticleFlags, (int)ParticleFlags::PARTICLE_RANDOM_LAUNCH_SPIN) ||
		TestBit(aDef->mParticleFlags, (int)ParticleFlags::PARTICLE_ALIGN_LAUNCH_SPIN);
	// 位置
	theParams->mPositionIsSet = aDef->mParticleFields.count > 0 ||
		FloatTrackIsSet(aDef->mEmitterRadius) || FloatTrackIsSet(aDef->mEmitterOffsetX) ||
		FloatTrackIsSet(aDef->mEmitterOffsetY) || FloatTrackIsSet(aDef->mEmitterBoxX) ||
		FloatTrackIsSet(aDef->mEmitterBoxY);

	const auto& aHolder = *aEmitter->mParticleSystem->mParticleHolder;
	const auto& aData = *aHolder.mData;
	const float aSystemTime = (aEmitter->mSystemDurationNs > 0 ? static_cast<float>(aData.mEmitterLifeFractions[aHolder.mEmitters.DataArrayIndexOf(aEmitter)]) : 0.0f);
	const float aParticleTime = (theParticle->mParticleDurationNs > 0 ? static_cast<float>(aData.mParticleLifeFractions[aHolder.mParticles.DataArrayIndexOf(theParticle)]) : 0.0f);
	const auto systemEvaluate = [aEmitter, aSystemTime](const FloatParameterTrack& track,
		ParticleSystemTracks index) {
		return FloatTrackEvaluate(track, aSystemTime, aEmitter->mTrackInterp[int(index)]);
	};
	const auto particleEvaluate = [theParticle, aParticleTime](const FloatParameterTrack& track,
		ParticleTracks index) {
		return FloatTrackEvaluate(track, aParticleTime, theParticle->mParticleInterp[int(index)]);
	};

	// 实际颜色 = 粒子颜色 * 系统颜色 * 覆写颜色 * 亮度
	const float aBrightness = particleEvaluate(aDef->mParticleBrightness, ParticleTracks::TRACK_PARTICLE_BRIGHTNESS) *
		systemEvaluate(aDef->mSystemBrightness, ParticleSystemTracks::TRACK_SYSTEM_BRIGHTNESS);
	theParams->mRed = particleEvaluate(aDef->mParticleRed, ParticleTracks::TRACK_PARTICLE_RED) *
		systemEvaluate(aDef->mSystemRed, ParticleSystemTracks::TRACK_SYSTEM_RED) *
		aEmitter->mColorOverride.mRed * aBrightness;
	theParams->mGreen = particleEvaluate(aDef->mParticleGreen, ParticleTracks::TRACK_PARTICLE_GREEN) *
		systemEvaluate(aDef->mSystemGreen, ParticleSystemTracks::TRACK_SYSTEM_GREEN) *
		aEmitter->mColorOverride.mGreen * aBrightness;
	theParams->mBlue = particleEvaluate(aDef->mParticleBlue, ParticleTracks::TRACK_PARTICLE_BLUE) *
		systemEvaluate(aDef->mSystemBlue, ParticleSystemTracks::TRACK_SYSTEM_BLUE) *
		aEmitter->mColorOverride.mBlue * aBrightness;
	theParams->mAlpha = particleEvaluate(aDef->mParticleAlpha, ParticleTracks::TRACK_PARTICLE_ALPHA) *
		systemEvaluate(aDef->mSystemAlpha, ParticleSystemTracks::TRACK_SYSTEM_ALPHA) *
		aEmitter->mColorOverride.mAlpha * aBrightness;
	theParams->mPosX = theParticle->mPosition.x;
	theParams->mPosY = theParticle->mPosition.y;
	theParams->mParticleScale = particleEvaluate(aDef->mParticleScale, ParticleTracks::TRACK_PARTICLE_SCALE) *
		aEmitter->mScaleOverride;
	theParams->mParticleStretch = particleEvaluate(aDef->mParticleStretch, ParticleTracks::TRACK_PARTICLE_STRETCH);
	theParams->mSpinPosition = theParticle->mSpinPosition;

	// 交叉混合：把源粒子的渲染参数按比例混入（源→本粒子方向）
	TodParticle* aCrossFadeParticle = aEmitter->mParticleSystem->mParticleHolder->mParticles.DataArrayTryToGet(
		(unsigned int)theParticle->mCrossFadeParticleID);
	if (theDepth < 32 && aCrossFadeParticle != nullptr &&
		aCrossFadeParticle->mCrossFadeDurationNs > 0)
	{
		ParticleRenderParams aCrossFadeParams;
		if (GetRenderParamsDepth(aCrossFadeParticle, &aCrossFadeParams, theDepth + 1))
		{
			float aFraction = aCrossFadeParticle->mCrossFadeDurationNs > 1
				? static_cast<float>(double(ParticleLifeAgeAt(aEmitter->mParticleSystem->mLastUpdateTimeNs, theParticle->mBirthTimeNs, theParticle->mParticleDurationNs)) / double(aCrossFadeParticle->mCrossFadeDurationNs - 1))
				: 1.0f;
			theParams->mRed = CrossFadeLerp(aCrossFadeParams.mRed, theParams->mRed, aCrossFadeParams.mRedIsSet, theParams->mRedIsSet, aFraction);
			theParams->mGreen = CrossFadeLerp(aCrossFadeParams.mGreen, theParams->mGreen, aCrossFadeParams.mGreenIsSet, theParams->mGreenIsSet, aFraction);
			theParams->mBlue = CrossFadeLerp(aCrossFadeParams.mBlue, theParams->mBlue, aCrossFadeParams.mBlueIsSet, theParams->mBlueIsSet, aFraction);
			theParams->mAlpha = CrossFadeLerp(aCrossFadeParams.mAlpha, theParams->mAlpha, aCrossFadeParams.mAlphaIsSet, theParams->mAlphaIsSet, aFraction);
			theParams->mParticleScale = CrossFadeLerp(
				aCrossFadeParams.mParticleScale, theParams->mParticleScale, aCrossFadeParams.mParticleScaleIsSet, theParams->mParticleScaleIsSet, aFraction);
			theParams->mParticleStretch = CrossFadeLerp(
				aCrossFadeParams.mParticleStretch, theParams->mParticleStretch, aCrossFadeParams.mParticleStretchIsSet, theParams->mParticleStretchIsSet, aFraction);
			theParams->mSpinPosition = CrossFadeLerp(
				aCrossFadeParams.mSpinPosition, theParams->mSpinPosition, aCrossFadeParams.mSpinPositionIsSet, theParams->mSpinPositionIsSet, aFraction);
			theParams->mPosX = CrossFadeLerp(aCrossFadeParams.mPosX, theParams->mPosX, aCrossFadeParams.mPositionIsSet, theParams->mPositionIsSet, aFraction);
			theParams->mPosY = CrossFadeLerp(aCrossFadeParams.mPosY, theParams->mPosY, aCrossFadeParams.mPositionIsSet, theParams->mPositionIsSet, aFraction);
			// 源已设定的通道视为本粒子也已设定
			theParams->mRedIsSet |= aCrossFadeParams.mRedIsSet;
			theParams->mGreenIsSet |= aCrossFadeParams.mGreenIsSet;
			theParams->mBlueIsSet |= aCrossFadeParams.mBlueIsSet;
			theParams->mAlphaIsSet |= aCrossFadeParams.mAlphaIsSet;
			theParams->mParticleScaleIsSet |= aCrossFadeParams.mParticleScaleIsSet;
			theParams->mParticleStretchIsSet |= aCrossFadeParams.mParticleStretchIsSet;
			theParams->mSpinPositionIsSet |= aCrossFadeParams.mSpinPositionIsSet;
			theParams->mPositionIsSet |= aCrossFadeParams.mPositionIsSet;
		}
	}
	return true;
}

// 求值完成的粒子 → 一个变换后的贴图三角形（v2.0 RenderParticle 语义，
// 图源从 Image cel 网格改为编译图集的逐帧 TextureRegion）。
static void RenderParticle(Graphics* g, TodParticle* theParticle, const Color& theColor,
	ParticleRenderParams* theParams, TodTriangleGroup* theTriangleGroup)
{
	TodParticleEmitter* aEmitter = theParticle->mParticleEmitter;
	TodEmitterDefinition* aEmitterDef = aEmitter->mEmitterDef;
	auto& aSystem = *aEmitter->mParticleSystem;
	TodParticleDefinition& aDefinition = *aSystem.mParticleDef;
	if (!aEmitterDef->mImageRectCount) return;
	auto* const aAtlas = aDefinition.mTextures[aSystem.mVariant];
	if (!aAtlas) return;

	const auto& aHolder = *aSystem.mParticleHolder;
	const auto& aData = *aHolder.mData;
	const auto aParticleTime = (theParticle->mParticleDurationNs > 0 ? static_cast<float>(aData.mParticleLifeFractions[aHolder.mParticles.DataArrayIndexOf(theParticle)]) : 0.0f);
	int aFrame = aEmitter->mFrameOverride;
	if (aFrame == -1)
	{
		if (FloatTrackIsSet(aEmitterDef->mAnimationRate))
		{
			// 恒定速率：相位锚定取整帧；采样速率：装载期积分直接可用。
			const auto aPhase = TodParticleAnimationPhase(*theParticle, aEmitterDef->mAnimationRate);
			const double aPhaseFraction = double(aPhase % ParticleClock::Cycle) / double(ParticleClock::Cycle);
			aFrame = ClampInt(int(aPhaseFraction * aEmitterDef->mImageFrames), 0, aEmitterDef->mImageFrames - 1);
		}
		else if (aEmitterDef->mAnimated)
			aFrame = ClampInt(int(aParticleTime * aEmitterDef->mImageFrames), 0, aEmitterDef->mImageFrames - 1);
		else
			aFrame = theParticle->mImageFrame;
	}
	aFrame = std::clamp(aFrame, 0, aEmitterDef->mImageRectCount - 1);
	TextureRegion aRegion = aDefinition.mImageRects[
		std::size_t(aSystem.mVariant) * aDefinition.mImageRectCount + aEmitterDef->mImageRectOffset + unsigned(aFrame)];
	if (!aRegion.mWidth || !aRegion.mHeight) return;

	const auto particleEvaluate = [theParticle, aParticleTime](const FloatParameterTrack& track,
		ParticleTracks index) {
		return FloatTrackEvaluate(track, aParticleTime, theParticle->mParticleInterp[int(index)]);
	};
	const float aClipTop = particleEvaluate(aEmitterDef->mClipTop, ParticleTracks::TRACK_PARTICLE_CLIP_TOP);
	const float aClipBottom = particleEvaluate(aEmitterDef->mClipBottom, ParticleTracks::TRACK_PARTICLE_CLIP_BOTTOM);
	const float aClipLeft = particleEvaluate(aEmitterDef->mClipLeft, ParticleTracks::TRACK_PARTICLE_CLIP_LEFT);
	const float aClipRight = particleEvaluate(aEmitterDef->mClipRight, ParticleTracks::TRACK_PARTICLE_CLIP_RIGHT);
	const float aCelWidth = float(aRegion.mWidth), aCelHeight = float(aRegion.mHeight);
	if (aClipLeft > 0.0f || aClipTop > 0.0f || aClipRight > 0.0f || aClipBottom > 0.0f)
	{
		// Legacy clipping rounds source boundaries to texels before converting
		// them to UVs. The draw-position shift retains the unrounded fraction.
		theParams->mPosX += aClipLeft * aCelWidth;
		theParams->mPosY += aClipTop * aCelHeight;
		const float uSpan = aRegion.mU1 - aRegion.mU0, vSpan = aRegion.mV1 - aRegion.mV0;
		const int left = FloatRoundToInt(aClipLeft * aCelWidth);
		const int top = FloatRoundToInt(aClipTop * aCelHeight);
		aRegion.mWidth -= FloatRoundToInt(aCelWidth * (aClipLeft + aClipRight));
		aRegion.mHeight -= FloatRoundToInt(aCelHeight * (aClipTop + aClipBottom));
		aRegion.mU1 = aRegion.mU0 + uSpan * (float(left + aRegion.mWidth) / aCelWidth);
		aRegion.mV1 = aRegion.mV0 + vSpan * (float(top + aRegion.mHeight) / aCelHeight);
		aRegion.mU0 += uSpan * (float(left) / aCelWidth);
		aRegion.mV0 += vSpan * (float(top) / aCelHeight);
	}

	if (TestBit(aEmitterDef->mParticleFlags, (int)ParticleFlags::PARTICLE_ALIGN_TO_PIXELS))
	{
		theParams->mPosX = FloatRoundToInt(theParams->mPosX);
		theParams->mPosY = FloatRoundToInt(theParams->mPosY);
	}
	int aDrawMode = g->mDrawMode;
	if (TestBit(aEmitterDef->mParticleFlags, (int)ParticleFlags::PARTICLE_ADDITIVE))
		aDrawMode = Graphics::DRAWMODE_ADDITIVE;
	if (TestBit(aEmitterDef->mParticleFlags, (int)ParticleFlags::PARTICLE_FULLSCREEN))
	{
		// 全屏粒子先冲刷攒批再整屏填色（覆盖渲染目标，不随世界宽）
		theTriangleGroup->DrawGroup(g);
		Color anOldColor = g->mColor;
		int anOldDrawMode = g->mDrawMode;
		g->mColor = theColor;
		g->FillRect(-g->mTransX, -g->mTransY, g->mDestImage->mWidth, g->mDestImage->mHeight);
		g->mColor = anOldColor;
		g->mDrawMode = anOldDrawMode;
	}
	else
	{
		SexyMatrix3 aTransform;
		TodScaleRotateTransformMatrix(aTransform,
			theParams->mPosX,
			theParams->mPosY,
			theParams->mSpinPosition,
			theParams->mParticleScale,
			theParams->mParticleStretch * theParams->mParticleScale);
		const TextureSource aSource{aAtlas, Sexy::RenderInterface::sImageSamplingFlags};
		theTriangleGroup->AddTriangle(g, aSource, aTransform, g->mClipRect, theColor, aDrawMode, aRegion);
		if (aEmitter->mExtraAdditiveDrawOverride)
			theTriangleGroup->AddTriangle(g, aSource, aTransform, g->mClipRect, theColor, Graphics::DRAWMODE_ADDITIVE, aRegion);
	}
}

void TodParticleEmitter::DrawParticle(Graphics* g, TodParticle* theParticle, TodTriangleGroup* theTriangleGroup)
{
	if (theParticle->mCrossFadeDurationNs > 0)  // 交叉混合的源粒子不直接绘制
		return;

	ParticleRenderParams aParams;
	if (GetRenderParams(theParticle, &aParams))
	{
		Color aColor(
			ClampInt(FloatRoundToInt(aParams.mRed), 0, 255),
			ClampInt(FloatRoundToInt(aParams.mGreen), 0, 255),
			ClampInt(FloatRoundToInt(aParams.mBlue), 0, 255),
			ClampInt(FloatRoundToInt(aParams.mAlpha), 0, 255));
		if (aColor.mAlpha > 0)
		{
			aParams.mPosX += g->mTransX;
			aParams.mPosY += g->mTransY;

			TodParticle* aParticle;
			if (mEmitterDef->mImageRectCount)  // 有贴图时渲染本粒子
				aParticle = theParticle;
			else  // 无贴图时取交叉混合来源的图像
				aParticle = mParticleSystem->mParticleHolder->mParticles.DataArrayTryToGet(
					(unsigned int)theParticle->mCrossFadeParticleID);
			if (aParticle != nullptr)
				RenderParticle(g, aParticle, aColor, &aParams, theTriangleGroup);
		}
	}
}

//0x518370
void TodParticleSystem::Draw(Graphics* g)
{
	const int aDrawCount = EmitterRosterCount();
	for (int i = 0; i < aDrawCount; i++)
		EmitterRosterEmitterAt(i)->Draw(g);
}

//0x5183A0
void TodParticleEmitter::Draw(Graphics* g)
{
	if (TestBit(mEmitterDef->mParticleFlags, (int)ParticleFlags::PARTICLE_SOFTWARE_ONLY))
		return;

	// 同发射器内逐粒 CPU 求值攒一批三角形一次提交（尾存倒序=原头→尾序=新→旧叠放序）
	TodTriangleGroup aTriangleGroup;
	const int aProbeCount = ParticleRosterCount();
	aTriangleGroup.mVertArray.reserve(size_t(aProbeCount) * 2);
	for (int i = aProbeCount - 1; i >= 0; i--)
		DrawParticle(g, ParticleRosterParticleAt(i), &aTriangleGroup);
	aTriangleGroup.DrawGroup(g);
}


//0x518440
void TodParticleSystem::SystemMove(float theX, float theY)
{
	const int aMoveCount = EmitterRosterCount();
	for (int i = 0; i < aMoveCount; i++)
		EmitterRosterEmitterAt(i)->SystemMove(theX, theY);
}

//0x518480
void TodParticleEmitter::SystemMove(float theX, float theY)
{
	// 原版累加（2026-10-27 恢复）：外部输入直接按位移差平移中心与全体
	// 粒子；不再保存外部输入的重复锚点。
	float aDeltaX = theX - mSystemCenter.x;
	float aDeltaY = theY - mSystemCenter.y;
	if (FloatApproxEqual(aDeltaX, 0.0f) && FloatApproxEqual(aDeltaY, 0.0f))
		return;
	mSystemCenter.x = theX;
	mSystemCenter.y = theY;
	if (!TestBit(mEmitterDef->mParticleFlags, (int)ParticleFlags::PARTICLE_PARTICLES_DONT_FOLLOW))
	{
		for (int i = ParticleRosterCount() - 1; i >= 0; i--)
		{
			TodParticle* aParticle = ParticleRosterParticleAt(i);
			aParticle->mPosition.x += aDeltaX;
			aParticle->mPosition.y += aDeltaY;
		}
	}
}

//0x518560
void TodParticleSystem::OverrideColor(ParticleEmitterNameRange theRange, const Color& theColor)
{
	const int aRosterCount = EmitterRosterCount();
	for (int i = 0; i < aRosterCount; i++)
	{
		TodParticleEmitter* aEmitter = EmitterRosterEmitterAt(i);
		if (theRange.Contains(static_cast<ParticleEmitterName>(aEmitter->mEmitterDef->mNameId)))
			aEmitter->mColorOverride = theColor;
	}
}

void TodParticleSystem::OverrideColor(ParticleEmitterName theName, const Color& theColor)
{
	const int aRosterCount = EmitterRosterCount();
	for (int i = 0; i < aRosterCount; i++)
	{
		TodParticleEmitter* aEmitter = EmitterRosterEmitterAt(i);
		if (static_cast<ParticleEmitterName>(aEmitter->mEmitterDef->mNameId) == theName)
			aEmitter->mColorOverride = theColor;
	}
}


//0x5185D0
void TodParticleSystem::OverrideExtraAdditiveDraw(ParticleEmitterNameRange theRange, bool theEnableExtraAdditiveDraw)
{
	const int aRosterCount = EmitterRosterCount();
	for (int i = 0; i < aRosterCount; i++)
	{
		TodParticleEmitter* aEmitter = EmitterRosterEmitterAt(i);
		if (theRange.Contains(static_cast<ParticleEmitterName>(aEmitter->mEmitterDef->mNameId)))
			aEmitter->mExtraAdditiveDrawOverride = theEnableExtraAdditiveDraw;
	}
}

void TodParticleSystem::OverrideExtraAdditiveDraw(ParticleEmitterName theName, bool theEnableExtraAdditiveDraw)
{
	const int aRosterCount = EmitterRosterCount();
	for (int i = 0; i < aRosterCount; i++)
	{
		TodParticleEmitter* aEmitter = EmitterRosterEmitterAt(i);
		if (static_cast<ParticleEmitterName>(aEmitter->mEmitterDef->mNameId) == theName)
			aEmitter->mExtraAdditiveDrawOverride = theEnableExtraAdditiveDraw;
	}
}


//0x518560
void TodParticleSystem::OverrideFrame(ParticleEmitterNameRange theRange, int theFrame)
{
	const int aRosterCount = EmitterRosterCount();
	for (int i = 0; i < aRosterCount; i++)
	{
		TodParticleEmitter* aEmitter = EmitterRosterEmitterAt(i);
		if (theRange.Contains(static_cast<ParticleEmitterName>(aEmitter->mEmitterDef->mNameId)))
			aEmitter->mFrameOverride = theFrame;
	}
}

void TodParticleSystem::OverrideFrame(ParticleEmitterName theName, int theFrame)
{
	const int aRosterCount = EmitterRosterCount();
	for (int i = 0; i < aRosterCount; i++)
	{
		TodParticleEmitter* aEmitter = EmitterRosterEmitterAt(i);
		if (static_cast<ParticleEmitterName>(aEmitter->mEmitterDef->mNameId) == theName)
			aEmitter->mFrameOverride = theFrame;
	}
}


//0x518630
void TodParticleSystem::OverrideScale(ParticleEmitterNameRange theRange, float theScale)
{
	const int aRosterCount = EmitterRosterCount();
	for (int i = 0; i < aRosterCount; i++)
	{
		TodParticleEmitter* aEmitter = EmitterRosterEmitterAt(i);
		if (theRange.Contains(static_cast<ParticleEmitterName>(aEmitter->mEmitterDef->mNameId)))
			aEmitter->mScaleOverride = theScale;
	}
}

void TodParticleSystem::OverrideScale(ParticleEmitterName theName, float theScale)
{
	const int aRosterCount = EmitterRosterCount();
	for (int i = 0; i < aRosterCount; i++)
	{
		TodParticleEmitter* aEmitter = EmitterRosterEmitterAt(i);
		if (static_cast<ParticleEmitterName>(aEmitter->mEmitterDef->mNameId) == theName)
			aEmitter->mScaleOverride = theScale;
	}
}




//0x5186D0
void TodParticleEmitter::CrossFadeEmitter(TodParticleEmitter* theToEmitter)
{
	const auto& aHolder = *mParticleSystem->mParticleHolder;
	const auto& aData = *aHolder.mData;
	if (mCrossFadeEndTimeNs > mParticleSystem->mLastUpdateTimeNs)
	{
		TodTraceAndLog("We don't support cross fading emitters more than one at a time\n");
		return;
	}
	if (!FloatTrackIsSet(theToEmitter->mEmitterDef->mCrossFadeDuration))
	{
		TodTrace("Can't cross fade to emitter that doesn't have CrossFadeDuration");
		return;
	}
	TOD_ASSERT(theToEmitter != this);

	float aCrossFadeDurationInterp = RandomFloat(1.0f);
	const auto durationNs = std::max(ParticleSourceIntervalNs, Sexy::TimeCounter::Encode(
		FloatTrackEvaluate(theToEmitter->mEmitterDef->mCrossFadeDuration, aData.mEmitterLifeFractions[aHolder.mEmitters.DataArrayIndexOf(this)], aCrossFadeDurationInterp)));
	mCrossFadeEndTimeNs = ReanimClock::Add(mParticleSystem->mLastUpdateTimeNs, durationNs);
	mCrossFadeEmitterID = (ParticleEmitterID)mParticleSystem->mParticleHolder->mEmitters.DataArrayGetID(theToEmitter);
	if (!FloatTrackIsSet(theToEmitter->mEmitterDef->mSystemDuration))
	{
		theToEmitter->mSystemDurationNs = durationNs;
		RefreshParticleLifeFraction(*theToEmitter, mParticleSystem->mLastUpdateTimeNs);
	}

	const int aXFadeParticleCount = ParticleRosterCount();
	for (int i = aXFadeParticleCount - 1; i >= 0; i--)
		CrossFadeParticle(ParticleRosterParticleAt(i), theToEmitter);
}

//0x518790
void TodParticleSystem::CrossFade(ParticleEmitterName theName)
{
	const auto aId = static_cast<std::size_t>(theName);
	const int aIndex = aId < mParticleDef->mEmitterIndexByName.size()
		? mParticleDef->mEmitterIndexByName[aId] : -1;
	TodEmitterDefinition* aEmitterDef = aIndex >= 0
		? &mParticleDef->mEmitterDefs[aIndex] : nullptr;
	if (aEmitterDef == nullptr)
	{
		TodTraceAndLog("Can't find cross fade emitter id %u\n", static_cast<std::uint32_t>(theName));
		return;
	}
	if (!FloatTrackIsSet(aEmitterDef->mCrossFadeDuration))
	{
		TodTraceAndLog("Can't cross fade without duration set\n");
		return;
	}
	const int aXFadeCount = EmitterRosterCount();
	if (mParticleHolder->mEmitters.mSize + aXFadeCount > mParticleHolder->mEmitters.mMaxSize)
	{
		TodTraceAndLog("Too many emitters to cross fade\n");
		ParticleSystemDie();
		return;
	}

	// Appended target emitters do not participate as sources in this call.
	for (int i = 0; i < aXFadeCount; i++)
	{
		TodParticleEmitter* aEmitter = EmitterRosterEmitterAt(i);
		if (aEmitter->mEmitterDef != aEmitterDef)
		{
			TodParticleEmitter* aCrossFadeEmitter = mParticleHolder->mEmitters.DataArrayAlloc();
			aCrossFadeEmitter->TodEmitterInitialize(aEmitter->mSystemCenter.x, aEmitter->mSystemCenter.y, this, aEmitterDef);
			if (!EmitterRosterAdd(aCrossFadeEmitter))
			{
				mParticleHolder->mEmitters.DataArrayFree(aCrossFadeEmitter);
				continue;
			}
			aEmitter->CrossFadeEmitter(aCrossFadeEmitter);
		}
	}
}

void TodParticleSystem::CrossFade(ParticleEmitterNameRange theRange)
{
	// A range resolves one target, in definition declaration order.
	for (int i = 0; i < mParticleDef->mEmitterDefCount; i++)
	{
		const ParticleEmitterName aName = static_cast<ParticleEmitterName>(mParticleDef->mEmitterDefs[i].mNameId);
		if (theRange.Contains(aName))
		{
			CrossFade(aName);
			return;
		}
	}
}

// ── 发射器名册紧凑侧表（2026-10-04 P3 第一步，用户裁决）──
// 按系统池槽位键控（格桶/行段桶同款派生索引先例）；成员 mEmitterList 已
// 退役（保留以钉 sizeof/dat64 分段）。迭代从"链表节点(24B)→ID→池槽"
// 三跳缩为"2B 槽号数组→池槽"一跳；AddTail 序=原链表序逐字保持。
// 清册纪律：ParticleSystemDie/析构/池 free/FreeAll 全挂钩，槽位回收后
// 新系统复用键前必清。
namespace
{
constexpr int EMITTER_ROSTER_MAX = 64;
constexpr int PARTICLE_ROSTER_MAX = 256;
}  // namespace

// Dense slot rows belong to the supplied data, not to an individual holder view.
struct TodParticleRosterStorage
{
	struct EmitterRosterRow
	{
		uint32_t mOwnerKey;
		uint16_t mSlots[EMITTER_ROSTER_MAX];
		uint16_t mCount;
	};

	struct ParticleRosterRow
	{
		uint32_t mOwnerKey;
		uint16_t mSlots[PARTICLE_ROSTER_MAX];
		uint16_t mCount;
	};

	EmitterRosterRow mEmitterRosters[PARTICLE_POOL_CAPACITY];
	ParticleRosterRow mParticleRosters[PARTICLE_POOL_CAPACITY];

	static EmitterRosterRow* EmitterRosterRowFor(const TodParticleSystem* theSystem)
	{
		// Unbound systems are valid during construction and interrupted loads.
		if (theSystem->mParticleHolder == nullptr)
			return nullptr;
		auto& aHolder = *theSystem->mParticleHolder;
		auto& aPool = aHolder.mParticleSystems;
		const unsigned int aSlot = aPool.DataArrayIndexOf(theSystem);
		EmitterRosterRow& aRow = aHolder.mData->mRosterStorage->mEmitterRosters[aSlot];
		const uint32_t aOwnerKey = (uint32_t)aPool.DataArrayGetID(const_cast<TodParticleSystem*>(theSystem));
		if (aRow.mOwnerKey != aOwnerKey)
		{
			aRow.mOwnerKey = aOwnerKey;
			aRow.mCount = 0;
		}
		return &aRow;
	}

	static ParticleRosterRow* ParticleRosterRowFor(const TodParticleEmitter* theEmitter)
	{
		if (theEmitter->mParticleSystem == nullptr || theEmitter->mParticleSystem->mParticleHolder == nullptr)
			return nullptr;
		auto& aHolder = *theEmitter->mParticleSystem->mParticleHolder;
		auto& aPool = aHolder.mEmitters;
		const unsigned int aSlot = aPool.DataArrayIndexOf(theEmitter);
		ParticleRosterRow& aRow = aHolder.mData->mRosterStorage->mParticleRosters[aSlot];
		const uint32_t aOwnerKey = (uint32_t)aPool.DataArrayGetID(const_cast<TodParticleEmitter*>(theEmitter));
		if (aRow.mOwnerKey != aOwnerKey)
		{
			aRow.mOwnerKey = aOwnerKey;
			aRow.mCount = 0;
		}
		return &aRow;
	}
};

TodParticleData::TodParticleData() = default;

//0x518870
TodParticleData::~TodParticleData()
{
	Dispose();
}

TodParticleHolder::TodParticleHolder(TodParticleData* theData)
	: mParticleSystems(theData->mParticleSystems),
	  mEmitters(theData->mEmitters), mParticles(theData->mParticles), mData(theData)
{
}

int TodParticleSystem::EmitterRosterCount() const
{
	auto* aRow = TodParticleRosterStorage::EmitterRosterRowFor(this);
	return aRow ? aRow->mCount : 0;
}

TodParticleEmitter* TodParticleSystem::EmitterRosterEmitterAt(int theIndex) const
{
	auto* aRow = TodParticleRosterStorage::EmitterRosterRowFor(this);
	if (aRow == nullptr || (unsigned)theIndex >= aRow->mCount)
		return nullptr;
	return &mParticleHolder->mEmitters.mBlock[aRow->mSlots[theIndex]].mItem;
}

ParticleEmitterID TodParticleSystem::EmitterRosterIDAt(int theIndex) const
{
	auto* aRow = TodParticleRosterStorage::EmitterRosterRowFor(this);
	if (aRow == nullptr || (unsigned)theIndex >= aRow->mCount)
		return (ParticleEmitterID)0;
	return (ParticleEmitterID)mParticleHolder->mEmitters.DataArrayGetID(&mParticleHolder->mEmitters.mBlock[aRow->mSlots[theIndex]].mItem);
}

bool TodParticleSystem::EmitterRosterAdd(TodParticleEmitter* theEmitter)
{
	auto* aRow = TodParticleRosterStorage::EmitterRosterRowFor(this);
	if (aRow == nullptr)
		return false;
	if (aRow->mCount >= EMITTER_ROSTER_MAX)
	{
		TodTrace("Too many emitters in roster\n");
		return false;
	}
	aRow->mSlots[aRow->mCount++] = (uint16_t)mParticleHolder->mEmitters.DataArrayIndexOf(theEmitter);
	return true;
}

void TodParticleSystem::EmitterRosterClear()
{
	auto* aRow = TodParticleRosterStorage::EmitterRosterRowFor(this);
	if (aRow != nullptr)
		aRow->mCount = 0;
}

// ── 粒子名册紧凑侧表（2026-10-04 P3 第二步，用户裁决）──
// 按发射器池槽位键控 + 世代键行主自失效（发射器侧表同款，装载
// ClearNativeObject memset 不经钩子的防陈旧机制）；尾存倒序遍历保
// AddHead 头→尾序（AddHead 的 O(1) 由尾插达成）；倒序迭代中删除只
// memmove 已遍历过的新侧段。容量 256/发射器，溢界拒生=池满同款。
int TodParticleEmitter::ParticleRosterCount() const
{
	auto* aRow = TodParticleRosterStorage::ParticleRosterRowFor(this);
	return aRow ? aRow->mCount : 0;
}

TodParticle* TodParticleEmitter::ParticleRosterParticleAt(int theIndex) const
{
	auto* aRow = TodParticleRosterStorage::ParticleRosterRowFor(this);
	if (aRow == nullptr || (unsigned)theIndex >= aRow->mCount)
		return nullptr;
	return &mParticleSystem->mParticleHolder->mParticles.mBlock[aRow->mSlots[theIndex]].mItem;
}

ParticleID TodParticleEmitter::ParticleRosterIDAt(int theIndex) const
{
	auto* aRow = TodParticleRosterStorage::ParticleRosterRowFor(this);
	if (aRow == nullptr || (unsigned)theIndex >= aRow->mCount)
		return ParticleID::PARTICLEID_NULL;
	return (ParticleID)mParticleSystem->mParticleHolder->mParticles.DataArrayGetID(
		&mParticleSystem->mParticleHolder->mParticles.mBlock[aRow->mSlots[theIndex]].mItem);
}

bool TodParticleEmitter::ParticleRosterAdd(TodParticle* theParticle)
{
	auto* aRow = TodParticleRosterStorage::ParticleRosterRowFor(this);
	if (aRow == nullptr)
		return false;
	if (aRow->mCount >= PARTICLE_ROSTER_MAX)
	{
		TodTraceWithoutSpamming("Too many particles in roster (emitter id=%u)\n", static_cast<unsigned>(mEmitterDef->mNameId));
		return false;
	}
	aRow->mSlots[aRow->mCount++] = (uint16_t)mParticleSystem->mParticleHolder->mParticles.DataArrayIndexOf(theParticle);
	return true;
}

int TodParticleEmitter::ParticleRosterFind(TodParticle* theParticle) const
{
	auto* aRow = TodParticleRosterStorage::ParticleRosterRowFor(this);
	if (aRow == nullptr)
		return -1;
	const uint16_t aSlot = (uint16_t)mParticleSystem->mParticleHolder->mParticles.DataArrayIndexOf(theParticle);
	for (int i = 0; i < aRow->mCount; i++)
		if (aRow->mSlots[i] == aSlot)
			return i;
	return -1;
}

void TodParticleEmitter::ParticleRosterRemoveAt(int theIndex)
{
	auto* aRow = TodParticleRosterStorage::ParticleRosterRowFor(this);
	if (aRow == nullptr || (unsigned)theIndex >= aRow->mCount)
		return;
	std::memmove(&aRow->mSlots[theIndex], &aRow->mSlots[theIndex + 1],
		(aRow->mCount - theIndex - 1) * sizeof(uint16_t));
	aRow->mCount--;
}

void TodParticleEmitter::ParticleRosterClear()
{
	auto* aRow = TodParticleRosterStorage::ParticleRosterRowFor(this);
	if (aRow != nullptr)
		aRow->mCount = 0;
}

//0x518900
void TodParticleData::Initialize()
{
	mRosterStorage = std::make_unique<TodParticleRosterStorage>();
	mParticleLifeFractions.fill(0);
	mEmitterLifeFractions.fill(0);
	mParticleSystems.DataArrayInitialize(PARTICLE_POOL_CAPACITY, DataArrayKind::ParticleSystems);
	mEmitters.DataArrayInitialize(PARTICLE_POOL_CAPACITY, DataArrayKind::ParticleEmitters);
	mParticles.DataArrayInitialize(PARTICLE_POOL_CAPACITY, DataArrayKind::Particles);
}

//0x5189A0
void TodParticleData::Dispose()
{
	mParticleSystems.DataArrayDispose();
	mEmitters.DataArrayDispose();
	mParticles.DataArrayDispose();
	// Pool object destructors must finish before their roster rows are released.
	mRosterStorage.reset();
	mParticleLifeFractions.fill(0);
	mEmitterLifeFractions.fill(0);
}

bool TodParticleHolder::IsOverLoaded()
{
	return mParticleSystems.mSize > MAX_PARTICLES_SIZE || mEmitters.mSize > MAX_PARTICLES_SIZE || mParticles.mSize > MAX_PARTICLES_SIZE;
}

TodParticleSystem* TodParticleHolder::AllocParticleSystemFromDef(float theX, float theY, int theRenderOrder, TodParticleDefinition* theDefinition, ParticleEffect theParticleEffect, std::int64_t nowNs, std::uint32_t theVariant)
{
	if (!theDefinition || theVariant>=theDefinition->mVariantCount)
		throw std::out_of_range("particle appearance variant out of range");
	if (mParticleSystems.mSize == mParticleSystems.mMaxSize)
	{
		TodTrace("Too many particle systems\n");
		return nullptr;
	}
	if (theDefinition->mEmitterDefCount + mEmitters.mSize > mEmitters.mMaxSize)
	{
		TodTrace("Too many particle emitters\n");
		return nullptr;
	}

	TodParticleSystem* aTodParticle = mParticleSystems.DataArrayAlloc();
	aTodParticle->mParticleHolder = this;
	aTodParticle->TodParticleInitializeFromDef(theX, theY, theRenderOrder, theDefinition, theParticleEffect, nowNs, theVariant);
	return aTodParticle;
}

//0x518A70
TodParticleSystem* TodParticleHolder::AllocParticleSystem(float theX, float theY, int theRenderOrder, ParticleEffect theParticleEffect, std::int64_t nowNs, std::uint32_t theVariant)
{
	TOD_ASSERT((int)theParticleEffect >= 0 && (int)theParticleEffect < gParticleDefCount);
	return AllocParticleSystemFromDef(theX, theY, theRenderOrder, &gParticleDefArray[(int)theParticleEffect], theParticleEffect, nowNs, theVariant);
}
