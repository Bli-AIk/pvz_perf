#include "OpeningFlowRuntimeChecks.h"
#include "LawnApp.h"
#include "Lawn/Board.h"
#include "Lawn/Cutscene.h"
#include "Lawn/Plant.h"
#include "Lawn/SeedPacket.h"
#include "Lawn/System/ReanimationLawn.h"
#include "Lawn/System/PlayerInfo.h"
#include "Lawn/System/PoolEffect.h"
#include "Lawn/ZenGarden.h"
#include "Lawn/Widget/GameButton.h"
#include "Lawn/Widget/LawnDialog.h"
#include "Lawn/Widget/SeedChooserScreen.h"
#include "Lawn/Widget/StoreScreen.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/Reanimator.h"
#include "SexyAppFramework/graphics/AtlasDraw.h"
#include "SexyAppFramework/graphics/DDInterface.h"
#include "SexyAppFramework/graphics/Graphics.h"
#include "SexyAppFramework/graphics/RenderInterface.h"
#include "SexyAppFramework/misc/MTRand.h"
#include "SexyAppFramework/widget/WidgetManager.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace
{
using namespace Sexy;

void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

struct Cleanup
{
    LawnApp& app;
    std::function<bool(bool*)>& pump;
    void (*deletes)(LawnApp&);
    PlayerInfo* player=app.mPlayerInfo;
    ZenGarden* garden=app.mZenGarden;
    PoolEffect* pool=app.mPoolEffect;
    GameMode mode=app.mGameMode;
    GameScenes scene=app.mGameScene;
    Rect bounds=app.mScreenBounds;
    BoardResult result=app.mBoardResult;
    unsigned long long interval=gUpdateIntervalNs, legacyPhase=gLegacyTickPhaseNs;
    int whole=gWholeLegacyTicks;
    double ticks=gUpdateTicks,steps=gStepsPerLegacyTick;
    std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
    std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;

    void Close()
    {
        pump={};
        app.KillDialog(DIALOG_PURCHASE_PACKET_SLOT);
        app.KillDialog(DIALOG_CHOOSER_WARNING);
        app.KillBoard();
        deletes(app);
    }

    ~Cleanup()
    {
        Close();
        app.mPlayerInfo=player;app.mZenGarden=garden;app.mPoolEffect=pool;
        app.mGameMode=mode;app.mGameScene=scene;app.mScreenBounds=bounds;app.mBoardResult=result;
        gUpdateIntervalNs=interval;gLegacyTickPhaseNs=legacyPhase;gWholeLegacyTicks=whole;
        gUpdateTicks=ticks;gStepsPerLegacyTick=steps;
        app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
        app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
    }
};
}

void CheckOpeningFlows(LawnApp& app, std::function<bool(bool*)>& nestedPump,
    void (*processDeletes)(LawnApp&))
{
    using namespace Sexy;
    Check(!app.mBoard&&!app.mSeedChooserScreen,"Opening fixture requires an empty Board route");
    Cleanup restore{app,nestedPump,processDeletes};
    app.mReanimatorCache->PrebuildCacheFrames();
    constexpr std::array<unsigned long long,7> jitter{
        1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    auto& render=*app.mDDInterface->mRenderInterface;
    struct Target {Image* image;~Target(){delete image;}} target{render.CreateRenderImage(1400,600)};
    bool passed=true;
    int cases=0;
    const auto run=[&](GameMode mode,int level,bool replay,int stock,int answer,int hz)
    {
        PlayerInfo profile;ZenGarden garden;PoolEffect pool;pool.PoolEffectInitialize();
        profile.mId=50;profile.mLevel=level;profile.mFinishedAdventure=replay?1:0;
        profile.mCoins=answer>=0?99999:0;
        profile.mPurchases[STORE_ITEM_PACKET_UPGRADE]=stock;
        std::fill(std::begin(profile.mEarnedAchievements),std::end(profile.mEarnedAchievements),true);
        std::fill(std::begin(profile.mShownAchievements),std::end(profile.mShownAchievements),true);
        app.mPlayerInfo=&profile;app.mZenGarden=&garden;app.mPoolEffect=&pool;
        app.mScreenBounds=Rect(-300,0,1400,600);
        clocks.mVisualTimeNs=456'789'012'000;clocks.mBattleTimeNs=0;
        gLegacyTickPhaseNs=0;SRand(0x0F1000+level+int(mode)*100);
        int sequence=0,offers=0,warnings=0,dialogue=0,shovels=0;
        unsigned long long elapsed=0,nestedElapsed=0,lastStep=0;
        const auto nextStep=[&]() {return hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()];};
        const auto draw=[&]() {
            GpuImageDrawScope scope(render,target.image);Graphics graphics(target.image);
            graphics.Translate(app.mBoard->mX-app.mScreenBounds.mX,app.mBoard->mY-app.mScreenBounds.mY);
            app.mBoard->Draw(&graphics);
        };
        nestedPump=[&](bool* updated) {
            nestedElapsed+=nextStep();
            Check(nestedElapsed<5'000'000'000ULL,"Opening confirmation did not finish within five simulated seconds");
            auto* dialog=static_cast<LawnDialog*>(app.GetDialog(DIALOG_PURCHASE_PACKET_SLOT));
            if(dialog) {
                ++offers;
                if(answer==2) profile.mCoins=0; // Balance can change while the offer is pending.
                dialog->ButtonDepress(answer==1?Dialog::ID_NO:Dialog::ID_YES);
            } else if((dialog=static_cast<LawnDialog*>(app.GetDialog(DIALOG_CHOOSER_WARNING)))) {
                ++warnings;dialog->ButtonDepress(Dialog::ID_YES);
            } else throw std::runtime_error("Opening waited on an unexpected dialog");
            if(updated) *updated=true;
            processDeletes(app);return true;
        };
        try {
            std::cout<<"Opening begin mode="<<int(mode)<<" level="<<level<<" replay="<<replay
                <<" stock="<<stock<<" answer="<<answer<<" Hz="<<hz<<'\n';
            app.PreNewGame(mode,false);
            auto& board=*app.mBoard;
            auto& movie=*board.mCutScene;
            const int initialDave=movie.mCrazyDaveDialogStart;
            const bool expectChooser=board.ChooseSeedsOnCurrentLevel();
            bool chooserUsed=false;
            draw(); // CutScene deliberately waits until its Board has been drawn once.
            while(app.mGameScene==SCENE_LEVEL_INTRO&&elapsed<60'000'000'000ULL)
            {
                Check(board.mLevelTimeNs==0,"Opening advanced the Battle clock before StartPlaying");
                if(movie.IsInShovelTutorial()) {
                    const auto shovel=board.GetShovelButtonRect();
                    board.MouseDown(shovel.mX+shovel.mWidth/2,shovel.mY+shovel.mHeight/2,1);
                    if(board.mTutorialState==TUTORIAL_SHOVEL_DIG||board.mTutorialState==TUTORIAL_SHOVEL_KEEP_DIGGING) {
                        Plant* plant=nullptr;
                        Check(board.IteratePlants(plant),"Shovel tutorial has no plant to dig");
                        board.MouseDown(int(plant->mX)+40,int(plant->mY)+40,1);
                        ++shovels;
                    }
                }
                else if(app.mCrazyDaveMessageIndex!=-1&&movie.IsShowingCrazyDave()) {
                    movie.KeyDown(KEYCODE_SPACE);++dialogue;
                }
                else if(movie.mSeedChoosing) {
                    auto* chooser=app.mSeedChooserScreen;
                    Check(chooser&&chooser->mMouseVisible,"CutScene chose seeds without its visible chooser");
                    chooserUsed=true;
                    for(int seed=0;seed<NUM_SEEDS_IN_CHOOSER&&chooser->mSeedsInBank<board.mSeedBank->mNumPackets;++seed)
                        if(app.SeedTypeAvailable(SeedType(seed))&&
                           chooser->mChosenSeeds[seed].mSeedState==SEED_IN_CHOOSER&&
                           !chooser->SeedNotAllowedToPick(SeedType(seed)))
                            chooser->ClickedSeedInChooser(chooser->mChosenSeeds[seed]);
                    Check(chooser->mSeedsInBank==board.mSeedBank->mNumPackets,"Available chooser cards cannot fill the bank");
                    if(chooser->mSeedsInFlight==0) chooser->ButtonDepress(chooser->mStartButton->mId);
                }
                lastStep=nextStep();ApplySimulationStep(lastStep);app.UpdateFrames();elapsed+=lastStep;
                processDeletes(app);
                Check(app.mBoard==&board,"Opening replaced its Board unexpectedly");
            }
            if(app.mGameScene!=SCENE_PLAYING) {
                std::cerr<<"Opening timeout scene="<<int(app.mGameScene)<<" movie="<<movie.mCutsceneTime.mNanoseconds
                    <<" dave="<<app.mCrazyDaveMessageIndex<<" tutorial="<<int(board.mTutorialState)
                    <<" choosing="<<movie.mSeedChoosing<<'\n';
                throw std::runtime_error("Opening failed to reach Playing within sixty simulated seconds");
            }
            Check(chooserUsed==expectChooser,"Opening skipped or invented its seed selection");
            Check(!app.mSeedChooserScreen,"Opening retained its chooser after StartPlaying");
            Check(board.mLevelTimeNs<=lastStep*(mode==GAMEMODE_MINIGAME_SPEED?2:1),
                "Opening leaked the introduction age into Battle time");
            Check(initialDave==-1||dialogue>0,"Opening bypassed its authored Dave dialogue");
            if(mode==GAMEMODE_ADVENTURE&&level==5&&!replay)
                Check(shovels>0&&board.mTutorialState==TUTORIAL_SHOVEL_COMPLETED,"First bowling opening bypassed shovel teaching");
            if(answer>=0) {
                const bool expectOffer=!(level==13&&stock==1);
                Check(offers==int(expectOffer),"Opening offered the wrong packet upgrade");
                const bool purchased=expectOffer&&answer==0;
                Check(profile.mPurchases[STORE_ITEM_PACKET_UPGRADE]==stock+int(purchased),"Packet confirmation granted the wrong stock");
                Check(profile.mDidntPurchasePacketUpgrade==int(expectOffer&&!purchased),"Packet rejection count is incorrect");
                Check(board.mSeedBank->mNumPackets==6+stock+int(purchased),"Packet purchase did not resize the bank");
                if(expectOffer&&answer==1) Check(profile.mCoins==99999,"Declined packet offer consumed coins");
                if(expectOffer&&answer==2) Check(profile.mCoins==0,"Unaffordable packet offer changed the zero balance");
                if(purchased) Check(profile.mCoins<99999&&profile.mCoins>=0,"Accepted packet offer failed to deduct its price");
            }
            draw();
            std::cout<<"Opening primary mode="<<int(mode)<<" level="<<level<<" replay="<<replay
                <<" stock="<<stock<<" answer="<<answer<<" Hz="<<hz<<" dave="<<initialDave
                <<" dialogue="<<dialogue<<" chooser="<<chooserUsed<<" offers="<<offers<<" warnings="<<warnings
                <<" shovels="<<shovels<<" elapsedNs="<<elapsed<<" ok=1\n";
        } catch(const std::exception& error) {
            passed=false;std::cerr<<"Opening failed mode="<<int(mode)<<" level="<<level<<" replay="<<replay
                <<" stock="<<stock<<" answer="<<answer<<" Hz="<<hz<<" message="<<error.what()<<'\n';
        }
        restore.Close();++cases;
    };
    for(bool replay:{false,true})
        for(int level=1;level<=50;++level)
            for(int hz:{100,30,240,0}) run(GAMEMODE_ADVENTURE,level,replay,0,-1,hz);
    for(int mode=1;mode<=int(GAMEMODE_TREE_OF_WISDOM);++mode)
        if(mode!=int(GAMEMODE_INTRO))
            for(int hz:{100,30,240,0}) run(GameMode(mode),50,true,0,-1,hz);
    for(const auto [level,stock]:{std::array{13,0},std::array{16,0},std::array{16,1}})
        for(int answer:{0,1,2})
            for(int hz:{100,30,240,0}) run(GAMEMODE_ADVENTURE,level,false,stock,answer,hz);
    for(int hz:{100,30,240,0}) run(GAMEMODE_ADVENTURE,13,false,1,0,hz);
    Check(cases==720,"Opening scenario coverage changed unexpectedly");
    Check(passed,"Opening routes have failed cases; inspect per-case output");
    std::cout<<"PASS actual opening flows: 720 contexts, authored Dave inputs, shovel, chooser and packet confirmations\n";
}
