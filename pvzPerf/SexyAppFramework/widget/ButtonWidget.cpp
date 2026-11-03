#include "SexyAppBase.h"
#include <algorithm>
#include "ButtonWidget.h"
#include "graphics/Image.h"
#include "graphics/Font.h"
#include "WidgetManager.h"
#include "ButtonListener.h"

using namespace Sexy;

static int gButtonWidgetColors[][3] = {
	{0, 0, 0},
	{0, 0, 0},
	{0, 0, 0},
	{255, 255, 255},
	{132, 132, 132},
	{212, 212, 212}};

ButtonWidget::ButtonWidget(int theId, ButtonListener* theButtonListener)	
{
	mId = theId;
	mFont = NULL;
	mLabelJustify = BUTTON_LABEL_CENTER;
	mButtonImage = NULL;
	mOverImage = NULL;
	mDownImage = NULL;
	mDisabledImage = NULL;
	mInverted = false;
	mBtnNoDraw = false;
	mFrameNoDraw = false;
	mButtonListener = theButtonListener;
	mHasAlpha = true;

	mOverAlpha = 0;
	mOverAlphaSpeed = 0;
	mOverAlphaFadeInSpeed = 0;




	SetColors(gButtonWidgetColors, NUM_COLORS);
}

ButtonWidget::~ButtonWidget()
{
	delete mFont;
}

void ButtonWidget::SetFont(_Font* theFont)
{
	delete mFont;
	mFont = theFont->Duplicate();
}

bool ButtonWidget::IsButtonDown()
{
	return mIsDown && mIsOver && !mDisabled;
}

bool ButtonWidget::HaveButtonImage(Image *theImage, const TextureRegion& theRegion)
{
	return (theImage!=NULL || theRegion.mWidth!=0);
}
	
void ButtonWidget::DrawButtonImage(Graphics *g, Image *theImage, const TextureRegion& theRegion, int x, int y)
{
	if (theRegion.mWidth != 0)
		g->DrawImage(mButtonImage,x,y,theRegion);
	else
		g->DrawImage(theImage,x,y);
}

void ButtonWidget::Draw(Graphics* g)
{
	if (mBtnNoDraw)
		return;

	if ((mFont == NULL) && (mLabel.length() > 0))
		throw FontNotConfigured{};

	bool isDown = mIsDown && mIsOver && !mDisabled;
	isDown ^= mInverted;

	int aFontX = 0; // BUTTON_LABEL_LEFT
	int aFontY = 0;
	
	if (mFont != NULL)
	{
		if (mLabelJustify == BUTTON_LABEL_CENTER)
			aFontX = (mWidth - mFont->StringWidth(mLabel))/2;
		else if (mLabelJustify == BUTTON_LABEL_RIGHT)
			aFontX = mWidth - mFont->StringWidth(mLabel);
		aFontY = (mHeight + mFont->mAscent - mFont->mAscent/6 - 1)/2;

		//aFontX = (mWidth - mFont->StringWidth(mLabel))/2;
		//aFontY = (mHeight - mFont->mHeight)/2 + mFont->mAscent - 1;		
	}

	g->mFont = mFont;
	
	if ((mButtonImage == NULL) && (mDownImage == NULL))
	{
		if (!mFrameNoDraw)
		{
			g->mColor = mColors[COLOR_BKG];
			g->FillRect(0, 0, mWidth, mHeight);
		}

		if (isDown)
		{
			if (!mFrameNoDraw)
			{
				g->mColor = mColors[COLOR_DARK_OUTLINE];
				g->FillRect(0, 0, mWidth-1, 1);
				g->FillRect(0, 0, 1, mHeight-1);
				
				g->mColor = mColors[COLOR_LIGHT_OUTLINE];
				g->FillRect(0, mHeight - 1, mWidth, 1);
				g->FillRect(mWidth - 1, 0, 1, mHeight);									
		
				g->mColor = mColors[COLOR_MEDIUM_OUTLINE];
				g->FillRect(1, 1, mWidth - 3, 1);
				g->FillRect(1, 1, 1, mHeight - 3);
			}

			if (mIsOver)
				g->mColor = mColors[COLOR_LABEL_HILITE];
			else
				g->mColor = mColors[COLOR_LABEL];

			g->DrawString(mLabel, aFontX+1, aFontY+1);
		}
		else
		{			
			if (!mFrameNoDraw)
			{
				g->mColor = mColors[COLOR_LIGHT_OUTLINE];
				g->FillRect(0, 0, mWidth-1, 1);
				g->FillRect(0, 0, 1, mHeight-1);
				
				g->mColor = mColors[COLOR_DARK_OUTLINE];
				g->FillRect(0, mHeight - 1, mWidth, 1);
				g->FillRect(mWidth - 1, 0, 1, mHeight);									
		
				g->mColor = mColors[COLOR_MEDIUM_OUTLINE];
				g->FillRect(1, mHeight - 2, mWidth - 2, 1);
				g->FillRect(mWidth - 2, 1, 1, mHeight - 2);			
			}
			
			if (mIsOver)
				g->mColor = mColors[COLOR_LABEL_HILITE];
			else
				g->mColor = mColors[COLOR_LABEL];

			g->DrawString(mLabel, aFontX, aFontY);		
		}		
	}
	else
	{
		if (!isDown)
		{
			if (mDisabled && HaveButtonImage(mDisabledImage,mDisabledRegion))
				DrawButtonImage(g,mDisabledImage,mDisabledRegion,0,0);
			else if ((mOverAlpha > 0) && HaveButtonImage(mOverImage,mOverRegion))
			{
				if (HaveButtonImage(mButtonImage, mNormalRegion)  && mOverAlpha<1)
					DrawButtonImage(g,mButtonImage,mNormalRegion,0,0);

				g->mColorizeImages = true;
				g->mColor = Color(255,255,255,(int)(mOverAlpha * 255));
				DrawButtonImage(g,mOverImage,mOverRegion,0,0);
				g->mColorizeImages = false;
			}
			else if (mIsOver && HaveButtonImage(mOverImage,mOverRegion))
			{
				DrawButtonImage(g,mOverImage,mOverRegion,0,0);
			}
			else if (HaveButtonImage(mButtonImage,mNormalRegion))
				DrawButtonImage(g,mButtonImage,mNormalRegion,0,0);

			if (mIsOver)
				g->mColor = mColors[COLOR_LABEL_HILITE];
			else
				g->mColor = mColors[COLOR_LABEL];
			g->DrawString(mLabel, aFontX, aFontY);
		}
		else
		{
			if (HaveButtonImage(mDownImage, mDownRegion))
				DrawButtonImage(g, mDownImage, mDownRegion, 0, 0);
			else if (HaveButtonImage(mOverImage,mOverRegion))
				DrawButtonImage(g, mOverImage, mOverRegion, 1, 1);
			else
				DrawButtonImage(g, mButtonImage, mNormalRegion, 1, 1);

			g->mColor = mColors[COLOR_LABEL_HILITE];
			g->DrawString(mLabel, aFontX+1, aFontY+1);
		}
	}
}

void ButtonWidget::SetDisabled(bool isDisabled)
{
	Widget::SetDisabled(isDisabled);
	
	if (HaveButtonImage(mDisabledImage,mDisabledRegion))
		MarkDirty();
}

void ButtonWidget::MouseEnter()
{
	Widget::MouseEnter();

	// 2026-10-12 鼠标事件案（用户规格：鼠标在任何情况下都不要标脏）：悬停
	// 高亮 = mIsOver/mOverAlpha 状态位，棋盘每帧全量重绘，下一正常帧自然
	// 画出（≤10ms）。此前这里的 MarkDirty 让 1000Hz 鼠标每次进出控件都
	// 触发一次等待期完整重绘（Vulkan 下=完整管线，零售同路径=廉价 blit），
	// 是高回报率鼠标掉帧的唯一输入侧源头。悬停音效在 ButtonMouseEnter
	// 监听器里，不受影响。MouseDown/Up 的 MarkDirty 是点击反馈，保留。

	mButtonListener->ButtonMouseEnter(mId);
}

void ButtonWidget::MouseLeave()
{
	Widget::MouseLeave();

	// 2026-10-12 同 MouseEnter：悬停离开不标脏，下一正常帧自然呈现。

	mButtonListener->ButtonMouseLeave(mId);
}

void ButtonWidget::MouseMove(int theX, int theY)
{
	Widget::MouseMove(theX, theY);
	
	mButtonListener->ButtonMouseMove(mId, theX, theY);
}

void ButtonWidget::MouseDown(int theX, int theY, int theBtnNum, int theClickCount)
{
	Widget::MouseDown(theX, theY, theBtnNum, theClickCount);
		
	mButtonListener->ButtonPress(mId);
	
	// 2026-10-12 对齐零售：WidgetManager::MouseDown 不标脏，mIsDown 状态位
	// 由下一正常帧自然画出（点击"立即感"来自 ButtonPress 的即时音效/逻辑，
	// 不是重绘先行）。棋盘每帧全量重绘，延迟≤10ms。
}

void ButtonWidget::MouseUp(int theX, int theY, int theBtnNum, int theClickCount)
{	
	Widget::MouseUp(theX, theY, theBtnNum, theClickCount);
	
	if (mIsOver && mWidgetManager->mHasFocus)
		mButtonListener->ButtonDepress(mId);
	
	// 2026-10-12 同 MouseDown：抬起不标脏。
}

void ButtonWidget::Update()
{
	Widget::Update();

	if (mIsDown && mIsOver)
		mButtonListener->ButtonDownTick(mId);


    if ((mWidgetManager && !mWidgetManager->mMouseHoverEnabled && !mIsOver) || (!mIsDown && !mIsOver && mOverAlphaSpeed <= 0)) mOverAlpha = 0;
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
        MarkDirty();
    }

}


