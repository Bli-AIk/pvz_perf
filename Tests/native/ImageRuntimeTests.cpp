#include "Lawn/BoardTouch.h"
#include "Lawn/System/SaveGame.h"
#include "ReanimClock.h"
#include "SexyAppFramework/SexyAppBase.h"
#include "SexyAppFramework/graphics/DDInterface.h"
#include "SexyAppFramework/graphics/Graphics.h"
#include "SexyAppFramework/graphics/AtlasDraw.h"
#include "SexyAppFramework/graphics/ImageFont.h"
#include "SexyAppFramework/graphics/RenderInterface.h"
#include "SexyAppFramework/paklib/PakInterface.h"
#include "Sexy.TodLib/TodCommon.h"
#include "Sexy.TodLib/TodParticle.h"
#include "ParticleDrawReference.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/Attachment.h"
#include "Sexy.TodLib/Definition.h"
#include "Sexy.TodLib/generated/GeneratedDefinitions.h"
#include "Sexy.TodLib/generated/NumericResourceBindings.h"
#include "Sexy.TodLib/generated/SceneAtlasData.h"
#include "Resources.h"
#include "LawnApp.h"
#include "Lawn/Board.h"
#include "Lawn/MessageWidget.h"
#include "Lawn/Challenge.h"
#include "Lawn/CutScene.h"
#include "Lawn/GridItem.h"
#include "Lawn/Coin.h"
#include "Lawn/ZenGarden.h"
#include "Lawn/Projectile.h"
#include "Lawn/Plant.h"
#include "Lawn/ProjectileResources.h"
#include "Lawn/Zombie.h"
#include "Lawn/SeedPacket.h"
#include "Lawn/CursorObject.h"
#include "Lawn/LawnMower.h"
#include "Lawn/Widget/GameButton.h"
#include "SexyAppFramework/widget/WidgetManager.h"
#include "SexyAppFramework/widget/WidgetLifetimeHold.h"
#include "Lawn/System/PlayerInfo.h"
#include "Lawn/System/Music.h"
#include "Lawn/System/ReanimationLawn.h"
#include "Lawn/Widget/StoreScreen.h"
#include "Lawn/Widget/SeedChooserScreen.h"
#include "Lawn/Widget/TitleScreen.h"
#include "Lawn/Widget/CreditScreen.h"
#include "Lawn/Widget/LawnDialog.h"
#include "Lawn/Widget/AchievementsWidget.h"
#include "Lawn/Widget/Almanac.h"
#include "Lawn/Widget/AwardScreen.h"
#include "Lawn/Widget/ChallengeScreen.h"
#include "Lawn/Widget/GameSelector.h"
#include <chrono>
#include "SexyAppFramework/widget/HyperlinkWidget.h"
#include "SexyAppFramework/widget/ListWidget.h"
#include "SexyAppFramework/widget/ListListener.h"
#include "SexyAppFramework/widget/ButtonWidget.h"
#include "SexyAppFramework/widget/ButtonListener.h"
#include "SexyAppFramework/misc/MTRand.h"
#include "Sexy.TodLib/TodFoley.h"
#include "SexyAppFramework/imagelib/ImageLib.h"
#include "SexyAppFramework/sound/SoundManager.h"
#include "SexyAppFramework/sound/SoundDecode.h"
#include "SexyAppFramework/sound/DummyMusicInterface.h"
#include "PoolGpuChecks.h"
#include "ParticleRuntimeDrawChecks.h"
#include "Lawn/System/PoolEffect.h"
#include "Lawn/System/V7SaveSchema.h"
#include "Lawn/System/SaveClockProjection.h"
#include "QuitDialogRuntimeChecks.h"
#include "OpeningFlowRuntimeChecks.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <DbgHelp.h>
#pragma comment(lib, "dbghelp.lib")
#include <stdexcept>
#include <string_view>
#include <vector>

// The normal application entry point defines this callback. This executable
// links the same runtime objects with a separate, hidden test entry point.
bool FixtureCloseRequest() { return false; }
bool (*gAppCloseRequest)() = FixtureCloseRequest;

namespace
{
using namespace Sexy;
bool gPlantCatalogOnly=false;
bool gZombieCatalogOnly=false;
bool gIce3Only=false;
bool gMinigameRulesOnly=false;
bool gPuzzleRulesOnly=false;
bool gSurvivalRulesOnly=false;
bool gAdventureRulesOnly=false;
bool gHiddenRulesOnly=false;
bool gGardenRulesOnly=false;
bool gIntroRulesOnly=false;
bool gUiRulesOnly=false;
bool gAwardRulesOnly=false;
bool gAlmanacClockOnly=false;
bool gActorBirthOnly=false;
bool gLossRulesOnly=false;
bool gOpeningRulesOnly=false;
bool gLifecycleRulesOnly=false;
bool gGraveRulesOnly=false;
bool gHudSeamsOnly=false;
bool gTouchOnly=false;
bool gTouchPerf=false;

LONG CALLBACK SuppressFixtureCrashDialogs(EXCEPTION_POINTERS* exception)
{
    const DWORD code=exception->ExceptionRecord->ExceptionCode;
    if (code==EXCEPTION_ACCESS_VIOLATION || code==EXCEPTION_IN_PAGE_ERROR ||
        code==EXCEPTION_ILLEGAL_INSTRUCTION || code==EXCEPTION_STACK_OVERFLOW ||
        code==EXCEPTION_INT_DIVIDE_BY_ZERO)
    {
        std::fprintf(stderr,"FAIL image runtime fixture: hardware exception %08lX\n",code);
        SymInitialize(GetCurrentProcess(), nullptr, TRUE);
        alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
        auto* symbol=reinterpret_cast<SYMBOL_INFO*>(storage);
        symbol->SizeOfStruct=sizeof(SYMBOL_INFO);
        symbol->MaxNameLen=MAX_SYM_NAME;
        DWORD64 displacement=0;
        if (SymFromAddr(GetCurrentProcess(),
            reinterpret_cast<DWORD64>(exception->ExceptionRecord->ExceptionAddress),&displacement,symbol))
            std::fprintf(stderr,"Fault: %s + %llX\n",symbol->Name,displacement);
        void* frames[24]{};
        const auto count=CaptureStackBackTrace(0,24,frames,nullptr);
        for (USHORT frame=0;frame<count;++frame)
            if (SymFromAddr(GetCurrentProcess(),reinterpret_cast<DWORD64>(frames[frame]),&displacement,symbol))
                std::fprintf(stderr,"Frame: %s + %llX\n",symbol->Name,displacement);
        std::fflush(stderr);
        TerminateProcess(GetCurrentProcess(),code);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

class TestApp final : public LawnApp
{
public:
    using SexyAppBase::ProcessMessageNow;
    using SexyAppBase::ProcessSafeDeleteList;
    std::function<bool(bool*)> fixtureNestedPump;
    bool UpdateAppStep(bool* updated) override {
        return fixtureNestedPump?fixtureNestedPump(updated):LawnApp::UpdateAppStep(updated);
    }
    void ButtonPress(int) override {}
    void ButtonDepress(int) override {}
    void ButtonDownTick(int) override {}
    void ButtonMouseEnter(int) override {}
    void ButtonMouseLeave(int) override {}
    void ButtonMouseMove(int,int,int) override {}
};

void Require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

class HiddenWindow
{
public:
    HWND handle = nullptr;
    HiddenWindow()
    {
        WNDCLASSW type{};
        type.style=CS_OWNDC;
        type.lpfnWndProc=DefWindowProcW;
        type.hInstance=GetModuleHandleW(nullptr);
        type.lpszClassName=L"PvzImageRuntimeTests";
        Require(RegisterClassW(&type)!=0,"Cannot register hidden test window");
        handle=CreateWindowExW(0,type.lpszClassName,L"Image runtime tests",
            WS_POPUP,0,0,64,64,nullptr,nullptr,type.hInstance,nullptr);
        Require(handle!=nullptr,"Cannot create hidden test window");
    }
    ~HiddenWindow()
    {
        if (handle) DestroyWindow(handle);
        UnregisterClassW(L"PvzImageRuntimeTests",GetModuleHandleW(nullptr));
    }
};

void Pixel(const ImagePixels& image,int x,unsigned expected,int tolerance=1)
{
    const auto actual=image.pixels.at(x);
    for (int shift:{0,8,16,24})
        Require(std::abs(int((actual>>shift)&255)-int((expected>>shift)&255))<=tolerance,
            "Public image draw lost alpha or produced incorrect colors");
}

void SourceRectangleDraws(SexyAppBase& app)
{
    auto& render=*app.mDDInterface->mRenderInterface;
    Image source(&app);
    // A nonzero origin on both axes, four distinguishable pixels, and a
    // conspicuous surrounding border catch source-offset and UV mistakes.
    std::array<unsigned,30> pixels;
    pixels.fill(0xffff00ffu);
    constexpr std::array<unsigned,4> colors{
        0xffff0000u,0xff00ff00u,0xff0000ffu,0xffffffffu};
    pixels[8]=colors[0];pixels[9]=colors[1];
    pixels[14]=colors[2];pixels[15]=colors[3];
    source.SetBits(pixels.data(),6,5);
    const Rect region(2,1,2,2);
    constexpr int targetWidth=18,targetHeight=16;
    std::unique_ptr<Image> target(render.CreateRenderImage(targetWidth,targetHeight));

    enum Method { Direct, Float, Scaled, ScaledFloat, CenterScaledFloat,
        Mirror, ScaledMirror, ScaledFloatUnit, CenterScaledFloatUnit };
    for (const auto method:{Direct,Float,Scaled,ScaledFloat,CenterScaledFloat,
        Mirror,ScaledMirror,ScaledFloatUnit,CenterScaledFloatUnit})
    {
        const bool scaled=method==Scaled || method==ScaledFloat ||
            method==CenterScaledFloat || method==ScaledMirror;
        const bool mirrored=method==Mirror || method==ScaledMirror;
        const int scaleX=scaled ? 2 : 1,scaleY=scaled ? 3 : 1;
        // Scaling around the original 2x2 center moves the upper-left corner
        // left by one and up by two. Other helpers keep the requested origin.
        const int left=method==CenterScaledFloat ? 6 : 7;
        const int top=method==CenterScaledFloat ? 5 : 7;
        {
            GpuImageDrawScope destination(render,target.get());
            Graphics graphics(target.get());
            graphics.mColor=Color::Black;
            graphics.FillRect(Rect(0,0,targetWidth,targetHeight));
            graphics.Translate(1,2);
            graphics.mFastStretch=true;
            graphics.mLinearBlend=false;
            switch (method)
            {
            case Direct: graphics.DrawImage(&source,6,5,region); break;
            case Float: TodDrawImageRectF(&graphics,&source,6.f,5.f,region); break;
            case Scaled: TodDrawImageRectScaled(&graphics,&source,6,5,region,2,3); break;
            case ScaledFloat: TodDrawImageRectScaledF(&graphics,&source,6.f,5.f,region,2,3); break;
            case CenterScaledFloat: TodDrawImageRectCenterScaledF(&graphics,&source,6.f,5.f,region,2,3); break;
            case Mirror: graphics.DrawImageMirror(&source,6,5,region); break;
            case ScaledMirror: graphics.DrawImageMirror(&source,Rect(6,5,4,6),region); break;
            case ScaledFloatUnit: TodDrawImageRectScaledF(&graphics,&source,6.f,5.f,region,1,1); break;
            case CenterScaledFloatUnit: TodDrawImageRectCenterScaledF(&graphics,&source,6.f,5.f,region,1,1); break;
            }
        }
        render.Flush();
        const auto actual=render.ReadImagePixels(*target);
        // Check the full target, including untouched pixels around every edge.
        // The oracle is a CPU grid, independent of renderer drawing methods.
        for (int y=0;y<targetHeight;++y)
            for (int x=0;x<targetWidth;++x)
            {
                unsigned expected=0xff000000u;
                if (x>=left && x<left+2*scaleX && y>=top && y<top+2*scaleY)
                {
                    const int column=(x-left)/scaleX;
                    const int row=(y-top)/scaleY;
                    expected=colors[row*2+(mirrored ? 1-column : column)];
                }
                try { Pixel(actual,y*targetWidth+x,expected); }
                catch (...)
                {
                    std::cerr<<"Rectangle method "<<method<<" mismatched pixel "<<x<<","<<y<<"\n";
                    throw;
                }
            }
    }
    // Repack the entire 6x5 sheet at a second nonzero origin. Its internal
    // 2x2 view must preserve all existing source-relative draw semantics.
    Image packed(&app);
    std::array<unsigned,90> packedPixels;
    packedPixels.fill(0xff00ffffu);
    for (int y=0;y<5;++y)
        for (int x=0;x<6;++x)
            packedPixels[(y+2)*10+x+1]=pixels[y*6+x];
    packed.SetBits(packedPixels.data(),10,9);
    {
        GpuImageDrawScope destination(render,target.get());
        Graphics graphics(target.get());
        graphics.DrawImage(&packed,0,0);
    }
    render.Flush();
    Require(packed.mTexture!=nullptr,"Atlas test sheet was not uploaded");
    TextureSource packedTexture{packed.mTexture,0};
    const TextureRegion packedSheet{1.f/10,2.f/9,7.f/10,7.f/9,6,5};
    const TextureRegion packedView{3.f/10,3.f/9,5.f/10,5.f/9,2,2};
    std::unique_ptr<Image> expectedTarget(render.CreateRenderImage(targetWidth,targetHeight));
    for (int method=0;method<4;++method)
        for (int style=0;style<4;++style)
            for (bool fast:{false,true})
            {
                const auto draw=[&](Image* output,bool useAtlas) {
                    GpuImageDrawScope destination(render,output);
                    Graphics graphics(output);
                    graphics.mColor=Color(11,17,23);
                    graphics.FillRect(Rect(0,0,targetWidth,targetHeight));
                    graphics.ClipRect(5,7,6,6);
                    graphics.Translate(1,2);
                    graphics.mFastStretch=fast;
                    graphics.mLinearBlend=!fast;
                    graphics.mColorizeImages=(style&1)!=0;
                    graphics.mColor=Color(113,197,83,137);
                    graphics.mDrawMode=(style&2) ? Graphics::DRAWMODE_ADDITIVE :
                        Graphics::DRAWMODE_NORMAL;
                    // Nearest sampling has no unique neighbor exactly at a
                    // texel boundary when repacking changes the normalized UV
                    // denominator; this is not a pixel-copy difference. Even
                    // nearest raster extents avoid ties. Keep the original
                    // fractional extents for linear filtering.
                    if (method==1) graphics.SetScale(fast ? 1.75f : 1.5f,
                        fast ? 2.75f : 2.5f,3.f,2.f);
                    if (method<=1)
                    {
                        if (useAtlas) DrawAtlasImage(&graphics,packedTexture,packedView,6.f,5.f);
                        else graphics.DrawImage(&source,6,5,region);
                    }
                    else if (method==2)
                    {
                        const Rect outputRectangle(3,4,fast ? 8 : 7,8);
                        if (useAtlas)
                            DrawAtlasImage(&graphics,packedTexture,packedSheet,outputRectangle,region);
                        else graphics.DrawImage(&source,outputRectangle,region);
                    }
                    else
                    {
                        // A half-pixel origin with scale 2 puts nearest samples
                        // exactly on a texel boundary. Different normalized UV
                        // denominators may choose either neighbor; keep this
                        // strict comparison off that tie on every backend.
                        const float y = fast ? 3.25f : 3.5f;
                        if (useAtlas)
                            DrawAtlasImageScaled(&graphics,packedTexture,packedView,4.25f,y,1.5f,2.f);
                        else TodDrawImageRectScaledF(&graphics,&source,4.25f,y,region,1.5f,2.f);
                    }
                };
                draw(expectedTarget.get(),false);
                draw(target.get(),true);
                render.Flush();
                const auto expected=render.ReadImagePixels(*expectedTarget);
                const auto actual=render.ReadImagePixels(*target);
                bool touched=false;
                for (int pixel=0;pixel<targetWidth*targetHeight;++pixel)
                {
                    touched=touched || expected.pixels[pixel]!=0xff0b1117u;
                    try { Pixel(actual,pixel,expected.pixels[pixel]); }
                    catch (...)
                    {
                        std::cerr<<"Atlas helper method "<<method<<" style "<<style
                            <<" fast "<<fast<<" mismatched pixel "<<pixel<<"\n";
                        throw;
                    }
                }
                Require(touched,"Atlas helper comparison clipped away the complete draw");
            }
    std::cout<<"PASS nonzero source rectangles and repacked whole-sheet atlas views: translation, "
        "scale origins, clipping, tint, additive, local-source views and floating scales\n";
}


void NecessaryLocalClipDraws(SexyAppBase& app)
{
    auto& render=*app.mDDInterface->mRenderInterface;
    constexpr int width=16,height=12;
    constexpr unsigned background=0xff030507u, solid=0xff113355u;
    std::array<unsigned,8> colors{0xffff0000u,0xff00ff00u,0xff0000ffu,0xffffffffu,
        0xffffff00u,0xffff00ffu,0xff00ffffu,0xff805020u};
    Image source(&app);
    source.SetBits(colors.data(),4,2);
    const Rect region(1,0,2,2);
    const TextureRegion uv{.25f,0,.75f,1,2,2};
    std::unique_ptr<Image> target(render.CreateRenderImage(width,height));
    enum Method { Fill,Outline,FullImage,SourceImage,MirrorImage,MatrixImage,
        MatrixTexture,ParticleQuad,ConvexPolygon };
    for(const auto method:{Fill,Outline,FullImage,SourceImage,MirrorImage,MatrixImage,
        MatrixTexture,ParticleQuad,ConvexPolygon})
    for(const auto origin:{Point(2,3),Point(-1,-1)})
    for(const int clipCase:{0,1,2,3})
    {
        const bool empty=clipCase>=2;
        const Rect clip=clipCase==0 ? Rect(10,8,1,1) : clipCase==1 ?
            Rect(origin.mX+1,origin.mY+1,1,1) : clipCase==2 ? Rect(0,0,0,4) : Rect(0,0,4,0);
        {
            GpuImageDrawScope destination(render,target.get());
            Graphics g(target.get());
            g.mColor=Color(3,5,7,255);
            g.FillRect(0,0,width,height);
            g.mClipRect=clip;
            g.mColor=Color(17,51,85,255);
            g.mLinearBlend=false;g.mFastStretch=true;
            switch(method)
            {
            case Fill:g.FillRect(origin.mX,origin.mY,4,2);break;
            case Outline:g.DrawRect(origin.mX,origin.mY,4,2);break;
            case FullImage:g.DrawImage(&source,origin.mX,origin.mY);break;
            case SourceImage:g.DrawImage(&source,origin.mX,origin.mY,region);break;
            case MirrorImage:g.DrawImageMirror(&source,origin.mX,origin.mY,region,true);break;
            case MatrixImage:case MatrixTexture:case ParticleQuad:
            {
                SexyMatrix3 matrix;matrix.LoadIdentity();
                matrix.m02=origin.mX+1;matrix.m12=origin.mY+1;
                const TextureSource texture{render.CreateImageTexture(&source),0};
                if(method==MatrixImage)
                    TodBltMatrix(&g,&source,matrix,clip,Color::White,g.mDrawMode,region);
                else if(method==MatrixTexture)
                    TodBltMatrix(&g,texture,matrix,clip,Color::White,g.mDrawMode,uv);
                else {
                    TodTriangleGroup triangles;
                    triangles.AddTriangle(&g,texture,matrix,clip,Color::White,g.mDrawMode,uv);
                    triangles.DrawGroup(&g);
                }
                break;
            }
            case ConvexPolygon:
            {
                const Point polygon[]{origin,Point(origin.mX+4,origin.mY),
                    Point(origin.mX+4,origin.mY+2),Point(origin.mX,origin.mY+2)};
                g.PolyFill(polygon,4,true);
                break;
            }
            }
        }
        render.Flush();
        const auto actual=render.ReadImagePixels(*target);
        for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
            unsigned expected=background;
            const int column=x-origin.mX,row=y-origin.mY;
            if(!empty && x>=clip.mX && x<clip.mX+clip.mWidth &&
                y>=clip.mY && y<clip.mY+clip.mHeight) {
                if(method==Outline) {
                    if(column>=0 && column<=4 && row>=0 && row<=2 &&
                        (column==0 || column==4 || row==0 || row==2)) expected=solid;
                } else {
                    const bool plain=method==FullImage;
                    const bool shape=method==Fill || method==ConvexPolygon;
                    const int drawnWidth=shape || plain ? 4 : 2;
                    if(column>=0 && column<drawnWidth && row>=0 && row<2)
                        expected=shape ? solid : colors[row*4+(plain ? column :
                            1+(method==MirrorImage ? 1-column : column))];
                }
            }
            try {Pixel(actual,y*width+x,expected,0);}
            catch(...) {
                std::cerr<<"Necessary-local-clip method="<<method<<" case="<<clipCase
                    <<" origin="<<origin.mX<<','<<origin.mY<<" pixel="<<x<<','<<y<<'\n';
                throw;
            }
        }
    }
    // A native line's edge-coverage convention is backend-specific. Compare
    // exact bytes with the same geometry under the full target clip, and require
    // that reference to be visible; no tolerance or relaxed pixel comparison.
    for(const bool negative:{false,true}) {
        const auto line=[&](const Rect& clip,bool explicitClippedEndpoints=false) {
            {
                GpuImageDrawScope destination(render,target.get());
                Graphics g(target.get());
                g.mColor=Color(3,5,7,255);g.FillRect(0,0,width,height);
                g.mClipRect=clip;g.mColor=Color::White;
                g.DrawLine(explicitClippedEndpoints ? 4 : negative ? -3 : 2,4,explicitClippedEndpoints ? 5 : 8,4);
                g.DrawLineAA(explicitClippedEndpoints ? 4 : negative ? -2 : 3,7,explicitClippedEndpoints ? 5 : 9,7);
            }
            render.Flush();
            return render.ReadImagePixels(*target).pixels;
        };
        const auto reference=line(Rect(0,0,width,height));
        Require(std::any_of(reference.begin(),reference.end(),[](unsigned p){return p!=background;}),
            "Unclipped line reference did not draw");
        const auto outside=line(Rect(12,9,1,1));
        Require(std::all_of(outside.begin(),outside.end(),[](unsigned p){return p==background;}),
            "Disjoint clip did not suppress native lines");
        const Rect partial(4,0,2,height);
        const auto clipped=line(partial);
        // Legacy line clipping ends at the last integer coordinate (5),
        // while native triangle coverage excludes that endpoint. Compare to
        // independently submitted explicit endpoints, not a scissored long line.
        Require(clipped==line(Rect(0,0,width,height),true),
            "Partial line clip differs from its exact historical endpoint geometry");
        const auto hidden=line(Rect(0,0,0,height));
        Require(std::all_of(hidden.begin(),hidden.end(),[](unsigned p){return p==background;}),
            "Empty clip did not suppress line drawing");
    }
    std::cout<<"PASS necessary local-clip contract: independent full-pixel rect/image/source/mirror/"
        "matrix/particle/convex oracles, disjoint/partial/empty clips and negative framebuffer crossings\n";
}

void AtlasButtonStates(SexyAppBase& app)
{
    auto& render=*app.mDDInterface->mRenderInterface;
    std::array<Image,5> original{Image(&app),Image(&app),Image(&app),Image(&app),Image(&app)};
    constexpr std::array<unsigned,5> colors{
        0xffb02010u,0xff20a030u,0xff3040d0u,0xffb09020u,0x80607080u};
    std::array<unsigned,132> packedPixels;
    packedPixels.fill(0xffff00ffu);
    std::array<Rect,5> rectangles;
    std::array<TextureRegion,5> regions;
    for (int state=0;state<5;++state)
    {
        unsigned pixels[]{colors[state],colors[state]^0x00101010u,
            colors[state]^0x00080808u,colors[state]};
        original[state].SetBits(pixels,2,2);
        rectangles[state]=Rect(2+state*4,2,2,2);
        regions[state]=PixelSourceToUv(rectangles[state],22,6);
        for (int y=0;y<2;++y)
            for (int x=0;x<2;++x)
                packedPixels[(2+y)*22+2+state*4+x]=pixels[y*2+x];
    }
    Image packed(&app);
    packed.SetBits(packedPixels.data(),22,6);
    std::unique_ptr<Image> actualTarget(render.CreateRenderImage(12,10));
    std::unique_ptr<Image> expectedTarget(render.CreateRenderImage(12,10));
    {
        GpuImageDrawScope destination(render,actualTarget.get());
        Graphics graphics(actualTarget.get());
        graphics.DrawImage(&packed,0,0);
    }
    render.Flush();
    TextureSource texture{packed.mTexture,0};
    struct MouseRestore {
        WidgetManager& manager;
        int x,y;
        ~MouseRestore() { manager.mLastMouseX=x;manager.mLastMouseY=y; }
    } restore{*app.mWidgetManager,app.mWidgetManager->mLastMouseX,app.mWidgetManager->mLastMouseY};
    // Normal, hover, explicit press, disabled, fading highlight, glow,
    // pressed glow, pressed hover fallback, normal fallback, and hidden.
    for (int state=0;state<10;++state)
    {
        GameButton oldButton(100),atlasButton(101);
        for (GameButton* button:{&oldButton,&atlasButton})
        {
            button->mApp=static_cast<LawnApp*>(&app);
            button->mX=4;button->mY=3;button->mWidth=2;button->mHeight=2;
            button->mLabel.clear();
            button->mButtonOffsetX=1;
            button->mButtonOffsetY=1;
            button->mIsOver=state==2 || state==6 || state==7 || state==8;
            button->mIsDown=button->mIsOver;
            button->mDisabled=state==3;
            button->mOverAlpha=state==4 ? 0.4 : 0;
            button->mBtnNoDraw=state==9;
        }
        const bool hovered=state==1 || state==2 || state==5 ||
            state==6 || state==7 || state==8;
        app.mWidgetManager->mLastMouseX=hovered ? 5 : -100;
        app.mWidgetManager->mLastMouseY=hovered ? 4 : -100;
        oldButton.mButtonImage=&original[0];
        oldButton.mOverImage=&original[1];
        oldButton.mDownImage=&original[2];
        oldButton.mDisabledImage=&original[3];
        atlasButton.mNormalRegion=PixelSourceToUv(rectangles[0],22,6);
        atlasButton.mOverRegion=PixelSourceToUv(rectangles[1],22,6);
        atlasButton.mDownRegion=PixelSourceToUv(rectangles[2],22,6);
        atlasButton.mDisabledRegion=PixelSourceToUv(rectangles[3],22,6);
        if (state==7 || state==8)
        {
            oldButton.mDownImage=nullptr;
            atlasButton.mDownRegion=TextureRegion{};
        }
        if (state==8)
        {
            oldButton.mOverImage=nullptr;
            atlasButton.mOverRegion=TextureRegion{};
        }
        const bool glow=state==5 || state==6;
        oldButton.mOverOverlayImage=glow ? &original[4] : nullptr;
        const auto draw=[&](Image* target,bool useAtlas) {
            GpuImageDrawScope destination(render,target);
            Graphics graphics(target);
            graphics.mColor=Color(11,17,23);
            graphics.FillRect(Rect(0,0,12,10));
            graphics.TranslateF(1.1f,2.3f);
            graphics.mFastStretch=true;
            if (useAtlas)
                atlasButton.DrawAtlas(&graphics,texture,glow ? &regions[4] : nullptr);
            else oldButton.Draw(&graphics);
            Require(graphics.mTransX==1.1f && graphics.mTransY==2.3f,
                "Button drawing failed to restore the caller's translation");
        };
        draw(expectedTarget.get(),false);
        draw(actualTarget.get(),true);
        render.Flush();
        const auto expected=render.ReadImagePixels(*expectedTarget);
        const auto actual=render.ReadImagePixels(*actualTarget);
        bool touched=false;
        for (int pixel=0;pixel<120;++pixel)
        {
            touched=touched || expected.pixels[pixel]!=0xff0b1117u;
            try { Pixel(actual,pixel,expected.pixels[pixel]); }
            catch (...)
            {
                std::cerr<<"GameButton atlas state "<<state<<" mismatched pixel "<<pixel<<"\n";
                throw;
            }
        }
        Require(touched==(state!=9),"Button state comparison had no visible coverage");
    }
    std::cout<<"PASS actual GameButton atlas states: normal, hover, press/fallbacks, disabled, "
        "overAlpha, additive glow, translation/offsets and hidden state match independent images\n";
}

void WrappedTextMeasurements()
{
    struct CountingFont final : _Font {
        int draws=0;
        CountingFont() { mAscent=7;mHeight=10;mLineSpacingOffset=2; }
        int CharWidth(SexyChar ch) override { return ch=='\n' ? 0 : ch==' ' ? 3 : 5; }
        int StringWidth(const SexyString& text) override {
            int width=0;
            for (auto ch:text) width+=CharWidth(ch);
            return width;
        }
        void DrawString(Graphics*,int,int,const SexyString&,const Color&,const Rect&) override { ++draws; }
        _Font* Duplicate() override { return new CountingFont(*this); }
    } font;
    for (bool colored:{false,true})
        for (int width:{5,20,60})
            for (const SexyString text:{"","word","word word word","longunbrokenword",
                "first\nsecond\n","^ff0000^colored words","many   spaces"})
            {
                Graphics measure(nullptr),drawing(nullptr);
                measure.mFont=drawing.mFont=&font;
                measure.mWriteColoredString=drawing.mWriteColoredString=colored;
                measure.mColor=drawing.mColor=Color(17,31,47,191);
                drawing.mClipRect=Rect(0,0,10000,10000);
                int measuredWidth=0,drawnWidth=0;
                font.draws=0;
                int standaloneWidth=0;
                const int standaloneHeight=Graphics::MeasureWordWrappedHeight(
                    font,width,text,-1,&standaloneWidth,colored);
                Require(font.draws==0,"Standalone text measurement invoked glyph drawing");
                const int height=measure.GetWordWrappedHeight(width,text,-1,&measuredWidth);
                Require(font.draws==0,"Wrapped text measurement invoked glyph drawing");
                const int reference=drawing.WriteWordWrapped(Rect(0,0,width,0),text,-1,-1,&drawnWidth);
                Require(height==reference && standaloneHeight==reference &&
                    standaloneWidth==drawnWidth && measuredWidth==drawnWidth &&
                    measure.mColor==Color(17,31,47,191),
                    "Pure wrapped measurement changed line breaking, width or color state");
            }
    Graphics invisible(nullptr);
    invisible.mColor=Color::White;
    invisible.FillRect(0,0,20,20);
    std::cout<<"PASS pure wrapped text measurement: wrapping, newlines, color tags, no glyph draws and empty fill\n";
}

void FontUploadAndCopies(SexyAppBase& app)
{
    auto& render=*app.mDDInterface->mRenderInterface;
    Image source(&app);
    unsigned pixels[]{0x00ffffffu,0x80ffffffu,0xffffffffu};
    source.SetBits(pixels,3,1,false);
    Require(source.mBitsChanged && !source.mTexture,"Source was not deferred");

    ImageFont font(&source);
    auto& layer=font.mFontData->mFontLayerList.front();
    layer.mPointSize=0;
    layer.mAscent=1;
    layer.mHeight=1;
    auto& glyph=layer.mCharData['A'];
    glyph.mImageRect={0,0,1,1,3,1};
    glyph.mWidth=3;
    {
        Graphics measuring(nullptr);
        int measuredWidth=0;
        font.DrawStringEx(&measuring,0,1,"A",Color::White,nullptr,&measuredWidth);
        Require(measuredWidth==3 && !source.mTexture,
            "Text measurement required a GPU draw target or uploaded glyphs");
    }

    Image* target=render.CreateRenderImage(3,1);
    try
    {
        const auto draw=[&] {
            GpuImageDrawScope destination(render,target);
            Graphics graphics(target);
            graphics.mColor=Color::Black;
            graphics.FillRect(Rect(0,0,3,1));
            font.DrawString(&graphics,0,1,"A",Color::White,graphics.mClipRect);
        };
        draw();
        render.Flush();
        Require(source.mTexture && !source.mPendingDecoded && !source.mBitsChanged,
            "Font upload did not consume and commit the loading payload");
        Require(source.mHasTrans && source.mHasAlpha,"Font alpha flags were not analyzed");
        const auto first=render.ReadImagePixels(*target);
        Pixel(first,0,0xff000000u);
        Pixel(first,1,0xff808080u);
        Pixel(first,2,0xffffffffu);
        auto* uploaded=source.mTexture;
        draw();
        render.Flush();
        Require(source.mTexture==uploaded,"Second font draw reuploaded the source");
        Require(render.ReadImagePixels(*target).pixels==first.pixels,"Repeated font draw changed pixels");

        SexyTransform2D translated;
        translated.Translate(1,0);
        render.PushTransform(translated);
        {
            GpuImageSamplingScope filter(render,std::uint32_t(GpuImageColorFilter::White));
            const bool previousWrap=render.sSamplerAddressWrap;
            render.sSamplerAddressWrap=true;
            Image copy(*target);
            Require(copy.mRenderTarget,"Render-target copy lost writable target capability");
            Require(render.ReadImagePixels(copy).pixels==first.pixels,
                "Render-target copy inherited caller transforms or filters");
            Require(render.sImageSamplingFlags==std::uint32_t(GpuImageColorFilter::White) &&
                render.sSamplerAddressWrap,"Copy did not restore sampling state");
            render.sSamplerAddressWrap=previousWrap;
        }
        Require(render.mTransformStack.size()==1,"Copy damaged the caller transform stack");
        render.PopTransform();

        Image deferred(&app);
        deferred.SetBits(pixels,3,1,false);
        Image copy(deferred);
        Require(copy.mHasTrans && copy.mHasAlpha && !deferred.mPendingDecoded,
            "Copy retained stale deferred alpha metadata");
        Require(copy.mTexture!=deferred.mTexture,"Image copy aliased an owning GPU texture");
        Require(render.ReadImagePixels(copy).pixels==render.ReadImagePixels(deferred).pixels,
            "Image copy changed source pixels");
    }
    catch (...) { delete target; throw; }
    delete target;
    // A nonzero glyph origin exercises the UV-only persistent source through
    // the loading-time pixel-exact resampler. Its oracle supplies the original
    // integer texel coordinates directly, independently of glyph UV storage.
    {
        Image glyphSheet(&app);
        std::array<unsigned,15> sheetPixels;
        sheetPixels.fill(0xffff00ffu);
        std::copy(std::begin(pixels),std::end(pixels),sheetPixels.begin()+6);
        glyphSheet.SetBits(sheetPixels.data(),5,3,false);
        ImageFont scaledFont(&glyphSheet);
        auto& scaledLayer=scaledFont.mFontData->mFontLayerList.front();
        scaledLayer.mPointSize=0;
        scaledLayer.mAscent=1;
        scaledLayer.mHeight=1;
        auto& scaledGlyph=scaledLayer.mCharData['A'];
        scaledGlyph.mImageRect={1.f/5,1.f/3,4.f/5,2.f/3,3,1};
        scaledGlyph.mWidth=3;
        scaledFont.SetScale(2);
        scaledFont.GenerateActiveFontLayers();
        Require(scaledFont.mActiveLayerList.size()==1,"Missing scaled font UV layer");
        const auto& active=scaledFont.mActiveLayerList.front();
        Require(active.mOwnsImage && active.mScaledImage->mWidth==6 &&
            active.mScaledImage->mHeight==2,"Scaled font UV geometry changed");
        Image* expected=render.CreateRenderImage(6,2);
        try
        {
            const GpuFontGlyph oldPixelGlyph{1,1,3,1,0,0,6,2};
            render.RenderFontAtlas(expected,&glyphSheet,std::span(&oldPixelGlyph,1),false);
            render.Flush();
            Require(render.ReadImagePixels(*expected).pixels==
                render.ReadImagePixels(*active.mScaledImage).pixels,
                "Scaled font UV origin changed pixel-exact resampling");
            const auto& region=active.mScaledCharImageRects['A'];
            Require(region.mU0==0 && region.mV0==0 && region.mU1==1 && region.mV1==1 &&
                region.mWidth==6 && region.mHeight==2,"Scaled font kept pixel source coordinates");
        }
        catch (...) { delete expected; throw; }
        delete expected;
    }
    std::cout<<"PASS public font upload: deferred alpha, upload-and-discard, texture reuse, independent copies\n";
}

void SharedReferences(SexyAppBase& app)
{
    auto* source=new Image(&app);
    const unsigned before=app.mSharedImageMap.size();
    {
        auto first=app.SetSharedImage("image-runtime-fixture",0,source,nullptr);
        Require(first.mImage==source,"New shared-image reference missed the published image");
        auto second=app.GetSharedImage("image-runtime-fixture");
        Require(second.mImage==source,"Cached reference missed the direct image snapshot");
        SharedImageRef third(second);
        Require(third.mImage==source && third.mSharedImage->mRefCount==3,
            "Shared reference copy changed image or lifetime accounting");
        first=source;
        Require(first.mSharedImage && first.mImage==source,
            "Same-image rebind dropped its owning cache reference");
    }
    Require(app.mSharedImageMap.size()==before+1 &&
        app.mSharedImageMap.find({"IMAGE-RUNTIME-FIXTURE",0})->second.mRefCount==0,
        "Shared reference release lost its cache node or count");
    std::cout<<"PASS direct shared-image snapshots and reference lifetime\n";
}

void ReanimationAppearancePixels(SexyAppBase& app)
{
    auto& render=*app.mDDInterface->mRenderInterface;
    ReanimatorLoadDefinitions(gLawnReanimationArray,static_cast<int>(ReanimationType::NUM_REANIMS));
    struct CatalogCleanup { ~CatalogCleanup() { ReanimatorFreeDefinitions(); } } catalogCleanup;
    Reanimation lookup;
    lookup.mReanimationType=ReanimationType::REANIM_GARLIC;
    const int face=lookup.FindTrackIndex(ReanimTrackId::anim_face);
    const int stem=lookup.FindTrackIndex(ReanimTrackId::Garlic_stem1);
    const int trackCount=std::max(face,stem)+1;
    std::vector<ReanimatorTrack> tracks(trackCount);
    std::vector<ReanimatorSourceTransform> transforms(trackCount);
    for (int t=0;t<trackCount;++t)
    {
        auto& transform=transforms[t];
        transform.mTransX=transform.mTransY=0;
        transform.mSkewX=transform.mSkewY=0;
        transform.mScaleX=transform.mScaleY=1;
        transform.mFrame=-1;
        transform.mAlpha=1;
        tracks[t].mTransforms={&transform,1};
    }
    tracks[face].mTrackId=ReanimTrackId::anim_face;
    tracks[stem].mTrackId=ReanimTrackId::Garlic_stem1;
    transforms[face].mTransX=4.5f;
    transforms[face].mTransY=4.5f;
    transforms[face].mFrame=0;
    transforms[face].mImageSlot=1;
    transforms[stem].mTransX=10.5f;
    transforms[stem].mTransY=4.5f;
    transforms[stem].mFrame=0;
    transforms[stem].mImageSlot=2;

    Image base(&app),alternate(&app);
    std::array<unsigned,12*8> basePixels;
    std::array<unsigned,16*10> alternatePixels;
    basePixels.fill(0xffff00ffu);
    alternatePixels.fill(0xffff00ffu);
    for (int y=2;y<4;++y)
        for (int x=2;x<4;++x)
        {
            basePixels[y*12+x]=0xffff0000u;
            alternatePixels[y*16+x]=0xffff0000u;
        }
    for (int y=2;y<4;++y)
        for (int x=6;x<8;++x)
            basePixels[y*12+x]=0xff0000ffu;
    for (int y=5;y<7;++y)
        for (int x=7;x<11;++x)
            alternatePixels[y*16+x]=0xff00ff00u;
    base.SetBits(basePixels.data(),12,8);
    alternate.SetBits(alternatePixels.data(),16,10);
    render.CreateImageTexture(&base);
    render.CreateImageTexture(&alternate);
    GpuTexture* textures[]{base.mTexture,base.mTexture,alternate.mTexture,alternate.mTexture};
    TextureRegion rectangles[]{
        {},{2.f/12,2.f/8,4.f/12,4.f/8,2,2},{6.f/12,2.f/8,8.f/12,4.f/8,2,2},
        {},{},                                  {6.f/12,2.f/8,8.f/12,4.f/8,2,2},
        {},{2.f/16,2.f/10,4.f/16,4.f/10,2,2},{7.f/16,5.f/10,11.f/16,7.f/10,4,2},
        {},{},                                  {7.f/16,5.f/10,11.f/16,7.f/10,4,2}};
    std::array<std::uint32_t,16> transitions;
    std::vector<std::uint32_t> starts(trackCount),counts(trackCount),options(4*trackCount);
    starts[face]=0;counts[face]=2;
    starts[stem]=2;counts[stem]=2;
    for (unsigned v=0;v<4;++v)
    {
        transitions[v*4]=v&~1u;
        transitions[v*4+1]=v|1u;
        transitions[v*4+2]=v&~2u;
        transitions[v*4+3]=v|2u;
        options[v*trackCount+face]=v&1u;
        options[v*trackCount+stem]=v>>1;
    }
    ReanimatorDefinition definition;
    definition.mTracks={tracks.data(),trackCount};
    definition.mVariantCount=4;
    definition.mImageSlotCount=3;
    definition.mVariantActionCount=4;
    definition.mTextures=textures;
    definition.mImageRects=rectangles;
    definition.mVariantTransitions=transitions.data();
    definition.mVariantActionStarts=starts.data();
    definition.mVariantActionCounts=counts.data();
    definition.mVariantOptions=options.data();
    EffectSystem effects;
    effects.EffectSystemInitialize();
    Reanimation animation;
    animation.mReanimationType=ReanimationType::REANIM_GARLIC;
    animation.ReanimationInitialize(0,0,&definition, 0);
    // Use the actual pools and AttachmentDraw path. The child keeps its default
    // appearance below the parent, and must honor the parent's visibility flag.
    auto* child=effects.mReanimationHolder->mReanimations.DataArrayAlloc();
    child->mReanimationHolder=effects.mReanimationHolder.get();
    child->mReanimationType=ReanimationType::REANIM_GARLIC;
    child->ReanimationInitialize(0,4,&definition, 0);
    auto* attachment=AttachReanim(animation.mTrackInstances[face].mAttachmentID,child,0,0);
    attachment->mDontDrawIfParentHidden=true;
    animation.SetVariantOption(ReanimTrackId::anim_face,1);
    animation.SetVariantOption(ReanimTrackId::Garlic_stem1,1);
    Require(animation.mVariant==3 && animation.VariantOption(ReanimTrackId::anim_face)==1 &&
        animation.VariantOption(ReanimTrackId::Garlic_stem1)==1,
        "Independent appearance options overwrote one another");
    animation.SetVariantOption(ReanimTrackId::anim_face,0);
    Require(animation.mVariant==2 && animation.VariantOption(ReanimTrackId::Garlic_stem1)==1,
        "Resetting one appearance option reset another track");
    std::unique_ptr<Image> target(render.CreateRenderImage(20,12));
    for (unsigned v=0;v<4;++v)
    {
        animation.SelectVariant(v);
        SexyTransform2D center;
        animation.GetTrackMatrix(stem,center);
        Require(center.m02==11 && center.m12==5,
            "Replacement dimensions changed the original track center");
        animation.GetTrackMatrix(face,center);
        Require(center.m02==5 && center.m12==5,
            "Blank appearance changed attachment track geometry");
        {
            GpuImageDrawScope destination(render,target.get());
            Graphics graphics(target.get());
            graphics.mColor=Color::Black;
            graphics.FillRect(Rect(0,0,20,12));
            graphics.mLinearBlend=false;
            animation.DrawRenderGroup(&graphics,0);
        }
        render.Flush();
        const auto pixels=render.ReadImagePixels(*target);
        for (int y=0;y<12;++y)
            for (int x=0;x<20;++x)
            {
                unsigned expected=0xff000000u;
                if (!(v&1) && x>=4 && x<6 && y>=4 && y<6) expected=0xffff0000u;
                if (v&2)
                {
                    if (x>=9 && x<13 && y>=4 && y<6) expected=0xff00ff00u;
                }
                else if (x>=10 && x<12 && y>=4 && y<6) expected=0xff0000ffu;
                if (x>=4 && x<6 && y>=8 && y<10) expected=0xffff0000u;
                if (x>=10 && x<12 && y>=8 && y<10) expected=0xff0000ffu;
                Pixel(pixels,y*20+x,expected);
            }
    }
    // DrawTrack visibility: explicit synchronous visibility for metadata tracks,
    // incomplete text and a real bitmap-font text track, including blank rows.
    {
        Image glyphImage(&app);
        unsigned glyphPixels[]{0xffffffffu, 0xffffffffu, 0xffffffffu};
        glyphImage.SetBits(glyphPixels,3,1,false);
        ImageFont font(&glyphImage);
        auto& layer=font.mFontData->mFontLayerList.front();
        layer.mPointSize=0; layer.mAscent=1; layer.mHeight=1;
        auto& glyph=layer.mCharData['A'];
        glyph.mImageRect={0,0,1,1,3,1}; glyph.mWidth=3;
        const auto original=transforms[face];
        const auto originalVariant=animation.mVariant;
        const bool originalTruncation=animation.mTrackInstances[face].mTruncateDisappearingFrames;
        ReanimatorFrameTime time{0,0,0};
        const auto check=[&](bool expected,const char* message) {
            {
                GpuImageDrawScope destination(render,target.get());
                Graphics graphics(target.get());
                graphics.mColor=Color::Black;
                graphics.FillRect(Rect(0,0,20,12));
                Require(animation.DrawTrack(&graphics,face,0,&time)==expected,message);
            }
            render.Flush();
            const auto pixels=render.ReadImagePixels(*target);
            Require(pixels.pixels.size()==240,"Metadata fixture changed target size");
        };
        transforms[face].mImageSlot=0;
        transforms[face].mFont=nullptr; transforms[face].mText=nullptr;
        check(false,"Control track incorrectly reports visible");
        transforms[face].mText="attacher__Garlic";
        check(false,"Attacher metadata incorrectly draws as text");
        transforms[face].mFont=&font; transforms[face].mText=nullptr;
        check(false,"Font without text incorrectly reports visible");
        transforms[face].mText="A";
        check(true,"Font/text track was rejected by metadata fast path");
        transforms[face].mText="";
        check(true,"Empty but non-null text changed visibility");
        transforms[face].mText="A"; transforms[face].mAlpha=0;
        check(false,"Zero-alpha text incorrectly reports visible");
        transforms[face].mAlpha=1; transforms[face].mFrame=-1;
        check(false,"Hidden text incorrectly reports visible");
        transforms[face]=original;
        animation.SelectVariant(1);
        check(true,"Blank appearance incorrectly hides attachment parent");
        transforms[face].mAlpha=0;
        check(false,"Zero-alpha blank appearance incorrectly reports visible");
        transforms[face]=original;
        std::vector<std::array<ReanimatorSourceTransform,2>> fade(trackCount);
        for (int t=0;t<trackCount;++t)
        {
            fade[t]={transforms[t],transforms[t]};
            tracks[t].mTransforms={fade[t].data(),2};
        }
        fade[face][1].mFrame=-1;
        time={.5f,0,1};
        animation.mTrackInstances[face].mTruncateDisappearingFrames=true;
        check(false,"Disappearing truncated track incorrectly reports visible");
        animation.mTrackInstances[face].mTruncateDisappearingFrames=false;
        check(true,"Nontruncated disappearing track lost its visible frame");
        for (int t=0;t<trackCount;++t) tracks[t].mTransforms={&transforms[t],1};
        animation.SelectVariant(originalVariant);
        animation.mTrackInstances[face].mTruncateDisappearingFrames=originalTruncation;
    }
    // A genuinely hidden track must still hide this same attachment, so the
    // visible child above cannot pass merely because parentHidden was ignored.
    transforms[face].mAlpha=0;
    {
        GpuImageDrawScope destination(render,target.get());
        Graphics graphics(target.get());
        graphics.mColor=Color::Black;
        graphics.FillRect(Rect(0,0,20,12));
        animation.DrawRenderGroup(&graphics,0);
    }
    render.Flush();
    const auto hidden=render.ReadImagePixels(*target);
    Pixel(hidden,8*20+4,0xff000000u);
    Pixel(hidden,8*20+10,0xff000000u);
    transforms[face].mAlpha=1;
    {
        auto& lawn=static_cast<LawnApp&>(app);
        auto* previousEffects=lawn.mEffectSystem;
        lawn.mEffectSystem=&effects;
        struct RestoreEffects {
            LawnApp& app;
            EffectSystem* previous;
            ~RestoreEffects() { app.mEffectSystem=previous; }
        } restoreEffects{lawn,previousEffects};
        Zombie boss{};
        boss.mFromWave=Zombie::ZOMBIE_WAVE_UI;
        boss.mZombieType=ZombieType::ZOMBIE_BOSS;
        boss.mFadeEndTimeNs=-1;
        boss.mBodyHealth=boss.mBodyMaxHealth=1000;
        boss.mShieldType=ShieldType::SHIELDTYPE_NONE;
        boss.mBodyReanimID=static_cast<ReanimationID>(
            effects.mReanimationHolder->mReanimations.DataArrayGetID(child));
        for (int t=0;t<trackCount;++t)
            child->mTrackInstances[t].mRenderGroup=RENDER_GROUP_BOSS_BACK_ARM;
        child->mOverlayMatrix.m02=0.1f;
        child->mOverlayMatrix.m12=-0.3f;
        const auto original=child->mOverlayMatrix;
        ZombieDrawPosition drawPosition{};
        drawPosition.mClipHeight=CLIP_HEIGHT_OFF;
        std::unique_ptr<Image> bossTarget(render.CreateRenderImage(256,128));
        const ZombiePhase phases[]{PHASE_BOSS_DROP_RV,PHASE_BOSS_BUNGEES_ENTER,
            PHASE_BOSS_BUNGEES_DROP,PHASE_BOSS_BUNGEES_LEAVE};
        for (const auto phase:phases)
        {
            boss.mZombiePhase=phase;
            boss.mTargetCol=1;
            boss.mTargetRow=2;
            // Independent expected draw: apply the intended attack offset,
            // then restore by assignment before exercising the Zombie method.
            child->mOverlayMatrix.m02=original.m02+(phase==PHASE_BOSS_DROP_RV?80.f:57.f);
            child->mOverlayMatrix.m12=original.m12+(phase==PHASE_BOSS_DROP_RV?65.f:0.f);
            {
                GpuImageDrawScope destination(render,bossTarget.get());
                Graphics graphics(bossTarget.get());
                graphics.mColor=Color::Black;
                graphics.FillRect(Rect(0,0,256,128));
                child->DrawRenderGroup(&graphics,RENDER_GROUP_BOSS_BACK_ARM);
            }
            render.Flush();
            const auto expected=render.ReadImagePixels(*bossTarget);
            Require(std::any_of(expected.pixels.begin(),expected.pixels.end(),
                [](unsigned pixel) { return pixel!=0xff000000u; }),
                "Boss back-arm reference did not draw a visible track");
            child->mOverlayMatrix=original;
            {
                GpuImageDrawScope destination(render,bossTarget.get());
                Graphics graphics(bossTarget.get());
                graphics.mColor=Color::Black;
                graphics.FillRect(Rect(0,0,256,128));
                boss.DrawBossBackArm(0, &graphics,drawPosition);
                Require(child->mOverlayMatrix.m02==original.m02 &&
                    child->mOverlayMatrix.m12==original.m12,
                    "Boss back-arm drawing changed the shared animation's fractional translation");
            }
            render.Flush();
            Require(render.ReadImagePixels(*bossTarget).pixels==expected.pixels,
                "Boss back-arm drawing changed its attack offset pixels");
        }
        {
            GpuImageDrawScope destination(render,bossTarget.get());
            Graphics graphics(bossTarget.get());
            for (int i=0;i<1024;++i)
            {
                boss.mZombiePhase=phases[i%4];
                boss.mTargetCol=i%3;
                boss.mTargetRow=i%4;
                boss.DrawBossBackArm(0, &graphics,drawPosition);
                Require(child->mOverlayMatrix.m02==original.m02 &&
                    child->mOverlayMatrix.m12==original.m12,
                    "Repeated boss back-arm drawing accumulated animation translation rounding");
            }
        }
        render.Flush();
        std::cout<<"PASS actual boss back-arm drawing: four attack offsets, reference pixels, "
            "1024 exact shared-animation translation restorations\n";
    }
    bool rejected=false;
    try { animation.SelectVariant(4); } catch (const std::out_of_range&) { rejected=true; }
    Require(rejected && animation.mVariant==3,"Invalid appearance selection damaged the current row");
    std::cout<<"PASS reanimation appearances: independent options, image slots, atlas dimensions, "
        "original centers and actual AttachmentDraw visibility pixels\n";
}

void ReanimationFullscreenPixels(SexyAppBase& app)
{
    auto& render=*app.mDDInterface->mRenderInterface;
    ReanimatorLoadDefinitions(gLawnReanimationArray,static_cast<int>(ReanimationType::NUM_REANIMS));
    struct CatalogCleanup { ~CatalogCleanup() { ReanimatorFreeDefinitions(); } } cleanup;
    Reanimation lookup;
    lookup.mReanimationType=ReanimationType::REANIM_ZOMBIES_WON;
    const int fullscreen=lookup.FindTrackIndex(ReanimTrackId::fullscreen);
    std::vector<ReanimatorTrack> tracks(fullscreen+1);
    std::vector<ReanimatorSourceTransform> transforms(fullscreen+1);
    for (int t=0;t<=fullscreen;++t)
    {
        transforms[t].mScaleX=transforms[t].mScaleY=1;
        transforms[t].mAlpha=0.5f;
        transforms[t].mFrame=-1;
        tracks[t].mTransforms={&transforms[t],1};
    }
    auto& transform=transforms[fullscreen];
    transform.mFrame=0;
    transform.mTransX=12345; transform.mTransY=-7654;
    transform.mSkewX=37; transform.mSkewY=-91;
    transform.mScaleX=-17; transform.mScaleY=23;
    TextureRegion empty{};
    ReanimatorDefinition definition;
    definition.mTracks={tracks.data(),int(tracks.size())};
    definition.mImageRects=&empty;
    Reanimation animation;
    animation.mReanimationType=lookup.mReanimationType;
    animation.ReanimationInitialize(0,0,&definition, 0);
    animation.mColorOverride=Color(200,120,220,200);
    auto& instance=animation.mTrackInstances[fullscreen];
    instance.mTrackColor=Color(90,160,210,180);
    ReanimatorFrameTime time{0,0,0};
    std::unique_ptr<Image> target(render.CreateRenderImage(20,12));
    for (bool hidden:{false,true})
    for (bool transparent:{false,true})
    for (int mode:{Graphics::DRAWMODE_NORMAL,Graphics::DRAWMODE_ADDITIVE})
    for (bool ignoreClip:{false,true})
    {
        transform.mFrame=hidden ? -1 : 0;
        transform.mAlpha=transparent ? 0 : 0.5f;
        instance.mIgnoreClipRect=ignoreClip;
        std::vector<unsigned> expected;
        for (bool reference:{true,false})
        {
            {
                GpuImageDrawScope destination(render,target.get());
                Graphics graphics(target.get());
                graphics.mColor=Color::White;
                graphics.FillRect(Rect(0,0,20,12));
                graphics.mTransX=2.25f; graphics.mTransY=-1.5f;
                graphics.mClipRect=Rect(3,2,11,7);
                graphics.mDrawMode=mode;
                graphics.mColorizeImages=true;
                const Color original(140,220,160,128);
                graphics.mColor=original;
                if (reference && !hidden && !transparent)
                {
                    Color color=ColorsMultiply(ColorsMultiply(instance.mTrackColor,
                        animation.mColorOverride),original);
                    color.mAlpha=FloatRoundToInt(color.mAlpha*0.5f);
                    graphics.mColor=color;
                    graphics.FillRect(-graphics.mTransX,-graphics.mTransY,20,12);
                }
                if (!reference)
                {
                    Require(animation.DrawTrack(&graphics,fullscreen,0,&time)==(!hidden && !transparent),
                        "Fullscreen track changed synchronous visibility");
                    Require(graphics.mColor==original,"Fullscreen track did not restore Graphics color");
                }
            }
            render.Flush();
            const auto pixels=render.ReadImagePixels(*target);
            if (reference) expected=pixels.pixels;
            else Require(pixels.pixels==expected,
                "Fullscreen reanimation differs from destination fill after matrix bypass");
        }
    }
    std::cout<<"PASS fullscreen reanimation: destination fill pixels, hidden/zero alpha, "
        "color modulation, both blend modes, translation and effective clip without track matrices\n";
}

void ReanimationAppearanceDefinitions(SexyAppBase& app)
{
    ReanimatorLoadDefinitions(gLawnReanimationArray,static_cast<int>(ReanimationType::NUM_REANIMS));
    struct CatalogCleanup {
        ~CatalogCleanup() { ReanimatorFreeDefinitions(); DefinitionFreeAtlasImages(); }
    } catalogCleanup;
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_GARLIC,false);
    Reanimation first,second;
    first.ReanimationInitializeType(0,0,ReanimationType::REANIM_GARLIC, 0);
    second.ReanimationInitializeType(0,0,ReanimationType::REANIM_GARLIC, 0);
    Require(first.mDefinition==second.mDefinition && first.mDefinition->mVariantCount==3,
        "Garlic appearances duplicated shared animation definitions");
    const auto* sharedTracks=first.mDefinition->mTracks.tracks;
    for (int track=0;track<first.mDefinition->mTracks.count;++track)
        for (int frame=0;frame<sharedTracks[track].mTransforms.count;++frame)
            Require(sharedTracks[track].mTransforms.mTransforms[frame].mFrame!=-10000.0f,
                "Loaded Garlic retained an unresolved cel placeholder");
    const int face=second.FindTrackIndex(ReanimTrackId::anim_face);
    const int blink=second.FindTrackIndex(ReanimTrackId::Garlic_blink);
    const auto* blinkFrames=sharedTracks[blink].mTransforms.mTransforms;
    Require(blinkFrames[0].mFrame==-1 && blinkFrames[1].mFrame==0 &&
        blinkFrames[2].mFrame==0 && blinkFrames[4].mFrame==-1 && blinkFrames[5].mFrame==-1,
        "Cel inheritance changed Garlic's explicit or inherited hidden frames");
    auto& render=*app.mDDInterface->mRenderInterface;
    std::unique_ptr<Image> target(render.CreateRenderImage(128,128));
    {
        GpuImageDrawScope destination(render,target.get());
        Graphics graphics(target.get());
        for (int frame=0;frame<sharedTracks[face].mTransforms.count;++frame)
        {
            const ReanimatorFrameTime time{0.0f,frame,frame};
            Require(second.DrawTrack(&graphics,face,0,&time)==(frame>=4),
                "Garlic cel inheritance hid a visible face frame or exposed a hidden frame");
        }
    }
    render.Flush();
    first.SetVariantOption(ReanimTrackId::anim_face,2);
    Require(first.VariantOption(ReanimTrackId::anim_face)==2 && second.mVariant==0 &&
        first.mDefinition->mTracks.tracks==sharedTracks,
        "Appearance selection modified another instance or copied shared tracks");
    for (unsigned v=0;v<first.mDefinition->mVariantCount;++v)
    {
        first.SelectVariant(v);
        Require(first.mDefinition->mTextures[v],"Appearance atlas binding was not uploaded");
        for (int track=0;track<first.mDefinition->mTracks.count;++track)
        {
            const auto count=first.mDefinition->mVariantActionCounts[track];
            if (!count) continue;
            const auto id=first.mDefinition->mTracks.tracks[track].mTrackId;
            const auto original=first.VariantOption(id);
            for (unsigned option=0;option<count;++option)
            {
                first.SelectVariant(v);
                first.SetVariantOption(id,option);
                Require(first.VariantOption(id)==option,
                    "Loaded appearance transition disagrees with its constant-time option table");
            }
            first.SelectVariant(v);
            Require(first.VariantOption(id)==original,"Appearance option lookup changed state");
        }
    }
    ReanimatorEnsureDefinitionLoaded(ReanimationType::REANIM_GARGANTUAR,false);
    Reanimation giant;
    giant.ReanimationInitializeType(0,0,ReanimationType::REANIM_GARGANTUAR, 0);
    giant.SetVariantOption(ReanimTrackId::Zombie_gargantuar_telephonepole,2);
    giant.SetVariantOption(ReanimTrackId::anim_head1,1);
    giant.SetVariantOption(ReanimTrackId::Zombie_gargantua_body1,2);
    Require(giant.VariantOption(ReanimTrackId::Zombie_gargantuar_telephonepole)==2 &&
        giant.VariantOption(ReanimTrackId::anim_head1)==1 &&
        giant.VariantOption(ReanimTrackId::Zombie_gargantua_body1)==2,
        "Loaded independent damage/weapon/head combinations lost previous state");
    giant.SetVariantOption(ReanimTrackId::Zombie_gargantua_body1,0);
    Require(giant.VariantOption(ReanimTrackId::Zombie_gargantuar_telephonepole)==2 &&
        giant.VariantOption(ReanimTrackId::anim_head1)==1,
        "Resetting loaded body damage reset the selected weapon or red eyes");
    std::cout<<"PASS production RAV7/IDT6 body-first: cel inheritance and hidden-frame draws, "
        "shared definitions, all Garlic rows, uploaded bindings, "
        "derived option tables and Gargantuar independent damage/weapon/head transitions\n";
}

// Exercise the production decoder without creating a playback device or channels.
class DecodeOnlySoundManager final : public SoundManager
{
public:
    SoundDecoderLease decoder;
    struct Slot { bool live=false; double volume=-1; int pan=0; };
    std::array<Slot,MAX_SOURCE_SOUNDS> slots{};
    std::array<unsigned int,MAX_SOURCE_SOUNDS> playbackRequests{};
    std::vector<ResourceManager::SoundRes> expected;
    unsigned nextExpected=0,loads=0,releases=0,volumeCalls=0,panCalls=0,invalidReleases=0;
    bool allowSilentPlayback=false;

    bool LoadSound(unsigned id,const std::string& path) override
    {
        Require(id<slots.size() && !slots[id].live && nextExpected<expected.size(),
            "Sound load reused a live handle or exceeded its group plan");
        const auto& metadata=expected[nextExpected++];
        Require(path==metadata.mPath,"Generated sound binding consumed the wrong plan record");
        DecodedSound pcm;
        if (!DecodeSound(path,pcm)) return false;
        Require(!pcm.samples.empty() && pcm.frequency && (pcm.channels==1 || pcm.channels==2),
            "Production sound decoder returned invalid PCM");
        slots[id]={true,metadata.mVolume,metadata.mPanning};
        ++loads;
        return true;
    }
    int LoadSound(const std::string& path) override
    {
        const int id=GetFreeSoundId();
        return id>=0 && LoadSound(unsigned(id),path) ? id : -1;
    }
    void ReleaseSound(unsigned id) override
    {
        if (id>=slots.size() || !slots[id].live) { ++invalidReleases; return; }
        slots[id]={}; ++releases;
    }
    void SetVolume(double) override {}
    bool SetBaseVolume(unsigned id,double volume) override
    {
        Require(id<slots.size() && slots[id].live && slots[id].volume==volume,
            "Sound base volume was applied to the wrong handle or value");
        ++volumeCalls; return true;
    }
    bool SetBasePan(unsigned id,int pan) override
    {
        Require(id<slots.size() && slots[id].live && slots[id].pan==pan,
            "Sound pan was applied to the wrong handle or value");
        ++panCalls; return true;
    }
    SoundInstance* GetSoundInstance(unsigned id) override
    {
        if (allowSilentPlayback) {
            if(id<playbackRequests.size()) ++playbackRequests[id];
            return nullptr;
        }
        throw std::runtime_error("Resource loading unexpectedly requested sound playback");
    }
    void ReleaseSounds() override
    {
        for (unsigned id=0;id<slots.size();++id) if (slots[id].live) ReleaseSound(id);
    }
    void ReleaseChannels() override {}
    void Flush() override {}
    void SetCooperativeWindow(NativeWindowHandle) override {}
    void StopAllSounds() override {}
    int GetFreeSoundId() override
    {
        for (unsigned id=0;id<slots.size();++id) if (!slots[id].live) return int(id);
        return -1;
    }
    int GetNumSounds() override
    {
        return int(std::count_if(slots.begin(),slots.end(),[](const auto& slot){return slot.live;}));
    }
};

void FrameDivisionSampleRuntime()
{
    Reanimation animation;
    animation.mDead=true;
    animation.mFrameCount=9;
    animation.mFramePeriodNs=17'000'003;
    constexpr std::int64_t base=9'000'000'000;
    unsigned int checked=0;
    for(int kind=0;kind<6;++kind) {
        animation.mLoopType=static_cast<ReanimLoopType>(kind);
        const bool full=kind==1 || kind==4 || kind==5;
        const int intervals=full?9:8;
        animation.mPeriodNs=animation.mFramePeriodNs*intervals;
        const auto q=animation.mFramePeriodNs;
        const bool looping=kind<2;
        for(float rate:{-30.0f,0.0f,30.0f}) for(int cached:{0,1,intervals-1,intervals})
            for(std::int64_t age:{0LL,1LL,q/2,q-1}) {
                const bool reverse=rate<0;
                if(age && ((reverse && cached==0) || (!reverse && cached==intervals))) continue;
                animation.mAnimRate=rate;animation.mCurrentFrame=cached;
                animation.mStartTimeNs=base-age;animation.mLastUpdateTimeNs=base;
                for(std::int64_t delta:{-3*animation.mPeriodNs-7,-q-1,-q,-1LL,0LL,1LL,q-1,q,q+1,2*q,3*animation.mPeriodNs+7})
                    for(std::int64_t offset:{-1LL,0LL,1LL}) {
                        const auto now=base+delta;
                        auto phase=std::int64_t(cached)*q;
                        std::int64_t cycles=0;
                        const auto sampleAge=(rate==0?base:now+offset)-animation.mStartTimeNs;
                        ReanimClock::Advance(phase,cycles,animation.mPeriodNs,reverse?-1:1,sampleAge);
                        ReanimFramePosition expected;
                        if(!looping && cycles) expected={0,cycles>0?intervals:0,0};
                        else {
                            const auto frame=phase/q,remainder=phase%q;
                            expected={cycles,int(frame+(reverse && remainder!=0)),
                                reverse && remainder!=0?q-remainder:remainder};
                        }
                        const auto actual=animation.SampleFrameAt(now,offset);
                        Require(actual.frame==expected.frame && actual.ageNs==expected.ageNs && actual.cycles==expected.cycles,
                            "Division frame sample changed reverse/hold/endpoint/offset semantics");
                        ++checked;
                    }
            }
    }
    std::cout<<"PASS division frame samples: "<<checked<<" exact forward/reverse/stopped, loop/hold/endpoint and retrospective/offset queries\n";
    unsigned int heldChecks=0;
    for (const auto kind : {REANIM_PLAY_ONCE_AND_HOLD,REANIM_PLAY_ONCE_FULL_LAST_FRAME_AND_HOLD})
        for (const float rate : {-30.0f,0.0f,30.0f})
    {
        Reanimation held;
        held.mFrameCount=9;held.mLoopType=kind;held.mAnimRate=rate;
        held.mFramePeriodNs=17'000'003;
        const int intervals=kind==REANIM_PLAY_ONCE_AND_HOLD?8:9;
        held.mPeriodNs=held.mFramePeriodNs*intervals;
        held.Seek(rate<0?1:0,1);
        if (rate!=0) held.Advance(held.mPeriodNs*3+7);
        else held.Seek(1,1);
        const auto endpoint=rate<0?0:held.mPeriodNs;
        for (int i=0;i<256;++i)
        {
            held.Advance((i%3==0?133'333'333:1'234'567)+i);
            Require(held.SamplePlayback().phaseNs==endpoint &&
                held.mLastFrameNs==endpoint && held.mStartTimeNs==held.mLastUpdateTimeNs &&
                held.SampleFrameAt(held.mLastUpdateTimeNs).ageNs==0,
                "Held endpoint accumulated age beyond its animation period");
            Require(held.TimedEventCount(1,2)==0 && held.TimedEventCount(1,1)==0,
                "Held endpoint emitted a repeated animation event");
            std::array<unsigned char,72> state{};
            V7SaveSchema::Write(state,4,held.mStartTimeNs,8);
            V7SaveSchema::Write(state,12,held.mCurrentFrame,4);
            V7SaveSchema::Write(state,16,held.mFramePeriodNs,8);
            V7SaveSchema::Write(state,24,held.mPeriodNs,8);
            V7SaveSchema::Write(state,40,held.mLastFrameNs,8);
            V7SaveSchema::Write(state,56,held.mHasLastFrame,1);
            V7SaveSchema::Write(state,64,held.mLastUpdateTimeNs,8);
            V7SaveSchema::Validate(LevelSaveFormat::RecordKind::Reanimation,state);
            ++heldChecks;
        }
        held.mDead=true; // No owned track/attachment fixture was allocated.
    }
    std::cout<<"PASS held endpoint cache/save: "<<heldChecks
        <<" forward/reverse/stopped samples, repeated updates and no duplicate events\n";

}

void ExplicitReanimationOwnerTime(LawnApp& app)
{
    auto& holder=*app.mEffectSystem->mReanimationHolder;
    struct RestoreClocks {
        ReanimationHolder& holder;
        std::int64_t visual, battle;
        ~RestoreClocks() { holder.mVisualTimeNs=visual; holder.mBattleTimeNs=battle; }
    } restore{holder,holder.mVisualTimeNs,holder.mBattleTimeNs};
    constexpr std::int64_t start=5'123'456'789;
    holder.mVisualTimeNs=900'000'000'000;
    holder.mBattleTimeNs=-800'000'000'000;
    auto* first=app.AddReanimation(0,0,0,REANIM_PEASHOOTER,start,ReanimTimeDomain::Visual);
    holder.mVisualTimeNs=-700'000'000'000;
    holder.mBattleTimeNs=600'000'000'000;
    auto* second=app.AddReanimation(0,0,0,REANIM_PEASHOOTER,start,ReanimTimeDomain::Visual);
    struct RemoveAnimations {
        LawnApp& app;
        ReanimationID first, second;
        ~RemoveAnimations() { app.RemoveReanimation(first); app.RemoveReanimation(second); }
    } remove{app,app.ReanimationGetID(first),app.ReanimationGetID(second)};
    Require(first->mLastUpdateTimeNs==start && second->mLastUpdateTimeNs==start,
        "Explicit animation creation read a holder clock");
    for (auto* animation:{first,second})
        animation->PlayReanim(ReanimTrackId::anim_idle,REANIM_LOOP,0,12);
    for (const std::int64_t elapsed:{17'123'457LL,321'987'654LL,2'345'678'901LL})
    {
        const auto previous=first->SamplePlayback().phaseNs;
        holder.mVisualTimeNs+=37'000'000'000;
        holder.mBattleTimeNs-=41'000'000'000;
        Require(first->SamplePlayback().phaseNs==previous,
            "Changing a holder clock changed an unadvanced animation query");
        first->UpdateAt(start+elapsed);
        holder.mVisualTimeNs=-holder.mVisualTimeNs;
        holder.mBattleTimeNs=-holder.mBattleTimeNs;
        second->UpdateAt(start+elapsed);
        ReanimatorFrameTime a{}, b{};
        first->GetFrameTime(&a); second->GetFrameTime(&b);
        Require(first->SamplePlayback().phaseNs==second->SamplePlayback().phaseNs &&
            a.mAnimFrameBeforeInt==b.mAnimFrameBeforeInt && a.mFraction==b.mFraction,
            "Supplied animation time produced holder-dependent phases or frame queries");
    }
    constexpr auto prepared=start+2'567'890'123;
    first->PrepareTimeAt(prepared);
    const auto preparedPhase=first->SamplePlayback().phaseNs;
    holder.mVisualTimeNs=1'234'567'890'123;
    first->ApplyRate(7.5f);
    second->PrepareTimeAt(prepared);
    holder.mVisualTimeNs=-1'234'567'890'123;
    second->ApplyRate(7.5f);
    Require(first->mLastUpdateTimeNs==prepared && second->mLastUpdateTimeNs==prepared &&
        first->SamplePlayback().phaseNs==second->SamplePlayback().phaseNs && preparedPhase>0,
        "Rate mutation did not use the prepared owner time");
    first->Seek(1,3);
    holder.mVisualTimeNs+=999'000'000'000;
    second->Seek(1,3);
    const auto phase=first->SamplePlayback().phaseNs;
    Require(phase==first->mPeriodNs/3 && second->SamplePlayback().phaseNs==phase &&
        first->mLastUpdateTimeNs==prepared && second->mLastUpdateTimeNs==prepared,
        "Seek rebound playback to a holder clock instead of prepared owner time");
    constexpr std::int64_t destination=90'123'456'789;
    holder.BindAnimationTime(*first,ReanimTimeDomain::Battle,destination);
    Require(holder.mTimeDomains[holder.mReanimations.DataArrayIndexOf(first)]==ReanimTimeDomain::Battle &&
        first->mLastUpdateTimeNs==destination && first->SamplePlayback().phaseNs==phase,
        "Switching clock domains changed the animation phase");
    constexpr std::int64_t delta=123'456'789;
    first->UpdateAt(destination+delta);
    second->UpdateAt(prepared+delta);
    Require(first->SamplePlayback().phaseNs==second->SamplePlayback().phaseNs,
        "Clock switching changed subsequent advancement");
    const auto switchedPhase=first->SamplePlayback().phaseNs;
    holder.BindAnimationTime(*first,ReanimTimeDomain::Visual,-7'654'321'987);
    Require(first->SamplePlayback().phaseNs==switchedPhase,
        "Switching back to a differently based visual clock changed phase");
    std::cout<<"PASS explicit reanimation owner time: creation, prepared rate/seek, frame queries and clock switching\n";
}

void BoardlessPlantPreviewRuntime(LawnApp& app)
{
    auto& holder=*app.mEffectSystem->mReanimationHolder;
    struct RestorePreviewContext {
        LawnApp& app;
        Board* board;
        std::int64_t visual, battle;
        ~RestorePreviewContext() {
            app.mBoard=board;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
        }
    } restore{app,app.mBoard,holder.mVisualTimeNs,holder.mBattleTimeNs};
    app.mBoard=nullptr;
    auto& render=*app.mDDInterface->mRenderInterface;
    std::unique_ptr<Image> target(render.CreateRenderImage(256,256));
    {
        GpuImageDrawScope destination(render,target.get());
        Graphics graphics(target.get());
        constexpr std::int64_t start=37'123'456'789;
        for (const auto seed:{SEED_PEASHOOTER,SEED_SUNFLOWER,SEED_MAGNETSHROOM})
        {
            holder.mVisualTimeNs=-800'000'000'000;
            holder.mBattleTimeNs=900'000'000'000;
            Plant preview;
            preview.mIsOnBoard=false;
            preview.PlantInitialize(start,start,start,0,0,seed,SEED_NONE);
            Require(preview.mBoard==nullptr && !preview.mIsOnBoard,
                "Boardless preview unexpectedly acquired a Board");
            auto* body=app.ReanimationGet(preview.mBodyReanimID);
            Require(body->mLastUpdateTimeNs==start && holder.mTimeDomains[holder.mReanimations.DataArrayIndexOf(body)]==ReanimTimeDomain::Visual,
                "Boardless preview initialization ignored explicit visual time");
            const auto phase=body->SamplePlayback().phaseNs;
            for (const std::int64_t elapsed:{10'000'000LL,123'456'789LL,987'654'321LL})
            {
                const auto now=start+elapsed;
                holder.mVisualTimeNs+=101'000'000'000;
                holder.mBattleTimeNs-=103'000'000'000;
                preview.Update(now,now,now,0);
                preview.Draw(now,&graphics);
                if (seed==SEED_MAGNETSHROOM)
                    preview.DrawMagnetItems(now,&graphics);
                Require(preview.mBoard==nullptr && body->mLastUpdateTimeNs==now &&
                    holder.mTimeDomains[holder.mReanimations.DataArrayIndexOf(body)]==ReanimTimeDomain::Visual,
                    "Boardless preview update/draw did not retain its explicit visual timeline");
            }
            Require(body->SamplePlayback().phaseNs!=phase,
                "Boardless preview pose did not advance with supplied visual time");
            for (const auto id:{preview.mBodyReanimID,preview.mHeadReanimID,
                preview.mHeadReanimID2,preview.mHeadReanimID3,preview.mBlinkReanimID,
                preview.mLightReanimID,preview.mSleepingReanimID})
                app.RemoveReanimation(id);
        }
    }
    render.Flush();
    std::cout<<"PASS Boardless Plant previews: explicit visual init/update/draw including Magnet-shroom items\n";
}

void CreditsMovieOwnerClockRuntime(TestApp& app)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    PoolEffect pool;
    pool.PoolEffectInitialize();
    struct RestoreCreditsContext {
        TestApp& app;
        CreditScreen* credits;
        PoolEffect* pool;
        Board* board;
        GameScenes scene;
        Rect bounds;
        std::int64_t visual, battle;
        unsigned long long interval;
        double ticks, steps;
        ~RestoreCreditsContext() {
            app.mCreditScreen=credits;
            app.mPoolEffect=pool;
            app.mBoard=board;
            app.mGameScene=scene;
            app.mScreenBounds=bounds;
            app.mEffectSystem->EffectSystemFreeAll();
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
            Sexy::gUpdateIntervalNs=interval;
            Sexy::gUpdateTicks=ticks;
            Sexy::gStepsPerLegacyTick=steps;
        }
    } restore{app,app.mCreditScreen,app.mPoolEffect,app.mBoard,app.mGameScene,app.mScreenBounds,
        clocks.mVisualTimeNs,clocks.mBattleTimeNs,Sexy::gUpdateIntervalNs,
        Sexy::gUpdateTicks,Sexy::gStepsPerLegacyTick};
    app.mPoolEffect=&pool;
    app.mBoard=nullptr;
    app.mGameScene=GameScenes::SCENE_CREDIT;
    Sexy::gUpdateIntervalNs=10'000'000;
    Sexy::gUpdateTicks=1;
    Sexy::gStepsPerLegacyTick=1;
    CreditScreen credits(&app);
    app.mCreditScreen=&credits;
    credits.Resize(0,0,800,600);
    auto& render=*app.mDDInterface->mRenderInterface;
    std::unique_ptr<Image> target(render.CreateRenderImage(800,600));
    const auto beforeDraw=clocks.mVisualTimeNs;
    credits.Update();
    Require(credits.mUpdateCount==0 && clocks.mVisualTimeNs==beforeDraw,
        "Credits advanced before its first loading draw");
    {
        GpuImageDrawScope destination(render,target.get());
        Graphics graphics(target.get());
        credits.Draw(&graphics);
    }
    credits.Update();
    Require(credits.mPreloaded && credits.mUpdateCount==1 &&
        credits.mCreditsReanimID!=ReanimationID::REANIMATIONID_NULL,
        "Credits first draw did not enter the original movie initialization path");
    credits.mDontSync=true;
    auto* movie=app.ReanimationGet(credits.mCreditsReanimID);
    const auto start=clocks.mVisualTimeNs;
    const auto firstExpected=movie->SamplePlaybackAt(start+10'000'000).phaseNs;
    credits.UpdateMovie();
    Require(clocks.mVisualTimeNs==start+10'000'000 &&
        movie->mLastUpdateTimeNs==clocks.mVisualTimeNs &&
        movie->SamplePlayback().phaseNs==firstExpected,
        "First Credits movie step did not own its supplied time");
    const auto firstPhase=movie->SamplePlayback().phaseNs;
    const auto secondExpected=movie->SamplePlaybackAt(start+20'000'000).phaseNs;
    credits.UpdateMovie();
    Require(clocks.mVisualTimeNs==start+20'000'000 &&
        movie->SamplePlayback().phaseNs==secondExpected && secondExpected!=firstPhase,
        "Credits catch-up iterations reused one absolute animation timestamp");

    auto* sunflower=CreditScreen::FindSubReanim(movie,REANIM_SUNFLOWER);
    if (!sunflower)
    {
        sunflower=app.AddReanimation(0,0,0,REANIM_SUNFLOWER,
            clocks.mVisualTimeNs,ReanimTimeDomain::Visual);
        sunflower->PlayReanim(ReanimTrackId::anim_idle,REANIM_LOOP,0,12);
        sunflower->AttachToAnotherReanimation(movie,ReanimTrackId::Words);
    }
    credits.mBlinkCountdown=0;
    credits.UpdateMovie();
    auto* blink=FindReanimAttachment(
        sunflower->GetTrackInstanceByName(ReanimTrackId::anim_idle)->mAttachmentID);
    Require(blink && blink->mReanimationType==REANIM_SUNFLOWER &&
        blink->mLoopType==REANIM_PLAY_ONCE_FULL_LAST_FRAME && !blink->mDead,
        "Credits blink fixture failed to attach its real one-shot animation");
    const auto blinkId=app.ReanimationGetID(blink);
    const auto blinkDuration=blink->mPeriodNs;
    const int blinkSteps=static_cast<int>(blinkDuration/10'000'000)+3;
    Require(blinkSteps<600,"Credits blink fixture would cross the next scheduled blink");
    for (int step=0;step<blinkSteps;++step) credits.UpdateMovie();
    Require(blink->mDead && credits.mBlinkCountdown>0,
        "Credits blink countdown advanced while its attached blink remained frozen");

    credits.mCreditsPaused=true;
    const auto pausedTime=clocks.mVisualTimeNs;
    const auto pausedPhase=movie->SamplePlayback().phaseNs;
    const auto pausedBlink=credits.mBlinkCountdown.mNanoseconds;
    for (int step=0;step<250;++step)
    {
        app.UpdateFrames();
        credits.Update();
    }
    Require(clocks.mVisualTimeNs==pausedTime && movie->mLastUpdateTimeNs==pausedTime &&
        movie->SamplePlayback().phaseNs==pausedPhase &&
        credits.mBlinkCountdown.mNanoseconds==pausedBlink,
        "Paused Credits advanced its movie clock, pose or blink schedule");
    const auto resumeExpected=movie->SamplePlaybackAt(
        pausedTime+static_cast<std::int64_t>(Sexy::gUpdateIntervalNs)).phaseNs;
    credits.mCreditsPaused=false;
    app.UpdateFrames();
    credits.Update();
    Require(clocks.mVisualTimeNs==pausedTime+static_cast<std::int64_t>(Sexy::gUpdateIntervalNs) &&
        movie->SamplePlayback().phaseNs==resumeExpected,
        "Resuming Credits consumed paused wall time or double-advanced the movie step");
    Require(!app.ReanimationTryToGet(blinkId) || app.ReanimationTryToGet(blinkId)->mDead,
        "Completed Credits blink became continuously active after pause/resume");
    {
        GpuImageDrawScope destination(render,target.get());
        Graphics graphics(target.get());
        credits.Draw(&graphics);
    }
    render.Flush();
    std::cout<<"PASS Credits movie owner clock: distinct catch-up steps, one-shot blink, paused app updates and single-step resume\n";
    std::unique_ptr<Image> wide(render.CreateRenderImage(1400,600));
    credits.mCreditsPaused=true;
    struct Scene { CreditsPhase phase; int frame; };
    for (const auto scene : {Scene{CreditsPhase::CREDITS_MAIN1,230},
            Scene{CreditsPhase::CREDITS_MAIN1,320}, Scene{CreditsPhase::CREDITS_MAIN2,50},
            Scene{CreditsPhase::CREDITS_MAIN2,210}, Scene{CreditsPhase::CREDITS_MAIN3,50},
            Scene{CreditsPhase::CREDITS_MAIN3,250}, Scene{CreditsPhase::CREDITS_END,1}})
    {
        app.mEffectSystem->EffectSystemFreeAll();
        movie=credits.PlayReanim(scene.phase==CreditsPhase::CREDITS_END ? 3 : int(scene.phase)+1);
        credits.mCreditsPhase=scene.phase;
        movie->Seek(scene.frame,movie->mDefinition->mTracks.tracks->mTransforms.count-1);
        movie->UpdateAt(clocks.mVisualTimeNs);
        credits.mDrawCount=10;
        app.mScreenBounds=Rect(0,0,800,600);
        {
            GpuImageDrawScope destination(render,target.get());
            Graphics graphics(target.get());
            credits.Draw(&graphics);
        }
        const auto reference=render.ReadImagePixels(*target);
        app.mScreenBounds=Rect(-300,0,1400,600);
        {
            GpuImageDrawScope destination(render,wide.get());
            Graphics graphics(wide.get());
            graphics.mTransX=300;
            const auto originalClip=graphics.mClipRect;
            credits.Draw(&graphics);
            Require(graphics.mClipRect==originalClip && graphics.mTransX==300,
                "Credits movie draw leaked its crop into sibling buttons");
        }
        const auto actual=render.ReadImagePixels(*wide);
        int differences=0;
        for (int y=0;y<600;++y) for (int x=0;x<1400;++x)
        {
            const auto pixel=actual.pixels[y*1400+x];
            if (x<300 || x>=1100)
            {
                if (pixel!=0xff000000u)
                    std::cerr<<"Credits crop leak: phase="<<int(scene.phase)<<" frame="<<scene.frame
                        <<" x="<<x<<" y="<<y<<" color="<<std::hex<<pixel<<std::dec<<'\n';
                Require(pixel==0xff000000u,"Credits pool/lights escaped the centered 800x600 movie");
            }
            else
            {
                const auto expected=reference.pixels[y*800+x-300];
                for (const int shift:{0,8,16,24})
                    if (std::abs(int((pixel>>shift)&255)-int((expected>>shift)&255))>4)
                    { ++differences; break; }
            }
        }
        Require(differences<1440,"Credits wide crop differs from the original 800x600 movie");
        credits.Update(); // Paused layout still follows the physical window edge.
        Require(credits.mReplayButton->mX==-290,"Credits Replay did not follow the whole-window left edge");
    }
    std::cout<<"PASS Credits 800x600 crop: day/night pool, disco, scene transitions, final roll and window-left Replay\n";
}

void PottedGiftPickupRuntime(TestApp& app, Board& board)
{
    PlayerInfo profile;
    profile.mLevel=50;
    ZenGarden garden;
    struct Restore {
        TestApp& app;
        PlayerInfo* profile;
        ZenGarden* garden;
        Board* board;
        ~Restore() { app.mPlayerInfo=profile; app.mZenGarden=garden; app.mBoard=board; }
    } restore{app,app.mPlayerInfo,app.mZenGarden,app.mBoard};
    app.mPlayerInfo=&profile;
    app.mZenGarden=&garden;
    app.mBoard=&board;
    garden.mApp=&app;
    garden.mBoard=nullptr; // Normal combat has never entered Zen Garden.
    auto& cache=*app.mReanimatorCache;
    for (const auto seed:{SEED_PEASHOOTER,SEED_TANGLEKELP,SEED_FLOWERPOT})
        if (!cache.mPlantImages[seed]) cache.mPlantImages[seed]=cache.MakeCachedPlantFrame(seed,VARIATION_NORMAL);
    for (const auto variation:{VARIATION_ZEN_GARDEN,VARIATION_ZEN_GARDEN_WATER})
    {
        auto*& image=cache.mImageVariations.At(SEED_FLOWERPOT,variation);
        if (!image) { image=cache.MakeCachedPlantFrame(SEED_FLOWERPOT,variation); ++cache.mImageVariations.mCount; }
    }
    auto*& kelp=cache.mImageVariations.At(SEED_TANGLEKELP,VARIATION_AQUARIUM);
    if (!kelp) { kelp=cache.MakeCachedPlantFrame(SEED_TANGLEKELP,VARIATION_AQUARIUM); ++cache.mImageVariations.mCount; }
    auto& render=*app.mDDInterface->mRenderInterface;
    std::unique_ptr<Image> target(render.CreateRenderImage(240,200));
    for (const auto kind:{COIN_PRESENT_PLANT,COIN_AWARD_PRESENT})
        for (const auto seed:{SEED_PEASHOOTER,SEED_TANGLEKELP})
        {
            Coin gift;
            gift.CoinInitialize(60,80,kind,COIN_MOTION_COIN);
            gift.mPottedPlantSpec.mSeedType=seed;
            gift.mPottedPlantSpec.mPlantAge=PLANTAGE_FULL;
            gift.mPottedPlantSpec.mDrawVariation=VARIATION_NORMAL;
            const int before=profile.mNumPottedPlants;
            gift.Collect();
            Require(gift.mIsBeingCollected && profile.mNumPottedPlants==before+1 && !garden.mBoard,
                "Gift pickup required entering Zen Garden or failed to store the plant");
            {
                GpuImageDrawScope destination(render,target.get());
                Graphics graphics(target.get());
                graphics.mColor=Color::Black;
                graphics.FillRect(0,0,240,200);
                gift.Draw(&graphics);
            }
            const auto pixels=render.ReadImagePixels(*target);
            Require(std::count_if(pixels.pixels.begin(),pixels.pixels.end(),
                [](unsigned p) { return (p&0xffffffu)!=0; })>20,
                "Collected garden gift did not draw its plant icon");
            gift.Die();
        }
    std::cout<<"PASS collected garden gifts: both present types, ordinary/aquatic plants, real Collect/Draw with null garden Board\n";
}

void SurvivalOwnerClockRuntime(TestApp& app, int hz)
{
    auto& holder=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        Board* board;
        GameMode mode;
        GameScenes scene;
        std::int64_t visual, battle;
        std::uint64_t interval=Sexy::gUpdateIntervalNs;
        double ticks=Sexy::gUpdateTicks, steps=Sexy::gStepsPerLegacyTick;
        ~Restore() {
            app.mBoard=board; app.mGameMode=mode; app.mGameScene=scene;
            auto& holder=*app.mEffectSystem->mReanimationHolder;
            holder.mVisualTimeNs=visual; holder.mBattleTimeNs=battle;
            Sexy::gUpdateIntervalNs=interval; Sexy::gUpdateTicks=ticks; Sexy::gStepsPerLegacyTick=steps;
        }
    } restore{app,app.mBoard,app.mGameMode,app.mGameScene,holder.mVisualTimeNs,holder.mBattleTimeNs};
    app.mGameMode=GAMEMODE_SURVIVAL_ENDLESS_STAGE_3;
    app.mGameScene=SCENE_PLAYING;
    Board board(&app);
    app.mBoard=&board;
    board.mLevelTimeNs=123'456'000'000;
    holder.mBattleTimeNs=static_cast<std::int64_t>(board.mLevelTimeNs);
    holder.mVisualTimeNs=987'654'000'000;
    board.mNumWaves=20;
    board.mZombieCountDown=100000;
    board.mSunCountDown=100000;
    auto* plant=board.NewPlant(2,2,SEED_PEASHOOTER,SEED_NONE);
    auto* body=app.ReanimationGet(plant->mBodyReanimID);
    auto battleTime=board.mLevelTimeNs;
    auto phase=body->SamplePlayback().phaseNs;
    auto deadline=plant->mNextLaunchTimeNs;
    board.mNextSurvivalStageCounter=500;
    const auto step=1'000'000'000ULL/hz;
    Sexy::gUpdateIntervalNs=step;
    Sexy::gUpdateTicks=100.0/hz;
    Sexy::gStepsPerLegacyTick=hz/100.0;
    for (int frame=0;frame<30;++frame) {
        holder.mVisualTimeNs+=step;
        board.Update();
    }
    Require(board.mLevelTimeNs==battleTime+30*step && holder.mBattleTimeNs==static_cast<std::int64_t>(battleTime+30*step) &&
        body->SamplePlayback().phaseNs!=phase &&
        board.mNextSurvivalStageCounter.mNanoseconds==5'000'000'000LL-30*static_cast<std::int64_t>(step),
        "Survival round-end wait froze battle time/animation or failed to advance its countdown");
    board.mNextSurvivalStageCounter=0;
    board.mChallenge->mSurvivalStage=1;
    app.mGameScene=SCENE_LEVEL_INTRO;
    board.mDrawCount=0; // Keep the real cutscene from starting the chooser in this hidden fixture.
    battleTime=board.mLevelTimeNs;
    deadline=plant->mNextLaunchTimeNs;
    phase=body->SamplePlayback().phaseNs;
    // Retail stops immediately on the PLAYING -> LEVEL_INTRO transition,
    // before the camera moves. Verify the stationary and scrolling portions.
    for (const int introTime : {0,1490,1500,3500}) {
        board.mCutScene->mCutsceneTime=introTime;
        holder.mVisualTimeNs+=step;
        board.Update();
        Require(board.mLevelTimeNs==battleTime &&
            holder.mBattleTimeNs==static_cast<std::int64_t>(battleTime),
            "Survival intro advanced Board time before/during camera movement");
        Require(body->SamplePlayback().phaseNs==phase,
            "Survival intro advanced retained animation before camera movement");
        Require(plant->mNextLaunchTimeNs==deadline,
            "Survival intro changed the inactive plant attack deadline");
    }
    auto* preview=board.AddZombieInRow(ZOMBIE_NORMAL,2,Zombie::ZOMBIE_WAVE_CUTSCENE);
    auto* previewBody=app.ReanimationGet(preview->mBodyReanimID);
    Require(holder.mTimeDomains[holder.mReanimations.DataArrayIndexOf(previewBody)]==ReanimTimeDomain::Visual &&
        previewBody->mLastUpdateTimeNs==holder.mVisualTimeNs,
        "Street preview was created in the battle epoch");
    const auto previewPhase=previewBody->SamplePlayback().phaseNs;
    TodParticleSystem* cutsceneParticle=nullptr;
    std::int64_t cutsceneParticleBirth=0;
    GridItem portal;
    portal.mBoard=&board;
    portal.mGridItemType=GRIDITEM_PORTAL_CIRCLE;
    portal.mGridX=2;
    portal.mGridY=2;
    portal.OpenPortal();
    app.ReanimationGet(portal.mGridItemReanimID)->mLoopCount=1;
    portal.UpdatePortal();
    auto* portalParticle=app.ParticleGet(portal.mGridItemParticleID);
    const auto portalParticleBirth=portalParticle->EmitterRosterEmitterAt(0)->mBirthTimeNs;
    Require(portalParticleBirth==static_cast<std::int64_t>(battleTime) && !portalParticle->mIsAttachment,
        "Portal particle did not retain global Board-time ownership");
    for (int frame=0;frame<30;++frame) {
        // PeaSplat lives 200 ms; create it late enough to remain alive at every tested rate.
        if (frame==20) {
            cutsceneParticle=board.mCutScene->AddVisualParticle(0,0,0,PARTICLE_PEA_SPLAT);
            cutsceneParticleBirth=cutsceneParticle->EmitterRosterEmitterAt(0)->mBirthTimeNs;
            Require(cutsceneParticleBirth==holder.mVisualTimeNs && cutsceneParticle->mIsAttachment &&
                std::find(board.mCutScene->mVisualParticleIDs.begin(),board.mCutScene->mVisualParticleIDs.end(),
                    app.ParticleGetID(cutsceneParticle))!=board.mCutScene->mVisualParticleIDs.end(),
                "Cutscene particle did not register with its visual owner");
        }
        holder.mVisualTimeNs+=step;
        board.Update();
    }
    Require(board.mLevelTimeNs==battleTime, "Repick advanced Board time during camera movement");
    Require(holder.mTimeDomains[holder.mReanimations.DataArrayIndexOf(body)]==ReanimTimeDomain::Battle,
        "Repick rebound retained actors to visual time");
    Require(body->SamplePlayback().phaseNs==phase, "Repick advanced retained animation phase");
    Require(plant->mNextLaunchTimeNs==deadline, "Repick changed the plant launch deadline");
    Require(holder.mTimeDomains[holder.mReanimations.DataArrayIndexOf(previewBody)]==ReanimTimeDomain::Visual &&
        previewBody->mLastUpdateTimeNs==holder.mVisualTimeNs &&
        previewBody->SamplePlayback().phaseNs!=previewPhase,
        "Street preview did not consume the Board-supplied visual time");
    Require(cutsceneParticle->mLastUpdateTimeNs==holder.mVisualTimeNs &&
        cutsceneParticle->EmitterRosterEmitterAt(0)->mBirthTimeNs==cutsceneParticleBirth,
        "Cutscene particle froze with the held Board clock or changed its visual birth");
    Require(portalParticle->mLastUpdateTimeNs==static_cast<std::int64_t>(battleTime) &&
        portalParticle->EmitterRosterEmitterAt(0)->mBirthTimeNs==portalParticleBirth,
        "Globally scheduled portal particle aged while Board time was held");
    auto* particle=app.AddTodParticle(0,0,0,PARTICLE_PEA_SPLAT,
        static_cast<std::int64_t>(battleTime));
    auto* visualParticle=app.AddTodParticle(0,0,0,PARTICLE_PEA_SPLAT,
        holder.mVisualTimeNs);
    visualParticle->mIsAttachment=true; // This visual fixture owns its update timestamp.
    Require(particle->mLastUpdateTimeNs==static_cast<std::int64_t>(battleTime) &&
        particle->EmitterRosterEmitterAt(0)->mBirthTimeNs==static_cast<std::int64_t>(battleTime),
        "Battle particle creation did not use the Board clock");
    const auto particleBirth=particle->EmitterRosterEmitterAt(0)->mBirthTimeNs;
    auto* boardParticle=app.AddTodParticle(0,0,0,PARTICLE_PEA_SPLAT,
        static_cast<std::int64_t>(battleTime));
    const auto visualParticleBirth=visualParticle->EmitterRosterEmitterAt(0)->mBirthTimeNs;
    AttachmentID attachment=ATTACHMENTID_NULL;
    AttachParticle(attachment,particle,0,0);
    holder.mVisualTimeNs+=step;
    app.mEffectSystem->UpdateAt(holder.mVisualTimeNs,static_cast<std::int64_t>(battleTime));
    AttachmentUpdateAndMoveAt(attachment,0,0,static_cast<std::int64_t>(battleTime));
    visualParticle->Update(step,holder.mVisualTimeNs);
    Require(particle->mLastUpdateTimeNs==static_cast<std::int64_t>(battleTime) &&
        particle->EmitterRosterEmitterAt(0)->mBirthTimeNs==particleBirth &&
        boardParticle->mLastUpdateTimeNs==static_cast<std::int64_t>(battleTime) &&
        static_cast<std::int64_t>(battleTime)-particleBirth==0 &&
        holder.mVisualTimeNs-visualParticleBirth==static_cast<std::int64_t>(step) &&
        visualParticle->mLastUpdateTimeNs==holder.mVisualTimeNs,
        "Battle particle birth or attachment update consumed the visual epoch");
    AttachmentDie(attachment);
    visualParticle->ParticleSystemDie();
    boardParticle->ParticleSystemDie();
    board.RemoveCutsceneZombies();
    app.mGameScene=SCENE_PLAYING;
    holder.mVisualTimeNs+=step;
    board.Update();
    Require(board.mLevelTimeNs==battleTime+step && holder.mTimeDomains[holder.mReanimations.DataArrayIndexOf(body)]==ReanimTimeDomain::Battle &&
        body->mLastUpdateTimeNs==static_cast<std::int64_t>(board.mLevelTimeNs) &&
        body->SamplePlayback().phaseNs!=phase,
        "Resuming survival did not advance exactly one battle step");
    Require(portalParticle->mLastUpdateTimeNs==static_cast<std::int64_t>(board.mLevelTimeNs) &&
        portalParticle->EmitterRosterEmitterAt(0)->mBirthTimeNs==portalParticleBirth,
        "Portal particle did not resume through the global Board effect update");
    portal.GridItemDie();
    std::cout<<"PASS survival owner clocks "<<hz<<" Hz: intermission, immediate intro freeze, pre-pan/scroll, retained actors, visual preview/effects and resume\n";
}

void PoolSparkleOwnerClockRuntime(TestApp& app, int hz)
{
    auto& holder=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        Board* board;
        PoolEffect* pool;
        GameMode mode;
        GameScenes scene;
        std::int64_t visual, battle;
        std::uint64_t interval=Sexy::gUpdateIntervalNs;
        double ticks=Sexy::gUpdateTicks, steps=Sexy::gStepsPerLegacyTick;
        ~Restore() {
            app.mBoard=board; app.mPoolEffect=pool; app.mGameMode=mode; app.mGameScene=scene;
            auto& holder=*app.mEffectSystem->mReanimationHolder;
            holder.mVisualTimeNs=visual; holder.mBattleTimeNs=battle;
            Sexy::gUpdateIntervalNs=interval; Sexy::gUpdateTicks=ticks; Sexy::gStepsPerLegacyTick=steps;
        }
    } restore{app,app.mBoard,app.mPoolEffect,app.mGameMode,app.mGameScene,holder.mVisualTimeNs,holder.mBattleTimeNs};
    app.mGameMode=GAMEMODE_SURVIVAL_ENDLESS_STAGE_3;
    app.mGameScene=SCENE_LEVEL_INTRO;
    PoolEffect pool;
    pool.PoolEffectInitialize();
    app.mPoolEffect=&pool;
    Board board(&app);
    // These focused fixtures skip InitLevel; initialize its ice rows explicitly
    // rather than interpreting recycled Board storage as an active ice trail.
    for (int row=0;row<MAX_GRID_SIZE_Y;++row) {
        board.mIceTimer[row]=0;
        board.mIceMinX[row]=BOARD_ICE_START;
        board.mIceParticleID[row]=PARTICLESYSTEMID_NULL;
    }
    app.mBoard=&board;
    board.mBackground=BACKGROUND_3_POOL;
    board.mDrawCount=0; // Keep the chooser UI out of this owner-clock fixture.
    holder.mVisualTimeNs=900'000'000'000;
    holder.mBattleTimeNs=0;
    const auto step=1'000'000'000ULL/hz;
    Sexy::gUpdateIntervalNs=step;
    Sexy::gUpdateTicks=100.0/hz;
    Sexy::gStepsPerLegacyTick=hz/100.0;
    board.mNumWaves=20;
    board.mZombieCountDown=1'000'000;
    // This fixture constructs Board without InitLevel/InitZombieWaves. Seed all
    // scheduler inputs explicitly; recycled heap contents must not spawn a wave.
    board.mZombieCountDownStart=board.mZombieCountDown;
    board.mCurrentWave=0;
    board.mHugeWaveCountDown=0;
    board.mRiseFromGraveCounter=0;
    board.mZombieHealthToNextWave=-1;
    board.mSunCountDown=1'000'000;
    for (int row=0;row<MAX_GRID_SIZE_Y;++row) {
        board.mIceTimer[row]=0;
        board.mIceMinX[row]=0;
    }
    // The first new-level intro owns the water on Visual, even before the
    // first draw. Its Board clock stays zero throughout the initial pan/chooser.
    board.Update();
    auto* sparkle=app.ParticleTryToGet(board.mPoolSparklyParticleID);
    Require(sparkle && sparkle->mLastUpdateTimeNs==holder.mVisualTimeNs && board.mLevelTimeNs==0,
        "Initial intro failed to bind pool sparkles to supplied Visual time");
    auto* emitter=sparkle->EmitterRosterEmitterAt(0);
    const auto birth=emitter->mBirthTimeNs;
    board.FreezeEffectsForCutscene(true);
    Require(!sparkle->mDontUpdate,"Initial intro incorrectly froze its Visual pool sparkles");
    for (int frame=0;frame<hz/2;++frame) {
        holder.mVisualTimeNs+=step;
        board.Update();
    }
    int introLive=0;
    for (int i=0;i<sparkle->EmitterRosterCount();++i)
        introLive+=sparkle->EmitterRosterEmitterAt(i)->ParticleRosterCount();
    Require(introLive>0 && board.mLevelTimeNs==0 &&
        sparkle->mLastUpdateTimeNs==holder.mVisualTimeNs && emitter->mBirthTimeNs==birth &&
        pool.mSampleTimeNs-pool.mBirthTimeNs>0,
        "New-level intro water did not animate on supplied Visual time");
    const auto introAge=sparkle->mLastUpdateTimeNs-emitter->mBirthTimeNs;
    const auto introWaterAge=pool.mSampleTimeNs-pool.mBirthTimeNs;
    board.mPaused=true;
    holder.mVisualTimeNs+=3'000'000'000;
    board.Update();
    Require(sparkle->mLastUpdateTimeNs-emitter->mBirthTimeNs==introAge &&
        pool.mSampleTimeNs-pool.mBirthTimeNs==introWaterAge,
        "Paused new-level intro advanced water ages");
    board.mPaused=false;
    app.mGameScene=SCENE_PLAYING;
    holder.mVisualTimeNs+=step;
    board.Update();
    Require(!board.mPoolIntroVisual && board.mLevelTimeNs==step &&
        sparkle->mLastUpdateTimeNs-emitter->mBirthTimeNs==introAge+step &&
        pool.mSampleTimeNs-pool.mBirthTimeNs==introWaterAge+step,
        "First battle update lost the intro phase or inherited a Visual-clock gap");
    for (int frame=0;frame<hz/2;++frame) {
        holder.mVisualTimeNs+=step;
        board.Update();
    }
    int live=0;
    for (int i=0;i<sparkle->EmitterRosterCount();++i)
        live+=sparkle->EmitterRosterEmitterAt(i)->ParticleRosterCount();
    Require(live>0 && sparkle->mLastUpdateTimeNs==static_cast<std::int64_t>(board.mLevelTimeNs),
        "Combat pool sparkles did not consume Board time");
    const auto age=sparkle->mLastUpdateTimeNs-emitter->mBirthTimeNs;
    const auto waterAge=pool.mSampleTimeNs-pool.mBirthTimeNs;
    board.mPaused=true;
    holder.mVisualTimeNs+=10'000'000'000;
    board.Update();
    Require(sparkle->mLastUpdateTimeNs-emitter->mBirthTimeNs==age &&
        pool.mSampleTimeNs-pool.mBirthTimeNs==waterAge,
        "Paused water consumed advancing Visual time");
    board.mPaused=false;
    holder.mVisualTimeNs+=step;board.Update();
    Require(sparkle->mLastUpdateTimeNs-emitter->mBirthTimeNs==age+step &&
        pool.mSampleTimeNs-pool.mBirthTimeNs==waterAge+step,
        "Water resume caught up paused Visual time");
    // Exercise actual IceZombies, not a fixture-only write to mDontUpdate.
    Plant ice;
    ice.mBoard=&board;ice.mApp=&app;
    ice.IceZombies(board.mLevelTimeNs,holder.mVisualTimeNs,board.mLevelTimeNs);
    Require(sparkle->mDontUpdate && board.mIceTrapCounter>0,
        "Ice-shroom did not freeze pool sparkle like GOTY 1073 0x469EFF");
    const auto frozenAge=sparkle->mLastUpdateTimeNs-emitter->mBirthTimeNs;
    const auto frozenWater=pool.mSampleTimeNs-pool.mBirthTimeNs;
    for (int frame=0;frame<5;++frame) {
        holder.mVisualTimeNs+=step;board.Update();
    }
    Require(sparkle->mLastUpdateTimeNs-emitter->mBirthTimeNs==frozenAge &&
        pool.mSampleTimeNs-pool.mBirthTimeNs==frozenWater,
        "Ice-shroom failed to freeze water and sparkles together");
    board.FreezeEffectsForCutscene(false);
    Require(sparkle->mDontUpdate,"Cutscene resume overrode the active ice freeze");
    board.mIceTrapCounter.mNanoseconds=step;
    holder.mVisualTimeNs+=step;board.Update();
    Require(!sparkle->mDontUpdate && board.mIceTrapCounter==0,
        "Ice expiry did not release the sparkle freeze");
    holder.mVisualTimeNs+=step;board.Update();
    Require(sparkle->mLastUpdateTimeNs-emitter->mBirthTimeNs==frozenAge+step &&
        pool.mSampleTimeNs-pool.mBirthTimeNs==frozenWater+step,
        "Ice expiry jumped water phase instead of resuming one Board interval");
    app.mGameScene=SCENE_LEVEL_INTRO;
    board.FreezeEffectsForCutscene(true);
    const auto repickAge=sparkle->mLastUpdateTimeNs-emitter->mBirthTimeNs;
    holder.mVisualTimeNs+=3'000'000'000;
    board.Update();
    Require(sparkle->mLastUpdateTimeNs-emitter->mBirthTimeNs==repickAge,
        "Repick consumed Visual time for retained water effects");
    board.FreezeEffectsForCutscene(false);
    app.mGameScene=SCENE_PLAYING;
    holder.mVisualTimeNs+=step;board.Update();
    Require(sparkle->mLastUpdateTimeNs-emitter->mBirthTimeNs==repickAge+step,
        "Repick resume failed to consume exactly one Board interval");
    std::cout<<"PASS pool Board clock "<<hz<<" Hz: Visual intro, paused freeze, phase-preserving Battle handoff, IceZombies and repick resume\n";
}

void PauseDialogWallClockRuntime(TestApp& app, Board& board)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    const auto wallNow=[] {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    };
    struct RestorePause {
        TestApp& app;
        Board& board;
        Board* previousBoard;
        bool paused;
        std::int64_t visual, battle;
        unsigned long long interval=Sexy::gUpdateIntervalNs;
        double ticks=Sexy::gUpdateTicks, steps=Sexy::gStepsPerLegacyTick;
        ~RestorePause() {
            app.KillDialog(Dialogs::DIALOG_PAUSED);
            app.ProcessSafeDeleteList();
            board.Pause(paused);
            app.mBoard=previousBoard;
            auto& clocks=*app.mEffectSystem->mReanimationHolder;
            clocks.mVisualTimeNs=visual;
            clocks.mBattleTimeNs=battle;
            Sexy::gUpdateIntervalNs=interval;
            Sexy::gUpdateTicks=ticks;
            Sexy::gStepsPerLegacyTick=steps;
        }
    } restore{app,board,app.mBoard,board.mPaused,clocks.mVisualTimeNs,clocks.mBattleTimeNs};
    app.mBoard=&board;
    clocks.mVisualTimeNs=7'000'000'000;
    clocks.mBattleTimeNs=9'000'000'000;
    const auto before=wallNow();
    app.DoPauseDialog();
    auto* dialog=static_cast<LawnDialog*>(app.GetDialog(Dialogs::DIALOG_PAUSED));
    Require(dialog && dialog->mReanimation->mReanim,
        "Pause dialog did not create its newspaper animation");
    auto& animation=*dialog->mReanimation->mReanim;
    Require(board.mPaused && animation.mLastUpdateTimeNs>=before &&
        animation.mLastUpdateTimeNs<=wallNow(),
        "Pause newspaper was created on a frozen game clock instead of steady_clock");
    // Simulate time since the preceding UI update without sleeping or advancing
    // either game clock. The real widget update must sample the wall clock.
    animation.RebaseTime(wallNow()-123'456'789);
    const auto previous=animation.mLastUpdateTimeNs;
    const auto phase=animation.SamplePlayback().phaseNs;
    const auto boardTime=board.mLevelTimeNs;
    Sexy::ApplySimulationStep(0);
    app.UpdateFrames();
    Require(animation.mLastUpdateTimeNs>=previous+123'456'789 &&
        animation.mLastUpdateTimeNs<=wallNow() && animation.SamplePlayback().phaseNs!=phase,
        "Pause newspaper did not advance with wall time through the actual widget update");
    Require(board.mLevelTimeNs==boardTime && clocks.mVisualTimeNs==7'000'000'000 &&
        clocks.mBattleTimeNs==9'000'000'000,
        "Pause newspaper advanced a frozen Board or Visual clock");
    std::cout<<"PASS pause newspaper: steady-clock creation and actual UI update, zero simulation step, frozen Board/Visual clocks\n";
}

void InitialGraveAppearanceRuntime(TestApp& app, Board& board)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct RestoreGrave {
        TestApp& app;
        Board& board;
        GameMode mode;
        GameScenes scene;
        std::uint64_t battle;
        std::int64_t visual;
        bool enabled;
        std::array<GridSquareType,MAX_GRID_SIZE_Y> squares;
        unsigned long long interval=Sexy::gUpdateIntervalNs;
        double ticks=Sexy::gUpdateTicks, steps=Sexy::gStepsPerLegacyTick;
        ~RestoreGrave() {
            board.mGridItems.DataArrayFreeAll();
            board.mLevelTimeNs=battle;
            board.mEnableGraveStones=enabled;
            app.mGameMode=mode;
            app.mGameScene=scene;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            for (int row=0;row<MAX_GRID_SIZE_Y;++row)
                board.mGridSquareType[3][row]=squares[row];
            Sexy::gUpdateIntervalNs=interval;
            Sexy::gUpdateTicks=ticks;
            Sexy::gStepsPerLegacyTick=steps;
        }
    } restore{app,board,app.mGameMode,app.mGameScene,board.mLevelTimeNs,
        clocks.mVisualTimeNs,board.mEnableGraveStones,{}};
    for (int row=0;row<MAX_GRID_SIZE_Y;++row)
    {
        restore.squares[row]=board.mGridSquareType[3][row];
        board.mGridSquareType[3][row]=GRIDSQUARE_GRASS;
    }
    Require(board.mGridItems.mSize==0,"Grave fixture requires an empty Board");
    app.mGameMode=GAMEMODE_ADVENTURE;
    app.mGameScene=SCENE_LEVEL_INTRO;
    board.mLevelTimeNs=0;
    board.mEnableGraveStones=false;
    MTRand levelRng(0x601982);
    board.AddGraveStones(3,1,levelRng);
    GridItem* grave=nullptr;
    Require(board.IterateGridItems(grave) &&
        static_cast<std::int64_t>(board.mLevelTimeNs)-grave->mStateStartTimeNs>=1'000'000'000,
        "Initial grave did not start fully risen before any Board update");
    grave->mGridItemState=GRIDITEM_STATE_NORMAL;
    auto& render=*app.mDDInterface->mRenderInterface;
    std::unique_ptr<Image> target(render.CreateRenderImage(160,140));
    const auto capture=[&] {
        {
            GpuImageDrawScope destination(render,target.get());
            Graphics g(target.get());
            g.mColor=Color::Black;
            g.FillRect(0,0,160,140);
            const int x=grave->mGridX,y=grave->mGridY;
            g.mTransX=20-(board.GridToPixelX(x,y)+board.mGridCelOffset[x][y][0]-4);
            g.mTransY=110-(board.GridToPixelY(x,y)+91+board.mGridCelOffset[x][y][1]-9);
            grave->DrawGridItem(&g);
        }
        render.Flush();
        return render.ReadImagePixels(*target).pixels;
    };
    const auto mature=capture();
    Require(std::any_of(mature.begin(),mature.end(),[](unsigned pixel){return (pixel&0xffffff)!=0;}),
        "Mature grave fixture did not draw its production textures");
    const auto birth=grave->mStateStartTimeNs;
    Sexy::ApplySimulationStep(37'123'457);
    for (int i=0;i<100;++i)
    {
        clocks.mVisualTimeNs+=Sexy::gUpdateIntervalNs;
        board.UpdateGridItems();
    }
    Require(grave->mStateStartTimeNs==birth && capture()==mature,
        "Intro hid a fully risen initial grave while Board time was frozen");
    // Whack-a-Zombie creates its initial graves through a separate caller.
    board.mChallenge->WhackAZombiePlaceGraves(1);
    Require(board.mGridItems.mSize==2,"Whack-a-Zombie did not place its initial grave");
    GridItem* initial=nullptr;
    while (board.IterateGridItems(initial))
        Require(static_cast<std::int64_t>(board.mLevelTimeNs)-initial->mStateStartTimeNs>=1'000'000'000,
            "Whack-a-Zombie initial grave was not fully risen");
    board.mEnableGraveStones=true;
    for (int i=0;i<50;++i)
    {
        clocks.mVisualTimeNs+=Sexy::gUpdateIntervalNs;
        board.UpdateGridItems();
    }
    Require(board.mLevelTimeNs==0 && capture()==mature,
        "Initial grave changed its full-height pixels during the intro");
    app.mGameScene=SCENE_PLAYING;
    board.mLevelTimeNs+=Sexy::gUpdateIntervalNs;
    board.UpdateGridItems();
    Require(capture()==mature,"Entering battle hid an already visible grave");
    Require(board.CanPlantAt(grave->mGridX,grave->mGridY,SEED_PEASHOOTER)==PlantingReason::PLANTING_NOT_ON_GRAVE &&
        board.CanPlantAt(grave->mGridX,grave->mGridY,SEED_GRAVEBUSTER)==PlantingReason::PLANTING_OK,
        "Grave appearance changed its planting obstruction");
    const auto offset=offsetof(GridItem,mStateStartTimeNs);
    const auto wire=SaveClockProjection::Encode(*grave,offset,grave->mStateStartTimeNs);
    grave->mStateStartTimeNs=SaveClockProjection::Decode(*grave,offset,wire);
    board.mLevelTimeNs=0;
    SaveClockProjection::Bind(*grave);
    Require(capture()==mature,"Save/reload lost the grave's visible appearance age");
    grave=board.AddAGraveStone(4,2);
    grave->mGridItemState=GRIDITEM_STATE_NORMAL;
    Require(grave->mStateStartTimeNs>=static_cast<std::int64_t>(board.mLevelTimeNs) &&
        grave->mStateStartTimeNs<static_cast<std::int64_t>(board.mLevelTimeNs)+500'000'000,
        "Runtime grave lost its original staggered rise");
    const auto pending=capture();
    Require(std::none_of(pending.begin(),pending.end(),[](unsigned pixel){return (pixel&0xffffff)!=0;}),
        "Runtime grave was displayed before its rise began");
    for (int i=0;i<50;++i)
    {
        board.mLevelTimeNs+=Sexy::gUpdateIntervalNs;
        board.UpdateGridItems();
    }
    const auto risen=capture();
    Require(std::any_of(risen.begin(),risen.end(),[](unsigned pixel){return (pixel&0xffffff)!=0;}),
        "Runtime grave failed to finish its original rise");
    std::cout<<"PASS real grave pixels: initial full height, frozen intro, battle, planting obstruction, saved age and preserved runtime rise\n";
}

void MinigameBehaviorRuntime(TestApp& app)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct RestoreBlover {
        TestApp& app;
        Board* board;
        PlayerInfo* profile;
        ZenGarden* garden;
        PoolEffect* pool;
        GameMode mode;
        GameScenes scene;
        std::int64_t visual, battle;
        unsigned long long interval=Sexy::gUpdateIntervalNs;
        double ticks=Sexy::gUpdateTicks, steps=Sexy::gStepsPerLegacyTick;
        int mouseX=app.mWidgetManager->mLastMouseX, mouseY=app.mWidgetManager->mLastMouseY;
        ~RestoreBlover() {
            app.mBoard=board;app.mPlayerInfo=profile;app.mZenGarden=garden;app.mGameMode=mode;app.mGameScene=scene;
            app.mPoolEffect=pool;
            app.mWidgetManager->mLastMouseX=mouseX;app.mWidgetManager->mLastMouseY=mouseY;
            auto& clocks=*app.mEffectSystem->mReanimationHolder;
            clocks.mVisualTimeNs=visual;clocks.mBattleTimeNs=battle;
            Sexy::gUpdateIntervalNs=interval;Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
        }
    } restore{app,app.mBoard,app.mPlayerInfo,app.mZenGarden,app.mPoolEffect,app.mGameMode,app.mGameScene,
        clocks.mVisualTimeNs,clocks.mBattleTimeNs};
    PlayerInfo profile;
    ZenGarden garden;
    profile.mLevel=11;profile.mCoins=0;profile.mFinishedAdventure=0;
    app.mPlayerInfo=&profile;app.mZenGarden=&garden;app.mGameMode=GAMEMODE_ADVENTURE;app.mGameScene=SCENE_PLAYING;
    Board board(&app);
    board.mLevel=11;board.mCurrentWave=6;board.mNumWaves=20;board.mTotalSpawnedWaves=0;
    board.mBackground=BACKGROUND_3_POOL;
    for(int row=0;row<MAX_GRID_SIZE_Y;++row)
        board.mPlantRow[row]=(row==2||row==3)?PLANTROW_POOL:PLANTROW_NORMAL;
    for(const int hz:{30,100,240})
    for(int row=0;row<MAX_GRID_SIZE_Y;++row)
    {
        board.mDroppedFirstCoin=false;
        Zombie* balloon=board.AddZombieInRow(ZOMBIE_BALLOON,row,0);
        Plant blover;
        blover.BlowAwayFliers();
        Require(balloon->mBlowingAway,"Blover did not mark the flying balloon");
        const auto step=1'000'000'000ULL/hz;
        for(int n=0;n<hz*2 && !balloon->mDead;++n)
        {
            Sexy::ApplySimulationStep(step);
            board.mLevelTimeNs+=step;clocks.mVisualTimeNs+=step;
            balloon->Update(static_cast<std::int64_t>(board.mLevelTimeNs),
                clocks.mVisualTimeNs,static_cast<std::int64_t>(board.mLevelTimeNs),step);
        }
        Require(balloon->mDead && balloon->mDroppedLoot,"Blown balloon never reached its loot exit");
        Coin* coin=nullptr;
        Require(board.IterateCoins(coin) && !coin->mDead,"Guaranteed first-coin fixture did not drop a coin");
        const auto hitbox=balloon->GetZombieRect();
        const int expectedY=hitbox.mY+hitbox.mHeight/4-8;
        std::cout<<"Blover loot "<<hz<<" Hz row="<<row<<" zombie=("<<balloon->mX<<','<<balloon->mY
            <<") coin=("<<coin->mPosX<<','<<coin->mPosY<<") ground="<<coin->mGroundY<<'\n';
        Require(coin->mPosY==expectedY,"Blover coin lost its source row at creation");
        Require(coin->mPosX==hitbox.mX+hitbox.mWidth/2-50,
            "Blover coin did not use the source zombie's right-edge position");
        board.mCoins.DataArrayFreeAll();board.mZombies.DataArrayFreeAll();board.RebuildZombieRowBucket();
    }
    std::cout<<"PASS actual Blover loot: every pool row, right-edge exit and per-row coin creation at 30/100/240 Hz\n";

    auto& challenge=*board.mChallenge;
    app.mGameMode=GAMEMODE_MINIGAME_ZOMBIQUARIUM;
    board.mSunMoney=25;
    HitResult empty;
    empty.mObject=nullptr;empty.mObjectType=OBJECT_TYPE_NONE;
    Require(challenge.MouseDown(300,200,-1,&empty) && board.mGridItems.mSize==0 && board.mSunMoney==25,
        "Aquarium right click must only tap glass, without food or payment");
    for(const int click:{1,0,2})
        Require(challenge.MouseDown(300,200,click,&empty),"Aquarium left click did not reach paid feeding");
    Require(board.mGridItems.mSize==3 && board.mSunMoney==10,
        "Aquarium left feeding must spend five sun per brain");
    challenge.MouseDown(300,200,1,&empty);
    Require(board.mGridItems.mSize==3 && board.mSunMoney==10,
        "Aquarium did not retain the retail three-brain cap without charging a rejected feed");
    board.mGridItems.DataArrayFreeAll();board.mSunMoney=4;
    challenge.MouseDown(300,200,1,&empty);
    Require(board.mGridItems.mSize==0 && board.mSunMoney==4,"Aquarium accepted food without enough sun");
    board.mSunMoney=5;
    challenge.MouseDown(300,200,1,&empty);
    Require(board.mGridItems.mSize==1 && board.mSunMoney==0,"Aquarium exact-cost feed failed");
    board.mGridItems.DataArrayFreeAll();
    std::cout<<"PASS actual Aquarium input: left/touch feeding, right glass tap, five-sun cost and three-brain cap\n";

    app.mGameMode=GAMEMODE_MINIGAME_BEGHOULED_TWIST;
    board.mBackground=BACKGROUND_1_DAY;
    challenge.mChallengeState=STATECHALLENGE_NORMAL;
    challenge.mChallengeStateCounter=100000;
    const std::array<SeedType,4> seeds{SEED_PEASHOOTER,SEED_SUNFLOWER,SEED_SNOWPEA,SEED_WALLNUT};
    for(int i=0;i<4;++i) board.NewPlant(2+i%2,1+i/2,seeds[i],SEED_NONE);
    const int localX=board.GridToPixelX(2,1)+60, localY=board.GridToPixelY(2,1)+60;
    int expectedX=-1,expectedY=-1;
    Require(challenge.BeghouledTwistSquareFromMouse(localX,localY,expectedX,expectedY),"Twist fixture missed its local square");
    for(const int offset:{0,-220,100})
    {
        board.Move(offset,17);
        app.mWidgetManager->mLastMouseX=localX+offset;
        app.mWidgetManager->mLastMouseY=localY+17;
        challenge.UpdateBeghouled();
        Require(challenge.mChallengeGridX==expectedX && challenge.mChallengeGridY==expectedY,
            "Twist hover used global mouse coordinates instead of Board-local coordinates");
    }
    board.mPlants.DataArrayFreeAll();board.RebuildPlantCellBucket();board.Move(0,0);
    std::cout<<"PASS actual Beghouled Twist hover: matching click coordinates across negative/positive Board offsets\n";

    app.mGameMode=GAMEMODE_MINIGAME_SPEED;
    board.mBackground=BACKGROUND_3_POOL;
    board.mLevelTimeNs=0;clocks.mBattleTimeNs=0;clocks.mVisualTimeNs=0;
    board.mZombieCountDown=100000;board.mSunCountDown=100000;
    PoolEffect pool;
    pool.PoolEffectInitialize();app.mPoolEffect=&pool;
    auto* shooter=board.NewPlant(3,0,SEED_PEASHOOTER,SEED_NONE);
    shooter->mNextLaunchTimeNs=300'000'000;shooter->mLaunchWindowStartNs=0;
    auto* target=board.AddZombieInRow(ZOMBIE_NORMAL,0,0);
    target->mX=650;target->mVelocityX={};target->mBodyHealth=target->mBodyMaxHealth=1000000;
    target->mRootBindingGeneration=0;
    Require(shooter->FindTargetZombie(0,WEAPON_PRIMARY)==target,"Speed-mode fixture could not acquire its target");
    auto* body=app.ReanimationGet(shooter->mBodyReanimID);
    auto* independent=app.AddReanimation(0,0,0,REANIM_SUN,0,ReanimTimeDomain::Battle);
    auto* child=app.AddReanimation(0,0,0,REANIM_SUN,0,ReanimTimeDomain::Battle);
    auto* reference=app.AddReanimation(0,0,0,REANIM_SUN,0,ReanimTimeDomain::Battle);
    reference->mIsAttachment=true;
    for(auto* animation:{independent,child,reference})
    {
        animation->ApplyRate(6);
        animation->ChangeLoopType(REANIM_LOOP);
    }
    AttachReanim(independent->mTrackInstances[0].mAttachmentID,child,0,0);
    auto* childParticle=app.AddTodParticle(0,0,0,PARTICLE_POOL_SPARKLY,0);
    AttachParticle(child->mTrackInstances[0].mAttachmentID,childParticle,0,0);
    constexpr auto speedStep=37'123'457ULL;
    bool emitted=false;
    for(int i=0;i<20;++i)
    {
        Sexy::ApplySimulationStep(speedStep);clocks.mVisualTimeNs+=speedStep;
        board.Update();
        emitted|=board.mProjectiles.mSize>0;
        reference->UpdateAt((i+1)*speedStep);
        Require(independent->SamplePlayback().phaseNs==reference->SamplePlayback().phaseNs &&
            child->SamplePlayback().phaseNs==reference->SamplePlayback().phaseNs &&
            child->mLastUpdateTimeNs==static_cast<std::int64_t>(board.mLevelTimeNs) &&
            childParticle->mLastUpdateTimeNs==static_cast<std::int64_t>(board.mLevelTimeNs),
            "Speed mode failed to preserve the independent animation/attachment clock tree");
        Require(board.mLevelTimeNs==(i+1)*2*speedStep && clocks.mBattleTimeNs==board.mLevelTimeNs,
            "Speed mode repeated logic without advancing its actor timestamps");
        Require(body->mLastUpdateTimeNs==static_cast<std::int64_t>(board.mLevelTimeNs),
            "Speed-mode plant animation missed the second actor pass");
        Require(pool.mSampleTimeNs-pool.mBirthTimeNs==static_cast<std::int64_t>((i+1)*speedStep),
            "Speed mode doubled the independent water phase");
    }
    Require(emitted,"Speed-mode shooter failed to emit actual projectiles");
    auto* sparkle=app.ParticleGet(board.mPoolSparklyParticleID);
    auto* emitter=sparkle->EmitterRosterEmitterAt(0);
    Require(sparkle->mLastUpdateTimeNs-emitter->mBirthTimeNs==20*speedStep,
        "Speed mode doubled independent pool-sparkle age");
    std::cout<<"PASS actual Speed mode: double Board time, two actor passes, real shots, ordinary-speed water/sparkles and independent attachment trees\n";

    app.mGameMode=GAMEMODE_ADVENTURE;
    board.mBackground=BACKGROUND_1_DAY;
    board.ZombiesWon(target);
    Require(app.mGameScene==SCENE_ZOMBIES_WON,"Actual home invasion did not enter the loss scene");
    const auto frozen=board.mLevelTimeNs;
    const auto cutscene=board.mCutScene->mCutsceneTime.mNanoseconds;
    for(int i=0;i<10;++i)
    {
        Sexy::ApplySimulationStep(speedStep);clocks.mVisualTimeNs+=speedStep;board.Update();
        Require(board.mLevelTimeNs==frozen && clocks.mBattleTimeNs==frozen,
            "Home invasion advanced the frozen battle clock");
    }
    Require(board.mCutScene->mCutsceneTime.mNanoseconds>cutscene,"Freezing battle time stopped the loss cutscene");
    std::cout<<"PASS actual home invasion: Board clock frozen while the loss cutscene continues\n";
}

constexpr std::array kSpecialPlantAuditSeeds{SEED_CHERRYBOMB,SEED_POTATOMINE,SEED_CHOMPER,SEED_GRAVEBUSTER,
        SEED_HYPNOSHROOM,SEED_ICESHROOM,SEED_DOOMSHROOM,SEED_SQUASH,SEED_TANGLEKELP,
        SEED_JALAPENO,SEED_INSTANT_COFFEE,SEED_GARLIC,SEED_UMBRELLA,SEED_COBCANNON,SEED_IMITATER,
        SEED_WALLNUT,SEED_TALLNUT,SEED_PUMPKINSHELL,SEED_LILYPAD,SEED_FLOWERPOT,SEED_SPIKEWEED,
        SEED_SPIKEROCK,SEED_TORCHWOOD,SEED_PLANTERN,SEED_BLOVER,SEED_MAGNETSHROOM,SEED_GOLD_MAGNET,
        SEED_EXPLODE_O_NUT,SEED_GIANT_WALLNUT,SEED_SPROUT};

void PlantSpecialCatalogRuntime(TestApp& app)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    for(const auto seed:kSpecialPlantAuditSeeds)
    for(const int hz:{100,30,240,0})
    {
        app.mGameScene=SCENE_PLAYING;
        std::cout<<"Plant special begin seed="<<int(seed)<<" Hz="<<hz<<'\n';
        Sexy::SRand(0xF170A);Sexy::gLegacyTickPhaseNs=0;
        clocks.mVisualTimeNs=clocks.mBattleTimeNs=0;
        app.mGameMode=(seed==SEED_EXPLODE_O_NUT||seed==SEED_GIANT_WALLNUT)?GAMEMODE_MINIGAME_WALLNUT_BOWLING:
            seed==SEED_SPROUT?GAMEMODE_LIMBO_ZEN_GARDEN:GAMEMODE_ADVENTURE;
        Board board(&app);
        board.mLevel=31;board.mCurrentWave=6;board.mNumWaves=20;board.mTotalSpawnedWaves=0;
        board.mSeedBank->mNumPackets=0;
        board.mBackground=seed==SEED_INSTANT_COFFEE?BACKGROUND_3_POOL:BACKGROUND_4_FOG;
        for(int row=0;row<MAX_GRID_SIZE_Y;++row)
        {
            board.mPlantRow[row]=(row==2||row==3)?PLANTROW_POOL:PLANTROW_NORMAL;
            board.mIceMinX[row]=1000;board.mIceTimer[row]=0;board.mIceParticleID[row]=PARTICLESYSTEMID_NULL;
        }
        const int row=(seed==SEED_TANGLEKELP||seed==SEED_LILYPAD)?2:1;
        if(seed==SEED_GRAVEBUSTER) board.AddAGraveStone(3,row);
        Plant* sleeper=nullptr;
        if(seed==SEED_INSTANT_COFFEE)
        {
            sleeper=board.NewPlant(3,row,SEED_PUFFSHROOM,SEED_NONE);
            Require(sleeper->mIsAsleep,"Coffee fixture mushroom was not sleeping in daylight");
        }
        auto* plant=board.NewPlant(3,row,seed,seed==SEED_IMITATER?SEED_PEASHOOTER:SEED_NONE);
        const int startingHealth=plant->mPlantHealth;
        if(seed==SEED_SPROUT)
        {
            app.mPlayerInfo->mNumPottedPlants=1;
            app.mPlayerInfo->mPottedPlant[0].InitializePottedPlant(SEED_SUNFLOWER);
            app.mPlayerInfo->mPottedPlant[0].mPlantAge=PLANTAGE_SPROUT;
            plant->mPottedPlantIndex=0;
        }
        auto* target=board.AddZombieInRow(seed==SEED_MAGNETSHROOM?ZOMBIE_PAIL:
            seed==SEED_BLOVER?ZOMBIE_BALLOON:ZOMBIE_NORMAL,row,0);
        target->mX=plant->mX+(seed==SEED_CHOMPER?50:0);
        target->mRootBindingGeneration=0;target->mVelX=0;target->mVelocityX={};
        target->mBodyHealth=target->mBodyMaxHealth=1000000;
        app.ReanimationGet(target->mBodyReanimID)->ApplyRate(0);
        if(seed==SEED_TANGLEKELP)
        {
            target->mInPool=true;target->mZombieHeight=HEIGHT_ZOMBIE_NORMAL;
            target->mZombiePhase=PHASE_ZOMBIE_NORMAL;
        }
        // These scenarios test the plant action; a huge-health target keeps
        // explosion damage observable without replacing death with a stub.
        if(seed!=SEED_HYPNOSHROOM && seed!=SEED_WALLNUT && seed!=SEED_TALLNUT && seed!=SEED_PUMPKINSHELL)
            plant->mPlantHealth=1000000;
        if(seed==SEED_GRAVEBUSTER||seed==SEED_INSTANT_COFFEE||seed==SEED_UMBRELLA||seed==SEED_IMITATER||seed==SEED_COBCANNON||
            seed==SEED_LILYPAD||seed==SEED_FLOWERPOT||seed==SEED_TORCHWOOD||seed==SEED_PLANTERN||seed==SEED_GOLD_MAGNET||seed==SEED_SPROUT)
        {
            target->mX=700;target->mRootBindingGeneration=0;
        }
        if(seed==SEED_UMBRELLA) plant->DoSpecial(0,0,0);
        Projectile* cold=nullptr;
        Projectile* warm=nullptr;
        if(seed==SEED_TORCHWOOD)
        {
            cold=board.AddProjectile(plant->mX-50,plant->mY+20,0,row,PROJECTILE_SNOWPEA);
            warm=board.AddProjectile(plant->mX-70,plant->mY+20,0,row,PROJECTILE_PEA);
        }
        if(seed==SEED_GOLD_MAGNET)
        {
            board.AddCoin(plant->mX+150,plant->mY,COIN_GOLD,COIN_MOTION_COIN);
            plant->mState=STATE_MAGNETSHROOM_SUCKING;
            plant->PlayBodyReanim(0,ReanimTrackId::anim_attract,REANIM_PLAY_ONCE_AND_HOLD,20,12);
        }
        const auto duration=(seed==SEED_POTATOMINE||seed==SEED_COBCANNON)?20'000'000'000ULL:
            (seed==SEED_GRAVEBUSTER||seed==SEED_GOLD_MAGNET)?8'000'000'000ULL:4'000'000'000ULL;
        bool firedCob=false,seenCob=false,seenCrater=false,seenFrozen=false,seenReflecting=false;
        constexpr std::array<long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
        for(int stepIndex=0;board.mLevelTimeNs<duration;++stepIndex)
        {
            const auto step=std::min<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[stepIndex%jitter.size()],duration-board.mLevelTimeNs);
            Sexy::ApplySimulationStep(step);
            board.mLevelTimeNs+=step;clocks.mVisualTimeNs+=step;clocks.mBattleTimeNs=board.mLevelTimeNs;
            app.mEffectSystem->UpdateAt(clocks.mVisualTimeNs,clocks.mBattleTimeNs);
            board.UpdateGameObjects(step);
            if(seed==SEED_COBCANNON && !firedCob && plant->mState==STATE_COBCANNON_READY)
            {
                plant->CobCannonFire(board.mLevelTimeNs,650,board.GridToPixelY(0,row)+40);
                firedCob=true;
            }
            Projectile* projectile=nullptr;
            while(board.IterateProjectiles(projectile)) seenCob|=projectile->mProjectileType==PROJECTILE_COBBIG;
            seenCrater|=board.GetCraterAt(3,row)!=nullptr;
            seenFrozen|=target->mIceTrapEndTimeNs>static_cast<std::int64_t>(board.mLevelTimeNs);
            seenReflecting|=plant->mState==STATE_UMBRELLA_REFLECTING;
            app.mEffectSystem->ProcessDeleteQueue();
        }
        std::cout<<"Plant special end seed="<<int(seed)<<" Hz="<<hz<<" state="<<int(plant->mState)
            <<" dead="<<plant->mDead<<" targetDead="<<target->mDead<<" damage="<<1000000-target->mBodyHealth
            <<" targetRow="<<target->mRow<<'\n';
        switch(seed)
        {
        case SEED_CHERRYBOMB:case SEED_POTATOMINE:case SEED_DOOMSHROOM:case SEED_SQUASH:case SEED_JALAPENO:
            Require(plant->mDead && 1000000-target->mBodyHealth==1800,"Instant plant missed or repeated its damage action");
            if(seed==SEED_DOOMSHROOM) Require(seenCrater,"Doom-shroom did not create its crater");
            break;
        case SEED_CHOMPER:
            Require(target->mDead && plant->mState==STATE_CHOMPER_DIGESTING,"Chomper did not complete its swallow/digestion transition");break;
        case SEED_GRAVEBUSTER:
            Require(plant->mDead && !board.GetGraveStoneAt(3,row) && board.mGravesCleared==1,
                "Grave Buster did not remove exactly one grave");break;
        case SEED_HYPNOSHROOM:
            Require(plant->mDead && target->mMindControlled,"Eating Hypno-shroom did not hypnotize the eater");break;
        case SEED_ICESHROOM:
            Require(plant->mDead && seenFrozen,"Ice-shroom did not freeze its target");break;
        case SEED_TANGLEKELP:
            Require(plant->mDead && target->mDead,"Tangle Kelp did not drown its swimming target");break;
        case SEED_INSTANT_COFFEE:
            Require(plant->mDead && !sleeper->mIsAsleep && sleeper->mWakeUpEndTimeNs==0,
                "Coffee did not disappear and finish waking its mushroom");break;
        case SEED_GARLIC:
            Require(target->mRow!=row && !target->mDead,"Garlic eater failed to change to a permitted neighbouring row");break;
        case SEED_UMBRELLA:
            Require(seenReflecting && plant->mState==STATE_NOTREADY,"Umbrella did not enter reflecting and return to idle");break;
        case SEED_COBCANNON:
            Require(firedCob && seenCob,"Cob Cannon did not complete charging and launch a real cob");break;
        case SEED_IMITATER:
        {
            auto* replacement=board.GetTopPlantAt(3,row,TOPPLANT_ONLY_NORMAL_POSITION);
            Require(plant->mDead && replacement && replacement!=plant && replacement->mSeedType==SEED_PEASHOOTER &&
                replacement->mImitaterType==SEED_IMITATER,"Imitater did not publish its replacement in the cell bucket");break;
        }
        case SEED_WALLNUT:case SEED_TALLNUT:case SEED_PUMPKINSHELL:
            Require(startingHealth==(seed==SEED_TALLNUT?8000:4000) && !plant->mDead &&
                plant->mPlantHealth>0 && plant->mPlantHealth<startingHealth,
                "Defensive plant health or real chewing damage changed");break;
        case SEED_LILYPAD:case SEED_FLOWERPOT:
            Require(!plant->mDead && plant->mState==STATE_NOTREADY && plant->mStateCountdown==0,
                "Support plant did not finish its initial invulnerability");break;
        case SEED_SPIKEWEED:case SEED_SPIKEROCK:
            Require(target->mBodyHealth<1000000 && (1000000-target->mBodyHealth)%20==0 && !plant->mDead,
                "Spike ground damage did not reach the overlapping zombie");break;
        case SEED_TORCHWOOD:
            Require(cold->mProjectileType==PROJECTILE_PEA && warm->mProjectileType==PROJECTILE_FIREBALL,
                "Torchwood did not thaw snow peas and ignite peas exactly once");break;
        case SEED_PLANTERN:
            Require(!plant->mDead && app.ParticleTryToGet(plant->mParticleID),
                "Plantern lost its persistent shine particle");break;
        case SEED_BLOVER:
            Require(target->mDead && target->mBlowingAway && plant->mDead,"Blover did not blow away its balloon and disappear");break;
        case SEED_MAGNETSHROOM:
            Require(target->mHelmType==HELMTYPE_NONE && plant->mMagnetItems[0].mItemType!=MAGNET_ITEM_NONE,
                "Magnet-shroom did not remove and retain the metal helmet");break;
        case SEED_GOLD_MAGNET:
            Require(board.mCoinsCollected==Coin::GetCoinValue(COIN_GOLD),"Gold Magnet lost or duplicated its collected gold coin");break;
        case SEED_EXPLODE_O_NUT:
            Require(plant->mDead && 1000000-target->mBodyHealth==1800,"Explode-o-nut collision did not apply its explosion");break;
        case SEED_GIANT_WALLNUT:
            Require(target->mBodyHealth<=0,"Giant Wall-nut did not kill its collision target");break;
        case SEED_SPROUT:
            Require(!plant->mDead && plant->mPottedPlantIndex==0 && app.ReanimationTryToGet(plant->mBodyReanimID),
                "Garden sprout lost its owned plant animation");break;
        default:Require(false,"Unhandled plant-special audit case");
        }
    }
    app.mGameMode=GAMEMODE_ADVENTURE;
    std::cout<<"PASS actual Plant specials/support: 30 types, real damage/status/consumption, defenses, torchwood, magnets, bowling and garden sprout at four schedules\n";
}

void PlantAttackCatalogRuntime(TestApp& app)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        Board* board=app.mBoard;
        PlayerInfo* player=app.mPlayerInfo;
        ZenGarden* garden=app.mZenGarden;
        GameMode mode=app.mGameMode;
        GameScenes scene=app.mGameScene;
        unsigned long long interval=Sexy::gUpdateIntervalNs, phase=Sexy::gLegacyTickPhaseNs;
        int whole=Sexy::gWholeLegacyTicks;
        double ticks=Sexy::gUpdateTicks, steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        ~Restore() {
            app.mBoard=board;app.mPlayerInfo=player;app.mZenGarden=garden;
            app.mGameMode=mode;app.mGameScene=scene;
            Sexy::gUpdateIntervalNs=interval;Sexy::gLegacyTickPhaseNs=phase;Sexy::gWholeLegacyTicks=whole;
            Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
        }
    } restore{app};
    PlayerInfo profile;
    ZenGarden garden;
    profile.mLevel=31;profile.mFinishedAdventure=1;
    app.mPlayerInfo=&profile;app.mZenGarden=&garden;
    app.mGameMode=GAMEMODE_ADVENTURE;app.mGameScene=SCENE_PLAYING;
    struct Attack {
        SeedType seed;
        int projectiles;
        unsigned int types;
        int damage;
        int coins;
        unsigned int coinTypes;
    };
    constexpr auto projectile=[](ProjectileType type){return 1u<<type;};
    constexpr auto coin=[](CoinType type){return 1u<<type;};
    const std::array cases{
        Attack{SEED_PEASHOOTER,1,projectile(PROJECTILE_PEA),0,0,0},
        Attack{SEED_SNOWPEA,1,projectile(PROJECTILE_SNOWPEA),0,0,0},
        Attack{SEED_REPEATER,2,projectile(PROJECTILE_PEA),0,0,0},
        Attack{SEED_PUFFSHROOM,1,projectile(PROJECTILE_PUFF),0,0,0},
        Attack{SEED_FUMESHROOM,0,0,20,0,0},
        Attack{SEED_SCAREDYSHROOM,1,projectile(PROJECTILE_PUFF),0,0,0},
        Attack{SEED_THREEPEATER,3,projectile(PROJECTILE_PEA),0,0,0},
        Attack{SEED_SEASHROOM,1,projectile(PROJECTILE_PUFF),0,0,0},
        Attack{SEED_CACTUS,1,projectile(PROJECTILE_SPIKE),0,0,0},
        Attack{SEED_SPLITPEA,3,projectile(PROJECTILE_PEA),0,0,0},
        Attack{SEED_STARFRUIT,5,projectile(PROJECTILE_STAR),0,0,0},
        Attack{SEED_CABBAGEPULT,1,projectile(PROJECTILE_CABBAGE),0,0,0},
        Attack{SEED_KERNELPULT,1,projectile(PROJECTILE_KERNEL)|projectile(PROJECTILE_BUTTER),0,0,0},
        Attack{SEED_MELONPULT,1,projectile(PROJECTILE_MELON),0,0,0},
        Attack{SEED_GATLINGPEA,4,projectile(PROJECTILE_PEA),0,0,0},
        Attack{SEED_GLOOMSHROOM,0,0,80,0,0},
        Attack{SEED_CATTAIL,2,projectile(PROJECTILE_SPIKE),0,0,0},
        Attack{SEED_WINTERMELON,1,projectile(PROJECTILE_WINTERMELON),0,0,0},
        Attack{SEED_LEFTPEATER,2,projectile(PROJECTILE_PEA),0,0,0},
        Attack{SEED_SUNFLOWER,0,0,0,1,coin(COIN_SUN)},
        Attack{SEED_TWINSUNFLOWER,0,0,0,2,coin(COIN_SUN)},
        Attack{SEED_SUNSHROOM,0,0,0,1,coin(COIN_SMALLSUN)},
        Attack{SEED_MARIGOLD,0,0,0,1,coin(COIN_SILVER)|coin(COIN_GOLD)},
    };
    std::array<unsigned char,NUM_SEED_TYPES> coverage{};
    for(const auto& test:cases) ++coverage[test.seed];
    for(const auto seed:kSpecialPlantAuditSeeds) ++coverage[seed];
    Require(std::all_of(coverage.begin(),coverage.end(),[](auto count){return count==1;}),
        "Plant behavior catalog must audit every actual SeedType exactly once");
    for(const auto& test:cases)
    for(const int hz:{100,30,240,0})
    {
        Sexy::SRand(0x4A715);
        clocks.mVisualTimeNs=0;clocks.mBattleTimeNs=0;
        Sexy::gLegacyTickPhaseNs=0;
        Board board(&app);
        board.mLevel=31;board.mCurrentWave=6;board.mNumWaves=20;board.mTotalSpawnedWaves=0;
        board.mSeedBank->mNumPackets=0;
        board.mBackground=BACKGROUND_4_FOG;
        for(int row=0;row<MAX_GRID_SIZE_Y;++row)
        {
            board.mPlantRow[row]=(row==2||row==3)?PLANTROW_POOL:PLANTROW_NORMAL;
            board.mIceMinX[row]=1000;board.mIceTimer[row]=0;
            board.mIceParticleID[row]=PARTICLESYSTEMID_NULL;
        }
        const int row=(test.seed==SEED_SEASHROOM||test.seed==SEED_CATTAIL)?2:1;
        auto* plant=board.NewPlant(3,row,test.seed,SEED_NONE);
        plant->mPlantHealth=1000000;
        plant->mNextLaunchTimeNs=1'000'000'000;plant->mLaunchWindowStartNs=0;
        plant->mLaunchRate=1000000;
        const auto targetAt=[&](int targetRow,float x) {
            auto* target=board.AddZombieInRow(ZOMBIE_NORMAL,targetRow,0);
            target->mX=x;target->mRootBindingGeneration=0;target->mVelocityX={};target->mVelX=0;
            target->mBodyHealth=target->mBodyMaxHealth=1000000;
            app.ReanimationGet(target->mBodyReanimID)->ApplyRate(0);
            return target;
        };
        auto* target=targetAt(row,test.seed==SEED_GLOOMSHROOM?plant->mX+60:500);
        if(test.seed==SEED_THREEPEATER) {targetAt(row-1,500);targetAt(row+1,500);}
        if(test.seed==SEED_SPLITPEA||test.seed==SEED_LEFTPEATER||test.seed==SEED_STARFRUIT)
            targetAt(row,100);
        Require(plant->MakesSun()||test.seed==SEED_MARIGOLD||
            plant->FindTargetZombie(row,WEAPON_PRIMARY)||plant->FindTargetZombie(row,WEAPON_SECONDARY),
            "Plant catalog fixture did not acquire its intended target");
        std::vector<unsigned int> shots,coins;
        unsigned int shotTypes=0,coinTypes=0;
        constexpr std::array<long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
        for(int stepIndex=0;board.mLevelTimeNs<4'000'000'000ULL;++stepIndex)
        {
            const auto step=std::min<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[stepIndex%jitter.size()],
                4'000'000'000ULL-board.mLevelTimeNs);
            Sexy::ApplySimulationStep(step);
            board.mLevelTimeNs+=step;clocks.mVisualTimeNs+=step;clocks.mBattleTimeNs=board.mLevelTimeNs;
            app.mEffectSystem->UpdateAt(clocks.mVisualTimeNs,clocks.mBattleTimeNs);
            board.UpdateGameObjects(step);
            Projectile* bullet=nullptr;
            while(board.IterateProjectiles(bullet))
            {
                const auto id=board.mProjectiles.DataArrayGetID(bullet);
                if(std::find(shots.begin(),shots.end(),id)==shots.end()) shots.push_back(id);
                shotTypes|=projectile(bullet->mProjectileType);
            }
            Coin* drop=nullptr;
            while(board.IterateCoins(drop))
            {
                const auto id=board.mCoins.DataArrayGetID(drop);
                if(std::find(coins.begin(),coins.end(),id)==coins.end()) coins.push_back(id);
                coinTypes|=coin(drop->mType);
            }
            app.mEffectSystem->ProcessDeleteQueue();
        }
        std::cout<<"Plant attack seed="<<int(test.seed)<<" Hz="<<hz<<" shots="<<shots.size()
            <<" types="<<shotTypes<<" damage="<<1000000-target->mBodyHealth<<" coins="<<coins.size()<<'\n';
        Require(shots.size()==test.projectiles,"Plant catalog lost or duplicated a projectile in one attack");
        Require((shotTypes&~test.types)==0 && (test.projectiles==0||shotTypes!=0),"Plant catalog emitted the wrong projectile type");
        if(test.damage) Require(1000000-target->mBodyHealth==test.damage,"Plant catalog area-damage pulse count changed");
        Require(coins.size()==test.coins && (coinTypes&~test.coinTypes)==0,
            "Plant catalog production count/type changed");
    }
    std::cout<<"PASS actual Plant attack catalog: 23 types, first-volley counts/types, area pulses and production at 100/30/240Hz and jitter\n";
    PlantSpecialCatalogRuntime(app);
    std::cout<<"PASS complete Plant primary-behavior catalog: all "<<NUM_SEED_TYPES<<" SeedType values, four schedules each\n";
}

void ZombieBehaviorCatalogRuntime(TestApp& app)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        Board* board=app.mBoard;
        PlayerInfo* player=app.mPlayerInfo;
        ZenGarden* garden=app.mZenGarden;
        GameMode mode=app.mGameMode;
        GameScenes scene=app.mGameScene;
        Sexy::UnsignedTimeCounter appCounter=app.mAppCounter;
        unsigned long long interval=Sexy::gUpdateIntervalNs, phase=Sexy::gLegacyTickPhaseNs;
        int whole=Sexy::gWholeLegacyTicks;
        double ticks=Sexy::gUpdateTicks, steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        ~Restore() {
            app.mBoard=board;app.mPlayerInfo=player;app.mZenGarden=garden;
            app.mGameMode=mode;app.mGameScene=scene;
            app.mAppCounter=appCounter;
            Sexy::gUpdateIntervalNs=interval;Sexy::gLegacyTickPhaseNs=phase;Sexy::gWholeLegacyTicks=whole;
            Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
        }
    } restore{app};
    PlayerInfo profile;
    ZenGarden garden;
    profile.mLevel=31;profile.mFinishedAdventure=1;
    app.mPlayerInfo=&profile;app.mZenGarden=&garden;
    app.mGameMode=GAMEMODE_ADVENTURE;app.mGameScene=SCENE_PLAYING;
    for(const auto type:{ZOMBIE_GARGANTUAR,ZOMBIE_REDEYE_GARGANTUAR})
    for(const int hz:{100,30,240,0})
    for(const int stun:{0,1,2})
    {
        clocks.mVisualTimeNs=clocks.mBattleTimeNs=0;
        Sexy::gLegacyTickPhaseNs=0;Sexy::SRand(0x1CE3);
        Board board(&app);board.mLevel=31;board.mCurrentWave=6;board.mNumWaves=20;
        board.mSeedBank->mNumPackets=0;board.mBackground=BACKGROUND_1_DAY;
        for(int row=0;row<MAX_GRID_SIZE_Y;++row) board.mPlantRow[row]=row==5?PLANTROW_DIRT:PLANTROW_NORMAL;
        auto* giant=board.AddZombieInRow(type,2,0);
        giant->mX=700;giant->mRootBindingGeneration=0;
        giant->mBodyHealth=giant->mBodyMaxHealth/2-100;
        giant->mZombiePhase=PHASE_GARGANTUAR_THROWING;
        giant->PlayZombieReanim(0,ReanimTrackId::anim_throw,REANIM_PLAY_ONCE_AND_HOLD,0,24.f);
        auto* animation=app.ReanimationGet(giant->mBodyReanimID);
        bool armed=false,applied=false,retried=false,finishedFirst=false;
        int imps=0;
        constexpr std::array<long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
        for(int index=0;board.mLevelTimeNs<15'000'000'000ULL;++index)
        {
            const bool pending=animation->ShouldTriggerTimedEvent(.74f);
            if(!armed && pending) {
                Require(giant->mHasObject,"ICE3 event was committed before the plant could interrupt it");
                armed=true;
                if(stun==2) {giant->ApplyButter(board.mLevelTimeNs);applied=true;} // Projectile stage after the last actor update.
            }
            const auto step=static_cast<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[index%jitter.size()]);
            Sexy::ApplySimulationStep(step);board.mLevelTimeNs+=step;
            clocks.mVisualTimeNs+=step;clocks.mBattleTimeNs=board.mLevelTimeNs;
            app.mEffectSystem->UpdateAt(clocks.mVisualTimeNs,clocks.mBattleTimeNs);
            if(pending && armed && !applied && stun==1) {
                giant->HitIceTrap(board.mLevelTimeNs,clocks.mVisualTimeNs,board.mLevelTimeNs); // Plant stage before actors.
                applied=true;
            }
            board.UpdateGameObjects(step);
            if(armed && giant->mZombiePhase==PHASE_ZOMBIE_NORMAL) finishedFirst=true;
            if(finishedFirst && giant->mZombiePhase==PHASE_GARGANTUAR_THROWING) retried=true;
            imps=0;Zombie* other=nullptr;while(board.IterateZombies(other)) imps+=other->mZombieType==ZOMBIE_IMP;
            if(imps) break;
            app.mEffectSystem->ProcessDeleteQueue();
        }
        std::cout<<"ICE3 result type="<<int(type)<<" Hz="<<hz<<" stun="<<stun<<" armed="<<armed<<" retried="<<retried<<" imps="<<imps<<'\n';
        Require(armed&&imps==1&&!giant->mHasObject,"Gargantuar did not eventually release exactly one imp");
        Require(stun?retried:!retried,"ICE3 did not skip and retry the throw, or the unstunned control retried");
        std::cout<<"PASS ICE3 type="<<int(type)<<" Hz="<<hz<<" stun="<<stun<<" retried="<<retried<<" imps="<<imps<<'\n';
    }
    for(const auto type:{ZOMBIE_GARGANTUAR,ZOMBIE_REDEYE_GARGANTUAR})
    for(const int hz:{100,30,240,0})
    for(const int stun:{0,1,2})
    {
        clocks.mVisualTimeNs=clocks.mBattleTimeNs=0;
        Sexy::gLegacyTickPhaseNs=0;Sexy::SRand(0x1CE4);
        Board board(&app);board.mLevel=31;board.mCurrentWave=6;board.mNumWaves=20;
        board.mSeedBank->mNumPackets=0;board.mBackground=BACKGROUND_1_DAY;
        for(int row=0;row<MAX_GRID_SIZE_Y;++row) board.mPlantRow[row]=row==5?PLANTROW_DIRT:PLANTROW_NORMAL;
        auto* target=board.NewPlant(5,2,SEED_WALLNUT,SEED_NONE);
        auto* giant=board.AddZombieInRow(type,2,0);
        giant->mX=float(target->mX);giant->mRootBindingGeneration=0;
        Require(giant->FindPlantTarget(ATTACKTYPE_CHEW)==target,"ICE4 fixture wallnut is outside the real smash target rectangle");
        giant->mZombiePhase=PHASE_GARGANTUAR_SMASHING;
        giant->PlayZombieReanim(0,ReanimTrackId::anim_smash,REANIM_PLAY_ONCE_AND_HOLD,0,16.f);
        auto* animation=app.ReanimationGet(giant->mBodyReanimID);
        bool armed=false,applied=false,retried=false,finishedFirst=false;
        constexpr std::array<long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
        for(int index=0;board.mLevelTimeNs<20'000'000'000ULL;++index)
        {
            const bool pending=animation->ShouldTriggerTimedEvent(.64f);
            if(!armed && pending) {
                Require(!target->mDead && !target->mSquished,"ICE4 smash was committed before the plant could interrupt it");
                armed=true;
                if(stun==2) {giant->ApplyButter(board.mLevelTimeNs);applied=true;}
            }
            const auto step=static_cast<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[index%jitter.size()]);
            Sexy::ApplySimulationStep(step);board.mLevelTimeNs+=step;
            clocks.mVisualTimeNs+=step;clocks.mBattleTimeNs=board.mLevelTimeNs;
            app.mEffectSystem->UpdateAt(clocks.mVisualTimeNs,clocks.mBattleTimeNs);
            if(pending && armed && !applied && stun==1) {
                giant->HitIceTrap(board.mLevelTimeNs,clocks.mVisualTimeNs,board.mLevelTimeNs);
                applied=true;
            }
            board.UpdateGameObjects(step);
            if(armed && !finishedFirst && giant->mZombiePhase==PHASE_ZOMBIE_NORMAL) {
                Require(stun && !target->mDead && !target->mSquished,"ICE4 first smash must finish without hitting the wallnut");
                finishedFirst=true;
            }
            if(finishedFirst && giant->mZombiePhase==PHASE_GARGANTUAR_SMASHING) retried=true;
            if(target->mSquished || target->mDead) break; // Squish retains its flattened sprite for five seconds before Die.
            app.mEffectSystem->ProcessDeleteQueue();
        }
        std::cout<<"ICE4 result type="<<int(type)<<" Hz="<<hz<<" stun="<<stun<<" armed="<<armed
            <<" finishedFirst="<<finishedFirst<<" retried="<<retried<<" squished="<<target->mSquished<<'\n';
        Require(armed && target->mSquished,"Gargantuar did not eventually squish its real ICE4 target");
        Require(stun?(applied&&finishedFirst&&retried):(!finishedFirst&&!retried),
            "ICE4 merely delayed the same smash instead of skipping its event and starting a second smash");
        std::cout<<"PASS ICE4 type="<<int(type)<<" Hz="<<hz<<" stun="<<stun<<" retried="<<retried<<'\n';
    }
    if(gIce3Only) return;
    for(const int hz:{100,30,240,0})
    {
        Sexy::SRand(0x6C715);Sexy::gLegacyTickPhaseNs=0;
        app.mGameScene=SCENE_PLAYING;
        clocks.mVisualTimeNs=clocks.mBattleTimeNs=0;
        Board board(&app);
        board.mLevel=31;board.mCurrentWave=6;board.mNumWaves=20;board.mTotalSpawnedWaves=0;
        board.mSeedBank->mNumPackets=0;board.mBackground=BACKGROUND_1_DAY;
        for(int row=0;row<MAX_GRID_SIZE_Y;++row)
        {
            board.mPlantRow[row]=row==5?PLANTROW_DIRT:PLANTROW_NORMAL;
            board.mIceMinX[row]=1000;board.mIceTimer[row]=0;board.mIceParticleID[row]=PARTICLESYSTEMID_NULL;
        }
        auto* plant=board.NewPlant(3,1,SEED_WALLNUT,SEED_NONE);
        plant->mPlantHealth=1000000;
        auto* zombie=board.AddZombieInRow(ZOMBIE_CATAPULT,1,0);
        zombie->mX=660;zombie->mRootBindingGeneration=0;
        int launches=0;
        std::vector<unsigned int> shots;
        constexpr std::array<long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
        for(int stepIndex=0;board.mLevelTimeNs<80'000'000'000ULL && launches<20;++stepIndex)
        {
            const auto step=std::min<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[stepIndex%jitter.size()],80'000'000'000ULL-board.mLevelTimeNs);
            const bool launching=zombie->mZombiePhase==PHASE_CATAPULT_LAUNCHING;
            Sexy::ApplySimulationStep(step);
            board.mLevelTimeNs+=step;clocks.mVisualTimeNs+=step;clocks.mBattleTimeNs=board.mLevelTimeNs;
            app.mEffectSystem->UpdateAt(clocks.mVisualTimeNs,clocks.mBattleTimeNs);
            board.UpdateGameObjects(step);
            Projectile* bullet=nullptr;
            while(board.IterateProjectiles(bullet))
                if(bullet->mProjectileType==PROJECTILE_BASKETBALL)
                {
                    const auto id=board.mProjectiles.DataArrayGetID(bullet);
                    if(std::find(shots.begin(),shots.end(),id)==shots.end()) shots.push_back(id);
                }
            if(launching && zombie->mZombiePhase!=PHASE_CATAPULT_LAUNCHING)
            {
                ++launches;
                std::cout<<"Catapult Hz="<<hz<<" launch="<<launches<<" ammoNs="<<zombie->mSummonCounter.mNanoseconds
                    <<" shots="<<shots.size()<<'\n';
                Require(zombie->mSummonCounter.mNanoseconds==TimeCounter::Encode(20-launches),
                    "Catapult ammunition was decremented by elapsed time instead of one launch");
            }
            app.mEffectSystem->ProcessDeleteQueue();
        }
        std::cout<<"Catapult end Hz="<<hz<<" launches="<<launches<<" shots="<<shots.size()
            <<" ammoNs="<<zombie->mSummonCounter.mNanoseconds<<" x="<<zombie->mX<<" scene="<<app.mGameScene<<'\n';
        Require(app.mGameScene==SCENE_PLAYING && zombie->mX>500,
            "Catapult resumed its walking segment by catching up the stationary shooting time");
        Require(launches==20 && shots.size()==20 && zombie->mSummonCounter.mNanoseconds==0,
            "Catapult did not fire exactly twenty basketballs before exhausting ammunition");
    }
    std::cout<<"PASS actual Catapult ammunition: twenty shots and one unit per completed launch at four schedules\n";
    for(const auto type:{ZOMBIE_CATAPULT,ZOMBIE_SNORKEL,ZOMBIE_DOLPHIN_RIDER,ZOMBIE_NORMAL,ZOMBIE_DUCKY_TUBE})
    for(const int hz:{100,30,240,0})
    {
        Sexy::SRand(0x207B);Sexy::gLegacyTickPhaseNs=0;
        clocks.mVisualTimeNs=clocks.mBattleTimeNs=0;app.mGameScene=SCENE_PLAYING;
        Board board(&app);
        board.mLevel=21;board.mCurrentWave=6;board.mNumWaves=20;board.mTotalSpawnedWaves=0;
        board.mSeedBank->mNumPackets=0;board.mBackground=BACKGROUND_3_POOL;
        for(int row=0;row<MAX_GRID_SIZE_Y;++row)
        {
            board.mPlantRow[row]=(row==2||row==3)?PLANTROW_POOL:PLANTROW_NORMAL;
            board.mIceMinX[row]=1000;board.mIceTimer[row]=0;board.mIceParticleID[row]=PARTICLESYSTEMID_NULL;
        }
        const bool poolMover=type==ZOMBIE_SNORKEL||type==ZOMBIE_DOLPHIN_RIDER||type==ZOMBIE_DUCKY_TUBE;
        const int row=poolMover?2:1;
        auto* zombie=board.AddZombieInRow(type,row,0);
        zombie->mX=660;zombie->mRootBindingGeneration=0;
        if(type==ZOMBIE_SNORKEL) zombie->mZombiePhase=PHASE_SNORKEL_WALKING_IN_POOL;
        if(type==ZOMBIE_DOLPHIN_RIDER) zombie->mZombiePhase=PHASE_DOLPHIN_RIDING;
        if(poolMover) {zombie->mInPool=true;zombie->mAltitude=-40;}
        if(type==ZOMBIE_DUCKY_TUBE) {zombie->mZombieHeight=HEIGHT_IN_TO_POOL;zombie->mAltitude=-5;}
        bool applied=false,resumed=false;
        float frozenX=0,frozenAltitude=0,maxResumeJump=0,maxAltitudeJump=0;
        constexpr std::array<long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
        for(int n=0;board.mLevelTimeNs<5'000'000'000ULL;++n)
        {
            const auto step=std::min<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[n%jitter.size()],5'000'000'000ULL-board.mLevelTimeNs);
            if(!applied && board.mLevelTimeNs>=(type==ZOMBIE_DUCKY_TUBE?50'000'000ULL:500'000'000ULL))
            {
                zombie->ApplyButter(board.mLevelTimeNs);
                Require(zombie->mButteredEndTimeNs>static_cast<std::int64_t>(board.mLevelTimeNs),"Linear mover rejected its legal butter fixture");
                applied=true;frozenX=zombie->mX;frozenAltitude=zombie->mAltitude;
            }
            if(type==ZOMBIE_NORMAL&&!applied) zombie->mPendingAdditionalSpeedX=-1.0f;
            const auto before=zombie->mX;
            const auto beforeAltitude=zombie->mAltitude;
            Sexy::ApplySimulationStep(step);
            board.mLevelTimeNs+=step;clocks.mVisualTimeNs+=step;clocks.mBattleTimeNs=board.mLevelTimeNs;
            app.mEffectSystem->UpdateAt(clocks.mVisualTimeNs,clocks.mBattleTimeNs);
            board.UpdateGameObjects(step);
            if(applied)
            {
                if(zombie->IsImmobilizied(board.mLevelTimeNs))
                {
                    Require(zombie->mX==frozenX,"Buttered linear zombie moved while immobilized");
                    Require(zombie->mAltitude==frozenAltitude,"Buttered zombie changed altitude while immobilized");
                }
                else
                {
                    resumed=true;maxResumeJump=std::max(maxResumeJump,std::abs(zombie->mX-before));
                    maxAltitudeJump=std::max(maxAltitudeJump,std::abs(zombie->mAltitude-beforeAltitude));
                }
            }
            app.mEffectSystem->ProcessDeleteQueue();
        }
        std::cout<<"Zombie butter type="<<int(type)<<" Hz="<<hz<<" maxResumeJump="<<maxResumeJump
            <<" finalX="<<zombie->mX<<'\n';
        Require(resumed && maxResumeJump<10 && zombie->mX<frozenX,
            "Linear zombie caught up the immobilized interval when butter expired");
        Require(maxAltitudeJump<4,"Zombie altitude caught up the immobilized interval on resume");
    }
    std::cout<<"PASS actual zombie butter: three linear movers, root plus offset, and pool entry hold/resume at four schedules\n";
    enum class Action { Chew, Paper, Vault, Snorkel, Dolphin, Zamboni, Sled, Jack, Balloon,
        Digger, Pogo, Yeti, Bungee, Ladder, Giant, Dancer, Pea, Gatling, Jalapeno, Squash };
    struct Entry {ZombieType type;Action action;};
    static constexpr Entry entries[]{
        {ZOMBIE_NORMAL,Action::Chew},{ZOMBIE_FLAG,Action::Chew},{ZOMBIE_TRAFFIC_CONE,Action::Chew},
        {ZOMBIE_POLEVAULTER,Action::Vault},{ZOMBIE_PAIL,Action::Chew},{ZOMBIE_NEWSPAPER,Action::Paper},
        {ZOMBIE_DOOR,Action::Chew},{ZOMBIE_FOOTBALL,Action::Chew},{ZOMBIE_DANCER,Action::Dancer},
        {ZOMBIE_BACKUP_DANCER,Action::Chew},{ZOMBIE_DUCKY_TUBE,Action::Chew},{ZOMBIE_SNORKEL,Action::Snorkel},
        {ZOMBIE_ZAMBONI,Action::Zamboni},{ZOMBIE_BOBSLED,Action::Sled},{ZOMBIE_DOLPHIN_RIDER,Action::Dolphin},
        {ZOMBIE_JACK_IN_THE_BOX,Action::Jack},{ZOMBIE_BALLOON,Action::Balloon},{ZOMBIE_DIGGER,Action::Digger},
        {ZOMBIE_POGO,Action::Pogo},{ZOMBIE_YETI,Action::Yeti},{ZOMBIE_BUNGEE,Action::Bungee},
        {ZOMBIE_LADDER,Action::Ladder},{ZOMBIE_GARGANTUAR,Action::Giant},{ZOMBIE_IMP,Action::Chew},
        {ZOMBIE_PEA_HEAD,Action::Pea},{ZOMBIE_WALLNUT_HEAD,Action::Chew},{ZOMBIE_JALAPENO_HEAD,Action::Jalapeno},
        {ZOMBIE_GATLING_HEAD,Action::Gatling},{ZOMBIE_SQUASH_HEAD,Action::Squash},{ZOMBIE_TALLNUT_HEAD,Action::Chew},
        {ZOMBIE_REDEYE_GARGANTUAR,Action::Giant},{ZOMBIE_ZOMBATAR,Action::Chew}
    };
    static_assert(std::size(entries)+2==NUM_ZOMBIE_TYPES);
    std::array<bool,NUM_ZOMBIE_TYPES> covered{};
    covered[ZOMBIE_CATAPULT]=covered[ZOMBIE_BOSS]=true;
    for(const auto& entry:entries)
    {
        Require(!covered[entry.type],"Zombie primary catalog contains duplicate types");
        covered[entry.type]=true;
    }
    Require(std::all_of(covered.begin(),covered.end(),[](bool value){return value;}),"Zombie primary catalog misses a type");
    bool allPassed=true;
    for(const auto [type,action]:entries)
    for(const int hz:{100,30,240,0})
    {
        Sexy::SRand(0x58B10+int(type));Sexy::gLegacyTickPhaseNs=0;
        app.mAppCounter=0;
        clocks.mVisualTimeNs=clocks.mBattleTimeNs=0;app.mGameScene=SCENE_PLAYING;
        Board board(&app);
        board.mLevel=31;board.mCurrentWave=6;board.mNumWaves=20;board.mTotalSpawnedWaves=0;
        board.mSeedBank->mNumPackets=0;board.mBackground=BACKGROUND_1_DAY;
        const bool pool=action==Action::Snorkel||action==Action::Dolphin;
        if(pool) board.mBackground=BACKGROUND_3_POOL;
        for(int row=0;row<MAX_GRID_SIZE_Y;++row)
        {
            board.mPlantRow[row]=pool&&(row==2||row==3)?PLANTROW_POOL:row==5?PLANTROW_DIRT:PLANTROW_NORMAL;
            board.mIceMinX[row]=1000;board.mIceTimer[row]=0;board.mIceParticleID[row]=PARTICLESYSTEMID_NULL;
        }
        const int row=pool?2:1;
        auto* plant=board.NewPlant(action==Action::Giant?5:3,row,action==Action::Ladder?SEED_TALLNUT:SEED_WALLNUT,SEED_NONE);
        plant->mPlantHealth=1000000;
        if(action==Action::Sled) board.mIceMinX[row]=0;
        auto* zombie=board.AddZombieInRow(type,row,0);
        Require(zombie!=nullptr&&!zombie->mDead,"Zombie catalog initialization failed");
        if(action!=Action::Bungee)
        {
            zombie->mX=pool?725.0f:action==Action::Dancer?720.0f:action==Action::Yeti?780.0f:
                action==Action::Digger?200.0f:action==Action::Sled?660.0f:action==Action::Giant?600.0f:440.0f;
            zombie->mRootBindingGeneration=0;
        }
        if(action==Action::Dancer)
        {
            plant->Die(0,0,0);
            // Give the leader a full middle-row cross for its four followers.
            zombie->SetRow(2);zombie->mY=zombie->GetPosYBasedOnRow(2);
        }
        const float initialX=zombie->mX;
        float minimumX=initialX;
        bool ate=false,acted=false,completed=false,damaged=false;
        bool modified=false,seenAir=false,seenPool=false;
        bool mounted=false,checkedRideStep=false;
        float mountDelta=0,firstRideDelta=0;
        int maxFollowers=0;
        std::vector<unsigned int> shots;
        constexpr std::array<long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
        const unsigned long long limit=action==Action::Digger?35'000'000'000ULL:25'000'000'000ULL;
        for(int n=0;board.mLevelTimeNs<limit&&!completed;++n)
        {
            const auto step=std::min<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[n%jitter.size()],limit-board.mLevelTimeNs);
            if(action==Action::Paper&&ate&&!modified)
            {
                zombie->TakeShieldDamage(board.mLevelTimeNs,clocks.mVisualTimeNs,board.mLevelTimeNs,zombie->mShieldHealth,0);
                modified=true;
            }
            if(action==Action::Giant&&plant->mDead&&!modified)
            {
                zombie->TakeDamage(board.mLevelTimeNs,clocks.mVisualTimeNs,board.mLevelTimeNs,zombie->mBodyMaxHealth/2+1,0);
                modified=true;
            }
            if(action==Action::Sled&&board.mLevelTimeNs>=7'000'000'000ULL) board.mIceMinX[row]=1000;
            const auto oldPhase=zombie->mZombiePhase;
            const auto oldX=zombie->mX;
            Sexy::ApplySimulationStep(step);
            board.mLevelTimeNs+=step;clocks.mVisualTimeNs+=step;clocks.mBattleTimeNs=board.mLevelTimeNs;
            app.mEffectSystem->UpdateAt(clocks.mVisualTimeNs,clocks.mBattleTimeNs);
            board.UpdateGameObjects(step);
            if(action==Action::Dolphin)
            {
                if(oldPhase==PHASE_DOLPHIN_INTO_POOL&&zombie->mZombiePhase==PHASE_DOLPHIN_RIDING)
                {
                    mounted=true;mountDelta=zombie->mX-oldX;
                }
                else if(mounted&&!checkedRideStep&&oldPhase==PHASE_DOLPHIN_RIDING)
                {
                    checkedRideStep=true;firstRideDelta=zombie->mX-oldX;
                }
            }
            // The production outer update advances this application clock;
            // legacy dancer phases read it independently of Board time.
            app.mAppCounter++;
            minimumX=std::min(minimumX,zombie->mX);
            ate|=zombie->mIsEating;damaged|=plant->mPlantHealth<1000000;
            seenAir|=zombie->mAltitude>5;seenPool|=zombie->mInPool;
            Projectile* bullet=nullptr;
            while(board.IterateProjectiles(bullet))
                if(bullet->mProjectileType==PROJECTILE_ZOMBIE_PEA)
                {
                    const auto id=board.mProjectiles.DataArrayGetID(bullet);
                    if(std::find(shots.begin(),shots.end(),id)==shots.end()) shots.push_back(id);
                }
            int followers=0;Zombie* other=nullptr;
            while(board.IterateZombies(other))
                if(other!=zombie&&(other->mZombieType==ZOMBIE_BACKUP_DANCER||other->mZombieType==ZOMBIE_BOBSLED||other->mZombieType==ZOMBIE_IMP)) ++followers;
            maxFollowers=std::max(maxFollowers,followers);
            switch(action)
            {
            case Action::Chew: completed=ate&&damaged;break;
            case Action::Paper: completed=ate&&damaged&&zombie->mZombiePhase==PHASE_NEWSPAPER_MAD;break;
            case Action::Vault: acted|=zombie->mZombiePhase==PHASE_POLEVAULTER_IN_VAULT;
                completed=acted&&zombie->mZombiePhase==PHASE_POLEVAULTER_POST_VAULT&&!zombie->mHasObject&&zombie->mX>100;break;
            case Action::Snorkel: completed=seenPool&&ate&&damaged&&zombie->mZombiePhase==PHASE_SNORKEL_EATING_IN_POOL;break;
            case Action::Dolphin: acted|=zombie->mZombiePhase==PHASE_DOLPHIN_IN_JUMP;
                completed=seenPool&&acted&&zombie->mZombiePhase==PHASE_DOLPHIN_WALKING_IN_POOL;break;
            case Action::Zamboni: completed=plant->mDead&&board.mIceMinX[row]<1000;break;
            case Action::Sled: acted|=zombie->mZombiePhase==PHASE_BOBSLED_CRASHING;
                completed=acted&&maxFollowers==3&&zombie->mZombiePhase==PHASE_ZOMBIE_NORMAL;break;
            case Action::Jack: acted|=zombie->mZombiePhase==PHASE_JACK_IN_THE_BOX_POPPING;completed=acted&&zombie->mDead;break;
            case Action::Balloon: completed=zombie->mZombiePhase==PHASE_BALLOON_FLYING&&zombie->mX<plant->mX-100&&!damaged;break;
            case Action::Digger: acted|=zombie->mZombiePhase==PHASE_DIGGER_RISING;
                completed=acted&&zombie->mZombiePhase==PHASE_DIGGER_WALKING&&zombie->mX>30;break;
            case Action::Pogo: completed=seenAir&&zombie->mX<plant->mX-100&&zombie->mHasObject&&!damaged;break;
            case Action::Yeti: completed=zombie->mZombiePhase==PHASE_YETI_RUNNING&&zombie->mX>minimumX+20;break;
            case Action::Bungee: acted|=zombie->mZombiePhase==PHASE_BUNGEE_GRABBING;completed=acted&&plant->mDead&&zombie->mDead;break;
            case Action::Ladder: completed=board.GetLadderAt(plant->mPlantCol,row)!=nullptr&&zombie->mShieldHealth==0;break;
            case Action::Giant: acted|=zombie->mZombiePhase==PHASE_GARGANTUAR_SMASHING;
                completed=acted&&plant->mDead&&modified&&maxFollowers==1&&!zombie->mHasObject;break;
            case Action::Dancer: completed=maxFollowers==4&&zombie->mZombiePhase==PHASE_DANCER_DANCING_LEFT;break;
            case Action::Pea: completed=shots.size()==1;break;
            case Action::Gatling: completed=shots.size()==4;break;
            case Action::Jalapeno: completed=plant->mDead&&zombie->mDead;break;
            case Action::Squash: completed=plant->mSquished&&zombie->mZombiePhase==PHASE_SQUASH_DONE_FALLING;break;
            }
            app.mEffectSystem->ProcessDeleteQueue();
            if(zombie->mDead&&!completed) break;
        }
        const bool mountPassed=action!=Action::Dolphin||
            (mounted&&checkedRideStep&&mountDelta<-65&&mountDelta>-80&&firstRideDelta<=0&&firstRideDelta>-10);
        if(action==Action::Dolphin)
            std::cout<<"Dolphin mount Hz="<<hz<<" delta="<<mountDelta<<" nextDelta="<<firstRideDelta<<" ok="<<mountPassed<<'\n';
        const bool passed=completed&&mountPassed&&app.mGameScene==SCENE_PLAYING;
        std::cout<<"Zombie primary type="<<int(type)<<" Hz="<<hz<<" ok="<<passed
            <<" phase="<<int(zombie->mZombiePhase)<<" x="<<zombie->mX<<" timeNs="<<board.mLevelTimeNs
            <<" dead="<<zombie->mDead<<" plantDead="<<plant->mDead<<" ate="<<ate<<" shots="<<shots.size()
            <<" followers="<<maxFollowers<<'\n';
        allPassed&=passed;
    }
    if(allPassed) std::cout<<"PASS actual Zombie primary catalog: 32 types plus twenty-shot Catapult, four schedules each\n";
    for(const int hz:{100,30,240,0})
    {
        Sexy::SRand(0xB055);Sexy::gLegacyTickPhaseNs=0;
        clocks.mVisualTimeNs=clocks.mBattleTimeNs=0;app.mGameScene=SCENE_PLAYING;
        Board board(&app);
        board.mLevel=50;board.mCurrentWave=6;board.mNumWaves=20;board.mTotalSpawnedWaves=0;
        board.mSeedBank->mNumPackets=0;board.mBackground=BACKGROUND_5_ROOF;
        for(int row=0;row<MAX_GRID_SIZE_Y;++row)
        {
            board.mPlantRow[row]=row==5?PLANTROW_DIRT:PLANTROW_NORMAL;
            board.mIceMinX[row]=1000;board.mIceTimer[row]=0;board.mIceParticleID[row]=PARTICLESYSTEMID_NULL;
            if(row<5)
                for(int col=0;col<9;++col)
                {
                    auto* target=board.NewPlant(col,row,SEED_WALLNUT,SEED_NONE);
                    target->mPlantHealth=1000000;
                }
        }
        auto* boss=board.AddZombieInRow(ZOMBIE_BOSS,0,0);
        bool spawned=false,ball=false,rv=false,bungee=false,stomp=false;
        int stage=0;
        constexpr std::array<long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
        for(int n=0;board.mLevelTimeNs<100'000'000'000ULL&&stage<5;++n)
        {
            if(boss->mZombiePhase==PHASE_BOSS_IDLE)
            {
                if(stage==0&&spawned) {boss->BossHeadAttack(board.mLevelTimeNs);stage=1;}
                else if(stage==1&&ball) {boss->BossRVAttack(board.mLevelTimeNs);stage=2;}
                else if(stage==2&&rv) {boss->BossBungeeAttack(board.mLevelTimeNs);stage=3;}
                else if(stage==3&&bungee) {boss->BossStompAttack(board.mLevelTimeNs);stage=4;}
                else if(stage==4&&stomp) {stage=5;break;}
            }
            const auto step=hz?1'000'000'000ULL/hz:static_cast<unsigned long long>(jitter[n%jitter.size()]);
            Sexy::ApplySimulationStep(step);
            board.mLevelTimeNs+=step;clocks.mVisualTimeNs+=step;clocks.mBattleTimeNs=board.mLevelTimeNs;
            app.mEffectSystem->UpdateAt(clocks.mVisualTimeNs,clocks.mBattleTimeNs);
            board.UpdateGameObjects(step);
            Zombie* other=nullptr;
            while(board.IterateZombies(other))
            {
                if(other->mZombieType==ZOMBIE_NORMAL) spawned=true;
                if(other->mZombieType==ZOMBIE_BUNGEE) bungee=true;
            }
            ball|=boss->mBossFireBallReanimID!=REANIMATIONID_NULL;
            if(stage==2||stage==4)
            {
                Plant* target=nullptr;
                while(board.IteratePlants(target))
                    if(target->mSquished) {if(stage==2) rv=true;else stomp=true;}
            }
            app.mEffectSystem->ProcessDeleteQueue();
        }
        std::cout<<"Zombie Boss Hz="<<hz<<" stage="<<stage<<" spawn="<<spawned<<" ball="<<ball
            <<" rv="<<rv<<" bungee="<<bungee<<" stomp="<<stomp<<" phase="<<int(boss->mZombiePhase)<<'\n';
        Require(stage==5&&app.mGameScene==SCENE_PLAYING,"Boss entry/spawn/head/RV/bungee/stomp actions did not complete");
    }
    std::cout<<"PASS actual Boss primary actions: entry, spawn, head ball, RV, bungee and stomp at four schedules\n";
    Require(allPassed,"Zombie primary behavior catalog has failed cases; inspect per-type results");
}

void MinigameRulesRuntime(TestApp& app)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        Board* board=app.mBoard;
        PlayerInfo* player=app.mPlayerInfo;
        ZenGarden* garden=app.mZenGarden;
        PoolEffect* pool=app.mPoolEffect;
        GameMode mode=app.mGameMode;
        GameScenes scene=app.mGameScene;
        BoardResult result=app.mBoardResult;
        Sexy::UnsignedTimeCounter counter=app.mAppCounter;
        unsigned long long interval=Sexy::gUpdateIntervalNs,phase=Sexy::gLegacyTickPhaseNs;
        int whole=Sexy::gWholeLegacyTicks;
        double ticks=Sexy::gUpdateTicks,steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        int mouseX=app.mWidgetManager->mLastMouseX,mouseY=app.mWidgetManager->mLastMouseY;
        ~Restore() {
            app.mBoard=board;app.mPlayerInfo=player;app.mZenGarden=garden;app.mPoolEffect=pool;
            app.mGameMode=mode;app.mGameScene=scene;app.mBoardResult=result;app.mAppCounter=counter;
            app.mWidgetManager->mLastMouseX=mouseX;app.mWidgetManager->mLastMouseY=mouseY;
            Sexy::gUpdateIntervalNs=interval;Sexy::gLegacyTickPhaseNs=phase;Sexy::gWholeLegacyTicks=whole;
            Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
        }
    } restore{app};
    PlayerInfo profile;
    ZenGarden garden;
    profile.mId=42;profile.mLevel=50;profile.mFinishedAdventure=1;
    app.mPlayerInfo=&profile;app.mZenGarden=&garden;
    static constexpr BackgroundType backgrounds[]{
        BACKGROUND_2_NIGHT,BACKGROUND_2_NIGHT,BACKGROUND_3_POOL,BACKGROUND_5_ROOF,BACKGROUND_6_BOSS,
        BACKGROUND_4_FOG,BACKGROUND_3_POOL,BACKGROUND_3_POOL,BACKGROUND_5_ROOF,BACKGROUND_2_NIGHT,
        BACKGROUND_4_FOG,BACKGROUND_1_DAY,BACKGROUND_1_DAY,BACKGROUND_3_POOL,BACKGROUND_1_DAY,
        BACKGROUND_1_DAY,BACKGROUND_1_DAY,BACKGROUND_3_POOL,BACKGROUND_2_NIGHT,BACKGROUND_ZOMBIQUARIUM
    };
    bool allPassed=true;
    for(int mode=GAMEMODE_MINIGAME_BEGHOULED;mode<=GAMEMODE_MINIGAME_ZOMBIQUARIUM;++mode)
    for(const int hz:{100,30,240,0})
    {
        std::cout<<"Minigame begin mode="<<mode<<" Hz="<<hz<<'\n';
        Sexy::SRand(0x81F00+mode);Sexy::gLegacyTickPhaseNs=0;
        clocks.mVisualTimeNs=clocks.mBattleTimeNs=0;app.mAppCounter=0;
        app.mGameMode=static_cast<GameMode>(mode);app.mGameScene=SCENE_PLAYING;app.mBoardResult=BOARDRESULT_NONE;
        PoolEffect pool;pool.PoolEffectInitialize();app.mPoolEffect=&pool;
        Board board(&app);
        try
        {
        board.InitLevel();
        Require(board.mBackground==backgrounds[mode-1],"Minigame selected the wrong background");
        const int initialSun=mode==GAMEMODE_MINIGAME_BEGHOULED||mode==GAMEMODE_MINIGAME_BEGHOULED_TWIST||
            mode==GAMEMODE_MINIGAME_WHACK_A_ZOMBIE?0:mode==GAMEMODE_MINIGAME_LAST_STAND?5000:50;
        Require(board.mSunMoney==initialSun,"Minigame starting sun differs from its rule");
        board.StartLevel();
        auto& challenge=*board.mChallenge;
        int sequence=0;
        constexpr std::array<long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
        const auto update=[&](unsigned long long step) {
            Sexy::ApplySimulationStep(step);clocks.mVisualTimeNs+=step;
            board.Update();app.mAppCounter++;
            board.ProcessDeleteQueue();app.mEffectSystem->ProcessDeleteQueue();
        };
        const auto advance=[&](unsigned long long duration) {
            unsigned long long elapsed=0;
            while(elapsed<duration)
            {
                const auto step=std::min<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()],duration-elapsed);
                update(step);
                elapsed+=step;
            }
        };
        const auto plantFromBank=[&](SeedType seed,int col,int row) {
            board.mSeedBank->mSeedPackets[0].SetPacketType(seed,SEED_NONE);
            board.mCursorObject->mCursorType=CURSOR_TYPE_PLANT_FROM_BANK;
            board.mCursorObject->mType=seed;board.mCursorObject->mImitaterType=SEED_NONE;
            board.mCursorObject->mSeedBankIndex=0;
            board.MouseDownWithPlant(board.GridToPixelX(col,row)+20,board.GridToPixelY(col,row)+20,1);
        };
        switch(app.mGameMode)
        {
        case GAMEMODE_MINIGAME_BEGHOULED:
        case GAMEMODE_MINIGAME_BEGHOULED_TWIST:
        {
            board.mZombieCountDown=100000;
            advance(2'000'000'000);
            Require(board.mPlants.mSize==40,"Beghouled initial board is not a full 8 by 5 grid");
            BeghouledBoardState state;
            challenge.LoadBeghouledBoardState(&state);
            Require(!challenge.BeghouledBoardHasMatch(&state),"Beghouled started with an unsolicited match");
            bool moved=false;
            for(int row=0;row<5&&!moved;++row)
            for(int col=0;col<8&&!moved;++col)
            {
                if(mode==GAMEMODE_MINIGAME_BEGHOULED_TWIST)
                {
                    if(col>=7||row>=4||!challenge.BeghouledTwistMoveCausesMatch(col,row,&state)) continue;
                    HitResult hit;hit.mObject=nullptr;hit.mObjectType=OBJECT_TYPE_NONE;
                    challenge.MouseDown(board.GridToPixelX(col,row)+60,board.GridToPixelY(col,row)+60,1,&hit);
                    moved=true;
                }
                else
                {
                    for(const auto offset:{Point{1,0},Point{0,1}})
                    {
                        const int toCol=col+offset.mX,toRow=row+offset.mY;
                        if(toCol>=8||toRow>=5||!challenge.BeghouledIsValidMove(col,row,toCol,toRow,&state)) continue;
                        HitResult hit;hit.mObject=board.GetTopPlantAt(col,row,TOPPLANT_ONLY_NORMAL_POSITION);hit.mObjectType=OBJECT_TYPE_PLANT;
                        challenge.MouseDown(board.GridToPixelX(col,row)+40,board.GridToPixelY(col,row)+40,1,&hit);
                        challenge.MouseMove(board.GridToPixelX(toCol,toRow)+40,board.GridToPixelY(toCol,toRow)+40);
                        challenge.MouseUp(0,0);moved=true;break;
                    }
                }
            }
            Require(moved,"Beghouled fixture has no playable move");
            advance(4'000'000'000);
            Require(challenge.mChallengeScore>0&&board.mPlants.mSize==40,"Beghouled input failed to score/refill its grid");
            challenge.mChallengeScore=BEGHOULED_WINNING_SCORE-1;
            challenge.BeghouledScore(3,2,3,true);
            Require(board.HasLevelAwardDropped()&&challenge.mChallengeScore==BEGHOULED_WINNING_SCORE,"Beghouled match threshold did not award the level");
            break;
        }
        case GAMEMODE_MINIGAME_BOBSLED_BONANZA:
            for(int row=0;row<6;++row)
                Require((board.mIceMinX[row]==400)==(board.mPlantRow[row]!=PLANTROW_POOL),"Bobsled mode did not initialize its four land ice lanes");
            board.mZombieCountDown=1;advance(100'000'000);
            Require(board.mZombies.mSize>0,"Bobsled mode failed to spawn its first wave");
            break;
        case GAMEMODE_MINIGAME_COLUMN:
            Require(board.mCurrentWave==9&&board.mSeedBank->GetNumSeedsOnConveyorBelt()==6,"Column mode lost its initial wave or conveyor");
            plantFromBank(SEED_FLOWERPOT,3,2);
            Require(board.CountPlantByType(SEED_FLOWERPOT)==5,"Column planting failed to populate five roof rows");
            plantFromBank(SEED_MELONPULT,3,2);
            Require(board.CountPlantByType(SEED_MELONPULT)==5,"Column plant propagation did not use the pots in every row");
            break;
        case GAMEMODE_MINIGAME_FINAL_BOSS:
            Require(board.mSeedBank->GetNumSeedsOnConveyorBelt()==4&&board.HasConveyorBeltSeedBank(),"Boss mode lost its initial conveyor cards");
            challenge.PlayBossEnter();advance(1'000'000'000);
            Require(board.GetBossZombie()!=nullptr,"Boss mode entry did not create the boss");
            break;
        case GAMEMODE_MINIGAME_INVISIGHOUL:
        {
            auto* zombie=board.AddZombieInRow(ZOMBIE_NORMAL,1,0);
            zombie->mX=300;zombie->mRootBindingGeneration=0;
            auto& render=*app.mDDInterface->mRenderInterface;
            const auto draw=[&]() {
                auto* image=render.CreateRenderImage(800,600);
                {GpuImageDrawScope destination(render,image);Graphics graphics(image);zombie->Draw(0,&graphics);}
                render.Flush();auto pixels=render.ReadImagePixels(*image).pixels;
                delete image;return pixels;
            };
            const auto hidden=draw();
            Require(std::none_of(hidden.begin(),hidden.end(),[](unsigned pixel){return pixel!=0;}),"Invisible zombie rendered a body");
            app.mGameMode=GAMEMODE_ADVENTURE;
            const auto visible=draw();
            Require(std::any_of(visible.begin(),visible.end(),[](unsigned pixel){return pixel!=0;}),"Invisible-zombie reference failed to draw the same body");
            app.mGameMode=GAMEMODE_MINIGAME_INVISIGHOUL;
            break;
        }
        case GAMEMODE_MINIGAME_LAST_STAND:
            board.mZombieCountDown=0;advance(500'000'000);
            Require(board.mZombies.mSize==0&&challenge.mChallengeState==STATECHALLENGE_NORMAL,"Last Stand spawned zombies during preparation");
            Require(!board.mStoreButton->mBtnNoDraw,"Last Stand did not expose Start Onslaught");
            app.mWidgetManager->mLastMouseX=board.mStoreButton->mX+20;
            app.mWidgetManager->mLastMouseY=board.mStoreButton->mY+20;
            board.MouseUp(app.mWidgetManager->mLastMouseX,app.mWidgetManager->mLastMouseY,1);
            advance(500'000'000);
            Require(challenge.mChallengeState==STATECHALLENGE_LAST_STAND_ONSLAUGHT&&board.mZombies.mSize>0,"Last Stand button failed to start waves");
            break;
        case GAMEMODE_MINIGAME_LITTLE_TROUBLE:
        {
            auto* zombie=board.AddZombieInRow(ZOMBIE_NORMAL,1,0);
            Require(zombie->mScaleZombie==0.5f,"Little Trouble zombie did not use half scale");
            Require(board.HasConveyorBeltSeedBank(),"Little Trouble lost its conveyor");
            break;
        }
        case GAMEMODE_MINIGAME_POGO_PARTY:
        case GAMEMODE_MINIGAME_WAR_AND_PEAS:
        case GAMEMODE_MINIGAME_WAR_AND_PEAS_2:
        {
            board.mZombieCountDown=1;advance(100'000'000);
            Require(board.mZombies.mSize>0,"Special roster mode failed to spawn a wave");
            Zombie* zombie=nullptr;
            while(board.IterateZombies(zombie))
            {
                const auto type=zombie->mZombieType;
                Require(mode==GAMEMODE_MINIGAME_POGO_PARTY?type==ZOMBIE_POGO:
                    type==ZOMBIE_PEA_HEAD||type==ZOMBIE_WALLNUT_HEAD||
                    (mode==GAMEMODE_MINIGAME_WAR_AND_PEAS_2&&type>=ZOMBIE_JALAPENO_HEAD&&type<=ZOMBIE_TALLNUT_HEAD),
                    "Special roster spawned an unrelated zombie type");
            }
            break;
        }
        case GAMEMODE_MINIGAME_PORTAL_COMBAT:
        {
            advance(500'000'000);
            auto* portal=challenge.GetPortalAt(2,0);
            Require(portal&&challenge.GetOtherPortal(portal),"Portal mode lost its paired square portals");
            auto* exit=challenge.GetOtherPortal(portal);
            for(const auto type:{ZOMBIE_NORMAL,ZOMBIE_BALLOON,ZOMBIE_CATAPULT})
            {
                auto* zombie=board.AddZombieInRow(type,0,0);
                zombie->mX=200;zombie->mRootBindingGeneration=0;
                advance(100'000'000);
                const auto rect=zombie->GetZombieRect();
                zombie->mX+=(portal->mGridX*80+25)-(rect.mX+rect.mWidth/2);
                zombie->mRootBindingGeneration=0;
                // Bind movement at the entrance before the real teleport.
                zombie->mVelocityX.bound=false;
                update(hz?1'000'000'000ULL/hz:1'234'567ULL);
                challenge.UpdatePortal(portal);
                Require(zombie->mRow==exit->mGridY&&zombie->mLastPortalX==exit->mGridX,"Portal did not move the zombie to its paired exit");
                const auto arrivalX=zombie->mX;
                Require(arrivalX>600&&arrivalX<760,"Zombie did not remain at the portal exit at the end of its teleport update");
                advance(100'000'000);
                std::cout<<"Portal zombie type="<<int(type)<<" Hz="<<hz<<" arrivalX="<<arrivalX
                    <<" nextX="<<zombie->mX<<" row="<<zombie->mRow<<'\n';
                Require(std::abs(zombie->mX-arrivalX)<15,"Zombie sampled its old movement origin after teleporting");
                zombie->DieNoLoot(board.mLevelTimeNs,clocks.mVisualTimeNs,board.mLevelTimeNs);
            }
            auto* projectile=board.AddProjectile(portal->mGridX*80+55,180,RENDER_LAYER_PROJECTILE,0,PROJECTILE_PEA);
            const auto projectileRect=projectile->GetProjectileRect();
            projectile->mX+=(portal->mGridX*80+55)-(projectileRect.mX+projectileRect.mWidth/2);
            projectile->mPosX=projectile->mX;
            const float oldY=projectile->mY,oldShadow=projectile->mShadowY;
            challenge.UpdatePortal(portal);
            Require(projectile->mRow==exit->mGridY&&projectile->mLastPortalX==exit->mGridX&&
                projectile->mX==projectile->mPosX&&projectile->mY==projectile->mPosY&&
                projectile->mY==oldY+100&&projectile->mShadowY==oldShadow+100,
                "Portal failed to move projectile coordinates, shadow and row together");
            auto* mower=board.mLawnMowers.DataArrayAlloc();
            mower->LawnMowerInitialize(0);mower->StartMower();
            mower->mPosX=portal->mGridX*80+25-45;
            challenge.UpdatePortal(portal);
            Require(mower->mRow==exit->mGridY&&mower->mLastPortalX==exit->mGridX&&
                mower->mPosY==board.GetPosYBasedOnRow(mower->mPosX+40,mower->mRow)+23,
                "Portal failed to settle the mower on its destination row");
            break;
        }
        case GAMEMODE_MINIGAME_RAINING_SEEDS:
        {
            board.mZombieCountDown=100000;advance(1'500'000'000);
            bool found=false;Coin* coin=nullptr;
            while(board.IterateCoins(coin)) if(coin->mType==COIN_USABLE_SEED_PACKET)
            {
                found=true;
                Require(coin->mUsableSeedType!=SEED_SUNFLOWER&&coin->mUsableSeedType!=SEED_SUNSHROOM&&
                    coin->mUsableSeedType!=SEED_UMBRELLA&&!Plant::IsUpgrade(coin->mUsableSeedType),"Raining Seeds produced an excluded card");
            }
            Require(found,"Raining Seeds failed to drop a usable card");
            break;
        }
        case GAMEMODE_MINIGAME_SEEING_STARS:
        {
            int stars=0;
            for(int row=0;row<6;++row) for(int col=0;col<9;++col)
                if(challenge.GetArtChallengeSeed(col,row)==SEED_STARFRUIT)
                {
                    Require(challenge.CanPlantAt(col,row,SEED_PEASHOOTER)==PLANTING_NOT_ON_ART,"Seeing Stars allowed the wrong plant on a target");
                    board.AddPlant(col,row,SEED_STARFRUIT,SEED_NONE);++stars;
                }
            Require(stars>0&&board.HasLevelAwardDropped(),"Seeing Stars failed to award its completed pattern");
            break;
        }
        case GAMEMODE_MINIGAME_SLOT_MACHINE:
        {
            board.mZombieCountDown=100000;board.mSunCountDown=100000;
            HitResult hit;hit.mObjectType=OBJECT_TYPE_SLOT_MACHINE_HANDLE;hit.mObject=nullptr;
            challenge.MouseDown(0,0,1,&hit);
            Require(board.mSunMoney==25&&challenge.mSlotMachineRollCount==1&&challenge.mChallengeState==STATECHALLENGE_SLOT_MACHINE_ROLLING,
                "Slot-machine handle did not charge 25 sun and start one roll");
            advance(8'000'000'000);
            Require(challenge.mChallengeState==STATECHALLENGE_NORMAL,"Slot machine failed to finish its roll");
            for(int packet=0;packet<3;++packet)
                Require(board.mSeedBank->mSeedPackets[packet].mSlotMachineEndTimeNs==0,"Slot reel did not stop");
            board.mSunMoney=2000;challenge.UpdateSlotMachine();
            Require(board.HasLevelAwardDropped(),"Slot-machine sun threshold did not award the level");
            break;
        }
        case GAMEMODE_MINIGAME_SPEED:
        {
            board.mZombieCountDown=100000;advance(500'000'000);
            Require(board.mLevelTimeNs==1'000'000'000ULL,"Speed mode did not advance twice per outer update");
            break;
        }
        case GAMEMODE_MINIGAME_WALLNUT_BOWLING:
        case GAMEMODE_MINIGAME_WALLNUT_BOWLING_2:
        {
            Require(challenge.CanPlantAt(3,1,SEED_WALLNUT)==PLANTING_NOT_PASSED_LINE&&
                challenge.CanPlantAt(2,1,SEED_WALLNUT)==PLANTING_OK,"Bowling placement line was not enforced");
            plantFromBank(SEED_WALLNUT,2,1);
            Plant* nut=nullptr;Plant* candidate=nullptr;
            while(board.IteratePlants(candidate)) if(candidate->mSeedType==SEED_WALLNUT) nut=candidate;
            Require(nut&&nut->mSeedType==SEED_WALLNUT,"Bowling placement did not create a nut");
            const float x=nut->mX;advance(500'000'000);
            Require(nut->mX>x+50,"Bowling mode nut did not roll forwards");
            break;
        }
        case GAMEMODE_MINIGAME_WHACK_A_ZOMBIE:
        {
            Require(board.mCursorObject->mCursorType==CURSOR_TYPE_HAMMER,"Whack mode did not select the hammer");
            board.mSunMoney=25;board.mSeedBank->mY=-1;board.mTutorialState=TUTORIAL_OFF;
            challenge.Update(0);
            Require(board.mSeedBank->mY==0 && board.mTutorialState==TUTORIAL_WHACK_A_ZOMBIE_BEFORE_PICK_SEED,
                "Unified Whack update lost its same-update tutorial trigger or final HUD clamp");
            board.mSeedBank->mY=-1;board.mPaused=true;
            challenge.Update(0);
            Require(board.mSeedBank->mY==-1,"Paused challenge dispatched its seed-bank slide");
            board.mPaused=false;board.mSeedBank->mY=0;
            advance(2'500'000'000);
            Zombie* zombie=nullptr;
            Require(board.IterateZombies(zombie),"Whack mode did not spawn its first zombie");
            const auto id=board.ZombieGetID(zombie);
            const auto rect=zombie->GetZombieRect();
            HitResult hit;hit.mObjectType=OBJECT_TYPE_NONE;hit.mObject=nullptr;
            challenge.MouseDown(rect.mX+rect.mWidth/2,rect.mY+rect.mHeight/2+20,1,&hit);
            Require(!board.ZombieTryToGet(id)||zombie->mDead||zombie->IsDeadOrDying(),"Hammer hit did not defeat an unarmored zombie");
            break;
        }
        case GAMEMODE_MINIGAME_ZOMBIQUARIUM:
        {
            Require(board.mZombies.mSize==2,"Aquarium did not start with two fish");
            HitResult hit;hit.mObjectType=OBJECT_TYPE_NONE;hit.mObject=nullptr;
            challenge.MouseDown(400,220,1,&hit);
            Require(board.mSunMoney==45,"Aquarium feed did not charge five sun");
            board.mSunMoney=150;
            challenge.ZombiquariumPacketClicked(&board.mSeedBank->mSeedPackets[0]);
            Require(board.mZombies.mSize==3&&board.mSunMoney==50,"Aquarium did not sell one fish for 100 sun");
            GridItem* brain=nullptr;
            while(board.IterateGridItems(brain)) if(brain->mGridItemType==GRIDITEM_BRAIN) break;
            Require(brain!=nullptr,"Aquarium feeding did not create a brain");
            const auto brainID=board.mGridItems.DataArrayGetID(brain);
            Zombie* fish=nullptr;
            Require(board.IterateZombies(fish),"Aquarium lost its fish");
            const auto hungryID=board.ZombieGetID(fish);
            fish->mBodyHealth=150;fish->mX=350;fish->mY=180;fish->mRootBindingGeneration=0;
            advance(149'999'999);
            Require(board.mGridItems.DataArrayTryToGet(brainID)&&fish->mBodyHealth<=150,
                "Aquarium brain was edible before its 150ms age threshold");
            advance(1);
            Require(!board.mGridItems.DataArrayTryToGet(brainID)&&fish->mBodyHealth>150,
                "Aquarium brain born at time zero was not eaten at 150ms");
            advance(350'000'000);
            challenge.MouseDown(400,220,1,&hit);
            brain=nullptr;while(board.IterateGridItems(brain)) if(brain->mGridItemType==GRIDITEM_BRAIN) break;
            Require(brain!=nullptr,"Aquarium failed to drop a later brain");
            const auto laterID=board.mGridItems.DataArrayGetID(brain);
            fish=board.ZombieGet(hungryID);fish->mBodyHealth=150;fish->mX=350;fish->mY=180;
            fish->mZombiePhase=PHASE_ZOMBIQUARIUM_DRIFT;fish->mPhaseCounter=100;
            advance(149'999'999);
            Require(board.mGridItems.DataArrayTryToGet(laterID),"Nonzero brain birth time was mistaken for its age");
            advance(1);
            Require(!board.mGridItems.DataArrayTryToGet(laterID)&&fish->mBodyHealth>150,
                "Later aquarium brain failed its 150ms age threshold");
            challenge.MouseDown(200,220,1,&hit);
            brain=nullptr;while(board.IterateGridItems(brain)) if(brain->mGridItemType==GRIDITEM_BRAIN) break;
            Require(brain!=nullptr,"Aquarium failed to create a pursuit target");
            const auto pursuitID=board.mGridItems.DataArrayGetID(brain);
            fish->mBodyHealth=150;fish->mX=600;fish->mY=180;
            fish->mZombiePhase=PHASE_ZOMBIQUARIUM_DRIFT;fish->mPhaseCounter=100;
            // Keep the companions non-hungry so the distant pursuit target
            // cannot be consumed by a different fish before this one arrives.
            Zombie* companion=nullptr;
            while(board.IterateZombies(companion)) if(companion!=fish)
                companion->mBodyHealth=companion->mBodyMaxHealth=1000;
            for(int slice=0;slice<24&&board.mGridItems.DataArrayTryToGet(pursuitID);++slice) advance(500'000'000);
            fish=board.ZombieTryToGet(hungryID);
            std::cout<<"Aquarium pursuit Hz="<<hz<<" brain="<<bool(board.mGridItems.DataArrayTryToGet(pursuitID))
                <<" fishHealth="<<(fish?fish->mBodyHealth:0)<<" x="<<(fish?fish->mX:0)<<'\n';
            Require(!board.mGridItems.DataArrayTryToGet(pursuitID)&&fish&&fish->mBodyHealth>150,
                "Hungry aquarium fish failed to pursue/eat a falling brain and recover health");
            board.mSunMoney=999;
            challenge.ZombiquariumPacketClicked(&board.mSeedBank->mSeedPackets[1]);
            Require(!board.HasLevelAwardDropped()&&board.mSunMoney==999,"Aquarium sold its trophy below the 1000-sun price");
            board.mSunMoney=1000;
            challenge.ZombiquariumPacketClicked(&board.mSeedBank->mSeedPackets[1]);
            Require(board.HasLevelAwardDropped()&&board.mSunMoney==0,"Aquarium trophy purchase failed to charge 1000 sun and win");
            break;
        }
        default:Require(false,"Missing minigame rules case");
        }
        std::cout<<"Minigame primary mode="<<mode<<" Hz="<<hz<<" wave="<<board.mCurrentWave
            <<" sun="<<board.mSunMoney<<" plants="<<board.mPlants.mSize<<" zombies="<<board.mZombies.mSize
            <<" score="<<challenge.mChallengeScore<<" award="<<board.HasLevelAwardDropped()<<'\n';
        Require(app.mGameScene==SCENE_PLAYING,"Minigame unexpectedly ended in the loss scene");
        board.mPaused=true;
        const auto battle=board.mLevelTimeNs;
        advance(100'000'000);
        Require(board.mLevelTimeNs==battle,"Paused minigame continued advancing Board time");
        }
        catch(const std::exception& error)
        {
            allPassed=false;
            std::cerr<<"Minigame failed mode="<<mode<<" Hz="<<hz<<" timeNs="<<board.mLevelTimeNs
                <<" wave="<<board.mCurrentWave<<" message="<<error.what()<<'\n';
        }
    }
    Require(allPassed,"Minigame rules catalog has failed cases; inspect per-mode results");
    std::cout<<"PASS actual twenty minigame rules: initial state, primary actions and paused Board clock at four schedules\n";
}

void PuzzleRulesRuntime(TestApp& app)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        Board* board=app.mBoard;
        PlayerInfo* player=app.mPlayerInfo;
        ZenGarden* garden=app.mZenGarden;
        GameMode mode=app.mGameMode;
        GameScenes scene=app.mGameScene;
        BoardResult result=app.mBoardResult;
        Sexy::UnsignedTimeCounter counter=app.mAppCounter;
        unsigned long long interval=Sexy::gUpdateIntervalNs,phase=Sexy::gLegacyTickPhaseNs;
        int whole=Sexy::gWholeLegacyTicks;
        double ticks=Sexy::gUpdateTicks,steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        ~Restore() {
            app.mBoard=board;app.mPlayerInfo=player;app.mZenGarden=garden;
            app.mGameMode=mode;app.mGameScene=scene;app.mBoardResult=result;app.mAppCounter=counter;
            Sexy::gUpdateIntervalNs=interval;Sexy::gLegacyTickPhaseNs=phase;Sexy::gWholeLegacyTicks=whole;
            Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
        }
    } restore{app};
    PlayerInfo profile;ZenGarden garden;
    profile.mId=42;profile.mLevel=50;profile.mFinishedAdventure=1;
    app.mPlayerInfo=&profile;app.mZenGarden=&garden;
    bool allPassed=true;
    for(int mode=GAMEMODE_PUZZLE_I_ZOMBIE_1;mode<=GAMEMODE_SCARY_POTTER_ENDLESS;++mode)
    for(const int hz:{100,30,240,0})
    {
        std::cout<<"Puzzle begin mode="<<mode<<" Hz="<<hz<<'\n';
        Sexy::SRand(0xB68A0+mode);Sexy::gLegacyTickPhaseNs=0;
        clocks.mVisualTimeNs=clocks.mBattleTimeNs=0;app.mAppCounter=0;
        app.mGameMode=static_cast<GameMode>(mode);app.mGameScene=SCENE_PLAYING;app.mBoardResult=BOARDRESULT_NONE;
        Board board(&app);
        try
        {
            board.InitLevel();board.StartLevel();
            auto& challenge=*board.mChallenge;
            int sequence=0;
            constexpr std::array<long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
            const auto update=[&](unsigned long long step) {
                Sexy::ApplySimulationStep(step);clocks.mVisualTimeNs+=step;
                board.Update();app.mAppCounter++;
                board.ProcessDeleteQueue();app.mEffectSystem->ProcessDeleteQueue();
            };
            const auto advance=[&](unsigned long long duration) {
                unsigned long long elapsed=0;
                while(elapsed<duration)
                {
                    const auto step=std::min<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()],duration-elapsed);
                    update(step);elapsed+=step;
                }
            };
            if(app.IsIZombieLevel())
            {
                static constexpr int packets[]{3,3,3,3,4,4,4,6,8,9};
                const int index=mode-GAMEMODE_PUZZLE_I_ZOMBIE_1;
                const int limit=index<5?4:index==8?6:5;
                Require(board.mSunMoney==150&&board.mSeedBank->mNumPackets==packets[index],"I-Zombie initial sun or card count differs from its rule");
                Require(board.mPlants.mSize>0,"I-Zombie did not populate its defenders");
                for(int row=0;row<5;++row)
                {
                    auto* brain=board.GetGridItemAt(GRIDITEM_IZOMBIE_BRAIN,0,row);
                    Require(brain&&brain->mBrainHealth==70,"I-Zombie must initialize five 70-bite brains");
                }
                Plant* defender=nullptr;
                while(board.IteratePlants(defender))
                {
                    Require(defender->mPlantCol<limit&&defender->mRow<5,"I-Zombie defender is outside its permitted half");
                    auto* body=app.ReanimationTryToGet(defender->mBodyReanimID);
                    Require(!body||body->mAnimRate==0,"I-Zombie cardboard defender retained an idle animation rate");
                    if(defender->mSeedType==SEED_POTATOMINE) Require(defender->mState==STATE_POTATO_ARMED,"I-Zombie potato mine was not armed");
                }
                Require(challenge.CanPlantAt(limit-1,2,SEED_ZOMBIE_NORMAL)==PLANTING_NOT_HERE&&
                    challenge.CanPlantAt(limit,2,SEED_ZOMBIE_NORMAL)==PLANTING_OK&&
                    challenge.CanPlantAt(limit-1,2,SEED_ZOMBIE_BUNGEE)==PLANTING_OK&&
                    challenge.CanPlantAt(limit,2,SEED_ZOMBIE_BUNGEE)==PLANTING_NOT_HERE,
                    "I-Zombie normal/bungee placement sides disagree with the line");
                board.mSunMoney=0;
                board.AddCoin(400,200,COIN_SUN,COIN_MOTION_COIN);
                board.AddCoin(430,200,COIN_SUN,COIN_MOTION_COIN);
                challenge.IZombieUpdate();
                Require(app.mBoardResult!=BOARDRESULT_LOST&&!app.GetDialog(DIALOG_GAME_OVER),"I-Zombie ignored uncollected sun when checking a loss");
                Coin* coin=nullptr;while(board.IterateCoins(coin)) coin->Die();board.ProcessDeleteQueue();
                board.mSunMoney=150;
                const auto seed=board.mSeedBank->mSeedPackets[0].mPacketType;
                Require(seed==SEED_ZOMBIE_NORMAL||seed==SEED_ZOMBIE_IMP,"I-Zombie initial affordable card changed unexpectedly");
                const auto select=[&]() {
                    board.mCursorObject->mCursorType=CURSOR_TYPE_PLANT_FROM_BANK;
                    board.mCursorObject->mType=seed;board.mCursorObject->mImitaterType=SEED_NONE;
                    board.mCursorObject->mSeedBankIndex=0;
                };
                select();
                board.MouseDownWithPlant(board.GridToPixelX(limit-1,2)+20,board.GridToPixelY(limit-1,2)+20,1);
                Require(board.mZombies.mSize==0&&board.mSunMoney==150,"I-Zombie charged/spawned on the forbidden side");
                select();
                board.MouseDownWithPlant(board.GridToPixelX(limit,2)+20,board.GridToPixelY(limit,2)+20,1);
                Require(board.mZombies.mSize==1&&board.mSunMoney==100,"I-Zombie did not charge 50 sun for its first affordable card");
                Zombie* eater=nullptr;Require(board.IterateZombies(eater),"I-Zombie placement produced no zombie");
                Require(eater->mZombieType==Challenge::IZombieSeedTypeToZombieType(seed)&&eater->mRow==2,"I-Zombie card mapped to the wrong type or row");
                const int originalHealth=eater->mBodyHealth,originalMaxHealth=eater->mBodyMaxHealth;
                Plant* shooter=nullptr;Plant* mine=nullptr;defender=nullptr;
                while(board.IteratePlants(defender))
                {
                    if(defender->mSeedType==SEED_PEASHOOTER||defender->mSeedType==SEED_SNOWPEA||
                        defender->mSeedType==SEED_FUMESHROOM||defender->mSeedType==SEED_CACTUS||
                        defender->mSeedType==SEED_PUFFSHROOM||defender->mSeedType==SEED_SPLITPEA) shooter=defender;
                    if(defender->mSeedType==SEED_POTATOMINE) mine=defender;
                }
                Require(shooter||mine,"I-Zombie fixture has no ranged or mine defender to exercise");
                auto* attack=shooter?shooter:mine;
                defender=nullptr;while(board.IteratePlants(defender)) if(defender!=attack) defender->Die(0,0,0);
                board.ProcessDeleteQueue();
                eater->SetRow(attack->mRow);eater->mX=attack->mX+(shooter?140:10);
                eater->mY=eater->GetPosYBasedOnRow(eater->mRow);eater->mRootBindingGeneration=0;
                if(shooter)
                {
                    eater->mBodyHealth=eater->mBodyMaxHealth=1000000;
                    eater->mVelX=0;app.ReanimationGet(eater->mBodyReanimID)->ApplyRate(0);
                    shooter->mNextLaunchTimeNs=300'000'000;shooter->mLaunchWindowStartNs=0;
                    advance(1'000'000'000);
                    Require(eater->mBodyHealth<1000000||board.mProjectiles.mSize>0,
                        "I-Zombie cardboard shooter never attacked its actual target");
                    eater->RemoveColdEffects(board.mLevelTimeNs);
                    eater->mBodyHealth=originalHealth;eater->mBodyMaxHealth=originalMaxHealth;
                }
                else
                {
                    const auto id=board.ZombieGetID(eater);advance(1'000'000'000);
                    Require(!board.ZombieTryToGet(id)||board.ZombieGet(id)->IsDeadOrDying(),"I-Zombie armed cardboard mine failed to defeat its target");
                    select();board.MouseDownWithPlant(board.GridToPixelX(limit,2)+20,board.GridToPixelY(limit,2)+20,1);
                    eater=nullptr;while(board.IterateZombies(eater)) if(!eater->IsDeadOrDying()) break;
                    Require(eater!=nullptr,"I-Zombie could not buy a replacement after its mine encounter");
                }
                std::cout<<"Puzzle defender mode="<<mode<<" Hz="<<hz<<" kind="<<(shooter?"shoot":"mine")<<" observed=1\n";
                // Isolate the 70-bite rule after verifying the authored formation.
                defender=nullptr;while(board.IteratePlants(defender)) defender->Die(0,0,0);
                Projectile* projectile=nullptr;while(board.IterateProjectiles(projectile)) projectile->Die();
                board.ProcessDeleteQueue();
                eater->SetRow(2);eater->mY=eater->GetPosYBasedOnRow(2);
                eater->mX=-70;eater->mRootBindingGeneration=0;eater->mVelocityX.bound=false;
                advance(2'760'000'000);
                auto* brain=board.GetGridItemAt(GRIDITEM_IZOMBIE_BRAIN,0,2);
                std::cout<<"Puzzle bite mode="<<mode<<" Hz="<<hz<<" type="<<int(eater->mZombieType)
                    <<" health="<<(brain?brain->mBrainHealth:0)<<" score="<<challenge.mChallengeScore<<'\n';
                Require(brain&&brain->mBrainHealth==1&&challenge.mChallengeScore==0,"I-Zombie brain was not exactly 69 bites into its consumption");
                advance(40'000'000);
                Require(!board.GetGridItemAt(GRIDITEM_IZOMBIE_BRAIN,0,2)&&challenge.mChallengeScore==1,
                    "The seventieth actual bite did not remove and score the brain");
                for(const int row:{0,1,3,4})
                {
                    eater->SetRow(row);eater->mY=eater->GetPosYBasedOnRow(row);eater->mX=-70;
                    eater->mRootBindingGeneration=0;eater->mVelocityX.bound=false;eater->mVelocityY.bound=false;
                    advance(2'800'000'000);
                }
                Require(challenge.mChallengeScore==5,"I-Zombie failed to score all five actual brains");
                if(app.IsEndlessIZombie(app.mGameMode))
                {
                    Require(board.mNextSurvivalStageCounter>0,"Endless I-Zombie did not schedule the next round");
                    const auto battle=board.mLevelTimeNs;advance(5'100'000'000);
                    Require(challenge.mSurvivalStage==1&&challenge.mChallengeScore==0&&board.mLevelTimeNs==battle+5'100'000'000ULL,
                        "Endless I-Zombie repopulation reset time or failed to start the next round");
                    Require(board.GetGridItemAt(GRIDITEM_IZOMBIE_BRAIN,0,4)!=nullptr,"Endless I-Zombie did not repopulate its brains");
                    PlayerInfo loaded;loaded.mId=42;
                    Require(loaded.LoadDetails()&&loaded.mChallengeRecords[mode]>=1,"Endless I-Zombie failed to persist its completed-round record");
                }
                else Require(board.HasLevelAwardDropped(),"Fixed I-Zombie level did not award five scored brains");
                std::cout<<"Puzzle brains mode="<<mode<<" Hz="<<hz<<" complete=5 nextStage="<<challenge.mSurvivalStage<<'\n';
                // Rebuild a legal exhausted-funds state, without an old award.
                eater=nullptr;while(board.IterateZombies(eater)) eater->DieNoLoot(board.mLevelTimeNs,clocks.mVisualTimeNs,board.mLevelTimeNs);
                coin=nullptr;while(board.IterateCoins(coin)) coin->Die();
                defender=nullptr;while(board.IteratePlants(defender)) defender->Die(board.mLevelTimeNs,clocks.mVisualTimeNs,board.mLevelTimeNs);
                GridItem* item=nullptr;while(board.IterateGridItems(item)) item->GridItemDie();
                board.ProcessDeleteQueue();app.mEffectSystem->ProcessDeleteQueue();
                board.mLevelAwardSpawned=false;board.mLevelComplete=false;board.mBoardFadeOutCounter=-1;board.mNextSurvivalStageCounter=0;
                challenge.IZombieInitLevel();board.mSunMoney=0;app.mBoardResult=BOARDRESULT_NONE;
                challenge.IZombieUpdate();
                Require(app.mBoardResult==BOARDRESULT_LOST&&app.GetDialog(DIALOG_GAME_OVER),"I-Zombie did not end when no zombie or spendable sun remained");
            }
            else
            {
                static constexpr int counts[]{25,25,30,35,35,35,30,35,35,35};
                static constexpr int firstColumns[]{4,3,3,2,2,2,3,2,2,2};
                const int index=mode-GAMEMODE_SCARY_POTTER_1;
                Require(board.StageIsNight()&&board.mSunMoney==0&&board.mSeedBank->mNumPackets==1&&
                    board.mSeedBank->mSeedPackets[0].mPacketType==SEED_CHERRYBOMB,"Vasebreaker background, starting sun or rescue card changed");
                Require(challenge.ScaryPotterCountPots()==counts[index]&&!challenge.ScaryPotterIsCompleted(),"Vasebreaker initial pot count or completion rule is wrong");
                std::vector<unsigned int> seeds,zombies;
                std::array<bool,54> occupied{};
                GridItem* pot=nullptr;int green=0;
                while(board.IterateGridItems(pot)) if(pot->mGridItemType==GRIDITEM_SCARY_POT)
                {
                    Require(pot->mGridX>=firstColumns[index]&&pot->mGridX<=(index==1?7:8)&&pot->mGridY<5,
                        "Vasebreaker placed a pot in an excluded cell");
                    const int cell=pot->mGridY*9+pot->mGridX;
                    Require(!occupied[cell],"Vasebreaker placed two pots on one cell");occupied[cell]=true;
                    if(pot->mGridItemState==GRIDITEM_STATE_SCARY_POT_LEAF)
                    {++green;Require(pot->mScaryPotType==SCARYPOT_SEED,"Green pot did not contain a plant");}
                    const auto id=board.mGridItems.DataArrayGetID(pot);
                    if(pot->mScaryPotType==SCARYPOT_ZOMBIE) zombies.push_back(id);else seeds.push_back(id);
                }
                Require(green==2&&!zombies.empty(),"Vasebreaker lost its two green pots or its zombie payloads");
                seeds.insert(seeds.end(),zombies.begin(),zombies.end());
                ZombieID lastEnemy=ZOMBIEID_NULL;
                bool plantedFromCoin=false;
                for(std::size_t opened=0;opened<seeds.size();++opened)
                {
                    pot=board.mGridItems.DataArrayTryToGet(seeds[opened]);
                    Require(pot!=nullptr,"Vasebreaker pot disappeared before its input");
                    const auto payload=pot->mScaryPotType;
                    const auto seed=pot->mSeedType;const auto zombie=pot->mZombieType;
                    const int sunCount=pot->mSunCount,row=pot->mGridY,col=pot->mGridX;
                    std::vector<unsigned int> oldCoins;
                    Coin* coin=nullptr;while(board.IterateCoins(coin)) oldCoins.push_back(board.mCoins.DataArrayGetID(coin));
                    HitResult hit;hit.mObjectType=OBJECT_TYPE_SCARY_POT;hit.mObject=pot;
                    challenge.MouseDown(board.GridToPixelX(col,row)+40,board.GridToPixelY(col,row)+40,1,&hit);
                    Require(challenge.mChallengeState==STATECHALLENGE_SCARY_POTTER_MALLETING,"Vase click did not start the real hammer animation");
                    for(int frame=0;frame<2*(hz?hz:100)&&board.mGridItems.DataArrayTryToGet(seeds[opened]);++frame)
                        update(hz?1'000'000'000ULL/hz:static_cast<unsigned long long>(jitter[sequence++%jitter.size()]));
                    Require(!board.mGridItems.DataArrayTryToGet(seeds[opened])&&challenge.mChallengeState==STATECHALLENGE_NORMAL,
                        "Vase hammer never released its payload and returned to idle");
                    if(payload==SCARYPOT_ZOMBIE)
                    {
                        Zombie* enemy=nullptr;Require(board.IterateZombies(enemy),"Zombie vase produced no enemy");
                        Require(enemy->mZombieType==zombie&&enemy->mRow==row&&std::abs(enemy->mX-board.GridToPixelX(col,row))<1,
                            "Vase zombie type, row or birth position differs from its contents");
                        if(opened+1==seeds.size()) lastEnemy=board.ZombieGetID(enemy);
                        else {enemy->DieNoLoot(board.mLevelTimeNs,clocks.mVisualTimeNs,board.mLevelTimeNs);board.ProcessDeleteQueue();}
                    }
                    else
                    {
                        int produced=0;Coin* usable=nullptr;coin=nullptr;
                        while(board.IterateCoins(coin)) if(std::find(oldCoins.begin(),oldCoins.end(),board.mCoins.DataArrayGetID(coin))==oldCoins.end())
                        {
                            ++produced;
                            Require(payload==SCARYPOT_SEED?coin->mType==COIN_USABLE_SEED_PACKET&&coin->mUsableSeedType==seed:coin->mType==COIN_SUN,
                                "Vase released a different plant/sun payload");
                            if(payload==SCARYPOT_SEED) usable=coin;
                        }
                        Require(produced==(payload==SCARYPOT_SEED?1:sunCount),"Vase released the wrong number of payload coins");
                        if(usable&&!plantedFromCoin)
                        {
                            usable->MouseDown(0,0,1);
                            Require(board.mCursorObject->mCursorType==CURSOR_TYPE_PLANT_FROM_USABLE_COIN&&board.mCursorObject->mType==seed,
                                "Vase plant card could not be picked up");
                            const int sun=board.mSunMoney;
                            board.MouseDownWithPlant(board.GridToPixelX(0,0)+20,board.GridToPixelY(0,0)+20,1);
                            auto* planted=board.GetTopPlantAt(0,0,TOPPLANT_ONLY_NORMAL_POSITION);
                            Require(planted&&planted->mSeedType==seed&&board.mSunMoney==sun,"Vase plant card could not be planted without charging sun");
                            plantedFromCoin=true;
                        }
                    }
                }
                Require(plantedFromCoin&&challenge.ScaryPotterCountPots()==0&&!challenge.ScaryPotterIsCompleted()&&lastEnemy,
                    "Vasebreaker incorrectly finished with its last enemy still alive");
                board.ZombieGet(lastEnemy)->DieNoLoot(board.mLevelTimeNs,clocks.mVisualTimeNs,board.mLevelTimeNs);
                board.ProcessDeleteQueue();advance(100'000'000);
                Require(challenge.ScaryPotterIsCompleted(),"Vasebreaker did not finish after pots and enemies were gone");
                if(app.IsEndlessScaryPotter(app.mGameMode))
                {
                    Require(board.mNextSurvivalStageCounter>0,"Endless Vasebreaker did not schedule its next round");
                    advance(5'100'000'000);
                    Require(challenge.mSurvivalStage==1&&challenge.ScaryPotterCountPots()==35,"Endless Vasebreaker did not repopulate the next round");
                    PlayerInfo loaded;loaded.mId=42;
                    Require(loaded.LoadDetails()&&loaded.mChallengeRecords[mode]>=1,"Endless Vasebreaker failed to persist its streak");
                }
                else Require(board.HasLevelAwardDropped(),"Fixed Vasebreaker did not award its completed board");
                std::cout<<"Puzzle pots mode="<<mode<<" Hz="<<hz<<" opened="<<seeds.size()<<" nextStage="<<challenge.mSurvivalStage<<'\n';
            }
            app.KillDialog(DIALOG_GAME_OVER);app.ProcessSafeDeleteList();
            std::cout<<"Puzzle primary mode="<<mode<<" Hz="<<hz<<" stage="<<challenge.mSurvivalStage
                <<" award="<<board.HasLevelAwardDropped()<<" result="<<int(app.mBoardResult)<<'\n';
        }
        catch(const std::exception& error)
        {
            allPassed=false;
            std::cerr<<"Puzzle failed mode="<<mode<<" Hz="<<hz<<" timeNs="<<board.mLevelTimeNs
                <<" message="<<error.what()<<'\n';
            app.KillDialog(DIALOG_GAME_OVER);app.ProcessSafeDeleteList();
        }
    }
    // Construct later rounds explicitly: these check authored endless layouts
    // and the reward boundary, not a claim that the fixture solved 80 rounds.
    for(const auto mode:{GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS,GAMEMODE_SCARY_POTTER_ENDLESS})
    for(const int stage:{0,1,2,8,9,10,19,79,80})
    {
        Sexy::SRand(0xE380+int(mode)*83+stage);
        clocks.mVisualTimeNs=clocks.mBattleTimeNs=0;app.mAppCounter=0;
        app.mGameMode=mode;app.mGameScene=SCENE_PLAYING;app.mBoardResult=BOARDRESULT_NONE;
        Board board(&app);
        board.mChallenge->mSurvivalStage=stage;
        board.InitLevel();board.StartLevel();
        auto& challenge=*board.mChallenge;
        const bool expectedAward=(stage+1)%(mode==GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS?3:10)==0;
        Require(bool(challenge.PuzzleIsAwardStage())==expectedAward,"Endless puzzle reward cadence changed");
        int gargantuars=0,puffshrooms=0,sunflowers=0;
        if(mode==GAMEMODE_SCARY_POTTER_ENDLESS)
        {
            Require(challenge.ScaryPotterCountPots()==35,"Later endless Vasebreaker round lost a pot");
            GridItem* pot=nullptr;
            while(board.IterateGridItems(pot)) if(pot->mGridItemType==GRIDITEM_SCARY_POT)
            {
                Require(pot->mGridX>=2&&pot->mGridX<=8&&pot->mGridY>=0&&pot->mGridY<5,
                    "Later endless Vasebreaker round has an illegal cell");
                gargantuars+=pot->mScaryPotType==SCARYPOT_ZOMBIE&&pot->mZombieType==ZOMBIE_GARGANTUAR;
            }
            Require(gargantuars==1+std::min(stage/10,8),"Endless Vasebreaker giant count did not follow its stage cap");
        }
        else
        {
            Plant* plant=nullptr;
            while(board.IteratePlants(plant))
            {
                Require(plant->mPlantCol>=0&&plant->mPlantCol<5&&plant->mRow>=0&&plant->mRow<5,
                    "Later endless I-Zombie defender is outside its half");
                puffshrooms+=plant->mSeedType==SEED_PUFFSHROOM;
                sunflowers+=plant->mSeedType==SEED_SUNFLOWER;
            }
            Require(puffshrooms+sunflowers>=8,"Endless I-Zombie lost its eight baseline sun/puff plants");
            if(stage==0) Require(puffshrooms==0,"First endless I-Zombie round has a puffshroom");
            if(stage==1) Require(puffshrooms==1,"Second endless I-Zombie round must have one puffshroom");
        }
        const int coinsBefore=board.mCoins.mSize;
        challenge.PuzzlePhaseComplete(2,2);
        Require(expectedAward?board.mCoins.mSize==coinsBefore+1&&board.mNextSurvivalStageCounter==0:
            board.mCoins.mSize==coinsBefore&&board.mNextSurvivalStageCounter>0,
            "Endless puzzle completion did not follow its reward/nonreward branch");
        std::cout<<"Puzzle endless mode="<<int(mode)<<" stage="<<stage<<" reward="<<expectedAward
            <<" giants="<<gargantuars<<" puffs="<<puffshrooms<<" sunflowers="<<sunflowers<<'\n';
    }
    Require(allPassed,"Puzzle rules catalog has failed cases; inspect per-mode results");
    std::cout<<"PASS actual twenty puzzle rules: formations, inputs, payloads, brain bites, terminal rules and endless repopulation at four schedules\n";
}

void SurvivalRulesRuntime(TestApp& app)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        Board* board=app.mBoard;
        PlayerInfo* player=app.mPlayerInfo;
        ZenGarden* garden=app.mZenGarden;
        PoolEffect* pool=app.mPoolEffect;
        GameMode mode=app.mGameMode;
        GameScenes scene=app.mGameScene;
        BoardResult result=app.mBoardResult;
        Sexy::UnsignedTimeCounter counter=app.mAppCounter;
        unsigned long long interval=Sexy::gUpdateIntervalNs,phase=Sexy::gLegacyTickPhaseNs;
        int whole=Sexy::gWholeLegacyTicks;
        double ticks=Sexy::gUpdateTicks,steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        ~Restore() {
            app.mBoard=board;app.mPlayerInfo=player;app.mZenGarden=garden;app.mPoolEffect=pool;
            app.mGameMode=mode;app.mGameScene=scene;app.mBoardResult=result;app.mAppCounter=counter;
            Sexy::gUpdateIntervalNs=interval;Sexy::gLegacyTickPhaseNs=phase;Sexy::gWholeLegacyTicks=whole;
            Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
        }
    } restore{app};
    PlayerInfo profile;ZenGarden garden;
    profile.mId=42;profile.mLevel=50;profile.mFinishedAdventure=1;
    app.mPlayerInfo=&profile;app.mZenGarden=&garden;
    Require(app.mSeedChooserScreen==nullptr,"Survival fixture requires no preexisting chooser");
    bool allPassed=true;
    for(int mode=GAMEMODE_SURVIVAL_ENDLESS_STAGE_1;mode<=GAMEMODE_SURVIVAL_NORMAL_STAGE_5;++mode)
    for(const int hz:{100,30,240,0})
    {
        std::cout<<"Survival begin mode="<<mode<<" Hz="<<hz<<'\n';
        Sexy::SRand(0xC62E0+mode);Sexy::gLegacyTickPhaseNs=0;
        clocks.mVisualTimeNs=clocks.mBattleTimeNs=0;app.mAppCounter=0;
        app.mGameMode=static_cast<GameMode>(mode);app.mGameScene=SCENE_PLAYING;app.mBoardResult=BOARDRESULT_NONE;
        PoolEffect pool;pool.PoolEffectInitialize();app.mPoolEffect=&pool;
        Board board(&app);
        try
        {
            board.InitLevel();board.StartLevel();
            auto& challenge=*board.mChallenge;
            const int background=(mode-GAMEMODE_SURVIVAL_ENDLESS_STAGE_1)%5;
            const bool normal=app.IsSurvivalNormal(app.mGameMode),endless=app.IsSurvivalEndless(app.mGameMode);
            Require(int(board.mBackground)==background&&board.mSunMoney==50&&board.mNumWaves==(normal?10:20),
                "Survival initial background, sun or wave count differs from its rule");
            Require(board.IsSurvivalStageWithRepick()&&!board.IsFinalSurvivalStage(),"Survival first round cannot be a final stage");
            // InitLevel precedes normal seed selection. Supply a legal chosen
            // bank before exercising the real repick constructor, which saves
            // each chosen seed's cooldown into its authored seed slot.
            constexpr std::array<SeedType,10> chosen{SEED_SUNFLOWER,SEED_PEASHOOTER,SEED_WALLNUT,SEED_FLOWERPOT,
                SEED_POTATOMINE,SEED_SNOWPEA,SEED_CHERRYBOMB,SEED_REPEATER,SEED_PUFFSHROOM,SEED_SQUASH};
            for(int packet=0;packet<board.mSeedBank->mNumPackets;++packet)
                board.mSeedBank->mSeedPackets[packet].SetPacketType(chosen[packet],SEED_NONE);
            int sequence=0;
            constexpr std::array<long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
            const auto update=[&](unsigned long long step) {
                Sexy::ApplySimulationStep(step);clocks.mVisualTimeNs+=step;
                board.Update();app.mAppCounter++;
                board.ProcessDeleteQueue();app.mEffectSystem->ProcessDeleteQueue();
            };
            const auto advance=[&](unsigned long long duration) {
                unsigned long long elapsed=0;
                while(elapsed<duration) {
                    const auto step=std::min<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()],duration-elapsed);
                    update(step);elapsed+=step;
                }
            };
            const auto removeEnemies=[&]() {
                Zombie* enemy=nullptr;
                while(board.IterateZombies(enemy)) if(!enemy->mDead)
                    enemy->DieNoLoot(board.mLevelTimeNs,clocks.mVisualTimeNs,board.mLevelTimeNs);
                board.ProcessDeleteQueue();app.mEffectSystem->ProcessDeleteQueue();
            };
            const auto validateWaves=[&]() {
                int scheduled=0;
                for(int wave=0;wave<board.mNumWaves;++wave)
                for(int slot=0;slot<MAX_ZOMBIES_IN_WAVE;++slot) {
                    const auto type=board.mZombiesInWave[wave][slot];
                    if(type==ZOMBIE_INVALID) break;
                    ++scheduled;
                    Require(type>=0&&type<NUM_ZOMBIE_TYPES&&(board.mZombieAllowed[type]||
                        (type==ZOMBIE_FLAG&&board.IsFlagWave(wave))),"Survival wave contains an unselected zombie type");
                    Require(board.StageHasPool()||!board.IsZombieTypePoolOnly(type),"Survival wave put a water-only zombie on dry land");
                    Require(!board.StageHasRoof()||(type!=ZOMBIE_DIGGER&&type!=ZOMBIE_DANCER),"Roof survival selected an incompatible zombie");
                    Require(board.GetSurvivalFlagsCompleted()>=10||type!=ZOMBIE_REDEYE_GARGANTUAR,"Survival scheduled red-eye before ten completed flags");
                }
                Require(scheduled>=board.mNumWaves,"Survival has an empty scheduled campaign");
            };
            validateWaves();
            profile.mPurchases[STORE_ITEM_ROOF_CLEANER]=0;
            board.mCutScene->PlaceLawnItems();
            if(board.StageHasRoof()) {
                Require(board.mLawnMowers.mSize==0,"Roof survival supplied unpurchased cleaners");
                profile.mPurchases[STORE_ITEM_ROOF_CLEANER]=1;
                board.InitLawnMowers();
            }
            const int mowerCount=board.mLawnMowers.mSize;
            Require(mowerCount==(board.StageHasPool()?6:5),"Survival did not initialize one mower per row");
            if(board.StageHasRoof()&&!board.GetFlowerPotAt(1,0)) board.AddPlant(1,0,SEED_FLOWERPOT,SEED_NONE);
            auto* retained=board.AddPlant(1,0,SEED_PEASHOOTER,SEED_NONE);
            const auto retainedID=board.mPlants.DataArrayGetID(retained);
            retained->mNextLaunchTimeNs=100'000'000'000LL;
            board.mSunMoney=375;
            auto* retainedBody=app.ReanimationGet(retained->mBodyReanimID);
            board.mZombieCountDown=1;advance(100'000'000);
            Require(board.mCurrentWave==1&&board.mZombies.mSize>0,"Survival did not spawn its first scheduled wave");
            Zombie* enemy=nullptr;
            while(board.IterateZombies(enemy)) Require(enemy->mRow>=0&&enemy->mRow<(board.StageHasPool()?6:5),"Survival spawned a zombie outside its rows");
            removeEnemies();
            // Skip intermediate combat, but use the actual last-wave spawn and
            // death callbacks to enter the five-second repick sequence.
            board.mCurrentWave=board.mNumWaves-1;board.SpawnZombieWave();
            Require(board.mCurrentWave==board.mNumWaves&&board.mZombies.mSize>0,"Survival final wave was not generated");
            auto& cooling=board.mSeedBank->mSeedPackets[0];
            cooling.mRefreshing=true;cooling.mActive=false;
            cooling.mRefreshStartTimeNs=static_cast<std::int64_t>(board.mLevelTimeNs);
            cooling.mRefreshTime=Plant::GetRefreshTime(cooling.mPacketType,cooling.mImitaterType);
            removeEnemies();
            Require(board.mNextSurvivalStageCounter>0&&!board.mLevelComplete,"Survival did not enter its round-end wait");
            const auto waitStart=board.mLevelTimeNs;
            advance(500'000'000);
            Require(board.mLevelTimeNs==waitStart+500'000'000ULL&&!board.mLevelComplete,
                "Survival froze Board time during the round-end banner");
            while(!board.mLevelComplete&&board.mLevelTimeNs-waitStart<6'000'000'000ULL)
                update(hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()]);
            Require(board.mLevelComplete,"Survival round-end wait never completed");
            Require(board.mLevelTimeNs-waitStart>=5'000'000'000ULL&&board.mLevelTimeNs-waitStart<5'040'000'000ULL,
                "Survival round-end banner did not retain its five-second duration");
            const auto beforeRepick=board.mLevelTimeNs;
            const auto cooldownStart=cooling.mRefreshStartTimeNs;
            app.CheckForGameEnd();
            Require(challenge.mSurvivalStage==1&&app.mGameScene==SCENE_LEVEL_INTRO&&app.mSeedChooserScreen&&
                !board.mLevelComplete&&board.mLevelTimeNs==beforeRepick,"Actual survival completion did not create the next chooser on the same Board");
            Require(board.mPlants.DataArrayTryToGet(retainedID)==retained&&board.mSunMoney==375&&board.mLawnMowers.mSize==mowerCount,
                "Survival repick lost retained plants, sun or mowers");
            const auto phase=retainedBody->SamplePlayback().phaseNs;
            const auto deadline=retained->mNextLaunchTimeNs;
            board.mDrawCount=0;
            advance(200'000'000);
            Require(board.mLevelTimeNs==beforeRepick&&retainedBody->SamplePlayback().phaseNs==phase&&retained->mNextLaunchTimeNs==deadline,
                "Survival chooser advanced retained actor time");
            validateWaves();
            auto* chooser=app.mSeedChooserScreen;
            Require(board.mSeedBank->mNumPackets==board.GetNumSeedsInBank(),"Repick bank did not restore its selectable capacity");
            const int capacity=board.mSeedBank->mNumPackets;
            for(int packet=0;packet<capacity;++packet) chooser->ClickedSeedInChooser(chooser->mChosenSeeds[chosen[packet]]);
            // The real application updates the chooser as a separate Widget.
            // Keep Board's scene held while its Visual-driven cards land.
            unsigned long long choosing=0;
            while(choosing<300'000'000ULL) {
                const auto step=std::min<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()],300'000'000ULL-choosing);
                update(step);chooser->Update();choosing+=step;
            }
            Require(chooser->mSeedsInBank==capacity&&chooser->mSeedsInFlight==0,"Survival repick cards did not finish their real flights");
            chooser->CloseSeedChooser();
            Require(cooling.mPacketType==chosen[0]&&cooling.mRefreshing&&cooling.mRefreshStartTimeNs==cooldownStart&&
                board.mLevelTimeNs==beforeRepick,"Survival repick did not preserve the existing cooldown in the held Board epoch");
            app.StartPlaying();app.ProcessSafeDeleteList();
            advance(200'000'000);
            Require(board.mLevelTimeNs==beforeRepick+200'000'000ULL&&retainedBody->SamplePlayback().phaseNs!=phase,
                "Survival retained actor did not resume with Board time");
            board.mPaused=true;const auto pause=board.mLevelTimeNs;advance(200'000'000);board.mPaused=false;
            Require(board.mLevelTimeNs==pause,"Survival pause advanced Board time");
            if(!endless) {
                challenge.mSurvivalStage=3;
                Require(!board.IsFinalSurvivalStage(),"Survival ended before its fifth stage");
                challenge.mSurvivalStage=4;board.InitZombieWaves();
                Require(board.IsFinalSurvivalStage()&&!board.IsSurvivalStageWithRepick(),"Survival did not end at five/ten flags");
                board.mCurrentWave=board.mNumWaves-1;board.SpawnZombieWave();removeEnemies();
                Require(board.HasLevelAwardDropped()&&board.mNextSurvivalStageCounter==0&&app.mBoardResult==BOARDRESULT_WON,
                    "Final survival stage attempted another chooser instead of an award");
            }
            else {
                challenge.mSurvivalStage=5;board.InitZombieWaves();validateWaves();
                Require(!board.IsFinalSurvivalStage()&&board.IsSurvivalStageWithRepick(),"Endless survival acquired a final stage");
            }
            std::cout<<"Survival primary mode="<<mode<<" Hz="<<hz<<" background="<<background
                <<" waves="<<board.mNumWaves<<" retained=1 repick=1 final="<<!endless<<'\n';
        }
        catch(const std::exception& error) {
            allPassed=false;
            std::cerr<<"Survival failed mode="<<mode<<" Hz="<<hz<<" timeNs="<<board.mLevelTimeNs<<" message="<<error.what()<<'\n';
        }
        app.KillSeedChooserScreen();app.ProcessSafeDeleteList();
    }
    Require(allPassed,"Survival rules have failed cases; inspect per-mode output");
    std::cout<<"PASS actual fifteen survival rules: first/final waves, repick callbacks, retained actors, pause and fixed/endless boundaries at four schedules\n";
}

void AdventureRulesRuntime(TestApp& app)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        Board* board=app.mBoard;
        PlayerInfo* player=app.mPlayerInfo;
        ZenGarden* garden=app.mZenGarden;
        PoolEffect* pool=app.mPoolEffect;
        GameMode mode=app.mGameMode;
        GameScenes scene=app.mGameScene;
        BoardResult result=app.mBoardResult;
        Sexy::UnsignedTimeCounter counter=app.mAppCounter;
        unsigned long long interval=Sexy::gUpdateIntervalNs,phase=Sexy::gLegacyTickPhaseNs;
        int whole=Sexy::gWholeLegacyTicks;
        double ticks=Sexy::gUpdateTicks,steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        ~Restore() {
            app.mBoard=board;app.mPlayerInfo=player;app.mZenGarden=garden;app.mPoolEffect=pool;
            app.mGameMode=mode;app.mGameScene=scene;app.mBoardResult=result;app.mAppCounter=counter;
            Sexy::gUpdateIntervalNs=interval;Sexy::gLegacyTickPhaseNs=phase;Sexy::gWholeLegacyTicks=whole;
            Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
        }
    } restore{app};
    PlayerInfo profile;ZenGarden garden;
    profile.mId=43;app.mPlayerInfo=&profile;app.mZenGarden=&garden;
    static constexpr std::array<int,50> firstWaves{
        4,6,8,10,8,10,20,10,20,20,10,20,10,20,10,10,20,10,20,20,
        10,20,20,30,20,20,30,20,30,30,10,20,10,20,20,10,20,10,20,20,
        10,20,20,30,20,20,30,20,30,30};
    bool allPassed=true;
    for(const int replay:{0,1})
    for(int level=1;level<=50;++level)
    for(const int hz:{100,30,240,0})
    {
        std::cout<<"Adventure begin level="<<level<<" replay="<<replay<<" Hz="<<hz<<'\n';
        Sexy::SRand(0xA30C0+level+replay*100);Sexy::gLegacyTickPhaseNs=0;
        profile.mLevel=level;profile.mFinishedAdventure=replay;profile.mPurchases[STORE_ITEM_ROOF_CLEANER]=0;
        clocks.mVisualTimeNs=clocks.mBattleTimeNs=0;app.mAppCounter=0;
        app.mGameMode=GAMEMODE_ADVENTURE;app.mGameScene=SCENE_LEVEL_INTRO;app.mBoardResult=BOARDRESULT_NONE;
        PoolEffect pool;pool.PoolEffectInitialize();app.mPoolEffect=&pool;
        Board board(&app);
        try
        {
            board.InitLevel();
            const int background=level==35?int(BACKGROUND_2_NIGHT):level==50?int(BACKGROUND_6_BOSS):(level-1)/10;
            const bool conveyor=level==5||level==10||level==20||level==25||level==30||level==40||level==45||level==50;
            const bool miniBoss=level==10||level==20||level==30;
            const int expectedWaves=level==15?8:replay&&!miniBoss?(firstWaves[level-1]<10?20:firstWaves[level-1]+10):firstWaves[level-1];
            Require(board.mLevel==level&&int(board.mBackground)==background&&board.mNumWaves==expectedWaves,
                "Adventure level, background or wave count differs from its original rule");
            Require(board.HasConveyorBeltSeedBank()==conveyor&&board.ChooseSeedsOnCurrentLevel()==(!conveyor&&level!=15&&level!=35&&(replay||level>7)),
                "Adventure chose the wrong conveyor/chooser route");
            const int expectedSun=level==15||level==35?0:!replay&&level==1?150:50;
            Require(board.mSunMoney==expectedSun,"Adventure starting sun changed");
            int rows=0;for(int row=0;row<6;++row) rows+=board.mPlantRow[row]!=PLANTROW_DIRT;
            const int expectedRows=!replay&&level==1?1:!replay&&(level==2||level==3)?3:
                background==BACKGROUND_3_POOL||background==BACKGROUND_4_FOG?6:5;
            Require(rows==expectedRows,"Adventure active rows do not match its lawn stage");
            if(level==15) Require(board.GetGraveStonesCount()==9&&board.mSeedBank->mNumPackets==3&&
                board.mSeedBank->mSeedPackets[2].mPacketType==SEED_CHERRYBOMB,"Adventure Whack lost its graves or cherry rescue card");
            if(level==35) Require(board.mChallenge->ScaryPotterCountPots()==15&&board.mSeedBank->mNumPackets==1,
                "Adventure Vasebreaker first board is not its fifteen-vase tutorial layout");
            if(level>=11&&level<=20&&level!=15) Require(board.GetGraveStonesCount()>0,"Night adventure did not initialize its graves");
            // Normal chooser input is covered independently. This catalog
            // supplies legal chosen seeds before entering each combat fixture.
            if(!conveyor&&level!=15&&level!=35)
                for(int packet=0;packet<board.mSeedBank->mNumPackets;++packet)
                    board.mSeedBank->mSeedPackets[packet].SetPacketType(static_cast<SeedType>(packet),SEED_NONE);
            int sequence=0;
            constexpr std::array<long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
            const auto update=[&](unsigned long long step) {
                Sexy::ApplySimulationStep(step);clocks.mVisualTimeNs+=step;
                board.Update();app.mAppCounter++;
                board.ProcessDeleteQueue();app.mEffectSystem->ProcessDeleteQueue();
            };
            const auto advance=[&](unsigned long long duration) {
                unsigned long long elapsed=0;
                while(elapsed<duration) {
                    const auto step=std::min<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()],duration-elapsed);
                    update(step);elapsed+=step;
                }
            };
            board.mDrawCount=0;advance(100'000'000);
            Require(board.mLevelTimeNs==0&&board.mTutorialState==TUTORIAL_OFF,
                "Adventure intro advanced battle time or launched its first tutorial early");
            app.mGameScene=SCENE_PLAYING;
            // The real intro creates Zomboss before the conveyor is live.
            if(level==50) board.mChallenge->PlayBossEnter();
            board.StartLevel();advance(100'000'000);
            Require(board.mLevelTimeNs==100'000'000ULL,"Adventure did not start its Board clock with combat");
            const auto plantFromBank=[&](int packet,int col,int row) {
                auto& card=board.mSeedBank->mSeedPackets[packet];
                const auto seed=card.mPacketType;
                card.MouseDown(0,0,1);
                Require(board.mCursorObject->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&board.mCursorObject->mType==seed,
                    "Adventure tutorial could not pick up its real seed packet");
                const int sun=board.mSunMoney,cost=board.GetCurrentPlantCost(seed,SEED_NONE);
                board.MouseDownWithPlant(board.GridToPixelX(col,row)+20,board.GridToPixelY(col,row)+20,1);
                auto* planted=board.GetTopPlantAt(col,row,TOPPLANT_ONLY_NORMAL_POSITION);
                Require(planted&&planted->mSeedType==seed&&board.mSunMoney==sun-cost,
                    "Adventure tutorial planting or sun deduction failed");
            };
            if(!replay&&level<=2) {
                Require(board.mTutorialState==(level==1?TUTORIAL_LEVEL_1_PICK_UP_PEASHOOTER:TUTORIAL_LEVEL_2_PICK_UP_SUNFLOWER),
                    "First positive Board time did not trigger the correct tutorial once");
                board.mSunMoney=1000; // Isolate teaching/card rules from collecting enough sun.
                const int packet=level==1?0:1;
                const int count=level==1?2:3;
                for(int planted=0;planted<count;++planted) {
                    plantFromBank(packet,planted,2);
                    if(planted+1<count) advance(7'600'000'000);
                }
                Require(board.mTutorialState==(level==1?TUTORIAL_LEVEL_1_COMPLETED:TUTORIAL_LEVEL_2_COMPLETED),
                    "Real planting and cooldown did not complete the initial teaching chain");
                std::cout<<"Adventure tutorial level="<<level<<" Hz="<<hz<<" planted="<<count<<" completed=1\n";
            }
            if(conveyor) {
                board.mChallenge->mConveyorBeltCounter=1;advance(100'000'000);
                Require(board.mSeedBank->mNumPackets>0&&board.mSeedBank->mSeedPackets[0].mPacketType!=SEED_NONE,
                    "Adventure conveyor never produced an actual usable seed");
            }
            if(level==35) {
                for(int stage=0;stage<3;++stage) {
                    Require(board.mChallenge->mSurvivalStage==stage&&board.mChallenge->ScaryPotterCountPots()==15+stage*5&&
                        board.IsFinalScaryPotterStage()==(stage==2),"Adventure Vasebreaker three-board layout/final-stage rule changed");
                    if(stage<2) {board.mChallenge->PuzzleNextStageClear();board.ProcessDeleteQueue();board.mChallenge->ScaryPotterPopulate();}
                }
            }
            else if(level==50) {
                advance(200'000'000);
                Zombie* boss=nullptr;
                while(board.IterateZombies(boss)&&boss->mZombieType!=ZOMBIE_BOSS) {}
                Require(boss&&boss->mRow==0,"Adventure final boss did not enter in its required row");
            }
            else {
                board.mZombieCountDown=1;advance(100'000'000);
                Require(board.mCurrentWave>=1&&board.mZombies.mSize>0,"Adventure did not spawn its actual first wave");
                Zombie* enemy=nullptr;
                while(board.IterateZombies(enemy)) {
                    Require(board.RowCanHaveZombies(enemy->mRow),"Adventure first wave spawned on a dirt/absent row");
                    if(level==25) Require(enemy->mScaleZombie==0.5f,"Adventure Little Trouble did not use half-size zombies");
                }
            }
            const auto pause=board.mLevelTimeNs;board.mPaused=true;advance(100'000'000);board.mPaused=false;
            Require(board.mLevelTimeNs==pause,"Adventure pause advanced Board time");
            // Isolate final-loot selection from solving the whole combat. An
            // actual final enemy death drives TrySpawnLevelAward in each case.
            board.mCurrentWave=0;
            Zombie* enemy=nullptr;while(board.IterateZombies(enemy)) enemy->DieNoLoot(board.mLevelTimeNs,clocks.mVisualTimeNs,board.mLevelTimeNs);
            GridItem* item=nullptr;while(board.IterateGridItems(item)) if(item->mGridItemType==GRIDITEM_SCARY_POT) item->GridItemDie();
            Coin* coin=nullptr;while(board.IterateCoins(coin)) coin->Die();
            board.ProcessDeleteQueue();app.mEffectSystem->ProcessDeleteQueue();
            board.mCurrentWave=board.mNumWaves;board.mZombieCountDown=0;board.mLevelAwardSpawned=false;
            board.mBoardFadeOutCounter=-1;board.mNextSurvivalStageCounter=0;board.mLevelComplete=false;
            auto* finalEnemy=board.AddZombieInRow(level==50?ZOMBIE_BOSS:ZOMBIE_NORMAL,level==50?0:2,board.mNumWaves-1);
            finalEnemy->mX=500;
            finalEnemy->DieNoLoot(board.mLevelTimeNs,clocks.mVisualTimeNs,board.mLevelTimeNs);
            CoinType expectedAward=level%10==9?COIN_NOTE:replay?COIN_AWARD_MONEY_BAG:
                level==4?COIN_SHOVEL:level==14?COIN_ALMANAC:level==24?COIN_CARKEYS:
                level==34?COIN_TACO:level==44?COIN_WATERING_CAN:level==50?COIN_AWARD_SILVER_SUNFLOWER:COIN_FINAL_SEED_PACKET;
            bool found=false;coin=nullptr;
            while(board.IterateCoins(coin)) if(coin->mType==expectedAward) {
                found=true;
                if(expectedAward==COIN_FINAL_SEED_PACKET)
                    Require(coin->GetFinalSeedPacketType()==static_cast<SeedType>(level-level/5),
                        "Adventure progression reward contains the wrong newly unlocked plant");
            }
            Require(found&&board.HasLevelAwardDropped()&&app.mBoardResult==BOARDRESULT_WON,"Adventure final enemy did not drop its correct progression reward");
            std::cout<<"Adventure primary level="<<level<<" replay="<<replay<<" Hz="<<hz<<" background="<<background
                <<" waves="<<expectedWaves<<" rows="<<rows<<" conveyor="<<conveyor<<" award="<<int(expectedAward)<<'\n';
        }
        catch(const std::exception& error) {
            allPassed=false;
            std::cerr<<"Adventure failed level="<<level<<" replay="<<replay<<" Hz="<<hz<<" timeNs="<<board.mLevelTimeNs<<" message="<<error.what()<<'\n';
        }
    }
    Require(allPassed,"Adventure rules have failed cases; inspect per-level output");
    std::cout<<"PASS actual fifty adventure levels: first/replay rules, teaching actions, waves, special routes, pause and enemy-driven rewards at four schedules\n";
}

void HiddenRulesRuntime(TestApp& app, DecodeOnlySoundManager& sounds)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        Board* board=app.mBoard;
        PlayerInfo* player=app.mPlayerInfo;
        ZenGarden* garden=app.mZenGarden;
        PoolEffect* pool=app.mPoolEffect;
        GameMode mode=app.mGameMode;
        GameScenes scene=app.mGameScene;
        BoardResult result=app.mBoardResult;
        Sexy::UnsignedTimeCounter counter=app.mAppCounter;
        unsigned long long interval=Sexy::gUpdateIntervalNs,phase=Sexy::gLegacyTickPhaseNs;
        int whole=Sexy::gWholeLegacyTicks;
        double ticks=Sexy::gUpdateTicks,steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        ~Restore() {
            app.mBoard=board;app.mPlayerInfo=player;app.mZenGarden=garden;app.mPoolEffect=pool;
            app.mGameMode=mode;app.mGameScene=scene;app.mBoardResult=result;app.mAppCounter=counter;
            Sexy::gUpdateIntervalNs=interval;Sexy::gLegacyTickPhaseNs=phase;Sexy::gWholeLegacyTicks=whole;
            Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
        }
    } restore{app};
    PlayerInfo profile;ZenGarden garden;
    profile.mId=44;profile.mLevel=50;profile.mFinishedAdventure=1;
    app.mPlayerInfo=&profile;app.mZenGarden=&garden;
    static constexpr std::array<int,13> backgrounds{3,0,0,0,4,1,4,0,0,0,0,3,0};
    static constexpr std::array<int,13> waves{20,40,40,40,30,20,20,40,40,30,0,30,40};
    bool allPassed=true;
    for(int mode=GAMEMODE_LIMBO_AIR_RAID;mode<=GAMEMODE_LIMBO_SUNNY_DAY;++mode)
    for(const int hz:{100,30,240,0})
    {
        std::cout<<"Hidden begin mode="<<mode<<" Hz="<<hz<<'\n';
        Sexy::SRand(0xD7A00+mode);Sexy::gLegacyTickPhaseNs=0;
        clocks.mVisualTimeNs=clocks.mBattleTimeNs=0;app.mAppCounter=0;
        app.mGameMode=static_cast<GameMode>(mode);app.mGameScene=SCENE_PLAYING;app.mBoardResult=BOARDRESULT_NONE;
        PoolEffect pool;pool.PoolEffectInitialize();app.mPoolEffect=&pool;
        Board board(&app);
        try
        {
            board.InitLevel();board.StartLevel();auto& challenge=*board.mChallenge;
            Require(int(board.mBackground)==backgrounds[mode-21]&&board.mNumWaves==waves[mode-21]&&board.mSunMoney==50,
                "Hidden mode background, waves or starting sun differs from its rule");
            int sequence=0;
            constexpr std::array<long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
            const auto update=[&](unsigned long long step) {
                Sexy::ApplySimulationStep(step);clocks.mVisualTimeNs+=step;
                board.Update();app.mAppCounter++;
                board.ProcessDeleteQueue();app.mEffectSystem->ProcessDeleteQueue();
            };
            const auto advance=[&](unsigned long long duration) {
                unsigned long long elapsed=0;
                while(elapsed<duration) {
                    const auto step=std::min<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()],duration-elapsed);
                    update(step);elapsed+=step;
                }
            };
            const auto shovel=[&](Plant& plant) {
                board.MouseDownWithTool(plant.mX+40,plant.mY+40,1,CURSOR_TYPE_SHOVEL);
                Require(plant.mDead,"Actual shovel input did not remove its hidden-mode wallnut");
            };
            if(mode==GAMEMODE_LIMBO_ART_CHALLENGE_SUNFLOWER||mode==GAMEMODE_LIMBO_ART_CHALLENGE_WALLNUT) {
                int planted=0;
                for(int row=0;row<6;++row) for(int col=0;col<9;++col) {
                    const auto seed=challenge.GetArtChallengeSeed(col,row);
                    if(seed==SEED_NONE) continue;
                    Require(challenge.CanPlantAt(col,row,seed==SEED_WALLNUT?SEED_PEASHOOTER:SEED_WALLNUT)==PLANTING_NOT_ON_ART,
                        "Art challenge accepted a wrong seed in its required pattern");
                    board.AddPlant(col,row,seed,SEED_NONE);++planted;
                }
                Require(planted>0&&board.HasLevelAwardDropped(),"Filling the actual art pattern did not produce its reward");
            }
            else if(mode==GAMEMODE_LIMBO_BIG_TIME) {
                auto* nut=board.AddPlant(1,0,SEED_WALLNUT,SEED_NONE);
                auto* sun=board.AddPlant(1,1,SEED_SUNFLOWER,SEED_NONE);
                auto* flower=board.AddPlant(1,2,SEED_MARIGOLD,SEED_NONE);
                Require(nut->mPlantHealth==8000&&sun->mPlantHealth==600&&flower->mPlantHealth==600,"Big Time did not double its selected plants' health");
                sun->mNextLaunchTimeNs=flower->mNextLaunchTimeNs=1'000'000'000;
                sun->mLaunchWindowStartNs=flower->mLaunchWindowStartNs=0;
                advance(3'000'000'000);
                int suns=0,money=0,silvers=0;Coin* coin=nullptr;
                while(board.IterateCoins(coin)) {
                    suns+=coin->mType==COIN_SUN;
                    money+=coin->mType==COIN_SILVER||coin->mType==COIN_GOLD;
                    silvers+=coin->mType==COIN_SILVER;
                }
                Require(suns==2&&money==2&&silvers>=1,"Big Time did not produce its extra sun and silver coin");
                Require(std::abs(app.ReanimationGet(sun->mBodyReanimID)->mOverlayMatrix.m00-1.5f)<0.001f,
                    "Big Time did not apply its larger visual size");
            }
            else if(mode==GAMEMODE_LIMBO_BUNGEE_BLITZ) {
                board.mCurrentWave=9;board.SpawnZombieWave();int count=0;Zombie* enemy=nullptr;
                while(board.IterateZombies(enemy)) {Require(enemy->mZombieType==ZOMBIE_BUNGEE,"Bungee Blitz flag wave has a wrong zombie");++count;}
                Require(count==5&&board.mCurrentWave==10,"Bungee Blitz did not create its five bungees");
            }
            else if(mode==GAMEMODE_LIMBO_GRAVE_DANGER) {
                const int graves=board.GetGraveStonesCount();
                Require(graves>=4&&graves<=8,"Grave Danger initial columns/count changed");
                auto* nut=board.AddPlant(4,0,SEED_WALLNUT,SEED_NONE);
                auto* shell=board.AddPlant(4,0,SEED_PUMPKINSHELL,SEED_NONE);
                challenge.GraveDangerSpawnGraveAt(4,0);
                Require(nut->mDead&&shell->mDead&&board.GetGridItemAt(GRIDITEM_GRAVESTONE,4,0)&&board.GetGraveStonesCount()==graves+1,
                    "Grave Danger did not replace every plant layer in the chosen cell");
                board.ProcessDeleteQueue();board.mCurrentWave=6;challenge.SpawnZombieWave();
                Require(board.GetGraveStonesCount()==graves+2,"Grave Danger did not add a grave after its sixth wave");
            }
            else if(mode==GAMEMODE_LIMBO_HIGH_GRAVITY) {
                auto* balloon=board.AddZombieInRow(ZOMBIE_BALLOON,0,0);
                balloon->mX=700;balloon->mRootBindingGeneration=0;balloon->mVelocityX.bound=false;
                const float altitude=balloon->mAltitude;
                advance(1'000'000'000);
                Require(balloon->mAltitude<altitude-9.9f&&balloon->mAltitude>altitude-10.1f,"High Gravity flyer altitude is not driven by its elapsed time");
                Projectile pea;pea.ProjectileInitialize(300,200,0,0,PROJECTILE_PEA);
                pea.mVelX=0;pea.mVelY=0;pea.mVelZ=0;
                const float y=pea.mPosY;Sexy::ApplySimulationStep(1'000'000'000ULL/(hz?hz:100));
                pea.UpdateNormalMotion();Require(pea.mVelZ>0&&pea.mPosY>y,"High Gravity did not pull a straight projectile down");pea.Die();
                advance(9'000'000'000);
                Require(balloon->mZombiePhase==PHASE_BALLOON_WALKING,"High Gravity balloon did not eventually land");
            }
            else if(mode==GAMEMODE_LIMBO_ICE) {
                Require(!board.NeedSaveGame()&&board.mSeedBank->mNumPackets==6,"Ice demonstration gained persistence or lost its fixed cards");
                board.mZombieCountDown=1;advance(100'000'000);
                Require(board.mZombies.mSize==0&&board.mCurrentWave==0,"Ice demonstration used the ordinary wave scheduler");
                const auto before=sounds.playbackRequests[Sexy::SOUND_LOSEMUSIC];
                board.mLevelTimeNs=29'999'999'999ULL;clocks.mBattleTimeNs=board.mLevelTimeNs;
                advance(3);Require(sounds.playbackRequests[Sexy::SOUND_LOSEMUSIC]==before+1,"Ice demonstration missed its thirty-second audio event");
                advance(200'000'000);Require(sounds.playbackRequests[Sexy::SOUND_LOSEMUSIC]==before+1,"Ice demonstration repeated its one-shot audio event");
            }
            else if(mode==GAMEMODE_LIMBO_RESODDED) {
                Require(board.mPlantRow[0]==PLANTROW_DIRT&&board.mPlantRow[4]==PLANTROW_DIRT&&
                    !board.RowCanHaveZombieType(0,ZOMBIE_NORMAL)&&!board.RowCanHaveZombieType(4,ZOMBIE_NORMAL),
                    "Resodded admitted its dirt-edge rows before the fifth wave");
                board.mZombieCountDown=1;advance(100'000'000);
                Zombie* enemy=nullptr;while(board.IterateZombies(enemy)) Require(enemy->mRow>=1&&enemy->mRow<=3,"Resodded first wave used an edge row");
                board.mCurrentWave=5;
                Require(board.RowCanHaveZombieType(0,ZOMBIE_NORMAL)&&board.RowCanHaveZombieType(4,ZOMBIE_NORMAL),
                    "Resodded did not unlock its two edge rows after five waves");
            }
            else if(mode==GAMEMODE_LIMBO_SHOVEL) {
                Require(board.mPlants.mSize==45,"Shovel mode did not initialize five full rows of wallnuts");
                auto* nut=board.GetTopPlantAt(1,2,TOPPLANT_ONLY_NORMAL_POSITION);Require(nut!=nullptr,"Shovel target missing");
                shovel(*nut);board.ProcessDeleteQueue();Require(board.mPlants.mSize==44&&board.mPlantsShoveled==1,"Shovel removal count changed");
                challenge.mConveyorBeltCounter=1;advance(100'000'000);
                Require(board.mSeedBank->mNumPackets>0&&board.mSeedBank->mSeedPackets[0].mPacketType==SEED_PEASHOOTER,"Shovel conveyor did not deliver peashooters");
                const int sun=board.mSunMoney;board.mSeedBank->mSeedPackets[0].MouseDown(0,0,1);
                board.MouseDownWithPlant(board.GridToPixelX(1,2)+20,board.GridToPixelY(1,2)+20,1);
                auto* pea=board.GetTopPlantAt(1,2,TOPPLANT_ONLY_NORMAL_POSITION);
                Require(pea&&pea->mSeedType==SEED_PEASHOOTER&&board.mSunMoney==sun,"Shovel conveyor could not refill the hole without charging sun");
            }
            else if(mode==GAMEMODE_LIMBO_SQUIRREL) {
                Require(board.mPlants.mSize==45&&challenge.SquirrelCountUncaught()==7,"Squirrel initial wallnut/hidden target count changed");
                GridItem* squirrel=nullptr;GridItem* moving=nullptr;
                while(board.IterateGridItems(squirrel)) if(squirrel->mGridItemType==GRIDITEM_SQUIRREL&&squirrel->mGridItemState==GRIDITEM_STATE_SQUIRREL_WAITING) {
                    for(const auto direction:{std::pair{-1,0},std::pair{1,0},std::pair{0,-1},std::pair{0,1}})
                        if(!board.GetGridItemAt(GRIDITEM_SQUIRREL,squirrel->mGridX+direction.first,squirrel->mGridY+direction.second)&&
                            board.GetTopPlantAt(squirrel->mGridX+direction.first,squirrel->mGridY+direction.second,TOPPLANT_EATING_ORDER)) moving=squirrel;
                }
                Require(moving!=nullptr,"No squirrel has an open adjacent hiding place");
                const int x=moving->mGridX,y=moving->mGridY;
                shovel(*board.GetTopPlantAt(x,y,TOPPLANT_EATING_ORDER));board.ProcessDeleteQueue();advance(100'000'000);
                Require(std::abs(moving->mGridX-x)+std::abs(moving->mGridY-y)==1&&moving->mGridItemState>=GRIDITEM_STATE_SQUIRREL_RUNNING_UP&&
                    moving->mGridItemState<=GRIDITEM_STATE_SQUIRREL_RUNNING_RIGHT,"Uncovered squirrel did not run to an adjacent wallnut");
                advance(600'000'000);
                Require(moving->mGridItemState==GRIDITEM_STATE_SQUIRREL_WAITING,"Running squirrel did not return to its waiting state");
                Plant* nut=nullptr;while(board.IteratePlants(nut)) shovel(*nut);
                board.ProcessDeleteQueue();advance(100'000'000);
                Require(challenge.SquirrelCountUncaught()==0&&challenge.mChallengeScore==7&&board.HasLevelAwardDropped(),"Catching all seven squirrels did not produce its reward");
                advance(1'100'000'000);squirrel=nullptr;
                while(board.IterateGridItems(squirrel)) Require(squirrel->mGridItemType!=GRIDITEM_SQUIRREL,"Caught squirrel did not leave after its deadline");
            }
            else if(mode==GAMEMODE_LIMBO_STORMY_NIGHT) {
                Require(challenge.mChallengeState==STATECHALLENGE_STORM_FLASH_1,"Stormy Night did not begin its first flash");
                challenge.mChallengeStateCounter=1;advance(100'000'000);
                Require(challenge.mChallengeState>=STATECHALLENGE_STORM_FLASH_1&&challenge.mChallengeState<=STATECHALLENGE_STORM_FLASH_3&&
                    challenge.mChallengeStateCounter>0,"Stormy Night did not schedule its next flash");
                board.mPaused=true;challenge.mChallengeState=STATECHALLENGE_STORM_FLASH_1;challenge.mChallengeStateCounter=151;
                // Retail checks the paused stop before decrementing. A step
                // crossing the threshold therefore settles on the next call.
                advance(100'000'000);Require(challenge.mChallengeStateCounter==1,"Paused storm did not finish at its safe dark frame");
                const auto held=challenge.mChallengeStateCounter.mNanoseconds;advance(200'000'000);
                Require(challenge.mChallengeStateCounter.mNanoseconds==held,"Paused storm continued past its safe dark frame");board.mPaused=false;
            }
            else if(mode==GAMEMODE_LIMBO_SUNNY_DAY) {
                board.mSunCountDown=1;advance(100'000'000);
                Coin* sun=nullptr;bool large=false;while(board.IterateCoins(sun)) large|=sun->mType==COIN_LARGESUN;
                Require(large,"Sunny Day did not drop a large sun");
            }
            else { // Air Raid: use the actual first-wave scheduler, not a hand-created flyer.
                board.mZombieCountDown=1;advance(100'000'000);int flyers=0;Zombie* enemy=nullptr;
                while(board.IterateZombies(enemy)) {Require(enemy->mZombieType==ZOMBIE_BALLOON,"Air Raid first wave contains a non-flyer");++flyers;}
                Require(board.mCurrentWave==1&&flyers>0,"Air Raid did not spawn its first balloon wave");
            }
            const auto pause=board.mLevelTimeNs;board.mPaused=true;advance(100'000'000);board.mPaused=false;
            Require(board.mLevelTimeNs==pause,"Hidden-mode pause advanced Board time");
            std::cout<<"Hidden primary mode="<<mode<<" Hz="<<hz<<" background="<<backgrounds[mode-21]
                <<" waves="<<waves[mode-21]<<" checked=1\n";
        }
        catch(const std::exception& error) {
            allPassed=false;
            std::cerr<<"Hidden failed mode="<<mode<<" Hz="<<hz<<" timeNs="<<board.mLevelTimeNs<<" message="<<error.what()<<'\n';
        }
    }
    Require(allPassed,"Hidden mode rules have failed cases; inspect per-mode output");
    std::cout<<"PASS actual thirteen hidden modes: authored rules, actions, time thresholds and pause at four schedules\n";
}

void GardenRulesRuntime(TestApp& app)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        Board* board=app.mBoard;
        PlayerInfo* player=app.mPlayerInfo;
        ZenGarden* garden=app.mZenGarden;
        GameMode mode=app.mGameMode;
        GameScenes scene=app.mGameScene;
        BoardResult result=app.mBoardResult;
        Sexy::UnsignedTimeCounter counter=app.mAppCounter;
        unsigned long long interval=Sexy::gUpdateIntervalNs,phase=Sexy::gLegacyTickPhaseNs;
        int whole=Sexy::gWholeLegacyTicks;
        double ticks=Sexy::gUpdateTicks,steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        ~Restore() {
            app.mBoard=board;app.mPlayerInfo=player;app.mZenGarden=garden;
            app.mGameMode=mode;app.mGameScene=scene;app.mBoardResult=result;app.mAppCounter=counter;
            Sexy::gUpdateIntervalNs=interval;Sexy::gLegacyTickPhaseNs=phase;Sexy::gWholeLegacyTicks=whole;
            Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
        }
    } restore{app};
    bool allPassed=true;
    constexpr std::array<long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
    for(const int hz:{100,30,240,0})
    {
        std::cout<<"Garden begin Hz="<<hz<<'\n';
        Sexy::SRand(0x6A8E0);Sexy::gLegacyTickPhaseNs=0;
        clocks.mVisualTimeNs=123'456'789'000;clocks.mBattleTimeNs=0;app.mAppCounter=0;
        PlayerInfo profile;ZenGarden garden;
        profile.mId=45;profile.mLevel=50;profile.mFinishedAdventure=1;
        profile.mNumPottedPlants=4;
        constexpr std::array<SeedType,4> seeds{SEED_PEASHOOTER,SEED_SUNFLOWER,SEED_FUMESHROOM,SEED_TANGLEKELP};
        constexpr std::array<GardenType,4> gardens{GARDEN_MAIN,GARDEN_MAIN,GARDEN_MUSHROOM,GARDEN_AQUARIUM};
        for(int index=0;index<4;++index) {
            auto& spec=profile.mPottedPlant[index];spec.InitializePottedPlant(seeds[index]);
            spec.mWhichZenGarden=gardens[index];spec.mPlantAge=index?PLANTAGE_FULL:PLANTAGE_SPROUT;
            spec.mX=index==1?2:1;spec.mY=index<2?1:0;
            spec.mLastNeedFulfilledTime=garden.mFrameTime;
        }
        for(const auto purchase:{STORE_ITEM_MUSHROOM_GARDEN,STORE_ITEM_AQUARIUM_GARDEN,STORE_ITEM_WHEEL_BARROW,STORE_ITEM_PHONOGRAPH})
            profile.mPurchases[purchase]=1;
        for(const auto purchase:{STORE_ITEM_FERTILIZER,STORE_ITEM_BUG_SPRAY,STORE_ITEM_CHOCOLATE})
            profile.mPurchases[purchase]=PURCHASE_COUNT_OFFSET+10;
        profile.mPurchases[STORE_ITEM_STINKY_THE_SNAIL]=1;
        profile.mStinkyPosX=400;profile.mStinkyPosY=300;
        app.mPlayerInfo=&profile;app.mZenGarden=&garden;
        app.mGameMode=GAMEMODE_LIMBO_ZEN_GARDEN;app.mGameScene=SCENE_PLAYING;app.mBoardResult=BOARDRESULT_NONE;
        Board board(&app);
        try
        {
            board.InitLevel();board.StartLevel();
            Require(board.mBackground==BACKGROUND_GREENHOUSE&&!board.NeedSaveGame()&&garden.GetStinky(),
                "Garden did not initialize its background/snail or gained level-file persistence");
            int sequence=0;
            const auto update=[&](unsigned long long step) {
                Sexy::ApplySimulationStep(step);clocks.mVisualTimeNs+=step;
                board.Update();app.mAppCounter++;
                board.ProcessDeleteQueue();app.mEffectSystem->ProcessDeleteQueue();
            };
            const auto advance=[&](unsigned long long duration) {
                unsigned long long elapsed=0;
                while(elapsed<duration) {
                    const auto step=std::min<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()],duration-elapsed);
                    update(step);elapsed+=step;
                }
            };
            const auto findPlant=[&](int index) {
                Plant* plant=nullptr;while(board.IteratePlants(plant)) if(plant->mPottedPlantIndex==index) return plant;
                return static_cast<Plant*>(nullptr);
            };
            const auto setNeed=[&](int index,PottedPlantNeed need) {
                garden.RefreshTimeCache();auto& spec=profile.mPottedPlant[index];
                spec.mLastWateredTime=garden.mFrameTime-20;spec.mLastFertilizedTime=0;spec.mLastNeedFulfilledTime=0;
                spec.mPlantNeed=need;spec.mTimesFed=need==PLANTNEED_WATER?0:spec.mFeedingsPerGrow;
                Require(garden.GetPlantsNeed(&spec)==need,"Garden fixture did not establish its intended current need");
            };
            const auto feed=[&](int index,CursorType tool,int x=-1,int y=-1) {
                auto* target=findPlant(index);Require(target!=nullptr,"Garden feeding target is not instantiated");
                Plant* plant=nullptr;while(board.IteratePlants(plant)) plant->mHighlighted=false;
                target->mHighlighted=true;
                garden.MouseDownWithTool(x<0?int(target->mX+40):x,y<0?int(target->mY+40):y,tool);
                GridItem* item=nullptr;unsigned int id=0;
                while(board.IterateGridItems(item)) if(item->mGridItemType==GRIDITEM_ZEN_TOOL) id=board.mGridItems.DataArrayGetID(item);
                Require(id!=0,"Garden input did not create a real animated tool");
                const auto start=clocks.mVisualTimeNs;
                while(board.mGridItems.DataArrayTryToGet(id)&&clocks.mVisualTimeNs-start<30'000'000'000LL)
                    update(hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()]);
                Require(!board.mGridItems.DataArrayTryToGet(id),"Garden tool animation never completed and released its item");
            };
            Require(findPlant(0)&&findPlant(0)->mSeedType==SEED_SPROUT&&findPlant(1)&&!findPlant(2)&&!findPlant(3),
                "Garden instantiated the wrong visit's plants");
            setNeed(0,PLANTNEED_WATER);setNeed(1,PLANTNEED_WATER);
            feed(0,CURSOR_TYPE_WATERING_CAN);
            Require(profile.mPottedPlant[0].mTimesFed==1&&profile.mPottedPlant[1].mTimesFed==0,
                "Ordinary watering affected a neighboring plant or failed to water its target");
            for(const auto age:{PLANTAGE_SMALL,PLANTAGE_MEDIUM,PLANTAGE_FULL}) {
                setNeed(0,PLANTNEED_FERTILIZER);
                const int fertilizer=profile.mPurchases[STORE_ITEM_FERTILIZER];
                feed(0,CURSOR_TYPE_FERTILIZER);
                Require(profile.mPottedPlant[0].mPlantAge==age&&profile.mPurchases[STORE_ITEM_FERTILIZER]==fertilizer-1&&
                    findPlant(0)&&findPlant(0)->mSeedType==SEED_PEASHOOTER,
                    "Fertilizer did not advance one growth stage, preserve the plant record and charge one item");
            }
            profile.mPurchases[STORE_ITEM_GOLD_WATERINGCAN]=1;
            setNeed(0,PLANTNEED_WATER);setNeed(1,PLANTNEED_WATER);
            const int waterX=int((findPlant(0)->mX+findPlant(1)->mX)/2+40),waterY=int(findPlant(0)->mY+40);
            Require(board.IsPlantInGoldWateringCanRange(waterX,waterY,findPlant(0))&&board.IsPlantInGoldWateringCanRange(waterX,waterY,findPlant(1)),
                "Garden golden-can fixture does not cover both plants");
            feed(0,CURSOR_TYPE_WATERING_CAN,waterX,waterY);
            Require(profile.mPottedPlant[0].mTimesFed==1&&profile.mPottedPlant[1].mTimesFed==1,"Golden watering did not feed both eligible plants");
            for(const auto need:{PLANTNEED_BUGSPRAY,PLANTNEED_PHONOGRAPH}) {
                setNeed(0,need);
                const int spray=profile.mPurchases[STORE_ITEM_BUG_SPRAY];
                feed(0,need==PLANTNEED_BUGSPRAY?CURSOR_TYPE_BUG_SPRAY:CURSOR_TYPE_PHONOGRAPH);
                Require(profile.mPottedPlant[0].mPlantNeed==PLANTNEED_NONE&&profile.mPottedPlant[0].mLastNeedFulfilledTime>0,
                    "Garden special need was not fulfilled after its tool completed");
                Require(profile.mPurchases[STORE_ITEM_BUG_SPRAY]==spray-(need==PLANTNEED_BUGSPRAY),"Garden spray/phonograph inventory charge changed");
            }
            auto* grown=findPlant(0);Plant* plant=nullptr;
            while(board.IteratePlants(plant)) plant->mHighlighted=false;grown->mHighlighted=true;
            const int chocolate=profile.mPurchases[STORE_ITEM_CHOCOLATE];
            garden.MouseDownWithTool(grown->mX+40,grown->mY+40,CURSOR_TYPE_CHOCOLATE);
            Require(profile.mPurchases[STORE_ITEM_CHOCOLATE]==chocolate-1&&profile.mPottedPlant[0].mLastChocolateTime>0,
                "Garden chocolate did not charge and bind its wall-clock effect");
            garden.MovePlant(grown,2,2);
            Require(profile.mPottedPlant[0].mX==2&&profile.mPottedPlant[0].mY==2&&
                board.GetTopPlantAt(2,2,TOPPLANT_ONLY_NORMAL_POSITION)==grown,"Garden move did not update runtime buckets and persistent coordinates");
            garden.MouseDownWithTool(grown->mX+40,grown->mY+40,CURSOR_TYPE_WHEEELBARROW);
            board.ProcessDeleteQueue();
            Require(garden.GetPottedPlantInWheelbarrow()==&profile.mPottedPlant[0]&&!findPlant(0),"Wheelbarrow failed to remove the live plant while retaining its record");
            garden.GotoNextGarden();board.ProcessDeleteQueue();
            Require(garden.mGardenType==GARDEN_MUSHROOM&&board.mBackground==BACKGROUND_MUSHROOM_GARDEN&&
                findPlant(2)&&!findPlant(2)->mIsAsleep&&!findPlant(1)&&!garden.GetStinky(),"Mushroom garden did not restore the correct awake plant set");
            garden.GotoNextGarden();board.ProcessDeleteQueue();
            Require(garden.mGardenType==GARDEN_AQUARIUM&&board.mBackground==BACKGROUND_ZOMBIQUARIUM&&findPlant(3)&&!findPlant(2),
                "Aquarium garden did not restore its aquatic plant set");
            garden.MouseDownWithFullWheelBarrow(200,200);
            Require(garden.GetPottedPlantInWheelbarrow()==&profile.mPottedPlant[0]&&!findPlant(0),"Aquarium accepted a non-aquatic wheelbarrow plant");
            const int savedSnailX=profile.mStinkyPosX,savedSnailY=profile.mStinkyPosY;
            garden.GotoNextGarden();board.ProcessDeleteQueue();
            Require(garden.mGardenType==GARDEN_MAIN&&findPlant(1)&&garden.GetStinky()&&
                garden.GetStinky()->mPosX==savedSnailX&&garden.GetStinky()->mPosY==savedSnailY,"Returning to the main garden lost plants or the saved snail position");
            garden.MouseDownWithFullWheelBarrow(garden.GridToPixelX(2,2)+40,garden.GridToPixelY(2,2)+40);
            Require(!garden.GetPottedPlantInWheelbarrow()&&findPlant(0)&&profile.mPottedPlant[0].mWhichZenGarden==GARDEN_MAIN,
                "Wheelbarrow could not restore its plant to a free main-garden cell");
            profile.SaveDetails();PlayerInfo loaded;loaded.mId=45;
            Require(loaded.LoadDetails()&&loaded.mNumPottedPlants==4&&loaded.mPottedPlant[0].mPlantAge==PLANTAGE_FULL&&
                loaded.mPottedPlant[0].mX==2&&loaded.mPottedPlant[0].mY==2&&loaded.mPottedPlant[0].mWhichZenGarden==GARDEN_MAIN,
                "Garden profile roundtrip lost growth, placement or its garden");
            garden.LeaveGarden();
            std::cout<<"Garden primary Hz="<<hz<<" growth=3 tools=7 gardens=3 wheelbarrow=1 reload=1\n";
        }
        catch(const std::exception& error) {
            allPassed=false;
            std::cerr<<"Garden failed Hz="<<hz<<" timeNs="<<board.mLevelTimeNs<<" message="<<error.what()<<'\n';
        }
    }
    for(const int height:{0,1,12,50,99})
    for(const int hz:{100,30,240,0})
    {
        std::cout<<"Tree begin height="<<height<<" Hz="<<hz<<'\n';
        Sexy::SRand(0x7EE00+height);Sexy::gLegacyTickPhaseNs=0;
        clocks.mVisualTimeNs=123'456'789'000;clocks.mBattleTimeNs=0;app.mAppCounter=0;
        PlayerInfo profile;ZenGarden garden;
        profile.mId=46;profile.mLevel=50;profile.mFinishedAdventure=1;
        profile.mChallengeRecords[GAMEMODE_TREE_OF_WISDOM]=height;
        profile.mPurchases[STORE_ITEM_TREE_FOOD]=PURCHASE_COUNT_OFFSET+3;
        app.mPlayerInfo=&profile;app.mZenGarden=&garden;app.mGameMode=GAMEMODE_TREE_OF_WISDOM;
        app.mGameScene=SCENE_LEVEL_INTRO;app.mBoardResult=BOARDRESULT_NONE;
        Board board(&app);
        try
        {
            board.InitLevel();auto& challenge=*board.mChallenge;
            auto* tree=app.ReanimationGet(challenge.mReanimChallenge);
            Require(board.mBackground==BACKGROUND_TREEOFWISDOM&&!board.NeedSaveGame()&&
                tree->SamplePlayback().phaseNs==ReanimClock::SignedMulDiv(990,tree->mPeriodNs,1000),"Tree entry did not show its already-grown pose");
            int sequence=0;
            const auto update=[&](unsigned long long step) {
                Sexy::ApplySimulationStep(step);clocks.mVisualTimeNs+=step;board.Update();app.mAppCounter++;
                board.ProcessDeleteQueue();app.mEffectSystem->ProcessDeleteQueue();
            };
            const auto advance=[&](unsigned long long duration) {
                unsigned long long elapsed=0;
                while(elapsed<duration) {
                    const auto step=std::min<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()],duration-elapsed);
                    update(step);elapsed+=step;
                }
            };
            board.mDrawCount=0;advance(100'000'000);
            Require(board.mLevelTimeNs==0&&challenge.TreeOfWisdomGetSize()==height,"Tree intro advanced Board time or invented growth");
            app.mGameScene=SCENE_PLAYING;board.StartLevel();advance(100'000'000);
            const int inventory=profile.mPurchases[STORE_ITEM_TREE_FOOD];
            board.mCursorObject->mCursorType=CURSOR_TYPE_TREE_FOOD;
            challenge.TreeOfWisdomTool(380,340);
            Require(profile.mPurchases[STORE_ITEM_TREE_FOOD]==inventory-1&&!challenge.TreeOfWisdomCanFeed()&&challenge.TreeOfWisdomGetSize()==height,
                "Tree food click did not charge once and defer growth to animation completion");
            for(int frame=0;frame<(hz?hz:100)*30&&challenge.TreeOfWisdomGetSize()==height;++frame)
                update(hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()]);
            Require(challenge.TreeOfWisdomGetSize()==height+1,"Tree food animation did not cause exactly one growth");
            advance(3'000'000'000);
            Require(challenge.TreeOfWisdomGetSize()==height+1&&profile.mPurchases[STORE_ITEM_TREE_FOOD]==inventory-1,
                "Tree continued growing or charged repeatedly after a single feeding");
            if(height==99) Require(profile.mEarnedAchievements[ToweringWisdom],"Tree reaching one hundred failed to grant its achievement");
            const auto pause=board.mLevelTimeNs;board.mPaused=true;advance(100'000'000);board.mPaused=false;
            Require(board.mLevelTimeNs==pause,"Tree pause advanced Board time");
            std::cout<<"Tree primary height="<<height<<" Hz="<<hz<<" grown="<<height+1<<" charged=1\n";
        }
        catch(const std::exception& error) {
            allPassed=false;
            std::cerr<<"Tree failed height="<<height<<" Hz="<<hz<<" timeNs="<<board.mLevelTimeNs<<" message="<<error.what()<<'\n';
        }
    }
    Require(allPassed,"Garden/tree rules have failed cases; inspect scenario output");
    std::cout<<"PASS actual garden/tree rules: animated tools, growth, needs, movement, three-garden cycle, wheelbarrow, profile reload and one-shot tree feeding at four schedules\n";
}

void IntroRulesRuntime(TestApp& app, DecodeOnlySoundManager& sounds)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        PlayerInfo* player=app.mPlayerInfo;
        ZenGarden* garden=app.mZenGarden;
        PoolEffect* pool=app.mPoolEffect;
        GameMode mode=app.mGameMode;
        GameScenes scene=app.mGameScene;
        BoardResult result=app.mBoardResult;
        bool mute=app.mMuteSoundsForCutscene;
        Sexy::UnsignedTimeCounter counter=app.mAppCounter;
        unsigned long long interval=Sexy::gUpdateIntervalNs,phase=Sexy::gLegacyTickPhaseNs;
        int whole=Sexy::gWholeLegacyTicks;
        double ticks=Sexy::gUpdateTicks,steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        ~Restore() {
            app.KillBoard();app.ProcessSafeDeleteList();
            app.mPlayerInfo=player;app.mZenGarden=garden;app.mPoolEffect=pool;
            app.mGameMode=mode;app.mGameScene=scene;app.mBoardResult=result;app.mAppCounter=counter;
            app.mMuteSoundsForCutscene=mute;
            Sexy::gUpdateIntervalNs=interval;Sexy::gLegacyTickPhaseNs=phase;Sexy::gWholeLegacyTicks=whole;
            Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
        }
    } restore{app};
    Require(!app.mBoard&&!app.mSeedChooserScreen,"Intro fixture requires no prior live Board/chooser");
    bool allPassed=true;
    constexpr std::array<unsigned long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
    for(const int hz:{100,30,240,0})
    {
        std::cout<<"Intro begin Hz="<<hz<<'\n';
        PlayerInfo profile;ZenGarden garden;PoolEffect pool;pool.PoolEffectInitialize();
        profile.mId=47;profile.mLevel=1;profile.mFinishedAdventure=0;
        app.mPlayerInfo=&profile;app.mZenGarden=&garden;app.mPoolEffect=&pool;
        clocks.mVisualTimeNs=123'456'789'000;clocks.mBattleTimeNs=0;app.mAppCounter=0;
        Sexy::SRand(0x1A7E0);Sexy::gLegacyTickPhaseNs=0;
        try
        {
            app.PreNewGame(GAMEMODE_INTRO,false);
            auto* introBoard=app.mBoard;
            Require(introBoard&&introBoard->mBackground==BACKGROUND_3_POOL&&!introBoard->NeedSaveGame(),"Intro application entry did not create its pool demonstration");
            const auto hugeBefore=sounds.playbackRequests[Sexy::SOUND_HUGE_WAVE],sirenBefore=sounds.playbackRequests[Sexy::SOUND_SIREN];
            int sequence=0;
            unsigned long long elapsed=0;
            const auto update=[&](unsigned long long step) {
                Sexy::ApplySimulationStep(step);Sexy::BeginTimedUpdate();clocks.mVisualTimeNs+=step;
                auto* active=app.mBoard;
                {WidgetLifetimeHold hold(*active);active->mDrawCount=1;active->Update();}
                app.mAppCounter++;
                if(app.mBoard) app.mBoard->ProcessDeleteQueue();
                app.mEffectSystem->ProcessDeleteQueue();app.ProcessSafeDeleteList();
            };
            auto step=hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()];
            do {
                update(step);elapsed+=step;
                if(elapsed>=10'000'000ULL) break;
                step=hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()];
            } while(true);
            std::cout<<"Intro warmup Hz="<<hz<<" triggerElapsedNs="<<elapsed<<" boardNs="<<introBoard->mLevelTimeNs<<'\n';
            Require(introBoard->mLevelTimeNs==elapsed+1'000'000'000ULL,
                "Intro warmup simulated a target-FPS-dependent duration instead of its authored one second");
            Require(Sexy::gUpdateIntervalNs==step&&!introBoard->mCutScene->mPreUpdatingBoard&&introBoard->mPlants.mSize>0&&introBoard->mZombies.mSize>0,
                "Intro warmup lost the caller step, guard or demonstration actors");
            const auto heldTime=introBoard->mLevelTimeNs;
            const auto heldCutscene=introBoard->mCutScene->mCutsceneTime.mNanoseconds;
            introBoard->mPaused=true;update(step);introBoard->mPaused=false;
            Require(introBoard->mLevelTimeNs==heldTime&&introBoard->mCutScene->mCutsceneTime.mNanoseconds==heldCutscene,
                "Paused intro advanced its demonstration or movie timeline");
            while(app.mGameMode==GAMEMODE_INTRO&&elapsed<14'000'000'000ULL) {
                const auto dt=hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()];update(dt);elapsed+=dt;
            }
            Require(app.mGameMode==GAMEMODE_ADVENTURE&&app.mBoard&&app.mBoard!=introBoard&&app.mGameScene==SCENE_LEVEL_INTRO&&
                app.mBoard->mLevel==1&&app.mBoard->mLevelTimeNs==0,"Intro did not hand off to a clean first Adventure Board");
            Require(elapsed>=13'890'000'000ULL&&elapsed<13'930'000'000ULL,"Intro movie end time changed with its schedule");
            Require(sounds.playbackRequests[Sexy::SOUND_HUGE_WAVE]==hugeBefore+1&&sounds.playbackRequests[Sexy::SOUND_SIREN]==sirenBefore+1&&
                !app.mMuteSoundsForCutscene,"Intro audio events repeated, were missed or left Adventure muted");
            std::cout<<"Intro primary Hz="<<hz<<" warmupNs=1000000000 adventure=1 audio=1 elapsedNs="<<elapsed<<'\n';
        }
        catch(const std::exception& error) {
            allPassed=false;
            std::cerr<<"Intro failed Hz="<<hz<<" message="<<error.what()<<'\n';
        }
        app.KillBoard();app.ProcessSafeDeleteList();
    }
    Require(allPassed,"Intro rules have failed cases; inspect per-schedule output");
    std::cout<<"PASS actual intro: fixed-duration warmup, paused clocks, movie events and application handoff at four schedules\n";
}

void AlmanacRulesRuntime(TestApp& app, bool clockOnly=false)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        PlayerInfo* player=app.mPlayerInfo;
        ZenGarden* garden=app.mZenGarden;
        PoolEffect* pool=app.mPoolEffect;
        GameMode mode=app.mGameMode;
        GameScenes scene=app.mGameScene;
        Rect bounds=app.mScreenBounds;
        Rect mouseBounds=app.mWidgetManager->mMouseDestRect;
        Sexy::UnsignedTimeCounter counter=app.mAppCounter;
        unsigned long long interval=Sexy::gUpdateIntervalNs,phase=Sexy::gLegacyTickPhaseNs;
        int whole=Sexy::gWholeLegacyTicks;
        double ticks=Sexy::gUpdateTicks,steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        ~Restore() {
            app.KillAlmanacDialog();app.KillBoard();app.ProcessSafeDeleteList();
            app.mPlayerInfo=player;app.mZenGarden=garden;app.mPoolEffect=pool;
            app.mGameMode=mode;app.mGameScene=scene;app.mScreenBounds=bounds;app.mWidgetManager->mMouseDestRect=mouseBounds;
            app.mAppCounter=counter;
            Sexy::gUpdateIntervalNs=interval;Sexy::gLegacyTickPhaseNs=phase;Sexy::gWholeLegacyTicks=whole;
            Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
        }
    } restore{app};
    Require(!app.mBoard&&!app.GetDialog(DIALOG_ALMANAC),"Almanac fixture requires an empty application route");
    app.mReanimatorCache->PrebuildCacheFrames();
    bool allPassed=true;
    constexpr std::array<unsigned long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
    for(const bool inLevel:{false,true})
    for(const int width:{800,1400})
    for(const int hz:{100,30,240,0})
    {
        PlayerInfo profile;ZenGarden garden;PoolEffect pool;pool.PoolEffectInitialize();
        profile.mId=48;profile.mLevel=31;profile.mFinishedAdventure=2;
        std::fill(std::begin(profile.mPurchases),std::end(profile.mPurchases),1);
        app.mPlayerInfo=&profile;app.mZenGarden=&garden;app.mPoolEffect=&pool;
        app.mGameMode=GAMEMODE_ADVENTURE;app.mGameScene=SCENE_MENU;
        app.mScreenBounds=Rect((800-width)/2,0,width,600);
        clocks.mVisualTimeNs=123'456'789'000;clocks.mBattleTimeNs=0;
        Sexy::SRand(0xA1AA0);Sexy::gLegacyTickPhaseNs=0;
        auto& render=*app.mDDInterface->mRenderInterface;
        struct Target {Image* image;~Target(){delete image;}} target{render.CreateRenderImage(width,600)};
        try
        {
            std::cout<<"Almanac begin level="<<inLevel<<" width="<<width<<" Hz="<<hz<<'\n';
            if(inLevel)
            {
                app.PreNewGame(GAMEMODE_ADVENTURE,false);
                app.mBoard->RemoveCutsceneZombies();app.StartPlaying();
                app.mBoard->NewPlant(0,0,SEED_PEASHOOTER,SEED_NONE);
            }
            auto* dialog=app.DoAlmanacDialog();
            Require(dialog&&dialog->mPlant&&dialog->mZombie,"Almanac index did not create both previews");
            Require(!dialog->mPlant->mBoard&&!dialog->mZombie->mBoard,"Almanac previews acquired the live battle Board");
            const auto battle=app.mBoard?app.mBoard->mLevelTimeNs:0;
            Reanimation* retained=nullptr;
            if(inLevel) {
                Plant* first=nullptr;Require(app.mBoard->IteratePlants(first),"Almanac fixture has no retained battle plant");
                retained=app.ReanimationGet(first->mBodyReanimID);
            }
            const auto retainedPhase=retained?retained->SamplePlayback().phaseNs:0;
            const auto retainedBirthDomain=retained?clocks.mTimeDomains[clocks.mReanimations.DataArrayIndexOf(retained)]:ReanimTimeDomain::Visual;
            const auto start=clocks.mVisualTimeNs;
            const auto indexPhase=app.ReanimationGet(dialog->mPlant->mBodyReanimID)->SamplePlayback().phaseNs;
            int sequence=0;
            const auto advance=[&](unsigned long long duration) {
                unsigned long long elapsed=0;
                while(elapsed<duration) {
                    const auto step=std::min<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()],duration-elapsed);
                    Sexy::ApplySimulationStep(step);app.UpdateFrames();
                    app.mEffectSystem->ProcessDeleteQueue();elapsed+=step;
                }
            };
            const auto capture=[&]() {
                {GpuImageDrawScope scope(render,target.image);Graphics graphics(target.image);
                 graphics.mColor=Color::Black;graphics.FillRect(0,0,width,600);
                 graphics.Translate(dialog->mX-app.mScreenBounds.mX,dialog->mY-app.mScreenBounds.mY);
                 dialog->Draw(&graphics);}
                render.Flush();return render.ReadImagePixels(*target.image).pixels;
            };
            advance(200'000'000);
            Require(clocks.mVisualTimeNs==start+200'000'000&&
                app.ReanimationGet(dialog->mPlant->mBodyReanimID)->SamplePlayback().phaseNs!=indexPhase,
                "Actual Almanac index did not animate while its owning battle was paused");
            if(inLevel) Require(app.mBoard->mPaused&&app.mBoard->mLevelTimeNs==battle,"Almanac advanced the paused battle");
            if(!clockOnly) {
            dialog->SetPage(ALMANAC_PAGE_PLANTS);
            for(int seed=0;seed<NUM_ALMANAC_SEEDS;++seed)
            {
                int x,y;dialog->GetSeedPosition(static_cast<SeedType>(seed),x,y);
                Require(dialog->SeedHitTest(x+5,y+5)==seed,"Unlocked Almanac plant icon is not selectable");
                dialog->MouseDown(x+5,y+5,1);
                Require(dialog->mSelectedSeed==seed&&dialog->mPlant&&dialog->mPlant->mSeedType==seed&&!dialog->mPlant->mBoard,
                    "Almanac plant click selected a wrong or Board-owned preview");
                advance(200'000'000);
                const auto withPreview=capture();const auto originalX=dialog->mPlant->mX;dialog->mPlant->mX+=10000;
                const auto withoutPreview=capture();dialog->mPlant->mX=originalX;
                Require(withPreview!=withoutPreview,"Almanac plant preview produced no visible pixels");
                std::cout<<"Almanac plant level="<<inLevel<<" width="<<width<<" Hz="<<hz<<" seed="<<seed<<" visible=1\n";
            }
            dialog->SetPage(ALMANAC_PAGE_ZOMBIES);
            for(int type=0;type<NUM_ALMANAC_ZOMBIES;++type)
            {
                const auto zombieType=static_cast<ZombieType>(type);
                int x,y;dialog->GetZombiePosition(zombieType,x,y);
                Require(dialog->ZombieHitTest(x+5,y+5)==type,"Unlocked Almanac zombie icon is not selectable");
                dialog->MouseDown(x+5,y+5,1);
                Require(dialog->mSelectedZombie==type&&dialog->mZombie&&dialog->mZombie->mZombieType==type&&!dialog->mZombie->mBoard,
                    "Almanac zombie click selected a wrong or Board-owned preview");
                advance(200'000'000);
                const auto withPreview=capture();const auto originalX=dialog->mZombie->mX;dialog->mZombie->mX+=10000;
                const auto withoutPreview=capture();dialog->mZombie->mX=originalX;
                Require(withPreview!=withoutPreview,"Almanac zombie preview produced no visible pixels");
                std::cout<<"Almanac zombie level="<<inLevel<<" width="<<width<<" Hz="<<hz<<" type="<<type<<" visible=1\n";
            }
            }
            dialog->SetPage(ALMANAC_PAGE_INDEX);advance(100'000'000);capture();
            if(inLevel) Require(retained->SamplePlayback().phaseNs==retainedPhase&&app.mBoard->mLevelTimeNs==battle,
                "Almanac UI time advanced the retained battle plant");
            app.KillAlmanacDialog();app.ProcessSafeDeleteList();
            Require(!app.GetDialog(DIALOG_ALMANAC),"Almanac close retained its dialog");
            if(inLevel) Require(!app.mBoard->mPaused&&app.mBoard->mLevelTimeNs==battle,"Almanac close changed the paused battle time or failed to resume it");
            if(inLevel) {
                const auto expected=retained->SamplePlaybackAt(static_cast<std::int64_t>(battle)+7'891'011).phaseNs;
                Sexy::ApplySimulationStep(7'891'011);app.UpdateFrames();
                std::cout<<"Almanac resume width="<<width<<" Hz="<<hz<<" birthDomain="<<int(retainedBirthDomain)
                    <<" domain="<<int(clocks.mTimeDomains[clocks.mReanimations.DataArrayIndexOf(retained)])
                    <<" battle="<<app.mBoard->mLevelTimeNs<<" expectedPhase="<<expected<<" actualPhase="<<retained->SamplePlayback().phaseNs<<'\n';
                Require(app.mBoard->mLevelTimeNs==battle+7'891'011&&retained->SamplePlayback().phaseNs==expected,
                    "Closing Almanac did not resume with exactly one battle animation step");
            }
            if(clockOnly) std::cout<<"Almanac clock level="<<inLevel<<" width="<<width<<" Hz="<<hz<<" closed=1\n";
            else std::cout<<"Almanac primary level="<<inLevel<<" width="<<width<<" Hz="<<hz<<" plants=49 zombies=26 closed=1\n";
        }
        catch(const std::exception& error) {
            allPassed=false;std::cerr<<"Almanac failed level="<<inLevel<<" width="<<width<<" Hz="<<hz<<" message="<<error.what()<<'\n';
        }
        app.KillAlmanacDialog();app.KillBoard();app.ProcessSafeDeleteList();
        app.mEffectSystem->ProcessDeleteQueue();
    }
    Require(allPassed,"Almanac rules have failed cases; inspect per-case output");
    if(clockOnly) std::cout<<"PASS actual Almanac clocks: index animation, paused battle and exact resume at two widths/four schedules\n";
    else std::cout<<"PASS actual Almanac: all49 plants/all26 zombies, icon input, visible previews, menu/paused-battle clocks and close at two widths/four schedules\n";
}

void ActorBirthClockRuntime(TestApp& app)
{
    auto& holder=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        Board* board=app.mBoard;PlayerInfo* player=app.mPlayerInfo;ZenGarden* garden=app.mZenGarden;
        GameMode mode=app.mGameMode;GameScenes scene=app.mGameScene;
        unsigned long long interval=Sexy::gUpdateIntervalNs;
        double ticks=Sexy::gUpdateTicks,steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        ~Restore() {
            app.mBoard=board;app.mPlayerInfo=player;app.mZenGarden=garden;app.mGameMode=mode;app.mGameScene=scene;
            Sexy::gUpdateIntervalNs=interval;Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
        }
    } restore{app};
    PlayerInfo profile;ZenGarden garden;
    profile.mId=50;profile.mLevel=31;profile.mFinishedAdventure=2;
    app.mPlayerInfo=&profile;app.mZenGarden=&garden;app.mGameMode=GAMEMODE_ADVENTURE;
    bool allPassed=true;int checked=0;
    for(const auto scene:{SCENE_PLAYING,SCENE_LEVEL_INTRO})
    for(const unsigned long long born:{0ULL,31'234'567'890ULL})
    for(int group=0;group<4;++group)
    for(int type=0;type<(group==0?NUM_SEED_TYPES:NUM_ZOMBIE_TYPES);++type)
    {
        app.mGameScene=scene;Sexy::SRand(0xB1700+type);
        Board board(&app);board.mLevel=31;board.mBackground=BACKGROUND_3_POOL;
        board.mCurrentWave=5;board.mNumWaves=20;board.mTotalSpawnedWaves=0;board.mSeedBank->mNumPackets=0;
        board.mLevelTimeNs=born;holder.mBattleTimeNs=born;holder.mVisualTimeNs=123'456'789'000+born;
        for(int row=0;row<MAX_GRID_SIZE_Y;++row) {
            board.mPlantRow[row]=(row==2||row==3)?PLANTROW_POOL:PLANTROW_NORMAL;
            board.mIceMinX[row]=1000;board.mIceTimer[row]=0;board.mIceParticleID[row]=PARTICLESYSTEMID_NULL;
        }
        const bool preview=group>=2;
        const auto domain=preview||scene==SCENE_LEVEL_INTRO?ReanimTimeDomain::Visual:ReanimTimeDomain::Battle;
        const auto animationNow=domain==ReanimTimeDomain::Visual?holder.mVisualTimeNs:holder.mBattleTimeNs;
        int animations=0;
        const auto check=[&](ReanimationID id) {
            if(auto* animation=app.ReanimationTryToGet(id)) {
                ++animations;
                const auto expectedDomain=animation->mIsAttachment?domain:ReanimTimeDomain::Visual;
                const auto expectedTime=animation->mIsAttachment?animationNow:holder.mVisualTimeNs;
                Require(holder.mTimeDomains[holder.mReanimations.DataArrayIndexOf(animation)]==expectedDomain&&
                    animation->mLastUpdateTimeNs==expectedTime,"Actor birth left its animation in a different clock until the first update");
            }
        };
        try
        {
            Reanimation* body=nullptr;
            if(group==0) {
                auto* plant=board.NewPlant(0,1,static_cast<SeedType>(type),SEED_NONE);
                for(const auto id:{plant->mBodyReanimID,plant->mHeadReanimID,plant->mHeadReanimID2,plant->mHeadReanimID3,
                    plant->mBlinkReanimID,plant->mLightReanimID,plant->mSleepingReanimID}) check(id);
                body=app.ReanimationTryToGet(plant->mBodyReanimID);
            }
            else {
                const int from=group==1?0:group==2?Zombie::ZOMBIE_WAVE_CUTSCENE:Zombie::ZOMBIE_WAVE_UI;
                auto* zombie=board.AddZombieInRow(static_cast<ZombieType>(type),2,from);
                body=app.ReanimationTryToGet(zombie->mBodyReanimID);
                Zombie* member=nullptr;
                while(board.IterateZombies(member))
                    for(const auto id:{member->mBodyReanimID,member->mSpecialHeadReanimID,member->mZombatarHeadReanimID,
                        member->mMoweredReanimID,member->mBossFireBallReanimID}) check(id);
            }
            // Use actual manager dispatch as well as birth metadata for the
            // two ordinary actors; their first update must consume its age.
            if(body&&((group==0&&type==SEED_PEASHOOTER)||(group==1&&type==ZOMBIE_NORMAL))) {
                constexpr auto step=7'891'011ULL;
                // Retained on-board zombies deliberately do not update during
                // the intro. Their street-preview counterparts are group 2.
                const bool retainedIntroZombie = group==1 && scene==SCENE_LEVEL_INTRO;
                const auto expected = retainedIntroZombie ? body->SamplePlayback().phaseNs
                    : body->SamplePlaybackAt(animationNow+step).phaseNs;
                Sexy::ApplySimulationStep(step);holder.mVisualTimeNs+=step;
                if(scene==SCENE_PLAYING) board.mLevelTimeNs+=step;
                holder.mBattleTimeNs=board.mLevelTimeNs;board.UpdateGameObjects(scene==SCENE_PLAYING?step:0);
                Require(body->SamplePlayback().phaseNs==expected,"The first managed actor update discarded its elapsed animation age");
            }
            ++checked;
            std::cout<<"Actor birth group="<<group<<" type="<<type<<" scene="<<scene<<" born="<<born<<" animations="<<animations<<" ok=1\n";
        }
        catch(const std::exception& error) {
            allPassed=false;std::cerr<<"Actor birth failed group="<<group<<" type="<<type<<" scene="<<scene<<" born="<<born<<" message="<<error.what()<<'\n';
        }
    }
    Require(allPassed&&checked==620,"Actor birth clock cases failed or the catalog is incomplete");
    std::cout<<"PASS actual actor birth clocks: 620 plant/zombie/preview creation contexts, all animation fields and the first managed update\n";
}

void AwardRulesRuntime(TestApp& app)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        PlayerInfo* player=app.mPlayerInfo;
        ZenGarden* garden=app.mZenGarden;
        PoolEffect* pool=app.mPoolEffect;
        GameMode mode=app.mGameMode;
        GameScenes scene=app.mGameScene;
        Rect bounds=app.mScreenBounds;
        BoardResult result=app.mBoardResult;
        unsigned long long interval=Sexy::gUpdateIntervalNs,phase=Sexy::gLegacyTickPhaseNs;
        int whole=Sexy::gWholeLegacyTicks;
        double ticks=Sexy::gUpdateTicks,steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        ~Restore() {
            app.fixtureNestedPump={};app.KillAlmanacDialog();app.KillStoreScreen();
            app.KillAwardScreen();app.KillCreditScreen();app.KillChallengeScreen();app.KillGameSelector();
            app.KillBoard();app.ProcessSafeDeleteList();
            app.mPlayerInfo=player;app.mZenGarden=garden;app.mPoolEffect=pool;
            app.mGameMode=mode;app.mGameScene=scene;app.mScreenBounds=bounds;app.mBoardResult=result;
            Sexy::gUpdateIntervalNs=interval;Sexy::gLegacyTickPhaseNs=phase;Sexy::gWholeLegacyTicks=whole;
            Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
        }
    } restore{app};
    Require(!app.mBoard&&!app.mAwardScreen&&!app.mCreditScreen&&!app.mChallengeScreen,"Award fixture requires an empty application route");
    app.mReanimatorCache->PrebuildCacheFrames();
    bool allPassed=true;
    constexpr std::array<unsigned long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
    auto& render=*app.mDDInterface->mRenderInterface;
    struct Target {Image* image;~Target(){delete image;}} target{render.CreateRenderImage(1400,600)};
    for(const bool replay:{false,true})
    for(int level=1;level<=50;++level)
    for(const int hz:{100,30,240,0})
    {
        PlayerInfo profile;ZenGarden garden;PoolEffect pool;pool.PoolEffectInitialize();
        profile.mId=49;profile.mLevel=level;profile.mFinishedAdventure=replay?1:0;
        profile.mCoins=99999;
        std::fill(std::begin(profile.mEarnedAchievements),std::end(profile.mEarnedAchievements),true);
        std::fill(std::begin(profile.mShownAchievements),std::end(profile.mShownAchievements),true);
        app.mPlayerInfo=&profile;app.mZenGarden=&garden;app.mPoolEffect=&pool;
        app.mScreenBounds=Rect(-300,0,1400,600);clocks.mVisualTimeNs=123'456'789'000;clocks.mBattleTimeNs=0;
        Sexy::SRand(0xA7A00+level);Sexy::gLegacyTickPhaseNs=0;
        int sequence=0,sawAlmanac=0,sawStore=-1,storeClicks=0;
        unsigned long long nestedElapsed=0;
        const auto nextStep=[&]() {return hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()];};
        const auto draw=[&](Widget& widget) {
            GpuImageDrawScope scope(render,target.image);Graphics graphics(target.image);
            graphics.Translate(widget.mX-app.mScreenBounds.mX,widget.mY-app.mScreenBounds.mY);widget.Draw(&graphics);
        };
        app.fixtureNestedPump=[&](bool* updated) {
            const auto step=nextStep();nestedElapsed+=step;
            Require(nestedElapsed<20'000'000'000ULL,"Award nested modal failed to finish within twenty simulated seconds");
            if(auto* almanac=static_cast<AlmanacDialog*>(app.GetDialog(DIALOG_ALMANAC))) {
                ++sawAlmanac;draw(*almanac);
                const auto position=almanac->GetAbsPos();
                const int x=almanac->mCloseButton->mX+almanac->mCloseButton->mWidth/2;
                const int y=almanac->mCloseButton->mY+almanac->mCloseButton->mHeight/2;
                app.mWidgetManager->mLastMouseX=position.mX+x;app.mWidgetManager->mLastMouseY=position.mY+y;
                almanac->MouseUp(x,y,1);
            }
            else if(auto* store=static_cast<StoreScreen*>(app.GetDialog(DIALOG_STORE))) {
                if(sawStore==-1) sawStore=store->mStartDialog;
                if(!store->mDrawnOnce) draw(*store);
                Sexy::ApplySimulationStep(step);app.UpdateFrames();
                if(store->mBubbleClickToContinue) {store->KeyChar(' ');++storeClicks;}
                else if(store->CanInteractWithButtons()) store->ButtonDepress(store->mBackButton->mId);
            }
            else throw std::runtime_error("Award waited on an unexpected modal");
            if(updated) *updated=true;
            app.ProcessSafeDeleteList();return true;
        };
        try
        {
            std::cout<<"Award begin level="<<level<<" replay="<<replay<<" Hz="<<hz<<'\n';
            app.PreNewGame(GAMEMODE_ADVENTURE,false);
            app.KillSeedChooserScreen();app.mGameScene=SCENE_PLAYING;
            app.mBoard->mLevelComplete=true;
            app.CheckForGameEnd();app.ProcessSafeDeleteList();
            Require(profile.mLevel==(level==50?1:level+1)&&profile.mFinishedAdventure==(replay?1:0)+(level==50),
                "Adventure completion did not advance the profile exactly once");
            const bool expectAward=!replay||level==50||level%10==9;
            Require(bool(app.mAwardScreen)==expectAward,"Adventure completion chose the wrong award route");
            const auto continueAward=[&]() {
                auto* award=app.mAwardScreen;Require(award!=nullptr,"Award route lost its screen");
                unsigned long long elapsed=0;
                while(elapsed<1'900'000'000ULL) {
                    const auto step=std::min<unsigned long long>(nextStep(),1'900'000'000ULL-elapsed);
                    Sexy::ApplySimulationStep(step);app.UpdateFrames();elapsed+=step;
                }
                Require(award->mFadeInCounter==0&&!award->mShowingAchievements,"Controlled award failed to finish its normal fade");
                draw(*award);
                {WidgetLifetimeHold hold(*award);award->StartButtonPressed();}
                app.ProcessSafeDeleteList();
            };
            if(expectAward) continueAward();
            if(level==50)
            {
                if(app.mAwardScreen) {
                    Require(!replay&&app.mAwardScreen->mAwardType==AWARD_CREDITS_ZOMBIENOTE,"First completion did not show its Credits note");
                    continueAward();
                }
                Require(app.mGameScene==SCENE_CREDIT&&app.mCreditScreen&&!app.mBoard,"Final Adventure award did not open Credits");
                draw(*app.mCreditScreen);
            }
            else if(!replay&&level==44)
                Require(app.mGameMode==GAMEMODE_LIMBO_ZEN_GARDEN&&app.mBoard&&app.mBoard->mBackground==BACKGROUND_GREENHOUSE&&
                    app.mCrazyDaveMessageIndex==2100,"Watering-can award did not enter the Garden tutorial");
            else
                Require(app.mGameMode==GAMEMODE_ADVENTURE&&app.mBoard&&app.mBoard->mLevel==level+1&&
                    app.mGameScene==SCENE_LEVEL_INTRO,"Adventure award did not enter the next level");
            const int expectedStore=!replay?(level==24?301:level==34?601:level==41?3100:-1):-1;
            Require(sawAlmanac==int(!replay&&level==14)&&sawStore==expectedStore,
                "Adventure award omitted or invented an Almanac/Store introduction");
            if(expectedStore!=-1) Require(storeClicks>0,"Award Store intro bypassed its real click-through dialogue");
            std::cout<<"Award primary level="<<level<<" replay="<<replay<<" Hz="<<hz<<" next="<<profile.mLevel
                <<" scene="<<app.mGameScene<<" almanac="<<sawAlmanac<<" store="<<sawStore<<" clicks="<<storeClicks<<'\n';
        }
        catch(const std::exception& error) {
            allPassed=false;std::cerr<<"Award failed level="<<level<<" replay="<<replay<<" Hz="<<hz<<" message="<<error.what()<<'\n';
        }
        app.fixtureNestedPump={};app.KillAlmanacDialog();app.KillStoreScreen();
        app.KillAwardScreen();app.KillCreditScreen();app.KillChallengeScreen();app.KillGameSelector();
        app.KillBoard();app.ProcessSafeDeleteList();
    }
    Require(allPassed,"Award routes have failed cases; inspect per-case output");
    std::cout<<"PASS actual Adventure award routes: fifty levels/first/replay/four schedules, nested Almanac/Store inputs, Garden tutorial and Credits\n";
}

void LateLifecycleRuntime(TestApp& app)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        PlayerInfo* player=app.mPlayerInfo;
        ZenGarden* garden=app.mZenGarden;
        PoolEffect* pool=app.mPoolEffect;
        GameMode mode=app.mGameMode;
        GameScenes scene=app.mGameScene;
        Rect bounds=app.mScreenBounds;
        BoardResult result=app.mBoardResult;
        unsigned long long interval=Sexy::gUpdateIntervalNs,phase=Sexy::gLegacyTickPhaseNs;
        int whole=Sexy::gWholeLegacyTicks;
        double ticks=Sexy::gUpdateTicks,steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        void Close() {
            app.fixtureNestedPump={};app.KillDialog(DIALOG_ZEN_SELL);
            app.KillBoard();app.ProcessSafeDeleteList();
        }
        ~Restore() {
            Close();app.mPlayerInfo=player;app.mZenGarden=garden;app.mPoolEffect=pool;
            app.mGameMode=mode;app.mGameScene=scene;app.mScreenBounds=bounds;app.mBoardResult=result;
            Sexy::gUpdateIntervalNs=interval;Sexy::gLegacyTickPhaseNs=phase;Sexy::gWholeLegacyTicks=whole;
            Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
        }
    } restore{app};
    Require(!app.mBoard,"Lifecycle fixture requires an empty Board route");
    constexpr std::array<unsigned long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
    bool allPassed=true;
    for(const auto mode:{GAMEMODE_ADVENTURE,GAMEMODE_MINIGAME_FINAL_BOSS})
    for(const int hz:{100,30,240,0})
    {
        PlayerInfo profile;ZenGarden garden;PoolEffect pool;pool.PoolEffectInitialize();
        profile.mId=51;profile.mLevel=50;profile.mFinishedAdventure=mode==GAMEMODE_ADVENTURE?0:1;
        app.mPlayerInfo=&profile;app.mZenGarden=&garden;app.mPoolEffect=&pool;
        app.mGameMode=mode;app.mGameScene=SCENE_PLAYING;app.mBoardResult=BOARDRESULT_NONE;
        clocks.mVisualTimeNs=clocks.mBattleTimeNs=0;Sexy::gLegacyTickPhaseNs=0;Sexy::SRand(0xB05517);
        {
            Board board(&app);
            try {
                board.mLevel=50;board.mBackground=BACKGROUND_5_ROOF;
                board.mCurrentWave=6;board.mNumWaves=20;board.mTotalSpawnedWaves=0;
                board.mSeedBank->mNumPackets=0;
                for(int row=0;row<MAX_GRID_SIZE_Y;++row) {
                    board.mPlantRow[row]=row==5?PLANTROW_DIRT:PLANTROW_NORMAL;
                    board.mIceMinX[row]=1000;board.mIceTimer[row]=0;board.mIceParticleID[row]=PARTICLESYSTEMID_NULL;
                    if(row<5) for(int col=0;col<9;++col) board.NewPlant(col,row,SEED_WALLNUT,SEED_NONE)->mPlantHealth=1'000'000;
                }
                auto* boss=board.AddZombieInRow(ZOMBIE_BOSS,0,0);
                int stage=0;bool firstAttack=false,secondAttack=false,dying=false;
                for(int sequence=0;board.mLevelTimeNs<90'000'000'000ULL&&!board.HasLevelAwardDropped();++sequence) {
                    if(boss->mZombiePhase==PHASE_BOSS_IDLE) {
                        if(stage==0) {
                            const int desired=boss->mBodyMaxHealth*4/5-1;
                            boss->TakeDamage(board.mLevelTimeNs,clocks.mVisualTimeNs,board.mLevelTimeNs,boss->mBodyHealth-desired,0U);
                            Require(boss->GetBodyDamageIndex()==1,"Boss first damage boundary failed");stage=1;
                        } else if(stage==1&&firstAttack) {
                            const int desired=boss->mBodyMaxHealth/2-1;
                            boss->TakeDamage(board.mLevelTimeNs,clocks.mVisualTimeNs,board.mLevelTimeNs,boss->mBodyHealth-desired,0U);
                            Require(boss->GetBodyDamageIndex()==2,"Boss second damage boundary failed");stage=2;
                        } else if(stage==2&&secondAttack) {
                            boss->TakeDamage(board.mLevelTimeNs,clocks.mVisualTimeNs,board.mLevelTimeNs,boss->mBodyHealth+100,0U);
                            Require(boss->mBodyHealth==1,"Boss lethal damage bypassed its one-health death sentinel");stage=3;
                        }
                    }
                    const auto step=hz?1'000'000'000ULL/hz:jitter[sequence%jitter.size()];
                    Sexy::ApplySimulationStep(step);board.mLevelTimeNs+=step;
                    clocks.mVisualTimeNs+=step;clocks.mBattleTimeNs=board.mLevelTimeNs;
                    app.mEffectSystem->UpdateAt(clocks.mVisualTimeNs,clocks.mBattleTimeNs);
                    board.UpdateGameObjects(step);
                    firstAttack|=stage==1&&boss->mBossMode==1&&boss->mZombiePhase==PHASE_BOSS_BUNGEES_ENTER;
                    secondAttack|=stage==2&&boss->mBossMode==2&&boss->mZombiePhase==PHASE_BOSS_DROP_RV;
                    dying|=boss->mZombiePhase==PHASE_ZOMBIE_DYING;
                    app.mEffectSystem->ProcessDeleteQueue();
                }
                int awards=0;Coin* award=nullptr;
                while(board.IterateCoins(award)) if(award->IsLevelAward()) ++awards;
                Require(stage==3&&firstAttack&&secondAttack&&dying&&board.HasLevelAwardDropped()&&
                    app.mBoardResult==BOARDRESULT_WON&&awards==1,
                    "Boss damage stages/death did not produce exactly one real level award");
                std::cout<<"Lifecycle boss mode="<<int(mode)<<" Hz="<<hz<<" stage="<<stage<<" bungee="<<firstAttack
                    <<" rv="<<secondAttack<<" dying="<<dying<<" awards="<<awards<<" elapsedNs="<<board.mLevelTimeNs<<" ok=1\n";
            } catch(const std::exception& error) {
                allPassed=false;std::cerr<<"Lifecycle boss failed mode="<<int(mode)<<" Hz="<<hz<<" message="<<error.what()<<'\n';
            }
        }
        app.mBoard=nullptr;
    }
    for(const int hz:{100,30,240,0})
    for(const bool accept:{false,true})
    {
        PlayerInfo profile;ZenGarden garden;PoolEffect pool;pool.PoolEffectInitialize();
        profile.mId=52;profile.mLevel=50;profile.mFinishedAdventure=1;profile.mCoins=1000;
        profile.mNumPottedPlants=3;
        for(int index=0;index<3;++index) {
            auto& spec=profile.mPottedPlant[index];spec.InitializePottedPlant(index==1?SEED_SUNFLOWER:SEED_PEASHOOTER);
            spec.mWhichZenGarden=GARDEN_MAIN;spec.mPlantAge=PLANTAGE_FULL;spec.mX=index+1;spec.mY=1;
            spec.mLastNeedFulfilledTime=garden.mFrameTime;
        }
        app.mPlayerInfo=&profile;app.mZenGarden=&garden;app.mPoolEffect=&pool;
        clocks.mVisualTimeNs=123'456'789'000;clocks.mBattleTimeNs=0;Sexy::gLegacyTickPhaseNs=0;
        app.mScreenBounds=Rect(-300,0,1400,600);Sexy::SRand(0x5A1E);
        int confirmations=0;unsigned long long wait=0;
        app.fixtureNestedPump=[&](bool* updated) {
            auto* dialog=static_cast<LawnDialog*>(app.GetDialog(DIALOG_ZEN_SELL));
            Require(dialog!=nullptr,"Garden sale waited on an unexpected dialog");
            Require(++confirmations<10,"Garden sale confirmation did not close");
            dialog->ButtonDepress(accept?Dialog::ID_YES:Dialog::ID_NO);
            const auto step=hz?1'000'000'000ULL/hz:jitter[confirmations%jitter.size()];
            wait+=step;Sexy::ApplySimulationStep(step);app.UpdateFrames();
            if(updated) *updated=true;
            app.ProcessSafeDeleteList();return true;
        };
        try {
            app.PreNewGame(GAMEMODE_LIMBO_ZEN_GARDEN,false);
            app.KillSeedChooserScreen();app.StartPlaying();
            auto& board=*app.mBoard;
            Plant* sale=nullptr;Plant* item=nullptr;
            while(board.IteratePlants(item)) if(item->mPottedPlantIndex==1) sale=item;
            Require(sale!=nullptr,"Garden sale target is not instantiated");
            const auto id=board.mPlants.DataArrayGetID(sale);
            const int price=garden.GetPlantSellPrice(sale);
            garden.MouseDownWithMoneySign(sale);
            board.ProcessDeleteQueue();
            Require(confirmations==1&&!app.GetDialog(DIALOG_ZEN_SELL),"Garden sale did not use one real confirmation");
            Require(profile.mCoins==1000+int(accept)*price&&profile.mNumPottedPlants==3-int(accept),
                "Garden sale charged the wrong balance or removed the wrong number of records");
            if(accept) {
                Require(!board.mPlants.DataArrayTryToGet(id),"Sold Garden plant was not released");
                Require(profile.mPottedPlant[1].mSeedType==SEED_PEASHOOTER,"Garden sale did not compact the surviving record");
            } else Require(board.mPlants.DataArrayTryToGet(id)&&profile.mPottedPlant[1].mSeedType==SEED_SUNFLOWER,
                "Declining the sale changed its target");
            int live=0,pots=0;item=nullptr;
            while(board.IteratePlants(item)) {
                if(item->mPottedPlantIndex==-1) {
                    Require(item->mSeedType==SEED_FLOWERPOT,"Garden has an unexpected unowned live plant");
                    ++pots;continue;
                }
                Require(item->mPottedPlantIndex>=0&&item->mPottedPlantIndex<profile.mNumPottedPlants,
                    "Garden sale left a surviving plant with an invalid record index");
                ++live;
            }
            Require(live==profile.mNumPottedPlants&&pots==live,"Garden record, plant and supporting pot counts disagree after sale");
            std::cout<<"Lifecycle sale Hz="<<hz<<" accept="<<accept<<" price="<<price<<" coins="<<profile.mCoins
                <<" records="<<profile.mNumPottedPlants<<" live="<<live<<" pots="<<pots<<" waitNs="<<wait<<" ok=1\n";
        } catch(const std::exception& error) {
            allPassed=false;std::cerr<<"Lifecycle sale failed Hz="<<hz<<" accept="<<accept<<" message="<<error.what()<<'\n';
        }
        restore.Close();
    }
    Require(allPassed,"Late lifecycle routes have failed cases; inspect per-case output");
    std::cout<<"PASS actual late lifecycle: eight Boss damage/death routes and eight real Garden sale confirmations\n";
}

void GraveSpawnRulesRuntime(TestApp& app)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        PlayerInfo* player=app.mPlayerInfo;
        ZenGarden* garden=app.mZenGarden;
        PoolEffect* pool=app.mPoolEffect;
        GameMode mode=app.mGameMode;
        GameScenes scene=app.mGameScene;
        Rect bounds=app.mScreenBounds;
        BoardResult result=app.mBoardResult;
        unsigned long long interval=Sexy::gUpdateIntervalNs,phase=Sexy::gLegacyTickPhaseNs;
        int whole=Sexy::gWholeLegacyTicks;
        double ticks=Sexy::gUpdateTicks,steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        void Close(){app.KillBoard();app.ProcessSafeDeleteList();}
        ~Restore(){Close();app.mPlayerInfo=player;app.mZenGarden=garden;app.mPoolEffect=pool;
            app.mGameMode=mode;app.mGameScene=scene;app.mScreenBounds=bounds;app.mBoardResult=result;
            Sexy::gUpdateIntervalNs=interval;Sexy::gLegacyTickPhaseNs=phase;Sexy::gWholeLegacyTicks=whole;
            Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;auto& clocks=*app.mEffectSystem->mReanimationHolder;
            clocks.mVisualTimeNs=visual;clocks.mBattleTimeNs=battle;}
    } restore{app};
    Require(!app.mBoard,"Grave rules fixture requires an empty application Board route");
    constexpr std::array<unsigned long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
    bool passed=true;
    for(const auto mode:{GAMEMODE_ADVENTURE,GAMEMODE_SURVIVAL_NORMAL_STAGE_2,
        GAMEMODE_SURVIVAL_HARD_STAGE_2,GAMEMODE_SURVIVAL_ENDLESS_STAGE_2})
    for(int hz:{100,30,240,0})
    for(int variant=0;variant<(mode==GAMEMODE_ADVENTURE?4:3);++variant)
    {
        PlayerInfo profile;ZenGarden garden;PoolEffect pool;pool.PoolEffectInitialize();
        profile.mId=53;profile.mLevel=mode==GAMEMODE_ADVENTURE?std::array{11,14,16,20}[variant]:11;
        profile.mFinishedAdventure=1;
        app.mPlayerInfo=&profile;app.mZenGarden=&garden;app.mPoolEffect=&pool;
        clocks.mVisualTimeNs=123'456'789'000;clocks.mBattleTimeNs=0;Sexy::gLegacyTickPhaseNs=0;Sexy::SRand(0x6A8E+variant);
        try {
            app.PreNewGame(mode,false);app.KillSeedChooserScreen();app.StartPlaying();
            auto& board=*app.mBoard;
            Require(board.mBackground==BACKGROUND_2_NIGHT&&board.mEnableGraveStones,"Grave mode did not enter a live night battle");
            const int limit=mode==GAMEMODE_SURVIVAL_NORMAL_STAGE_2?8:12;
            if(mode!=GAMEMODE_ADVENTURE) {
                board.mGridItems.DataArrayFreeAll();
                const int requested=variant==0?0:limit-2+variant;
                for(int col=4;col<9&&board.GetGraveStonesCount()<requested;++col)
                    for(int row=0;row<5&&board.GetGraveStonesCount()<requested;++row) board.AddAGraveStone(col,row);
                Require(board.GetGraveStonesCount()==requested,"Grave fixture could not establish its intended limit boundary");
            }
            const int initial=board.GetGraveStonesCount();
            board.mCurrentWave=0;board.SpawnZombieWave();
            Require(board.GetGraveStonesCount()==initial,"An ordinary early wave unexpectedly grew a grave");
            board.RemoveAllZombies();board.ProcessDeleteQueue();
            board.mCurrentWave=board.mNumWaves-1;board.SpawnZombieWave();
            const int expected=initial+int(mode!=GAMEMODE_ADVENTURE&&initial<limit);
            Require(board.GetGraveStonesCount()==expected,"Final night wave grave count differs from the retail rule");
            if(expected>initial) {
                GridItem* grave=nullptr;std::int64_t earliest=INT64_MAX;
                while(board.IterateGridItems(grave)) if(grave->mGridItemType==GRIDITEM_GRAVESTONE)
                    earliest=std::min(earliest,grave->mStateStartTimeNs);
                Require(earliest>=static_cast<std::int64_t>(board.mLevelTimeNs),"Battle grave was initialized as already risen");
            }
            std::cout<<"Grave wave mode="<<int(mode)<<" level="<<profile.mLevel<<" Hz="<<hz<<" initial="<<initial
                <<" final="<<board.GetGraveStonesCount()<<" limit="<<(mode==GAMEMODE_ADVENTURE?0:limit)<<" ok=1\n";
        } catch(const std::exception& error){passed=false;std::cerr<<"Grave wave failed mode="<<int(mode)<<" Hz="<<hz<<" message="<<error.what()<<'\n';}
        restore.Close();
    }
    for(const auto mode:{GAMEMODE_ADVENTURE,GAMEMODE_MINIGAME_WHACK_A_ZOMBIE})
    for(int hz:{100,30,240,0})
    for(bool depleted:{false,true})
    {
        PlayerInfo profile;ZenGarden garden;PoolEffect pool;pool.PoolEffectInitialize();
        profile.mId=54;profile.mLevel=15;profile.mFinishedAdventure=1;
        app.mPlayerInfo=&profile;app.mZenGarden=&garden;app.mPoolEffect=&pool;
        clocks.mVisualTimeNs=123'456'789'000;clocks.mBattleTimeNs=0;Sexy::gLegacyTickPhaseNs=0;Sexy::SRand(0x6A8E5);
        try {
            app.PreNewGame(mode,false);app.KillSeedChooserScreen();app.StartPlaying();
            auto& board=*app.mBoard;
            Require(board.GetGraveStonesCount()==9,"Whack initial grave count differs from retail");
            if(depleted) {
                int keep=0;GridItem* grave=nullptr;
                while(board.IterateGridItems(grave)) if(grave->mGridItemType==GRIDITEM_GRAVESTONE&&++keep>2) grave->mDead=true;
                board.ProcessDeleteQueue();
            }
            const int initial=board.GetGraveStonesCount();
            Require(initial==(depleted?2:9),"Whack grave-removal fixture failed");
            std::vector<unsigned long long> events;std::vector<int> counts;
            int previous=initial,sequence=0;
            while(board.mLevelTimeNs<62'000'000'000ULL) {
                const auto step=std::min<unsigned long long>(hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()],62'000'000'000ULL-board.mLevelTimeNs);
                Sexy::ApplySimulationStep(step);clocks.mVisualTimeNs+=step;board.Update();
                board.RemoveAllZombies();board.ProcessDeleteQueue();app.mEffectSystem->ProcessDeleteQueue();
                const int count=board.GetGraveStonesCount();
                if(count!=previous) {events.push_back(board.mLevelTimeNs);counts.push_back(count);previous=count;}
                Require(app.mGameScene==SCENE_PLAYING,"Whack timing fixture unexpectedly left battle");
            }
            Require(events.size()==3,"Whack did not grow graves three times during its first 62 seconds");
            const unsigned long long tolerance=(hz?1'000'000'000ULL/hz:32'333'333)*4;
            for(int event=0;event<3;++event) {
                const unsigned long long expected=21'000'000'000ULL+event*20'000'000'000ULL;
                Require(events[event]>=expected&&events[event]-expected<=tolerance,"Whack grave growth does not follow the retail 21/41/61-second schedule");
                Require(counts[event]==(depleted?5:10)+event,"Whack did not refill to five, then add one per growth event");
                std::cout<<"Grave whack mode="<<int(mode)<<" Hz="<<hz<<" depleted="<<depleted<<" event="<<event
                    <<" timeNs="<<events[event]<<" count="<<counts[event]<<" ok=1\n";
            }
        } catch(const std::exception& error){passed=false;std::cerr<<"Grave whack failed mode="<<int(mode)<<" Hz="<<hz<<" depleted="<<depleted<<" message="<<error.what()<<'\n';}
        restore.Close();
    }
    Require(passed,"Grave generation rules have failed scenarios; inspect per-case output");
    std::cout<<"PASS actual grave generation: 52 night wave boundaries and 16 Whack 62-second timelines at four schedules\n";
}

void AtlasTransparentEdges(TestApp& app)
{
    auto& render=*app.mDDInterface->mRenderInterface;
    auto* decoded=ImageLib::GetImage("compiled/reanim/gravebuster.atlas.png",false);
    Require(decoded && decoded->mBits && !decoded->HasComposition(),"Cannot decode Gravebuster atlas");
    Image reference,raw;
    reference.SetBits(decoded->mBits,decoded->mWidth,decoded->mHeight);
    raw.SetBits(decoded->mBits,decoded->mWidth,decoded->mHeight);
    reference.mGpuSamplingFlags|=GpuVertex::kAlphaEdgeFilter;
    const TextureRegion region{0,0,1,1,decoded->mWidth,decoded->mHeight};
    const TextureSource uploaded{render.UploadAtlasTexture(decoded),0};
    Require(!decoded,"Atlas retained decoded pixels after upload");
    struct Target{Image* image;~Target(){delete image;}} target{render.CreateRenderImage(769,677)};
    bool passed=true;unsigned changed=0;
    for(bool linear:{false,true})
    for(float offset:{0.f,.375f})
    {
        SexyMatrix3 transform;transform.LoadIdentity();
        transform.m00=6.125f;transform.m01=0;transform.m02=7.f+offset;
        transform.m10=0;transform.m11=5.375f;transform.m12=8.f+offset;
        const auto capture=[&](const TextureSource& source) {
            {
                GpuImageDrawScope destination(render,target.image);
                // The scene uses independent straight-RGB/alpha sampling.
                // A generated render target normally premultiplies each tap;
                // request scene semantics explicitly for this readback fixture.
                GpuImageSamplingScope sampling(render,GpuVertex::kIndependentColorFilter);
                Graphics graphics(target.image);
                graphics.mColor=Color(35,71,109);graphics.FillRect(0,0,769,677);
                render.BltTransformed(source,nullptr,Color::White,Graphics::DRAWMODE_NORMAL,region,transform,linear);
            }
            return render.ReadImagePixels(*target.image).pixels;
        };
        const auto original=capture({render.CreateImageTexture(&raw),0});
        const auto expected=linear ? capture({render.CreateImageTexture(&reference),reference.mGpuSamplingFlags}) : original;
        const auto actual=capture(uploaded);
        int maximum=0;
        for(std::size_t i=0;i<actual.size();++i) {
            changed+=actual[i]!=original[i];
            for(unsigned shift:{0u,8u,16u,24u})
                maximum=std::max(maximum,std::abs(int((actual[i]>>shift)&255)-int((expected[i]>>shift)&255)));
        }
        passed&=maximum<=2;
        std::cout<<"Atlas transparent edges linear="<<linear<<" offset="<<offset<<" maxError="<<maximum<<'\n';
        if(linear && offset!=0) {
            Require(ImageLib::WritePNGImage("gravebuster-atlas-raw.png",769,677,original) &&
                ImageLib::WritePNGImage("gravebuster-atlas-repaired.png",769,677,actual),"Cannot write atlas edge comparison");
        }
    }
    Require(passed,"Atlas upload leaked transparent RGB instead of matching the legacy edge repair");
    Require(changed>0,"Atlas edge fixture did not exercise transparent RGB interpolation");
    std::cout<<"PASS atlas edge repair: actual Gravebuster PNG, nearest/linear, integral/fractional, legacy shader oracle, upload-discard\n";
}

void HudAndGraveSeamsRuntime(TestApp& app)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;PlayerInfo* player=app.mPlayerInfo;ZenGarden* garden=app.mZenGarden;PoolEffect* pool=app.mPoolEffect;
        GameMode mode=app.mGameMode;GameScenes scene=app.mGameScene;Rect bounds=app.mScreenBounds;BoardResult result=app.mBoardResult;
        unsigned long long interval=Sexy::gUpdateIntervalNs,phase=Sexy::gLegacyTickPhaseNs;
        int whole=Sexy::gWholeLegacyTicks;double ticks=Sexy::gUpdateTicks,steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs,battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        void Close(){app.KillBoard();app.ProcessSafeDeleteList();}
        ~Restore(){Close();app.mPlayerInfo=player;app.mZenGarden=garden;app.mPoolEffect=pool;app.mGameMode=mode;app.mGameScene=scene;
            app.mScreenBounds=bounds;app.mBoardResult=result;Sexy::gUpdateIntervalNs=interval;Sexy::gLegacyTickPhaseNs=phase;
            Sexy::gWholeLegacyTicks=whole;Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
            auto& c=*app.mEffectSystem->mReanimationHolder;c.mVisualTimeNs=visual;c.mBattleTimeNs=battle;}
    } restore{app};
    Require(!app.mBoard,"HUD seam fixture requires an empty Board route");
    PlayerInfo profile;ZenGarden garden;PoolEffect pool;pool.PoolEffectInitialize();
    profile.mId=56;profile.mLevel=11;profile.mFinishedAdventure=1;
    app.mPlayerInfo=&profile;app.mZenGarden=&garden;app.mPoolEffect=&pool;
    auto& render=*app.mDDInterface->mRenderInterface;
    struct Target{Image* image;~Target(){delete image;}} slotTarget{render.CreateRenderImage(1400,600)},gallery{render.CreateRenderImage(1280,480)};
    bool passed=true;std::array<std::vector<unsigned int>,2> references;
    for(const Rect bounds:{Rect(0,0,800,600),Rect(0,0,1400,600),Rect(-300,0,1400,600)})
    for(int bankY:{0,-40})
    for(int boardX:{0,-160,120})
    {
        app.mScreenBounds=Rect(0,0,800,600);clocks.mVisualTimeNs=123'456'789'000;clocks.mBattleTimeNs=0;
        app.PreNewGame(GAMEMODE_MINIGAME_SLOT_MACHINE,false);app.KillSeedChooserScreen();app.StartPlaying();
        app.mScreenBounds=bounds;auto& board=*app.mBoard;board.mX=boardX;board.mY=17;
        board.mSeedBank->mY=bankY;board.mChallenge->mSlotMachineRollCount=3;
        // Playing normally has camera X=0. For synthetic camera positions,
        // isolate the handle from unrelated seed-card hit regions.
        if(boardX!=0) board.mSeedBank->mNumPackets=0;
        {
            GpuImageDrawScope scope(render,slotTarget.image);Graphics graphics(slotTarget.image);
            graphics.mColor=Color(20,45,30);graphics.FillRect(0,0,1400,600);
            graphics.Translate(board.mX-bounds.mX,board.mY-bounds.mY);
            board.mChallenge->DrawSlotMachine(&graphics);
        }
        const auto pixels=render.ReadImagePixels(*slotTarget.image);
        auto& reference=references[bankY==0?0:1];
        if(reference.empty()) reference=pixels.pixels;
        const bool same=reference==pixels.pixels;
        const int x=500+bounds.mX-board.mX,y=std::max(5,bankY+40)+bounds.mY-board.mY;
        HitResult hit;board.MouseHitTest(x,y,&hit);
        const bool hits=hit.mObjectType==OBJECT_TYPE_SLOT_MACHINE_HANDLE;
        const bool tooltip=board.mChallenge->UpdateToolTip(x,y);
        const auto rect=board.mChallenge->SlotMachineGetHandleRect();
        passed&=same&&hits&&tooltip;
        std::cout<<"Slot HUD width="<<bounds.mWidth<<" left="<<bounds.mX<<" boardX="<<boardX<<" bankY="<<bankY
            <<" same="<<same<<" hit="<<hits<<" tooltip="<<tooltip<<" rectX="<<rect.mX<<'\n';
        if(hits) {
            board.mSunMoney=50;board.MouseDown(x,y,1);
            passed&=board.mSunMoney==25&&board.mChallenge->mChallengeState==STATECHALLENGE_SLOT_MACHINE_ROLLING;
        }
        restore.Close();
    }
    app.mScreenBounds=Rect(0,0,800,600);clocks.mVisualTimeNs=123'456'789'000;clocks.mBattleTimeNs=0;
    app.PreNewGame(GAMEMODE_ADVENTURE,false);app.KillSeedChooserScreen();app.StartPlaying();
    auto& board=*app.mBoard;board.mGridItems.DataArrayFreeAll();board.mZombieCountDown=100000;board.mSunCountDown=100000;
    auto* grave=board.AddAGraveStone(3,1);grave->mStateStartTimeNs=-1'000'000'000;
    auto* plant=board.NewPlant(3,1,SEED_GRAVEBUSTER,SEED_NONE);
    constexpr std::array<unsigned long long,24> samples{0,100'000'000,200'000'000,300'000'000,500'000'000,660'000'000,750'000'000,1'000'000'000,
        1'250'000'000,1'500'000'000,1'750'000'000,2'000'000'000,2'250'000'000,2'500'000'000,2'750'000'000,3'000'000'000,
        3'250'000'000,3'500'000'000,3'750'000'000,4'000'000'000,4'100'000'000,4'200'000'000,4'300'000'000,4'400'000'000};
    {
        GpuImageDrawScope scope(render,gallery.image);Graphics graphics(gallery.image);
        graphics.mColor=Color(45,90,45);graphics.FillRect(0,0,1280,480);
    }
    for(std::size_t index=0;index<samples.size();++index) {
        while(board.mLevelTimeNs<samples[index]) {
            const auto step=std::min<unsigned long long>(16'666'666,samples[index]-board.mLevelTimeNs);
            Sexy::ApplySimulationStep(step);clocks.mVisualTimeNs+=step;board.Update();board.ProcessDeleteQueue();app.mEffectSystem->ProcessDeleteQueue();
        }
        std::cout<<"Gravebuster seam frame="<<index<<" timeNs="<<board.mLevelTimeNs<<" state="<<int(plant->mState)
            <<" remainingNs="<<plant->mStateCountdown.mNanoseconds<<" frame="<<app.ReanimationGet(plant->mBodyReanimID)->mCurrentFrame<<'\n';
        GpuImageDrawScope scope(render,gallery.image);Graphics graphics(gallery.image);graphics.mLinearBlend=true;
        graphics.Translate(int(index%8)*160+40-board.GridToPixelX(3,1),int(index/8)*160+50-board.GridToPixelY(3,1));
        grave->DrawGraveStone(&graphics);
        const float x=graphics.mTransX,y=graphics.mTransY;
        if(plant->BeginDraw(&graphics)){plant->Draw(board.mLevelTimeNs,&graphics);plant->EndDraw(&graphics,x,y);}
    }
    const auto pixels=render.ReadImagePixels(*gallery.image);
    Require(ImageLib::WritePNGImage("gravebuster-seam-gallery.png",1280,480,pixels.pixels),"Cannot write Gravebuster seam gallery");
    restore.Close();
    Require(passed,"Slot HUD coordinates or image follow Board movement; inspect focused output");
    std::cout<<"PASS Slot HUD: eighteen camera/viewport/bank positions, fixed draw pixels, hit, tooltip and actual pull\n";
}

void LossRulesRuntime(TestApp& app)
{
    auto& clocks=*app.mEffectSystem->mReanimationHolder;
    struct Restore {
        TestApp& app;
        PlayerInfo* player=app.mPlayerInfo;ZenGarden* garden=app.mZenGarden;PoolEffect* pool=app.mPoolEffect;
        GameMode mode=app.mGameMode;GameScenes scene=app.mGameScene;BoardResult result=app.mBoardResult;
        Rect bounds=app.mScreenBounds;
        unsigned long long interval=Sexy::gUpdateIntervalNs,phase=Sexy::gLegacyTickPhaseNs;
        int whole=Sexy::gWholeLegacyTicks;
        double ticks=Sexy::gUpdateTicks,steps=Sexy::gStepsPerLegacyTick;
        std::int64_t visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
        std::int64_t battle=app.mEffectSystem->mReanimationHolder->mBattleTimeNs;
        ~Restore() {
            app.KillDialog(DIALOG_GAME_OVER);app.KillBoard();app.KillGameSelector();app.KillChallengeScreen();
            app.ProcessSafeDeleteList();
            app.mPlayerInfo=player;app.mZenGarden=garden;app.mPoolEffect=pool;
            app.mGameMode=mode;app.mGameScene=scene;app.mBoardResult=result;app.mScreenBounds=bounds;
            Sexy::gUpdateIntervalNs=interval;Sexy::gLegacyTickPhaseNs=phase;Sexy::gWholeLegacyTicks=whole;
            Sexy::gUpdateTicks=ticks;Sexy::gStepsPerLegacyTick=steps;
            app.mEffectSystem->mReanimationHolder->mVisualTimeNs=visual;
            app.mEffectSystem->mReanimationHolder->mBattleTimeNs=battle;
        }
    } restore{app};
    Require(!app.mBoard&&!app.mGameSelector&&!app.mChallengeScreen,"Loss fixture requires an empty application route");
    app.mReanimatorCache->PrebuildCacheFrames();
    struct Entry {GameMode mode;int level;bool cutscene;};
    constexpr Entry entries[]{
        {GAMEMODE_ADVENTURE,1,true},{GAMEMODE_ADVENTURE,11,true},{GAMEMODE_ADVENTURE,21,true},
        {GAMEMODE_ADVENTURE,31,true},{GAMEMODE_ADVENTURE,41,true},{GAMEMODE_ADVENTURE,50,true},
        {GAMEMODE_MINIGAME_POGO_PARTY,50,true},{GAMEMODE_SURVIVAL_NORMAL_STAGE_1,50,true},
        {GAMEMODE_SURVIVAL_ENDLESS_STAGE_3,50,true},
        {GAMEMODE_MINIGAME_ZOMBIQUARIUM,50,false},{GAMEMODE_MINIGAME_LAST_STAND,50,false},
        {GAMEMODE_PUZZLE_I_ZOMBIE_1,50,false},{GAMEMODE_PUZZLE_I_ZOMBIE_ENDLESS,50,false},
        {GAMEMODE_SCARY_POTTER_ENDLESS,50,false}};
    constexpr std::array<unsigned long long,7> jitter{1'234'567,18'765'433,4'000'003,32'333'333,7'891'011,5'432'109,11'111'111};
    auto& render=*app.mDDInterface->mRenderInterface;
    struct Target {Image* image;~Target(){delete image;}} target{render.CreateRenderImage(1400,600)};
    bool allPassed=true;int checked=0;
    for(const auto& entry:entries)
    for(const int width:{800,1400})
    for(const int hz:{100,30,240,0})
    for(const bool retry:{false,true})
    {
        PlayerInfo profile;ZenGarden garden;PoolEffect pool;pool.PoolEffectInitialize();
        profile.mId=51;profile.mLevel=entry.level;profile.mFinishedAdventure=1;
        app.mPlayerInfo=&profile;app.mZenGarden=&garden;app.mPoolEffect=&pool;
        app.mBoardResult=BOARDRESULT_NONE;app.mScreenBounds=Rect((800-width)/2,0,width,600);
        clocks.mVisualTimeNs=123'456'789'000;clocks.mBattleTimeNs=0;
        Sexy::gLegacyTickPhaseNs=0;Sexy::SRand(0x10550+entry.level);
        unsigned long long elapsed=0;int sequence=0;
        try
        {
            std::cout<<"Loss begin mode="<<int(entry.mode)<<" level="<<entry.level<<" width="<<width<<" Hz="<<hz<<" retry="<<retry<<'\n';
            app.PreNewGame(entry.mode,false);app.mBoard->RemoveCutsceneZombies();app.StartPlaying();
            auto* board=app.mBoard;board->mDrawCount=1;board->mLevelTimeNs=31'234'567'890ULL;clocks.mBattleTimeNs=board->mLevelTimeNs;
            Zombie* intruder=nullptr;
            if(entry.cutscene) {
                intruder=board->AddZombieInRow(ZOMBIE_NORMAL,1,0);intruder->mX=-120;intruder->mRootBindingGeneration=0;
            }
            board->ZombiesWon(intruder);
            const auto heldBattle=board->mLevelTimeNs;
            Require(app.mBoardResult==BOARDRESULT_LOST,"ZombiesWon did not record a loss");
            if(entry.cutscene) Require(app.mGameScene==SCENE_ZOMBIES_WON&&!app.GetDialog(DIALOG_GAME_OVER),"Loss cutscene was skipped or showed its dialog immediately");
            else Require(app.GetDialog(DIALOG_GAME_OVER)&&board->mPaused,"Direct loss did not show its immediate modal");
            ReanimationID lossAnimation=ReanimationID::REANIMATIONID_NULL;
            bool animated=false;std::int64_t firstPhase=0;
            while(!app.GetDialog(DIALOG_GAME_OVER)&&elapsed<12'000'000'000ULL)
            {
                const auto step=hz?1'000'000'000ULL/hz:jitter[sequence++%jitter.size()];
                Sexy::ApplySimulationStep(step);app.UpdateFrames();elapsed+=step;
                Require(board->mLevelTimeNs==heldBattle&&clocks.mBattleTimeNs==static_cast<std::int64_t>(heldBattle),"Loss movie advanced Board/Battle time");
                if(auto* movie=app.ReanimationTryToGet(board->mCutScene->mZombiesWonReanimID)) {
                    if(lossAnimation==ReanimationID::REANIMATIONID_NULL) {
                        lossAnimation=board->mCutScene->mZombiesWonReanimID;firstPhase=movie->SamplePlayback().phaseNs;
                    }
                    else {Require(lossAnimation==board->mCutScene->mZombiesWonReanimID,"Loss movie was created more than once");animated|=movie->SamplePlayback().phaseNs!=firstPhase;}
                }
                app.mEffectSystem->ProcessDeleteQueue();
            }
            auto* dialog=static_cast<GameOverDialog*>(app.GetDialog(DIALOG_GAME_OVER));
            Require(dialog&&board->mPaused,"Loss movie never reached the Game Over dialog");
            if(entry.cutscene) Require(elapsed>=11'000'000'000ULL&&elapsed<11'040'000'000ULL&&animated&&board->mCutScene->IsCutSceneOver(),
                "Loss movie timing or its visible animation did not complete");
            const auto heldVisual=clocks.mVisualTimeNs;
            for(int i=0;i<3;++i) {Sexy::ApplySimulationStep(33'345'678);app.UpdateFrames();}
            Require(app.GetDialog(DIALOG_GAME_OVER)==dialog&&board->mLevelTimeNs==heldBattle&&clocks.mVisualTimeNs==heldVisual,
                "Game Over duplicated its modal or advanced the frozen scene");
            {GpuImageDrawScope scope(render,target.image);Graphics graphics(target.image);
             graphics.Translate(-app.mScreenBounds.mX,0);board->Draw(&graphics);
             Graphics overlay(target.image);const auto point=dialog->GetAbsPos();overlay.Translate(point.mX-app.mScreenBounds.mX,point.mY);dialog->Draw(&overlay);}
            {WidgetLifetimeHold hold(*dialog);dialog->ButtonDepress(retry?Dialog::ID_FOOTER:1);}
            app.ProcessSafeDeleteList();
            Require(!app.GetDialog(DIALOG_GAME_OVER)&&profile.mLevel==entry.level,"Loss action retained its dialog or advanced Adventure progress");
            if(retry) Require(app.mBoard&&app.mBoard!=board&&app.mGameMode==entry.mode&&app.mGameScene==SCENE_LEVEL_INTRO&&
                app.mBoard->mLevelTimeNs==0&&app.mBoardResult==BOARDRESULT_NONE,"Retry did not create a clean replacement level");
            else if(entry.mode==GAMEMODE_ADVENTURE)
                Require(!app.mBoard&&app.mGameSelector&&app.mGameScene==SCENE_MENU,"Adventure loss did not return to the selector");
            else {
                const auto page=app.IsSurvivalMode()?CHALLENGE_PAGE_SURVIVAL:app.IsPuzzleMode()?CHALLENGE_PAGE_PUZZLE:CHALLENGE_PAGE_CHALLENGE;
                Require(!app.mBoard&&app.mChallengeScreen&&app.mChallengeScreen->mPageIndex==page,"Loss returned to a wrong challenge page");
            }
            ++checked;std::cout<<"Loss primary mode="<<int(entry.mode)<<" level="<<entry.level<<" width="<<width<<" Hz="<<hz
                <<" retry="<<retry<<" cutscene="<<entry.cutscene<<" movieNs="<<elapsed<<" ok=1\n";
        }
        catch(const std::exception& error) {
            allPassed=false;std::cerr<<"Loss failed mode="<<int(entry.mode)<<" level="<<entry.level<<" width="<<width<<" Hz="<<hz
                <<" retry="<<retry<<" elapsedNs="<<elapsed<<" message="<<error.what()<<'\n';
        }
        app.KillDialog(DIALOG_GAME_OVER);app.KillBoard();app.KillGameSelector();app.KillChallengeScreen();app.ProcessSafeDeleteList();
    }
    Require(allPassed&&checked==224,"Loss rules have failed cases or an incomplete catalog");
    std::cout<<"PASS actual loss routes: 224 cutscene/direct-modal, frozen clocks, visible movie, retry and menu contexts\n";
}


Image* gTouchFrameReference=nullptr;

bool TouchFrameMatches(const std::vector<unsigned int>& actual,const std::vector<unsigned int>& expected)
{
    if(actual.size()!=expected.size()) return false;
    int maximum=0;
    for(size_t i=0;i<actual.size();++i)
        for(int shift=0;shift<32;shift+=8)
            maximum=std::max(maximum,std::abs(int((actual[i]>>shift)&255)-int((expected[i]>>shift)&255)));
    std::cout<<"Frame atlas/reference maximum channel difference="<<maximum<<'\n';
#if defined(PVZ_GPU_OPENGL)
    return maximum<=1; // Repacked normalized UVs can round one RGBA8 unit in the GL sampler.
#else
    return maximum==0;
#endif
}

void TouchPresentationRuntime(TestApp& app, Board& board)
{
    app.mReanimatorCache->PrebuildCacheFrames();
    auto& render=*app.mDDInterface->mRenderInterface;
    auto& manager=*app.mWidgetManager;
    auto& contact=*board.mTouchState->selection;
    auto& cursor=*contact.cursor;
    const auto savedType=cursor.mCursorType;
    const auto savedBackground=board.mBackground;
    Require(gTouchFrameReference&&gTouchFrameReference->mWidth==104&&
        gTouchFrameReference->mHeight==144,"Standalone TV selection frame is missing from the actual PAK");
    for(const int width:{800,1400}) {
        app.mScreenBounds=Rect((800-width)/2,0,width,600);manager.Resize(app.mScreenBounds,app.mScreenBounds);
        for(const bool shovel:{false,true}) {
                cursor.mCursorType=shovel?CURSOR_TYPE_SHOVEL:savedType;
                const auto& packet=board.mSeedBank->mSeedPackets[cursor.mSeedBankIndex];
                const Rect bounds=shovel?board.GetShovelButtonRect():Rect(app.mScreenBounds.mX+board.mSeedBank->mX+
                    packet.mX+packet.mOffsetX,board.mSeedBank->mY+packet.mY,packet.mWidth,packet.mHeight);
                std::unique_ptr<Image> actual(render.CreateRenderImage(width,600)),expected(render.CreateRenderImage(width,600));
                const auto draw=[&](Image* target,bool production) {
                    GpuImageDrawScope destination(render,target);Graphics g(target);
                    g.mColor=Color::Black;g.FillRect(0,0,width,600);g.Translate(-app.mScreenBounds.mX,0);
                    if(production) board.DrawTouchSelection(&g);
                    else {
                        for(int i=0;i<4;++i) {
                            const Rect source(i&1?62:0,i&2?108:0,42,36);
                            const Rect destination(i&1?bounds.mX+bounds.mWidth-18:bounds.mX-3,
                                i&2?bounds.mY+bounds.mHeight-15:bounds.mY-3,21,18);
                            g.DrawImage(gTouchFrameReference,destination,source);
                        }
                    }
                };
                draw(actual.get(),true);draw(expected.get(),false);render.Flush();
                const auto pixels=render.ReadImagePixels(*actual);
                Require(TouchFrameMatches(pixels.pixels,render.ReadImagePixels(*expected).pixels),
                    "Selection UVs or card/shovel/widescreen placement differ from independent pixel-crop reference");
                Require(std::count_if(pixels.pixels.begin(),pixels.pixels.end(),[](unsigned int p){return (p&255)>80;})>80,
                    "Retained selection did not draw visible blue corners");
                const auto name="touch-frame-"+std::to_string(width)+(shovel?"-shovel.png":"-card.png");
                Require(ImageLib::WritePNGImage(name,width,600,pixels.pixels),"Cannot write selection frame proof");
                // Exercise the actual bottom/top passes, with an opaque object
                // between them. A frame accidentally left in top UI will leak
                // blue pixels through this occluder.
                {
                    GpuImageDrawScope destination(render,actual.get());Graphics g(actual.get());
                    g.mColor=Color::Black;g.FillRect(0,0,width,600);g.Translate(-app.mScreenBounds.mX,0);
                    board.DrawUIBottom(&g);
                    g.mColor=Color(191,31,47);
                    g.FillRect(bounds.mX-5,bounds.mY-5,bounds.mWidth+10,bounds.mHeight+10);
                    board.DrawUITop(&g);
                }
                render.Flush();
                const auto occluded=render.ReadImagePixels(*actual);
                const auto expectedColor=0xffbf1f2fu;
                for(int py=std::max(0,bounds.mY-3);py<std::min(600,bounds.mY+bounds.mHeight+3);++py)
                    for(int px=std::max(0,bounds.mX-app.mScreenBounds.mX-3);
                        px<std::min(width,bounds.mX-app.mScreenBounds.mX+bounds.mWidth+3);++px)
                        Require(occluded.pixels.at(py*width+px)==expectedColor,
                            "Top UI redrew the card/tool frame over a later occluder");
            }
        }
        cursor.mCursorType=savedType;
        app.mScreenBounds=Rect(0,0,800,600);manager.Resize(app.mScreenBounds,app.mScreenBounds);
        for(const auto background:{BACKGROUND_1_DAY,BACKGROUND_3_POOL,BACKGROUND_5_ROOF}) {
            board.mBackground=background;
            const int col=background==BACKGROUND_5_ROOF?2:5,row=background==BACKGROUND_3_POOL?3:1;
            const int x=board.GridToPixelX(col,row),y=board.GridToPixelY(col,row);
            const int height=board.GridToPixelY(col,1)-board.GridToPixelY(col,0);
            const TouchEvent event{751,x+40,y+height/2};
            for(const bool shovel:{false,true}) {
            cursor.mCursorType=shovel?CURSOR_TYPE_SHOVEL:savedType;
            Plant* shovelPlant=shovel?board.NewPlant(col,row,SEED_PEASHOOTER,SEED_NONE):nullptr;
            manager.TouchBegan(event);board.UpdateTouchCursors();
            Require(contact.preview->mGridX==col&&contact.preview->mGridY==row,"Guide preview cached the wrong planting cell");
            contact.preview->mVisible=false; // Isolate the guides; invalid cells still show an intended region.
            std::unique_ptr<Image> target(render.CreateRenderImage(800,600));
            {
                GpuImageDrawScope destination(render,target.get());Graphics g(target.get());
                g.mColor=Color::Black;g.FillRect(0,0,800,600);board.DrawTouchPreviews(&g);
            }
            render.Flush();const auto pixels=render.ReadImagePixels(*target);
            const auto brightness=[&](int px,int py){return pixels.pixels.at(py*800+px)&255;};
            const bool roof=background==BACKGROUND_5_ROOF;
            const int inset=roof?30:5;
            const unsigned int vertical=brightness(x+5,board.GridToPixelY(col,0)+inset);
            const unsigned int horizontal=brightness(board.GridToPixelX(8,row)+5,board.GridToPixelY(8,row)+inset);
            const unsigned int overlap=brightness(x+5,y+inset);
            Require(std::abs(int(vertical)-64)<=1&&std::abs(int(horizontal)-64)<=1&&
                std::abs(int(overlap)-128)<=1&&brightness(10,590)==0,
                "Plant/shovel guides did not match additive white alpha 64 with a twice-bright target");
            if(roof) {
                const int slopeY=board.GridToPixelY(col,0)+10;
                Require(brightness(x+5,slopeY)==0&&brightness(x+75,slopeY)>60,
                    "Roof column top is a rectangle/staircase instead of following its slope");
                const int rowSlopeY=y+10;
                Require(brightness(x+85,rowSlopeY)>60,
                    "Roof row band has a gap instead of joining adjacent column heights");
            }
            const auto name="touch-guides-"+std::to_string(int(background))+(shovel?"-shovel.png":"-plant.png");
            Require(ImageLib::WritePNGImage(name,800,600,pixels.pixels),"Cannot write planting guide proof");
            // Check the real blend against a colored background; normal alpha
            // compositing cannot satisfy this reference even if black looks similar.
            {
                GpuImageDrawScope destination(render,target.get());Graphics g(target.get());
                g.mColor=Color(24,48,72,255);g.FillRect(0,0,800,600);board.DrawTouchPreviews(&g);
            }
            render.Flush();const auto colored=render.ReadImagePixels(*target);
            const unsigned int rgb=colored.pixels.at((y+inset)*800+x+5);
            Require(std::abs(int((rgb>>16)&255)-152)<=1&&std::abs(int((rgb>>8)&255)-176)<=1&&
                std::abs(int(rgb&255)-200)<=1,"Guide color did not add to the destination background");
            if(shovel) {
                manager.TouchMoved({event.id,board.GridToPixelX(8,4)+40,board.GridToPixelY(8,4)+40});
                board.UpdateTouchCursors();
                {
                    GpuImageDrawScope destination(render,target.get());Graphics g(target.get());
                    g.mColor=Color::Black;g.FillRect(0,0,800,600);board.DrawTouchPreviews(&g);
                }
                render.Flush();const auto noTarget=render.ReadImagePixels(*target);
                Require(std::all_of(noTarget.pixels.begin(),noTarget.pixels.end(),[](unsigned int p){return p==0xff000000u;}),
                    "Shovel guides remained over an empty planting cell");
            }
            manager.TouchCanceled(event);
            std::unique_ptr<Image> idle(render.CreateRenderImage(800,600));
            {
                GpuImageDrawScope destination(render,idle.get());Graphics g(idle.get());
                g.mColor=Color::Black;g.FillRect(0,0,800,600);board.DrawTouchPreviews(&g);
            }
            render.Flush();const auto inactive=render.ReadImagePixels(*idle);
            Require(std::all_of(inactive.pixels.begin(),inactive.pixels.end(),[](unsigned int p){return p==0xff000000u;}),
                "Planting guides remained after the gesture ended");
            if(shovelPlant) {
                shovelPlant->Die(board.mLevelTimeNs,app.mEffectSystem->mReanimationHolder->mVisualTimeNs,board.mLevelTimeNs);
                board.ProcessDeleteQueue();
            }
        }
        cursor.mCursorType=savedType;
    }
    board.mBackground=savedBackground;
    std::cout<<"PASS touch presentation: standalone blue corners, card/shovel 800/1400 widths, independent UV oracle, plant/shovel additive 64 bands, colored-background 128 target, sloping roof, empty-target/inactive suppression\n";
}

void TouchFieldCardsRuntime(TestApp& app, Board& board)
{
    auto& manager=*app.mWidgetManager;
    auto& render=*app.mDDInterface->mRenderInterface;
    const auto savedMode=app.mGameMode;
    const auto savedBackground=board.mBackground;
    const auto savedPacket=board.mSeedBank->mSeedPackets[0];
    const auto point=[&](Coin* coin,unsigned long long id=801){
        return TouchEvent{id,int(coin->mPosX)+25,int(coin->mPosY)+35};
    };
    const auto tap=[&](const TouchEvent& event){manager.TouchBegan(event);manager.TouchEnded(event);};
    const auto makeCard=[&](SeedType seed,int x,int y){
        auto* coin=board.AddCoin(x,y,COIN_USABLE_SEED_PACKET,COIN_MOTION_COIN);
        coin->mPosX=float(x);coin->mPosY=float(y);coin->mUsableSeedType=seed;
        return coin;
    };
    const auto coinId=[&](Coin* coin){return static_cast<CoinID>(board.mCoins.DataArrayGetID(coin));};
    for(const auto mode:{GAMEMODE_MINIGAME_RAINING_SEEDS,GAMEMODE_SCARY_POTTER_1}) {
        app.mGameMode=mode;
        board.mBackground=mode==GAMEMODE_SCARY_POTTER_1?BACKGROUND_2_NIGHT:BACKGROUND_1_DAY;
        board.ClearTouchSelection();board.RefreshSeedPacketFromCursor();
        const int initialPlants=board.mPlants.mSize;
        std::vector<PlantID> placed;
        auto* first=makeCard(SEED_SUNFLOWER,300,180);
        auto* second=makeCard(SEED_PEASHOOTER,500,180);
        const auto firstId=coinId(first),secondId=coinId(second);
        const auto a=point(first),b=point(second);
        auto& selected=*board.mTouchState->selection->cursor;
        manager.TouchBegan(a);manager.TouchEnded({a.id,a.x+4,a.y});
        Require(selected.mCoinID==firstId&&first->mIsBeingCollected&&board.mPlants.mSize==initialPlants,
            "Field-card tap/jitter planted at its source instead of retaining selection");
        tap(a);
        Require(selected.mCursorType==CURSOR_TYPE_NORMAL&&!first->mIsBeingCollected&&first->mTimesDropped==1&&
            board.mPlants.mSize==initialPlants,"Tapping the same field card did not return it exactly once");
        tap(a);tap(b);
        Require(!first->mIsBeingCollected&&second->mIsBeingCollected&&selected.mCoinID==secondId&&
            board.mPlants.mSize==initialPlants,"Tapping another field card planted the old card or lost its source");

        manager.TouchBegan(b);manager.TouchMoved({b.id,b.x+100,b.y+100});manager.TouchEnded(b);
        Require(selected.mCoinID==secondId&&second->mIsBeingCollected&&board.mPlants.mSize==initialPlants,
            "Dragging back onto the source card canceled or planted it");
        manager.TouchBegan(b);manager.TouchEnded({b.id,b.x+7,b.y});
        Require(selected.mCoinID==secondId&&board.mPlants.mSize==initialPlants,
            "A release inside the source card planted it after exceeding movement tolerance");

        tap(a);
        const TouchEvent target{a.id,board.GridToPixelX(7,2)+40,board.GridToPixelY(7,2)+40};
        manager.TouchBegan(a);manager.TouchMoved({a.id,b.x,b.y});
        manager.TouchBegan({802,b.x,b.y});manager.TouchEnded({802,b.x,b.y});
        Require(selected.mCoinID==firstId&&!second->mIsBeingCollected&&manager.mTouchCount==1,
            "Passing another card or a second finger replaced the shared field-card source");
        manager.TouchEnded(target);
        auto* plant=board.GetTopPlantAt(7,2,TOPPLANT_ONLY_NORMAL_POSITION);
        Require(plant&&plant->mSeedType==SEED_SUNFLOWER&&first->mDead&&!second->mIsBeingCollected&&
            board.mPlants.mSize==initialPlants+1,"Field-card drag did not consume exactly its captured source");
        placed.push_back(static_cast<PlantID>(board.mPlants.DataArrayGetID(plant)));

        manager.TouchBegan(b);manager.TouchEnded({b.id,-100,700});
        Require(selected.mCoinID==secondId&&second->mIsBeingCollected&&board.mPlants.mSize==initialPlants+1,
            "Failed field-card drop lost selection or planted outside the lawn");
        const TouchEvent lawn{803,board.GridToPixelX(6,4)+40,board.GridToPixelY(6,4)+40};
        tap(lawn);plant=board.GetTopPlantAt(6,4,TOPPLANT_ONLY_NORMAL_POSITION);
        Require(plant&&plant->mSeedType==SEED_PEASHOOTER&&second->mDead&&selected.mCursorType==CURSOR_TYPE_NORMAL,
            "A retained field card could not be planted by tapping the lawn");
        placed.push_back(static_cast<PlantID>(board.mPlants.DataArrayGetID(plant)));

        auto* third=makeCard(SEED_SUNFLOWER,420,300);
        const auto c=point(third);const auto thirdId=coinId(third);
        manager.MouseDown(c.x,c.y,1);manager.MouseUp(c.x,c.y,1);
        Require(board.mCursorObject->mCoinID==thirdId,"Mouse fixture did not hold its own field card");
        const int mouseX=manager.mLastMouseX,mouseY=manager.mLastMouseY;
        tap(c);
        Require(selected.mCursorType==CURSOR_TYPE_NORMAL&&board.mCursorObject->mCursorType==CURSOR_TYPE_NORMAL&&!third->mIsBeingCollected&&
            board.mPlants.mSize==initialPlants+2,"Touch stole a mouse-owned field card or committed another plant");
        board.RefreshSeedPacketFromCursor();
        auto& packet=board.mSeedBank->mSeedPackets[0];
        packet.mPacketType=SEED_PEASHOOTER;packet.mRefreshing=false;packet.mRefreshTime=0;packet.mActive=true;
        const TouchEvent bank{804,int(board.mSeedBank->mX+packet.mX)+packet.mOffsetX+20,int(board.mSeedBank->mY)+30};
        tap(bank);tap(c);
        Require(selected.mCoinID==thirdId&&packet.mActive,"Field-card pickup did not return the held bank card");
        tap(bank);
        Require(selected.mCursorType==CURSOR_TYPE_PLANT_FROM_BANK&&!third->mIsBeingCollected,
            "Bank-card pickup did not return the held field card");
        tap(c);
        const auto shovel=board.GetShovelButtonRect();
        tap({805,shovel.mX+shovel.mWidth/2,shovel.mY+shovel.mHeight/2});
        Require(selected.mCursorType==CURSOR_TYPE_SHOVEL&&!third->mIsBeingCollected,
            "Shovel pickup did not share selection with a field card");
        tap(c);
        Require(selected.mCoinID==thirdId&&manager.mLastMouseX==mouseX&&manager.mLastMouseY==mouseY,
            "Field-card pickup did not replace the shovel or changed physical mouse position");

        for(const int width:{800,1400}) {
            app.mScreenBounds=Rect((800-width)/2,0,width,600);manager.Resize(app.mScreenBounds,app.mScreenBounds);
            std::unique_ptr<Image> actual(render.CreateRenderImage(width,600)),expected(render.CreateRenderImage(width,600));
            const auto draw=[&](Image* image,bool production){
                GpuImageDrawScope destination(render,image);Graphics g(image);
                g.mColor=Color::Black;g.FillRect(0,0,width,600);g.Translate(37-app.mScreenBounds.mX,11);
                if(production) board.DrawFieldCardSelection(&g,*third);
                else for(int i=0;i<4;++i) {
                    const Rect src(i&1?62:0,i&2?108:0,42,36);
                    const Rect dst(int(third->mPosX)+(i&1?third->mWidth-18:-3),
                        int(third->mPosY)+(i&2?third->mHeight-15:-3),21,18);
                    g.DrawImage(gTouchFrameReference,dst,src);
                }
            };
            draw(actual.get(),true);draw(expected.get(),false);render.Flush();
            const auto pixels=render.ReadImagePixels(*actual);
            Require(TouchFrameMatches(pixels.pixels,render.ReadImagePixels(*expected).pixels)&&
                std::count_if(pixels.pixels.begin(),pixels.pixels.end(),[](unsigned int p){return (p&255)>80;})>80,
                "Field-card blue frame did not follow its source bounds under translation/widescreen");
            Require(ImageLib::WritePNGImage("touch-field-card-"+std::to_string(int(mode))+"-"+
                std::to_string(width)+".png",width,600,pixels.pixels),"Cannot write field-card frame proof");
        }
        app.mScreenBounds=Rect(0,0,800,600);manager.Resize(app.mScreenBounds,app.mScreenBounds);
        board.ClearTouchSelection();
        auto* bottom=makeCard(SEED_SUNFLOWER,550,400);
        auto* top=makeCard(SEED_WALLNUT,550,400);top->mRenderOrder=bottom->mRenderOrder+1;
        tap(point(top));
        Require(selected.mCoinID==coinId(top)&&top->mIsBeingCollected&&!bottom->mIsBeingCollected,
            "Overlapping field cards did not select the visible top card");
        tap(point(top));Require(selected.mCursorType==CURSOR_TYPE_NORMAL,"Top-card repeat tap did not cancel");

        if(mode==GAMEMODE_SCARY_POTTER_1) {
            tap(c);
            const auto makePot=[&](int col){
                auto* pot=board.mGridItems.DataArrayAlloc();pot->mGridItemType=GRIDITEM_SCARY_POT;
                pot->mGridItemState=GRIDITEM_STATE_SCARY_POT_QUESTION;pot->mGridX=col;pot->mGridY=0;
                pot->mScaryPotType=SCARYPOT_SEED;pot->mSeedType=SEED_SNOWPEA;
                pot->mRenderOrder=Board::MakeRenderOrder(RENDER_LAYER_PLANT,0,0);return pot;
            };
            auto* pot=makePot(8);auto* spare=makePot(6);
            const auto potId=board.mGridItems.DataArrayGetID(pot);
            const TouchEvent hit{806,board.GridToPixelX(8,0)+40,board.GridToPixelY(8,0)+40};
            manager.TouchBegan(hit);
            Require(board.mChallenge->mChallengeState==STATECHALLENGE_SCARY_POTTER_MALLETING&&
                selected.mCoinID==thirdId,"Touching a vase with a held card did not start only the vase action");
            for(int i=0;i<240&&!pot->mDead;++i) {
                board.mLevelTimeNs+=8'333'333;
                app.mEffectSystem->mReanimationHolder->mBattleTimeNs=board.mLevelTimeNs;
                if(auto* hammer=app.ReanimationTryToGet(board.mChallenge->mReanimChallenge)) hammer->UpdateAt(board.mLevelTimeNs);
                board.mChallenge->ScaryPotterUpdate();
            }
            Require(pot->mDead&&board.mChallenge->mChallengeState==STATECHALLENGE_NORMAL,
                "Actual vase hammer did not release its card while the touch stayed down");
            Coin* dropped=nullptr;Coin* coin=nullptr;
            while(board.IterateCoins(coin)) if(coin->mType==COIN_USABLE_SEED_PACKET&&coin->mUsableSeedType==SEED_SNOWPEA) dropped=coin;
            Require(dropped,"Actual vase did not produce its configured plant card");
            const auto end=point(dropped,hit.id);
            manager.TouchMoved(end);manager.TouchEnded(end);
            Require(selected.mCoinID==thirdId&&!dropped->mIsBeingCollected&&board.mPlants.mSize==initialPlants+2&&
                manager.mTouchCount==0,"Vase gesture selected its new card or planted the previous selection on release");
            tap(point(dropped));
            Require(selected.mCoinID==coinId(dropped)&&!third->mIsBeingCollected&&board.mPlants.mSize==initialPlants+2,
                "A fresh gesture could not select the card released by the vase");
            spare->GridItemDie();
            Require(board.mGridItems.DataArrayTryToGet(potId)==pot,"Vase fixture lost its stable grid-item allocation");
        }

        board.ClearTouchSelection();
        auto* stale=makeCard(SEED_PEASHOOTER,360,400);const auto oldId=coinId(stale);
        const auto stalePoint=point(stale);manager.TouchBegan(stalePoint);
        stale->Die();board.mCoins.DataArrayFree(stale);
        auto* replacement=makeCard(SEED_PEASHOOTER,360,400);
        manager.TouchEnded({stalePoint.id,740,540});
        Require(coinId(replacement)!=oldId&&selected.mCursorType==CURSOR_TYPE_NORMAL&&
            !replacement->mIsBeingCollected&&board.mPlants.mSize==initialPlants+2,
            "A reused Coin slot became the captured card or was planted by a stale gesture");
        tap(point(replacement));
        const auto save=(std::filesystem::current_path()/("touch-field-card-"+std::to_string(int(mode))+".dat64")).string();
        Require(LawnSaveGame(&board,save)&&!replacement->mIsBeingCollected&&selected.mCursorType==CURSOR_TYPE_NORMAL&&
            LawnLoadGame(&board,save),"Saving a retained field-card selection failed or serialized the transient gesture");
        for(const auto id:placed) if(auto* p=board.mPlants.DataArrayTryToGet(id))
            p->Die(board.mLevelTimeNs,app.mEffectSystem->mReanimationHolder->mVisualTimeNs,board.mLevelTimeNs);
        Coin* coin=nullptr;while(board.IterateCoins(coin)) if(coin->mType==COIN_USABLE_SEED_PACKET) coin->Die();
        board.ProcessDeleteQueue();
        Require(board.mPlants.mSize==initialPlants,"Field-card fixture left a planted test object");
        std::cout<<"PASS actual field cards mode="<<int(mode)<<": tap/jitter, same-card cancel, switch, source-return guard, drag through cards, shared selection, mouse ownership, frame UVs, stale generation and DAT64\n";
    }
    app.mGameMode=savedMode;board.mBackground=savedBackground;
    board.mSeedBank->mSeedPackets[0]=savedPacket;
    std::cout<<"PASS actual vase touch: hammer/payload during held gesture, no chained pickup/planting, fresh gesture selects the released card\n";
}

void TouchCannonPickupRuntime(TestApp& app, Board& board)
{
    auto& manager=*app.mWidgetManager;
    board.ClearTouchSelection();board.RefreshSeedPacketFromCursor();
    // Hit areas must remain byte-for-byte equivalent to the old mouse formula.
    Coin sample;
    sample.mAttachmentID=ATTACHMENTID_NULL;sample.mApp=&app;sample.mBoard=&board;
    sample.mPosX=300.25f;sample.mPosY=300.5f;sample.mWidth=40;sample.mHeight=40;
    sample.mDead=false;sample.mIsBeingCollected=false;
    for(const bool whack:{false,true}) {
        app.mGameMode=whack?GAMEMODE_MINIGAME_WHACK_A_ZOMBIE:GAMEMODE_ADVENTURE;
        for(int type=0;type<=int(COIN_PRESENT_SURVIVAL_MODE);++type) {
            sample.mType=static_cast<CoinType>(type);
            const bool lowered=sample.mType==COIN_PRESENT_PLANT||sample.mType==COIN_AWARD_PRESENT||
                sample.mType==COIN_PRESENT_MINIGAMES||sample.mType==COIN_PRESENT_PUZZLE_MODE||sample.mType==COIN_PRESENT_SURVIVAL_MODE;
            const int offset=lowered?-20:0,margin=whack||sample.mType==COIN_SUN?15:0;
            for(const int x:{285,286,300,301,340,341,355,356})
            for(const int y:{265,266,280,281,285,286,300,301,340,341,355,356,370,371,385,386}) {
                HitResult hit;
                const bool expected=x>=sample.mPosX-margin&&x<sample.mPosX+sample.mWidth+margin&&
                    y>=sample.mPosY+offset-margin&&y<sample.mPosY+sample.mHeight+offset+margin+(whack?30:0);
                Require(sample.MouseHitTest(x,y,&hit)==expected,"Shared coin hit area differs from the original mouse formula");
                Require(sample.TouchHitTest(x,y,x,y)==(expected&&(sample.IsSun()||sample.IsMoney()||sample.IsLevelAward())),
                    "Point touch pickup changed hit area or award/card eligibility");
            }
        }
    }
    app.mGameMode=GAMEMODE_ADVENTURE;
    sample.mType=COIN_GOLD;sample.mPosX=sample.mPosY=300;
    Require(sample.TouchHitTest(250,320,400,320)&&sample.TouchHitTest(320,250,320,400),
        "Touch movement skipped a coin between sample endpoints");
    Require(!sample.TouchHitTest(250,310,310,250)&&!sample.TouchHitTest(340,310,340,330)&&
        !sample.TouchHitTest(300,380,380,300),"Touch sweep accepted an AABB-only hit or an excluded edge");
    sample.mIsBeingCollected=true;
    Require(!sample.TouchHitTest(250,320,400,320),"Touch sweep recollected a coin");
    sample.mIsBeingCollected=false;sample.mDead=true;
    Require(!sample.TouchHitTest(250,320,400,320),"Touch sweep collected a dead coin");

    auto* first=board.NewPlant(0,3,SEED_COBCANNON,SEED_NONE);
    auto* second=board.NewPlant(3,3,SEED_COBCANNON,SEED_NONE);
    const auto firstId=static_cast<PlantID>(board.mPlants.DataArrayGetID(first));
    const auto secondId=static_cast<PlantID>(board.mPlants.DataArrayGetID(second));
    const TouchEvent a{701,board.GridToPixelX(1,3)+40,board.GridToPixelY(1,3)+40}; // Right half of cannon one.
    const TouchEvent b{702,board.GridToPixelX(3,3)+40,board.GridToPixelY(3,3)+40};
    const TouchEvent targetA{a.id,700,200},targetB{b.id,680,450};
    const auto placeCoin=[&](CoinType type,int centerX,int centerY){
        auto* coin=board.AddCoin(centerX,centerY,type,COIN_MOTION_COIN);
        coin->mPosX=float(centerX-20);coin->mPosY=float(centerY-20);coin->mWidth=coin->mHeight=40;
        return coin;
    };
    const auto arm=[&]{first->mState=second->mState=STATE_COBCANNON_READY;};
    arm();
    auto* startSun=placeCoin(COIN_SUN,a.x,a.y);
    auto* startGold=placeCoin(COIN_GOLD,b.x,b.y);
    auto* pathSilver=placeCoin(COIN_SILVER,(a.x+targetA.x)/2,(a.y+targetA.y)/2);
    auto* pathSun=placeCoin(COIN_SMALLSUN,(b.x+targetB.x)/2,(b.y+targetB.y)/2);
    manager.MouseMove(450,550);const int mouseX=manager.mLastMouseX,mouseY=manager.mLastMouseY;
    manager.TouchBegan(a);manager.TouchBegan(b);
    auto& ca=board.mTouchState->contacts[0];auto& cb=board.mTouchState->contacts[1];
    Require(ca.cannon&&cb.cannon&&ca.cursor!=cb.cursor&&ca.cursor->mCobCannonPlantID==firstId&&
        cb.cursor->mCobCannonPlantID==secondId,"Two fingers did not claim two independent cob cannons");
    Require(startSun->mIsBeingCollected&&startGold->mIsBeingCollected,
        "Coins in front of a cannon blocked selection or were not collected");
    auto* incidentalGold=placeCoin(COIN_GOLD,640,550);
    manager.TouchBegan({708,600,550});manager.TouchMoved({708,700,550});manager.TouchEnded({708,700,550});
    Require(incidentalGold->mIsBeingCollected&&ca.cannon&&cb.cannon&&manager.mTouchCount==2&&
        first->mState==STATE_COBCANNON_READY&&second->mState==STATE_COBCANNON_READY,
        "An unassigned third finger failed pickup or disturbed the two aiming fingers");
    const int startBX=cb.startX,startBY=cb.startY;
    manager.TouchMoved(targetA);board.UpdateTouchCursors();
    Require(pathSilver->mIsBeingCollected&&ca.cursor->mCobCannonPlantID==firstId&&cb.cursor->mCobCannonPlantID==secondId,
        "Sweep pickup stole a cannon target or missed the movement segment");
    manager.TouchEnded(targetA);
    Require(first->mState==STATE_COBCANNON_FIRING&&first->mTargetX==targetA.x-47&&first->mTargetY==targetA.y&&
        second->mState==STATE_COBCANNON_READY&&cb.active&&cb.cannon&&cb.startX==startBX&&cb.startY==startBY,
        "First release fired the wrong cannon or changed the remaining finger");
    manager.TouchMoved(targetB);board.UpdateTouchCursors();manager.TouchEnded(targetB);
    Require(second->mState==STATE_COBCANNON_FIRING&&second->mTargetX==targetB.x-47&&second->mTargetY==targetB.y&&
        pathSun->mIsBeingCollected&&manager.mTouchCount==0&&board.mTouchState->activeCount==0&&
        manager.mLastMouseX==mouseX&&manager.mLastMouseY==mouseY,"Second cannon release/pickup changed IDs, aim or the physical mouse");
    manager.TouchEnded(targetA);manager.TouchEnded(targetB);
    Require(first->mTargetY==targetA.y&&second->mTargetY==targetB.y,"A late release fired again");

    arm();manager.TouchBegan(a);manager.TouchBegan({703,a.x,a.y});
    Require(ca.cannon&&!cb.cannon,"Two fingers claimed the same cannon");
    manager.TouchEnded({703,targetA.x,targetA.y});
    Require(first->mState==STATE_COBCANNON_READY&&ca.active,"Duplicate claim released or fired the owning finger");
    manager.TouchCanceled(a);
    Require(first->mState==STATE_COBCANNON_READY&&ca.cursor->mCursorType==CURSOR_TYPE_NORMAL,
        "Cancel fired a cannon or retained its canceled target");
    arm();manager.TouchBegan(a);manager.TouchBegan(b);manager.TouchCanceled(a);
    manager.TouchMoved(targetB);manager.TouchEnded(targetB);
    Require(first->mState==STATE_COBCANNON_READY&&second->mState==STATE_COBCANNON_FIRING,
        "Canceling one cannon canceled another finger");

    arm();manager.TouchBegan(a);manager.TouchBegan(b);manager.TouchEnded(a);
    Require(ca.cursor->mCursorType==CURSOR_TYPE_NORMAL&&cb.active,
        "Short cannon tap retained a pending tap-to-aim target");
    manager.TouchBegan({709,b.x,b.y});manager.TouchMoved({709,targetA.x,targetA.y});manager.TouchEnded({709,targetA.x,targetA.y});
    Require(first->mState==STATE_COBCANNON_READY&&second->mState==STATE_COBCANNON_READY&&cb.active&&
        ca.cursor->mCursorType==CURSOR_TYPE_NORMAL,"Rejected claim of B fired a cannon");
    manager.TouchCanceled(b);board.ClearTouchSelection();

    for(const auto offset:std::array<std::pair<int,int>,6>{{{0,0},{80,0},{0,-80},{81,0},{60,-60},{-81,0}}}) {
        arm();manager.TouchBegan(a);manager.TouchEnded({a.id,a.x+offset.first,a.y+offset.second});
        const bool fires=offset.first*offset.first+offset.second*offset.second>80*80;
        Require((first->mState==STATE_COBCANNON_FIRING)==fires&&ca.cursor->mCursorType==CURSOR_TYPE_NORMAL,
            "Cannon drag did not enforce the one-cell radial boundary or clear its target");
        manager.TouchBegan({710,targetA.x,targetA.y});manager.TouchEnded({710,targetA.x,targetA.y});
        Require((first->mState==STATE_COBCANNON_FIRING)==fires,
            "Tap cannon then tap lawn fired a pending cannon");
    }

    arm();manager.TouchBegan(a);manager.TouchBegan(b);
    const auto aimingSave=(std::filesystem::current_path()/"touch-cannon-aiming.dat64").string();
    Require(LawnSaveGame(&board,aimingSave)&&manager.mTouchCount==0&&board.mTouchState->activeCount==0&&
        !ca.cannon&&!cb.cannon&&ca.cursor->mCursorType==CURSOR_TYPE_NORMAL&&
        cb.cursor->mCursorType==CURSOR_TYPE_NORMAL&&first->mState==STATE_COBCANNON_READY&&
        second->mState==STATE_COBCANNON_READY,"Saving two active cannon gestures fired or serialized a touch target");

    // Mouse and touch can race to fire the same cannon. The second submit must
    // never assert in Plant::CobCannonFire or overwrite the first target.
    arm();manager.MouseDown(a.x,a.y,1);manager.MouseUp(a.x,a.y,1);
    manager.TouchBegan(a);manager.TouchEnded(targetA);
    Require(!ca.cannon&&first->mState==STATE_COBCANNON_FIRING&&
        board.mCursorObject->mCursorType==CURSOR_TYPE_NORMAL,
        "Touch did not cancel the mouse cannon target before starting its own drag");
    board.RefreshSeedPacketFromCursor();
    arm();
    manager.TouchBegan(a);manager.MouseDown(a.x,a.y,1);manager.MouseUp(a.x,a.y,1);
    manager.TouchEnded(targetA);
    manager.MouseDown(targetB.x,targetB.y,1);manager.MouseUp(targetB.x,targetB.y,1);
    Require(first->mState==STATE_COBCANNON_FIRING&&first->mTargetY==targetA.y,
        "A stale mouse target fired the cannon a second time");

    auto* removed=board.NewPlant(6,1,SEED_COBCANNON,SEED_NONE);removed->mState=STATE_COBCANNON_READY;
    const TouchEvent dying{704,board.GridToPixelX(6,1)+40,board.GridToPixelY(6,1)+40};
    manager.TouchBegan(dying);
    const auto visual=app.mEffectSystem->mReanimationHolder->mVisualTimeNs;
    removed->Die(static_cast<std::int64_t>(board.mLevelTimeNs),visual,static_cast<std::int64_t>(board.mLevelTimeNs));
    board.ProcessDeleteQueue();manager.TouchEnded({dying.id,700,250});
    Require(ca.cursor->mCursorType==CURSOR_TYPE_NORMAL&&manager.mTouchCount==0,
        "Removing a cannon while aiming left a dangling target");

    auto* pausedGold=placeCoin(COIN_GOLD,500,550);
    board.Pause(true);manager.TouchBegan({705,420,550});manager.TouchMoved({705,600,550});manager.TouchEnded({705,600,550});
    Require(!pausedGold->mIsBeingCollected,"Touch sweep collected money while Board was paused");
    board.Pause(false);
    manager.TouchBegan({706,420,550});manager.TouchMoved({706,600,550});manager.TouchEnded({706,600,550});
    Require(pausedGold->mIsBeingCollected,"Ordinary touch movement did not collect money");
    const float collectionOrigin=pausedGold->mCollectX;
    pausedGold->mPosX+=10;
    manager.TouchBegan({707,420,550});manager.TouchMoved({707,600,550});manager.TouchEnded({707,600,550});
    Require(pausedGold->mCollectX==collectionOrigin,"Passing the same coin collected it twice");
    board.ClearTouchSelection();
    std::cout<<"PASS cannon touch/pickup: independent cannons, drag-only one-cell boundary, no pending tap aim, interleaved cancel, duplicate/mouse claims, stale/deleted target, swept sun/money, pause and idempotence\n";

    // Exercise the real reward handler, not just its hit rectangle. An award
    // encountered mid-drag must cancel the cannon before it can fire on up.
    board.RefreshSeedPacketFromCursor();
    for(const auto type:{COIN_FINAL_SEED_PACKET,COIN_AWARD_MONEY_BAG,COIN_TROPHY}) {
        arm();board.mBoardFadeOutCounter=-1;board.mLevelComplete=false;
        auto* award=placeCoin(type,type==COIN_FINAL_SEED_PACKET?450:(a.x+targetA.x)/2,
            type==COIN_FINAL_SEED_PACKET?260:(a.y+targetA.y)/2);
        if(type==COIN_FINAL_SEED_PACKET) {
            manager.TouchBegan({711,450,260});manager.TouchEnded({711,450,260});
        } else {
            manager.TouchBegan(a);
            if(type==COIN_AWARD_MONEY_BAG) manager.TouchMoved(targetA);
            else manager.TouchEnded(targetA);
        }
        Require(award->mIsBeingCollected&&board.mBoardFadeOutCounter>=0&&manager.mTouchCount==0&&
            board.mTouchState->activeCount==0&&first->mState==STATE_COBCANNON_READY,
            ("Touch award did not enter the existing level transition or committed a canceled cannon: type="+
            std::to_string(int(type))+" collected="+std::to_string(award->mIsBeingCollected)+
            " fade="+std::to_string(board.mBoardFadeOutCounter.mNanoseconds)+" touches="+
            std::to_string(manager.mTouchCount)+" active="+std::to_string(board.mTouchState->activeCount)+
            " cannon="+std::to_string(int(first->mState))).c_str());
        manager.TouchEnded({a.id,700,200});
        Require(first->mState==STATE_COBCANNON_READY,"A late award gesture fired a cannon");
    }
    board.mBoardFadeOutCounter=-1;board.mLevelComplete=false;
    std::cout<<"PASS award touch pickup: new plant on down, moneybag on move, trophy on up, real FadeOutLevel, canceled gesture and late release\n";
}

void TouchGardenRuntime(TestApp& app)
{
    auto& manager=*app.mWidgetManager;
    struct Restore {
        TestApp& app;
        Board* board=app.mBoard;
        PlayerInfo* player=app.mPlayerInfo;
        ZenGarden* garden=app.mZenGarden;
        GameMode mode=app.mGameMode;
        GameScenes scene=app.mGameScene;
        Rect bounds=app.mScreenBounds;
        int daveMessage=app.mCrazyDaveMessageIndex;
        ~Restore() {
            app.mBoard=board;app.mPlayerInfo=player;app.mZenGarden=garden;
            app.mGameMode=mode;app.mGameScene=scene;app.mCrazyDaveMessageIndex=daveMessage;
            app.mScreenBounds=bounds;app.mWidgetManager->Resize(bounds,bounds);
        }
    } restore{app};
    PlayerInfo profile;
    ZenGarden garden;
    profile.mId=46;profile.mLevel=50;profile.mFinishedAdventure=1;
    profile.mNumPottedPlants=1;
    auto& spec=profile.mPottedPlant[0];
    spec.InitializePottedPlant(SEED_PEASHOOTER);spec.mWhichZenGarden=GARDEN_MAIN;
    spec.mPlantAge=PLANTAGE_FULL;spec.mX=1;spec.mY=1;
    for(const auto item:{STORE_ITEM_GARDENING_GLOVE,STORE_ITEM_WHEEL_BARROW,
        STORE_ITEM_PHONOGRAPH,STORE_ITEM_MUSHROOM_GARDEN}) profile.mPurchases[item]=1;
    for(const auto item:{STORE_ITEM_FERTILIZER,STORE_ITEM_BUG_SPRAY,STORE_ITEM_CHOCOLATE,STORE_ITEM_TREE_FOOD})
        profile.mPurchases[item]=PURCHASE_COUNT_OFFSET+10;
    app.mPlayerInfo=&profile;app.mZenGarden=&garden;
    app.mGameMode=GAMEMODE_LIMBO_ZEN_GARDEN;app.mGameScene=SCENE_PLAYING;app.mCrazyDaveMessageIndex=-1;
    app.mScreenBounds=Rect(0,0,800,600);manager.Resize(app.mScreenBounds,app.mScreenBounds);
    Board board(&app);board.InitLevel();board.StartLevel();board.Resize(0,0,800,600);
    struct Remove {WidgetManager& manager;Board& board;~Remove(){
        manager.CancelTouches(&board);manager.RemoveWidget(&board);manager.CancelMouseDownState();
    }} remove{manager,board};
    manager.AddWidget(&board);manager.MouseMove(770,550);
    const int mouseX=manager.mLastMouseX,mouseY=manager.mLastMouseY;
    const auto purchases=std::to_array(profile.mPurchases);
    const auto point=[&](GameObjectType tool,unsigned long long id=901) {
        Rect bounds=board.GetShovelButtonRect();board.GetZenButtonRect(tool,bounds);
        return TouchEvent{id,board.mX+bounds.mX+bounds.mWidth/2,board.mY+bounds.mY+bounds.mHeight/2};
    };
    const auto tap=[&](GameObjectType tool) {const auto e=point(tool);manager.TouchBegan(e);manager.TouchEnded(e);};
    constexpr std::array tools{
        std::pair{OBJECT_TYPE_WATERING_CAN,CURSOR_TYPE_WATERING_CAN},
        std::pair{OBJECT_TYPE_FERTILIZER,CURSOR_TYPE_FERTILIZER},
        std::pair{OBJECT_TYPE_BUG_SPRAY,CURSOR_TYPE_BUG_SPRAY},
        std::pair{OBJECT_TYPE_PHONOGRAPH,CURSOR_TYPE_PHONOGRAPH},
        std::pair{OBJECT_TYPE_CHOCOLATE,CURSOR_TYPE_CHOCOLATE},
        std::pair{OBJECT_TYPE_GLOVE,CURSOR_TYPE_GLOVE},
        std::pair{OBJECT_TYPE_MONEY_SIGN,CURSOR_TYPE_MONEY_SIGN},
        std::pair{OBJECT_TYPE_WHEELBARROW,CURSOR_TYPE_WHEEELBARROW}
    };
    for(const auto [tool,cursor]:tools) {
        tap(tool);
        Require(board.mTouchState->selection->cursor->mCursorType==cursor,
            "Garden tool tap did not retain its unique selection");
        tap(tool);
        Require(board.mTouchState->selection->cursor->mCursorType==CURSOR_TYPE_NORMAL,
            "Second tap did not cancel the garden tool");
    }
    for(const auto [tool,cursor]:tools) {
        tap(tool);
        Require(board.mTouchState->selection->cursor->mCursorType==cursor,
            "Garden tool could not replace another selected tool");
    }
    Require(std::equal(std::begin(purchases),std::end(purchases),std::begin(profile.mPurchases))&&
        board.mGridItems.mSize==0,"Selecting tools used inventory or spawned a feeding animation");
    board.ClearTouchSelection();
    auto water=point(OBJECT_TYPE_WATERING_CAN,902),glove=point(OBJECT_TYPE_GLOVE,903);
    manager.TouchBegan(water);manager.TouchBegan(glove);manager.TouchEnded(glove);
    Require(board.mTouchState->activeCount==1&&board.mTouchState->selection->cursor->mCursorType==CURSOR_TYPE_WATERING_CAN,
        "Second garden finger replaced the active shared tool");
    manager.TouchEnded(water);
    Require(board.mCursorObject->mCursorType==CURSOR_TYPE_NORMAL&&manager.mLastMouseX==mouseX&&manager.mLastMouseY==mouseY,
        "Garden touch changed the physical mouse position or cursor");
    auto& render=*app.mDDInterface->mRenderInterface;
    for(const int width:{800,1400}) {
        app.mScreenBounds=Rect((800-width)/2,0,width,600);manager.Resize(app.mScreenBounds,app.mScreenBounds);
        board.Resize(app.mScreenBounds.mX+110,0,width,600);
        for(const auto [tool,cursor]:{tools.front(),tools[5],tools.back()}) {
            board.mTouchState->selection->cursor->mCursorType=cursor;
            Rect bounds=board.GetShovelButtonRect();board.GetZenButtonRect(tool,bounds);
            std::unique_ptr<Image> actual(render.CreateRenderImage(width,600)),expected(render.CreateRenderImage(width,600));
            const auto draw=[&](Image* target,bool production) {
                GpuImageDrawScope destination(render,target);Graphics g(target);
                g.mColor=Color::Black;g.FillRect(0,0,width,600);
                g.Translate(board.mX-app.mScreenBounds.mX,board.mY);
                if(production) board.DrawTouchSelection(&g);
                else for(int i=0;i<4;++i) {
                    const Rect source(i&1?62:0,i&2?108:0,42,36);
                    const Rect dest(i&1?bounds.mX+bounds.mWidth-18:bounds.mX-3,
                        i&2?bounds.mY+bounds.mHeight-15:bounds.mY-3,21,18);
                    g.DrawImage(gTouchFrameReference,dest,source);
                }
            };
            draw(actual.get(),true);draw(expected.get(),false);render.Flush();
            const auto pixels=render.ReadImagePixels(*actual);
            Require(TouchFrameMatches(pixels.pixels,render.ReadImagePixels(*expected).pixels)&&
                std::count_if(pixels.pixels.begin(),pixels.pixels.end(),[](unsigned int p){return (p&255)>80;})>80,
                "Garden selection frame lost its actual tool bounds, UVs or viewport/Board offset");
            Require(ImageLib::WritePNGImage("touch-garden-frame-"+std::to_string(width)+"-"+std::to_string(tool)+".png",
                width,600,pixels.pixels),"Cannot write garden selection proof");
        }
    }
    app.mScreenBounds=Rect(0,0,800,600);manager.Resize(app.mScreenBounds,app.mScreenBounds);board.Resize(0,0,800,600);
    board.ClearTouchSelection();
    Plant* target=nullptr;while(board.IteratePlants(target)) if(target->mPottedPlantIndex==0) break;
    Require(target,"Garden touch fixture did not instantiate its real potted plant");
    const TouchEvent plantPoint{904,int(target->mX+40),int(target->mY+40)};
    tap(OBJECT_TYPE_GLOVE);manager.TouchBegan(plantPoint);manager.TouchEnded(plantPoint);
    Require(board.mTouchState->selection->cursor->mCursorType==CURSOR_TYPE_PLANT_FROM_GLOVE,
        "Garden glove could not pick up its actual plant");
    tap(OBJECT_TYPE_GLOVE);
    Require(board.mTouchState->selection->cursor->mCursorType==CURSOR_TYPE_NORMAL&&target->mPottedPlantIndex==0,
        "Canceling the glove's carried plant altered or lost the plant");
    tap(OBJECT_TYPE_GLOVE);manager.TouchBegan(plantPoint);manager.TouchEnded(plantPoint);
    tap(OBJECT_TYPE_WHEELBARROW);
    Require(garden.GetPottedPlantInWheelbarrow()==&spec&&board.mTouchState->selection->cursor->mCursorType==CURSOR_TYPE_NORMAL,
        "Touch toolbar switching bypassed the glove-to-wheelbarrow transfer");
    tap(OBJECT_TYPE_GLOVE);tap(OBJECT_TYPE_WHEELBARROW);
    Require(board.mTouchState->selection->cursor->mCursorType==CURSOR_TYPE_PLANT_FROM_WHEEL_BARROW,
        "Glove could not retrieve a plant from the loaded wheelbarrow");
    tap(OBJECT_TYPE_WHEELBARROW);
    Require(board.mTouchState->selection->cursor->mCursorType==CURSOR_TYPE_NORMAL&&garden.GetPottedPlantInWheelbarrow()==&spec,
        "Canceling a wheelbarrow pickup lost its contents");
    app.mGameMode=GAMEMODE_TREE_OF_WISDOM;
    tap(OBJECT_TYPE_TREE_FOOD);
    Require(board.mTouchState->selection->cursor->mCursorType==CURSOR_TYPE_TREE_FOOD,"Tree food did not retain touch selection");
    tap(OBJECT_TYPE_TREE_FOOD);
    Require(board.mTouchState->selection->cursor->mCursorType==CURSOR_TYPE_NORMAL,"Tree food could not cancel selection");
    std::cout<<"PASS garden touch: eight tools, same-tool cancel, direct replacement, one shared selection, independent mouse, real glove/wheelbarrow transfers, tree food, 800/1400 offset frame GPU readback\n";
}

void TouchInputRuntime(TestApp& app)
{
    auto& manager = *app.mWidgetManager;
    struct Probe : Widget {
        int began=0, moved=0, ended=0, canceled=0, mouseDown=0;
        TouchEvent last;
        void TouchBegan(const TouchEvent& e) override {++began;last=e;}
        void TouchMoved(const TouchEvent& e) override {++moved;last=e;}
        void TouchEnded(const TouchEvent& e) override {++ended;last=e;}
        void TouchCanceled(const TouchEvent& e) override {++canceled;last=e;}
        void MouseDown(int,int,int) override {++mouseDown;}
    } first,second;
    first.Resize(10,20,100,100);second.Resize(150,20,100,100);
    manager.AddWidget(&first);manager.AddWidget(&second);
    manager.TouchBegan({11,30,40});manager.TouchBegan({22,180,40});
    Require(first.began==1&&second.began==1&&manager.mTouchCount==2,
        "Touch IDs did not capture independent widgets");
    Require(manager.mLastPointerInput==PointerInputSource::Touch,"Touch lost its input source");
    manager.TouchMoved({11,190,80});
    Require(first.moved==1&&first.last.x==180&&first.last.y==60&&second.moved==0,
        "Contact movement lost capture or local coordinate mapping");
    manager.MouseDown(180,40,1);
    Require(second.mouseDown==1&&manager.mLastDownWidget==&second&&manager.mTouchCount==2,
        "Touch capture stole mouse capture");
    Require(manager.mLastPointerInput==PointerInputSource::Mouse,"Physical mouse lost its input source");
    manager.TouchEnded({11,190,80});
    Require(first.ended==1&&second.ended==0&&manager.mDownButtons==1,
        "Releasing a touch released another input");
    manager.RemoveWidget(&second);
    Require(second.canceled==1&&manager.mTouchCount==0,"Removed widget retained a contact");
    manager.CancelMouseDownState();manager.RemoveWidget(&first);
    manager.TouchEnded({22,180,40});
    Require(second.ended==0,"Canceled contact activated on late release");

    struct LegacyButton : Widget {
        int clicks=0;
        bool mouseStateChanged=false;
        void MouseDown(int,int,int) override {
            mouseStateChanged|=mWidgetManager->mLastMouseX!=400||mWidgetManager->mLastMouseY!=200||mWidgetManager->mMouseIn;
        }
        void MouseDrag(int,int) override {
            mouseStateChanged|=mWidgetManager->mLastMouseX!=400||mWidgetManager->mLastMouseY!=200||mWidgetManager->mMouseIn;
        }
        void MouseUp(int,int,int) override {
            mouseStateChanged|=mWidgetManager->mLastMouseX!=400||mWidgetManager->mLastMouseY!=200||mWidgetManager->mMouseIn;
            if(mIsOver) ++clicks;
        }
    } button;
    button.Resize(10,20,100,100);manager.AddWidget(&button);
    manager.MouseMove(400,200);manager.MouseExit(400,200);
    manager.TouchBegan({33,30,40});manager.TouchMoved({33,31,41});manager.TouchEnded({33,30,40});
    Require(button.clicks==1&&manager.mLastMouseX==400&&manager.mLastMouseY==200&&!button.mouseStateChanged,
        "Legacy widget touch adapter lost click position or changed mouse position");
    manager.TouchBegan({33,30,40});manager.TouchEnded({33,400,200});
    manager.TouchBegan({33,30,40});manager.CancelTouches(&button);
    Require(button.clicks==1,"Outside release or cancellation clicked a button");
    manager.RemoveWidget(&button);
    struct ClickListener : ButtonListener {
        int clicks=0;
        void ButtonPress(int) override {}
        void ButtonDepress(int) override {++clicks;}
        void ButtonDownTick(int) override {}
        void ButtonMouseEnter(int) override {}
        void ButtonMouseLeave(int) override {}
        void ButtonMouseMove(int,int,int) override {}
    } clickListener;
    ButtonWidget clickButton(73,&clickListener);
    clickButton.Resize(10,20,100,100);manager.AddWidget(&clickButton);
    const bool hadFocus=manager.mHasFocus;manager.mHasFocus=true;
    manager.MouseDown(30,40,1);manager.MouseUp(400,200,1);
    Require(clickListener.clicks==0,"Mouse release used stale hover instead of event coordinates");
    manager.MouseDown(30,40,1);manager.MouseUp(30,40,1);
    Require(clickListener.clicks==1,"Mouse press/release at the same button did not activate");
    manager.RemoveWidget(&clickButton);manager.mHasFocus=hadFocus;
    manager.AddWidget(&first);manager.AddWidget(&second);
    manager.TouchBegan({41,30,40});manager.TouchBegan({42,180,40});
    manager.TouchCanceled({41,30,40});
    Require(manager.mTouchCount==1&&second.canceled==1,
        "Canceling one ID canceled another contact");
    manager.MouseMove(400,200); // An actual move is mouse input.
    manager.TouchMoved({42,180,41});
    manager.MouseMove(400,200); // A stationary heartbeat is not new mouse input.
    Require(manager.mLastPointerInput==PointerInputSource::Touch&&!manager.mMouseHoverEnabled,
        "Stationary mouse polling overwrote the last real touch source");
    manager.CancelTouches();manager.RemoveWidget(&first);manager.RemoveWidget(&second);

    struct ListProbe : ListListener {
        int clicks=0, index=-1, count=0;
        void ListClicked(int, int row, int clickCount) override {++clicks;index=row;count=clickCount;}
        void ListClosed(int) override {}
        void ListHiliteChanged(int,int,int) override {}
    } listListener;
    ListWidget list(17,FONT_BRIANNETOD16,&listListener);
    list.mItemHeight=24;list.Resize(20,20,200,140);
    list.AddLine("First user",false);list.AddLine("Second user",false);
    list.AddLine("Create a New User",false);
    manager.AddWidget(&list);
    manager.MouseMove(400,200);
    Require(list.mHiliteIdx==-1,"List touch fixture started with mouse hover");
    manager.TouchBegan({51,40,60});manager.TouchEnded({51,40,60});
    Require(listListener.clicks==1&&listListener.index==1&&listListener.count==1,
        "Touch could not select a user without mouse hover");
    manager.MouseMove(40,30);
    Require(list.mHiliteIdx==0,"List mouse hover did not select the first row");
    manager.TouchBegan({52,40,84});manager.TouchEnded({52,40,84});
    Require(listListener.clicks==2&&listListener.index==2&&list.mHiliteIdx==0&&
        manager.mLastMouseX==40&&manager.mLastMouseY==30&&
        manager.mLastPointerInput==PointerInputSource::Touch,
        "Touch selected the mouse-hovered user or changed the mouse state");
    manager.TouchBegan({53,40,130});manager.TouchEnded({53,40,130});
    Require(listListener.clicks==2,"Empty list space selected the mouse-hovered user");
    list.mPosition=1.0;
    manager.TouchBegan({54,40,30});manager.TouchEnded({54,40,30});
    Require(listListener.clicks==3&&listListener.index==1,"Touch ignored the list scroll offset");
    manager.RemoveWidget(&list);
    std::cout<<"PASS list press coordinates: touch without hover, mouse/touch separation, empty space and scroll offset\n";

    struct CompositeButtons : Widget {
        GameButton left{31},right{32};
        int leftClicks=0,rightClicks=0,cancels=0;
        CompositeButtons() {
            left.mParentWidget=right.mParentWidget=this;
            left.mX=0;right.mX=100;left.mY=right.mY=0;
            left.mWidth=right.mWidth=80;left.mHeight=right.mHeight=40;
            left.mBtnNoDraw=right.mBtnNoDraw=false;left.mDisabled=right.mDisabled=false;
        }
        const void* TouchHitTest(int x,int y) override {
            if(left.HitTest(x,y,this)) return &left;
            if(right.HitTest(x,y,this)) return &right;
            return this;
        }
        void MouseUp(int x,int y,int) override {
            if(left.HitTest(x,y,this)) ++leftClicks;
            if(right.HitTest(x,y,this)) ++rightClicks;
        }
        void TouchCanceled(const TouchEvent&) override {++cancels;}
    } controls;
    controls.Resize(10,20,200,50);manager.AddWidget(&controls);
    manager.MouseMove(30,30);controls.left.Update();
    Require(controls.left.mIsOver,"Physical mouse hover did not highlight the button");
    manager.TouchBegan({61,130,30});controls.left.Update();controls.right.Update();
    Require(!manager.mMouseHoverEnabled&&!controls.left.mIsOver&&controls.right.mIsOver,
        "Touch left a stale mouse hover or did not highlight its pressed button");
    manager.TouchMoved({61,30,30});controls.left.Update();controls.right.Update();
    Require(!controls.left.mIsOver&&!controls.right.mIsOver,
        "Dragging onto another button highlighted a button that was not pressed");
    manager.TouchEnded({61,30,30});
    Require(controls.leftClicks==0&&controls.rightClicks==0&&controls.cancels==1,
        "Different press/release buttons activated a control");
    manager.TouchBegan({62,130,30});manager.TouchEnded({62,130,30});
    Require(controls.rightClicks==1&&controls.leftClicks==0,"Matching press/release did not activate once");
    manager.MouseMove(30,30);
    Require(!manager.mMouseHoverEnabled,"Stationary mouse polling enabled hover after touch");
    manager.MouseMove(31,30);controls.left.Update();
    Require(manager.mMouseHoverEnabled&&controls.left.mIsOver,"Actual mouse movement did not restore hover");
    manager.RemoveWidget(&controls);
    std::cout<<"PASS button touch capture: hover source, held target, leave, cross-button cancel, matching release\n";

    app.mGameMode=GAMEMODE_ADVENTURE;app.mGameScene=SCENE_PLAYING;
    app.mScreenBounds=Rect(0,0,800,600);
    manager.Resize(Rect(0,0,800,600),Rect(0,0,800,600));
    struct TouchBoard : Board {
        using Board::Board;
        bool saveOnDown=false;
        bool touchDispatch=false;
        int mouseX=0,mouseY=0;
        bool mouseIn=false;
        std::string nestedSave;
        void TouchBegan(const TouchEvent& event) override {
            const auto& manager=*mApp->mWidgetManager;
            mouseX=manager.mLastMouseX;mouseY=manager.mLastMouseY;mouseIn=manager.mMouseIn;
            touchDispatch=true;Board::TouchBegan(event);touchDispatch=false;
        }
        void MouseDown(int x,int y,int clicks) override {
            if(touchDispatch) Require(mApp->mWidgetManager->mLastMouseX==mouseX&&
                mApp->mWidgetManager->mLastMouseY==mouseY&&mApp->mWidgetManager->mMouseIn==mouseIn,
                "Board touch temporarily rewrote the physical mouse position");
            Board::MouseDown(x,y,clicks);
            if(saveOnDown) Require(LawnSaveGame(this,nestedSave),"Nested touch save failed");
        }
    } board(&app);
    struct Restore {TestApp& app;Board& board;~Restore(){
        app.mWidgetManager->CancelTouches(&board);
        app.mWidgetManager->RemoveWidget(&board);app.mWidgetManager->CancelMouseDownState();
        app.mBoard=nullptr;
    }} restore{app,board};
    board.Resize(0,0,800,600);app.mGameMode=GAMEMODE_ADVENTURE;app.mGameScene=SCENE_PLAYING;
    app.mCrazyDaveState=CRAZY_DAVE_OFF;app.mEasyPlantingCheat=false;
    app.mPlayerInfo->mLevel=12; // Adventure predicates read the profile, not Board::mLevel.
    board.mLevel=12;board.mBackground=BACKGROUND_1_DAY;board.mSunMoney=9990;
    board.mCutScene->mSeedChoosing=false;
    board.mSeedBank->mX=0;board.mSeedBank->mY=0;board.mSeedBank->mNumPackets=3;
    const SeedType seeds[]{SEED_PEASHOOTER,SEED_SUNFLOWER,SEED_WALLNUT};
    for(int i=0;i<3;++i) {
        auto& packet=board.mSeedBank->mSeedPackets[i];
        packet.mX=board.GetSeedPacketPositionX(i);packet.mY=0;packet.mIndex=i;
        packet.mPacketType=seeds[i];packet.mImitaterType=SEED_NONE;
        packet.mActive=true;packet.mRefreshing=false;
    }
    manager.AddWidget(&board);manager.MouseMove(400,250);
    auto* mouseCursor=board.mCursorObject;
    const auto seedPoint=[&](int i){return TouchEvent{static_cast<unsigned long long>(101+i),
        int(board.mSeedBank->mSeedPackets[i].mX)+board.mSeedBank->mSeedPackets[i].mOffsetX+20,30};};
    manager.TouchBegan(seedPoint(0));manager.TouchBegan(seedPoint(1));
    Require(board.mTouchState&&board.mTouchState->contacts[0].active&&board.mTouchState->contacts[1].active&&
        board.mTouchState->selection==&board.mTouchState->contacts[0]&&
        board.mTouchState->contacts[1].cursor==nullptr,
        "Board allocated a second plant/tool selection");
    Require(!board.mSeedBank->mSeedPackets[0].mActive&&board.mSeedBank->mSeedPackets[1].mActive&&
        board.mCursorObject==mouseCursor&&mouseCursor->mCursorType==CURSOR_TYPE_NORMAL,
        "Touch card pickup corrupted the mouse cursor or did not reserve cards");
    auto mouseCard=seedPoint(2);
    Require(mouseCursor->mCursorType==CURSOR_TYPE_NORMAL,"Unexpected mouse selection");
    int mouseX=manager.mLastMouseX,mouseY=manager.mLastMouseY;
    const int x=board.GridToPixelX(2,2)+40,y=board.GridToPixelY(2,2)+40;
    manager.TouchMoved({101,x,y});board.UpdateTouchCursors();
    Require(board.mTouchState->contacts[0].preview->mVisible&&manager.mLastMouseX==mouseX&&
        manager.mLastMouseY==mouseY&&board.mCursorObject==mouseCursor,
        "Touch preview was invisible or leaked temporary input coordinates");
    manager.TouchEnded({101,x,y});
    auto* planted=board.GetTopPlantAt(2,2,TOPPLANT_ONLY_NORMAL_POSITION);
    Require(board.mPlants.mSize==1&&planted&&planted->mSeedType==SEED_PEASHOOTER&&
        mouseCursor->mCursorType==CURSOR_TYPE_NORMAL&&board.mTouchState->contacts[1].active,
        "Touch release did not plant exactly once or disturbed another cursor");
    const int afterFirst=board.mSunMoney;Require(afterFirst==9890,"Touch planting charged sun incorrectly");
    manager.TouchEnded({102,x+80,y});
    Require(board.mPlants.mSize==1&&board.mSeedBank->mSeedPackets[1].mActive,
        "An additional finger planted a second selected card");
    board.mSunMoney=9990;board.mSeedBank->mSeedPackets[1].mActive=true;
    manager.TouchBegan(seedPoint(1));board.mSunMoney=0;manager.TouchEnded({102,x+80,y});
    Require(board.mPlants.mSize==1&&!board.mSeedBank->mSeedPackets[1].mActive&&
        board.mTouchState->selection->cursor->mType==SEED_SUNFLOWER,
        "Failed placement did not keep the shared selected card");
    board.mSunMoney=9990;board.ClearTouchSelection();
    manager.TouchBegan(seedPoint(1));manager.CancelTouches(&board);manager.TouchEnded({102,x+80,y});
    Require(board.mPlants.mSize==1&&!board.mSeedBank->mSeedPackets[1].mActive&&manager.mTouchCount==0,
        "Cancellation planted or lost the retained selection");
    board.ClearTouchSelection();
    manager.TouchBegan(seedPoint(1));board.Pause(true);
    Require(manager.mTouchCount==0&&!board.mSeedBank->mSeedPackets[1].mActive,
        "Pause retained an active drag or lost the selected card");
    board.Pause(false);board.ClearTouchSelection();
    manager.TouchBegan(seedPoint(1));manager.TouchEnded({102,-100,700});
    Require(board.mPlants.mSize==1&&!board.mSeedBank->mSeedPackets[1].mActive,
        "Outside release planted or lost the selected card");
    board.nestedSave=(std::filesystem::current_path()/"touch-input-nested-save.dat64").string();
    board.ClearTouchSelection();
    board.saveOnDown=true;manager.TouchBegan(seedPoint(1));board.saveOnDown=false;
    Require(manager.mTouchCount==0&&board.mSeedBank->mSeedPackets[1].mActive&&
        board.mCursorObject==mouseCursor,"Save inside touch callback retained a gesture");
    Require(LawnLoadGame(&board,board.nestedSave)&&mouseCursor->mCursorType==CURSOR_TYPE_NORMAL,
        "Nested touch save serialized the contact cursor instead of the real mouse");
    board.RefreshSeedPacketFromCursor();
    manager.TouchBegan(seedPoint(1));
    const auto savePath=(std::filesystem::current_path()/"touch-input-save.dat64").string();
    Require(LawnSaveGame(&board,savePath)&&manager.mTouchCount==0&&
        board.mSeedBank->mSeedPackets[1].mActive&&board.mCursorObject==mouseCursor,
        "Save did not cancel and return the touch card before serialization");
    auto* touchState=board.mTouchState;
    board.mSunMoney=0;
    Require(LawnLoadGame(&board,savePath)&&board.mSunMoney==9990&&board.mPlants.mSize==1&&
        board.mTouchState==touchState&&board.mCursorObject==mouseCursor&&touchState->activeCount==0,
        "DAT64 roundtrip changed persistent state or serialized transient touch pointers");
    {
        const auto previousWindow=app.mHWnd;
        const bool previousReady=app.mNativeWindowReady;
        app.mHWnd=app.mDDInterface->mHWnd;app.mNativeWindowReady=true;
        const auto card=seedPoint(1);
        RECT client{};Require(GetClientRect(app.mHWnd,&client),"Cannot query hidden input window");
        const auto present=app.mDDInterface->mRenderInterface->mRenderer->PresentRect(client.right,client.bottom);
        const double px=present.x+(card.x+0.5)*present.width/800.0;
        const double py=present.y+(card.y+0.5)*present.height/600.0;
        const auto mouseBefore=Point(manager.mLastMouseX,manager.mLastMouseY);
        app.ProcessMessageNow(WM_XBUTTONDOWN,MAKEWPARAM(0,XBUTTON1),MAKELPARAM(int(px),int(py)),app.mHWnd);
        Require(manager.mTouchCount==0,"Side button still emulates touch");
        app.QueueNativeTouch(17,TOUCHEVENTF_DOWN,px,py);
        app.QueueNativeTouch(23,TOUCHEVENTF_DOWN,px+50,py);
        Require(manager.mTouchCount==0,"Native touch reentered widgets from the window procedure");
        app.DrainWindowMessages();
        Require(manager.mTouchCount==2,"Native contact IDs did not survive deferred dispatch");
        app.QueueNativeTouch(17,TOUCHEVENTF_MOVE,px+80,py+60);
        app.QueueNativeTouch(23,TOUCHEVENTF_UP,px+50,py);
        app.DrainWindowMessages();
        Require(manager.mTouchCount==1&&manager.mLastMouseX==mouseBefore.mX&&manager.mLastMouseY==mouseBefore.mY,
            "Native touch movement/up changed the mouse or ended another contact");
        app.QueueNativeTouch(17,0x80000000u,px+80,py+60);app.DrainWindowMessages();
        Require(manager.mTouchCount==0,"Native pointer capture cancellation retained a contact");
        app.mNativeTouchOwnsCursor=false;app.mNativeMouseMotionPending=false;
        app.mHWnd=previousWindow;app.mNativeWindowReady=previousReady;
    }
    board.ClearTouchSelection();
    auto* looseCard=board.AddCoin(500,350,COIN_USABLE_SEED_PACKET,COIN_MOTION_COIN);
    looseCard->mUsableSeedType=SEED_SUNFLOWER;
    manager.TouchBegan({301,int(looseCard->mPosX)+10,int(looseCard->mPosY)+10});
    Require(board.mTouchState->contacts[0].cursor->mCursorType==CURSOR_TYPE_PLANT_FROM_USABLE_COIN,
        "Touch did not pick up the dropped card");
    looseCard->Die();board.mCoins.DataArrayFree(looseCard);
    manager.TouchEnded({301,x+80,y});
    Require(board.mPlants.mSize==1&&manager.mTouchCount==0,
        "Expired dropped card was dereferenced or planted on release");
    app.mGameMode=GAMEMODE_MINIGAME_INVISIGHOUL;
    for(int i=0;i<3;++i) {
        auto& packet=board.mSeedBank->mSeedPackets[i];
        packet.mPacketType=seeds[i];packet.mActive=true;packet.mRefreshing=false;
    }
    manager.TouchBegan(seedPoint(1));
    mouseCard=seedPoint(2);manager.MouseDown(mouseCard.x,mouseCard.y,1);manager.MouseUp(mouseCard.x,mouseCard.y,1);
    // The first mouse command takes the shared held card and cancels it at the bank.
    manager.MouseDown(mouseCard.x,mouseCard.y,1);manager.MouseUp(mouseCard.x,mouseCard.y,1);
    board.mSeedBank->RemoveSeed(0);
    Require(board.mTouchState->contacts[0].cursor->mCursorType==CURSOR_TYPE_NORMAL&&mouseCursor->mSeedBankIndex==1&&
        board.mSeedBank->mSeedPackets[0].mActive&&!board.mSeedBank->mSeedPackets[1].mActive,
        "Conveyor removal changed held cards or made reserved cards available");
    manager.CancelTouches(&board);board.ClearTouchSelection();board.RefreshSeedPacketFromCursor();
    app.mGameMode=GAMEMODE_MINIGAME_WHACK_A_ZOMBIE;
    manager.TouchBegan({401,780,570});
    auto* hammer=board.mTouchState->contacts[0].cursor;
    Require(hammer->mCursorType==CURSOR_TYPE_HAMMER&&hammer->mReanimCursorID!=REANIMATIONID_NULL,
        "Whack touch did not initialize its own hammer");
    manager.TouchEnded({401,780,570});
    Require(hammer->mReanimCursorID==REANIMATIONID_NULL,
        "Released touch retained an unowned hammer animation");
    manager.TouchBegan({402,780,570});
    Require(hammer->mReanimCursorID!=REANIMATIONID_NULL,"Reused touch hammer did not recreate its animation");
    manager.CancelTouches(&board);
    app.mGameMode=GAMEMODE_ADVENTURE;
    board.ClearTouchSelection();
    board.mShowShovel=true;board.mSunMoney=9990;
    for(auto& packet:board.mSeedBank->mSeedPackets) packet.mOffsetX=0; // Return from the conveyor fixture to a settled normal bank.
    board.mSeedBank->mSeedPackets[1].mPacketType=SEED_SUNFLOWER;
    board.mSeedBank->mSeedPackets[1].mRefreshing=false;
    board.mSeedBank->mSeedPackets[1].mRefreshTime=0;
    board.mSeedBank->mSeedPackets[1].mActive=true;
    manager.TouchBegan(seedPoint(1));manager.TouchEnded(seedPoint(1));
    Require(board.mTouchState->selection->cursor->mType==SEED_SUNFLOWER&&
        !board.mTouchState->selection->active&&!board.mSeedBank->mSeedPackets[1].mActive,
        "Tapping a card did not retain its selected slot");
    manager.TouchBegan(seedPoint(1));manager.TouchEnded(seedPoint(1));
    Require(board.mTouchState->selection->cursor->mCursorType==CURSOR_TYPE_NORMAL&&
        board.mSeedBank->mSeedPackets[1].mActive,"Tapping the selected card did not cancel selection");
    manager.TouchBegan(seedPoint(1));manager.TouchEnded(seedPoint(1));
    const auto shovel=board.GetShovelButtonRect();
    mouseX=manager.mLastMouseX;mouseY=manager.mLastMouseY;
    TouchPresentationRuntime(app,board);
    const TouchEvent shovelPoint{501,shovel.mX+shovel.mWidth/2,shovel.mY+shovel.mHeight/2};
    manager.TouchBegan(shovelPoint);manager.TouchEnded(shovelPoint);
    Require(board.mTouchState->selection->cursor->mCursorType==CURSOR_TYPE_SHOVEL&&
        board.mSeedBank->mSeedPackets[1].mActive,"Shovel and plant did not share one selection");
    manager.TouchBegan(shovelPoint);manager.TouchEnded(shovelPoint);
    Require(board.mTouchState->selection->cursor->mCursorType==CURSOR_TYPE_NORMAL,
        "Tapping the selected shovel did not cancel selection");
    manager.TouchBegan(shovelPoint);manager.TouchMoved({shovelPoint.id,x,y});board.UpdateMousePosition();
    Require(planted->mHighlighted&&manager.mLastMouseX==mouseX&&manager.mLastMouseY==mouseY,
        "Shovel touch did not highlight its actual plant or changed mouse coordinates");
    manager.TouchMoved({shovelPoint.id,790,590});board.UpdateMousePosition();
    Require(!planted->mHighlighted,"Shovel plant highlight persisted after the finger moved away");
    manager.TouchCanceled(shovelPoint);board.ClearTouchSelection();
    auto* savedGarden=app.mZenGarden;
    ZenGarden touchGarden;touchGarden.mBoard=&board;
    app.mZenGarden=&touchGarden;app.mGameMode=GAMEMODE_LIMBO_ZEN_GARDEN;
    for(const auto tool:{CURSOR_TYPE_WATERING_CAN,CURSOR_TYPE_GLOVE,CURSOR_TYPE_MONEY_SIGN}) {
        board.mTouchState->selection->cursor->mCursorType=tool;
        manager.TouchBegan({752,x,y});
        Require(board.UpdateTouchHighlights()&&planted->mHighlighted,
            "Garden tool drag did not highlight its touch target");
        planted->UpdateReanimColor(static_cast<std::int64_t>(board.mLevelTimeNs));
        const auto* animation=app.ReanimationTryToGet(planted->mBodyReanimID);
        Require(animation&&animation->mEnableExtraAdditiveDraw,"Highlighted garden plant did not enable its existing glow draw");
        manager.TouchMoved({752,790,590});
        Require(board.UpdateTouchHighlights()&&!planted->mHighlighted,
            "Garden target kept glowing after the finger moved away");
        manager.TouchCanceled({752,790,590});board.ClearTouchSelection();
    }
    app.mGameMode=GAMEMODE_ADVENTURE;app.mZenGarden=savedGarden;
    std::cout<<"PASS touch tool glow: shovel target and garden watering/glove/sell targets, actual additive draw, moving away, independent mouse position\n";
    manager.TouchBegan(shovelPoint);manager.TouchEnded(shovelPoint);
    manager.TouchBegan(seedPoint(1));manager.TouchEnded(seedPoint(1));
    Require(board.mTouchState->selection->cursor->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK,
        "Selecting a plant did not replace the shovel");
    const int plantsBeforeTap=board.mPlants.mSize;
    manager.TouchBegan({502,x+160,y});
    Require(board.mPlants.mSize==plantsBeforeTap,"Tap-to-plant committed on down instead of up");
    manager.TouchEnded({502,x+160,y});
    Require(board.mPlants.mSize==plantsBeforeTap+1&&board.GetTopPlantAt(4,2,TOPPLANT_ONLY_NORMAL_POSITION),
        "Retained card could not be planted by tapping the lawn");
    std::cout<<"PASS touch input: shared plant/tool selection, one drag, retained tap/failed drop, tap-to-plant, independent mouse, DAT64, conveyor and hammer\n";
    TouchFieldCardsRuntime(app,board);
    TouchCannonPickupRuntime(app,board);
    board.ClearTouchSelection();board.RefreshSeedPacketFromCursor();
    for(int i=0;i<2;++i) {
        auto& packet=board.mSeedBank->mSeedPackets[i];
        packet.mPacketType=seeds[i];packet.mRefreshing=false;packet.mRefreshTime=0;packet.mActive=true;
    }
    auto sharedMouse=seedPoint(0);
    manager.MouseDown(sharedMouse.x,sharedMouse.y,1);manager.MouseUp(sharedMouse.x,sharedMouse.y,1);
    Require(board.mCursorObject->mCursorType==CURSOR_TYPE_PLANT_FROM_BANK,"Mouse did not establish shared selection");
    manager.TouchBegan(seedPoint(1));manager.TouchEnded(seedPoint(1));
    Require(board.mCursorObject->mCursorType==CURSOR_TYPE_NORMAL&&
        board.mTouchState->selection->cursor->mSeedBankIndex==1&&board.mSeedBank->mSeedPackets[0].mActive,
        "Touch did not replace and return the mouse's selected card");
    manager.MouseDown(790,590,-1);manager.MouseUp(790,590,-1);
    Require(board.mCursorObject->mCursorType==CURSOR_TYPE_NORMAL&&
        board.mTouchState->selection->cursor->mCursorType==CURSOR_TYPE_NORMAL&&board.mSeedBank->mSeedPackets[1].mActive,
        "Mouse cancellation left a second touch selection or reserved card");
    std::cout<<"PASS shared mouse/touch selection: cross-input replacement, source return, mouse cancel, one live selection\n";
    for(const int width:{800,1400}) {
        const Rect bounds((800-width)/2,0,width,600);
        app.mScreenBounds=bounds;manager.Resize(bounds,bounds);
        app.mGameScene=SCENE_LEVEL_INTRO;board.mCutScene->mSeedChoosing=true;
        SeedChooserScreen chooser;
        app.mSeedChooserScreen=&chooser;
        chooser.Resize(bounds.mX,0,width,600);chooser.mMouseVisible=true;
        chooser.MouseUp(-2000,-2000,1); // Settle its real responsive layout, without activating a button.
        manager.AddWidget(&chooser);manager.SetFocus(&chooser);
        const int menuX=chooser.mMenuButton->mX+50,menuY=10;
        manager.MouseMove(bounds.mX+10,500);
        const int savedMouseX=manager.mLastMouseX,savedMouseY=manager.mLastMouseY;
        const int viewX=chooser.mX+chooser.mViewLawnButton->mX+chooser.mViewLawnButton->mWidth/2;
        const int viewY=chooser.mY+chooser.mViewLawnButton->mY+10;
        manager.TouchBegan({600,viewX,viewY});
        Require(chooser.mChooseState==CHOOSE_NORMAL,"View Lawn activated before release");
        manager.TouchMoved({600,menuX,menuY});manager.TouchEnded({600,menuX,menuY});
        Require(chooser.mChooseState==CHOOSE_NORMAL&&!app.GetDialog(DIALOG_NEWOPTIONS),
            "Cross-button chooser drag activated View Lawn or Menu");
        manager.TouchBegan({601,menuX,menuY});chooser.mMenuButton->Update();
        Require(chooser.mMenuButton->mIsOver,"Chooser Menu did not highlight at the touch event position");
        manager.TouchEnded({601,menuX,menuY});
        Require(app.GetDialog(DIALOG_NEWOPTIONS)&&manager.mLastMouseX==savedMouseX&&manager.mLastMouseY==savedMouseY,
            "Chooser Menu touch used the global mouse position or wrong coordinate frame");
        app.KillDialog(DIALOG_NEWOPTIONS);app.ProcessSafeDeleteList();
        manager.MouseDown(menuX,menuY,1);manager.MouseUp(menuX,menuY,1);
        Require(app.GetDialog(DIALOG_NEWOPTIONS),"Chooser Menu mouse event did not open actual options");
        app.KillDialog(DIALOG_NEWOPTIONS);app.ProcessSafeDeleteList();
        manager.RemoveWidget(&chooser);app.mSeedChooserScreen=nullptr;
    }
    app.mScreenBounds=Rect(0,0,800,600);manager.Resize(app.mScreenBounds,app.mScreenBounds);
    app.mGameScene=SCENE_PLAYING;board.mCutScene->mSeedChoosing=false;board.Pause(false);
    std::cout<<"PASS actual chooser Menu: 800/1400 widths, touch with mouse elsewhere, physical click, real options callback\n";
    if(gTouchPerf)
    {
        DWORD_PTR processMask=0,systemMask=0;
        Require(GetProcessAffinityMask(GetCurrentProcess(),&processMask,&systemMask),"Cannot query affinity");
        DWORD_PTR affinity=(processMask&(DWORD_PTR(1)<<14)) ? (DWORD_PTR(1)<<14) :
            (processMask&(~processMask+1));
        if(const char* requested=std::getenv("PVZ_TOUCH_BENCH_AFFINITY"))
            affinity=static_cast<DWORD_PTR>(std::strtoull(requested,nullptr,0));
        Require(affinity&&(affinity&(affinity-1))==0&&(affinity&processMask)==affinity,
            "Touch benchmark requires one available logical processor");
        Require(SetProcessAffinityMask(GetCurrentProcess(),affinity),"Cannot pin touch benchmark");
        std::cout<<"TOUCH_PERF_AFFINITY "<<affinity<<'\n';
        struct Context {Board* board;WidgetManager* manager;unsigned int touches;};
        Context context{&board,&manager,0};
        using Work=void(*)(Context&,unsigned int);
        const auto measure=[&](const char* name,unsigned int iterations,Work work)
        {
            Work volatile entry=work;
            for(unsigned int i=0;i<2000;++i) entry(context,i);
            std::array<double,7> samples{};
            for(auto& ns:samples)
            {
                const auto start=std::chrono::steady_clock::now();
                for(unsigned int i=0;i<iterations;++i) entry(context,i);
                ns=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count()/iterations;
            }
            std::sort(samples.begin(),samples.end());
            std::cout<<"TOUCH_PERF {\"case\":\""<<name<<"\",\"iterations\":"<<iterations
                <<",\"rounds\":7,\"median_ns\":"<<samples[3]<<"}\n";
        };
        measure("no_touch_update_draw",2000000,[](Context& c,unsigned int){
            c.board->UpdateTouchCursors();c.board->DrawTouchPreviews(nullptr);c.board->DrawTouchCursors(nullptr);
        });
        first.Resize(0,570,100,30);second.Resize(200,570,100,30);
        manager.AddWidget(&first);manager.AddWidget(&second);
        measure("mouse_hover_no_touch",300000,[](Context& c,unsigned int i){
            c.manager->MouseMove((i&1)?20:220,585);
        });
        manager.RemoveWidget(&first);manager.RemoveWidget(&second);
        for(const bool mousePresent:{false,true})
        for(const unsigned int fingers:{1U,10U})
        {
            if(mousePresent) manager.MouseMove(400,250);
            else manager.MouseExit(-100,-100);
            board.mSeedBank->mNumPackets=10;
            for(int i=0;i<10;++i)
            {
                auto& packet=board.mSeedBank->mSeedPackets[i];
                packet.mPacketType=SEED_PEASHOOTER;packet.mActive=true;packet.mRefreshing=false;
                packet.mRefreshTime=0;packet.mOffsetX=0;packet.mIndex=i;
                packet.mX=board.GetSeedPacketPositionX(i);packet.mY=0;
            }
            for(unsigned int i=0;i<fingers;++i)
                manager.TouchBegan({777+i,int(board.mSeedBank->mSeedPackets[i].mX)+20,30});
            Require(manager.mTouchCount==fingers,"Benchmark did not capture every finger");
            context.touches=fingers;
            const char* label=mousePresent ? (fingers==1?"one_touch_with_mouse":"ten_touch_with_mouse") :
                (fingers==1?"one_touch_move_update":"ten_touch_move_update");
            measure(label,fingers==1?30000:10000,
                [](Context& c,unsigned int frame){
                    for(unsigned int i=0;i<c.touches;++i)
                        c.manager->TouchMoved({777+i,430+int(frame&7),350});
                    c.board->UpdateTouchCursors();
                });
            manager.CancelTouches(&board);
        }
    }
}

void StorePauseClockRuntime(TestApp& app, DecodeOnlySoundManager& sounds)
{
    std::cerr<<"Store clock fixture: initialize\n";
    ReanimatorLoadDefinitions(gLawnReanimationArray,static_cast<int>(ReanimationType::NUM_REANIMS));
    struct FreeCatalog { ~FreeCatalog() { TodParticleFreeDefinitions(); ReanimatorFreeDefinitions(); } } freeCatalog;
    TodParticleLoadDefinitions(gLawnParticleArray,ParticleEffect::NUM_PARTICLES);
    EffectSystem effects;
    effects.EffectSystemInitialize();
    PlayerInfo profile;
    profile.mLevel=50;
    DummyMusicInterface silentMusicInterface;
    Music music;
    music.SetMusicDisabled(true);
    TodFoleyInitialize(gLawnFoleyParamArray,NUM_FOLEY);
    TodFoley foley;
    ReanimatorCache cache;
    cache.ReanimatorCacheInitialize();
    for (const auto seed:{SEED_GATLINGPEA,SEED_TWINSUNFLOWER,SEED_GLOOMSHROOM,SEED_CATTAIL})
        cache.mPlantImages[seed]=cache.MakeCachedPlantFrame(seed,VARIATION_NORMAL);
    struct RestoreWorld {
        TestApp& app;
        DecodeOnlySoundManager& sounds;
        PlayerInfo* profile;
        Music* music;
        TodFoley* foley;
        MusicInterface* musicInterface;
        ReanimatorCache* cache;
        GameScenes scene;
        std::uint64_t interval=Sexy::gUpdateIntervalNs;
        double ticks=Sexy::gUpdateTicks, steps=Sexy::gStepsPerLegacyTick;
        ~RestoreWorld() {
            app.KillStoreScreen();
            app.ProcessSafeDeleteList();
            app.mBoard=nullptr;
            app.mEffectSystem=nullptr;
            app.mPlayerInfo=profile;
            app.mMusic=music;
            app.mSoundSystem=foley;
            app.mMusicInterface=musicInterface;
            app.mReanimatorCache=cache;
            app.mGameScene=scene;
            sounds.allowSilentPlayback=false;
            Sexy::gUpdateIntervalNs=interval;
            Sexy::gUpdateTicks=ticks;
            Sexy::gStepsPerLegacyTick=steps;
        }
    } restore{app,sounds,app.mPlayerInfo,app.mMusic,app.mSoundSystem,app.mMusicInterface,app.mReanimatorCache,app.mGameScene};
    app.mEffectSystem=&effects;
    app.mPlayerInfo=&profile;
    app.mMusic=&music;
    app.mSoundSystem=&foley;
    app.mMusicInterface=&silentMusicInterface;
    app.mReanimatorCache=&cache;
    sounds.allowSilentPlayback=true;
    if(gTouchOnly) {
        FixedUIAtlas::Load();
        struct FreeTouchAtlas { ~FreeTouchAtlas(){FixedUIAtlas::Free();} } freeTouchAtlas;
        const auto source=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/"assets/runtime/images/touch_selection_frame_blue.png";
        auto* decoded=ImageLib::GetImage(source.string(),false);
        Require(decoded&&decoded->mBits,"Cannot decode independent original TV frame");
        ImageLib::FixTransparentEdges(*decoded); // Match the original ResourceManager upload preparation.
        Image frame;frame.SetBits(decoded->mBits,decoded->mWidth,decoded->mHeight);delete decoded;
        gTouchFrameReference=&frame;
        TouchInputRuntime(app);TouchGardenRuntime(app);gTouchFrameReference=nullptr;return;
    }
    if(gPlantCatalogOnly) {PlantAttackCatalogRuntime(app);return;}
    if(gZombieCatalogOnly) {ZombieBehaviorCatalogRuntime(app);return;}
    if(gMinigameRulesOnly) {MinigameBehaviorRuntime(app);MinigameRulesRuntime(app);return;}
    if(gPuzzleRulesOnly) {PuzzleRulesRuntime(app);return;}
    if(gSurvivalRulesOnly) {SurvivalRulesRuntime(app);return;}
    if(gAdventureRulesOnly) {AdventureRulesRuntime(app);return;}
    if(gHiddenRulesOnly) {HiddenRulesRuntime(app,sounds);return;}
    if(gGardenRulesOnly) {GardenRulesRuntime(app);return;}
    if(gIntroRulesOnly) {IntroRulesRuntime(app,sounds);return;}
    if(gUiRulesOnly) {AlmanacRulesRuntime(app);AwardRulesRuntime(app);return;}
    if(gAwardRulesOnly) {AwardRulesRuntime(app);return;}
    if(gAlmanacClockOnly) {AlmanacRulesRuntime(app,true);return;}
    if(gActorBirthOnly) {ActorBirthClockRuntime(app);return;}
    if(gLossRulesOnly) {LossRulesRuntime(app);return;}
    if(gOpeningRulesOnly) {CheckOpeningFlows(app,app.fixtureNestedPump,[](LawnApp& a){static_cast<TestApp&>(a).ProcessSafeDeleteList();});return;}
    if(gLifecycleRulesOnly) {LateLifecycleRuntime(app);return;}
    if(gGraveRulesOnly) {GraveSpawnRulesRuntime(app);return;}
    if(gHudSeamsOnly) {HudAndGraveSeamsRuntime(app);AtlasTransparentEdges(app);return;}
    AtlasTransparentEdges(app);
    FrameDivisionSampleRuntime();
    ExplicitReanimationOwnerTime(app);
    BoardlessPlantPreviewRuntime(app);
    CreditsMovieOwnerClockRuntime(app);
    for (const int hz:{100,240,480,1000}) SurvivalOwnerClockRuntime(app,hz);
    for (const int hz:{100,240,480,1000}) PoolSparkleOwnerClockRuntime(app,hz);
    MinigameBehaviorRuntime(app);
    PlantAttackCatalogRuntime(app);
    ZombieBehaviorCatalogRuntime(app);
    MinigameRulesRuntime(app);
    PuzzleRulesRuntime(app);
    SurvivalRulesRuntime(app);
    AdventureRulesRuntime(app);
    HiddenRulesRuntime(app,sounds);
    GardenRulesRuntime(app);
    IntroRulesRuntime(app,sounds);
    LossRulesRuntime(app);
    AlmanacRulesRuntime(app);
    ActorBirthClockRuntime(app);
    AwardRulesRuntime(app);
    CheckOpeningFlows(app,app.fixtureNestedPump,[](LawnApp& a){static_cast<TestApp&>(a).ProcessSafeDeleteList();});
    LateLifecycleRuntime(app);
    GraveSpawnRulesRuntime(app);
    Board board(&app);
    std::cerr<<"Store clock fixture: board initialized\n";
    app.mGameScene=GameScenes::SCENE_PLAYING;
    PauseDialogWallClockRuntime(app,board);
    InitialGraveAppearanceRuntime(app,board);
    // The opening title owns a Visual deadline from creation, even while
    // Board time is zero; switching to battle must not rebase that deadline.
    for (const int hz:{30,100,240,1000})
    {
        MessageWidget advice(&app);
        const auto savedTime=board.mLevelTimeNs;
        auto& visual=effects.mReanimationHolder->mVisualTimeNs;
        const auto savedVisual=visual;
        board.mLevelTimeNs=0;
        visual=17'000'000'000LL;
        advice.SetLabel("Pogo Party",MessageStyle::MESSAGE_STYLE_HOUSE_NAME);
        Require(advice.mEndTimeNs==19'500'000'000LL,"Opening title was not anchored to Visual creation time");
        const auto deadline=advice.mEndTimeNs;
        auto elapsed=0LL;
        const auto step=1'000'000'000LL/hz;
        while (elapsed<2'500'000'000LL)
        {
            const auto dt=std::min(step,2'500'000'000LL-elapsed);
            Sexy::gUpdateIntervalNs=dt;
            board.mPaused=true;
            advice.Update(visual);
            Require(advice.mEndTimeNs==deadline,
                "Paused opening title consumed its duration");
            board.mPaused=false;
            const bool battle=elapsed>=1'000'000'000LL;
            if (battle) board.mLevelTimeNs+=dt;
            visual+=dt;
            advice.Update(visual);
            elapsed+=dt;
            Require(advice.IsBeingDisplayed()==(elapsed<2'500'000'000LL),
                "Opening title duration changed across intro/battle clocks");
        }
        Require(advice.mMessageStyle==MessageStyle::MESSAGE_STYLE_OFF,
            "Opening title expiry did not clear its style");
        board.mLevelTimeNs=0;
        advice.SetLabel("Pogo Party",MessageStyle::MESSAGE_STYLE_HOUSE_NAME);
        const auto offset=offsetof(MessageWidget,mEndTimeNs);
        const auto wire=SaveClockProjection::Encode(advice,offset,advice.mEndTimeNs);
        Require(wire==2'500'000'000LL,"Opening title saved an absolute Visual timestamp");
        visual+=7'000'000'000LL;
        advice.mEndTimeNs=SaveClockProjection::Decode(advice,offset,wire);
        SaveClockProjection::Bind(advice);
        Require(advice.mEndTimeNs==visual+2'500'000'000LL,"Opening title did not rebind to restored Visual time");
        Sexy::gUpdateIntervalNs=2'500'000'000ULL;
        visual+=2'500'000'000LL;
        advice.Update(visual);
        Require(!advice.IsBeingDisplayed() && advice.mMessageStyle==MessageStyle::MESSAGE_STYLE_OFF,
            "Opening title could not expire entirely inside the intro at time zero");
        advice.SetLabel("Stay",MessageStyle::MESSAGE_STYLE_HINT_STAY);
        advice.Update(0);
        Require(advice.mEndTimeNs==INT64_MAX,
            "Inactive battle clock consumed an indefinite advice message");
        board.mLevelTimeNs=savedTime;
        visual=savedVisual;
    }
    Sexy::gUpdateIntervalNs=restore.interval;
    PottedGiftPickupRuntime(app,board);
    auto& holder=*effects.mReanimationHolder;
    auto* background=app.AddReanimation(0,0,0,REANIM_PEASHOOTER, app.mEffectSystem->mReanimationHolder->mVisualTimeNs, ReanimTimeDomain::Visual);
    background->PlayReanim(ReanimTrackId::anim_idle,REANIM_LOOP,0,12);
    auto* battle=app.AddReanimation(0,0,0,REANIM_PEASHOOTER, app.mEffectSystem->mReanimationHolder->mBattleTimeNs, ReanimTimeDomain::Battle);
    battle->PlayReanim(ReanimTrackId::anim_idle,REANIM_LOOP,0,12);
    holder.BindAnimationTime(*battle,ReanimTimeDomain::Battle, app.mEffectSystem->mReanimationHolder->mBattleTimeNs);
    auto& render=*app.mDDInterface->mRenderInterface;
    Image* target=render.CreateRenderImage(800,600);
    struct FreeTarget { Image* image; ~FreeTarget() { delete image; } } freeTarget{target};
    for (const int hz:{100,240,480,1000})
    {
        std::cerr<<"Store clock fixture: "<<hz<<" Hz ordinary pause\n";
        Sexy::gUpdateIntervalNs=1'000'000'000ULL/hz;
        Sexy::gUpdateTicks=100.0/hz;
        Sexy::gStepsPerLegacyTick=hz/100.0;
        const auto phase=background->SamplePlayback().phaseNs;
        const auto battleTime=holder.mBattleTimeNs;
        board.Pause(true);
        for (int frame=0;frame<hz;++frame) app.UpdateFrames();
        Require(background->SamplePlayback().phaseNs==phase && holder.mBattleTimeNs==battleTime,
            "Ordinary pause advanced the background pose or battle clock");
        board.Pause(false);
        background->UpdateAt(holder.mVisualTimeNs);
        Require(background->SamplePlayback().phaseNs==phase,
            "Resuming an ordinary pause consumed elapsed UI time");
        for (const bool inLevel:{false,true})
        {
            std::cerr<<"Store clock fixture: opening "<<(inLevel ? "level" : "menu")<<" Store\n";
            app.mBoard=inLevel ? &board : nullptr;
            auto* store=app.ShowStoreScreen();
            std::cerr<<"Store clock fixture: drawing Store\n";
            Require(board.mPaused==inLevel,"Store modal did not preserve the expected board pause");
            {
                GpuImageDrawScope destination(render,target);
                Graphics graphics(target);
                store->Draw(&graphics);
            }
            std::cerr<<"Store clock fixture: drawing completed, updating entry\n";
            for (int frame=0;frame<3*hz;++frame)
            {
                try { app.UpdateFrames(); }
                catch (...) { std::cerr<<"Store entry update failed at frame "<<frame<<'\n'; throw; }
            }
            Require(app.mCrazyDaveState!=CRAZY_DAVE_ENTERING && app.mCrazyDaveState!=CRAZY_DAVE_OFF,
                "Store Crazy Dave did not complete his entry animation");
            std::cerr<<"Store clock fixture: entry completed, navigating\n";
            for (const int button:{store->mNextButton->mId,store->mPrevButton->mId})
            {
                const auto previousPage=store->mPage;
                store->ButtonDepress(button);
                Require(store->mPage!=previousPage && store->mHatchTimer>0,
                    "Store page button did not start its hatch transition");
                for (int frame=0;frame<hz;++frame) app.UpdateFrames();
                Require(store->mHatchTimer==0 && !store->mPrevButton->mDisabled &&
                    !store->mNextButton->mDisabled,"Store hatch remained closed after page navigation");
            }
            constexpr int nestedDialogId=41000;
            std::cerr<<"Store clock fixture: navigation completed, removing Store\n";
            if (inLevel)
            {
                auto* nested=new Dialog(nullptr,nullptr,nestedDialogId,true,"","","",Dialog::BUTTONS_NONE);
                nested->Resize(0,0,100,100);
                app.AddDialog(nested);
            }
            app.KillStoreScreen();
            std::cerr<<"Store clock fixture: Store removed\n";
            app.ProcessSafeDeleteList();
            if (inLevel)
            {
                Require(board.mPaused && background->SamplePlayback().phaseNs==phase,
                    "Removing Store under another modal did not restore the paused background pose");
                for (int frame=0;frame<hz;++frame) app.UpdateFrames();
                Require(background->SamplePlayback().phaseNs==phase,
                    "Background pose advanced under the modal remaining after Store closed");
                app.KillDialog(nestedDialogId);
                app.ProcessSafeDeleteList();
                background->UpdateAt(holder.mVisualTimeNs);
                battle->UpdateAt(holder.mBattleTimeNs);
                Require(background->SamplePlayback().phaseNs==phase && holder.mBattleTimeNs==battleTime,
                    "Store resume advanced a paused background animation or battle clock");
            }
            Require(!board.mPaused,"Closing the Store and its modal left the board paused");
            // The menu case deliberately had no board. Consume its visual time
            // before measuring the next paused in-level interval.
            background->RebaseTime(holder.mVisualTimeNs);
        }
        app.mBoard=&board;
        std::cout<<"PASS Store clocks "<<hz<<" Hz: menu/level Dave entry, next/prev hatch, pause/resume poses and battle clock\n";
    }
}

void ProjectileAtlasPixels(LawnApp& app)
{
    auto& render=*app.mDDInterface->mRenderInterface;
    const auto& definition=GeneratedDefinitions::Projectiles;
    ProjectileLoadResources();
    struct FreeResources {
        ~FreeResources() { ProjectileFreeResources(); DefinitionFreeAtlasImages(); }
    } freeResources;
    ImageLib::Image* decoded=ImageLib::GetImage("compiled/atlases/projectiles.atlas.png",false);
    // Independent pre-migration packed pixel layout. The new program data must
    // normalize these exact regions, not redefine its own pixel oracle.
    constexpr Rect bodyRegions[]{
        {156,102,28,28},{168,44,28,28},{210,2,30,27},{38,84,55,48},
        {2,104,32,27},{97,84,55,48},{0,0,0,0},{168,2,38,38},
        {2,84,30,16},{210,33,31,31},{168,76,21,22},{2,2,162,78},
        {193,81,49,50},{156,102,28,28}};
    constexpr Rect shadowRegions[]{{200,68,21,9},{221,68,21,9}};
    Require(decoded && decoded->mBits && decoded->mWidth==244 &&
        decoded->mHeight==134,"Cannot decode projectile atlas oracle");
    struct FreeDecoded { ImageLib::Image* image; ~FreeDecoded() { delete image; } } freeDecoded{decoded};
    EffectSystem effects;
    effects.EffectSystemInitialize();
    app.mEffectSystem=&effects;
    PlayerInfo profile;
    profile.mLevel=1;
    auto* previousProfile=app.mPlayerInfo;
    app.mPlayerInfo=&profile;
    struct RestoreWorld {
        LawnApp& app;
        PlayerInfo* previousProfile;
        ~RestoreWorld() {
            app.mBoard=nullptr;
            app.mEffectSystem=nullptr;
            app.mPlayerInfo=previousProfile;
        }
    } restoreWorld{app,previousProfile};
    Board board(&app);
    Projectile projectile;
    projectile.mAttachmentID=AttachmentID::ATTACHMENTID_NULL;
    projectile.ProjectileInitialize(120,120,0,0,ProjectileType::PROJECTILE_PEA);
    Require(projectile.mWidth==40 && projectile.mHeight==40,
        "Pea initialization changed collision/save dimensions");
    projectile.ProjectileInitialize(120,120,0,0,ProjectileType::PROJECTILE_COBBIG);
    Require(projectile.mWidth==162 && projectile.mHeight==78,
        "Cob initialization lost the original unscaled collision dimensions");
    projectile.mX=100;projectile.mY=100;
    projectile.mPosX=100.375f;projectile.mPosY=100.625f;projectile.mPosZ=-3.25f;
    projectile.mShadowY=130;
    projectile.mOnHighGround=false;
    projectile.mRow=0;
    constexpr int width=360,height=300;
    Image* target=render.CreateRenderImage(width,height);
    struct FreeTarget { Image* image; ~FreeTarget() { delete image; } } freeTarget{target};
    const auto sourceImage=[&](Image& image,const Rect& source) {
        if (!source.mWidth) return;
        std::vector<unsigned> pixels(source.mWidth*source.mHeight);
        for (int y=0;y<source.mHeight;++y)
            std::copy_n(decoded->mBits+(source.mY+y)*decoded->mWidth+source.mX,
                source.mWidth,pixels.data()+y*source.mWidth);
        image.SetBits(pixels.data(),source.mWidth,source.mHeight);
    };
    const auto compare=[&](auto&& actual,auto&& reference,const char* label,
        const SexyMatrix3* nearestTransform,const Rect* nearestSource,const Color& sampleColor) {
        const auto draw=[&](auto&& operation) {
            {
                GpuImageDrawScope destination(render,target);
                Graphics graphics(target);
                graphics.mColor=Color::Black;
                graphics.FillRect(Rect(0,0,width,height));
                graphics.TranslateF(110.25f,90.5f);
                graphics.mColorizeImages=true;
                graphics.mColor=Color(160,210,120,230);
                operation(graphics);
            }
            render.Flush();
            return render.ReadImagePixels(*target);
        };
        const auto expected=draw(reference);
        const auto result=draw(actual);
        unsigned boundaryDifferences=0;
        int maximumDifference=0;
        for (int i=0;i<width*height;++i)
            try { Pixel(result,i,expected.pixels[i],2); }
            catch (...) {
                bool boundaryMatch=false;
                if (nearestTransform && nearestSource)
                {
                    const auto& m=*nearestTransform;
                    const float determinant=m.m00*m.m11-m.m01*m.m10;
                    const float ix=m.m11/determinant, iy=-m.m01/determinant;
                    const float jx=-m.m10/determinant, jy=m.m00/determinant;
                    const float dx=i%width+0.5f-m.m02,dy=i/width+0.5f-m.m12;
                    const float sx=ix*dx+iy*dy+nearestSource->mWidth*0.5f;
                    const float sy=jx*dx+jy*dy+nearestSource->mHeight*0.5f;
                    // Rasterized vertices have subpixel precision. At most
                    // two 1/256-pixel steps, mapped back through this matrix,
                    // bound a nearest-sampling texel-boundary ambiguity.
                    const float ex=(std::abs(ix)+std::abs(iy))/128.0f+0.0001f;
                    const float ey=(std::abs(jx)+std::abs(jy))/128.0f+0.0001f;
                    const bool edgeX=std::abs(sx-std::round(sx))<=ex;
                    const bool edgeY=std::abs(sy-std::round(sy))<=ey;
                    if ((edgeX || edgeY) && sx>=-ex && sy>=-ey &&
                        sx<=nearestSource->mWidth+ex && sy<=nearestSource->mHeight+ey)
                    {
                        for (int cy=0;cy<(edgeY ? 2 : 1);++cy)
                            for (int cx=0;cx<(edgeX ? 2 : 1);++cx)
                            {
                                const int x=edgeX ? int(std::round(sx))-1+cx : int(std::floor(sx));
                                const int y=edgeY ? int(std::round(sy))-1+cy : int(std::floor(sy));
                                unsigned candidate=0xff000000u;
                                if (x>=0 && y>=0 && x<nearestSource->mWidth && y<nearestSource->mHeight)
                                {
                                    const unsigned pixel=decoded->mBits[
                                        (nearestSource->mY+y)*decoded->mWidth+nearestSource->mX+x];
                                    const float alpha=float(pixel>>24)*sampleColor.mAlpha/(255.0f*255.0f);
                                    const auto channel=[&](int shift,int tint) {
                                        return unsigned(std::lround(float((pixel>>shift)&255)*tint/255.0f*alpha));
                                    };
                                    candidate|=channel(16,sampleColor.mRed)<<16 |
                                        channel(8,sampleColor.mGreen)<<8 | channel(0,sampleColor.mBlue);
                                }
                                bool matches=true;
                                for (int shift:{0,8,16,24})
                                    matches&=std::abs(int((result.pixels[i]>>shift)&255)-
                                        int((candidate>>shift)&255))<=2;
                                boundaryMatch|=matches;
                            }
                    }
                }
                if (boundaryMatch)
                {
                    ++boundaryDifferences;
                    for (int shift:{0,8,16,24})
                        maximumDifference=std::max(maximumDifference,
                            std::abs(int((result.pixels[i]>>shift)&255)-
                                int((expected.pixels[i]>>shift)&255)));
                    continue;
                }
                std::cerr<<label<<" type="<<int(projectile.mProjectileType)
                    <<" rotation="<<projectile.mRotation
                    <<" motion="<<int(projectile.mMotionType)
                    <<" creationTimeNs="<<projectile.mCreationTimeNs
                    <<" pixel="<<i%width<<","<<i/width
                    <<" actual="<<std::hex<<result.pixels[i]
                    <<" expected="<<expected.pixels[i]<<std::dec<<'\n';
                throw;
            }
        if (boundaryDifferences)
            std::cout<<"Nearest texel boundaries type="<<int(projectile.mProjectileType)
                <<" rotation="<<projectile.mRotation<<" motion="<<int(projectile.mMotionType)
                <<" creationTimeNs="<<projectile.mCreationTimeNs<<" pixels="<<boundaryDifferences
                <<" maxChannelDelta="<<maximumDifference
                <<" (all match adjacent CPU texels; all other pixels exact within2)\n";
    };
    for (int type=0;type<14;++type)
    {
        projectile.mProjectileType=static_cast<ProjectileType>(type);
        const auto& visual=definition.mProjectiles[type];
        const auto& oldRegion=bodyRegions[type];
        Require(visual.mSource.mWidth==oldRegion.mWidth &&
            visual.mSource.mHeight==oldRegion.mHeight &&
            visual.mSource.mU0==float(oldRegion.mX)/244 &&
            visual.mSource.mV0==float(oldRegion.mY)/134 &&
            visual.mSource.mU1==float(oldRegion.mX+oldRegion.mWidth)/244 &&
            visual.mSource.mV1==float(oldRegion.mY+oldRegion.mHeight)/134,
            "Generated projectile UV differs from the preserved pixel layout");
        Image body(&app);
        sourceImage(body,oldRegion);
        for (const std::int64_t ageNs:{0LL,123'456'789LL,300'000'000LL})
            for (const bool rotated:{false,true})
                for (const bool mirror:{false,true})
                {
                    projectile.mCreationTimeNs=
                        static_cast<std::int64_t>(board.mLevelTimeNs)-ageNs;
                    projectile.mRotation=rotated ? 0.37f : 0.0f;
                    projectile.mMotionType=mirror ? ProjectileMotion::MOTION_BEE_BACKWARDS :
                        ProjectileMotion::MOTION_STRAIGHT;
                    float scale=visual.mScale;
                    if (type==int(ProjectileType::PROJECTILE_PUFF))
                        scale=TodAnimateCurveNs(0,300'000'000,ageNs,0.3f,1.0f,Curves::Linear)*(visual.mScale/0.3f);
                    SexyTransform2D nearestTransform;
                    TodScaleRotateTransformMatrix(nearestTransform,
                        projectile.mPosX-projectile.mX+body.mWidth*0.5f+110.25f,
                        projectile.mPosZ+projectile.mPosY-projectile.mY+body.mHeight*0.5f+90.5f,
                        projectile.mRotation,scale,scale);
                    const bool nearest=rotated || !FloatApproxEqual(scale,1);
                    compare([&](Graphics& g) { projectile.Draw(&g); },[&](Graphics& g) {
                        if (!visual.mSource.mWidth) return;
                        const Rect source(0,0,body.mWidth,body.mHeight);
                        if (!rotated && FloatApproxEqual(scale,1))
                            g.DrawImageMirror(&body,Rect(0,0,body.mWidth,body.mHeight),source,mirror);
                        else {
                            SexyTransform2D transform;
                            TodScaleRotateTransformMatrix(transform,
                                projectile.mPosX-projectile.mX+body.mWidth*0.5f+g.mTransX,
                                projectile.mPosZ+projectile.mPosY-projectile.mY+body.mHeight*0.5f+g.mTransY,
                                projectile.mRotation,scale,scale);
                            TodBltMatrix(&g,&body,transform,g.mClipRect,Color::White,g.mDrawMode,source);
                        }
                    },"Projectile body atlas differs from standalone legacy draw",
                        nearest ? &nearestTransform : nullptr,&oldRegion,Color::White);
                }
        static constexpr struct { float x,y,scale,stretch; bool skip; } shadows[]{
            {3,0,1,1,false},{-1,0,1.3f,1,false},{3,10,1.6f,1,false},
            {3,10,1.6f,1,false},{0,0,1,1,true},{3,10,1.6f,1,false},
            {0,0,1.4f,1,false},{7,0,1,1,false},{0,0,1,1,false},
            {0,0,1,1,false},{3,10,1.6f,1,false},{57,0,1,3,false},
            {3,10,1.6f,1,false},{3,0,1,1,false}};
        for (int night=0;night<2;++night)
            for (bool lobbed:{false,true})
            {
                board.mBackground=night ? BackgroundType::BACKGROUND_2_NIGHT :
                    BackgroundType::BACKGROUND_1_DAY;
                projectile.mMotionType=lobbed ? ProjectileMotion::MOTION_LOBBED :
                    ProjectileMotion::MOTION_STRAIGHT;
                Image shadow(&app);
                sourceImage(shadow,shadowRegions[night]);
                const auto& params=shadows[type];
                const float scale=params.scale*(lobbed ? 200.0f/203.25f : 1.0f);
                SexyTransform2D nearestTransform;
                TodScaleRotateTransformMatrix(nearestTransform,
                    projectile.mPosX-projectile.mX+params.x+110.25f+shadow.mWidth*0.5f*scale*params.stretch,
                    projectile.mShadowY-projectile.mY+params.y+90.5f+shadow.mHeight*0.5f*scale,
                    0,scale*params.stretch,scale);
                compare([&](Graphics& g) { projectile.DrawShadow(&g); },[&](Graphics& g) {
                    if (params.skip) return;
                    TodDrawImageRectScaledF(&g,&shadow,
                        projectile.mPosX-projectile.mX+params.x,
                        projectile.mShadowY-projectile.mY+params.y,
                        Rect(0,0,shadow.mWidth,shadow.mHeight),scale*params.stretch,scale);
                },"Projectile shadow atlas differs from standalone legacy draw",
                    (scale*params.stretch!=1 || scale!=1) ? &nearestTransform : nullptr,
                    &shadowRegions[night],Color(160,210,120,230));
            }
    }
    std::cout<<"PASS actual projectile body/shadow atlas pixels: all14 types, both shadow cels, "
        "source offsets, scale, rotation, mirrors, puff ages, lobbed shadows and collision dimensions\n";
}

void PoolScenePixels(SexyAppBase& app)
{
    auto& render=*app.mDDInterface->mRenderInterface;
    PoolEffect pool;
    pool.PoolEffectInitialize();
    for (const long long step : {33'333'333LL,10'000'000LL,4'166'666LL,1'000'000LL})
    {
        pool.mBirthTimeNs=pool.mSampleTimeNs=0;
        long long now=0,expectedAge=0;
        for (int i=0;i<200;++i)
        {
            const auto actual=step+i%3;
            now+=actual;
            const bool frozen=i%17<5;
            pool.PoolEffectUpdate(now,frozen?0:actual);
            if (!frozen) expectedAge+=actual;
            Require(pool.mSampleTimeNs-pool.mBirthTimeNs==expectedAge,
                "Pool birth-time phase drifted through jitter/freeze");
            if (i%11==0)
            {
                pool.PoolEffectUpdate(now,actual);
                Require(pool.mSampleTimeNs-pool.mBirthTimeNs==expectedAge,
                    "Two pool owners advanced the same timestamp twice");
            }
        }
        // Ice/repick/paused paths skip the update entirely. Resume must translate
        // birth by that gap instead of adding it to the visual phase.
        const auto birthBefore=pool.mBirthTimeNs;
        now+=3'000'000'000LL;
        pool.PoolEffectUpdate(now,0);
        Require(pool.mBirthTimeNs==birthBefore+3'000'000'000LL &&
            pool.mSampleTimeNs-pool.mBirthTimeNs==expectedAge,
            "Pool freeze gap was not absorbed by birth");
        now+=step;
        pool.PoolEffectUpdate(now,step);expectedAge+=step;
        Require(pool.mSampleTimeNs-pool.mBirthTimeNs==expectedAge,
            "Pool resume jumped phase");
        now+=17'000'000'000LL+step;
        pool.PoolEffectUpdate(now,step);expectedAge+=step;
        Require(pool.mSampleTimeNs-pool.mBirthTimeNs==expectedAge,
            "Pool inherited inactive scene time when changing owner");
        now=123;
        pool.PoolEffectUpdate(now,0);
        Require(pool.mSampleTimeNs-pool.mBirthTimeNs==expectedAge,
            "Pool clock epoch rebind changed phase");
        const auto oldBirth=pool.mBirthTimeNs,oldSample=pool.mSampleTimeNs;
        bool rejected=false;
        try { pool.PoolEffectUpdate(now,~0ull); }
        catch (const std::overflow_error&) { rejected=true; }
        Require(rejected && pool.mBirthTimeNs==oldBirth && pool.mSampleTimeNs==oldSample,
            "Invalid pool interval mutated the clock sample");
    }
    std::cout<<"PASS pool birth clock: 30/100/240/1000Hz jitter, freeze/resume, duplicate owner, scene gap and epoch rebind\n";
    pool.mSampleTimeNs=970'000'000;
    pool.mBirthTimeNs=0;
    render.SetCausticsTimeNs(970'000'000ull);
    for (int width:{800,1400})
    {
        Image* target=render.CreateRenderImage(width,600);
        struct Cleanup { Image* image; ~Cleanup() { delete image; } } cleanup{target};
        for (bool night:{false,true})
        {
            const auto draw=[&](bool legacy) {
                {
                    GpuImageDrawScope destination(render,target);
                    Graphics g(target);
                    g.mColor=Color::Black;
                    g.FillRect(0,0,width,600);
                    g.mTransX=width==1400 ? 230.f : 0.f;
                    g.mLinearBlend=true;
                    g.DrawImage(night ? IMAGE_BACKGROUND4 : IMAGE_BACKGROUND3,-220,0);
                    if (!legacy) pool.PoolEffectDraw(&g,night);
                    else
                    {
                        Image* layers[]{night ? IMAGE_POOL_BASE_NIGHT : IMAGE_POOL_BASE,
                            night ? IMAGE_POOL_SHADING_NIGHT : IMAGE_POOL_SHADING,
                            pool.mCausticImage};
                        for (unsigned layer=0;layer<3;++layer)
                        {
                            auto p=PoolGpuChecks::Parameters(97,layer,night,GpuBlend::Normal,false,false);
                            p.width=float(IMAGE_POOL->mWidth);
                            p.height=float(IMAGE_POOL->mHeight);
                            p.transX=p.transY=0;
                            p.clip={g.mClipRect.mX,g.mClipRect.mY,g.mClipRect.mWidth,g.mClipRect.mHeight};
                            const auto vertices=PoolGpuChecks::LegacyVertices(p,0);
                            TriVertex triangles[150][3]{};
                            for (unsigned i=0;i<vertices.size();++i)
                            {
                                const auto& v=vertices[i];
                                auto& out=triangles[i/3][i%3];
                                out.x=v.x;out.y=v.y;out.u=v.u;out.v=v.v;
                                out.color=(unsigned(std::lround(v.a*255))<<24)|0xffffffu;
                            }
                            render.sSamplerAddressWrap=layer==2;
                            g.DrawTrianglesTex(layers[layer],triangles,150);
                        }
                        render.sSamplerAddressWrap=false;
                    }
                }
                render.Flush();
                return render.ReadImagePixels(*target);
            };
            const auto expected=draw(true);
            const auto actual=draw(false);
            unsigned maxDelta=0;
            for (std::size_t i=0;i<actual.pixels.size();++i)
                for (unsigned shift:{0,8,16,24})
                    maxDelta=std::max(maxDelta,unsigned(std::abs(
                        int((actual.pixels[i]>>shift)&255)-int((expected.pixels[i]>>shift)&255))));
            std::cout<<"pool scene width="<<width<<" night="<<night<<" max="<<maxDelta<<'\n';
            const auto name="pool-scene-"+std::to_string(width)+(night ? "-night" : "-day");
            Require(ImageLib::WritePNGImage(name+"-legacy.png",width,600,expected.pixels) &&
                ImageLib::WritePNGImage(name+"-gpu.png",width,600,actual.pixels),
                "Cannot write pool scene diagnostic");
            Require(maxDelta<=3,"Actual PoolEffectDraw differs from legacy Graphics triangle path");
        }
    }
}

void SeedPacketCooldownPixels(SexyAppBase& app)
{
    auto& render=*app.mDDInterface->mRenderInterface;
    Require(IMAGE_SEEDS && IMAGE_SEEDS->mTexture,"Cooldown test requires resident production seed texture");
    std::unique_ptr<Image> target(render.CreateRenderImage(80,100));
    const auto capture=[&](float fraction,int grayness) {
        {
            GpuImageDrawScope destination(render,target.get());
            Graphics g(target.get());
            g.mColor=Color::Black;g.FillRect(0,0,80,100);
            // This production packet has no cached plant thumbnail, isolating
            // the actual background and its same cooldown overlay draw path.
            DrawSeedPacket(&g,10,10,SEED_BEGHOULED_BUTTON_SHUFFLE,SEED_NONE,
                fraction,grayness,false,false);
        }
        render.Flush();
        return render.ReadImagePixels(*target).pixels;
    };
    const auto ready=capture(0,128), dark=capture(1,255);
    Require(ready!=dark,"Actual fully dark seed packet did not differ from its ready base");
    std::vector<unsigned int> previous;
    for (const auto [fraction,edge]:{std::pair{.25f,19},std::pair{.5f,36},std::pair{.75f,53}})
    {
        const auto actual=capture(fraction,255);
        unsigned changedTop=0,unchangedBottom=0;
        for (int y=0;y<100;++y)for(int x=0;x<80;++x)
        {
            const bool covered=x>=10 && x<60 && y>=10 && y<10+edge;
            const unsigned expected=covered ? dark[y*80+x] : ready[y*80+x];
            Require(actual[y*80+x]==expected,
                "Actual seed cooldown overlay leaked below its reveal edge or changed the ready portion");
            if(covered && dark[y*80+x]!=ready[y*80+x])++changedTop;
            if(x>=10 && x<60 && y>=10+edge && y<80 && actual[y*80+x]==ready[y*80+x])++unchangedBottom;
        }
        Require(changedTop>0 && unchangedBottom>0,"Cooldown fixture did not cover both dark and ready pixels");
        Require(previous.empty() || previous!=actual,"Different cooldown fractions produced the same full-gray packet");
        previous=actual;
    }
    std::cout<<"PASS actual DrawSeedPacket cooldown: production atlas, quarter/half/three-quarter edges, dark upper and unchanged ready lower pixels\n";
}

void AllResourceGroups(SexyAppBase& app)
{
    DecodeOnlySoundManager sounds;
    ResourceManager manager(&app);
    struct RestoreAppManagers
    {
        SexyAppBase& app;
        ResourceManager& resources;
        ResourceManager* previousResources;
        SoundManager* previousSounds;
        ~RestoreAppManagers()
        {
            TodWarmDispose();
            resources.DeleteResources();
            app.mResourceManager=previousResources;
            app.mSoundManager=previousSounds;
            app.CleanSharedImages();
        }
    } restore{app,manager,app.mResourceManager,app.mSoundManager};
    app.mResourceManager=&manager;
    app.mSoundManager=&sounds;
    Require(manager.ParseResourcesFile("compiled/resources.compiled"),
        "Cannot parse production resource plans for full loading");
    Require(manager.GetNumResources()==RESOURCE_LOAD_PLAN_RECORD_COUNT &&
        manager.GetNumImages()==RESOURCE_LOAD_IMAGE_COUNT &&
        manager.GetNumSounds()==168 && manager.GetNumFonts()==20,
        "Production resource acceptance counts changed");
    unsigned expectedVolumeCalls=0,expectedPanCalls=0,loadedGroups=0;
    std::vector<SharedImageMap::key_type> imageRecipes;
    for (unsigned group=1;group<=RESOURCE_GROUP_COUNT;++group)
    {
        auto* plan=manager.mPendingPlans[group];
        Require(plan!=nullptr,"Production group has no loading plan");
        for (const auto& image:plan->mImages)
        {
            Require(!image.mFromProgram && !image.mUnavailable,
                "Production image coverage requires a loadable image recipe");
            imageRecipes.emplace_back(StringToUpper(image.mPath),image.mVariant);
        }
        sounds.expected=plan->mSounds;
        sounds.nextExpected=0;
        for (const auto& sound:sounds.expected)
        {
            expectedVolumeCalls+=sound.mVolume>=0;
            expectedPanCalls+=sound.mPanning!=0;
        }
        try
        {
            Require(manager.LoadResources(static_cast<ResourceGroupId>(group)),
                "A production resource group failed actual loading");
        }
        catch (...)
        {
            std::cerr<<"Full resource loading failed in group "<<group<<'\n';
            throw;
        }
        Require(!manager.mLoadPlan && !manager.mPendingPlans[group] &&
            sounds.nextExpected==sounds.expected.size(),
            "Completed group retained its plan or skipped sound records");
        ++loadedGroups;
        TodWarmDrainAll();
    }
    sounds.expected.clear();
    Require(loadedGroups==RESOURCE_GROUP_COUNT && sounds.loads==168 && sounds.GetNumSounds()==168 &&
        sounds.volumeCalls==expectedVolumeCalls && sounds.panCalls==expectedPanCalls,
        "Full sound load skipped a resource or its volume/pan setup");
    Require(std::all_of(manager.mPendingPlans.begin(),manager.mPendingPlans.end(),
        [](const auto* plan){return plan==nullptr;}),
        "Full loading retained temporary plan metadata");
    unsigned loadedImages=0;
    for (const auto& recipe:imageRecipes)
    {
        const auto cached=app.mSharedImageMap.find(recipe);
        Require(cached!=app.mSharedImageMap.end() && cached->second.mRefCount.load()>0,
            "Loaded image recipe lacks its counted cache ownership");
        auto* image=cached->second.mImage;
        Require(image && image->mTexture && !image->mPendingDecoded,
            "Loaded image recipe lacks its GPU texture or retains CPU pixels");
        ++loadedImages;
    }
    std::array<Image**,6> namedImages{
        &IMAGE_BLANK,&IMAGE_POPCAP_LOGO,&IMAGE_TITLESCREEN,&IMAGE_SHOVEL,
        &IMAGE_BRAIN,&IMAGE_COINBANK};
    for (auto** image:namedImages)
        Require(*image && (*image)->mTexture && !(*image)->mPendingDecoded,
            "Resource loading failed to publish a named GPU-only image");
    std::array<_Font**,20> fonts{
        &FONT_BRIANNETOD32,&FONT_BRIANNETOD32BLACK,&FONT_BRIANNETOD16,&FONT_BRIANNETOD12,
        &FONT_CONTINUUMBOLD14,&FONT_CONTINUUMBOLD14OUTLINE,&FONT_DWARVENTODCRAFT12,
        &FONT_DWARVENTODCRAFT15,&FONT_DWARVENTODCRAFT18,&FONT_DWARVENTODCRAFT18BRIGHTGREENINSET,
        &FONT_DWARVENTODCRAFT18GREENINSET,&FONT_DWARVENTODCRAFT18YELLOW,&FONT_DWARVENTODCRAFT24,
        &FONT_DWARVENTODCRAFT36BRIGHTGREENINSET,&FONT_DWARVENTODCRAFT36GREENINSET,
        &FONT_HOUSEOFTERROR16,&FONT_HOUSEOFTERROR20,&FONT_HOUSEOFTERROR28,&FONT_PICO129,&FONT_TINYBOLD};
    for (auto** font:fonts)
    {
        auto* bitmap=dynamic_cast<ImageFont*>(*font);
        Require(bitmap && bitmap->mFontData && bitmap->mFontData->mInitialized,
            "A named font global was not loaded");
        for (const auto& layer:bitmap->mFontData->mFontLayerList)
        {
            auto* glyphs=layer.mImage.mImage;
            Require(glyphs && glyphs->mTexture && !glyphs->mPendingDecoded,
                "Loaded font glyphs retained CPU pixels or lacked a GPU texture");
        }
    }
    Require(loadedImages==RESOURCE_LOAD_IMAGE_COUNT,"Full loading skipped an image recipe");
    if(gPlantCatalogOnly||gZombieCatalogOnly||gMinigameRulesOnly||gPuzzleRulesOnly||gSurvivalRulesOnly||gAdventureRulesOnly||gHiddenRulesOnly||gGardenRulesOnly||gIntroRulesOnly||gUiRulesOnly||gAwardRulesOnly||gAlmanacClockOnly||gActorBirthOnly||gLossRulesOnly||gOpeningRulesOnly||gLifecycleRulesOnly||gGraveRulesOnly||gHudSeamsOnly||gTouchOnly)
    {
        StorePauseClockRuntime(static_cast<TestApp&>(app),sounds);
        DefinitionFreeAtlasImages();
        return;
    }
    SeedPacketCooldownPixels(app);
    ProjectileAtlasPixels(static_cast<LawnApp&>(app));
    PoolScenePixels(app);
    FixedUIAtlas::Load();
    StorePauseClockRuntime(static_cast<TestApp&>(app),sounds);
    {
        auto& render=*app.mDDInterface->mRenderInterface;
        Image* target=render.CreateRenderImage(800,600);
        try
        {
            GpuImageDrawScope destination(render,target);
            Graphics graphics(target);
            CheckQuitDialogRuntime(static_cast<LawnApp&>(app),graphics);
            static_cast<TestApp&>(app).ProcessSafeDeleteList();
        }
        catch (...) { delete target; FixedUIAtlas::Free(); throw; }
        delete target;
    }
    FixedUIAtlas::Free();
    DefinitionFreeAtlasImages();
    std::cout<<"PASS actual Quit confirmation: target-free layout, body, yes/no buttons and clean dialog release\n";
    TodWarmDispose();
    manager.DeleteResources();
    manager.DeleteResources();
    Require(sounds.releases==168 && !sounds.invalidReleases && !sounds.GetNumSounds(),
        "Sound shutdown leaked or released a handle twice");
    for (auto** font:fonts) Require(!*font,"Font shutdown left a named global dangling");
    for (auto** image:namedImages)
        Require(!*image,"Image shutdown left a named global dangling");
    app.CleanSharedImages();
    for (const auto& recipe:imageRecipes)
        Require(app.mSharedImageMap.find(recipe)==app.mSharedImageMap.end(),
            "Image shutdown retained a resource owner in the shared cache");
    std::cout<<"PASS all "<<loadedGroups<<" resource groups / "<<RESOURCE_LOAD_PLAN_RECORD_COUNT
        <<" resources: "<<RESOURCE_LOAD_IMAGE_COUNT<<" GPU-only images, "
        "20 named fonts, 168 real sound decodes, volume/pan setup, all plans discarded, "
        "complete release and repeated shutdown\n";
}

void ResourceLoadingOwnership(SexyAppBase& app)
{
    auto& manager=*app.mResourceManager;
    Require(manager.ParseResourcesFile("compiled/resources.compiled"),
        "Cannot parse production NRSC4 resource plans");
    auto title=manager.mPendingPlans[static_cast<unsigned>(ResourceGroupId::LoaderBar)]->mImages[0];
    Require(manager.LoadResources(ResourceGroupId::Init) && IMAGE_BLANK && IMAGE_POPCAP_LOGO,
        "Explicit image group did not bind named globals");
    Require(!manager.mLoadPlan &&
        !manager.mPendingPlans[static_cast<unsigned>(ResourceGroupId::Init)],
        "Completed group retained its loading metadata");
    Require(manager.LoadResources(ResourceGroupId::LoadingFonts) && FONT_HOUSEOFTERROR28,
        "Explicit font group did not bind named globals");
    ResourceManager::FontRes source;
    source.mFont=FONT_HOUSEOFTERROR28;
    ResourceManager::FontRes reference;
    reference.mReference=&source;
    _Font* clone=nullptr;
    Require(manager.LoadFont(reference,clone) && clone && clone!=source.mFont,
        "Temporary font reference failed to clone the named font");
    Require(dynamic_cast<ImageFont*>(clone)->mFontData==
        dynamic_cast<ImageFont*>(source.mFont)->mFontData,
        "Referenced font lost shared FontData ownership");
    manager.ReleaseFont(clone);
    manager.ReleaseFont(clone);
    manager.DeleteResources(ResourceGroupId::Init);
    auto& titlePlan=*manager.mPendingPlans[static_cast<unsigned>(ResourceGroupId::LoaderBar)];
    Require(ResourceLoadBindings::LoadStep(manager,titlePlan,ResourceGroupId::LoaderBar,title.mRecord)
        && IMAGE_TITLESCREEN,
        "Cannot load title image through its generated named owner");
    auto alias=title;
    SharedImageRef aliasOwner;
    Require(manager.LoadImage(alias,IMAGE_BLANK,aliasOwner) && IMAGE_BLANK==IMAGE_TITLESCREEN,
        "Identical image recipes failed to share the cache object");
    auto* shared=IMAGE_BLANK;
    TodWarmDrainAll();
    ReleaseTitleScreenResource(&manager);
    ReleaseTitleScreenResource(&manager);
    Require(!IMAGE_TITLESCREEN && IMAGE_BLANK==shared && aliasOwner.mImage==shared &&
        aliasOwner.mSharedImage && aliasOwner.mSharedImage->mRefCount.load()==1,
        "Title release cleared an independently owned image alias");
    manager.ReleaseImage(IMAGE_BLANK,aliasOwner);
    manager.ReleaseImage(IMAGE_BLANK,aliasOwner);
    Require(!IMAGE_BLANK && !aliasOwner.mImage && !aliasOwner.mSharedImage,
        "Independent alias release retained its named target or cache reference");
    TodWarmDispose();
    app.CleanSharedImages();
    std::cout<<"PASS NRSC4 explicit groups: plan disposal, named image/font ownership, "
        "temporary font clone, title release, independent shared aliases\n";
}

void ResourceLoadingShutdown(SexyAppBase& app)
{
    auto& manager=*app.mResourceManager;
    TodWarmDispose();
    manager.DeleteResources();
    manager.DeleteResources();
    Require(!FONT_HOUSEOFTERROR28 && !manager.mLoadPlan &&
        std::all_of(manager.mPendingPlans.begin(),manager.mPendingPlans.end(),
            [](const auto* plan){return plan==nullptr;}),
        "Shutdown retained named fonts or temporary resource plans");
    app.CleanSharedImages();
    std::cout<<"PASS resource shutdown: all pending plans freed and repeated release safe\n";
}

void ParticleDefinitionsAndFrames(SexyAppBase& app, const char* pakPath)
{
    Require(gPakInterface && gPakInterface->AddPakFile(pakPath),
        "Cannot mount the production particle package");
    AllResourceGroups(app);
    if(gPlantCatalogOnly||gZombieCatalogOnly||gMinigameRulesOnly||gPuzzleRulesOnly||gSurvivalRulesOnly||gAdventureRulesOnly||gHiddenRulesOnly||gGardenRulesOnly||gIntroRulesOnly||gUiRulesOnly||gAwardRulesOnly||gAlmanacClockOnly||gActorBirthOnly||gLossRulesOnly||gOpeningRulesOnly||gLifecycleRulesOnly||gGraveRulesOnly||gHudSeamsOnly||gTouchOnly) return;
    ResourceLoadingOwnership(app);
    ReanimationAppearanceDefinitions(app);
    constexpr ParticleEffect fireEffect=ParticleEffect::PARTICLE_ZOMBIE_BOSS_FIREBALL;
    constexpr ParticleEffect glassesEffect=ParticleEffect::PARTICLE_ZOMBIE_SUNGLASS;
    constexpr ParticleEffect trailEffect=ParticleEffect::PARTICLE_BOSS_ICE_BALL;
    constexpr ParticleEffect selectedEffects[]{fireEffect,glassesEffect,trailEffect};
    // Keep cleanup inside the application's lifetime, including rollback after a failed check.
    try
    {
        bool invalidCatalogRejected = false;
        try {
            ParticleParams missing[]{ {static_cast<ParticleEffect>(0),
                "particles\\ImageRuntimeMissingRoot.xml"} };
            TodParticleLoadDefinitions(missing, 1);
        } catch (const std::exception&) {
            invalidCatalogRejected = true;
        }
        Require(invalidCatalogRejected && !gParticleDefArray && !gParticleDefCount &&
            !gParticleAllDefinitionDataLoaded.load(),
            "Invalid particle catalog published definition state");
        TodParticleLoadDefinitions(gLawnParticleArray,ParticleEffect::NUM_PARTICLES);
        ParticleDrawReference::LoadRenderDefinitions();
        ParticleRuntimeDrawChecks(*app.mDDInterface->mRenderInterface);
        Require(gParticleAllDefinitionDataLoaded.load() &&
            gParticleDefCount==ParticleEffect::NUM_PARTICLES &&
            gParticleDefArray==GeneratedDefinitions::Particles,
            "Public particle loader did not publish the static catalog");
        for (int d=0;d<gParticleDefCount;++d)
        {
            const auto& definition=gParticleDefArray[d];
            Require(definition.mVariantCount>0 &&
                definition.mEmitterDefCount>=0 &&
                (definition.mEmitterDefCount==0 || definition.mEmitterDefs),
                "Static particle catalog has incomplete behavior data");
            for (unsigned v=0;v<definition.mVariantCount;++v)
            {
                bool hasImage=false;
                for (unsigned f=0;f<definition.mImageRectCount;++f)
                    hasImage|=definition.mImageRects[v*definition.mImageRectCount+f].mWidth>0;
                Require(!hasImage || definition.mTextures[v],
                    "Static particle catalog has an unbound atlas");
            }
        }
        constexpr int expectedCounts[]{2,1,2};
        for (int d=0;d<3;++d)
        {
            auto& definition=gParticleDefArray[selectedEffects[d]];
            Require(definition.mVariantCount>0 && definition.mEmitterDefCount==expectedCounts[d],
                "Loaded particle emitter count changed");
            unsigned next=0;
            for (int e=0;e<definition.mEmitterDefCount;++e)
            {
                const auto& emitter=definition.mEmitterDefs[e];
                Require(emitter.mImageRectOffset==next,
                    "Particle rectangle offsets are not consecutive");
                next+=emitter.mImageRectCount;
            }
            Require(next==definition.mImageRectCount,"Particle rectangle row length changed");
            for (unsigned v=0;v<definition.mVariantCount;++v)
            {
                Require(definition.mTextures[v],"Loaded appearance has no resident atlas");
                for (unsigned f=0;f<definition.mImageRectCount;++f)
                {
                    const auto& r=definition.mImageRects[v*definition.mImageRectCount+f];
                    Require(r.mU0>0 && r.mV0>0 && r.mWidth>0 && r.mHeight>0 &&
                        r.mU0<r.mU1 && r.mV0<r.mV1 && r.mU1<1 && r.mV1<1,
                        "Loaded particle frame is outside its resident atlas");
                }
            }
        }
        auto& fire=gParticleDefArray[fireEffect];
        const auto emitterCount=fire.mEmitterDefCount;
        const auto rectangleCount=fire.mImageRectCount;
        const std::vector<TextureRegion> originalRectangles(fire.mImageRects,
            fire.mImageRects+fire.mVariantCount*fire.mImageRectCount);
        const auto& duration=fire.mEmitterDefs[0].mParticleDuration;
        const std::vector<FloatParameterTrackNode> originalDurationNodes(duration.mNodes,
            duration.mNodes+duration.mCountNodes);
        Require(fire.mVariantCount==2,"Fire/ice appearances were published as separate effects");
        for (unsigned v=0;v<2;++v)
        {
            for (int e=0;e<2;++e)
            {
                const auto& emitter=fire.mEmitterDefs[e];
                Require(emitter.mImageFrames==2 && emitter.mImageRectCount==(e==0 ? 4 : 2),
                    "Fire/ice logical frames or available override frames changed");
                for (int f=0;f<emitter.mImageRectCount;++f)
                {
                    const auto& rect=fire.mImageRects[v*fire.mImageRectCount+emitter.mImageRectOffset+f];
                    Require(rect.mWidth==(v==0 ? 112 : 87) && rect.mHeight==(v==0 ? 74 : 46),
                        "Fire/ice variant lost its own source frame dimensions");
                }
            }
        }
        auto& glasses=gParticleDefArray[glassesEffect].mEmitterDefs[0];
        Require(glasses.mImageFrames==4 && glasses.mImageRectCount==4,
            "Sunglasses frames changed");
        const auto& noImage=gParticleDefArray[trailEffect].mEmitterDefs[1];
        Require(noImage.mImageRectCount==0 && noImage.mImageFrames==1,
            "No-image emitter lost its logical frame count");

        // Preserve the pixel rectangles from the pre-UV fixture as an independent
        // oracle. Reference rendering below decodes the packed PNGs and uses the
        // ordinary Image pixel-rectangle path, never the generated UV endpoints.
        const Rect fireRectangles[]{
            {2,2,112,74},{118,2,112,74},{2,80,112,74},{118,80,112,74},
            {2,80,112,74},{118,80,112,74}};
        const Rect iceRectangles[]{
            {2,2,87,46},{2,52,87,46},{2,102,87,46},{2,152,87,46},
            {2,102,87,46},{2,152,87,46}};
        const Rect glassesRectangles[]{
            {2,2,61,48},{67,2,61,48},{2,54,61,48},{67,54,61,48}};
        const Rect snowRectangles[]{{2,202,20,20},{26,202,20,20},{50,202,20,20}};
        Image fireAtlas(&app),iceAtlas(&app),glassesAtlas(&app);
        Image* referenceAtlases[]{&fireAtlas,&iceAtlas,&glassesAtlas};
        const char* referencePaths[]{"compiled/atlases/fireball_burst.atlas.png",
            "compiled/atlases/ice_trail.atlas.png",
            "compiled/particles/zombiefutureglasses.atlas.png"};
        const int referenceWidths[]{232,91,130},referenceHeights[]{156,224,104};
        for (int atlas=0;atlas<3;++atlas)
        {
            ImageLib::Image* decoded=ImageLib::GetImage(referencePaths[atlas],false);
            struct FreeDecoded { ImageLib::Image* image; ~FreeDecoded() { delete image; } } cleanup{decoded};
            Require(decoded && decoded->mBits && decoded->mWidth==referenceWidths[atlas] &&
                decoded->mHeight==referenceHeights[atlas],"Particle pixel oracle PNG dimensions changed");
            referenceAtlases[atlas]->SetBits(decoded->mBits,decoded->mWidth,decoded->mHeight);
        }
        const auto checkUV=[](const TextureRegion& actual,const Rect& pixel,int width,int height) {
            const auto closeUV=[](float left,float right) { return std::abs(left-right)<0.000001f; };
            Require(actual.mWidth==pixel.mWidth && actual.mHeight==pixel.mHeight &&
                closeUV(actual.mU0,float(pixel.mX)/width) &&
                closeUV(actual.mV0,float(pixel.mY)/height) &&
                closeUV(actual.mU1,float(pixel.mX+pixel.mWidth)/width) &&
                closeUV(actual.mV1,float(pixel.mY+pixel.mHeight)/height),
                "Generated particle UVs disagree with the original pixel rectangles");
        };
        for (unsigned f=0;f<6;++f)
        {
            checkUV(fire.mImageRects[f],fireRectangles[f],232,156);
            checkUV(fire.mImageRects[6+f],iceRectangles[f],91,224);
        }
        for (unsigned f=0;f<4;++f)
            checkUV(gParticleDefArray[glassesEffect].mImageRects[f],glassesRectangles[f],130,104);
        for (unsigned f=0;f<3;++f)
            checkUV(gParticleDefArray[trailEffect].mImageRects[f],snowRectangles[f],91,224);

        {
            TodParticleDefinition emptyDefinition;
            emptyDefinition.mVariantCount=1;
            TodParticleData data;
            data.Initialize();
            TodParticleHolder holder(&data);
            struct PoolCleanup {
                TodParticleData& data;
                ~PoolCleanup() { data.Dispose(); }
            } poolCleanup{data};
            const auto systemCount=holder.mParticleSystems.mSize;
            bool invalidVariantRejected=false;
            try { holder.AllocParticleSystem(0,0,0,fireEffect, 0,2); }
            catch (const std::out_of_range&) { invalidVariantRejected=true; }
            Require(invalidVariantRejected && holder.mParticleSystems.mSize==systemCount,
                "Invalid particle variant consumed a system pool slot");
            auto* empty=holder.AllocParticleSystemFromDef(0,0,0,&emptyDefinition,
                fireEffect, 0);
            Require(empty && empty->mVariant==0 && empty->EmitterRosterCount()==0,
                "Zero-emitter definition failed to initialize");
            auto* fireSystem=holder.AllocParticleSystem(0,0,0,fireEffect, 0,0);
            auto* iceSystem=holder.AllocParticleSystem(0,0,0,fireEffect, 0,1);
            Require(fireSystem && iceSystem && fireSystem->mVariant==0 && iceSystem->mVariant==1 &&
                fireSystem->mParticleDef==iceSystem->mParticleDef &&
                fireSystem->EmitterRosterCount()>0 && iceSystem->EmitterRosterCount()>0,
                "Fire/ice allocation lost the shared root or selected variant");
            Require(fireSystem->EmitterRosterEmitterAt(0)->mEmitterDef==
                iceSystem->EmitterRosterEmitterAt(0)->mEmitterDef,
                "Particle appearance selection copied emitter behavior");
            auto* system=holder.AllocParticleSystem(0,0,0,glassesEffect, 0);
            Require(system && system->EmitterRosterCount()==1,
                "Cannot initialize the real sunglasses particle emitter");
            auto* emitter=system->EmitterRosterEmitterAt(0);
            auto& render=*app.mDDInterface->mRenderInterface;
            std::unique_ptr<Image> target(render.CreateRenderImage(256,256));
            const auto drawAppearance=[&](TodParticleSystem* selected,bool reference) {
                {
                    GpuImageDrawScope destination(render,target.get());
                    Graphics graphics(target.get());
                    graphics.mColor=Color::Black;
                    graphics.FillRect(Rect(0,0,256,256));
                    TodTriangleGroup triangles;
                    auto* selectedEmitter=selected->EmitterRosterEmitterAt(0);
                    if (reference)
                    {
                        const auto v=selected->mVariant;
                        SexyMatrix3 transform;
                        TodScaleRotateTransformMatrix(transform,128,128,0,1,1);
                        const int mode=TestBit(selectedEmitter->mEmitterDef->mParticleFlags,
                            ParticleFlags::PARTICLE_ADDITIVE) ? Graphics::DRAWMODE_ADDITIVE :
                            Graphics::DRAWMODE_NORMAL;
                        triangles.AddTriangle(&graphics,referenceAtlases[v],transform,graphics.mClipRect,
                            Color::White,mode,v==0 ? fireRectangles[0] : iceRectangles[0]);
                    }
                    else
                    {
                        TodParticle particle{};
                        particle.mParticleEmitter=selectedEmitter;
                        selectedEmitter->mFrameOverride=0;
                        ParticleDrawReference::ParticleRenderParams values{};
                        values.mParticleScale=values.mParticleStretch=1;
                        values.mPosX=values.mPosY=128;
                        ParticleDrawReference::RenderParticle(&graphics,&particle,Color::White,&values,&triangles);
                    }
                    triangles.DrawGroup(&graphics);
                }
                render.Flush();
                return render.ReadImagePixels(*target).pixels;
            };
            const auto firePixels=drawAppearance(fireSystem,true);
            const auto icePixels=drawAppearance(iceSystem,true);
            Require(firePixels!=icePixels && drawAppearance(fireSystem,false)==firePixels &&
                drawAppearance(iceSystem,false)==icePixels,
                "Shared fire/ice behavior sampled the wrong variant texture or rectangle row");
            {
                GpuImageDrawScope destination(render,target.get());
                Graphics graphics(target.get());
                graphics.mColor=Color::Black;
                graphics.FillRect(Rect(0,0,256,256));
                TodParticleEmitter noImageEmitter{};
                noImageEmitter.mEmitterDef=const_cast<TodEmitterDefinition*>(&noImage);
                TodParticle noImageParticle{};
                noImageParticle.mParticleEmitter=&noImageEmitter;
                ParticleDrawReference::ParticleRenderParams values{};
                TodTriangleGroup triangles;
                ParticleDrawReference::RenderParticle(&graphics,&noImageParticle,Color::White,&values,&triangles);
                empty->Draw(&graphics);
                triangles.DrawGroup(&graphics);
            }
            render.Flush();
            const auto noImagePixels=render.ReadImagePixels(*target).pixels;
            Require(std::all_of(noImagePixels.begin(),noImagePixels.end(),
                [](auto pixel){ return pixel==0xff000000u; }),
                "No-image or zero-emitter particle wrote pixels");
            TodParticle particle{};
            particle.mParticleEmitter=emitter;
            particle.mImageFrame=0;
            const auto draw=[&](bool reference, int frame, bool clipped=false) {
                {
                    GpuImageDrawScope destination(render,target.get());
                    Graphics graphics(target.get());
                    if (clipped) graphics.mLinearBlend=false;
                    graphics.mColor=Color::Black;
                    graphics.FillRect(Rect(0,0,256,256));
                    TodTriangleGroup triangles;
                    if (reference)
                    {
                        SexyMatrix3 transform;
                        TodScaleRotateTransformMatrix(transform,
                            clipped ? 143.25f : 128.f,clipped ? 134.f : 128.f,0,1,1);
                        triangles.AddTriangle(&graphics,&glassesAtlas,transform,graphics.mClipRect,
                            Color::White,Graphics::DRAWMODE_NORMAL,
                            clipped ? Rect(82,60,38,30) : glassesRectangles[frame]);
                    }
                    else
                    {
                        ParticleDrawReference::ParticleRenderParams values{};
                        values.mParticleScale=values.mParticleStretch=1;
                        values.mPosX=values.mPosY=128;
                        ParticleDrawReference::RenderParticle(&graphics,&particle,Color::White,&values,&triangles);
                    }
                    triangles.DrawGroup(&graphics);
                }
                render.Flush();
                return render.ReadImagePixels(*target).pixels;
            };
            const auto first=draw(true,0), last=draw(true,3);
            Require(first!=last,"Sunglasses source frames do not distinguish frame selection");
            Require(std::any_of(last.begin(),last.end(),[](auto p){return (p&0xFFFFFFu)!=0;}),
                "Particle reference draw produced no visible pixels");
            system->OverrideFrame(ParticleEmitterRanges::All,3);
            Require(emitter->mFrameOverride==3 && draw(false,0)==last,
                "Public sunglasses override selected the wrong atlas rectangle");
            system->OverrideFrame(ParticleEmitterRanges::All,999);
            Require(draw(false,0)==last,"Particle frame upper clamp changed");
            system->OverrideFrame(ParticleEmitterRanges::All,-2);
            Require(draw(false,0)==first,"Particle frame lower clamp changed");
            system->OverrideFrame(ParticleEmitterRanges::All,-1);
            particle.mImageFrame=3;
            Require(draw(false,0)==last,"Particle fixed random frame was not retained");
            const int animated=glasses.mAnimated;
            glasses.mAnimated=1;
            particle.mParticleDurationNs = std::int64_t(101) * 10'000'000;
            particle.mBirthTimeNs = 0 - (990'000'000);
            Require(draw(false,0)==last,"Particle lifetime animation selected the wrong frame");
            particle.mBirthTimeNs = 0 - (0);
            Require(draw(false,0)==first,"Particle lifetime animation did not return to frame zero");
            glasses.mAnimated=animated;
            // 61x48 logical geometry: left=.25 rounds to 15 source pixels,
            // left+right=.375 removes 23; top=.125 removes 6, and
            // top+bottom=.375 removes 18. Position keeps the original float
            // shifts (15.25,6), independently of rounded source coordinates.
            TodEmitterDefinition cropped=glasses;
            auto croppedRender = ParticleDrawReference::RenderData(glasses);
            FloatParameterTrackNode clipNodes[]{
                {0,.25f,.25f,1,1},{0,.125f,.125f,1,1},
                {0,.125f,.125f,1,1},{0,.25f,.25f,1,1}};
            croppedRender.mClipLeft={&clipNodes[0],1};
            croppedRender.mClipRight={&clipNodes[1],1};
            croppedRender.mClipTop={&clipNodes[2],1};
            croppedRender.mClipBottom={&clipNodes[3],1};
            struct RestoreEmitterDefinition {
                TodParticleEmitter* emitter;
                TodEmitterDefinition* original;
                ~RestoreEmitterDefinition() { emitter->mEmitterDef=original; }
            } restoreEmitter{emitter,emitter->mEmitterDef};
            ParticleDrawReference::BindRenderData(cropped,croppedRender);
            emitter->mEmitterDef=&cropped;
            particle.mImageFrame=3;
            Require(draw(false,0,true)==draw(true,3,true),
                "Particle UV crop changed rounded pixels or logical center shifts");
        }
        ParticleDrawReference::ClearRenderDefinitions();
        TodParticleFreeDefinitions();
        Require(!gParticleDefArray && !gParticleDefCount && !gParticleAllDefinitionDataLoaded.load(),
            "Public particle free left published definition state");
        DefinitionFreeAtlasImages();
        TodParticleLoadDefinitions(gLawnParticleArray,ParticleEffect::NUM_PARTICLES);
        Require(gParticleDefArray[fireEffect].mVariantCount==2 &&
            gParticleDefArray[fireEffect].mTextures[1],
            "Repeated particle load lost a root appearance atlas");
        const auto& reloaded=gParticleDefArray[fireEffect];
        Require(reloaded.mEmitterDefCount==emitterCount && reloaded.mImageRectCount==rectangleCount &&
            std::equal(originalRectangles.begin(),originalRectangles.end(),reloaded.mImageRects,
                [](const TextureRegion& a,const TextureRegion& b) {
                    return a.mU0==b.mU0 && a.mV0==b.mV0 && a.mU1==b.mU1 && a.mV1==b.mV1 &&
                        a.mWidth==b.mWidth && a.mHeight==b.mHeight;
                }) &&
            reloaded.mEmitterDefs[0].mParticleDuration.mCountNodes==int(originalDurationNodes.size()) &&
            std::equal(originalDurationNodes.begin(),originalDurationNodes.end(),
                reloaded.mEmitterDefs[0].mParticleDuration.mNodes,
                [](const FloatParameterTrackNode& a,const FloatParameterTrackNode& b) {
                    return a.mTime==b.mTime && a.mLow==b.mLow && a.mHigh==b.mHigh &&
                        a.mCurveType==b.mCurveType && a.mDistribution==b.mDistribution;
                }),
            "Repeated compiled particle load changed emitter, geometry or track values");
        ParticleDrawReference::ClearRenderDefinitions();
        TodParticleFreeDefinitions();
        DefinitionFreeAtlasImages();
    }
    catch (...)
    {
        ParticleDrawReference::ClearRenderDefinitions();
        TodParticleFreeDefinitions();
        DefinitionFreeAtlasImages();
        throw;
    }
    ResourceLoadingShutdown(app);
    std::cout<<"PASS static production particles: catalog validation, flat appearance rows, shared behavior, "
        "fire/ice dimensions and pixels, no-image, logical frames, sunglasses override, frame clamps, "
        "animation pixels, repeated load and release\n";
}
void TitleGrassRevealPixels(TestApp& app)
{
    auto& render=*app.mDDInterface->mRenderInterface;
    constexpr unsigned int grassColor=0xff29b74d;
    Image grass, transparent;
    std::vector<unsigned int> grassBits(314*8,grassColor);
    unsigned int clear=0;
    grass.SetBits(grassBits.data(),314,8);
    transparent.SetBits(&clear,1,1);
    EffectSystem effects;
    effects.EffectSystemInitialize();
    struct RestoreTitleWorld {
        TestApp& app;
        EffectSystem* effects;
        Rect bounds;
        Image* title=IMAGE_TITLESCREEN;
        Image* logo=IMAGE_PVZ_LOGO;
        Image* dirt=IMAGE_LOADBAR_DIRT;
        Image* grass=IMAGE_LOADBAR_GRASS;
        Image* cap=IMAGE_REANIM_SODROLLCAP;
        ~RestoreTitleWorld() {
            app.mEffectSystem=effects;
            app.mScreenBounds=bounds;
            IMAGE_TITLESCREEN=title;
            IMAGE_PVZ_LOGO=logo;
            IMAGE_LOADBAR_DIRT=dirt;
            IMAGE_LOADBAR_GRASS=grass;
            IMAGE_REANIM_SODROLLCAP=cap;
        }
    } restore{app,app.mEffectSystem,app.mScreenBounds};
    app.mEffectSystem=&effects;
    app.mScreenBounds=Rect(0,0,800,600);
    IMAGE_TITLESCREEN=IMAGE_PVZ_LOGO=IMAGE_LOADBAR_DIRT=IMAGE_REANIM_SODROLLCAP=&transparent;
    IMAGE_LOADBAR_GRASS=&grass;
    TitleScreen title(&app);
    title.mTitleState=TITLESTATE_SCREEN;
    title.mLoaderScreenIsLoaded=true;
    title.mNeedToInit=false;
    title.mLoadingThreadComplete=false;
    title.mWidth=800;
    title.mHeight=600;
    title.mStartButton->mX=240;
    title.mStartButton->mY=300;
    std::unique_ptr<Image> target(render.CreateRenderImage(800,600));
    for (const Point translation:{Point(0,0),Point(13,9)})
        for (const int width:{0,1,157,313,314,400})
        {
            title.mCurBarWidth=static_cast<float>(width);
            {
                GpuImageDrawScope destination(render,target.get());
                Graphics graphics(target.get());
                graphics.mColor=Color::Black;
                graphics.FillRect(Rect(0,0,800,600));
                graphics.Translate(translation.mX,translation.mY);
                title.Draw(&graphics);
            }
            const auto pixels=render.ReadImagePixels(*target).pixels;
            const int startX=240+translation.mX, startY=283+translation.mY;
            // Check every pixel of all lawn rows, including both sides of the
            // revealed edge. The production Draw selects the source region.
            for (int y=startY; y<startY+8; ++y)
                for (int x=0; x<800; ++x)
                    Require(pixels[y*800+x]==
                        (x>=startX && x<startX+std::min(width,314) ? grassColor : 0xff000000),
                        "TitleScreen grass appeared beyond its revealed animation width");
        }
    std::cout<<"PASS actual TitleScreen grass reveal: widths 0/1/157/313/314/400, translated draw and unrevealed black pixels\n";
}

void PrecomputedTransparentEdges(TestApp& app)
{
    auto& render = *app.mDDInterface->mRenderInterface;
    constexpr int width = 9, height = 7;
    std::array<unsigned int, width * height> pixels;
    for (unsigned int i = 0; i < pixels.size(); ++i)
        pixels[i] = ((i % 4 == 0 ? 0u : (i % 3 == 0 ? 97u : 255u)) << 24) |
            (((i * 71) & 255) << 16) | (((i * 53) & 255) << 8) | ((i * 137) & 255);
    Image original, repaired;
    original.SetBits(pixels.data(), width, height);
    repaired.SetBits(pixels.data(), width, height);
    original.mGpuSamplingFlags |= GpuVertex::kAlphaEdgeFilter;
    FixPixelsOnAlphaEdgeForBlending(&repaired);
    Require(repaired.mGpuSamplingFlags == 0, "Plain loading image retained runtime sanding");
    for (unsigned int i = 0; i < pixels.size(); ++i)
        Require((pixels[i] >> 24) ? repaired.mPendingDecoded->mBits[i] == pixels[i] :
            (repaired.mPendingDecoded->mBits[i] >> 24) == 0,
            "Edge repair changed visible pixels or alpha");
    // A second loading pass must not propagate hidden RGB into later neighbors.
    const std::vector<unsigned int> once(repaired.mPendingDecoded->mBits,
        repaired.mPendingDecoded->mBits + pixels.size());
    FixPixelsOnAlphaEdgeForBlending(&repaired);
    Require(std::equal(once.begin(), once.end(), repaired.mPendingDecoded->mBits),
        "Edge repair was not idempotent");
    std::unique_ptr<Image> target(render.CreateRenderImage(73, 61));
    for (bool linear : {false, true})
        for (float offset : {0.f, .375f})
        {
            const auto capture = [&](Image& source) {
                {
                    GpuImageDrawScope destination(render, target.get());
                    Graphics graphics(target.get());
                    graphics.mColor = Color(35, 71, 109, 255);
                    graphics.FillRect(Rect(0, 0, 73, 61));
                    graphics.mLinearBlend = linear;
                    TodDrawImageRectScaledF(&graphics, &source, 5.f + offset, 4.f + offset,
                        Rect(0, 0, width, height), 6.125f, 5.375f);
                }
                return render.ReadImagePixels(*target).pixels;
            };
            const auto expected = capture(original), actual = capture(repaired);
            for (unsigned int i = 0; i < actual.size(); ++i)
                for (unsigned int shift : {0u, 8u, 16u, 24u})
                    Require(std::abs(int((actual[i] >> shift) & 255) -
                        int((expected[i] >> shift) & 255)) <= 2,
                        "Precomputed edge repair differs from shader reference");
        }
    Require(!repaired.mPendingDecoded, "Repaired image retained loading pixels after upload");
    std::cout << "PASS precomputed transparent edges: alpha, visible RGB, idempotency, nearest/linear, upload-discard\n";
}

}

int main(int argc, char** argv)
{
    gPlantCatalogOnly=argc==3 && std::string_view(argv[2])=="plant-attacks";
    gIce3Only=argc==3 && std::string_view(argv[2])=="ice3";
    gZombieCatalogOnly=gIce3Only || (argc==3 && std::string_view(argv[2])=="zombie-behavior");
    gMinigameRulesOnly=argc==3 && std::string_view(argv[2])=="minigame-rules";
    gPuzzleRulesOnly=argc==3 && std::string_view(argv[2])=="puzzle-rules";
    gSurvivalRulesOnly=argc==3 && std::string_view(argv[2])=="survival-rules";
    gAdventureRulesOnly=argc==3 && std::string_view(argv[2])=="adventure-rules";
    gHiddenRulesOnly=argc==3 && std::string_view(argv[2])=="hidden-rules";
    gGardenRulesOnly=argc==3 && std::string_view(argv[2])=="garden-rules";
    gIntroRulesOnly=argc==3 && std::string_view(argv[2])=="intro-rules";
    gUiRulesOnly=argc==3 && std::string_view(argv[2])=="ui-rules";
    gAwardRulesOnly=argc==3 && std::string_view(argv[2])=="award-rules";
    gAlmanacClockOnly=argc==3 && std::string_view(argv[2])=="almanac-clock";
    gActorBirthOnly=argc==3 && std::string_view(argv[2])=="actor-birth";
    gLossRulesOnly=argc==3 && std::string_view(argv[2])=="loss-rules";
    gOpeningRulesOnly=argc==3 && std::string_view(argv[2])=="opening-rules";
    gLifecycleRulesOnly=argc==3 && std::string_view(argv[2])=="lifecycle-rules";
    gGraveRulesOnly=argc==3 && std::string_view(argv[2])=="grave-rules";
    gHudSeamsOnly=argc==3 && std::string_view(argv[2])=="hud-seams";
    gTouchPerf=argc==3 && std::string_view(argv[2])=="touch-perf";
    gTouchOnly=gTouchPerf||(argc==3 && std::string_view(argv[2])=="touch-input");
    std::cout<<std::unitbuf;
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    AddVectoredExceptionHandler(1,SuppressFixtureCrashDialogs);
    try
    {
        HiddenWindow window;
        // Puzzle records can save from real production callbacks. Keep all
        // such writes under this fixture process, never in a player directory.
        const auto userData=std::filesystem::absolute("fixture-user-data")/std::to_string(GetCurrentProcessId());
        std::filesystem::create_directories(userData);
        Sexy::SetAppDataFolder(userData.string());
        TestApp app;
        gLawnApp=&app;
        app.mPrimaryThreadId=CurrentNativeThreadIdentity();
        app.mScaleHDREnabled=false;
        app.mReadFromRegistry=false;
        app.mWidth=800;app.mHeight=600;
#if defined(PVZ_GPU_OPENGL) || defined(PVZ_GPU_D3D9) || defined(PVZ_GPU_D3D12)
        app.mGpuContext=std::make_unique<GpuContext>(window.handle,false,false);
#else
        VulkanInitConfig init{};
        init.appInfo.pApplicationName="Image runtime tests";
        init.features.shaderFloat64=VK_TRUE;
        VkSwapchainCreateInfoKHR swapchain{
            .sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
            .imageFormat=VK_FORMAT_B8G8R8A8_UNORM,
            .imageColorSpace=VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            .imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            .compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .presentMode=VK_PRESENT_MODE_FIFO_KHR,
            .clipped=VK_TRUE};
        app.mGpuContext=std::make_unique<GpuContext>(init,window.handle,swapchain);
#endif
        app.mDDInterface=new DDInterface(&app);
        Require(app.mDDInterface->Init(window.handle,true)==DDInterface::RESULT_OK,
            "Cannot initialize public image runtime");
        WrappedTextMeasurements();
        PoolGpuChecks::Run(*app.mDDInterface->mRenderInterface->mRenderer,
#if defined(PVZ_GPU_VULKAN)
            true
#else
            false
#endif
        );
        PrecomputedTransparentEdges(app);
        FontUploadAndCopies(app);
        SourceRectangleDraws(app);
        NecessaryLocalClipDraws(app);
        TitleGrassRevealPixels(app);
        AtlasButtonStates(app);
        SharedReferences(app);
        ReanimationAppearancePixels(app);
        ReanimationFullscreenPixels(app);
        Require(argc==2||gPlantCatalogOnly||gZombieCatalogOnly||gMinigameRulesOnly||gPuzzleRulesOnly||gSurvivalRulesOnly||gAdventureRulesOnly||gHiddenRulesOnly||gGardenRulesOnly||gIntroRulesOnly||gUiRulesOnly||gAwardRulesOnly||gAlmanacClockOnly||gActorBirthOnly||gLossRulesOnly||gOpeningRulesOnly||gLifecycleRulesOnly||gGraveRulesOnly||gHudSeamsOnly||gTouchOnly,"Pass the production main.pak64 path and optional focused case");
        ParticleDefinitionsAndFrames(app,argv[1]);
    }
    catch (const ResourceManagerException& error)
    {
        std::cerr<<"FAIL image runtime resource: reason="<<int(error.error.reason)
            <<" record="<<error.error.record<<" value="<<error.error.value
            <<" offset="<<error.error.offset<<'\n';
        return 1;
    }
    catch (const std::exception& error)
    {
        std::cerr<<"FAIL image runtime: "<<error.what()<<'\n';
        return 1;
    }
}
