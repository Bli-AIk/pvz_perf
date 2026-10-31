// bench06_zimgoff - 目标6: Zombie::GetDrawPos 体型偏移 switch -> kZombieImageOffsetFix[36] static 表
// FormA = fc0f3526^ 原链逐字（switch 11 组）；FormB = 当前 HEAD（表带 static + 守卫）逐字。
// 账本旧数（裸 constexpr 表时代）：5.90 -> 1.55（-74%）。
// 计时分派区：mArmY 之后至偏移累加；域 ztype 0..33。
#include "bench_common.h"

enum ZombieType
{
    ZOMBIE_NORMAL = 0, ZOMBIE_FLAG, ZOMBIE_TRAFFIC_CONE, ZOMBIE_POLEVAULTER, ZOMBIE_PAIL,
    ZOMBIE_NEWSPAPER, ZOMBIE_DOOR, ZOMBIE_FOOTBALL, ZOMBIE_DANCER, ZOMBIE_BACKUP_DANCER,
    ZOMBIE_DUCKY_TUBE, ZOMBIE_SNORKEL, ZOMBIE_ZAMBONI, ZOMBIE_BOBSLED, ZOMBIE_DOLPHIN_RIDER,
    ZOMBIE_JACK_IN_THE_BOX, ZOMBIE_BALLOON, ZOMBIE_DIGGER, ZOMBIE_POGO, ZOMBIE_YETI,
    ZOMBIE_BUNGEE, ZOMBIE_LADDER, ZOMBIE_CATAPULT, ZOMBIE_GARGANTUAR, ZOMBIE_IMP,
    ZOMBIE_BOSS, ZOMBIE_PEA_HEAD, ZOMBIE_WALLNUT_HEAD, ZOMBIE_JALAPENO_HEAD, ZOMBIE_GATLING_HEAD,
    ZOMBIE_SQUASH_HEAD, ZOMBIE_TALLNUT_HEAD, ZOMBIE_REDEYE_GARGANTUAR, ZOMBIE_ZOMBATAR,
    NUM_ZOMBIE_TYPES
};

struct ZombieDrawPosition
{
    float mImageOffsetX, mImageOffsetY;
    int mHeadX, mHeadY;
    int mArmY;
};

static ZombieType gZombieType;

// ---------------- FormA：原 switch（fc0f3526^ 逐字） ----------------
__declspec(noinline) void OneA(uint32_t inp)
{
    gZombieType = (ZombieType)(inp % (uint32_t)NUM_ZOMBIE_TYPES);
    ZombieDrawPosition theDrawPos;
    theDrawPos.mImageOffsetX = BitsToF(inp, 0);
    theDrawPos.mImageOffsetY = BitsToF(inp, 8);
    theDrawPos.mHeadY = (int)(inp & 7);
    theDrawPos.mArmY = theDrawPos.mHeadY / 2;

    switch (gZombieType)
    {
    case ZombieType::ZOMBIE_FOOTBALL:
        theDrawPos.mImageOffsetY -= 16.0f;
        break;
    case ZombieType::ZOMBIE_YETI:
        theDrawPos.mImageOffsetY -= 20.0f;
        break;
    case ZombieType::ZOMBIE_CATAPULT:
        theDrawPos.mImageOffsetX -= 25.0f;
        theDrawPos.mImageOffsetY -= 18.0f;
        break;
    case ZombieType::ZOMBIE_POGO:
        theDrawPos.mImageOffsetY += 16.0f;
        break;
    case ZombieType::ZOMBIE_BALLOON:
        theDrawPos.mImageOffsetY += 17.0f;
        break;
    case ZombieType::ZOMBIE_POLEVAULTER:
        theDrawPos.mImageOffsetX -= 6.0f;
        theDrawPos.mImageOffsetY -= 11.0f;
        break;
    case ZombieType::ZOMBIE_ZAMBONI:
        theDrawPos.mImageOffsetX += 68.0f;
        theDrawPos.mImageOffsetY -= 23.0f;
        break;
    case ZombieType::ZOMBIE_GARGANTUAR:
    case ZombieType::ZOMBIE_REDEYE_GARGANTUAR:
        theDrawPos.mImageOffsetY -= 8.0f;
        break;
    case ZombieType::ZOMBIE_BOBSLED:
        theDrawPos.mImageOffsetY -= 12.0f;
        break;
    default:
        break;
    }
    MixF(theDrawPos.mImageOffsetX); MixF(theDrawPos.mImageOffsetY);
}

// ---------------- FormB：当前 HEAD（static 表逐字） ----------------
__declspec(noinline) void OneB(uint32_t inp)
{
    gZombieType = (ZombieType)(inp % (uint32_t)NUM_ZOMBIE_TYPES);
    ZombieDrawPosition theDrawPos;
    theDrawPos.mImageOffsetX = BitsToF(inp, 0);
    theDrawPos.mImageOffsetY = BitsToF(inp, 8);
    theDrawPos.mHeadY = (int)(inp & 7);
    theDrawPos.mArmY = theDrawPos.mHeadY / 2;

    static constexpr struct { float mDX; float mDY; } kZombieImageOffsetFix[] = {
		{ 0, 0 },   //  0 ZOMBIE_NORMAL                    普通僵尸
		{ 0, 0 },   //  1 ZOMBIE_FLAG                      旗帜僵尸
		{ 0, 0 },   //  2 ZOMBIE_TRAFFIC_CONE              路障僵尸
		{ -6, -11 },   //  3 ZOMBIE_POLEVAULTER               撑杆僵尸
		{ 0, 0 },   //  4 ZOMBIE_PAIL                      铁桶僵尸
		{ 0, 0 },   //  5 ZOMBIE_NEWSPAPER                 读报僵尸
		{ 0, 0 },   //  6 ZOMBIE_DOOR                      铁门僵尸
		{ 0, -16 },   //  7 ZOMBIE_FOOTBALL                  橄榄球僵尸
		{ 0, 0 },   //  8 ZOMBIE_DANCER                    舞王僵尸
		{ 0, 0 },   //  9 ZOMBIE_BACKUP_DANCER             伴舞僵尸
		{ 0, 0 },   // 10 ZOMBIE_DUCKY_TUBE                鸭子救生圈僵尸
		{ 0, 0 },   // 11 ZOMBIE_SNORKEL                   潜水僵尸
		{ 68, -23 },   // 12 ZOMBIE_ZAMBONI                   雪橇车僵尸
		{ 0, -12 },   // 13 ZOMBIE_BOBSLED                   雪橇小队僵尸
		{ 0, 0 },   // 14 ZOMBIE_DOLPHIN_RIDER             海豚骑士僵尸
		{ 0, 0 },   // 15 ZOMBIE_JACK_IN_THE_BOX           玩偶匣僵尸
		{ 0, 17 },   // 16 ZOMBIE_BALLOON                   气球僵尸
		{ 0, 0 },   // 17 ZOMBIE_DIGGER                    矿工僵尸
		{ 0, 16 },   // 18 ZOMBIE_POGO                      跳跳僵尸
		{ 0, -20 },   // 19 ZOMBIE_YETI                      雪人僵尸
		{ 0, 0 },   // 20 ZOMBIE_BUNGEE                    蹦极僵尸
		{ 0, 0 },   // 21 ZOMBIE_LADDER                    梯子僵尸
		{ -25, -18 },   // 22 ZOMBIE_CATAPULT                  投石车僵尸
		{ 0, -8 },   // 23 ZOMBIE_GARGANTUAR                伽刚特尔
		{ 0, 0 },   // 24 ZOMBIE_IMP                       小鬼
		{ 0, 0 },   // 25 ZOMBIE_BOSS                      僵王博士
		{ 0, 0 },   // 26 ZOMBIE_PEA_HEAD                  豌豆头
		{ 0, 0 },   // 27 ZOMBIE_WALLNUT_HEAD              坚果头
		{ 0, 0 },   // 28 ZOMBIE_JALAPENO_HEAD             辣椒头
		{ 0, 0 },   // 29 ZOMBIE_GATLING_HEAD              机枪头
		{ 0, 0 },   // 30 ZOMBIE_SQUASH_HEAD               倭瓜头
		{ 0, 0 },   // 31 ZOMBIE_TALLNUT_HEAD              高坚果头
		{ 0, -8 },   // 32 ZOMBIE_REDEYE_GARGANTUAR         红眼伽刚特尔
		{ 0, 0 },   // 33 ZOMBIE_ZOMBATAR                  自定义僵尸
		{ 0, 0 },   // 34 NUM_ZOMBIE_TYPES                 哨兵位
		{ 0, 0 },   // 35 ZOMBIE_CACHED_POLEVAULTER_WITH_POLE 缓存撑杆
    };
    if ((unsigned)gZombieType < sizeof(kZombieImageOffsetFix) / sizeof(kZombieImageOffsetFix[0]))
    {
        theDrawPos.mImageOffsetX += kZombieImageOffsetFix[gZombieType].mDX;
        theDrawPos.mImageOffsetY += kZombieImageOffsetFix[gZombieType].mDY;
    }
    MixF(theDrawPos.mImageOffsetX); MixF(theDrawPos.mImageOffsetY);
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
    printf("bench06 kZombieImageOffsetFix | before %.2f ns | after %.2f ns | diff %.1f%% | checksum=%s (%016llX/%016llX)\n",
        a, b, (a - b) / a * 100.0, ca == cb ? "OK" : "MISMATCH", (unsigned long long)ca, (unsigned long long)cb);
    return ca == cb ? 0 : 1;
}
