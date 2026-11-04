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
namespace DesktopUIAtlas {
extern Sexy::GpuTexture* Texture;
enum class Sprite : std::uint16_t {
    SEEDCHOOSER_BACKGROUND = 0,
    SEEDCHOOSER_IMITATERADDON = 1,
    SEEDPACKETSILHOUETTE = 2,
    Count = 3,
};
inline constexpr std::size_t Count = 3;
extern const Sexy::TextureRegion Images[Count];
void Load();
void Free();
}
namespace LandscapeUIAtlas {
extern Sexy::GpuTexture* Texture;
enum class Sprite : std::uint16_t {
    SELECTED_TL = 0,
    SELECTED_TR = 1,
    SELECTED_BL = 2,
    SELECTED_BR = 3,
    NORMAL_0 = 4,
    NORMAL_1 = 5,
    NORMAL_2 = 6,
    NORMAL_3 = 7,
    NORMAL_4 = 8,
    NORMAL_5 = 9,
    NORMAL_6 = 10,
    NORMAL_7 = 11,
    NORMAL_8 = 12,
    NORMAL_9 = 13,
    NORMAL_10 = 14,
    NORMAL_11 = 15,
    NORMAL_12 = 16,
    NORMAL_13 = 17,
    NORMAL_14 = 18,
    NORMAL_15 = 19,
    NORMAL_16 = 20,
    NORMAL_17 = 21,
    NORMAL_18 = 22,
    NORMAL_19 = 23,
    NORMAL_20 = 24,
    NORMAL_21 = 25,
    NORMAL_22 = 26,
    NORMAL_23 = 27,
    NORMAL_24 = 28,
    NORMAL_25 = 29,
    NORMAL_26 = 30,
    NORMAL_27 = 31,
    NORMAL_28 = 32,
    NORMAL_29 = 33,
    NORMAL_30 = 34,
    NORMAL_31 = 35,
    NORMAL_32 = 36,
    NORMAL_33 = 37,
    NORMAL_34 = 38,
    NORMAL_35 = 39,
    NORMAL_36 = 40,
    NORMAL_37 = 41,
    NORMAL_38 = 42,
    NORMAL_39 = 43,
    NORMAL_40 = 44,
    NORMAL_41 = 45,
    NORMAL_42 = 46,
    NORMAL_43 = 47,
    NORMAL_44 = 48,
    NORMAL_45 = 49,
    NORMAL_46 = 50,
    NORMAL_47 = 51,
    NORMAL_48 = 52,
    NORMAL_49 = 53,
    NORMAL_50 = 54,
    NORMAL_51 = 55,
    IMITATER_0 = 56,
    IMITATER_1 = 57,
    IMITATER_2 = 58,
    IMITATER_3 = 59,
    IMITATER_4 = 60,
    IMITATER_5 = 61,
    IMITATER_6 = 62,
    IMITATER_7 = 63,
    IMITATER_8 = 64,
    IMITATER_9 = 65,
    IMITATER_10 = 66,
    IMITATER_11 = 67,
    IMITATER_12 = 68,
    IMITATER_13 = 69,
    IMITATER_14 = 70,
    IMITATER_15 = 71,
    IMITATER_16 = 72,
    IMITATER_17 = 73,
    IMITATER_18 = 74,
    IMITATER_19 = 75,
    IMITATER_20 = 76,
    IMITATER_21 = 77,
    IMITATER_22 = 78,
    IMITATER_23 = 79,
    IMITATER_24 = 80,
    IMITATER_25 = 81,
    IMITATER_26 = 82,
    IMITATER_27 = 83,
    IMITATER_28 = 84,
    IMITATER_29 = 85,
    IMITATER_30 = 86,
    IMITATER_31 = 87,
    IMITATER_32 = 88,
    IMITATER_33 = 89,
    IMITATER_34 = 90,
    IMITATER_35 = 91,
    IMITATER_36 = 92,
    IMITATER_37 = 93,
    IMITATER_38 = 94,
    IMITATER_39 = 95,
    IMITATER_40 = 96,
    IMITATER_41 = 97,
    IMITATER_42 = 98,
    IMITATER_43 = 99,
    IMITATER_44 = 100,
    IMITATER_45 = 101,
    IMITATER_46 = 102,
    IMITATER_47 = 103,
    IMITATER_48 = 104,
    IMITATER_49 = 105,
    IMITATER_50 = 106,
    IMITATER_51 = 107,
    SILHOUETTE = 108,
    GREEN_TAB = 109,
    PURPLE_TAB = 110,
    GRAY_TAB = 111,
    SUNBANK = 112,
    SHOVEL = 113,
    SHOVELBANK = 114,
    BELT = 115,
    BELT_BACKDROP = 116,
    CHOOSER_TOP = 117,
    CHOOSER_MIDDLE = 118,
    CHOOSER_BOTTOM = 119,
    PLAY = 120,
    PLAY_DOWN = 121,
    ALMANAC = 122,
    SHOP = 123,
    FLASH = 124,
    PLAY_DISABLED = 125,
    ALMANAC_DOWN = 126,
    SHOP_DOWN = 127,
    IMITATER_ADDON = 128,
    SPECIAL_SHUFFLE = 129,
    SPECIAL_CRATER = 130,
    SPECIAL_SUN = 131,
    SPECIAL_DIAMOND = 132,
    SPECIAL_AQUARIUM = 133,
    SPECIAL_TROPHY = 134,
    Count = 135,
};
inline constexpr std::size_t Count = 135;
extern const Sexy::TextureRegion Images[Count];
void Load();
void Free();
}
namespace CommonUIAtlas {
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
void LoadSceneAtlases(bool landscapeUI);
void FreeSceneAtlases();
