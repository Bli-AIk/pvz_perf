#include "LandscapeHud.h"
#include "Board.h"
#include "SeedPacket.h"
#include "Plant.h"
#include "Challenge.h"
#include "System/ReanimationLawn.h"
#include "graphics/Image.h"
#include "LawnCommon.h"
#include "CursorObject.h"
#include <tuple>
#include "LawnApp.h"
#include "Resources.h"
#include "Widget/SeedChooserScreen.h"
#include "Widget/GameButton.h"
#include "graphics/Graphics.h"
#include "graphics/Font.h"
#include "graphics/AtlasDraw.h"
#include "Sexy.TodLib/TodStringFile.h"
#include "Sexy.TodLib/generated/SceneAtlasData.h"
#include <algorithm>
#include <array>

using namespace Sexy;
namespace {
using Sprite = LandscapeUIAtlas::Sprite;
constexpr int CardWidth=81, CardHeight=53, Pitch=48, SidebarWidth=81;
struct PortraitOrigin { int x,y; };
// Native-pixel face crops in the existing 200x210 loading cache. No resampling.
static constexpr PortraitOrigin ZombiePortraits[] = {
    {25,42}, {35,44}, {40,44}, {30,43}, {20,44},
    {25,43}, {40,43}, {40,46}, {15,65}, {25,44},
    {45,35}, {35,49}, {30,30}, {0,28}, {45,82}
};
static_assert(std::size(ZombiePortraits)==SEED_ZOMBIE_IMP-SEED_ZOMBIE_NORMAL+1);
void DrawSprite(Graphics* g, Sprite sprite, float x, float y) {
    const auto& region=LandscapeUIAtlas::Images[static_cast<size_t>(sprite)];
    g->mLinearBlend=false;
    DrawAtlasImage(g,{LandscapeUIAtlas::Texture,0},region,x,y);
}
const SexyString& CostText(SeedType seed, SeedType imitater, bool current) {
    struct Entry { int cost=-1; bool plus=false; SexyString text; };
    static std::array<Entry, static_cast<size_t>(SEED_ZOMBIE_IMP)+1> cache;
    const auto effective=seed==SEED_IMITATER && imitater!=SEED_NONE ? imitater : seed;
    const bool dynamic=gLawnApp->mBoard && gLawnApp->mBoard->PlantUsesAcceleratedPricing(effective);
    const int cost=dynamic&&current ? gLawnApp->mBoard->GetCurrentPlantCost(seed,imitater) : Plant::GetCost(seed,imitater);
    const bool plus=dynamic&&!current;
    auto& entry=cache[static_cast<unsigned>(effective)<cache.size()?static_cast<unsigned>(effective):0];
    if(entry.cost!=cost||entry.plus!=plus) {
        char text[16];entry.text.assign(FormatInt(text,cost));if(plus)entry.text+='+';
        entry.cost=cost;entry.plus=plus;
    }
    return entry.text;
}
}
namespace LandscapeHud {
Rect BankBounds(const Board& board) {
    // Reuse the existing intro progress, changing its presentation axis only.
    const int slide=static_cast<int>(std::clamp(board.mSeedBank->mY,-87.0f,0.0f)*85/87);
    return {board.mApp->mScreenBounds.mX-board.mX+slide,-board.mY,SidebarWidth,600};
}
Rect PacketBounds(const Board& board,int index) {
    const auto bank=BankBounds(board);
    int y=bank.mY+33+index*Pitch;
    if(board.HasConveyorBeltSeedBank()) {
        const auto& packet=board.mSeedBank->mSeedPackets[index];
        y=bank.mY+33+(packet.mX-91+packet.mOffsetX)*Pitch/50;
    }
    return {bank.mX,y,CardWidth,Pitch};
}
Rect SunBounds(const Board& board) {
    const auto bank=BankBounds(board);return {bank.mX,bank.mY,85,31};
}
Rect ShovelBounds(const Board& board) {
    return {board.mApp->mScreenBounds.mX-board.mX,520-board.mY,84,41};
}
void DrawSilhouette(Graphics* g,int x,int y) {DrawSprite(g,Sprite::SILHOUETTE,x,y);}
void DrawPacket(Graphics* graphics,float x,float y,SeedType seed,SeedType imitater,
                float dark,int gray,bool cost,bool currentCost) {
    constexpr int width=CardWidth,height=CardHeight;
    Graphics g(*graphics);g.mLinearBlend=false;g.mColorizeImages=true;
    g.mColor=Color(gray,gray,gray,graphics->mColorizeImages?graphics->mColor.mAlpha:255);
    const auto effective=seed==SEED_IMITATER&&imitater!=SEED_NONE?imitater:seed;
    const int index=static_cast<int>(effective);
    const auto drawFace=[&](Graphics* target) {
        if((index>=0&&index<51)||effective==SEED_LEFTPEATER) {
            const auto first=seed==SEED_IMITATER?Sprite::IMITATER_0:Sprite::NORMAL_0;
            DrawSprite(target,static_cast<Sprite>(static_cast<int>(first)+(effective==SEED_LEFTPEATER?51:index)),x,y);
        } else if(effective>=SEED_BEGHOULED_BUTTON_SHUFFLE && effective<=SEED_ZOMBIQUARIUM_TROPHY) {
            DrawSprite(target,static_cast<Sprite>(static_cast<int>(Sprite::SPECIAL_SHUFFLE)+index-SEED_BEGHOULED_BUTTON_SHUFFLE),x,y);
        } else {
            // Extra PC modes have no Free card; compose their existing live icon.
            DrawSprite(target,Sprite::SILHOUETTE,x,y);
            Graphics icon(*target);icon.ClipRect(static_cast<int>(x)+2,static_cast<int>(y)+2,width-4,height-4);
            if(effective>=SEED_ZOMBIE_NORMAL && effective<=SEED_ZOMBIE_IMP) {
                // Loading prebuilds this image. No Zombie instance/update is needed by a card.
                auto* image=gLawnApp->mReanimatorCache->mZombieImages[Challenge::IZombieSeedTypeToZombieType(effective)];
                // Native-size portrait crop: no resampling of the cached character image.
                const auto origin=ZombiePortraits[effective-SEED_ZOMBIE_NORMAL];
                icon.DrawImage(image,static_cast<int>(x),static_cast<int>(y),Rect(origin.x,origin.y,width,height));
            } else {
                auto* image=gLawnApp->mReanimatorCache->mPlantImages[effective];
                icon.DrawImage(image,static_cast<int>(x),static_cast<int>(y),Rect(20,20,width,height));
            }
        }
        if(cost) {
            // Inset inside the frame; the complete card, including price, is re-drawn under the cooldown clip.
            const auto tab=seed==SEED_IMITATER?Sprite::GRAY_TAB:Plant::IsUpgrade(effective)?Sprite::PURPLE_TAB:Sprite::GREEN_TAB;
            const int alpha=target->mColor.mAlpha;
            DrawSprite(target,tab,x+42,y+26);
            TodDrawString(target,CostText(seed,imitater,currentCost),x+74,y+42,
                          FONT_PICO129,Color(0,0,0,alpha),DS_ALIGN_RIGHT);
        }
    };
    drawFace(&g);
    if(dark>0) {
        Graphics shade(g);shade.mColor=Color(64,64,64,g.mColor.mAlpha);
        const int covered=std::clamp(FloatRoundToInt(width*dark),0,width);
        shade.ClipRect(static_cast<int>(x)+width-covered,static_cast<int>(y),covered,height);
        drawFace(&shade);
    }

}
void DrawBank(SeedBank& bank,Graphics* graphics) {
    auto& board=*bank.mBoard;
    Graphics g(*graphics);g.Translate(-bank.mX,-bank.mY);
    const auto bounds=BankBounds(board);
    const bool conveyor=board.HasConveyorBeltSeedBank();
    if(conveyor) {
        // One native 93x480 frame; reserve the sunlight header above it.
        const Rect track(bounds.mX-1,bounds.mY+33,83,480);
        Graphics belt(g);belt.ClipRect(track);
        const int phase=static_cast<int>((board.mLevelTimeNs/50'000'000ULL)%165);
        for(int y=track.mY-phase;y<track.mY+480;y+=165)DrawSprite(&belt,Sprite::BELT,track.mX,y);
        DrawSprite(&g,Sprite::BELT_BACKDROP,bounds.mX-6,track.mY);
        g.ClipRect(bounds.mX,track.mY,CardWidth,480);
    } else g.ClipRect(bounds);
    const auto drawCard=[&](int i,int y) {
        auto& packet=bank.mSeedPackets[i];
        if(packet.mPacketType==SEED_NONE||!packet.mVisible)return;
        Graphics card(g);card.Translate(bounds.mX,y);
        packet.Draw(&card);
        const auto start=board.mHudFlashStartNs[i];
        if(start>=0 && static_cast<std::int64_t>(board.mLevelTimeNs)>=start) {
            const auto age=static_cast<std::int64_t>(board.mLevelTimeNs)-start;
            if(age<350'000'000) {
                card.mColorizeImages=true;card.mColor=Color(255,255,255,static_cast<int>(255*(350'000'000-age)/350'000'000));
                DrawSprite(&card,Sprite::FLASH,0,0);
            }
        }
    };
    // Classify the bank once, not for each packet's geometry and clip.
    if(conveyor)for(int i=0;i<bank.mNumPackets;++i) {
        const auto& packet=bank.mSeedPackets[i];
        drawCard(i,bounds.mY+33+(packet.mX-91+packet.mOffsetX)*Pitch/50);
    } else for(int i=0;i<bank.mNumPackets;++i)drawCard(i,bounds.mY+33+i*Pitch);
    bank.DrawSun(graphics);
}
void DrawShovel(const Board& board,Graphics* graphics) {
    const auto bounds=ShovelBounds(board);Graphics g(*graphics);
    DrawSprite(&g,Sprite::SHOVELBANK,bounds.mX,bounds.mY);
    if(board.mCursorObject->mCursorType!=CURSOR_TYPE_SHOVEL)
        DrawSprite(&g,Sprite::SHOVEL,bounds.mX+3,bounds.mY+5);
}
Rect ChooserViewport() { return {118,53,347,450}; }
void DrawChooserBackground(Graphics* g) {
    DrawSprite(g,Sprite::CHOOSER_TOP,106,5);
    DrawSprite(g,Sprite::CHOOSER_MIDDLE,106,49);
    DrawSprite(g,Sprite::CHOOSER_BOTTOM,106,503);
}
void DrawImitaterAddon(Graphics* g) { DrawSprite(g,Sprite::IMITATER_ADDON,487,65); }
void ConfigureChooser(SeedChooserScreen& chooser) {
    const auto region=[](Sprite sprite) {
        return LandscapeUIAtlas::Images[static_cast<size_t>(sprite)];
    };
    auto& play=*chooser.mStartButton;play.mLabel="PLAY";play.mButtonImage=nullptr;
    play.mNormalRegion=region(Sprite::PLAY);play.mDownRegion=region(Sprite::PLAY_DOWN);
    play.mDisabledRegion=region(Sprite::PLAY_DISABLED);play.mOverRegion=play.mNormalRegion;
    play.mX=241;play.mY=535;play.mWidth=101;play.mHeight=54;
    for(auto [button,sprite,x]: {std::tuple{chooser.mAlmanacButton,Sprite::ALMANAC,114},
                                std::tuple{chooser.mStoreButton,Sprite::SHOP,346}}) {
        button->mLabel=sprite==Sprite::ALMANAC?gStr.ALMANAC_BUTTON:gStr.SHOP_BUTTON;button->mButtonImage=nullptr;
        button->mNormalRegion=region(sprite);button->mOverRegion=button->mNormalRegion;
        button->mDownRegion=region(sprite==Sprite::ALMANAC?Sprite::ALMANAC_DOWN:Sprite::SHOP_DOWN);button->mX=x;button->mY=543;button->mWidth=123;button->mHeight=38;
    }
    auto& imitater=*chooser.mImitaterButton;imitater.mButtonImage=nullptr;imitater.mOverImage=nullptr;
    imitater.mDownImage=nullptr;imitater.mDisabledImage=nullptr;
    imitater.mNormalRegion=region(Sprite::NORMAL_48);
    imitater.mOverRegion=imitater.mDownRegion=imitater.mDisabledRegion=imitater.mNormalRegion;
    imitater.mX=500;imitater.mY=81;imitater.mWidth=81;imitater.mHeight=53;
    chooser.mViewLawnButton->mX=626;chooser.mViewLawnButton->mY=86;
    chooser.mViewLawnButton->mNormalRegion=region(Sprite::ALMANAC);
    chooser.mViewLawnButton->mOverRegion=region(Sprite::ALMANAC_DOWN);
    chooser.mViewLawnButton->mWidth=chooser.mViewLawnButton->mNormalRegion.mWidth;
    chooser.mViewLawnButton->mHeight=chooser.mViewLawnButton->mNormalRegion.mHeight;
    chooser.mRandomButton->mBtnNoDraw=true;chooser.mRandomButton->mDisabled=true;
}
}
