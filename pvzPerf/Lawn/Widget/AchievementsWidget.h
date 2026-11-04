#pragma once
// @Patoke: implement file

#include "ConstEnums.h"
#include "widget/Widget.h"
#include "widget/InertialScroll.h"

class LawnApp;
class GameSelector;

using namespace Sexy;

enum AchievementId {
	HomeSecurity, //
	NovelPeasPrize, //
	BetterOffDead, //
	ChinaShop, //
	Spudow, //
	Explodonator, //
	Morticulturalist, //
	DontPea, //
	RollSomeHeads, //
	Grounded, //
	Zombologist, //
	PennyPincher, //
	SunnyDays, //
	PopcornParty, //
	GoodMorning, //
	NoFungusAmongUs, //
	BeyondTheGrave, //
	Immortal, //
	ToweringWisdom, //
	MustacheMode, //
    MAX_ACHIEVEMENTS
};

// todo @Patoke: add these
class AchievementItem {
public:
    std::string name;
    std::string description;
};

extern AchievementItem gAchievementList[MAX_ACHIEVEMENTS];

class AchievementsWidget : public Widget {
public:
    InertialScroll mTouchScroll;
    int mTouchPressX = 0, mTouchPressY = 0;
    const void* TouchHitTest(int x, int y) override;
    void TouchBegan(const TouchEvent& event) override;
    void TouchMoved(const TouchEvent& event) override;
    void TouchEnded(const TouchEvent& event) override;
    void TouchCanceled(const TouchEvent& event) override;
	LawnApp*	mApp;                       //+GOTY @Patoke: 0xA8
	int			mScrollDirection;			//+GOTY @Patoke: 0xAC
	Rect		mMoreRockRect;				//+GOTY @Patoke: 0xC0
	float		mScrollValue;             // Pixels per second.
	float		mScrollDecay;             // Pixels per second squared.
	float		mDefaultScrollValue;      // Maximum pixels per second.
	float		mScrollRemainderY;        // Subpixel displacement for the integer widget position.
	bool		mDidPressMoreButton;		//+GOTY @Patoke: 0xBC

	AchievementsWidget(LawnApp* theApp);
	virtual ~AchievementsWidget();

	virtual void                Update();
	virtual void                Draw(Graphics* g);
	virtual void                KeyDown(KeyCode theKey);
	virtual void                MouseDown(int x, int y, int theClickCount);
	virtual void                MouseUp(int x, int y, int theClickCount);
	virtual void                MouseMove(int x, int y);
	virtual void                MouseLeave();
	virtual void				MouseWheel(int theDelta);
};

class ReportAchievement {
public:
	static bool GiveAchievement(LawnApp* theApp, int theAchievement, bool theForceGive);
	static void AchievementInitForPlayer(GameSelector* theSelector);
};

