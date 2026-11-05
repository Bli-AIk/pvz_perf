#include "Sexy.TodLib/generated/EntityStrings.h"
#include "Lawn/Board.h"
#include "GameButton.h"
#include "Lawn/Cutscene.h"
#include "Almanac.h"
#include "Lawn/LawnCommon.h"
#include "LawnApp.h"
#include "Lawn/System/Music.h"
#include "Resources.h"
#include "NewOptionsDialog.h"
#include "ConstEnums.h"
#include "Sexy.TodLib/TodFoley.h"
#include "widget/Slider.h"
#include "widget/Checkbox.h"
#include "widget/WidgetLifetimeHold.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "pvz/BuildConfig.h"

using namespace Sexy;

//0x45C050
NewOptionsDialog::NewOptionsDialog(LawnApp* theApp, bool theFromGameSelector) : 
	Dialog(nullptr, nullptr, Dialogs::DIALOG_NEWOPTIONS, true, ("Options"), (""), (""), Dialog::BUTTONS_NONE)
{
    mApp = theApp;
    mFromGameSelector = theFromGameSelector;
    SetColor(Dialog::COLOR_BUTTON_TEXT, Color(255, 255, 100));
    mAlmanacButton = MakeButton(NewOptionsDialog::NewOptionsDialog_Almanac, this, gStr.VIEW_ALMANAC_BUTTON);
	// 1073's string table exposes the label as RESTART_LEVEL. The former key
	// leaked the literal fallback "restart_level_b" into the in-level menu.
	mRestartButton = MakeButton(NewOptionsDialog::NewOptionsDialog_Restart, this, gStr.RESTART_LEVEL);
    mBackToMainButton = MakeButton(NewOptionsDialog::NewOptionsDialog_MainMenu, this, gStr.MAIN_MENU_BUTTON);

    mBackToGameButton = MakeNewButton(
        Dialog::ID_OK, 
        this, 
        gStr.BACK_TO_GAME, 
        nullptr, 
        IMAGE_OPTIONS_BACKTOGAMEBUTTON0, 
        IMAGE_OPTIONS_BACKTOGAMEBUTTON0, 
        IMAGE_OPTIONS_BACKTOGAMEBUTTON2
    );
    mBackToGameButton->mTranslateX = 0;
    mBackToGameButton->mTranslateY = 0;
    mBackToGameButton->mTextOffsetX = -2;
    mBackToGameButton->mTextOffsetY = -5;
    mBackToGameButton->mTextDownOffsetX = 0;
    mBackToGameButton->mTextDownOffsetY = 1;
    mBackToGameButton->SetFont(FONT_DWARVENTODCRAFT36GREENINSET);
    mBackToGameButton->SetColor(ButtonWidget::COLOR_LABEL, Color::White);
    mBackToGameButton->SetColor(ButtonWidget::COLOR_LABEL_HILITE, Color::White);
    mBackToGameButton->mHiliteFont = FONT_DWARVENTODCRAFT36BRIGHTGREENINSET;
    
    mMusicVolumeSlider = new Slider(IMAGE_OPTIONS_SLIDERSLOT, IMAGE_OPTIONS_SLIDERKNOB2, NewOptionsDialog::NewOptionsDialog_MusicVolume, this);
    double aMusicVolume = theApp->mMusicVolume;
    aMusicVolume = std::max(0.0, std::min(1.0, aMusicVolume));
    mMusicVolumeSlider->SetValue(aMusicVolume);

    mSfxVolumeSlider = new Slider(IMAGE_OPTIONS_SLIDERSLOT, IMAGE_OPTIONS_SLIDERKNOB2, NewOptionsDialog::NewOptionsDialog_SoundVolume, this);
    mSfxVolumeSlider->SetValue(theApp->mSfxVolume / 0.65);

#if defined(__ANDROID__)
    mFullscreenCheckbox = MakeNewCheckbox(NewOptionsDialog::NewOptionsDialog_Fullscreen, this, false);
    mFullscreenCheckbox->mDisabled = true;
#else
    mFullscreenCheckbox = MakeNewCheckbox(NewOptionsDialog::NewOptionsDialog_Fullscreen, this, !theApp->mIsWindowed);
#endif
    // 3D (D3D9) acceleration is always on. This checkbox now controls custom
    // scaling (scale.json): OFF = native 800x600, ON = config-driven.
    mScaleCheckbox = MakeNewCheckbox(NewOptionsDialog::NewOptionsDialog_Scale, this, theApp->mUseCustomScale);
    if constexpr (BuildConfig::Window == BuildConfig::WindowSystem::Browser) {
        mScaleCheckbox->SetChecked(false, false);
        mScaleCheckbox->mDisabled = true;
    }

    if (mFromGameSelector)
    {
        mRestartButton->SetVisible(false);
        mBackToGameButton->mLabel = gStr.DIALOG_BUTTON_OK;
        if (mApp->HasFinishedAdventure() && !mApp->IsTrialStageLocked())
        {
            mBackToMainButton->mLabel = gStr.CREDITS;
        }
        else
        {
            mBackToMainButton->SetVisible(false);
        }
    }

    if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ICE || 
        mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN || 
        mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
    {
        mRestartButton->SetVisible(false);
    }
    if (mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO && !mApp->mBoard->mCutScene->IsSurvivalRepick())
    {
        mRestartButton->SetVisible(false);
    }
    if (!mApp->CanShowAlmanac() || 
        mApp->mGameScene == GameScenes::SCENE_LEVEL_INTRO || 
        mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN ||
        mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM || 
        mFromGameSelector)
    {
        mAlmanacButton->SetVisible(false);
    }
}

//0x45C760、0x45C780
NewOptionsDialog::~NewOptionsDialog()
{
    delete mMusicVolumeSlider;
    delete mSfxVolumeSlider;
    delete mFullscreenCheckbox;
    delete mScaleCheckbox;
    delete mAlmanacButton;
    delete mRestartButton;
    delete mBackToMainButton;
    delete mBackToGameButton;
}

//0x45C880
int NewOptionsDialog::GetPreferredHeight(int theWidth)
{
    (void)theWidth;
    return IMAGE_OPTIONS_MENUBACK->mWidth;
}

//0x45C890
void NewOptionsDialog::AddedToManager(Sexy::WidgetManager* theWidgetManager)
{
    Dialog::AddedToManager(theWidgetManager);
    AddWidget(mAlmanacButton);
    AddWidget(mRestartButton);
    AddWidget(mBackToMainButton);
    AddWidget(mMusicVolumeSlider);
    AddWidget(mSfxVolumeSlider);
    AddWidget(mScaleCheckbox);
    AddWidget(mFullscreenCheckbox);
    AddWidget(mBackToGameButton);
}

//0x45C930
void NewOptionsDialog::RemovedFromManager(Sexy::WidgetManager* theWidgetManager)
{
    Dialog::RemovedFromManager(theWidgetManager);
    RemoveWidget(mAlmanacButton);
    RemoveWidget(mMusicVolumeSlider);
    RemoveWidget(mSfxVolumeSlider);
    RemoveWidget(mFullscreenCheckbox);
    RemoveWidget(mScaleCheckbox);
    RemoveWidget(mBackToMainButton);
    RemoveWidget(mBackToGameButton);
    RemoveWidget(mRestartButton);
}

//0x45C9D0
void NewOptionsDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
    Dialog::Resize(theX, theY, theWidth, theHeight);
    mMusicVolumeSlider->Resize(199, 116, 135, 40);
    mSfxVolumeSlider->Resize(199, 143, 135, 40);
    mScaleCheckbox->Resize(283, 175, 46, 45);
    mFullscreenCheckbox->Resize(284, 206, 46, 45);
    mAlmanacButton->Resize(107, 241, 209, 46);
    mRestartButton->Resize(mAlmanacButton->mX, mAlmanacButton->mY + 43, 209, 46);
    mBackToMainButton->Resize(mRestartButton->mX, mRestartButton->mY + 43, 209, 46);
    mBackToGameButton->Resize(30, 381, mBackToGameButton->mWidth, mBackToGameButton->mHeight);

    if (mFromGameSelector)
    {
        mMusicVolumeSlider->mY += 5;
        mSfxVolumeSlider->mY += 10;
        mScaleCheckbox->mY += 15;
        mFullscreenCheckbox->mY += 20;
    }

    if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
    {
        mAlmanacButton->mY += 43;
    }
}

//0x45CB50
void NewOptionsDialog::Draw(Sexy::Graphics* g)
{
    g->DrawImage(IMAGE_OPTIONS_MENUBACK, 0, 0);

    int aMusicOffset = 0;
    int aSfxOffset = 0;
    int a3DAccelOffset = 0;
    int aFullScreenOffset = 0;
    if (mFromGameSelector)
    {
        aMusicOffset = 5;
        aSfxOffset = 10;
        a3DAccelOffset = 15;
        aFullScreenOffset = 20;
    }
    Sexy::Color aTextColor(107, 109, 145);

    TodDrawString(g, ("Music"), 186, 140 + aMusicOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
    TodDrawString(g, ("Sound FX"), 186, 167 + aSfxOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
    TodDrawString(g, ("Custom Scale"), 274, 197 + a3DAccelOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
    TodDrawString(g, gStr.OPTIONS_DISPLAY_MODE, 274, 229 + aFullScreenOffset, FONT_DWARVENTODCRAFT18, aTextColor, DrawStringJustification::DS_ALIGN_RIGHT);
}

//0x45CF50
void NewOptionsDialog::SliderVal(int theId, double theVal)
{
    switch (theId)
    {
    case NewOptionsDialog::NewOptionsDialog_MusicVolume:
        mApp->SetMusicVolume(theVal);
        mApp->mSoundSystem->RehookupSoundWithMusicVolume();
        break;

    case NewOptionsDialog::NewOptionsDialog_SoundVolume:
        mApp->SetSfxVolume(theVal * 0.65);
        mApp->mSoundSystem->RehookupSoundWithMusicVolume();
        if (!mSfxVolumeSlider->mDragging)
        {
            mApp->PlaySample(SOUND_BUTTONCLICK);
        }
        break;
    }
}

//0x45CFF0
void NewOptionsDialog::CheckboxChecked(int theId, bool checked)
{
    switch (theId)
    {
    case NewOptionsDialog::NewOptionsDialog_Fullscreen:
#if defined(__ANDROID__)
        // This widget is HDR on Android. Never feed it into windowed/fullscreen.
        break;
#else
        if (!checked && mApp->mForceFullscreen)
        {
            mApp->DoDialog(
                Dialogs::DIALOG_COLORDEPTH_EXP, 
                true, 
                ("No Windowed Mode"), 
                ( "Windowed mode is only available if your desktop was running in either\n"
                    "16 bit or 32 bit color mode when you started the game.\n\n"
                    "If you'd like to run in Windowed mode then you need to quit the game and switch your desktop to 16 or 32 bit color mode."), 
                ("OK"), 
                Dialog::BUTTONS_FOOTER
            );

            mFullscreenCheckbox->SetChecked(true, false);
        }
        break;
#endif

    case NewOptionsDialog::NewOptionsDialog_Scale:
        // The 2x scale selection is applied when the dialog is closed
        // (KillNewOptionsDialog -> mWindowScale).
        if (checked)
            mApp->PlaySample(SOUND_BUTTONCLICK);
        break;
    }
}

//0x45D290
void NewOptionsDialog::KeyDown(Sexy::KeyCode theKey)
{
    if (mApp->mBoard)
    {
        mApp->mBoard->DoTypingCheck(theKey);
    }

    if (theKey == KeyCode::KEYCODE_SPACE || theKey == KeyCode::KEYCODE_RETURN)
    {
        Dialog::ButtonDepress(Dialog::ID_OK);
    }
    else if (theKey == KeyCode::KEYCODE_ESCAPE)
    {
        Dialog::ButtonDepress(Dialog::ID_CANCEL);
    }
}

//0x45D2F0
void NewOptionsDialog::ButtonPress(int theId)
{
    (void)theId;
    mApp->PlaySample(SOUND_GRAVEBUTTON);
}

//0x45D310
void NewOptionsDialog::ButtonDepress(int theId)
{
    WidgetLifetimeHold aCallLifetime(*this);
    Dialog::ButtonDepress(theId);

    switch (theId)
    {
    case NewOptionsDialog::NewOptionsDialog_Almanac:
    {
        AlmanacDialog* aDialog = mApp->DoAlmanacDialog(SeedType::SEED_NONE, ZombieType::ZOMBIE_INVALID);
        aDialog->WaitForResult(true);
        break;
    }

    case NewOptionsDialog::NewOptionsDialog_MainMenu:
    {
        if (mFromGameSelector)
        {
            mApp->KillNewOptionsDialog();
            mApp->KillGameSelector();
            mApp->ShowAwardScreen(AwardType::AWARD_CREDITS_ZOMBIENOTE, false);
        }
        else if (mApp->mBoard && mApp->mBoard->NeedSaveGame())
        {
            mApp->DoConfirmBackToMain();
        }
        else if (mApp->mBoard && mApp->mBoard->mCutScene && mApp->mBoard->mCutScene->IsSurvivalRepick())
        {
            mApp->DoConfirmBackToMain();
        }
        else
        {
            mApp->mBoardResult = BoardResult::BOARDRESULT_QUIT;
            mApp->DoBackToMain();
        }
        break;
    }

    case NewOptionsDialog::NewOptionsDialog_Restart:
    {
        const auto aBoardRevision = mApp->mBoardRevision;
        const auto* aPlayer = mApp->mPlayerInfo;
        if (mApp->mBoard)
        {
            SexyString aDialogTitle;
            SexyString aDialogMessage;
            if (mApp->IsPuzzleMode())
            {
                aDialogTitle = gStr.RESTART_PUZZLE_HEADER;
                aDialogMessage = gStr.RESTART_PUZZLE_BODY;
            }
            else if (mApp->IsChallengeMode())
            {
                aDialogTitle = gStr.RESTART_CHALLENGE_HEADER;
                aDialogMessage = gStr.RESTART_CHALLENGE_BODY;
            }
            else if (mApp->IsSurvivalMode())
            {
                aDialogTitle = gStr.RESTART_SURVIVAL_HEADER;
                aDialogMessage = gStr.RESTART_SURVIVAL_BODY;
            }
            else
            {
                aDialogTitle = gStr.RESTART_LEVEL_HEADER;
                aDialogMessage = gStr.RESTART_LEVEL_BODY;
            }

            LawnDialog* aDialog = (LawnDialog*)mApp->DoDialog(Dialogs::DIALOG_CONFIRM_RESTART, true, aDialogTitle, aDialogMessage, (""), Dialog::BUTTONS_YES_NO);
		aDialog->mYesButton->mLabel = gStr.RESTART_LEVEL;
            aDialog->mNoButton->mLabel = gStr.DIALOG_BUTTON_CANCEL;
            
            if (aDialog->WaitForResult(true) == Dialog::ID_YES &&
                aBoardRevision == mApp->mBoardRevision &&
                aPlayer == mApp->mPlayerInfo &&
                mApp->GetDialog(Dialogs::DIALOG_NEWOPTIONS) == this)
            {
                mApp->mMusic->StopAllMusic();
                mApp->mSoundSystem->CancelPausedFoley();
                mApp->KillNewOptionsDialog();
                mApp->mBoardResult = BoardResult::BOARDRESULT_RESTART;
                mApp->mSawYeti = mApp->mBoard->mKilledYeti;
                mApp->PreNewGame(mApp->mGameMode, false);
            }
        }
        break;
    }

    case NewOptionsDialog::NewOptionsDialog_Update:
        break;
    }
}
