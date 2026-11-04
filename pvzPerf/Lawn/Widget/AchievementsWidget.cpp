// @Patoke: implement file
#include "AchievementsWidget.h"
#include "Lawn/Board.h"
#include "GameButton.h"
#include "GameSelector.h"
#include "LawnApp.h"
#include "Almanac.h"
#include "Resources.h"
#include "Lawn/System/Music.h"
#include "GameConstants.h"
#include "Lawn/System/PlayerInfo.h"
#include "Lawn/System/ProfileMgr.h"
#include "Sexy.TodLib/TodFoley.h"
#include "Sexy.TodLib/TodDebug.h"
#include "Sexy.TodLib/TodCommon.h"
#include "graphics/Font.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/TodParticle.h"
#include "widget/Dialog.h"
#include "widget/WidgetManager.h"
#include <algorithm>
#include "Sexy.TodLib/generated/SceneAtlasData.h"
#include "graphics/AtlasDraw.h"

namespace
{
using AchievementSprite = AchievementsAtlas::Sprite;
static_assert(AchievementsAtlas::IconCount == MAX_ACHIEVEMENTS,
	"Achievement icons need one authored rectangle per achievement");
constexpr int kBackgroundExtension = 300;
constexpr int kBackgroundWidth = BOARD_WIDTH + 2 * kBackgroundExtension;
const Rect aBackButtonRect = { 120, 35, 130, 80 };
}  // namespace

AchievementItem gAchievementList[MAX_ACHIEVEMENTS] = {
	{ "Home Lawn Security", "Complete Adventure Mode." },
	{ "Nobel Peas Prize", "Get the golden sunflower trophy." },
	{ "Better Off Dead", "Get to a streak of 10 in I, Zombie Endless" },
	{ "China Shop", "Get to a streak of 15 in Vasebreaker Endless" },
	{ "SPUDOW!", "Blow up a zombie using a Potato Mine." },
	{ "Explodonator", "Take out 10 full-sized zombies with a single Cherry Bomb." },
	{ "Morticulturalist", "Collect all 49 plants (including plants from Crazy Dave's shop)." },
	{ "Don't Pea in the Pool", "Complete a daytime pool level without using pea shooters of any kind." },
	{ "Roll Some Heads", "Bowl over 5 zombies with a single Wall-Nut." },
	{ "Grounded", "Defeat a normal roof level without using any catapult plants." },
	{ "Zombologist", "Discover the Yeti zombie." },
	{ "Penny Pincher", "Pick up 30 coins in a row on a single level without letting any disappear." },
	{ "Sunny Days", "Get 8000 sun during a single level." },
	{ "Popcorn Party", "Defeat 2 Gargantuars with Corn Cob missiles in a single level." },
	{ "Good Morning", "Complete a daytime level by planting only Mushrooms and Coffee Beans." },
	{ "No Fungus Among Us", "Complete a nighttime level without planting any Mushrooms." },
	{ "Beyond the Grave", "Beat all 20 mini games." },
	{ "Immortal", "Survive 20 waves of pure zombie ferocity." },
	{ "Towering Wisdom", "Grow the Tree of Wisdom to 100 feet." },
	{ "Mustache Mode", "Enable Mustache Mode" }
};

// GOTY @Patoke: 0x401000
AchievementsWidget::AchievementsWidget(LawnApp* theApp) {
	mApp = theApp;
	mWidth = BOARD_WIDTH;
	mClip = false;
	mMouseInsets.mLeft = -kBackgroundExtension;
	mMouseInsets.mRight = -kBackgroundExtension;
	mHeight = AchievementsAtlas::Images[static_cast<unsigned>(AchievementSprite::ACHEESEMENTS_CHINA)].mHeight
		+ AchievementsAtlas::Images[static_cast<unsigned>(AchievementSprite::SELECTORSCREEN_ACHIEVEMENTS_BG)].mHeight + 15700;
	mScrollDirection = -1;
	mScrollValue = 0;
	mDefaultScrollValue = 3000.0f;
	mScrollDecay = 10000.0f;
	mScrollRemainderY = 0.0f;
	mDidPressMoreButton = false;
	const TextureRegion& aRock=AchievementsAtlas::Images[static_cast<unsigned>(AchievementSprite::ACHEESEMENTS_MORE_ROCK)];
	mMoreRockRect = Rect(710,470,aRock.mWidth-25,aRock.mHeight-50);
}

// GOTY @Patoke: 0x4010E0
AchievementsWidget::~AchievementsWidget() {

}

// GOTY @Patoke: 0x401A10
void AchievementsWidget::Update() {
    if(mTouchScroll.pressed || mTouchScroll.velocity!=0) {
        mTouchScroll.Advance(ScrollTimeNs(),2*mApp->mHeight+50-mHeight,static_cast<float>(mApp->mHeight));
        Move(mX,FloatRoundToInt(mTouchScroll.position));MarkDirty();return;
    }
	if (mScrollValue <= 0)
		return;

	const float aSeconds = static_cast<float>(Sexy::gUpdateIntervalNs) * 1.0e-9f;
	mScrollValue = std::max(0.0f, std::min(mScrollValue, mDefaultScrollValue) - mScrollDecay * aSeconds);
	const float aDisplacement = mScrollRemainderY + mScrollValue * mScrollDirection * aSeconds;
	const int aPixels = FloatRoundToInt(aDisplacement);
	mScrollRemainderY = aDisplacement - aPixels;
	int aNewY = mY + aPixels;
	if (aNewY >= mApp->mHeight)
	{
		aNewY = mApp->mHeight;
		mScrollRemainderY = 0.0f;
	}

	int aMaxScroll = 2 * mApp->mHeight + 50 - mHeight;
	if (aNewY <= aMaxScroll)
	{
		aNewY = aMaxScroll;
		mScrollRemainderY = 0.0f;
	}

	// Mouse events and hit rectangles are already in this widget's local frame.
	Move(mX, aNewY);
}

// GOTY @Patoke: 0x401160
void AchievementsWidget::Draw(Graphics* g) {
	const TextureSource aTexture{AchievementsAtlas::Texture, 0};
	g->ClipRect(-kBackgroundExtension, 0, kBackgroundWidth, mHeight);
	if (g->mClipRect.mWidth <= 0 || g->mClipRect.mHeight <= 0)
		return;

	const Point anAbsolutePosition = GetAbsPos();
	Point aMousePosition(
		mWidgetManager->mLastMouseX - anAbsolutePosition.mX,
		mWidgetManager->mLastMouseY - anAbsolutePosition.mY);
    if(!mWidgetManager->mMouseHoverEnabled) {
        aMousePosition=Point(-1,-1);
        if(mTouchScroll.pressed && !mTouchScroll.dragging)
            for(const auto& touch:mWidgetManager->mTouches) if(touch.widget==this && touch.event.id==mTouchScroll.touchId) {
                const Point held(touch.event.x-anAbsolutePosition.mX,touch.event.y-anAbsolutePosition.mY);
                if((aBackButtonRect.Contains(mTouchPressX,mTouchPressY)&&aBackButtonRect.Contains(held)) ||
                   (mMoreRockRect.Contains(mTouchPressX,mTouchPressY)&&mMoreRockRect.Contains(held))) aMousePosition=held;
            }
    }

	// Opaque underpaint also covers the original artwork's antialiased edges.
	g->mColor = Color(55, 37, 19);
	g->FillRect(-kBackgroundExtension, 0, kBackgroundWidth, mHeight);
	DrawAtlasImage(g,aTexture,AchievementsAtlas::Images[static_cast<unsigned>(AchievementSprite::SELECTORSCREEN_ACHIEVEMENTS_BG)],-kBackgroundExtension,0);

	int aHeight = AchievementsAtlas::Images[static_cast<unsigned>(AchievementSprite::SELECTORSCREEN_ACHIEVEMENTS_BG)].mHeight;
	for (int i = 1; i <= 70; i++)
		DrawAtlasImage(g,aTexture,AchievementsAtlas::Images[static_cast<unsigned>(AchievementSprite::ACHEESEMENTS_HOLE_TILE)],-kBackgroundExtension,aHeight*i);

	DrawAtlasImage(g,aTexture,AchievementsAtlas::Images[static_cast<unsigned>(AchievementSprite::ACHEESEMENTS_BOOKWORM)],-kBackgroundExtension,1125);
	DrawAtlasImage(g,aTexture,AchievementsAtlas::Images[static_cast<unsigned>(AchievementSprite::ACHEESEMENTS_BEJEWELED)],-kBackgroundExtension,2250);
	DrawAtlasImage(g,aTexture,AchievementsAtlas::Images[static_cast<unsigned>(AchievementSprite::ACHEESEMENTS_CHUZZLE)],-kBackgroundExtension,4500);
	DrawAtlasImage(g,aTexture,AchievementsAtlas::Images[static_cast<unsigned>(AchievementSprite::ACHEESEMENTS_PEGGLE)],-kBackgroundExtension,6750);
	DrawAtlasImage(g,aTexture,AchievementsAtlas::Images[static_cast<unsigned>(AchievementSprite::ACHEESEMENTS_PIPE)],-kBackgroundExtension,9000);
	DrawAtlasImage(g,aTexture,AchievementsAtlas::Images[static_cast<unsigned>(AchievementSprite::ACHEESEMENTS_ZUMA)],-kBackgroundExtension,11250);

	const TextureRegion& aChina=AchievementsAtlas::Images[static_cast<unsigned>(AchievementSprite::ACHEESEMENTS_CHINA)];
	DrawAtlasImage(g,aTexture,aChina,-kBackgroundExtension,mHeight-aChina.mHeight-50);
	
	if (aBackButtonRect.Contains(aMousePosition))
		DrawAtlasImage(g,aTexture,AchievementsAtlas::Images[static_cast<unsigned>(AchievementSprite::ACHEESEMENTS_BACK_HIGHLIGHT)],128,55);

	for (int i = 0; i < MAX_ACHIEVEMENTS; i++) {
		bool aHasAchievement;
		if (mApp->mPlayerInfo) aHasAchievement = mApp->mPlayerInfo->mEarnedAchievements[i];
		else aHasAchievement = false;

		int aCurrAchievementOff = 57 * int(i / 2);
		int aImageXPos = i % 2 == 0 ? 120 : 410;
		int aImageYPos = 178 + aCurrAchievementOff;
		int aTextXPos = aImageXPos + 70;
		int aTextYPos = aImageYPos + 16;

		// Achievement images
		const TextureRegion& aIcon=AchievementsAtlas::Icons[i];
		const Rect aSrcRect(0,0,aIcon.mWidth,aIcon.mHeight);
		Rect aDestRect(aImageXPos, aImageYPos, 56, 56);
		
		g->mColorizeImages = true;
		g->mColor = aHasAchievement ? Color(255, 255, 255) : Color(255, 255, 255, 32);

		DrawAtlasImage(g,aTexture,aIcon,aDestRect,aSrcRect);
		g->mColorizeImages = false;
		
		// Achievement titles
		g->mFont = FONT_DWARVENTODCRAFT15;
		g->mColor = Color(21, 175, 0);

		g->DrawString(gAchievementList[i].name, aTextXPos, aTextYPos);

		// Achievement descriptions	
		Rect aPos = Rect(aTextXPos, aTextYPos + 3, 212, 60);
		
		g->mFont = FONT_DWARVENTODCRAFT12;
		g->mColor = Color(255, 255, 255);

		g->WriteWordWrapped(aPos, gAchievementList[i].description, 12);
	}

	DrawAtlasImage(g,aTexture,AchievementsAtlas::Images[static_cast<unsigned>(AchievementSprite::ACHEESEMENTS_MORE_ROCK)],700,450);

	bool aIsHighlight = mMoreRockRect.Contains(aMousePosition);
	if (mDidPressMoreButton) {
		const auto aButton=aIsHighlight ? AchievementSprite::ACHEESEMENTS_TOP_BUTTON_HIGHLIGHT : AchievementSprite::ACHEESEMENTS_TOP_BUTTON;
		DrawAtlasImage(g,aTexture,AchievementsAtlas::Images[static_cast<unsigned>(aButton)],700,450);
	}
	else {
		const auto aButton=aIsHighlight ? AchievementSprite::ACHEESEMENTS_MORE_BUTTON_HIGHLIGHT : AchievementSprite::ACHEESEMENTS_MORE_BUTTON;
		DrawAtlasImage(g,aTexture,AchievementsAtlas::Images[static_cast<unsigned>(aButton)],700,450);
	}
}

// GOTY @Patoke: 0x4019D0
void AchievementsWidget::KeyDown(KeyCode theKey) {
    mTouchScroll.Cancel();
	if (theKey == KEYCODE_UP) {
		mScrollValue = mDefaultScrollValue;
		mScrollDirection = 1;
	}
	else if (theKey == KEYCODE_DOWN) {
		mScrollValue = mDefaultScrollValue;
		mScrollDirection = -1;
	}
}

// GOTY @Patoke: 0x4017F0
void AchievementsWidget::MouseDown(int x, int y, int theClickCount) {
	(void)theClickCount;
	if (aBackButtonRect.Contains(x, y))
		mApp->PlaySample(SOUND_GRAVEBUTTON);

	if (mMoreRockRect.Contains(x, y))
		mApp->PlaySample(SOUND_GRAVEBUTTON);
}

// GOTY @Patoke: 0x401890
void AchievementsWidget::MouseUp(int x, int y, int theClickCount) {
	(void)theClickCount;
	Point aPos = Point(x, y);
	if (aBackButtonRect.Contains(aPos)) {
        mTouchScroll.Cancel();
		mScrollValue = 0;
		mDidPressMoreButton = false;
		Move(mX, mApp->mHeight);
		mScrollRemainderY = 0.0f;
		mApp->mGameSelector->SlideTo(-mX, 0);
		mApp->mGameSelector->mWidgetManager->SetFocus(mApp->mGameSelector);
		return;
	}

	if (mMoreRockRect.Contains(aPos)) {
        mTouchScroll.Cancel();
		mDidPressMoreButton = !mDidPressMoreButton;
		mScrollDirection = mDidPressMoreButton ? -1 : 1;
		mScrollValue = 2000.0f;
	}
}

void AchievementsWidget::MouseMove(int x, int y) {
	const bool aIsOver = aBackButtonRect.Contains(x, y) || mMoreRockRect.Contains(x, y);
	ShowFinger(aIsOver);
}

void AchievementsWidget::MouseLeave() {
	ShowFinger(false);
}

// GOTY @Patoke: 0x4019A0
void AchievementsWidget::MouseWheel(int theDelta) {
    mTouchScroll.Cancel();
	if (theDelta == 0)
		return;
	mScrollValue = mDefaultScrollValue;

	if (theDelta > 0)
		mScrollDirection = 1;
	else if (theDelta < 0)
		mScrollDirection = -1;
}

const void* AchievementsWidget::TouchHitTest(int,int) { return this; }
void AchievementsWidget::TouchBegan(const TouchEvent& event) {
    if(mTouchScroll.pressed)return;
    const auto origin=GetAbsPos();
    mScrollValue=0;mScrollRemainderY=0;mTouchScroll.position=static_cast<float>(mY);
    mTouchPressX=event.x;mTouchPressY=event.y;
    mTouchScroll.Begin(event.id,event.x+origin.mX,event.y+origin.mY,ScrollTimeNs());
    MouseDown(event.x,event.y,1);
}
void AchievementsWidget::TouchMoved(const TouchEvent& event) {
    if(!mTouchScroll.pressed || mTouchScroll.touchId!=event.id)return;
    const auto origin=GetAbsPos();
    mTouchScroll.Move(event.id,event.x+origin.mX,event.y+origin.mY,ScrollTimeNs(),
                      2*mApp->mHeight+50-mHeight,static_cast<float>(mApp->mHeight));
    Move(mX,FloatRoundToInt(mTouchScroll.position));MarkDirty();
}
void AchievementsWidget::TouchEnded(const TouchEvent& event) {
    if(!mTouchScroll.pressed || mTouchScroll.touchId!=event.id)return;
    TouchMoved(event);
    const bool moved=mTouchScroll.End(event.id,ScrollTimeNs());
    if(!moved && ((aBackButtonRect.Contains(mTouchPressX,mTouchPressY)&&aBackButtonRect.Contains(event.x,event.y)) ||
                 (mMoreRockRect.Contains(mTouchPressX,mTouchPressY)&&mMoreRockRect.Contains(event.x,event.y))))
        MouseUp(event.x,event.y,1);
}
void AchievementsWidget::TouchCanceled(const TouchEvent& event) {
    if(mTouchScroll.pressed && mTouchScroll.touchId==event.id)mTouchScroll.Cancel();
}

// GOTY @Patoke: 0x459670
bool ReportAchievement::GiveAchievement(LawnApp* theApp, int theAchievement, bool theForceGive) {
	if (!theApp || !theApp->mPlayerInfo ||
		theAchievement < 0 || theAchievement >= MAX_ACHIEVEMENTS)
		return false;

	if (theApp->mPlayerInfo->mEarnedAchievements[theAchievement])
		return false;

	theApp->mPlayerInfo->mEarnedAchievements[theAchievement] = true;

	// GOTY 1.2.0.1073 records menu-time reconciliation without trying to
	// display board advice. Immediate in-level awards also mark the popup as
	// shown so the award screen does not present the same achievement again.
	if (!theForceGive || !theApp->mBoard)
		return true;
	theApp->mPlayerInfo->mShownAchievements[theAchievement] = true;

	std::string aAchievementName = gAchievementList[theAchievement].name;
	aAchievementName.append(" Achievement!");

	theApp->mBoard->DisplayAdvice(aAchievementName, MESSAGE_STYLE_ACHIEVEMENT, AdviceType::ADVICE_NONE);
	theApp->PlaySample(SOUND_ACHIEVEMENT);

	return true;
}

// GOTY @Patoke: 0x44D5B0
void ReportAchievement::AchievementInitForPlayer(GameSelector* theSelector) {
	if (!theSelector->mApp || !theSelector->mApp->mPlayerInfo)
		return;

	if (theSelector->mApp->HasFinishedAdventure()) {
		GiveAchievement(theSelector->mApp, AchievementId::HomeSecurity, true);
	}

	if (theSelector->mApp->EarnedGoldTrophy()) {
		GiveAchievement(theSelector->mApp, AchievementId::NovelPeasPrize, true);
	}

	if (theSelector->mApp->CanSpawnYetis()) {
		GiveAchievement(theSelector->mApp, AchievementId::Zombologist, true);
	}

	int aTreeSize = theSelector->mApp->mPlayerInfo->mChallengeRecords[(int)GameMode::GAMEMODE_TREE_OF_WISDOM];  // 存档 v13 直下标
	if (aTreeSize >= 100) {
		GiveAchievement(theSelector->mApp, AchievementId::ToweringWisdom, true);
	}

	bool aGiveAchievement = true;
	for (int aSeedType = SEED_GATLINGPEA; aSeedType <= SEED_IMITATER; aSeedType++) {
		if (!theSelector->mApp->SeedTypeAvailable(SeedType(aSeedType)))
			aGiveAchievement = false;
	}

	if (aGiveAchievement) {
		GiveAchievement(theSelector->mApp, AchievementId::Morticulturalist, aGiveAchievement);
	}
}
