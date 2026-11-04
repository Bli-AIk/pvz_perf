#pragma once
#include "SexyAppFramework/misc/Rect.h"
#include "ConstEnums.h"
class Board;
class SeedBank;
class SeedChooserScreen;
namespace Sexy { class Graphics; }
namespace LandscapeHud {
// All HUD rectangles are Board-local window anchors, independent of the world camera.
Sexy::Rect BankBounds(const Board& board);
Sexy::Rect PacketBounds(const Board& board, int index);
Sexy::Rect SunBounds(const Board& board);
Sexy::Rect ShovelBounds(const Board& board);
void DrawPacket(Sexy::Graphics* graphics, float x, float y, SeedType seed, SeedType imitater,
                float dark, int gray, bool cost, bool currentCost);
Sexy::Rect ChooserViewport();
void DrawImitaterAddon(Sexy::Graphics* g);
void DrawBank(SeedBank& bank, Sexy::Graphics* graphics);
void DrawShovel(const Board& board, Sexy::Graphics* graphics);
void DrawSilhouette(Sexy::Graphics* graphics, int x, int y);
void DrawChooserBackground(Sexy::Graphics* graphics);
void ConfigureChooser(SeedChooserScreen& chooser);
}
