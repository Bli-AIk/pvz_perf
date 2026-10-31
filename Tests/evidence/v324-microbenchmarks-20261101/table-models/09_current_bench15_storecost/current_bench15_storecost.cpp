// bench15_storecost - 目标15: StoreScreen::GetItemCost switch(29 case) -> kStoreItemCosts[31] static 价表
// FormA = fc0f3526^ 原链逐字（前置割草机动态价 + switch + PACKET_UPGRADE 阶梯价 case）；
// FormB = 当前 HEAD（两个动态价前置 + static 表 + 断言尾）逐字。
// 账本旧数：同 14 批（fc0f3526，-87% 为 GetCost 整数覆盖表时代数字）。
#include "bench_common.h"

enum StoreItem
{
    STORE_ITEM_PLANT_GATLINGPEA = 0, STORE_ITEM_PLANT_TWINSUNFLOWER, STORE_ITEM_PLANT_GLOOMSHROOM,
    STORE_ITEM_PLANT_CATTAIL, STORE_ITEM_PLANT_WINTERMELON, STORE_ITEM_PLANT_GOLD_MAGNET,
    STORE_ITEM_PLANT_SPIKEROCK, STORE_ITEM_PLANT_COBCANNON, STORE_ITEM_PLANT_IMITATER,
    STORE_ITEM_BONUS_LAWN_MOWER, STORE_ITEM_POTTED_MARIGOLD_1, STORE_ITEM_POTTED_MARIGOLD_2,
    STORE_ITEM_POTTED_MARIGOLD_3, STORE_ITEM_GOLD_WATERINGCAN, STORE_ITEM_FERTILIZER,
    STORE_ITEM_BUG_SPRAY, STORE_ITEM_PHONOGRAPH, STORE_ITEM_GARDENING_GLOVE,
    STORE_ITEM_MUSHROOM_GARDEN, STORE_ITEM_WHEEL_BARROW, STORE_ITEM_STINKY_THE_SNAIL,
    STORE_ITEM_PACKET_UPGRADE, STORE_ITEM_POOL_CLEANER, STORE_ITEM_ROOF_CLEANER,
    STORE_ITEM_RAKE, STORE_ITEM_AQUARIUM_GARDEN, STORE_ITEM_CHOCOLATE,
    STORE_ITEM_TREE_OF_WISDOM, STORE_ITEM_TREE_FOOD, STORE_ITEM_FIRSTAID,
    STORE_ITEM_PVZ,
    STORE_ITEM_INVALID = -1
};

// mPurchases 桩（确定性）
static int gPurchases[32];
static void InitPurchases()
{
    for (int i = 0; i < 32; i++) gPurchases[i] = i % 4;
}

// ---------------- FormA：原 switch（fc0f3526^ 逐字，整个 GetItemCost） ----------------
__declspec(noinline) int OneA(uint32_t inp)
{
    StoreItem theStoreItem = (StoreItem)(inp % 31u);

    if (theStoreItem == STORE_ITEM_BONUS_LAWN_MOWER)    return gPurchases[STORE_ITEM_BONUS_LAWN_MOWER] ? 500 : 200;
    switch (theStoreItem)
    {
    case STORE_ITEM_PLANT_GATLINGPEA:                   return 500;
    case STORE_ITEM_PLANT_TWINSUNFLOWER:                return 500;
    case STORE_ITEM_PLANT_GLOOMSHROOM:                  return 750;
    case STORE_ITEM_PLANT_CATTAIL:                      return 1000;
    case STORE_ITEM_PLANT_WINTERMELON:                  return 1000;
    case STORE_ITEM_PLANT_GOLD_MAGNET:                  return 300;
    case STORE_ITEM_PLANT_SPIKEROCK:                    return 750;
    case STORE_ITEM_PLANT_COBCANNON:                    return 2000;
    case STORE_ITEM_PLANT_IMITATER:                     return 3000;
    case STORE_ITEM_POTTED_MARIGOLD_1:                  return 250;
    case STORE_ITEM_POTTED_MARIGOLD_2:                  return 250;
    case STORE_ITEM_POTTED_MARIGOLD_3:                  return 250;
    case STORE_ITEM_GOLD_WATERINGCAN:                   return 1000;
    case STORE_ITEM_FERTILIZER:                         return 75;
    case STORE_ITEM_BUG_SPRAY:                          return 100;
    case STORE_ITEM_PHONOGRAPH:                         return 1500;
    case STORE_ITEM_GARDENING_GLOVE:                    return 100;
    case STORE_ITEM_MUSHROOM_GARDEN:                    return 3000;
    case STORE_ITEM_WHEEL_BARROW:                       return 20;
    case STORE_ITEM_STINKY_THE_SNAIL:                   return 300;
    case STORE_ITEM_PACKET_UPGRADE:
    {
        int aPurchase = gPurchases[STORE_ITEM_PACKET_UPGRADE];
        return aPurchase == 0 ? 75 : aPurchase == 1 ? 500 : aPurchase == 2 ? 2000 : 8000;
    }
    case STORE_ITEM_POOL_CLEANER:                       return 100;
    case STORE_ITEM_ROOF_CLEANER:                       return 300;
    case STORE_ITEM_RAKE:                               return 20;
    case STORE_ITEM_AQUARIUM_GARDEN:                    return 3000;
    case STORE_ITEM_TREE_OF_WISDOM:                     return 1000;
    case STORE_ITEM_TREE_FOOD:                          return 250;
    case STORE_ITEM_FIRSTAID:                           return 200;
    default: TOD_ASSERT(false);                              return 0;
    }
}

// ---------------- FormB：当前 HEAD（static 价表逐字，整个 GetItemCost） ----------------
__declspec(noinline) int OneB(uint32_t inp)
{
    StoreItem theStoreItem = (StoreItem)(inp % 31u);

    if (theStoreItem == STORE_ITEM_BONUS_LAWN_MOWER)    return gPurchases[STORE_ITEM_BONUS_LAWN_MOWER] ? 500 : 200;
    if (theStoreItem == STORE_ITEM_PACKET_UPGRADE)
    {
        int aPurchase = gPurchases[STORE_ITEM_PACKET_UPGRADE];
        return aPurchase == 0 ? 75 : aPurchase == 1 ? 500 : aPurchase == 2 ? 2000 : 8000;
    }

    static constexpr int kStoreItemCosts[] = {
        500,    // STORE_ITEM_PLANT_GATLINGPEA
        500,    // STORE_ITEM_PLANT_TWINSUNFLOWER
        750,    // STORE_ITEM_PLANT_GLOOMSHROOM
        1000,   // STORE_ITEM_PLANT_CATTAIL
        1000,   // STORE_ITEM_PLANT_WINTERMELON
        300,    // STORE_ITEM_PLANT_GOLD_MAGNET
        750,    // STORE_ITEM_PLANT_SPIKEROCK
        2000,   // STORE_ITEM_PLANT_COBCANNON
        3000,   // STORE_ITEM_PLANT_IMITATER
        -1,     // STORE_ITEM_BONUS_LAWN_MOWER（动态价，前置分支处理）
        250,    // STORE_ITEM_POTTED_MARIGOLD_1
        250,    // STORE_ITEM_POTTED_MARIGOLD_2
        250,    // STORE_ITEM_POTTED_MARIGOLD_3
        1000,   // STORE_ITEM_GOLD_WATERINGCAN
        75,     // STORE_ITEM_FERTILIZER
        100,    // STORE_ITEM_BUG_SPRAY
        1500,   // STORE_ITEM_PHONOGRAPH
        100,    // STORE_ITEM_GARDENING_GLOVE
        3000,   // STORE_ITEM_MUSHROOM_GARDEN
        20,     // STORE_ITEM_WHEEL_BARROW
        300,    // STORE_ITEM_STINKY_THE_SNAIL
        -1,     // STORE_ITEM_PACKET_UPGRADE（阶梯价，前置分支处理）
        100,    // STORE_ITEM_POOL_CLEANER
        300,    // STORE_ITEM_ROOF_CLEANER
        20,     // STORE_ITEM_RAKE
        3000,   // STORE_ITEM_AQUARIUM_GARDEN
        -1,     // 库存保留槽，不是商店商品
        1000,   // STORE_ITEM_TREE_OF_WISDOM
        250,    // STORE_ITEM_TREE_FOOD
        200,    // STORE_ITEM_FIRSTAID
    };
    if ((unsigned)theStoreItem < sizeof(kStoreItemCosts) / sizeof(kStoreItemCosts[0]) && kStoreItemCosts[theStoreItem] >= 0)
    {
        return kStoreItemCosts[theStoreItem];
    }
    TOD_ASSERT(false);
    return 0;
}

static void RunA() { int acc = 0; for (int i = 0; i < kN; i++) acc += OneA(gInput[i]); MixI(acc); }
static void RunB() { int acc = 0; for (int i = 0; i < kN; i++) acc += OneB(gInput[i]); MixI(acc); }

int main()
{
    GenInput();
    InitPurchases();
    uint64_t ca = CheckOnly(RunA);
    uint64_t cb = CheckOnly(RunB);
    double a = TimePerIterNs(RunA, kN);
    double b = TimePerIterNs(RunB, kN);
    printf("bench15 kStoreItemCosts | before %.2f ns | after %.2f ns | diff %.1f%% | checksum=%s (%016llX/%016llX)\n",
        a, b, (a - b) / a * 100.0, ca == cb ? "OK" : "MISMATCH", (unsigned long long)ca, (unsigned long long)cb);
    return ca == cb ? 0 : 1;
}
