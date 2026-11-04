#include "graphics/GpuBackend.h"
#include <stdexcept>
#include "SexyAppBase.h"
#include "misc/FloatingPointMode.h"
#include <optional>
#include "misc/SEHCatcher.h"
#include "widget/WidgetManager.h"
#include "widget/Widget.h"
#include "misc/Debug.h"
#include "misc/KeyCodes.h"
#include "misc/FrameCounterMath.h"
#include "misc/DemoJumpTime.h"
#include "misc/DemoFileReader.h"
#include "misc/DemoReplayDispatch.h"
#include <array>
#include "effect/ScaleEffectSizing.h"
#include "misc/StableTime.h"
#include "JsonDoc.h"
#include "graphics/DDInterface.h"
#include "graphics/RenderInterface.h"
#include "graphics/Image.h"
#include "graphics/Font.h"
#include "graphics/Image.h"
#include "widget/Dialog.h"
#include "imagelib/ImageLib.h"
#include "graphics/ImageDiagnostics.h"
#include <filesystem>
#include "platform/Audio.h"
#if defined(__APPLE__)
#include "platform/macos/MacWindow.h"
#elif !defined(_WIN32)
#include "platform/NativeWindow.h"
#endif
#include "misc/Rect.h"
#include "misc/PerfTimer.h"
#include "Sexy.TodLib/TodDebug.h"
#include "misc/MTRand.h"
#include "misc/ModVal.h"
#include "Common.h"
#ifdef _WIN32
#include <process.h>
#include <direct.h>
#include <regstr.h>
#include <shlobj.h>
#else
#include <unistd.h>
#include "platform/ErrorDialog.h"
#ifdef __APPLE__
#include "platform/macos/MacSystem.h"
#endif
#include <fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <spawn.h>
#include <signal.h>
extern char** environ;
#endif
#include <cerrno>
#include <cctype>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <memory>
#include <filesystem>
#include <time.h>
#include <math.h>
#include <cstdlib>
#include "misc/ResourceManager.h"
#include "sound/BassMusicInterface.h"
#include "misc/AutoCrit.h"
#include <optional>
#include <future>
#include <chrono>
#include <memory>
#include <stdexcept>
#include "misc/Debug.h"
#include "paklib/PakInterface.h"
#include <string>

#include "sound/DummyMusicInterface.h"

#include "misc/memmgr.h"
#if defined(__ANDROID__)
#include <android/log.h>
#endif

using namespace Sexy;

namespace
{
#if defined(__linux__) && !defined(__ANDROID__)
std::string TransferClipboard(const std::string* input)
{
	// Native selection owners must outlive the caller; xclip/wl-copy provide
	// that lifetime. Arguments are fixed and user text only travels over stdin.
#if defined(SEXY_WINDOW_WAYLAND)
	static constexpr const char* copyArgs[] = {"wl-copy", "--type", "text/plain;charset=utf-8", nullptr};
	static constexpr const char* pasteArgs[] = {"wl-paste", "--no-newline", "--type", "text", nullptr};
#else
	static constexpr const char* copyArgs[] = {"xclip", "-selection", "clipboard", "-in", nullptr};
	static constexpr const char* pasteArgs[] = {"xclip", "-selection", "clipboard", "-out", nullptr};

	int endpoints[2];
	if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, endpoints) != 0)
		throw std::runtime_error("Cannot create native clipboard transport");
	struct Endpoints {
		int parent, child;
		~Endpoints() { if (parent >= 0) close(parent); if (child >= 0) close(child); }
	} sockets{endpoints[0], endpoints[1]};
	for (int* descriptor : {&sockets.parent, &sockets.child})
	{
		if (*descriptor >= 3) continue;
		const int duplicate = fcntl(*descriptor, F_DUPFD_CLOEXEC, 3);
		if (duplicate < 0) throw std::runtime_error("Cannot reserve native clipboard descriptor");
		close(*descriptor);
		*descriptor = duplicate;
	}
	posix_spawn_file_actions_t actions;
	int status = posix_spawn_file_actions_init(&actions);
	if (status) throw std::runtime_error("Cannot initialize native clipboard process actions");
	struct Actions {
		posix_spawn_file_actions_t& value;
		~Actions() { posix_spawn_file_actions_destroy(&value); }
	} actionLifetime{actions};
	status = posix_spawn_file_actions_adddup2(&actions, sockets.child,
		input ? STDIN_FILENO : STDOUT_FILENO);
	if (!status) status = posix_spawn_file_actions_addclose(&actions, sockets.parent);
	if (!status) status = posix_spawn_file_actions_addclose(&actions, sockets.child);
	if (status) throw std::runtime_error("Cannot configure native clipboard process");
	const auto args = input ? copyArgs : pasteArgs;
	pid_t process;
	status = posix_spawnp(&process, args[0], &actions, nullptr,
		const_cast<char* const*>(args), environ);
	if (status)
		throw std::runtime_error(std::string("Native clipboard requires ") + args[0] +
			": " + std::strerror(status));
	close(sockets.child);
	sockets.child = -1;
	struct Process {
		pid_t id;
		~Process() {
			if (id > 0) {
				kill(id, SIGTERM);
				while (waitpid(id, nullptr, 0) < 0 && errno == EINTR) {}
			}
		}
	} child{process};
	std::string output;
	if (input)
	{
		size_t written = 0;
		while (written < input->size())
		{
			const auto count = send(sockets.parent, input->data() + written,
				input->size() - written, MSG_NOSIGNAL);
			if (count < 0 && errno == EINTR) continue;
			if (count <= 0) throw std::runtime_error("Native clipboard copy failed");
			written += static_cast<size_t>(count);
		}
		shutdown(sockets.parent, SHUT_WR);
	}
	else
	{
		char buffer[4096];
		for (;;)
		{
			const auto count = read(sockets.parent, buffer, sizeof(buffer));
			if (count < 0 && errno == EINTR) continue;
			if (count < 0) throw std::runtime_error("Native clipboard paste failed");
			if (!count) break;
			output.append(buffer, static_cast<size_t>(count));
		}
	}
	int result = 0;
	while (waitpid(process, &result, 0) < 0)
		if (errno != EINTR) throw std::runtime_error("Cannot wait for native clipboard transfer");
	child.id = -1;
	if (!WIFEXITED(result) || WEXITSTATUS(result) != 0)
		throw std::runtime_error(std::string("Native clipboard transfer failed: ") + args[0]);
	return output;
}
#endif
#endif
unsigned int RuntimeTick()
{
#ifdef _WIN32
	return GetTickCount();
#else
	return SexyGetTickCount();
#endif
}
void RuntimeSleep(unsigned int milliseconds)
{
#ifdef _WIN32
	Sleep(milliseconds);
#else
	std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
#endif
}
std::string SettingsPath()
{
#ifdef _WIN32
	if (const char* path = std::getenv("PVZ_SETTINGS_FILE"); path && *path) return path;
	return gSexyAppBase && !gSexyAppBase->mChangeDirTo.empty()
		? AddTrailingSlash(gSexyAppBase->mChangeDirTo, true) + "settings.json"
		: "settings.json";
#else
	static const std::string path = [] {
		if (const char* overridePath = std::getenv("PVZ_SETTINGS_FILE"); overridePath && *overridePath)
			return std::filesystem::absolute(overridePath).lexically_normal().string();
		return GetAppDataFolder() + "settings.json";
	}();
	return path;
#endif
}

#ifdef _WIN32
bool GetFullscreenMonitorRect(HWND theWindow, RECT& outRect)
{
	HMONITOR aMonitor = MonitorFromWindow(
		theWindow,
		MONITOR_DEFAULTTONEAREST);
	MONITORINFO aInfo;
	ZeroMemory(&aInfo, sizeof(aInfo));
	aInfo.cbSize = sizeof(aInfo);
	if (aMonitor != NULL && GetMonitorInfo(aMonitor, &aInfo))
	{
		outRect = aInfo.rcMonitor;
		return outRect.right > outRect.left && outRect.bottom > outRect.top;
	}

	outRect.left = 0;
	outRect.top = 0;
	outRect.right = GetSystemMetrics(SM_CXSCREEN);
	outRect.bottom = GetSystemMetrics(SM_CYSCREEN);
	return outRect.right > outRect.left && outRect.bottom > outRect.top;
}
#endif
}

namespace
{
const int DEMO_FILE_ID = 0x42BEEF78;
const int DEMO_VERSION = 2;
}  // namespace

SexyAppBase* Sexy::gSexyAppBase = NULL;

namespace Sexy
{
// 命名污染审计：仅本 TU 使用，声明自对应头文件下沉（内部链接）。
static SEHCatcher gSEHCatcher;
}

HMODULE gDSoundDLL = NULL;
#ifdef _WIN32
static HMODULE gVersionDLL = NULL;

//typedef struct { UINT cbSize; DWORD dwTime; } LASTINPUTINFO;
namespace
{
typedef BOOL (WINAPI*GetLastInputInfoFunc)(LASTINPUTINFO *plii);
GetLastInputInfoFunc gGetLastInputInfoFunc = NULL;
}  // namespace
#endif
static bool gScreenSaverActive = false;

#ifndef SPI_GETSCREENSAVERRUNNING
#define SPI_GETSCREENSAVERRUNNING 114
#endif


static Image* gFPSImage = NULL;
static Image* gDemoTimeLeftImage = nullptr;

//////////////////////////////////////////////////////////////////////////

namespace
{
#ifdef _WIN32
typedef HRESULT (WINAPI *SHGetFolderPathFunc)(HWND, int, HANDLE, DWORD, LPTSTR);
void* GetSHGetFolderPath(const char* theDLL, HMODULE* theMod)
{
	HMODULE aMod = LoadLibrary(theDLL);
	SHGetFolderPathFunc aFunc = NULL;

	if (aMod != NULL)
	{
		*((void**)&aFunc) = (void*)GetProcAddress(aMod, "SHGetFolderPathA");
		if (aFunc == NULL)
		{
			FreeLibrary(aMod);
			aMod = NULL;
		}
	}	

	*theMod = aMod;
	return (void *)aFunc;
}
#endif
}  // namespace

//////////////////////////////////////////////////////////////////////////

SexyAppBase::SexyAppBase()
{
	gSexyAppBase = this;

#ifdef _WIN32
	gVersionDLL = LoadLibraryA("version.dll");
	gDSoundDLL = LoadLibraryA("dsound.dll");
	gGetLastInputInfoFunc = (GetLastInputInfoFunc) GetProcAddress(GetModuleHandleA("user32.dll"),"GetLastInputInfo");
#endif

	//ImageLib::InitJPEG2000();

	mMutex = NULL;
	mNotifyGameMessage = 0;

#ifdef _DEBUG
	mOnlyAllowOneCopyToRun = false;
#else
	mOnlyAllowOneCopyToRun = true;
#endif

	// Extract product version
#ifdef _WIN32
	char aPath[_MAX_PATH];
	GetModuleFileNameA(NULL, aPath, 256);
	mProductVersion = GetProductVersion(aPath);	
	mChangeDirTo = GetFileDir(aPath);
#else
	mProductVersion = "1.0.0";
	mChangeDirTo = GetExecutableDirectory();
#endif

	mNoDefer = false;	
	mFullScreenPageFlip = true; // should we page flip in fullscreen?
	mTimeLoaded = RuntimeTick();
	mSEHOccured = false;
	mProdName = "Product";
	mTitle = ("SexyApp");
	mShutdown = false;
	mExitToTop = false;
	mWidth = 640;
	mHeight = 480;
	mFullscreenBits = 16;
	mIsWindowed = true;
	mIsPhysWindowed = true;
	mPreferredX = -1;
	mPreferredY = -1;
	mIsScreenSaver = false;
	mAllowMonitorPowersave = true;
	mHWnd = NULL;
	mDDInterface = NULL;	
	mMusicInterface = NULL;
	mInvisHWnd = NULL;
	mUseExternalHWND = false;
	mExternalHWND = NULL;
	mFrameTime = 10;
	mNonDrawCount = 0;
	mDrawCount = 0;
	mUpdateCount = 0;	
	mUpdateAppState = 0;
	mUpdateAppDepth = 0;
	mPendingUpdatesAccNs = 0;
	mUpdateFTimeAccNs = 0;
	mHasPendingDraw = true;
	mIsDrawing = false;
	mRedrawInProgress = false;
	mLastDrawWasEmpty = false;	

	mLastTimeCheck = {};
	mUpdateMultiplier = 1;
	mPaused = false;
	mFastForwardToUpdateNum = 0;
	mFastForwardToMarker = false;
	mFastForwardStep = false;
	mSoundManager = NULL;
	mCursorNum = CURSOR_POINTER;		
	mMouseIn = false;
	mRunning = false;
	mActive = true;
	mProcessInTimer = false;
	mMinimized = false;	
	mPhysMinimized = false;
	mIsDisabled = false;
	mLoaded = false;	
	mYieldMainThread = false; 
	mLoadingFailed = false;
	mLoadingThreadStarted = false;
	mAutoStartLoadingThread = true;
	mLoadingThreadCompleted = false;
	mNumLoadingThreadTasks = 0;
	mCompletedLoadingThreadTasks = 0;	
	mLastDrawTick = SexyGetTickCount();
	// These clocks are read by the first pre-Start frame.  Leaving them
	// uninitialized lets the screen-saver probe suppress the title screen
	// before Start() has a chance to establish the real input epoch.
	mLastUserInputTick = mLastDrawTick;
	mLastTimerTime = mLastDrawTick;
	mNextDrawTick = SexyGetTickCount();
	mSysCursor = true;	
	mForceFullscreen = false;
	mForceWindowed = false;
	mHasFocus = true;			
	mCustomCursorsEnabled = false;	
	mCustomCursorDirty = false;
	mOverrideCursor = NULL;
	mInitialized = false;	
	mLastShutdownWasGraceful = true;	
	mReadFromRegistry = false;	
	mCmdLineParsed = false;
	mSkipSignatureChecks = false;	
	mCtrlDown = false;
	mAltDown = false;
	mAltEnterDown = false;
	mSwitchingScreenMode = false;
	mCursorClipActive = false;
	mCursorClipRect = {};
	mStepMode = 0;
	mCleanupSharedImages = false;
	mStandardWordWrap = true;
	mbAllowExtendedChars = true;
	mEnableMaximizeButton = false;
	mMusicVolume = 0.85f;
	mSfxVolume = 0.85;
	mDemoMusicVolume = mDemoSfxVolume = 0.0;
	mMuteCount = 0;	
	mAutoMuteCount = 0;
	mDemoMute = false;
	mMuteOnLostFocus = true;
	mCurHandleNum = 0;
	mShowFPS = false;
	mShowFPSMode = FPS_ShowFPS;
	mAlphaDisabled = false;	
	mDebugKeysEnabled = false;
	mOldWndProc = 0;
	mNoSoundNeeded = false;

	mSyncRefreshRate = 100;
	mVSyncUpdates = false;
	mVSyncBroken = false;
	mVSyncBrokenCount = 0;
	mVSyncBrokenTestStartTick = 0;
	mVSyncBrokenTestUpdates = 0;
	mWaitForVSync = false;
	mSoftVSyncWait = true;
	mTest3D = false;
	mMinVidMemory3D = 6;
	mRecommendedVidMemory3D = 14;
	mRelaxUpdateBacklogCount = 0;
	mWidescreenAware = false;
	mEnableWindowAspect = false;
	mWindowAspect.Set(4, 3);
	mWindowScale = 2;
	mUseCustomScale = true;
	mScaleConfigWindow = 2;
	mWindowScaleFromPipeline = 1;
	mScaleConfigFullscreenWidth = 0;
	mScaleConfigFullscreenHeight = 0;
	mScaleWindowConfig.mOutputWidth = 0;
	mScaleWindowConfig.mOutputHeight = 0;
	mScaleFullscreenConfig.mOutputWidth = 0;
	mScaleFullscreenConfig.mOutputHeight = 0;
	mIsWideWindow = false;

	int i;

	for (i = 0; i < NUM_CURSORS; i++)
		mCursorImages[i] = NULL;	

	for (i = 0; i < 256; i++)
		mAdd8BitMaxTable[i] = i;

	for (i = 256; i < 512; i++)
		mAdd8BitMaxTable[i] = 255;
	
	// Set default strings.  Init could read in overrides from partner.xml

	mDemoPrefix = "sexyapp";
	mDemoFileName = mDemoPrefix + ".dmo";
	mPlayingDemoBuffer = false;
	mManualShutdown = false;
	mRecordingDemoBuffer = false;
	mLastDemoMouseX = 0;
	mLastDemoMouseY = 0;
	mLastDemoUpdateCnt = 0;
	mDemoNeedsCommand = true;
	mDemoLoadingComplete = false;
	mDemoLength = 0;
	mDemoCmdNum = 0;
	mDemoCmdOrder = static_cast<std::uint32_t>(-1); // Means we haven't processed any demo commands yet
	mDemoCmdBitPos = 0;

	mWidgetManager = new WidgetManager(this);
	mResourceManager = new ResourceManager(this);

	mPrimaryThreadId = {};

#ifdef _WIN32
	if (GetSystemMetrics(86)) // check for tablet pc
	{
		mTabletPC = true;
		mFullScreenPageFlip = false; // so that tablet keyboard can show up
	}
	else
		mTabletPC = false;	
#else
	mTabletPC = false;
#endif

	gSEHCatcher.mApp = this;	
	
	//std::wifstream stringsFile(_wfopen(L".\\properties\\fstrings", L"rb"));
	//
	//if(!stringsFile)
	//{
	//	MessageBox(NULL, "file missing: 'install-folder\\properties\\fstrings' Please re-install", "FATAL ERROR", MB_OK);
	//	DoExit(1);
	//}
	//std::getline(stringsFile, mString_HardwareAccelSwitchedOn);
	//std::getline(stringsFile, mString_HardwareAccelConfirm);
	//std::getline(stringsFile, mString_HardwareAccelNotWorking);
	//std::getline(stringsFile, mString_SetColorDepth);
	//std::getline(stringsFile, mString_FailedInitDirectDrawColon);
	//std::getline(stringsFile, mString_UnableOpenProperties);
	//std::getline(stringsFile, mString_SigCheckFailed);
	//std::getline(stringsFile, mString_InvalidCommandLineParam);
	//std::getline(stringsFile, mString_RequiresDirectX);
	//std::getline(stringsFile, mString_YouNeedDirectX);
	//std::getline(stringsFile, mString_FailedInitDirectDraw);
	//std::getline(stringsFile, mString_FatalError);
	//std::getline(stringsFile, mString_UnexpectedErrorOccured);
	//std::getline(stringsFile, mString_PleaseHelpBy);
	//std::getline(stringsFile, mString_FailedConnectPopcap);
	//stringsFile.close();
}

SexyAppBase::~SexyAppBase()
{
	WriteDemoBuffer();
	ReleaseCursorClip();
	if (mLoadingThreadStarted && !mLoadingThreadCompleted)
		mLoadingFailed = true;
	WaitForLoadingThread();
	Shutdown();



	DialogMap::iterator aDialogItr = mDialogMap.begin();
	while (aDialogItr != mDialogMap.end())
	{
		mWidgetManager->RemoveWidget(aDialogItr->second);
		delete aDialogItr->second;
		++aDialogItr;
	}
	mDialogMap.clear();
	mDialogList.clear();
	
#ifdef _WIN32
	if (mInvisHWnd != NULL)
	{
		HWND aWindow = mInvisHWnd;
		mInvisHWnd = NULL;
		SetWindowLongPtr(aWindow, GWLP_USERDATA, 0);
		DestroyWindow(aWindow);
	}	
#endif
	
	delete mWidgetManager;	
	delete mResourceManager;
	delete gFPSImage;
	gFPSImage = NULL;
	delete gDemoTimeLeftImage;
	gDemoTimeLeftImage = nullptr;
	
	SharedImageMap::iterator aSharedImageItr = mSharedImageMap.begin();
	while (aSharedImageItr != mSharedImageMap.end())
	{
		SharedImage* aSharedImage = &aSharedImageItr->second;
		DBG_ASSERTE(aSharedImage->mRefCount == 0);		
		delete (aSharedImage->mImage);
		mSharedImageMap.erase(aSharedImageItr++);		
	}
	
	delete mDDInterface;
	delete mMusicInterface;
	delete mSoundManager;			

	DestroyGameWindow();

	gSexyAppBase = NULL;

	WriteDemoBuffer();

#ifdef _WIN32
	if (mMutex != NULL)
		::CloseHandle(mMutex);	

	FreeLibrary(gDSoundDLL);
	FreeLibrary(gVersionDLL);
#else
	if (mInstanceLockFd >= 0) close(mInstanceLockFd);
#endif
}

void SexyAppBase::ClearUpdateBacklog(bool relaxForASecond)
{
	mLastTimeCheck = std::chrono::time_point_cast<std::chrono::nanoseconds>(
		FrameDeadline::Clock::now());
	mUpdateFTimeAccNs = 0;
    mPendingUpdatesAccNs = 0;
    mFramePacingPhaseNs = mSimulationPhaseNs = 0;

	if (relaxForASecond)
		mRelaxUpdateBacklogCount = 1000;
}

bool SexyAppBase::AppCanRestore()
{
	return !mIsDisabled;
}

bool SexyAppBase::ReadDemoBuffer(std::string &theError)
{
    DemoTools::DemoContainer parsed;
    if (!DemoTools::ReadDemoContainer(mDemoFileName, mProductVersion, parsed, theError))
        return false;

    // Commit only a complete v1/v2 container. Buffer owns the allocation directly;
    // no intermediate marker/command buffer and no appending to a prior session.
    mDemoBuffer.mData.swap(parsed.commands);
    mDemoBuffer.mDataBitSize = static_cast<int>(mDemoBuffer.mData.size() * 8);
    mDemoBuffer.mReadBitPos = 0;
    mDemoBuffer.mWriteBitPos = mDemoBuffer.mDataBitSize;
    mDemoMarkerList.swap(parsed.markers);
    mDemoLength = parsed.duration;
    mRandSeed = parsed.seed;
    SRand(mRandSeed);
    return true;
}

void SexyAppBase::WriteDemoBuffer()
{
	if (mRecordingDemoBuffer)
	{
		FILE* aFP = fopen(mDemoFileName.c_str(), "w+b");

		if (aFP != NULL)
		{
			unsigned int aFileID = DEMO_FILE_ID;
			fwrite(&aFileID, 4, 1, aFP);		

			unsigned int aVersion = DEMO_VERSION;
			fwrite(&aVersion, 4, 1, aFP);
			
			fwrite(&mRandSeed, 4, 1, aFP);

			ushort aStrLen = mProductVersion.length();
			fwrite(&aStrLen, 2, 1, aFP);		
			fwrite(mProductVersion.c_str(), 1, mProductVersion.length(), aFP);

			Buffer aMarkerBuffer;
			aMarkerBuffer.WriteLong(mDemoMarkerList.size());
			for (DemoMarkerList::iterator aMarkerItr = mDemoMarkerList.begin(); aMarkerItr != mDemoMarkerList.end(); ++aMarkerItr)
			{
				aMarkerBuffer.WriteString(aMarkerItr->first);
				aMarkerBuffer.WriteLong(static_cast<std::int32_t>(aMarkerItr->second));
			}
			int aMarkerBufferSize = aMarkerBuffer.GetDataLen();
			fwrite(&aMarkerBufferSize, 4, 1, aFP);
			fwrite(aMarkerBuffer.GetDataPtr(), aMarkerBufferSize, 1, aFP);

			const std::uint32_t aDemoLength = mUpdateCount;
			fwrite(&aDemoLength, 4, 1, aFP);

			fwrite(mDemoBuffer.GetDataPtr(), 1, mDemoBuffer.GetDataLen(), aFP);
			fclose(aFP);
		}
		mRecordingDemoBuffer = false;  // 幂等：Win32 清理分支与析构路径各调一次也只写一盘
	}
}

// demo 落盘的 atexit 钩子：~SexyAppBase 不保证执行（app 对象可泄漏或走快速退出，
// 点窗口叉即如此），而 atexit 已被 perf 日志验证可靠。对象若已析构，其
// WriteDemoBuffer 因 mRecordingDemoBuffer=false 幂等空转，双保险无害。
namespace
{
	SexyAppBase* gDemoFlushTarget = nullptr;
	void DemoFlushAtExit()
	{
		if (gDemoFlushTarget != nullptr)
			gDemoFlushTarget->WriteDemoBuffer();
	}
}
void SexyAppBase::ArmDemoFlushAtExit()
{
	gDemoFlushTarget = this;
	std::atexit(DemoFlushAtExit);
}

void SexyAppBase::DemoSyncBuffer(Buffer* theBuffer)
{
	if (mPlayingDemoBuffer)
	{
		if (mManualShutdown)
			return;

		RequireDemoCommand(DEMO_SYNC);

		unsigned int aLen = mDemoBuffer.ReadLong();
		
		theBuffer->Clear();
		for (int i = 0; i < (int) aLen; i++)
			theBuffer->WriteByte(mDemoBuffer.ReadByte());		
	}
	else if (mRecordingDemoBuffer)
	{
		WriteDemoTimingBlock();
		mDemoBuffer.WriteNumBits(0, 1);
		mDemoBuffer.WriteNumBits(DEMO_SYNC, 5);		
		mDemoBuffer.WriteLong(theBuffer->GetDataLen());
		mDemoBuffer.WriteBytes((uchar*) theBuffer->GetDataPtr(), theBuffer->GetDataLen());
	}
}

void SexyAppBase::DemoSyncString(std::string* theString)
{
	Buffer aBuffer;
	aBuffer.WriteString(*theString);
	DemoSyncBuffer(&aBuffer);
	*theString = aBuffer.ReadString();
}

void SexyAppBase::DemoSyncInt(int* theInt)
{
	Buffer aBuffer;
	aBuffer.WriteLong(*theInt);
	DemoSyncBuffer(&aBuffer);
	*theInt = aBuffer.ReadLong();
}

void SexyAppBase::DemoSyncBool(bool* theBool)
{
	Buffer aBuffer;
	aBuffer.WriteBoolean(*theBool);
	DemoSyncBuffer(&aBuffer);
	*theBool = aBuffer.ReadBoolean();
}

void SexyAppBase::DemoAssertStringEqual(const std::string& theString)
{
	if (mPlayingDemoBuffer)
	{
		if (mManualShutdown)
			return;

		RequireDemoCommand(DEMO_ASSERT_STRING_EQUAL);

		std::string aString = mDemoBuffer.ReadString();
		DBG_ASSERTE(aString == theString);
	}
	else if (mRecordingDemoBuffer)
	{
		WriteDemoTimingBlock();
		mDemoBuffer.WriteNumBits(0, 1);
		mDemoBuffer.WriteNumBits(DEMO_ASSERT_STRING_EQUAL, 5);				
		mDemoBuffer.WriteString(theString);
	}
}

void SexyAppBase::DemoAddMarker(const std::string& theString)
{
	if (mPlayingDemoBuffer)
	{
		mFastForwardToMarker = false;
	}
	else if (mRecordingDemoBuffer)
	{
		mDemoMarkerList.push_back(DemoMarker(theString,mUpdateCount));
	}
}

void SexyAppBase::DemoRegisterHandle(HANDLE theHandle)
{
	if ((mRecordingDemoBuffer) || (mPlayingDemoBuffer))
	{
		// Insert the handle into a map with an auto-incrementing number so
		//  we can match up the auto-incrementing numbers with the handle
		//  later on, as handles may not be the same between executions
		std::pair<HandleToIntMap::iterator, bool> aPair = mHandleToIntMap.insert(HandleToIntMap::value_type(theHandle, mCurHandleNum));
		(void)aPair; // unused in Release mode
		DBG_ASSERT(aPair.second);
		mCurHandleNum++;
	}
}

#ifdef _WIN32
void SexyAppBase::DemoWaitForHandle(HANDLE theHandle)
{
	WaitForSingleObject(theHandle, INFINITE);
	
	if ((mRecordingDemoBuffer) || (mPlayingDemoBuffer))
	{
		// Remove the handle from our waiting map
		HandleToIntMap::iterator anItr = mHandleToIntMap.find(theHandle);					
		DBG_ASSERT(anItr != mHandleToIntMap.end());
		mHandleToIntMap.erase(anItr);
	}
}

bool SexyAppBase::DemoCheckHandle(HANDLE theHandle)
{
	if (mPlayingDemoBuffer)
	{	
		// We only need to try to get the result if we think we are waiting for one	
		if (gSexyAppBase->PrepareDemoCommand(false))
		{
			if ((!gSexyAppBase->mDemoIsShortCmd) && (gSexyAppBase->mDemoCmdNum == DEMO_HANDLE_COMPLETE))
			{
				// Find auto-incrementing handle num from handle
				HandleToIntMap::iterator anItr = mHandleToIntMap.find(theHandle);					
				DBG_ASSERT(anItr != mHandleToIntMap.end());

				int anOldBufferPos = gSexyAppBase->mDemoBuffer.mReadBitPos;

				// Since we don't require a demo result entry to be here, we must verify
				//  that this is referring to us
				int aDemoHandleNum = gSexyAppBase->mDemoBuffer.ReadLong();
				
				if (aDemoHandleNum == anItr->second)
				{
					// Alright, this was the handle we were waiting for!
					gSexyAppBase->mDemoNeedsCommand = true;

					// Actually wait for our local buddy to complete
					WaitForSingleObject(theHandle, INFINITE);
					mHandleToIntMap.erase(anItr);

					return true;
				}
				else
				{
					// Not us, go back
					gSexyAppBase->mDemoBuffer.mReadBitPos = anOldBufferPos;
				}
			}
		}

		return false;	
	}
	else 
	{
		if (WaitForSingleObject(theHandle, 0) == WAIT_OBJECT_0)
		{
			if (mRecordingDemoBuffer)
			{
				// Find auto-incrementing handle num from handle
				HandleToIntMap::iterator anItr = mHandleToIntMap.find(theHandle);					
				DBG_ASSERT(anItr != mHandleToIntMap.end());

				gSexyAppBase->WriteDemoTimingBlock();
				gSexyAppBase->mDemoBuffer.WriteNumBits(0, 1);
				gSexyAppBase->mDemoBuffer.WriteNumBits(DEMO_HANDLE_COMPLETE, 5);
				gSexyAppBase->mDemoBuffer.WriteLong(anItr->second);

				mHandleToIntMap.erase(anItr);
			}

			return true;
		}

		return false;
	}
}

#else
void SexyAppBase::DemoWaitForHandle(HANDLE)
{
	throw std::logic_error("Win32 demo wait handles are unavailable on Linux; loading uses std::thread");
}
bool SexyAppBase::DemoCheckHandle(HANDLE)
{
	throw std::logic_error("Win32 demo wait handles are unavailable on Linux; loading uses std::thread");
}
#endif

void SexyAppBase::DemoAssertIntEqual(int theInt)
{
	if (mPlayingDemoBuffer)
	{
		if (mManualShutdown)
			return;

		RequireDemoCommand(DEMO_ASSERT_INT_EQUAL);

		int anInt = mDemoBuffer.ReadLong();
		(void)anInt; // unused in Release mode
		DBG_ASSERTE(anInt == theInt);
	}
	else if (mRecordingDemoBuffer)
	{		
		WriteDemoTimingBlock();
		mDemoBuffer.WriteNumBits(0, 1);
		mDemoBuffer.WriteNumBits(DEMO_ASSERT_INT_EQUAL, 5);				
		mDemoBuffer.WriteLong(theInt);
	}
}

Dialog* SexyAppBase::NewDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode)
{	
	Dialog* aDialog = new Dialog(NULL, NULL, theDialogId, isModal, theDialogHeader,	theDialogLines, theDialogFooter, theButtonMode);		
	return aDialog;
}

Dialog* SexyAppBase::DoDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode)
{
	KillDialog(theDialogId);

	Dialog* aDialog = NewDialog(theDialogId, isModal, theDialogHeader, theDialogLines, theDialogFooter, theButtonMode);		

	AddDialog(theDialogId, aDialog);

	return aDialog;
}


Dialog*	SexyAppBase::GetDialog(int theDialogId)
{
	DialogMap::iterator anItr = mDialogMap.find(theDialogId);

	if (anItr != mDialogMap.end())	
		return anItr->second;

	return NULL;
}

bool SexyAppBase::KillDialog(int theDialogId, bool removeWidget, bool deleteWidget)
{
	DialogMap::iterator anItr = mDialogMap.find(theDialogId);

	if (anItr != mDialogMap.end())
	{
		Dialog* aDialog = anItr->second;

		// set the result to something else so DoMainLoop knows that the dialog is gone 
		// in case nobody else sets mResult		
		if (aDialog->mResult == 0x7FFFFFFF)
			aDialog->mResult = 0;
		
		DialogList::iterator aListItr = std::find(mDialogList.begin(),mDialogList.end(),aDialog);
		if (aListItr != mDialogList.end())
			mDialogList.erase(aListItr);
		
		mDialogMap.erase(anItr);

		if (removeWidget || deleteWidget)
		mWidgetManager->RemoveWidget(aDialog);

		if (aDialog->mIsModal)
		{			
			ModalClose();
			mWidgetManager->RemoveBaseModal(aDialog);
		}				

		if (deleteWidget)
		SafeDeleteWidget(aDialog);
		
		return true;
	}

	return false;
}

bool SexyAppBase::KillDialog(int theDialogId)
{
	return KillDialog(theDialogId,true,true);
}

bool SexyAppBase::KillDialog(Dialog* theDialog)
{
	// An earlier dialog can remain alive on its waiting caller's stack after
	// another instance has reused the same id. Close the requested object only.
	if (GetDialog(theDialog->mId) != theDialog)
		return false;
	return KillDialog(theDialog->mId);
}

int SexyAppBase::GetDialogCount()
{
	return mDialogMap.size();
}

void SexyAppBase::AddDialog(int theDialogId, Dialog* theDialog)
{
	KillDialog(theDialogId);

	if (theDialog->mWidth == 0)
	{
		// Set the dialog position ourselves
		int aWidth = mWidth/2;
		theDialog->Resize((mWidth - aWidth)/2, mHeight / 5, aWidth, theDialog->GetPreferredHeight(aWidth));
	}

	mDialogMap.insert(DialogMap::value_type(theDialogId, theDialog));
	mDialogList.push_back(theDialog);

	mWidgetManager->AddWidget(theDialog);
	if (theDialog->mIsModal)
	{
		mWidgetManager->AddBaseModal(theDialog);
		ModalOpen();
	}
}

void SexyAppBase::AddDialog(Dialog* theDialog)
{
	AddDialog(theDialog->mId, theDialog);
}

void SexyAppBase::ModalOpen()
{
}

void SexyAppBase::ModalClose()
{
}

void SexyAppBase::DialogButtonPress(int theDialogId, int theButtonId)
{	
	if (theButtonId == Dialog::ID_YES)
		ButtonPress(2000 + theDialogId);
	else if (theButtonId == Dialog::ID_NO)
		ButtonPress(3000 + theDialogId);	
}

void SexyAppBase::DialogButtonDepress(int theDialogId, int theButtonId)
{
	if (theButtonId == Dialog::ID_YES)
		ButtonDepress(2000 + theDialogId);
	else if (theButtonId == Dialog::ID_NO)
		ButtonDepress(3000 + theDialogId);
}

void SexyAppBase::GotFocus()
{	
}

void SexyAppBase::LostFocus()
{	
}

std::string SexyAppBase::GetProductVersion(const std::string& thePath)
{	
#ifdef _WIN32
	// Dynamically Load Version.dll
	typedef DWORD (APIENTRY *GetFileVersionInfoSizeFunc)(LPSTR lptstrFilename, LPDWORD lpdwHandle);
	typedef BOOL (APIENTRY *GetFileVersionInfoFunc)(LPSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData);
	typedef BOOL (APIENTRY *VerQueryValueFunc)(const LPVOID pBlock, LPSTR lpSubBlock, LPVOID * lplpBuffer, PUINT puLen);

	static GetFileVersionInfoSizeFunc aGetFileVersionInfoSizeFunc = NULL;
	static GetFileVersionInfoFunc aGetFileVersionInfoFunc = NULL;
	static VerQueryValueFunc aVerQueryValueFunc = NULL;

	if (aGetFileVersionInfoSizeFunc==NULL)
	{
		aGetFileVersionInfoSizeFunc = (GetFileVersionInfoSizeFunc)GetProcAddress(gVersionDLL,"GetFileVersionInfoSizeA");
		aGetFileVersionInfoFunc = (GetFileVersionInfoFunc)GetProcAddress(gVersionDLL,"GetFileVersionInfoA");
		aVerQueryValueFunc = (VerQueryValueFunc)GetProcAddress(gVersionDLL,"VerQueryValueA");
	}

	// Get Product Version
	std::string aProductVersion;
	
	uint aSize = aGetFileVersionInfoSizeFunc((char*) thePath.c_str(), 0);
	if (aSize > 0)		
	{
		uchar* aVersionBuffer = new uchar[aSize];
		aGetFileVersionInfoFunc((char*) thePath.c_str(), 0, aSize, aVersionBuffer);	
		char* aBuffer;
		if (aVerQueryValueFunc(aVersionBuffer, 
				  (char*)"\\StringFileInfo\\040904B0\\ProductVersion",
				  (void**) &aBuffer, 
				  &aSize))
		{
			aProductVersion = aBuffer;
		}
		else if (aVerQueryValueFunc(aVersionBuffer, 
				  (char*)"\\StringFileInfo\\040904E4\\ProductVersion", 
				  (void**) &aBuffer, 
				  &aSize))
		{
			aProductVersion = aBuffer;
		}

		delete[] aVersionBuffer;	
	}

	return aProductVersion;
#else
	(void)thePath;
	return "1.0.0";
#endif
}

void SexyAppBase::WaitForLoadingThread()
{
	// Destructors also join here; rethrow only at explicit main-thread barriers.
	if (mLoadingThread.joinable())
		mLoadingThread.join();
}

void SexyAppBase::SetCursorImage(int theCursorNum, Image* theImage)
{
	if ((theCursorNum >= 0) && (theCursorNum < NUM_CURSORS))
	{
		mCursorImages[theCursorNum] = theImage;
		EnforceCursor();
	}
}

void SexyAppBase::TakeScreenshot()
{
	if (!mDDInterface || !mDDInterface->mRenderInterface || !mDDInterface->mRenderInterface->mRenderer)
		throw std::logic_error("Screenshot requires an initialized Vulkan scene");
	auto& render = *mDDInterface->mRenderInterface;
	render.Flush();
	const auto pixels = render.mRenderer->ReadScenePixels();
	ImageLib::Image image;
	image.mWidth = render.mRenderer->sceneWidth;
	image.mHeight = render.mRenderer->sceneHeight;
	image.mBits = new unsigned int[pixels.size()];
	std::copy(pixels.begin(), pixels.end(), image.mBits);

	// Capture the logical canvas, not the desktop or a scaled/HDR swapchain.
	// Portable data root also keeps automated probes away from live user files.
	const auto directory = std::filesystem::path(GetAppDataFolder()) / "screenshots";
	std::filesystem::create_directories(directory);
	std::filesystem::path path;
	for (std::uint64_t number = 1;; ++number)
	{
		path = directory / ("screenshot-" + std::to_string(number) + ".png");
		if (!std::filesystem::exists(path)) break;
	}
	if (!ImageLib::WritePNGImage(path.string(), &image))
		throw std::runtime_error("Cannot write Vulkan screenshot: " + path.string());
	ClearUpdateBacklog(false);
}

bool SexyAppBase::DeferImageDiagnostics(unsigned request)
{
    if (CurrentNativeThreadIdentity() != mPrimaryThreadId)
        throw std::logic_error("Image diagnostics must run on the application thread");
    if (!mLoaded || (mLoadingThreadStarted && !mLoadingThreadCompleted.load(std::memory_order_acquire)) ||
        mLoadingFailed || mShutdown || mIsDrawing || mSwitchingScreenMode || mImageDiagnosticsActive)
    {
        mPendingImageDiagnostics |= request;
        return true;
    }
    return false;
}

void SexyAppBase::ProcessPendingImageDiagnostics()
{
    if (!mPendingImageDiagnostics || DeferImageDiagnostics(0))
        return;
    const auto requests = std::exchange(mPendingImageDiagnostics, 0u);
    if (requests & 1) DumpProgramInfo();
    if (requests & 2) ShowMemoryUsage();
}

std::optional<std::vector<ImageDiagnosticRecord>> SexyAppBase::CaptureImageDiagnostics(bool includePixels)
{
    if (CurrentNativeThreadIdentity() != mPrimaryThreadId || !mLoaded ||
        (mLoadingThreadStarted && !mLoadingThreadCompleted.load(std::memory_order_acquire)) ||
        mLoadingFailed || mShutdown || mIsDrawing || mSwitchingScreenMode)
        throw std::logic_error("Image diagnostic snapshot requires a loaded, idle application thread");
    if (!mDDInterface || !mDDInterface->mRenderInterface)
        throw std::logic_error("Image diagnostic snapshot requires the native renderer");

    // Production pixel construction/mutation after loading belongs to the main
    // thread. This lock excludes registry cleanup/removal while values are copied.
    // It is not permission for arbitrary worker threads to mutate live images.
    AutoCrit lock(mDDInterface->mCritSect);
    // Initializers register images before all pixel fields are published.
    // Check readiness while holding the same lock that starts/publishes a cache
    // load; no new initializer can begin between this check and the snapshot.
    // Never wait for a loader while holding its publication/registry lock.
    for (const auto& entry : mSharedImageMap)
        if (entry.second.mLoadReady.valid() &&
            entry.second.mLoadReady.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return std::nullopt;
    auto& render = *mDDInterface->mRenderInterface;
    if (includePixels) render.Flush();
    std::vector<ImageDiagnosticRecord> records;
    records.reserve(mGpuImageSet.size());
    for (const auto* image : mGpuImageSet)
    {
        ImageDiagnosticRecord record;
        record.path = "<generated GPU image>";
        for (const auto& entry : mSharedImageMap)
            if (entry.second.mImage == image) {
                record.path = entry.first.first;
                break;
            }
        record.width = image->mWidth; record.height = image->mHeight;
        record.cpu = MeasureImageCpuUsage(*image);
        record.traits = std::string(image->mForcedMode ? "forced " : "") + (image->mHasAlpha ? "alpha " : "") +
            (image->mHasTrans ? "trans " : "");
        if (image->mTexture)
            record.gpuTexels = ImageTexelBytes(image->mWidth, image->mHeight);
        const bool screen = image == mDDInterface->mScreenImage;
        if (screen) record.gpuTexels = AddImageBytes(record.gpuTexels, ImageTexelBytes(record.width, record.height));
        if (!includePixels) record.pixelStatus = "metadata only";
        else if (record.width == 0 || record.height == 0) record.pixelStatus = "empty image";
        else if (!image->mTexture && !screen)
            record.pixelStatus = "no GPU pixels available; no pixels synthesized";
        else
        {
            record.pixels = render.ReadImagePixels(*image);
            record.pixelStatus = screen ? "native screen" : "native GPU";
        }
        records.push_back(std::move(record));
    }
    AppendAtlasTextureDiagnostics(render, records, includePixels);
    std::stable_sort(records.begin(), records.end(), [](const auto& a, const auto& b) {
        return a.Total() > b.Total();
    });
    return records; // No image references survive beyond the lock.
}

void SexyAppBase::DumpProgramInfo()
{
    if (DeferImageDiagnostics(1)) return;
    ImageDiagnosticActivity active(mImageDiagnosticsActive);
    const auto records = CaptureImageDiagnostics(true);
    if (!records) { mPendingImageDiagnostics |= 1; return; }
    WriteImageDiagnosticDump(std::filesystem::path(GetLogFilePath("image-dumps")), *records);
    ClearUpdateBacklog(false);
}

double SexyAppBase::GetLoadingThreadProgress()
{
	if (mLoaded)
		return 1.0;
	if (!mLoadingThreadStarted)
		return 0.0;
	if (mNumLoadingThreadTasks == 0)
		return 0.0;
	return std::min(mCompletedLoadingThreadTasks / (double) mNumLoadingThreadTasks, 1.0);
}

#ifdef _WIN32
bool SexyAppBase::RegistryWrite(const std::string& theValueName, unsigned int theType, const uchar* theValue, unsigned int theLength)
{
	if (mRegKey.length() == 0)
		return false;

	if (mPlayingDemoBuffer)
	{
		if (mManualShutdown)
			return true;

		RequireDemoCommand(DEMO_REGISTRY_WRITE);

		return mDemoBuffer.ReadNumBits(1, false) != 0;		
	}

	HKEY aGameKey;
	
	std::string aKeyName = RemoveTrailingSlash("SOFTWARE\\" + mRegKey);
	std::string aValueName;

	int aSlashPos = (int) theValueName.rfind('\\');
	if (aSlashPos != -1)
	{
		aKeyName += "\\" + theValueName.substr(0, aSlashPos);
		aValueName = theValueName.substr(aSlashPos + 1);
	}
	else
	{
		aValueName = theValueName;
	}

	int aResult = RegOpenKeyExA(HKEY_CURRENT_USER, aKeyName.c_str(), 0, KEY_WRITE, &aGameKey);
	if (aResult != ERROR_SUCCESS)
	{
		DWORD aDisp;
		aResult = RegCreateKeyExA(HKEY_CURRENT_USER, aKeyName.c_str(), 0, (char *)"Key", REG_OPTION_NON_VOLATILE,
			KEY_ALL_ACCESS, NULL, &aGameKey, &aDisp);
	}

	if (aResult != ERROR_SUCCESS)
	{
		if (mRecordingDemoBuffer)
		{
			WriteDemoTimingBlock();
			mDemoBuffer.WriteNumBits(0, 1);
			mDemoBuffer.WriteNumBits(DEMO_REGISTRY_WRITE, 5);
			mDemoBuffer.WriteNumBits(0, 1); // failure
		}

		return false;
	}
		
	RegSetValueExA(aGameKey, aValueName.c_str(), 0, theType, theValue, theLength);
	RegCloseKey(aGameKey);

	if (mRecordingDemoBuffer)
	{
		WriteDemoTimingBlock();
		mDemoBuffer.WriteNumBits(0, 1);
		mDemoBuffer.WriteNumBits(DEMO_REGISTRY_WRITE, 5);
		mDemoBuffer.WriteNumBits(1, 1); // success
	}

	return true;
}

bool SexyAppBase::RegistryWriteString(const std::string& theValueName, const std::string& theString)
{
	return RegistryWrite(theValueName, REG_SZ, (uchar*) theString.c_str(), theString.length());
}

bool SexyAppBase::RegistryWriteInteger(const std::string& theValueName, int theValue)
{
	return RegistryWrite(theValueName, REG_DWORD, (uchar*) &theValue, sizeof(int));
}

bool SexyAppBase::RegistryWriteBoolean(const std::string& theValueName, bool theValue)
{
	int aValue = theValue ? 1 : 0;
	return RegistryWrite(theValueName, REG_DWORD, (uchar*) &aValue, sizeof(int));
}

bool SexyAppBase::RegistryWriteData(const std::string& theValueName, const uchar* theValue, unsigned int theLength)
{
	return RegistryWrite(theValueName, REG_BINARY, (uchar*) theValue, theLength);
}

#else
bool SexyAppBase::RegistryWrite(const std::string&, unsigned int, const uchar*, unsigned int) { return false; }
bool SexyAppBase::RegistryWriteString(const std::string&, const std::string&) { return false; }
bool SexyAppBase::RegistryWriteInteger(const std::string&, int) { return false; }
bool SexyAppBase::RegistryWriteBoolean(const std::string&, bool) { return false; }
bool SexyAppBase::RegistryWriteData(const std::string&, const uchar*, unsigned int) { return false; }
#endif

void SexyAppBase::WriteToRegistry()
{
	JsonDoc aDoc;
	aDoc.SetInt("targetFps", mFrameTiming.mTargetFps);
	aDoc.SetBool("autoDetectFrameRate",mAutoDetectFrameRate);
    aDoc.SetBool("landscapeUI",mLandscapeUI);
#if defined(__ANDROID__)
    aDoc.SetBool("hdr", false);
#endif
    // Persist slider percentages, not mixer gain. The original SFX slider's
    // full-scale gain is 0.65; both JSON fields still range from 0 to 100.
    aDoc.SetInt("musicVolume", std::llround(std::clamp(mMusicVolume, 0.0f, 1.0f) * 100.0));
    aDoc.SetInt("sfxVolume", std::llround(std::clamp(mSfxVolume / 0.65, 0.0, 1.0) * 100.0));
	aDoc.SetInt("muted", (mMuteCount - mAutoMuteCount > 0) ? 1 : 0);
	aDoc.SetString("screenMode", mIsWindowed ? "windowed" : "fullscreen");
	aDoc.SetInt("preferredX", mPreferredX);
	aDoc.SetInt("preferredY", mPreferredY);
	aDoc.SetInt("customCursors", mCustomCursorsEnabled ? 1 : 0);
	aDoc.SetInt("waitForVSync", mWaitForVSync ? 1 : 0);
	aDoc.SetInt("customScale", mUseCustomScale ? 1 : 0);
#ifdef _WIN32
	aDoc.SaveFile(SettingsPath());
#else
	if (!aDoc.SaveFile(SettingsPath()))
		throw std::runtime_error("Cannot write Linux settings file: " + SettingsPath());
#endif
}

#ifdef _WIN32
bool SexyAppBase::RegistryEraseKey(const SexyString& _theKeyName)
{
	std::string theKeyName = SexyStringToStringFast(_theKeyName);
	if (mRegKey.length() == 0)
		return false;

	if (mPlayingDemoBuffer)
	{
		if (mManualShutdown)
			return true;

		RequireDemoCommand(DEMO_REGISTRY_ERASE);

		return mDemoBuffer.ReadNumBits(1, false) != 0;		
	}	
	
	std::string aKeyName = RemoveTrailingSlash("SOFTWARE\\" + mRegKey) + "\\" + theKeyName;

	int aResult = RegDeleteKeyA(HKEY_CURRENT_USER, aKeyName.c_str());
	if (aResult != ERROR_SUCCESS)
	{
		if (mRecordingDemoBuffer)
		{
			WriteDemoTimingBlock();
			mDemoBuffer.WriteNumBits(0, 1);
			mDemoBuffer.WriteNumBits(DEMO_REGISTRY_ERASE, 5);
			mDemoBuffer.WriteNumBits(0, 1); // failure
		}

		return false;
	}		

	if (mRecordingDemoBuffer)
	{
		WriteDemoTimingBlock();
		mDemoBuffer.WriteNumBits(0, 1);
		mDemoBuffer.WriteNumBits(DEMO_REGISTRY_ERASE, 5);
		mDemoBuffer.WriteNumBits(1, 1); // success
	}

	return true;
}

void SexyAppBase::RegistryEraseValue(const SexyString& _theValueName)
{
	std::string theValueName = SexyStringToStringFast(_theValueName);
	if (mRegKey.length() == 0)
		return;

	HKEY aGameKey;	
	std::string aKeyName = RemoveTrailingSlash("SOFTWARE\\" + mRegKey);
	std::string aValueName;

	int aSlashPos = (int) theValueName.rfind('\\');
	if (aSlashPos != -1)
	{
		aKeyName += "\\" + theValueName.substr(0, aSlashPos);
		aValueName = theValueName.substr(aSlashPos + 1);
	}
	else
	{
		aValueName = theValueName;
	}

	int aResult = RegOpenKeyExA(HKEY_CURRENT_USER, aKeyName.c_str(), 0, KEY_WRITE, &aGameKey);
	if (aResult == ERROR_SUCCESS)
	{		
		RegDeleteValueA(aGameKey, aValueName.c_str());
		RegCloseKey(aGameKey);
	}
}

bool SexyAppBase::RegistryGetSubKeys(const std::string& theKeyName, StringVector* theSubKeys)
{
	theSubKeys->clear();

	if (mRegKey.length() == 0)
		return false;

	if (mPlayingDemoBuffer)
	{
		if (mManualShutdown)
			return true;

		RequireDemoCommand(DEMO_REGISTRY_GETSUBKEYS);

		bool success = mDemoBuffer.ReadNumBits(1, false) != 0;
		if (!success)
			return false;

		int aNumKeys = mDemoBuffer.ReadLong();

		for (int i = 0; i < aNumKeys; i++)
			theSubKeys->push_back(mDemoBuffer.ReadString());

		return true;
	}
	else
	{
		HKEY aKey;
		
		std::string aKeyName = RemoveTrailingSlash(RemoveTrailingSlash("SOFTWARE\\" + mRegKey) + "\\" + theKeyName);	
		int aResult = RegOpenKeyExA(HKEY_CURRENT_USER, aKeyName.c_str(), 0, KEY_READ, &aKey);
		
		if (aResult == ERROR_SUCCESS)
		{		
			for (int anIdx = 0; ; anIdx++)
			{
				char aStr[1024];

				aResult = RegEnumKeyA(aKey, anIdx, aStr, 1024);
				if (aResult != ERROR_SUCCESS)
					break;

				theSubKeys->push_back(aStr);
			}

			RegCloseKey(aKey);

			if (mRecordingDemoBuffer)
			{
				WriteDemoTimingBlock();
				mDemoBuffer.WriteNumBits(0, 1);
				mDemoBuffer.WriteNumBits(DEMO_REGISTRY_GETSUBKEYS, 5);
				mDemoBuffer.WriteNumBits(1, 1); // success
				mDemoBuffer.WriteLong(theSubKeys->size());

				for (int i = 0; i < (int) theSubKeys->size(); i++)
					mDemoBuffer.WriteString((*theSubKeys)[i]);				
			}

			return true;
		}
		else
		{
			if (mRecordingDemoBuffer)
			{
				WriteDemoTimingBlock();
				mDemoBuffer.WriteNumBits(0, 1);
				mDemoBuffer.WriteNumBits(DEMO_REGISTRY_GETSUBKEYS, 5);
				mDemoBuffer.WriteNumBits(0, 1); // failure
			}

			return false;	
		}
	}
}

bool SexyAppBase::RegistryRead(const std::string& theValueName, unsigned int* theType, uchar* theValue, unsigned int* theLength)
{
	return RegistryReadKey(theValueName, theType, theValue, theLength, HKEY_CURRENT_USER);
}

bool SexyAppBase::RegistryReadKey(const std::string& theValueName, unsigned int* theType, uchar* theValue, unsigned int* theLength, HKEY theKey)
{
	if (mRegKey.length() == 0)
		return false;

	if (mPlayingDemoBuffer)
	{
		if (mManualShutdown)
			return false;

		RequireDemoCommand(DEMO_REGISTRY_READ);

		bool success = mDemoBuffer.ReadNumBits(1, false) != 0;
		if (!success)
			return false;

		*theType = mDemoBuffer.ReadLong();

		unsigned int aLen = mDemoBuffer.ReadLong();
		*theLength = aLen;
		
		if (*theLength >= aLen)
		{			
			mDemoBuffer.ReadBytes(theValue, aLen);
			return true;
		}
		else
		{
			for (int i = 0; i < (int) aLen; i++)
				mDemoBuffer.ReadByte();
			return false;
		}		
	}
	else
	{		
		HKEY aGameKey;

		std::string aKeyName = RemoveTrailingSlash("SOFTWARE\\" + mRegKey);
		std::string aValueName;

		int aSlashPos = (int) theValueName.rfind('\\');
		if (aSlashPos != -1)
		{
			aKeyName += "\\" + theValueName.substr(0, aSlashPos);
			aValueName = theValueName.substr(aSlashPos + 1);
		}
		else
		{
			aValueName = theValueName;
		}		

		if (RegOpenKeyExA(theKey, aKeyName.c_str(), 0, KEY_READ, &aGameKey) == ERROR_SUCCESS)
		{
			DWORD nativeType = 0, nativeLength = *theLength;
			const auto result = RegQueryValueExA(aGameKey, aValueName.c_str(), 0,
				&nativeType, (uchar*)theValue, &nativeLength);
			*theType = static_cast<unsigned int>(nativeType);
			*theLength = static_cast<unsigned int>(nativeLength);
			if (result == ERROR_SUCCESS)
			{
				if (mRecordingDemoBuffer)
				{
					WriteDemoTimingBlock();
					mDemoBuffer.WriteNumBits(0, 1);
					mDemoBuffer.WriteNumBits(DEMO_REGISTRY_READ, 5);
					mDemoBuffer.WriteNumBits(1, 1); // success
					mDemoBuffer.WriteLong(*theType);
					mDemoBuffer.WriteLong(*theLength);
					mDemoBuffer.WriteBytes(theValue, *theLength);
				}

				RegCloseKey(aGameKey);
				return true;	
			}

			RegCloseKey(aGameKey);
		}

		if (mRecordingDemoBuffer)
		{
			WriteDemoTimingBlock();
			mDemoBuffer.WriteNumBits(0, 1);
			mDemoBuffer.WriteNumBits(DEMO_REGISTRY_READ, 5);
			mDemoBuffer.WriteNumBits(0, 1); // failure
		}
		
		return false;
	}
}

// aStr isn't initialised lmao
bool SexyAppBase::RegistryReadString(const std::string& theKey, std::string* theString)
{
	char aStr[1024];
	
	unsigned int aType;
	unsigned int aLen = sizeof(aStr) - 1;
	if (!RegistryRead(theKey, &aType, (uchar*) aStr, &aLen))
		return false;

	if (aType != REG_SZ)
		return false;

	aStr[aLen] = 0;

	*theString = aStr;
	return true;
}

bool SexyAppBase::RegistryReadInteger(const std::string& theKey, int* theValue)
{
	unsigned int aType;
	unsigned int aLong;
	unsigned int aLen = 4;
	if (!RegistryRead(theKey, &aType, (uchar*) &aLong, &aLen))
		return false;

	if (aType != REG_DWORD)
		return false;

	*theValue = aLong;
	return true;
}

bool SexyAppBase::RegistryReadBoolean(const std::string& theKey, bool* theValue)
{
	int aValue;
	if (!RegistryReadInteger(theKey, &aValue))
		return false;
	
	*theValue = aValue != 0;
	return true;
}

bool SexyAppBase::RegistryReadData(const std::string& theKey, uchar* theValue, unsigned int* theLength)
{		
	unsigned int aType;
	if (!RegistryRead(theKey, &aType, (uchar*) theValue, theLength))
		return false;

	if (aType != REG_BINARY)
		return false;
	
	return true;
}

#else
bool SexyAppBase::RegistryEraseKey(const SexyString&) { return false; }
void SexyAppBase::RegistryEraseValue(const SexyString&) {}
bool SexyAppBase::RegistryGetSubKeys(const std::string&, StringVector*) { return false; }
bool SexyAppBase::RegistryRead(const std::string&, unsigned int*, uchar*, unsigned int*) { return false; }
bool SexyAppBase::RegistryReadString(const std::string&, std::string*) { return false; }
bool SexyAppBase::RegistryReadInteger(const std::string&, int*) { return false; }
bool SexyAppBase::RegistryReadBoolean(const std::string&, bool*) { return false; }
bool SexyAppBase::RegistryReadData(const std::string&, uchar*, unsigned int*) { return false; }
#endif

static std::string LowerScaleText(const std::string& theText);

void SexyAppBase::DetectTargetFrameRate()
{
	if (!mAutoDetectFrameRate) return;
#ifdef _WIN32
	MONITORINFOEXW monitorInfo{};
	monitorInfo.cbSize=sizeof(monitorInfo);
	DEVMODEW mode{};
	mode.dmSize=sizeof(mode);
	const auto monitor=MonitorFromWindow(mHWnd,MONITOR_DEFAULTTONEAREST);
	const double refreshRate=monitor && GetMonitorInfoW(monitor,&monitorInfo) &&
		EnumDisplaySettingsW(monitorInfo.szDevice,ENUM_CURRENT_SETTINGS,&mode)
		? mode.dmDisplayFrequency : 0;
#else
	const double refreshRate=mNativeWindow ? mNativeWindow->RefreshRate() : 0;
#endif
	mFrameTiming=FrameTiming::FromDisplayRefreshRate(refreshRate);
	ApplySimulationStep(mFrameTiming.mIntervalNs);
	JsonDoc settings;
	if (!settings.LoadFile(SettingsPath())) return;
	settings.SetInt("targetFps",mFrameTiming.mTargetFps);
	settings.SetBool("autoDetectFrameRate",true);
	if (!settings.SaveFile(SettingsPath()))
		throw std::runtime_error("Cannot save detected target frame rate: " + SettingsPath());
}

void SexyAppBase::ReadFromRegistry()
{
	mReadFromRegistry = true;
	mRegKey.clear();

	// Settings live in settings.json next to the executable (self-contained,
	// no registry / user-profile dependency). 缺失/损坏 → 全默认（沿袭
	// XML 时代静默默认）；键缺失 → 成员保持构造默认，故逐键 Has 后再赋值。
	JsonDoc aDoc;
	if (!aDoc.LoadFile(SettingsPath().c_str()))
		return;
	mAutoDetectFrameRate=aDoc.GetBool("autoDetectFrameRate",true);
    mLandscapeUI=aDoc.GetBool("landscapeUI",false);
	bool settingsChanged=!aDoc.Has("autoDetectFrameRate");
	if (settingsChanged) aDoc.SetBool("autoDetectFrameRate",true);

	if (aDoc.GetType("targetFps") == JsonDoc::Type::Number)
	{
		const auto targetFps = aDoc.GetFloat("targetFps", FrameTiming::DefaultTargetFps);
		mFrameTiming = FrameTiming::FromSetting(targetFps);
		if (targetFps < FrameTiming::MinimumTargetFps)
		{
			aDoc.SetInt("targetFps", FrameTiming::MinimumTargetFps);
			settingsChanged=true;
		}
	}
#if defined(__ANDROID__)
    if (aDoc.GetBool("hdr", false)) {
        aDoc.SetBool("hdr", false);
        settingsChanged = true;
    }
#endif
	if (aDoc.Has("musicVolume"))
        mMusicVolume = static_cast<float>(std::clamp(aDoc.GetInt("musicVolume", 0), 0LL, 100LL)) / 100.0f;
	if (aDoc.Has("sfxVolume"))
        mSfxVolume = std::clamp(aDoc.GetInt("sfxVolume", 0), 0LL, 100LL) / 100.0 * 0.65;
	if (aDoc.Has("muted"))
		mMuteCount = (int)aDoc.GetInt("muted", 0);
	if (aDoc.Has("screenMode"))
		mIsWindowed = LowerScaleText(aDoc.GetString("screenMode", "")) == "windowed";
	if (aDoc.Has("preferredX"))
		mPreferredX = (int)aDoc.GetInt("preferredX", 0);
	if (aDoc.Has("preferredY"))
		mPreferredY = (int)aDoc.GetInt("preferredY", 0);
	if (aDoc.Has("customCursors"))
		EnableCustomCursors(aDoc.GetInt("customCursors", 0) != 0);
	if (aDoc.Has("waitForVSync"))
		mWaitForVSync = aDoc.GetInt("waitForVSync", 0) != 0;
	if (aDoc.Has("customScale"))
		mUseCustomScale = aDoc.GetInt("customScale", 0) != 0;
	if (settingsChanged && !aDoc.SaveFile(SettingsPath()))
		throw std::runtime_error("Cannot normalize frame rate settings: " + SettingsPath());
}

bool SexyAppBase::WriteBytesToFile(const std::string& theFileName, const void *theData, unsigned int theDataLen)
{
	if (mPlayingDemoBuffer)
	{
		if (mManualShutdown)
			return true;

		RequireDemoCommand(DEMO_FILE_WRITE);

		bool success = mDemoBuffer.ReadNumBits(1, false) != 0;
		if (!success)
			return false;

		return true;
	}	

	MkDir(GetFileDir(theFileName));
	std::string nativePath = theFileName;
#ifndef _WIN32
	std::replace(nativePath.begin(), nativePath.end(), '\\', '/');
#endif
	FILE* aFP = fopen(nativePath.c_str(), "w+b");

	if (aFP == NULL)
	{
		if (mRecordingDemoBuffer)
		{
			WriteDemoTimingBlock();
			mDemoBuffer.WriteNumBits(0, 1);
			mDemoBuffer.WriteNumBits(DEMO_FILE_WRITE, 5);
			mDemoBuffer.WriteNumBits(0, 1); // failure				
		}

		return false;
	}

	const size_t aWritten = fwrite(theData, 1, theDataLen, aFP);
	const int closeResult = fclose(aFP);
	if (aWritten != theDataLen || closeResult != 0)
	{
		// 写入不完整（磁盘满/文件被锁）：如实记失败位并返回，避免存档假成功
		if (mRecordingDemoBuffer)
		{
			WriteDemoTimingBlock();
			mDemoBuffer.WriteNumBits(0, 1);
			mDemoBuffer.WriteNumBits(DEMO_FILE_WRITE, 5);
			mDemoBuffer.WriteNumBits(0, 1); // failure
		}

		return false;
	}

	if (mRecordingDemoBuffer)
	{
		WriteDemoTimingBlock();
		mDemoBuffer.WriteNumBits(0, 1);
		mDemoBuffer.WriteNumBits(DEMO_FILE_WRITE, 5);
		mDemoBuffer.WriteNumBits(1, 1); // success
	}

	return true;
}

bool SexyAppBase::WriteBufferToFile(const std::string& theFileName, const Buffer* theBuffer)
{
	return WriteBytesToFile(theFileName,theBuffer->GetDataPtr(),theBuffer->GetDataLen());
}


bool SexyAppBase::ReadBufferFromFile(const std::string& theFileName, Buffer* theBuffer, bool dontWriteToDemo)
{
	if ((mPlayingDemoBuffer) && (!dontWriteToDemo))
	{
		if (mManualShutdown)
			return false;

		RequireDemoCommand(DEMO_FILE_READ);

		bool success = mDemoBuffer.ReadNumBits(1, false) != 0;
		if (!success)
			return false;

		unsigned int aLen = mDemoBuffer.ReadLong();
				
		theBuffer->Clear();
		for (int i = 0; i < (int) aLen; i++)
			theBuffer->WriteByte(mDemoBuffer.ReadByte());

		return true;		
	}
	else
	{
		std::unique_ptr<PFILE, decltype(&p_fclose)> aFile(p_fopen(theFileName.c_str(), "rb"), &p_fclose);
		PFILE* aFP = aFile.get();

		if (aFP == NULL)
		{
			if ((mRecordingDemoBuffer) && (!dontWriteToDemo))
			{
				WriteDemoTimingBlock();
				mDemoBuffer.WriteNumBits(0, 1);
				mDemoBuffer.WriteNumBits(DEMO_FILE_READ, 5);
				mDemoBuffer.WriteNumBits(0, 1); // failure				
			}

			return false;
		}
		
		if (p_fseek(aFP, 0, SEEK_END) != 0)
			throw BufferReadError{};
		int aFileSize = p_ftell(aFP);
		if (aFileSize < 0 || aFileSize > std::numeric_limits<int>::max() / 8 ||
			p_fseek(aFP, 0, SEEK_SET) != 0)
			throw BufferReadError{};
		
		ByteVector aData(static_cast<size_t>(aFileSize));

		if (aFileSize != 0 && p_fread(aData.data(), 1, aFileSize, aFP) != static_cast<size_t>(aFileSize))
			throw BufferReadError{};
		aFile.reset();

		theBuffer->Clear();
		theBuffer->SetData(std::move(aData));

		if ((mRecordingDemoBuffer) && (!dontWriteToDemo))
		{
			WriteDemoTimingBlock();
			mDemoBuffer.WriteNumBits(0, 1);
			mDemoBuffer.WriteNumBits(DEMO_FILE_READ, 5);
			mDemoBuffer.WriteNumBits(1, 1); // success			
			mDemoBuffer.WriteLong(aFileSize);
			mDemoBuffer.WriteBytes(theBuffer->GetDataPtr(), aFileSize);
		}

		return true;
	}
}

bool SexyAppBase::FileExists(const std::string& theFileName)
{
	if (mPlayingDemoBuffer)
	{
		if (mManualShutdown)
			return true;

		RequireDemoCommand(DEMO_FILE_EXISTS);

		bool success = mDemoBuffer.ReadNumBits(1, false) != 0;		
		return success;
	}
	else
	{
		PFILE* aFP = p_fopen(theFileName.c_str(), "rb");

		if (mRecordingDemoBuffer)
		{
			WriteDemoTimingBlock();
			mDemoBuffer.WriteNumBits(0, 1);
			mDemoBuffer.WriteNumBits(DEMO_FILE_EXISTS, 5);
			mDemoBuffer.WriteNumBits((aFP != NULL) ? 1 : 0, 1); 
		}

		if (aFP == NULL)		
			return false;		
		
		p_fclose(aFP);
		return true;
	}
}

bool SexyAppBase::EraseFile(const std::string& theFileName)
{
	if (mPlayingDemoBuffer)
		return true;

#ifdef _WIN32
	return DeleteFileA(theFileName.c_str()) != 0;
#else
	std::string path = theFileName;
	std::replace(path.begin(), path.end(), '\\', '/');
	return unlink(path.c_str()) == 0;
#endif
}

void SexyAppBase::ReleaseCursorClip()
{
#ifdef _WIN32
	if (!mCursorClipActive)
		return;

	// ClipCursor is process-global desktop state. Clear it only while the OS
	// still exposes the exact rectangle installed by this application; if a
	// different owner replaced it (or the query fails), drop only our claim.
	RECT anActualClip;
	if (GetClipCursor(&anActualClip) && EqualRect(&anActualClip, &mCursorClipRect))
		ClipCursor(nullptr);
	mCursorClipActive = false;
	SetRectEmpty(&mCursorClipRect);
#else
	mCursorClipActive = false; // Linux implicit button grabs belong to NativeWindow.
	mCursorClipRect = {};
#endif
}

void SexyAppBase::UpdateCursorClip()
{
#ifdef _WIN32
	const bool aCanClip = !mIsWindowed && !mUseExternalHWND && mHWnd != NULL &&
		!mShutdown && !mSEHOccured && !mMinimized && !mPhysMinimized &&
		!mIsDisabled && mActive && GetForegroundWindow() == mHWnd &&
		mDDInterface != nullptr;
	if (!aCanClip)
	{
		ReleaseCursorClip();
		return;
	}

	RECT aClientRect;
	if (!GetClientRect(mHWnd, &aClientRect))
	{
		ReleaseCursorClip();
		return;
	}
	const Rect& aGameRect = mDDInterface->mVisibleGameRect;
	RECT aVisible = {
		std::max((LONG)aGameRect.mX, aClientRect.left),
		std::max((LONG)aGameRect.mY, aClientRect.top),
		std::min((LONG)(aGameRect.mX + aGameRect.mWidth), aClientRect.right),
		std::min((LONG)(aGameRect.mY + aGameRect.mHeight), aClientRect.bottom)
	};
	if (aVisible.left >= aVisible.right || aVisible.top >= aVisible.bottom)
	{
		ReleaseCursorClip();
		return;
	}

	POINT aTopLeft = { aVisible.left, aVisible.top };
	POINT aBottomRight = { aVisible.right, aVisible.bottom };
	if (!ClientToScreen(mHWnd, &aTopLeft) || !ClientToScreen(mHWnd, &aBottomRight))
	{
		ReleaseCursorClip();
		return;
	}
	RECT aScreenClip = { aTopLeft.x, aTopLeft.y, aBottomRight.x, aBottomRight.y };

	HMONITOR aMonitor = MonitorFromWindow(mHWnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO aMonitorInfo;
	ZeroMemory(&aMonitorInfo, sizeof(aMonitorInfo));
	aMonitorInfo.cbSize = sizeof(aMonitorInfo);
	if (GetMonitorInfo(aMonitor, &aMonitorInfo))
	{
		aScreenClip.left = std::max(aScreenClip.left, aMonitorInfo.rcMonitor.left);
		aScreenClip.top = std::max(aScreenClip.top, aMonitorInfo.rcMonitor.top);
		aScreenClip.right = std::min(aScreenClip.right, aMonitorInfo.rcMonitor.right);
		aScreenClip.bottom = std::min(aScreenClip.bottom, aMonitorInfo.rcMonitor.bottom);
	}
	if (aScreenClip.left >= aScreenClip.right || aScreenClip.top >= aScreenClip.bottom)
	{
		ReleaseCursorClip();
		return;
	}
	RECT anActualClip;
	if (GetClipCursor(&anActualClip) && EqualRect(&anActualClip, &aScreenClip))
	{
		mCursorClipActive = true;
		mCursorClipRect = aScreenClip;
		return;
	}
	if (ClipCursor(&aScreenClip))
	{
		mCursorClipActive = true;
		mCursorClipRect = aScreenClip;
	}
#endif
}

void SexyAppBase::SEHOccured()
{
	ReleaseCursorClip();
	SetMusicVolume(0);
#ifdef _WIN32
	::ShowWindow(mHWnd, SW_HIDE);
#endif
	mSEHOccured = true;
	EnforceCursor();
}

std::string SexyAppBase::GetGameSEHInfo()
{
	int aSecLoaded = (RuntimeTick() - mTimeLoaded) / 1000;

	char aTimeStr[16];
	sprintf(aTimeStr, "%02d:%02d:%02d", (aSecLoaded/60/60), (aSecLoaded/60)%60, aSecLoaded%60);
	
	char aThreadIdStr[16];
#ifdef _WIN32
	snprintf(aThreadIdStr, sizeof(aThreadIdStr), "%X", mPrimaryThreadId);
#else
	snprintf(aThreadIdStr, sizeof(aThreadIdStr), "%zX", std::hash<NativeThreadId>{}(mPrimaryThreadId));
#endif

	std::string anInfoString = 
		"Product: " + mProdName + "\r\n" +		
		"Version: " + mProductVersion + "\r\n";			

	anInfoString +=
		"Time Loaded: " + std::string(aTimeStr) + "\r\n"
		"Fullscreen: " + (mIsWindowed ? std::string("No") : std::string("Yes")) + "\r\n"
		"Primary ThreadId: " + aThreadIdStr + "\r\n";	

	return anInfoString;						
}


void SexyAppBase::ShutdownHook()
{
}

void SexyAppBase::Shutdown()
{
	WriteDemoBuffer();  // 显式关闭路径最先落盘（atexit 链尾实测不可靠）；幂等可重复调用
	ReleaseCursorClip();
	if ((mPrimaryThreadId != NativeThreadId{}) && (CurrentNativeThreadIdentity() != mPrimaryThreadId))
	{
		mLoadingFailed = true;
	}
	else if (!mShutdown)
	{
		mExitToTop = true;
		mShutdown = true;
		ShutdownHook();

		if (mPlayingDemoBuffer)
		{
			//if the music/sfx volume is 0, then it means that in playback
			//someone pressed the "S" key to mute sounds (or that the 
			//sound volume was set to 0 in the first place). Out of politeness,
			//return the system sound volume to what it last was in the game.
			SetMusicVolume(mDemoMusicVolume);
			SetSfxVolume(mDemoSfxVolume);
		}

	// Blah

	if (mMusicInterface != NULL)
		mMusicInterface->StopAllMusic();
		
		if (mHWnd != NULL) 
		{
#ifdef _WIN32
			ShowWindow(mHWnd, SW_HIDE);			
#endif
		}

		if (mReadFromRegistry)
			WriteToRegistry();

		if (mDDInterface && mDDInterface->mRenderInterface)
			mDDInterface->mRenderInterface->Flush();
		CleanupScalePresent();
#ifdef _WIN32
        CloseWindowsHdr(mNativeHdrState);
#endif

		//ImageLib::CloseJPEG2000();
	}
}

void SexyAppBase::DoExit(int theCode)
{
	ReleaseCursorClip();
	exit(theCode);
}

void SexyAppBase::UpdateFrames()
{
	if (gUpdateIntervalNs > static_cast<unsigned long long>(INT64_MAX - mApplicationTimeNs))
		throw std::overflow_error("application clock exceeds nanosecond range");
	mApplicationTimeNs += static_cast<std::int64_t>(gUpdateIntervalNs);
	mUpdateCount++;
	SampleNativeMousePosition();

	if (!mMinimized)
	{
		mWidgetManager->UpdateFrame();
	}

	{
		mMusicInterface->Update();
	}
	{
		CleanSharedImages();
	}
}

void SexyAppBase::DoUpdateFramesF(float theFrac)
{
	if ((mVSyncUpdates) && (!mMinimized))
		mWidgetManager->UpdateFrameF(theFrac);	
}

bool SexyAppBase::DoUpdateFrames()
{
	RethrowLoadingThreadError();

	// Linux keeps the inherited scheduling policy throughout loading.
	// Only Windows has a legacy priority restoration at this transition.
	if (gScreenSaverActive)
		return false;

	if (mPlayingDemoBuffer)
	{
		if ((mLoadingThreadCompleted) && (!mLoaded) && (mDemoLoadingComplete))
		{			
			mLoaded = true;
#ifdef _WIN32
			::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_NORMAL);
#endif
			mYieldMainThread = false;
			LoadingThreadCompleted();
		}

		// Allow one update for a queued call-site command; overdue commands fail
		// explicitly on the next inspection instead of pinning this tick forever.
		if ((mLoaded == mDemoLoadingComplete) &&
		    (mUpdateCount != mLastDemoUpdateCnt || DemoTools::HasPendingCallsite(*this)))
		{
			UpdateFrames();		
			return true;
		}

		return false;
	}
	else
	{
		if ((mLoadingThreadCompleted) && (!mLoaded))
		{
#ifdef _WIN32
			::SetThreadPriority(::GetCurrentThread(), THREAD_PRIORITY_NORMAL);
#endif
			mLoaded = true;
			mYieldMainThread = false;
			LoadingThreadCompleted();

			if (mRecordingDemoBuffer)
			{
				WriteDemoTimingBlock();
				mDemoBuffer.WriteNumBits(0, 1);
				mDemoBuffer.WriteNumBits(DEMO_LOADING_COMPLETE, 5);	
			}
		}
				
		UpdateFrames();		
		return true;
	}
}

void SexyAppBase::Redraw(Rect* theClipRect)
{

    // Native frame ownership is not a DirectDraw lost-surface state. WSI
    // deferral is handled by GpuContext; real failures propagate to WinMain.
    if (mIsDrawing || mRedrawInProgress || mSwitchingScreenMode || mShutdown || gScreenSaverActive)
        return;
    if (!mDDInterface)
        throw std::logic_error("Redraw without a native display");
    mRedrawInProgress = true;
    struct Reset { bool& value; ~Reset() { value = false; } } reset{mRedrawInProgress};
    mDDInterface->Redraw(theClipRect);
}

///////////////////////////// FPS Stuff
static PerfTimer gFPSTimer;
static int gFrameCount;
static int gFPSDisplay;
static bool gForceDisplay = false;
static void CalculateFPS()
{
	gFrameCount++;

	_Font* aFont=gSexyAppBase->GetDebugFont();
	auto& render=*gSexyAppBase->mDDInterface->mRenderInterface;
	if (gFPSImage==NULL)
		gFPSImage=render.CreateRenderImage(192,aFont->mHeight+4);

	if (gFPSTimer.GetDuration() >= 1000 || gForceDisplay)
	{
		gFPSTimer.Stop();
		if (!gForceDisplay)
			gFPSDisplay = (int)(static_cast<double>(gFrameCount)*1000/gFPSTimer.GetDuration() + 0.5f);
		else
		{
			gForceDisplay = false;
			gFPSDisplay = 0;
		}

		gFPSTimer.Start();
		gFrameCount = 0;

		GpuImageDrawScope drawing(render,gFPSImage);
		Graphics aDrawG(gFPSImage);
		aDrawG.mFont = aFont;
		SexyString aFPS = StrFormat(("FPS: %d"), gFPSDisplay);
		aDrawG.mColor = Color(0x000000);
		aDrawG.FillRect(0,0,gFPSImage->mWidth,gFPSImage->mHeight);
		aDrawG.mColor = Color(0xFFFFFF);
		aDrawG.DrawString(aFPS,2,aFont->mAscent);
	}
}

///////////////////////////// FPS Stuff to draw mouse coords
static void FPSDrawCoords(int theX, int theY)
{
	_Font* aFont=gSexyAppBase->GetDebugFont();
	auto& render=*gSexyAppBase->mDDInterface->mRenderInterface;
	if (gFPSImage==NULL)
		gFPSImage=render.CreateRenderImage(192,aFont->mHeight+4);

	GpuImageDrawScope drawing(render,gFPSImage);
	Graphics aDrawG(gFPSImage);
	aDrawG.mFont = aFont;
	SexyString aFPS = StrFormat(("%d,%d"),theX,theY);
	aDrawG.mColor = Color(0x000000);
	aDrawG.FillRect(0,0,gFPSImage->mWidth,gFPSImage->mHeight);
	aDrawG.mColor = Color(0xFFFFFF);
	aDrawG.DrawString(aFPS,2,aFont->mAscent);
}

///////////////////////////// Demo TimeLeft Stuff
static void CalculateDemoTimeLeft()
{
	_Font* aFont=gSexyAppBase->GetDebugFont();
	auto& render=*gSexyAppBase->mDDInterface->mRenderInterface;
	static DWORD aLastTick = 0;

	if (gDemoTimeLeftImage==NULL)
		gDemoTimeLeftImage=render.CreateRenderImage(192,aFont->mHeight+4);

	DWORD aTick = RuntimeTick();
	if (aTick - aLastTick < 1000/gSexyAppBase->mUpdateMultiplier)
		return;

	aLastTick = aTick;

	const auto aNumUpdatesLeft = FrameCounterMath::BeforeEnd(gSexyAppBase->mDemoLength, gSexyAppBase->mUpdateCount);
	GpuImageDrawScope drawing(render,gDemoTimeLeftImage);
	Graphics aDrawG(gDemoTimeLeftImage);
	aDrawG.mFont = aFont;

	const std::uint64_t aTotalSeconds = static_cast<std::uint64_t>(aNumUpdatesLeft) * gSexyAppBase->mFrameTime / 1000;
	const auto aSeconds = aTotalSeconds%60;
	const auto aMinutes = (aTotalSeconds/60)%60;
	const auto anHours = (aTotalSeconds/3600);

	SexyString aFPS = StrFormat(("%02llu:%02llu:%02llu"), static_cast<unsigned long long>(anHours), static_cast<unsigned long long>(aMinutes), static_cast<unsigned long long>(aSeconds));
	aDrawG.mColor = Color(0x000000);
	aDrawG.FillRect(0,0,gDemoTimeLeftImage->mWidth,gDemoTimeLeftImage->mHeight);
	aDrawG.mColor = Color(0xFFFFFF);
	aDrawG.DrawString(aFPS,2,aFont->mAscent);
}

static void UpdateScreenSaverInfo(DWORD theTick)
{
#ifdef _WIN32
	if (gSexyAppBase->mIsScreenSaver || !gSexyAppBase->mIsPhysWindowed)
		return;

	// Get screen saver timeout		
	static DWORD aPeriodicTick = 0;
	static DWORD aScreenSaverTimeout = 60000;
	static BOOL aScreenSaverEnabled = TRUE;

	if (theTick-aPeriodicTick > 10000)
	{
		aPeriodicTick = theTick;

		int aTimeout = 0;

		SystemParametersInfo(SPI_GETSCREENSAVETIMEOUT,0,&aTimeout,0);
		SystemParametersInfo(SPI_GETSCREENSAVEACTIVE,0,&aScreenSaverEnabled,0);
		aTimeout-=2;

		if (aTimeout < 1)
			aTimeout = 1;

		aScreenSaverTimeout = aTimeout*1000;

		if (!aScreenSaverEnabled)
			gScreenSaverActive = false;

	}

	// Get more accurate last user input time
	if (gGetLastInputInfoFunc)
	{
		LASTINPUTINFO anInfo;
		anInfo.cbSize = sizeof(anInfo);
		if (gGetLastInputInfoFunc(&anInfo))
		{
			// anInfo.dwTime is on the GetTickCount clock, while theTick
			// (SexyGetTickCount) is QPC-derived. Translating before comparing
			// keeps the screensaver idle test on one clock; subtracting the
			// raw values made the game look idle for the whole clock offset
			// and spuriously froze it in a 100% CPU busy loop.
			DWORD aNowGetTick = GetTickCount();
			DWORD aLastInput = anInfo.dwTime;
			if (aLastInput > aNowGetTick)
				aLastInput = aNowGetTick;

			DWORD aQpcLastInput = theTick - (aNowGetTick - aLastInput);
			gSexyAppBase->mLastUserInputTick = std::max(gSexyAppBase->mLastUserInputTick, aQpcLastInput);
		}
	}

	if (!aScreenSaverEnabled)
		return;

	DWORD anIdleTime = theTick - gSexyAppBase->mLastUserInputTick;
	if (gScreenSaverActive)
	{
		BOOL aBool = FALSE;
		if (SystemParametersInfo(SPI_GETSCREENSAVERRUNNING, 0, &aBool, 0))
		{
			if (aBool) // screen saver not off yet
				return;
		}

		if (anIdleTime < aScreenSaverTimeout)
		{
			gScreenSaverActive = false;
			gSexyAppBase->mWidgetManager->MarkAllDirty();
		}
	}
	else if (anIdleTime > aScreenSaverTimeout)
	{
		gScreenSaverActive = true;
	}
#else
	(void)theTick;
	// Session idle/inhibit policy belongs to the compositor; no Win32 query.
#endif
}

bool SexyAppBase::DrawDirtyStuff()
{
	MTAutoDisallowRand aDisallowRand;
    // Each scheduled draw gets one screen admission attempt. Once deferred,
    // no later FPS/overlay primitive may start a partial replacement frame.
    auto* render = mDDInterface->mRenderInterface;
    render->mScreenDrawDeferred = false;


	if (mShowFPS)
	{
		switch(mShowFPSMode)
		{
			case FPS_ShowFPS: CalculateFPS(); break;
			case FPS_ShowCoords:
				if (mWidgetManager!=NULL)
					FPSDrawCoords(mWidgetManager->mLastMouseX, mWidgetManager->mLastMouseY); 
				break;
		}

		if (mPlayingDemoBuffer)
			CalculateDemoTimeLeft();
	}

	DWORD aStartTime = SexyGetTickCount();

	// Update user input and screen saver info
	static DWORD aPeriodicTick = 0;
	if (aStartTime-aPeriodicTick > 1000)
	{
		aPeriodicTick = aStartTime;
		UpdateScreenSaverInfo(aStartTime);
	}

	if (gScreenSaverActive)
	{
		mHasPendingDraw = false;
		mLastDrawWasEmpty = true;		
		return false;
	}

	mIsDrawing = true;
    bool drewScreen = mWidgetManager->DrawScreen();
	mIsDrawing = false;
    if (render->mScreenDrawDeferred)
    {
        // WidgetManager preflights before consuming dirty flags. Keep those
        // flags for the next scheduled draw and do not count an absent frame.
        mHasPendingDraw = false;
        mLastDrawWasEmpty = true;
        return false;
    }


	if ((drewScreen || (aStartTime - mLastDrawTick >= 1000) || (mCustomCursorDirty)) &&
		((int) (aStartTime - mNextDrawTick) >= 0))
	{
		mLastDrawWasEmpty = false;

		mDrawCount++;		


		if (mShowFPS)
		{
			Graphics g(mDDInterface->mScreenImage);
			g.DrawImage(gFPSImage,mWidth-gFPSImage->mWidth-10,mHeight-gFPSImage->mHeight-10);
		
			if (mPlayingDemoBuffer)
				g.DrawImage(gDemoTimeLeftImage,mWidth-gDemoTimeLeftImage->mWidth-10,mHeight-gFPSImage->mHeight-gDemoTimeLeftImage->mHeight-15);
		}

		DWORD aPreScreenBltTime = SexyGetTickCount();
		mLastDrawTick = aPreScreenBltTime;

		Redraw(NULL);		

		// Capture presentation time; Process also samples before scheduling.
		UpdateFTimeAcc(); 

		DWORD aEndTime = SexyGetTickCount();


		if ((mLoadingThreadStarted) && (!mLoadingThreadCompleted))
		{
			int aTotalTime = aEndTime - aStartTime;

			mNextDrawTick += 35 + std::max(aTotalTime, 15);

			if ((int) (aEndTime - mNextDrawTick) >= 0)			
				mNextDrawTick = aEndTime;			

		}
		else
			mNextDrawTick = aEndTime;

		mHasPendingDraw = false;		
		mCustomCursorDirty = false;

		return true;
	}
	else
	{		
		mHasPendingDraw = false;
		mLastDrawWasEmpty = true;		
		return false;
	}
}

void SexyAppBase::LogScreenSaverError(const std::string &theError)
{
	static bool firstTime = true;
	char aBuf[512];

	const char *aFlag = firstTime?"w":"a+";
	firstTime = false;

	FILE *aFile = fopen(GetLogFilePath("ScrError.txt").c_str(),aFlag);
	if (aFile != NULL)
	{
		const auto now = std::time(nullptr);
		std::strftime(aBuf, sizeof(aBuf), "%H:%M:%S", std::localtime(&now));
		fprintf(aFile,"%s %s %u\n",theError.c_str(),aBuf,RuntimeTick());
		fclose(aFile);
	}
}

void SexyAppBase::BeginPopup()
{
	ReleaseCursorClip();
	if (!mIsPhysWindowed)
	{
		mNoDefer = true;
	}
}

void SexyAppBase::EndPopup()
{
	if (!mIsPhysWindowed)
		mNoDefer = false;

	ClearUpdateBacklog();
	ClearKeysDown();

	if (mWidgetManager->mDownButtons)
	{
		mWidgetManager->DoMouseUps();
#ifdef _WIN32
		ReleaseCapture();
#endif
	}
	UpdateCursorClip();
}

int SexyAppBase::MsgBox(const std::string& theText, const std::string& theTitle, int theFlags)
{
//	if (mDDInterface && mDDInterface->mDD)
//		mDDInterface->mDD->FlipToGDISurface();
	if (mIsScreenSaver)
	{
		LogScreenSaverError(theText);
		return IDOK;
	}

	BeginPopup();
#ifdef _WIN32
	int aResult = MessageBoxA(mHWnd, theText.c_str(), theTitle.c_str(), theFlags);
#else
	(void)theFlags;
	fprintf(stderr, "%s: %s\n", theTitle.c_str(), theText.c_str());
	ShowNativeErrorDialog(theText.c_str(), theTitle.c_str());
	int aResult = IDOK;
#endif
	EndPopup();

	return aResult;
}

int SexyAppBase::MsgBox(const std::wstring& theText, const std::wstring& theTitle, int theFlags)
{
//	if (mDDInterface && mDDInterface->mDD)
//		mDDInterface->mDD->FlipToGDISurface();
	if (mIsScreenSaver)
	{
		LogScreenSaverError(WStringToString(theText));
		return IDOK;
	}

	BeginPopup();
#ifdef _WIN32
	int aResult = MessageBoxW(mHWnd, theText.c_str(), theTitle.c_str(), theFlags);
#else
	(void)theFlags;
	fprintf(stderr, "%s: %s\n", WStringToString(theTitle).c_str(), WStringToString(theText).c_str());
	ShowNativeErrorDialog(WStringToString(theText).c_str(), WStringToString(theTitle).c_str());
	int aResult = IDOK;
#endif
	EndPopup();

	return aResult;
}

void SexyAppBase::Popup(const std::string& theString)
{
	if (mIsScreenSaver)
	{
		LogScreenSaverError(theString);
		return;
	}

	BeginPopup();
	if (!mShutdown)
#ifdef _WIN32
		::MessageBoxA(mHWnd, theString.c_str(), SexyStringToString((("FATAL ERROR"))).c_str(), MB_APPLMODAL | MB_ICONSTOP);
#else
	{
		fprintf(stderr, "FATAL ERROR: %s\n", theString.c_str());
		ShowNativeErrorDialog(theString.c_str(), "pvz_perf - Fatal error");
	}
#endif
	EndPopup();
}

void SexyAppBase::Popup(const std::wstring& theString)
{
	if (mIsScreenSaver)
	{
		LogScreenSaverError(WStringToString(theString));
		return;
	}

	BeginPopup();
	if (!mShutdown)
#ifdef _WIN32
		::MessageBoxW(mHWnd, theString.c_str(), SexyStringToWString((("FATAL ERROR"))).c_str(), MB_APPLMODAL | MB_ICONSTOP);
#else
	{
		fprintf(stderr, "FATAL ERROR: %s\n", WStringToString(theString).c_str());
		ShowNativeErrorDialog(WStringToString(theString).c_str(), "pvz_perf - Fatal error");
	}
#endif
	EndPopup();
}

void SexyAppBase::SafeDeleteWidget(Widget* theWidget)
{
	WidgetSafeDeleteInfo aWidgetSafeDeleteInfo;
	aWidgetSafeDeleteInfo.mUpdateAppDepth = mUpdateAppDepth;
	aWidgetSafeDeleteInfo.mWidget = theWidget;
	mSafeDeleteList.push_back(aWidgetSafeDeleteInfo);
}

void SexyAppBase::HandleNotifyGameMessage(int theType)
{
#ifdef _WIN32
	if (theType==0) // bring to front message
	{
		WINDOWPLACEMENT aWindowPlacement;
		aWindowPlacement.length = sizeof(WINDOWPLACEMENT);
		GetWindowPlacement(mHWnd, &aWindowPlacement);		

		if (aWindowPlacement.showCmd == SW_SHOWMINIMIZED)
			ShowWindow(mHWnd, SW_RESTORE);

		::SetForegroundWindow(mHWnd);
	}
#else
	(void)theType;
#endif
}

void SexyAppBase::RehupFocus()
{
	bool wantHasFocus = mActive && !mMinimized;

	if (wantHasFocus != mHasFocus)
	{					
		mHasFocus = wantHasFocus;

		if (mHasFocus)
		{
			if (mMuteOnLostFocus)
				Unmute(true);
	
			mWidgetManager->GotFocus();
			GotFocus();
		}
		else
		{
			if (mMuteOnLostFocus)
				Mute(true);
	
			mWidgetManager->LostFocus();
			LostFocus();

#ifdef _WIN32
			ReleaseCapture();
#endif
			mWidgetManager->DoMouseUps();
		}
	}
	UpdateCursorClip();
}

void SexyAppBase::ClearKeysDown()
{
	if (mWidgetManager != NULL) // fix stuck alt-key problem
	{
		for (int aKeyNum = 0; aKeyNum < 0xFF; aKeyNum++)
			mWidgetManager->mKeyDown[aKeyNum] = false;
	}
	mCtrlDown = false;
	mAltDown = false;
}

void SexyAppBase::WriteDemoTimingBlock()
{
	// Demo writing functions can only be called from the main thread and after SexyAppBase::Init
	DBG_ASSERTE(CurrentNativeThreadIdentity() == mPrimaryThreadId);

	while (mUpdateCount - mLastDemoUpdateCnt > 15)
	{
		mDemoBuffer.WriteNumBits(15, 4);
		mLastDemoUpdateCnt += 15;

		mDemoBuffer.WriteNumBits(0, 1);
		mDemoBuffer.WriteNumBits(DEMO_IDLE, 5);
		mDemoCmdOrder++;
	}
	
	mDemoBuffer.WriteNumBits(mUpdateCount - mLastDemoUpdateCnt, 4);
	mLastDemoUpdateCnt = mUpdateCount;
	mDemoCmdOrder++;
}

bool SexyAppBase::PrepareDemoCommand(bool required)
{
    if (mDemoNeedsCommand)
    {
        DemoTools::ReplayBits bits(*this);
        const auto delta = bits.Read(4);
        const bool shortCommand = bits.Read(1) != 0;
        const int command = static_cast<int>(bits.Read(shortCommand ? 1 : 5));
        DemoTools::ValidatePayload(*this, command, shortCommand, bits);

        mDemoCmdBitPos = mDemoBuffer.mReadBitPos;
        mLastDemoUpdateCnt += delta;
        mDemoIsShortCmd = shortCommand;
        mDemoCmdNum = command;
        mDemoBuffer.mReadBitPos = bits.pos;
        mDemoNeedsCommand = false;
        ++mDemoCmdOrder;
    }

    // Positions wrap at 32 bits. A future tick across UINT_MAX is not overdue.
    const auto elapsed = mUpdateCount - mLastDemoUpdateCnt;
    if (elapsed != 0 && elapsed < 0x80000000U)
        DemoTools::ReplayError(*this, "unconsumed command passed its recorded tick");
    if (required && elapsed != 0)
        DemoTools::ReplayError(*this, "caller requested a future command");
    return elapsed == 0;
}

void SexyAppBase::RequireDemoCommand(int expected)
{
    PrepareDemoCommand(true);
    if (mDemoIsShortCmd || mDemoCmdNum != expected)
        DemoTools::ReplayError(*this, "wrong caller; expected command " + std::to_string(expected));
    mDemoNeedsCommand = true;
}

void SexyAppBase::ProcessDemo()
{
	if (mPlayingDemoBuffer)
	{
		while (!mShutdown)
		{
			if (mDemoNeedsCommand && mDemoBuffer.AtEnd())
				DemoTools::ReplayError(*this, "stream ended without a close command");
			if (PrepareDemoCommand(false))
			{
				// Keep one parsed header; its owner may run after a modal wait.
				if (DemoTools::HasPendingCallsite(*this))
					return;
				mDemoNeedsCommand = true;
				
				if (mDemoIsShortCmd)
				{
					switch (mDemoCmdNum)
					{
					case 0:
						{
							int aDeltaX = mDemoBuffer.ReadNumBits(6, true);
							int aDeltaY = mDemoBuffer.ReadNumBits(6, true);
							mLastDemoMouseX += aDeltaX;
							mLastDemoMouseY += aDeltaY;

							mWidgetManager->MouseMove(mLastDemoMouseX, mLastDemoMouseY);
						}
						break;
					case 1:
						{
							bool down = mDemoBuffer.ReadNumBits(1, false) != 0;
							int aBtnCount = mDemoBuffer.ReadNumBits(3, true);							
		
							if (down)
								mWidgetManager->MouseDown(mLastDemoMouseX, mLastDemoMouseY, aBtnCount);
							else
								mWidgetManager->MouseUp(mLastDemoMouseX, mLastDemoMouseY, aBtnCount);
						}
						break;
					}
				}
				else
				{				
					switch (mDemoCmdNum)
					{
					case DEMO_MOUSE_POSITION:
						{
							mLastDemoMouseX = mDemoBuffer.ReadNumBits(12, false);
							mLastDemoMouseY = mDemoBuffer.ReadNumBits(12, false);

							mWidgetManager->MouseMove(mLastDemoMouseX, mLastDemoMouseY);						
						}
						break;
					case DEMO_ACTIVATE_APP:
						{
							mActive = mDemoBuffer.ReadNumBits(1, false) != 0;

							RehupFocus();
							
							if ((mActive) && (!mIsWindowed))
								mWidgetManager->MarkAllDirty();

						}
						break;
					case DEMO_SIZE:
						{
							bool isMinimized = mDemoBuffer.ReadBoolean();

							if ((!mShutdown) && (isMinimized != mMinimized))
							{
								mMinimized = isMinimized;

								// We don't want any sounds (or music) playing while its minimized
								if (mMinimized)
									Mute(true);
								else
								{
									Unmute(true);
									mWidgetManager->MarkAllDirty();
								}
							}

							RehupFocus();
						}
						break;
					case DEMO_MOUSE_WHEEL:
						{
							int aScroll = mDemoBuffer.ReadNumBits(8, true);
							mWidgetManager->MouseWheel(aScroll);
						}
						break;
					case DEMO_KEY_DOWN:
						{
							KeyCode aKeyCode = (KeyCode) mDemoBuffer.ReadNumBits(8, false);
							mWidgetManager->KeyDown(aKeyCode);
						}
						break;
					case DEMO_KEY_UP:
						{
							KeyCode aKeyCode = (KeyCode) mDemoBuffer.ReadNumBits(8, false);
							mWidgetManager->KeyUp(aKeyCode);
						}
						break;
					case DEMO_KEY_CHAR:
						{
							int sizeMult = (int)mDemoBuffer.ReadNumBits(1, false) + 1; // will be 1 for single, 2 for double
							SexyChar aChar = (SexyChar) mDemoBuffer.ReadNumBits(8*sizeMult, false);
							mWidgetManager->KeyChar(aChar);
						}
						break;
					case DEMO_CLOSE:
						Shutdown();
						break;
					case DEMO_MOUSE_ENTER:
						mMouseIn = true;
						EnforceCursor();
						break;
					case DEMO_MOUSE_EXIT:
						mWidgetManager->MouseExit(mLastDemoMouseX, mLastDemoMouseY);
						mMouseIn = false;
						EnforceCursor();
						break;
					case DEMO_LOADING_COMPLETE:
						mDemoLoadingComplete = true;
						break;
					case DEMO_VIDEO_DATA:
						mIsWindowed = mDemoBuffer.ReadBoolean();
						mSyncRefreshRate = mDemoBuffer.ReadByte();
						break;
					case DEMO_IDLE:
						break;
					default:
						DemoTools::ReplayError(*this, "invalid input command");
					}
				}
			}
			else
				break; // Cached header belongs to a future tick.
		}
	}
}

void SexyAppBase::ShowMemoryUsage()
{
    if (DeferImageDiagnostics(2)) return;
    ImageDiagnosticActivity active(mImageDiagnosticsActive);
    const auto records = CaptureImageDiagnostics(false);
    if (!records) { mPendingImageDiagnostics |= 2; return; }
    const auto text = std::string("Renderer: Vulkan (native GPU)\r\n\r\n") +
        DescribeImageDiagnostics(*records);
    MsgBox(text, "Image storage diagnostics", MB_OK);
    mLastTime = SexyGetTickCount();
}

bool SexyAppBase::IsAltKeyUsed(WPARAM wParam)
{
	int aChar = tolower(wParam);
	switch (aChar)
	{
		case 13: // alt-enter
		case 'r': 
			return true;		
		default: 
			return false;
	}
}

void SexyAppBase::DispatchKeyDown(int theKey)
{
#ifdef __linux__
	// Ctrl+Alt+D is a desktop shortcut on several Linux desktops. Consume the
	// replacement chord and only toggle once until D is physically released.
	if (theKey == 'D' && mWidgetManager->mKeyDown[KEYCODE_CONTROL] &&
		mWidgetManager->mKeyDown[KEYCODE_SHIFT] && !mWidgetManager->mKeyDown[KEYCODE_MENU])
	{
		if (!mWidgetManager->mKeyDown['D'])
			mDebugKeysEnabled = !mDebugKeysEnabled;
		mWidgetManager->mKeyDown['D'] = true;
		return;
	}
#else
	if (theKey == 'D' && mWidgetManager->mKeyDown[KEYCODE_CONTROL] &&
		mWidgetManager->mKeyDown[KEYCODE_MENU])
		mDebugKeysEnabled = !mDebugKeysEnabled;
#endif
	if (!mDebugKeysEnabled || !DebugKeyDown(theKey))
		mWidgetManager->KeyDown(static_cast<KeyCode>(theKey));
}

bool SexyAppBase::DebugKeyDown(int theKey)
{
	if ((theKey == 'R') && (mWidgetManager->mKeyDown[KEYCODE_MENU]))
	{	
#if !defined(RELEASEFINAL) && !defined(SEXY_DISABLE_MODVAL)
		if (ReparseModValues())
		{
		}
		else
		{
			for (int aKeyNum = 0; aKeyNum < 0xFF; aKeyNum++) // prevent alt from getting stuck
				mWidgetManager->mKeyDown[aKeyNum] = false;
		}
#endif
	}
	else if (theKey == KEYCODE_F3)
	{
		if(mWidgetManager->mKeyDown[KEYCODE_SHIFT])
		{
			mShowFPS = true;
			if (++mShowFPSMode >= Num_FPS_Types)
				mShowFPSMode = 0;
		}
		else
			mShowFPS = !mShowFPS;

		mWidgetManager->MarkAllDirty();

		if (mShowFPS)
		{
			gFPSTimer.Start();
			gFrameCount = 0;
			gFPSDisplay = 0;
			gForceDisplay = true;
		}
	}
	else if (theKey == KEYCODE_F8)
	{
		// Retain the diagnostic shortcut; there is no software/DX mode to toggle.
		ShowMemoryUsage();

		return true;
	}
	else if (theKey == KEYCODE_F10)
	{
#ifndef RELEASEFINAL
		if (mWidgetManager->mKeyDown[KEYCODE_CONTROL])
		{
			if (mUpdateMultiplier==0.25)
				mUpdateMultiplier = 1.0;
			else
				mUpdateMultiplier = 0.25;
		}
		else if(mWidgetManager->mKeyDown[KEYCODE_SHIFT])
		{
			mStepMode = 0;
			ClearUpdateBacklog();
		}
		else
			mStepMode = 1;
#endif

		return true;
	}
	else if (theKey == KEYCODE_F11)
	{
		if (mWidgetManager->mKeyDown[KEYCODE_SHIFT])
			DumpProgramInfo();
		else
			TakeScreenshot();

		return true;
	}
	else
		return false;

	return false;
}

/*
bool SexyAppBase::DebugKeyDownAsync(int theKey, bool ctrlDown, bool altDown)
{
	return false;
}
*/

void SexyAppBase::CloseRequestAsync()
{
}

void SexyAppBase::Done3dTesting()
{
}










void SexyAppBase::LoadingThreadProc()
{
}

void SexyAppBase::LoadingThreadCompleted()
{
}

void SexyAppBase::LoadingThreadProcStub(void *theArg)
{
	EnableDenormalFlushForCurrentThread();
	SexyAppBase* aSexyApp = (SexyAppBase*) theArg;
	try
	{
		aSexyApp->LoadingThreadProc();
	}
	catch (...)
	{
		// Publish the original exception before completion; never let it escape
		// std::thread, or mark a failed load as a successful resource barrier.
		aSexyApp->mLoadingThreadError = std::current_exception();
		aSexyApp->mLoadingFailed.store(true, std::memory_order_release);
	}

	// Release publishes every resource initialized by LoadingThreadProc before
	// the main thread observes completion.
	aSexyApp->mLoadingThreadCompleted.store(true, std::memory_order_release);
}

void SexyAppBase::RethrowLoadingThreadError()
{
	if (mLoadingThreadCompleted.load(std::memory_order_acquire) && mLoadingThreadError)
		std::rethrow_exception(mLoadingThreadError);
}

void SexyAppBase::StartLoadingThread()
{	
	if (!mLoadingThreadStarted.exchange(true, std::memory_order_acq_rel))
	{
		// Decode and upload need equal scheduling priority on a single core.
		// Raising only the main thread can starve the normal-priority loader.
		try
		{
			mLoadingThread = std::thread(LoadingThreadProcStub, this);
		}
		catch (...)
		{
			mLoadingFailed.store(true, std::memory_order_release);
			mLoadingThreadCompleted.store(true, std::memory_order_release);
			throw;
		}
	}
}

void SexyAppBase::RequestScreenMode(bool wantWindowed, bool useCustomScale)
{
	if (mSwitchingScreenMode)
		return;

	bool anEffectiveWindowed = wantWindowed;
	if (mForceWindowed)
		anEffectiveWindowed = true;
	else if (mForceFullscreen)
		anEffectiveWindowed = false;

	const bool aScaleChanged = mUseCustomScale != useCustomScale;

	// Normalize input before either source enters the transaction. This is a
	// state cancellation, not a synthetic MouseUp: Alt+Enter while dragging must
	// not depress the widget that happened to own capture.
#ifdef _WIN32
	if (GetCapture() == mHWnd)
		ReleaseCapture();
#endif
	if (mWidgetManager != nullptr)
		mWidgetManager->CancelMouseDownState();
	ClearKeysDown();

    if (aScaleChanged) {
        if (anEffectiveWindowed==mIsWindowed) {
            ReconfigureScale(useCustomScale);
        } else {
            // Commit both choices in the single mode transaction below. Do not
            // resize/present an intermediate window for the old mode first.
            auto& render=*mDDInterface->mRenderInterface;
            render.Flush();
            mUseCustomScale=useCustomScale;
            mWindowScale=mUseCustomScale?GetWindowScaleForCustomScale():1;
            PrepareScalePipelineObjects();
            if (mUseCustomScale) {
                render.mRenderer->WarmScale(false);
                render.mRenderer->WarmScale(true);
            }
        }
    }
    // The virtual completion hook still synchronizes the options checkboxes.
    // ReconfigureScale already completed a scale-only resize; do not do it twice.
    SwitchScreenMode(anEffectiveWindowed,false);
	if (mShutdown)
		return;

#ifdef _WIN32
    if (mHWnd)
        SetFocus(mHWnd);
#endif
	if (mSoundManager != NULL)
		mSoundManager->SetCooperativeWindow(mHWnd);

	// Persist the mode/scale pair only after both public states describe the
	// committed transaction. A no-op still saves unrelated options fields.
	WriteToRegistry();
	ClearUpdateBacklog(false);
	UpdateCursorClip();
}

// Sizes the existing HWND for the requested screen mode without destroying
// it. Windowed uses the logical size times the custom-scale multiplier;
// fullscreen is a borderless window covering the desktop (matching the
// runtime behavior of the original: no display-mode change on toggle).
bool SexyAppBase::ResizeAppWindowForMode(bool wantWindowed)
{
#ifdef _WIN32
	if (mHWnd == NULL)
		return false;

	// Preserve visibility. GWL_STYLE is replaced wholesale below, and dropping
	// WS_VISIBLE makes the window disappear (clicks then fall through to the
	// desktop while DWM keeps compositing the last frame).
	::SetLastError(ERROR_SUCCESS);
	const DWORD aCurrentStyle = (DWORD)::GetWindowLongPtr(mHWnd, GWL_STYLE);
	if (aCurrentStyle == 0 && ::GetLastError() != ERROR_SUCCESS)
	{
		return false;
	}
	const bool aWasVisible = (aCurrentStyle & WS_VISIBLE) != 0;

	DWORD aWindowStyle;
	int aWidth;
	int aHeight;
	int aPlaceX;
	int aPlaceY;

	if (wantWindowed)
	{
		aWindowStyle = WS_CLIPCHILDREN | WS_POPUP | WS_BORDER | WS_CAPTION |
			WS_SYSMENU | WS_MINIMIZEBOX;
		if (mEnableMaximizeButton)
			aWindowStyle |= WS_MAXIMIZEBOX;

		int aClientW = mWidth * (mWindowScale > 1 ? mWindowScale : 1);
		int aClientH = mHeight * (mWindowScale > 1 ? mWindowScale : 1);

		RECT aRect;
		aRect.left = 0;
		aRect.top = 0;
		aRect.right = aClientW;
		aRect.bottom = aClientH;
		if (!AdjustWindowRect(&aRect, aWindowStyle, FALSE))
		{
			return false;
		}
		aWidth = aRect.right - aRect.left;
		aHeight = aRect.bottom - aRect.top;

		RECT aDesktopRect;
		if (!::SystemParametersInfo(SPI_GETWORKAREA, 0, &aDesktopRect, 0))
		{
			return false;
		}

		aPlaceX = 64;
		aPlaceY = 64;
		if (mPreferredX != -1)
		{
			aPlaceX = mPreferredX;
			aPlaceY = mPreferredY;

			int aSpacing = 4;
			if (aPlaceX < aDesktopRect.left + aSpacing)
				aPlaceX = aDesktopRect.left + aSpacing;
			if (aPlaceY < aDesktopRect.top + aSpacing)
				aPlaceY = aDesktopRect.top + aSpacing;
			if (aPlaceX + aWidth >= aDesktopRect.right - aSpacing)
				aPlaceX = aDesktopRect.right - aWidth - aSpacing;
			if (aPlaceY + aHeight >= aDesktopRect.bottom - aSpacing)
				aPlaceY = aDesktopRect.bottom - aHeight - aSpacing;
		}
		else
		{
			aPlaceX = aDesktopRect.left + ((aDesktopRect.right - aDesktopRect.left) - aWidth) / 2;
			aPlaceY = aDesktopRect.top + (int)(((aDesktopRect.bottom - aDesktopRect.top) - aHeight) * 0.382);
		}
	}
		else
		{
			aWindowStyle = WS_POPUP;
			RECT aMonitorRect;
			if (!GetFullscreenMonitorRect(mHWnd, aMonitorRect))
			{
				return false;
			}
			aPlaceX = aMonitorRect.left;
			aPlaceY = aMonitorRect.top;
			aWidth = aMonitorRect.right - aMonitorRect.left;
			aHeight = aMonitorRect.bottom - aMonitorRect.top;
		}

	if (aWasVisible)
		aWindowStyle |= WS_VISIBLE;

	::SetLastError(ERROR_SUCCESS);
	const LONG_PTR aPreviousStyle = ::SetWindowLongPtr(mHWnd, GWL_STYLE, aWindowStyle);
	if (aPreviousStyle == 0 && ::GetLastError() != ERROR_SUCCESS)
	{
		return false;
	}
	if (!::SetWindowPos(
		mHWnd,
		NULL,
		aPlaceX,
		aPlaceY,
		aWidth,
		aHeight,
		SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE |
		(aWasVisible ? SWP_SHOWWINDOW : 0)))
	{
		return false;
	}
	return true;
#else
	if (!mNativeWindow) return false;
	SetWindowFullscreen(!wantWindowed);
	if (wantWindowed)
		ResizeWindowClient(mWidth * std::max(1, mWindowScale), mHeight * std::max(1, mWindowScale));
	return true;
#endif
}

void SexyAppBase::SwitchScreenMode(bool wantWindowed)
{
	RequestScreenMode(wantWindowed, mUseCustomScale);
}

void SexyAppBase::SwitchScreenMode()
{
	SwitchScreenMode(mIsWindowed, true);
}

void SexyAppBase::SetAlphaDisabled(bool isDisabled)
{
	if (mAlphaDisabled != isDisabled)
	{
		mAlphaDisabled = isDisabled;
		mDDInterface->mVideoOnlyDraw = mAlphaDisabled;		
		mWidgetManager->mImage = mDDInterface->mScreenImage;
		mWidgetManager->MarkAllDirty();
	}
}



void SexyAppBase::ProcessSafeDeleteList()
{
	MTAutoDisallowRand aDisallowRand;

	WidgetSafeDeleteList::iterator anItr = mSafeDeleteList.begin();
	while (anItr != mSafeDeleteList.end())
	{
		WidgetSafeDeleteInfo* aWidgetSafeDeleteInfo = &(*anItr);
		if (mUpdateAppDepth <= aWidgetSafeDeleteInfo->mUpdateAppDepth &&
			aWidgetSafeDeleteInfo->mWidget->mDeferredDeleteHolds == 0)
		{
			delete aWidgetSafeDeleteInfo->mWidget;
			anItr = mSafeDeleteList.erase(anItr);
		}
		else
			++anItr;
	}	
}

void SexyAppBase::UpdateFTimeAcc()
{
	const auto aCurTime = std::chrono::time_point_cast<std::chrono::nanoseconds>(
		FrameDeadline::Clock::now());
	if (mLastTimeCheck != FrameDeadline::TimePoint{})
	{
		const auto aDeltaTimeNs = static_cast<unsigned long long>(
			std::max<std::int64_t>(0,
				std::chrono::duration_cast<std::chrono::nanoseconds>(
					aCurTime - mLastTimeCheck).count()));
		if (mUpdateFTimeAccNs > UINT64_MAX - aDeltaTimeNs)
			throw std::overflow_error("wall-clock backlog exceeds nanosecond range");
		mUpdateFTimeAccNs += aDeltaTimeNs;
		const double aDeltaTime = double(aDeltaTimeNs) / 1'000'000.0;

		if (mRelaxUpdateBacklogCount > 0)
			mRelaxUpdateBacklogCount = std::max(mRelaxUpdateBacklogCount - aDeltaTime, 0.0);
	}

	mLastTimeCheck = aCurTime;
}

bool SexyAppBase::Process(bool allowSleep)
{
	if (mLoadingFailed)
	{
		WaitForLoadingThread();
		RethrowLoadingThreadError();
		Shutdown();
	}
	
	unsigned long long aFrameIntervalNs;
	double anUpdatesPerUpdateF;

	if (mVSyncUpdates)
	{
		aFrameIntervalNs = static_cast<unsigned long long>(std::max(
			1.0, std::round(1'000'000'000.0 / mSyncRefreshRate / mUpdateMultiplier)));
		anUpdatesPerUpdateF = 1'000'000'000.0 / (mFrameTiming.mIntervalNs * mSyncRefreshRate);
	}
	else
	{
		aFrameIntervalNs = static_cast<unsigned long long>(std::max(
			1.0, std::round(double(mFrameTiming.mIntervalNs) / mUpdateMultiplier)));
		anUpdatesPerUpdateF = 1.0;
	}
	// Do we need to fast forward?
	if (mPlayingDemoBuffer)
	{
		if (mUpdateCount < mFastForwardToUpdateNum || mFastForwardToMarker)
		{
            const auto pendingAdvanceNs = static_cast<unsigned long long>(std::min(
                double(UINT64_MAX), std::round(anUpdatesPerUpdateF * double(gUpdateIntervalNs))));
			if (!mDemoMute && !mFastForwardStep)
			{
				mDemoMute = true;
				Mute(true);
			}

			static DWORD aTick = RuntimeTick();
			while (mUpdateCount < mFastForwardToUpdateNum || mFastForwardToMarker)
			{
				ClearUpdateBacklog();
				const auto aLastUpdateCount = mUpdateCount;
								
				// Actual updating code below
				//////////////////////////////////////////////////////////////////////////
				
				bool hadRealUpdate = DoUpdateFrames();

				if (hadRealUpdate)
				{					
					mPendingUpdatesAccNs = mPendingUpdatesAccNs > UINT64_MAX - pendingAdvanceNs
						? UINT64_MAX : mPendingUpdatesAccNs + pendingAdvanceNs;
					mPendingUpdatesAccNs = mPendingUpdatesAccNs >= gUpdateIntervalNs
						? mPendingUpdatesAccNs - gUpdateIntervalNs : 0;
					ProcessSafeDeleteList();

					// Process any extra updates
					while (mPendingUpdatesAccNs >= gUpdateIntervalNs)
					{						
						// These should just be IDLE commands we have to clear out
						ProcessDemo();												

						bool hasRealUpdate = DoUpdateFrames();
						DBG_ASSERTE(hasRealUpdate);

						if (!hasRealUpdate)
							break;
												
						ProcessSafeDeleteList();
						mPendingUpdatesAccNs -= gUpdateIntervalNs;
					}
										
					DoUpdateFramesF((float) anUpdatesPerUpdateF);					
					ProcessSafeDeleteList();
				}

				//////////////////////////////////////////////////////////////////////////				

				// If the update count doesn't change, its because we are
				//  playing back a demo and need to read more
				if (aLastUpdateCount == mUpdateCount)
					return true;

				DWORD aNewTick = RuntimeTick();
				if (aNewTick - aTick >= 1000 || mFastForwardStep) // let the app draw some
				{
					mFastForwardStep = false;
					aTick = RuntimeTick();
					DrawDirtyStuff();
					return true;
				}
			}
		}

		if (mDemoMute)
		{
			mDemoMute = false;
			mSoundManager->StopAllSounds();
			Unmute(true);
		}
	}

	// Make sure we're not paused
	if ((!mPaused) && (mUpdateMultiplier > 0))
	{
		unsigned int aStartTime = SexyGetTickCount();
		
		// unsigned int aCurTime = aStartTime; // Unused
		int aCumSleepTime = 0;
		
		// Sample actual elapsed time independently of presentation pacing.
		
		UpdateFTimeAcc();

		bool didUpdate = false;
		
		if (mUpdateAppState == UPDATESTATE_PROCESS_1)
		{
			if (!mShutdown)
			{
				bool doUpdate = false;
				
				doUpdate = FrameWaitNs(mUpdateFTimeAccNs, aFrameIntervalNs, mFramePacingPhaseNs) == 0;


				if (doUpdate)
				{
					// Do VSyncBroken test.  This test fails if we're in fullscreen and
					// "don't vsync" has been forced in Advanced settings up Display Properties
					if ((!mPlayingDemoBuffer) && (mUpdateMultiplier == 1.0))
					{
						mVSyncBrokenTestUpdates++;
						if (mVSyncBrokenTestUpdates >= mFrameTiming.mTargetFps)
						{
							// It has to be running 33% fast to be "broken" (25% = 1/0.800)
							if (aStartTime - mVSyncBrokenTestStartTick <= 800)
							{
								// The test has to fail 3 times in a row before we decide that
								//  vsync is broken overall
								mVSyncBrokenCount++;
								if (mVSyncBrokenCount >= 3)
									mVSyncBroken = true;
							}
							else
								mVSyncBrokenCount = 0;

							mVSyncBrokenTestStartTick = aStartTime;
							mVSyncBrokenTestUpdates = 0;
						}
					}
					
					// Wall time controls presentation; unconsumed simulation time
					// survives the batch. Overshoot shortens the next interval.
					const auto advanceNs = SimulationTimeFromWallTime(mUpdateFTimeAccNs, mUpdateMultiplier);
					if (mPendingUpdatesAccNs > UINT64_MAX - advanceNs)
						throw std::overflow_error("simulation backlog exceeds nanosecond range");
					mPendingUpdatesAccNs += advanceNs;
					mFramePacingPhaseNs = (mFramePacingPhaseNs % aFrameIntervalNs +
						mUpdateFTimeAccNs % aFrameIntervalNs) % aFrameIntervalNs;
					mUpdateFTimeAccNs = 0;
					const auto stepNs = TakeSimulationStep(mPendingUpdatesAccNs,
						mFrameTiming.mIntervalNs, mSimulationPhaseNs);
					if (stepNs) ApplySimulationStep(stepNs);
					bool hadRealUpdate = stepNs == 0 || DoUpdateFrames();
					if (hadRealUpdate)
						mUpdateAppState = UPDATESTATE_PROCESS_2;					
					else
					{
						// A refused update must yield to the native message stage.
						// Otherwise screensaver idle mode pins PROCESS_1 forever:
						// neither input nor DrawDirtyStuff's idle recheck can run.
						mUpdateAppState = UPDATESTATE_PROCESS_DONE;
						ClearUpdateBacklog(false);
						if (gScreenSaverActive)
						{
							UpdateScreenSaverInfo(SexyGetTickCount());
							if (allowSleep) RuntimeSleep(10);
						}
						ProcessSafeDeleteList();
						return true;
					}

					mHasPendingDraw = true;
					didUpdate = true;
				}
			}
		}
		else if (mUpdateAppState == UPDATESTATE_PROCESS_2)
		{
			mUpdateAppState = UPDATESTATE_PROCESS_DONE;
			
			ProcessSafeDeleteList();

			// The update consumes the full sampled wall interval. No catch-up substeps.

			//aNumCalls++;
			DoUpdateFramesF((float) anUpdatesPerUpdateF);
			ProcessSafeDeleteList();		
		
			// Always present after a batch: an over-budget high-FPS target
			// must not keep scheduling updates and starve rendering.
			if (mHasPendingDraw)
				DrawDirtyStuff();
			mNonDrawCount = 0;

			didUpdate = true;
		}
		
		if (!didUpdate)
		{
			mUpdateAppState = UPDATESTATE_PROCESS_DONE;

			mNonDrawCount = 0;

			if (mHasPendingDraw)
			{
				DrawDirtyStuff();
			}
			else
			{
				// Let us take into account the time it took to draw dirty stuff
				const auto aTimeToNextFrameNs = FrameWaitNs(
					mUpdateFTimeAccNs, aFrameIntervalNs, mFramePacingPhaseNs);
				if (aTimeToNextFrameNs > 0)
				{
					if (!allowSleep)
						return false;
					const auto aDeadline = mLastTimeCheck +
						std::chrono::nanoseconds(aTimeToNextFrameNs);
					const auto aSleepStart = FrameDeadline::Clock::now();
					std::this_thread::sleep_until(aDeadline);
					aCumSleepTime += static_cast<int>(
						std::chrono::duration_cast<std::chrono::milliseconds>(
							FrameDeadline::Clock::now() - aSleepStart).count());
				}
			}
		}

		if (mYieldMainThread)
		{
			// This is to make sure that the title screen doesn't take up any more than 
			// 1/3 of the processor time

			unsigned int anEndTime = SexyGetTickCount();
			int anElapsedTime = (anEndTime - aStartTime) - aCumSleepTime;
			int aLoadingYieldSleepTime = std::min(250, (anElapsedTime * 2) - aCumSleepTime);

			if (aLoadingYieldSleepTime >= 0)
			{
				if (!allowSleep)
					return false;

				RuntimeSleep(aLoadingYieldSleepTime);
			}
		}
	}

	ProcessSafeDeleteList();	
	return true;
}

/*void SexyAppBase::DoMainLoop()
{
	Dialog* aDialog = NULL;
	if (theModalDialogId != -1)
	{
		aDialog = GetDialog(theModalDialogId);
		DBG_ASSERTE(aDialog != NULL);
		if (aDialog == NULL)
			return;
	}

	while (!mShutdown)
	{		
		MSG msg;
		while ((PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) && (!mShutdown))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		ProcessDemo();		
		ProcessDeferredMessages();

		if ((aDialog != NULL) && (aDialog->mResult != -1))
			return;

		if (!mShutdown)
		{
			//++aCount;
			Process();
		}		
	}
}*/

void SexyAppBase::DoMainLoop()
{
	// A cancelled/failed load must leave the loop before resource cleanup.
	// Start joins the loader; LawnApp then performs its full Shutdown path.
	while (!mShutdown && !mLoadingFailed)
	{
		if (mExitToTop)
			mExitToTop = false;
		UpdateApp();
	}
}

bool SexyAppBase::UpdateAppStep(bool* updated)
{
	RethrowLoadingThreadError();
	if (mUpdateAppDepth == 0) ProcessPendingImageDiagnostics();
	if (updated != NULL)
		*updated = false;

	if (mExitToTop)
		return false;

	if (mUpdateAppState == UPDATESTATE_PROCESS_DONE)
		mUpdateAppState = UPDATESTATE_MESSAGES;

	struct UpdateDepth
	{
		int& depth;
		explicit UpdateDepth(int& value) noexcept : depth(value) { ++depth; }
		~UpdateDepth() { --depth; }
	} updateDepth(mUpdateAppDepth);

	// We update in two stages to avoid doing a Process if our loop termination
	//  condition has already been met by processing windows messages		
	if (mUpdateAppState == UPDATESTATE_MESSAGES)
	{
		PumpWindowEvents();

		{
			ProcessDemo();
		}

		// Drain the native burst in order before advancing the game. Consecutive
		// motion is coalesced; button and key transitions are never dropped.
		{
			while (!mShutdown && !mDeferredMessages.empty())
			{
				ProcessDeferredMessages(true);
			}
		}

		// The 100Hz SetTimer stream is gone; do mouse enter/exit bookkeeping once
		// per frame instead.
		{
			if (!mShutdown)
#ifdef _WIN32
				ProcessTimerMessage(mHWnd);
#else
				SampleNativeMousePosition();
#endif
		}

		// 鼠标移动抽稀：每帧排水点统一执行一次最新坐标的 MouseMove（对齐
		// 原版 30fps 节流的吸收效果；悬停/拖动延迟一帧=10ms@100fps，可接受）

		mUpdateAppState = UPDATESTATE_PROCESS_1;

	}
	else
	{
		// Process changes state by itself
		if (mStepMode)
		{
			if (mStepMode==2)
			{
				RuntimeSleep(mFrameTime);
				mUpdateAppState = UPDATESTATE_PROCESS_DONE; // skip actual update until next step
			}
			else
			{
				mStepMode = 2;
				DoUpdateFrames();
				DoUpdateFramesF(1.0f);
				DrawDirtyStuff();
			}
		}
		else
		{
			const auto anOldUpdateCnt = mUpdateCount;
			Process();		
			if (updated != NULL)
				*updated = mUpdateCount != anOldUpdateCnt;			
		}
	}

	return true;
}

bool SexyAppBase::UpdateApp()
{
	bool updated;
	for (;;)
	{
		if (!UpdateAppStep(&updated))
			return false;
		if (updated)
			return true;
	}
}

int SexyAppBase::InitDDInterface()
{
    PreDDInterfaceInitHook();
    const int result = mDDInterface->Init(mHWnd, mIsWindowed);
    if (result != DDInterface::RESULT_OK) return result;
    mWidgetManager->Resize(mScreenBounds,mDDInterface->mVisibleGameRect);
    DemoSyncRefreshRate();
    PostDDInterfaceInitHook();
    return result;
}

bool SexyAppBase::PrepareScalePipelineObjects()
{
    auto& renderer=*mDDInterface->mRenderInterface->mRenderer;
    if (!mUseCustomScale) {
        if (mModernScalePrepared) {
            renderer.SetScaleMode(false,!mIsWindowed);
            // Empty chains wait for in-flight work and release their intermediates.
            for (bool fullscreen : {false,true})
                renderer.PrepareScale(fullscreen,{},uint32_t(renderer.sceneWidth),uint32_t(renderer.sceneHeight));
            mModernScalePrepared=false;
        }
        return true;
    }
    if (mModernScalePrepared) return true;
    const int windowScale = std::max(1, mScaleWindowConfig.mEffects.empty() ?
        mScaleConfigWindow : mWindowScaleFromPipeline);
    const int windowTargetW = renderer.sceneWidth * windowScale;
    const int windowTargetH = renderer.sceneHeight * windowScale;
    std::vector<GpuScaleStep> steps;
    steps.reserve(std::max(mScaleWindowConfig.mEffects.size(),
        mScaleFullscreenConfig.mEffects.size()));
    for (bool fullscreen : {false,true}) {
        steps.clear();
        const ScalePipelineConfig& config = fullscreen ?
            mScaleFullscreenConfig : mScaleWindowConfig;
        if (config.mDeclared) {
            for (const ScaleEffectConfig& entry : config.mEffects) {
                if (entry.mEffectSlot >= mScaleEffects.size())
                    throw std::runtime_error("Cannot resolve complete scale pipeline");
                steps.push_back({&mScaleEffects[entry.mEffectSlot],
                    {entry.mScalingType, entry.mScaleX, entry.mScaleY}});
            }
        }
        int targetW = windowTargetW, targetH = windowTargetH;
        if (fullscreen) {
#ifdef _WIN32
            const auto mode=TargetVideoMode();
            targetW=int(mode.dmPelsWidth); targetH=int(mode.dmPelsHeight);
#else
			targetW = mNativeWindow->Width;
			targetH = mNativeWindow->Height;
#endif
            mModernScaleTargetW=targetW; mModernScaleTargetH=targetH;
        }
        renderer.PrepareScale(fullscreen,steps,uint32_t(targetW),uint32_t(targetH));
    }
    mModernScalePrepared=true;
    return true;
}





void SexyAppBase::WarmScalePipelinesBeforeShow()
{
    PrepareScalePipelineObjects();
    auto& renderer=*mDDInterface->mRenderInterface->mRenderer;
    if (mUseCustomScale) {
        renderer.WarmScale(false);
        renderer.WarmScale(true);
    }
    renderer.SetScaleMode(IsScalePresentActive(),!mIsWindowed);
    if (mIsWindowed) {
        int width=renderer.sceneWidth,height=renderer.sceneHeight;
        if (IsScalePresentActive()) {
            const auto image=renderer.ScaleOutput(false);
            width=int(image.width); height=int(image.height);
        }
        ResizeWindowClient(width,height);
    }
    UpdateWindowGeometry();
}

void SexyAppBase::PreTerminate()
{
}

void SexyAppBase::Start()
{
	if (mShutdown)
		return;

	if (mAutoStartLoadingThread)
		StartLoadingThread();

    {
        if (!mIsWindowed) {
            SetWindowFullscreen(true);
            UpdateWindowGeometry();
        }
    }
    // Without scale warm-up, presenting may defer until the first scene.
    mDDInterface->mRenderInterface->PresentRetainedFrame();
    {
        mNativeWindowShown=true;
#ifdef _WIN32
        ShowWindow(mHWnd,SW_SHOW);
        SetForegroundWindow(mHWnd);
        SetFocus(mHWnd);
#else
		mNativeWindow->Show();
#endif
    }
	// Hidden smoke windows do not activate or take the user's focus.
	UpdateCursorClip();

#ifdef _WIN32
	timeBeginPeriod(1);
#endif

	DWORD aStartTime = SexyGetTickCount();		

	mRunning = true;
	mLastTime = aStartTime;
	mLastUserInputTick = aStartTime;
	mLastTimerTime = aStartTime;
	// Initialization can perform one framework draw before Start().  Never let
	// that pre-run idle check carry a stale screen-saver state into the game.
	gScreenSaverActive = false;

	DoMainLoop();
	if (mLoadingFailed)
	{
		WaitForLoadingThread();
		RethrowLoadingThreadError();
	}

	mRunning = false;

	WaitForLoadingThread();
	RethrowLoadingThreadError();
	ProcessSafeDeleteList();


#ifdef _WIN32
	timeEndPeriod(1);
#endif

	PreTerminate();

	WriteToRegistry();
}

/*
bool SexyAppBase::CheckSignature(const Buffer& theBuffer, const std::string& theFileName)
{
	(void)theBuffer;(void)theFileName;
	// Add your own signature checking code here
	return false;
}
*/


void SexyAppBase::ShowResourceError(bool doExit)
{
	const auto& error=mResourceManager->mError;
	Popup(StrFormat("Resource loading failed.\nError code: %u\nPlan record: %u\nValue: %u\nByte offset: %zu",
		static_cast<unsigned>(error.reason),error.record,error.value,error.offset));
	if (doExit)
		DoExit(1);
}















void SexyAppBase::DoParseCmdLine()
{
#ifdef _WIN32
	char* aCmdLine = GetCommandLineA();
	char* aCmdLinePtr = aCmdLine;
	if (aCmdLinePtr[0] == '"')
	{
		aCmdLinePtr = strchr(aCmdLinePtr + 1, '"');
		if (aCmdLinePtr != NULL)
			aCmdLinePtr++;
	}
	
	if (aCmdLinePtr != NULL)
	{
		aCmdLinePtr = strchr(aCmdLinePtr, ' ');
		if (aCmdLinePtr != NULL)
			ParseCmdLine(aCmdLinePtr+1);	
	}

#endif
	mCmdLineParsed = true; // Linux main passes argv before Init.
}

void SexyAppBase::ParseCmdLine(const std::string& theCmdLine)
{
	// Command line example:  -play -demofile="game demo.dmo"
	// Results in HandleCmdLineParam("-play", ""); HandleCmdLineParam("-demofile", "game demo.dmo");
	std::string aCurParamName;
	std::string aCurParamValue;

	//int aSpacePos = 0; // Unused
	bool inQuote = false;
	bool onValue = false;

	for (size_t i = 0; i < theCmdLine.length(); i++)
	{
		char c = theCmdLine[i];
		bool atEnd = false;
		
		if (c == '"')
		{
			inQuote = !inQuote;

			if (!inQuote)
				atEnd = true;
		}
		else if ((c == ' ') && (!inQuote))
			atEnd = true;
		else if (c == '=')
			onValue = true;
		else if (onValue)
			aCurParamValue += c;
		else
			aCurParamName += c;
		
		if (i == theCmdLine.length() - 1)
			atEnd = true;
		
		if (atEnd && !aCurParamName.empty())
		{
			HandleCmdLineParam(aCurParamName, aCurParamValue);
			aCurParamName = "";
			aCurParamValue = "";
			onValue = false;
		}	
	}
}

static int GetMaxDemoFileNum(const std::string& theDemoPrefix, int theMaxToKeep, bool doErase)
{
#ifdef _WIN32
	WIN32_FIND_DATAA aData;
	HANDLE aHandle = FindFirstFileA((theDemoPrefix + "*.dmo").c_str(), &aData);
	if (aHandle==INVALID_HANDLE_VALUE)
		return 0;

	typedef std::set<int> IntSet;
	IntSet aSet;

	do {
		int aNum = 0;
		if (sscanf(aData.cFileName,(theDemoPrefix + "%d.dmo").c_str(), &aNum)==1)
			aSet.insert(aNum);

	} while(FindNextFileA(aHandle,&aData));
	FindClose(aHandle);

	IntSet::iterator anItr = aSet.begin();
	if ((int)aSet.size()>theMaxToKeep-1 && doErase)
		DeleteFile(StrFormat((theDemoPrefix + "%d.dmo").c_str(),*anItr).c_str());

	if (aSet.empty())
		return 0;

	anItr = aSet.end();
	--anItr;
	return (*anItr);
#else
	std::set<int> numbers;
	const std::filesystem::path prefix(theDemoPrefix);
	const auto directory = prefix.has_parent_path() ? prefix.parent_path() :
		std::filesystem::path(GetAppDataFolder());
	const auto base = prefix.filename().string();
	for (const auto& entry : std::filesystem::directory_iterator(directory))
	{
		if (!entry.is_regular_file()) continue;
		const auto name = entry.path().filename().string();
		if (!name.starts_with(base) || !name.ends_with(".dmo")) continue;
		int number = 0;
		if (StringToInt(name.substr(base.size(), name.size() - base.size() - 4), &number))
			numbers.insert(number);
	}
	if (doErase && !numbers.empty() && static_cast<int>(numbers.size()) > theMaxToKeep - 1)
		std::filesystem::remove(directory / (base + std::to_string(*numbers.begin()) + ".dmo"));
	return numbers.empty() ? 0 : *numbers.rbegin();
#endif
}

void SexyAppBase::HandleCmdLineParam(const std::string& theParamName, const std::string& theParamValue)
{
	if (theParamName == "-play")
	{
		mPlayingDemoBuffer = true;
		mRecordingDemoBuffer = false;
	}
	else if (theParamName == "-recnum")
	{
		int aNum = atoi(theParamValue.c_str());
		if (aNum<=0)
			aNum=5;

		int aDemoFileNum = GetMaxDemoFileNum(mDemoPrefix, aNum, true) + 1;
		mDemoFileName = SexyStringToString(StrFormat(StringToSexyString(mDemoPrefix + "%d.dmo").c_str(),aDemoFileNum));
		if (mDemoFileName.length() < 2 || (mDemoFileName[1] != ':' && mDemoFileName[2] != '\\'))
		{
			mDemoFileName = GetAppDataFolder() + mDemoFileName;
		}
		mRecordingDemoBuffer = true;
		mPlayingDemoBuffer = false;
		ArmDemoFlushAtExit();
	}
	else if (theParamName == "-playnum")
	{
		int aNum = atoi(theParamValue.c_str())-1;
		if (aNum<0)
			aNum=0;

		int aDemoFileNum = GetMaxDemoFileNum(mDemoPrefix, aNum, false)-aNum;
		mDemoFileName = SexyStringToString(StrFormat(StringToSexyString(mDemoPrefix + "%d.dmo").c_str(),aDemoFileNum));
		mRecordingDemoBuffer = false;
		mPlayingDemoBuffer = true;
	}
	else if (theParamName == "-record")
	{
		mRecordingDemoBuffer = true;
		mPlayingDemoBuffer = false;
		ArmDemoFlushAtExit();  // ~SexyAppBase 不保证执行（app 对象可泄漏/快速退出），atexit 已验证可靠
	}
	else if (theParamName == "-demofile")
	{
		mDemoFileName = theParamValue;
		if (mDemoFileName.length() < 2 || (mDemoFileName[1] != ':' && mDemoFileName[2] != '\\'))
		{
			mDemoFileName = GetAppDataFolder() + mDemoFileName;
		}
	}	
	else if (theParamName == "-crash")
	{
		// Try to access NULL
		char* a = 0;
		*a = '!';		
	}
	else if (theParamName == "-nosound")
	{
		mNoSoundNeeded = true;
	}
	else if (theParamName == "-screensaver")
	{
		mIsScreenSaver = true;
	}
	else if (theParamName == "-changedir")
	{
		mChangeDirTo = theParamValue;
	}
	else
	{
		Popup((("Invalid command line parameter: ")) + StringToSexyString(theParamName));
		DoExit(0);
	}
}

void SexyAppBase::PreDisplayHook()
{
}

_Font* SexyAppBase::GetDebugFont()
{
	throw FontNotConfigured{};
}

void SexyAppBase::PreDDInterfaceInitHook()
{
}

void SexyAppBase::PostDDInterfaceInitHook()
{
}

bool SexyAppBase::ChangeDirHook(const char *theIntendedPath)
{
	(void)theIntendedPath;
	return false;
}

MusicInterface* SexyAppBase::CreateMusicInterface()
{
	if (mNoSoundNeeded)
		return new DummyMusicInterface();
	return new BassMusicInterface(mInvisHWnd);
}

void SexyAppBase::InitPropertiesHook()
{
}

void SexyAppBase::InitHook()
{
}

void SexyAppBase::Init()
{
	mPrimaryThreadId = CurrentNativeThreadIdentity();
	
	if (mShutdown)
		return;

#ifdef _WIN32
	if (gDSoundDLL==NULL)
	{
		MessageBox(NULL, 
						(("This application requires a compatible DirectX runtime.")),
						(("You need DirectX")), 
						MB_OK | MB_ICONERROR);
		DoExit(0);
	}
#endif

	InitPropertiesHook();
	// Settings (settings.json, incl. customScale) are read before the immutable
	// scale configuration. Both pipelines compile regardless of checkbox state.
	ReadFromRegistry();
	gUpdateTicks = static_cast<double>(mFrameTiming.mIntervalNs) / FrameTiming::LegacyTickNs;
	gStepsPerLegacyTick = 1.0 / gUpdateTicks;
	gUpdateIntervalNs = mFrameTiming.mIntervalNs;
	gLegacyTickPhaseNs = 0;
	LoadScaleConfig();

	// The two XML pipelines are process-global immutable configuration. Read and
	// inline-compile them exactly once before any main HWND exists, even when the
	// direct (Custom Scale off) presentation path is initially selected. Their
	// Vulkan shader objects are materialized after the device is created and then
	// remain resident for that device's lifetime.
	CompileScaleEffects();
	UpdateWindowScaleFromPipeline();
	mWindowScale = mUseCustomScale ? GetWindowScaleForCustomScale() : 1;

#ifdef _WIN32
	if (CheckForVista())
	{
		HMODULE aMod;
		SHGetFolderPathFunc aFunc = (SHGetFolderPathFunc)GetSHGetFolderPath("shell32.dll", &aMod);
		if (aFunc == NULL || aMod == NULL)
			// SHGetFolderPathFunc is shadowed!
			// SHGetFolderPathFunc aFunc = (SHGetFolderPathFunc)GetSHGetFolderPath("shfolder.dll", &aMod);
			aFunc = (SHGetFolderPathFunc)GetSHGetFolderPath("shfolder.dll", &aMod);

		if (aMod != NULL)
		{
			char aPath[MAX_PATH];
			aFunc(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, aPath);

			std::string aDataPath = RemoveTrailingSlash(aPath) + "\\" + mFullCompanyName + "\\" + mProdName;
			SetAppDataFolder(aDataPath + "\\");
			//MkDir(aDataPath);
			//AllowAllAccess(aDataPath);
			if (mDemoFileName.length() < 2 || (mDemoFileName[1] != ':' && mDemoFileName[2] != '\\'))
			{
				mDemoFileName = GetAppDataFolder() + mDemoFileName;
			}

			FreeLibrary(aMod);
		}
	}

	// Test harnesses can override the local profile root explicitly.
	const char* anAppDataOverride = std::getenv("PVZ_APPDATA_DIR");
	if (anAppDataOverride != NULL && anAppDataOverride[0] != '\0')
		SetAppDataFolder(anAppDataOverride);
	else
	{
		// Keep the remake self-contained and never mix its native GOTY saves with
		// files owned by an installed 32-bit release.
		SetAppDataFolder(AddTrailingSlash(mChangeDirTo, true) + "data\\");
	}
#else
	(void)GetAppDataFolder(); // Resolved before settings/logs, stable across chdir.
#endif
	
	if (!mCmdLineParsed)
		DoParseCmdLine();
	if ((mPlayingDemoBuffer || mRecordingDemoBuffer) && mFrameTiming.mTargetFps != 100)
		throw std::runtime_error("Legacy demo recording/playback requires targetFps=100");

	if (mIsScreenSaver)
		mOnlyAllowOneCopyToRun = false;	


#ifdef _WIN32
	if(gHInstance==NULL)
		gHInstance = (HINSTANCE)GetModuleHandle(NULL);
#endif

	// Change directory
	if (!ChangeDirHook(mChangeDirTo.c_str()))
	{
#ifdef _WIN32
		chdir(mChangeDirTo.c_str());
#else
		if (chdir(mChangeDirTo.c_str()) != 0)
			throw std::runtime_error("Cannot select Linux resource directory: " + mChangeDirTo);
#endif
	}

	// The x64 build has exactly one packaged-resource format: the converter's
	// plain main.pak64.  Never mount the official 32-bit/XOR main.pak here.
	if (!gPakInterface->AddPakFile("main.pak64"))
	{
		throw std::runtime_error("missing or invalid plain x64 resource archive: main.pak64");
	}

	// Create a message we can use to talk to ourselves inter-process
#ifdef _WIN32
	mNotifyGameMessage = RegisterWindowMessageA("pvz_perf.Notify");

	std::string instanceName="pvz_perf.SingleInstance";
	::SetLastError(ERROR_SUCCESS);
	mMutex = ::CreateMutexA(NULL, TRUE, instanceName.c_str());
	const DWORD aMutexError = ::GetLastError();
	if (mMutex == NULL)
	{
		throw std::runtime_error("Unable to create pvz_perf instance mutex: "+std::to_string(aMutexError));
	}
	if (aMutexError == ERROR_ALREADY_EXISTS)
	{
		HandleGameAlreadyRunning();
		return;
	}
#else
	// flock lifetime follows the descriptor; a crash releases it automatically.
	if (mOnlyAllowOneCopyToRun)
	{
#if defined(__ANDROID__)
		// Shared Android/media supports data files but not reliable flock.
		// Runtime synchronization belongs to the private native runtime folder.
		const auto path = GetExecutableDirectory() + ".instance.lock";
#else
		const auto path = GetAppDataFolder() + ".instance.lock";
#endif
		mInstanceLockFd = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
		if (mInstanceLockFd < 0)
			throw std::runtime_error("Cannot open Linux instance lock: " + path);
		if (flock(mInstanceLockFd, LOCK_EX | LOCK_NB) != 0)
		{
			if (errno != EWOULDBLOCK && errno != EAGAIN)
				throw std::runtime_error("Cannot lock instance file: " + path + " (errno " + std::to_string(errno) + ")");
			HandleGameAlreadyRunning();
			return;
		}
	}
#endif

	mRandSeed = RuntimeTick();
	SRand(mRandSeed);	

	// Set up demo recording stuff
	if (mPlayingDemoBuffer)
	{
		std::string anError;
		if (!ReadDemoBuffer(anError))
		{
			mPlayingDemoBuffer = false;
			Popup(anError);
			DoExit(0);
		}
	}

	srand(RuntimeTick());

#ifdef _WIN32
	RegisterWindowClasses();
#endif
		
	// Let app do something before showing window, or switching to fullscreen mode
	// NOTE: Moved call to PreDisplayHook above mIsWindowed and GetSystemsMetrics
	// checks because the checks below use values that could change in PreDisplayHook.
	// PreDisplayHook must call mWidgetManager->Resize if it changes mWidth or mHeight.
	PreDisplayHook();

	mWidgetManager->Resize(Rect(0, 0, mWidth, mHeight), Rect(0, 0, mWidth, mHeight));

	// Keep the requested startup mode even when the desktop work area is smaller
	// than the original 800x600 client area.  A clipped window is recoverable;
	// silently entering exclusive fullscreen changes the user's display mode.

	MakeWindow();
	DetectTargetFrameRate();
	// Device/window remain hidden while BOTH native chains are materialized and
	// warmed. This happens even when the checkbox is initially off.
	WarmScalePipelinesBeforeShow();
		if (mPlayingDemoBuffer)
	{
		// Get video data
		
		RequireDemoCommand(DEMO_VIDEO_DATA);

		mIsWindowed = mDemoBuffer.ReadBoolean();
		mSyncRefreshRate = mDemoBuffer.ReadByte();
	}

	if (mSoundManager == NULL)		
		mSoundManager = new NativeSoundManager(mNoSoundNeeded?NULL:mHWnd);

	SetSfxVolume(mSfxVolume);
	
	mMusicInterface = CreateMusicInterface();	

	SetMusicVolume(mMusicVolume);	

	if (mIsScreenSaver)
	{
		SetCursor(CURSOR_NONE);
	}

	InitHook();

	mInitialized = true;
}

void SexyAppBase::HandleGameAlreadyRunning()
{
	if(mOnlyAllowOneCopyToRun)
	{
		// Notify the other window and then shut ourselves down
#ifdef _WIN32
		if (mNotifyGameMessage != 0)
			PostMessage(HWND_BROADCAST, mNotifyGameMessage, 0, 0);
		DoExit(0);
#else
#if defined(__ANDROID__)
		throw std::runtime_error("Another game process holds " + GetExecutableDirectory() + ".instance.lock");
#else
		throw std::runtime_error("Another game process holds " + GetAppDataFolder() +
			".instance.lock; close that game before starting another window backend");
#endif
#endif
	}
}

void SexyAppBase::CopyToClipboard(const std::string& theString)
{
#ifdef _WIN32
	if (mPlayingDemoBuffer)
		return;

	HGLOBAL				aGlobalHandle;
	char*				theData;	
	WCHAR*				theWData;

	if (OpenClipboard(mHWnd))
	{
		EmptyClipboard();

		aGlobalHandle = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, theString.length()+1);
		theData = (char*) GlobalLock(aGlobalHandle);
		strcpy(theData, theString.c_str());
		GlobalUnlock(aGlobalHandle);
		SetClipboardData(CF_TEXT, aGlobalHandle);
		SetClipboardData(CF_OEMTEXT, aGlobalHandle);		
		SetClipboardData(CF_LOCALE, aGlobalHandle);

		int aSize = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, theString.c_str(), theString.length(), NULL, 0);
		aGlobalHandle = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, (aSize + 1) * sizeof(WCHAR));
		theWData = (WCHAR*) GlobalLock(aGlobalHandle);
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, theString.c_str(), theString.length(), theWData, aSize);
		theWData[aSize] = '\0';
		GlobalUnlock(aGlobalHandle);
		SetClipboardData(CF_UNICODETEXT, aGlobalHandle);
		
		CloseClipboard();
	}
#else
	if (!mPlayingDemoBuffer)
	{
#ifdef __APPLE__
        MacClipboard(&theString);
#elif defined(__linux__) && !defined(__ANDROID__)
        TransferClipboard(&theString);
#else
        ;  // Android：系统剪贴板接入暂缺，空操作
#endif
	}
#endif
}

std::string	SexyAppBase::GetClipboard()
{
#ifdef _WIN32
	HGLOBAL				aGlobalHandle;	
	std::string			aString;

	if (!mPlayingDemoBuffer)
	{
		if (OpenClipboard(mHWnd))
		{
			aGlobalHandle = GetClipboardData(CF_TEXT);	
			if (aGlobalHandle != NULL)
			{
				char* theData = (char*) GlobalLock(aGlobalHandle);
				if (theData != NULL)
				{
					aString = theData;		
					GlobalUnlock(aGlobalHandle);
				}
			}
			
			CloseClipboard();
		}
	}

	DemoSyncString(&aString);

	return aString;
#else
	std::string value;
	if (!mPlayingDemoBuffer)
	{
#ifdef __APPLE__
        value = MacClipboard(nullptr);
#elif defined(__linux__) && !defined(__ANDROID__)
        value = TransferClipboard(nullptr);
#else
        ;  // Android：系统剪贴板接入暂缺，空操作
#endif
	}
	DemoSyncString(&value);
	return value;
#endif
}

void SexyAppBase::SetCursor(int theCursorNum)
{
	mCursorNum = theCursorNum;
	EnforceCursor();
}

void SexyAppBase::EnableCustomCursors(bool enabled)
{
	mCustomCursorsEnabled = enabled;
	EnforceCursor();
}

Sexy::Image* SexyAppBase::GetImage(const std::string& theFileName, bool commitBits, bool lookForAlphaImage)
{
	ImageLib::Image* loaded = ImageLib::GetImage(theFileName, lookForAlphaImage);
	if (!loaded)
		return nullptr;
	Image* image = nullptr;
	try {
		image = new Image(this);
		image->AdoptDecodedImage(std::exchange(loaded, nullptr), commitBits);
		return image;
	} catch (...) {
		delete image;
		delete loaded;
		throw;
	}
}

Image* SexyAppBase::CreateColorizedImage(Image* theImage, const Color& theColor)
{
	auto* source=theImage;
	if(!source) throw GpuImageCpuAccess{};
	const GpuImageProcessing processing{0,0,source->mWidth,source->mHeight,
		theColor.mRed,theColor.mGreen,theColor.mBlue,theColor.mAlpha,GpuImageOperation::Colorize};
	return mDDInterface->mRenderInterface->ProcessImage(source,processing);
}

Image* SexyAppBase::CopyImage(Image* theImage, const Rect& theRect)
{
	auto* source=theImage;
	if(!source) throw GpuImageCpuAccess{};
	const GpuImageProcessing processing{theRect.mX,theRect.mY,theRect.mWidth,theRect.mHeight,
		0,0,0,0,GpuImageOperation::Copy};
	return mDDInterface->mRenderInterface->ProcessImage(source,processing);
}

Image* SexyAppBase::CopyImage(Image* theImage)
{
	return CopyImage(theImage, Rect(0, 0, theImage->mWidth, theImage->mHeight));
}

void SexyAppBase::PlaySample(int theSoundNum)
{
	if (!mSoundManager)
		return;

	SoundInstance* aSoundInstance = mSoundManager->GetSoundInstance(theSoundNum);
	if (aSoundInstance != NULL)
	{
		aSoundInstance->Play(false, true);
	}
}


void SexyAppBase::PlaySample(int theSoundNum, int thePan)
{
	if (!mSoundManager)
		return;

	SoundInstance* aSoundInstance = mSoundManager->GetSoundInstance(theSoundNum);
	if (aSoundInstance != NULL)
	{
		aSoundInstance->SetPan(thePan);
		aSoundInstance->Play(false, true);
	}
}

bool SexyAppBase::IsMuted()
{
	return mMuteCount > 0;
}

void SexyAppBase::Mute(bool autoMute)
{	
	mMuteCount++;
	if (autoMute)
		mAutoMuteCount++;

	SetMusicVolume(mMusicVolume);
	SetSfxVolume(mSfxVolume);
}

void SexyAppBase::Unmute(bool autoMute)
{	
	if (mMuteCount > 0)
	{
		mMuteCount--;
		if (autoMute)
			mAutoMuteCount--;
	}

	SetMusicVolume(mMusicVolume);
	SetSfxVolume(mSfxVolume);
}


void SexyAppBase::SetMusicVolume(float theVolume)
{
	mMusicVolume = theVolume;

	if (mMusicInterface != NULL)
		mMusicInterface->SetVolume((mMuteCount > 0) ? 0.0f : mMusicVolume);
}

void SexyAppBase::SetSfxVolume(double theVolume)
{
	mSfxVolume = theVolume;

	if (mSoundManager != NULL)
		mSoundManager->SetVolume((mMuteCount > 0) ? 0.0 : mSfxVolume);
}

void SexyAppBase::AddGpuImage(Image* image)
{
	AutoCrit anAutoCrit(mDDInterface->mCritSect);
	mGpuImageSet.insert(image);
}

void SexyAppBase::RemoveGpuImage(Image* image)
{
	AutoCrit anAutoCrit(mDDInterface->mCritSect);
	GpuImageSet::iterator anItr = mGpuImageSet.find(image);
	if (anItr != mGpuImageSet.end())
		mGpuImageSet.erase(anItr);

	Remove3DData(image);
}

void SexyAppBase::Remove3DData(Image* theMemoryImage)
{
	if (mDDInterface)
		mDDInterface->Remove3DData(theMemoryImage);
}




void SexyAppBase::DemoSyncRefreshRate()
{
	mSyncRefreshRate = mDDInterface->mRefreshRate;

	if (mRecordingDemoBuffer)
	{
		WriteDemoTimingBlock();
		mDemoBuffer.WriteNumBits(0, 1);
		mDemoBuffer.WriteNumBits(DEMO_VIDEO_DATA, 5);
		mDemoBuffer.WriteBoolean(mIsWindowed);
		uchar aByte = (uchar) mSyncRefreshRate;
		mDemoBuffer.WriteByte(aByte);		
	}
}


// Keep compiled scale programs available, but allocate/warm their image chains
// only while custom scaling is enabled. Disabling releases both mode chains.
void SexyAppBase::ReconfigureScale(bool theUseCustomScale)
{
    if (mUseCustomScale==theUseCustomScale) return;
    if (mSwitchingScreenMode) throw std::logic_error("Nested scale/mode transaction");
    mSwitchingScreenMode=true;
    struct Reset { bool& flag; ~Reset(){flag=false;} } reset{mSwitchingScreenMode};
    ReleaseCursorClip();
    mWidgetManager->CancelMouseDownState(); ClearKeysDown();
    auto& render=*mDDInterface->mRenderInterface;
    render.Flush();
    mUseCustomScale=theUseCustomScale;
    mWindowScale=mUseCustomScale?GetWindowScaleForCustomScale():1;
    PrepareScalePipelineObjects();
    if (mUseCustomScale) {
        render.mRenderer->WarmScale(false);
        render.mRenderer->WarmScale(true);
    }
    render.mRenderer->SetScaleMode(IsScalePresentActive(),!mIsWindowed);
    // Change geometry after enabled pipelines are ready, then present the canvas.
    if (mIsWindowed) {
        int width=render.mRenderer->sceneWidth,height=render.mRenderer->sceneHeight;
        if (IsScalePresentActive()) {
            const auto image=render.mRenderer->ScaleOutput(false);
            width=int(image.width);height=int(image.height);
        }
        ResizeWindowClient(width,height);
    }
    UpdateWindowGeometry();
    render.PresentRetainedFrame();
    ClearUpdateBacklog(false);
}

static std::string LowerScaleText(const std::string& theText)
{
	std::string aResult = theText;
	for (size_t i = 0; i < aResult.size(); i++)
		aResult[i] = (char)tolower((unsigned char)aResult[i]);
	return aResult;
}

[[maybe_unused]] static std::string NormalizeScaleEffectPath(std::string thePath)
{
	if (thePath.empty())
		return thePath;
	for (char& aChar : thePath)
		if (aChar == '\\')
			aChar = '/';
	if (LowerScaleText(thePath).compare(0, 8, "effects/") == 0)
		thePath.erase(0, 8);
	const std::string aLowerPath = LowerScaleText(thePath);
	if (aLowerPath.size() <= 5 || aLowerPath.compare(aLowerPath.size() - 5, 5, ".hlsl") != 0)
		thePath += ".hlsl";
	return thePath;
}

[[maybe_unused]] static bool ParseScaleParamValue(const std::string& text, float& out)
{
    const char* begin=text.c_str(); char* end=nullptr; errno=0;
    const float value=std::strtof(begin,&end);
    while(end && *end && isspace(static_cast<unsigned char>(*end))) ++end;
    if(end==begin || !end || *end || errno==ERANGE || !std::isfinite(value)) return false;
    out=value; return true;
}

// Read scale.json (same directory as the executable / working directory).
// Schema（键区分大小写；沿袭 XML 时代的 pvzScale 管线格式，旧 <Scale> 简式
// 的 multiplier/output 能力并入同名键）:
//
//  {
//    "HDR": { "enable": true, "strength": 1.0 },
//    "window": {
//      "multiplier": 2,                       // 可选：无管线时的窗口倍率
//      "effects": [ { "path": "Anime4K/Anime4K_Upscale_L.hlsl" } ]
//    },
//    "fullScreen": {
//      "output": { "width": 0, "height": 0 }, // 可选：全屏输出尺寸
//      "effects": [
//        { "path": "SSimDownscaler.hlsl", "params": { "overSharp": "1.5" } }
//      ]
//    }
//  }
//
// effect 条目：path 相对 effects/ 目录；params 为参数名→值（值取数字或
// 字符串，读出按序列化文本处理）；mode/scaleX/scaleY 写在 params 内按
// XML 子元素同语义提升为字段。文件缺失 → 全默认；存在但 JSON 损坏 →
// 抛错（与 HDR strength 校验同级，坏配置不静默吞）。
void SexyAppBase::LoadScaleConfig()
{
    if(mModernScalePrepared) throw std::logic_error("Scale configuration may only load before renderer preparation");
    mScaleEffects.clear();
	mScaleConfigWindow = 1;
	mWindowScaleFromPipeline = 1;
	mScaleConfigFullscreenWidth = 0;
	mScaleConfigFullscreenHeight = 0;
	mScaleHDREnabled = false;
	mScaleHDRPaperWhite = 80.0f;          // 探测失败 fallback：SDR 参考白
	mScaleHDRMaxNits = 1000.0f;           // 探测失败 fallback：微软 MaxTVNits 模板/默认同为 1000
	mScaleHDRReinhardPaperNits = 0.0f;    // 由 SDR 白按 dxgi 公式运行时计算
	mScaleHDRStrength = 1.0f;             // 唯一可调（AutoHDRStrength 同义）
	mScaleWindowConfig.mEffects.clear();
	mScaleWindowConfig.mOutputWidth = 0;
	mScaleWindowConfig.mOutputHeight = 0;
	mScaleWindowConfig.mDeclared = false;
	mScaleFullscreenConfig.mEffects.clear();
	mScaleFullscreenConfig.mOutputWidth = 0;
	mScaleFullscreenConfig.mOutputHeight = 0;
	mScaleFullscreenConfig.mDeclared = false;

	JsonDoc aDoc;
	const char* scaleFile=getenv("PVZ_SCALE_CONFIG_FILE");
	if (!aDoc.LoadFile(scaleFile?scaleFile:"scale.json"))
	{
		if (aDoc.mStatus == JsonDoc::LoadStatus::ParseError)
			throw std::runtime_error("scale.json: " + aDoc.mError);
		return; // no scale.json -> use defaults
	}

	// Decode protocol fields once; only numeric IDs/values survive in app state.
#if defined(PVZ_GPU_OPENGL) || defined(PVZ_GPU_D3D9) || (defined(__ANDROID__) && !defined(SEXY_SCALE_EFFECT_AUTHORING))
    auto LoadPipelineEffects = [&aDoc](const char* section, ScalePipelineConfig&)
    {
        if(aDoc.GetArraySize(std::string(section)+"/effects") != 0)
            throw std::runtime_error(std::string("This graphics backend cannot execute HLSL/compute scale effects in ")+
                section+"; use an empty effects array for built-in presentation scaling");
    };
#else
    ScaleEffectCompiler sourceCompiler;
	auto LoadPipelineEffects = [this,&aDoc,&sourceCompiler](const char* theSection, ScalePipelineConfig& thePipeline)
	{
		const size_t aCount = aDoc.GetArraySize(std::string(theSection) + "/effects");
		for (size_t i = 0; i < aCount; i++)
		{
			const std::string aBase = std::string(theSection) + "/effects/" + std::to_string(i);
            ScaleEffectConfig config;
            config.mPath=NormalizeScaleEffectPath(aDoc.GetString(aBase+"/path",""));
            if(config.mPath.empty()) throw std::runtime_error("scale.json effect path is empty");
            const std::string sourcePath="effects/"+config.mPath;
            ScaleEffectRuntimeDesc metadata;
            std::string error;
#if defined(PVZ_GPU_D3D12)
            if(!sourceCompiler.CompileD3d12(sourcePath,{},metadata,error))
#else
            if(!sourceCompiler.CompileFile(sourcePath,{},metadata,error))
#endif
                throw std::runtime_error(sourcePath+": "+error);
            metadata.mSourcePath=sourcePath;
            ScaleEffectParams sourceOverrides;
			std::vector<std::string> aKeys;
			aDoc.GetKeys(aBase + "/params", aKeys);
            std::vector<std::optional<float>> decoded(metadata.mParams.size());
			for (const std::string& aKey : aKeys)
			{
				const std::string aLowerKey = LowerScaleText(aKey);
				const std::string aValue = aDoc.GetString(aBase + "/params/" + aKey, "");
                if(aLowerKey=="mode") {
                    const auto mode=LowerScaleText(aValue);
                    if(mode.empty() || mode=="normal") config.mScalingType=SCALE_SCALING_NORMAL;
                    else if(mode=="fit") config.mScalingType=SCALE_SCALING_FIT;
                    else if(mode=="absolute") config.mScalingType=SCALE_SCALING_ABSOLUTE;
                    else if(mode=="fill") config.mScalingType=SCALE_SCALING_FILL;
                    else throw std::runtime_error("scale.json has an unknown scaling mode");
                } else if(aLowerKey=="scalex") config.mScaleX=float(atof(aValue.c_str()));
                else if(aLowerKey=="scaley") config.mScaleY=float(atof(aValue.c_str()));
                else {
                    float value=0; ScaleParamValue numeric;
                    if(!ParseScaleParamValue(aValue,value) || !ScaleEffectBinary::DecodeParam(metadata,aKey,value,numeric,error))
                        throw std::runtime_error("scale.json has an invalid effect parameter: "+error);
                    // Preserve the old JSON boundary's case-insensitive overwrite.
                    decoded[ScaleSlot(numeric.mId)]=numeric.mValue;
                    sourceOverrides[aKey]=numeric.mValue;
                }
			}
            ScaleRuntimeParams overrides;
            for(size_t slot=0;slot<decoded.size();++slot)
                if(decoded[slot]) overrides.push_back({static_cast<ScaleParamId>(slot),*decoded[slot]});
            if(!ScaleEffectBinary::ResolveParams(metadata,overrides,config.mParams,error))
                throw std::runtime_error("scale.json: "+error);
            if(!std::isfinite(config.mScaleX) || !std::isfinite(config.mScaleY) || config.mScaleX<=0 || config.mScaleY<=0)
                throw std::runtime_error("scale.json effect scaling must be finite and positive");
            for(const auto& value:config.mParams) metadata.mParams[ScaleSlot(value.mId)].mInlineValue=value.mValue;
#if defined(PVZ_GPU_D3D12)
            if(!sourceOverrides.empty() &&
                !sourceCompiler.CompileD3d12(sourcePath,sourceOverrides,metadata,error))
                throw std::runtime_error(sourcePath+": "+error);
#endif
            for(size_t slot=0;slot<mScaleEffects.size();++slot) {
                const auto& existing=mScaleEffects[slot];
                if(LowerScaleText(existing.mSourcePath)!=LowerScaleText(sourcePath) || existing.mParams.size()!=metadata.mParams.size()) continue;
                bool same=true;
                for(size_t p=0;p<existing.mParams.size();++p)
                    if(existing.mParams[p].mInlineValue!=metadata.mParams[p].mInlineValue) { same=false; break; }
                if(same) { config.mEffectSlot=slot; break; }
            }
            if(config.mEffectSlot==SIZE_MAX) {
                config.mEffectSlot=mScaleEffects.size(); mScaleEffects.push_back(std::move(metadata));
            }
            thePipeline.mEffects.push_back(std::move(config));
		}
	};
#endif

	if (aDoc.GetType("window") == JsonDoc::Type::Object)
	{
		mScaleWindowConfig.mDeclared = true;
		const long long aMultiplier = aDoc.GetInt("window/multiplier", 0);
		if (aMultiplier > 0)
			mScaleConfigWindow = (int)aMultiplier;
        LoadPipelineEffects("window", mScaleWindowConfig);
	}

	if (aDoc.GetType("fullScreen") == JsonDoc::Type::Object)
	{
		mScaleFullscreenConfig.mDeclared = true;
		const int aWidth = (int)aDoc.GetInt("fullScreen/output/width", 0);
		const int aHeight = (int)aDoc.GetInt("fullScreen/output/height", 0);
		mScaleFullscreenConfig.mOutputWidth = std::max(0, aWidth);
		mScaleFullscreenConfig.mOutputHeight = std::max(0, aHeight);
		mScaleConfigFullscreenWidth = mScaleFullscreenConfig.mOutputWidth;
		mScaleConfigFullscreenHeight = mScaleFullscreenConfig.mOutputHeight;
        LoadPipelineEffects("fullScreen", mScaleFullscreenConfig);
	}

	if (aDoc.GetType("HDR") == JsonDoc::Type::Object)
	{
		// 顶级 HDR 开关 + 参数容器：出现即启用，enable 可显式关（false/0）。
		mScaleHDREnabled = aDoc.GetBool("HDR/enable", true);
		if (aDoc.Has("HDR/strength"))
		{
			// strength 是 HDR 唯一暴露的数值项，全量校验：类型必须是数字
			// 且有限、落在 [0,1]；非法值直接报错——0 本身合法，不能拿它
			// 掩盖解析失败。
			const double aValue = aDoc.GetFloat("HDR/strength", 0.0);
			if (aDoc.GetType("HDR/strength") != JsonDoc::Type::Number ||
				!std::isfinite(aValue) || aValue < 0.0 || aValue > 1.0)
				throw std::runtime_error("scale.json HDR strength must be a finite number in [0,1]");
			mScaleHDRStrength = (float)aValue;
		}
	}

}


// <HDR> 参数解析：亮度锚全部来自显示器运行时探测（与微软 AutoHDR 同源，
// 查询使用独立于渲染 API 的 Windows AdvancedColorInfo；公式保留 legacy），
// 不暴露、不回写 scale.json；启动读一次，之后接收系统通知而不轮询。strength 是 scale.json
// 唯一暴露项（AutoHDRStrength 同义，默认 1）。
void SexyAppBase::ResolveHDRAutoParams()
{
#if defined(PVZ_GPU_OPENGL) || defined(PVZ_GPU_D3D9)
    mScaleHDREnabled = false;
#elif defined(_WIN32)
    ResolveWindowsHdr(mNativeHdrState, *this, mScaleHDREnabled ? TargetMonitor() : nullptr);
#elif defined(__ANDROID__)
    // v3.3 ships Android in SDR until device HDR calibration is validated.
    mScaleHDREnabled = false;
#else
	if (mScaleHDREnabled) throw std::runtime_error("Linux HDR output is not implemented");
#endif
}

void SexyAppBase::CompileScaleEffects()
{
    // Source was compiled while resolving startup configuration, before GPU preparation.
    if(mModernScalePrepared) throw std::logic_error("Scale shaders may only load before device preparation");
#if defined(PVZ_GPU_OPENGL) || defined(PVZ_GPU_D3D9) || (defined(__ANDROID__) && !defined(SEXY_SCALE_EFFECT_AUTHORING))
    if(!mScaleEffects.empty())
        throw std::runtime_error("This graphics backend does not support HLSL/compute scale effects");
#else
    for(const auto& effect:mScaleEffects) {
        std::string error;
#if defined(PVZ_GPU_D3D12)
        if(!ScaleEffectCompiler::ValidateD3d12(effect,error)) throw std::runtime_error(error);
#else
        if(!ScaleEffectBinary::Validate(effect,error)) throw std::runtime_error(error);
#endif
    }
#endif
}

	bool SexyAppBase::IsScalePresentActive() const
	{
		return IsScalePresentActiveForMode(mIsWindowed);
	}

	bool SexyAppBase::IsScalePresentActiveForMode(bool theWindowed) const
	{
		if (!mUseCustomScale)
			return false;
		const ScalePipelineConfig& aConfig =
			theWindowed ? mScaleWindowConfig : mScaleFullscreenConfig;
		// An empty mode section is an intentional direct-present path. Do not
		// install a callback merely because the XML container or an output marker
		// exists: without effects there is no custom shader stage to execute.
		return aConfig.mDeclared && !aConfig.mEffects.empty();
	}


bool SexyAppBase::BuildScalePipelineForMode(bool theWindowed,
    std::vector<ScaleEffectRuntimeDesc*>& outEffects, std::vector<ScaleEffectScaling>& outScalings)
{
    outEffects.clear(); outScalings.clear();
    const auto& config=theWindowed?mScaleWindowConfig:mScaleFullscreenConfig;
    if(!config.mDeclared) return false;
    for(const auto& entry:config.mEffects) {
        if(entry.mEffectSlot>=mScaleEffects.size()) return false;
        outEffects.push_back(&mScaleEffects[entry.mEffectSlot]);
        outScalings.push_back({entry.mScalingType,entry.mScaleX,entry.mScaleY});
    }
    return true;
}

	bool SexyAppBase::ComputeCustomScaleOutputSize(
		bool theWindowed, int theSrcW, int theSrcH, int& outW, int& outH)
	{
		std::vector<ScaleEffectRuntimeDesc*> aEffects;
		std::vector<ScaleEffectScaling> aScalings;
		if (!BuildScalePipelineForMode(theWindowed, aEffects, aScalings))
			return false;

		int aTgtW = theSrcW;
		int aTgtH = theSrcH;
		if (theWindowed)
		{
			aTgtW = theSrcW * GetWindowScaleForCustomScale();
			aTgtH = theSrcH * GetWindowScaleForCustomScale();
		}
		else
		{
#ifdef _WIN32
			RECT aMonitorRect;
			if (!GetFullscreenMonitorRect(mHWnd, aMonitorRect))
				return false;
			aTgtW = aMonitorRect.right - aMonitorRect.left;
			aTgtH = aMonitorRect.bottom - aMonitorRect.top;
#else
			if (!mNativeWindow) return false;
			aTgtW = mNativeWindow->Width;
			aTgtH = mNativeWindow->Height;
#endif
		}
		if (aTgtW <= 0 || aTgtH <= 0)
			return false;
		return ScaleEffectSizing::ComputePipelineOutputSize(
			aEffects, aScalings, theSrcW, theSrcH, aTgtW, aTgtH, outW, outH);
	}

	bool SexyAppBase::GetCustomScalePresentSize(int& outW, int& outH)
	{
		return GetCustomScalePresentSizeForMode(mIsWindowed, outW, outH);
	}

	bool SexyAppBase::GetCustomScalePresentSizeForMode(
		bool theWindowed, int& outW, int& outH)
	{
		if (!IsScalePresentActiveForMode(theWindowed))
			return false;
		return ComputeCustomScaleOutputSize(theWindowed, mWidth, mHeight, outW, outH);
	}

	int SexyAppBase::GetWindowScaleForCustomScale() const
	{
		if (!mScaleWindowConfig.mEffects.empty())
			return std::max(1, mWindowScaleFromPipeline);
		return std::max(1, mScaleConfigWindow);
	}

	void SexyAppBase::UpdateWindowScaleFromPipeline()
	{
		mWindowScaleFromPipeline = 1;
		std::vector<ScaleEffectRuntimeDesc*> aEffects;
		std::vector<ScaleEffectScaling> aScalings;
		if (!BuildScalePipelineForMode(true, aEffects, aScalings))
			return;

		int aW = 0;
		int aH = 0;
		if (!ScaleEffectSizing::ComputePipelineOutputSize(
			aEffects, aScalings, 800, 600, 800, 600, aW, aH))
			return;
		if (aW >= 800 && aW % 800 == 0 && aH >= 600 && aH % 600 == 0)
			mWindowScaleFromPipeline = std::max(1, aW / 800);
	}

void SexyAppBase::CleanupScalePresent()
{
}

SharedImageRef SexyAppBase::LoadSharedImage(const std::string& file, unsigned variant,
	bool* isNew, const std::function<Image*()>& load)
{
	const SharedImageMap::key_type key(StringToUpper(file), variant);
	SharedImageRef result;
	std::optional<std::promise<void>> completion;
	std::shared_future<void> ready;
	std::thread::id loaderThread;
	bool inserted;
	{
		AutoCrit lock(mDDInterface->mCritSect);
		auto [entry, fresh] = mSharedImageMap.try_emplace(key);
		inserted = fresh;
		if (fresh)
		{
			try
			{
				completion.emplace();
				entry->second.mLoadReady = completion->get_future().share();
				entry->second.mLoaderThread = std::this_thread::get_id();
			}
			catch (...)
			{
				mSharedImageMap.erase(entry);
				throw;
			}
		}
		result = &entry->second;
		ready = entry->second.mLoadReady;
		loaderThread = entry->second.mLoaderThread;
	}
	if (isNew != nullptr)
		*isNew = inserted;
	if (!inserted)
	{
		if (ready.valid())
		{
			if (loaderThread == std::this_thread::get_id() &&
				ready.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
				throw std::logic_error("Recursive loading of shared image: " + file);
			// Never hold the registry/render lock while waiting for this key.
			ready.get();
		}
		result.mImage=result.mSharedImage->mImage;
		return result;
	}
	Image* image = nullptr;
	try
	{
		// Decode and prepare privately. Other keys and image registration can
		// proceed; waiters keep this map node alive through counted references.
		image = load();
		{
			AutoCrit lock(mDDInterface->mCritSect);
			result.mSharedImage->mImage = std::exchange(image, nullptr);
			result.mImage=result.mSharedImage->mImage;
			result.mSharedImage->mLoadReady = {};
			result.mSharedImage->mLoaderThread = {};
		}
		completion->set_value();
	}
	catch (...)
	{
		delete image;
		// Publish the actual failure to all existing waiters, then propagate.
		// Keep it on this node until normal zero-reference cache cleanup.
		completion->set_exception(std::current_exception());
		throw;
	}
	return result;
}

SharedImageRef SexyAppBase::SetSharedImage(const std::string& theFileName, unsigned theVariant, Image* theImage, bool* isNew)
{
	// Only the winning insertion adopts the input. A hit never owns/deletes it.
	return LoadSharedImage(theFileName, theVariant, isNew, [theImage] { return theImage; });
}

SharedImageRef SexyAppBase::GetPreparedSharedImage(const std::string& file, unsigned variant,
	bool* isNew, const std::function<void(Image*)>& prepare, bool lookForAlphaImage)
{
	return LoadSharedImage(file, variant, isNew, [&]
	{
		Image* image = GetImage(file, false, lookForAlphaImage);
		try {
			if (image && prepare)
				prepare(image);
			return image;
		} catch (...) {
			delete image;
			throw;
		}
	});
}

SharedImageRef SexyAppBase::GetSharedImage(const std::string& theFileName, unsigned theVariant, bool* isNew)
{
	return GetPreparedSharedImage(theFileName, theVariant, isNew, {});
}

void SexyAppBase::CleanSharedImages()
{
	AutoCrit anAutoCrit(mDDInterface->mCritSect);	

	// Consume this notification before scanning. A final reference released
	// during the scan must remain queued for the next cleanup pass.
	if (mCleanupSharedImages.exchange(false))
	{
		// Delete shared images with reference counts of 0
		// This doesn't occur in ~SharedImageRef because sometimes we can not only access the image
		//  through the SharedImageRef returned by GetSharedImage, but also by calling GetSharedImage
		//  again with the same params -- so we can have instances where we do the 'final' deref on
		//  an image but immediately re-request it via GetSharedImage
		SharedImageMap::iterator aSharedImageItr = mSharedImageMap.begin();
		while (aSharedImageItr != mSharedImageMap.end())
		{
			SharedImage* aSharedImage = &aSharedImageItr->second;
			if (aSharedImage->mRefCount == 0)
			{
				delete (aSharedImage->mImage);
				mSharedImageMap.erase(aSharedImageItr++);
			}
			else
				++aSharedImageItr;
		}

	}
}
