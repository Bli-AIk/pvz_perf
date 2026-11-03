#include "ReanimClock.h"
#include "Music.h"
#include "SaveGame.h"
#include "LevelSaveFormat.h"
#include "Lawn/Board.h"
#include "Lawn/BoardTouch.h"
#include "Lawn/Challenge.h"
#include "Lawn/SeedPacket.h"
#include "LawnApp.h"
#include "Lawn/CursorObject.h"
#include "widget/WidgetManager.h"
#include "Resources.h"
#include "ConstEnums.h"
#include "Lawn/MessageWidget.h"
#include "Sexy.TodLib/Attachment.h"
#include "Sexy.TodLib/Reanimator.h"
#include "Sexy.TodLib/TodParticle.h"
#include "Sexy.TodLib/TodAllocator.h"
#include "Sexy.TodLib/TodCommon.h"
#include "Sexy.TodLib/EffectSystem.h"
#include "TimeCounterFields.h"
#include "NativeSaveFields.h"
#include "Dat64TimeSchema.h"
#include <span>

#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>
#include "SaveRecordMapping.h"

using namespace SaveRecordMapping;

namespace
{
using LegacyByte = unsigned char;
}

//0x4813D0
void SaveGameContext::SyncBytes(void* theDest, int theReadSize)
{
	int aReadSize = theReadSize;
	if (mReading)
	{
		if ((unsigned long)ByteLeftToRead() < 4)
		{
			mFailed = true;
		}

		aReadSize = mFailed ? 0 : mBuffer.ReadLong();
	}
	else
	{
		mBuffer.WriteLong(theReadSize);
	}

	if (mReading)
	{
		if (aReadSize != theReadSize || ByteLeftToRead() < theReadSize)
		{
			mFailed = true;
		}

		if (mFailed)
		{
			memset(theDest, 0, theReadSize);
		}
		else
		{
			mBuffer.ReadBytes((uchar*)theDest, theReadSize);
		}
	}
	else
	{
		mBuffer.WriteBytes((uchar*)theDest, theReadSize);
	}
}

//0x481470
void SaveGameContext::SyncInt(int& theInt)
{
	if (mReading)
	{
		if ((unsigned long)ByteLeftToRead() < 4)
		{
			mFailed = true;
		}

		theInt = mFailed ? 0 : mBuffer.ReadLong();
	}
	else
	{
		mBuffer.WriteLong(theInt);
	}
}

namespace
{
template <typename T>
void SyncDat64Object(SaveGameContext& theContext, T& theObject)
{
	std::vector<LegacyByte> aDat64Bytes(Dat64Layout<T>::Size(), 0);
	if (!theContext.mReading)
	{
		Dat64Layout<T>::Write(aDat64Bytes.data(), theObject);
	}

	theContext.SyncBytes(
		aDat64Bytes.data(),
		static_cast<int>(aDat64Bytes.size()));
	if (theContext.mReading && !theContext.mFailed)
	{
		Dat64Layout<T>::Read(theObject, aDat64Bytes.data());
	}
}

// Validate the ID/free-list graph before publishing any loaded pool state.
// A free ID is the next slot index, ending at used-count; active IDs embed
// their slot index in the low 16 bits. No object or pointer is read here.
bool ValidateDataArraySlots(const LegacyByte* theBlock, size_t theStride,
    size_t theIDOffset, unsigned int theUsed, unsigned int theSize,
    unsigned int theFreeHead)
{
    const auto ReadID = [&](unsigned int theIndex)
    {
        unsigned int anID = 0;
        std::memcpy(&anID, theBlock + theIndex * theStride + theIDOffset, sizeof(anID));
        return anID;
    };
    unsigned int anActiveCount = 0;
    for (unsigned int i = 0; i < theUsed; ++i)
    {
        const unsigned int anID = ReadID(i);
        if (anID & DATA_ARRAY_KEY_MASK)
        {
            if ((anID & DATA_ARRAY_INDEX_MASK) != i)
                return false;
            ++anActiveCount;
        }
        else if (anID > theUsed)
            return false;
    }
    if (anActiveCount != theSize)
        return false;

    unsigned int aNext = theFreeHead;
    for (unsigned int i = 0; i < theUsed - theSize; ++i)
    {
        if (aNext >= theUsed)
            return false;
        aNext = ReadID(aNext);
        if (aNext & DATA_ARRAY_KEY_MASK)
            return false;
    }
    // An early end, cycle or unreachable free slot cannot satisfy both the
    // exact number of free links above and this terminal sentinel.
    return aNext == theUsed;
}

template <typename T>
void SyncDat64DataArray(
	SaveGameContext& theContext,
	DataArray<T>& theDataArray)
{
	if (theContext.mFailed)
		return;
	unsigned int aFreeListHead = theDataArray.mFreeListHead;
	unsigned int aMaxUsedCount = theDataArray.mMaxUsedCount;
	unsigned int aSize = theDataArray.mSize;
	theContext.SyncUint(aFreeListHead);
	theContext.SyncUint(aMaxUsedCount);
	theContext.SyncUint(aSize);

	const size_t aDat64ItemSize = Dat64Layout<T>::SlotSize();  // 文件步长（非原生步长）
	if (theContext.mFailed || aMaxUsedCount > theDataArray.mMaxSize ||
		aSize > aMaxUsedCount || aFreeListHead > aMaxUsedCount ||
		aMaxUsedCount > static_cast<size_t>(INT_MAX) / aDat64ItemSize)
	{
		theContext.mFailed = true;
		return;
	}

	const size_t aDat64BlockSize =
		static_cast<size_t>(aMaxUsedCount) * aDat64ItemSize;
	std::vector<LegacyByte> aDat64Block(aDat64BlockSize, 0);
	LegacyByte anEmptyBlockByte = 0;
	if (!theContext.mReading)
	{
		for (unsigned int i = 0; i < theDataArray.mMaxUsedCount; ++i)
		{
			auto& aNativeItem = theDataArray.mBlock[i];
			LegacyByte* aDat64Item = aDat64Block.data() + i * aDat64ItemSize;
			if (aNativeItem.mGeneration != 0)
			{
				Dat64Layout<T>::Write(aDat64Item, aNativeItem.mItem);
			}
			const unsigned int anID = aNativeItem.PackedID();
			std::memcpy(
				aDat64Item + Dat64Layout<T>::Size(),
				&anID,
				sizeof(anID));
		}
	}

	theContext.SyncBytes(
		aDat64Block.empty() ? &anEmptyBlockByte : aDat64Block.data(),
		static_cast<int>(aDat64Block.size()));
	if (!theContext.mReading || theContext.mFailed)
	{
		return;
	}

	if (!ValidateDataArraySlots(aDat64Block.data(), aDat64ItemSize,
		Dat64Layout<T>::Size(), aMaxUsedCount, aSize, aFreeListHead))
	{
		theContext.mFailed = true;
		return;
	}
	theDataArray.DataArrayFreeAll();
	theDataArray.mFreeListHead = aFreeListHead;
	theDataArray.mMaxUsedCount = aMaxUsedCount;
	theDataArray.mSize = aSize;
	try
	{
		for (unsigned int i = 0; i < aMaxUsedCount; ++i)
		{
			const LegacyByte* aDat64Item =
				aDat64Block.data() + i * aDat64ItemSize;
			unsigned int anID = 0U;
			std::memcpy(
				&anID,
				aDat64Item + Dat64Layout<T>::Size(),
				sizeof(anID));
			T* aNativeItem = theDataArray.DataArrayPrepareItemForLoad(i, anID);
			if (aNativeItem != nullptr)
			{
				if constexpr (std::is_same_v<T, Reanimation>)
					Dat64Layout<T>::Read(*aNativeItem, aDat64Item,
						gEffectSystem ? gEffectSystem->mReanimationHolder.get() : nullptr);
				else
					Dat64Layout<T>::Read(*aNativeItem, aDat64Item);
			}
		}
	}
	catch (...)
	{
		theDataArray.DataArrayRepairAfterFailedLoad();
		throw;
	}
}

void SyncDat64BoardState(
	SaveGameContext& theContext,
	Board& theBoard)
{
	std::vector<LegacyByte> aDat64Bytes(Dat64Layout<Board>::Size(), 0);
	if (!theContext.mReading)
	{
		Dat64Layout<Board>::Write(aDat64Bytes.data(), theBoard);
	}

	theContext.SyncBytes(
		aDat64Bytes.data(),
		static_cast<int>(aDat64Bytes.size()));
	if (theContext.mReading && !theContext.mFailed)
	{
		Dat64Layout<Board>::Read(theBoard, aDat64Bytes.data());
	}
}

void SyncDat64TrackInstances(
	SaveGameContext& theContext,
	ReanimatorTrackInstance* theTrackInstances,
	int theTrackCount)
{
	if (theTrackCount < 0 ||
		static_cast<size_t>(theTrackCount) >
			static_cast<size_t>(INT_MAX) / Dat64Layout<ReanimatorTrackInstance>::Size())
	{
		theContext.mFailed = true;
		return;
	}

	const size_t aDat64Size = static_cast<size_t>(theTrackCount) *
		Dat64Layout<ReanimatorTrackInstance>::Size();
	std::vector<LegacyByte> aDat64Bytes(aDat64Size, 0);
	if (!theContext.mReading)
	{
		for (int i = 0; i < theTrackCount; ++i)
		{
			Dat64Layout<ReanimatorTrackInstance>::Write(
				aDat64Bytes.data() +
					i * Dat64Layout<ReanimatorTrackInstance>::Size(),
				theTrackInstances[i]);
		}
	}

	theContext.SyncBytes(
		aDat64Bytes.data(),
		static_cast<int>(aDat64Bytes.size()));
	if (!theContext.mReading || theContext.mFailed)
	{
		return;
	}

	for (int i = 0; i < theTrackCount; ++i)
	{
		Dat64Layout<ReanimatorTrackInstance>::Read(
			theTrackInstances[i],
			aDat64Bytes.data() +
				i * Dat64Layout<ReanimatorTrackInstance>::Size());
	}
}
}

//0x4814C0
void SaveGameContext::SyncReanimationDef(ReanimatorDefinition*& theDefinition)
{
	if (mReading)
	{
		int aReanimType;
		SyncInt(aReanimType);
		// SyncInt returns zero on failure. Do not publish definition zero
		// without tracks: a partially loaded Reanimation must remain destructible.
		if (mFailed)
			return;
		if (aReanimType == (int)ReanimationType::REANIM_NONE)
		{
			theDefinition = nullptr;
		}
		else if (aReanimType >= 0 && aReanimType < (int)ReanimationType::NUM_REANIMS)
		{
			ReanimatorEnsureDefinitionLoaded((ReanimationType)aReanimType, true);
			theDefinition = &gReanimatorDefArray[aReanimType];
		}
		else
		{
			mFailed = true;
		}
	}
	else
	{
		int aReanimType = (int)ReanimationType::REANIM_NONE;
		for (int i = 0; i < (int)ReanimationType::NUM_REANIMS; i++)
		{
			ReanimatorDefinition* aDef = &gReanimatorDefArray[i];
			if (theDefinition == aDef)
			{
				aReanimType = i;
				break;
			}
		}
		SyncInt(aReanimType);
	}
}

//0x481710
// 【存档层 TodList 三件套已删（2026-10-02）】SwapDataIDLists/SyncDataIDList/
// ValidateOwnedDataIDs 自 P3 名册化起零调用（名册块内联于 SyncParticleSystem/
// SyncParticleEmitter，磁盘格式 count+N×4B 逐字节不变）——随 TodList 模板
// 退役一并删除，防回退参考不再保留。

//0x4817C0
static void SyncParticleEmitter(TodParticleSystem* theParticleSystem, TodParticleEmitter* theParticleEmitter, SaveGameContext& theContext)
{
	if (theContext.mFailed)
		return;
	if (!theParticleSystem || !theParticleEmitter ||
		!theParticleSystem->mParticleDef || !theParticleSystem->mParticleHolder)
	{
		theContext.mFailed = true;
		return;
	}
	int aEmitterDefIndex = 0;
	if (theContext.mReading)
	{
		theContext.SyncInt(aEmitterDefIndex);
		if (theContext.mFailed || aEmitterDefIndex < 0 ||
			aEmitterDefIndex >= theParticleSystem->mParticleDef->mEmitterDefCount)
		{
			theContext.mFailed = true;
			return;
		}
	}
	else
	{
		aEmitterDefIndex = ((intptr_t)theParticleEmitter->mEmitterDef - (intptr_t)theParticleSystem->mParticleDef->mEmitterDefs) / sizeof(TodEmitterDefinition);
		theContext.SyncInt(aEmitterDefIndex);
	}

	auto& aHolder = *theParticleSystem->mParticleHolder;
	// 粒子名册（2026-10-04 P3 第二步连续化）：磁盘格式不变——count(int)+
	// N×4B ID 序列逐字节同旧 SyncDataIDList（头→尾序=新→旧）；读写走侧表
	//（尾存=旧→新，边界处倒序折算）；装载先攒局部数组，全部成功后一次
	// 性清册+入册（失败时池自持已建粒子，原册不动——与旧临时链表语义同）
	constexpr int kMaxParticleRoster = 1024;
	ParticleID aRosterScratch[kMaxParticleRoster];
	int aRosterCount = 0;
	if (theContext.mReading)
	{
		int aCountOnDisk = 0;
		theContext.SyncInt(aCountOnDisk);
		if (theContext.mFailed || aCountOnDisk < 0 || aCountOnDisk > kMaxParticleRoster ||
			static_cast<unsigned long>(theContext.ByteLeftToRead()) <
				static_cast<unsigned long>(aCountOnDisk) * 8UL)
		{
			theContext.mFailed = true;
			return;
		}
		// 归属校验对齐原 ValidateOwnedDataIDs 语义：非空且不同才拒
		//（裸恢复粒子 mParticleEmitter 为空，由本循环回填）+同池去重
		static bool aSeenParticles[1024];
		std::memset(aSeenParticles, 0, sizeof(aSeenParticles));
		for (int i = 0; i < aCountOnDisk; ++i)
		{
			unsigned int aDataID = 0;
			theContext.SyncBytes(&aDataID, sizeof(aDataID));
			if (theContext.mFailed)
				return;
			TodParticle* aParticle = aHolder.mParticles.DataArrayTryToGet(aDataID);
			const unsigned int anIndex = aDataID & 0xFFFF;
			if (aParticle == nullptr || anIndex >= 1024 || aSeenParticles[anIndex] ||
				(aParticle->mParticleEmitter != nullptr && aParticle->mParticleEmitter != theParticleEmitter))
			{
				theContext.mFailed = true;
				return;
			}
			aSeenParticles[anIndex] = true;
			aParticle->mBirthTimeNs = ReanimClock::Add(theParticleSystem->mLastUpdateTimeNs, aParticle->mBirthTimeNs);
			aParticle->mAnimationBirthTimeNs = ReanimClock::Add(theParticleSystem->mLastUpdateTimeNs, aParticle->mAnimationBirthTimeNs);
			aParticle->mParticleEmitter = theParticleEmitter;  // 回指回填（=原循环同位）
			aRosterScratch[i] = static_cast<ParticleID>(aDataID);
		}
		aRosterCount = aCountOnDisk;
	}
	else
	{
		// 写路径：count 恰一次；头→尾序=新→旧=侧表倒序读
		const int aCount = theParticleEmitter->ParticleRosterCount();
		int aCountOnDisk = aCount;
		theContext.SyncInt(aCountOnDisk);
		for (int i = 0; i < aCount; ++i)
		{
			unsigned int aDataID = static_cast<unsigned int>(theParticleEmitter->ParticleRosterIDAt(aCount - 1 - i));
			theContext.SyncBytes(&aDataID, sizeof(aDataID));
			if (theContext.mFailed)
				return;
		}
	}
	if (theContext.mReading)
	{
		theParticleEmitter->mBirthTimeNs = ReanimClock::Add(theParticleSystem->mLastUpdateTimeNs, theParticleEmitter->mBirthTimeNs);
		if (theParticleEmitter->mCrossFadeEndTimeNs != 0)
			theParticleEmitter->mCrossFadeEndTimeNs = ReanimClock::Add(theParticleSystem->mLastUpdateTimeNs, theParticleEmitter->mCrossFadeEndTimeNs);
		theParticleEmitter->mParticleSystem = theParticleSystem;
		theParticleEmitter->mEmitterDef = &theParticleSystem->mParticleDef->mEmitterDefs[aEmitterDefIndex];
        RefreshParticleLifeFraction(*theParticleEmitter, theParticleSystem->mLastUpdateTimeNs);
		// 入册：盘上头→尾（新→旧）→侧表尾存（旧→新）= 倒序填
		theParticleEmitter->ParticleRosterClear();
		for (int i = aRosterCount - 1; i >= 0; --i)
		{
			TodParticle* aParticle = aHolder.mParticles.DataArrayTryToGet((unsigned int)aRosterScratch[i]);
            // Restore ownership before rebuilding this pool-indexed cache.
            RefreshParticleLifeFraction(*aParticle, theParticleSystem->mLastUpdateTimeNs);
			theParticleEmitter->ParticleRosterAdd(aParticle);
		}
	}
}

//0x481880
void SyncParticleSystem(Board* theBoard, TodParticleSystem* theParticleSystem, SaveGameContext& theContext)
{
	if (theContext.mFailed)
		return;
	// Current v3.2 stores the original base effect and its internal appearance
	// index in the fixed pool record, with no supplemental identity.
	const int aEffectType = static_cast<int>(theParticleSystem->mEffectType);
	if (!gParticleDefArray || aEffectType < 0 ||
		aEffectType >= static_cast<int>(ParticleEffect::NUM_PARTICLES) ||
		aEffectType >= gParticleDefCount)
	{
		theContext.mFailed = true;
		return;
	}
	TodParticleDefinition* aDefinition = &gParticleDefArray[aEffectType];
	if (theParticleSystem->mVariant >= aDefinition->mVariantCount)
	{
		theContext.mFailed = true;
		return;
	}
	if (!theContext.mReading && theParticleSystem->mParticleDef != aDefinition)
	{
		theContext.mFailed = true;
		return;
	}
	auto* aHolder = theContext.mReading ?
		theBoard->mApp->mEffectSystem->mParticleHolder.get() : theParticleSystem->mParticleHolder;
	// 发射器名册（2026-10-04 P3 连续化）：磁盘格式不变——count(int)+N×4B ID
	// 序列逐字节同旧 SyncDataIDList；读写走侧表（裸槽位），世代 ID 仅在
	// 磁盘边界重建（DataArrayGetID/TryToGet）。装载先攒局部数组，全部发射器
	// 同步成功后一次性清册+入册（失败时池自持已建发射器，原册不动——与旧
	// 临时链表 Swap 语义一致）
	constexpr int kMaxRosterEntries = 1024;
	ParticleEmitterID aRosterScratch[kMaxRosterEntries];
	int aRosterCount = 0;
	if (theContext.mReading)
	{
		int aCountOnDisk = 0;
		theContext.SyncInt(aCountOnDisk);
		if (theContext.mFailed || aCountOnDisk < 0 || aCountOnDisk > kMaxRosterEntries ||
			static_cast<unsigned long>(theContext.ByteLeftToRead()) <
				static_cast<unsigned long>(aCountOnDisk) * 8UL)
		{
			theContext.mFailed = true;
			return;
		}
		// 归属校验对齐原 ValidateOwnedDataIDs 语义（2026-10-04 双写事故后
		// 二次修正）：裸恢复的发射器 mParticleSystem 尚为空（该字段不进
		// 拷贝段，由下方 SyncParticleEmitter 读块回填）——**非空且不同**
		// 才拒，空指针放行；另加同池去重（原 aSeen 位图同款）
		static bool aSeenSlots[1024];
		std::memset(aSeenSlots, 0, sizeof(aSeenSlots));
		for (int i = 0; i < aCountOnDisk; ++i)
		{
			unsigned int aDataID = 0;
			theContext.SyncBytes(&aDataID, sizeof(aDataID));
			if (theContext.mFailed)
				return;
			TodParticleEmitter* aEmitter = aHolder->mEmitters.DataArrayTryToGet(aDataID);
			const unsigned int anIndex = aDataID & 0xFFFF;
			if (aEmitter == nullptr || anIndex >= 1024 || aSeenSlots[anIndex] ||
				(aEmitter->mParticleSystem != nullptr && aEmitter->mParticleSystem != theParticleSystem))
			{
				theContext.mFailed = true;
				return;
			}
			aSeenSlots[anIndex] = true;
			aRosterScratch[i] = static_cast<ParticleEmitterID>(aDataID);
		}
		aRosterCount = aCountOnDisk;
		// The saved pool belongs to this Board. UI particle pools are not saved.
		theParticleSystem->mLastUpdateTimeNs = static_cast<std::int64_t>(theBoard->mLevelTimeNs);
		theParticleSystem->mParticleDef = aDefinition;
		theParticleSystem->mParticleHolder = aHolder;
	}
	else
	{
		// 写路径：count 恰一次（2026-10-04 双写事故修正——曾在此前置一次
		// 无条件 SyncInt，磁盘多 4B 致回读把重复 count 当首 ID 校验失败）
		int aCountOnDisk = theParticleSystem->EmitterRosterCount();
		theContext.SyncInt(aCountOnDisk);
		for (int i = 0; i < aCountOnDisk; ++i)
		{
			unsigned int aDataID = static_cast<unsigned int>(theParticleSystem->EmitterRosterIDAt(i));
			theContext.SyncBytes(&aDataID, sizeof(aDataID));
			if (theContext.mFailed)
				return;
		}
		// 写路径发射器本体走活名册逐个同步（与旧链表遍历同构）
		for (int i = 0; i < aCountOnDisk; ++i)
			SyncParticleEmitter(theParticleSystem, theParticleSystem->EmitterRosterEmitterAt(i), theContext);
		return;
	}
	// 读路径：先按盘上 ID 序同步全部发射器本体，成功后入册
	for (int i = 0; i < aRosterCount; ++i)
	{
		TodParticleEmitter* aEmitter = aHolder->mEmitters.DataArrayTryToGet((unsigned int)aRosterScratch[i]);
		SyncParticleEmitter(theParticleSystem, aEmitter, theContext);
		if (theContext.mFailed)
			return;
	}
	theParticleSystem->EmitterRosterClear();
	for (int i = 0; i < aRosterCount; ++i)
	{
		TodParticleEmitter* aEmitter = aHolder->mEmitters.DataArrayTryToGet((unsigned int)aRosterScratch[i]);
		theParticleSystem->EmitterRosterAdd(aEmitter);
	}
}

//0x4818F0
void SyncReanimation(Board* theBoard, Reanimation* theReanimation, SaveGameContext& theContext)
{
	if (theContext.mFailed)
		return;
	// SyncBoard restores the pool record first; its runtime track pointer is
	// empty. Overwriting a live animation would discard its attachment owner.
	if (theContext.mReading && theReanimation->mTrackInstances != nullptr)
		throw std::logic_error("SyncReanimation: destination already owns tracks");

	ReanimatorDefinition* aDefinition = theReanimation->mDefinition;
	theContext.SyncReanimationDef(aDefinition);
	if (theContext.mFailed || aDefinition == nullptr)
	{
		theContext.mFailed = true;
		return;
	}
	if (theReanimation->mReanimationType < 0 ||
		theReanimation->mReanimationType >= ReanimationType::NUM_REANIMS ||
		aDefinition != &gReanimatorDefArray[theReanimation->mReanimationType] ||
		theReanimation->mVariant >= aDefinition->mVariantCount)
	{
		theContext.mFailed = true;
		return;
	}

	const int aTrackCount = aDefinition->mTracks.count;
	if (aTrackCount < 0 || static_cast<size_t>(aTrackCount) >
		static_cast<size_t>(INT_MAX) / sizeof(ReanimatorTrackInstance))
	{
		theContext.mFailed = true;
		return;
	}
	const int aNativeSize = static_cast<int>(
		static_cast<size_t>(aTrackCount) * sizeof(ReanimatorTrackInstance));
	TodAllocator* anAllocator = nullptr;
	auto aReleaseTracks = [&](ReanimatorTrackInstance* theTracks)
	{
		// Pending IDs are not owned attachments yet. Free only the raw track
		// storage, without running ReanimationDie against partially read IDs.
		anAllocator->Free(theTracks, aNativeSize);
	};
	std::unique_ptr<ReanimatorTrackInstance, decltype(aReleaseTracks)>
		aPendingTracks(nullptr, aReleaseTracks);
	ReanimatorTrackInstance* aTracks = theReanimation->mTrackInstances;

	if (aTrackCount != 0)
	{
		if (theContext.mReading)
		{
			anAllocator = FindGlobalAllocator(aNativeSize);
			aPendingTracks.reset(static_cast<ReanimatorTrackInstance*>(
				anAllocator->Calloc(aNativeSize)));
			aTracks = aPendingTracks.get();
		}
		SyncDat64TrackInstances(theContext, aTracks, aTrackCount);
		if (theContext.mFailed)
			return;

		for (int aTrackIndex = 0; aTrackIndex < aTrackCount; aTrackIndex++)
		{
			ReanimatorTrackInstance& aTrackInstance = aTracks[aTrackIndex];
			if (theContext.mReading)
			{
				aTrackInstance.mBlendTransform.mText = nullptr;
				TOD_ASSERT(aTrackInstance.mBlendTransform.mFont == nullptr);
				TOD_ASSERT(aTrackInstance.mBlendTransform.mImageSlot == 0);
			}
			else
			{
				TOD_ASSERT(aTrackInstance.mBlendTransform.mText == nullptr);
				TOD_ASSERT(aTrackInstance.mBlendTransform.mFont == nullptr);
				TOD_ASSERT(aTrackInstance.mBlendTransform.mImageSlot == 0);
			}
		}
	}
	if (theContext.mReading)
	{
		// Definition, holder and all tracks become visible together only after
		// the complete substream succeeded. Other saved animation fields stay put.
		theReanimation->mDefinition = aDefinition;
		theReanimation->mReanimationHolder = theBoard->mApp->mEffectSystem->mReanimationHolder.get();
		theReanimation->mTrackInstances = aPendingTracks.release();
		theReanimation->RebuildTrackActivityMasks();  // P4：派生掩码随轨道段重建（不入盘）
	}
}

//0x4819D0
void SyncBoard(SaveGameContext& theContext, Board* theBoard)
{
	if (theContext.mFailed)
		return;
	// Pointer-free records deliberately clear process-local owners. Rebind
	// them before ANY read exit, including a later bad field or exception,
	// so normal Board/Zombie cleanup never sees a half-restored null owner.
	struct RuntimeBindings
	{
		Board* board;
		~RuntimeBindings() { if (board) FixBoardAfterLoad(board); }
	} aRuntimeBindings{theContext.mReading ? theBoard : nullptr};

	// 附件池在流序中最后替换，而 ~Attachment 会把 EFFECT_REANIM 引用级联成
	// ReanimationDie。活板流出时旧 reanim 先死并经轨道 AttachmentDie 预先静默
	// 旧附件；失败加载留下的壳态属主（mDefinition 为空）死得安静，带引用的旧
	// 附件活到替换那一刻，级联命中刚恢复的新 reanim——mDead 置位后尾部子流
	// 不再恢复，回滚存档就此漂移。读取前先清空附件池：级联只落在即将整体
	// 替换的旧池上，与 1073 池替换无析构级联的语义一致。
	if (theContext.mReading)
	{
		theBoard->mApp->mEffectSystem->mAttachmentHolder->mAttachments.DataArrayFreeAll();
	}

	{
		SyncDat64BoardState(theContext, *theBoard);
		if (theContext.mFailed || theBoard->mLevelTimeNs > std::uint64_t(INT64_MAX))
		{
			theContext.mFailed = true;
			return;
		}
		SyncDat64DataArray(theContext, theBoard->mZombies);
		SyncDat64DataArray(theContext, theBoard->mPlants);
		SyncDat64DataArray(theContext, theBoard->mProjectiles);
		SyncDat64DataArray(theContext, theBoard->mCoins);
		SyncDat64DataArray(theContext, theBoard->mLawnMowers);
		SyncDat64DataArray(theContext, theBoard->mGridItems);
		SyncDat64DataArray(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mParticleSystems);
		SyncDat64DataArray(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mEmitters);
		SyncDat64DataArray(theContext, theBoard->mApp->mEffectSystem->mParticleHolder->mParticles);
		SyncDat64DataArray(theContext, theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations);
		SyncDat64DataArray(theContext, theBoard->mApp->mEffectSystem->mAttachmentHolder->mAttachments);
	}
	if (theContext.mFailed)
	{
		return;
	}

	{
		TodParticleSystem* aParticle = nullptr;
		while (theBoard->mApp->mEffectSystem->mParticleHolder->mParticleSystems.IterateNext(aParticle))
		{
			SyncParticleSystem(theBoard, aParticle, theContext);
			if (theContext.mFailed)
			{
				return;
			}
		}
	}
	{
		Reanimation* aReanimation = nullptr;
		while (theBoard->mApp->mEffectSystem->mReanimationHolder->mReanimations.IterateNext(aReanimation))
		{
			SyncReanimation(theBoard, aReanimation, theContext);
			if (theContext.mFailed)
			{
				return;
			}
		}
	}
	// 装载孤儿修剪已撤（2026-10-05 用户裁决：属回退路径，同"服务器有问题
	// 就定时重启"——根因已修于死亡回收，污染存档经一次性装载清创完毕）

	{
		SyncDat64Object(theContext, *theBoard->mCursorObject);
		SyncDat64Object(theContext, *theBoard->mCursorPreview);
		SyncDat64Object(theContext, *theBoard->mAdvice);
		SyncDat64Object(theContext, *theBoard->mSeedBank);
		SyncDat64Object(theContext, *theBoard->mChallenge);
		SyncDat64Object(theContext, *theBoard->mApp->mMusic);

		std::uint64_t anEndMarker = LevelSaveFormat::DAT64_END_MARKER;
		theContext.SyncBytes(&anEndMarker, sizeof(anEndMarker));
		if (theContext.mReading && anEndMarker != LevelSaveFormat::DAT64_END_MARKER)
		{
			theContext.mFailed = true;
		}
	}
}

//0x481CE0
void FixBoardAfterLoad(Board* theBoard)
{
	{
		Plant* aPlant = nullptr;
		while (theBoard->mPlants.IterateNext(aPlant))
		{
			aPlant->mApp = theBoard->mApp;
			aPlant->mBoard = theBoard;
			SaveClockProjection::Bind(*aPlant);
		}
	}
	{
		Zombie* aZombie = nullptr;
		while (theBoard->mZombies.IterateNext(aZombie))
		{
			aZombie->mApp = theBoard->mApp;
			aZombie->mBoard = theBoard;
			SaveClockProjection::Bind(*aZombie);
		}
	}
	{
		Projectile* aProjectile = nullptr;
		while (theBoard->mProjectiles.IterateNext(aProjectile))
		{
			aProjectile->mApp = theBoard->mApp;
			aProjectile->mBoard = theBoard;
		}
	}
	{
		Coin* aCoin = nullptr;
		while (theBoard->mCoins.IterateNext(aCoin))
		{
			aCoin->mApp = theBoard->mApp;
			aCoin->mBoard = theBoard;
			SaveClockProjection::Bind(*aCoin);
		}
	}
	{
		LawnMower* aLawnMower = nullptr;
		while (theBoard->mLawnMowers.IterateNext(aLawnMower))
		{
			aLawnMower->mApp = theBoard->mApp;
			aLawnMower->mBoard = theBoard;
			SaveClockProjection::Bind(*aLawnMower);
		}
	}
	{
		GridItem* aGridItem = nullptr;
		while (theBoard->mGridItems.IterateNext(aGridItem))
		{
			aGridItem->mApp = theBoard->mApp;
			aGridItem->mBoard = theBoard;
			SaveClockProjection::Bind(*aGridItem);
		}
	}

	theBoard->mAdvice->mApp = theBoard->mApp;
	SaveClockProjection::Bind(*theBoard->mAdvice);
	theBoard->mCursorObject->mApp = theBoard->mApp;
	theBoard->mCursorObject->mBoard = theBoard;
	theBoard->mCursorPreview->mApp = theBoard->mApp;
	theBoard->mCursorPreview->mBoard = theBoard;
	theBoard->mSeedBank->mApp = theBoard->mApp;
	theBoard->mSeedBank->mBoard = theBoard;
	for (int i = 0; i < SEEDBANK_MAX; i++)
	{
		theBoard->mSeedBank->mSeedPackets[i].mApp = theBoard->mApp;
		theBoard->mSeedBank->mSeedPackets[i].mBoard = theBoard;
	}
	SaveClockProjection::Bind(*theBoard->mSeedBank);
	theBoard->mChallenge->mApp = theBoard->mApp;
	theBoard->mChallenge->mBoard = theBoard;
	theBoard->mApp->mMusic->mApp = theBoard->mApp;
	theBoard->mApp->mMusic->mMusicInterface = theBoard->mApp->mMusicInterface;
	SaveClockProjection::Bind(*theBoard->mApp->mMusic);
	if (theBoard->mLevelTimeNs > std::uint64_t(INT64_MAX))
		return; // A failed partial read must still leave owner pointers repaired.
	auto& aHolder = *theBoard->mApp->mEffectSystem->mReanimationHolder;
	aHolder.mBattleTimeNs = static_cast<std::int64_t>(theBoard->mLevelTimeNs);
	Reanimation* aReanimation = nullptr;
	while (aHolder.mReanimations.IterateNext(aReanimation))
		if (aReanimation->mDefinition && aReanimation->mReanimationHolder == &aHolder)
			aReanimation->RebaseTime(aHolder.mTimeDomains[aHolder.mReanimations.DataArrayIndexOf(aReanimation)] == ReanimTimeDomain::Battle
				? aHolder.mBattleTimeNs : aHolder.mVisualTimeNs);
}

namespace
{
template <typename T>
bool ValidateNextDat64Pool(const LegacyByte* thePayload, size_t thePayloadSize,
    size_t& theOffset, const DataArray<T>& thePool, DataArrayKind& theFailedPool)
{
    theFailedPool = thePool.mKind;
    if (theOffset > thePayloadSize || thePayloadSize - theOffset < 16)
        return false;
    const auto ReadWord = [&](size_t theRelativeOffset)
    {
        unsigned int aValue = 0;
        std::memcpy(&aValue, thePayload + theOffset + theRelativeOffset, sizeof(aValue));
        return aValue;
    };
    const unsigned int aFreeHead = ReadWord(0);
    const unsigned int aUsed = ReadWord(4);
    const unsigned int aSize = ReadWord(8);
    const unsigned int aBytes = ReadWord(12);
    const size_t aStride = Dat64Layout<T>::SlotSize();  // 文件槽步长（与原生解耦）
    if (aUsed > thePool.mMaxSize || aSize > aUsed || aFreeHead > aUsed ||
        aUsed > static_cast<size_t>(INT_MAX) / aStride)
        return false;
    const size_t aExpectedBytes = aUsed * aStride;
    if (aBytes != aExpectedBytes || aExpectedBytes > thePayloadSize - theOffset - 16)
        return false;
    if (!ValidateDataArraySlots(thePayload + theOffset + 16, aStride,
        Dat64Layout<T>::Size(), aUsed, aSize, aFreeHead))
        return false;
    theOffset += 16 + aExpectedBytes;
    return true;
}

// Read-only structural pass: a bad later pool must be discovered before any
// earlier pool (or the Board state) is replaced. This does not instantiate
// objects, consume RNG, copy payload blocks, or alter the on-disk schema.
bool ValidateDat64PoolHeaders(const LegacyByte* thePayload, size_t thePayloadSize,
    const Board& theBoard, DataArrayKind& theFailedPool)
{
    theFailedPool = DataArrayKind::Unknown;
    const size_t boardSize = Dat64Layout<Board>::Size();
    size_t anOffset = 4 + boardSize;
    if (thePayloadSize < anOffset)
        return false;
    unsigned int aBoardSize = 0;
    std::memcpy(&aBoardSize, thePayload, sizeof(aBoardSize));
    if (aBoardSize != boardSize)
        return false;
    const auto Check = [&](const auto& thePool)
    {
        return ValidateNextDat64Pool(thePayload, thePayloadSize, anOffset, thePool, theFailedPool);
    };
    const auto& effects = *theBoard.mApp->mEffectSystem;
    return Check(theBoard.mZombies) && Check(theBoard.mPlants) &&
        Check(theBoard.mProjectiles) && Check(theBoard.mCoins) &&
        Check(theBoard.mLawnMowers) && Check(theBoard.mGridItems) &&
        Check(effects.mParticleHolder->mParticleSystems) &&
        Check(effects.mParticleHolder->mEmitters) &&
        Check(effects.mParticleHolder->mParticles) &&
        Check(effects.mReanimationHolder->mReanimations) &&
        Check(effects.mAttachmentHolder->mAttachments);
}
}

//0x481FE0
// GOTY @Patoke: 0x48CBC0
// 当前活动状态 → 完整 dat64 文件形态缓冲。
static bool BuildDat64SaveBuffer(Board* theBoard, Buffer& theOutBuffer)
{
    // Gestures are runtime state. Return reserved cards before serializing the
    // real mouse cursor and seed bank; no touch pointers enter DAT64.
    if (theBoard->mWidgetManager) theBoard->mWidgetManager->CancelTouches(theBoard);
    theBoard->ClearTouchSelection();
	// A legacy input callback may save while its contact context is selected.
	// Serialize the real mouse even in that nested call, then restore the caller.
	struct RestoreCursor {
		Board& board;
		CursorObject* cursor;
		CursorPreview* preview;
		~RestoreCursor() { board.mCursorObject = cursor; board.mCursorPreview = preview; }
	} restoreCursor{*theBoard, theBoard->mCursorObject, theBoard->mCursorPreview};
	if (theBoard->mTouchState)
	{
		theBoard->mCursorObject = theBoard->mTouchState->mouseCursor;
		theBoard->mCursorPreview = theBoard->mTouchState->mousePreview;
	}
	SaveGameContext aContext;
	aContext.mFailed = false;
	aContext.mReading = false;
	SyncBoard(aContext, theBoard);
	if (aContext.mFailed)
	{
		return false;
	}

	const int aPayloadSize = aContext.mBuffer.GetDataLen();

	const auto WriteU32 = [](Buffer& theBuffer, std::uint32_t theValue)
	{
		LegacyByte aBytes[4] = {
			static_cast<LegacyByte>(theValue),
			static_cast<LegacyByte>(theValue >> 8),
			static_cast<LegacyByte>(theValue >> 16),
			static_cast<LegacyByte>(theValue >> 24)
		};
		theBuffer.WriteBytes(aBytes, sizeof(aBytes));
	};
	const auto WriteU64 = [&WriteU32](Buffer& theBuffer, std::uint64_t theValue)
	{
		WriteU32(theBuffer, static_cast<std::uint32_t>(theValue));
		WriteU32(theBuffer, static_cast<std::uint32_t>(theValue >> 32));
	};

	theOutBuffer.WriteBytes(
		LevelSaveFormat::DAT64_MAGIC.data(),
		static_cast<int>(LevelSaveFormat::DAT64_MAGIC.size()));
	WriteU32(theOutBuffer, Dat64TimeSchema::VERSION);
	WriteU32(theOutBuffer, LevelSaveFormat::DAT64_HEADER_SIZE);
	WriteU32(theOutBuffer, LevelSaveFormat::DAT64_AUTHORITY_BUILD);
	WriteU32(theOutBuffer, Dat64TimeSchema::FLAGS);
	WriteU64(theOutBuffer, static_cast<std::uint64_t>(aPayloadSize));
	WriteU32(theOutBuffer, 0); // Former payload CRC32; reserved and ignored.
	WriteU32(theOutBuffer, Dat64TimeSchema::SCHEMA_ID);
	WriteU32(theOutBuffer, LevelSaveFormat::DAT64_MINOR_VERSION);
	WriteU32(theOutBuffer, 0);
	TOD_ASSERT(theOutBuffer.GetDataLen() == LevelSaveFormat::DAT64_HEADER_SIZE);
	theOutBuffer.WriteBytes(aContext.mBuffer.GetDataPtr(), aPayloadSize);
	return true;
}

// 缓冲 → 板（原 LawnLoadGame 主体；文件读取与事务回滚在外层）
static bool LoadDat64FromBuffer(Board* theBoard, Buffer& aFileBuffer, const std::string& theFilePath)
{

	const auto ReadU32 = [](const LegacyByte* p) -> std::uint32_t
	{
		return static_cast<std::uint32_t>(p[0]) |
			(static_cast<std::uint32_t>(p[1]) << 8) |
			(static_cast<std::uint32_t>(p[2]) << 16) |
			(static_cast<std::uint32_t>(p[3]) << 24);
	};
	const auto ReadU64 = [&ReadU32](const LegacyByte* p) -> std::uint64_t
	{
		return static_cast<std::uint64_t>(ReadU32(p)) |
			(static_cast<std::uint64_t>(ReadU32(p + 4)) << 32);
	};

	const int aFileSize = aFileBuffer.GetDataLen();
	const LegacyByte* aFileData = aFileBuffer.GetDataPtr();
	if (aFileSize < static_cast<int>(LevelSaveFormat::DAT64_HEADER_SIZE) ||
		std::memcmp(
			aFileData,
			LevelSaveFormat::DAT64_MAGIC.data(),
			LevelSaveFormat::DAT64_MAGIC.size()) != 0)
	{
		TodTraceAndLog("DAT64_HEADER_REJECT file=%s reason=magic-or-size", theFilePath.c_str());
		return false;
	}

	const std::uint32_t aVersion = ReadU32(aFileData + 8);
	const std::uint32_t aHeaderSize = ReadU32(aFileData + 12);
	const std::uint32_t anAuthorityBuild = ReadU32(aFileData + 16);
	const std::uint32_t aFlags = ReadU32(aFileData + 20);
	const std::uint64_t aPayloadSize = ReadU64(aFileData + 24);
	const std::uint32_t aSchemaID = ReadU32(aFileData + 36);
	const std::span aFile(aFileData, static_cast<size_t>(aFileSize));
	if (LevelSaveFormat::IsSupportedDat64Header(aFile) && !LevelSaveFormat::IsCurrentDat64Header(aFile))
	{
		const auto aMinor = ReadU32(aFileData + 40);
		TodTraceAndLog("DAT64_UPGRADE_REQUIRED file=%s version=%u.%u current=%u.%u",
			theFilePath.c_str(), aVersion, aMinor, LevelSaveFormat::DAT64_VERSION,
			LevelSaveFormat::DAT64_MINOR_VERSION);
		throw SaveGameUpgradeRequired(StrFormat(
			"Saved game needs a format upgrade:\n%s\n\n"
			"This file uses DAT64 v%u.%u (schema 0x%08X);\n"
			"this build reads DAT64 v%u.%u (schema 0x%08X).\n"
			"Use the bundled save converter to create a current-format copy.\n"
			"This load attempt has not changed the original file.\n"
			"A new game will not be started over this save.",
			theFilePath.c_str(), aVersion, aMinor, aSchemaID, LevelSaveFormat::DAT64_VERSION,
			LevelSaveFormat::DAT64_MINOR_VERSION, LevelSaveFormat::DAT64_SCHEMA_ID));
	}
	if (!LevelSaveFormat::IsCurrentDat64Header(aFile) ||
		aPayloadSize > static_cast<std::uint64_t>(INT_MAX))
	{
		TodTraceAndLog(
			"DAT64_HEADER_REJECT file=%s version=%u header=%u authority=%u flags=%08X schema=%08X payload=%llu",
			theFilePath.c_str(),
			aVersion,
			aHeaderSize,
			anAuthorityBuild,
			aFlags,
			aSchemaID,
			static_cast<unsigned long long>(aPayloadSize));
		return false;
	}

	const LegacyByte* aPayload = aFileData + aHeaderSize;
	// Offset 0x20 was a payload CRC32. The current format ignores it,
	// so editing valid fields never requires recalculating a checksum.

	DataArrayKind aFailedPool = DataArrayKind::Unknown;
	if (!ValidateDat64PoolHeaders(aPayload, static_cast<size_t>(aPayloadSize), *theBoard, aFailedPool))
	{
		TodTraceAndLog("DAT64_POOL_REJECT file=%s pool=%llu reason=structure", theFilePath.c_str(),
			static_cast<unsigned long long>(aFailedPool));
		return false;
	}

	SaveGameContext aContext;
	aContext.mFailed = false;
	aContext.mReading = true;
	aContext.mBuffer.SetData(
		const_cast<LegacyByte*>(aPayload),
		static_cast<int>(aPayloadSize));

	SyncBoard(aContext, theBoard);
	if (aContext.mFailed || aContext.ByteLeftToRead() != 0)
	{
		TodTraceAndLog(
			"DAT64_BOARD_REJECT file=%s failed=%d left=%lu",
			theFilePath.c_str(),
			aContext.mFailed ? 1 : 0,
			static_cast<unsigned long>(aContext.ByteLeftToRead()));
		return false;
	}
    // Restored effects already belong to the saved Board clock, including
    // loading into the Continue/chooser UI. Never replay the first-new intro.
    theBoard->mPoolIntroVisual = false;
	theBoard->RebuildPlantCellBucket();  // 格桶为派生索引不进存档，装载验收后重建
	theBoard->RebuildZombieRowBucket();  // 行段桶同款：整池镜像装载不走初始化尾
	// 关卡放行门（与 LawnApp::StartPlaying 同语义，2026-10-09 用户规格
	// "加载界面卡住不放行，首绘路径不得存在"）：存档恢复是另一条进玩游
	// 路径（Continue 继承局不经 StartPlaying），棋盘相关新加载图必须
	// 在此排干+清扫到位，不允许首绘在绘制路径上传。
	TodWarmDrainAll();
	theBoard->mApp->mGameScene = GameScenes::SCENE_PLAYING;
	return true;
}

bool LawnLoadGame(Board* theBoard, const std::string& theFilePath)
{
	Buffer aFileBuffer;
	if (!gSexyAppBase->ReadBufferFromFile(theFilePath, &aFileBuffer, false))
	{
		return false;
	}

	return LoadDat64FromBuffer(theBoard, aFileBuffer, theFilePath);
}

//0x4820D0
bool LawnSaveGame(Board* theBoard, const std::string& theFilePath)
{
	Buffer aFileBuffer;
	if (!BuildDat64SaveBuffer(theBoard, aFileBuffer))
	{
		return false;
	}
	return gSexyAppBase->WriteBufferToFile(theFilePath, &aFileBuffer);
}
