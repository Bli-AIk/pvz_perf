#include "Sexy.TodLib/generated/EntityStrings.h"
#include "Lawn/LawnCommon.h"
#include "Lawn/Board.h"
#include "Lawn/Zombie.h"
#include "GameButton.h"
#include "LawnApp.h"
#include "ContinueDialog.h"
#include "Resources.h"
#include "Sexy.TodLib/TodFoley.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "Sexy.TodLib/generated/SceneAtlasData.h"

//0x4330D0
// GOTY @Patoke: 0x435E40
// @Patoke: these dialogs don't have localizations
ContinueDialog::ContinueDialog(LawnApp* theApp) : LawnDialog(
	theApp, 
	Dialogs::DIALOG_CONTINUE, 
	true, 
	("CONTINUE GAME?"), 
	(""), 
	gStr.DIALOG_BUTTON_CANCEL, 
	Dialog::BUTTONS_FOOTER)
{
    if (theApp->IsAdventureMode())
    {
        mDialogLines = ("Do you want to continue your current game or restart the level?");
        mContinueButton = MakeButton(ContinueDialog::ContinueDialog_Continue, this, gStr.CONTINUE_BUTTON);
        mNewGameButton = MakeButton(ContinueDialog::ContinueDialog_NewGame, this, gStr.RESTART_LEVEL);
    }
    else
    {
        mDialogLines = ("Do you want to continue your current game or start a new game?");
        mContinueButton = MakeButton(ContinueDialog::ContinueDialog_Continue, this, gStr.CONTINUE_BUTTON);
        mNewGameButton = MakeButton(ContinueDialog::ContinueDialog_NewGame, this, gStr.NEW_GAME_BUTTON);
    }

    mTallBottom = true;
    CalcSize(10, 60);
}

//0x4333D0、0x4333F0
ContinueDialog::~ContinueDialog()
{
    delete mContinueButton;
    delete mNewGameButton;
}

//0x433470
int ContinueDialog::GetPreferredHeight(int theWidth)
{
    return LawnDialog::GetPreferredHeight(theWidth) + 40;
}

//0x433480
void ContinueDialog::Resize(int theX, int theY, int theWidth, int theHeight)
{
    LawnDialog::Resize(theX, theY, theWidth, theHeight);

    int aBtnWidth = CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::BUTTON_LEFT)].mWidth +
        CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::BUTTON_MIDDLE)].mWidth * 3 +
        CommonUIAtlas::Images[size_t(CommonUIAtlas::Sprite::BUTTON_RIGHT)].mWidth;
    int aBtnHeight = mYesButton->mHeight;

    /*
    int aContinueX = mYesButton->mX - 20;
    int aNewGameX = mYesButton->mX + mYesButton->mWidth - aBtnWidth + 24;
    while (aContinueX + aBtnWidth > aNewGameX)
    {
        aContinueX -= 20;
        aNewGameX += 20;
    }

    mContinueButton->Resize(aContinueX, mYesButton->mY - aBtnHeight - 2, aBtnWidth, aBtnHeight);
    mNewGameButton->Resize(aNewGameX, mContinueButton->mY, aBtnWidth, aBtnHeight);
    mYesButton->Resize(theWidth / 2 - aBtnWidth / 2, mYesButton->mY, aBtnWidth, aBtnHeight);
    */

    mContinueButton->Resize(mYesButton->mX - 20, mYesButton->mY - aBtnHeight, aBtnWidth, aBtnHeight);
    mNewGameButton->Resize(mYesButton->mX + mYesButton->mWidth - aBtnWidth + 20, mContinueButton->mY, aBtnWidth, aBtnHeight);
}

//0x433520
void ContinueDialog::AddedToManager(WidgetManager* theWidgetManager)
{
    LawnDialog::AddedToManager(theWidgetManager);
    AddWidget(mContinueButton);
    AddWidget(mNewGameButton);
}

//0x433590
void ContinueDialog::RemovedFromManager(WidgetManager* theWidgetManager)
{
    LawnDialog::RemovedFromManager(theWidgetManager);
    RemoveWidget(mContinueButton);
    RemoveWidget(mNewGameButton);
}

//0x4335D0
void ContinueDialog::RestartLoopingSounds()
{
    if (mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_RAINING_SEEDS || mApp->IsStormyNightLevel())
    {
        mApp->PlayFoley(FoleyType::FOLEY_RAIN);
    }

    Zombie* aZombie = nullptr;
    while (mApp->mBoard->IterateZombies(aZombie))
    {
        if (aZombie->mPlayingSong)
        {
            aZombie->StartZombieSound();
        }
    }
}

//0x4336C0
void ContinueDialog::ButtonDepress(int theId)
{
    if (theId == ContinueDialog::ContinueDialog_Continue)
    {
        if (mApp->mBoard->mNextSurvivalStageCounter != 1)
        {
            std::string aFileName = GetSavedGameName(mApp->mGameMode, mApp->mPlayerInfo->mId);
            mApp->EraseFile(aFileName);
        }

        RestartLoopingSounds();
        mApp->KillDialog(mId);
    }
    else if (theId == ContinueDialog::ContinueDialog_NewGame)
    {
        if (mApp->IsAdventureMode())
        {
            LawnDialog* aDialog = (LawnDialog*)mApp->DoDialog(
                Dialogs::DIALOG_RESTARTCONFIRM, 
                true, 
                ("Restart Level?"), 
                ("Are you sure that you want to restart the level?"), 
                (""), 
                Dialog::BUTTONS_OK_CANCEL
            );
            aDialog->mYesButton->mLabel = gStr.RESTART_BUTTON;
            //aDialog->CalcSize(0, 0);
        }
        else
        {
            LawnDialog* aDialog = (LawnDialog*)mApp->DoDialog(
                Dialogs::DIALOG_RESTARTCONFIRM, 
                true, 
                ("New Game?"), 
                ("Are you sure that you want to start a new game?"), 
                (""), 
                Dialog::BUTTONS_OK_CANCEL
            );
            aDialog->mYesButton->mLabel = gStr.NEW_GAME_BUTTON;
            //aDialog->CalcSize(0, 0);
        }
    }
    else
    {
        mApp->KillDialog(mId);
        // Record an explicit cancellation. This is not what protects the save:
        // like 1073, TryLoadGame succeeds once this dialog is shown, and normal
        // interruption files are consumed by Continue, not by KillBoard.
        mApp->mBoardResult = BoardResult::BOARDRESULT_QUIT;
        if (mApp->IsAdventureMode())
        {
            mApp->ShowGameSelector();
        }
        else if (mApp->IsSurvivalMode())
        {
            mApp->KillBoard();
            mApp->ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_SURVIVAL);
        }
        else if (mApp->IsPuzzleMode())
        {
            mApp->KillBoard();
            mApp->ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_PUZZLE);
        }
        else
        {
            mApp->KillBoard();
            mApp->ShowChallengeScreen(ChallengePage::CHALLENGE_PAGE_CHALLENGE);
        }
    }
}
