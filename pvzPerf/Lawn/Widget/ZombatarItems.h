#pragma once
#include "misc/TimeCounter.h"
#include "ConstEnums.h"
#include "Sexy.TodLib/ReanimTrackId.h"
#include "Lawn/System/PlayerInfo.h"
#include "ZombatarResources.h"
#include "ZombatarColors.h"
#include "widget/Widget.h"

#include <array>
#include <cstddef>

class LawnApp;
class Reanimation;

using Sexy::Color;
using Sexy::Graphics;
using Sexy::Rect;
using Sexy::Widget;

class ZombatarButton final
{
public:
	LawnApp* mApp;
	Widget* mParentWidget = nullptr;
	int mX = 0;
	int mY = 0;
	int mWidth = 0;
	int mHeight = 0;
	int mId;
	bool mIsOver = false;
	bool mIsDown = false;
	bool mDisabled = false;
	bool mBtnNoDraw = false;
	ZombatarImage mNormalImage;
	ZombatarImage mHoverImage;
	ZombatarImage mPressedImage;
	ZombatarImage mInactiveImage;

	ZombatarButton(int theId, ZombatarImage theNormal, ZombatarImage theHover,
		ZombatarImage thePressed, ZombatarImage theInactive);
	void Draw(Graphics* g);
	void DrawNormal(Graphics* g);
	bool IsButtonDown() const;
	bool IsMouseOver() const;
	void Update();
};

class ZombatarItem
{
public:
	ZombatarImage mImage;
	ZombatarImage mColorMask;
	int mX;
	int mY;
	int mMaskX;
	int mMaskY;
	ZombatarColorMode mColorMode;
	int mAlignment;
	ReanimTrackId mTrack;      // 部件轨道句柄（None = 名字不在映射表，复刻原版回退语义）
	ReanimTrackId mLineTrack;  // 描线变体轨道（None = 无此轨道）
	int mRenderGroup;

	constexpr ZombatarItem(ZombatarImage theImage, ZombatarImage theColorMask, int theX, int theY,
		int theMaskX, int theMaskY, ZombatarColorMode theColorMode,
		int theAlignment, ReanimTrackId theTrack, ReanimTrackId theLineTrack, int theRenderGroup) :
		mImage(theImage),
		mColorMask(theColorMask),
		mX(theX),
		mY(theY),
		mMaskX(theMaskX),
		mMaskY(theMaskY),
		mColorMode(theColorMode),
		mAlignment(theAlignment),
		mTrack(theTrack),
		mLineTrack(theLineTrack),
		mRenderGroup(theRenderGroup)
	{
	}

};

const Color& ZombatarGetColor(int theColorIndex);
const ZombatarItem* ZombatarGetItem(ZombatarCustomizationPage thePage, int theSelectionIndex);
void ResetZombatarReanimation(Reanimation* theReanimation);
void ApplyZombatarReanimation(Reanimation* theReanimation, const Zombatar& theZombatar);
void DrawZombatarPortrait(Graphics* g, const Zombatar& theZombatar, int theX, int theY);

enum class ZombatarEditorState
{
	EDITING = 0,
	ENTERING_CONFIRMATION = 1,
	LEAVING_CONFIRMATION = 2,
	CONFIRMATION = 3,
	BROWSING = 4
};

struct ZombatarItemCell
{
	const ZombatarItem* mItem = nullptr;
	// The grid contains separator/"none" cells. GOTY stores the logical grid
	// index; ZombatarGetItem performs the 1073 separator-to-raw mapping.
	int mRawIndex = -1; // historical name; this is the logical cell index
	int mX = 0;
	int mY = 0;
};

class ZombatarItems : public Widget
{
public:
	enum ButtonId
	{
		BUTTON_NEXT_PAGE = 1000,
		BUTTON_PREV_PAGE = 1001,
		BUTTON_BACK = 1002,
		BUTTON_FINISHED = 1003,
		BUTTON_NEW_ZOMBIE = 1004,
		BUTTON_NEXT_ZOMBIE = 1005,
		BUTTON_PREV_ZOMBIE = 1006,
		BUTTON_MAIN_MENU = 1007,
		BUTTON_VIEW = 1008,
		BUTTON_SKIN = 1009,
		BUTTON_HAIR = 1010,
		BUTTON_FACIAL_HAIR = 1011,
		BUTTON_TIDBITS = 1012,
		BUTTON_EYEWEAR = 1013,
		BUTTON_CLOTHES = 1014,
		BUTTON_ACCESSORIES = 1015,
		BUTTON_HATS = 1016,
		BUTTON_BACKDROPS = 1017
	};

	static constexpr int MAX_ITEM_CELLS = 128;

	LawnApp* mApp;
	ZombatarButton* mPrevPageButton;
	ZombatarButton* mNextPageButton;
	ZombatarButton* mSkinButton;
	ZombatarButton* mHairButton;
	ZombatarButton* mFacialHairButton;
	ZombatarButton* mTidbitsButton;
	ZombatarButton* mEyeWearButton;
	ZombatarButton* mClothesButton;
	ZombatarButton* mAccessoryButton;
	ZombatarButton* mHatsButton;
	ZombatarButton* mBackdropsButton;
	ZombatarButton* mBackButton;
	ZombatarButton* mFinishedButton;
	ZombatarButton* mNewZombieButton;
	ZombatarButton* mNextZombieButton;
	ZombatarButton* mPrevZombieButton;
	ZombatarButton* mMainMenuButton;
	ZombatarButton* mViewButton;
	ZombatarColors* mColors;

	int mCurrentPage;
	int mPageCount;
	int mItemGap;
	int mItemCount;
	int mItemsPerPage;
	int mCellPadding;
	int mHoveredItem;
	int mPressedItem;
	ZombatarCustomizationPage mCurrentCategory;
	std::array<ZombatarItemCell, MAX_ITEM_CELLS> mItemCells;
	Zombatar mZombatar;
	ZombatarEditorState mState;
	Sexy::TimeCounter mTransitionCounter;
	int mSelectedZombatar;
	int mPortraitGap;
	bool mSelectionChanged;
	bool mSelectionChangedLastFrame;
	bool mDeleteHovered;
	Rect mDeleteRect;
	int mPressedButtonId;

	explicit ZombatarItems(LawnApp* theApp);
	~ZombatarItems() override;

	void Update() override;
	void Draw(Graphics* g) override;
    const void* TouchHitTest(int x, int y) override;
	void MouseDown(int x, int y, int theClickCount) override;
	void MouseDown(int x, int y, int theBtnNum, int theClickCount) override;
	void MouseMove(int x, int y) override;
	void MouseLeave() override;
	void MouseUp(int x, int y, int theLastDownButtonId) override;
	void MouseUp(int x, int y, int theBtnNum, int theClickCount) override;

    void ButtonDepress(int theId);
    void CancelDeleteDialog();
    static void FinishDeleteDialog(LawnApp& theApp);
	void EnterInitialState();
	void ResetZombatar();
	void SetCategory(ZombatarCustomizationPage thePage);
	void RefreshState();
	void SyncColorMode();
	int HitTestItem(int x, int y) const;
	bool HasVisualSelectionChanged() const;

private:
	void LayoutCategoryItems();
	void UpdateEditorButtons();
	void DrawEditor(Graphics* g);
	void DrawItemGrid(Graphics* g);
	void DrawCategoryHighlight(Graphics* g);
	void DrawConfirmation(Graphics* g);
	void DrawSavedZombatars(Graphics* g);
	void SaveCurrentZombatar();
	void ReturnToMainMenu();
	ZombatarButton* FindPressedButton() const;
	void ResetTransientLayout();
	void UpdateViewButtonLayout();
	ZombatarButton* HitTestButton(int x, int y) const;
};

