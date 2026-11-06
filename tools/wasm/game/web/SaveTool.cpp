#include "SaveTool.h"
#include "LawnApp.h"
#include "Lawn/Board.h"
#include "Lawn/Plant.h"
#include "Lawn/GridItem.h"
#include "Lawn/LawnMower.h"
#include "Lawn/Challenge.h"
#include "Lawn/SeedPacket.h"
#include "Lawn/System/PlayerInfo.h"
#include "Lawn/System/SaveGame.h"
#include "Lawn/System/Music.h"
#include "Lawn/System/PoolEffect.h"
#include "Resources.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/TodParticle.h"
#include "Lawn/LawnCommon.h"
#include "paklib/PakInterface.h"
#include <nlohmann/json.hpp>
#include <emscripten.h>
#include <fstream>
#include <array>
#include <vector>
#include <stdexcept>
extern bool (*gAppCloseRequest)();
namespace {
using Json = nlohmann::json;
EM_ASYNC_JS(int, ReadMusicStart, (const unsigned char* data, int size, int order, float* tempo, float* speed), {
    try {
        const state=await parent.PVZSaveToolJob.readMusicStart(HEAPU8.slice(data,data+size),order);
        HEAPF32[tempo>>2]=state.tempo;HEAPF32[speed>>2]=state.speed;return 1;
    } catch(error) { console.error('Offline music initialization:',error);return 0; }
});
void ReleaseToolApp() {
    if (!gLawnApp) return;
    // Detach the offline Board before ordinary shutdown so it cannot autosave
    // or write user settings. All files in this instance are temporary anyway.
    delete gLawnApp->mBoard;gLawnApp->mBoard=nullptr;
    gLawnApp->Shutdown();delete gLawnApp;gLawnApp=nullptr;
}
void Require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
Json Extract(Board& board) {
    std::array<unsigned short,54> cells{};
    int rake=0,count=0,healthy=0,mowers=0;
    LawnMower* mower=nullptr;
    while(board.IterateLawnMowers(mower))if(!mower->mDead)++mowers;
    Plant* p=nullptr;
    while(board.IteratePlants(p)) {
        if(p->mDead || p->mSquished) continue;
        const int type=static_cast<int>(p->mSeedType),x=p->mPlantCol,y=p->mRow;
        Require(type>=0 && type<48 && x>=0 && x<9 && y>=0 && y<6,"Unsupported plant in DAT64");
        const bool im=p->mImitaterType==SeedType::SEED_IMITATER;
        auto& cell=cells[y*9+x];++count;if(p->mPlantHealth==p->mPlantMaxHealth)++healthy;
        if(type==16||type==33)cell|=((type==16?1:2)<<6)|(im<<5);
        else if(type==30)cell|=16|(im<<3);
        else if(type==35)cell|=4|(im<<1);
        else {Require(!(cell>>10),"More than one main plant in a tile");cell|=((type+1)<<10)|(im<<9)|(!p->mIsAsleep<<8);}
    }
    GridItem* item=nullptr;
    while(board.IterateGridItems(item)) {
        if(item->mDead)continue;
        const int x=item->mGridX,y=item->mGridY;
        if(x<0||x>=9||y<0||y>=6)continue;
        if(item->mGridItemType==GridItemType::GRIDITEM_GRAVESTONE)cells[y*9+x]=(cells[y*9+x]&~0xe0)|0xc0;
        if(item->mGridItemType==GridItemType::GRIDITEM_LADDER)cells[y*9+x]|=1;
        if(item->mGridItemType==GridItemType::GRIDITEM_RAKE)rake=y+1;
    }
    return Json{{"sun",board.mSunMoney},{"mowers",mowers},{"music",{{"tune",board.mApp->mMusic->mCurMusicTune},{"order",board.mApp->mMusic->mPauseOffset},{"drumsOrder",board.mApp->mMusic->mPauseOffsetDrums},{"tempo",board.mApp->mMusic->mBaseBPM},{"speed",board.mApp->mMusic->mBaseModSpeed}}},{"healthyPlants",healthy},{"cards",board.mSeedBank->mNumPackets},{"cells",cells},{"scene",static_cast<int>(board.mBackground)},{"rake",rake},{"plants",count},{"flags",board.GetSurvivalFlagsCompleted()},{"repickPending",board.mNextSurvivalStageCounter>0},{"seed",board.mBoardRandSeed}};
}
void Generate(LawnApp& app,Board& board,const Json& r) {
    const int flags=r.at("flags"),scene=r.at("scene"),seed=r.at("seed"),rake=r.at("rake");
    Require(!r.contains("sun")||r.at("sun").is_number_integer(),"Sun must be an integer");
    const int sun=r.value("sun",9990);
    Require(sun>=0&&sun<=9990,"Sun must be between 0 and 9990");
    const auto cells=r.at("cells").get<std::array<unsigned short,54>>();
    Require(flags>=0&&flags<=10000000&&flags%2==0&&scene>=0&&scene<6,"Invalid flags or scene");
    const int rows=scene==2||scene==3?6:5;
    Require(rake>=0&&rake<=rows&&!((scene==2||scene==3)&&(rake==3||rake==4)),"Invalid rake row");
    for(int at=0;at<54;++at)Require((cells[at]>>10)<=48&&(at<rows*9||!cells[at]),"Invalid plant or sixth row");
    board.InitLevel();
    board.mBackground=static_cast<BackgroundType>(scene);
    board.mBoardRandSeed=seed;
    board.mChallenge->mSurvivalStage=flags/2;
    board.mPoolIntroVisual=false;
    board.mSodPosition=0;
    board.mSunMoney=sun;
    board.mShowShovel=true;
    board.mFogOffset=0;board.mFogBlownCountDown=0;
    for(int y=0;y<6;++y){const bool water=(scene==2||scene==3)&&(y==2||y==3);
        board.mPlantRow[y]=y>=rows?PlantRowType::PLANTROW_DIRT:water?PlantRowType::PLANTROW_POOL:PlantRowType::PLANTROW_NORMAL;
        for(int x=0;x<9;++x)board.mGridSquareType[x][y]=y>=rows?GridSquareType::GRIDSQUARE_DIRT:water?GridSquareType::GRIDSQUARE_POOL:GridSquareType::GRIDSQUARE_GRASS;
    }
    // This is an isolated offline generator, never an active Board load hook.
    // Fresh constructors supply normal health; lineup codes carry geometry only.
    app.mGameScene=GameScenes::SCENE_PLAYING;
    const auto add=[&](int at,int type,bool im){return board.AddPlant(at%9,at/9,static_cast<SeedType>(type),im?SeedType::SEED_IMITATER:SeedType::SEED_NONE);};
    for(int at=0;at<rows*9;++at){const auto c=cells[at];const int base=(c>>6)&3;if(base==1||base==2)add(at,base==1?16:33,c&32);}
    for(int at=0;at<rows*9;++at){const auto c=cells[at];const int type=(c>>10)-1;if(type>=0){auto* p=add(at,type,c&512);p->SetSleeping(0,0,!(c&256));}}
    for(int at=0;at<rows*9;++at){const auto c=cells[at];if(c&16)add(at,30,c&8);if(c&4)add(at,35,c&2);if(((c>>6)&3)==3)board.AddAGraveStone(at%9,at/9);if(c&1)board.AddALadder(at%9,at/9);}
    if(rake){const int row=rake-1;auto* p=board.mGridItems.DataArrayAlloc();p->mGridItemType=GridItemType::GRIDITEM_RAKE;p->mGridX=7;p->mGridY=row;p->mPosX=board.GridToPixelX(7,row);p->mPosY=board.GridToPixelY(7,row);p->mRenderOrder=Board::MakeRenderOrder(RenderLayer::RENDER_LAYER_GRAVE_STONE,row,9);p->mGridItemReanimID=app.ReanimationGetID(board.CreateRakeReanim(p->mPosX,p->mPosY,0));p->mGridItemState=GridItemState::GRIDITEM_STATE_RAKE_ATTRACTING;}
    // Custom endless intentionally has no lawn mowers; do not run the intro initializer.
    board.InitZombieWaves();
    // Serialize the same intermission boundary used by normal endless saves.
    // Continue consumes the final end-of-round step; the existing game then
    // increments stage and opens its chooser. Completed flags stay unchanged.
    // For 0 flags, -1 is only this pre-first-round boundary; normal continuation
    // immediately advances it to stage 0. No negative stage runs a battle.
    board.mChallenge->mSurvivalStage=flags/2-1;
    board.mCurrentWave=board.mNumWaves;
    board.mTotalSpawnedWaves=board.mNumWaves;
    board.mNextSurvivalStageCounter=1;
    // There is no previous hand in a newly generated lineup. Repick imports
    // cooldowns by indexing previous packet types, so an empty hand must have
    // count zero, not ten SEED_NONE entries. Capacity comes from the profile.
    board.mSeedBank->mNumPackets=0;
    constexpr int chooserOrder=0x7A; // Music.cpp: MUSIC_TUNE_CHOOSE_YOUR_SEEDS.
    PFILE* file=p_fopen("sounds/mainmusic.mo3","rb");
    Require(file!=nullptr,"Cannot read the packaged MO3 music");
    p_fseek(file,0,SEEK_END);const auto size=p_ftell(file);p_fseek(file,0,SEEK_SET);
    std::vector<unsigned char> bytes(size);const auto read=p_fread(bytes.data(),1,bytes.size(),file);p_fclose(file);
    Require(read==bytes.size(),"Cannot read complete MO3 music");
    Require(ReadMusicStart(bytes.data(),static_cast<int>(bytes.size()),chooserOrder,&app.mMusic->mBaseBPM,&app.mMusic->mBaseModSpeed),"Cannot determine the selected song's natural tempo/speed");
    app.mMusic->mCurMusicTune=MUSIC_TUNE_CHOOSE_YOUR_SEEDS;
    app.mMusic->mCurMusicFileMain=MUSIC_FILE_MAIN_MUSIC;
    app.mMusic->mCurMusicFileDrums=MUSIC_FILE_NONE;
    app.mMusic->mCurMusicFileHihats=MUSIC_FILE_NONE;
    app.mMusic->mPaused=true;app.mMusic->mPauseOffset=chooserOrder;app.mMusic->mPauseOffsetDrums=0;
    board.mPaused=false;
    Require(LawnSaveGame(&board,"/tool/generated.dat64"),"Cannot serialize generated DAT64");
}
}
int RunWebSaveTool() {
    try {
        // A separate iframe owns this entire instance and virtual filesystem.
        // No gameplay update, rendering loop, IDBFS mount, or persistent write runs.
        setenv("PVZ_APPDATA_DIR","/tool/",1);
        gTodStringFormats=gLawnStringFormats;gTodStringFormatCount=gLawnStringFormatCount;gAppCloseRequest=LawnGetCloseRequest;
        gLawnApp=new LawnApp;
        auto& app=*gLawnApp;app.mChangeDirTo="/";app.mIsWindowed=true;app.mOnlyAllowOneCopyToRun=false;app.mCmdLineParsed=true;app.mNoSoundNeeded=true;
        app.Init();
        app.LoadingThreadProc();
        Require(!app.mLoadingFailed && app.mPlayerInfo,"Cannot initialize offline save resources/profile");
        app.mGameMode=GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_3;
        app.mGameScene=GameScenes::SCENE_PLAYING;
        app.mBoard=new Board(&app);
        Json request;std::ifstream("/tool/request.json")>>request;
        if(request.at("op")=="generate")Generate(app,*app.mBoard,request.at("recipe"));
        else Require(LawnLoadGame(app.mBoard,"/tool/input.dat64"),"Cannot load DAT64 in offline extractor");
        const auto result=Extract(*app.mBoard).dump();
        ReleaseToolApp();
        EM_ASM({Module.pvzToolComplete(UTF8ToString($0));},result.c_str());
        // The caller also destroys the iframe and remaining Wasm memory.
        return 0;
    } catch(const std::exception& error) {
        const std::string message=error.what();ReleaseToolApp();
        EM_ASM({Module.pvzToolFail(UTF8ToString($0));},message.c_str());return 1;
    }
}

