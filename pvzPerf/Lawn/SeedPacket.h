#pragma once
#include "misc/TimeCounter.h"
#include "GameObject.h"
#include "GameConstants.h"
#include "SlotMachineMotion.h"

constexpr const int CONVEYOR_SPEED = 4;

class HitResult;
class SeedPacket : public GameObject
{
public:
    std::int64_t                      mRefreshStartTimeNs;                            //+0x24
    Sexy::TimeCounter                 mRefreshTime;                               //+0x28
    int                 mIndex;                                     //+0x2C
    int                 mOffsetX;                                   //+0x30
    SeedType            mPacketType;                                //+0x34
    SeedType            mImitaterType;                              //+0x38
    std::int64_t                      mSlotMachineEndTimeNs;                      //+0x3C
    float               mSlotMachiningPosition;
    SeedType            mSlotMachiningNextSeed;                     //+0x40
    bool                mActive;                                    //+0x48
    bool                mRefreshing;                                //+0x49
    int                 mTimesUsed;                                 //+0x4C

public:
    SeedPacket();

    void                Update();
    void                Draw(Graphics* g);
    void                MouseDown(int x, int y, int theClickCount);
    bool                MouseHitTest(int theX, int theY, HitResult* theHitResult);
    void                Activate();
    /*inline*/ void     SetActivate(bool theActivate);
    void                PickNextSlotMachineSeed();
    void                WasPlanted();
    /*inline*/ void     SlotMachineStart();
    void                FlashIfReady();
    bool                CanPickUp();
    void                SetPacketType(SeedType theSeedType, SeedType theImitaterType = SeedType::SEED_NONE);
};

class SeedBank : public GameObject
{
public:
    int				    mNumPackets;                    //+0x24
    SeedPacket		    mSeedPackets[SEEDBANK_MAX];     //+0x28
    int				    mCutSceneDarken;                //+0x348
    Sexy::TimeCounter				    mConveyorBeltCounter;           //+0x34C

public:
    SeedBank();
    void DrawSun(Graphics* g);

    void			    Draw(Graphics* g);
    bool			    MouseHitTest(int x, int y, HitResult* theHitResult);
    bool			    ContainsPoint(int theX, int theY);
    void			    AddSeed(SeedType theSeedType, bool thePlaceOnLeft = false);
    void			    RemoveSeed(int theIndex);
    int				    GetNumSeedsOnConveyorBelt();
    /*inline*/ int		CountOfTypeOnConveyorBelt(SeedType theSeedType);
    void			    UpdateConveyorBelt();
    void			    UpdateWidth();
    void			    RefreshAllPackets();
};

void				    DrawSeedPacket(Graphics* g, float x, float y, SeedType theSeedType, SeedType theImitaterType, float thePercentDark, int theGrayness, bool theDrawCost, bool theUseCurrentCost);

