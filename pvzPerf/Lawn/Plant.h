#pragma once
#include "misc/TimeCounter.h"

#include <string>
#include "GameObject.h"
#include "Sexy.TodLib/ReanimTrackId.h"


#define MAX_MAGNET_ITEMS 5

enum PlantSubClass
{
    SUBCLASS_NORMAL = 0,
    SUBCLASS_SHOOTER = 1
};

enum PlantWeapon
{
    WEAPON_PRIMARY,
    WEAPON_SECONDARY
};

enum PlantOnBungeeState
{
    NOT_ON_BUNGEE,
    GETTING_GRABBED_BY_BUNGEE,
    RISING_WITH_BUNGEE
};

enum PlantState
{
    STATE_NOTREADY,
    STATE_READY,
    STATE_DOINGSPECIAL,
    STATE_SQUASH_LOOK,
    STATE_SQUASH_PRE_LAUNCH,
    STATE_SQUASH_RISING,
    STATE_SQUASH_FALLING,
    STATE_SQUASH_DONE_FALLING,
    STATE_GRAVEBUSTER_LANDING,
    STATE_GRAVEBUSTER_EATING,
    STATE_CHOMPER_BITING,
    STATE_CHOMPER_BITING_GOT_ONE,
    STATE_CHOMPER_BITING_MISSED,
    STATE_CHOMPER_DIGESTING,
    STATE_CHOMPER_SWALLOWING,
    STATE_POTATO_RISING,
    STATE_POTATO_ARMED,
    STATE_POTATO_MASHED,
    STATE_SPIKEWEED_ATTACKING,
    STATE_SPIKEWEED_ATTACKING_2,
    STATE_SCAREDYSHROOM_LOWERING,
    STATE_SCAREDYSHROOM_SCARED,
    STATE_SCAREDYSHROOM_RAISING,
    STATE_SUNSHROOM_SMALL,
    STATE_SUNSHROOM_GROWING,
    STATE_SUNSHROOM_BIG,
    STATE_MAGNETSHROOM_SUCKING,
    STATE_MAGNETSHROOM_CHARGING,
    STATE_BOWLING_UP,
    STATE_BOWLING_DOWN,
    STATE_CACTUS_LOW,
    STATE_CACTUS_RISING,
    STATE_CACTUS_HIGH,
    STATE_CACTUS_LOWERING,
    STATE_TANGLEKELP_GRABBING,
    STATE_COBCANNON_ARMING,
    STATE_COBCANNON_LOADING,
    STATE_COBCANNON_READY,
    STATE_COBCANNON_FIRING,
    STATE_KERNELPULT_BUTTER,
    STATE_UMBRELLA_TRIGGERED,
    STATE_UMBRELLA_REFLECTING,
    STATE_IMITATER_MORPHING,
    STATE_ZEN_GARDEN_WATERED,
    STATE_ZEN_GARDEN_NEEDY,
    STATE_ZEN_GARDEN_HAPPY,
    STATE_MARIGOLD_ENDING,
    STATE_FLOWERPOT_INVULNERABLE,
    STATE_LILYPAD_INVULNERABLE
};

// PLANT_LAYER 枚举删除（2026-10-03 单消费点普查：全库零引用死枚举）

enum PLANT_ORDER
{
    PLANT_ORDER_LILYPAD,
    PLANT_ORDER_NORMAL,
    PLANT_ORDER_PUMPKIN,
    PLANT_ORDER_FLYER,
    PLANT_ORDER_CHERRYBOMB
};

enum MagnetItemType
{
    MAGNET_ITEM_NONE,
    MAGNET_ITEM_PAIL_1,
    MAGNET_ITEM_PAIL_2,
    MAGNET_ITEM_PAIL_3,
    MAGNET_ITEM_FOOTBALL_HELMET_1,
    MAGNET_ITEM_FOOTBALL_HELMET_2,
    MAGNET_ITEM_FOOTBALL_HELMET_3,
    MAGNET_ITEM_DOOR_1,
    MAGNET_ITEM_DOOR_2,
    MAGNET_ITEM_DOOR_3,
    //MAGNET_ITEM_PROPELLER,
    MAGNET_ITEM_POGO_1,
    MAGNET_ITEM_POGO_2,
    MAGNET_ITEM_POGO_3,
    MAGNET_ITEM_JACK_IN_THE_BOX,
    MAGNET_ITEM_LADDER_1,
    MAGNET_ITEM_LADDER_2,
    MAGNET_ITEM_LADDER_3,
    MAGNET_ITEM_LADDER_PLACED,
    MAGNET_ITEM_SILVER_COIN,
    MAGNET_ITEM_GOLD_COIN,
    MAGNET_ITEM_DIAMOND,
    MAGNET_ITEM_PICK_AXE
};

class MagnetItem
{
public:
    float                   mPosX;                          //+0x0
    float                   mPosY;                          //+0x4
    float                   mDestOffsetX;                   //+0x8
    float                   mDestOffsetY;                   //+0xC
    MagnetItemType          mItemType;                      //+0x10
    // Discrete inverse-square attraction velocity, in pixels/second.
    float                   mVelocityX = 0;
    float                   mVelocityY = 0;
};

class Coin;
class Zombie;
class Reanimation;
class TodParticleSystem;

class Plant : public GameObject
{
public:
    double                 mRootOriginX = 0;
    std::uint32_t           mRootBindingGeneration = 0;
    float                  mRootScale = 0;
    std::int64_t            mBowlingRowArrivalTimeNs = 0;
    SeedType                mSeedType;                      //+0x24
    int                     mPlantCol;                      //+0x28
    Sexy::TimeCounter                     mAnimCounter;                   //+0x2C
    int                     mFrame;                         //+0x30
    Sexy::TimeCounter                     mFrameLength;                   //+0x34
    int                     mNumFrames;                     //+0x38
    PlantState              mState;                         //+0x3C
    int                     mPlantHealth;                   //+0x40
    int                     mPlantMaxHealth;                //+0x44
    int                     mSubclass;                      //+0x48
    std::int64_t                     mDisappearEndTimeNs;            //+0x4C
    Sexy::TimeCounter                     mDoSpecialCountdown;            //+0x50
    Sexy::TimeCounter                     mStateCountdown;                //+0x54
    // ZenGarden production duration; mutually exclusive bowling counts impacts
    // in legacy units in this same historical auxiliary slot.
    Sexy::TimeCounter                     mZenProductionCountdown;       //+0x58
    std::int64_t            mNextLaunchTimeNs = 0;
    std::int64_t            mLaunchWindowStartNs = 0;
    // All items in one magnet suction start together. Integer elapsed time is
    // shared across the batch and pauses when this plant's abilities stop.
    std::int64_t            mMagnetStartTimeNs = 0;
    bool                    mLaunchClockActive = false;
    Sexy::TimeCounter                     mLaunchRate;                    //+0x5C
    Rect                    mPlantRect;                     //+0x60
    Rect                    mPlantAttackRect;               //+0x70
    int                     mTargetX;                       //+0x80
    int                     mTargetY;                       //+0x84
    int                     mStartRow;                      //+0x88
    ParticleSystemID        mParticleID;                    //+0x8C
    std::int64_t                          mShootingEndTimeNs;             // 0 = inactive; expired stays active until animation cleanup
    ReanimationID           mBodyReanimID;                  //+0x94
    ReanimationID           mHeadReanimID;                  //+0x98
    ReanimationID           mHeadReanimID2;                 //+0x9C
    ReanimationID           mHeadReanimID3;                 //+0xA0
    ReanimationID           mBlinkReanimID;                 //+0xA4
    ReanimationID           mLightReanimID;                 //+0xA8
    ReanimationID           mSleepingReanimID;              //+0xAC
    Sexy::TimeCounter                     mBlinkCountdown;                //+0xB0
    std::int64_t                     mRecentlyEatenEndTimeNs;        //+0xB4
    std::int64_t                     mEatenFlashEndTimeNs;           //+0xB8
    std::int64_t                     mBeghouledFlashEndTimeNs;       //+0xBC
    float                   mShakeOffsetX;                  //+0xC0
    float                   mShakeOffsetY;                  //+0xC4
    MagnetItem              mMagnetItems[MAX_MAGNET_ITEMS]; //+0xC8
    ZombieID                mTargetZombieID;                //+0x12C
    std::int64_t                     mWakeUpEndTimeNs;                 //+0x130
    PlantOnBungeeState      mOnBungeeState;                 //+0x134
    SeedType                mImitaterType;                  //+0x138
    int                     mPottedPlantIndex;              //+0x13C
    bool                    mAnimPing;                      //+0x140
    bool                    mDead;                          //+0x141
    bool                    mSquished;                      //+0x142
    bool                    mIsAsleep;                      //+0x143
    bool                    mIsOnBoard;                     //+0x144
    bool                    mHighlighted;                   //+0x145

public:
    Plant();

    void                    PlantInitialize(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theGridX, int theGridY, SeedType theSeedType, SeedType theImitaterType);
    void                    Update(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, std::uint64_t theLevelDeltaNs);
    void                    Animate(std::int64_t ownerNowNs, std::int64_t animationNowNs);
    void                    Draw(std::int64_t ownerNowNs, Graphics* g);
    void                    MouseDown(int x, int y, int theClickCount);
    void                    DoSpecial(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                    Fire(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie* theTargetZombie, int theRow, PlantWeapon thePlantWeapon = PlantWeapon::WEAPON_PRIMARY);
    [[nodiscard]] Zombie*                 FindTargetZombie(int theRow, PlantWeapon thePlantWeapon = PlantWeapon::WEAPON_PRIMARY);
    void                    Die(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                    UpdateProductionPlant(std::int64_t ownerNowNs);
    void                    UpdateShooter(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    bool                    FindTargetAndFire(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theRow, PlantWeapon thePlantWeapon = PlantWeapon::WEAPON_PRIMARY);
    void                    LaunchThreepeater(std::int64_t ownerNowNs);
    static int              GetCost(SeedType theSeedType, SeedType theImitaterType = SeedType::SEED_NONE);
    static SexyString       GetNameString(SeedType theSeedType, SeedType theImitaterType = SeedType::SEED_NONE);
    static SexyString       GetToolTip(SeedType theSeedType);
    static int              GetRefreshTime(SeedType theSeedType, SeedType theImitaterType = SeedType::SEED_NONE);
    static /*inline*/ bool  IsNocturnal(SeedType theSeedtype);
    static /*inline*/ bool  IsFungus(SeedType theSeedType);
    static /*inline*/ bool  IsAquatic(SeedType theSeedType);
    static /*inline*/ bool  IsFlying(SeedType theSeedtype);
    static /*inline*/ bool  IsUpgrade(SeedType theSeedtype);
    void                    UpdateAbilities(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                    Squish(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                    DoRowAreaDamage(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theDamage, unsigned int theDamageFlags);
    int                     GetDamageRangeFlags(PlantWeapon thePlantWeapon = PlantWeapon::WEAPON_PRIMARY);
    inline Rect                    GetPlantRect()
    {
        Rect aRect;
        if (mSeedType == SeedType::SEED_TALLNUT)
        {
            aRect = Rect(mX + 10, mY, mWidth, mHeight);
        }
        else if (mSeedType == SeedType::SEED_PUMPKINSHELL)
        {
            aRect = Rect(mX, mY, mWidth - 20, mHeight);
        }
        else if (mSeedType == SeedType::SEED_COBCANNON)
        {
            aRect = Rect(mX, mY, 140, 80);
        }
        else
        {
            aRect = Rect(mX + 10, mY, mWidth - 20, mHeight);
        }

        return aRect;
    }
    Rect                    GetPlantAttackRect(PlantWeapon thePlantWeapon = PlantWeapon::WEAPON_PRIMARY);
    [[nodiscard]] Zombie*                 FindSquashTarget();
    void                    UpdateSquash(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    /*inline*/ bool         NotOnGround();
    void                    DoSquashDamage(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                    BurnRow(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theRow);
    void                    IceZombies(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                    BlowAwayFliers();
    void                    UpdateGraveBuster(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    TodParticleSystem*      AddAttachedParticle(std::int64_t ownerNowNs, int thePosX, int thePosY, int theRenderPosition, ParticleEffect theEffect);
    void                    GetPeaHeadOffset(std::int64_t ownerNowNs, int& theOffsetX, int& theOffsetY);
    /*inline*/ bool         MakesSun();
    static void             DrawSeedType(Graphics* g, SeedType theSeedType, SeedType theImitaterType, DrawVariation theDrawVariation, float thePosX, float thePosY);
    void                    KillAllPlantsNearDoom(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    bool                    IsOnHighGround();
    void                    UpdateTorchwood();
    void                    LaunchStarFruit(std::int64_t ownerNowNs);
    bool                    FindStarFruitTarget(std::int64_t ownerNowNs);
    void                    UpdateChomper(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                    DoBlink(std::int64_t ownerNowNs, std::int64_t animationNowNs);
    void                    UpdateBlink(std::int64_t ownerNowNs, std::int64_t animationNowNs);
    void                    PlayBodyReanim(std::int64_t ownerNowNs, ReanimTrackId theTrackId, ReanimLoopType theLoopType, int theBlendTime, float theAnimRate);
    void                    UpdateMagnetShroom(std::int64_t ownerNowNs);
    MagnetItem*             GetFreeMagnetItem();
    void                    DrawMagnetItems(std::int64_t ownerNowNs, Graphics* g);
    void                    UpdateDoomShroom(std::int64_t ownerNowNs);
    void                    UpdateIceShroom(std::int64_t ownerNowNs);
    void                    UpdatePotato(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    int                     CalcRenderOrder();
    void                    AnimateNuts(std::int64_t ownerNowNs);
    void                    SetSleeping(std::int64_t ownerNowNs, std::int64_t animationNowNs, bool theIsAsleep);
    void                    UpdateShooting(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                    DrawShadow(std::int64_t ownerNowNs, Graphics* g, float theOffsetX, float theOffsetY);
    void                    UpdateScaredyShroom(std::int64_t ownerNowNs);
    int                     DistanceToClosestZombie();
    void                    UpdateSpikeweed(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                    MagnetShroomAttactItem(std::int64_t ownerNowNs, Zombie* theZombie);
    void                    UpdateSunShroom(std::int64_t ownerNowNs);
    void                    UpdateBowling(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                    AnimatePumpkin();
    void                    UpdateBlover(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                    UpdateCactus(std::int64_t ownerNowNs);
    void                    StarFruitFire();
    void                    UpdateTanglekelp(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    Reanimation*            AttachBlinkAnim(std::int64_t ownerNowNs, std::int64_t animationNowNs, Reanimation* theReanimBody);
    void                    UpdateReanimColor(std::int64_t ownerNowNs);
    bool                    IsUpgradableTo(SeedType theUpgradedType);
    bool                    IsPartOfUpgradableTo(SeedType theUpgradedType);
    void                    UpdateCobCannon(std::int64_t ownerNowNs);
    void                    CobCannonFire(std::int64_t ownerNowNs, int theTargetX, int theTargetY);
    void                    UpdateGoldMagnetShroom(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                    RemoveEffects();
    void                    UpdateCoffeeBean(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                    UpdateUmbrella(std::int64_t ownerNowNs);
    void                    EndBlink();
    void                    AnimateGarlic();
    [[nodiscard]] Coin*                   FindGoldMagnetTarget(std::int64_t visualNowNs);
    void                    SpikeweedAttack(std::int64_t ownerNowNs);
    void                    ImitaterMorph(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                    UpdateImitater(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                    UpdateReanim(std::int64_t ownerNowNs, std::int64_t animationNowNs);
    void                    SpikeRockTakeDamage(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    bool                    IsSpiky();
    static /*inline*/ void  PreloadPlantResources(SeedType theSeedType);
    /*inline*/ bool         IsInPlay();
    void                    UpdateNeedsFood() { ; }
    void                    PlayIdleAnim(std::int64_t ownerNowNs, float theRate);
    void                    UpdateFlowerPot();
    void                    UpdateLilypad();
    void                    GoldMagnetFindTargets(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    bool                    IsAGoldMagnetAboutToSuck(std::int64_t ownerNowNs);
    bool                    DrawMagnetItemsOnTop();
};

float                       PlantDrawHeightOffset(std::int64_t ownerNowNs, Board* theBoard, Plant* thePlant, SeedType theSeedType, int theCol, int theRow);
float                       PlantFlowerPotHeightOffset(SeedType theSeedType, float theFlowerPotScale);

class PlantDefinition
{
public:
    SeedType                mSeedType;          //+0x0
    ReanimationType         mReanimationType;   //+0x8
    int                     mPacketIndex;       //+0xC
    int                     mSeedCost;          //+0x10
    int                     mRefreshTime;       //+0x14
    PlantSubClass           mSubClass;          //+0x18
    Sexy::TimeCounter                     mLaunchRate;        //+0x1C
};

/*inline*/ PlantDefinition& GetPlantDefinition(SeedType theSeedType);
