#pragma once
#include "misc/TimeCounter.h"
#include "ConstEnums.h"
#include "Sexy.TodLib/DataArray.h"
#include "widget/Widget.h"
#include "widget/ButtonListener.h"

#include "Plant.h"
#include "Zombie.h"
#include "Projectile.h"
#include "Coin.h"
#include "LawnMower.h"
#include "GridItem.h"

using namespace Sexy;

constexpr int MAX_GRID_SIZE_X = 9;
constexpr int MAX_GRID_SIZE_Y = 6;
// 植物池容量（2026-10-03 用户指令 constexpr 化，原 1024）：合法盘面上限
// 9x6x4=216，256 留余量；改版扩容单点改此处——池内存与格桶 next 表随之。
constexpr int PLANT_POOL_CAPACITY = 256;
// 满盘理论上限 = 9x6x4 层 = 216（PLANT_CELL_COUNT 定义在后，此处用层常量 4）
static_assert(PLANT_POOL_CAPACITY >= MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y * 4,
    "植物池容量须不小于满盘理论上限");
// 僵尸池容量（2026-10-04 收编 Board.cpp DataArrayInitialize 字面量，原值
// 1024）：dat64 整池镜像语义不动；行段桶表尺寸随之（改版扩容单点改）。
constexpr int ZOMBIE_POOL_CAPACITY = 1024;
#define MAX_ZOMBIES_IN_WAVE 50
#define MAX_ZOMBIE_WAVES 100
// 原为无括号表达式宏（优先级隐患），转 constexpr
constexpr int MAX_GRAVE_STONES = MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y;
#define MAX_POOL_GRID_SIZE 10
#define MAX_RENDER_ITEMS 2048
#define PROGRESS_METER_COUNTER 150

class LawnApp;
class CursorObject;
class CursorPreview;
class GameButton;
class MessageWidget;
class SeedBank;
class ToolTipWidget;
class CutScene;
class Challenge;
class Reanimation;
class DataSync;
class TodParticleSystem;
namespace Sexy
{
	class Graphics;
	class ButtonWidget;
	class WidgetManager;
	class Image;
	class MTRand;
}

class HitResult
{
public:
	void*							mObject;
	GameObjectType					mObjectType;
};

class RenderItem
{
public:
	RenderObjectType				mRenderObjectType;
	int								mZPos;
	union
	{
		GameObject*					mGameObject;
		Plant*						mPlant;
		Zombie*						mZombie;
		Coin*						mCoin;
		Projectile*					mProjectile;
		CursorPreview*				mCursorPreview;
		TodParticleSystem*			mParticleSytem;
		Reanimation*				mReanimation;
		GridItem*					mGridItem;
		LawnMower*					mMower;
		BossPart					mBossPart;
		int							mBoardGridY;
	};
};

struct ZombiePicker
{
	int								mZombieCount;
	int								mZombiePoints;
	int								mZombieTypeCount[NUM_ZOMBIE_TYPES];
	int								mAllWavesZombieTypeCount[NUM_ZOMBIE_TYPES];
};

/*inline*/ void						ZombiePickerInitForWave(ZombiePicker* theZombiePicker);
/*inline*/ void						ZombiePickerInit(ZombiePicker* theZombiePicker);

// 植物格桶层特化（2026-10-03 用户方案；桶体在 Board.cpp，见 GetCellPlant）
enum PlantCellLayerIdx : int
{
	PLANT_CELL_UNDER = 0,   // 花盆/荷叶（禅园荷叶除外→普通层）
	PLANT_CELL_NORMAL = 1,  // 普通植物（含玉米炮双格注册）
	PLANT_CELL_PUMPKIN = 2, // 南瓜壳
	PLANT_CELL_FLYING = 3,  // 飞行（咖啡豆）
	PLANT_CELL_COUNT = 4
};
struct PlantsOnLawn
{
	// 字段声明序 = 桶层特化序（PlantCellLayerIdx 0..3，2026-10-03 对齐；
	// 栈上查询结果结构，无序列化，纯声明序调整零语义）
	Plant*							mUnderPlant;      // 0 底座（花盆/荷叶）
	Plant*							mNormalPlant;     // 1 普通
	Plant*							mPumpkinPlant;    // 2 南瓜壳
	Plant*							mFlyingPlant;     // 3 飞行（咖啡豆）
};

struct BungeeDropGrid
{
	TodWeightedGridArray			mGridArray[MAX_GRID_SIZE_X * MAX_GRID_SIZE_Y];
	int								mGridArrayCount;
};

class Board : public Widget, public ButtonListener
{
public:
	inline static bool sShownMoreSunTutorial = false;
    // Runtime-only ownership state; precedes the serialized tail at mPaused.
    bool mPoolIntroVisual;
	LawnApp*						mApp;													//+0x8C
	DataArray<Zombie>				mZombies;												//+0x90
	DataArray<Plant>				mPlants;												//+0xAC
	DataArray<Projectile>			mProjectiles;											//+0xC8
	DataArray<Coin>					mCoins;													//+0xE4
	DataArray<LawnMower>			mLawnMowers;											//+0x100
	DataArray<GridItem>				mGridItems;												//+0x11C
	CursorObject*					mCursorObject;											//+0x138
	CursorPreview*					mCursorPreview;											//+0x13C
	MessageWidget*					mAdvice;												//+0x140
	SeedBank*						mSeedBank;												//+0x144
	GameButton*						mMenuButton;											//+0x148
	GameButton*						mStoreButton;											//+0x14C
	bool							mIgnoreMouseUp;											//+0x150
	ToolTipWidget*					mToolTip;												//+0x154
	_Font*							mDebugFont;												//+0x158
	CutScene*						mCutScene;												//+0x15C
	Challenge*						mChallenge;												//+0x160
	bool							mPaused;												//+0x164
	GridSquareType					mGridSquareType[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y];		//+0x168
	int								mGridCelLook[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y];			//+0x240
	int								mGridCelOffset[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y][2];	//+0x318
	int								mGridCelFog[MAX_GRID_SIZE_X][MAX_GRID_SIZE_Y + 1];		//+0x4C8
	bool							mEnableGraveStones;										//+0x5C4; false: initial graves fully risen, true: runtime graves rise
	int								mSpecialGraveStoneX;									//+0x5C8
	int								mSpecialGraveStoneY;									//+0x5CC
	float							mFogOffset;												//+0x5D0
	Sexy::TimeCounter								mFogBlownCountDown;										//+0x5D4
	PlantRowType					mPlantRow[MAX_GRID_SIZE_Y];								//+0x5D8
	int								mWaveRowGotLawnMowered[MAX_GRID_SIZE_Y];				//+0x5F0
	int								mBonusLawnMowersRemaining;								//+0x608
	int								mIceMinX[MAX_GRID_SIZE_Y];								//+0x60C
	Sexy::TimeCounter								mIceTimer[MAX_GRID_SIZE_Y];								//+0x624
	ParticleSystemID				mIceParticleID[MAX_GRID_SIZE_Y];						//+0x63C
	TodSmoothArray					mRowPickingArray[MAX_GRID_SIZE_Y];						//+0x654
	ZombieType						mZombiesInWave[MAX_ZOMBIE_WAVES][MAX_ZOMBIES_IN_WAVE];	//+0x6B4
	bool							mZombieAllowed[100];									//+0x54D4
	Sexy::TimeCounter								mSunCountDown;											//+0x5538
	int								mNumSunsFallen;											//+0x553C
	Sexy::TimeCounter								mShakeCounter;											//+0x5540
	int								mShakeAmountX;											//+0x5544
	int								mShakeAmountY;											//+0x5548
	BackgroundType					mBackground;											//+0x554C
	int								mLevel;													//+0x5550
	int								mSodPosition;											//+0x5554
	int								mPrevMouseX;											//+0x5558
	int								mPrevMouseY;											//+0x555C
	int								mSunMoney;												//+0x5560
	int								mNumWaves;												//+0x5564
	std::uint64_t					mLevelTimeNs;
	Sexy::UnsignedTimeCounter						mEffectCounter;											//+0x556C（存储 uint；DrawRain 按 1073 有符号取模读取，负分数钳回起点）
	int								mDrawCount;												//+0x5570
	Sexy::TimeCounter								mRiseFromGraveCounter;									//+0x5574
	Sexy::TimeCounter								mOutOfMoneyCounter;										//+0x5578
	int								mCurrentWave;											//+0x557C
	int								mTotalSpawnedWaves;										//+0x5580
	TutorialState					mTutorialState;											//+0x5584
	ParticleSystemID				mTutorialParticleID;									//+0x5588
	Sexy::TimeCounter								mTutorialTimer;											//+0x558C
	int								mLastBungeeWave;										//+0x5590
	int								mZombieHealthToNextWave;								//+0x5594
	int								mZombieHealthWaveStart;									//+0x5598
	Sexy::TimeCounter								mZombieCountDown;										//+0x559C
	Sexy::TimeCounter								mZombieCountDownStart;									//+0x55A0
	Sexy::TimeCounter								mHugeWaveCountDown;										//+0x55A4
	bool							mHelpDisplayed[NUM_ADVICE_TYPES];						//+0x55A8
	AdviceType						mHelpIndex;												//+0x55EC
	bool							mFinalBossKilled;										//+0x55F0
	bool							mShowShovel;											//+0x55F1
	Sexy::TimeCounter								mCoinBankFadeCount;										//+0x55F4
	DebugTextMode					mDebugTextMode;											//+0x55F8
	bool							mLevelComplete;											//+0x55FC
	Sexy::TimeCounter								mBoardFadeOutCounter;									//+0x5600
	Sexy::TimeCounter								mNextSurvivalStageCounter;								//+0x5604
	Sexy::TimeCounter								mScoreNextMowerCounter;									//+0x5608
	bool							mLevelAwardSpawned;										//+0x560C
	int								mProgressMeterWidth;									//+0x5610
	Sexy::TimeCounter								mFlagRaiseCounter;										//+0x5614
	Sexy::TimeCounter								mIceTrapCounter;										//+0x5618
	int								mBoardRandSeed;											//+0x561C
	ParticleSystemID				mPoolSparklyParticleID;									//+0x5620
	ReanimationID					mFwooshID[MAX_GRID_SIZE_Y][12];							//+0x5624
	Sexy::TimeCounter								mFwooshCountDown;										//+0x5744
	Sexy::TimeCounter								mTimeStopCounter;										//+0x5748
	bool							mDroppedFirstCoin;										//+0x574C
	Sexy::TimeCounter								mFinalWaveSoundCounter;									//+0x5750
	Sexy::TimeCounter								mCobCannonCursorDelayCounter;							//+0x5754
	int								mCobCannonMouseX;										//+0x5758
	int								mCobCannonMouseY;										//+0x575C
	bool							mKilledYeti;											//+0x5760
	bool							mMustacheMode;											//+0x5761
	bool							mSuperMowerMode;										//+0x5762
	bool							mFutureMode;											//+0x5763
	bool							mPinataMode;											//+0x5764
	bool							mDanceMode;												//+0x5765
	bool							mDaisyMode;												//+0x5766
	bool							mSukhbirMode;											//+0x5767
	BoardResult						mPrevBoardResult;										//+0x5768
	bool							mPeaShooterUsed;										//+GOTY @Patoke: 0x5784
	bool							mCatapultPlantsUsed;									//+GOTY @Patoke: 0x5785
	int								mLevelCoinsCollected;									//+GOTY @Patoke: 0x5788
	int								mGargantuarsKillsByCornCob;								//+GOTY @Patoke: 0x578C
	bool							mMushroomAndCoffeeBeansOnly;							//+GOTY @Patoke: 0x5790
	bool							mMushroomsUsed;											//+GOTY @Patoke: 0x5791
	int								mTriggeredLawnMowers;									//+0x576C
	Sexy::SecondCounter								mPlayTimeActiveLevel;									//+0x5770
	Sexy::SecondCounter								mPlayTimeInactiveLevel;									//+0x5774
	int								mMaxSunPlants;											//+0x5778
	Sexy::UnsignedMillisecondCounter							mStartDrawTime;											//+0x577C
	Sexy::UnsignedMillisecondCounter							mIntervalDrawTime;										//+0x5780
	int								mIntervalDrawCountStart;								//+0x5784
	float							mMinFPS;												//+0x5788
	Sexy::MillisecondCounter								mPreloadTime;											//+0x578C
	intptr_t						mGameID;												//+0x5790
	int								mGravesCleared;											//+0x5794
	int								mPlantsEaten;											//+0x5798
	int								mPlantsShoveled;										//+0x579C
	int								mCoinsCollected;										//+0x57A0 GOTY @Patoke: 0x57C8
	int								mDiamondsCollected;										//+0x57A4 GOTY @Patoke: 0x57CC
	int								mPottedPlantsCollected;									//+0x57A8
	int								mChocolateCollected;									//+0x57AC
	// SaveRecordMapping pins the persistent members to the versioned wire ABI;
	// native padding and transient timer crossing state never enter a save.

public:
	Board(LawnApp* theApp);
	virtual ~Board();

	void							DisposeBoard();
	int								CountSunBeingCollected();
	void							DrawGameObjects(Graphics* g);
	void							ClearCursor();
	[[nodiscard]] /*inline*/ bool					AreEnemyZombiesOnScreen();
	LawnMower*						FindLawnMowerInRow(int theRow);
//  inline bool						SyncState(DataSync& theDataSync) { /* 未发现 */return true; }
	/*inline*/ void					SaveGame(const std::string& theFileName);
	bool							LoadGame(const std::string& theFileName);
	void							InitLevel();
	void							DisplayAdvice(const SexyString& theAdvice, MessageStyle theMessageStyle, AdviceType theHelpIndex);
	void							StartLevel();
	Plant*							AddPlant(int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType = SeedType::SEED_NONE);
	Projectile*						AddProjectile(int theX, int theY, int theRenderOrder, int theRow, ProjectileType theProjectileType);
	Coin*							AddCoin(int theX, int theY, CoinType theCoinType, CoinMotion theCoinMotion);
	void							RefreshSeedPacketFromCursor();
	ZombieType						PickGraveRisingZombieType();
	ZombieType						PickZombieType(int theZombiePoints, int theWaveIndex, ZombiePicker* theZombiePicker);
	int								PickRowForNewZombie(ZombieType theZombieType);
	/*inline*/ Zombie*				AddZombie(ZombieType theZombieType, int theFromWave);
	void							SpawnZombieWave();
	void							RemoveAllZombies();
	void							RemoveCutsceneZombies();
	void							SpawnZombiesFromGraves();
	PlantingReason					CanPlantAt(int theGridX, int theGridY, SeedType theSeedType);	virtual void					MouseMove(int x, int y);
	virtual void					MouseDrag(int x, int y);
	virtual void					MouseDown(int x, int y, int theClickCount);
	virtual void					MouseUp(int x, int y, int theClickCount);
	virtual void					KeyChar(SexyChar theChar);
	virtual void					KeyUp(KeyCode) {}
	virtual void					KeyDown(KeyCode theKey);
	virtual void					Update();
	void							UpdateLayers();
	virtual void					Draw(Graphics* g);
	void							DrawBackdrop(Graphics* g);
	virtual void					ButtonPress  	(int){}
	virtual void					ButtonDepress	(int){}
	virtual void					ButtonDownTick	(int){}
	virtual void					ButtonMouseEnter(int){}
	virtual void					ButtonMouseLeave(int){}
	virtual void					ButtonMouseMove(int, int, int){}
	/*inline*/ void					AddSunMoney(int theAmount);
	bool							TakeSunMoney(int theAmount);
	/*inline*/ bool					CanTakeSunMoney(int theAmount);
	/*inline*/ void					Pause(bool thePause);
	inline bool						MakeEasyZombieType() { /* 未发现 */return false; }
	void							TryToSaveGame();
	/*inline*/ bool					NeedSaveGame();
	/*inline*/ bool					RowCanHaveZombies(int theRow);
	void							ProcessDeleteQueue();
	bool							ChooseSeedsOnCurrentLevel();
	int								GetNumSeedsInBank();
	/*inline*/ bool					StageIsNight();
	/*inline*/ bool					StageHasPool();
	/*inline*/ bool					StageHas6Rows();
	/*inline*/ bool					StageHasFog();
	/*inline*/ bool					StageIsDayWithoutPool();
	/*inline*/ bool					StageIsDayWithPool();
	bool							StageHasGraveStones();
	int								PixelToGridX(int theX, int theY);
	int								PixelToGridY(int theX, int theY);
	/*inline*/ int					GridToPixelX(int theGridX, int theGridY);
	int								GridToPixelY(int theGridX, int theGridY);
	/*inline*/ int					PixelToGridXKeepOnBoard(int theX, int theY);
	/*inline*/ int					PixelToGridYKeepOnBoard(int theX, int theY);
	void							UpdateGameObjects(std::uint64_t theLevelDeltaNs);
	ReanimTimeDomain					AnimationTimeDomain() const;
	bool							MouseHitTest(int x, int y, HitResult* theHitResult);
	void							MouseDownWithPlant(int x, int y, int theClickCount);
	void							MouseDownWithTool(int x, int y, int theClickCount, CursorType theCursorType);
//	inline void						MouseDownNormal(int x, int y, int theClickCount) { /* 未发现 */; }
	bool							CanInteractWithBoardButtons();
	void							DrawProgressMeter(Graphics* g);
	void							UpdateToolTip();
	Plant*							GetTopPlantAt(int theGridX, int theGridY, PlantPriority thePriority);
	void							GetPlantsOnLawn(int theGridX, int theGridY, PlantsOnLawn* thePlantOnLawn);
	// 植物格桶（2026-10-03 用户方案）：层特化 0=底座(花盆/荷叶) 1=普通
	// 2=南瓜壳 3=飞行(咖啡豆)。sizeof(Board) 受 DAT64 镜像硬校验不可加
	// 成员（见上方注释），桶体在 Board.cpp 函数域 static（条目3/4 缓存
	// 先例）；单 Board 存活期约定下安全。派生索引不进存档，装载/重开后
	// RebuildPlantCellBucket() 重建。
	Plant*							GetCellPlant(int theRow, int theCol, int theLayer);
	Plant*							PlantAtSlot(unsigned int theSlot);
	unsigned int					PlantSlotOf(const Plant* thePlant);
	Plant*							NextCellPlant(const Plant* thePlant);
	void							InsertCellChain(int theRow, int theCol, int theLayer, unsigned int theSlot);
	void							RemoveCellChain(int theRow, int theCol, int theLayer, unsigned int theSlot);
	void							RegisterPlantCellBucket(Plant* thePlant);
	void							UnregisterPlantCellBucket(Plant* thePlant, int theRow, int theOldCol);
	void							MovePlantCellBucket(Plant* thePlant, int theOldRow, int theOldCol);
	void							RebuildPlantCellBucket();
	// 僵尸行段桶（2026-10-04 用户方案，行段形态）：按行聚拢的槽位号升序
	// 紧凑数组 + Boss 侧数组（原查询 Boss 行偏差归零=跨行候选）+ 槽→行内
	// 下标反向指针。迭代序 = 原全池扫（升序槽）按行过滤后的序，读侧免决胜
	// 重排；插入/删除/换行 O(行内) memmove（≤2KB，事件级频率）。桶体在
	// Board.cpp 函数域 static（sizeof(Board) 硬锁同植物桶）；派生索引不进
	// 存档，装载/重开后 RebuildZombieRowBucket() 重建。
	Zombie*							ZombieAtSlot(unsigned int theSlot);
	unsigned int					ZombieSlotOf(const Zombie* theZombie);
	bool							IterateZombiesInRow(int theRow, Zombie*& theZombie);
	// 纯行段（不含侧 Boss；供主动排除 Boss 的裸 row== 查询——用户裁决
	// 割草机/钉耙/土豆灯偏离 1073，见 Board.cpp 注）
	bool							IterateZombiesInRowOnly(int theRow, Zombie*& theZombie);
	// 多行区间（行主序 rLo..rHi 各行段 + 尾吐全部侧 Boss；行过滤交循环体
	// 原样保留的检查——统一候选超集纪律，见 Board.cpp 注）
	bool							IterateZombiesInRowRange(int theRowLo, int theRowHi, Zombie*& theZombie);
	void							RegisterZombieRowBucket(Zombie* theZombie);
	void							RemoveZombieRowBucket(Zombie* theZombie);
	void							MoveZombieRowBucket(Zombie* theZombie, int theOldRow);
	void							RebuildZombieRowBucket();
	/*inline*/ int					CountSunFlowers();
	int								GetSeedPacketPositionX(int theIndex);
	void							AddGraveStones(int theGridX, int theCount, MTRand& theLevelRNG);
	int								GetGraveStoneCount();
	void							ZombiesWon(Zombie* theZombie = nullptr);
	void							DrawLevel(Graphics* g);
	void							DrawShovel(Graphics* g);
	void							UpdateZombieSpawning();
private:
	void							UpdateZombieWaveScheduler();
public:
	void							UpdateSunSpawning();
	/*inline*/ void					ClearAdvice(AdviceType theHelpIndex);
	bool							RowCanHaveZombieType(int theRow, ZombieType theZombieType);
	/*inline*/ int					NumberZombiesInWave(int theWaveIndex);
	int								TotalZombiesHealthInWave(int theWaveIndex);
	void							DrawDebugText(Graphics* g);
	void							DrawUICoinBank(Graphics* g);
	void							FadeOutLevel();
	void							DrawFadeOut(Graphics* g);
	void							DrawIce(Graphics* g, int theGridY);
	bool							IsIceAt(int theGridX, int theGridY);
	/*inline*/ ZombieID				ZombieGetID(Zombie* theZombie);
	/*inline*/ Zombie*				ZombieGet(ZombieID theZombieID);
	[[nodiscard]] /*inline*/ Zombie*				ZombieTryToGet(ZombieID theZombieID);
	void							DrawDebugObjectRects(Graphics* g);
	void							UpdateIce();
	/*inline*/ int					GetIceZPos(int theRow);
	/*inline*/ bool					CanAddBobSled();
	int								CountUntriggerLawnMowers();
	bool							IterateZombies(Zombie*& theZombie);
	bool							IteratePlants(Plant*& thePlant);
	bool							IterateProjectiles(Projectile*& theProjectile);
	bool							IterateCoins(Coin*& theCoin);
	bool							IterateLawnMowers(LawnMower*& theLawnMower);
	bool							IterateParticles(TodParticleSystem*& theParticle);
	bool							IterateReanimations(Reanimation*& theReanimation);
	bool							IterateGridItems(GridItem*& theGridItem);
	/*inline*/ Zombie*				AddZombieInRow(ZombieType theZombieType, int theRow, int theFromWave);
	/*inline*/ bool					IsPoolSquare(int theGridX, int theGridY);
	void							PickZombieWaves();
	void							StopAllZombieSounds();
	/*inline*/ bool					HasLevelAwardDropped();
	void							UpdateProgressMeter(std::uint64_t theDeltaNs);
	void							DrawUIBottom(Graphics* g);
	void							DrawUITop(Graphics* g);
	Zombie*							ZombieHitTest(int theMouseX, int theMouseY);
	void							KillAllPlantsInRadius(int theX, int theY, int theRadius);
	Plant*							GetPumpkinAt(int theGridX, int theGridY);
	Plant*							GetFlowerPotAt(int theGridX, int theGridY);
	static bool						CanZombieSpawnOnLevel(ZombieType theZombieType, int theLevel);
	bool							IsZombieWaveDistributionOk();
	void							PickBackground();
	void							InitZombieWaves();
	void							InitSurvivalStage();
	static /*inline*/ int			MakeRenderOrder(RenderLayer theRenderLayer, int theRow, int theLayerOffset);
	void							UpdateGame(std::uint64_t theDeltaNs);
	void							InitZombieWavesForLevel(int theForLevel);
	unsigned int					SeedNotRecommendedForLevel(SeedType theSeedType);
	void							DrawTopRightUI(Graphics* g);
	void							RefreshHudLayout();
	void							DrawFog(Graphics* g);
	void							UpdateFog();
	/*inline*/ int					LeftFogColumn();
	static /*inline*/ bool			IsZombieTypePoolOnly(ZombieType theZombieType);
	void							DropLootPiece(int thePosX, int thePosY, int theDropFactor);
	void							UpdateLevelEndSequence();
	LawnMower*						GetBottomLawnMower();
	bool							CanDropLoot();
	ZombieType						GetIntroducedZombieType();
	void							PickSpecialGraveStone();
	float							GetPosYBasedOnRow(float thePosX, int theRow);
	void							NextWaveComing();
	bool							BungeeIsTargetingCell(int theGridX, int theGridY);
	/*inline*/ int					PlantingPixelToGridX(int theX, int theY, SeedType theSeedType);
	/*inline*/ int					PlantingPixelToGridY(int theX, int theY, SeedType theSeedType);
	[[nodiscard]] Plant*							FindUmbrellaPlant(int theGridX, int theGridY);
	void							SetTutorialState(TutorialState theTutorialState);
	void							DoFwoosh(int theRow);
	void							UpdateFwoosh();
	Plant*							SpecialPlantHitTest(int x, int y);
	void							UpdateMousePosition();
	/*inline*/ Plant*				ToolHitTestHelper(HitResult* theHitResult);
	/*inline*/ Plant*				ToolHitTest(int theX, int theY);
	bool							CanAddGraveStoneAt(int theGridX, int theGridY);
	void							UpdateGridItems();
	/*inline*/ GridItem*			AddAGraveStone(int theGridX, int theGridY);
	int								GetSurvivalFlagsCompleted();
	bool							HasProgressMeter();
	void							UpdateCursor();
	void							UpdateTutorial();
	SeedType						GetSeedTypeInCursor();
	/*inline*/ int					CountPlantByType(SeedType theSeedType);
	bool							HasValidCobCannonSpot();
	// 基座检查：紫卡（升级种子段 40..47）要求基础植物在场，kUpgradeBasisReqs 表
	// 烘焙检查（实现于 Board.cpp——头文件不留实现，函数指针表须单一副本）；非紫卡
	// 短路直返 true。阳光（CanTakeSunMoney）与冷却（SeedPacket::mActive）是平级
	// 前置，地面规则在 CanPlantAt——均不在此。
	bool							Met(SeedType theSeedType);
	bool							IsValidCobCannonSpot(int theGridX, int theGridY);
	bool							IsValidCobCannonSpotHelper(int theGridX, int theGridY);
	void							MouseDownCobcannonFire(int x, int y, int theClickCount);
	int								KillAllZombiesInRadius(int theRow, int theX, int theY, int theRadius, int theRowRange, bool theBurn, int theDamageRangeFlags); // @Patoke: modified function prototype
	/*inline*/ int					GetSeedBankExtraWidth();
	bool							IsFlagWave(int theWaveNumber);
	void							DrawHouseDoorTop(Graphics* g);
	void							DrawHouseDoorBottom(Graphics* g);
	Zombie*							GetBossZombie();
	bool							HasConveyorBeltSeedBank();
	/*inline*/ bool					StageHasRoof();
	void							SpawnZombiesFromPool();
	void							SpawnZombiesFromSky();
	void							PickUpTool(GameObjectType theObjectType);
	void							TutorialArrowShow(int theX, int theY);
	void							TutorialArrowRemove();
	int								CountCoinsBeingCollected();
	void							BungeeDropZombie(BungeeDropGrid* theBungeeDropGrid, ZombieType theZombieType);
	void							SetupBungeeDrop(BungeeDropGrid* theBungeeDropGrid);
	/*inline*/ void					PutZombieInWave(ZombieType theZombieType, int theWaveNumber, ZombiePicker* theZombiePicker);
	/*inline*/ void					PutInMissingZombies(int theWaveNumber, ZombiePicker* theZombiePicker);
	Rect							GetShovelButtonRect();
	void							GetZenButtonRect(GameObjectType theObjectType, Rect& theRect);
	Plant*							NewPlant(int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType = SeedType::SEED_NONE);
	void							DoPlantingEffects(int theGridX, int theGridY, Plant* thePlant);
	bool							IsFinalSurvivalStage();
	void							SurvivalSaveScore();
	int								CountZombiesOnScreen();
	int								GetLiveGargantuarCount(); // @Patoke: implemented
	/*inline*/ int					GetNumWavesPerSurvivalStage();
	int								GetLevelRandSeed();
	void							AddBossRenderItem(RenderItem* theRenderList, int& theCurRenderItem, Zombie* theBossZombie);
	/*inline*/ GridItem*			GetCraterAt(int theGridX, int theGridY);
	/*inline*/ GridItem*			GetGraveStoneAt(int theGridX, int theGridY);
	/*inline*/ GridItem*			GetLadderAt(int theGridX, int theGridY);
	/*inline*/ GridItem*			AddALadder(int theGridX, int theGridY);
	/*inline*/ GridItem*			AddACrater(int theGridX, int theGridY);
	void							InitLawnMowers();
	/*inline*/ bool					IsPlantInCursor();
	void							HighlightPlantsForMouse(int theMouseX, int theMouseY);
	void							ClearFogAroundPlant(Plant* thePlant, int theSize);
	/*inline*/ void					RemoveParticleByType(ParticleEffect theEffectType);
	/*inline*/ GridItem*			GetScaryPotAt(int theGridX, int theGridY);
	void							PuzzleSaveStreak();
	/*inline*/ void					ClearAdviceImmediately();
	/*inline*/ bool					IsFinalScaryPotterStage();
	/*inline*/ void					DisplayAdviceAgain(const SexyString& theAdvice, MessageStyle theMessageStyle, AdviceType theHelpIndex);
	GridItem*						GetSquirrelAt(int theGridX, int theGridY);
	GridItem*						GetZenToolAt(int theGridX, int theGridY);
	bool							IsPlantInGoldWateringCanRange(int theMouseX, int theMouseY, Plant* thePlant);
	bool							StageHasZombieWalkInFromRight();
	void							PlaceRake();
	GridItem*						GetRake();
	/*inline*/ bool					IsScaryPotterDaveTalking();
	/*inline*/ Zombie*				GetWinningZombie();
	/*inline*/ void					ResetFPSStats();
	int								CountEmptyPotsOrLilies(SeedType theSeedType);
	GridItem*						GetGridItemAt(GridItemType theGridItemType, int theGridX, int theGridY);
	bool							ProgressMeterHasFlags();
	/*inline*/ bool					IsLastStandFinalStage();
	/*inline*/ int					GetNumWavesPerFlag();
	int								GetCurrentPlantCost(SeedType theSeedType, SeedType theImitaterType);
	/*inline*/ bool					PlantUsesAcceleratedPricing(SeedType theSeedType);
	void							FreezeEffectsForCutscene(bool theFreeze);
	void							LoadBackgroundImages();
	bool							CanUseGameObject(GameObjectType theGameObject);
	void							SetMustacheMode(bool theEnableMustache);
	int								CountCoinByType(CoinType theCoinType);
	void							SetSuperMowerMode(bool theEnableSuperMower);
	void							DrawZenWheelBarrowButton(Graphics* g, const Rect& aButtonRect, int theOffsetY);
	void							DrawZenButtons(Graphics* g);
	/*inline*/ void					OffsetYForPlanting(int& theY, SeedType theSeedType);
	void							SetDanceMode(bool theEnableDance);
	void							SetFutureMode(bool theEnableFuture);
	void							SetPinataMode(bool theEnablePinata);
	void							SetDaisyMode(bool theEnableDaisy);
	void							SetSukhbirMode(bool theEnableSukhbir);
	bool							MouseHitTestPlant(int x, int y, HitResult* theHitResult);
	
	/*inline*/ Reanimation*			CreateRakeReanim(float theRakeX, float theRakeY, int theRenderOrder);
	void							CompleteEndLevelSequenceForSaving();
	void							RemoveZombiesForRepick();
	int								GetGraveStonesCount();
	/*inline*/ bool					IsSurvivalStageWithRepick();
	/*inline*/ bool					IsLastStandStageWithRepick();
	void							DoTypingCheck(KeyCode theKey);
	int								CountZombieByType(ZombieType theZombieType);
	bool							CheckForPostGameAchievements();
	static /*inline*/ bool			IsZombieTypeSpawnedOnly(ZombieType theZombieType);
};

int									GetRectOverlap(const Rect& rect1, const Rect& rect2);
bool								GetCircleRectOverlap(int theCircleX, int theCircleY, int theRadius, const Rect& theRect);


