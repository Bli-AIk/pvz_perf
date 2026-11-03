#include "Sexy.TodLib/generated/EntityStrings.h"
#include "GameButton.h"
#include "LawnApp.h"
#include "Resources.h"
#include "Sexy.TodLib/TodCommon.h"
#include "graphics/Font.h"
#include "graphics/Graphics.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "widget/WidgetManager.h"
#include "graphics/AtlasDraw.h"
#include "Sexy.TodLib/generated/SceneAtlasData.h"

static Color gGameButtonColors[6] = { Color(0, 0, 0), Color(0, 0, 0), Color(0, 0, 0), Color(255, 255, 255), Color(132, 132, 132), Color(212, 212, 212) };

//0x447B00
static void DrawStoneButton(Graphics* g, int x, int y, int theWidth, int theHeight, bool isDown, bool isHighLighted, const SexyString& theLabel)
{
	using enum FixedUIAtlas::Sprite;
	const TextureRegion& left = FixedUIAtlas::Images[static_cast<size_t>(isDown ? BUTTON_DOWN_LEFT : BUTTON_LEFT)];
	const TextureRegion& middle = FixedUIAtlas::Images[static_cast<size_t>(isDown ? BUTTON_DOWN_MIDDLE : BUTTON_MIDDLE)];
	const TextureRegion& right = FixedUIAtlas::Images[static_cast<size_t>(isDown ? BUTTON_DOWN_RIGHT : BUTTON_RIGHT)];
	const Sexy::TextureSource texture{FixedUIAtlas::Texture, 0};
	int aFontX = x;
	int aFontY = y;
	int aImageX = x;
	if (isDown)
	{
		aFontX++;
		aFontY++;
		aImageX++;
	}

	int aRepeat = (theWidth - left.mWidth - right.mWidth) / middle.mWidth;
	Sexy::DrawAtlasImage(g, texture, left, aImageX, y);
	aImageX += left.mWidth;
	while (aRepeat > 0)
	{
		Sexy::DrawAtlasImage(g, texture, middle, aImageX, y);
		aImageX += middle.mWidth;
		--aRepeat;
	}
	Sexy::DrawAtlasImage(g, texture, right, aImageX, y);

	g->mFont = isHighLighted ? Sexy::FONT_DWARVENTODCRAFT18BRIGHTGREENINSET : Sexy::FONT_DWARVENTODCRAFT18GREENINSET;
	aFontX += (theWidth - Sexy::FONT_DWARVENTODCRAFT18GREENINSET->StringWidth(theLabel)) / 2 + 1;
	aFontY += (theHeight - Sexy::FONT_DWARVENTODCRAFT18GREENINSET->mAscent / 6 - 1 + Sexy::FONT_DWARVENTODCRAFT18GREENINSET->mAscent) / 2 - 4;
	g->mColor = Color::White;
	g->DrawString(theLabel, aFontX, aFontY);
}

//0x447C60
GameButton::GameButton(int theId)
{
	mLabel = "";
	mOverAlpha = mOverAlphaSpeed = mOverAlphaFadeInSpeed = 0;



	mApp = (LawnApp*)gSexyAppBase;
	mId = theId;
	mLabelJustify = 0;
	mFont = nullptr;
	mButtonImage = mOverImage = mDownImage = mDisabledImage = mOverOverlayImage = nullptr;
	mInverted = mBtnNoDraw = mFrameNoDraw = false;
	mIsOver = mIsDown = mDisabled = false;
	mX = mY = mWidth = mHeight = 0;
	mParentWidget = nullptr;
	mDrawStoneButton = false;
	mTextOffsetX = mTextOffsetY = mButtonOffsetX = mButtonOffsetY = 0;
	for (int i = 0; i < 6; i++) mColors[i] = gGameButtonColors[i];
}

//0x447DE0
GameButton::~GameButton()
{
	if (mFont)
		delete mFont;
}

bool GameButton::HaveButtonImage(Image* theImage, const TextureRegion& theRegion)
{ 
	return theImage != nullptr || theRegion.mWidth != 0;
}

//0x447E60
void GameButton::DrawButtonImage(Graphics* g, Image* theImage, const TextureRegion& theRegion, int theX, int theY)
{
	int aPosX = theX + mButtonOffsetX;
	int aPosY = theY + mButtonOffsetY;
	if (theRegion.mWidth != 0)
		g->DrawImage(mButtonImage, aPosX, aPosY, theRegion);
	else
		g->DrawImage(theImage, aPosX, aPosY);
}

void GameButton::SetFont(_Font* theFont)
{
	if (mFont)
		delete mFont;
	
	mFont = theFont->Duplicate();
}

bool GameButton::IsButtonDown()
{ 
	return mIsDown && mIsOver && !mDisabled && !mBtnNoDraw;
}

//0x447EC0
// GOTY @Patoke: 0x44AAC0
void GameButton::Draw(Graphics* g)
{
	if (mBtnNoDraw)
		return;

	bool isDown = IsButtonDown() ^ mInverted;
	bool isHighLighted = mIsOver || IsMouseOver();
	if (mDrawStoneButton)
	{
		DrawStoneButton(g, mX, mY, mWidth, mHeight, isDown, isHighLighted, mLabel);
		return;
	}

	if (!mFont && mLabel.size() > 0)
		throw FontNotConfigured{};
	const float parentTransX = g->mTransX, parentTransY = g->mTransY;
	g->mTransX = parentTransX + mX;
	g->mTransY = parentTransY + mY;
	
	int aFontX = mTextOffsetX;
	int aFontY = mTextOffsetY;
	if (mFont)
	{
		if (mLabelJustify == GameButton::BUTTON_LABEL_CENTER)
			aFontX += (mWidth - mFont->StringWidth(mLabel)) / 2;
		else if (mLabelJustify == GameButton::BUTTON_LABEL_RIGHT)
			aFontX += mWidth - mFont->StringWidth(mLabel);

		aFontY += (mHeight - mFont->mAscent / 6 + mFont->mAscent - 1) / 2;
	}
	g->mFont = mFont;

	if (!isDown)
	{
		if (mDisabled && HaveButtonImage(mDisabledImage, mDisabledRegion))
			DrawButtonImage(g, mDisabledImage, mDisabledRegion, 0, 0);
		else if (mOverAlpha > 0.0f && HaveButtonImage(mOverImage, mOverRegion))
		{
			if (HaveButtonImage(mButtonImage, mNormalRegion) && mOverAlpha < 1.0f)  // 未完全过渡结束
				DrawButtonImage(g, mButtonImage, mNormalRegion, 0, 0);

			g->mColorizeImages = true;
			g->mColor = Color(255, 255, 255, mOverAlpha * 255);
			DrawButtonImage(g, mOverImage, mOverRegion, 0, 0);
			g->mColorizeImages = false;
		}
		else if (isHighLighted && HaveButtonImage(mOverImage, mOverRegion))
			DrawButtonImage(g, mOverImage, mOverRegion, 0, 0);
		else if (HaveButtonImage(mButtonImage, mNormalRegion))
			DrawButtonImage(g, mButtonImage, mNormalRegion, 0, 0);

		g->mColor = mColors[isHighLighted ? GameButton::COLOR_LABEL_HILITE : GameButton::COLOR_LABEL];
		g->DrawString(StringToSexyStringFast(mLabel), aFontX, aFontY);

		if (isHighLighted && mOverOverlayImage)
		{
			g->mDrawMode = Graphics::DRAWMODE_ADDITIVE;
			DrawButtonImage(g, mOverOverlayImage, mNormalRegion, 0, 0);
			g->mDrawMode = Graphics::DRAWMODE_NORMAL;
		}
	}
	else
	{
		if (HaveButtonImage(mDownImage, mDownRegion))
			DrawButtonImage(g, mDownImage, mDownRegion, 0, 0);
		else if (HaveButtonImage(mOverImage, mOverRegion))
			DrawButtonImage(g, mOverImage, mOverRegion, 1, 1);
		else
			DrawButtonImage(g, mButtonImage, mNormalRegion, 1, 1);
		
		g->mColor = mColors[GameButton::COLOR_LABEL_HILITE];
		g->DrawString(StringToSexyStringFast(mLabel), aFontX + 1, aFontY + 1);

		if (isHighLighted && mOverOverlayImage)
		{
			g->mDrawMode = Graphics::DRAWMODE_ADDITIVE;
			DrawButtonImage(g, mOverOverlayImage, mNormalRegion, 0, 0);
			g->mDrawMode = Graphics::DRAWMODE_NORMAL;
		}
	}

	g->mTransX = parentTransX;
	g->mTransY = parentTransY;
}

void GameButton::DrawAtlas(Graphics* g, const Sexy::TextureSource& texture, const TextureRegion* overlay)
{
	if (mBtnNoDraw)
		return;
	const bool down = IsButtonDown() ^ mInverted;
	const bool highlighted = mIsOver || IsMouseOver();
	if (!mFont && !mLabel.empty())
		throw FontNotConfigured{};
	int fontX = mTextOffsetX;
	int fontY = mTextOffsetY;
	if (mFont)
	{
		if (mLabelJustify == BUTTON_LABEL_CENTER)
			fontX += (mWidth - mFont->StringWidth(mLabel)) / 2;
		else if (mLabelJustify == BUTTON_LABEL_RIGHT)
			fontX += mWidth - mFont->StringWidth(mLabel);
		fontY += (mHeight - mFont->mAscent / 6 + mFont->mAscent - 1) / 2;
	}
	const float parentTransX = g->mTransX, parentTransY = g->mTransY;
	g->mTransX = parentTransX + mX;
	g->mTransY = parentTransY + mY;
	g->mFont = mFont;
	const auto draw = [&](const TextureRegion& rectangle, int x = 0, int y = 0) {
		Sexy::DrawAtlasImage(g, texture, rectangle,
			x + mButtonOffsetX, y + mButtonOffsetY);
	};
	if (!down)
	{
		if (mDisabled && mDisabledRegion.mWidth)
			draw(mDisabledRegion);
		else if (mOverAlpha > 0 && mOverRegion.mWidth)
		{
			if (mNormalRegion.mWidth && mOverAlpha < 1)
				draw(mNormalRegion);
			g->mColorizeImages = true;
			g->mColor = Color(255, 255, 255, mOverAlpha * 255);
			draw(mOverRegion);
			g->mColorizeImages = false;
		}
		else if (highlighted && mOverRegion.mWidth)
			draw(mOverRegion);
		else
			draw(mNormalRegion);
		g->mColor = mColors[highlighted ? COLOR_LABEL_HILITE : COLOR_LABEL];
		g->DrawString(mLabel, fontX, fontY);
	}
	else
	{
		if (mDownRegion.mWidth)
			draw(mDownRegion);
		else if (mOverRegion.mWidth)
			draw(mOverRegion, 1, 1);
		else
			draw(mNormalRegion, 1, 1);
		g->mColor = mColors[COLOR_LABEL_HILITE];
		g->DrawString(mLabel, fontX + 1, fontY + 1);
	}
	if (highlighted && overlay)
	{
		g->mDrawMode = Graphics::DRAWMODE_ADDITIVE;
		draw(*overlay);
		g->mDrawMode = Graphics::DRAWMODE_NORMAL;
	}
	g->mTransX = parentTransX;
	g->mTransY = parentTransY;
}

//0x448130
bool GameButton::IsMouseOver()
{
    if (!mApp->mWidgetManager->mMouseHoverEnabled) return false;
    return HitTest(mApp->mWidgetManager->mLastMouseX,
        mApp->mWidgetManager->mLastMouseY);
}

bool GameButton::HitTest(int x, int y, Widget* eventOwner)
{
	if (mDisabled || mBtnNoDraw)
		return false;

	// Input can arrive before Update. Hit-test this event without changing any
	// mouse position or advancing the button's highlight animation.
	WidgetManager* aManager = mApp->mWidgetManager;
	const bool aFocusMatches = aManager->mFocusWidget && aManager->mFocusWidget == mParentWidget;
	if (!aFocusMatches && mApp->GetDialogCount() > 0)
		return false;

    if (eventOwner != mParentWidget)
    {
        const auto source = eventOwner ? eventOwner->GetAbsPos() : Point(0, 0);
        const auto target = mParentWidget ? mParentWidget->GetAbsPos() : Point(0, 0);
        x += source.mX - target.mX;
        y += source.mY - target.mY;
    }
	return Rect(mX, mY, mWidth, mHeight).Contains(x, y);
}

//0x448330
// GOTY @Patoke: 0x44AF50
void GameButton::Update()
{
	WidgetManager* aManager = mApp->mWidgetManager;
	const Point origin = mParentWidget ? mParentWidget->GetAbsPos() : Point(0, 0);
	const int aMouseX = aManager->mLastMouseX - origin.mX;
	const int aMouseY = aManager->mLastMouseY - origin.mY;

	if ((aManager->mFocusWidget && aManager->mFocusWidget == mParentWidget) || mApp->GetDialogCount() <= 0)
	{
		mIsOver = aManager->mMouseHoverEnabled && Rect(mX, mY, mWidth, mHeight).Contains(aMouseX, aMouseY);
		mIsDown = aManager->mDownButtons & 5;
        if (aManager->mTouchCount != 0)
            for (const auto& contact : aManager->mTouches)
                if (contact.widget && contact.pressedTarget == this)
                {
                    if (Rect(mX, mY, mWidth, mHeight).Contains(
                        contact.event.x - origin.mX, contact.event.y - origin.mY))
                    {
                        mIsOver = mIsDown = true;
                        break;
                    }
                }
	}
	else
	{
		mIsOver = false;
		mIsDown = false;
	}


    if ((!aManager->mMouseHoverEnabled && !mIsOver) || (!mIsDown && !mIsOver && mOverAlphaSpeed < 0)) mOverAlpha = 0;
    const double rate = !mIsDown && !mIsOver && mOverAlpha > 0 && mOverAlphaSpeed > 0
        ? -mOverAlphaSpeed : mIsOver && mOverAlphaFadeInSpeed > 0 && mOverAlpha < 1
        ? mOverAlphaFadeInSpeed : 0;
    const auto nowNs = gSexyAppBase ? gSexyAppBase->mApplicationTimeNs : 0;
    if (rate != mOverAlphaRate)
    {
        mOverAlphaOrigin = mOverAlpha;
        mOverAlphaRate = rate;
        mOverAlphaStartTimeNs = nowNs - static_cast<std::int64_t>(Sexy::gUpdateIntervalNs);
    }
    if (rate != 0)
    {
        mOverAlpha = std::clamp(mOverAlphaOrigin + rate *
            (static_cast<double>(nowNs - mOverAlphaStartTimeNs) / 10'000'000.0), 0.0, 1.0);
    }

}

//0x4485C0
void LawnStoneButton::Draw(Graphics* g)
{
	if (mBtnNoDraw)
		return;

	bool isDown = (mIsDown && mIsOver && !mDisabled) ^ mInverted;
	DrawStoneButton(g, 0, 0, mWidth, mHeight, isDown, mIsOver, mLabel);
}

//0x448620
LawnStoneButton* MakeButton(int theId, ButtonListener* theListener, const SexyString& theText)
{
	LawnStoneButton* aButton = new LawnStoneButton(nullptr, theId, theListener);
	aButton->mLabel = theText;

	aButton->mTranslateX = 1;
	aButton->mTranslateY = 1;
	aButton->mHasAlpha = true;
	aButton->mHasTransparencies = true;
	aButton->mHeight = 33;
	return aButton;
}

//0x4486C0
// GOTY @Patoke: 0x44B2B0
NewLawnButton::NewLawnButton(Image* theComponentImage, int theId, ButtonListener* theListener) : DialogButton(theComponentImage, theId, theListener)
{
	mHiliteFont = nullptr;
	mTextDownOffsetX = 0;
	mTextDownOffsetY = 0;
	mButtonOffsetX = 0;
	mButtonOffsetY = 0;
	mUsePolygonShape = false;
	SetColor(ButtonWidget::COLOR_BKG, Color::White);
}

NewLawnButton::~NewLawnButton()
{
	//if (mHiliteFont)
	//	delete mHiliteFont;
}

//0x448790
// GOTY @Patoke: 0x44B383
void NewLawnButton::Draw(Graphics* g)
{
	if (mBtnNoDraw)
		return;

	bool isDown = (mIsDown && mIsOver && !mDisabled) ^ mInverted;
	int aFontX = mTextOffsetX + mTranslateX;
	int aFontY = mTextOffsetY + mTranslateY;
	if (mFont)
	{
		if (mLabelJustify == ButtonWidget::BUTTON_LABEL_CENTER)
			aFontX += (mWidth - mFont->StringWidth(mLabel)) / 2;
		else if (mLabelJustify == ButtonWidget::BUTTON_LABEL_RIGHT)
			aFontX += mWidth - mFont->StringWidth(mLabel);

		aFontY += (mHeight - mFont->mAscent / 6 + mFont->mAscent - 1) / 2;
	}

	g->mColorizeImages = true;
	if (!isDown)
	{
		g->mColor = mColors[ButtonWidget::COLOR_BKG];
		if (mDisabled && HaveButtonImage(mDisabledImage, mDisabledRegion))
			DrawButtonImage(g, mDisabledImage, mDisabledRegion, mButtonOffsetX, mButtonOffsetY);
		else if (mOverAlpha > 0.0f && HaveButtonImage(mOverImage, mOverRegion))
		{
			if (HaveButtonImage(mButtonImage, mNormalRegion) && mOverAlpha < 1.0f)  // 未完全过渡结束
				DrawButtonImage(g, mButtonImage, mNormalRegion, mButtonOffsetX, mButtonOffsetY);

			g->mColor.mAlpha = mOverAlpha * 255;
			DrawButtonImage(g, mOverImage, mOverRegion, mButtonOffsetX, mButtonOffsetY);
		}
		else if ((mIsOver || mIsDown) && HaveButtonImage(mOverImage, mOverRegion))
			DrawButtonImage(g, mOverImage, mOverRegion, mButtonOffsetX, mButtonOffsetY);
		else if (HaveButtonImage(mButtonImage, mNormalRegion))
			DrawButtonImage(g, mButtonImage, mNormalRegion, mButtonOffsetX, mButtonOffsetY);

		g->mColorizeImages = false;
		if (mIsOver)
		{
			g->mFont = mHiliteFont ? mHiliteFont : mFont;
			g->mColor = mColors[ButtonWidget::COLOR_LABEL_HILITE];
		}
		else
		{
			g->mFont = mFont;
			g->mColor = mColors[ButtonWidget::COLOR_LABEL];
		}
		g->DrawString(mLabel, aFontX, aFontY);
	}
	else
	{
		g->mColor = mColors[ButtonWidget::COLOR_BKG];
		if (HaveButtonImage(mDownImage, mDownRegion))
			DrawButtonImage(g, mDownImage, mDownRegion, mButtonOffsetX + mTranslateX, mButtonOffsetY + mTranslateY);
		else if (HaveButtonImage(mOverImage, mOverRegion))
			DrawButtonImage(g, mOverImage, mOverRegion, mButtonOffsetX + mTranslateX, mButtonOffsetY + mTranslateY);
		else
			DrawButtonImage(g, mButtonImage, mNormalRegion, mButtonOffsetX + mTranslateX, mButtonOffsetY + mTranslateY);

		g->mColorizeImages = false;
		g->mFont = mHiliteFont ? mHiliteFont : mFont;
		g->mColor = mColors[ButtonWidget::COLOR_LABEL_HILITE];
		g->DrawString(mLabel, aFontX + mTextDownOffsetX, aFontY + mTextDownOffsetY);
	}
}

//0x448B70
// GOTY @Patoke: 0x44B7C0
bool NewLawnButton::IsPointVisible(int x, int y)
{
	if (!mUsePolygonShape)
		return DialogButton::IsPointVisible(x, y);

	return TodIsPointInPolygon(mPolygonShape, 4, SexyVector2(x, y));
}

//0x448BC0
// GOTY @Patoke: 0x44B810
NewLawnButton* MakeNewButton(int theId, ButtonListener* theListener, const SexyString& theText, _Font* theFont, Image* theImageNormal, Image* theImageOver, Image* theImageDown)
{
	NewLawnButton* aButton = new NewLawnButton(nullptr, theId, theListener);
	aButton->SetFont(theFont == nullptr ? Sexy::FONT_BRIANNETOD12 : theFont);
	aButton->mLabel = theText;

	aButton->mWidth = theImageNormal->mWidth;
	aButton->mHeight = theImageNormal->mHeight;
	aButton->mButtonImage = theImageNormal;
	aButton->mDownImage = theImageDown;
	aButton->mOverImage = theImageOver;
	aButton->mHasAlpha = true;
	aButton->mHasTransparencies = true;
	aButton->mTranslateX = 1;
	aButton->mTranslateY = 1;

	return aButton;
}

namespace {
class FixedUIBackButton final : public NewLawnButton
{
public:
	FixedUIBackButton(int id, ButtonListener* listener, const SexyString& text)
		: NewLawnButton(nullptr, id, listener)
	{
		using enum FixedUIAtlas::Sprite;
		mNormalRegion = FixedUIAtlas::Images[static_cast<size_t>(SEEDCHOOSER_BUTTON2)];
		mOverRegion = FixedUIAtlas::Images[static_cast<size_t>(SEEDCHOOSER_BUTTON2_GLOW)];
		mLabel = text;
		SetFont(Sexy::FONT_BRIANNETOD12);
		mWidth = mNormalRegion.mWidth;
		mHeight = mNormalRegion.mHeight;
		mHasAlpha = mHasTransparencies = true;
		mTranslateX = mTranslateY = 1;
	}

	void Draw(Graphics* g) override
	{
		if (mBtnNoDraw)
			return;
		const Sexy::TextureSource texture{
			FixedUIAtlas::Texture, 0};
		const bool down = (mIsDown && mIsOver && !mDisabled) ^ mInverted;
		int fontX = mTextOffsetX + mTranslateX;
		int fontY = mTextOffsetY + mTranslateY;
		if (mFont)
		{
			if (mLabelJustify == ButtonWidget::BUTTON_LABEL_CENTER)
				fontX += (mWidth - mFont->StringWidth(mLabel)) / 2;
			else if (mLabelJustify == ButtonWidget::BUTTON_LABEL_RIGHT)
				fontX += mWidth - mFont->StringWidth(mLabel);
			fontY += (mHeight - mFont->mAscent / 6 + mFont->mAscent - 1) / 2;
		}
		const auto draw = [&](const TextureRegion& rectangle, bool pressed) {
			Sexy::DrawAtlasImage(g, texture, rectangle,
				mButtonOffsetX + (pressed ? mTranslateX : 0),
				mButtonOffsetY + (pressed ? mTranslateY : 0));
		};
		g->mColorizeImages = true;
		g->mColor = mColors[ButtonWidget::COLOR_BKG];
		if (down)
			draw(mOverRegion, true);
		else if (mOverAlpha > 0)
		{
			if (mOverAlpha < 1)
				draw(mNormalRegion, false);
			g->mColor.mAlpha = mOverAlpha * 255;
			draw(mOverRegion, false);
		}
		else
			draw(mIsOver || mIsDown ? mOverRegion : mNormalRegion, false);
		g->mColorizeImages = false;
		g->mFont = down || mIsOver ? (mHiliteFont ? mHiliteFont : mFont) : mFont;
		g->mColor = mColors[down || mIsOver
			? ButtonWidget::COLOR_LABEL_HILITE : ButtonWidget::COLOR_LABEL];
		g->DrawString(mLabel, fontX + (down ? mTextDownOffsetX : 0),
			fontY + (down ? mTextDownOffsetY : 0));
	}
};
}

NewLawnButton* MakeFixedUIBackButton(int id, ButtonListener* listener, const SexyString& text)
{
	return new FixedUIBackButton(id, listener, text);
}

