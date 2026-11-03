#include "Sexy.TodLib/generated/EntityStrings.h"  // 动态键实体表（阶段 3）
#include "Lawn/Board.h"
#include "Lawn/Plant.h"
#include "Lawn/Zombie.h"
#include "GameButton.h"
#include "Lawn/SeedPacket.h"
#include "LawnApp.h"
#include "graphics/DDInterface.h"
#include "Almanac.h"
#include "Resources.h"
#include "Lawn/System/Music.h"
#include "GameConstants.h"
#include "Lawn/System/PlayerInfo.h"
#include "Lawn/System/PoolEffect.h"
#include "Lawn/System/ReanimationLawn.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/Reanimator.h"
#include "widget/WidgetManager.h"
#include "Sexy.TodLib/generated/SceneAtlasData.h"
#include "graphics/AtlasDraw.h"

static bool gZombieDefeated[NUM_ZOMBIE_TYPES] = { false };

namespace
{
using AlmanacSprite = AlmanacAtlas::Sprite;
void RefreshAlmanacLayout(AlmanacDialog& theDialog)
{
	const Rect& aBounds = theDialog.mApp->mScreenBounds;
	// Battle viewports can be asymmetric while the page remains 800 units wide.
	theDialog.Move(aBounds.mX + (aBounds.mWidth - theDialog.mWidth) / 2, theDialog.mY);
	const int aRight = aBounds.mX + aBounds.mWidth;
	// Keep the original 800-wide page's button margins at the visible edges.
	theDialog.mIndexButton->mX = aBounds.mX - theDialog.mX + 32;
	theDialog.mCloseButton->mX = aRight - theDialog.mX - 35 - theDialog.mCloseButton->mWidth;
	theDialog.mMouseInsets.mLeft = std::min(0, aBounds.mX - theDialog.mX);
	theDialog.mMouseInsets.mRight = std::min(0, theDialog.mX + theDialog.mWidth - aRight);
}

void DrawAlmanacBackground(Graphics* g, const TextureSource& theTexture,
	const TextureRegion& theImage, const Rect& theBounds, int theOriginX)
{
	const int aArtPadding = (theImage.mWidth - BOARD_WIDTH) / 2;
	const int aBorderWidth = 16;
	const int aPaperWidth = theBounds.mWidth - aBorderWidth * 2;
	const int aLeft = theBounds.mX - theOriginX;
	const int aSourceX = aArtPadding + aLeft + aBorderWidth;
	Graphics aBackground(*g);
	aBackground.ClipRect(aLeft, theBounds.mY, theBounds.mWidth, theBounds.mHeight);
	// Crop the fixed native canvas; only the frame edges follow the visible bounds.
	DrawAtlasImage(&aBackground,theTexture,theImage,
		Rect(aLeft,0,aBorderWidth,theImage.mHeight),
		Rect(0,0,aBorderWidth,theImage.mHeight));
	DrawAtlasImage(&aBackground,theTexture,theImage,
		Rect(aLeft+aBorderWidth,0,aPaperWidth,theImage.mHeight),
		Rect(aSourceX,0,aPaperWidth,theImage.mHeight));
	DrawAtlasImage(&aBackground,theTexture,theImage,
		Rect(aLeft+theBounds.mWidth-aBorderWidth,0,aBorderWidth,theImage.mHeight),
		Rect(theImage.mWidth-aBorderWidth,0,aBorderWidth,theImage.mHeight));
}

void DrawAlmanacScrollbar(Graphics* g, const Rect& theDescRect, int theScroll, int theScrollMax)
{
	if (theScrollMax <= 0)
		return;

	Rect aTrack(theDescRect.mX + theDescRect.mWidth + 2, theDescRect.mY, 8, theDescRect.mHeight);
	g->mColor = Color(0, 0, 0, 60);
	g->FillRect(aTrack);

	int aTotal = theScrollMax + theDescRect.mHeight;
	int aThumbHeight = std::max(14, aTrack.mHeight * theDescRect.mHeight / aTotal);
	int aTravel = aTrack.mHeight - aThumbHeight;
	int aThumbY = aTrack.mY + (aTravel * theScroll) / std::max(1, theScrollMax);
	Rect aThumb(aTrack.mX, aThumbY, aTrack.mWidth, aThumbHeight);
	g->mColor = Color(190, 160, 90);
	g->FillRect(aThumb);
	g->mColor = Color(120, 95, 50);
	g->FillRect(Rect(aThumb.mX, aThumb.mY, aThumb.mWidth, 1));
	g->FillRect(Rect(aThumb.mX, aThumb.mY + aThumb.mHeight - 1, aThumb.mWidth, 1));
}

void UpdateAlmanacScroll(int theY, const Rect& theDescRect, int& theScroll, int theScrollMax)
{
	if (theScrollMax <= 0)
	{
		theScroll = 0;
		return;
	}

	Rect aTrack(theDescRect.mX + theDescRect.mWidth + 2, theDescRect.mY, 8, theDescRect.mHeight);
	int aTotal = theScrollMax + theDescRect.mHeight;
	int aThumbHeight = std::max(14, aTrack.mHeight * theDescRect.mHeight / aTotal);
	int aTravel = aTrack.mHeight - aThumbHeight;
	if (aTravel <= 0)
	{
		theScroll = 0;
		return;
	}

	int aY = theY - theDescRect.mY - aThumbHeight / 2;
	float aRatio = (float)aY / (float)aTravel;
	theScroll = (int)(aRatio * theScrollMax);
	theScroll = std::min(std::max(0, theScroll), theScrollMax);
}
}

//0x401010
AlmanacDialog::AlmanacDialog(LawnApp* theApp) : LawnDialog(theApp, DIALOG_ALMANAC, true, ("Almanac"), (""), (""), BUTTONS_NONE)
{
	mApp = (LawnApp*)gSexyAppBase;
	mOpenPage = ALMANAC_PAGE_INDEX;
	mSelectedSeed = SEED_PEASHOOTER;
	mSelectedZombie = ZOMBIE_NORMAL;
	mPlantDescScroll = 0;
	mPlantDescScrollMax = 0;
	mZombieDescScroll = 0;
	mZombieDescScrollMax = 0;
	mDescScrollBarDragging = false;
	mZombie = nullptr;
	mPlant = nullptr;
	mDrawStandardBack = false;
	mClip = false;
	TodLoadResources(ResourceGroupId::DelayLoad_Almanac);
	LawnDialog::Resize(0, 0, BOARD_WIDTH, BOARD_HEIGHT);

	mCloseButton = new GameButton(AlmanacDialog::ALMANAC_BUTTON_CLOSE);
	mCloseButton->mLabel = gStr.CLOSE_BUTTON;
	mCloseButton->mNormalRegion = AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_CLOSEBUTTON)];
	mCloseButton->mOverRegion = AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_CLOSEBUTTONHIGHLIGHT)];
	mCloseButton->mDownImage = nullptr;
	mCloseButton->SetFont(Sexy::FONT_BRIANNETOD12);
	Color aColor = Color(42, 42, 90);
	mCloseButton->mColors[ButtonWidget::COLOR_LABEL] = aColor;
	mCloseButton->mColors[ButtonWidget::COLOR_LABEL_HILITE] = aColor;
	mCloseButton->mX = 676;
	mCloseButton->mY = 567;
	mCloseButton->mWidth = 89;
	mCloseButton->mHeight = 26;
	mCloseButton->mParentWidget = this;
	mCloseButton->mTextOffsetX = -8;
	mCloseButton->mTextOffsetY = 1;

	mIndexButton = new GameButton(AlmanacDialog::ALMANAC_BUTTON_INDEX);
	mIndexButton->mLabel = gStr.ALMANAC_INDEX;
	mIndexButton->mNormalRegion = AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_INDEXBUTTON)];
	mIndexButton->mOverRegion = AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_INDEXBUTTONHIGHLIGHT)];
	mIndexButton->mDownImage = nullptr;
	mIndexButton->SetFont(Sexy::FONT_BRIANNETOD12);
	mIndexButton->mColors[ButtonWidget::COLOR_LABEL] = aColor;
	mIndexButton->mColors[ButtonWidget::COLOR_LABEL_HILITE] = aColor;
	mIndexButton->mX = 32;
	mIndexButton->mY = 567;
	mIndexButton->mWidth = 164;
	mIndexButton->mHeight = 26;
	mIndexButton->mParentWidget = this;
	mIndexButton->mTextOffsetX = 8;
	mIndexButton->mTextOffsetY = 1;

	mPlantButton = new GameButton(AlmanacDialog::ALMANAC_BUTTON_PLANT);
	mPlantButton->mLabel = gStr.VIEW_PLANTS;
	mPlantButton->mNormalRegion = FixedUIAtlas::Images[size_t(FixedUIAtlas::Sprite::SEEDCHOOSER_BUTTON)];
	mPlantButton->mOverImage = nullptr;
	mPlantButton->mDownImage = nullptr;
	mPlantButton->mDisabledRegion = FixedUIAtlas::Images[size_t(FixedUIAtlas::Sprite::SEEDCHOOSER_BUTTON_DISABLED)];
	mPlantButton->SetFont(Sexy::FONT_DWARVENTODCRAFT18YELLOW);
	mPlantButton->mColors[ButtonWidget::COLOR_LABEL] = Color::White;
	mPlantButton->mColors[ButtonWidget::COLOR_LABEL_HILITE] = Color::White;
	mPlantButton->mX = 130;
	mPlantButton->mY = 345;
	mPlantButton->mWidth = 156;
	mPlantButton->mHeight = 42;
	mPlantButton->mTextOffsetY = -1;
	mPlantButton->mParentWidget = this;

	mZombieButton = new GameButton(AlmanacDialog::ALMANAC_BUTTON_ZOMBIE);
	mZombieButton->mLabel = gStr.VIEW_ZOMBIES;
	mZombieButton->mX = 487;
	mZombieButton->mY = 345;
	mZombieButton->mWidth = 210;
	mZombieButton->mHeight = 48;
	mZombieButton->mDrawStoneButton = true;
	mZombieButton->mParentWidget = this;

	SetPage(ALMANAC_PAGE_INDEX);
	RefreshAlmanacLayout(*this);
	if (!mApp->mBoard || !mApp->mBoard->mPaused)
		mApp->mMusic->MakeSureMusicIsPlaying(MUSIC_TUNE_CHOOSE_YOUR_SEEDS);
}

//0x401880 & 0x4018A0
AlmanacDialog::~AlmanacDialog()
{
	if (mCloseButton)	delete mCloseButton;
	if (mIndexButton)	delete mIndexButton;
	if (mPlantButton)	delete mPlantButton;
	if (mZombieButton)	delete mZombieButton;

	ClearPlantsAndZombies();
}

//0x401970
void AlmanacDialog::ClearPlantsAndZombies()
{
	const auto visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	if (mPlant)
	{
		mPlant->Die(visualNowNs, visualNowNs, visualNowNs);
		delete mPlant;
		mPlant = nullptr;
	}
	if (mZombie)
	{
		mZombie->DieNoLoot(visualNowNs, visualNowNs, visualNowNs);
		delete mZombie;
		mZombie = nullptr;
	}
}

//0x401A10
void AlmanacDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
	LawnDialog::RemovedFromManager(theWidgetManager);
	ClearPlantsAndZombies();
}

//0x401A30
// GOTY @Patoke: 0x402C50
void AlmanacDialog::SetupPlant()
{
	const auto visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	ClearPlantsAndZombies();

	float aPosX = ALMANAC_PLANT_POSITION_X;
	float aPosY = ALMANAC_PLANT_POSITION_Y;
	if (mSelectedSeed == SEED_TALLNUT)				aPosY += 18;
	else if (mSelectedSeed == SEED_COBCANNON)		aPosX -= 40;
	else if (mSelectedSeed == SEED_FLOWERPOT)		aPosY -= 20;
	else if (mSelectedSeed == SEED_INSTANT_COFFEE)	aPosY += 20;
	else if (mSelectedSeed == SEED_GRAVEBUSTER)		aPosY += 55;

	mPlant = new Plant();
	mPlant->mBoard = nullptr;
	mPlant->mIsOnBoard = false;
	mPlant->PlantInitialize(visualNowNs, visualNowNs, visualNowNs, 0, 0, mSelectedSeed, SEED_NONE);
	{ mPlant->mX = (aPosX); mPlant->mRootBindingGeneration = 0; }
	mPlant->mY = (aPosY);
}

//0x401B70
// GOTY @Patoke: 0x402D90
void AlmanacDialog::SetupZombie()
{
	const auto visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	ClearPlantsAndZombies();

	mZombie = new Zombie();
	mZombie->mBoard = nullptr;
	mZombie->ZombieInitialize(visualNowNs, visualNowNs, visualNowNs, 0, mSelectedZombie, false, nullptr, Zombie::ZOMBIE_WAVE_UI);
	{ mZombie->mX = (ALMANAC_ZOMBIE_POSITION_X); mZombie->mRootBindingGeneration = 0; }
	mZombie->mY = (ALMANAC_ZOMBIE_POSITION_Y);
}

//0x401BE0
void AlmanacDialog::SetPage(AlmanacPage thePage)
{
	const auto visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	mOpenPage = thePage;
	ClearPlantsAndZombies();

	if (mOpenPage == AlmanacPage::ALMANAC_PAGE_INDEX)
	{
		mPlant = new Plant();
		mPlant->mBoard = nullptr;
		mPlant->mIsOnBoard = false;
		mPlant->PlantInitialize(visualNowNs, visualNowNs, visualNowNs, 0, 0, SeedType::SEED_SUNFLOWER, SeedType::SEED_NONE);
		{ mPlant->mX = (ALMANAC_INDEXPLANT_POSITION_X); mPlant->mRootBindingGeneration = 0; }
		mPlant->mY = (ALMANAC_INDEXPLANT_POSITION_Y);

		mZombie = new Zombie();
		mZombie->mBoard = nullptr;
		mZombie->ZombieInitialize(visualNowNs, visualNowNs, visualNowNs, 0, ZombieType::ZOMBIE_NORMAL, false, nullptr, Zombie::ZOMBIE_WAVE_UI);
		{ mZombie->mX = (ALMANAC_INDEXZOMBIE_POSITION_X); mZombie->mRootBindingGeneration = 0; }
		mZombie->mY = (ALMANAC_INDEXZOMBIE_POSITION_Y);

		mIndexButton->mBtnNoDraw = true;
		mPlantButton->mBtnNoDraw = false;
		mZombieButton->mBtnNoDraw = false;
	}
	else
	{
		if (mOpenPage == AlmanacPage::ALMANAC_PAGE_PLANTS)
			SetupPlant();
		else if (mOpenPage == AlmanacPage::ALMANAC_PAGE_ZOMBIES)
			SetupZombie();
		else return;

		mIndexButton->mBtnNoDraw = false;
		mPlantButton->mBtnNoDraw = true;
		mZombieButton->mBtnNoDraw = true;
	}
}

void AlmanacDialog::ShowPlant(SeedType theSeedType)
{
	mSelectedSeed = theSeedType;
	mPlantDescScroll = 0;
	SetPage(ALMANAC_PAGE_PLANTS);
}

void AlmanacDialog::ShowZombie(ZombieType theZombieType)
{
	mSelectedZombie = theZombieType;
	mZombieDescScroll = 0;
	SetPage(ALMANAC_PAGE_ZOMBIES);
}

//0x401D30
void AlmanacDialog::Update()
{
	const auto visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	RefreshAlmanacLayout(*this);
	mCloseButton->Update();
	mIndexButton->Update();
	mPlantButton->Update();
	mZombieButton->Update();
	if (mPlant) mPlant->Update(visualNowNs, visualNowNs, visualNowNs, 0);
	if (mZombie) mZombie->Update(visualNowNs, visualNowNs, visualNowNs, Sexy::gUpdateIntervalNs);

	int aMouseX = mApp->mWidgetManager->mLastMouseX - mX;
	int aMouseY = mApp->mWidgetManager->mLastMouseY - mY;
	if (SeedHitTest(aMouseX, aMouseY) != SeedType::SEED_NONE || ZombieHitTest(aMouseX, aMouseY) != ZombieType::ZOMBIE_INVALID ||
		mCloseButton->IsMouseOver() || mIndexButton->IsMouseOver() || mPlantButton->IsMouseOver() || mZombieButton->IsMouseOver())
	{
		mApp->SetCursor(CURSOR_HAND);
	}
	else
	{
		mApp->SetCursor(CURSOR_POINTER);
	}

	mApp->mPoolEffect->PoolEffectUpdate(visualNowNs, Sexy::gUpdateIntervalNs);
	MarkDirty();
}

//0x401E70
void AlmanacDialog::DrawIndex(Graphics* g)
{
	const auto visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	const TextureSource aTexture{AlmanacAtlas::Texture, 0};
	DrawAlmanacBackground(g,aTexture,AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_INDEXBACK)],
		mApp->mScreenBounds,mX);
	Graphics aContentGraphics(*g);
	aContentGraphics.ClipRect(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
	g = &aContentGraphics;
	TodDrawString(g, gStr.SUBURBAN_ALMANAC_INDEX, BOARD_WIDTH / 2, 60, Sexy::FONT_HOUSEOFTERROR28, Color(220, 220, 220), DrawStringJustification::DS_ALIGN_CENTER);

	if (mPlant)
	{
		Graphics aPlantGraphics = Graphics(*g);
		mPlant->BeginDraw(&aPlantGraphics);
		mPlant->Draw(visualNowNs, &aPlantGraphics);
	}
	if (mZombie)
	{
		Graphics aZombieGraphics = Graphics(*g);
		mZombie->BeginDraw(&aZombieGraphics);
		mZombie->Draw(visualNowNs, &aZombieGraphics);
	}
}

//0x402060
void AlmanacDialog::DrawPlants(Graphics* g)
{
	const auto visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	const TextureSource aTexture{AlmanacAtlas::Texture, 0};
	DrawAlmanacBackground(g,aTexture,AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_PLANTBACK)],
		mApp->mScreenBounds,mX);
	Graphics aContentGraphics(*g);
	aContentGraphics.ClipRect(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
	g = &aContentGraphics;
	TodDrawString(g, gStr.SUBURBAN_ALMANAC_PLANTS, BOARD_WIDTH / 2, 48, Sexy::FONT_HOUSEOFTERROR20, Color(213, 159, 43), DrawStringJustification::DS_ALIGN_CENTER);

	SeedType aSeedMouseOn = SeedHitTest(mApp->mWidgetManager->mLastMouseX - mX, mApp->mWidgetManager->mLastMouseY - mY);
	for (SeedType aSeedType = SeedType::SEED_PEASHOOTER; aSeedType < NUM_ALMANAC_SEEDS; aSeedType = (SeedType)(aSeedType + 1))
	{
		int aPosX, aPosY;
		GetSeedPosition(aSeedType, aPosX, aPosY);
		if (mApp->SeedTypeAvailable(aSeedType))
		{
			if (aSeedType == SeedType::SEED_IMITATER)
			{
				if (aSeedType == aSeedMouseOn)
					DrawAtlasImage(g,aTexture,AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_IMITATER)],aPosX,aPosY);
				DrawAtlasImage(g,aTexture,AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_IMITATER)],aPosX,aPosY);
			}
			else
			{
				DrawSeedPacket(g, aPosX, aPosY, aSeedType, SeedType::SEED_NONE, 0, 255, true, false);
				if (aSeedType == aSeedMouseOn)
					g->DrawImage(Sexy::IMAGE_SEEDPACKETFLASH, aPosX, aPosY);
			}
		}
	}

	if (mSelectedSeed == SeedType::SEED_LILYPAD || mSelectedSeed == SeedType::SEED_TANGLEKELP ||
		mSelectedSeed == SeedType::SEED_CATTAIL || mSelectedSeed == SeedType::SEED_SEASHROOM)
	{
		bool aNight = mSelectedSeed == SeedType::SEED_SEASHROOM;
		const auto aGround=aNight ? AlmanacSprite::ALMANAC_GROUNDNIGHTPOOL : AlmanacSprite::ALMANAC_GROUNDPOOL;
		DrawAtlasImage(g,aTexture,AlmanacAtlas::Images[static_cast<unsigned>(aGround)],521,107);

		Graphics aPoolGraphics(*g);
		aPoolGraphics.ClipRect(475, 0, 397, 500);
		aPoolGraphics.Translate(0, -145);
		mApp->mPoolEffect->PoolEffectDraw(&aPoolGraphics, aNight);
	}
	else
	{
		const auto aGround=
			Plant::IsNocturnal(mSelectedSeed) || mSelectedSeed == SeedType::SEED_GRAVEBUSTER || mSelectedSeed == SeedType::SEED_PLANTERN ? AlmanacSprite::ALMANAC_GROUNDNIGHT :
			mSelectedSeed == SeedType::SEED_FLOWERPOT ? AlmanacSprite::ALMANAC_GROUNDROOF : AlmanacSprite::ALMANAC_GROUNDDAY;
		DrawAtlasImage(g,aTexture,AlmanacAtlas::Images[static_cast<unsigned>(aGround)],521,107);
	}

	if (mPlant)
	{
		Graphics aPlantGraphics = Graphics(*g);
		mPlant->BeginDraw(&aPlantGraphics);
		mPlant->Draw(visualNowNs, &aPlantGraphics);
	}

	DrawAtlasImage(g,aTexture,AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_PLANTCARD)],459,86);
	PlantDefinition& aPlantDef = GetPlantDefinition(mSelectedSeed);
	SexyString aName = Plant::GetNameString(mSelectedSeed, SEED_NONE);
	SexyString aDescriptionName = EntityPlantDescription(mSelectedSeed);
	TodDrawString(g, aName, 617, 288, Sexy::FONT_DWARVENTODCRAFT18YELLOW, Color::White, DS_ALIGN_CENTER);
	Rect aDescRect(485, 309, 258, 205); // stop above the Cost/Recharge row (y=520)
	int aTextHeight = TodDrawStringWrappedHelper(g, aDescriptionName, aDescRect, Sexy::FONT_BRIANNETOD12, Color(40, 50, 90), DS_ALIGN_LEFT, false);
	mPlantDescScrollMax = std::max(0, aTextHeight - aDescRect.mHeight);
	mPlantDescScroll = std::min(std::max(0, mPlantDescScroll), mPlantDescScrollMax);
	{
		GraphicsAutoState aState(g);
		g->SetClipRect(aDescRect);
		Rect aScrollRect = aDescRect;
		aScrollRect.mY -= mPlantDescScroll;
		TodDrawStringWrapped(g, aDescriptionName, aScrollRect, Sexy::FONT_BRIANNETOD12, Color(40, 50, 90), DS_ALIGN_LEFT);
	}
	DrawAlmanacScrollbar(g, aDescRect, mPlantDescScroll, mPlantDescScrollMax);

	if (mSelectedSeed != SeedType::SEED_IMITATER)
	{
		SexyString aCostStr = TodReplaceString(
			UiTextFormatToken(StringFormatId::Keyword) + UiTextPlaceholderToken(UiTextPlaceholder::Cost) +
			":" + UiTextFormatToken(StringFormatId::Stat) + StrFormat(" %d", aPlantDef.mSeedCost),
			UiTextPlaceholder::Cost, gStr.COST);
		TodDrawStringWrapped(g, aCostStr, Rect(485, 520, 134, 50), Sexy::FONT_BRIANNETOD12, Color::White, DS_ALIGN_LEFT);

		SexyString aRechargeStr = TodReplaceString(
			UiTextFormatToken(StringFormatId::Keyword) + UiTextPlaceholderToken(UiTextPlaceholder::WaitTime) +
			":" + UiTextFormatToken(StringFormatId::Stat) + " " + UiTextPlaceholderToken(UiTextPlaceholder::WaitTimeLength),
			UiTextPlaceholder::WaitTimeLength,
			aPlantDef.mRefreshTime == 750 ? gStr.WAIT_TIME_SHORT : aPlantDef.mRefreshTime == 3000 ? gStr.WAIT_TIME_LONG : gStr.WAIT_TIME_VERY_LONG // @Patoke: fix typo XD
		);
		aRechargeStr = TodReplaceString(aRechargeStr, UiTextPlaceholder::WaitTime, gStr.WAIT_TIME);
		TodDrawStringWrapped(g, aRechargeStr, Rect(600, 520, 139, 50), Sexy::FONT_BRIANNETOD12, Color(40, 50, 90), DS_ALIGN_RIGHT);
	}
}

namespace
{
// kAlmanacIconOffset：生成器解析原 switch 生成（逐槽对拍 0 差异）；
// 哨兵 0x7FFFFFFF, 0x7FFFFFFF = 无 case 槽（default 无操作）。
constexpr const int kAlmanacIconOffset[(int)ZombieType::NUM_ZOMBIE_TYPES][2] = {
		{0x7FFFFFFF, 0x7FFFFFFF},  // 0 ZOMBIE_NORMAL（无 case）
		{2, 10},  // 1 ZOMBIE_FLAG
		{0, 12},  // 2 ZOMBIE_TRAFFIC_CONE
		{0x7FFFFFFF, 0x7FFFFFFF},  // 3 ZOMBIE_POLEVAULTER（无 case）
		{0, 9},  // 4 ZOMBIE_PAIL
		{0x7FFFFFFF, 0x7FFFFFFF},  // 5 ZOMBIE_NEWSPAPER（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 6 ZOMBIE_DOOR（无 case）
		{-15, -1},  // 7 ZOMBIE_FOOTBALL
		{0, 15},  // 8 ZOMBIE_DANCER
		{-5, 20},  // 9 ZOMBIE_BACKUP_DANCER
		{0x7FFFFFFF, 0x7FFFFFFF},  // 10 ZOMBIE_DUCKY_TUBE（无 case）
		{-10, 0},  // 11 ZOMBIE_SNORKEL
		{0, 3},  // 12 ZOMBIE_ZAMBONI
		{0, -8},  // 13 ZOMBIE_BOBSLED
		{-2, -10},  // 14 ZOMBIE_DOLPHIN_RIDER
		{0x7FFFFFFF, 0x7FFFFFFF},  // 15 ZOMBIE_JACK_IN_THE_BOX（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 16 ZOMBIE_BALLOON（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 17 ZOMBIE_DIGGER（无 case）
		{0, -3},  // 18 ZOMBIE_POGO
		{0, 4},  // 19 ZOMBIE_YETI
		{-4, 3},  // 20 ZOMBIE_BUNGEE
		{0, -3},  // 21 ZOMBIE_LADDER
		{-24, -1},  // 22 ZOMBIE_CATAPULT
		{15, 17},  // 23 ZOMBIE_GARGANTUAR
		{-8, -7},  // 24 ZOMBIE_IMP
		{0x7FFFFFFF, 0x7FFFFFFF},  // 25 ZOMBIE_BOSS（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 26 ZOMBIE_PEA_HEAD（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 27 ZOMBIE_WALLNUT_HEAD（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 28 ZOMBIE_JALAPENO_HEAD（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 29 ZOMBIE_GATLING_HEAD（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 30 ZOMBIE_SQUASH_HEAD（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 31 ZOMBIE_TALLNUT_HEAD（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 32 ZOMBIE_REDEYE_GARGANTUAR（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 33 ZOMBIE_ZOMBATAR（无 case）
};
static_assert(sizeof(kAlmanacIconOffset) / sizeof(kAlmanacIconOffset[0]) ==
    (int)ZombieType::NUM_ZOMBIE_TYPES, "槽位须与 ZombieType 枚举同步");
}  // namespace

namespace
{
// kAlmanacPortraitOffset：生成器解析原 switch 生成（逐槽对拍 0 差异）；
// 哨兵 0x7FFFFFFF, 0x7FFFFFFF = 无 case 槽（default 无操作）。
constexpr const int kAlmanacPortraitOffset[(int)ZombieType::NUM_ZOMBIE_TYPES][2] = {
		{0x7FFFFFFF, 0x7FFFFFFF},  // 0 ZOMBIE_NORMAL（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 1 ZOMBIE_FLAG（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 2 ZOMBIE_TRAFFIC_CONE（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 3 ZOMBIE_POLEVAULTER（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 4 ZOMBIE_PAIL（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 5 ZOMBIE_NEWSPAPER（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 6 ZOMBIE_DOOR（无 case）
		{-10, 0},  // 7 ZOMBIE_FOOTBALL
		{0x7FFFFFFF, 0x7FFFFFFF},  // 8 ZOMBIE_DANCER（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 9 ZOMBIE_BACKUP_DANCER（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 10 ZOMBIE_DUCKY_TUBE（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 11 ZOMBIE_SNORKEL（无 case）
		{-30, 5},  // 12 ZOMBIE_ZAMBONI
		{0x7FFFFFFF, 0x7FFFFFFF},  // 13 ZOMBIE_BOBSLED（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 14 ZOMBIE_DOLPHIN_RIDER（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 15 ZOMBIE_JACK_IN_THE_BOX（无 case）
		{0, -20},  // 16 ZOMBIE_BALLOON
		{0x7FFFFFFF, 0x7FFFFFFF},  // 17 ZOMBIE_DIGGER（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 18 ZOMBIE_POGO（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 19 ZOMBIE_YETI（无 case）
		{15, 0},  // 20 ZOMBIE_BUNGEE
		{0x7FFFFFFF, 0x7FFFFFFF},  // 21 ZOMBIE_LADDER（无 case）
		{-10, 0},  // 22 ZOMBIE_CATAPULT
		{0, 40},  // 23 ZOMBIE_GARGANTUAR
		{0x7FFFFFFF, 0x7FFFFFFF},  // 24 ZOMBIE_IMP（无 case）
		{-540, -175},  // 25 ZOMBIE_BOSS
		{0x7FFFFFFF, 0x7FFFFFFF},  // 26 ZOMBIE_PEA_HEAD（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 27 ZOMBIE_WALLNUT_HEAD（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 28 ZOMBIE_JALAPENO_HEAD（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 29 ZOMBIE_GATLING_HEAD（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 30 ZOMBIE_SQUASH_HEAD（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 31 ZOMBIE_TALLNUT_HEAD（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 32 ZOMBIE_REDEYE_GARGANTUAR（无 case）
		{0x7FFFFFFF, 0x7FFFFFFF},  // 33 ZOMBIE_ZOMBATAR（无 case）
};
static_assert(sizeof(kAlmanacPortraitOffset) / sizeof(kAlmanacPortraitOffset[0]) ==
    (int)ZombieType::NUM_ZOMBIE_TYPES, "槽位须与 ZombieType 枚举同步");
}  // namespace

//0x402C00
// GOTY @Patoke: 0x403DE0
void AlmanacDialog::DrawZombies(Graphics* g)
{
	const auto visualNowNs = mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs;
	const TextureSource aTexture{AlmanacAtlas::Texture, 0};
	DrawAlmanacBackground(g,aTexture,AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_ZOMBIEBACK)],
		mApp->mScreenBounds,mX);
	Graphics aContentGraphics(*g);
	aContentGraphics.ClipRect(0, 0, BOARD_WIDTH, BOARD_HEIGHT);
	g = &aContentGraphics;
	TodDrawString(g, gStr.SUBURBAN_ALMANAC_ZOMBIES, BOARD_WIDTH / 2, 54, Sexy::FONT_DWARVENTODCRAFT24, Color(0, 196, 0), DS_ALIGN_CENTER);

	ZombieType aZombieMouseOn = ZombieHitTest(mApp->mWidgetManager->mLastMouseX - mX, mApp->mWidgetManager->mLastMouseY - mY);
	for (int i = 0; i < NUM_ALMANAC_ZOMBIES; i++)
	{
		ZombieType aZombieType = GetZombieType(i);
		int aPosX, aPosY;
		GetZombiePosition(aZombieType, aPosX, aPosY);
		if (aZombieType != ZombieType::ZOMBIE_INVALID)
		{
			if (!ZombieIsShown(aZombieType))
				DrawAtlasImage(g,aTexture,AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_ZOMBIEBLANK)],aPosX,aPosY);
			else
			{
				DrawAtlasImage(g,aTexture,AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_ZOMBIEWINDOW)],aPosX,aPosY);
				if (aZombieType == aZombieMouseOn)
				{
					g->mDrawMode = Graphics::DRAWMODE_ADDITIVE;
					g->mColor = Color(255, 255, 255, 48);
					g->mColorizeImages = true;
					DrawAtlasImage(g,aTexture,AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_ZOMBIEWINDOW)],aPosX,aPosY);
					g->mDrawMode = Graphics::DRAWMODE_NORMAL;
					g->mColorizeImages = false;
				}

				ZombieType aZombieTypeToDraw = aZombieType;
				Graphics aZombieGraphics = Graphics(*g);
				aZombieGraphics.SetClipRect(aPosX + 2, aPosY + 2, 72, 72);
				aZombieGraphics.Translate(aPosX + 1, aPosY - 6);
				aZombieGraphics.mScaleX = 0.5f;
				aZombieGraphics.mScaleY = 0.5f;
				// GOTY 1073 icon offsets; Backup Dancer is -5, not Portable's -4.
				// B 形态 -> 常量表（见上 kAlmanacIconOffset）；POLEVAULTER
				// 特例（平移+换缓存型）表外。
				if (aZombieType == ZombieType::ZOMBIE_POLEVAULTER)
				{
					aZombieGraphics.TranslateF(2, -3);
					aZombieTypeToDraw = ZombieType::ZOMBIE_CACHED_POLEVAULTER_WITH_POLE;
				}
				else if (static_cast<unsigned>(aZombieType) < static_cast<unsigned>(ZombieType::NUM_ZOMBIE_TYPES) && kAlmanacIconOffset[aZombieType][0] != 0x7FFFFFFF)
				{
					aZombieGraphics.TranslateF(kAlmanacIconOffset[aZombieType][0], kAlmanacIconOffset[aZombieType][1]);
				}

				if (ZombieHasSilhouette(aZombieType))
				{
					aZombieGraphics.mColor = Color(0, 0, 0, 40);
					aZombieGraphics.mColorizeImages = true;
				}
				mApp->mReanimatorCache->DrawCachedZombie(&aZombieGraphics, 0, 0, aZombieTypeToDraw);
				aZombieGraphics.mColorizeImages = false;

				DrawAtlasImage(g,aTexture,AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_ZOMBIEWINDOW2)],aPosX,aPosY);
				if (aZombieType == aZombieMouseOn)
				{
					g->mDrawMode = Graphics::DRAWMODE_ADDITIVE;
					g->mColor = Color(255, 255, 255, 48);
					g->mColorizeImages = true;
					DrawAtlasImage(g,aTexture,AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_ZOMBIEWINDOW2)],aPosX,aPosY);
					g->mDrawMode = Graphics::DRAWMODE_NORMAL;
					g->mColorizeImages = false;
				}
			}
		}
	}

	const auto aGround=mZombie->mZombieType == ZombieType::ZOMBIE_ZAMBONI || mZombie->mZombieType == ZombieType::ZOMBIE_BOBSLED ?
		AlmanacSprite::ALMANAC_GROUNDICE : AlmanacSprite::ALMANAC_GROUNDDAY;
	DrawAtlasImage(g,aTexture,AlmanacAtlas::Images[static_cast<unsigned>(aGround)],518,110);
	if (mZombie && !ZombieHasSilhouette(mZombie->mZombieType))
	{
		Graphics aZombieGraphics = Graphics(*g);
		mZombie->BeginDraw(&aZombieGraphics);
		aZombieGraphics.SetClipRect(-42, -51, 197, 187);
		// @Patoke todo: add new functionality
		// B 形态 -> 常量表（见上 kAlmanacPortraitOffset），越界/无 case 槽
		// = 哨兵行，不平移。
		if (static_cast<unsigned>(mZombie->mZombieType) < static_cast<unsigned>(ZombieType::NUM_ZOMBIE_TYPES) && kAlmanacPortraitOffset[mZombie->mZombieType][0] != 0x7FFFFFFF)
		{
			aZombieGraphics.TranslateF(kAlmanacPortraitOffset[mZombie->mZombieType][0], kAlmanacPortraitOffset[mZombie->mZombieType][1]);
		}

		if (mZombie->mZombieType != ZombieType::ZOMBIE_BUNGEE && mZombie->mZombieType != ZombieType::ZOMBIE_BOSS &&
			mZombie->mZombieType != ZombieType::ZOMBIE_ZAMBONI && mZombie->mZombieType != ZombieType::ZOMBIE_CATAPULT)
			mZombie->DrawShadow(&aZombieGraphics);
		mZombie->Draw(visualNowNs, &aZombieGraphics);
	}
	DrawAtlasImage(g,aTexture,AlmanacAtlas::Images[static_cast<unsigned>(AlmanacSprite::ALMANAC_ZOMBIECARD)],455,78);

	SexyString aName = ZombieHasSilhouette(mSelectedZombie) ? ("???") : EntityZombieName(mSelectedZombie);
	TodDrawString(g, aName, 613, 362, Sexy::FONT_DWARVENTODCRAFT18GREENINSET, Color(190, 255, 235, 255), DS_ALIGN_CENTER);

	SexyString aDescription;
	DrawStringJustification aAlign;
	if (ZombieHasDescription(mSelectedZombie))
	{
		aDescription = EntityZombieDescription(mSelectedZombie);
		aAlign = DS_ALIGN_LEFT;
	}
	else
	{
		aDescription = gStr.NOT_ENCOUNTERED_YET;
		aAlign = DS_ALIGN_CENTER_VERTICAL_MIDDLE;
	}
	for (TodStringListFormat& aFormat : gLawnStringFormats)
	{
		if (TestBit(aFormat.mFormatFlags, TodStringFormatFlag::TOD_FORMAT_HIDE_UNTIL_MAGNETSHROOM))
		{
			if (mApp->HasSeedType(SeedType::SEED_MAGNETSHROOM))
			{
				aFormat.mNewColor.mAlpha = 255;
				aFormat.mLineSpacingOffset = 0;
			}
			else
			{
				aFormat.mNewColor.mAlpha = 0;
				aFormat.mLineSpacingOffset = -17;
			}
		}
	}
	// todo @Patoke: fix stuff that have another formatter after them, ex: "{KEYWORD}Weakness:{STAT} fume-shroom{METAL} and magnet-shroom{KEYWORD}" (magnet-shroom will show with the {KEYWORD} colors)
	// @Patoke: added extra check for the zamboni zombie
	Rect aDescRect(484, mSelectedZombie == ZombieType::ZOMBIE_ZAMBONI ? 372 : 377, 258, 170);
	int aTextHeight = TodDrawStringWrappedHelper(g, aDescription, aDescRect, Sexy::FONT_BRIANNETOD12, Color(40, 50, 90), aAlign, false);
	mZombieDescScrollMax = std::max(0, aTextHeight - aDescRect.mHeight);
	mZombieDescScroll = std::min(std::max(0, mZombieDescScroll), mZombieDescScrollMax);
	{
		GraphicsAutoState aState(g);
		g->SetClipRect(aDescRect);
		Rect aScrollRect = aDescRect;
		aScrollRect.mY -= mZombieDescScroll;
		TodDrawStringWrapped(g, aDescription, aScrollRect, Sexy::FONT_BRIANNETOD12, Color(40, 50, 90), aAlign);
	}
	DrawAlmanacScrollbar(g, aDescRect, mZombieDescScroll, mZombieDescScrollMax);
}

//0x403810
void AlmanacDialog::Draw(Graphics* g)
{
	const int aPreviousX = mX;
	RefreshAlmanacLayout(*this);
	Graphics aPageGraphics(*g);
	aPageGraphics.Translate(mX - aPreviousX, 0);
	g = &aPageGraphics;
	g->mLinearBlend = true;
	switch (mOpenPage)
	{
	case AlmanacPage::ALMANAC_PAGE_INDEX:	DrawIndex(g);	break;
	case AlmanacPage::ALMANAC_PAGE_PLANTS:	DrawPlants(g);	break;
	case AlmanacPage::ALMANAC_PAGE_ZOMBIES:	DrawZombies(g);	break;
	}

	const TextureSource aTexture{AlmanacAtlas::Texture, 0};
	mCloseButton->DrawAtlas(g,aTexture);
	mIndexButton->DrawAtlas(g,aTexture);
	const Sexy::TextureSource fixedUI{FixedUIAtlas::Texture, 0};
	mPlantButton->DrawAtlas(g, fixedUI,
		&FixedUIAtlas::Images[size_t(FixedUIAtlas::Sprite::SEEDCHOOSER_BUTTON_GLOW)]);
	mZombieButton->Draw(g);
}

void AlmanacDialog::GetSeedPosition(SeedType theSeedType, int& x, int& y)
{
	if (theSeedType == SeedType::SEED_IMITATER)
		x = 20, y = 23;
	else
	{
		x = theSeedType % 8 * 52 + 26;
		y = theSeedType / 8 * 78 + 92;
	}
}

//0x403940
SeedType AlmanacDialog::SeedHitTest(int x, int y)
{
	if (mMouseVisible && mOpenPage == AlmanacPage::ALMANAC_PAGE_PLANTS)
	{
		for (SeedType aSeedType = SeedType::SEED_PEASHOOTER; aSeedType < NUM_ALMANAC_SEEDS; aSeedType = (SeedType)(aSeedType + 1))
		{
			if (mApp->SeedTypeAvailable(aSeedType))
			{
				int aSeedX, aSeedY;
				GetSeedPosition(aSeedType, aSeedX, aSeedY);
				Rect aSeedRect = aSeedType == SeedType::SEED_IMITATER ? Rect(aSeedX, aSeedY, 34, 46) : Rect(aSeedX, aSeedY, SEED_PACKET_WIDTH, SEED_PACKET_HEIGHT);
				if (aSeedRect.Contains(x, y)) return aSeedType;
			}
		}
	}
	return SeedType::SEED_NONE;
}

bool AlmanacDialog::ZombieHasSilhouette(ZombieType theZombieType)
{
	// ��ѩ�˽�ʬ�����������ʬ������ѩ�˽�ʬ�Ѿ�����ˢ�����Ѿ���������ð��ģʽ����Ŀ 4-10 �ؿ������򲻻���ʾΪ��Ӱ
	if (theZombieType != ZombieType::ZOMBIE_YETI || mApp->CanSpawnYetis())
		return false;

	// �ų�����������������ѩ�˽�ʬ���ֵĹؿ���ð��ģʽһ��Ŀ 4-10 �ؿ�������ѩ�˽�ʬ��ʾΪ��Ӱ
	return mApp->HasFinishedAdventure() || mApp->mPlayerInfo->mLevel > GetZombieDefinition(ZombieType::ZOMBIE_YETI).mStartingLevel;
}

//0x403A10
// GOTY @Patoke: 0x404C50
bool AlmanacDialog::ZombieIsShown(ZombieType theZombieType)
{
	// ����ģʽ�£���չʾǱˮ��ʬ����֮ǰ���ֵĽ�ʬ
	if (mApp->IsTrialStageLocked() && theZombieType > ZombieType::ZOMBIE_SNORKEL)
		return false;

	// ����ѩ�˽�ʬ��Ҫ���������ˢ���г��֣��Ѿ���������ð��ģʽ����Ŀ 4-10 �ؿ�����
	// ���ѵ�֪����ڵ�δ�����������Ѿ����ð��ģʽһ��Ŀ 4-10 �ؿ�����δ�������Ŀ 4-10 �ؿ���
	if (theZombieType == ZombieType::ZOMBIE_YETI)
		return mApp->CanSpawnYetis() || ZombieHasSilhouette(ZombieType::ZOMBIE_YETI);

	// ����ð��ģʽ�г��ֵĽ�ʬ
	if (theZombieType <= ZombieType::ZOMBIE_BOSS)
	{
		// ð��ģʽһ��Ŀ��ɺ�ͼ��չʾ���н�ʬ
		if (mApp->HasFinishedAdventure())
			return true;

		int aLevel = mApp->mPlayerInfo->mLevel;
		int aStart = GetZombieDefinition(theZombieType).mStartingLevel;
		// Ҫ���Ѿ��ﵽ��ʬ�״γ��ֵĹؿ�
		// ���ڲ���ͨ����Ȼˢ�ֳ��ֵĽ�ʬ��С����ʬ��ѩ����ʬС�ӡ����轩ʬ��������Ҫ����ͨ�����״γ��ֵĹؿ����ѻ��ܹ��ý�ʬ
		return aStart <= aLevel && (aStart != aLevel || !Board::IsZombieTypeSpawnedOnly(theZombieType) || gZombieDefeated[theZombieType]);
	}

	return false;
}

//0x403B30
// GOTY @Patoke: 0x404D50
bool AlmanacDialog::ZombieHasDescription(ZombieType theZombieType)
{
	int aLevel = mApp->mPlayerInfo->mLevel;
	int aStart = GetZombieDefinition(theZombieType).mStartingLevel;

	// ����ѩ�˽�ʬ
	if (theZombieType == ZombieType::ZOMBIE_YETI)
	{
		// ��ѩ�˽�ʬ������ˢ���г���ʱ��ð��ģʽ����Ŀ 4-10 �ؿ�֮ǰ��������ʾ��ʬ����
		if (!mApp->CanSpawnYetis())
			return false;
		// �ӵ�����Ŀ��ʼ��������ʾѩ�˽�ʬ������
		if (mApp->mPlayerInfo->mFinishedAdventure >= 2)
			return true;
	}
	// ����ѩ�˽�ʬ���������ʬ����ð��ģʽ�����ʱ��������ʾ��ʬ������
	else if (mApp->HasFinishedAdventure())
		return true;

	// ѩ�˽�ʬ�ڶ���Ŀ 4-10 �ؿ�������Ŀ֮�䣬��������ʬ��ð��ģʽһ��Ŀ�е������
	// Ҫ���Ѿ��ﵽ��ʬ�״γ��ֵĹؿ�������ͨ�����״γ��ֵĹؿ����ѻ��ܹ��ý�ʬ
	return aStart <= aLevel && (aStart != aLevel || gZombieDefeated[theZombieType]);
}

void AlmanacDialog::GetZombiePosition(ZombieType theZombieType, int& x, int& y)
{
	if (theZombieType == ZombieType::ZOMBIE_BOSS)
		x = 192, y = 486;
	else
	{
		x = theZombieType % 5 * 85 + 22;
		y = theZombieType / 5 * 80 + 86;
	}
}

//0x403BB0
// GOTY @Patoke: 0x404DD0
ZombieType AlmanacDialog::ZombieHitTest(int x, int y)
{
	if (mMouseVisible && mOpenPage == AlmanacPage::ALMANAC_PAGE_ZOMBIES)
	{
		for (int i = 0; i < NUM_ALMANAC_ZOMBIES; i++)
		{
			ZombieType aZombieType = GetZombieType(i);
			// @Patoke: added IsShown check
			if (aZombieType != ZombieType::ZOMBIE_INVALID && ZombieIsShown(aZombieType))
			{
				int aZombieX, aZombieY;
				GetZombiePosition(aZombieType, aZombieX, aZombieY);
				if (Rect(aZombieX, aZombieY, 76, 76).Contains(x, y))
					return aZombieType;
			}
		}
	}
	return ZombieType::ZOMBIE_INVALID;
}

//0x403C60
void AlmanacDialog::MouseUp(int x, int y, int theClickCount)
{
	const int aPreviousX = mX;
	RefreshAlmanacLayout(*this);
	x += aPreviousX - mX;
	(void)theClickCount;
	mDescScrollBarDragging = false;
	if (mPlantButton->HitTest(x, y, this))		SetPage(ALMANAC_PAGE_PLANTS);
	else if (mZombieButton->HitTest(x, y, this))	SetPage(ALMANAC_PAGE_ZOMBIES);
	else if (mCloseButton->HitTest(x, y, this))	mApp->KillAlmanacDialog();
	else if (mIndexButton->HitTest(x, y, this))	SetPage(ALMANAC_PAGE_INDEX);
}

//0x403D00
// GOTY @Patoke: 0x404F10
void AlmanacDialog::MouseDown(int x, int y, int theClickCount)
{
	const int aPreviousX = mX;
	RefreshAlmanacLayout(*this);
	x += aPreviousX - mX;
	(void)theClickCount;

	Rect aDescRect = GetDescRect();
	Rect aTrackRect(aDescRect.mX + aDescRect.mWidth + 2, aDescRect.mY, 8, aDescRect.mHeight);
	if (aTrackRect.Contains(x, y) && DescScrollMax() > 0)
	{
		UpdateAlmanacScroll(y, aDescRect, DescScrollRef(), DescScrollMax());
		mDescScrollBarDragging = true;
		MarkDirty();
		return;
	}

	if (mPlantButton->HitTest(x, y, this) || mCloseButton->HitTest(x, y, this) || mIndexButton->HitTest(x, y, this))
		mApp->PlaySample(Sexy::SOUND_TAP);
	if (mZombieButton->HitTest(x, y, this))
		mApp->PlaySample(Sexy::SOUND_GRAVEBUTTON);

	SeedType aSeedType = SeedHitTest(x, y);
	if (aSeedType != SeedType::SEED_NONE && aSeedType != mSelectedSeed)
	{
		mSelectedSeed = aSeedType;
		SetupPlant();
		mApp->PlaySample(Sexy::SOUND_TAP);
	}
	ZombieType aZombieType = ZombieHitTest(x, y);
	if (aZombieType != ZombieType::ZOMBIE_INVALID && aZombieType != mSelectedZombie)
	{
		mSelectedZombie = aZombieType;
		SetupZombie();
		mApp->PlaySample(Sexy::SOUND_TAP);
	}
}

// Long plant/zombie descriptions are clipped to the description panel;
// the mouse wheel scrolls them.
void AlmanacDialog::MouseWheel(int theDelta)
{
	if (mOpenPage == ALMANAC_PAGE_PLANTS)
	{
		int aStep = theDelta > 0 ? -18 : 18;
		mPlantDescScroll = std::min(std::max(0, mPlantDescScroll + aStep), mPlantDescScrollMax);
		MarkDirty();
	}
	else if (mOpenPage == ALMANAC_PAGE_ZOMBIES)
	{
		int aStep = theDelta > 0 ? -18 : 18;
		mZombieDescScroll = std::min(std::max(0, mZombieDescScroll + aStep), mZombieDescScrollMax);
		MarkDirty();
	}
	else
		LawnDialog::MouseWheel(theDelta);
}

void AlmanacDialog::MouseDrag(int x, int y)
{
	if (!mDescScrollBarDragging)
	{
		LawnDialog::MouseDrag(x, y);
		return;
	}

	Rect aDescRect = GetDescRect();
	UpdateAlmanacScroll(y, aDescRect, DescScrollRef(), DescScrollMax());
	MarkDirty();
}

int& AlmanacDialog::DescScrollRef()
{
	return mOpenPage == ALMANAC_PAGE_PLANTS ? mPlantDescScroll : mZombieDescScroll;
}


void AlmanacInitForPlayer()
{
	for (int i = 0; i < ZombieType::NUM_ZOMBIE_TYPES; i++)
		gZombieDefeated[i] = false;
}

void AlmanacPlayerDefeatedZombie(ZombieType theZombieType)
{
	gZombieDefeated[(int)theZombieType] = true;
}

const void* AlmanacDialog::TouchHitTest(int x, int y)
{
    if (mPlantButton->HitTest(x, y, this)) return mPlantButton;
    if (mZombieButton->HitTest(x, y, this)) return mZombieButton;
    if (mCloseButton->HitTest(x, y, this)) return mCloseButton;
    if (mIndexButton->HitTest(x, y, this)) return mIndexButton;
    return this;
}
