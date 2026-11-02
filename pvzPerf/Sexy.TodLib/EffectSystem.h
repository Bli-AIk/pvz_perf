#pragma once
#include "DataArray.h"
#include "ConstEnums.h"
#include "graphics/SWTri.h"
#include "graphics/Graphics.h"
#include "graphics/TextureSource.h"
#include "graphics/TextureRegion.h"
#include <memory>
#include <array>
#include <vector>
using namespace Sexy;

class TodTriVertex
{
public:
    float                       x;
    float                       y;
    float                       u;
    float                       v;
    unsigned int               color;
};

class TodTriangleGroup
{
public:
    TextureSource               mTexture;
    std::vector<std::array<TriVertex,3>> mVertArray;
#if defined(PVZ_GPU_VULKAN) || defined(PVZ_GPU_D3D12) || defined(PVZ_GPU_OPENGL)
    std::vector<std::array<TriVertex,4>> mQuadArray;
#endif
    int                         mDrawMode;
    Rect                        mClipRect{};

    TodTriangleGroup();
    void                        DrawGroup(Graphics* g);
    void                        AddTriangle(Graphics* g, Image* theImage, const SexyMatrix3& theMatrix, const Rect& theClipRect, const Color& theColor, int theDrawMode, const Rect& theSrcRect);
    void                        AddTriangle(Graphics* g, const TextureSource& texture, const SexyMatrix3& theMatrix, const Rect& theClipRect, const Color& theColor, int theDrawMode, const TextureRegion& theSrcRect);
};

extern bool gTodTriangleDrawAdditive;  //0x6A9EF5

extern void TodDrawTriangle_8888_TEX1_TALPHA0_MOD0_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_8888_TEX1_TALPHA0_MOD0_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_8888_TEX1_TALPHA0_MOD0_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_8888_TEX1_TALPHA0_MOD0_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_8888_TEX1_TALPHA0_MOD1_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_8888_TEX1_TALPHA0_MOD1_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_8888_TEX1_TALPHA0_MOD1_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_8888_TEX1_TALPHA0_MOD1_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_8888_TEX1_TALPHA1_MOD0_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_8888_TEX1_TALPHA1_MOD0_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_8888_TEX1_TALPHA1_MOD0_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_8888_TEX1_TALPHA1_MOD0_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_8888_TEX1_TALPHA1_MOD1_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_8888_TEX1_TALPHA1_MOD1_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_8888_TEX1_TALPHA1_MOD1_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_8888_TEX1_TALPHA1_MOD1_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA0_MOD0_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA0_MOD0_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA0_MOD0_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA0_MOD0_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA0_MOD1_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA0_MOD1_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA0_MOD1_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA0_MOD1_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD0_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD0_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD0_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD0_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD1_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD1_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD1_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD1_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA0_MOD0_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA0_MOD0_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA0_MOD0_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA0_MOD0_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA0_MOD1_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA0_MOD1_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA0_MOD1_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA0_MOD1_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD0_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD0_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD0_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD0_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD1_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD1_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD1_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD1_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA0_MOD0_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA0_MOD0_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA0_MOD0_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA0_MOD0_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA0_MOD1_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA0_MOD1_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA0_MOD1_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA0_MOD1_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA1_MOD0_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA1_MOD0_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA1_MOD0_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA1_MOD0_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA1_MOD1_GLOB0_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA1_MOD1_GLOB0_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA1_MOD1_GLOB1_BLEND0(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0555_TEX1_TALPHA1_MOD1_GLOB1_BLEND1(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA0_MOD0_GLOB1_BLEND0_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA0_MOD0_GLOB1_BLEND1_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA0_MOD1_GLOB0_BLEND0_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA0_MOD1_GLOB0_BLEND1_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA0_MOD1_GLOB1_BLEND0_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA0_MOD1_GLOB1_BLEND1_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD0_GLOB0_BLEND0_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD0_GLOB0_BLEND1_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD0_GLOB1_BLEND0_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD0_GLOB1_BLEND1_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD1_GLOB0_BLEND0_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD1_GLOB0_BLEND1_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD1_GLOB1_BLEND0_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0888_TEX1_TALPHA1_MOD1_GLOB1_BLEND1_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA0_MOD0_GLOB1_BLEND0_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA0_MOD0_GLOB1_BLEND1_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA0_MOD1_GLOB0_BLEND0_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA0_MOD1_GLOB0_BLEND1_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA0_MOD1_GLOB1_BLEND0_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA0_MOD1_GLOB1_BLEND1_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD0_GLOB0_BLEND0_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD0_GLOB0_BLEND1_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD0_GLOB1_BLEND0_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD0_GLOB1_BLEND1_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD1_GLOB0_BLEND0_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD1_GLOB0_BLEND1_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD1_GLOB1_BLEND0_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);
extern void TodDrawTriangle_0565_TEX1_TALPHA1_MOD1_GLOB1_BLEND1_ADDITIVE(SWHelper::SWVertex* pVerts, void* pFrameBuffer, const unsigned int bytepitch, const SWHelper::SWTextureInfo* textureInfo, SWHelper::SWDiffuse& globalDiffuse);

class Reanimation;
class TodParticleData;
class TodParticleHolder;
class TodParticleSystem;
class ReanimationHolder;
class AttachmentHolder;
class EffectSystem
{
private:
    std::unique_ptr<TodParticleData>    mParticleData;

public:
    // 裸 new/delete 改 unique_ptr 托管；析构顺序仍为 particle→reanim→attachment
    //（EffectSystemDispose 显式按序 reset，先于成员逆序析构生效）。
    std::unique_ptr<TodParticleHolder>  mParticleHolder;
    std::unique_ptr<ReanimationHolder>  mReanimationHolder;
    std::unique_ptr<AttachmentHolder>   mAttachmentHolder;

public:
    EffectSystem() = default;
    ~EffectSystem();
    EffectSystem(const EffectSystem&) = delete;
    EffectSystem& operator=(const EffectSystem&) = delete;

    void                        EffectSystemInitialize();
    void                        EffectSystemDispose();
    void                        EffectSystemFreeAll();
    void                        ProcessDeleteQueue();
    void                        UpdateAt(std::int64_t visualNowNs, std::int64_t battleNowNs);
    void                        UpdateAt(std::int64_t visualNowNs, std::int64_t battleNowNs, std::int64_t poolNowNs);
    void                        RebaseIndependentBattleEffects(std::int64_t battleNowNs);
};
extern EffectSystem* gEffectSystem;  //[0x6A9EB8]
