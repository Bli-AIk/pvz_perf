#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "LawnApp.h"
#include "Resources.h"
#include "Lawn/Board.h"
#include "Lawn/Plant.h"
#include "Lawn/Zombie.h"
#include "Lawn/System/PlayerInfo.h"
#include "Lawn/System/ProfileMgr.h"
#include "Lawn/System/PoolEffect.h"
#include "Lawn/Widget/GameButton.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/TodCommon.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "SexyAppFramework/graphics/DDInterface.h"
#include "SexyAppFramework/graphics/RenderInterface.h"
#include "SexyAppFramework/graphics/Image.h"
#include "SexyAppFramework/graphics/Graphics.h"
#include "SexyAppFramework/widget/WidgetManager.h"
#include "SexyAppFramework/platform/windows/WindowApi.h"
#include <array>
#include <cstdlib>
#include "misc/TimeCounter.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>

bool (*gAppCloseRequest)();

namespace {
class FixtureApp : public LawnApp {
public:
    void DoParseCmdLine() override { mCmdLineParsed = true; }
    void Popup(const std::string& text) override { throw std::runtime_error(text); }
    void Popup(const std::wstring& text) override { throw std::runtime_error(std::string(text.begin(), text.end())); }
};
void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
}

int main(int argc, char** argv)
{
    using namespace Sexy;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try {
        Require(argc == 3, "Pass paired Release runtime directory and existing output directory");
        Require(SetProcessAffinityMask(GetCurrentProcess(), 0x4000), "Cannot pin LP14");
        Require(NativeWindowsWindowApi::EnableSystemDpiAwareness(), "Cannot set DPI awareness");
        const std::filesystem::path output(argv[2]);
        gTodStringFormats = gLawnStringFormats;
        gTodStringFormatCount = gLawnStringFormatCount;
        gAppCloseRequest = LawnGetCloseRequest;
        auto* application = new FixtureApp;
        auto& app = *application;
        gLawnApp = application;
        app.mChangeDirTo = argv[1];
        app.mReadFromRegistry = false;
        app.mCmdLineParsed = true;
        app.mTodCheatKeys = true;
        app.mOnlyAllowOneCopyToRun = false;
        app.mNoSoundNeeded = true;
        app.mShowFPS = false;
        app.Init();
        Require(!app.mLoadingFailed, "Init resource failure");
        app.StartLoadingThread();
        const auto started = std::chrono::steady_clock::now();
        while (!app.mLoadingThreadCompleted.load(std::memory_order_acquire)) {
            app.UpdateApp();
            Require(std::chrono::steady_clock::now() - started < std::chrono::seconds(120), "Loading timeout");
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (app.mLoadingThread.joinable()) app.mLoadingThread.join();
        app.RethrowLoadingThreadError();
        app.mLoaded = true;
        app.LoadingThreadCompleted();
        if (!app.mPlayerInfo) app.mPlayerInfo = app.mProfileMgr->AddProfile("FixedDraw");
        app.mPlayerInfo->mFinishedAdventure = 1;
        app.mPlayerInfo->mLevel = 50;
        app.LoadingCompleted();
        app.KillGameSelector();
        Require(!app.mLoadingFailed, "Runtime resources failed");
        RECT bounds{0, 0, 800, 600};
        const auto style = static_cast<DWORD>(GetWindowLongPtrW(app.mHWnd, GWL_STYLE));
        const auto extended = static_cast<DWORD>(GetWindowLongPtrW(app.mHWnd, GWL_EXSTYLE));
        Require(NativeWindowsWindowApi::AdjustClientRect(bounds, style, extended, app.mHWnd), "Cannot size fixture window");
        Require(SetWindowPos(app.mHWnd, nullptr, 0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top,
            SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE), "Cannot resize fixture window");
        app.UpdateWindowGeometry();
        SRand(0x13579bd);
        app.PreNewGame(GameMode::GAMEMODE_SURVIVAL_ENDLESS_STAGE_3, false);
        app.StartPlaying();
        auto& board = *app.mBoard;
        board.mLevelTimeNs = 1000ULL * 10000000ULL;
        SRand(0x13579bd);
        constexpr std::array<SeedType, 4> seeds{SEED_PEASHOOTER, SEED_SUNFLOWER, SEED_WALLNUT, SEED_SNOWPEA};
        constexpr std::array<ZombieType, 4> zombies{ZOMBIE_NORMAL, ZOMBIE_TRAFFIC_CONE, ZOMBIE_PAIL, ZOMBIE_FLAG};
        constexpr std::array<int, 4> rows{0, 1, 4, 5};
        for (int row = 0; row < 4; ++row) for (int col = 0; col < 8; ++col)
            board.AddPlant(col, rows[row], seeds[(col + row) % 4]);
        const int perRow = std::getenv("PVZ_AB_DENSE") ? 60 : 10;
        for (int row = 0; row < 4; ++row) for (int col = 0; col < perRow; ++col) {
            auto* zombie = board.AddZombieInRow(zombies[(col + row) % 4], rows[row], 0);
            zombie->mX = 320.0f + (col % 10) * 43.0f;
            zombie->mVelX = 0;
        }
        app.mPoolEffect->mSampleTimeNs = 2'560'000'000;
        app.mPoolEffect->mBirthTimeNs = 0;
        auto& render = *app.mDDInterface->mRenderInterface;
        auto& gpu = *render.mRenderer;
        Require(gpu.sceneWidth == 800 && gpu.sceneHeight == 600, "Fixture requires 800x600 scene");
        const auto frozenDrawCount = board.mDrawCount;
        const auto clock = board.mLevelTimeNs;
        const auto plants = board.mPlants.mSize;
        const auto count = board.mZombies.mSize;
        Require(plants == 32 && count == 4 * perRow, "Fixture population differs");
        render.Flush();
        render.SetCausticsTimeNs(2'560'000'000ull);
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        bool replaying = false;
        auto draw = [&] {
            if (!replaying)
            {
                board.mDrawCount = frozenDrawCount;
                SRand(0x2468ace);
            }
            render.Flush();
            app.mDDInterface->mScreenImage->FillRect(Rect(0, 0, 800, 600), Color::Black, Graphics::DRAWMODE_NORMAL);
            app.mWidgetManager->MarkAllDirty();
            LARGE_INTEGER begin, end;
            ULONG64 first, last;
            QueryThreadCycleTime(GetCurrentThread(), &first);
            QueryPerformanceCounter(&begin);
            Require(app.mWidgetManager->DrawScreen(), "Widget draw failed");
            QueryPerformanceCounter(&end);
            QueryThreadCycleTime(GetCurrentThread(), &last);
            render.Flush();
            return std::pair{(end.QuadPart - begin.QuadPart) * 1e6 / frequency.QuadPart, last - first};
        };
        for (int i = 0; i < 128; ++i) draw();
        const auto pixels = gpu.ReadScenePixels();
        Require(pixels.size() == 800 * 600, "Unexpected readback size");
        std::ofstream binary(output / "frozen-draw-pixels.bin", std::ios::binary);
        binary.write(reinterpret_cast<const char*>(pixels.data()), pixels.size() * sizeof(pixels[0]));
        binary.close();
        std::ofstream samples(output / "frozen-draw-perf.csv");
        samples << "round,sample,draw_cpu_us,thread_cycles,plants,zombies\n";
        samples.setf(std::ios::fixed); samples.precision(3);
        for (int round = 0; round < 20; ++round) {
            for (int sample = 0; sample < 32; ++sample) {
                const auto measured = draw();
                samples << round << ',' << sample << ',' << measured.first << ',' << measured.second << ',' << plants << ',' << count << '\n';
            }
            Require(gpu.ReadScenePixels() == pixels, "Frozen pixels changed within run");
            Require(board.mLevelTimeNs == clock && board.mPlants.mSize == plants && board.mZombies.mSize == count, "Frozen state changed");
        }
        samples.close();
        std::printf("PASS frozen DrawScreen 128 warmup, 640 samples, 32 plants, 40 zombies, 800x600; flush/readback excluded\n");

        std::ofstream phases(output / "phase-perf.csv");
        phases << "phase,round,iterations,cpu_us,thread_cycles\n";
        auto measure = [&](const char* name, int iterations, auto action) {
            for (int i = 0; i < 128; ++i) action();
            for (int round = 0; round < 15; ++round) {
                LARGE_INTEGER begin, end;
                ULONG64 first, last;
                QueryThreadCycleTime(GetCurrentThread(), &first);
                QueryPerformanceCounter(&begin);
                for (int i = 0; i < iterations; ++i) action();
                QueryPerformanceCounter(&end);
                QueryThreadCycleTime(GetCurrentThread(), &last);
                phases << name << ',' << round << ',' << iterations << ','
                    << (end.QuadPart - begin.QuadPart) * 1e6 / frequency.QuadPart / iterations
                    << ',' << double(last - first) / iterations << '\n';
            }
        };
        app.mActive = true;
        Sexy::gUpdateIntervalNs = 0;
        app.mWidgetManager->mLastMouseX = 100;
        app.mWidgetManager->mLastMouseY = 100;
        measure("board_hover", 1000, [&] { board.UpdateMousePosition(); });
        measure("menu_button_update", 1000, [&] { board.mMenuButton->Update(); });
        app.mNativeWindowShown = true;
        app.mPhysMinimized = app.mMinimized = app.mSwitchingScreenMode = false;
        app.mPlayingDemoBuffer = false;
        measure("native_mouse_poll", 1000, [&] { app.SampleNativeMousePosition(); });
        measure("actors_zero_delta", 256, [&] { board.UpdateGameObjects(0); });
        std::printf("population after phases: %u plants, %u zombies\n", board.mPlants.mSize, board.mZombies.mSize);
        Require(board.mPlants.mSize == plants && board.mZombies.mSize == count, "Phase population changed");
        phases.close();
        if (const char* checkpoint = std::getenv("PVZ_AB_ENDLESS"))
        {
            replaying = true;
            Require(board.LoadGame(checkpoint), "Cannot load canonical checkpoint");
            board.mPaused = false;
            app.mGameScene = SCENE_PLAYING;
            app.mActive = true;
            app.mNativeWindowShown = false; // Keep physical input out of deterministic replay.
            app.mMinimized = false;
            Sexy::gUpdateIntervalNs = 4'166'666;
            Sexy::gUpdateTicks = double(Sexy::gUpdateIntervalNs) / 10'000'000;
            Sexy::gStepsPerLegacyTick = 1 / Sexy::gUpdateTicks;
            Sexy::gLegacyTickPhaseNs = 0;
            auto expectedTime = board.mLevelTimeNs;
            SRand(0x13579bd);
            std::ofstream replay(output / "replay.csv");
            replay << "second,level_ns,wave,plants,zombies,projectiles,update_us,draw_us\n";
            for (int second = 0; second < 25; ++second)
            {
                double updateUs = 0, drawUs = 0;
                for (int frame = 0; frame < 240; ++frame)
                {
                    LARGE_INTEGER begin, end;
                    const auto stepNs = Sexy::gUpdateIntervalNs;
                    QueryPerformanceCounter(&begin);
                    app.UpdateFrames();
                    QueryPerformanceCounter(&end);
                    updateUs += (end.QuadPart - begin.QuadPart) * 1e6 / frequency.QuadPart;
                    drawUs += draw().first;
                    expectedTime += stepNs;
                    if (board.mLevelTimeNs != expectedTime)
                        throw std::runtime_error("Replay clock mismatch: actual=" + std::to_string(board.mLevelTimeNs) +
                            " expected=" + std::to_string(expectedTime) + " scene=" + std::to_string(app.mGameScene) +
                            " paused=" + std::to_string(board.mPaused));
                }
                replay << second + 1 << ',' << board.mLevelTimeNs << ',' << board.mCurrentWave << ','
                    << board.mPlants.mSize << ',' << board.mZombies.mSize << ',' << board.mProjectiles.mSize << ','
                    << updateUs / 240 << ',' << drawUs / 240 << '\n';
            }
            replay.close();
        }
        app.Shutdown();
        delete application;
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
