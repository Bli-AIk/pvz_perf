#include "DDInterface.h"
#include "Image.h"
#include "RenderInterface.h"
#include "Graphics.h"
#include "SexyAppBase.h"
#include "misc/AutoCrit.h"
#include "widget/WidgetManager.h"
#include "SexyAppFramework/platform/WindowGeometry.h"
#include "GameViewport.h"

#include <algorithm>
#include <stdexcept>

using namespace Sexy;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
DDInterface::DDInterface(SexyAppBase* theApp)
	: mApp(theApp),
	mRenderInterface(NULL),
	mInRedraw(false),
	mWidth(640),
	mHeight(480),
	mIsWidescreen(false),
	mDisplayWidth(640),
	mDisplayHeight(480),
	mFullscreenBits(32),
	mRefreshRate(60),
	mMillisecondsPerFrame(16),
	mScanLineFailCount(0),
	mRedAddTable(NULL),
	mGreenAddTable(NULL),
	mBlueAddTable(NULL),
	mInitialized(false),
	mHWnd(NULL),
	mIsWindowed(true),
	mScreenImage(NULL),
	mVideoOnlyDraw(false),
	mInitCount(0),
	mCursorWidth(32),
	mCursorHeight(32),
	mNextCursorX(0),
	mNextCursorY(0),
	mCursorX(0),
	mCursorY(0),
	mCursorImage(NULL),
	mHasOldCursorArea(false)
{
	mRenderInterface = new RenderInterface;
	mRGBBits = 32;
	mRedMask = 0xFF0000;
	mGreenMask = 0x00FF00;
	mBlueMask = 0x0000FF;
	mRedBits = 8;
	mGreenBits = 8;
	mBlueBits = 8;
	mRedShift = 23;
	mGreenShift = 15;
	mBlueShift = 7;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
DDInterface::~DDInterface()
{
	Cleanup();
	delete mRenderInterface;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool DDInterface::SetCursorImage(Image* theImage)
{
	AutoCrit anAutoCrit(mCritSect);
	if (mCursorImage != theImage)
	{
		mCursorImage = theImage;
		return true;
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool DDInterface::Redraw(Rect* theClipRect)
{
	(void)theClipRect;
	AutoCrit anAutoCrit(mCritSect);
    if (!mInitialized || !mRenderInterface || !mRenderInterface->mRenderer)
        throw std::logic_error("Redraw without an initialized native Vulkan renderer");
    mInRedraw = true;
    struct Reset { bool& value; ~Reset() { value = false; } } reset{mInRedraw};
    const bool submitted = mRenderInterface->mSceneBegun;
    mRenderInterface->Flush(); // VkResult failures throw; no DX/software fallback.
    return submitted;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
	void DDInterface::RemapMouse(int& theX, int& theY)
	{
		if (mInitialized)
		{
			theX = (theX - mVisibleGameRect.mX) * mWidth / std::max(1, mVisibleGameRect.mWidth)
                + mApp->mScreenBounds.mX;
			theY = (theY - mVisibleGameRect.mY) * mHeight / std::max(1, mVisibleGameRect.mHeight);
		}
	}

    void DDInterface::UpdateVisibleGameRect()
    {
        const auto rect=mRenderInterface->mRenderer->PresentRect(mPresentationRect.mWidth,mPresentationRect.mHeight);
        mVisibleGameRect=Rect(mPresentationRect.mX+rect.x,mPresentationRect.mY+rect.y,rect.width,rect.height);
    }

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
unsigned int DDInterface::GetColorRef(unsigned int theRGB)
{
	return theRGB;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void DDInterface::Remove3DData(Image* theImage)
{
	mRenderInterface->RemoveImage(theImage);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void DDInterface::Cleanup()
{
	AutoCrit anAutoCrit(mCritSect);

	mInitialized = false;
	if (mRenderInterface != NULL)
		mRenderInterface->Cleanup();

	if (mScreenImage != NULL)
	{
		delete (mScreenImage);
		mScreenImage = NULL;
	}

	delete [] mRedAddTable;
	mRedAddTable = NULL;
	delete [] mGreenAddTable;
	mGreenAddTable = NULL;
	delete [] mBlueAddTable;
	mBlueAddTable = NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int DDInterface::Init(NativeWindowHandle theWindow, bool isWindowed)
{
    AutoCrit lock(mCritSect);
    if (mInitialized) {
        const int result = PrepareScreenMode(theWindow,isWindowed);
        return result == RESULT_OK ? ResizePreparedPresent() : result;
    }
    mHWnd = theWindow;
    mWidth = mApp->mWidth; mHeight = mApp->mHeight;
    mAspect = Ratio(mWidth,mHeight);
    mIsWindowed = isWindowed;
    const auto client = QueryNativeClientExtent(mHWnd);
    mDisplayWidth=client.width;mDisplayHeight=client.height;
#if defined(__ANDROID__)
    constexpr bool mobile=true;
#else
    constexpr bool mobile=false;
#endif
    mApp->mLandscapeUIActive=SelectLandscapeUI(mApp->mLandscapeUI,mobile,client.width,client.height);
    auto viewport = GameViewport::Fit(client.width,client.height);
    if(mApp->mLandscapeUIActive && viewport.width<890)viewport=GameViewport::Fit(890,600);
    mWidth = viewport.width;
    mHeight = viewport.height;
    mAspect = Ratio(mWidth, mHeight);
    mDesktopWidth = mDisplayWidth; mDesktopHeight = mDisplayHeight;
    mDisplayAspect = Ratio(mDisplayWidth,mDisplayHeight);
    mDesktopAspect = mDisplayAspect;
    mPresentationRect = Rect(0,0,mDisplayWidth,mDisplayHeight);
    mRefreshRate = 100;
    mMillisecondsPerFrame = 10;
    mRGBBits=32;
    mRedMask=0xff0000; mGreenMask=0xff00; mBlueMask=0xff;
    mRedBits=mGreenBits=mBlueBits=8;
    mRedShift=23; mGreenShift=15; mBlueShift=7;
    mRedAddTable = new int[256]; mGreenAddTable = new int[256]; mBlueAddTable = new int[256];
    for(int i=0;i<256;++i) {
        mRedAddTable[i]=mGreenAddTable[i]=mBlueAddTable[i]=i;
        mRedConvTable[i]=static_cast<unsigned int>(i)<<16;
        mGreenConvTable[i]=static_cast<unsigned int>(i)<<8;
        mBlueConvTable[i]=static_cast<unsigned int>(i);
    }
    mScreenImage = new Image(mApp);
    mScreenImage->mRenderOwner=mRenderInterface;
    mScreenImage->mWidth=mWidth; mScreenImage->mHeight=mHeight;
    mScreenImage->mForcedMode = true;
    mScreenImage->mHasTrans = false;
    mScreenImage->mHasAlpha = false;
    mApp->mWidgetManager->mImage=mScreenImage;
    if (!mRenderInterface->InitFromDDInterface(this)) return RESULT_3D_FAIL;
    UpdateVisibleGameRect();
    mApp->mScreenBounds=Rect(viewport.left,0,mWidth,mHeight);
    ++mInitCount;
    mInitialized=true;
    return RESULT_OK;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
int DDInterface::PrepareScreenMode(NativeWindowHandle theWindow, bool wantWindowed)
{
    AutoCrit lock(mCritSect);
    if (!mInitialized || !mScreenImage || !mRenderInterface->mRenderer)
        return RESULT_FAIL;
    mHWnd=theWindow;
    mIsWindowed=wantWindowed;
    const auto [width, height] = QueryNativeClientExtent(mHWnd);
    if(mApp->mPhysMinimized || width<=0 || height<=0) return RESULT_DEFERRED;
    return RESULT_OK;
}

int DDInterface::ResizePreparedPresent()
{
	AutoCrit anAutoCrit(mCritSect);
	if (mRenderInterface == NULL || mScreenImage == NULL)
	{
		return RESULT_FAIL;
	}
    const auto [width, height] = QueryNativeClientExtent(mHWnd);
    if(mApp->mPhysMinimized || width<=0 || height<=0) return RESULT_DEFERRED;
    if (!mRenderInterface->ResizePresent(width,height)) return RESULT_DEFERRED;
    auto viewport = GameViewport::Fit(width,height);
    if(mApp->mLandscapeUIActive && viewport.width<890)viewport=GameViewport::Fit(890,600);
    if (viewport.width != mWidth || viewport.height != mHeight) {
        mRenderInterface->ResizeScene(viewport.width, viewport.height);
        mWidth = viewport.width;
        mHeight = viewport.height;
        mAspect = Ratio(mWidth, mHeight);
        mScreenImage->mWidth = mWidth;
        mScreenImage->mHeight = mHeight;
        // Scale chains borrow the main canvas. Rebuild only after its fence and
        // only on an aspect change; resident game images never need host pixels.
        if (mApp->mModernScalePrepared) {
            mApp->mModernScalePrepared = false;
            mApp->PrepareScalePipelineObjects();
            auto& renderer = *mRenderInterface->mRenderer;
            renderer.WarmScale(false);
            renderer.WarmScale(true);
            renderer.SetScaleMode(mApp->IsScalePresentActive(), !mIsWindowed);
        }
    }
    mDisplayWidth=width; mDisplayHeight=height;
    mPresentationRect=Rect(0,0,width,height);
    mDisplayAspect=Ratio(width,height);

	mRenderInterface->mIsWindowed = mIsWindowed;
	UpdateVisibleGameRect();
	mApp->mScreenBounds = Rect(viewport.left, 0, mWidth, mHeight);
	if (mApp->mWidgetManager != nullptr)
		mApp->mWidgetManager->mImage = mScreenImage;
	mInitCount++;
	mInitialized = true;
	return RESULT_OK;
}
