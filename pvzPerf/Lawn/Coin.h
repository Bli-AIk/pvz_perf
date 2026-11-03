#pragma once
#include "misc/TimeCounter.h"
#include "GameObject.h"
#include "System/PlayerInfo.h"

class HitResult;
namespace Sexy
{
    class Graphics;
}
using namespace Sexy;

class Coin : public GameObject
{
public:
    float                   mPosX;                  //+0x24
    float                   mPosY;                  //+0x28
    float                   mVelX;                  //+0x2C
    float                   mVelY;                  //+0x30
    float                   mScale;                 //+0x34
    bool                    mDead;                  //+0x38
    std::int64_t            mFadeEndTimeNs;             //+0x3C
    // Collection start position, retained for award curves and widescreen drawing.
    float                   mCollectX;              //+0x40
    float                   mCollectY;              //+0x44
    int                     mGroundY;               //+0x48
    std::int64_t            mBirthTimeNs;               //+0x4C
    bool                    mIsBeingCollected;      //+0x50
    // Ground disappearance time; awards and special collected items reuse it as a timer.
    std::int64_t            mDisappearStartTimeNs; // INT64_MAX means this phase has not started.      //+0x54
    CoinType                mType;                  //+0x58
    CoinMotion              mCoinMotion;            //+0x5C
    AttachmentID            mAttachmentID;          //+0x60
    // Distance before this update's pickup movement, used for scoring, scale and fade.
    float                   mCollectionDistance;    //+0x64
    SeedType                mUsableSeedType;        //+0x68
    PottedPlant             mPottedPlantSpec;       //+0x70
    bool                    mNeedsBouncyArrow;      //+0xC8
    bool                    mHasBouncyArrow;        //+0xC9
    bool                    mHitGround;             //+0xCA
    int                     mTimesDropped;          //+0xCC

public:
    Coin();
    ~Coin();

    void                    CoinInitialize(int theX, int theY, CoinType theCoinType, CoinMotion theCoinMotion);
    void                    MouseDown(int x, int y, int theClickCount);
    bool                    MouseHitTest(int theX, int theY, HitResult* theHitResult);
    bool                    TouchHitTest(int fromX, int fromY, int toX, int toY);
    void                    Die();
    void                    Update();
    void                    Draw(Graphics* g);
    void                    Collect();
    inline int              GetSunValue()
    {
        return mType == CoinType::COIN_SUN ? 25 : mType == CoinType::COIN_SMALLSUN ? 15 : mType == CoinType::COIN_LARGESUN ? 50 : 0;
    }
    static inline int       GetCoinValue(CoinType theCoinType)
    {
        // B 形态 -> 常量表 kCoinValueRows（原 4 支三目链合并；行序 = CoinType 枚举序
        // 0..27，count_last=末值 PRESENT_SURVIVAL_MODE 锚定；0 槽 = 原默认支）
        static constexpr int kCoinValueRows[(int)CoinType::COIN_PRESENT_SURVIVAL_MODE + 1] = {
            0,    // 0 COIN_NONE（原默认支）
            1,    // 1 COIN_SILVER
            5,    // 2 COIN_GOLD
            100,  // 3 COIN_DIAMOND
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 4..27 其余（原默认支）
        };
        static_assert(sizeof(kCoinValueRows) / sizeof(kCoinValueRows[0]) == (int)CoinType::COIN_PRESENT_SURVIVAL_MODE + 1,
            "kCoinValueRows 槽位须与 CoinType 枚举同步");

        return (unsigned)theCoinType < sizeof(kCoinValueRows) / sizeof(kCoinValueRows[0]) ?
            kCoinValueRows[(int)theCoinType] : 0;
    }
    void                    UpdateFade();
    void                    UpdateFall();
    void                    ScoreCoin();
    void                    UpdateCollected();
    Color                   GetColor();
    /*inline*/ bool         IsMoney();
    /*inline*/ bool         IsSun();
    inline float            GetSunScale()
    {
        return mType == CoinType::COIN_SMALLSUN ? 0.5f : mType == CoinType::COIN_LARGESUN ? 2.0f : 1.0f;
    }
    inline bool             IsOnGround() { return false; }
    SeedType                GetFinalSeedPacketType();
    bool                    IsLevelAward();
    bool                    CoinGetsBouncyArrow();
    void                    FanOutCoins(CoinType theCoinType, int theNumCoins);
    int                     GetDisappearTime();
    void                    DroppedUsableSeed();
    void                    PlayCollectSound();
    void                    TryAutoCollectAfterLevelAward();
    bool                    IsPresentWithAdvice();
    void                    PlayLaunchSound();
    void                    PlayGroundSound();

    static /*inline*/ bool  IsMoney(CoinType theType);
};

