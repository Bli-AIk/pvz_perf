// Generated scene atlas geometry and direct texture bindings.
#pragma once
#include "SexyAppFramework/graphics/GpuBackend.h"
#include "SexyAppFramework/graphics/TextureRegion.h"
#include <cstddef>
#include <cstdint>
namespace MagnetAtlas {
extern Sexy::GpuTexture* Texture;
enum class Sprite : std::uint16_t {
    REANIM_ZOMBIE_BUCKET1 = 0,
    REANIM_ZOMBIE_BUCKET2 = 1,
    REANIM_ZOMBIE_BUCKET3 = 2,
    REANIM_ZOMBIE_FOOTBALL_HELMET = 3,
    REANIM_ZOMBIE_FOOTBALL_HELMET2 = 4,
    REANIM_ZOMBIE_FOOTBALL_HELMET3 = 5,
    REANIM_ZOMBIE_SCREENDOOR1 = 6,
    REANIM_ZOMBIE_SCREENDOOR2 = 7,
    REANIM_ZOMBIE_SCREENDOOR3 = 8,
    ZOMBIEPOGO_1 = 9,
    ZOMBIEPOGO_2 = 10,
    ZOMBIEPOGO_3 = 11,
    REANIM_ZOMBIE_JACKBOX_BOX = 12,
    REANIM_ZOMBIE_LADDER_1 = 13,
    REANIM_ZOMBIE_LADDER_1_DAMAGE1 = 14,
    REANIM_ZOMBIE_LADDER_1_DAMAGE2 = 15,
    REANIM_ZOMBIE_LADDER_5 = 16,
    REANIM_COIN_SILVER_DOLLAR = 17,
    REANIM_COIN_GOLD_DOLLAR = 18,
    REANIM_DIAMOND = 19,
    REANIM_ZOMBIE_DIGGER_PICKAXE = 20,
    Count = 21,
};
inline constexpr std::size_t Count = 21;
extern const Sexy::TextureRegion Images[Count];
extern const float Scales[Count];
void Load();
void Free();
}
namespace AlmanacAtlas {
extern Sexy::GpuTexture* Texture;
enum class Sprite : std::uint16_t {
    ALMANAC_INDEXBACK = 0,
    ALMANAC_PLANTBACK = 1,
    ALMANAC_ZOMBIEBACK = 2,
    ALMANAC_PLANTCARD = 3,
    ALMANAC_ZOMBIECARD = 4,
    ALMANAC_ZOMBIEWINDOW = 5,
    ALMANAC_ZOMBIEWINDOW2 = 6,
    ALMANAC_ZOMBIEBLANK = 7,
    ALMANAC_GROUNDDAY = 8,
    ALMANAC_GROUNDNIGHT = 9,
    ALMANAC_GROUNDPOOL = 10,
    ALMANAC_GROUNDNIGHTPOOL = 11,
    ALMANAC_GROUNDROOF = 12,
    ALMANAC_GROUNDICE = 13,
    ALMANAC_CLOSEBUTTON = 14,
    ALMANAC_CLOSEBUTTONHIGHLIGHT = 15,
    ALMANAC_INDEXBUTTON = 16,
    ALMANAC_INDEXBUTTONHIGHLIGHT = 17,
    ALMANAC_IMITATER = 18,
    Count = 19,
};
inline constexpr std::size_t Count = 19;
extern const Sexy::TextureRegion Images[Count];
void Load();
void Free();
}
namespace AchievementsAtlas {
extern Sexy::GpuTexture* Texture;
enum class Sprite : std::uint16_t {
    SELECTORSCREEN_ACHIEVEMENTS_BG = 0,
    ACHEESEMENTS_BEJEWELED = 1,
    ACHEESEMENTS_BOOKWORM = 2,
    ACHEESEMENTS_CHINA = 3,
    ACHEESEMENTS_CHUZZLE = 4,
    ACHEESEMENTS_HOLE_TILE = 5,
    ACHEESEMENTS_PEGGLE = 6,
    ACHEESEMENTS_PIPE = 7,
    ACHEESEMENTS_ZUMA = 8,
    ACHEESEMENTS_BACK_HIGHLIGHT = 9,
    ACHEESEMENTS_MORE_ROCK = 10,
    ACHEESEMENTS_MORE_BUTTON = 11,
    ACHEESEMENTS_MORE_BUTTON_HIGHLIGHT = 12,
    ACHEESEMENTS_TOP_BUTTON = 13,
    ACHEESEMENTS_TOP_BUTTON_HIGHLIGHT = 14,
    ICON_0 = 15,
    ICON_1 = 16,
    ICON_2 = 17,
    ICON_3 = 18,
    ICON_4 = 19,
    ICON_5 = 20,
    ICON_6 = 21,
    ICON_7 = 22,
    ICON_8 = 23,
    ICON_9 = 24,
    ICON_10 = 25,
    ICON_11 = 26,
    ICON_12 = 27,
    ICON_13 = 28,
    ICON_14 = 29,
    ICON_15 = 30,
    ICON_16 = 31,
    ICON_17 = 32,
    ICON_18 = 33,
    ICON_19 = 34,
    Count = 35,
};
inline constexpr std::size_t Count = 35;
extern const Sexy::TextureRegion Images[Count];
inline constexpr std::size_t IconCount = 20;
extern const Sexy::TextureRegion Icons[IconCount];
void Load();
void Free();
}
namespace FixedUIAtlas {
extern Sexy::GpuTexture* Texture;
enum class Sprite : std::uint16_t {
    BUTTON_LEFT = 0,
    BUTTON_MIDDLE = 1,
    BUTTON_RIGHT = 2,
    BUTTON_DOWN_LEFT = 3,
    BUTTON_DOWN_MIDDLE = 4,
    BUTTON_DOWN_RIGHT = 5,
    DIALOG_TOPLEFT = 6,
    DIALOG_TOPMIDDLE = 7,
    DIALOG_TOPRIGHT = 8,
    DIALOG_CENTERLEFT = 9,
    DIALOG_CENTERMIDDLE = 10,
    DIALOG_CENTERRIGHT = 11,
    DIALOG_BOTTOMLEFT = 12,
    DIALOG_BOTTOMMIDDLE = 13,
    DIALOG_BOTTOMRIGHT = 14,
    DIALOG_BIGBOTTOMLEFT = 15,
    DIALOG_BIGBOTTOMMIDDLE = 16,
    DIALOG_BIGBOTTOMRIGHT = 17,
    DIALOG_HEADER = 18,
    SEEDCHOOSER_BUTTON = 19,
    SEEDCHOOSER_BUTTON_DISABLED = 20,
    SEEDCHOOSER_BUTTON_GLOW = 21,
    SEEDCHOOSER_BUTTON2 = 22,
    SEEDCHOOSER_BUTTON2_GLOW = 23,
    TOUCH_FRAME_TL = 24,
    TOUCH_FRAME_TR = 25,
    TOUCH_FRAME_BL = 26,
    TOUCH_FRAME_BR = 27,
    Count = 28,
};
inline constexpr std::size_t Count = 28;
extern const Sexy::TextureRegion Images[Count];
void Load();
void Free();
}
void LoadSceneAtlases();
void FreeSceneAtlases();
