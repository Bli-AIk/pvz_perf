// bench07_walkhead - 目标7: Zombie::GetDrawPos 行走帧头位 switch(11 帧+default) -> kWalkHeadPos[11][2] static 表
// FormA = 5b90d9f3^ 原链逐字；FormB = 当前 HEAD（表带 static + 守卫）逐字。
// 账本旧数（裸 constexpr 表时代）：7.69 -> 1.81（-77%）。
// 域：frame 0..15（越界帧走 default，两形态同值）；mIsEating=false 恒入行走支。
#include "bench_common.h"

struct ZombieDrawPosition
{
    float mImageOffsetX, mImageOffsetY;
    int mHeadX, mHeadY;
    int mArmY;
};

// ---------------- FormA：原 switch（5b90d9f3^ 逐字） ----------------
__declspec(noinline) void OneA(uint32_t inp)
{
    int mFrame = (int)(inp % 16u);
    bool mIsEating = false;
    ZombieDrawPosition theDrawPos;
    theDrawPos.mImageOffsetX = BitsToF(inp, 0);
    theDrawPos.mImageOffsetY = BitsToF(inp, 8);

    if (mIsEating)
    {
        theDrawPos.mHeadX = 47;
        theDrawPos.mHeadY = 4;
    }
    else
    {
        switch (mFrame)
        {
        case 0:
            theDrawPos.mHeadX = 50;
            theDrawPos.mHeadY = 2;
            break;
        case 1:
            theDrawPos.mHeadX = 49;
            theDrawPos.mHeadY = 1;
            break;
        case 2:
            theDrawPos.mHeadX = 49;
            theDrawPos.mHeadY = 2;
            break;
        case 3:
            theDrawPos.mHeadX = 48;
            theDrawPos.mHeadY = 4;
            break;
        case 4:
            theDrawPos.mHeadX = 48;
            theDrawPos.mHeadY = 5;
            break;
        case 5:
            theDrawPos.mHeadX = 48;
            theDrawPos.mHeadY = 4;
            break;
        case 6:
            theDrawPos.mHeadX = 48;
            theDrawPos.mHeadY = 2;
            break;
        case 7:
            theDrawPos.mHeadX = 49;
            theDrawPos.mHeadY = 1;
            break;
        case 8:
            theDrawPos.mHeadX = 49;
            theDrawPos.mHeadY = 2;
            break;
        case 9:
            theDrawPos.mHeadX = 50;
            theDrawPos.mHeadY = 4;
            break;
        case 10:
            theDrawPos.mHeadX = 50;
            theDrawPos.mHeadY = 5;
            break;
        default:
            theDrawPos.mHeadX = 50;
            theDrawPos.mHeadY = 4;
            break;
        }
    }

    theDrawPos.mArmY = theDrawPos.mHeadY / 2;
    MixI(theDrawPos.mHeadX); MixI(theDrawPos.mHeadY); MixI(theDrawPos.mArmY);
}

// ---------------- FormB：当前 HEAD（static 表逐字） ----------------
__declspec(noinline) void OneB(uint32_t inp)
{
    int mFrame = (int)(inp % 16u);
    bool mIsEating = false;
    ZombieDrawPosition theDrawPos;
    theDrawPos.mImageOffsetX = BitsToF(inp, 0);
    theDrawPos.mImageOffsetY = BitsToF(inp, 8);

    if (mIsEating)
    {
        theDrawPos.mHeadX = 47;
        theDrawPos.mHeadY = 4;
    }
    else
    {
        static constexpr int kWalkHeadPos[][2] = {
		{ 50, 2 },   // frame 0
		{ 49, 1 },   // frame 1
		{ 49, 2 },   // frame 2
		{ 48, 4 },   // frame 3
		{ 48, 5 },   // frame 4
		{ 48, 4 },   // frame 5
		{ 48, 2 },   // frame 6
		{ 49, 1 },   // frame 7
		{ 49, 2 },   // frame 8
		{ 50, 4 },   // frame 9
		{ 50, 5 },   // frame 10
        };
        if ((unsigned)mFrame < sizeof(kWalkHeadPos) / sizeof(kWalkHeadPos[0]))
        {
            theDrawPos.mHeadX = kWalkHeadPos[mFrame][0];
            theDrawPos.mHeadY = kWalkHeadPos[mFrame][1];
        }
        else
        {
            theDrawPos.mHeadX = 50;
            theDrawPos.mHeadY = 4;
        }
    }

    theDrawPos.mArmY = theDrawPos.mHeadY / 2;
    MixI(theDrawPos.mHeadX); MixI(theDrawPos.mHeadY); MixI(theDrawPos.mArmY);
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
    printf("bench07 kWalkHeadPos | before %.2f ns | after %.2f ns | diff %.1f%% | checksum=%s (%016llX/%016llX)\n",
        a, b, (a - b) / a * 100.0, ca == cb ? "OK" : "MISMATCH", (unsigned long long)ca, (unsigned long long)cb);
    return ca == cb ? 0 : 1;
}
