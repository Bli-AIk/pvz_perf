#pragma once
#include <cstdint>
#include "misc/SexyVector.h"
#include "graphics/TextureRegion.h"
#include "widget/DialogButton.h"

//using namespace std;
using namespace Sexy;

class LawnApp;
namespace Sexy { struct TextureSource; }
class GameButton
{
public:
	enum
	{
		BUTTON_LABEL_LEFT = -1,
		BUTTON_LABEL_CENTER = 0,
		BUTTON_LABEL_RIGHT = 1
	};
	enum
	{
		COLOR_LABEL = 0,
		COLOR_LABEL_HILITE = 1,
		COLOR_DARK_OUTLINE = 2,
		COLOR_LIGHT_OUTLINE = 3,
		COLOR_MEDIUM_OUTLINE = 4,
		COLOR_BKG = 5,
		NUM_COLORS = 6
	};

public:
	LawnApp*				mApp;							//+0x0
	Widget*					mParentWidget;					//+0x4
	int						mX;								//+0x8
	int						mY;								//+0xC
	int						mWidth;							//+0x10
	int						mHeight;						//+0x14
	bool					mIsOver;						//+0x18
	bool					mIsDown;						//+0x19
	bool					mDisabled;						//+0x1A
	Color					mColors[6];						//+0x1C
	int						mId;							//+0x7C
	SexyString				mLabel;							//+0x80
	int						mLabelJustify;					//+0x9C
	_Font*					mFont;							//+0xA0
	Image*					mButtonImage;					//+0xA4
	Image*					mOverImage;						//+0xA8
	Image*					mDownImage;						//+0xAC
	Image*					mDisabledImage;					//+0xB0
	Image*					mOverOverlayImage;				//+0xB4
	TextureRegion			mNormalRegion{};
	TextureRegion			mOverRegion{};
	TextureRegion			mDownRegion{};
	TextureRegion			mDisabledRegion{};
	bool					mInverted;						//+0xF8
	bool					mBtnNoDraw;						//+0xF9
	bool					mFrameNoDraw;					//+0xFA
	double					mOverAlpha;						//+0x100
	double					mOverAlphaSpeed;				//+0x108
	double					mOverAlphaFadeInSpeed;
    double mOverAlphaOrigin = 0;
    double mOverAlphaRate = 0;
    std::int64_t mOverAlphaStartTimeNs = 0;
	bool					mDrawStoneButton;				//+0x118
	int						mTextOffsetX;					//+0x11C
	int						mTextOffsetY;					//+0x120
	int						mButtonOffsetX;					//+0x124
	int						mButtonOffsetY;					//+0x128

public:
	GameButton(int theId);
	~GameButton();

	static /*inline*/ bool	HaveButtonImage(Image* theImage, const TextureRegion& theRegion);
	void					DrawButtonImage(Graphics* g, Image* theImage, const TextureRegion& theRegion, int theX, int theY);
	/*inline*/ void			SetFont(_Font* theFont);
	/*inline*/ bool			IsButtonDown();
	void					Draw(Graphics* g);
	void                    DrawAtlas(Graphics* g, const Sexy::TextureSource& texture,
                                const TextureRegion* overlay = nullptr);
	/*inline*/ bool			IsMouseOver();
    bool HitTest(int x, int y, Widget* eventOwner = nullptr); // Null owner means window logical coordinates.
	void					Update();
};

class LawnStoneButton : public DialogButton
{
public:
	LawnStoneButton(Image* theComponentImage, int theId, ButtonListener* theListener) : DialogButton(theComponentImage, theId, theListener) { }

	virtual void			Draw(Graphics* g);
};

class NewLawnButton : public DialogButton
{
public:
    _Font*					mHiliteFont;            //+0x138
    int						mTextDownOffsetX;       //+0x13C
    int						mTextDownOffsetY;       //+0x140
	int						mButtonOffsetX;			//+0x144
	int						mButtonOffsetY;			//+0x148
	bool					mUsePolygonShape;		//+0x14C
	SexyVector2				mPolygonShape[4];		//+0x150

public:
    NewLawnButton(Image* theComponentImage, int theId, ButtonListener* theListener);
	virtual ~NewLawnButton();
	
    virtual void			Draw(Graphics* g);
	virtual bool			IsPointVisible(int x, int y);
	// @Patoke: user defined
};

LawnStoneButton*			MakeButton(int theId, ButtonListener* theListener, const SexyString& theText);
NewLawnButton*				MakeNewButton(int theId, ButtonListener* theListener, const SexyString& theText, _Font* theFont, Image* theImageNormal, Image* theImageOver, Image* theImageDown);
NewLawnButton* MakeCommonUIBackButton(int id, ButtonListener* listener, const SexyString& text);

