// bench05_packetdraw - 目标5: DrawSeedPacket 绘制参数 switch 长链 -> kSeedPacketDrawMetrics[75] static 表
// FormA = fc0f3526^ 原链逐字（switch ~40 组）；FormB = 当前 HEAD（表带 static + 守卫）逐字。
// 账本旧数（裸 constexpr 表时代）：9.97 -> 1.89（-81%）。
// 域：aSeedType ∈ -1..74（含 SEED_NONE 越界走默认），两形态同数组。
#include "bench_common.h"

enum SeedType
{
    SEED_PEASHOOTER = 0, SEED_SUNFLOWER, SEED_CHERRYBOMB, SEED_WALLNUT, SEED_POTATOMINE,
    SEED_SNOWPEA, SEED_CHOMPER, SEED_REPEATER, SEED_PUFFSHROOM, SEED_SUNSHROOM,
    SEED_FUMESHROOM, SEED_GRAVEBUSTER, SEED_HYPNOSHROOM, SEED_SCAREDYSHROOM, SEED_ICESHROOM,
    SEED_DOOMSHROOM, SEED_LILYPAD, SEED_SQUASH, SEED_THREEPEATER, SEED_TANGLEKELP,
    SEED_JALAPENO, SEED_SPIKEWEED, SEED_TORCHWOOD, SEED_TALLNUT, SEED_SEASHROOM,
    SEED_PLANTERN, SEED_CACTUS, SEED_BLOVER, SEED_SPLITPEA, SEED_STARFRUIT,
    SEED_PUMPKINSHELL, SEED_MAGNETSHROOM, SEED_CABBAGEPULT, SEED_FLOWERPOT, SEED_KERNELPULT,
    SEED_INSTANT_COFFEE, SEED_GARLIC, SEED_UMBRELLA, SEED_MARIGOLD, SEED_MELONPULT,
    SEED_GATLINGPEA, SEED_TWINSUNFLOWER, SEED_GLOOMSHROOM, SEED_CATTAIL, SEED_WINTERMELON,
    SEED_GOLD_MAGNET, SEED_SPIKEROCK, SEED_COBCANNON, SEED_IMITATER, SEED_EXPLODE_O_NUT,
    SEED_GIANT_WALLNUT, SEED_SPROUT, SEED_LEFTPEATER,
    NUM_SEED_TYPES,
    SEED_BEGHOULED_BUTTON_SHUFFLE,
    SEED_BEGHOULED_BUTTON_CRATER,
    SEED_SLOT_MACHINE_SUN,
    SEED_SLOT_MACHINE_DIAMOND,
    SEED_ZOMBIQUARIUM_SNORKLE,
    SEED_ZOMBIQUARIUM_TROPHY,
    SEED_ZOMBIE_NORMAL,
    SEED_ZOMBIE_TRAFFIC_CONE,
    SEED_ZOMBIE_POLEVAULTER,
    SEED_ZOMBIE_PAIL,
    SEED_ZOMBIE_LADDER,
    SEED_ZOMBIE_DIGGER,
    SEED_ZOMBIE_BUNGEE,
    SEED_ZOMBIE_FOOTBALL,
    SEED_ZOMBIE_BALLOON,
    SEED_ZOMBIE_SCREEN_DOOR,
    SEED_ZOMBONI,
    SEED_ZOMBIE_POGO,
    SEED_ZOMBIE_DANCER,
    SEED_ZOMBIE_GARGANTUAR,
    SEED_ZOMBIE_IMP,
    SEED_NONE = -1
};
static_assert(SEED_ZOMBIE_IMP == 74, "enum 尾段须与仓库一致");

struct Graphics
{
    float mScaleX, mScaleY;
    Graphics() : mScaleX(1.0f), mScaleY(1.0f) {}
};
static int gGameMode = 0;   // 非 GAMEMODE_CHALLENGE_BIG_TIME

static void SeedPacketDrawSeedStub(Graphics* g, float x, float y, SeedType theSeedType, SeedType theImitaterType, float ox, float oy, float scale)
{
    Mix(0xD5E1u); MixI((int)theSeedType); MixI((int)theImitaterType); MixF(x); MixF(y); MixF(ox); MixF(oy); MixF(scale);
}

static void DrawTail(Graphics* g, float x, float y, SeedType aSeedType, SeedType theSeedType, SeedType theImitaterType, float& aScale, bool& aDrawSeedInMiddle, float& aOffsetX, float& aOffsetY)
{
    if (gGameMode == 1)
    {
        if (aSeedType == SeedType::SEED_WALLNUT || aSeedType == SeedType::SEED_SUNFLOWER || aSeedType == SeedType::SEED_MARIGOLD)
        {
            aOffsetX = 16.0f;
            aOffsetY = 34.0f;
        }
    }
    if (aSeedType == SeedType::SEED_GIANT_WALLNUT)
    {
        aScale *= 0.75f;
        aOffsetX = 52.0f;
        aOffsetY = 58.0f;
    }
    aOffsetX = g->mScaleX * aOffsetX;
    aOffsetY = g->mScaleY * (aOffsetY + 1.0f);
    if (aDrawSeedInMiddle)
    {
        SeedPacketDrawSeedStub(g, x, y, theSeedType, theImitaterType, aOffsetX, aOffsetY, aScale);
    }
}

// ---------------- FormA：原 switch 长链（fc0f3526^ 逐字，计时分派区） ----------------
__declspec(noinline) void OneA(uint32_t inp)
{
    Graphics gStorage;
    Graphics* g = &gStorage;
    g->mScaleX = BitsToF(inp, 0);
    g->mScaleY = BitsToF(inp, 4);
    float x = BitsToF(inp, 8), y = BitsToF(inp, 16);
    SeedType aSeedType = (SeedType)((int)(inp % 76u) - 1);   // -1..74
    SeedType theSeedType = aSeedType;
    SeedType theImitaterType = SeedType::SEED_NONE;

    float aScale = 0.5f;
    bool aDrawSeedInMiddle = true;
    float aOffsetX = 5.0f;
    float aOffsetY = 8.0f;
    switch (aSeedType)
    {
    case SeedType::SEED_TALLNUT:
        aScale = 0.3f; aOffsetX = 12.0f; aOffsetY = 22.0f;
        break;
    case SeedType::SEED_INSTANT_COFFEE:
        aScale = 0.55f; aOffsetX = 0; aOffsetY = 9.0f;
        break;
    case SeedType::SEED_COBCANNON:
        aScale = 0.26f; aOffsetX = 6.0f; aOffsetY = 22.0f;
        break;
    case SeedType::SEED_CACTUS:
        aOffsetX = 9.0f; aOffsetY = 13.0f;
        break;
    case SeedType::SEED_POTATOMINE:
        aScale = 0.4f; aOffsetX = 8.0f; aOffsetY = 12.0f;
        break;
    case SeedType::SEED_MAGNETSHROOM:
        aOffsetY = 12.0f;
        break;
    case SeedType::SEED_FUMESHROOM:
    case SeedType::SEED_PUMPKINSHELL:
    case SeedType::SEED_CHOMPER:
    case SeedType::SEED_DOOMSHROOM:
    case SeedType::SEED_SQUASH:
    case SeedType::SEED_HYPNOSHROOM:
    case SeedType::SEED_SPIKEWEED:
    case SeedType::SEED_SPIKEROCK:
    case SeedType::SEED_PLANTERN:
    case SeedType::SEED_TORCHWOOD:
    case SeedType::SEED_TANGLEKELP:
        aScale = 0.4f; aOffsetX = 8.0f; aOffsetY = 12.0f;
        break;
    case SeedType::SEED_TWINSUNFLOWER:
    case SeedType::SEED_GLOOMSHROOM:
        aScale = 0.45f; aOffsetX = 7.0f; aOffsetY = 14.0f;
        break;
    case SeedType::SEED_CATTAIL:
        aScale = 0.45f; aOffsetX = 8.0f; aOffsetY = 13.0f;
        break;
    case SeedType::SEED_UMBRELLA:
        aScale = 0.5f; aOffsetX = 5.0f; aOffsetY = 10.0f;
        break;
    case SeedType::SEED_KERNELPULT:
        aScale = 0.4f; aOffsetX = 13.0f; aOffsetY = 14.0f;
        break;
    case SeedType::SEED_CABBAGEPULT:
        aScale = 0.4f; aOffsetX = 15.0f; aOffsetY = 14.0f;
        break;
    case SeedType::SEED_MELONPULT:
    case SeedType::SEED_WINTERMELON:
        aScale = 0.35f; aOffsetX = 18.0f; aOffsetY = 19.0f;
        break;
    case SeedType::SEED_GRAVEBUSTER:
        aScale = 0.4f; aOffsetX = 10.0f; aOffsetY = 15.0f;
        break;
    case SeedType::SEED_SPLITPEA:
        aScale = 0.45f; aOffsetX = 12.0f; aOffsetY = 12.0f;
        break;
    case SeedType::SEED_BLOVER:
        aScale = 0.4f; aOffsetX = 8.0f; aOffsetY = 17.0f;
        break;
    case SeedType::SEED_STARFRUIT:
        aScale = 0.5f; aOffsetX = 6.0f; aOffsetY = 8.0f;
        break;
    case SeedType::SEED_THREEPEATER:
        aScale = 0.5f; aOffsetX = 5.0f; aOffsetY = 10.0f;
        break;
    case SeedType::SEED_GATLINGPEA:
        aScale = 0.5f; aOffsetX = 2.0f; aOffsetY = 8.0f;
        break;
    case SeedType::SEED_ZOMBIE_NORMAL:
    case SeedType::SEED_ZOMBIE_TRAFFIC_CONE:
    case SeedType::SEED_ZOMBIE_PAIL:
        aScale = 0.35f; aOffsetX = -3.0f; aOffsetY = -7.0f;
        break;
    case SeedType::SEED_ZOMBIE_DANCER:
        aScale = 0.37f; aOffsetX = -18.0f; aOffsetY = -38.0f;
        break;
    case SeedType::SEED_ZOMBIE_POLEVAULTER:
        aScale = 0.35f; aOffsetX = -8.0f; aOffsetY = -12.0f;
        break;
    case SeedType::SEED_ZOMBIE_LADDER:
    case SeedType::SEED_ZOMBIE_DIGGER:
    case SeedType::SEED_ZOMBIE_SCREEN_DOOR:
    case SeedType::SEED_ZOMBIE_POGO:
        aScale = 0.35f; aOffsetX = -3.0f; aOffsetY = -10.0f;
        break;
    case SeedType::SEED_ZOMBIE_BUNGEE:
        aScale = 0.3f; aOffsetX = 1.0f; aOffsetY = -1.0f;
        break;
    case SeedType::SEED_ZOMBIE_FOOTBALL:
        aScale = 0.33f; aOffsetX = -7.0f; aOffsetY = -9.0f;
        break;
    case SeedType::SEED_ZOMBIE_BALLOON:
        aScale = 0.35f; aOffsetX = -3.0f; aOffsetY = -5.0f;
        break;
    case SeedType::SEED_ZOMBIE_IMP:
        aScale = 0.4f; aOffsetX = -12.0f; aOffsetY = -17.0f;
        break;
    case SeedType::SEED_ZOMBONI:
        aScale = 0.23f; aOffsetX = 12.0f; aOffsetY = 3.0f;
        break;
    case SeedType::SEED_ZOMBIE_GARGANTUAR:
        aScale = 0.23f; aOffsetX = 4.0f; aOffsetY = 3.0f;
        break;
    case SeedType::SEED_BEGHOULED_BUTTON_SHUFFLE:
    case SeedType::SEED_BEGHOULED_BUTTON_CRATER:
    case SeedType::SEED_SLOT_MACHINE_SUN:
    case SeedType::SEED_SLOT_MACHINE_DIAMOND:
    case SeedType::SEED_ZOMBIQUARIUM_SNORKLE:
    case SeedType::SEED_ZOMBIQUARIUM_TROPHY:
        aDrawSeedInMiddle = false;
        break;
    default:
        break;
    }
    DrawTail(g, x, y, aSeedType, theSeedType, theImitaterType, aScale, aDrawSeedInMiddle, aOffsetX, aOffsetY);
}

// ---------------- FormB：当前 HEAD（static 表逐字，计时分派区） ----------------
__declspec(noinline) void OneB(uint32_t inp)
{
    Graphics gStorage;
    Graphics* g = &gStorage;
    g->mScaleX = BitsToF(inp, 0);
    g->mScaleY = BitsToF(inp, 4);
    float x = BitsToF(inp, 8), y = BitsToF(inp, 16);
    SeedType aSeedType = (SeedType)((int)(inp % 76u) - 1);   // -1..74
    SeedType theSeedType = aSeedType;
    SeedType theImitaterType = SeedType::SEED_NONE;

    struct SeedPacketDrawMetrics
    {
        float mScale;
        float mOffsetX;
        float mOffsetY;
        bool mDrawInMiddle;
    };
    static constexpr SeedPacketDrawMetrics kSeedPacketDefault = { 0.5f, 5.0f, 8.0f, true };
	static constexpr SeedPacketDrawMetrics kSeedPacketDrawMetrics[] = {
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_PEASHOOTER
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_SUNFLOWER
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_CHERRYBOMB
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_WALLNUT
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_POTATOMINE
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_SNOWPEA
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_CHOMPER
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_REPEATER
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_PUFFSHROOM
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_SUNSHROOM
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_FUMESHROOM
		{ 0.4f, 10.0f,  15.0f, true },   // SEED_GRAVEBUSTER
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_HYPNOSHROOM
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_SCAREDYSHROOM
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_ICESHROOM
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_DOOMSHROOM
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_LILYPAD
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_SQUASH
		{ 0.5f,  5.0f,  10.0f, true },   // SEED_THREEPEATER
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_TANGLEKELP
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_JALAPENO
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_SPIKEWEED
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_TORCHWOOD
		{ 0.3f, 12.0f,  22.0f, true },   // SEED_TALLNUT
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_SEASHROOM
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_PLANTERN
		{ 0.5f,  9.0f,  13.0f, true },   // SEED_CACTUS
		{ 0.4f,  8.0f,  17.0f, true },   // SEED_BLOVER
		{ 0.45f, 12.0f,  12.0f, true },  // SEED_SPLITPEA
		{ 0.5f,  6.0f,   8.0f, true },   // SEED_STARFRUIT
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_PUMPKINSHELL
		{ 0.5f,  5.0f,  12.0f, true },   // SEED_MAGNETSHROOM
		{ 0.4f, 15.0f,  14.0f, true },   // SEED_CABBAGEPULT
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_FLOWERPOT
		{ 0.4f, 13.0f,  14.0f, true },   // SEED_KERNELPULT
		{ 0.55f, 0.0f,   9.0f, true },   // SEED_INSTANT_COFFEE
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_GARLIC
		{ 0.5f,  5.0f,  10.0f, true },   // SEED_UMBRELLA
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_MARIGOLD
		{ 0.35f, 18.0f,  19.0f, true },  // SEED_MELONPULT
		{ 0.5f,  2.0f,   8.0f, true },   // SEED_GATLINGPEA
		{ 0.45f, 7.0f,  14.0f, true },   // SEED_TWINSUNFLOWER
		{ 0.45f, 7.0f,  14.0f, true },   // SEED_GLOOMSHROOM
		{ 0.45f, 8.0f,  13.0f, true },   // SEED_CATTAIL
		{ 0.35f, 18.0f,  19.0f, true },  // SEED_WINTERMELON
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_GOLD_MAGNET
		{ 0.4f,  8.0f,  12.0f, true },   // SEED_SPIKEROCK
		{ 0.26f, 6.0f,  22.0f, true },   // SEED_COBCANNON
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_IMITATER
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_EXPLODE_O_NUT
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_GIANT_WALLNUT
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_SPROUT
		{ 0.5f,  5.0f,   8.0f, true },   // SEED_LEFTPEATER
		kSeedPacketDefault,              // 53 = NUM_SEED_TYPES 哨兵位（无种子）
		{ 0.5f,  5.0f,   8.0f, false },  // SEED_BEGHOULED_BUTTON_SHUFFLE
		{ 0.5f,  5.0f,   8.0f, false },  // SEED_BEGHOULED_BUTTON_CRATER
		{ 0.5f,  5.0f,   8.0f, false },  // SEED_SLOT_MACHINE_SUN
		{ 0.5f,  5.0f,   8.0f, false },  // SEED_SLOT_MACHINE_DIAMOND
		{ 0.5f,  5.0f,   8.0f, false },  // SEED_ZOMBIQUARIUM_SNORKLE
		{ 0.5f,  5.0f,   8.0f, false },  // SEED_ZOMBIQUARIUM_TROPHY
		{ 0.35f, -3.0f,  -7.0f, true },  // SEED_ZOMBIE_NORMAL
		{ 0.35f, -3.0f,  -7.0f, true },  // SEED_ZOMBIE_TRAFFIC_CONE
		{ 0.35f, -8.0f, -12.0f, true },  // SEED_ZOMBIE_POLEVAULTER
		{ 0.35f, -3.0f,  -7.0f, true },  // SEED_ZOMBIE_PAIL
		{ 0.35f, -3.0f, -10.0f, true },  // SEED_ZOMBIE_LADDER
		{ 0.35f, -3.0f, -10.0f, true },  // SEED_ZOMBIE_DIGGER
		{ 0.3f,  1.0f,  -1.0f, true },   // SEED_ZOMBIE_BUNGEE
		{ 0.33f, -7.0f,  -9.0f, true },  // SEED_ZOMBIE_FOOTBALL
		{ 0.35f, -3.0f,  -5.0f, true },  // SEED_ZOMBIE_BALLOON
		{ 0.35f, -3.0f, -10.0f, true },  // SEED_ZOMBIE_SCREEN_DOOR
		{ 0.23f, 12.0f,   3.0f, true },  // SEED_ZOMBONI
		{ 0.35f, -3.0f, -10.0f, true },  // SEED_ZOMBIE_POGO
		{ 0.37f, -18.0f, -38.0f, true }, // SEED_ZOMBIE_DANCER
		{ 0.23f,  4.0f,   3.0f, true },  // SEED_ZOMBIE_GARGANTUAR
		{ 0.4f, -12.0f, -17.0f, true },  // SEED_ZOMBIE_IMP
	};
    static_assert(sizeof(kSeedPacketDrawMetrics) / sizeof(kSeedPacketDrawMetrics[0]) == (int)SeedType::SEED_ZOMBIE_IMP + 1,
        "槽位数须与 SeedType 枚举尾段同步");
    const SeedPacketDrawMetrics& aMetrics = (unsigned)aSeedType < sizeof(kSeedPacketDrawMetrics) / sizeof(kSeedPacketDrawMetrics[0])
        ? kSeedPacketDrawMetrics[aSeedType] : kSeedPacketDefault;
    float aScale = aMetrics.mScale;
    bool aDrawSeedInMiddle = aMetrics.mDrawInMiddle;
    float aOffsetX = aMetrics.mOffsetX;
    float aOffsetY = aMetrics.mOffsetY;
    DrawTail(g, x, y, aSeedType, theSeedType, theImitaterType, aScale, aDrawSeedInMiddle, aOffsetX, aOffsetY);
}

static void RunA() { for (int i = 0; i < kN; i++) OneA(gInput[i]); }
static void RunB() { for (int i = 0; i < kN; i++) OneB(gInput[i]); }

int main()
{
    GenInput();
    uint64_t ca = CheckOnly(RunA);
    uint64_t cb = CheckOnly(RunB);
    double a = TimePerIterNs(RunA, kN);
    double b = TimePerIterNs(RunB, kN);
    printf("bench05 kSeedPacketDrawMetrics | before %.2f ns | after %.2f ns | diff %.1f%% | checksum=%s (%016llX/%016llX)\n",
        a, b, (a - b) / a * 100.0, ca == cb ? "OK" : "MISMATCH", (unsigned long long)ca, (unsigned long long)cb);
    return ca == cb ? 0 : 1;
}
