// bench04_flowerpot - 目标4: PlantFlowerPotHeightOffset switch -> kFlowerPotAdjust[53] static 表
// FormA = fc0f3526^ 原链逐字；FormB = 当前 HEAD（表带 static + 守卫）逐字。
// 账本旧数（裸 constexpr 表时代）：6.49 -> 1.38（-79%）。
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
    SEED_NONE = -1
};

// ---------------- FormA：原 switch 链（fc0f3526^ 逐字） ----------------
__declspec(noinline) void OneA(uint32_t inp)
{
    SeedType theSeedType = (SeedType)(inp % (uint32_t)NUM_SEED_TYPES);
    float theFlowerPotScale = BitsToF(inp, 12);

    float aHeightOffset = -5.0f * theFlowerPotScale;
    float aScaleOffsetFix = 0.0f;

    switch (theSeedType)
    {
    case SeedType::SEED_CHOMPER:
    case SeedType::SEED_PLANTERN:
        aHeightOffset -= 5.0f;
        break;
    case SeedType::SEED_SCAREDYSHROOM:
        aHeightOffset += 5.0f;
        aScaleOffsetFix -= 8.0f;
        break;
    case SeedType::SEED_SUNSHROOM:
    case SeedType::SEED_PUFFSHROOM:
        aScaleOffsetFix -= 4.0f;
        break;
    case SeedType::SEED_HYPNOSHROOM:
    case SeedType::SEED_MAGNETSHROOM:
    case SeedType::SEED_PEASHOOTER:
    case SeedType::SEED_REPEATER:
    case SeedType::SEED_LEFTPEATER:
    case SeedType::SEED_SNOWPEA:
    case SeedType::SEED_THREEPEATER:
    case SeedType::SEED_SUNFLOWER:
    case SeedType::SEED_MARIGOLD:
    case SeedType::SEED_CABBAGEPULT:
    case SeedType::SEED_MELONPULT:
    case SeedType::SEED_TANGLEKELP:
    case SeedType::SEED_BLOVER:
    case SeedType::SEED_SPIKEWEED:
        aScaleOffsetFix -= 8.0f;
        break;
    case SeedType::SEED_SEASHROOM:
    case SeedType::SEED_POTATOMINE:
        aScaleOffsetFix -= 4.0f;
        break;
    case SeedType::SEED_LILYPAD:
        aScaleOffsetFix -= 16.0f;
        break;
    case SeedType::SEED_INSTANT_COFFEE:
        aScaleOffsetFix -= 20.0f;
        break;
    default:
        break;
    }

    float r = aHeightOffset + (theFlowerPotScale * aScaleOffsetFix - aScaleOffsetFix);
    MixF(r);
}

// ---------------- FormB：当前 HEAD（static 表逐字） ----------------
__declspec(noinline) void OneB(uint32_t inp)
{
    SeedType theSeedType = (SeedType)(inp % (uint32_t)NUM_SEED_TYPES);
    float theFlowerPotScale = BitsToF(inp, 12);

    static constexpr struct { float mHeightAdjust; float mScaleFix; } kFlowerPotAdjust[] = {
        { 0, -8 },   // SEED_PEASHOOTER
        { 0, -8 },   // SEED_SUNFLOWER
        { 0, 0 },    // SEED_CHERRYBOMB
        { 0, 0 },    // SEED_WALLNUT
        { 0, -4 },   // SEED_POTATOMINE
        { 0, -8 },   // SEED_SNOWPEA
        { -5, 0 },   // SEED_CHOMPER
        { 0, -8 },   // SEED_REPEATER
        { 0, -4 },   // SEED_PUFFSHROOM
        { 0, -4 },   // SEED_SUNSHROOM
        { 0, 0 },    // SEED_FUMESHROOM
        { 0, 0 },    // SEED_GRAVEBUSTER
        { 0, -8 },   // SEED_HYPNOSHROOM
        { 5, -8 },   // SEED_SCAREDYSHROOM
        { 0, 0 },    // SEED_ICESHROOM
        { 0, 0 },    // SEED_DOOMSHROOM
        { 0, -16 },  // SEED_LILYPAD
        { 0, 0 },    // SEED_SQUASH
        { 0, -8 },   // SEED_THREEPEATER
        { 0, -8 },   // SEED_TANGLEKELP
        { 0, 0 },    // SEED_JALAPENO
        { 0, -8 },   // SEED_SPIKEWEED
        { 0, 0 },    // SEED_TORCHWOOD
        { 0, 0 },    // SEED_TALLNUT
        { 0, -4 },   // SEED_SEASHROOM
        { -5, 0 },   // SEED_PLANTERN
        { 0, 0 },    // SEED_CACTUS
        { 0, -8 },   // SEED_BLOVER
        { 0, 0 },    // SEED_SPLITPEA
        { 0, 0 },    // SEED_STARFRUIT
        { 0, 0 },    // SEED_PUMPKINSHELL
        { 0, -8 },   // SEED_MAGNETSHROOM
        { 0, -8 },   // SEED_CABBAGEPULT
        { 0, 0 },    // SEED_FLOWERPOT
        { 0, 0 },    // SEED_KERNELPULT
        { 0, -20 },  // SEED_INSTANT_COFFEE
        { 0, 0 },    // SEED_GARLIC
        { 0, 0 },    // SEED_UMBRELLA
        { 0, -8 },   // SEED_MARIGOLD
        { 0, -8 },   // SEED_MELONPULT
        { 0, 0 },    // SEED_GATLINGPEA
        { 0, 0 },    // SEED_TWINSUNFLOWER
        { 0, 0 },    // SEED_GLOOMSHROOM
        { 0, 0 },    // SEED_CATTAIL
        { 0, 0 },    // SEED_WINTERMELON
        { 0, 0 },    // SEED_GOLD_MAGNET
        { 0, 0 },    // SEED_SPIKEROCK
        { 0, 0 },    // SEED_COBCANNON
        { 0, 0 },    // SEED_IMITATER
        { 0, 0 },    // SEED_EXPLODE_O_NUT
        { 0, 0 },    // SEED_GIANT_WALLNUT
        { 0, 0 },    // SEED_SPROUT
        { 0, -8 },   // SEED_LEFTPEATER
    };
    static_assert(sizeof(kFlowerPotAdjust) / sizeof(kFlowerPotAdjust[0]) == (int)SeedType::NUM_SEED_TYPES,
        "槽位数须与 SeedType 枚举同步");
    float aHeightAdjust = 0.0f;
    float aScaleOffsetFix = 0.0f;
    if ((unsigned)theSeedType < sizeof(kFlowerPotAdjust) / sizeof(kFlowerPotAdjust[0]))
    {
        aHeightAdjust = kFlowerPotAdjust[theSeedType].mHeightAdjust;
        aScaleOffsetFix = kFlowerPotAdjust[theSeedType].mScaleFix;
    }

    float aHeightOffset = -5.0f * theFlowerPotScale + aHeightAdjust;
    float r = aHeightOffset + (theFlowerPotScale * aScaleOffsetFix - aScaleOffsetFix);
    MixF(r);
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
    printf("bench04 kFlowerPotAdjust | before %.2f ns | after %.2f ns | diff %.1f%% | checksum=%s (%016llX/%016llX)\n",
        a, b, (a - b) / a * 100.0, ca == cb ? "OK" : "MISMATCH", (unsigned long long)ca, (unsigned long long)cb);
    return ca == cb ? 0 : 1;
}
