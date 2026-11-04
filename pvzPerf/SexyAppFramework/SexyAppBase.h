#pragma once
#include "Common.h"
#include "platform/NativeHandles.h"
#include "misc/Rect.h"
#include "graphics/Color.h"
#include "widget/ButtonListener.h"
#include "widget/DialogListener.h"
#include "misc/Buffer.h"
#include "misc/CritSect.h"
#include "graphics/SharedImage.h"
#include "graphics/ImageDiagnostics.h"
#include "graphics/GpuBackend.h"
#include "misc/Ratio.h"
#include "misc/FrameTiming.h"
#include "effect/ScaleEffectCompiler.h"
#include "effect/ScaleEffectSizing.h"
#include <atomic>
#include <functional>
#include <thread>
#include <cstdint>
#include <memory>
#include <array>
#include <exception>
#if defined(_WIN32)
#include "platform/windows/HdrPolicy.h"
#elif defined(__APPLE__)
#include "platform/macos/MacWindow.h"
#elif !defined(_WIN32)
#include "platform/NativeWindow.h"
#endif

extern HMODULE gDSoundDLL;


namespace ImageLib
{
	class Image;
};

namespace Sexy
{

class WidgetManager;
class DDInterface;
class VulkanHdrDisplayObserver;
class Image;
class Image;
class Widget;
class SoundManager;
class MusicInterface;
class Image;
class Dialog;
class _Font;

class ResourceManager;

class WidgetSafeDeleteInfo
{
public:
	int						mUpdateAppDepth;
	Widget*					mWidget;
};


typedef std::list<WidgetSafeDeleteInfo> WidgetSafeDeleteList;
typedef std::set<Image*> GpuImageSet;
typedef std::map<int, Dialog*> DialogMap;
typedef std::list<Dialog*> DialogList;
#ifdef _WIN32
typedef std::list<MSG> WindowsMessageList;
#endif
typedef std::vector<std::string> StringVector;
//typedef std::basic_string<TCHAR> tstring; // string of TCHARs

typedef std::map<std::string, SexyString> StringSexyStringMap;
typedef std::map<std::string, std::string> StringStringMap;
typedef std::map<std::string, std::wstring> StringWStringMap;
typedef std::map<std::string, bool> StringBoolMap;
typedef std::map<std::string, int> StringIntMap;
typedef std::map<std::string, double> StringDoubleMap;
typedef std::map<std::string, StringVector> StringStringVectorMap;

enum
{
	CURSOR_POINTER,
	CURSOR_HAND,
	CURSOR_DRAGGING,
	CURSOR_TEXT,
	CURSOR_CIRCLE_SLASH,
	CURSOR_SIZEALL,
	CURSOR_SIZENESW,
	CURSOR_SIZENS,
	CURSOR_SIZENWSE,
	CURSOR_SIZEWE,	
	CURSOR_WAIT,
	CURSOR_NONE,
	CURSOR_CUSTOM,
	NUM_CURSORS
};

enum
{
	DEMO_MOUSE_POSITION,	
	DEMO_ACTIVATE_APP,
	DEMO_SIZE,
	DEMO_KEY_DOWN,
	DEMO_KEY_UP,
	DEMO_KEY_CHAR,
	DEMO_CLOSE,
	DEMO_MOUSE_ENTER,
	DEMO_MOUSE_EXIT,
	DEMO_LOADING_COMPLETE,
	DEMO_REGISTRY_GETSUBKEYS,
	DEMO_REGISTRY_READ,
	DEMO_REGISTRY_WRITE,
	DEMO_REGISTRY_ERASE,	
	DEMO_FILE_EXISTS,
	DEMO_FILE_READ,
	DEMO_FILE_WRITE,
	DEMO_SYNC,
	DEMO_ASSERT_STRING_EQUAL,
	DEMO_ASSERT_INT_EQUAL,
	DEMO_MOUSE_WHEEL,
	DEMO_HANDLE_COMPLETE,
	DEMO_VIDEO_DATA,
	DEMO_IDLE = 31
};

enum {
	FPS_ShowFPS,
	FPS_ShowCoords,
	Num_FPS_Types
};

enum
{
	UPDATESTATE_MESSAGES,
	UPDATESTATE_PROCESS_1,
	UPDATESTATE_PROCESS_2,
	UPDATESTATE_PROCESS_DONE
};

typedef std::map<HANDLE, int> HandleToIntMap;

class SexyAppBase : public ButtonListener, public DialogListener
{
public:
#ifndef _WIN32
#ifdef __APPLE__
    std::unique_ptr<MacWindow> mNativeWindow;
#else
    std::unique_ptr<NativeWindow> mNativeWindow;
#endif
	int mInstanceLockFd = -1;
#endif
    std::unique_ptr<GpuContext> mGpuContext;
    std::array<HCURSOR, 10> mNativeCursors{};
    int mNativeCursorIndex = -1;
    HCURSOR mNativeAppliedCursor = nullptr;
    bool mNativeResizePending = false;
    uint64_t mNativeMonitorGeneration = 0;
    bool mNativeTopologyPending = false;
    bool mNativeWindowReady = false;
    bool mNativeWindowShown = false;
    bool mNativeFullscreen = false;
    RECT mNativeSavedWindowRect{};
    LONG_PTR mNativeSavedWindowStyle = 0;
    LONG_PTR mNativeSavedWindowExStyle = 0;
#ifdef _WIN32
    [[msvc::no_unique_address]] NativeWindowsHdrState mNativeHdrState;
#endif
    uint64_t mHdrDisplayRevision=0, mHdrTopologyGeneration=0;
    bool mHdrTargetPending=false;
    std::exception_ptr mWindowMessageError;
    std::array<double, 3> mNativeLastClickTime{-1000,-1000,-1000};
    std::array<double, 3> mNativeLastClickX{}, mNativeLastClickY{};
    double mNativeWheelRemainder = 0;
    bool mNativeTouchOwnsCursor = false;
    bool mNativeMouseMotionPending = false;
    POINT mNativeMouseClientPosition{};
    void QueueNativeTouch(unsigned int id, unsigned int flags, double x, double y);
    void PumpWindowEvents();
    void RequestNativeClose();
    void DrainWindowMessages() { ProcessDeferredMessages(false); }
    void RegisterWindowClasses();
    void UpdateWindowGeometry();
    void UpdateMonitorState();
    void UpdateHdrDisplay();
    void ResizeWindowClient(int width, int height);
    void SetWindowFullscreen(bool fullscreen);
#ifdef _WIN32
    NativeMonitorHandle TargetMonitor() const;
    DEVMODEW TargetVideoMode() const;
#endif
    void UpdateNativeCursorSelection();
    void PrepareFullscreenScaleForMonitor();
    void MapWindowMouse(double windowX, double windowY, int& sceneX, int& sceneY) const;
    void NativeMouseMove(double x, double y);
    POINT ReadNativeMousePosition();
    void SampleNativeMousePosition();
    void NativeMouseButton(int button, bool pressed, double x, double y, double time);
    void NativeMouseWheel(int delta);
    void NativeMouseLeave();
    void NativeFocusChanged(bool focused);
    void DestroyGameWindow();

	
	unsigned int			mRandSeed;
		
	std::string				mCompanyName;
	std::string				mFullCompanyName;
	std::string				mProdName;	
	SexyString				mTitle;	
	std::string				mRegKey;
	std::string				mChangeDirTo;
	
	double					mRelaxUpdateBacklogCount; // Remaining milliseconds without catch-up.
	int						mPreferredX;
	int						mPreferredY;
	int						mWidth;
	int						mHeight;
	int						mFullscreenBits;
	float					mMusicVolume;
	double					mSfxVolume;
	float					mDemoMusicVolume;
	double					mDemoSfxVolume;
	bool					mNoSoundNeeded;
	bool					mCmdLineParsed;
	bool					mSkipSignatureChecks;
	bool					mStandardWordWrap;
	bool					mbAllowExtendedChars;

	HANDLE					mMutex;
	bool					mOnlyAllowOneCopyToRun;
	UINT					mNotifyGameMessage;
	CritSect				mCritSect;	
	bool					mBetaValidate;
	uchar					mAdd8BitMaxTable[512];
	WidgetManager*			mWidgetManager;
	DialogMap				mDialogMap;
	DialogList				mDialogList;
	NativeThreadId			mPrimaryThreadId;
	std::thread             mLoadingThread;
	bool					mSEHOccured;
	std::atomic<bool>		mShutdown;
	bool					mExitToTop;
	bool					mIsWindowed;
	bool					mIsPhysWindowed;
	bool					mForceFullscreen;
	bool					mForceWindowed;	
	bool					mInitialized;	
	bool					mProcessInTimer;
	DWORD					mTimeLoaded;
	NativeWindowHandle		mHWnd;
	NativeWindowHandle		mInvisHWnd;
	bool					mUseExternalHWND;
	NativeWindowHandle		mExternalHWND;
	bool					mIsScreenSaver;
	bool					mAllowMonitorPowersave;
#ifdef _WIN32
	WindowsMessageList		mDeferredMessages;
#elif defined(__APPLE__)
	std::list<MacWindowEvent> mDeferredMessages;
#else
	std::list<NativeWindowEvent> mDeferredMessages;
#endif
	bool					mNoDefer;	
	bool					mFullScreenPageFlip;	
	bool					mTabletPC;
	DDInterface*			mDDInterface;
	bool					mAlphaDisabled;
	MusicInterface*			mMusicInterface;	
	bool					mReadFromRegistry;
	std::string				mProductVersion;	
	Image*					mCursorImages[NUM_CURSORS];
	HCURSOR						mOverrideCursor; // Borrowed shared/native cursor; not owned by the game.
	DWORD					mLastTimerTime;
	DWORD					mLastBigDelayTime;	
	float					mUnmutedMusicVolume;
	double					mUnmutedSfxVolume;	
	int						mMuteCount;
	int						mAutoMuteCount;
	bool					mDemoMute;
	bool					mMuteOnLostFocus;
	GpuImageSet				mGpuImageSet;
	SharedImageMap			mSharedImageMap;
	std::atomic<bool>		mCleanupSharedImages;
	
	int						mNonDrawCount;
	int						mFrameTime;
	FrameTiming				mFrameTiming{};
	bool                    mAutoDetectFrameRate = true;
    bool                    mLandscapeUI = false; // settings.json preference; keep on mobile fallback.
    bool                    mLandscapeUIActive = false; // Resolved once at native-window initialization.

		bool					mIsDrawing;
		bool					mRedrawInProgress;
		bool					mLastDrawWasEmpty;

	bool					mHasPendingDraw;
	unsigned long long		mPendingUpdatesAccNs;
	unsigned long long		mUpdateFTimeAccNs;
    unsigned long long mFramePacingPhaseNs = 0; // Wall-clock overshoot of the presentation interval.
    unsigned long long mSimulationPhaseNs = 0; // Already simulated part of the next logical interval.
	FrameDeadline::TimePoint	mLastTimeCheck{};
	DWORD					mLastTime;
	DWORD					mLastUserInputTick;

	std::uint32_t			mDrawCount;
	std::uint32_t			mUpdateCount;
	std::int64_t            mApplicationTimeNs = 0; // Monotonic logical app clock; continues through board pauses.
	int						mUpdateAppState;
	int						mUpdateAppDepth;
	double					mUpdateMultiplier;		
	bool					mPaused;
	std::uint32_t			mFastForwardToUpdateNum;
	bool					mFastForwardToMarker;
	bool					mFastForwardStep;
	DWORD					mLastDrawTick;
	DWORD					mNextDrawTick;
	int						mStepMode;  // 0 = off, 1 = step, 2 = waiting for step

	int						mCursorNum;
	SoundManager*			mSoundManager;
	WidgetSafeDeleteList	mSafeDeleteList;
	bool					mMouseIn;	
	bool					mRunning;
	bool					mActive;
	bool					mMinimized;
	bool					mPhysMinimized;
	bool					mIsDisabled;
	bool					mHasFocus;
	bool					mShowFPS;
	int						mShowFPSMode;
	bool					mAutoStartLoadingThread;
	std::atomic<bool>		mLoadingThreadStarted;
	std::atomic<bool>		mLoadingThreadCompleted;
	bool					mLoaded;
	bool					mYieldMainThread;
	std::atomic<bool>		mLoadingFailed;
	bool					mSysCursor;
	bool					mCustomCursorsEnabled;
	bool					mCustomCursorDirty;	
	bool					mLastShutdownWasGraceful;
	bool					mIsWideWindow;
	std::atomic<int>		mNumLoadingThreadTasks;
	std::atomic<int>		mCompletedLoadingThreadTasks;

	// For recording/playback of program control
	bool					mRecordingDemoBuffer;
	bool					mPlayingDemoBuffer;
	bool					mManualShutdown;
	std::string				mDemoPrefix;
	std::string				mDemoFileName;
	Buffer					mDemoBuffer;
	std::uint32_t			mDemoLength;
	int						mLastDemoMouseX;
	int						mLastDemoMouseY;
	std::uint32_t			mLastDemoUpdateCnt;
	bool					mDemoNeedsCommand;
	bool					mDemoIsShortCmd;
	int						mDemoCmdNum;
	std::uint32_t			mDemoCmdOrder;
	int						mDemoCmdBitPos;
	bool					mDemoLoadingComplete;
	HandleToIntMap			mHandleToIntMap; // For waiting on handles
	int						mCurHandleNum;

	typedef std::pair<std::string, std::uint32_t> DemoMarker;
	typedef std::list<DemoMarker> DemoMarkerList;
	DemoMarkerList			mDemoMarkerList;

	bool					mDebugKeysEnabled;
	bool					mEnableMaximizeButton;
		bool					mCtrlDown;
		bool					mAltDown;
		bool					mAltEnterDown;
		bool					mSwitchingScreenMode;

	bool					mCursorClipActive;
	RECT					mCursorClipRect;

	int						mSyncRefreshRate;
	bool					mVSyncUpdates;
	bool					mVSyncBroken;
	int						mVSyncBrokenCount;
	DWORD					mVSyncBrokenTestStartTick;
	DWORD					mVSyncBrokenTestUpdates;
	bool					mWaitForVSync;
	bool					mSoftVSyncWait;
	bool					mTest3D;
	DWORD					mMinVidMemory3D;
	DWORD					mRecommendedVidMemory3D;

	bool					mWidescreenAware;
	Rect					mScreenBounds;
	// Optional source-art bounds for horizontal framing; zero width means centered.
	bool					mEnableWindowAspect;
	Ratio					mWindowAspect;
	int						mWindowScale;
	bool						mUseCustomScale;				// 自定义放缩开关
	int						mScaleConfigWindow;				// scale.json 窗口放缩倍率
	int						mWindowScaleFromPipeline;			// 当前窗口 pipeline 推导出的运行时倍率

	int						mScaleConfigFullscreenWidth;
	int						mScaleConfigFullscreenHeight;

	// MagpieFX-style scaling pipelines (scale.json):
	// External JSON paths are retained; parameter names and mode strings decode
	// once at LoadScaleConfig into local ParamIds and numeric scaling fields.
	struct ScaleEffectConfig
	{
		std::string mPath;
		ScaleRuntimeParams mParams;
		size_t mEffectSlot = SIZE_MAX;
		int mScalingType = SCALE_SCALING_NORMAL;
		float mScaleX = 1.0f;
		float mScaleY = 1.0f;
	};
	struct ScalePipelineConfig
	{
		std::vector<ScaleEffectConfig> mEffects;
		int mOutputWidth;
		int mOutputHeight;
		bool mDeclared = false;
	};
	ScalePipelineConfig					mScaleWindowConfig;
	ScalePipelineConfig					mScaleFullscreenConfig;
	// scale.json 顶级 HDR 对象：启用时交换链切 HDR10/PQ（运行时不可切换，
	// 仅启动时应用一次；请求后若显示器/Windows/交换链不满足则立即失败）。
	// 唯一暴露项 = strength（AutoHDRStrength 同义，默认 1）。亮度锚
	// 不暴露、不回写：paperWhite/maxTV/reinhardPaper 全部来自显示器
	// 运行时探测（微软 AutoHDR 同源，启动读一次，不轮询）。
	// 渲染为 AutoHDR 复刻（dxgi.dll Sdr2HdrData 反汇编重建）；无饱和度
	// 参数（Windows 校准色度矩阵已作用于全部内容）。
	bool								mScaleHDREnabled = false;
    bool mModernScalePrepared = false;
    int mModernScaleTargetW = 0, mModernScaleTargetH = 0;
	float								mScaleHDRPaperWhite = 80.0f;   // nits ↔ PaperWhiteNits
	float								mScaleHDRMaxNits = 1000.0f;    // nits ↔ MaxTVNits（私有查询/面板元数据）
	float								mScaleHDRReinhardPaperNits = 0.0f; // nits ↔ ReinhardPaperWhiteNits（SDR 白公式）
	float								mScaleHDRStrength = 1.0f;      // ↔ AutoHDRStrength（唯一可调）
	std::vector<ScaleEffectRuntimeDesc>		mScaleEffects;
	ResourceManager*		mResourceManager;

	LONG					mOldWndProc;

protected:	
	void					RehupFocus();
	void					ClearKeysDown();
	virtual void			SwitchScreenMode();
	virtual void			SwitchScreenMode(bool wantWindowed);
	virtual void			SwitchScreenMode(bool wantWindowed, bool force);
	void					ReconfigureScale(bool theUseCustomScale);
	bool					ProcessDeferredMessages(bool singleMessage);
#if defined(__APPLE__)
	void ProcessMacWindowEvent(const MacWindowEvent& event);
#elif !defined(_WIN32)
	void ProcessNativeWindowEvent(const NativeWindowEvent& event);
#endif
#ifdef _WIN32
	void					ProcessTimerMessage(NativeWindowHandle hWnd);
	void					ProcessMessageNow(UINT uMsg, WPARAM wParam, LPARAM lParam, NativeWindowHandle hWnd);
#endif
	void					UpdateFTimeAcc();
	bool					ResizeAppWindowForMode(bool wantWindowed);
	virtual bool			Process(bool allowSleep = true);		
	virtual void			UpdateFrames();
	virtual bool			DoUpdateFrames();
	virtual void			DoUpdateFramesF(float theFrac);
	virtual void			MakeWindow();
	virtual void			EnforceCursor();
	
	// Loading thread methods	
	std::exception_ptr		mLoadingThreadError;
	virtual void			LoadingThreadCompleted();
	static void				LoadingThreadProcStub(void *theArg);

	
	void					WaitForLoadingThread();				
	void					ProcessSafeDeleteList();	
	void					DoExit(int theCode);

	unsigned mPendingImageDiagnostics = 0;
	bool mImageDiagnosticsActive = false;
	bool DeferImageDiagnostics(unsigned request);
	void ProcessPendingImageDiagnostics();
	std::optional<std::vector<ImageDiagnosticRecord>> CaptureImageDiagnostics(bool includePixels);
	void					TakeScreenshot();
	void					DumpProgramInfo();	
	void					ShowMemoryUsage();			

	// Registry helpers
	bool					RegistryRead(const std::string& theValueName, unsigned int* theType, uchar* theValue, unsigned int* theLength);
#ifdef _WIN32
	bool					RegistryReadKey(const std::string& theValueName, unsigned int* theType, uchar* theValue, unsigned int* theLength, HKEY theMainKey = HKEY_CURRENT_USER);
#endif
	bool					RegistryWrite(const std::string& theValueName, unsigned int theType, const uchar* theValue, unsigned int theLength);

	// Demo recording helpers	
	void					ProcessDemo();

public:
	SexyAppBase();
	virtual ~SexyAppBase();

	// Common overrides:
	virtual MusicInterface*	CreateMusicInterface();
	virtual void			InitHook();
	virtual void			ShutdownHook();	
	virtual void			PreTerminate();
	virtual void			LoadingThreadProc();
	virtual void			WriteToRegistry();
	virtual void			ReadFromRegistry();
	void                    DetectTargetFrameRate();
	virtual Dialog*			NewDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode);		
	virtual void			PreDisplayHook();
	virtual _Font*			GetDebugFont();

	// Public methods
	virtual void			BeginPopup();
	virtual void			EndPopup();
	virtual int				MsgBox(const std::string &theText, const std::string &theTitle = "Message", int theFlags = MB_OK);
	virtual int				MsgBox(const std::wstring &theText, const std::wstring &theTitle = L"Message", int theFlags = MB_OK);
	virtual void			Popup(const std::string& theString);
	virtual void			Popup(const std::wstring& theString);
	virtual void			LogScreenSaverError(const std::string &theError);
	virtual void			SafeDeleteWidget(Widget* theWidget);	

	virtual std::string		GetProductVersion(const std::string& thePath);	

		virtual void			SEHOccured();
		virtual std::string		GetGameSEHInfo();
		void					UpdateCursorClip();
		void					ReleaseCursorClip();
		virtual void			Shutdown();

	virtual void			DoParseCmdLine();
	virtual void			ParseCmdLine(const std::string& theCmdLine);
	virtual void			HandleCmdLineParam(const std::string& theParamName, const std::string& theParamValue);
	virtual void			HandleNotifyGameMessage(int theType); // for HWND_BROADCAST of mNotifyGameMessage (0-1000 are reserved for SexyAppBase for theType)
	virtual void			HandleGameAlreadyRunning(); 

	virtual void			Start();	
	virtual void			Init();	
	virtual void			PreDDInterfaceInitHook();
	virtual void			PostDDInterfaceInitHook();
	virtual bool			ChangeDirHook(const char *theIntendedPath);
	virtual void			PlaySample(int theSoundNum);
	virtual void			PlaySample(int theSoundNum, int thePan);

	virtual bool			IsMuted();

	virtual void			SetMusicVolume(float theVolume);
	virtual void			SetSfxVolume(double theVolume);	
	virtual void			Mute(bool autoMute = false);
	virtual void			Unmute(bool autoMute = false);

	void					StartLoadingThread();
	void					RethrowLoadingThreadError();
	virtual double			GetLoadingThreadProgress();	

	void					CopyToClipboard(const std::string& theString);
	std::string				GetClipboard();

	void					SetCursor(int theCursorNum);
	void					EnableCustomCursors(bool enabled);	
	[[nodiscard("caller owns the returned Image")]]
	virtual Image*		GetImage(const std::string& theFileName, bool commitBits = true,
		bool lookForAlphaImage = true);
	virtual SharedImageRef	SetSharedImage(const std::string& theFileName, unsigned theVariant, Image* theImage, bool* isNew);
	virtual SharedImageRef	GetSharedImage(const std::string& theFileName, unsigned theVariant = 0, bool* isNew = NULL);
	// The initializer runs only for a new image, before it becomes visible to
	// cache hits. Existing callers retain their serial hit/reconfiguration rules.
	SharedImageRef GetPreparedSharedImage(const std::string& file, unsigned variant,
		bool* isNew, const std::function<void(Image*)>& prepare, bool lookForAlphaImage = true);
	SharedImageRef LoadSharedImage(const std::string& file, unsigned variant,
		bool* isNew, const std::function<Image*()>& load);

	void					CleanSharedImages();
	void					SetCursorImage(int theCursorNum, Image* theImage);

	[[nodiscard("caller owns the returned Image")]]
	Image*				CreateColorizedImage(Image* theImage, const Color& theColor);
	[[nodiscard("caller owns the returned Image")]]
	Image*				CopyImage(Image* theImage, const Rect& theRect);
	[[nodiscard("caller owns the returned Image")]]
	Image*				CopyImage(Image* theImage);

	void					AddGpuImage(Image* image);
	void					RemoveGpuImage(Image* image);
		void					Remove3DData(Image* theMemoryImage);
		// The sole user-facing display request. Input and UI callers provide only
		// the desired logical mode and scale policy; transaction details remain
		// internal so every source receives the same completion semantics.
		void					RequestScreenMode(bool wantWindowed, bool useCustomScale);
		virtual void			SetAlphaDisabled(bool isDisabled);

	
	virtual Dialog*			DoDialog(int theDialogId, bool isModal, const SexyString& theDialogHeader, const SexyString& theDialogLines, const SexyString& theDialogFooter, int theButtonMode);
	virtual Dialog*			GetDialog(int theDialogId);
	virtual void			AddDialog(int theDialogId, Dialog* theDialog);
	virtual void			AddDialog(Dialog* theDialog);
	virtual bool			KillDialog(int theDialogId, bool removeWidget, bool deleteWidget);
	virtual bool			KillDialog(int theDialogId);
	virtual bool			KillDialog(Dialog* theDialog);
	virtual int				GetDialogCount();
	virtual void			ModalOpen();
	virtual void			ModalClose();	
	virtual void			DialogButtonPress(int theDialogId, int theButtonId);
	virtual void			DialogButtonDepress(int theDialogId, int theButtonId);

	virtual void			GotFocus();
	virtual void			LostFocus();	
	virtual bool			IsAltKeyUsed(WPARAM wParam);
	void					DispatchKeyDown(int theKey);
	virtual bool			DebugKeyDown(int theKey);	
//	virtual bool			DebugKeyDownAsync(int theKey, bool ctrlDown, bool altDown);
	virtual void			CloseRequestAsync();
	void					DemoSyncRefreshRate();
	virtual void			Done3dTesting();
	
//	virtual bool			CheckSignature(const Buffer& theBuffer, const std::string& theFileName);
	virtual bool			DrawDirtyStuff();
	virtual void			Redraw(Rect* theClipRect);

	virtual void			InitPropertiesHook();

	// Resource access methods
	void					ShowResourceError(bool doExit = false);
	
	// Demo access methods
	bool					PrepareDemoCommand(bool required);
	void                    RequireDemoCommand(int expected);
	void					WriteDemoTimingBlock();
	void					WriteDemoBuffer();
	void					ArmDemoFlushAtExit();  // -record 时挂 atexit 落盘（析构路径不保证执行）
	bool					ReadDemoBuffer(std::string &theError);//UNICODE
	void					DemoSyncBuffer(Buffer* theBuffer);
	void					DemoSyncString(std::string* theString);
	void					DemoSyncInt(int* theInt);
	void					DemoSyncBool(bool* theBool);
	void					DemoAssertStringEqual(const std::string& theString);
	void					DemoAssertIntEqual(int theInt);
	void					DemoAddMarker(const std::string& theString);
	void					DemoRegisterHandle(HANDLE theHandle);
	void					DemoWaitForHandle(HANDLE theHandle);
	bool					DemoCheckHandle(HANDLE theHandle);
	

	// Registry access methods
	bool					RegistryGetSubKeys(const std::string& theKeyName, StringVector* theSubKeys);
	bool					RegistryReadString(const std::string& theValueName, std::string* theString);
	bool					RegistryReadInteger(const std::string& theValueName, int* theValue);
	bool					RegistryReadBoolean(const std::string& theValueName, bool* theValue);
	bool					RegistryReadData(const std::string& theValueName, uchar* theValue, unsigned int* theLength);
	bool					RegistryWriteString(const std::string& theValueName, const std::string& theString);
	bool					RegistryWriteInteger(const std::string& theValueName, int theValue);
	bool					RegistryWriteBoolean(const std::string& theValueName, bool theValue);
	bool					RegistryWriteData(const std::string& theValueName, const uchar* theValue, unsigned int theLength);
	bool					RegistryEraseKey(const SexyString& theKeyName);
	void					RegistryEraseValue(const SexyString& theValueName);

	// File access methods
	bool					WriteBufferToFile(const std::string& theFileName, const Buffer* theBuffer);
	bool					ReadBufferFromFile(const std::string& theFileName, Buffer* theBuffer, bool dontWriteToDemo = false);//UNICODE
	bool					WriteBytesToFile(const std::string& theFileName, const void *theData, unsigned int theDataLen);
	bool					FileExists(const std::string& theFileName);
	bool					EraseFile(const std::string& theFileName);

	// Misc methods
	virtual void			DoMainLoop();
	virtual bool			UpdateAppStep(bool* updated);
	virtual bool			UpdateApp();
		int						InitDDInterface();
		void					LoadScaleConfig();

	void					ResolveHDRAutoParams();
	void					CompileScaleEffects();
				bool					IsScalePresentActive() const;
				bool					IsScalePresentActiveForMode(bool theWindowed) const;

			bool					BuildScalePipelineForMode(bool theWindowed, std::vector<ScaleEffectRuntimeDesc*>& outEffects, std::vector<ScaleEffectScaling>& outScalings);
			bool					ComputeCustomScaleOutputSize(bool theWindowed, int theSrcW, int theSrcH, int& outW, int& outH);
				bool					GetCustomScalePresentSize(int& outW, int& outH);
				bool					GetCustomScalePresentSizeForMode(bool theWindowed, int& outW, int& outH);

			int					GetWindowScaleForCustomScale() const;
			void					UpdateWindowScaleFromPipeline();
	bool					PrepareScalePipelineObjects();
	void					WarmScalePipelinesBeforeShow();
	void					CleanupScalePresent();
	void					ClearUpdateBacklog(bool relaxForASecond = false);
	virtual bool			AppCanRestore();
#ifdef _WIN32
	static LRESULT CALLBACK	WindowProc(NativeWindowHandle hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
#endif
};

extern SexyAppBase* gSexyAppBase;

};
