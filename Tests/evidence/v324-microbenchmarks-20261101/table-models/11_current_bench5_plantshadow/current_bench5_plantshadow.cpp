// bench5_plantshadow.cpp - 目标5: Plant::DrawShadow 16 支 -> kPlantShadow 53 槽
// 计时区 = 分派链（SQUASH..GIANT_WALLNUT）；前后公共段（守卫/StageIsNight/IsFlying/末尾绘制）两形态逐字相同，不计入
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
enum PlantState { STATE_NORMAL = 0, STATE_SUNSHROOM_SMALL, STATE_SUNSHROOM_GROWING };

struct Reanimation { float mAnimTime; };
struct Board { float GridToPixelY(int col, int row) { return (float)(col * 80 + row * 100 + 37); } };
struct LawnAppStub
{
    Reanimation mReanim;
    Reanimation* ReanimationGet(int id) { (void)id; return &mReanim; }
};

struct Plant
{
    SeedType mSeedType;
    PlantState mState;
    int mBodyReanimID;
    int mPlantCol, mRow;
    float mY;
    Board* mBoard;
    LawnAppStub* mApp;
};
static Board gBoard;
static LawnAppStub gApp;
static Plant gPlant;

static void SetupPlant(uint32_t inp)
{
    gPlant.mSeedType = (SeedType)(inp % (uint32_t)NUM_SEED_TYPES);
    gPlant.mState = (PlantState)((inp >> 20) % 3u);
    gPlant.mBodyReanimID = (int)(inp >> 5);
    gPlant.mPlantCol = (int)((inp >> 8) & 0xFu);
    gPlant.mRow = (int)((inp >> 12) & 0x7u);
    gPlant.mY = BitsToF(inp, 16);
    gPlant.mBoard = &gBoard;
    gPlant.mApp = &gApp;
    gApp.mReanim.mAnimTime = BitsToF(inp, 24);
}

// ---------------- FormA：原 16 支链（逐字） ----------------
__declspec(noinline) void OneA(uint32_t inp)
{
    SetupPlant(inp);
    Plant& p = gPlant;
    float aShadowOffsetX = -3.0f;
    float aShadowOffsetY = 51.0f;
    float aScale = 1.0f;

    if (p.mSeedType == SeedType::SEED_SQUASH)
    {
        if (p.mBoard)
        {
            aShadowOffsetY += p.mBoard->GridToPixelY(p.mPlantCol, p.mRow) - p.mY;
        }
        aShadowOffsetY += 5.0f;
    }
    else if (p.mSeedType == SeedType::SEED_PUFFSHROOM)
    {
        aScale = 0.5f;
        aShadowOffsetY = 42.0f;
    }
    else if (p.mSeedType == SeedType::SEED_SUNSHROOM)
    {
        aShadowOffsetY = 42.0f;
        if (p.mState == PlantState::STATE_SUNSHROOM_SMALL)
        {
            aScale = 0.5f;
        }
        else if (p.mState == PlantState::STATE_SUNSHROOM_GROWING)
        {
            Reanimation* aBodyReanim = p.mApp->ReanimationGet(p.mBodyReanimID);
            aScale = 0.5f + 0.5f * aBodyReanim->mAnimTime;
        }
    }
    else if (p.mSeedType == SeedType::SEED_UMBRELLA)
    {
        aScale = 0.5f;
        aShadowOffsetX = -7.0f;
        aShadowOffsetY = 52.0f;
    }
    else if (p.mSeedType == SeedType::SEED_FUMESHROOM || p.mSeedType == SeedType::SEED_GLOOMSHROOM)
    {
        aScale = 1.3f;
        aShadowOffsetY = 47.0f;
    }
    else if (p.mSeedType == SeedType::SEED_CABBAGEPULT || p.mSeedType == SeedType::SEED_MELONPULT || p.mSeedType == SeedType::SEED_WINTERMELON)
    {
        aShadowOffsetY = 47.0f;
    }
    else if (p.mSeedType == SeedType::SEED_KERNELPULT)
    {
        aShadowOffsetX = 0.0f;
        aShadowOffsetY = 47.0f;
    }
    else if (p.mSeedType == SeedType::SEED_SCAREDYSHROOM)
    {
        aShadowOffsetX = -9.0f;
        aShadowOffsetY = 55.0f;
    }
    else if (p.mSeedType == SeedType::SEED_CHOMPER)
    {
        aShadowOffsetX = -21.0f;
        aShadowOffsetY = 57.0f;
    }
    else if (p.mSeedType == SeedType::SEED_FLOWERPOT)
    {
        aShadowOffsetX = -4.0f;
        aShadowOffsetY = 46.0f;
    }
    else if (p.mSeedType == SeedType::SEED_TALLNUT)
    {
        aShadowOffsetY = 54.0f;
        aScale = 1.3f;
    }
    else if (p.mSeedType == SeedType::SEED_PUMPKINSHELL)
    {
        aShadowOffsetY = 46.0f;
        aScale = 1.4f;
    }
    else if (p.mSeedType == SeedType::SEED_CACTUS)
    {
        aShadowOffsetX = -8.0f;
        aShadowOffsetY = 50.0f;
    }
    else if (p.mSeedType == SeedType::SEED_PLANTERN)
    {
        aShadowOffsetY = 57.0f;
    }
    else if (p.mSeedType == SeedType::SEED_INSTANT_COFFEE)
    {
        aShadowOffsetY = 71.0f;
    }
    else if (p.mSeedType == SeedType::SEED_GIANT_WALLNUT)
    {
        aShadowOffsetX = -33.0f;
        aShadowOffsetY = 56.0f;
        aScale = 1.7f;
    }

    MixF(aShadowOffsetX); MixF(aShadowOffsetY); MixF(aScale);
}

// ---------------- FormB：当前表形态（逐字：SQUASH/SUNSHROOM 特例 + 53 槽表） ----------------
__declspec(noinline) void OneB(uint32_t inp)
{
    SetupPlant(inp);
    Plant& p = gPlant;
    float aShadowOffsetX = -3.0f;
    float aShadowOffsetY = 51.0f;
    float aScale = 1.0f;

    constexpr struct { float mScale; float mOffX; float mOffY; } kPlantShadow[] = {
        { 1.0f,  -3.0f, 51.0f },  // 0 SEED_PEASHOOTER
        { 1.0f,  -3.0f, 51.0f },  // 1 SEED_SUNFLOWER
        { 1.0f,  -3.0f, 51.0f },  // 2 SEED_CHERRYBOMB
        { 1.0f,  -3.0f, 51.0f },  // 3 SEED_WALLNUT
        { 1.0f,  -3.0f, 51.0f },  // 4 SEED_POTATOMINE
        { 1.0f,  -3.0f, 51.0f },  // 5 SEED_SNOWPEA
        { 1.0f, -21.0f, 57.0f },  // 6 SEED_CHOMPER
        { 1.0f,  -3.0f, 51.0f },  // 7 SEED_REPEATER
        { 0.5f,  -3.0f, 42.0f },  // 8 SEED_PUFFSHROOM
        { 1.0f,  -3.0f, 51.0f },  // 9 SEED_SUNSHROOM（特例表外）
        { 1.3f,  -3.0f, 47.0f },  // 10 SEED_FUMESHROOM
        { 1.0f,  -3.0f, 51.0f },  // 11 SEED_GRAVEBUSTER
        { 1.0f,  -3.0f, 51.0f },  // 12 SEED_HYPNOSHROOM
        { 1.0f,  -9.0f, 55.0f },  // 13 SEED_SCAREDYSHROOM
        { 1.0f,  -3.0f, 51.0f },  // 14 SEED_ICESHROOM
        { 1.0f,  -3.0f, 51.0f },  // 15 SEED_DOOMSHROOM
        { 1.0f,  -3.0f, 51.0f },  // 16 SEED_LILYPAD
        { 1.0f,  -3.0f, 51.0f },  // 17 SEED_SQUASH（特例表外）
        { 1.0f,  -3.0f, 51.0f },  // 18 SEED_THREEPEATER
        { 1.0f,  -3.0f, 51.0f },  // 19 SEED_TANGLEKELP
        { 1.0f,  -3.0f, 51.0f },  // 20 SEED_JALAPENO
        { 1.0f,  -3.0f, 51.0f },  // 21 SEED_SPIKEWEED
        { 1.0f,  -3.0f, 51.0f },  // 22 SEED_TORCHWOOD
        { 1.3f,  -3.0f, 54.0f },  // 23 SEED_TALLNUT
        { 1.0f,  -3.0f, 51.0f },  // 24 SEED_SEASHROOM
        { 1.0f,  -3.0f, 57.0f },  // 25 SEED_PLANTERN
        { 1.0f,  -8.0f, 50.0f },  // 26 SEED_CACTUS
        { 1.0f,  -3.0f, 51.0f },  // 27 SEED_BLOVER
        { 1.0f,  -3.0f, 51.0f },  // 28 SEED_SPLITPEA
        { 1.0f,  -3.0f, 51.0f },  // 29 SEED_STARFRUIT
        { 1.4f,  -3.0f, 46.0f },  // 30 SEED_PUMPKINSHELL
        { 1.0f,  -3.0f, 51.0f },  // 31 SEED_MAGNETSHROOM
        { 1.0f,  -3.0f, 47.0f },  // 32 SEED_CABBAGEPULT
        { 1.0f,  -4.0f, 46.0f },  // 33 SEED_FLOWERPOT
        { 1.0f,   0.0f, 47.0f },  // 34 SEED_KERNELPULT
        { 1.0f,  -3.0f, 71.0f },  // 35 SEED_INSTANT_COFFEE
        { 1.0f,  -3.0f, 51.0f },  // 36 SEED_GARLIC
        { 0.5f,  -7.0f, 52.0f },  // 37 SEED_UMBRELLA
        { 1.0f,  -3.0f, 51.0f },  // 38 SEED_MARIGOLD
        { 1.0f,  -3.0f, 47.0f },  // 39 SEED_MELONPULT
        { 1.0f,  -3.0f, 51.0f },  // 40 SEED_GATLINGPEA
        { 1.0f,  -3.0f, 51.0f },  // 41 SEED_TWINSUNFLOWER
        { 1.3f,  -3.0f, 47.0f },  // 42 SEED_GLOOMSHROOM
        { 1.0f,  -3.0f, 51.0f },  // 43 SEED_CATTAIL
        { 1.0f,  -3.0f, 47.0f },  // 44 SEED_WINTERMELON
        { 1.0f,  -3.0f, 51.0f },  // 45 SEED_GOLD_MAGNET
        { 1.0f,  -3.0f, 51.0f },  // 46 SEED_SPIKEROCK
        { 1.0f,  -3.0f, 51.0f },  // 47 SEED_COBCANNON
        { 1.0f,  -3.0f, 51.0f },  // 48 SEED_IMITATER
        { 1.0f,  -3.0f, 51.0f },  // 49 SEED_EXPLODE_O_NUT
        { 1.7f, -33.0f, 56.0f },  // 50 SEED_GIANT_WALLNUT
        { 1.0f,  -3.0f, 51.0f },  // 51 SEED_SPROUT
        { 1.0f,  -3.0f, 51.0f },  // 52 SEED_LEFTPEATER
    };
    static_assert(sizeof(kPlantShadow) / sizeof(kPlantShadow[0]) == (int)SeedType::NUM_SEED_TYPES,
        "kPlantShadow 槽位须与 SeedType 枚举同步");

    if (p.mSeedType == SeedType::SEED_SQUASH)
    {
        if (p.mBoard)
        {
            aShadowOffsetY += p.mBoard->GridToPixelY(p.mPlantCol, p.mRow) - p.mY;
        }
        aShadowOffsetY += 5.0f;
    }
    else if (p.mSeedType == SeedType::SEED_SUNSHROOM)
    {
        aShadowOffsetY = 42.0f;
        if (p.mState == PlantState::STATE_SUNSHROOM_SMALL)
        {
            aScale = 0.5f;
        }
        else if (p.mState == PlantState::STATE_SUNSHROOM_GROWING)
        {
            Reanimation* aBodyReanim = p.mApp->ReanimationGet(p.mBodyReanimID);
            aScale = 0.5f + 0.5f * aBodyReanim->mAnimTime;
        }
    }
    else if ((unsigned)p.mSeedType < sizeof(kPlantShadow) / sizeof(kPlantShadow[0]))
    {
        const auto& aRow = kPlantShadow[(int)p.mSeedType];
        aScale = aRow.mScale;
        aShadowOffsetX = aRow.mOffX;
        aShadowOffsetY = aRow.mOffY;
    }

    MixF(aShadowOffsetX); MixF(aShadowOffsetY); MixF(aScale);
}

// ---------------- FormC：FormB 逐字 + static constexpr（诊断：免每次调用栈物化） ----------------
__declspec(noinline) void OneC(uint32_t inp)
{
    SetupPlant(inp);
    Plant& p = gPlant;
    float aShadowOffsetX = -3.0f;
    float aShadowOffsetY = 51.0f;
    float aScale = 1.0f;

    static constexpr struct { float mScale; float mOffX; float mOffY; } kPlantShadow[] = {
        { 1.0f,  -3.0f, 51.0f },  // 0 SEED_PEASHOOTER（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 1 SEED_SUNFLOWER（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 2 SEED_CHERRYBOMB（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 3 SEED_WALLNUT（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 4 SEED_POTATOMINE（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 5 SEED_SNOWPEA（原 default）
        { 1.0f, -21.0f, 57.0f },  // 6 SEED_CHOMPER
        { 1.0f,  -3.0f, 51.0f },  // 7 SEED_REPEATER（原 default）
        { 0.5f,  -3.0f, 42.0f },  // 8 SEED_PUFFSHROOM
        { 1.0f,  -3.0f, 51.0f },  // 9 SEED_SUNSHROOM（特例表外，槽位不可达）
        { 1.3f,  -3.0f, 47.0f },  // 10 SEED_FUMESHROOM
        { 1.0f,  -3.0f, 51.0f },  // 11 SEED_GRAVEBUSTER（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 12 SEED_HYPNOSHROOM（原 default）
        { 1.0f,  -9.0f, 55.0f },  // 13 SEED_SCAREDYSHROOM
        { 1.0f,  -3.0f, 51.0f },  // 14 SEED_ICESHROOM（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 15 SEED_DOOMSHROOM（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 16 SEED_LILYPAD（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 17 SEED_SQUASH（特例表外，槽位不可达）
        { 1.0f,  -3.0f, 51.0f },  // 18 SEED_THREEPEATER（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 19 SEED_TANGLEKELP（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 20 SEED_JALAPENO（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 21 SEED_SPIKEWEED（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 22 SEED_TORCHWOOD（原 default）
        { 1.3f,  -3.0f, 54.0f },  // 23 SEED_TALLNUT
        { 1.0f,  -3.0f, 51.0f },  // 24 SEED_SEASHROOM（原 default）
        { 1.0f,  -3.0f, 57.0f },  // 25 SEED_PLANTERN
        { 1.0f,  -8.0f, 50.0f },  // 26 SEED_CACTUS
        { 1.0f,  -3.0f, 51.0f },  // 27 SEED_BLOVER（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 28 SEED_SPLITPEA（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 29 SEED_STARFRUIT（原 default）
        { 1.4f,  -3.0f, 46.0f },  // 30 SEED_PUMPKINSHELL
        { 1.0f,  -3.0f, 51.0f },  // 31 SEED_MAGNETSHROOM（原 default）
        { 1.0f,  -3.0f, 47.0f },  // 32 SEED_CABBAGEPULT
        { 1.0f,  -4.0f, 46.0f },  // 33 SEED_FLOWERPOT
        { 1.0f,   0.0f, 47.0f },  // 34 SEED_KERNELPULT
        { 1.0f,  -3.0f, 71.0f },  // 35 SEED_INSTANT_COFFEE
        { 1.0f,  -3.0f, 51.0f },  // 36 SEED_GARLIC（原 default）
        { 0.5f,  -7.0f, 52.0f },  // 37 SEED_UMBRELLA
        { 1.0f,  -3.0f, 51.0f },  // 38 SEED_MARIGOLD（原 default）
        { 1.0f,  -3.0f, 47.0f },  // 39 SEED_MELONPULT（共体）
        { 1.0f,  -3.0f, 51.0f },  // 40 SEED_GATLINGPEA（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 41 SEED_TWINSUNFLOWER（原 default）
        { 1.3f,  -3.0f, 47.0f },  // 42 SEED_GLOOMSHROOM（共体）
        { 1.0f,  -3.0f, 51.0f },  // 43 SEED_CATTAIL（原 default）
        { 1.0f,  -3.0f, 47.0f },  // 44 SEED_WINTERMELON（共体）
        { 1.0f,  -3.0f, 51.0f },  // 45 SEED_GOLD_MAGNET（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 46 SEED_SPIKEROCK（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 47 SEED_COBCANNON（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 48 SEED_IMITATER（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 49 SEED_EXPLODE_O_NUT（原 default）
        { 1.7f, -33.0f, 56.0f },  // 50 SEED_GIANT_WALLNUT
        { 1.0f,  -3.0f, 51.0f },  // 51 SEED_SPROUT（原 default）
        { 1.0f,  -3.0f, 51.0f },  // 52 SEED_LEFTPEATER（原 default）
    };
    static_assert(sizeof(kPlantShadow) / sizeof(kPlantShadow[0]) == (int)SeedType::NUM_SEED_TYPES,
        "kPlantShadow 槽位须与 SeedType 枚举同步");

    if (p.mSeedType == SeedType::SEED_SQUASH)
    {
        if (p.mBoard)
        {
            aShadowOffsetY += p.mBoard->GridToPixelY(p.mPlantCol, p.mRow) - p.mY;
        }
        aShadowOffsetY += 5.0f;
    }
    else if (p.mSeedType == SeedType::SEED_SUNSHROOM)
    {
        aShadowOffsetY = 42.0f;
        if (p.mState == PlantState::STATE_SUNSHROOM_SMALL)
        {
            aScale = 0.5f;
        }
        else if (p.mState == PlantState::STATE_SUNSHROOM_GROWING)
        {
            Reanimation* aBodyReanim = p.mApp->ReanimationGet(p.mBodyReanimID);
            aScale = 0.5f + 0.5f * aBodyReanim->mAnimTime;
        }
    }
    else if ((unsigned)p.mSeedType < sizeof(kPlantShadow) / sizeof(kPlantShadow[0]))
    {
        const auto& aRow = kPlantShadow[(int)p.mSeedType];
        aScale = aRow.mScale;
        aShadowOffsetX = aRow.mOffX;
        aShadowOffsetY = aRow.mOffY;
    }

    MixF(aShadowOffsetX); MixF(aShadowOffsetY); MixF(aScale);
}

static void RunA() { for (int i = 0; i < kN; i++) OneA(gInput[i]); }
static void RunB() { for (int i = 0; i < kN; i++) OneB(gInput[i]); }
static void RunC() { for (int i = 0; i < kN; i++) OneC(gInput[i]); }

int main()
{
    GenInput();
    uint64_t ca = CheckOnly(RunA);
    uint64_t cb = CheckOnly(RunB);
    uint64_t cc = CheckOnly(RunC);
    double a = TimePerIterNs(RunA, kN);
    double b = TimePerIterNs(RunB, kN);
    double c = TimePerIterNs(RunC, kN);
    printf("bench5 PlantDrawShadow | before %.2f ns | after %.2f ns | diff %.1f%% | C(static) %.2f ns | checksum=%s (A=%016llX B=%016llX C=%016llX)\n",
        a, b, (a - b) / a * 100.0, c, ca == cb && cb == cc ? "OK" : "MISMATCH", (unsigned long long)ca, (unsigned long long)cb, (unsigned long long)cc);
    return (ca == cb && cb == cc) ? 0 : 1;
}
