#include "Sexy.TodLib/generated/EntityStrings.h"
#include "ZombatarItems.h"
#include "GameButton.h"
#include "LawnApp.h"
#include "Resources.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/TodCommon.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "SexyAppFramework/graphics/Image.h"
#include "SexyAppFramework/graphics/DDInterface.h"
#include "SexyAppFramework/graphics/RenderInterface.h"
#include "SexyAppFramework/imagelib/ImageLib.h"
#include "GameSelector.h"
#include "ZombatarScreen.h"
#include "widget/Dialog.h"
#include "widget/WidgetManager.h"

#include "platform/UserDirectories.h"
#include <filesystem>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

ZombatarButton::ZombatarButton(int theId, ZombatarImage theNormal,
	ZombatarImage theHover, ZombatarImage thePressed, ZombatarImage theInactive)
	: mApp(static_cast<LawnApp*>(gSexyAppBase)), mId(theId),
	  mNormalImage(theNormal), mHoverImage(theHover),
	  mPressedImage(thePressed), mInactiveImage(theInactive)
{
	const ZombatarImage aSizeImage = theNormal != ZombatarImage::None ? theNormal :
		(theHover != ZombatarImage::None ? theHover : thePressed);
	if (aSizeImage != ZombatarImage::None)
	{
		const TextureRegion& aSize = gZombatarAtlas.mImages[static_cast<size_t>(aSizeImage)];
		mWidth = aSize.mWidth;
		mHeight = aSize.mHeight;
	}
}

void ZombatarButton::DrawNormal(Graphics* g)
{
	DrawZombatarImage(g, mNormalImage,
		mX, mY);
}

bool ZombatarButton::IsButtonDown() const
{
	return mIsDown && mIsOver && !mDisabled && !mBtnNoDraw;
}

bool ZombatarButton::IsMouseOver() const
{
    if (!mApp->mWidgetManager->mMouseHoverEnabled) return false;
	if (mDisabled || mBtnNoDraw)
		return false;
	WidgetManager* aManager = mApp->mWidgetManager;
	const bool aFocusMatches = aManager->mFocusWidget &&
		aManager->mFocusWidget == mParentWidget;
	if (!aFocusMatches && mApp->GetDialogCount() > 0)
		return false;
	int aMouseX = aManager->mLastMouseX;
	int aMouseY = aManager->mLastMouseY;
	if (mParentWidget)
	{
		const Point aPosition = mParentWidget->GetAbsPos();
		aMouseX -= aPosition.mX;
		aMouseY -= aPosition.mY;
	}
	return Rect(mX, mY, mWidth, mHeight).Contains(aMouseX, aMouseY);
}

void ZombatarButton::Update()
{
	WidgetManager* aManager = mApp->mWidgetManager;
	int aMouseX = aManager->mLastMouseX;
	int aMouseY = aManager->mLastMouseY;
	if (mParentWidget)
	{
		const Point aPosition = mParentWidget->GetAbsPos();
		aMouseX -= aPosition.mX;
		aMouseY -= aPosition.mY;
	}
	if ((aManager->mFocusWidget && aManager->mFocusWidget == mParentWidget) ||
		mApp->GetDialogCount() <= 0)
	{
		mIsOver = aManager->mMouseHoverEnabled && Rect(mX, mY, mWidth, mHeight).Contains(aMouseX, aMouseY);
		mIsDown = (aManager->mDownButtons & 5) != 0;
        if (aManager->mTouchCount)
            for (const auto& contact : aManager->mTouches)
                if (contact.widget == mParentWidget && contact.pressedTarget == this &&
                    Rect(mX, mY, mWidth, mHeight).Contains(contact.event.x - mParentWidget->GetAbsPos().mX,
                        contact.event.y - mParentWidget->GetAbsPos().mY))
                {mIsOver = mIsDown = true;break;}
	}
	else
		mIsOver = mIsDown = false;
}

void ZombatarButton::Draw(Graphics* g)
{
	if (mBtnNoDraw)
		return;
	const bool isDown = IsButtonDown();
	const bool isHighlighted = mIsOver || IsMouseOver();
	const float parentTransX = g->mTransX, parentTransY = g->mTransY;
	g->mTransX = parentTransX + mX;
	g->mTransY = parentTransY + mY;
	const int aX = 0;
	const int aY = 0;
	if (isDown)
	{
		if (mPressedImage != ZombatarImage::None)
			DrawZombatarImage(g, mPressedImage, aX, aY);
		else
			DrawZombatarImage(g, mHoverImage != ZombatarImage::None ? mHoverImage : mNormalImage, aX + 1, aY + 1);
	}
	else if (mDisabled && mInactiveImage != ZombatarImage::None)
		DrawZombatarImage(g, mInactiveImage, aX, aY);
	else
		DrawZombatarImage(g, isHighlighted && mHoverImage != ZombatarImage::None ? mHoverImage : mNormalImage, aX, aY);
	// Match the image-only GameButton's final text state.
	g->mColor = Color::Black;
	g->mFont = nullptr;
	g->mTransX = parentTransX;
	g->mTransY = parentTransY;
}

namespace
{
// 1073 creates a nonblocking confirmation (0052F210) and deletes through the
// application's button callback (00453630), not a nested mouse-event wait.
// All borrowed addresses below are identity tokens until CurrentItems proves
// that the corresponding objects still belong to the active selector/profile.
class ZombatarDeleteDialog final : public LawnDialog
{
    GameSelector* mOwner;
    ZombatarItems* mItems;
    PlayerInfo* mPlayer;
    unsigned int mPlayerId;
    unsigned int mPlayerUseSeq;
    int mIndex;
    std::vector<Zombatar> mRecords;
    bool mConsumed = false;

    ZombatarItems* CurrentItems() const
    {
        if (mApp->GetDialog(mId) != this || mApp->mGameSelector != mOwner ||
            !mOwner->mZombatarScreen || mOwner->mZombatarScreen->mZombatarItems != mItems)
            return nullptr;
        PlayerInfo* aPlayer = mApp->mPlayerInfo;
        if (aPlayer != mPlayer || !aPlayer || aPlayer->mId != mPlayerId ||
            aPlayer->mUseSeq != mPlayerUseSeq || mItems->mWidgetManager != mApp->mWidgetManager ||
            mItems->mState != ZombatarEditorState::BROWSING || mItems->mSelectedZombatar != mIndex ||
            aPlayer->mNumZombatars != static_cast<int>(mRecords.size()))
            return nullptr;
        const bool aSameRecords = std::equal(mRecords.begin(), mRecords.end(), aPlayer->mZombatars,
            [](const Zombatar& a, const Zombatar& b) {
                return std::equal(std::begin(a.mItems), std::end(a.mItems), std::begin(b.mItems)) &&
                    std::equal(std::begin(a.mColors), std::end(a.mColors), std::begin(b.mColors));
            });
        return aSameRecords ? mItems : nullptr;
    }
public:
    ZombatarDeleteDialog(LawnApp& app, GameSelector& owner, ZombatarItems& items, PlayerInfo& player)
        : LawnDialog(&app, DIALOG_DELETEZOMBATAR, false,
            gStr.ZOMBATAR_DELETE_HEADER, gStr.ZOMBATAR_DELETE_BODY, (""), BUTTONS_OK_CANCEL),
          mOwner(&owner), mItems(&items), mPlayer(&player), mPlayerId(player.mId),
          mPlayerUseSeq(player.mUseSeq), mIndex(items.mSelectedZombatar),
          mRecords(player.mZombatars, player.mZombatars + player.mNumZombatars)
    {
        mYesButton->mLabel = gStr.ZOMBATAR_DELETE_BUTTON;
    }
    bool IsFor(const ZombatarItems& items) const { return mItems == &items; }
    void ButtonDepress(int button) override
    {
        // An already removed dialog must not route its ID to a newer instance.
        if (mApp->GetDialog(mId) == this)
            LawnDialog::ButtonDepress(button);
    }
    void Confirm()
    {
        if (mConsumed || mResult != ID_YES)
            return;
        mConsumed = true;
        ZombatarItems* aItems = CurrentItems();
        if (!aItems)
            return;
        int anIndex = mIndex;
        mPlayer->DeleteZombatarFromIndex(&anIndex);
        mPlayer->SaveDetails();
        if (mPlayer->mNumZombatars == 0)
        {
            aItems->ResetZombatar();
            aItems->mState = ZombatarEditorState::EDITING;
        }
        else
        {
            // 1073 00531610 selects the preceding portrait after deletion.
            aItems->mSelectedZombatar = std::max(mIndex - 1, 0);
        }
        aItems->RefreshState();
    }
};

const Color gZombatarColors[] = {
	{ 134, 147, 122, 255 }, { 79, 135, 94, 255 }, { 127, 135, 94, 255 },
	{ 120, 130, 50, 255 }, { 156, 163, 105, 255 }, { 96, 151, 11, 255 },
	{ 147, 184, 77, 255 }, { 82, 143, 54, 255 }, { 121, 168, 99, 255 },
	{ 65, 156, 74, 255 }, { 107, 178, 114, 255 }, { 104, 121, 90, 255 },
	{ 151, 33, 33, 255 }, { 199, 53, 53, 255 }, { 220, 112, 47, 255 },
	{ 251, 251, 172, 255 }, { 240, 210, 87, 255 }, { 165, 126, 65, 255 },
	{ 106, 72, 32, 255 }, { 72, 35, 5, 255 }, { 50, 56, 61, 255 },
	{ 0, 0, 10, 255 }, { 197, 239, 239, 255 }, { 63, 109, 242, 255 },
	{ 13, 202, 151, 255 }, { 158, 183, 19, 255 }, { 30, 210, 64, 255 },
	{ 225, 65, 230, 255 }, { 128, 47, 204, 255 }, { 255, 255, 255, 255 },
	{ 238, 19, 24, 255 }, { 247, 89, 215, 255 }, { 239, 198, 253, 255 },
	{ 160, 56, 241, 255 }, { 86, 74, 241, 255 }, { 74, 160, 241, 255 },
	{ 199, 244, 251, 255 }, { 49, 238, 237, 255 }, { 16, 194, 66, 255 },
	{ 112, 192, 33, 255 }, { 16, 145, 52, 255 }, { 248, 247, 41, 255 },
	{ 227, 180, 20, 255 }, { 241, 115, 25, 255 }, { 248, 247, 175, 255 },
	{ 103, 85, 54, 255 }, { 159, 17, 20, 255 }, { 255, 255, 255, 255 }
};

const ZombatarItem gHairItems[] = {
	{ ZombatarImage::HAIR_1, ZombatarImage::HAIR_1_MASK, 23, 0, 8, 1, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::hair_00, ReanimTrackId::hair_00_line, 8 },
	{ ZombatarImage::HAIR_2, ZombatarImage::HAIR_2_MASK, 23, 25, 2, 3, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::hair_01, ReanimTrackId::hair_01_line, 8 },
	{ ZombatarImage::HAIR_3, ZombatarImage::None, 23, 30, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::None, ReanimTrackId::hair_02_line, 8 },
	{ ZombatarImage::HAIR_4, ZombatarImage::None, 30, 15, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::hair_03, ReanimTrackId::hair_03_line, 8 },
	{ ZombatarImage::HAIR_5, ZombatarImage::None, 36, 37, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::hair_04, ReanimTrackId::hair_04_line, 8 },
	{ ZombatarImage::HAIR_6, ZombatarImage::None, 39, 13, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::hair_05, ReanimTrackId::hair_05_line, 8 },
	{ ZombatarImage::HAIR_7, ZombatarImage::None, 51, 22, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::hair_06, ReanimTrackId::hair_06_line, 8 },
	{ ZombatarImage::HAIR_8, ZombatarImage::None, 28, 15, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::hair_07, ReanimTrackId::hair_07_line, 8 },
	{ ZombatarImage::HAIR_9, ZombatarImage::None, 128, 55, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::hair_08, ReanimTrackId::hair_08_line, 8 },
	{ ZombatarImage::HAIR_10, ZombatarImage::None, 22, 32, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::hair_09, ReanimTrackId::hair_09_line, 8 },
	{ ZombatarImage::HAIR_11, ZombatarImage::HAIR_11_MASK, 25, 19, 2, 2, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::hair_10, ReanimTrackId::hair_10_line, 8 },
	{ ZombatarImage::HAIR_12, ZombatarImage::HAIR_12_MASK, 51, -5, 2, 2, ZOMBATAR_COLOR_HAIR, 2, ReanimTrackId::hair_11, ReanimTrackId::hair_11_line, 8 },
	{ ZombatarImage::HAIR_13, ZombatarImage::HAIR_13_MASK, 33, 13, 2, 2, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::hair_12, ReanimTrackId::hair_12_line, 8 },
	{ ZombatarImage::HAIR_14, ZombatarImage::HAIR_14_MASK, 9, -2, 1, 5, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::hair_13, ReanimTrackId::hair_13_line, 8 },
	{ ZombatarImage::HAIR_15, ZombatarImage::HAIR_15_MASK, 45, 4, 0, -1, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::hair_14, ReanimTrackId::hair_14_line, 8 },
	{ ZombatarImage::HAIR_16, ZombatarImage::None, 26, 20, 0, 0, ZOMBATAR_COLOR_HAIR, 2, ReanimTrackId::hair_15, ReanimTrackId::hair_15_line, 8 }
};

const ZombatarItem gFacialHairItems[] = {
	{ ZombatarImage::FACIALHAIR_1, ZombatarImage::FACIALHAIR_1_MASK, 35, 107, 1, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_00, ReanimTrackId::facialHair_00_line, 6 },
	{ ZombatarImage::FACIALHAIR_2, ZombatarImage::None, 51, 110, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_01, ReanimTrackId::facialHair_01_line, 6 },
	{ ZombatarImage::FACIALHAIR_3, ZombatarImage::None, 45, 110, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_02, ReanimTrackId::facialHair_02_line, 6 },
	{ ZombatarImage::FACIALHAIR_4, ZombatarImage::FACIALHAIR_4_MASK, 38, 105, 3, 2, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_03, ReanimTrackId::facialHair_03_line, 6 },
	{ ZombatarImage::FACIALHAIR_5, ZombatarImage::None, 69, 145, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_04, ReanimTrackId::facialHair_04_line, 6 },
	{ ZombatarImage::FACIALHAIR_6, ZombatarImage::None, 48, 112, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_05, ReanimTrackId::facialHair_05_line, 6 },
	{ ZombatarImage::FACIALHAIR_7, ZombatarImage::None, 13, 107, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_06, ReanimTrackId::facialHair_06_line, 6 },
	{ ZombatarImage::FACIALHAIR_8, ZombatarImage::FACIALHAIR_8_MASK, 45, 105, 1, 1, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_07, ReanimTrackId::facialHair_07_line, 6 },
	{ ZombatarImage::FACIALHAIR_9, ZombatarImage::FACIALHAIR_9_MASK, 41, 105, 1, 1, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_08, ReanimTrackId::facialHair_08_line, 6 },
	{ ZombatarImage::FACIALHAIR_10, ZombatarImage::FACIALHAIR_10_MASK, 44, 112, 1, 2, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_09, ReanimTrackId::facialHair_09_line, 6 },
	{ ZombatarImage::FACIALHAIR_11, ZombatarImage::FACIALHAIR_11_MASK, 43, 88, 1, 4, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_10, ReanimTrackId::facialHair_10_line, 6 },
	{ ZombatarImage::FACIALHAIR_12, ZombatarImage::FACIALHAIR_12_MASK, 28, 105, 8, 1, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_11, ReanimTrackId::facialHair_11_line, 6 },
	{ ZombatarImage::FACIALHAIR_13, ZombatarImage::None, 45, 110, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_12, ReanimTrackId::facialHair_12_line, 6 },
	{ ZombatarImage::FACIALHAIR_14, ZombatarImage::FACIALHAIR_14_MASK, 18, 103, 1, 1, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_13, ReanimTrackId::facialHair_13_line, 6 },
	{ ZombatarImage::FACIALHAIR_15, ZombatarImage::FACIALHAIR_15_MASK, 63, 145, 2, 1, ZOMBATAR_COLOR_HAIR, 4, ReanimTrackId::facialHair_14, ReanimTrackId::facialHair_14_line, 6 },
	{ ZombatarImage::FACIALHAIR_16, ZombatarImage::FACIALHAIR_16_MASK, 63, 140, 1, 1, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_15, ReanimTrackId::facialHair_15_line, 6 },
	{ ZombatarImage::FACIALHAIR_17, ZombatarImage::None, 43, 110, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_16, ReanimTrackId::facialHair_16_line, 6 },
	{ ZombatarImage::FACIALHAIR_18, ZombatarImage::FACIALHAIR_18_MASK, 58, 96, 1, 3, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_17, ReanimTrackId::facialHair_17_line, 6 },
	{ ZombatarImage::FACIALHAIR_19, ZombatarImage::None, 46, 92, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_18, ReanimTrackId::facialHair_18_line, 6 },
	{ ZombatarImage::FACIALHAIR_20, ZombatarImage::None, 114, 80, 0, 0, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_19, ReanimTrackId::facialHair_19_line, 6 },
	{ ZombatarImage::FACIALHAIR_21, ZombatarImage::FACIALHAIR_21_MASK, 118, 83, 1, 1, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_20, ReanimTrackId::facialHair_20_line, 6 },
	{ ZombatarImage::FACIALHAIR_22, ZombatarImage::FACIALHAIR_22_MASK, 13, 87, 3, 4, ZOMBATAR_COLOR_HAIR, 4, ReanimTrackId::facialHair_21, ReanimTrackId::facialHair_21_line, 6 },
	{ ZombatarImage::FACIALHAIR_23, ZombatarImage::FACIALHAIR_23_MASK, 58, 145, 1, 1, ZOMBATAR_COLOR_HAIR, 32, ReanimTrackId::facialHair_22, ReanimTrackId::facialHair_22_line, 6 },
	{ ZombatarImage::FACIALHAIR_24, ZombatarImage::FACIALHAIR_24_MASK, 38, 108, 4, 2, ZOMBATAR_COLOR_HAIR, 4, ReanimTrackId::facialHair_23, ReanimTrackId::facialHair_23_line, 6 }
};

const ZombatarItem gTidbitsItems[] = {
	{ ZombatarImage::TIDBITS_1, ZombatarImage::TIDBITS_3, 28, 63, 18, 48, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::tidBits_00, ReanimTrackId::tidBits_00_line, 2 },
	{ ZombatarImage::TIDBITS_2, ZombatarImage::None, 28, 63, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::tidBits_01, ReanimTrackId::tidBits_01_line, 2 },
	{ ZombatarImage::TIDBITS_3, ZombatarImage::None, 46, 111, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::tidBits_02, ReanimTrackId::None, 2 },
	{ ZombatarImage::TIDBITS_4, ZombatarImage::None, 31, 62, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::tidBits_03, ReanimTrackId::None, 2 },
	{ ZombatarImage::TIDBITS_5, ZombatarImage::None, 31, 58, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::tidBits_04, ReanimTrackId::None, 2 },
	{ ZombatarImage::TIDBITS_6, ZombatarImage::None, 28, 66, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::tidBits_05, ReanimTrackId::None, 2 },
	{ ZombatarImage::TIDBITS_7, ZombatarImage::None, 28, 72, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::tidBits_06, ReanimTrackId::None, 2 },
	{ ZombatarImage::TIDBITS_8, ZombatarImage::None, 33, 55, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::tidBits_07, ReanimTrackId::None, 2 },
	{ ZombatarImage::TIDBITS_9, ZombatarImage::None, 21, 76, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::tidBits_08, ReanimTrackId::None, 2 },
	{ ZombatarImage::TIDBITS_10, ZombatarImage::None, 36, 71, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::tidBits_09, ReanimTrackId::tidBits_09_line, 2 },
	{ ZombatarImage::TIDBITS_11, ZombatarImage::None, 36, 70, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::tidBits_10, ReanimTrackId::tidBits_10_line, 2 },
	{ ZombatarImage::TIDBITS_12, ZombatarImage::None, 86, 91, 0, 6, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::tidBits_11, ReanimTrackId::tidBits_11_line, 2 },
	{ ZombatarImage::TIDBITS_13, ZombatarImage::None, 88, 50, 0, 8, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::tidBits_12, ReanimTrackId::None, 2 },
	{ ZombatarImage::TIDBITS_14, ZombatarImage::None, 113, 115, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::tidBits_13, ReanimTrackId::None, 2 }
};

const ZombatarItem gEyewearItems[] = {
	{ ZombatarImage::EYEWEAR_1, ZombatarImage::EYEWEAR_1_MASK, 28, 73, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::eyeWear_00, ReanimTrackId::eyeWear_00_line, 10 },
	{ ZombatarImage::EYEWEAR_2, ZombatarImage::EYEWEAR_2_MASK, 31, 85, 0, -1, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::eyeWear_01, ReanimTrackId::eyeWear_01_line, 10 },
	{ ZombatarImage::EYEWEAR_3, ZombatarImage::EYEWEAR_3_MASK, 28, 69, 0, 1, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::eyeWear_02, ReanimTrackId::eyeWear_02_line, 10 },
	{ ZombatarImage::EYEWEAR_4, ZombatarImage::EYEWEAR_4_MASK, 28, 78, 0, -1, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::eyeWear_03, ReanimTrackId::eyeWear_03_line, 10 },
	{ ZombatarImage::EYEWEAR_5, ZombatarImage::EYEWEAR_5_MASK, 30, 75, -1, -1, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::eyeWear_04, ReanimTrackId::eyeWear_04_line, 10 },
	{ ZombatarImage::EYEWEAR_6, ZombatarImage::EYEWEAR_6_MASK, 30, 78, -1, -1, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::eyeWear_05, ReanimTrackId::eyeWear_05_line, 10 },
	{ ZombatarImage::EYEWEAR_7, ZombatarImage::EYEWEAR_7_MASK, 50, 90, -1, -1, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::eyeWear_06, ReanimTrackId::eyeWear_06_line, 10 },
	{ ZombatarImage::EYEWEAR_8, ZombatarImage::EYEWEAR_8_MASK, 32, 70, -1, -1, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::eyeWear_07, ReanimTrackId::eyeWear_07_line, 10 },
	{ ZombatarImage::EYEWEAR_9, ZombatarImage::EYEWEAR_9_MASK, 36, 100, -1, -1, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::eyeWear_08, ReanimTrackId::eyeWear_08_line, 10 },
	{ ZombatarImage::EYEWEAR_10, ZombatarImage::EYEWEAR_10_MASK, 31, 75, -1, -1, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::eyeWear_09, ReanimTrackId::eyeWear_09_line, 10 },
	{ ZombatarImage::EYEWEAR_11, ZombatarImage::EYEWEAR_11_MASK, 31, 67, -1, -1, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::eyeWear_10, ReanimTrackId::eyeWear_10_line, 10 },
	{ ZombatarImage::EYEWEAR_12, ZombatarImage::EYEWEAR_12_MASK, 38, 95, -1, -1, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::eyeWear_11, ReanimTrackId::eyeWear_11_line, 10 },
	{ ZombatarImage::EYEWEAR_13, ZombatarImage::None, 30, 81, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::eyeWear_12, ReanimTrackId::None, 10 },
	{ ZombatarImage::EYEWEAR_14, ZombatarImage::None, 35, 64, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::eyeWear_13, ReanimTrackId::None, 10 },
	{ ZombatarImage::EYEWEAR_15, ZombatarImage::None, 42, 65, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::eyeWear_14, ReanimTrackId::None, 10 },
	{ ZombatarImage::EYEWEAR_16, ZombatarImage::None, 35, 65, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::eyeWear_15, ReanimTrackId::None, 10 }
};

const ZombatarItem gClothesItems[] = {
	{ ZombatarImage::CLOTHES_1, ZombatarImage::None, 88, 110, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 20, ReanimTrackId::None, ReanimTrackId::None, 0 },
	{ ZombatarImage::CLOTHES_2, ZombatarImage::None, 75, 100, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 20, ReanimTrackId::None, ReanimTrackId::None, 0 },
	{ ZombatarImage::CLOTHES_3, ZombatarImage::None, 85, 112, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 20, ReanimTrackId::None, ReanimTrackId::None, 0 },
	{ ZombatarImage::CLOTHES_4, ZombatarImage::None, 76, 110, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 20, ReanimTrackId::None, ReanimTrackId::None, 0 },
	{ ZombatarImage::CLOTHES_5, ZombatarImage::None, 89, 115, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 20, ReanimTrackId::None, ReanimTrackId::None, 0 },
	{ ZombatarImage::CLOTHES_6, ZombatarImage::None, 93, 110, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::None, ReanimTrackId::None, 0 },
	{ ZombatarImage::CLOTHES_7, ZombatarImage::None, 78, 105, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 20, ReanimTrackId::None, ReanimTrackId::None, 0 },
	{ ZombatarImage::CLOTHES_8, ZombatarImage::None, 88, 110, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 20, ReanimTrackId::None, ReanimTrackId::None, 0 },
	{ ZombatarImage::CLOTHES_9, ZombatarImage::None, 88, 102, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 20, ReanimTrackId::None, ReanimTrackId::None, 0 },
	{ ZombatarImage::CLOTHES_10, ZombatarImage::None, 85, 110, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 20, ReanimTrackId::None, ReanimTrackId::None, 0 },
	{ ZombatarImage::CLOTHES_11, ZombatarImage::None, 85, 110, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 20, ReanimTrackId::None, ReanimTrackId::None, 0 },
	{ ZombatarImage::CLOTHES_12, ZombatarImage::None, 79, 112, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 20, ReanimTrackId::None, ReanimTrackId::None, 0 }
};

const ZombatarItem gAccessoriesItems[] = {
	{ ZombatarImage::ACCESSORY_1, ZombatarImage::None, 103, 110, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::accessories_00, ReanimTrackId::None, 4 },
	{ ZombatarImage::ACCESSORY_2, ZombatarImage::None, 108, 110, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::accessories_01, ReanimTrackId::None, 4 },
	{ ZombatarImage::ACCESSORY_3, ZombatarImage::None, 86, 113, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::accessories_02, ReanimTrackId::None, 4 },
	{ ZombatarImage::ACCESSORY_4, ZombatarImage::None, 131, 95, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::accessories_03, ReanimTrackId::None, 4 },
	{ ZombatarImage::ACCESSORY_5, ZombatarImage::None, 131, 100, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::accessories_04, ReanimTrackId::None, 4 },
	{ ZombatarImage::ACCESSORY_6, ZombatarImage::None, 131, 100, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::accessories_14, ReanimTrackId::None, 4 },
	{ ZombatarImage::ACCESSORY_8, ZombatarImage::None, 104, 111, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::accessories_05, ReanimTrackId::None, 4 },
	{ ZombatarImage::ACCESSORY_9, ZombatarImage::None, 118, 65, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::accessories_06, ReanimTrackId::None, 4 },
	{ ZombatarImage::ACCESSORY_10, ZombatarImage::None, 61, 118, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::accessories_12, ReanimTrackId::None, 4 },
	{ ZombatarImage::ACCESSORY_11, ZombatarImage::None, 43, 100, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::accessories_07, ReanimTrackId::None, 4 },
	{ ZombatarImage::ACCESSORY_12, ZombatarImage::None, 135, 92, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::accessories_09, ReanimTrackId::None, 4 },
	{ ZombatarImage::ACCESSORY_13, ZombatarImage::None, 78, 130, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::accessories_10, ReanimTrackId::None, 4 },
	{ ZombatarImage::ACCESSORY_14, ZombatarImage::None, 68, 145, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::accessories_11, ReanimTrackId::None, 4 },
	{ ZombatarImage::ACCESSORY_15, ZombatarImage::None, 133, 70, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::accessories_13, ReanimTrackId::None, 4 },
	{ ZombatarImage::ACCESSORY_16, ZombatarImage::None, 13, 40, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::accessories_08, ReanimTrackId::None, 10 }
};

const ZombatarItem gHatsItems[] = {
	{ ZombatarImage::HATS_1, ZombatarImage::HATS_1_MASK, 28, 5, 2, 1, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::hats_00, ReanimTrackId::hats_00_line, 12 },
	{ ZombatarImage::HATS_2, ZombatarImage::None, 47, 12, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::hats_01, ReanimTrackId::hats_01_line, 12 },
	{ ZombatarImage::HATS_3, ZombatarImage::HATS_3_MASK, 36, 20, 15, -1, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::hats_02, ReanimTrackId::hats_02_line, 12 },
	{ ZombatarImage::HATS_4, ZombatarImage::None, 11, 10, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::hats_03, ReanimTrackId::hats_03_line, 12 },
	{ ZombatarImage::HATS_5, ZombatarImage::None, 41, 16, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::hats_04, ReanimTrackId::hats_04_line, 12 },
	{ ZombatarImage::HATS_6, ZombatarImage::HATS_6_MASK, 18, 3, 4, -2, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::hats_05, ReanimTrackId::hats_05_line, 12 },
	{ ZombatarImage::HATS_7, ZombatarImage::HATS_7_MASK, 53, 17, 0, 15, ZOMBATAR_COLOR_BRIGHT, 2, ReanimTrackId::hats_06, ReanimTrackId::hats_06_line, 12 },
	{ ZombatarImage::HATS_8, ZombatarImage::HATS_8_MASK, 3, 0, 0, -2, ZOMBATAR_COLOR_BRIGHT, 2, ReanimTrackId::hats_07, ReanimTrackId::hats_07_line, 12 },
	{ ZombatarImage::HATS_9, ZombatarImage::HATS_9_MASK, 38, 0, -1, -2, ZOMBATAR_COLOR_BRIGHT, 2, ReanimTrackId::hats_08, ReanimTrackId::hats_08_line, 12 },
	{ ZombatarImage::HATS_10, ZombatarImage::None, 13, 45, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::hats_09, ReanimTrackId::hats_09_line, 12 },
	{ ZombatarImage::HATS_11, ZombatarImage::HATS_11_MASK, 63, 8, 1, 14, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::hats_10, ReanimTrackId::hats_10_line, 12 },
	{ ZombatarImage::HATS_12, ZombatarImage::None, 43, 15, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::hats_11, ReanimTrackId::hats_11_line, 12 },
	{ ZombatarImage::HATS_13, ZombatarImage::None, 18, 0, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::None, ReanimTrackId::hats_12_line, 12 },
	{ ZombatarImage::HATS_14, ZombatarImage::None, 23, 5, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::hats_13, ReanimTrackId::hats_13_line, 12 }
};

const ZombatarItem gBackdropItems[] = {
	{ ZombatarImage::BACKGROUND_CRAZYDAVE, ZombatarImage::None, 0, 0, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::None, ReanimTrackId::None, 0 },
	{ ZombatarImage::BACKGROUND_MENU, ZombatarImage::None, 0, 0, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::None, ReanimTrackId::None, 0 },
	{ ZombatarImage::BACKGROUND_MENU_DOS, ZombatarImage::None, 0, 0, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::None, ReanimTrackId::None, 0 },
	{ ZombatarImage::BACKGROUND_ROOF, ZombatarImage::None, 0, 0, 0, 0, ZOMBATAR_COLOR_NOT_APPLICABLE, 32, ReanimTrackId::None, ReanimTrackId::None, 0 },
	{ ZombatarImage::BACKGROUND_BLANK, ZombatarImage::None, 0, 0, 0, 0, ZOMBATAR_COLOR_BRIGHT, 32, ReanimTrackId::None, ReanimTrackId::None, 0 }
};

struct ZombatarItemSpan
{
	const ZombatarItem* mItems;
	int mCount;
};

template <size_t Count>
constexpr ZombatarItemSpan MakeSpan(const ZombatarItem (&theItems)[Count])
{
	return { theItems, static_cast<int>(Count) };
}

ZombatarItemSpan GetRawItems(ZombatarCustomizationPage thePage)
{
	// 页 → 物品区段：按页枚举序直查（行序 = 枚举序 0..8；SKIN 页无条目表）。
	static constexpr ZombatarItemSpan kPageItems[] = {
		{ nullptr, 0 },                   // ZOMBATAR_CUSTOM_SKIN
		MakeSpan(gClothesItems),          // ZOMBATAR_CUSTOM_CLOTHES
		MakeSpan(gTidbitsItems),          // ZOMBATAR_CUSTOM_TIDBITS
		MakeSpan(gAccessoriesItems),      // ZOMBATAR_CUSTOM_ACCESSORIES
		MakeSpan(gFacialHairItems),       // ZOMBATAR_CUSTOM_FACIAL_HAIR
		MakeSpan(gHairItems),             // ZOMBATAR_CUSTOM_HAIR
		MakeSpan(gEyewearItems),          // ZOMBATAR_CUSTOM_EYEWEAR
		MakeSpan(gHatsItems),             // ZOMBATAR_CUSTOM_HATS
		MakeSpan(gBackdropItems),         // ZOMBATAR_CUSTOM_BACKDROPS
	};
	static_assert(sizeof(kPageItems) / sizeof(kPageItems[0]) == ZOMBATAR_CUSTOM_BACKDROPS + 1,
		"页条目槽位须与 ZombatarCustomizationPage 枚举同步");
	return (unsigned)thePage < sizeof(kPageItems) / sizeof(kPageItems[0]) ? kPageItems[thePage] : ZombatarItemSpan{ nullptr, 0 };
}

ZombatarButton* MakeZombatarButton(
	int theId,
	Widget* theParent,
	ZombatarImage theNormalImage,
	ZombatarImage theOverImage,
	ZombatarImage theDownImage,
	ZombatarImage theDisabledImage = ZombatarImage::None)
{
	ZombatarButton* aButton = new ZombatarButton(theId, theNormalImage,
		theOverImage, theDownImage, theDisabledImage);
	aButton->mParentWidget = theParent;
	return aButton;
}

void MoveButton(ZombatarButton* theButton, int theX, int theY)
{
	if (theButton)
	{
		theButton->mX = theX;
		theButton->mY = theY;
	}
}

bool CanPress(const ZombatarButton* theButton)
{
	return theButton && theButton->mIsOver && !theButton->mDisabled &&
		!theButton->mBtnNoDraw;
}

// The alignment bits in the 1073 Zombatar table are the same bits used by
// the original FitIconRect helper: 2=top, 4=bottom, 8=left, 16=right.
// Keeping this separate from the portrait coordinates prevents the thumbnail
// page from accidentally treating vertical alignment as horizontal.
Rect FitZombatarIconRect(const Rect& theCell, ZombatarImage theImage, int theAlignment)
{
	if (theImage == ZombatarImage::None)
		return Rect();

	const int aInset = 10;
	const int aAvailableWidth = std::max(1, theCell.mWidth - aInset * 2);
	const int aAvailableHeight = std::max(1, theCell.mHeight - aInset * 2);
	int aWidth = gZombatarAtlas.mImages[static_cast<size_t>(theImage)].mWidth;
	int aHeight = gZombatarAtlas.mImages[static_cast<size_t>(theImage)].mHeight;
	const bool aWidthOverflows = aWidth > aAvailableWidth;
	const bool aHeightOverflows = aHeight > aAvailableHeight;
	if (aWidthOverflows && (!aHeightOverflows || aWidth >= aHeight))
	{
		aHeight = std::max(1, static_cast<int>(aHeight *
			(static_cast<float>(aAvailableWidth) / aWidth)));
		aWidth = aAvailableWidth;
	}
	else if (aHeightOverflows)
	{
		aWidth = std::max(1, static_cast<int>(aWidth *
			(static_cast<float>(aAvailableHeight) / aHeight)));
		aHeight = aAvailableHeight;
	}

	int aX = theCell.mX + (theCell.mWidth - aWidth) / 2;
	int aY = theCell.mY + (theCell.mHeight - aHeight) / 2;
	if (theAlignment & 2)
		aY = theCell.mY + aInset;
	else if (theAlignment & 4)
		aY = theCell.mY + theCell.mHeight - aInset - aHeight;
	if (theAlignment & 8)
		aX = theCell.mX + aInset;
	else if (theAlignment & 16)
		aX = theCell.mX + theCell.mWidth - aInset - aWidth;
	return Rect(aX, aY, aWidth, aHeight);
}

bool ExportZombatarJpeg(const Zombatar& theZombatar, int theNumber, LawnApp* theApp)
{
	if (theNumber <= 0 || !theApp)
		return false;

	if (!theApp->mDDInterface ||
		!theApp->mDDInterface->mRenderInterface ||
		!theApp->mDDInterface->mRenderInterface->mRenderer)
		throw std::logic_error("Zombatar export requires a native GPU renderer");
	RenderInterface& aRender = *theApp->mDDInterface->mRenderInterface;

	const ZombatarItem* aBackdrop = ZombatarGetItem(
		ZOMBATAR_CUSTOM_BACKDROPS,
		theZombatar.mItems[ZOMBATAR_CUSTOM_BACKDROPS]);
	const int aWidth = aBackdrop ? gZombatarAtlas.mImages[static_cast<size_t>(aBackdrop->mImage)].mWidth : 180;
	const int aHeight = aBackdrop ? gZombatarAtlas.mImages[static_cast<size_t>(aBackdrop->mImage)].mHeight : 180;
	if (aWidth <= 0 || aHeight <= 0)
		throw std::logic_error("Zombatar export requires valid GPU target dimensions");

	Image* aRenderImage = aRender.CreateRenderImage(aWidth, aHeight);
	ImagePixels aPixels;
	try
	{
		GpuImageDrawScope aDrawScope(aRender, aRenderImage);
		Graphics aGraphics(aRenderImage);
		aGraphics.mLinearBlend = true;
		DrawZombatarPortrait(&aGraphics, theZombatar, 0, 0);
		aRender.Flush();
		// Snapshot the export before restoring the caller's in-progress scene.
		aPixels = aRender.ReadImagePixels(*aRenderImage);
	}
	catch (...)
	{
		delete aRenderImage;
		throw;
	}
	delete aRenderImage;

	const auto aDesktopPath = Sexy::ResolveDesktopDirectory();
	if (!aDesktopPath)
	{
		TodTraceAndLog("ZOMBATAR_EXPORT_DESKTOP_PATH_FAILED number=%d", theNumber);
		return false;
	}

	const std::string aFileName = (std::filesystem::path(*aDesktopPath) /
		("Zombatar_" + std::to_string(theNumber) + ".png")).string();
	// The only CPU pixels are this final PNG snapshot, never a drawing target.
	static_assert(sizeof(unsigned int) == 4,
		"PNG export requires 32-bit ImageLib pixels");
	ImageLib::Image aExportImage;
	aExportImage.mWidth = aPixels.width;
	aExportImage.mHeight = aPixels.height;
	aExportImage.mBits = aPixels.pixels.data();
	// The refreshed build keeps the portrait lossless.  The original 1073
	// export used a low-quality JPEG, which introduced avoidable colour and
	// alpha differences in the saved avatar.
	bool aWritten;
	try
	{
		aWritten = ImageLib::WritePNGImage(aFileName, &aExportImage);
	}
	catch (...)
	{
		aExportImage.mBits = nullptr;
		throw;
	}
	// ImageLib::Image owns mBits by default; detach the borrowed vector buffer.
	aExportImage.mBits = nullptr;
	return aWritten;
}

std::array<ZombatarButton*, 18> GetAllButtons(ZombatarItems* theItems)
{
	return {
		theItems->mPrevPageButton,
		theItems->mNextPageButton,
		theItems->mSkinButton,
		theItems->mHairButton,
		theItems->mFacialHairButton,
		theItems->mTidbitsButton,
		theItems->mEyeWearButton,
		theItems->mClothesButton,
		theItems->mAccessoryButton,
		theItems->mHatsButton,
		theItems->mBackdropsButton,
		theItems->mBackButton,
		theItems->mFinishedButton,
		theItems->mNewZombieButton,
		theItems->mNextZombieButton,
		theItems->mPrevZombieButton,
		theItems->mMainMenuButton,
		theItems->mViewButton
	};
}
}

const Color& ZombatarGetColor(int theColorIndex)
{
	static const Color aWhite(255, 255, 255, 255);
	if (theColorIndex < 0 ||
		theColorIndex >= static_cast<int>(sizeof(gZombatarColors) / sizeof(gZombatarColors[0])))
		return aWhite;
	return gZombatarColors[theColorIndex];
}

const ZombatarItem* ZombatarGetItem(ZombatarCustomizationPage thePage, int theSelectionIndex)
{
	if (theSelectionIndex < 0)
		return nullptr;

	// 1073 FUN_0052FCD0 stores the display-cell index in the save record and
	// removes the inserted separator cells only when looking up the raw item
	// table.  Keep this intentionally asymmetric with the editor grid: the
	// serialized value is the logical grid index, not a compact raw index.
	if (thePage != ZOMBATAR_CUSTOM_BACKDROPS && theSelectionIndex > 16)
		theSelectionIndex -= theSelectionIndex / 17;

	ZombatarItemSpan aItems = GetRawItems(thePage);
	if (theSelectionIndex < 0 || theSelectionIndex >= aItems.mCount)
		return nullptr;
	return &aItems.mItems[theSelectionIndex];
}

static int ZombatarGetRawItemCount(ZombatarCustomizationPage thePage);

namespace
{
// 1073's editor initializes the skin swatch to palette entry 0.  A partially
// written/old detail record can contain -1 (or an out-of-range value) there;
// feeding that value into the portrait path selects the white fallback palette
// and makes the entire skin appear bleached.  Normalize only values that are
// invalid for the corresponding table; valid component values are left byte
// for byte compatible with the GOTY save.
Zombatar NormalizeZombatarForRender(const Zombatar& theZombatar)
{
	Zombatar aResult = theZombatar;
	if (aResult.mColors[ZOMBATAR_CUSTOM_SKIN] < 0 ||
		aResult.mColors[ZOMBATAR_CUSTOM_SKIN] >= 12)
	{
		aResult.mColors[ZOMBATAR_CUSTOM_SKIN] = 0;
	}

	for (int aPage = ZOMBATAR_CUSTOM_CLOTHES;
		aPage <= ZOMBATAR_CUSTOM_HATS;
		++aPage)
	{
		if (aResult.mItems[aPage] >= 0 &&
			ZombatarGetItem(
				static_cast<ZombatarCustomizationPage>(aPage),
				aResult.mItems[aPage]) == nullptr)
		{
			aResult.mItems[aPage] = -1;
		}
	}
	if (aResult.mItems[ZOMBATAR_CUSTOM_BACKDROPS] < 0 ||
		aResult.mItems[ZOMBATAR_CUSTOM_BACKDROPS] >= ZombatarGetRawItemCount(ZOMBATAR_CUSTOM_BACKDROPS))
	{
		aResult.mItems[ZOMBATAR_CUSTOM_BACKDROPS] = 4;
	}
	return aResult;
}
}  // namespace

static int ZombatarGetRawItemCount(ZombatarCustomizationPage thePage)
{
	return GetRawItems(thePage).mCount;
}

void ResetZombatarReanimation(Reanimation* theReanimation)
{
	if (!theReanimation)
		return;

	for (int aPage = ZOMBATAR_CUSTOM_TIDBITS; aPage <= ZOMBATAR_CUSTOM_HATS; ++aPage)
	{
		ZombatarItemSpan aItems = GetRawItems(static_cast<ZombatarCustomizationPage>(aPage));
		for (int anIndex = 0; anIndex < aItems.mCount; ++anIndex)
		{
			const ReanimTrackId aTrack = aItems.mItems[anIndex].mTrack;
			if (aTrack != ReanimTrackId::None)  // None = 名字不在映射表：原版此处静默无操作
				theReanimation->AssignRenderGroupToTrack(aTrack, RENDER_GROUP_HIDDEN);

			const ReanimTrackId aLineTrack = aItems.mItems[anIndex].mLineTrack;
			if (aLineTrack != ReanimTrackId::None && theReanimation->TrackExists(aLineTrack))
				theReanimation->AssignRenderGroupToTrack(aLineTrack, RENDER_GROUP_HIDDEN);
		}
	}
}

void ApplyZombatarReanimation(Reanimation* theReanimation, const Zombatar& theZombatar)
{
	if (!theReanimation)
		return;

	const Zombatar aNormalized = NormalizeZombatarForRender(theZombatar);
	ResetZombatarReanimation(theReanimation);
	for (int aPage = ZOMBATAR_CUSTOM_TIDBITS; aPage <= ZOMBATAR_CUSTOM_HATS; ++aPage)
	{
		const ZombatarItem* anItem = ZombatarGetItem(
			static_cast<ZombatarCustomizationPage>(aPage),
			aNormalized.mItems[aPage]);
		if (!anItem)
			continue;

		if (anItem->mTrack != ReanimTrackId::None)  // None：原版此处静默无操作
			theReanimation->AssignRenderGroupToTrack(anItem->mTrack, anItem->mRenderGroup);
		// 涂色目标：None 显式取轨道 0——原版是经"名字查不到 -> FindTrackIndex
		// 回退轨道 0"达成同一行为的；回退语义删除后由这里显式表达，非 None
		// 路径的 miss 仍会经 FindTrackIndex 硬错。
		ReanimatorTrackInstance* aTrack = anItem->mTrack == ReanimTrackId::None
			? &theReanimation->mTrackInstances[0]
			: theReanimation->GetTrackInstanceByName(anItem->mTrack);
		aTrack->mTrackColor = ZombatarGetColor(aNormalized.mColors[aPage]);

		if (anItem->mLineTrack != ReanimTrackId::None &&
			theReanimation->TrackExists(anItem->mLineTrack))
			theReanimation->AssignRenderGroupToTrack(anItem->mLineTrack, anItem->mRenderGroup + 1);
	}
}

void DrawZombatarPortrait(Graphics* g, const Zombatar& theZombatar, int theX, int theY)
{
	if (!g)
		return;

	GraphicsAutoState aState(g);
	// A portrait is composed from several independent images.  Some of the
	// source sheets intentionally contain pixels outside the 180x180 portrait
	// canvas (for example a hat brim or a backdrop edge).  The original
	// Zombatar renderer establishes the portrait canvas before composing those
	// images; without that state adjacent saved portraits can bleed into one
	// another and through the stone display frame.
	const TextureRegion& aCanvas = gZombatarAtlas.mImages[static_cast<size_t>(ZombatarImage::BACKGROUND_BLANK)];
	const int aPortraitWidth = aCanvas.mWidth;
	const int aPortraitHeight = aCanvas.mHeight;
	g->ClipRect(theX, theY, aPortraitWidth, aPortraitHeight);
	const Zombatar aNormalized = NormalizeZombatarForRender(theZombatar);
	// A portrait may follow a reanimation draw in the same Graphics object.
	// Establish the neutral state explicitly so a stale tint cannot leak into
	// the skin or the next component.
	g->mColorizeImages = false;
	g->mColor = Color::White;
	const ZombatarItem* aBackdrop = ZombatarGetItem(
		ZOMBATAR_CUSTOM_BACKDROPS,
		aNormalized.mItems[ZOMBATAR_CUSTOM_BACKDROPS]);
	if (aBackdrop && aBackdrop->mImage != ZombatarImage::None)
	{
		if (aBackdrop->mColorMode != ZOMBATAR_COLOR_NOT_APPLICABLE)
		{
			g->mColor = ZombatarGetColor(aNormalized.mColors[ZOMBATAR_CUSTOM_BACKDROPS]);
			g->mColorizeImages = true;
		}
		DrawZombatarImage(g, aBackdrop->mImage, theX + aBackdrop->mX, theY + aBackdrop->mY);
		g->mColorizeImages = false;
		g->mColor = Color::White;
	}

	const ZombatarImage aSkin = ZombatarImage::ZOMBIE_BLANK_SKIN;
	const ZombatarImage aBlank = ZombatarImage::ZOMBIE_BLANK;
	{
		// 1073 FUN_00530970 aligns both zombie base images to the backdrop's
		// bottom-right corner.  Centering them shifts the portrait about
		// 19 pixels left and 20 pixels up in the 180x180 display window.
		// 1073 anchors the two base images to the blank-background canvas,
		// rather than to whichever backdrop happens to be selected.
		const TextureRegion& aSkinRect = gZombatarAtlas.mImages[static_cast<size_t>(aSkin)];
		const TextureRegion& aBlankRect = gZombatarAtlas.mImages[static_cast<size_t>(aBlank)];
		const int aSkinX = theX + aCanvas.mWidth - aSkinRect.mWidth;
		const int aSkinY = theY + aCanvas.mHeight - aSkinRect.mHeight;
		g->mColor = ZombatarGetColor(aNormalized.mColors[ZOMBATAR_CUSTOM_SKIN]);
		g->mColorizeImages = true;
		DrawZombatarImage(g, aSkin, aSkinX, aSkinY);
		g->mColorizeImages = false;

		const int aBlankX = theX + aCanvas.mWidth - aBlankRect.mWidth;
		const int aBlankY = theY + aCanvas.mHeight - aBlankRect.mHeight;
		DrawZombatarImage(g, aBlank, aBlankX, aBlankY);
		g->mColor = Color::White;
	}

	struct SelectedItem
	{
		int mPage;
		const ZombatarItem* mItem;
	};
	std::vector<SelectedItem> aSelectedItems;
	for (int aPage = ZOMBATAR_CUSTOM_CLOTHES; aPage <= ZOMBATAR_CUSTOM_HATS; ++aPage)
	{
		const ZombatarItem* anItem = ZombatarGetItem(
			static_cast<ZombatarCustomizationPage>(aPage),
			aNormalized.mItems[aPage]);
		if (anItem)
			aSelectedItems.push_back({ aPage, anItem });
	}

	std::stable_sort(aSelectedItems.begin(), aSelectedItems.end(),
		[](const SelectedItem& theLeft, const SelectedItem& theRight)
		{
			if (theLeft.mItem->mRenderGroup != theRight.mItem->mRenderGroup)
				return theLeft.mItem->mRenderGroup < theRight.mItem->mRenderGroup;
			return theLeft.mPage > theRight.mPage;
		});

	for (const SelectedItem& aSelected : aSelectedItems)
	{
		const ZombatarItem& anItem = *aSelected.mItem;
		if (anItem.mImage == ZombatarImage::None)
			continue;

		if (anItem.mColorMode != ZOMBATAR_COLOR_NOT_APPLICABLE)
		{
			g->mColor = ZombatarGetColor(aNormalized.mColors[aSelected.mPage]);
			g->mColorizeImages = true;
		}

		if (anItem.mColorMask != ZombatarImage::None)
		{
			DrawZombatarImage(g,
				anItem.mColorMask,
				theX + anItem.mX + anItem.mMaskX,
				theY + anItem.mY + anItem.mMaskY);
			g->mColorizeImages = false;
			DrawZombatarImage(g, anItem.mImage, theX + anItem.mX, theY + anItem.mY);
		}
		else
		{
			DrawZombatarImage(g, anItem.mImage, theX + anItem.mX, theY + anItem.mY);
			g->mColorizeImages = false;
		}
		g->mColor = Color::White;
	}
}

ZombatarItems::ZombatarItems(LawnApp* theApp) :
	mApp(theApp),
	mPrevPageButton(nullptr),
	mNextPageButton(nullptr),
	mSkinButton(nullptr),
	mHairButton(nullptr),
	mFacialHairButton(nullptr),
	mTidbitsButton(nullptr),
	mEyeWearButton(nullptr),
	mClothesButton(nullptr),
	mAccessoryButton(nullptr),
	mHatsButton(nullptr),
	mBackdropsButton(nullptr),
	mBackButton(nullptr),
	mFinishedButton(nullptr),
	mNewZombieButton(nullptr),
	mNextZombieButton(nullptr),
	mPrevZombieButton(nullptr),
	mMainMenuButton(nullptr),
	mViewButton(nullptr),
	mColors(nullptr),
	mCurrentPage(1),
	mPageCount(1),
	mItemGap(-4),
	mItemCount(0),
	mItemsPerPage(18),
	mCellPadding(10),
	mHoveredItem(-1),
	mPressedItem(-1),
	mCurrentCategory(ZOMBATAR_CUSTOM_SKIN),
	mState(ZombatarEditorState::EDITING),
	mTransitionCounter(0),
	mSelectedZombatar(0),
	mPortraitGap(40),
	mSelectionChanged(false),
	mSelectionChangedLastFrame(false),
	mDeleteHovered(false),
	mDeleteRect(321, 121, 60, 20),
	mPressedButtonId(-1)
{
	// 1073 gives the child the asset width but keeps the full logical screen
	// height for the controls below the 560x493 artwork.
	const int aWidth = gZombatarAtlas.mImages[static_cast<size_t>(ZombatarImage::WIDGET_BG)].mWidth;
	const int aHeight = mApp ? mApp->mHeight : 600;
	Resize(25, 25, aWidth, aHeight);

	mPrevPageButton = MakeZombatarButton(
		BUTTON_PREV_PAGE, this, ZombatarImage::PREV_BUTTON,
		ZombatarImage::PREV_BUTTON_HIGHLIGHT,
		ZombatarImage::PREV_BUTTON_HIGHLIGHT,
		ZombatarImage::PREV_BUTTON);
	mNextPageButton = MakeZombatarButton(
		BUTTON_NEXT_PAGE, this, ZombatarImage::NEXT_BUTTON,
		ZombatarImage::NEXT_BUTTON_HIGHLIGHT,
		ZombatarImage::NEXT_BUTTON_HIGHLIGHT,
		ZombatarImage::NEXT_BUTTON);

	const int aPageButtonY = mHeight - mPrevPageButton->mHeight - 215;
	MoveButton(mPrevPageButton, 150, aPageButtonY);
	MoveButton(mNextPageButton, mWidth - mNextPageButton->mWidth - 55, aPageButtonY);

	mSkinButton = MakeZombatarButton(
		BUTTON_SKIN, this, ZombatarImage::SKIN_BUTTON,
		ZombatarImage::SKIN_BUTTON, ZombatarImage::SKIN_BUTTON_HIGHLIGHT);
	mHairButton = MakeZombatarButton(
		BUTTON_HAIR, this, ZombatarImage::HAIR_BUTTON,
		ZombatarImage::HAIR_BUTTON_OVER, ZombatarImage::HAIR_BUTTON_HIGHLIGHT);
	mFacialHairButton = MakeZombatarButton(
		BUTTON_FACIAL_HAIR, this, ZombatarImage::FACIAL_HAIR_BUTTON,
		ZombatarImage::FACIAL_HAIR_BUTTON_OVER, ZombatarImage::FACIAL_HAIR_BUTTON_HIGHLIGHT);
	mTidbitsButton = MakeZombatarButton(
		BUTTON_TIDBITS, this, ZombatarImage::TIDBITS_BUTTON,
		ZombatarImage::TIDBITS_BUTTON_OVER, ZombatarImage::TIDBITS_BUTTON_HIGHLIGHT);
	mEyeWearButton = MakeZombatarButton(
		BUTTON_EYEWEAR, this, ZombatarImage::EYEWEAR_BUTTON,
		ZombatarImage::EYEWEAR_BUTTON_OVER, ZombatarImage::EYEWEAR_BUTTON_HIGHLIGHT);
	mClothesButton = MakeZombatarButton(
		BUTTON_CLOTHES, this, ZombatarImage::CLOTHES_BUTTON,
		ZombatarImage::CLOTHES_BUTTON_OVER, ZombatarImage::CLOTHES_BUTTON_HIGHLIGHT);
	mAccessoryButton = MakeZombatarButton(
		BUTTON_ACCESSORIES, this, ZombatarImage::ACCESSORY_BUTTON,
		ZombatarImage::ACCESSORY_BUTTON_OVER, ZombatarImage::ACCESSORY_BUTTON_HIGHLIGHT);
	mHatsButton = MakeZombatarButton(
		BUTTON_HATS, this, ZombatarImage::HATS_BUTTON,
		ZombatarImage::HATS_BUTTON_OVER, ZombatarImage::HATS_BUTTON_HIGHLIGHT);
	mBackdropsButton = MakeZombatarButton(
		BUTTON_BACKDROPS, this, ZombatarImage::BACKDROPS_BUTTON,
		ZombatarImage::BACKDROPS_BUTTON_OVER, ZombatarImage::BACKDROPS_BUTTON_HIGHLIGHT);

	const std::array<ZombatarButton*, 9> aCategoryButtons = {
		mSkinButton, mHairButton, mFacialHairButton, mTidbitsButton,
		mEyeWearButton, mClothesButton, mAccessoryButton, mHatsButton,
		mBackdropsButton
	};
	int aCategoryY = 103;
	for (ZombatarButton* aButton : aCategoryButtons)
	{
		MoveButton(aButton, 33, aCategoryY);
		aCategoryY += aButton->mHeight - 3;
	}

	mBackButton = MakeZombatarButton(
		BUTTON_BACK, this, ZombatarImage::BACK_BUTTON,
		ZombatarImage::BACK_BUTTON_HIGHLIGHT, ZombatarImage::None);
	MoveButton(mBackButton, 360, 320);

	mFinishedButton = MakeZombatarButton(
		BUTTON_FINISHED, this, ZombatarImage::FINISHED_BUTTON,
		ZombatarImage::FINISHED_BUTTON_HIGHLIGHT, ZombatarImage::FINISHED_BUTTON_HIGHLIGHT);
	MoveButton(mFinishedButton, 420, 447);

	mNewZombieButton = MakeZombatarButton(
		BUTTON_NEW_ZOMBIE, this, ZombatarImage::NEWZOMBIE_BUTTON,
		ZombatarImage::NEWZOMBIE_BUTTON_HIGHLIGHT, ZombatarImage::NEWZOMBIE_BUTTON_HIGHLIGHT);
	MoveButton(mNewZombieButton, 170, 370);

	mNextZombieButton = MakeZombatarButton(
		BUTTON_NEXT_ZOMBIE, this, ZombatarImage::NEXT_BUTTON,
		ZombatarImage::NEXT_BUTTON_HIGHLIGHT,
		ZombatarImage::NEXT_BUTTON_HIGHLIGHT,
		ZombatarImage::NEXT_BUTTON);
	mPrevZombieButton = MakeZombatarButton(
		BUTTON_PREV_ZOMBIE, this, ZombatarImage::PREV_BUTTON,
		ZombatarImage::PREV_BUTTON_HIGHLIGHT,
		ZombatarImage::PREV_BUTTON_HIGHLIGHT,
		ZombatarImage::PREV_BUTTON);
	const int aBrowseButtonY = mHeight - mNextZombieButton->mHeight - 189;
	MoveButton(mNextZombieButton, mWidth - mNextZombieButton->mWidth - 85, aBrowseButtonY);
	MoveButton(mPrevZombieButton, 95, aBrowseButtonY);

	mMainMenuButton = MakeZombatarButton(
		BUTTON_MAIN_MENU, this, ZombatarImage::None,
		ZombatarImage::MAINMENUBACK_HIGHLIGHT,
		ZombatarImage::MAINMENUBACK_HIGHLIGHT);
	MoveButton(mMainMenuButton, 253, mHeight - 97);

	mViewButton = MakeZombatarButton(
		BUTTON_VIEW, this, ZombatarImage::VIEW_BUTTON,
		ZombatarImage::VIEW_BUTTON_HIGHLIGHT,
		ZombatarImage::VIEW_BUTTON_HIGHLIGHT);
	// FUN_0052D550 initially gives button 0x3F0 the same location as the
	// main-menu hit target.  FUN_0052E710 moves it to the editor frame's
	// lower-left corner before every draw; Draw() reproduces that second step.
	MoveButton(mViewButton, 253, mHeight - 97);

	mColors = new ZombatarColors(mApp);
	AddWidget(mColors);

	ResetZombatar();
	EnterInitialState();
}

ZombatarItems::~ZombatarItems()
{
	if (mColors && mColors->mParent == this)
		RemoveWidget(mColors);
	delete mColors;
	mColors = nullptr;

	for (ZombatarButton* aButton : GetAllButtons(this))
		delete aButton;
}

void ZombatarItems::ResetZombatar()
{
    CancelDeleteDialog();
	for (int aPage = 0; aPage < ZOMBATAR_NUM_ZOMBATAR_CUSTOM_PAGES; ++aPage)
	{
		mZombatar.mColors[aPage] = -1;
		mZombatar.mItems[aPage] = -1;
	}
	mZombatar.mColors[ZOMBATAR_CUSTOM_SKIN] = 0;
	mZombatar.mColors[ZOMBATAR_CUSTOM_BACKDROPS] = 47;
	mZombatar.mItems[ZOMBATAR_CUSTOM_BACKDROPS] = 4;

	mSelectedZombatar = 0;
	mSelectionChanged = true;
	SetCategory(ZOMBATAR_CUSTOM_SKIN);
}

void ZombatarItems::EnterInitialState()
{
    CancelDeleteDialog();
	PlayerInfo* aPlayer = mApp ? mApp->mPlayerInfo : nullptr;
	mState = aPlayer && aPlayer->mNumZombatars > 0
		? ZombatarEditorState::BROWSING
		: ZombatarEditorState::EDITING;
	mSelectedZombatar = 0;
	mCurrentPage = 1;
	mHoveredItem = -1;
	mPressedItem = -1;
	mPressedButtonId = -1;
	mDeleteHovered = false;
	mTransitionCounter = 0;
	ResetTransientLayout();
	RefreshState();
}

void ZombatarItems::ResetTransientLayout()
{
	// These controls participate in the 180-tick editor/confirmation
	// transition.  The selector keeps this widget alive while it slides to
	// other pages, so a transition can be interrupted and leave the button at
	// its animated coordinate.  1073 reconstructs the static endpoints every
	// time the page is entered.
	MoveButton(mBackButton, 360, 320);
	MoveButton(mFinishedButton, 420, 447);
	MoveButton(mNewZombieButton, 170, 370);
	MoveButton(mMainMenuButton, 253, mHeight - 97);
}

void ZombatarItems::UpdateViewButtonLayout()
{
	if (!mViewButton)
		return;

	// FUN_0052E710 applies this relocation before the input pass as well as
	// before drawing.  Doing it in one helper keeps the visual rectangle and
	// the mouse hit rectangle identical on the first frame after a page change.
	MoveButton(
		mViewButton,
		40,
		(mApp ? mApp->mHeight : 600) - 153);
}

void ZombatarItems::SetCategory(ZombatarCustomizationPage thePage)
{
	if (thePage < ZOMBATAR_CUSTOM_SKIN || thePage >= ZOMBATAR_NUM_ZOMBATAR_CUSTOM_PAGES)
		return;

	mCurrentCategory = thePage;
	mCurrentPage = 1;
	mHoveredItem = -1;
	mPressedItem = -1;
	LayoutCategoryItems();
	SyncColorMode();
	MarkDirty();
}

void ZombatarItems::LayoutCategoryItems()
{
	for (ZombatarItemCell& aCell : mItemCells)
		aCell = ZombatarItemCell();

	if (mCurrentCategory == ZOMBATAR_CUSTOM_SKIN)
	{
		mItemCount = 0;
		mPageCount = 1;
		mItemsPerPage = 18;
		mPrevPageButton->mDisabled = true;
		mNextPageButton->mDisabled = true;
		return;
	}

	std::vector<const ZombatarItem*> aLogicalItems;
	const int aRawCount = ZombatarGetRawItemCount(mCurrentCategory);
	for (int aRawIndex = 0; aRawIndex < aRawCount; ++aRawIndex)
	{
		// FUN_0052f6a0 appends the separator after every seventeenth raw
		// item, then appends one final separator for non-backdrop pages.
		// The save record remains compact; separators are display-only.
		aLogicalItems.push_back(&GetRawItems(mCurrentCategory).mItems[aRawIndex]);
		if (mCurrentCategory != ZOMBATAR_CUSTOM_BACKDROPS &&
			(((aRawIndex + 1) % 17) == 0 || aRawIndex == aRawCount - 1))
		{
			aLogicalItems.push_back(nullptr);
		}
	}

	mItemCount = std::min(static_cast<int>(aLogicalItems.size()), MAX_ITEM_CELLS);
	mItemsPerPage = 18;
	mPageCount = std::max(1, (mItemCount + mItemsPerPage - 1) / mItemsPerPage);
	mCurrentPage = std::clamp(mCurrentPage, 1, mPageCount);

	const int aTileWidth = gZombatarAtlas.mImages[static_cast<size_t>(ZombatarImage::ACCESSORY_BG)].mWidth;
	const int aTileHeight = gZombatarAtlas.mImages[static_cast<size_t>(ZombatarImage::ACCESSORY_BG)].mHeight;
	const int aStartX = (mWidth - (aTileWidth + mItemGap) * 6) / 2 + 50;
	const int aStartY = mItemGap * 2 + 120;

	for (int anIndex = 0; anIndex < mItemCount; ++anIndex)
	{
		const int anIndexOnPage = anIndex % mItemsPerPage;
		const int aColumn = anIndexOnPage % 6;
		const int aRow = anIndexOnPage / 6;
		ZombatarItemCell& aCell = mItemCells[anIndex];
		aCell.mItem = aLogicalItems[anIndex];
		if (aCell.mItem)
		{
			// The original stores this logical grid index, including the
			// separator slots, in Zombatar::mItems.
			aCell.mRawIndex = anIndex;
		}
		aCell.mX = aStartX + aColumn * (aTileWidth + mItemGap);
		aCell.mY = aStartY + aRow * (aTileHeight + mItemGap + 3);

	}

	mPrevPageButton->mDisabled = mCurrentPage <= 1;
	mNextPageButton->mDisabled = mCurrentPage >= mPageCount;
}

void ZombatarItems::SyncColorMode()
{
	if (!mColors)
		return;

	ZombatarColorMode aMode = ZOMBATAR_COLOR_SKIN;
	if (mCurrentCategory != ZOMBATAR_CUSTOM_SKIN)
	{
		const int aSelection = mZombatar.mItems[mCurrentCategory];
		if (aSelection < 0)
			aMode = ZOMBATAR_COLOR_ITEM_NOT_CHOSEN;
		else
		{
			const ZombatarItem* anItem = ZombatarGetItem(mCurrentCategory, aSelection);
			aMode = anItem ? anItem->mColorMode : ZOMBATAR_COLOR_NOT_APPLICABLE;
		}
	}
	mColors->SetColorMode(aMode, mZombatar.mColors[mCurrentCategory]);
}

void ZombatarItems::RefreshState()
{
	const bool isEditing =
		mState == ZombatarEditorState::EDITING ||
		mState == ZombatarEditorState::LEAVING_CONFIRMATION;
	const bool isConfirmation =
		mState == ZombatarEditorState::ENTERING_CONFIRMATION ||
		mState == ZombatarEditorState::CONFIRMATION;

	for (ZombatarButton* aButton : {
		mPrevPageButton, mNextPageButton, mSkinButton, mHairButton,
		mFacialHairButton, mTidbitsButton, mEyeWearButton, mClothesButton,
		mAccessoryButton, mHatsButton, mBackdropsButton })
	{
		aButton->mDisabled = !isEditing;
	}

	if (isEditing)
	{
		mPrevPageButton->mDisabled = mCurrentPage <= 1;
		mNextPageButton->mDisabled = mCurrentPage >= mPageCount;
	}

	mBackButton->mDisabled = !isConfirmation;
	mFinishedButton->mDisabled = mState == ZombatarEditorState::BROWSING;
	mMainMenuButton->mDisabled = false;

	PlayerInfo* aPlayer = mApp ? mApp->mPlayerInfo : nullptr;
	const int aCount = aPlayer ? aPlayer->mNumZombatars : 0;
	mViewButton->mDisabled = !isEditing || aCount <= 0;
	mNewZombieButton->mDisabled = mState != ZombatarEditorState::BROWSING ||
		aCount >= MAX_NUM_ZOMBATARS;
	mPrevZombieButton->mDisabled = mState != ZombatarEditorState::BROWSING ||
		mSelectedZombatar <= 0;
	mNextZombieButton->mDisabled = mState != ZombatarEditorState::BROWSING ||
		mSelectedZombatar >= aCount - 1;

	if (mColors)
		mColors->SetVisible(isEditing);
	MarkDirty();
}

void ZombatarItems::UpdateEditorButtons()
{
	for (ZombatarButton* aButton : GetAllButtons(this))
		aButton->Update();
}

void ZombatarItems::Update()
{
	Widget::Update();
	UpdateViewButtonLayout();
	mSelectionChangedLastFrame = mSelectionChanged;
	mSelectionChanged = false;
	UpdateEditorButtons();

	if (mState == ZombatarEditorState::EDITING)
	{
		if (mColors && mColors->mSelectionChangedLastFrame)
		{
			mZombatar.mColors[mCurrentCategory] = mColors->mSelectedColor;
			mSelectionChanged = true;
		}
	}
	else if (mState == ZombatarEditorState::ENTERING_CONFIRMATION ||
		mState == ZombatarEditorState::LEAVING_CONFIRMATION)
	{
		--mTransitionCounter;
		const bool isEntering = mState == ZombatarEditorState::ENTERING_CONFIRMATION;
		const int aStartX = isEntering ? 420 : 130;
		const int anEndX = isEntering ? 130 : 420;
		const int aStartY = isEntering ? 445 : 320;
		const int anEndY = isEntering ? 320 : 445;
		const int aX = TodAnimateCurveNs(1'800'000'000, 0, mTransitionCounter.mNanoseconds, aStartX, anEndX, Curves::Linear);
		const int aY = TodAnimateCurveNs(1'800'000'000, 0, mTransitionCounter.mNanoseconds, aStartY, anEndY, Curves::Linear);
		MoveButton(mFinishedButton, aX, aY);

		if (mTransitionCounter <= 0)
		{
			mTransitionCounter = 0;
			MoveButton(
				mFinishedButton,
				isEntering ? 130 : 420,
				isEntering ? 320 : 447);
			mState = isEntering
				? ZombatarEditorState::CONFIRMATION
				: ZombatarEditorState::EDITING;
			RefreshState();
		}
	}
}

void ZombatarItems::Draw(Graphics* g)
{
	// Original 1073 FUN_0052E710 overwrites the constructor position of the
	// View Avatars button on every draw: x=40, y=appHeight-153.  The button at
	// (253, appHeight-97) is the separate transparent Main Menu/Back target.
	UpdateViewButtonLayout();

	DrawZombatarImage(g, ZombatarImage::WIDGET_BG, 0, 0);

	// C 形态 -> 成员函数内 lambda 表（表在类作用域内→private 方法 DrawEditor/
	// DrawConfirmation/DrawSavedZombatars 可访问）；ENTERING/LEAVING 共体
	{
		static constexpr void (*kDrawCases[5])(ZombatarItems&, Graphics*) = {
		    nullptr,  // 0 EDITING（走下方分支）
		    [](ZombatarItems& zi, Graphics* g) {   // 1 ENTERING_CONFIRMATION
		        zi.DrawEditor(g);
		        const int anAlpha = TodAnimateCurveNs(1'800'000'000, 0, zi.mTransitionCounter.mNanoseconds, 0, 255, Curves::Linear);
		        g->mColor = Color(0, 0, 0, anAlpha);
		        g->FillRect(33, 100, zi.mWidth - 63, zi.mHeight - 269);
		    },
		    [](ZombatarItems& zi, Graphics* g) {   // 2 LEAVING_CONFIRMATION
		        zi.DrawEditor(g);
		        const int anAlpha = TodAnimateCurveNs(1'800'000'000, 0, zi.mTransitionCounter.mNanoseconds, 255, 0, Curves::Linear);
		        g->mColor = Color(0, 0, 0, anAlpha);
		        g->FillRect(33, 100, zi.mWidth - 63, zi.mHeight - 269);
		    },
		    nullptr,  // 3 CONFIRMATION（走下方分支）
		    nullptr,  // 4 BROWSING（走下方分支）
		};
		const auto aState = static_cast<unsigned>(mState);
		if (aState < 5 && kDrawCases[aState] != nullptr)
		{
			kDrawCases[aState](*this, g);
		}
		else if (mState == ZombatarEditorState::EDITING)
		{
			DrawEditor(g);
		}
		else if (mState == ZombatarEditorState::CONFIRMATION)
		{
			DrawConfirmation(g);
		}
		else if (mState == ZombatarEditorState::BROWSING)
		{
			DrawSavedZombatars(g);
		}
	}

	mMainMenuButton->Draw(g);
}

void ZombatarItems::DrawEditor(Graphics* g)
{
	DrawZombatarImage(g, ZombatarImage::WIDGET_INNER_BG, 127, 100);
	for (ZombatarButton* aButton : {
		mSkinButton, mHairButton, mFacialHairButton, mTidbitsButton,
		mEyeWearButton, mClothesButton, mAccessoryButton, mHatsButton,
		mBackdropsButton })
	{
		aButton->Draw(g);
	}

	if (mCurrentCategory == ZOMBATAR_CUSTOM_SKIN)
	{
		TodDrawStringWrapped(
			g,
			gStr.ZOMBATAR_START_TEXT,
			Rect(mWidth / 2 - 200, 160, 500, 100),
			// 1073 FUN_0052FE90 loads DAT_00726728 here, which the
			// LoadingFonts table identifies as FONT_DWARVENTODCRAFT15.
			FONT_DWARVENTODCRAFT15,
			Color(254, 227, 0, 175),
			// The original passes justification 2 (horizontal centering only).
			DS_ALIGN_CENTER);
	}
	else
	{
		DrawItemGrid(g);
	}

	DrawCategoryHighlight(g);
	mFinishedButton->Draw(g);
	if (!mViewButton->mDisabled)
		mViewButton->Draw(g);
}

void ZombatarItems::DrawItemGrid(Graphics* g)
{
	const int aFirstIndex = (mCurrentPage - 1) * mItemsPerPage;
	const int anEndIndex = std::min(mItemCount, aFirstIndex + mItemsPerPage);
	const int aSelectedIndex = mZombatar.mItems[mCurrentCategory];
	const int aTileWidth = gZombatarAtlas.mImages[static_cast<size_t>(ZombatarImage::ACCESSORY_BG)].mWidth;
	const int aTileHeight = gZombatarAtlas.mImages[static_cast<size_t>(ZombatarImage::ACCESSORY_BG)].mHeight;

	for (int anIndex = aFirstIndex; anIndex < anEndIndex; ++anIndex)
	{
		const ZombatarItemCell& aCell = mItemCells[anIndex];
		const bool isSelected = aCell.mItem && aCell.mRawIndex == aSelectedIndex;
		const bool isHovered = anIndex == mHoveredItem;
		const bool isDimmed = !isSelected && !isHovered;
		if (isSelected)
		{
			// In 1073 only the selected cell has the green border.  Hovering a
			// cell cancels the dim overlay but keeps its normal stone frame.
			g->mColorizeImages = false;
			DrawZombatarImage(g, ZombatarImage::ACCESSORY_BG_HIGHLIGHT, aCell.mX, aCell.mY);
		}
		else
		{
			if (!isDimmed)
				g->mColorizeImages = false;
			else
			{
				g->mColorizeImages = true;
				g->mColor = Color(128, 128, 128, 128);
			}
			DrawZombatarImage(g, ZombatarImage::ACCESSORY_BG, aCell.mX, aCell.mY);
		}

		if (!aCell.mItem)
		{
			// The separator/none image inherits the same cell-wide state as the
			// stone frame. 1073 does not establish a second tint here.
			DrawZombatarImage(g, ZombatarImage::ACCESSORY_BG_NONE, aCell.mX, aCell.mY);
			continue;
		}
		if (aCell.mItem->mImage == ZombatarImage::None)
		{
			continue;
		}

		// The clothes page shows the same cropped blank zombie behind every
		// clothing item in 1073.  It is part of the editor, not a separate
		// saved component.
		if (mCurrentCategory == ZOMBATAR_CUSTOM_CLOTHES)
		{
			GraphicsAutoState aClothesState(g);
			// 1073 0x530117/0x530152 deliberately queries the dimensions of
			// ZombatarImage::ACCESSORY_BG, not the 142x140 zombie images.  Both
			// zombie layers are therefore scaled to the 67x67 stone-cell size,
			// positioned ten pixels up/left, and clipped by the inset cell rect.
			// Drawing the source images at native size leaves only a giant eye and
			// mouth visible in each thumbnail.
			g->ClipRect(
				aCell.mX + mCellPadding,
				aCell.mY + mCellPadding,
				aTileWidth - mCellPadding,
				aTileHeight - mCellPadding);
			const Zombatar aNormalized = NormalizeZombatarForRender(mZombatar);
			Color aSkinColor = ZombatarGetColor(
				aNormalized.mColors[ZOMBATAR_CUSTOM_SKIN]);
			if (isDimmed)
				aSkinColor.mAlpha = 128;
			g->mColorizeImages = true;
			g->mColor = aSkinColor;
			DrawZombatarImage(g,
				ZombatarImage::ZOMBIE_BLANK_SKIN,
				aCell.mX - mCellPadding,
				aCell.mY - mCellPadding,
				aTileWidth,
				aTileHeight);
			g->mColorizeImages = false;
			DrawZombatarImage(g,
				ZombatarImage::ZOMBIE_BLANK,
				aCell.mX - mCellPadding,
				aCell.mY - mCellPadding,
				aTileWidth,
				aTileHeight);
			g->mColor = Color::White;
		}

		const ZombatarImage anImage = aCell.mItem->mImage;
		const TextureRegion& anImageRect = gZombatarAtlas.mImages[static_cast<size_t>(anImage)];
		const Rect aIconRect = FitZombatarIconRect(
			Rect(aCell.mX, aCell.mY, aTileWidth, aTileHeight),
			anImage,
			aCell.mItem->mAlignment);
		const Rect aSourceRect(0, 0, anImageRect.mWidth, anImageRect.mHeight);

		if (aCell.mItem->mColorMask != ZombatarImage::None)
		{
			const ZombatarImage aMask = aCell.mItem->mColorMask;
			{
				const TextureRegion& aMaskSource = gZombatarAtlas.mImages[static_cast<size_t>(aMask)];
				const float aScaleX = static_cast<float>(aIconRect.mWidth) / anImageRect.mWidth;
				const float aScaleY = static_cast<float>(aIconRect.mHeight) / anImageRect.mHeight;
				const Rect aMaskRect(
					aIconRect.mX + static_cast<int>(aCell.mItem->mMaskX * aScaleX),
					aIconRect.mY + static_cast<int>(aCell.mItem->mMaskY * aScaleY),
					std::max(1, static_cast<int>(aMaskSource.mWidth * aScaleX)),
					std::max(1, static_cast<int>(aMaskSource.mHeight * aScaleY)));
				DrawZombatarImage(g, aMask, aMaskRect,
					Rect(0, 0, aMaskSource.mWidth, aMaskSource.mHeight));
			}
			// The mask and primary image both inherit the cell-wide gray/original
			// state. Thumbnail colors never follow the selected portrait palette.
			DrawZombatarImage(g, anImage, aIconRect, aSourceRect);
		}
		else
		{
			DrawZombatarImage(g, anImage, aIconRect, aSourceRect);
		}
	}
	g->mColorizeImages = false;
	g->mColor = Color::White;

	if (mPageCount > 1)
	{
		// 1073 draws the normal arrow through the image-color path with this
		// translucent blue-grey tint after reaching the first or last page.
		auto DrawPageButton = [g](ZombatarButton* theButton)
		{
			if (!theButton->mDisabled)
			{
				theButton->Draw(g);
				return;
			}

			g->mColorizeImages = true;
			g->mColor = Color(108, 109, 140, 40);
			theButton->DrawNormal(g);
			g->mColorizeImages = false;
		};
		DrawPageButton(mPrevPageButton);
		DrawPageButton(mNextPageButton);
		char aPageText[32];
		std::snprintf(aPageText, sizeof(aPageText), "Page %d / %d", mCurrentPage, mPageCount);
		TodDrawString(
			g,
			StringToSexyStringFast(aPageText),
			(mPrevPageButton->mX + mNextPageButton->mX) / 2 - 15,
			mPrevPageButton->mY + mPrevPageButton->mHeight / 2 + 50,
			FONT_BRIANNETOD12,
			Color::White,
			DS_ALIGN_CENTER);
	}
}

void ZombatarItems::DrawCategoryHighlight(Graphics* g)
{
	ZombatarImage aHighlight = ZombatarImage::None;
	ZombatarButton* aButton = nullptr;
	switch (mCurrentCategory)
	{
	case ZOMBATAR_CUSTOM_SKIN:
		aHighlight = ZombatarImage::SKIN_BUTTON_HIGHLIGHT;
		aButton = mSkinButton;
		break;
	case ZOMBATAR_CUSTOM_CLOTHES:
		aHighlight = ZombatarImage::CLOTHES_BUTTON_HIGHLIGHT;
		aButton = mClothesButton;
		break;
	case ZOMBATAR_CUSTOM_TIDBITS:
		aHighlight = ZombatarImage::TIDBITS_BUTTON_HIGHLIGHT;
		aButton = mTidbitsButton;
		break;
	case ZOMBATAR_CUSTOM_ACCESSORIES:
		aHighlight = ZombatarImage::ACCESSORY_BUTTON_HIGHLIGHT;
		aButton = mAccessoryButton;
		break;
	case ZOMBATAR_CUSTOM_FACIAL_HAIR:
		aHighlight = ZombatarImage::FACIAL_HAIR_BUTTON_HIGHLIGHT;
		aButton = mFacialHairButton;
		break;
	case ZOMBATAR_CUSTOM_HAIR:
		aHighlight = ZombatarImage::HAIR_BUTTON_HIGHLIGHT;
		aButton = mHairButton;
		break;
	case ZOMBATAR_CUSTOM_EYEWEAR:
		aHighlight = ZombatarImage::EYEWEAR_BUTTON_HIGHLIGHT;
		aButton = mEyeWearButton;
		break;
	case ZOMBATAR_CUSTOM_HATS:
		aHighlight = ZombatarImage::HATS_BUTTON_HIGHLIGHT;
		aButton = mHatsButton;
		break;
	case ZOMBATAR_CUSTOM_BACKDROPS:
		aHighlight = ZombatarImage::BACKDROPS_BUTTON_HIGHLIGHT;
		aButton = mBackdropsButton;
		break;
	default:
		break;
	}
	if (aHighlight != ZombatarImage::None && aButton)
		DrawZombatarImage(g, aHighlight, aButton->mX, aButton->mY);
}

void ZombatarItems::DrawConfirmation(Graphics* g)
{
	TodDrawString(
		g,
		gStr.ZOMBATAR_FINISHED_WARNING_HEADER,
		mWidth / 2,
		160,
		// 1073 loads FONT_HOUSEOFTERROR28 for the large warning header.
		FONT_HOUSEOFTERROR28,
		Color(254, 227, 0),
		DS_ALIGN_CENTER);
	TodDrawStringWrapped(
		g,
		gStr.ZOMBATAR_FINISHED_WARNING_TEXT,
		Rect(mWidth / 2 - 245, 200, 500, 100),
		// 1073 DAT_00726804 = FONT_CONTINUUMBOLD14 for the body.
		FONT_CONTINUUMBOLD14,
		Color::White,
		DS_ALIGN_CENTER_VERTICAL_MIDDLE);
	TodDrawString(
		g,
		gStr.ZOMBATAR_FINISHED_BUTTON_TEXT,
		mFinishedButton->mX + mFinishedButton->mWidth / 2,
		mFinishedButton->mY - 10,
		FONT_BRIANNETOD12,
		Color::White,
		DS_ALIGN_CENTER);
	TodDrawString(
		g,
		gStr.ZOMBATAR_BACK_BUTTON_TEXT,
		mBackButton->mX + mBackButton->mWidth / 2,
		mBackButton->mY - 10,
		FONT_BRIANNETOD12,
		Color::White,
		DS_ALIGN_CENTER);
	mBackButton->Draw(g);
	mFinishedButton->Draw(g);
}

void ZombatarItems::DrawSavedZombatars(Graphics* g)
{
	PlayerInfo* aPlayer = mApp ? mApp->mPlayerInfo : nullptr;
	if (!aPlayer || aPlayer->mNumZombatars <= 0)
		return;

	mSelectedZombatar = std::clamp(mSelectedZombatar, 0, aPlayer->mNumZombatars - 1);
	const Zombatar& aCurrent = aPlayer->mZombatars[mSelectedZombatar];
	// FUN_0052FE90 uses the fixed blank-background canvas for the three
	// adjacent portraits.  Using the selected backdrop's width makes the
	// neighbours drift when records contain different backgrounds.
	const int aPortraitWidth = gZombatarAtlas.mImages[static_cast<size_t>(ZombatarImage::BACKGROUND_BLANK)].mWidth;
	const int aCurrentX = (mWidth - 30) / 2 + mPortraitGap / 2 - aPortraitWidth / 2;
	const int aPortraitY = 150;

	{
		// 1073 0052EB10 clips to height-163, then 0052EB2C fills only
		// height-269. These are different rectangles; the shipped black
		// artwork must not substitute for the explicit panel fill.
		// Restore the caller's clip before drawing controls and labels.
		GraphicsAutoState aPortraitPanelState(g);
		g->ClipRect(33, 100, mWidth - 63, mHeight - 0xA3);
		g->mColor = Color::Black;
		g->FillRect(33, 100, mWidth - 63, mHeight - 0x10D);

		DrawZombatarPortrait(g, aCurrent, aCurrentX, aPortraitY);
		if (mSelectedZombatar + 1 < aPlayer->mNumZombatars)
		{
			DrawZombatarPortrait(
				g,
				aPlayer->mZombatars[mSelectedZombatar + 1],
				aCurrentX + mPortraitGap + aPortraitWidth,
				aPortraitY);
		}
		if (mSelectedZombatar > 0)
		{
			DrawZombatarPortrait(
				g,
				aPlayer->mZombatars[mSelectedZombatar - 1],
				aCurrentX - mPortraitGap - aPortraitWidth,
				aPortraitY);
		}
	}

	if (!mPrevZombieButton->mDisabled)
		mPrevZombieButton->Draw(g);
	if (!mNextZombieButton->mDisabled)
		mNextZombieButton->Draw(g);
	if (!mNewZombieButton->mDisabled)
		mNewZombieButton->Draw(g);

	char aCounter[32];
	std::snprintf(
		aCounter,
		sizeof(aCounter),
		"%d / %d",
		mSelectedZombatar + 1,
		aPlayer->mNumZombatars);
	// 1073 0052ECF9 uses the editor's page arrows (a4/a8), not the
	// portrait-browsing arrows (dc/e0), and DrawString is left-aligned.
	const int aCounterX = mPrevPageButton->mX +
		(mNextPageButton->mX - mPrevPageButton->mX) / 2 - 115;
	const int aCounterY = mPrevPageButton->mY + mPrevPageButton->mHeight / 2 - 230;
	g->mFont = FONT_BRIANNETOD12;
	g->mColor = Color::White;
	g->DrawString(StringToSexyStringFast(aCounter), aCounterX, aCounterY);
	g->mFont = FONT_DWARVENTODCRAFT12;
	g->mColor = mDeleteHovered ? Color(22, 253, 5) : Color::White;
	g->DrawString(("Delete?"), aCounterX + 130, aCounterY);
}

int ZombatarItems::HitTestItem(int x, int y) const
{
	if (mState != ZombatarEditorState::EDITING || mItemCount <= 0)
		return -1;

	const int aFirstIndex = (mCurrentPage - 1) * mItemsPerPage;
	const int anEndIndex = std::min(mItemCount, aFirstIndex + mItemsPerPage);
	for (int anIndex = aFirstIndex; anIndex < anEndIndex; ++anIndex)
	{
		const ZombatarItemCell& aCell = mItemCells[anIndex];
		const Rect aHitRect(
			aCell.mX + mCellPadding,
			aCell.mY + mCellPadding,
			gZombatarAtlas.mImages[static_cast<size_t>(ZombatarImage::ACCESSORY_BG)].mWidth - mCellPadding * 2,
			gZombatarAtlas.mImages[static_cast<size_t>(ZombatarImage::ACCESSORY_BG)].mHeight - mCellPadding * 2);
		if (aHitRect.Contains(x, y))
			return anIndex;
	}
	return -1;
}

void ZombatarItems::MouseMove(int x, int y)
{
	UpdateViewButtonLayout();
	ZombatarButton* aHoveredButton = HitTestButton(x, y);
	if (mState == ZombatarEditorState::EDITING)
	{
		mHoveredItem = HitTestItem(x, y);
		mDeleteHovered = false;
	}
	else if (mState == ZombatarEditorState::BROWSING)
	{
		mHoveredItem = -1;
		mDeleteHovered = mDeleteRect.Contains(x, y);
	}
	else
	{
		mHoveredItem = -1;
		mDeleteHovered = false;
	}
	if (mApp)
	{
		const bool aClickable = aHoveredButton != nullptr ||
			(mState == ZombatarEditorState::EDITING && mHoveredItem >= 0) ||
			(mState == ZombatarEditorState::BROWSING && mDeleteHovered);
		mApp->SetCursor(aClickable ? CURSOR_HAND : CURSOR_POINTER);
	}
	MarkDirty();
}

void ZombatarItems::MouseLeave()
{
	mHoveredItem = -1;
	mDeleteHovered = false;
	mPressedButtonId = -1;
	if (mApp)
		mApp->SetCursor(CURSOR_POINTER);
	MarkDirty();
}

ZombatarButton* ZombatarItems::FindPressedButton() const
{
	for (ZombatarButton* aButton : GetAllButtons(const_cast<ZombatarItems*>(this)))
	{
		if (CanPress(aButton))
			return aButton;
	}
	return nullptr;
}

ZombatarButton* ZombatarItems::HitTestButton(int x, int y) const
{
	for (ZombatarButton* aButton : GetAllButtons(const_cast<ZombatarItems*>(this)))
	{
		if (aButton == nullptr || aButton->mDisabled || aButton->mBtnNoDraw)
			continue;
		if (Rect(aButton->mX, aButton->mY, aButton->mWidth, aButton->mHeight).Contains(x, y))
			return aButton;
	}
	return nullptr;
}

void ZombatarItems::MouseDown(int x, int y, int theClickCount)
{
	// Let Widget translate the legacy click-count convention (1/-1/2) to the
	// explicit button-number overload below.  In particular, a right click
	// must never activate a Zombatar control.
	Widget::MouseDown(x, y, theClickCount);
}

void ZombatarItems::MouseDown(int x, int y, int theBtnNum, int theClickCount)
{
	mPressedButtonId = -1;
	if (theBtnNum == 0 && theClickCount > 0)
	{
		if (ZombatarButton* aButton = HitTestButton(x, y))
			mPressedButtonId = aButton->mId;
	}
	Widget::MouseDown(x, y, theBtnNum, theClickCount);
}

void ZombatarItems::MouseUp(int x, int y, int theLastDownButtonId)
{
	// Widget converts the stored button id back to the explicit overload.  Do
	// not interpret it as a click count: -1 is a right-button release.
	Widget::MouseUp(x, y, theLastDownButtonId);
}

void ZombatarItems::MouseUp(int x, int y, int theBtnNum, int theClickCount)
{
	if (theBtnNum != 0)
	{
		mPressedButtonId = -1;
		return;
	}

	const int aPressedButtonId = mPressedButtonId;
	mPressedButtonId = -1;
	if (aPressedButtonId >= 0)
	{
		for (ZombatarButton* aButton : GetAllButtons(this))
		{
			if (aButton == nullptr || aButton->mId != aPressedButtonId ||
				aButton->mDisabled || aButton->mBtnNoDraw)
				continue;
			if (Rect(aButton->mX, aButton->mY, aButton->mWidth, aButton->mHeight).Contains(x, y))
			{
				ButtonDepress(aPressedButtonId);
			}
			// A press that began on a button is consumed even if the release
			// moved outside it.  This is the normal ButtonWidget semantic and
			// prevents a drag-release from selecting a grid cell underneath.
			return;
		}
	}

	if (mState == ZombatarEditorState::EDITING)
	{
		const int anItemIndex = HitTestItem(x, y);
		if (anItemIndex >= 0)
		{
			mPressedItem = anItemIndex;
			mZombatar.mItems[mCurrentCategory] =
				mItemCells[anItemIndex].mItem
					? mItemCells[anItemIndex].mRawIndex
					: -1;
			SyncColorMode();
			mSelectionChanged = true;
			MarkDirty();
		}
	}
	else if (mState == ZombatarEditorState::BROWSING &&
		mDeleteRect.Contains(x, y))
	{
		PlayerInfo* aPlayer = mApp ? mApp->mPlayerInfo : nullptr;
		if (!aPlayer || aPlayer->mNumZombatars <= 0)
			return;

        GameSelector* aOwner = mApp->mGameSelector;
        if (!mWidgetManager || !aOwner || !aOwner->mZombatarScreen ||
            aOwner->mZombatarScreen->mZombatarItems != this ||
            mSelectedZombatar < 0 || mSelectedZombatar >= aPlayer->mNumZombatars)
            return;
        mApp->KillDialog(DIALOG_DELETEZOMBATAR);
        auto aDialog = std::make_unique<ZombatarDeleteDialog>(*mApp, *aOwner, *this, *aPlayer);
        mApp->CenterDialog(aDialog.get(), aDialog->mWidth, aDialog->mHeight);
        mApp->AddDialog(DIALOG_DELETEZOMBATAR, aDialog.get());
        if (!mApp->mWidgetManager->mFocusWidget)
            mApp->mWidgetManager->SetFocus(aDialog.get());
        aDialog.release(); // The application's dialog registry owns it now.
	}
	(void)theClickCount;
}

void ZombatarItems::ButtonDepress(int theId)
{
    CancelDeleteDialog();
	switch (theId)
	{
	case BUTTON_NEXT_PAGE:
		mCurrentPage = std::min(mCurrentPage + 1, mPageCount);
		mHoveredItem = -1;
		mPressedItem = -1;
		LayoutCategoryItems();
		break;
	case BUTTON_PREV_PAGE:
		mCurrentPage = std::max(mCurrentPage - 1, 1);
		mHoveredItem = -1;
		mPressedItem = -1;
		LayoutCategoryItems();
		break;
	case BUTTON_BACK:
		if (mState == ZombatarEditorState::CONFIRMATION)
		{
			mState = ZombatarEditorState::LEAVING_CONFIRMATION;
			mTransitionCounter = 180;
			RefreshState();
		}
		break;
	case BUTTON_FINISHED:
		if (mState == ZombatarEditorState::EDITING)
		{
			MoveButton(mFinishedButton, 420, 447);
			mState = ZombatarEditorState::ENTERING_CONFIRMATION;
			mTransitionCounter = 180;
			RefreshState();
		}
		else if (mState == ZombatarEditorState::CONFIRMATION)
		{
			SaveCurrentZombatar();
			mState = ZombatarEditorState::BROWSING;
			RefreshState();
		}
		break;
	case BUTTON_NEW_ZOMBIE:
		if (mState == ZombatarEditorState::BROWSING)
		{
			ResetZombatar();
			mState = ZombatarEditorState::EDITING;
			ResetTransientLayout();
			RefreshState();
		}
		break;
	case BUTTON_NEXT_ZOMBIE:
		if (mApp && mApp->mPlayerInfo)
			mSelectedZombatar = std::min(
				mSelectedZombatar + 1,
				mApp->mPlayerInfo->mNumZombatars - 1);
		RefreshState();
		break;
	case BUTTON_PREV_ZOMBIE:
		mSelectedZombatar = std::max(mSelectedZombatar - 1, 0);
		RefreshState();
		break;
	case BUTTON_MAIN_MENU:
		ReturnToMainMenu();
		break;
	case BUTTON_VIEW:
		if (mApp && mApp->mPlayerInfo && mApp->mPlayerInfo->mNumZombatars > 0)
		{
			mState = ZombatarEditorState::BROWSING;
			mSelectedZombatar = 0;
			RefreshState();
		}
		break;
	case BUTTON_SKIN:
		SetCategory(ZOMBATAR_CUSTOM_SKIN);
		break;
	case BUTTON_HAIR:
		SetCategory(ZOMBATAR_CUSTOM_HAIR);
		break;
	case BUTTON_FACIAL_HAIR:
		SetCategory(ZOMBATAR_CUSTOM_FACIAL_HAIR);
		break;
	case BUTTON_TIDBITS:
		SetCategory(ZOMBATAR_CUSTOM_TIDBITS);
		break;
	case BUTTON_EYEWEAR:
		SetCategory(ZOMBATAR_CUSTOM_EYEWEAR);
		break;
	case BUTTON_CLOTHES:
		SetCategory(ZOMBATAR_CUSTOM_CLOTHES);
		break;
	case BUTTON_ACCESSORIES:
		SetCategory(ZOMBATAR_CUSTOM_ACCESSORIES);
		break;
	case BUTTON_HATS:
		SetCategory(ZOMBATAR_CUSTOM_HATS);
		break;
	case BUTTON_BACKDROPS:
		SetCategory(ZOMBATAR_CUSTOM_BACKDROPS);
		break;
	default:
		break;
	}
	MarkDirty();
}

void ZombatarItems::CancelDeleteDialog()
{
    if (!mApp || !mWidgetManager)
        return;
    auto* aDialog = dynamic_cast<ZombatarDeleteDialog*>(mApp->GetDialog(DIALOG_DELETEZOMBATAR));
    if (aDialog && aDialog->IsFor(*this))
        mApp->SexyAppBase::KillDialog(aDialog);
}

void ZombatarItems::FinishDeleteDialog(LawnApp& theApp)
{
    auto* aDialog = dynamic_cast<ZombatarDeleteDialog*>(theApp.GetDialog(DIALOG_DELETEZOMBATAR));
    if (aDialog)
        aDialog->Confirm();
}

void ZombatarItems::SaveCurrentZombatar()
{
	PlayerInfo* aPlayer = mApp ? mApp->mPlayerInfo : nullptr;
	if (!aPlayer || aPlayer->mNumZombatars >= MAX_NUM_ZOMBATARS)
		return;

	aPlayer->mZombatars[aPlayer->mNumZombatars] = mZombatar;
	++aPlayer->mNumZombatars;
	mSelectedZombatar = aPlayer->mNumZombatars - 1;
	aPlayer->SaveDetails();
	ExportZombatarJpeg(
		aPlayer->mZombatars[mSelectedZombatar],
		aPlayer->mNumZombatars,
		mApp);
}

void ZombatarItems::ReturnToMainMenu()
{
	ResetZombatar();
	if (mApp && mApp->mGameSelector)
	{
		mApp->mGameSelector->HideZombatarScreen();
	}
}

bool ZombatarItems::HasVisualSelectionChanged() const
{
	return mSelectionChangedLastFrame ||
		(mColors && mColors->mSelectionChangedLastFrame);
}

const void* ZombatarItems::TouchHitTest(int x, int y)
{
    if (auto* button = HitTestButton(x, y)) return button;
    return this;
}
