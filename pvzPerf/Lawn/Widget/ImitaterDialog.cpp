#include "Lawn/Plant.h"
#include "Lawn/Board.h"
#include "Lawn/LandscapeHud.h"
#include "Sexy.TodLib/generated/SceneAtlasData.h"
#include <algorithm>
#include "GameButton.h"
#include "Lawn/SeedPacket.h"
#include "LawnApp.h"
#include "ImitaterDialog.h"
#include "SeedChooserScreen.h"
#include "Lawn/ToolTipWidget.h"
#include "GameConstants.h"
#include "widget/WidgetManager.h"

void ImitaterDialog::Resize(int x,int y,int width,int height) {
    LawnDialog::Resize(x,y,width,height);
    if(!mLandscapePicker)return;
    // LawnDialog tiles whole center strips; its painted inner bottom can end
    // above mHeight. Match that boundary instead of clipping against widget size.
    const int top=CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::DIALOG_TOPLEFT)].mHeight;
    const int middle=CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::DIALOG_CENTERLEFT)].mHeight;
    const int bottom=CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::DIALOG_BOTTOMLEFT)].mHeight;
    const int rows=std::max(0,(mHeight-top-bottom-DIALOG_HEADER_OFFSET)/middle);
    const int innerBottom=DIALOG_HEADER_OFFSET+top+rows*middle-4;
    mCardViewport={(mWidth-347)/2,130,347,std::max(0,innerBottom-130)};
    mCardScrollMinimum=static_cast<float>(std::min(0,mCardViewport.mHeight-10*55));
}

//0x482B00
ImitaterDialog::ImitaterDialog() : 
	LawnDialog(gLawnApp, Dialogs::DIALOG_IMITATER, true, gStr.CHOOSE_SEED_TO_COPY, (""), gStr.DIALOG_BUTTON_OK, Dialog::BUTTONS_YES_NO)
{
	CalcSize(IMITATER_DIALOG_WIDTH - mWidth, IMITATER_DIALOG_HEIGHT - mHeight);
    mLandscapePicker=mApp->mBoard&&mApp->mBoard->mLandscapeHud;
    Resize(mX,mY,mWidth,mHeight);
	mToolTip = new ToolTipWidget();
	mClip = false;
	mToolTipSeed = SeedType::SEED_NONE;
	mYesButton->mBtnNoDraw = true;
	mYesButton->mMouseVisible = false;
	mNoButton->mBtnNoDraw = true;
	mNoButton->mMouseVisible = false;
}

//0x482CE0
ImitaterDialog::~ImitaterDialog()
{
	delete mToolTip;
}

//0x482D30
SeedType ImitaterDialog::SeedHitTest(int x, int y)
{
    if(mLandscapePicker && !mCardViewport.Contains(x,y))return SEED_NONE;
	for (SeedType aSeedType = (SeedType)0; aSeedType < SeedType::SEED_GATLINGPEA; aSeedType = (SeedType)(aSeedType + 1))
	{
		if (mApp->SeedTypeAvailable(aSeedType))
		{
			int aSeedX, aSeedY;
			GetSeedPosition(aSeedType, aSeedX, aSeedY);
			if (Rect(aSeedX, aSeedY, mLandscapePicker?81:SEED_PACKET_WIDTH, mLandscapePicker?53:SEED_PACKET_HEIGHT).Contains(x, y))
			{
				return aSeedType;
			}
		}
	}
	return SeedType::SEED_NONE;
}

//0x482DD0
void ImitaterDialog::UpdateCursor()
{
    if(mLandscapePicker&&!mApp->mWidgetManager->mMouseHoverEnabled)return;
	SeedType aSeedType = SeedHitTest(mApp->mWidgetManager->mLastMouseX - mX, mApp->mWidgetManager->mLastMouseY - mY);
	if (aSeedType != SeedType::SEED_NONE && !mApp->mSeedChooserScreen->SeedNotAllowedToPick(aSeedType))
	{
		mApp->SetCursor(CURSOR_HAND);
	}
	else
	{
		mApp->SetCursor(CURSOR_POINTER);
	}
}

//0x482E50
void ImitaterDialog::Update()
{
	LawnDialog::Update();
    if(mLandscapePicker && !mCardScroll.pressed && mCardScroll.velocity!=0) {
        mCardScroll.Advance(ScrollTimeNs(),mCardScrollMinimum,0);MarkDirty();
    }
	ShowToolTip();
	UpdateCursor();
}

//0x482E70
void ImitaterDialog::GetSeedPosition(int theIndex, int& x, int& y)
{
    if(mLandscapePicker) {
        const auto viewport=mCardViewport;
        x=viewport.mX+4+(theIndex%4)*85;
        y=viewport.mY+2+(theIndex/4)*55+FloatRoundToInt(mCardScroll.position);
        return;
    }
	x = (theIndex % 8) * (SEED_PACKET_WIDTH + 1) + mWidth / 2 - 210;
	y = (theIndex / 8) * (SEED_PACKET_HEIGHT + 1) + 112;
}

//0x482EC0
void ImitaterDialog::Draw(Graphics* g)
{
	LawnDialog::Draw(g);
	g->mLinearBlend = true;
    Graphics list(*g);
    if(mLandscapePicker)list.ClipRect(mCardViewport);
    auto* cards=mLandscapePicker?&list:g;
	for (SeedType aSeedType = (SeedType)0; aSeedType < SeedType::SEED_GATLINGPEA; aSeedType = (SeedType)(aSeedType + 1))
	{
		if (mApp->SeedTypeAvailable(aSeedType))
		{
			int aSeedX, aSeedY;
			GetSeedPosition(aSeedType, aSeedX, aSeedY);
			bool aGrayed = mApp->mSeedChooserScreen->SeedNotAllowedToPick(aSeedType) || mApp->mSeedChooserScreen->SeedNotRecommendedToPick(aSeedType);
			if(mLandscapePicker)LandscapeHud::DrawPacket(cards,aSeedX,aSeedY,SEED_IMITATER,aSeedType,0,aGrayed?115:255,true,false);
            else DrawSeedPacket(cards, aSeedX, aSeedY, SeedType::SEED_IMITATER, aSeedType, 0, aGrayed ? 115 : 255, true, false);
		}
	}
	mToolTip->Draw(g);
}

//0x483030
void ImitaterDialog::ShowToolTip()
{
	if ((mLandscapePicker&&!mApp->mWidgetManager->mMouseHoverEnabled) || !mApp->mWidgetManager->mMouseIn || !mApp->mActive)
	{
		mToolTip->mVisible = false;
		mToolTipSeed = SeedType::SEED_NONE;
		return;
	}
	
	SeedType aSeedType = SeedHitTest(mApp->mWidgetManager->mLastMouseX - mX, mApp->mWidgetManager->mLastMouseY - mY);
	if (aSeedType == SeedType::SEED_NONE)
	{
		mToolTip->mVisible = false;
		mToolTipSeed = SeedType::SEED_NONE;
	}
	else if (aSeedType != mToolTipSeed)
	{
		mToolTip->mVisible = false;
		mToolTipSeed = SeedType::SEED_NONE;
		uint aRecFlags = mApp->mSeedChooserScreen->SeedNotRecommendedToPick(aSeedType);
		if (mApp->mSeedChooserScreen->SeedNotAllowedToPick(aSeedType))  // �������Я��
		{
			mToolTip->SetWarningText(gStr.NOT_ALLOWED_ON_THIS_LEVEL);
		}
		else if (aRecFlags)  // ������Ƽ�Я��
		{
			if (TestBit(aRecFlags, NotRecommend::NOT_RECOMMENDED_NOCTURNAL))
			{
				mToolTip->SetWarningText(gStr.NOCTURNAL_WARNING);
			}
			else
			{
				mToolTip->SetWarningText(gStr.NOT_RECOMMENDED_FOR_LEVEL);
			}
		}
		else
		{
			mToolTip->SetWarningText((""));
		}
		mToolTip->SetTitle(Plant::GetNameString(SeedType::SEED_IMITATER, aSeedType));
		mToolTip->SetLabel(Plant::GetToolTip(aSeedType));

		int aSeedX, aSeedY;
		GetSeedPosition(aSeedType, aSeedX, aSeedY);
		mToolTip->mX = ((mLandscapePicker?81:SEED_PACKET_WIDTH) - mToolTip->mWidth) / 2 + aSeedX;
		mToolTip->mY = (mLandscapePicker?53:SEED_PACKET_HEIGHT) + aSeedY;
		mToolTip->mVisible = true;
		mToolTipSeed = aSeedType;
	}
}

//0x483270
void ImitaterDialog::MouseDown(int x, int y, int theClickCount)
{
    if(mLandscapePicker)mCardScroll.Cancel();
	SeedType aSeedType = SeedHitTest(x, y);
	if (aSeedType != SeedType::SEED_NONE)
	{
		SeedChooserScreen* aSeedChooser = mApp->mSeedChooserScreen;
		if (!aSeedChooser->SeedNotAllowedToPick(aSeedType))
		{
			ChosenSeed& aImitater = aSeedChooser->mChosenSeeds[SeedType::SEED_IMITATER];
			aImitater.mSeedState = ChosenSeedState::SEED_IN_CHOOSER;
			aImitater.mImitaterType = aSeedType;
			aImitater.mX = aSeedChooser->mImitaterButton->mX;
			aImitater.mY = aSeedChooser->mImitaterButton->mY;
			aSeedChooser->ClickedSeedInChooser(aImitater);
			aSeedChooser->UpdateImitaterButton();
			mApp->KillDialog(mId);
		}
	}
	else
	{
		Widget::MouseDown(x, y, theClickCount);
	}
}

void ImitaterDialog::MouseUp(int,int,int) {}
void ImitaterDialog::MouseWheel(int delta) {
    if(!mLandscapePicker)return;
    mCardScroll.Cancel();mCardScroll.position=std::clamp(mCardScroll.position+delta*55.0f,mCardScrollMinimum,0.0f);MarkDirty();
}
void ImitaterDialog::TouchBegan(const TouchEvent& event) {
    if(!mLandscapePicker) {LawnDialog::TouchBegan(event);return;}
    if(mCardScroll.pressed || !mCardViewport.Contains(event.x,event.y))return;
    mTouchSeed=SeedHitTest(event.x,event.y);
    const auto origin=GetAbsPos();mCardScroll.Begin(event.id,event.x+origin.mX,event.y+origin.mY,ScrollTimeNs());
}
void ImitaterDialog::TouchMoved(const TouchEvent& event) {
    if(!mLandscapePicker) {LawnDialog::TouchMoved(event);return;}
    const auto origin=GetAbsPos();
    mCardScroll.Move(event.id,event.x+origin.mX,event.y+origin.mY,ScrollTimeNs(),mCardScrollMinimum,0);MarkDirty();
}
void ImitaterDialog::TouchEnded(const TouchEvent& event) {
    if(!mLandscapePicker) {LawnDialog::TouchEnded(event);return;}
    if(!mCardScroll.pressed||mCardScroll.touchId!=event.id)return;
    TouchMoved(event);const bool moved=mCardScroll.End(event.id,ScrollTimeNs());
    const bool select=!moved&&mTouchSeed!=SEED_NONE&&SeedHitTest(event.x,event.y)==mTouchSeed;
    mTouchSeed=SEED_NONE;
    if(select)MouseDown(event.x,event.y,1);
}
void ImitaterDialog::TouchCanceled(const TouchEvent& event) {
    if(!mLandscapePicker) {LawnDialog::TouchCanceled(event);return;}
    if(mCardScroll.pressed&&mCardScroll.touchId==event.id){mCardScroll.Cancel();mTouchSeed=SEED_NONE;}
}
