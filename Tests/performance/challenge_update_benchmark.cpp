// Actual Challenge::Update before/after dispatch, linked with Release/LTO.
// The frozen pre-change function below is a test oracle, never a game fallback.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "LawnApp.h"
#include "Lawn/Board.h"
#include "Lawn/Challenge.h"
#include "Lawn/SeedPacket.h"
#include "Lawn/System/PlayerInfo.h"
#include "Resources.h"
#include "Sexy.TodLib/Attachment.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/TodParticle.h"
#include "Sexy.TodLib/TodCommon.h"
#include "Sexy.TodLib/TodFoley.h"
#include "SexyAppFramework/graphics/DDInterface.h"
#include "SexyAppFramework/graphics/Font.h"
#include "SexyAppFramework/graphics/Image.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <stdexcept>

bool (*gAppCloseRequest)();
namespace {
class FixtureFont final : public Sexy::_Font {
public:
    _Font* Duplicate() override { return new FixtureFont(*this); }
};
class BeforeChallenge final : public Challenge {
public:
    void UpdateBefore(std::uint64_t deltaNs);
};

// Frozen source SHA256: a20f796fe509b8422f29432a7cd094017ea0145b0540e93e3af164a852ebd0e6
void BeforeChallenge::UpdateBefore(std::uint64_t theLevelDeltaNs)
{
	if (mApp->IsStormyNightLevel())
		UpdateStormyNight();

	if (mBoard->mPaused)
	{
		if (mApp->mGameMode == GAMEMODE_MINIGAME_BEGHOULED_TWIST)
		{
			mChallengeGridX = -1;
			mChallengeGridY = -1;
		}
		return;
	}

	if (mApp->mGameMode == GAMEMODE_MINIGAME_RAINING_SEEDS || mApp->IsStormyNightLevel())
	{
		UpdateRain();
	}

	if (mApp->mGameScene != SCENE_PLAYING && mApp->mGameMode != GAMEMODE_TREE_OF_WISDOM)
		return;

	if (mBoard->HasConveyorBeltSeedBank())
	{
		UpdateConveyorBelt();
	}
	if (mApp->mGameMode == GAMEMODE_MINIGAME_BEGHOULED || mApp->mGameMode == GAMEMODE_MINIGAME_BEGHOULED_TWIST)
	{
		UpdateBeghouled();
	}
	if (mApp->IsScaryPotterLevel())
	{
		ScaryPotterUpdate();
	}
	if ((mApp->IsScaryPotterLevel() || mApp->IsWhackAZombieLevel()) && mBoard->mSeedBank->mY < 0)
	{
		if (mBoard->mSunMoney + mBoard->CountSunBeingCollected() > 0 || mBoard->mSeedBank->mY > Sexy::IMAGE_SEEDBANK->mWidth)
		{
			mBoard->mSeedBank->mY += 2;
			if (mBoard->mSeedBank->mY > 0)
			{
				mBoard->mSeedBank->mY = 0;
			}
		}
	}

	if (mApp->IsWhackAZombieLevel())
	{
		WhackAZombieUpdate();
	}
	if (mApp->IsIZombieLevel())
	{
		IZombieUpdate();
	}
	if (mApp->IsSlotMachineLevel())
	{
		UpdateSlotMachine();
	}
	if (mApp->mGameMode == GAMEMODE_MINIGAME_SPEED)
	{
		// 1073 repeats UpdateGame, but not the surrounding water/effect update.
		// Timestamp-driven actors need a distinct time for this second pass too.
		if (mBoard->mLevelTimeNs > std::uint64_t(INT64_MAX) ||
			theLevelDeltaNs > std::uint64_t(INT64_MAX) - mBoard->mLevelTimeNs)
			throw std::overflow_error("speed-mode battle timestamp exceeds clock range");
		mBoard->mLevelTimeNs += theLevelDeltaNs;
		mApp->mEffectSystem->mReanimationHolder->mBattleTimeNs =
			static_cast<std::int64_t>(mBoard->mLevelTimeNs);
		mApp->mEffectSystem->RebaseIndependentBattleEffects(static_cast<std::int64_t>(mBoard->mLevelTimeNs));
		mBoard->UpdateGame(theLevelDeltaNs);
	}
	if (mApp->mGameMode == GAMEMODE_MINIGAME_RAINING_SEEDS)
	{
		UpdateRainingSeeds();
	}
	if (mApp->mGameMode == GAMEMODE_MINIGAME_PORTAL_COMBAT)
	{
		UpdatePortalCombat();
	}
	if (mApp->IsSquirrelLevel())
	{
		SquirrelUpdate();
	}
	if (mApp->mGameMode == GAMEMODE_MINIGAME_ZOMBIQUARIUM)
	{
		ZombiquariumUpdate();
	}
	if (mApp->mGameMode == GAMEMODE_TREE_OF_WISDOM)
	{
		TreeOfWisdomUpdate();
	}
	if (mApp->mGameMode == GAMEMODE_LIMBO_ICE &&
		mBoard->mLevelTimeNs - theLevelDeltaNs < 30'000'000'000 &&
		mBoard->mLevelTimeNs >= 30'000'000'000)
	{
		mApp->PlayFoley(FOLEY_FLOOP);
		mApp->PlaySample(Sexy::SOUND_LOSEMUSIC);
	}
	if (mApp->mGameMode == GAMEMODE_MINIGAME_LAST_STAND)
	{
		LastStandUpdate();
	}

	Reanimation* aReanim = mApp->ReanimationTryToGet(mReanimChallenge);
	if (aReanim && aReanim->mIsAttachment)
	{
		aReanim->UpdateAt((mBoard->AnimationTimeDomain() == ReanimTimeDomain::Battle ? static_cast<std::int64_t>(mBoard->mLevelTimeNs) : mApp->mEffectSystem->mReanimationHolder->mVisualTimeNs));
	}
}

void Require(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
constexpr int Iterations=2'000'000;
volatile unsigned long long sink;
using Update=void (*)(BeforeChallenge&,std::uint64_t);
__declspec(noinline) void Before(BeforeChallenge& challenge,std::uint64_t deltaNs) { challenge.UpdateBefore(deltaNs); }
__declspec(noinline) void After(BeforeChallenge& challenge,std::uint64_t deltaNs) { challenge.Challenge::Update(deltaNs); }
unsigned long long Hash(const BeforeChallenge& c) {
    return static_cast<unsigned int>(c.mChallengeState)*13ULL+c.mBoard->mSunMoney*7ULL+
        static_cast<unsigned int>(c.mBoard->mSeedBank->mY)*17ULL+static_cast<unsigned int>(c.mBoard->mTutorialState);
}
__declspec(noinline) unsigned long long Run(BeforeChallenge& challenge,Update update,int count,bool mixed) {
    Update volatile selected=update;
    static constexpr GameMode modes[]{GAMEMODE_SURVIVAL_ENDLESS_STAGE_3,GAMEMODE_MINIGAME_POGO_PARTY,
        GAMEMODE_SCARY_POTTER_1,GAMEMODE_MINIGAME_WHACK_A_ZOMBIE,GAMEMODE_PUZZLE_I_ZOMBIE_1};
    unsigned long long hash=0;
    for(int i=0;i<count;++i) {
        if(mixed) challenge.mApp->mGameMode=modes[static_cast<unsigned int>(i)%std::size(modes)];
        selected(challenge,16'666'667);
        hash+=Hash(challenge);
    }
    return hash;
}
}

int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    try {
        Require(SetProcessAffinityMask(GetCurrentProcess(),0x4000)!=0,"Cannot pin LP14");
        LawnApp app;gLawnApp=&app;
        PlayerInfo profile;profile.mLevel=11;app.mPlayerInfo=&profile;
        app.mGameMode=GAMEMODE_SURVIVAL_ENDLESS_STAGE_3;
        FixtureFont font;Sexy::FONT_TINYBOLD=&font;
        app.mDDInterface=new Sexy::DDInterface(&app);
        Sexy::Image bank(&app);bank.mWidth=446;bank.mHeight=87;Sexy::IMAGE_SEEDBANK=&bank;
        app.mEffectSystem=new EffectSystem;app.mEffectSystem->EffectSystemInitialize();
        auto* board=new Board(&app);
        auto* original=board->mChallenge;
        {
            BeforeChallenge challenge;board->mChallenge=&challenge;
            struct RestoreChallenge {Board& board;Challenge* original;~RestoreChallenge(){board.mChallenge=original;}} restore{*board,original};
            board->mSunMoney=9990;board->mSeedBank->mY=0;
            board->mTutorialState=TUTORIAL_WHACK_A_ZOMBIE_COMPLETED;
            challenge.mChallengeState=STATECHALLENGE_NORMAL;
            challenge.mReanimChallenge=REANIMATIONID_NULL;
            Sexy::ApplySimulationStep(0); // Stable downstream state, no elapsed timer events.
            struct Case {const char* name;GameMode mode;int level;bool paused;GameScenes scene;bool mixed;};
            static constexpr Case cases[]{
                {"endless",GAMEMODE_SURVIVAL_ENDLESS_STAGE_3,11,false,SCENE_PLAYING,false},
                {"adventure",GAMEMODE_ADVENTURE,1,false,SCENE_PLAYING,false},
                {"pogo",GAMEMODE_MINIGAME_POGO_PARTY,11,false,SCENE_PLAYING,false},
                {"paused",GAMEMODE_SURVIVAL_ENDLESS_STAGE_3,11,true,SCENE_PLAYING,false},
                {"intro",GAMEMODE_SURVIVAL_ENDLESS_STAGE_3,11,false,SCENE_LEVEL_INTRO,false},
                {"whack_idle",GAMEMODE_MINIGAME_WHACK_A_ZOMBIE,15,false,SCENE_PLAYING,false},
                {"adventure_whack",GAMEMODE_ADVENTURE,15,false,SCENE_PLAYING,false},
                {"vase_idle",GAMEMODE_SCARY_POTTER_1,35,false,SCENE_PLAYING,false},
                {"adventure_vase",GAMEMODE_ADVENTURE,35,false,SCENE_PLAYING,false},
                {"izombie_idle",GAMEMODE_PUZZLE_I_ZOMBIE_1,11,false,SCENE_PLAYING,false},
                {"mixed",GAMEMODE_SURVIVAL_ENDLESS_STAGE_3,11,false,SCENE_PLAYING,true},
            };
            LARGE_INTEGER frequency;QueryPerformanceFrequency(&frequency);
            std::puts("# Full real function and real downstream calls, stable non-event state; no game simulation/frame claim.");
            std::puts("# LP14, Release/LTO, equal indirect-call barrier and checksum, alternating before/after order.");
            std::puts("case,implementation,round,iterations,ns_per_update,checksum");
            for(const auto& entry:cases) {
                app.mGameMode=entry.mode;app.mGameScene=entry.scene;profile.mLevel=entry.level;board->mPaused=entry.paused;
                const auto expected=Hash(challenge)*Iterations;
                for(Update update:{Before,After}) sink=Run(challenge,update,100000,entry.mixed);
                for(int round=0;round<10;++round)
                for(int order=0;order<2;++order) {
                    const bool after=(round+order)%2!=0;
                    LARGE_INTEGER begin,end;QueryPerformanceCounter(&begin);
                    const auto result=Run(challenge,after?After:Before,Iterations,entry.mixed);
                    QueryPerformanceCounter(&end);sink=result;
                    Require(result==expected,"Dispatch changed the stable downstream state");
                    Require(GetCurrentProcessorNumber()==14,"Timed thread left LP14");
                    std::printf("%s,%s,%d,%d,%.6f,%llu\n",entry.name,after?"table":"before",round,Iterations,
                        (end.QuadPart-begin.QuadPart)*1e9/frequency.QuadPart/Iterations,result);
                }
                std::fflush(stdout);
            }
            board->mChallenge=original;
        }
        delete board;app.mBoard=nullptr;app.mPlayerInfo=nullptr;
        Sexy::IMAGE_SEEDBANK=nullptr;Sexy::FONT_TINYBOLD=nullptr;
        return 0;
    } catch(const std::exception& error) {std::fprintf(stderr,"FAIL: %s\n",error.what());return 1;}
}
