// bench14_seedcost - 目标14: Plant::GetCost 覆盖价 switch(19 case) -> kSeedCostOverrides[19] static 表（尾段直查）
// FormA = fc0f3526^ 原链逐字（beghouled 前置 + switch + default 定义表）；
// FormB = 当前 HEAD（beghouled 5 支短链保留 + static 尾段表 + IMITATER/定义表尾）逐字。
// 账本旧数（GetCost 整数覆盖表时代）：9.86 -> 1.31（-87%，含 StoreScreen 价表同批）。
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
static_assert(SEED_SLOT_MACHINE_SUN == 56 && SEED_ZOMBIE_IMP == 74, "尾段序须与仓库一致");

struct PlantDefinition { int mSeedCost; };
static const PlantDefinition gPlantDefs[75] = {};   // GetPlantDefinition 桩（域外价随索引扰动）
static int gDefProbe = 0;
static const PlantDefinition& GetPlantDefinitionStub(SeedType s)
{
    // 命中次数入 checksum（桩编号），价格 = 索引扰动防折叠
    Mix(0x9D01u + (uint64_t)(int)s);
    gDefProbe++;
    static PlantDefinition defs[75];
    defs[(int)s].mSeedCost = 100 + (int)s;
    return defs[(int)s];
}
static int gGameMode = 0;   // 非 BEGHOULED

// ---------------- FormA：原 switch（fc0f3526^ 逐字，计时段为整个 GetCost） ----------------
__declspec(noinline) int OneA(uint32_t inp)
{
    SeedType theSeedType = (SeedType)(inp % 75u);
    SeedType theImitaterType = ((inp >> 12) & 1) ? (SeedType)((inp >> 13) % 53u) : SeedType::SEED_NONE;

    if (gGameMode == 1 || gGameMode == 2)
    {
        if (theSeedType == SeedType::SEED_REPEATER)
        {
            return 1000;
        }
        else if (theSeedType == SeedType::SEED_FUMESHROOM)
        {
            return 500;
        }
        else if (theSeedType == SeedType::SEED_TALLNUT)
        {
            return 250;
        }
        else if (theSeedType == SeedType::SEED_BEGHOULED_BUTTON_SHUFFLE)
        {
            return 100;
        }
        else if (theSeedType == SeedType::SEED_BEGHOULED_BUTTON_CRATER)
        {
            return 200;
        }
    }

    switch (theSeedType)
    {
    case SeedType::SEED_SLOT_MACHINE_SUN:           return 0;
    case SeedType::SEED_SLOT_MACHINE_DIAMOND:       return 0;
    case SeedType::SEED_ZOMBIQUARIUM_SNORKLE:       return 100;
    case SeedType::SEED_ZOMBIQUARIUM_TROPHY:        return 1000;
    case SeedType::SEED_ZOMBIE_NORMAL:              return 50;
    case SeedType::SEED_ZOMBIE_TRAFFIC_CONE:        return 75;
    case SeedType::SEED_ZOMBIE_POLEVAULTER:         return 75;
    case SeedType::SEED_ZOMBIE_PAIL:                return 125;
    case SeedType::SEED_ZOMBIE_LADDER:              return 150;
    case SeedType::SEED_ZOMBIE_DIGGER:              return 125;
    case SeedType::SEED_ZOMBIE_BUNGEE:              return 125;
    case SeedType::SEED_ZOMBIE_FOOTBALL:            return 175;
    case SeedType::SEED_ZOMBIE_BALLOON:             return 150;
    case SeedType::SEED_ZOMBIE_SCREEN_DOOR:         return 100;
    case SeedType::SEED_ZOMBONI:                    return 175;
    case SeedType::SEED_ZOMBIE_POGO:                return 200;
    case SeedType::SEED_ZOMBIE_DANCER:              return 350;
    case SeedType::SEED_ZOMBIE_GARGANTUAR:          return 300;
    case SeedType::SEED_ZOMBIE_IMP:                 return 50;
    default:
    {
        if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
        {
            const PlantDefinition& aPlantDef = GetPlantDefinitionStub(theImitaterType);
            return aPlantDef.mSeedCost;
        }
        else
        {
            const PlantDefinition& aPlantDef = GetPlantDefinitionStub(theSeedType);
            return aPlantDef.mSeedCost;
        }
    }
    }
}

// ---------------- FormB：当前 HEAD（static 尾段表逐字，计时段为整个 GetCost） ----------------
__declspec(noinline) int OneB(uint32_t inp)
{
    SeedType theSeedType = (SeedType)(inp % 75u);
    SeedType theImitaterType = ((inp >> 12) & 1) ? (SeedType)((inp >> 13) % 53u) : SeedType::SEED_NONE;

    if (gGameMode == 1 || gGameMode == 2)
    {
        if (theSeedType == SeedType::SEED_REPEATER)
        {
            return 1000;
        }
        else if (theSeedType == SeedType::SEED_FUMESHROOM)
        {
            return 500;
        }
        else if (theSeedType == SeedType::SEED_TALLNUT)
        {
            return 250;
        }
        else if (theSeedType == SeedType::SEED_BEGHOULED_BUTTON_SHUFFLE)
        {
            return 100;
        }
        else if (theSeedType == SeedType::SEED_BEGHOULED_BUTTON_CRATER)
        {
            return 200;
        }
    }

    static constexpr int kSeedCostOverrides[] = {
        0,     // SEED_SLOT_MACHINE_SUN
        0,     // SEED_SLOT_MACHINE_DIAMOND
        100,   // SEED_ZOMBIQUARIUM_SNORKLE
        1000,  // SEED_ZOMBIQUARIUM_TROPHY
        50,    // SEED_ZOMBIE_NORMAL
        75,    // SEED_ZOMBIE_TRAFFIC_CONE
        75,    // SEED_ZOMBIE_POLEVAULTER
        125,   // SEED_ZOMBIE_PAIL
        150,   // SEED_ZOMBIE_LADDER
        125,   // SEED_ZOMBIE_DIGGER
        125,   // SEED_ZOMBIE_BUNGEE
        175,   // SEED_ZOMBIE_FOOTBALL
        150,   // SEED_ZOMBIE_BALLOON
        100,   // SEED_ZOMBIE_SCREEN_DOOR
        175,   // SEED_ZOMBONI
        200,   // SEED_ZOMBIE_POGO
        350,   // SEED_ZOMBIE_DANCER
        300,   // SEED_ZOMBIE_GARGANTUAR
        50,    // SEED_ZOMBIE_IMP
    };
    static_assert(sizeof(kSeedCostOverrides) / sizeof(kSeedCostOverrides[0])
        == (int)SeedType::SEED_ZOMBIE_IMP - (int)SeedType::SEED_SLOT_MACHINE_SUN + 1,
        "覆盖价槽位须与 SeedType 枚举尾段同步");
    if (theSeedType >= SeedType::SEED_SLOT_MACHINE_SUN && theSeedType <= SeedType::SEED_ZOMBIE_IMP)
    {
        return kSeedCostOverrides[(int)theSeedType - (int)SeedType::SEED_SLOT_MACHINE_SUN];
    }

    if (theSeedType == SeedType::SEED_IMITATER && theImitaterType != SeedType::SEED_NONE)
    {
        const PlantDefinition& aPlantDef = GetPlantDefinitionStub(theImitaterType);
        return aPlantDef.mSeedCost;
    }
    const PlantDefinition& aPlantDef = GetPlantDefinitionStub(theSeedType);
    return aPlantDef.mSeedCost;
}

static void RunA() { int acc = 0; for (int i = 0; i < kN; i++) acc += OneA(gInput[i]); MixI(acc); }
static void RunB() { int acc = 0; for (int i = 0; i < kN; i++) acc += OneB(gInput[i]); MixI(acc); }

int main()
{
    GenInput();
    uint64_t ca = CheckOnly(RunA);
    uint64_t cb = CheckOnly(RunB);
    double a = TimePerIterNs(RunA, kN);
    double b = TimePerIterNs(RunB, kN);
    printf("bench14 kSeedCostOverrides | before %.2f ns | after %.2f ns | diff %.1f%% | checksum=%s (%016llX/%016llX)\n",
        a, b, (a - b) / a * 100.0, ca == cb ? "OK" : "MISMATCH", (unsigned long long)ca, (unsigned long long)cb);
    return ca == cb ? 0 : 1;
}
