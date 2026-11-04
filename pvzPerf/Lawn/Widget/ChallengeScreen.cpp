#include "Sexy.TodLib/generated/EntityStrings.h"  // 动态键实体表（阶段 3）
#include "GameButton.h"
#include "LawnApp.h"
#include "GameConstants.h"
#include "Lawn/System/Music.h"
#include "ChallengeScreen.h"
#include "Lawn/LawnCommon.h"
#include "Resources.h"
#include "Lawn/ToolTipWidget.h"
#include "Lawn/System/PlayerInfo.h"
#include "Sexy.TodLib/TodDebug.h"
#include "Sexy.TodLib/TodFoley.h"
#include "Sexy.TodLib/TodCommon.h"
#include "misc/Debug.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "widget/WidgetManager.h"

// 梅耶单例：mChallengeName 指向 gStr 成员的 c_str()（运行期动态初始化），
// 命名空间作用域静态表会踩初始化顺序雷——首次调用时构造则 gStr 必已就绪
static ChallengeDefinition* ChallengeDefs()
{
	// 无尺寸声明（计数锚在表尾）：本表曾以 71 行填 72 声明槽，尾槽零值
	// ={ADVENTURE,0,生存页,0,0,nullptr}——幽灵按钮恰好盖在生存白天(38,125)
	// 上、生存页可见、点击路由到模式 0=直接进冒险（2026-10-08 实机"点生存
	// 白天跳冒险/名字消失"三症状同源）。UPSELL 删行时少一行，CUSTOM 落位
	// 时未回填；保留 CUSTOM 占位行以稳定编号，按钮隐藏且不允许进入。
	static ChallengeDefinition gChallengeDefs[] = {
	{ GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_1,              0,   ChallengePage::CHALLENGE_PAGE_SURVIVAL,    0,  0,  gStr.SURVIVAL_DAY_NORMAL.c_str() },
	{ GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_2,              1,   ChallengePage::CHALLENGE_PAGE_SURVIVAL,    0,  1,  gStr.SURVIVAL_NIGHT_NORMAL.c_str() },
	{ GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_3,              2,   ChallengePage::CHALLENGE_PAGE_SURVIVAL,    0,  2,  gStr.SURVIVAL_POOL_NORMAL.c_str() },
	{ GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_4,              3,   ChallengePage::CHALLENGE_PAGE_SURVIVAL,    0,  3,  gStr.SURVIVAL_FOG_NORMAL.c_str() },
	{ GameMode::GAMEMODE_SURVIVAL_NORMAL_STAGE_5,              4,   ChallengePage::CHALLENGE_PAGE_SURVIVAL,    0,  4,  gStr.SURVIVAL_ROOF_NORMAL.c_str() },
	{ GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_1,                5,   ChallengePage::CHALLENGE_PAGE_SURVIVAL,    1,  0,  gStr.SURVIVAL_DAY_HARD.c_str() },
	{ GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_2,                6,   ChallengePage::CHALLENGE_PAGE_SURVIVAL,    1,  1,  gStr.SURVIVAL_NIGHT_HARD.c_str() },
	{ GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_3,                7,   ChallengePage::CHALLENGE_PAGE_SURVIVAL,    1,  2,  gStr.SURVIVAL_POOL_HARD.c_str() },
	{ GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_4,                8,   ChallengePage::CHALLENGE_PAGE_SURVIVAL,    1,  3,  gStr.SURVIVAL_FOG_HARD.c_str() },
	{ GameMode::GAMEMODE_SURVIVAL_HARD_STAGE_5,                9,   ChallengePage::CHALLENGE_PAGE_SURVIVAL,    1,  4,  gStr.SURVIVAL_ROOF_HARD.c_str() },
	{ GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_1,             10,  ChallengePage::CHALLENGE_PAGE_LIMBO,       3,  0,  gStr.SURVIVAL_DAY_ENDLESS.c_str() },
	{ GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_2,             10,  ChallengePage::CHALLENGE_PAGE_LIMBO,       3,  1,  gStr.SURVIVAL_NIGHT_ENDLESS.c_str() },
	{ GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_3,             10,  ChallengePage::CHALLENGE_PAGE_SURVIVAL,    2,  2,  gStr.SURVIVAL_POOL_ENDLESS.c_str() },
	{ GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_4,             10,  ChallengePage::CHALLENGE_PAGE_LIMBO,       3,  2,  gStr.SURVIVAL_FOG_ENDLESS.c_str() },
	{ GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_5,             10,  ChallengePage::CHALLENGE_PAGE_LIMBO,       3,  3,  gStr.SURVIVAL_ROOF_ENDLESS.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_WAR_AND_PEAS,               0,   ChallengePage::CHALLENGE_PAGE_CHALLENGE,   0,  0,  gStr.WAR_AND_PEAS.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_WALLNUT_BOWLING,            6,   ChallengePage::CHALLENGE_PAGE_CHALLENGE,   0,  1,  gStr.WALL_NUT_BOWLING.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_SLOT_MACHINE,               2,   ChallengePage::CHALLENGE_PAGE_CHALLENGE,   0,  2,  gStr.SLOT_MACHINE.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_RAINING_SEEDS,              3,   ChallengePage::CHALLENGE_PAGE_CHALLENGE,   0,  3,  gStr.ITS_RAINING_SEEDS.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_BEGHOULED,                  1,   ChallengePage::CHALLENGE_PAGE_CHALLENGE,   0,  4,  gStr.BEGHOULED.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_INVISIGHOUL,                8,   ChallengePage::CHALLENGE_PAGE_CHALLENGE,   1,  0,  gStr.INVISIGHOUL.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_SEEING_STARS,               5,   ChallengePage::CHALLENGE_PAGE_CHALLENGE,   1,  1,  gStr.SEEING_STARS.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_ZOMBIQUARIUM,               7,   ChallengePage::CHALLENGE_PAGE_CHALLENGE,   1,  2,  gStr.ZOMBIQUARIUM.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_BEGHOULED_TWIST,            20,  ChallengePage::CHALLENGE_PAGE_CHALLENGE,   1,  3,  gStr.BEGHOULED_TWIST.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_LITTLE_TROUBLE,             12,  ChallengePage::CHALLENGE_PAGE_CHALLENGE,   1,  4,  gStr.LITTLE_TROUBLE.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_PORTAL_COMBAT,              15,  ChallengePage::CHALLENGE_PAGE_CHALLENGE,   2,  0,  gStr.PORTAL_COMBAT.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_COLUMN,                     4,   ChallengePage::CHALLENGE_PAGE_CHALLENGE,   2,  1,  gStr.COLUMN_AS_YOU_SEE_EM.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_BOBSLED_BONANZA,            17,  ChallengePage::CHALLENGE_PAGE_CHALLENGE,   2,  2,  gStr.BOBSLED_BONANZA.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_SPEED,                      18,  ChallengePage::CHALLENGE_PAGE_CHALLENGE,   2,  3,  gStr.ZOMBIES_ON_SPEED.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_WHACK_A_ZOMBIE,             16,  ChallengePage::CHALLENGE_PAGE_CHALLENGE,   2,  4,  gStr.WHACK_A_ZOMBIE.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_LAST_STAND,                 21,  ChallengePage::CHALLENGE_PAGE_CHALLENGE,   3,  0,  gStr.LAST_STAND.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_WAR_AND_PEAS_2,             0,   ChallengePage::CHALLENGE_PAGE_CHALLENGE,   3,  1,  gStr.WAR_AND_PEAS_2.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_WALLNUT_BOWLING_2,          6,   ChallengePage::CHALLENGE_PAGE_CHALLENGE,   3,  2,  gStr.WALL_NUT_BOWLING_EXTREME.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_POGO_PARTY,                 14,  ChallengePage::CHALLENGE_PAGE_CHALLENGE,   3,  3,  gStr.POGO_PARTY.c_str() },
	{ GameMode::GAMEMODE_MINIGAME_FINAL_BOSS,                 19,  ChallengePage::CHALLENGE_PAGE_CHALLENGE,   3,  4,  gStr.FINAL_BOSS.c_str() },
	{ GameMode::GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT,      0,   ChallengePage::CHALLENGE_PAGE_LIMBO,       0,  0,  gStr.ART_CHALLENGE_WALL_NUT.c_str() },
	{ GameMode::GAMEMODE_LIMBO_SUNNY_DAY,                  1,   ChallengePage::CHALLENGE_PAGE_LIMBO,       0,  1,  gStr.SUNNY_DAY.c_str() },
	{ GameMode::GAMEMODE_LIMBO_RESODDED,                   2,   ChallengePage::CHALLENGE_PAGE_LIMBO,       0,  2,  gStr.UNSODDED.c_str() },
	{ GameMode::GAMEMODE_LIMBO_BIG_TIME,                   3,   ChallengePage::CHALLENGE_PAGE_LIMBO,       0,  3,  gStr.BIG_TIME.c_str() },
	{ GameMode::GAMEMODE_LIMBO_ART_CHALLENGE_SUNFLOWER,    4,   ChallengePage::CHALLENGE_PAGE_LIMBO,       0,  4,  gStr.ART_CHALLENGE_SUNFLOWER.c_str() },
	{ GameMode::GAMEMODE_LIMBO_AIR_RAID,                   5,   ChallengePage::CHALLENGE_PAGE_LIMBO,       1,  0,  gStr.AIR_RAID.c_str() },
	{ GameMode::GAMEMODE_LIMBO_ICE,                        6,   ChallengePage::CHALLENGE_PAGE_LIMBO,       1,  1,  gStr.ICE_LEVEL.c_str() },
	{ GameMode::GAMEMODE_LIMBO_ZEN_GARDEN,                 7,   ChallengePage::CHALLENGE_PAGE_LIMBO,       1,  2,  gStr.ZEN_GARDEN.c_str() },
	{ GameMode::GAMEMODE_LIMBO_HIGH_GRAVITY,               8,   ChallengePage::CHALLENGE_PAGE_LIMBO,       1,  3,  gStr.HIGH_GRAVITY.c_str() },
	{ GameMode::GAMEMODE_LIMBO_GRAVE_DANGER,               11,  ChallengePage::CHALLENGE_PAGE_LIMBO,       1,  4,  gStr.GRAVE_DANGER.c_str() },
	{ GameMode::GAMEMODE_LIMBO_SHOVEL,                     10,  ChallengePage::CHALLENGE_PAGE_LIMBO,       2,  0,  gStr.CAN_YOU_DIG_IT.c_str() },
	{ GameMode::GAMEMODE_LIMBO_STORMY_NIGHT,               13,  ChallengePage::CHALLENGE_PAGE_LIMBO,       2,  1,  gStr.DARK_STORMY_NIGHT.c_str() },
	{ GameMode::GAMEMODE_LIMBO_BUNGEE_BLITZ,               9,   ChallengePage::CHALLENGE_PAGE_LIMBO,       2,  2,  gStr.BUNGEE_BLITZ.c_str() },
	{ GameMode::GAMEMODE_LIMBO_SQUIRREL,                   10,  ChallengePage::CHALLENGE_PAGE_LIMBO,       2,  3,  ("Squirrel") },
	{ GameMode::GAMEMODE_TREE_OF_WISDOM,                       10,  ChallengePage::CHALLENGE_PAGE_LIMBO,       2,  4,  ("Tree of Wisdom") }, // @Patoke: replaced for english
	{ GameMode::GAMEMODE_SCARY_POTTER_1,                       10,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      0,  0,  gStr.SCARY_POTTER_1.c_str() },
	{ GameMode::GAMEMODE_SCARY_POTTER_2,                       10,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      0,  1,  gStr.SCARY_POTTER_2.c_str() },
	{ GameMode::GAMEMODE_SCARY_POTTER_3,                       10,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      0,  2,  gStr.SCARY_POTTER_3.c_str() },
	{ GameMode::GAMEMODE_SCARY_POTTER_4,                       10,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      0,  3,  gStr.SCARY_POTTER_4.c_str() },
	{ GameMode::GAMEMODE_SCARY_POTTER_5,                       10,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      0,  4,  gStr.SCARY_POTTER_5.c_str() },
	{ GameMode::GAMEMODE_SCARY_POTTER_6,                       10,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      1,  0,  gStr.SCARY_POTTER_6.c_str() },
	{ GameMode::GAMEMODE_SCARY_POTTER_7,                       10,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      1,  1,  gStr.SCARY_POTTER_7.c_str() },
	{ GameMode::GAMEMODE_SCARY_POTTER_8,                       10,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      1,  2,  gStr.SCARY_POTTER_8.c_str() },
	{ GameMode::GAMEMODE_SCARY_POTTER_9,                       10,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      1,  3,  gStr.SCARY_POTTER_9.c_str() },
	{ GameMode::GAMEMODE_SCARY_POTTER_ENDLESS,                 10,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      1,  4,  gStr.SCARY_POTTER_ENDLESS.c_str() },
	{ GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_1,                    11,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      2,  0,  gStr.I_ZOMBIE_1.c_str() },
	{ GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_2,                    11,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      2,  1,  gStr.I_ZOMBIE_2.c_str() },
	{ GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_3,                    11,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      2,  2,  gStr.I_ZOMBIE_3.c_str() },
	{ GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_4,                    11,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      2,  3,  gStr.I_ZOMBIE_4.c_str() },
	{ GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_5,                    11,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      2,  4,  gStr.I_ZOMBIE_5.c_str() },
	{ GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_6,                    11,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      3,  0,  gStr.I_ZOMBIE_6.c_str() },
	{ GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_7,                    11,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      3,  1,  gStr.I_ZOMBIE_7.c_str() },
	{ GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_8,                    11,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      3,  2,  gStr.I_ZOMBIE_8.c_str() },
	{ GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_9,                    11,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      3,  3,  gStr.I_ZOMBIE_9.c_str() },
	{ GameMode::GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS,              11,  ChallengePage::CHALLENGE_PAGE_PUZZLE,      3,  4,  gStr.I_ZOMBIE_ENDLESS.c_str() },
	{ GameMode::GAMEMODE_INTRO,                                10,  ChallengePage::CHALLENGE_PAGE_LIMBO,       2,  3,  ("Intro") },
	{ GameMode::GAMEMODE_CUSTOM_LEVEL,                         10,  ChallengePage::CHALLENGE_PAGE_LIMBO,       3,  4,  ("") }, // Reserved slot; no playable level.
	};
	static_assert(sizeof(gChallengeDefs) / sizeof(gChallengeDefs[0]) == NUM_CHALLENGE_MODES,
		"挑战定义表行数须恰为 NUM_CHALLENGE_MODES（少行=尾槽零值幽灵按钮，多行=越界锚）");
	// 一次性自检（Release 亦生效，日志留痕不硬崩）：行内模式值必须唯一且
	// 落在挑战域——行序与模式值的恒等式曾是三起路由事故的温床。
	static bool aVerified = []
	{
		bool aSeen[(int)GameMode::NUM_GAME_MODES] = {};
		for (const ChallengeDefinition& aDef : gChallengeDefs)
		{
			const int aMode = (int)aDef.mChallengeMode;
			if (aMode < 1 || aMode >= (int)GameMode::NUM_GAME_MODES || aSeen[aMode])
			{
				TodTraceAndLog(("ChallengeDefs inconsistent: mode=%d"), aMode);
				break;
			}
			aSeen[aMode] = true;
		}
		return true;
	}();
	(void)aVerified;
	return gChallengeDefs;
}

//0x42DAE0
// GOTY @Patoke: 0x430810
ChallengeScreen::ChallengeScreen(LawnApp* theApp, ChallengePage thePage)
{
	mLockShakeX = 0;
	mLockShakeY = 0;
	mPageIndex = thePage;
	mApp = theApp;
	mClip = false;
	mCheatEnableChallenges = false;
	mUnlockState = UNLOCK_OFF;
	mUnlockChallengeIndex = -1;
	mUnlockStateCounter = 0;
	TodLoadResources(ResourceGroupId::DelayLoad_ChallengeScreen);

	mBackButton = MakeCommonUIBackButton(ChallengeScreen::ChallengeScreen_Back, this, gStr.BACK_TO_MENU);
	mBackButton->mTextDownOffsetX = 1;
	mBackButton->mTextDownOffsetY = 1;
	mBackButton->mColors[ButtonWidget::COLOR_LABEL] = Color(42, 42, 90);
	mBackButton->mColors[ButtonWidget::COLOR_LABEL_HILITE] = Color(42, 42, 90);
	mBackButton->Resize(18, 568, 111, 26);

	for (int aPageIdx = CHALLENGE_PAGE_SURVIVAL; aPageIdx < MAX_CHALLANGE_PAGES; aPageIdx++)
	{
		ButtonWidget* aPageButton = new ButtonWidget(ChallengeScreen::ChallengeScreen_Page + aPageIdx, this);
		aPageButton->mDoFinger = true;
		mPageButton[aPageIdx] = aPageButton;
		if (aPageIdx == CHALLENGE_PAGE_LIMBO)
			aPageButton->mLabel = ("Limbo Page");
		else
			aPageButton->mLabel = TodReplaceNumberString(gStr.PAGE_X, UiTextPlaceholder::Page, aPageIdx);
		aPageButton->mButtonImage = Sexy::IMAGE_BLANK;
		aPageButton->mOverImage = Sexy::IMAGE_BLANK;
		aPageButton->mDownImage = Sexy::IMAGE_BLANK;
		aPageButton->SetFont(Sexy::FONT_BRIANNETOD12);
		aPageButton->mColors[ButtonWidget::COLOR_LABEL] = Color(255, 240, 0);
		aPageButton->mColors[ButtonWidget::COLOR_LABEL_HILITE] = Color(220, 220, 0);
		aPageButton->Resize(200 + 100 * aPageIdx, 540, 100, 75);
		if (!ShowPageButtons() || aPageIdx == CHALLENGE_PAGE_SURVIVAL || aPageIdx == CHALLENGE_PAGE_PUZZLE)
			aPageButton->mVisible = false;
	}
	
	for (int aChallengeMode = 0; aChallengeMode < NUM_CHALLENGE_MODES; aChallengeMode++)
	{
		ChallengeDefinition& aChlDef = GetChallengeDefinition(aChallengeMode);
		ButtonWidget* aChallengeButton = new ButtonWidget(ChallengeScreen::ChallengeScreen_Mode + aChallengeMode, this);
		mChallengeButtons[aChallengeMode] = aChallengeButton;
		aChallengeButton->mDoFinger = true;
		aChallengeButton->mFrameNoDraw = true;
		if (aChlDef.mPage == CHALLENGE_PAGE_CHALLENGE || aChlDef.mPage == CHALLENGE_PAGE_LIMBO || aChlDef.mPage == CHALLENGE_PAGE_PUZZLE)
			aChallengeButton->Resize(38 + aChlDef.mCol * 155, 93 + aChlDef.mRow * 119, 104, 115);
		else
			aChallengeButton->Resize(38 + aChlDef.mCol * 155, 125 + aChlDef.mRow * 145, 104, 115);
		if (MoreTrophiesNeeded(aChallengeMode))
		{
			aChallengeButton->mDoFinger = false;
			aChallengeButton->mDisabled = true;
		}
	}

	mToolTip = new ToolTipWidget();
	mToolTip->mCenter = true;
	mToolTip->mVisible = false;
	UpdateButtons();
	
	if (mApp->mGameScene != SCENE_LEVEL_INTRO)
		mApp->mMusic->MakeSureMusicIsPlaying(MUSIC_TUNE_CHOOSE_YOUR_SEEDS);

	// bool aIsIZombie = false; // Unused
	if (mPageIndex == CHALLENGE_PAGE_SURVIVAL && mApp->mPlayerInfo->mHasNewSurvival)
	{
		SetUnlockChallengeIndex(mPageIndex, false);
		mApp->mPlayerInfo->mHasNewSurvival = false;
	}
	else if (mPageIndex == CHALLENGE_PAGE_CHALLENGE && mApp->mPlayerInfo->mHasNewMiniGame)
	{
		SetUnlockChallengeIndex(mPageIndex, false);
		mApp->mPlayerInfo->mHasNewMiniGame = false;
	}
	else if (mPageIndex == CHALLENGE_PAGE_PUZZLE)
	{
		if (mApp->mPlayerInfo->mHasNewScaryPotter)
		{
			SetUnlockChallengeIndex(mPageIndex, false);
			mApp->mPlayerInfo->mHasNewScaryPotter = false;
		}
		else if (mApp->mPlayerInfo->mHasNewIZombie)
		{
			SetUnlockChallengeIndex(mPageIndex, true);
			mApp->mPlayerInfo->mHasNewIZombie = false;
		}
	}
}

//0x42E280 & 0x42E2A0
ChallengeScreen::~ChallengeScreen()
{
	delete mBackButton;
	for (ButtonWidget* aPageButton : mPageButton) delete aPageButton;
	for (ButtonWidget* aChallengeButton : mChallengeButtons) delete aChallengeButton;
	delete mToolTip;
}

// 表的行序=原版挑战页序（v1 枚举序）；行内 mChallengeMode 是该行的 v3 模式值。
// 行号只是 UI 下标——与模式值的换算必须走行内字段，禁止 index+1 / index-1
// 算术（v3 改名聚簇后该恒等式已破，ButtonDepress 曾因此全盘错位）。
ChallengeDefinition& GetChallengeDefinition(int theChallengeIndex)
{
	TOD_ASSERT(theChallengeIndex >= 0 && theChallengeIndex < NUM_CHALLENGE_MODES);

	return ChallengeDefs()[theChallengeIndex];
}

// 模式值 → 定义行（GameMode v3 直查）。 GetCurrentChallengeDef 等按模式取
// 定义的消费点一律走这里；72 行线性扫，纯 UI 冷路径。
ChallengeDefinition& GetChallengeDefinitionByMode(GameMode theGameMode)
{
	for (int i = 0; i < NUM_CHALLENGE_MODES; i++)
	{
		ChallengeDefinition& aDef = ChallengeDefs()[i];
		if (aDef.mChallengeMode == theGameMode)
			return aDef;
	}
	TOD_ASSERT(false);  // 模式不在挑战页表内（ADVENTURE/CUSTOM/垃圾值）
	return ChallengeDefs()[0];
}

bool ChallengeScreen::IsScaryPotterLevel(GameMode theGameMode)
{
	return theGameMode >= GAMEMODE_SCARY_POTTER_1 && theGameMode <= GAMEMODE_SCARY_POTTER_ENDLESS;
}

bool ChallengeScreen::IsIZombieLevel(GameMode theGameMode)
{
	return theGameMode >= GAMEMODE_PUZZLE_I_ZOMBIE_1 && theGameMode <= GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS;
}

//0x42E3C0
void ChallengeScreen::SetUnlockChallengeIndex(ChallengePage thePage, bool theIsIZombie)
{
	mUnlockState = UNLOCK_SHAKING;
	mUnlockStateCounter = 100;
	mUnlockChallengeIndex = 0;
	for (int aChallengeMode = 0; aChallengeMode < NUM_CHALLENGE_MODES; aChallengeMode++)
	{
		ChallengeDefinition& aDef = GetChallengeDefinition(aChallengeMode);
		if (aDef.mPage == thePage)
		{
			if (thePage != CHALLENGE_PAGE_PUZZLE || (!theIsIZombie && IsScaryPotterLevel(aDef.mChallengeMode)) || (theIsIZombie && IsIZombieLevel(aDef.mChallengeMode)))
			{
				if (AccomplishmentsNeeded(aChallengeMode) <= 0)
				{
					mUnlockChallengeIndex = aChallengeMode;
				}
			}
		}
	}
}

//0x42E440
int ChallengeScreen::MoreTrophiesNeeded(int theChallengeIndex)
{
	ChallengeDefinition& aDef = GetChallengeDefinition(theChallengeIndex);
	if (aDef.mChallengeMode == GAMEMODE_CUSTOM_LEVEL)
		return 1;
	if (false) // GAMEMODE_UPSELL 已删除
	{
		return aDef.mChallengeMode == GAMEMODE_MINIGAME_FINAL_BOSS ? 1 : 0;
	}
	
	if (mApp->IsTrialStageLocked())
	{
		if (mPageIndex == CHALLENGE_PAGE_PUZZLE && aDef.mChallengeMode >= GAMEMODE_SCARY_POTTER_4)
		{
			return aDef.mChallengeMode == GAMEMODE_SCARY_POTTER_4 ? 1 : 2;
		}
		else if (mPageIndex == CHALLENGE_PAGE_CHALLENGE && aDef.mChallengeMode >= GAMEMODE_MINIGAME_RAINING_SEEDS)
		{
			return aDef.mChallengeMode == GAMEMODE_MINIGAME_RAINING_SEEDS ? 1 : 2;
		}
		else if (mPageIndex == CHALLENGE_PAGE_SURVIVAL && aDef.mChallengeMode >= GAMEMODE_SURVIVAL_NORMAL_STAGE_4)
		{
			return aDef.mChallengeMode == GAMEMODE_SURVIVAL_NORMAL_STAGE_4 ? 1 : 2;
		}
	}

	if (aDef.mPage == CHALLENGE_PAGE_PUZZLE)
	{
		if (IsScaryPotterLevel(aDef.mChallengeMode))
		{
			int aLevelsCompleted = 0;
			for (int aSPIdx = 0; aSPIdx < NUM_CHALLENGE_MODES; aSPIdx++)
			{
				ChallengeDefinition& aSPDef = ChallengeDefs()[aSPIdx];
				if (IsScaryPotterLevel(aSPDef.mChallengeMode) && mApp->HasBeatenChallenge(aSPDef.mChallengeMode))
				{
					aLevelsCompleted++;
				}
			}

			if (aDef.mChallengeMode < GAMEMODE_SCARY_POTTER_4 || mApp->HasFinishedAdventure() || aLevelsCompleted < 3)
			{
				return ClampInt(aDef.mChallengeMode - GAMEMODE_SCARY_POTTER_1 - aLevelsCompleted, 0, 9);
			}
			else
			{
				return aDef.mChallengeMode == GAMEMODE_SCARY_POTTER_4 ? 1 : 2;
			}
		}
		else if (IsIZombieLevel(aDef.mChallengeMode))
		{
			int aLevelsCompleted = 0;
			for (int aIZIdx = 0; aIZIdx < NUM_CHALLENGE_MODES; aIZIdx++)
			{
				ChallengeDefinition& aIZDef = ChallengeDefs()[aIZIdx];
				if (IsIZombieLevel(aIZDef.mChallengeMode) && mApp->HasBeatenChallenge(aIZDef.mChallengeMode))
				{
					aLevelsCompleted++;
				}
			}

			if (aDef.mChallengeMode < GAMEMODE_PUZZLE_I_ZOMBIE_4 || mApp->HasFinishedAdventure() || aLevelsCompleted < 3)
			{
				return ClampInt(aDef.mChallengeMode - GAMEMODE_PUZZLE_I_ZOMBIE_1 - aLevelsCompleted, 0, 9);
			}
			else
			{
				return aDef.mChallengeMode == GAMEMODE_PUZZLE_I_ZOMBIE_4 ? 1 : 2;
			}
		}
	}
	else
	{
		int aIdxInPage = aDef.mRow * 5 + aDef.mCol;
		if ((aDef.mPage == CHALLENGE_PAGE_CHALLENGE || aDef.mPage == CHALLENGE_PAGE_SURVIVAL) && !mApp->HasFinishedAdventure())
		{
			return aIdxInPage < 3 ? 0 : aIdxInPage == 3 ? 1 : 2;
		}
		else
		{
			int aNumTrophies = mApp->GetNumTrophies(aDef.mPage);
			if (aDef.mPage == CHALLENGE_PAGE_LIMBO)
			{
				return 0;
			}
			if (mApp->IsSurvivalEndless(aDef.mChallengeMode))
			{
				return 10 - aNumTrophies;
			}
			if (aDef.mPage == CHALLENGE_PAGE_SURVIVAL || aDef.mPage == CHALLENGE_PAGE_CHALLENGE)
			{
				aNumTrophies += 3;
			}
			else
			{
				TOD_ASSERT(false);
			}

			return aIdxInPage >= aNumTrophies ? aIdxInPage - aNumTrophies + 1 : 0;
		}
	}

	return 0;
}

//0x42E6E0
bool ChallengeScreen::ShowPageButtons()
{
	return mApp->mTodCheatKeys && mPageIndex != CHALLENGE_PAGE_SURVIVAL && mPageIndex != CHALLENGE_PAGE_PUZZLE;
}

//0x42E710
void ChallengeScreen::UpdateButtons()
{
	for (int aChallengeMode = 0; aChallengeMode < NUM_CHALLENGE_MODES; aChallengeMode++)
	{
		const ChallengeDefinition& aDef = GetChallengeDefinition(aChallengeMode);
		mChallengeButtons[aChallengeMode]->mVisible =
			aDef.mChallengeMode != GAMEMODE_CUSTOM_LEVEL && aDef.mPage == mPageIndex;
	}
	for (int aPage = 0; aPage < MAX_CHALLANGE_PAGES; aPage++)
	{
		ButtonWidget* aPageButton = mPageButton[aPage];
		if (aPage == mPageIndex)
		{
			aPageButton->mColors[ButtonWidget::COLOR_LABEL] = Color(64, 64, 64);
			aPageButton->mDisabled = true;
		}
		else
		{
			aPageButton->mColors[ButtonWidget::COLOR_LABEL] = Color(255, 240, 0);
			aPageButton->mDisabled = false;
		}
	}
}

//0x42E8A0
int ChallengeScreen::AccomplishmentsNeeded(int theChallengeIndex)
{
	int aTrophiesNeeded = MoreTrophiesNeeded(theChallengeIndex);
	GameMode aGameMode = GetChallengeDefinition(theChallengeIndex).mChallengeMode;
	if (mApp->IsSurvivalEndless(aGameMode) && aTrophiesNeeded <= 3 && mApp->GetNumTrophies(CHALLENGE_PAGE_SURVIVAL) < 10 &&
		mApp->HasFinishedAdventure() && !mApp->IsTrialStageLocked()) aTrophiesNeeded = 1;
	return mCheatEnableChallenges ? 0 : aTrophiesNeeded;
}

//0x42E920
void ChallengeScreen::DrawButton(Graphics* g, int theChallengeIndex)
{
	ButtonWidget* aChallengeButton = mChallengeButtons[theChallengeIndex];
	if (aChallengeButton->mVisible)
	{
		ChallengeDefinition& aDef = GetChallengeDefinition(theChallengeIndex);
		int aPosX = aChallengeButton->mX;
		int aPosY = aChallengeButton->mY;
		if (aChallengeButton->mIsDown)
		{
			aPosX++;
			aPosY++;
		}

		if (AccomplishmentsNeeded(theChallengeIndex) <= 1)
		{
			// ============================================================================================
			// ▲ 绘制按钮上的小游戏图标
			// ============================================================================================
			if (aChallengeButton->mDisabled)
			{
				g->mColor = Color(92, 92, 92);
				g->mColorizeImages = true;
			}
			if (theChallengeIndex == mUnlockChallengeIndex)
			{
				if (mUnlockState == UNLOCK_SHAKING)
				{
					g->mColor = Color(92, 92, 92);
				}
				else if (mUnlockState == UNLOCK_FADING)
				{
					int aColor = TodAnimateCurveNs(500'000'000, 250'000'000, mUnlockStateCounter.mNanoseconds, 92, 255, Curves::Linear);
					g->mColor = Color(aColor, aColor, aColor);
				}
				g->mColorizeImages = true;
			}

			if (mPageIndex == CHALLENGE_PAGE_SURVIVAL)
			{
				g->DrawImage(Sexy::IMAGE_SURVIVAL_THUMBNAILS, aPosX + 13, aPosY + 4, Rect(aDef.mChallengeIconIndex * 80, 0, 80, 65));
			}
			else
			{
				g->DrawImage(Sexy::IMAGE_CHALLENGE_THUMBNAILS, aPosX + 13, aPosY + 4, Rect(aDef.mChallengeIconIndex * 80, 0, 80, 65));
			}

			// ============================================================================================
			// ▲ 绘制小游戏按钮边框
			// ============================================================================================
			bool aHighLight = aChallengeButton->mIsOver && theChallengeIndex != mUnlockChallengeIndex;
			g->mColorizeImages = false;
			g->DrawImage(aHighLight ? Sexy::IMAGE_CHALLENGE_WINDOW : Sexy::IMAGE_CHALLENGE_WINDOW_HIGHLIGHT, aPosX - 6, aPosY - 2);

			// ============================================================================================
			// ▲ 绘制小游戏的名称
			// ============================================================================================
			Color aTextColor = aHighLight ? Color(250, 40, 40) : Color(42, 42, 90);
			SexyString aName = aDef.mChallengeName;
			if (aChallengeButton->mDisabled || (theChallengeIndex == mUnlockChallengeIndex && mUnlockState == UNLOCK_SHAKING))
			{
				aName = ("?");
			}

			int aNameLen = aName.size();
			if (aNameLen < 13)
			{
				TodDrawString(g, aName, aPosX + 52, aPosY + 96, Sexy::FONT_BRIANNETOD12, aTextColor, DS_ALIGN_CENTER);
			}
			else
			{
				// 先尝试在名称字符串的后半段取空格以将字符串分隔为两行，若后半段中无空格则在整个字符串中寻找空格
				int aHalfPos = (mPageIndex == CHALLENGE_PAGE_SURVIVAL && !aChallengeButton->mDisabled) ? 7 : (aNameLen / 2 - 1);
				const SexyChar* aSpacedChar = sexystrchr(aName.c_str() + aHalfPos, (' '));
				if (aSpacedChar == nullptr)
				{
					aSpacedChar = sexystrchr(aName.c_str(), (' '));
				}

				// 分别计算取得两行文本的长度
				int aLine1Len = aNameLen;
				int aLine2Len = 0;
				if (aSpacedChar != nullptr)
				{
					aLine1Len = aSpacedChar - aName.c_str();
					aLine2Len = aNameLen - aLine1Len - 1;
				}

				// 分别绘制两行文本字符串
				TodDrawString(g, aName.substr(0, aLine1Len), aPosX + 52, aPosY + 88, Sexy::FONT_BRIANNETOD12, aTextColor, DS_ALIGN_CENTER);
				if (aLine2Len > 0)
				{
					TodDrawString(g, aName.substr(aLine1Len + 1, aLine2Len), aPosX + 52, aPosY + 102, Sexy::FONT_BRIANNETOD12, aTextColor, DS_ALIGN_CENTER);
				}
			}

			// ============================================================================================
			// ▲ 绘制关卡锁定或关卡完成的贴图以及关卡最高记录的文本等
			// ============================================================================================
			// 存档 v13：记录槽=GameMode 直下标（行号与模式值不再恒等）
			int aRecord = mApp->mPlayerInfo->mChallengeRecords[(int)aDef.mChallengeMode];
			if (theChallengeIndex == mUnlockChallengeIndex)
			{
				Image* aLockImage = Sexy::IMAGE_LOCK;
				if (mUnlockState == UNLOCK_FADING)
				{
					aLockImage = Sexy::IMAGE_LOCK_OPEN;
					g->mColor = Color(255, 255, 255, TodAnimateCurveNs(250'000'000, 0, mUnlockStateCounter.mNanoseconds, 255, 0, Curves::Linear));
					g->mColorizeImages = true;
				}
				TodDrawImageScaledF(g, aLockImage, aPosX + 24 + mLockShakeX, aPosY + 9 + mLockShakeY, 0.7f, 0.7f);
				g->mColorizeImages = false;
			}
			else if (aRecord > 0)
			{
				if (mApp->HasBeatenChallenge(aDef.mChallengeMode))
				{
					g->DrawImage(Sexy::IMAGE_MINIGAME_TROPHY, aPosX - 6, aPosY - 2);
				}
				else if (mApp->IsSurvivalEndless(aDef.mChallengeMode))
				{
					SexyString aAchievement = mApp->Pluralize(aRecord, gStr.ONE_FLAG.c_str(), gStr.COUNT_FLAGS.c_str());
					TodDrawString(g, aAchievement, aPosX + 48, aPosY + 48, Sexy::FONT_CONTINUUMBOLD14OUTLINE, Color::White, DS_ALIGN_CENTER);
					TodDrawString(g, aAchievement, aPosX + 48, aPosY + 48, Sexy::FONT_CONTINUUMBOLD14, Color(255, 0, 0), DS_ALIGN_CENTER);
				}
				else if (mApp->IsEndlessScaryPotter(aDef.mChallengeMode) || mApp->IsEndlessIZombie(aDef.mChallengeMode))
				{
					SexyString aAchievement = TodReplaceNumberString(gStr.LONGEST_STREAK, UiTextPlaceholder::Streak, aRecord);
					Rect aRect(aPosX, aPosY + 15, 96, 200);
					TodDrawStringWrapped(g, aAchievement, aRect, Sexy::FONT_CONTINUUMBOLD14OUTLINE, Color::White, DS_ALIGN_CENTER);
					TodDrawStringWrapped(g, aAchievement, aRect, Sexy::FONT_CONTINUUMBOLD14, Color(255, 0, 0), DS_ALIGN_CENTER);
				}
			}
			else if (aChallengeButton->mDisabled)
			{
				TodDrawImageScaledF(g, Sexy::IMAGE_LOCK, aPosX + 24, aPosY + 9, 0.7f, 0.7f);
			}
		}
		else
		{
			g->DrawImage(Sexy::IMAGE_CHALLENGE_BLANK, aPosX, aPosY);
		}
	}
}

//0x42F160
void ChallengeScreen::Draw(Graphics* g)
{
    mBackButton->mX = mApp->mScreenBounds.mX + 18;
	g->mLinearBlend = true;
	Image* aBackgroundImage = Sexy::IMAGE_CHALLENGE_BACKGROUND;
	const Rect& aBounds = mApp->mScreenBounds;
	const int aBorderWidth = 16;
	const int aPaperWidth = aBounds.mWidth - aBorderWidth * 2;
	// Keep paper and the title plaque in the original 800-wide world coordinates.
	// Move only the native stone edge strips as the viewport changes, including odd widths.
	const int aArtPadding = (aBackgroundImage->mWidth - BOARD_WIDTH) / 2;
	const int aSourceX = aArtPadding + aBounds.mX + aBorderWidth;
	Graphics aBackground(*g);
	aBackground.ClipRect(aBounds);
	aBackground.DrawImage(aBackgroundImage, aBounds.mX, 0,
		Rect(0, 0, aBorderWidth, aBackgroundImage->mHeight));
	aBackground.DrawImage(aBackgroundImage, aBounds.mX + aBorderWidth, 0,
		Rect(aSourceX, 0, aPaperWidth, aBackgroundImage->mHeight));
	aBackground.DrawImage(aBackgroundImage, aBounds.mX + aBounds.mWidth - aBorderWidth, 0,
		Rect(aBackgroundImage->mWidth - aBorderWidth, 0, aBorderWidth, aBackgroundImage->mHeight));

	SexyString aTitleString = 
		mPageIndex == CHALLENGE_PAGE_SURVIVAL ? gStr.PICK_AREA.c_str() : 
		mPageIndex == CHALLENGE_PAGE_PUZZLE ? gStr.SCARY_POTTER.c_str() : gStr.PICK_CHALLENGE.c_str();
	TodDrawString(g, aTitleString, 400, 58, Sexy::FONT_HOUSEOFTERROR28, Color(220, 220, 220), DS_ALIGN_CENTER);

	int aTrophiesGot = mApp->GetNumTrophies(mPageIndex);
	int aTrophiesTotal = mPageIndex == CHALLENGE_PAGE_SURVIVAL ? 10 : mPageIndex == CHALLENGE_PAGE_CHALLENGE ? 20 : mPageIndex == CHALLENGE_PAGE_PUZZLE ? 18 : 0;
	if (aTrophiesTotal > 0)
	{
		// C++23 第一批·条目9：纯整型 to_chars 直填（原 StrFormat("%d/%d")，输出逐字节等价）
		char aGotBuf[16], aTotalBuf[16];
		SexyString aTrophyString(FormatInt(aGotBuf, aTrophiesGot));
		aTrophyString += ('/');
		aTrophyString += FormatInt(aTotalBuf, aTrophiesTotal);
		TodDrawString(g, aTrophyString, 739, 73, Sexy::FONT_DWARVENTODCRAFT15, Color(255, 240, 0), DS_ALIGN_CENTER);
	}
	TodDrawImageScaledF(g, Sexy::IMAGE_TROPHY, 718, 26, 0.5f, 0.5f);

	for (int aChallengeMode = 0; aChallengeMode < NUM_CHALLENGE_MODES; aChallengeMode++)
		DrawButton(g, aChallengeMode);

	mToolTip->Draw(g);
}

//0x42F510
void ChallengeScreen::Update()
{
    mBackButton->mX = mApp->mScreenBounds.mX + 18;
	Widget::Update();
	UpdateToolTip();

	if (mUnlockStateCounter > 0) mUnlockStateCounter--;
	if (mUnlockState == UNLOCK_SHAKING)
	{
		if (mUnlockStateCounter == 0)
		{
			mApp->PlayFoley(FOLEY_PAPER);
			mUnlockState = UNLOCK_FADING;
			mUnlockStateCounter = 50;
			mLockShakeX = 0;
			mLockShakeY = 0;
		}
		else
		{
			mLockShakeX = RandRangeFloat(-2, 2);
			mLockShakeY = RandRangeFloat(-2, 2);
		}
	}
	else if (mUnlockState == UNLOCK_FADING && mUnlockStateCounter == 0)
	{
		mUnlockState = UNLOCK_OFF;
		mUnlockStateCounter = 0;
		mUnlockChallengeIndex = -1;
	}

	MarkDirty();
}

//0x42F640
void ChallengeScreen::AddedToManager(WidgetManager* theWidgetManager)
{
	Widget::AddedToManager(theWidgetManager);
	AddWidget(mBackButton);
	for (ButtonWidget* aButton : mPageButton) AddWidget(aButton);
	for (ButtonWidget* aButton : mChallengeButtons) AddWidget(aButton);
}

//0x42F6B0
void ChallengeScreen::RemovedFromManager(WidgetManager* theWidgetManager)
{
	Widget::RemovedFromManager(theWidgetManager);
	RemoveWidget(mBackButton);
	for (ButtonWidget* aButton : mPageButton) RemoveWidget(aButton);
	for (ButtonWidget* aButton : mChallengeButtons) RemoveWidget(aButton);
}

//0x42F720
void ChallengeScreen::ButtonPress(int theId)
{
	(void)theId;
	mApp->PlaySample(Sexy::SOUND_BUTTONCLICK);
}

//0x42F740
void ChallengeScreen::ButtonDepress(int theId)
{
	if (theId == ChallengeScreen::ChallengeScreen_Back)
	{
		mApp->KillChallengeScreen();
		mApp->DoBackToMain();
	}

	int aChallengeMode = theId - ChallengeScreen::ChallengeScreen_Mode;
	if (aChallengeMode >= 0 && aChallengeMode < NUM_CHALLENGE_MODES)
	{
		if (GetChallengeDefinition(aChallengeMode).mChallengeMode == GAMEMODE_CUSTOM_LEVEL)
			return;
		mApp->KillChallengeScreen();
		// 行号 → 模式必须走行内字段：原版 index+1 恒等式只在 v1 枚举序下
		// 成立，v3 改名聚簇后该算术把每个按钮都指去错误的玩法。
		mApp->PreNewGame(GetChallengeDefinition(aChallengeMode).mChallengeMode, true);
	}

	int aPageIndex = theId - ChallengeScreen::ChallengeScreen_Page;
	if (aPageIndex >= 0 && aPageIndex < 4)
	{
		mPageIndex = (ChallengePage)aPageIndex;
		UpdateButtons();
	}
}

//0x42F7E0
void ChallengeScreen::UpdateToolTip()
{
	if (!mApp->mWidgetManager->mMouseIn || !mApp->mActive)
	{
		mToolTip->mVisible = false;
		return;
	}

	for (int aChallengeMode = 0; aChallengeMode < NUM_CHALLENGE_MODES; aChallengeMode++)
	{
		ChallengeDefinition& aDef = GetChallengeDefinition(aChallengeMode);
		ButtonWidget* aChallengeButton = mChallengeButtons[aChallengeMode];
		if (aChallengeButton->mVisible && aChallengeButton->mDisabled &&
			aChallengeButton->Contains(mApp->mWidgetManager->mLastMouseX, mApp->mWidgetManager->mLastMouseY) &&
			AccomplishmentsNeeded(aChallengeMode) <= 1)
		{
			mToolTip->mX = aChallengeButton->mWidth / 2 + aChallengeButton->mX;
			mToolTip->mY = aChallengeButton->mY;
			if (MoreTrophiesNeeded(aChallengeMode) > 0)
			{
				SexyString aLabel;
				if (mPageIndex == CHALLENGE_PAGE_PUZZLE)
				{
					if (IsScaryPotterLevel(aDef.mChallengeMode))
					{
						if (!mApp->HasFinishedAdventure() && aDef.mChallengeMode == GAMEMODE_SCARY_POTTER_4)
						{
							aLabel = gStr.FINISH_ADVENTURE_TOOLTIP.c_str();
						}
						else
						{
							aLabel = gStr.ONE_MORE_SCARY_POTTER_TOOLTIP.c_str();
						}
					}
					else if (IsIZombieLevel(aDef.mChallengeMode))
					{
						if (!mApp->HasFinishedAdventure() && aDef.mChallengeMode == GAMEMODE_PUZZLE_I_ZOMBIE_4)
						{
							aLabel = gStr.FINISH_ADVENTURE_TOOLTIP.c_str();
						}
						else
						{
							aLabel = gStr.ONE_MORE_IZOMBIE_TOOLTIP.c_str();
						}
					}
				}
				else if (!mApp->HasFinishedAdventure() || mApp->IsTrialStageLocked())
				{
					aLabel = gStr.FINISH_ADVENTURE_TOOLTIP.c_str();
				}
				else if (mApp->IsSurvivalEndless(aDef.mChallengeMode))
				{
					aLabel = ("[10_SURVIVAL_TOOLTIP]");
				}
				else if (mPageIndex == CHALLENGE_PAGE_SURVIVAL)
				{
					aLabel = gStr.ONE_MORE_SURVIVAL_TOOLTIP.c_str();
				}
				else if (mPageIndex == CHALLENGE_PAGE_CHALLENGE)
				{
					aLabel = gStr.ONE_MORE_CHALLENGE_TOOLTIP.c_str();
				}
				else continue;

				mToolTip->SetLabel(aLabel);
				mToolTip->mVisible = true;
				return;
			} // end if (MoreTrophiesNeeded(aChallengeMode) > 0)
		} // end 需要显示标签的条件判断
	}

	mToolTip->mVisible = false;
}
