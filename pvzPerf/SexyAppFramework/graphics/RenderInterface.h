#pragma once
#include "ImageReadback.h"

#include "GpuBackend.h"
#include "TextureSource.h"

#include "Common.h"
#include "Image.h"
#include "TriVertex.h"
#include "misc/SexyMatrix.h"
#include <array>
#include <span>
#include <unordered_map>


namespace Sexy
{

class DDInterface;
class SexyMatrix3;
class TriVertex;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
enum class GpuImageColorFilter : uint32_t
{
	None = 0,
	White = 0x10000000u,
	WashedOut = 0x20000000u,
	LessWashedOut = 0x30000000u
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class RenderInterface
{
public:
	NativeWindowHandle		mHWnd;
	int						mWidth;
	int						mHeight;

	int						mPresentWidth;				// output (scaled) width
	int						mPresentHeight;				// output (scaled) height

	bool					mSceneBegun;
    // Latched for one outer screen draw; offscreen rendering is independent.
    bool mScreenDrawDeferred = false;
	bool					mIsWindowed;

	typedef std::set<Image*> ImageSet;
	ImageSet mImageSet;
	// Atlas lifetime/diagnostic bookkeeping only; definitions borrow the keys
	// directly and do not point at entries in this registry.
	std::unordered_map<GpuTexture*,std::array<int,2>> mAtlasTextures;

	typedef std::list<SexyMatrix3> TransformStack;
	TransformStack mTransformStack;

	GpuRenderer*			mRenderer;
	Image*                  mDrawSurface = nullptr;

public:
	// 池水焦散 GPU 直写（2026-10-05 立项：每帧 CPU 生成 8192px + 纹理重传
	// 是性能缺陷）。Install 一次性接管 theImage 的纹理并上传 LUT；此后每帧
	// SetCausticsTimeNs(elapsedNs)，BeginScene 前渲染器 compute 直写目标纹理。
	bool HasCausticsCompute() const;
	bool InstallCausticsCompute(Image* theImage, Image* source);
	void UseCausticsImages(Image* output, Image* source);
	void SetCausticsTimeNs(unsigned long long theTimeNs);
	GpuTexture* GetCausticsTexture() const;
    void DrawPool(Image* texture, const GpuPoolParams& params);

protected:
	bool					InitGPU();	

public:
	RenderInterface();
	~RenderInterface();

	void					Cleanup();
	void					PushTransform(const SexyMatrix3 &theTransform, bool concatenate = true);
	void					PopTransform();
	
	bool					PreDraw();
    void ResizeScene(int width, int height);
	[[nodiscard("caller owns the returned Image")]]
	Image* CreateRenderImage(int width, int height);
	[[nodiscard("assign the returned GPU pointer to the shared atlas binding")]]
	GpuTexture* UploadAtlasTexture(ImageLib::Image*& decoded);
	void FreeAtlasTextures();
	[[nodiscard("caller owns the returned Image")]]
	Image* ProcessImage(Image* source,const GpuImageProcessing& processing);
	void RenderFontAtlas(Image* target,Image* source,
		std::span<const GpuFontGlyph> glyphs,bool forceWhite);
	void                    SelectDrawSurface(Image* image);
	inline static uint32_t  sImageSamplingFlags = 0;
	inline static bool      sSamplerAddressWrap = false;
	void					Flush();
	// Diagnostic/export only, on the owning thread with a flushed scene.
	// Always reads the GPU; no CPU bitmap or palette fallback.
	ImagePixels ReadImagePixels(const Image& image);
	void RemoveImage(Image* image);
	// Window/fullscreen or custom-scale changes: resize the swapchain only.
	// The device, scene target and uploaded image textures all survive, so
	// toggling modes never triggers a full re-upload or GPU stall.
	bool					ResizePresent(int thePresentWidth, int thePresentHeight);
    // false means deferred, not failed. Invalid ownership / Vulkan errors throw.
	bool					PresentRetainedFrame();

	// Commit/upload once, discard the loading payload, and return the GPU handle.
	GpuTexture* CreateImageTexture(Image *theImage);
	void CloneUploadedImage(Image* target,const Image& source);
	void					SetCurTexture(Image *theImage);
	void					Blt(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode, bool linearFilter = false);
	void					BltClipF(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Rect *theClipRect, const Color& theColor, int theDrawMode);
	void					BltMirror(Image* theImage, float theX, float theY, const Rect& theSrcRect, const Color& theColor, int theDrawMode, bool linearFilter = false);
	void					StretchBlt(Image* theImage,  const Rect& theDestRect, const Rect& theSrcRect, const Rect* theClipRect, const Color &theColor, int theDrawMode, bool fastStretch, bool mirror = false);
	void					BltRotated(Image* theImage, float theX, float theY, const Rect* theClipRect, const Color& theColor, int theDrawMode, double theRot, float theRotCenterX, float theRotCenterY, const Rect& theSrcRect);
	void					BltTransformed(Image* theImage, const Rect* theClipRect, const Color& theColor, int theDrawMode, const Rect &theSrcRect, const SexyMatrix3 &theTransform, bool linearFilter, float theX = 0, float theY = 0, bool center = false);
	void BltTransformed(const TextureSource& source, const Rect* clip, const Color& color,
		int drawMode, const TextureRegion& sourceRect, const SexyMatrix3& transform, bool linearFilter,
		float x = 0, float y = 0, bool center = false);
	void					DrawLine(double theStartX, double theStartY, double theEndX, double theEndY, const Color& theColor, int theDrawMode);
	void					FillRect(const Rect& theRect, const Color& theColor, int theDrawMode);
    void FillScanLinesWithCoverage(const Span* spans,int spanCount,const Color& color,int drawMode,
        const BYTE* coverage,int coverX,int coverY,int coverWidth,int coverHeight);
	void					DrawTriangle(const TriVertex &p1, const TriVertex &p2, const TriVertex &p3, const Color &theColor, int theDrawMode);
	void					DrawTriangleTex(const TriVertex &p1, const TriVertex &p2, const TriVertex &p3, const Color &theColor, int theDrawMode, Image *theTexture, bool blend = true);
	void					DrawTrianglesTex(const TriVertex theVertices[][3], int theNumTriangles, const Color &theColor, int theDrawMode, Image *theTexture, float tx = 0, float ty = 0, bool blend = true);
	void DrawTrianglesTexClipped(const TriVertex vertices[][3], int count,
		const Color& color, int drawMode, Image* image, const Rect& clip,
		float tx = 0, float ty = 0, bool blend = true);
	void DrawTrianglesTex(std::span<const std::array<TriVertex,3>> theTriangles, const Color& theColor, int theDrawMode, Image* theTexture, float tx = 0, float ty = 0, bool blend = true);
	void DrawTrianglesTex(std::span<const std::array<TriVertex,3>> triangles,
		const Color& color, int drawMode, const TextureSource& source,
		float tx = 0, float ty = 0, bool blend = true, const Rect* clip = nullptr);
	void					DrawTrianglesTexStrip(const TriVertex theVertices[], int theNumTriangles, const Color &theColor, int theDrawMode, Image *theTexture, float tx = 0, float ty = 0, bool blend = true);
#if defined(PVZ_GPU_VULKAN) || defined(PVZ_GPU_D3D12) || (defined(PVZ_GPU_OPENGL) || defined(PVZ_GPU_WEBGL) || defined(PVZ_GPU_WEBGPU))
    void DrawQuadsTex(std::span<const std::array<TriVertex,4>> quads,const Color& color,
        int drawMode,const TextureSource& source,float tx=0,float ty=0,bool blend=true,const Rect* clip=nullptr);
#endif
	void					FillPoly(const Point theVertices[], int theNumVertices, const Rect *theClipRect, const Color &theColor, int theDrawMode, int tx, int ty);

	// Sampler wrap toggle used by the pool caustic effect.

	bool					InitFromDDInterface(DDInterface *theInterface);


};

class GpuImageDrawScope
{
	RenderInterface& mRender;
	Image* mPrevious;
public:
	GpuImageDrawScope(RenderInterface& render, Image* image) :
		mRender(render), mPrevious(render.mDrawSurface) { render.SelectDrawSurface(image); }
	~GpuImageDrawScope() noexcept(false) { mRender.SelectDrawSurface(mPrevious); }
	GpuImageDrawScope(const GpuImageDrawScope&) = delete;
	GpuImageDrawScope& operator=(const GpuImageDrawScope&) = delete;
};

class GpuImageSamplingScope
{
	RenderInterface& mRender;
	uint32_t mPrevious;
public:
	GpuImageSamplingScope(RenderInterface& render, uint32_t flags) :
		mRender(render), mPrevious(render.sImageSamplingFlags) { render.sImageSamplingFlags = flags; }
	~GpuImageSamplingScope() { mRender.sImageSamplingFlags = mPrevious; }
	GpuImageSamplingScope(const GpuImageSamplingScope&) = delete;
	GpuImageSamplingScope& operator=(const GpuImageSamplingScope&) = delete;
};

}
