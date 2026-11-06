#include "graphics/Graphics.h"
#include "TodDebug.h"
#include "Attachment.h"
#include "Reanimator.h"
#include "TodParticle.h"
#include "EffectSystem.h"
#include "GameConstants.h"
#include "graphics/Image.h"
#include "graphics/DDInterface.h"
#include "graphics/RenderInterface.h"
#include <memory>
#include <stdexcept>

EffectSystem* gEffectSystem = nullptr;  //[0x6A9EB8]

EffectSystem::~EffectSystem()
{
	EffectSystemDispose();
}

//0x445330
void EffectSystem::EffectSystemInitialize()
{
	if (gEffectSystem || mParticleHolder || mReanimationHolder || mAttachmentHolder)
		throw std::logic_error("EffectSystem: initialization requires an empty system and no active global system");
	auto aParticleData = std::make_unique<TodParticleData>();
	aParticleData->Initialize();
	auto aParticles = std::make_unique<TodParticleHolder>(aParticleData.get());
	auto aReanimations = std::make_unique<ReanimationHolder>();
	auto aAttachments = std::make_unique<AttachmentHolder>();
	aReanimations->InitializeHolder();
	aAttachments->InitializeHolder();

	mParticleData = std::move(aParticleData);
	mParticleHolder = std::move(aParticles);
	mReanimationHolder = std::move(aReanimations);
	mAttachmentHolder = std::move(aAttachments);
	gEffectSystem = this; // Publish only after every pool is ready.
}

//0x445490
void EffectSystem::EffectSystemDispose()
{
	// Particle destructors resolve through their still-live borrowed holder.
	mParticleData.reset();
	mParticleHolder.reset();
	mReanimationHolder.reset();
	mAttachmentHolder.reset();

	if (gEffectSystem == this)
		gEffectSystem = nullptr;
}

//0x4455E0
void EffectSystem::EffectSystemFreeAll()
{
	mParticleHolder->mParticleSystems.DataArrayFreeAll();
	mParticleHolder->mEmitters.DataArrayFreeAll();
	mParticleHolder->mParticles.DataArrayFreeAll();

	mReanimationHolder->mReanimations.DataArrayFreeAll();
	mAttachmentHolder->mAttachments.DataArrayFreeAll();
}

//0x445680
void EffectSystem::ProcessDeleteQueue()
{
	TodParticleSystem* aParticle = nullptr;
	while (mParticleHolder->mParticleSystems.IterateNext(aParticle))
		if (aParticle->mDead)
		{
			// 2026-10-05 泄漏修复（B 树无尽实机定罪：发射器池爬满 1024 拒生）：
			// 自然死亡原先只释系统槽、发射器滞留池内（长局必满）。补完整 Die
			// 序列（逐发射器 DeleteAll+释槽→清册），与显式 ParticleSystemDie 同构
			aParticle->ParticleSystemDie();
			mParticleHolder->mParticleSystems.DataArrayFree(aParticle);
		}

	Reanimation* aReanim = nullptr;
	while (mReanimationHolder->mReanimations.IterateNext(aReanim))
		if (aReanim->mDead)
			mReanimationHolder->mReanimations.DataArrayFree(aReanim);

	Attachment* aAttachment = nullptr;
	while (mAttachmentHolder->mAttachments.IterateNext(aAttachment))
		if (aAttachment->mDead)
			mAttachmentHolder->mAttachments.DataArrayFree(aAttachment);
}

//0x445890
void EffectSystem::UpdateAt(std::int64_t visualNowNs, std::int64_t battleNowNs)
{
    UpdateAt(visualNowNs, battleNowNs, battleNowNs);
}

void EffectSystem::UpdateAt(std::int64_t visualNowNs, std::int64_t battleNowNs, std::int64_t poolNowNs)
{
	TodParticleSystem* aParticle = nullptr;
	while (mParticleHolder->mParticleSystems.IterateNext(aParticle))
		if (!aParticle->mIsAttachment)
		{
            // The owner supplies each effect's current timestamp. First-intro
            // pool sparkles may advance while the rest of the Board is frozen.
            const auto nowNs = aParticle->mEffectType == PARTICLE_POOL_SPARKLY ? poolNowNs : battleNowNs;
			const auto deltaNs = nowNs > aParticle->mLastUpdateTimeNs
				? static_cast<std::uint64_t>(nowNs - aParticle->mLastUpdateTimeNs) : 0;
			aParticle->Update(deltaNs, nowNs);
		}

	Reanimation* aReanim = nullptr;
	while (mReanimationHolder->mReanimations.IterateNext(aReanim))
		if (!aReanim->mIsAttachment)
			aReanim->UpdateAt(mReanimationHolder->mTimeDomains[mReanimationHolder->mReanimations.DataArrayIndexOf(aReanim)]
				== ReanimTimeDomain::Battle ? battleNowNs : visualNowNs);
}

namespace
{
void RebaseAnimationTree(EffectSystem& effects, Reanimation& animation, std::int64_t nowNs)
{
    animation.RebaseTime(nowNs);
    if (!animation.mDefinition || !animation.mTrackInstances) return;
    for (int i = 0; i < animation.mDefinition->mTracks.count; ++i)
        if (auto* attachment = effects.mAttachmentHolder->mAttachments.DataArrayTryToGet(
                static_cast<unsigned int>(animation.mTrackInstances[i].mAttachmentID)))
            for (int j = 0; j < attachment->mNumEffects; ++j)
            {
                const auto& child = attachment->mEffectArray[j];
                if (child.mEffectType == EFFECT_REANIM)
                {
                    if (auto* reanim = effects.mReanimationHolder->mReanimations.DataArrayTryToGet(child.mEffectID))
                        RebaseAnimationTree(effects, *reanim, nowNs);
                }
                else if (child.mEffectType == EFFECT_PARTICLE)
                    if (auto* particle = effects.mParticleHolder->mParticleSystems.DataArrayTryToGet(child.mEffectID))
                        particle->Update(0, nowNs);
            }
}
}

void EffectSystem::RebaseIndependentBattleEffects(std::int64_t battleNowNs)
{
    // An actor-only extra pass must not age independently updated effects.
    // This traversal is used only by the speed minigame; ordinary updates
    // keep the same hot loop and the same per-effect branches.
    TodParticleSystem* particle = nullptr;
    while (mParticleHolder->mParticleSystems.IterateNext(particle))
        if (!particle->mIsAttachment)
            particle->Update(0, battleNowNs);
    Reanimation* animation = nullptr;
    while (mReanimationHolder->mReanimations.IterateNext(animation))
        if (!animation->mIsAttachment && mReanimationHolder->mTimeDomains[
                mReanimationHolder->mReanimations.DataArrayIndexOf(animation)] == ReanimTimeDomain::Battle)
            RebaseAnimationTree(*this, *animation, battleNowNs);
}

// #################################################################################################### //
// #################################################################################################### //
// #################################################################################################### //

static int FixedFloor(int x)
{
	if (x > 0)
		return x & 0xFFFF0000;
	else
		return (x & 0xFFFF0000) - 0x10000;
}

bool gTodTriangleDrawAdditive = false;

#include "TodDrawTriangleInc.cpp"

//0x4461B0
TodTriangleGroup::TodTriangleGroup()
{
	mDrawMode = Graphics::DRAWMODE_NORMAL;
}

//0x4461F0
void TodTriangleGroup::DrawGroup(Graphics* g)
{
    // The legacy full-board rectangle means no local clip, including when the
    // drawable canvas is wider than the original board.
    const Rect* clip = mClipRect == Rect(0, 0, BOARD_WIDTH, BOARD_HEIGHT) ? nullptr : &mClipRect;
#if defined(PVZ_GPU_VULKAN) || defined(PVZ_GPU_D3D12) || (defined(PVZ_GPU_OPENGL) || defined(PVZ_GPU_WEBGL) || defined(PVZ_GPU_WEBGPU))
    if (mTexture.mTexture && !mQuadArray.empty()) {
        if (!g->mDestImage || !g->mDestImage->IsGpuDrawTarget())
            throw NonScreenDrawError(DrawOperation::BltTrianglesTex);
        auto& render=*g->mDestImage->mRenderOwner;
        render.SelectDrawSurface(g->mDestImage);
        render.DrawQuadsTex(mQuadArray,Color::White,mDrawMode,mTexture,0,0,g->mLinearBlend,clip);
        mQuadArray.clear();
        gTodTriangleDrawAdditive=false;
    }
#endif

	if (mTexture.mTexture && !mVertArray.empty())
	{
		if (!g->mDestImage || !g->mDestImage->IsGpuDrawTarget())
			throw NonScreenDrawError(DrawOperation::BltTrianglesTex);
		auto* image=g->mDestImage;
		auto& render=*image->mRenderOwner;
		render.SelectDrawSurface(image);
		render.DrawTrianglesTex(mVertArray,Color::White,mDrawMode,mTexture,0.0f,0.0f,g->mLinearBlend,clip);
		mVertArray.clear();
		gTodTriangleDrawAdditive = false;
	}
}

//0x446300
void TodTriangleGroup::AddTriangle(Graphics* g, Image* theImage, const SexyMatrix3& theMatrix, const Rect& theClipRect, const Color& theColor, int theDrawMode, const Rect& theSrcRect)
{
	if (theClipRect.mWidth <= 0 || theClipRect.mHeight <= 0) return;
	TOD_ASSERT(theImage != nullptr);
	if (!g->mDestImage || !g->mDestImage->IsGpuDrawTarget())
		throw NonScreenDrawError(DrawOperation::BltTrianglesTex);
	auto& render=*g->mDestImage->mRenderOwner;
	auto* gpuTexture=render.CreateImageTexture(theImage);
	if (!gpuTexture) return;
	const TextureSource texture{gpuTexture, theImage->mGpuSamplingFlags};
	const TextureRegion source{
		float(theSrcRect.mX) / theImage->mWidth,
		float(theSrcRect.mY) / theImage->mHeight,
		float(theSrcRect.mX + theSrcRect.mWidth) / theImage->mWidth,
		float(theSrcRect.mY + theSrcRect.mHeight) / theImage->mHeight,
		theSrcRect.mWidth, theSrcRect.mHeight};
	AddTriangle(g, texture, theMatrix, theClipRect, theColor, theDrawMode, source);
}

void TodTriangleGroup::AddTriangle(Graphics* g, const TextureSource& texture, const SexyMatrix3& theMatrix, const Rect& theClipRect, const Color& theColor, int theDrawMode, const TextureRegion& theSrcRect)
{
	if (theClipRect.mWidth <= 0 || theClipRect.mHeight <= 0) return;
	TOD_ASSERT(texture.mTexture != nullptr);

	if ((!mVertArray.empty()
#if defined(PVZ_GPU_VULKAN) || defined(PVZ_GPU_D3D12) || (defined(PVZ_GPU_OPENGL) || defined(PVZ_GPU_WEBGL) || defined(PVZ_GPU_WEBGPU))
        || !mQuadArray.empty()
#endif
    ) && (mDrawMode != theDrawMode || !(mClipRect == theClipRect) ||
		mTexture.mTexture != texture.mTexture || mTexture.mSamplingFlags != texture.mSamplingFlags))
		DrawGroup(g);
	mTexture = texture;
	mDrawMode = theDrawMode;
	mClipRect = theClipRect;

	SexyVector2 p[4];
	float x = -theSrcRect.mWidth * 0.5f;
	float y = -theSrcRect.mHeight * 0.5f;
	p[0].x = x;
	p[0].y = y;
	p[1].x = x;
	p[1].y = y + theSrcRect.mHeight;
	p[2].x = x + theSrcRect.mWidth;
	p[2].y = y;
	p[3].x = x + theSrcRect.mWidth;
	p[3].y = y + theSrcRect.mHeight;
	SexyVector2 tp[4];
	// Native GPU coordinates use pixel corners, matching TextureSource::BltTransformed.
	for (int i = 0; i < 4; i++)
	{
		tp[i] = theMatrix * p[i];
	}

	TOD_ASSERT(theSrcRect.mWidth >= 0 && theSrcRect.mHeight >= 0);
	TOD_ASSERT(theSrcRect.mU0 >= 0.0f && theSrcRect.mU0 <= theSrcRect.mU1 && theSrcRect.mU1 <= 1.0f);
	TOD_ASSERT(theSrcRect.mV0 >= 0.0f && theSrcRect.mV0 <= theSrcRect.mV1 && theSrcRect.mV1 <= 1.0f);

	float aTexX1 = theSrcRect.mU0;
	float aTexY1 = theSrcRect.mV0;
	float aTexX2 = theSrcRect.mU1;
	float aTexY2 = theSrcRect.mV1;
	unsigned int aTexColor = theColor.ToInt();

	// Keep uncut topology and pixel/UV coordinates. The batch's local clip is
	// applied by GPU scissor; changing it flushes the preceding batch in order.
#if defined(PVZ_GPU_VULKAN) || defined(PVZ_GPU_D3D12) || (defined(PVZ_GPU_OPENGL) || defined(PVZ_GPU_WEBGL) || defined(PVZ_GPU_WEBGPU))
	if (!mVertArray.empty()) DrawGroup(g);
	mQuadArray.push_back({TriVertex{tp[0].x,tp[0].y,aTexX1,aTexY1,aTexColor},
		TriVertex{tp[1].x,tp[1].y,aTexX1,aTexY2,aTexColor},
		TriVertex{tp[2].x,tp[2].y,aTexX2,aTexY1,aTexColor},
		TriVertex{tp[3].x,tp[3].y,aTexX2,aTexY2,aTexColor}});
#else
	mVertArray.push_back({TriVertex{tp[0].x,tp[0].y,aTexX1,aTexY1,aTexColor},
		TriVertex{tp[1].x,tp[1].y,aTexX1,aTexY2,aTexColor},
		TriVertex{tp[2].x,tp[2].y,aTexX2,aTexY1,aTexColor}});
	mVertArray.push_back({TriVertex{tp[2].x,tp[2].y,aTexX2,aTexY1,aTexColor},
		TriVertex{tp[1].x,tp[1].y,aTexX1,aTexY2,aTexColor},
		TriVertex{tp[3].x,tp[3].y,aTexX2,aTexY2,aTexColor}});
#endif
}
