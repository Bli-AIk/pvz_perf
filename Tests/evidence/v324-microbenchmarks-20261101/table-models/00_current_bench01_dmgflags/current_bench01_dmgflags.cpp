// bench01_dmgflags - 目标1: Plant::GetDamageRangeFlags switch 链 -> kDamageRangeFlags [53][2] static 表
// FormA = 5b90d9f3^ 原链逐字（switch 47 行）；FormB = 当前 HEAD（表带 static）逐字。
// 账本旧数（裸 constexpr 表时代）：6.24 -> 1.45（-77%）。
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
enum PlantWeapon { WEAPON_PRIMARY = 0, WEAPON_SECONDARY = 1 };

static SeedType gSeedType;   // mSeedType 桩（字段保留）
static PlantWeapon gWeapon;  // 入参桩

// ---------------- FormA：原 switch 链（5b90d9f3^ 逐字） ----------------
__declspec(noinline) void OneA(uint32_t inp)
{
    gSeedType = (SeedType)(inp % (uint32_t)NUM_SEED_TYPES);
    gWeapon = (PlantWeapon)((inp >> 16) & 1);
    PlantWeapon thePlantWeapon = gWeapon;
    int r;
    switch (gSeedType)
    {
    case SeedType::SEED_CACTUS:
        r = thePlantWeapon == PlantWeapon::WEAPON_SECONDARY ? 1 : 2;
        break;
    case SeedType::SEED_CHERRYBOMB:
    case SeedType::SEED_JALAPENO:
    case SeedType::SEED_COBCANNON:
    case SeedType::SEED_DOOMSHROOM:
        r = 127;
        break;
    case SeedType::SEED_MELONPULT:
    case SeedType::SEED_CABBAGEPULT:
    case SeedType::SEED_KERNELPULT:
    case SeedType::SEED_WINTERMELON:
        r = 13;
        break;
    case SeedType::SEED_POTATOMINE:
        r = 77;
        break;
    case SeedType::SEED_SQUASH:
        r = 13;
        break;
    case SeedType::SEED_PUFFSHROOM:
    case SeedType::SEED_SEASHROOM:
    case SeedType::SEED_FUMESHROOM:
    case SeedType::SEED_GLOOMSHROOM:
    case SeedType::SEED_CHOMPER:
        r = 9;
        break;
    case SeedType::SEED_CATTAIL:
        r = 11;
        break;
    case SeedType::SEED_TANGLEKELP:
        r = 5;
        break;
    case SeedType::SEED_GIANT_WALLNUT:
        r = 17;
        break;
    default:
        r = 1;
        break;
    }
    MixI(r);
}

// ---------------- FormB：当前 HEAD（static 表逐字） ----------------
__declspec(noinline) void OneB(uint32_t inp)
{
    gSeedType = (SeedType)(inp % (uint32_t)NUM_SEED_TYPES);
    gWeapon = (PlantWeapon)((inp >> 16) & 1);
    PlantWeapon thePlantWeapon = gWeapon;
	static constexpr uint8_t kDamageRangeFlags[][2] = {
		/* SEED_PEASHOOTER        */ { 1, 1 },
		/* SEED_SUNFLOWER         */ { 1, 1 },
		/* SEED_CHERRYBOMB        */ { 127, 127 },
		/* SEED_WALLNUT           */ { 1, 1 },
		/* SEED_POTATOMINE        */ { 77, 77 },
		/* SEED_SNOWPEA           */ { 1, 1 },
		/* SEED_CHOMPER           */ { 9, 9 },
		/* SEED_REPEATER          */ { 1, 1 },
		/* SEED_PUFFSHROOM        */ { 9, 9 },
		/* SEED_SUNSHROOM         */ { 1, 1 },
		/* SEED_FUMESHROOM        */ { 9, 9 },
		/* SEED_GRAVEBUSTER       */ { 1, 1 },
		/* SEED_HYPNOSHROOM       */ { 1, 1 },
		/* SEED_SCAREDYSHROOM     */ { 1, 1 },
		/* SEED_ICESHROOM         */ { 1, 1 },
		/* SEED_DOOMSHROOM        */ { 127, 127 },
		/* SEED_LILYPAD           */ { 1, 1 },
		/* SEED_SQUASH            */ { 13, 13 },
		/* SEED_THREEPEATER       */ { 1, 1 },
		/* SEED_TANGLEKELP        */ { 5, 5 },
		/* SEED_JALAPENO          */ { 127, 127 },
		/* SEED_SPIKEWEED         */ { 1, 1 },
		/* SEED_TORCHWOOD         */ { 1, 1 },
		/* SEED_TALLNUT           */ { 1, 1 },
		/* SEED_SEASHROOM         */ { 9, 9 },
		/* SEED_PLANTERN          */ { 1, 1 },
		/* SEED_CACTUS            */ { 2, 1 },
		/* SEED_BLOVER            */ { 1, 1 },
		/* SEED_SPLITPEA          */ { 1, 1 },
		/* SEED_STARFRUIT         */ { 1, 1 },
		/* SEED_PUMPKINSHELL      */ { 1, 1 },
		/* SEED_MAGNETSHROOM      */ { 1, 1 },
		/* SEED_CABBAGEPULT       */ { 13, 13 },
		/* SEED_FLOWERPOT         */ { 1, 1 },
		/* SEED_KERNELPULT        */ { 13, 13 },
		/* SEED_INSTANT_COFFEE    */ { 1, 1 },
		/* SEED_GARLIC            */ { 1, 1 },
		/* SEED_UMBRELLA          */ { 1, 1 },
		/* SEED_MARIGOLD          */ { 1, 1 },
		/* SEED_MELONPULT         */ { 13, 13 },
		/* SEED_GATLINGPEA        */ { 1, 1 },
		/* SEED_TWINSUNFLOWER     */ { 1, 1 },
		/* SEED_GLOOMSHROOM       */ { 9, 9 },
		/* SEED_CATTAIL           */ { 11, 11 },
		/* SEED_WINTERMELON       */ { 13, 13 },
		/* SEED_GOLD_MAGNET       */ { 1, 1 },
		/* SEED_SPIKEROCK         */ { 1, 1 },
		/* SEED_COBCANNON         */ { 127, 127 },
		/* SEED_IMITATER          */ { 1, 1 },
		/* SEED_EXPLODE_O_NUT     */ { 1, 1 },
		/* SEED_GIANT_WALLNUT     */ { 17, 17 },
		/* SEED_SPROUT            */ { 1, 1 },
		/* SEED_LEFTPEATER        */ { 1, 1 },
	};
    static_assert(sizeof(kDamageRangeFlags) / sizeof(kDamageRangeFlags[0]) == (int)SeedType::NUM_SEED_TYPES,
        "伤害范围槽位须与 SeedType 枚举同步");
    int r = kDamageRangeFlags[gSeedType][(int)thePlantWeapon];
    MixI(r);
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
    printf("bench01 kDamageRangeFlags | before %.2f ns | after %.2f ns | diff %.1f%% | checksum=%s (%016llX/%016llX)\n",
        a, b, (a - b) / a * 100.0, ca == cb ? "OK" : "MISMATCH", (unsigned long long)ca, (unsigned long long)cb);
    return ca == cb ? 0 : 1;
}
