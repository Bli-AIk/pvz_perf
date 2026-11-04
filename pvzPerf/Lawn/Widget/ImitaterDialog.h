#pragma once
#include "LawnDialog.h"
#include "widget/InertialScroll.h"

class ToolTipWidget;
class ImitaterDialog : public LawnDialog
{
public:
    Rect mCardViewport;
    float mCardScrollMinimum = 0;
    void Resize(int x, int y, int width, int height) override;
    InertialScroll mCardScroll;
    bool mLandscapePicker = false;
    SeedType mTouchSeed = SEED_NONE;
    void MouseWheel(int delta) override;
    void TouchBegan(const TouchEvent& event) override;
    void TouchMoved(const TouchEvent& event) override;
    void TouchEnded(const TouchEvent& event) override;
    void TouchCanceled(const TouchEvent& event) override;
    ToolTipWidget*          mToolTip;
    SeedType                mToolTipSeed;

public:
    ImitaterDialog();
    virtual ~ImitaterDialog();

    SeedType                SeedHitTest(int x, int y);
    void                    UpdateCursor();
    virtual void            Update();
    /*inline*/ void         GetSeedPosition(int theIndex, int& x, int& y);
    virtual void            Draw(Graphics* g);
    void                    ShowToolTip();
    virtual void            MouseDown(int x, int y, int theClickCount);
    virtual void            MouseUp(int, int, int);
};

