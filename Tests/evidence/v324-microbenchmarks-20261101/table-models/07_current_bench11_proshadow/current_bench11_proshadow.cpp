// bench11_proshadow - 目标11: Projectile::DrawShadow switch -> kShadowParams[14] 五元组 static 表
// FormA = fdab0487^ 原链逐字（switch + PUFF 早退）；FormB = 当前 HEAD（表带 static + skip 位）逐字。
// 账本旧数（裸 constexpr 表时代）：8.73 -> 2.88（-67%）。
// 计时分派区：参数修正 + lobbed 缩放 + 末次绘制桩（两形态同段）。
#include "bench_common.h"

enum ProjectileType
{
    PROJECTILE_PEA = 0, PROJECTILE_SNOWPEA, PROJECTILE_CABBAGE, PROJECTILE_MELON,
    PROJECTILE_PUFF, PROJECTILE_WINTERMELON, PROJECTILE_FIREBALL, PROJECTILE_STAR,
    PROJECTILE_SPIKE, PROJECTILE_BASKETBALL, PROJECTILE_KERNEL, PROJECTILE_COBBIG,
    PROJECTILE_BUTTER, PROJECTILE_ZOMBIE_PEA, NUM_PROJECTILES
};
enum ProjectileMotion { MOTION_NORMAL = 0, MOTION_LOBBED, MOTION_FLOAT_OVER, MOTION_BEE_BACKWARDS };

struct Image { int id; };
static Image gImgShadow{ 0x31 };
static Image* IMAGE_PEA_SHADOWS = &gImgShadow;

struct Graphics { float mScaleX, mScaleY; };

static float ClampFloat(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void TodDrawImageCelScaledFStub(Graphics* g, Image* img, float x, float y, int celCol, int celRow, float sx, float sy)
{
    Mix(0x7E5Fu); MixI(img ? img->id : -1); MixF(x); MixF(y); MixI(celCol); MixI(celRow); MixF(sx); MixF(sy);
}

// 两形态共尾（lobbed 缩放 + 绘制），逐字同段
static void ShadowTail(Graphics* g, ProjectileMotion mMotionType, float mPosZ, float mShadowY, float mPosY,
                       int aCelCol, float aScale, float aStretch, float aOffsetX, float aOffsetY)
{
    if (mMotionType == ProjectileMotion::MOTION_LOBBED)
    {
        float aHeight = ClampFloat(-mPosZ, 0.0f, 200.0f);
        aScale *= 200.0f / (aHeight + 200.0f);
    }
    TodDrawImageCelScaledFStub(g, IMAGE_PEA_SHADOWS, aOffsetX, (mShadowY - mPosY + aOffsetY), aCelCol, 0, aScale * aStretch, aScale);
}

// ---------------- FormA：原 switch（fdab0487^ 逐字，参数分派段） ----------------
__declspec(noinline) void OneA(uint32_t inp)
{
    ProjectileType mProjectileType = (ProjectileType)(inp % (uint32_t)NUM_PROJECTILES);
    ProjectileMotion mMotionType = (ProjectileMotion)((inp >> 12) & 3);
    float mPosX = BitsToF(inp, 0), mPosY = BitsToF(inp, 4);
    int mX = (int)(inp & 0x3F), mY = (int)((inp >> 6) & 0x3F);
    float mPosZ = BitsToF(inp, 8) * 40.0f;
    float mShadowY = BitsToF(inp, 16) * 40.0f;
    Graphics gStorage; Graphics* g = &gStorage;

    int aCelCol = 0;
    float aScale = 1.0f;
    float aStretch = 1.0f;
    float aOffsetX = mPosX - mX;
    float aOffsetY = mPosY - mY;
    int aGridX = (int)((inp >> 20) & 7);            // PixelToGridXKeepOnBoard 确定性桩
    bool isHighGround = ((inp >> 24) & 1) != 0;
    bool mOnHighGround = ((inp >> 25) & 1) != 0;
    if (mOnHighGround && !isHighGround)
    {
        aOffsetY += 20.0f /*HIGH_GROUND_HEIGHT*/;
    }
    else if (!mOnHighGround && isHighGround)
    {
        aOffsetY -= 20.0f;
    }
    if (((inp >> 26) & 1))
    {
        aCelCol = 1;
    }

    switch (mProjectileType)
    {
    case ProjectileType::PROJECTILE_PEA:
    case ProjectileType::PROJECTILE_ZOMBIE_PEA:
        aOffsetX += 3.0f;
        break;

    case ProjectileType::PROJECTILE_SNOWPEA:
        aOffsetX += -1.0f;
        aScale = 1.3f;
        break;

    case ProjectileType::PROJECTILE_STAR:
        aOffsetX += 7.0f;
        break;

    case ProjectileType::PROJECTILE_CABBAGE:
    case ProjectileType::PROJECTILE_KERNEL:
    case ProjectileType::PROJECTILE_BUTTER:
    case ProjectileType::PROJECTILE_MELON:
    case ProjectileType::PROJECTILE_WINTERMELON:
        aOffsetX += 3.0f;
        aOffsetY += 10.0f;
        aScale = 1.6f;
        break;

    case ProjectileType::PROJECTILE_PUFF:
        return;

    case ProjectileType::PROJECTILE_COBBIG:
        aScale = 1.0f;
        aStretch = 3.0f;
        aOffsetX += 57.0f;
        break;

    case ProjectileType::PROJECTILE_FIREBALL:
        aScale = 1.4f;
        break;
    default:
        break;
    }

    ShadowTail(g, mMotionType, mPosZ, mShadowY, mPosY, aCelCol, aScale, aStretch, aOffsetX, aOffsetY);
}

// ---------------- FormB：当前 HEAD（static 表逐字，参数分派段） ----------------
__declspec(noinline) void OneB(uint32_t inp)
{
    ProjectileType mProjectileType = (ProjectileType)(inp % (uint32_t)NUM_PROJECTILES);
    ProjectileMotion mMotionType = (ProjectileMotion)((inp >> 12) & 3);
    float mPosX = BitsToF(inp, 0), mPosY = BitsToF(inp, 4);
    int mX = (int)(inp & 0x3F), mY = (int)((inp >> 6) & 0x3F);
    float mPosZ = BitsToF(inp, 8) * 40.0f;
    float mShadowY = BitsToF(inp, 16) * 40.0f;
    Graphics gStorage; Graphics* g = &gStorage;

    int aCelCol = 0;
    float aScale = 1.0f;
    float aStretch = 1.0f;
    float aOffsetX = mPosX - mX;
    float aOffsetY = mPosY - mY;
    int aGridX = (int)((inp >> 20) & 7);
    bool isHighGround = ((inp >> 24) & 1) != 0;
    bool mOnHighGround = ((inp >> 25) & 1) != 0;
    if (mOnHighGround && !isHighGround)
    {
        aOffsetY += 20.0f /*HIGH_GROUND_HEIGHT*/;
    }
    else if (!mOnHighGround && isHighGround)
    {
        aOffsetY -= 20.0f;
    }
    if (((inp >> 26) & 1))
    {
        aCelCol = 1;
    }

	static constexpr struct { float mDX, mDY, mScale, mStretch; bool mSkip; } kShadowParams[] = {
		{  3,  0, 1.0f, 1.0f, false },  // PROJECTILE_PEA          豌豆
		{ -1,  0, 1.3f, 1.0f, false },  // PROJECTILE_SNOWPEA      冰豌豆
		{  3, 10, 1.6f, 1.0f, false },  // PROJECTILE_CABBAGE      卷心菜
		{  3, 10, 1.6f, 1.0f, false },  // PROJECTILE_MELON        西瓜
		{  0,  0, 1.0f, 1.0f, true  },  // PROJECTILE_PUFF         孢子（无影）
		{  3, 10, 1.6f, 1.0f, false },  // PROJECTILE_WINTERMELON  冰瓜
		{  0,  0, 1.4f, 1.0f, false },  // PROJECTILE_FIREBALL     火豌豆
		{  7,  0, 1.0f, 1.0f, false },  // PROJECTILE_STAR         星星
		{  0,  0, 1.0f, 1.0f, false },  // PROJECTILE_SPIKE        尖刺
		{  0,  0, 1.0f, 1.0f, false },  // PROJECTILE_BASKETBALL   篮球
		{  3, 10, 1.6f, 1.0f, false },  // PROJECTILE_KERNEL       玉米粒
		{ 57,  0, 1.0f, 3.0f, false },  // PROJECTILE_COBBIG       玉米加农炮
		{  3, 10, 1.6f, 1.0f, false },  // PROJECTILE_BUTTER       黄油
		{  3,  0, 1.0f, 1.0f, false },  // PROJECTILE_ZOMBIE_PEA   僵尸豌豆
	};
    static_assert(sizeof(kShadowParams) / sizeof(kShadowParams[0]) == (int)ProjectileType::NUM_PROJECTILES,
        "阴影槽位须与 ProjectileType 枚举同步");
    {
        const auto& aShadow = kShadowParams[mProjectileType];
        if (aShadow.mSkip)
            return;
        aOffsetX += aShadow.mDX;
        aOffsetY += aShadow.mDY;
        aScale = aShadow.mScale;
        aStretch = aShadow.mStretch;
    }

    ShadowTail(g, mMotionType, mPosZ, mShadowY, mPosY, aCelCol, aScale, aStretch, aOffsetX, aOffsetY);
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
    printf("bench11 kShadowParams | before %.2f ns | after %.2f ns | diff %.1f%% | checksum=%s (%016llX/%016llX)\n",
        a, b, (a - b) / a * 100.0, ca == cb ? "OK" : "MISMATCH", (unsigned long long)ca, (unsigned long long)cb);
    return ca == cb ? 0 : 1;
}
