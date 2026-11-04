#include "Sexy.TodLib/Reanimator.h"
#include "ParticleRenderParamsTests.h"
#include "SexyAppFramework/SexyAppBase.h"
#include "SexyAppFramework/graphics/DDInterface.h"
#include "SexyAppFramework/graphics/Graphics.h"
#include "SexyAppFramework/graphics/RenderInterface.h"
#include "SexyAppFramework/misc/ResourceManager.h"
#include "SexyAppFramework/paklib/PakInterface.h"
#include "Sexy.TodLib/TodCommon.h"
#include "Sexy.TodLib/DefinitionImageRegion.h"
#include "Sexy.TodLib/generated/GeneratedDefinitions.h"
#include "Sexy.TodLib/generated/SceneAtlasData.h"
#include "Sexy.TodLib/generated/SceneAtlasBindings.h"
#include "SexyAppFramework/graphics/Image.h"
#include "SexyAppFramework/graphics/AtlasDraw.h"
#include "SexyAppFramework/imagelib/ImageLib.h"
#include "LawnApp.h"
#include "Lawn/Widget/ZombatarResources.h"
#include "Resources.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <cmath>
#include <random>
#include <limits>
#include "ActorFloatRuntimeChecks.h"

// Replace only the real game's entry point; all application/rendering objects
// and generated definition objects come from the existing production build.
bool FixtureCloseRequest() { return false; }
bool (*gAppCloseRequest)() = FixtureCloseRequest;

namespace
{
using namespace Sexy;
namespace Definitions = GeneratedDefinitions;

void Require(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error(message);
}

LONG CALLBACK SuppressCrashDialogs(EXCEPTION_POINTERS* exception)
{
    const DWORD code = exception->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR ||
        code == EXCEPTION_ILLEGAL_INSTRUCTION || code == EXCEPTION_STACK_OVERFLOW ||
        code == EXCEPTION_INT_DIVIDE_BY_ZERO)
    {
        std::fprintf(stderr, "FAIL static definitions: hardware exception %08lX\n", code);
        std::fflush(stderr);
        TerminateProcess(GetCurrentProcess(), code);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

class TestApp final : public LawnApp
{
public:
    using Sexy::SexyAppBase::Process;
    bool scheduleProbe = false;
    bool refuseProbeUpdate = false;
    unsigned long long probeTimeNs = 0;
    unsigned long long probeMaxStepNs = 0;
    unsigned int probeUpdates = 0;
    unsigned int probeDraws = 0;
    bool DoUpdateFrames() override
    {
        if (!scheduleProbe) return LawnApp::DoUpdateFrames();
        if (refuseProbeUpdate) return false;
        probeTimeNs += Sexy::gUpdateIntervalNs;
        probeMaxStepNs = std::max(probeMaxStepNs, Sexy::gUpdateIntervalNs);
        ++probeUpdates;
        ++mUpdateCount;
        return true;
    }
    void DoUpdateFramesF(float fraction) override
    {
        if (!scheduleProbe) LawnApp::DoUpdateFramesF(fraction);
    }
    bool DrawDirtyStuff() override
    {
        if (!scheduleProbe) return LawnApp::DrawDirtyStuff();
        ++probeDraws;
        mHasPendingDraw = false;
        return true;
    }
    void ButtonPress(int) override {}
    void ButtonDepress(int) override {}
    void ButtonDownTick(int) override {}
    void ButtonMouseEnter(int) override {}
    void ButtonMouseLeave(int) override {}
    void ButtonMouseMove(int, int, int) override {}
};

void CheckActualScheduler(TestApp& app)
{
    const auto oldTiming = app.mFrameTiming;
    const auto oldStep = Sexy::gUpdateIntervalNs;
    const auto oldCount = app.mUpdateCount;
    const auto oldLoaded = app.mLoaded;
    const auto oldYield = app.mYieldMainThread;
    app.scheduleProbe = true;
    app.mLoaded = true;
    app.mYieldMainThread = false;
    for (const auto [fps, elapsed] : std::vector<std::pair<unsigned int, unsigned long long>>{
             {10000, 6'000'000}, {250, 5'123'456}, {240, 6'000'000}, {100, 12'500'000},
             {100, 17'000'000}, {100, 40'000'000}})
    {
        app.mFrameTiming = Sexy::FrameTiming::FromTargetFps(fps);
        app.mUpdateAppState = Sexy::UPDATESTATE_PROCESS_1;
        app.mNonDrawCount = 0;
        app.mUpdateFTimeAccNs = 0;
        app.mPendingUpdatesAccNs = 0;
        app.mFramePacingPhaseNs = app.mSimulationPhaseNs = 0;
        app.probeTimeNs = app.probeMaxStepNs = 0;
        app.probeUpdates = app.probeDraws = 0;
        const auto origin = std::chrono::time_point_cast<std::chrono::nanoseconds>(
            Sexy::FrameDeadline::Clock::now()) - std::chrono::nanoseconds(elapsed);
        app.mLastTimeCheck = origin;
        app.Process(false);
        const auto elapsedUpperBound = static_cast<unsigned long long>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                Sexy::FrameDeadline::Clock::now() - origin).count());
        Require(app.mUpdateAppState == Sexy::UPDATESTATE_PROCESS_2,
            "Actual scheduler did not start an overdue update");
        app.Process(false);
        Require(app.probeTimeNs + app.mPendingUpdatesAccNs >= elapsed &&
            app.probeTimeNs + app.mPendingUpdatesAccNs <= elapsedUpperBound,
            "Actual Process discarded elapsed time or used the target interval");
        Require(app.probeUpdates == 1 && app.probeMaxStepNs == app.probeTimeNs &&
            app.probeDraws == 1,
            "Actual Process split elapsed time into fixed updates or starved drawing");
        const auto quantum = app.mFrameTiming.mIntervalNs;
        Require(app.mPendingUpdatesAccNs < quantum - app.mSimulationPhaseNs,
            "Actual Process left a due logical interval unprocessed");
        if (fps == 250 && app.probeTimeNs < 8'000'000 && app.mSimulationPhaseNs > 0)
        {
            const auto previous = app.probeTimeNs;
            const auto compensation = quantum - app.mSimulationPhaseNs;
            app.mUpdateAppState = Sexy::UPDATESTATE_PROCESS_1;
            app.mLastTimeCheck = std::chrono::time_point_cast<std::chrono::nanoseconds>(
                Sexy::FrameDeadline::Clock::now()) - std::chrono::nanoseconds(compensation);
            app.mUpdateFTimeAccNs = 0;
            app.Process(false);
            Require(app.mUpdateAppState == Sexy::UPDATESTATE_PROCESS_2,
                "Actual Process ignored the shortened compensated interval");
            app.Process(false);
            Require(app.probeTimeNs - previous >= compensation &&
                app.probeTimeNs - previous < quantum && app.probeDraws == 2,
                "Actual Process could not execute a short compensation update");
        }
    }
    // DoUpdateFrames also refuses work while the screensaver is active or
    // a demo awaits input. Neither case may strand the native message stage.
    app.mFrameTiming = Sexy::FrameTiming::FromTargetFps(100);
    app.mUpdateAppState = Sexy::UPDATESTATE_PROCESS_1;
    app.mHasPendingDraw = false;
    app.probeTimeNs = app.probeUpdates = app.probeDraws = 0;
    app.refuseProbeUpdate = true;
    app.mLastTimeCheck = std::chrono::time_point_cast<std::chrono::nanoseconds>(
        Sexy::FrameDeadline::Clock::now()) - std::chrono::milliseconds(250);
    app.Process(false);
    Require(app.mUpdateAppState == Sexy::UPDATESTATE_PROCESS_DONE &&
        app.probeUpdates == 0 && app.mPendingUpdatesAccNs == 0 && app.mUpdateFTimeAccNs == 0,
        "Refused update stranded PROCESS_1 or retained frozen simulation backlog");
    bool updated = true;
    Require(app.UpdateAppStep(&updated) && !updated &&
        app.mUpdateAppState == Sexy::UPDATESTATE_PROCESS_1,
        "Refused update did not return through the native message stage");
    app.refuseProbeUpdate = false;
    app.mLastTimeCheck = std::chrono::time_point_cast<std::chrono::nanoseconds>(
        Sexy::FrameDeadline::Clock::now()) - std::chrono::milliseconds(10);
    app.Process(false);
    Require(app.mUpdateAppState == Sexy::UPDATESTATE_PROCESS_2 && app.probeUpdates == 1,
        "Scheduler did not resume after a refused update");
    app.Process(false);
    Require(app.probeTimeNs < 30'000'000 && app.probeDraws == 1,
        "Scheduler replayed the frozen quarter-second after resuming");
    std::cout << "PASS refused updates: native messages resume, frozen backlog discarded and retry draws\n";
    app.scheduleProbe = false;
    app.mFrameTiming = oldTiming;
    app.mLoaded = oldLoaded;
    app.mYieldMainThread = oldYield;
    app.mUpdateCount = oldCount;
    app.mUpdateAppState = Sexy::UPDATESTATE_PROCESS_DONE;
    app.mHasPendingDraw = false;
    app.ClearUpdateBacklog(false);
    Sexy::ApplySimulationStep(oldStep);
    std::cout << "PASS actual Process: elapsed-time conservation, uncapped actual steps and per-update drawing\n";
}

class HiddenWindow
{
public:
    HWND handle = nullptr;
    HiddenWindow()
    {
        WNDCLASSW type{};
        type.style = CS_OWNDC;
        type.lpfnWndProc = DefWindowProcW;
        type.hInstance = GetModuleHandleW(nullptr);
        type.lpszClassName = L"PvzStaticDefinitionTests";
        Require(RegisterClassW(&type) != 0, "Cannot register hidden test window");
        handle = CreateWindowExW(0, type.lpszClassName, L"Static definition tests",
            WS_POPUP, 0, 0, 64, 64, nullptr, nullptr, type.hInstance, nullptr);
        Require(handle != nullptr, "Cannot create hidden test window");
    }
    ~HiddenWindow()
    {
        if (handle) DestroyWindow(handle);
        UnregisterClassW(L"PvzStaticDefinitionTests", GetModuleHandleW(nullptr));
    }
};

using TrackMember = FloatParameterTrack TodEmitterDefinition::*;
constexpr TrackMember Tracks[]{
    &TodEmitterDefinition::mSystemDuration, &TodEmitterDefinition::mCrossFadeDuration,
    &TodEmitterDefinition::mSpawnRate, &TodEmitterDefinition::mSpawnMinActive,
    &TodEmitterDefinition::mSpawnMaxActive, &TodEmitterDefinition::mSpawnMaxLaunched,
    &TodEmitterDefinition::mEmitterRadius, &TodEmitterDefinition::mEmitterOffsetX,
    &TodEmitterDefinition::mEmitterOffsetY, &TodEmitterDefinition::mEmitterBoxX,
    &TodEmitterDefinition::mEmitterBoxY, &TodEmitterDefinition::mEmitterSkewX,
    &TodEmitterDefinition::mEmitterSkewY, &TodEmitterDefinition::mEmitterPath,
    &TodEmitterDefinition::mParticleDuration, &TodEmitterDefinition::mLaunchSpeed,
    &TodEmitterDefinition::mLaunchAngle, &TodEmitterDefinition::mParticleSpinAngle,
    &TodEmitterDefinition::mParticleSpinSpeed, &TodEmitterDefinition::mCollisionReflect,
    &TodEmitterDefinition::mCollisionSpin, &TodEmitterDefinition::mAnimationRate,
    &TodEmitterDefinition::mSystemRed, &TodEmitterDefinition::mSystemGreen,
    &TodEmitterDefinition::mSystemBlue, &TodEmitterDefinition::mSystemAlpha,
    &TodEmitterDefinition::mSystemBrightness, &TodEmitterDefinition::mParticleRed,
    &TodEmitterDefinition::mParticleGreen, &TodEmitterDefinition::mParticleBlue,
    &TodEmitterDefinition::mParticleAlpha, &TodEmitterDefinition::mParticleBrightness,
    &TodEmitterDefinition::mParticleScale, &TodEmitterDefinition::mParticleStretch,
    &TodEmitterDefinition::mClipTop, &TodEmitterDefinition::mClipBottom,
    &TodEmitterDefinition::mClipLeft, &TodEmitterDefinition::mClipRight};
static_assert(std::size(Tracks) == 38);

// Static reanimation storage retains its addresses. Compiled particle records
// retain their authored values across allocation, release and reload.
struct StaticSnapshot
{
    struct Block
    {
        const unsigned char* address;
        std::vector<unsigned char> bytes;
        std::string label;
        bool transforms;
        std::vector<unsigned char> Read() const
        {
            std::vector<unsigned char> result(address, address + bytes.size());
            if (transforms)
                for (std::size_t offset = 0; offset < result.size();
                    offset += sizeof(ReanimatorTransform))
                    std::memset(result.data() + offset + offsetof(ReanimatorTransform, mFont),
                        0, sizeof(ReanimatorTransform::mFont));
            return result;
        }
    };
    std::vector<Block> blocks;
    std::size_t nodeCount = 0, frameCount = 0, fieldCount = 0;
    mutable std::vector<unsigned char> particleValues;

    template<class T>
    void Add(const T* pointer, std::size_t count, const std::string& label, bool transforms = false)
    {
        if (!count) return;
        Require(pointer != nullptr, "Null static backing: " + label);
        Block block{reinterpret_cast<const unsigned char*>(pointer),
            std::vector<unsigned char>(count * sizeof(T)), label, transforms};
        block.bytes = block.Read();
        blocks.push_back(std::move(block));
    }
    void Track(const FloatParameterTrack& track)
    {
        Require(track.mCountNodes >= 0, "Negative static track length");
        Add(track.mNodes, track.mCountNodes, "particle track nodes");
        nodeCount += track.mCountNodes;
    }
    StaticSnapshot()
    {
        static_assert(sizeof(FloatParameterTrackNode) == 24);
        Add(Definitions::Reanimations, Definitions::ReanimationCount, "reanimation roots");
        Add(Definitions::Projectiles.mProjectiles.data(), 14, "projectile visuals");
        Add(Definitions::Projectiles.mShadows.data(), 2, "projectile shadows");
        Add(MagnetAtlas::Images,MagnetAtlas::Count,"magnet scene rectangles");
        Add(MagnetAtlas::Scales,MagnetAtlas::Count,"magnet scene scales");
        Add(AlmanacAtlas::Images,AlmanacAtlas::Count,"almanac scene rectangles");
        Add(AchievementsAtlas::Images,AchievementsAtlas::Count,"achievement scene rectangles");
        Add(CommonUIAtlas::Images,CommonUIAtlas::Count,"common UI scene rectangles");
        Add(DesktopUIAtlas::Images,DesktopUIAtlas::Count,"desktop UI scene rectangles");
        Add(LandscapeUIAtlas::Images,LandscapeUIAtlas::Count,"landscape UI scene rectangles");
        for (const auto& definition : Definitions::Reanimations)
        {
            Add(definition.mTracks.tracks, definition.mTracks.count, "reanimation tracks");
            Add(definition.mImageRects, definition.mVariantCount * definition.mImageSlotCount,
                "reanimation rectangles");
            Add(definition.mVariantTransitions, definition.mVariantCount * definition.mVariantActionCount,
                "variant transitions");
            Add(definition.mVariantActionStarts, definition.mTracks.count,
                "variant action starts");
            Add(definition.mVariantActionCounts, definition.mTracks.count,
                "variant action counts");
            Add(definition.mVariantOptions, definition.mVariantCount * definition.mTracks.count,
                "variant options");
            for (int track = 0; track < definition.mTracks.count; ++track)
            {
                const auto& transforms = definition.mTracks.tracks[track].mTransforms;
                Add(transforms.mTransforms, transforms.count, "reanimation transforms", true);
                frameCount += transforms.count;
            }
        }
        for (const auto segments : Definitions::AttacherSegments)
            Add(segments.data(), segments.size(), "prelinked attacher segments");
        Require(frameCount > 10000, "Snapshot did not cover the reanimation catalog");
    }
    std::vector<unsigned char> ReadParticles() const
    {
        std::vector<unsigned char> result;
        std::size_t nodes = 0, fieldsCount = 0;
        const auto append = [&](const auto& value) {
            const auto* bytes = reinterpret_cast<const unsigned char*>(&value);
            result.insert(result.end(), bytes, bytes + sizeof(value));
        };
        const auto track = [&](const FloatParameterTrack& value) {
            append(value.mCountNodes);
            for (int i = 0; i < value.mCountNodes; ++i) append(value.mNodes[i]);
            nodes += value.mCountNodes;
        };
        for (const auto& definition : Definitions::Particles)
        {
            append(definition.mVariantCount); append(definition.mImageRectCount);
            append(definition.mEmitterDefCount); append(definition.mEmitterIndexByName);
            for (std::size_t i = 0; i < std::size_t(definition.mVariantCount) * definition.mImageRectCount; ++i)
                append(definition.mImageRects[i]);
            for (int index = 0; index < definition.mEmitterDefCount; ++index)
            {
                const auto& emitter = definition.mEmitterDefs[index];
                append(emitter.mImageRectOffset); append(emitter.mImageRectCount);
                append(emitter.mImageFrames); append(emitter.mAnimated);
                append(emitter.mParticleFlags); append(emitter.mEmitterType);
                append(emitter.mNameId); append(emitter.mOnDurationEmitterIndex);
                for (const auto member : Tracks) track(emitter.*member);
                for (const auto& fields : {emitter.mParticleFields, emitter.mSystemFields})
                {
                    append(fields.count);
                    fieldsCount += fields.count;
                    for (int f = 0; f < fields.count; ++f)
                    {
                        append(fields.Fields[f].mFieldType);
                        track(fields.Fields[f].mX);
                        track(fields.Fields[f].mY);
                    }
                }
            }
        }
        Require(nodes == 5328 && fieldsCount == 260,
            "Snapshot did not cover the compiled particle catalog");
        return result;
    }
    void Check() const
    {
        for (const auto& block : blocks)
            Require(block.Read() == block.bytes, "Static backing changed: " + block.label);
        if (gParticleAllDefinitionDataLoaded.load())
        {
            auto values = ReadParticles();
            if (particleValues.empty()) particleValues = std::move(values);
            else Require(values == particleValues, "Reloaded compiled particle values changed");
        }
    }
};

template<class Definition>
void CheckTextures(const Definition& definition, bool loaded)
{
    for (unsigned variant = 0; variant < definition.mVariantCount; ++variant)
    {
        const auto count=[](const auto& value) {
            if constexpr (requires { value.mImageSlotCount; }) return value.mImageSlotCount;
            else return value.mImageRectCount;
        }(definition);
        bool hasImage=false;
        for (unsigned slot=0;slot<count;++slot)
            hasImage|=definition.mImageRects[variant*count+slot].mWidth>0;
        Require(loaded ? (!hasImage || definition.mTextures[variant]) :
            !definition.mTextures[variant],
            "Definition GPU slots disagree with their loading state");
    }
}

void CheckParticleTextures(bool loaded)
{
    for (const auto& definition : Definitions::Particles) CheckTextures(definition, loaded);
}

void CheckReanimationTextures(bool loaded)
{
    for (const auto& definition : Definitions::Reanimations)
    {
        CheckTextures(definition, loaded);
    }
}

void CheckSceneTextures(TestApp& app,bool loaded)
{
    struct Scene {
        GpuTexture* texture;
        const TextureRegion* rectangles;
        std::size_t count;
    };
    const Scene scenes[]{
        {MagnetAtlas::Texture,MagnetAtlas::Images,MagnetAtlas::Count},
        {AlmanacAtlas::Texture,AlmanacAtlas::Images,AlmanacAtlas::Count},
        {AchievementsAtlas::Texture,AchievementsAtlas::Images,AchievementsAtlas::Count},
        {CommonUIAtlas::Texture,CommonUIAtlas::Images,CommonUIAtlas::Count},
        {DesktopUIAtlas::Texture,DesktopUIAtlas::Images,DesktopUIAtlas::Count},
        {LandscapeUIAtlas::Texture,LandscapeUIAtlas::Images,LandscapeUIAtlas::Count}};
    std::set<GpuTexture*> textures;
    std::size_t rectangles=0;
    for (const auto& scene:scenes)
    {
        Require((scene.texture!=nullptr)==loaded,"Scene GPU slot disagrees with loading state");
        for (std::size_t index=0;index<scene.count;++index)
        {
            const auto& rectangle=scene.rectangles[index];
            Require(std::isfinite(rectangle.mU0) && std::isfinite(rectangle.mV0) &&
                std::isfinite(rectangle.mU1) && std::isfinite(rectangle.mV1) &&
                rectangle.mU0>=0 && rectangle.mV0>=0 &&
                rectangle.mU0<rectangle.mU1 && rectangle.mV0<rectangle.mV1 &&
                rectangle.mU1<=1 && rectangle.mV1<=1 &&
                rectangle.mWidth>0 && rectangle.mHeight>0,
                "Scene UV view exceeds its packed sheet atlas or lost its logical size");
            ++rectangles;
        }
        if (!loaded) continue;
        Require(textures.insert(scene.texture).second,"Independent scene atlases aliased one GPU allocation");
        for (const auto& [key,entry]:app.mSharedImageMap)
        {
            const auto* image=entry.mImage;
            Require(!image || image->mTexture!=scene.texture,
                "Scene atlas retained an ordinary image wrapper instead of a direct GPU pointer");
        }
    }
    Require(rectangles==MagnetAtlas::Count+AlmanacAtlas::Count+AchievementsAtlas::Count+CommonUIAtlas::Count+DesktopUIAtlas::Count+LandscapeUIAtlas::Count,"Scene fixture must cover all authored UI views");
    Require(!loaded || textures.size()==6,"Scene fixture omitted a GPU atlas");
}

std::size_t FontBindingCount()
{
    std::size_t count = 0;
    for (const auto& definition : Definitions::Reanimations)
        for (int t = 0; t < definition.mTracks.count; ++t)
            for (int f = 0; f < definition.mTracks.tracks[t].mTransforms.count; ++f)
                count += definition.mTracks.tracks[t].mTransforms.mTransforms[f].mFont != nullptr;
    return count;
}

void ReleaseAll(TestApp& app)
{
    FreeSceneAtlases();
    ZombatarFreeResources();
    TodParticleFreeDefinitions();
    ReanimatorFreeDefinitions();
    DefinitionFreeAtlasImages();
    FreeSceneAtlases();
    TodParticleFreeDefinitions();
    ReanimatorFreeDefinitions();
    DefinitionFreeAtlasImages();
    Require(!gParticleDefArray && !gParticleDefCount &&
        !gParticleAllDefinitionDataLoaded.load() && !gReanimatorDefArray,
        "Repeated release left published catalogs");
    CheckParticleTextures(false);
    CheckReanimationTextures(false);
    CheckSceneTextures(app,false);
    Require(!Definitions::Projectiles.mTexture && FontBindingCount() == 0,
        "Full release retained projectile/font bindings");
}

void LoadAll(TestApp& app)
{
    LoadSceneAtlases(false);
    LandscapeUIAtlas::Load();
    // Exercise the consuming upload boundary with every real scene PNG.
    // Definition loading uses this same Image*& contract; it must clear the
    // decode owner immediately, even while the GPU atlas remains drawable.
    auto& render=*app.mDDInterface->mRenderInterface;
    for (const auto& extent:SceneAtlasBindings::Extents)
    {
        ImageLib::Image* decoded=ImageLib::GetImage(extent.image,false);
        Require(decoded && decoded->mBits && decoded->mWidth==int(extent.width) &&
            decoded->mHeight==int(extent.height),"Cannot decode the complete scene PNG upload fixture");
        auto* texture=render.UploadAtlasTexture(decoded);
        Require(texture && !decoded,"Scene PNG upload retained its CPU decode owner");
    }
    ZombatarLoadResources();
    ReanimatorLoadDefinitions(gLawnReanimationArray, static_cast<int>(ReanimationType::NUM_REANIMS));
    TodParticleLoadDefinitions(gLawnParticleArray, ParticleEffect::NUM_PARTICLES);
    ReanimatorLoadAllDefinitionData();
    const auto tasks = app.mCompletedLoadingThreadTasks.load();
    const auto projectile = Definitions::Projectiles.mTexture;
    const std::array<GpuTexture*,6> scenes{
        MagnetAtlas::Texture,AlmanacAtlas::Texture,AchievementsAtlas::Texture,CommonUIAtlas::Texture,DesktopUIAtlas::Texture,LandscapeUIAtlas::Texture};
    CheckSceneTextures(app,true);
    std::vector<GpuTexture*> textures;
    for (const auto& definition : Definitions::Reanimations)
        textures.insert(textures.end(), definition.mTextures,
            definition.mTextures + definition.mVariantCount);
    for (const auto& definition : Definitions::Particles)
        textures.insert(textures.end(), definition.mTextures,
            definition.mTextures + definition.mVariantCount);
    ReanimatorLoadDefinitions(gLawnReanimationArray, static_cast<int>(ReanimationType::NUM_REANIMS));
    TodParticleLoadDefinitions(gLawnParticleArray, ParticleEffect::NUM_PARTICLES);
    ReanimatorLoadAllDefinitionData();
    Definitions::LoadAllImages();
    LoadSceneAtlases(false);
    LandscapeUIAtlas::Load();
    std::vector<GpuTexture*> repeated;
    for (const auto& definition : Definitions::Reanimations)
        repeated.insert(repeated.end(), definition.mTextures,
            definition.mTextures + definition.mVariantCount);
    for (const auto& definition : Definitions::Particles)
        repeated.insert(repeated.end(), definition.mTextures,
            definition.mTextures + definition.mVariantCount);
    Require(repeated == textures && Definitions::Projectiles.mTexture == projectile &&
        app.mCompletedLoadingThreadTasks == tasks, "Repeated bulk load changed live handles or progress");
    Require(scenes==std::array<GpuTexture*,6>{MagnetAtlas::Texture,AlmanacAtlas::Texture,
        AchievementsAtlas::Texture,CommonUIAtlas::Texture,DesktopUIAtlas::Texture,LandscapeUIAtlas::Texture},"Repeated scene load replaced live GPU pointers");
    CheckSceneTextures(app,true);
    Require(gReanimatorDefArray == Definitions::Reanimations &&
        gParticleDefArray == Definitions::Particles && gParticleDefCount == 106 &&
        gParticleAllDefinitionDataLoaded.load(), "Public loaders did not publish the static catalogs");
    CheckParticleTextures(true);
    CheckReanimationTextures(true);
    Require(projectile && FontBindingCount() > 0, "Bulk load omitted projectile/font bindings");
    Require(DefinitionUploadAtlasImages() == 0 && ReanimatorUploadAllTexturesToGPU() == 0,
        "Primary-thread repeated bulk load queued duplicate uploads");
    std::set<GpuTexture*> unique(textures.begin(), textures.end());
    unique.insert(projectile);
    unique.insert(gZombatarAtlas.mTexture);
    unique.erase(nullptr);
    Require(unique.size() == 193, "Definitions did not share the 193 physical atlas textures");
    std::unique_ptr<Image> sceneTarget(render.CreateRenderImage(120,120));
    std::size_t drawn=0;
    {
        GpuImageDrawScope destination(render,sceneTarget.get());
        Graphics graphics(sceneTarget.get());
        graphics.mColor=Color::Black;
        graphics.FillRect(Rect(0,0,120,120));
        const auto draw=[&](GpuTexture* texture,const TextureRegion* regions,std::size_t count) {
            TextureSource source{texture,0};
            for (std::size_t index=0;index<count;++index,++drawn)
                DrawAtlasImage(&graphics,source,regions[index],
                    Rect(int(drawn%10)*12,int(drawn/10)*12,10,10),
                    Rect(0,0,regions[index].mWidth,regions[index].mHeight));
        };
        draw(MagnetAtlas::Texture,MagnetAtlas::Images,MagnetAtlas::Count);
        draw(AlmanacAtlas::Texture,AlmanacAtlas::Images,AlmanacAtlas::Count);
        draw(AchievementsAtlas::Texture,AchievementsAtlas::Images,AchievementsAtlas::Count);
        draw(CommonUIAtlas::Texture,CommonUIAtlas::Images,CommonUIAtlas::Count);
        draw(DesktopUIAtlas::Texture,DesktopUIAtlas::Images,DesktopUIAtlas::Count);
        draw(LandscapeUIAtlas::Texture,LandscapeUIAtlas::Images,LandscapeUIAtlas::Count);
    }
    render.Flush();
    const auto scenePixels=render.ReadImagePixels(*sceneTarget);
    Require(drawn==MagnetAtlas::Count+AlmanacAtlas::Count+AchievementsAtlas::Count+CommonUIAtlas::Count+DesktopUIAtlas::Count+LandscapeUIAtlas::Count && std::any_of(scenePixels.pixels.begin(),scenePixels.pixels.end(),
        [](unsigned pixel) { return pixel!=0xff000000u; }),
        "Real scene consumers failed to draw the complete scene view catalog");
}

std::vector<unsigned> Sample(TestApp& app, GpuTexture* texture, const TextureRegion& rectangle)
{
    auto& render = *app.mDDInterface->mRenderInterface;
    Require(texture && rectangle.mWidth > 0 && rectangle.mHeight > 0, "Missing shared atlas sample");
    std::unique_ptr<Image> target(render.CreateRenderImage(64, 64));
    {
        GpuImageDrawScope destination(render, target.get());
        Graphics graphics(target.get());
        graphics.mColor = Color::Black;
        graphics.FillRect(Rect(0, 0, 64, 64));
        TextureSource source{texture, 0};
        SexyMatrix3 transform;
        const float scale = 48.0f / std::max(rectangle.mWidth, rectangle.mHeight);
        TodScaleRotateTransformMatrix(transform, 32, 32, 0, scale, scale);
        source.BltTransformed(transform, rectangle, Color::White, &graphics.mClipRect, 0, 0, true);
    }
    render.Flush();
    auto pixels = render.ReadImagePixels(*target).pixels;
    Require(std::any_of(pixels.begin(), pixels.end(),
        [](auto pixel) { return (pixel & 0xFFFFFFu) != 0; }), "Shared atlas GPU draw was empty");
    return pixels;
}

std::vector<unsigned> Sample(TestApp& app, GpuTexture* texture, const TodParticleDefinition& definition)
{
    Require(definition.mImageRectCount > 0, "Missing shared atlas definition");
    return Sample(app, texture, definition.mImageRects[0]);
}

void RealConsumers(int reanimationIndex, int particleIndex)
{
    Reanimation animation;
    animation.ReanimationInitializeType(0, 0, static_cast<ReanimationType>(reanimationIndex), 0);
    Require(animation.mDefinition == &Definitions::Reanimations[reanimationIndex] &&
        animation.mTrackInstances, "Real reanimation did not borrow the static definition");
    TodParticleData data;
    data.Initialize();
    struct PoolCleanup { TodParticleData& data; ~PoolCleanup() { data.Dispose(); } } cleanup{data};
    TodParticleHolder holder(&data);
    auto* system = holder.AllocParticleSystem(0, 0, 0, static_cast<ParticleEffect>(particleIndex), 0, 0);
    Require(system && system->mParticleDef == &Definitions::Particles[particleIndex] &&
        system->EmitterRosterCount() > 0 && system->EmitterRosterEmitterAt(0)->mEmitterDef ==
        Definitions::Particles[particleIndex].mEmitterDefs,
        "Real particle system did not borrow the static emitter backing");
}

void CurveTiming()
{
    using I = std::int64_t;
    constexpr I span = 1'000'000'000;
    constexpr I epoch = 8'000'000'000'000'000'000LL;
    const auto local = TodAnimateCurveNs(0, span, 1, 0.0f, 1.0f, Curves::Linear);
    Require(local > 0 && local == static_cast<float>(1.0 / span),
        "Production nanosecond curve discarded a one-nanosecond interval");
    Require(TodAnimateCurveNs(epoch, epoch + span, epoch + 1,
        0.0f, 1.0f, Curves::Linear) == local,
        "Production curve converted an absolute epoch before subtracting timestamps");
    constexpr I fractionalAge = 1'234'567, fractionalSpan = 17'000'003;
    const auto fractional = TodAnimateCurveNs(0, fractionalSpan, fractionalAge,
        0.0f, 1.0f, Curves::Linear);
    Require(fractional == static_cast<float>(double(fractionalAge) / fractionalSpan) &&
        fractional != TodAnimateCurveNs(0, fractionalSpan, 0, 0.0f, 1.0f, Curves::Linear),
        "Production curve quantized a fractional legacy tick");
    Require(TodAnimateCurveNs(span, 0, span - 1, 0.0f, 1.0f, Curves::Linear) == local &&
        TodAnimateCurveNs(epoch + span, epoch, epoch + span - 1,
            0.0f, 1.0f, Curves::Linear) == local,
        "Production reversed curve lost local nanosecond progress");
    Require(TodAnimateCurveNs(-1'000'000'003, 1'000'000'007, 7,
        0.0f, 1.0f, Curves::Linear) ==
        static_cast<float>(1'000'000'010.0 / 2'000'000'010.0),
        "Production curve mishandled a negative origin followed by a positive time");

    constexpr I minimum = std::numeric_limits<I>::min();
    constexpr I maximum = std::numeric_limits<I>::max();
    const float fullRangeFirst = static_cast<float>(
        1.0 / double(std::numeric_limits<std::uint64_t>::max()));
    Require(TodAnimateCurveNs(minimum, maximum, minimum + 1,
        0.0f, 1.0f, Curves::Linear) == fullRangeFirst &&
        TodAnimateCurveNs(maximum, minimum, maximum - 1,
            0.0f, 1.0f, Curves::Linear) == fullRangeFirst,
        "Production full signed timestamp range overflowed or lost its first interval");
    Require(TodAnimateCurveNs(minimum, maximum, 0, 0.0f, 1.0f, Curves::Linear) == 0.5f &&
        TodAnimateCurveNs(maximum, minimum, 0, 0.0f, 1.0f, Curves::Linear) == 0.5f,
        "Production full signed timestamp range did not interpolate through zero");
    Require(TodAnimateCurveNs(41, 41, 40, 3.0f, 7.0f, Curves::Linear) == 3.0f &&
        TodAnimateCurveNs(41, 41, 41, 3.0f, 7.0f, Curves::Linear) == 7.0f &&
        TodAnimateCurveNs(41, 41, 42, 3.0f, 7.0f, Curves::Linear) == 7.0f &&
        TodAnimateCurveNs(maximum, maximum, maximum, 3.0f, 7.0f, Curves::Linear) == 7.0f,
        "Production zero-duration curve does not act as an instantaneous step");

    struct EndpointCase { TodCurveEvalFn curve; bool returnsToStart; };
    constexpr EndpointCase curves[] = {
        {Curves::Constant, true}, {Curves::Linear, false},
        {Curves::EaseIn, false}, {Curves::EaseOut, false},
        {Curves::EaseInOut, false}, {Curves::EaseInOutWeak, false},
        {Curves::FastInOut, false}, {Curves::FastInOutWeak, false},
        {Curves::Bounce, true}, {Curves::BounceFastMiddle, true},
        {Curves::BounceSlowMiddle, true}, {Curves::SinWave, true},
        {Curves::EaseSinWave, true}
    };
    for (const auto& entry : curves)
    {
        constexpr float start = -3.25f, end = 8.75f;
        const float finish = entry.returnsToStart ? start : end;
        for (const I time : {I(99), I(100)})
            Require(TodAnimateCurveNs(100, 107, time, start, end, entry.curve) == start,
                "Production curve changed its initial endpoint or clamp");
        for (const I time : {I(107), I(108)})
            Require(TodAnimateCurveNs(100, 107, time, start, end, entry.curve) == finish,
                "Production curve changed its final endpoint or return-to-start behavior");
        Require(TodAnimateCurveNs(107, 100, 108, start, end, entry.curve) == start &&
            TodAnimateCurveNs(107, 100, 99, start, end, entry.curve) == finish &&
            entry.curve(-1.0, start, end) == start &&
            entry.curve(2.0, start, end) == finish,
            "Production reverse range or direct curve endpoint clamp changed");
    }
    Require(TodAnimateCurveNs(0, 10, 5, 0, 1, Curves::Linear) == 1 &&
        TodAnimateCurveNs(0, 10, 5, 0, -1, Curves::Linear) == -1 &&
        TodAnimateCurveNs(10, 0, 5, 0, 3, Curves::Linear) == 2,
        "Production integer curve output no longer uses legacy nearest rounding");
    Require(TodAnimateCurveFloat(4.25, 9.75, 4.375, 0.0f, 44.0f, Curves::Linear) == 1.0f &&
        TodAnimateCurve(0.0, 10.0, 2.5, 0, 10, Curves::Linear) == 3,
        "Production generic range truncated a fractional distance or input");
    constexpr float farEndpoint = 1'000'000'000.0f;
    const double tinyProgress = 1.0 / (double(farEndpoint) + 1.0);
    Require(std::fabs(TodAnimateCurveFloat(0.0, 1.0, tinyProgress,
        -1.0f, farEndpoint, Curves::Linear)) < 1e-12f,
        "Production curve rounded local progress to float before evaluating its warp");
    std::cout << "PASS production curves: nanoseconds, extreme epochs, signed and reversed ranges, "
        "zero-duration steps, 13 endpoint contracts, integer rounding and double progress\n";
}

void ReanimationIntegerPlayback()
{
    using I = std::int64_t;
    const auto initialize = [](Reanimation& animation, ReanimLoopType loop, float rate, I nowNs = 0) {
        animation.ReanimationInitializeType(0, 0, ReanimationType::REANIM_STINKY, nowNs);
        animation.PlayReanim(ReanimTrackId::anim_crawl, loop, 0, rate);
        animation.Seek(337);
    };
    for (const auto loop : {ReanimLoopType::REANIM_LOOP, ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME})
    for (const auto rate : {13.3072289f, -13.3072289f, 700.001f})
    {
        constexpr I total = 13'781'234'567;
        Reanimation reference;
        initialize(reference, loop, rate);
        const I startPhase = reference.SamplePlayback().phaseNs;
        const I signedPhase = startPhase + (rate < 0 ? -total : total);
        I expectedWraps = signedPhase / reference.mPeriodNs;
        I expectedPhase = signedPhase % reference.mPeriodNs;
        if (expectedPhase < 0) { expectedPhase += reference.mPeriodNs; --expectedWraps; }
        const auto rootOrigin = reference.RootMotion(ReanimTrackId::_ground, 15.0f);
        const auto clippedOrigin = reference.RootMotionClamped(ReanimTrackId::_ground, 15.0f, 50.0f);
        const auto future = reference.RootMotion(ReanimTrackId::_ground, 15.0f, total);
        reference.Advance(total);
        const auto expected = reference.SamplePlayback();
        Require(expected.phaseNs == expectedPhase && expected.cycles == 0,
            "Consumed cursor differs from independent signed integer phase");
        Require(reference.RootMotion(ReanimTrackId::_ground, 15.0f) == future,
            "Production future root sample differs from actual elapsed playback");
        const auto analytic = [&reference, loop, expectedWraps, expectedPhase](bool clipped) {
            const auto* keys = reference.mDefinition->mTracks.tracks[
                reference.FindTrackIndex(ReanimTrackId::_ground)].mTransforms.mTransforms;
            const int count = reference.mFrameCount;
            const int intervals = loop == ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME ? count : count - 1;
            const long double source = static_cast<long double>(expectedPhase) * intervals / reference.mPeriodNs;
            const int current = int(source);
            const long double seconds = static_cast<long double>(reference.mPeriodNs) / intervals / 1e9L;
            long double cycle = 0, partial = 0;
            for (int i = 0; i < intervals; ++i)
            {
                const auto delta = (static_cast<long double>(
                    keys[reference.mFrameStart + std::min(i + 1, count - 1)].mTransX) -
                    keys[reference.mFrameStart + i].mTransX);
                const long double velocity = delta * static_cast<long double>(count) * 15 / intervals / seconds;
                const long double distance = (clipped ? std::max(velocity, 50.0L) : velocity) * seconds;
                cycle += distance;
                if (i < current) partial += distance;
                else if (i == current) partial += distance * (source - current);
            }
            return expectedWraps * cycle + partial;
        };
        const auto checkAnalytic = [](double actual, long double integral) {
            Require(std::fabs(static_cast<long double>(actual) - integral) <=
                1e-9L * (1 + std::fabs(integral)),
                "Production root differs from independently integrated source intervals");
        };
        checkAnalytic(reference.RootMotion(ReanimTrackId::_ground, 15.0f), analytic(false));
        checkAnalytic(reference.RootMotionClamped(ReanimTrackId::_ground, 15.0f, 50.0f), analytic(true));
        for (const int frequency : {30, 60, 100, 144, 200, 240, 1000, 0})
        {
            Reanimation animation;
            initialize(animation, loop, rate);
            I elapsed = 0, events = 0;
            double lastRoot = rootOrigin, lastClipped = clippedOrigin;
            std::mt19937_64 random(0x7181);
            for (I n = 1; elapsed < total; ++n)
            {
                const I next = frequency ? std::min(total, n * 1'000'000'000 / frequency) :
                    std::min(total, elapsed + I(random() % 71'000'003 + 1));
                animation.Advance(next - elapsed);
                elapsed = next;
                lastRoot = animation.RootMotion(ReanimTrackId::_ground, 15.0f);
                lastClipped = animation.RootMotionClamped(ReanimTrackId::_ground, 15.0f, 50.0f);
                events += animation.TimedEventCount(2, 5);
            }
            const auto actual = animation.SamplePlayback();
            Require(actual.phaseNs == expected.phaseNs && actual.cycles == expected.cycles &&
                animation.mLastUpdateTimeNs == total && animation.mLoopCount == reference.mLoopCount,
                "Production elapsed playback depends on outer update partition");
            // Root bases contain physical doubles, not lifetime cycle counters.
            // Compare each partition to the independently integrated authored keys.
            checkAnalytic(lastRoot, analytic(false));
            checkAnalytic(lastClipped, analytic(true));
            Require(events == reference.TimedEventCount(2, 5),
                "Production event multiplicity depends on outer update partition");
        }
        const auto generation = reference.mPlaybackGeneration;
        reference.ApplyRate(0);
        const auto paused = reference.SamplePlayback();
        reference.Advance(7'000'000'001);
        Require(reference.SamplePlayback().phaseNs == paused.phaseNs &&
            reference.SamplePlayback().cycles == paused.cycles, "Production pause moved phase");
        const auto oldPeriod = reference.mPeriodNs;
        reference.ApplyRate(17.003f);
        const auto resumed = reference.SamplePlayback();
        Require(resumed.phaseNs == ReanimClock::SignedMulDiv(paused.phaseNs, reference.mPeriodNs, oldPeriod) &&
            resumed.cycles == paused.cycles && reference.mPlaybackGeneration == generation,
            "Production rate change reset playback or discarded whole phase");
        reference.ApplyRate(-17.003f);
        Require(reference.SamplePlayback().phaseNs == resumed.phaseNs,
            "Production reverse-rate switch reset phase");
    }
    for (const auto loop : {REANIM_LOOP, REANIM_LOOP_FULL_LAST_FRAME})
    for (const float rate : {12.0f, -12.0f})
    {
        Reanimation cursor;
        initialize(cursor, loop, rate);
        cursor.Seek(rate > 0 ? 999 : 1);
        const auto initial = cursor.SamplePlayback();
        const I delta = rate > 0 ? cursor.mPeriodNs - initial.phaseNs + 1 : initial.phaseNs + 1;
        const auto pendingRoot = cursor.RootMotion(ReanimTrackId::_ground, 1.0f, delta);
        const auto pendingFrame = cursor.SamplePlayback(delta);
        Require(pendingFrame.cycles == (rate > 0 ? 1 : -1),
            "Pending wrap must be relative to the cached frame");
        cursor.Advance(delta);
        Require(cursor.SamplePlayback().cycles == 0 && cursor.mCycleCrossings == (rate > 0 ? 1 : -1),
            "Consumed wrap was retained as a cumulative cursor cycle");
        Require(cursor.RootMotion(ReanimTrackId::_ground, 1.0f) == pendingRoot,
            "Committing a forward/reverse wrap changed sampled root displacement");
        const double committed = cursor.mRootDisplacement;
        const auto phase = cursor.SamplePlayback().phaseNs;
        const int loops = cursor.mLoopCount;
        cursor.Advance(0);
        Require(cursor.mRootDisplacement == committed && cursor.mCycleCrossings == 0 &&
            cursor.mLoopCount == loops && cursor.SamplePlayback().phaseNs == phase,
            "Consuming the same timestamp added a root cycle twice");
        cursor.ApplyRate(rate * 2);
        Require(cursor.mRootDisplacement == committed,
            "Changing rate rewrote physical completed root displacement");
        cursor.ApplyRate(0);
        Require(cursor.mRootDisplacement == committed,
            "Pausing rewrote physical completed root displacement");
    }
    {
        ReanimationHolder clock;
        Reanimation paused;
        initialize(paused, REANIM_LOOP, 12.0f);
        paused.Advance(123'456'789);
        paused.ApplyRate(0);
        const auto expected = paused.SamplePlayback();
        paused.mReanimationHolder = &clock;
        clock.mVisualTimeNs = paused.mLastUpdateTimeNs + 3 * paused.mFramePeriodNs;
        const auto wire = SaveRecordMapping::WriteSaveRecord(paused);
        Reanimation restored;
        SaveRecordMapping::ReadSaveRecord(restored, wire.data());
        restored.mDead = true;
        restored.mTrackInstances = nullptr; // The record reader does not own source instance storage.
        restored.RebaseTime(clock.mVisualTimeNs + 987'654'321);
        const auto actual = restored.SamplePlayback();
        Require(actual.phaseNs == expected.phaseNs && actual.cycles == expected.cycles &&
            restored.mCurrentFrame == paused.mCurrentFrame,
            "Paused frame cursor changed when saving ahead of its last Consume");
        std::cout << "PASS paused shared-clock frame cursor save/rebase without age change\n";
    }
    Reanimation held;
    initialize(held, ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD, 12.0f);
    held.Advance(90'000'000'000);
    Require(held.SamplePlayback().phaseNs == held.mPeriodNs && held.mLoopCount == 1 && !held.mDead,
        "Production hold failed to clamp at end");
    Reanimation synchronized;
    initialize(synchronized, ReanimLoopType::REANIM_LOOP, 6.0f);
    synchronized.SynchronizePhase(held);
    Require(synchronized.SamplePlayback().phaseNs == synchronized.mPeriodNs,
        "Production synchronized phase lost the held final frame");
    Reanimation mode;
    initialize(mode, ReanimLoopType::REANIM_LOOP, 13.3072289f);
    mode.Advance(mode.mPeriodNs * 17 + mode.mPeriodNs / 3);
    const auto original = mode.SamplePlayback();
    const auto generation = mode.mPlaybackGeneration;
    mode.ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
    Require(mode.SamplePlayback().phaseNs == original.phaseNs && mode.SamplePlayback().cycles == 0 &&
        mode.mLoopCount == 0 && !mode.mHasLastFrame && mode.mPlaybackGeneration == generation + 1,
        "Production mode change retained historical cycles or reset phase");
    mode.Advance(17);
    const auto startTime = mode.mStartTimeNs;
    mode.ChangeLoopType(ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD);
    Require(mode.mStartTimeNs == startTime && mode.mPlaybackGeneration == generation + 1,
        "Unchanged production mode rebased playback");
    mode.Advance(mode.mPeriodNs);
    Require(mode.SamplePlayback().phaseNs == mode.mPeriodNs && mode.mLoopCount == 1,
        "Production mode change did not finish from preserved phase");
    mode.ChangeLoopType(ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME);
    Require(mode.SamplePlayback().phaseNs == mode.mPeriodNs,
        "Production mode change discarded held endpoint");
    mode.Advance(1);
    Require(mode.SamplePlayback().phaseNs == 1 && mode.mLoopCount == 1,
        "Production held endpoint did not wrap in the new loop mode");
    // Owners prepare animation time before action queries and mutations, then
    // UpdateAt performs track work without discarding that consumed interval.
    // Preparing a later time must not include the preceding update's interval.
    ReanimationHolder clocks;
    for (const auto domain : {ReanimTimeDomain::Battle, ReanimTimeDomain::Visual})
    for (const float rate : {13.3072289f, -13.3072289f})
    {
        Reanimation events;
        events.mReanimationHolder = &clocks;
        auto& now = domain == ReanimTimeDomain::Battle ? clocks.mBattleTimeNs : clocks.mVisualTimeNs;
        now = 123'456'789'000;
        initialize(events, ReanimLoopType::REANIM_LOOP, rate, now);
        events.Seek(rate > 0 ? 0 : 800);
        const I origin = now, period = events.mPeriodNs;
        const I threshold = rate > 0 ? 400 : 700;
        now = origin + period / 2;
        events.UpdateAt(now);
        Require(events.TimedEventCount(threshold, 1000) == (rate > 0 ? 1 : -1),
            "Production explicit-time event setup did not cross its threshold");
        now = origin + period * 3 / 5;
        events.PrepareTimeAt(now);
        Require(events.TimedEventCount(threshold, 1000) == 0 &&
            !events.ShouldTriggerTimedEvent(static_cast<float>(threshold) / 1000),
            "Production event query after PrepareTimeAt repeated the preceding interval");
        events.UpdateAt(now);
        Require(events.TimedEventCount(threshold, 1000) == 0,
            "Production event interval changed after UpdateAt completed prepared track work");

        events.ApplyRate(13.3072289f);
        events.Seek(0);
        const I rateStart = now, originalPeriod = events.mPeriodNs;
        now = rateStart + originalPeriod / 2;
        events.PrepareTimeAt(now);
        events.ApplyRate(6.65361445f);
        Require(events.TimedEventCount(2, 5) == 1,
            "Production rate change discarded the consumed event interval");
        events.UpdateAt(now);
        Require(events.TimedEventCount(2, 5) == 1,
            "Production same-time UpdateAt discarded the rate-change event interval");
        now += events.mPeriodNs / 10;
        events.PrepareTimeAt(now);
        Require(events.TimedEventCount(2, 5) == 0,
            "Production prepared event query repeated a rate-change interval");
        events.UpdateAt(now);
        events.Seek(0);
        now += events.mPeriodNs / 2;
        events.PrepareTimeAt(now);
        events.ApplyRate(0);
        Require(events.TimedEventCount(2, 5) == 1,
            "Production pause discarded the consumed event interval");
        events.UpdateAt(now);
        Require(events.TimedEventCount(2, 5) == 1,
            "Production same-time pause update discarded its event interval");
        now += events.mPeriodNs;
        events.PrepareTimeAt(now);
        Require(events.TimedEventCount(2, 5) == 0,
            "Production prepared paused query repeated the pause event interval");
        events.UpdateAt(now);
        Require(events.TimedEventCount(2, 5) == 0,
            "Production paused update repeated the pause event interval");
    }
    std::cout << "PASS production reanimation integer playback, original-speed roots, clipped roots, "
        "rate continuity, hold, synchronization, and partition-independent events\n";
}

void PrepareResources(TestApp& app)
{
    Require(app.mResourceManager->ParseResourcesFile("compiled/resources.compiled"),
        "Cannot parse real resource plans for the generated font bindings");
}

void Lifecycle(TestApp& app, const StaticSnapshot& snapshot, int reanimationIndex, int particleIndex)
{
    PrepareResources(app);
    std::cerr << "PASS lifecycle resource plans\n";
    TextureRegion sharedSample;
    for (int repeat = 0; repeat < 2; ++repeat)
    {
        LoadAll(app);
        std::cerr << "PASS lifecycle definitions loaded: repeat " << repeat << '\n';
        ParticleRenderParamsChecks();
        std::cerr << "PASS lifecycle authored particle render parameters\n";
        CurveTiming();
        std::cerr << "PASS lifecycle curve timing\n";
        ReanimationIntegerPlayback();
        std::cerr << "PASS lifecycle reanimation playback\n";
        // Actor checks consume then release the one-shot resource plans needed
        // by the real Board constructor. Their own schedules cover repeated
        // actors; the outer loop separately exercises definition reloads.
        if (repeat == 0)
        {
            ActorFloatRuntime::Check(app);
            std::cerr << "PASS lifecycle actor coordinates\n";
        }
        RealConsumers(reanimationIndex, particleIndex);
        std::cerr << "PASS lifecycle real consumers\n";
        snapshot.Check();
        auto& reanimation = Definitions::Reanimations[reanimationIndex];
        auto& particle = Definitions::Particles[particleIndex];
        Require(reanimation.mTextures[0] == particle.mTextures[0],
            "Configured cross-kind atlas was not shared");
        const auto pixels = Sample(app, particle.mTextures[0], particle);
        const auto sampleRectangle = particle.mImageRects[0];
        sharedSample = sampleRectangle;
        const auto projectile = Definitions::Projectiles.mTexture;
        TodParticleFreeDefinitions();
        CheckParticleTextures(false);
        CheckReanimationTextures(true);
        Require(Definitions::Projectiles.mTexture == projectile &&
            Sample(app, reanimation.mTextures[0], sampleRectangle) == pixels,
            "Particle-only release invalidated another definition kind");
        TodParticleLoadDefinitions(gLawnParticleArray, ParticleEffect::NUM_PARTICLES);
        Require(particle.mTextures[0] == reanimation.mTextures[0],
            "Particle reload did not reuse its still-live shared atlas");
        ReanimatorFreeDefinitions();
        CheckReanimationTextures(false);
        CheckParticleTextures(true);
        Require(!gReanimatorDefArray && FontBindingCount() == 0 &&
            Definitions::Projectiles.mTexture == projectile &&
            Sample(app, particle.mTextures[0], particle) == pixels,
            "Reanimation-only release invalidated particle/projectile data");
        ReanimatorLoadDefinitions(gLawnReanimationArray, static_cast<int>(ReanimationType::NUM_REANIMS));
        ReanimatorLoadAllDefinitionData();
        Definitions::ReleaseProjectileImages();
        Require(!Definitions::Projectiles.mTexture, "Projectile-only release retained its slot");
        CheckParticleTextures(true);
        CheckReanimationTextures(true);
        Require(Sample(app, particle.mTextures[0], particle) == pixels,
            "Projectile-only release damaged a shared definition atlas");
        Definitions::LoadProjectileImages();
        Require(Definitions::Projectiles.mTexture == projectile,
            "Projectile reload replaced a still-cached atlas");
        snapshot.Check();
        ReleaseAll(app);
        snapshot.Check();
    }
    // Worker loads create pending GPU assignments. Releasing one kind must
    // cancel its assignments without suppressing a surviving kind's upload.
    ReanimatorLoadDefinitions(gLawnReanimationArray, static_cast<int>(ReanimationType::NUM_REANIMS));
    std::exception_ptr workerError;
    std::thread worker([&] {
        try
        {
            Definitions::LoadReanimationImages(reanimationIndex);
            TodParticleLoadDefinitions(gLawnParticleArray, ParticleEffect::NUM_PARTICLES);
            Definitions::LoadProjectileImages();
        }
        catch (...) { workerError = std::current_exception(); }
    });
    worker.join();
    if (workerError) std::rethrow_exception(workerError);
    CheckParticleTextures(false);
    CheckReanimationTextures(false);
    Require(!Definitions::Projectiles.mTexture, "Worker thread directly uploaded a GPU atlas");
    TodParticleFreeDefinitions();
    Definitions::ReleaseProjectileImages();
    Require(DefinitionUploadAtlasImages() > 0, "Worker atlas decode queued no primary uploads");
    CheckParticleTextures(false);
    Require(!Definitions::Projectiles.mTexture, "Cancelled projectile assignment was republished");
    auto& reanimation = Definitions::Reanimations[reanimationIndex];
    CheckTextures(reanimation, true);
    Sample(app, reanimation.mTextures[0], sharedSample);
    TodParticleLoadDefinitions(gLawnParticleArray, ParticleEffect::NUM_PARTICLES);
    CheckParticleTextures(true);
    Require(Definitions::Particles[particleIndex].mTextures[0] == reanimation.mTextures[0],
        "Reload after cancelling a pending assignment lost the shared atlas");
    snapshot.Check();
    ReleaseAll(app);
    snapshot.Check();
    std::cout << "PASS full 144/106 catalogs: stable roots/tracks/fields/nodes/rectangles/actions, "
        "two release/reload cycles, repeated bulk loads, 193 shared GPU atlases, real consumers, "
        "independent kind release and pixels, pending worker assignment cancellation\n";
}

void PngFailure(TestApp& app, const StaticSnapshot& snapshot, const std::string& caseName,
    const char* goodPak, const std::string& badPath, int reanimationIndex, int particleIndex)
{
    auto expectFailure = [&](auto&& load) {
        bool rejected = false;
        try { load(); }
        catch (const std::runtime_error& error)
        {
            rejected = true;
            Require(std::string(error.what()).find(badPath) != std::string::npos,
                "PNG failure did not identify the failing direct asset path");
            std::cout << "Expected " << caseName << " rejection: " << error.what() << '\n';
        }
        Require(rejected, "Invalid PNG was accepted");
        snapshot.Check();
    };
    for (int repeat = 0; repeat < 2; ++repeat)
    {
        expectFailure([] { TodParticleLoadDefinitions(gLawnParticleArray, ParticleEffect::NUM_PARTICLES); });
        Require(!gParticleDefArray && !gParticleDefCount && !gParticleAllDefinitionDataLoaded.load(),
            "Failed particle PNG load left the catalog published");
        CheckParticleTextures(false);
    }
    ReanimatorLoadDefinitions(gLawnReanimationArray, static_cast<int>(ReanimationType::NUM_REANIMS));
    expectFailure([&] {
        ReanimatorEnsureDefinitionLoaded(static_cast<ReanimationType>(reanimationIndex), true);
    });
    ReleaseAll(app);
    snapshot.Check();
    // The fixture owns this in-process mount table. Drop its records only after
    // every definition binding has been released; neither archive is modified.
    gPakInterface->mPakRecordMap.clear();
    Require(gPakInterface->AddPakFile(goodPak), "Cannot mount the intact package for recovery");
    PrepareResources(app);
    LoadAll(app);
    RealConsumers(reanimationIndex, particleIndex);
    Sample(app, Definitions::Particles[particleIndex].mTextures[0],
        Definitions::Particles[particleIndex]);
    snapshot.Check();
    ReleaseAll(app);
    snapshot.Check();
    std::cout << "PASS " << caseName
        << ": repeated particle rollback, lazy reanimation rejection, clean release, "
        "same-process intact-PNG recovery and full catalog reload\n";
}
}

int main(int argc, char** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    AddVectoredExceptionHandler(1, SuppressCrashDialogs);
    try
    {
        Require(argc == 7, "Pass fixture ZIP, case, intact PAK, shared reanimation/particle IDs, PNG path");
        HiddenWindow window;
        TestApp app;
        gLawnApp = &app;
        CheckActualScheduler(app);
        app.mPrimaryThreadId = CurrentNativeThreadIdentity();
        app.mScaleHDREnabled = false;
        app.mReadFromRegistry = false;
        app.mWidth = 800;
        app.mHeight = 600;
#if defined(PVZ_GPU_OPENGL) || defined(PVZ_GPU_D3D9) || defined(PVZ_GPU_D3D12)
        app.mGpuContext = std::make_unique<GpuContext>(window.handle, false, false);
#else
        VulkanInitConfig init{};
        init.appInfo.pApplicationName = "Static definition runtime tests";
        init.features.shaderFloat64 = VK_TRUE;
        VkSwapchainCreateInfoKHR swapchain{
            .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .imageFormat = VK_FORMAT_B8G8R8A8_UNORM,
            .imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode = VK_PRESENT_MODE_FIFO_KHR,
            .clipped = VK_TRUE};
        app.mGpuContext = std::make_unique<GpuContext>(init, window.handle, swapchain);
#endif
        app.mDDInterface = new DDInterface(&app);
        Require(app.mDDInterface->Init(window.handle, true) == DDInterface::RESULT_OK,
            "Cannot initialize the real hidden GPU renderer");
        struct Cleanup
        {
            TestApp& app;
            ~Cleanup()
            {
                ReleaseAll(app);
                app.mResourceManager->DeleteResources();
                app.CleanSharedImages();
                FreeGlobalAllocators();
            }
        } cleanup{app};
        Require(gPakInterface && gPakInterface->AddPakFile(argv[1]), "Cannot mount isolated test package");
        const int reanimationIndex = std::stoi(argv[4]), particleIndex = std::stoi(argv[5]);
        Require(reanimationIndex >= 0 && reanimationIndex < Definitions::ReanimationCount &&
            particleIndex >= 0 && particleIndex < Definitions::ParticleCount,
            "Shared atlas fixture indices are out of range");
        StaticSnapshot snapshot;
        if (std::string(argv[2]) == "lifecycle")
            Lifecycle(app, snapshot, reanimationIndex, particleIndex);
        else
            PngFailure(app, snapshot, argv[2], argv[3], argv[6], reanimationIndex, particleIndex);
    }
    catch (const ResourceManagerException& error)
    {
        std::cerr << "FAIL static definition resource: reason=" << int(error.error.reason)
            << " record=" << error.error.record << " value=" << error.error.value
            << " offset=" << error.error.offset << '\n';
        return 1;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL static definition runtime: " << error.what() << '\n';
        return 1;
    }
}
