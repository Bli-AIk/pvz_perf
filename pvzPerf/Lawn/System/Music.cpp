#include "pvz/BuildConfig.h"
#include "Music.h"
#include "Sexy.TodLib/ReanimClock.h"
#include "MusicLoadingPlan.h"
#include "Lawn/Board.h"
#include "PlayerInfo.h"
#include "LawnApp.h"
#include "Sexy.TodLib/TodDebug.h"
#include "Sexy.TodLib/TodCommon.h"
#include "sound/BassLoader.h"
#include "sound/BassMusicInterface.h"

using namespace Sexy;

// The sound framework cannot include game headers, so its music slot
// capacity is a numeric constant; keep it in lockstep with the MusicFile
// key domain (slot 0 unused: MusicFile values start at 1).
static_assert(Sexy::BassMusicInterface::kMusicSlotCount == (int)MusicFile::NUM_MUSIC_FILES,
	"BassMusicInterface slot capacity must match MusicFile::NUM_MUSIC_FILES");

//0x45A260
Music::Music()
{
	mApp = (LawnApp*)gSexyAppBase;
	mMusicInterface = gSexyAppBase->mMusicInterface;
	mCurMusicTune = MusicTune::MUSIC_TUNE_NONE;
	mCurMusicFileMain = MusicFile::MUSIC_FILE_NONE;
	mCurMusicFileDrums = MusicFile::MUSIC_FILE_NONE;
	mCurMusicFileHihats = MusicFile::MUSIC_FILE_NONE;
	mBurstOverride = -1;
	mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_OFF;
	mDrumsEndTimeNs = 0;
	mQueuedDrumTrackPackedOrder = -1;
	mBaseBPM = 155.0f;
	mBaseModSpeed = 3.0f;
	mMusicBurstState = MusicBurstState::MUSIC_BURST_OFF;
	mBurstEndTimeNs = 0;
	mPauseOffset = 0;
	mPauseOffsetDrums = 0;
	mPaused = false;
    SetMusicDisabled(mApp->mNoSoundNeeded || !BuildConfig::SupportsModuleMusic);
	mFadeOutEndTimeNs = 0;
	mFadeOutDurationNs = 0;
	mLastUpdateTimeNs = mApp->mApplicationTimeNs;
}

//0x45A2C0
bool Music::TodLoadMusic(MusicFile theMusicFile, const std::string& theFileName)
{
	if (IsMusicDisabled() || mApp->mNoSoundNeeded)
		return false;
	return static_cast<BassMusicInterface*>(mApp->mMusicInterface)->LoadMusic(theMusicFile, theFileName);
}


//0x45A6C0
void Music::SetupMusicFileForTune(MusicFile theMusicFile, MusicTune theMusicTune)
{
	// Music::SetupMusicFileForTune 分发表：每槽内层 MusicFile switch 随体迁移
	//（生成器 + 逆变换 token 对拍 0 差异）；default 具名并被缺槽/越界复用。
	struct MusicSetupLocals
	{
	int mTrackCount;
	int mTrackStart1;
	int mTrackEnd1;
	int mTrackStart2;
	int mTrackEnd2;
	};
	using MusicSetupCaseFn = void (*)(Music&, MusicFile, MusicSetupLocals&);

	// 原 default（无 case 的 9 型与表外哨兵共用）
	static constexpr auto MusicSetupDefault = [](Music& m, MusicFile theMusicFile, MusicSetupLocals& lc)
	{
	if (theMusicFile == MusicFile::MUSIC_FILE_MAIN_MUSIC || theMusicFile == MusicFile::MUSIC_FILE_DRUMS)
	{
	lc.mTrackCount = 30;
	lc.mTrackStart1 = 0;
	lc.mTrackEnd1 = 29;
	}
	};

	// 匿名 lambda 函数指针表，行序 = MusicTune 枚举值序（1..13）。
	static constexpr MusicSetupCaseFn kMusicSetupCases[(int)MusicTune::NUM_MUSIC_TUNES - (int)MusicTune::MUSIC_TUNE_DAY_GRASSWALK] = {
	// 原 case MUSIC_TUNE_DAY_GRASSWALK
	[](Music& m, MusicFile theMusicFile, MusicSetupLocals& lc)
	{
	switch (theMusicFile) {
	case MusicFile::MUSIC_FILE_MAIN_MUSIC:		lc.mTrackCount = 30;	lc.mTrackStart1 = 0;	lc.mTrackEnd1 = 23;											break;
	case MusicFile::MUSIC_FILE_HIHATS:			lc.mTrackCount = 30;	lc.mTrackStart1 = 27;	lc.mTrackEnd1 = 27;											break;
	case MusicFile::MUSIC_FILE_DRUMS:			lc.mTrackCount = 30;	lc.mTrackStart1 = 24;	lc.mTrackEnd1 = 26;											break;
	default: break;
	}
	},  // 0 MUSIC_TUNE_DAY_GRASSWALK
	MusicSetupDefault,  // 1 MUSIC_TUNE_NIGHT_MOONGRAINS（原 default）
	// 原 case MUSIC_TUNE_POOL_WATERYGRAVES
	[](Music& m, MusicFile theMusicFile, MusicSetupLocals& lc)
	{
	switch (theMusicFile) {
	case MusicFile::MUSIC_FILE_MAIN_MUSIC:		lc.mTrackCount = 30;	lc.mTrackStart1 = 0;	lc.mTrackEnd1 = 17;											break;
	case MusicFile::MUSIC_FILE_HIHATS:			lc.mTrackCount = 30;	lc.mTrackStart1 = 18;	lc.mTrackEnd1 = 24;	lc.mTrackStart2 = 29;	lc.mTrackEnd2 = 29;	break;
	// 1073 0045E0DF: DRUMS starts at 18, overlapping the HIHATS range.
	case MusicFile::MUSIC_FILE_DRUMS:			lc.mTrackCount = 30;	lc.mTrackStart1 = 18;	lc.mTrackEnd1 = 28;											break;
	default: break;
	}
	},  // 2 MUSIC_TUNE_POOL_WATERYGRAVES
	// 原 case MUSIC_TUNE_FOG_RIGORMORMIST
	[](Music& m, MusicFile theMusicFile, MusicSetupLocals& lc)
	{
	switch (theMusicFile) {
	case MusicFile::MUSIC_FILE_MAIN_MUSIC:		lc.mTrackCount = 30;	lc.mTrackStart1 = 0;	lc.mTrackEnd1 = 15;											break;
	case MusicFile::MUSIC_FILE_HIHATS:			lc.mTrackCount = 30;	lc.mTrackStart1 = 23;	lc.mTrackEnd1 = 23;											break;
	case MusicFile::MUSIC_FILE_DRUMS:			lc.mTrackCount = 30;	lc.mTrackStart1 = 16;	lc.mTrackEnd1 = 22;											break;
	default: break;
	}
	},  // 3 MUSIC_TUNE_FOG_RIGORMORMIST
	// 原 case MUSIC_TUNE_ROOF_GRAZETHEROOF
	[](Music& m, MusicFile theMusicFile, MusicSetupLocals& lc)
	{
	switch (theMusicFile) {
	case MusicFile::MUSIC_FILE_MAIN_MUSIC:		lc.mTrackCount = 30;	lc.mTrackStart1 = 0;	lc.mTrackEnd1 = 17;											break;
	case MusicFile::MUSIC_FILE_HIHATS:			lc.mTrackCount = 30;	lc.mTrackStart1 = 21;	lc.mTrackEnd1 = 21;											break;
	case MusicFile::MUSIC_FILE_DRUMS:			lc.mTrackCount = 30;	lc.mTrackStart1 = 18;	lc.mTrackEnd1 = 20;											break;
	default: break;
	}
	},  // 4 MUSIC_TUNE_ROOF_GRAZETHEROOF
	MusicSetupDefault,  // 5 MUSIC_TUNE_CHOOSE_YOUR_SEEDS（原 default）
	MusicSetupDefault,  // 6 MUSIC_TUNE_TITLE_CRAZY_DAVE_MAIN_THEME（原 default）
	MusicSetupDefault,  // 7 MUSIC_TUNE_ZEN_GARDEN（原 default）
	MusicSetupDefault,  // 8 MUSIC_TUNE_PUZZLE_CEREBRAWL（原 default）
	MusicSetupDefault,  // 9 MUSIC_TUNE_MINIGAME_LOONBOON（原 default）
	MusicSetupDefault,  // 10 MUSIC_TUNE_CONVEYER（原 default）
	MusicSetupDefault,  // 11 MUSIC_TUNE_FINAL_BOSS_BRAINIAC_MANIAC（原 default）
	MusicSetupDefault,  // 12 MUSIC_TUNE_CREDITS_ZOMBIES_ON_YOUR_LAWN（原 default）
	};
	static_assert(sizeof(kMusicSetupCases) / sizeof(kMusicSetupCases[0]) ==
	(int)MusicTune::NUM_MUSIC_TUNES - (int)MusicTune::MUSIC_TUNE_DAY_GRASSWALK, "kMusicSetupCases 槽位数须与 MusicTune 枚举同步");

	int aTrackCount = 0;
	int aTrackStart1 = -1, aTrackEnd1 = -1, aTrackStart2 = -1, aTrackEnd2 = -1;

	// C 形态 -> 匿名 lambda 函数指针表：行序 = MusicTune 枚举值序（1..13，
	// 下标平移）。内层 MusicFile switch 随体迁移保持原样；五常量经
	// MusicSetupLocals 传递（dispatch 前载入、后写回）；default 非空
	//（MAIN/DRUMS 全曲段）具名并被缺槽与越界（NONE/0）分支复用。
	MusicSetupLocals aSetupLocals{aTrackCount, aTrackStart1, aTrackEnd1, aTrackStart2, aTrackEnd2};
	const int aTuneIndex = static_cast<int>(theMusicTune) - (int)MusicTune::MUSIC_TUNE_DAY_GRASSWALK;
	if (static_cast<unsigned>(aTuneIndex) < static_cast<unsigned>((int)MusicTune::NUM_MUSIC_TUNES - (int)MusicTune::MUSIC_TUNE_DAY_GRASSWALK))
	{
		kMusicSetupCases[aTuneIndex](*this, theMusicFile, aSetupLocals);
	}
	else
	{
		MusicSetupDefault(*this, theMusicFile, aSetupLocals);
	}
	aTrackCount = aSetupLocals.mTrackCount;
	aTrackStart1 = aSetupLocals.mTrackStart1;
	aTrackEnd1 = aSetupLocals.mTrackEnd1;
	aTrackStart2 = aSetupLocals.mTrackStart2;
	aTrackEnd2 = aSetupLocals.mTrackEnd2;


	HMUSIC aHMusic = GetBassMusicHandle(theMusicFile);
	for (int aTrack = 0; aTrack < aTrackCount; aTrack++)
	{
		float aVolume;
		if (aTrack >= aTrackStart1 && aTrack <= aTrackEnd1)
			aVolume = 1;
		else if (aTrack >= aTrackStart2 && aTrack <= aTrackEnd2)
			aVolume = 1;
		else
			aVolume = 0;

		gBass->BASS_ChannelSetAttribute(aHMusic, BASS_ATTRIB_MUSIC_VOL_CHAN + aTrack, aVolume);  // 设置音乐每条轨道的音量属性（静音与否）
	}
}

void Music::LoadSong(MusicFile theMusicFile, const std::string& theFileName)
{
	if (IsMusicDisabled())
		return;

	if (!TodLoadMusic(theMusicFile, theFileName))
	{
		TodTrace("music failed to load\n");
		SetMusicDisabled(true);
	}
	else
	{
		gBass->BASS_ChannelSetAttribute(GetBassMusicHandle(theMusicFile), BASS_ATTRIB_MUSIC_PSCALER, 4);  // 设置音乐定位精确度属性
	}
}

//0x45A8A0
void Music::MusicTitleScreenInit()
{
	if (IsMusicDisabled())
		return;

	LoadSong(MusicFile::MUSIC_FILE_MAIN_MUSIC, "sounds\\mainmusic.mo3");
	MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_TITLE_CRAZY_DAVE_MAIN_THEME);
}

//0x45A980
void Music::MusicInit()
{
	if (IsMusicDisabled())
		return;

#ifdef _DEBUG
	int aNumLoadingTasks = mApp->mCompletedLoadingThreadTasks + GetNumLoadingTasks();
#endif

	for (const auto& entry : MusicLoading::Files)
	{
		LoadSong(entry.file, entry.path);
		mApp->mCompletedLoadingThreadTasks += MusicLoading::TaskWeight;
	}

	// 装载线程是音乐表唯一写入者：全部曲目装载完成后发布，此后槽位不可变、
	// 主线程只读零锁（发布前的主线程调用在接口层安全 no-op）。
	static_cast<BassMusicInterface*>(mApp->mMusicInterface)->PublishSlots();

#ifdef _DEBUG
	if (mApp->mCompletedLoadingThreadTasks != aNumLoadingTasks)
		TodTrace("Didn't calculate loading task count correctly!!!!");
#endif
}

//0x45AAC0
void Music::MusicCreditScreenInit()
{
	if (IsMusicDisabled())
		return;

	BassMusicInterface* aBass = (BassMusicInterface*)mApp->mMusicInterface;
	// credits 曲目已在启动装载清单（MusicLoadingPlan）；主线程不再写音乐表
	//（懒加载分支已随"装载线程唯一写入者"改造删除）。未装载且音乐未禁用 =
	// 安装损坏/装载失败被吞，按硬错纪律弹错。
	if (!aBass->IsMusicLoaded((int)MusicFile::MUSIC_FILE_CREDITS_ZOMBIES_ON_YOUR_LAWN))
	{
		TodErrorMessageBox("Credits song missing after startup music load; the loading manifest should have shipped it.", "Error");
		exit(0);
	}
}

//0x45ABB0
void Music::StopAllMusic()
{
	if (mMusicInterface != nullptr)
	{
		if (mCurMusicFileMain != MusicFile::MUSIC_FILE_NONE)
			mMusicInterface->StopMusic(mCurMusicFileMain);
		if (mCurMusicFileDrums != MusicFile::MUSIC_FILE_NONE)
			mMusicInterface->StopMusic(mCurMusicFileDrums);
		if (mCurMusicFileHihats != MusicFile::MUSIC_FILE_NONE)
			mMusicInterface->StopMusic(mCurMusicFileHihats);
	}

	mCurMusicTune = MusicTune::MUSIC_TUNE_NONE;
	mCurMusicFileMain = MusicFile::MUSIC_FILE_NONE;
	mCurMusicFileDrums = MusicFile::MUSIC_FILE_NONE;
	mCurMusicFileHihats = MusicFile::MUSIC_FILE_NONE;
	mQueuedDrumTrackPackedOrder = -1;
	mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_OFF;
	mMusicBurstState = MusicBurstState::MUSIC_BURST_OFF;
	mPauseOffset = 0;
	mPauseOffsetDrums = 0;
	mPaused = false;
	mFadeOutEndTimeNs = 0;
}

//0x45AC20
HMUSIC Music::GetBassMusicHandle(MusicFile theMusicFile)
{
	BassMusicInterface* aBass = (BassMusicInterface*)mApp->mMusicInterface;
	const std::unique_ptr<BassMusicInfo>& aSlot = aBass->mMusicSlots.at((size_t)theMusicFile);
	TOD_ASSERT(aSlot != nullptr);
	return aSlot->mHMusic;
}

//0x45AC70
void Music::PlayFromOffset(MusicFile theMusicFile, int theOffset, float theVolume)
{
	BassMusicInterface* aBass = (BassMusicInterface*)mApp->mMusicInterface;
	const std::unique_ptr<BassMusicInfo>& aSlot = aBass->mMusicSlots.at((size_t)theMusicFile);
	TOD_ASSERT(aSlot != nullptr);
	BassMusicInfo* aMusicInfo = aSlot.get();

	if (aMusicInfo->mHStream)
	{
		bool aNoLoop = theMusicFile == MusicFile::MUSIC_FILE_CREDITS_ZOMBIES_ON_YOUR_LAWN;  // MV 音乐不循环
		mMusicInterface->PlayMusic(theMusicFile, theOffset, aNoLoop);
	}
	else
	{
		aMusicInfo->mPlayback.Stop();
		SetupMusicFileForTune(theMusicFile, mCurMusicTune);  // 调整每条轨道的静音与否
		aMusicInfo->mStopOnFade = false;
		aMusicInfo->mVolume = aMusicInfo->mVolumeCap * theVolume;
		aMusicInfo->mFadeDurationNs = 0;
		//gBass->BASS_ChannelSetAttribute(aMusicInfo->mHMusic, -1, aMusicInfo->mVolume * 100.0, -101);  // 调整音乐音量
		aMusicInfo->mPlayback.SetGain(aMusicInfo->mVolume);
		gBass->BASS_ChannelFlags(aMusicInfo->mHMusic,
			BASS_MUSIC_POSRESET | BASS_MUSIC_RAMP | BASS_MUSIC_LOOP,
			BASS_MUSIC_POSRESET | BASS_MUSIC_RAMP | BASS_MUSIC_LOOP);
		// BASS_POS_MUSIC_ORDER is a packed order/row DWORD.  theOffset already
		// contains both halves when it comes from GameMusicPause(); wrapping it
		// in MAKELONG(theOffset, 0) used to discard the row and made a pause jump
		// back to the beginning of the current order.
		aMusicInfo->mPlayback.Seek(
			static_cast<QWORD>(static_cast<DWORD>(theOffset)),
			BASS_POS_MUSIC_ORDER);  // 设置偏移位置
		aMusicInfo->mPlayback.Play(false);
	}
}

namespace
{
// Music::PlayMusic 分发表：表内匿名 lambda = 原 case 体逐句迁移（生成器
// + 逆变换 token 对拍 0 差异）；default 具名并被越界分支复用。
struct MusicPlayLocals
{
    int mOffset;
    int mDrumsOffset;
};
using MusicPlayCaseFn = void (*)(Music&, MusicPlayLocals&);

// 原 default（无 case 的 0 型与表外哨兵共用）
void MusicPlayDefault(Music& m, MusicPlayLocals& lc)
{
    		TOD_ASSERT(false);
}

// 匿名 lambda 函数指针表，行序 = MusicTune 枚举值序（1..13）。
constexpr MusicPlayCaseFn kMusicPlayCases[(int)MusicTune::NUM_MUSIC_TUNES - (int)MusicTune::MUSIC_TUNE_DAY_GRASSWALK] = {
    // 原 case MUSIC_TUNE_DAY_GRASSWALK
    [](Music& m, MusicPlayLocals& lc)
    {
    		m.mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
    		m.mCurMusicFileDrums = MusicFile::MUSIC_FILE_DRUMS;
    		m.mCurMusicFileHihats = MusicFile::MUSIC_FILE_HIHATS;
    		if (lc.mOffset == -1)
    			lc.mOffset = 0;
    		m.PlayFromOffset(m.mCurMusicFileMain, lc.mOffset, 1.0);
    		m.PlayFromOffset(m.mCurMusicFileDrums, lc.mOffset, 0.0);
    		m.PlayFromOffset(m.mCurMusicFileHihats, lc.mOffset, 0.0);
    },  // 0 MUSIC_TUNE_DAY_GRASSWALK
    // 原 case MUSIC_TUNE_NIGHT_MOONGRAINS
    [](Music& m, MusicPlayLocals& lc)
    {
    		m.mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
    		m.mCurMusicFileDrums = MusicFile::MUSIC_FILE_DRUMS;
    		if (lc.mOffset == -1)
    		{
    			lc.mOffset = 0x30;
    			lc.mDrumsOffset = 0x5C;
    		}
    		m.PlayFromOffset(m.mCurMusicFileMain, lc.mOffset, 1.0);
    		m.PlayFromOffset(m.mCurMusicFileDrums, lc.mDrumsOffset, 0.0);
    },  // 1 MUSIC_TUNE_NIGHT_MOONGRAINS
    // 原 case MUSIC_TUNE_POOL_WATERYGRAVES
    [](Music& m, MusicPlayLocals& lc)
    {
    		m.mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
    		m.mCurMusicFileDrums = MusicFile::MUSIC_FILE_DRUMS;
    		m.mCurMusicFileHihats = MusicFile::MUSIC_FILE_HIHATS;
    		if (lc.mOffset == -1)
    			lc.mOffset = 0x5E;
    		m.PlayFromOffset(m.mCurMusicFileMain, lc.mOffset, 1.0);
    		m.PlayFromOffset(m.mCurMusicFileDrums, lc.mOffset, 0.0);
    		m.PlayFromOffset(m.mCurMusicFileHihats, lc.mOffset, 0.0);
    },  // 2 MUSIC_TUNE_POOL_WATERYGRAVES
    // 原 case MUSIC_TUNE_FOG_RIGORMORMIST
    [](Music& m, MusicPlayLocals& lc)
    {
    		m.mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
    		m.mCurMusicFileDrums = MusicFile::MUSIC_FILE_DRUMS;
    		m.mCurMusicFileHihats = MusicFile::MUSIC_FILE_HIHATS;
    		if (lc.mOffset == -1)
    			lc.mOffset = 0x7D;
    		m.PlayFromOffset(m.mCurMusicFileMain, lc.mOffset, 1.0);
    		m.PlayFromOffset(m.mCurMusicFileDrums, lc.mOffset, 0.0);
    		m.PlayFromOffset(m.mCurMusicFileHihats, lc.mOffset, 0.0);
    },  // 3 MUSIC_TUNE_FOG_RIGORMORMIST
    // 原 case MUSIC_TUNE_ROOF_GRAZETHEROOF
    [](Music& m, MusicPlayLocals& lc)
    {
    		m.mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
    		m.mCurMusicFileDrums = MusicFile::MUSIC_FILE_DRUMS;
    		m.mCurMusicFileHihats = MusicFile::MUSIC_FILE_HIHATS;
    		if (lc.mOffset == -1)
    			lc.mOffset = 0xB8;
    		m.PlayFromOffset(m.mCurMusicFileMain, lc.mOffset, 1.0);
    		m.PlayFromOffset(m.mCurMusicFileDrums, lc.mOffset, 0.0);
    		m.PlayFromOffset(m.mCurMusicFileHihats, lc.mOffset, 0.0);
    },  // 4 MUSIC_TUNE_ROOF_GRAZETHEROOF
    // 原 case MUSIC_TUNE_CHOOSE_YOUR_SEEDS
    [](Music& m, MusicPlayLocals& lc)
    {
    		m.mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
    		if (lc.mOffset == -1)
    			lc.mOffset = 0x7A;
    		m.PlayFromOffset(m.mCurMusicFileMain, lc.mOffset, 1.0);
    },  // 5 MUSIC_TUNE_CHOOSE_YOUR_SEEDS
    // 原 case MUSIC_TUNE_TITLE_CRAZY_DAVE_MAIN_THEME
    [](Music& m, MusicPlayLocals& lc)
    {
    		m.mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
    		if (lc.mOffset == -1)
    			lc.mOffset = 0x98;
    		m.PlayFromOffset(m.mCurMusicFileMain, lc.mOffset, 1.0);
    },  // 6 MUSIC_TUNE_TITLE_CRAZY_DAVE_MAIN_THEME
    // 原 case MUSIC_TUNE_ZEN_GARDEN
    [](Music& m, MusicPlayLocals& lc)
    {
    		m.mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
    		if (lc.mOffset == -1)
    			lc.mOffset = 0xDD;
    		m.PlayFromOffset(m.mCurMusicFileMain, lc.mOffset, 1.0);
    },  // 7 MUSIC_TUNE_ZEN_GARDEN
    // 原 case MUSIC_TUNE_PUZZLE_CEREBRAWL
    [](Music& m, MusicPlayLocals& lc)
    {
    		m.mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
    		if (lc.mOffset == -1)
    			lc.mOffset = 0xB1;
    		m.PlayFromOffset(m.mCurMusicFileMain, lc.mOffset, 1.0);
    },  // 8 MUSIC_TUNE_PUZZLE_CEREBRAWL
    // 原 case MUSIC_TUNE_MINIGAME_LOONBOON
    [](Music& m, MusicPlayLocals& lc)
    {
    		m.mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
    		if (lc.mOffset == -1)
    			lc.mOffset = 0xA6;
    		m.PlayFromOffset(m.mCurMusicFileMain, lc.mOffset, 1.0);
    },  // 9 MUSIC_TUNE_MINIGAME_LOONBOON
    // 原 case MUSIC_TUNE_CONVEYER
    [](Music& m, MusicPlayLocals& lc)
    {
    		m.mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
    		if (lc.mOffset == -1)
    			lc.mOffset = 0xD4;
    		m.PlayFromOffset(m.mCurMusicFileMain, lc.mOffset, 1.0);
    },  // 10 MUSIC_TUNE_CONVEYER
    // 原 case MUSIC_TUNE_FINAL_BOSS_BRAINIAC_MANIAC
    [](Music& m, MusicPlayLocals& lc)
    {
    		m.mCurMusicFileMain = MusicFile::MUSIC_FILE_MAIN_MUSIC;
    		if (lc.mOffset == -1)
    			lc.mOffset = 0x9E;
    		m.PlayFromOffset(m.mCurMusicFileMain, lc.mOffset, 1.0);
    },  // 11 MUSIC_TUNE_FINAL_BOSS_BRAINIAC_MANIAC
    // 原 case MUSIC_TUNE_CREDITS_ZOMBIES_ON_YOUR_LAWN
    [](Music& m, MusicPlayLocals& lc)
    {
    		m.mCurMusicFileMain = MusicFile::MUSIC_FILE_CREDITS_ZOMBIES_ON_YOUR_LAWN;
    		if (lc.mOffset == -1)
    			lc.mOffset = 0;
    		m.PlayFromOffset(m.mCurMusicFileMain, lc.mOffset, 1.0);
    },  // 12 MUSIC_TUNE_CREDITS_ZOMBIES_ON_YOUR_LAWN
};
static_assert(sizeof(kMusicPlayCases) / sizeof(kMusicPlayCases[0]) ==
    (int)MusicTune::NUM_MUSIC_TUNES - (int)MusicTune::MUSIC_TUNE_DAY_GRASSWALK, "kMusicPlayCases 槽位数须与 MusicTune 枚举同步");
}  // namespace

//0x45ADB0
void Music::PlayMusic(MusicTune theMusicTune, int theOffset, int theDrumsOffset)
{
	if (IsMusicDisabled())
		return;

	mCurMusicTune = theMusicTune;
	mCurMusicFileMain = MusicFile::MUSIC_FILE_NONE;
	mCurMusicFileDrums = MusicFile::MUSIC_FILE_NONE;
	mCurMusicFileHihats = MusicFile::MUSIC_FILE_NONE;
	bool aRestartingSong = theOffset != -1;

	// C 形态 -> 匿名 lambda 函数指针表：行序 = MusicTune 枚举值序（1..13，
	// 下标平移）。theOffset/theDrumsOffset 仅被 case 读写，经 MusicPlayLocals
	// 传递；default（TOD_ASSERT）具名并被越界分支复用（1..13 全覆盖无缺槽）。
	MusicPlayLocals aPlayLocals{theOffset, theDrumsOffset};
	const int aTuneIndex = static_cast<int>(theMusicTune) - (int)MusicTune::MUSIC_TUNE_DAY_GRASSWALK;
	if (static_cast<unsigned>(aTuneIndex) < static_cast<unsigned>((int)MusicTune::NUM_MUSIC_TUNES - (int)MusicTune::MUSIC_TUNE_DAY_GRASSWALK))
	{
		kMusicPlayCases[aTuneIndex](*this, aPlayLocals);
	}
	else
	{
		MusicPlayDefault(*this, aPlayLocals);
	}


	if (aRestartingSong)
	{
		if (mCurMusicFileMain != MusicFile::MUSIC_FILE_NONE)
		{
			HMUSIC aHMusic = GetBassMusicHandle(mCurMusicFileMain);
			gBass->BASS_ChannelSetAttribute(aHMusic, BASS_ATTRIB_MUSIC_BPM, mBaseBPM);
			gBass->BASS_ChannelSetAttribute(aHMusic, BASS_ATTRIB_MUSIC_SPEED, mBaseModSpeed);
		}
		if (mCurMusicFileDrums != -1)
		{
			HMUSIC aHMusic = GetBassMusicHandle(mCurMusicFileDrums);
			gBass->BASS_ChannelSetAttribute(aHMusic, BASS_ATTRIB_MUSIC_BPM, mBaseBPM);
			gBass->BASS_ChannelSetAttribute(aHMusic, BASS_ATTRIB_MUSIC_SPEED, mBaseModSpeed);
		}
		if (mCurMusicFileHihats != -1)
		{
			HMUSIC aHMusic = GetBassMusicHandle(mCurMusicFileHihats);
			gBass->BASS_ChannelSetAttribute(aHMusic, BASS_ATTRIB_MUSIC_BPM, mBaseBPM);
			gBass->BASS_ChannelSetAttribute(aHMusic, BASS_ATTRIB_MUSIC_SPEED, mBaseModSpeed);
		}
	}
	else
	{
		HMUSIC aHMusic = GetBassMusicHandle(mCurMusicFileMain);
		gBass->BASS_ChannelGetAttribute(aHMusic, BASS_ATTRIB_MUSIC_BPM, &mBaseBPM);
		gBass->BASS_ChannelGetAttribute(aHMusic, BASS_ATTRIB_MUSIC_SPEED, &mBaseModSpeed);
	}
}

unsigned int Music::GetMusicOrder(MusicFile theMusicFile)
{
	TOD_ASSERT(theMusicFile != MusicFile::MUSIC_FILE_NONE);
	return ((BassMusicInterface*)mApp->mMusicInterface)->GetMusicOrder((int)theMusicFile);
}

//0x45B1B0
void Music::MusicResyncChannel(MusicFile theMusicFileToMatch, MusicFile theMusicFileToSync)
{
	unsigned int aPosToMatch = GetMusicOrder(theMusicFileToMatch);
	unsigned int aPosToSync = GetMusicOrder(theMusicFileToSync);
	int aDiff = (aPosToSync >> 16) - (aPosToMatch >> 16);  // 待同步的音乐与目标音乐的乐曲序号之差
	if (abs(aDiff) <= 128)  // 当前进行的乐曲序号之差超过 128 时，开摆（
	{
		HMUSIC aHMusic = GetBassMusicHandle(theMusicFileToSync);

		float aBPM = mBaseBPM;
		if (aDiff > 2)
			aBPM -= 2.0f;
		else if (aDiff > 0)
			aBPM -= 1.0f;
		else if (aDiff < -2)
			aBPM += 2.0f;
		else if (aDiff < 0)
			aBPM += 1.0f;

		gBass->BASS_ChannelSetAttribute(aHMusic, BASS_ATTRIB_MUSIC_BPM, aBPM);  // 适当调整待同步音乐的速率以缩小差距
	}
}

void Music::MusicResync()
{
	if (mCurMusicFileMain != MusicFile::MUSIC_FILE_NONE)
	{
		if (mCurMusicFileDrums != MusicFile::MUSIC_FILE_NONE)
			MusicResyncChannel(mCurMusicFileMain, mCurMusicFileDrums);
		if (mCurMusicFileHihats != MusicFile::MUSIC_FILE_NONE)
			MusicResyncChannel(mCurMusicFileMain, mCurMusicFileHihats);
	}
}

//0x45B240
void Music::StartBurst()
{ 
	if (mMusicBurstState == MusicBurstState::MUSIC_BURST_OFF)
	{ 
		mMusicBurstState = MusicBurstState::MUSIC_BURST_STARTING;
		mBurstEndTimeNs = ReanimClock::Add(mApp->mApplicationTimeNs, 4000000000LL);
	}
}

void Music::FadeOut(int theFadeOutDuration)
{ 
	if (mCurMusicTune != MusicTune::MUSIC_TUNE_NONE)
	{
		mFadeOutDurationNs = static_cast<std::int64_t>(theFadeOutDuration) * 10'000'000;
		mFadeOutEndTimeNs = mFadeOutDurationNs > 0
			? ReanimClock::Add(mApp->mApplicationTimeNs, mFadeOutDurationNs) : 0;
	}
}

//0x45B260
void Music::UpdateMusicBurst(std::int64_t nowNs)
{
	if (mApp->mBoard == nullptr)
		return;

	// 1073 keeps the burst controller inactive during the first-time PopCap
	// Presents intro. Its preview board has enough street zombies to trip the
	// normal >=10-zombie test, which would otherwise enable the drum channel.
	if (mApp->mGameMode == GameMode::GAMEMODE_INTRO)
		return;

	int aBurstScheme;
	if (mCurMusicTune == MusicTune::MUSIC_TUNE_DAY_GRASSWALK || mCurMusicTune == MusicTune::MUSIC_TUNE_POOL_WATERYGRAVES ||
		mCurMusicTune == MusicTune::MUSIC_TUNE_FOG_RIGORMORMIST || mCurMusicTune == MusicTune::MUSIC_TUNE_ROOF_GRAZETHEROOF)
		aBurstScheme = 1;
	else if (mCurMusicTune == MusicTune::MUSIC_TUNE_NIGHT_MOONGRAINS)
		aBurstScheme = 2;
	else
		return;

	int aPackedOrderMain = GetMusicOrder(mCurMusicFileMain);
	const auto burstRemainingNs = std::max<std::int64_t>(0, ReanimClock::Subtract(mBurstEndTimeNs, nowNs));
	const auto drumsRemainingNs = std::max<std::int64_t>(0, ReanimClock::Subtract(mDrumsEndTimeNs, nowNs));

	float aFadeTrackVolume = 0.0f;
	float aDrumsVolume = 0.0f;
	float aMainTrackVolume = 1.0f;
	switch (mMusicBurstState)
	{
		case MusicBurstState::MUSIC_BURST_OFF:
			if (mApp->mBoard->CountZombiesOnScreen() >= 10 || mBurstOverride == 1)
				StartBurst();
			break;
		case MusicBurstState::MUSIC_BURST_STARTING:
			if (aBurstScheme == 1)
			{
				aFadeTrackVolume = TodAnimateCurveNs(4'000'000'000LL, 0, burstRemainingNs, 0.0f, 1.0f, Curves::Linear);
				if (burstRemainingNs <= 1'000'000'000 && mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_OFF)
				{
					mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_ON_QUEUED;
					mQueuedDrumTrackPackedOrder = aPackedOrderMain;
				}
				else if (burstRemainingNs == 0)
				{
					mMusicBurstState = MusicBurstState::MUSIC_BURST_ON;
					mBurstEndTimeNs = ReanimClock::Add(nowNs, 8000000000LL);
				}
			}
			else if (aBurstScheme == 2)
			{
				if (mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_OFF)
				{
					mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_ON_QUEUED;
					mQueuedDrumTrackPackedOrder = aPackedOrderMain;
					mBurstEndTimeNs = ReanimClock::Add(nowNs, 4000000000LL);
				}
				else if (mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_ON_QUEUED)
					mBurstEndTimeNs = ReanimClock::Add(nowNs, 4000000000LL);
				else
				{
					aMainTrackVolume = TodAnimateCurveNs(4'000'000'000LL, 0, burstRemainingNs, 1.0f, 0.0f, Curves::Linear);
					if (burstRemainingNs == 0)
					{
						mMusicBurstState = MusicBurstState::MUSIC_BURST_ON;
						mBurstEndTimeNs = ReanimClock::Add(nowNs, 8000000000LL);
					}
				}
			}
			break;
		case MusicBurstState::MUSIC_BURST_ON:
			aFadeTrackVolume = 1.0f;
			if (aBurstScheme == 2)
				aMainTrackVolume = 0.0f;
			if (burstRemainingNs == 0 && ((mApp->mBoard->CountZombiesOnScreen() < 4 && mBurstOverride == -1) || mBurstOverride == 2))
			{
				if (aBurstScheme == 1)
				{
					mMusicBurstState = MusicBurstState::MUSIC_BURST_FINISHING;
					mBurstEndTimeNs = ReanimClock::Add(nowNs, 8000000000LL);
					mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_OFF_QUEUED;
					mQueuedDrumTrackPackedOrder = aPackedOrderMain;
				}
				else if (aBurstScheme == 2)
				{
					mMusicBurstState = MusicBurstState::MUSIC_BURST_FINISHING;
					mBurstEndTimeNs = ReanimClock::Add(nowNs, 11000000000LL);
					mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_FADING;
					mDrumsEndTimeNs = ReanimClock::Add(nowNs, 8000000000LL);
				}
			}
			break;
		case MusicBurstState::MUSIC_BURST_FINISHING:
			if (aBurstScheme == 1)
				aFadeTrackVolume = TodAnimateCurveNs(8'000'000'000LL, 0, burstRemainingNs, 1.0f, 0.0f, Curves::Linear);
			else
				aMainTrackVolume = TodAnimateCurveNs(4'000'000'000LL, 0, burstRemainingNs, 0.0f, 1.0f, Curves::Linear);
			if (burstRemainingNs == 0 && mMusicDrumsState == MusicDrumsState::MUSIC_DRUMS_OFF)
				mMusicBurstState = MusicBurstState::MUSIC_BURST_OFF;
			break;
	}

	int aDrumsJumpOrder = -1;
	int aOrderMain = 0, aOrderDrum = 0;
	if (aBurstScheme == 1)
	{
		aOrderMain = HIWORD(aPackedOrderMain) / 128;
		aOrderDrum = HIWORD(mQueuedDrumTrackPackedOrder) / 128;
	}
	else if (aBurstScheme == 2)
	{
		aOrderMain = LOWORD(aPackedOrderMain);
		aOrderDrum = LOWORD(mQueuedDrumTrackPackedOrder);
		if (HIWORD(aPackedOrderMain) > 252)
			aOrderMain++;
		if (HIWORD(mQueuedDrumTrackPackedOrder) > 252)
			aOrderDrum++;
	}

	switch (mMusicDrumsState)
	{
		case MusicDrumsState::MUSIC_DRUMS_ON_QUEUED:
			if (aOrderMain != aOrderDrum)
			{
				aDrumsVolume = 1.0f;
				mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_ON;
				if (aBurstScheme == 2)
					aDrumsJumpOrder = (aOrderMain % 2 == 0) ? 76 : 77;
			}
			break;
		case MusicDrumsState::MUSIC_DRUMS_ON:
			aDrumsVolume = 1.0f;
			break;
		case MusicDrumsState::MUSIC_DRUMS_OFF_QUEUED:
			aDrumsVolume = 1.0f;
			if (aOrderMain != aOrderDrum && aBurstScheme == 1)
			{
				mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_FADING;
				mDrumsEndTimeNs = ReanimClock::Add(nowNs, 500000000LL);
			}
			break;
		case MusicDrumsState::MUSIC_DRUMS_FADING:
			if (aBurstScheme == 2)
				aDrumsVolume = TodAnimateCurveNs(8'000'000'000LL, 0, drumsRemainingNs, 1.0f, 0.0f, Curves::Linear);
			else
				aDrumsVolume = TodAnimateCurveNs(500'000'000, 0, drumsRemainingNs, 1.0f, 0.0f, Curves::Linear);
			if (drumsRemainingNs == 0)
				mMusicDrumsState = MusicDrumsState::MUSIC_DRUMS_OFF;
			break;
		case MusicDrumsState::MUSIC_DRUMS_OFF:
			break;
	}

	if (aBurstScheme == 1)
	{
		mMusicInterface->SetSongVolume(mCurMusicFileHihats, aFadeTrackVolume);
		mMusicInterface->SetSongVolume(mCurMusicFileDrums, aDrumsVolume);
	}
	else if (aBurstScheme == 2)
	{
		mMusicInterface->SetSongVolume(mCurMusicFileMain, aMainTrackVolume);
		mMusicInterface->SetSongVolume(mCurMusicFileDrums, aDrumsVolume);
		if (aDrumsJumpOrder != -1)
			static_cast<BassMusicInterface*>(mMusicInterface)->mMusicSlots[
				static_cast<size_t>(mCurMusicFileDrums)]->mPlayback.Seek(
					MAKELONG(aDrumsJumpOrder, 0), BASS_POS_MUSIC_ORDER);
	}
}

//0x45B670
void Music::MusicUpdate(std::int64_t nowNs)
{
    const auto elapsedNs = ReanimClock::Subtract(nowNs, mLastUpdateTimeNs);
    mLastUpdateTimeNs = nowNs;
    // The application clock keeps running through a board pause. Burst/drum
    // controller deadlines are translated; the ordinary song fade continues.
    const bool disabled = IsMusicDisabled();
    if (disabled || (mApp->mBoard && mApp->mBoard->mPaused))
    {
        if (mBurstEndTimeNs != 0) mBurstEndTimeNs = ReanimClock::Add(mBurstEndTimeNs, elapsedNs);
        if (mDrumsEndTimeNs != 0) mDrumsEndTimeNs = ReanimClock::Add(mDrumsEndTimeNs, elapsedNs);
        if (disabled && mFadeOutEndTimeNs != 0)
            mFadeOutEndTimeNs = ReanimClock::Add(mFadeOutEndTimeNs, elapsedNs);
    }
    if (disabled) return;
    if (mFadeOutEndTimeNs != 0)
    {
        const auto remainingNs = std::max<std::int64_t>(0, ReanimClock::Subtract(mFadeOutEndTimeNs, nowNs));
        if (remainingNs == 0)
            StopAllMusic();
        else
            mMusicInterface->SetSongVolume(mCurMusicFileMain,
                TodAnimateCurveNs(mFadeOutDurationNs, 0, remainingNs, 1.0f, 0.0f, Curves::Linear));
    }
    if (mApp->mBoard == nullptr || !mApp->mBoard->mPaused)
    {
        UpdateMusicBurst(nowNs);
        MusicResync();
    }
}

//0x45B750
// GOTY @Patoke: 0x45EFA0
void Music::MakeSureMusicIsPlaying(MusicTune theMusicTune)
{
	if (mCurMusicTune != theMusicTune)
	{
		StopAllMusic();
		PlayMusic(theMusicTune, -1, -1);
	}
}

//0x45B770
void Music::StartGameMusic()
{
	TOD_ASSERT(mApp->mBoard);

	if (mApp->mGameMode == GameMode::GAMEMODE_LIMBO_ZEN_GARDEN || mApp->mGameMode == GameMode::GAMEMODE_TREE_OF_WISDOM)
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_ZEN_GARDEN);
	else if (mApp->IsFinalBossLevel())
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_FINAL_BOSS_BRAINIAC_MANIAC);
	else if (mApp->IsWallnutBowlingLevel() || mApp->IsWhackAZombieLevel() || mApp->IsLittleTroubleLevel() || mApp->IsBungeeBlitzLevel() ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_SPEED)
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_MINIGAME_LOONBOON);
	else if ((mApp->IsAdventureMode() && (mApp->mPlayerInfo->mLevel == 10 || mApp->mPlayerInfo->mLevel == 20 || mApp->mPlayerInfo->mLevel == 30)) ||
		mApp->mGameMode == GameMode::GAMEMODE_MINIGAME_COLUMN)
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_CONVEYER);
	else if (mApp->IsStormyNightLevel())
		StopAllMusic();
	else if (mApp->IsScaryPotterLevel() || mApp->IsIZombieLevel())
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_PUZZLE_CEREBRAWL);
	else if (mApp->mBoard->mBackground == BackgroundType::BACKGROUND_4_FOG)
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_FOG_RIGORMORMIST);
	else if (mApp->mBoard->StageIsNight())
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_NIGHT_MOONGRAINS);
	else if (mApp->mBoard->StageHasPool())
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_POOL_WATERYGRAVES);
	else if (mApp->mBoard->StageHasRoof())
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_ROOF_GRAZETHEROOF);
	else
		MakeSureMusicIsPlaying(MusicTune::MUSIC_TUNE_DAY_GRASSWALK);
}

//0x45B930
void Music::GameMusicPause(bool thePause)
{
	if (thePause)
	{
		if (!mPaused && mCurMusicTune != MusicTune::MUSIC_TUNE_NONE)
		{
			if (IsMusicDisabled())
			{
				// Preserve saved positions, but keep logical pause/resume usable.
				mPaused = true;
				return;
			}
			BassMusicInterface* aBass = (BassMusicInterface*)mMusicInterface;
			const std::unique_ptr<BassMusicInfo>& aSlot = aBass->mMusicSlots.at((size_t)mCurMusicFileMain);
			TOD_ASSERT(aSlot != nullptr);
			BassMusicInfo* aMusicInfo = aSlot.get();
			
			if (aMusicInfo->mHStream)
			{
				// BASS_POS_MUSIC_ORDER 只对 HMUSIC 有效；Credits 的 OGG 是
				// HSTREAM，暂停位置必须取字节位置，恢复时按 BASS_POS_BYTE 回设。
				mPauseOffset = static_cast<int>(aMusicInfo->mPlayback.Position(BASS_POS_BYTE));
				mMusicInterface->StopMusic(mCurMusicFileMain);
			}
			else
			{
				int aOrderMain = GetMusicOrder(mCurMusicFileMain);
				mPauseOffset = MAKELONG(LOWORD(aOrderMain), HIWORD(aOrderMain) / 4);
				mMusicInterface->StopMusic(mCurMusicFileMain);

				if (mCurMusicTune == MusicTune::MUSIC_TUNE_DAY_GRASSWALK || mCurMusicTune == MusicTune::MUSIC_TUNE_POOL_WATERYGRAVES ||
					mCurMusicTune == MusicTune::MUSIC_TUNE_FOG_RIGORMORMIST || mCurMusicTune == MusicTune::MUSIC_TUNE_ROOF_GRAZETHEROOF)
				{
					mMusicInterface->StopMusic(mCurMusicFileDrums);
					mMusicInterface->StopMusic(mCurMusicFileHihats);
				}
				else if (mCurMusicTune == MusicTune::MUSIC_TUNE_NIGHT_MOONGRAINS)
				{
					int aOrderDrum = GetMusicOrder(mCurMusicFileDrums);
					mPauseOffsetDrums = MAKELONG(LOWORD(aOrderDrum), HIWORD(aOrderDrum) / 4);
					mMusicInterface->StopMusic(mCurMusicFileDrums);
				}
			}
			mPaused = true;
		}
	}
	else if (mPaused)
	{
		if (mCurMusicTune != MusicTune::MUSIC_TUNE_NONE)
			PlayMusic(mCurMusicTune, mPauseOffset, mPauseOffsetDrums);
		mPaused = false;
	}
}

int Music::GetNumLoadingTasks()
{
	return MusicLoading::TaskCount(IsMusicDisabled());
}
