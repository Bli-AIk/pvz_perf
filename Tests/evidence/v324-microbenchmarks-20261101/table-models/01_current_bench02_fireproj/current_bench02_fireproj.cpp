// bench02_fireproj - 目标2: Plant::Fire 弹种映射 switch -> kFireProjectiles [53][2] static 表
// FormA = 5b90d9f3^ 原链逐字（switch + KERNELPULT 黄油覆盖）；
// FormB = 当前 HEAD（表带 static + 边界守卫）逐字。
// 账本旧数（裸 constexpr 表时代）：4.21 -> 1.44（-66%）。
// 域说明：原链 default = TOD_ASSERT + 未初始化（原版不可达 UB），对拍域限 17 个射手型
//（switch 覆盖域），两形态同数组。
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
enum ProjectileType
{
    PROJECTILE_PEA = 0, PROJECTILE_SNOWPEA, PROJECTILE_CABBAGE, PROJECTILE_MELON,
    PROJECTILE_PUFF, PROJECTILE_WINTERMELON, PROJECTILE_FIREBALL, PROJECTILE_STAR,
    PROJECTILE_SPIKE, PROJECTILE_BASKETBALL, PROJECTILE_KERNEL, PROJECTILE_COBBIG,
    PROJECTILE_BUTTER, PROJECTILE_ZOMBIE_PEA, NUM_PROJECTILES
};

// 对拍域：switch 覆盖的 17 个射手型（原链 default 不可达）
static const SeedType kDomain[] = {
    SEED_PEASHOOTER, SEED_REPEATER, SEED_THREEPEATER, SEED_SPLITPEA, SEED_GATLINGPEA,
    SEED_LEFTPEATER, SEED_SNOWPEA, SEED_PUFFSHROOM, SEED_SCAREDYSHROOM, SEED_SEASHROOM,
    SEED_CACTUS, SEED_CATTAIL, SEED_CABBAGEPULT, SEED_KERNELPULT, SEED_MELONPULT,
    SEED_WINTERMELON, SEED_COBCANNON,
};
static const int kDomainN = sizeof(kDomain) / sizeof(kDomain[0]);

static SeedType gSeedType;
static PlantWeapon gWeapon;

// ---------------- FormA：原 switch + 黄油覆盖（5b90d9f3^ 逐字，计时分派区） ----------------
__declspec(noinline) void OneA(uint32_t inp)
{
    gSeedType = kDomain[inp % kDomainN];
    gWeapon = (PlantWeapon)((inp >> 20) & 1);
    PlantWeapon thePlantWeapon = gWeapon;
    ProjectileType aProjectileType;
    switch (gSeedType)
    {
    case SeedType::SEED_PEASHOOTER:
    case SeedType::SEED_REPEATER:
    case SeedType::SEED_THREEPEATER:
    case SeedType::SEED_SPLITPEA:
    case SeedType::SEED_GATLINGPEA:
    case SeedType::SEED_LEFTPEATER:
        aProjectileType = ProjectileType::PROJECTILE_PEA;
        break;
    case SeedType::SEED_SNOWPEA:
        aProjectileType = ProjectileType::PROJECTILE_SNOWPEA;
        break;
    case SeedType::SEED_PUFFSHROOM:
    case SeedType::SEED_SCAREDYSHROOM:
    case SeedType::SEED_SEASHROOM:
        aProjectileType = ProjectileType::PROJECTILE_PUFF;
        break;
    case SeedType::SEED_CACTUS:
    case SeedType::SEED_CATTAIL:
        aProjectileType = ProjectileType::PROJECTILE_SPIKE;
        break;
    case SeedType::SEED_CABBAGEPULT:
        aProjectileType = ProjectileType::PROJECTILE_CABBAGE;
        break;
    case SeedType::SEED_KERNELPULT:
        aProjectileType = ProjectileType::PROJECTILE_KERNEL;
        break;
    case SeedType::SEED_MELONPULT:
        aProjectileType = ProjectileType::PROJECTILE_MELON;
        break;
    case SeedType::SEED_WINTERMELON:
        aProjectileType = ProjectileType::PROJECTILE_WINTERMELON;
        break;
    case SeedType::SEED_COBCANNON:
        aProjectileType = ProjectileType::PROJECTILE_COBBIG;
        break;
    default:
        TOD_ASSERT(false);
        break;
    }
    if (gSeedType == SeedType::SEED_KERNELPULT && thePlantWeapon == PlantWeapon::WEAPON_SECONDARY)
    {
        aProjectileType = ProjectileType::PROJECTILE_BUTTER;
    }
    MixI((int)aProjectileType);
}

// ---------------- FormB：当前 HEAD（static 表逐字，计时分派区） ----------------
__declspec(noinline) void OneB(uint32_t inp)
{
    gSeedType = kDomain[inp % kDomainN];
    gWeapon = (PlantWeapon)((inp >> 20) & 1);
    PlantWeapon thePlantWeapon = gWeapon;
    ProjectileType aProjectileType;
    static constexpr ProjectileType kFireProjectiles[][2] = {
		/* SEED_PEASHOOTER        */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SUNFLOWER         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_CHERRYBOMB        */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_WALLNUT           */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_POTATOMINE        */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SNOWPEA           */ { PROJECTILE_SNOWPEA, PROJECTILE_SNOWPEA },
		/* SEED_CHOMPER           */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_REPEATER          */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_PUFFSHROOM        */ { PROJECTILE_PUFF, PROJECTILE_PUFF },
		/* SEED_SUNSHROOM         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_FUMESHROOM        */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_GRAVEBUSTER       */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_HYPNOSHROOM       */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SCAREDYSHROOM     */ { PROJECTILE_PUFF, PROJECTILE_PUFF },
		/* SEED_ICESHROOM         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_DOOMSHROOM        */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_LILYPAD           */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SQUASH            */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_THREEPEATER       */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_TANGLEKELP        */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_JALAPENO          */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SPIKEWEED         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_TORCHWOOD         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_TALLNUT           */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SEASHROOM         */ { PROJECTILE_PUFF, PROJECTILE_PUFF },
		/* SEED_PLANTERN          */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_CACTUS            */ { PROJECTILE_SPIKE, PROJECTILE_SPIKE },
		/* SEED_BLOVER            */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SPLITPEA          */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_STARFRUIT         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_PUMPKINSHELL      */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_MAGNETSHROOM      */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_CABBAGEPULT       */ { PROJECTILE_CABBAGE, PROJECTILE_CABBAGE },
		/* SEED_FLOWERPOT         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_KERNELPULT        */ { PROJECTILE_KERNEL, PROJECTILE_BUTTER },
		/* SEED_INSTANT_COFFEE    */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_GARLIC            */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_UMBRELLA          */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_MARIGOLD          */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_MELONPULT         */ { PROJECTILE_MELON, PROJECTILE_MELON },
		/* SEED_GATLINGPEA        */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_TWINSUNFLOWER     */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_GLOOMSHROOM       */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_CATTAIL           */ { PROJECTILE_SPIKE, PROJECTILE_SPIKE },
		/* SEED_WINTERMELON       */ { PROJECTILE_WINTERMELON, PROJECTILE_WINTERMELON },
		/* SEED_GOLD_MAGNET       */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SPIKEROCK         */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_COBCANNON         */ { PROJECTILE_COBBIG, PROJECTILE_COBBIG },
		/* SEED_IMITATER          */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_EXPLODE_O_NUT     */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_GIANT_WALLNUT     */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_SPROUT            */ { PROJECTILE_PEA, PROJECTILE_PEA },
		/* SEED_LEFTPEATER        */ { PROJECTILE_PEA, PROJECTILE_PEA },
    };
    static_assert(sizeof(kFireProjectiles) / sizeof(kFireProjectiles[0]) == (int)SeedType::NUM_SEED_TYPES,
        "弹种槽位须与 SeedType 枚举同步");
    if ((unsigned)gSeedType < sizeof(kFireProjectiles) / sizeof(kFireProjectiles[0]))
    {
        aProjectileType = kFireProjectiles[gSeedType][(int)thePlantWeapon];
    }
    else
    {
        aProjectileType = ProjectileType::PROJECTILE_PEA;   // 原版不可达 default 定值
    }
    MixI((int)aProjectileType);
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
    printf("bench02 kFireProjectiles | before %.2f ns | after %.2f ns | diff %.1f%% | checksum=%s (%016llX/%016llX)\n",
        a, b, (a - b) / a * 100.0, ca == cb ? "OK" : "MISMATCH", (unsigned long long)ca, (unsigned long long)cb);
    return ca == cb ? 0 : 1;
}
