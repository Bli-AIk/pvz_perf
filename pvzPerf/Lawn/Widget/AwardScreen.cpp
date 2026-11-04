#include "Lawn/Plant.h"
#include "Lawn/Board.h"
#include "GameButton.h"
#include "StoreScreen.h"
#include "AwardScreen.h"
#include "Lawn/ZenGarden.h"
#include "Lawn/SeedPacket.h"
#include "LawnApp.h"
#include "Almanac.h"
#include "Lawn/System/Music.h"
#include "Resources.h"
#include "GameConstants.h"
#include "Lawn/System/PlayerInfo.h"
#include "Sexy.TodLib/TodFoley.h"
#include "Sexy.TodLib/TodCommon.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "AchievementsWidget.h"
#include "Sexy.TodLib/generated/SceneAtlasData.h"
#include "graphics/AtlasDraw.h"

namespace
{
void RefreshAwardLayout(AwardScreen& theScreen)
{
	const Rect& aBounds = theScreen.mApp->mScreenBounds;
	theScreen.Move(aBounds.mX + (aBounds.mWidth - BOARD_WIDTH) / 2,
		aBounds.mY + (aBounds.mHeight - BOARD_HEIGHT) / 2);
	theScreen.mMenuButton->mX = aBounds.mX + aBounds.mWidth - theScreen.mX
		- 12 - theScreen.mMenuButton->mWidth;
	theScreen.mMouseInsets.mLeft = std::min(0, aBounds.mX - theScreen.mX);
	theScreen.mMouseInsets.mRight = std::min(0,
		theScreen.mX + theScreen.mWidth - aBounds.mX - aBounds.mWidth);
}

void DrawAwardBackground(Graphics* g, Image* theImage, const AwardScreen& theScreen)
{
	Rect aBounds = theScreen.mApp->mScreenBounds;
	aBounds.Offset(-theScreen.mX, -theScreen.mY);
	const int aBorder = 16;
	const int aPadding = (theImage->mWidth - BOARD_WIDTH) / 2;
	const int aSourceX = aPadding + aBounds.mX + aBorder;
	Graphics aBackground(*g);
	aBackground.ClipRect(aBounds);
	// Native center crops retain the authored UI; the outer frame follows the viewport.
	// AwardScreen_Back uses the original brown wooden border.
	aBackground.DrawImage(theImage, aBounds.mX, 0,
		Rect(0, 0, aBorder, theImage->mHeight));
	aBackground.DrawImage(theImage, aBounds.mX + aBorder, 0,
		Rect(aSourceX, 0, aBounds.mWidth - 2 * aBorder, theImage->mHeight));
	aBackground.DrawImage(theImage, aBounds.mX + aBounds.mWidth - aBorder, 0,
		Rect(theImage->mWidth - aBorder, 0, aBorder, theImage->mHeight));
}
}

//0x405780
// GOTY @Patoke: 0x4063E0
AwardScreen::AwardScreen(LawnApp* theApp, AwardType theAwardType, bool theShowingAchievements)
{
	mApp = theApp;
	mClip = false;
	mFadeInCounter = 180;
	mAwardType = theAwardType;
	mShowingAchievements = false;
	mShowStartButtonAfterAchievements = false;
	mShowMenuButtonAfterAchievements = false;
	mAchievementAnimTime = 0;
	TodLoadResources(ResourceGroupId::DelayLoad_AwardScreen);

	if (theShowingAchievements) {
		for (int i = 0; i < MAX_ACHIEVEMENTS; i++) {
			if (mApp->mPlayerInfo->mEarnedAchievements[i] && !mApp->mPlayerInfo->mShownAchievements[i]) {
				mApp->mPlayerInfo->mShownAchievements[i] = true;

				AchievementScreenItem aAchievementItem{};
				aAchievementItem.mId = i;
				aAchievementItem.mStartAnimTime = 100 * mAchievementItems.size() + 150;
				aAchievementItem.mEndAnimTime = aAchievementItem.mStartAnimTime + 100;
				aAchievementItem.mStartY = 750;
				aAchievementItem.mY = aAchievementItem.mStartY;
				mAchievementItems.push_back(aAchievementItem);
			}
		}

		if (!mAchievementItems.empty()) {
			mShowingAchievements = true;
			TodLoadResources(ResourceGroupId::DelayLoad_ChallengeScreen);
			//int aDestY = 284 - ((76 * mAchievementItems.size()) >> 1);
			int aDestY = 284 - ((76 * mAchievementItems.size()) / 2);

			for (size_t j = 0; j < mAchievementItems.size(); j++) {
				mAchievementItems[j].mDestY = aDestY;
				aDestY += 76;
			}

			mApp->WriteCurrentUserConfig();
		}
	}

	int aLevel = mApp->mPlayerInfo->mLevel;
	if (mAwardType == AWARD_CREDITS_ZOMBIENOTE)
	{
		TodLoadResources(ResourceGroupId::DelayLoad_Background6);
		TodLoadResources(ResourceGroupId::DelayLoad_ZombieNote);
		TodLoadResources(ResourceGroupId::DelayLoad_Credits);
	}
	else if (mAwardType == AWARD_HELP_ZOMBIENOTE)
	{
		TodLoadResources(ResourceGroupId::DelayLoad_Background1);
		TodLoadResources(ResourceGroupId::DelayLoad_ZombieNote);
		TodLoadResources(ResourceGroupId::DelayLoad_ZombieNoteHelp);
	}
	else if (mApp->IsAdventureMode())
	{
		// B 形态 -> 常量表 kAwardNoteResources（原 5 支链合并；下标平移
		// aLevel/10-1，仅 10 的倍数命中；非倍数/越界 = 原链不匹配不加载）
			static constexpr struct { ResourceGroupId mBg; ResourceGroupId mNote; ResourceGroupId mNoteVariant; } kAwardNoteResources[] = {
				{ ResourceGroupId::DelayLoad_Background1, ResourceGroupId::DelayLoad_ZombieNote, ResourceGroupId::DelayLoad_ZombieNote1 },      // 10
				{ ResourceGroupId::DelayLoad_Background2, ResourceGroupId::DelayLoad_ZombieNote, ResourceGroupId::DelayLoad_ZombieNote2 },      // 20
				{ ResourceGroupId::DelayLoad_Background1, ResourceGroupId::DelayLoad_ZombieNote, ResourceGroupId::DelayLoad_ZombieNote3 },      // 30
				{ ResourceGroupId::DelayLoad_Background2, ResourceGroupId::DelayLoad_ZombieNote, ResourceGroupId::DelayLoad_ZombieNote4 },      // 40
				{ ResourceGroupId::DelayLoad_Background1, ResourceGroupId::DelayLoad_ZombieNote, ResourceGroupId::DelayLoad_ZombieFinalNote },  // 50
			};
		static_assert(sizeof(kAwardNoteResources) / sizeof(kAwardNoteResources[0]) == 5,
			"kAwardNoteResources 槽位须与 10..50 纸条关卡段同步");

		const int aNoteIndex = aLevel / 10 - 1;
		if (aLevel % 10 == 0 && (unsigned)aNoteIndex < sizeof(kAwardNoteResources) / sizeof(kAwardNoteResources[0]))
		{
			TodLoadResources(kAwardNoteResources[aNoteIndex].mBg);
			TodLoadResources(kAwardNoteResources[aNoteIndex].mNote);
			TodLoadResources(kAwardNoteResources[aNoteIndex].mNoteVariant);
		}
	}

	mStartButton = new GameButton(AwardScreen::AwardScreen_Start);
	mStartButton->mNormalRegion = CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::SEEDCHOOSER_BUTTON)];
	mStartButton->mOverImage = nullptr;
	mStartButton->mDownImage = nullptr;
	mStartButton->mDisabledRegion = CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::SEEDCHOOSER_BUTTON_DISABLED)];
	mStartButton->SetFont(Sexy::FONT_DWARVENTODCRAFT15);
	mStartButton->mColors[ButtonWidget::COLOR_LABEL] = Color(213, 159, 43);
	mStartButton->mColors[ButtonWidget::COLOR_LABEL_HILITE] = Color(213, 159, 43);
	mStartButton->mX = 324;
	mStartButton->mY = 500;
	mStartButton->mWidth = 156;
	mStartButton->mHeight = 42;
	mStartButton->mTextOffsetY = -1;
	mStartButton->mParentWidget = this;

	// @Patoke: implemented
	mContinueButton = new GameButton(AwardScreen::AwardScreen_Start);
	mContinueButton->mNormalRegion = CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::SEEDCHOOSER_BUTTON)];
	mContinueButton->mOverImage = nullptr;
	mContinueButton->mDownImage = nullptr;
	mContinueButton->mDisabledRegion = CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::SEEDCHOOSER_BUTTON_DISABLED)];
	mContinueButton->SetFont(Sexy::FONT_DWARVENTODCRAFT15);
	mContinueButton->mColors[ButtonWidget::COLOR_LABEL] = Color(213, 159, 43);
	mContinueButton->mColors[ButtonWidget::COLOR_LABEL_HILITE] = Color(213, 159, 43);
	mContinueButton->mX = 324;
	mContinueButton->mY = 515;
	mContinueButton->mWidth = 156;
	mContinueButton->mHeight = 42;
	mContinueButton->mParentWidget = this;
	mContinueButton->mTextOffsetY = -1;
	mContinueButton->mBtnNoDraw = true;
	mContinueButton->mDisabled = true;

	mMenuButton = new GameButton(AwardScreen::AwardScreen_Menu);
	mMenuButton->mLabel = gStr.AWARD_MAIN_MENU_BUTTON;
	mMenuButton->mNormalRegion = CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::SEEDCHOOSER_BUTTON2)];
	mMenuButton->mOverRegion = CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::SEEDCHOOSER_BUTTON2_GLOW)];
	mMenuButton->mDownImage = nullptr;
	mMenuButton->SetFont(Sexy::FONT_BRIANNETOD12);
	mMenuButton->mColors[ButtonWidget::COLOR_LABEL] = Color(42, 42, 90);
	mMenuButton->mColors[ButtonWidget::COLOR_LABEL_HILITE] = Color(42, 42, 90);
	mMenuButton->mParentWidget = this;
	mMenuButton->mX = 677;
	mMenuButton->mY = 16;
	mMenuButton->mWidth = 111;
	mMenuButton->mHeight = 26;
	mMenuButton->mTextOffsetY = 1;
	if (!mApp->HasFinishedAdventure() && aLevel <= 3)
	{
		mMenuButton->mBtnNoDraw = true;
		mMenuButton->mDisabled = true;
	}

	if (mAwardType == AWARD_CREDITS_ZOMBIENOTE)
	{
		mStartButton->mLabel = gStr.ROLL_CREDITS;
		mStartButton->mButtonImage = Sexy::IMAGE_CREDITS_PLAYBUTTON;
		// Credits uses the standalone music-note image, not the seedchooser atlas.
		mStartButton->mNormalRegion = {};
		mStartButton->mDisabledRegion = {};
		mStartButton->mOverImage = nullptr;
		mStartButton->mDownImage = nullptr;
		mStartButton->mDisabledImage = nullptr;
		mStartButton->mOverOverlayImage = nullptr;
		mStartButton->SetFont(Sexy::FONT_HOUSEOFTERROR20);
		mStartButton->mColors[ButtonWidget::COLOR_LABEL] = Color(255, 255, 255);
		mStartButton->mColors[ButtonWidget::COLOR_LABEL_HILITE] = Color(213, 159, 43);
		mStartButton->mX = 325;
		mStartButton->mY = 505;
		mStartButton->mWidth = 190;
		mStartButton->mHeight = 73;
		mStartButton->mTextOffsetX = 33;
		mStartButton->mTextOffsetY = -2;
		mStartButton->mButtonOffsetX = -2;
		mStartButton->mButtonOffsetY = 8;
		mStartButton->mParentWidget = this;
	}
	else if (mAwardType == AWARD_HELP_ZOMBIENOTE)
	{
		mStartButton->mLabel = gStr.MAIN_MENU_BUTTON;
		mMenuButton->mBtnNoDraw = true;
		mMenuButton->mDisabled = true;
	}
	else if (!mApp->IsAdventureMode())
	{
		mStartButton->mLabel = gStr.MAIN_MENU_BUTTON;
		mMenuButton->mBtnNoDraw = true;
		mMenuButton->mDisabled = true;
	}
	else if (aLevel == 1 && mApp->HasFinishedAdventure()) // 1073: only the wrap after the final level.
	{
		ReportAchievement::GiveAchievement(mApp, HomeSecurity, false); // @Patoke: add achievement
		mStartButton->mLabel = gStr.CONTINUE_BUTTON;
		mMenuButton->mBtnNoDraw = true;
		mMenuButton->mDisabled = true;
	}
	else if (aLevel == 15)
		mStartButton->mLabel = gStr.VIEW_ALMANAC_BUTTON;
	else if (aLevel == 25 || aLevel == 35 || aLevel == 45)
		mStartButton->mLabel = gStr.CONTINUE_BUTTON;
	else
		mStartButton->mLabel = gStr.NEXT_LEVEL_BUTTON;

	// @Patoke: implemented
	// 1073 00407512: the award-screen grant belongs to non-adventure modes.
	if (!mApp->IsAdventureMode() && mApp->EarnedGoldTrophy()) {
		ReportAchievement::GiveAchievement(mApp, NovelPeasPrize, false);
	}

	if (mApp->IsFirstTimeAdventureMode() && aLevel == 25 && mApp->IsTrialStageLocked() && !mApp->mPlayerInfo->mHasSeenUpsell)
	{
		mMenuButton->mBtnNoDraw = true;
		mMenuButton->mDisabled = true;
	}

	// @Patoke: implemented
	if (mShowingAchievements) {
		mShowStartButtonAfterAchievements = !mStartButton->mBtnNoDraw;
		mShowMenuButtonAfterAchievements = !mMenuButton->mBtnNoDraw;
		mStartButton->mBtnNoDraw = true;
		mStartButton->mDisabled = true;
		mMenuButton->mBtnNoDraw = true;
		mMenuButton->mDisabled = true;
		mContinueButton->mLabel = gStr.CONTINUE_BUTTON;
	}

	if (IsPaperNote())
	{
		mApp->mMusic->StopAllMusic();
		mStartButton->mY += 20;
		mApp->PlayFoley(FOLEY_PAPER);
	}
	else
		mApp->mMusic->MakeSureMusicIsPlaying(MUSIC_TUNE_ZEN_GARDEN);
}

//0x406420 & 0x406440
AwardScreen::~AwardScreen()
{
	if (mStartButton) delete mStartButton;
	if (mContinueButton) delete mContinueButton; // @Patoke: add new button
	if (mMenuButton) delete mMenuButton;
}

bool AwardScreen::IsPaperNote()
{
	if (mAwardType == AWARD_CREDITS_ZOMBIENOTE || mAwardType == AWARD_HELP_ZOMBIENOTE)
		return true;

	int aLevel = mApp->mPlayerInfo->mLevel;
	return mApp->IsAdventureMode() && (aLevel == 10 || aLevel == 20 || aLevel == 30 || aLevel == 40 || aLevel == 50);
}

//0x4064D0
void AwardScreen::DrawBottom(Graphics* g, const SexyString& theTitle, const SexyString& theAward, const SexyString& theMessage)
{
	TodDrawString(g, theTitle, BOARD_WIDTH / 2, 58, Sexy::FONT_DWARVENTODCRAFT24, Color(213, 159, 43), DS_ALIGN_CENTER);
	TodDrawString(g, theAward, BOARD_WIDTH / 2, 326, Sexy::FONT_DWARVENTODCRAFT18YELLOW, Color::White, DS_ALIGN_CENTER);
	TodDrawStringWrapped(g, theMessage, Rect(285, 360, 230, 90), Sexy::FONT_BRIANNETOD16, Color(40, 50, 90), DS_ALIGN_CENTER_VERTICAL_MIDDLE);
}

//0x4066A0
void AwardScreen::DrawAwardSeed(Graphics* g)
{
	SeedType aSeedType = mApp->GetAwardSeedForLevel(mApp->mPlayerInfo->mLevel - 1);
	SexyString aAward = Plant::GetNameString(aSeedType, SEED_NONE);
	SexyString aMessage;
	if (mApp->IsTrialStageLocked() && aSeedType >= SEED_SQUASH && aSeedType != SEED_TANGLEKELP)
		aMessage = gStr.AVAILABLE_IN_FULL_VERSION;
	else
		aMessage = Plant::GetToolTip(aSeedType);
	DrawBottom(g, gStr.NEW_PLANT, aAward, aMessage);

	g->SetScale(2, 2, 350, 129);
	DrawSeedPacket(g, 350, 129, aSeedType, SEED_NONE, 0, 255, true, false);
	g->SetScale(1, 1, 0, 0);
}

//0x4068D0
// GOTY @Patoke: 0x4081C0
void AwardScreen::Draw(Graphics* g)
{
	Graphics aGraphics(*g);
	const int anOldX = mX;
	const int anOldY = mY;
	RefreshAwardLayout(*this);
	aGraphics.mTransX += mX - anOldX;
	aGraphics.mTransY += mY - anOldY;
	g = &aGraphics;
	g->mLinearBlend = true;

	int aLevel = mApp->mPlayerInfo->mLevel;
	if (!mShowingAchievements && !IsPaperNote() && mAwardType != AWARD_ACHIEVEMENTONLY)
		DrawAwardBackground(g, Sexy::IMAGE_AWARDSCREEN_BACK, *this);
	if (mShowingAchievements) // @Patoke: add call
		DrawAchievements(g);
	else if (mAwardType == AWARD_CREDITS_ZOMBIENOTE)
	{
		g->mColor = Color(125, 200, 255, 255);
		g->mColorizeImages = true;
		g->DrawImage(Sexy::IMAGE_BACKGROUND6BOSS, -900, -400, 2800, 1200);
		g->mColorizeImages = false;
		g->mColor = Color(0, 0, 0, 64);
		g->FillRect(mApp->mScreenBounds.mX - mX, 525,
			mApp->mScreenBounds.mWidth, BOARD_HEIGHT - 525);
		g->DrawImage(Sexy::IMAGE_ZOMBIE_NOTE, 75, 60);
		g->DrawImage(Sexy::IMAGE_CREDITS_ZOMBIENOTE, 149, 103, 475, 325);
	}
	else if (mAwardType == AWARD_HELP_ZOMBIENOTE)
	{
		g->DrawImage(Sexy::IMAGE_BACKGROUND1, -700, -300, 2800, 1200);
		g->DrawImage(Sexy::IMAGE_ZOMBIE_NOTE, 80, 80);
		g->DrawImage(Sexy::IMAGE_ZOMBIE_NOTE_HELP, 131, 132);
	}
	else if (mAwardType != AWARD_ACHIEVEMENTONLY) // @Patoke: add check
	{
		if (!mApp->IsAdventureMode())
		{
			if (mApp->EarnedGoldTrophy())
			{
				DrawBottom(g, gStr.BEAT_GAME_MESSAGE1, gStr.GOLD_SUNFLOWER_TROPHY, gStr.BEAT_GAME_MESSAGE2);
				TodDrawImageRectCenterScaledF(g, Sexy::IMAGE_SUNFLOWER_TROPHY, 325, 65, Rect(157, 0, 157, 269), 0.6f, 0.6f);
			}
			else
			{
				const char* aMsgChar;
				if (mApp->IsSurvivalMode())
				{
					int aNumTrophies = mApp->GetNumTrophies(CHALLENGE_PAGE_SURVIVAL);
					aMsgChar =
						aNumTrophies <= 7 ? gStr.YOU_UNLOCKED_A_SURVIVAL.c_str() :
						aNumTrophies == 10 ? gStr.YOU_UNLOCKED_ENDLESS_SURVIVAL.c_str() : gStr.EARN_MORE_TROPHIES_FOR_ENDLESS_SURVIVAL.c_str();

				}
				else if (mApp->IsScaryPotterLevel())
					aMsgChar = gStr.UNLOCKED_VASEBREAKER_LEVEL.c_str();
				else if (mApp->IsPuzzleMode())
					aMsgChar = gStr.UNLOCKED_I_ZOMBIE_LEVEL.c_str();
				else
					aMsgChar = mApp->GetNumTrophies(CHALLENGE_PAGE_CHALLENGE) <= 17 ? gStr.CHALLENGE_UNLOCKED.c_str() : gStr.GET_MORE_TROPHIES.c_str();

				DrawBottom(g, gStr.GOT_TROPHY, gStr.TROPHY, aMsgChar);
				g->DrawImage(Sexy::IMAGE_TROPHY_HI_RES, BOARD_WIDTH / 2 - Sexy::IMAGE_TROPHY_HI_RES->mWidth / 2, 137);
			}
		}
		else
		{
			// B 形态 -> 常量表 kAwardDrawRows（原 11 支关卡链合并；下标平移
			// aLevel/5-1，仅 5 的倍数命中）。mIsNote=true 纸条形态（背景 + 纸条
			// + FOUND_NOTE 居中），false 奖杯形态（DrawBottom 三行 + 居中图）；
			// aLevel==1 且已通关的银向日葵奖特例与 DrawAwardSeed 默认支表外保留。
			static constexpr struct {
				bool mIsNote;
				const std::string* mStr1;
				const std::string* mStr2;
				const std::string* mStr3;
				Image* const* mImage;  // 奖杯图 / 纸条变体图
				Image* const* mBg;     // 纸条形态背景；奖杯形态 nullptr
				int mX;
				int mY;
			} kAwardDrawRows[] = {
				{ false, &gStr.GOT_SHOVEL, &gStr.SHOVEL, &gStr.SHOVEL_DESCRIPTION, &Sexy::IMAGE_SHOVEL_HI_RES, nullptr, 0, 137 },        // 5
				{ true,  &gStr.FOUND_NOTE, nullptr, nullptr, &Sexy::IMAGE_ZOMBIE_NOTE1, &Sexy::IMAGE_BACKGROUND1, 131, 132 },            // 10
				{ false, &gStr.FOUND_SUBURBAN_ALMANAC, &gStr.SUBURBAN_ALMANAC, &gStr.SUBURBAN_ALMANAC_DESCRIPTION, &Sexy::IMAGE_ALMANAC, nullptr, 0, 160 },  // 15
				{ true,  &gStr.FOUND_NOTE, nullptr, nullptr, &Sexy::IMAGE_ZOMBIE_NOTE2, &Sexy::IMAGE_BACKGROUND2, 133, 127 },            // 20
				{ false, &gStr.FOUND_KEYS, &gStr.KEYS, &gStr.KEYS_DESCRIPTION, &Sexy::IMAGE_CARKEYS, nullptr, 0, 160 },                 // 25
				{ true,  &gStr.FOUND_NOTE, nullptr, nullptr, &Sexy::IMAGE_ZOMBIE_NOTE3, &Sexy::IMAGE_BACKGROUND1, 120, 117 },            // 30
				{ false, &gStr.FOUND_TACO, &gStr.TACO, &gStr.TACO_DESCRIPTION, &Sexy::IMAGE_TACO, nullptr, 0, 160 },                    // 35
				{ true,  &gStr.FOUND_NOTE, nullptr, nullptr, &Sexy::IMAGE_ZOMBIE_NOTE4, &Sexy::IMAGE_BACKGROUND2, 102, 117 },            // 40
				{ false, &gStr.FOUND_WATERING_CAN, &gStr.WATERING_CAN, &gStr.WATERING_CAN_DESCRIPTION, &Sexy::IMAGE_WATERINGCAN, nullptr, 0, 160 },  // 45
				{ true,  &gStr.FOUND_NOTE, nullptr, nullptr, &Sexy::IMAGE_ZOMBIE_FINAL_NOTE, &Sexy::IMAGE_BACKGROUND1, 114, 138 },       // 50
			};
			static_assert(sizeof(kAwardDrawRows) / sizeof(kAwardDrawRows[0]) == 10,
				"kAwardDrawRows 槽位须与 5..50 奖励关卡段同步");

			const int anAwardIndex = aLevel / 5 - 1;
			if (aLevel % 5 == 0 && (unsigned)anAwardIndex < sizeof(kAwardDrawRows) / sizeof(kAwardDrawRows[0]))
			{
				const auto& aRow = kAwardDrawRows[anAwardIndex];
				if (aRow.mIsNote)
				{
					g->DrawImage(*aRow.mBg, -700, -300, 2800, 1200);
					g->DrawImage(Sexy::IMAGE_ZOMBIE_NOTE, 80, 80);
					g->DrawImage(*aRow.mImage, aRow.mX, aRow.mY);
					TodDrawString(g, *aRow.mStr1, BOARD_WIDTH / 2, 70, Sexy::FONT_DWARVENTODCRAFT24, Color(255, 200, 0, 255), DS_ALIGN_CENTER);
				}
				else
				{
					DrawBottom(g, *aRow.mStr1, *aRow.mStr2, *aRow.mStr3);
					g->DrawImage(*aRow.mImage, BOARD_WIDTH / 2 - (*aRow.mImage)->mWidth / 2, aRow.mY);
				}
			}
			else if (aLevel == 1 && mApp->HasFinishedAdventure())
			{
				DrawBottom(g, gStr.WIN_MESSAGE1, gStr.SILVER_SUNFLOWER_TROPHY, gStr.WIN_MESSAGE2);
				TodDrawImageRectCenterScaledF(g, Sexy::IMAGE_SUNFLOWER_TROPHY, 325, 65, Rect(0, 0, 157, 269), 0.7f, 0.7f);
			}
			else
			{
				DrawAwardSeed(g);
			}
		}
	}

	const Sexy::TextureSource fixedUI{CommonUIAtlas::Texture, 0};
	const TextureRegion& glow = CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::SEEDCHOOSER_BUTTON_GLOW)];
	if (mAwardType == AWARD_CREDITS_ZOMBIENOTE)
		mStartButton->Draw(g);
	else
		mStartButton->DrawAtlas(g, fixedUI, &glow);
	mMenuButton->DrawAtlas(g, fixedUI);
	mContinueButton->DrawAtlas(g, fixedUI, &glow);

	int aFadeInAlpha = TodAnimateCurveNs(1'800'000'000, 0, mFadeInCounter.mNanoseconds, 255, 0, Curves::Linear);
	g->mColor = IsPaperNote() ? Color(0, 0, 0, aFadeInAlpha) : Color(255, 255, 255, aFadeInAlpha);
	Rect aScreenRect = mApp->mScreenBounds;
	aScreenRect.Offset(-mX, -mY);
	g->FillRect(aScreenRect);
}

//0x4076A0
// GOTY @Patoke: 0x408FE0
void AwardScreen::Update()
{
	RefreshAwardLayout(*this);
	Widget::Update();
	// @Patoke: implemented
	if (mShowingAchievements) {
		mAchievementAnimTime++;

		for (size_t i = 0; i < mAchievementItems.size(); i++) {
			if (mAchievementAnimTime >= mAchievementItems[i].mStartAnimTime) {
				mAchievementItems[i].mY = TodAnimateCurveNs(
					std::int64_t(mAchievementItems[i].mStartAnimTime) * 10'000'000,
					std::int64_t(mAchievementItems[i].mEndAnimTime) * 10'000'000,
					mAchievementAnimTime.mNanoseconds, mAchievementItems[i].mStartY, mAchievementItems[i].mDestY, Curves::EaseInOut);
			}

			if (!mAchievementItems.empty() && mAchievementItems[mAchievementItems.size() - 1].mY == mAchievementItems[mAchievementItems.size() - 1].mDestY) {
				mContinueButton->mBtnNoDraw = false;
				mContinueButton->mDisabled = false;
			}
		}
	}

	if (mApp->GetDialogCount() > 0) return;
	mStartButton->Update();
	mMenuButton->Update();
	mContinueButton->Update(); // @Patoke: add call
	mApp->SetCursor(mStartButton->IsMouseOver() || mMenuButton->IsMouseOver() || mContinueButton->IsMouseOver() ? CURSOR_HAND : CURSOR_POINTER);
	MarkDirty();
	if (mFadeInCounter > 0) mFadeInCounter--;
}

//0x407760
void AwardScreen::KeyChar(char theChar)
{
	if (theChar == ' ' || theChar == '\r' || theChar == '\u001B')
		StartButtonPressed();
}

//0x407780
// GOTY @Patoke: 0x409530
void AwardScreen::StartButtonPressed()
{
	if (mApp->GetDialog(DIALOG_STORE))
		return;

	if (mAwardType == AWARD_CREDITS_ZOMBIENOTE)
	{
		mApp->KillAwardScreen();
		mApp->ShowCreditScreen();
	}
	else if (mAwardType == AWARD_HELP_ZOMBIENOTE)
	{
		mApp->KillAwardScreen();
		mApp->ShowGameSelector();
	}
	else if (mApp->IsSurvivalMode())
	{
		mApp->KillAwardScreen();
		mApp->ShowChallengeScreen(CHALLENGE_PAGE_SURVIVAL);
	}
	else if (mApp->IsPuzzleMode())
	{
		mApp->KillAwardScreen();
		mApp->ShowChallengeScreen(CHALLENGE_PAGE_PUZZLE);
	}
	else if (mApp->IsChallengeMode())
	{
		mApp->KillAwardScreen();
		mApp->ShowChallengeScreen(CHALLENGE_PAGE_CHALLENGE);
	}
	else
	{
		int aLevel = mApp->mPlayerInfo->mLevel;
		if (aLevel == 1)
		{
			mApp->KillAwardScreen();
			if (mApp->HasFinishedAdventure())
			{
				mApp->ShowAwardScreen(AWARD_CREDITS_ZOMBIENOTE, false);
			}
			else
			{
				mApp->PreNewGame(GAMEMODE_ADVENTURE, false);
			}
		}
		else
		{
			if (aLevel == 15)
			{
				mApp->DoAlmanacDialog()->WaitForResult();
			}
			else
			{
				// B 形态 -> 常量表 kAwardStoreIntro（原 25/35/42 三支共体合并；
				// int 判别式配对查表）；15 图鉴与 45 禅意花园特例表外保留。
				static constexpr struct { int mLevel; int mIntro; } kAwardStoreIntro[] = {
					{ 25, 301 },
					{ 35, 601 },
					{ 42, 3100 },
				};

				for (const auto& aRow : kAwardStoreIntro)
				{
					if (aRow.mLevel != aLevel)
						continue;

					StoreScreen* aStore = mApp->ShowStoreScreen();
					aStore->SetupForIntro(aRow.mIntro);
					aStore->WaitForResult(true);

					if (aLevel == 25 && aStore->mPurchasedFullVersion)
					{
						mApp->KillAwardScreen();
						mApp->ShowGameSelector();
						return;
					}
					break;
				}
			}

			if (aLevel == 45)
			{
				mApp->KillAwardScreen();
				mApp->PreNewGame(GAMEMODE_LIMBO_ZEN_GARDEN, false);
				mApp->mZenGarden->SetupForZenTutorial();
				return;
			}

			mApp->KillAwardScreen();
			mApp->PreNewGame(GAMEMODE_ADVENTURE, false);
		}
	}
}

//0x4079F0
// GOTY @Patoke: 0x4097A0
void AwardScreen::MouseDown(int x, int y, int theClickCount)
{
	const int aPreviousX = mX;
	RefreshAwardLayout(*this);
	x += aPreviousX - mX;
	if (theClickCount == 1) {
		mStartButton->Update(); // @Patoke: implemented
		mMenuButton->Update();
		mContinueButton->Update();
		if (mStartButton->HitTest(x, y, this) || mMenuButton->HitTest(x, y, this) || mContinueButton->HitTest(x, y, this))
			mApp->PlaySample(Sexy::SOUND_TAP);
	}
}

//0x407A70
// GOTY @Patoke: 0x409840
void AwardScreen::MouseUp(int x, int y, int theClickCount)
{
	const int aPreviousX = mX;
	RefreshAwardLayout(*this);
	x += aPreviousX - mX;
	if (theClickCount == 1)
	{
		if (mStartButton->HitTest(x, y, this))
			StartButtonPressed();
		if (mContinueButton->HitTest(x, y, this)) // @Patoke: add call
			AchievementsContinuePressed();
		if (mMenuButton->HitTest(x, y, this))
		{
			mApp->KillAwardScreen();
			mApp->ShowGameSelector();
		}
	}
}

// @Patoke: implement functions
// GOTY @Patoke: 0x407C20
void AwardScreen::DrawAchievements(Graphics* g) {
	const TextureSource aTexture{AchievementsAtlas::Texture, 0};
	g->mColorizeImages = true;
	// Rect aTextWrap = Rect(0, 0, 0, 77); // unused
	g->mColor = Color(255, 255, 255);
	Rect aScreenRect = mApp->mScreenBounds;
	aScreenRect.Offset(-mX, -mY);
	g->FillRect(aScreenRect);
	g->mColorizeImages = false;

	DrawAwardBackground(g, IMAGE_CHALLENGE_BACKGROUND, *this);

	TodDrawString(g, ("ACHIEVEMENTS"), BOARD_WIDTH / 2, 58, FONT_HOUSEOFTERROR28, Color(220, 220, 220), DS_ALIGN_CENTER);

	for (size_t i = 0; i < mAchievementItems.size(); i++) {
		std::string aAchievementName = gAchievementList[mAchievementItems[i].mId].name;
		std::string aAchievementDesc = gAchievementList[mAchievementItems[i].mId].description;
		aAchievementName.append(" Earned!");

		const TextureRegion& aIcon=AchievementsAtlas::Icons[mAchievementItems[i].mId];
		const Rect aSrcRect(0,0,aIcon.mWidth,aIcon.mHeight);
		Rect aDestRect = Rect(220, mAchievementItems[i].mY + 10, 70, 70);
		Rect aTextRect = Rect(300, mAchievementItems[i].mY + 20, 300, 60);

		DrawAtlasImage(g,aTexture,aIcon,aDestRect,aSrcRect);

		TodDrawString(g, aAchievementName, 450, mAchievementItems[i].mY + 25, FONT_DWARVENTODCRAFT15, Color(224, 187, 98), DS_ALIGN_CENTER);
		TodDrawStringWrapped(g, aAchievementDesc, aTextRect, FONT_DWARVENTODCRAFT12, Color(255, 255, 255), DS_ALIGN_CENTER_VERTICAL_MIDDLE);
	}
}

// GOTY @Patoke: 0x409400
void AwardScreen::AchievementsContinuePressed() {
	// @Patoke: implemented
	if (mAwardType == AWARD_ACHIEVEMENTONLY) {
		mApp->KillAwardScreen();
		mApp->PreNewGame(GAMEMODE_ADVENTURE, false);
	}
	else {
		mStartButton->mBtnNoDraw = !mShowStartButtonAfterAchievements;
		mStartButton->mDisabled = !mShowStartButtonAfterAchievements;
		mMenuButton->mBtnNoDraw = !mShowMenuButtonAfterAchievements;
		mMenuButton->mDisabled = !mShowMenuButtonAfterAchievements;
		mContinueButton->mDisabled = true;
		mContinueButton->mBtnNoDraw = true;
		mShowingAchievements = false;
		int level = mApp->mPlayerInfo->mLevel;
		if (mApp->IsAdventureMode() && level == 1 && mApp->HasFinishedAdventure()) {
			mApp->KillAwardScreen();
			mApp->ShowAwardScreen(AWARD_CREDITS_ZOMBIENOTE, false);
		}
	}
}

const void* AwardScreen::TouchHitTest(int x, int y)
{
    if (mStartButton->HitTest(x, y, this)) return mStartButton;
    if (mMenuButton->HitTest(x, y, this)) return mMenuButton;
    if (mContinueButton->HitTest(x, y, this)) return mContinueButton;
    return this;
}
