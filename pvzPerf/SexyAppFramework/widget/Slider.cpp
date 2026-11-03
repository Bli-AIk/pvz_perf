#include "Slider.h"
#include "graphics/Graphics.h"
#include "graphics/Image.h"
#include "SliderListener.h"
#include "WidgetManager.h"
#include "SexyAppBase.h"

using namespace Sexy;

Slider::Slider(Image* theTrackImage, Image* theThumbImage, int theId, SliderListener* theListener) : 		
	mListener(theListener),
	mVal(0.0),
	mId(theId),
	mTrackImage(theTrackImage),
	mThumbImage(theThumbImage)
{
	mDragging = false;
	mHorizontal = true;
	mRelX = mRelY = 0;
}

void Slider::SetValue(double theValue)
{	
	mVal = theValue;
	if (mVal < 0.0)
		mVal = 0.0;
	else if (mVal > 1.0)
		mVal = 1.0;

	MarkDirtyFull();
}

bool Slider::HasTransparencies()
{
	return true;
}

void Slider::Draw(Graphics* g)
{	
	if (mTrackImage != NULL)
	{
		int cw = mHorizontal ? mTrackImage->mWidth/3 : mTrackImage->mWidth;
		int ch = mHorizontal ? mTrackImage->mHeight : mTrackImage->mHeight/3;

		if (mHorizontal)
		{
			int ty = (mHeight - ch) / 2;

			g->DrawImage(mTrackImage, 0, ty, Rect(0, 0, cw, ch));

			Graphics aClipG(*g);
			aClipG.ClipRect(cw, ty, mWidth - cw*2, ch);
			for (int i = 0; i < (mWidth-cw*2+cw-1)/cw; i++)
				aClipG.DrawImage(mTrackImage, cw + i*cw, ty, Rect(cw, 0, cw, ch));

			g->DrawImage(mTrackImage, mWidth-cw, ty, Rect(cw*2, 0, cw, ch));
		}
		else
		{
			g->DrawImage(mTrackImage, 0, 0, Rect(0, 0, cw, ch));
			Graphics aClipG(*g);
			aClipG.ClipRect(0, ch, cw, mHeight - ch * 2);
			for (int i = 0; i < (mHeight-ch*2+ch-1)/ch; i++)
				aClipG.DrawImage(mTrackImage, 0, ch + i*ch, Rect(0, ch, cw, ch));

			g->DrawImage(mTrackImage, 0, mHeight-ch, Rect(0, ch*2, cw, ch));
		}
	}

	if (mHorizontal && (mThumbImage != NULL))
		g->DrawImage(mThumbImage, (int) (mVal * (mWidth - mThumbImage->mWidth)), (mHeight - mThumbImage->mHeight) / 2);
	else if (!mHorizontal && (mThumbImage != NULL))
		g->DrawImage(mThumbImage, (mWidth - mThumbImage->mWidth) / 2, (int) (mVal * (mHeight - mThumbImage->mHeight)));

	//g->mColor = Color(255, 255, 0);
	//g->FillRect(0, 0, mWidth, mHeight);	
}

void Slider::MouseDown(int x, int y, int theClickCount)
{
	(void)theClickCount;
	if (mHorizontal)
	{
		int aThumbX = (int) (mVal * (mWidth - mThumbImage->mWidth));

		if ((x >= aThumbX) && (x < aThumbX + mThumbImage->mWidth))
		{
			mWidgetManager->mApp->SetCursor(CURSOR_DRAGGING);
			mDragging = true;
			mRelX = x - aThumbX;
		}
		else
		{
			// clicked on the bar, set position to mouse click
			double pos = (double)x / mWidth;
			SetValue(pos);
		}
	}
	else
	{
		int aThumbY = (int) (mVal * (mHeight - mThumbImage->mHeight));

		if ((y >= aThumbY) && (y < aThumbY + mThumbImage->mHeight))
		{
			mWidgetManager->mApp->SetCursor(CURSOR_DRAGGING);
			mDragging = true;
			mRelY = y - aThumbY;
		}
		else
		{
			// clicked on the bar, set position to mouse click
			double pos = (double)y / mHeight;
			SetValue(pos);
		}
	}
}

void Slider::MouseMove(int x, int y)
{
	if (mHorizontal)
	{
		int aThumbX = (int) (mVal * (mWidth - mThumbImage->mWidth));

		if ((x >= aThumbX) && (x < aThumbX + mThumbImage->mWidth))
			mWidgetManager->mApp->SetCursor(CURSOR_DRAGGING);
		else
			mWidgetManager->mApp->SetCursor(CURSOR_POINTER);
	}
	else
	{
		int aThumbY = (int) (mVal * (mHeight - mThumbImage->mHeight));

		if ((y >= aThumbY) && (y < aThumbY + mThumbImage->mHeight))
			mWidgetManager->mApp->SetCursor(CURSOR_DRAGGING);
		else
			mWidgetManager->mApp->SetCursor(CURSOR_POINTER);
	}
}

void Slider::MouseDrag(int x, int y)
{
	if (mDragging)
	{	
		double anOldVal = mVal;

		if (mHorizontal)
			mVal = (x - mRelX) / (double) (mWidth - mThumbImage->mWidth);
		else
			mVal = (y - mRelY) / (double) (mHeight - mThumbImage->mHeight);

		if (mVal < 0.0)
			mVal = 0.0;
		if (mVal > 1.0)
			mVal = 1.0;

		if (mVal != anOldVal)
		{
			mListener->SliderVal(mId, mVal);
			MarkDirtyFull();
		}
	}
}

void Slider::MouseUp(int x, int y)
{
	(void)x;(void)y;
	mDragging = false;
	mWidgetManager->mApp->SetCursor(CURSOR_POINTER);
	mListener->SliderVal(mId, mVal);
}

void Slider::TouchCanceled(const TouchEvent&)
{
    mDragging = false;
}

void Slider::MouseLeave()
{
	if (!mDragging)
		mWidgetManager->mApp->SetCursor(CURSOR_POINTER);
}
