// bench7_plantheight.cpp - 目标7: PlantDrawHeightOffset 12 支 -> kPlantHeightOffsets 53 槽
// 计时区 = 种子分派段（FLOWERPOT 链起至 return；浮动/花盆公共前缀两形态逐字相同，不计入）
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
enum GameMode { GAMEMODE_ADVENTURE = 0, GAMEMODE_CHALLENGE_ZEN_GARDEN = 20 };

struct Plant;
struct Board
{
    int mBgFlags;
    bool StageHas6Rows() const { return (mBgFlags & 1) != 0; }
    bool StageHasRoof() const { return (mBgFlags & 2) != 0; }
    bool IsPoolSquare(int theCol, int theRow) const { (void)theCol; (void)theRow; return (mBgFlags & 4) != 0; }
    Plant* GetFlowerPotAt(int theCol, int theRow) { (void)theCol; (void)theRow; return (mBgFlags & 8) ? (Plant*)1 : nullptr; }
};
struct LawnAppStub { int mGameMode; };
static Board gBoard;
static LawnAppStub gLawnAppStorage;
static LawnAppStub* gLawnApp = &gLawnAppStorage;

// ---------------- FormA：原 12 支链（逐字，含 SPIKEWEED 嵌套地形支） ----------------
__declspec(noinline) float OneA(uint32_t inp)
{
    SeedType theSeedType = (SeedType)(inp % (uint32_t)NUM_SEED_TYPES);
    int theCol = (int)((inp >> 8) & 0xFu);
    int theRow = (int)((inp >> 12) & 0x7u);
    gBoard.mBgFlags = (int)((inp >> 16) & 0xFu);
    gLawnApp->mGameMode = ((inp >> 21) & 1u) ? GAMEMODE_CHALLENGE_ZEN_GARDEN : GAMEMODE_ADVENTURE;
    Board* theBoard = &gBoard;
    Plant* thePlant = nullptr;

    float aHeightOffset = 0.0f;

    if (theSeedType == SeedType::SEED_FLOWERPOT)
    {
        aHeightOffset += 26.0f;
    }
    else if (theSeedType == SeedType::SEED_LILYPAD)
    {
        aHeightOffset += 25.0f;
    }
    else if (theSeedType == SeedType::SEED_STARFRUIT)
    {
        aHeightOffset += 10.0f;
    }
    else if (theSeedType == SeedType::SEED_TANGLEKELP)
    {
        aHeightOffset += 24.0f;
    }
    else if (theSeedType == SeedType::SEED_SEASHROOM)
    {
        aHeightOffset += 28.0f;
    }
    else if (theSeedType == SeedType::SEED_INSTANT_COFFEE)
    {
        aHeightOffset -= 20.0f;
    }
    //else if (Plant::IsFlying(theSeedType))
    //{
    //    aHeightOffset -= 30.0f;
    //}
    else if (theSeedType == SeedType::SEED_CACTUS)
    {
        return aHeightOffset;
    }
    else if (theSeedType == SeedType::SEED_PUMPKINSHELL)
    {
        aHeightOffset += 15.0f;
    }
    else if (theSeedType == SeedType::SEED_PUFFSHROOM)
    {
        aHeightOffset += 5.0f;
    }
    else if (theSeedType == SeedType::SEED_SCAREDYSHROOM)
    {
        aHeightOffset -= 14.0f;
    }
    else if (theSeedType == SeedType::SEED_GRAVEBUSTER)
    {
        aHeightOffset -= 40.0f;
    }
    else if (theSeedType == SeedType::SEED_SPIKEWEED || theSeedType == SeedType::SEED_SPIKEROCK)
    {
        int aBottomRow = 4;
        if (theBoard && theBoard->StageHas6Rows())
        {
            aBottomRow = 5;
        }

        if (theSeedType == SeedType::SEED_SPIKEROCK)
        {
            aHeightOffset += 6.0f;
        }

        if (theBoard && theBoard->GetFlowerPotAt(theCol, theRow) && gLawnApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
        {
            aHeightOffset += 5.0f;
        }
        else if (theBoard && theBoard->StageHasRoof())
        {
            aHeightOffset += 15.0f;
        }
        else if (theBoard && theBoard->IsPoolSquare(theCol, theRow))
        {
            aHeightOffset += 0.0f;
        }
        else if (theRow == aBottomRow && theCol >= 7 && theBoard->StageHas6Rows())
        {
            aHeightOffset += 1.0f;
        }
        else if (theRow == aBottomRow && theCol < 7)
        {
            aHeightOffset += 12.0f;
        }
        else
        {
            aHeightOffset += 15.0f;
        }
    }

    (void)thePlant;
    return aHeightOffset;
}

// ---------------- FormB：当前表形态（逐字：SPIKEWEED 特例 + 53 槽表） ----------------
__declspec(noinline) float OneB(uint32_t inp)
{
    SeedType theSeedType = (SeedType)(inp % (uint32_t)NUM_SEED_TYPES);
    int theCol = (int)((inp >> 8) & 0xFu);
    int theRow = (int)((inp >> 12) & 0x7u);
    gBoard.mBgFlags = (int)((inp >> 16) & 0xFu);
    gLawnApp->mGameMode = ((inp >> 21) & 1u) ? GAMEMODE_CHALLENGE_ZEN_GARDEN : GAMEMODE_ADVENTURE;
    Board* theBoard = &gBoard;
    Plant* thePlant = nullptr;

    float aHeightOffset = 0.0f;

    constexpr float kPlantHeightOffsets[] = {
        0,     // 0 SEED_PEASHOOTER
        0,     // 1 SEED_SUNFLOWER
        0,     // 2 SEED_CHERRYBOMB
        0,     // 3 SEED_WALLNUT
        0,     // 4 SEED_POTATOMINE
        0,     // 5 SEED_SNOWPEA
        0,     // 6 SEED_CHOMPER
        0,     // 7 SEED_REPEATER
        5,     // 8 SEED_PUFFSHROOM
        0,     // 9 SEED_SUNSHROOM
        0,     // 10 SEED_FUMESHROOM
        -40,   // 11 SEED_GRAVEBUSTER
        0,     // 12 SEED_HYPNOSHROOM
        -14,   // 13 SEED_SCAREDYSHROOM
        0,     // 14 SEED_ICESHROOM
        0,     // 15 SEED_DOOMSHROOM
        25,    // 16 SEED_LILYPAD
        0,     // 17 SEED_SQUASH
        0,     // 18 SEED_THREEPEATER
        24,    // 19 SEED_TANGLEKELP
        0,     // 20 SEED_JALAPENO
        0,     // 21 SEED_SPIKEWEED（特例表外，槽位不可达）
        0,     // 22 SEED_TORCHWOOD
        0,     // 23 SEED_TALLNUT
        28,    // 24 SEED_SEASHROOM
        0,     // 25 SEED_PLANTERN
        0,     // 26 SEED_CACTUS（原早退 = 不调整）
        0,     // 27 SEED_BLOVER
        0,     // 28 SEED_SPLITPEA
        10,    // 29 SEED_STARFRUIT
        15,    // 30 SEED_PUMPKINSHELL
        0,     // 31 SEED_MAGNETSHROOM
        0,     // 32 SEED_CABBAGEPULT
        26,    // 33 SEED_FLOWERPOT
        0,     // 34 SEED_KERNELPULT
        -20,   // 35 SEED_INSTANT_COFFEE
        0,     // 36 SEED_GARLIC
        0,     // 37 SEED_UMBRELLA
        0,     // 38 SEED_MARIGOLD
        0,     // 39 SEED_MELONPULT
        0,     // 40 SEED_GATLINGPEA
        0,     // 41 SEED_TWINSUNFLOWER
        0,     // 42 SEED_GLOOMSHROOM
        0,     // 43 SEED_CATTAIL
        0,     // 44 SEED_WINTERMELON
        0,     // 45 SEED_GOLD_MAGNET
        0,     // 46 SEED_SPIKEROCK（特例表外，槽位不可达）
        0,     // 47 SEED_COBCANNON
        0,     // 48 SEED_IMITATER
        0,     // 49 SEED_EXPLODE_O_NUT
        0,     // 50 SEED_GIANT_WALLNUT
        0,     // 51 SEED_SPROUT
        0,     // 52 SEED_LEFTPEATER
    };
    static_assert(sizeof(kPlantHeightOffsets) / sizeof(kPlantHeightOffsets[0]) == (int)SeedType::NUM_SEED_TYPES,
        "kPlantHeightOffsets 槽位须与 SeedType 枚举同步");

    if (theSeedType == SeedType::SEED_SPIKEWEED || theSeedType == SeedType::SEED_SPIKEROCK)
    {
        int aBottomRow = 4;
        if (theBoard && theBoard->StageHas6Rows())
        {
            aBottomRow = 5;
        }

        if (theSeedType == SeedType::SEED_SPIKEROCK)
        {
            aHeightOffset += 6.0f;
        }

        if (theBoard && theBoard->GetFlowerPotAt(theCol, theRow) && gLawnApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
        {
            aHeightOffset += 5.0f;
        }
        else if (theBoard && theBoard->StageHasRoof())
        {
            aHeightOffset += 15.0f;
        }
        else if (theBoard && theBoard->IsPoolSquare(theCol, theRow))
        {
            aHeightOffset += 0.0f;
        }
        else if (theRow == aBottomRow && theCol >= 7 && theBoard->StageHas6Rows())
        {
            aHeightOffset += 1.0f;
        }
        else if (theRow == aBottomRow && theCol < 7)
        {
            aHeightOffset += 12.0f;
        }
        else
        {
            aHeightOffset += 15.0f;
        }
    }
    else if ((unsigned)theSeedType < sizeof(kPlantHeightOffsets) / sizeof(kPlantHeightOffsets[0]))
    {
        aHeightOffset += kPlantHeightOffsets[(int)theSeedType];
    }

    (void)thePlant;
    return aHeightOffset;
}

// ---------------- FormC：FormB 逐字 + static constexpr（诊断：免每次调用栈物化） ----------------
__declspec(noinline) float OneC(uint32_t inp)
{
    SeedType theSeedType = (SeedType)(inp % (uint32_t)NUM_SEED_TYPES);
    int theCol = (int)((inp >> 8) & 0xFu);
    int theRow = (int)((inp >> 12) & 0x7u);
    gBoard.mBgFlags = (int)((inp >> 16) & 0xFu);
    gLawnApp->mGameMode = ((inp >> 21) & 1u) ? GAMEMODE_CHALLENGE_ZEN_GARDEN : GAMEMODE_ADVENTURE;
    Board* theBoard = &gBoard;
    Plant* thePlant = nullptr;

    float aHeightOffset = 0.0f;

    static constexpr float kPlantHeightOffsets[] = {
        0,     // 0 SEED_PEASHOOTER
        0,     // 1 SEED_SUNFLOWER
        0,     // 2 SEED_CHERRYBOMB
        0,     // 3 SEED_WALLNUT
        0,     // 4 SEED_POTATOMINE
        0,     // 5 SEED_SNOWPEA
        0,     // 6 SEED_CHOMPER
        0,     // 7 SEED_REPEATER
        5,     // 8 SEED_PUFFSHROOM
        0,     // 9 SEED_SUNSHROOM
        0,     // 10 SEED_FUMESHROOM
        -40,   // 11 SEED_GRAVEBUSTER
        0,     // 12 SEED_HYPNOSHROOM
        -14,   // 13 SEED_SCAREDYSHROOM
        0,     // 14 SEED_ICESHROOM
        0,     // 15 SEED_DOOMSHROOM
        25,    // 16 SEED_LILYPAD
        0,     // 17 SEED_SQUASH
        0,     // 18 SEED_THREEPEATER
        24,    // 19 SEED_TANGLEKELP
        0,     // 20 SEED_JALAPENO
        0,     // 21 SEED_SPIKEWEED（特例表外，槽位不可达）
        0,     // 22 SEED_TORCHWOOD
        0,     // 23 SEED_TALLNUT
        28,    // 24 SEED_SEASHROOM
        0,     // 25 SEED_PLANTERN
        0,     // 26 SEED_CACTUS（原早退 = 不调整）
        0,     // 27 SEED_BLOVER
        0,     // 28 SEED_SPLITPEA
        10,    // 29 SEED_STARFRUIT
        15,    // 30 SEED_PUMPKINSHELL
        0,     // 31 SEED_MAGNETSHROOM
        0,     // 32 SEED_CABBAGEPULT
        26,    // 33 SEED_FLOWERPOT
        0,     // 34 SEED_KERNELPULT
        -20,   // 35 SEED_INSTANT_COFFEE
        0,     // 36 SEED_GARLIC
        0,     // 37 SEED_UMBRELLA
        0,     // 38 SEED_MARIGOLD
        0,     // 39 SEED_MELONPULT
        0,     // 40 SEED_GATLINGPEA
        0,     // 41 SEED_TWINSUNFLOWER
        0,     // 42 SEED_GLOOMSHROOM
        0,     // 43 SEED_CATTAIL
        0,     // 44 SEED_WINTERMELON
        0,     // 45 SEED_GOLD_MAGNET
        0,     // 46 SEED_SPIKEROCK（特例表外，槽位不可达）
        0,     // 47 SEED_COBCANNON
        0,     // 48 SEED_IMITATER
        0,     // 49 SEED_EXPLODE_O_NUT
        0,     // 50 SEED_GIANT_WALLNUT
        0,     // 51 SEED_SPROUT
        0,     // 52 SEED_LEFTPEATER
    };
    static_assert(sizeof(kPlantHeightOffsets) / sizeof(kPlantHeightOffsets[0]) == (int)SeedType::NUM_SEED_TYPES,
        "kPlantHeightOffsets 槽位须与 SeedType 枚举同步");

    if (theSeedType == SeedType::SEED_SPIKEWEED || theSeedType == SeedType::SEED_SPIKEROCK)
    {
        int aBottomRow = 4;
        if (theBoard && theBoard->StageHas6Rows())
        {
            aBottomRow = 5;
        }

        if (theSeedType == SeedType::SEED_SPIKEROCK)
        {
            aHeightOffset += 6.0f;
        }

        if (theBoard && theBoard->GetFlowerPotAt(theCol, theRow) && gLawnApp->mGameMode != GameMode::GAMEMODE_CHALLENGE_ZEN_GARDEN)
        {
            aHeightOffset += 5.0f;
        }
        else if (theBoard && theBoard->StageHasRoof())
        {
            aHeightOffset += 15.0f;
        }
        else if (theBoard && theBoard->IsPoolSquare(theCol, theRow))
        {
            aHeightOffset += 0.0f;
        }
        else if (theRow == aBottomRow && theCol >= 7 && theBoard->StageHas6Rows())
        {
            aHeightOffset += 1.0f;
        }
        else if (theRow == aBottomRow && theCol < 7)
        {
            aHeightOffset += 12.0f;
        }
        else
        {
            aHeightOffset += 15.0f;
        }
    }
    else if ((unsigned)theSeedType < sizeof(kPlantHeightOffsets) / sizeof(kPlantHeightOffsets[0]))
    {
        aHeightOffset += kPlantHeightOffsets[(int)theSeedType];
    }

    (void)thePlant;
    return aHeightOffset;
}

static void RunA() { for (int i = 0; i < kN; i++) MixF(OneA(gInput[i])); }
static void RunB() { for (int i = 0; i < kN; i++) MixF(OneB(gInput[i])); }
static void RunC() { for (int i = 0; i < kN; i++) MixF(OneC(gInput[i])); }

int main()
{
    GenInput();
    uint64_t ca = CheckOnly(RunA);
    uint64_t cb = CheckOnly(RunB);
    uint64_t cc = CheckOnly(RunC);
    double a = TimePerIterNs(RunA, kN);
    double b = TimePerIterNs(RunB, kN);
    double c = TimePerIterNs(RunC, kN);
    printf("bench7 PlantDrawHeightOffset | before %.2f ns | after %.2f ns | diff %.1f%% | C(static) %.2f ns | checksum=%s (A=%016llX B=%016llX C=%016llX)\n",
        a, b, (a - b) / a * 100.0, c, ca == cb && cb == cc ? "OK" : "MISMATCH", (unsigned long long)ca, (unsigned long long)cb, (unsigned long long)cc);
    return (ca == cb && cb == cc) ? 0 : 1;
}
