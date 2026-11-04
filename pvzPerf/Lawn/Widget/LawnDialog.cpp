#include "Sexy.TodLib/generated/EntityStrings.h"
#include "Lawn/Board.h"
#include "LawnDialog.h"
#include "ZombiesWonLayout.h"
#include "GameButton.h"
#include "LawnApp.h"
#include "Lawn/LawnCommon.h"
#include "Resources.h"
#include "ChallengeScreen.h"
#include "Sexy.TodLib/TodDebug.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "graphics/ImageFont.h"
#include "graphics/AtlasDraw.h"
#include "Sexy.TodLib/generated/SceneAtlasData.h"
#include <chrono>

namespace { using enum CommonUIAtlas::Sprite; }

//0x456A80
LawnDialog::LawnDialog(LawnApp* theApp, int theId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode) :
	Dialog(nullptr, nullptr, theId, isModal, theDialogHeader, theDialogLines, (""), BUTTONS_NONE)
{
    mApp = theApp;
    mButtonDelay = -1;
    mReanimation = new ReanimationWidget();
    mReanimation->mLawnDialog = this;
    mDrawStandardBack = true;
    mTallBottom = false;
    mVerticalCenterText = true;
    mDialogHeader = theDialogHeader;
    mDialogLines = theDialogLines;
    SetColor(0, { 0xE0,0xBB,0x62 });
    SetColor(1, { 0xE0,0xBB,0x62 });
    SetHeaderFont(Sexy::FONT_DWARVENTODCRAFT24);
    SetLinesFont(Sexy::FONT_DWARVENTODCRAFT15);
    mContentInsets = Insets(36, 35, 46, 36);

    // @Patoke: these dialogs had the wrong local name
    // @Patoke: these also don't use localized names anymore
    if (theButtonMode == 1)
    {
        mYesButton = MakeButton(1000, this, ("Yes"));
        mNoButton = MakeButton(1001, this, ("No"));
    }
    else if (theButtonMode == 2)
    {
        mYesButton = MakeButton(1000, this, ("Ok"));
        mNoButton = MakeButton(1001, this, ("Cancel"));
    }
    else if (theButtonMode == 3)
    {
        mYesButton = MakeButton(1000, this, theDialogFooter);
        mNoButton = nullptr;
    }
    else
    {
        mYesButton = nullptr;
        mNoButton = nullptr;
    }

    mApp->SetCursor(CURSOR_POINTER);
    CalcSize(0, 0);
}

//0x456E80 & 0x456EA0
LawnDialog::~LawnDialog()
{
    if (mReanimation) delete mReanimation;
}

//0x456F30
void LawnDialog::CalcSize(int theExtraX, int theExtraY)
{
    // 根据边距计算一个最小宽度
    int aWidth = mBackgroundInsets.mLeft + mBackgroundInsets.mRight + mContentInsets.mLeft + mContentInsets.mRight + theExtraX;
    // 最小宽度需要额外考虑到标题字符串的宽度
    if (mDialogHeader.size() > 0) aWidth += mHeaderFont->StringWidth(mDialogHeader);
    // 对话框贴图的最小宽度
    int aTopMidWidth = CommonUIAtlas::Images[size_t(DIALOG_TOPMIDDLE)].mWidth;
    int aImageWidth = CommonUIAtlas::Images[size_t(DIALOG_TOPLEFT)].mWidth + CommonUIAtlas::Images[size_t(DIALOG_TOPRIGHT)].mWidth + aTopMidWidth;
    // 计算最终的宽度
    if (aWidth <= aImageWidth)
    {
        aWidth = aImageWidth;
    }
    else if (aTopMidWidth > 0)
    {
        int anExtraWidth = (aWidth - aImageWidth) % aTopMidWidth;
        if (anExtraWidth)
        {
            aWidth += aTopMidWidth - anExtraWidth;
        }
    }

    // 根据边距计算一个最小高度
    int aHeight = mBackgroundInsets.mTop + mBackgroundInsets.mBottom + mContentInsets.mTop + mContentInsets.mBottom + theExtraY + DIALOG_HEADER_OFFSET;
    // 额外考虑到标题字符串的高度
    if (mDialogHeader.size() > 0)
    {
        aHeight += -mHeaderFont->mAscentPadding + mHeaderFont->mHeight + mSpaceAfterHeader;
    }
    // 对话框宽度和高度额外考虑正文文本
    if (mDialogLines.size() > 0)
    {
        aWidth += aTopMidWidth;
        int aBasicWidth = aWidth - mBackgroundInsets.mLeft - mBackgroundInsets.mRight - mContentInsets.mLeft - mContentInsets.mRight - 4;
        aHeight += Graphics::MeasureWordWrappedHeight(*mLinesFont,aBasicWidth,mDialogLines,
            mLinesFont->GetLineSpacing()+mLineSpacingOffset)+30;
    }
    aHeight += mButtonHeight;
    // 对话框贴图的最小高度
    int aBottomHeight = CommonUIAtlas::Images[size_t(mTallBottom ? DIALOG_BIGBOTTOMLEFT : DIALOG_BOTTOMLEFT)].mHeight;
    int aImageHeight = CommonUIAtlas::Images[size_t(DIALOG_TOPLEFT)].mHeight + aBottomHeight + DIALOG_HEADER_OFFSET;
    // 计算最终的高度
    if (aHeight < aImageHeight)
    {
        aHeight = aImageHeight;
    }
    else
    {
        int aCenterHeight = CommonUIAtlas::Images[size_t(DIALOG_CENTERLEFT)].mHeight;
        int anExtraHeight = (aHeight - aImageHeight) % aCenterHeight;
        if (anExtraHeight) aHeight += aCenterHeight - anExtraHeight;
    }

    Resize(mX, mY, aWidth, aHeight);
}

//0x457160
void LawnDialog::AddedToManager(WidgetManager* theWidgetManager)
{
    Dialog::AddedToManager(theWidgetManager);
    AddWidget(mReanimation);
    if (mYesButton) AddWidget(mYesButton);
    if (mNoButton) AddWidget(mNoButton);
}

//0x4571B0
void LawnDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
    Dialog::RemovedFromManager(theWidgetManager);
    if (mYesButton) RemoveWidget(mYesButton);
    if (mNoButton) RemoveWidget(mNoButton);
    RemoveWidget(mReanimation);

    if (mReanimation->mReanim)
    {
        mApp->mEffectSystem->mReanimationHolder->mReanimations.DataArrayFree(mReanimation->mReanim);
        mReanimation->mReanim = nullptr;
    }
}

//0x457230
void LawnDialog::SetButtonDelay(int theDelay)
{
    mButtonDelay = theDelay;
    if (mYesButton) mYesButton->SetDisabled(true);
    if (mNoButton) mNoButton->SetDisabled(true);
}

//0x457280
void LawnDialog::Update()
{
    Dialog::Update();
    // 1073 0x45ABA0 compares widget age with the delay threshold.
    if (mButtonDelay >= 0 && mUpdateCnt == static_cast<std::uint32_t>(mButtonDelay))
    {
        if (mYesButton) mYesButton->SetDisabled(false);
        if (mNoButton) mNoButton->SetDisabled(false);
    }
    MarkDirty();
}

//0x4572E0
void LawnDialog::ButtonPress(int theId)
{
    (void)theId;
    mApp->PlaySample(Sexy::SOUND_GRAVEBUTTON);
}

//0x457300
void LawnDialog::ButtonDepress(int theId)
{
    if (mButtonDelay < 0 || mUpdateCnt > static_cast<std::uint32_t>(mButtonDelay))
    {
        Dialog::ButtonDepress(theId);
    }
}

//0x457320
void LawnDialog::CheckboxChecked()
{
    mApp->PlaySample(Sexy::SOUND_BUTTONCLICK);
}

//0x457340
void LawnDialog::KeyDown(KeyCode theKey)
{
    if (mId == DIALOG_PAUSED && mApp->mBoard)
    {
        mApp->mBoard->DoTypingCheck(theKey);
    }

    if (mId != DIALOG_ALMANAC)
    {
        if (theKey == KEYCODE_SPACE || theKey == KEYCODE_RETURN || theKey == 'y' || theKey == 'Y')
        {
            Dialog::ButtonDepress(Dialog::ID_YES);
        }
        else if ((theKey == KEYCODE_ESCAPE || theKey == 'n' || theKey == 'N') && mNoButton)
        {
            Dialog::ButtonDepress(Dialog::ID_NO);
        }
    }
}

//0x4573D0
void LawnDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
    Dialog::Resize(theX, theY, theWidth, theHeight);

    int aButtonAreaX = mContentInsets.mLeft + mBackgroundInsets.mLeft - 5;
    int aButtonAreaY = mHeight - mContentInsets.mBottom - mBackgroundInsets.mBottom - CommonUIAtlas::Images[size_t(BUTTON_LEFT)].mHeight + 2;
    int aButtonAreaWidth = mWidth - mContentInsets.mRight - mBackgroundInsets.mRight - mBackgroundInsets.mLeft - mContentInsets.mLeft + 8;
    // 按钮贴图限制下，按钮的最小宽度
    int aButtonMinWidth = CommonUIAtlas::Images[size_t(BUTTON_LEFT)].mWidth + CommonUIAtlas::Images[size_t(BUTTON_RIGHT)].mWidth;
    // 按钮区域额外空余的宽度
    int aBtnMidWidth = CommonUIAtlas::Images[size_t(BUTTON_MIDDLE)].mWidth;
    int aButtonExtraWidth = (aButtonAreaWidth - 10) / 2 - aBtnMidWidth - aButtonMinWidth + 1;
    // 计算按钮可以拓展的额外宽度
    if (aButtonExtraWidth <= 0)
    {
        aButtonExtraWidth = 0;
    }
    else if (aBtnMidWidth > 0)
    {
        int anExtraWidth = aButtonExtraWidth % aBtnMidWidth;
        if (anExtraWidth)
        {
            // 不足中部贴图宽度的部分补充至中部贴图宽度
            aButtonExtraWidth += aBtnMidWidth - anExtraWidth;
        }
    }
    // 最终宽度
    int aButtonWidth = aButtonMinWidth + aButtonExtraWidth;

    if (mTallBottom)
    {
        aButtonAreaY += 5;
    }

    if (mYesButton && mNoButton)
    {
        mYesButton->Resize(aButtonAreaX, aButtonAreaY, aButtonWidth, CommonUIAtlas::Images[size_t(BUTTON_LEFT)].mHeight);
        mNoButton->Resize(aButtonAreaWidth - aButtonWidth + aButtonAreaX, aButtonAreaY, aButtonWidth, CommonUIAtlas::Images[size_t(BUTTON_LEFT)].mHeight);
    }
    else if (mYesButton)
    {
        aButtonExtraWidth = aButtonAreaWidth - aBtnMidWidth - aButtonMinWidth + 1;
        if (aButtonExtraWidth <= 0)
        {
            aButtonExtraWidth = 0;
        }
        else if (aBtnMidWidth > 0)
        {
            int anExtraWidth = aButtonExtraWidth % aBtnMidWidth;
            if (anExtraWidth)
            {
                aButtonExtraWidth += aBtnMidWidth - anExtraWidth;
            }
        }
        int aButtonWidth = aButtonMinWidth + aButtonExtraWidth;

        mYesButton->Resize(aButtonAreaX + (aButtonAreaWidth - aButtonWidth) / 2, aButtonAreaY, aButtonWidth, CommonUIAtlas::Images[size_t(BUTTON_LEFT)].mHeight);
    }

    if (mReanimation->mReanim)
    {
        mReanimation->Resize(mReanimation->mPosX, mReanimation->mPosY + DIALOG_HEADER_OFFSET, mReanimation->mWidth, mReanimation->mHeight);
    }
}

//0x4575A0
void LawnDialog::Draw(Graphics* g)
{
    if (!mDrawStandardBack)
        return;

    const TextureRegion& bottomLeft = CommonUIAtlas::Images[size_t(mTallBottom ? DIALOG_BIGBOTTOMLEFT : DIALOG_BOTTOMLEFT)];
    const TextureRegion& bottomMiddle = CommonUIAtlas::Images[size_t(mTallBottom ? DIALOG_BIGBOTTOMMIDDLE : DIALOG_BOTTOMMIDDLE)];
    const TextureRegion& bottomRight = CommonUIAtlas::Images[size_t(mTallBottom ? DIALOG_BIGBOTTOMRIGHT : DIALOG_BOTTOMRIGHT)];
    const TextureRegion& topLeft = CommonUIAtlas::Images[size_t(DIALOG_TOPLEFT)];
    const TextureRegion& topMiddle = CommonUIAtlas::Images[size_t(DIALOG_TOPMIDDLE)];
    const TextureRegion& topRight = CommonUIAtlas::Images[size_t(DIALOG_TOPRIGHT)];
    const TextureRegion& centerLeft = CommonUIAtlas::Images[size_t(DIALOG_CENTERLEFT)];
    const TextureRegion& centerMiddle = CommonUIAtlas::Images[size_t(DIALOG_CENTERMIDDLE)];
    const TextureRegion& centerRight = CommonUIAtlas::Images[size_t(DIALOG_CENTERRIGHT)];
    const TextureRegion& header = CommonUIAtlas::Images[size_t(DIALOG_HEADER)];
    const Sexy::TextureSource texture{CommonUIAtlas::Texture, 0};

    int aRepeatX = (mWidth - topRight.mWidth - topLeft.mWidth) / topMiddle.mWidth;
    int aRepeatY = (mHeight - topLeft.mHeight - bottomLeft.mHeight - DIALOG_HEADER_OFFSET) / centerLeft.mHeight;

    int aPosX = 0;
    int aPosY = DIALOG_HEADER_OFFSET;
    Sexy::DrawAtlasImage(g, texture, topLeft, aPosX, aPosY);
    aPosX += topLeft.mWidth;
    for (int i = 0; i < aRepeatX; i++)
    {
        Sexy::DrawAtlasImage(g, texture, topMiddle, aPosX, aPosY);
        aPosX += topMiddle.mWidth;
    }
    Sexy::DrawAtlasImage(g, texture, topRight, aPosX, aPosY);

    aPosY += topRight.mHeight;
    for (int y = 0; y < aRepeatY; y++)
    {
        aPosX = 0;

        Sexy::DrawAtlasImage(g, texture, centerLeft, aPosX, aPosY);
        aPosX += centerLeft.mWidth;

        for (int x = 0; x < aRepeatX; x++)
        {
            Sexy::DrawAtlasImage(g, texture, centerMiddle, aPosX, aPosY);
            aPosX += centerMiddle.mWidth;
        }

        Sexy::DrawAtlasImage(g, texture, centerRight, aPosX, aPosY);
        aPosY += centerLeft.mHeight;
    }

    aPosX = 0;
    Sexy::DrawAtlasImage(g, texture, bottomLeft, aPosX, aPosY);
    aPosX += bottomLeft.mWidth;
    for (int i = 0; i < aRepeatX; i++)
    {
        Sexy::DrawAtlasImage(g, texture, bottomMiddle, aPosX, aPosY);
        aPosX += bottomMiddle.mWidth;
    }
    Sexy::DrawAtlasImage(g, texture, bottomRight, aPosX, aPosY);
    Sexy::DrawAtlasImage(g, texture, header, (mWidth - header.mWidth) / 2 - 5, 0);

    int aFontY = mContentInsets.mTop + mBackgroundInsets.mTop + DIALOG_HEADER_OFFSET;
    if (mDialogHeader.size() > 0)
    {
        int aOffsetY = aFontY - mHeaderFont->mAscentPadding + mHeaderFont->mAscent;
        g->mFont = mHeaderFont;
        g->mColor = mColors[Dialog::COLOR_HEADER];
        WriteCenteredLine(g, aOffsetY, mDialogHeader);
        aFontY = aOffsetY - mHeaderFont->mAscent + mHeaderFont->mHeight + mSpaceAfterHeader;
    }

    g->mFont = mLinesFont;
    g->mColor = mColors[Dialog::COLOR_LINES];
    int aLinesAreaWidth = mWidth - mContentInsets.mLeft - mContentInsets.mRight - mBackgroundInsets.mLeft - mBackgroundInsets.mRight - 4;
    Rect aRect(mBackgroundInsets.mLeft + mContentInsets.mLeft + 2, aFontY, aLinesAreaWidth, 0);
    if (mVerticalCenterText)
    {
        int aLinesHeight = Graphics::MeasureWordWrappedHeight(*mLinesFont, aLinesAreaWidth,
            mDialogLines, mLinesFont->GetLineSpacing() + mLineSpacingOffset, nullptr, mWriteColoredString);
        int aLinesAreaHeight = mHeight - mContentInsets.mBottom - mBackgroundInsets.mBottom - mButtonHeight - aFontY - 55;
        if (mTallBottom)
        {
            aLinesAreaHeight -= 36;
        }
        aRect.mY += (aLinesAreaHeight - aLinesHeight) / 2;
    }
    WriteWordWrapped(g, aRect, mDialogLines, mLinesFont->GetLineSpacing() + mLineSpacingOffset, mTextAlign);
}

//0x4579A0
ReanimationWidget::ReanimationWidget()
{
    mApp = (LawnApp*)gSexyAppBase;
    mPosX = 0.0f;
    mPosY = 0.0f;
    mReanim = nullptr;
    mMouseVisible = false;
    mClip = false;
    mLawnDialog = nullptr;
    mHasAlpha = true;
}

//0x4579F0、0x457A10
ReanimationWidget::~ReanimationWidget()
{
    Dispose();
}

void ReanimationWidget::Dispose()
{
    if (mReanim)
    {
        mApp->mEffectSystem->mReanimationHolder->mReanimations.DataArrayFree(mReanim);
        mReanim = nullptr;
    }
}

//0x457A80
void ReanimationWidget::AddReanimation(float x, float y, ReanimationType theReanimationType)
{
    TOD_ASSERT(mReanim == nullptr);
    
    mPosX = x;
    mPosY = y;
    const auto aNowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    mReanim = mApp->mEffectSystem->mReanimationHolder->AllocReanimation(x, y, 0, theReanimationType, aNowNs);
    mReanim->ChangeLoopType(ReanimLoopType::REANIM_LOOP);
    mReanim->mIsAttachment = true;
    if (mReanim->TrackExists(ReanimTrackId::anim_idle))
        mReanim->SetFramesForLayer(ReanimTrackId::anim_idle);

    Resize(x, y, 10, 10);
}

//0x457B80
void ReanimationWidget::Draw(Graphics* g)
{
    if (mReanim)
        mReanim->Draw(g);
}

//0x457BA0
void ReanimationWidget::Update()
{
    if (mReanim)
    {
        // The pause dialog owns this animation. Both game clocks may be frozen.
        const auto aNowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        mReanim->UpdateAt(aNowNs);
        MarkDirty();
    }
}

//0x457BC0
GameOverDialog::GameOverDialog(const SexyString& theMessage, bool theShowChallengeName) : LawnDialog(
    gLawnApp, 
    Dialogs::DIALOG_GAME_OVER, 
    true, 
    gStr.GAME_OVER, 
    theMessage,
    (""), 
    Dialog::BUTTONS_FOOTER)
{
    mMenuButton = nullptr;
    mYesButton->mLabel = gStr.TRY_AGAIN_;
    if (theShowChallengeName)
    {
        mDialogHeader = mApp->GetCurrentChallengeDef().mChallengeName;
    }

    if (theMessage.size() == 0)
    {
        mContentInsets.mTop += 15;
    }
    CalcSize(0, 0);
    const Rect& aView = mApp->mScreenBounds;
    Resize(ZombiesWonLayout::DialogX(aView.mX, aView.mWidth, mWidth),
        aView.mY + (aView.mHeight - mHeight) / 2, mWidth, mHeight);
    mClip = false;

    mMenuButton = MakeButton(1, this, gStr.MAIN_MENU_BUTTON);
    mMenuButton->Resize(ZombiesWonLayout::MenuX(aView.mX, aView.mWidth, mX),
        aView.mY - 10 - mY, 163, 46);

    gLawnApp->mBoard->mShowShovel = false;
    gLawnApp->mBoard->mMenuButton->mBtnNoDraw = true;
}

GameOverDialog::~GameOverDialog()
{
    delete mMenuButton;
}

//0x457E50
void GameOverDialog::ButtonDepress(int theId)
{
    if (theId == 1)
    {
        mApp->KillDialog(Dialogs::DIALOG_GAME_OVER);
        mApp->KillBoard();
        if (mApp->IsSurvivalMode())
        {
            mApp->ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_SURVIVAL);
        }
        else if (mApp->IsPuzzleMode())
        {
            mApp->ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_PUZZLE);
        }
        else if (mApp->IsAdventureMode())
        {
            mApp->ShowGameSelector();
        }
        else
        {
            mApp->ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_CHALLENGE);
        }
    }
    else if (theId == Dialog::ID_FOOTER)
    {
        mApp->KillDialog(Dialogs::DIALOG_GAME_OVER);
        mApp->EndLevel();
    }
}

//0x457F20
void GameOverDialog::AddedToManager(WidgetManager* theWidgetManager)
{
    LawnDialog::AddedToManager(theWidgetManager);
    if (mMenuButton)
    {
        // Attachment may follow a viewport resize after construction.
        const Rect& aView = mApp->mScreenBounds;
        Resize(ZombiesWonLayout::DialogX(aView.mX, aView.mWidth, mWidth),
            aView.mY + (aView.mHeight - mHeight) / 2, mWidth, mHeight);
        AddWidget(mMenuButton);
    }
}

//0x457F80
void GameOverDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
    LawnDialog::RemovedFromManager(theWidgetManager);
    if (mMenuButton)
    {
        RemoveWidget(mMenuButton);
    }
}

//0x457FB0
void GameOverDialog::MouseDrag(int x, int y)
{
    LawnDialog::MouseDrag(x, y);
}

void GameOverDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
    LawnDialog::Resize(theX, theY, theWidth, theHeight);
    if (mMenuButton)
    {
        const Rect& aView = mApp->mScreenBounds;
        mMenuButton->Resize(ZombiesWonLayout::MenuX(aView.mX, aView.mWidth, mX),
            aView.mY - 10 - mY, 163, 46);
    }
}

void GameOverDialog::Update()
{
    LawnDialog::Update();
    const Rect& aView = mApp->mScreenBounds;
    // The pinned menu button retains the previous viewport edge. Recenter
    // after a viewport change while preserving ordinary dialog dragging.
    if (mMenuButton && (mX + mMenuButton->mX != aView.mX + aView.mWidth - 165 ||
        mY + mMenuButton->mY != aView.mY - 10))
    {
        Resize(ZombiesWonLayout::DialogX(aView.mX, aView.mWidth, mWidth),
            aView.mY + (aView.mHeight - mHeight) / 2, mWidth, mHeight);
    }
}
