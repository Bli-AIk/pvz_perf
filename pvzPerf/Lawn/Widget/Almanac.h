#pragma once
#include "LawnDialog.h"

// 图鉴"可见"子集计数（有意小于枚举总数：49/53 种植物、26/34 种僵尸——
// 隐藏变身坚果/巨型坚果/苗/左向射手与各 Head/GARGANTUAR 变体等）。语义独立
// 于 NUM_SEED_TYPES/NUM_ZOMBIE_TYPES，不可派生、勿同步
constexpr int NUM_ALMANAC_SEEDS = 49;
constexpr int NUM_ALMANAC_ZOMBIES = 26;

constexpr const float			ALMANAC_PLANT_POSITION_X		= 578.0f;
constexpr const float			ALMANAC_PLANT_POSITION_Y		= 140.0f;
constexpr const float			ALMANAC_ZOMBIE_POSITION_X		= 559.0f;
constexpr const float			ALMANAC_ZOMBIE_POSITION_Y		= 175.0f;
constexpr const int				ALMANAC_INDEXPLANT_POSITION_X	= 167;
constexpr const int				ALMANAC_INDEXPLANT_POSITION_Y	= 255;
constexpr const float			ALMANAC_INDEXZOMBIE_POSITION_X	= 535.0f;
constexpr const float			ALMANAC_INDEXZOMBIE_POSITION_Y	= 215.0f;

class Plant;
class Zombie;
class LawnApp;
class GameButton;
class Reanimation;
class AlmanacDialog : public LawnDialog
{
private:
	enum
	{
		ALMANAC_BUTTON_CLOSE = 0,
		ALMANAC_BUTTON_PLANT = 1,
		ALMANAC_BUTTON_ZOMBIE = 2,
		ALMANAC_BUTTON_INDEX = 3
	};

public:
	LawnApp*					mApp;					//+0x16C
	GameButton*					mCloseButton;			//+0x170
	GameButton*					mIndexButton;			//+0x174
	GameButton*					mPlantButton;			//+0x178
	GameButton*					mZombieButton;			//+0x17C
	AlmanacPage					mOpenPage;				//+0x180
	Reanimation*				mReanim[4];				//+0x184
	SeedType					mSelectedSeed;			//+0x194
	ZombieType					mSelectedZombie;		//+0x198
	Plant*						mPlant;					//+0x19C
	Zombie*						mZombie;				//+0x1A0
	int							mPlantDescScroll;
	int							mPlantDescScrollMax;
	int							mZombieDescScroll;
	int							mZombieDescScrollMax;
	bool						mDescScrollBarDragging;
	
public:
	AlmanacDialog(LawnApp* theApp);
	virtual ~AlmanacDialog();

	void						ClearPlantsAndZombies();
	virtual void				RemovedFromManager(WidgetManager* theWidgetManager);
	void						SetupPlant();
	void						SetupZombie();
	void						SetPage(AlmanacPage thePage);
	virtual void				Update();
	void						DrawIndex(Graphics* g);
	void						DrawPlants(Graphics* g);
	void						DrawZombies(Graphics* g);
	virtual void				Draw(Graphics* g);
	void						GetSeedPosition(SeedType theSeedType, int& x, int& y);
	SeedType					SeedHitTest(int x, int y);
	/*inline*/ bool				ZombieHasSilhouette(ZombieType theZombieType);
	bool						ZombieIsShown(ZombieType theZombieType);
	bool						ZombieHasDescription(ZombieType theZombieType);
	void						GetZombiePosition(ZombieType theZombieType, int& x, int& y);
	ZombieType					ZombieHitTest(int x, int y);
	virtual void				MouseUp(int x, int y, int theClickCount);
    const void* TouchHitTest(int x, int y) override;
	virtual void				MouseDown(int x, int y, int theClickCount);
	virtual void				MouseDrag(int x, int y);
	virtual void				MouseWheel(int theDelta);
	inline Rect					GetDescRect() const
	{
		if (mOpenPage == ALMANAC_PAGE_PLANTS)
			return Rect(485, 309, 258, 205);
		return Rect(484, mSelectedZombie == ZombieType::ZOMBIE_ZAMBONI ? 372 : 377, 258, 170);
	}
	int&						DescScrollRef();
	inline int					DescScrollMax() const
	{
		return mOpenPage == ALMANAC_PAGE_PLANTS ? mPlantDescScrollMax : mZombieDescScrollMax;
	}
//	virtual void				KeyChar(char theChar);

	static inline ZombieType	GetZombieType(int theIndex)
	{
		return theIndex < NUM_ZOMBIE_TYPES ? (ZombieType)theIndex : ZOMBIE_INVALID;
	}
	/*inline*/ void				ShowPlant(SeedType theSeedType);
	/*inline*/ void				ShowZombie(ZombieType theZombieType);
};

/*inline*/ void					AlmanacInitForPlayer();
/*inline*/ void					AlmanacPlayerDefeatedZombie(ZombieType theZombieType);

