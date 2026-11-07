#pragma once
#include "misc/TimeCounter.h"
#include "GameObject.h"
#include "Motion.h"
#include "GameConstants.h"
#include "Sexy.TodLib/ReanimTrackId.h"

#define MAX_ZOMBIE_FOLLOWERS 4
#define NUM_BOBSLED_FOLLOWERS 3
#define NUM_BACKUP_DANCERS 4
#define NUM_BOSS_BUNGEES 3

constexpr const int ZOMBIE_START_RANDOM_OFFSET = 40;
constexpr const int BUNGEE_ZOMBIE_HEIGHT = 3000;
constexpr const int RENDER_GROUP_SHIELD = 1;
constexpr const int RENDER_GROUP_ARMS = 2;
constexpr const int RENDER_GROUP_OVER_SHIELD = 3;
constexpr const int RENDER_GROUP_BOSS_BACK_LEG = 4;
constexpr const int RENDER_GROUP_BOSS_FRONT_LEG = 5;
constexpr const int RENDER_GROUP_BOSS_BACK_ARM = 6;
constexpr const int RENDER_GROUP_BOSS_FIREBALL_ADDITIVE = 7;
constexpr const int RENDER_GROUP_BOSS_FIREBALL_TOP = 8;
constexpr const int ZOMBIE_LIMP_SPEED_FACTOR = 2;
constexpr const int POGO_BOUNCE_TIME = 80;
constexpr const int DOLPHIN_JUMP_TIME = 120;
constexpr const int JackInTheBoxZombieRadius = 115;
constexpr const int JackInTheBoxPlantRadius = 90;
constexpr const int BOBSLED_CRASH_TIME = 150;
constexpr const int ZOMBIE_BACKUP_DANCER_RISE_HEIGHT = -200;
constexpr const int BOSS_FLASH_HEALTH_FRACTION = 10;
constexpr const int TICKS_BETWEEN_EATS = 4;
constexpr const int DAMAGE_PER_EAT = TICKS_BETWEEN_EATS;
constexpr const float THOWN_ZOMBIE_GRAVITY = 0.05f;
constexpr const float CHILLED_SPEED_FACTOR = 0.4f;
constexpr const float CLIP_HEIGHT_LIMIT = -100.0f;
constexpr const float CLIP_HEIGHT_OFF = -200.0f;
const Color ZOMBIE_MINDCONTROLLED_COLOR = Color(128, 64, 192, 255);

enum ZombieAttackType
{
    ATTACKTYPE_CHEW,
    ATTACKTYPE_DRIVE_OVER,
    ATTACKTYPE_VAULT,
    ATTACKTYPE_LADDER
};

enum ZombieParts
{
    PART_BODY,
    PART_HEAD,
    PART_HEAD_EATING,
    PART_TONGUE,
    PART_ARM,
    PART_HAIR,
    PART_HEAD_YUCKY,
    PART_ARM_PICKAXE,
    PART_ARM_POLEVAULT,
    PART_ARM_LEASH,
    PART_ARM_FLAG,
    PART_POGO,
    PART_DIGGER
};

class ZombieDrawPosition
{
public:
    int                             mHeadX;
    int                             mHeadY;
    int                             mArmY;
    float                           mBodyY;
    float                           mImageOffsetX;
    float                           mImageOffsetY;
    float                           mClipHeight;
};


class Plant;
class Zombatar;
class Reanimation;
class TodParticleSystem;
class Zombie : public GameObject
{
public:
    enum
    {
        ZOMBIE_WAVE_DEBUG = -1,
        ZOMBIE_WAVE_CUTSCENE = -2,
        ZOMBIE_WAVE_UI = -3,
        ZOMBIE_WAVE_WINNER = -4
    };

public:
	ZombieType			            mZombieType;                                //+0x24
	ZombiePhase			            mZombiePhase;                               //+0x28
	float				            mVelX;                                      //+0x34
    Sexy::TimeCounter                             mAnimCounter;                               //+0x38
    Sexy::TimeCounter                             mGroanCounter;                              //+0x3C
    Sexy::TimeCounter                             mAnimTicksPerFrame;                         //+0x40
    int                             mAnimFrames;                                //+0x44
    int                             mFrame;                                     //+0x48
    int                             mPrevFrame;                                 //+0x4C
    bool                            mVariant;                                   //+0x50
    bool                            mIsEating;                                  //+0x51
    std::int64_t                             mShotFlashEndTimeNs;                        //+0x54
    std::int64_t                             mShieldFlashEndTimeNs;                  //+0x58
    std::int64_t                             mShieldRecoilEndTimeNs;                       //+0x5C
    std::int64_t                             mBirthTimeNs;                                 //+0x60
    ZombieHeight                    mZombieHeight;                              //+0x64
    Sexy::TimeCounter                             mPhaseCounter;                              //+0x68
    int                             mFromWave;                                  //+0x6C
    bool                            mDroppedLoot;                               //+0x70
    std::int64_t                             mFadeEndTimeNs;                                //+0x74
    bool                            mFlatTires;                                 //+0x78
    int                             mUseLadderCol;                              //+0x7C
    int                             mTargetCol;                                 //+0x80
    float                           mAltitude;                                  //+0x84
    bool                            mHitUmbrella;                               //+0x88
    Rect                            mZombieRect;                                //+0x8C
    Rect                            mZombieAttackRect;                          //+0x9C
    std::int64_t                             mChilledEndTimeNs;                            //+0xAC
    std::int64_t                             mButteredEndTimeNs;                           //+0xB0
    std::int64_t                             mIceTrapEndTimeNs;                            //+0xB4
    bool                            mMindControlled;                            //+0xB8
    bool                            mBlowingAway;                               //+0xB9
    bool                            mHasHead;                                   //+0xBA
    bool                            mHasArm;                                    //+0xBB
    bool                            mHasObject;                                 //+0xBC
    bool                            mInPool;                                    //+0xBD
    bool                            mOnHighGround;                              //+0xBE
    bool                            mYuckyFace;                                 //+0xBF
    std::int64_t                             mYuckyFaceStartTimeNs;                          //+0xC0
    HelmType                        mHelmType;                                  //+0xC4
    int                             mBodyHealth;                                //+0xC8
    int                             mBodyMaxHealth;                             //+0xCC
    int                             mHelmHealth;                                //+0xD0
    int                             mHelmMaxHealth;                             //+0xD4
    ShieldType                      mShieldType;                                //+0xD8
    int                             mShieldHealth;                              //+0xDC
    int                             mShieldMaxHealth;                           //+0xE0
    int                             mFlyingHealth;                              //+0xE4
    int                             mFlyingMaxHealth;                           //+0xE8
    bool                            mDead;                                      //+0xEC
    ZombieID                        mRelatedZombieID;                           //+0xF0
    ZombieID                        mFollowerZombieID[MAX_ZOMBIE_FOLLOWERS];    //+0xF4
    bool                            mPlayingSong;                               //+0x104
    int                             mParticleOffsetX;                           //+0x108
    int                             mParticleOffsetY;                           //+0x10C
    AttachmentID                    mAttachmentID;                              //+0x110
    Sexy::TimeCounter                             mSummonCounter;                             //+0x114; Catapult uses encoded whole units as ammo, not elapsed time.
    ReanimationID                   mBodyReanimID;                              //+0x118
    float                           mScaleZombie;                               //+0x11C
    float                           mVelZ;                                      //+0x120
    float                           mOriginalAnimRate;                          //+0x124
    PlantID                         mTargetPlantID;                             //+0x128
    int                             mBossMode;                                  //+0x12C
    int                             mTargetRow;                                 //+0x130
    Sexy::TimeCounter                             mBossBungeeCounter;                         //+0x134
    Sexy::TimeCounter                             mBossStompCounter;                          //+0x138
    Sexy::TimeCounter                             mBossHeadCounter;                           //+0x13C
    // 每个僵王同时至多持有一个冰球或火球，两者共用此 ID，并非各有一个。
    // BossHeadSpit 先销毁旧球，BossHeadSpitContact 再断言槽位为空后创建新球。
    ReanimationID                   mBossFireBallReanimID;                      //+0x140
    ReanimationID                   mSpecialHeadReanimID;                       //+0x144
    ReanimationID                   mZombatarHeadReanimID;                      //+GOTY @Patoke: 0x148
    int                             mFireballRow;                               //+0x148
    bool                            mIsFireBall;                                //+0x14C
    ReanimationID                   mMoweredReanimID;                           //+0x150
    int                             mLastPortalX;                               //+0x154
    bool                            mHasZombatar;                               //+GOTY @Patoke: 0x15C
    int                             mZombatarIndex;                             //+GOTY @Patoke: 0x160
    // 普通僵尸用于自身根运动；僵王不行走，复用于上述唯一冰球/火球的根运动。
    // 僵王本体被冻结时球仍独立滚动，因此冻结清理不能重置僵王的这组绑定。
    double                          mRootOriginX = 0;
    std::uint32_t                   mRootBindingGeneration = 0;
    float                           mRootScale = 0;
    // Flat-tire Zamboni reuses rate as its initial positive speed and startTimeNs
    // as the absolute level timestamp of that speed; origin is unused there.
    // Other types retain a signed linear rate and an absolute segment origin.
    TimedLinearMotion               mVelocityX;
    TimedLinearMotion               mVelocityY;
    // Shared by mutually exclusive linear altitude and thrown-imp segments.
    // For an imp, rate is its initial vertical velocity; startTimeNs remains ns.
    TimedLinearMotion               mAltitudeMotion;
    float                           mPendingAdditionalSpeedX = 0; // consumed within the current Update

public:
    double RootPosition(const Reanimation& animation, float signedScale);
    // Pool value-initialization must clear reused state before animation setup.
    Zombie() = default;
    ~Zombie();

    void                            ZombieInitialize(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theRow, ZombieType theType, bool theVariant, Zombie* theParentZombie, int theFromWave);
    void                            Animate(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            CheckIfPreyCaught(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            EatZombie(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Zombie* theZombie);
    void                            EatPlant(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, Plant* thePlant);
    void                            Update(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, std::uint64_t ownerDeltaNs);
    void                            DieNoLoot(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    /*inline*/ void                 DieWithLoot(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            Draw(std::int64_t ownerNowNs, Graphics* g);
    void                            DrawBungeeCord(Graphics* g, int theOffsetX);
    void                            TakeDamage(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theDamage, unsigned int theDamageFlags);
    /*inline*/ void                 SetRow(int theRow);
    float                           GetPosYBasedOnRow(int theRow);
    void                            ApplyChill(std::int64_t ownerNowNs, bool theIsIceTrap);
    void                            UpdateZombieBungee(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            BungeeLanding(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    bool                            EffectedByDamage(unsigned int theDamageRangeFlags);
    void                            PickRandomSpeed(std::int64_t ownerNowNs);
    void                            UpdateZombiePolevaulter(std::int64_t ownerNowNs);
    void                            UpdateZombieDolphinRider(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            PickBungeeZombieTarget(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theColumn);
    int                             CountBungeesTargetingSunFlowers();
    [[nodiscard]] Plant*                          FindPlantTarget(ZombieAttackType theAttackType);
    void                            CheckSquish(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, ZombieAttackType theAttackType);
    void                            RiseFromGrave(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theCol, int theRow);
    void                            UpdateZombieRiseFromGrave();
    void                            UpdateDamageStates(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, unsigned int theDamageFlags);
    void                            UpdateZombiePool(std::int64_t ownerNowNs);
    void                            CheckForPool(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            GetDrawPos(ZombieDrawPosition& theDrawPos);
    void                            UpdateZombieHighGround(std::int64_t ownerNowNs);
    void                            CheckForHighGround();
    bool                            IsOnHighGround();
    void                            DropLoot();
    bool                            TrySpawnLevelAward();
    /*inline*/ void                 StartZombieSound();
    void                            StopZombieSound();
    void                            UpdateZombieJackInTheBox(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            UpdateZombiePosition(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    Rect                            GetZombieRect();
    Rect                            GetZombieAttackRect();
    void                            UpdateZombieWalking(std::int64_t ownerNowNs);
    void                            UpdateZombieBobsled(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            BobsledCrash(std::int64_t ownerNowNs);
    Plant*                          IsStandingOnSpikeweed();
    void                            CheckForZombieStep(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            CountExpectedMowers() { ; }
    /*inline*/ void                 OverrideParticleColor(std::int64_t ownerNowNs, TodParticleSystem* aParticle);
    /*inline*/ void                 OverrideParticleScale(TodParticleSystem* aParticle);
    void                            PoolSplash(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, bool theInToPoolSound);
    void                            UpdateZombieFlyer(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, bool theAltitudeAlreadyAdvanced = false);
    void                            UpdateZombiePogo(std::int64_t ownerNowNs);
    void                            UpdateZombieNewspaper(std::int64_t ownerNowNs);
    void                            LandFlyer(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, unsigned int theDamageFlags);
    void                            UpdateZombieDigger(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    bool                            IsWalkingBackwards();
    TodParticleSystem*              AddAttachedParticle(std::int64_t ownerNowNs, int thePosX, int thePosY, ParticleEffect theEffect);
    void                            PogoBreak(std::int64_t ownerNowNs, unsigned int theDamageFlags);
    void                            UpdateZombieFalling(std::int64_t ownerNowNs, float theAdditionalRate = 0);
    void                            UpdateZombieDancer(std::int64_t ownerNowNs);
    ZombieID                        SummonBackupDancer(std::int64_t ownerNowNs, int theRow, int thePosX);
    void                            SummonBackupDancers(std::int64_t ownerNowNs);
    int                             GetDancerFrame(std::int64_t ownerNowNs);
    void                            BungeeStealTarget(std::int64_t ownerNowNs);
    void                            BungeeLiftTarget(std::int64_t ownerNowNs);
    void                            UpdateYuckyFace(std::int64_t ownerNowNs);
    void                            DrawIceTrap(Graphics* g, const ZombieDrawPosition& theDrawPos, bool theFront);
    void                            HitIceTrap(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    inline int                             GetHelmDamageIndex()
    {
        if (mHelmHealth < mHelmMaxHealth / 3)
        {
            return 2;
        }

        if (mHelmHealth < mHelmMaxHealth * 2 / 3)
        {
            return 1;
        }

        return 0;
    }
    inline int                             GetShieldDamageIndex()
    {
        if (mShieldHealth < mShieldMaxHealth / 3)
        {
            return 2;
        }

        if (mShieldHealth < mShieldMaxHealth * 2 / 3)
        {
            return 1;
        }

        return 0;
    }
    void                            DrawReanim(std::int64_t ownerNowNs, Graphics* g, const ZombieDrawPosition& theDrawPos, int theBaseRenderGroup);
    void                            UpdatePlaying(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    bool                            NeedsMoreBackupDancers();
    void                            ConvertToNormalZombie(std::int64_t ownerNowNs);
    void                            UpdateDancerWalking() { ; }
    void                            StartEating(std::int64_t ownerNowNs);
    void                            StopEating(std::int64_t ownerNowNs);
    void                            UpdateAnimSpeed(std::int64_t ownerNowNs);
    /*inline*/ void                 ReanimShowRange(const ReanimTrackIdRange& theTrackRange, int theRenderGroup);
    void                            PlayDeathAnim(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, unsigned int theDamageFlags);
    void                            UpdateDeath(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            DrawShadow(Graphics* g);
    bool                            HasShadow();
    Reanimation*                    LoadReanim(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, ReanimationType theReanimationType);
    /*inline*/ int                  TakeFlyingDamage(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theDamage, unsigned int theDamageFlags);
    int                             TakeShieldDamage(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theDamage, unsigned int theDamageFlags);
    int                             TakeHelmDamage(std::int64_t ownerNowNs, int theDamage, unsigned int theDamageFlags);
    void                            TakeBodyDamage(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theDamage, unsigned int theDamageFlags);
    void                            AttachShield();
    void                            DetachShield(std::int64_t ownerNowNs);
    void                            UpdateReanim(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            GetTrackPosition(ReanimTrackId theTrackId, float& thePosX, float& thePosY);
    void                            LoadPlainZombieReanim();
    void                            ShowDoorArms(bool theShow);
    /*inline*/ void                 ReanimShowTrack(ReanimTrackId theTrackId, int theRenderGroup);
    /*inline*/ void                 PlayZombieAppearSound();
    void                            StartMindControlled();
    bool                            IsFlying();
    void                            DropHead(std::int64_t ownerNowNs, unsigned int theDamageFlags);
    bool                            CanTargetPlant(Plant* thePlant, ZombieAttackType theAttackType);
    void                            UpdateZombieCatapult(std::int64_t ownerNowNs);
    [[nodiscard]] Plant*                          FindCatapultTarget();
    void                            ZombieCatapultFire(Plant* thePlant);
    void                            UpdateClimbingLadder(std::int64_t ownerNowNs);
    void                            UpdateZombieGargantuar(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    int                             GetBodyDamageIndex();
    void                            ApplyBurn(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            UpdateBurn(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    bool                            ZombieNotWalking(std::int64_t ownerNowNs);
    [[nodiscard]] Zombie*                         FindZombieTarget();
    /*inline*/ void                 PlayZombieReanim(std::int64_t ownerNowNs, ReanimTrackId theTrackId, ReanimLoopType theLoopType, int theBlendTime, float theAnimRate);
    void                            UpdateZombieBackupDancer(std::int64_t ownerNowNs);
    ZombiePhase                     GetDancerPhase(std::int64_t ownerNowNs);
    bool                            IsMovingAtChilledSpeed(std::int64_t ownerNowNs);
    void                            StartWalkAnim(std::int64_t ownerNowNs, int theBlendTime);
    Reanimation*                    AddAttachedReanim(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int thePosX, int thePosY, ReanimationType theReanimType);
    void                            DragUnder(std::int64_t ownerNowNs);
    static /*inline*/ void          SetupDoorArms(Reanimation* aReanim, bool theShow);
    static void                     SetupReanimLayers(Reanimation* aReanim, ZombieType theZombieType);
    /*inline*/ bool                 IsOnBoard();
    void                            DrawButter(Graphics* g, const ZombieDrawPosition& theDrawPos);
    bool                            IsImmobilizied(std::int64_t ownerNowNs);
    void                            ApplyButter(std::int64_t ownerNowNs);
    float                           ZombieTargetLeadX(std::int64_t ownerNowNs, float theTime);
    void                            UpdateZombieImp(std::int64_t ownerNowNs);
    void                            SquishAllInSquare(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theX, int theY, ZombieAttackType theAttackType);
    void                            RemoveIceTrap(std::int64_t ownerNowNs);
    bool                            IsBouncingPogo();
    int                             GetBobsledPosition();
    void                            DrawBobsledReanim(std::int64_t ownerNowNs, Graphics* g, const ZombieDrawPosition& theDrawPos, bool theBeforeZombie);
    void                            BobsledDie(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            BobsledBurn(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    bool                            IsBobsledTeamWithSled();
    bool                            CanBeFrozen();
    bool                            CanBeChilled();
    void                            UpdateZombieSnorkel(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            ReanimIgnoreClipRect(ReanimTrackId theTrackId, bool theIgnoreClipRect);
    void                            SetAnimRate(std::int64_t ownerNowNs, float theAnimRate);
    void                            ApplyAnimRate(std::int64_t ownerNowNs, float theAnimRate);
    /*inline*/ bool                 IsDeadOrDying();
    void                            DrawDancerReanim(std::int64_t ownerNowNs, Graphics* g);
    void                            DrawBungeeReanim(std::int64_t ownerNowNs, Graphics* g);
    void                            DrawBungeeTarget(Graphics* g);
    void                            BungeeDie(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            ZamboniDeath(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, unsigned int theDamageFlags);
    void                            CatapultDeath(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, unsigned int theDamageFlags);
    bool                            SetupDrawZombieWon(Graphics* g);
    void                            WalkIntoHouse(std::int64_t ownerNowNs);
    void                            UpdateZamboni(std::int64_t ownerNowNs);
    void                            UpdateZombieChimney();
    void                            UpdateLadder(std::int64_t ownerNowNs);
    void                            DropArm(std::int64_t ownerNowNs, unsigned int theDamageFlags);
    bool                            CanLoseBodyParts();
    void                            DropHelm(std::int64_t ownerNowNs, unsigned int theDamageFlags);
    void                            DropShield(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, unsigned int theDamageFlags);
    void                            ReanimReenableClipping();
    void                            UpdateBoss(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            BossPlayIdle(std::int64_t ownerNowNs);
    void                            BossRVLanding(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            BossStompContact(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    bool                            BossAreBungeesDone();
    void                            BossBungeeSpawn(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            BossSpawnAttack(std::int64_t ownerNowNs);
    void                            BossBungeeAttack(std::int64_t ownerNowNs);
    void                            BossRVAttack(std::int64_t ownerNowNs);
    void                            BossSpawnContact(std::int64_t ownerNowNs);
    void                            BossBungeeLeave(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            BossStompAttack(std::int64_t ownerNowNs);
    bool                            BossCanStompRow(int theRow);
    void                            BossDie(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            BossHeadAttack(std::int64_t ownerNowNs);
    void                            BossHeadSpitContact(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            BossHeadSpit(std::int64_t ownerNowNs);
    void                            UpdateBossFireball(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            BossDestroyFireball(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            BossDestroyIceballInRow(std::int64_t ownerNowNs);
    void                            DiggerLoseAxe(std::int64_t ownerNowNs);
    void                            BungeeDropZombie(std::int64_t ownerNowNs, Zombie* theDroppedZombie, int theGridX, int theGridY);
    void                            ShowYuckyFace(bool theShow);
    void                            AnimateChewSound(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            AnimateChewEffect(std::int64_t ownerNowNs);
    void                            UpdateActions(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            CheckForBoardEdge(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            UpdateYeti(std::int64_t ownerNowNs);
    void                            DrawBossPart(std::int64_t ownerNowNs, Graphics* g, BossPart theBossPart);
    void                            BossSetupReanim(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            MowDown(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            UpdateMowered(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            DropFlag(std::int64_t ownerNowNs);
    void                            DropPole();
    void                            DrawBossBackArm(std::int64_t ownerNowNs, Graphics* g, const ZombieDrawPosition& theDrawPos);
    static void                     PreloadZombieResources(ZombieType theZombieType);
    void                            BossStartDeath(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            RemoveColdEffects(std::int64_t ownerNowNs);
    void                            BossHeadSpitEffect(std::int64_t ownerNowNs);
    void                            DrawBossFireBall(Graphics* g);
    void                            UpdateZombiePeaHead();
    void                            UpdateZombieJalapenoHead(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            ApplyBossSmokeParticles(std::int64_t ownerNowNs, bool theEnable);
    void                            UpdateZombiquarium(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    bool                            ZombiquariumFindClosestBrain(std::int64_t ownerNowNs);
    void                            UpdateZombieGatlingHead();
    void                            UpdateZombieSquashHead(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    bool                            IsTanglekelpTarget();
    bool                            HasYuckyFaceImage();
    bool                            IsTangleKelpTarget();
    bool                            IsFireResistant();
    /*inline*/ void                 EnableMustache(bool theEnableMustache);
    /*inline*/ void                 EnableFuture(bool theEnableFuture);
    /*inline*/ void                 EnableDance(std::int64_t ownerNowNs);
    void                            BungeeDropPlant(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs);
    void                            RemoveButter(std::int64_t ownerNowNs);
    void                            BalloonPropellerHatSpin(bool theSpinning);
    void                            DoDaisies(std::int64_t ownerNowNs);
    static /*inline*/ bool          ZombieTypeCanGoOnHighGround(ZombieType theZombieType);
    static /*inline*/ bool          ZombieTypeCanGoInPool(ZombieType theZombieType);
    void                            SetupWaterTrack(ReanimTrackId theTrackId);
    void                            BurnRow(std::int64_t ownerNowNs, std::int64_t visualNowNs, std::int64_t animationNowNs, int theRow);
    void                            SetupReanimForLostHead();
    void                            SetupReanimForLostArm(std::int64_t ownerNowNs, unsigned int theDamageFlags);
    bool                            IsSquashTarget(Plant* theExcept);
    static /*inline*/ bool			IsZombotany(ZombieType theZombieType);
    void                            ResetZombatarHead(LawnApp* theApp);
    void                            ApplyZombatarHead(LawnApp* theApp, Zombatar* theZombatar);
};

class ZombieDefinition
{
public:
    ZombieType                      mZombieType;
    ReanimationType                 mReanimationType;
    int                             mZombieValue;
    int                             mStartingLevel;
    int                             mFirstAllowedWave;
    int                             mPickWeight;
};

/*inline*/ ZombieDefinition&            GetZombieDefinition(ZombieType theZombieType);

