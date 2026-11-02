#include "RenderInterface.h"
#include "DDInterface.h"
#include "Graphics.h"
#include "misc/AutoCrit.h"
#include "misc/SexyMatrix.h"
#include "SexyAppBase.h"
#include "TriVertex.h"
#include "imagelib/ImageLib.h"
#include <assert.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

#pragma warning(disable:4244)

using namespace Sexy;
static uint32_t gGpuSourceSamplingFlags = 0;
namespace {
class TextureSamplingScope {
	uint32_t mPrevious;
public:
	explicit TextureSamplingScope(uint32_t flags) : mPrevious(gGpuSourceSamplingFlags) { gGpuSourceSamplingFlags=flags; }
	~TextureSamplingScope() { gGpuSourceSamplingFlags=mPrevious; }
};
uint32_t CombinedImageSamplingFlags()
{
	const auto overrideFlags=RenderInterface::sImageSamplingFlags;
	const auto sourceFlags=gGpuSourceSamplingFlags;
	// Color filter bits encode one enum, not independent feature flags.
	return (sourceFlags & ((overrideFlags & 0x30000000u) ? ~0x30000000u : ~0u)) |
		overrideFlags;
}
}

static bool gLinearFilter = false;
static GpuRenderer* gGpuRenderer = NULL;
static int gGpuCurrentDrawMode = 0;
static int gGpuSceneWidth = 800;
static int gGpuSceneHeight = 600;

// Framework-local primitive topology, expanded to native Vulkan triangle lists.
enum PrimitiveType
{
	PRIMITIVE_TRIANGLELIST = 0,
	PRIMITIVE_TRIANGLESTRIP = 1,
	PRIMITIVE_TRIANGLEFAN = 2
};

namespace
{
struct ScreenVertex
{
	float sx, sy, sz, rhw;
	DWORD color;
	float tu, tv;
};

#define RGBA_MAKE(r, g, b, a) (((DWORD)(a) << 24) | ((DWORD)(r) << 16) | ((DWORD)(g) << 8) | (DWORD)(b))

}  // namespace

static void RequireImageDimensions(const GpuRenderer* renderer, int width, int height)
{
	if (!renderer)
		throw std::logic_error("Image creation requires a native Vulkan renderer");
	const uint32_t limit = renderer->MaxImageDimension2D();
	if (width <= 0 || height <= 0 || uint32_t(width) > limit || uint32_t(height) > limit)
		throw std::invalid_argument("Image dimensions exceed the Vulkan device limit");
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RenderInterface::RenderInterface()
{
	mHWnd = NULL;
	mWidth = 640;
	mHeight = 480;
	mRenderer = NULL;
	mPresentWidth = 0;
	mPresentHeight = 0;
	mSceneBegun = false;
	mIsWindowed = true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
RenderInterface::~RenderInterface()
{
	Cleanup();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool RenderInterface::InitGPU()
{
	mPresentWidth = mPresentWidth > 0 ? mPresentWidth : mWidth;
	mPresentHeight = mPresentHeight > 0 ? mPresentHeight : mHeight;

	gLinearFilter = false;
	RenderInterface::sSamplerAddressWrap = false;

	const std::string shaderDir = AddTrailingSlash(GetExecutableDirectory(), true) + "shaders/";
	const GpuHdrParams hdr{gSexyAppBase->mScaleHDRPaperWhite,gSexyAppBase->mScaleHDRMaxNits,
        gSexyAppBase->mScaleHDRReinhardPaperNits,gSexyAppBase->mScaleHDRStrength};
    mRenderer = GpuBackendDetail::CreateRenderer<BuildConfig::Graphics, GpuRenderer>(
        *gSexyAppBase->mGpuContext, mWidth, mHeight,
        shaderDir, gSexyAppBase->mScaleHDREnabled ? &hdr : nullptr);
	mRenderer->PrepareLoadingPipelines();

	gGpuRenderer = mRenderer;
	gGpuSceneWidth = mWidth;
	gGpuSceneHeight = mHeight;
	return true;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool RenderInterface::InitFromDDInterface(DDInterface *theInterface)
{
	mHWnd = theInterface->mHWnd;
	mWidth = theInterface->mWidth;
	mHeight = theInterface->mHeight;
	mPresentWidth = theInterface->mDisplayWidth;
	mPresentHeight = theInterface->mDisplayHeight;
	mIsWindowed = theInterface->mIsWindowed;

	return InitGPU();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool RenderInterface::ResizePresent(int thePresentWidth, int thePresentHeight)
{
    if (mRenderer == nullptr) throw std::logic_error("Resize without a native Vulkan renderer");
    if (mSceneBegun) Flush();
    if (!mRenderer->ResizePresent()) return false;
    mPresentWidth = thePresentWidth;
    mPresentHeight = thePresentHeight;
    return true;
}

void RenderInterface::ResizeScene(int width, int height)
{
    if (mSceneBegun) Flush();
    mRenderer->ResizeScene(width, height);
    mWidth = gGpuSceneWidth = width;
    mHeight = gGpuSceneHeight = height;
}

bool RenderInterface::PresentRetainedFrame()
{
    if (!mRenderer || mSceneBegun)
        throw NativeDisplayError(NativeDisplayOperation::PresentRetainedFrame,DDInterface::RESULT_FAIL);
    if (gSexyAppBase->mPhysMinimized) return false;
    // No first scene yet, iconified surface, and an out-of-date acquire are
    // temporary deferrals. Never latch them as permanent renderer failures.
    return mRenderer->PresentRetainedScene();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool RenderInterface::PreDraw()
{
    if (mDrawSurface == nullptr && mScreenDrawDeferred)
        return false;
	if (gSexyAppBase->mPhysMinimized && mDrawSurface == nullptr)
		return false;

	if (mRenderer == NULL)
		throw std::logic_error("Drawing without a native Vulkan renderer");

	if (!mSceneBegun)
	{
        if (!mRenderer->BeginScene())
        {
            if (mDrawSurface == nullptr) mScreenDrawDeferred = true;
            return false;
        }
		mSceneBegun = true;
	}
	return true;
}

Image* RenderInterface::CreateRenderImage(int width, int height)
{
	RequireImageDimensions(mRenderer, width, height);
	auto* image = new Image(gSexyAppBase);
	try
	{
	image->mRenderOwner=this;
	image->mWidth = width;
	image->mHeight = height;
	image->mHasTrans = true;
	image->mHasAlpha = true;
	image->mForcedMode = true;
	image->mRenderTarget = true;
	image->mTexture = mRenderer->CreateRenderImage(width,height);
	if (!image->mTexture)
		throw std::runtime_error("Render image allocation returned no GPU texture");
	mImageSet.insert(image);
	image->mRenderOwner = this;
	return image;
	}
	catch (...) { delete image; throw; }
}

Image* RenderInterface::ProcessImage(Image* source,const GpuImageProcessing& processing)
{
	if(CurrentNativeThreadIdentity()!=gSexyAppBase->mPrimaryThreadId || !source)
		throw GpuImageCpuAccess{};
	RequireImageDimensions(mRenderer, processing.width, processing.height);
	if (processing.x<0 || processing.y<0 || processing.width>source->mWidth ||
		processing.height>source->mHeight || processing.x>source->mWidth-processing.width ||
		processing.y>source->mHeight-processing.height)
		throw std::invalid_argument("Image processing rectangle exceeds logical image");
	auto* original=CreateImageTexture(source);
	if(!original) throw GpuImageCpuAccess{};
	auto* image=new Image(gSexyAppBase);
	try
	{
	image->mRenderOwner=this;
	image->mWidth=processing.width;image->mHeight=processing.height;
	image->mHasTrans=true;image->mHasAlpha=true;image->mForcedMode=true;
	image->mRenderTarget=false;
	image->mTexture=mRenderer->ProcessImage(original,processing);
	if (!image->mTexture)
		throw std::runtime_error("Image processing returned no GPU texture");
	mImageSet.insert(image);
	image->mRenderOwner=this;
	return image;
	}
	catch (...) { delete image; throw; }
}

void RenderInterface::RenderFontAtlas(Image* target,Image* source,
	std::span<const GpuFontGlyph> glyphs,bool forceWhite)
{
	auto* sourceTexture=CreateImageTexture(source);
	if(!sourceTexture || !target || !target->mTexture) throw GpuImageCpuAccess{};
	GpuImageDrawScope drawing(*this,target);
	if (!PreDraw()) throw std::exception{};
	mRenderer->RenderFontAtlas(target->mTexture,sourceTexture,
		glyphs,source->mGpuSamplingFlags,forceWhite);
}

void RenderInterface::SelectDrawSurface(Image* image)
{
	GpuTexture* target = nullptr;
	if (image != nullptr && image != gSexyAppBase->mDDInterface->mScreenImage)
	{
		if (!image->mRenderTarget || !image->mTexture)
			throw std::logic_error("Drawing requires a native GPU image target");
		target = image->mTexture;
	}
	else image = nullptr;
	mRenderer->SetRenderImage(target);
	mDrawSurface = image;
	mSceneBegun = mRenderer->sceneOpen;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
ImagePixels RenderInterface::ReadImagePixels(const Image& image)
{
	if (CurrentNativeThreadIdentity()!=gSexyAppBase->mPrimaryThreadId) throw GpuImageCpuAccess{};
	if (mSceneBegun) throw std::logic_error("Image readback requires Flush before snapshot");
	if (&image==gSexyAppBase->mDDInterface->mScreenImage)
	{
		if (gSexyAppBase->mDDInterface->mRenderInterface!=this)
			throw std::invalid_argument("Screen does not belong to this renderer");
		ImagePixels result(image.mWidth,image.mHeight);
		result.pixels=mRenderer->ReadScenePixels();
		return result;
	}
	auto* texture=CreateImageTexture(const_cast<Image*>(&image));
	if (!texture) throw std::logic_error("Image has no GPU texture");
	auto result=mRenderer->ReadTexturePixels(texture);
	if (result.width!=image.mWidth || result.height!=image.mHeight)
		throw std::logic_error("GPU readback dimensions disagree with source image");
	return result;
}

namespace {
GpuTexture* TrackCompositionScratch(GpuRenderer& renderer,
	std::vector<GpuTexture*>& scratch,GpuTexture* texture)
{
#if defined(PVZ_GPU_OPENGL) || defined(PVZ_GPU_D3D9)
	// These adapters retain raw inputs until successful adoption. If recording
	// the handle fails, it has not entered the caller's cleanup list yet.
	try { scratch.push_back(texture); }
	catch (...) { renderer.DestroyTexture(texture); throw; }
#else
	// Vulkan owns raw handles in its transfer-retirement queue. In particular,
	// failed adoption may already have destroyed the final raw handle.
	(void)renderer;
	(void)scratch;
#endif
	return texture;
}

// 裸图展平：把该图自身的颜色合成与嵌套层全部应用，返回合成结果裸图
// （与批量化前 UploadDecodedImage 递归语义逐位一致，只是不再全局登记）。
GpuTexture* UploadDecodedRaw(GpuRenderer& renderer,const ImageLib::Image& image,
	std::vector<GpuTexture*>& scratch)
{
	RequireImageDimensions(&renderer,image.mWidth,image.mHeight);
	GpuTexture* base=TrackCompositionScratch(renderer,scratch,
		renderer.CreateRawTexture(image.mWidth,image.mHeight,image.mBits,image.mWidth*4));
	if(image.mColorFromAlpha)
		base=TrackCompositionScratch(renderer,scratch,
			renderer.ComposeAlphaRaw(base,base,*image.mColorFromAlpha,true));
	for(const auto* layer:image.mAlphaLayers) {
		GpuTexture* alphaMask=UploadDecodedRaw(renderer,*layer,scratch);
		base=TrackCompositionScratch(renderer,scratch,
			renderer.ComposeAlphaRaw(base,alphaMask,0,false));
	}
	return base;
}

GpuTexture* UploadDecodedImage(GpuRenderer& renderer,const ImageLib::Image& image,bool opaque)
{
	RequireImageDimensions(&renderer,image.mWidth,image.mHeight);
	// 2026-10-11 用户裁决：只有最终可绘制结果进全局纹理表。合成链的底图/
	// 掩码/中间结果一律裸图，调用链内传句柄，批次完成后由渲染器统一销毁。
	const void* bits=image.mBits;
	if(!image.mColorFromAlpha && image.mAlphaLayers.empty())
		return renderer.CreateTexture(image.mWidth,image.mHeight,bits,image.mWidth*4,opaque);
	std::vector<GpuTexture*> scratch;
	try
	{
		GpuTexture* base=TrackCompositionScratch(renderer,scratch,
			renderer.CreateRawTexture(image.mWidth,image.mHeight,bits,image.mWidth*4));
		if(image.mColorFromAlpha)
			base=TrackCompositionScratch(renderer,scratch,
				renderer.ComposeAlphaRaw(base,base,*image.mColorFromAlpha,true));
		for(const auto* layer:image.mAlphaLayers) {
			GpuTexture* alphaMask=UploadDecodedRaw(renderer,*layer,scratch);
			base=TrackCompositionScratch(renderer,scratch,
				renderer.ComposeAlphaRaw(base,alphaMask,0,false));
		}
		// GL/DX9 adoption destroys the other raw handles on success; the
		// non-owning list must never be visited again after this returns.
		return renderer.AdoptComposedTexture(base);
	}
	catch (...)
	{
#if defined(PVZ_GPU_OPENGL) || defined(PVZ_GPU_D3D9)
		for (auto it=scratch.rbegin();it!=scratch.rend();++it)
			renderer.DestroyTexture(*it);
#endif
		throw;
	}
}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static void SubmitNativeTriangles(std::span<const GpuVertex> vertices,
	GpuTexture* texture,bool linearFilter,int drawMode,const Rect* clipRect)
{
	const int mode=drawMode<0?gGpuCurrentDrawMode:drawMode;
	// A deliberately empty clip is a visibility/measurement marker.
	if (clipRect && (clipRect->mWidth <= 0 || clipRect->mHeight <= 0)) return;
	GpuClip nativeClip{};
	if (clipRect) nativeClip={clipRect->mX,clipRect->mY,clipRect->mWidth,clipRect->mHeight};
	const auto blend=mode==1?GpuBlend::Additive:mode==2?GpuBlend::Opaque:GpuBlend::Normal;
	gGpuRenderer->SubmitTriangles(vertices,texture,linearFilter,RenderInterface::sSamplerAddressWrap,
		blend,clipRect ? &nativeClip : nullptr);
}

template<bool Quads = false, class VertexAt>
static void SubmitTextureTriangleBatch(size_t triangleCount,VertexAt vertexAt,
	const TextureSource& data,DWORD color,float tx,float ty,const Rect* clip=nullptr)
{
	if(!triangleCount) return;
	GpuTexture* texture=data.mTexture;
	TextureSamplingScope sampling(data.mSamplingFlags);
	const size_t count=triangleCount*(Quads ? 4 : 3);
	GpuVertex local[192];
	auto* vertices=local;
	if(count>std::size(local)) {
		// SubmitTriangles copies this span into its owned mapped stream before returning.
		static thread_local std::vector<GpuVertex> scratch;
		scratch.resize(count);
		vertices=scratch.data();
	}
	const uint32_t slot=GpuRenderer::SlotOf(texture)|CombinedImageSamplingFlags();
	const uint32_t sampler=(gLinearFilter?1u:0u)|(RenderInterface::sSamplerAddressWrap?2u:0u);
	for(size_t i=0;i<count;++i) {
		const auto& source=vertexAt(i);
		const DWORD tint=source.color?source.color:color;
		vertices[i]={source.x+tx,source.y+ty,
			((tint>>16)&0xff)/255.0f,((tint>>8)&0xff)/255.0f,
			(tint&0xff)/255.0f,((tint>>24)&0xff)/255.0f,
			source.u,source.v,slot,sampler};
	}
#if defined(PVZ_GPU_VULKAN) || defined(PVZ_GPU_D3D12) || defined(PVZ_GPU_OPENGL)
    if constexpr (Quads) {
        const auto blend=gGpuCurrentDrawMode==1 ? GpuBlend::Additive :
            gGpuCurrentDrawMode==2 ? GpuBlend::Opaque : GpuBlend::Normal;
        GpuClip nativeClip{};
        if(clip) nativeClip={clip->mX,clip->mY,clip->mWidth,clip->mHeight};
        gGpuRenderer->SubmitQuads(std::span<const GpuVertex>(vertices,count),texture,
            gLinearFilter,RenderInterface::sSamplerAddressWrap,blend,clip ? &nativeClip : nullptr);
    } else
#endif
	SubmitNativeTriangles(std::span<const GpuVertex>(vertices,count),texture,gLinearFilter,-1,clip);
}

static void SubmitGpuTriangles(
	const ScreenVertex* theVertices,
	int theNumVertices,
	PrimitiveType theType,
	GpuTexture* theSRV,
	bool theLinearFilter,
	int theDrawMode,
	const Rect* theClipRect)
{
	if (gGpuRenderer == NULL || theNumVertices <= 0)
		return;
	int aDrawMode = theDrawMode < 0 ? gGpuCurrentDrawMode : theDrawMode;

	// Bound the stack frame for general triangle batches; large batches use
	// dynamic storage. Uniform image quads bypass this conversion entirely.
	GpuVertex aLocal[192];
	const size_t aRequired = theType == PRIMITIVE_TRIANGLELIST ? size_t(theNumVertices) :
		size_t(std::max(0, theNumVertices - 2)) * 3;
	std::vector<GpuVertex> aLarge;
	if (aRequired > std::size(aLocal)) aLarge.resize(aRequired);
	GpuVertex* aConverted = aLarge.empty() ? aLocal : aLarge.data();
	int aCount = 0;
	const uint32_t aSlot = GpuRenderer::SlotOf(theSRV) | CombinedImageSamplingFlags();
	// 采样器索引同法逐顶点携带（bit0=线性 bit1=重复），消除描述符重绑
	const uint32_t aSampler = (gLinearFilter ? 1u : 0u) | (RenderInterface::sSamplerAddressWrap ? 2u : 0u);

	auto ConvertVertex = [&](const ScreenVertex& theVertex)
	{
		GpuVertex& aOut = aConverted[aCount++];
		aOut.x = theVertex.sx;
		aOut.y = theVertex.sy;
		aOut.r = ((theVertex.color >> 16) & 0xFF) / 255.0f;
		aOut.g = ((theVertex.color >> 8) & 0xFF) / 255.0f;
		aOut.b = (theVertex.color & 0xFF) / 255.0f;
		aOut.a = ((theVertex.color >> 24) & 0xFF) / 255.0f;
		aOut.u = theVertex.tu;
		aOut.v = theVertex.tv;
		aOut.slot = aSlot;
		aOut.sampler = aSampler;
	};

#if defined(PVZ_GPU_VULKAN) || defined(PVZ_GPU_D3D12) || defined(PVZ_GPU_OPENGL)
    if (theNumVertices==4 && theType!=PRIMITIVE_TRIANGLELIST) {
        if (theType==PRIMITIVE_TRIANGLESTRIP)
            for (int i=0;i<4;++i) ConvertVertex(theVertices[i]);
        else
            for (int i : {1,2,0,3}) ConvertVertex(theVertices[i]);
    } else
#endif
	if (theType == PRIMITIVE_TRIANGLESTRIP)
	{
		for (int i = 0; i + 2 < theNumVertices; i++)
		{
			ConvertVertex(theVertices[i]);
			ConvertVertex(theVertices[i + 1]);
			ConvertVertex(theVertices[i + 2]);
		}
	}
	else if (theType == PRIMITIVE_TRIANGLEFAN)
	{
		for (int i = 1; i + 1 < theNumVertices; i++)
		{
			ConvertVertex(theVertices[0]);
			ConvertVertex(theVertices[i]);
			ConvertVertex(theVertices[i + 1]);
		}
	}
	else
	{
		for (int i = 0; i < theNumVertices; i++)
			ConvertVertex(theVertices[i]);
	}

    if (theClipRect && (theClipRect->mWidth <= 0 || theClipRect->mHeight <= 0)) return;
    GpuClip nativeClip{};
    if(theClipRect) nativeClip={theClipRect->mX,theClipRect->mY,theClipRect->mWidth,theClipRect->mHeight};
    const auto aBlend = aDrawMode == 1 ? GpuBlend::Additive :
        aDrawMode == 2 ? GpuBlend::Opaque : GpuBlend::Normal;
#if defined(PVZ_GPU_VULKAN) || defined(PVZ_GPU_D3D12) || defined(PVZ_GPU_OPENGL)
    if (aCount==4) {
        gGpuRenderer->SubmitQuads(std::span<const GpuVertex>(aConverted,aCount),theSRV,
            theLinearFilter,RenderInterface::sSamplerAddressWrap,aBlend,theClipRect ? &nativeClip : nullptr);
        return;
    }
#endif
    gGpuRenderer->SubmitTriangles(
        std::span<const GpuVertex>(aConverted, aCount), theSRV,
        theLinearFilter, RenderInterface::sSamplerAddressWrap, aBlend, theClipRect ? &nativeClip : nullptr);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static void SubmitTextureQuad(const SexyVector2 (&points)[4], const TextureRegion& source,
	const Color& color, GpuTexture* texture, const Rect* clip)
{
	if (!gGpuRenderer || (clip && (clip->mWidth <= 0 || clip->mHeight <= 0))) return;
	// Decode the uniform tint once, preserving the existing packed-color semantics.
	const DWORD packed = RGBA_MAKE(color.mRed, color.mGreen, color.mBlue, color.mAlpha);
	const float red = ((packed >> 16) & 0xff) / 255.0f;
	const float green = ((packed >> 8) & 0xff) / 255.0f;
	const float blue = (packed & 0xff) / 255.0f;
	const float alpha = ((packed >> 24) & 0xff) / 255.0f;
	const uint32_t slot = GpuRenderer::SlotOf(texture) | CombinedImageSamplingFlags();
	const uint32_t sampler = (gLinearFilter ? 1u : 0u) | (RenderInterface::sSamplerAddressWrap ? 2u : 0u);
#if defined(PVZ_GPU_VULKAN) || defined(PVZ_GPU_D3D12) || defined(PVZ_GPU_OPENGL)
	// Preserve the previous strip/fan diagonal, including non-parallelogram UVs.
	static constexpr unsigned int order[2][4]{{0,1,2,3},{1,3,0,2}};
	GpuVertex vertices[4];
#else
	static constexpr unsigned int order[2][6]{{0,1,2,1,2,3},{0,1,3,0,3,2}};
	GpuVertex vertices[6];
#endif
	const auto& indices = order[clip != nullptr];
	for (size_t i = 0; i < std::size(vertices); ++i)
	{
		const auto index = indices[i];
		vertices[i] = {points[index].x, points[index].y, red, green, blue, alpha,
			(index & 2) ? source.mU1 : source.mU0, (index & 1) ? source.mV1 : source.mV0,
			slot, sampler};
	}
#if defined(PVZ_GPU_VULKAN) || defined(PVZ_GPU_D3D12) || defined(PVZ_GPU_OPENGL)
	GpuClip nativeClip{};
	if (clip) nativeClip = {clip->mX,clip->mY,clip->mWidth,clip->mHeight};
	const auto blend = gGpuCurrentDrawMode == 1 ? GpuBlend::Additive :
		gGpuCurrentDrawMode == 2 ? GpuBlend::Opaque : GpuBlend::Normal;
	gGpuRenderer->SubmitQuads(vertices,texture,gLinearFilter,RenderInterface::sSamplerAddressWrap,
		blend,clip ? &nativeClip : nullptr);
#else
	SubmitNativeTriangles(vertices,texture,gLinearFilter,-1,clip);
#endif
}

void TextureSource::Blt(float theX, float theY, const TextureRegion& theSrcRect, const Color& theColor) const
{
	TextureSamplingScope sampling(mSamplingFlags);
	if(theSrcRect.mWidth<=0 || theSrcRect.mHeight<=0)
		return;
	const float right=theX+theSrcRect.mWidth;
	const float bottom=theY+theSrcRect.mHeight;
	const SexyVector2 points[4]{{theX,theY},{theX,bottom},{right,theY},{right,bottom}};
	SubmitTextureQuad(points,theSrcRect,theColor,mTexture,nullptr);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void TextureSource::BltTransformed(const SexyMatrix3 &theTrans, const TextureRegion& theSrcRect, const Color& theColor, const Rect *theClipRect, float theX, float theY, bool center) const
{
	TextureSamplingScope sampling(mSamplingFlags);
	if(theSrcRect.mWidth<=0 || theSrcRect.mHeight<=0)
		return;
	const float x=center ? -theSrcRect.mWidth/2.0f : 0.0f;
	const float y=center ? -theSrcRect.mHeight/2.0f : 0.0f;
	SexyVector2 p[4] = {
		SexyVector2(x,y), SexyVector2(x,y+theSrcRect.mHeight),
		SexyVector2(x+theSrcRect.mWidth,y), SexyVector2(x+theSrcRect.mWidth,y+theSrcRect.mHeight)
	};
	for(auto& point:p)
	{
		point=theTrans*point;
		point.x+=theX;
		point.y+=theY;
	}
	SubmitTextureQuad(p,theSrcRect,theColor,mTexture,theClipRect);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void TextureSource::BltTriangles(const TriVertex theVertices[][3], int theNumTriangles, DWORD theColor, float tx, float ty) const
{
	if(theNumTriangles<=0) return;
	SubmitTextureTriangleBatch(size_t(theNumTriangles),
		[&](size_t i)->const TriVertex& {return theVertices[i/3][i%3];},
		*this,theColor,tx,ty);
}

void TextureSource::BltTriangles(std::span<const std::array<TriVertex,3>> theTriangles, DWORD theColor, float tx, float ty,const Rect* clip) const
{
	SubmitTextureTriangleBatch(theTriangles.size(),
		[&](size_t i)->const TriVertex& {return theTriangles[i/3][i%3];},
		*this,theColor,tx,ty,clip);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
GpuTexture* RenderInterface::CreateImageTexture(Image *image)
{
	if(CurrentNativeThreadIdentity()!=gSexyAppBase->mPrimaryThreadId) throw GpuImageCpuAccess{};
	// Empty images have no texture; oversized images throw without tiling.
	if (image == nullptr || image->mWidth <= 0 || image->mHeight <= 0)
	{
		return nullptr;
	}
	if (image->mRenderOwner && image->mRenderOwner!=this)
		throw std::logic_error("Image belongs to another renderer");
	if (image->mTexture)
		return image->mTexture;
	RequireImageDimensions(mRenderer,image->mWidth,image->mHeight);
	if (!image->mPendingDecoded || !image->mPendingDecoded->mBits)
		throw std::logic_error("Image has no loading payload for GPU upload");
	image->CommitBits();
	const auto& decoded=*image->mPendingDecoded;
	if (decoded.mWidth!=image->mWidth || decoded.mHeight!=image->mHeight)
		throw std::logic_error("Image loading payload dimensions differ");
	auto* texture=UploadDecodedImage(*mRenderer,decoded,!image->mHasAlpha && !image->mHasTrans);
	if (!texture)
		throw std::runtime_error("Image upload returned no GPU texture");
	try
	{
		AutoCrit lock(gSexyAppBase->mDDInterface->mCritSect);
		mImageSet.insert(image);
	}
	catch (...) { mRenderer->DestroyTexture(texture); throw; }
	image->mTexture=texture;
	image->mRenderTarget=false;
	image->mRenderOwner=this;
	image->PurgeBits();
	return texture;
}

GpuTexture* RenderInterface::UploadAtlasTexture(ImageLib::Image*& decoded)
{
	if (CurrentNativeThreadIdentity()!=gSexyAppBase->mPrimaryThreadId)
		throw GpuImageCpuAccess{};
	if (!decoded || !decoded->mBits || decoded->HasComposition())
		throw std::logic_error("Atlas has no plain loading payload");
	RequireImageDimensions(mRenderer,decoded->mWidth,decoded->mHeight);
	// Atlases bypass ResourceManager's ordinary image sanding. Transparent
	// source texels may still contain white RGB, which linear sampling blends
	// into visible edges. Repair once before upload; alpha and visible RGB stay
	// unchanged, and the decoded pixels are still discarded below.
	ImageLib::FixTransparentEdges(*decoded);
	auto* texture=mRenderer->CreateTexture(decoded->mWidth,decoded->mHeight,
		decoded->mBits,decoded->mWidth*4,false);
	if (!texture)
		throw std::runtime_error("Atlas upload returned no GPU texture");
	try
	{
		mAtlasTextures.emplace(texture,std::array{decoded->mWidth,decoded->mHeight});
	}
	catch (...) { mRenderer->DestroyTexture(texture); throw; }
	delete std::exchange(decoded,nullptr);
	return texture;
}

void RenderInterface::FreeAtlasTextures()
{
	for (const auto& [texture,dimensions]:mAtlasTextures)
		mRenderer->DestroyTexture(texture);
	mAtlasTextures.clear();
}

void RenderInterface::CloneUploadedImage(Image* target,const Image& source)
{
	auto* original=CreateImageTexture(const_cast<Image*>(&source));
	if(!original) throw GpuImageCpuAccess{};
	if(!target || target->mTexture || target->mRenderOwner)
		throw GpuImageCpuAccess{};
	if (source.mRenderTarget)
	{
		// A copied font/render image must remain a writable GPU target. Use a
		// temporary render target rather than a sampled-only texture copy.
		auto* clone=CreateRenderImage(source.mWidth,source.mHeight);
		try
		{
			{
				GpuImageDrawScope drawing(*this,clone);
				GpuImageSamplingScope sampling(*this,0);
				struct RestoreCopyState
				{
					int mode=gGpuCurrentDrawMode;
					bool linear=gLinearFilter;
					bool wrap=RenderInterface::sSamplerAddressWrap;
					~RestoreCopyState()
					{
						gGpuCurrentDrawMode=mode;
						gLinearFilter=linear;
						RenderInterface::sSamplerAddressWrap=wrap;
					}
				} restore;
				if (!PreDraw()) throw GpuImageCpuAccess{};
				gGpuCurrentDrawMode=Graphics::DRAWMODE_OPAQUE;
				gLinearFilter=false;
				sSamplerAddressWrap=false;
				// Raw pixel copy: ignore the caller's transform and tint/filter
				// state while retaining the source/target alpha convention.
				TextureSource{original,0}.Blt(
					0,0,TextureRegion{0,0,1,1,source.mWidth,source.mHeight},Color::White);
			}
			// Registration may allocate. Publish no ownership until it succeeds.
			mImageSet.insert(target);
			target->mTexture=clone->mTexture;
			target->mRenderTarget=true;
			target->mRenderOwner=this;
			clone->mTexture=nullptr;
			delete clone;
			return;
		}
		catch (...) { delete clone; throw; }
	}
	RequireImageDimensions(mRenderer,source.mWidth,source.mHeight);
	auto* copy=mRenderer->CopyTextureRegion(original,0,0,source.mWidth,source.mHeight);
	if (!copy)
		throw std::runtime_error("Image copy returned no GPU texture");
	try
	{
	mImageSet.insert(target);
	target->mTexture=copy;
	target->mRenderTarget=false;
	target->mRenderOwner=this;
	}
	catch (...) { mRenderer->DestroyTexture(copy); throw; }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool RenderInterface::HasCausticsCompute() const
{
	return mRenderer != NULL && mRenderer->HasCausticsCompute();
}

bool RenderInterface::InstallCausticsCompute(Image* theImage, Image* source)
{
	if (mRenderer == NULL || !mRenderer->HasCausticsCompute())
		throw std::logic_error("Pool requires native GPU caustics");
	if (theImage == NULL || theImage->mTexture != nullptr || source==nullptr)
		throw std::logic_error("Invalid pool GPU image ownership");
	auto* sourceTexture=CreateImageTexture(source);
	if(!sourceTexture)
		throw std::logic_error("Pool lookup requires one resident GPU texture");

	const int& aWidth = theImage->mWidth;
	const int& aHeight = theImage->mHeight;
	RequireImageDimensions(mRenderer,aWidth,aHeight);
	auto* texture=mRenderer->CreateCausticsTexture(aWidth,aHeight,sourceTexture);
	if (!texture)
		throw std::runtime_error("Caustics allocation returned no GPU texture");
	try
	{
	AutoCrit aCrit(gSexyAppBase->mDDInterface->mCritSect);
	mImageSet.insert(theImage);
	theImage->mTexture=texture;
	theImage->mRenderTarget=false;
	theImage->mRenderOwner=this;
	theImage->PurgeBits();
	return true;
	}
	catch (...) { mRenderer->DestroyTexture(texture); throw; }
}

void RenderInterface::SetCausticsTimeNs(unsigned long long theTimeNs)
{
	if (mRenderer != NULL)
		mRenderer->SetCausticsTimeNs(theTimeNs);
}

void RenderInterface::UseCausticsImages(Image* output,Image* source)
{
	mRenderer->UseCausticsTexture(output->mTexture,source->mTexture);
}

GpuTexture* RenderInterface::GetCausticsTexture() const
{
	return mRenderer != NULL ?
		GpuBackendDetail::CausticsTexture<BuildConfig::Graphics>(*mRenderer) : NULL;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::RemoveImage(Image* image)
{
	if (mDrawSurface==image)
	{
		if (mRenderer) SelectDrawSurface(nullptr);
		else mDrawSurface=nullptr;
	}
	AutoCrit aCrit(gSexyAppBase->mDDInterface->mCritSect);
	mImageSet.erase(image);
	if (image->mTexture) mRenderer->DestroyTexture(image->mTexture);
	image->mTexture=nullptr;
	image->mRenderOwner=nullptr;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::Cleanup()
{
	Flush();
	if(mRenderer!=nullptr) SelectDrawSurface(nullptr);

	ImageSet::iterator anItr;
	for (anItr = mImageSet.begin(); anItr != mImageSet.end(); ++anItr)
	{
		Image *anImage = *anItr;
		if (anImage->mTexture) mRenderer->DestroyTexture(anImage->mTexture);
		anImage->mTexture = nullptr;
		anImage->mRenderOwner=nullptr;
	}
	mImageSet.clear();
	FreeAtlasTextures();

	if (mRenderer != NULL)
	{
		delete mRenderer;
		mRenderer = NULL;
		gGpuRenderer = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::SetCurTexture(Image *theImage)
{
	CreateImageTexture(theImage);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::PushTransform(const SexyMatrix3 &theTransform, bool concatenate)
{
	if (concatenate && !mTransformStack.empty())
		mTransformStack.push_back(mTransformStack.back() * theTransform);
	else
		mTransformStack.push_back(theTransform);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::PopTransform()
{
	if (!mTransformStack.empty())
		mTransformStack.pop_back();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::Blt(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode, bool linearFilter)
{
	if (!mTransformStack.empty())
	{
		BltClipF(theImage, theX, theY, theSrcRect, NULL, theColor, theDrawMode);
		return;
	}

	if (!PreDraw())
		return;

	auto* texture = CreateImageTexture(theImage);
	if (!texture)
		return;

	gGpuCurrentDrawMode = theDrawMode;

	gLinearFilter = linearFilter;
	TextureSource{texture,theImage->mGpuSamplingFlags}.Blt(
		theX,theY,PixelSourceToUv(theSrcRect,theImage->mWidth,theImage->mHeight),theColor);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::BltMirror(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode, bool linearFilter)
{
	SexyTransform2D aTransform;
	aTransform.Translate(-theSrcRect.mWidth, 0);
	aTransform.Scale(-1, 1);
	aTransform.Translate(theX, theY);
	BltTransformed(theImage, NULL, theColor, theDrawMode, theSrcRect, aTransform, linearFilter);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::BltClipF(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Rect *theClipRect, const Color& theColor, int theDrawMode)
{
	SexyTransform2D aTransform;
	aTransform.Translate(theX, theY);
	BltTransformed(theImage, theClipRect, theColor, theDrawMode, theSrcRect, aTransform, true);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::StretchBlt(Image* theImage, const Rect& theDestRect, const Rect& theSrcRect, const Rect* theClipRect, const Color &theColor, int theDrawMode, bool fastStretch, bool mirror)
{
	float xScale = (float)theDestRect.mWidth / theSrcRect.mWidth;
	float yScale = (float)theDestRect.mHeight / theSrcRect.mHeight;

	SexyTransform2D aTransform;
	if (mirror)
	{
		aTransform.Translate(-theSrcRect.mWidth, 0);
		aTransform.Scale(-xScale, yScale);
	}
	else
		aTransform.Scale(xScale, yScale);

	aTransform.Translate(theDestRect.mX, theDestRect.mY);
	BltTransformed(theImage, theClipRect, theColor, theDrawMode, theSrcRect, aTransform, !fastStretch);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::BltRotated(Image* theImage, float theX, float theY, const Rect* theClipRect, const Color& theColor, int theDrawMode, double theRot, float theRotCenterX, float theRotCenterY, const Rect &theSrcRect)
{
	SexyTransform2D aTransform;
	aTransform.Translate(-theRotCenterX, -theRotCenterY);
	aTransform.RotateRad(theRot);
	aTransform.Translate(theX + theRotCenterX, theY + theRotCenterY);
	BltTransformed(theImage, theClipRect, theColor, theDrawMode, theSrcRect, aTransform, true);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
static void BltPreparedSource(RenderInterface& render,const TextureSource& source,
	const Rect* theClipRect,const Color& theColor,int theDrawMode,const TextureRegion& theSrcRect,
	const SexyMatrix3& theTransform,bool linearFilter,float theX,float theY,bool center)
{
	gGpuCurrentDrawMode = theDrawMode;

	if (!render.mTransformStack.empty())
	{
		gLinearFilter = true;
		if (theX != 0 || theY != 0)
		{
			SexyTransform2D aTransform;
			if (center)
				aTransform.Translate(-theSrcRect.mWidth/2.0f, -theSrcRect.mHeight/2.0f);
			aTransform = theTransform * aTransform;
			aTransform.Translate(theX, theY);
			aTransform = render.mTransformStack.back() * aTransform;
			source.BltTransformed(aTransform, theSrcRect, theColor, theClipRect);
		}
		else
		{
			SexyTransform2D aTransform = render.mTransformStack.back() * theTransform;
			source.BltTransformed(aTransform, theSrcRect, theColor, theClipRect, theX, theY, center);
		}
	}
	else
	{
		gLinearFilter = linearFilter;
		source.BltTransformed(theTransform, theSrcRect, theColor, theClipRect, theX, theY, center);
	}
}

void RenderInterface::BltTransformed(Image* image,const Rect* clip,const Color& color,
	int drawMode,const Rect& sourceRect,const SexyMatrix3& transform,bool linearFilter,
	float x,float y,bool center)
{
	if (!PreDraw()) return;
	auto* texture=CreateImageTexture(image);
	if (!texture) return;
	BltPreparedSource(*this,TextureSource{texture,image->mGpuSamplingFlags},
		clip,color,drawMode,PixelSourceToUv(sourceRect,image->mWidth,image->mHeight),
		transform,linearFilter,x,y,center);
}

void RenderInterface::BltTransformed(const TextureSource& source,const Rect* clip,
	const Color& color,int drawMode,const TextureRegion& sourceRect,const SexyMatrix3& transform,
	bool linearFilter,float x,float y,bool center)
{
	if (!source.mTexture || !PreDraw()) return;
	BltPreparedSource(*this,source,clip,color,drawMode,sourceRect,transform,linearFilter,x,y,center);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::DrawLine(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor, int theDrawMode)
{
	if (!PreDraw())
		return;

	gGpuCurrentDrawMode = theDrawMode;

	float x1, y1, x2, y2;
	DWORD aColor = RGBA_MAKE(theColor.mRed, theColor.mGreen, theColor.mBlue, theColor.mAlpha);

	if (!mTransformStack.empty())
	{
		SexyVector2 p1(theStartX, theStartY);
		SexyVector2 p2(theEndX, theEndY);
		p1 = mTransformStack.back()*p1;
		p2 = mTransformStack.back()*p2;
		x1 = p1.x;
		y1 = p1.y;
		x2 = p2.x;
		y2 = p2.y;
	}
	else
	{
		x1 = theStartX;
		y1 = theStartY;
		x2 = theEndX;
		y2 = theEndY;
	}

	// Rasterize the one-pixel line as a native triangle-list quad.
	float aDx = x2 - x1;
	float aDy = y2 - y1;
	float aLen = sqrtf(aDx*aDx + aDy*aDy);
	if (aLen < 1.0f)
		aLen = 1.0f;
	float aPx = -aDy / aLen * 0.5f;
	float aPy = aDx / aLen * 0.5f;

	ScreenVertex aVertex[6] =
	{
		{ x1 + aPx, y1 + aPy, 0, 1, aColor, 0, 0 },
		{ x2 + aPx, y2 + aPy, 0, 1, aColor, 0, 0 },
		{ x2 - aPx, y2 - aPy, 0, 1, aColor, 0, 0 },
		{ x1 + aPx, y1 + aPy, 0, 1, aColor, 0, 0 },
		{ x2 - aPx, y2 - aPy, 0, 1, aColor, 0, 0 },
		{ x1 - aPx, y1 - aPy, 0, 1, aColor, 0, 0 }
	};
	SubmitGpuTriangles(aVertex, 6, PRIMITIVE_TRIANGLELIST,
		gGpuRenderer->whiteTexture, false, theDrawMode, NULL);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::FillRect(const Rect& theRect, const Color& theColor, int theDrawMode)
{
	if (!PreDraw())
		return;

	gGpuCurrentDrawMode = theDrawMode;

	DWORD aColor = RGBA_MAKE(theColor.mRed, theColor.mGreen, theColor.mBlue, theColor.mAlpha);
	float x = theRect.mX;
	float y = theRect.mY;
	float aWidth = theRect.mWidth;
	float aHeight = theRect.mHeight;

	ScreenVertex aVertex[4] =
	{
		{ x, y, 0, 1, aColor, 0, 0 },
		{ x, y + aHeight, 0, 1, aColor, 0, 0 },
		{ x + aWidth, y, 0, 1, aColor, 0, 0 },
		{ x + aWidth, y + aHeight, 0, 1, aColor, 0, 0 }
	};

	if (!mTransformStack.empty())
	{
		SexyVector2 p[4] = { SexyVector2(x, y), SexyVector2(x, y+aHeight), SexyVector2(x+aWidth, y), SexyVector2(x+aWidth, y+aHeight) };
		for (int i=0; i<4; i++)
		{
			p[i] = mTransformStack.back()*p[i];
			aVertex[i].sx = p[i].x;
			aVertex[i].sy = p[i].y;
		}
	}

	SubmitGpuTriangles(aVertex, 4, PRIMITIVE_TRIANGLESTRIP,
		gGpuRenderer->whiteTexture, false, theDrawMode, NULL);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::FillScanLinesWithCoverage(const Span* spans,int spanCount,const Color& color,int drawMode,
    const BYTE* coverage,int coverX,int coverY,int coverWidth,int coverHeight)
{
    if(spanCount<0 || (spanCount && (!spans || !coverage || coverWidth<=0 || coverHeight<=0)))
        throw std::invalid_argument("Invalid screen coverage input");
    if(!spanCount) return;
    // Validate before recording anything; do not turn an invalid span into a
    // clipped success or read beyond the caller's coverage rectangle.
    bool hasPixels=false;
    for(int i=0;i<spanCount;++i) {
        const auto& span=spans[i];
        if(span.mWidth<0) throw std::invalid_argument("Negative screen coverage span width");
        if(!span.mWidth) continue;
        const int64_t x=int64_t(span.mX)-coverX,y=int64_t(span.mY)-coverY;
        if(x<0 || y<0 || y>=coverHeight || x+span.mWidth>coverWidth)
            throw std::invalid_argument("Screen coverage span exceeds mask bounds");
        hasPixels=true;
    }
    if(!hasPixels || !PreDraw()) return;
    RequireImageDimensions(mRenderer,coverWidth,coverHeight);
    gGpuCurrentDrawMode = drawMode;
    auto* mask=mRenderer->CreateCoverageMask(coverWidth,coverHeight,
        std::span<const uint8_t>(coverage,size_t(coverWidth)*coverHeight),coverWidth);
    // bindless 槽位逐顶点携带：蒙版槽必须写进顶点（零初始化的 slot=0 是
    // 场景纹理——覆盖度会采到场景自身，cdd454d4 bindless 迁移漏改此路径
    // 的回归，Scenes 冒烟 ProbeCoverage 定罪）。采样器恒 0=nearest/clamp。
    const uint32_t maskSlot=GpuRenderer::SlotOf(mask);
    // Retire the mask on both success and propagated draw failure. This does
    // not wait or read pixels back; the renderer retains submitted references.
    try
    {
    std::vector<GpuVertex> vertices;
    vertices.reserve(size_t(spanCount)*6);
    const auto blend=drawMode==1?GpuBlend::Additive:drawMode==2?GpuBlend::Opaque:GpuBlend::Normal;
    const float r=color.mRed/255.0f,g=color.mGreen/255.0f,b=color.mBlue/255.0f,a=color.mAlpha/255.0f;
    for(int i=0;i<spanCount;++i) {
        const auto& span=spans[i];
        if(!span.mWidth) continue;
        const float x=float(span.mX),y=float(span.mY),right=float(int64_t(span.mX)+span.mWidth);
        SexyVector2 points[]={{x,y},{right,y},{right,y+1},{x,y+1}};
        if(!mTransformStack.empty()) for(auto& point:points) point=mTransformStack.back()*point;
        const float u1=float(int64_t(span.mX)-coverX)/coverWidth;
        const float u2=float(int64_t(span.mX)-coverX+span.mWidth)/coverWidth;
        const float v1=float(int64_t(span.mY)-coverY)/coverHeight,v2=v1+1.0f/coverHeight;
        const float u[]={u1,u2,u2,u1},v[]={v1,v1,v2,v2};
        for(const int n:{0,1,2,0,2,3}) vertices.push_back({points[n].x,points[n].y,r,g,b,a,u[n],v[n],maskSlot,0});
    }
    mRenderer->SubmitCoverageTriangles(vertices,mask,blend);
    }
    catch (...) { mRenderer->DestroyTexture(mask); throw; }
    mRenderer->DestroyTexture(mask);
}

void RenderInterface::DrawTriangle(const TriVertex &p1, const TriVertex &p2, const TriVertex &p3, const Color &theColor, int theDrawMode)
{
	if (!PreDraw())
		return;

	gGpuCurrentDrawMode = theDrawMode;
	DWORD aColor = RGBA_MAKE(theColor.mRed, theColor.mGreen, theColor.mBlue, theColor.mAlpha);

	ScreenVertex aVertex[3] =
	{
		{ p1.x, p1.y, 0, 1, aColor, 0, 0 },
		{ p2.x, p2.y, 0, 1, aColor, 0, 0 },
		{ p3.x, p3.y, 0, 1, aColor, 0, 0 }
	};
	SubmitGpuTriangles(aVertex, 3, PRIMITIVE_TRIANGLESTRIP,
		gGpuRenderer->whiteTexture, false, theDrawMode, NULL);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::FillPoly(const Point theVertices[], int theNumVertices, const Rect *theClipRect, const Color &theColor, int theDrawMode, int tx, int ty)
{
	if (theNumVertices < 3)
		return;

	if (!PreDraw())
		return;

	gGpuCurrentDrawMode = theDrawMode;
	DWORD aColor = RGBA_MAKE(theColor.mRed, theColor.mGreen, theColor.mBlue, theColor.mAlpha);

	ScreenVertex local[100];
	std::vector<ScreenVertex> large;
	if (theNumVertices > int(std::size(local))) large.resize(theNumVertices);
	ScreenVertex* vertices = large.empty() ? local : large.data();
	for (int i=0; i<theNumVertices; i++)
	{
		ScreenVertex vert = { theVertices[i].mX + (float)tx, theVertices[i].mY + (float)ty, 0, 1, aColor, 0, 0 };
		if (!mTransformStack.empty())
		{
			SexyVector2 v(vert.sx, vert.sy);
			v = mTransformStack.back()*v;
			vert.sx = v.x;
			vert.sy = v.y;
		}
		vertices[i] = vert;
	}

	SubmitGpuTriangles(vertices, theNumVertices, PRIMITIVE_TRIANGLEFAN,
		gGpuRenderer->whiteTexture, false, theDrawMode, theClipRect);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::DrawTriangleTex(const TriVertex &p1, const TriVertex &p2, const TriVertex &p3, const Color &theColor, int theDrawMode, Image *theTexture, bool blend)
{
	TriVertex aVertices[1][3] = {{p1, p2, p3}};
	DrawTrianglesTex(aVertices, 1, theColor, theDrawMode, theTexture, 0.0f, 0.0f, blend);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::DrawTrianglesTex(const TriVertex theVertices[][3], int theNumTriangles, const Color &theColor, int theDrawMode, Image *theTexture, float tx, float ty, bool blend)
{
	if (!PreDraw())
		return;

	auto* texture = CreateImageTexture(theTexture);
	if (!texture)
		return;

	gGpuCurrentDrawMode = theDrawMode;
	DWORD aColor = RGBA_MAKE(theColor.mRed, theColor.mGreen, theColor.mBlue, theColor.mAlpha);

	gLinearFilter = blend;
	TextureSource{texture,theTexture->mGpuSamplingFlags}.BltTriangles(
		theVertices,theNumTriangles,aColor,tx,ty);
}

void RenderInterface::DrawTrianglesTexClipped(const TriVertex vertices[][3], int count,
    const Color& color, int drawMode, Image* image, const Rect& clip,
    float tx, float ty, bool blend)
{
    if (count <= 0 || clip.mWidth <= 0 || clip.mHeight <= 0 || !PreDraw()) return;
    auto* texture = CreateImageTexture(image);
    if (!texture) return;
    gGpuCurrentDrawMode = drawMode;
    gLinearFilter = blend;
    SubmitTextureTriangleBatch(size_t(count),
        [&](size_t i)->const TriVertex& { return vertices[i/3][i%3]; },
        TextureSource{texture,image->mGpuSamplingFlags},
        RGBA_MAKE(color.mRed,color.mGreen,color.mBlue,color.mAlpha),tx,ty,&clip);
}

void RenderInterface::DrawPool(Image* image,const GpuPoolParams& params)
{
    if (!PreDraw()) return;
    auto* texture=CreateImageTexture(image);
    if (!texture) return;
    TextureSamplingScope sampling(image->mGpuSamplingFlags);
    if (params.clip.width <= 0 || params.clip.height <= 0) return;
    auto state=params;
    state.samplingFlags|=CombinedImageSamplingFlags();
    mRenderer->SubmitPool(texture,state);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::DrawTrianglesTexStrip(const TriVertex theVertices[], int theNumTriangles, const Color &theColor, int theDrawMode, Image *theTexture, float tx, float ty, bool blend)
{
	if(theNumTriangles<=0 || !PreDraw()) return;
	auto* texture=CreateImageTexture(theTexture);
	if(!texture) return;
	gGpuCurrentDrawMode = theDrawMode;
	gLinearFilter = blend;
	const DWORD color=RGBA_MAKE(theColor.mRed,theColor.mGreen,theColor.mBlue,theColor.mAlpha);
	SubmitTextureTriangleBatch(size_t(theNumTriangles),
		[&](size_t i)->const TriVertex& {return theVertices[i/3+i%3];},
		TextureSource{texture,theTexture->mGpuSamplingFlags},color,tx,ty);
}

void RenderInterface::DrawTrianglesTex(std::span<const std::array<TriVertex,3>> theTriangles,
	const Color& theColor,int theDrawMode,Image* theTexture,float tx,float ty,bool blend)
{
	if(theTriangles.empty() || !PreDraw()) return;
	auto* texture=CreateImageTexture(theTexture);
	if(!texture) return;
	gGpuCurrentDrawMode = theDrawMode;
	gLinearFilter = blend;
	TextureSource{texture,theTexture->mGpuSamplingFlags}.BltTriangles(theTriangles,
		RGBA_MAKE(theColor.mRed,theColor.mGreen,theColor.mBlue,theColor.mAlpha),tx,ty);
}

void RenderInterface::DrawTrianglesTex(std::span<const std::array<TriVertex,3>> triangles,
	const Color& color,int drawMode,const TextureSource& source,float tx,float ty,bool blend,const Rect* clip)
{
	if (triangles.empty() || !source.mTexture || !PreDraw()) return;
	gGpuCurrentDrawMode=drawMode;
	gLinearFilter=blend;
	source.BltTriangles(triangles,
		RGBA_MAKE(color.mRed,color.mGreen,color.mBlue,color.mAlpha),tx,ty,clip);
}

#if defined(PVZ_GPU_VULKAN) || defined(PVZ_GPU_D3D12) || defined(PVZ_GPU_OPENGL)
void RenderInterface::DrawQuadsTex(std::span<const std::array<TriVertex,4>> quads,
    const Color& color,int drawMode,const TextureSource& source,float tx,float ty,bool blend,const Rect* clip)
{
    if (quads.empty() || !source.mTexture || !PreDraw()) return;
    gGpuCurrentDrawMode=drawMode;
    gLinearFilter=blend;
    SubmitTextureTriangleBatch<true>(quads.size(),
        [&](size_t i)->const TriVertex& {return quads[i/4][i%4];},source,
        RGBA_MAKE(color.mRed,color.mGreen,color.mBlue,color.mAlpha),tx,ty,clip);
}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void RenderInterface::Flush()
{
	if (mRenderer != NULL)
	{
		if (mSceneBegun)
		{
			// EndSceneAndPresent may synchronously dispatch window messages through
			// Present. Clear ownership before entering it so a re-entrant redraw
			// cannot submit the same scene a second time.
			mSceneBegun = false;
			mRenderer->EndSceneAndPresent();
		}
		return;
	}
}
